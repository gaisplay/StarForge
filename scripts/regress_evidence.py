# -*- coding: utf-8 -*-
"""Replay every headless run kept in the evidence folder and compare what the rigs print now.

Every flight check in this project ends as a log in docs/superpowers/notes/evidence/, and almost
every log carries the command line that produced it (four 2026-09-08 extracts of LogStarForge
lines do not, and are not replayed). So a regression needs no list of its own: take each headless
log, run its command line again verbatim, and compare the project's lines. The engine may log
`-game` last whatever position it was given, so command lines are compared argument by argument.

    python scripts/regress_evidence.py                 # rebuild, test, replay everything
    python scripts/regress_evidence.py --skip-build    # replay only
    python scripts/regress_evidence.py --only combat   # replays whose baseline name contains it
    python scripts/regress_evidence.py --resume        # keep results already on disk

Verdicts, per replayed command:

    SAME          every LogStarForge line, timestamp and frame number stripped, equals the baseline
    CHANGED       differs, and a second replay equals the first: the build prints something else now
    VARIES        differs, and the second replay differs from the first: the run does not repeat
                  itself (two hulls colliding resolve on a contact manifold the step decides; see
                  CLAUDE.md), so its figures are protocol and only its red counters are judged
    CHANGED?      differs, and the second replay could not be read or died before its world
                  began, so change and variance cannot be told apart
    INVALID       no log carries this run's command line (it went elsewhere, or the run never
                  started), or the one that does died before its world began (live_log, below).
                  A run the engine wrote to StarForge_2.log beside an open editor is accepted

CHANGED is not a verdict of failure. The tree moves, and a change after a known commit is expected;
the report lists what changed so a person can say which. What a person must never have to do is
find the difference by eye.

Results go to --out (default: Saved/Regress under the project root, which .gitignore keeps out of
the repository), one JSON per replay so that --resume survives an interrupted sweep, plus
report.md.
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

# Command lines that cannot be replayed: they name a console variable that no longer exists.
UNREPLAYABLE = {
    "sf.SpikeOriginOffsetM": "the world-size spike's throwaway variable, rolled back on 2026-09-17",
    "sf.SpikeDockGapCm": "F3's throwaway docked-gap variable, measured in a scratch worktree on 2026-09-25",
}

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

STAMP = re.compile(r"^\[[^\]]*\]\[[^\]]*\]")

# Controls CLAUDE.md documents that no headless log in evidence carried when this list was written
# (2026-09-23; the two undock-thrust-x claims have since been kept in evidence as
# 2026-09-23-review-undock-sideways-*.log and are replayed as baselines too). Each has no baseline, so
# it is judged by the lines the guide says it must and must not produce -- the guide's own words,
# which is what makes a drift between guide and code show up here rather than at a keyboard.
_SPIKE = "-game -nullrhi -unattended -nosplash -benchmark"
_LOOP = "/Game/Maps/L_Loop " + _SPIKE
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
     "doc": "yaw 120.0 deg/s with nothing on the other axes",
     "cmd": f'{_SPIKE} -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.Yaw 1" -log',
     "must": [r"ang roll/pitch/yaw \(-?0\.0 -?0\.0 -?120\.0\)"], "must_not": [r"LogStarForge: Error"]},
    {"name": "claim-yaw-400",
     "doc": "same, at 400 fps",
     "cmd": f'{_SPIKE} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.Yaw 1" -log',
     "must": [r"ang roll/pitch/yaw \(-?0\.0 -?0\.0 -?120\.0\)"], "must_not": [r"LogStarForge: Error"]},
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


def project_lines(log_text):
    """Every LogStarForge line with its timestamp and frame number stripped, in order."""
    return [STAMP.sub("", l) for l in log_text.splitlines() if "LogStarForge" in l]


def family(line):
    """The first words after the verbosity: 'Flight:', 'Dock:', 'Place size:', ..."""
    body = re.sub(r"^LogStarForge: (Display: |Warning: |Error: |Verbose: )?", "", line)
    m = re.match(r"([A-Za-z][A-Za-z ]{0,24}?[:\[])", body)
    return m.group(1).rstrip("[").strip() if m else body[:16]


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
    """One replay per distinct headless command line, with its most recently opened good log as
    baseline. Logs with no command line at all (the 2026-09-08 extracts of LogStarForge lines) and
    logs that are not headless benchmarks are left out: there is nothing to replay."""
    by_cmd = {}
    skipped = []
    for p in sorted(EVIDENCE.glob("*.log")):
        text = p.read_text("utf-8", errors="replace")
        cmd = command_line(text[:400000])
        if not cmd or "-nullrhi" not in cmd or "-benchmark" not in cmd or "RunTests" in cmd:
            continue
        why = next((w for k, w in UNREPLAYABLE.items() if k in cmd), None)
        if why:
            skipped.append((p.name, why))
            continue
        by_cmd.setdefault(cmd, []).append((opened_at(text, p.name), p.name))
    plan = []
    for cmd, entries in by_cmd.items():
        good = sorted(e for e in entries if not any(t in e[1] for t in NOT_A_BASELINE))
        if not good:
            skipped.append((entries[-1][1], "only broken or pre-fix logs carry this command"))
            continue
        base = good[-1][1]
        plan.append({"baseline": base, "cmd": cmd, "also": [n for _, n in entries if n != base]})
    plan.sort(key=lambda r: r["baseline"])
    return plan, skipped


def run_engine(cmd):
    """Run one headless command verbatim and return the text of the log it wrote, or None."""
    for stale in ("StarForge.log", "StarForge_2.log"):
        f = LOGS / stale
        if f.exists():
            try:
                f.unlink()
            except OSError:
                pass  # held by a running editor; the command-line check below will catch it
    full = f'"{EDITOR}" "{UPROJECT}" {cmd}'
    subprocess.run(full, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    for name in ("StarForge.log", "StarForge_2.log"):
        f = LOGS / name
        if f.exists():
            text = f.read_text("utf-8", errors="replace")
            if same_command(command_line(text[:400000]), cmd):
                return live_log(text)
    return None


def first_difference(a, b):
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
                    "was": pair[0] if pair else (x[len(y)] if len(x) > len(y) else None),
                    "now": pair[1] if pair else (y[len(x)] if len(y) > len(x) else None)})
    return out


def build_and_test(out):
    res = {}
    r = subprocess.run(f'"{REBUILD}" StarForgeEditor Win64 Development -Project="{UPROJECT}" -WaitMutex',
                       cwd=ROOT, capture_output=True, text=True, errors="replace")
    (out / "rebuild.log").write_text(r.stdout + r.stderr, "utf-8")
    log = r.stdout + r.stderr
    res["build"] = {
        "succeeded": "Result: Succeeded" in log,
        "warnings": len(re.findall(r"warning", log, re.I)),
        "errors": len(re.findall(r"\berror\b", log, re.I)),
        "actions": len(re.findall(r"^\[\d+/\d+\]", log, re.M)),
    }
    subprocess.run(f'"{EDITOR_CMD}" "{UPROJECT}" -ExecCmds="Automation RunTests StarForge.Core;Quit" '
                   f'-unattended -nopause -nullrhi -nosplash -log', cwd=ROOT, stdin=subprocess.DEVNULL,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t = (LOGS / "StarForge.log").read_text("utf-8", errors="replace")
    shutil.copy(LOGS / "StarForge.log", out / "tests.log")
    fails = re.findall(r"Result=\{Fail\} Name=\{([^}]+)\}", t)
    res["tests"] = {"success": t.count("Result={Success}"), "fail": len(fails), "failed": fails,
                    "exit": (re.search(r"EXIT CODE: (\d+)", t) or [None, "?"])[1]}
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None)
    ap.add_argument("--only", default=None)
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--resume", action="store_true")
    args = ap.parse_args()

    out = Path(args.out) if args.out else ROOT / "Saved" / "Regress"
    (out / "logs").mkdir(parents=True, exist_ok=True)
    plan, skipped = collect()
    if args.only:
        plan = [r for r in plan if args.only in r["baseline"]]

    summary = {"skipped": skipped}
    if not args.skip_build:
        summary.update(build_and_test(out))
        print(f"build: {summary['build']}  tests: {summary['tests']['success']}/{summary['tests']['fail']}",
              flush=True)
    (out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=1), "utf-8")

    for i, r in enumerate(plan, 1):
        tag = r["baseline"][:-4]
        res_file = out / f"{tag}.json"
        if args.resume and res_file.exists():
            continue
        base_text = (EVIDENCE / r["baseline"]).read_text("utf-8", errors="replace")
        t0 = time.time()
        now = run_engine(r["cmd"])
        rec = {"baseline": r["baseline"], "cmd": r["cmd"], "seconds": round(time.time() - t0, 1),
               "red_baseline": red_counts(base_text)}
        if now is None:
            rec["verdict"] = "INVALID"
        else:
            (out / "logs" / f"{tag}.now.log").write_text(now, "utf-8")
            rec["red_now"] = red_counts(now)
            a, b = project_lines(base_text), project_lines(now)
            rec["lines"] = len(b)
            if a == b:
                rec["verdict"] = "SAME"
            else:
                rec["diff"] = first_difference(a, b)
                again = run_engine(r["cmd"])
                if again is None:
                    rec["verdict"] = "CHANGED?"  # could not repeat to tell change from variance
                elif project_lines(again) == b:
                    rec["verdict"] = "CHANGED"
                else:
                    rec["verdict"] = "VARIES"
                    rec["varies_in"] = [d["family"] for d in first_difference(b, project_lines(again))]
                    (out / "logs" / f"{tag}.again.log").write_text(again, "utf-8")
        res_file.write_text(json.dumps(rec, ensure_ascii=False, indent=1), "utf-8")
        print(f"[{i:2d}/{len(plan)}] {rec['verdict']:8s} {tag}  ({rec['seconds']} s)", flush=True)

    if not args.only:
        for c in CLAIMS:
            res_file = out / f"{c['name']}.json"
            if args.resume and res_file.exists():
                continue
            now = run_engine(c["cmd"])
            rec = {"claim": c["name"], "doc": c["doc"], "cmd": c["cmd"]}
            if now is None:
                rec["verdict"] = "INVALID"
            else:
                (out / "logs" / f"{c['name']}.now.log").write_text(now, "utf-8")
                missing = [p for p in c["must"] if not re.search(p, now)]
                present = [p for p in c["must_not"] if re.search(p, now)]
                rec.update(verdict="HOLDS" if not missing and not present else "BROKEN",
                           missing=missing, present=present, red_now=red_counts(now))
            res_file.write_text(json.dumps(rec, ensure_ascii=False, indent=1), "utf-8")
            print(f"[claim] {rec['verdict']:8s} {c['name']}", flush=True)

    write_report(out, plan, summary)


def write_report(out, plan, summary):
    rows = []
    for r in plan:
        f = out / f"{r['baseline'][:-4]}.json"
        if f.exists():
            rows.append(json.loads(f.read_text("utf-8")))
    counts = {}
    for r in rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    md = ["# Regression against the evidence folder", ""]
    if "build" in summary:
        md += [f"Build: {summary['build']}", f"Tests: {summary['tests']}", ""]
    md += [f"Replayed: {len(rows)} of {len(plan)}. " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())), ""]
    md += ["| Baseline | Verdict | Lines | Red counters that moved |", "|---|---|---|---|"]
    for r in rows:
        moved = [f"{k} {r['red_baseline'][k]}→{r.get('red_now', {}).get(k, '?')}"
                 for k in RED if r["red_baseline"].get(k) != r.get("red_now", {}).get(k)]
        md.append(f"| `{r['baseline']}` | {r['verdict']} | {r.get('lines', '-')} | {'; '.join(moved) or '—'} |")
    md += ["", "## What changed", ""]
    for r in rows:
        if r["verdict"] in ("SAME",):
            continue
        md.append(f"### `{r['baseline']}` — {r['verdict']}")
        if r.get("varies_in"):
            md.append(f"Varies between two replays in: {', '.join(r['varies_in'])}")
        for d in r.get("diff", [])[:8]:
            md.append(f"- **{d['family']}** ({d['baseline_n']} → {d['now_n']} lines)")
            if d["was"]:
                md.append(f"  - was: `{d['was'][:220]}`")
            if d["now"]:
                md.append(f"  - now: `{d['now'][:220]}`")
        md.append("")
    md += ["## Documented controls with no headless evidence", "",
           "| Control | Verdict | What the guide says | Missing / present that must not be |", "|---|---|---|---|"]
    for c in CLAIMS:
        f = out / f"{c['name']}.json"
        if f.exists():
            r = json.loads(f.read_text("utf-8"))
            bad = "; ".join([f"missing `{p}`" for p in r.get("missing", [])] +
                            [f"present `{p}`" for p in r.get("present", [])]) or "—"
            md.append(f"| `{c['name']}` | {r['verdict']} | {c['doc']} | {bad} |")
    md += ["", "## Not replayed", ""] + [f"- `{n}` — {w}" for n, w in summary.get("skipped", [])]
    (out / "report.md").write_text("\n".join(md), "utf-8")
    print(f"report: {out / 'report.md'}", flush=True)


def live_log(text):
    """The log, or None when the run it records died before its world began.

    Two replays of the station-kit base did (2026-09-28): each log ended at frame 0 on
    `Engine exit requested (reason: ConsoleCtrl RequestExit)`, with no world brought up and not one
    LogStarForge line. Each carried its command line, so each was taken for the run: one read
    CHANGED with 0 lines, and the other VARIES, because its second run was alive -- and a VARIES
    replay is judged by its red counters, which on an empty log are all zero. Every game world has
    written a World bounds: line since 2026-09-26, so a live run has project lines; and a console
    control event is not how a -benchmark -seconds run ends, at any frame.
    """
    why = [w for w, dead in (
        ("no LogStarForge line", not project_lines(text)),
        ("no world brought up for play", "Bringing World" not in text),
        ("stopped by a console control event", "Engine exit requested (reason: ConsoleCtrl" in text),
    ) if dead]
    if why:
        print(f"  dead run: {'; '.join(why)}", flush=True)
        return None
    return text


if __name__ == "__main__":
    sys.exit(main())
