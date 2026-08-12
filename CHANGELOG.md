# Changelog

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
