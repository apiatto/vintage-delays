# Vintage Delays

Plugin delay ispirati ai rack digitali a modulazione delta adattiva (ADM) di fine anni '70 e primi anni '80.
I plugin non usano nomi o grafiche di marchi esistenti: modellano il comportamento e il suono.

Formati: VST3 e Standalone su macOS e Windows, più AU su macOS.

## Struttura

```
core/                         vintage_delay_core, condiviso da tutti i plugin
  include/vdc/Filters.h         filtri (biquad, Butterworth 4° ordine, shelf, allpass)
  include/vdc/AdmDelayEngine.h  motore ADM a clock variabile, configurabile con EngineConfig
  include/vdc/ui/RackLookAndFeel.h  aspetto comune dell'interfaccia (JUCE)
plugins/slopebox/             Slopebox 1024 (ADM, memoria da 1024 ms, flanger/eco)
  Source/SlopeboxConfig.h       configurazione del motore per questo modello
  tools/render.cpp              render offline e test di regressione
  tests/reference_levels.txt    livelli di riferimento del test
plugins/dl4/                  Holdfast 512 (ADM, 1-512 ms a passi, VCO a forma variabile)
  Source/Dl4Config.h            configurazione del motore per questo modello
  tools/dl4_test.cpp            controlli offline (tempi dei passi, repeat, VCO) e demo audio
plugins/<altro>/              ogni nuovo plugin ha la sua cartella con CMakeLists.txt
```

Il motore in `core/` usa solo la libreria standard C++: ogni plugin passa una `vdc::EngineConfig`
(celle di memoria, clock a x1, rapporto del VCO, tabella delle prese, banda, filtro del feedback,
pre/de-enfasi, stereo sintetizzato, costanti ADM) e riceve `vdc::Params` a ogni blocco.

Test di regressione: `ctest --test-dir build -C Release`. Rende i preset di Slopebox sul segnale di
prova interno e confronta i livelli con `tests/reference_levels.txt` (tolleranza 0,05 dB). Se una modifica
al motore cambia il suono di proposito, rigenera il riferimento con
`build/plugins/slopebox/slopebox_render --write-ref plugins/slopebox/tests/reference_levels.txt`.

## Slopebox 1024

Delay, flanger e modulatore modellato sul modello da 1024 ms di un rack ADM dei primi anni '80,
seguendo lo schema a blocchi del manuale originale.

### Come funziona

1. **Input Level** regola il guadagno di tutto il sistema (come sull'originale non c'è un livello d'uscita;
   il Trim è un'aggiunta del plugin). Il LED LIMIT si accende a 0 dBFS, oltre ci sono circa 6 dB di margine
   prima del limitatore; il LED ACTIVE indica segnale sopra −20 dB.
2. Pre-enfasi, somma con il feedback, filtro anti-alias a 16 kHz.
3. Un modulatore delta adattivo (stile CVSD) converte l'audio in un flusso a 1 bit.
4. I bit entrano in una memoria unica da 256K celle. Le prese sono a 1, 4, 16, 64, 256 e 1024 ms con il
   Delay Factor a ×1. Con tutti i pulsanti spenti la memoria è scavalcata (nessun ritardo).
5. Un solo clock, da 256 kHz (×1) a 1,024 MHz (×0,25), guida encoder, memoria e decoder. Il ritardo è
   prese/clock: cambiare il clock cambia anche l'intonazione, come un nastro a velocità variabile.
6. Decoder, filtro di ricostruzione, de-enfasi. Il feedback passa da un passa-basso a 12 kHz.
7. **Infinite Repeat** stacca l'ingresso e fa girare l'intera memoria su se stessa, bit per bit.
   Il Delay Factor ne cambia durata e intonazione (fino a due ottave).
8. L'uscita destra è lo "stereo sintetizzato": i bassi restano in fase, medi e alti vengono sfasati.

### Controlli

| Controllo | Cosa fa |
|-----------|---------|
| Level | Guadagno d'ingresso e di sistema, −24/+26 dB. |
| Feedback | Al centro nessuna rigenerazione; a destra in fase, a sinistra in controfase. |
| Digital Delay | HI 1, MID 4, LO 16 (Flange), DBL 64 (Double), SHORT 256, LONG 1024 ms (Echo), valori a ×1. Ripremere il pulsante acceso lo spegne. |
| Repeat | Infinite repeat sull'intera memoria. |
| Delay Factor | ×0,25 – ×1: ritardo reale = pulsante × fattore. |
| Width | Miscela il controllo del VCO tra Delay Factor (0) e modulatore (100%). Al massimo lo sweep è circa 5:1 e il Delay Factor non conta più. |
| Speed | Velocità dell'LFO, 0,1 – 10 Hz. |
| Modulatore | Sine LFO (come l'originale), Triangle LFO, Envelope (l'inviluppo del segnale pilota il VCO, al posto di un envelope follower sul jack CONTROL). |
| Delay Mix | −100% solo ritardato in controfase, centro solo sorgente, +100% solo ritardato in fase. |
| Stereo | Quantità di stereo sintetizzato sull'uscita destra. |
| Vintage | 100% = conversione ADM completa, 0% = stessa catena con memoria "pulita". |
| Trim | Livello d'uscita del plugin. |

## Holdfast 512

Delay digitale ADM mono ispirato a un rack del 1980 da 1-512 ms: passi di ritardo fissi in tre range,
un Delay Factor ×0,25–×1 per accordare fra un passo e l'altro, un VCO con forma d'onda continua
da triangolo a sinusoide a quadra, feedback, repeat/hold infinito ed espansione di memoria fino a 2 s.
Usa lo stesso motore della Slopebox con un'altra configurazione: banda 15 kHz, memoria 128K celle
a 256 kHz (512 ms a ×1), nessuno stereo sintetizzato (l'originale era mono).

**Valori stimati, da verificare sul manuale originale:** i passi di ogni range (ora 2/4/8, 16/32/64,
128/256/512 ms a ×1), la frequenza del clock, la pre-enfasi e il filtro del feedback.

| Controllo | Cosa fa |
|-----------|---------|
| Level | Guadagno d'ingresso e di sistema, −24/+26 dB; LED LIMIT a 0 dBFS, SIGNAL sopra −20 dB. |
| Range / Step | Tre range (Short, Medium, Long) con tre passi ciascuno; i pulsanti mostrano i ms a ×1. |
| Delay Factor | ×0,25 – ×1: ritardo reale = passo × fattore. Cambia anche l'intonazione, come sull'hardware. |
| Mem x4 | Espansione di memoria: il range Long diventa 512/1024/2048 ms e il repeat dura 2 s. |
| Repeat | Stacca l'ingresso e fa girare l'intera memoria su se stessa, senza degrado. |
| Depth | Quanto il VCO prende il posto del Delay Factor; al 100% copre l'intero range 4:1. |
| Rate | Velocità del VCO, 0,05 – 12 Hz. |
| Shape | Forma del VCO: triangolo, sinusoide, quadra e tutte le forme intermedie. |
| Pedal | Simula il pedale di controllo: se attivo, sostituisce il Delay Factor. |
| Feedback | Al centro nessuna rigenerazione; a destra in fase, a sinistra in controfase. |
| Delay Mix | −100% solo ritardato in controfase, centro solo sorgente, +100% solo ritardato. |
| Mono / Linked Pair | Linked Pair usa due unità, una per lato, come due rack accoppiati per lo stereo. |
| Spread | In Linked Pair, sfasa Delay Factor e Rate dell'unità destra. |
| Vintage | 100% = conversione ADM completa, 0% = memoria "pulita". |
| Trim | Livello d'uscita del plugin. |

Controlli offline: `ctest` esegue `holdfast_test`, che verifica che ogni passo cada al tempo nominale
a ×1 e ×0,25 (memoria normale ed espansa), che il repeat non decada, che il VCO copra il range 4:1 e
che le impostazioni estreme restino stabili. `holdfast_test --render` scrive anche delle demo WAV.

## Build

Serve CMake 3.22 o più recente e un compilatore C++17. JUCE 8.0.4 viene scaricato in automatico.

**macOS** (bastano i Command Line Tools; Xcode completo non è necessario):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release
```

I plugin finiscono in `build/plugins/slopebox/Slopebox_artefacts/Release/` (VST3, AU, Standalone).
Per usarli copia `Slopebox 1024.vst3` in `~/Library/Audio/Plug-Ins/VST3/` e `Slopebox 1024.component`
in `~/Library/Audio/Plug-Ins/Components/`.

**Windows** (Visual Studio 2022):

```sh
cmake -B build
cmake --build build --config Release
```

Copia `Slopebox 1024.vst3` in `C:\Program Files\Common Files\VST3\`.

Con una copia locale di JUCE: `cmake -B build -DJUCE_DIR=/percorso/JUCE`.

La GitHub Action in `.github/workflows/build.yml` compila e testa su macOS e Windows a ogni push e
allega i plugin compilati agli artefatti della run.

## Render offline

```sh
cmake --build build --target slopebox_render
./build/plugins/slopebox/slopebox_render              # segnale di prova interno
./build/plugins/slopebox/slopebox_render mio_file.wav # un tuo file WAV
```

Scrive `slopebox_<preset>.wav` nella cartella corrente, con le impostazioni tipiche del manuale.
