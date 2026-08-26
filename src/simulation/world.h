/*!
 *\file world.h
 *\brief Real-time simulation facade for stations, lines, trains and passengers.
 */

#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include <random>
#include <string_view>

#include "core/constants.h"
#include "core/result.h"
#include "core/types.h"
#include "simulation/network.h"
#include "simulation/passenger.h"
#include "simulation/pathfinder.h"
#include "simulation/train.h"

namespace MiniDb
{
   enum class PassengerAutoSpawn : bool
   {
      No = false,
      Yes = true
   };

   enum class RandomPool : bool
   {
      No = false,
      Yes = true
   };

   enum class RandomOrder : bool
   {
      No = false,
      Yes = true
   };

   enum class EventsEnabled : bool
   {
      No = false,
      Yes = true
   };

   struct StationEvent
   {
      StationId stationId;
      float endTimeSeconds;
   };

   using StationEventList = std::vector<StationEvent>;

   using WaitingPassengerQueue = std::vector<PassengerId>;
   using WaitingPassengerQueueList = std::vector<WaitingPassengerQueue>;
   using GravityWeightMatrix = std::vector<WeightList>;

   class World
   {
   public:
      /*!
       *\brief Creates an empty world.
       *
       *\param[in] randomSeed Seed for passenger origin and destination sampling.
       */
      explicit World(uint32_t randomSeed);

      /*!
       *\brief Loads the city catalog from a JSON file.
       *
       *\param[in] filePath Path to stations.json.
       */
      Result LoadCatalogFromFile(std::string_view filePath);

      /*!
       *\brief Loads the city catalog from a JSON string.
       *
       *\param[in] jsonText Catalog JSON.
       */
      Result LoadCatalogFromString(std::string_view jsonText);

      /*!
       *\brief Clears lines, trains and passengers while keeping the catalog.
       */
      void ResetSimulation(void);

      /*!
       *\brief Caps how many catalog cities become playable stations.
       *
       *\param[in] maxStationCount Maximum active stations, or UnlimitedStationCount.
       */
      void SetMaxStationCount(uint32_t maxStationCount);

      /*!
       *\brief Configures spawn pool, spawn order and destination events for a new run.
       *
       *\param[in] randomPool Whether to sample a random catalog subset.
       *\param[in] randomOrder Whether to shuffle the spawn queue.
       *\param[in] eventsEnabled Whether timed destination events are active.
       */
      void ConfigureNewGame(RandomPool randomPool, RandomOrder randomOrder, EventsEnabled eventsEnabled);

      /*!
       *\brief Spawns the first cities from the spawn queue.
       */
      Result SpawnInitialStations(void);

      /*!
       *\brief Spawns the next city from the spawn queue if any remain under the station cap.
       */
      Result SpawnNextStation(void);

      /*!
       *\brief Enables or disables automatic passenger spawning.
       *
       *\param[in] autoSpawn Whether passengers appear over time.
       */
      void SetPassengerAutoSpawn(PassengerAutoSpawn autoSpawn);

      /*!
       *\brief Sets train capacity used when boarding.
       *
       * Passenger spawn rate scales linearly with capacity
       * (`PassengerSpawnPerSecondForCapacity`).
       *
       *\param[in] capacity Maximum passengers per train.
       */
      void SetTrainCapacity(uint32_t capacity);

      /*!
       *\brief Advances the simulation.
       *
       *\param[in] deltaSeconds Elapsed simulation time.
       */
      void Tick(float deltaSeconds);

      /*!
       *\brief Creates a finished line and identifiers it.
       *
       *\param[in] stationIds Ordered stations on the line.
       *\param[out] lineId New line identifier.
       */
      Result AddLine(const StationIdList& stationIds, LineId& lineId);

      /*!
       *\brief Appends a station to an existing line.
       *
       *\param[in] lineId Line to extend.
       *\param[in] stationId Station to append.
       */
      Result ExtendLine(LineId lineId, StationId stationId);

      /*!
       *\brief Prepends or appends a station at one end of an existing line.
       *
       *\param[in] lineId Line to extend.
       *\param[in] end Which terminus receives the station.
       *\param[in] stationId Station to add.
       */
      Result ExtendLineAt(LineId lineId, LineEnd end, StationId stationId);

      /*!
       *\brief Returns the map position of a terminus anchor handle.
       *
       *\param[in] lineId Line that owns the anchor.
       *\param[in] end Which terminus anchor to locate.
       *\param[in] offsetKm Outward offset from the station in kilometres.
       *\param[out] anchorPoint Anchor position in map kilometres.
       */
      Result GetTerminusAnchorPosition(LineId lineId, LineEnd end, float offsetKm, MapPoint& anchorPoint) const;

      /*!
       *\brief Returns true when a point hits a terminus anchor on the given line.
       *
       *\param[in] lineId Line to test.
       *\param[in] point Map location in kilometres.
       *\param[in] radiusKm Hit radius around each anchor.
       *\param[in] offsetKm Outward anchor offset from each terminus station.
       *\param[out] outEnd Which anchor was hit.
       */
      bool HitTestTerminusAnchor(
         LineId lineId,
         MapPoint point,
         float radiusKm,
         float offsetKm,
         LineEnd& outEnd) const;

      /*!
       *\brief Inserts a station into a line between the two ends of a segment.
       *
       *\param[in] lineId Line to change.
       *\param[in] segmentIndex Segment that receives the station.
       *\param[in] stationId Station to insert.
       */
      Result InsertStationOnLine(LineId lineId, uint32_t segmentIndex, StationId stationId);

      /*!
       *\brief Deletes a line, its trains, and repaths affected passengers.
       *
       *\param[in] lineId Line to delete.
       */
      Result RemoveLine(LineId lineId);

      /*!
       *\brief Adds a shuttle train at the start of a line.
       *
       *\param[in] lineId Line that receives the train.
       */
      Result AddTrainToLine(LineId lineId);

      /*!
       *\brief Adds a train on the nearest segment to a drop point.
       *
       *\param[in] lineId Line that receives the train.
       *\param[in] dropPoint Map location where the train was released.
       */
      Result AddTrainToLineAt(LineId lineId, MapPoint dropPoint);

      /*!
       *\brief Spawns one passenger with a chosen origin and destination.
       *
       *\param[in] originId Start station.
       *\param[in] destinationId Target station.
       */
      Result SpawnPassenger(StationId originId, StationId destinationId);

      /*!
       *\brief Returns the closest station within radius, or InvalidStationId.
       *
       *\param[in] point Map location in kilometres.
       *\param[in] radiusKm Hit radius.
       */
      StationId HitTestStation(MapPoint point, float radiusKm) const;

      /*!
       *\brief Returns the closest train within radius, or InvalidTrainId.
       *
       *\param[in] point Map location in kilometres.
       *\param[in] radiusKm Hit radius.
       */
      TrainId HitTestTrain(MapPoint point, float radiusKm) const;

      /*!
       *\brief Returns the closest line within radius, or InvalidLineId.
       *
       *\param[in] point Map location in kilometres.
       *\param[in] radiusKm Maximum distance to a line segment.
       */
      LineId FindNearestLine(MapPoint point, float radiusKm) const;

      /*!
       *\brief Closest line segment within radius, or an invalid hit.
       *
       *\param[in] point Map location in kilometres.
       *\param[in] radiusKm Maximum distance to a line segment.
       */
      LineSegmentHit FindNearestLineSegment(MapPoint point, float radiusKm) const;

      /*!
       *\brief Closest segment of one line, with no radius limit.
       *
       *\param[in] lineId Line to search.
       *\param[in] point Map location in kilometres.
       */
      LineSegmentHit FindNearestSegmentOnLine(LineId lineId, MapPoint point) const;

      /*!
       *\brief Stations that are not yet on any line.
       *
       *\param[out] stationIds Unconnected station identifiers.
       */
      Result CollectUnconnectedStations(StationIdList& stationIds) const;

      /*!
       *\brief Number of passengers waiting at a station.
       *
       *\param[in] stationId Station to inspect.
       */
      uint32_t GetWaitingCountAt(StationId stationId) const;

      /*!
       *\brief Groups waiting passengers at a station by destination, largest first.
       *
       *\param[in] stationId Station whose platform demand is listed.
       *\param[out] demand Destination counts for waiting passengers.
       */
      Result CollectWaitingDemand(StationId stationId, DestinationDemandList& demand) const;

      /*!
       *\brief Groups all waiting passengers by destination, largest first.
       *
       *\param[out] demand Destination counts for passengers not riding trains.
       */
      Result CollectGlobalWaitingDemand(DestinationDemandList& demand) const;

      /*!
       *\brief Lists the busiest stations by waiting passengers, up to ten.
       *
       *\param[out] crowded Station crowding counts, busiest first.
       */
      Result CollectCrowdedStations(StationCrowdingList& crowded) const;

      /*!
       *\brief Groups onboard passengers by destination and first transfer.
       *
       *\param[in] trainId Train to inspect.
       *\param[out] demand Destination counts and transfer stations.
       */
      Result CollectOnboardDemand(TrainId trainId, OnboardDemandList& demand) const;

      /*!
       *\brief Lists trains on a line with occupancy and next stop, by train id.
       *
       *\param[in] lineId Line to inspect.
       *\param[out] occupancy One entry per train on that line.
       */
      Result CollectTrainsOnLine(LineId lineId, TrainOccupancyList& occupancy) const;

      /*!
       *\brief Groups onboard passengers on a line by destination, largest first.
       *
       * Only counts people currently riding trains on this line.
       *
       *\param[in] lineId Line to inspect.
       *\param[out] demand Destination counts for those passengers.
       */
      Result CollectLineDemand(LineId lineId, DestinationDemandList& demand) const;

      /*!
       *\brief Lists stations with an active destination event.
       *
       *\param[out] events Active events, soonest expiry first.
       */
      Result CollectActiveEvents(StationEventList& events) const;

      /*!
       *\brief Finds a train by id.
       *
       *\param[in] trainId Train identifier.
       */
      const Train* FindTrain(TrainId trainId) const;

      const Network& GetNetwork(void) const;
      const TrainList& GetTrains(void) const;
      const PassengerList& GetPassengers(void) const;
      uint32_t GetArrivedPassengerCount(void) const;
      uint32_t GetWaitingPassengerCount(void) const;
      uint32_t GetOnboardPassengerCount(void) const;
      float GetSimulationTimeSeconds(void) const;
      uint32_t GetCatalogStationCount(void) const;

      /*!
       *\brief Configured station cap, or UnlimitedStationCount.
       */
      uint32_t GetMaxStationCount(void) const;

      /*!
       *\brief How many stations can actually appear (min of cap and catalog size).
       */
      uint32_t GetStationCap(void) const;

      uint32_t GetTrainCapacity(void) const;

   private:
      void UnloadTrainPassengers(Train& train);
      void RemoveTrainsOnLine(LineId lineId);

      /*!
       *\brief Fills expected platform waits from cached cycle times and train counts.
       *
       *\param[out] lineWaits One entry per finished line.
       */
      void CollectLineWaits(LineWaitList& lineWaits) const;

      /*!
       *\brief Rebuilds cached line waits after train or network changes.
       */
      void RebuildLineWaitCache(void);

      /*!
       *\brief Marks routes stale and recomputes them after topology changes.
       */
      void NoteTopologyChanged(void);

      /*!
       *\brief Invalidates wait-sensitive routes after train count changes.
       */
      void NoteWaitChanged(void);

      void RepathAllPassengers(void);
      void AdjustTrainsAfterInsert(LineId lineId, uint32_t insertIndex);
      void ClampTrainToCurrentSegment(Train& train);
      LineSegmentHit FindNearestSegmentOnLineInternal(const Line& line, MapPoint point) const;
      void MaybeSpawnStations(float deltaSeconds);
      void MaybeSpawnPassengers(float deltaSeconds);
      void MaybeUpdateEvents(float deltaSeconds);
      void ExpireFinishedEvents(void);
      void RefreshEventTargets(void);
      bool IsStationEventActive(StationId stationId) const;
      uint32_t TargetEventStationCount(void) const;
      void BuildSpawnQueue(void);
      Result SpawnRandomPassenger(void);
      void UpdateTrains(float deltaSeconds);
      void AlightAndBoard(Train& train);
      void HandleOnboardArrival(Passenger& passenger, Train& train, StationId stationId, StationId nextStationId);
      void CompletePassenger(Passenger& passenger, Train& train);

      /*!
       *\brief Rebuilds the passenger route when revisions changed.
       *
       *\param[in,out] passenger Passenger to update.
       *\param[in] lineWaits Expected wait per line.
       */
      void MaybeRepath(Passenger& passenger, const LineWaitList& lineWaits);

      void RegisterPassenger(Passenger& passenger);
      Passenger* FindMutablePassenger(PassengerId passengerId);
      void RemovePassengerById(PassengerId passengerId);
      void RemovePassengerIdFromList(PassengerIdList& passengerIds, PassengerId passengerId);

      void EnsureWaitingCacheSize(void);
      void AddPassengerToWaitingQueue(Passenger& passenger);
      void RemovePassengerFromWaitingQueue(PassengerId passengerId, StationId stationId);
      void RebuildWaitingCaches(void);
      void RebuildGravityWeightMatrix(void);
      void IncrementTrainCount(LineId lineId);
      void DecrementTrainCount(LineId lineId);
      void EnsureTrainCountCapacity(LineId lineId);
      void TryBoardDwellingTrainsAt(StationId stationId);

      StationRecordList _catalog;
      StationRecordList _spawnQueue;
      uint32_t _nextSpawnIndex = 0;
      uint32_t _maxStationCount = DefaultMaxStationCount;
      RandomPool _randomPool = RandomPool::No;
      RandomOrder _randomOrder = RandomOrder::No;
      EventsEnabled _eventsEnabled = EventsEnabled::No;
      StationEventList _activeEvents;
      float _eventCheckAccumulatorSeconds = 0.0f;
      Network _network;
      TrainList _trains;
      PassengerList _passengers;
      TrainId _nextTrainId = 1;
      PassengerId _nextPassengerId = 1;
      uint32_t _arrivedPassengerCount = 0;
      uint32_t _trainCapacity = 0;
      uint64_t _topologyRevision = 0;
      uint64_t _waitRevision = 0;
      LineWaitList _cachedLineWaits;
      std::vector<uint32_t> _waitingCountByStationIndex;
      WaitingPassengerQueueList _waitingPassengersByStationIndex;
      std::vector<uint32_t> _trainCountByLineId;
      std::vector<uint32_t> _passengerSlotById;
      GravityWeightMatrix _gravityWeightsByOriginIndex;
      float _simulationTimeSeconds = 0.0f;
      float _timeUntilNextStationSeconds = 0.0f;
      float _passengerSpawnAccumulator = 0.0f;
      PassengerAutoSpawn _passengerAutoSpawn = PassengerAutoSpawn::Yes;
      std::mt19937 _generator;
      std::uniform_real_distribution<float> _unitDistribution;
   };
} // namespace MiniDb

#endif // WORLD_H
