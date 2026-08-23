/*!
 *\file network_test.cpp
 *\brief Tests for line insert, loops and unconnected stations.
 */

#include <gtest/gtest.h>

#include "simulation/network.h"
#include "test_helpers.h"

TEST(NetworkTest, InsertStationBetweenSegmentEnds)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(3, "D", 51.30f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(4, "P", 51.15f, 10.20f, 100000))));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   stationIds.push_back(3);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(stationIds, 0, lineId)));
   ASSERT_TRUE(MiniDb::IsOk(network.InsertStationOnLine(lineId, 1, 4)));

   const MiniDb::Line* pLine = network.FindLine(lineId);
   ASSERT_NE(pLine, nullptr);
   ASSERT_EQ(pLine->stationIds.size(), 5u);
   EXPECT_EQ(pLine->stationIds[0], 0u);
   EXPECT_EQ(pLine->stationIds[1], 1u);
   EXPECT_EQ(pLine->stationIds[2], 4u);
   EXPECT_EQ(pLine->stationIds[3], 2u);
   EXPECT_EQ(pLine->stationIds[4], 3u);
}

TEST(NetworkTest, ClosedStationListCreatesLoop)
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
   EXPECT_EQ(pLine->loop, MiniDb::LineLoop::Yes);
   ASSERT_EQ(pLine->stationIds.size(), 3u);
   EXPECT_EQ(MiniDb::LineSegmentCount(*pLine), 3u);
}

TEST(NetworkTest, ExtendToFirstStationClosesLoop)
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
   ASSERT_TRUE(MiniDb::IsOk(network.ExtendLine(lineId, 0)));

   const MiniDb::Line* pLine = network.FindLine(lineId);
   ASSERT_NE(pLine, nullptr);
   EXPECT_EQ(pLine->loop, MiniDb::LineLoop::Yes);
   EXPECT_EQ(pLine->stationIds.size(), 3u);
}

TEST(NetworkTest, PrependStationAddsAtFront)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(3, "X", 50.90f, 10.00f, 100000))));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   stationIds.push_back(2);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(stationIds, 0, lineId)));
   ASSERT_TRUE(MiniDb::IsOk(network.ExtendLineAt(lineId, MiniDb::LineEnd::Front, 3)));

   const MiniDb::Line* pLine = network.FindLine(lineId);
   ASSERT_NE(pLine, nullptr);
   ASSERT_EQ(pLine->stationIds.size(), 4u);
   EXPECT_EQ(pLine->stationIds[0], 3u);
   EXPECT_EQ(pLine->stationIds[1], 0u);
   EXPECT_EQ(pLine->stationIds[2], 1u);
   EXPECT_EQ(pLine->stationIds[3], 2u);
}

TEST(NetworkTest, PrependRejectsDuplicateStation)
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
   EXPECT_TRUE(MiniDb::IsErr(network.ExtendLineAt(lineId, MiniDb::LineEnd::Front, 1)));
}

TEST(NetworkTest, PrependToBackTerminusClosesLoop)
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
   ASSERT_TRUE(MiniDb::IsOk(network.ExtendLineAt(lineId, MiniDb::LineEnd::Front, 2)));

   const MiniDb::Line* pLine = network.FindLine(lineId);
   ASSERT_NE(pLine, nullptr);
   EXPECT_EQ(pLine->loop, MiniDb::LineLoop::Yes);
   EXPECT_EQ(pLine->stationIds.size(), 3u);
}

TEST(NetworkTest, RejectsInsertOfStationAlreadyOnLine)
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
   EXPECT_TRUE(MiniDb::IsErr(network.InsertStationOnLine(lineId, 0, 2)));
}

TEST(NetworkTest, IsStationOnAnyLine)
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
   EXPECT_TRUE(network.IsStationOnAnyLine(0));
   EXPECT_TRUE(network.IsStationOnAnyLine(1));
   EXPECT_FALSE(network.IsStationOnAnyLine(2));
}

TEST(NetworkTest, RemoveLineDeletesOnlyThatLine)
{
   MiniDb::Network network;
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(0, "A", 51.00f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(1, "B", 51.10f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(2, "C", 51.20f, 10.00f, 100000))));
   ASSERT_TRUE(MiniDb::IsOk(network.AddStation(MiniDb::Test::MakeStation(3, "D", 51.30f, 10.00f, 100000))));

   MiniDb::StationIdList firstStations;
   firstStations.push_back(0);
   firstStations.push_back(1);
   MiniDb::LineId firstLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(firstStations, 0, firstLineId)));

   MiniDb::StationIdList secondStations;
   secondStations.push_back(2);
   secondStations.push_back(3);
   MiniDb::LineId secondLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(network.AddLine(secondStations, 1, secondLineId)));
   const uint32_t createdLineCount = network.GetCreatedLineCount();

   ASSERT_TRUE(MiniDb::IsOk(network.RemoveLine(firstLineId)));
   EXPECT_EQ(network.FindLine(firstLineId), nullptr);
   EXPECT_NE(network.FindLine(secondLineId), nullptr);
   EXPECT_FALSE(network.IsStationOnAnyLine(0));
   EXPECT_FALSE(network.IsStationOnAnyLine(1));
   EXPECT_TRUE(network.IsStationOnAnyLine(2));
   EXPECT_EQ(network.GetCreatedLineCount(), createdLineCount);
}

TEST(NetworkTest, RemoveLineRejectsUnknownId)
{
   MiniDb::Network network;
   EXPECT_TRUE(MiniDb::IsErr(network.RemoveLine(99)));
}
