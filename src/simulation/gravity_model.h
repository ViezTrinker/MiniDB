/*!
 *\file gravity_model.h
 *\brief Population-and-distance destination choice for passengers.
 */

#ifndef GRAVITY_MODEL_H
#define GRAVITY_MODEL_H

#include "core/result.h"
#include "core/types.h"

namespace MiniDb
{
   struct GravityParameters
   {
      float alpha;
      float gamma;
      float distanceOffsetKm;
   };

   /*!
    *\brief Returns default gravity-model parameters.
    */
   GravityParameters DefaultGravityParameters(void);

   /*!
    *\brief Picks a weighted index using a unit random value.
    *
    *\param[in] weights Non-negative weights.
    *\param[in] randomZeroToOne Value in [0, 1].
    */
   uint32_t PickWeightedIndex(const WeightList& weights, float randomZeroToOne);

   /*!
    *\brief Fills destination weights for one origin using the gravity model.
    *
    * The origin station itself receives weight 0.
    *
    *\param[in] originId Origin station.
    *\param[in] stations Active stations.
    *\param[in] parameters Gravity exponents and distance offset.
    *\param[out] weights One weight per station, same order as stations.
    */
   Result ComputeGravityWeights(
      StationId originId,
      const StationRecordList& stations,
      GravityParameters parameters,
      WeightList& weights);

   /*!
    *\brief Probability that a passenger from origin chooses destination.
    *
    *\param[in] originId Origin station.
    *\param[in] destinationId Candidate destination.
    *\param[in] stations Active stations.
    *\param[in] parameters Gravity exponents and distance offset.
    */
   float GravityProbability(
      StationId originId,
      StationId destinationId,
      const StationRecordList& stations,
      GravityParameters parameters);

   /*!
    *\brief Samples a destination for a passenger using the gravity model.
    *
    *\param[in] originId Origin station.
    *\param[in] stations Active stations.
    *\param[in] parameters Gravity exponents and distance offset.
    *\param[in] randomZeroToOne Value in [0, 1].
    *\param[out] destinationId Chosen destination.
    */
   Result PickGravityDestination(
      StationId originId,
      const StationRecordList& stations,
      GravityParameters parameters,
      float randomZeroToOne,
      StationId& destinationId);

   /*!
    *\brief Samples a destination from precomputed gravity weights.
    *
    *\param[in] weights One weight per station, same order as stations.
    *\param[in] stations Active stations.
    *\param[in] randomZeroToOne Value in [0, 1].
    *\param[out] destinationId Chosen destination.
    */
   Result PickGravityDestinationFromWeights(
      const WeightList& weights,
      const StationRecordList& stations,
      float randomZeroToOne,
      StationId& destinationId);
} // namespace MiniDb

#endif // GRAVITY_MODEL_H
