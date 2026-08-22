/*!
 *\file geojson_outline.h
 *\brief Loads a country outline from GeoJSON into projected polygons.
 */

#ifndef GEOJSON_OUTLINE_H
#define GEOJSON_OUTLINE_H

#include <string_view>

#include "core/result.h"
#include "core/types.h"

namespace MiniDb
{
   /*!
    *\brief Loads polygons from a GeoJSON file and projects them onto the map.
    *
    *\param[in] filePath Path to a GeoJSON FeatureCollection, Polygon or MultiPolygon.
    *\param[out] polygons Projected exterior rings.
    */
   Result LoadOutlineFromFile(std::string_view filePath, MapPolygonList& polygons);

   /*!
    *\brief Loads polygons from a GeoJSON string and projects them onto the map.
    *
    *\param[in] jsonText GeoJSON document.
    *\param[out] polygons Projected exterior rings.
    */
   Result LoadOutlineFromString(std::string_view jsonText, MapPolygonList& polygons);
} // namespace MiniDb

#endif // GEOJSON_OUTLINE_H
