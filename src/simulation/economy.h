/*!
 *\file economy.h
 *\brief Money, track inventory, and bankruptcy for economic mode.
 */

#ifndef ECONOMY_H
#define ECONOMY_H

#include <cstdint>

#include "core/constants.h"
#include "simulation/track_inventory.h"

namespace MiniDb
{
   enum class GameMode : uint8_t
   {
      Economic = 0,
      Sandbox = 1
   };

   enum class NeverLose : bool
   {
      No = false,
      Yes = true
   };

   enum class BankruptcyTickResult : uint8_t
   {
      Continue = 0,
      GameOver = 1
   };

   enum class GameOverReason : uint8_t
   {
      None = 0,
      Bankruptcy = 1,
      PlatformWait = 2
   };

   class Economy
   {
   public:
      Economy(void);

      /*!
       *\brief Resets economy for a new run.
       *
       *\param[in] trainCapacity Train capacity setting used for scaling.
       *\param[in] gameMode Whether money applies.
       *\param[in] neverLose Whether bankruptcy game over is disabled.
       */
      void ResetForNewGame(uint32_t trainCapacity, GameMode gameMode, NeverLose neverLose);

      /*!
       *\brief Clears built track and balance state.
       */
      void Clear(void);

      GameMode GetGameMode(void) const;
      NeverLose GetNeverLose(void) const;
      float GetEconomyScale(void) const;
      int64_t GetBalance(void) const;
      float GetNegativeBalanceRealSeconds(void) const;
      float GetSecondsUntilGameOver(void) const;
      float GetMaintenancePerSecond(float uniqueTrackKm, uint32_t trainCount) const;
      int64_t GetLifetimeFareTotal(void) const;
      int64_t GetFareSinceLastSnapshot(void) const;
      uint32_t GetArrivalsSinceLastSnapshot(void) const;
      void ResetSnapshotCounters(void);

      bool IsEconomicMode(void) const;
      bool CanAfford(int64_t cost) const;

      /*!
       *\brief Debits the balance when affordable.
       *
       *\param[in] cost Amount to subtract.
       *\return True when the debit succeeded.
       */
      bool TryDebit(int64_t cost);

      void Credit(int64_t amount);
      void CreditFare(int64_t amount);

      /*!
       *\brief Charges maintenance for the elapsed simulation time.
       *
       *\param[in] simDeltaSeconds Elapsed simulation seconds.
       *\param[in] uniqueTrackKm Built unique track length.
       *\param[in] trainCount Active trains.
       */
      void TickMaintenance(float simDeltaSeconds, float uniqueTrackKm, uint32_t trainCount);

      /*!
       *\brief Computes build cost for new track kilometres.
       *
       *\param[in] newTrackKm Kilometres not yet in inventory.
       */
      int64_t TrackBuildCost(float newTrackKm) const;

      /*!
       *\brief Computes train purchase cost at current scale.
       */
      int64_t TrainPurchaseCostScaled(void) const;

      /*!
       *\brief Computes fare for a completed trip.
       *
       *\param[in] beelineKm Origin-destination beeline distance.
       */
      int64_t FareForTrip(float beelineKm) const;

      /*!
       *\brief Registers newly built track pairs after a successful build.
       *
       *\param[in] newSegments Segments that were charged.
       */
      void RegisterBuiltSegments(const TrackSegmentRecordList& newSegments);

      /*!
       *\brief Removes track pairs from inventory (rollback after a failed build).
       *
       *\param[in] segments Segments previously registered.
       */
      void UnregisterBuiltSegments(const TrackSegmentRecordList& segments);

      /*!
       *\brief Returns kilometres of segments not yet in inventory.
       *
       *\param[in] candidateSegments Segments to evaluate.
       *\param[out] newSegments Segments that would be built.
       */
      float CollectNewSegmentKilometers(
         const TrackSegmentRecordList& candidateSegments,
         TrackSegmentRecordList& newSegments) const;

      /*!
       *\brief Updates bankruptcy timer using wall-clock seconds.
       *
       *\param[in] realDeltaSeconds Unscaled elapsed seconds.
       *\param[in] pause Whether simulation pause is active.
       */
      BankruptcyTickResult TickBankruptcyTimer(float realDeltaSeconds, bool pause);

   private:
      int64_t ScaleMoney(float baseAmount) const;

      GameMode _gameMode = GameMode::Sandbox;
      NeverLose _neverLose = NeverLose::No;
      float _economyScale = 1.0f;
      int64_t _balance = 0;
      float _negativeBalanceRealSeconds = 0.0f;
      int64_t _lifetimeFareTotal = 0;
      int64_t _fareSinceLastSnapshot = 0;
      uint32_t _arrivalsSinceLastSnapshot = 0;
      TrackPairKeySet _builtPairKeys;
   };
} // namespace MiniDb

#endif // ECONOMY_H
