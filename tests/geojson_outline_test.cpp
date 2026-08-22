/*!
 *\file geojson_outline_test.cpp
 *\brief Tests for GeoJSON outline loading.
 */

#include <gtest/gtest.h>

#include <string>

#include "geo/geojson_outline.h"

TEST(GeoJsonOutlineTest, LoadsPolygonFromString)
{
   const char jsonText[] =
      "{"
      "\"type\":\"Polygon\","
      "\"coordinates\":[[[8.0,50.0],[9.0,50.0],[9.0,51.0],[8.0,51.0],[8.0,50.0]]]"
      "}";

   MiniDb::MapPolygonList polygons;
   const MiniDb::Result result = MiniDb::LoadOutlineFromString(jsonText, polygons);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(polygons.size(), 1u);
   EXPECT_GE(polygons[0].size(), 4u);
}

TEST(GeoJsonOutlineTest, RejectsEmptyDocument)
{
   MiniDb::MapPolygonList polygons;
   const MiniDb::Result result = MiniDb::LoadOutlineFromString("", polygons);
   EXPECT_TRUE(MiniDb::IsErr(result));
}

TEST(GeoJsonOutlineTest, LoadsGermanyOutlineFile)
{
   const std::string filePath = std::string(MINIDB_SOURCE_DIR) + "/data/germany.geojson";
   MiniDb::MapPolygonList polygons;
   const MiniDb::Result result = MiniDb::LoadOutlineFromFile(filePath, polygons);
   EXPECT_TRUE(MiniDb::IsOk(result));
   EXPECT_FALSE(polygons.empty());
   EXPECT_GE(polygons[0].size(), 20u);
}
