/*!
 *\file ai_planner.cpp
 *\brief Candidate generation and greedy selection for the spectator AI.
 */

#include "ai/ai_planner.h"

#include <algorithm>
#include <cmath>

#include "ai/ai_scorer.h"
#include "core/constants.h"
#include "simulation/network.h"

namespace MiniDb
{
   namespace
   {
      struct CityRank
      {
         StationId stationId = InvalidStationId;
         uint32_t population = 0;
         uint32_t waitingCount = 0;
         bool connected = false;
      };

      using CityRankList = std::vector<CityRank>;

      bool CompareCityRankDescending(const CityRank& left, const CityRank& right)
      {
         if (left.waitingCount != right.waitingCount)
         {
            return left.waitingCount > right.waitingCount;
         }

         return left.population > right.population;
      }

      void BuildCityRanks(const AiObservation& observation, CityRankList& ranks)
      {
         ranks.clear();
         for (const AiStationState& station : observation.stations)
         {
            CityRank rank;
            rank.stationId = station.stationId;
            rank.population = station.population;
            rank.waitingCount = station.waitingCount;
            rank.connected = station.connected;
            ranks.push_back(rank);
         }

         std::sort(ranks.begin(), ranks.end(), CompareCityRankDescending);
         if (ranks.size() > AiTopCityCandidateCount)
         {
            ranks.resize(AiTopCityCandidateCount);
         }
      }

      bool StationAlreadyOnLine(const Line& line, StationId stationId)
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

      float PointToSegmentDistanceKm(
         MapPoint point,
         MapPoint from,
         MapPoint to,
         float& distanceAlongKm)
      {
         const float deltaX = to.xKm - from.xKm;
         const float deltaY = to.yKm - from.yKm;
         const float lengthSquared = deltaX * deltaX + deltaY * deltaY;
         if (lengthSquared <= 0.0001f)
         {
            distanceAlongKm = 0.0f;
            return DistanceKm(point, from);
         }

         float projection =
            ((point.xKm - from.xKm) * deltaX + (point.yKm - from.yKm) * deltaY) / lengthSquared;
         if (projection < 0.0f)
         {
            projection = 0.0f;
         }
         if (projection > 1.0f)
         {
            projection = 1.0f;
         }

         MapPoint closest;
         closest.xKm = from.xKm + deltaX * projection;
         closest.yKm = from.yKm + deltaY * projection;
         distanceAlongKm = std::sqrt(lengthSquared) * projection;
         return DistanceKm(point, closest);
      }

      bool LineHasPatienceEmergency(
         const World& world,
         const AiObservation& observation,
         LineId lineId)
      {
         if (!observation.patienceActive)
         {
            return false;
         }

         const Line* pLine = world.GetNetwork().FindLine(lineId);
         if (pLine == nullptr)
         {
            return false;
         }

         for (const AiStationState& station : observation.stations)
         {
            bool onLine = false;
            for (StationId lineStationId : pLine->stationIds)
            {
               if (lineStationId == station.stationId)
               {
                  onLine = true;
                  break;
               }
            }
            if (!onLine)
            {
               continue;
            }
            if (station.patienceLimitSeconds <= 0.0f)
            {
               continue;
            }

            const float fraction = station.maxWaitedSeconds / station.patienceLimitSeconds;
            if (fraction >= AiPatienceEmergencyFraction)
            {
               return true;
            }
         }

         return false;
      }

      bool ActionHasPatienceEmergency(
         const World& world,
         const AiObservation& observation,
         const AiAction& action)
      {
         if (action.type == AiActionType::AddTrain)
         {
            return LineHasPatienceEmergency(world, observation, action.lineId);
         }
         if (action.type == AiActionType::ExtendLine ||
            action.type == AiActionType::InsertStation)
         {
            for (const AiStationState& station : observation.stations)
            {
               if (station.stationId != action.stationId)
               {
                  continue;
               }
               if (station.patienceLimitSeconds <= 0.0f)
               {
                  return false;
               }

               const float fraction = station.maxWaitedSeconds / station.patienceLimitSeconds;
               return fraction >= AiPatienceEmergencyFraction;
            }
         }
         if (action.type == AiActionType::AddLine)
         {
            for (StationId stationId : action.stationIds)
            {
               for (const AiStationState& station : observation.stations)
               {
                  if (station.stationId != stationId)
                  {
                     continue;
                  }
                  if (station.patienceLimitSeconds <= 0.0f)
                  {
                     continue;
                  }

                  const float fraction = station.maxWaitedSeconds / station.patienceLimitSeconds;
                  if (fraction >= AiPatienceEmergencyFraction)
                  {
                     return true;
                  }
               }
            }
         }

         return false;
      }

      int32_t StationComponentId(const AiObservation& observation, StationId stationId)
      {
         for (const AiStationState& station : observation.stations)
         {
            if (station.stationId == stationId)
            {
               return station.networkComponentId;
            }
         }

         return -1;
      }

      int32_t LineComponentId(
         const World& world,
         const AiObservation& observation,
         LineId lineId)
      {
         const Line* pLine = world.GetNetwork().FindLine(lineId);
         if (pLine == nullptr || pLine->stationIds.empty())
         {
            return -1;
         }

         return StationComponentId(observation, pLine->stationIds.front());
      }

      bool ActionBridgesComponents(
         const World& world,
         const AiObservation& observation,
         const AiAction& action)
      {
         if (action.type == AiActionType::AddLine && action.stationIds.size() >= 2)
         {
            const int32_t leftComponent =
               StationComponentId(observation, action.stationIds.front());
            const int32_t rightComponent =
               StationComponentId(observation, action.stationIds.back());
            if (leftComponent < 0 || rightComponent < 0)
            {
               return false;
            }

            return leftComponent != rightComponent;
         }

         if (action.type == AiActionType::ExtendLine ||
            action.type == AiActionType::InsertStation)
         {
            const int32_t targetComponent = StationComponentId(observation, action.stationId);
            const int32_t lineComponent = LineComponentId(world, observation, action.lineId);
            if (targetComponent < 0 || lineComponent < 0)
            {
               return false;
            }

            return targetComponent != lineComponent;
         }

         return false;
      }

      void ConsiderAction(
         const World& world,
         const AiObservation& observation,
         AiAction& candidate,
         AiAction& bestAction)
      {
         const bool allowEmergencySpend =
            ActionHasPatienceEmergency(world, observation, candidate) ||
            ActionBridgesComponents(world, observation, candidate);
         const AllowAiReserveSpend allowReserve = allowEmergencySpend
            ? AllowAiReserveSpend::Yes
            : AllowAiReserveSpend::No;
         if (!AiActionIsAffordable(world, candidate, allowReserve))
         {
            return;
         }

         ScoreAiAction(world, observation, candidate);
         if (candidate.score > bestAction.score)
         {
            bestAction = candidate;
         }
      }

      void GenerateAddTrainActions(
         const World& world,
         const AiObservation& observation,
         AiAction& bestAction)
      {
         for (const AiLineState& line : observation.lines)
         {
            AiAction action;
            action.type = AiActionType::AddTrain;
            action.lineId = line.lineId;
            ConsiderAction(world, observation, action, bestAction);
         }
      }

      bool StationsShareALine(const Network& network, StationId leftId, StationId rightId)
      {
         for (const Line& line : network.GetLines())
         {
            bool hasLeft = false;
            bool hasRight = false;
            for (StationId stationId : line.stationIds)
            {
               if (stationId == leftId)
               {
                  hasLeft = true;
               }
               if (stationId == rightId)
               {
                  hasRight = true;
               }
            }
            if (hasLeft && hasRight)
            {
               return true;
            }
         }

         return false;
      }

      float PairDistanceKm(const World& world, StationId leftId, StationId rightId)
      {
         const StationRecord* pLeft = world.GetNetwork().FindStation(leftId);
         const StationRecord* pRight = world.GetNetwork().FindStation(rightId);
         if (pLeft == nullptr || pRight == nullptr)
         {
            return 1.0e9f;
         }

         return DistanceKm(pLeft->position, pRight->position);
      }

      void GenerateAddLineActions(
         const World& world,
         const AiObservation& observation,
         const CityRankList& ranks,
         AiAction& bestAction)
      {
         for (size_t leftIndex = 0; leftIndex < ranks.size(); ++leftIndex)
         {
            for (size_t rightIndex = leftIndex + 1; rightIndex < ranks.size(); ++rightIndex)
            {
               if (StationsShareALine(
                  world.GetNetwork(),
                  ranks[leftIndex].stationId,
                  ranks[rightIndex].stationId))
               {
                  continue;
               }

               const bool seedNetwork = world.GetNetwork().GetLines().empty();
               if (!seedNetwork)
               {
                  const int32_t leftComponent =
                     StationComponentId(observation, ranks[leftIndex].stationId);
                  const int32_t rightComponent =
                     StationComponentId(observation, ranks[rightIndex].stationId);
                  const bool isBridge =
                     leftComponent >= 0 &&
                     rightComponent >= 0 &&
                     leftComponent != rightComponent;
                  const float maxDistanceKm =
                     isBridge ? AiMaxBridgeDistanceKm : AiMaxAddLineDistanceKm;
                  const float distanceKm = PairDistanceKm(
                     world,
                     ranks[leftIndex].stationId,
                     ranks[rightIndex].stationId);
                  if (distanceKm > maxDistanceKm)
                  {
                     continue;
                  }
               }

               AiAction action;
               action.type = AiActionType::AddLine;
               action.stationIds.push_back(ranks[leftIndex].stationId);
               action.stationIds.push_back(ranks[rightIndex].stationId);
               ConsiderAction(world, observation, action, bestAction);
            }
         }
      }

      float ExtendDistanceKm(
         const World& world,
         const Line& line,
         LineEnd end,
         StationId stationId)
      {
         if (line.stationIds.empty())
         {
            return 1.0e9f;
         }

         StationId terminusId = InvalidStationId;
         if (end == LineEnd::Front)
         {
            terminusId = line.stationIds.front();
         }
         else
         {
            terminusId = line.stationIds.back();
         }

         return PairDistanceKm(world, terminusId, stationId);
      }

      void GenerateExtendActions(
         const World& world,
         const AiObservation& observation,
         const CityRankList& ranks,
         AiAction& bestAction)
      {
         for (const Line& line : world.GetNetwork().GetLines())
         {
            if (line.stationIds.size() >= AiMaxLineStationCount)
            {
               continue;
            }

            for (const CityRank& city : ranks)
            {
               if (StationAlreadyOnLine(line, city.stationId))
               {
                  continue;
               }

               AiAction frontAction;
               frontAction.type = AiActionType::ExtendLine;
               frontAction.lineId = line.id;
               frontAction.lineEnd = LineEnd::Front;
               frontAction.stationId = city.stationId;
               const int32_t lineComponent = LineComponentId(world, observation, line.id);
               const int32_t cityComponent = StationComponentId(observation, city.stationId);
               const bool isBridge =
                  lineComponent >= 0 &&
                  cityComponent >= 0 &&
                  lineComponent != cityComponent;
               const float maxExtendKm =
                  isBridge ? AiMaxBridgeExtendDistanceKm : AiMaxExtendDistanceKm;
               if (ExtendDistanceKm(world, line, LineEnd::Front, city.stationId) <= maxExtendKm)
               {
                  ConsiderAction(world, observation, frontAction, bestAction);
               }

               AiAction backAction;
               backAction.type = AiActionType::ExtendLine;
               backAction.lineId = line.id;
               backAction.lineEnd = LineEnd::Back;
               backAction.stationId = city.stationId;
               if (ExtendDistanceKm(world, line, LineEnd::Back, city.stationId) <= maxExtendKm)
               {
                  ConsiderAction(world, observation, backAction, bestAction);
               }
            }
         }
      }

      void GenerateInsertActions(
         const World& world,
         const AiObservation& observation,
         const CityRankList& ranks,
         AiAction& bestAction)
      {
         for (const Line& line : world.GetNetwork().GetLines())
         {
            if (line.stationIds.size() >= AiMaxLineStationCount)
            {
               continue;
            }

            const uint32_t segmentCount = LineSegmentCount(line);
            for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
            {
               StationId fromId = InvalidStationId;
               StationId toId = InvalidStationId;
               if (IsErr(LineSegmentEndpoints(line, segmentIndex, fromId, toId)))
               {
                  continue;
               }

               const StationRecord* pFrom = world.GetNetwork().FindStation(fromId);
               const StationRecord* pTo = world.GetNetwork().FindStation(toId);
               if (pFrom == nullptr || pTo == nullptr)
               {
                  continue;
               }

               for (const CityRank& city : ranks)
               {
                  if (StationAlreadyOnLine(line, city.stationId))
                  {
                     continue;
                  }

                  const StationRecord* pCity = world.GetNetwork().FindStation(city.stationId);
                  if (pCity == nullptr)
                  {
                     continue;
                  }

                  float distanceAlongKm = 0.0f;
                  const float distanceKm = PointToSegmentDistanceKm(
                     pCity->position,
                     pFrom->position,
                     pTo->position,
                     distanceAlongKm);
                  if (distanceKm > AiMaxInsertDistanceKm)
                  {
                     continue;
                  }

                  AiAction action;
                  action.type = AiActionType::InsertStation;
                  action.lineId = line.id;
                  action.segmentIndex = segmentIndex;
                  action.stationId = city.stationId;
                  ConsiderAction(world, observation, action, bestAction);
               }
            }
         }
      }

      void CollectComponentHubs(
         const AiObservation& observation,
         int32_t componentId,
         StationIdList& hubs)
      {
         hubs.clear();
         struct HubRank
         {
            StationId stationId = InvalidStationId;
            uint32_t population = 0;
         };

         std::vector<HubRank> ranks;
         for (const AiStationState& station : observation.stations)
         {
            if (station.networkComponentId != componentId)
            {
               continue;
            }

            HubRank rank;
            rank.stationId = station.stationId;
            rank.population = station.population;
            ranks.push_back(rank);
         }

         for (size_t leftIndex = 0; leftIndex < ranks.size(); ++leftIndex)
         {
            for (size_t rightIndex = leftIndex + 1; rightIndex < ranks.size(); ++rightIndex)
            {
               if (ranks[rightIndex].population > ranks[leftIndex].population)
               {
                  const HubRank swap = ranks[leftIndex];
                  ranks[leftIndex] = ranks[rightIndex];
                  ranks[rightIndex] = swap;
               }
            }
         }

         constexpr size_t MaxHubs = 4;
         const size_t hubCount = ranks.size() < MaxHubs ? ranks.size() : MaxHubs;
         for (size_t index = 0; index < hubCount; ++index)
         {
            hubs.push_back(ranks[index].stationId);
         }
      }

      void GenerateBridgeActions(
         const World& world,
         const AiObservation& observation,
         AiAction& bestAction)
      {
         if (observation.networkComponentCount < 2)
         {
            return;
         }

         for (uint32_t leftComponent = 0; leftComponent < observation.networkComponentCount;
              ++leftComponent)
         {
            StationIdList leftHubs;
            CollectComponentHubs(observation, static_cast<int32_t>(leftComponent), leftHubs);
            for (uint32_t rightComponent = leftComponent + 1;
                 rightComponent < observation.networkComponentCount;
                 ++rightComponent)
            {
               StationIdList rightHubs;
               CollectComponentHubs(
                  observation,
                  static_cast<int32_t>(rightComponent),
                  rightHubs);

               StationId bestLeftId = InvalidStationId;
               StationId bestRightId = InvalidStationId;
               float bestDistanceKm = 1.0e9f;
               for (StationId leftId : leftHubs)
               {
                  for (StationId rightId : rightHubs)
                  {
                     const float distanceKm = PairDistanceKm(world, leftId, rightId);
                     if (distanceKm < bestDistanceKm)
                     {
                        bestDistanceKm = distanceKm;
                        bestLeftId = leftId;
                        bestRightId = rightId;
                     }
                  }
               }

               if (bestLeftId == InvalidStationId || bestRightId == InvalidStationId)
               {
                  continue;
               }

               if (bestDistanceKm <= AiMaxBridgeDistanceKm)
               {
                  AiAction bridgeAction;
                  bridgeAction.type = AiActionType::AddLine;
                  bridgeAction.stationIds.push_back(bestLeftId);
                  bridgeAction.stationIds.push_back(bestRightId);
                  ConsiderAction(world, observation, bridgeAction, bestAction);
                  continue;
               }

               for (const AiStationState& stepStation : observation.stations)
               {
                  if (stepStation.networkComponentId == static_cast<int32_t>(leftComponent))
                  {
                     continue;
                  }

                  const float fromLeftKm =
                     PairDistanceKm(world, bestLeftId, stepStation.stationId);
                  if (fromLeftKm > AiMaxAddLineDistanceKm)
                  {
                     continue;
                  }

                  const float stepToRightKm =
                     PairDistanceKm(world, stepStation.stationId, bestRightId);
                  if (stepToRightKm >= bestDistanceKm)
                  {
                     continue;
                  }

                  if (StationsShareALine(
                     world.GetNetwork(),
                     bestLeftId,
                     stepStation.stationId))
                  {
                     continue;
                  }

                  AiAction stepAction;
                  stepAction.type = AiActionType::AddLine;
                  stepAction.stationIds.push_back(bestLeftId);
                  stepAction.stationIds.push_back(stepStation.stationId);
                  ConsiderAction(world, observation, stepAction, bestAction);
               }

               for (const AiStationState& stepStation : observation.stations)
               {
                  if (stepStation.networkComponentId == static_cast<int32_t>(rightComponent))
                  {
                     continue;
                  }

                  const float fromRightKm =
                     PairDistanceKm(world, bestRightId, stepStation.stationId);
                  if (fromRightKm > AiMaxAddLineDistanceKm)
                  {
                     continue;
                  }

                  const float stepToLeftKm =
                     PairDistanceKm(world, stepStation.stationId, bestLeftId);
                  if (stepToLeftKm >= bestDistanceKm)
                  {
                     continue;
                  }

                  if (StationsShareALine(
                     world.GetNetwork(),
                     bestRightId,
                     stepStation.stationId))
                  {
                     continue;
                  }

                  AiAction stepAction;
                  stepAction.type = AiActionType::AddLine;
                  stepAction.stationIds.push_back(bestRightId);
                  stepAction.stationIds.push_back(stepStation.stationId);
                  ConsiderAction(world, observation, stepAction, bestAction);
               }
            }
         }
      }
   } // namespace

   void PlanAiAction(const World& world, AiAction& action)
   {
      action = AiAction();
      action.type = AiActionType::Noop;
      action.score = 0.0f;

      AiObservation observation;
      BuildAiObservation(world, observation);

      CityRankList ranks;
      BuildCityRanks(observation, ranks);

      AiAction bestAction;
      bestAction.type = AiActionType::Noop;
      bestAction.score = 50.0f;

      GenerateBridgeActions(world, observation, bestAction);
      GenerateAddTrainActions(world, observation, bestAction);
      GenerateAddLineActions(world, observation, ranks, bestAction);
      GenerateExtendActions(world, observation, ranks, bestAction);
      GenerateInsertActions(world, observation, ranks, bestAction);

      if (bestAction.type == AiActionType::Noop || bestAction.score <= 50.0f)
      {
         action.type = AiActionType::Noop;
         action.score = 0.0f;
         return;
      }

      action = bestAction;
   }
} // namespace MiniDb
