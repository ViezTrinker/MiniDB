/*!
 *\file pathfinder_test.cpp
 *\brief Tests for passenger pathfinding.
 */

#include <gtest/gtest.h>

#include "core/constants.h"
#include "simulation/network.h"
#include "simulation/pathfinder.h"
#include "test_helpers.h"

TEST(PathfinderTest, DirectLineProducesAdjacentHops)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(stationIds, 0, lineId)));

   MiniDb::Route route;
   MiniDb::LineWaitList lineWaits;
   const MiniDb::Result result = MiniDb::FindRoute(network, 0, 2, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(route.size(), 2u);
   EXPECT_EQ(route[0].fromStationId, 0u);
   EXPECT_EQ(route[0].toStationId, 1u);
   EXPECT_EQ(route[0].lineId, lineId);
   EXPECT_EQ(route[1].fromStationId, 1u);
   EXPECT_EQ(route[1].toStationId, 2u);
   EXPECT_EQ(route[1].lineId, lineId);
}

TEST(PathfinderTest, TransferUsesTwoLines)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));

   MiniDb::StationIdList firstLineStations;
   firstLineStations.push_back(0);
   firstLineStations.push_back(1);
   MiniDb::LineId firstLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(firstLineStations, 0, firstLineId)));

   MiniDb::StationIdList secondLineStations;
   secondLineStations.push_back(1);
   secondLineStations.push_back(2);
   MiniDb::LineId secondLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(secondLineStations, 1, secondLineId)));

   MiniDb::Route route;
   MiniDb::LineWaitList lineWaits;
   const MiniDb::Result result = MiniDb::FindRoute(network, 0, 2, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(route.size(), 2u);
   EXPECT_EQ(route[0].lineId, firstLineId);
   EXPECT_EQ(route[1].lineId, secondLineId);
}

TEST(PathfinderTest, UnconnectedDestinationHasNoRoute)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(stationIds, 0, lineId)));

   MiniDb::Route route;
   MiniDb::LineWaitList lineWaits;
   const MiniDb::Result result = MiniDb::FindRoute(network, 0, 2, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsErr(result));
   EXPECT_TRUE(route.empty());
}

TEST(PathfinderTest, LoopConnectsLastStationToFirst)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   stationIds.push_back(0);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(stationIds, 0, lineId)));

   MiniDb::Route route;
   MiniDb::LineWaitList lineWaits;
   const MiniDb::Result result = MiniDb::FindRoute(network, 2, 0, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(route.size(), 1u);
   EXPECT_EQ(route[0].fromStationId, 2u);
   EXPECT_EQ(route[0].toStationId, 0u);
   EXPECT_EQ(route[0].lineId, lineId);
}

TEST(PathfinderTest, LoopRouteFollowsTrainDirection)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   stationIds.push_back(0);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(stationIds, 0, lineId)));

   MiniDb::Route route;
   MiniDb::LineWaitList lineWaits;
   const MiniDb::Result result = MiniDb::FindRoute(network, 1, 0, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(route.size(), 2u);
   EXPECT_EQ(route[0].fromStationId, 1u);
   EXPECT_EQ(route[0].toStationId, 2u);
   EXPECT_EQ(route[0].lineId, lineId);
   EXPECT_EQ(route[1].fromStationId, 2u);
   EXPECT_EQ(route[1].toStationId, 0u);
   EXPECT_EQ(route[1].lineId, lineId);
}

TEST(PathfinderTest, SameStationReturnsEmptyRoute)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));

   MiniDb::Route route;
   MiniDb::LineWaitList lineWaits;
   const MiniDb::Result result = MiniDb::FindRoute(network, 0, 0, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsOk(result));
   EXPECT_TRUE(route.empty());
}

TEST(PathfinderTest, PrefersLineWithShorterWait)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));

   MiniDb::StationIdList firstStations;
   firstStations.push_back(0);
   firstStations.push_back(1);
   MiniDb::LineId slowLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(firstStations, 0, slowLineId)));

   MiniDb::StationIdList secondStations;
   secondStations.push_back(0);
   secondStations.push_back(1);
   MiniDb::LineId fastLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(secondStations, 1, fastLineId)));

   MiniDb::LineWaitList lineWaits;
   MiniDb::LineWait slowWait;
   slowWait.lineId = slowLineId;
   slowWait.waitSeconds = 40.0f;
   lineWaits.push_back(slowWait);
   MiniDb::LineWait fastWait;
   fastWait.lineId = fastLineId;
   fastWait.waitSeconds = 2.0f;
   lineWaits.push_back(fastWait);

   MiniDb::Route route;
   const MiniDb::Result result = MiniDb::FindRoute(network, 0, 1, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(route.size(), 1u);
   EXPECT_EQ(route[0].lineId, fastLineId);
}

TEST(PathfinderTest, PrefersDirectShuttleOverLongLoop)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "Berlin", 52.52f, 13.40f, 3600000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "Hamburg", 53.55f, 10.00f, 1800000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "Munich", 48.14f, 11.58f, 1500000))));

   MiniDb::StationIdList loopStations;
   loopStations.push_back(1);
   loopStations.push_back(2);
   loopStations.push_back(0);
   loopStations.push_back(1);
   MiniDb::LineId loopLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(loopStations, 0, loopLineId)));

   MiniDb::StationIdList shuttleStations;
   shuttleStations.push_back(1);
   shuttleStations.push_back(0);
   MiniDb::LineId shuttleLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(shuttleStations, 1, shuttleLineId)));

   const MiniDb::Line* pLoop = network.FindLine(loopLineId);
   const MiniDb::Line* pShuttle = network.FindLine(shuttleLineId);
   ASSERT_NE(pLoop, nullptr);
   ASSERT_NE(pShuttle, nullptr);

   MiniDb::LineWaitList lineWaits;
   MiniDb::LineWait loopWait;
   loopWait.lineId = loopLineId;
   loopWait.waitSeconds = MiniDb::ExpectedLineWaitSeconds(
      MiniDb::LineCycleTimeSeconds(network, *pLoop),
      1);
   lineWaits.push_back(loopWait);
   MiniDb::LineWait shuttleWait;
   shuttleWait.lineId = shuttleLineId;
   shuttleWait.waitSeconds = MiniDb::ExpectedLineWaitSeconds(
      MiniDb::LineCycleTimeSeconds(network, *pShuttle),
      1);
   lineWaits.push_back(shuttleWait);

   MiniDb::Route route;
   const MiniDb::Result result = MiniDb::FindRoute(network, 0, 1, lineWaits, route);
   EXPECT_TRUE(MiniDb::IsOk(result));
   ASSERT_EQ(route.size(), 1u);
   EXPECT_EQ(route[0].lineId, shuttleLineId);
}

TEST(PathfinderTest, ExpectedWaitIsHalfHeadway)
{
   EXPECT_FLOAT_EQ(MiniDb::ExpectedLineWaitSeconds(20.0f, 2), 5.0f);
   EXPECT_FLOAT_EQ(MiniDb::ExpectedLineWaitSeconds(20.0f, 0), 20.0f);
}

TEST(PathfinderTest, LoopCycleTimeIsOneCircuit)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   stationIds.push_back(0);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(stationIds, 0, lineId)));
   const MiniDb::Line* pLine = network.FindLine(lineId);
   ASSERT_NE(pLine, nullptr);

   const float cycleTimeSeconds = MiniDb::LineCycleTimeSeconds(network, *pLine);
   const MiniDb::StationRecord* pFirst = network.FindStation(0);
   const MiniDb::StationRecord* pSecond = network.FindStation(1);
   const MiniDb::StationRecord* pThird = network.FindStation(2);
   ASSERT_NE(pFirst, nullptr);
   ASSERT_NE(pSecond, nullptr);
   ASSERT_NE(pThird, nullptr);

   const float abSeconds =
      (MiniDb::DistanceKm(pFirst->position, pSecond->position) / MiniDb::TrainSpeedKmPerHour) *
      MiniDb::SecondsPerHour;
   const float bcSeconds =
      (MiniDb::DistanceKm(pSecond->position, pThird->position) / MiniDb::TrainSpeedKmPerHour) *
      MiniDb::SecondsPerHour;
   const float caSeconds =
      (MiniDb::DistanceKm(pThird->position, pFirst->position) / MiniDb::TrainSpeedKmPerHour) *
      MiniDb::SecondsPerHour;
   const float expectedSeconds =
      abSeconds + bcSeconds + caSeconds + (3.0f * MiniDb::TrainDwellSeconds);
   EXPECT_NEAR(cycleTimeSeconds, expectedSeconds, 0.001f);
}
