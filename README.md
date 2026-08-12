# Phob Ult Control Plus

![PhobGCC](https://img.shields.io/badge/PhobGCC--SW-b4f175e-1fc28e)
![Board](https://img.shields.io/badge/board-PhobGCC%202.0%20(RP2040)-blue)
![License](https://img.shields.io/badge/license-GPL--3.0-green)

Three optional PhobGCC firmware add-ons that give Melee a few of the control
options Smash Ultimate players are used to. Each one is independent — install
one, two, or all three.

| Mod | What it gives you |
|---|---|
| **[Tap Jump Off](docs/tap-jump-off.md)** | Up on the control stick stops jumping. Up-tilt, up-smash, up-B and jump-cancelled up-smash all still work. |
| **[Tilt Stick](docs/tilt-stick.md)** | The C-stick does tilt attacks on the ground instead of smashes, and still does aerials in the air. Angled forward tilts included. |
| **[Free Shield Tilt](docs/shield-tilt.md)** | Hold both shields and tilt your shield anywhere without rolling, spotdodging or jumping. |

This is **not** a rewrite of PhobGCC. It is a set of files dropped into the
`extras/` folder that upstream already provides for exactly this purpose, plus a
handful of hook calls. Stock behaviour is untouched when the mods are off.

---

## Please read this first

**These mods change the inputs your controller reports.** They are almost
certainly **illegal under current Melee tournament rulesets.** The
[Conch ruleset](https://github.com/CarVac/MeleeConchRuleset), which most events
follow and which is maintained by a PhobGCC developer, prohibits firmware that
alters coordinates or synthesises inputs. The Tilt Stick in particular
synthesises a button press, which is the clearest violation of the rules on
macros.

**Use these for solo play, training and friendlies. Do not take a controller
running these to bracket.** Assume any event will consider it illegal, and check
with a TO if you are ever unsure.

They are not built to give anyone an edge, and they do not. Every one of them
gives up something in exchange (see the limitations in each doc). The goal is to
lower the wall for players coming from Ultimate, where these options are just
settings in a menu.

Melee's inputs are not badly designed — they are *differently* designed, and a
lot of Ultimate players bounce off the game before they get to that. If you want
to learn the game as it is, do that instead; it is better. This is for the people
who would otherwise stop playing.

---

## How each one works, briefly

Everything here comes from the
[Melee decompilation](https://github.com/doldecomp/melee). Each doc cites the
functions so you can check the claims yourself.

The useful thing the decomp shows is that most of Melee's "accidental" inputs
share one shape: a threshold **and** a timer. For example, a tap jump is

```c
lstick.y >= tap_jump_threshold && x671_timer_lstick_tilt_y < x74   // ftCo_Jump.c
```

The timer restarts when the axis crosses a small threshold, then counts frames.
That is why a fast push up jumps and a slow push up does not. Rolls, spotdodges,
dashes and smash attacks all work the same way.

A controller can therefore prevent a specific accident by holding a value
briefly, letting the timer age, and then handing the real stick back — without
knowing anything about the game state. Two of these mods do exactly that. The
third (Tilt Stick) cannot, because Melee has no tilt-from-C-stick to filter
towards, so it translates the input instead.

**None of these mods track game state.** No character, no action, no
grounded-or-airborne guessing. They read the stick and the buttons and nothing
else. That constraint is deliberate: earlier drafts tried to time the dash to
tell Melee's Dash state from its Run state, and every error in that guess turned
into a move that would not come out.

---

## Install

### The easy way: download a build

Grab the `.uf2` for the combination you want from
**[Releases](../../releases)**, hold **Start** while plugging in your PhobGCC
2.0, and copy the file onto the `RPI-RP2` drive that appears.

Builds are produced by
[GitHub Actions](.github/workflows/build.yml) straight from this repo, against a
pinned upstream commit. The workflow log shows every command, so you can see
exactly what went into the file you are flashing.

### Building it yourself

You need the [Pico SDK toolchain](https://github.com/PhobGCC/PhobGCC-SW/blob/main/PhobGCC/rp2040/README.md)
set up first.

```bash
git clone https://github.com/PhobGCC/PhobGCC-SW.git
git clone https://github.com/2gifts/phob-ult-control-plus.git

cd phob-ult-control-plus
python tools/patch.py ../PhobGCC-SW --mods tapjump,tiltstick,shieldtilt
```

Pick any subset you like — `--mods tiltstick` on its own is fine. Then build
PhobGCC-SW normally:

```bash
cd ../PhobGCC-SW/PhobGCC/rp2040
mkdir build && cd build
cmake -G Ninja .. && cmake --build .
```

The `.uf2` lands in that `build` folder.

On Windows you can double-click **[`install.bat`](install.bat)** instead of
running the Python command by hand.

To undo everything and get a stock tree back:

```bash
python tools/patch.py ../PhobGCC-SW --revert
```

The patcher keeps `.orig` backups, refuses to run if upstream has moved and an
anchor no longer matches, and is safe to run twice.

**Teensy boards (PhobGCC 1.x):** the mod files live in shared code and use
nothing RP2040-specific, so the patch applies. You will need to build through
Arduino/Teensyduino yourself, and it is untested there.

---

## Turning them on and off

All three ship **on**. Toggling needs Safe Mode off first (**A+X+Y+Start**),
same as every other PhobGCC setting. Both sticks freeze for two seconds to show
the result: **up-right is on, down-left is off.**

| Combo | Does |
|---|---|
| **Z + Start + Dpad Down** | Tap Jump Off on/off |
| **Z + Start + Dpad Left / Right** | Tap Jump: lower / raise the Y ceiling (see its doc) |
| **Z + Dpad Right** | Tilt Stick on/off |
| **Z + Dpad Left** | Free Shield Tilt on/off |

These slots were picked because nothing upstream uses them. `Z+Start+Dpad Up` is
the stock tournament toggle and is left alone. Settings are saved to EEPROM and
survive unplugging.

---

## Compatibility

Built and tested against PhobGCC-SW commit
[`b4f175e`](https://github.com/PhobGCC/PhobGCC-SW/commit/b4f175eaff2578193a82ad9706f92bf1cd733312)
(2025-11-15) on **PhobGCC 2.0 (RP2040)**.

The patcher checks its anchors and stops with a clear message if upstream has
changed underneath it, rather than producing a broken tree.

Tested by hand on hardware in Melee 1.02 over several sessions. Two constants
that Melee keeps in `PlCo.dat` rather than in code (`x80` and `x74`) are set from
community-reported values, so a couple of thresholds are informed estimates. Each
doc says which ones and how to adjust them.

---

## Licence and credits

**GPL-3.0**, because [PhobGCC-SW](https://github.com/PhobGCC/PhobGCC-SW) is
GPL-3.0 and this is a derivative work. See [LICENSE](LICENSE).

This repo contains **only the added files and a patch script**. It does not
redistribute PhobGCC-SW, and the patcher works against your own checkout of it.
If a release binary is distributed here, the complete corresponding source is
this repo plus the pinned upstream commit named above, which is what the build
workflow uses.

PhobGCC is by Phobos132, FrostSSBM, CarVac, wav, Savestate, bjartskular1,
NiceMitch and others — see
[CONTRIBUTORS](https://github.com/PhobGCC/PhobGCC-SW/blob/main/CONTRIBUTORS.md).
This project is not affiliated with or endorsed by the PhobGCC project. Please
do not take support questions about these mods to the PhobGCC Discord; open an
issue here instead.

Mechanics research comes from the [Melee decompilation](https://github.com/doldecomp/melee).
