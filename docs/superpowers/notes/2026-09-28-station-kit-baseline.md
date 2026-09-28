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

## 6. Что считается расхождением

Любая находка сравнителя. План не имеет права её объяснить — только записать в таблицу выше и
остановиться.
