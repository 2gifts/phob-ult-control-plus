# Free Shield Tilt: tilt your shield in Melee without rolling

*Part of [Phob Ult Control Plus](../README.md), a PhobGCC GameCube controller mod.
Hold both triggers, like holding both shield buttons in Ultimate.*

Hold **both** shield triggers and you can move the stick anywhere without
rolling, spotdodging or jumping. Same idea as holding both shield buttons in
Ultimate.

Source: [`src/shieldTilt.h`](../src/shieldTilt.h) · Toggle: **Z + Dpad Left**

---

## The mechanic

All three ways out of a shield need a **fast** stick input, not just a far one
([`ftCo_Escape.c`](https://github.com/doldecomp/melee/blob/master/src/melee/ft/chara/ftCommon/ftCo_Escape.c),
`ftCo_Jump.c`):

```c
roll       abs(lstick.x) >= x31C && x670_timer_lstick_tilt_x < x320
spotdodge  lstick.y <= x314      && x671_timer_lstick_tilt_y < x318
jump       lstick.y >= tap_jump  && x671_timer_lstick_tilt_y < x74
```

Each timer restarts when that axis crosses a small threshold, then counts frames.

## What the mod does

While both shields are held, each axis is **clamped immediately** to a value
below every escape line. Your shield tilts the moment the stick moves. There is
no delay.

Once an axis has been held past the small threshold for 200 ms, its clamp
**lifts entirely** and the true stick goes through. By then Melee's timer has
aged out, and lifting a clamp is not a crossing, so nothing can fire.

So: instant response up to the clamp, and full deflection if you keep holding.

The first version of this mod rate-limited the stick instead. It was safe on
paper and felt awful, because every shield tilt lagged your hand. Clamp-then-release
gives the same safety with none of the lag.

## The C-stick

It is centred while the lock is on.

The C-stick's shield escapes have **no timer at all** and fire on position alone
(`ftCo_800DF8B0`, `ftCo_800DF8E8`, `ftCo_800DF910` in `ft_0DF1.c`), so neither a
clamp nor a delay can make them safe. The C-stick does not tilt the shield in
Melee, so nothing is lost. You only give up C-stick rolls and C-stick jump out
of shield, which is what you asked for by holding both triggers.

## Light shield

Both triggers count as held from either the digital press **or** a firm analog
press. Light shielding never reaches the digital point, and the lock has to work
there too.

## Limitations

**Both triggers means both triggers.** With one trigger held, everything behaves
exactly as stock. That is the point. The lock is opt-in per shield, and normal
shielding, wavedashing and light shielding are untouched.

**You cannot roll or spotdodge while both are held.** Release one to get them
back.

**The clamp values are estimates.** `x31C` and `x314` live in `PlCo.dat` rather
than in code, so the clamps (50 on X, 42 on Y) are set from community-reported
thresholds with margin. If an escape ever slips out, they can be lowered in
[`src/shieldTilt.h`](../src/shieldTilt.h). The constants are named and
commented at the top of the file.
