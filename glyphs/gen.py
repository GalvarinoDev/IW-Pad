# Packs the button icons into glyphs.bin for iw-pad.dll (#embed). The icons are used
# with the artist's permission (see glyphs/CREDITS.txt). Needs Pillow. Run after changing one.
# Format, per set (Xbox 360, then PS3), per button in the DLL's Btn order A..START, then the whole d-pad (Console Style
# HUD, drawn as big as the engine allows):
#   u8 w, u8 h  -- icon size in the engine's text units: (lines * 32) + 16, so 0x30 = one text line
#   u8 tw, u8 th, then tw*th BGRA pixels (D3DFMT_A8R8G8B8) at the PNG's own size
# Entries are "name" or ("name", scale): scale converts a bigger drawing (the 40 px "Beeg" face buttons, which look
# smoother scaled down than the 28 px ones scaled up) to the set's size units.
import os
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__))
SETS = [
    ("xbox360", 28, [("A_buttonBeeg40x", 28 / 40), ("B_buttonBeeg40x", 28 / 40), ("X_buttonBeeg40x", 28 / 40),
                     ("Y_buttonBeeg40x", 28 / 40), "LB_Button", "RB_Button", "LT_Button", "RT_Button",
                     "LeftStickClick", "RightStickClick", "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
                     "Back_Button", "Start_Button", "Dpad"]),
    ("ps3", 34, ["Cross_button", "Circle_Button", "Square_Button", "Triangle_Button", "L1_Button", "R1_Button",
                 "L2_Button", "R2_Button", "L3_ButtonFrame1", "R3_ButtonFrame1", "DpadUp", "DpadDown", "DpadLeft",
                 "DpadRight", "Select_Button", "Start_Button", "Dpad"]),
]
LINES = 1.1  # a face button is this many text lines tall; the rest keep their size relative to it
# percent: the whole d-pad behind the HUD's action slots, dark grey like the console. The Xbox 360 art is itself ~53%
# opaque (alpha 136), the PS3 art solid; alpha is scaled and capped at 255, so the Xbox d-pad ends up ~75% opaque.
DPAD_ALPHA, DPAD_BRIGHT = 140, 45

def bleed(im):
    """Gives fully transparent pixels their opaque neighbours' colour: the game filters textures, and black or white
    under alpha 0 shows as a dark/light rim."""
    w, h = im.size
    px = im.load()
    known = {(x, y) for y in range(h) for x in range(w) if px[x, y][3] > 0}
    for _ in range(4):
        add = {}
        for y in range(h):
            for x in range(w):
                if (x, y) in known: continue
                nb = [px[x + dx, y + dy] for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                      if 0 <= x + dx < w and 0 <= y + dy < h and (x + dx, y + dy) in known]
                if nb: add[x, y] = tuple(sum(c[i] for c in nb) // len(nb) for i in range(3)) + (0,)
        for k, v in add.items(): px[k] = v
        known |= add.keys()
    return im

out = bytearray()
for folder, face, names in SETS:
    for n in names:
        n, scale = (n, 1) if isinstance(n, str) else n
        im = bleed(Image.open(os.path.join(here, folder, n + ".png")).convert("RGBA"))
        w, h = im.size
        size = [127, 127] if n == "Dpad" else [round(16 + 32 * LINES * v * scale / face) for v in (w, h)]
        assert all(s < 128 for s in size) and w < 256 and h < 256, n  # signed char in the engine
        r, g, b, a = im.split()
        if n == "Dpad":  # sits dimmed inside the compass
            a = a.point(lambda v: min(255, v * DPAD_ALPHA // 100))
            r, g, b = (c.point(lambda v: v * DPAD_BRIGHT // 100) for c in (r, g, b))
        out += bytes(size + [w, h]) + Image.merge("RGBA", (b, g, r, a)).tobytes()
open(os.path.join(here, "glyphs.bin"), "wb").write(out)
print(len(out), "bytes")
