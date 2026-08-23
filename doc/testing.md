# Testing

`MiniDBTests` is a GoogleTest executable. Simulation tests do not open a window.

```text
cmake --build build --target MiniDBTests
build/bin/MiniDBTests.exe
```

`MINIDB_SOURCE_DIR` is defined so tests can load `data/` and `tests/fixtures/`.

`tests/test_helpers.h` builds `StationRecord` values for network tests.

## Suites

| File | Coverage |
| --- | --- |
| `station_catalog_test.cpp` | JSON load, population filter, projection, UTF-8 names in `stations.json` |
| `projection_test.cpp` | Relative positions of major cities, positive map size |
| `geojson_outline_test.cpp` | Polygon parse, empty reject, real Germany file |
| `gravity_model_test.cpp` | Large vs small city, near vs far, origin weight 0 |
| `network_test.cpp` | Insert, loops, `IsStationOnAnyLine`, `RemoveLine` |
| `pathfinder_test.cpp` | Direct hops, transfers, one-way loops, wait-aware line choice, cycle time |
| `train_test.cpp` | Arrival, capacity, loop wrap, reverse-loop trips, reverse at terminal |
| `world_test.cpp` | Spawn order, cap, transfers, insert, unconnected list, train drop, demand, delete line, platform FIFO boarding |
| `line_editor_test.cpp` | `DeleteSelectedLine` (compiles `src/input/line_editor.cpp` into the test target) |

When you add a simulation feature or fix a bug, add or extend a test in the matching suite.

The renderer and `Game` event loop are not unit-tested; they need a window.
