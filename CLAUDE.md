# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

StarForge is a solo-developed space-sim prototype in Unreal Engine 5.8.2. The repository currently holds **only the vertical slice** — a technical probe, not a game. Its job is to answer architectural questions before content exists. Everything renders as grey engine primitives on purpose.

Design docs are in Russian under `docs/superpowers/`; code, comments and commit messages are in English.

- `docs/superpowers/specs/` — the design spec. The authority on *why*. Section numbers (§6, §7, §12) are referenced throughout the code and plans.
- `docs/superpowers/plans/` — implementation plans, task-by-task with checkboxes.
- `docs/superpowers/notes/` — checkpoint runbooks and result forms for manual measurements.
- `docs/superpowers/source/` — the novel the design was originally argued from, read and
  digested. **Its standing changed on 2026-09-17, and the old rule is kept below because the
  reason for it still holds inside the slice.**

  *During the slice* it was background reading and not a source of requirements: the spec was
  the authority, §1–§13 said what to build and why, and opening the novel to plan work could
  only introduce requirements the spec had already weighed and rejected. That rule did its job.

  *After the slice* there is no "why" left to follow — §13 ends at "Точка проверки 4. Срез
  готов." and names no stage after it. The second-phase spec is therefore argued **from the
  novel again**, deliberately and by the owner's decision. The order still stands: novel →
  spec → plan → code. Nothing is built from the novel directly, and no plan cites it.

  **The five deliberate departures in its own §9.2 stay departures**, and one of them carries a
  number worth knowing before reading anything else: the source's combat speeds are 1–5 km/s
  against §7's 200/400 m/s. That is not a sixth disagreement — it is the arithmetic of the
  third, which records that nobody walks the ship during combat in the source. Walking is this
  project's own idea, and 200/400 is what walking costs. Section §10 of the digest lists what
  was thrown out on purpose; do not bring it back.

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

**The combat rig is a third map, and neither of the other two moved.** `ASFCombatGameMode` on
`Content/Maps/L_Combat` — an empty level whose one setting is a `GameMode Override` of
`SFCombatGameMode`, created by hand in the editor (step 0 of checkpoint 7's runbook). The mode
spawns the player's ship at the origin and the pirate 1000 m along +X and 150 m up, yawed 180°.
The pirate is the same `ASFTestShip` with `Crewing = Automated`: the pursuit operator holds its
pilot's post and the automation its gunner's. Neither ship gets a target drone; each is the
other's target. The pawn goes into the player's cabin. Then the mode writes one line, every state
in it read back:
`Combat rig: scripted profile off; the player's ship is crewed and holds fire until sf.Fire 1; the pirate is automated and armed; the man stands in the cabin; separation 1011.2 m.`
An upper-case word in it is a state that did not hold, and the line goes out as an `Error`. Two
cases make it a `Warning` by design: `sf.Pirate 0`, and a pilot's seat left free by
`sf.ConsoleOperator 0` set before `StartPlay`.

By map, once `L_Combat.umap` exists:

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Combat -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=40 -ExecCmds="sf.FlightLog 1" -log < /dev/null
```

By the URL, which needs no map. The first argument is quoted, for the same reason as the loop's.
Every headless figure stage 4 recorded on this rig was taken this way, because the map did not
exist yet.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" "/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFCombatGameMode" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=40 -ExecCmds="sf.FlightLog 1" -log < /dev/null
```

`MSYS_NO_PATHCONV=1` applies to `/Game/Maps/L_Combat` exactly as it does to `L_Loop`.

**Headless launches from Git Bash end in `< /dev/null`.** Without it, two launches at 400 fps
exited on startup with code 130 and
`LogCore: Engine exit requested (reason: ConsoleCtrl RequestExit)`, with not one `LogStarForge`
line. With it, none of the next 23 did.

**`sf.Pirate` has to be on the command line, as `"-ini:Engine:[ConsoleVariables]:sf.Pirate=0"`.**
The mode reads it in `StartPlay`, before `-ExecCmds` applies. Use one `-ini` switch per variable:
two variables joined by a comma in one switch were not taken.

**Three lines that are red on the other rigs are correct here.** Checkpoint 7's runbook lists
them with the condition under which each becomes a finding again:

- `dock free -- NO STATION in this level` — there is no station;
- `Frame handover REFUSED` when the hulls touch;
- `thruster array NOT built` with its `Error`, once hits have left the survivors on two corners.
  The `Error` repeats on every later destruction.

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
| `sf.Fire` | -1 | The player's trigger, as a latch rather than a held key. `-1`: the trigger is whatever the gunner post's operator asks for, on a weapons-free hull. `0`: hold fire. `1`: hold the trigger down. It replaces the operator's trigger, never the operator, so an empty or locked gunner's post does not fire under it (0 shots measured with `sf.AutomationOperator 0`). Every ship on the spike and loop rigs, and the player's ship on the combat rig, is weapons-hold (`bWeaponsFree = false`), so nothing fires until `sf.Fire 1`. Ignored by an automated hull |
| `sf.WeaponDebug` | 0 | 1 logs one line per shot: `Weapon: [Ship] shot N at t … s from muzzle …, turret <state> …`. Hits are logged whatever it says, because a hit is an event, not a stream. Counting shots needs it. Acts on every hull, and each line names the ship that fired |
| `sf.ProjectileSweep` | 1 | 0 replaces each projectile's swept segment with a zero-length test at the end of its step. This is the tunnelling control, and it must miss: 0 hits at 60 and 400 fps where the sweep hits every on-target shot. Acts on every projectile in the world |
| `sf.AimAt` | *(empty)* | `station` or `ship`: what the gunner's automation tracks instead of what the rig gave it. `ship` means another ship, never its own. Empty is the rig's target: the drone on both measurement rigs, the pirate on the combat rig. Acted on when the value changes. A miss is a `Warning` and leaves the gunner with no target. Ignored by an automated hull |
| `sf.Shields` | 1 | 0 sends every hit straight to the hull, and each hit says so: `Hit on [X] +X face: shields OFF (sf.Shields 0), 100.0 straight to hull.` Face charges are left untouched, so only `hull` falls in the `Combat:` line. **A named exception** to the rule that an automated hull answers no `sf.*` variable: it acts on every hull, so on the combat rig both ships lose their shields. Checkpoint 7's control C1 turns off the pirate's |
| `sf.Pirate` | 1 | 0: the combat rig spawns no pirate. The player's automation has no target, and the rig line goes out as a `Warning`. **Read once in `StartPlay`, before `-ExecCmds` applies**, so it has to be set before the level starts: `"-ini:Engine:[ConsoleVariables]:sf.Pirate=0"` on the command line, or in the editor's console before Play. Typed mid-run, it does nothing until the level restarts |
| `sf.PirateAI` | 1 | 0: the pursuit operator declines the pirate's pilot post. It returns `false`, not a zeroed intent, which with assist on would brake the ship. The pirate coasts, the distance does not change (1011.2 m for 40 s at 60 and 400 fps), and it keeps firing. Read live. **A named exception**: only automated hulls read it, because only they seat this operator |
| `sf.KillThruster N [DelaySeconds]` | *(a command)* | Not a variable. Destroys thruster `N`, by its index in the ship data asset, on the player's ship (never an automated one), and rebuilds the array over the survivors. Runs once per entry, so twelve entries in one `-ExecCmds` list are twelve kills. The optional second argument delays the kill by that many seconds of game time, because every `-ExecCmds` entry runs on the first frame, before anything has spooled. Refuses with a `Warning` a thruster already destroyed, an index out of range, a malformed argument or a negative delay |

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

**The vertical slice is finished. Stage 4 — combat and the pirate — is merged: `main` is at
merge commit `a256a02`, fifty-one commits over the branch point; `main` now stands five commits past it, at `1e208f9`.** 88 headless tests, a build
from nothing with no warnings, and all three rigs green at 60 and 400 fps on the merged result.
Spec §13 ends at "Точка проверки 4. Срез готов." — there is no stage the spec names after this
one, and **the next step is a spec, not a plan.** Start from
`docs/superpowers/notes/2026-09-17-handoff-stage-5.md`.

**What stage 4 added.** Ballistics and damage maths in `StarForgeCore` under tests; a projectile
with a swept path and a muzzle on the turret; shields per face whose regeneration integrates over
the step (the switch-on form disagrees with itself by 43% between frame rates); hull strength and a
power-down that takes shields, turret and thrusters with it; thrusters destroyed one at a time with
the allocator rebuilt over the survivors and refusing once two corners cannot span six axes; and a
pirate that is `ASFTestShip` with `Crewing = Automated` — a pursuit operator in the pilot's seat and
stage 2's gunner automation in the other. There is no AI ship. The rig is `ASFCombatGameMode` on
`L_Combat`, and every launch line is above.

**Checkpoint 7 was taken by hand on 2026-09-17** and its form,
`docs/superpowers/notes/2026-09-14-checkpoint-7-results.md`, filled from eleven logs in
`notes/evidence/`: six controls counted, `skipped carries 0` in every line of every log, no carry
that stopped, no handover without a refusal, two hulls colliding with momentum conserved to the
last digit at both rates. Six of the ten by-eye rows are answered and four go forward as open
observations. Two of the answers are findings in their own right: the cabin is a blind cube — «буквально
бой я не вижу, только по панели» — and the collision reads as getting stuck, «как будто застрял
прижатым к потолку», where in truth the man is knocked down and carried throughout.

**The main risk of §12 is closed.** Motion decomposition survived four manual checkpoints (1, 4/5,
6, 7); plans B and C were never needed. What the slice carried forward is not in the mechanism but
in what a person meets first: a knockdown threshold that chatters at its edge (19.63 / 19.82 /
19.71 against 19.62, three trips in a row), a cabin the hatch left unsealed (`peak drift 884 cm`
wall to wall under a hull strike), a seated man never struck, a human who cannot take the gunner's
post, the owner's open choice between refusing to fly on a degraded array and flying sideways, and
a collision whose outcome does not repeat even on one build at one rate — which is why the gates on
that run are the carry counters and the numbers are protocol. The handoff lists all of it with
addresses.

**Every earlier flight check still passes at 60 and 400 fps**: 200.0 m/s saturating at exactly
800 kN, roll 180.0 and pitch/yaw 120.0 with nothing on the other axes, the scripted profile
standing the pilot's post down, the `sf.ShipGravity 0` control detaching the occupant, a
knocked-down occupant staying aboard, the loop's hatch crossing at `0.000 cm`. Warnings the
instruments cannot give and that still hold: take a seat on the move (sitting is one teleport the
drift gauge does not see); the interior flies with the occupant, so a ship turning at 180 deg/s
changes nothing on screen; `F` outside the pilot's seat answers in the log only.

**Next action:** read the stage-5 handoff, then write the spec for the phase after the slice. The
handoff's recommended order is: harden what a person feels (the list above), then a second-phase
spec whose first candidate is the crew — a third implementation of §4's operator beside the player
and the automation. The spec's own rules for that text: thresholds in seconds, never frames; every
constant derived, never re-read; every negative control performable by hand and able to fail.

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
- **`USFShipFrameComponent` asks a simulating hull for its angular velocity and differences
  only a carrier that has no body.** The branch in `TickComponent` looks like an optimisation
  over one uniform read, and the uniform read is what it replaced. Differencing a transform
  divides that transform's *error* by the frame time, and the transform an actor hands back is
  not its body's: every physics sync round-trips the rotation through an `FRotator`
  (`SceneComponent.cpp` 3314–3341), and `FQuat::Rotator()` answers exactly ±90° of pitch for
  anything within 0.081° of gimbal lock (`SINGULARITY_THRESHOLD 0.4999995f`, `UnrealMath.cpp`
  654–669). A fixed error in *position*, divided by a frame, is a rate that grows with the frame
  rate: 4.86 deg/s at 60 fps and 32.41 at 400.

  It cost a round to find, because it hid under a ceiling. On the spike rig under full pitch the
  instrument read 146.7–152.1 deg/s once every half turn where the hull turned 120.0 and Chaos
  reported 119.999355 on all 10002 frames; at r = 3 m the grav-plate ceiling is crossed at
  146.5 deg/s, so the occupant was knocked down eleven times at 400 fps and not once at 60. The
  arithmetic predicts it exactly: `120 + 0.0666 × 400 = 146.6`. Pure roll is immune — the
  engine's singularity test `Z·X − W·Y` is identically zero there — which is why checkpoint 5's
  three grav-plate thresholds and checkpoint 4's felt figure were unaffected and needed no
  re-measurement.

  Two consequences to keep in mind. The linear half of the same instrument is still differenced
  on every carrier, deliberately: a location passes through that round trip as the doubles it
  already was, and differencing the root keeps *v* the velocity of the frame origin, which is
  what the handover maths composes ω against — `GetPhysicsLinearVelocity` would hand back the
  centre of mass instead. And the two halves are now sampled differently, so on the frame of a
  collision `Footing LOST` reports the body's instantaneous turn in the hundreds of deg/s while
  the once-a-second `Flight:` line, sampled between impacts, reports tens. Both are right.

  **Which read a frame is on is in the log:** `Frame [X]: angular velocity read from the
  simulating body` or `… differenced from the transform -- no simulating body`, said on the
  first tick and again on every change. A hull says the first; the station and a docked hull say
  the second, because docking takes the hull out of simulation before the eased run-in. The dock
  cycle (`sf.Dock 2`) prints both, one per change.
- **A second console key, `F1`, in `Config/DefaultInput.ini`.** The engine binds the console
  to `Tilde` alone, and on a keyboard layout that does not produce Tilde from the key left of
  "1" the console never opens — silently, with nothing in the log. It cost a whole run of
  checkpoint 4 before it was understood, and every manual check that needs a command mid-run
  is impossible without it. `Tilde` is kept, so a Latin layout behaves exactly as before.
- **Shield regeneration integrated over the part of the step past the delay, not switched on
  once the timer passes it.** `RegenSeconds = max(0, (t + dt) - max(t, Delay))` in
  `TickShields` looks like an over-careful way of writing
  `if (timer > delay) regen += rate * dt`. The simple form gives two answers:
  - 500 for one 10-second step with a 3-second delay;
  - 350 for a thousand steps of 0.01 s.

  That is 43% apart between two frame rates, in a figure nobody would suspect. The integrated
  form gives 350 both ways. Measured in C++, the two agree to 4.4e-12, and the test holds them
  to 1e-9. The simple form was run as a mutant and fails three tests:
  `RegenIsFrameRateIndependent`, `RegenWaitsForTheDelay` and `AHitResetsOnlyItsOwnFaceTimer`.
- **The hull box blocks `ECC_WorldDynamic`.** It is one `SetCollisionResponseToChannel` in
  `ASFTestShip`'s constructor, and it looks unrelated to people. Without it two hulls overlap
  and pass through each other. On the first combat run a powered-down pirate coasting at
  106 m/s flew through the player's cabin, and its interior volume bound the man
  (`Frame handover: Ship -> Pirate`). How he was lost was not even repeatable. At 60 fps he fell
  out of every carry. At 400 fps he rode away on the pirate's floor with 284 skipped carries in
  one run, and in another run at the same rate he was left falling, as at 60.
  Blocking, the hulls collide the way §5 says a real physical body should. Contact comes at
  15.07 s (60) and 14.88 s (400); the pirate goes 79.6 → 28.5 m/s and the player's ship
  0 → 51.1 m/s at 60 fps, 81.2 → 23.5 and 0 → 57.7 at 400, so momentum is conserved to the last
  digit at both rates. No handover, 0 skipped carries. Nothing else physical in the project
  answers that channel, and both old rigs are line-for-line identical.

  **What the collision does not do is repeat itself across frame rates, and that is not a
  defect.** Two boxes meeting at 30 m/s resolve on a contact manifold the step size decides, so
  the same code leaves the player's hull tumbling at 21 deg/s of pitch at 60 fps and 170 at 400
  — and the tumble, not the impact, decides what happens to the man. At 60 fps he is knocked
  down for 0.5 s and stands up at the front wall. At 400 fps the hull's spin puts the
  centripetal term at r = 3 m over the 19.62 ceiling (which it crosses at 146.5 deg/s), so the
  knockdown never ends: he is pinned in the back-top corner, ship-local `(-356, y, +302)`,
  sliding along −Y at about 7 cm/s, away from the hatch, for 65.7 s of an 80 s window. The
  drift line reads `784.6 cm` and that is his corner, not an escape — `skipped carries 0`,
  `Carry NOT RUNNING` never, the carry called once a frame throughout. Read the verdict and the
  carry counters, never the figures, when judging this run.
- **An interior volume hands a person over only between carriers moving within 1.0 m/s of each
  other where he stands.** `USFInteriorVolumeComponent::TryHandover` looks like it second-guesses
  stage 3's rule that entering a volume binds. It is what keeps a man in his own frame when hulls
  touch, not the CCD thresholds below. The figure is not new: it is the docking window's speed
  limit, the one number the design has for two carriers at rest relative to each other. It is
  measured at his position, because a spinning deck moves at ω × r.
  - A docked pair reads `0.000000 m/s` at 60 and 400 fps, so the hatch crossing is unchanged by
    construction.
  - A man against the struck wall was refused at 53.28 m/s at 60 fps.
  - At 400 fps the pirate's volume never reached into the cabin; outside the hull the refusal
    came at 81.19 m/s.

  `Frame handover REFUSED` is a `Warning`, said once per episode, and it is legitimate during a
  docking capture and whenever hulls collide. A refused man is retried every tick while he stays
  inside, so one refused during a capture binds to the station once it settles. The 1.0 is a
  literal: editing the docking window does not move it.
- **CCD thresholds in `Config/DefaultEngine.ini` that do not bound penetration.** The two lines
  `p.Chaos.CCD.EnableThresholdBoundsScale=0.01` and `p.Chaos.CCD.AllowedDepthBoundsScale=0.005`
  come with a comment full of arithmetic, and they read like a promise that hulls never overlap
  by more than 8 cm. They are not one. The scales multiply the hull's full 800 cm extent, so a
  step is swept only when the pair moves more than 8 cm relative to each other. Ordinary contact
  builds penetration over several shorter, unswept steps.

  Measured on the duel, where the pirate arrives pitched 8.53° and first touches at 854.9 cm
  between centres:
  - the engine defaults, 0.4 and 0.2, sweep only steps over 320 cm and let the hulls overlap by
    about 122 cm at 60 fps;
  - these values cut the overlap to about 10 cm at 60 fps;
  - at 400 fps about 37 cm is left, because the hulls close at about 30 m/s, 7.45 cm a step,
    under the threshold.

  The values stay for the overlap they remove; the handover guard above is what protects the
  person. They are global to Chaos and reach only bodies with `bUseCCD`, which today is the hull
  alone.
- **`FThrustAllocator::Build` refuses unless each probe axis returns itself within 1e-3.** It
  looks stricter than a layout that is obviously fine needs. The old check refused only when an
  achieved component was under 1e-6, that is, only when a coordinate axis was wholly unreachable.
  It returned ready at rank 5 (two full corners) and at rank 3 (one corner): a ship that "built"
  and could not do what it was asked. The new criterion is provably enough, since some component
  of any null vector, squared, is at least 1/6.
  - `TwoCornersCannotSpanSixAxes` failed against the old check.
  - Twelve `sf.KillThruster` commands on the spike rig now refuse on exactly the twelfth, with
    one `Error` and `thrusters 12/24 NOT built`, at 60 and 400 fps.

  The tolerance is absolute, so an array of thrusters of a few newtons would be refused; the
  project's 200 kN is nowhere near that. It checks linear span, not reachability with throttles
  confined to [0, 1]: a `+X/+Y/+Z`-only layout still builds, whatever older comments say.
- **`Rebuild` carries each surviving thruster's spooled and commanded throttle across by asset
  index.** Mapping the columns through `LiveAssetIndices` looks like bookkeeping a rebuild could
  skip by zeroing. It used to zero them, and a thruster lost in flight snapped every survivor to
  nothing. Three frames after the kill at 60 fps the ship read `accel 8.9`, `got 444444 N`,
  `0 spooled`, and lost 0.5 m/s. Carried, it reads `accel 16.0`, `got 800000 N`, `4 spooled`, and
  the speed goes 79.1 → 95.1 → 111.1 → 127.1 m/s, the same figures as a kill before launch. At
  400 fps it is 16.0 before and after (79.4 → 95.4 → 111.5 → 127.5). Those speeds were 0.1 m/s
  higher until the spool was given the lag's exact solution: a forward-Euler step of the same
  time constant arrives a shade early, and the sequence is the place it shows. No path needs a reset: undocking carries zeros, because
  docking locks the array and clears its demand. A kill in flight can only be asked for because
  `sf.KillThruster` takes a delay.
- **The pursuit asks for `sign(e) * sqrt(2 * a * |e|)` with `a = 8 m/s²`, scaled by
  `max(0, cos(pitch) * cos(yaw))`.** It looks like an elaborate replacement for "full stick
  until 350 m, linear down to 250". That simpler law was measured. The pirate built up 143 m/s,
  which needs 640 m to stop at 16 m/s². It flew through the player's ship at 12 s, 12.1 m apart,
  and then swung between 509, 125 and 444 m for the rest of the run.

  The new law asks for the speed from which a constant deceleration stops the ship at the hold
  distance. The pirate enters 300 ± 50 m at about 12.3 s at 60 and 400 fps, never goes below
  300.0 m, and holds 300.0 from 19 s.
  - `a` is half the intact 16 m/s², so a ship that has lost a thruster (12 m/s²) still stops
    inside the curve.
  - Inside the 50 m band the demand falls linearly from 28.3 m/s, because the root's slope is
    infinite at zero.
  - The cosine gate stops it thrusting forward, and so away, while its target is behind it
    during a turn.

  Its limit: it asks for an absolute speed, not a closing speed, so it trails a target that runs
  by `Hold + v²/(2a)`. At 200 m/s that is 2.8 km, and 200 m/s is the pirate's own top speed.
- **The player's ship on the combat rig does not fire without `sf.Fire 1`, and neither does any
  ship on the spike and loop rigs.** `bWeaponsFree = false` on `ASFTestShip` looks like a
  disarmed default nobody went back to. There are two reasons for it.
  - The automation holds its trigger whenever it has a target, so a muzzle on a weapons-free hull
    would have both measurement rigs shooting at their drone.
  - Armed on both sides, two identical hulls aiming at each other's centres put identical shots
    into identical faces. They powered down in the same frame at 6.683 s at 60 fps, and one
    frame apart at 400 (6.6600 and 6.6625 s), before the pursuit got
    anywhere near its hold distance, with 33 shots in ten seconds instead of 50. That was a
    mirror, not a fight.

  The pirate stays armed.
- **A powered-down ship's shields go to zero and its turret locks with its own lock; docking
  locks neither.** The plan's power-down was only the thruster array's `SetLocked`, so zeroing
  six faces and calling `USFTurretComponent::SetLocked` in `PowerDown` looks like two
  consequences nobody asked for. Without them:
  - faces frozen with charge left would go on absorbing hits on a ship that reads
    "powered down";
  - a pirate that has lost would go on aiming and firing.

  Measured in the duel at 60 and 400 fps: `Combat: … shields +X 0 … powered down`,
  `Turret: … (standing down) … UNCOMMANDED`, and 0 shots after the event. The turret's lock is
  its own, and only `PowerDown` sets it, so the docked loop rig still reads `ON TARGET` and is
  line-for-line identical. The thruster lock is still one bool shared by every reason: undocking
  clears it unconditionally, so a ship powered down while docked would fly again after undocking.
  No rig can do that to a ship this stage.

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
