#!/usr/bin/env python3
"""
raceGPS — Citypack Validation Suite
Validates citypack directory structure, OpenDRIVE XML, and route connectivity.
"""

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def validate_citypack_json(filepath: str) -> list:
    errors = []
    try:
        with open(filepath) as f:
            data = json.load(f)
    except Exception as e:
        errors.append(f'Invalid JSON: {e}')
        return errors

    required = ['name', 'version', 'generated_at', 'format', 'statistics', 'roads', 'buildings', 'pois', 'routes']
    for key in required:
        if key not in data:
            errors.append(f'Missing required key: {key}')

    stats = data.get('statistics', {})
    roads = data.get('roads', [])
    buildings = data.get('buildings', [])
    pois = data.get('pois', [])
    routes = data.get('routes', [])

    if len(roads) != stats.get('roads', 0):
        errors.append(f'Road count mismatch: stats says {stats.get("roads", 0)}, found {len(roads)}')
    if len(buildings) != stats.get('buildings', 0):
        errors.append(f'Building count mismatch: stats says {stats.get("buildings", 0)}, found {len(buildings)}')
    if len(pois) != stats.get('pois', 0):
        errors.append(f'POI count mismatch: stats says {stats.get("pois", 0)}, found {len(pois)}')

    for i, road in enumerate(roads):
        if len(road.get('coordinates', [])) < 2:
            errors.append(f'Road {i} ({road.get("id")}) has fewer than 2 coordinates')

    for i, route in enumerate(routes):
        if not route.get('id') or not route.get('name'):
            errors.append(f'Route {i} missing id or name')

    return errors


def validate_xodr(filepath: str) -> list:
    errors = []
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
    except ET.ParseError as e:
        errors.append(f'Invalid XML: {e}')
        return errors
    except Exception as e:
        errors.append(f'Cannot read XODR: {e}')
        return errors

    if root.tag != 'OpenDRIVE':
        errors.append(f'Root tag is "{root.tag}", expected "OpenDRIVE"')

    roads = root.findall('road')
    if len(roads) == 0:
        errors.append('No roads found in XODR')

    for road in roads:
        if 'id' not in road.attrib:
            errors.append('Road missing id attribute')
        if 'length' not in road.attrib:
            errors.append(f'Road {road.attrib.get("id", "?")} missing length attribute')

    return errors


def check_connectivity(roads: list) -> dict:
    """Simple graph connectivity check on road endpoints."""
    from collections import defaultdict
    graph = defaultdict(list)

    for road in roads:
        coords = road.get('coordinates', [])
        if len(coords) < 2:
            continue
        start = tuple(coords[0])
        end = tuple(coords[-1])
        graph[start].append(end)
        graph[end].append(start)

    if not graph:
        return {'connected': False, 'components': 0, 'error': 'No valid road endpoints'}

    # BFS from first node
    nodes = list(graph.keys())
    visited = set()
    stack = [nodes[0]]
    while stack:
        node = stack.pop()
        if node in visited:
            continue
        visited.add(node)
        for neighbor in graph[node]:
            if neighbor not in visited:
                stack.append(neighbor)

    components = 1 if len(visited) == len(nodes) else 2  # Simplified
    return {
        'connected': len(visited) == len(nodes),
        'components': components,
        'nodes': len(nodes),
        'visited': len(visited)
    }


def main():
    parser = argparse.ArgumentParser(description='raceGPS Citypack Validator')
    parser.add_argument('citypack_dir', help='Path to citypack directory')
    args = parser.parse_args()

    if not os.path.isdir(args.citypack_dir):
        print(f'ERROR: Not a directory: {args.citypack_dir}', file=sys.stderr)
        sys.exit(1)

    cp_path = os.path.join(args.citypack_dir, 'citypack.json')
    xodr_path = os.path.join(args.citypack_dir, 'city.xodr')

    all_errors = []

    if os.path.exists(cp_path):
        errors = validate_citypack_json(cp_path)
        all_errors.extend(errors)
    else:
        all_errors.append('citypack.json not found')

    if os.path.exists(xodr_path):
        errors = validate_xodr(xodr_path)
        all_errors.extend(errors)
    else:
        all_errors.append('city.xodr not found')

    # Load citypack for connectivity check
    if os.path.exists(cp_path):
        with open(cp_path) as f:
            data = json.load(f)
        conn = check_connectivity(data.get('roads', []))
        print(f'Connectivity: {conn}')
        if not conn['connected']:
            all_errors.append(f'Road graph not fully connected ({conn["visited"]}/{conn["nodes"]} nodes reachable)')

        stats = data.get('statistics', {})
        print(f'Statistics:')
        print(f'  Roads:      {stats.get("roads", 0)}')
        print(f'  Junctions:  {stats.get("junctions", 0)}')
        print(f'  Buildings:  {stats.get("buildings", 0)}')
        print(f'  POIs:       {stats.get("pois", 0)}')
        print(f'  Routes:     {len(data.get("routes", []))}')

    if all_errors:
        print(f'\nERRORS ({len(all_errors)}):')
        for e in all_errors:
            print(f'  - {e}')
        sys.exit(1)
    else:
        print('\nValidation passed.')
        sys.exit(0)


if __name__ == '__main__':
    main()
