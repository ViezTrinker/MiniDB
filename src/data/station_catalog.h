/*!
 *\file station_catalog.h
 *\brief Loads the static city/station catalog from JSON.
 */

#ifndef STATION_CATALOG_H
#define STATION_CATALOG_H

#include <string_view>

#include "core/result.h"
#include "core/types.h"

namespace MiniDb
{
   /*!
    *\brief Parses a station catalog JSON document.
    *
    * Stations are sorted by descending population after loading.
    *
    *\param[in] jsonText Catalog JSON.
    *\param[out] stations Parsed and projected station records.
    */
   Result LoadStationCatalogFromString(std::string_view jsonText, StationRecordList& stations);

   /*!
    *\brief Loads a station catalog from a JSON file.
    *
    *\param[in] filePath Path to stations.json.
    *\param[out] stations Parsed and projected station records.
    */
   Result LoadStationCatalogFromFile(std::string_view filePath, StationRecordList& stations);
} // namespace MiniDb

#endif // STATION_CATALOG_H
