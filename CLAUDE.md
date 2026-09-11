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
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log
```

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

**Close the editor before building.** A running editor holds the Live Coding lock and the build will fail. Check with `tasklist | findstr Unreal`.

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

**Stage 2 is finished in full — all three of its plans — and every commit is on `main`.**
The flight maths and the ship on Chaos landed as `8ffa63b`; the pilot's post came with it;
the gunner's post, the automation operator, the target drone and the grav-plate knockdown
of §6 landed as `41a8f80` over twenty-three commits. 56 headless tests, clean build.

The ship is a Chaos rigid body flown by the maths from `StarForgeCore`. It has a data
asset (`Content/Ships/DA_TestShip`), a thruster array component that reads mass
properties back from Chaos, a closed control loop, and a flight readout on screen and
in the log. The loop is owned by `USFPilotStationComponent`, which asks whoever holds
the post for an intent through `ISFStationOperator` — the console operator, the player
and the ship's own automation are three implementations of that one interface, and no
station can tell them apart. That indifference is the load-bearing claim of spec §4, and
it is no longer theoretical: a player flying from the pilot's seat and automation tracking
a drone from the gunner's were logged in the same millisecond of the same frame, with
neither post nor hull noticing the difference.

**The risk that made stage 2a open with a measurement is gone.** Checkpoint 1 was
earned against a ship that moved by setting its transform; it was re-earned against a
simulated body: ninety unbroken seconds, zero drift, zero jitter, no skipped carries,
the movement base never once changing. Figures and the negative control are in
`docs/superpowers/notes/2026-09-08-checkpoint-1-rerun-results.md`.

**Every flight check passes.** Forward thrust reaches 200.0 m/s and saturates at exactly
800 kN; a full roll settles at exactly 180.0 deg/s on the roll axis and a full pitch at
exactly 120.0 on pitch, with nothing on the other two; a pure torque produces no linear
speed; with the scripted profile on, the station stands down and commands nothing. The
hull's inertia reads 533 333 kg m², which is `m(a²+b²)/12` for an 8 m cube of 50 t to the
unit — the SI/centimetre boundary is right end to end.

**Those rates are now checked at four frame rates, not one, and that is not pedantry.**
Until 2026-09-10 every one of them was measured only at a fixed 60 fps, and "a full roll
settles at exactly 180.0" was true there and false above about 250 fps, where the hull slept
through its own thrusters. When adding a flight check, run it at `-fps=400` as well as
`-fps=60`; a rate that holds at one and not the other is the shape this project's worst
defect took.

**Both manual checkpoints have been passed by a person, on 2026-09-10.** Checkpoint 4 (the
pilot's seat) had been owed since the previous stage; checkpoint 5 (turret, crew, knockdown)
closed this one. The forms carry the figures and the logs sit in `notes/evidence/` beside
them. Across five interactive runs there was not one `CARRY NOT RUNNING`, not one
`CARRY FAILED`, and not one skipped carry — a knocked-down occupant stays aboard, which is
the distinction §6 exists to draw. Three independent measurements of the grav-plate threshold
— 146.6 deg/s at the cabin's centre, 123.2 at the side wall, 178.4 standing up out of the
seat — all landed on `sqrt(19.62/r)` to two decimals.

**Two defects were found by hand that no headless check in this project could have caught**,
and both are fixed: the hull fell asleep and stopped answering roll and pitch above about
250 fps, and the console never opened at all on a non-Latin keyboard layout, silently. Every
headless run here is a fixed 60 fps, so the first lived entirely above the tested range while
"a full roll settles at exactly 180.0 deg/s" stayed true of the rig and false of the game.
**Read the two results forms before trusting any figure older than 2026-09-10.**

Two warnings from checkpoint 4 that the instrument cannot give and that still hold: sitting
down is a single teleport the drift gauge does not know about, so taking the seat from a
standstill prints the distance walked to the chair as drift — measured at 46.429 cm from
46.5 cm away, so press the key on the move; and the interior flies with the occupant, so a
ship turning at 180 deg/s changes nothing on screen, which is the design working and reads
like nothing happening.

**Next action:** the stage-3 handoff, then plan stage 3 — station, docking port, handing the
reference frame over in an airlock.

Two things are open, and neither blocked the merge. No gamepad exists, so the analog input
path is compiled and unit-tested but has never been watched. And §6's "dragged across the
deck" is **marked inexact**: a knocked-down man is thrown about the cabin, ceiling included,
because friction bounds only the component along the deck. Accepting that and rewriting the
sentence, or damping the component across the deck, are different games; the choice is
deliberately deferred and the spec says so where it matters.

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
