# -*- coding: utf-8 -*-
"""Compare two passes of regress_evidence.py command by command (station-kit spec 6.7).

    python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-1.2
    python scripts/compare_regress.py --groups Saved/Regress/kit-0-before

regress_evidence.py judges every replay against its evidence log, and almost every evidence log
predates the World bounds: line that every game world has written since 2026-09-26. So nearly every
replay reads CHANGED in both passes, and a second difference brought in by the station kit would
not move a single verdict. This compares the two passes' own logs instead:

  - a replay that VARIES in neither pass: the project lines of the two .now logs must be equal;
  - a replay that VARIES in either pass: its red counters must be equal between the passes, except
    a counter that differs between .now and .again inside one pass, which is protocol;
  - INVALID in either pass: the replay did not happen, and that replay is taken again;
  - a command run at a variable step, which regress_evidence.py replays twice and judges on its red
    counters against every good log of it (since the 2026-10-01 review): STEADY in both passes. Its
    lines do not repeat from one run to the next, so they are not compared;
  - every claim reads HOLDS in both passes;
  - a command present only in the later pass (its log was laid in evidence after the base) is judged
    by its own verdict: SAME, or VARIES with the red counters of its baseline unchanged, or STEADY at
    a variable step;
  - a command present only in the base is a finding: a baseline has left the evidence folder.

Exit code 0 when every rule holds and 1 when any does not. The report goes to stdout and to
<later pass>/compare.md. A difference is a finding to be written down, never explained after the fact.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import regress_evidence as R  # noqa: E402

# Spec 6.7: the five groups of commands that must be in every pass, because they go through what the
# station kit touches -- the rig station's room box at start, its tunnel box, the docking capture and
# the release, the port holder and its notification.
_SPIKE = "-nullrhi -unattended -nosplash -benchmark"
_LOOPURL = "/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFLoopGameMode -game " + _SPIKE
GROUPS = [
    f'{_SPIKE} -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log -game',
    f'{_SPIKE} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log -game',
    f'/Game/Maps/L_Loop -game {_SPIKE} -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log',
    f'/Game/Maps/L_Loop -game {_SPIKE} -fps=400 -seconds=70 -ExecCmds="sf.FlightLog 1" -log',
    f'/Game/Maps/L_Loop -game {_SPIKE} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Knockdown 0, sf.Dock 0, sf.Intent.Roll 1" -log',
    f'/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFCombatGameMode -game {_SPIKE} -fps=60 -seconds=40 -ExecCmds="sf.FlightLog 1" -log',
    f'/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFCombatGameMode -game {_SPIKE} -fps=400 -seconds=40 -ExecCmds="sf.FlightLog 1" -log',
    f'/Game/Maps/L_Place -game {_SPIKE} -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log',
    f'/Game/Maps/L_Place -game {_SPIKE} -fps=400 -seconds=70 -ExecCmds="sf.FlightLog 1" -log',
    f'{_LOOPURL} -fps=60 -seconds=40 -ExecCmds="sf.FlightLog 1, sf.Dock 2" -log',
    f'{_LOOPURL} -fps=400 -seconds=40 -ExecCmds="sf.FlightLog 1, sf.Dock 2" -log',
    f'{_LOOPURL} -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1" -log',
    f'{_LOOPURL} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1" -log',
    f'/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFPlaceGameMode -game {_SPIKE} -fps=60 -seconds=40 '
    f'-ini:Engine:[ConsoleVariables]:sf.PlaceOffsetM=43000000000 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1" -log',
    f'/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFPlaceGameMode -game {_SPIKE} -fps=400 -seconds=40 '
    f'-ini:Engine:[ConsoleVariables]:sf.PlaceOffsetM=43000000000 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1" -log',
    f'{_LOOPURL} -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Knockdown 0, sf.Dock 0, sf.Intent.Roll 1" -log',
    f'{_LOOPURL} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Knockdown 0, sf.Dock 0, sf.Intent.Roll 1" -log',
    f'{_LOOPURL} -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.Y 0.004" -log',
    f'{_LOOPURL} -fps=400 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Dock 0, sf.Intent.Y 0.004" -log',
]


def load(pass_dir):
    replays, claims = {}, {}
    for f in sorted(Path(pass_dir).glob("*.json")):
        if f.name == "summary.json":
            continue
        rec = json.loads(f.read_text("utf-8"))
        if "claim" in rec:
            claims[rec["claim"]] = rec
        elif "cmd" in rec:
            replays[rec["cmd"]] = rec
    summary_file = Path(pass_dir) / "summary.json"
    summary = json.loads(summary_file.read_text("utf-8")) if summary_file.exists() else {}
    return replays, claims, summary


def log_text(pass_dir, rec, which):
    f = Path(pass_dir) / "logs" / f"{rec['baseline'][:-4]}.{which}.log"
    return f.read_text("utf-8", errors="replace") if f.exists() else None


def protocol(pass_dir, rec):
    """Counters that differ between .now and .again of one command on one build: protocol."""
    now, again = log_text(pass_dir, rec, "now"), log_text(pass_dir, rec, "again")
    if now is None or again is None:
        return set()
    a, b = R.red_counts(now), R.red_counts(again)
    return {k for k in R.RED if a[k] != b[k]}


def check_groups(pass_dir):
    replays, _, _ = load(pass_dir)
    missing = [g for g in GROUPS if not any(R.same_command(c, g) for c in replays)]
    print(f"groups: {len(GROUPS) - len(missing)} of {len(GROUPS)} present")
    for g in missing:
        print(f"  MISSING: {g}")
    return 0 if not missing else 1


def compare(base_dir, run_dir):
    base, base_claims, base_sum = load(base_dir)
    run, run_claims, run_sum = load(run_dir)
    findings, notes = [], []

    for label, summary in (("base", base_sum), ("run", run_sum)):
        build, tests = summary.get("build", {}), summary.get("tests", {})
        notes.append(f"{label}: build {build}; tests {tests.get('success')} passed, {tests.get('fail')} failed")
        if build and not build.get("succeeded"):
            findings.append(f"{label}: the clean rebuild did not succeed")
        if tests and tests.get("fail"):
            findings.append(f"{label}: headless tests failed: {tests.get('failed')}")

    for cmd in sorted(set(base) | set(run)):
        b, r = base.get(cmd), run.get(cmd)
        if r is None:
            findings.append(f"MISSING FROM RUN: {b['baseline']} -- a baseline left the evidence folder")
            continue
        tag = r["baseline"]
        if "variable" in (r.get("step"), (b or {}).get("step")):
            if b is None and r["verdict"] == "STEADY":
                notes.append(f"new in run, STEADY at a variable step: {tag}")
            elif b is None or b["verdict"] != "STEADY" or r["verdict"] != "STEADY":
                findings.append(f"VARIABLE {tag}: base {b['verdict'] if b else 'absent'}, run {r['verdict']}; "
                                f"rose {r.get('rose')}, fell {r.get('fell')}")
            continue
        if b is None:
            if r["verdict"] == "SAME":
                notes.append(f"new in run, SAME: {tag}")
            elif r["verdict"] == "VARIES":
                skip = protocol(run_dir, r)
                moved = [k for k in R.RED if k not in skip
                         and r["red_baseline"].get(k) != r.get("red_now", {}).get(k)]
                if moved:
                    findings.append(f"NEW {tag}: VARIES and counters moved against its baseline: {moved}")
                else:
                    notes.append(f"new in run, VARIES with counters unchanged: {tag}")
            else:
                findings.append(f"NEW {tag}: verdict {r['verdict']} against its own baseline")
            continue
        if "INVALID" in (b["verdict"], r["verdict"]):
            findings.append(f"INVALID {tag}: base {b['verdict']}, run {r['verdict']} -- take this replay again")
            continue
        if "VARIES" in (b["verdict"], r["verdict"]):
            skip = protocol(base_dir, b) | protocol(run_dir, r)
            bn, rn = log_text(base_dir, b, "now"), log_text(run_dir, r, "now")
            cb, cr = R.red_counts(bn or ""), R.red_counts(rn or "")
            moved = [f"{k} {cb[k]}->{cr[k]}" for k in R.RED if k not in skip and cb[k] != cr[k]]
            if moved:
                findings.append(f"COUNTERS {tag}: {moved} (protocol: {sorted(skip)})")
            continue
        bn, rn = log_text(base_dir, b, "now"), log_text(run_dir, r, "now")
        if bn is None or rn is None:
            findings.append(f"NO LOG {tag}: a .now log is missing")
            continue
        a, c = R.project_lines(bn), R.project_lines(rn)
        if a != c:
            diff = R.first_difference(a, c)
            first = diff[0] if diff else {"family": "?", "was": None, "now": None}
            findings.append(f"LINES {tag}: {len(diff)} families differ; first {first['family']}: "
                            f"was {first['was']!r} now {first['now']!r}")

    for label, claims in (("base", base_claims), ("run", run_claims)):
        for name, rec in sorted(claims.items()):
            if rec["verdict"] != "HOLDS":
                findings.append(f"CLAIM {label} {name}: {rec['verdict']} "
                                f"missing {rec.get('missing')} present {rec.get('present')}")

    lines = ["# compare_regress", "", f"base: {base_dir}", f"run: {run_dir}", ""]
    lines += [f"- {n}" for n in notes] + [""] + [f"- FINDING: {f}" for f in findings]
    lines += ["", f"compare: {len(findings)} findings"]
    text = "\n".join(lines)
    print(text)
    (Path(run_dir) / "compare.md").write_text(text, "utf-8")
    return 0 if not findings else 1


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--groups":
        return check_groups(sys.argv[2])
    if len(sys.argv) == 3:
        return compare(sys.argv[1], sys.argv[2])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
