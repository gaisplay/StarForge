# A/B: the release's deferred let-go and exact final pose, against the F4 build the regression replayed

Runs: 113. DIFF 26, SAME 87

## `2026-09-18-place-roll-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.1 -4.7) ship, 0.0 m/s, fwd 77.6 deg, up 77.6 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.0 -4.7) ship, 0.0 m/s, fwd 77.2 deg, up 77.2 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (155.1 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (702586 0 0) got (849572 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (154.8 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (709014 0 0) got (857108 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.1 pitch -21.8 deg | aim yaw 11.1 pitch -21.8 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.2 pitch -21.7 deg | aim yaw 11.2 pitch -21.7 deg | ON TARGET`

## `2026-09-18-place-thrust-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.3 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18478016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.2 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18482016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (3546 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (3542 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-18-place-thrust-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (28 -> 28)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.1 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.6 m at (-7.0 +2.9 +0.0) ship, 14.8 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (25 -> 25)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18494073 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.1 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18520740 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (25 -> 25)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (3542 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (3517 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-19-checkpoint-8-B6-roll-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.1 -4.7) ship, 0.0 m/s, fwd 77.6 deg, up 77.6 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.0 -4.7) ship, 0.0 m/s, fwd 77.2 deg, up 77.2 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (155.1 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (702586 0 0) got (849572 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (154.8 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (709014 0 0) got (857108 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.1 pitch -21.8 deg | aim yaw 11.1 pitch -21.8 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.2 pitch -21.7 deg | aim yaw 11.2 pitch -21.7 deg | ON TARGET`

## `2026-09-22-f2fix-F1-60` -- DIFF (regression said VARIES, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 1}
- **Combat:** (0 -> 30)
  - 1: `None`
  - 0: `LogStarForge: Combat: [Ship] shields +X 1000 -X 1000 +Y 1000 -Y 1000 +Z 1000 -Z 1000, hull 2000.0/2000.0, thrusters 24/24, powered up.`
- **Dock:** (0 -> 33)
  - 1: `None`
  - 0: `LogStarForge: Display: Dock: [Ship] docked.`
- **Docking port on ** (0 -> 2)
  - 1: `None`
  - 0: `LogStarForge: Display: Docking port on Station at hull-local (0 -600 -265) cm, out along (0.00 -1.00 0.00), deck up (0.00 0.00 1.00).`
- **Docking:** (0 -> 1)
  - 1: `None`
  - 0: `LogStarForge: Display: Docking: [Ship] station found, window 3.0 m, 1.0 m/s, fwd 10 deg, up 10 deg, capture 1.0 s.`
- **Drift debug** (0 -> 3)
  - 1: `None`
  - 0: `LogStarForge: Drift debug [CHANGED]: calls/frame 1 peak 1, dt 0.0167, mode Walking, base Floor, ship-local (-0 6 -300) dist 300 cm, settled 0.0 s, measured 0.0 s, ship gravity 1`
- **Drift debug:** (0 -> 29)
  - 1: `None`
  - 0: `LogStarForge: Drift debug: calls/frame 1 peak 2, dt 0.0167, mode Walking, base Floor, ship-local (0 0 -300) dist 300 cm, settled 1.0 s, measured 0.5 s, ship gravity 1`

## `2026-09-22-physics-dockcycle-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (44 -> 44)
  - 1: `LogStarForge: Display: Dock: [Ship] docked.`
  - 0: `LogStarForge: Dock: [Ship] capturing 1.0 of 1.0 s`
- **Flight:** (39 -> 39)
  - 1: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 8.8 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 17.5 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`

## `2026-09-22-physics-dockcycle-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (45 -> 45)
  - 1: `LogStarForge: Display: Dock: [Ship] docked.`
  - 0: `LogStarForge: Dock: [Ship] capturing 1.0 of 1.0 s`
- **Flight:** (40 -> 40)
  - 1: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 17.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 16.4 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (40 -> 40)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2598 410 513) cm firing 1 | bearing yaw 28.5 pitch 4.5 deg | aim yaw 28.5 pitch 4.5 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2598 409 513) cm firing 1 | bearing yaw 28.5 pitch 4.5 deg | aim yaw 28.5 pitch 4.5 deg | ON TARGET`

## `2026-09-22-physics-F1-slow-intrusion-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 2}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 2}
- **Drift debug:** (28 -> 28)
  - 1: `LogStarForge: Drift debug: calls/frame 1 peak 2, dt 0.0025, mode Walking, base Floor, ship-local (0 0 -300) dist 300 cm, settled 1.0 s, measured 0.5 s, ship gravity 1`
  - 0: `LogStarForge: Drift debug: calls/frame 1 peak 2, dt 0.0025, mode Walking, base Floor, ship-local (-0 0 -300) dist 300 cm, settled 1.0 s, measured 0.5 s, ship gravity 1`
- **Flight:** (29 -> 29)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.7 m/s, accel 0.2 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 9593 0) got (0 10781 0) N | torque want (0 0 -0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.7 m/s, accel 0.2 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 9647 0) got (0 10842 0) N | torque want (0 0 -0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Footing LOST at ** (2 -> 2)
  - 1: `LogStarForge: Display: Footing LOST at t=5.16 s, ship-local (-0 0 -300) cm: felt 281.99 m/s2 over the 19.62 limit, ship turning 2.8 deg/s; falling, walking refused, still carried.`
  - 0: `LogStarForge: Display: Footing LOST at t=5.17 s, ship-local (-0 0 -300) cm: felt 281.99 m/s2 over the 19.62 limit, ship turning 2.8 deg/s; falling, walking refused, still carried.`
- **Shipboard drift:** (2 -> 2)
  - 1: `LogStarForge: Shipboard drift: 20.0 s settled, drift now 25.509 cm, peak drift 25.599 cm, peak jitter 2.148 cm/tick, skipped carries 0, knocked down 1.4 s -- KNOCKED DOWN: the drift is his slide, not the carry`
  - 0: `LogStarForge: Shipboard drift: 20.0 s settled, drift now 25.512 cm, peak drift 25.602 cm, peak jitter 2.148 cm/tick, skipped carries 0, knocked down 1.4 s -- KNOCKED DOWN: the drift is his slide, not the carry`
- **Turret:** (29 -> 29)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2295 724 661) cm firing 1 | bearing yaw 38.4 pitch 7.4 deg | aim yaw 38.4 pitch 7.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2295 723 661) cm firing 1 | bearing yaw 38.4 pitch 7.4 deg | aim yaw 38.4 pitch 7.4 deg | ON TARGET`

## `2026-09-22-physics-loop-roll-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.1 -4.7) ship, 0.0 m/s, fwd 77.6 deg, up 77.6 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.0 -4.7) ship, 0.0 m/s, fwd 77.2 deg, up 77.2 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (155.1 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (702586 0 0) got (849572 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (154.8 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (709014 0 0) got (857108 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.1 pitch -21.8 deg | aim yaw 11.1 pitch -21.8 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.2 pitch -21.7 deg | aim yaw 11.2 pitch -21.7 deg | ON TARGET`

## `2026-09-22-physics-loop-roll-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (28 -> 28)
  - 1: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.0 -4.7) ship, 0.0 m/s, fwd 77.1 deg, up 77.1 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 6.7 m at (-0.0 -4.7 -4.7) ship, 0.0 m/s, fwd 74.5 deg, up 74.5 deg -- out of window: distance`
- **Flight:** (25 -> 25)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (155.5 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (727906 0 0) got (859007 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (153.9 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (773450 0 0) got (910872 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (25 -> 25)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -329 351) cm firing 1 | bearing yaw 11.2 pitch -21.7 deg | aim yaw 11.2 pitch -21.7 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -329 351) cm firing 1 | bearing yaw 11.9 pitch -21.2 deg | aim yaw 11.9 pitch -21.2 deg | ON TARGET`

## `2026-09-23-review-undock-sideways-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (28 -> 28)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.1 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.6 m at (-7.0 +2.9 +0.0) ship, 14.8 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (25 -> 25)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18494073 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.1 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18520740 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (25 -> 25)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (3542 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (3517 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-23-review-undock-straight-off-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (14 -> 14)
  - 1: `LogStarForge: Dock: [Ship] free | port 33.5 m at (-0.0 +33.5 +0.0) ship, 31.3 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 33.4 m at (-0.0 +33.4 +0.0) ship, 31.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (11 -> 11)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 -1.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 -18478016 0) got (0 -800000 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 -1.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.2 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 -18482016 0) got (0 -800000 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (11 -> 11)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -1054 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -1050 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-23-review-undock-straight-off-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (15 -> 15)
  - 1: `LogStarForge: Dock: [Ship] free | port 10.2 m at (-0.0 +10.2 +0.0) ship, 15.1 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 9.9 m at (-0.0 +9.9 +0.0) ship, 14.8 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (12 -> 12)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 -1.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 -18494073 0) got (0 -800000 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 -1.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.1 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 -18520740 0) got (0 -800000 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (12 -> 12)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -1052 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -1027 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-25-f3fix-b1-near-the-border-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 1, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 1, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (42 -> 42)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.3 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (39 -> 39)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 15.6 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18478016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.2 m/s, accel 16.4 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18482016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (39 -> 39)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (4300000003546 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (4300000003542 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-25-f3fix-b1-near-the-border-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 1, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 1, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (43 -> 43)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.1 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.6 m at (-7.0 +2.9 +0.0) ship, 14.8 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (40 -> 40)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18494073 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.1 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18520740 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (40 -> 40)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (4300000003542 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (4300000003517 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-25-f3fix-dockcycle-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (44 -> 44)
  - 1: `LogStarForge: Display: Dock: [Ship] docked.`
  - 0: `LogStarForge: Dock: [Ship] capturing 1.0 of 1.0 s`
- **Flight:** (39 -> 39)
  - 1: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 8.8 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 17.5 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`

## `2026-09-25-f3fix-dockcycle-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (45 -> 45)
  - 1: `LogStarForge: Display: Dock: [Ship] docked.`
  - 0: `LogStarForge: Dock: [Ship] capturing 1.0 of 1.0 s`
- **Flight:** (40 -> 40)
  - 1: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 17.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console (standing down) | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 16.4 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (40 -> 40)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2598 410 513) cm firing 1 | bearing yaw 28.5 pitch 4.5 deg | aim yaw 28.5 pitch 4.5 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2598 409 513) cm firing 1 | bearing yaw 28.5 pitch 4.5 deg | aim yaw 28.5 pitch 4.5 deg | ON TARGET`

## `2026-09-25-f3fix-down-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-0.0 +2.9 +7.3) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-0.0 +2.9 +7.2) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 -1.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 -18478016) got (0 0 -800000) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 -1.00) ang (0.00 0.00 0.00) assist 1 | speed 15.2 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 -18482016) got (0 0 -800000) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 -376) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 -372) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-25-f3fix-down-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (28 -> 28)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-0.0 +2.9 +7.2) ship, 15.1 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.6 m at (-0.0 +2.9 +7.0) ship, 14.8 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (25 -> 25)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 -1.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 -18494073) got (0 0 -800000) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 -1.00) ang (0.00 0.00 0.00) assist 1 | speed 15.1 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 0 -18520740) got (0 0 -800000) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (25 -> 25)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -329 -372) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -329 -347) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-25-f3fix-f1-intrusion-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 2}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 2}
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.7 m/s, accel 0.2 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 9593 0) got (0 10781 0) N | torque want (0 0 -0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.7 m/s, accel 0.2 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 9647 0) got (0 10842 0) N | torque want (0 0 -0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Footing LOST at ** (2 -> 2)
  - 1: `LogStarForge: Display: Footing LOST at t=5.16 s, ship-local (-0 0 -300) cm: felt 281.99 m/s2 over the 19.62 limit, ship turning 2.8 deg/s; falling, walking refused, still carried.`
  - 0: `LogStarForge: Display: Footing LOST at t=5.17 s, ship-local (-0 0 -300) cm: felt 281.99 m/s2 over the 19.62 limit, ship turning 2.8 deg/s; falling, walking refused, still carried.`
- **Shipboard drift:** (2 -> 2)
  - 1: `LogStarForge: Shipboard drift: 20.0 s settled, drift now 25.509 cm, peak drift 25.599 cm, peak jitter 2.148 cm/tick, skipped carries 0, knocked down 1.4 s -- KNOCKED DOWN: the drift is his slide, not the carry`
  - 0: `LogStarForge: Shipboard drift: 20.0 s settled, drift now 25.512 cm, peak drift 25.602 cm, peak jitter 2.148 cm/tick, skipped carries 0, knocked down 1.4 s -- KNOCKED DOWN: the drift is his slide, not the carry`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2295 724 661) cm firing 1 | bearing yaw 38.4 pitch 7.4 deg | aim yaw 38.4 pitch 7.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2295 723 661) cm firing 1 | bearing yaw 38.4 pitch 7.4 deg | aim yaw 38.4 pitch 7.4 deg | ON TARGET`

## `2026-09-25-f3fix-f1-intrusion-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 1}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 1}
- **Flight:** (25 -> 25)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.7 m/s, accel 0.2 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 9789 0) got (0 10839 0) N | torque want (0 0 -0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 0.7 m/s, accel 0.2 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (0 10164 0) got (0 11255 0) N | torque want (0 0 -0) got (0 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Footing LOST at ** (1 -> 1)
  - 1: `LogStarForge: Display: Footing LOST at t=5.18 s, ship-local (-0 0 -300) cm: felt 42.01 m/s2 over the 19.62 limit, ship turning 2.8 deg/s; falling, walking refused, still carried.`
  - 0: `LogStarForge: Display: Footing LOST at t=5.20 s, ship-local (-0 0 -300) cm: felt 42.01 m/s2 over the 19.62 limit, ship turning 2.8 deg/s; falling, walking refused, still carried.`
- **Footing regained** (1 -> 1)
  - 1: `LogStarForge: Display: Footing regained at t=5.68 s after 0.5 s down, ship-local (1 8 -302) cm; landing is the engine's again.`
  - 0: `LogStarForge: Display: Footing regained at t=5.70 s after 0.5 s down, ship-local (1 8 -302) cm; landing is the engine's again.`
- **Turret:** (25 -> 25)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -284 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -286 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-25-f3fix-roll-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.1 -4.7) ship, 0.0 m/s, fwd 77.6 deg, up 77.6 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.0 -4.7) ship, 0.0 m/s, fwd 77.2 deg, up 77.2 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (155.1 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (702586 0 0) got (849572 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (154.8 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (709014 0 0) got (857108 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.1 pitch -21.8 deg | aim yaw 11.1 pitch -21.8 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2818 -326 352) cm firing 1 | bearing yaw 11.2 pitch -21.7 deg | aim yaw 11.2 pitch -21.7 deg | ON TARGET`

## `2026-09-25-f3fix-roll-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (28 -> 28)
  - 1: `LogStarForge: Dock: [Ship] free | port 6.9 m at (-0.0 -5.0 -4.7) ship, 0.0 m/s, fwd 77.1 deg, up 77.1 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 6.7 m at (-0.0 -4.7 -4.7) ship, 0.0 m/s, fwd 74.5 deg, up 74.5 deg -- out of window: distance`
- **Flight:** (25 -> 25)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (155.5 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (727906 0 0) got (859007 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (0.00 0.00 0.00) ang (1.00 0.00 0.00) assist 1 | speed 0.0 m/s, accel 0.0 m/s2, ang roll/pitch/yaw (153.9 0.0 0.0) deg/s | force want (0 0 0) got (0 0 0) N | torque want (773450 0 0) got (910872 0 0) Nm | throttles 0 of 24 commanded, 0 spooled`
- **Turret:** (25 -> 25)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -329 351) cm firing 1 | bearing yaw 11.2 pitch -21.7 deg | aim yaw 11.2 pitch -21.7 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (2819 -329 351) cm firing 1 | bearing yaw 11.9 pitch -21.2 deg | aim yaw 11.9 pitch -21.2 deg | ON TARGET`

## `2026-09-25-f3fix-sideways-400` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.3 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18478016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.2 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18482016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (3546 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (3542 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-25-f3fix-sideways-60` -- DIFF (regression said CHANGED, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (28 -> 28)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.1 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.6 m at (-7.0 +2.9 +0.0) ship, 14.8 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (25 -> 25)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18494073 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.1 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18520740 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (25 -> 25)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (3542 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (3517 -329 351) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## `2026-09-26-owner-B1-release-sideways-400` -- DIFF (regression said SAME, log closed: True)
- red now {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- red B   {'Error': 0, 'Warning': 0, 'skipped>0': 0, 'NOT RUNNING': 0, 'CARRY FAILED': 0, 'cross-check': 0, 'handover REFUSED': 0, 'Footing LOST': 0}
- **Dock:** (27 -> 27)
  - 1: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.3 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
  - 0: `LogStarForge: Dock: [Ship] free | port 7.8 m at (-7.2 +2.9 +0.0) ship, 15.2 m/s, fwd 0.0 deg, up 0.0 deg -- out of window: distance`
- **Flight:** (24 -> 24)
  - 1: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.3 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18478016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
  - 0: `LogStarForge: Flight: [Ship] flown by console | intent lin (1.00 0.00 0.00) ang (0.00 0.00 0.00) assist 1 | speed 15.2 m/s, accel 16.0 m/s2, ang roll/pitch/yaw (0.0 0.0 0.0) deg/s | force want (18482016 0 0) got (800000 0 0) N | torque want (0 0 0) got (0 0 0) Nm | throttles 4 of 24 commanded, 4 spooled`
- **Turret:** (24 -> 24)
  - 1: `LogStarForge: Turret: [Ship] gunned by automation | target (3546 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`
  - 0: `LogStarForge: Turret: [Ship] gunned by automation | target (3542 -326 352) cm firing 1 | bearing yaw 18.9 pitch 1.4 deg | aim yaw 18.9 pitch 1.4 deg | ON TARGET`

## Note added by the main session, 2026-09-28

`2026-09-22-f2fix-F1-60` is listed above against its first replay, `now`, which the regression
itself had found truncated at 964 lines (`Engine exit requested (reason: ConsoleCtrl RequestExit)`),
so almost every family differs there. Compared instead with that command's complete second replay,
`again`, the build with the let-go fix differs only by the one-frame shift: the station-bump
knockdown stays `felt 42.01 m/s2`, at `t=5.18 s` before and `t=5.20 s` after (one frame at 60 fps),
`Footing regained` 5.68 -> 5.70 s, and `Footing LOST` counts 1 and 1. So all 26 changed runs are
the undock runs, each shifted by one frame, and no red counter moved in any of them.
