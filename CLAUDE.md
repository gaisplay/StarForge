# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

StarForge is a solo-developed space-sim prototype in Unreal Engine 5.8.2. The repository currently holds **only the vertical slice** — a technical probe, not a game. Its job is to answer architectural questions before content exists. Everything renders as grey engine primitives on purpose.

Design docs are in Russian under `docs/superpowers/`; code, comments and commit messages are in English.

- `docs/superpowers/specs/` — the design spec. The authority on *why*. Section numbers (§6, §7, §12) are referenced throughout the code and plans.
- `docs/superpowers/plans/` — implementation plans, task-by-task with checkboxes.
- `docs/superpowers/notes/` — checkpoint runbooks and result forms for manual measurements.
- `docs/superpowers/source/` — the novel the design was originally argued from, read and
  digested. **Background reading, not a source of requirements.** Development follows the
  spec and the plans; where the two disagree, the spec wins and the disagreement is not a
  defect to fix. Its own reconciliation section lists five places the design departs from
  the book on purpose. Do not open it to plan work.

Read the spec before changing anything structural. Most non-obvious code here is a deliberate answer to a documented failure mode, not an accident.

## Engine location and commands

The engine is a launcher-installed build at `C:\Program Files\Epic Games\UE_5.8`. Commands assume Git Bash.

**Build** (editor target — this is the normal build):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

**Run the headless unit tests** (`StarForgeCore` only — no editor, no world):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Run a single test or subtree by narrowing the filter — tests are named hierarchically, e.g. `Automation RunTests StarForge.Core.ShipFrame.CarryRotation`.

**`UnrealEditor-Cmd.exe` truncates its stdout on Windows.** A full test session came
through as about a kilobyte of console output. Read results from
`Saved/Logs/StarForge.log` instead, looking for `Test Completed. Result=` and
`TEST COMPLETE. EXIT CODE:`. A short console capture does not mean the tests did not
run. The project logs its own summaries under the `LogStarForge` category.

**Play the measurement rig:**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

**Run the rig headlessly and read the result from the log.** This is how the flight
checks are done without a human at the keyboard: `-benchmark -seconds` gives a fixed
timestep and, more importantly, a *clean exit* — killing the process instead leaves
the log buffer unflushed and the file ends at frame zero, which looks exactly like a
session that never ticked.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log
```

**The drift line carries knocked-down time and a verdict.** `Shipboard drift: … skipped
carries N, knocked down S s -- <verdict>`, where the verdict is `footing intact`,
`KNOCKED DOWN: …`, `CARRY FAILED: …` or `knockdown off (sf.Knockdown 0)`. Under the
recipe above it reads `knocked down 0.0 s -- footing intact`: 800 kN on 50 t is 16 m/s²,
under the 19.62 limit. The scripted profile (`sf.ShipProfile 1`) is over the limit almost
continuously — 314 m/s² of linear acceleration at its peaks — so a standing occupant is
knocked down for the whole run and the drift figure is his slide across the cabin.
**Re-running checkpoint 1's baseline needs `sf.Knockdown 0`**, and so does the
`sf.ShipGravity 0` negative control: with the knockdown on, `bStayBasedInAir` keeps the
movement base through the fall, the carry keeps running, and the control shows nothing.
A carry that has genuinely stopped is reported by `Carry NOT RUNNING` once a second,
from outside the carry, because the counter inside it cannot report a gap that never ends.

**The loop rig lives on a second map, and the default map has not changed.** `L_FrameSpike`
does not move a single default: every figure this project has recorded was taken on it. The
loop — station, ship spawned already docked, person in the cabin — is `ASFLoopGameMode`, and
there are two ways to reach it.

By map, once `Content/Maps/L_Loop.umap` exists. The level is empty and carries one thing, a
`GameMode Override` of `SFLoopGameMode`; the mode spawns the station, the ship and the pawn.

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log
```

**From Git Bash the map argument needs `MSYS_NO_PATHCONV=1` in front of the command.** MSYS
rewrites an argument that starts with `/` as a Windows path — `/Game/Maps/L_Loop` reaches the
engine as `C:/Program Files/Git/Game/Maps/L_Loop` — and the engine then finds no map, brings
up no world and sits in an empty level for the whole run: no station, no `Loop rig:` line,
`dock free -- NO STATION in this level`, and an exit code of 0. Measured on 2026-09-13; the
tell in the log is `LogInit: Command Line: "C:/Program Files/Git/Game/Maps/L_Loop" …` with no
`Bringing World` line at all. From cmd or PowerShell the plain form is fine. The URL form
below was not rewritten in any recorded run.

By overriding the game mode on the URL, which needs no map at all. **Quote the first
argument**: `?` is a shell metacharacter, and unquoted the command silently loses everything
after it and runs the ordinary spike rig instead.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" "/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFLoopGameMode" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log
```

The two differ in exactly one thing: which map file is loaded. Both maps are empty and the
mode assembles the world either way, so the second form does not touch a byte of a map —
which is why every headless figure stage 3 recorded was taken that way, before `L_Loop`
existed. The map is what a person needs: a URL cannot be typed into a shortcut.

`ASFLoopGameMode` forces `sf.ShipProfile 0` and says so in the log; the ship is docked before
the world begins play, so there is no frame in which it is adrift. **Run every flight check on
this rig at `-fps=400` as well as `-fps=60`**: undocking recreates the hull's physics state,
and whether `SleepFamily` survives that is a question for a run, not for a header.

Console variables the rig understands:

| Variable | Default | What it does |
|---|---|---|
| `sf.ShipGravity` | 1 | 0 leaves gravity at world down — the negative control that detaches the occupant |
| `sf.ShipProfile` | 1 | 1 flies the scripted checkpoint manoeuvre, 0 flies on thrust from the intent |
| `sf.ConsoleOperator` | 1 | 1 the console operator holds the pilot's seat; 0 leaves it free, which is what a person needs before E will seat him. A runtime switch, not a startup default — it can be typed at the console mid-run |
| `sf.Knockdown` | 1 | 1 knocks a standing occupant off his feet above the 19.62 m/s² grav-plate limit (spec §6) — falling, walking refused, still carried; 0 keeps him standing. Checkpoint 1's scripted-profile baseline and the `sf.ShipGravity 0` negative control need 0 |
| `sf.Intent.X/Y/Z` | 0 | Linear intent in ship axes, -1..1 |
| `sf.Intent.Roll/Pitch/Yaw` | 0 | Angular intent, -1..1, stored as X roll / Y pitch / Z yaw |
| `sf.Intent.Assist` | 1 | 1 the stick asks for a speed, 0 it asks for thrust |
| `sf.FlightLog` | 0 | 1 logs the flight readout once a second |
| `sf.DriftDebug` | 0 | 1 logs carry calls per frame, movement mode, base and ship-local position |
| `sf.TargetDrone` | 1 | 0 withdraws the rig's target drone; the turret must then report no target rather than aim at the origin |
| `sf.AutomationOperator` | 1 | 1 the ship's automation holds the gunner's seat; 0 leaves it free. The gunner's post is what tests spec §4's claim that the ship cannot tell who holds a post, so it is held by something that is not a person by default |
| `sf.Dock` | -1 | Docking from the console, the headless twin of the `F` key. `1` docks if the approach is inside the window, `0` undocks, `-1` is no command. Acted on when the value **changes**, not on its level. `2` is the headless cycle — undock now and dock again one capture later — and it exists because `-ExecCmds` applies its whole list at startup, so `sf.Dock 0, sf.Dock 1` leaves only the last value and "docked → free → docked" cannot be asked for in one run at all |
| `sf.BindFrame` | *(empty)* | `ship` or `station`: binds the occupant to that carrier's frame **wherever he is standing**. The negative control for the handover, and the same trick as `sf.ShipGravity 0` for the carry. Docked, it is a legitimate handover and everything stays quiet; undocked and under thrust it is knowingly wrong, and the cross-check must warn within ten frames and the carry must stop about a second later; bound the other way round — to the moving ship from the station's floor — the `Frame handover` line must show one frame of the ship's travel, never zero |

**Open the editor** (launch this binary directly; the launcher's `UnrealVersionSelector` may show a spurious engine-conversion prompt):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject"
```

**Close the editor before building.** A running editor holds the Live Coding lock and the build will fail. Check with `tasklist | findstr Unreal`. From Git Bash, `taskkill /F` does not work — MSYS turns the switch `/F` into the path `F:/` — so once everything is saved close it with `powershell -NoProfile -Command "Stop-Process -Name UnrealEditor -Force"`.

## Module split — the load-bearing boundary

Two runtime modules, and the split is the single most important structural fact in the repo.

**`StarForgeCore`** depends on `Core` and `CoreUObject` only. **Adding `Engine` to it is an error that destroys the point of the split.** It holds all the maths — reference-frame conversions, first-person look composition,
the SI/centimetre boundary, the 6x6 matrix, the thruster effectiveness matrix, the
thrust allocator and the IFCS flight-control loop. No actors, no components, no meshes. The reason is testability: a bug in the thrust solver does not crash or log, it surfaces weeks later as "the ship feels wrong". A headless unit test catches it in a second.

**`StarForge`** is the game module — actors, components, input, UI, AI. It depends on `StarForgeCore`.

**Units.** Physical quantities inside `StarForgeCore` are SI (metres, kilograms, newtons, radians). Unreal works in centimetres. Conversion happens at the module boundary and nowhere else. The exception is the geometric transform helpers in `ShipFrameMath.h`, which are deliberately unit-agnostic and operate in whatever unit the caller passes consistently — the game passes centimetres.

## The central mechanism: motion decomposition

This is the design's one unresolved technical risk (spec §6) and the reason most of the game module exists.

Stock `UCharacterMovementComponent` carries a character on a moving base with a **swept** move. At 400 m/s and 60 fps that sweep is a ~6.6 m line through world space, and it collides with the ship's own hull — the occupant sticks, jitters or falls through.

`USFShipboardMovementComponent` overrides `UpdateBasedMovement` and splits the frame's motion into two things that stock UE wrongly treats alike:

- **Frame carry** — motion caused purely by the ship flying. Relative to the ship the occupant has not moved at all, so nothing along that path can legitimately block it. Done as a **teleport** (`bSweep = false`).
- **Own walking** — a few centimetres against geometry that is momentarily static. Left to the **stock swept path**, untouched.

After the split, ship speed stops affecting walking entirely.

Related invariants:

- **View direction is stored in ship coordinates**, not world. The interior must stand still in front of the eyes while the ship rotates.
- Each occupant keeps **its own** last-known ship transform (`LastFrameTransform`), so no tick-order dependency exists between a ship and the people standing in it.
- `USFShipFrameComponent` is the reference-frame facade and deliberately **moves nothing** — it answers "where is this frame" and converts coordinates. Carrying occupants is the movement component's job.

## Design budgets that code depends on

These are spec constants, not tuning knobs. Changing one invalidates the measurement it was chosen for.

| Quantity | Value | Source |
|---|---|---|
| Manoeuvring speed | 200 m/s (walking free) | §7 |
| Boost speed | 400 m/s (walking free) | §7 |
| Pitch / yaw rate | 120°/s | §7 |
| Roll rate | 180°/s | §7 |
| Grav-plate compensation ceiling | 2 g = 19.62 m/s² | §6 |
| Occupant drift budget | ≤ 1 cm per minute, no input | §10 |
| Solver damping lambda | 0.01 | flight-maths plan |
| Clamp-and-correct passes | 3 | flight-maths plan |
| Parasitic wrench tolerance | ≤ 1% of that axis's capability | flight-maths plan |

## UE 5.8 API notes

Examples written for earlier 5.x will not compile:

- `EAutomationTestFlags` is an `enum class`, and the context mask is a separate constant. Use `EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter`. The 5.3-era `EAutomationTestFlags::ApplicationContextMask` does not build.
- `UCharacterMovementComponent::GetMovementBase()` is `UE_DEPRECATED(5.8)`. Use `GetMovementBaseObject()`.

## Driving the editor over MCP

`.mcp.json` at the project root points Claude Code at an MCP server that runs **inside the Unreal Editor process** on `http://127.0.0.1:8000/mcp`. The tools only exist while the editor is open; a closed editor shows as a connection refusal, not a missing capability.

Auto-start is already configured in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` (`bAutoStartServer=True`), so opening the project is enough — no console command needed. The `ModelContextProtocol` and `AllToolsets` plugins are enabled in `StarForge.uproject`; `AllToolsets` is what actually supplies the tools, and without it the server exposes none.

Use the `unreal-mcp` skill for editor-driving tasks. Tools are reached through the three meta-tools `list_toolsets` / `describe_toolset` / `call_tool`.

## Conventions

- C++ types in the game module use the `SF` prefix (`ASFCharacter`, `USFShipFrameComponent`). `StarForgeCore` uses plain namespaced free functions (`StarForge::ShipFrame::...`), since it has no `UObject`s.
- Game module layout mirrors subsystem folders under both `Public/` and `Private/`: `Ship/`, `Player/`, `Debug/`.
- Commit after each task. English messages; the body explains **why**, not what.
- Do not add "Co-Authored-By: Claude" or any similar attribution line to commit messages.
- `.gitattributes` routes `*.uasset`, `*.umap` and source media through Git LFS.

## Current state

**Stage 3 is finished, passed by hand, and merged: `main` is at merge commit `8695a09`,
thirty-one commits over the plan's own commit.** 66 headless tests, clean build, and the loop rig
verified at 60 and 400 fps both before and after the merge.

There are now two reference frames. `ASFTestStation` is a boxed room with its own
`USFShipFrameComponent` and no physics body of any kind; the ship docks to it kinematically —
thruster array locked, hull taken out of simulation, a one-second eased run-in to the mated pose,
root attached to the station's socket — so the relative velocity afterwards is zero by
construction rather than by convergence. Docking is commanded (`F` from the pilot's seat,
`sf.Dock` from the console), never automatic. A person is bound to a carrier by walking into its
`USFInteriorVolumeComponent`; entering binds and leaving deliberately does not unbind, so the ten
centimetres of wall between the two volumes belong to whoever bound him last. The second rig is
`ASFLoopGameMode` on `Content/Maps/L_Loop`: station at the origin, ship spawned already docked,
person in the cabin. `L_FrameSpike` did not move a single default.

**The sentence the stage was built for came from the person who walked it:** the handover through
the hatch is not visible — "as though I were moving across a single surface" — and the log reads
`0.000 cm` for all twenty-seven crossings of that session. Checkpoint 6's form,
`docs/superpowers/notes/2026-09-10-checkpoint-6-results.md`, carries every figure with its log in
`notes/evidence/`. **Read it, and the stage-4 handoff beside it, before trusting anything older.**

**What is now measured rather than reasoned about, at 60 and 400 fps:** the ship spawns docked and
a standing occupant drifts `0.000 cm` over sixty settled seconds; a full roll after undocking
still reaches `180.0 deg/s` at 400 fps, so the hull's never-sleep settings survive being switched
out of simulation and back; a kinematic Chaos body reports its mass properties (`533333` before
and after undocking); the carry survives the schedule change in both directions — once per frame
on a docked base, twice on a simulating one; refusals name the failing condition in the order
distance, speed, forward, up; and three instruments were each shown to speak where the other two
cannot — the floor-versus-frame cross-check on a still ship (49 warnings while drift and carry
read healthy), `Carry NOT RUNNING` on a ship that leaves from under a wrongly-bound man (2.7 s,
`mode Falling, base none`), and the handover line on a binding to a moving carrier
(`58.978 cm over 1 frame(s)`, which is 200 m/s at 339 fps to 0.03%).

**The plan had two arithmetic errors, both wrong in the direction that looks right, and both were
found by re-deriving rather than re-reading.** The capture's peak acceleration is 23.0 m/s² at the
window's edge, not the 18.0 the plan claimed, and it is over the 19.62 grav-plate ceiling rather
than under it — accepted because the command comes from a strapped-in pilot and nobody else is
aboard. And the station's interior volume was four metres short in Y, leaving the whole airlock
tunnel claimed by no volume at all; no run could have caught it, only arithmetic. A trailing
comment with a number in it lied both times.

**The manual checkpoint found three more, none of them in the mechanism.** The cross-check's
threshold was a frame count where its own comment said a sixth of a second, so at 320 fps it
warned on every walk through the hatch — a frame count is a frame rate, the Chaos sleep counter's
defect wearing a different name; one runbook step asked for something two hands cannot do; and
another could be "performed" without leaving the room and look exactly like a control that found
nothing. All three are fixed, and the first is confirmed by twenty-seven crossings producing one
warning — a man who stopped on the seam, which is reported on purpose.

**Every stage-2 flight check still passes, and every one runs at 60 and 400 fps.** Forward thrust
reaches 200.0 m/s and saturates at exactly 800 kN; roll settles at 180.0 deg/s, pitch and yaw at
120.0, with nothing on the other axes and no linear speed from a pure torque; the scripted profile
stands the pilot's post down; the `sf.ShipGravity 0` control detaches the occupant; a knocked-down
occupant under the profile stays aboard. Until 2026-09-10 those rates were measured only at a
fixed 60 fps and "a full roll settles at exactly 180.0" was false above about 250 fps; a rate that
holds at one frame rate and not the other is the shape this project's worst defect took, and
every threshold added since is written in seconds.

Two warnings from checkpoint 4 that the instruments cannot give and that still hold: sitting down
is a single teleport the drift gauge does not know about, so take the seat on the move; and the
interior flies with the occupant, so a ship turning at 180 deg/s changes nothing on screen, which
is the design working and reads like nothing happening. One more from checkpoint 6: `F` outside
the pilot's seat answers in the log only, and at the keyboard that is indistinguishable from an
unbound key.

**Next action:** plan stage 4 from `docs/superpowers/notes/2026-09-14-handoff-stage-4.md` —
combat: projectiles, hits, shields, hull strength, and a pirate that is the same `ASFTestShip`
with its posts held by automation. The gunner's post, the turret, the automation operator and the
target drone already exist from stage 2's plan «в»; §13 places them in stage 4 by an accident of
dating, so the plan starts from projectiles. The handoff lists what the second ship breaks first:
every `sf.*` variable assumes one ship, `ResolvePorts` is bound to the station's type, and the two
hulls answer to different collision channels.

Still open, none blocking: no gamepad exists, so the analog input path has never been watched;
§6's "dragged across the deck" stays marked inexact by choice; and twenty-odd deferred minors are
grouped in the handoff, none of them structural.

Things in the code that look like mistakes and are not:

- `SetGravityDirection` along the ship's Z in the shipboard movement component. Without
  it a rolling deck stops being walkable within a quarter second and the carry stops
  being called at all. Proven again on 2026-09-08: with `sf.ShipGravity 0` the occupant
  went from Floor to WallXNeg to Ceiling inside 202 ms and the engine stopped calling
  the carry entirely. That control is run with `sf.Knockdown 0` from now on; the
  `bStayBasedInAir` entry below says why.
- `bSweep = false` on the carry's `MoveUpdatedComponent`. Relative to the ship the
  occupant has not moved, and a sweep at 400 m/s traces six metres through the hull.
- Camera view limits of `0 / 359.999` rather than `±180`. `FMath::ClampAngle` computes
  its half-width as `ClampAxis(Max - Min) * 0.5`, and `ClampAxis(360)` is zero, so a
  symmetric range pins every angle to 180°.
- **Look input going through `StarForge::ShipFrame::ApplyLookStep` rather than
  `AddPitchInput` / `AddYawInput`.** Those feed `RotationInput`, which `UpdateRotation`
  adds to the *world* Euler components of the control rotation. The view is carried in
  the ship's frame, so on a deck rolling at 180 deg/s the axis "look up" moves along
  precesses continuously and the view sweeps a circle. Yaw turns about the deck normal,
  pitch about the view's own right axis re-squared against the deck.
- **The once-per-frame guard in `UpdateDriftMeasurement`.** The engine calls
  `UpdateBasedMovement` **twice per frame**, and the clock used to add a full delta on
  each call, so it ran at exactly 2.000 times real time. Every duration this instrument
  ever reported was double — checkpoint 1's "57.2 s" was 28.6 seconds against a 55 s
  gate. The carry itself is unharmed by the second call (the ship has not moved between
  them), so do not "simplify" the guard away.
- **`SetLinearDamping(0.0)` on the hull.** Chaos defaults a body to 0.01, which is drag.
  The ship settled at 199.0 m/s instead of 200 with the controller holding 99.5 kN
  forever to stand still against it. Both damping values are stated explicitly so a
  change to the project physics settings cannot quietly put the atmosphere back.
- **Two capability numbers on the allocator, not one.** `GetAxisCapability` sums every
  thruster's contribution in absolute value and is the right scale for judging a
  parasitic component. `GetAxisAchievable` takes the smaller of the two directional
  sums and is the right scale for a stick, because on a balanced array only half the
  thrusters push a given way. Using the first for the second doubles the figure and
  kills the top half of the stick travel.
- Test thruster arrays carrying **opposed pairs** on every axis. An array pushing only
  along +X, +Y and +Z cannot produce torque without also shoving the ship, because
  zeroing the net force zeroes every contributing throttle. Turning without being
  pushed needs a force couple, and a couple needs two thrusters facing opposite ways.
  This is a constraint on real ship layouts too, not just on the fixture.
- **`bStayBasedInAir` set only for the duration of a knockdown, `IsValidLandingSpot`
  returning false while down, and `ProcessLanded` refusing to land him.** The knockdown
  puts the occupant into `MOVE_Falling`, and entering falling clears the movement base
  unless that flag is set (`CharacterMovementComponent.cpp` 1454–1471); with no base the
  engine never calls `UpdateBasedMovement`, and the carry — the one thing that keeps him
  aboard at 400 m/s — stops. §6 is explicit that it must not. The flag is not left on
  permanently because a deck made unwalkable by `sf.ShipGravity 0` would then keep its
  base too, and the negative control would show nothing. The landing-spot override is
  what keeps him down: `PhysFalling` lands him on the first blocking hit it is allowed
  to (5050), in the same frame, under the same acceleration. `ProcessLanded` is
  overridden as well because a character wedged between two walls is landed without
  asking (5221–5225), and landing re-derives the base from the floor under him.
- **`UpdateFooting` called from two places — the carry and `TickComponent`.** The engine
  calls `UpdateBasedMovement` only while a movement base is set (2476–2487), so a man who
  loses his base while down would freeze there: no landing, because the override above
  refuses one while he is down, and so no base and no carry ever again. The tick runs
  either way, and the same once-per-frame guard that stops the push being applied twice
  makes the second call free whenever the carry has already run. The flag comes off in
  the same function when the base is genuinely gone — there is nothing left to keep.
- **`FallingLateralFriction = 2.0` where the plan says the coefficient is 4/s.**
  `ApplyVelocityBraking` multiplies it by `BrakingFrictionFactor`, 2.0 by default (734,
  4384–4385). The 4/s that acts is what makes a knocked-down slide settle at a quarter
  of the excess acceleration; writing 4 here would halve that. It bounds only the part
  of the push lying along the deck — `PhysFalling` friction applies to
  `ProjectToGravityFloor(Velocity)` alone — so it is not a speed limit on a knockdown:
  under the scripted profile a knocked-down occupant was measured at 358 m/s inside the
  eight-metre cabin, stopped by the walls and nothing else.

- **`SleepFamily = ESleepFamily::Custom` with `CustomSleepThresholdMultiplier = 0` on the
  hull.** Two lines in `ASFTestShip`'s constructor that look like leftover tuning and are
  the difference between a ship that flies and one that does not. Chaos sleeps a body whose
  per-step velocity change stays under a threshold, and that change shrinks as the frame rate
  rises: a full roll gains 0.05 rad/s per step at 60 fps and stays awake, gains 0.0075 at 400
  and is re-slept inside the step, losing what it gained. Above about 250 fps the ship simply
  stops answering roll and pitch while the readout reports the whole commanded 1.6 MNm as
  achieved. A multiplier of zero means the hull sleeps only when exactly motionless, so the
  first step of any commanded thrust keeps it awake. Waking the body wherever a wrench is
  applied was tried first and measured to change nothing — being awake is not what is in
  question — and the reason it cannot work is written where that attempt stood.
- **The frame cross-check's threshold is a time, `FrameMismatchThresholdSeconds`, not a
  frame count.** It was ten frames until the first manual run of the loop at 320 fps, which
  printed `Standing on … while bound to …` on every single walk through the hatch — twelve
  false findings in one session, in an instrument whose red line the runbook says stops the
  run. A volume binds the occupant when the *edge* of his capsule touches it; the floor
  under him switches when its *centre* crosses the panel boundary; the 24–34 cm between
  the two is two or three frames at 60 fps and thirteen to eighteen at 320. A frame count
  is a frame rate — the same shape as the Chaos sleep counter — and any threshold added to
  this project from now on is written in seconds and tested at 400 fps as well as 60.
- **A second console key, `F1`, in `Config/DefaultInput.ini`.** The engine binds the console
  to `Tilde` alone, and on a keyboard layout that does not produce Tilde from the key left of
  "1" the console never opens — silently, with nothing in the log. It cost a whole run of
  checkpoint 4 before it was understood, and every manual check that needs a command mid-run
  is impossible without it. `Tilde` is kept, so a Latin layout behaves exactly as before.

When reading the checkpoint HUD: **check the measured-time line before the drift
figure.** A drift of zero is what both a perfect minute and a dead instrument look
like, and only `measured` tells them apart. And read the `footing` line before judging a
drift over budget: a slide while knocked down is not a carry failure, and the line says
which it was.

**Numbers go in the log, not on a screen.** Both instrument defects found so far — the
doubled clock and the hull's drag — were caught by comparing logged figures against
arithmetic, not by looking at the HUD. `sf.FlightLog` and `sf.DriftDebug` exist for
that, and a figure transcribed from a screen is a figure nobody can check next week.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `python -m graphify query "<question>"` when graphify-out/graph.json exists. Use `python -m graphify path "<A>" "<B>"` for relationships and `python -m graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `python -m graphify update .` to keep the graph current (AST-only, no API cost).
