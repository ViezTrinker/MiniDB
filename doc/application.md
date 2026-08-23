# Application and input

SFML-facing code: `src/application/`, `src/input/`, `src/main.cpp`.

## Game

`Game` (`game.h`) owns the window and the frame loop.

`Run` processes events, updates simulation when the play screen is active, then renders. Views are synced every frame so HUD and sidebar stay correct in fullscreen (`F11`).

Screens (`AppScreen`):

- `Menu` — start / resume / quit and station cap.
- `Playing` — map, drafting, simulation.

Escape during play: cancel line drag, then cancel a draft, otherwise return to the menu. The current `World` stays so Resume works.

## Main menu

`MainMenu` draws a panel over a map backdrop.

- **Start** resets the simulation, applies the cap, spawns the first cities.
- **Resume** is shown only when `HasActiveGame` is `Yes`.
- Station cap: click the number to type digits, or `<` / `>` (and arrow keys) in steps of 50. Default is `DefaultMaxStationCount` (100). The value is clamped between `MinimumStationCap` (2) and the catalog size. If the cap is below `InitialStationCount`, the first wave is only that many cities.

## Line editor

`LineEditor` (`input/line_editor.h`) is SFML-free. `Game` translates clicks into `OnStationClicked`, `Confirm`, `Cancel`, `SelectLine`, `DeleteSelectedLine`, `UndoDraft`, `RedoDraft`, `AddTrainToSelectedLine`.

Drafting:

- First click starts a draft.
- Further clicks append stations (no duplicates except closing a loop).
- Clicking the first station again with at least three unique stations confirms a loop.
- Enter or right-click confirms an open line (at least two stations).
- Clicking the terminal of the **selected** finished line starts an extension draft.
- Ctrl+Z removes the last drafted station; Ctrl+Y puts it back. A new click clears the redo stack. Confirm and cancel also clear it.

`World::AddLine` / `ExtendLine` do the mutation. The last confirmed line stays selected.

## Input wiring in Game

| Action | Handling |
| --- | --- |
| Left-click station | Inspect it; `LineEditor::OnStationClicked` |
| Left-click train | Inspect it; select its line |
| Left-click line (no drag) | Select the line; inspect trains, occupancy, and destinations |
| Drag line onto a station | `InsertStationOnLine` on that segment |
| Drag train token onto a line | `AddTrainToLineAt` at the cursor |
| Ctrl+Z / Ctrl+Y | Undo or redo the last draft station |
| Delete | Delete selected line, or the inspected train’s line; cancels an in-progress line drag |
| T | Extra train on the selected line |
| Space | Pause |
| 1 / 2 / 4 / 8 | Time scale |
| Wheel | Zoom at cursor, or scroll the unconnected list |
| Middle-drag | Pan |
| Click empty map | Clear inspector and help |

Unconnected stations are listed in the right sidebar. Clicking a name inspects that station.

## Station cap and performance

The catalog can contain hundreds of cities. The menu cap limits how many become playable stations (minimum `MinimumStationCap`). That is the main performance lever; pathfinding and boarding scan passengers and trains each tick.
