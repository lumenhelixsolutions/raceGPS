#!/usr/bin/env python3
"""One-off: fetch water-only OSM for the Cleveland bbox and run extract_water.

Does NOT touch the cached cleveland_5.0km_raw.osm. Writes the water-only
extract next to it so a later recompile can merge/reuse it.
"""

import sys
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "universal-city-compiler"))

from water_extractor import extract_water

BOUNDS = {"south": 41.377114486486484, "north": 41.617949513513516,
          "west": -81.89256836762486, "east": -81.51926933237513}
OUT = ROOT / "citypacks" / "cleveland_5.0km" / "cleveland_5.0km_water_raw.osm"

s, w, n, e = BOUNDS["south"], BOUNDS["west"], BOUNDS["north"], BOUNDS["east"]
query = f"""
[out:xml][timeout:300];
(
  way["natural"="water"]({s},{w},{n},{e});
  node(w);
  way["waterway"~"^(river|stream|canal|riverbank)$"]({s},{w},{n},{e});
  node(w);
  way["natural"="coastline"]({s},{w},{n},{e});
  node(w);
  relation["natural"="water"]({s},{w},{n},{e});
  way(r);
  node(r);
  relation["waterway"="riverbank"]({s},{w},{n},{e});
  way(r);
  node(r);
  relation["water"]({s},{w},{n},{e});
  way(r);
  node(r);
);
out body;
>;
out skel qt;
"""

data = urllib.parse.urlencode({"data": query}).encode("utf-8")
req = urllib.request.Request("https://overpass-api.de/api/interpreter", data=data, method="POST")
req.add_header("User-Agent", "raceGPS-Universal-Compiler/1.0")
with urllib.request.urlopen(req, timeout=300) as resp:
    xml = resp.read().decode("utf-8")

OUT.write_text(xml, encoding="utf-8")
print(f"saved {OUT} ({len(xml)} bytes)")

water = extract_water(OUT)
print(f"rivers (polylines):   {water['river_count']}")
print(f"river_polygons:       {water['river_polygon_count']}")
print(f"lakes (polygons):     {water['lake_count']}")
print(f"coastlines:           {water['coastline_count']}")
print(f"total water features: {water['water_count']}")
for r in water["rivers"][:5]:
    print(f"  river: {r['name']!r} type={r['type']} pts={len(r['points'])}")
for p in water["river_polygons"][:5]:
    print(f"  river_polygon: {p['name']!r} src={p['source']} pts={len(p['points'])} area={p['area_approx_m2']}")
for l in water["lakes"][:5]:
    print(f"  lake: {l['name']!r} type={l['type']} pts={len(l['points'])} area={l['area_approx_m2']}")
