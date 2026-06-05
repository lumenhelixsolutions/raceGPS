#!/usr/bin/env python3
"""
Semantic Map Server — serves compiled city data for the raceGPS Unreal beta.
"""

import json
from pathlib import Path
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="raceGPS Semantic Map Server", version="0.1.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CITYPACK_DIR = PROJECT_ROOT / "citypacks" / "akron-oh-beta-001"


def _load(name: str):
    path = CITYPACK_DIR / name
    if not path.exists():
        raise HTTPException(status_code=404, detail=f"{name} not found")
    with open(path) as f:
        return json.load(f)


@app.get("/")
def root():
    return {"service": "raceGPS Semantic Map Server", "version": "0.1.0"}


@app.get("/city/akron")
def get_city():
    return _load("akron_semantic_manifest.json")


@app.get("/routes")
def get_routes():
    return _load("akron_routes.json")


@app.get("/routes/{route_id}")
def get_route(route_id: str):
    routes = _load("akron_routes.json")
    for r in routes:
        if r["route_id"] == route_id:
            return r
    raise HTTPException(status_code=404, detail="Route not found")


@app.get("/spawn-points")
def get_spawn_points():
    return _load("akron_spawn_points.json")


@app.get("/road-graph")
def get_road_graph():
    return _load("akron_road_graph.json")


@app.get("/pois")
def get_pois():
    return _load("akron_pois.json")


@app.get("/gameplay-layer")
def get_gameplay_layer():
    return _load("akron_gameplay_layer.json")


@app.get("/checkpoints/{route_id}")
def get_checkpoints(route_id: str):
    routes = _load("akron_routes.json")
    for r in routes:
        if r["route_id"] == route_id:
            return r.get("checkpoints", [])
    raise HTTPException(status_code=404, detail="Route not found")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8788)
