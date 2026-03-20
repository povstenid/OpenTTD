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
#include "../station_base.h"
#include "../station_kdtree.h"
#include "../town.h"
#include "../station_func.h"
#include "../table/strings.h"

#include "../safeguards.h"

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
	if (!IsValidMetroZ(tile, depth)) {
		IConsolePrint(CC_DEBUG, "  FAIL: z check (z={} surface={} min={})", depth, GetMetroSurfaceZ(tile), GetMetroMinZ(tile));
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
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!IsValidMetroZ(tile, depth)) return CMD_ERROR;

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
 * Works like vanilla tunnel entry on sloped tiles, but also accepts flat tiles
 * when the caller provides an explicit direction.
 * @param flags    Command flags.
 * @param tile     Surface tile on a slope.
 * @param railtype Type of rail.
 * @param dir      Portal direction on flat tiles, otherwise auto-detected from slope.
 * @param depth    Underground depth the portal connects to.
 * @return The cost of the operation or an error.
 */
CommandCost CmdBuildPortal(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir, int16_t depth)
{
	if (railtype >= RAILTYPE_END) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!IsValidMetroZ(tile, depth)) return CMD_ERROR;

	/* Prefer the slope direction on inclined tiles. Flat tiles use the caller-supplied direction. */
	Slope slope = GetTileSlope(tile);
	DiagDirection slope_dir = GetInclinedSlopeDirection(slope);
	if (slope_dir != INVALID_DIAGDIR) {
		dir = slope_dir;
	} else if (slope != SLOPE_FLAT || dir >= DIAGDIR_END) {
		return CMD_ERROR;
	}

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
				/* Try the perpendicular track as fallback. */
				portal_track = (portal_track == TRACK_X) ? TRACK_Y : TRACK_X;
				Command<Commands::BuildRail>::Do(DoCommandFlags{DoCommandFlag::Execute}, tile, railtype, portal_track, false);
			}
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
 * @param station_id Station to attach to (0 = create new).
 * @param axis       Platform direction (AXIS_X or AXIS_Y).
 * @return Cost or error.
 */
CommandCost CmdBuildUndergroundStation(DoCommandFlags flags, TileIndex tile, RailType railtype, int16_t depth, uint16_t station_id, Axis axis)
{
	if (railtype >= RAILTYPE_END) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!IsValidMetroZ(tile, depth)) return CMD_ERROR;

	/* Check for existing slice at this depth. Allow replacing Track with Station. */
	SliceID existing = _multilayer_map.FindSliceAt(tile, depth);
	if (existing != INVALID_SLICE_ID) {
		const TileSlice &ex_slice = _multilayer_map.GetSlice(existing);
		if (ex_slice.kind == SliceKind::Track) {
			/* Will replace Track with StationTile — inherit railtype if not specified. */
			if (railtype == RAILTYPE_END || railtype > RAILTYPE_END) {
				railtype = static_cast<RailType>(ex_slice.track.railtype);
			}
			if (flags.Test(DoCommandFlag::Execute)) {
				_multilayer_map.RemoveSliceFromColumn(tile, existing);
				_multilayer_map.FreeSlice(existing);
			}
		} else {
			return CMD_ERROR; /* Something else at this depth. */
		}
	}

	/* If station_id != 0, validate it exists. */
	if (station_id != 0 && !Station::IsValidID(StationID(station_id))) return CMD_ERROR;

	/* If station_id == 0, we need to create a new station. */
	if (station_id == 0 && !Station::CanAllocateItem()) return CMD_ERROR;

	TrackBits platform_tracks = (axis == AXIS_X) ? TRACK_BIT_X : TRACK_BIT_Y;

	if (flags.Test(DoCommandFlag::Execute)) {
		/* Create new Station if needed. */
		if (station_id == 0) {
			Station *st = Station::Create(tile);
			_station_kdtree.Insert(st->index);
			st->town = ClosestTownFromTile(tile, UINT_MAX);
			st->string_id = STR_SV_STNAME;
			st->owner = _current_company;
			st->rect.BeforeAddTile(tile, StationRect::ADD_FORCE);
			st->AddFacility(StationFacility::Train, tile);
			/* Set train_station to valid 1x1 area at surface tile above the platform.
			 * Not a real station tile, but prevents INVALID_TILE assertion crashes
			 * when station code iterates train_station. Surface code filters by
			 * IsTileType(Station) and will skip this tile safely. */
			st->train_station = TileArea(tile, 1, 1);
			if (Company::IsValidID(_current_company)) {
				st->town->have_ratings.Set(_current_company);
			}
			station_id = st->index.base();
			st->UpdateVirtCoord();
			IConsolePrint(CC_DEBUG, "Created new Station id={} for underground platform", station_id);
		} else {
			/* Extend existing station rect. */
			Station *st = Station::Get(StationID(station_id));
			st->rect.BeforeAddTile(tile, StationRect::ADD_FORCE);
			if (!st->facilities.Test(StationFacility::Train)) {
				st->AddFacility(StationFacility::Train, tile);
				st->train_station = TileArea(tile, 1, 1);
			} else if (st->train_station.tile == INVALID_TILE) {
				st->train_station = TileArea(tile, 1, 1);
			}
		}

		SliceID sid = _multilayer_map.AllocateSlice();
		TileSlice &s = _multilayer_map.GetSlice(sid);
		s.kind = SliceKind::StationTile;
		s.span = {static_cast<int16_t>(depth + 1), depth};
		s.owner = _current_company.base();
		s.station.station_id = station_id;
		s.station.role = 0; /* Platform. */
		s.station.tracks = static_cast<uint8_t>(platform_tracks);
		s.station.railtype = static_cast<uint8_t>(railtype);
		s.station.reserved = 0;

		if (!_multilayer_map.InsertSlice(tile, sid)) {
			_multilayer_map.FreeSlice(sid);
			return CMD_ERROR;
		}

		/* Register in StationComplex. */
		StationComplex &sc = GetOrCreateStationComplex(station_id);
		sc.station_id = station_id;
		StationComplexNode node;
		node.ref = TileRef{tile, sid};
		node.role = StationRole::Platform;
		node.station_id = station_id;
		node.depth = depth;
		sc.nodes.push_back(node);

		/* Auto-create ExitSurface on the same tile for catchment. */
		StationComplexNode exit_node;
		exit_node.ref = TileRef{tile, _multilayer_map.FindSliceByKind(tile, SliceKind::Surface)};
		exit_node.role = StationRole::ExitSurface;
		exit_node.station_id = station_id;
		exit_node.depth = 0;
		sc.nodes.push_back(exit_node);

		/* Recompute catchment now that the ExitSurface node is registered. */
		Station::Get(StationID(station_id))->RecomputeCatchment();
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
	if (!Station::IsValidID(StationID(station_id))) return CMD_ERROR;

	auto sc_it = _station_complexes.find(station_id);
	if (sc_it != _station_complexes.end()) {
		for (const auto &existing : sc_it->second.nodes) {
			if (existing.role != StationRole::ExitSurface) continue;
			if (existing.ref.tile == tile) return CMD_ERROR;
		}
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		/* Register exit in StationComplex. */
		StationComplex &sc = GetOrCreateStationComplex(station_id);
		StationComplexNode node;
		node.ref = TileRef{tile, _multilayer_map.FindSliceByKind(tile, SliceKind::Surface)};
		node.role = StationRole::ExitSurface;
		node.station_id = station_id;
		node.depth = 0;
		sc.nodes.push_back(node);

		Station *st = Station::Get(StationID(station_id));
		st->rect.BeforeAddTile(tile, StationRect::ADD_FORCE);
		st->RecomputeCatchment();
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
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!IsValidMetroZ(tile, from_depth) || !IsValidMetroZ(tile, to_depth)) return CMD_ERROR;
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

/**
 * Build a PBS signal on an underground track.
 * @param flags   Command flags.
 * @param tile    Surface tile position.
 * @param track   Track to place the signal on.
 * @param depth   Underground depth.
 * @return Cost or error.
 */
CommandCost CmdBuildUndergroundSignal(DoCommandFlags flags, TileIndex tile, Track track, int16_t depth)
{
	if (!IsValidTrack(track)) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!IsValidMetroZ(tile, depth)) return CMD_ERROR;

	SliceID sid = _multilayer_map.FindSliceAt(tile, depth);
	if (sid == INVALID_SLICE_ID) return CMD_ERROR;

	TileSlice &slice = _multilayer_map.GetSlice(sid);
	if (slice.kind != SliceKind::Track) return CMD_ERROR;

	TrackBits trackbit = TrackToTrackBits(track);
	if (!(slice.track.tracks & trackbit)) return CMD_ERROR;

	/* If signal already exists on this track — toggle (remove it). */
	if (slice.track.signal_mask & trackbit) {
		if (flags.Test(DoCommandFlag::Execute)) {
			slice.track.signal_mask &= ~trackbit;
		}
		return CommandCost(EXPENSES_CONSTRUCTION, static_cast<Money>(0));
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		slice.track.signal_mask |= trackbit;
		IConsolePrint(CC_DEBUG, "Underground signal placed: tile={} track={} depth={}", tile.base(), (int)track, depth);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, static_cast<Money>(500));
}

/**
 * Remove a PBS signal from an underground track.
 * @param flags   Command flags.
 * @param tile    Surface tile position.
 * @param track   Track to remove the signal from.
 * @param depth   Underground depth.
 * @return Cost or error.
 */
CommandCost CmdRemoveUndergroundSignal(DoCommandFlags flags, TileIndex tile, Track track, int16_t depth)
{
	if (!IsValidTrack(track)) return CMD_ERROR;
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!IsValidMetroZ(tile, depth)) return CMD_ERROR;

	SliceID sid = _multilayer_map.FindSliceAt(tile, depth);
	if (sid == INVALID_SLICE_ID) return CMD_ERROR;

	TileSlice &slice = _multilayer_map.GetSlice(sid);
	if (slice.kind != SliceKind::Track) return CMD_ERROR;

	TrackBits trackbit = TrackToTrackBits(track);
	if (!(slice.track.signal_mask & trackbit)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		slice.track.signal_mask &= ~trackbit;
	}

	return CommandCost(EXPENSES_CONSTRUCTION, static_cast<Money>(-200));
}

/**
 * Remove an underground station platform.
 * @param flags Command flags.
 * @param tile  Surface tile position.
 * @param depth Underground depth.
 * @return Cost or error.
 */
CommandCost CmdRemoveUndergroundStation(DoCommandFlags flags, TileIndex tile, int16_t depth)
{
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!IsValidMetroZ(tile, depth)) return CMD_ERROR;

	SliceID sid = _multilayer_map.FindSliceAt(tile, depth);
	if (sid == INVALID_SLICE_ID) return CMD_ERROR;

	TileSlice &slice = _multilayer_map.GetSlice(sid);
	if (slice.kind != SliceKind::StationTile) return CMD_ERROR;

	if (slice.owner != _current_company.base()) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		uint16_t station_id = slice.station.station_id;

		/* Remove from StationComplex. */
		auto sc_it = _station_complexes.find(station_id);
		if (sc_it != _station_complexes.end()) {
			auto &nodes = sc_it->second.nodes;
			std::erase_if(nodes, [&](const StationComplexNode &n) {
				return n.ref.tile == tile && n.ref.slice == sid;
			});
			/* Also remove auto-created ExitSurface for this tile. */
			std::erase_if(nodes, [&](const StationComplexNode &n) {
				return n.ref.tile == tile && n.role == StationRole::ExitSurface && n.station_id == station_id;
			});
			if (nodes.empty()) {
				RemoveStationComplex(station_id);
			}
		}

		_multilayer_map.RemoveSliceFromColumn(tile, sid);
		_multilayer_map.FreeSlice(sid);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, static_cast<Money>(-500));
}
