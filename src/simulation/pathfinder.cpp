/*!
 *\file pathfinder.cpp
 *\brief Shortest passenger routes on the player-built network.
 */

#include "simulation/pathfinder.h"

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

      uint32_t FindOpenNodeIndex(const SearchNodeList& nodes, StationId stationId, LineId lineId)
      {
         for (uint32_t index = 0; index < nodes.size(); ++index)
         {
            if (nodes[index].isClosed)
            {
               continue;
            }
            if (nodes[index].stationId == stationId && nodes[index].lineId == lineId)
            {
               return index;
            }
         }

         return InvalidIndex;
      }

      bool HasClosedNode(const SearchNodeList& nodes, StationId stationId, LineId lineId)
      {
         for (const SearchNode& node : nodes)
         {
            if (node.isClosed && node.stationId == stationId && node.lineId == lineId)
            {
               return true;
            }
         }

         return false;
      }

      uint32_t FindCheapestOpenIndex(const SearchNodeList& nodes)
      {
         uint32_t bestIndex = InvalidIndex;
         float bestCost = 0.0f;
         bool hasBest = false;
         for (uint32_t index = 0; index < nodes.size(); ++index)
         {
            if (nodes[index].isClosed)
            {
               continue;
            }
            if (!hasBest || nodes[index].costSeconds < bestCost)
            {
               bestCost = nodes[index].costSeconds;
               bestIndex = index;
               hasBest = true;
            }
         }

         return bestIndex;
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

      uint32_t destinationNodeIndex = InvalidIndex;

      while (true)
      {
         const uint32_t currentIndex = FindCheapestOpenIndex(nodes);
         if (currentIndex == InvalidIndex)
         {
            break;
         }

         nodes[currentIndex].isClosed = true;
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
            if (HasClosedNode(nodes, edge.toStationId, edge.lineId))
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
            const uint32_t existingIndex = FindOpenNodeIndex(nodes, edge.toStationId, edge.lineId);
            if (existingIndex != InvalidIndex)
            {
               if (newCost < nodes[existingIndex].costSeconds)
               {
                  nodes[existingIndex].costSeconds = newCost;
                  nodes[existingIndex].previousIndex = currentIndex;
               }
               continue;
            }

            SearchNode nextNode;
            nextNode.stationId = edge.toStationId;
            nextNode.lineId = edge.lineId;
            nextNode.costSeconds = newCost;
            nextNode.previousIndex = currentIndex;
            nextNode.isClosed = false;
            nodes.push_back(nextNode);
         }
      }

      if (destinationNodeIndex == InvalidIndex)
      {
         return Result::Error;
      }

      return ReconstructRoute(nodes, destinationNodeIndex, route);
   }
} // namespace MiniDb
