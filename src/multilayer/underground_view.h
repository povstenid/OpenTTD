/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_view.h Underground view mode state and helpers. */

#ifndef UNDERGROUND_VIEW_H
#define UNDERGROUND_VIEW_H

#include "../openttd.h"

/** The current underground view depth. Negative values = underground. */
extern int16_t _underground_view_depth;

/** Check if underground view mode is active. */
inline bool IsUndergroundViewActive()
{
	extern uint8_t _display_opt;
	return HasBit(_display_opt, DO_UNDERGROUND_VIEW);
}

/** Toggle underground view on/off. Also toggles surface transparency for cutaway effect. */
void ToggleUndergroundView();

/** Set the underground view depth. */
inline void SetUndergroundViewDepth(int16_t depth)
{
	_underground_view_depth = depth;
}

/** Get the current underground view depth. */
inline int16_t GetUndergroundViewDepth()
{
	return _underground_view_depth;
}

#endif /* UNDERGROUND_VIEW_H */
