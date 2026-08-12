/* Free Shield Tilt - a PhobGCC extra for Super Smash Bros. Melee
 *
 * Copyright (C) 2026 2gifts
 *
 * This file is part of Phob Ult Control Plus, an add-on for the PhobGCC
 * firmware. It is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See the LICENSE file at the root of this repository.
 */

#ifndef EXTRAS_SHIELDTILT_H
#define EXTRAS_SHIELDTILT_H

#include "../common/structsAndEnums.h"

namespace shieldTilt {
	/* ------------------------------------------------------------------------
	 * How Melee decides to leave a shield
	 * ------------------------------------------------------------------------
	 * All three escapes need a FAST stick input, not just a far one:
	 *
	 *   roll       abs(lstick.x) >= x31C && X timer < x320   ftCo_Escape.c
	 *   spotdodge  lstick.y <= x314      && Y timer < x318   ftCo_Escape.c
	 *   jump       lstick.y >= tap_jump  && Y timer < x74    ftCo_Jump.c
	 *
	 * Each timer restarts when that axis crosses a small threshold, then counts
	 * frames (fighter.c).
	 *
	 * ------------------------------------------------------------------------
	 * What this extra does
	 * ------------------------------------------------------------------------
	 * While both shields are held, each axis is CLAMPED, immediately, to a value
	 * below every escape line. The shield tilts the moment the stick moves, with
	 * no added delay.
	 *
	 * Once an axis has been held past the small threshold for longer than any
	 * escape window, the clamp lifts and the true stick goes through. By then
	 * Melee's timer has aged out, and lifting a clamp is not a crossing, so
	 * nothing can fire. Hold a direction and you reach full deflection.
	 *
	 * So: instant response up to the clamp, full range if you keep holding, and
	 * no roll, spotdodge or jump at any point.
	 *
	 * An earlier version rate limited the stick instead. It was safe on paper and
	 * felt terrible, because every shield tilt then lagged the hand. Clamping and
	 * releasing gives the same safety with none of the lag.
	 *
	 * ------------------------------------------------------------------------
	 * The C-stick
	 * ------------------------------------------------------------------------
	 * Its shield escapes have NO timer at all and fire on position alone
	 * (ftCo_800DF8B0, ftCo_800DF8E8, ftCo_800DF910 in ft_0DF1.c), so neither a
	 * clamp nor a delay can make them safe. It is centred while the lock is on.
	 * The C-stick does not tilt the shield in Melee, so nothing is lost.
	 */

	//------------------------------------------------------------------
	// Settings, stored in the PhobGCC extras EEPROM slot
	//------------------------------------------------------------------

	const ExtrasSlot slot = EXTRAS_UP;

	enum ShieldTiltSetting {
		SHIELDTILT_ENABLE,
		SHIELDTILT_UNUSED1,
		SHIELDTILT_UNUSED2,
		SHIELDTILT_UNUSED3
	};

	//0 must mean ON. resetDefaults() writes 0 to every extras word.
	enum ShieldTiltEnable {
		SHIELDTILT_ON  = 0,
		SHIELDTILT_OFF = 1
	};

	/* Clamps, in stick units. Melee's escape lines are about 64 for a roll and 53
	 * for a spotdodge or a jump, so these sit clear below them. */
	const int clampX = 50;
	const int clampY = 42;

	//Melee's small tilt threshold, where its timers restart
	const int crossV = 23;

	/* Hold an axis past that threshold this long and its clamp lifts. Melee's
	 * escape windows are a few frames; this is about twelve, with margin. */
	const uint32_t freeMs = 200;

	/* A trigger counts as held from either the digital press or a firm analog
	 * press. Light shielding never reaches the digital point, and the lock has to
	 * work there too. */
	const int analogHeld = 60;

	//------------------------------------------------------------------
	// State
	//------------------------------------------------------------------

	ExtrasSlot configSlot = slot;

	bool     _xPast  = false;//the X axis is past crossV
	bool     _yPast  = false;
	uint32_t _xSince = 0;//and since when
	uint32_t _ySince = 0;

	inline bool enabled(const IntOrFloat config[]) {
		return config[SHIELDTILT_ENABLE].intValue != SHIELDTILT_OFF;
	}

	inline int clampTo(const int v, const int limit) {
		if(v >  limit) { return  limit; }
		if(v < -limit) { return -limit; }
		return v;
	}

	/* Track how long one axis has been past the small threshold. Returns true
	 * once that has outlasted every escape window, so the clamp can go. */
	inline bool axisFree(const int v, bool &past, uint32_t &since, const uint32_t now) {
		const int a = (v < 0) ? -v : v;
		if(a < crossV) {
			past = false;
			return false;
		}
		if(!past) {
			past = true;
			since = now;
			return false;
		}
		return (now - since) >= freeMs * 1000u;
	}

	//------------------------------------------------------------------
	// Hook
	//------------------------------------------------------------------

	//Called from readSticks, after btn.Ax, btn.Ay, btn.Cx and btn.Cy are written.
	void hold(Buttons &btn, const IntOrFloat config[]) {
		const int trueX = (int) btn.Ax - _intOrigin;
		const int trueY = (int) btn.Ay - _intOrigin;

		const bool lHeld = btn.L || (btn.La >= analogHeld);
		const bool rHeld = btn.R || (btn.Ra >= analogHeld);

		if(!enabled(config) || !lHeld || !rHeld) {
			_xPast = false;
			_yPast = false;
			return;
		}

		const uint32_t now = micros();
		const bool xFree = axisFree(trueX, _xPast, _xSince, now);
		const bool yFree = axisFree(trueY, _yPast, _ySince, now);

		if(!xFree) {
			btn.Ax = (uint8_t) (clampTo(trueX, clampX) + _floatOrigin);
		}
		if(!yFree) {
			btn.Ay = (uint8_t) (clampTo(trueY, clampY) + _floatOrigin);
		}

		//The C-stick escapes have no timer, so centre it
		btn.Cx = (uint8_t) _intOrigin;
		btn.Cy = (uint8_t) _intOrigin;
	}
}

#endif //EXTRAS_SHIELDTILT_H
