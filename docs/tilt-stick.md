# Tilt Stick: C-stick tilt attacks in Super Smash Bros. Melee

*Part of [Phob Ult Control Plus](../README.md), a PhobGCC GameCube controller mod.
Ultimate's "C-stick = Attack" option, for a game that never had one.*

The C-stick does tilt attacks on the ground instead of smash attacks, and still
does aerials in the air. Same as Ultimate's "Attack" C-stick setting.

Source: [`src/tiltStick.h`](../src/tiltStick.h) · Toggle: **Z + Dpad Right**

---

## Why this one is different

The other two mods work by filtering. This one cannot, because there is nothing
to filter towards: **Melee has no tilt-from-C-stick.** On the ground the C-stick
always smashes, and every tilt reads the **left** stick together with A
([`ftCo_AttackS3.c`](https://github.com/doldecomp/melee/blob/master/src/melee/ft/chara/ftCommon/ftCo_AttackS3.c),
`ftCo_AttackHi3.c`, `ftCo_AttackLw3.c`):

```c
side tilt  A && lstick.x * facing >= x98 && abs(angle) <  x20_radians
up tilt    A && lstick.y >= attackhi3_stick_threshold_y && angle >  x20_radians
down tilt  A && lstick.y <= xB0                         && angle < -x20_radians
```

So the input has to be **translated**: the mod reports the left stick at a
tilt-sized value in the C-stick's direction and synthesises an A press for two
frames.

**This is a synthesised button press.** If you only want mods that filter your
own inputs, use the other two and leave this one off.

Aerials come out correct for free, because in the air A plus a direction already
selects the aerial. No special case is needed.

## Angled tilts

Flick the C-stick into one of the **four corners** and you get an angled forward
tilt. No holding a precise spot on the rim: the corner is the input.

The flick is snapped to one of eight directions, and the corners are reported to
Melee as a **shallow** angle rather than the 45 degrees you actually flicked.
That is the whole trick. Melee reads the tilt from the stick angle, and a raw 45
sits outside its side-tilt band, so it would come out as an up or down tilt
instead.

**The corners get a wider slice than the cardinals**, because a cardinal is easy
to find on the gate and a corner is not. Anything more than 20 degrees off a
cardinal counts as a corner, which leaves the cardinals 40 degrees wide each and
the corners 50.

The reported length is 45 units, under Melee's smash lines of 64 on X and 53 on
Y, so no timing can turn it into a smash, and clear of every tilt floor.

## Limitations

**A backward side tilt on the ground is not a Melee move.** `AttackS3` is facing
relative, and a controller cannot know which way you are facing. A backward
flick and a forward flick are the same input to the firmware. So a backward
C-stick flick on the ground gives a jab.

Turning around first would mean holding the stick back for a couple of frames
before the A press, and since the firmware cannot tell backward from forward,
that delay would land on **every** side tilt. That was judged not worth it.
Aerials are unaffected and work in all four directions.

**C-stick DI and SDI are gone while this is on.** The C-stick never reaches the
console at all: not while held, not while returning to centre, not partially.

That is deliberate and it is stricter than it first looks. Melee reads a C-stick
smash from a *crossing*, so any value getting through risks a smash, and even a
value too small to smash still moves the camera and the character select cursor.
There is no safe amount to let through, so none is let through. Anything else
that wants a live C-stick is off too, including PhobGCC's own two-stick extras
combos.

One flick gives one tilt. Nothing more happens until the C-stick returns near
centre **and stays there for 40 ms**, which re-arms it.

That short wait is a snapback filter. Releasing a stick overshoots past centre
and rings for a few milliseconds, and PhobGCC's Kalman snapback filter only runs
on the left stick. Without the wait, letting go of a down input rings up past the
trigger and fires an up tilt on the way back. Melee's own tilt endlag is far
longer than 40 ms, so nothing real is lost.

**If A is already held**, the synthesised press has no rising edge, so no move
comes out.

**Smash attacks now need the control stick.** That is the trade, and it is the
same trade Ultimate makes.

## Timing

The A press fires on the same firmware loop as the C-stick flick, so there is no
added delay compared to a vanilla C-stick smash.

The mod edits the stick values **before** PhobGCC writes them into the report,
rather than correcting the report afterwards. That is not a stylistic choice. The
console polls from its own core at any moment, so correcting afterwards leaves a
short window on every single loop holding a live C-stick value. At a 1 kHz loop
against a 60 Hz poll, one of those windows gets caught every few seconds of
holding the C-stick, and the result is a smash attack out of nowhere. Writing the
corrected value in the first place removes the window.
