/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_render.cpp Drawing underground tiles in the viewport. */

#include "../stdafx.h"
#include "underground_render.h"
#include "underground_view.h"
#include "multilayer_map.h"
#include "../viewport_func.h"
#include "../rail.h"
#include "../sprite.h"
#include "../slope_func.h"
#include "../table/sprites.h"
#include "../track_func.h"
#include "../pbs.h"

#include "../safeguards.h"

/**
 * Get a recolour palette for underground ground based on depth.
 * Deeper = darker. Uses available palette constants.
 *   z=-1: PALETTE_NEWSPAPER (light grey)
 *   z=-2: PALETTE_TO_GREY (medium grey)
 *   z=-3 and deeper: PALETTE_TO_STRUCT_CONCRETE (dark grey)
 */
static PaletteID GetDepthPalette(int16_t depth)
{
	if (depth >= -1) return PALETTE_NEWSPAPER;        /* Light grey for shallow. */
	if (depth >= -3) return PALETTE_TO_GREY;           /* Medium grey. */
	return PALETTE_TO_STRUCT_CONCRETE;                  /* Dark grey for deep. */
}

/**
 * Check if a tile has underground content at the current view depth.
 */
bool HasUndergroundContent(TileIndex tile)
{
	if (!_multilayer_map.IsAllocated()) return false;
	return _multilayer_map.GetColumn(tile).HasUnderground();
}

/**
 * Check if a tile has a portal (for surface rendering — portals should
 * be visible even when NOT in underground view).
 */
bool HasPortalOnTile(TileIndex tile)
{
	if (!_multilayer_map.IsAllocated()) return false;
	return _multilayer_map.FindSliceByKind(tile, SliceKind::Portal) != INVALID_SLICE_ID;
}

/**
 * Draw portal indicator on the surface (always visible, not just in underground view).
 * Called from the normal tile drawing path.
 */
void DrawPortalOverlay(TileIndex tile)
{
	SliceID sid = _multilayer_map.FindSliceByKind(tile, SliceKind::Portal);
	if (sid == INVALID_SLICE_ID) return;

	const TileSlice &ps = _multilayer_map.GetSlice(sid);
	DiagDirection dir = static_cast<DiagDirection>(ps.portal.direction);

	/* Tunnel portal sprite: direction * 2 offset (8 sprites total, 2 per dir). */
	DrawGroundSprite(SPR_TUNNEL_ENTRY_REAR_RAIL + dir * 2, PAL_NONE);
}

/**
 * Draw underground content for a tile (underground view mode).
 * Replaces the normal surface tile drawing when underground view is active
 * and this tile has underground content.
 *
 * Visual design:
 *   - Background: bare land recoloured to grey (darker = deeper)
 *   - Track: standard rail sprites on top of grey ground
 *   - Portal: tunnel entrance sprite
 *   - Station: rail track with highlighted platform
 *   - Reserved: crash-palette overlay for PBS reservations
 */
void DrawUndergroundTile(TileIndex tile)
{
	int16_t depth = GetUndergroundViewDepth();

	/* Check all depths at this tile for content. */
	SliceID sid = _multilayer_map.FindSliceAt(tile, depth);

	if (sid == INVALID_SLICE_ID) {
		/* No content at this depth — draw flat grey ground. */
		DrawGroundSprite(SPR_FLAT_BARE_LAND, GetDepthPalette(depth));

		/* Show faint overlay of tracks at OTHER depths (crossing hint). */
		const TileColumn &col = _multilayer_map.GetColumn(tile);
		for (SliceID other_sid : col.slices) {
			const TileSlice &other = _multilayer_map.GetSlice(other_sid);
			if (other.kind == SliceKind::Track && other.span.z_bot != depth) {
				TrackBits tracks = static_cast<TrackBits>(other.track.tracks);
				RailType rt = static_cast<RailType>(other.track.railtype);
				const RailTypeInfo *rti = GetRailTypeInfo(rt);
				if (tracks & TRACK_BIT_X) DrawGroundSprite(rti->base_sprites.single_x, PALETTE_TO_TRANSPARENT);
				if (tracks & TRACK_BIT_Y) DrawGroundSprite(rti->base_sprites.single_y, PALETTE_TO_TRANSPARENT);
			}
		}
		return;
	}

	const TileSlice &slice = _multilayer_map.GetSlice(sid);
	PaletteID ground_pal = GetDepthPalette(depth);

	/* Draw underground tiles at surface level (z=0) — depth is shown via colour only.
	 * Using z-offset causes overlap with neighboring surface tiles. */
	int z_off = 0;

	switch (slice.kind) {
		case SliceKind::Track: {
			/* Draw ground with depth-based grey, lowered by z_off. */
			DrawGroundSpriteAt(SPR_FLAT_BARE_LAND, ground_pal, 0, 0, z_off);

			TrackBits tracks = static_cast<TrackBits>(slice.track.tracks);
			RailType rt = static_cast<RailType>(slice.track.railtype);
			const RailTypeInfo *rti = GetRailTypeInfo(rt);

			/* Draw each track piece at underground depth. */
			if (tracks & TRACK_BIT_X)     DrawGroundSpriteAt(rti->base_sprites.single_x, PAL_NONE, 0, 0, z_off);
			if (tracks & TRACK_BIT_Y)     DrawGroundSpriteAt(rti->base_sprites.single_y, PAL_NONE, 0, 0, z_off);
			if (tracks & TRACK_BIT_UPPER) DrawGroundSpriteAt(rti->base_sprites.single_n, PAL_NONE, 0, 0, z_off);
			if (tracks & TRACK_BIT_LOWER) DrawGroundSpriteAt(rti->base_sprites.single_s, PAL_NONE, 0, 0, z_off);
			if (tracks & TRACK_BIT_LEFT)  DrawGroundSpriteAt(rti->base_sprites.single_w, PAL_NONE, 0, 0, z_off);
			if (tracks & TRACK_BIT_RIGHT) DrawGroundSpriteAt(rti->base_sprites.single_e, PAL_NONE, 0, 0, z_off);

			/* PBS reservation overlay. */
			if (slice.track.reserved != 0) {
				TrackBits reserved = static_cast<TrackBits>(slice.track.reserved);
				if (reserved & TRACK_BIT_X)     DrawGroundSpriteAt(rti->base_sprites.single_x, PALETTE_CRASH, 0, 0, z_off);
				if (reserved & TRACK_BIT_Y)     DrawGroundSpriteAt(rti->base_sprites.single_y, PALETTE_CRASH, 0, 0, z_off);
				if (reserved & TRACK_BIT_UPPER) DrawGroundSpriteAt(rti->base_sprites.single_n, PALETTE_CRASH, 0, 0, z_off);
				if (reserved & TRACK_BIT_LOWER) DrawGroundSpriteAt(rti->base_sprites.single_s, PALETTE_CRASH, 0, 0, z_off);
				if (reserved & TRACK_BIT_LEFT)  DrawGroundSpriteAt(rti->base_sprites.single_w, PALETTE_CRASH, 0, 0, z_off);
				if (reserved & TRACK_BIT_RIGHT) DrawGroundSpriteAt(rti->base_sprites.single_e, PALETTE_CRASH, 0, 0, z_off);
			}

			/* Draw PBS signals. */
			if (slice.track.signal_mask != 0) {
				TileRef ref{tile, sid};
				TrackBits signal_tracks = static_cast<TrackBits>(slice.track.signal_mask);
				for (Track t = TRACK_BEGIN; t < TRACK_END; t++) {
					if (!(signal_tracks & TrackToTrackBits(t))) continue;
					/* Use PBS signal sprites. Compute state dynamically. */
					Trackdir td = TrackToTrackdir(t);
					SignalState state = GetUndergroundSignalState(ref, td);
					/* SPR_SIGNALS_BASE + signal type offset + state.
					 * PBS = type 4, 16 sprites per type, state 0=red 1=green. */
					SpriteID signal_sprite = SPR_SIGNALS_BASE + (4 * 16) + (state == SIGNAL_STATE_GREEN ? 1 : 0);
					/* Draw at an offset based on track direction. */
					int x_off = 0, y_off = 0;
					if (t == TRACK_X) { x_off = 8; y_off = 0; }
					else if (t == TRACK_Y) { x_off = 0; y_off = 8; }
					else if (t == TRACK_UPPER) { x_off = 8; y_off = -4; }
					else if (t == TRACK_LOWER) { x_off = 8; y_off = 12; }
					else if (t == TRACK_LEFT) { x_off = -4; y_off = 8; }
					else if (t == TRACK_RIGHT) { x_off = 12; y_off = 8; }
					DrawGroundSpriteAt(signal_sprite, PAL_NONE, x_off, y_off, z_off);
				}
			}
			break;
		}

		case SliceKind::Portal: {
			/* Portal: depth-colored background + tunnel entry sprite (dir * 2 offset). */
			DrawGroundSpriteAt(SPR_FLAT_BARE_LAND, ground_pal, 0, 0, z_off);
			DiagDirection dir = static_cast<DiagDirection>(slice.portal.direction);
			DrawGroundSpriteAt(SPR_TUNNEL_ENTRY_REAR_RAIL + dir * 2, PAL_NONE, 0, 0, z_off);
			break;
		}

		case SliceKind::StationTile: {
			/* Station platform: blue-highlighted ground with rail in correct direction. */
			DrawGroundSpriteAt(SPR_FLAT_BARE_LAND, PALETTE_TO_STRUCT_BLUE, 0, 0, z_off);

			TrackBits st_tracks = static_cast<TrackBits>(slice.station.tracks);
			RailType st_rt = static_cast<RailType>(slice.station.railtype);
			if (st_rt < RAILTYPE_END) {
				const RailTypeInfo *st_rti = GetRailTypeInfo(st_rt);
				if (st_tracks & TRACK_BIT_X) DrawGroundSpriteAt(st_rti->base_sprites.single_x, PAL_NONE, 0, 0, z_off);
				if (st_tracks & TRACK_BIT_Y) DrawGroundSpriteAt(st_rti->base_sprites.single_y, PAL_NONE, 0, 0, z_off);
			} else {
				/* Fallback: default rail sprite. */
				if (st_tracks & TRACK_BIT_X) DrawGroundSpriteAt(SPR_RAIL_TRACK_X, PAL_NONE, 0, 0, z_off);
				if (st_tracks & TRACK_BIT_Y) DrawGroundSpriteAt(SPR_RAIL_TRACK_Y, PAL_NONE, 0, 0, z_off);
			}

			/* Draw platform edge sprites (simple visual indicators). */
			if (st_tracks & TRACK_BIT_X) {
				DrawGroundSpriteAt(SPR_RAIL_PLATFORM_X_REAR, PALETTE_TO_STRUCT_BLUE, 0, 0, z_off);
				DrawGroundSpriteAt(SPR_RAIL_PLATFORM_X_FRONT, PALETTE_TO_STRUCT_BLUE, 0, 0, z_off);
			} else if (st_tracks & TRACK_BIT_Y) {
				DrawGroundSpriteAt(SPR_RAIL_PLATFORM_Y_REAR, PALETTE_TO_STRUCT_BLUE, 0, 0, z_off);
				DrawGroundSpriteAt(SPR_RAIL_PLATFORM_Y_FRONT, PALETTE_TO_STRUCT_BLUE, 0, 0, z_off);
			}

			/* PBS reservation overlay for station. */
			if (slice.station.reserved != 0) {
				if (st_tracks & TRACK_BIT_X) DrawGroundSpriteAt(SPR_RAIL_TRACK_X, PALETTE_CRASH, 0, 0, z_off);
				if (st_tracks & TRACK_BIT_Y) DrawGroundSpriteAt(SPR_RAIL_TRACK_Y, PALETTE_CRASH, 0, 0, z_off);
			}
			break;
		}

		case SliceKind::Ramp: {
			/* Ramp: sloped ground with depth palette + directional slope rail sprite. */
			DiagDirection ramp_dir = static_cast<DiagDirection>(slice.ramp.direction);

			/* Map DiagDirection to Slope for ground sprite. */
			static const Slope _dir_to_slope[] = {SLOPE_NE, SLOPE_SE, SLOPE_SW, SLOPE_NW};
			Slope ramp_slope = _dir_to_slope[ramp_dir];

			/* Sloped ground with depth color. */
			DrawGroundSpriteAt(SPR_FLAT_BARE_LAND + SlopeToSpriteOffset(ramp_slope), ground_pal, 0, 0, z_off);

			/* Rail on slope — 4 directional sprites. */
			DrawGroundSpriteAt(SPR_TRACKS_FOR_SLOPES_RAIL_BASE + ramp_dir, PAL_NONE, 0, 0, z_off);
			break;
		}

		default:
			break;
	}
}
