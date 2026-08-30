# Changelog

All notable changes to MiniDB are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-08-30

Initial public release.

### Added

- Real-time schematic railway game set in Germany with cities drawn at real coordinates
- Line drafting, terminus extend, station insert, train placement, and line deletion
- Passenger simulation with gravity-model destinations, pathfinding, transfers, and boarding
- Economic mode: track and train costs, fares, maintenance, play session JSONL logs
- Sandbox mode and Never lose option
- Lose conditions: sustained negative balance, and platform patience after a grace period
- Main menu with Settings (station cap, train capacity, speed, random pool/order, events)
- About / Info screen with version, release date, and GitHub links
- In-game help, HUD, station/train/line inspectors, and unconnected-station list
- Unit tests for core simulation and data loading (CMake + GoogleTest)

### Credits

- Author: [ViezTrinker](https://github.com/ViezTrinker)
- Repository: [MiniDB](https://github.com/ViezTrinker/MiniDB)

[1.0.0]: https://github.com/ViezTrinker/MiniDB/releases/tag/v1.0.0
