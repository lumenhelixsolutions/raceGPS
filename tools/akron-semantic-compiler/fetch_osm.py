#!/usr/bin/env python3
"""Fetch OSM data for a bounding box from the Overpass API."""

import time
import urllib.request
import urllib.error

OVERPASS_URL = "https://overpass-api.de/api/interpreter"


def fetch_osm_for_bounds(west: float, south: float, east: float, north: float) -> str:
    """Download OSM XML for the given bounds."""
    query = f"""
[out:xml][timeout:120];
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|residential|unclassified|service)$"]({south},{west},{north},{east});
  node(w);
  way["building"]({south},{west},{north},{east});
  node(w);
  relation["building"]({south},{west},{north},{east});
  way(r);
  node(r);
);
out body;
>;
out skel qt;
"""
    data = urllib.parse.urlencode({"data": query}).encode("utf-8")
    req = urllib.request.Request(OVERPASS_URL, data=data, method="POST")
    req.add_header("User-Agent", "raceGPS-Semantic-Compiler/0.1")

    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=180) as resp:
                return resp.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            if e.code == 429:
                wait = 5 * (attempt + 1)
                print(f"      Overpass rate limited. Waiting {wait}s...")
                time.sleep(wait)
            else:
                raise
    raise RuntimeError("Overpass API failed after 3 retries")
