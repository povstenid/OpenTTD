/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_gui.cpp GUI window for underground rail construction. */

#include "../stdafx.h"
#include "underground_gui.h"
#include "underground_view.h"
#include "underground_cmd.h"
#include "multilayer_map.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../viewport_func.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../rail_gui.h"
#include "../rail.h"
#include "../sound_func.h"
#include "../strings_func.h"
#include "../string_func.h"
#include "../tilehighlight_func.h"
#include "../gfx_func.h"
#include "../track_func.h"

#include "../table/sprites.h"
#include "../table/strings.h"

#include "../safeguards.h"

/** Widget IDs for the underground toolbar. */
enum UndergroundToolbarWidgets : WidgetID {
	WID_UG_VIEW_TOGGLE,   ///< Toggle underground view.
	WID_UG_BUILD_RAIL,    ///< Build underground rail (autorail).
	WID_UG_BUILD_PORTAL,  ///< Build portal (drag direction).
	WID_UG_BUILD_RAMP_DN, ///< Build ramp going deeper.
	WID_UG_BUILD_RAMP_UP, ///< Build ramp going shallower.
	WID_UG_BUILD_SIGNAL,  ///< Build PBS signal on underground track.
	WID_UG_BUILD_STATION, ///< Build underground station platform.
	WID_UG_REMOVE,        ///< Remove underground rail.
	WID_UG_DEPTH_DOWN,    ///< Decrease depth.
	WID_UG_DEPTH_DISPLAY, ///< Show current depth.
	WID_UG_DEPTH_UP,      ///< Increase depth.
	WID_UG_RAILTYPE,      ///< Railtype indicator / cycle.
};

/** Underground toolbar window. */
struct UndergroundToolbarWindow : Window {
	RailType railtype;

	UndergroundToolbarWindow(WindowDesc &desc) : Window(desc)
	{
		this->railtype = RAILTYPE_RAIL;
		this->InitNested(0);

		if (!IsUndergroundViewActive()) {
			ToggleUndergroundView();
			MarkWholeScreenDirty();
		}
	}

	void Close(int data = 0) override
	{
		if (IsUndergroundViewActive()) {
			ToggleUndergroundView();
			MarkWholeScreenDirty();
		}
		this->Window::Close(data);
	}

	void OnPaint() override
	{
		this->SetWidgetLoweredState(WID_UG_VIEW_TOGGLE, IsUndergroundViewActive());
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_UG_VIEW_TOGGLE:
				ToggleUndergroundView();
				MarkWholeScreenDirty();
				this->SetDirty();
				break;

			case WID_UG_BUILD_RAIL:
				HandlePlacePushButton(this, WID_UG_BUILD_RAIL, SPR_CURSOR_NS_TRACK, HT_RAIL);
				break;

			case WID_UG_BUILD_PORTAL:
				/* Portal uses same drag mode as rail — direction from drag. */
				HandlePlacePushButton(this, WID_UG_BUILD_PORTAL, SPR_CURSOR_TUNNEL_RAIL, HT_RAIL);
				break;

			case WID_UG_BUILD_RAMP_DN:
				HandlePlacePushButton(this, WID_UG_BUILD_RAMP_DN, SPR_CURSOR_NS_TRACK, HT_RAIL);
				break;

			case WID_UG_BUILD_RAMP_UP:
				HandlePlacePushButton(this, WID_UG_BUILD_RAMP_UP, SPR_CURSOR_NS_TRACK, HT_RAIL);
				break;

			case WID_UG_BUILD_SIGNAL:
				HandlePlacePushButton(this, WID_UG_BUILD_SIGNAL, SPR_CURSOR_BUILDSIGNALS_FIRST, HT_RECT);
				break;

			case WID_UG_BUILD_STATION:
				HandlePlacePushButton(this, WID_UG_BUILD_STATION, SPR_CURSOR_NS_TRACK, HT_RAIL);
				break;

			case WID_UG_REMOVE:
				HandlePlacePushButton(this, WID_UG_REMOVE, ANIMCURSOR_DEMOLISH, HT_RECT);
				break;

			case WID_UG_DEPTH_DOWN: {
				int16_t depth = GetUndergroundViewDepth();
				if (depth > GetMetroGlobalMinZ()) {
					SetUndergroundViewDepth(depth - 1);
					MarkWholeScreenDirty();
					this->SetDirty();
				}
				break;
			}

			case WID_UG_DEPTH_UP: {
				int16_t depth = GetUndergroundViewDepth();
				if (depth < GetMetroGlobalMaxZ()) {
					SetUndergroundViewDepth(depth + 1);
					MarkWholeScreenDirty();
					this->SetDirty();
				}
				break;
			}

			case WID_UG_RAILTYPE:
				/* Cycle through railtypes. */
				this->railtype = static_cast<RailType>((this->railtype + 1) % RAILTYPE_END);
				this->SetDirty();
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		int16_t depth = GetUndergroundViewDepth();

		if (this->IsWidgetLowered(WID_UG_BUILD_SIGNAL)) {
			/* Place signal on the first track found at this depth. */
			SliceID sid = _multilayer_map.FindSliceAt(tile, depth);
			if (sid != INVALID_SLICE_ID) {
				const TileSlice &slice = _multilayer_map.GetSlice(sid);
				if (slice.kind == SliceKind::Track) {
					Track t = FindFirstTrack(static_cast<TrackBits>(slice.track.tracks));
					Command<Commands::BuildUndergroundSignal>::Post(tile, t, depth);
				}
			}
			return;
		}

		if (this->IsWidgetLowered(WID_UG_BUILD_STATION)) {
			VpStartPlaceSizing(tile, VPM_RAILDIRS, DDSP_PLACE_RAIL);
			return;
		}

		if (this->IsWidgetLowered(WID_UG_BUILD_RAIL) ||
		    this->IsWidgetLowered(WID_UG_BUILD_PORTAL) ||
		    this->IsWidgetLowered(WID_UG_BUILD_RAMP_DN) ||
		    this->IsWidgetLowered(WID_UG_BUILD_RAMP_UP)) {
			VpStartPlaceSizing(tile, VPM_RAILDIRS, DDSP_PLACE_RAIL);
		} else if (this->IsWidgetLowered(WID_UG_REMOVE)) {
			VpStartPlaceSizing(tile, VPM_RAILDIRS, DDSP_DEMOLISH_AREA);
		}
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method,
		[[maybe_unused]] ViewportDragDropSelectionProcess select_proc,
		[[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x == -1) return;

		int16_t depth = GetUndergroundViewDepth();
		Track track = static_cast<Track>(_thd.drawstyle & HT_DIR_MASK);

		if (select_proc == DDSP_PLACE_RAIL) {
			if (this->IsWidgetLowered(WID_UG_BUILD_RAIL)) {
				/* Build underground rail. */
				Command<Commands::BuildUndergroundRail>::Post(STR_ERROR_CAN_T_BUILD_UNDERGROUND_RAIL,
					start_tile, this->railtype, track, depth);

			} else if (this->IsWidgetLowered(WID_UG_BUILD_PORTAL)) {
				/* Build portal — direction from track orientation. */
				DiagDirection dir;
				switch (track) {
					case TRACK_X:     dir = DIAGDIR_SW; break;
					case TRACK_Y:     dir = DIAGDIR_SE; break;
					case TRACK_UPPER: dir = DIAGDIR_NE; break;
					case TRACK_LOWER: dir = DIAGDIR_SW; break;
					case TRACK_LEFT:  dir = DIAGDIR_NW; break;
					case TRACK_RIGHT: dir = DIAGDIR_SE; break;
					default:          dir = DIAGDIR_NE; break;
				}
				/* If user dragged, use drag direction instead. */
				if (start_tile != end_tile) {
					dir = DiagdirBetweenTiles(start_tile, end_tile);
				}
				Command<Commands::BuildPortal>::Post(STR_ERROR_CAN_T_BUILD_PORTAL,
					start_tile, this->railtype, dir, depth);

			} else if (this->IsWidgetLowered(WID_UG_BUILD_RAMP_DN)) {
				/* Build ramp going deeper: from current depth to depth-1. */
				DiagDirection dir = DIAGDIR_NE;
				if (start_tile != end_tile) dir = DiagdirBetweenTiles(start_tile, end_tile);
				Command<Commands::BuildRamp>::Post(
					start_tile, this->railtype, dir, depth, static_cast<int16_t>(depth - 1));

			} else if (this->IsWidgetLowered(WID_UG_BUILD_RAMP_UP)) {
				/* Build ramp going shallower: from current depth to depth+1. */
				DiagDirection dir = DIAGDIR_NE;
				if (start_tile != end_tile) dir = DiagdirBetweenTiles(start_tile, end_tile);
				Command<Commands::BuildRamp>::Post(
					start_tile, this->railtype, dir, depth, static_cast<int16_t>(depth + 1));
			}
		} else if (select_proc == DDSP_DEMOLISH_AREA) {
			Command<Commands::RemoveUndergroundRail>::Post(STR_ERROR_CAN_T_REMOVE_UNDERGROUND_RAIL,
				start_tile, track, depth);
		}

		/* Station drag: build platform tiles from start to end. */
		if (this->IsWidgetLowered(WID_UG_BUILD_STATION) && select_proc == DDSP_PLACE_RAIL) {
			Axis axis = (track == TRACK_Y || track == TRACK_LEFT || track == TRACK_RIGHT) ? AXIS_Y : AXIS_X;

			/* Use Do (synchronous) for first tile to get station_id, then Post for rest. */
			uint16_t station_id = 0;

			int sx = TileX(start_tile), sy = TileY(start_tile);
			int ex = TileX(end_tile), ey = TileY(end_tile);

			/* Build tiles along axis. First tile creates the station. */
			auto build_tile = [&](TileIndex t) {
				auto result = Command<Commands::BuildUndergroundStation>::Do(
					DoCommandFlags{DoCommandFlag::Execute}, t, this->railtype, depth, station_id, axis);
				if (result.Succeeded() && station_id == 0) {
					/* Find the station that was just created at this tile. */
					SliceID sid = _multilayer_map.FindSliceAt(t, depth);
					if (sid != INVALID_SLICE_ID) {
						const TileSlice &s = _multilayer_map.GetSlice(sid);
						if (s.kind == SliceKind::StationTile) {
							station_id = s.station.station_id;
						}
					}
				}
			};

			if (axis == AXIS_X) {
				if (ex < sx) std::swap(sx, ex);
				for (int x = sx; x <= ex; x++) build_tile(TileXY(x, sy));
			} else {
				if (ey < sy) std::swap(sy, ey);
				for (int y = sy; y <= ey; y++) build_tile(TileXY(sx, y));
			}
		}
	}

	void OnPlaceObjectAbort() override
	{
		this->RaiseButtons();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
		[[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_UG_DEPTH_DISPLAY) {
			uint min_width = GetStringBoundingBox(GetString(STR_JUST_INT, GetMetroGlobalMinZ())).width;
			uint max_width = GetStringBoundingBox(GetString(STR_JUST_INT, GetMetroGlobalMaxZ())).width;
			size.width = std::max(min_width, max_width) + padding.width;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_UG_DEPTH_DISPLAY) {
			int16_t depth = GetUndergroundViewDepth();
			DrawString(r.left, r.right, r.top + (r.bottom - r.top - GetCharacterHeight(FS_NORMAL)) / 2,
				GetString(STR_JUST_INT, depth), TC_WHITE, SA_HOR_CENTER);
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_UG_RAILTYPE) {
			const RailTypeInfo *rti = GetRailTypeInfo(this->railtype);
			return GetString(rti->strings.name);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}
};

/** Widget definition for the underground toolbar. */
static constexpr std::initializer_list<NWidgetPart> _nested_underground_toolbar_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_UNDERGROUND_TOOLBAR_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_VIEW_TOGGLE), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_IMG_SMALLMAP, STR_TOOLBAR_TOOLTIP_UNDERGROUND_VIEW),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_BUILD_RAIL), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_IMG_RAIL_NS, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TRACK),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_BUILD_PORTAL), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_IMG_TUNNEL_RAIL, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_BUILD_RAMP_DN), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_ARROW_DOWN, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_BUILD_RAMP_UP), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_ARROW_UP, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_BUILD_SIGNAL), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_IMG_SIGNAL_ELECTRIC_PBS, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_SIGNALS),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_BUILD_STATION), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_IMG_RAIL_STATION, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_UG_REMOVE), SetMinimalSize(22, 22),
			SetSpriteTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),

		NWidget(NWID_SPACER), SetMinimalSize(4, 0),

		NWidget(WWT_PUSHIMGBTN, COLOUR_DARK_GREEN, WID_UG_DEPTH_DOWN), SetMinimalSize(12, 22),
			SetSpriteTip(SPR_ARROW_DOWN, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_UG_DEPTH_DISPLAY), SetMinimalSize(30, 22),
		EndContainer(),
		NWidget(WWT_PUSHIMGBTN, COLOUR_DARK_GREEN, WID_UG_DEPTH_UP), SetMinimalSize(12, 22),
			SetSpriteTip(SPR_ARROW_UP, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST),

		NWidget(NWID_SPACER), SetMinimalSize(4, 0),

		NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_UG_RAILTYPE), SetMinimalSize(50, 22),
			SetStringTip(STR_JUST_STRING, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TRACK),
	EndContainer(),
};

static WindowDesc _underground_toolbar_desc(
	WDP_ALIGN_TOOLBAR, "toolbar_underground", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_underground_toolbar_widgets
);

/** Show the underground construction toolbar. */
void ShowUndergroundToolbar()
{
	if (!Company::IsValidID(_local_company)) return;

	CloseWindowByClass(WC_BUILD_TOOLBAR);
	new UndergroundToolbarWindow(_underground_toolbar_desc);
}
