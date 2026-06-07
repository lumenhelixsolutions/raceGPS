#!/usr/bin/env python3
"""Universal City Compiler CLI.

Usage:
    python cli.py "Cleveland, OH" --radius 8 --routes 6
    python cli.py "41.5,-81.7" --radius 5 --detail full
    python cli.py --batch batch_cities.txt --output ../../citypacks
"""

import argparse
import sys
from pathlib import Path

from city_compiler import compile_city


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile any real-world city into a raceGPS citypack."
    )
    parser.add_argument("city", nargs="?", help="City name or 'lat,lon' coordinates")
    parser.add_argument("--radius", type=float, default=5.0, help="Radius in km (default: 5)")
    parser.add_argument("--detail", choices=["minimal", "standard", "full"], default="standard",
                        help="OSM detail level")
    parser.add_argument("--routes", type=int, default=4, help="Routes per mode (default: 4)")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    parser.add_argument("--output", type=Path, default=Path("../../citypacks"),
                        help="Output directory for citypacks")
    parser.add_argument("--batch", type=Path, default=None,
                        help="File with one city per line for batch compilation")

    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[2]
    output_dir = args.output if args.output.is_absolute() else project_root / args.output
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.batch:
        if not args.batch.exists():
            print(f"Batch file not found: {args.batch}")
            return 1
        cities = [line.strip() for line in args.batch.read_text(encoding="utf-8").splitlines() if line.strip()]
        print(f"Batch mode: {len(cities)} cities")
        for city in cities:
            result = compile_city(city, output_dir, args.radius, args.detail, args.routes, args.seed)
            if not result["success"]:
                print(f"[FAIL] {city}: {result.get('error')}")
        return 0

    if not args.city:
        parser.print_help()
        return 1

    result = compile_city(args.city, output_dir, args.radius, args.detail, args.routes, args.seed)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    sys.exit(main())
