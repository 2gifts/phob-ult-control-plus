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

	/* Clamps, in stick units, where 80 units is 1.0 in Melee's own scale.
	 *
	 * These match the values the pico-rectangle firmware uses for the same job.
	 * A digital controller reaches its coordinate in a single frame, so anything
	 * it can hold while shielding is a coordinate Melee will not escape from even
	 * with its timer at zero. That makes it a useful independent check on numbers
	 * that otherwise come from PlCo.dat estimates.
	 *
	 * Sideways: 0.6625, which is 53 units. Held while shielding without rolling.
	 * Down: 0.5375, which is 43 units. Held while shielding without spotdodging.
	 *
	 * Y stays well short of Melee's shield drop band (-0.6625 to -0.6875) on
	 * purpose. Going further would drop you through a platform when you only
	 * wanted to angle the shield.
	 */
	const int clampX = 53;
	const int clampY = 43;

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

	/* Called from readSticks after the stick values are clamped and BEFORE they
	 * are written into btn. btn is still needed for the trigger state, which is
	 * one loop old and none the worse for it.
	 *
	 * calStep is passed so the extra stands aside during stick calibration. */
	void hold(Buttons &btn, float &ax, float &ay, float &cx, float &cy,
	          const int calStep, const IntOrFloat config[]) {
		const bool lHeld = btn.L || (btn.La >= analogHeld);
		const bool rHeld = btn.R || (btn.Ra >= analogHeld);

		if(!enabled(config) || calStep != -1 || !lHeld || !rHeld) {
			_xPast = false;
			_yPast = false;
			return;
		}

		const int trueX = (int) ax;
		const int trueY = (int) ay;
		const uint32_t now = micros();

		if(!axisFree(trueX, _xPast, _xSince, now)) {
			ax = (float) clampTo(trueX, clampX);
		}
		if(!axisFree(trueY, _yPast, _ySince, now)) {
			ay = (float) clampTo(trueY, clampY);
		}

		//The C-stick escapes have no timer, so silence it
		cx = 0.0f;
		cy = 0.0f;
	}
}

#endif //EXTRAS_SHIELDTILT_H
