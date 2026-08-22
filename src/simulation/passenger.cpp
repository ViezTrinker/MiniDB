/*!
 *\file passenger.cpp
 *\brief Passenger counting helpers.
 */

#include "simulation/passenger.h"

namespace MiniDb
{
   uint32_t CountWaitingPassengers(const PassengerList& passengers)
   {
      uint32_t count = 0;
      for (const Passenger& passenger : passengers)
      {
         if (passenger.state == PassengerState::Waiting)
         {
            ++count;
         }
      }

      return count;
   }

   uint32_t CountOnboardPassengers(const PassengerList& passengers)
   {
      uint32_t count = 0;
      for (const Passenger& passenger : passengers)
      {
         if (passenger.state == PassengerState::Onboard)
         {
            ++count;
         }
      }

      return count;
   }
} // namespace MiniDb
