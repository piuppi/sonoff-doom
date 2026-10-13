#!/usr/bin/env python3
"""
wad2sonoff.py - Extracts real Doom assets from an IWAD (e.g. doom1.wad shareware)
and generates doom_assets.h for SONOFF DOOM (ESP8285).

Usage (run in the sketch folder):
    python wad2sonoff.py doom1.wad
    python wad2sonoff.py --html-only        (only refresh the embedded web page in doom_assets.h:
                                             no WAD needed, use it after editing index_html.h)
    python wad2sonoff.py doom1.wad --walls STARTAN3,COMPTALL,BROWN1 --list

Pure Python 3, no extra modules required.

What is extracted:
  - PLAYPAL  (palette 0, sent to the browser)
  - COLORMAP (16 of the 32 light levels -> authentic Doom lighting)
  - 3 wall textures + 1 door texture (composed from patches, scaled to 64x64)
  - Imp sprites: walk A-D, pain H, death I-M (64x64 box)
  - Pistol and shotgun: idle, firing with muzzle flash, shotgun pump (pre-scaled to 128x80)
  - Average colour of the floor / ceiling flats
  - Status bar: STBAR, digits, arms, Doomguy faces + all 14 PLAYPAL palettes
    + classic menu graphics, HUD font and sound effects
    (gzip blob served by the Sonoff at /stbar.bin, drawn and played by the browser)
  - Music: MUS lumps (default D_E1M1..D_E1M3), gzip blob served at /music.bin,
    played by the browser with a WebAudio synth
"""
import argparse
import gzip
import os
import struct
import sys

TEX = 64          # wall texture size (column-major, TEX x TEX)
SPR = 64          # sprite box size
LIGHTS = 16       # light levels kept from COLORMAP
TRANSP = 247      # palette index reserved for transparency (black in PLAYPAL)
SCR_W, SCR_H = 128, 80

DEFAULT_WALLS = ["STARTAN3", "COMPTALL", "BROWN1"]
DEFAULT_DOOR = "BIGDOOR2"                 # map tile 4 (rises like a Doom door)
SPRITE_FRAMES = ["TROOA1", "TROOB1", "TROOC1", "TROOD1",       # walk
                 "TROOH1",                                       # pain
                 "TROOI0", "TROOJ0", "TROOK0", "TROOL0", "TROOM0"]  # death
# Weapon frames (layers drawn bottom->top). Order must match WPN_* defines in sonoff_doom.ino
WEAPON_FRAMES = [["PISGA0"], ["PISGB0", "PISFA0"],                       # 0 pistol idle, 1 pistol fire
                 ["SHTGA0"], ["SHTGA0", "SHTFA0"], ["SHTGA0", "SHTFB0"],  # 2 shotgun idle, 3-4 fire+flash
                 ["SHTGB0"], ["SHTGC0"], ["SHTGD0"]]                      # 5-7 shotgun pump
# Menu graphics (classic Doom menu) and sound effects sent to the browser
MENU_PATCHES = ["M_DOOM", "M_NGAME", "M_OPTION", "M_LOADG", "M_SAVEG", "M_RDTHIS", "M_QUITG",
                "M_SKULL1", "M_SKULL2", "M_NEWG", "M_SKILL", "M_JKILL", "M_ROUGH", "M_HURT",
                "M_ULTRA", "M_NMARE", "M_EPISOD", "M_EPI1", "M_EPI2", "M_EPI3", "M_OPTTTL",
                "M_MSENS", "M_SFXVOL", "M_MUSVOL", "M_THERML", "M_THERMM", "M_THERMR", "M_THERMO"]
DEFAULT_SOUNDS = ("DSPISTOL,DSSHOTGN,DSSGCOCK,DSPSTOP,DSSWTCHN,DSSWTCHX,DSPOPAIN,DSBGDTH1,DSPLPAIN,DSPLDETH,"
                  "DSDOROPN,DSDORCLS,DSNOWAY")
FLOOR_FLAT = "FLOOR4_8"
CEIL_FLAT = "CEIL3_5"


# ---------------------------------------------------------------- WAD
class Wad:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        ident, n, ofs = struct.unpack_from("<4sii", self.data, 0)
        if ident not in (b"IWAD", b"PWAD"):
            sys.exit("Error: %s is not a WAD file" % path)
        self.lumps = []
        for i in range(n):
            pos, size, name = struct.unpack_from("<ii8s", self.data, ofs + 16 * i)
            self.lumps.append((cstr(name), pos, size))

    def find(self, name, required=True):
        name = name.upper()
        for nm, pos, size in reversed(self.lumps):
            if nm == name:
                return self.data[pos:pos + size]
        if required:
            sys.exit("Error: lump %s not found in the WAD" % name)
        return None


def cstr(b):
    return b.split(b"\0")[0].decode("ascii", "replace").upper()


def decode_patch(b):
    """Doom picture format -> (w, h, leftoffset, topoffset, pixels[] with None = transparent)."""
    w, h, lo, to = struct.unpack_from("<hhhh", b, 0)
    px = [None] * (w * h)
    for x in range(w):
        p = struct.unpack_from("<i", b, 8 + 4 * x)[0]
        top = -1
        while p < len(b) and b[p] != 0xFF:
            td = b[p]
            top = td if td > top else top + td      # supports "tall" patches
            ln = b[p + 1]
            for i in range(ln):
                y = top + i
                if 0 <= y < h:
                    px[y * w + x] = b[p + 3 + i]
            p += ln + 4
    return w, h, lo, to, px


def load_textures(wad):
    pn = wad.find("PNAMES")
    n = struct.unpack_from("<i", pn, 0)[0]
    pnames = [cstr(pn[4 + 8 * i:12 + 8 * i]) for i in range(n)]
    tex = {}
    for lump in ("TEXTURE1", "TEXTURE2"):
        b = wad.find(lump, required=False)
        if not b:
            continue
        cnt = struct.unpack_from("<i", b, 0)[0]
        for i in range(cnt):
            o = struct.unpack_from("<i", b, 4 + 4 * i)[0]
            name = cstr(b[o:o + 8])
            w, h = struct.unpack_from("<hh", b, o + 12)
            pc = struct.unpack_from("<h", b, o + 20)[0]
            patches = [struct.unpack_from("<hhh", b, o + 22 + 10 * j) for j in range(pc)]
            tex[name] = (w, h, patches)
    return pnames, tex


def compose_texture(wad, pnames, tex, name):
    w, h, patches = tex[name]
    canvas = [0] * (w * h)
    for ox, oy, pi in patches:
        pw, ph, _, _, px = decode_patch(wad.find(pnames[pi]))
        for x in range(pw):
            cx = ox + x
            if not 0 <= cx < w:
                continue
            for y in range(ph):
                v = px[y * pw + x]
                cy = oy + y
                if v is not None and 0 <= cy < h:
                    canvas[cy * w + cx] = v
    return w, h, canvas


# ---------------------------------------------------------------- COLOUR
class Palette:
    def __init__(self, raw):
        self.rgb = [tuple(raw[i * 3:i * 3 + 3]) for i in range(256)]
        self.cache = {}

    def nearest(self, r, g, b):
        k = (r, g, b)
        if k not in self.cache:
            best, bd = 0, 1 << 30
            for i, (pr, pg, pb) in enumerate(self.rgb):
                if i == TRANSP:
                    continue
                d = 3 * (pr - r) ** 2 + 4 * (pg - g) ** 2 + 2 * (pb - b) ** 2
                if d < bd:
                    bd, best = d, i
            self.cache[k] = best
        return self.cache[k]


def resample(pal, getpix, tw, th, ss=4):
    """Area resample: getpix(u, v) on target-space float coords -> palette index or None."""
    out = []
    for ty in range(th):
        row = []
        for tx in range(tw):
            vals = []
            for j in range(ss):
                for i in range(ss):
                    vals.append(getpix(tx + (i + .5) / ss, ty + (j + .5) / ss))
            opaque = [v for v in vals if v is not None]
            if len(opaque) * 2 < len(vals):
                row.append(None)
            elif all(v == opaque[0] for v in opaque):
                row.append(0 if opaque[0] == TRANSP else opaque[0])
            else:
                r = sum(pal.rgb[v][0] for v in opaque) // len(opaque)
                g = sum(pal.rgb[v][1] for v in opaque) // len(opaque)
                b = sum(pal.rgb[v][2] for v in opaque) // len(opaque)
                row.append(pal.nearest(r, g, b))
        out.append(row)
    return out   # out[y][x]


def col_major(img, w, h):
    return [TRANSP if img[y][x] is None else img[y][x] for x in range(w) for y in range(h)]


# ---------------------------------------------------------------- ASSETS
def wall_texture(pal, wad, pnames, tex, name):
    w, h, canvas = compose_texture(wad, pnames, tex, name)
    scale = TEX / h                      # full texture height fits the wall

    def gp(u, v):
        sx = int(u / scale) % w
        sy = min(int(v / scale), h - 1)
        return canvas[sy * w + sx]
    return col_major(resample(pal, gp, TEX, TEX), TEX, TEX)


def sprite_frames(pal, wad):
    pats = [decode_patch(wad.find(n)) for n in SPRITE_FRAMES]
    tallest = max(max(p[3], p[1]) for p in pats)
    widest = max(max(p[2], p[0] - p[2]) * 2 for p in pats)
    scale = min(1.0, (SPR - 1) / tallest, SPR / widest)
    frames = []
    for w, h, lo, to, px in pats:
        bx = SPR / 2 - lo * scale            # leftoffset = horizontal centre
        by = SPR - to * scale                # topoffset  = height above floor

        def gp(u, v, w=w, h=h, px=px, bx=bx, by=by):
            x = int((u - bx) / scale)
            y = int((v - by) / scale)
            if 0 <= x < w and 0 <= y < h and u >= bx and v >= by:
                return px[y * w + x]
            return None
        frames.append(col_major(resample(pal, gp, SPR, SPR), SPR, SPR))
    return frames, scale


def weapon_frames(pal, wad):
    """Compose psprites like R_DrawPSprite on a 320x200 screen, then scale to 128x80."""
    layers_all = []
    for names in WEAPON_FRAMES:
        layers = []
        for n in names:
            w, h, lo, to, px = decode_patch(wad.find(n))
            layers.append((w, h, px, 1 - lo, 32 - to))      # sx=1, sy=WEAPONTOP=32
        layers_all.append(layers)
    w0, h0, _, x0, y0 = layers_all[0][0]
    dy = 200 - (y0 + h0)                                   # idle gun rests on the bottom edge

    out = []
    for layers in layers_all:
        def gp(u, v, layers=layers):
            X = u * 320 / SCR_W
            Y = v * 200 / SCR_H - dy
            for w, h, px, lx, ly in reversed(layers):       # flash drawn on top
                x, y = int(X - lx), int(Y - ly)
                if X >= lx and Y >= ly and x < w and y < h:
                    c = px[y * w + x]
                    if c is not None:
                        return c
            return None
        img = resample(pal, gp, SCR_W, SCR_H, ss=3)
        xs = [x for y in range(SCR_H) for x in range(SCR_W) if img[y][x] is not None]
        ys = [y for y in range(SCR_H) for x in range(SCR_W) if img[y][x] is not None]
        if not xs:
            sys.exit("Error: empty weapon frame after conversion")
        bx0, bx1, by0, by1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
        sub = [row[bx0:bx1] for row in img[by0:by1]]
        out.append((bx0, by0, bx1 - bx0, by1 - by0, col_major(sub, bx1 - bx0, by1 - by0)))
    return out


def flat_index(pal, wad, name, fallback):
    b = wad.find(name, required=False)
    if not b or len(b) < 4096:
        print("  warning: flat %s not found, using default colour" % name)
        return fallback
    r = sum(pal.rgb[v][0] for v in b[:4096]) // 4096
    g = sum(pal.rgb[v][1] for v in b[:4096]) // 4096
    bl = sum(pal.rgb[v][2] for v in b[:4096]) // 4096
    return pal.nearest(r, g, bl)


# ---------------------------------------------------------------- STATUS BAR
def status_names():
    n = ["STBAR", "STARMS", "STTPRCNT"]
    n += ["STTNUM%d" % i for i in range(10)]
    n += ["STYSNUM%d" % i for i in range(10)]
    n += ["STGNUM%d" % i for i in range(2, 8)]
    for l in range(5):                                   # 5 pain levels
        n += ["STFST%d%d" % (l, j) for j in range(3)]    # straight, looking around
        n += ["STFTR%d0" % l, "STFTL%d0" % l]            # turn right / left
        n += ["STFOUCH%d" % l, "STFKILL%d" % l]          # ouch / rampage-grimace
    n += ["STFDEAD0"]
    return n


IMA_STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
            73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
            408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
            1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
            7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
            24623, 27086, 29794, 32767]
IMA_IDX = [-1, -1, -1, -1, 2, 4, 6, 8]


def ima_adpcm(pcm8):
    """Unsigned 8-bit PCM -> IMA ADPCM 4-bit (2 samples per byte, low nibble first, start pred 0 idx 0)."""
    pred, idx, out, nib = 0, 0, bytearray(), []
    for s8 in pcm8:
        x = (s8 - 128) << 8
        step = IMA_STEP[idx]
        diff = x - pred
        code = 0
        if diff < 0:
            code = 8; diff = -diff
        dq = step >> 3
        if diff >= step: code |= 4; diff -= step; dq += step
        step >>= 1
        if diff >= step: code |= 2; diff -= step; dq += step
        step >>= 1
        if diff >= step: code |= 1; dq += step
        pred = pred - dq if code & 8 else pred + dq
        pred = max(-32768, min(32767, pred))
        idx = max(0, min(88, idx + IMA_IDX[code & 7]))
        nib.append(code)
    if len(nib) & 1:
        nib.append(0)
    for i in range(0, len(nib), 2):
        out.append(nib[i] | (nib[i + 1] << 4))
    return bytes(out)


def crop_frame(data, size):
    """Column-major size x size frame -> (x0, y0, w, h, cropped column-major data)."""
    xs = [x for x in range(size) for y in range(size) if data[x * size + y] != TRANSP]
    ys = [y for x in range(size) for y in range(size) if data[x * size + y] != TRANSP]
    if not xs:
        return 0, 0, 1, 1, [TRANSP]
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    out = [data[x * size + y] for x in range(x0, x1) for y in range(y0, y1)]
    return x0, y0, x1 - x0, y1 - y0, out


def sound_lump(b, rate_out):
    """DMX sound lump -> (rate, unsigned 8-bit PCM), optionally resampled down."""
    if len(b) < 8 or struct.unpack_from("<H", b, 0)[0] != 3:
        return None
    rate, n = struct.unpack_from("<HI", b, 2)
    pcm = b[8:8 + n]
    if len(pcm) > 32:                                    # strip DMX 16-byte padding
        pcm = pcm[16:-16]
    if rate_out and rate_out < rate:
        m = int(len(pcm) * rate_out / rate)
        pcm = bytes(pcm[min(len(pcm) - 1, int(i * rate / rate_out))] for i in range(m))
        rate = rate_out
    return rate, pcm


def status_blob(wad, playpal, sounds, rate_out):
    """'SDS2' u16 nPal, nPal*768 palettes, u16 count, then per patch:
    name[8] u16 w u16 h i16 leftofs i16 topofs, w*h bytes row-major (TRANSP = transparent);
    then u8 nSounds, per sound: name[8] u16 rate u32 samples u32 bytes, IMA ADPCM 4-bit data."""
    npal = min(14, len(playpal) // 768)
    out = bytearray(b"SDS2") + struct.pack("<H", npal) + playpal[:npal * 768]
    items, missing = [], []
    names = status_names() + MENU_PATCHES + ["STCFN%03d" % c for c in range(33, 96)]
    for name in names:
        b = wad.find(name, required=False)
        if not b:
            missing.append(name)
            continue
        w, h, lo, to, px = decode_patch(b)
        data = bytes(TRANSP if v is None else (0 if v == TRANSP else v) for v in px)
        items.append(name.encode().ljust(8, b"\0") + struct.pack("<HHhh", w, h, lo, to) + data)
    if missing:
        print("  warning: missing graphics: %s" % ", ".join(missing))
    out += struct.pack("<H", len(items)) + b"".join(items)
    snd = []
    for nm in sounds:
        b = wad.find(nm, required=False)
        r = sound_lump(b, rate_out) if b else None
        if not r:
            print("  warning: sound %s not found" % nm)
            continue
        adp = ima_adpcm(r[1])
        snd.append(nm.encode().ljust(8, b"\0") + struct.pack("<HII", r[0], len(r[1]), len(adp)) + adp)
    out += bytes([len(snd)]) + b"".join(snd)
    return gzip.compress(bytes(out), 9, mtime=0), len(out), len(items), len(snd)


# ---------------------------------------------------------------- MUSIC
def music_blob(wad, names):
    """'SDMU' u8 count, then per track: name[8] u32 len, raw MUS lump."""
    if names == ["ALL"]:
        names = [nm for nm, _, _ in wad.lumps if nm.startswith("D_")]
    tracks = []
    for nm in names:
        b = wad.find(nm, required=False)
        if not b or b[:4] != b"MUS\x1a":
            print("  warning: music %s not found or not in MUS format" % nm)
            continue
        tracks.append(nm.encode().ljust(8, b"\0") + struct.pack("<I", len(b)) + b)
    if not tracks:
        return None, 0
    raw = b"SDMU" + bytes([len(tracks)]) + b"".join(tracks)
    return gzip.compress(raw, 9, mtime=0), len(tracks)


# ---------------------------------------------------------------- OUTPUT
def c_array(f, ctype, name, data, progmem=True):
    f.write("const %s %s[%d]%s = {\n" % (ctype, name, len(data), " PROGMEM" if progmem else ""))
    for i in range(0, len(data), 32):
        f.write("  " + ",".join(str(v) for v in data[i:i + 32]) + ",\n")
    f.write("};\n\n")


def html_gzip(path):
    t = open(path, encoding="utf-8").read()
    i, j = t.find('R"rawliteral('), t.rfind(')rawliteral"')
    if i < 0 or j <= i:
        sys.exit("Error: %s has no R\"rawliteral(...)rawliteral\" block" % path)
    return gzip.compress(t[i + 13:j].encode("utf-8"), 9, mtime=0)


def patch_html(header, html):
    """Replace only the gzip copy of the web page inside an existing doom_assets.h."""
    import re
    if not os.path.exists(header):
        sys.exit("Error: %s not found: run 'python wad2sonoff.py doom1.wad' once first" % header)
    h = open(header, encoding="utf-8").read()
    gz = html_gzip(html)
    block = "#define DA_INDEX_LEN %d\nconst uint8_t DA_INDEX_GZ[%d] PROGMEM = {\n" % (len(gz), len(gz))
    for i in range(0, len(gz), 32):
        block += "  " + ",".join(str(v) for v in gz[i:i + 32]) + ",\n"
    block += "};\n"
    pat = re.compile(r"#define DA_INDEX_LEN \d+\nconst uint8_t DA_INDEX_GZ\[\d+\] PROGMEM = \{.*?\};\n", re.S)
    if not pat.search(h):
        sys.exit("Error: no embedded web page in %s: run 'python wad2sonoff.py doom1.wad' once" % header)
    open(header, "w", encoding="utf-8").write(pat.sub(lambda m: block, h, count=1))
    print("Web page refreshed in %s (%.1f KB gzip)" % (os.path.basename(header), len(gz) / 1024))


def main():
    ap = argparse.ArgumentParser(description="doom1.wad -> doom_assets.h for SONOFF DOOM")
    ap.add_argument("wad", nargs="?", help="doom1.wad (not needed with --html-only)")
    ap.add_argument("--html-only", action="store_true", help="only refresh the embedded web page, no WAD needed")
    ap.add_argument("--walls", default=",".join(DEFAULT_WALLS),
                    help="3 comma-separated textures (map walls 1,2,3)")
    ap.add_argument("--door", default=DEFAULT_DOOR, help="door texture (map tile 4)")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "doom_assets.h"))
    ap.add_argument("--list", action="store_true", help="list available textures and music")
    ap.add_argument("--music", default="D_E1M1,D_E1M2,D_E1M3",
                    help="tracks to include (e.g. D_E1M1,D_E1M4), 'all' = every track, 'none' = no music")
    ap.add_argument("--sounds", default=DEFAULT_SOUNDS,
                    help="sound effects to include (comma-separated DS* lumps), 'none' = no WAD sounds")
    ap.add_argument("--html", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "index_html.h"),
                    help="web page to embed gzip-compressed (saves flash)")
    ap.add_argument("--sfx-rate", type=int, default=11025,
                    help="resample sound effects to this rate to save flash (e.g. 8000)")
    a = ap.parse_args()
    if a.html_only:
        patch_html(a.out, a.html)
        return
    if not a.wad:
        ap.error("the WAD file is required (or use --html-only)")

    wad = Wad(a.wad)
    playpal = wad.find("PLAYPAL")
    pal_raw = playpal[:768]
    pal = Palette(pal_raw)
    pnames, tex = load_textures(wad)
    if a.list:
        print("Available textures (%d):" % len(tex))
        print(", ".join(sorted(tex)))
        print("Available music: %s" % ", ".join(nm for nm, _, _ in wad.lumps if nm.startswith("D_")))
        return

    walls = [n.strip().upper() for n in a.walls.split(",") if n.strip()]
    if len(walls) != 3:
        sys.exit("Error: --walls needs exactly 3 textures")
    walls.append(a.door.strip().upper())                 # tile 4 = door
    for n in walls:
        if n not in tex:
            sys.exit("Error: texture %s not found. Use --list to see the available ones." % n)

    print("Palette and colormap...")
    cm = wad.find("COLORMAP")
    cmap = []
    for l in range(LIGHTS):
        cmap += list(cm[(l * 2) * 256:(l * 2) * 256 + 256])

    print("Walls: %s, door: %s" % (", ".join(walls[:3]), walls[3]))
    wall_data = []
    for n in walls:
        wall_data += wall_texture(pal, wad, pnames, tex, n)

    print("Imp sprites (%d frames)..." % len(SPRITE_FRAMES))
    sprites, sscale = sprite_frames(pal, wad)
    print("Weapons (pistol, shotgun)...")
    weapon = weapon_frames(pal, wad)
    floor_i = flat_index(pal, wad, FLOOR_FLAT, 0)
    ceil_i = flat_index(pal, wad, CEIL_FLAT, 0)

    sounds = [] if a.sounds.strip().lower() == "none" else [n.strip().upper() for n in a.sounds.split(",") if n.strip()]
    print("Status bar, menu, font and %d sound effects..." % len(sounds))
    stbar, stbar_raw, stbar_n, snd_n = status_blob(wad, playpal, sounds, a.sfx_rate)
    music, music_n = None, 0
    if a.music.strip().lower() != "none":
        names = [n.strip().upper() for n in a.music.split(",") if n.strip()]
        print("Music: %s" % ", ".join(names))
        music, music_n = music_blob(wad, names)

    html_gz = None
    if a.html and os.path.exists(a.html):
        html_gz = html_gzip(a.html)
        print("Web page: %s (%.1f KB gzip)" % (os.path.basename(a.html), len(html_gz) / 1024))

    wpn_data, wpn_ofs = [], []
    for fr in weapon:
        wpn_ofs.append(len(wpn_data))
        wpn_data += fr[4]

    with open(a.out, "w") as f:
        f.write("// Generated by wad2sonoff.py from %s - do not edit.\n" % os.path.basename(a.wad))
        f.write("// Contains data derived from the WAD: for personal use only, do not redistribute.\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define DA_TEX %d\n#define DA_SPR %d\n#define DA_LIGHTS %d\n#define DA_TRANSP %d\n" % (TEX, SPR, LIGHTS, TRANSP))
        f.write("#define DA_NWALLS %d\n#define DA_DOOR_TILE 4\n#define DA_NFRAMES %d\n" % (len(walls), len(SPRITE_FRAMES)))
        f.write("#define DA_FLOOR %d\n#define DA_CEIL %d\n" % (floor_i, ceil_i))
        f.write("#define SPR_WALK0 0\n#define SPR_PAIN 4\n#define SPR_DIE0 5\n#define SPR_DIE_N 5\n")
        f.write("#define DA_WPN_FRAMES %d\n\n" % len(weapon))
        f.write("// walls: %s\n" % ", ".join(walls))
        c_array(f, "uint8_t", "DA_PAL", list(pal_raw))
        c_array(f, "uint8_t", "DA_CMAP", cmap)
        c_array(f, "uint8_t", "DA_WALLS", wall_data)
        crops = [crop_frame(fr, SPR) for fr in sprites]
        f.write("#define DA_SPR_CROPPED 1\n")
        sofs, sdata = [], []
        for c in crops:
            sofs.append(len(sdata)); sdata += c[4]
        c_array(f, "uint8_t", "DA_SPR_X0", [c[0] for c in crops], False)
        c_array(f, "uint8_t", "DA_SPR_Y0", [c[1] for c in crops], False)
        c_array(f, "uint8_t", "DA_SPR_W", [c[2] for c in crops], False)
        c_array(f, "uint8_t", "DA_SPR_H", [c[3] for c in crops], False)
        c_array(f, "uint32_t", "DA_SPR_OFS", sofs, False)
        c_array(f, "uint8_t", "DA_SPRITES", sdata)
        c_array(f, "int16_t", "DA_WPN_X", [w[0] for w in weapon], False)
        c_array(f, "int16_t", "DA_WPN_Y", [w[1] for w in weapon], False)
        c_array(f, "int16_t", "DA_WPN_W", [w[2] for w in weapon], False)
        c_array(f, "int16_t", "DA_WPN_H", [w[3] for w in weapon], False)
        c_array(f, "uint32_t", "DA_WPN_OFS", wpn_ofs, False)
        c_array(f, "uint8_t", "DA_WPN", wpn_data)
        f.write("// UI blob: status bar + menu + font + sounds (gzip, %d patches, %d sounds, %d bytes uncompressed)\n"
                % (stbar_n, snd_n, stbar_raw))
        f.write("#define DA_STBAR_LEN %d\n" % len(stbar))
        c_array(f, "uint8_t", "DA_STBAR_GZ", list(stbar))
        if html_gz:
            f.write("// web page (index_html.h, gzip)\n#define DA_INDEX_LEN %d\n" % len(html_gz))
            c_array(f, "uint8_t", "DA_INDEX_GZ", list(html_gz))
        if music:
            f.write("// music blob (gzip, %d tracks)\n" % music_n)
            f.write("#define DA_MUSIC_LEN %d\n" % len(music))
            c_array(f, "uint8_t", "DA_MUSIC_GZ", list(music))

    total = 768 + len(cmap) + len(wall_data) + SPR * SPR * len(sprites) + len(wpn_data) + len(stbar) + (len(music) if music else 0) + (len(html_gz) if html_gz else 0)
    total -= SPR * SPR * len(sprites) - len(sdata)
    print("OK: %s generated (%.1f KB of data in flash, sprite scale %.2f)" % (a.out, total / 1024, sscale))
    print("    UI blob %.1f KB, music %.1f KB" % (len(stbar) / 1024, (len(music) if music else 0) / 1024))
    print("    If Arduino IDE says 'Sketch too big': rerun with --sfx-rate 8000, fewer --sounds or --music D_E1M1")


if __name__ == "__main__":
    main()
