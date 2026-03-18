/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_train.h Underground train movement support. */

#ifndef UNDERGROUND_TRAIN_H
#define UNDERGROUND_TRAIN_H

#include "multilayer_types.h"
#include "../track_type.h"
#include "../direction_type.h"

struct Train;

/**
 * Per-train underground state.
 * Stored externally (not in Train struct, to minimize core changes).
 */
struct UndergroundTrainState {
	bool is_underground = false;  ///< True if train is currently underground.
	TileRef current_ref;          ///< Current position in multi-layer map.
	int16_t depth = 0;            ///< Current underground depth.
};

/**
 * Get the underground state for a train.
 * Returns nullptr if the train has no underground state.
 */
UndergroundTrainState *GetUndergroundState(const Train *v);

/**
 * Create or get underground state for a train entering underground.
 */
UndergroundTrainState &GetOrCreateUndergroundState(const Train *v);

/**
 * Remove underground state when train returns to surface.
 */
void ClearUndergroundState(const Train *v);

/**
 * Check if a tile has a portal that a train can enter.
 * @param tile The surface tile.
 * @param enterdir Direction the train is entering from.
 * @return True if there's an enterable portal.
 */
bool HasEnterablePortal(TileIndex tile, DiagDirection enterdir);

/**
 * Get the underground track bits available at a given tile and depth.
 * This is the underground equivalent of GetTileTrackStatus.
 * @param tile Surface tile position.
 * @param depth Underground depth.
 * @param enterdir Direction entering the tile.
 * @return Available TrackBits.
 */
TrackBits GetUndergroundTrackBits(TileIndex tile, int16_t depth, DiagDirection enterdir);

#endif /* UNDERGROUND_TRAIN_H */
