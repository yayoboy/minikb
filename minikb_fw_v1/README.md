# minikb — firmware v1.1

Firmware **collaudato su hardware** per la minikb: micro-tastiera ortholineare 5×11 su
**RP2040**, con joystick a 5 vie. Sviluppato in **PlatformIO** (core arduino-pico
earlephilhower + TinyUSB).

Una sola tastiera, tre interfacce in parallelo:

- ⌨️ **USB HID** — tastiera completa a 3 layer, mappata per il layout host **macOS "Italian - Pro"**.
- 🕹️ **Joystick** TM-2028 → frecce (USB) e, in **modalità mouse**, cursore + click (v1.1).
- 🔌 **Slave I2C compatibile M5Stack CardKB** (`0x5F`) — 1 byte ASCII per tasto.
- 💡 **LED WS2812** di stato (Caps / Sym / Fn / Shift / mouse).

## Anteprima del layout

![Layout minikb](../docs/keymap_layout.svg)

> Centro di ogni tasto = layer **BASE** · in alto a destra (blu) = **Sym** · in basso a destra (grigio) = **Fn**.
> Tasti **Fn** (blu) e **Sym** (verde) nei colori del LED. Joystick integrato in basso a sinistra.

Mappa modificabile dall'editor visuale: [`docs/keymap_editor.html`](../docs/keymap_editor.html)
(esporta JSON, SVG e PNG stampabile).

---

## Hardware (pin confermati su board reale)

| Funzione | GPIO |
|----------|------|
| Colonne COL0..COL10 | GP0–GP10 (input pull-up) |
| Righe ROW0..ROW4 | GP12–GP16 (pilotate LOW) |
| Diodi | COL2ROW (riga LOW, colonna LOW = premuto) |
| LED WS2812 (DIN) | **GP28** (via R1 300 Ω) |
| I2C CardKB slave (0x5F) | SDA=GP20, SCL=GP21 |
| Joystick TM-2028 (comune GND, attivo-basso) | UP=GP26, DOWN=GP29, LEFT=GP11, RIGHT=GP27, PUSH=GP22 |

> Note verificate a mano: il LED è su **GP28** (non GP29 come indicava la doc) e il pinout del
> modulo joystick è **ruotato** rispetto allo schematico — i valori qui sopra sono quelli reali.

---

## Layer

Layout host di riferimento: **macOS "Italian - Pro"**. I caratteri sono già tradutti nel
firmware (es. `;` = Shift+`,`, `@` = Option+ò, `{` = Shift+Option+è), così escono giusti
senza cambiare il layout del Mac.

- **BASE** — lettere, numeri, punteggiatura, Space, Bksp, Caps, Shift. Fn/Sym in basso a sinistra.
- **Fn** (tieni Fn) — F1–F12, media (Vol/Mute/Play/Prev/Next), PgUp/PgDn/Home/End, Del, PrtSc,
  e scorciatoie **⌘A / ⌘X / ⌘C / ⌘V** (Sel.tutto / Taglia / Copia / Incolla).
- **Sym** (tieni Sym) — **Esc**, simboli (`! " £ $ % & ( ) ?`, `@ # § { } ∞ ◊ + - * | < > ~`, `` ` ``),
  e i modificatori **Ctrl / Alt / GUI** su A / S / D.

**Joystick**: ↑ ↓ ← → + centro = Invio (PUSH).

**LED di stato** (spento a riposo): mouse = ciano · Caps = rosso · Sym = verde · Fn = blu · Shift = giallo.

---

## Modalità mouse (v1.1) — solo USB

Tieni **Fn da solo per 2 secondi** per attivare/disattivare la modalità mouse (il LED diventa **ciano**):

- **Cursore**: joystick, con **accelerazione** (preciso sui piccoli spostamenti, veloce sulle lunghe distanze).
- **Click sinistro**: PUSH (centro joystick).
- **Click destro**: **Sym + PUSH**.
- La tastiera continua a scrivere normalmente.
- Esci tenendo di nuovo **Fn 2 s**.

> La modalità mouse riguarda **solo l'USB**. Sul canale I2C il joystick resta sempre frecce.

---

## I2C compatibile CardKB (0x5F)

Un host (Raspberry Pi, ESP32, …) su SDA=GP20 / SCL=GP21 legge **1 byte ASCII per tasto** all'indirizzo `0x5F`:

- Minuscole di default; **Shift** → maiuscole/simboli US; **Sym** → secondo valore.
- Speciali: Enter `0x0D`, Esc `0x1B`, Backspace `0x08`, Tab `0x09`, Del `0x7F`, Space `0x20`.
- Joystick → frecce CardKB: Left `0xB4`, Up `0xB5`, Down `0xB6`, Right `0xB7`, centro → Enter.
- `0x00` = nessun tasto (coda FIFO, 1 byte per read). ASCII **US puro**, indipendente dal layout.
- Senza equivalente CardKB → `0x00`: scorciatoie ⌘, modificatori, caratteri non-ASCII (`£ § ∞ ◊`).

```python
# host Raspberry Pi (smbus2)
from smbus2 import SMBus, i2c_msg
with SMBus(1) as bus:
    msg = i2c_msg.read(0x5F, 1); bus.i2c_rdwr(msg)
    b = list(msg)[0]
    if b: print(hex(b), chr(b) if 32 <= b < 127 else "")
```

> L'I2C va testato con un **master esterno** (non verificabile via USB dal Mac).

---

## Build e flash

Richiede [PlatformIO](https://platformio.org/) (`pip install platformio`).

```bash
cd minikb_fw_v1
pio run                 # compila -> .pio/build/minikb/firmware.uf2
pio run -t upload       # flash (reset automatico 1200bps)
```

In alternativa copia `firmware.uf2` sull'RP2040 in modalità BOOTSEL (drive `RPI-RP2`).

---

## Rimappare i tasti

1. Apri [`docs/keymap_editor.html`](../docs/keymap_editor.html) nel browser.
2. Clicca una cella → scrivi un carattere, o clicca un **simbolo** / **funzione** / **scorciatoia**.
3. **Esporta JSON** (e, se vuoi, **SVG/PNG** stampabile).
4. Dal JSON si rigenera la `keymap[3][...]` in `src/main.cpp`; i caratteri si traducono al layout
   con `tools/it_layout_map.py` (stampa usage HID + Shift/Option per ogni carattere).

| File | A cosa serve |
|------|--------------|
| `docs/keymap_editor.html` | Editor visuale (layout fisico, joystick, simboli, export JSON/SVG/PNG) |
| `tools/it_layout_map.py` | Estrae da macOS la combinazione HID per ogni carattere del layout attivo |
| `tools/keymap_diag.py` | Probe seriale dei tasti (richiede un build con diagnostica) |

---

## Changelog

- **v1.1** — modalità mouse (joystick come mouse, Fn 2 s on/off; click sx = PUSH, dx = Sym+PUSH);
  layer Sym ampliato; editor con layout fisico, joystick integrato ed export SVG/PNG.
- **v1.0** — tastiera USB HID (3 layer, layout Italian-Pro, scorciatoie), joystick frecce,
  LED di stato, slave I2C CardKB.
