> Перенесено из рабочего каталога этапа (`.superpowers/`, в репозиторий не входит) при уборке
> 2026-09-17. Это первоисточник записи в `CLAUDE.md` про чтение угловой скорости у физического
> тела: там приведён вывод, здесь — как он получен. Сырые прогоны диагностики (14 журналов,
> 12 МБ) **невоспроизводимы** — переменная `sf.OmegaDebug`, которой они сняты, из кода удалена;
> они лежат в архиве рабочего каталога рядом с проектом, а не в репозитории.

# The two angular rates: diagnosis, cause, fix

Branch `feat/combat-and-the-pirate`, from `2e37c06`. The disagreement between the `Flight:`
readout (`120.0 deg/s`) and `USFShipFrameComponent::GetAngularVelocityRadPerSec` (146.7 to
152.1 deg/s at the knockdown instants) is **established, not hypothesised**. The differenced
instrument was wrong. It is fixed, and the 400 fps pitch row is back to `0.000 cm` and
`footing intact`.

---

## 1. The cause, in one paragraph

Every frame, the engine copies the simulated hull's pose onto its component **through an
`FRotator`**: `USceneComponent::InternalSetWorldLocationAndRotation` converts the body's
quaternion to a rotator, stores that, and then rebuilds the component's world transform from the
*rotator* — its own comment (SceneComponent.cpp 3321-3327) insists that it must
("it is important to compute the quaternion from the rotator and not the opposite"). And
`FQuat::Rotator()` does not answer honestly near gimbal lock: inside a window of **0.08103°**
either side of pitch = ±90 it discards the quaternion and returns **exactly ±90, roll exactly
zero** (UnrealMath.cpp 654-669, `SINGULARITY_THRESHOLD = 0.4999995`). So the actor transform the
frame component samples carries up to 0.081° of **position** error at one attitude. A rate
obtained by differencing divides that fixed position error by the frame time, which makes it
**4.9 deg/s at 60 fps and 32.4 deg/s at 400** — the project's oldest defect shape, a figure that
is really a frame rate. At r = 3 m the grav-plate ceiling of 19.62 m/s² is crossed at
146.5 deg/s, 26.5 deg/s above the true 120, so only the 400 fps end of the error can reach it.

Hypothesis (c) — that the hull oscillates and the once-a-second `Flight:` line aliases it — is
**destroyed**: Chaos reported `119.999355 deg/s` on every single frame across every episode,
including the offending ones, and the attitude one frame after the spike is exactly where the
unperturbed trajectory puts it (0.366626° + 0.233365° = 0.599991° = two ordinary steps). Not one
degree is gained or lost. Hypothesis (a) is half right — the sampled transform is not the body's —
but it is not *stale*: it advances every tick, and it is *quantised*. (b) is out: the interval
differenced is exactly `DeltaTime`.

## 2. How it was measured

A temporary per-frame diagnostic in `USFShipFrameComponent::TickComponent`, behind
`sf.OmegaDebug`, logging on the same tick: the differenced rate, the delta angle, Chaos's own
`GetPhysicsAngularVelocityInDegrees`, the tick's `DeltaTime`, the actor quaternion at 15 decimals,
the **body's** quaternion from `FBodyInstance::GetUnrealWorldTransform`, and the root's
`FRotator`. Spike rig, `sf.ShipProfile 0, sf.Intent.Pitch 1`, 30 s at 400 fps and 30 s at 60 fps.

The 400 fps run reproduced the episode exactly: eleven `Footing LOST`, 11.70 s to 26.70 s, felt
19.64 → 21.12, turning 146.7 → 152.1 — line for line the prior wave's `final-fix-pitchlong-400.log`,
so the diagnostic perturbs nothing.

**The frame the footing is lost on** (`diag-omega-pitch-400-body.log`, frames 4678-4682):

| frame | body quaternion (Y, W) | component's rotator | differenced rate |
|---|---|---|---|
| 4679 | -0.709365487 / 0.704840839 | P 89.633374062 (Y 180, R 180) | 119.997 |
| **4680** | **-0.707517803 / 0.706695497** | **P 90.000000000 (Y -0, R 0)** | **146.650** |
| 4681 | -0.705665290 / 0.708545327 | P 89.766634863 (Y 0, R 0) | 93.346 |

The body is at 90.0666° of pitch. The component is at exactly 90.0000° — its world quaternion
reads `(0, -0.707106781186547, 0, 0.707106781186548)`, the two doubles either side of √½, which
is not something that happens by chance. The 0.0666° the snap removed is added to the step before
it and taken off the step after: 146.650 + 93.346 = 239.996 = 2 × 120.

**Every snap in the 30 s run**, one per half turn (600 frames at 400 fps), from the moment the
hull reaches rate:

```
t= 2.7025 excess 0.055452 deg -> 142.18 deg/s      t=16.2025 excess 0.070723 -> 148.29
t= 4.2025        0.059871       143.95             t=17.7025        0.072071    148.83
t= 5.7025        0.061228       144.49             t=19.2025        0.073447    149.38
t= 7.2025        0.062595       145.04             t=20.7025        0.074814    149.92
t= 8.7025        0.063937       145.57             t=22.2025        0.076161    150.46
t=10.2025        0.065261       146.10             t=23.7025        0.077538    151.01
t=11.7025        0.066628       146.65  <- first over the 146.5 ceiling
t=13.2025        0.068023       147.21             t=25.2025        0.078895    151.56
t=14.7025        0.069376       147.75             t=26.7025        0.080160    152.06  <- last
```

Three things fall out of that table, and all three are predictions the data confirms:

- **Six snaps pass unnoticed** because they are under the ceiling, and **eleven are over** — which
  is exactly the count of `Footing LOST` lines.
- The excess creeps by a constant **0.00154° per half turn**, because the hull's rate is not an
  exact divisor of the frame grid, so the sample that lands inside the window sits a little
  further past 90° each time. That is the "growing then stopping" the prior report could not
  explain.
- **It stops because the creep carries the sample out of the window.** The last excess is
  0.08016°; the next would be 0.08170°; the window's half-width is **0.08103°**. Nothing after
  26.70 s in a 60 s run, and nothing until the beat wraps round — about five minutes.

**At 60 fps, 30 s, not one frame landed inside the window at all** (largest differenced step
1.99980° against the ordinary 1.99999°), so there is nothing to see — and even a direct hit would
read (2.0 + 0.081)/dt = **124.9 deg/s**, which at r = 3 m is 14.3 m/s², under the ceiling. At this
radius the defect *cannot* trip the knockdown at 60 fps. That is the whole of "the same run shows
nothing at 60 fps".

The window's width, derived rather than guessed: for a pure pitch the engine's
`SingularityTest = Z·X − W·Y` is `−½ sin p`, so the branch fires when `|sin p| > 0.999999`, that is
within `arccos(0.999999) = 1.41421e-3 rad = 0.081028°` of ±90.

**Only pitch can reach it.** For a pure roll or a pure yaw `SingularityTest` is identically zero.

## 3. The fix

`Source/StarForge/Private/Ship/SFShipFrameComponent.cpp`, `…/Public/Ship/SFShipFrameComponent.h`.

A simulating hull is asked for its own angular velocity
(`UPrimitiveComponent::GetPhysicsAngularVelocityInRadians`, world space, read in `TG_PostPhysics`
where the body has already stepped); only a carrier **without** a physics body still has its
rotation differenced. The linear velocity and acceleration are untouched and are still differenced
on every carrier: a location passes through that same round trip as the doubles it already was, and
there is no singularity in it.

**Why it cannot be a threshold in seconds.** The error is in *position*, not in time: a fixed
0.081° divided by whatever interval is differenced. Widening the window only trades error for lag
(bounding it to 2% of 120 deg/s needs a 34 ms window, two frames at 60 fps, and the knockdown then
carries that lag), and it would still be wrong by an amount that depends on the frame rate inside that window. There
is nothing to tune: the differencing has to go where the body can be asked instead.

**What still differences, and is right to.** The station (no body at all), a docked hull
(`SetSimulatePhysics(false)` at capture, restored at undock) and the one-second capture slew.
None of them rotates through pitch ±90 on these rigs, and for a carrier without a body there is
no other source.

`GetFrameTransform()` is deliberately **not** touched. It is what the carry positions the occupant
by, and it must stay the pose the hull's geometry is actually at — the occupant and the deck move
through the 0.0666° snap together, so nothing physical happens to him.

## 4. Re-runs

Build clean. Headless tests **88 Success, 0 Fail**, `TEST COMPLETE. EXIT CODE: 0`
(`evidence/diag-omega-tests.log`). All rig logs below are `evidence/diag-omega-fix-*.log`, taken
from a tree with **no diagnostic in it**, rebuilt from the fix alone.

| Run | 60 fps | 400 fps |
|---|---|---|
| Spike, full pitch | `120.0` deg/s, drift `0.000 cm`, 0 `Footing LOST`, `footing intact` | `120.0` deg/s, drift **`0.000 cm`**, **0** `Footing LOST`, **`footing intact`** |
| Spike, full forward | `200.0 m/s`, drift `0.000`, `footing intact`, carries 0 | identical to the prior wave, digit for digit |
| Spike, full roll | `180.0` deg/s, drift `2.043` (was 2.043), jitter `0.748` (was 0.749) | `180.0` deg/s, drift `2.121` (was 2.121), jitter `0.166` (was 0.166) |
| Loop, 70 s | `60.0 s settled`, drift `0.000 cm`, carries 0, `footing intact` | same |
| Duel, 60 s (added — the handover guard reads this ω) | drift `354.981` (was 354.989), `knocked down 0.5 s`, 71 shots, both power down | drift `741.500` (was 741.520), `knocked down 35.7 s`, 70 shots |

The pitch row is the one that moved, and it moved back to the `8f4347e` baseline
(`verify/final-spike-pitch-400.log`): `0.000 cm`, `footing intact`. Everything else is the prior
wave's figures to the last digit except three thousandths of a centimetre of slide in runs where
the occupant is knocked down for real — the push is now computed from an exact ω.

No ship-side figure can move and none did: this ω has three consumers (the footing push on the
*character*, the handover guard, the HUD) and none of them touches the hull. The duel's contact,
power-down and shot counts are unchanged at both rates.

## 5. Reach — which recorded figures depend on the differenced rate

The instrument is read in three places: `USFShipboardMovementComponent::UpdateFooting` (line 651),
`USFInteriorVolumeComponent` (the handover guard's carrier motion) and `SFFrameDriftHUD`.

| Recorded figure | Affected? | Why |
|---|---|---|
| Checkpoint 5 B1, threshold **146.6 deg/s** at r = 3 m | **No** | A pure **roll**. `SingularityTest = Z·X − W·Y` is identically zero for a rotation about X, so the singularity branch can never fire. The axis, not the frame rate, is what makes it immune. |
| Checkpoint 5 B3, threshold **123.2 deg/s** at r = 4.24 m | **No** | Same roll. |
| Checkpoint 5 C4, **178.4 deg/s / felt 29.08** | **No** | Same roll (`E` while `R` is held). |
| Checkpoint 5's conclusion that three thresholds fall on √(19.62/r) | **No** | It rests on the three above. |
| Stage 3 handover, `0.000 cm` over 27 crossings; `58.978 cm over 1 frame` | **No** | Those are the frame *transform* and the carry, not this rate. The snap moves hull and occupant together. |
| Handover guard, `0.000000 m/s` docked | **No** | Docked, neither carrier simulates, so both still difference — and neither rotates. |
| Handover guard refusals at 53.28 and 81.19 m/s | **Not in substance** | Both hulls simulating and tumbling, so a snap was possible on the sampled frame; but the figure is dominated by linear speed, which never went through this path, and an overstated ω can only *refuse* more, never admit. Now exact. |
| Checkpoint 4 / scripted-profile knockdowns (`358 m/s` slide, "down for the whole run") | **No** | The profile sweeps the nose through the zenith, so snaps certainly occurred — but its linear acceleration is 314 m/s², sixteen times the ceiling. The verdict does not turn on 0.081°. |
| Every `Flight:` rate — 200.0 m/s, 180.0, 120.0 deg/s, 800 kN, 533333 kg m² | **No** | Read from Chaos through `USFThrusterArrayComponent::ReadState`. Never this instrument. |
| Hull-collision figures (14.93 / 14.88 s, 80.9 → 28.9, 0 → 52.0 m/s) | **No** | Physics. This rate is read-only and reaches no body. |
| Prior wave's Finding **F1** ("fix 2 caused the 400 fps pitch knockdowns") | **Superseded** | Fix 2 did not cause it and reverting fix 2 did not cure it; it re-phased the beat. The defect predates it — the snap fires every half turn on both builds. F1 is now closed by cause. |

Nothing above was edited. The only project record that should eventually be re-worded is the
prior wave's F1, and the CLAUDE.md line that would have had to be written about it never was.

## 6. Two things found that are outside this task

- **`evidence/final-fix-spike-pitch-400.log` is not what the prior report cites it as.** It is
  byte-for-byte identical (md5 `611857aa…`) to the reverted-spool control run
  (`scratchpad/euler-pitch-400.log`): it contains **zero** `Footing LOST` lines and reads
  `0.000 cm, footing intact`, while the report's table quotes it for "9 in 25 s, drift 0.001 cm,
  knocked down 3.0 s". The control overwrote the measurement. The 60 s twin,
  `final-fix-pitchlong-400.log`, is genuine and was reproduced here exactly.
- Two headless launches from Git Bash died on startup with `rc=130` even with `< /dev/null`
  (`spike-fwd-60`, `loop-60`, `duel-60` twice). Re-running cured the first two; the duel needed
  `Start-Process` from PowerShell, i.e. its own console. CLAUDE.md's known shape, but `< /dev/null`
  is evidently not sufficient on its own.

## 7. Files

- `Source/StarForge/Private/Ship/SFShipFrameComponent.cpp` — the fix and its reason.
- `Source/StarForge/Public/Ship/SFShipFrameComponent.h` — the accessor's contract.
- `evidence/diag-omega-pitch-400.log`, `diag-omega-pitch-400-body.log`, `diag-omega-pitch-60.log`
  — the diagnostic runs, at `2e37c06` plus logging only.
- `evidence/diag-omega-fix-*.log` — the ten re-runs, from a tree with no diagnostic in it.
- `evidence/diag-omega-tests.log` — 88/0.

The diagnostic was removed and the binary rebuilt from the fix alone before every run in §4.
