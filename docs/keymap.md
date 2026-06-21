# minikb — Keymap

Dalla serigrafia `silkscreen_aligned.jpg`. Matrice logica 5×11 (riga, colonna).
Posizioni vuote = `XXX` (no key in quella cella della matrice).

`Fn` = layer momentaneo (raise). `Sym` = layer simboli. `Shift/Ctrl/Alt/GUI` = modificatori.

## Layer 0 — BASE

```
Col:    0     1     2     3     4     5     6     7     8     9     10
R0:    Esc    1     2     3     4     5     6     7     8     9    Bksp
R1:    Tab    Q     W     E     R     T     Y     U     I     O     P
R2:    Caps   A     S     D     F     G     H     J     K     L    Enter
R3:    Shift  Z     X     C     V     B     N     M     ,     .     /
R4:    Fn     Ctrl  Alt   GUI   Sym  Space  ;     '    Left  Down  Up/Right
```

> Nota: la fila R4 ha meno legende fisiche dei 11 fori; le celle non popolate sono `XXX`.
> Le posizioni esatte di Sym/`;`/`'`/ScrUp/ScrDn vanno riconciliate con EasyEDA al primo flash.

## Layer 1 — Fn (raise)

```
Col:    0     1     2     3     4     5     6     7     8     9     10
R0:     `    F1    F2    F3    F4    F5    F6    F7    F8    F9    Del
R1:    F11    .   Vol+   .     .     .     .   PgUp   Up    .   PrtSc
R2:    F12    .   Vol-   .     .     .   Home  Left  Down  Right  .
R3:     .   Mute  Play  Prev  Next   .   End  PgDn   .     .     .
R4:     .     .     .     .     .     .     .     .     .     .     .
```

Frecce stile vim su **I/J/K/L** (↑ ← ↓ →). Media su **Z/X/C/V**. Volume su **W/S**.

## Mappa ASCII per I2C (CardKB)

In modalità I2C il firmware ritorna **1 byte ASCII già risolto** (applicati Shift/Fn).
Tasti senza ASCII (modificatori puri, media) → `0x00`.

| Tasto | byte | Tasto | byte |
|-------|------|-------|------|
| lettere a–z | 0x61–0x7A | A–Z (con Shift) | 0x41–0x5A |
| 0–9 | 0x30–0x39 | Space | 0x20 |
| Enter | 0x0D | Esc | 0x1B |
| Backspace | 0x08 | Tab | 0x09 |
| Left | 0xB4 | Up | 0xB5 |
| Down | 0xB6 | Right | 0xB7 |

> I codici frecce 0xB4–0xB7 seguono la convenzione M5 CardKB — **verificare sul datasheet
> ufficiale** e confrontare con un CardKB reale prima di dichiarare la compatibilità drop-in.
