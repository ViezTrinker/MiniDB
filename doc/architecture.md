# Architecture

MiniDB is a C++20 real-time game. Simulation code has no SFML dependency so it can be tested without a window.

## Layers

```text
src/main.cpp
      |
      v
src/application/     Game, MainMenu          (SFML window and loop)
src/input/           LineEditor
src/rendering/       Renderer, Utf8SfString
      |
      v
src/simulation/      World facade
      |-- Network, Train, Passenger, Pathfinder, GravityModel
src/data/            Station catalog JSON
src/geo/             WGS84 projection, GeoJSON outline
src/core/            Types, Result, constants
```

`World` is the simulation facade. `Game` owns the window, input, `World`, `Renderer`, `MainMenu`, and `LineEditor`.

## CMake targets

Defined in the root `CMakeLists.txt`.

| Target | Role |
| --- | --- |
| `MiniDbCore` | Static library: data, geo, simulation. Links nlohmann/json. |
| `MiniDB` | Game executable. Links `MiniDbCore` and SFML 3.1 Graphics. |
| `MiniDBTests` | GoogleTest runner. Links `MiniDbCore`. Also compiles `line_editor.cpp` because the editor is not in the core library. |

Dependencies are fetched with CMake `FetchContent`: SFML 3.1.0, GoogleTest 1.15.2, nlohmann/json 3.11.3.

Include root is `src/`. Headers are listed explicitly in CMake (required for this project). After building `MiniDB`, `data/` is copied next to the executable.

## Startup

1. `main` passes `argv[0]` into `Game::Initialize`.
2. The window is created (`1280x720` by default).
3. A system font is loaded (Segoe UI / Arial on Windows, DejaVu or Liberation on Linux).
4. `stations.json` and `germany.geojson` are resolved relative to the executable (and a few fallback paths).
5. The start menu is shown. `Start` calls `World::ResetSimulation`, applies the station cap, and `SpawnInitialStations`.

## Identifiers and results

IDs are `uint32_t` aliases in `core/types.h`. Invalid sentinels are `0xFFFFFFFF`.

Functions that can fail return `enum class Result` (`core/result.h`). Use `IsErr`, `IsOk`, and `IsMsg` instead of treating the enum as a boolean. Negative values are errors.

## Time

`Game::Update` multiplies wall-clock delta by `_timeScale` (default `4`) and passes that to `World::Tick`. Pause sets the scaled delta to zero. Simulation constants such as train speed and passenger spawn rates are in simulation seconds, not wall-clock seconds.
