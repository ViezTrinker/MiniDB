/*!
 *\file network.h
 *\brief Stations, lines and the travel graph.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "core/constants.h"
#include "core/result.h"
#include "core/types.h"

namespace MiniDb
{
   struct NetworkEdge
   {
      StationId toStationId;
      LineId lineId;
      float distanceKm;
      float travelTimeSeconds;
   };

   using NetworkEdgeList = std::vector<NetworkEdge>;
   using AdjacencyList = std::vector<NetworkEdgeList>;

   enum class LineLoop : bool
   {
      No = false,
      Yes = true
   };

   struct Line
   {
      LineId id;
      StationIdList stationIds;
      uint32_t colorIndex;
      LineLoop loop = LineLoop::No;
   };

   using LineList = std::vector<Line>;

   /*!
    *\brief Number of drawable segments, including the closer of a loop.
    *
    *\param[in] line Line to inspect.
    */
   inline uint32_t LineSegmentCount(const Line& line)
   {
      if (line.stationIds.size() < MinimumLineStations)
      {
         return 0;
      }
      if (line.loop == LineLoop::Yes)
      {
         return static_cast<uint32_t>(line.stationIds.size());
      }

      return static_cast<uint32_t>(line.stationIds.size() - 1);
   }

   /*!
    *\brief Station ids at both ends of a line segment.
    *
    *\param[in] line Line to inspect.
    *\param[in] segmentIndex Segment starting at this station index.
    *\param[out] fromId Segment start.
    *\param[out] toId Segment end.
    */
   inline Result LineSegmentEndpoints(
      const Line& line,
      uint32_t segmentIndex,
      StationId& fromId,
      StationId& toId)
   {
      if (segmentIndex >= LineSegmentCount(line))
      {
         fromId = InvalidStationId;
         toId = InvalidStationId;
         return Result::InvalidArgument;
      }

      fromId = line.stationIds[segmentIndex];
      if (segmentIndex + 1 < line.stationIds.size())
      {
         toId = line.stationIds[segmentIndex + 1];
      }
      else
      {
         toId = line.stationIds.front();
      }

      return Result::Ok;
   }

   class Network
   {
   public:
      /*!
       *\brief Removes all stations, lines and graph edges.
       */
      void Clear(void);

      /*!
       *\brief Adds an active station to the network.
       *
       *\param[in] record Station to add.
       */
      Result AddStation(const StationRecord& record);

      /*!
       *\brief Creates a line through the given stations.
       *
       *\param[in] stationIds Ordered stations on the line.
       *\param[in] colorIndex Palette index.
       *\param[out] lineId Identifier of the new line.
       */
      Result AddLine(const StationIdList& stationIds, uint32_t colorIndex, LineId& lineId);

      /*!
       *\brief Appends a station to the end of an existing line.
       *
       *\param[in] lineId Line to extend.
       *\param[in] stationId Station to append.
       */
      Result ExtendLine(LineId lineId, StationId stationId);

      /*!
       *\brief Inserts a station into a line between the two ends of a segment.
       *
       *\param[in] lineId Line to change.
       *\param[in] segmentIndex Segment that receives the station.
       *\param[in] stationId Station to insert.
       */
      Result InsertStationOnLine(LineId lineId, uint32_t segmentIndex, StationId stationId);

      /*!
       *\brief Removes a finished line from the network.
       *
       *\param[in] lineId Line to delete.
       */
      Result RemoveLine(LineId lineId);

      /*!
       *\brief Returns true when the station already belongs to any line.
       *
       *\param[in] stationId Station to inspect.
       */
      bool IsStationOnAnyLine(StationId stationId) const;

      /*!
       *\brief Finds a station by id.
       *
       *\param[in] stationId Station identifier.
       */
      const StationRecord* FindStation(StationId stationId) const;

      /*!
       *\brief Finds a mutable station by id.
       *
       *\param[in] stationId Station identifier.
       */
      StationRecord* FindMutableStation(StationId stationId);

      /*!
       *\brief Finds a line by id.
       *
       *\param[in] lineId Line identifier.
       */
      const Line* FindLine(LineId lineId) const;

      /*!
       *\brief Finds a mutable line by id.
       *
       *\param[in] lineId Line identifier.
       */
      Line* FindMutableLine(LineId lineId);

      /*!
       *\brief Returns the index of a station on a line, or InvalidIndex.
       *
       *\param[in] line Line to search.
       *\param[in] stationId Station identifier.
       */
      uint32_t StationIndexOnLine(const Line& line, StationId stationId) const;

      /*!
       *\brief Active stations in spawn order.
       */
      const StationRecordList& GetStations(void) const;

      /*!
       *\brief All finished lines.
       */
      const LineList& GetLines(void) const;

      /*!
       *\brief Adjacency lists aligned with GetStations().
       */
      const AdjacencyList& GetAdjacency(void) const;

      /*!
       *\brief Increments whenever lines change.
       */
      uint64_t GetRevision(void) const;

      /*!
       *\brief Number of created lines, used for color cycling.
       */
      uint32_t GetCreatedLineCount(void) const;

   private:
      void RebuildGraph(void);
      uint32_t StationVectorIndex(StationId stationId) const;
      Result ValidateLineStations(const StationIdList& stationIds) const;

      StationRecordList _stations;
      LineList _lines;
      AdjacencyList _adjacency;
      LineId _nextLineId = 1;
      uint64_t _revision = 0;
      uint32_t _createdLineCount = 0;
   };
} // namespace MiniDb

#endif // NETWORK_H
