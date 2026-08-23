# Rendering

`Renderer` (`src/rendering/renderer.h`) draws the map, network, trains, HUD, help, and the right-hand sidebar. It does not own simulation state.

## Views

Two SFML views:

- **Map view** — camera in map kilometres (same space as `ProjectWgs84`). Zoom and pan change this view.
- **Default / HUD view** — pixel space for menu, HUD, help, train token, and sidebar.

`SyncWindowViews` runs every frame so a fullscreen resize does not leave the HUD stuck at `1280x720`.

Station markers and labels are drawn in map space but scaled by kilometres-per-pixel so they stay a constant size on screen.

## What is drawn

- Germany outline from `germany.geojson`.
- Finished lines (thicker when selected). Segments shared by several lines are drawn as parallel colored strokes; hit-tests still use the true station-to-station geometry. Draft polyline and insert-drag preview stay on the true segment.
- Stations (radius from population). Names when there are few stations, the city is large, or it is inspected.
- Waiting counts next to stations.
- Trains as short rectangles along their segment.
- HUD: time, speed, pause, station count / cap.
- Help button (`?`) and popup.
- Train token (bottom left) for dropping a new train.
- Sidebar (play only): inspector (station demand, train onboard / next / transfers, or line trains / occupancy / destinations) and the unconnected list.

`SetMapSidebar` shrinks the map viewport when the sidebar is visible so Germany stays framed.

## UTF-8 text

Catalog names are UTF-8 (`Köln`, `Lübeck`, `München`). SFML 3 `sf::String` constructed from `std::string` is **ANSI / locale**, not UTF-8.

`Utf8SfString` in `src/rendering/utf8_text.h` calls `sf::String::fromUtf8`. Every `sf::Text` in the renderer and main menu goes through that helper.

## Fonts

`Game::LoadFont` tries Windows then Linux system sans fonts. There is no bundled font file.
