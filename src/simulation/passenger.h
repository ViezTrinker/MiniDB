/*!
 *\file passenger.h
 *\brief Passengers waiting on platforms or riding trains.
 */

#ifndef PASSENGER_H
#define PASSENGER_H

#include "core/types.h"
#include "simulation/pathfinder.h"

namespace MiniDb
{
   enum class PassengerState : uint8_t
   {
      Waiting,
      Onboard
   };

   struct Passenger
   {
      PassengerId id;
      StationId originId;
      StationId destinationId;
      StationId currentStationId;
      TrainId trainId;
      PassengerState state;
      Route route;
      uint32_t routeHopIndex;
      uint64_t routeRevision;
      float platformArrivalTimeSeconds;
   };

   using PassengerList = std::vector<Passenger>;

   struct DestinationDemand
   {
      StationId destinationId;
      uint32_t waitingCount;
   };

   using DestinationDemandList = std::vector<DestinationDemand>;

   struct OnboardDemand
   {
      StationId destinationId;
      StationId transferStationId;
      uint32_t passengerCount;
   };

   using OnboardDemandList = std::vector<OnboardDemand>;

   /*!
    *\brief Counts passengers currently waiting at stations.
    *
    *\param[in] passengers Passenger pool.
    */
   uint32_t CountWaitingPassengers(const PassengerList& passengers);

   /*!
    *\brief Counts passengers currently riding a train.
    *
    *\param[in] passengers Passenger pool.
    */
   uint32_t CountOnboardPassengers(const PassengerList& passengers);
} // namespace MiniDb

#endif // PASSENGER_H
