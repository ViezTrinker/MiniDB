# MiniDB code documentation

This folder describes how the C++ code is structured and how the main systems work. Player-facing controls stay in the root [README](../README.md). Data licenses and generation are in [data/DATA_SOURCES.md](../data/DATA_SOURCES.md).

| Document | Contents |
| --- | --- |
| [Architecture](architecture.md) | Layers, CMake targets, startup, namespace |
| [Simulation](simulation.md) | Network, trains, passengers, routing, boarding |
| [Application and input](application.md) | Game loop, menu, line editor, controls wiring |
| [Rendering](rendering.md) | Views, map drawing, HUD, UTF-8 text |
| [Data and geography](data_and_geo.md) | Catalog, projection, outline |
| [Conventions](conventions.md) | Naming, headers, results, style used in this repo |
| [Testing](testing.md) | Test targets and what each suite covers |

`main.cpp` only constructs `Game`, initializes it from the executable path, and runs the loop. Core logic lives in `src/` under the namespace `MiniDb`.
