/*!
 *\file geojson_outline.cpp
 *\brief Loads a country outline from GeoJSON into projected polygons.
 */

#include "geo/geojson_outline.h"

#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "geo/projection.h"

namespace MiniDb
{
   namespace
   {
      Result AppendRing(const nlohmann::json& ring, MapPolygonList& polygons)
      {
         if (!ring.is_array() || ring.size() < 3)
         {
            return Result::FileError;
         }

         MapPolygon polygon;
         polygon.reserve(ring.size());
         for (const nlohmann::json& point : ring)
         {
            if (!point.is_array() || point.size() < 2)
            {
               return Result::FileError;
            }
            if (!point[0].is_number() || !point[1].is_number())
            {
               return Result::FileError;
            }

            const auto longitude = static_cast<float>(point[0].get<double>());
            const auto latitude = static_cast<float>(point[1].get<double>());
            polygon.push_back(ProjectWgs84(latitude, longitude));
         }

         polygons.push_back(polygon);
         return Result::Ok;
      }

      Result AppendPolygonCoordinates(const nlohmann::json& coordinates, MapPolygonList& polygons)
      {
         if (!coordinates.is_array() || coordinates.empty())
         {
            return Result::FileError;
         }

         return AppendRing(coordinates[0], polygons);
      }

      Result AppendMultiPolygonCoordinates(const nlohmann::json& coordinates, MapPolygonList& polygons)
      {
         if (!coordinates.is_array())
         {
            return Result::FileError;
         }

         for (const nlohmann::json& polygonCoordinates : coordinates)
         {
            const Result result = AppendPolygonCoordinates(polygonCoordinates, polygons);
            if (IsErr(result))
            {
               return result;
            }
         }

         return Result::Ok;
      }

      Result AppendGeometry(const nlohmann::json& geometry, MapPolygonList& polygons);

      Result AppendGeometryCollection(const nlohmann::json& geometries, MapPolygonList& polygons)
      {
         if (!geometries.is_array())
         {
            return Result::FileError;
         }

         for (const nlohmann::json& geometry : geometries)
         {
            const Result result = AppendGeometry(geometry, polygons);
            if (IsErr(result))
            {
               return result;
            }
         }

         return Result::Ok;
      }

      Result AppendGeometry(const nlohmann::json& geometry, MapPolygonList& polygons)
      {
         if (!geometry.is_object() || !geometry.contains("type") || !geometry.contains("coordinates"))
         {
            if (geometry.is_object() && geometry.contains("geometries"))
            {
               return AppendGeometryCollection(geometry["geometries"], polygons);
            }
            return Result::FileError;
         }

         const std::string type = geometry["type"].get<std::string>();
         if (type == "Polygon")
         {
            return AppendPolygonCoordinates(geometry["coordinates"], polygons);
         }
         if (type == "MultiPolygon")
         {
            return AppendMultiPolygonCoordinates(geometry["coordinates"], polygons);
         }
         if (type == "GeometryCollection")
         {
            return AppendGeometryCollection(geometry["geometries"], polygons);
         }

         return Result::Ok;
      }

      Result AppendDocument(const nlohmann::json& document, MapPolygonList& polygons)
      {
         if (!document.is_object() || !document.contains("type"))
         {
            return Result::FileError;
         }

         const std::string type = document["type"].get<std::string>();
         if (type == "FeatureCollection")
         {
            if (!document.contains("features") || !document["features"].is_array())
            {
               return Result::FileError;
            }

            for (const nlohmann::json& feature : document["features"])
            {
               if (!feature.contains("geometry"))
               {
                  continue;
               }

               const Result result = AppendGeometry(feature["geometry"], polygons);
               if (IsErr(result))
               {
                  return result;
               }
            }

            return polygons.empty() ? Result::FileError : Result::Ok;
         }

         if (type == "Feature")
         {
            if (!document.contains("geometry"))
            {
               return Result::FileError;
            }

            return AppendGeometry(document["geometry"], polygons);
         }

         return AppendGeometry(document, polygons);
      }
   } // namespace

   Result LoadOutlineFromString(std::string_view jsonText, MapPolygonList& polygons)
   {
      if (jsonText.empty())
      {
         return Result::InvalidArgument;
      }

      polygons.clear();
      const nlohmann::json document = nlohmann::json::parse(jsonText, nullptr, false);
      if (document.is_discarded())
      {
         return Result::FileError;
      }

      return AppendDocument(document, polygons);
   }

   Result LoadOutlineFromFile(std::string_view filePath, MapPolygonList& polygons)
   {
      if (filePath.empty())
      {
         return Result::InvalidArgument;
      }

      const std::string pathText(filePath);
      std::ifstream file;
      file.open(pathText);
      if (!file.is_open())
      {
         return Result::FileError;
      }

      std::ostringstream buffer;
      buffer << file.rdbuf();
      return LoadOutlineFromString(buffer.str(), polygons);
   }
} // namespace MiniDb
