/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_cmd.cpp Commands for building and removing underground rail tracks. */

#include "../stdafx.h"
#include "underground_cmd.h"
#include "multilayer_map.h"
#include "station_complex.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../rail.h"
#include "../track_func.h"
#include "../tile_map.h"
#include "../slope_func.h"
#include "../tunnel_map.h"
#include "../rail_cmd.h"
#include "../debug.h"
#include "../console_func.h"
#include "portal_registry.h"

#include "../safeguards.h"

/** Maximum allowed underground depth (negative Z). */
static constexpr int16_t MAX_UNDERGROUND_DEPTH = -10;

/** Cost multiplier for underground construction (more expensive than surface). */
static constexpr int UNDERGROUND_COST_MULTIPLIER = 4;

/**
 * Build a single underground rail track at a given depth.
 * @param flags   Command flags.
 * @param tile    Surface tile position (x, y).
 * @param railtype Type of rail to build.
 * @param track   Track orientation.
 * @param depth   Z depth (negative, e.g. -1 = first underground level).
 * @return The cost of the operation or an error.
 */
CommandCost CmdBuildUndergroundRail(DoCommandFlags flags, TileIndex tile, RailType railtype, Track track, int16_t depth)
{
	IConsolePrint(CC_DEBUG, "CmdBuildUndergroundRail: tile={} rt={} track={} depth={} flags_exec={}",
		tile.base(), (int)railtype, (int)track, depth, flags.Test(DoCommandFlag::Execute));

	/* Validate parameters. */
	if (railtype >= RAILTYPE_END) {
		IConsolePrint(CC_DEBUG, "  FAIL: railtype >= RAILTYPE_END ({})", (int)railtype);
		return CMD_ERROR;
	}
	/* Note: we skip ValParamRailType which checks company availability.
	 * Underground rail uses any valid railtype regardless of company research. */
	if (!IsValidTrack(track)) {
		IConsolePrint(CC_DEBUG, "  FAIL: IsValidTrack");
		return CMD_ERROR;
	}
	if (depth >= 0 || depth < MAX_UNDERGROUND_DEPTH) {
		IConsolePrint(CC_DEBUG, "  FAIL: depth check (depth={} MAX={})", depth, MAX_UNDERGROUND_DEPTH);
		return CMD_ERROR;
	}

	/* Check if tile is within map bounds. */
	if (!IsValidTile(tile)) {
		IConsolePrint(CC_DEBUG, "  FAIL: IsValidTile");
		return CMD_ERROR;
	}

	IConsolePrint(CC_DEBUG, "  Validation passed, checking existing slices...");

	/* Underground tiles can be built under any surface tile. */

	TrackBits trackbit = TrackToTrackBits(track);

	/* Calculate the ZSpan for this depth level. */
	int16_t z_bot = depth;
	int16_t z_top = depth + 1;

	/* Check if there's already a Track slice at this depth. */
	SliceID existing = _multilayer_map.FindSliceAt(tile, depth);
	IConsolePrint(CC_DEBUG, "  FindSliceAt result: {}", existing);
	if (existing != INVALID_SLICE_ID) {
		TileSlice &slice = _multilayer_map.GetSlice(existing);
		if (slice.kind != SliceKind::Track) {
			return CMD_ERROR; /* Something else is here. */
		}

		/* Check if the track bit is already set. */
		if (slice.track.tracks & trackbit) {
			return CMD_ERROR; /* Track already exists. */
		}

		/* Can add more track bits to existing slice. */
		if (slice.track.railtype != static_cast<uint8_t>(railtype)) {
			return CMD_ERROR; /* Rail type mismatch. */
		}

		if (flags.Test(DoCommandFlag::Execute)) {
			slice.track.tracks |= trackbit;
		}
	} else {
		/* Need to create a new Track slice. */
		IConsolePrint(CC_DEBUG, "  No existing slice, creating new. Execute={}", flags.Test(DoCommandFlag::Execute));
		if (flags.Test(DoCommandFlag::Execute)) {
			SliceID new_id = _multilayer_map.AllocateSlice();
			IConsolePrint(CC_DEBUG, "  Allocated slice id={}", new_id);
			TileSlice &slice = _multilayer_map.GetSlice(new_id);
			slice.kind = SliceKind::Track;
			slice.span = {z_top, z_bot};
			slice.owner = _current_company.base();
			slice.track.railtype = static_cast<uint8_t>(railtype);
			slice.track.tracks = trackbit;
			slice.track.signal_mask = 0;
			slice.track.reserved = 0;

			if (!_multilayer_map.InsertSlice(tile, new_id)) {
				IConsolePrint(CC_DEBUG, "  FAIL: InsertSlice ZSpan conflict");
				_multilayer_map.FreeSlice(new_id);
				return CMD_ERROR;
			}
			IConsolePrint(CC_DEBUG, "  Slice inserted successfully!");
		}
	}

	/* Calculate cost: base rail cost * underground multiplier. */
	Money cost = RailBuildCost(railtype) * UNDERGROUND_COST_MULTIPLIER;
	IConsolePrint(CC_DEBUG, "  SUCCESS! Cost={}", cost);
	if (flags.Test(DoCommandFlag::Execute)) RecomputePortalConnections();
	return CommandCost(EXPENSES_CONSTRUCTION, cost);
}

/**
 * Remove a single underground rail track at a given depth.
 * @param flags   Command flags.
 * @param tile    Surface tile position (x, y).
 * @param track   Track orientation to remove.
 * @param depth   Z depth (negative).
 * @return The cost of the operation or an error.
 */
CommandCost CmdRemoveUndergroundRail(DoCommandFlags flags, TileIndex tile, Track track, int16_t depth)
{
	if (!IsValidTrack(track)) return CMD_ERROR;
	if (depth >= 0 || depth < MAX_UNDERGROUND_DEPTH) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;

	TrackBits trackbit = TrackToTrackBits(track);

	/* Find the Track slice at this depth. */
	SliceID sid = _multilayer_map.FindSliceAt(tile, depth);
	if (sid == INVALID_SLICE_ID) return CMD_ERROR;

	TileSlice &slice = _multilayer_map.GetSlice(sid);
	if (slice.kind != SliceKind::Track) return CMD_ERROR;

	/* Check if this track bit exists. */
	if (!(slice.track.tracks & trackbit)) return CMD_ERROR;

	/* Check ownership. */
	if (slice.owner != (_current_company.base())) {
		return CMD_ERROR;
	}

	/* Save railtype before potentially freeing the slice. */
	RailType rt = static_cast<RailType>(slice.track.railtype);

	if (flags.Test(DoCommandFlag::Execute)) {
		slice.track.tracks &= ~trackbit;

		/* If no tracks remain, remove the slice entirely. */
		if (slice.track.tracks == 0) {
			_multilayer_map.RemoveSliceFromColumn(tile, sid);
			_multilayer_map.FreeSlice(sid);
		}
	}

	if (flags.Test(DoCommandFlag::Execute)) RecomputePortalConnections();
	return CommandCost(EXPENSES_CONSTRUCTION, -RailBuildCost(rt));
}

/**
 * Build a portal (entry/exit between surface and underground).
 * Works like vanilla tunnel entry: requires inclined slope, direction auto-detected.
 * @param flags    Command flags.
 * @param tile     Surface tile on a slope.
 * @param railtype Type of rail.
 * @param dir      Ignored (auto-detected from slope). Kept for API compatibility.
 * @param depth    Underground depth the portal connects to.
 * @return The cost of the operation or an error.
 */
CommandCost CmdBuildPortal(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir, int16_t depth)
{
	if (railtype >= RAILTYPE_END) return CMD_ERROR;
	if (depth >= 0 || depth < MAX_UNDERGROUND_DEPTH) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;

	/* Portal requires inclined slope — like vanilla tunnel. */
	Slope slope = GetTileSlope(tile);
	DiagDirection slope_dir = GetInclinedSlopeDirection(slope);
	if (slope_dir == INVALID_DIAGDIR) {
		IConsolePrint(CC_DEBUG, "CmdBuildPortal: not an inclined slope (slope={})", (int)slope);
		return CMD_ERROR;
	}

	/* Use slope direction as portal direction. */
	dir = slope_dir;

	IConsolePrint(CC_DEBUG, "CmdBuildPortal: tile={} slope={} dir={} depth={}", tile.base(), (int)slope, (int)dir, depth);

	/* Check if there's already a portal at this tile. */
	SliceID existing = _multilayer_map.FindSliceByKind(tile, SliceKind::Portal);
	if (existing != INVALID_SLICE_ID) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		/* Create Portal slice in multilayer map. */
		SliceID portal_sid = _multilayer_map.AllocateSlice();
		TileSlice &ps = _multilayer_map.GetSlice(portal_sid);
		ps.kind = SliceKind::Portal;
		ps.span = {static_cast<int16_t>(depth + 1), depth};
		ps.owner = _current_company.base();
		ps.portal.direction = static_cast<uint8_t>(dir);
		ps.portal.target_slice = INVALID_SLICE_ID;

		if (!_multilayer_map.InsertSlice(tile, portal_sid)) {
			_multilayer_map.FreeSlice(portal_sid);
			return CMD_ERROR;
		}

		/* Make surface tile passable: build rail track matching the slope direction.
		 * On slopes, only certain tracks are valid (X track on NE/SW slope, Y on NW/SE). */
		Track portal_track = (DiagDirToAxis(dir) == AXIS_X) ? TRACK_X : TRACK_Y;
		auto rail_result = Command<Commands::BuildRail>::Do(DoCommandFlags{DoCommandFlag::Execute}, tile, railtype, portal_track, false);
		if (!rail_result.Succeeded()) {
			IConsolePrint(CC_DEBUG, "Portal: failed to build surface rail (track={}), trying other", (int)portal_track);
			/* Try the perpendicular track as fallback. */
			portal_track = (portal_track == TRACK_X) ? TRACK_Y : TRACK_X;
			Command<Commands::BuildRail>::Do(DoCommandFlags{DoCommandFlag::Execute}, tile, railtype, portal_track, false);
		}

		IConsolePrint(CC_DEBUG, "Portal built: tile={} dir={} depth={} track={}", tile.base(), (int)dir, depth, (int)portal_track);
	}

	if (flags.Test(DoCommandFlag::Execute)) RecomputePortalConnections();
	return CommandCost(EXPENSES_CONSTRUCTION, RailBuildCost(railtype) * UNDERGROUND_COST_MULTIPLIER * 2);
}

/**
 * Build an underground station platform at a given depth.
 * @param flags      Command flags.
 * @param tile       Surface tile position.
 * @param railtype   Rail type for the platform.
 * @param depth      Underground depth.
 * @param station_id Station to attach to (0 = create new via StationComplex).
 * @return Cost or error.
 */
CommandCost CmdBuildUndergroundStation(DoCommandFlags flags, TileIndex tile, RailType railtype, int16_t depth, uint16_t station_id)
{
	if (!ValParamRailType(railtype)) return CMD_ERROR;
	if (depth >= 0 || depth < MAX_UNDERGROUND_DEPTH) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;

	/* Check for existing slice at this depth. */
	SliceID existing = _multilayer_map.FindSliceAt(tile, depth);
	if (existing != INVALID_SLICE_ID) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		SliceID sid = _multilayer_map.AllocateSlice();
		TileSlice &s = _multilayer_map.GetSlice(sid);
		s.kind = SliceKind::StationTile;
		s.span = {static_cast<int16_t>(depth + 1), depth};
		s.owner = _current_company.base();
		s.station.station_id = station_id;
		s.station.role = 0; /* Platform. */

		if (!_multilayer_map.InsertSlice(tile, sid)) {
			_multilayer_map.FreeSlice(sid);
			return CMD_ERROR;
		}

		/* Register in StationComplex. */
		StationComplex &sc = GetOrCreateStationComplex(station_id);
		StationComplexNode node;
		node.ref = TileRef{tile, sid};
		node.role = StationRole::Platform;
		node.station_id = station_id;
		node.depth = depth;
		sc.nodes.push_back(node);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, RailBuildCost(railtype) * UNDERGROUND_COST_MULTIPLIER * 3);
}

/**
 * Build a surface exit for an underground station complex.
 * @param flags      Command flags.
 * @param tile       Surface tile for the exit.
 * @param station_id Station to attach to.
 * @return Cost or error.
 */
CommandCost CmdBuildExitSurface(DoCommandFlags flags, TileIndex tile, uint16_t station_id)
{
	if (!IsValidTile(tile)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		/* Register exit in StationComplex. */
		StationComplex &sc = GetOrCreateStationComplex(station_id);
		StationComplexNode node;
		node.ref = TileRef{tile, _multilayer_map.FindSliceByKind(tile, SliceKind::Surface)};
		node.role = StationRole::ExitSurface;
		node.station_id = station_id;
		node.depth = 0;
		sc.nodes.push_back(node);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, static_cast<Money>(1000)); /* Fixed cost for exit. */
}

/**
 * Build a ramp connecting two underground depth levels.
 * @param flags      Command flags.
 * @param tile       Surface tile position.
 * @param railtype   Rail type.
 * @param dir        Direction the ramp descends towards.
 * @param from_depth Upper depth (e.g. -1).
 * @param to_depth   Lower depth (e.g. -2). Must be from_depth - 1.
 * @return Cost or error.
 */
CommandCost CmdBuildRamp(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir, int16_t from_depth, int16_t to_depth)
{
	if (railtype >= RAILTYPE_END) return CMD_ERROR;
	if (from_depth >= 0 || to_depth >= 0) return CMD_ERROR;
	if (from_depth < MAX_UNDERGROUND_DEPTH || to_depth < MAX_UNDERGROUND_DEPTH) return CMD_ERROR;
	if (to_depth != from_depth - 1 && to_depth != from_depth + 1) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;

	/* Ramp span covers both depths. */
	int16_t z_bot = std::min(from_depth, to_depth);
	int16_t z_top = std::max(from_depth, to_depth) + 1;

	/* Check for existing slices in the ramp's ZSpan. */
	for (int16_t z = z_bot; z < z_top; z++) {
		SliceID existing = _multilayer_map.FindSliceAt(tile, z);
		if (existing != INVALID_SLICE_ID) return CMD_ERROR;
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		SliceID sid = _multilayer_map.AllocateSlice();
		TileSlice &s = _multilayer_map.GetSlice(sid);
		s.kind = SliceKind::Ramp;
		s.span = {z_top, z_bot};
		s.owner = _current_company.base();
		s.ramp.entry_depth = from_depth;
		s.ramp.exit_depth = to_depth;
		s.ramp.direction = static_cast<uint8_t>(dir);
		s.ramp.railtype = static_cast<uint8_t>(railtype);

		if (!_multilayer_map.InsertSlice(tile, sid)) {
			_multilayer_map.FreeSlice(sid);
			return CMD_ERROR;
		}
	}

	return CommandCost(EXPENSES_CONSTRUCTION, RailBuildCost(railtype) * UNDERGROUND_COST_MULTIPLIER * 3);
}
