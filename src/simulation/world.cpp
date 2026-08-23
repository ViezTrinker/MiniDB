/*!
 *\file world.cpp
 *\brief Real-time simulation facade for stations, lines, trains and passengers.
 */

#include "simulation/world.h"

#include <algorithm>

#include "core/constants.h"
#include "data/station_catalog.h"
#include "simulation/gravity_model.h"
#include "simulation/pathfinder.h"

namespace MiniDb
{
   namespace
   {
      bool CompareDemandDescending(const DestinationDemand& left, const DestinationDemand& right)
      {
         return left.waitingCount > right.waitingCount;
      }

      bool CompareOnboardDemandDescending(const OnboardDemand& left, const OnboardDemand& right)
      {
         return left.passengerCount > right.passengerCount;
      }

      bool CompareTrainOccupancyById(const TrainOccupancy& left, const TrainOccupancy& right)
      {
         return left.trainId < right.trainId;
      }

      bool CompareCrowdingDescending(const StationCrowding& left, const StationCrowding& right)
      {
         if (left.waitingCount != right.waitingCount)
         {
            return left.waitingCount > right.waitingCount;
         }

         return left.stationId < right.stationId;
      }

      void AddDestinationCount(DestinationDemandList& demand, StationId destinationId)
      {
         for (DestinationDemand& entry : demand)
         {
            if (entry.destinationId != destinationId)
            {
               continue;
            }

            ++entry.waitingCount;
            return;
         }

         DestinationDemand entry;
         entry.destinationId = destinationId;
         entry.waitingCount = 1;
         demand.push_back(entry);
      }

      bool PassengerIsOnboardLine(const Passenger& passenger, LineId lineId, const TrainList& trains)
      {
         if (passenger.state != PassengerState::Onboard)
         {
            return false;
         }

         for (const Train& train : trains)
         {
            if (train.id != passenger.trainId)
            {
               continue;
            }

            return train.lineId == lineId;
         }

         return false;
      }

      bool IsEarlierPlatformArrival(const Passenger& left, const Passenger& right)
      {
         if (left.platformArrivalTimeSeconds != right.platformArrivalTimeSeconds)
         {
            return left.platformArrivalTimeSeconds < right.platformArrivalTimeSeconds;
         }

         return left.id < right.id;
      }

      bool PassengerWantsNextStation(
         const Passenger& passenger,
         StationId stationId,
         StationId nextStationId)
      {
         if (passenger.state != PassengerState::Waiting)
         {
            return false;
         }
         if (passenger.currentStationId != stationId)
         {
            return false;
         }
         if (passenger.routeHopIndex >= passenger.route.size())
         {
            return false;
         }

         const RouteHop& hop = passenger.route[passenger.routeHopIndex];
         if (hop.fromStationId != stationId)
         {
            return false;
         }

         return hop.toStationId == nextStationId;
      }

      StationId FirstTransferStation(const Passenger& passenger)
      {
         if (passenger.route.empty() || passenger.routeHopIndex >= passenger.route.size())
         {
            return InvalidStationId;
         }

         for (uint32_t hopIndex = passenger.routeHopIndex; hopIndex + 1 < passenger.route.size(); ++hopIndex)
         {
            if (passenger.route[hopIndex].lineId != passenger.route[hopIndex + 1].lineId)
            {
               return passenger.route[hopIndex].toStationId;
            }
         }

         return InvalidStationId;
      }
   } // namespace

   World::World(uint32_t randomSeed) :
      _maxStationCount(DefaultMaxStationCount),
      _trainCapacity(TrainCapacity),
      _timeUntilNextStationSeconds(StationSpawnIntervalSeconds),
      _generator(randomSeed),
      _unitDistribution(0.0f, 1.0f)
   {
   }

   void World::ResetSimulation(void)
   {
      _network.Clear();
      _trains.clear();
      _passengers.clear();
      _nextCatalogIndex = 0;
      _nextTrainId = 1;
      _nextPassengerId = 1;
      _arrivedPassengerCount = 0;
      _pathRevision = 0;
      _simulationTimeSeconds = 0.0f;
      _timeUntilNextStationSeconds = StationSpawnIntervalSeconds;
      _passengerSpawnAccumulator = 0.0f;
   }

   void World::SetMaxStationCount(uint32_t maxStationCount)
   {
      if (maxStationCount == 0)
      {
         _maxStationCount = 1;
         return;
      }

      _maxStationCount = maxStationCount;
   }

   Result World::LoadCatalogFromFile(std::string_view filePath)
   {
      const Result result = LoadStationCatalogFromFile(filePath, _catalog);
      if (IsErr(result))
      {
         return result;
      }

      _nextCatalogIndex = 0;
      return Result::Ok;
   }

   Result World::LoadCatalogFromString(std::string_view jsonText)
   {
      const Result result = LoadStationCatalogFromString(jsonText, _catalog);
      if (IsErr(result))
      {
         return result;
      }

      _nextCatalogIndex = 0;
      return Result::Ok;
   }

   Result World::SpawnInitialStations(void)
   {
      if (_catalog.empty())
      {
         return Result::Error;
      }

      uint32_t spawned = 0;
      uint32_t targetCount = InitialStationCount;
      const uint32_t stationCap = GetStationCap();
      if (targetCount > stationCap)
      {
         targetCount = stationCap;
      }

      while (spawned < targetCount)
      {
         const Result result = SpawnNextStation();
         if (IsErr(result))
         {
            if (spawned == 0)
            {
               return result;
            }
            break;
         }
         ++spawned;
      }

      _timeUntilNextStationSeconds = StationSpawnIntervalSeconds;
      return Result::Ok;
   }

   Result World::SpawnNextStation(void)
   {
      if (_nextCatalogIndex >= _catalog.size())
      {
         return Result::Error;
      }

      const auto activeStationCount = static_cast<uint32_t>(_network.GetStations().size());
      if (activeStationCount >= GetStationCap())
      {
         return Result::Error;
      }

      const Result result = _network.AddStation(_catalog[_nextCatalogIndex]);
      if (IsErr(result))
      {
         return result;
      }

      ++_nextCatalogIndex;
      return Result::Ok;
   }

   void World::SetPassengerAutoSpawn(PassengerAutoSpawn autoSpawn)
   {
      _passengerAutoSpawn = autoSpawn;
   }

   void World::SetTrainCapacity(uint32_t capacity)
   {
      if (capacity == 0)
      {
         return;
      }

      _trainCapacity = capacity;
   }

   void World::Tick(float deltaSeconds)
   {
      float clampedDelta = deltaSeconds;
      if (clampedDelta < 0.0f)
      {
         clampedDelta = 0.0f;
      }

      _simulationTimeSeconds += clampedDelta;
      MaybeSpawnStations(clampedDelta);
      MaybeSpawnPassengers(clampedDelta);
      UpdateTrains(clampedDelta);
   }

   Result World::AddLine(const StationIdList& stationIds, LineId& lineId)
   {
      const uint32_t colorIndex = _network.GetCreatedLineCount() % LineColorCount;
      const Result result = _network.AddLine(stationIds, colorIndex, lineId);
      if (IsErr(result))
      {
         return result;
      }

      return AddTrainToLine(lineId);
   }

   Result World::ExtendLine(LineId lineId, StationId stationId)
   {
      const Result result = _network.ExtendLine(lineId, stationId);
      if (IsErr(result))
      {
         return result;
      }

      NotePathChanged();
      return Result::Ok;
   }

   void World::UnloadTrainPassengers(Train& train)
   {
      const Line* pLine = _network.FindLine(train.lineId);
      StationId dropStationId = InvalidStationId;
      if (pLine != nullptr)
      {
         dropStationId = CurrentStationOnLine(train, *pLine);
      }

      const PassengerIdList onboardCopy = train.passengerIds;
      for (PassengerId passengerId : onboardCopy)
      {
         Passenger* pPassenger = FindMutablePassenger(passengerId);
         if (pPassenger == nullptr)
         {
            continue;
         }

         pPassenger->state = PassengerState::Waiting;
         pPassenger->trainId = InvalidTrainId;
         pPassenger->platformArrivalTimeSeconds = _simulationTimeSeconds;
         if (dropStationId != InvalidStationId)
         {
            pPassenger->currentStationId = dropStationId;
         }
      }

      train.passengerIds.clear();
   }

   void World::RemoveTrainsOnLine(LineId lineId)
   {
      uint32_t index = 0;
      while (index < _trains.size())
      {
         if (_trains[index].lineId != lineId)
         {
            ++index;
            continue;
         }

         UnloadTrainPassengers(_trains[index]);
         _trains[index] = _trains.back();
         _trains.pop_back();
      }
   }

   void World::CollectLineWaits(LineWaitList& lineWaits) const
   {
      lineWaits.clear();
      for (const Line& line : _network.GetLines())
      {
         uint32_t trainCount = 0;
         for (const Train& train : _trains)
         {
            if (train.lineId == line.id)
            {
               ++trainCount;
            }
         }

         LineWait lineWait;
         lineWait.lineId = line.id;
         lineWait.waitSeconds = ExpectedLineWaitSeconds(
            LineCycleTimeSeconds(_network, line),
            trainCount);
         lineWaits.push_back(lineWait);
      }
   }

   void World::NotePathChanged(void)
   {
      ++_pathRevision;
      RepathAllPassengers();
   }

   void World::RepathAllPassengers(void)
   {
      LineWaitList lineWaits;
      CollectLineWaits(lineWaits);
      for (Passenger& passenger : _passengers)
      {
         passenger.routeRevision = 0;
         MaybeRepath(passenger, lineWaits);
      }
   }

   Result World::RemoveLine(LineId lineId)
   {
      if (_network.FindLine(lineId) == nullptr)
      {
         return Result::InvalidArgument;
      }

      RemoveTrainsOnLine(lineId);
      const Result result = _network.RemoveLine(lineId);
      if (IsErr(result))
      {
         return result;
      }

      NotePathChanged();
      return Result::Ok;
   }

   Result World::InsertStationOnLine(LineId lineId, uint32_t segmentIndex, StationId stationId)
   {
      const Result result = _network.InsertStationOnLine(lineId, segmentIndex, stationId);
      if (IsErr(result))
      {
         return result;
      }

      AdjustTrainsAfterInsert(lineId, segmentIndex + 1);
      NotePathChanged();
      return Result::Ok;
   }

   Result World::AddTrainToLine(LineId lineId)
   {
      const Line* pLine = _network.FindLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }
      if (pLine->stationIds.size() < MinimumLineStations)
      {
         return Result::LineTooShort;
      }

      Train train;
      train.id = _nextTrainId;
      ++_nextTrainId;
      train.lineId = lineId;
      train.fromIndex = 0;
      train.direction = 1;
      train.distanceFromFromStationKm = 0.0f;
      train.dwellRemainingSeconds = TrainDwellSeconds;
      train.motion = TrainMotion::Dwelling;
      _trains.push_back(train);
      NotePathChanged();
      return Result::Ok;
   }

   Result World::AddTrainToLineAt(LineId lineId, MapPoint dropPoint)
   {
      const Line* pLine = _network.FindLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }
      if (LineSegmentCount(*pLine) == 0)
      {
         return Result::LineTooShort;
      }

      const LineSegmentHit hit = FindNearestSegmentOnLine(lineId, dropPoint);
      if (hit.lineId == InvalidLineId)
      {
         return Result::Error;
      }

      Train train;
      train.id = _nextTrainId;
      ++_nextTrainId;
      train.lineId = lineId;
      train.fromIndex = static_cast<int32_t>(hit.segmentIndex);
      train.direction = 1;
      train.distanceFromFromStationKm = hit.distanceAlongKm;
      train.dwellRemainingSeconds = TrainDwellSeconds;
      train.motion = TrainMotion::Moving;
      if (hit.distanceAlongKm <= 0.01f)
      {
         train.distanceFromFromStationKm = 0.0f;
         train.motion = TrainMotion::Dwelling;
      }
      _trains.push_back(train);
      ClampTrainToCurrentSegment(_trains.back());
      NotePathChanged();
      return Result::Ok;
   }

   Result World::SpawnPassenger(StationId originId, StationId destinationId)
   {
      if (originId == destinationId)
      {
         return Result::InvalidArgument;
      }
      if (_network.FindStation(originId) == nullptr || _network.FindStation(destinationId) == nullptr)
      {
         return Result::StationNotFound;
      }

      Passenger passenger;
      passenger.id = _nextPassengerId;
      ++_nextPassengerId;
      passenger.originId = originId;
      passenger.destinationId = destinationId;
      passenger.currentStationId = originId;
      passenger.trainId = InvalidTrainId;
      passenger.state = PassengerState::Waiting;
      passenger.routeHopIndex = 0;
      passenger.routeRevision = 0;
      passenger.platformArrivalTimeSeconds = _simulationTimeSeconds;
      LineWaitList lineWaits;
      CollectLineWaits(lineWaits);
      MaybeRepath(passenger, lineWaits);
      _passengers.push_back(passenger);
      return Result::Ok;
   }

   StationId World::HitTestStation(MapPoint point, float radiusKm) const
   {
      StationId bestId = InvalidStationId;
      float bestDistance = radiusKm;
      bool found = false;
      for (const StationRecord& station : _network.GetStations())
      {
         const float distance = DistanceKm(point, station.position);
         if (distance > radiusKm)
         {
            continue;
         }
         if (!found || distance < bestDistance)
         {
            bestDistance = distance;
            bestId = station.id;
            found = true;
         }
      }

      return bestId;
   }

   TrainId World::HitTestTrain(MapPoint point, float radiusKm) const
   {
      TrainId bestId = InvalidTrainId;
      float bestDistance = radiusKm;
      bool found = false;
      for (const Train& train : _trains)
      {
         const Line* pLine = _network.FindLine(train.lineId);
         if (pLine == nullptr)
         {
            continue;
         }

         const MapPoint trainPosition = TrainMapPosition(train, *pLine, _network);
         const float distance = DistanceKm(point, trainPosition);
         if (distance > radiusKm)
         {
            continue;
         }
         if (!found || distance < bestDistance)
         {
            bestDistance = distance;
            bestId = train.id;
            found = true;
         }
      }

      return bestId;
   }

   const Train* World::FindTrain(TrainId trainId) const
   {
      for (const Train& train : _trains)
      {
         if (train.id == trainId)
         {
            return &train;
         }
      }

      return nullptr;
   }

   Result World::CollectOnboardDemand(TrainId trainId, OnboardDemandList& demand) const
   {
      demand.clear();
      if (FindTrain(trainId) == nullptr)
      {
         return Result::InvalidArgument;
      }

      for (const Passenger& passenger : _passengers)
      {
         if (passenger.state != PassengerState::Onboard || passenger.trainId != trainId)
         {
            continue;
         }

         const StationId transferStationId = FirstTransferStation(passenger);
         bool foundDestination = false;
         for (OnboardDemand& entry : demand)
         {
            if (entry.destinationId != passenger.destinationId)
            {
               continue;
            }
            if (entry.transferStationId != transferStationId)
            {
               continue;
            }

            ++entry.passengerCount;
            foundDestination = true;
            break;
         }

         if (!foundDestination)
         {
            OnboardDemand entry;
            entry.destinationId = passenger.destinationId;
            entry.transferStationId = transferStationId;
            entry.passengerCount = 1;
            demand.push_back(entry);
         }
      }

      std::sort(demand.begin(), demand.end(), CompareOnboardDemandDescending);
      return Result::Ok;
   }

   Result World::CollectTrainsOnLine(LineId lineId, TrainOccupancyList& occupancy) const
   {
      occupancy.clear();
      const Line* pLine = _network.FindLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }

      for (const Train& train : _trains)
      {
         if (train.lineId != lineId)
         {
            continue;
         }

         TrainOccupancy entry;
         entry.trainId = train.id;
         entry.passengerCount = static_cast<uint32_t>(train.passengerIds.size());
         entry.nextStationId = NextStationOnLine(train, *pLine);
         occupancy.push_back(entry);
      }

      std::sort(occupancy.begin(), occupancy.end(), CompareTrainOccupancyById);
      return Result::Ok;
   }

   Result World::CollectLineDemand(LineId lineId, DestinationDemandList& demand) const
   {
      demand.clear();
      if (_network.FindLine(lineId) == nullptr)
      {
         return Result::InvalidArgument;
      }

      for (const Passenger& passenger : _passengers)
      {
         if (!PassengerIsOnboardLine(passenger, lineId, _trains))
         {
            continue;
         }

         AddDestinationCount(demand, passenger.destinationId);
      }

      std::sort(demand.begin(), demand.end(), CompareDemandDescending);
      return Result::Ok;
   }

   LineSegmentHit World::FindNearestSegmentOnLineInternal(const Line& line, MapPoint point) const
   {
      LineSegmentHit bestHit = MakeInvalidLineSegmentHit();
      bool found = false;
      const uint32_t segmentCount = LineSegmentCount(line);
      for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
      {
         StationId fromId = InvalidStationId;
         StationId toId = InvalidStationId;
         const Result endpointResult = LineSegmentEndpoints(line, segmentIndex, fromId, toId);
         if (IsErr(endpointResult))
         {
            continue;
         }

         const StationRecord* pFrom = _network.FindStation(fromId);
         const StationRecord* pTo = _network.FindStation(toId);
         if (pFrom == nullptr || pTo == nullptr)
         {
            continue;
         }

         const SegmentProjection projection = ProjectPointOnSegment(point, pFrom->position, pTo->position);
         if (!found || projection.distanceToSegmentKm < bestHit.distanceToSegmentKm)
         {
            bestHit.lineId = line.id;
            bestHit.segmentIndex = segmentIndex;
            bestHit.distanceAlongKm = projection.distanceAlongKm;
            bestHit.distanceToSegmentKm = projection.distanceToSegmentKm;
            found = true;
         }
      }

      return bestHit;
   }

   LineId World::FindNearestLine(MapPoint point, float radiusKm) const
   {
      return FindNearestLineSegment(point, radiusKm).lineId;
   }

   LineSegmentHit World::FindNearestLineSegment(MapPoint point, float radiusKm) const
   {
      LineSegmentHit bestHit = MakeInvalidLineSegmentHit();
      bool found = false;
      for (const Line& line : _network.GetLines())
      {
         const LineSegmentHit hit = FindNearestSegmentOnLineInternal(line, point);
         if (hit.lineId == InvalidLineId)
         {
            continue;
         }
         if (hit.distanceToSegmentKm > radiusKm)
         {
            continue;
         }
         if (!found || hit.distanceToSegmentKm < bestHit.distanceToSegmentKm)
         {
            bestHit = hit;
            found = true;
         }
      }

      return bestHit;
   }

   LineSegmentHit World::FindNearestSegmentOnLine(LineId lineId, MapPoint point) const
   {
      const Line* pLine = _network.FindLine(lineId);
      if (pLine == nullptr)
      {
         return MakeInvalidLineSegmentHit();
      }

      return FindNearestSegmentOnLineInternal(*pLine, point);
   }

   Result World::CollectUnconnectedStations(StationIdList& stationIds) const
   {
      stationIds.clear();
      for (const StationRecord& station : _network.GetStations())
      {
         if (!_network.IsStationOnAnyLine(station.id))
         {
            stationIds.push_back(station.id);
         }
      }

      return Result::Ok;
   }

   void World::AdjustTrainsAfterInsert(LineId lineId, uint32_t insertIndex)
   {
      for (Train& train : _trains)
      {
         if (train.lineId != lineId)
         {
            continue;
         }
         if (train.fromIndex >= static_cast<int32_t>(insertIndex))
         {
            train.fromIndex += 1;
         }

         ClampTrainToCurrentSegment(train);
      }
   }

   void World::ClampTrainToCurrentSegment(Train& train)
   {
      const Line* pLine = _network.FindLine(train.lineId);
      if (pLine == nullptr)
      {
         return;
      }

      const StationId fromId = CurrentStationOnLine(train, *pLine);
      const StationId toId = NextStationOnLine(train, *pLine);
      const StationRecord* pFrom = _network.FindStation(fromId);
      const StationRecord* pTo = _network.FindStation(toId);
      if (pFrom == nullptr || pTo == nullptr)
      {
         return;
      }

      const float lengthKm = DistanceKm(pFrom->position, pTo->position);
      if (train.distanceFromFromStationKm > lengthKm)
      {
         train.distanceFromFromStationKm = lengthKm;
      }
      if (train.distanceFromFromStationKm < 0.0f)
      {
         train.distanceFromFromStationKm = 0.0f;
      }
   }

   uint32_t World::GetWaitingCountAt(StationId stationId) const
   {
      uint32_t count = 0;
      for (const Passenger& passenger : _passengers)
      {
         if (passenger.state == PassengerState::Waiting && passenger.currentStationId == stationId)
         {
            ++count;
         }
      }

      return count;
   }

   Result World::CollectWaitingDemand(StationId stationId, DestinationDemandList& demand) const
   {
      demand.clear();
      if (_network.FindStation(stationId) == nullptr)
      {
         return Result::StationNotFound;
      }

      for (const Passenger& passenger : _passengers)
      {
         if (passenger.state != PassengerState::Waiting)
         {
            continue;
         }
         if (passenger.currentStationId != stationId)
         {
            continue;
         }

         bool foundDestination = false;
         for (DestinationDemand& entry : demand)
         {
            if (entry.destinationId != passenger.destinationId)
            {
               continue;
            }

            ++entry.waitingCount;
            foundDestination = true;
            break;
         }

         if (!foundDestination)
         {
            DestinationDemand entry;
            entry.destinationId = passenger.destinationId;
            entry.waitingCount = 1;
            demand.push_back(entry);
         }
      }

      std::sort(demand.begin(), demand.end(), CompareDemandDescending);
      return Result::Ok;
   }

   Result World::CollectGlobalWaitingDemand(DestinationDemandList& demand) const
   {
      demand.clear();
      for (const Passenger& passenger : _passengers)
      {
         if (passenger.state != PassengerState::Waiting)
         {
            continue;
         }

         AddDestinationCount(demand, passenger.destinationId);
      }

      std::sort(demand.begin(), demand.end(), CompareDemandDescending);
      return Result::Ok;
   }

   Result World::CollectCrowdedStations(StationCrowdingList& crowded) const
   {
      crowded.clear();
      for (const StationRecord& station : _network.GetStations())
      {
         const uint32_t waitingCount = GetWaitingCountAt(station.id);
         if (waitingCount == 0)
         {
            continue;
         }

         StationCrowding entry;
         entry.stationId = station.id;
         entry.waitingCount = waitingCount;
         crowded.push_back(entry);
      }

      std::sort(crowded.begin(), crowded.end(), CompareCrowdingDescending);
      if (crowded.size() > CrowdedStationMaxRows)
      {
         crowded.resize(CrowdedStationMaxRows);
      }

      return Result::Ok;
   }

   const Network& World::GetNetwork(void) const
   {
      return _network;
   }

   const TrainList& World::GetTrains(void) const
   {
      return _trains;
   }

   const PassengerList& World::GetPassengers(void) const
   {
      return _passengers;
   }

   uint32_t World::GetArrivedPassengerCount(void) const
   {
      return _arrivedPassengerCount;
   }

   uint32_t World::GetWaitingPassengerCount(void) const
   {
      return CountWaitingPassengers(_passengers);
   }

   uint32_t World::GetOnboardPassengerCount(void) const
   {
      return CountOnboardPassengers(_passengers);
   }

   float World::GetSimulationTimeSeconds(void) const
   {
      return _simulationTimeSeconds;
   }

   uint32_t World::GetCatalogStationCount(void) const
   {
      const auto catalogSize = static_cast<uint32_t>(_catalog.size());
      return catalogSize;
   }

   uint32_t World::GetMaxStationCount(void) const
   {
      return _maxStationCount;
   }

   uint32_t World::GetStationCap(void) const
   {
      const uint32_t catalogSize = GetCatalogStationCount();
      if (_maxStationCount < catalogSize)
      {
         return _maxStationCount;
      }

      return catalogSize;
   }

   uint32_t World::GetTrainCapacity(void) const
   {
      return _trainCapacity;
   }

   void World::MaybeSpawnStations(float deltaSeconds)
   {
      const auto activeStationCount = static_cast<uint32_t>(_network.GetStations().size());
      if (activeStationCount >= GetStationCap())
      {
         return;
      }

      if (_nextCatalogIndex >= _catalog.size())
      {
         return;
      }

      _timeUntilNextStationSeconds -= deltaSeconds;
      while (_timeUntilNextStationSeconds <= 0.0f && _nextCatalogIndex < _catalog.size())
      {
         const Result result = SpawnNextStation();
         if (IsErr(result))
         {
            break;
         }
         _timeUntilNextStationSeconds += StationSpawnIntervalSeconds;
      }
   }

   void World::MaybeSpawnPassengers(float deltaSeconds)
   {
      if (_passengerAutoSpawn == PassengerAutoSpawn::No)
      {
         return;
      }
      if (_network.GetStations().size() < 2)
      {
         return;
      }

      _passengerSpawnAccumulator += GlobalPassengerSpawnPerSecond * deltaSeconds;
      while (_passengerSpawnAccumulator >= 1.0f)
      {
         _passengerSpawnAccumulator -= 1.0f;
         SpawnRandomPassenger();
      }
   }

   Result World::SpawnRandomPassenger(void)
   {
      const StationRecordList& stations = _network.GetStations();
      if (stations.size() < 2)
      {
         return Result::Error;
      }

      WeightList originWeights;
      originWeights.reserve(stations.size());
      for (const StationRecord& station : stations)
      {
         originWeights.push_back(static_cast<float>(station.population));
      }

      const uint32_t originIndex = PickWeightedIndex(originWeights, _unitDistribution(_generator));
      if (originIndex == InvalidIndex || originIndex >= stations.size())
      {
         return Result::Error;
      }

      StationId destinationId = InvalidStationId;
      const Result destinationResult = PickGravityDestination(
         stations[originIndex].id,
         stations,
         DefaultGravityParameters(),
         _unitDistribution(_generator),
         destinationId);
      if (IsErr(destinationResult))
      {
         return destinationResult;
      }

      return SpawnPassenger(stations[originIndex].id, destinationId);
   }

   void World::UpdateTrains(float deltaSeconds)
   {
      for (Train& train : _trains)
      {
         const Line* pLine = _network.FindLine(train.lineId);
         if (pLine == nullptr)
         {
            continue;
         }

         if (train.motion == TrainMotion::Dwelling)
         {
            AlightAndBoard(train);
            train.dwellRemainingSeconds -= deltaSeconds;
            if (train.dwellRemainingSeconds <= 0.0f)
            {
               if (NextStationOnLine(train, *pLine) == InvalidStationId)
               {
                  train.direction = -train.direction;
               }
               train.motion = TrainMotion::Moving;
               train.distanceFromFromStationKm = 0.0f;
            }
            continue;
         }

         const TrainStepResult stepResult = AdvanceTrainMotion(train, *pLine, _network, deltaSeconds);
         if (stepResult == TrainStepResult::ArrivedAtStation)
         {
            AlightAndBoard(train);
         }
      }
   }

   void World::AlightAndBoard(Train& train)
   {
      const Line* pLine = _network.FindLine(train.lineId);
      if (pLine == nullptr)
      {
         return;
      }

      const StationId stationId = CurrentStationOnLine(train, *pLine);
      const StationId nextStationId = NextStationOnLine(train, *pLine);
      if (stationId == InvalidStationId)
      {
         return;
      }

      const PassengerIdList onboardCopy = train.passengerIds;
      for (PassengerId passengerId : onboardCopy)
      {
         Passenger* pPassenger = FindMutablePassenger(passengerId);
         if (pPassenger == nullptr)
         {
            RemovePassengerIdFromList(train.passengerIds, passengerId);
            continue;
         }

         HandleOnboardArrival(*pPassenger, train, stationId, nextStationId);
      }

      if (nextStationId == InvalidStationId)
      {
         return;
      }

      LineWaitList lineWaits;
      CollectLineWaits(lineWaits);
      for (Passenger& passenger : _passengers)
      {
         if (passenger.state != PassengerState::Waiting)
         {
            continue;
         }
         if (passenger.currentStationId != stationId)
         {
            continue;
         }

         MaybeRepath(passenger, lineWaits);
      }

      while (train.passengerIds.size() < _trainCapacity)
      {
         Passenger* pBest = nullptr;
         for (Passenger& passenger : _passengers)
         {
            if (!PassengerWantsNextStation(passenger, stationId, nextStationId))
            {
               continue;
            }
            if (pBest == nullptr || IsEarlierPlatformArrival(passenger, *pBest))
            {
               pBest = &passenger;
            }
         }
         if (pBest == nullptr)
         {
            break;
         }

         pBest->state = PassengerState::Onboard;
         pBest->trainId = train.id;
         pBest->route[pBest->routeHopIndex].lineId = train.lineId;
         train.passengerIds.push_back(pBest->id);
      }
   }

   void World::HandleOnboardArrival(
      Passenger& passenger,
      Train& train,
      StationId stationId,
      StationId nextStationId)
   {
      if (passenger.destinationId == stationId)
      {
         CompletePassenger(passenger, train);
         return;
      }

      while (passenger.routeHopIndex < passenger.route.size() &&
         passenger.route[passenger.routeHopIndex].toStationId == stationId)
      {
         ++passenger.routeHopIndex;
      }

      if (passenger.routeHopIndex >= passenger.route.size())
      {
         CompletePassenger(passenger, train);
         return;
      }

      const RouteHop& hop = passenger.route[passenger.routeHopIndex];
      if (hop.toStationId != nextStationId)
      {
         passenger.state = PassengerState::Waiting;
         passenger.trainId = InvalidTrainId;
         passenger.currentStationId = stationId;
         passenger.platformArrivalTimeSeconds = _simulationTimeSeconds;
         RemovePassengerIdFromList(train.passengerIds, passenger.id);
      }
   }

   void World::CompletePassenger(Passenger& passenger, Train& train)
   {
      const PassengerId passengerId = passenger.id;
      RemovePassengerIdFromList(train.passengerIds, passengerId);
      RemovePassengerById(passengerId);
      ++_arrivedPassengerCount;
   }

   void World::MaybeRepath(Passenger& passenger, const LineWaitList& lineWaits)
   {
      if (passenger.routeRevision == _pathRevision)
      {
         return;
      }

      Route route;
      const Result result = FindRoute(
         _network,
         passenger.currentStationId,
         passenger.destinationId,
         lineWaits,
         route);
      passenger.routeRevision = _pathRevision;
      passenger.routeHopIndex = 0;
      if (IsOk(result))
      {
         passenger.route = route;
      }
      else
      {
         passenger.route.clear();
      }
   }

   Passenger* World::FindMutablePassenger(PassengerId passengerId)
   {
      for (Passenger& passenger : _passengers)
      {
         if (passenger.id == passengerId)
         {
            return &passenger;
         }
      }

      return nullptr;
   }

   void World::RemovePassengerById(PassengerId passengerId)
   {
      for (uint32_t index = 0; index < _passengers.size(); ++index)
      {
         if (_passengers[index].id != passengerId)
         {
            continue;
         }

         _passengers[index] = _passengers.back();
         _passengers.pop_back();
         return;
      }
   }

   void World::RemovePassengerIdFromList(PassengerIdList& passengerIds, PassengerId passengerId)
   {
      for (uint32_t index = 0; index < passengerIds.size(); ++index)
      {
         if (passengerIds[index] != passengerId)
         {
            continue;
         }

         passengerIds[index] = passengerIds.back();
         passengerIds.pop_back();
         return;
      }
   }
} // namespace MiniDb
