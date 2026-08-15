#!/usr/bin/env python3
"""Tests for tiled/chunked Overpass fetching in fetch_overpass.py.

All HTTP is mocked — no live network access.
"""

import io
import sys
import urllib.error
from pathlib import Path
from unittest import mock

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools" / "universal-city-compiler"))

import fetch_overpass as fo


def _osm_doc(nodes=(), ways=(), relations=(), note=True):
    """Build a minimal OSM XML document string."""
    parts = ['<?xml version="1.0" encoding="UTF-8"?>\n<osm version="0.6">']
    if note:
        parts.append("<note>test data</note>")
    for nid, lat, lon in nodes:
        parts.append(f'<node id="{nid}" lat="{lat}" lon="{lon}"/>')
    for wid, refs in ways:
        nds = "".join(f'<nd ref="{r}"/>' for r in refs)
        parts.append(f'<way id="{wid}">{nds}</way>')
    for rid in relations:
        parts.append(f'<relation id="{rid}"/>')
    parts.append("</osm>")
    return "".join(parts)


class FakeResponse:
    """Context-manager response with chunked read(), like urlopen returns."""

    def __init__(self, body: str):
        self._buf = io.BytesIO(body.encode("utf-8"))

    def read(self, size=-1):
        return self._buf.read(size)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False


def _http_error(code):
    return urllib.error.HTTPError(fo.OVERPASS_URL, code, "err", {}, None)


class TestTileGridMath:
    def test_small_bbox_single_tile(self):
        tiles = fo._tile_grid(-81.6, 41.0, -81.4, 41.2, 0.25)
        assert len(tiles) == 1
        assert tiles[0] == (-81.6, 41.0, -81.4, 41.2)

    def test_exact_cover_no_ragged_remainder(self):
        # 0.6 x 0.3 deg bbox at 0.25 tile size -> 3 x 2 = 6 tiles
        west, south, east, north = -82.0, 41.0, -81.4, 41.3
        tiles = fo._tile_grid(west, south, east, north, 0.25)
        assert len(tiles) == 6
        # Exact coverage: no gaps, no overlaps beyond shared edges
        xs = sorted({t[0] for t in tiles} | {t[2] for t in tiles})
        ys = sorted({t[1] for t in tiles} | {t[3] for t in tiles})
        assert xs[0] == west and xs[-1] == east
        assert ys[0] == south and ys[-1] == north
        for a, b in zip(xs, xs[1:]):
            assert abs((b - a) - 0.2) < 1e-9  # 0.6/3
        for a, b in zip(ys, ys[1:]):
            assert abs((b - a) - 0.15) < 1e-9  # 0.3/2

    def test_every_tile_within_threshold(self):
        tiles = fo._tile_grid(-82.0, 41.0, -80.9, 42.1, 0.25)
        assert len(tiles) == 25  # ceil(1.1/.25)=5 x ceil(1.1/.25)=5
        for w, s, e, n in tiles:
            assert e - w <= 0.25 + 1e-9
            assert n - s <= 0.25 + 1e-9


class TestThreshold:
    def test_below_threshold_no_tiling(self):
        assert not fo.needs_tiling(-81.6, 41.0, -81.4, 41.2)  # 0.2 x 0.2

    def test_exactly_at_threshold_no_tiling(self):
        assert not fo.needs_tiling(-81.5, 41.0, -81.25, 41.25)  # 0.25 x 0.25

    def test_above_threshold_tiles(self):
        assert fo.needs_tiling(-81.6, 41.0, -81.3, 41.1)  # width 0.3
        assert fo.needs_tiling(-81.6, 41.0, -81.5, 41.3)  # height 0.3

    def test_configurable_threshold(self):
        assert fo.needs_tiling(-81.6, 41.0, -81.4, 41.2, tile_deg=0.1)


class TestMerge:
    def test_dedupe_by_element_id(self):
        doc_a = _osm_doc(nodes=[(1, 41.0, -81.5), (2, 41.1, -81.4)],
                         ways=[(100, [1, 2])])
        doc_b = _osm_doc(nodes=[(2, 41.1, -81.4), (3, 41.2, -81.3)],
                         ways=[(100, [1, 2]), (200, [2, 3])])
        merged = fo.merge_osm_xml([doc_a, doc_b])
        assert merged.count('id="1"') == 1
        assert merged.count('id="2"') == 1
        assert merged.count('id="3"') == 1
        assert merged.count('id="100"') == 1
        assert merged.count('id="200"') == 1

    def test_boundary_way_keeps_full_node_list(self):
        # Way 100 crosses the tile boundary; Overpass node(w) recursion means
        # each tile returns it with its complete node list, including the node
        # outside the tile bbox.
        doc_a = _osm_doc(nodes=[(1, 41.0, -81.5), (2, 41.1, -81.4)],
                         ways=[(100, [1, 2, 3])])
        doc_b = _osm_doc(nodes=[(3, 41.2, -81.3)], ways=[(100, [1, 2, 3])])
        merged = fo.merge_osm_xml([doc_a, doc_b])
        assert merged.count('id="100"') == 1
        for ref in ('ref="1"', 'ref="2"', 'ref="3"'):
            assert ref in merged
        # Node 3 (outside tile A) still present from tile B
        assert 'id="3"' in merged

    def test_note_meta_taken_once(self):
        doc = _osm_doc(nodes=[(1, 41.0, -81.5)])
        merged = fo.merge_osm_xml([doc, doc])
        assert merged.count("<note>") == 1

    def test_relations_deduped(self):
        doc_a = _osm_doc(relations=[9001])
        doc_b = _osm_doc(relations=[9001, 9002])
        merged = fo.merge_osm_xml([doc_a, doc_b])
        assert merged.count('id="9001"') == 1
        assert merged.count('id="9002"') == 1


class TestFetchBehavior:
    def test_small_bbox_single_request_unchanged_path(self, tmp_path):
        bounds = {"west": -81.6, "south": 41.0, "east": -81.4, "north": 41.2}
        body = _osm_doc(nodes=[(1, 41.05, -81.5)])
        with mock.patch("urllib.request.urlopen",
                        return_value=FakeResponse(body)) as m:
            xml = fo.fetch_and_cache(bounds, tmp_path / "city.osm", "standard")
        assert m.call_count == 1
        # Same POST contract as before: single query covering the whole bbox
        sent = m.call_args[0][0]
        assert sent.data is not None and b"data=" in sent.data
        assert xml == body

    def test_large_bbox_tiled_and_merged(self, tmp_path):
        bounds = {"west": -82.0, "south": 41.0, "east": -81.4, "north": 41.3}
        bodies = [
            _osm_doc(nodes=[(1, 41.0, -82.0)], ways=[(100, [1, 2])]),
            _osm_doc(nodes=[(2, 41.1, -81.9)], ways=[(100, [1, 2])]),
        ] * 3  # 6 tiles
        with mock.patch("urllib.request.urlopen",
                        side_effect=[FakeResponse(b) for b in bodies]) as m:
            xml = fo.fetch_and_cache(bounds, tmp_path / "big.osm", "full")
        assert m.call_count == 6
        assert xml.count('id="100"') == 1  # boundary way deduped
        assert 'ref="1"' in xml and 'ref="2"' in xml

    def test_429_retry_backoff_preserved(self):
        body = _osm_doc(nodes=[(1, 41.0, -81.5)])
        calls = [_http_error(429), _http_error(429), FakeResponse(body)]
        with mock.patch("urllib.request.urlopen", side_effect=calls) as m, \
             mock.patch("time.sleep") as sleep:
            xml = fo.fetch_osm_for_bounds(-81.6, 41.0, -81.4, 41.2, "minimal")
        assert m.call_count == 3
        assert [c.args[0] for c in sleep.call_args_list] == [5, 10]
        assert xml == body

    def test_429_exhaustion_raises(self):
        with mock.patch("urllib.request.urlopen",
                        side_effect=[_http_error(429)] * 3), \
             mock.patch("time.sleep"):
            with pytest.raises(RuntimeError, match="3 retries"):
                fo.fetch_osm_for_bounds(-81.6, 41.0, -81.4, 41.2)

    def test_non_429_http_error_raises_immediately(self):
        with mock.patch("urllib.request.urlopen",
                        side_effect=[_http_error(500)]) as m:
            with pytest.raises(urllib.error.HTTPError):
                fo.fetch_osm_for_bounds(-81.6, 41.0, -81.4, 41.2)
        assert m.call_count == 1


class TestStreaming:
    def test_chunked_reads_via_temp_file(self):
        # Response larger than one chunk must still be assembled correctly.
        body = "<osm>" + "x" * (fo._CHUNK_SIZE * 2 + 1000) + "</osm>"
        reads = []

        class TrackingResponse(FakeResponse):
            def read(self, size=-1):
                reads.append(size)
                return super().read(size)

        with mock.patch("urllib.request.urlopen",
                        return_value=TrackingResponse(body)):
            xml = fo.fetch_osm_for_bounds(-81.6, 41.0, -81.4, 41.2)
        assert xml == body
        # Multiple chunk-sized reads, never one unbounded read
        assert len(reads) >= 3
        assert all(r == fo._CHUNK_SIZE for r in reads)


class TestCacheInterplay:
    def test_existing_cache_skips_network(self, tmp_path):
        cache = tmp_path / "cached.osm"
        cache.write_text("<osm>cached</osm>", encoding="utf-8")
        bounds = {"west": -82.0, "south": 41.0, "east": -81.4, "north": 41.3}
        with mock.patch("urllib.request.urlopen") as m:
            xml = fo.fetch_and_cache(bounds, cache, "standard")
        m.assert_not_called()
        assert xml == "<osm>cached</osm>"

    def test_tiled_result_written_to_cache_and_reused(self, tmp_path):
        cache = tmp_path / "sub" / "big.osm"
        bounds = {"west": -82.0, "south": 41.0, "east": -81.4, "north": 41.3}
        bodies = [_osm_doc(nodes=[(i, 41.0, -82.0)]) for i in range(6)]
        with mock.patch("urllib.request.urlopen",
                        side_effect=[FakeResponse(b) for b in bodies]) as m:
            xml_first = fo.fetch_and_cache(bounds, cache, "standard")
        assert m.call_count == 6
        assert cache.exists()
        assert cache.read_text(encoding="utf-8") == xml_first

        # Second call must hit the cache, not the network
        with mock.patch("urllib.request.urlopen") as m2:
            xml_second = fo.fetch_and_cache(bounds, cache, "standard")
        m2.assert_not_called()
        assert xml_second == xml_first
