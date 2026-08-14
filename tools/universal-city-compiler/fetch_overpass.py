#!/usr/bin/env python3
"""Enhanced Overpass API fetcher with support for roads, buildings, water, landuse, and natural features.

Large bounding boxes are automatically split into a grid of tile sub-bboxes
(see ``TILE_THRESHOLD_DEG``) to avoid Overpass timeouts and memory blowups.
Tiles are fetched sequentially with the same 3-retry 429 backoff as before and
merged with dedupe by OSM element id.

Boundary-way completeness: every tile query recurses with ``node(w)`` /
``way(r)`` / ``node(r)``, so Overpass returns the FULL node list for any way
that touches the tile bbox — including nodes outside the tile. Ways that span
a tile boundary therefore arrive complete in every tile they touch, and
first-seen-wins dedupe by (tag, id) never truncates geometry.
"""

import math
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

OVERPASS_URL = "https://overpass-api.de/api/interpreter"

# Bboxes wider or taller than this (degrees) are split into a tile grid.
TILE_THRESHOLD_DEG = 0.25

# Chunk size for streaming response bodies to a temp file before parsing.
_CHUNK_SIZE = 256 * 1024


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


def _post_query(query: str) -> str:
    """POST a query to Overpass with the standard 3-retry 429 backoff.

    The response body is streamed to a temp file in fixed-size chunks before
    being decoded, so peak memory during transfer is capped by the chunk size
    rather than the full extract size.
    """
    data = urllib.parse.urlencode({"data": query}).encode("utf-8")
    req = urllib.request.Request(OVERPASS_URL, data=data, method="POST")
    req.add_header("User-Agent", "raceGPS-Universal-Compiler/1.0")

    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=300) as resp:
                with tempfile.TemporaryFile(mode="w+b") as tmp:
                    while True:
                        chunk = resp.read(_CHUNK_SIZE)
                        if not chunk:
                            break
                        tmp.write(chunk)
                    tmp.seek(0)
                    return tmp.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            if e.code == 429:
                wait = 5 * (attempt + 1)
                print(f"      Overpass rate limited. Waiting {wait}s...")
                time.sleep(wait)
            else:
                raise
    raise RuntimeError("Overpass API failed after 3 retries")


def fetch_osm_for_bounds(west: float, south: float, east: float, north: float, detail: str = "standard") -> str:
    """Download OSM XML for the given bounds with configurable detail level.

    This is the single-request path; small bboxes behave exactly as before.
    Use ``fetch_osm_tiled`` (or ``fetch_and_cache``, which auto-selects) for
    large extracts.
    """
    query = _build_query(west, south, east, north, detail)
    return _post_query(query)


def needs_tiling(west: float, south: float, east: float, north: float,
                 tile_deg: float = TILE_THRESHOLD_DEG) -> bool:
    """True when the bbox is large enough to warrant tiled fetching."""
    return (east - west) > tile_deg or (north - south) > tile_deg


def _tile_grid(west: float, south: float, east: float, north: float,
               tile_deg: float = TILE_THRESHOLD_DEG) -> list:
    """Split a bbox into a grid of sub-bboxes of at most ``tile_deg`` per side.

    Tiles exactly cover the original bbox (edges are subdivided evenly, so the
    last tile is never a ragged remainder). Overpass bbox filters are
    inclusive, so elements on shared edges appear in both adjacent tiles and
    are deduped at merge time.
    """
    span_x = east - west
    span_y = north - south
    nx = max(1, math.ceil(span_x / tile_deg))
    ny = max(1, math.ceil(span_y / tile_deg))
    dx = span_x / nx
    dy = span_y / ny
    tiles = []
    for i in range(nx):
        for j in range(ny):
            tiles.append((west + i * dx, south + j * dy,
                          west + (i + 1) * dx, south + (j + 1) * dy))
    return tiles


def merge_osm_xml(xml_docs: list) -> str:
    """Merge OSM XML documents, deduping elements by (tag, id).

    First occurrence wins. Because each tile query recurses with ``node(w)``,
    every copy of a boundary-crossing way carries its full node list, so
    keeping the first copy never loses geometry. Non-element children of the
    root (e.g. ``<note>``, ``<meta>``) are taken from the first document.
    """
    merged_root = None
    seen = set()
    for doc in xml_docs:
        root = ET.fromstring(doc)
        if merged_root is None:
            merged_root = ET.Element("osm", root.attrib)
            for child in root:
                if child.tag not in ("node", "way", "relation"):
                    merged_root.append(child)
        for el in root:
            if el.tag not in ("node", "way", "relation"):
                continue
            key = (el.tag, el.get("id"))
            if key in seen:
                continue
            seen.add(key)
            merged_root.append(el)
    if merged_root is None:
        merged_root = ET.Element("osm", {"version": "0.6", "generator": "Overpass API"})
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(merged_root, encoding="unicode")


def fetch_osm_tiled(west: float, south: float, east: float, north: float,
                    detail: str = "standard", tile_deg: float = TILE_THRESHOLD_DEG) -> str:
    """Fetch a large bbox as a grid of tiles and merge the results.

    Tiles are fetched sequentially through ``fetch_osm_for_bounds``, so each
    tile gets the standard 3-retry 429 backoff. Results are merged with
    dedupe by OSM element id (see ``merge_osm_xml``).
    """
    tiles = _tile_grid(west, south, east, north, tile_deg)
    print(f"      Large bbox ({east - west:.3f}x{north - south:.3f} deg); "
          f"fetching {len(tiles)} tiles...")
    docs = []
    for idx, (w, s, e, n) in enumerate(tiles, 1):
        print(f"      Tile {idx}/{len(tiles)}: ({s:.4f},{w:.4f})-({n:.4f},{e:.4f})")
        docs.append(fetch_osm_for_bounds(w, s, e, n, detail))
    return merge_osm_xml(docs)


def fetch_and_cache(bounds: dict, cache_path: Path, detail: str = "standard",
                    tile_deg: float = TILE_THRESHOLD_DEG) -> str:
    """Fetch OSM data and cache to disk. Returns XML string.

    Small bboxes take the exact same single-request path as before. Bboxes
    exceeding ``tile_deg`` on either axis are fetched as a tile grid and the
    merged document is cached under the same ``cache_path``.
    """
    if cache_path.exists():
        print(f"      Using cached OSM: {cache_path}")
        return cache_path.read_text(encoding="utf-8")

    print(f"      Fetching OSM ({detail})...")
    west, south = bounds["west"], bounds["south"]
    east, north = bounds["east"], bounds["north"]
    if needs_tiling(west, south, east, north, tile_deg):
        xml = fetch_osm_tiled(west, south, east, north, detail, tile_deg)
    else:
        xml = fetch_osm_for_bounds(west, south, east, north, detail)
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text(xml, encoding="utf-8")
    print(f"      Saved: {cache_path}")
    return xml
