#!/usr/bin/env python3
"""
raceGPS — Batch Citypack Generator
Generates citypack JSON + OpenDRIVE XML from OSM extracts.
"""

import argparse
import json
import os
import sys
import zipfile
from pathlib import Path
from datetime import datetime


def generate_id() -> str:
    import uuid
    return uuid.uuid4().hex[:12]


def parse_osm_xml(filepath: str):
    """Parse .osm XML and extract roads, junctions, buildings, POIs."""
    try:
        import xml.etree.ElementTree as ET
    except ImportError:
        raise RuntimeError("xml.etree.ElementTree not available")

    tree = ET.parse(filepath)
    root = tree.getroot()

    nodes = {}
    ways = []
    relations = []

    for elem in root:
        if elem.tag == 'node':
            nodes[elem.attrib['id']] = {
                'lat': float(elem.attrib.get('lat', 0)),
                'lon': float(elem.attrib.get('lon', 0)),
                'tags': {t.attrib['k']: t.attrib['v'] for t in elem if t.tag == 'tag'}
            }
        elif elem.tag == 'way':
            way = {
                'id': elem.attrib['id'],
                'nodes': [nd.attrib['ref'] for nd in elem if nd.tag == 'nd'],
                'tags': {t.attrib['k']: t.attrib['v'] for t in elem if t.tag == 'tag'}
            }
            ways.append(way)
        elif elem.tag == 'relation':
            rel = {
                'id': elem.attrib['id'],
                'members': [(m.attrib['type'], m.attrib['ref'], m.attrib.get('role', '')) for m in elem if m.tag == 'member'],
                'tags': {t.attrib['k']: t.attrib['v'] for t in elem if t.tag == 'tag'}
            }
            relations.append(rel)

    roads = []
    buildings = []
    pois = []
    junctions = set()

    for way in ways:
        tags = way['tags']
        highway = tags.get('highway', '')
        building = tags.get('building', '')
        name = tags.get('name', '')

        if highway and highway not in ('footway', 'path', 'cycleway', 'steps'):
            coords = []
            for nid in way['nodes']:
                n = nodes.get(nid)
                if n:
                    coords.append([n['lon'], n['lat']])
            if len(coords) >= 2:
                roads.append({
                    'id': way['id'],
                    'name': name or f"Road {len(roads)+1}",
                    'type': highway,
                    'coordinates': coords,
                    'lanes': int(tags.get('lanes', 2)),
                    'oneway': tags.get('oneway', 'no') == 'yes'
                })
                # Junction detection: nodes shared by multiple roads
                for nid in way['nodes']:
                    junctions.add(nid)

        elif building:
            coords = []
            for nid in way['nodes']:
                n = nodes.get(nid)
                if n:
                    coords.append([n['lon'], n['lat']])
            if len(coords) >= 3:
                buildings.append({
                    'id': way['id'],
                    'name': name or f"Building {len(buildings)+1}",
                    'type': building,
                    'coordinates': coords,
                    'levels': int(tags.get('building:levels', 1))
                })

        # POIs from nodes that are part of ways
        amenity = tags.get('amenity', '')
        if amenity:
            pois.append({
                'id': way['id'],
                'name': name or amenity,
                'category': amenity,
                'type': 'way'
            })

    # Node-based POIs
    for nid, n in nodes.items():
        tags = n['tags']
        amenity = tags.get('amenity', '')
        if amenity:
            pois.append({
                'id': nid,
                'name': tags.get('name', amenity),
                'category': amenity,
                'coordinates': [n['lon'], n['lat']],
                'type': 'node'
            })

    # Count actual junctions (nodes shared by 2+ roads)
    node_usage = {}
    for way in ways:
        if way['tags'].get('highway', '') and way['tags'].get('highway') not in ('footway', 'path', 'cycleway'):
            for nid in way['nodes']:
                node_usage[nid] = node_usage.get(nid, 0) + 1
    junction_count = sum(1 for c in node_usage.values() if c >= 2)

    return {
        'roads': roads,
        'buildings': buildings,
        'pois': pois,
        'junction_count': junction_count,
        'node_count': len(nodes),
        'way_count': len(ways)
    }


def build_xodr(data: dict, city_name: str) -> str:
    """Build minimal OpenDRIVE 1.4 XML from parsed OSM data."""
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<OpenDRIVE>',
        f'  <header name="{city_name}" revMajor="1" revMinor="4" date="{datetime.utcnow().isoformat()}Z"/>'
    ]

    for i, road in enumerate(data['roads']):
        coords = road['coordinates']
        plan_view = '      <planView>\n'
        for j, (lon, lat) in enumerate(coords):
            plan_view += f'        <geometry s="{j*10:.1f}" x="{lon:.6f}" y="{lat:.6f}" hdg="0" length="10">\n'
            plan_view += '          <line/>\n        </geometry>\n'
        plan_view += '      </planView>'

        lines.append(f'  <road name="{road["name"]}" length="{len(coords)*10:.1f}" id="{i}">')
        lines.append('    <type s="0" type="town"/>')
        lines.append('    <planView>')
        for j, (lon, lat) in enumerate(coords):
            lines.append(f'      <geometry s="{j*10:.1f}" x="{lon:.6f}" y="{lat:.6f}" hdg="0" length="10"><line/></geometry>')
        lines.append('    </planView>')
        lines.append('    <lanes>')
        lines.append('      <laneSection s="0">')
        lines.append('        <center><lane id="0" type="none"><width sOffset="0" a="0"/></lane></center>')
        for lane_idx in range(1, road.get('lanes', 2) + 1):
            lines.append(f'        <right><lane id="-{lane_idx}" type="driving"><width sOffset="0" a="3.5"/></lane></right>')
        lines.append('      </laneSection>')
        lines.append('    </lanes>')
        lines.append('  </road>')

    lines.append('</OpenDRIVE>')
    return '\n'.join(lines)


def build_citypack(data: dict, city_name: str, version: str = '1.0.0') -> dict:
    """Build citypack JSON manifest."""
    return {
        'name': city_name,
        'version': version,
        'generated_at': datetime.utcnow().isoformat() + 'Z',
        'format': 'racegps-citypack-v1',
        'statistics': {
            'roads': len(data['roads']),
            'junctions': data['junction_count'],
            'buildings': len(data['buildings']),
            'pois': len(data['pois']),
            'nodes': data['node_count'],
            'ways': data['way_count']
        },
        'roads': data['roads'],
        'buildings': data['buildings'],
        'pois': data['pois'],
        'routes': [
            {
                'id': generate_id(),
                'name': f'{city_name} Sprint A',
                'type': 'cruise_sprint',
                'difficulty': 'medium',
                'estimated_length_km': round(len(data['roads']) * 0.5, 1)
            }
        ]
    }


def main():
    parser = argparse.ArgumentParser(description='raceGPS Batch Citypack Generator')
    parser.add_argument('--input', '-i', required=True, help='Input .osm or .osm.pbf file')
    parser.add_argument('--output', '-o', required=True, help='Output directory')
    parser.add_argument('--name', '-n', default='GeneratedCity', help='City name')
    parser.add_argument('--version', '-v', default='1.0.0', help='Citypack version')
    parser.add_argument('--road-quality', type=float, default=0.5, help='Road quality threshold (0-1)')
    parser.add_argument('--building-density', type=float, default=0.5, help='Building density (0-1)')
    parser.add_argument('--poi-categories', default='all', help='Comma-separated POI categories or "all"')
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f'ERROR: Input file not found: {args.input}', file=sys.stderr)
        sys.exit(1)

    os.makedirs(args.output, exist_ok=True)

    print(f'Parsing {args.input}...')
    data = parse_osm_xml(args.input)

    print(f'Found {len(data["roads"])} roads, {data["junction_count"]} junctions, {len(data["buildings"])} buildings, {len(data["pois"])} POIs')

    citypack = build_citypack(data, args.name, args.version)
    xodr = build_xodr(data, args.name)

    cp_path = os.path.join(args.output, 'citypack.json')
    with open(cp_path, 'w') as f:
        json.dump(citypack, f, indent=2)
    print(f'Wrote citypack: {cp_path}')

    xodr_path = os.path.join(args.output, 'city.xodr')
    with open(xodr_path, 'w') as f:
        f.write(xodr)
    print(f'Wrote OpenDRIVE: {xodr_path}')

    # Validate against schema
    schema_path = os.path.join(os.path.dirname(__file__), 'schema', 'citypack.schema.json')
    if os.path.exists(schema_path):
        with open(schema_path) as f:
            schema = json.load(f)
        # Basic validation: check required top-level keys
        required = schema.get('required', [])
        missing = [k for k in required if k not in citypack]
        if missing:
            print(f'WARNING: Citypack missing required keys: {missing}', file=sys.stderr)
        else:
            print('Schema validation: OK')
    else:
        print('Schema not found, skipping validation')

    print('Done.')


if __name__ == '__main__':
    main()
