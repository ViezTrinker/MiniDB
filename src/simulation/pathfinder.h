/*!
 *\file pathfinder.h
 *\brief Shortest passenger routes on the player-built network.
 */

#ifndef PATHFINDER_H
#define PATHFINDER_H

#include "core/result.h"
#include "core/types.h"
#include "simulation/network.h"

namespace MiniDb
{
   struct RouteHop
   {
      LineId lineId;
      StationId fromStationId;
      StationId toStationId;
   };

   using Route = std::vector<RouteHop>;

   struct LineWait
   {
      LineId lineId;
      float waitSeconds;
   };

   using LineWaitList = std::vector<LineWait>;

   /*!
    *\brief Round-trip time of a shuttle, or one circuit of a loop.
    *
    *\param[in] network Station positions and the line list.
    *\param[in] line Line whose cycle time is computed.
    */
   float LineCycleTimeSeconds(const Network& network, const Line& line);

   /*!
    *\brief Expected platform wait, half the headway, or a full cycle if no trains.
    *
    *\param[in] cycleTimeSeconds Line cycle time.
    *\param[in] trainCount Trains currently on the line.
    */
   float ExpectedLineWaitSeconds(float cycleTimeSeconds, uint32_t trainCount);

   /*!
    *\brief Finds a fastest route using travel time, dwell and line waits.
    *
    *\param[in] network Current station and line graph.
    *\param[in] originId Start station.
    *\param[in] destinationId Target station.
    *\param[in] lineWaits Expected wait per line. Missing lines use zero wait.
    *\param[out] route Ordered adjacent hops. Empty if origin equals destination.
    */
   Result FindRoute(
      const Network& network,
      StationId originId,
      StationId destinationId,
      const LineWaitList& lineWaits,
      Route& route);
} // namespace MiniDb

#endif // PATHFINDER_H
