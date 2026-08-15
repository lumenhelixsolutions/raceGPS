#!/usr/bin/env python3
"""
Headless daytime lighting rig for baked city maps (hotfix: black viewport).

The level-spec importer only rotates an EXISTING DirectionalLight; headless-
created maps have no sun/sky at all, so Lumen renders black. This script adds
the full rig, idempotently (existing actors are left alone):

  - DirectionalLight  (sun; Movable, casts shadows, drives SkyAtmosphere)
  - SkyAtmosphere
  - SkyLight          (Movable, real-time capture -> no lightmass build needed)
  - ExponentialHeightFog

Also sets the map's World Settings GameMode override to CruiseSprintGameMode
when accessible (belt-and-braces; DefaultEngine.ini already sets it globally).

Usage (commandlet):
    set RACEGPS_RIG_MAP=Cleveland5_0KmWorld
    UnrealEditor-Cmd.exe "<uproject>" -run=pythonscript ^
        -script="tools/ue5-headless-lighting-rig.py" -unattended -nop4 -nullrhi

Importable: ensure_lighting_rig(level_subsys) operates on the CURRENT level
and is reused by tools/ue5-headless-city-import.py.
"""

import os
from pathlib import Path

import unreal

SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent

SUN_ROTATION = (-50.0, 135.0, 0.0)   # pitch, yaw, roll — matches spec default
SUN_INTENSITY_LUX = 75000.0
GAME_MODE_CLASS_PATH = "/Script/raceGPSAkronBeta.CruiseSprintGameMode"


def log(msg):
    unreal.log_warning(f"[lighting-rig] {msg}")


def _set(obj, prop, value):
    """set_editor_property with a soft failure (API names shift between versions)."""
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as e:
        log(f"  warn: could not set {prop}={value}: {type(e).__name__}: {e}")
        return False


def _find_by_class(class_name):
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if a.get_class().get_name() == class_name:
            return a
    return None


def ensure_lighting_rig(level_subsys=None):
    """Idempotently add the daytime lighting rig to the current level.

    Returns a dict of {actor_kind: "added"|"existing"}.
    """
    result = {}

    # --- Sun (DirectionalLight) ---
    sun = _find_by_class("DirectionalLight")
    if sun is None:
        sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.DirectionalLight, unreal.Vector(0, 0, 100000))
        if sun:
            sun.set_actor_label("Sun")
            result["sun"] = "added"
    else:
        result["sun"] = "existing"
    if sun:
        # NOTE: unreal.Rotator positional args are (roll, pitch, yaw) — the
        # Python binding orders struct fields alphabetically. Positional
        # (pitch, yaw, roll) here made the sun point UP (pitch=135) -> black
        # city. Use keyword args.
        sun.set_actor_rotation(
            unreal.Rotator(roll=SUN_ROTATION[2], pitch=SUN_ROTATION[0], yaw=SUN_ROTATION[1]), False)
        light_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
        if light_comp:
            _set(light_comp, "mobility", unreal.ComponentMobility.MOVABLE)
            _set(light_comp, "intensity", SUN_INTENSITY_LUX)
            _set(light_comp, "cast_shadows", True)
            _set(light_comp, "atmosphere_sun_light", True)  # drives SkyAtmosphere

    # --- SkyAtmosphere ---
    sky = _find_by_class("SkyAtmosphere")
    if sky is None:
        sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
        if sky:
            sky.set_actor_label("SkyAtmosphere")
            result["sky_atmosphere"] = "added"
    else:
        result["sky_atmosphere"] = "existing"

    # --- SkyLight (real-time capture: no lightmass build required) ---
    skylight = _find_by_class("SkyLight")
    if skylight is None:
        skylight = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyLight, unreal.Vector(0, 0, 0))
        if skylight:
            skylight.set_actor_label("SkyLight")
            result["sky_light"] = "added"
    else:
        result["sky_light"] = "existing"
    if skylight:
        sl_comp = skylight.get_component_by_class(unreal.SkyLightComponent)
        if sl_comp:
            _set(sl_comp, "mobility", unreal.ComponentMobility.MOVABLE)
            _set(sl_comp, "real_time_capture", True)

    # --- ExponentialHeightFog ---
    fog = _find_by_class("ExponentialHeightFog")
    if fog is None:
        fog = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
        if fog:
            fog.set_actor_label("HeightFog")
            result["height_fog"] = "added"
    else:
        result["height_fog"] = "existing"

    # --- Belt-and-braces: per-map GameMode override (one-liner, best effort) ---
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        ws = world.get_world_settings() if hasattr(world, "get_world_settings") else None
        if ws is None:
            # alternate access path
            ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
        gm_class = unreal.load_class(None, GAME_MODE_CLASS_PATH)
        if ws and gm_class:
            _set(ws, "default_game_mode", gm_class)
            result["game_mode_override"] = "set"
        else:
            result["game_mode_override"] = "skipped (no access)"
    except Exception as e:
        log(f"  game mode override skipped: {type(e).__name__}: {e}")
        result["game_mode_override"] = "skipped"

    log("rig: " + ", ".join(f"{k}={v}" for k, v in result.items()))
    return result


def main() -> int:
    map_name = os.environ.get("RACEGPS_RIG_MAP", "Cleveland5_0KmWorld")
    if "/Game/" not in map_name:
        map_name = "/Game/Maps/" + map_name.rsplit("/", 1)[-1]
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsys.load_level(map_name):
        log(f"ERROR: cannot load {map_name}")
        return 1
    ensure_lighting_rig(level_subsys)
    if not level_subsys.save_current_level():
        log(f"ERROR: save failed for {map_name}")
        return 1
    log(f"saved {map_name}")
    return 0


if __name__ == "__main__":
    rc = main()
    log(f"exit code: {rc}")
    if rc != 0:
        raise SystemExit(rc)
