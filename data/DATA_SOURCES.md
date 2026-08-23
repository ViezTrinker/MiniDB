# Data sources

MiniDB loads only static files at runtime. `tools/generate_station_data.py` rebuilds them.

## `data/stations.json`

Cities in Germany with at least 10,000 inhabitants, matched to a nearby railway station.

- **Populated places:** [GeoNames](https://www.geonames.org/) dump `DE.zip` (`https://download.geonames.org/export/dump/DE.zip`). Feature codes `PPLC`, `PPLA`, `PPLA2`, `PPLA3`, `PPLA4`, `PPL` with population >= 10,000. License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **Railway stations:** same GeoNames dump, feature codes `RSTN` and `RSTP`.
- **Matching:** for each city (largest population first), pick the best unused station within 15 km. Stations whose name contains the city are preferred over a nearby unrelated Hauptbahnhof. Each station coordinate/name may only be assigned once, so cities no longer stack on the same map point (for example Marl no longer shares Recklinghausen Hbf). Cities without an unused station are omitted.
- **Suburb filter:** a smaller place within 15 km of a much larger city (population below 30% of the larger city) is dropped so Berlin/Hamburg districts do not appear as separate cities.

This is a stand-in for Destatis GV-ISys municipality lists plus DB StaDa station coordinates. Those official tables can replace GeoNames later without changing the JSON schema.

Passenger destinations are **not** taken from ticket statistics. Deutsche Bahn does not publish origin-destination matrices as open data. The game uses a gravity / Huff model:

`P(destination j | origin i) = pop(j)^α / (distance(i,j) + d0)^γ`

normalized over currently spawned cities. Later this can be calibrated with Destatis / Bundesagentur für Arbeit commuting flows (`Pendlerrechnung`, Regionaldatenbank table 19321).

## `data/germany.geojson`

Germany outline from [isellsoap/deutschlandGeoJSON](https://github.com/isellsoap/deutschlandGeoJSON) (`1_deutschland/3_mittel.geo.json`), derived from BKG administrative boundaries. See that repository's license notes. Natural Earth public-domain country polygons are an alternative.

Station coordinates are projected onto this silhouette; rivers and the real railway geometry are intentionally omitted.
