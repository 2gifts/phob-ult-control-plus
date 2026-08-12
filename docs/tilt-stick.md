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

The C-stick's **angle is kept**, not snapped to a cardinal. Melee reads the tilt
from the stick angle, and inside the side-tilt band it uses that same angle to
aim the attack up or down.

So holding the C-stick off a notch, between straight forward and the
up-forward diagonal, gives you an angled forward tilt. Fox's angled f-tilt works
exactly as it does on the control stick.

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
console. It is centred for as long as it is deflected, not only during the
press. That is deliberate: Melee reads a C-stick smash from a *crossing*, so
letting a still-deflected C-stick snap back to its true value would fire the
smash the mod just replaced.

**If A is already held**, the synthesised press has no rising edge, so no move
comes out.

**Smash attacks now need the control stick.** That is the trade, and it is the
same trade Ultimate makes.

## Timing

The A press fires on the same firmware loop as the C-stick flick, so there is no
added delay compared to a vanilla C-stick smash.
