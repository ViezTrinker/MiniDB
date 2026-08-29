/*!
 *\file economy.cpp
 *\brief Money, track inventory, and bankruptcy for economic mode.
 */

#include "simulation/economy.h"

#include <cmath>

namespace MiniDb
{
   Economy::Economy(void)
   {
   }

   void Economy::ResetForNewGame(uint32_t trainCapacity, GameMode gameMode, NeverLose neverLose)
   {
      _gameMode = gameMode;
      _neverLose = neverLose;
      _economyScale = EconomyScaleForCapacity(trainCapacity);
      _negativeBalanceRealSeconds = 0.0f;
      _lifetimeFareTotal = 0;
      _fareSinceLastSnapshot = 0;
      _arrivalsSinceLastSnapshot = 0;
      _builtPairKeys.clear();
      if (_gameMode == GameMode::Sandbox)
      {
         _balance = 0;
         return;
      }

      _balance = ScaleMoney(static_cast<float>(DefaultStartingBalance));
   }

   void Economy::Clear(void)
   {
      _gameMode = GameMode::Sandbox;
      _neverLose = NeverLose::No;
      _economyScale = 1.0f;
      _balance = 0;
      _negativeBalanceRealSeconds = 0.0f;
      _lifetimeFareTotal = 0;
      _fareSinceLastSnapshot = 0;
      _arrivalsSinceLastSnapshot = 0;
      _builtPairKeys.clear();
   }

   GameMode Economy::GetGameMode(void) const
   {
      return _gameMode;
   }

   NeverLose Economy::GetNeverLose(void) const
   {
      return _neverLose;
   }

   float Economy::GetEconomyScale(void) const
   {
      return _economyScale;
   }

   int64_t Economy::GetBalance(void) const
   {
      return _balance;
   }

   float Economy::GetNegativeBalanceRealSeconds(void) const
   {
      return _negativeBalanceRealSeconds;
   }

   float Economy::GetSecondsUntilGameOver(void) const
   {
      if (_gameMode != GameMode::Economic || _neverLose == NeverLose::Yes || _balance >= 0)
      {
         return NegativeBalanceGameOverRealSeconds;
      }

      const float remaining = NegativeBalanceGameOverRealSeconds - _negativeBalanceRealSeconds;
      if (remaining < 0.0f)
      {
         return 0.0f;
      }

      return remaining;
   }

   float Economy::GetMaintenancePerSecond(float uniqueTrackKm, uint32_t trainCount) const
   {
      if (_gameMode != GameMode::Economic)
      {
         return 0.0f;
      }

      const float trackMaintenance =
         uniqueTrackKm * TrackMaintenanceCostPerKmPerSecond * _economyScale;
      const float trainMaintenance =
         static_cast<float>(trainCount) * TrainMaintenanceCostPerSecond * _economyScale;
      return trackMaintenance + trainMaintenance;
   }

   int64_t Economy::GetLifetimeFareTotal(void) const
   {
      return _lifetimeFareTotal;
   }

   int64_t Economy::GetFareSinceLastSnapshot(void) const
   {
      return _fareSinceLastSnapshot;
   }

   uint32_t Economy::GetArrivalsSinceLastSnapshot(void) const
   {
      return _arrivalsSinceLastSnapshot;
   }

   void Economy::ResetSnapshotCounters(void)
   {
      _fareSinceLastSnapshot = 0;
      _arrivalsSinceLastSnapshot = 0;
   }

   bool Economy::IsEconomicMode(void) const
   {
      return _gameMode == GameMode::Economic;
   }

   bool Economy::CanAfford(int64_t cost) const
   {
      if (_gameMode != GameMode::Economic)
      {
         return true;
      }

      return _balance >= cost;
   }

   bool Economy::TryDebit(int64_t cost)
   {
      if (_gameMode != GameMode::Economic)
      {
         return true;
      }
      if (_balance < cost)
      {
         return false;
      }

      _balance -= cost;
      return true;
   }

   void Economy::Credit(int64_t amount)
   {
      if (_gameMode != GameMode::Economic)
      {
         return;
      }

      _balance += amount;
   }

   void Economy::CreditFare(int64_t amount)
   {
      if (_gameMode != GameMode::Economic)
      {
         return;
      }

      _balance += amount;
      _lifetimeFareTotal += amount;
      _fareSinceLastSnapshot += amount;
      ++_arrivalsSinceLastSnapshot;
   }

   void Economy::TickMaintenance(float simDeltaSeconds, float uniqueTrackKm, uint32_t trainCount)
   {
      if (_gameMode != GameMode::Economic || simDeltaSeconds <= 0.0f)
      {
         return;
      }

      const float maintenanceCost = GetMaintenancePerSecond(uniqueTrackKm, trainCount) * simDeltaSeconds;
      const auto debitAmount = static_cast<int64_t>(std::ceil(maintenanceCost));
      if (debitAmount > 0)
      {
         _balance -= debitAmount;
      }
   }

   int64_t Economy::TrackBuildCost(float newTrackKm) const
   {
      return ScaleMoney(newTrackKm * TrackBuildCostPerKm);
   }

   int64_t Economy::TrainPurchaseCostScaled(void) const
   {
      return ScaleMoney(static_cast<float>(TrainPurchaseCost));
   }

   int64_t Economy::FareForTrip(float beelineKm) const
   {
      return ScaleMoney(beelineKm * FarePerPassengerKm);
   }

   void Economy::RegisterBuiltSegments(const TrackSegmentRecordList& newSegments)
   {
      for (const TrackSegmentRecord& segment : newSegments)
      {
         const uint64_t pairKey = CanonicalStationPairKey(segment.stationA, segment.stationB);
         _builtPairKeys.insert(pairKey);
      }
   }

   void Economy::UnregisterBuiltSegments(const TrackSegmentRecordList& segments)
   {
      for (const TrackSegmentRecord& segment : segments)
      {
         const uint64_t pairKey = CanonicalStationPairKey(segment.stationA, segment.stationB);
         _builtPairKeys.erase(pairKey);
      }
   }

   float Economy::CollectNewSegmentKilometers(
      const TrackSegmentRecordList& candidateSegments,
      TrackSegmentRecordList& newSegments) const
   {
      return SumNewSegmentKilometers(candidateSegments, _builtPairKeys, newSegments);
   }

   BankruptcyTickResult Economy::TickBankruptcyTimer(float realDeltaSeconds, bool pause)
   {
      if (_gameMode != GameMode::Economic || _neverLose == NeverLose::Yes)
      {
         return BankruptcyTickResult::Continue;
      }
      if (pause || realDeltaSeconds <= 0.0f)
      {
         return BankruptcyTickResult::Continue;
      }

      if (_balance >= 0)
      {
         _negativeBalanceRealSeconds = 0.0f;
         return BankruptcyTickResult::Continue;
      }

      _negativeBalanceRealSeconds += realDeltaSeconds;
      if (_negativeBalanceRealSeconds >= NegativeBalanceGameOverRealSeconds)
      {
         return BankruptcyTickResult::GameOver;
      }

      return BankruptcyTickResult::Continue;
   }

   int64_t Economy::ScaleMoney(float baseAmount) const
   {
      return static_cast<int64_t>(std::lround(baseAmount * _economyScale));
   }
} // namespace MiniDb
