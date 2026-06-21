// minikb — firmware v1.2 (PlatformIO / arduino-pico + TinyUSB).
// USB HID 5x11 (3 layer) + joystick (frecce / mouse) + media + LED stato WS2812 + I2C CardKB.
// v1.1: modalita' mouse (Fn tenuto 2s) -> joystick come mouse, solo USB.
// v1.2: in modalita' mouse, Sym tenuto -> joystick = rotella (scroll verticale + orizzontale).
//
// Hardware confermato:
//   Colonne COL0..COL10 = GP0..GP10   (lette, INPUT_PULLUP)
//   Righe   ROW0..ROW4  = GP12..GP16   (pilotate LOW una alla volta)
//   Diodi COL2ROW -> scansione: riga LOW, colonna LOW = tasto premuto.
//   LED WS2812 DIN = GP28 (confermato su hardware; schematico: -> R1 300ohm -> DIN LED1 WS2812B-V6).
//
// Slave I2C compatibile CardKB (0x5F) su SDA=GP20 / SCL=GP21: 1 byte ASCII per tasto.

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// ---------------------------------------------------------------- pin hardware
static const uint8_t COL_PINS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}; // GP0..GP10
static const uint8_t ROW_PINS[] = {12, 13, 14, 15, 16};               // GP12..GP16
#define NCOLS 11
#define NROWS 5
#define LED_PIN 28

// Joystick digitale 5 vie TM-2028 (attivo-basso, comune GND).
// Mappa REALE verificata su hardware (il pinout del modulo era ruotato rispetto allo schematico):
// UP=GP26, DOWN=GP29, LEFT=GP11, RIGHT=GP27, PUSH=GP22.
#define NJOY 5
static const uint8_t  JOY_PINS[NJOY]   = {26, 29, 11, 27, 22};
static const uint8_t  JOY_ARROW[NJOY]  = {0x52, 0x51, 0x50, 0x4F, 0x28}; // Up,Down,Left,Right,Enter
static bool joyState[NJOY];

// ---------------------------------------------------------------- keymap encoding
// Ogni cella e' un uint16_t:
//   0x0000          = NO (cella matrice vuota)
//   0xFFFF          = TRNS (trasparente: ricade sul layer base)
//   0x4001          = FN  (layer momentaneo 1)
//   0x8000 | usage  = Consumer Control (media), usage = HID consumer usage
//   0x00..0xE7      = HID keyboard usage. 0xE0..0xE7 sono i modificatori.
#define NO   0x0000
#define TRNS 0xFFFF
#define FN   0x4001   // momentary -> layer 1 (Fn)
#define SYM  0x4002   // momentary -> layer 2 (Sym)
#define CONS(u) (0x8000 | (u))
// Usage con modificatore forzato (simboli IT + scorciatoie/combo).
// Bit di flag (combinabili) nei bit alti, usage HID nel byte basso:
#define F_SHIFT 0x2000
#define F_ALT   0x1000   // Option su Mac
#define F_CTRL  0x0400
#define F_GUI   0x0800   // Cmd su Mac
#define KS(u)  (F_SHIFT | (u))            // + Shift
#define KA(u)  (F_ALT   | (u))            // + Option
#define KSA(u) (F_SHIFT | F_ALT | (u))    // + Shift + Option
#define KG(u)  (F_GUI   | (u))            // + Cmd   (es. Cmd+C = copia)
#define KC_(u) (F_CTRL  | (u))            // + Ctrl

// HID consumer usages
#define CC_VOLUP  0x00E9
#define CC_VOLDN  0x00EA
#define CC_MUTE   0x00E2
#define CC_PLAY   0x00CD
#define CC_NEXT   0x00B5
#define CC_PREV   0x00B6

// Generato dall'editor (docs/keymap_editor.html), tradotto al layout macOS "Italian - Pro".
static const uint16_t keymap[3][NROWS * NCOLS] = {
  // Layer 0 — BASE
  {
    HID_KEY_1,   HID_KEY_2, HID_KEY_3, HID_KEY_4, HID_KEY_5, HID_KEY_6, HID_KEY_7, HID_KEY_8, HID_KEY_9, HID_KEY_0, HID_KEY_BACKSPACE,
    HID_KEY_TAB, HID_KEY_Q, HID_KEY_W, HID_KEY_E, HID_KEY_R, HID_KEY_T, HID_KEY_Y, HID_KEY_U, HID_KEY_I, HID_KEY_O, HID_KEY_P,
    HID_KEY_CAPS_LOCK, HID_KEY_A, HID_KEY_S, HID_KEY_D, HID_KEY_F, HID_KEY_G, HID_KEY_H, HID_KEY_J, HID_KEY_K, HID_KEY_L, HID_KEY_ENTER,
    HID_KEY_SHIFT_LEFT, HID_KEY_Z, HID_KEY_X, HID_KEY_C, HID_KEY_V, HID_KEY_B, HID_KEY_N, HID_KEY_M, HID_KEY_COMMA, HID_KEY_PERIOD, KS(0x24) /* / */,
    FN, SYM, KS(0x36) /* ; */, 0x2D /* ' */, 0x38 /* - */, HID_KEY_SPACE, HID_KEY_SHIFT_LEFT, KS(0x27) /* = */, KA(0x2F) /* [ */, KA(0x30) /* ] */, 0x64 /* \ */,
  },
  // Layer 1 — Fn (F-key, media, navigazione, scorciatoie taglia/copia/incolla)
  {
    HID_KEY_F1,  HID_KEY_F2, HID_KEY_F3, HID_KEY_F4, HID_KEY_F5, HID_KEY_F6, HID_KEY_F7, HID_KEY_F8, HID_KEY_F9, HID_KEY_F10, HID_KEY_DELETE,
    HID_KEY_F11, HID_KEY_F12, TRNS, TRNS, TRNS, TRNS, TRNS, HID_KEY_PAGE_UP, HID_KEY_HOME, TRNS, HID_KEY_PRINT_SCREEN,
    TRNS, TRNS, CONS(CC_VOLDN), CONS(CC_VOLUP), TRNS, TRNS, TRNS, HID_KEY_PAGE_DOWN, HID_KEY_END, TRNS, TRNS,
    TRNS, CONS(CC_MUTE), CONS(CC_PLAY), CONS(CC_PREV), CONS(CC_NEXT), TRNS, TRNS, TRNS, TRNS, TRNS, TRNS,
    FN, SYM, TRNS, TRNS, TRNS, TRNS, TRNS, KG(HID_KEY_A) /* Sel.tutto */, KG(HID_KEY_X) /* Taglia */, KG(HID_KEY_C) /* Copia */, KG(HID_KEY_V) /* Incolla */,
  },
  // Layer 2 — Sym (Esc, simboli, modificatori)
  {
    HID_KEY_ESCAPE, KS(0x1E) /* ! */, KS(0x1F) /* " */, KS(0x20) /* £ */, KS(0x21) /* $ */, KS(0x22) /* % */, KS(0x23) /* & */, KS(0x25) /* ( */, KS(0x26) /* ) */, KS(0x2D) /* ? */, HID_KEY_DELETE,
    KA(0x64) /* ` */, KA(0x33) /* @ */, KA(0x34) /* # */, KS(0x31) /* § */, KSA(0x2F) /* { */, KSA(0x30) /* } */, KA(0x0A) /* ∞ */, KSA(0x31) /* ◊ */, 0x30 /* + */, 0x38 /* - */, KS(0x30) /* * */,
    TRNS, HID_KEY_CONTROL_LEFT, HID_KEY_ALT_LEFT, HID_KEY_GUI_LEFT, KS(0x64) /* | */, 0x35 /* < */, KS(0x35) /* > */, KA(0x22) /* ~ */, TRNS, TRNS, TRNS,
    TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS,
    FN, SYM, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS, TRNS,
  },
};

// ---------------------------------------------------------------- USB HID
enum { RID_KEYBOARD = 1, RID_CONSUMER = 2, RID_MOUSE = 3 };
static const uint8_t desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE)),
};
Adafruit_USBD_HID usb_hid;
static bool mouseMode = false;   // v1.1: joystick come mouse (Fn tenuto 2s on/off)

// ---------------------------------------------------------------- LED stato
Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);
static bool capsOn = false;

// Output report dall'host: bit0 = NumLock, bit1 = CapsLock, ...
static void hid_led_cb(uint8_t /*id*/, hid_report_type_t /*type*/, uint8_t const *buf, uint16_t len) {
  if (len < 1) return;
  capsOn = (buf[0] & 0x02) != 0;
}

// LED: modalita' mouse = ciano (priorita'); poi Caps/Sym/Fn/Shift. Spento altrimenti.
static void setStatusLed(bool fnActive, bool symActive, bool shiftHeld) {
  uint32_t c;
  if (mouseMode)      c = led.Color(0, 40, 50);  // ciano  = modalita' mouse
  else if (capsOn)    c = led.Color(60, 0, 0);   // rosso  = Caps Lock
  else if (symActive) c = led.Color(0, 40, 0);   // verde  = layer Sym
  else if (fnActive)  c = led.Color(0, 0, 60);   // blu    = layer Fn
  else if (shiftHeld) c = led.Color(50, 30, 0);  // giallo = Shift tenuto
  else                c = 0;                       // spento
  led.setPixelColor(0, c);
  led.show();
}

// ---------------------------------------------------------------- I2C CardKB (slave 0x5F)
// Canale ASCII separato dall'HID: il master legge 1 byte ASCII per tasto (0x00 = niente),
// frecce 0xB4-0xB7, Enter 0x0D, Esc 0x1B, Bksp 0x08, Tab 0x09, Del 0x7F.
// Le scorciatoie PC (Cmd/Ctrl) NON hanno equivalente CardKB -> byte 0x00 (ignorate).
#define I2C_ADDR 0x5F
#define I2C_SDA  20
#define I2C_SCL  21

// Tabelle ASCII US (CardKB e' ASCII puro, indipendente dal layout host).
static const uint8_t ascii_base[NROWS*NCOLS] = {
  '1','2','3','4','5','6','7','8','9','0',0x08,
  0x09,'q','w','e','r','t','y','u','i','o','p',
  0,'a','s','d','f','g','h','j','k','l',0x0D,
  0,'z','x','c','v','b','n','m',',','.','/',
  0,0,';','\'','-',' ',0,'=','[',']','\\',
};
static const uint8_t ascii_shift[NROWS*NCOLS] = {
  '!','@','#','$','%','^','&','*','(',')',0x08,
  0x09,'Q','W','E','R','T','Y','U','I','O','P',
  0,'A','S','D','F','G','H','J','K','L',0x0D,
  0,'Z','X','C','V','B','N','M','<','>','?',
  0,0,':','"','_',' ',0,'+','{','}','|',
};
static const uint8_t ascii_sym[NROWS*NCOLS] = {
  0x1B,'!','"',0,'$','%','&','(',')','?',0x7F,    // £(idx3) non e' ASCII -> 0
  '`','@','#',0,'{','}',0,0,'+','-','*',          // §,∞,◊ non ASCII -> 0
  0,0,0,0,'|','<','>','~',0,0,0,                  // Ctrl/Alt/GUI -> nessun ASCII
  0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,
};
static const uint8_t ascii_fn[NROWS*NCOLS] = {
  0,0,0,0,0,0,0,0,0,0,0x7F,                       // Fn = funzioni: solo Del ha un ASCII
  0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,
};
// joystick -> codici CardKB: UP,DOWN,LEFT,RIGHT,PUSH
static const uint8_t JOY_I2C[NJOY] = {0xB5, 0xB6, 0xB4, 0xB7, 0x0D};

static uint8_t asciiFor(uint8_t layer, uint8_t idx, bool shift) {
  if (layer == 2) return ascii_sym[idx];
  if (layer == 1) return ascii_fn[idx];
  return shift ? ascii_shift[idx] : ascii_base[idx];
}

// coda come la CardKB: 1 byte per read, 0x00 se vuota
#define I2C_QSIZE 32
static volatile uint8_t i2cq[I2C_QSIZE];
static volatile uint8_t i2cqh = 0, i2cqt = 0;
static void i2cPush(uint8_t b) {
  if (!b) return;
  uint8_t n = (i2cqt + 1) % I2C_QSIZE;
  if (n == i2cqh) return;             // piena: scarta
  i2cq[i2cqt] = b; i2cqt = n;
}
static void onI2CRequest() {
  uint8_t b = 0;
  if (i2cqh != i2cqt) { b = i2cq[i2cqh]; i2cqh = (i2cqh + 1) % I2C_QSIZE; }
  Wire.write(b);
}

// ---------------------------------------------------------------- matrice
static bool state[NROWS][NCOLS];      // stato debounced
static uint8_t deb[NROWS][NCOLS];     // contatore debounce
#define DEBOUNCE 4

static void scanMatrix() {
  for (uint8_t r = 0; r < NROWS; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], LOW);
    delayMicroseconds(8);             // assestamento
    for (uint8_t c = 0; c < NCOLS; c++) {
      bool pressed = (digitalRead(COL_PINS[c]) == LOW);
      if (pressed != state[r][c]) {
        if (++deb[r][c] >= DEBOUNCE) { state[r][c] = pressed; deb[r][c] = 0; }
      } else {
        deb[r][c] = 0;
      }
    }
    pinMode(ROW_PINS[r], INPUT);      // hi-Z: non disturba le altre righe
  }
}

static uint8_t joyDeb[NJOY];
static void scanJoystick() {
  for (uint8_t i = 0; i < NJOY; i++) {
    bool pressed = (digitalRead(JOY_PINS[i]) == LOW);
    if (pressed != joyState[i]) {
      if (++joyDeb[i] >= DEBOUNCE) { joyState[i] = pressed; joyDeb[i] = 0; }
    } else {
      joyDeb[i] = 0;
    }
  }
}

// ---------------------------------------------------------------- report build
static void buildAndSend() {
  // 1) layer attivo: Fn / Sym premuti? (Sym ha priorita'). Anche Shift fisico per il LED.
  bool fnActive = false, symActive = false, shiftHeld = false;
  for (uint8_t r = 0; r < NROWS; r++)
    for (uint8_t c = 0; c < NCOLS; c++)
      if (state[r][c]) {
        uint16_t b = keymap[0][r * NCOLS + c];
        if (b == FN)  fnActive = true;
        else if (b == SYM) symActive = true;
        else if (b == HID_KEY_SHIFT_LEFT || b == HID_KEY_SHIFT_RIGHT) shiftHeld = true;
      }
  uint8_t layer = symActive ? 2 : (fnActive ? 1 : 0);

  // conteggio tasti/joystick premuti (per il gesto Fn-da-solo)
  uint8_t pressedCount = 0;
  for (uint8_t r = 0; r < NROWS; r++)
    for (uint8_t c = 0; c < NCOLS; c++) if (state[r][c]) pressedCount++;
  bool joyAny = false;
  for (uint8_t i = 0; i < NJOY; i++) if (joyState[i]) joyAny = true;

  // v1.1: Fn tenuto da solo per 2s -> commuta modalita' mouse
  static uint32_t fnHoldStart = 0; static bool fnHoldDone = false;
  bool fnOnly = fnActive && pressedCount == 1 && !joyAny;
  if (fnOnly) {
    if (fnHoldStart == 0) fnHoldStart = millis();
    else if (!fnHoldDone && millis() - fnHoldStart >= 2000) { mouseMode = !mouseMode; fnHoldDone = true; }
  } else { fnHoldStart = 0; fnHoldDone = false; }

  uint8_t mods = 0;
  uint8_t kc[6] = {0};
  uint8_t nkc = 0;
  uint16_t consumer = 0;

  for (uint8_t r = 0; r < NROWS; r++) {
    for (uint8_t c = 0; c < NCOLS; c++) {
      if (!state[r][c]) continue;
      uint16_t v = keymap[layer][r * NCOLS + c];
      if (layer != 0 && v == TRNS) v = keymap[0][r * NCOLS + c]; // fall-through al base
      if (v == NO || v == TRNS || v == FN || v == SYM) continue;
      if (v & 0x8000) { consumer = v & 0x0FFF; continue; }     // media
      if (v & F_SHIFT) mods |= 0x02;                           // forza Shift
      if (v & F_ALT)   mods |= 0x04;                           // forza Option (LAlt)
      if (v & F_CTRL)  mods |= 0x01;                           // forza Ctrl
      if (v & F_GUI)   mods |= 0x08;                           // forza Cmd (LGui)
      uint8_t u = (uint8_t)v;
      if (u == 0) continue;
      if (u >= 0xE0 && u <= 0xE7) { mods |= (1 << (u - 0xE0)); continue; } // modificatore
      if (nkc < 6) kc[nkc++] = u;                              // tasto normale (6KRO)
    }
  }

  // joystick -> frecce (solo se NON in modalita' mouse)
  if (!mouseMode)
    for (uint8_t i = 0; i < NJOY; i++)
      if (joyState[i] && nkc < 6) kc[nkc++] = JOY_ARROW[i];

  // --- canale I2C CardKB: accoda 1 byte ASCII su ogni nuova pressione ---
  static bool i2cPrev[NROWS][NCOLS];
  for (uint8_t r = 0; r < NROWS; r++)
    for (uint8_t c = 0; c < NCOLS; c++) {
      bool now = state[r][c];
      if (now && !i2cPrev[r][c]) i2cPush(asciiFor(layer, r * NCOLS + c, shiftHeld));
      i2cPrev[r][c] = now;
    }
  // I2C CardKB: il joystick resta SEMPRE frecce (la modalita' mouse e' solo USB)
  static bool joyPrevI2C[NJOY];
  for (uint8_t i = 0; i < NJOY; i++) {
    if (joyState[i] && !joyPrevI2C[i]) i2cPush(JOY_I2C[i]);
    joyPrevI2C[i] = joyState[i];
  }

  static uint8_t lastMods = 0; static uint8_t lastKc[6] = {0};
  static uint16_t lastConsumer = 0;

  if (mods != lastMods || memcmp(kc, lastKc, 6) != 0) {
    if (usb_hid.ready()) {
      usb_hid.keyboardReport(RID_KEYBOARD, mods, kc);
      lastMods = mods; memcpy(lastKc, kc, 6);
    }
  }
  if (consumer != lastConsumer) {
    if (usb_hid.ready()) {
      usb_hid.sendReport16(RID_CONSUMER, consumer); // 0 = rilascio
      lastConsumer = consumer;
    }
  }

  // --- modalita' mouse (solo USB): joystick = cursore, PUSH = click, Sym = rotella ---
  //   Sym tenuto -> joystick = rotella: su/giu' = scroll V, sx/dx = scroll H (cursore fermo).
  if (mouseMode) {
    static uint32_t lastMs = 0; static int accel = 0; static uint8_t lastBtn = 0;
    static uint32_t lastScrollMs = 0; static int scrollAccel = 0;
    int dx = 0, dy = 0;
    if (joyState[2]) dx -= 1;   // LEFT
    if (joyState[3]) dx += 1;   // RIGHT
    if (joyState[0]) dy -= 1;   // UP
    if (joyState[1]) dy += 1;   // DOWN
    uint8_t btn = joyState[4] ? (symActive ? MOUSE_BUTTON_RIGHT : MOUSE_BUTTON_LEFT) : 0;
    uint32_t now = millis();

    if (symActive) {
      // Sym tenuto: rotella. Convenzione HID wheel: V positivo = su, H positivo = destra.
      accel = 0;
      int sv = 0, sh = 0;
      if (dy < 0) sv = 1; else if (dy > 0) sv = -1;   // UP -> su, DOWN -> giu'
      if (dx > 0) sh = 1; else if (dx < 0) sh = -1;   // RIGHT -> destra, LEFT -> sinistra
      if (sv || sh) {
        if (now - lastScrollMs >= 60 && usb_hid.ready()) {  // throttle: rotella piu' lenta del cursore
          lastScrollMs = now;
          if (scrollAccel < 36) scrollAccel++;
          int st = 1 + scrollAccel / 12; if (st > 3) st = 3; // lieve accelerazione (1..3 tacche)
          usb_hid.mouseReport(RID_MOUSE, btn, 0, 0, sv * st, sh * st);
          lastBtn = btn;
        }
      } else {
        scrollAccel = 0;
        if (btn != lastBtn) { usb_hid.mouseReport(RID_MOUSE, btn, 0, 0, 0, 0); lastBtn = btn; }
      }
    } else if (now - lastMs >= 8 && usb_hid.ready()) {
      scrollAccel = 0;
      lastMs = now;
      if (dx || dy) {
        if (accel < 48) accel++;
        int sp = 1 + accel / 6; if (sp > 9) sp = 9;     // accelerazione
        usb_hid.mouseReport(RID_MOUSE, btn, dx * sp, dy * sp, 0, 0);
        lastBtn = btn;
      } else {
        accel = 0;
        if (btn != lastBtn) { usb_hid.mouseReport(RID_MOUSE, btn, 0, 0, 0, 0); lastBtn = btn; }
      }
    }
  }

  setStatusLed(fnActive, symActive, shiftHeld);
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  // --- USB HID PER PRIMA COSA: deve essere registrato prima che il core enumeri,
  //     altrimenti l'host enumera senza endpoint HID e usb_hid.ready() resta false.
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setReportCallback(NULL, hid_led_cb);
  usb_hid.begin();
  // Se il core aveva gia' enumerato (warm reflash), forza la re-enumerazione con l'HID.
  if (TinyUSBDevice.mounted()) { TinyUSBDevice.detach(); delay(10); TinyUSBDevice.attach(); }

  // colonne in input pull-up
  for (uint8_t c = 0; c < NCOLS; c++) pinMode(COL_PINS[c], INPUT_PULLUP);
  // righe hi-Z a riposo
  for (uint8_t r = 0; r < NROWS; r++) pinMode(ROW_PINS[r], INPUT);
  // joystick: ingressi con pull-up (attivi-bassi, comune GND)
  for (uint8_t i = 0; i < NJOY; i++) pinMode(JOY_PINS[i], INPUT_PULLUP);

  // slave I2C CardKB su GP20/GP21, indirizzo 0x5F
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin(I2C_ADDR);
  Wire.onRequest(onI2CRequest);

  led.begin();
  led.setBrightness(255);
  // self-test LED all'avvio: R -> G -> B (conferma cablaggio GP28)
  const uint32_t boot[] = {led.Color(80,0,0), led.Color(0,80,0), led.Color(0,0,80)};
  for (uint8_t i = 0; i < 3; i++) { led.setPixelColor(0, boot[i]); led.show(); delay(180); }
  led.setPixelColor(0, 0); led.show();
}

void loop() {
  scanMatrix();
  scanJoystick();
  buildAndSend();
  delay(1);
}
