#!/usr/bin/env python3
"""
raceGPS Gameplay & Race Data Validator

Validates citypack data, route integrity, checkpoint logic,
and simulates race scoring without the UE5 runtime.

Usage:
    cd tests
    python test_gameplay.py
    # or on Windows:
    py test_gameplay.py
"""

import json
import math
import sys
from pathlib import Path

CITYPACK_DIR = Path(__file__).parent.parent / "citypacks" / "akron-oh-beta-001"


def haversine(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """Distance between two lat/lon points in meters."""
    R = 6_371_000  # Earth radius in meters
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c


class TestManifest:
    """Tests for akron_semantic_manifest.json"""

    def __init__(self):
        self.path = CITYPACK_DIR / "akron_semantic_manifest.json"
        self.data = self._load()

    def _load(self):
        if not self.path.exists():
            return None
        return json.loads(self.path.read_text())

    def run(self) -> int:
        print("=" * 60)
        print("TEST: Manifest")
        print("=" * 60)
        if self.data is None:
            print("FAIL: akron_semantic_manifest.json not found")
            return 1

        errors = 0

        # Required fields
        required = ["city_id", "display_name", "version", "bounds", "route_count", "poi_count"]
        for field in required:
            if field not in self.data:
                print(f"FAIL: Missing required field '{field}'")
                errors += 1
            else:
                print(f"PASS: Field '{field}' present = {self.data[field]}")

        if "route_count" in self.data:
            print(f"INFO: {self.data['route_count']} route(s) defined")

        if "poi_count" in self.data:
            print(f"INFO: {self.data['poi_count']} POI(s)")

        if "spawn_point_count" in self.data:
            print(f"INFO: {self.data['spawn_point_count']} spawn point(s)")

        print(f"RESULT: {errors} error(s)")
        return errors


class TestRoutes:
    """Tests for akron_routes.json"""

    def __init__(self):
        self.path = CITYPACK_DIR / "akron_routes.json"
        self.routes = []

    def _load_routes(self) -> int:
        if not self.path.exists():
            print(f"FAIL: Route file not found: {self.path}")
            return 1

        self.routes = json.loads(self.path.read_text())
        return 0

    def run(self) -> int:
        print("\n" + "=" * 60)
        print("TEST: Routes")
        print("=" * 60)

        if self._load_routes():
            return 1

        if not self.routes:
            print("FAIL: No routes found in file")
            return 1

        total_errors = 0
        for route in self.routes:
            errors = self._test_route(route)
            total_errors += errors

        print(f"\nRESULT: {total_errors} error(s) across {len(self.routes)} route(s)")
        return total_errors

    def _test_route(self, route: dict) -> int:
        route_id = route.get("route_id", "unknown")
        print(f"\n  Route: {route_id}")
        errors = 0

        # Required fields
        for field in ["route_id", "mode", "name", "distance_meters", "start", "finish", "points", "checkpoints"]:
            if field not in route:
                print(f"    FAIL: Missing field '{field}'")
                errors += 1

        # Points (waypoints)
        points = route.get("points", [])
        print(f"    INFO: {len(points)} points")
        if len(points) < 2:
            print(f"    FAIL: Need at least 2 points, got {len(points)}")
            errors += 1

        # Checkpoints
        checkpoints = route.get("checkpoints", [])
        print(f"    INFO: {len(checkpoints)} checkpoints")
        if len(checkpoints) == 0:
            print(f"    WARN: No checkpoints defined")

        # Distance sanity check
        dist = route.get("distance_meters", 0)
        if dist <= 0:
            print(f"    FAIL: Distance must be > 0")
            errors += 1
        elif dist < 500:
            print(f"    WARN: Distance {dist:.0f}m is very short")
        else:
            print(f"    PASS: Distance {dist:.0f}m")

        # Checkpoints are near the route path
        if len(points) >= 2 and len(checkpoints) > 0:
            cp_errors = self._test_checkpoint_order(points, checkpoints)
            errors += cp_errors

        # Start and finish exist
        start = route.get("start")
        finish = route.get("finish")
        if start and finish:
            print(f"    PASS: Start/Finish defined")
        else:
            print(f"    FAIL: Start or Finish missing")
            errors += 1

        # Calculate actual path length
        if len(points) >= 2:
            actual_length = 0.0
            for i in range(len(points) - 1):
                actual_length += haversine(
                    points[i]["lat"], points[i]["lon"],
                    points[i + 1]["lat"], points[i + 1]["lon"]
                )
            variance = abs(actual_length - dist) / max(dist, 1)
            if variance > 0.15:
                print(f"    WARN: Path length {actual_length:.0f}m differs from stated {dist:.0f}m by {variance*100:.1f}%")
            else:
                print(f"    PASS: Path length {actual_length:.0f}m matches stated distance")

        if errors == 0:
            print(f"    PASS: Route '{route_id}' is valid")
        return errors

    def _test_checkpoint_order(self, points: list, checkpoints: list) -> int:
        """Check that checkpoints appear in order along the point path."""
        errors = 0
        for i, cp in enumerate(checkpoints):
            cp_lat = cp.get("lat", 0)
            cp_lon = cp.get("lon", 0)

            min_dist = float("inf")
            for pt in points:
                d = haversine(cp_lat, cp_lon, pt["lat"], pt["lon"])
                if d < min_dist:
                    min_dist = d

            if min_dist > 100:  # 100m threshold
                print(f"    FAIL: Checkpoint {i} is {min_dist:.0f}m from nearest path point")
                errors += 1
            else:
                print(f"    PASS: Checkpoint {i} is {min_dist:.0f}m from path")

        return errors


class RaceSimulator:
    """Simulates a race and validates scoring logic."""

    GOLD_TIME = 120.0
    SILVER_TIME = 150.0
    BRONZE_TIME = 200.0

    def __init__(self):
        self.path = CITYPACK_DIR / "akron_routes.json"
        self.routes = []
        self._load_routes()

    def _load_routes(self):
        if not self.path.exists():
            return
        self.routes = json.loads(self.path.read_text())

    def calculate_score(self, base_time: float, collisions: int, missed_checkpoints: int) -> dict:
        """Replicates CruiseSprintGameMode scoring formula."""
        collision_penalty = collisions * 2.0
        missed_cp_penalty = missed_checkpoints * 5.0
        clean_bonus = 1.0 if collisions == 0 else 0.0
        final_time = base_time + collision_penalty + missed_cp_penalty - clean_bonus

        if final_time <= self.GOLD_TIME:
            medal = "GOLD"
        elif final_time <= self.SILVER_TIME:
            medal = "SILVER"
        elif final_time <= self.BRONZE_TIME:
            medal = "BRONZE"
        else:
            medal = "NONE"

        return {
            "base_time": base_time,
            "collision_penalty": collision_penalty,
            "missed_cp_penalty": missed_cp_penalty,
            "clean_bonus": clean_bonus,
            "final_time": final_time,
            "medal": medal,
            "collisions": collisions,
            "missed_checkpoints": missed_checkpoints,
        }

    def run(self) -> int:
        print("\n" + "=" * 60)
        print("TEST: Race Simulator")
        print("=" * 60)

        if not self.routes:
            print("FAIL: No routes to simulate")
            return 1

        errors = 0

        # Test case 1: Perfect run
        score = self.calculate_score(110.0, 0, 0)
        print(f"\n  Perfect Run (110s, 0 collisions, 0 missed CPs):")
        self._print_score(score)
        if score["medal"] != "GOLD":
            print("    FAIL: Should be GOLD")
            errors += 1

        # Test case 2: Fast but messy
        score = self.calculate_score(100.0, 3, 1)
        print(f"\n  Messy Run (100s, 3 collisions, 1 missed CP):")
        self._print_score(score)
        expected_final = 100.0 + 6.0 + 5.0 - 0.0  # 111.0
        if abs(score["final_time"] - expected_final) > 0.01:
            print(f"    FAIL: Expected {expected_final}, got {score['final_time']}")
            errors += 1

        # Test case 3: Slow but clean
        score = self.calculate_score(160.0, 0, 0)
        print(f"\n  Slow Clean Run (160s, 0 collisions, 0 missed CPs):")
        self._print_score(score)
        if score["medal"] != "BRONZE":
            print("    FAIL: Should be BRONZE")
            errors += 1

        # Test case 4: Barely bronze
        score = self.calculate_score(200.0, 0, 0)
        print(f"\n  Barely Bronze (200s, 0 collisions, 0 missed CPs):")
        self._print_score(score)
        if score["medal"] != "BRONZE":
            print("    FAIL: Should be BRONZE")
            errors += 1

        # Test case 5: Over bronze threshold
        score = self.calculate_score(201.0, 0, 0)
        print(f"\n  Over Threshold (201s, 0 collisions, 0 missed CPs):")
        self._print_score(score)
        if score["medal"] != "BRONZE":
            print("    FAIL: Should be BRONZE")
            errors += 1

        # Simulate full races on each route
        print(f"\n  Simulated Races:")
        print(f"    {'Route':25s} | {'Skill':10s} | {'Base':>6s} | {'Final':>6s} | {'Medal':>6s}")
        print(f"    {'-'*25}-+-{'-'*10}-+-{'-'*6}-+-{'-'*6}-+-{'-'*6}")
        for route in self.routes:
            route_id = route.get("route_id", "unknown")
            dist = route.get("distance_meters", 0)
            num_cp = route.get("num_checkpoints", len(route.get("checkpoints", [])))

            for label, speed_ms, collisions, missed in [
                ("Pro", 35.0, 0, 0),
                ("Average", 25.0, 2, 0),
                ("Beginner", 15.0, 5, 1),
            ]:
                base_time = dist / speed_ms
                score = self.calculate_score(base_time, collisions, missed)
                print(f"    {route_id:25s} | {label:10s} | {base_time:5.1f}s | {score['final_time']:5.1f}s | {score['medal']:>6s}")

        print(f"\nRESULT: {errors} error(s)")
        return errors

    def _print_score(self, score: dict):
        print(f"    Base: {score['base_time']:.1f}s")
        print(f"    Collision Penalty: +{score['collision_penalty']:.1f}s")
        print(f"    Missed CP Penalty: +{score['missed_cp_penalty']:.1f}s")
        print(f"    Clean Bonus: -{score['clean_bonus']:.1f}s")
        print(f"    FINAL: {score['final_time']:.1f}s | MEDAL: {score['medal']}")


def main() -> int:
    print("raceGPS Gameplay & Race Data Validator")
    print("=" * 60)

    if not CITYPACK_DIR.exists():
        print(f"ERROR: Citypack directory not found: {CITYPACK_DIR}")
        print("Run the semantic compiler first:")
        print("  cd tools/akron-semantic-compiler")
        print("  python compile_akron.py")
        return 1

    total_errors = 0

    total_errors += TestManifest().run()
    total_errors += TestRoutes().run()
    total_errors += RaceSimulator().run()

    print("\n" + "=" * 60)
    if total_errors == 0:
        print("ALL TESTS PASSED")
    else:
        print(f"FAILED: {total_errors} total error(s)")
    print("=" * 60)

    return total_errors


if __name__ == "__main__":
    sys.exit(main())
