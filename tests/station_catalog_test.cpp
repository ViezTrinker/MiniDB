/*!
 *\file station_catalog_test.cpp
 *\brief Tests for station catalog loading.
 */

#include <gtest/gtest.h>

#include <string>

#include "data/station_catalog.h"

TEST(StationCatalogTest, LoadsFixtureAndSortsByPopulation)
{
   const std::string filePath = std::string(MINIDB_SOURCE_DIR) + "/tests/fixtures/stations_tiny.json";
   MiniDb::StationRecordList stations;
   const MiniDb::Result result = MiniDb::LoadStationCatalogFromFile(filePath, stations);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(stations.size(), 3u);
   EXPECT_EQ(stations[0].cityName, "Berlin");
   EXPECT_EQ(stations[1].cityName, "Hamburg");
   EXPECT_EQ(stations[2].cityName, "TinyTown");
   EXPECT_GE(stations[0].population, stations[1].population);
   EXPECT_GE(stations[1].population, stations[2].population);
}

TEST(StationCatalogTest, FiltersStationsBelowMinimumPopulation)
{
   const std::string filePath = std::string(MINIDB_SOURCE_DIR) + "/tests/fixtures/stations_tiny.json";
   MiniDb::StationRecordList stations;
   ASSERT_TRUE(MiniDb::IsOk(MiniDb::LoadStationCatalogFromFile(filePath, stations)));
   for (const MiniDb::StationRecord& station : stations)
   {
      EXPECT_GE(station.population, 10000u);
      EXPECT_NE(station.cityName, "TooSmall");
   }
}

TEST(StationCatalogTest, AssignsProjectedPositions)
{
   const std::string jsonText =
      "{\"stations\":[{\"id\":0,\"cityName\":\"Berlin\",\"stationName\":\"Berlin Hbf\","
      "\"latitude\":52.525,\"longitude\":13.369,\"population\":3600000}]}";
   MiniDb::StationRecordList stations;
   ASSERT_TRUE(MiniDb::IsOk(MiniDb::LoadStationCatalogFromString(jsonText, stations)));
   ASSERT_EQ(stations.size(), 1u);
   EXPECT_GT(stations[0].position.xKm, 0.0f);
   EXPECT_GT(stations[0].position.yKm, 0.0f);
}

TEST(StationCatalogTest, RejectsEmptyInput)
{
   MiniDb::StationRecordList stations;
   EXPECT_TRUE(MiniDb::IsErr(MiniDb::LoadStationCatalogFromString("", stations)));
}

TEST(StationCatalogTest, LoadsUmlautCityNamesFromCatalog)
{
   const std::string filePath = std::string(MINIDB_SOURCE_DIR) + "/data/stations.json";
   MiniDb::StationRecordList stations;
   ASSERT_TRUE(MiniDb::IsOk(MiniDb::LoadStationCatalogFromFile(filePath, stations)));

   std::string koeln;
   koeln.push_back('K');
   koeln.push_back(static_cast<char>(0xC3));
   koeln.push_back(static_cast<char>(0xB6));
   koeln.push_back('l');
   koeln.push_back('n');

   std::string luebeck;
   luebeck.push_back('L');
   luebeck.push_back(static_cast<char>(0xC3));
   luebeck.push_back(static_cast<char>(0xBC));
   luebeck.push_back('b');
   luebeck.push_back('e');
   luebeck.push_back('c');
   luebeck.push_back('k');

   bool foundKoeln = false;
   bool foundLuebeck = false;
   for (const MiniDb::StationRecord& station : stations)
   {
      if (station.cityName == koeln)
      {
         foundKoeln = true;
      }
      if (station.cityName == luebeck)
      {
         foundLuebeck = true;
      }
   }

   EXPECT_TRUE(foundKoeln);
   EXPECT_TRUE(foundLuebeck);
}
