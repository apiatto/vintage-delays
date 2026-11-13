#!/usr/bin/env python3
"""Measure the character of a delay recording, to calibrate Holdfast 512 against real hardware.

    python3 analyze_recording.py clip.wav [--start S --end E] [--label NAME]

Works on any WAV (convert other formats first, e.g. ffmpeg -i in.m4a -ac 1 -ar 48000 out.wav).
Needs only numpy. Prints a JSON report with:

  * spectrum: long-term average spectrum in 1/3-octave bands (dB re. the 1 kHz band) for the
    loud frames and the 'tail' frames (decaying echoes after a note stops)
  * bandwidth: frequency where the loud and tail spectra fall 10 dB and 20 dB below 1 kHz
  * noise: level and spectral tilt of the quietest 5 % of frames (hiss / idle noise floor),
    and the noise-to-loud ratio in dB
  * echoes: delays (ms) of the strongest peaks of the envelope autocorrelation, the ratio
    between successive echo peaks (dB per repeat) and the high-frequency loss per repeat
  * modulation: dominant rate (Hz) of pitch wobble from a frame-wise spectral centroid track
"""

import json
import subprocess
import sys
import wave

import numpy as np


def load(path, start=None, end=None, fs=48000):
    # Always normalise through ffmpeg: handles mp3/m4a/webm and float WAVs.
    import os, tempfile
    tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    tmp.close()
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", path, "-ac", "1", "-ar", str(fs),
                    "-c:a", "pcm_s24le", tmp.name], check=True)
    path = tmp.name
    with wave.open(path, "rb") as w:
        n, ch, sw, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    os.unlink(path)
    if sw == 2:
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) << 8 | b[:, 1].astype(np.int32) << 16 | b[:, 2].astype(np.int32) << 24) >> 8) / 8388608.0
    elif sw == 4:
        x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise SystemExit("unsupported sample width")
    x = x.reshape(-1, ch).mean(axis=1)
    if start is not None or end is not None:
        a = int((start or 0) * rate)
        b = int(end * rate) if end else len(x)
        x = x[a:b]
    return x, rate


def frames(x, n, hop):
    count = max(0, (len(x) - n) // hop + 1)
    idx = np.arange(n)[None, :] + hop * np.arange(count)[:, None]
    return x[idx]


THIRD_OCT = [31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000,
             2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000]


def band_levels(power, freqs):
    out = []
    for fc in THIRD_OCT:
        lo, hi = fc / 2 ** (1 / 6), fc * 2 ** (1 / 6)
        m = (freqs >= lo) & (freqs < hi)
        out.append(10 * np.log10(power[m].mean() + 1e-20) if m.any() else float("nan"))
    out = np.array(out)
    ref = out[THIRD_OCT.index(1000)]
    return out - ref, ref


def edge(rel, drop):
    """Highest band above 1 kHz that is still within 'drop' dB of the 1 kHz band."""
    i0 = THIRD_OCT.index(1000)
    last = 1000.0
    for i in range(i0, len(THIRD_OCT)):
        if np.isnan(rel[i]):
            break
        if rel[i] < -drop:
            return last
        last = THIRD_OCT[i]
    return last


def analyze(x, fs):
    n, hop = 4096, 1024
    win = np.hanning(n)
    fr = frames(x, n, hop)
    spec = np.abs(np.fft.rfft(fr * win, axis=1)) ** 2
    freqs = np.fft.rfftfreq(n, 1 / fs)
    energy = 10 * np.log10(spec.sum(axis=1) + 1e-20)

    loud = energy > np.percentile(energy, 70)
    quiet = energy <= np.percentile(energy, 5)

    # Tail frames: falling energy, 10-35 dB below the loud level (decaying echoes).
    loud_level = np.median(energy[loud])
    d = np.diff(energy, prepend=energy[0])
    tail = (energy < loud_level - 10) & (energy > loud_level - 35) & (d < 0)

    rep = {}
    for name, mask in (("loud", loud), ("tail", tail), ("quiet", quiet)):
        if mask.sum() < 3:
            continue
        rel, ref = band_levels(spec[mask].mean(axis=0), freqs)
        rep[name] = {
            "frames": int(mask.sum()),
            "level_1k_db": round(float(ref), 1),
            "third_octave_db_re_1k": {str(f): round(float(v), 1) for f, v in zip(THIRD_OCT, rel)},
            "minus10db_hz": edge(rel, 10),
            "minus20db_hz": edge(rel, 20),
        }

    if "quiet" in rep and "loud" in rep:
        rep["noise_to_loud_db"] = round(float(np.median(energy[quiet]) - loud_level), 1)
        q = rep["quiet"]["third_octave_db_re_1k"]
        rep["noise_tilt_db_8k_minus_500"] = round(q["8000"] - q["500"], 1)

    # Echo structure: autocorrelation of the amplitude envelope (5 ms resolution).
    env_hop = int(fs * 0.005)
    env = np.sqrt(np.mean(frames(x, env_hop, env_hop) ** 2, axis=1))
    env = env - env.mean()
    ac = np.correlate(env, env, mode="full")[len(env) - 1:]
    ac /= ac[0] + 1e-20
    lags_ms = np.arange(len(ac)) * 5.0
    peaks = [i for i in range(4, min(len(ac) - 1, 600)) if ac[i] > ac[i - 1] and ac[i] >= ac[i + 1] and ac[i] > 0.1]
    peaks = sorted(peaks, key=lambda i: -ac[i])[:6]
    rep["echo_peaks_ms"] = sorted(round(float(lags_ms[i]), 0) for i in peaks)
    if peaks:
        p = min(peaks, key=lambda i: lags_ms[i])
        chain = [p * k for k in range(1, 6) if p * k < len(ac)]
        vals = [ac[i - 1:i + 2].max() for i in chain]
        if len(vals) > 1 and vals[0] > 0:
            ratios = [20 * np.log10(max(vals[k + 1], 1e-6) / max(vals[k], 1e-6)) / 2 for k in range(len(vals) - 1)]
            rep["echo_period_ms"] = float(lags_ms[p])
            rep["db_per_repeat_estimate"] = round(float(np.median(ratios)), 1)

    # High-frequency loss per repeat: compare spectra of successive echoes in tail frames.
    if "echo_period_ms" in rep:
        period = int(rep["echo_period_ms"] / 1000 * fs / hop)
        if period >= 1:
            hf, lf = [], []
            for i in np.where(tail)[0]:
                j = i + period
                if j < len(spec) and tail[j]:
                    a, b = spec[i], spec[j]
                    hf.append(10 * np.log10((b[(freqs > 3000) & (freqs < 8000)].sum() + 1e-20) / (a[(freqs > 3000) & (freqs < 8000)].sum() + 1e-20)))
                    lf.append(10 * np.log10((b[(freqs > 200) & (freqs < 1000)].sum() + 1e-20) / (a[(freqs > 200) & (freqs < 1000)].sum() + 1e-20)))
            if len(hf) > 3:
                rep["hf_minus_lf_loss_per_repeat_db"] = round(float(np.median(hf) - np.median(lf)), 1)

    # Modulation: dominant rate of the spectral-centroid wobble on loud frames (0.1 - 12 Hz).
    cen = (spec * freqs).sum(axis=1) / (spec.sum(axis=1) + 1e-20)
    if loud.sum() > 64:
        c = np.where(loud, cen, np.median(cen[loud]))
        c = c - np.convolve(c, np.ones(31) / 31, mode="same")
        cs = np.abs(np.fft.rfft(c * np.hanning(len(c))))
        cf = np.fft.rfftfreq(len(c), hop / fs)
        m = (cf > 0.1) & (cf < 12)
        if m.any():
            rep["centroid_wobble_hz"] = round(float(cf[m][np.argmax(cs[m])]), 2)

    rep["duration_s"] = round(len(x) / fs, 1)
    return rep


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(__doc__)
    path = args[0]
    start = float(args[args.index("--start") + 1]) if "--start" in args else None
    end = float(args[args.index("--end") + 1]) if "--end" in args else None
    label = args[args.index("--label") + 1] if "--label" in args else path
    x, fs = load(path, start, end)
    rep = analyze(x, fs)
    rep["label"] = label
    def clean(o):
        if isinstance(o, dict):
            return {k: clean(v) for k, v in o.items()}
        if isinstance(o, (list, tuple)):
            return [clean(v) for v in o]
        if isinstance(o, float) and not np.isfinite(o):
            return None
        return o
    print(json.dumps(clean(rep), indent=1))


if __name__ == "__main__":
    main()
