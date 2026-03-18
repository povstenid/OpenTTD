/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_view.cpp Underground view mode state and cutaway transparency. */

#include "../stdafx.h"
#include "underground_view.h"
#include "../transparency.h"

#include "../safeguards.h"

/** Current underground view depth (default: -1 = first underground level). */
int16_t _underground_view_depth = -1;

/** Saved transparency state before entering underground view. */
static TransparencyOptionBits _saved_transparency_opt = 0;
static bool _transparency_saved = false;

/**
 * Toggle underground view on/off.
 * When enabling: save current transparency and enable cutaway (transparent trees, houses, etc.)
 * When disabling: restore original transparency.
 */
void ToggleUndergroundView()
{
	extern uint8_t _display_opt;
	ToggleBit(_display_opt, DO_UNDERGROUND_VIEW);

	if (IsUndergroundViewActive()) {
		/* Entering underground view — enable cutaway transparency. */
		_saved_transparency_opt = _transparency_opt;
		_transparency_saved = true;

		/* Make trees, houses, industries, buildings, structures transparent. */
		SetBit(_transparency_opt, TO_TREES);
		SetBit(_transparency_opt, TO_HOUSES);
		SetBit(_transparency_opt, TO_INDUSTRIES);
		SetBit(_transparency_opt, TO_BUILDINGS);
		SetBit(_transparency_opt, TO_STRUCTURES);
		SetBit(_transparency_opt, TO_BRIDGES);
	} else {
		/* Leaving underground view — restore transparency. */
		if (_transparency_saved) {
			_transparency_opt = _saved_transparency_opt;
			_transparency_saved = false;
		}
	}
}
