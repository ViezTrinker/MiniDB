# Application and input

SFML-facing code: `src/application/`, `src/input/`, `src/main.cpp`.

## Game

`Game` (`game.h`) owns the window and the frame loop.

`Run` processes events, updates simulation when the play screen is active, then renders. Views are synced every frame so HUD and sidebar stay correct in fullscreen (`F11`).

Screens (`AppScreen`):

- `Menu` — start / resume / settings / quit.
- `Playing` — map, drafting, simulation.

Escape during play: cancel line or anchor drag, then cancel a draft, otherwise return to the menu. The current `World` stays so Resume works.

## Main menu

`MainMenu` draws a panel over a map backdrop.

- **Start** resets the simulation, applies Settings (station cap, train capacity, game speed, sandbox, never lose, AI play, random pool, random order, events), configures economy, builds the spawn queue, and spawns the first cities. Sandbox sets unlimited station cap.
- **Resume** is shown only when `HasActiveGame` is `Yes`. It continues the current world without re-applying Settings.
- **Settings** opens a second page:
  - Station cap: click the number to type digits. Default is `DefaultMaxStationCount` (100). Clamped between `MinimumStationCap` (2) and catalog size. If the cap is below `InitialStationCount`, the first wave is only that many cities.
  - **Train capacity**: click the number to type freely (minimum 1). Default is `DefaultTrainCapacity` (160). Passenger spawn rate is not set separately; it scales with capacity (`160` → `7.5` / s, `320` → `15` / s).
  - **Game speed**: click to cycle `1x` / `2x` / `4x` / `8x` / `16x` for the next Start (can still be changed in-game).
  - **Random pool**: sample `cap` cities uniformly from the catalog instead of the largest ones.
  - **Random order**: shuffle spawn order of the chosen pool.
  - **Events**: timed destination boosts during play.
  - **Sandbox**: no money, no station cap, no platform crowding penalty, no play log.
  - **Never lose**: economic costs and revenue still apply, but bankruptcy and platform-patience game over are disabled (hidden when Sandbox is on).
  - **AI play**: spectator mode. `PlayAgent` (`src/ai/`) builds lines, extends/inserts stations, and buys trains via `World` APIs each decision interval. Human build input (draft, confirm, train token, delete) is disabled; camera, pause, speed, and inspect still work.
- All Settings values apply only on **Start**.

## Spectator AI

`PlayAgent` runs in `Game::Update` before `World::Tick` when AI play is on. It plans greedily from an observation (waiting, patience risk, gravity demand, line headways, network components) and applies one affordable action. In economic mode it keeps a cash reserve (maintenance runway), caps trains per line by cycle/headway, prefers short regional links over mega-lines, and prioritizes bridging disconnected regional networks (with a longer bridge span and reserve spend for merges). Core AI code lives under `src/ai/` and is unit-tested without SFML.

## Line editor

`LineEditor` (`input/line_editor.h`) is SFML-free. `Game` translates clicks into `OnStationClicked`, `Confirm`, `Cancel`, `SelectLine`, `DeleteSelectedLine`, `UndoDraft`, `RedoDraft`, `AddTrainToSelectedLine`.

Drafting:

- First click starts a draft.
- Further clicks append stations (no duplicates except closing a loop).
- Clicking the first station again with at least three unique stations confirms a loop.
- Enter or right-click confirms an open line (at least two stations).
- Ctrl+Z removes the last drafted station; Ctrl+Y puts it back. A new click clears the redo stack. Confirm and cancel also clear it.

Terminus extension is handled in `Game` via draggable anchors on the selected line, not through `LineEditor`.

`World::AddLine` / `ExtendLineAt` / `InsertStationOnLine` do the mutation. In economic mode, `AddLine` requires funds for new track **and** the automatic first train before anything is created. The last confirmed or selected line stays selected.

## Input wiring in Game

| Action | Handling |
| --- | --- |
| Left-click station | Inspect it; `LineEditor::OnStationClicked` (starts or continues a draft) |
| Left-click train | Inspect it; select its line |
| Left-click line (no drag; away from stations) | Select the line; inspect trains, occupancy, and destinations |
| Drag terminus anchor onto a station | `ExtendLineAt` at the front or back of the selected line |
| Drag line onto a station | `InsertStationOnLine` on that segment |
| Drag train token onto a line | `AddTrainToLineAt` at the cursor |
| Bottom bar `<` / `>` | Slow down or speed up (1x / 2x / 4x / 8x / 16x) |
| Bottom bar Pause / Resume | Pause or resume the simulation |
| Bottom bar Menu | Return to the start menu |
| Ctrl+Z / Ctrl+Y | Undo or redo the last draft station |
| Delete | Delete selected line, or the inspected train’s line; cancels an in-progress line drag |
| T | Extra train on the selected line |
| Space | Pause |
| 1 / 2 / 4 / 8 | Time scale (1x–8x; use `>` for 16x) |
| Wheel | Zoom at cursor, or scroll the unconnected list |
| `+` / `-` | Zoom in or out at the map center |
| Arrow keys | Pan the map |
| Middle-drag | Pan |
| Click empty map | Return to the overview panel and close help |

Unconnected stations are listed in the right sidebar. Clicking a name inspects that station.

## Station cap and performance

The catalog can contain hundreds of cities. The Settings cap limits how many become playable stations (minimum `MinimumStationCap`). That is the main performance lever; pathfinding and boarding scan passengers and trains each tick.
