/*!
 *\file network.cpp
 *\brief Stations, lines and the travel graph.
 */

#include "simulation/network.h"

#include "core/constants.h"
#include "simulation/pathfinder.h"

namespace MiniDb
{
   namespace
   {
      bool ContainsStationId(const StationIdList& stationIds, StationId stationId)
      {
         for (StationId candidate : stationIds)
         {
            if (candidate == stationId)
            {
               return true;
            }
         }

         return false;
      }

      bool IsClosedLoopList(const StationIdList& stationIds)
      {
         if (stationIds.size() < (MinimumLoopStations + 1))
         {
            return false;
         }

         return stationIds.front() == stationIds.back();
      }

      void AddDirectedEdge(
         AdjacencyList& adjacency,
         uint32_t fromIndex,
         StationId toId,
         LineId lineId,
         float distanceKm,
         float travelTimeSeconds)
      {
         NetworkEdge edge;
         edge.toStationId = toId;
         edge.lineId = lineId;
         edge.distanceKm = distanceKm;
         edge.travelTimeSeconds = travelTimeSeconds;
         adjacency[fromIndex].push_back(edge);
      }

      void AddUndirectedEdge(
         AdjacencyList& adjacency,
         uint32_t fromIndex,
         uint32_t toIndex,
         StationId fromId,
         StationId toId,
         LineId lineId,
         float distanceKm,
         float travelTimeSeconds)
      {
         AddDirectedEdge(adjacency, fromIndex, toId, lineId, distanceKm, travelTimeSeconds);
         AddDirectedEdge(adjacency, toIndex, fromId, lineId, distanceKm, travelTimeSeconds);
      }
   } // namespace

   void Network::Clear(void)
   {
      _stations.clear();
      _lines.clear();
      _adjacency.clear();
      _lineVectorIndexById.clear();
      _nextLineId = 1;
      _revision = 0;
      _createdLineCount = 0;
   }

   void Network::EnsureLineIndexCapacity(LineId lineId)
   {
      const auto requiredSize = static_cast<size_t>(lineId) + 1U;
      if (_lineVectorIndexById.size() < requiredSize)
      {
         _lineVectorIndexById.resize(requiredSize, InvalidIndex);
      }
   }

   void Network::RebuildLineIndexMap(void)
   {
      _lineVectorIndexById.clear();
      for (uint32_t index = 0; index < _lines.size(); ++index)
      {
         EnsureLineIndexCapacity(_lines[index].id);
         _lineVectorIndexById[_lines[index].id] = index;
      }
   }

   uint32_t Network::GetStationVectorIndex(StationId stationId) const
   {
      return StationVectorIndex(stationId);
   }

   uint32_t Network::StationVectorIndex(StationId stationId) const
   {
      for (uint32_t index = 0; index < _stations.size(); ++index)
      {
         if (_stations[index].id == stationId)
         {
            return index;
         }
      }

      return InvalidIndex;
   }

   Result Network::ValidateLineStations(const StationIdList& stationIds) const
   {
      if (stationIds.size() < MinimumLineStations)
      {
         return Result::LineTooShort;
      }

      const bool closedLoop = IsClosedLoopList(stationIds);
      uint32_t uniqueCount = static_cast<uint32_t>(stationIds.size());
      if (closedLoop)
      {
         uniqueCount -= 1;
         if (uniqueCount < MinimumLoopStations)
         {
            return Result::LineTooShort;
         }
      }

      for (uint32_t index = 0; index < uniqueCount; ++index)
      {
         if (FindStation(stationIds[index]) == nullptr)
         {
            return Result::StationNotFound;
         }

         for (uint32_t previous = 0; previous < index; ++previous)
         {
            if (stationIds[previous] == stationIds[index])
            {
               return Result::DuplicateStation;
            }
         }
      }

      if (closedLoop && FindStation(stationIds.back()) == nullptr)
      {
         return Result::StationNotFound;
      }

      return Result::Ok;
   }

   void Network::RebuildGraph(void)
   {
      _adjacency.clear();
      _adjacency.resize(_stations.size());

      for (Line& line : _lines)
      {
         line.cycleTimeSeconds = LineCycleTimeSeconds(*this, line);
         if (line.stationIds.size() < MinimumLineStations)
         {
            continue;
         }

         const uint32_t segmentCount = LineSegmentCount(line);
         for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
         {
            StationId fromId = InvalidStationId;
            StationId toId = InvalidStationId;
            const Result endpointResult = LineSegmentEndpoints(line, segmentIndex, fromId, toId);
            if (IsErr(endpointResult))
            {
               continue;
            }

            const StationRecord* pFrom = FindStation(fromId);
            const StationRecord* pTo = FindStation(toId);
            if (pFrom == nullptr || pTo == nullptr)
            {
               continue;
            }

            const uint32_t fromIndex = StationVectorIndex(pFrom->id);
            const uint32_t toIndex = StationVectorIndex(pTo->id);
            if (fromIndex == InvalidIndex || toIndex == InvalidIndex)
            {
               continue;
            }

            const float distanceKm = DistanceKm(pFrom->position, pTo->position);
            const float travelTimeSeconds = (distanceKm / TrainSpeedKmPerHour) * SecondsPerHour;
            if (line.loop == LineLoop::Yes)
            {
               AddDirectedEdge(
                  _adjacency,
                  fromIndex,
                  pTo->id,
                  line.id,
                  distanceKm,
                  travelTimeSeconds);
            }
            else
            {
               AddUndirectedEdge(
                  _adjacency,
                  fromIndex,
                  toIndex,
                  pFrom->id,
                  pTo->id,
                  line.id,
                  distanceKm,
                  travelTimeSeconds);
            }
         }
      }

      RebuildLineIndexMap();
      ++_revision;
   }

   Result Network::AddStation(const StationRecord& record)
   {
      if (FindStation(record.id) != nullptr)
      {
         return Result::DuplicateStation;
      }

      _stations.push_back(record);
      RebuildGraph();
      return Result::Ok;
   }

   Result Network::AddLine(const StationIdList& stationIds, uint32_t colorIndex, LineId& lineId)
   {
      const Result validation = ValidateLineStations(stationIds);
      if (IsErr(validation))
      {
         return validation;
      }

      Line line;
      line.id = _nextLineId;
      ++_nextLineId;
      line.stationIds = stationIds;
      line.colorIndex = colorIndex;
      line.loop = LineLoop::No;
      if (IsClosedLoopList(stationIds))
      {
         line.stationIds.pop_back();
         line.loop = LineLoop::Yes;
      }
      _lines.push_back(line);
      ++_createdLineCount;
      lineId = line.id;
      RebuildGraph();
      return Result::Ok;
   }

   Result Network::ExtendLine(LineId lineId, StationId stationId)
   {
      return ExtendLineAt(lineId, LineEnd::Back, stationId);
   }

   Result Network::ExtendLineAt(LineId lineId, LineEnd end, StationId stationId)
   {
      Line* pLine = FindMutableLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }
      if (FindStation(stationId) == nullptr)
      {
         return Result::StationNotFound;
      }
      if (ContainsStationId(pLine->stationIds, stationId))
      {
         if (pLine->loop == LineLoop::No &&
            pLine->stationIds.size() >= MinimumLoopStations)
         {
            if (end == LineEnd::Back && stationId == pLine->stationIds.front())
            {
               pLine->loop = LineLoop::Yes;
               RebuildGraph();
               return Result::Ok;
            }
            if (end == LineEnd::Front && stationId == pLine->stationIds.back())
            {
               pLine->loop = LineLoop::Yes;
               RebuildGraph();
               return Result::Ok;
            }
         }

         return Result::DuplicateStation;
      }

      if (pLine->loop == LineLoop::Yes)
      {
         return Result::DuplicateStation;
      }

      if (end == LineEnd::Back)
      {
         pLine->stationIds.push_back(stationId);
      }
      else
      {
         pLine->stationIds.insert(pLine->stationIds.begin(), stationId);
      }

      RebuildGraph();
      return Result::Ok;
   }

   Result Network::InsertStationOnLine(LineId lineId, uint32_t segmentIndex, StationId stationId)
   {
      Line* pLine = FindMutableLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }
      if (FindStation(stationId) == nullptr)
      {
         return Result::StationNotFound;
      }
      if (ContainsStationId(pLine->stationIds, stationId))
      {
         return Result::DuplicateStation;
      }
      if (segmentIndex >= LineSegmentCount(*pLine))
      {
         return Result::InvalidArgument;
      }

      const uint32_t insertIndex = segmentIndex + 1;
      if (insertIndex >= pLine->stationIds.size())
      {
         pLine->stationIds.push_back(stationId);
      }
      else
      {
         pLine->stationIds.insert(pLine->stationIds.begin() + static_cast<int32_t>(insertIndex), stationId);
      }

      RebuildGraph();
      return Result::Ok;
   }

   Result Network::RemoveLine(LineId lineId)
   {
      for (uint32_t index = 0; index < _lines.size(); ++index)
      {
         if (_lines[index].id != lineId)
         {
            continue;
         }

         _lines[index] = _lines.back();
         _lines.pop_back();
         RebuildGraph();
         return Result::Ok;
      }

      return Result::InvalidArgument;
   }

   bool Network::IsStationOnAnyLine(StationId stationId) const
   {
      for (const Line& line : _lines)
      {
         if (ContainsStationId(line.stationIds, stationId))
         {
            return true;
         }
      }

      return false;
   }

   const StationRecord* Network::FindStation(StationId stationId) const
   {
      const uint32_t index = StationVectorIndex(stationId);
      if (index == InvalidIndex)
      {
         return nullptr;
      }

      return &_stations[index];
   }

   StationRecord* Network::FindMutableStation(StationId stationId)
   {
      const uint32_t index = StationVectorIndex(stationId);
      if (index == InvalidIndex)
      {
         return nullptr;
      }

      return &_stations[index];
   }

   const Line* Network::FindLine(LineId lineId) const
   {
      if (lineId >= _lineVectorIndexById.size())
      {
         return nullptr;
      }

      const uint32_t index = _lineVectorIndexById[lineId];
      if (index == InvalidIndex || index >= _lines.size())
      {
         return nullptr;
      }

      if (_lines[index].id != lineId)
      {
         return nullptr;
      }

      return &_lines[index];
   }

   Line* Network::FindMutableLine(LineId lineId)
   {
      if (lineId >= _lineVectorIndexById.size())
      {
         return nullptr;
      }

      const uint32_t index = _lineVectorIndexById[lineId];
      if (index == InvalidIndex || index >= _lines.size())
      {
         return nullptr;
      }

      if (_lines[index].id != lineId)
      {
         return nullptr;
      }

      return &_lines[index];
   }

   uint32_t Network::StationIndexOnLine(const Line& line, StationId stationId) const
   {
      for (uint32_t index = 0; index < line.stationIds.size(); ++index)
      {
         if (line.stationIds[index] == stationId)
         {
            return index;
         }
      }

      return InvalidIndex;
   }

   const StationRecordList& Network::GetStations(void) const
   {
      return _stations;
   }

   const LineList& Network::GetLines(void) const
   {
      return _lines;
   }

   const AdjacencyList& Network::GetAdjacency(void) const
   {
      return _adjacency;
   }

   uint64_t Network::GetRevision(void) const
   {
      return _revision;
   }

   uint32_t Network::GetCreatedLineCount(void) const
   {
      return _createdLineCount;
   }
} // namespace MiniDb
