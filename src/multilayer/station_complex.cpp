/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_complex.cpp Implementation of StationComplex management. */

#include "../stdafx.h"
#include "station_complex.h"

#include "../safeguards.h"

/** Global registry of station complexes. */
std::unordered_map<uint16_t, StationComplex> _station_complexes;

/**
 * Find or create a StationComplex for a given station ID.
 * @param station_id The station pool index.
 * @return Reference to the StationComplex.
 */
StationComplex &GetOrCreateStationComplex(uint16_t station_id)
{
	auto it = _station_complexes.find(station_id);
	if (it != _station_complexes.end()) return it->second;

	auto &sc = _station_complexes[station_id];
	sc.station_id = station_id;
	return sc;
}

/**
 * Remove a StationComplex when the station is deleted.
 * @param station_id The station pool index.
 */
void RemoveStationComplex(uint16_t station_id)
{
	_station_complexes.erase(station_id);
}
