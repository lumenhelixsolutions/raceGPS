#!/usr/bin/env python3
"""
Convert OSM → OpenDRIVE using CARLA's Osm2Odr.
Requires CARLA Python API installed.
"""

from pathlib import Path


def convert_osm_to_xodr(osm_path: Path) -> str:
    """Convert OSM file to OpenDRIVE XML string."""
    try:
        import carla
    except ImportError as e:
        raise ImportError(
            "CARLA Python API not installed. "
            "Install from CARLA/PythonAPI/carla/dist/carla-*.whl"
        ) from e

    settings = carla.Osm2OdrSettings()
    settings.generate_traffic_lights = True
    settings.center_map = True
    settings.use_offsets = False

    xodr_string = carla.Osm2Odr.convert(str(osm_path), settings)
    return xodr_string
