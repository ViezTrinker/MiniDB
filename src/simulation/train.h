/*!
 *\file train.h
 *\brief Real-time trains shuttling along a line.
 */

#ifndef TRAIN_H
#define TRAIN_H

#include "core/result.h"
#include "core/types.h"
#include "simulation/network.h"

namespace MiniDb
{
   enum class TrainMotion : uint8_t
   {
      Moving,
      Dwelling
   };

   enum class TrainStepResult : int8_t
   {
      Error = -1,
      StillMoving = 0,
      ArrivedAtStation = 1
   };

   struct Train
   {
      TrainId id;
      LineId lineId;
      int32_t fromIndex;
      int32_t direction;
      float distanceFromFromStationKm;
      float dwellRemainingSeconds;
      TrainMotion motion;
      PassengerIdList passengerIds;
   };

   using TrainList = std::vector<Train>;

   /*!
    *\brief Returns the next station id in the train's current direction.
    *
    *\param[in] train Train to inspect.
    *\param[in] line Line the train runs on.
    */
   StationId NextStationOnLine(const Train& train, const Line& line);

   /*!
    *\brief Returns the station the train is currently at or departing from.
    *
    *\param[in] train Train to inspect.
    *\param[in] line Line the train runs on.
    */
   StationId CurrentStationOnLine(const Train& train, const Line& line);

   /*!
    *\brief Interpolated map position of a train.
    *
    *\param[in] train Train to locate.
    *\param[in] line Line the train runs on.
    *\param[in] network Station positions.
    */
   MapPoint TrainMapPosition(const Train& train, const Line& line, const Network& network);

   /*!
    *\brief Advances a moving train along its current segment.
    *
    *\param[in,out] train Train to update.
    *\param[in] line Line the train runs on.
    *\param[in] network Station positions.
    *\param[in] deltaSeconds Elapsed simulation time.
    */
   TrainStepResult AdvanceTrainMotion(
      Train& train,
      const Line& line,
      const Network& network,
      float deltaSeconds);
} // namespace MiniDb

#endif // TRAIN_H
