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
#include "../track_type.h"
#include "../rail_type.h"
#include "../direction_type.h"

CommandCost CmdBuildUndergroundRail(DoCommandFlags flags, TileIndex tile, RailType railtype, Track track, int16_t depth);
CommandCost CmdRemoveUndergroundRail(DoCommandFlags flags, TileIndex tile, Track track, int16_t depth);
CommandCost CmdBuildPortal(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir, int16_t depth);
CommandCost CmdBuildUndergroundStation(DoCommandFlags flags, TileIndex tile, RailType railtype, int16_t depth, uint16_t station_id);
CommandCost CmdBuildExitSurface(DoCommandFlags flags, TileIndex tile, uint16_t station_id);
CommandCost CmdBuildRamp(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir, int16_t from_depth, int16_t to_depth);

DEF_CMD_TRAIT(Commands::BuildUndergroundRail, CmdBuildUndergroundRail, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::RemoveUndergroundRail, CmdRemoveUndergroundRail, CommandFlag::Auto, CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildPortal, CmdBuildPortal, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildUndergroundStation, CmdBuildUndergroundStation, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildExitSurface, CmdBuildExitSurface, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildRamp, CmdBuildRamp, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)

#endif /* UNDERGROUND_CMD_H */
