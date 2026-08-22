# Simulation

All of this lives under `src/simulation/` and is owned by `World` (`world.h` / `world.cpp`).

## World tick

`World::Tick(deltaSeconds)`:

1. Advances `_simulationTimeSeconds`.
2. Maybe spawns the next catalog city (`StationSpawnIntervalSeconds`, default 5 s), until the station cap or catalog end.
3. Maybe spawns passengers (`GlobalPassengerSpawnPerSecond`, default 1.5 / s) if auto-spawn is on.
4. Moves every train and runs alight/board at stations.

`ResetSimulation` clears lines, trains, and passengers but keeps the loaded catalog.

## Stations and catalog

The catalog is the full JSON list, sorted by population. Playable stations are a prefix of that list.

- First `InitialStationCount` (6) cities appear on Start.
- Further cities appear over time until `GetStationCap()` (`min(maxStationCount, catalog size)`).
- `UnlimitedStationCount` means “all catalog cities”.

## Network

`Network` stores stations, finished lines, and an undirected adjacency graph used by the pathfinder.

A line is an ordered station list plus a color index and `LineLoop` (`Yes` / `No`).

- Open line: at least two stations. Trains reverse at the terminals.
- Loop: at least three unique stations, stored without a duplicated closing id. `LineSegmentCount` includes the closer back to the first station. Trains keep direction and wrap.

`AddLine` / `ExtendLine` / `InsertStationOnLine` / `RemoveLine` rebuild the graph and bump `Network::GetRevision()`. `World` also keeps `_pathRevision` so adding or removing trains invalidates passenger routes.

`World::AddLine` assigns the next palette color and places one train at the start of the line.

## Trains

`Train` (`train.h`) tracks line, `fromIndex`, `direction` (`+1` / `-1`), distance along the current segment, dwell, and onboard passenger ids.

- Speed is `TrainSpeedKmPerHour` (schematic, not timetable speed).
- On arrival the train dwells `TrainDwellSeconds`.
- Open lines reverse when the next index would leave the line.
- Loops wrap the index and never reverse.
- Capacity is `TrainCapacity` (32) unless tests override it with `SetTrainCapacity`.

`AddTrainToLineAt` places a train on the segment closest to a drop point (train-token drag).

Deleting a line unloads its passengers at the current station, removes its trains, then removes the line.

## Passenger destinations

Origins are weighted by city population. Destinations use the gravity / Huff model in `gravity_model.h`:

```text
P(j | i) = pop(j)^α / (distance(i,j) + d0)^γ
```

Defaults: `α = 1`, `γ = 1.6`, `d0 = 20 km`. The origin itself has weight 0. There is no real ticket OD matrix; see [data/DATA_SOURCES.md](../data/DATA_SOURCES.md).

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

Routes are a list of adjacent `RouteHop` values (`lineId`, `fromStationId`, `toStationId`). They are recomputed when `_pathRevision` changes (network or train count). Crowding is not part of the cost.

## Boarding

On dwell or arrival, `AlightAndBoard`:

1. Onboard passengers whose next hop is no longer this train (destination reached, transfer, or wrong direction) get off. Transfer and dump-off set `platformArrivalTimeSeconds` to now.
2. Waiting passengers at this station repath if needed.
3. They board only if the next hop matches this train’s line **and** its next station (correct direction).
4. If several people want that hop, the earliest `platformArrivalTimeSeconds` boards first; `id` breaks ties. This is a per-platform queue, not global spawn order.
5. Boarding stops at capacity. The rest wait for the next matching train.

Passengers do not pick a train id. They take the first matching train with space.
