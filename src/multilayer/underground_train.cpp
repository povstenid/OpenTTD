/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_train.cpp Underground train movement implementation. */

#include "../stdafx.h"
#include "underground_train.h"
#include "multilayer_map.h"
#include "../vehicle_base.h"
#include "../train.h"
#include "../track_func.h"
#include "../direction_func.h"
#include "../map_func.h"

#include "../safeguards.h"

#include <unordered_map>

/** Map from vehicle index to underground state. */
static std::unordered_map<uint32_t, UndergroundTrainState> _underground_train_states;

UndergroundTrainState *GetUndergroundState(const Train *v)
{
	auto it = _underground_train_states.find(v->index.base());
	if (it == _underground_train_states.end()) return nullptr;
	return &it->second;
}

UndergroundTrainState &GetOrCreateUndergroundState(const Train *v)
{
	return _underground_train_states[v->index.base()];
}

void ClearUndergroundState(const Train *v)
{
	_underground_train_states.erase(v->index.base());
}

/**
 * Check if a tile has a portal that a train can enter from a given direction.
 */
bool HasEnterablePortal(TileIndex tile, DiagDirection enterdir)
{
	SliceID sid = _multilayer_map.FindSliceByKind(tile, SliceKind::Portal);
	if (sid == INVALID_SLICE_ID) return false;

	const TileSlice &ps = _multilayer_map.GetSlice(sid);
	/* A surface train can enter a portal when it approaches along the same rail axis.
	 * The exact diagonal may differ depending on how the surface throat was built. */
	return DiagDirToAxis(static_cast<DiagDirection>(ps.portal.direction)) == DiagDirToAxis(enterdir);
}

/**
 * Get underground track bits at a tile and depth.
 */
TrackBits GetUndergroundTrackBits(TileIndex tile, int16_t depth, DiagDirection enterdir)
{
	SliceID sid = _multilayer_map.FindSliceAt(tile, depth);
	if (sid == INVALID_SLICE_ID) return TRACK_BIT_NONE;

	const TileSlice &slice = _multilayer_map.GetSlice(sid);

	if (slice.kind == SliceKind::Track) {
		TrackBits all_tracks = static_cast<TrackBits>(slice.track.tracks);
		/* Filter by reachability from enterdir. */
		return all_tracks & DiagdirReachesTracks(enterdir);
	}

	if (slice.kind == SliceKind::StationTile) {
		TrackBits all_tracks = static_cast<TrackBits>(slice.station.tracks);
		return all_tracks & DiagdirReachesTracks(enterdir);
	}

	/* Portal slices return NO tracks — exit is handled by TrainController
	 * dead-end detection: bits==NONE → check Portal → exit to surface. */

	return TRACK_BIT_NONE;
}

/**
 * Get the length of an underground station platform by walking in a direction.
 */
int GetUndergroundPlatformLength(TileIndex tile, int16_t depth, uint16_t station_id, DiagDirection dir)
{
	int length = 0;
	TileIndexDiffC diff = TileIndexDiffCByDiagDir(dir);

	while (true) {
		int x = TileX(tile);
		int y = TileY(tile);
		if (x < 0 || y < 0 || x >= (int)Map::SizeX() || y >= (int)Map::SizeY()) break;

		SliceID sid = _multilayer_map.FindSliceAt(tile, depth);
		if (sid == INVALID_SLICE_ID) break;

		const TileSlice &slice = _multilayer_map.GetSlice(sid);
		if (slice.kind != SliceKind::StationTile) break;
		if (slice.station.station_id != station_id) break;

		length++;
		int nx = x + diff.x;
		int ny = y + diff.y;
		if (nx < 0 || ny < 0 || nx >= (int)Map::SizeX() || ny >= (int)Map::SizeY()) break;
		tile = TileXY(nx, ny);
	}

	return length;
}
