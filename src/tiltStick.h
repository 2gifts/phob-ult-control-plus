/* Tilt Stick - a PhobGCC extra for Super Smash Bros. Melee
 *
 * Copyright (C) 2026 2gifts
 *
 * This file is part of Phob Ult Control Plus, an add-on for the PhobGCC
 * firmware. It is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See the LICENSE file at the root of this repository.
 */

#ifndef EXTRAS_TILTSTICK_H
#define EXTRAS_TILTSTICK_H

#include "../common/structsAndEnums.h"

namespace tiltStick {
	/* ------------------------------------------------------------------------
	 * Why a translation is needed
	 * ------------------------------------------------------------------------
	 * Melee has no tilt stick setting. On the ground the C-stick always smashes,
	 * and every tilt reads the LEFT stick together with A:
	 *
	 *   side tilt  A && lstick.x * facing >= x98 && abs(angle) <  x20_radians
	 *   up tilt    A && lstick.y >= attackhi3_stick_threshold_y && angle >  x20
	 *   down tilt  A && lstick.y <= xB0                         && angle < -x20
	 *
	 * (ftCo_AttackS3.c, ftCo_AttackHi3.c, ftCo_AttackLw3.c)
	 *
	 * So the C-stick cannot be made to tilt by filtering. It has to be
	 * translated: report the left stick at a tilt sized value in the C-stick's
	 * direction, and synthesise an A press.
	 *
	 * Aerials come out correct for free, because in the air A plus a direction
	 * already selects the aerial.
	 *
	 * ------------------------------------------------------------------------
	 * Two details that matter
	 * ------------------------------------------------------------------------
	 * The angle is kept, not snapped to a cardinal. Melee reads the tilt from the
	 * stick angle, and inside the side tilt band it uses that same angle to aim
	 * the attack up or down. Holding the C-stick off a notch therefore gives an
	 * angled forward tilt, as Fox has.
	 *
	 * The C-stick is centred for as long as it is deflected, not only during the
	 * press. Melee reads a C-stick smash from a CROSSING, so letting a still
	 * deflected C-stick snap back to its true value would read as a fresh flick
	 * and fire the smash this extra just replaced.
	 *
	 * ------------------------------------------------------------------------
	 * Known limits, all from the game rather than the firmware
	 * ------------------------------------------------------------------------
	 * A backward side tilt on the ground is not a Melee move. AttackS3 is facing
	 * relative and a controller cannot know which way you face, so a backward
	 * flick gives a jab. Aerials are unaffected and work in all directions.
	 *
	 * C-stick DI and SDI are gone while this is on, because the C-stick never
	 * reaches the console.
	 *
	 * If A is already held, the synthesised press has no rising edge, so no move
	 * comes out.
	 */

	//------------------------------------------------------------------
	// Settings, stored in the PhobGCC extras EEPROM slot
	//------------------------------------------------------------------

	const ExtrasSlot slot = EXTRAS_DOWN;

	enum TiltStickSetting {
		TILTSTICK_ENABLE,
		TILTSTICK_UNUSED1,
		TILTSTICK_UNUSED2,
		TILTSTICK_UNUSED3
	};

	//0 must mean ON. resetDefaults() writes 0 to every extras word.
	enum TiltStickEnable {
		TILTSTICK_ON  = 0,
		TILTSTICK_OFF = 1
	};

	//Stick units: 100 is full deflection
	const int trigger = 40;//C-stick travel needed to fire
	const int release = 28;//and to re-arm, so a resting stick cannot chatter

	/* Reported left stick length. Under Melee's smash lines of 64 on X and 53 on
	 * Y, so no timing can turn it into a smash, and clear of every tilt floor. */
	const int tiltMag = 45;

	const int holdMs = 34;//two frames, so the console is certain to read the press

	//------------------------------------------------------------------
	// State
	//------------------------------------------------------------------

	ExtrasSlot configSlot = slot;

	bool     _deflected = false;//C-stick is out, so keep it hidden
	bool     _firing    = false;
	uint32_t _fireStart = 0;
	int      _dirX      = 0;
	int      _dirY      = 0;

	inline bool enabled(const IntOrFloat config[]) {
		return config[TILTSTICK_ENABLE].intValue != TILTSTICK_OFF;
	}

	//------------------------------------------------------------------
	// Hooks
	//------------------------------------------------------------------

	/* Called from readSticks, after both sticks are written and after every other
	 * extra, so the translated input is what the console finally sees. */
	void hold(Buttons &btn, const IntOrFloat config[]) {
		if(!enabled(config)) {
			_deflected = false;
			_firing = false;
			return;
		}

		const int cx = (int) btn.Cx - _intOrigin;
		const int cy = (int) btn.Cy - _intOrigin;
		const int ax = (cx < 0) ? -cx : cx;
		const int ay = (cy < 0) ? -cy : cy;
		const int mag = (ax > ay) ? ax : ay;
		const uint32_t now = micros();

		if(!_deflected && mag >= trigger) {
			_deflected = true;
			_firing    = true;
			_fireStart = now;
			//Keep the angle, fix only the length
			const float len = sqrtf((float) (cx * cx + cy * cy));
			if(len < 1.0f) {
				_dirX = 0;
				_dirY = tiltMag;
			} else {
				_dirX = (int) ((cx * (float) tiltMag) / len);
				_dirY = (int) ((cy * (float) tiltMag) / len);
			}
		} else if(_deflected && mag < release) {
			_deflected = false;
		}

		if(_firing && (now - _fireStart) >= (uint32_t) holdMs * 1000u) {
			_firing = false;
		}

		if(_deflected) {
			btn.Cx = (uint8_t) _intOrigin;
			btn.Cy = (uint8_t) _intOrigin;
		}
		if(_firing) {
			btn.Ax = (uint8_t) (_dirX + _floatOrigin);
			btn.Ay = (uint8_t) (_dirY + _floatOrigin);
			/* Set A here as well as in injectButtons. Writing it beside the stick
			 * closes the gap where the console could poll a tilt sized stick with
			 * no button attached, which another extra could read as a bare input. */
			btn.A = (uint8_t) 1;
		}
	}

	/* Called from processButtons on tempBtn, before anything else reads the
	 * buttons, so the synthesised press is visible to the other extras and
	 * survives copyButtons. */
	void injectButtons(Buttons &tempBtn) {
		if(_firing) {
			tempBtn.A = (uint8_t) 1;
		}
	}
}

#endif //EXTRAS_TILTSTICK_H
