/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file legacy_import.h Conversion of vanilla savegames to multi-layer format. */

#ifndef LEGACY_IMPORT_H
#define LEGACY_IMPORT_H

/**
 * Materialize all existing tunnels as real underground Track slices.
 *
 * For each tunnel portal pair, creates a chain of Track slices at z=-1
 * (or deeper if there's a conflict) connecting the two portals.
 * Portal slices are created at the entry and exit tiles.
 *
 * Called from AfterLoadGame() when loading a pre-SLV_MULTILAYER_MAP save.
 */
void MaterializeLegacyTunnels();

#endif /* LEGACY_IMPORT_H */
