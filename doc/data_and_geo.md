# Data and geography

Runtime data is static files next to the executable (`data/`, copied on build). Regeneration is `python tools/generate_station_data.py`. Sources and licenses: [data/DATA_SOURCES.md](../data/DATA_SOURCES.md).

## Station catalog

`LoadStationCatalogFromFile` / `FromString` (`src/data/station_catalog.h`) parse `stations.json`.

Each entry needs `id`, `cityName`, `stationName`, `latitude`, `longitude`, `population`. Cities below `MinimumCityPopulation` (10 000) are dropped. The list is sorted by descending population. Coordinates are projected immediately onto `StationRecord::position`.

`cityName` / `stationName` stay UTF-8 in `std::string`. Do not reinterpret them as the Windows ANSI code page.

One city, one station. Districts of large cities are filtered out in the generator, not at runtime.

## Projection

`ProjectWgs84` (`src/geo/projection.h`) maps WGS84 into schematic kilometres.

- X grows east from `GermanyLonMin`.
- Y is **0 at the north** (`GermanyLatMax`) and grows south, so SFML’s y-down view matches the silhouette.
- Longitude is scaled by `cos(51°)` (`ProjectionReferenceLatitudeDegrees`).

`MapWidthKm` / `MapHeightKm` are the projected size of the Germany bounding box. Hit tests and train motion use these kilometres, not pixels.

## Outline

`LoadGeoJsonOutline` (`src/geo/geojson_outline.h`) reads a GeoJSON polygon/multipolygon and projects each ring. Rivers and the real railway network are not loaded.

## Schema (stations.json)

```json
{
  "stations": [
    {
      "id": 3,
      "cityName": "Köln",
      "stationName": "Köln Hauptbahnhof",
      "latitude": 50.9429,
      "longitude": 6.95801,
      "population": 1024621
    }
  ]
}
```

Ids in the file are stable catalog ids. After spawn they are the same `StationId` values the network uses.
