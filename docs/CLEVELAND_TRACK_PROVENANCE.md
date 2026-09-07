# Cleveland Historic Circuit — track provenance

**Product name:** raceGPS: Cleveland Historic Circuit  
**Pack id:** `cleveland_burke_gp_1997`  
**Place:** Burke Lakefront Airport (BKL), 1501 N Marginal Rd, Cleveland, Ohio  
**Coords:** 41.51722 N, 81.68306 W (41°31′02″N 81°40′59″W)  
**Lake Erie** north, downtown Cleveland skyline south. Flat airport circuit.

This pack is **not** a title-sponsor revival and is **not** affiliated with any historical event sponsor. HUD / menu copy must stay **raceGPS: Cleveland Historic Circuit**.

## Three lengths, two geometries

| Era | Official length | Turns | What actually changed |
|---|---|---|---|
| 1982–1989 (practice 1990) | ~2.48 mi / 3.991 km (gdecarli also lists 2.485 mi) | 12 (Encyclopedia of Cleveland History: 8 right, 4 left) | Original CART airport course. After the start, a **left-right kink (T1/T2)** sat on a bumpy section. |
| 1990 race – 1996 | **Published** 2.369 mi / 3.812–3.813 km | 10 | After practice, T1/T2 was bypassed. Main straight extended to **old T3**, which became **new T1** (the vortex). The deleted segment became an **extended pit exit**. Geometry then frozen. |
| **1997–2007 (this pack)** | **2.106 mi / 3.389 km** | **10, clockwise** | **Remeasured only.** Wikipedia, gdecarli, Motor Sport Magazine, and ChampCarStats all state there was **no visible layout change** vs 1990. gdecarli labels the 1990–96 figure as the wrong official length. |

**Do not** draw the 1997 centerline as the 1982 ~2.48 mi course. **Do not** put 1982 T1/T2 on the driven racing line.

## T1 vortex

Wikipedia: at the end of the front straight the runway narrowed into an almost **135°** right-hander. On rolling starts cars fanned out across the wide concrete and were “sucked” to the apex. German-language circuit notes describe a hard brake and a sharp right. Histor’s Eye (1995 race report, same 1990+ geometry) calls T1 a **tight right-hand hairpin**, T3/T4 a **right-left**, and T9/T10 a **fast right-left chicane** that ended the lap.

gdecarli: start/finish straight now ends directly at the right hairpin; old track forms the longer pit lane.

## How the centerline was derived

No authoritative GPS/telemetry polyline was fetched (Wikimedia SVG binaries were rate-limited 429; racingcircuits.info / the-fastlane map pages timed out or were image-only). Reconstruction:

1. **OSM Overpass** (2026-08-22, ODbL) `aeroway=runway|taxiway` at BKL.
   - **06L/24R** (north / lake): 41.514138, −81.692047 → 41.523767, −81.671639, width 46 m, ~2013 m.
   - **06R/24L** (south): 41.512760, −81.691587 → 41.520220, −81.675721, width 30 m, ~1584 m.
   - **Taxiway G** parallel south of 06R (pit / S/F pavement).
   - **Taxiway A** SW connectors (06R ↔ 06L) = T1 / old pit-exit pad.
   - **Taxiway E/F** east connectors.
2. True runway heading **058° / 238°** (AirNav KBKL / SkyVector).
3. **Clockwise** driven line: Taxiway G heading ~SW (238°) to T1, right hairpin on Taxiway A onto **06L/24R** heading NE (058°), east box near taxiway E back to G, T9/T10 chicane, S/F.
4. Corners filleted (circular arcs) on a closed polyline; OpenDRIVE planView is those line+arc segments. Reference line = centerline. Elevation **174 m AMSL** (gdecarli; FAA field elev ~583 ft).
5. Length target is the **1997 official 3389 m**, not a full-runway outer barrier trace. Tracing G and 06L all the way to 24R/taxiway H is ~3.8 km — the same ballpark as the later-corrected 2.369 mi publication. The racing centerline therefore leaves 06L near **taxiway E** and uses an inner line on G.

Pit lane: **metadata only** (old 1982 T1/T2 = extended pit exit). Not a second XODR road.

## Measured vs official

| | mi | m |
|---|---|---|
| Official 1997–2007 | 2.106 | 3389 |
| This pack (consecutive haversine on `racing_line.json`) | — | **3275.82** (**−3.34%**) |
| OpenDRIVE planView sum | — | 3280.28 |

Tolerance in `tests/test_cleveland_circuit.py` is **5%** of 3389 m.

## Sources

- https://en.wikipedia.org/wiki/Grand_Prix_of_Cleveland
- https://gdecarli.it/php2/circuit.php?var1=788&var2=2
- https://www.motorsportmagazine.com/database/circuits/cleveland/
- http://www.champcarstats.com/tracks/cleveland.htm
- https://commons.wikimedia.org/wiki/File:Cleveland_Street_Course_at_Burke_Lakefront_Airport.svg (gray = pre-1990)
- https://historseye.wordpress.com/2020/07/23/the-greatest-races-1995-grand-prix-of-cleveland/
- https://www.airnav.com/airport/KBKL
- OpenStreetMap BKL aeroways (ODbL)

Regenerate pack files: `python3 scripts/build_cleveland_circuit.py` from this showcase tree.
