# Builds zz_iwpad.pk3 (CoD1 / UO button icons) from glyphs.bin: one 32-bit TGA per button, iwpad_<set>_<button>.tga
# at the root (stock TGA layout: bottom-up, power-of-two), loaded by the DLL with RegisterShader("iwpad_<set>_<button>") (implicit 2D shader, alpha blended).
# Goes in uo/ as zz_iwpad.pk3 (UO; zz_ sorts after the stock paks) or main/ as z_iwpad.pk3 (CoD1: its pak scan 0x424aa0
# turns zz_ / sp_ names into "s" and fails to open them, a bug; mp_ is skipped. z_ still sorts after pak6).
# Also packs the given menu folder (menus replacing stock ones, e.g. uo/ for UO's Look screen controller rows).
# usage: python3 glyphs/q3_pk3.py out.pk3 [menu folder]   (stdlib only)
import os, struct, sys, zipfile

here = os.path.dirname(os.path.abspath(__file__))
SETS, BUTTONS = ("xbox", "ps3"), 16  # glyphs.bin per set: the 16 buttons (A..START), then the d-pad (unused here)


def pow2(v):
    p = 1
    while p < v: p *= 2
    return p


def tga(tw, th, bgra):  # like the stock ones: uncompressed, bottom-up rows, power-of-two sides (transparent padding;
    pw, ph = pow2(tw), pow2(th)  # the DLL draws only the tw x th top-left corner)
    px = bytearray(pw * ph * 4)
    for y in range(th):  # image row y (from the top) is file row ph - 1 - y
        o = (ph - 1 - y) * pw * 4
        px[o:o + tw * 4] = bgra[y * tw * 4:(y + 1) * tw * 4]
    return struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, pw, ph, 32, 0x08) + bytes(px)


def main(out, menus=None):
    data = open(os.path.join(here, "glyphs.bin"), "rb").read()
    pos = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for s in SETS:
            for i in range(BUTTONS + 1):
                w, h, tw, th = data[pos:pos + 4]
                bgra = data[pos + 4:pos + 4 + tw * th * 4]
                pos += 4 + tw * th * 4
                if i < BUTTONS: z.writestr(f"iwpad_{s}_{i}.tga", tga(tw, th, bgra))
        for root, _, files in os.walk(menus) if menus else ():
            for f in files:
                path = os.path.join(root, f)
                z.write(path, os.path.relpath(path, menus).replace(os.sep, "/"))
    assert pos == len(data), (pos, len(data))
    print(out, os.path.getsize(out), "bytes")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
