/*!
 *\file economy_test.cpp
 *\brief Tests for economy scaling, bankruptcy, and sandbox mode.
 */

#include <gtest/gtest.h>

#include "core/constants.h"
#include "simulation/economy.h"

TEST(EconomyTest, EconomyScalesWithTrainCapacity)
{
   MiniDb::Economy economyAtDefault;
   economyAtDefault.ResetForNewGame(MiniDb::DefaultTrainCapacity, MiniDb::GameMode::Economic, MiniDb::NeverLose::No);
   const int64_t defaultBalance = economyAtDefault.GetBalance();
   const int64_t defaultBuildCost = economyAtDefault.TrackBuildCost(10.0f);
   const int64_t defaultFare = economyAtDefault.FareForTrip(100.0f);

   MiniDb::Economy economyAtDouble;
   economyAtDouble.ResetForNewGame(MiniDb::DefaultTrainCapacity * 2, MiniDb::GameMode::Economic, MiniDb::NeverLose::No);
   EXPECT_EQ(economyAtDouble.GetBalance(), defaultBalance * 2);
   EXPECT_EQ(economyAtDouble.TrackBuildCost(10.0f), defaultBuildCost * 2);
   EXPECT_EQ(economyAtDouble.FareForTrip(100.0f), defaultFare * 2);
}

TEST(EconomyTest, BankruptcyTimerAccumulatesOnlyWhenNegative)
{
   MiniDb::Economy economy;
   economy.ResetForNewGame(MiniDb::DefaultTrainCapacity, MiniDb::GameMode::Economic, MiniDb::NeverLose::No);
   EXPECT_EQ(economy.TickBankruptcyTimer(10.0f, false), MiniDb::BankruptcyTickResult::Continue);
   EXPECT_FLOAT_EQ(economy.GetNegativeBalanceRealSeconds(), 0.0f);

   economy.TryDebit(economy.GetBalance());
   economy.TickMaintenance(10.0f, 100.0f, 1);
   ASSERT_LT(economy.GetBalance(), 0);
   EXPECT_EQ(economy.TickBankruptcyTimer(5.0f, false), MiniDb::BankruptcyTickResult::Continue);
   EXPECT_FLOAT_EQ(economy.GetNegativeBalanceRealSeconds(), 5.0f);

   economy.Credit(1000);
   EXPECT_EQ(economy.TickBankruptcyTimer(3.0f, false), MiniDb::BankruptcyTickResult::Continue);
   EXPECT_FLOAT_EQ(economy.GetNegativeBalanceRealSeconds(), 0.0f);
}

TEST(EconomyTest, BankruptcyTimerPausesWithSimulation)
{
   MiniDb::Economy economy;
   economy.ResetForNewGame(MiniDb::DefaultTrainCapacity, MiniDb::GameMode::Economic, MiniDb::NeverLose::No);
   economy.TryDebit(economy.GetBalance() + 1);
   EXPECT_EQ(economy.TickBankruptcyTimer(20.0f, true), MiniDb::BankruptcyTickResult::Continue);
   EXPECT_FLOAT_EQ(economy.GetNegativeBalanceRealSeconds(), 0.0f);
}

TEST(EconomyTest, NeverLoseDisablesGameOver)
{
   MiniDb::Economy economy;
   economy.ResetForNewGame(MiniDb::DefaultTrainCapacity, MiniDb::GameMode::Economic, MiniDb::NeverLose::Yes);
   economy.TryDebit(economy.GetBalance() + 1);
   const MiniDb::BankruptcyTickResult result = economy.TickBankruptcyTimer(
      MiniDb::NegativeBalanceGameOverRealSeconds + 1.0f,
      false);
   EXPECT_EQ(result, MiniDb::BankruptcyTickResult::Continue);
}

TEST(EconomyTest, SandboxSkipsCharges)
{
   MiniDb::Economy economy;
   economy.ResetForNewGame(MiniDb::DefaultTrainCapacity, MiniDb::GameMode::Sandbox, MiniDb::NeverLose::No);
   EXPECT_FALSE(economy.IsEconomicMode());
   EXPECT_EQ(economy.GetBalance(), 0);
   EXPECT_TRUE(economy.TryDebit(1000000));
   EXPECT_EQ(economy.GetBalance(), 0);
   economy.Credit(500);
   EXPECT_EQ(economy.GetBalance(), 0);
}

TEST(EconomyTest, BankruptcyTimerReachesGameOver)
{
   MiniDb::Economy economy;
   economy.ResetForNewGame(MiniDb::DefaultTrainCapacity, MiniDb::GameMode::Economic, MiniDb::NeverLose::No);
   economy.TryDebit(economy.GetBalance());
   economy.TickMaintenance(1.0f, 1000.0f, 10);
   ASSERT_LT(economy.GetBalance(), 0);
   const MiniDb::BankruptcyTickResult result = economy.TickBankruptcyTimer(
      MiniDb::NegativeBalanceGameOverRealSeconds,
      false);
   EXPECT_EQ(result, MiniDb::BankruptcyTickResult::GameOver);
}
