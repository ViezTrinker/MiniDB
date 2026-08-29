/*!
 *\file train.cpp
 *\brief Real-time trains shuttling along a line.
 */

#include "simulation/train.h"

#include "core/constants.h"

namespace MiniDb
{
   namespace
   {
      bool IsIndexOnLine(const Line& line, int32_t index)
      {
         if (index < 0)
         {
            return false;
         }

         const auto stationCount = static_cast<int32_t>(line.stationIds.size());
         return index < stationCount;
      }

      int32_t WrappedLineIndex(const Line& line, int32_t index)
      {
         const auto stationCount = static_cast<int32_t>(line.stationIds.size());
         if (stationCount <= 0)
         {
            return 0;
         }

         int32_t wrapped = index % stationCount;
         if (wrapped < 0)
         {
            wrapped += stationCount;
         }

         return wrapped;
      }

      int32_t NextIndexOnLine(const Train& train, const Line& line)
      {
         const int32_t nextIndex = train.fromIndex + train.direction;
         if (line.loop == LineLoop::Yes)
         {
            return WrappedLineIndex(line, nextIndex);
         }

         return nextIndex;
      }
   } // namespace

   StationId CurrentStationOnLine(const Train& train, const Line& line)
   {
      if (!IsIndexOnLine(line, train.fromIndex))
      {
         return InvalidStationId;
      }

      return line.stationIds[static_cast<uint32_t>(train.fromIndex)];
   }

   StationId NextStationOnLine(const Train& train, const Line& line)
   {
      const int32_t nextIndex = NextIndexOnLine(train, line);
      if (!IsIndexOnLine(line, nextIndex))
      {
         return InvalidStationId;
      }

      return line.stationIds[static_cast<uint32_t>(nextIndex)];
   }

   MapPoint TrainMapPosition(const Train& train, const Line& line, const Network& network)
   {
      const StationId fromId = CurrentStationOnLine(train, line);
      const StationRecord* pFrom = network.FindStation(fromId);
      if (pFrom == nullptr)
      {
         MapPoint fallback;
         fallback.xKm = 0.0f;
         fallback.yKm = 0.0f;
         return fallback;
      }

      if (train.motion == TrainMotion::Dwelling)
      {
         return pFrom->position;
      }

      const StationId toId = NextStationOnLine(train, line);
      const StationRecord* pTo = network.FindStation(toId);
      if (pTo == nullptr)
      {
         return pFrom->position;
      }

      const float lengthKm = DistanceKm(pFrom->position, pTo->position);
      float fraction = 0.0f;
      if (lengthKm > 0.0f)
      {
         fraction = train.distanceFromFromStationKm / lengthKm;
      }
      if (fraction < 0.0f)
      {
         fraction = 0.0f;
      }
      if (fraction > 1.0f)
      {
         fraction = 1.0f;
      }

      MapPoint point;
      point.xKm = pFrom->position.xKm + (pTo->position.xKm - pFrom->position.xKm) * fraction;
      point.yKm = pFrom->position.yKm + (pTo->position.yKm - pFrom->position.yKm) * fraction;
      return point;
   }

   TrainStepResult AdvanceTrainMotion(
      Train& train,
      const Line& line,
      const Network& network,
      float deltaSeconds)
   {
      if (train.motion != TrainMotion::Moving)
      {
         return TrainStepResult::StillMoving;
      }
      if (deltaSeconds <= 0.0f)
      {
         return TrainStepResult::StillMoving;
      }

      const StationId fromId = CurrentStationOnLine(train, line);
      const StationId toId = NextStationOnLine(train, line);
      const StationRecord* pFrom = network.FindStation(fromId);
      const StationRecord* pTo = network.FindStation(toId);
      if (pFrom == nullptr || pTo == nullptr)
      {
         return TrainStepResult::Error;
      }

      const float lengthKm = DistanceKm(pFrom->position, pTo->position);
      const float speedKmPerSecond = TrainSpeedKmPerHour / SecondsPerHour;
      train.distanceFromFromStationKm += speedKmPerSecond * deltaSeconds;
      if (lengthKm <= 0.0f || train.distanceFromFromStationKm >= lengthKm)
      {
         train.fromIndex = NextIndexOnLine(train, line);
         train.distanceFromFromStationKm = 0.0f;
         train.motion = TrainMotion::Dwelling;
         train.dwellRemainingSeconds = TrainDwellSeconds;
         train.crowdingDwellApplied = CrowdingDwellApplied::No;

         if (line.loop == LineLoop::No)
         {
            const int32_t nextIndex = train.fromIndex + train.direction;
            if (!IsIndexOnLine(line, nextIndex))
            {
               train.direction = -train.direction;
            }
         }

         return TrainStepResult::ArrivedAtStation;
      }

      return TrainStepResult::StillMoving;
   }
} // namespace MiniDb
