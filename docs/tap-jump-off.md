# Tap Jump Off

Up on the control stick stops jumping. Everything else you do with up keeps
working.

Source: [`src/tapJump.h`](../src/tapJump.h) · Toggle: **Z + Start + Dpad Down**

---

## The mechanic

Melee jumps from the stick when two things are true on the same frame
([`ftCo_Jump.c`](https://github.com/doldecomp/melee/blob/master/src/melee/ft/chara/ftCommon/ftCo_Jump.c)):

```c
lstick.y >= tap_jump_threshold && x671_timer_lstick_tilt_y < x74
```

While dashing or running it uses a **second, lower** threshold instead
(`fn_800CAF78`, reached from Dash, Run, RunBrake, RunDirect and TurnRun):

```c
lstick.y >= x80 && x671_timer_lstick_tilt_y < x74
```

That second threshold is easy to miss and is why a dash followed by a tap up
jumps so readily.

The timer restarts when Y crosses a **small** threshold upward, then counts
frames (`fighter.c`). A fast push up jumps; a slow one does not.

## What the mod does

After each upward crossing it holds a **ceiling** on the reported Y, below both
jump thresholds, for 100 ms. Melee's timer ages out under the ceiling, so when
the ceiling lifts the stick is free and no jump can fire.

Tap jump is the only up action in the game that needs no button. So **any button
press takes the ceiling away instantly** and hands over your real stick:

| Input | Result |
|---|---|
| Up, no button | Ceiling holds. No jump. |
| Up + A | True stick — up-tilt, up-smash |
| Up + B | True stick once Y is high enough for an up-B to exist |
| Up + X or Y | True stick — a real jump, and jump-cancelled up-smash |

This is safe because **every state that has an up-move tests it before the
jump.** Handing over the true stick can never produce a jump there.

The mod reads only the stick and the buttons. It does not try to work out what
the character is doing.

## Limitations

**Stick magnitude drops for about 100 ms** on a fast push up with no button held.
The *angle* is preserved exactly — X is scaled along with Y — so the direction
you input is the direction the game reads. Only the length is briefly shorter.

For **DI this is the thing to know about.** Melee applies DI at the end of
hitlag, and hitlag is usually longer than the ceiling window, so in practice the
read lands after the ceiling has lifted. Short-hitlag situations are where you
would notice, and a flick you make *while already holding a direction* is not
affected at all, since there is no new crossing.

If you dash and hold the direction, X keeps its true value rather than being
scaled, so a fast up-flick cannot drop you out of a dash.

**The initial dash is the one hole.** `ftCo_Dash_IASA` reaches no up-B at all —
side-B is the only special it can start — so nothing there can answer a B press
ahead of the jump check. In that short window, up plus B can still jump. This is
how Melee is built, and closing it would mean guessing at game state, which
caused worse problems than it solved.

**Control-stick up-smash still works**, through the A exception. Running
up-smash is a jump-cancelled up-smash in Melee (`ftCo_Run_IASA` contains no
up-smash check at all — only `ftCo_KneeBend_IASA` does), so with tap jump off you
jump-cancel with **X or Y** instead of the stick. That path is fully supported.

## Tuning

The ceiling defaults to 28 units, where 100 is full deflection and Melee divides
the report by 80.

It has to sit inside a band. Too high and the running jump threshold goes
straight through it. Too low and Melee marks its timer old, then reads the
ceiling lifting as a fresh push up and jumps anyway.

`x80` lives in `PlCo.dat` rather than in code, so 28 is a reasoned choice from
community-reported values, not a measured one. If a jump ever slips through:

- **Z + Start + Dpad Left** lowers the ceiling, down to 24
- **Z + Start + Dpad Right** raises it, up to 52

The C-stick shows the current value after a change. Lower is safer; higher keeps
more stick magnitude during the window.

The fastest way to find the true edge is to dash and tap up: raise the ceiling
one unit at a time until a jump appears, then drop two.
