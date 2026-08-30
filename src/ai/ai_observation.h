/*!
 *\file ai_observation.h
 *\brief World snapshot used by the spectator AI planner.
 */

#ifndef AI_OBSERVATION_H
#define AI_OBSERVATION_H

#include <cstdint>
#include <vector>

#include "ai/ai_actions.h"
#include "core/constants.h"
#include "core/result.h"
#include "core/types.h"
#include "simulation/economy.h"
#include "simulation/world.h"

namespace MiniDb
{
   struct AiStationState
   {
      StationId stationId = InvalidStationId;
      uint32_t population = 0;
      uint32_t waitingCount = 0;
      float maxWaitedSeconds = 0.0f;
      float patienceLimitSeconds = MaxPassengerPlatformWaitSeconds;
      bool connected = false;
      int32_t networkComponentId = -1;
      MapPoint position;
   };

   using AiStationStateList = std::vector<AiStationState>;

   struct AiLineState
   {
      LineId lineId = InvalidLineId;
      uint32_t trainCount = 0;
      uint32_t stationCount = 0;
      uint32_t waitingAlongLine = 0;
      float cycleTimeSeconds = 0.0f;
      float expectedWaitSeconds = 0.0f;
   };

   using AiLineStateList = std::vector<AiLineState>;

   struct AiObservation
   {
      float simulationTimeSeconds = 0.0f;
      int64_t balance = 0;
      float uniqueTrackKm = 0.0f;
      uint32_t trainCount = 0;
      float maintenancePerSecond = 0.0f;
      GameMode gameMode = GameMode::Sandbox;
      NeverLose neverLose = NeverLose::No;
      uint32_t trainCapacity = DefaultTrainCapacity;
      bool patienceActive = false;
      uint32_t networkComponentCount = 0;
      AiStationStateList stations;
      AiLineStateList lines;
      StationIdList unconnectedStationIds;
   };

   /*!
    *\brief Builds an observation snapshot from the live world.
    *
    *\param[in] world Current simulation.
    *\param[out] observation Filled snapshot.
    */
   void BuildAiObservation(const World& world, AiObservation& observation);

   /*!
    *\brief Estimates the cash cost of an action in economic mode.
    *
    *\param[in] world Current simulation.
    *\param[in] action Candidate action.
    */
   int64_t EstimateAiActionCost(const World& world, const AiAction& action);

   /*!
    *\brief Minimum balance that should remain after spending (maintenance buffer).
    *
    *\param[in] world Current simulation.
    */
   int64_t AiRequiredCashReserve(const World& world);

   /*!
    *\brief Returns true when the action's cash cost is affordable (or sandbox).
    *
    *\param[in] world Current simulation.
    *\param[in] action Candidate action.
    *\param[in] allowReserveSpend Yes when patience/capacity emergency may dip into reserve.
    */
   bool AiActionIsAffordable(
      const World& world,
      const AiAction& action,
      AllowAiReserveSpend allowReserveSpend = AllowAiReserveSpend::No);

   /*!
    *\brief Applies a planned action to the world.
    *
    *\param[in,out] world Simulation to mutate.
    *\param[in] action Action to execute.
    */
   Result ApplyAiAction(World& world, const AiAction& action);
} // namespace MiniDb

#endif // AI_OBSERVATION_H
