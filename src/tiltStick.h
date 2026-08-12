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
	 * Where this runs, and why it matters
	 * ------------------------------------------------------------------------
	 * This extra edits the stick values BEFORE readSticks writes them into the
	 * report, rather than overwriting the report afterwards.
	 *
	 * That is not a detail. The console polls from the other core at any moment.
	 * If the true C-stick were written first and corrected a moment later, then
	 * on every single loop there would be a short window holding a live C-stick
	 * value. A poll landing in one of those windows reads a C-stick flick, and
	 * Melee fires the smash attack this extra exists to replace. At a 1 kHz loop
	 * against a 60 Hz poll that leaks every few seconds of holding the C-stick,
	 * which is exactly the "sometimes a smash comes out" symptom. Writing the
	 * corrected value in the first place removes the window entirely.
	 *
	 * ------------------------------------------------------------------------
	 * The C-stick is silenced completely
	 * ------------------------------------------------------------------------
	 * While this extra is on, the C-stick NEVER reaches the console. Not when
	 * held, not when returning to centre, not partially.
	 *
	 * Melee reads a C-stick smash from a CROSSING, so any value getting through
	 * risks a smash, and a partial value that is too small to smash still moves
	 * the camera and the character select cursor. There is no safe amount to let
	 * through, so none is.
	 *
	 * One flick gives one tilt. Nothing else happens until the C-stick returns
	 * near centre AND stays there briefly, which re-arms it for the next input.
	 * The wait is what stops snapback ringing from firing a second, opposite
	 * tilt on the way back.
	 *
	 * ------------------------------------------------------------------------
	 * Two details that matter
	 * ------------------------------------------------------------------------
	 * The angle is kept, not snapped to a cardinal. Melee reads the tilt from the
	 * stick angle, and inside the side tilt band it uses that same angle to aim
	 * the attack up or down. Holding the C-stick off a notch therefore gives an
	 * angled forward tilt, as Fox has.
	 *
	 * A is set on the button struct as well as on the copy processButtons uses.
	 * The other extras read it from there, and an A press is what tells the tap
	 * jump lockout that a real move is happening.
	 *
	 * ------------------------------------------------------------------------
	 * Known limits, all from the game rather than the firmware
	 * ------------------------------------------------------------------------
	 * A backward side tilt on the ground is not a Melee move. AttackS3 is facing
	 * relative and a controller cannot know which way you face, so a backward
	 * flick gives a jab. Aerials are unaffected.
	 *
	 * C-stick DI and SDI are gone while this is on, as is anything else that
	 * wants a live C-stick, including PhobGCC's own two-stick extras combos.
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
	const int trigger = 40;//C-stick travel that fires a tilt
	const int rearm   = 20;//and the travel it must fall back under to fire again

	/* How long the C-stick must stay under rearm before it may fire again.
	 *
	 * Releasing a stick overshoots past centre and rings for a few milliseconds.
	 * PhobGCC's Kalman snapback filter only runs on the LEFT stick; the C-stick
	 * gets smoothing, which shrinks the overshoot but does not remove it. Without
	 * this wait, letting go of a down input rings up past the trigger and fires
	 * an up tilt on the way back. Melee's own tilt endlag is far longer than
	 * this, so it costs nothing real. */
	const int settleMs = 40;

	/* Reported left stick length. Under Melee's smash lines of 64 on X and 53 on
	 * Y, so no timing can turn it into a smash, and clear of every tilt floor. */
	const int tiltMag = 45;

	const int holdMs = 34;//two frames, so the console is certain to read the press

	//------------------------------------------------------------------
	// State
	//------------------------------------------------------------------

	ExtrasSlot configSlot = slot;

	bool     _armed      = true; //the C-stick has settled near centre
	bool     _firing     = false;
	uint32_t _fireStart  = 0;
	uint32_t _centreSince = 0;   //when it fell under rearm
	bool     _atCentre   = true;
	int      _dirX      = 0;
	int      _dirY      = 0;

	inline bool enabled(const IntOrFloat config[]) {
		return config[TILTSTICK_ENABLE].intValue != TILTSTICK_OFF;
	}

	//------------------------------------------------------------------
	// Hooks
	//------------------------------------------------------------------

	/* Called from readSticks after the stick values are clamped and BEFORE they
	 * are written into btn. Editing them here is what closes the polling window
	 * described above.
	 *
	 * calStep is passed so the extra stands aside during stick calibration,
	 * which needs to see the real C-stick. */
	void hold(Buttons &btn, float &ax, float &ay, float &cx, float &cy,
	          const int calStep, const IntOrFloat config[]) {
		if(!enabled(config) || calStep != -1) {
			_armed = true;
			_atCentre = true;
			_firing = false;
			return;
		}

		const int icx = (int) cx;
		const int icy = (int) cy;
		const int adx = (icx < 0) ? -icx : icx;
		const int ady = (icy < 0) ? -icy : icy;
		const int mag = (adx > ady) ? adx : ady;
		const uint32_t now = micros();

		/* Re-arm only after the stick has been quiet under rearm for settleMs.
		 * Snapback ringing crosses back over rearm, which restarts the wait. */
		if(mag < rearm) {
			if(!_atCentre) {
				_atCentre = true;
				_centreSince = now;
			} else if(!_armed && (now - _centreSince) >= (uint32_t) settleMs * 1000u) {
				_armed = true;
			}
		} else {
			_atCentre = false;
		}

		if(_armed && mag >= trigger) {
			_armed     = false;//one flick, one tilt, until it settles again
			_firing    = true;
			_fireStart = now;
			//Keep the angle, fix only the length
			const float len = sqrtf((float) (icx * icx + icy * icy));
			if(len < 1.0f) {
				_dirX = 0;
				_dirY = tiltMag;
			} else {
				_dirX = (int) ((icx * (float) tiltMag) / len);
				_dirY = (int) ((icy * (float) tiltMag) / len);
			}
		}

		if(_firing && (now - _fireStart) >= (uint32_t) holdMs * 1000u) {
			_firing = false;
		}

		//Silence the C-stick unconditionally. See the note above.
		cx = 0.0f;
		cy = 0.0f;

		if(_firing) {
			ax = (float) _dirX;
			ay = (float) _dirY;
			//Set A beside the stick so no poll can see one without the other
			btn.A = (uint8_t) 1;
		}
	}

	/* Called from processButtons on tempBtn, before anything else reads the
	 * buttons, so the synthesised press survives copyButtons. */
	void injectButtons(Buttons &tempBtn) {
		if(_firing) {
			tempBtn.A = (uint8_t) 1;
		}
	}
}

#endif //EXTRAS_TILTSTICK_H
