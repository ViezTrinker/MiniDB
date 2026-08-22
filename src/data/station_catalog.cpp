/*!
 *\file station_catalog.cpp
 *\brief Loads the static city/station catalog from JSON.
 */

#include "data/station_catalog.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "core/constants.h"
#include "geo/projection.h"

namespace MiniDb
{
   namespace
   {
      bool ComparePopulationDescending(const StationRecord& left, const StationRecord& right)
      {
         return left.population > right.population;
      }

      Result ParseStations(const nlohmann::json& document, StationRecordList& stations)
      {
         if (!document.is_object() || !document.contains("stations") || !document["stations"].is_array())
         {
            return Result::FileError;
         }

         stations.clear();
         for (const nlohmann::json& entry : document["stations"])
         {
            if (!entry.is_object())
            {
               return Result::FileError;
            }
            if (!entry.contains("id") || !entry.contains("cityName") || !entry.contains("stationName"))
            {
               return Result::FileError;
            }
            if (!entry.contains("latitude") || !entry.contains("longitude") || !entry.contains("population"))
            {
               return Result::FileError;
            }
            if (!entry["id"].is_number_unsigned() || !entry["population"].is_number_unsigned())
            {
               return Result::FileError;
            }
            if (!entry["latitude"].is_number() || !entry["longitude"].is_number())
            {
               return Result::FileError;
            }
            if (!entry["cityName"].is_string() || !entry["stationName"].is_string())
            {
               return Result::FileError;
            }

            StationRecord record;
            record.id = entry["id"].get<StationId>();
            record.cityName = entry["cityName"].get<std::string>();
            record.stationName = entry["stationName"].get<std::string>();
            record.latitude = static_cast<float>(entry["latitude"].get<double>());
            record.longitude = static_cast<float>(entry["longitude"].get<double>());
            record.population = entry["population"].get<uint32_t>();
            if (record.population < MinimumCityPopulation)
            {
               continue;
            }
            if (record.cityName.empty() || record.stationName.empty())
            {
               return Result::FileError;
            }

            record.position = ProjectWgs84(record.latitude, record.longitude);
            stations.push_back(record);
         }

         if (stations.empty())
         {
            return Result::FileError;
         }

         std::sort(stations.begin(), stations.end(), ComparePopulationDescending);
         return Result::Ok;
      }
   } // namespace

   Result LoadStationCatalogFromString(std::string_view jsonText, StationRecordList& stations)
   {
      if (jsonText.empty())
      {
         return Result::InvalidArgument;
      }

      const nlohmann::json document = nlohmann::json::parse(jsonText, nullptr, false);
      if (document.is_discarded())
      {
         return Result::FileError;
      }

      return ParseStations(document, stations);
   }

   Result LoadStationCatalogFromFile(std::string_view filePath, StationRecordList& stations)
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
      return LoadStationCatalogFromString(buffer.str(), stations);
   }
} // namespace MiniDb
