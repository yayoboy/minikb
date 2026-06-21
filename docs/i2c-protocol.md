# minikb — Protocollo I2C (compatibile CardKB)

## Scommario

- **Ruolo**: slave / target I2C.
- **Indirizzo**: `0x5F` (7-bit), identico al M5Stack CardKB.
- **Velocità**: 100 kHz (standard) e 400 kHz (fast) supportati.
- **Lettura master**: il master esegue una read di **1 byte**.
  - `0x00` → nessun tasto premuto dall'ultima lettura.
  - altrimenti → codice ASCII (o codice speciale) del tasto.

## Semantica

Identica a CardKB: il dispositivo mantiene **l'ultimo carattere** prodotto e lo restituisce
alla prossima read; dopo la read il buffer torna a `0x00` finché non viene premuto un nuovo tasto.
Il byte è **già risolto** (Shift/Fn applicati internamente), quindi l'host riceve il carattere
finale, non la posizione fisica del tasto.

## Codici (vedi anche docs/keymap.md)

```
ASCII stampabili : 0x20–0x7E
Enter            : 0x0D
Esc              : 0x1B
Backspace        : 0x08
Tab              : 0x09
Frecce  L/U/D/R  : 0xB4 / 0xB5 / 0xB6 / 0xB7   (convenzione CardKB — VERIFICARE)
```

## Esempio host (Raspberry Pi / Linux, Python smbus2)

```python
from smbus2 import SMBus, i2c_msg
ADDR = 0x5F
with SMBus(1) as bus:
    while True:
        msg = i2c_msg.read(ADDR, 1)
        bus.i2c_rdwr(msg)
        b = list(msg)[0]
        if b:
            print(hex(b), repr(chr(b)) if 0x20 <= b < 0x7f else "")
```

## Esempio host (Arduino / ESP32)

```cpp
#include <Wire.h>
void setup() { Wire.begin(); Serial.begin(115200); }
void loop() {
  Wire.requestFrom(0x5F, 1);
  if (Wire.available()) {
    uint8_t b = Wire.read();
    if (b) Serial.println(b, HEX);
  }
  delay(10);
}
```

## Validazione drop-in

1. `i2cdetect -y 1` deve mostrare `0x5F`.
2. Confrontare i byte restituiti (lettere, numeri, frecce, enter) con quelli di un **CardKB reale**.
3. Verificare il comportamento "un carattere per pressione" (no autorepeat sul canale I2C, come CardKB).
