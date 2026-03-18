/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file multilayer_sl.cpp Save/load for underground slices (portals, tracks, etc). */

#include "../stdafx.h"
#include "saveload.h"
#include "../multilayer/multilayer_map.h"
#include "../multilayer/portal_registry.h"
#include "../map_func.h"

#include "../safeguards.h"

/*
 * Strategy: save only non-Surface slices (Track, Portal, Ramp, StationTile).
 * Each saved record = one slice with its tile index and all fields.
 * On load, rebuild columns from saved slices. Surface slices are auto-created
 * by Map::Allocate() and don't need saving.
 */

/** Flat struct for saving a slice + its tile location. */
struct SliceSaveData {
	uint32_t tile_index = 0;
	uint8_t kind = 0;
	int16_t z_top = 0;
	int16_t z_bot = 0;
	uint8_t owner = 0;
	uint8_t payload0 = 0; /* track.railtype OR portal.direction OR ramp.direction */
	uint8_t payload1 = 0; /* track.tracks */
	uint8_t payload2 = 0; /* track.signal_mask */
	uint8_t payload3 = 0; /* track.reserved */
	uint32_t payload4 = 0; /* portal.target_slice OR ramp depths packed */
	uint16_t payload5 = 0; /* station.station_id */
};

static const SaveLoad _slice_save_desc[] = {
	SLE_VAR(SliceSaveData, tile_index, SLE_UINT32),
	SLE_VAR(SliceSaveData, kind,       SLE_UINT8),
	SLE_VAR(SliceSaveData, z_top,      SLE_INT16),
	SLE_VAR(SliceSaveData, z_bot,      SLE_INT16),
	SLE_VAR(SliceSaveData, owner,      SLE_UINT8),
	SLE_VAR(SliceSaveData, payload0,   SLE_UINT8),
	SLE_VAR(SliceSaveData, payload1,   SLE_UINT8),
	SLE_VAR(SliceSaveData, payload2,   SLE_UINT8),
	SLE_VAR(SliceSaveData, payload3,   SLE_UINT8),
	SLE_VAR(SliceSaveData, payload4,   SLE_UINT32),
	SLE_VAR(SliceSaveData, payload5,   SLE_UINT16),
};

/** Pack a TileSlice into SliceSaveData. */
static SliceSaveData PackSlice(TileIndex tile, const TileSlice &s)
{
	SliceSaveData d;
	d.tile_index = tile.base();
	d.kind = static_cast<uint8_t>(s.kind);
	d.z_top = s.span.z_top;
	d.z_bot = s.span.z_bot;
	d.owner = s.owner;

	switch (s.kind) {
		case SliceKind::Track:
			d.payload0 = s.track.railtype;
			d.payload1 = s.track.tracks;
			d.payload2 = s.track.signal_mask;
			d.payload3 = s.track.reserved;
			break;
		case SliceKind::Portal:
			d.payload0 = s.portal.direction;
			d.payload4 = s.portal.target_slice;
			break;
		case SliceKind::StationTile:
			d.payload5 = s.station.station_id;
			d.payload0 = s.station.role;
			break;
		case SliceKind::Ramp:
			d.payload0 = s.ramp.direction;
			d.payload1 = s.ramp.railtype;
			d.payload4 = (static_cast<uint32_t>(static_cast<uint16_t>(s.ramp.entry_depth)) << 16) |
			             static_cast<uint16_t>(s.ramp.exit_depth);
			break;
		default:
			break;
	}
	return d;
}

/** Unpack SliceSaveData into a TileSlice. */
static TileSlice UnpackSlice(const SliceSaveData &d)
{
	TileSlice s;
	s.kind = static_cast<SliceKind>(d.kind);
	s.span = {d.z_top, d.z_bot};
	s.owner = d.owner;

	switch (s.kind) {
		case SliceKind::Track:
			s.track.railtype = d.payload0;
			s.track.tracks = d.payload1;
			s.track.signal_mask = d.payload2;
			s.track.reserved = d.payload3;
			break;
		case SliceKind::Portal:
			s.portal.direction = d.payload0;
			s.portal.target_slice = d.payload4;
			break;
		case SliceKind::StationTile:
			s.station.station_id = d.payload5;
			s.station.role = d.payload0;
			break;
		case SliceKind::Ramp:
			s.ramp.direction = d.payload0;
			s.ramp.railtype = d.payload1;
			s.ramp.entry_depth = static_cast<int16_t>(d.payload4 >> 16);
			s.ramp.exit_depth = static_cast<int16_t>(d.payload4 & 0xFFFF);
			break;
		default:
			break;
	}
	return s;
}

/**
 * MLSL chunk: All non-surface slices.
 * Saves Track, Portal, Ramp, StationTile slices with tile location.
 */
struct MLSLChunkHandler : ChunkHandler {
	MLSLChunkHandler() : ChunkHandler('MLSL', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_slice_save_desc);

		uint idx = 0;
		for (uint i = 0; i < Map::Size(); i++) {
			TileIndex tile(i);
			const TileColumn &col = _multilayer_map.GetColumn(tile);
			for (SliceID sid : col.slices) {
				const TileSlice &s = _multilayer_map.GetSlice(sid);
				if (s.kind == SliceKind::Surface || s.kind == SliceKind::End) continue;

				SlSetArrayIndex(idx++);
				SliceSaveData d = PackSlice(tile, s);
				SlObject(&d, _slice_save_desc);
			}
		}
	}

	void Load() const override
	{
		if (IsSavegameVersionBefore(SLV_MULTILAYER_MAP)) return;

		const std::vector<SaveLoad> slt = SlTableHeader(_slice_save_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			SliceSaveData d;
			SlObject(&d, slt);

			TileIndex tile(d.tile_index);
			if (tile.base() >= Map::Size()) continue;

			TileSlice s = UnpackSlice(d);

			/* Allocate and insert into multilayer map. */
			SliceID new_id = _multilayer_map.AllocateSlice();
			_multilayer_map.GetSlice(new_id) = s;
			_multilayer_map.InsertSlice(tile, new_id);
		}

		/* Rebuild portal connections after loading. */
		RecomputePortalConnections();
	}
};

/* MLCL, MLUT, STCX: empty stubs for format compatibility. */
struct DummySL { uint8_t dummy = 0; };
static const SaveLoad _dummy_desc[] = { SLE_VAR(DummySL, dummy, SLE_UINT8) };

struct MLCLChunkHandler : ChunkHandler {
	MLCLChunkHandler() : ChunkHandler('MLCL', CH_TABLE) {}
	void Save() const override { SlTableHeader(_dummy_desc); }
	void Load() const override {
		if (IsSavegameVersionBefore(SLV_MULTILAYER_MAP)) return;
		SlTableHeader(_dummy_desc);
		while (SlIterateArray() != -1) { DummySL d; SlObject(&d, _dummy_desc); }
	}
};

struct MLUTChunkHandler : ChunkHandler {
	MLUTChunkHandler() : ChunkHandler('MLUT', CH_TABLE) {}
	void Save() const override { SlTableHeader(_dummy_desc); }
	void Load() const override {
		if (IsSavegameVersionBefore(SLV_MULTILAYER_MAP)) return;
		SlTableHeader(_dummy_desc);
		while (SlIterateArray() != -1) { DummySL d; SlObject(&d, _dummy_desc); }
	}
};

struct STCXChunkHandler : ChunkHandler {
	STCXChunkHandler() : ChunkHandler('STCX', CH_TABLE) {}
	void Save() const override { SlTableHeader(_dummy_desc); }
	void Load() const override {
		if (IsSavegameVersionBefore(SLV_MULTILAYER_MAP)) return;
		SlTableHeader(_dummy_desc);
		while (SlIterateArray() != -1) { DummySL d; SlObject(&d, _dummy_desc); }
	}
};

static const MLSLChunkHandler MLSL;
static const MLCLChunkHandler MLCL;
static const MLUTChunkHandler MLUT;
static const STCXChunkHandler STCX;
static const ChunkHandlerRef multilayer_chunk_handlers[] = { MLSL, MLCL, MLUT, STCX };
extern const ChunkHandlerTable _multilayer_chunk_handlers(multilayer_chunk_handlers);
