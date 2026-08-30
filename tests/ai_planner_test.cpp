/*!
 *\file ai_planner_test.cpp
 *\brief Tests for spectator AI planning heuristics.
 */

#include <gtest/gtest.h>

#include "ai/ai_observation.h"
#include "ai/ai_planner.h"
#include "ai/ai_scorer.h"
#include "ai/play_agent.h"
#include "core/constants.h"
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
      "\"latitude\":51.01,\"longitude\":10.00,\"population\":90000}"
      "]"
      "}";
} // namespace

TEST(AiPlannerTest, PrefersConnectingLargeCitiesOnEmptyMap)
{
   MiniDb::World world(41);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Sandbox,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));

   MiniDb::AiAction action;
   MiniDb::PlanAiAction(world, action);
   EXPECT_EQ(action.type, MiniDb::AiActionType::AddLine);
   ASSERT_GE(action.stationIds.size(), 2u);
   EXPECT_TRUE(MiniDb::IsOk(MiniDb::ApplyAiAction(world, action)));
   EXPECT_FALSE(world.GetNetwork().GetLines().empty());
}

TEST(AiPlannerTest, AddTrainScoresHighOnBusyLine)
{
   MiniDb::World world(43);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Sandbox,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));

   MiniDb::StationIdList lineStations;
   lineStations.push_back(0);
   lineStations.push_back(1);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(lineStations, lineId)));

   for (uint32_t passengerIndex = 0; passengerIndex < 40; ++passengerIndex)
   {
      ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(0, 1)));
   }

   MiniDb::AiObservation observation;
   MiniDb::BuildAiObservation(world, observation);

   MiniDb::AiAction trainAction;
   trainAction.type = MiniDb::AiActionType::AddTrain;
   trainAction.lineId = lineId;
   MiniDb::ScoreAiAction(world, observation, trainAction);

   MiniDb::AiAction noopAction;
   noopAction.type = MiniDb::AiActionType::Noop;
   MiniDb::ScoreAiAction(world, observation, noopAction);

   EXPECT_GT(trainAction.score, noopAction.score);
}

TEST(AiPlannerTest, InsufficientFundsBlocksAddLine)
{
   MiniDb::World world(47);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Economic,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));
   world.GetEconomy().TryDebit(world.GetBalance());

   MiniDb::AiAction action;
   action.type = MiniDb::AiActionType::AddLine;
   action.stationIds.push_back(0);
   action.stationIds.push_back(1);
   EXPECT_FALSE(MiniDb::AiActionIsAffordable(world, action));
   EXPECT_EQ(MiniDb::ApplyAiAction(world, action), MiniDb::Result::InsufficientFunds);
}

TEST(AiPlannerTest, CashReserveBlocksNonEmergencySpend)
{
   MiniDb::World world(59);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Economic,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));

   MiniDb::StationIdList lineStations;
   lineStations.push_back(0);
   lineStations.push_back(1);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(lineStations, lineId)));

   const int64_t reserve = MiniDb::AiRequiredCashReserve(world);
   const int64_t trainCost = world.GetEconomy().TrainPurchaseCostScaled();
   const int64_t keep = reserve + trainCost - 1;
   ASSERT_GT(world.GetBalance(), keep);
   world.GetEconomy().TryDebit(world.GetBalance() - keep);

   MiniDb::AiAction trainAction;
   trainAction.type = MiniDb::AiActionType::AddTrain;
   trainAction.lineId = lineId;
   EXPECT_FALSE(
      MiniDb::AiActionIsAffordable(world, trainAction, MiniDb::AllowAiReserveSpend::No));
   EXPECT_TRUE(
      MiniDb::AiActionIsAffordable(world, trainAction, MiniDb::AllowAiReserveSpend::Yes));
}

TEST(AiPlannerTest, CapsTrainsPerLineWithoutPatienceEmergency)
{
   MiniDb::World world(61);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Sandbox,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));

   MiniDb::StationIdList lineStations;
   lineStations.push_back(3);
   lineStations.push_back(4);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(lineStations, lineId)));

   for (uint32_t trainIndex = 0; trainIndex < MiniDb::AiMaxTrainsPerLine; ++trainIndex)
   {
      ASSERT_TRUE(MiniDb::IsOk(world.AddTrainToLine(lineId)));
   }

   for (uint32_t passengerIndex = 0; passengerIndex < 80; ++passengerIndex)
   {
      ASSERT_TRUE(MiniDb::IsOk(world.SpawnPassenger(3, 4)));
   }

   MiniDb::AiObservation observation;
   MiniDb::BuildAiObservation(world, observation);

   MiniDb::AiAction trainAction;
   trainAction.type = MiniDb::AiActionType::AddTrain;
   trainAction.lineId = lineId;
   MiniDb::ScoreAiAction(world, observation, trainAction);
   EXPECT_LT(trainAction.score, 50.0f);
}

TEST(AiPlannerTest, PlayAgentBuildsOverTime)
{
   MiniDb::World world(53);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Sandbox,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));

   MiniDb::PlayAgent agent;
   agent.Reset();
   for (uint32_t step = 0; step < 20; ++step)
   {
      ASSERT_TRUE(MiniDb::IsOk(agent.Step(world, MiniDb::AiDecisionIntervalSeconds)));
      world.Tick(MiniDb::AiDecisionIntervalSeconds);
   }

   EXPECT_FALSE(world.GetNetwork().GetLines().empty());
}

TEST(AiPlannerTest, EconomicAgentAvoidsTrainSpam)
{
   MiniDb::World world(67);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Economic,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));

   MiniDb::PlayAgent agent;
   agent.Reset();
   for (uint32_t step = 0; step < 40; ++step)
   {
      ASSERT_TRUE(MiniDb::IsOk(agent.Step(world, MiniDb::AiDecisionIntervalSeconds)));
      world.Tick(MiniDb::AiDecisionIntervalSeconds);
   }

   EXPECT_LE(world.GetTrains().size(), static_cast<size_t>(MiniDb::AiMaxTrainsPerLine * 4u));
   EXPECT_GE(world.GetBalance(), MiniDb::AiMinCashReserve / 2);
}

TEST(AiPlannerTest, PrefersBridgingDisconnectedNetworks)
{
   MiniDb::World world(71);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   world.ConfigureEconomy(
      MiniDb::DefaultTrainCapacity,
      MiniDb::GameMode::Sandbox,
      MiniDb::NeverLose::No);
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnInitialStations()));

   MiniDb::StationIdList northLine;
   northLine.push_back(0);
   northLine.push_back(1);
   MiniDb::LineId northLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(northLine, northLineId)));

   MiniDb::StationIdList southLine;
   southLine.push_back(2);
   southLine.push_back(3);
   MiniDb::LineId southLineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(southLine, southLineId)));

   MiniDb::AiObservation observation;
   MiniDb::BuildAiObservation(world, observation);
   ASSERT_GE(observation.networkComponentCount, 2u);

   MiniDb::AiAction action;
   MiniDb::PlanAiAction(world, action);
   EXPECT_EQ(action.type, MiniDb::AiActionType::AddLine);
   ASSERT_EQ(action.stationIds.size(), 2u);

   const MiniDb::AiStationState* pLeft = nullptr;
   const MiniDb::AiStationState* pRight = nullptr;
   for (const MiniDb::AiStationState& station : observation.stations)
   {
      if (station.stationId == action.stationIds[0])
      {
         pLeft = &station;
      }
      if (station.stationId == action.stationIds[1])
      {
         pRight = &station;
      }
   }

   ASSERT_NE(pLeft, nullptr);
   ASSERT_NE(pRight, nullptr);
   EXPECT_GE(pLeft->networkComponentId, 0);
   EXPECT_GE(pRight->networkComponentId, 0);
   EXPECT_NE(pLeft->networkComponentId, pRight->networkComponentId);
}

