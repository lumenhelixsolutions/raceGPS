# Sprint Plan: Sprint 1 (Mon 2026-08-17 – Fri 2026-08-28, 10 working days)

**Project:** RaceGPS — Midnight Club–Inspired Open-World Street Racing Game
**Phase:** Phase 1 — Foundation (roadmap weeks 9–10 scope, pulled forward)
**Source:** RACEGPS Technical Specification & Engineering Master Plan v1.0

---

## Sprint Goal

**Prove the world-generation pipeline end-to-end: convert one real city district from OpenStreetMap into a drivable UE5.7 level with one exported CARLA hero car driving on it.**

This de-risks the highest-impact technical risk in the spec (Section 10: "World generation fails — Critical") before any gameplay work begins.

---

## Inputs & Assumptions (Skill Defaults Applied)

| Input | Value | Basis |
|---|---|---|
| Sprint duration | 2 weeks (10 working days) | Skill default; none specified |
| Availability | 8 days/member (Sprint × 0.8) | Skill default; no PTO data provided |
| Historical velocity | None — new team, first sprint | Conservative commitment rule applied |
| Team roster | 8-person sprint team drawn from the 36-person org (spec §7.1) | Full 36-person org is not sprint-planable; scoped to Phase 1 roles |
| Estimates | Derived from spec §4/§8 engineering detail | Skill estimation reference table |

---

## Team Capacity

Effective hours/day = 6, focus factor = 0.8 → **38.4 hrs/person**. Planning conversion: 1 SP ≈ 6 hrs → **6.4 SP/person**.

| Member | Role (spec §7.1) | Available Days | Capacity (hrs) | Capacity (SP) |
|---|---|---|---|---|
| Alex | Technical Director (Sprint Lead) | 8 | 38.4 | 6.4 |
| Sam | World Builder Engineer 1 | 8 | 38.4 | 6.4 |
| Riley | World Builder Engineer 2 | 8 | 38.4 | 6.4 |
| Jordan | World Builder Engineer 3 | 8 | 38.4 | 6.4 |
| Casey | Tools Engineer | 8 | 38.4 | 6.4 |
| Morgan | Senior Graphics Engineer | 8 | 38.4 | 6.4 |
| Pat | Senior Physics Engineer | 8 | 38.4 | 6.4 |
| Quinn | QA Engineer | 8 | 38.4 | 6.4 |
| **Total** | | | **307.2** | **51.2** |

- Total Team Capacity: **307 person-hours (~51 SP)**
- Reference Velocity: **N/A** (no history) → conservative target = 70% of capacity ≈ 36 SP
- This Sprint commitment: **39 SP (76% of capacity)** + 13 SP stretch → **~24% buffer** retained for new-team ramp-up

---

## Backlog & Story Assignments

Derived from spec §4.1 (World Generation), §4.2 (Vehicle System), and §8 (Day-1 Critical Bugs — mitigations planned as stories).

### Committed Scope (39 SP)

| # | Story | Priority | Points | Owner | Dependencies | Status |
|---|---|---|---|---|---|---|
| 1 | Repo, CI/CD & build pipeline bootstrap (UE5.7 + Perforce/Git LFS) | P0 | 5 | Alex | None | Committed |
| 2 | OSM fetch + road-type filter service (motorway→residential, per spec §4.1.1) | P0 | 3 | Sam | #1 | Committed |
| 3 | carla.Osm2Odr conversion harness + course validation stub (Python, spec §4.1.1) | P0 | 5 | Riley | #2 (SS) | Committed |
| 5 | Bridge/overpass pre-filter for OSM data (Bug #3 mitigation) | P0 | 3 | Casey | #2 | Committed |
| 6 | OpenDRIVE → UE5 spline import via LandscapeCombinator | P0 | 5 | Jordan | #3 (FS) | Committed |
| 7 | Spline inverted-normal auto-fix tool (Bug #4 mitigation) | P0 | 3 | Sam | #6 (SS) | Committed |
| 8 | CARLA hero vehicle export → RaceGPS import (1 car, skeletal mesh + LODs, spec §4.2.1) | P0 | 5 | Morgan | #1 | Committed |
| 9 | Vehicle physics materials + arcade tire baseline (Bug #5 mitigation, spec §4.2.2) | P0 | 5 | Pat | #8 (FS) | Committed |
| 14 | Automated validation test suite (OpenDRIVE connectivity, length, loop checks) | P0 | 5 | Quinn | #3 (SS) | Committed |

### Stretch Scope (13 SP — pull only if committed scope is Done)

| # | Story | Priority | Points | Owner | Dependencies |
|---|---|---|---|---|---|
| 4 | Streaming OSM parser for >50 MB files (Bug #1 mitigation) | P1 | 5 | Casey + Riley | #3 |
| 10 | PCG city population v1 — buildings + streetlights along splines (spec §4.1.2) | P1 | 8 | Jordan + Sam | #6 |

Deferred to Sprint 2: #11 PCG road-overlap constraint (3 SP, needs #10), #12 Lumen/Nanite validation scene (3 SP), #13 Course Validation Service spike (3 SP, backend — team not yet in sprint roster).

**INVEST check passed** on all committed stories; no story exceeds 8 SP (13+ SP stories split per skill rules).

---

## Workload Distribution

| Member | Assigned SP | Load % (vs 6.4 SP) | Status |
|---|---|---|---|
| Alex | 5 | 78% | ✅ Healthy |
| Sam | 6 | 94% | ⚠️ High — no buffer |
| Riley | 5 | 78% | ✅ Healthy |
| Jordan | 5 | 78% | ✅ Healthy |
| Casey | 3 | 47% | ⚠️ Low — see note |
| Morgan | 5 | 78% | ✅ Healthy |
| Pat | 5 | 78% | ✅ Healthy |
| Quinn | 5 | 78% | ✅ Healthy |
| **Avg** | **4.9** | **76%** | |

**Rebalancing notes:**
- *Casey (47%):* under-loaded on paper, but pre-staged as pair on #3 and first puller of stretch #4. This is deliberate — Tools holds the contingency capacity for the known OSM memory-overflow risk. Deviation 29% > 20% threshold → **flagged, accepted with mitigation** (stretch work pre-assigned).
- *Sam (94%):* no buffer. If #7 (normal fix) expands, #7 moves to Casey; do not add scope to Sam.
- No member carries >40% of total SP (max is 15%). ✅

---

## Dependency Analysis

| Story | Depends On | Depended On By | Type | Risk |
|---|---|---|---|---|
| 1. CI/CD bootstrap | None | All | — | Low |
| 2. OSM fetch/filter | #1 | #3, #5 | FS | Low |
| 3. Osm2Odr harness | #2 (SS) | #6, #14, #4 | FS | **Med** — known memory overflow on large files (Bug #1) |
| 5. Bridge pre-filter | #2 | #3 (feeds) | SS | Low |
| 6. Spline import | #3 | #7, #10 | FS | **Med** — LandscapeCombinator version compatibility unverified |
| 7. Normal fix tool | #6 (SS) | #10 | FS | Low — mitigation code exists in spec §8 |
| 8. CARLA car export | #1 | #9 | FS | **Med** — external asset pack (CC-BY 4.0) download/licensing check |
| 9. Physics materials | #8 | None | FS | Low |
| 14. Validation suite | #3 (SS) | All (quality gate) | SS | Low |

**No circular dependencies. ✅**

### Critical Path

```
#1 CI/CD → #2 OSM fetch → #3 Osm2Odr harness → #6 Spline import → [stretch #10 PCG]
```

- Longest chain: 4 stories, all on separate owners (Alex → Sam → Riley → Jordan) — no single point of failure. ✅
- Parallel track: #8 → #9 (Morgan → Pat) must land by Day 8 for the end-of-sprint demo (car driving on generated roads).

### External Dependencies (flagged)

| Dependency | Risk | Mitigation |
|---|---|---|
| OpenStreetMap API rate limits | Medium | Cache OSM extracts locally after first fetch; use Geofabrik mirrors |
| CARLA 0.10.0 UE5 asset pack availability | Medium | Verify download + CC-BY 4.0 attribution file on Day 1 (Alex) |
| LandscapeCombinator plugin vs UE5.7 | Medium | Version spike on Day 1–2; fallback = esmini RoadManager (spec Bug #3 mitigation) |

---

## Risks & Notes

1. **No velocity history (new team, new stack).** Commitment is deliberately at 76% of capacity with 13 SP of pre-groomed stretch work. Recalibrate velocity after Sprint 1 review.
2. **Bug #1 (OSM memory overflow) is the biggest technical unknown.** Mitigation: sprint uses a small district extract (<10 MB) first; streaming parser is stretch, not committed, so a miss doesn't break the sprint goal.
3. **UE5.7 early-adoption risk.** Pin engine version in repo on Day 1; no mid-sprint upgrades.
4. **Sam at 94% load** — escalation path: #7 transfers to Casey if it grows beyond 3 SP.
5. **Demo definition (Sprint Review):** one CARLA hero car, arcade tire config applied, drivable on a real imported street network with validated loop — shown live, not on slides.
6. **Spike policy:** any story that blows its estimate by >50% converts to a Spike and exits velocity (per skill guidance).

---

## Final Commitment Checklist

- [x] Total SP within 80–100% of reference velocity? — N/A (no velocity); 76% of capacity used instead, with 24% buffer ✅ (exceeds the 10–15% buffer minimum)
- [x] Each member's load 60–90%? — 6 of 8 ✅; Sam 94% ⚠️ (mitigation noted); Casey 47% ⚠️ (mitigation noted)
- [ ] Load deviation ≤ 20%? — ❌ Casey deviation 29% → **accepted with explicit mitigation** (stretch pre-assignment)
- [x] Circular dependencies? — None
- [x] External dependencies flagged? — 3 flagged with mitigations (OSM API, CARLA pack, LandscapeCombinator)
- [x] Critical path staffed? — Yes, spread across 4 owners
- [x] 10–15% buffer reserved? — 24% reserved ✅
- [x] Any story >13 SP? — No (max 8 SP)
- [x] Any member >40% of total SP? — No (max 15%)

**Commitment verdict: GO** — commit to 39 SP, hold 13 SP stretch, one accepted deviation (Casey) with mitigation in place.
