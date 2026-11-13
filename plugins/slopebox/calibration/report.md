# Effectron II (ADM 1024): misure di riferimento da YouTube

Data: 2026-11-13. Misure fatte sul Mac Studio (Python, numpy e scipy) sull'audio estratto da video YouTube.
Audio e fotogrammi restano solo in locale (`~/Documents/Workstation Audio/effectron-ref/`) e non sono nel repository.

**Avvertenza generale.** Le fonti sono registrazioni amatoriali compresse (YouTube, AAC/Opus) con catena di registrazione sconosciuta
(sintetizzatore, mixer, a volte un microfono davanti all'amplificatore). Le misure *relative*, cioè per ripetizione, rispetto a 1 kHz e i
tempi di ritardo, sono affidabili. I livelli assoluti e il rumore descrivono "il sistema registrato", non solo l'Effectron.

## Fonti

| ID | Titolo | Durata | Uso |
|---|---|---|---|
| [HcfrjhGU15U](https://www.youtube.com/watch?v=HcfrjhGU15U) | Six-Trak through Effectron II ADM 1024 (SynthMania, 2013) | 11:30 | **Fonte principale**: audio di linea pulito fino a 20 kHz, segmenti etichettati a schermo |
| [92mGQqRzdNg](https://www.youtube.com/watch?v=92mGQqRzdNg) | DeltaLab Effectron II ADM 1024 – Demo (The Echo Mill) | 1:23 | sweep di flanger (00:47–01:20), pannello visibile |
| [Q4Nb9Ma0Qeg](https://www.youtube.com/watch?v=Q4Nb9Ma0Qeg) | Deltalab ADM 1024 Effectron II (Retrosonic Pro Audio) | 0:57 | rumore prima dell'attacco (00:00–00:01.3) |
| [0nSEyUmCkzc](https://www.youtube.com/watch?v=0nSEyUmCkzc) | Deltalab Effectron 2 test | 2:14 | chitarra, amplificatore ripreso col microfono, banda circa 5 kHz: solo qualitativo |
| [UUP9kc9Akn4](https://www.youtube.com/watch?v=UUP9kc9Akn4) | Delta Lab Effectron II | 1:08 | audio da telefono, banda di 4,5 kHz e distorto: **scartato** per le misure |
| [A2rccqTVuf4](https://www.youtube.com/watch?v=A2rccqTVuf4) | Delta Lab Effectron Guitar Delay ADM 1024 | 6:57 | passa-basso a 10 kHz della codifica, pannello mai inquadrato: **scartato** |

Scartati senza scaricare: s50_frZrNuw (solo modulazione esterna) e hSkhwzuZfWw (mix pieno di altre macchine).

### Segmentazione di HcfrjhGU15U (dalle didascalie a schermo, risoluzione 10 s)

| Tempo | Didascalia |
|---|---|
| 00:10–00:20 | Dry synth |
| 00:30–01:40 | Long Echo (coda libera da circa 01:32) |
| 01:50 | Dry flutes |
| 02:00–02:25 | Flange Hi |
| 02:30 | Dry soft brass |
| 02:50–03:25 | Flange Mid |
| 03:30 | Dry synth bass |
| 03:40–04:05 | Flange Lo |
| 04:20 | Dry synth |
| 04:30–05:05 | Doubler |
| 05:20–05:45 | Short Echo |
| 05:50–06:35 | Long Echo |
| 06:40–08:05 | Infinite Repeat |
| 08:20–11:20 | Long Echo with modulation |

Le posizioni delle manopole **non sono leggibili** dai fotogrammi: video a 480p, inquadratura di scorcio. Le impostazioni qui sotto sono
dedotte dai tempi misurati e dalle didascalie, non lette dal pannello.

## a) Flanger e chorus: traiettoria del ritardo

Il ritardo del filtro a pettine è stato seguito nel tempo con il **picco del cepstrum reale** (STFT 4096, hop 256, 48 kHz). È equivalente a
seguire il primo notch, con f1 = 1/(2·τ). File: `notch_traj_*.csv`, tempo in s, ritardo in ms, ricampionati a 0,1 s.

| Segmento | Tempo | Ritardo p5–p95 | Rapporto | f1 (primo notch) | Note |
|---|---|---|---|---|---|
| Flange Hi | 01:58–02:28 | 0,17–0,92 ms | circa 5 | 545–3000 Hz | tracking rumoroso (il sorgente ha poche armoniche alte) |
| Flange Mid | 02:45–03:27 | 1,3–4,5 ms | circa 3 | 111–385 Hz | si vedono "V" arrotondate da 3,4 a 1,3 ms |
| **Flange Lo** | **03:41–04:00** | **4,35–16,5 ms** (p2–p98) | **3,8** | **30–115 Hz** | **traiettoria pulita, la migliore** |
| Doubler | 04:28–05:08 | 10–33 ms | circa 3,3 | 15–49 Hz | salti tra plateau: artefatti del tracking dovuti al pitch del sintetizzatore |

**Flange Lo (03:41–04:00), il dato più affidabile:**
- L'escursione 4,35–16,5 ms coincide con la gamma nominale del tasto LO (4–16 ms). È quindi verosimile WIDTH al 100%, con lo sweep da circa ×0,27 a ×1,03 del tasto: due ottave, come dice il manuale.
- Periodo dell'LFO di **13,3–13,8 s** (circa 0,073 Hz), cioè SPEED vicino al minimo (0,1 Hz nominale).
- Lo sweep è **simmetrico**: salita 03:44.3→03:50.3 (6,0 s), discesa 03:50.5→03:56.8 (6,3 s). Quindi il pitch sale e scende in modo simmetrico.
- Forma: i fit sinusoidali e triangolari, sul ritardo lineare e sul log-ritardo, danno lo stesso errore (NRMSE 0,081–0,086). I dati **non distinguono** sinusoide da triangolo, né andamento lineare o esponenziale. Il minimo a 03:56.8 è arrotondato (tipico di una sinusoide), il massimo è più appuntito. Coerente con l'LFO sinusoidale del manuale che pilota un VCO.
- **Profondità dei notch: non misurabile in modo affidabile.** Il sorgente è armonico e saltella tra le note, e il mix dry/wet non è noto.

Sweep di The Echo Mill (00:47–01:20): stima 0,21–0,35 ms, ma con confidenza bassa. Il CSV c'è (`notch_traj_em_sweep.csv`), da non usare per tarare.

## b) Echi con feedback

### Ritardi misurati (cepstrum a lunga quefrency, FFT 2^17)

| Segmento | Ritardo | Fattore implicito | Note |
|---|---|---|---|
| Short Echo, 05:20–05:46 | **170,5 ms** (z = 64) | 0,67 sul tasto SHORT (64–256 ms) | |
| Long Echo, 05:50–06:36 | **485,7 ms** (z = 77), con seconda ripetizione a 971,4 ms | 0,474 sul tasto LONG (256–1024 ms) | |
| Infinite Repeat, 06:40–08:05 | loop di **484–486 ms** (periodicità dell'inviluppo) | uguale al Long Echo | conferma: il repeat ricircola la memoria piena = 1024 ms × fattore |
| Long Echo con modulazione, 08:20–11:10 | **737 ms** (z = 13,5) | circa 0,72 | picco allargato dalla modulazione |

### Perdita per ripetizione: coda libera con ripetizioni discrete, 06:37.4–06:41.3 (τ = 485,7 ms)

Finestre sulle ripetizioni 2…7 (la 0–1 contiene ancora la nota dry), bande d'ottava. Livello in dB rispetto alla prima finestra.
CSV: `long_echo_repeats_octave.csv`.

| Rip. | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 12k | 16k |
|---|---|---|---|---|---|---|---|---|---|
| 2 | +6,5 | −0,9 | −1,2 | +4,5 | +3,5 | +4,4 | +2,7 | −0,6 | −0,4 |
| 3 | +5,3 | −4,9 | −5,0 | +0,4 | −0,8 | −0,8 | −4,4 | −10,5 | −10,5 |
| 4 | +2,4 | −8,7 | −9,0 | −4,0 | −5,2 | −6,1 | −11,4 | −19,4 | −20,4 |
| 5 | −0,7 | −12,7 | −13,0 | −7,8 | −9,2 | −11,0 | −18,0 | −27,7 | −28,0 |
| 6 | −3,3 | −16,4 | −16,6 | −11,7 | −13,3 | −15,8 | −24,4 | −33,9 | −34,2 |
| 7 | −7,9 | −20,1 | −20,4 | −15,6 | −17,3 | −20,6 | −30,6 | −37,7 | −39,2 |

**Pendenza (regressione sulle ripetizioni 2…7), in dB per ripetizione:**

| | 1 kHz | 2 kHz | 4 kHz | 8 kHz | 12 kHz | 16 kHz |
|---|---|---|---|---|---|---|
| assoluta | **−4,0** | −4,2 | −5,0 | −6,7 | −7,5 | −7,8 |
| **relativa a 1 kHz** | 0 | **−0,1** | **−1,0** | **−2,6** | **−3,5** | −3,8 |

Pendenza in terzi d'ottava, ripetizioni 2…7 (CSV: `echo_loss_per_pass_third_octave.csv`), relativa a 1 kHz:
−0,3 dB a 2,5 kHz, −0,7 a 3,2 kHz, −1,1 a 4 kHz, −1,6 a 5 kHz, −2,3 a 6,3 kHz, −3,3 a 8 kHz, −3,5 a 10 kHz, −3,8 a 12,7 kHz e 16 kHz.
Sopra circa 10 kHz la curva si appiattisce perché dalla ripetizione 4–5 le bande alte toccano il rumore di fondo: la perdita vera a 12–16 kHz
è quindi **almeno** −3,8 dB per ripetizione. Usando solo le ripetizioni 2…5: −2,8 dB a 8 kHz, −4,9 a 12 kHz e −5,1 a 16 kHz per ripetizione rispetto a 1 kHz.

- Guadagno d'anello a 1 kHz: −4,0 dB per ripetizione, cioè g ≈ 0,63 (FEEDBACK a circa due terzi).
- Le bande a 125 e 250 Hz sono contaminate dalla nota di basso e dal riverbero ambientale: non usarle.
- Banda della singola passata nell'anello (rispetto a 1 kHz): **−1 dB a circa 4 kHz, −3 dB a circa 8 kHz**. −20 dB in una sola passata non viene mai raggiunto entro 16 kHz.

**Confronto con Slopebox (preset echo_feedback, numeri del coordinatore):**

| dB per ripetizione rispetto a 1 kHz | 2 kHz | 4 kHz | 8 kHz | 12 kHz |
|---|---|---|---|---|
| Effectron (questa misura) | −0,1 | −1,0 | −2,6 (−2,8 sulle rip. 2…5) | −3,5 (−4,9 sulle rip. 2…5) |
| Slopebox (da rip. 1 a rip. 5, diviso 4) | −0,1 | −0,3 | −1,7 | −4,5 |

→ A 12 kHz Slopebox è già molto vicino (−4,5 contro −3,5…−4,9). Perde invece **troppo poco tra 4 e 8 kHz**: servirebbero circa 0,7 dB in più per passata a 4 kHz e
circa 0,9 dB in più a 8 kHz. Il roll-off dell'Effectron nell'anello è più dolce e comincia prima (circa 3 kHz), invece di un taglio netto sopra gli 8 kHz.

### Coda dello Short Echo, 05:22.2–05:24.4 (τ = 170,5 ms)

Decadimento in dB/s per banda, convertito con τ (CSV: `short_echo_tail_decay.csv`): −4,4 dB per ripetizione a 1 kHz. Relativo a 1 kHz:
2 kHz +0,05, 4 kHz −0,6, 8 kHz −0,4, 12 kHz −1,1 e 16 kHz −0,75 dB per ripetizione. Qui però la coda contiene anche il decadimento della
nota del sintetizzatore e il riverbero, quindi il dato è meno pulito di quello del Long Echo. Va comunque nella stessa direzione: perdita
moderata sugli alti, nessun taglio brusco.

## c) Banda del segnale ritardato

- Nella coda del Long Echo (01:32–01:45, solo segnale ritardato) l'energia sopra circa 10 kHz cade molto più in fretta del resto: dopo circa 3 s la coda è limitata a circa 8–10 kHz.
- Nella nota iniziale della coda (06:37.4) c'è energia fino a circa 20 kHz e nelle prime ripetizioni fino a circa 15 kHz, quindi **il primo passaggio nell'ADM non ha un passa-basso netto sotto i 15 kHz**, in linea con le specifiche del manuale (20 Hz–15 kHz +1/−3 dB). Il limite si accumula ripetizione dopo ripetizione (vedi sopra).
- Il confronto diretto tra segnale dry e wet non è possibile: il sorgente cambia nota tra un segmento e l'altro e il mix non è noto.

## d) Rumore di fondo

Pause di HcfrjhGU15U con l'unità accesa nella catena (CSV: `noise_floor_sixtrak_third_octave.csv`):
- RMS **−72…−73 dBFS** (00:23, 01:55, 02:29, 02:40, 03:25).
- Spettro in terzi d'ottava quasi **piatto, da −87 a −91 dBFS per banda tra 1 e 10 kHz**, che cala sopra 12,7 kHz (−94, −103 e −105 dBFS a 12,7, 16 e 20 kHz).
- Sotto 400 Hz ci sono righe a 125, 157 e 315 Hz (ronzio o resti del sintetizzatore).

Pausa iniziale di Q4Nb9Ma0Qeg (00:00–00:01.3), RMS −85 dBFS: spettro in salita verso gli alti, da −109 dBFS a 125 Hz a −90 dBFS a 12,7 kHz.
È un sibilo tipo rumore bianco o pre-enfasi (CSV: `noise_floor_retrosonic_third_octave.csv`).

"Granulo" ADM: nelle code (01:35–01:45 e 06:39–06:41) non si sente un rumore granulare che segue il segnale distinguibile dalla compressione di YouTube.
La coda scende pulita fino a circa −70 dBFS e poi resta il fondo fisso. Con la compressione Opus/AAC questa osservazione è poco conclusiva.

## e) Distorsione e limiter

Nessun segmento con saturazione evidente e isolabile nelle fonti utilizzabili. Il video hSkhwzuZfWw ("overloading the preamp limiter") è stato
scartato perché nel mix ci sono altri limiter e riverberi. **Non misurato.**

## f) Pitch durante la modulazione

Nel Flange Lo la salita e la discesa del ritardo durano quasi lo stesso tempo (6,0 s contro 6,3 s), quindi la variazione di pitch è
**simmetrica**: in salita e in discesa ha la stessa entità con segno opposto. Il manuale nota che il processo ADM aggiunge un leggero tremolo al vibrato:
in queste fonti non l'ho potuto verificare.

## Numeri chiave per la taratura

1. Le gamme dei tasti corrispondono al manuale: Flange Lo con WIDTH al 100% spazza 4,35–16,5 ms, rapporto 3,8, circa 2 ottave.
2. LFO a SPEED minima: circa 13,5 s di periodo, sweep simmetrico.
3. Loop dell'infinite repeat = 1024 ms × DELAY FACTOR (misurato 485,7 ms sia come Long Echo sia come repeat).
4. Perdita per ripetizione rispetto a 1 kHz: **−0,1 dB a 2 kHz, −1,0 a 4 kHz, −2,6/−2,8 a 8 kHz, da −3,5 a −4,9 a 12 kHz** (il secondo valore usa solo le ripetizioni 2…5, prima che le bande alte arrivino al rumore di fondo).
5. Fondo di rumore: circa −88 dBFS per terzo d'ottava, piatto 1–10 kHz, in calo sopra 12 kHz. RMS di circa −73 dBFS nel sistema registrato.

## Metodo e script

Gli script sono in `effectron-ref/scripts/` sul Mac (`common.py`, `flange.py`, `fit_lo.py`, `long_ceps.py`, `steps.py`, `pass_resp.py`, `noise.py`, `tail2.py`), e gli spettrogrammi in `effectron-ref/plots/`.
