/*!
 *\file ai_scorer.cpp
 *\brief Heuristic scores for spectator AI candidate actions.
 */

#include "ai/ai_scorer.h"

#include "ai/ai_observation.h"
#include "core/constants.h"
#include "simulation/gravity_model.h"
#include "simulation/track_inventory.h"

namespace MiniDb
{
   namespace
   {
      const AiStationState* FindStationState(
         const AiObservation& observation,
         StationId stationId)
      {
         for (const AiStationState& state : observation.stations)
         {
            if (state.stationId == stationId)
            {
               return &state;
            }
         }

         return nullptr;
      }

      const AiLineState* FindLineState(const AiObservation& observation, LineId lineId)
      {
         for (const AiLineState& state : observation.lines)
         {
            if (state.lineId == lineId)
            {
               return &state;
            }
         }

         return nullptr;
      }

      float GravityPairScore(
         const World& world,
         StationId originId,
         StationId destinationId)
      {
         const StationRecord* pOrigin = world.GetNetwork().FindStation(originId);
         const StationRecord* pDestination = world.GetNetwork().FindStation(destinationId);
         if (pOrigin == nullptr || pDestination == nullptr)
         {
            return 0.0f;
         }

         const float probability = GravityProbability(
            originId,
            destinationId,
            world.GetNetwork().GetStations(),
            DefaultGravityParameters());
         const auto averagePopulation = static_cast<float>(
            (pOrigin->population + pDestination->population) / 2u);
         return probability * averagePopulation;
      }

      float TrackKmForStations(const World& world, const StationIdList& stationIds)
      {
         TrackSegmentRecordList segments;
         CollectSegmentsForLineDefinition(world.GetNetwork(), stationIds, segments);
         float totalKm = 0.0f;
         for (const TrackSegmentRecord& segment : segments)
         {
            totalKm += segment.distanceKm;
         }

         return totalKm;
      }

      float PatienceUrgency(const AiStationState& state, bool patienceActive)
      {
         if (!patienceActive)
         {
            return 0.0f;
         }
         if (state.patienceLimitSeconds <= 0.0f)
         {
            return 0.0f;
         }

         const float fraction = state.maxWaitedSeconds / state.patienceLimitSeconds;
         if (fraction < AiPatienceEmergencyFraction)
         {
            return 0.0f;
         }

         return (fraction - AiPatienceEmergencyFraction) * 100000.0f +
            static_cast<float>(state.waitingCount) * 50.0f;
      }

      bool LineContainsStation(const Line& line, StationId stationId)
      {
         for (StationId lineStationId : line.stationIds)
         {
            if (lineStationId == stationId)
            {
               return true;
            }
         }

         return false;
      }

      float ApplyEconomicPenalties(
         const World& world,
         const AiAction& action,
         float score,
         bool isComponentBridge)
      {
         if (!world.GetEconomy().IsEconomicMode())
         {
            return score;
         }

         const int64_t cost = EstimateAiActionCost(world, action);
         float penaltyScale = AiCostScorePenaltyPerEuro;
         if (isComponentBridge)
         {
            penaltyScale *= AiBridgeCostPenaltyScale;
         }

         score -= static_cast<float>(cost) * penaltyScale;
         return score;
      }

      int32_t LineNetworkComponentId(
         const World& world,
         const AiObservation& observation,
         LineId lineId)
      {
         const Line* pLine = world.GetNetwork().FindLine(lineId);
         if (pLine == nullptr || pLine->stationIds.empty())
         {
            return -1;
         }

         const AiStationState* pState = FindStationState(observation, pLine->stationIds.front());
         if (pState == nullptr)
         {
            return -1;
         }

         return pState->networkComponentId;
      }

      bool ActionMergesComponents(
         const World& world,
         const AiObservation& observation,
         const AiAction& action)
      {
         if (action.type == AiActionType::AddLine)
         {
            if (action.stationIds.size() < 2)
            {
               return false;
            }

            const AiStationState* pLeft = FindStationState(observation, action.stationIds.front());
            const AiStationState* pRight = FindStationState(observation, action.stationIds.back());
            if (pLeft == nullptr || pRight == nullptr)
            {
               return false;
            }
            if (pLeft->networkComponentId < 0 || pRight->networkComponentId < 0)
            {
               return false;
            }

            return pLeft->networkComponentId != pRight->networkComponentId;
         }

         if (action.type == AiActionType::ExtendLine ||
            action.type == AiActionType::InsertStation)
         {
            const AiStationState* pTarget = FindStationState(observation, action.stationId);
            if (pTarget == nullptr || pTarget->networkComponentId < 0)
            {
               return false;
            }

            const int32_t lineComponent = LineNetworkComponentId(world, observation, action.lineId);
            if (lineComponent < 0)
            {
               return false;
            }

            return pTarget->networkComponentId != lineComponent;
         }

         return false;
      }

      uint32_t DesiredTrainCount(float cycleTimeSeconds)
      {
         if (cycleTimeSeconds <= 0.0f)
         {
            return 1;
         }

         auto desired = static_cast<uint32_t>(
            cycleTimeSeconds / AiTargetHeadwaySeconds + 0.999f);
         if (desired < 1u)
         {
            desired = 1u;
         }
         if (desired > AiMaxTrainsPerLine)
         {
            desired = AiMaxTrainsPerLine;
         }

         return desired;
      }
   } // namespace

   void ScoreAiAction(const World& world, const AiObservation& observation, AiAction& action)
   {
      action.score = -1000000.0f;
      if (action.type == AiActionType::Noop)
      {
         action.score = 0.0f;
         return;
      }

      if (action.type == AiActionType::AddTrain)
      {
         const AiLineState* pLine = FindLineState(observation, action.lineId);
         if (pLine == nullptr)
         {
            return;
         }

         const uint32_t desiredTrains = DesiredTrainCount(pLine->cycleTimeSeconds);
         if (pLine->trainCount >= desiredTrains)
         {
            float patienceOnly = 0.0f;
            const Line* pNetworkLine = world.GetNetwork().FindLine(action.lineId);
            if (pNetworkLine != nullptr)
            {
               for (const AiStationState& station : observation.stations)
               {
                  if (!LineContainsStation(*pNetworkLine, station.stationId))
                  {
                     continue;
                  }

                  patienceOnly += PatienceUrgency(station, observation.patienceActive);
               }
            }

            if (patienceOnly <= 0.0f)
            {
               return;
            }

            action.score = ApplyEconomicPenalties(world, action, patienceOnly * 0.25f, false);
            return;
         }

         const auto trainsForAverage = pLine->trainCount == 0 ? 1u : pLine->trainCount;
         const float waitingPerTrain =
            static_cast<float>(pLine->waitingAlongLine) / static_cast<float>(trainsForAverage);
         float score = waitingPerTrain * 4.0f;
         score += pLine->expectedWaitSeconds * 8.0f;
         if (pLine->trainCount == 0)
         {
            score += 8000.0f;
         }
         if (pLine->expectedWaitSeconds < AiAddTrainMinExpectedWaitSeconds &&
            pLine->trainCount > 0)
         {
            score *= 0.15f;
         }

         score -= static_cast<float>(pLine->trainCount * pLine->trainCount) * 80.0f;

         const Line* pNetworkLine = world.GetNetwork().FindLine(action.lineId);
         if (pNetworkLine != nullptr)
         {
            for (const AiStationState& station : observation.stations)
            {
               if (!LineContainsStation(*pNetworkLine, station.stationId))
               {
                  continue;
               }

               score += PatienceUrgency(station, observation.patienceActive);
            }
         }

         action.score = ApplyEconomicPenalties(world, action, score, false);
         return;
      }

      if (action.type == AiActionType::AddLine)
      {
         if (action.stationIds.size() < MinimumLineStations)
         {
            return;
         }

         float gravityScore = 0.0f;
         float waitingBonus = 0.0f;
         float patienceBonus = 0.0f;
         for (size_t leftIndex = 0; leftIndex < action.stationIds.size(); ++leftIndex)
         {
            const StationId leftId = action.stationIds[leftIndex];
            const AiStationState* pLeft = FindStationState(observation, leftId);
            if (pLeft != nullptr)
            {
               waitingBonus += static_cast<float>(pLeft->waitingCount) * 4.0f;
               patienceBonus += PatienceUrgency(*pLeft, observation.patienceActive);
            }

            for (size_t rightIndex = leftIndex + 1; rightIndex < action.stationIds.size(); ++rightIndex)
            {
               gravityScore += GravityPairScore(world, leftId, action.stationIds[rightIndex]);
            }
         }

         const float trackKm = TrackKmForStations(world, action.stationIds);
         float distancePenalty = trackKm;
         if (distancePenalty < 1.0f)
         {
            distancePenalty = 1.0f;
         }

         float connectBonus = 0.0f;
         for (StationId stationId : action.stationIds)
         {
            const AiStationState* pState = FindStationState(observation, stationId);
            if (pState != nullptr && !pState->connected)
            {
               connectBonus += static_cast<float>(pState->population) * 0.00005f + 200.0f;
            }
         }

         const bool isBridge = ActionMergesComponents(world, observation, action);
         float trackPenalty = AiTrackKmScorePenalty;
         if (isBridge)
         {
            trackPenalty *= 0.35f;
         }

         float score =
            (gravityScore * 0.002f) / distancePenalty +
            waitingBonus +
            patienceBonus +
            connectBonus -
            trackKm * trackPenalty;
         if (isBridge)
         {
            score += AiComponentBridgeScoreBonus;
         }

         action.score = ApplyEconomicPenalties(world, action, score, isBridge);
         return;
      }

      if (action.type == AiActionType::ExtendLine)
      {
         const AiStationState* pStation = FindStationState(observation, action.stationId);
         const AiLineState* pLine = FindLineState(observation, action.lineId);
         if (pStation == nullptr || pLine == nullptr)
         {
            return;
         }

         const Line* pNetworkLine = world.GetNetwork().FindLine(action.lineId);
         const StationRecord* pNewStation = world.GetNetwork().FindStation(action.stationId);
         if (pNetworkLine == nullptr || pNewStation == nullptr || pNetworkLine->stationIds.empty())
         {
            return;
         }

         StationId terminusId = InvalidStationId;
         if (action.lineEnd == LineEnd::Front)
         {
            terminusId = pNetworkLine->stationIds.front();
         }
         else
         {
            terminusId = pNetworkLine->stationIds.back();
         }

         const StationRecord* pTerminus = world.GetNetwork().FindStation(terminusId);
         if (pTerminus == nullptr)
         {
            return;
         }

         const float extendKm = DistanceKm(pTerminus->position, pNewStation->position);
         const bool isBridge = ActionMergesComponents(world, observation, action);
         float score = static_cast<float>(pStation->population) * 0.00008f;
         score += static_cast<float>(pStation->waitingCount) * 10.0f;
         score += PatienceUrgency(*pStation, observation.patienceActive);
         if (!pStation->connected)
         {
            score += 400.0f;
         }
         score += static_cast<float>(pLine->waitingAlongLine) * 1.0f;
         score -= extendKm * AiTrackKmScorePenalty;
         score -= static_cast<float>(pLine->stationCount) * 40.0f;
         if (isBridge)
         {
            score += AiComponentBridgeScoreBonus;
         }

         action.score = ApplyEconomicPenalties(world, action, score, isBridge);
         return;
      }

      if (action.type == AiActionType::InsertStation)
      {
         const AiStationState* pStation = FindStationState(observation, action.stationId);
         const AiLineState* pLine = FindLineState(observation, action.lineId);
         if (pStation == nullptr || pLine == nullptr)
         {
            return;
         }

         const bool isBridge = ActionMergesComponents(world, observation, action);
         float score = static_cast<float>(pStation->population) * 0.0001f;
         score += static_cast<float>(pStation->waitingCount) * 12.0f;
         score += PatienceUrgency(*pStation, observation.patienceActive);
         if (!pStation->connected)
         {
            score += 600.0f;
         }
         score -= static_cast<float>(pLine->stationCount) * 40.0f;
         if (isBridge)
         {
            score += AiComponentBridgeScoreBonus;
         }

         action.score = ApplyEconomicPenalties(world, action, score, isBridge);
         return;
      }

      if (action.type == AiActionType::RemoveLine)
      {
         const AiLineState* pLine = FindLineState(observation, action.lineId);
         if (pLine == nullptr)
         {
            return;
         }

         if (pLine->waitingAlongLine > 0 || pLine->stationCount <= 2)
         {
            action.score = -50000.0f;
            return;
         }

         action.score = -1000.0f - static_cast<float>(pLine->trainCount) * 100.0f;
      }
   }
} // namespace MiniDb
