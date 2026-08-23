/*!
 *\file train_test.cpp
 *\brief Tests for real-time train motion and boarding.
 */

#include <gtest/gtest.h>

#include "simulation/world.h"
#include "test_helpers.h"

namespace
{
   const char TinyCatalogJson[] =
      "{"
      "\"stations\":["
      "{\"id\":0,\"cityName\":\"Alpha\",\"stationName\":\"Alpha\","
      "\"latitude\":51.00,\"longitude\":10.00,\"population\":100000},"
      "{\"id\":1,\"cityName\":\"Beta\",\"stationName\":\"Beta\","
      "\"latitude\":51.01,\"longitude\":10.00,\"population\":100000},"
      "{\"id\":2,\"cityName\":\"Gamma\",\"stationName\":\"Gamma\","
      "\"latitude\":51.02,\"longitude\":10.00,\"population\":100000}"
      "]"
      "}";

   MiniDb::World MakeTinyWorld(void)
   {
      MiniDb::World world(42);
      world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
      const MiniDb::Result loadResult = world.LoadCatalogFromString(TinyCatalogJson);
      EXPECT_TRUE(MiniDb::IsOk(loadResult));
      EXPECT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
      EXPECT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
      EXPECT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
      return world;
   }
} // namespace

TEST(TrainTest, PassengerArrivesOnDirectLine)
{
   MiniDb::World world = MakeTinyWorld();
   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(0, 1)));

   bool arrived = false;
   for (uint32_t step = 0; step < 400; ++step)
   {
      world.Tick(0.05f);
      if (world.GetArrivedPassengerCount() >= 1)
      {
         arrived = true;
         break;
      }
   }

   EXPECT_TRUE(arrived);
   EXPECT_EQ(world.GetWaitingPassengerCount() + world.GetOnboardPassengerCount(), 0u);
}

TEST(TrainTest, CapacityLimitLeavesPassengerWaiting)
{
   MiniDb::World world = MakeTinyWorld();
   world.SetTrainCapacity(1);

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(0, 1)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(0, 1)));

   world.Tick(0.0f);
   EXPECT_EQ(world.GetOnboardPassengerCount(), 1u);
   EXPECT_EQ(world.GetWaitingPassengerCount(), 1u);
}

TEST(TrainTest, LoopTrainKeepsDirectionAndWraps)
{
   MiniDb::World world = MakeTinyWorld();
   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   stationIds.push_back(0);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_FALSE(world.GetTrains().empty());

   const int32_t initialDirection = world.GetTrains()[0].direction;
   bool visitedLast = false;
   bool wrappedToFirst = false;
   for (uint32_t step = 0; step < 800; ++step)
   {
      world.Tick(0.05f);
      EXPECT_EQ(world.GetTrains()[0].direction, initialDirection);
      if (world.GetTrains()[0].fromIndex == 2)
      {
         visitedLast = true;
      }
      if (visitedLast && world.GetTrains()[0].fromIndex == 0)
      {
         wrappedToFirst = true;
         break;
      }
   }

   EXPECT_TRUE(visitedLast);
   EXPECT_TRUE(wrappedToFirst);
}

TEST(TrainTest, PassengerUsesLoopCloser)
{
   MiniDb::World world = MakeTinyWorld();
   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   stationIds.push_back(0);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(2, 0)));

   bool arrived = false;
   for (uint32_t step = 0; step < 400; ++step)
   {
      world.Tick(0.05f);
      if (world.GetArrivedPassengerCount() >= 1)
      {
         arrived = true;
         break;
      }
   }

   EXPECT_TRUE(arrived);
}

TEST(TrainTest, PassengerOnLoopTakesForwardWayAround)
{
   MiniDb::World world = MakeTinyWorld();
   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   stationIds.push_back(0);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(1, 0)));

   bool arrived = false;
   for (uint32_t step = 0; step < 800; ++step)
   {
      world.Tick(0.05f);
      if (world.GetArrivedPassengerCount() >= 1)
      {
         arrived = true;
         break;
      }
   }

   EXPECT_TRUE(arrived);
}

TEST(TrainTest, TrainReversesAtTerminal)
{
   MiniDb::World world = MakeTinyWorld();
   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_FALSE(world.GetTrains().empty());

   const int32_t initialDirection = world.GetTrains()[0].direction;
   bool reversed = false;
   for (uint32_t step = 0; step < 400; ++step)
   {
      world.Tick(0.05f);
      if (world.GetTrains()[0].direction != initialDirection)
      {
         reversed = true;
         break;
      }
   }

   EXPECT_TRUE(reversed);
}
