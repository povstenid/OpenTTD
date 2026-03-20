/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_metro.hpp Native metro / underground helpers for GameScripts. */

#ifndef SCRIPT_METRO_HPP
#define SCRIPT_METRO_HPP

#include "script_company.hpp"
#include "script_order.hpp"
#include "script_rail.hpp"
#include "script_station.hpp"
#include "script_vehicle.hpp"

/**
 * Class that handles metro / underground construction helpers for GameScripts.
 * @api game
 */
class ScriptMetro : public ScriptObject {
public:
	/**
	 * Underground tile kinds as exposed to scripts.
	 */
	enum TileKind {
		TK_NONE = 0,     ///< No underground content at the requested depth.
		TK_TRACK,        ///< Underground track slice.
		TK_STATION,      ///< Underground station platform slice.
		TK_PORTAL,       ///< Portal slice.
		TK_RAMP,         ///< Ramp slice.
		TK_INVALID = -1, ///< Invalid kind.
	};

	/**
	 * Diagonal directions used by portals and ramps.
	 */
	enum DiagDirection {
		DIAGDIR_NE = ::DIAGDIR_NE, ///< Northeast.
		DIAGDIR_SE = ::DIAGDIR_SE, ///< Southeast.
		DIAGDIR_SW = ::DIAGDIR_SW, ///< Southwest.
		DIAGDIR_NW = ::DIAGDIR_NW, ///< Northwest.
		DIAGDIR_INVALID = -1,      ///< Invalid direction.
	};

	/**
	 * Create a playable company for headless GS scenarios.
	 * @param company Optional preferred company slot or COMPANY_INVALID for first free slot.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return The created company id or COMPANY_INVALID on failure.
	 */
	static ScriptCompany::CompanyID CreateCompany(ScriptCompany::CompanyID company);

	/**
	 * Build a single underground rail track at an absolute underground z level.
	 * @param tile Tile above the underground slice.
	 * @param rail_track Single track bit from ScriptRail::RailTrack.
	 * @param depth Absolute z below the local surface, e.g. -1 at sea level or 7 under a height-8 mountain.
	 * @return Whether building succeeded.
	 */
	static bool BuildUndergroundRail(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth);

	/**
	 * Remove a single underground rail track at a depth.
	 */
	static bool RemoveUndergroundRail(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth);

	/**
	 * Build a surface portal into underground at an absolute z level.
	 */
	static bool BuildPortal(TileIndex tile, DiagDirection direction, SQInteger depth);

	/**
	 * Build an underground station platform.
	 * @param tile Tile above the underground platform.
	 * @param direction Platform direction.
	 * @param depth Absolute underground z level.
	 * @param station_id Station to join, or ScriptStation::STATION_NEW for a new station.
	 */
	static bool BuildUndergroundStation(TileIndex tile, ScriptRail::RailTrack direction, SQInteger depth, StationID station_id);

	/**
	 * Remove an underground station platform.
	 */
	static bool RemoveUndergroundStation(TileIndex tile, SQInteger depth);

	/**
	 * Build an extra surface exit for an underground station complex.
	 */
	static bool BuildExitSurface(TileIndex tile, StationID station_id);

	/**
	 * Build a ramp between two underground levels.
	 */
	static bool BuildRamp(TileIndex tile, DiagDirection direction, SQInteger from_depth, SQInteger to_depth);

	/**
	 * Build a PBS signal on an underground track.
	 */
	static bool BuildUndergroundSignal(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth);

	/**
	 * Remove a PBS signal from an underground track.
	 */
	static bool RemoveUndergroundSignal(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth);

	/**
	 * Append a goto-station order using a station id.
	 * This is needed for underground stations, which do not have surface Station tiles.
	 */
	static bool AppendOrderByStationID(VehicleID vehicle_id, StationID station_id, ScriptOrder::ScriptOrderFlags order_flags);

	/**
	 * Check whether a tile has any underground content.
	 */
	static bool HasUnderground(TileIndex tile);

	/**
	 * Check whether a tile has underground content at the given depth.
	 */
	static bool IsUndergroundTile(TileIndex tile, SQInteger depth);

	/**
	 * Check whether a tile contains a portal.
	 */
	static bool IsPortalTile(TileIndex tile);

	/**
	 * Check whether a tile has traversable underground rail at the given depth.
	 */
	static bool HasUndergroundRail(TileIndex tile, SQInteger depth);

	/**
	 * Get the kind of underground content at the given depth.
	 */
	static TileKind GetUndergroundTileKind(TileIndex tile, SQInteger depth);

	/**
	 * Get the traversable underground track bits at the given depth.
	 */
	static SQInteger GetUndergroundTrackBits(TileIndex tile, SQInteger depth);

	/**
	 * Check whether an underground track has a PBS signal.
	 */
	static bool HasUndergroundSignal(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth);

	/**
	 * Get the deepest underground level on this tile, or 0 when there is none.
	 */
	static SQInteger GetMetroDepth(TileIndex tile);

	/**
	 * Get the station id of an underground platform tile.
	 */
	static StationID GetUndergroundStationID(TileIndex tile, SQInteger depth);

	/**
	 * Check whether a station complex has at least one surface exit.
	 */
	static bool HasSurfaceExit(StationID station_id);

	/**
	 * Count underground platform nodes in a station complex.
	 */
	static SQInteger GetStationComplexPlatformCount(StationID station_id);

	/**
	 * Count surface exits in a station complex.
	 */
	static SQInteger GetStationComplexExitCount(StationID station_id);

	/**
	 * Get the deepest platform level of a station complex.
	 */
	static SQInteger GetStationComplexDeepestLevel(StationID station_id);

	/**
	 * Check whether two portals are connected at a depth.
	 */
	static bool HasPortalConnection(TileIndex entry_tile, TileIndex exit_tile, SQInteger depth);

	/**
	 * Check whether a train is currently underground.
	 */
	static bool IsVehicleUnderground(VehicleID vehicle_id);

	/**
	 * Get the current underground depth of a train, or 0 when on surface.
	 */
	static SQInteger GetVehicleUndergroundDepth(VehicleID vehicle_id);

	/**
	 * Get the tile currently occupied by the underground train state.
	 */
	static TileIndex GetVehicleUndergroundTile(VehicleID vehicle_id);
};

#endif /* SCRIPT_METRO_HPP */
