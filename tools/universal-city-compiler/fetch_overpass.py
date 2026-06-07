#!/usr/bin/env python3
"""Enhanced Overpass API fetcher with support for roads, buildings, water, landuse, and natural features."""

import time
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path

OVERPASS_URL = "https://overpass-api.de/api/interpreter"


def _build_query(west: float, south: float, east: float, north: float, detail: str = "standard") -> str:
    """Build an Overpass QL query based on detail level."""
    # Base highways always included
    highway_filter = '"highway"~"^(motorway|trunk|primary|secondary|tertiary|residential|unclassified|service|living_street|pedestrian)$"'

    if detail == "minimal":
        return f"""
[out:xml][timeout:120];
(
  way[{highway_filter}]({south},{west},{north},{east});
  node(w);
);
out body;
>;
out skel qt;
"""

    # Standard: roads + buildings + POI nodes
    if detail == "standard":
        return f"""
[out:xml][timeout:180];
(
  way[{highway_filter}]({south},{west},{north},{east});
  node(w);
  way["building"]({south},{west},{north},{east});
  node(w);
  relation["building"]({south},{west},{north},{east});
  way(r);
  node(r);
  node["amenity"]({south},{west},{north},{east});
  node["tourism"]({south},{west},{north},{east});
  node["shop"]({south},{west},{north},{east});
  node["historic"]({south},{west},{north},{east});
  node["leisure"]({south},{west},{north},{east});
);
out body;
>;
out skel qt;
"""

    # Full: everything for procedural generation
    return f"""
[out:xml][timeout:300];
(
  way[{highway_filter}]({south},{west},{north},{east});
  node(w);
  way["building"]({south},{west},{north},{east});
  node(w);
  relation["building"]({south},{west},{north},{east});
  way(r);
  node(r);
  node["amenity"]({south},{west},{north},{east});
  node["tourism"]({south},{west},{north},{east});
  node["shop"]({south},{west},{north},{east});
  node["historic"]({south},{west},{north},{east});
  node["leisure"]({south},{west},{north},{east});
  way["natural"~"^(water|wood|scrub|heath|grassland)$"]({south},{west},{north},{east});
  node(w);
  way["waterway"~"^(river|stream|canal)$"]({south},{west},{north},{east});
  node(w);
  relation["water"]({south},{west},{north},{east});
  way(r);
  node(r);
  way["landuse"~"^(forest|grass|meadow|farmland|residential|commercial|industrial)$"]({south},{west},{north},{east});
  node(w);
);
out body;
>;
out skel qt;
"""


def fetch_osm_for_bounds(west: float, south: float, east: float, north: float, detail: str = "standard") -> str:
    """Download OSM XML for the given bounds with configurable detail level."""
    query = _build_query(west, south, east, north, detail)
    data = urllib.parse.urlencode({"data": query}).encode("utf-8")
    req = urllib.request.Request(OVERPASS_URL, data=data, method="POST")
    req.add_header("User-Agent", "raceGPS-Universal-Compiler/1.0")

    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=300) as resp:
                return resp.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            if e.code == 429:
                wait = 5 * (attempt + 1)
                print(f"      Overpass rate limited. Waiting {wait}s...")
                time.sleep(wait)
            else:
                raise
    raise RuntimeError("Overpass API failed after 3 retries")


def fetch_and_cache(bounds: dict, cache_path: Path, detail: str = "standard") -> str:
    """Fetch OSM data and cache to disk. Returns XML string."""
    if cache_path.exists():
        print(f"      Using cached OSM: {cache_path}")
        return cache_path.read_text(encoding="utf-8")

    print(f"      Fetching OSM ({detail})...")
    xml = fetch_osm_for_bounds(bounds["west"], bounds["south"], bounds["east"], bounds["north"], detail)
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text(xml, encoding="utf-8")
    print(f"      Saved: {cache_path}")
    return xml
