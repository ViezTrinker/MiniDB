/*!
 *\file track_inventory.cpp
 *\brief Canonical track pairs and unique segment collection.
 */

#include "simulation/track_inventory.h"

namespace MiniDb
{
   uint64_t CanonicalStationPairKey(StationId stationA, StationId stationB)
   {
      const StationId lowId = stationA < stationB ? stationA : stationB;
      const StationId highId = stationA < stationB ? stationB : stationA;
      return (static_cast<uint64_t>(lowId) << 32u) | static_cast<uint64_t>(highId);
   }

   void CollectUniqueTrackSegments(const Network& network, TrackSegmentRecordList& segments)
   {
      segments.clear();
      TrackPairKeySet seenKeys;
      const LineList& lines = network.GetLines();
      for (const Line& line : lines)
      {
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

            const uint64_t pairKey = CanonicalStationPairKey(fromId, toId);
            if (seenKeys.find(pairKey) != seenKeys.end())
            {
               continue;
            }

            const StationRecord* pFrom = network.FindStation(fromId);
            const StationRecord* pTo = network.FindStation(toId);
            if (pFrom == nullptr || pTo == nullptr)
            {
               continue;
            }

            TrackSegmentRecord record;
            record.stationA = fromId;
            record.stationB = toId;
            record.distanceKm = DistanceKm(pFrom->position, pTo->position);
            segments.push_back(record);
            seenKeys.insert(pairKey);
         }
      }
   }

   void CollectSegmentsAlongStationIds(
      const Network& network,
      const StationIdList& stationIds,
      TrackSegmentRecordList& segments)
   {
      segments.clear();
      if (stationIds.size() < 2)
      {
         return;
      }

      for (uint32_t index = 1; index < stationIds.size(); ++index)
      {
         const StationId fromId = stationIds[index - 1];
         const StationId toId = stationIds[index];
         const StationRecord* pFrom = network.FindStation(fromId);
         const StationRecord* pTo = network.FindStation(toId);
         if (pFrom == nullptr || pTo == nullptr)
         {
            continue;
         }

         TrackSegmentRecord record;
         record.stationA = fromId;
         record.stationB = toId;
         record.distanceKm = DistanceKm(pFrom->position, pTo->position);
         segments.push_back(record);
      }
   }

   float SumNewSegmentKilometers(
      const TrackSegmentRecordList& segments,
      const TrackPairKeySet& builtPairKeys,
      TrackSegmentRecordList& newSegments)
   {
      newSegments.clear();
      float totalKm = 0.0f;
      for (const TrackSegmentRecord& segment : segments)
      {
         const uint64_t pairKey = CanonicalStationPairKey(segment.stationA, segment.stationB);
         if (builtPairKeys.find(pairKey) != builtPairKeys.end())
         {
            continue;
         }

         newSegments.push_back(segment);
         totalKm += segment.distanceKm;
      }

      return totalKm;
   }

   void CollectSegmentsForLineDefinition(
      const Network& network,
      const StationIdList& stationIds,
      TrackSegmentRecordList& segments)
   {
      segments.clear();
      if (stationIds.size() < MinimumLineStations)
      {
         return;
      }

      Line tempLine;
      tempLine.stationIds = stationIds;
      tempLine.loop = LineLoop::No;
      if (stationIds.size() >= MinimumLoopStations &&
         stationIds.front() == stationIds.back())
      {
         tempLine.stationIds.pop_back();
         tempLine.loop = LineLoop::Yes;
      }

      const uint32_t segmentCount = LineSegmentCount(tempLine);
      for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
      {
         StationId fromId = InvalidStationId;
         StationId toId = InvalidStationId;
         const Result endpointResult = LineSegmentEndpoints(tempLine, segmentIndex, fromId, toId);
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

         TrackSegmentRecord record;
         record.stationA = fromId;
         record.stationB = toId;
         record.distanceKm = DistanceKm(pFrom->position, pTo->position);
         segments.push_back(record);
      }
   }

   float TotalUniqueTrackKilometers(const Network& network)
   {
      TrackSegmentRecordList segments;
      CollectUniqueTrackSegments(network, segments);
      float totalKm = 0.0f;
      for (const TrackSegmentRecord& segment : segments)
      {
         totalKm += segment.distanceKm;
      }

      return totalKm;
   }
} // namespace MiniDb
