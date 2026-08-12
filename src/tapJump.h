/* Tap Jump Off - a PhobGCC extra for Super Smash Bros. Melee
 *
 * Copyright (C) 2026 2gifts
 *
 * This file is part of Phob Ult Control Plus, an add-on for the PhobGCC
 * firmware. It is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See the LICENSE file at the root of this repository.
 */

#ifndef EXTRAS_TAPJUMP_H
#define EXTRAS_TAPJUMP_H

#include "../common/structsAndEnums.h"

namespace tapJump {
	/* ------------------------------------------------------------------------
	 * How Melee decides to jump from the stick
	 * ------------------------------------------------------------------------
	 * ftCo_Jump.c, ftCo_Jump_GetInput:
	 *
	 *     lstick.y >= tap_jump_threshold   &&   tilt timer < x74
	 *
	 * While dashing or running, Melee uses a second and LOWER threshold instead.
	 * fn_800CAF78, reached from Dash, Run, RunBrake, RunDirect and TurnRun:
	 *
	 *     lstick.y >= x80                  &&   tilt timer < x74
	 *
	 * The tilt timer restarts when Y crosses a small threshold upward, then
	 * counts frames (fighter.c). That is why a fast push up jumps and a slow push
	 * up does not.
	 *
	 * ------------------------------------------------------------------------
	 * What this extra does
	 * ------------------------------------------------------------------------
	 * After each upward crossing it holds a ceiling on the reported Y, below both
	 * jump thresholds, for one window. Melee's timer ages out under the ceiling,
	 * so when the ceiling lifts the stick is free and no jump can fire.
	 *
	 * Tap jump is the only up action in the game that needs no button, so ANY
	 * button press takes the ceiling away at once and hands over the true stick:
	 *
	 *     no button  ->  ceiling holds, timer ages out, no jump
	 *     A, X or Y  ->  true stick
	 *     B          ->  true stick, once Y is high enough for an up-B to exist
	 *
	 * Every state that has an up-move tests it BEFORE the jump, so handing over
	 * the true stick can never produce a jump there. Up-tilt, up-smash, up-B and
	 * jump-cancelled up-smash all keep working.
	 *
	 * The rule uses only the reported stick and the buttons. It deliberately does
	 * not try to work out which state the character is in. Earlier versions timed
	 * the dash to tell Dash from Run, and every error in that guess turned into a
	 * move that would not come out.
	 *
	 * ------------------------------------------------------------------------
	 * Known limits
	 * ------------------------------------------------------------------------
	 * The initial dash. ftCo_Dash_IASA reaches no up-B at all, so nothing there
	 * can answer a B press ahead of the jump check. In that short window, up plus
	 * B can still jump. This is a property of the game.
	 *
	 * Stick magnitude drops while the ceiling is on. The angle is kept, and only
	 * a fast push up with no button is affected. See docs/tap-jump-off.md.
	 */

	//------------------------------------------------------------------
	// Settings, stored in the PhobGCC extras EEPROM slot
	//------------------------------------------------------------------

	const ExtrasSlot slot = EXTRAS_LEFT;

	enum TapJumpSetting {
		TAPJUMP_ENABLE,
		TAPJUMP_CAP,
		TAPJUMP_UNUSED2,
		TAPJUMP_UNUSED3
	};

	/* 0 must mean ON. resetDefaults() writes 0 to every extras word, and a new
	 * controller runs a factory reset on first boot. If 0 meant OFF, the extra
	 * would be dead out of the box with nothing to show why. */
	enum TapJumpEnable {
		TAPJUMP_ON  = 0,
		TAPJUMP_OFF = 1
	};

	/* All values below are PhobGCC stick units: 100 is full deflection, and
	 * Melee divides the report by 80, so one unit is 0.0125 in Melee. */

	/* The Y ceiling. It has to sit inside a band:
	 *   at or above _crossY, or Melee marks its timer old and then reads the
	 *     lifting of the ceiling as a fresh crossing and jumps
	 *   below x80, the running jump threshold, or a dash jumps straight through
	 * x80 lives in PlCo.dat, so 28 is chosen low in that band. Lower is safer;
	 * higher keeps more stick magnitude during the window. */
	const int capDefault = 28;
	const int capMin = 24, capMax = 52;

	/* How long the ceiling stays on after a crossing. Melee's own window is about
	 * four frames and the ceiling must outlast it. Six frames of margin covers a
	 * lag frame, which stretches a frame past 16.7 ms. */
	const int windowMs = 100;

	//Melee restarts its tilt timer at this small threshold, not at the jump line
	const int crossY = 23;

	/* Melee's up-B threshold, 0.6625. B only takes the ceiling away once Y
	 * reaches this. Below it Melee reads a neutral-B either way, so handing over
	 * the true stick could not change which move comes out. */
	const int upBY = 53;

	/* Melee's dash threshold, 0.8. While the ceiling is on, X is normally scaled
	 * with Y to hold the angle, but past this line X keeps its true value:
	 * scaling it down would drop the report under the dash threshold and end a
	 * dash. This is a plain test on the reported stick, not a state guess. */
	const int dashX = 64;

	//------------------------------------------------------------------
	// State
	//------------------------------------------------------------------

	ExtrasSlot configSlot = slot;

	bool     _armed     = false;//inside the window after an upward crossing
	bool     _wasAbove  = false;//the true Y was above crossY last loop
	uint32_t _riseStart = 0;
	int      _capX      = 0;//the values reported while the ceiling is on
	int      _capY      = 0;
	bool     _capping   = false;//this extra wrote a modified stick
	int      _capLimit  = 0;
	int      _trueX     = 0;//the real stick, kept before the ceiling hides it
	int      _trueY     = 0;

	//------------------------------------------------------------------
	// Helpers
	//------------------------------------------------------------------

	inline bool enabled(const IntOrFloat config[]) {
		return config[TAPJUMP_ENABLE].intValue != TAPJUMP_OFF;
	}

	inline int cap(const IntOrFloat config[]) {
		const int v = config[TAPJUMP_CAP].intValue;
		return (v < capMin || v > capMax) ? capDefault : v;
	}

	inline void apply(Buttons &btn) {
		btn.Ax = (uint8_t) (_capX + _floatOrigin);
		btn.Ay = (uint8_t) (_capY + _floatOrigin);
	}

	inline void restore(Buttons &btn) {
		btn.Ax = (uint8_t) (_trueX + _floatOrigin);
		btn.Ay = (uint8_t) (_trueY + _floatOrigin);
	}

	inline bool lifts(const bool a, const bool b, const bool x, const bool y) {
		return a || x || y || (b && (_trueY >= upBY));
	}

	//------------------------------------------------------------------
	// Hooks
	//------------------------------------------------------------------

	/* Called from readSticks, directly after readSticks writes btn.Ax and btn.Ay.
	 *
	 * The whole decision happens here and it cannot happen later. The console
	 * polls from the other core at any moment. If the crossing were detected in
	 * processButtons instead, then on the loop where the stick crosses, btn would
	 * hold the true high Y for the whole button reading section. A poll landing
	 * in that gap reads a jump coordinate with a fresh timer, and Melee jumps.
	 *
	 * btn.A and btn.B are one loop old here. That is close enough to keep the
	 * ceiling off while a button is held, and check() corrects a fresh press
	 * before the console can see it. */
	void hold(Buttons &btn, const IntOrFloat config[]) {
		_trueX = (int) btn.Ax - _intOrigin;
		_trueY = (int) btn.Ay - _intOrigin;

		if(!enabled(config)) {
			_armed = false;
			_wasAbove = false;
			_capping = false;
			return;
		}

		const uint32_t now = micros();

		const bool above = (_trueY >= crossY);
		if(above && !_wasAbove) {
			_armed = true;
			_riseStart = now;
		} else if(!above) {
			_armed = false;
		}
		_wasAbove = above;

		if(_armed && (now - _riseStart) >= (uint32_t) windowMs * 1000u) {
			_armed = false;//Melee's timer has aged out, so Y is safe again
		}

		_capLimit = cap(config);
		_capping = _armed && !lifts(btn.A, btn.B, btn.X, btn.Y) && (_trueY > _capLimit);
		if(_capping) {
			const int ax = (_trueX < 0) ? -_trueX : _trueX;
			_capY = _capLimit;
			_capX = (ax >= dashX) ? _trueX : ((_trueX * _capLimit) / _trueY);
			apply(btn);
		}
	}

	/* Called from processButtons, before it copies the buttons into btn. Lifts
	 * the ceiling for a button pressed this loop, which hold() could not see yet.
	 * Running before copyButtons is what lets the true stick and the button press
	 * reach the console together. */
	void check(Buttons &btn, const bool aNow, const bool bNow, const bool xNow, const bool yNow) {
		if(_capping && lifts(aNow, bNow, xNow, yNow)) {
			_capping = false;
			restore(btn);
		}
	}
}

#endif //EXTRAS_TAPJUMP_H
