/*!
 *\file test_helpers.h
 *\brief Shared helpers for MiniDB unit tests.
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <string>
#include <string_view>

#include "core/types.h"
#include "geo/projection.h"

namespace MiniDb
{
   namespace Test
   {
      inline StationRecord MakeStation(
         StationId stationId,
         std::string_view name,
         float latitude,
         float longitude,
         uint32_t population)
      {
         StationRecord record;
         record.id = stationId;
         record.cityName = std::string(name);
         record.stationName = std::string(name);
         record.latitude = latitude;
         record.longitude = longitude;
         record.position = ProjectWgs84(latitude, longitude);
         record.population = population;
         return record;
      }
   } // namespace Test
} // namespace MiniDb

#endif // TEST_HELPERS_H
