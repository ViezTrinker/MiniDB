#!/usr/bin/env python3
"""Generate data/stations.json from GeoNames populated places and railway stations."""

from __future__ import annotations

import io
import json
import math
import os
import urllib.request
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CACHE_DIR = os.path.join(os.path.dirname(__file__), "_download_cache")
OUTPUT_PATH = os.path.join(ROOT, "data", "stations.json")

GEONAMES_DE_URL = "https://download.geonames.org/export/dump/DE.zip"
GERMANY_GEOJSON_URL = (
    "https://raw.githubusercontent.com/isellsoap/deutschlandGeoJSON/"
    "main/1_deutschland/3_mittel.geo.json"
)
GERMANY_GEOJSON_PATH = os.path.join(ROOT, "data", "germany.geojson")

MIN_POPULATION = 10000
MAX_STATION_DISTANCE_KM = 15.0
SUBURB_DISTANCE_KM = 15.0
SUBURB_POPULATION_RATIO = 0.30

CITY_FEATURE_CODES = {"PPLC", "PPLA", "PPLA2", "PPLA3", "PPLA4", "PPL"}
STATION_FEATURE_CODES = {"RSTN", "RSTP"}


def haversine_km(lat_left: float, lon_left: float, lat_right: float, lon_right: float) -> float:
    earth_radius_km = 6371.0
    delta_lat = math.radians(lat_right - lat_left)
    delta_lon = math.radians(lon_right - lon_left)
    latitude_left = math.radians(lat_left)
    latitude_right = math.radians(lat_right)
    arc = (
        math.sin(delta_lat / 2.0) ** 2
        + math.cos(latitude_left) * math.cos(latitude_right) * math.sin(delta_lon / 2.0) ** 2
    )
    return 2.0 * earth_radius_km * math.asin(min(1.0, math.sqrt(arc)))


def normalize_name(name: str) -> str:
    lowered = name.casefold()
    replacements = (
        ("ä", "ae"),
        ("ö", "oe"),
        ("ü", "ue"),
        ("ß", "ss"),
        ("á", "a"),
        ("à", "a"),
        ("é", "e"),
        ("(", " "),
        (")", " "),
        ("-", " "),
        ("/", " "),
        (".", " "),
    )
    for source, target in replacements:
        lowered = lowered.replace(source, target)
    return " ".join(lowered.split())


def download(url: str, destination: str) -> None:
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    if os.path.isfile(destination) and os.path.getsize(destination) > 0:
        print(f"Using cached {destination}")
        return
    print(f"Downloading {url}")
    request = urllib.request.Request(url, headers={"User-Agent": "MiniDB-data-generator/1.0"})
    with urllib.request.urlopen(request, timeout=120) as response:
        payload = response.read()
    with open(destination, "wb") as file_handle:
        file_handle.write(payload)


def parse_geonames_de(zip_path: str) -> tuple[list[dict], list[dict]]:
    cities: list[dict] = []
    stations: list[dict] = []
    with zipfile.ZipFile(zip_path) as archive:
        with archive.open("DE.txt") as raw_file:
            text_file = io.TextIOWrapper(raw_file, encoding="utf-8")
            for line in text_file:
                fields = line.rstrip("\n").split("\t")
                if len(fields) < 15:
                    continue
                name = fields[1]
                ascii_name = fields[2]
                latitude = float(fields[4])
                longitude = float(fields[5])
                feature_class = fields[6]
                feature_code = fields[7]
                population = int(fields[14])

                if feature_class == "P" and feature_code in CITY_FEATURE_CODES and population >= MIN_POPULATION:
                    cities.append(
                        {
                            "name": name,
                            "asciiName": ascii_name,
                            "latitude": latitude,
                            "longitude": longitude,
                            "population": population,
                        }
                    )
                elif feature_class == "S" and feature_code in STATION_FEATURE_CODES:
                    stations.append(
                        {
                            "name": name,
                            "asciiName": ascii_name,
                            "latitude": latitude,
                            "longitude": longitude,
                        }
                    )
    return cities, stations


def drop_suburbs(cities: list[dict]) -> list[dict]:
    ordered = sorted(cities, key=lambda city: city["population"], reverse=True)
    kept: list[dict] = []
    for city in ordered:
        is_suburb = False
        for larger in kept:
            if city["population"] >= larger["population"] * SUBURB_POPULATION_RATIO:
                continue
            distance = haversine_km(
                city["latitude"],
                city["longitude"],
                larger["latitude"],
                larger["longitude"],
            )
            if distance <= SUBURB_DISTANCE_KM:
                is_suburb = True
                break
        if not is_suburb:
            kept.append(city)
    return kept


def station_score(city: dict, station: dict, distance_km: float) -> float:
    city_norm = normalize_name(city["name"])
    station_norm = normalize_name(station["name"])
    ascii_city = normalize_name(city["asciiName"])
    score = distance_km
    has_city_name = city_norm in station_norm or ascii_city in station_norm
    is_hbf = ("hauptbahnhof" in station_norm) or ("hbf" in station_norm.split())
    if has_city_name and is_hbf:
        score -= 80.0
    elif is_hbf:
        score -= 25.0
    elif has_city_name:
        score -= 15.0
    return score


def match_station(city: dict, stations: list[dict]) -> dict | None:
    best_station = None
    best_score = None
    for station in stations:
        distance = haversine_km(
            city["latitude"],
            city["longitude"],
            station["latitude"],
            station["longitude"],
        )
        if distance > MAX_STATION_DISTANCE_KM:
            continue
        score = station_score(city, station, distance)
        if best_score is None or score < best_score:
            best_score = score
            best_station = station
    return best_station


def build_records(cities: list[dict], stations: list[dict]) -> list[dict]:
    records: list[dict] = []
    skipped_without_station = 0
    for city in cities:
        station = match_station(city, stations)
        if station is None:
            skipped_without_station += 1
            continue
        records.append(
            {
                "cityName": city["name"],
                "stationName": station["name"],
                "latitude": round(station["latitude"], 6),
                "longitude": round(station["longitude"], 6),
                "population": city["population"],
            }
        )
    records.sort(key=lambda record: record["population"], reverse=True)
    for index, record in enumerate(records):
        record["id"] = index
    print(f"Matched {len(records)} cities, skipped {skipped_without_station} without a station")
    return records


def main() -> None:
    os.makedirs(os.path.join(ROOT, "data"), exist_ok=True)
    os.makedirs(CACHE_DIR, exist_ok=True)

    geojson_cache = os.path.join(CACHE_DIR, "germany.geojson")
    download(GERMANY_GEOJSON_URL, geojson_cache)
    with open(geojson_cache, "r", encoding="utf-8") as source:
        geojsonText = source.read()
    if len(geojsonText) == 0:
        raise RuntimeError("germany.geojson is empty")
    with open(GERMANY_GEOJSON_PATH, "w", encoding="utf-8", newline="\n") as target:
        target.write(geojsonText)

    zip_path = os.path.join(CACHE_DIR, "DE.zip")
    download(GEONAMES_DE_URL, zip_path)
    cities, stations = parse_geonames_de(zip_path)
    print(f"Loaded {len(cities)} cities (>= {MIN_POPULATION}) and {len(stations)} railway stops")
    cities = drop_suburbs(cities)
    print(f"Kept {len(cities)} cities after suburb filtering")
    records = build_records(cities, stations)
    if len(records) < 50:
        raise RuntimeError("Too few matched stations; matching likely failed")

    document = {"stations": records}
    with open(OUTPUT_PATH, "w", encoding="utf-8", newline="\n") as file_handle:
        json.dump(document, file_handle, ensure_ascii=False, indent=2)
        file_handle.write("\n")
    print(f"Wrote {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
