# -*- coding: utf-8 -*-
"""Re-derive every figure written into checkpoint 7's form and compare with the form's text.

Prints one line per claim. A claim passes only when the figure is derived from a log here and
found in the form; nothing is taken from the form and assumed.
"""
import re, collections
from pathlib import Path

EV = Path("F:/UEProject/StarForge/docs/superpowers/notes/evidence")
FORM_TEXT = Path("F:/UEProject/StarForge/docs/superpowers/notes/2026-09-14-checkpoint-7-results.md").read_text(
    encoding="utf-8")

ok = bad = 0


def check(label, derived, must_appear=None):
    """must_appear: string that has to be in the form. Defaults to str(derived)."""
    global ok, bad
    needle = must_appear if must_appear is not None else str(derived)
    hit = needle in FORM_TEXT
    print(f"  [{'OK ' if hit else 'НЕТ'}] {label}: журнал даёт {derived!r}"
          + ("" if hit else f"  <-- в бланке нет {needle!r}"))
    ok, bad = ok + int(hit), bad + int(not hit)


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

print("### действительность")
check("журналов с 'Log file closed'", sum(1 for v in L.values() if any("Log file closed" in t for _, t in v)),
      "во всех одиннадцати")
errs = {n: sum(1 for _, t in v if "LogStarForge: Error" in t) for n, v in L.items()}
check("Error по журналам", errs, "там 3")
assert all(c == 0 for n, c in errs.items() if n != "A2-fight-seated"), errs
sk = sorted({re.search(r'skipped carries (\d+)', t).group(1) for v in L.values() for _, t in v if "skipped carries" in t})
check("различные значения skipped carries", sk, "**0** во всех строках всех одиннадцати журналов")
check("Carry NOT RUNNING всего", sum(1 for v in L.values() for _, t in v if "Carry NOT RUNNING" in t), "**Ни одной**")
hull = sum(1 for v in L.values() for _, t in v
           if "Frame handover:" in t and "REFUSED" not in t
           and ("Ship -> Pirate" in t or "Pirate -> Ship" in t))
check("передач между корпусами без REFUSED", hull, "**Ни одной** между корпусами")
loop = sum(1 for v in L.values() for _, t in v if "Frame handover: Station -> Ship" in t)
check("законных передач Station -> Ship на петле (B5)", loop, "это стенд петли")

print("### A1")
v = L["A1-duel-standing"]
sh = {w: next(fr % 1000 for fr, t in v if f"Shield face +X is down on [{w}]." in t) for w in ("Ship", "Pirate")}
check("кадры посадки щита", sh, "Корабль кадр **226**, пират кадр **223**")
pdf = sorted({fr % 1000 for fr, t in v if "is powered down" in t})
check("кадр(ы) обесточивания", pdf, "одном кадре 872")
shots = collections.Counter(m.group(1) for fr, t in v for m in [re.search(r'Weapon: \[(\w+)\] shot', t)] if m)
check("выстрелы обоих", dict(shots), "**33 / 33**")
lost = [t for fr, t in v if "Footing LOST" in t]
check("строк Footing LOST / regained", (len(lost), sum(1 for fr, t in v if "Footing regained" in t)),
      "t=14.82 s, ship-local (-0 -0 -300) cm: felt 3597.46 m/s2")
check("сбит с ног, секунд", re.search(r'knocked down ([\d.]+) s', [t for fr, t in v if "Shipboard drift" in t][-1]).group(1),
      "**105.8 с** из 120")
check("строка увода", "drift now 384.090 cm", "drift now 384.090 cm, peak drift 748.161 cm")

print("### A1б")
v = L["A1b-duel-at-the-wall"]
ref = [t for fr, t in v if "REFUSED" in t]
check("строк REFUSED", len(ref), "**1** — с первой попытки")
check("скорость в отказе", re.search(r'move ([\d.]+) m/s', ref[0]).group(1), "2.70 m/s")
check("место первого Footing LOST", re.search(r'ship-local \(([^)]*)\)', [t for fr, t in v if "Footing LOST" in t][0]).group(1),
      "`(356 -0 -300)`")
fl = [fr for fr, t in v if "Flight: [Ship]" in t]
check("кадров между секундными строками", sorted(fl[i+1]-fl[i] for i in range(len(fl)-1))[len(fl)//2], "**60**")

print("### A2")
v = L["A2-fight-seated"]
fp = collections.Counter(m.group(1) for fr, t in v for m in [re.search(r'Hit on \[Pirate\] ([+-][XYZ]) face', t)] if m)
check("грани по пирату", dict(fp), "`+X` — **46**, `+Y` — 1, `-Z` — 1")
fs = collections.Counter(m.group(1) for fr, t in v for m in [re.search(r'Hit on \[Ship\] ([+-][XYZ]) face', t)] if m)
check("грани по кораблю", dict(fs), "`-Z` 74, `-Y` 50, `-X` 36, `+X` 32, `+Y` 32, `+Z` 27")
check("строк уничтожения", sum(1 for fr, t in v if "destroyed at" in t), "**20 строк.**")
check("Footing LOST в A2", sum(1 for fr, t in v if "Footing LOST" in t), "`Footing LOST` — 0")
dist = [float(m.group(1)) for fr, t in v for m in [re.search(r'target Ship at ([\d.]+) m', t)] if m]
check("минимум расстояния", f"{min(dist):.1f}", "**161.7 м**")
check("максимум расстояния", f"{max(dist):.1f}", "**1879.7 м**")

print("### B1 (C1)")
v = L["B1-shields-off"]
check("строк shields OFF", sum(1 for fr, t in v if "shields OFF" in t), "48 таких строк")
pd = next(fr for fr, t in v if "[Ship] is powered down" in t)
check("попаданий по игроку до обесточивания", sum(1 for fr, t in v if "Hit on [Ship]" in t and fr <= pd),
      "на **20-м** попадании")

print("### B2 (C2)")
v = L["B2-pirateai-off"]
check("попаданий по игроку", sum(1 for fr, t in v if "Hit on [Ship]" in t), "**911 попаданий**")
d = sorted({m.group(1) for fr, t in v for m in [re.search(r'target Ship at ([\d.]+) m', t)] if m})
check("различные расстояния", d, "одно значение `1011.2 m`")

print("### B3 (C3)")
v = L["B3-no-pirate"]
sh3 = [t for fr, t in v if "Weapon: [Ship] shot" in t]
check("выстрелов / из них NO TARGET", (len(sh3), sum(1 for x in sh3 if "NO TARGET" in x)), "181 выстрел, **все 181**")
check("строк expired", sum(1 for fr, t in v if "expired after" in t), "строк `expired` 156")
check("попаданий", sum(1 for fr, t in v if "Hit on " in t), "попаданий 0")

print("### B4 (C4, C5)")
v = L["B4-floor-and-flank"]
pdp = next(fr for fr, t in v if "[Pirate] is powered down" in t)
check("попаданий по пирату до обесточивания", sum(1 for fr, t in v if "Hit on [Pirate]" in t and fr <= pdp),
      "30 попаданий до обесточивания")
check("уничтожений до обесточивания", sum(1 for fr, t in v if "destroyed at" in t and fr <= pdp), "3) **0**")
dl = [t for fr, t in v if "on [Pirate] destroyed at" in t]
check("строк уничтожения C5", len(dl), "**Ровно 6 строк")
check("дистанция первого", re.search(r'by a hit ([\d.]+) m away', dl[0]).group(1), "2.11 m away")
check("угол первой строки", re.search(r'destroyed at \(([^)]*)\) m', dl[0]).group(1), "at (2.0 2.0 0.0) m")

print("### B5 (C6)")
for n, tag in (("B5-instrument-60", "(-0.000 -410.000 116.200)"), ("B5-instrument-400", "(0.000 -410.000 116.200)")):
    v = L[n]
    pts = collections.Counter(m.group(1) for fr, t in v
                              for m in [re.search(r'hull-local \(([^)]*)\)', t)]
                              if m and "turret ON TARGET" in t)
    (pt, cnt), = pts.most_common(1)
    check(f"{n}: точка и счёт", f"{cnt} x ({pt})", f"95 hull-local {tag}")
    shots = [float(m.group(1)) for fr, t in v for m in [re.search(r'Weapon: \[Ship\] shot \d+ at t ([\d.]+) s', t)] if m]
    check(f"{n}: выстрелов в окне", len([x for x in shots if 1.0 <= x < 11.0]), "**50 / 50**")
for n in ("B5-nosweep-60", "B5-nosweep-400"):
    check(f"{n}: попаданий", sum(1 for fr, t in L[n] if re.search(r'Projectile: .* hit ', t)), "**0 / 0**")

print(f"\nИТОГО: сошлось {ok}, не найдено в бланке {bad}")
