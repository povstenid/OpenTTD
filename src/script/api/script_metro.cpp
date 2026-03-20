/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_metro.cpp Implementation of ScriptMetro. */

#include "../../stdafx.h"

#include "script_metro.hpp"
#include "script_companymode.hpp"
#include "script_error.hpp"
#include "../../company_base.h"
#include "../../company_cmd.h"
#include "../../command_func.h"
#include "../../direction_type.h"
#include "../../multilayer/multilayer_map.h"
#include "../../multilayer/multilayer_types.h"
#include "../../multilayer/portal_registry.h"
#include "../../multilayer/station_complex.h"
#include "../../multilayer/underground_cmd.h"
#include "../../order_base.h"
#include "../../order_cmd.h"
#include "../../station_base.h"
#include "../../track_func.h"
#include "../../train.h"
#include "../../vehicle_base.h"

#include "../../safeguards.h"

extern Company *DoStartupNewCompany(bool is_ai, CompanyID company = CompanyID::Invalid());

static bool IsValidMetroDepth(SQInteger depth)
{
	return depth >= GetMetroGlobalMinZ() && depth <= GetMetroGlobalMaxZ();
}

static bool IsValidMetroTrack(ScriptRail::RailTrack rail_track)
{
	if (rail_track == ScriptRail::RAILTRACK_INVALID || rail_track == 0) return false;
	uint bits = static_cast<uint>(rail_track);
	return (bits & ~static_cast<uint>(TRACK_BIT_ALL)) == 0 && KillFirstBit(bits) == 0;
}

static ::Track MetroScriptTrackToTrack(ScriptRail::RailTrack rail_track)
{
	return FindFirstTrack(static_cast<TrackBits>(rail_track));
}

static bool IsValidMetroDirection(ScriptMetro::DiagDirection direction)
{
	return direction >= ScriptMetro::DIAGDIR_NE && direction <= ScriptMetro::DIAGDIR_NW;
}

static ScriptMetro::TileKind MetroSliceKindToScript(SliceKind kind)
{
	switch (kind) {
		case SliceKind::Track: return ScriptMetro::TK_TRACK;
		case SliceKind::StationTile: return ScriptMetro::TK_STATION;
		case SliceKind::Portal: return ScriptMetro::TK_PORTAL;
		case SliceKind::Ramp: return ScriptMetro::TK_RAMP;
		default: return ScriptMetro::TK_NONE;
	}
}

static bool AreStationOrderFlagsValid(ScriptOrder::ScriptOrderFlags order_flags)
{
	return (order_flags & ~(ScriptOrder::OF_NON_STOP_FLAGS | ScriptOrder::OF_UNLOAD_FLAGS | ScriptOrder::OF_LOAD_FLAGS)) == 0 &&
			HasAtMostOneBit(order_flags & (ScriptOrder::OF_TRANSFER | ScriptOrder::OF_UNLOAD | ScriptOrder::OF_NO_UNLOAD)) &&
			HasAtMostOneBit(order_flags & (ScriptOrder::OF_NO_UNLOAD | ScriptOrder::OF_NO_LOAD)) &&
			HasAtMostOneBit(order_flags & (ScriptOrder::OF_FULL_LOAD | ScriptOrder::OF_NO_LOAD)) &&
			((order_flags & ScriptOrder::OF_FULL_LOAD_ANY) != (ScriptOrder::OF_FULL_LOAD_ANY & ~ScriptOrder::OF_FULL_LOAD));
}

/* static */ ScriptCompany::CompanyID ScriptMetro::CreateCompany(ScriptCompany::CompanyID company)
{
	EnforceDeityMode(ScriptCompany::COMPANY_INVALID);

	CompanyID preferred = CompanyID::Invalid();
	if (company != ScriptCompany::COMPANY_INVALID) {
		if (company < ScriptCompany::COMPANY_FIRST || company >= ScriptCompany::COMPANY_LAST) return ScriptCompany::COMPANY_INVALID;
		if (ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID) return ScriptCompany::COMPANY_INVALID;
		preferred = static_cast<CompanyID>(company);
	}

	Company *created = DoStartupNewCompany(false, preferred);
	return created == nullptr ? ScriptCompany::COMPANY_INVALID : ScriptCompany::ToScriptCompanyID(created->index);
}

/* static */ bool ScriptMetro::BuildUndergroundRail(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidMetroTrack(rail_track));
	EnforcePrecondition(false, IsValidMetroDepth(depth));
	EnforcePrecondition(false, ScriptRail::IsRailTypeAvailable(ScriptRail::GetCurrentRailType()));

	return ScriptObject::Command<Commands::BuildUndergroundRail>::Do(tile, static_cast<::RailType>(ScriptRail::GetCurrentRailType()), MetroScriptTrackToTrack(rail_track), static_cast<int16_t>(depth));
}

/* static */ bool ScriptMetro::RemoveUndergroundRail(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidMetroTrack(rail_track));
	EnforcePrecondition(false, IsValidMetroDepth(depth));

	return ScriptObject::Command<Commands::RemoveUndergroundRail>::Do(tile, MetroScriptTrackToTrack(rail_track), static_cast<int16_t>(depth));
}

/* static */ bool ScriptMetro::BuildPortal(TileIndex tile, DiagDirection direction, SQInteger depth)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidMetroDirection(direction));
	EnforcePrecondition(false, IsValidMetroDepth(depth));
	EnforcePrecondition(false, ScriptRail::IsRailTypeAvailable(ScriptRail::GetCurrentRailType()));

	return ScriptObject::Command<Commands::BuildPortal>::Do(tile, static_cast<::RailType>(ScriptRail::GetCurrentRailType()), static_cast<::DiagDirection>(direction), static_cast<int16_t>(depth));
}

/* static */ bool ScriptMetro::BuildUndergroundStation(TileIndex tile, ScriptRail::RailTrack direction, SQInteger depth, StationID station_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, direction == ScriptRail::RAILTRACK_NW_SE || direction == ScriptRail::RAILTRACK_NE_SW);
	EnforcePrecondition(false, IsValidMetroDepth(depth));
	EnforcePrecondition(false, ScriptRail::IsRailTypeAvailable(ScriptRail::GetCurrentRailType()));
	EnforcePrecondition(false, station_id == ScriptStation::STATION_NEW || ScriptStation::IsValidStation(station_id));

	uint16_t internal_station_id = 0;
	if (station_id != ScriptStation::STATION_NEW) internal_station_id = station_id.base();
	Axis axis = direction == ScriptRail::RAILTRACK_NW_SE ? AXIS_Y : AXIS_X;

	return ScriptObject::Command<Commands::BuildUndergroundStation>::Do(tile, static_cast<::RailType>(ScriptRail::GetCurrentRailType()), static_cast<int16_t>(depth), internal_station_id, axis);
}

/* static */ bool ScriptMetro::RemoveUndergroundStation(TileIndex tile, SQInteger depth)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidMetroDepth(depth));

	return ScriptObject::Command<Commands::RemoveUndergroundStation>::Do(tile, static_cast<int16_t>(depth));
}

/* static */ bool ScriptMetro::BuildExitSurface(TileIndex tile, StationID station_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ScriptStation::IsValidStation(station_id));

	return ScriptObject::Command<Commands::BuildExitSurface>::Do(tile, station_id.base());
}

/* static */ bool ScriptMetro::BuildRamp(TileIndex tile, DiagDirection direction, SQInteger from_depth, SQInteger to_depth)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidMetroDirection(direction));
	EnforcePrecondition(false, IsValidMetroDepth(from_depth));
	EnforcePrecondition(false, IsValidMetroDepth(to_depth));
	EnforcePrecondition(false, ScriptRail::IsRailTypeAvailable(ScriptRail::GetCurrentRailType()));

	return ScriptObject::Command<Commands::BuildRamp>::Do(tile, static_cast<::RailType>(ScriptRail::GetCurrentRailType()), static_cast<::DiagDirection>(direction), static_cast<int16_t>(from_depth), static_cast<int16_t>(to_depth));
}

/* static */ bool ScriptMetro::BuildUndergroundSignal(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidMetroTrack(rail_track));
	EnforcePrecondition(false, IsValidMetroDepth(depth));

	return ScriptObject::Command<Commands::BuildUndergroundSignal>::Do(tile, MetroScriptTrackToTrack(rail_track), static_cast<int16_t>(depth));
}

/* static */ bool ScriptMetro::RemoveUndergroundSignal(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidMetroTrack(rail_track));
	EnforcePrecondition(false, IsValidMetroDepth(depth));

	return ScriptObject::Command<Commands::RemoveUndergroundSignal>::Do(tile, MetroScriptTrackToTrack(rail_track), static_cast<int16_t>(depth));
}

/* static */ bool ScriptMetro::AppendOrderByStationID(VehicleID vehicle_id, StationID station_id, ScriptOrder::ScriptOrderFlags order_flags)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ScriptVehicle::IsPrimaryVehicle(vehicle_id));
	EnforcePrecondition(false, ScriptStation::IsValidStation(station_id));
	EnforcePrecondition(false, AreStationOrderFlagsValid(order_flags));

	Order order;
	order.MakeGoToStation(station_id);
	order.SetLoadType(static_cast<OrderLoadType>(GB(order_flags, 5, 3)));
	order.SetUnloadType(static_cast<OrderUnloadType>(GB(order_flags, 2, 3)));
	order.SetStopLocation(OrderStopLocation::FarEnd);
	order.SetNonStopType(static_cast<OrderNonStopFlags>(GB(order_flags, 0, 2)));

	return ScriptObject::Command<Commands::InsertOrder>::Do(0, vehicle_id, ::Vehicle::Get(vehicle_id)->GetNumManualOrders(), order);
}

/* static */ bool ScriptMetro::HasUnderground(TileIndex tile)
{
	if (!::IsValidTile(tile) || !_multilayer_map.IsAllocated()) return false;
	return _multilayer_map.GetColumn(tile).HasUnderground();
}

/* static */ bool ScriptMetro::IsUndergroundTile(TileIndex tile, SQInteger depth)
{
	if (!::IsValidTile(tile) || !IsValidMetroDepth(depth) || !_multilayer_map.IsAllocated()) return false;

	SliceID sid = _multilayer_map.FindSliceAt(tile, static_cast<int16_t>(depth));
	if (sid == INVALID_SLICE_ID) return false;

	return _multilayer_map.GetSlice(sid).kind != SliceKind::Surface;
}

/* static */ bool ScriptMetro::IsPortalTile(TileIndex tile)
{
	if (!::IsValidTile(tile) || !_multilayer_map.IsAllocated()) return false;
	return _multilayer_map.FindSliceByKind(tile, SliceKind::Portal) != INVALID_SLICE_ID;
}

/* static */ bool ScriptMetro::HasUndergroundRail(TileIndex tile, SQInteger depth)
{
	if (!::IsValidTile(tile) || !IsValidMetroDepth(depth) || !_multilayer_map.IsAllocated()) return false;

	SliceID sid = _multilayer_map.FindSliceAt(tile, static_cast<int16_t>(depth));
	if (sid == INVALID_SLICE_ID) return false;

	const TileSlice &slice = _multilayer_map.GetSlice(sid);
	return slice.kind == SliceKind::Track || slice.kind == SliceKind::StationTile || slice.kind == SliceKind::Ramp;
}

/* static */ ScriptMetro::TileKind ScriptMetro::GetUndergroundTileKind(TileIndex tile, SQInteger depth)
{
	if (!::IsValidTile(tile) || !IsValidMetroDepth(depth) || !_multilayer_map.IsAllocated()) return TK_NONE;

	SliceID sid = _multilayer_map.FindSliceAt(tile, static_cast<int16_t>(depth));
	if (sid == INVALID_SLICE_ID) return TK_NONE;

	return MetroSliceKindToScript(_multilayer_map.GetSlice(sid).kind);
}

/* static */ SQInteger ScriptMetro::GetUndergroundTrackBits(TileIndex tile, SQInteger depth)
{
	if (!::IsValidTile(tile) || !IsValidMetroDepth(depth) || !_multilayer_map.IsAllocated()) return ScriptRail::RAILTRACK_INVALID;

	SliceID sid = _multilayer_map.FindSliceAt(tile, static_cast<int16_t>(depth));
	if (sid == INVALID_SLICE_ID) return ScriptRail::RAILTRACK_INVALID;

	const TileSlice &slice = _multilayer_map.GetSlice(sid);
	if (slice.kind == SliceKind::Track) return slice.track.tracks;
	if (slice.kind == SliceKind::StationTile) return slice.station.tracks;
	return 0;
}

/* static */ bool ScriptMetro::HasUndergroundSignal(TileIndex tile, ScriptRail::RailTrack rail_track, SQInteger depth)
{
	if (!::IsValidTile(tile) || !IsValidMetroTrack(rail_track) || !IsValidMetroDepth(depth) || !_multilayer_map.IsAllocated()) return false;

	SliceID sid = _multilayer_map.FindSliceAt(tile, static_cast<int16_t>(depth));
	if (sid == INVALID_SLICE_ID) return false;

	const TileSlice &slice = _multilayer_map.GetSlice(sid);
	TrackBits bits = static_cast<TrackBits>(rail_track);
	return slice.kind == SliceKind::Track && (slice.track.signal_mask & bits) != 0;
}

/* static */ SQInteger ScriptMetro::GetMetroDepth(TileIndex tile)
{
	if (!::IsValidTile(tile) || !_multilayer_map.IsAllocated()) return 0;

	int16_t deepest = INT16_MAX;
	for (SliceID sid : _multilayer_map.GetColumn(tile).slices) {
		const TileSlice &slice = _multilayer_map.GetSlice(sid);
		if (slice.kind != SliceKind::Surface) deepest = std::min<int16_t>(deepest, slice.span.z_bot);
	}
	return deepest == INT16_MAX ? 0 : deepest;
}

/* static */ StationID ScriptMetro::GetUndergroundStationID(TileIndex tile, SQInteger depth)
{
	if (!::IsValidTile(tile) || !IsValidMetroDepth(depth) || !_multilayer_map.IsAllocated()) return StationID::Invalid();

	SliceID sid = _multilayer_map.FindSliceAt(tile, static_cast<int16_t>(depth));
	if (sid == INVALID_SLICE_ID) return StationID::Invalid();

	const TileSlice &slice = _multilayer_map.GetSlice(sid);
	return slice.kind == SliceKind::StationTile ? StationID(slice.station.station_id) : StationID::Invalid();
}

/* static */ bool ScriptMetro::HasSurfaceExit(StationID station_id)
{
	if (!ScriptStation::IsValidStation(station_id)) return false;

	auto it = _station_complexes.find(station_id.base());
	return it != _station_complexes.end() && it->second.HasSurfaceExit();
}

/* static */ SQInteger ScriptMetro::GetStationComplexPlatformCount(StationID station_id)
{
	if (!ScriptStation::IsValidStation(station_id)) return -1;

	auto it = _station_complexes.find(station_id.base());
	return it == _station_complexes.end() ? 0 : static_cast<SQInteger>(it->second.GetPlatforms().size());
}

/* static */ SQInteger ScriptMetro::GetStationComplexExitCount(StationID station_id)
{
	if (!ScriptStation::IsValidStation(station_id)) return -1;

	auto it = _station_complexes.find(station_id.base());
	return it == _station_complexes.end() ? 0 : static_cast<SQInteger>(it->second.GetExits().size());
}

/* static */ SQInteger ScriptMetro::GetStationComplexDeepestLevel(StationID station_id)
{
	if (!ScriptStation::IsValidStation(station_id)) return 0;

	auto it = _station_complexes.find(station_id.base());
	return it == _station_complexes.end() ? 0 : it->second.GetDeepestLevel();
}

/* static */ bool ScriptMetro::HasPortalConnection(TileIndex entry_tile, TileIndex exit_tile, SQInteger depth)
{
	if (!::IsValidTile(entry_tile) || !::IsValidTile(exit_tile) || !IsValidMetroDepth(depth)) return false;

	for (const PortalConnection &connection : GetPortalConnections(entry_tile)) {
		if (connection.exit_tile == exit_tile && connection.depth == depth) return true;
	}
	return false;
}

/* static */ bool ScriptMetro::IsVehicleUnderground(VehicleID vehicle_id)
{
	if (!ScriptVehicle::IsValidVehicle(vehicle_id)) return false;
	if (ScriptVehicle::GetVehicleType(vehicle_id) != ScriptVehicle::VT_RAIL) return false;

	const Train *train = Train::GetIfValid(vehicle_id);
	return train != nullptr && train->IsUnderground();
}

/* static */ SQInteger ScriptMetro::GetVehicleUndergroundDepth(VehicleID vehicle_id)
{
	if (!IsVehicleUnderground(vehicle_id)) return 0;

	const Train *train = Train::Get(vehicle_id);
	return train->underground_depth;
}

/* static */ TileIndex ScriptMetro::GetVehicleUndergroundTile(VehicleID vehicle_id)
{
	if (!IsVehicleUnderground(vehicle_id)) return INVALID_TILE;

	const Train *train = Train::Get(vehicle_id);
	return train->underground_tile_raw == 0 ? INVALID_TILE : TileIndex(train->underground_tile_raw);
}
