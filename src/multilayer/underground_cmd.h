/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_cmd.h Command definitions for underground rail construction. */

#ifndef UNDERGROUND_CMD_H
#define UNDERGROUND_CMD_H

#include "../command_type.h"
#include "../settings_type.h"
#include "../track_type.h"
#include "../rail_type.h"
#include "../direction_type.h"
#include "../tile_map.h"

static constexpr int16_t MIN_UNDERGROUND_Z = -3;

inline int16_t GetMetroSurfaceZ(TileIndex tile)
{
	return static_cast<int16_t>(TileHeight(tile));
}

inline int16_t GetMetroMinZ(TileIndex tile)
{
	int16_t surface_z = GetMetroSurfaceZ(tile);
	int16_t relative_floor = static_cast<int16_t>(surface_z - _settings_game.construction.max_underground_depth);
	return std::max<int16_t>(MIN_UNDERGROUND_Z, relative_floor);
}

inline bool IsValidMetroZ(TileIndex tile, int16_t z)
{
	if (!IsValidTile(tile)) return false;
	int16_t surface_z = GetMetroSurfaceZ(tile);
	return z < surface_z && z >= GetMetroMinZ(tile);
}

inline int16_t GetMetroGlobalMinZ()
{
	return MIN_UNDERGROUND_Z;
}

inline int16_t GetMetroGlobalMaxZ()
{
	return static_cast<int16_t>(std::max<int>(0, _settings_game.construction.map_height_limit - 1));
}

CommandCost CmdBuildUndergroundRail(DoCommandFlags flags, TileIndex tile, RailType railtype, Track track, int16_t depth);
CommandCost CmdRemoveUndergroundRail(DoCommandFlags flags, TileIndex tile, Track track, int16_t depth);
CommandCost CmdBuildPortal(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir, int16_t depth);
CommandCost CmdBuildUndergroundStation(DoCommandFlags flags, TileIndex tile, RailType railtype, int16_t depth, uint16_t station_id, Axis axis);
CommandCost CmdBuildExitSurface(DoCommandFlags flags, TileIndex tile, uint16_t station_id);
CommandCost CmdBuildRamp(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir, int16_t from_depth, int16_t to_depth);
CommandCost CmdBuildUndergroundSignal(DoCommandFlags flags, TileIndex tile, Track track, int16_t depth);
CommandCost CmdRemoveUndergroundSignal(DoCommandFlags flags, TileIndex tile, Track track, int16_t depth);
CommandCost CmdRemoveUndergroundStation(DoCommandFlags flags, TileIndex tile, int16_t depth);

DEF_CMD_TRAIT(Commands::BuildUndergroundRail, CmdBuildUndergroundRail, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::RemoveUndergroundRail, CmdRemoveUndergroundRail, CommandFlag::Auto, CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildPortal, CmdBuildPortal, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildUndergroundStation, CmdBuildUndergroundStation, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildExitSurface, CmdBuildExitSurface, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildRamp, CmdBuildRamp, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildUndergroundSignal, CmdBuildUndergroundSignal, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::RemoveUndergroundSignal, CmdRemoveUndergroundSignal, CommandFlag::Auto, CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::RemoveUndergroundStation, CmdRemoveUndergroundStation, CommandFlag::Auto, CommandType::LandscapeConstruction)

#endif /* UNDERGROUND_CMD_H */
