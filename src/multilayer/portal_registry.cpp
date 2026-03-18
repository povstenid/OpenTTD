/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file portal_registry.cpp BFS-based portal connection precomputation. */

#include "../stdafx.h"
#include "portal_registry.h"
#include "multilayer_map.h"
#include "../map_func.h"
#include "../track_func.h"
#include "../direction_func.h"
#include "../debug.h"

#include "../safeguards.h"

#include <unordered_map>
#include <unordered_set>
#include <queue>

/** Global registry: entry_tile → list of connections. */
static std::unordered_map<uint32_t, std::vector<PortalConnection>> _portal_connections;

/** Empty vector for tiles with no connections. */
static const std::vector<PortalConnection> _empty_connections;

const std::vector<PortalConnection> &GetPortalConnections(TileIndex entry_tile)
{
	auto it = _portal_connections.find(entry_tile.base());
	if (it != _portal_connections.end()) return it->second;
	return _empty_connections;
}

void ClearPortalConnections()
{
	_portal_connections.clear();
}

/**
 * BFS from a portal tile through underground Track slices.
 * Finds all reachable portal tiles and records connections.
 */
static void BFSFromPortal(TileIndex entry_tile, SliceID portal_sid)
{
	const TileSlice &entry_ps = _multilayer_map.GetSlice(portal_sid);
	int16_t depth = entry_ps.span.z_bot;
	DiagDirection entry_dir = static_cast<DiagDirection>(entry_ps.portal.direction);

	/* Start BFS from the tile adjacent to portal in its direction. */
	TileIndexDiffC step = TileIndexDiffCByDiagDir(entry_dir);
	int start_x = TileX(entry_tile) + step.x;
	int start_y = TileY(entry_tile) + step.y;

	if (start_x < 0 || start_y < 0 ||
	    start_x >= (int)Map::SizeX() || start_y >= (int)Map::SizeY()) return;

	TileIndex start_tile = TileXY(start_x, start_y);

	/* Check if start tile has underground track at this depth. */
	SliceID start_sid = _multilayer_map.FindSliceAt(start_tile, depth);
	if (start_sid == INVALID_SLICE_ID) return;
	const TileSlice &start_slice = _multilayer_map.GetSlice(start_sid);
	if (start_slice.kind != SliceKind::Track) return;

	/* BFS through underground Track slices. */
	struct BFSNode {
		TileIndex tile;
		int distance;
	};

	std::queue<BFSNode> queue;
	std::unordered_set<uint32_t> visited;

	queue.push({start_tile, 1});
	visited.insert(start_tile.base());

	while (!queue.empty()) {
		BFSNode current = queue.front();
		queue.pop();

		/* Check all 4 neighbors. */
		for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
			TileIndexDiffC diff = TileIndexDiffCByDiagDir(dir);
			int nx = TileX(current.tile) + diff.x;
			int ny = TileY(current.tile) + diff.y;

			if (nx < 0 || ny < 0 || nx >= (int)Map::SizeX() || ny >= (int)Map::SizeY()) continue;

			TileIndex neighbor = TileXY(nx, ny);
			if (visited.count(neighbor.base())) continue;

			/* Check what's at this neighbor tile at our depth. */
			SliceID nsid = _multilayer_map.FindSliceAt(neighbor, depth);
			if (nsid == INVALID_SLICE_ID) continue;

			const TileSlice &nslice = _multilayer_map.GetSlice(nsid);

			if (nslice.kind == SliceKind::Track) {
				/* Check if there's a track connecting current→neighbor. */
				SliceID cur_sid = _multilayer_map.FindSliceAt(current.tile, depth);
				if (cur_sid == INVALID_SLICE_ID) continue;
				const TileSlice &cur_slice = _multilayer_map.GetSlice(cur_sid);
				if (cur_slice.kind != SliceKind::Track) continue;

				/* Check track connectivity: current tile must have a track
				 * that exits in direction 'dir'. */
				TrackBits cur_tracks = static_cast<TrackBits>(cur_slice.track.tracks);
				TrackBits exit_tracks = DiagdirReachesTracks(dir);
				if (!(cur_tracks & exit_tracks)) continue;

				/* Neighbor track must be reachable from entering direction. */
				TrackBits n_tracks = static_cast<TrackBits>(nslice.track.tracks);
				TrackBits enter_tracks = DiagdirReachesTracks(ReverseDiagDir(dir));
				if (!(n_tracks & enter_tracks)) continue;

				visited.insert(neighbor.base());
				queue.push({neighbor, current.distance + 1});

			} else if (nslice.kind == SliceKind::Portal && neighbor != entry_tile) {
				/* Found an exit portal! Record connection. */
				DiagDirection exit_dir = static_cast<DiagDirection>(nslice.portal.direction);

				PortalConnection conn;
				conn.entry_tile = entry_tile;
				conn.exit_tile = neighbor;
				conn.path_length = current.distance + 1;
				conn.entry_dir = entry_dir;
				conn.exit_dir = ReverseDiagDir(exit_dir);
				conn.depth = depth;

				_portal_connections[entry_tile.base()].push_back(conn);

				visited.insert(neighbor.base());
				/* Don't continue BFS through portal — it's an endpoint. */
			}
		}
	}
}

/**
 * Recompute all portal connections by scanning the map for Portal slices
 * and running BFS from each one.
 */
void RecomputePortalConnections()
{
	_portal_connections.clear();

	if (!_multilayer_map.IsAllocated()) return;

	int portal_count = 0;

	/* Find all Portal slices on the map. */
	for (uint i = 0; i < Map::Size(); i++) {
		TileIndex tile(i);
		const TileColumn &col = _multilayer_map.GetColumn(tile);

		for (SliceID sid : col.slices) {
			const TileSlice &s = _multilayer_map.GetSlice(sid);
			if (s.kind == SliceKind::Portal) {
				BFSFromPortal(tile, sid);
				portal_count++;
			}
		}
	}

	int connection_count = 0;
	for (const auto &[key, conns] : _portal_connections) {
		connection_count += static_cast<int>(conns.size());
	}

	Debug(misc, 1, "Portal registry: {} portals, {} connections", portal_count, connection_count);
}
