/*!
 *\file gravity_model.cpp
 *\brief Population-and-distance destination choice for passengers.
 */

#include "simulation/gravity_model.h"

#include <cmath>

#include "core/constants.h"

namespace MiniDb
{
   namespace
   {
      const StationRecord* FindStationRecord(const StationRecordList& stations, StationId stationId)
      {
         for (const StationRecord& station : stations)
         {
            if (station.id == stationId)
            {
               return &station;
            }
         }

         return nullptr;
      }

      float GravityWeight(const StationRecord& origin, const StationRecord& destination, GravityParameters parameters)
      {
         if (origin.id == destination.id)
         {
            return 0.0f;
         }

         const float distanceKm = DistanceKm(origin.position, destination.position);
         const float populationTerm = std::pow(static_cast<float>(destination.population), parameters.alpha);
         const float impedance = std::pow(distanceKm + parameters.distanceOffsetKm, parameters.gamma);
         if (impedance <= 0.0f)
         {
            return 0.0f;
         }

         return populationTerm / impedance;
      }
   } // namespace

   GravityParameters DefaultGravityParameters(void)
   {
      GravityParameters parameters;
      parameters.alpha = GravityAlpha;
      parameters.gamma = GravityGamma;
      parameters.distanceOffsetKm = GravityDistanceOffsetKm;
      return parameters;
   }

   uint32_t PickWeightedIndex(const WeightList& weights, float randomZeroToOne)
   {
      if (weights.empty())
      {
         return InvalidIndex;
      }

      float clampedRandom = randomZeroToOne;
      if (clampedRandom < 0.0f)
      {
         clampedRandom = 0.0f;
      }
      if (clampedRandom > 1.0f)
      {
         clampedRandom = 1.0f;
      }

      float weightSum = 0.0f;
      for (float weight : weights)
      {
         if (weight > 0.0f)
         {
            weightSum += weight;
         }
      }

      if (weightSum <= 0.0f)
      {
         return InvalidIndex;
      }

      const float threshold = clampedRandom * weightSum;
      float cumulative = 0.0f;
      uint32_t lastPositiveIndex = InvalidIndex;
      for (uint32_t index = 0; index < weights.size(); ++index)
      {
         if (weights[index] <= 0.0f)
         {
            continue;
         }

         lastPositiveIndex = index;
         cumulative += weights[index];
         if (threshold <= cumulative)
         {
            return index;
         }
      }

      return lastPositiveIndex;
   }

   Result ComputeGravityWeights(
      StationId originId,
      const StationRecordList& stations,
      GravityParameters parameters,
      WeightList& weights)
   {
      const StationRecord* pOrigin = FindStationRecord(stations, originId);
      if (pOrigin == nullptr)
      {
         return Result::StationNotFound;
      }
      if (stations.size() < 2)
      {
         return Result::InvalidArgument;
      }

      weights.clear();
      weights.reserve(stations.size());
      float weightSum = 0.0f;
      for (const StationRecord& station : stations)
      {
         const float weight = GravityWeight(*pOrigin, station, parameters);
         weights.push_back(weight);
         weightSum += weight;
      }

      if (weightSum <= 0.0f)
      {
         return Result::Error;
      }

      return Result::Ok;
   }

   float GravityProbability(
      StationId originId,
      StationId destinationId,
      const StationRecordList& stations,
      GravityParameters parameters)
   {
      WeightList weights;
      const Result result = ComputeGravityWeights(originId, stations, parameters, weights);
      if (IsErr(result))
      {
         return 0.0f;
      }

      float weightSum = 0.0f;
      float destinationWeight = 0.0f;
      for (uint32_t index = 0; index < stations.size(); ++index)
      {
         weightSum += weights[index];
         if (stations[index].id == destinationId)
         {
            destinationWeight = weights[index];
         }
      }

      if (weightSum <= 0.0f)
      {
         return 0.0f;
      }

      return destinationWeight / weightSum;
   }

   Result PickGravityDestination(
      StationId originId,
      const StationRecordList& stations,
      GravityParameters parameters,
      float randomZeroToOne,
      StationId& destinationId)
   {
      WeightList weights;
      const Result result = ComputeGravityWeights(originId, stations, parameters, weights);
      if (IsErr(result))
      {
         return result;
      }

      const uint32_t index = PickWeightedIndex(weights, randomZeroToOne);
      if (index == InvalidIndex || index >= stations.size())
      {
         return Result::Error;
      }

      destinationId = stations[index].id;
      return Result::Ok;
   }

   Result PickGravityDestinationFromWeights(
      const WeightList& weights,
      const StationRecordList& stations,
      float randomZeroToOne,
      StationId& destinationId)
   {
      if (weights.size() != stations.size())
      {
         return Result::InvalidArgument;
      }
      if (stations.size() < 2)
      {
         return Result::InvalidArgument;
      }

      const uint32_t index = PickWeightedIndex(weights, randomZeroToOne);
      if (index == InvalidIndex || index >= stations.size())
      {
         return Result::Error;
      }

      destinationId = stations[index].id;
      return Result::Ok;
   }
} // namespace MiniDb
