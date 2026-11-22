# Builds zz_iwpad.iwd (CoD2 / Quake 3 family button icons) from glyphs.bin: one IWi v5 image (32-bit BGRA, no mips,
# padded to power-of-two sides with transparent pixels) and one 2D material per button, named iwpad_<set>_<button>.
# Also packs cod2/ (menus that replace stock ones).
# usage: python3 glyphs/cod2_iwd.py out.iwd   (stdlib only)
import os, struct, sys, zipfile

here = os.path.dirname(os.path.abspath(__file__))
SETS, BUTTONS = ("xbox", "ps3"), 16  # glyphs.bin per set: the 16 buttons (A..START), then the d-pad (unused here)

# 2D material header from the stock materials/hud_grenadeicon (techset "2d", one colorMap, sampler 0x30)
TEMPLATE = bytes.fromhex(
    "53000000630000000000000000 1b0101 0000000000 00a000 000000004000 4000"
    "00000000000000000100000065511228 02000000010000005000000044000000 50000000".replace(" ", ""))
assert len(TEMPLATE) == 0x44


def pow2(v):
    p = 1
    while p < v: p *= 2
    return p


def iwi(tw, th, bgra):
    pw, ph = pow2(tw), pow2(th)
    px = bytearray(pw * ph * 4)  # transparent padding; the DLL draws only the tw x th corner
    for y in range(th):
        px[y * pw * 4:(y * pw + tw) * 4] = bgra[y * tw * 4:(y + 1) * tw * 4]
    size = 0x1c + len(px)
    # IWi v5: format 1 = ARGB32, flags 3 = no mips (like the stock $white), w, h, depth, then 4 offsets (= file size)
    return struct.pack("<3sBBBHHH4I", b"IWi", 5, 1, 3, pw, ph, 1, size, size, size, size) + bytes(px), pw, ph


def material(name, image, pw, ph):
    m = bytearray(TEMPLATE)
    strings = b"2d\0"
    name_off = 0x50 + len(strings); strings += name.encode() + b"\0"
    image_off = 0x50 + len(strings); strings += image.encode() + b"\0"
    cmap_off = 0x50 + len(strings); strings += b"colorMap\0"
    struct.pack_into("<II", m, 0, name_off, image_off)
    struct.pack_into("<HH", m, 0x1c, pw, ph)
    return bytes(m) + struct.pack("<III", cmap_off, 0x30, image_off) + strings


def main(out):
    data = open(os.path.join(here, "glyphs.bin"), "rb").read()
    pos = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for s in SETS:
            for i in range(BUTTONS + 1):
                w, h, tw, th = data[pos:pos + 4]
                bgra = data[pos + 4:pos + 4 + tw * th * 4]
                pos += 4 + tw * th * 4
                if i == BUTTONS: continue
                name = f"iwpad_{s}_{i}"
                img, pw, ph = iwi(tw, th, bgra)
                z.writestr(f"images/{name}.iwi", img)
                z.writestr(f"materials/{name}", material(name, name, pw, ph))
        # transparent stand-in for the menu cursor while the pad is in use (the DLL swaps the cursor's material)
        img, pw, ph = iwi(1, 1, bytes(4))
        z.writestr("images/iwpad_blank.iwi", img)
        z.writestr("materials/iwpad_blank", material("iwpad_blank", "iwpad_blank", pw, ph))
        # menus overriding the stock ones (zz_ sorts after iw_*.iwd, so these win): the Look screen's controller rows
        menus = os.path.join(here, "..", "cod2")
        for root, _, files in os.walk(menus):
            for f in files:
                path = os.path.join(root, f)
                z.write(path, os.path.relpath(path, menus).replace(os.sep, "/"))
    assert pos == len(data), (pos, len(data))
    print(out, os.path.getsize(out), "bytes")


if __name__ == "__main__":
    main(sys.argv[1])
