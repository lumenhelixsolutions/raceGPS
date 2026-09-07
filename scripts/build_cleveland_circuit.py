#!/usr/bin/env python3
"""Build raceGPS Cleveland Historic Circuit (1997) citypack from OSM BKL geometry.

Reconstruction, not an official surveyed polyline. Assumptions documented in
docs/CLEVELAND_TRACK_PROVENANCE.md.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

ROOT = Path("/workspace/cleveland-showcase")
PACK = ROOT / "citypacks/cleveland/burke_gp_1997"
DOCS = ROOT / "docs"
TESTS = ROOT / "tests"

LAT0 = 41.51722
LON0 = -81.68306
ELEV_M = 174.0
R_LAT = 111320.0
R_LON = 111320.0 * math.cos(math.radians(LAT0))
OFFICIAL_M = 3389.0  # 2.106 statute miles
EARTH_R = 6371000.0

# OpenDRIVE local: x east, y north, hdg from +x CCW (standard).
# Compass heading 0=N increases clockwise.


def ll_to_xy(lat: float, lon: float) -> tuple[float, float]:
    return ((lon - LON0) * R_LON, (lat - LAT0) * R_LAT)


def xy_to_ll(x: float, y: float) -> tuple[float, float]:
    return (LAT0 + y / R_LAT, LON0 + x / R_LON)


def hav_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    p1, p2 = map(math.radians, (lat1, lat2))
    dlat = p2 - p1
    dlon = math.radians(lon2 - lon1)
    h = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * EARTH_R * math.asin(math.sqrt(min(1.0, h)))


def dist(a, b) -> float:
    return math.hypot(b[0] - a[0], b[1] - a[1])


def lerp(a, b, t: float):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def vsub(a, b):
    return (a[0] - b[0], a[1] - b[1])


def vadd(a, b):
    return (a[0] + b[0], a[1] + b[1])


def smul(a, s: float):
    return (a[0] * s, a[1] * s)


def nrm(v):
    l = math.hypot(*v) or 1.0
    return (v[0] / l, v[1] / l)


def wrap_pi(a: float) -> float:
    while a > math.pi:
        a -= 2 * math.pi
    while a < -math.pi:
        a += 2 * math.pi
    return a


def compass_hdg(p, q) -> float:
    """Radians, 0=north, clockwise positive (atan2(east, north))."""
    return math.atan2(q[0] - p[0], q[1] - p[1])


def odr_hdg_from_dir(dx: float, dy: float) -> float:
    """OpenDRIVE heading: radians from +x (east) CCW (toward north = +pi/2)."""
    return math.atan2(dy, dx)


# --- OSM anchors (Overpass API 2026-08-22, ODbL) ---
RW_N_06 = ll_to_xy(41.5141381, -81.6920474)
RW_N_A = ll_to_xy(41.5144103, -81.6914984)
RW_N_B = ll_to_xy(41.5163927, -81.6872895)
RW_N_C = ll_to_xy(41.5173557, -81.6852383)
RW_N_D = ll_to_xy(41.5187902, -81.6822007)
RW_N_E = ll_to_xy(41.5211882, -81.6771026)
RW_N_F = ll_to_xy(41.5226861, -81.6739268)

RW_S_06 = ll_to_xy(41.5127597, -81.6915869)

G_SW = ll_to_xy(41.5124880, -81.6906416)
G_A = ll_to_xy(41.5126601, -81.6902787)
G_B = ll_to_xy(41.5138273, -81.6877950)
G_C = ll_to_xy(41.5154966, -81.6842622)
G_D = ll_to_xy(41.5172433, -81.6805527)
G_E = ll_to_xy(41.5195908, -81.6755688)
G_F = ll_to_xy(41.5209968, -81.6725930)

A_MID = ll_to_xy(41.5136244, -81.6914723)
A_06L = RW_N_A

F_MID = ll_to_xy(41.5222690, -81.6731255)
F_G = G_F


def closed_polyline() -> list[tuple[float, float]]:
    """Clockwise centerline vertices (S/F first). 10 corners after filleting.

    Race direction: Taxiway G SW -> T1 vortex (Taxiway A / old T3) -> 06L/24R NE
    -> Taxiway F east complex -> G SW with T9/T10 chicane.
    """
    # Shorten vs full-runway trace so measured length tracks 1997 official
    # 2.106 mi (the 1990–96 2.369 mi figure was later called wrong). Centerline
    # uses inner racing line on 06L/G rather than full 24R/G-NE pavement.
    dir_ne = nrm(vsub(RW_N_B, RW_N_A))  # ~058 true
    right_ne = (dir_ne[1], -dir_ne[0])
    dir_sw = nrm(vsub(G_A, G_C))        # along G toward 06
    right_sw = (dir_sw[1], -dir_sw[0])

    sf = lerp(G_C, G_D, 0.55)
    # Stay on G until the vortex; do not take the 1982 left-right (pit exit).
    t1_in = G_A
    t1_apex = vadd(A_MID, smul(dir_sw, 6.0))  # T1 right hairpin on Taxiway A
    t2 = vadd(lerp(A_06L, RW_N_B, 0.05), smul(right_ne, -14.0))  # T2
    t3 = vadd(lerp(RW_N_A, RW_N_B, 0.20), smul(right_ne, 38.0))  # T3 right
    t4 = lerp(RW_N_A, RW_N_B, 0.38)  # T4 left
    t5 = lerp(RW_N_E, RW_N_F, 0.12)  # T5 right near taxiway E
    t6 = vadd(t5, smul(right_ne, 110.0))
    t6 = vadd(t6, smul(dir_ne, 55.0))
    t7 = vadd(lerp(G_E, G_F, 0.05), smul(dir_ne, 22.0))  # T7
    t8 = vadd(lerp(G_E, G_D, 0.12), smul(right_sw, -6.0))  # T8 left onto G
    t9 = vadd(lerp(G_D, G_C, 0.08), smul(right_sw, 32.0))  # T9 right chicane
    t10 = lerp(G_D, G_C, 0.26)  # T10 left

    # t1_in kept off the driven polyline so T1 is a single hairpin (1990+).
    _ = t1_in
    pts = [sf, t1_apex, t2, t3, t4, t5, t6, t7, t8, t9, t10]
    return pts


def fillet_closed(pts, radii: list[float], step: float = 2.5):
    """Fillet each vertex of a closed polyline. Returns dense XY samples + geom segments."""
    n = len(pts)
    assert len(radii) == n
    samples = []
    segs = []  # OpenDRIVE planView pieces in order, start at fillet exit of vertex 0? 
    # We start samples at vertex 0 along the incoming line's fillet exit... 
    # Better: start at the midpoint of the first straight (S/F on a line).
    # Vertex 0 is S/F and should NOT be a turn (radius 0).

    def vertex_fillet(i):
        p_prev = pts[(i - 1) % n]
        p = pts[i]
        p_next = pts[(i + 1) % n]
        r = radii[i]
        u_in = nrm(vsub(p, p_prev))
        u_out = nrm(vsub(p_next, p))
        # internal angle via cross/dot in 2D
        dot = max(-1.0, min(1.0, u_in[0] * u_out[0] + u_in[1] * u_out[1]))
        cross = u_in[0] * u_out[1] - u_in[1] * u_out[0]  # >0 CCW (left)
        turn_ang = math.acos(dot)
        if turn_ang < math.radians(4) or r < 1.0:
            return None
        # tangent length
        # For a fillet, t = r * tan(theta/2) where theta is turn angle
        tlen = r * math.tan(turn_ang / 2.0)
        max_in = dist(p_prev, p) * 0.48
        max_out = dist(p, p_next) * 0.48
        if tlen > max_in or tlen > max_out:
            tlen = min(max_in, max_out)
            if tlen < 0.5:
                return None
            r_eff = tlen / math.tan(turn_ang / 2.0)
        else:
            r_eff = r
        p_tan_in = vsub(p, smul(u_in, tlen))
        p_tan_out = vadd(p, smul(u_out, tlen))
        # center: from tan_in, move perpendicular
        # left turn (cross>0): center to the left of u_in
        left_in = (-u_in[1], u_in[0])
        if cross > 0:
            center = vadd(p_tan_in, smul(left_in, r_eff))
            kappa = +1.0 / r_eff
        else:
            center = vsub(p_tan_in, smul(left_in, r_eff))
            kappa = -1.0 / r_eff
        return {
            "tan_in": p_tan_in,
            "tan_out": p_tan_out,
            "center": center,
            "r": r_eff,
            "kappa": kappa,
            "ang": turn_ang,
            "left": cross > 0,
            "i": i,
        }

    fillets = [vertex_fillet(i) for i in range(n)]

    # Walk i = 0..n-1: line from previous fillet tan_out (or pts[i] if none) to this fillet tan_in, then arc
    dense = []
    odr = []

    def append_line(a, b):
        L = dist(a, b)
        if L < 0.15:
            return
        nstep = max(1, int(math.ceil(L / step)))
        for k in range(nstep):
            t = k / nstep
            dense.append(lerp(a, b, t))
        dx, dy = b[0] - a[0], b[1] - a[1]
        odr.append({
            "type": "line",
            "x": a[0],
            "y": a[1],
            "hdg": odr_hdg_from_dir(dx, dy),
            "length": L,
            "curvature": 0.0,
        })

    def append_arc(f):
        a, b, c = f["tan_in"], f["tan_out"], f["center"]
        va = vsub(a, c)
        vb = vsub(b, c)
        ang0 = math.atan2(va[1], va[0])
        ang1 = math.atan2(vb[1], vb[0])
        dang = wrap_pi(ang1 - ang0)
        # left turn: CCW, dang>0; right: dang<0
        if f["left"] and dang < 0:
            dang += 2 * math.pi
        if (not f["left"]) and dang > 0:
            dang -= 2 * math.pi
        L = f["r"] * abs(dang)
        nstep = max(2, int(math.ceil(L / step)))
        for k in range(nstep):
            t = k / nstep
            ang = ang0 + dang * t
            dense.append((c[0] + f["r"] * math.cos(ang), c[1] + f["r"] * math.sin(ang)))
        # OpenDRIVE arc: start heading along tangent. Tangent of circle:
        # position angle ang0 is radius vector; tangent CCW is ang0+pi/2
        if f["left"]:
            th = ang0 + math.pi / 2
        else:
            th = ang0 - math.pi / 2
        odr.append({
            "type": "arc",
            "x": a[0],
            "y": a[1],
            "hdg": th,
            "length": L,
            "curvature": f["kappa"],
        })

    for i in range(n):
        f_prev = fillets[(i - 1) % n]
        f = fillets[i]
        start = f_prev["tan_out"] if f_prev else pts[(i - 1) % n]
        if f is None:
            append_line(start, pts[i])
            continue
        append_line(start, f["tan_in"])
        append_arc(f)

    # close: last fillet tan_out already connected when i wraps? 
    # After i=n-1 we have drawn up through last arc. Need line from last tan_out to first tan_in.
    f0 = fillets[0]
    fn = fillets[-1]
    close_from = fn["tan_out"] if fn else pts[-1]
    close_to = f0["tan_in"] if f0 else pts[0]
    # When i=0 we started from fillets[-1].tan_out already — yes i=0 uses f_prev = last.
    # So the loop is closed. Do not duplicate.

    # Ensure first sample is S/F (pts[0]) if vertex 0 has no fillet
    if fillets[0] is None:
        # rotate dense so closest to pts[0] is first
        best_i, best_d = 0, 1e9
        for i, p in enumerate(dense):
            d = dist(p, pts[0])
            if d < best_d:
                best_i, best_d = i, d
        dense = dense[best_i:] + dense[:best_i]

    # close densify: add pts[0] at end for wrap measurement without duplicating in JSON
    if dist(dense[0], dense[-1]) > 0.5:
        dense.append(dense[0])

    return dense, odr, fillets


def densify_equal(dense, spacing=8.0):
    """Resample closed ring (last==first) to ~spacing meters."""
    if dist(dense[0], dense[-1]) > 1.0:
        dense = list(dense) + [dense[0]]
    acc = [0.0]
    for i in range(1, len(dense)):
        acc.append(acc[-1] + dist(dense[i - 1], dense[i]))
    total = acc[-1]
    n = max(3, int(round(total / spacing)))
    out = []
    for k in range(n):
        s = (k / n) * total  # exclude closing duplicate
        # binary search
        lo, hi = 0, len(acc) - 1
        while lo + 1 < hi:
            mid = (lo + hi) // 2
            if acc[mid] <= s:
                lo = mid
            else:
                hi = mid
        seg = acc[hi] - acc[lo] or 1.0
        t = (s - acc[lo]) / seg
        out.append(lerp(dense[lo], dense[hi], t))
    return out, total


def curvature_series(pts):
    n = len(pts)
    ks = []
    for i in range(n):
        p0 = pts[(i - 1) % n]
        p1 = pts[i]
        p2 = pts[(i + 1) % n]
        a, b, c = dist(p0, p1), dist(p1, p2), dist(p0, p2)
        if a < 0.2 or b < 0.2:
            ks.append(0.0)
            continue
        # signed curvature via turning angle / path
        u = nrm(vsub(p1, p0))
        v = nrm(vsub(p2, p1))
        cross = u[0] * v[1] - u[1] * v[0]
        dot = max(-1.0, min(1.0, u[0] * v[0] + u[1] * v[1]))
        dtheta = math.atan2(cross, dot)  # left positive
        ds = 0.5 * (a + b)
        ks.append(dtheta / ds if ds > 1e-6 else 0.0)
    return ks


def assign_turns(pts, ks, k_thresh=0.008):
    """Mark 10 turn regions from curvature peaks, clockwise circuit (right = k<0)."""
    n = len(pts)
    mag = [abs(k) for k in ks]
    # smooth
    sm = []
    for i in range(n):
        sm.append((mag[(i - 1) % n] + mag[i] + mag[(i + 1) % n]) / 3)

    # find contiguous above threshold
    regions = []
    i = 0
    visited = [False] * n
    # unwrap circular
    while i < n:
        if sm[i] >= k_thresh and not visited[i]:
            # expand
            j = i
            idxs = []
            while sm[j % n] >= k_thresh and not visited[j % n]:
                visited[j % n] = True
                idxs.append(j % n)
                j += 1
                if j - i > n:
                    break
            # peak
            peak = max(idxs, key=lambda t: sm[t])
            regions.append((peak, idxs))
            i = j
        else:
            i += 1
    # If wrap joined two regions at ends, merge
    if len(regions) >= 2 and 0 in regions[-1][1] and (n - 1) in regions[0][1]:
        # actually first region starts at 0? handle if first and last merge
        pass
    if regions and 0 in regions[0][1] and (n - 1) in regions[-1][1] and len(regions) > 1:
        peak = regions[0][0] if sm[regions[0][0]] >= sm[regions[-1][0]] else regions[-1][0]
        idxs = regions[-1][1] + regions[0][1]
        regions = [(peak, idxs)] + regions[1:-1]

    # sort by s index
    regions.sort(key=lambda r: r[0] if r[0] > 0 else r[0])

    # If not 10, adjust threshold
    return regions


def pick_10_turns(pts, ks):
    n = len(pts)
    for thr in (0.012, 0.010, 0.008, 0.006, 0.005, 0.004, 0.0035):
        regs = assign_turns(pts, ks, thr)
        if len(regs) == 10:
            return regs, thr
    # take strongest 10 peaks with min separation
    mag = [abs(k) for k in ks]
    order = sorted(range(n), key=lambda i: mag[i], reverse=True)
    chosen = []
    min_sep = n // 16
    for i in order:
        if all(min(abs(i - c), n - abs(i - c)) >= min_sep for c in chosen):
            chosen.append(i)
        if len(chosen) == 10:
            break
    chosen.sort()
    return [(c, [c]) for c in chosen], None


def target_speed(k: float) -> float:
    # Match C++-ish envelope: vmax 180 km/h ≈ 50 m/s, vmin 40 km/h ≈ 11 m/s
    # Champ Car was faster; use 72 m/s (260 km/h) straights, 22 m/s hairpin.
    vmax, vmin, kcurve = 72.0, 22.0, 28.0
    raw = vmax / (1.0 + kcurve * abs(k))
    return max(vmin, min(vmax, raw))


def write_xodr(odr_segs, total_s: float) -> str:
    # Recompute s along odr
    s = 0.0
    geoms = []
    for g in odr_segs:
        geoms.append((s, g))
        s += g["length"]
    length = s
    # lane width 12 m driving, extra 5 m shoulder each side as border
    xml = []
    xml.append('<?xml version="1.0" encoding="UTF-8"?>')
    xml.append('<OpenDRIVE>')
    xml.append('  <header revMajor="1" revMinor="4" name="cleveland_burke_gp_1997" version="1.0"')
    xml.append(f'          north="{xy_to_ll(0, 2000)[0]:.8f}" south="{xy_to_ll(0, -2000)[0]:.8f}"')
    xml.append(f'          east="{xy_to_ll(2000, 0)[1]:.8f}" west="{xy_to_ll(-2000, 0)[1]:.8f}"')
    xml.append(f'          vendor="raceGPS">')
    xml.append(f'    <geoReference><![CDATA[+proj=tmerc +lat_0={LAT0} +lon_0={LON0} +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs]]></geoReference>')
    xml.append('  </header>')
    xml.append(f'  <!-- Closed clockwise airport circuit. Reference line = centerline. Elevation flat {ELEV_M} m AMSL. -->')
    xml.append(f'  <road name="Cleveland Historic Circuit" length="{length:.4f}" id="1" junction="-1">')
    xml.append('    <link>')
    xml.append('      <predecessor elementType="road" elementId="1" contactPoint="end"/>')
    xml.append('      <successor elementType="road" elementId="1" contactPoint="start"/>')
    xml.append('    </link>')
    xml.append('    <planView>')
    for ss, g in geoms:
        if g["type"] == "line":
            xml.append(
                f'      <geometry s="{ss:.4f}" x="{g["x"]:.4f}" y="{g["y"]:.4f}" hdg="{g["hdg"]:.6f}" length="{g["length"]:.4f}">'
            )
            xml.append('        <line/>')
            xml.append('      </geometry>')
        else:
            xml.append(
                f'      <geometry s="{ss:.4f}" x="{g["x"]:.4f}" y="{g["y"]:.4f}" hdg="{g["hdg"]:.6f}" length="{g["length"]:.4f}">'
            )
            xml.append(f'        <arc curvature="{g["curvature"]:.6f}"/>')
            xml.append('      </geometry>')
    xml.append('    </planView>')
    xml.append('    <elevationProfile>')
    xml.append(f'      <elevation s="0.0" a="{ELEV_M:.3f}" b="0" c="0" d="0"/>')
    xml.append('    </elevationProfile>')
    xml.append('    <lanes>')
    xml.append('      <laneSection s="0.0">')
    xml.append('        <left>')
    xml.append('          <lane id="2" type="border" level="false"><link/><width sOffset="0" a="5.0" b="0" c="0" d="0"/></lane>')
    xml.append('          <lane id="1" type="driving" level="false"><link/><width sOffset="0" a="6.0" b="0" c="0" d="0"/></lane>')
    xml.append('        </left>')
    xml.append('        <center>')
    xml.append('          <lane id="0" type="none" level="true"><link/></lane>')
    xml.append('        </center>')
    xml.append('        <right>')
    xml.append('          <lane id="-1" type="driving" level="false"><link/><width sOffset="0" a="6.0" b="0" c="0" d="0"/></lane>')
    xml.append('          <lane id="-2" type="border" level="false"><link/><width sOffset="0" a="5.0" b="0" c="0" d="0"/></lane>')
    xml.append('        </right>')
    xml.append('      </laneSection>')
    xml.append('    </lanes>')
    xml.append('  </road>')
    xml.append('</OpenDRIVE>')
    return "\n".join(xml) + "\n", length


def main():
    pts = closed_polyline()
    # radii per vertex: 0 at S/F (index 0), then 10 turns
    # pts: sf, t1_in, t1_apex, t2, t3, t4, t5, t6, t7, t8, t9, t10
    radii = [
        0.0,   # 0 S/F
        32.0,  # 1 T1 vortex
        36.0,  # 2 T2
        36.0,  # 3 T3
        38.0,  # 4 T4
        34.0,  # 5 T5
        36.0,  # 6 T6
        34.0,  # 7 T7
        32.0,  # 8 T8
        28.0,  # 9 T9 chicane
        30.0,  # 10 T10 chicane
    ]

    dense, odr, fillets = fillet_closed(pts, radii, step=2.0)
    samples, total_line = densify_equal(dense, spacing=8.0)

    # haversine length
    lls = [xy_to_ll(*p) for p in samples]
    hav_len = 0.0
    for i in range(len(lls)):
        a = lls[i]
        b = lls[(i + 1) % len(lls)]
        hav_len += hav_m(a[0], a[1], b[0], b[1])

    ks = curvature_series(samples)
    n = len(samples)
    # Turn indices follow the 10 filleted corners in path order (T1 = vortex).
    turn_at = [None] * n
    ti = 0
    fillet_peaks = []
    for f in fillets:
        if f is None:
            continue
        ti += 1
        apex = f["tan_in"]
        best = min(range(n), key=lambda k: dist(samples[k], apex))
        fillet_peaks.append((ti, best, f["kappa"]))
        for d in range(-4, 5):
            turn_at[(best + d) % n] = ti
    print("fillet turns", ti, "peaks", [(a, b, round(c, 4)) for a, b, c in fillet_peaks])
    thr = "fillets"
    regs_sorted = []

    headings = []
    n = len(samples)
    for i in range(n):
        nxt = samples[(i + 1) % n]
        headings.append(math.degrees(compass_hdg(samples[i], nxt)) % 360.0)

    s_m = [0.0]
    for i in range(1, n):
        a, b = lls[i - 1], lls[i]
        s_m.append(s_m[-1] + hav_m(a[0], a[1], b[0], b[1]))
    # last to first not added into s of last point (s of sample 0 is 0; length is wrap)

    racing = {
        "id": "cleveland_burke_gp_1997",
        "closed": True,
        "frame": "wgs84",
        "official_length_m": OFFICIAL_M,
        "measured_length_m": round(hav_len, 2),
        "origin": {"lat": LAT0, "lon": LON0},
        "samples": [],
    }
    for i in range(n):
        racing["samples"].append({
            "s": round(s_m[i], 3),
            "lat": round(lls[i][0], 7),
            "lon": round(lls[i][1], 7),
            "heading_deg": round(headings[i], 3),
            "curvature": round(ks[i], 5),
            "target_speed_mps": round(target_speed(ks[i]), 2),
            "turn_index": turn_at[i],
        })

    # Checkpoints: S/F + each turn peak + mid-straights
    gates = [{"index": 0, "name": "Start/Finish", "lat": round(lls[0][0], 7),
              "lon": round(lls[0][1], 7), "s": 0.0, "width_m": 22.0}]
    turn_names = {
        1: "T1 Vortex (right hairpin)",
        2: "T2",
        3: "T3",
        4: "T4",
        5: "T5",
        6: "T6",
        7: "T7",
        8: "T8",
        9: "T9 (chicane)",
        10: "T10 (chicane)",
    }
    # unique first index per turn
    seen = set()
    for i, t in enumerate(turn_at):
        if t and t not in seen:
            seen.add(t)
            gates.append({
                "index": len(gates),
                "name": turn_names.get(t, f"T{t}"),
                "lat": round(lls[i][0], 7),
                "lon": round(lls[i][1], 7),
                "s": round(s_m[i], 3),
                "width_m": 18.0 if t != 1 else 28.0,
            })
    # wrapping S/F
    gates.append({
        "index": len(gates),
        "name": "Start/Finish (lap wrap)",
        "lat": round(lls[0][0], 7),
        "lon": round(lls[0][1], 7),
        "s": round(hav_len, 3),
        "width_m": 22.0,
    })
    # fix indices
    for i, g in enumerate(gates):
        g["index"] = i

    err_pct = 100.0 * (hav_len - OFFICIAL_M) / OFFICIAL_M
    xodr_text, xodr_len = write_xodr(odr, hav_len)

    PACK.mkdir(parents=True, exist_ok=True)
    (PACK / "racing_line.json").write_text(json.dumps(racing, indent=2) + "\n")
    (PACK / "checkpoints.json").write_text(json.dumps({
        "track_id": "cleveland_burke_gp_1997",
        "lap_count": 1,
        "gates": gates,
    }, indent=2) + "\n")
    (PACK / "cleveland_burke_gp.xodr").write_text(xodr_text)
    (PACK / "manifest.json").write_text(json.dumps({
        "id": "cleveland_burke_gp_1997",
        "display_name": "Cleveland Historic Circuit",
        "engine": "UE5",
        "xodr": "cleveland_burke_gp.xodr",
        "racing_line": "racing_line.json",
        "checkpoints": "checkpoints.json",
        "metadata": "metadata.json",
        "offline": True,
        "carla_required": False,
        "cesium_required": False,
    }, indent=2) + "\n")

    (PACK / "metadata.json").write_text(json.dumps({
        "identifier": "cleveland_burke_gp_1997",
        "display_name": "Cleveland Historic Circuit",
        "years": "1997-2007",
        "layout_family": "1990-2007 (remeasured 1997)",
        "official_length_mi": 2.106,
        "official_length_m": 3389,
        "measured_length_m": round(hav_len, 2),
        "measured_length_delta_pct": round(err_pct, 3) if False else round(100.0 * (hav_len - OFFICIAL_M) / OFFICIAL_M, 3),
        "turns": 10,
        "direction": "clockwise",
        "surface": "concrete",
        "airport": "Burke Lakefront",
        "airport_code": "BKL",
        "address": "1501 N Marginal Rd, Cleveland, OH",
        "lat": LAT0,
        "lon": LON0,
        "elevation_m_amsl": ELEV_M,
        "series": "Champ Car / CART",
        "branding_guardrail": "Product title is raceGPS: Cleveland Historic Circuit. Do not use event title-sponsor names in UI, HUD, or pack display_name. This citypack is not affiliated with any historical race sponsor.",
        "layout_notes": "1997–2007 configuration: official 2.106 mi / 10 turns / clockwise / flat airport circuit. 1990 permanently bypassed the bumpy 1982 T1/T2 left-right; the main straight was extended to old T3, which became T1 (the vortex). 1997 remeasured the same geometry to 2.106 mi. Pit lane is the old 1982 T1/T2 segment (extended pit exit) — modeled as metadata only, not a separate XODR road. Do not represent this pack as the 1982 ~2.48 mi layout.",
        "pit_lane": {
            "modeled_in_xodr": False,
            "notes": "Old 1982 T1/T2 left-right after the pits became the extended pit exit in 1990 (Wikipedia, gdecarli). Visual/metadata only."
        },
        "sources": [
            {"url": "https://en.wikipedia.org/wiki/Grand_Prix_of_Cleveland", "title": "Grand Prix of Cleveland (Wikipedia)"},
            {"url": "https://gdecarli.it/php2/circuit.php?var1=788&var2=2", "title": "TRACKS: CLEVELAND, BURKE LAKEFRONT (Guido de Carli)"},
            {"url": "https://www.motorsportmagazine.com/database/circuits/cleveland/", "title": "Cleveland — Motor Sport Magazine circuit database"},
            {"url": "http://www.champcarstats.com/tracks/cleveland.htm", "title": "Burke Lakefront Airport — ChampCarStats"},
            {"url": "https://commons.wikimedia.org/wiki/File:Cleveland_Street_Course_at_Burke_Lakefront_Airport.svg", "title": "Cleveland Street Course at Burke Lakefront Airport.svg (Will Pittenger, CC BY-SA 3.0)"},
            {"url": "https://historseye.wordpress.com/2020/07/23/the-greatest-races-1995-grand-prix-of-cleveland/", "title": "Histor's Eye: 1995 Grand Prix of Cleveland (T1 / T3-T4 / T9-T10)"},
            {"url": "https://www.airnav.com/airport/KBKL", "title": "AirNav: KBKL runway endpoints"},
            {"url": "https://www.openstreetmap.org/", "title": "OpenStreetMap aeroway=runway/taxiway at BKL (ODbL, Overpass 2026-08-22)"}
        ],
        "assumptions": [
            "No official surveyed racing-line polyline was available; centerline is reconstructed from OSM BKL runways 06L/24R and 06R/24L plus taxiways A, E, F, G to match published 1990–2007 diagrams (Wikipedia/Pittenger SVG, gdecarli 1990+ map notes).",
            "Clockwise: start/finish on Taxiway G heading ~238° true (SW) toward the 06 ends, matching a right-hand T1 hairpin onto 06L/24R heading ~058°.",
            "T1 vortex uses Taxiway A pavement at the SW end (old T3). The 1982 left-right on/near 06R is not driven.",
            "East complex leaves 06L near taxiway E rather than the full 24R threshold so measured length tracks the 1997 official 3389 m (a full-runway outer trace is ~3.8 km, matching the later-corrected 1990–96 published figure).",
            "T3/T4 is a right-left after joining 06L; T9/T10 is a right-left chicane on G before S/F (Histor's Eye).",
            "Driving lane total ~12 m in XODR (6 m each side of centerline) even though runways are 30–46 m wide; airport width is used as a usable lane, not the full FAA width.",
            "Elevation locked flat at 174 m AMSL (gdecarli); FAA field elevation is 583.5 ft (~178 m).",
            "Pit lane is metadata only (not a second OpenDRIVE road).",
            "Curvature sign: positive = left. Target speeds are a control envelope, not historical lap data."
        ]
    }, indent=2) + "\n")

    used_turns = sorted({t for t in turn_at if t})
    print("samples", n)
    print("haversine_m", round(hav_len, 2), f"({err_pct:+.2f}% vs {OFFICIAL_M})")
    print("xodr_len", round(xodr_len, 2), "geoms", len(odr))
    print("turns", used_turns, "count", len(used_turns), "thr", thr)
    print("gates", len(gates))
    print("closed_gap_m", dist(samples[0], samples[-1]))
    print("SF", lls[0])
    # first heading should be ~238
    print("SF heading", headings[0])
    return {
        "hav_len": hav_len,
        "err_pct": err_pct,
        "n": n,
        "turns": used_turns,
        "xodr_len": xodr_len,
        "n_geoms": len(odr),
        "n_gates": len(gates),
        "sf": lls[0],
        "sf_hdg": headings[0],
        "fillets": sum(1 for f in fillets if f),
    }


if __name__ == "__main__":
    info = main()
    Path("/tmp/cleveland_build_info.json").write_text(json.dumps(info, indent=2, default=str))
