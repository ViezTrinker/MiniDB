/*!
 *\file ai_observation.cpp
 *\brief World snapshot used by the spectator AI planner.
 */

#include "ai/ai_observation.h"

#include <vector>

#include "core/constants.h"
#include "simulation/pathfinder.h"
#include "simulation/track_inventory.h"

namespace MiniDb
{
   namespace
   {
      uint32_t CountTrainsOnLine(const World& world, LineId lineId)
      {
         uint32_t count = 0;
         for (const Train& train : world.GetTrains())
         {
            if (train.lineId == lineId)
            {
               ++count;
            }
         }

         return count;
      }

      float StationPatienceLimitSeconds(const World& world, StationId stationId)
      {
         if (world.GetNetwork().IsStationOnAnyLine(stationId))
         {
            return MaxPassengerPlatformWaitSeconds;
         }

         return MaxUnconnectedPassengerPlatformWaitSeconds;
      }

      float MaxWaitedAtStation(const World& world, StationId stationId)
      {
         float maxWaited = 0.0f;
         const float simTime = world.GetSimulationTimeSeconds();
         for (const Passenger& passenger : world.GetPassengers())
         {
            if (passenger.state != PassengerState::Waiting)
            {
               continue;
            }
            if (passenger.currentStationId != stationId)
            {
               continue;
            }

            float patienceStart = passenger.platformArrivalTimeSeconds;
            if (patienceStart < PlatformPatienceGraceSeconds)
            {
               patienceStart = PlatformPatienceGraceSeconds;
            }

            float waited = simTime - patienceStart;
            if (waited < 0.0f)
            {
               waited = 0.0f;
            }
            if (waited > maxWaited)
            {
               maxWaited = waited;
            }
         }

         return maxWaited;
      }

      int64_t EstimateAddLineCost(const World& world, const StationIdList& stationIds)
      {
         if (!world.GetEconomy().IsEconomicMode())
         {
            return 0;
         }

         TrackSegmentRecordList candidateSegments;
         CollectSegmentsForLineDefinition(world.GetNetwork(), stationIds, candidateSegments);
         TrackSegmentRecordList newSegments;
         const float newTrackKm =
            world.GetEconomy().CollectNewSegmentKilometers(candidateSegments, newSegments);
         return world.GetEconomy().TrackBuildCost(newTrackKm) +
            world.GetEconomy().TrainPurchaseCostScaled();
      }

      int64_t EstimateExtendCost(
         const World& world,
         LineId lineId,
         LineEnd end,
         StationId stationId)
      {
         if (!world.GetEconomy().IsEconomicMode())
         {
            return 0;
         }

         const Line* pLine = world.GetNetwork().FindLine(lineId);
         const StationRecord* pNewStation = world.GetNetwork().FindStation(stationId);
         if (pLine == nullptr || pNewStation == nullptr || pLine->stationIds.empty())
         {
            return 0;
         }

         StationId terminusId = InvalidStationId;
         if (end == LineEnd::Front)
         {
            terminusId = pLine->stationIds.front();
         }
         else
         {
            terminusId = pLine->stationIds.back();
         }

         const StationRecord* pTerminus = world.GetNetwork().FindStation(terminusId);
         if (pTerminus == nullptr)
         {
            return 0;
         }

         TrackSegmentRecordList candidateSegments;
         TrackSegmentRecord segment;
         segment.stationA = terminusId;
         segment.stationB = stationId;
         segment.distanceKm = DistanceKm(pTerminus->position, pNewStation->position);
         candidateSegments.push_back(segment);
         TrackSegmentRecordList newSegments;
         const float newTrackKm =
            world.GetEconomy().CollectNewSegmentKilometers(candidateSegments, newSegments);
         return world.GetEconomy().TrackBuildCost(newTrackKm);
      }

      int64_t EstimateInsertCost(
         const World& world,
         LineId lineId,
         uint32_t segmentIndex,
         StationId stationId)
      {
         if (!world.GetEconomy().IsEconomicMode())
         {
            return 0;
         }

         const Line* pLine = world.GetNetwork().FindLine(lineId);
         const StationRecord* pInserted = world.GetNetwork().FindStation(stationId);
         if (pLine == nullptr || pInserted == nullptr)
         {
            return 0;
         }

         StationId fromId = InvalidStationId;
         StationId toId = InvalidStationId;
         if (IsErr(LineSegmentEndpoints(*pLine, segmentIndex, fromId, toId)))
         {
            return 0;
         }

         const StationRecord* pFrom = world.GetNetwork().FindStation(fromId);
         const StationRecord* pTo = world.GetNetwork().FindStation(toId);
         if (pFrom == nullptr || pTo == nullptr)
         {
            return 0;
         }

         TrackSegmentRecordList candidateSegments;
         TrackSegmentRecord firstSegment;
         firstSegment.stationA = fromId;
         firstSegment.stationB = stationId;
         firstSegment.distanceKm = DistanceKm(pFrom->position, pInserted->position);
         candidateSegments.push_back(firstSegment);

         TrackSegmentRecord secondSegment;
         secondSegment.stationA = stationId;
         secondSegment.stationB = toId;
         secondSegment.distanceKm = DistanceKm(pInserted->position, pTo->position);
         candidateSegments.push_back(secondSegment);

         TrackSegmentRecordList newSegments;
         const float newTrackKm =
            world.GetEconomy().CollectNewSegmentKilometers(candidateSegments, newSegments);
         return world.GetEconomy().TrackBuildCost(newTrackKm);
      }

      size_t FindStationObservationIndex(
         const AiObservation& observation,
         StationId stationId)
      {
         for (size_t index = 0; index < observation.stations.size(); ++index)
         {
            if (observation.stations[index].stationId == stationId)
            {
               return index;
            }
         }

         return observation.stations.size();
      }

      int32_t FindComponentRoot(std::vector<int32_t>& parents, int32_t index)
      {
         int32_t root = index;
         while (parents[static_cast<size_t>(root)] != root)
         {
            root = parents[static_cast<size_t>(root)];
         }

         int32_t walk = index;
         while (walk != root)
         {
            const int32_t next = parents[static_cast<size_t>(walk)];
            parents[static_cast<size_t>(walk)] = root;
            walk = next;
         }

         return root;
      }

      void UnionComponent(std::vector<int32_t>& parents, int32_t left, int32_t right)
      {
         const int32_t leftRoot = FindComponentRoot(parents, left);
         const int32_t rightRoot = FindComponentRoot(parents, right);
         if (leftRoot != rightRoot)
         {
            parents[static_cast<size_t>(rightRoot)] = leftRoot;
         }
      }

      void AssignNetworkComponents(const World& world, AiObservation& observation)
      {
         observation.networkComponentCount = 0;
         if (observation.stations.empty())
         {
            return;
         }

         std::vector<int32_t> parents(observation.stations.size(), 0);
         for (size_t index = 0; index < parents.size(); ++index)
         {
            parents[index] = static_cast<int32_t>(index);
            observation.stations[index].networkComponentId = -1;
         }

         for (const Line& line : world.GetNetwork().GetLines())
         {
            size_t previousIndex = observation.stations.size();
            for (StationId stationId : line.stationIds)
            {
               const size_t stationIndex = FindStationObservationIndex(observation, stationId);
               if (stationIndex >= observation.stations.size())
               {
                  continue;
               }

               if (previousIndex < observation.stations.size())
               {
                  UnionComponent(
                     parents,
                     static_cast<int32_t>(previousIndex),
                     static_cast<int32_t>(stationIndex));
               }

               previousIndex = stationIndex;
            }
         }

         std::vector<int32_t> rootToComponent(observation.stations.size(), -1);
         int32_t nextComponentId = 0;
         for (size_t index = 0; index < observation.stations.size(); ++index)
         {
            if (!observation.stations[index].connected)
            {
               continue;
            }

            const int32_t root = FindComponentRoot(parents, static_cast<int32_t>(index));
            if (rootToComponent[static_cast<size_t>(root)] < 0)
            {
               rootToComponent[static_cast<size_t>(root)] = nextComponentId;
               ++nextComponentId;
            }

            observation.stations[index].networkComponentId =
               rootToComponent[static_cast<size_t>(root)];
         }

         observation.networkComponentCount = static_cast<uint32_t>(nextComponentId);
      }
   } // namespace

   void BuildAiObservation(const World& world, AiObservation& observation)
   {
      observation.simulationTimeSeconds = world.GetSimulationTimeSeconds();
      observation.balance = world.GetBalance();
      observation.uniqueTrackKm = TotalUniqueTrackKilometers(world.GetNetwork());
      observation.trainCount = static_cast<uint32_t>(world.GetTrains().size());
      observation.maintenancePerSecond = world.GetEconomy().GetMaintenancePerSecond(
         observation.uniqueTrackKm,
         observation.trainCount);
      observation.gameMode = world.GetGameMode();
      observation.neverLose = world.GetEconomy().GetNeverLose();
      observation.trainCapacity = world.GetTrainCapacity();
      observation.patienceActive =
         world.GetEconomy().IsEconomicMode() &&
         world.GetEconomy().GetNeverLose() == NeverLose::No &&
         observation.simulationTimeSeconds >= PlatformPatienceGraceSeconds;
      observation.networkComponentCount = 0;
      observation.stations.clear();
      observation.lines.clear();
      observation.unconnectedStationIds.clear();

      for (const StationRecord& station : world.GetNetwork().GetStations())
      {
         AiStationState state;
         state.stationId = station.id;
         state.population = station.population;
         state.waitingCount = world.GetWaitingCountAt(station.id);
         state.maxWaitedSeconds = MaxWaitedAtStation(world, station.id);
         state.patienceLimitSeconds = StationPatienceLimitSeconds(world, station.id);
         state.connected = world.GetNetwork().IsStationOnAnyLine(station.id);
         state.networkComponentId = -1;
         state.position = station.position;
         observation.stations.push_back(state);
      }

      world.CollectUnconnectedStations(observation.unconnectedStationIds);
      AssignNetworkComponents(world, observation);

      for (const Line& line : world.GetNetwork().GetLines())
      {
         AiLineState state;
         state.lineId = line.id;
         state.trainCount = CountTrainsOnLine(world, line.id);
         state.stationCount = static_cast<uint32_t>(line.stationIds.size());
         state.cycleTimeSeconds = LineCycleTimeSeconds(world.GetNetwork(), line);
         if (state.trainCount == 0)
         {
            state.expectedWaitSeconds = state.cycleTimeSeconds;
         }
         else
         {
            state.expectedWaitSeconds =
               ExpectedWaitHeadwayFraction * (state.cycleTimeSeconds / static_cast<float>(state.trainCount));
         }

         for (StationId stationId : line.stationIds)
         {
            state.waitingAlongLine += world.GetWaitingCountAt(stationId);
         }

         observation.lines.push_back(state);
      }
   }

   int64_t EstimateAiActionCost(const World& world, const AiAction& action)
   {
      if (!world.GetEconomy().IsEconomicMode())
      {
         return 0;
      }

      if (action.type == AiActionType::AddLine)
      {
         return EstimateAddLineCost(world, action.stationIds);
      }
      if (action.type == AiActionType::ExtendLine)
      {
         return EstimateExtendCost(world, action.lineId, action.lineEnd, action.stationId);
      }
      if (action.type == AiActionType::InsertStation)
      {
         return EstimateInsertCost(world, action.lineId, action.segmentIndex, action.stationId);
      }
      if (action.type == AiActionType::AddTrain)
      {
         return world.GetEconomy().TrainPurchaseCostScaled();
      }

      return 0;
   }

   int64_t AiRequiredCashReserve(const World& world)
   {
      if (!world.GetEconomy().IsEconomicMode())
      {
         return 0;
      }

      const float uniqueTrackKm = TotalUniqueTrackKilometers(world.GetNetwork());
      const auto trainCount = static_cast<uint32_t>(world.GetTrains().size());
      const float maintenancePerSecond =
         world.GetEconomy().GetMaintenancePerSecond(uniqueTrackKm, trainCount);
      const auto maintenanceReserve = static_cast<int64_t>(
         maintenancePerSecond * AiCashReserveMaintenanceSeconds);
      if (maintenanceReserve > AiMinCashReserve)
      {
         return maintenanceReserve;
      }

      return AiMinCashReserve;
   }

   bool AiActionIsAffordable(
      const World& world,
      const AiAction& action,
      AllowAiReserveSpend allowReserveSpend)
   {
      if (!world.GetEconomy().IsEconomicMode())
      {
         return true;
      }
      if (action.type == AiActionType::Noop || action.type == AiActionType::RemoveLine)
      {
         return true;
      }

      const int64_t cost = EstimateAiActionCost(world, action);
      if (!world.GetEconomy().CanAfford(cost))
      {
         return false;
      }

      if (allowReserveSpend == AllowAiReserveSpend::Yes)
      {
         return true;
      }

      const int64_t balanceAfter = world.GetBalance() - cost;
      return balanceAfter >= AiRequiredCashReserve(world);
   }

   Result ApplyAiAction(World& world, const AiAction& action)
   {
      if (action.type == AiActionType::Noop)
      {
         return Result::Ok;
      }
      if (!AiActionIsAffordable(world, action, AllowAiReserveSpend::Yes))
      {
         return Result::InsufficientFunds;
      }

      if (action.type == AiActionType::AddLine)
      {
         LineId lineId = InvalidLineId;
         return world.AddLine(action.stationIds, lineId);
      }
      if (action.type == AiActionType::ExtendLine)
      {
         return world.ExtendLineAt(action.lineId, action.lineEnd, action.stationId);
      }
      if (action.type == AiActionType::InsertStation)
      {
         return world.InsertStationOnLine(action.lineId, action.segmentIndex, action.stationId);
      }
      if (action.type == AiActionType::AddTrain)
      {
         return world.AddTrainToLine(action.lineId);
      }
      if (action.type == AiActionType::RemoveLine)
      {
         return world.RemoveLine(action.lineId);
      }

      return Result::InvalidArgument;
   }
} // namespace MiniDb
