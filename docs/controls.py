# Draws the README controller diagrams: docs/controls-<layout>.svg, one per default pad layout.
# usage: python3 docs/controls.py   (standard library only; edit LAYOUTS when a default bind changes)
import os

LAYOUTS = {
    "cod1-uo": ("Call of Duty 1 and United Offensive", {
        "LT": "Aim down sights", "LB": "Previous weapon",
        "LS": "Move", "L3": "Sprint (United Offensive)",
        "DPAD": ["Pistol", "Alternate weapon mode", "Lean left", "Lean right"],
        "BACK": "Scores", "START": "Pause menu",
        "RT": "Fire", "RB": "Frag grenade",
        "FACE": ["Next weapon", "Crouch (tap), prone (hold)", "Jump", "Reload (tap), use (hold)"],
        "RS": "Look", "R3": "Melee"}),
    "cod2": ("Call of Duty 2", {
        "LT": "Aim down sights", "LB": "Smoke grenade",
        "LS": "Move", "L3": "Hold breath",
        "DPAD": ["Binoculars", "Prone", "Lean left", "Lean right"],
        "BACK": "Scores", "START": "Pause menu",
        "RT": "Fire", "RB": "Frag grenade",
        "FACE": ["Next weapon", "Crouch (tap), prone (hold)", "Jump", "Reload (tap), use (hold)"],
        "RS": "Look", "R3": "Melee"}),
    "mw": ("Modern Warfare 2 and Modern Warfare 3", {
        "LT": "Aim down sights", "LB": "Special grenade",
        "LS": "Move", "L3": "Sprint, hold breath",
        "DPAD": ["Action slot 1", "Action slot 2", "Action slot 3", "Action slot 4"],
        "BACK": "Objectives", "START": "Pause menu",
        "RT": "Fire", "RB": "Frag grenade",
        "FACE": ["Next weapon", "Crouch (tap), prone (hold)", "Jump", "Reload (tap), use (hold)"],
        "RS": "Look", "R3": "Melee"}),
}

W, H = 1280, 600
INK, MUTED, LINE, CARD = "#1f2328", "#57606a", "#8c959f", "#f3f4f6"
BADGE = "#2b2d31"
FACE = [("Y", "#f2c230", 419, 196), ("B", "#e5483d", 451, 230), ("A", "#5cbf4a", 420, 260), ("X", "#3b8fe6", 388, 227)]
S, TX, TY = 1.25, 265, -45  # controller drawn in a 600-wide space, then scaled onto the card


def P(x, y):
    return (round(TX + S * x, 1), round(TY + S * y, 1))


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def mirror_path(left, close=True):
    """left: start point + cubic segments for the left half, top centre -> bottom centre. Returns the full outline."""
    (sx, sy), segs = left
    d = [f"M{sx} {sy}"] + [f"C{a} {b} {c} {e} {f} {g}" for (a, b), (c, e), (f, g) in segs]
    pts = [(sx, sy)] + [s[2] for s in segs]
    for i in range(len(segs) - 1, -1, -1):  # walk back up the right side
        (a, b), (c, e), _ = segs[i]
        f, g = pts[i]
        d.append(f"C{600 - c} {e} {600 - a} {b} {600 - f} {g}")
    return " ".join(d) + (" Z" if close else "")


# All positions below are measured from a 600x600 front photo of the controller (silhouette, button centres, circle fits).
BODY = ((300, 147), [
    ((255, 147), (228, 146), (205, 138)),
    ((178, 137), (150, 150), (132, 180)),
    ((118, 207), (95, 265), (80, 320)),
    ((70, 355), (66, 395), (72, 425)),
    ((78, 450), (95, 462), (112, 460)),
    ((140, 445), (165, 410), (190, 383)),
    ((205, 372), (225, 370), (250, 370)),
    ((270, 370), (290, 370), (300, 370)),
])


def controller():
    s = ['<defs>',
         '<linearGradient id="body" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3b3d42"/>'
         '<stop offset="0.55" stop-color="#2a2c30"/><stop offset="1" stop-color="#1c1d20"/></linearGradient>',
         '<radialGradient id="sheen" cx="0.5" cy="0.15" r="0.6"><stop offset="0" stop-color="#ffffff" stop-opacity="0.10"/>'
         '<stop offset="1" stop-color="#ffffff" stop-opacity="0"/></radialGradient>',
         '<radialGradient id="cap" cx="0.42" cy="0.35" r="0.75"><stop offset="0" stop-color="#4a4d52"/>'
         '<stop offset="1" stop-color="#232529"/></radialGradient>',
         '<radialGradient id="btn" cx="0.4" cy="0.3" r="0.8"><stop offset="0" stop-color="#2e3034"/>'
         '<stop offset="1" stop-color="#121315"/></radialGradient>',
         '<linearGradient id="cross" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#43464b"/>'
         '<stop offset="1" stop-color="#26282b"/></linearGradient>',
         '<filter id="shadow" x="-20%" y="-20%" width="140%" height="160%"><feGaussianBlur stdDeviation="9"/></filter>',
         '</defs>',
         f'<g transform="translate({TX} {TY}) scale({S})">',
         '<ellipse cx="300" cy="458" rx="215" ry="16" fill="#000" opacity="0.22" filter="url(#shadow)"/>']
    body = mirror_path(BODY)
    s.append(f'<path d="{body}" fill="url(#body)" stroke="#121315" stroke-width="2"/>')
    s.append(f'<path d="{body}" fill="url(#sheen)"/>')
    for cx, cy in ((180, 227), (361, 298)):  # thumbsticks: well, cap, dished top
        s.append(f'<circle cx="{cx}" cy="{cy}" r="35" fill="#141517"/>')
        s.append(f'<circle cx="{cx}" cy="{cy}" r="28" fill="url(#cap)"/>')
        s.append(f'<circle cx="{cx}" cy="{cy}" r="20" fill="#2a2c30" stroke="#3c3f44" stroke-width="1.2"/>')
        s.append(f'<path d="M{cx - 14} {cy - 14} A20 20 0 0 1 {cx + 14} {cy - 14}" fill="none" stroke="#ffffff" stroke-opacity="0.10" stroke-width="2"/>')
    s.append('<circle cx="238" cy="303" r="35" fill="#141517"/>')  # d-pad in its well
    s.append('<path d="M229 273 h18 q4 0 4 4 v17 h17 q4 0 4 4 v18 q0 4 -4 4 h-17 v17 q0 4 -4 4 h-18 q-4 0 -4 -4 v-17 h-17 '
             'q-4 0 -4 -4 v-18 q0 -4 4 -4 h17 v-17 q0 -4 4 -4 Z" fill="url(#cross)" stroke="#1b1c1f" stroke-width="1"/>')
    s.append('<circle cx="238" cy="303" r="6" fill="#222427"/>')
    s.append('<circle cx="300" cy="180" r="17" fill="#26282b" stroke="#141517" stroke-width="2"/>')  # guide, plain
    s.append('<circle cx="300" cy="180" r="11" fill="#3a3d42"/>')
    for cx in (266, 334):
        s.append(f'<circle cx="{cx}" cy="226" r="8.5" fill="url(#btn)" stroke="#0e0f10" stroke-width="1"/>')
    s.append('<rect x="261.5" y="222.5" width="6" height="5" rx="1" fill="none" stroke="#9aa0a6" stroke-width="0.9"/>'
             '<rect x="264.5" y="224.5" width="6" height="5" rx="1" fill="none" stroke="#9aa0a6" stroke-width="0.9"/>')  # view
    s.append('<path d="M330 223 h8 M330 226 h8 M330 229 h8" stroke="#9aa0a6" stroke-width="1"/>')  # menu
    s.append('<rect x="292" y="243.5" width="16" height="11" rx="4" fill="url(#btn)" stroke="#0e0f10" stroke-width="1"/>'
             '<path d="M300 252 v-5 M297.5 249 l2.5 -2.5 l2.5 2.5" fill="none" stroke="#9aa0a6" stroke-width="0.9"/>')  # share
    for letter, color, cx, cy in FACE:
        s.append(f'<circle cx="{cx}" cy="{cy}" r="15.5" fill="url(#btn)" stroke="#0b0c0d" stroke-width="1"/>')
        s.append(f'<ellipse cx="{cx}" cy="{cy - 7}" rx="9" ry="4.5" fill="#ffffff" opacity="0.08"/>')
        s.append(f'<text x="{cx}" y="{cy + 5.5}" text-anchor="middle" font-size="15.5" font-weight="700" fill="{color}">{letter}</text>')
    s.append('</g>')
    return s


def badge(x, y, text, side, color=None):
    """a button-name pill at text baseline y; side 'left' puts it ending at x, 'right' starting at x. Returns (svg, far x)"""
    if color:  # face buttons: a coloured round button
        w = 24
        x0 = x - w if side == "left" else x
        return [f'<circle cx="{x0 + 12}" cy="{y - 5}" r="11.5" fill="{BADGE}"/>',
                f'<text x="{x0 + 12}" y="{y}" text-anchor="middle" font-size="13" font-weight="700" fill="{color}">{text}</text>'], \
            (x0 if side == "left" else x0 + w)
    w = 8.2 * len(text) + 16
    x0 = x - w if side == "left" else x
    return [f'<rect x="{x0}" y="{y - 17}" width="{w}" height="23" rx="11.5" fill="{BADGE}"/>',
            f'<text x="{x0 + w / 2}" y="{y - 1}" text-anchor="middle" font-size="12.5" font-weight="700" fill="#ffffff">{esc(text)}</text>'], \
        (x0 if side == "left" else x0 + w)


def callout(rows, x, y, anchor, side, step=30, down=False):
    """rows of (badge text, label[, colour]) stacked from baseline y; badges line up at x; one leader to the anchor"""
    s = []
    for i, row in enumerate(rows):
        b, label = row[0], row[1]
        yy = y + i * step
        parts, far = badge(x, yy, b, side, row[2] if len(row) > 2 else None)
        s += parts
        tx, ta = (far - 9, "end") if side == "left" else (far + 9, "start")
        s.append(f'<text x="{tx}" y="{yy}" text-anchor="{ta}" fill="{INK}">{esc(label)}</text>')
    if down:  # straight down from the (single) badge
        bx = (x + far) / 2
        s.insert(0, f'<path d="M{bx} {y + 6} L{anchor[0]} {anchor[1]}" stroke="{LINE}" stroke-width="1.4" fill="none"/>')
    else:
        mid = y + (len(rows) - 1) * step / 2 - 5
        x1 = x + 8 if side == "left" else x - 8
        x2 = x1 + 18 if side == "left" else x1 - 18
        s.insert(0, f'<path d="M{x1} {mid} H{x2} L{anchor[0]} {anchor[1]}" stroke="{LINE}" stroke-width="1.4" fill="none"/>')
    s.append(f'<circle cx="{anchor[0]}" cy="{anchor[1]}" r="4.5" fill="#ffffff" stroke="#6e7781" stroke-width="1.5"/>')
    return s


def diagram(title, b):
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
         f'font-family="Segoe UI, -apple-system, Helvetica, Arial, sans-serif" font-size="16">',
         f'<title>{esc(title)} controls</title>',
         f'<rect width="{W}" height="{H}" rx="18" fill="{CARD}"/>',
         f'<text x="36" y="50" font-size="22" font-weight="700" fill="{INK}">{esc(title)}</text>']
    s += controller()
    L, R = 330, 950
    s += callout([("LT", b["LT"])], L, 96, P(175, 140), "left")
    s += callout([("LB", b["LB"])], L, 160, P(134, 177), "left")
    s += callout([("LS", b["LS"]), ("L3", b["L3"])], L, 238, P(150, 227), "left")
    s += callout([(a, t) for a, t in zip("↑↓←→", b["DPAD"])], L, 338, P(203, 303), "left")
    s += callout([("Back", b["BACK"])], 628, 82, P(266, 217), "left", down=True)
    s += callout([("Start", b["START"])], 652, 82, P(334, 217), "right", down=True)
    s += callout([("RT", b["RT"])], R, 96, P(425, 140), "right")
    s += callout([("RB", b["RB"])], R, 160, P(466, 177), "right")
    s += callout([(l, t, c) for (l, c, _, _), t in zip(FACE, b["FACE"])], R, 220, P(466, 230), "right", step=29)
    s += callout([("RS", b["RS"]), ("R3", b["R3"])], R, 356, P(391, 298), "right")
    s.append("</svg>\n")
    return "\n".join(s)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    for name, (title, binds) in LAYOUTS.items():
        with open(os.path.join(here, f"controls-{name}.svg"), "w", encoding="utf-8") as f:
            f.write(diagram(title, binds))
        print("wrote", f"docs/controls-{name}.svg")
