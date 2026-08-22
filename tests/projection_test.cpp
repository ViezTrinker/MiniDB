/*!
 *\file projection_test.cpp
 *\brief Tests for the WGS84 map projection.
 */

#include <gtest/gtest.h>

#include "core/constants.h"
#include "geo/projection.h"

TEST(ProjectionTest, BerlinIsEastOfCologne)
{
   const MiniDb::MapPoint berlin = MiniDb::ProjectWgs84(52.5200f, 13.4050f);
   const MiniDb::MapPoint cologne = MiniDb::ProjectWgs84(50.9375f, 6.9603f);
   EXPECT_GT(berlin.xKm, cologne.xKm);
}

TEST(ProjectionTest, MunichIsSouthOfHamburg)
{
   const MiniDb::MapPoint munich = MiniDb::ProjectWgs84(48.1351f, 11.5820f);
   const MiniDb::MapPoint hamburg = MiniDb::ProjectWgs84(53.5511f, 9.9937f);
   EXPECT_GT(munich.yKm, hamburg.yKm);
}

TEST(ProjectionTest, MajorCitiesLieOnTheMap)
{
   const MiniDb::MapPoint berlin = MiniDb::ProjectWgs84(52.5200f, 13.4050f);
   const MiniDb::MapPoint munich = MiniDb::ProjectWgs84(48.1351f, 11.5820f);
   const MiniDb::MapPoint hamburg = MiniDb::ProjectWgs84(53.5511f, 9.9937f);
   const MiniDb::MapPoint cologne = MiniDb::ProjectWgs84(50.9375f, 6.9603f);
   const float widthKm = MiniDb::MapWidthKm();
   const float heightKm = MiniDb::MapHeightKm();

   EXPECT_GE(berlin.xKm, 0.0f);
   EXPECT_LE(berlin.xKm, widthKm);
   EXPECT_GE(berlin.yKm, 0.0f);
   EXPECT_LE(berlin.yKm, heightKm);

   EXPECT_GE(munich.xKm, 0.0f);
   EXPECT_LE(munich.xKm, widthKm);
   EXPECT_GE(munich.yKm, 0.0f);
   EXPECT_LE(munich.yKm, heightKm);

   EXPECT_GE(hamburg.xKm, 0.0f);
   EXPECT_LE(hamburg.xKm, widthKm);
   EXPECT_GE(hamburg.yKm, 0.0f);
   EXPECT_LE(hamburg.yKm, heightKm);

   EXPECT_GE(cologne.xKm, 0.0f);
   EXPECT_LE(cologne.xKm, widthKm);
   EXPECT_GE(cologne.yKm, 0.0f);
   EXPECT_LE(cologne.yKm, heightKm);
}

TEST(ProjectionTest, MapSizeIsPositive)
{
   EXPECT_GT(MiniDb::MapWidthKm(), 400.0f);
   EXPECT_GT(MiniDb::MapHeightKm(), 600.0f);
}
