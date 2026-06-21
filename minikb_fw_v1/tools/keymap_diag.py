#!/usr/bin/env python3
"""minikb — diagnostica keymap sul PC.

Legge la seriale di debug del firmware (righe "[key] rX cY DOWN/up ...") e mostra,
per ogni tasto premuto, la posizione nella matrice e COSA E' PROGRAMMATO nei tre
layer (BASE / Fn / Sym). Serve a verificare che ogni tasto fisico sia mappato giusto.

La tabella keymap qui sotto rispecchia quella nel firmware (src/main.cpp): se cambi
il firmware, aggiorna anche questa (o rigenerala).

Uso:
    python3 keymap_diag.py            # auto-rileva la porta (VID 2E8A)
    python3 keymap_diag.py /dev/cu.usbmodemXXXX
"""
import sys, re, time
import serial
import serial.tools.list_ports as list_ports

NCOLS = 11
NROWS = 5

# --- keymap speculare al firmware (BASE, Fn, Sym) ---
BASE = [
    "1","2","3","4","5","6","7","8","9","0","Bksp",
    "Tab","Q","W","E","R","T","Y","U","I","O","P",
    "Caps","A","S","D","F","G","H","J","K","L","Enter",
    "Shift","Z","X","C","V","B","N","M",",",".","/",
    "Fn","Sym",";","'","-","Space","Shift","=","[","]","\\",
]
FN = [
    "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","Del",
    "F11","F12","·","·","·","·","·","PgUp","Home","·","PrtSc",
    "·","·","Vol-","Vol+","·","·","·","PgDn","End","·","·",
    "·","Mute","Play","Prev","Next","·","·","·","·","·","·",
    "Fn","Sym","·","·","·","·","Redo","SelAll","Taglia","Copia","Incolla",
]
SYM = [
    "Esc","!","\"","£","$","%","&","(",")","+","Del",
    "`","@","#","·","·","·","·","·","·","-","@",
    "·","Ctrl","Alt","GUI","·","·","NewTab","Close","SwApp","—","·",
    "·","·","·","·","·","·","·","·","·","·","·",
    "Fn","Sym","·","·","·","·","·","·","·","·","·",
]
LAYERS = [("BASE", BASE), ("Fn", FN), ("Sym", SYM)]

def idx(r, c):
    return r * NCOLS + c

def find_port():
    for p in list_ports.comports():
        if p.vid == 0x2E8A:   # Raspberry Pi RP2040
            return p.device
    return None

CLR = "\033[2J\033[H"
held = set()          # (r,c) attualmente premuti
last = None           # ultimo (r,c) premuto

def active_layer():
    fn  = any(BASE[idx(r, c)] == "Fn"  for (r, c) in held)
    sym = any(BASE[idx(r, c)] == "Sym" for (r, c) in held)
    return 2 if sym else (1 if fn else 0), fn, sym

def render():
    layer, fn, sym = active_layer()
    name, table = LAYERS[layer]
    out = [CLR, f"minikb keymap diag — layer attivo: \033[1m{name}\033[0m  (Fn={'ON' if fn else 'off'} Sym={'ON' if sym else 'off'})", ""]
    for r in range(NROWS):
        row = []
        for c in range(NCOLS):
            cell = table[idx(r, c)]
            if (r, c) in held:
                row.append(f"\033[7m{cell:^6}\033[0m")   # invertito = premuto
            else:
                row.append(f"{cell:^6}")
        out.append(" ".join(row))
    out.append("")
    if last is not None:
        r, c = last
        out.append(f"ultimo tasto: (r{r} c{c})   "
                   f"BASE=\033[1m{BASE[idx(r,c)]}\033[0m   "
                   f"Fn={FN[idx(r,c)]}   Sym={SYM[idx(r,c)]}")
    out.append("\n(premi i tasti; Ctrl-C per uscire)")
    sys.stdout.write("\n".join(out) + "\n")
    sys.stdout.flush()

KEY_RE = re.compile(r"\[key\]\s+r(\d+)\s+c(\d+)\s+(DOWN|up)")

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if not port:
        print("Porta non trovata. Passa il device come argomento."); return
    s = serial.Serial(port, 115200, timeout=0.3)
    global last
    render()
    try:
        while True:
            ln = s.readline().decode(errors="replace").strip()
            m = KEY_RE.search(ln)
            if not m:
                continue
            r, c, st = int(m.group(1)), int(m.group(2)), m.group(3)
            if st == "DOWN":
                held.add((r, c)); last = (r, c)
            else:
                held.discard((r, c))
            render()
    except KeyboardInterrupt:
        s.close()
        print("\nuscita.")

if __name__ == "__main__":
    main()
