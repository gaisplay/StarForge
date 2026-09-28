# База станционного набора `kit-0-before` — до первой правки кода

Спека набора (§6.7) требует, чтобы четыре старых стенда и каждый headless-прогон улик вышли из
набора **строка в строку** такими же, как в проходе, снятом до первой правки кода, на этом дереве и
этой сборке. Здесь записан этот проход и сравнитель, которым с ним сверяется каждая следующая
регрессия плана (`plans/2026-09-28-station-kit-steps-1-2.md`, задача 1).

Сверяются не вердикты `regress_evidence.py`, а сами журналы двух проходов:
`python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/<проход>`.

## 1. Шапка

| | |
|---|---|
| Дата (местная, UTC+3) | 2026-09-28, проход с 04:10:20 по 06:40:39 |
| Дерево | `F:/UEProject/StarForge-station` |
| Ветка | `feat/station-kit` |
| Вершина | `9535ddcfe4ab0834183fc5e202a4b0245bbcb389` — `docs(plan): station-kit steps 1-2, twenty-one tasks from baseline to the checkpoint runbook` |
| Под `Source/`, `Config/`, `Content/` | ни одной правки: `git status --short` показывает только `scripts/__pycache__/`, `git diff --stat bcdb849 -- Source/ Config/ Content/` пуст |
| Над `bcdb849` | четыре коммита, все документы: `86a4e43`, `70800f4`, `737dfb4`, `9535ddc` |
| LFS | шесть файлов (`Content/Maps/*.umap`, `Content/Places/*.uasset`, `Content/Ships/*.uasset`) от 1846 до 8606 байт, ни одного указателя |
| Движок | UE 5.8, установка лаунчера `C:\Program Files\Epic Games\UE_5.8`, `++UE5+Release-5.8-CL-56702186` |
| Тулчейн | Visual Studio 14.44.35228, Windows SDK 10.0.22621.0, ISPC 1.24.0 |
| Пересборка (`summary.json` дословно) | `{'succeeded': True, 'warnings': 0, 'errors': 0, 'actions': 33}`; `Rebuild.bat`, `Total execution time: 42.48 seconds` |
| Головные тесты | **N₀ = 105**: `{'success': 105, 'fail': 0, 'failed': [], 'exit': '0'}`, `TEST COMPLETE. EXIT CODE: 0`; в исходниках `IMPLEMENT_SIMPLE_AUTOMATION_TEST` — 105 |
| Время прохода | **2 ч 30 мин 19 с** (пересборка, тесты, 112 повторов, из них 108 дважды, и 9 заявок) |
| Вариант слияния F4 | **не решён** — F4 в эту ветку не влита; решается владельцем до задачи 5 |

От N₀ = 105 считают задачи 7, 14, 20 и 21: после шага 1 тестов 113, после шага 2 — 121.

## 2. Группы §6.7

```
groups: 19 of 19 present
```

## 3. Вердикты базы

Из `Saved/Regress/kit-0-before/report.md`:

| SAME | CHANGED | VARIES | CHANGED? | INVALID |
|---|---|---|---|---|
| 4 | 108 | 0 | 0 | 0 |

`Replayed: 112 of 112.` Не повторено 16 журналов (`summary.json`, поле `skipped`). Все девять заявок
`CLAIMS` — `HOLDS`.

**`VARIES` — ни одного.** Протокол, с которым сравнивается следующая регрессия, пуст: каждая команда
базы судится равенством строк `project_lines` двух `.now.log`. В том числе поединок и все прогоны
контакта корпусов — у каждого два повтора на этой сборке совпали.

`SAME` — четыре журнала от 2026-09-26, единственные в уликах, которые уже несут строку
`World bounds:`: `worldbounds-control-minusZ-400`, `worldbounds-control-minusZ-60`,
`worldbounds-fly-minusZ-400`, `worldbounds-fly-minusZ-60`. Из 108 `CHANGED` у 58 расходится с опорным
журналом только семейство `World bounds:`; у остальных 50 — ещё и семейства, которые сдвинули более
поздние исправления (F1–F3), поверх той же строки.

## 4. Контроль сравнителя

База против себя:

```
compare: 0 findings
```

код выхода 0.

Подделанная копия (`Saved/Regress/kit-0-doctored`, одна строка
`LogStarForge: Display: Dock: [Ship] doctored.` дописана в конец `.now.log` первого повтора с вердиктом
`SAME` или `CHANGED`). Подделанный журнал — `2026-09-14-controller-task6-spool-kill3-400.log`:

```
- FINDING: LINES 2026-09-14-controller-task6-spool-kill3-400.log: 1 families differ; first Dock:: was None now 'LogStarForge: Display: Dock: [Ship] doctored.'
compare: 1 findings
```

код выхода 1. Копия удалена.

Самосравнение записало `compare.md` в сам каталог базы (`Saved/Regress/kit-0-before/compare.md`) —
это отчёт сравнения с собой, а не часть прохода.

## 5. Таблица регрессий

| Проход | Задача | Дата | Время прохода | Вывод сравнителя | Находки |
|---|---|---|---|---|---|
| `kit-1.2` | 2 — `sf.PlaceAsset`, строки отказа, `ShipStart` | 2026-09-28 | 2 ч 30 мин 11 с (06:55:18–09:25:29) | `compare: 0 findings`, код выхода 0. Пересборка `{'succeeded': True, 'warnings': 0, 'errors': 0, 'actions': 35}`, тесты 105 / 0; `Replayed: 112 of 112. CHANGED 108, SAME 4`, девять заявок `HOLDS`; `groups: 19 of 19 present` | нет |
| `kit-1.4` | 4 — объём `ASFTestStation` двумя коробками, эпизод отказа на носитель | 2026-09-28 | 2 ч 33 мин 23 с (09:59:19–12:32:42) | `compare: 0 findings`, код выхода 0. Пересборка `{'succeeded': True, 'warnings': 0, 'errors': 0, 'actions': 35}`, тесты 105 / 0; `Replayed: 116 of 116. CHANGED 108, SAME 8`, девять заявок `HOLDS`; `groups: 19 of 19 present`. Четыре новых повтора — журналы N9 и N9b после исправления (раздел 7) — `new in run, SAME` все четыре | нет |

## 6. Что считается расхождением

Любая находка сравнителя. План не имеет права её объяснить — только записать в таблицу выше и
остановиться.

## 7. N9 и N9b после исправления объёма `ASFTestStation` (задача 4)

Те же команды, что до исправления (задача 3), на сборке с двумя коробками и эпизодом отказа на
носитель. Журналы — `notes/evidence/2026-09-28-latent-f1-{side,roof}-{60,400}.log`, без метки
`BEFORE-FIX`, так что дальше `regress_evidence.py` повторяет их как опорные.

Стоп-ворота N9 после (§6.9), на каждом из четырёх журналов:

| Журнал | `Ship -> Station` | `while bound to` | `skipped carries [1-9]` | `Carry NOT RUNNING` | `Error` | `Warning` | `World bounds:` |
|---|---|---|---|---|---|---|---|
| `latent-f1-side-60` | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| `latent-f1-side-400` | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| `latent-f1-roof-60` | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| `latent-f1-roof-400` | 0 | 0 | 0 | 0 | 0 | 0 | 1 |

До исправления в тех же журналах было по одной `Frame handover: Ship -> Station` (кадр 147 на 60,
980 на 400) и по десять `Warning` `while bound to`.

**Протокол, класс O2, не ворота.** Без передачи человек остаётся на корабле, и удар корпуса о стену
станции на 0.8 м/с сбивает его во всех четырёх прогонах — строкой `Display`, перенос не прерывается.
До исправления сбития не было ни разу: привязанный к неподвижной станции, он удара не чувствовал.

| Журнал | `Footing LOST` | Лежал | `Shipboard drift` на 10 с |
|---|---|---|---|
| `latent-f1-side-60` | `t=4.28 s, ship-local (-100 320 -300) cm: felt 63.29 m/s2 over the 19.62 limit, ship turning 1.9 deg/s` | 0.5 с | `drift now 15.019 cm, … skipped carries 0, knocked down 0.5 s -- KNOCKED DOWN: the drift is his slide, not the carry` |
| `latent-f1-side-400` | `t=4.25 s, ship-local (-100 320 -300) cm: felt 421.88 m/s2 over the 19.62 limit, ship turning 1.9 deg/s` | 0.9 с | `drift now 26.114 cm, … skipped carries 0, knocked down 0.9 s -- KNOCKED DOWN: …` |
| `latent-f1-roof-60` | `t=4.28 s, ship-local (-100 320 -300) cm: felt 62.48 m/s2 over the 19.62 limit, ship turning 0.4 deg/s` | 0.5 с | `drift now 14.808 cm, … skipped carries 0, knocked down 0.5 s -- KNOCKED DOWN: …` |
| `latent-f1-roof-400` | `t=4.25 s, ship-local (-100 320 -300) cm: felt 416.52 m/s2 over the 19.62 limit, ship turning 0.4 deg/s` | 0.9 с | `drift now 27.268 cm, … skipped carries 0, knocked down 0.9 s -- KNOCKED DOWN: …` |

Сбитие приходит на 4.25–4.28 с — это удар о стену (3.75 с по геометрии плюс раскрутка), а не вход
в прежний объём (2.45 с до исправления). Снос — его скольжение лёжа, не перенос: `skipped carries 0`
во всех четырёх.
