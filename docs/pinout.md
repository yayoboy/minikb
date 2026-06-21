# minikb — Pinout RP2040

Matrice **5 righe × 11 colonne = 55 tasti**, diodo per-tasto (D1–D55) → NKRO.

## Pin confermati (estratti dallo schematico `SCH_Schematic1_1-P1_2026-06-20.svg`)

| Funzione | Net      | GPIO        |
|----------|----------|-------------|
| Colonna 0..10 | COL0..COL10 | **GP0 .. GP10** |
| Riga 0..4     | ROW0..ROW4  | **GP12 .. GP16** |

L'RP2040 usa GP0–GP21 per matrice/I2C; inoltre **GP28** è cablato al LED WS2812 (DIN via R1 300Ω).

## Pin confermati su hardware reale (test MicroPython, giugno 2026)

> Verificati direttamente sulla board: matrice via rilevamento pressioni, LED accendendolo,
> I2C dai pull-up esterni rilevati e dal net `SCL0` nello schematico.

| Funzione | GPIO | Come confermato |
|----------|------|-----------------|
| I2C SDA  | **GP20** | pull-up 4.7k rilevato; adiacente a SCL |
| I2C SCL  | **GP21** | net `SCL0` nello schematico; pull-up rilevato |
| LED WS2812 DIN | **GP28** | confermato su hardware (firmware Arduino, giugno 2026): self-test R/G/B su GP28 acceso. NB: GP29 non funziona |
| VBUS sense | **GP11** | da confermare; l'auto-mode usa di default l'enumerazione USB |

> Anche GP19 risulta tirato alto da un pull-up esterno (oltre a GP20/21): probabile terza
> linea del connettore I2C o un segnale ausiliario — non necessaria al firmware.

## Direzione diodi — CONFERMATA: COL2ROW

Verificato su hardware (test direzionale, giugno 2026): pilotando le **colonne** alte si
leggono alte le **righe** (2457 rilevazioni vs 0 nell'altro verso) → anodo sulla colonna,
conduzione **colonna → riga** = **COL2ROW** (convenzione QMK).

Per la scansione attiva-bassa del firmware: si pilotano le **righe** e si leggono le **colonne**.
Impostazioni: QMK `"diode_direction": "COL2ROW"`, KMK `DiodeOrientation.COL2ROW`,
Custom C `DIODE_COL2ROW 1`.

## Auto-mode USB / I2C

Primario (nessuna dipendenza da pin incerti): si inizializza lo stack USB e si attende
`tud_mounted()` per ~1 s. Se enumerato → **modalità USB HID**; altrimenti → **modalità slave I2C 0x5F**.
Opzionale: leggere VBUS-sense (GP11) come discriminante hardware (flag di compilazione).
