# MiniDB

Real-time schematic railway game for Germany. Cities with at least 10,000 inhabitants appear on a Germany silhouette at their real coordinates. You draw lines between stations; trains shuttle in real time and passengers choose destinations with a gravity model (large cities attract more trips than small towns).

This repository is the **core simulation MVP**: map, stations, line drawing, trains, and passengers. Economy, maintenance, and regional vs long-distance services are not included yet.

Code structure and system behaviour are documented in [doc/](doc/README.md).

## Build

Requirements: CMake 3.24+, a C++20 compiler (MSVC on Windows), Git, and a network connection on the first configure (SFML, GoogleTest, and nlohmann/json are fetched automatically).

```text
cmake -S . -B build
cmake --build build --config Release
```

Run tests:

```text
ctest --test-dir build -C Release --output-on-failure
```

The game executable is `build/bin/Release/MiniDB.exe` on Windows (or `build/bin/MiniDB` on single-config generators). `data/` is copied next to the executable.

The game opens on a start menu. The default station cap is 100 (enough for performance). Click the number field to type a count, or use `<` / `>` (and the arrow keys) to change it in steps of 50. Escape during play returns to the menu; Resume continues the current game.

## Controls

- Left-click a station to draft a line; waiting passengers appear in the right-hand panel
- Click a train to inspect onboard passengers, destinations and transfers in that panel
- Click the first station of a draft again to close a loop (`A, B, C, A`)
- Click empty map to clear the inspector
- Enter or right-click to finish a line (one train is added automatically)
- Click the end station of the selected line to extend it, or click the first station to close a loop
- Drag a line onto a station to insert it between the two segment ends
- Del deletes the selected line (or the line of an inspected train)
- Ctrl+Z undoes the last station in a draft, Ctrl+Y restores it
- Drag the train token (bottom left) onto an existing line to place a train at the drop point
- Unconnected stations are listed below the inspector on the right; click a name to inspect it
- `?` opens a short help popup
- Mouse wheel zooms (station markers stay a constant pixel size), middle-mouse drag pans
- Space pauses, `1` / `2` / `4` / `8` set simulation speed (default is 4x)
- F11 toggles fullscreen
- Esc cancels a draft, or returns to the start menu

## Passenger destinations

There is no public nationwide DB origin-destination matrix. Destinations are sampled from a gravity model using city population and map distance, so Berlin is a frequent target and a 12,000-inhabitant town is not. See [data/DATA_SOURCES.md](data/DATA_SOURCES.md).

## Routing and boarding

Passengers pick the fastest route in simulation time: riding time plus, whenever they board a line, the expected platform wait. That wait is half the line's headway (cycle time divided by the number of trains). A transfer also adds the station dwell. Routes are recomputed when lines or trains change.

At a station they board the first train that serves the next hop of that route and still has space. If the train is full, earlier arrivals on that platform board first (the time they spawned or got off to transfer), not spawn order across the whole map.

## Regenerating station data

```text
python tools/generate_station_data.py
```

This downloads GeoNames and the Germany outline, matches cities to railway stations, and overwrites `data/stations.json` and `data/germany.geojson`.

## What this MVP does not include

- Money, ticket prices, maintenance, unprofitable lines
- Regional vs long-distance trains
- Multiple stations in one city
- The real railway geometry or timetable speeds
