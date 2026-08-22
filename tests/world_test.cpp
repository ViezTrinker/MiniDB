/*!
 *\file world_test.cpp
 *\brief Tests for world spawn order, lines and transfers.
 */

#include <gtest/gtest.h>

#include "core/constants.h"
#include "simulation/train.h"
#include "simulation/world.h"

namespace
{
   const char CatalogJson[] =
      "{"
      "\"stations\":["
      "{\"id\":0,\"cityName\":\"Berlin\",\"stationName\":\"Berlin\","
      "\"latitude\":52.52,\"longitude\":13.40,\"population\":3600000},"
      "{\"id\":1,\"cityName\":\"Hamburg\",\"stationName\":\"Hamburg\","
      "\"latitude\":53.55,\"longitude\":10.00,\"population\":1800000},"
      "{\"id\":2,\"cityName\":\"Munich\",\"stationName\":\"Munich\","
      "\"latitude\":48.14,\"longitude\":11.58,\"population\":1500000},"
      "{\"id\":3,\"cityName\":\"Alpha\",\"stationName\":\"Alpha\","
      "\"latitude\":51.00,\"longitude\":10.00,\"population\":100000},"
      "{\"id\":4,\"cityName\":\"Beta\",\"stationName\":\"Beta\","
      "\"latitude\":51.01,\"longitude\":10.00,\"population\":90000},"
      "{\"id\":5,\"cityName\":\"Gamma\",\"stationName\":\"Gamma\","
      "\"latitude\":51.02,\"longitude\":10.00,\"population\":80000},"
      "{\"id\":6,\"cityName\":\"Delta\",\"stationName\":\"Delta\","
      "\"latitude\":51.03,\"longitude\":10.00,\"population\":70000}"
      "]"
      "}";
} // namespace

TEST(WorldTest, SpawnOrderFollowsPopulation)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));
   ASSERT_EQ(world.GetNetwork().GetStations().size(), MiniDb::InitialStationCount);
   EXPECT_EQ(world.GetNetwork().GetStations()[0].cityName, "Berlin");
   EXPECT_EQ(world.GetNetwork().GetStations()[1].cityName, "Hamburg");
}

TEST(WorldTest, SpawnsFurtherStationsOverTime)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));
   const uint32_t beforeCount = static_cast<uint32_t>(world.GetNetwork().GetStations().size());
   world.Tick(MiniDb::StationSpawnIntervalSeconds + 0.1f);
   EXPECT_GT(static_cast<uint32_t>(world.GetNetwork().GetStations().size()), beforeCount);
}

TEST(WorldTest, PassengerTransfersBetweenLines)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList firstLine;
   firstLine.push_back(3);
   firstLine.push_back(4);
   MiniDb::LineId firstLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(firstLine, firstLineId)));

   MiniDb::StationIdList secondLine;
   secondLine.push_back(4);
   secondLine.push_back(5);
   MiniDb::LineId secondLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(secondLine, secondLineId)));

   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 5)));

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

TEST(WorldTest, StationCapStopsFurtherSpawns)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.SetMaxStationCount(MiniDb::InitialStationCount);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));
   EXPECT_EQ(world.GetNetwork().GetStations().size(), MiniDb::InitialStationCount);
   EXPECT_EQ(world.GetStationCap(), MiniDb::InitialStationCount);

   world.Tick(MiniDb::StationSpawnIntervalSeconds + 0.1f);
   EXPECT_EQ(world.GetNetwork().GetStations().size(), MiniDb::InitialStationCount);
   EXPECT_TRUE(MiniDb::IsErr(world.SpawnNextStation()));
}

TEST(WorldTest, StationCapBelowInitialCount)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.SetMaxStationCount(3);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));
   EXPECT_EQ(world.GetNetwork().GetStations().size(), 3u);
   EXPECT_EQ(world.GetStationCap(), 3u);
}

TEST(WorldTest, ResetSimulationKeepsCatalogAndClearsNetwork)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));
   EXPECT_EQ(world.GetCatalogStationCount(), 7u);
   EXPECT_FALSE(world.GetNetwork().GetStations().empty());

   world.ResetSimulation();
   EXPECT_EQ(world.GetCatalogStationCount(), 7u);
   EXPECT_TRUE(world.GetNetwork().GetStations().empty());
   EXPECT_EQ(world.GetSimulationTimeSeconds(), 0.0f);

   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));
   EXPECT_EQ(world.GetNetwork().GetStations().size(), MiniDb::InitialStationCount);
}

TEST(WorldTest, CatalogCountMatchesLoadedStations)
{
   MiniDb::World world(1);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   EXPECT_EQ(world.GetCatalogStationCount(), 7u);
}

TEST(WorldTest, CollectWaitingDemandGroupsAndSortsDestinations)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 4)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 4)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 4)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 5)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 5)));

   MiniDb::DestinationDemandList demand;
   ASSERT_TRUE(MiniDb::IsOk(world.CollectWaitingDemand(3, demand)));
   ASSERT_EQ(demand.size(), 2u);
   EXPECT_EQ(demand[0].destinationId, 4u);
   EXPECT_EQ(demand[0].waitingCount, 3u);
   EXPECT_EQ(demand[1].destinationId, 5u);
   EXPECT_EQ(demand[1].waitingCount, 2u);
}

TEST(WorldTest, CollectWaitingDemandRejectsUnknownStation)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::DestinationDemandList demand;
   EXPECT_TRUE(MiniDb::IsErr(world.CollectWaitingDemand(99, demand)));
}

TEST(WorldTest, InsertStationOnLineUpdatesOrder)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(3);
   stationIds.push_back(4);
   stationIds.push_back(5);
   stationIds.push_back(6);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_TRUE(MiniDb::IsOk(world.InsertStationOnLine(lineId, 1, 2)));

   const MiniDb::Line* pLine = world.GetNetwork().FindLine(lineId);
   ASSERT_NE(pLine, nullptr);
   ASSERT_EQ(pLine->stationIds.size(), 5u);
   EXPECT_EQ(pLine->stationIds[0], 3u);
   EXPECT_EQ(pLine->stationIds[1], 4u);
   EXPECT_EQ(pLine->stationIds[2], 2u);
   EXPECT_EQ(pLine->stationIds[3], 5u);
   EXPECT_EQ(pLine->stationIds[4], 6u);
}

TEST(WorldTest, CollectUnconnectedStationsOmitsLinedStations)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(0);
   stationIds.push_back(1);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));

   MiniDb::StationIdList unconnected;
   ASSERT_TRUE(MiniDb::IsOk(world.CollectUnconnectedStations(unconnected)));
   ASSERT_EQ(unconnected.size(), 1u);
   EXPECT_EQ(unconnected[0], 2u);
}

TEST(WorldTest, AddTrainToLineAtPlacesTrainOnDroppedSegment)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(3);
   stationIds.push_back(4);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));

   const MiniDb::StationRecord* pFrom = world.GetNetwork().FindStation(3);
   const MiniDb::StationRecord* pTo = world.GetNetwork().FindStation(4);
   ASSERT_NE(pFrom, nullptr);
   ASSERT_NE(pTo, nullptr);

   MiniDb::MapPoint dropPoint;
   dropPoint.xKm = (pFrom->position.xKm + pTo->position.xKm) * 0.5f;
   dropPoint.yKm = (pFrom->position.yKm + pTo->position.yKm) * 0.5f;
   ASSERT_TRUE(MiniDb::IsOk(world.AddTrainToLineAt(lineId, dropPoint)));
   ASSERT_EQ(world.GetTrains().size(), 2u);

   const MiniDb::Train& droppedTrain = world.GetTrains().back();
   EXPECT_EQ(droppedTrain.lineId, lineId);
   EXPECT_EQ(droppedTrain.fromIndex, 0);
   EXPECT_GT(droppedTrain.distanceFromFromStationKm, 0.0f);
}

TEST(WorldTest, HitTestTrainFindsTrainAtMapPosition)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(3);
   stationIds.push_back(0);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));
   ASSERT_FALSE(world.GetTrains().empty());

   const MiniDb::Train& train = world.GetTrains().front();
   const MiniDb::Line* pLine = world.GetNetwork().FindLine(lineId);
   ASSERT_NE(pLine, nullptr);
   const MiniDb::MapPoint trainPosition = MiniDb::TrainMapPosition(train, *pLine, world.GetNetwork());
   EXPECT_EQ(world.HitTestTrain(trainPosition, 2.0f), train.id);

   MiniDb::MapPoint farPoint;
   farPoint.xKm = trainPosition.xKm + 80.0f;
   farPoint.yKm = trainPosition.yKm + 80.0f;
   EXPECT_EQ(world.HitTestTrain(farPoint, 2.0f), MiniDb::InvalidTrainId);
}

TEST(WorldTest, CollectOnboardDemandReportsTransfer)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList firstLine;
   firstLine.push_back(3);
   firstLine.push_back(4);
   MiniDb::LineId firstLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(firstLine, firstLineId)));

   MiniDb::StationIdList secondLine;
   secondLine.push_back(4);
   secondLine.push_back(5);
   MiniDb::LineId secondLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(secondLine, secondLineId)));

   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 5)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 4)));
   world.Tick(0.0f);

   ASSERT_FALSE(world.GetTrains().empty());
   MiniDb::OnboardDemandList demand;
   ASSERT_TRUE(MiniDb::IsOk(world.CollectOnboardDemand(world.GetTrains().front().id, demand)));
   ASSERT_EQ(demand.size(), 2u);

   bool foundTransfer = false;
   bool foundDirect = false;
   for (const MiniDb::OnboardDemand& entry : demand)
   {
      if (entry.destinationId == 5u)
      {
         EXPECT_EQ(entry.transferStationId, 4u);
         EXPECT_EQ(entry.passengerCount, 1u);
         foundTransfer = true;
      }
      if (entry.destinationId == 4u)
      {
         EXPECT_EQ(entry.transferStationId, MiniDb::InvalidStationId);
         EXPECT_EQ(entry.passengerCount, 1u);
         foundDirect = true;
      }
   }

   EXPECT_TRUE(foundTransfer);
   EXPECT_TRUE(foundDirect);
}

TEST(WorldTest, CollectOnboardDemandRejectsUnknownTrain)
{
   MiniDb::World world(1);
   MiniDb::OnboardDemandList demand;
   EXPECT_TRUE(MiniDb::IsErr(world.CollectOnboardDemand(99, demand)));
}

TEST(WorldTest, FindNearestLineHitsSegmentAndMissesFarPoint)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(3);
   stationIds.push_back(4);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));

   const MiniDb::StationRecord* pFrom = world.GetNetwork().FindStation(3);
   const MiniDb::StationRecord* pTo = world.GetNetwork().FindStation(4);
   ASSERT_NE(pFrom, nullptr);
   ASSERT_NE(pTo, nullptr);

   MiniDb::MapPoint midpoint;
   midpoint.xKm = (pFrom->position.xKm + pTo->position.xKm) * 0.5f;
   midpoint.yKm = (pFrom->position.yKm + pTo->position.yKm) * 0.5f;
   EXPECT_EQ(world.FindNearestLine(midpoint, 2.0f), lineId);

   MiniDb::MapPoint farPoint;
   farPoint.xKm = midpoint.xKm + 80.0f;
   farPoint.yKm = midpoint.yKm + 80.0f;
   EXPECT_EQ(world.FindNearestLine(farPoint, 2.0f), MiniDb::InvalidLineId);
}

TEST(WorldTest, RemoveLineDropsTrainsAndUnloadsPassengers)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList firstStations;
   firstStations.push_back(3);
   firstStations.push_back(4);
   MiniDb::LineId firstLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(firstStations, firstLineId)));

   MiniDb::StationIdList secondStations;
   secondStations.push_back(5);
   secondStations.push_back(6);
   MiniDb::LineId secondLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(secondStations, secondLineId)));

   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 4)));

   bool boarded = false;
   for (uint32_t step = 0; step < 400; ++step)
   {
      world.Tick(0.05f);
      if (world.GetOnboardPassengerCount() >= 1)
      {
         boarded = true;
         break;
      }
   }

   ASSERT_TRUE(boarded);
   ASSERT_TRUE(MiniDb::IsOk(world.RemoveLine(firstLineId)));
   EXPECT_EQ(world.GetNetwork().FindLine(firstLineId), nullptr);
   EXPECT_NE(world.GetNetwork().FindLine(secondLineId), nullptr);
   EXPECT_EQ(world.GetOnboardPassengerCount(), 0u);
   EXPECT_EQ(world.GetWaitingPassengerCount(), 1u);
   ASSERT_FALSE(world.GetPassengers().empty());
   EXPECT_EQ(world.GetPassengers()[0].state, MiniDb::PassengerState::Waiting);
   EXPECT_EQ(world.GetPassengers()[0].trainId, MiniDb::InvalidTrainId);

   uint32_t remainingTrainCount = 0;
   for (const MiniDb::Train& train : world.GetTrains())
   {
      EXPECT_NE(train.lineId, firstLineId);
      ++remainingTrainCount;
   }
   EXPECT_EQ(remainingTrainCount, 1u);

   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(5, 6)));
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

TEST(WorldTest, RemoveLineRejectsUnknownId)
{
   MiniDb::World world(1);
   EXPECT_TRUE(MiniDb::IsErr(world.RemoveLine(99)));
}

TEST(WorldTest, BoardingPrefersEarlierPlatformArrival)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   world.SetTrainCapacity(1);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList feederStations;
   feederStations.push_back(3);
   feederStations.push_back(4);
   MiniDb::LineId feederLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(feederStations, feederLineId)));

   MiniDb::StationIdList longStations;
   longStations.push_back(0);
   longStations.push_back(4);
   MiniDb::LineId longLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(longStations, longLineId)));

   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 0)));

   bool transferOnboard = false;
   for (uint32_t step = 0; step < 400; ++step)
   {
      world.Tick(0.05f);
      for (const MiniDb::Passenger& passenger : world.GetPassengers())
      {
         if (passenger.originId == 3 && passenger.state == MiniDb::PassengerState::Onboard)
         {
            transferOnboard = true;
            break;
         }
      }
      if (transferOnboard)
      {
         break;
      }
   }
   ASSERT_TRUE(transferOnboard);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(4, 0)));

   bool transferWaitingAtFour = false;
   for (uint32_t step = 0; step < 800; ++step)
   {
      world.Tick(0.05f);
      for (const MiniDb::Passenger& passenger : world.GetPassengers())
      {
         if (passenger.originId == 3 &&
            passenger.state == MiniDb::PassengerState::Waiting &&
            passenger.currentStationId == 4)
         {
            transferWaitingAtFour = true;
            break;
         }
      }
      if (transferWaitingAtFour)
      {
         break;
      }
   }
   ASSERT_TRUE(transferWaitingAtFour);

   bool localStillWaiting = false;
   for (const MiniDb::Passenger& passenger : world.GetPassengers())
   {
      if (passenger.originId == 4 &&
         passenger.state == MiniDb::PassengerState::Waiting &&
         passenger.currentStationId == 4)
      {
         localStillWaiting = true;
      }
   }
   ASSERT_TRUE(localStillWaiting);

   bool boardedSomeone = false;
   uint32_t localOnboard = 0;
   uint32_t transferOnLongLine = 0;
   for (uint32_t step = 0; step < 4000; ++step)
   {
      world.Tick(0.05f);
      localOnboard = 0;
      transferOnLongLine = 0;
      for (const MiniDb::Passenger& passenger : world.GetPassengers())
      {
         if (passenger.state != MiniDb::PassengerState::Onboard)
         {
            continue;
         }
         if (passenger.originId == 4)
         {
            ++localOnboard;
         }
         if (passenger.originId == 3)
         {
            ++transferOnLongLine;
         }
      }
      if ((localOnboard + transferOnLongLine) >= 1)
      {
         boardedSomeone = true;
         break;
      }
   }

   EXPECT_TRUE(boardedSomeone);
   EXPECT_EQ(localOnboard, 1u);
   EXPECT_EQ(transferOnLongLine, 0u);
}
