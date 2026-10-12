#!/usr/bin/env python3
"""Generates raw/ui/pausedmenu.menu: the stock MW2 pause menu (stock/pausedmenu.menu, dumped from common.ff with
OAT's Unlinker) plus an IW-Pad test menu. Everything lives in this one menu: the dvar iwpad_page picks the page
(0 = stock), so no new menu needs registering. Level sections come from each level's add_start() calls; picking a
level fills the reusable rows' dvars (iwpad_sN label, iwpad_gN command, run with vstr).
Run: python3 gen_menu.py && ../build.sh
"""
import os, re

HERE = os.path.dirname(os.path.abspath(__file__))
ROWS = 18          # reusable section rows (max sections in one level is 17)

LEVELS = [
    ('trainer', 'S.S.D.D.', [('timed_ads', ''), ('bullet_penetration', ''), ('frag', ''), ('pit', ''), ('course', ''), ('end', '')]),
    ('roadkill', 'Team Player', [('riverbank', ''), ('convoy', ''), ('ride', ''), ('ambush', ''), ('ride_later', ''), ('dismount', ''), ('school', ''), ('endfight', 'end'), ('end', '')]),
    ('cliffhanger', 'Cliffhanger', [('cave', 'With soap'), ('climb', 'Icepicks'), ('jump', 'Make the jump'), ('clifftop', 'Learn heartbeat sensor'), ('camp', 'sneak through'), ('c4', 'plant it on the tanks'), ('goto_hanger', 'Go there'), ('hanger', 'Be there'), ('satellite', 'Got DSM'), ('tarmac', 'escape'), ('icepick', 'Watch Soap stab'), ('snowmobile', 'Ride it to the finish')]),
    ('airport', 'No Russian', [('intro', 'assault the security checkpoint'), ('stairs', 'proceed up the escalaters'), ('massacre', 'massacre the remaing civilians'), ('gate', 'walk through the sea of dead bodies'), ('basement', 'move through the basement'), ('tarmac', 'assault the tarmac'), ('escape', 'escape from the airport')]),
    ('favela', 'Takedown', [('street', ''), ('chase', ''), ('favela', ''), ('torture', ''), ('soccer', ''), ('hilltop', ''), ('trailer1', ''), ('trailer2', ''), ('trailer3', ''), ('end', '')]),
    ('invasion', 'Wolverines!', [('humvee', ''), ('yards', ''), ('bmp', ''), ('pizza', ''), ('gas_station', ''), ('crash', ''), ('nates_roof', ''), ('attack_diner', ''), ('defend_diner', ''), ('diner', ''), ('burgertown', ''), ('vip_escort', ''), ('helis', ''), ('convoy', '')]),
    ('favela_escape', "The Hornet's Nest", [('intro', 'map start'), ('street', 'street after radiotower'), ('street_mid', 'street after first chopper reveal'), ('street_vista2', 'street second vista area'), ('market', 'market start'), ('market_evac', 'market evac goes wrong'), ('market_evac_escape', 'friendlies climb up to rooftops'), ('roofrun', 'roofrun start'), ('roofrun_bigjump', 'roofrun player jump'), ('solorun', 'player solo run'), ('solorun_rooftops', 'player solo run rooftops'), ('solorun_chopper', 'player jump to chopper')]),
    ('arcadia', 'Exodus', [('street', ''), ('checkpoint', ''), ('golf', ''), ('crash', '')]),
    ('oilrig', 'The Only Easy Day', [('underwater', ''), ('surface', ''), ('rig', ''), ('deck1', ''), ('heli', 'ambush'), ('deck2', ''), ('deck3', ''), ('escape', '')]),
    ('gulag', 'The Gulag', [('intro', ''), ('approach', ''), ('f15', ''), ('unload', ''), ('control_room', ''), ('armory', ''), ('rappel', ''), ('tunnel', ''), ('bathroom', ''), ('rescue', ''), ('run', ''), ('cafe', ''), ('evac', '')]),
    ('dcburning', 'Of Their Own Accord', [('elevator_bottom', ''), ('elevator_top', ''), ('crows_nest', ''), ('crows_nest_armor', ''), ('barrett', ''), ('to_roof', ''), ('roof', ''), ('heliride2', ''), ('crash', '')]),
    ('contingency', 'Contingency', [('start', ''), ('slide', ''), ('woods', ''), ('midwoods', 'mid woods'), ('ridge', ''), ('base', ''), ('defend_sub', '')]),
    ('dcemp', 'Second Sun', [('intro', 'play end of dc burning'), ('iss', 'see the emp up close and personal'), ('emp', 'see the emp effects from the ground'), ('street', 'find cover off the street'), ('corner', 'watch the plane crash'), ('meetup', 'meetup with some allies'), ('lobby', 'sneak through the lobby'), ('office', 'fight through the office'), ('parking', 'traverse the parking deck'), ('plaza', 'continue through the plaza'), ('tunnels', 'traverse the tunnels')]),
    ('dc_whitehouse', 'Whiskey Hotel', [('tunnels', 'make you way to WH'), ('flare', 'pop the flare')]),
    ('estate', 'Loose Ends', [('intro', ''), ('ambush', ''), ('forest_fight', ''), ('house_approach', ''), ('house_breach', ''), ('house_briefing', ''), ('house_defense', ''), ('escape', 'Escape to LZ'), ('ending', 'Ending Sequence')]),
    ('boneyard', 'The Enemy of My Enemy', [('intro', ''), ('road', ''), ('flyby', ''), ('higround', ''), ('ride', ''), ('ride_end', '')]),
    ('af_caves', 'Just Like Old Times', [('road', ''), ('rappel', ''), ('barracks', 'barracks (cave entrance)'), ('steamroom', ''), ('ledge', ''), ('overlook', ''), ('control', ''), ('airstrip', '')]),
    ('af_chase', 'Endgame: boat chase', [x for x in [('boatdrive_begin', ''), ('boatdrive_lake', ''), ('boatdrive_lake_mid', ''), ('boatdrive_rapids', ''), ('boatdrive_below_rapids', ''), ('boatdrive_end', ''), ('wakeup', ''), ('turnbuckle', ''), ('gloat', ''), ('gun_drop', ''), ('crawl', ''), ('gun_kick', ''), ('wounded', 'Watch Price/Shep fight'), ('pullout', ''), ('kill', ''), ('price_wakeup', ''), ('walkoff', '')] if x[0].startswith('boatdrive')]),
    ('ending', 'Endgame: knife fight', [('wakeup', ''), ('turnbuckle', ''), ('gloat', ''), ('gun_drop', ''), ('crawl', ''), ('gun_kick', ''), ('wounded', 'Watch Price/Shep fight'), ('pullout', ''), ('kill', ''), ('price_wakeup', ''), ('walkoff', '')]),
]

# Places where the controls differ from normal play: (label, map, start)
SPOTS = [
    ("Cliffhanger: ice pick climb (L2/R2)", "cliffhanger", "climb"),
    ("Cliffhanger: the big jump", "cliffhanger", "jump"),
    ("Cliffhanger: heartbeat sensor", "cliffhanger", "clifftop"),
    ("Cliffhanger: snowmobile", "cliffhanger", "snowmobile"),
    ("Team Player: Humvee turret", "roadkill", "ride"),
    ("Exodus: laser designator", "arcadia", "golf"),
    ("Contingency: Predator missile", "contingency", "ridge"),
    ("Of Their Own Accord: Barrett", "dcburning", "barrett"),
    ("Of Their Own Accord: heli minigun", "dcburning", "heliride2"),
    ("Whiskey Hotel: flare", "dc_whitehouse", "flare"),
    ("The Gulag: rappel", "gulag", "rappel"),
    ("Loose Ends: breach", "estate", "house_breach"),
    ("Enemy of My Enemy: vehicle ride", "boneyard", "ride"),
    ("Just Like Old Times: rappel", "af_caves", "rappel"),
    ("Endgame: boat chase", "af_chase", "boatdrive_begin"),
    ("Endgame: knife fight crawl (L2/R2)", "ending", "crawl"),
    ("S.S.D.D.: the Pit", "trainer", "pit"),
]

CHEATS = [
    ("God mode on/off", 'exec "god";'),
    ("Noclip on/off", 'exec "noclip";'),
    ("Notarget on/off", 'exec "notarget";'),
    ("Infinite ammo on/off", 'exec "toggle player_sustainAmmo 0 1";'),
    ("Slow motion on/off", 'exec "toggle timescale 1 0.25";'),
    ("Restart section", 'close pausedmenu; exec "fast_restart";'),
]

SHOW_STOCK = "showMenu objectiveinfo; showMenu overheadmap;"
HIDE_STOCK = "hideMenu objectiveinfo; hideMenu overheadmap;"


def q(s):
    return s.replace('"', "'")


def go(m, start):
    return 'close pausedmenu; exec "set start %s; devmap %s";' % (start or "none", m)


def page(n):
    return "setdvar iwpad_page %d; %s" % (n, SHOW_STOCK if n == 0 else HIDE_STOCK)


def item(name, row, visible, action, text=None, exptext=None, x=-330, w=466):
    t = 'text                        "%s"' % q(text) if text is not None else 'exp text                    %s;' % exptext
    return f"""        itemDef
        {{
            name                        "{name}"
            {t}
            group                       "paused_buttons"
            rect                        {x} {48 + row * 20} {w} 20 10 8
            style                       1
            type                        1
            visible                     when({visible});
            textalign                   4
            textalignx                  40
            textscale                   0.375
            textfont                    3
            disablecolor                0.6 0.55 0.55 1
            background                  "popup_button_selection_bar"
            onFocus
            {{
                play mouse_over;
                setItemColor self backcolor 0 0 0 1;
                setLocalVarBool ui_menuAButton 1;
            }}
            leaveFocus
            {{
                setItemColor self backcolor 0 0 0 "0.0";
                setLocalVarBool ui_menuAButton 0;
            }}
            action
            {{
                play mouse_click;
                {action}
            }}
        }}
"""


def title(text, visible):
    return f"""        itemDef
        {{
            text                        "{text}"
            rect                        -330 13 466 40 10 8
            decoration
            visible                     when({visible});
            textalign                   4
            textscale                   0.5
            textfont                    9
        }}
"""


def main():
    stock = open(os.path.join(HERE, "stock", "pausedmenu.menu")).read()
    on_page = lambda n: "dvarint(iwpad_page) == %d" % n
    # stock buttons only on page 0; the pause menu always opens on page 0
    stock = re.sub(r"visible(\s+)when\((.*)\);", lambda m: "visible%swhen((%s) && %s);" % (m.group(1), m.group(2), on_page(0)), stock)
    stock = stock.replace("onOpen\n        {\n", "onOpen\n        {\n            setdvar iwpad_page 0;\n", 1)
    assert "setdvar iwpad_page 0" in stock
    body, tail = stock.rstrip()[:-len("    }\n}")].rstrip() + "\n", "    }\n}\n"

    out = [item("iwpad_open", 7, on_page(0), page(1), text="IW-Pad test menu", x=-200, w=336)]
    # page 1: main
    out.append(title("IW-PAD TEST", on_page(1)))
    rows = [("Controller test spots", page(2)), ("Level select", page(3))] + CHEATS + [("Back", page(0))]
    out += [item("iwpad_main_%d" % i, i, on_page(1), a, text=t) for i, (t, a) in enumerate(rows)]
    # page 2: controller spots
    out.append(title("CONTROLLER TEST SPOTS", on_page(2)))
    out += [item("iwpad_spot_%d" % i, i, on_page(2), go(m, s), text=t) for i, (t, m, s) in enumerate(SPOTS)]
    out.append(item("iwpad_spot_back", len(SPOTS), on_page(2), page(1), text="Back"))
    # page 3: levels; picking one fills the section rows and opens page 4
    out.append(title("LEVEL SELECT", on_page(3)))
    for i, (m, name, starts) in enumerate(LEVELS):
        fill = ['setdvar iwpad_level "%s";' % q(name), 'setdvar iwpad_gstart "set start none; devmap %s";' % m]
        for r in range(ROWS):
            if r < len(starts):
                s, label = starts[r]
                fill.append('setdvar iwpad_s%d "%s";' % (r, q(s + (" - " + label if label else ""))))
                fill.append('setdvar iwpad_g%d "set start %s; devmap %s";' % (r, s, m))
            else:
                fill.append('setdvar iwpad_s%d "";' % r)
        out.append(item("iwpad_level_%d" % i, i, on_page(3), " ".join(fill) + " setdvar iwpad_page 4;", text=name))
    out.append(item("iwpad_level_back", len(LEVELS), on_page(3), page(1), text="Back"))
    # page 4: sections of the chosen level
    out.append(title("SECTIONS", on_page(4)))
    out.append(item("iwpad_sec_start", 0, on_page(4), 'close pausedmenu; exec "vstr iwpad_gstart";', exptext='"Start of level: " + dvarstring(iwpad_level)'))
    for r in range(ROWS):
        out.append(item("iwpad_sec_%d" % r, r + 1, on_page(4) + ' && dvarstring(iwpad_s%d) != ""' % r,
                        'close pausedmenu; exec "vstr iwpad_g%d";' % r, exptext="dvarstring(iwpad_s%d)" % r))
    out.append(item("iwpad_sec_back", ROWS + 1, on_page(4), "setdvar iwpad_page 3;", text="Back to levels"))

    os.makedirs(os.path.join(HERE, "raw", "ui"), exist_ok=True)
    open(os.path.join(HERE, "raw", "ui", "pausedmenu.menu"), "w").write(body + "".join(out) + tail)
    print(sum(x.count("itemDef") for x in out) + stock.count("itemDef"), "items")


if __name__ == "__main__":
    main()
