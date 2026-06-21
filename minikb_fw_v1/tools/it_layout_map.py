#!/usr/bin/env python3
"""Estrae dalla macOS attiva (layout corrente, es. 'Italian - Pro') la combinazione
HID necessaria a produrre ciascun carattere ASCII.

Per ogni virtual keycode + stato modificatori usa Carbon UCKeyTranslate per sapere
quale carattere esce, poi converte il virtual keycode in usage HID. Output: per ogni
carattere voluto, 'usage' HID e se serve Shift / Option (Alt su Mac).

Uso: python3 it_layout_map.py "; ' - = [ ] \\ / : _ @ # ( ) ? !"
(senza argomenti usa un set di default)
"""
import ctypes, ctypes.util, sys

# macOS virtual keycode -> HID usage (tabella fissa ANSI/ISO)
VK2HID = {
 0x00:0x04,0x01:0x16,0x02:0x07,0x03:0x09,0x04:0x0B,0x05:0x0A,0x06:0x1D,0x07:0x1B,
 0x08:0x06,0x09:0x19,0x0A:0x64,0x0B:0x05,0x0C:0x14,0x0D:0x1A,0x0E:0x08,0x0F:0x15,
 0x10:0x1C,0x11:0x17,0x12:0x1E,0x13:0x1F,0x14:0x20,0x15:0x21,0x16:0x23,0x17:0x22,
 0x18:0x2E,0x19:0x26,0x1A:0x24,0x1B:0x2D,0x1C:0x25,0x1D:0x27,0x1E:0x30,0x1F:0x12,
 0x20:0x18,0x21:0x2F,0x22:0x0C,0x23:0x13,0x24:0x28,0x25:0x0F,0x26:0x0D,0x27:0x34,
 0x28:0x0E,0x29:0x33,0x2A:0x31,0x2B:0x36,0x2C:0x38,0x2D:0x11,0x2E:0x10,0x2F:0x37,
 0x30:0x2B,0x31:0x2C,0x32:0x35,0x33:0x2A,0x35:0x29,
}

CF = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))
Carbon = ctypes.CDLL(ctypes.util.find_library("Carbon"))

CFDataGetBytePtr = CF.CFDataGetBytePtr
CFDataGetBytePtr.restype = ctypes.c_void_p
CFDataGetBytePtr.argtypes = [ctypes.c_void_p]

Carbon.TISCopyCurrentKeyboardLayoutInputSource.restype = ctypes.c_void_p
Carbon.TISGetInputSourceProperty.restype = ctypes.c_void_p
Carbon.TISGetInputSourceProperty.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
Carbon.LMGetKbdType.restype = ctypes.c_uint8

kTISPropertyUnicodeKeyLayoutData = ctypes.c_void_p.in_dll(
    Carbon, "kTISPropertyUnicodeKeyLayoutData")

UCKeyTranslate = Carbon.UCKeyTranslate
UCKeyTranslate.argtypes = [
    ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint32,
    ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32),
    ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
    ctypes.POINTER(ctypes.c_uint16 * 4)]

def layout_ptr():
    src = Carbon.TISCopyCurrentKeyboardLayoutInputSource()
    data = Carbon.TISGetInputSourceProperty(src, kTISPropertyUnicodeKeyLayoutData)
    return CFDataGetBytePtr(data)

def translate(layout, vk, mod, kbd):
    dead = ctypes.c_uint32(0)
    n = ctypes.c_ulong(0)
    buf = (ctypes.c_uint16 * 4)()
    UCKeyTranslate(layout, vk, 0, mod, kbd, 1, ctypes.byref(dead), 4,
                   ctypes.byref(n), ctypes.byref(buf))
    if n.value >= 1 and buf[0] != 0:
        return chr(buf[0])
    return None

# modifierKeyState per UCKeyTranslate: shift=2, option=8
MODS = [(0, False, False), (2, True, False), (8, False, True), (10, True, True)]

def build_map():
    layout = layout_ptr()
    kbd = Carbon.LMGetKbdType()
    m = {}
    for vk in VK2HID:
        for mstate, shift, opt in MODS:
            ch = translate(layout, vk, mstate, kbd)
            if ch and ch not in m:   # primo trovato = combo piu' semplice
                m[ch] = (VK2HID[vk], shift, opt)
    return m

def main():
    chars = sys.argv[1] if len(sys.argv) > 1 else "; ' - = [ ] \\ / : _ ? ! @ # ( ) & % $ \" | { } < > ^ ~ ` * + ="
    wanted = [c for c in chars if not c.isspace()]
    m = build_map()
    print(f"{'char':>4}  {'HID':>5}  Shift  Option   -> macro firmware")
    for c in wanted:
        if c in m:
            u, sh, op = m[c]
            if sh and op: macro = f"KSA(0x{u:02X})"
            elif sh:      macro = f"KS(0x{u:02X})"
            elif op:      macro = f"KA(0x{u:02X})"
            else:         macro = f"0x{u:02X}"
            print(f"{c!r:>4}  0x{u:02X}   {str(sh):>5}  {str(op):>6}   {macro}")
        else:
            print(f"{c!r:>4}   ---  (non producibile su questo layout con Shift/Option)")

if __name__ == "__main__":
    main()
