#!/usr/bin/env python3
"""Climate and biome classification for procedural material selection."""

from typing import Any


def classify_biome(lat: float, _lon: float, avg_elevation: float = 0.0) -> dict[str, Any]:
    """Classify biome based on latitude and elevation.

    Uses simplified Köppen-like classification:
    - Tropical (lat < 23.5)
    - Subtropical (23.5 <= lat < 35)
    - Temperate (35 <= lat < 50)
    - Boreal / Cold (50 <= lat < 66)
    - Polar (lat >= 66)

    Elevation modifiers:
    - > 1500m: mountain / alpine
    - > 2500m: snowcap
    """
    abs_lat = abs(lat)

    base = "temperate"
    if abs_lat < 23.5:
        base = "tropical"
    elif abs_lat < 35:
        base = "subtropical"
    elif abs_lat < 50:
        base = "temperate"
    elif abs_lat < 66:
        base = "boreal"
    else:
        base = "polar"

    elevation_tag = "lowland"
    if avg_elevation > 2500:
        elevation_tag = "snowcap"
    elif avg_elevation > 1500:
        elevation_tag = "alpine"
    elif avg_elevation > 500:
        elevation_tag = "highland"

    biome = f"{base}_{elevation_tag}"

    material_sets = {
        "tropical_lowland": {"ground": "tropical_grass", "tree": "palm", "building": "stucco", "sky": "tropical_blue"},
        "tropical_highland": {"ground": "jungle_dirt", "tree": "broadleaf", "building": "concrete", "sky": "tropical_blue"},
        "subtropical_lowland": {"ground": "savanna_grass", "tree": "oak", "building": "brick", "sky": "clear_blue"},
        "temperate_lowland": {"ground": "grass", "tree": "oak", "building": "brick", "sky": "clear_blue"},
        "temperate_highland": {"ground": "pine_needle", "tree": "pine", "building": "wood", "sky": "overcast"},
        "boreal_lowland": {"ground": "tundra_grass", "tree": "spruce", "building": "log_cabin", "sky": "grey"},
        "boreal_alpine": {"ground": "rocky_tundra", "tree": "scrub", "building": "concrete", "sky": "grey"},
        "polar_lowland": {"ground": "snow", "tree": "none", "building": "metal", "sky": "pale_blue"},
        "polar_snowcap": {"ground": "ice", "tree": "none", "building": "metal", "sky": "pale_blue"},
    }

    materials = material_sets.get(biome, material_sets["temperate_lowland"])

    return {
        "biome": biome,
        "base": base,
        "elevation_tag": elevation_tag,
        "materials": materials,
        "temperature_c": _estimate_temperature(abs_lat, avg_elevation),
        "precipitation": _estimate_precipitation(abs_lat),
    }


def _estimate_temperature(abs_lat: float, elevation: float) -> float:
    """Estimate average annual temperature in Celsius."""
    base_temp = 30 - abs_lat * 0.6
    elev_penalty = elevation / 1000 * 6.5
    return round(base_temp - elev_penalty, 1)


def _estimate_precipitation(abs_lat: float) -> str:
    """Rough precipitation classification."""
    if abs_lat < 10:
        return "heavy"
    elif abs_lat < 25:
        return "moderate"
    elif abs_lat < 40:
        return "variable"
    elif abs_lat < 60:
        return "moderate"
    else:
        return "low"
