# Simulation

All of this lives under `src/simulation/` and is owned by `World` (`world.h` / `world.cpp`).

## World tick

`World::Tick(deltaSeconds)`:

1. Advances `_simulationTimeSeconds`.
2. Maybe spawns the next city from the spawn queue (`StationSpawnIntervalSeconds`, default 5 s), until the station cap or queue end.
3. Maybe spawns passengers (`PassengerSpawnPerSecondForCapacity`, e.g. 7.5 / s at capacity 160) if auto-spawn is on.
4. Maybe updates destination events when enabled.
5. Moves every train and runs alight/board at stations.

`ResetSimulation` clears lines, trains, passengers, spawn queue, and events but keeps the loaded catalog.

## Stations and catalog

The catalog is the full JSON list, sorted by population. At **Start**, `ConfigureNewGame` builds a spawn queue:

- Default: first `cap` catalog entries (largest cities).
- **Random pool**: `cap` uniform random catalog entries.
- **Random order**: shuffle that queue before spawning.

Then:

- First `InitialStationCount` (6) cities appear on Start (or fewer if the cap is lower).
- Further cities appear over time until `GetStationCap()` (`min(maxStationCount, catalog size)`).
- `UnlimitedStationCount` means “all catalog cities”.

## Destination events

When Events are enabled at Start:

- About every `EventCheckIntervalSeconds` (90 s), the active event set is refreshed.
- Target size is `max(1, floor(0.05 * activeStationCount))`.
- Each event lasts `EventDurationSeconds` (60 s) and multiplies that city’s gravity destination weight by `EventDestinationWeightMultiplier` (10).
- Sidebar Events section (bottom) lists active event cities via `CollectActiveEvents`.

## Network

`Network` stores stations, finished lines, and the travel graph used by the pathfinder.

A line is an ordered station list plus a color index and `LineLoop` (`Yes` / `No`).

- Open line: at least two stations. Trains reverse at the terminals. Graph edges are **undirected**.
- Loop: at least three unique stations, stored without a duplicated closing id. `LineSegmentCount` includes the closer back to the first station. Trains keep direction and wrap. Graph edges are **directed** in draw order, so passengers only route the way trains actually run.

`AddLine` / `ExtendLine` / `InsertStationOnLine` / `RemoveLine` rebuild the graph and bump `Network::GetRevision()`. `World` keeps `_topologyRevision` for network edits and `_waitRevision` for train-count changes so passengers repath lazily when waits change.

`World::AddLine` assigns the next palette color and places one train at the start of the line.

## Trains

`Train` (`train.h`) tracks line, `fromIndex`, `direction` (`+1` / `-1`), distance along the current segment, dwell, and onboard passenger ids.

- Speed is `TrainSpeedKmPerHour` (schematic, not timetable speed).
- On arrival the train dwells `TrainDwellSeconds`.
- Open lines reverse when the next index would leave the line.
- Loops wrap the index and never reverse.
- Capacity defaults to `DefaultTrainCapacity` (160) from Settings on Start, unless tests override it with `SetTrainCapacity`.
- Passenger spawn rate scales with capacity via `PassengerSpawnPerSecondForCapacity` (160 → 7.5 / s, 320 → 15 / s).

`AddTrainToLineAt` places a train on the segment closest to a drop point (train-token drag).

Deleting a line unloads its passengers at the current station, removes its trains, then removes the line.

## Passenger destinations

Origins are weighted by city population. Destinations use the gravity / Huff model in `gravity_model.h`:

```text
P(j | i) = pop(j)^α / (distance(i,j) + d0)^γ
```

Defaults: `α = 1`, `γ = 1.6`, `d0 = 20 km`. The origin itself has weight 0. Active event destinations multiply that weight by 10. There is no real ticket OD matrix; see [data/DATA_SOURCES.md](../data/DATA_SOURCES.md).

## Routing

`FindRoute` (`pathfinder.h`) is a Dijkstra-style search on `(station, line)` nodes.

Cost of an edge:

- Riding time of that segment (`distance / speed`).
- When boarding a **new** line (including the first line): expected platform wait for that line.
- When **changing** lines: plus `TrainDwellSeconds`.

Expected wait is half the headway:

```text
wait = 0.5 * cycleTime / trainCount
```

`LineCycleTimeSeconds` is one circuit on a loop, or a full out-and-back on a shuttle (travel plus dwell at each stop). With zero trains the wait is a full cycle.

Routes are a list of adjacent `RouteHop` values (`lineId`, `fromStationId`, `toStationId`). They are recomputed when `_topologyRevision` or `_waitRevision` changes (network topology or train headways). Crowding is not part of the cost.

## Boarding

On dwell or arrival, `AlightAndBoard`:

1. Onboard passengers whose next hop is no longer this train (destination reached, transfer, or wrong direction) get off. Transfer and dump-off set `platformArrivalTimeSeconds` to now.
2. Waiting passengers at this station repath if needed.
3. They board any train whose next station is their next hop. Shared segments (a shuttle and a loop that both go Berlin→Hamburg) are interchangeable; they do not wait for a specific line id.
4. If several people want that hop, the earliest `platformArrivalTimeSeconds` boards first; `id` breaks ties. This is a per-platform queue, not global spawn order.
5. Boarding stops at capacity. The rest wait for the next matching train.

Passengers do not pick a train id. They take the first train going to the next hop that still has space.

## Inspection queries

The right-hand inspector reads these `World` helpers:

- `CollectWaitingDemand` — waiting passengers at a station, grouped by destination (sidebar shows up to 25).
- `CollectOnboardDemand` — riders on one train, grouped by destination and first transfer.
- `CollectTrainsOnLine` — trains on a line with occupancy and next stop, ordered by train id.
- `CollectLineDemand` — passengers currently riding trains on that line, grouped by destination.
- `CollectGlobalWaitingDemand` — all waiting passengers grouped by destination.
- `CollectCrowdedStations` — up to ten stations with the most waiting passengers.
- `CollectActiveEvents` — stations with a temporary destination boost.
