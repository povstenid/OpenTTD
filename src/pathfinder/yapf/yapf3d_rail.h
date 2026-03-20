/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf3d_rail.h YAPF3D: pathfinder for underground rail using TileRef. */

#ifndef YAPF3D_RAIL_H
#define YAPF3D_RAIL_H

#include "../../multilayer/multilayer_types.h"
#include "../../track_type.h"
#include "../../direction_type.h"
#include "../../vehicle_type.h"
#include "../../pbs.h"
#include <vector>
#include <unordered_map>

struct Train;

/**
 * Node in the YAPF3D search graph.
 * Uses TileRef instead of TileIndex to address underground slices.
 */
struct Yapf3DNode {
	TileRef ref;           ///< Position in multi-layer map.
	Trackdir td;           ///< Track direction at this node.
	int cost = 0;          ///< Total cost from start to this node.
	int estimate = 0;      ///< Estimated remaining cost (heuristic).
	int total() const { return cost + estimate; }

	Yapf3DNode *parent = nullptr; ///< Parent in search tree.
	bool closed = false;          ///< Whether this node is in the closed set.

	/** Key for deduplication: tile + slice + trackdir. */
	struct Key {
		TileRef ref;
		Trackdir td;
		bool operator==(const Key &other) const = default;
	};

	Key GetKey() const { return {ref, td}; }
};

/** Hash for Yapf3DNode::Key. */
struct Yapf3DNodeKeyHash {
	size_t operator()(const Yapf3DNode::Key &k) const
	{
		TileRefHash th;
		size_t h = th(k.ref);
		h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.td)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

/**
 * Result of a YAPF3D pathfind.
 */
struct Yapf3DResult {
	bool path_found = false;    ///< Whether a valid path was found.
	Trackdir first_trackdir;    ///< First trackdir to take.
	TileRef exit_portal;        ///< Portal where train exits underground (if applicable).
	int total_cost = 0;         ///< Total path cost.
	std::vector<std::pair<TileRef, Trackdir>> path;  ///< Full path with chosen trackdirs.
};

/**
 * YAPF3D: A* pathfinder for underground rail.
 *
 * This pathfinder operates on TileRef (tile + slice) instead of plain TileIndex.
 * It finds paths through underground Track slices and terminates at Portal slices.
 *
 * Usage:
 *   1. Surface YAPF hits a portal tile
 *   2. Surface YAPF calls Yapf3DFindPath() with the portal entry
 *   3. YAPF3D searches through underground slices
 *   4. YAPF3D returns the exit portal and first trackdir
 *   5. Surface YAPF continues from exit portal on surface
 */

/**
 * Find a path through underground from an entry portal.
 * @param v           The train vehicle.
 * @param entry_tile  Surface tile where the portal is.
 * @param entry_td    Trackdir entering the portal.
 * @param target_tile Desired destination tile (on surface, for heuristic).
 * @return Yapf3DResult with path info.
 */
Yapf3DResult Yapf3DFindPath(const Train *, TileIndex entry_tile, Trackdir entry_td, TileIndex target_tile);

/**
 * Find a path through underground starting from an already-underground position.
 * Used for re-pathfinding after a PBS signal turns green.
 * @param start_ref   Current underground tile + slice.
 * @param start_td    Current trackdir.
 * @param target_tile Desired destination tile (surface, for heuristic).
 * @return Yapf3DResult with path info.
 */
Yapf3DResult Yapf3DFindPathFromRef(TileRef start_ref, Trackdir start_td, TileIndex target_tile);

/**
 * Enumerate underground track neighbors of a TileRef.
 * Returns TrackdirBits reachable from the given position at the same depth.
 * @param ref   Current position.
 * @param td    Current trackdir.
 * @param out_neighbors  Output: vector of (TileRef, Trackdir) pairs.
 */
void Yapf3DEnumerateNeighbors(const TileRef &ref, Trackdir td,
                               std::vector<std::pair<TileRef, Trackdir>> &out_neighbors);

/**
 * Reserve underground tracks along a found path.
 * Reserves from the start until the first PBS signal (safe waiting position)
 * or until the end of the path.
 * @param result The path found by Yapf3DFindPath.
 * @return True if reservation succeeded for all tiles up to the first signal.
 */
bool Yapf3DReservePath(const Yapf3DResult &result);

#endif /* YAPF3D_RAIL_H */
