# -*- coding: utf-8 -*-
"""Replay every headless run kept in the evidence folder and compare what the rigs print now.

Every flight check in this project ends as a log in docs/superpowers/notes/evidence/, and almost
every log carries the command line that produced it (four 2026-09-08 extracts of LogStarForge
lines do not, and are not replayed). So a regression needs no list of its own: take each headless
log, run its command line again, and compare the project's lines. The engine may log `-game` last
whatever position it was given, so command lines are compared, and grouped, argument by argument.
A replay drops `-abslog=`, which says only where the log went: run_engine reads Saved/Logs.

    python scripts/regress_evidence.py                 # rebuild, test, replay everything
    python scripts/regress_evidence.py --skip-build    # replay only; the last build record is kept
    python scripts/regress_evidence.py --only combat   # replays whose baseline name contains it
    python scripts/regress_evidence.py --resume        # keep results already on disk

Verdicts, per replayed command at a fixed step (`-benchmark`):

    SAME          every LogStarForge line, timestamp and frame number stripped, equals the baseline
    CHANGED       differs, and a second replay equals the first: the build prints something else now
    VARIES        differs, and the second replay differs from the first: the run does not repeat
                  itself (two hulls colliding resolve on a contact manifold the step decides; see
                  CLAUDE.md), so its figures are protocol and only its red counters are judged --
                  both replays' counters, and the worse of the two
    CHANGED?      differs, and the second replay could not be read, so change and variance cannot
                  be told apart
    INVALID       two launches in a row could not be read. A launch is read only if it exited with
                  0, its log says `Log file closed` and carries at least one LogStarForge line, and
                  StarForge.log or StarForge_2.log carries its command line (a run the engine wrote
                  to StarForge_2.log beside an open editor is found and accepted). The command line
                  alone proves nothing: the engine logs it long before any world exists, so a run
                  that died on startup carries it too

A run with a variable step (`-seconds` and no `-benchmark`) is the only kind that can fail on a
defect of F4's class (CLAUDE.md), and its lines do not repeat from one run to the next. Each such
command is replayed VARIABLE_REPLAYS times and judged on the red counters alone, against every good
log of that command. A counter those logs agree on is held to that count; a counter they disagree
on is a figure of the run like any other, and only whether it trips at all is judged:

    STEADY        no replay moved a counter the way the next two verdicts describe
    RED ROSE      some replay is above a count every good log agrees on -- most often a counter none
                  of them ever tripped
    RED FELL      none rose, and some replay is below a count every good log agrees on, or at 0 on
                  a counter every good log tripped: how a negative control that no longer trips reads
    INVALID       any one of the replays could not be read, twice in a row

CHANGED is not a verdict of failure. The tree moves, and a change after a known commit is expected;
the report lists what changed so a person can say which. What a person must never have to do is
find the difference by eye.

Exit status: 2 when this invocation's build or unit tests failed, and then nothing is replayed;
1 when a replay or a claim is INVALID, a second replay could not be read (CHANGED?), a red counter
rose above its baseline, or a claim is BROKEN -- counted over every result in the report, those
--resume kept included; 0 otherwise.

Results go to --out (default: Saved/Regress under the project root, which .gitignore keeps out of
the repository), one JSON per replay so that --resume survives an interrupted sweep, plus
report.md, which says what this invocation ran, what it kept from disk and what it left out.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EVIDENCE = ROOT / "docs" / "superpowers" / "notes" / "evidence"
LOGS = ROOT / "Saved" / "Logs"
UPROJECT = ROOT / "StarForge.uproject"
ENGINE = Path(r"C:\Program Files\Epic Games\UE_5.8")
EDITOR = ENGINE / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
EDITOR_CMD = ENGINE / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
BUILD = ENGINE / "Engine" / "Build" / "BatchFiles" / "Build.bat"
REBUILD = ENGINE / "Engine" / "Build" / "BatchFiles" / "Rebuild.bat"

# Logs that are not baselines of the current tree, by construction. Each is kept in evidence as
# proof of something that was wrong, and replaying it today is supposed to disagree with it.
NOT_A_BASELINE = ("BROKEN", "BEFORE-FIX", "MISSING", "unidentified")

# Logs of a scratch build, told by their content: five of task 7's seven diagnostic logs of
# 2026-09-14 name no variable that is gone (UNREPLAYABLE below catches the other two), because the
# diagnostics were compiled into that build. Their `DIAG` lines come from code that never reached
# the tree, so no build can print them again, and replayed today these commands run another
# experiment: in task7-r4-diag-board-400 the replay's man stands in the cabin instead of on the
# shelf outside the hull, and its red counters fall (Warning 16 -> 0, Carry NOT RUNNING 15 -> 0)
# because the thing they counted is no longer there.
SCRATCH_BUILD = re.compile(r"LogStarForge: (?:\w+: )?DIAG ")

# Command lines that cannot be replayed: they name a console variable that no longer exists.
UNREPLAYABLE = {
    "sf.SpikeOriginOffsetM": "the world-size spike's throwaway variable, rolled back on 2026-09-17",
    "sf.SpikeDockGapCm": "F3's throwaway docked-gap variable, measured in a scratch worktree on 2026-09-25",
    "sf.DiagSpawn": "task 7's throwaway spawn diagnostic of 2026-09-14, never committed; the "
                    "checkpoint-7 form says these figures can only be re-taken with it",
}

# Replays per variable-step command. Before its fix F4 knocked a man down in 12 of 12 uncapped runs
# with a release in them, so one replay would have caught it; the second is for a defect that shows
# in some runs and not in others, as the let-go's lost last step did in six releases of twelve.
VARIABLE_REPLAYS = 2

# Red counters: judged on every run, whatever the verdict.
RED = {
    "Error": r"LogStarForge: Error",
    "Warning": r"LogStarForge: Warning",
    "skipped>0": r"skipped carries [1-9]",
    "NOT RUNNING": r"Carry NOT RUNNING",
    "CARRY FAILED": r"CARRY FAILED",
    "cross-check": r"while bound to",
    "handover REFUSED": r"Frame handover REFUSED",
    "Footing LOST": r"Footing LOST",
}

# The family whose line carries the drift verdict (`footing intact`, `KNOCKED DOWN`, ...): reported
# with the red-line families, ahead of the rest.
DRIFT = "Shipboard drift"

STAMP = re.compile(r"^\[[^\]]*\]\[[^\]]*\]")

# Controls CLAUDE.md documents that no headless log in evidence carried when this list was written
# (2026-09-23; the two undock-thrust-x claims have since been kept in evidence as
# 2026-09-23-review-undock-sideways-*.log and are replayed as baselines too). Each has no baseline, so
# it is judged by the lines the guide says it must and must not produce -- the guide's own words,
# which is what makes a drift between guide and code show up here rather than at a keyboard.
# `every` holds a claim on every line of a kind from a given second on: (the line, from which
# second, what each such line must say). `sf.FlightLog` writes one `Flight:` line a second, so the
# n-th line is the n-th second.
_SPIKE = "-game -nullrhi -unattended -nosplash -benchmark"
_LOOP = "/Game/Maps/L_Loop " + _SPIKE
_NOTHING_ON_ROLL_PITCH = r"ang roll/pitch/yaw \(-?0\.0 -?0\.0 "
# The yaw spin-up, measured in the 2026-09-30 sweep: 116.0 at 1 s and 119.9 at 2 s at 60 fps, 116.1
# and 119.9 at 400, and 120.0 on every line from the third second on at both rates.
_YAW_AT_RATE = (r"Flight: \[Ship\]", 3, r"ang roll/pitch/yaw \(-?0\.0 -?0\.0 -?120\.0\)")
CLAIMS = [
    {"name": "claim-shipgravity0-60",
     "doc": "sf.ShipGravity 0 (with sf.Knockdown 0) detaches the occupant and the carry stops",
     "cmd": f'{_SPIKE} -fps=60 -seconds=12 -ExecCmds="sf.FlightLog 1, sf.ShipGravity 0, sf.Knockdown 0" -log',
     "must": [r"Carry NOT RUNNING"], "must_not": []},
    {"name": "claim-shipgravity0-400",
     "doc": "same, at 400 fps",
     "cmd": f'{_SPIKE} -fps=400 -seconds=12 -ExecCmds="sf.FlightLog 1, sf.ShipGravity 0, sf.Knockdown 0" -log',
     "must": [r"Carry NOT RUNNING"], "must_not": []},
    {"name": "claim-yaw-60",
     "doc": "yaw 120.0 deg/s on every Flight line from the third second on, after the spin-up, and "
            "nothing on the other axes on any line",
     "cmd": f'{_SPIKE} -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.Yaw 1" -log',
     "must": [], "must_not": [r"LogStarForge: Error"],
     "every": [(r"Flight: \[Ship\]", 1, _NOTHING_ON_ROLL_PITCH), _YAW_AT_RATE]},
    {"name": "claim-yaw-400",
     "doc": "same, at 400 fps",
     "cmd": f'{_SPIKE} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.Yaw 1" -log',
     "must": [], "must_not": [r"LogStarForge: Error"],
     "every": [(r"Flight: \[Ship\]", 1, _NOTHING_ON_ROLL_PITCH), _YAW_AT_RATE]},
    {"name": "claim-targetdrone0-60",
     "doc": "sf.TargetDrone 0: the turret says it has no target rather than aim at the origin",
     "cmd": f'{_SPIKE} -fps=60 -seconds=10 -ExecCmds="sf.FlightLog 1, sf.TargetDrone 0" -log',
     "must": [r"NO TARGET"], "must_not": [r"ON TARGET"]},
    {"name": "claim-bindframe-station-undocked-60",
     "doc": "sf.BindFrame station, undocked under thrust: the cross-check warns and the carry stops",
     "cmd": f'{_LOOP} -fps=60 -seconds=20 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1, sf.BindFrame station" -log',
     "must": [r"while bound to", r"Carry NOT RUNNING"], "must_not": []},
    {"name": "claim-bindframe-station-undocked-400",
     "doc": "same, at 400 fps",
     "cmd": f'{_LOOP} -fps=400 -seconds=20 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1, sf.BindFrame station" -log',
     "must": [r"while bound to", r"Carry NOT RUNNING"], "must_not": []},
    {"name": "claim-undock-thrust-x-station-60",
     "doc": "undock and thrust along +X beside a physical station: flies to 200.0 m/s, man stays aboard",
     "cmd": f'{_LOOP} -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1" -log',
     "must": [r"speed 200\.0 m/s"], "must_not": [r"Carry NOT RUNNING", r"skipped carries [1-9]", r"while bound to"]},
    {"name": "claim-undock-thrust-x-station-400",
     "doc": "same, at 400 fps",
     "cmd": f'{_LOOP} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1" -log',
     "must": [r"speed 200\.0 m/s"], "must_not": [r"Carry NOT RUNNING", r"skipped carries [1-9]", r"while bound to"]},
]


def command_line(log_text):
    m = re.search(r"LogInit: Command Line: (.*)", log_text)
    return m.group(1).strip() if m else None


_ARG = re.compile(r'(?:[^\s"]+|"[^"]*")+')


def same_command(logged, cmd):
    """Whether the log's command line is this command, argument for argument.

    Compared as a multiset of arguments, not as a string: with no map on the command line the
    engine logs `-game` last whatever position it was given (`-nullrhi ... -log -game`), and a
    literal comparison refused every spike-rig run as not its own. Quoted arguments stay whole,
    so the order inside -ExecCmds still counts.
    """
    if logged is None:
        return False
    return sorted(_ARG.findall(logged)) == sorted(_ARG.findall(cmd))


def replay_command(cmd):
    """The logged command less `-abslog=`: every variable-step log of 2026-09-27 wrote to a scratch
    path with it, and a replay has to write Saved/Logs, where run_engine reads it."""
    args = _ARG.findall(cmd)
    if not any(a.startswith("-abslog=") for a in args):
        return cmd
    return " ".join(a for a in args if not a.startswith("-abslog="))


def command_key(cmd):
    """Which runs are one run: the same arguments in any order, as same_command reads them. Keyed by
    the string instead, `-fps=60 -seconds=20` and `-seconds=20 -fps=60` were two commands, and the
    four task-3 logs of 2026-09-14 were replayed as baselines of their own beside the checkpoint-7
    B5 logs of the same runs three days later."""
    return tuple(sorted(_ARG.findall(replay_command(cmd))))


def project_lines(log_text):
    """Every LogStarForge line with its timestamp and frame number stripped, in order."""
    return [STAMP.sub("", l) for l in log_text.splitlines() if "LogStarForge" in l]


def family(line):
    """The first words after the verbosity: 'Flight:', 'Dock:', 'Place size:', ...

    A line with a number before its first colon falls back to its first sixteen characters with
    every number written `#`. Cut first, every such line was a family of its own: 800 of the 810
    families of task7-r3-diag-contact-400 were `DIAG t 14.00xx s`.
    """
    body = re.sub(r"^LogStarForge: (Display: |Warning: |Error: |Verbose: )?", "", line)
    m = re.match(r"([A-Za-z][A-Za-z ]{0,24}?[:\[])", body)
    return m.group(1).rstrip("[").strip() if m else re.sub(r"\d[\d.]*", "#", body)[:16]


def is_red_line(line):
    return any(re.search(p, line) for p in RED.values())


def red_counts(log_text):
    return {k: len(re.findall(p, log_text)) for k, p in RED.items()}


_OPENED = re.compile(r"Log file open, (\d\d)/(\d\d)/(\d\d) (\d\d:\d\d:\d\d)")


def opened_at(text, name):
    """When the engine opened this log, as a sortable key; the file name if the log does not say.

    File names are not a clock: inside one day they sort alphabetically, so the first form of
    this script took `physics-F1-slow-intrusion-60` (11:25) as the baseline over `f2fix-F1-60`
    (14:21) with the same command, and reported a change the later log did not have.
    """
    m = _OPENED.search(text[:2000])
    if not m:
        return ("", name)
    mm, dd, yy, hms = m.groups()
    return (f"20{yy}-{mm}-{dd} {hms}", name)


def collect():
    """One replay per distinct headless command, with its most recently opened good log as baseline
    at a fixed step, and every good log as a baseline at a variable step. Logs with no command line
    at all (the 2026-09-08 extracts of LogStarForge lines) and logs that are not headless are left
    out: there is nothing to replay. Every other log left out is listed in `skipped` with the reason."""
    by_key = {}
    skipped = []
    for p in sorted(EVIDENCE.glob("*.log")):
        text = p.read_text("utf-8", errors="replace")
        cmd = command_line(text[:400000])
        if not cmd or "-nullrhi" not in cmd or "RunTests" in cmd:
            continue
        if "-benchmark" in cmd:
            step = "fixed"
        elif "-seconds=" in cmd:
            step = "variable"
        else:
            skipped.append((p.name, "headless with neither -benchmark nor -seconds: it would not end by itself"))
            continue
        why = next((w for k, w in UNREPLAYABLE.items() if k in cmd), None)
        if why:
            skipped.append((p.name, why))
            continue
        group = by_key.setdefault(command_key(cmd), {"step": step, "entries": []})
        group["entries"].append({"opened": opened_at(text, p.name), "name": p.name,
                                 "cmd": replay_command(cmd), "scratch": bool(SCRATCH_BUILD.search(text))})
    plan, varplan = [], []
    for group in by_key.values():
        entries = sorted(group["entries"], key=lambda e: e["opened"])
        good = [e for e in entries if not e["scratch"] and not any(t in e["name"] for t in NOT_A_BASELINE)]
        if not good:
            why = ("only logs of task 7's scratch build, with its temporary DIAG lines, carry this command"
                   if all(e["scratch"] for e in entries) else "only broken or pre-fix logs carry this command")
            skipped += [(e["name"], why) for e in entries]
            continue
        base = good[-1]
        rec = {"baseline": base["name"], "cmd": base["cmd"], "step": group["step"],
               "also": [e["name"] for e in entries if e is not base]}
        if group["step"] == "variable":
            rec["baselines"] = [e["name"] for e in good]
            rec["also"] = [e["name"] for e in entries if e not in good]
            varplan.append(rec)
        else:
            plan.append(rec)
    plan.sort(key=lambda r: r["baseline"])
    varplan.sort(key=lambda r: r["baseline"])
    return plan, varplan, skipped


def hidden():
    """Keep every launch off the desktop. `-log` makes the engine open a console window of its
    own, and a sweep of a hundred runs opens one a minute on a machine somebody is working at."""
    if sys.platform != "win32":
        return {}
    si = subprocess.STARTUPINFO()
    si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    si.wShowWindow = 0  # SW_HIDE
    return {"startupinfo": si}


def unreadable(text, returncode):
    """Why the log of a launch whose command line matched cannot stand for a run, or None.

    The 2026-09-27 sweep's first replay of 2026-09-22-f2fix-F1-60 lasted 6.1 s of a 30-second run and
    ended at `Engine exit requested (reason: ConsoleCtrl RequestExit)`, CLAUDE.md's exit-130 startup
    abort, with not one LogStarForge line. Its command line matched, so it was taken as a replay: an
    empty run against a full baseline read VARIES, with every red counter at 0.
    """
    if returncode != 0:
        return f"exit code {returncode}"
    if "Log file closed" not in text:
        return "its log was never closed"
    if not project_lines(text):
        return "not one LogStarForge line"
    return None


def run_engine(cmd):
    """Run one headless command and return (the text of the log it wrote or None, why that log cannot
    be read or None)."""
    for stale in ("StarForge.log", "StarForge_2.log"):
        f = LOGS / stale
        if f.exists():
            try:
                f.unlink()
            except OSError:
                pass  # held by a running editor; the command-line check below will catch it
    full = f'"{EDITOR}" "{UPROJECT}" {cmd}'
    r = subprocess.run(full, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, **hidden())
    for name in ("StarForge.log", "StarForge_2.log"):
        f = LOGS / name
        if f.exists():
            text = f.read_text("utf-8", errors="replace")
            if same_command(command_line(text[:400000]), cmd):
                return text, unreadable(text, r.returncode)
    return None, f"exit code {r.returncode}, and neither StarForge.log nor StarForge_2.log carries this command line"


def replay(cmd, rec):
    """Run, and run once more if the launch could not be read: a launch that dies on startup says
    nothing about the build. Every launch that could not be read is listed in the record."""
    text, why = run_engine(cmd)
    if why:
        rec.setdefault("unreadable_launches", []).append(why)
        text, why = run_engine(cmd)
        if why:
            rec["unreadable_launches"].append(why)
    return text, why


def first_difference(a, b):
    """Per family that differs: its line counts and its first differing pair. `red` marks a family
    with a line that matches a red counter, or the drift line, so the report can put it first."""
    fa, fb = {}, {}
    for l in a:
        fa.setdefault(family(l), []).append(l)
    for l in b:
        fb.setdefault(family(l), []).append(l)
    out = []
    for fam in sorted(set(fa) | set(fb)):
        x, y = fa.get(fam, []), fb.get(fam, [])
        if x == y:
            continue
        pair = next(((p, q) for p, q in zip(x, y) if p != q), None)
        out.append({"family": fam, "baseline_n": len(x), "now_n": len(y),
                    "red": fam.startswith(DRIFT) or any(is_red_line(l) for l in x + y),
                    "was": pair[0] if pair else (x[len(y)] if len(x) > len(y) else None),
                    "now": pair[1] if pair else (y[len(x)] if len(y) > len(x) else None)})
    return out


def replay_fixed(r, out, tag, head):
    base_text = (EVIDENCE / r["baseline"]).read_text("utf-8", errors="replace")
    t0 = time.time()
    rec = {"baseline": r["baseline"], "cmd": r["cmd"], "step": "fixed", "commit": head,
           "red_baseline": red_counts(base_text)}
    now, why = replay(r["cmd"], rec)
    if why:
        rec["verdict"] = "INVALID"
        rec["invalid"] = why
        if now is not None:
            (out / "logs" / f"{tag}.invalid.log").write_text(now, "utf-8")
    else:
        (out / "logs" / f"{tag}.now.log").write_text(now, "utf-8")
        rec["red_now"] = red_counts(now)
        a, b = project_lines(base_text), project_lines(now)
        rec["lines"] = len(b)
        if a == b:
            rec["verdict"] = "SAME"
        else:
            rec["diff"] = first_difference(a, b)
            again, why = replay(r["cmd"], rec)
            if why:
                rec["verdict"] = "CHANGED?"  # could not repeat to tell change from variance
                rec["again_invalid"] = why
            else:
                rec["red_again"] = red_counts(again)
                if project_lines(again) == b:
                    rec["verdict"] = "CHANGED"
                else:
                    rec["verdict"] = "VARIES"
                    rec["varies_in"] = [d["family"] for d in first_difference(b, project_lines(again))]
                    (out / "logs" / f"{tag}.again.log").write_text(again, "utf-8")
    rec["seconds"] = round(time.time() - t0, 1)
    return rec


def judge_variable(low, high, reads):
    """The red counters a variable-step replay took up and down, judged against its good logs.

    A count the good logs disagree on is not held to their range. `sf.FeltTimeBase 0`'s dock-cycle
    control knocked the man down 3, 2, 2, 2 and 1 times in its five logs, and the defect it puts
    back did 3, 1, 3, 2 and 4 in the five uncapped runs before F4's fix. A replay that reads 4 is
    the control working; held to 1..3, one run in those ten would have failed it, which over
    VARIABLE_REPLAYS replays is about one sweep in five. Only its falling to 0 is judged, because a
    control that never trips no longer controls anything.
    """
    rose = [k for k in RED if low[k] == high[k] and any(c[k] > high[k] for c in reads)]
    fell = [k for k in RED if any(c[k] < low[k] and (low[k] == high[k] or c[k] == 0) for c in reads)]
    return rose, fell


def replay_variable(r, out, tag, head):
    base = [red_counts((EVIDENCE / n).read_text("utf-8", errors="replace")) for n in r["baselines"]]
    t0 = time.time()
    rec = {"baseline": r["baseline"], "baselines": r["baselines"], "cmd": r["cmd"], "step": "variable",
           "commit": head, "red_low": {k: min(c[k] for c in base) for k in RED},
           "red_high": {k: max(c[k] for c in base) for k in RED}, "red_replays": []}
    invalid = []
    for i in range(1, VARIABLE_REPLAYS + 1):
        text, why = replay(r["cmd"], rec)
        if why:
            invalid.append(why)
            continue
        (out / "logs" / f"{tag}.var{i}.log").write_text(text, "utf-8")
        rec["red_replays"].append(red_counts(text))
    rec["rose"], rec["fell"] = judge_variable(rec["red_low"], rec["red_high"], rec["red_replays"])
    if invalid:
        rec["verdict"] = "INVALID"
        rec["invalid"] = "; ".join(invalid)
    else:
        rec["verdict"] = "RED ROSE" if rec["rose"] else "RED FELL" if rec["fell"] else "STEADY"
    rec["seconds"] = round(time.time() - t0, 1)
    return rec


def judge_claim(c, now):
    """What a claim's run misses of what the guide says it must print, and what it prints that the
    guide says it must not."""
    missing = [p for p in c["must"] if not re.search(p, now)]
    present = [p for p in c["must_not"] if re.search(p, now)]
    for kind, from_s, p in c.get("every", ()):
        lines = [l for l in now.splitlines() if re.search(kind, l)][from_s - 1:]
        held = sum(1 for l in lines if re.search(p, l))
        if not lines or held < len(lines):
            missing.append(f"{p} on every {kind} line from second {from_s} ({held} of {len(lines)})")
    return missing, present


def build_and_test(out):
    """Rebuild, then run the unit tests. A sweep over binaries this tree did not build, or that fail
    their own tests, describes something else, so main stops on either."""
    res = {}
    r = subprocess.run(f'"{REBUILD}" StarForgeEditor Win64 Development -Project="{UPROJECT}" -WaitMutex',
                       cwd=ROOT, capture_output=True, text=True, errors="replace", **hidden())
    (out / "rebuild.log").write_text(r.stdout + r.stderr, "utf-8")
    log = r.stdout + r.stderr
    res["build"] = {
        "succeeded": r.returncode == 0 and "Result: Succeeded" in log,
        "exit": r.returncode,
        "warnings": len(re.findall(r"warning", log, re.I)),
        "errors": len(re.findall(r"\berror\b", log, re.I)),
        "actions": len(re.findall(r"^\[\d+/\d+\]", log, re.M)),
    }
    if not res["build"]["succeeded"]:
        res["tests"] = {"ran": False, "why": "the build failed"}
        return res
    # The same three cases run_engine handles: a log left by an earlier run, a log the engine had to
    # write as StarForge_2.log beside an open editor, and a log that belongs to another session.
    for stale in ("StarForge.log", "StarForge_2.log"):
        f = LOGS / stale
        if f.exists():
            try:
                f.unlink()
            except OSError:
                pass
    subprocess.run(f'"{EDITOR_CMD}" "{UPROJECT}" -ExecCmds="Automation RunTests StarForge.Core;Quit" '
                   f'-unattended -nopause -nullrhi -nosplash -log', cwd=ROOT, stdin=subprocess.DEVNULL,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **hidden())
    for name in ("StarForge.log", "StarForge_2.log"):
        f = LOGS / name
        if f.exists():
            t = f.read_text("utf-8", errors="replace")
            if "RunTests StarForge.Core" in (command_line(t[:400000]) or ""):
                shutil.copy(f, out / "tests.log")
                fails = re.findall(r"Result=\{Fail\} Name=\{([^}]+)\}", t)
                res["tests"] = {"success": t.count("Result={Success}"), "fail": len(fails), "failed": fails,
                                "exit": (re.search(r"EXIT CODE: (\d+)", t) or [None, "?"])[1]}
                return res
    res["tests"] = {"ran": False, "why": "neither StarForge.log nor StarForge_2.log carries the test run's command line"}
    return res


def build_failure(res):
    """Why this invocation's build or tests mean nothing should be replayed, or None."""
    b, t = res.get("build", {}), res.get("tests", {})
    if not b.get("succeeded"):
        return f"the build failed (exit {b.get('exit')}); see rebuild.log"
    if t.get("ran") is False:
        return f"the unit tests did not run: {t.get('why')}"
    if t.get("fail") or t.get("exit") != "0" or not t.get("success"):
        return (f"the unit tests failed: {t.get('success')} passed, {t.get('fail')} failed "
                f"{t.get('failed')}, exit code {t.get('exit')}; see tests.log")
    return None


def git_head():
    """The commit the tree stands on, so that a result kept by --resume can be told from one taken now."""
    try:
        r = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, **hidden())
    except OSError:
        return None
    return r.stdout.strip() or None


def main():
    ap = argparse.ArgumentParser(description="Replay the headless evidence logs against the current build.")
    ap.add_argument("--out", default=None, help="results directory (default: Saved/Regress)")
    ap.add_argument("--only", default=None, help="replay only the commands whose baseline name contains this; no claims")
    ap.add_argument("--skip-build", action="store_true", help="do not rebuild or run the unit tests; the last record is kept")
    ap.add_argument("--resume", action="store_true", help="keep the results already on disk")
    args = ap.parse_args()

    out = Path(args.out) if args.out else ROOT / "Saved" / "Regress"
    (out / "logs").mkdir(parents=True, exist_ok=True)
    plan, varplan, skipped = collect()
    sizes = {"fixed": len(plan), "variable": len(varplan)}
    if args.only:
        plan = [r for r in plan if args.only in r["baseline"]]
        varplan = [r for r in varplan if args.only in r["baseline"]]
    head = git_head()

    summary_file = out / "summary.json"
    previous = json.loads(summary_file.read_text("utf-8")) if summary_file.exists() else {}
    summary = {"commit": head,
               "invocation": {"only": args.only, "skip_build": args.skip_build, "resume": args.resume},
               "plan": {**sizes, "selected_fixed": len(plan), "selected_variable": len(varplan)},
               "this_run": [], "claims_run": not args.only, "skipped": skipped}
    if args.skip_build:
        for k in ("build", "tests", "build_commit"):
            if k in previous:
                summary[k] = previous[k]
    else:
        summary.update(build_and_test(out))
        summary["build_commit"] = head
        print(f"build: {summary['build']}  tests: {summary['tests']}", flush=True)
        stopped = build_failure(summary)
        if stopped:
            summary["stopped"] = stopped
            summary_file.write_text(json.dumps(summary, ensure_ascii=False, indent=1), "utf-8")
            write_report(out, [], [], summary)
            print(f"STOPPED: {stopped}", flush=True)
            return 2
    summary_file.write_text(json.dumps(summary, ensure_ascii=False, indent=1), "utf-8")

    for i, r in enumerate(plan, 1):
        tag = r["baseline"][:-4]
        res_file = out / f"{tag}.json"
        if args.resume and res_file.exists():
            continue
        rec = replay_fixed(r, out, tag, head)
        res_file.write_text(json.dumps(rec, ensure_ascii=False, indent=1), "utf-8")
        summary["this_run"].append(tag)
        print(f"[{i:2d}/{len(plan)}] {rec['verdict']:8s} {tag}  ({rec['seconds']} s)", flush=True)

    for i, r in enumerate(varplan, 1):
        tag = r["baseline"][:-4]
        res_file = out / f"{tag}.json"
        if args.resume and res_file.exists():
            continue
        rec = replay_variable(r, out, tag, head)
        res_file.write_text(json.dumps(rec, ensure_ascii=False, indent=1), "utf-8")
        summary["this_run"].append(tag)
        print(f"[var {i:2d}/{len(varplan)}] {rec['verdict']:8s} {tag}  ({rec['seconds']} s)", flush=True)

    if not args.only:
        for c in CLAIMS:
            res_file = out / f"{c['name']}.json"
            if args.resume and res_file.exists():
                continue
            rec = {"claim": c["name"], "doc": c["doc"], "cmd": c["cmd"], "commit": head}
            now, why = replay(c["cmd"], rec)
            if why:
                rec["verdict"] = "INVALID"
                rec["invalid"] = why
            else:
                (out / "logs" / f"{c['name']}.now.log").write_text(now, "utf-8")
                missing, present = judge_claim(c, now)
                rec.update(verdict="HOLDS" if not missing and not present else "BROKEN",
                           missing=missing, present=present, red_now=red_counts(now))
            res_file.write_text(json.dumps(rec, ensure_ascii=False, indent=1), "utf-8")
            summary["this_run"].append(c["name"])
            print(f"[claim] {rec['verdict']:8s} {c['name']}", flush=True)

    summary_file.write_text(json.dumps(summary, ensure_ascii=False, indent=1), "utf-8")
    failed = write_report(out, plan, varplan, summary)
    for f in failed:
        print(f"FAIL: {f}", flush=True)
    return 1 if failed else 0


def rose_fixed(r):
    """The red counters either replay of a fixed-step command took above its baseline."""
    base = r.get("red_baseline", {})
    reads = [v for v in (r.get("red_now"), r.get("red_again")) if v is not None]
    return [k for k in RED if any(v.get(k, 0) > base.get(k, 0) for v in reads)]


def moved_fixed(r):
    """Every red counter a replay moved off its baseline, as `baseline→first/second` when the two
    replays disagree, so the worse of them is never hidden behind the other."""
    if r["verdict"] == "INVALID":
        return "nothing was read"
    base = r.get("red_baseline", {})
    out = []
    for k in RED:
        reads = [v[k] for v in (r.get("red_now"), r.get("red_again")) if v is not None and k in v]
        if any(x != base.get(k) for x in reads):
            shown = "/".join(str(x) for x in dict.fromkeys(reads))
            out.append(f"{k} {base.get(k)}→{shown}" + (" (rose)" if max(reads) > base.get(k, 0) else ""))
    return "; ".join(out) or "—"


def outside_variable(r):
    out = []
    for k in RED:
        reads = [c[k] for c in r.get("red_replays", [])]
        lo, hi = r["red_low"][k], r["red_high"][k]
        if any(x < lo or x > hi for x in reads):
            span = f"{lo}" if lo == hi else f"{lo}..{hi}"
            mark = (" (rose)" if k in r.get("rose", []) else " (fell)" if k in r.get("fell", [])
                    else " (the good logs disagree on it: not judged)")
            out.append(f"{k} {span}→{'/'.join(str(x) for x in reads)}" + mark)
    return "; ".join(out) or "—"


def taken(r, summary):
    name = r.get("claim") or r["baseline"][:-4]
    if name in summary.get("this_run", []):
        return "this run"
    c = r.get("commit")
    return f"kept, {c[:10]}" + ("" if c == summary.get("commit") else " (another commit)") if c else "kept, commit not recorded"


def is_red_diff(d):
    if "red" in d:
        return d["red"]
    return d["family"].startswith(DRIFT) or any(is_red_line(l or "") for l in (d["was"], d["now"]))


def failures(rows, varrows, claimrows):
    out = []
    for r in rows:
        if r["verdict"] in ("INVALID", "CHANGED?"):
            out.append(f"{r['verdict']} {r['baseline']}: {r.get('invalid') or r.get('again_invalid') or 'not read'}")
        rose = rose_fixed(r)
        if rose:
            out.append(f"red counter rose in {r['baseline']}: {', '.join(rose)}")
    for r in varrows:
        if r["verdict"] == "INVALID":
            out.append(f"INVALID {r['baseline']} (variable step): {r.get('invalid')}")
        if r.get("rose"):
            out.append(f"red counter rose in {r['baseline']} (variable step): {', '.join(r['rose'])}")
    for c in claimrows:
        if c["verdict"] in ("INVALID", "BROKEN"):
            out.append(f"{c['verdict']} {c['claim']}")
    return out


def write_report(out, plan, varplan, summary):
    """Write report.md from the results on disk for the selected plan, and return what fails the sweep."""
    def load(name):
        f = out / f"{name}.json"
        return json.loads(f.read_text("utf-8")) if f.exists() else None

    rows = [x for x in (load(r["baseline"][:-4]) for r in plan) if x]
    varrows = [x for x in (load(r["baseline"][:-4]) for r in varplan) if x]
    claimrows = [x for x in (load(c["name"]) for c in CLAIMS) if x] if summary.get("claims_run", True) else []
    inv = summary.get("invocation", {})
    flags = " ".join(f for f, on in (("--skip-build", inv.get("skip_build")), ("--resume", inv.get("resume"))) if on)
    flags = (flags + (f" --only {inv['only']!r}" if inv.get("only") else "")).strip()

    md = ["# Regression against the evidence folder", "",
          f"Commit: {summary.get('commit') or 'unknown'}. Invocation: {flags or 'full sweep'}.", ""]
    if summary.get("stopped"):
        md += [f"**Stopped before any replay: {summary['stopped']}.**", ""]
    if inv.get("skip_build"):
        if "build" in summary:
            bc = summary.get("build_commit")
            at = ("at a commit it does not record" if not bc else f"at commit {bc}"
                  + ("" if bc == summary.get("commit") else ", not this one"))
            md += [f"Build and tests not run this time (--skip-build). The last record on disk, taken {at}:"]
        else:
            md += ["Build and tests not run this time (--skip-build), and no earlier record is on disk."]
    if "build" in summary:
        md += [f"Build: {summary['build']}", f"Tests: {summary['tests']}"]
        if inv.get("skip_build") and build_failure(summary):
            md += [f"**That record is a failure, so these replays may describe other binaries: {build_failure(summary)}.**"]
    md += [""]
    if summary.get("stopped"):
        md += ["## Exit status", "", f"2: {summary['stopped']}."]
        (out / "report.md").write_text("\n".join(md), "utf-8")
        print(f"report: {out / 'report.md'}", flush=True)
        return []

    p = summary.get("plan", {})
    kept = [r for r in rows + varrows + claimrows if taken(r, summary) != "this run"]
    if inv.get("only"):
        md += [f"--only {inv['only']!r} selected {len(plan)} of {p.get('fixed', len(plan))} fixed-step and "
               f"{len(varplan)} of {p.get('variable', len(varplan))} variable-step commands; the rest were "
               f"not run this time and are not in this report.", ""]
    for label, these, of in (("Fixed step", rows, plan), ("Variable step", varrows, varplan)):
        counts = {}
        for r in these:
            counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
        md += [f"{label}: {len(these)} of {len(of)} have a result." + "".join(f" {k} {v}," for k, v in sorted(counts.items())).rstrip(",")]
    if kept:
        other = sum(1 for r in kept if r.get("commit") != summary.get("commit"))
        md += [f"{len(kept)} of the results in this report, claims included, were kept from disk and not replayed "
               f"this time" + (f"; {other} of them at another commit or with none recorded" if other else "")
               + " (see the Taken column)."]
    md += ["", "| Baseline | Verdict | Lines | Red counters that moved | Taken |", "|---|---|---|---|---|"]
    for r in rows:
        md.append(f"| `{r['baseline']}` | {r['verdict']} | {r.get('lines', '-')} | {moved_fixed(r)} | {taken(r, summary)} |")
    md += ["", "## What changed", "",
           "Every family that differs, the red-line families and the drift line first.", ""]
    for r in rows:
        if r["verdict"] == "SAME":
            continue
        md.append(f"### `{r['baseline']}` — {r['verdict']}")
        if r.get("invalid"):
            md.append(f"Could not be read: {r['invalid']}")
        if r.get("again_invalid"):
            md.append(f"Second replay could not be read: {r['again_invalid']}")
        if r.get("unreadable_launches"):
            md.append(f"Launches that could not be read and were run again: {'; '.join(r['unreadable_launches'])}")
        if r.get("varies_in"):
            md.append(f"Varies between two replays in: {', '.join(r['varies_in'])}")
        for d in sorted(r.get("diff", []), key=lambda d: (not is_red_diff(d), d["family"])):
            md.append(f"- **{d['family']}** ({d['baseline_n']} → {d['now_n']} lines)" + (" — red line" if is_red_diff(d) else ""))
            if d["was"]:
                md.append(f"  - was: `{d['was'][:220]}`")
            if d["now"]:
                md.append(f"  - now: `{d['now'][:220]}`")
        md.append("")
    md += ["## Variable step: red counters only", "",
           f"Each command replayed {VARIABLE_REPLAYS} times. A counter its good logs agree on is held to "
           "that count; one they disagree on is judged only on whether it trips at all. The lines are "
           "not compared, because they do not repeat.", "",
           "| Newest baseline | Verdict | Baselines | Red counters outside the baselines' range | Taken |",
           "|---|---|---|---|---|"]
    for r in varrows:
        seen = outside_variable(r)
        if r.get("invalid"):
            seen = ("" if seen == "—" else seen + "; ") + f"not read: {r['invalid']}"
        md.append(f"| `{r['baseline']}` | {r['verdict']} | {len(r['baselines'])} | {seen} | {taken(r, summary)} |")
    md += ["", "## Documented controls with no headless evidence", ""]
    if not summary.get("claims_run", True):
        md += ["Not run this time (--only); results on disk from an earlier invocation are not shown."]
    else:
        md += ["| Control | Verdict | What the guide says | Missing / present that must not be | Taken |", "|---|---|---|---|---|"]
        for r in claimrows:
            bad = "; ".join([f"missing `{p}`" for p in r.get("missing", [])] +
                            [f"present `{p}`" for p in r.get("present", [])] +
                            ([f"not read: {r['invalid']}"] if r.get("invalid") else [])) or "—"
            md.append(f"| `{r['claim']}` | {r['verdict']} | {r['doc']} | {bad} | {taken(r, summary)} |")
    md += ["", "## Not replayed", ""] + [f"- `{n}` — {w}" for n, w in summary.get("skipped", [])]
    failed = failures(rows, varrows, claimrows)
    md += ["", "## Exit status", ""] + ([f"1, because of {len(failed)}:"] + [f"- {f}" for f in failed] if failed else
                                     ["0: no replay or claim INVALID, none CHANGED?, no red counter rose, no claim BROKEN."])
    (out / "report.md").write_text("\n".join(md), "utf-8")
    print(f"report: {out / 'report.md'}", flush=True)
    return failed


if __name__ == "__main__":
    sys.exit(main())
