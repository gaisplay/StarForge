# -*- coding: utf-8 -*-
"""Re-derive every figure written into checkpoint 7's form and compare with the form's text.

Prints one line per claim. A claim's needle -- the text the form has to contain -- is built from
the figure the log gives, in the form's own words, so a log that gives another figure looks for
other text and the claim fails. Where the form says a figure in words a number cannot be turned
into (`**Ни одной**`, `во всех одиннадцати`), those words are looked for only when the log gives
the figure they stand for. A claim no log can make is printed as a transcription, `[ПЕР]`, and
counted apart: all the script does with it is find its text in the form. Exits 1 when any claim
fails.
"""
import re, collections, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EV = ROOT / "docs" / "superpowers" / "notes" / "evidence"
FORM_TEXT = (ROOT / "docs" / "superpowers" / "notes" / "2026-09-14-checkpoint-7-results.md").read_text(
    encoding="utf-8")

ok = bad = copied = 0


def check(label, derived, needle):
    """needle: the form's words for what the log gives, built from `derived`; None when the log gives
    something those words cannot say, which fails."""
    global ok, bad
    hit = needle is not None and needle in FORM_TEXT
    why = ("" if hit else f"  <-- в бланке нет {needle!r}" if needle is not None
           else "  <-- словами бланка этого не сказать")
    print(f"  [{'OK ' if hit else 'НЕТ'}] {label}: журнал даёт {derived!r}{why}")
    ok, bad = ok + int(hit), bad + int(not hit)


def transcribed(label, needle):
    """A claim about the session that no log records: only its text is looked for in the form."""
    global bad, copied
    hit = needle in FORM_TEXT
    print(f"  [ПЕР] {label}: переписано из бланка, журнал этого не говорит; в бланке "
          + ("есть" if hit else f"НЕТ {needle!r}"))
    copied, bad = copied + int(hit), bad + int(not hit)


def message(line):
    """A log line's text after its category and verbosity, as the form quotes it."""
    return re.sub(r"^LogStarForge: (?:Display: |Warning: |Error: )?", "", line).rstrip()


def frames(path):
    base, prev = 0, -1
    for ln in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r'^\[[^\]]*\]\[\s*(\d+)\](.*)$', ln)
        if not m:
            continue
        f = int(m.group(1))
        if prev >= 0 and f < prev - 500:
            base += 1000
        prev = f
        yield base + f, m.group(2)


L = {n: list(frames(EV / f"2026-09-17-checkpoint-7-{n}.log")) for n in (
    "A1-duel-standing", "A1b-duel-at-the-wall", "A2-fight-seated", "B1-shields-off", "B2-pirateai-off",
    "B3-no-pirate", "B4-floor-and-flank", "B5-instrument-60", "B5-instrument-400", "B5-nosweep-60",
    "B5-nosweep-400")}
B5 = [n for n in L if n.startswith("B5-")]

print("### действительность")
closed = sum(1 for v in L.values() if any("Log file closed" in t for _, t in v))
check("журналов с 'Log file closed'", closed, "| **Да**, во всех одиннадцати |" if closed == len(L) == 11 else None)
errs = {n: sum(1 for _, t in v if "LogStarForge: Error" in t) for n, v in L.items()}
nonzero = {n: c for n, c in errs.items() if c}
check("Error по журналам", errs,
      f"**0 во всех, кроме A2: там {nonzero['A2-fight-seated']}.**" if list(nonzero) == ["A2-fight-seated"] else None)
sk = sorted({re.search(r'skipped carries (\d+)', t).group(1) for v in L.values() for _, t in v if "skipped carries" in t})
in_every_log = all(any("skipped carries" in t for _, t in v) for v in L.values())
check("различные значения skipped carries", sk,
      f"**{sk[0]}** во всех строках всех одиннадцати журналов" if len(sk) == 1 and in_every_log and len(L) == 11
      else None)
nr = sum(1 for v in L.values() for _, t in v if "Carry NOT RUNNING" in t)
check("Carry NOT RUNNING всего", nr, "| `Carry NOT RUNNING` | ни одной | **Ни одной** |" if nr == 0 else None)
hull = sum(1 for v in L.values() for _, t in v
           if "Frame handover:" in t and "REFUSED" not in t
           and ("Ship -> Pirate" in t or "Pirate -> Ship" in t))
check("передач между корпусами без REFUSED", hull, "**Ни одной** между корпусами" if hull == 0 else None)
combat = [(n, "REFUSED" if "REFUSED" in t else "bound") for n, v in L.items() if n not in B5
          for _, t in v if "Frame handover" in t]
check("строки передачи на боевом стенде", combat,
      "Единственная строка передачи на боевом стенде — `REFUSED` в A1б"
      if combat == [("A1b-duel-at-the-wall", "REFUSED")] else None)
loop = {n: [m for _, t in L[n] for m in [re.search(
    r'(Frame handover: Station -> Ship, first carry moved him ([\d.]+) cm over (\d+) frame\(s\))', t)] if m]
    for n in B5}
one_each = len(B5) == 4 and all(len(x) == 1 for x in loop.values())
at60 = {x[0].group(1) for n, x in loop.items() if one_each and n.endswith("-60")}
at400 = {x[0].group(2, 3) for n, x in loop.items() if one_each and n.endswith("-400")}
same_cm = {re.search(r'him ([\d.]+) cm', p).group(1) for p in at60} == {cm for cm, _ in at400}
check("законных передач Station -> Ship на петле (B5)", {n: len(x) for n, x in loop.items()},
      f"В четырёх прогонах B5 есть по одной `{min(at60)}` (60 fps) и `over {min(at400)[1]} frame(s)` (400 fps)"
      if one_each and len(at60) == 1 and len(at400) == 1 and same_cm else None)

print("### A1")
v = L["A1-duel-standing"]
sh = {w: next(fr % 1000 for fr, t in v if f"Shield face +X is down on [{w}]." in t) for w in ("Ship", "Pirate")}
check("кадры посадки щита", sh, f"Корабль кадр **{sh['Ship']}**, пират кадр **{sh['Pirate']}**")
pdi = {w: next(i for i, (fr, t) in enumerate(v) if f"[{w}] is powered down" in t) for w in ("Ship", "Pirate")}
pdf = {w: v[i][0] % 1000 for w, i in pdi.items()}
same_frame = v[pdi["Ship"]][0] == v[pdi["Pirate"]][0]
check("кадр(ы) обесточивания", pdf, f"**Оба в одном кадре {pdf['Ship']}" if same_frame else None)
shots = {w: sum(1 for i, (fr, t) in enumerate(v) if f"Weapon: [{w}] shot" in t and i < pdi[w]) for w in ("Ship", "Pirate")}
check("выстрелы обоих до своего обесточивания", shots, f"| **{shots['Ship']} / {shots['Pirate']}** |")
lost = [message(t) for fr, t in v if "Footing LOST" in t]
check("строка Footing LOST дословно", lost, f"`{lost[0]}`" if len(lost) == 1 else None)
regained = sum(1 for fr, t in v if "Footing regained" in t)
check("строк Footing regained", regained, "**Строки нет вовсе:**" if regained == 0 else None)
drift = message([t for fr, t in v if "Shipboard drift" in t][-1])
down = re.search(r'([\d.]+) s settled.*knocked down ([\d.]+) s', drift)
check("сбит с ног, секунд", down.groups(), f"сбит **{down.group(2)} с** из {float(down.group(1)):g}")
check("строка увода", drift, f"`{drift}`")

print("### A1б")
v = L["A1b-duel-at-the-wall"]
ref = [message(t) for fr, t in v if "REFUSED" in t]
check("строка REFUSED дословно", ref, f"`{ref[0]}`" if len(ref) == 1 else None)
transcribed("номер прогона, в котором строка появилась", "**1** — с первой попытки")
pos = re.search(r'ship-local \(([^)]*)\)', [t for fr, t in v if "Footing LOST" in t][0]).group(1)
check("место первого Footing LOST", pos, f"`({pos})` — вплотную")
fl = [fr for fr, t in v if "Flight: [Ship]" in t]
med = sorted(fl[i+1]-fl[i] for i in range(len(fl)-1))[len(fl)//2]
check("кадров между секундными строками (медиана)", med, f"**{med}** — `t.MaxFPS 60` подействовал")

print("### A2")
v = L["A2-fight-seated"]


def faces(v, who):
    """Hits per face, most first and then by name, the order the form lists them in."""
    c = collections.Counter(m.group(1) for fr, t in v for m in [re.search(rf'Hit on \[{who}\] ([+-][XYZ]) face', t)] if m)
    return sorted(c.items(), key=lambda kv: (-kv[1], kv[0]))


fp = faces(v, "Pirate")
check("грани по пирату", dict(fp), ", ".join(f"`{f}` — **{n}**" if i == 0 else f"`{f}` — {n}" for i, (f, n) in enumerate(fp)))
fs = faces(v, "Ship")
check("грани по кораблю", dict(fs), ", ".join(f"`{f}` {n}" for f, n in fs))
n = sum(1 for fr, t in v if "destroyed at" in t)
check("строк уничтожения", n, f"**{n} строк.**")
n = sum(1 for fr, t in v if "Footing LOST" in t)
check("Footing LOST в A2", n, f"`Footing LOST` — {n}")
dist = [float(m.group(1)) for fr, t in v for m in [re.search(r'target Ship at ([\d.]+) m', t)] if m]
check("минимум расстояния", f"{min(dist):.1f}", f"**{min(dist):.1f} м**")
check("максимум расстояния", f"{max(dist):.1f}", f"**{max(dist):.1f} м**")

print("### B1 (C1)")
v = L["B1-shields-off"]
n = sum(1 for fr, t in v if "shields OFF" in t)
check("строк shields OFF", n, f"({n} таких строк")
pd = next(fr for fr, t in v if "[Ship] is powered down" in t)
n = sum(1 for fr, t in v if "Hit on [Ship]" in t and fr <= pd)
check("попаданий по игроку до обесточивания", n, f"на **{n}-м** попадании")

print("### B2 (C2)")
v = L["B2-pirateai-off"]
n = sum(1 for fr, t in v if "Hit on [Ship]" in t)
check("попаданий по игроку", n, f"**{n} попаданий**")
d = sorted({m.group(1) for fr, t in v for m in [re.search(r'target Ship at ([\d.]+) m', t)] if m})
check("различные расстояния", d, f"одно значение `{d[0]} m`" if len(d) == 1 else None)

print("### B3 (C3)")
v = L["B3-no-pirate"]
sh3 = [t for fr, t in v if "Weapon: [Ship] shot" in t]
nt = sum(1 for x in sh3 if "NO TARGET" in x)
check("выстрелов / из них NO TARGET", (len(sh3), nt), f"{len(sh3)} выстрел, **все {nt}**")
n = sum(1 for fr, t in v if "expired after" in t)
check("строк expired", n, f"строк `expired` {n}")
n = sum(1 for fr, t in v if "Hit on " in t)
check("попаданий", n, f"с `NO TARGET`, попаданий {n}")

print("### B4 (C4, C5)")
v = L["B4-floor-and-flank"]
pdp = next(fr for fr, t in v if "[Pirate] is powered down" in t)
n = sum(1 for fr, t in v if "Hit on [Pirate]" in t and fr <= pdp)
check("попаданий по пирату до обесточивания", n, f"{n} попаданий до обесточивания")
n = sum(1 for fr, t in v if "destroyed at" in t and fr <= pdp)
check("уничтожений до обесточивания", n, f"3) **{n}**")
dl = [t for fr, t in v if "on [Pirate] destroyed at" in t]
check("строк уничтожения C5", len(dl), f"**Ровно {len(dl)} строк")
d = re.search(r'by a hit ([\d.]+) m away', dl[0]).group(1)
check("дистанция первого", d, f"by a hit {d} m away")
corners = sorted({re.search(r'destroyed at \(([^)]*)\) m', t).group(1) for t in dl})
check("угол всех строк", corners, f"все с одним `at ({corners[0]}) m`**" if len(corners) == 1 else None)

print("### B5 (C6)")
window = {}
for n, item in (("B5-instrument-60", "1)"), ("B5-instrument-400", "2)")):
    v = L[n]
    pts = collections.Counter(m.group(1) for fr, t in v
                              for m in [re.search(r'hull-local \(([^)]*)\)', t)]
                              if m and "turret ON TARGET" in t)
    (pt, cnt), = pts.most_common(1)
    check(f"{n}: точка и счёт", f"{cnt} x ({pt})", f"{item} `{cnt} hull-local ({pt})` ({n.rsplit('-', 1)[1]} fps)")
    shots = [float(m.group(1)) for fr, t in v for m in [re.search(r'Weapon: \[Ship\] shot \d+ at t ([\d.]+) s', t)] if m]
    window[n] = len([x for x in shots if 1.0 <= x < 11.0])
check("B5-instrument: выстрелов в окне, 60 / 400", window,
      f"3) **{window['B5-instrument-60']} / {window['B5-instrument-400']}**")
hits = {n: sum(1 for fr, t in L[n] if re.search(r'Projectile: .* hit ', t)) for n in ("B5-nosweep-60", "B5-nosweep-400")}
check("B5-nosweep: попаданий, 60 / 400", hits, f"4) **{hits['B5-nosweep-60']} / {hits['B5-nosweep-400']}**")

print(f"\nИТОГО: сошлось {ok}, не сошлось {bad}, переписано без проверки журналом {copied}")
sys.exit(1 if bad else 0)
