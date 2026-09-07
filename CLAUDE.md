# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

StarForge is a solo-developed space-sim prototype in Unreal Engine 5.8.2. The repository currently holds **only the vertical slice** — a technical probe, not a game. Its job is to answer architectural questions before content exists. Everything renders as grey engine primitives on purpose.

Design docs are in Russian under `docs/superpowers/`; code, comments and commit messages are in English.

- `docs/superpowers/specs/` — the design spec. The authority on *why*. Section numbers (§6, §7, §12) are referenced throughout the code and plans.
- `docs/superpowers/plans/` — implementation plans, task-by-task with checkboxes.
- `docs/superpowers/notes/` — checkpoint runbooks and result forms for manual measurements.

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

**Play the measurement rig:**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

**Open the editor** (launch this binary directly; the launcher's `UnrealVersionSelector` may show a spurious engine-conversion prompt):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject"
```

**Close the editor before building.** A running editor holds the Live Coding lock and the build will fail. Check with `tasklist | findstr Unreal`.

## Module split — the load-bearing boundary

Two runtime modules, and the split is the single most important structural fact in the repo.

**`StarForgeCore`** depends on `Core` and `CoreUObject` only. **Adding `Engine` to it is an error that destroys the point of the split.** It holds all the maths — reference-frame conversions, and later the thruster efficiency matrix, thrust solver and flight-control loop. No actors, no components, no meshes. The reason is testability: a bug in the thrust solver does not crash or log, it surfaces weeks later as "the ship feels wrong". A headless unit test catches it in a second.

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

The branch `feat/frame-spike` covers spec stages 0–1 (foundation and measurement). Stage 1 ends at a fork: the measurement either confirms motion decomposition or moves the project to the plan B in spec §12. **Do not plan stages 2–4 in detail before that answer exists.**

Two things are deliberately unfinished and cannot be automated:

- `Content/` is empty. The map `/Game/Maps/L_FrameSpike` that `DefaultEngine.ini` points at **does not exist yet** — it must be authored once by hand as an *empty* level. The ship, character and HUD are all assembled in code by `ASFSpikeGameMode::StartPlay`, so an empty level is the correct level.
- The checkpoint-1 measurement is a manual run. Follow `docs/superpowers/notes/2026-09-06-checkpoint-1-runbook.md`.

When reading the checkpoint HUD: **check the measured-time line before the drift figure.** A drift of zero is what both a perfect minute and a dead instrument look like, and only `measured` tells them apart.
