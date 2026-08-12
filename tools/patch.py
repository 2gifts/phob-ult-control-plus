#!/usr/bin/env python3
"""Apply Phob Ult Control Plus to a PhobGCC-SW source tree.

Copies the extra headers into PhobGCC/extras/ and inserts the hook calls into
PhobGCC/extras/extras.h and PhobGCC/common/phobGCC.h.

Every hook is wrapped in an #ifdef, so which mods are active is decided purely
by the #define lines in extras.h. That means one patch covers any combination,
and re-running with a different --mods list just flips those lines.

The patch is idempotent: running it twice does nothing the second time, and
--mods can be changed at any point without starting over.

Usage:
    python patch.py <path-to-PhobGCC-SW> [--mods tapjump,tiltstick,shieldtilt]
    python patch.py <path-to-PhobGCC-SW> --revert
"""

import argparse
import shutil
import sys
from pathlib import Path

MODS = {
    "tapjump":    ("EXTRAS_TAPJUMP",    "tapJump.h",    "tapJump"),
    "tiltstick":  ("EXTRAS_TILTSTICK",  "tiltStick.h",  "tiltStick"),
    "shieldtilt": ("EXTRAS_SHIELDTILT", "shieldTilt.h", "shieldTilt"),
}

MARK = "PHOB_ULT_CONTROL_PLUS"

# --------------------------------------------------------------------------
# Blocks inserted into the upstream files. Each is anchored to a line that
# exists in stock PhobGCC-SW, and each is tagged so --revert can find it.
# --------------------------------------------------------------------------

EXTRAS_DEFINES = f"""//--- {MARK} begin ---
//#define EXTRAS_TAPJUMP
//#define EXTRAS_TILTSTICK
//#define EXTRAS_SHIELDTILT
//--- {MARK} end ---
"""

EXTRAS_INCLUDES = f"""
//--- {MARK} begin ---
#ifdef EXTRAS_TAPJUMP
#include "tapJump.h"
#endif
#ifdef EXTRAS_TILTSTICK
#include "tiltStick.h"
#endif
#ifdef EXTRAS_SHIELDTILT
#include "shieldTilt.h"
#endif
//--- {MARK} end ---
"""

EXTRAS_INIT = f"""
//--- {MARK} begin ---
	// These extras own their own button combos in processButtons, so both
	// function pointers are NULL. Registering still reserves the storage slot.
#ifdef EXTRAS_TAPJUMP
	debug_println("Extra: Tap Jump Off...");
	extrasConfigAssign(tapJump::configSlot, NULL, NULL);
#endif
#ifdef EXTRAS_TILTSTICK
	debug_println("Extra: Tilt Stick...");
	extrasConfigAssign(tiltStick::configSlot, NULL, NULL);
#endif
#ifdef EXTRAS_SHIELDTILT
	debug_println("Extra: Free Shield Tilt...");
	extrasConfigAssign(shieldTilt::configSlot, NULL, NULL);
#endif
//--- {MARK} end ---
"""

# readSticks: must run immediately after btn.Ax/btn.Ay are written, so the
# console can never poll a raw high Y on the loop the stick crosses.
HOOK_READSTICKS_A = f"""
//--- {MARK} begin ---
#ifdef EXTRAS_TAPJUMP
	// Must sit here, right after btn.Ax and btn.Ay are written. The console
	// polls from the other core at any time; deciding later would leave the raw
	// stick exposed for the whole button reading section of processButtons.
	tapJump::hold(btn, controls.extras[tapJump::configSlot].config);
#endif
//--- {MARK} end ---
"""

# readSticks: BEFORE the stick values are written into btn. Both of these edit
# the values in place. Correcting the report afterwards instead would leave a
# window on every loop holding an unfiltered value, and the C-stick's shield
# escapes have no timer at all, so one leaked poll is enough to roll.
HOOK_READSTICKS_PRE = f"""
//--- {MARK} begin ---
#ifdef EXTRAS_SHIELDTILT
	// Shield tilt runs first: it silences the C-stick, which is also what keeps
	// the tilt stick from firing while you are holding a shield.
	shieldTilt::hold(btn, remappedAx, remappedAy, remappedCx, remappedCy,
	                 currentCalStep, controls.extras[shieldTilt::configSlot].config);
#endif
#ifdef EXTRAS_TILTSTICK
	tiltStick::hold(btn, remappedAx, remappedAy, remappedCx, remappedCy,
	                currentCalStep, controls.extras[tiltStick::configSlot].config);
#endif
//--- {MARK} end ---
"""

HOOK_PROCESSBUTTONS = f"""
//--- {MARK} begin ---
#ifdef EXTRAS_TILTSTICK
	// Add the synthesised A press first so everything below sees it.
	tiltStick::injectButtons(tempBtn);
#endif
#ifdef EXTRAS_TAPJUMP
	// Runs before copyButtons, so a button press and the true stick reach the
	// console on the same report.
	tapJump::check(btn, tempBtn.A, tempBtn.B, tempBtn.X, tempBtn.Y);
#endif
//--- {MARK} end ---
"""

HOOK_COMBOS = f"""
//--- {MARK} begin ---
#ifdef EXTRAS_TAPJUMP
		// Tap Jump Off: toggle, and move the Y ceiling.
		// Z+Start+Du is the stock tourney toggle, so Dd/Dl/Dr are used instead.
		}} else if(hardware.Z && hardware.S && hardware.Dd && !hardware.A && !hardware.B
		          && !hardware.X && !hardware.Y) {{
			settingChangeCount++;
			IntOrFloat *tj = controls.extras[tapJump::slot].config;
			const bool nowOn = !tapJump::enabled(tj);
			tj[tapJump::TAPJUMP_ENABLE].intValue = nowOn ? tapJump::TAPJUMP_ON : tapJump::TAPJUMP_OFF;
			setExtrasSettingInt(tapJump::slot, tapJump::TAPJUMP_ENABLE, tj[tapJump::TAPJUMP_ENABLE].intValue);
			freezeSticksToggleIndicator(2000, btn, hardware, nowOn);
		}} else if(hardware.Z && hardware.S && (hardware.Dl || hardware.Dr) && !hardware.A && !hardware.B
		          && !hardware.X && !hardware.Y) {{
			settingChangeCount++;
			IntOrFloat *tj = controls.extras[tapJump::slot].config;
			int c = tapJump::cap(tj) + (hardware.Dr ? 1 : -1);
			c = fmin(tapJump::capMax, fmax(tapJump::capMin, c));
			tj[tapJump::TAPJUMP_CAP].intValue = c;
			setExtrasSettingInt(tapJump::slot, tapJump::TAPJUMP_CAP, c);
			btn.Cx = (uint8_t) _intOrigin;
			btn.Cy = (uint8_t) (_floatOrigin + c);//show the ceiling on the C-stick
			clearButtons(400, btn, hardware);
#endif
#ifdef EXTRAS_SHIELDTILT
		}} else if(hardware.Z && hardware.Dl && !hardware.A && !hardware.B && !hardware.S
		          && !hardware.X && !hardware.Y && !hardware.L && !hardware.R) {{
			settingChangeCount++;
			IntOrFloat *st = controls.extras[shieldTilt::slot].config;
			const bool nowOn = !shieldTilt::enabled(st);
			st[shieldTilt::SHIELDTILT_ENABLE].intValue = nowOn ? shieldTilt::SHIELDTILT_ON : shieldTilt::SHIELDTILT_OFF;
			setExtrasSettingInt(shieldTilt::slot, shieldTilt::SHIELDTILT_ENABLE, st[shieldTilt::SHIELDTILT_ENABLE].intValue);
			freezeSticksToggleIndicator(2000, btn, hardware, nowOn);
#endif
#ifdef EXTRAS_TILTSTICK
		}} else if(hardware.Z && hardware.Dr && !hardware.A && !hardware.B && !hardware.S
		          && !hardware.X && !hardware.Y && !hardware.L && !hardware.R) {{
			settingChangeCount++;
			IntOrFloat *ts = controls.extras[tiltStick::slot].config;
			const bool nowOn = !tiltStick::enabled(ts);
			ts[tiltStick::TILTSTICK_ENABLE].intValue = nowOn ? tiltStick::TILTSTICK_ON : tiltStick::TILTSTICK_OFF;
			setExtrasSettingInt(tiltStick::slot, tiltStick::TILTSTICK_ENABLE, ts[tiltStick::TILTSTICK_ENABLE].intValue);
			freezeSticksToggleIndicator(2000, btn, hardware, nowOn);
#endif
//--- {MARK} end ---
"""

# (anchor text, block, insert_after) - insert_after False means insert before
PATCHES = [
    ("extras/extras.h", "//#define EXTRAS_ESS\n", EXTRAS_DEFINES, True),
    ("extras/extras.h", '#ifdef EXTRAS_ESS\n#include "ess.h"\n#endif\n', EXTRAS_INCLUDES, True),
    ("extras/extras.h", "\textrasConfigAssign(ess::extrasEssConfigSlot, ess::toggle, NULL);\n#endif\n",
     EXTRAS_INIT, True),
    ("common/phobGCC.h", "\tfloat hystVal = 0.3;\n", HOOK_READSTICKS_PRE, False),
    ("common/phobGCC.h",
     "\t\t\tbtn.Ax = (uint8_t) (_floatOrigin + aStickX*100);\n"
     "\t\t\tbtn.Ay = (uint8_t) (_floatOrigin + aStickY*100);\n\t\t}\n\t}\n",
     HOOK_READSTICKS_A, True),
    ("common/phobGCC.h", "\t//Copy temp buttons (including analog triggers) back to btn\n",
     HOOK_PROCESSBUTTONS, False),
    ("common/phobGCC.h", "\t\t} else if(checkAdjustExtra(EXTRAS_UP, btn, false)) { // Toggle Extras\n",
     HOOK_COMBOS, False),
]


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def find_root(path: Path) -> Path:
    """Accept either the repo root or the PhobGCC folder inside it."""
    for candidate in (path / "PhobGCC", path):
        if (candidate / "common" / "phobGCC.h").is_file():
            return candidate
    fail(f"{path} does not look like a PhobGCC-SW checkout "
         f"(no PhobGCC/common/phobGCC.h)")


def backup(f: Path):
    b = f.with_suffix(f.suffix + ".orig")
    if not b.exists():
        shutil.copy2(f, b)


def revert(root: Path):
    for name in ("extras/extras.h", "common/phobGCC.h"):
        f = root / name
        b = f.with_suffix(f.suffix + ".orig")
        if b.exists():
            shutil.copy2(b, f)
            b.unlink()
            print(f"  restored {name}")
    for _, header, _ in MODS.values():
        h = root / "extras" / header
        if h.exists():
            h.unlink()
            print(f"  removed  extras/{header}")
    print("Reverted to stock PhobGCC-SW.")


def set_mods(root: Path, wanted):
    """Comment or uncomment the #define lines to match the requested set."""
    f = root / "extras" / "extras.h"
    text = f.read_text(encoding="utf-8")
    for key, (define, _, _) in MODS.items():
        on = key in wanted
        text = text.replace(f"//#define {define}\n", f"#define {define}\n") if on \
            else text.replace(f"\n#define {define}\n", f"\n//#define {define}\n")
    f.write_text(text, encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description="Patch PhobGCC-SW with Phob Ult Control Plus")
    ap.add_argument("phobgcc", type=Path, help="path to your PhobGCC-SW checkout")
    ap.add_argument("--mods", default="tapjump,tiltstick,shieldtilt",
                    help="comma separated: tapjump, tiltstick, shieldtilt (default: all)")
    ap.add_argument("--revert", action="store_true", help="undo the patch")
    args = ap.parse_args()

    root = find_root(args.phobgcc.expanduser().resolve())

    if args.revert:
        revert(root)
        return

    wanted = [m.strip().lower() for m in args.mods.split(",") if m.strip()]
    bad = [m for m in wanted if m not in MODS]
    if bad:
        fail(f"unknown mod(s): {', '.join(bad)}. Choose from: {', '.join(MODS)}")

    src = Path(__file__).resolve().parent.parent / "src"
    for _, header, _ in MODS.values():
        if not (src / header).is_file():
            fail(f"missing {src / header} - run this from a full checkout of the mod repo")

    print(f"PhobGCC-SW: {root}")
    print(f"Mods:       {', '.join(wanted)}\n")

    # 1. headers - always copy all three; the #defines decide what is compiled
    for _, header, _ in MODS.values():
        shutil.copy2(src / header, root / "extras" / header)
        print(f"  copied   extras/{header}")

    # 2. hooks - insert once, guarded by #ifdef
    already = MARK in (root / "common" / "phobGCC.h").read_text(encoding="utf-8")
    if already:
        print("  hooks    already present, skipping")
    else:
        for name, anchor, block, after in PATCHES:
            f = root / name
            text = f.read_text(encoding="utf-8")
            n = text.count(anchor)
            if n != 1:
                fail(f"anchor found {n} times in {name}, expected exactly 1.\n"
                     f"       Your PhobGCC-SW may be a different version than this "
                     f"mod was built against.\n       Anchor: {anchor[:70]!r}")
            backup(f)
            text = text.replace(anchor, anchor + block if after else block + anchor)
            f.write_text(text, encoding="utf-8")
        print("  hooks    inserted into extras/extras.h and common/phobGCC.h")

    # 3. select which mods are active
    set_mods(root, wanted)
    print(f"  enabled  {', '.join(MODS[m][0] for m in wanted)}\n")
    print("Done. Build PhobGCC-SW as normal - see the repo README.")


if __name__ == "__main__":
    main()
