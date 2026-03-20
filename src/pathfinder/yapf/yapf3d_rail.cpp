/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf3d_rail.cpp YAPF3D: A* pathfinder for underground rail tracks. */

#include "../../stdafx.h"
#include "yapf3d_rail.h"
#include "../../multilayer/multilayer_map.h"
#include "../../pbs.h"
#include "../../debug.h"
#include "../../map_func.h"
#include "../../track_func.h"
#include "../../direction_func.h"

#include "../../safeguards.h"

/** Maximum nodes to explore before giving up. */
static constexpr int YAPF3D_MAX_NODES = 10000;

/** Cost of one underground tile. */
static constexpr int YAPF3D_TILE_COST = 100;

/** Extra cost for curves underground. */
static constexpr int YAPF3D_CURVE_PENALTY = 200;

/**
 * Enumerate underground track neighbors from a given TileRef + Trackdir.
 *
 * For each exit direction of the current trackdir, checks the adjacent tile
 * at the same Z depth for compatible Track slices.
 */
void Yapf3DEnumerateNeighbors(const TileRef &ref, Trackdir td,
                               std::vector<std::pair<TileRef, Trackdir>> &out_neighbors)
{
	out_neighbors.clear();

	const TileSlice &cur_slice = _multilayer_map.GetSlice(ref.slice);
	if (cur_slice.kind != SliceKind::Track && cur_slice.kind != SliceKind::Portal && cur_slice.kind != SliceKind::StationTile) return;

	int16_t depth = cur_slice.span.z_bot;

	/* Get the exit direction from current trackdir. */
	DiagDirection exitdir = TrackdirToExitdir(td);

	/* Calculate neighbor tile. */
	TileIndexDiffC diff = TileIndexDiffCByDiagDir(exitdir);
	int nx = TileX(ref.tile) + diff.x;
	int ny = TileY(ref.tile) + diff.y;

	/* Bounds check. */
	if (nx < 0 || ny < 0 || nx >= (int)Map::SizeX() || ny >= (int)Map::SizeY()) return;

	TileIndex neighbor_tile = TileXY(nx, ny);

	/* Find a Track or Portal slice at the same depth in the neighbor column. */
	SliceID neighbor_sid = _multilayer_map.FindSliceAt(neighbor_tile, depth);
	if (neighbor_sid == INVALID_SLICE_ID) return;

	const TileSlice &neighbor_slice = _multilayer_map.GetSlice(neighbor_sid);
	if (neighbor_slice.kind != SliceKind::Track && neighbor_slice.kind != SliceKind::Portal && neighbor_slice.kind != SliceKind::StationTile) return;

	TileRef neighbor_ref{neighbor_tile, neighbor_sid};

	/* `TrackEnterdirToTrackdir` expects the movement direction into the tile,
	 * not the side we are coming from. For the neighboring tile that is the
	 * same direction we used to leave the current tile. */
	DiagDirection move_dir = exitdir;

	if (neighbor_slice.kind == SliceKind::Track || neighbor_slice.kind == SliceKind::StationTile) {
		TrackBits tracks = (neighbor_slice.kind == SliceKind::Track)
			? static_cast<TrackBits>(neighbor_slice.track.tracks)
			: static_cast<TrackBits>(neighbor_slice.station.tracks);
		TrackBits reachable = DiagdirReachesTracks(move_dir) & tracks;

		for (Track t = TRACK_BEGIN; t < TRACK_END; t++) {
			if (!(reachable & TrackToTrackBits(t))) continue;
			Trackdir new_td = TrackEnterdirToTrackdir(t, move_dir);

			/* Check if neighbor has a PBS signal on this track that is RED. */
			if (neighbor_slice.kind == SliceKind::Track &&
			    HasUndergroundSignalOnTrack(neighbor_ref, t) &&
			    GetUndergroundSignalState(neighbor_ref, new_td) == SIGNAL_STATE_RED) {
				continue; /* Blocked by red signal. */
			}

			out_neighbors.emplace_back(neighbor_ref, new_td);
		}
	} else if (neighbor_slice.kind == SliceKind::Portal) {
		/* Portal exit — return to surface. Use the entry trackdir. */
		Trackdir new_td = DiagDirToDiagTrackdir(exitdir);
		out_neighbors.emplace_back(neighbor_ref, new_td);
	}
}

/* Forward declaration — defined after Yapf3DFindPath. */
static void RunYapf3DSearch(
	std::unordered_map<Yapf3DNode::Key, Yapf3DNode, Yapf3DNodeKeyHash> &nodes,
	std::vector<Yapf3DNode *> &open,
	TileIndex target_tile,
	Yapf3DResult &result);

/**
 * YAPF3D A* search implementation.
 */
Yapf3DResult Yapf3DFindPath(const Train *, TileIndex entry_tile, Trackdir entry_td, TileIndex target_tile)
{
	Yapf3DResult result;

	/* Find the portal slice at the entry tile. */
	SliceID portal_sid = _multilayer_map.FindSliceByKind(entry_tile, SliceKind::Portal);
	if (portal_sid == INVALID_SLICE_ID) {
		/* No portal — can't enter underground. */
		return result;
	}

	const TileSlice &portal_slice = _multilayer_map.GetSlice(portal_sid);
	int16_t depth = portal_slice.span.z_bot;

	/* Find the underground Track slice connected to this portal. */
	DiagDirection exitdir = TrackdirToExitdir(entry_td);
	TileIndexDiffC diff = TileIndexDiffCByDiagDir(exitdir);
	int nx = TileX(entry_tile) + diff.x;
	int ny = TileY(entry_tile) + diff.y;

	if (nx < 0 || ny < 0 || nx >= (int)Map::SizeX() || ny >= (int)Map::SizeY()) return result;

	TileIndex first_ug_tile = TileXY(nx, ny);
	SliceID first_sid = _multilayer_map.FindSliceAt(first_ug_tile, depth);
	if (first_sid == INVALID_SLICE_ID) return result;

	/* Initialize the open set with the first underground tile. */
	std::unordered_map<Yapf3DNode::Key, Yapf3DNode, Yapf3DNodeKeyHash> nodes;
	std::vector<Yapf3DNode *> open; /* Simple open list (not a real priority queue for MVP). */

	TileRef start_ref{first_ug_tile, first_sid};
	DiagDirection move_dir = exitdir;

	/* Check which tracks are available on the first underground tile. */
	const TileSlice &first_slice = _multilayer_map.GetSlice(first_sid);
	if (first_slice.kind != SliceKind::Track && first_slice.kind != SliceKind::StationTile) return result;

	TrackBits first_tracks = (first_slice.kind == SliceKind::Track)
		? static_cast<TrackBits>(first_slice.track.tracks)
		: static_cast<TrackBits>(first_slice.station.tracks);
	TrackBits reachable = DiagdirReachesTracks(move_dir) & first_tracks;
	if (reachable == TRACK_BIT_NONE) return result;

	/* Create start nodes for each reachable trackdir. */
	for (Track t = TRACK_BEGIN; t < TRACK_END; t++) {
		if (!(reachable & TrackToTrackBits(t))) continue;
		Trackdir start_td = TrackEnterdirToTrackdir(t, move_dir);

		Yapf3DNode::Key key{start_ref, start_td};
		auto &node = nodes[key];
		node.ref = start_ref;
		node.td = start_td;
		node.cost = YAPF3D_TILE_COST;
		node.estimate = DistanceManhattan(first_ug_tile, target_tile) * YAPF3D_TILE_COST;
		node.parent = nullptr;
		open.push_back(&node);
	}

	RunYapf3DSearch(nodes, open, target_tile, result);
	return result;
}

/**
 * Shared A* loop used by both Yapf3DFindPath and Yapf3DFindPathFromRef.
 */
static void RunYapf3DSearch(
	std::unordered_map<Yapf3DNode::Key, Yapf3DNode, Yapf3DNodeKeyHash> &nodes,
	std::vector<Yapf3DNode *> &open,
	TileIndex target_tile,
	Yapf3DResult &result)
{
	int iterations = 0;
	Yapf3DNode *best_portal_node = nullptr;
	Yapf3DNode *best_target_node = nullptr;

	while (!open.empty() && iterations < YAPF3D_MAX_NODES) {
		iterations++;

		auto best_it = open.begin();
		for (auto it = open.begin(); it != open.end(); ++it) {
			if ((*it)->total() < (*best_it)->total()) best_it = it;
		}

		Yapf3DNode *current = *best_it;
		open.erase(best_it);
		current->closed = true;

		if (target_tile != INVALID_TILE && current->ref.tile == target_tile) {
			if (best_target_node == nullptr || current->cost < best_target_node->cost) {
				best_target_node = current;
			}
			continue;
		}

		const TileSlice &cur_slice = _multilayer_map.GetSlice(current->ref.slice);
		if (target_tile == INVALID_TILE && cur_slice.kind == SliceKind::Portal) {
			if (best_portal_node == nullptr || current->cost < best_portal_node->cost) {
				best_portal_node = current;
			}
			continue;
		}

		std::vector<std::pair<TileRef, Trackdir>> neighbors;
		Yapf3DEnumerateNeighbors(current->ref, current->td, neighbors);

		for (auto &[nref, ntd] : neighbors) {
			Yapf3DNode::Key nkey{nref, ntd};

			auto it = nodes.find(nkey);
			if (it != nodes.end() && it->second.closed) continue;

			int new_cost = current->cost + YAPF3D_TILE_COST;
			if (TrackdirToTrack(current->td) != TrackdirToTrack(ntd)) {
				new_cost += YAPF3D_CURVE_PENALTY;
			}

			int estimate = (target_tile != INVALID_TILE)
				? DistanceManhattan(nref.tile, target_tile) * YAPF3D_TILE_COST
				: 0;

			if (it == nodes.end()) {
				auto &node = nodes[nkey];
				node.ref = nref;
				node.td = ntd;
				node.cost = new_cost;
				node.estimate = estimate;
				node.parent = current;
				open.push_back(&node);
			} else if (new_cost < it->second.cost) {
				it->second.cost = new_cost;
				it->second.parent = current;
				if (std::find(open.begin(), open.end(), &it->second) == open.end()) {
					open.push_back(&it->second);
				}
			}
		}
	}

	Yapf3DNode *best_node = best_target_node != nullptr ? best_target_node : best_portal_node;
	if (best_node != nullptr) {
		result.path_found = true;
		result.exit_portal = best_node->ref;
		result.total_cost = best_node->cost;

		Yapf3DNode *n = best_node;
		while (n->parent != nullptr) {
			result.path.emplace_back(n->ref, n->td);
			if (n->parent->parent == nullptr) result.first_trackdir = n->parent->td;
			n = n->parent;
		}
		result.path.emplace_back(n->ref, n->td);
		if (n->parent == nullptr) result.first_trackdir = n->td;
	}
}

/**
 * Find a path from an already-underground position (e.g. stopped at a signal).
 */
Yapf3DResult Yapf3DFindPathFromRef(TileRef start_ref, Trackdir start_td, TileIndex target_tile)
{
	Yapf3DResult result;

	if (!start_ref.IsValid()) return result;

	const TileSlice &start_slice = _multilayer_map.GetSlice(start_ref.slice);
	if (start_slice.kind != SliceKind::Track && start_slice.kind != SliceKind::StationTile) return result;

	std::unordered_map<Yapf3DNode::Key, Yapf3DNode, Yapf3DNodeKeyHash> nodes;
	std::vector<Yapf3DNode *> open;

	Yapf3DNode::Key key{start_ref, start_td};
	auto &node = nodes[key];
	node.ref = start_ref;
	node.td = start_td;
	node.cost = 0;
	node.estimate = (target_tile != INVALID_TILE)
		? DistanceManhattan(start_ref.tile, target_tile) * YAPF3D_TILE_COST
		: 0;
	node.parent = nullptr;
	open.push_back(&node);

	RunYapf3DSearch(nodes, open, target_tile, result);
	return result;
}

/**
 * Reserve underground tracks along a found path.
 * Path is stored in reverse order (exit→start), so we iterate backwards.
 * Reserves all Track/StationTile slices, stops at first PBS signal after start.
 */
bool Yapf3DReservePath(const Yapf3DResult &result)
{
	if (!result.path_found || result.path.empty()) return false;

	/* Path is in reverse order: [exit_portal, ..., start]. Walk from start to exit. */
	bool first = true;
	for (int i = static_cast<int>(result.path.size()) - 1; i >= 0; i--) {
		const auto &[ref, td] = result.path[i];
		if (!ref.IsValid()) continue;

		const TileSlice &slice = _multilayer_map.GetSlice(ref.slice);
		Track reserved_track = TrackdirToTrack(td);

		if (slice.kind == SliceKind::Track) {
			if (!TryReserveUndergroundTrack(ref, reserved_track)) {
				/* Conflict — can't reserve. Unreserve what we did so far. */
				for (int j = static_cast<int>(result.path.size()) - 1; j > i; j--) {
					const auto &[undo_ref, undo_td] = result.path[j];
					if (!undo_ref.IsValid()) continue;
					UnreserveUndergroundTrack(undo_ref, TrackdirToTrack(undo_td));
				}
				return false;
			}

			/* After the first tile, check for PBS signal — stop reserving here. */
			if (!first && slice.track.signal_mask != 0) {
				break; /* Safe waiting position found. */
			}
		} else if (slice.kind == SliceKind::StationTile) {
			TryReserveUndergroundTrack(ref, reserved_track);
		}
		/* Skip Portal slices — they don't have tracks to reserve. */

		first = false;
	}

	return true;
}
