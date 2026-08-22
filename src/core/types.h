/*!
 *\file types.h
 *\brief Shared identifiers, map points and station records.
 */

#ifndef TYPES_H
#define TYPES_H

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace MiniDb
{
   using StationId = uint32_t;
   using LineId = uint32_t;
   using TrainId = uint32_t;
   using PassengerId = uint32_t;

   inline constexpr StationId InvalidStationId = 0xFFFFFFFFu;
   inline constexpr LineId InvalidLineId = 0xFFFFFFFFu;
   inline constexpr TrainId InvalidTrainId = 0xFFFFFFFFu;
   inline constexpr PassengerId InvalidPassengerId = 0xFFFFFFFFu;
   inline constexpr uint32_t InvalidIndex = 0xFFFFFFFFu;

   struct MapPoint
   {
      float xKm;
      float yKm;
   };

   using MapPolygon = std::vector<MapPoint>;
   using MapPolygonList = std::vector<MapPolygon>;
   using StationIdList = std::vector<StationId>;
   using WeightList = std::vector<float>;
   using PassengerIdList = std::vector<PassengerId>;

   struct StationRecord
   {
      StationId id;
      std::string cityName;
      std::string stationName;
      float latitude;
      float longitude;
      MapPoint position;
      uint32_t population;
   };

   using StationRecordList = std::vector<StationRecord>;

   /*!
    *\brief Euclidean distance between two projected map points in kilometres.
    *
    *\param[in] left First point.
    *\param[in] right Second point.
    */
   inline float DistanceKm(MapPoint left, MapPoint right)
   {
      const float deltaX = left.xKm - right.xKm;
      const float deltaY = left.yKm - right.yKm;
      return std::sqrt(deltaX * deltaX + deltaY * deltaY);
   }

   /*!
    *\brief Shortest distance from a point to a line segment in kilometres.
    *
    *\param[in] point Query location.
    *\param[in] segmentStart Segment start.
    *\param[in] segmentEnd Segment end.
    */
   inline float DistancePointToSegmentKm(MapPoint point, MapPoint segmentStart, MapPoint segmentEnd)
   {
      const float deltaX = segmentEnd.xKm - segmentStart.xKm;
      const float deltaY = segmentEnd.yKm - segmentStart.yKm;
      const float lengthSquared = (deltaX * deltaX) + (deltaY * deltaY);
      if (lengthSquared <= 0.000001f)
      {
         return DistanceKm(point, segmentStart);
      }

      const float toPointX = point.xKm - segmentStart.xKm;
      const float toPointY = point.yKm - segmentStart.yKm;
      float interpolation = ((toPointX * deltaX) + (toPointY * deltaY)) / lengthSquared;
      if (interpolation < 0.0f)
      {
         interpolation = 0.0f;
      }
      if (interpolation > 1.0f)
      {
         interpolation = 1.0f;
      }

      MapPoint closest;
      closest.xKm = segmentStart.xKm + (interpolation * deltaX);
      closest.yKm = segmentStart.yKm + (interpolation * deltaY);
      return DistanceKm(point, closest);
   }

   struct SegmentProjection
   {
      float distanceAlongKm;
      float distanceToSegmentKm;
      float segmentLengthKm;
   };

   /*!
    *\brief Projects a point onto a segment and returns along-track distance.
    *
    *\param[in] point Query location.
    *\param[in] segmentStart Segment start.
    *\param[in] segmentEnd Segment end.
    */
   inline SegmentProjection ProjectPointOnSegment(MapPoint point, MapPoint segmentStart, MapPoint segmentEnd)
   {
      SegmentProjection projection;
      const float deltaX = segmentEnd.xKm - segmentStart.xKm;
      const float deltaY = segmentEnd.yKm - segmentStart.yKm;
      projection.segmentLengthKm = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
      if (projection.segmentLengthKm <= 0.000001f)
      {
         projection.distanceAlongKm = 0.0f;
         projection.distanceToSegmentKm = DistanceKm(point, segmentStart);
         projection.segmentLengthKm = 0.0f;
         return projection;
      }

      const float toPointX = point.xKm - segmentStart.xKm;
      const float toPointY = point.yKm - segmentStart.yKm;
      float interpolation =
         ((toPointX * deltaX) + (toPointY * deltaY)) /
         (projection.segmentLengthKm * projection.segmentLengthKm);
      if (interpolation < 0.0f)
      {
         interpolation = 0.0f;
      }
      if (interpolation > 1.0f)
      {
         interpolation = 1.0f;
      }

      projection.distanceAlongKm = interpolation * projection.segmentLengthKm;
      MapPoint closest;
      closest.xKm = segmentStart.xKm + (interpolation * deltaX);
      closest.yKm = segmentStart.yKm + (interpolation * deltaY);
      projection.distanceToSegmentKm = DistanceKm(point, closest);
      return projection;
   }

   struct LineSegmentHit
   {
      LineId lineId;
      uint32_t segmentIndex;
      float distanceAlongKm;
      float distanceToSegmentKm;
   };

   inline LineSegmentHit MakeInvalidLineSegmentHit(void)
   {
      LineSegmentHit hit;
      hit.lineId = InvalidLineId;
      hit.segmentIndex = InvalidIndex;
      hit.distanceAlongKm = 0.0f;
      hit.distanceToSegmentKm = 0.0f;
      return hit;
   }
} // namespace MiniDb

#endif // TYPES_H
