/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_complex.h StationComplex: underground stations with surface exits. */

#ifndef STATION_COMPLEX_H
#define STATION_COMPLEX_H

#include "multilayer_types.h"
#include <vector>

/**
 * Role of a node within a StationComplex.
 */
enum class StationRole : uint8_t {
	Platform,    ///< Underground platform where trains stop.
	ExitSurface, ///< Surface exit providing passenger catchment.
	PortalTrack, ///< Track portal connecting to the station.
};

/**
 * A node in a StationComplex — either a platform, surface exit, or portal.
 */
struct StationComplexNode {
	TileRef ref;          ///< Location in the multi-layer map.
	StationRole role;     ///< What this node does.
	uint16_t station_id;  ///< Index of the parent Station in the station pool.
	int16_t depth;        ///< Z level of this node.
};

/**
 * A StationComplex extends a normal Station with multi-level structure.
 *
 * It wraps an existing Station (by station_id) and adds:
 * - Underground platforms (trains stop here)
 * - Surface exits (provide catchment area for passengers)
 * - Portal tracks (connect surface rail to underground platforms)
 *
 * Passengers are generated at the station only if at least one
 * ExitSurface node exists. The catchment area is the union of
 * all ExitSurface tiles' individual catchments.
 *
 * From the link graph's perspective, a StationComplex is just a
 * normal station — no new cargo distribution logic needed.
 */
struct StationComplex {
	uint16_t station_id = 0;                 ///< Station pool index.
	std::vector<StationComplexNode> nodes;   ///< All nodes in this complex.

	/** Check if this complex has any surface exits (for catchment). */
	bool HasSurfaceExit() const
	{
		for (const auto &n : this->nodes) {
			if (n.role == StationRole::ExitSurface) return true;
		}
		return false;
	}

	/** Get all platform nodes. */
	std::vector<const StationComplexNode *> GetPlatforms() const
	{
		std::vector<const StationComplexNode *> result;
		for (const auto &n : this->nodes) {
			if (n.role == StationRole::Platform) result.push_back(&n);
		}
		return result;
	}

	/** Get all surface exit nodes. */
	std::vector<const StationComplexNode *> GetExits() const
	{
		std::vector<const StationComplexNode *> result;
		for (const auto &n : this->nodes) {
			if (n.role == StationRole::ExitSurface) result.push_back(&n);
		}
		return result;
	}

	/** Get the minimum depth of any platform. */
	int16_t GetDeepestLevel() const
	{
		int16_t deepest = 0;
		for (const auto &n : this->nodes) {
			if (n.role == StationRole::Platform && n.depth < deepest) {
				deepest = n.depth;
			}
		}
		return deepest;
	}
};

/**
 * Global registry of StationComplex instances.
 * Keyed by station_id. Only stations with underground components
 * have entries here; normal surface-only stations are not tracked.
 */
#include <unordered_map>

extern std::unordered_map<uint16_t, StationComplex> _station_complexes;

/** Find or create a StationComplex for a given station ID. */
StationComplex &GetOrCreateStationComplex(uint16_t station_id);

/** Remove a StationComplex. */
void RemoveStationComplex(uint16_t station_id);

#endif /* STATION_COMPLEX_H */
