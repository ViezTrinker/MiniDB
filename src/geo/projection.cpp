/*!
 *\file projection.cpp
 *\brief WGS84 to schematic Germany map projection.
 */

#include "geo/projection.h"

#include <cmath>

#include "core/constants.h"

namespace MiniDb
{
   namespace
   {
      float ReferenceCosine(void)
      {
         const float radians = ProjectionReferenceLatitudeDegrees * Pi / 180.0f;
         return std::cos(radians);
      }
   } // namespace

   MapPoint ProjectWgs84(float latitudeDegrees, float longitudeDegrees)
   {
      MapPoint point;
      point.xKm = (longitudeDegrees - GermanyLonMin) * KilometersPerDegreeLatitude * ReferenceCosine();
      point.yKm = (GermanyLatMax - latitudeDegrees) * KilometersPerDegreeLatitude;
      return point;
   }

   float MapWidthKm(void)
   {
      return (GermanyLonMax - GermanyLonMin) * KilometersPerDegreeLatitude * ReferenceCosine();
   }

   float MapHeightKm(void)
   {
      return (GermanyLatMax - GermanyLatMin) * KilometersPerDegreeLatitude;
   }
} // namespace MiniDb
