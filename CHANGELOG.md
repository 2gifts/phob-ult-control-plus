# Changelog

## 1.0.1

**Tilt Stick: angled tilts are much easier to hit.**

A flick is now snapped to one of eight directions instead of passing the raw
C-stick angle through. The four corners give the angled forward tilt, and they
get a wider slice than the cardinals, 50 degrees against 40, because a cardinal
is easy to find on the gate and a corner is not.

A corner reports a clean 45 degrees, which is what Melee's `decideAngle` needs to
pick an angled variant rather than the straight one.

No change to Tap Jump Off or Free Shield Tilt. Their `.uf2` files are identical
to 1.0.0.

## 1.0.0

First release. Three independent PhobGCC extras for Melee:

- **Tap Jump Off** — holds a ceiling on Y after an upward crossing so Melee's
  tilt timer ages out, and lifts it instantly for any button press. Covers both
  of Melee's jump thresholds, including the lower one used by the dash and run
  states.
- **Tilt Stick** — translates a C-stick flick into a left-stick tilt value plus
  a synthesised A press. Keeps the C-stick angle, so angled forward tilts work.
  Aerials are unchanged.
- **Free Shield Tilt** — while both shields are held, clamps each axis below
  Melee's roll, spotdodge and jump lines, then releases the clamp once its timer
  has aged. Centres the C-stick, whose shield escapes have no timer.

Built and tested against PhobGCC-SW `b4f175e` on PhobGCC 2.0 (RP2040).
