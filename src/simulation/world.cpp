/*!
 *\file world.cpp
 *\brief Real-time simulation facade for stations, lines, trains and passengers.
 */

#include "simulation/world.h"

#include <algorithm>
#include <cmath>

#include "core/constants.h"
#include "data/station_catalog.h"
#include "simulation/gravity_model.h"
#include "simulation/pathfinder.h"
#include "simulation/play_session_log.h"

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

      bool CompareEventEndTimeAscending(const StationEvent& left, const StationEvent& right)
      {
         if (left.endTimeSeconds != right.endTimeSeconds)
         {
            return left.endTimeSeconds < right.endTimeSeconds;
         }

         return left.stationId < right.stationId;
      }

      void ShuffleStationRecords(StationRecordList& records, std::mt19937& generator)
      {
         if (records.size() < 2)
         {
            return;
         }

         for (uint32_t index = static_cast<uint32_t>(records.size()) - 1U; index > 0; --index)
         {
            std::uniform_int_distribution<uint32_t> distribution(0, index);
            const uint32_t swapIndex = distribution(generator);
            StationRecord temporary = records[index];
            records[index] = records[swapIndex];
            records[swapIndex] = temporary;
         }
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
      _trainCapacity(DefaultTrainCapacity),
      _timeUntilNextStationSeconds(StationSpawnIntervalSeconds),
      _generator(randomSeed),
      _unitDistribution(0.0f, 1.0f)
   {
      _economy.Clear();
   }

   void World::ResetSimulation(void)
   {
      _network.Clear();
      _trains.clear();
      _passengers.clear();
      _spawnQueue.clear();
      _nextSpawnIndex = 0;
      _nextTrainId = 1;
      _nextPassengerId = 1;
      _arrivedPassengerCount = 0;
      _topologyRevision = 0;
      _waitRevision = 0;
      _cachedLineWaits.clear();
      _waitingCountByStationIndex.clear();
      _waitingPassengersByStationIndex.clear();
      _trainCountByLineId.clear();
      _passengerSlotById.clear();
      _gravityWeightsByOriginIndex.clear();
      _activeEvents.clear();
      _eventCheckAccumulatorSeconds = 0.0f;
      _randomPool = RandomPool::No;
      _randomOrder = RandomOrder::No;
      _eventsEnabled = EventsEnabled::No;
      _simulationTimeSeconds = 0.0f;
      _timeUntilNextStationSeconds = StationSpawnIntervalSeconds;
      _passengerSpawnAccumulator = 0.0f;
      _passengerSpawnPressureMultiplier = 1.0f;
      _economy.Clear();
   }

   void World::ConfigureNewGame(RandomPool randomPool, RandomOrder randomOrder, EventsEnabled eventsEnabled)
   {
      _randomPool = randomPool;
      _randomOrder = randomOrder;
      _eventsEnabled = eventsEnabled;
      _activeEvents.clear();
      _eventCheckAccumulatorSeconds = 0.0f;
      BuildSpawnQueue();
   }

   void World::BuildSpawnQueue(void)
   {
      _spawnQueue.clear();
      _nextSpawnIndex = 0;
      if (_catalog.empty())
      {
         return;
      }

      const uint32_t stationCap = GetStationCap();
      uint32_t queueSize = stationCap;
      if (queueSize > _catalog.size())
      {
         queueSize = static_cast<uint32_t>(_catalog.size());
      }
      if (queueSize == 0)
      {
         return;
      }

      if (_randomPool == RandomPool::Yes)
      {
         StationRecordList pool = _catalog;
         ShuffleStationRecords(pool, _generator);
         _spawnQueue.assign(pool.begin(), pool.begin() + static_cast<std::ptrdiff_t>(queueSize));
      }
      else
      {
         _spawnQueue.assign(_catalog.begin(), _catalog.begin() + static_cast<std::ptrdiff_t>(queueSize));
      }

      if (_randomOrder == RandomOrder::Yes)
      {
         ShuffleStationRecords(_spawnQueue, _generator);
      }
   }

   void World::SetMaxStationCount(uint32_t maxStationCount)
   {
      uint32_t clamped = maxStationCount;
      if (clamped < MinimumStationCap)
      {
         clamped = MinimumStationCap;
      }

      const uint32_t catalogSize = GetCatalogStationCount();
      if (catalogSize > 0 && clamped > catalogSize)
      {
         clamped = catalogSize;
      }

      const auto activeStationCount = static_cast<uint32_t>(_network.GetStations().size());
      if (activeStationCount > clamped)
      {
         clamped = activeStationCount;
      }

      _maxStationCount = clamped;
   }

   Result World::LoadCatalogFromFile(std::string_view filePath)
   {
      const Result result = LoadStationCatalogFromFile(filePath, _catalog);
      if (IsErr(result))
      {
         return result;
      }

      _nextSpawnIndex = 0;
      BuildSpawnQueue();
      return Result::Ok;
   }

   Result World::LoadCatalogFromString(std::string_view jsonText)
   {
      const Result result = LoadStationCatalogFromString(jsonText, _catalog);
      if (IsErr(result))
      {
         return result;
      }

      _nextSpawnIndex = 0;
      BuildSpawnQueue();
      return Result::Ok;
   }

   Result World::SpawnInitialStations(void)
   {
      if (_catalog.empty())
      {
         return Result::Error;
      }

      if (_spawnQueue.empty())
      {
         BuildSpawnQueue();
      }

      uint32_t spawned = 0;
      uint32_t targetCount = GetInitialStationSpawnCount();
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

      _timeUntilNextStationSeconds = GetStationSpawnIntervalSeconds();
      return Result::Ok;
   }

   float World::GetStationSpawnIntervalSeconds(void) const
   {
      if (_economy.IsEconomicMode())
      {
         return EconomicStationSpawnIntervalSeconds;
      }

      return StationSpawnIntervalSeconds;
   }

   uint32_t World::GetInitialStationSpawnCount(void) const
   {
      if (_economy.IsEconomicMode())
      {
         return EconomicInitialStationCount;
      }

      return InitialStationCount;
   }

   Result World::SpawnNextStation(void)
   {
      if (_nextSpawnIndex >= _spawnQueue.size())
      {
         return Result::Error;
      }

      const auto activeStationCount = static_cast<uint32_t>(_network.GetStations().size());
      if (activeStationCount >= GetStationCap())
      {
         return Result::Error;
      }

      const Result result = _network.AddStation(_spawnQueue[_nextSpawnIndex]);
      if (IsErr(result))
      {
         return result;
      }

      ++_nextSpawnIndex;
      EnsureWaitingCacheSize();
      if (_eventsEnabled == EventsEnabled::Yes)
      {
         RefreshEventTargets();
      }
      else
      {
         RebuildGravityWeightMatrix();
      }
      ApplyPassengerSpawnPressureBump();
      return Result::Ok;
   }

   void World::SetPassengerAutoSpawn(PassengerAutoSpawn autoSpawn)
   {
      _passengerAutoSpawn = autoSpawn;
   }

   void World::SetTrainCapacity(uint32_t capacity)
   {
      if (capacity < MinimumTrainCapacity)
      {
         return;
      }

      _trainCapacity = capacity;
   }

   void World::ConfigureEconomy(uint32_t trainCapacity, GameMode gameMode, NeverLose neverLose)
   {
      _economy.ResetForNewGame(trainCapacity, gameMode, neverLose);
      _timeUntilNextStationSeconds = GetStationSpawnIntervalSeconds();
   }

   void World::SetPlaySessionLog(PlaySessionLog* pPlaySessionLog)
   {
      _pPlaySessionLog = pPlaySessionLog;
   }

   BankruptcyTickResult World::TickBankruptcy(float realDeltaSeconds, bool pause)
   {
      return _economy.TickBankruptcyTimer(realDeltaSeconds, pause);
   }

   const Economy& World::GetEconomy(void) const
   {
      return _economy;
   }

   Economy& World::GetEconomy(void)
   {
      return _economy;
   }

   GameMode World::GetGameMode(void) const
   {
      return _economy.GetGameMode();
   }

   int64_t World::GetBalance(void) const
   {
      return _economy.GetBalance();
   }

   uint32_t World::GetStationWaitingCapacity(StationId stationId) const
   {
      const StationRecord* pStation = _network.FindStation(stationId);
      if (pStation == nullptr)
      {
         return 1;
      }

      uint32_t capacity = pStation->population / StationWaitingCapacityPopulationFactor;
      if (capacity < 1)
      {
         capacity = 1;
      }

      return capacity;
   }

   float World::GetPassengerSpawnPressureMultiplier(void) const
   {
      return _passengerSpawnPressureMultiplier;
   }

   void World::ApplyPassengerSpawnPressureBump(void)
   {
      _passengerSpawnPressureMultiplier *= PassengerSpawnPressurePerUnlock;
   }

   Result World::TryPayForTrackSegments(
      const TrackSegmentRecordList& candidateSegments,
      TrackSegmentRecordList& newSegments)
   {
      if (!_economy.IsEconomicMode())
      {
         newSegments.clear();
         return Result::Ok;
      }

      const float newTrackKm = _economy.CollectNewSegmentKilometers(candidateSegments, newSegments);
      const int64_t buildCost = _economy.TrackBuildCost(newTrackKm);
      if (!_economy.TryDebit(buildCost))
      {
         if (_pPlaySessionLog != nullptr)
         {
            _pPlaySessionLog->LogPurchaseBlocked("track_build", buildCost, _economy.GetBalance());
         }
         return Result::InsufficientFunds;
      }

      _economy.RegisterBuiltSegments(newSegments);
      if (_pPlaySessionLog != nullptr)
      {
         const float totalUniqueKm = TotalUniqueTrackKilometers(_network) + newTrackKm;
         _pPlaySessionLog->LogTrackBuild(
            newTrackKm,
            buildCost,
            _economy.GetBalance(),
            totalUniqueKm);
      }

      return Result::Ok;
   }

   Result World::TryPayForTrainPurchase(void)
   {
      if (!_economy.IsEconomicMode())
      {
         return Result::Ok;
      }

      const int64_t purchaseCost = _economy.TrainPurchaseCostScaled();
      if (!_economy.TryDebit(purchaseCost))
      {
         if (_pPlaySessionLog != nullptr)
         {
            _pPlaySessionLog->LogPurchaseBlocked("train_purchase", purchaseCost, _economy.GetBalance());
         }
         return Result::InsufficientFunds;
      }

      return Result::Ok;
   }

   void World::TickEconomy(float simDeltaSeconds)
   {
      if (!_economy.IsEconomicMode())
      {
         return;
      }

      const float uniqueTrackKm = TotalUniqueTrackKilometers(_network);
      const auto trainCount = static_cast<uint32_t>(_trains.size());
      _economy.TickMaintenance(simDeltaSeconds, uniqueTrackKm, trainCount);
   }

   void World::ApplyCrowdingDwellPenalty(Train& train, StationId stationId)
   {
      if (!_economy.IsEconomicMode())
      {
         return;
      }
      if (train.crowdingDwellApplied == CrowdingDwellApplied::Yes)
      {
         return;
      }

      const uint32_t waitingCount = GetWaitingCountAt(stationId);
      const uint32_t waitingCapacity = GetStationWaitingCapacity(stationId);
      if (waitingCount < waitingCapacity)
      {
         return;
      }

      train.dwellRemainingSeconds += StationCrowdingDwellSeconds;
      train.crowdingDwellApplied = CrowdingDwellApplied::Yes;
      if (_pPlaySessionLog != nullptr)
      {
         _pPlaySessionLog->LogCrowdingDwell(
            stationId,
            waitingCount,
            waitingCapacity,
            StationCrowdingDwellSeconds);
      }
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
      MaybeUpdateEvents(clampedDelta);
      UpdateTrains(clampedDelta);
      TickEconomy(clampedDelta);
   }

   Result World::AddLine(const StationIdList& stationIds, LineId& lineId)
   {
      TrackSegmentRecordList candidateSegments;
      CollectSegmentsForLineDefinition(_network, stationIds, candidateSegments);

      TrackSegmentRecordList previewNewSegments;
      int64_t buildCost = 0;
      if (_economy.IsEconomicMode())
      {
         const float newTrackKm =
            _economy.CollectNewSegmentKilometers(candidateSegments, previewNewSegments);
         buildCost = _economy.TrackBuildCost(newTrackKm);
         const int64_t trainCost = _economy.TrainPurchaseCostScaled();
         const int64_t totalCost = buildCost + trainCost;
         if (_economy.GetBalance() < totalCost)
         {
            if (_pPlaySessionLog != nullptr)
            {
               _pPlaySessionLog->LogPurchaseBlocked("line_build", totalCost, _economy.GetBalance());
            }
            return Result::InsufficientFunds;
         }
      }

      TrackSegmentRecordList newSegments;
      const Result paymentResult = TryPayForTrackSegments(candidateSegments, newSegments);
      if (IsErr(paymentResult))
      {
         return paymentResult;
      }

      const uint32_t colorIndex = _network.GetCreatedLineCount() % LineColorCount;
      const Result result = _network.AddLine(stationIds, colorIndex, lineId);
      if (IsErr(result))
      {
         if (_economy.IsEconomicMode() && buildCost > 0)
         {
            _economy.Credit(buildCost);
            _economy.UnregisterBuiltSegments(newSegments);
         }
         return result;
      }

      NoteTopologyChanged();
      const Result trainResult = AddTrainToLine(lineId);
      if (IsErr(trainResult))
      {
         RemoveLine(lineId);
         if (_economy.IsEconomicMode() && buildCost > 0)
         {
            _economy.Credit(buildCost);
            _economy.UnregisterBuiltSegments(newSegments);
         }
         return trainResult;
      }

      return Result::Ok;
   }

   Result World::ExtendLine(LineId lineId, StationId stationId)
   {
      return ExtendLineAt(lineId, LineEnd::Back, stationId);
   }

   Result World::ExtendLineAt(LineId lineId, LineEnd end, StationId stationId)
   {
      const Line* pLine = _network.FindLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }

      TrackSegmentRecordList candidateSegments;
      if (pLine->loop == LineLoop::No)
      {
         TrackSegmentRecord record;
         const StationRecord* pNewStation = _network.FindStation(stationId);
         if (pNewStation == nullptr)
         {
            return Result::StationNotFound;
         }

         if (end == LineEnd::Back)
         {
            if (pLine->stationIds.empty())
            {
               return Result::LineTooShort;
            }
            const StationId fromId = pLine->stationIds.back();
            const StationRecord* pFrom = _network.FindStation(fromId);
            if (pFrom == nullptr)
            {
               return Result::StationNotFound;
            }
            record.stationA = fromId;
            record.stationB = stationId;
            record.distanceKm = DistanceKm(pFrom->position, pNewStation->position);
         }
         else
         {
            if (pLine->stationIds.empty())
            {
               return Result::LineTooShort;
            }
            const StationId toId = pLine->stationIds.front();
            const StationRecord* pTo = _network.FindStation(toId);
            if (pTo == nullptr)
            {
               return Result::StationNotFound;
            }
            record.stationA = stationId;
            record.stationB = toId;
            record.distanceKm = DistanceKm(pNewStation->position, pTo->position);
         }
         candidateSegments.push_back(record);
      }

      TrackSegmentRecordList newSegments;
      const Result paymentResult = TryPayForTrackSegments(candidateSegments, newSegments);
      if (IsErr(paymentResult))
      {
         return paymentResult;
      }

      const Result result = _network.ExtendLineAt(lineId, end, stationId);
      if (IsErr(result))
      {
         return result;
      }

      if (end == LineEnd::Front)
      {
         AdjustTrainsAfterInsert(lineId, 0);
      }

      NoteTopologyChanged();
      return Result::Ok;
   }

   Result World::GetTerminusAnchorPosition(LineId lineId, LineEnd end, float offsetKm, MapPoint& anchorPoint) const
   {
      const Line* pLine = _network.FindLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }
      if (pLine->loop == LineLoop::Yes || pLine->stationIds.size() < MinimumLineStations)
      {
         return Result::InvalidArgument;
      }

      const StationRecord* pTerminal = nullptr;
      const StationRecord* pInward = nullptr;
      if (end == LineEnd::Front)
      {
         pTerminal = _network.FindStation(pLine->stationIds.front());
         pInward = _network.FindStation(pLine->stationIds[1]);
      }
      else
      {
         pTerminal = _network.FindStation(pLine->stationIds.back());
         pInward = _network.FindStation(pLine->stationIds[pLine->stationIds.size() - 2]);
      }

      if (pTerminal == nullptr || pInward == nullptr)
      {
         return Result::Error;
      }

      anchorPoint = TerminusAnchorPosition(pTerminal->position, pInward->position, offsetKm);
      return Result::Ok;
   }

   bool World::HitTestTerminusAnchor(
      LineId lineId,
      MapPoint point,
      float radiusKm,
      float offsetKm,
      LineEnd& outEnd) const
   {
      const LineEnd ends[] = {LineEnd::Front, LineEnd::Back};
      bool found = false;
      float bestDistanceKm = 0.0f;

      for (LineEnd end : ends)
      {
         MapPoint anchorPoint;
         if (IsErr(GetTerminusAnchorPosition(lineId, end, offsetKm, anchorPoint)))
         {
            continue;
         }

         const float distanceKm = DistanceKm(point, anchorPoint);
         if (distanceKm > radiusKm)
         {
            continue;
         }

         if (!found || distanceKm < bestDistanceKm)
         {
            bestDistanceKm = distanceKm;
            outEnd = end;
            found = true;
         }
      }

      return found;
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
            AddPassengerToWaitingQueue(*pPassenger);
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
         DecrementTrainCount(lineId);
         _trains[index] = _trains.back();
         _trains.pop_back();
      }
   }

   void World::EnsureWaitingCacheSize(void)
   {
      const auto stationCount = static_cast<uint32_t>(_network.GetStations().size());
      if (_waitingCountByStationIndex.size() < stationCount)
      {
         _waitingCountByStationIndex.resize(stationCount, 0);
      }
      if (_waitingPassengersByStationIndex.size() < stationCount)
      {
         _waitingPassengersByStationIndex.resize(stationCount);
      }
   }

   void World::AddPassengerToWaitingQueue(Passenger& passenger)
   {
      const uint32_t stationIndex = _network.GetStationVectorIndex(passenger.currentStationId);
      if (stationIndex == InvalidIndex)
      {
         return;
      }

      EnsureWaitingCacheSize();
      WaitingPassengerQueue& queue = _waitingPassengersByStationIndex[stationIndex];
      for (PassengerId queuedPassengerId : queue)
      {
         if (queuedPassengerId == passenger.id)
         {
            return;
         }
      }

      uint32_t insertIndex = static_cast<uint32_t>(queue.size());
      for (uint32_t index = 0; index < queue.size(); ++index)
      {
         const Passenger* pExisting = FindMutablePassenger(queue[index]);
         if (pExisting == nullptr)
         {
            continue;
         }
         if (IsEarlierPlatformArrival(passenger, *pExisting))
         {
            insertIndex = index;
            break;
         }
      }

      queue.insert(queue.begin() + static_cast<int32_t>(insertIndex), passenger.id);
      ++_waitingCountByStationIndex[stationIndex];
   }

   void World::RemovePassengerFromWaitingQueue(PassengerId passengerId, StationId stationId)
   {
      const uint32_t stationIndex = _network.GetStationVectorIndex(stationId);
      if (stationIndex == InvalidIndex || stationIndex >= _waitingPassengersByStationIndex.size())
      {
         return;
      }

      WaitingPassengerQueue& queue = _waitingPassengersByStationIndex[stationIndex];
      for (uint32_t index = 0; index < queue.size(); ++index)
      {
         if (queue[index] != passengerId)
         {
            continue;
         }

         queue.erase(queue.begin() + static_cast<int32_t>(index));
         if (stationIndex < _waitingCountByStationIndex.size() && _waitingCountByStationIndex[stationIndex] > 0)
         {
            --_waitingCountByStationIndex[stationIndex];
         }
         return;
      }
   }

   void World::RebuildWaitingCaches(void)
   {
      _waitingCountByStationIndex.clear();
      _waitingPassengersByStationIndex.clear();
      EnsureWaitingCacheSize();

      for (Passenger& passenger : _passengers)
      {
         if (passenger.state != PassengerState::Waiting)
         {
            continue;
         }

         AddPassengerToWaitingQueue(passenger);
      }
   }

   void World::RebuildGravityWeightMatrix(void)
   {
      const StationRecordList& stations = _network.GetStations();
      const GravityParameters parameters = DefaultGravityParameters();
      _gravityWeightsByOriginIndex.clear();
      _gravityWeightsByOriginIndex.resize(stations.size());

      for (uint32_t originIndex = 0; originIndex < stations.size(); ++originIndex)
      {
         WeightList weights;
         const Result result = ComputeGravityWeights(
            stations[originIndex].id,
            stations,
            parameters,
            weights);
         if (IsErr(result))
         {
            continue;
         }

         for (uint32_t destinationIndex = 0; destinationIndex < stations.size(); ++destinationIndex)
         {
            if (IsStationEventActive(stations[destinationIndex].id))
            {
               weights[destinationIndex] *= EventDestinationWeightMultiplier;
            }
         }

         _gravityWeightsByOriginIndex[originIndex] = weights;
      }
   }

   bool World::IsStationEventActive(StationId stationId) const
   {
      for (const StationEvent& event : _activeEvents)
      {
         if (event.stationId == stationId)
         {
            return true;
         }
      }

      return false;
   }

   uint32_t World::TargetEventStationCount(void) const
   {
      const auto activeStationCount = static_cast<uint32_t>(_network.GetStations().size());
      if (activeStationCount == 0 || _eventsEnabled == EventsEnabled::No)
      {
         return 0;
      }

      const auto fractionCount = static_cast<uint32_t>(
         std::floor(static_cast<float>(activeStationCount) * EventStationFraction));
      if (fractionCount < 1)
      {
         return 1;
      }

      return fractionCount;
   }

   void World::ExpireFinishedEvents(void)
   {
      uint32_t index = 0;
      bool removed = false;
      while (index < _activeEvents.size())
      {
         if (_activeEvents[index].endTimeSeconds > _simulationTimeSeconds)
         {
            ++index;
            continue;
         }

         _activeEvents[index] = _activeEvents.back();
         _activeEvents.pop_back();
         removed = true;
      }

      if (removed)
      {
         RebuildGravityWeightMatrix();
      }
   }

   void World::RefreshEventTargets(void)
   {
      if (_eventsEnabled == EventsEnabled::No)
      {
         _activeEvents.clear();
         return;
      }

      const StationRecordList& stations = _network.GetStations();
      const uint32_t targetCount = TargetEventStationCount();
      if (targetCount == 0 || stations.empty())
      {
         if (!_activeEvents.empty())
         {
            _activeEvents.clear();
            RebuildGravityWeightMatrix();
         }
         return;
      }

      StationIdList candidates;
      candidates.reserve(stations.size());
      for (const StationRecord& station : stations)
      {
         if (!IsStationEventActive(station.id))
         {
            candidates.push_back(station.id);
         }
      }

      while (_activeEvents.size() > targetCount)
      {
         _activeEvents.pop_back();
      }

      while (_activeEvents.size() < targetCount && !candidates.empty())
      {
         std::uniform_int_distribution<uint32_t> distribution(
            0,
            static_cast<uint32_t>(candidates.size()) - 1U);
         const uint32_t pickIndex = distribution(_generator);
         StationEvent event;
         event.stationId = candidates[pickIndex];
         event.endTimeSeconds = _simulationTimeSeconds + EventDurationSeconds;
         _activeEvents.push_back(event);
         candidates[pickIndex] = candidates.back();
         candidates.pop_back();
      }

      RebuildGravityWeightMatrix();
   }

   void World::MaybeUpdateEvents(float deltaSeconds)
   {
      if (_eventsEnabled == EventsEnabled::No)
      {
         return;
      }

      ExpireFinishedEvents();
      _eventCheckAccumulatorSeconds += deltaSeconds;
      while (_eventCheckAccumulatorSeconds >= EventCheckIntervalSeconds)
      {
         _eventCheckAccumulatorSeconds -= EventCheckIntervalSeconds;
         RefreshEventTargets();
      }
   }

   Result World::CollectActiveEvents(StationEventList& events) const
   {
      events = _activeEvents;
      std::sort(events.begin(), events.end(), CompareEventEndTimeAscending);
      if (events.size() > EventStationMaxRows)
      {
         events.resize(EventStationMaxRows);
      }

      return Result::Ok;
   }

   void World::EnsureTrainCountCapacity(LineId lineId)
   {
      const auto requiredSize = static_cast<size_t>(lineId) + 1U;
      if (_trainCountByLineId.size() < requiredSize)
      {
         _trainCountByLineId.resize(requiredSize, 0);
      }
   }

   void World::IncrementTrainCount(LineId lineId)
   {
      EnsureTrainCountCapacity(lineId);
      ++_trainCountByLineId[lineId];
   }

   void World::DecrementTrainCount(LineId lineId)
   {
      if (lineId >= _trainCountByLineId.size())
      {
         return;
      }

      if (_trainCountByLineId[lineId] > 0)
      {
         --_trainCountByLineId[lineId];
      }
   }

   void World::CollectLineWaits(LineWaitList& lineWaits) const
   {
      lineWaits = _cachedLineWaits;
   }

   void World::RebuildLineWaitCache(void)
   {
      _cachedLineWaits.clear();
      for (const Line& line : _network.GetLines())
      {
         uint32_t trainCount = 0;
         if (line.id < _trainCountByLineId.size())
         {
            trainCount = _trainCountByLineId[line.id];
         }

         LineWait lineWait;
         lineWait.lineId = line.id;
         lineWait.waitSeconds = ExpectedLineWaitSeconds(line.cycleTimeSeconds, trainCount);
         _cachedLineWaits.push_back(lineWait);
      }
   }

   void World::NoteTopologyChanged(void)
   {
      ++_topologyRevision;
      RebuildLineWaitCache();
      RepathAllPassengers();
   }

   void World::NoteWaitChanged(void)
   {
      ++_waitRevision;
      RebuildLineWaitCache();
   }

   void World::RepathAllPassengers(void)
   {
      LineWaitList lineWaits;
      CollectLineWaits(lineWaits);
      for (Passenger& passenger : _passengers)
      {
         passenger.routeTopologyRevision = 0;
         passenger.routeWaitRevision = 0;
         MaybeRepath(passenger, lineWaits);
      }
   }

   void World::RegisterPassenger(Passenger& passenger)
   {
      const auto requiredSize = static_cast<size_t>(passenger.id) + 1U;
      if (_passengerSlotById.size() < requiredSize)
      {
         _passengerSlotById.resize(requiredSize, InvalidIndex);
      }

      const uint32_t slot = static_cast<uint32_t>(_passengers.size()) - 1U;
      _passengerSlotById[passenger.id] = slot;

      if (passenger.state == PassengerState::Waiting)
      {
         AddPassengerToWaitingQueue(passenger);
         TryBoardDwellingTrainsAt(passenger.currentStationId);
      }
   }

   void World::TryBoardDwellingTrainsAt(StationId stationId)
   {
      for (Train& train : _trains)
      {
         if (train.motion != TrainMotion::Dwelling)
         {
            continue;
         }

         const Line* pLine = _network.FindLine(train.lineId);
         if (pLine == nullptr)
         {
            continue;
         }

         if (CurrentStationOnLine(train, *pLine) != stationId)
         {
            continue;
         }

         AlightAndBoard(train);
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

      NoteTopologyChanged();
      return Result::Ok;
   }

   Result World::InsertStationOnLine(LineId lineId, uint32_t segmentIndex, StationId stationId)
   {
      const Line* pLine = _network.FindLine(lineId);
      if (pLine == nullptr)
      {
         return Result::InvalidArgument;
      }

      StationId fromId = InvalidStationId;
      StationId toId = InvalidStationId;
      const Result endpointResult = LineSegmentEndpoints(*pLine, segmentIndex, fromId, toId);
      if (IsErr(endpointResult))
      {
         return endpointResult;
      }

      const StationRecord* pInserted = _network.FindStation(stationId);
      const StationRecord* pFrom = _network.FindStation(fromId);
      const StationRecord* pTo = _network.FindStation(toId);
      if (pInserted == nullptr || pFrom == nullptr || pTo == nullptr)
      {
         return Result::StationNotFound;
      }

      TrackSegmentRecordList candidateSegments;
      TrackSegmentRecord firstSegment;
      firstSegment.stationA = fromId;
      firstSegment.stationB = stationId;
      firstSegment.distanceKm = DistanceKm(pFrom->position, pInserted->position);
      candidateSegments.push_back(firstSegment);

      TrackSegmentRecord secondSegment;
      secondSegment.stationA = stationId;
      secondSegment.stationB = toId;
      secondSegment.distanceKm = DistanceKm(pInserted->position, pTo->position);
      candidateSegments.push_back(secondSegment);

      TrackSegmentRecordList newSegments;
      const Result paymentResult = TryPayForTrackSegments(candidateSegments, newSegments);
      if (IsErr(paymentResult))
      {
         return paymentResult;
      }

      const Result result = _network.InsertStationOnLine(lineId, segmentIndex, stationId);
      if (IsErr(result))
      {
         return result;
      }

      AdjustTrainsAfterInsert(lineId, segmentIndex + 1);
      NoteTopologyChanged();
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

      const Result paymentResult = TryPayForTrainPurchase();
      if (IsErr(paymentResult))
      {
         return paymentResult;
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
      IncrementTrainCount(lineId);
      NoteWaitChanged();
      AlightAndBoard(_trains.back());
      if (_pPlaySessionLog != nullptr && _economy.IsEconomicMode())
      {
         _pPlaySessionLog->LogTrainPurchase(
            lineId,
            _economy.TrainPurchaseCostScaled(),
            _economy.GetBalance(),
            static_cast<uint32_t>(_trains.size()));
      }
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

      const Result paymentResult = TryPayForTrainPurchase();
      if (IsErr(paymentResult))
      {
         return paymentResult;
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
      IncrementTrainCount(lineId);
      NoteWaitChanged();
      if (_trains.back().motion == TrainMotion::Dwelling)
      {
         AlightAndBoard(_trains.back());
      }
      if (_pPlaySessionLog != nullptr && _economy.IsEconomicMode())
      {
         _pPlaySessionLog->LogTrainPurchase(
            lineId,
            _economy.TrainPurchaseCostScaled(),
            _economy.GetBalance(),
            static_cast<uint32_t>(_trains.size()));
      }
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
      passenger.routeTopologyRevision = 0;
      passenger.routeWaitRevision = 0;
      passenger.platformArrivalTimeSeconds = _simulationTimeSeconds;
      LineWaitList lineWaits;
      CollectLineWaits(lineWaits);
      MaybeRepath(passenger, lineWaits);
      _passengers.push_back(passenger);
      RegisterPassenger(_passengers.back());
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
      const uint32_t stationIndex = _network.GetStationVectorIndex(stationId);
      if (stationIndex == InvalidIndex || stationIndex >= _waitingCountByStationIndex.size())
      {
         return 0;
      }

      return _waitingCountByStationIndex[stationIndex];
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
         _timeUntilNextStationSeconds -= deltaSeconds;
         while (_timeUntilNextStationSeconds <= 0.0f)
         {
            ApplyPassengerSpawnPressureBump();
            _timeUntilNextStationSeconds += GetStationSpawnIntervalSeconds();
         }
         return;
      }

      if (_nextSpawnIndex >= _spawnQueue.size())
      {
         return;
      }

      _timeUntilNextStationSeconds -= deltaSeconds;
      while (_timeUntilNextStationSeconds <= 0.0f && _nextSpawnIndex < _spawnQueue.size())
      {
         const Result result = SpawnNextStation();
         if (IsErr(result))
         {
            break;
         }
         _timeUntilNextStationSeconds += GetStationSpawnIntervalSeconds();
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

      const float spawnPerSecond =
         PassengerSpawnPerSecondForCapacity(_trainCapacity) * _passengerSpawnPressureMultiplier;
      _passengerSpawnAccumulator += spawnPerSecond * deltaSeconds;
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
      if (originIndex >= _gravityWeightsByOriginIndex.size())
      {
         return Result::Error;
      }

      const Result destinationResult = PickGravityDestinationFromWeights(
         _gravityWeightsByOriginIndex[originIndex],
         stations,
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
            train.dwellRemainingSeconds -= deltaSeconds;
            if (train.dwellRemainingSeconds <= 0.0f)
            {
               if (NextStationOnLine(train, *pLine) == InvalidStationId)
               {
                  train.direction = -train.direction;
               }
               train.motion = TrainMotion::Moving;
               train.distanceFromFromStationKm = 0.0f;
               train.crowdingDwellApplied = CrowdingDwellApplied::No;
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

      ApplyCrowdingDwellPenalty(train, stationId);

      const PassengerIdList onboardCopy = train.passengerIds;
      for (PassengerId passengerId : onboardCopy)
      {
         Passenger* pPassenger = FindMutablePassenger(passengerId);
         if (pPassenger == nullptr)
         {
            RemovePassengerIdFromList(train.passengerIds, passengerId);
            continue;
         }

         if (pPassenger->state == PassengerState::Waiting)
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
      const uint32_t stationIndex = _network.GetStationVectorIndex(stationId);
      if (stationIndex != InvalidIndex && stationIndex < _waitingPassengersByStationIndex.size())
      {
         const WaitingPassengerQueue& waitingQueue = _waitingPassengersByStationIndex[stationIndex];
         for (PassengerId passengerId : waitingQueue)
         {
            Passenger* pPassenger = FindMutablePassenger(passengerId);
            if (pPassenger == nullptr)
            {
               continue;
            }

            MaybeRepath(*pPassenger, lineWaits);
         }
      }

      while (train.passengerIds.size() < _trainCapacity)
      {
         Passenger* pBest = nullptr;
         if (stationIndex != InvalidIndex && stationIndex < _waitingPassengersByStationIndex.size())
         {
            const WaitingPassengerQueue& waitingQueue = _waitingPassengersByStationIndex[stationIndex];
            for (PassengerId passengerId : waitingQueue)
            {
               Passenger* pPassenger = FindMutablePassenger(passengerId);
               if (pPassenger == nullptr)
               {
                  continue;
               }
               if (!PassengerWantsNextStation(*pPassenger, stationId, nextStationId))
               {
                  continue;
               }

               pBest = pPassenger;
               break;
            }
         }

         if (pBest == nullptr)
         {
            break;
         }

         RemovePassengerFromWaitingQueue(pBest->id, stationId);
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
      if (passenger.state == PassengerState::Waiting)
      {
         RemovePassengerIdFromList(train.passengerIds, passenger.id);
         return;
      }

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
         AddPassengerToWaitingQueue(passenger);
         TryBoardDwellingTrainsAt(stationId);
      }
   }

   void World::CompletePassenger(Passenger& passenger, Train& train)
   {
      if (_economy.IsEconomicMode())
      {
         const StationRecord* pOrigin = _network.FindStation(passenger.originId);
         const StationRecord* pDestination = _network.FindStation(passenger.destinationId);
         if (pOrigin != nullptr && pDestination != nullptr)
         {
            const float beelineKm = DistanceKm(pOrigin->position, pDestination->position);
            const int64_t fareAmount = _economy.FareForTrip(beelineKm);
            _economy.CreditFare(fareAmount);
            if (_pPlaySessionLog != nullptr)
            {
               _pPlaySessionLog->LogFare(
                  passenger.originId,
                  passenger.destinationId,
                  beelineKm,
                  fareAmount,
                  _economy.GetBalance());
            }
         }
      }

      const PassengerId passengerId = passenger.id;
      RemovePassengerIdFromList(train.passengerIds, passengerId);
      RemovePassengerById(passengerId);
      ++_arrivedPassengerCount;
   }

   void World::MaybeRepath(Passenger& passenger, const LineWaitList& lineWaits)
   {
      if (passenger.routeTopologyRevision == _topologyRevision &&
         passenger.routeWaitRevision == _waitRevision)
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
      passenger.routeTopologyRevision = _topologyRevision;
      passenger.routeWaitRevision = _waitRevision;
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
      if (passengerId >= _passengerSlotById.size())
      {
         return nullptr;
      }

      const uint32_t slot = _passengerSlotById[passengerId];
      if (slot == InvalidIndex || slot >= _passengers.size())
      {
         return nullptr;
      }

      if (_passengers[slot].id != passengerId)
      {
         return nullptr;
      }

      return &_passengers[slot];
   }

   void World::RemovePassengerById(PassengerId passengerId)
   {
      if (passengerId >= _passengerSlotById.size())
      {
         return;
      }

      const uint32_t slot = _passengerSlotById[passengerId];
      if (slot == InvalidIndex || slot >= _passengers.size())
      {
         return;
      }

      if (_passengers[slot].id != passengerId)
      {
         return;
      }

      if (_passengers[slot].state == PassengerState::Waiting)
      {
         RemovePassengerFromWaitingQueue(passengerId, _passengers[slot].currentStationId);
      }

      const PassengerId movedPassengerId = _passengers.back().id;
      _passengers[slot] = _passengers.back();
      _passengers.pop_back();
      _passengerSlotById[movedPassengerId] = slot;
      _passengerSlotById[passengerId] = InvalidIndex;
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
