/*!
 *\file gravity_model_test.cpp
 *\brief Tests for gravity-based destination choice.
 */

#include <gtest/gtest.h>

#include "simulation/gravity_model.h"
#include "test_helpers.h"

TEST(GravityModelTest, LargeCityBeatsSmallCityAtSimilarDistance)
{
   MiniDb::StationRecordList stations;
   stations.push_back(MiniDb::Test::MakeStation(0, "Origin", 51.00f, 10.00f, 15000));
   stations.push_back(MiniDb::Test::MakeStation(1, "Berlin", 52.52f, 13.40f, 3600000));
   stations.push_back(MiniDb::Test::MakeStation(2, "SmallWest", 51.00f, 6.80f, 15000));

   const MiniDb::GravityParameters parameters = MiniDb::DefaultGravityParameters();
   const float berlinProbability = MiniDb::GravityProbability(0, 1, stations, parameters);
   const float smallProbability = MiniDb::GravityProbability(0, 2, stations, parameters);

   EXPECT_GT(berlinProbability, smallProbability * 5.0f);
}

TEST(GravityModelTest, NearCityBeatsFarCityOfEqualSize)
{
   MiniDb::StationRecordList stations;
   stations.push_back(MiniDb::Test::MakeStation(0, "Origin", 50.00f, 8.00f, 20000));
   stations.push_back(MiniDb::Test::MakeStation(1, "Near", 50.20f, 8.20f, 20000));
   stations.push_back(MiniDb::Test::MakeStation(2, "Far", 53.00f, 13.00f, 20000));

   const MiniDb::GravityParameters parameters = MiniDb::DefaultGravityParameters();
   const float nearProbability = MiniDb::GravityProbability(0, 1, stations, parameters);
   const float farProbability = MiniDb::GravityProbability(0, 2, stations, parameters);

   EXPECT_GT(nearProbability, farProbability);
}

TEST(GravityModelTest, OriginHasZeroProbabilityAsDestination)
{
   MiniDb::StationRecordList stations;
   stations.push_back(MiniDb::Test::MakeStation(0, "Origin", 50.00f, 8.00f, 20000));
   stations.push_back(MiniDb::Test::MakeStation(1, "Other", 51.00f, 9.00f, 20000));

   const MiniDb::GravityParameters parameters = MiniDb::DefaultGravityParameters();
   const float selfProbability = MiniDb::GravityProbability(0, 0, stations, parameters);
   EXPECT_FLOAT_EQ(selfProbability, 0.0f);
}

TEST(GravityModelTest, PickDestinationReturnsOtherStation)
{
   MiniDb::StationRecordList stations;
   stations.push_back(MiniDb::Test::MakeStation(0, "Origin", 50.00f, 8.00f, 20000));
   stations.push_back(MiniDb::Test::MakeStation(1, "Other", 51.00f, 9.00f, 20000));

   MiniDb::StationId destinationId = MiniDb::InvalidStationId;
   const MiniDb::Result result = MiniDb::PickGravityDestination(
      0,
      stations,
      MiniDb::DefaultGravityParameters(),
      0.5f,
      destinationId);
   EXPECT_TRUE(MiniDb::IsOk(result));
   EXPECT_EQ(destinationId, 1u);
}
