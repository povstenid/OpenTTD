/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file underground_render.h Underground tile rendering declarations. */

#ifndef UNDERGROUND_RENDER_H
#define UNDERGROUND_RENDER_H

#include "../tile_type.h"

/** Check if a tile has underground content at any depth. */
bool HasUndergroundContent(TileIndex tile);

/** Check if a tile has a portal (visible on surface). */
bool HasPortalOnTile(TileIndex tile);

/** Draw portal overlay on the surface tile (always visible). */
void DrawPortalOverlay(TileIndex tile);

/** Draw underground content for a tile (in underground view mode). */
void DrawUndergroundTile(TileIndex tile);

#endif /* UNDERGROUND_RENDER_H */
