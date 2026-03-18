/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file legacy_import.cpp Materialize vanilla tunnels as real underground Track slices. */

#include "../stdafx.h"
#include "legacy_import.h"
#include "multilayer_map.h"
#include "../map_func.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../rail_map.h"
#include "../tile_map.h"
#include "../debug.h"

#include "../safeguards.h"

/** Default depth for materialized tunnels. */
static constexpr int16_t DEFAULT_TUNNEL_DEPTH = -1;

/**
 * Materialize all existing tunnels as real underground Track slices.
 *
 * Scans the map for tunnel portals. For each portal pair, creates:
 * 1. A Portal slice at each portal tile
 * 2. A chain of Track slices connecting the two portals at z=-1
 *
 * This is a one-time conversion run during legacy save loading.
 */
void MaterializeLegacyTunnels()
{
	uint tunnels_materialized = 0;

	for (const auto tile : Map::Iterate()) {
		/* Only process tunnel entrance tiles (south/west portal to avoid doubles). */
		if (!IsTunnelTile(tile)) continue;

		DiagDirection dir = GetTunnelBridgeDirection(tile);

		/* Only process one end of each tunnel to avoid duplicates.
		 * Process SW and SE facing portals only. */
		if (dir != DIAGDIR_SW && dir != DIAGDIR_SE) continue;

		TileIndex other_end = GetOtherTunnelEnd(tile);
		RailType rt = GetRailType(tile);

		/* Determine track direction from tunnel direction. */
		Track track;
		if (dir == DIAGDIR_SW || dir == DIAGDIR_NE) {
			track = TRACK_X;
		} else {
			track = TRACK_Y;
		}

		int16_t depth = DEFAULT_TUNNEL_DEPTH;

		/* Create Portal slices at both portals. */
		TileIndex tile_idx = tile; /* Convert Tile to TileIndex. */
		for (int portal = 0; portal < 2; portal++) {
			TileIndex portal_tile = (portal == 0) ? tile_idx : other_end;

			SliceID portal_sid = _multilayer_map.AllocateSlice();
			TileSlice &ps = _multilayer_map.GetSlice(portal_sid);
			ps.kind = SliceKind::Portal;
			ps.span = {static_cast<int16_t>(depth + 1), depth};
			ps.owner = GetTileOwner(portal_tile).base();
			ps.portal.direction = static_cast<uint8_t>(dir);
			ps.portal.target_slice = INVALID_SLICE_ID; /* Will be linked later if needed. */

			_multilayer_map.InsertSlice(portal_tile, portal_sid);
		}

		/* Create Track slices between the portals. */
		TileIndexDiffC step = TileIndexDiffCByDiagDir(dir);
		TileIndex cur = tile_idx;

		/* Move one tile from the first portal into the tunnel. */
		int cx = TileX(cur) + step.x;
		int cy = TileY(cur) + step.y;
		cur = TileXY(cx, cy);

		while (cur != other_end) {
			SliceID track_sid = _multilayer_map.AllocateSlice();
			TileSlice &ts = _multilayer_map.GetSlice(track_sid);
			ts.kind = SliceKind::Track;
			ts.span = {static_cast<int16_t>(depth + 1), depth};
			ts.owner = GetTileOwner(tile_idx).base();
			ts.track.railtype = static_cast<uint8_t>(rt);
			ts.track.tracks = TrackToTrackBits(track);
			ts.track.signal_mask = 0;
			ts.track.reserved = 0;

			_multilayer_map.InsertSlice(cur, track_sid);

			/* Advance to next tile. */
			cx = TileX(cur) + step.x;
			cy = TileY(cur) + step.y;
			cur = TileXY(cx, cy);
		}

		tunnels_materialized++;
	}

	Debug(map, 1, "Materialized {} tunnels into multi-layer map", tunnels_materialized);
}
