#!/usr/bin/env python3
"""Reference control math for ARaceAIDriverController / RaceAIControlMath.

KEEP IN SYNC with:
  apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/ClevelandShowcaseTypes.h
  (namespace RaceAIControlMath)

These tests do not require Unreal. Run:
  python3 tests/test_race_ai_control.py
"""

from __future__ import annotations

import math
import unittest

# --- constants matching FRaceAIControlGains defaults ---
K_HEADING = 1.25
K_CROSS_TRACK = 0.35
VMAX_KMH = 180.0
VMIN_KMH = 40.0
KCURVE = 25.0
RECOVERY_SPEED_KMH = 5.0
RECOVERY_THROTTLE_THRESH = 0.50
RECOVERY_STUCK_DELAY_SEC = 1.50
RECOVERY_MAX_CTE_CM = 800.0


def clamp11(value: float) -> float:
    return max(-1.0, min(1.0, value))


def wrap_s(s: float, track_length: float) -> float:
    if track_length <= 1e-8:
        return 0.0
    w = math.fmod(s, track_length)
    if w < 0.0:
        w += track_length
    return w


def target_speed_kmh(abs_curvature_per_meter: float, vmax: float, vmin: float, kcurve: float) -> float:
    """target_speed = clamp(Vmax / (1 + Kcurve * abs(curvature)), Vmin, Vmax)."""
    denom = 1.0 + kcurve * abs(abs_curvature_per_meter)
    raw = vmax / max(denom, 1e-12)
    return max(vmin, min(vmax, raw))


def signed_heading_error_rad(forward_xy: tuple[float, float], dir_to_target_xy: tuple[float, float]) -> float:
    def _norm(x: float, y: float) -> tuple[float, float]:
        n = math.hypot(x, y)
        if n <= 1e-12:
            return (0.0, 0.0)
        return (x / n, y / n)

    fx, fy = _norm(forward_xy[0], forward_xy[1])
    dx, dy = _norm(dir_to_target_xy[0], dir_to_target_xy[1])
    if fx == 0.0 and fy == 0.0:
        return 0.0
    if dx == 0.0 and dy == 0.0:
        return 0.0
    cross_z = fx * dy - fy * dx
    dot = fx * dx + fy * dy
    return math.atan2(cross_z, dot)


def steering_command(heading_error_rad: float, cte_meters: float, k_heading: float, k_cross_track: float) -> float:
    return clamp11(k_heading * heading_error_rad + k_cross_track * cte_meters)


def recovery_trigger(
    speed_kmh: float,
    throttle_cmd: float,
    stuck_time_sec: float,
    abs_cte_cm: float,
    speed_thresh: float = RECOVERY_SPEED_KMH,
    throttle_thresh: float = RECOVERY_THROTTLE_THRESH,
    delay_sec: float = RECOVERY_STUCK_DELAY_SEC,
    max_cte_cm: float = RECOVERY_MAX_CTE_CM,
) -> bool:
    stuck = (speed_kmh < speed_thresh) and (throttle_cmd > throttle_thresh) and (stuck_time_sec > delay_sec)
    off_line = abs_cte_cm > max_cte_cm
    return stuck or off_line


def race_progress(lap_index: int, track_length_cm: float, current_s_cm: float) -> float:
    """RaceProgress = lap * length + s. Euclidean-to-finish is NOT used."""
    return lap_index * track_length_cm + current_s_cm


def sort_standings(cars: list[dict]) -> list[dict]:
    ranked = sorted(cars, key=lambda c: race_progress(c["lap"], c["length"], c["s"]), reverse=True)
    for i, car in enumerate(ranked):
        car = dict(car)
        car["place"] = i + 1
        ranked[i] = car
    return ranked


class TestSteeringClamp(unittest.TestCase):
    def test_in_range_passthrough(self):
        self.assertAlmostEqual(steering_command(0.1, 0.2, K_HEADING, K_CROSS_TRACK), 0.195)

    def test_positive_saturates(self):
        self.assertEqual(steering_command(10.0, 50.0, K_HEADING, K_CROSS_TRACK), 1.0)

    def test_negative_saturates(self):
        self.assertEqual(steering_command(-10.0, -50.0, K_HEADING, K_CROSS_TRACK), -1.0)

    def test_clamp11_edges(self):
        self.assertEqual(clamp11(-1.0), -1.0)
        self.assertEqual(clamp11(1.0), 1.0)
        self.assertEqual(clamp11(-3.0), -1.0)
        self.assertEqual(clamp11(3.0), 1.0)


class TestTargetSpeed(unittest.TestCase):
    def test_zero_curvature_is_vmax(self):
        self.assertAlmostEqual(target_speed_kmh(0.0, VMAX_KMH, VMIN_KMH, KCURVE), VMAX_KMH)

    def test_formula_mid_corner(self):
        k = 0.04  # 1/m
        expected = max(VMIN_KMH, min(VMAX_KMH, VMAX_KMH / (1.0 + KCURVE * abs(k))))
        self.assertAlmostEqual(target_speed_kmh(k, VMAX_KMH, VMIN_KMH, KCURVE), expected)
        self.assertAlmostEqual(expected, 180.0 / (1.0 + 25.0 * 0.04))  # 90 km/h

    def test_tight_corner_clamps_vmin(self):
        k = 1.0
        raw = VMAX_KMH / (1.0 + KCURVE * k)
        self.assertLess(raw, VMIN_KMH)
        self.assertAlmostEqual(target_speed_kmh(k, VMAX_KMH, VMIN_KMH, KCURVE), VMIN_KMH)

    def test_negative_curvature_uses_abs(self):
        a = target_speed_kmh(0.02, VMAX_KMH, VMIN_KMH, KCURVE)
        b = target_speed_kmh(-0.02, VMAX_KMH, VMIN_KMH, KCURVE)
        self.assertAlmostEqual(a, b)


class TestRecoveryTrigger(unittest.TestCase):
    def test_not_stuck_if_moving(self):
        self.assertFalse(recovery_trigger(40.0, 1.0, 5.0, 10.0))

    def test_not_stuck_if_no_throttle(self):
        self.assertFalse(recovery_trigger(1.0, 0.1, 5.0, 10.0))

    def test_not_stuck_before_delay(self):
        self.assertFalse(recovery_trigger(1.0, 1.0, 0.4, 10.0))

    def test_stuck_after_delay(self):
        self.assertTrue(recovery_trigger(1.0, 0.8, 1.51, 10.0))

    def test_cte_alone_triggers(self):
        self.assertTrue(recovery_trigger(120.0, 0.0, 0.0, 801.0))

    def test_cte_at_limit_does_not(self):
        self.assertFalse(recovery_trigger(120.0, 0.0, 0.0, 800.0))


class TestFoldedCircuitStandings(unittest.TestCase):
    """A folded / out-and-back circuit: start/finish coincide geographically.

    Euclidean distance to the finish line would rank the car *about to finish
    lap 0* ahead of the car that *already took the flag*. RaceProgress =
    lap * length + s is the correct order.
    """

    def test_three_cars_euclidean_would_be_wrong(self):
        length = 1000.0  # cm along the line
        # Finish pose at s=0 / s=length.
        cars = [
            {"name": "AI01", "lap": 0, "s": 980.0, "length": length},  # geographically ~20 cm from finish
            {"name": "PLAYER", "lap": 1, "s": 50.0, "length": length},  # already on lap 2 start; ~50 cm from finish
            {"name": "AI02", "lap": 0, "s": 400.0, "length": length},
        ]

        def euclidean_to_finish(s: float) -> float:
            return min(s, length - s)

        euclid_order = sorted(cars, key=lambda c: euclidean_to_finish(c["s"]))
        self.assertEqual([c["name"] for c in euclid_order], ["AI01", "PLAYER", "AI02"])

        race_order = sort_standings(cars)
        self.assertEqual([c["name"] for c in race_order], ["PLAYER", "AI01", "AI02"])
        self.assertEqual([c["place"] for c in race_order], [1, 2, 3])
        self.assertGreater(
            race_progress(1, length, 50.0),
            race_progress(0, length, 980.0),
        )

    def test_wrap_s(self):
        self.assertAlmostEqual(wrap_s(-50.0, 1000.0), 950.0)
        self.assertAlmostEqual(wrap_s(1000.0, 1000.0), 0.0)
        self.assertAlmostEqual(wrap_s(1050.0, 1000.0), 50.0)


class TestHeadingSign(unittest.TestCase):
    def test_left_positive_z_up(self):
        err = signed_heading_error_rad((1.0, 0.0), (0.0, 1.0))
        self.assertGreater(err, 0.0)
        self.assertAlmostEqual(err, math.pi / 2.0, places=5)

    def test_aligned_zero(self):
        self.assertAlmostEqual(signed_heading_error_rad((1.0, 0.0), (2.0, 0.0)), 0.0)


if __name__ == "__main__":
    unittest.main()
