/*!
 *\file projection.h
 *\brief WGS84 to schematic Germany map projection.
 */

#ifndef PROJECTION_H
#define PROJECTION_H

#include "core/types.h"

namespace MiniDb
{
   /*!
    *\brief Projects geographic coordinates into map kilometres.
    *
    * North is at y = 0. X grows eastwards from the western bound of Germany.
    *
    *\param[in] latitudeDegrees Geographic latitude.
    *\param[in] longitudeDegrees Geographic longitude.
    */
   MapPoint ProjectWgs84(float latitudeDegrees, float longitudeDegrees);

   /*!
    *\brief Width of the Germany map in kilometres.
    */
   float MapWidthKm(void);

   /*!
    *\brief Height of the Germany map in kilometres.
    */
   float MapHeightKm(void);
} // namespace MiniDb

#endif // PROJECTION_H
