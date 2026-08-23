/*!
 *\file pathfinder.cpp
 *\brief Shortest passenger routes on the player-built network.
 */

#include "simulation/pathfinder.h"

#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "core/constants.h"

namespace MiniDb
{
   namespace
   {
      struct SearchNode
      {
         StationId stationId;
         LineId lineId;
         float costSeconds;
         uint32_t previousIndex;
         bool isClosed;
      };

      using SearchNodeList = std::vector<SearchNode>;
      using OpenNodeIndexMap = std::unordered_map<uint64_t, uint32_t>;
      using ClosedNodeKeySet = std::unordered_set<uint64_t>;

      uint64_t NodeKey(StationId stationId, LineId lineId)
      {
         return (static_cast<uint64_t>(stationId) << 32) |
            static_cast<uint64_t>(lineId);
      }

      struct OpenNodeCompare
      {
         const SearchNodeList* pNodes;

         bool operator()(uint32_t leftIndex, uint32_t rightIndex) const
         {
            return (*pNodes)[leftIndex].costSeconds > (*pNodes)[rightIndex].costSeconds;
         }
      };

      using OpenNodeHeap = std::priority_queue<uint32_t, std::vector<uint32_t>, OpenNodeCompare>;

      uint32_t FindOpenNodeIndex(const OpenNodeIndexMap& openIndexByKey, StationId stationId, LineId lineId)
      {
         const auto iterator = openIndexByKey.find(NodeKey(stationId, lineId));
         if (iterator == openIndexByKey.end())
         {
            return InvalidIndex;
         }

         return iterator->second;
      }

      bool HasClosedNode(const ClosedNodeKeySet& closedKeys, StationId stationId, LineId lineId)
      {
         return closedKeys.find(NodeKey(stationId, lineId)) != closedKeys.end();
      }

      uint32_t StationListIndex(const StationRecordList& stations, StationId stationId)
      {
         for (uint32_t index = 0; index < stations.size(); ++index)
         {
            if (stations[index].id == stationId)
            {
               return index;
            }
         }

         return InvalidIndex;
      }

      float WaitSecondsForLine(const LineWaitList& lineWaits, LineId lineId)
      {
         for (const LineWait& lineWait : lineWaits)
         {
            if (lineWait.lineId == lineId)
            {
               return lineWait.waitSeconds;
            }
         }

         return 0.0f;
      }

      Result ReconstructRoute(const SearchNodeList& nodes, uint32_t destinationIndex, Route& route)
      {
         route.clear();
         StationIdList stationTrace;
         std::vector<LineId> lineTrace;

         uint32_t index = destinationIndex;
         while (index != InvalidIndex)
         {
            stationTrace.push_back(nodes[index].stationId);
            lineTrace.push_back(nodes[index].lineId);
            index = nodes[index].previousIndex;
         }

         if (stationTrace.size() < 2)
         {
            return Result::Error;
         }

         const auto lastTraceIndex = static_cast<uint32_t>(stationTrace.size() - 1);
         for (uint32_t reverseIndex = lastTraceIndex; reverseIndex > 0; --reverseIndex)
         {
            const uint32_t fromTraceIndex = reverseIndex;
            const uint32_t toTraceIndex = reverseIndex - 1;
            if (lineTrace[toTraceIndex] == InvalidLineId)
            {
               continue;
            }

            RouteHop hop;
            hop.lineId = lineTrace[toTraceIndex];
            hop.fromStationId = stationTrace[fromTraceIndex];
            hop.toStationId = stationTrace[toTraceIndex];
            route.push_back(hop);
         }

         if (route.empty())
         {
            return Result::Error;
         }

         return Result::Ok;
      }
   } // namespace

   float LineCycleTimeSeconds(const Network& network, const Line& line)
   {
      const uint32_t segmentCount = LineSegmentCount(line);
      float oneWayTravelSeconds = 0.0f;
      for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
      {
         StationId fromId = InvalidStationId;
         StationId toId = InvalidStationId;
         const Result endpointResult = LineSegmentEndpoints(line, segmentIndex, fromId, toId);
         if (IsErr(endpointResult))
         {
            continue;
         }

         const StationRecord* pFrom = network.FindStation(fromId);
         const StationRecord* pTo = network.FindStation(toId);
         if (pFrom == nullptr || pTo == nullptr)
         {
            continue;
         }

         const float distanceKm = DistanceKm(pFrom->position, pTo->position);
         oneWayTravelSeconds += (distanceKm / TrainSpeedKmPerHour) * SecondsPerHour;
      }

      const auto stationCount = static_cast<float>(line.stationIds.size());
      const float oneWayDwellSeconds = stationCount * TrainDwellSeconds;
      if (line.loop == LineLoop::Yes)
      {
         return oneWayTravelSeconds + oneWayDwellSeconds;
      }

      return (2.0f * oneWayTravelSeconds) + (2.0f * oneWayDwellSeconds);
   }

   float ExpectedLineWaitSeconds(float cycleTimeSeconds, uint32_t trainCount)
   {
      float clampedCycleTimeSeconds = cycleTimeSeconds;
      if (clampedCycleTimeSeconds < 0.0f)
      {
         clampedCycleTimeSeconds = 0.0f;
      }
      if (trainCount == 0)
      {
         if (clampedCycleTimeSeconds <= 0.0f)
         {
            return TrainDwellSeconds;
         }

         return clampedCycleTimeSeconds;
      }

      return ExpectedWaitHeadwayFraction * clampedCycleTimeSeconds / static_cast<float>(trainCount);
   }

   Result FindRoute(
      const Network& network,
      StationId originId,
      StationId destinationId,
      const LineWaitList& lineWaits,
      Route& route)
   {
      route.clear();
      if (originId == destinationId)
      {
         return Result::Ok;
      }
      if (network.FindStation(originId) == nullptr || network.FindStation(destinationId) == nullptr)
      {
         return Result::StationNotFound;
      }

      const StationRecordList& stations = network.GetStations();
      const AdjacencyList& adjacency = network.GetAdjacency();
      const uint32_t originIndex = StationListIndex(stations, originId);
      if (originIndex == InvalidIndex)
      {
         return Result::StationNotFound;
      }

      SearchNodeList nodes;
      SearchNode start;
      start.stationId = originId;
      start.lineId = InvalidLineId;
      start.costSeconds = 0.0f;
      start.previousIndex = InvalidIndex;
      start.isClosed = false;
      nodes.push_back(start);

      OpenNodeCompare compare;
      compare.pNodes = &nodes;
      OpenNodeHeap openHeap(compare);
      openHeap.push(0);

      OpenNodeIndexMap openIndexByKey;
      openIndexByKey[NodeKey(originId, InvalidLineId)] = 0;

      ClosedNodeKeySet closedKeys;
      uint32_t destinationNodeIndex = InvalidIndex;

      while (!openHeap.empty())
      {
         const uint32_t currentIndex = openHeap.top();
         openHeap.pop();
         if (nodes[currentIndex].isClosed)
         {
            continue;
         }

         nodes[currentIndex].isClosed = true;
         closedKeys.insert(NodeKey(nodes[currentIndex].stationId, nodes[currentIndex].lineId));
         openIndexByKey.erase(NodeKey(nodes[currentIndex].stationId, nodes[currentIndex].lineId));

         if (nodes[currentIndex].stationId == destinationId)
         {
            destinationNodeIndex = currentIndex;
            break;
         }

         const uint32_t stationIndex = StationListIndex(stations, nodes[currentIndex].stationId);
         if (stationIndex == InvalidIndex || stationIndex >= adjacency.size())
         {
            continue;
         }

         const LineId currentLineId = nodes[currentIndex].lineId;
         for (const NetworkEdge& edge : adjacency[stationIndex])
         {
            if (HasClosedNode(closedKeys, edge.toStationId, edge.lineId))
            {
               continue;
            }

            float extraCost = edge.travelTimeSeconds;
            if (currentLineId != edge.lineId)
            {
               extraCost += WaitSecondsForLine(lineWaits, edge.lineId);
               if (currentLineId != InvalidLineId)
               {
                  extraCost += TrainDwellSeconds;
               }
            }

            const float newCost = nodes[currentIndex].costSeconds + extraCost;
            const uint32_t existingIndex = FindOpenNodeIndex(openIndexByKey, edge.toStationId, edge.lineId);
            if (existingIndex != InvalidIndex)
            {
               if (newCost < nodes[existingIndex].costSeconds)
               {
                  nodes[existingIndex].costSeconds = newCost;
                  nodes[existingIndex].previousIndex = currentIndex;
                  openHeap.push(existingIndex);
               }
               continue;
            }

            SearchNode nextNode;
            nextNode.stationId = edge.toStationId;
            nextNode.lineId = edge.lineId;
            nextNode.costSeconds = newCost;
            nextNode.previousIndex = currentIndex;
            nextNode.isClosed = false;
            const uint32_t nextIndex = static_cast<uint32_t>(nodes.size());
            nodes.push_back(nextNode);
            openIndexByKey[NodeKey(edge.toStationId, edge.lineId)] = nextIndex;
            openHeap.push(nextIndex);
         }
      }

      if (destinationNodeIndex == InvalidIndex)
      {
         return Result::Error;
      }

      return ReconstructRoute(nodes, destinationNodeIndex, route);
   }
} // namespace MiniDb
