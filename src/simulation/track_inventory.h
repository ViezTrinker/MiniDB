/*!
 *\file track_inventory.h
 *\brief Canonical track pairs and unique segment collection.
 */

#ifndef TRACK_INVENTORY_H
#define TRACK_INVENTORY_H

#include <cstdint>
#include <set>
#include <vector>

#include "core/types.h"
#include "simulation/network.h"

namespace MiniDb
{
   /*!
    *\brief Canonical undirected station pair key.
    *
    *\param[in] stationA First endpoint.
    *\param[in] stationB Second endpoint.
    */
   uint64_t CanonicalStationPairKey(StationId stationA, StationId stationB);

   struct TrackSegmentRecord
   {
      StationId stationA = InvalidStationId;
      StationId stationB = InvalidStationId;
      float distanceKm = 0.0f;
   };

   using TrackSegmentRecordList = std::vector<TrackSegmentRecord>;
   using TrackPairKeySet = std::set<uint64_t>;

   /*!
    *\brief Collects unique undirected segments from all finished lines.
    *
    *\param[in] network Network to scan.
    *\param[out] segments One entry per unique station pair.
    */
   void CollectUniqueTrackSegments(const Network& network, TrackSegmentRecordList& segments);

   /*!
    *\brief Collects consecutive segments along an ordered station list.
    *
    *\param[in] network Network used to resolve station positions.
    *\param[in] stationIds Ordered stations (open polyline, no loop closer).
    *\param[out] segments Segment records for each hop.
    */
   void CollectSegmentsAlongStationIds(
      const Network& network,
      const StationIdList& stationIds,
      TrackSegmentRecordList& segments);

   /*!
    *\brief Collects segments for a line definition, including loop closure.
    *
    *\param[in] network Network used to resolve station positions.
    *\param[in] stationIds Drafted or confirmed station order.
    *\param[out] segments Unique hop list for the line.
    */
   void CollectSegmentsForLineDefinition(
      const Network& network,
      const StationIdList& stationIds,
      TrackSegmentRecordList& segments);

   /*!
    *\brief Sums kilometres of segments whose pair keys are not yet built.
    *
    *\param[in] segments Candidate segments.
    *\param[in] builtPairKeys Already constructed pairs.
    *\param[out] newSegments Segments that would be charged.
    *\return Total kilometres of new segments.
    */
   float SumNewSegmentKilometers(
      const TrackSegmentRecordList& segments,
      const TrackPairKeySet& builtPairKeys,
      TrackSegmentRecordList& newSegments);

   /*!
    *\brief Total kilometres across all unique network segments.
    *
    *\param[in] network Network to measure.
    */
   float TotalUniqueTrackKilometers(const Network& network);
} // namespace MiniDb

#endif // TRACK_INVENTORY_H
