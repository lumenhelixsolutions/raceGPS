# Sprint 1 — AI Swarm Rebalance (supersedes human-team plan)

**Same goal, same backlog, same dependency graph** as `sprint-1-plan.md`. Only the execution model changes: humans → orchestrated agent swarm. Everything already written (stories, estimates, risks, checklist) is reused.

## What changes when the team is a swarm

| Human-team assumption | Swarm reality | Planning consequence |
|---|---|---|
| 10 working days | Wall-clock = slowest chain, not calendar | Sprint = one orchestrated run in dependency waves |
| 8 available days, PTO, meetings | Agents don't sleep; limit is tokens, context window, rate limits | Capacity measured in **token budget**, not hours |
| 1 SP ≈ 6 person-hours | 1 SP ≈ one focused subagent task (~1 context) | Estimates stay valid as *task-size* units |
| ±20% load balance | Balance = parallel width vs dependency depth | Maximize parallel waves, keep critical path shallow |
| 10–15% buffer | Buffer = **retry/repair token reserve (~20%)** | Failed agent runs get retried, not "carried over" |
| New-member 50% ramp | No ramp; but first-run pipeline unknowns remain | Keep the conservative scope anyway |

## Swarm roster (7 agents + orchestrator)

| Agent | Role (from spec §7.1) | Token Budget* | Notes |
|---|---|---|---|
| **ORCHESTRATOR** | Technical Director / Producer | 200k | Decomposes, dispatches, integrates, resolves conflicts; never implements |
| **WG-1 "Sam"** | World Builder — OSM/filters | 300k | Stories #2, #5 |
| **WG-2 "Riley"** | World Builder — Osm2Odr | 400k | Story #3 (+ stretch #4) |
| **WG-3 "Jordan"** | World Builder — splines/PCG | 400k | Stories #6, #7 (+ stretch #10) |
| **TOOLS-1 "Casey"** | Tools/CI | 300k | Story #1, repo plumbing, fallback spikes |
| **ASSET-1 "Morgan"** | Graphics/Vehicle pipeline | 300k | Story #8 (CARLA export) |
| **PHYS-1 "Pat"** | Physics | 200k | Story #9 |
| **QA-1 "Quinn"** | Validation/tests | 300k | Story #14, acceptance gates on every wave |

\* Budget = prompt + context + tool I/O per sprint run. Total 2.4M + 20% retry reserve (~480k) = **~2.9M token sprint envelope**. Tune to your actual model/limits.

## Wave schedule (dependency-driven, not calendar-driven)

```
WAVE 0 (serial)   TOOLS-1: #1 repo/CI bootstrap ── gates everything
WAVE 1 (parallel) WG-1: #2 OSM fetch/filter      │ ASSET-1: #8 CARLA car export
WAVE 2 (parallel) WG-2: #3 Osm2Odr harness       │ PHYS-1: #9 arcade tires (stub car OK)
                  WG-1: #5 bridge pre-filter     │
WAVE 3 (parallel) WG-3: #6 spline import         │ QA-1: #14 validation suite
                  WG-3: #7 normal-fix tool (SS)  │
WAVE 4 (serial)   ORCHESTRATOR: integration — car + world + physics + tests
                  QA-1: acceptance gate → demo artifact
STRETCH (parallel, only if reserve intact)
                  WG-2: #4 streaming parser │ WG-3: #10 PCG city v1
```

- **Critical path: W0 → #2 → #3 → #6 → W4** (4 hops; unchanged from human plan)
- **Parallel width: up to 3 agents** in Waves 2–3 — bounded by your rate limits, not by headcount
- Wall-clock estimate: each wave ≈ one agent run; sprint ≈ 6–8 sequential runs + integration vs 10 working days for humans

## Rebalanced loads

| Agent | Committed SP | % of budget | Flag |
|---|---|---|---|
| TOOLS-1 | 5 | ~55% | Holds fallback spikes (esmini, version pins) |
| WG-1 | 6 | ~70% | ✅ |
| WG-2 | 5 | ~60% | Pre-staged stretch #4 |
| WG-3 | 8 | ~80% | ⚠️ highest load — #6+#7 sequential in one context; if #6 overruns, #7 → WG-1 |
| ASSET-1 | 5 | ~60% | ✅ |
| PHYS-1 | 5 | ~70% | Starts on stub vehicle, no idle wait for #8 |
| QA-1 | 5 | ~60% | Also gates every wave (gate failures consume retry reserve) |

Deviation max 25% — accepted: imbalance is deliberate spare capacity where failure risk lives (WG-2 parser, TOOLS-1 fallbacks).

## Swarm-specific risks (added to the 6 in the human plan)

7. **Context overflow on big artifacts** (OSM extracts, .xodr, FBX): agents pass *file paths + summaries*, never raw file contents, between waves. ORCHESTRATOR enforces.
8. **Integration drift**: parallel waves produce incompatible assumptions → Wave 0 must commit interface contracts (file formats, dir layout, naming) into the repo before Wave 1 starts.
9. **Retry storms**: a failing wave consumes reserve fast → max 2 retries per story, then escalate to human with a written failure report.
10. **Single-context overload (WG-3)**: #6 and #7 share one context; if combined prompt+IO exceeds ~70% of window, split into two runs.

## Commitment verdict

**GO** — 39 SP committed / 13 SP stretch, 20% retry reserve, parallel-width 3. Same demo definition: CARLA hero car with arcade tires, drivable on a validated real-street loop, proven by QA-1's automated gate, not by slides.
