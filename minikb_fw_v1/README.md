# minikb_fw_v1 — firmware funzionante (PlatformIO / arduino-pico)

Firmware **collaudato su hardware** per la minikb (RP2040, matrice 5×11 + joystick).
Tastiera **USB HID** con 3 layer, scorciatoie macOS, LED di stato e joystick.

> Stato: ✅ tastiera, layer, LED, joystick, **I2C CardKB (0x5F)** funzionanti.

---

## Hardware (pin confermati su board reale)

| Funzione | GPIO |
|----------|------|
| Colonne COL0..COL10 | GP0–GP10 (input pull-up) |
| Righe ROW0..ROW4 | GP12–GP16 (pilotate LOW) |
| Diodi | COL2ROW (riga LOW, colonna LOW = premuto) |
| LED stato WS2812 (DIN) | **GP28** (via R1 300Ω) |
| I2C CardKB slave (0x5F) | SDA=GP20, SCL=GP21 |
| Joystick TM-2028 (comune GND, attivo-basso) | UP=GP26, DOWN=GP29, LEFT=GP11, RIGHT=GP27, PUSH=GP22 |

> Nota: il LED è su **GP28** (lo schematico/doc indicavano GP29: errato).
> Il pinout del modulo joystick è **ruotato** rispetto allo schematico: i valori sopra sono
> quelli verificati a mano sull'hardware.

---

## Layer

Layout host di riferimento: **macOS "Italian - Pro"**. I caratteri sono già tradotti nel
firmware (es. `;` = Shift+`,`, `@` = Option+ò) così escono giusti senza cambiare il layout del Mac.

**BASE**
```
 1     2    3    4    5    6    7    8    9    0    Bksp
 Tab   Q    W    E    R    T    Y    U    I    O    P
 Caps  A    S    D    F    G    H    J    K    L    Enter
 Shift Z    X    C    V    B    N    M    ,    .    /
 Fn    Sym  ;    '    -    Space Shift =   [    ]    \
```

**Fn** (tieni Fn)
```
 F1   F2   F3   F4   F5   F6   F7   F8   F9   F10  Del
 F11  F12  ·    ·    ·    ·    ·    PgUp Home ·    PrtSc
 ·    ·    Vol- Vol+ ·    ·    ·    PgDn End  ·    ·
 ·    Mute Play Prev Next ·    ·    ·    ·    ·    ·
 Fn   Sym  ·    ·    ·    ·    Ripeti SelTutto Taglia Copia Incolla
```
Scorciatoie: Ripeti=⌘⇧Z · SelTutto=⌘A · Taglia=⌘X · Copia=⌘C · Incolla=⌘V

**Sym** (tieni Sym)
```
 Esc  !    "    £    $    %    &    (    )    +    Del
 `    @    #    ·    ·    ·    ·    ·    ·    -    @
 ·    Ctrl Alt  GUI  ·    ·    NuovaTab Chiudi SwitchApp · ·
 ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 Fn   Sym  ·    ·    ·    ·    ·    ·    ·    ·    ·
```
NuovaTab=⌘T · Chiudi=⌘W · SwitchApp=⌘⇥

**Joystick**: ↑↓←→ + centro (PUSH) = Invio. Sostituisce i tasti freccia.

**LED di stato** (spento a riposo): Caps Lock = rosso · Sym = verde · Fn = blu · Shift = giallo.

---

## Build e flash

Richiede [PlatformIO](https://platformio.org/) (`pip install platformio`).

```bash
cd minikb_fw_v1
pio run                                   # compila -> .pio/build/minikb/firmware.uf2
pio run -t upload                         # flash (reset automatico 1200bps)
# oppure: copia firmware.uf2 sull'RP2040 in modalita' BOOTSEL (drive RPI-RP2)
```

Core: arduino-pico (earlephilhower) + Adafruit TinyUSB + Adafruit NeoPixel (vedi `platformio.ini`).

---

## Rimappare i tasti

1. Apri `docs/keymap_editor.html` nel browser.
2. Clicca una cella → scrivi un carattere o scegli funzione/scorciatoia.
3. **Esporta JSON**.
4. Rigenera la `keymap[3][...]` in `src/main.cpp` dal JSON (i caratteri vanno tradotti al
   layout con `tools/it_layout_map.py`, che stampa usage HID + Shift/Option per ogni carattere).

`docs/keymap_layers.html` mostra i layer in sola lettura.

---

## Strumenti

| File | A cosa serve |
|------|--------------|
| `docs/keymap_editor.html` | Editor visuale della keymap (esporta JSON) |
| `docs/keymap_layers.html` | Vista dei tre layer |
| `tools/it_layout_map.py` | Estrae da macOS la combinazione HID per ogni carattere del layout attivo |
| `tools/keymap_diag.py` | Probe seriale: mostra il tasto premuto e la mappatura per layer |

> `keymap_diag.py` richiede un build con la **diagnostica seriale** attiva. Questa v1 è la
> versione "pulita" senza diagnostica: per usare il probe, riattiva le `Serial.printf` di debug.

---

## I2C compatibile CardKB (0x5F)

La minikb funziona anche come **slave I2C compatibile M5Stack CardKB**: un host (Raspberry Pi,
ESP32, ecc.) collegato a SDA=GP20 / SCL=GP21 legge **1 byte ASCII per tasto** all'indirizzo `0x5F`.

- Lettere minuscole di default; **Shift** → maiuscole/simboli US; **Sym** → secondo valore.
- Speciali: Enter `0x0D`, Esc `0x1B`, Backspace `0x08`, Tab `0x09`, Del `0x7F`, Space `0x20`.
- Joystick → frecce CardKB: Left `0xB4`, Up `0xB5`, Down `0xB6`, Right `0xB7`, centro → Enter.
- `0x00` = nessun tasto. I tasti vengono accodati (coda FIFO), 1 byte per ogni read.
- L'ASCII è **US puro** (indipendente dal layout del Mac). Le scorciatoie PC (⌘C/⌘V…) e i
  modificatori non hanno equivalente CardKB → restituiscono `0x00`. Gli accenti non entrano in
  1 byte ASCII → non disponibili sul canale I2C (sono solo lato USB HID).

Esempio host (Raspberry Pi, Python `smbus2`):
```python
from smbus2 import SMBus, i2c_msg
with SMBus(1) as bus:
    msg = i2c_msg.read(0x5F, 1)
    bus.i2c_rdwr(msg)
    b = list(msg)[0]
    if b: print(hex(b), chr(b) if 32 <= b < 127 else "")
```

> Nota: l'I2C va testato con un **master esterno** (non verificabile via USB dal Mac).
> USB HID e I2C funzionano in parallelo.

## Limiti / TODO

- Lettere accentate `à è é ì ò ù`: disponibili solo via USB HID, non sul canale I2C (1 byte ASCII).
