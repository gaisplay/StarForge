# Станционный набор, шаги 1–2 — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Станция перестаёт быть коробкой рига 12 × 8 × 8 м и собирается из стандартного блока набора. Шаг 1 — одна причальная секция M01 (блок 60 × 24 × 32 м, стрела 12 м, зал) как станция, без нового механизма движения, стыковки или передачи. Шаг 2 — ядро причала `B_K3core`: два блока, два порта и один новый механизм, правило ближайшего свободного узла (§9 спеки второй фазы), чистой функцией в `StarForgeCore`. Сверх набора — исправление скрытого дефекта объёма `ASFTestStation` (§6.9) с воспроизведением **до** исправления и третье значение экипажа `Parked`, которым шаг 2 занимает порт (§7.6). Всё под регрессией четырёх стендов и всех улик против базы, снятой до первой правки кода (§6.7).

**Architecture:** Станция набора — класс C++ (§10.2 не пересматривается, решение 12): абстрактная база `ASFKitStation` со сборщиком блока, `ASFBerthStation` на один блок и `ASFPierCoreStation` на два. Три поиска станции перестают знать класс `ASFTestStation` и спрашивают явную метку — интерфейс `ISFDockingStation`, который несёт порты в порядке сборки, фрейм станции и уведомление о смене состояния порта. Порт узнаёт одно новое — кто его держит. Вся проверочная геометрия (выступ за плоскость порта, выход объёма из оболочки, порог печати) и выбор узла — чистые функции в `DockingMath`, под шестнадцатью головными тестами, каждый убит мутантом. Станция набора пишет о себе строку `Station rig:`, в которой каждое число прочитано обратно из мира. В уровень станция попадает через ассет места и стенд места этапа 5, выбранный новой переменной `sf.PlaceAsset`; новой карты и нового режима нет.

**Tech Stack:** Unreal Engine 5.8.2, C++, Automation Test Framework; Python 3 (сценарий регрессии проекта и его сравнитель); Python редактора (`-run=pythonscript`) для ассетов мест.

**Spec:** [docs/superpowers/specs/2026-09-27-station-kit.md](../specs/2026-09-27-station-kit.md) — §4 (стандарт), §5 (пять правил), §6 (шаг 1), §7 (шаг 2), §8 (проверки), §9 (порядок работ), §11 (что вносится в `CLAUDE.md` и в спеку второй фазы), §12 (решения владельца).

**Опирается на:** [мир как граф мест](../specs/2026-09-17-world-as-a-graph-of-places.md) — §9 (ближайший свободный узел), §13, §14; [механики набора](../notes/2026-09-27-station-kit-mechanics.md) — разделы 3 и 6.

**Предыдущий план:** [место как данные](2026-09-17-place-as-data.md) — этап 5; точка проверки 8 открыта и руками не закрыта.

## Global Constraints

- **`StarForgeCore` зависит от `Core` и `CoreUObject` только.** Добавление `Engine` уничтожает смысл разделения модулей и является ошибкой, а не компромиссом. Никаких `UObject` там. `BoundsPastPortPlane`, `BoxReachOutside` и `SelectNearestFreePort` идут туда, потому что это арифметика: ошибка в ней не падает и не логируется, а проявляется как «корабль встал не к тому порту» или «строка спокойна над сломанной станцией».
- **Единицы.** Внутри `StarForgeCore` — СИ. Unreal работает в сантиметрах. Преобразование — только на границе модуля, через `StarForge::Units` (`MetresToCm`, `CmToMetres`), и больше нигде. `DockingMath` по-прежнему безразличен к длине: игра подаёт сантиметры для выбора узла и метры для геометрии строки `Station rig:` и отхода — ровно как сегодня `ReleaseStandoff`.
- **Все пороги — в секундах, ни один в кадрах.** Порог в кадрах — это порог по частоте кадров; проект заплатил за это дважды (счётчик сна Chaos и `MismatchFrames >= 10`). Единственный порог по времени, который этот план читает, — `FrameMismatchThresholdSeconds = 1.0 / 6.0` (`SFShipboardMovementComponent.h:158`); задержки команд — секунды игрового времени через `FTimerManager`.
- **Каждая проверка на стенде гоняется на 60 и на 400 fps** (`-benchmark -fps=60` и `-fps=400`).
- **Каждая константа выводится, а не перечитывается.** Все числа плана — в разделе «Числа»; где спека величины не задаёт, написано «в спеке не задано».
- **Отрицательный контроль обязан быть выполним двумя руками и обязан уметь провалиться.** Механизм — **стоп-ворота**: контроль засчитан, только если строка журнала, которую он обязан породить, переписана в бланк **дословно**.
- **Индикатор не имеет права выглядеть спокойным, когда измеряемое сломано** (§2). Отсюда: NaN в любом числе строки `Station rig:` — красный, а не `0.000`.
- **Numbers go in the log, not on a screen.** Ворота каждого прогона — строки `Saved/Logs/StarForge.log` дерева `F:/UEProject/StarForge-station`, а не панель.
- **Commit after each task. English messages; the body explains why, not what. No `Co-Authored-By`, no "Generated with", no AI attribution of any kind.** Проверять: `git log -1 --format=%B | grep -ci co-authored` — `0`.
- **`git add` только по явным путям из блока Files задачи; `git add -A` и `git add .` запрещены.** Перед коммитом `git status --short` и `git diff --cached --name-only`: ни `.agents/`, ни `.claude/`, ни `.spike/`, ни `.codex/`, ни `docs/art/` в индексе.
- **Никогда не трогать `Source/StarForge/Public/Ship/SFShipFrameComponent.h`, `Source/StarForge/Private/Ship/SFShipFrameComponent.cpp`, `Source/StarForgeCore/Public/ShipFrameMath.h`, `Source/StarForgeCore/Private/ShipFrameMath.cpp`.** В основном дереве над ними идёт работа над находкой F4. Читать и включать (`#include "ShipFrameMath.h"`) можно; править нельзя.
- **F4 в основном дереве шире этих четырёх файлов, и два её файла план правит тоже.** Прочитано 2026-09-28 в `F:/UEProject/StarForge` по содержимому файлов, без git:
  - `Source/StarForge/Private/Ship/SFDockingComponent.cpp` и `.h` переписаны. Помощник `TeleportDetachedRootExactly` в безымянном пространстве заменяет `SetActorTransform` в `CompleteCapture` и на последнем шаге `AdvanceRelease`. Появилось поле `bReleaseArrived`: `LetGo` и отсчёт `sf.Dock 2` уходят на тик после прибытия, и `undocked.` выходит кадром позже — по комментарию самой F4, f61 вместо f60 на 60 fps и f402 вместо f401 на 400. Тела `LetGo`, `ResolvePorts` и `EnterCapture` те же. `RequestUndock` отличается одной строкой, `bReleaseArrived = false;` в ветке отхода. Номера строк сдвинуты.
  - Новые файлы: `Source/StarForge/Private/Debug/SFHitchCommand.cpp` (команда `sf.Hitch`) и `Source/StarForgeCore/Private/Tests/FeltTimeBaseTest.cpp` (7 тестов).

  План правит `SFDockingComponent.*` в задачах 5, 6, 15, 16, 17 и 18. Поэтому **до задачи 5 порядок слияния решает владелец**. Это стоп-точка (шаг 0 задачи 5), а не выбор исполнителя:
  - **(а) F4 вливается в `feat/station-kit` до задачи 5.** Сразу после слияния, до следующей правки, база снимается заново (§6.7, «Где снимаются цифры»). Старый каталог переносится в `Saved/Regress/kit-0-pre-f4`, новый проход снимается в `Saved/Regress/kit-0-before`, так что команды сравнения всех задач остаются прежними. Факт переноса записывается в бланк базы. `NotifyHolder(nullptr)` ставится один раз — в `LetGo`, после строки `undocked.`, а не в `AdvanceRelease`. Уведомление об отходе остаётся в конце ветки отхода `RequestUndock`, то есть в начале отхода, а не на тике прибытия.
  - **(б) F4 вливается после набора.** План идёт как написан. Конфликт слияния ожидается ровно в `CompleteCapture`, `AdvanceRelease` и `LetGo`. При разрешении держатся те же два положения уведомлений. Регрессия слитой ветки сравнивается с базой, снятой заново на ней, а не с `kit-0-before`.
  - **(в) F4 влита до задачи 1.** База сразу снимается с ней, и особых шагов нет.

  Номера строк в задачах даны по `bcdb849`. После F4 якорем служит названный в задаче текст (функция и строка кода рядом), а не номер. Счёт тестов записан от базы: **N₀** — число `Result={Success}` в `Saved/Regress/kit-0-before/summary.json`. Оно равно 105 на `bcdb849` и 105 + 7 = 112 с F4 в нынешнем виде. После шага 1 тестов N₀ + 8, после шага 2 — N₀ + 16.
- **Всё — в рабочем дереве `F:/UEProject/StarForge-station` (ветка `feat/station-kit`).** Никогда не писать в `F:/UEProject/StarForge` и в другие деревья. `F:/UEProject/StarForge-station-art/docs/art/station` — только чтение.
- **Редактор этого дерева закрыт при сборке** — он держит блокировку Live Coding. **Убивать процесс только по командной строке этого дерева, никогда по имени:** `Stop-Process -Name UnrealEditor` закроет и редактор другой сессии, работающей в основном дереве.
  ```bash
  powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe'\" | Where-Object { \$_.CommandLine -like '*StarForge-station*' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }"
  ```
- **Игровой модуль собирается unity-сборкой** (`Intermediate/Build/Win64/x64/UnrealEditor/Development/StarForge/Module.StarForge.cpp` — один блоб на весь модуль). Имена в безымянных `namespace` обязаны быть уникальны **на весь модуль**: `SFTestShip.cpp` уже держит там `CubeEdgeCm`, `PanelThickness`, `HatchHalfWidthCm` и ещё десяток. Константы и помощники новых файлов — во вложенных именованных пространствах (`namespace { namespace KitStation { … } }`), как `Station::` в `SFTestStation.cpp`. Adaptive unity прячет коллизию до коммита, после которого файл возвращается в блоб, и она всплывает в **следующей** задаче.
  Вложенное пространство защищает объявление, но не `using namespace` внутри функции. Директива ставит имена пространства рядом с именами безымянного пространства других файлов. Одноимённое имя становится неоднозначным, как только файл-сосед окажется в блобе раньше. Сегодня `SFKitStation.cpp` стоит в блобе перед `SFTestShip.cpp` только по алфавиту. Поэтому в функциях с `using namespace KitStation;` имена, которые есть и в безымянном пространстве `SFTestShip.cpp`, пишутся только с `KitStation::`. Таких имён два: `CubeEdgeCm` и `MarkerUniformScale` (`SFTestShip.cpp:61`, `:107`). Других совпадений 2026-09-28 нет ни в игровом модуле, ни в новых файлах F4. Ворота — `grep` в шаге 4 задачи 10.
- Каждую консольную переменную и команду объявлять **в файле, который её читает**.
- **UE 5.8 API:** `EAutomationTestFlags` — `enum class`; маска — `EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter`. `UCharacterMovementComponent::GetMovementBase()` помечен `UE_DEPRECATED(5.8)` — только `GetMovementBaseObject()`.
- `UnrealEditor-Cmd.exe` обрезает stdout. Результаты — из `Saved/Logs/StarForge.log`: `Test Completed. Result=`, `TEST COMPLETE. EXIT CODE:`.
- **Движок пишет журнал с концами строк CRLF.** Проверено 2026-09-28: в `Saved/Logs/StarForge.log` основного дерева 1869 строк с `\r`. Строка журнала, которую сравнивают `diff` с текстом из heredoc, сначала проходит через `tr -d '\r'`. Иначе норма не сойдётся на исправной сборке. На `grep -c` и поиск подстроки `\r` не влияет.
- **Мутант откатывается копией, а не git.** Коммитит оркестратор, и к шагу мутантов задача может быть ещё не закоммичена. Тогда `git checkout -- <файл>` вернул бы файл, каким он был до задачи: стёр бы её функции, а файл тестов оставил. Эталон снимается копией в `Saved/Mutants/<задача>/` (`Saved/` вне git, `.gitignore:6`) сразу после зелёного прогона. Каждый откат — `cp` из копии и `cmp` с ней.
- **Процесс UE не убивать по таймауту.** Журнал не сбросится и оборвётся на нулевом кадре. Ключ `-seconds=N` даёт чистый выход.
- **Headless-запуски из Git Bash кончаются на `< /dev/null`.** **Аргумент карты `/Game/...` — с `MSYS_NO_PATHCONV=1` впереди.** **URL с `?game=` — в кавычках.**
- **`-ExecCmds` применяет весь список на первом кадре, после `StartPlay`.** Переменная, которую читает сборка места, приходит через `-ini`, по одному ключу на переменную.
- Документы и планы по-русски; код, комментарии, строки журнала и сообщения коммитов по-английски.

---

## Поправки после слияния ревью 2026-10-01 (внесены 2026-10-14)

Набор отошёл от `feat/place-as-data` на `9cf8d30`, до ревью всего проекта 2026-10-01 (`notes/2026-10-01-review.md`). Задачи 1–5 сделаны до него. 2026-10-14 ревью влито в набор (`2861140`), стык закрыт `42c90a6`, и два независимых рецензента сверили задачи 6–21 с кодом после слияния. Эти поправки **главнее текста задач** там, где они расходятся; в задачах правки внесены на месте и помечены номером поправки. Спека получила свой §15.

- **П1. База и счёт тестов.** База `kit-0-before` снята заново на слиянии (прежняя — `Saved/Regress/kit-0-pre-review`, бланк базы, раздел 0). N₀ = **122**: после шага 1 — **130**, после шага 2 — **138**. Все проходы с `kit-1.6` сравниваются с этой базой; `kit-1.2`, `kit-1.4` сравнивались с `kit-0-pre-f4`, `kit-1.5` — с базой на слиянии F4 (теперь `kit-0-pre-review`).
- **П2. Номера строк.** Номера в задачах — по `bcdb849`/`9cf8d30`; ревью сдвинуло почти все. Якорь — названный текст, не номер. Где задача держится только за номер, вот новые места (2026-10-14): задача 15 — безымянное пространство `SFDockingComponent.cpp` кончается на строке 93, а не 54; задача 16 — `SFTestShip.cpp` 529–541 → 542–553 (блок раздачи флагов в `BeginPlay`: от строки `const bool bAnswersConsole = …` до закрывающей скобки блока `if (Weapon)` включительно — замена задачи сама объявляет `bAnswersConsole`, и начать с `PilotStation->…` значило бы объявить её дважды), 593–600 → 637–644 (ветка `else` без дрона), `SFAutomationOperatorComponent.h` 64/70 → 65/71, `.cpp` 138/169 → 142/193, `SFDockingComponent.h` 188 → 193, `SFThrusterArrayComponent.h` 104 → 105; задача 18 — `SFTestShip.cpp` 713–733 → 757–778, `SFDockingComponent.cpp` 175–180 → 246–261, `SFFrameDriftHUD.cpp` 330–333 → 386–405 (ветка теперь — вложенный `if`/`else`, вставка после 405). Задача 7: `#include <limits>` в `DockingMath.cpp` уже добавлен ревью.
- **П3. Числа из консоли — только `StarForge::Parse::TryParseFiniteNumber`** (`NumberText.h`). `FDefaultValueHelper::ParseDouble` пропускал `.`, `-` и `e10` нулём; ревью (5.1-3, Н22) убрало его из проекта и назвало эту ветку поимённо. Исправлено в задачах 10 (`sf.PortProtrusionM`, оба числа `sf.StationVolumeReach`), 17 (задержка `sf.UndockParked`) и 20 (строки таблицы `CLAUDE.md`); в задачи 10 и 17 добавлены мусорные прогоны с `.` и `-`.
- **П4. Возврат N2 — сам.** С Н2 ревью выход из объёма своего носителя, пока объём другого держит человека, — вход в другой (`HandleEndOverlap`). `sf.StationVolumeReach Boom0 0` возвращает человека на корабль строкой `Frame handover: Station -> Ship, first carry moved him 0.000 cm`; `sf.BindFrame ship` не нужен и теперь предупреждает `… Nothing was rebound; the control did not run.`; N2 больше не обязан быть последним. Задачи 10, 11 (headless-проверка возврата), 20, 21; спека §8.3, §8.5.
- **П5. Строка `SEATED` — только когда принявший носитель не владелец кресла** (задача 11; спека §6.7 п. 3). Иначе возврат N2 печатал бы ложь.
- **П6. Порт с не конечным поворотом не выбирается** (задача 14; спека §7.2). `CanDock` с ревью отвергает NaN-поворот навсегда (Н35). Помощник `IsPlaceable`, утверждения в двух существующих тестах (счёт тестов не меняется), мутант P9.
- **П7. Замок припаркованного** (задача 16). Механизм §7.6 прежний (`LetGo` не отпирает), правится комментарий к `USFThrusterArrayComponent::SetLocked` («undocking clears it»), иначе он лжёт про `Parked`.
- **П8. `sf.PortProtrusionM` в конце мира не сбрасывается** (задача 10). Сбрасываются команды по фронту (`sf.Dock`, `sf.BindFrame`, `sf.AimAt`, Н10 ревью); это уровень, как `sf.PlaceOffsetM`, и строка `Station rig:` показывает оставшееся значение красным. Решение записано в комментарии к переменной.
- **П9. Прогоны с переменным шагом.** H1.2 (задача 12) и H2.2 (задача 19) снимаются ещё и без `-benchmark` (`kitvar`), по два журнала: отход и захват со стоящим человеком — класс F4, который при ровном шаге не проваливается (`CLAUDE.md`). Ворота — красные счётчики и `footing intact`.
- **П10. `CLAUDE.md` в задаче 20.** Запись о двух объёмах ревью уже сделало (`since 2026-10-01 the station's interior is two volumes…`); задача дополняет её, а не пишет вторую с «Until 2026-09-28». Абзац «Current state» называет три базы (П1); абзац о ревью «not yet merged» переписывается по факту. Строка `CLAUDE.md` о белом `no station in this level` уже говорит «docking station (`ISFDockingStation`)» — после стыка слияния панель ищет станцию через интерфейс.
- **П11. Стартовая передача в прогонах с отстыковкой на первом кадре** (H1.2–H1.5, N1, N1b, N5): по Н1 ревью она показывает не `0.000 cm`, а путь корпуса за кадры до приземления (≈5 на 60 fps, ≈27 на 400): d × (3s² − 2s³) у отхода на d — ≈5.76 / 3.82 см при d = 2.93 м (H1.2–H1.5), ≈7.73 / 5.13 при 3.93 (N1), ≈6.16 / 4.08 при 3.13 (N1b); у N5 (`sf.DockRelease 0`, отхода нет) — путь от тяги, ≈2.04 / 1.21 см по журналам рига с тем же контролем. Протокол, не находка: ворот на этих числах нет (задача 12).
- **П12. Инструменты регрессии.** Отчёт `regress_evidence.py` пишет `Fixed step: … have a result.` и `Variable step: …` вместо `Replayed: N of M` — так и записывать строку таблицы регрессий. Прогоны с переменным шагом повторяются дважды и судятся по красным счётчикам (`STEADY` / `RED ROSE` / `RED FELL`); `compare_regress.py` требует `STEADY` в обоих проходах, а новую такую команду пишет заметкой `new in run, STEADY at a variable step`. Опорные журналы — `2026-10-01-review-rebase-*`, и большинство повторов теперь `SAME`. В Global Constraints: `FrameMismatchThresholdSeconds` стоит теперь на `SFShipboardMovementComponent.h:227`, и порогов по времени два — с ревью есть `NeverCarriedThresholdSeconds = 1.0` (сторож «привязан и ни разу не перенесён»). В «Числах» проход базы — 106 команд с ровным шагом и 10 с переменным.
- **П13. Панель.** Строки `NO STATION` больше нет (задачи 18, 21; спека §7.7): без станции — белая `no station in this level`, станция есть, а порт не найден — красная `… found NO STATION port`. Новые красные строки ревью — `CARRY NOT RUNNING - bound to … never carried` и `footing  NONE …` — находка в любом прогоне набора. Место, отвергнутое `sf.PlaceAsset` (N7), панель называет оранжевой строкой `PLACE REFUSED - sf.PlaceAsset '…': …` (стык `42c90a6`).
- **П14. Запрет трогать `SFShipFrameComponent.*` и `ShipFrameMath.*`** держался работой над F4 в основном дереве; F4 влита, но ни одной задаче эти файлы не нужны, и запрет остаётся.
- **П15. Самопроверка в конце плана:** «F4 добавляет семь тестов» — ревью добавило ещё десять (112 → 122); якоря «Counted rather than taken…» в `ResolvePorts` нет с задачи 5, задача 15 держится за новый блок.
- **П16. Бланк базы в `git add`.** Задачи с регрессией (6, 12, 15, 16, 19) дописывают строку в таблицу регрессий бланка базы и ставят его в `git add`, хотя их блок Files его не называет. Так и задумано: строка регрессии — часть задачи. В задаче 16 к её Files и `git add` добавлен `SFThrusterArrayComponent.h` (П7); в «Структуре файлов» он тоже есть.
- **П17. Результаты «в бланк точки 11/12» до задачи 21.** Задачи 7, 12 и 14 велят записать таблицы мутантов и строки ворот в бланк ручной точки, а бланк создаёт только задача 21. До неё исполнитель пишет их в свой отчёт задачи под заголовком «Для бланка точки 11» (или 12), дословно, и задача 21 переносит их в бланк.
- **П18. Ранбук задачи 21 ждёт окна.** Из PowerShell `& "…UnrealEditor.exe" --% …` возвращает управление сразу, и `Save-Log` скопировал бы журнал открытого окна. Строка запуска в ранбуке кончается на `| Out-Null` — конвейер прерывает действие `--%`, и PowerShell ждёт закрытия окна.
- **П20. NaN в строке `Station rig:` (задача 9; найдено рецензентом интерфейсов задачи 7).** `BoxReachOutside` отвечает NaN за сломанный бокс с любой стороны, а строка говорила только «оболочку не прочесть» — теперь различает сломанный объём (`… itself could NOT BE MEASURED`). И максимум по объёмам за плоскостью порта (`!(M <= max)`) пускал NaN внутрь, а следующий конечный объём его затирал: строка вышла бы спокойной над объёмом, которого никто не измерил. Теперь NaN держится до конца цикла и печатается словами `… could NOT BE MEASURED`. Код задачи 9 исправлен на месте.
- **П21. Порог контролей задачи 10 обоснован неверно.** Значение меньше 0.001 м печатается как `0.000`, только если оно меньше 0.0005 (`RedAtOrAboveMetres`); от 0.0005 до 0.001 строка печатает `0.001` и краснеет. Порог 0.001 оставлен — принятый контроль печатает не меньше того, что просили, — исправлены комментарии задачи 10 и фраза спеки §8.3.
- **П22. Задача 7 после приёмки.** `BoundsPastPortPlane` отвечает NaN за не конечную ось и над пустым боксом (её комментарий это обещал, код — нет); три теста получили проверки защитных ветвей, а `BoundsPastPortPlaneReadsZeroAtThePrintedResolution` — закрепление порога 0.0005 через печать `%.3f` (без них мутант `RedAtOrAboveMetres = PrintedResolutionMetres` выживал). Счёт тестов прежний, 130. Комментарий о порогах в коде задачи выше исправлен: 0.071797 и 0.341797 — округлённые вверх и уже за пределом, точны 7 − 4√3 и 7.27 − 4√3 (спека §5).
- **П19. Подписи интерфейсов** «Потребляет из задачи N» в задачах 6, 9, 10 и 14 местами называют не ту задачу-источник (строки сверки F8); на код это не влияет, источник — тот, где интерфейс объявлен по тексту плана.

---

## Область этого плана

Отвечает на один вопрос: **можно ли собрать станцию из стандартного блока так, чтобы каждое из пяти правил сборки (§5) было видно в журнале числом, а старые стенды не сдвинулись ни строкой.**

Проверок четыре рода:

1. **Головные тесты** — шестнадцать новых (§8.1), каждый со своим мутантом. Счёт — от базы: после шага 1 тестов **N₀ + 8**, после шага 2 — **N₀ + 16**. N₀ — число тестов в `kit-0-before`: 105 на `bcdb849`, 112, если до базы влита F4 (Global Constraints).
2. **Headless-прогоны** на стенде места с ассетами набора — H1.1–H1.6, H2.1–H2.2 и отрицательные контроли N1–N9 (§8.2, §8.3), на обеих частотах, каждый с журналом в `notes/evidence`.
3. **Регрессия** — `scripts/regress_evidence.py` отдельным проходом после каждого пункта §9, помеченного «регрессия», в свой каталог, и сравнение прохода с базой **строками**, а не вердиктами (§6.7).
4. **Ручные точки проверки 11 и 12** (§8.5) — по ранбуку, который пишет последняя задача.

### Решения, которые пришлось принять сверх спеки

- **Сравнение двух проходов — отдельный сценарий `scripts/compare_regress.py` на функциях `regress_evidence.py`** (§6.7 оставляет выбор плану). Отдельный, а не новый ключ старого: `regress_evidence.py` снимает улики, сравнитель их только читает, и ошибка в сравнителе не может испортить проход. У сравнителя есть свой отрицательный контроль — подделанная копия базы (задача 1).
- **Имена портов в строках `Dock:` на станции с несколькими портами — двумя способами.** Отказ по окну получает префикс, как в примере спеки: `Dock: [Ship] refused -- Station.Port1: distance 60.1 m over 3.0`. Строки доводки, стыковки, отхода и отстыковки **текст не меняют**, а получают **отдельную** строку `Dock: [Ship] the port is Station.Port1, one of 2.` Причина — ворота самой спеки: N8 и H2.2 цитируют на станции с двумя портами `Dock: [Parked] releasing, 2.93 m along the port axis over 1.0 s, peak 17.6 m/s2.` и `Dock: [Ship] undocked.` буква в букву. Имя, вставленное в эти строки, провалило бы их на исправной сборке. На станции с одним портом не меняется ни одна строка (§7.7).
- **Промах `sf.AimAt station` меняет текст** с `no ASFTestStation in this level` на `no docking station in this level` — единственное допустимое расхождение §6.7. Журналов улик, где встречается старый текст, **ноль** (`grep -rl "no ASFTestStation in this level" docs/superpowers/notes/evidence` — пусто, проверено 2026-09-28). Задача 5 повторяет проверку.
- **Getter `ASFTestStation::GetInterior()` удаляется.** Его не зовёт никто (§6.9), а после исправления его комментарий «The box, the tunnel and everything in them» лгал бы: коробок две.
- **Кто может сесть на пост — таблица по трём значениям экипажа, а не буква §7.6.** Спека пишет: «автоматике и преследованию — только на `Automated`». Буквально это сняло бы автоматику с поста стрелка корабля игрока, где она сидит по `sf.AutomationOperator 1` на всех четырёх стендах, и регрессия рухнула бы на первой строке `Turret: [Ship] gunned by automation`. Смысл §7.6 — что «не слушает консоль» перестаёт значить «автоматический». План делает так: автоматика — на `PlayerShip` по `sf.AutomationOperator`, на `Automated` всегда, на `Parked` никогда; преследование — только на `Automated`; консольный оператор — только на `PlayerShip` (сегодняшний `bConsoleControlled` это уже и есть). Записано в рисках для владельца.
- **Ассеты мест создаются Python-сценарием редактора (`-run=pythonscript`) и читаются им же обратно, а не руками.** Спека (§6.6, §9) пишет «руками в редакторе». Сценарий даёт то же и воспроизводимо, а приёмка остаётся спекиной — строка `Place carrier:`, где класс прочитан обратно. Если коммандлет Python не поднимется, задача 3 даёт точные клики в редакторе.
- **`DA_Place_Berth` создаётся в задаче 8, а не в пункте 10 §9**, и `DA_Place_PierCore` — в задаче 13: иначе классу станции нечем попасть в мир на своей же задаче, и её нельзя проверить. **`ASFPierCoreStation` строится до правила выбора** (задача 13 перед задачами 14–15): правилу нужна станция с двумя портами, чтобы его можно было проверить на стенде в той же задаче, где оно появилось.
- **Тесты шагов 1–2 — в двух новых файлах** (`StationKitDockingTest.cpp`, `PortSelectionTest.cpp`), а не в `DockingMathTest.cpp`, который правила починка F3.
- **Свет и маркеры зала** выведены из рига (§6.3 это и требует): радиус света — расстояние от центра зала до начала порта, 18.31 м, вверх до целого метра — 19 м; маркеры стоят от внутренних граней зала на тех же 140 и 90 см, что у рига (590 − 450 и 390 − 300).
- **Контрольный коллайдер N1 — только на блоке 0.** Бокс отхода один на станцию (§5, правило 1), и выступ в любом месте грани стоит полного p у каждого порта.
- **`sf.PortProtrusionM`, заданный повторно тем же значением, строку не перепечатывает.** Делегат изменения переменной срабатывает и на `Set` без изменения (`SFWorldBoundsSubsystem.h:52-55` говорит об этом прямо).
- **Запись `ParkedHulls`, не прошедшая проверку, делает строку `Place rig:` красной** (`N of M parked hulls, K NOT SPAWNED`). Спека требует `Error` только в момент решения; строка стенда, спокойная при непоявившемся корпусе, была бы индикатором, который выглядит спокойным над сломанным.
- **Ранбук точек 11 и 12 пишется последней задачей** (по указанию оркестратора), хотя §9 ставит точку 11 между шагами. Следствие записано в рисках: точка 11, пройденная по этому ранбуку, пройдена на сборке, где уже есть шаг 2.
- **Проверка записи `ParkedHulls` гоняется прогоном, а не держится кодом** (задача 17). Спека требует `Error` в момент решения (§7.6), но ни один прогон §8.2–§8.3 не даёт плохой записи. Механизм, до которого не доходит ни один прогон, никем не проверен. Поэтому есть контрольный ассет `DA_Place_PierCoreBadRecords`: одна годная запись и семь плохих, по одной на каждую проверку, до которой может дойти прогон. Он даёт семь строк `NOT SPAWNED` и красную строку `Place rig:`. Журнал — проверка задачи, в улики он не идёт. Две проверки прогоном недостижимы. «Носитель не станция» потребовал бы в месте носителя-корабля, то есть второго корпуса, который слушает консоль. «Не появился» возникает только при отказе `SpawnActor`.

---

## Числа, одной таблицей

Каждое выведено здесь или в разделе спеки, на который стоит ссылка. «В спеке не задано» написано явно.

| Величина | Значение | Откуда |
|---|---|---|
| Головных тестов сегодня | **105** на `bcdb849`; **112** с F4 в нынешнем виде (+7 в `FeltTimeBaseTest.cpp`) | `grep -c IMPLEMENT_SIMPLE_AUTOMATION_TEST` по `Source/StarForgeCore/Private/Tests/` обоих деревьев, 2026-09-28 |
| N₀ — тестов в базе | `Result={Success}` из `Saved/Regress/kit-0-before/summary.json` | Global Constraints, пункт об F4 |
| После шага 1 / после шага 2 | **N₀ + 8 / N₀ + 16** (130 / 138 при N₀ = 122 — база после слияния ревью, поправка П1; 113 / 121 при N₀ = 105 на `bcdb849`) | +8 и +8 (§8.1) |
| Корень станции набора | центр `Hall0`, в системе B (−11.62, −5.9, 16) м | §6.2 |
| Начало блока 0 в системе актора | (11.62, 5.9, −16) м = (1162, 590, −1600) см | §6.2: система актора = B − (−11.62, −5.9, 16) |
| Начало блока 1 (ядро) | (71.62, 5.9, −16) м | §7.1: `B_K3core` (−30, 0, 0) и (30, 0, 0) |
| `Port0` / `Port1` в системе актора | (0.00, −18.10, −2.75) / (60.00, −18.10, −2.75) м | §6.2, §7.1 |
| Бокс станции, шаг 1 | X −18.38..41.62, Y −18.10..17.90, Z −16..16 м | §6.8 |
| Бокс станции, шаг 2 | X −18.38..101.62, остальное то же | §7.1 |
| Коллайдеров на блок / объёмов на блок | 15 / 2 | §6.3, §6.4 |
| Отход при p = 0 | 4√3 − 4 = **2.928203 м** → печать `2.93` | §5 |
| Пик отхода при p = 0 | 6 × 2.928203 = 17.569 м/с² → `17.6` | §5 |
| Порог окна | p > 7 − 4√3 = **0.071797 м** (сравнение `>`) | §5, `SFDockingComponent.cpp:556` |
| Порог потолка | p ≥ 19.62/6 − (4√3 − 4) = **0.341797 м** (сравнение `>=`) | §5, `:562` |
| Порог красного в строке `Station rig:` | **0.0005 м** — то, что печатается как `0.001` | §6.8 |
| N1 | p = 1.0 → отход `3.93`, пик `23.6` | §8.3 |
| N1b | p = 0.2 → отход `3.13`, пик `18.8` | §8.3 |
| N2 | `Boom0` + 4.0 м, задержка 1.0 с; `Standing on … (0.17 s)` | §8.3 |
| N2b | `Hall0` + 1.0 м, задержка 1.0 с | §8.3 |
| Задержка 1.0 с у N2/N2b | **не выведена** — запас над стартовой передачей | §8.3 |
| Контрольный коллайдер N1 | 4 × 4 × 4 м, X 26..30, Z 0..4 блока 0, грань −Y на p за плоскостью | §8.3 |
| N9 | корабль (5.40, −11.10, 0.00) м; человек (−1.0, 3.2, 0.0) м в осях корабля; 0.8 м/с | §6.9 |
| N9b | корабль (0.00, −11.10, 3.00) м; человек тот же | §6.9 |
| N9: вход в старый объём / удар о стену | ≈ 1.95 с / 3.75 с; окно ворот — между 1-й и 3-й строками `Flight:` | §6.9 |
| H2.1, N3: до `Port0` / `Port1` | **22.10 / 63.94 м** | §7.4 |
| H2.2, N8: до соседнего порта после отхода | √(60² + 2.928²) = **60.07 м** | §8.2 |
| N4: старт корабля; до `Port0` / `Port1` | (0, −40, 0) м; **17.90 / 62.61 м** | §8.3 |
| N6: носитель; оба расстояния | (−30, 0, 0) м; √(30² + 22.1² + 0.1²) = **37.26 м** | §7.5 |
| N8: задержка `sf.UndockParked` | 2.0 с = отход 1.0 с + 1.0 с зазора | §8.3 |
| Свет зала | 2000 кд (как у рига); радиус **1900 см** | в спеке не задано; правило «не меньше расстояния до порта» — §6.3, 18.31 м вверх до целого метра — этот план |
| Маркеры зала | масштаб 0.8, смещения X 660 см, Y 500 см от центра зала | в спеке не задано; зазоры рига 140 и 90 см — этот план |
| Длительность headless-прогонов шагов 1–2 | H1.1, H2.1: 70 с; H1.2, H2.2, N3: 40 с; H1.6, N2, N2b: 20 с; остальные 25 с; N9: 12 с; проверка записи `ParkedHulls` (задача 17): 10 с | H1.1 и N9 — §8.2, §6.9; остальные в спеке не заданы. 25, 40 и 70 с взяты у прогонов улик той же команды на петле (`2026-09-25-f3fix-*`); 20 и 10 с выбраны этим планом |
| Проход регрессии | 112 повторов в базе. Большинство идёт дважды: почти всё `CHANGED` из-за строки `World bounds:`. Плюс 9 заявок, пересборка с очисткой и тесты. После задачи 4 повторов становится на 4 больше, после задачи 12 — ещё на 28, после задачи 19 — ещё на 12 | Повторы — `collect()` на этом дереве 2026-09-28: 112 в плане, 16 пропущено. Headless-журналов улик с командной строкой 184 (§6.7), различных команд среди них 112 |
| Время прохода | Оценка: 112 × 2 × ~40 с ≈ 2.5 ч, плюс пересборка. Девять проходов плана (`kit-0-before`, 1.2, 1.4, 1.5, 1.6, 1.10, 2.2, 2.4, 2.7) — порядка суток машинного времени | В спеке не задано. Запуск длится 36–53 с (поле `seconds` в `F:/UEProject/StarForge/Saved/Regress/*.json`, последний проход основного дерева). Это оценка, а не бюджет: фактическое время каждого прохода записывается в таблицу регрессий |

---

## Структура файлов

```
Source/StarForgeCore/
  Public/DockingMath.h                        правится: BoundsPastPortPlane, BoxReachOutside,
                                              PrintedResolutionMetres, RedAtOrAboveMetres,
                                              IsRedAtPrintedResolution (задача 7); FPortCandidate,
                                              EPortRefusal, FPortChoice, SelectNearestFreePort (задача 14)
  Private/DockingMath.cpp                     правится: то же
  Private/Tests/StationKitDockingTest.cpp     создаётся: 8 тестов шага 1 (задача 7)
  Private/Tests/PortSelectionTest.cpp         создаётся: 8 тестов шага 2 (задача 14)

Source/StarForge/
  Public/Ship/SFDockingStation.h              создаётся: ISFDockingStation, FindAll (задача 5)
  Private/Ship/SFDockingStation.cpp           создаётся (задача 5)
  Public/Ship/SFKitStation.h                  создаётся: база набора — сборщик блока (8),
                                              строка Station rig: (9), контроли (10)
  Private/Ship/SFKitStation.cpp               создаётся: то же; sf.PortProtrusionM и
                                              sf.StationVolumeReach объявлены здесь
  Public/Ship/SFBerthStation.h, Private/...   создаётся: один блок (8)
  Public/Ship/SFPierCoreStation.h, Private/...создаётся: два блока (13)
  Public/Ship/SFTestStation.h, Private/...    правится: две коробки объёма (4), интерфейс (5)
  Public/Ship/SFInteriorVolumeComponent.h,
    Private/...                               правится: эпизод отказа на носитель (4), SEATED (11)
  Public/Ship/SFDockingPortComponent.h,
    Private/...                               правится: строка о порте-сироте (5), держатель (6)
  Public/Ship/SFDockingComponent.h,
    Private/...                               правится: поиск по интерфейсу (5), уведомления и
                                              MeasureRelease (6), выбор узла (15), Parked (16),
                                              DockImmediatelyAt (17), NO FREE PORT и имена портов (18)
  Public/Ship/SFAutomationOperatorComponent.h,
    Private/...                               правится: поиск (5), SetMayTakeSeat (16)
  Private/Player/SFCharacter.cpp              правится: поиск (5)
  Public/Ship/SFPursuitOperatorComponent.h,
    Private/...                               правится: SetMayTakeSeat (16)
  Public/Ship/SFConsoleOperatorComponent.h    правится: IsConsoleControlled (16)
  Public/Ship/SFPilotStationComponent.h       правится: IsConsoleControlled (16)
  Public/Ship/SFWeaponComponent.h             правится: IsConsoleControlled (16)
  Public/Ship/SFThrusterArrayComponent.h      правится: комментарий к SetLocked (16, поправка П7)
  Public/Ship/SFTestShip.h, Private/...       правится: Parked (16), sf.UndockParked (17),
                                              строка полёта NO FREE PORT (18)
  Private/Debug/SFFrameDriftHUD.cpp           правится: панель NO FREE PORT (18)
  Public/World/SFPlaceDataAsset.h, Private/...правится: ShipStart (2), ParkedHulls (17)
  Public/SFPlaceGameMode.h, Private/...       правится: sf.PlaceAsset и ShipStart (2),
                                              ParkedHulls и строка Place parked hull: (17)

scripts/compare_regress.py                    создаётся: сравнение двух проходов (задача 1)
scripts/station_kit_places.py                 создаётся: ассеты мест набора (задача 3)

Content/Places/DA_Place_LatentF1Side.uasset   создаётся (3)
Content/Places/DA_Place_LatentF1Roof.uasset   создаётся (3)
Content/Places/DA_Place_Berth.uasset          создаётся (8)
Content/Places/DA_Place_PierCore.uasset       создаётся (13)
Content/Places/DA_Place_PierCoreTie.uasset    создаётся (15)
Content/Places/DA_Place_PierCoreParked0.uasset создаётся (17)
Content/Places/DA_Place_PierCoreParkedBoth.uasset создаётся (18)
Content/Places/DA_Place_PierCoreBadRecords.uasset создаётся (17): контроль проверки записи ParkedHulls, не место набора

docs/superpowers/notes/<дата>-station-kit-baseline.md   создаётся (1), дополняется каждой регрессией
docs/superpowers/notes/evidence/<дата>-*.log            журналы прогонов
docs/superpowers/notes/<дата>-checkpoint-11-12-runbook.md  создаётся (21)
docs/superpowers/notes/<дата>-checkpoint-11-12-results.md  создаётся (21)
CLAUDE.md                                               правится (20)
docs/superpowers/specs/2026-09-17-world-as-a-graph-of-places.md   правится (20)
```

`<дата>` — день выполнения задачи, `$(date +%F)` в Git Bash.

Разделение ответственности:

- **`DockingMath`** не знает, что такое актор и мир. Он знает боксы, плоскости, трансформы и флаги занятости.
- **`ISFDockingStation`** говорит, **что актор есть**: станция с портами в порядке сборки, фреймом и ухом для смены состояния порта. Больше ничего.
- **`USFDockingPortComponent`** по-прежнему ничего не двигает и не решает. Он знает одно — кто его держит.
- **`USFDockingComponent`** — единственное место, где решают о стыковке: выбирает узел, ставит и снимает держателя, меряет отход.
- **`ASFKitStation`** собирает блок и говорит о себе. Ни одного решения о стыковке и передаче в нём нет.
- **`ASFPlaceGameMode`** читает ассет, собирает и печатает. Какая станция стоит в месте, он не знает — только класс и позу.

---

## Как проверяется

Все команды — из Git Bash, из корня рабочего дерева:

```bash
cd /f/UEProject/StarForge-station
git branch --show-current     # feat/station-kit
```

Сборка:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
```

Пересборка с очисткой (её делает и `regress_evidence.py`; обычная сборка на свежем дереве отрабатывает за четыре действия, и «0 предупреждений» с неё ничего не мерит):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Rebuild.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
```

Головные тесты — все или подветка:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log < /dev/null
grep -c "Test Completed. Result={Success}" Saved/Logs/StarForge.log
grep -c "Test Completed. Result={Fail}" Saved/Logs/StarForge.log
grep "TEST COMPLETE. EXIT CODE:" Saved/Logs/StarForge.log
```

Подветка — заменить `StarForge.Core` на `StarForge.Core.Docking`.

Четыре старых стенда с путём этого дерева:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log < /dev/null
MSYS_NO_PATHCONV=1 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log < /dev/null
MSYS_NO_PATHCONV=1 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" /Game/Maps/L_Combat -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=40 -ExecCmds="sf.FlightLog 1" -log < /dev/null
MSYS_NO_PATHCONV=1 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" /Game/Maps/L_Place -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log < /dev/null
```

**Стенд места с ассетом набора** — одна функция на весь план. Она пускает стенд места по URL (как все цифры этапа 5), подаёт ассет ключом `-ini`, дописывает остальные ключи и копирует журнал в улики под датой дня:

```bash
kit() {
  local asset="$1" fps="$2" secs="$3" cmds="$4" name="$5"; shift 5
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" \
    "/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFPlaceGameMode" \
    "-ini:Engine:[ConsoleVariables]:sf.PlaceAsset=$asset" "$@" \
    -game -nullrhi -unattended -nosplash -benchmark -fps="$fps" -seconds="$secs" -ExecCmds="$cmds" -log < /dev/null
  cp Saved/Logs/StarForge.log "docs/superpowers/notes/evidence/$(date +%F)-$name.log"
}
L=Saved/Logs/StarForge.log
```

Функцию надо объявлять заново в каждой новой оболочке. В журнал движок пишет командную строку в той же форме, что у всех записанных прогонов (`-ExecCmds="…"`, `-ini:…` без кавычек), и `regress_evidence.py` потом повторяет её как опорную.

**Ворота, общие для всех прогонов** (§8.2), — каждый раз, на каждом журнале, и у N7 тоже:

```bash
grep -c "LogStarForge: Display: World bounds:" $L   # 1 -- во всех прогонах без исключения
grep -c "skipped carries [1-9]" $L                   # 0
grep -c "Carry NOT RUNNING" $L                       # 0
grep -c "LogStarForge: Error" $L                     # 0, если в таблице прогона не названо иное
grep -c "LogStarForge: Warning" $L                   # 0, если в таблице прогона не названо иное
```

Любая строка `Warning` или `Error`, не названная у своего прогона в таблице §8.2, — **находка**. Общего списка исключений нет намеренно. У N7 человека нет, и два счётчика переноса читают 0, ничего не доказывая. Вместо них у N7 работает отсутствие строк `Shipboard drift` и `Frame [`. Вердикт опоры из таблицы §8.2 читается у каждого прогона отдельной командой, в воротах этого прогона.

**Регрессия** — отдельный проход в свой каталог (`kit-<шаг>.<пункт §9>`), затем сравнение с базой:

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-1.2
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-1.2; echo "exit $?"
```

Ожидается последняя строка `compare: 0 findings` и `exit 0`. `--resume` допустим только для того, чтобы дописать прерванный проход в его же каталог; повтор, который был `INVALID`, переснимается так: удалить его `.json` из каталога прохода и дописать проход с `--resume`.

---

## Что проверено в исходниках движка

Адреса проверены 2026-09-28 в установленном `C:/Program Files/Epic Games/UE_5.8/Engine/Source` и `…/Engine/Plugins`. Задача, опирающаяся на пункт, переносит факт в комментарий у вызова.

| Что | Где | Что из этого следует |
|---|---|---|
| Коммандлет Python ищет `-Script=` без учёта регистра и, если значение не в кавычках, берёт его **до первого пробела** | `Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PythonScriptCommandlet.cpp:20-33` | Аргумент сценарию через `-script="путь аргумент"` из Git Bash не доходит: MSYS берёт в кавычки весь аргумент, и значение режется на пробеле. Группа ассетов передаётся **переменной окружения** `KIT_PLACES_GROUP` |
| Коммандлет пишет `Python script executed successfully` или `Python script executed with errors` в `LogPythonScriptCommandlet` | там же, `:65-72` | Это ворота каждого запуска сценария ассетов |
| `PythonScriptPlugin` включается в проекте транзитивно: `ToolsetRegistry` зависит от него и от `EditorScriptingUtilities` | `Plugins/Experimental/ToolsetRegistry/ToolsetRegistry.uplugin:30-37`; `AllToolsets` включён в `StarForge.uproject` | Коммандлет есть без правки `.uproject`. Проверяется пробой в задаче 3, а не этим доводом |
| Python выставляет свойство с `EditAnywhere` только при `GIsEditor` | `…/PythonScriptPlugin/Private/PyGenUtil.cpp:1814` | Новые поля ассета места — `EditAnywhere`, структуры — `BlueprintType` |
| `UDataAssetFactory::DataAssetClass` — `EditAnywhere` | `Editor/UnrealEd/Classes/Factories/DataAssetFactory.h:18-19` | Ассет создаётся фабрикой из Python |
| `UBoxComponent::SetBoxExtent(…, bUpdateOverlaps)` при созданном физическом состоянии зовёт `UpdateOverlaps()` | `Runtime/Engine/Private/Components/BoxComponent.cpp:28-47` | Выросший объём (N2) даёт `BeginOverlap` тому, кого накрыл, без тика |
| `UPrimitiveComponent::SetCollisionEnabled` при смене типа зовёт `EnsurePhysicsStateCreated()` | `Runtime/Engine/Private/PrimitiveComponentPhysics.cpp:1376-1394` | Контрольный коллайдер, созданный с `NoCollision`, можно включить посреди игры |
| `TActorIterator` принимает `const UWorld*` | `Runtime/Engine/Public/EngineUtils.h:582` | `ISFDockingStation::FindAll` берёт константный мир |
| Интерфейс с чисто виртуальными методами — `UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))` | образец `IAbilitySystemInterface`, `AbilitySystemInterface.h:19-31` (§6.7, п. 1) | Класс, объявивший интерфейс и забывший метод, не соберётся |

---

## Шаг 1: причальная секция

Порядок задач 1–4 задан спекой (§9, последний абзац): база — до первой правки кода; перед исправлением `ASFTestStation` — только то, что нужно его воспроизведению (инструменты стенда и прогоны до исправления); исправление — своей регрессией, чтобы его расхождение читалось без примеси набора.

### Task 1: База `kit-0-before` до первой правки кода, и сравнитель двух проходов

База — это то, с чем сверяется каждая следующая регрессия. Снимается **до первой правки кода**, на этом дереве и этой сборке (§6.7). Сравнитель пишется здесь же, потому что без него база — просто каталог: вердикты скрипта сравнивать бессмысленно, почти все повторы `CHANGED` уже в базе из-за строки `World bounds:` (§6.7).

Задача не правит ни одного файла под `Source/`, `Config/`, `Content/`.

**Files:**
- Create: `scripts/compare_regress.py`
- Create: `docs/superpowers/notes/<дата>-station-kit-baseline.md`

**Interfaces:**
- Потребляет: `scripts/regress_evidence.py` — `project_lines(text)`, `red_counts(text)`, `first_difference(a, b)`, `same_command(logged, cmd)`, `RED`; формат прохода: `<каталог>/<tag>.json` с полями `baseline`, `cmd`, `verdict`, `red_baseline`, `red_now`; `<каталог>/logs/<tag>.now.log`, `<tag>.again.log`; `<каталог>/summary.json`; заявки — `.json` с полем `claim`.
- Отдаёт всем регрессиям плана: `python scripts/compare_regress.py <база> <проход>` → код выхода 0/1, отчёт в stdout и в `<проход>/compare.md`; `python scripts/compare_regress.py --groups <проход>` → `groups: N of 19 present`.

- [ ] **Step 1: Убедиться, что дерево то, на котором снимается база**

```bash
cd /f/UEProject/StarForge-station
git branch --show-current                  # feat/station-kit
git log --oneline -2                       # 70800f4 (заметка), под ней bcdb849
git status --short                         # ни одного файла под Source/, Config/, Content/
git diff --stat bcdb849 -- Source/ Config/ Content/   # пусто
```

Любой изменённый файл под `Source/`, `Config/`, `Content/` — стоп: база снимается не с чьей-то недоделки. Одно исключение — вариант (в) из Global Constraints: владелец влил F4 до базы. Тогда вершина — слияние, а `git diff --stat bcdb849` показывает ровно файлы F4 (на 2026-09-28 это шесть изменённых и два новых, перечислены там же). Записать хэш вершины и вариант (а/б/в, если уже решён) в шапку бланка базы.

- [ ] **Step 2: Убедиться, что содержимое LFS настоящее, а не указатели**

В свежем рабочем дереве `.umap` и `.uasset` могут остаться текстовыми указателями LFS. Тогда карты не грузятся, прогоны по карте выходят с кодом 0 и нулём строк `LogStarForge`, а регрессия этого не заметит.

```bash
for f in Content/Maps/*.umap Content/Places/*.uasset Content/Ships/*.uasset; do
  printf "%8d %s %s\n" "$(stat -c %s "$f")" "$(head -c 40 "$f" | grep -c 'git-lfs')" "$f"
done
```

Ожидается у каждого файла размер больше 1500 байт и `0` во втором столбце. Иначе — `git lfs pull` в этом дереве и повтор шага.

- [ ] **Step 3: Написать сравнитель**

`scripts/compare_regress.py`:

```python
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
  - every claim reads HOLDS in both passes;
  - a command present only in the later pass (its log was laid in evidence after the base) is judged
    by its own verdict: SAME, or VARIES with the red counters of its baseline unchanged;
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
```

- [ ] **Step 4: Снять базу**

Редактор этого дерева закрыт (команда из Global Constraints). Проход пересобирает с очисткой, гоняет головные тесты, повторяет все улики и заявки. Headless-журналов улик с командной строкой 184, различных команд среди них 112, и `collect()` даёт по одному повтору на команду. Почти каждый повтор `CHANGED`, поэтому идёт дважды. Плюс 9 заявок. Оценка по «Числам» — около 2.5 ч на проход; время прохода записать в бланк, дальше по нему планируются остальные восемь.

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-0-before
```

- [ ] **Step 5: Прочитать базу**

```bash
python -c "import json;s=json.load(open('Saved/Regress/kit-0-before/summary.json'));print(s['build']);print(s['tests'])"
cat Source/StarForgeCore/Private/Tests/*.cpp | grep -c IMPLEMENT_SIMPLE_AUTOMATION_TEST   # столько же, сколько success выше
python scripts/compare_regress.py --groups Saved/Regress/kit-0-before     # groups: 19 of 19 present
grep -c '"verdict": "INVALID"' Saved/Regress/kit-0-before/*.json | grep -v ':0' ; echo "invalid files above (none expected)"
```

Ожидается: `succeeded: True`, тесты **N₀ / 0**, где N₀ равно числу тестов в исходниках (105 на `bcdb849`), `groups: 19 of 19 present`, ни одного `INVALID`. N₀ записывается в шапку бланка: от него считают задачи 7, 14, 20 и 21. `INVALID` — удалить его `.json` и дописать проход `--resume` в тот же каталог. Недостающая группа — снять её журнал на этом дереве до правки кода (команда — строка из `GROUPS`, запущенная так же, как в `regress_evidence.py`) и повторить проход.

- [ ] **Step 6: Самопроверка и отрицательный контроль сравнителя**

Сравнитель, который ничего не находит, выглядит так же, как сравнитель, который не умеет искать. Поэтому два прогона: база против себя обязана дать ноль, подделанная копия — находку.

```bash
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-0-before; echo "exit $?"   # 0 findings, exit 0
cp -r Saved/Regress/kit-0-before Saved/Regress/kit-0-doctored
python - <<'PY'
import json, pathlib
d = pathlib.Path("Saved/Regress/kit-0-doctored")
for f in sorted(d.glob("*.json")):
    r = json.loads(f.read_text("utf-8"))
    if r.get("verdict") in ("SAME", "CHANGED"):
        log = d / "logs" / (r["baseline"][:-4] + ".now.log")
        log.write_text(log.read_text("utf-8", errors="replace") +
                       "\n[2026.09.28-00.00.00:000][  0]LogStarForge: Display: Dock: [Ship] doctored.\n", "utf-8")
        print("doctored", r["baseline"])
        break
PY
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-0-doctored; echo "exit $?"  # 1 finding: LINES <тот журнал>, family Dock; exit 1
rm -rf Saved/Regress/kit-0-doctored
```

Если подделка не найдена — сравнитель сломан, и ни одна регрессия плана не засчитывается, пока он не починен.

- [ ] **Step 7: Бланк базы**

`docs/superpowers/notes/<дата>-station-kit-baseline.md`. Разделы:

1. **Шапка:** дата, ветка, хэш вершины, версия движка, результат пересборки (из `summary.json` дословно), тесты `N₀ / 0` с числом, время прохода, вариант слияния F4 (а/б/в или «не решён»; решается до задачи 5).
2. **Группы §6.7:** вывод `--groups` дословно.
3. **Вердикты базы:** счётчики `SAME / CHANGED / VARIES / CHANGED? / INVALID` из `report.md`, и список `VARIES` поимённо — это протокол, с которым сравнивается каждая следующая регрессия.
4. **Контроль сравнителя:** обе строки `compare:` из шага 6 и имя подделанного журнала.
5. **Таблица регрессий** — пустая, с колонками «проход», «задача», «дата», «время прохода», «вывод сравнителя», «находки». Каждая следующая регрессия дописывает в неё строку. Если база снимается заново после F4 (вариант (а)), это отдельная строка с пометкой «новая база, прежняя — `kit-0-pre-f4`».
6. **Что считается расхождением:** любая находка сравнителя. План не имеет права её объяснить — только записать и остановиться.

- [ ] **Step 8: Коммит**

```bash
git status --short
git add scripts/compare_regress.py docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
test: record what every rig and every evidence replay prints before the station kit

The station kit changes the one class every recorded figure was taken on, the
rig station, and several things the four rigs pass through: the three station
searches, the docking port, the docking component's release. Spec 6.7 asks that
the four rigs and every headless evidence replay come out line for line against
a pass taken before the first code change, on this tree and this build.

The verdicts of regress_evidence.py cannot be compared for that: nearly every
evidence log predates the World bounds line, so nearly every replay is CHANGED
in the base already and a second difference would not move a verdict. The
comparator added here compares the two passes' own logs instead, and it is
shown to find a doctored line before anything is allowed to depend on it.
MSG
git log -1 --format=%B | grep -ci co-authored   # 0
```

---

### Task 2: `sf.PlaceAsset`, строки отказа и старт корабля `ShipStart`

Стенду места нужен выбор ассета (§6.6): сегодня он читает один `DA_Place_OpenSpace` по жёсткому пути (`SFPlaceGameMode.cpp:67`). И нужен корабль, стартующий свободным, с человеком, поставленным от точки появления: без этого нельзя воспроизвести дефект `ASFTestStation` headless (§6.9, задача 3) и нельзя поставить корабль, которому некуда пристыковаться (N4). Задача идёт перед воспроизведением, потому что воспроизведению она и нужна (§9, пункт 2).

**Files:**
- Modify: `Source/StarForge/Public/World/SFPlaceDataAsset.h` (после `FSFPlaceCarrier`, строки 27–71; поле в `USFPlaceDataAsset`, после `Carriers`, строка 126)
- Modify: `Source/StarForge/Public/SFPlaceGameMode.h` (приватные поля, строки 70–90)
- Modify: `Source/StarForge/Private/SFPlaceGameMode.cpp` (объявления в безымянном пространстве, строки 20–118; `InitGame`, строки 152–168; `StartPlay`, строки 400–574)

**Interfaces:**
- Потребляет: `USFPlaceDataAsset`, `FSFPlaceCarrier`, `ASFTestShip::GetInteriorSpawnWorldLocation()`, `USFDockingComponent::DockImmediately()`, `StarForge::Units::MetresToCm/CmToMetres`, `LoadObject<T>`.
- Отдаёт задачам 3, 17, 18:
  - `UENUM(BlueprintType) enum class ESFShipStartMode : uint8 { Docked, Free };`
  - `USTRUCT(BlueprintType) struct FSFShipStart { ESFShipStartMode Mode; FVector LocationInPlaceMetres; FRotator Rotation; FVector OccupantOffsetShipMetres; };`
  - `USFPlaceDataAsset::ShipStart` (`FSFShipStart`, `EditAnywhere`)
  - консольная переменная `sf.PlaceAsset` (строковая, читается один раз в `InitGame`)
  - строки `Place rig: sf.PlaceAsset='…' … -- REFUSED before anything was measured; …` и `Place rig: place asset '…' REFUSED (…) -- NOTHING WAS ASSEMBLED. …`, обе `Error`
  - хвост строки `Place rig:` `the ship starts free at place-local (x y z) m, as the place asks`

- [ ] **Step 1: Увидеть, что сегодня имя ассета не значит ничего**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" "/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFPlaceGameMode" "-ini:Engine:[ConsoleVariables]:sf.PlaceAsset=DA_Place_Nope" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=10 -ExecCmds="sf.FlightLog 1" -log < /dev/null
grep -c "sf.PlaceAsset" $L          # >= 1: строка движка "deferred - dummy variable created"
grep -c "REFUSED" $L                # 0 -- ворота N7 проваливаются
grep -o "Place rig: scripted.*" $L  # place 'OpenSpace' read from DA_Place_OpenSpace -- собрано чужое место
```

Это «красный» шаг задачи: ворота N7 (§8.3) на сегодняшней сборке не выполняются.

- [ ] **Step 2: Поле старта в описании места**

`Source/StarForge/Public/World/SFPlaceDataAsset.h`, сразу после закрывающей `};` структуры `FSFPlaceCarrier` (строка 71):

```cpp
/**
 * How the player's ship begins on a place (station-kit spec 6.6).
 *
 * Docked is today's rig and the default, so every asset saved before this field existed reads
 * exactly as it did: a UDataAsset does not serialise what equals its class defaults, and the
 * default here is the old behaviour. Free exists for two controls that need a hull which is not
 * at a port: the latent-F1 reproductions, a hull standing against the rig station's wall
 * (N9, N9b), and the pier's all-ports-held control, a hull with nowhere to dock (N4).
 */
UENUM(BlueprintType)
enum class ESFShipStartMode : uint8
{
	Docked,
	Free
};

USTRUCT(BlueprintType)
struct STARFORGE_API FSFShipStart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ship start")
	ESFShipStartMode Mode = ESFShipStartMode::Docked;

	/** Free only: where the hull's pivot starts, in the PLACE's frame, in METRES. */
	UPROPERTY(EditAnywhere, Category = "Ship start")
	FVector LocationInPlaceMetres = FVector::ZeroVector;

	/** Free only. */
	UPROPERTY(EditAnywhere, Category = "Ship start")
	FRotator Rotation = FRotator::ZeroRotator;

	/**
	 * Where the man is stood, from the cabin's spawn point, in the SHIP's axes and METRES, in either
	 * mode. Zero by default. There is no headless walking in this project -- no sf.* moves the pawn
	 * -- so a control that needs him against a cabin wall has to put him there.
	 */
	UPROPERTY(EditAnywhere, Category = "Ship start")
	FVector OccupantOffsetShipMetres = FVector::ZeroVector;
};
```

В `USFPlaceDataAsset`, после `TArray<FSFPlaceCarrier> Carriers;` (строка 126):

```cpp
	/** Docked by default: see FSFShipStart. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	FSFShipStart ShipStart;
```

- [ ] **Step 3: Поля режима**

`Source/StarForge/Public/SFPlaceGameMode.h`, после `TArray<FSFPlaceCarrier> PlaceCarriers;` (строка 83):

```cpp
	/** How the player's ship begins, read with the rest of the place in InitGame. */
	UPROPERTY()
	FSFShipStart PlaceShipStart;

	/**
	 * Why sf.PlaceAsset refused the place, or empty. Held here and not in EPlaceRefusal because it
	 * is a fact about the engine's content, not about the size of a place -- the separation
	 * bApproachMeasured keeps beside EDockingRefusal.
	 */
	FString AssetRefusal;

	/** What sf.PlaceAsset asked for, printed back in both refusal lines. */
	FString RequestedAssetName;
```

- [ ] **Step 4: Переменная и путь**

`Source/StarForge/Private/SFPlaceGameMode.cpp`, в безымянное пространство, после объявления `CVarPlaceRadiusM` (строка 52):

```cpp
	/**
	 * Which place asset the rig reads, by its bare name in /Game/Places/ -- DA_Place_Berth, not a
	 * path. A bare name because MSYS rewrites an argument of the form x=/path, and a path mangled on
	 * the way in would refuse the place for a reason nobody typed. Empty keeps DA_Place_OpenSpace,
	 * so the rig's default does not move by a line (station-kit spec 6.6).
	 *
	 * Read ONCE, in InitGame, for sf.PlaceOffsetM's reason: set it with
	 * "-ini:Engine:[ConsoleVariables]:sf.PlaceAsset=DA_Place_Berth" on the command line.
	 */
	TAutoConsoleVariable<FString> CVarPlaceAsset(TEXT("sf.PlaceAsset"), TEXT(""),
		TEXT("The place asset the place rig reads, by its bare name in /Game/Places/ (DA_Place_Berth). Empty: DA_Place_OpenSpace.\n")
		TEXT("Read in InitGame, before -ExecCmds applies: set it with -ini on the command line."),
		ECVF_Default);
```

В `namespace PlaceRig`, после `AssetPath` (строка 67):

```cpp
		/** Where sf.PlaceAsset's bare names are looked for. */
		const TCHAR* PlaceAssetDirectory = TEXT("/Game/Places");
```

- [ ] **Step 5: Выбор ассета в `InitGame`**

После `PlaceCarriers.Reset();` (строка 157) и **перед** `bPlaceFromAsset = (PlaceData != nullptr);` (строка 159):

```cpp
	PlaceShipStart = FSFShipStart();
	AssetRefusal.Reset();
	RequestedAssetName = CVarPlaceAsset.GetValueOnGameThread();

	if (!RequestedAssetName.IsEmpty())
	{
		// A name, not a path. A slash means the value was meant as a path, which is exactly what
		// this variable refuses to carry; refused rather than trimmed, because a trimmed value
		// names a place nobody asked for.
		if (RequestedAssetName.Contains(TEXT("/")) || RequestedAssetName.Contains(TEXT("\\")))
		{
			AssetRefusal = TEXT("not a bare asset name");
		}
		else
		{
			const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"),
				PlaceRig::PlaceAssetDirectory, *RequestedAssetName, *RequestedAssetName);
			if (USFPlaceDataAsset* Named = LoadObject<USFPlaceDataAsset>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn))
			{
				PlaceData = Named;
			}
			else
			{
				AssetRefusal = TEXT("no such asset");
			}
		}
	}

	if (!AssetRefusal.IsEmpty())
	{
		// Refused before anything is measured, and nothing is put in its stead. The code-written
		// place and DA_Place_OpenSpace are other places, and their figures, assembled under this
		// name, would be credited to the asset that was asked for. No size line either: there is
		// nothing to measure, and a size line here would describe the wrong place.
		bPlaceMeasured = true;
		bPlaceFits = false;
		bPlaceFromAsset = false;

		const FString What = (AssetRefusal == TEXT("no such asset"))
			? FString::Printf(TEXT("names no place asset at %s/%s"), PlaceRig::PlaceAssetDirectory, *RequestedAssetName)
			: FString(TEXT("is not a bare asset name"));

		UE_LOG(LogStarForge, Error,
			TEXT("Place rig: sf.PlaceAsset='%s' %s -- REFUSED before anything was measured; nothing will be ")
			TEXT("assembled and the pawn class is withdrawn."),
			*RequestedAssetName, *What);
		return;
	}
```

И в ветке `if (bPlaceFromAsset)` (строки 160–168), после `PlaceCarriers = PlaceData->Carriers;`:

```cpp
		PlaceShipStart = PlaceData->ShipStart;
```

Отзыв класса пешки работает без правки: `GetDefaultPawnClassForController_Implementation` (строки 277–289) отвечает `nullptr` при `bPlaceMeasured && !bPlaceFits`.

- [ ] **Step 6: Старт корабля и человека в `StartPlay`**

Заменить блок от `bool bDocked = false;` (строка 400) до конца `if (bPlaceFits) { … }` с кораблём (строка 443) на:

```cpp
	// Hoisted: the free start is measured from it, and the rig line reads the ship back against it.
	const FVector PlaceOriginCm(
		StarForge::Units::MetresToCm(PlaceOriginMetres.X),
		StarForge::Units::MetresToCm(PlaceOriginMetres.Y),
		StarForge::Units::MetresToCm(PlaceOriginMetres.Z));
	const bool bStartsFree = (PlaceShipStart.Mode == ESFShipStartMode::Free);

	bool bDocked = false;
	if (bPlaceFits)
	{
		FActorSpawnParameters ShipParams;
		ShipParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ShipParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		ShipParams.Name = TEXT("Ship");

		// Spawned at the place's origin and immediately moved by the docking maths. The docked
		// pose is not authored here for the loop rig's reason: it is derived from the two ports,
		// so moving a wall or a port moves the docked ship with it, and a constant would
		// silently stop tracking them. A place that asks for a free start (station-kit spec 6.6)
		// gets the pose it wrote, and nothing docks the ship.
		const FTransform ShipSpawn = bStartsFree
			? FTransform(PlaceShipStart.Rotation, PlaceOriginCm + FVector(
				StarForge::Units::MetresToCm(PlaceShipStart.LocationInPlaceMetres.X),
				StarForge::Units::MetresToCm(PlaceShipStart.LocationInPlaceMetres.Y),
				StarForge::Units::MetresToCm(PlaceShipStart.LocationInPlaceMetres.Z)))
			: FTransform(PlaceOriginCm);

		Ship = World->SpawnActor<ASFTestShip>(ASFTestShip::StaticClass(), ShipSpawn, ShipParams);

		// Before Super::StartPlay, exactly as on the loop rig: Super is what begins play on the
		// world and spawns the pawn, and docking afterwards would put the pawn in the world while
		// the ship still sat at the place's origin inside the station's interior volume, with
		// nothing deciding which of the two claimed him.
		if (Ship && Ship->GetDocking())
		{
			if (!bStartsFree)
			{
				Ship->GetDocking()->DockImmediately();

				// Asked of the component afterwards rather than assumed from the call: docking
				// immediately still has to resolve two ports, and a station that failed to spawn
				// leaves it Free with the ship adrift at the place's origin.
				bDocked = Ship->GetDocking()->IsDocked();
				if (!bDocked)
				{
					UE_LOG(LogStarForge, Error,
						TEXT("Place rig: the ship did not dock and is adrift at the origin of the place."));
				}
			}
		}
		else
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Place rig: no ship or no docking component; the rig cannot start docked."));
		}
	}
```

В ветке отказа (строки 447–478) заменить последний `UE_LOG(LogStarForge, Error, TEXT("Place rig: place '%s' REFUSED (%s) -- NOTHING WAS ASSEMBLED. …` на:

```cpp
		if (!AssetRefusal.IsEmpty())
		{
			// Its own line, because the size refusal's names the place and its size reason, and here
			// there is neither: the place is OpenSpace only by default, and "none" is not a reason.
			// The size refusal's line below stays as it was to the character -- the size controls of
			// stage 5 and their evidence logs stand on it.
			UE_LOG(LogStarForge, Error,
				TEXT("Place rig: place asset '%s' REFUSED (%s) -- NOTHING WAS ASSEMBLED. No carriers, no ship, ")
				TEXT("and %s."),
				*RequestedAssetName, *AssetRefusal, *PawnText);
			return;
		}

		UE_LOG(LogStarForge, Error,
			TEXT("Place rig: place '%s' REFUSED (%s) -- NOTHING WAS ASSEMBLED. No carriers, no ship, ")
			TEXT("and %s."),
			*PlaceName.ToString(), StarForge::Place::RefusalName(PlaceFit.Refusal), *PawnText);
		return;
```

Точку, куда ставят человека (строка 493), заменить:

```cpp
				FVector Cabin = Ship->GetInteriorSpawnWorldLocation();
				if (!PlaceShipStart.OccupantOffsetShipMetres.IsZero())
				{
					// In the ship's axes, so "towards the hatch wall" means the same whatever way the
					// hull is turned. Added only when asked for: a zero added to a coordinate turns a
					// -0.0 into +0.0, and the docked rig must stand him on the very point it did.
					Cabin += Ship->GetActorQuat().RotateVector(FVector(
						StarForge::Units::MetresToCm(PlaceShipStart.OccupantOffsetShipMetres.X),
						StarForge::Units::MetresToCm(PlaceShipStart.OccupantOffsetShipMetres.Y),
						StarForge::Units::MetresToCm(PlaceShipStart.OccupantOffsetShipMetres.Z)));
				}
```

Строку стенда (строки 551–573) заменить от `const FString Report =` до конца функции:

```cpp
	// Read back from the hull, not from the asset: a ship asked to start somewhere and standing
	// somewhere else is exactly what this line exists to show.
	FString ShipText = bDocked ? FString(TEXT("spawns docked")) : FString(TEXT("is ADRIFT"));
	if (bStartsFree && Ship)
	{
		const FVector AtCm = Ship->GetActorLocation() - PlaceOriginCm;
		ShipText = FString::Printf(TEXT("starts free at place-local (%.1f %.1f %.1f) m, as the place asks"),
			StarForge::Units::CmToMetres(AtCm.X), StarForge::Units::CmToMetres(AtCm.Y),
			StarForge::Units::CmToMetres(AtCm.Z));
	}
	const bool bShipWrong = bStartsFree ? (Ship == nullptr) : !bDocked;

	// The startup line the runbook sends the operator to look for, and every state in it was read
	// back rather than attempted. A rig which cannot verify its own setup is not an instrument.
	const FString Report = FString::Printf(
		TEXT("Place rig: scripted profile %s; place '%s' read from %s and %s; the ship %s; the man %s."),
		bProfileOff ? TEXT("off") : TEXT("STILL ON"),
		*PlaceName.ToString(),
		bPlaceFromAsset ? *PlaceData->GetName() : TEXT("a place written in code"),
		*CarriersText,
		*ShipText,
		*ManText);

	if (!bProfileOff || bCarriersShort || bShipWrong || !bPawnInCabin || !bPawnBound)
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Report);
	}
	else if (!bPlaceFromAsset)
	{
		// Asked for by circumstance rather than broken, the way sf.Pirate 0 is on the combat rig:
		// the world is the one the data describes, but the data is not yet an asset anyone edited.
		UE_LOG(LogStarForge, Warning, TEXT("%s"), *Report);
	}
	else
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *Report);
	}
}
```

- [ ] **Step 7: Собрать и прогнать N7 и два его соседа**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
```

Ноль предупреждений, ноль ошибок. Затем, объявив `kit()` из раздела «Как проверяется»:

```bash
kit DA_Place_Nope 60 10 "sf.FlightLog 1" task2-n7-60
grep -F "Place rig: sf.PlaceAsset='DA_Place_Nope' names no place asset at /Game/Places/DA_Place_Nope -- REFUSED before anything was measured; nothing will be assembled and the pawn class is withdrawn." $L | wc -l   # 1
grep -F "Place rig: place asset 'DA_Place_Nope' REFUSED (no such asset) -- NOTHING WAS ASSEMBLED. No carriers, no ship, and no pawn:" $L | wc -l   # 1
for p in "Place size:" "Place carrier:" "Dock:" "Shipboard drift" "Frame \["; do printf "%s %s\n" "$(grep -c "$p" $L)" "$p"; done   # все 0

kit Places/DA_Place_OpenSpace 60 10 "sf.FlightLog 1" task2-slash-60
grep -c "is not a bare asset name -- REFUSED before anything was measured" $L   # 1
grep -c "REFUSED (not a bare asset name) -- NOTHING WAS ASSEMBLED" $L           # 1

kit DA_Place_OpenSpace 60 25 "sf.FlightLog 1" task2-named-default-60
grep -o "Place rig: scripted.*" $L   # ... place 'OpenSpace' read from DA_Place_OpenSpace and assembled with 1 carrier; the ship spawns docked; ...
```

Журналы задачи 2 — не улики этапа, а проверка задачи: после шага удалить их из `docs/superpowers/notes/evidence/` (`rm docs/superpowers/notes/evidence/$(date +%F)-task2-*.log`), иначе проход регрессии примет их за опорные. Улики N7 снимает задача 12.

Свободный старт на этой задаче не проверяется прогоном: ассета со свободным стартом ещё нет. Его первый прогон — воспроизведение в задаче 3, и строка `the ship starts free at place-local (5.4 -11.1 0.0) m` там — часть ворот.

- [ ] **Step 8: Регрессия `kit-1.2`**

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-1.2
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-1.2; echo "exit $?"
```

Ожидается `compare: 0 findings`, `exit 0`: ни один повтор стенда места не сдвинулся ни строкой — ни одно поле ассета без нового поля не читается иначе, и без `sf.PlaceAsset` путь данных прежний. Дописать строку в таблицу регрессий бланка базы.

- [ ] **Step 9: Коммит**

```bash
git status --short
git add Source/StarForge/Public/World/SFPlaceDataAsset.h Source/StarForge/Public/SFPlaceGameMode.h Source/StarForge/Private/SFPlaceGameMode.cpp docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
feat(place): choose the place asset by name, and let a place start the ship free

The place rig read one asset by one hard path. The station kit needs several
places on the same rig, so sf.PlaceAsset names one by its bare name in
/Game/Places -- a bare name because MSYS rewrites x=/path on the way in. Empty
keeps DA_Place_OpenSpace, so the rig's default does not move by a line.

A name with no asset behind it refuses the place outright, with its own two
Error lines, and assembles nothing. Falling back to the default place would put
another station's figures under the name that was asked for.

ShipStart lets a place start the ship free and stand the man at an offset in the
ship's axes. The latent-F1 reproduction of the rig station needs a hull against
the station's wall and a man against his cabin wall, and there is no headless
walking in this project to put him there.
MSG
```

---

### Task 3: Сценарий ассетов, два ассета воспроизведения и N9, N9b **до** исправления

Дефект объёма `ASFTestStation` (§6.9) выведен из чисел конструктора, но прогоном не воспроизводился ни разу. Исправление без воспроизведения доказывает только, что после него передачи нет, — но её могло не быть и до. Поэтому воспроизведение снимается **до** исправления, на обеих частотах, и служит ему отрицательным контролем.

**Files:**
- Create: `scripts/station_kit_places.py`
- Create: `Content/Places/DA_Place_LatentF1Side.uasset`, `Content/Places/DA_Place_LatentF1Roof.uasset`
- Create: `docs/superpowers/notes/evidence/<дата>-latent-f1-{side,roof}-{60,400}-BEFORE-FIX.log`

**Interfaces:**
- Потребляет из задачи 2: `FSFShipStart`, `ESFShipStartMode`, `USFPlaceDataAsset::ShipStart`, `sf.PlaceAsset`.
- Отдаёт задачам 8, 13, 15, 17, 18: сценарий с группами `probe`, `latent`, `berth`, `piercore`, `tie`, `parked0`, `parkedboth`, `badrecords`, `read`; запуск —

```bash
KIT_PLACES_GROUP=<группа> "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET\|Python script executed" Saved/Logs/StarForge.log
```

Группа идёт переменной окружения, а не аргументом: коммандлет режет значение `-Script=` на первом пробеле (таблица «Что проверено в исходниках движка»).

- [ ] **Step 1: Написать сценарий**

`scripts/station_kit_places.py`:

```python
# -*- coding: utf-8 -*-
"""Create, update and read back the station kit's place assets (station-kit plan, tasks 3, 8, 13, 15, 17, 18),
and the one control asset of the ParkedHulls record check (task 17).

Run by the editor's own Python through the commandlet, never by a system Python:

    KIT_PLACES_GROUP=latent UnrealEditor-Cmd.exe <worktree>/StarForge.uproject -run=pythonscript \
        -script="<worktree>/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log

The group comes in an environment variable because the commandlet cuts -Script= at the first space
when the value is not quoted, and from Git Bash it never arrives quoted
(PythonScriptCommandlet.cpp 20-33).

Groups: probe, latent, berth, piercore, tie, parked0, parkedboth, badrecords, read. Every asset is written with
only the fields its place needs; the rest keep their class defaults, and a UDataAsset does not
serialise what equals its defaults -- so an asset made here reads, field for field, like one made by
hand with the same values. Every write is saved and read back, and the read-back is the line to
check, not the write.
"""
import os
import unreal

PLACES = "/Game/Places"


def cls(name):
    c = unreal.load_class(None, f"/Script/StarForge.{name}")
    if c is None:
        raise RuntimeError(f"no class /Script/StarForge.{name}: is the editor target of this tree built?")
    return c


def vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]))


def carrier(class_name, name, loc=(0.0, 0.0, 0.0)):
    c = unreal.SFPlaceCarrier()
    c.set_editor_property("carrier_class", cls(class_name))
    c.set_editor_property("carrier_name", name)
    c.set_editor_property("location_in_place_metres", vec(loc))
    c.set_editor_property("rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    return c


def free_start(loc, occupant=(0.0, 0.0, 0.0)):
    s = unreal.SFShipStart()
    s.set_editor_property("mode", unreal.SFShipStartMode.FREE)
    s.set_editor_property("location_in_place_metres", vec(loc))
    s.set_editor_property("rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    s.set_editor_property("occupant_offset_ship_metres", vec(occupant))
    return s


def parked(hull, station, port):
    p = unreal.SFParkedHull()
    p.set_editor_property("hull_name", hull)
    p.set_editor_property("station_carrier_name", station)
    p.set_editor_property("port_index", int(port))
    return p


def make(asset_name, place_name, carriers, ship_start=None, parked_hulls=None):
    path = f"{PLACES}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls("SFPlaceDataAsset"))
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name, PLACES, cls("SFPlaceDataAsset"), factory)
    if asset is None:
        raise RuntimeError(f"{path} could not be created or loaded")
    asset.set_editor_property("place_name", place_name)
    asset.set_editor_property("carriers", list(carriers))
    if ship_start is not None:
        asset.set_editor_property("ship_start", ship_start)
    if parked_hulls is not None:
        asset.set_editor_property("parked_hulls", list(parked_hulls))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"{path} was not saved")
    unreal.log(f"KIT-ASSET made {path}")
    read(asset_name)


def fmt(v):
    return f"({v.x:.2f} {v.y:.2f} {v.z:.2f})"


def read(asset_name):
    path = f"{PLACES}/{asset_name}"
    asset = unreal.load_asset(path)
    if asset is None:
        unreal.log_error(f"KIT-ASSET read {path}: MISSING")
        return
    carriers = []
    for c in asset.get_editor_property("carriers"):
        k = c.get_editor_property("carrier_class")
        carriers.append(f"{k.get_name() if k else 'NO CLASS'} '{c.get_editor_property('carrier_name')}' "
                        f"at {fmt(c.get_editor_property('location_in_place_metres'))}")
    text = f"KIT-ASSET read {path}: place '{asset.get_editor_property('place_name')}', carriers [{'; '.join(carriers)}]"
    s = asset.get_editor_property("ship_start")
    text += (f", ship start {s.get_editor_property('mode')} at {fmt(s.get_editor_property('location_in_place_metres'))}"
             f", occupant {fmt(s.get_editor_property('occupant_offset_ship_metres'))}")
    try:
        hulls = asset.get_editor_property("parked_hulls")
        text += ", parked [" + "; ".join(
            f"'{h.get_editor_property('hull_name')}' at {h.get_editor_property('station_carrier_name')}"
            f".{h.get_editor_property('port_index')}" for h in hulls) + "]"
    except Exception:  # the field arrives with plan task 17
        text += ", parked (no such field yet)"
    unreal.log(text)


def probe():
    exists = unreal.EditorAssetLibrary.does_asset_exist(f"{PLACES}/DA_Place_OpenSpace")
    cls("SFPlaceDataAsset")
    unreal.log(f"KIT-ASSET probe: editor python ok, SFPlaceDataAsset found, DA_Place_OpenSpace exists {exists}")
    if not exists:
        raise RuntimeError("DA_Place_OpenSpace not found: the asset library cannot see /Game/Places")


# Spec 6.9: the rig station at the place's origin, the ship free with its +Y (hatch) side towards the
# station's -Y wall, the man 3.2 m towards the hatch wall and 1.0 m aside, in the ship's axes.
LATENT_OCCUPANT = (-1.0, 3.2, 0.0)

GROUPS = {
    "probe": lambda: probe(),
    "latent": lambda: (
        make("DA_Place_LatentF1Side", "LatentF1Side", [carrier("SFTestStation", "Station")],
             ship_start=free_start((5.40, -11.10, 0.00), LATENT_OCCUPANT)),
        make("DA_Place_LatentF1Roof", "LatentF1Roof", [carrier("SFTestStation", "Station")],
             ship_start=free_start((0.00, -11.10, 3.00), LATENT_OCCUPANT))),
    "berth": lambda: make("DA_Place_Berth", "Berth", [carrier("SFBerthStation", "Station")]),
    "piercore": lambda: make("DA_Place_PierCore", "PierCore", [carrier("SFPierCoreStation", "Station")]),
    # Spec 7.5: the mid-plane between Port0 and Port1 through the place's origin -- one number in the data.
    "tie": lambda: make("DA_Place_PierCoreTie", "PierCoreTie",
                        [carrier("SFPierCoreStation", "Station", (-30.0, 0.0, 0.0))]),
    "parked0": lambda: make("DA_Place_PierCoreParked0", "PierCoreParked0",
                            [carrier("SFPierCoreStation", "Station")],
                            parked_hulls=[parked("Parked", "Station", 0)]),
    # Spec 8.3, N4: both ports held, and the ship free at (0, -40, 0) m, 9.9 m clear of Parked0's hull.
    "parkedboth": lambda: make("DA_Place_PierCoreParkedBoth", "PierCoreParkedBoth",
                               [carrier("SFPierCoreStation", "Station")],
                               ship_start=free_start((0.0, -40.0, 0.0)),
                               parked_hulls=[parked("Parked0", "Station", 0), parked("Parked1", "Station", 1)]),
    # Plan task 17: the check of a ParkedHulls record, run rather than trusted. Record 0 is sound and must
    # be the only hull that spawns; each record after it fails exactly one check, in the order the rig
    # makes them -- no name, 'Ship', a carrier's name, an earlier record's name, no such carrier, no such
    # port, a port already held. A control, not a place of the kit: its run never goes into evidence.
    "badrecords": lambda: make("DA_Place_PierCoreBadRecords", "PierCoreBadRecords",
                               [carrier("SFPierCoreStation", "Station")],
                               parked_hulls=[parked("Parked0", "Station", 0),
                                             parked("", "Station", 1),
                                             parked("Ship", "Station", 1),
                                             parked("Station", "Station", 1),
                                             parked("Parked0", "Station", 1),
                                             parked("X", "Nope", 1),
                                             parked("Y", "Station", 5),
                                             parked("Parked1", "Station", 0)]),
    "read": lambda: [read(n) for n in ("DA_Place_OpenSpace", "DA_Place_LatentF1Side", "DA_Place_LatentF1Roof",
                                        "DA_Place_Berth", "DA_Place_PierCore", "DA_Place_PierCoreTie",
                                        "DA_Place_PierCoreParked0", "DA_Place_PierCoreParkedBoth",
                                        "DA_Place_PierCoreBadRecords")],
}

group = os.environ.get("KIT_PLACES_GROUP", "")
if group not in GROUPS:
    raise RuntimeError(f"KIT_PLACES_GROUP='{group}' is none of {sorted(GROUPS)}")
GROUPS[group]()
unreal.log(f"KIT-ASSET group '{group}' done")
```

- [ ] **Step 2: Пробой коммандлета**

Редактор этого дерева закрыт.

```bash
KIT_PLACES_GROUP=probe "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET\|Python script executed\|Unknown commandlet" Saved/Logs/StarForge.log
```

Ворота: `KIT-ASSET probe: editor python ok, SFPlaceDataAsset found, DA_Place_OpenSpace exists True` и `Python script executed successfully`. Иначе — **обход руками** (единственное место плана, где нужен человек в редакторе, и только на этот случай):

```
Открыть редактор этого дерева:
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject"
Window → Output Log; в строке ввода внизу слева выбрать Cmd → Python
Ввести: import os; os.environ["KIT_PLACES_GROUP"] = "latent"
Ввести: py "F:/UEProject/StarForge-station/scripts/station_kit_places.py"
Проверить в Output Log строки KIT-ASSET made и KIT-ASSET read для обоих ассетов
File → Save All; закрыть редактор
```

Если не работает и это: Content Browser → папка `Places` → Add → Miscellaneous → Data Asset → класс `SFPlaceDataAsset` → имя `DA_Place_LatentF1Side`; открыть; `PlaceName` = `LatentF1Side`; `Carriers` + → `CarrierClass` = `SFTestStation`, `CarrierName` = `Station`; `ShipStart` → `Mode` = `Free`, `LocationInPlaceMetres` = (5.4, −11.1, 0.0), `OccupantOffsetShipMetres` = (−1.0, 3.2, 0.0); **Ctrl+S**, звёздочка на вкладке исчезла. То же для `DA_Place_LatentF1Roof` с `PlaceName` `LatentF1Roof` и `LocationInPlaceMetres` (0.0, −11.1, 3.0).

- [ ] **Step 3: Создать два ассета и прочитать их обратно**

```bash
KIT_PLACES_GROUP=latent "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET\|Python script executed" Saved/Logs/StarForge.log
ls -la Content/Places/DA_Place_LatentF1Side.uasset Content/Places/DA_Place_LatentF1Roof.uasset
```

Ожидается (строки `LogPython`):

```
KIT-ASSET read /Game/Places/DA_Place_LatentF1Side: place 'LatentF1Side', carriers [SFTestStation 'Station' at (0.00 0.00 0.00)], ship start SFShipStartMode.FREE at (5.40 -11.10 0.00), occupant (-1.00 3.20 0.00), parked (no such field yet)
KIT-ASSET read /Game/Places/DA_Place_LatentF1Roof: place 'LatentF1Roof', carriers [SFTestStation 'Station' at (0.00 0.00 0.00)], ship start SFShipStartMode.FREE at (0.00 -11.10 3.00), occupant (-1.00 3.20 0.00), parked (no such field yet)
```

Точный вид имени перечисления (`SFShipStartMode.FREE` или `<SFShipStartMode.FREE: 1>`) даёт версия Python-привязки; ворота — слово `FREE` и числа. Оба файла больше 1500 байт.

- [ ] **Step 4: N9 и N9b до исправления, на 60 и 400**

Команда N9 — ровно та, что в §6.9; `kit()` даёт её же (§6.9 называет журналы датой 2026-09-28, здесь дата — день прогона):

```bash
kit DA_Place_LatentF1Side 60 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-side-60-BEFORE-FIX
kit DA_Place_LatentF1Side 400 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-side-400-BEFORE-FIX
kit DA_Place_LatentF1Roof 60 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-roof-60-BEFORE-FIX
kit DA_Place_LatentF1Roof 400 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-roof-400-BEFORE-FIX
```

На каждом из четырёх журналов:

```bash
F=docs/superpowers/notes/evidence/$(date +%F)-latent-f1-side-60-BEFORE-FIX.log   # и три остальных
grep -F "the ship starts free at place-local (5.4 -11.1 0.0) m, as the place asks" "$F" | wc -l   # 1 (у Roof: (0.0 -11.1 3.0))
awk '/Flight: \[Ship\]/{f++} /Frame handover: Ship -> Station/{ if (!h) { h=1; print (f>=1 && f<=2) ? "IN WINDOW after flight line " f : "OUT OF WINDOW after flight line " f } } END{ if (!h) print "NO HANDOVER" }' "$F"
grep -n "Frame handover: Ship -> Station" "$F" | head -1
```

**Ворота — `IN WINDOW` на всех четырёх** (§6.9): передача `Ship -> Station` стоит между первой и третьей строками `Flight: [Ship]`. В бланк переписываются строка передачи и номер кадра из её префикса. Всё, что следует дальше (`Standing on Ship's floor while bound to Station's frame`, увод, строки переноса), — улика, а не ворота.

**Если на каком-то журнале `NO HANDOVER` или `OUT OF WINDOW` — план останавливается.** Воспроизведение не состоялось или доказало другой механизм (до первой строки полёта — стартовая передача; после третьей — контакт у стены), исправлению нечего доказывать. Записать находку и разбирать, а не исправлять вслепую.

Журналы помечены `BEFORE-FIX`, и `regress_evidence.py` не возьмёт их опорными (`NOT_A_BASELINE`, `scripts/regress_evidence.py:57`).

- [ ] **Step 5: Коммит**

```bash
git status --short
git add scripts/station_kit_places.py Content/Places/DA_Place_LatentF1Side.uasset Content/Places/DA_Place_LatentF1Roof.uasset docs/superpowers/notes/evidence/*-latent-f1-*-BEFORE-FIX.log
git diff --cached --name-only
git commit -F - <<'MSG'
test(station): reproduce the rig station's latent F1 volume before fixing it

The rig station's interior volume is one box for the room and the tunnel, and
outside the -Y wall it reaches 4.7 m past each tunnel wall and 5.1 m above the
tunnel roof, into space where a ship can stand. A hull flush against that wall
has the last 1.8 m of its cabin inside the station's volume, and two carriers at
rest hand a man over on relative speed alone. That was derived from the
constructor's numbers and never seen in a run.

A fix without a reproduction proves only that the handover is absent after it,
which it may have been before. So the two cases -- beside the tunnel and above
its roof -- are reproduced headless at 60 and 400 fps first, with the handover
required inside the window the geometry predicts, between the first and third
flight lines. The logs are marked BEFORE-FIX so the evidence replay never takes
them for baselines.

The place assets are written by the editor's own Python and read back, so what
the run was given is on record.
MSG
```

---

### Task 4: Объём `ASFTestStation` двумя коробками и эпизод отказа на носитель

Исправление по решению 10: две коробки, выведенные из тех же констант, что и панели (§6.9). Вместе с ним — эпизод отказа передачи **на носитель**, а не на коробку (§6.7, п. 8): иначе строка `Frame handover REFUSED` обещала бы молчать «until he leaves Station's volume» и нарушала бы обещание, стоит человеку перейти из тоннеля в комнату.

**Files:**
- Modify: `Source/StarForge/Private/Ship/SFTestStation.cpp` (объём, строки 144–166)
- Modify: `Source/StarForge/Public/Ship/SFTestStation.h` (getter `GetInterior`, строки 47–48; поля, строки 72–73)
- Modify: `Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp` (`TryHandover`, строки 137–181)
- Modify: `Source/StarForge/Public/Ship/SFInteriorVolumeComponent.h` (приватные методы, после строки 88)

**Interfaces:**
- Потребляет: константы конструктора `Station::HalfXCm/HalfYCm/HalfZCm/PanelThickness/CubeEdgeCm/HatchHalfWidthCm/TunnelLengthCm`, локальные `DeckTopZ`, `HatchTopZ` (строки 99–100).
- Отдаёт: `ASFTestStation` с двумя объёмами `Interior` (комната) и `TunnelInterior` (тоннель); `USFInteriorVolumeComponent` с эпизодом отказа на владельца. Getter `GetInterior()` удалён (его не зовёт никто).

- [ ] **Step 1: «Красный» шаг уже снят**

Журналы `*-latent-f1-*-BEFORE-FIX.log` задачи 3 — это провалившийся тест исправления: передача `Ship -> Station` есть.

- [ ] **Step 2: Две коробки**

`Source/StarForge/Private/Ship/SFTestStation.cpp`, заменить строки 144–166 (комментарий «The box plus the tunnel…», `InteriorHalfYCm`, `InteriorCentreYCm` и создание `Interior`) на:

```cpp
	// Two boxes, the room and the tunnel. Until 2026-09-28 it was one, from the tunnel's mouth at
	// -600 to the far wall at +400 and as wide and as tall as the room, on the argument that "the
	// extra is inside solid panels, where a pawn cannot be". For the room that was true; for the
	// tunnel it was not. Outside the -Y wall the one box reached 4.7 m past each tunnel wall and
	// 5.1 m above the tunnel's roof, into open space where a ship can stand -- and a hull flush
	// against the -Y wall beside the tunnel, or above its roof, had the last 1.8 m of its cabin
	// inside the station's volume. Two carriers at rest hand a man over on relative speed alone
	// (USFInteriorVolumeComponent::TryHandover), so a man walking to that end of his cabin was bound
	// to the station while standing on his own ship: finding F1 without a hull inside the station.
	// Derived from these constants in station-kit spec 6.9 and reproduced headless before this fix
	// at 60 and 400 fps (evidence/*-latent-f1-{side,roof}-{60,400}-BEFORE-FIX.log).
	//
	// Both boxes come from the constants the panels come from, so a moved wall moves them too:
	//   - the room: out to the walls' inner faces, half-extent 590 x 390 x 390 about the root;
	//   - the tunnel: its bore, X +-HatchHalfWidthCm, Z from the deck's top to the hatch top, Y from
	//     the port plane at -600 to the -Y wall's inner face at -390. The opening in that wall
	//     (-410..-390) belongs to the tunnel, as the door in a kit hall's wall belongs to its boom.
	//
	// The tunnel box still begins on the port plane, so a docked ship's cabin still ends ten
	// centimetres short of it (-610 against -600): ten centimetres of wall, deliberately unclaimed,
	// and a man walking out through the hatch enters the station's volume on the same plane and in
	// the same frame as before. Two volumes of one carrier do not argue: entering either binds to
	// this frame, a move between them is no handover, and a refused episode is one per carrier.
	const double WallInnerOffsetCm = Station::PanelThickness * Station::CubeEdgeCm * 0.5;     // 10

	Interior = CreateDefaultSubobject<USFInteriorVolumeComponent>(TEXT("Interior"));
	Interior->SetupAttachment(Root);
	Interior->SetRelativeLocation(FVector::ZeroVector);
	Interior->SetBoxExtent(FVector(
		Station::HalfXCm - WallInnerOffsetCm,                                                 // 590
		Station::HalfYCm - WallInnerOffsetCm,                                                 // 390
		Station::HalfZCm - WallInnerOffsetCm));                                               // 390

	const double TunnelMouthYCm = -Station::HalfYCm - Station::TunnelLengthCm;                  // -600
	const double TunnelInnerEndYCm = -Station::HalfYCm + WallInnerOffsetCm;                     // -390

	TunnelInterior = CreateDefaultSubobject<USFInteriorVolumeComponent>(TEXT("TunnelInterior"));
	TunnelInterior->SetupAttachment(Root);
	TunnelInterior->SetRelativeLocation(FVector(
		0.0,
		(TunnelMouthYCm + TunnelInnerEndYCm) * 0.5,                                           // -495
		(DeckTopZ + HatchTopZ) * 0.5));                                                       // -265
	TunnelInterior->SetBoxExtent(FVector(
		Station::HatchHalfWidthCm,                                                            // 100
		(TunnelInnerEndYCm - TunnelMouthYCm) * 0.5,                                           // 105
		(HatchTopZ - DeckTopZ) * 0.5));                                                       // 125
```

`Source/StarForge/Public/Ship/SFTestStation.h`: удалить строки 47–48 (`/** The box, the tunnel … */` и `GetInterior()`); после поля `Interior` (строки 72–73) добавить:

```cpp
	/** The tunnel's bore, from the port plane to the -Y wall's inner face. See the constructor. */
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFInteriorVolumeComponent> TunnelInterior;
```

- [ ] **Step 3: Эпизод отказа на носитель**

`Source/StarForge/Public/Ship/SFInteriorVolumeComponent.h`, после объявления `TryHandover` (строка 88):

```cpp
	/**
	 * Whether another interior volume of this volume's owner still holds Character in an open refused
	 * episode. The REFUSED line promises silence "until he leaves <carrier>'s volume", naming the
	 * carrier and not the box, so a carrier built of several boxes says it once for all of them
	 * (station-kit spec 6.4, 6.7 item 8).
	 */
	bool IsRefusedByAnotherVolumeOfThisCarrier(const ASFCharacter* Character) const;

	/** An accepted handover closes the episode on every volume of the owner, not only this one. */
	void ForgetRefusalOnEveryVolumeOfThisCarrier(const ASFCharacter* Character);
```

`Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp`, в `TryHandover` заменить блок строк 162–174 (от `if (!RefusedOccupants.ContainsByPredicate(IsCharacter))` до закрывающей скобки перед `return;`) на:

```cpp
			if (!RefusedOccupants.ContainsByPredicate(IsCharacter))
			{
				RefusedOccupants.Add(Character);
				SetComponentTickEnabled(true);

				// One episode per carrier: a man refused in the tunnel and walking on into the room has
				// not left the station's volume, and the line below promised silence until he does.
				// Added to this box's list all the same, so the retry every tick goes on as before.
				if (!IsRefusedByAnotherVolumeOfThisCarrier(Character))
				{
					const FString FromName = From->GetOwner() ? From->GetOwner()->GetActorNameOrLabel() : FString(TEXT("none"));
					const FString ToName = To->GetOwner() ? To->GetOwner()->GetActorNameOrLabel() : FString(TEXT("none"));
					UE_LOG(LogStarForge, Warning,
						TEXT("Frame handover REFUSED: %s -> %s, the two carriers move %.2f m/s relative to each other where he ")
						TEXT("stands, over the %.1f m/s limit. He stays bound to %s; said once until he leaves %s's volume or the ")
						TEXT("two come within the limit."),
						*FromName, *ToName, RelativeSpeed, InteriorHandover::MaxRelativeSpeedMetresPerSec, *FromName, *ToName);
				}
			}
			return;
```

и заменить строку 179 `RefusedOccupants.RemoveAll(IsCharacter);` на:

```cpp
	ForgetRefusalOnEveryVolumeOfThisCarrier(Character);
```

Добавить в конец файла:

```cpp
bool USFInteriorVolumeComponent::IsRefusedByAnotherVolumeOfThisCarrier(const ASFCharacter* Character) const
{
	TInlineComponentArray<USFInteriorVolumeComponent*> Volumes(GetOwner());
	for (const USFInteriorVolumeComponent* Volume : Volumes)
	{
		if (Volume && Volume != this && Volume->RefusedOccupants.ContainsByPredicate(
			[Character](const TWeakObjectPtr<ASFCharacter>& Entry) { return Entry.Get() == Character; }))
		{
			return true;
		}
	}
	return false;
}

void USFInteriorVolumeComponent::ForgetRefusalOnEveryVolumeOfThisCarrier(const ASFCharacter* Character)
{
	// Every box, or a neighbour would go on retrying a handover that has already happened. Their
	// ticks switch themselves off on the next pass once their lists are empty.
	TInlineComponentArray<USFInteriorVolumeComponent*> Volumes(GetOwner());
	for (USFInteriorVolumeComponent* Volume : Volumes)
	{
		if (Volume)
		{
			Volume->RefusedOccupants.RemoveAll(
				[Character](const TWeakObjectPtr<ASFCharacter>& Entry) { return Entry.Get() == Character; });
		}
	}
}
```

Текст строки `REFUSED` не изменён ни символом. У носителя с одним объёмом (корабль) поведение прежнее по построению.

- [ ] **Step 4: Собрать, N9 и N9b после исправления**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
kit DA_Place_LatentF1Side 60 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-side-60
kit DA_Place_LatentF1Side 400 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-side-400
kit DA_Place_LatentF1Roof 60 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-roof-60
kit DA_Place_LatentF1Roof 400 12 "sf.FlightLog 1, sf.Intent.Y 0.004" latent-f1-roof-400
```

На каждом журнале (стоп-ворота N9 после, §6.9):

```bash
F=docs/superpowers/notes/evidence/$(date +%F)-latent-f1-side-60.log   # и три остальных
grep -c "Frame handover: Ship -> Station" "$F"   # 0
grep -c "while bound to" "$F"                    # 0
grep -c "skipped carries [1-9]" "$F"             # 0
grep -c "Carry NOT RUNNING" "$F"                 # 0
grep -c "LogStarForge: Error" "$F"               # 0
grep -c "LogStarForge: Warning" "$F"             # 0
grep -c "LogStarForge: Display: World bounds:" "$F"   # 1: во всех прогонах без исключения (§8.2)
```

Удар корпуса о стену на 0.8 м/с может сбить стоящего — `Footing LOST … felt …` уровня `Display`, класс O2, протокол, а не ворота (§6.9). Записать число в бланк, если было.

- [ ] **Step 5: Регрессия `kit-1.4`**

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-1.4
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-1.4; echo "exit $?"
```

`compare: 0 findings`: ни одна строка ни одного повтора не изменилась (§6.9). Четыре журнала N9 после исправления — команды, которых нет в базе; сравнитель судит их вердиктом по их собственным опорным. Проход — отдельный, чтобы расхождение исправления, если оно есть, читалось без примеси набора (§8.4). Строка в таблицу регрессий бланка.

- [ ] **Step 6: Коммит**

```bash
git status --short
git add Source/StarForge/Private/Ship/SFTestStation.cpp Source/StarForge/Public/Ship/SFTestStation.h Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp Source/StarForge/Public/Ship/SFInteriorVolumeComponent.h docs/superpowers/notes/evidence/*-latent-f1-side-60.log docs/superpowers/notes/evidence/*-latent-f1-side-400.log docs/superpowers/notes/evidence/*-latent-f1-roof-60.log docs/superpowers/notes/evidence/*-latent-f1-roof-400.log docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
fix(station): give the rig station a room box and a tunnel box, not one box for both

The one box reached out of the tunnel into open space, 4.7 m past each tunnel
wall and 5.1 m over its roof, and a man standing at the hatch end of a ship
flush against the station's wall was handed to the station while on his own
floor -- F1 without a hull inside the station. Reproduced headless in both
cases before this commit; after it, the same commands give no handover at
either frame rate.

Both boxes come from the constants the panels come from. The tunnel box still
begins on the port plane, so the hatch crossing is untouched: the evidence
replay comes out line for line against the pass taken before any code change.

With two boxes per carrier the refused-handover episode becomes per carrier,
not per box. The REFUSED line promises silence until he leaves the carrier's
volume, and per box it would have broken that promise at the tunnel's end.
MSG
```

---

### Task 5: Интерфейс `ISFDockingStation`, строка о порте-сироте и три поиска

Станцию набора не найдёт ни один из трёх поисков: все три перебирают `TActorIterator<ASFTestStation>` (§3). Решение 2 — явная метка, интерфейс. Чисто виртуальные методы закрывают «забыл определить», строка о порте-сироте — «забыл объявить» (§6.7, п. 1–2).

**Files:**
- Create: `Source/StarForge/Public/Ship/SFDockingStation.h`
- Create: `Source/StarForge/Private/Ship/SFDockingStation.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestStation.h` (наследование, строка 33; публичные методы после строки 45)
- Modify: `Source/StarForge/Private/Ship/SFTestStation.cpp` (новый метод в конце файла)
- Modify: `Source/StarForge/Private/Ship/SFDockingPortComponent.cpp` (`BeginPlay`, после строки 35)
- Modify: `Source/StarForge/Private/Ship/SFDockingComponent.cpp` (include, строка 11; `ResolvePorts`, строки 104–127)
- Modify: `Source/StarForge/Private/Player/SFCharacter.cpp` (include, строка 16; строки 432–439)
- Modify: `Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp` (include, строка 9; строки 75–83)

**Interfaces:**
- Отдаёт задачам 6, 8, 9, 13, 15, 17:
  - `UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint)) class USFDockingStation : public UInterface`
  - `class STARFORGE_API ISFDockingStation { virtual void GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const = 0; virtual USFShipFrameComponent* GetStationFrame() const = 0; virtual void OnDockingPortChanged(USFDockingPortComponent* Port) = 0; static TArray<AActor*> FindAll(const UWorld* World, const AActor* Except = nullptr); };`
  - `ASFTestStation : public AActor, public ISFDockingStation`

- [ ] **Step 0: Стоп-точка — порядок слияния с F4 решён**

Эта задача первой правит `SFDockingComponent.cpp`, а его же переписывает F4 (Global Constraints). Дальше без записанного решения владельца — (а), (б) или (в) — не идти.

```bash
grep -c "TeleportDetachedRootExactly" Source/StarForge/Private/Ship/SFDockingComponent.cpp   # 0: F4 не влита; >= 1: влита
cat Source/StarForgeCore/Private/Tests/*.cpp | grep -c IMPLEMENT_SIMPLE_AUTOMATION_TEST   # тестов в исходниках
python -c "import json;print(json.load(open('Saved/Regress/kit-0-before/summary.json'))['tests']['success'])"   # тестов в базе
```

Ворота: вариант записан в шапку бланка базы, и два последних числа равны при любом варианте. Задачи 1–4 тестов не добавляют. Неравные числа значат, что база снята не на том дереве, что лежит сейчас; обычно F4 влита после базы. Тогда стоп. Перенести `Saved/Regress/kit-0-before` в `Saved/Regress/kit-0-pre-f4` и снять базу заново шагами 4–6 задачи 1. Первое число ≥ 1 значит, что F4 в дереве: правки этой задачи и задач 6, 15–18 ставятся по якорному тексту, а не по номерам строк.

- [ ] **Step 1: Интерфейс**

`Source/StarForge/Public/Ship/SFDockingStation.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFDockingStation.generated.h"

class AActor;
class UWorld;
class USFDockingPortComponent;
class USFShipFrameComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USFDockingStation : public UInterface
{
	GENERATED_BODY()
};

/**
 * What makes an actor a docking station: it says so (station-kit spec 6.7, decision 2).
 *
 * An interface on the actor rather than a flag on the port, because two of the three searches ask
 * about the actor -- sf.BindFrame station wants its frame, sf.AimAt station wants it as a target --
 * and a flag on a port would, at boarding, call a ship with such a port a station. The interface
 * says what an actor IS; the rejected test by composition said what it LACKED, and boarding would
 * hand a target exactly the docking component that test demanded it not have.
 *
 * The methods are pure, after IAbilitySystemInterface, so a class that declares the interface and
 * forgets one does not build. A class with a port that forgets to declare it at all is caught by
 * USFDockingPortComponent::BeginPlay, which says so in the log. There is no honest default here --
 * unlike ISFStationOperator, where "I am not flying" is one.
 */
class STARFORGE_API ISFDockingStation
{
	GENERATED_BODY()

public:
	/**
	 * This station's ports IN BUILD ORDER (Port0, Port1, ...), replacing whatever Out held. The
	 * nearest-free-port rule breaks exact ties by this order (spec 7.5), so it must not depend on
	 * the order GetComponents happens to return.
	 */
	virtual void GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const = 0;

	/** The frame sf.BindFrame station binds to. */
	virtual USFShipFrameComponent* GetStationFrame() const = 0;

	/**
	 * A port of this station changed hands or state: captured, docked, releasing, let go. Called
	 * directly by the port, so a station that does not tick still hears every change.
	 */
	virtual void OnDockingPortChanged(USFDockingPortComponent* Port) = 0;

	/**
	 * Every docking station in World other than Except, SORTED BY NAME. Never by the order the
	 * iterator walks the level: that order is what the old warning in ResolvePorts called choosing
	 * "by nothing".
	 */
	static TArray<AActor*> FindAll(const UWorld* World, const AActor* Except = nullptr);
};
```

`Source/StarForge/Private/Ship/SFDockingStation.cpp`:

```cpp
#include "Ship/SFDockingStation.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"

TArray<AActor*> ISFDockingStation::FindAll(const UWorld* World, const AActor* Except)
{
	TArray<AActor*> Stations;
	if (!World)
	{
		return Stations;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor != Except && Actor->Implements<USFDockingStation>())
		{
			Stations.Add(Actor);
		}
	}

	Stations.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	return Stations;
}
```

- [ ] **Step 2: `ASFTestStation` объявляет интерфейс**

`Source/StarForge/Public/Ship/SFTestStation.h`: в include добавить `#include "Ship/SFDockingStation.h"` перед `#include "SFTestStation.generated.h"`; строку 33 заменить на `class STARFORGE_API ASFTestStation : public AActor, public ISFDockingStation`; после `GetDockingPort()` (строка 45):

```cpp
	/** Its one port. */
	virtual void GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const override;

	virtual USFShipFrameComponent* GetStationFrame() const override { return Frame; }

	/**
	 * Nothing. The rig station has no line to reprint, and every figure the project has recorded
	 * was taken on it: its log must not move by a character (station-kit spec 6.7 item 1).
	 */
	virtual void OnDockingPortChanged(USFDockingPortComponent* Port) override {}
```

`Source/StarForge/Private/Ship/SFTestStation.cpp`, в конец:

```cpp
void ASFTestStation::GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const
{
	Out.Reset();
	if (DockingPort)
	{
		Out.Add(DockingPort.Get());
	}
}
```

- [ ] **Step 3: Строка о порте-сироте**

`Source/StarForge/Private/Ship/SFDockingPortComponent.cpp`: добавить `#include "Ship/SFDockingComponent.h"` и `#include "Ship/SFDockingStation.h"`; в `BeginPlay`, **после** существующей строки `Docking port on …` (строка 35):

```cpp
	// A port nobody can use says so. It belongs to a station, which a hull docks to, or to a ship,
	// whose docking component is what docks; anything else was built with a port and without the
	// declaration that makes it a station, and every search for stations would pass it by in silence.
	const AActor* Owner = GetOwner();
	if (Owner && !Owner->Implements<USFDockingStation>() && !Owner->FindComponentByClass<USFDockingComponent>())
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Docking port on '%s' belongs to neither a docking station nor a docking ship: no hull can dock to it, and it docks to nothing."),
			*Owner->GetName());
	}
```

- [ ] **Step 4: Три поиска**

`Source/StarForge/Private/Ship/SFDockingComponent.cpp`: include `"Ship/SFTestStation.h"` (строка 11) заменить на `"Ship/SFDockingStation.h"`. В `ResolvePorts` заменить строки 104–127 (комментарий «Counted rather than taken…», цикл по `TActorIterator<ASFTestStation>`, `Warning` и `bHasStation = …`) на:

```cpp
	// Every actor that says it is a docking station, by name (ISFDockingStation::FindAll), and its
	// first port. One station and one port is all step 1 of the station kit has; the rule that
	// chooses among several -- the nearest free port -- is its step 2, and until then a second
	// station is said out loud rather than taken by accident.
	const TArray<AActor*> Stations = ISFDockingStation::FindAll(GetWorld(), Owner);
	FString ChosenName;
	if (Stations.Num() > 0)
	{
		TArray<USFDockingPortComponent*> StationPorts;
		Cast<ISFDockingStation>(Stations[0])->GetDockingPorts(StationPorts);
		StationPort = StationPorts.Num() > 0 ? StationPorts[0] : nullptr;
		ChosenName = Stations[0]->GetName();
	}
	if (Stations.Num() > 1)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Docking: [%s] %d docking stations in this level; '%s' was taken first by name. ")
			TEXT("The nearest free port is not chosen until step 2 of the station kit."),
			*Owner->GetName(), Stations.Num(), *ChosenName);
	}
	bHasStation = StationPort != nullptr;
```

`Source/StarForge/Private/Player/SFCharacter.cpp`: include `"Ship/SFTestStation.h"` (строка 16) заменить на `"Ship/SFDockingStation.h"`; строки 432–439 заменить на:

```cpp
	else if (Command.Equals(TEXT("station"), ESearchCase::IgnoreCase))
	{
		// The first docking station by name, whatever its class: the kit's stations are not
		// ASFTestStation, and a control that could not find them would prove nothing on them.
		const TArray<AActor*> Stations = ISFDockingStation::FindAll(GetWorld(), this);
		if (Stations.Num() > 0)
		{
			Frame = Cast<ISFDockingStation>(Stations[0])->GetStationFrame();
		}
	}
```

`Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp`: include `"Ship/SFTestStation.h"` (строка 9) заменить на `"Ship/SFDockingStation.h"`; строки 75–83 заменить на:

```cpp
	if (Command.Equals(TEXT("station"), ESearchCase::IgnoreCase))
	{
		// Any docking station, the first by name. The miss says "docking station" now, not
		// ASFTestStation: no evidence log carries the old text (station-kit plan, task 5).
		SoughtClass = TEXT("docking station");
		const TArray<AActor*> Stations = ISFDockingStation::FindAll(GetWorld(), Owner);
		if (Stations.Num() > 0)
		{
			ConsoleTarget = Stations[0];
		}
	}
```

- [ ] **Step 5: Собрать; убедиться, что старый текст промаха нигде не записан**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
grep -rl "no ASFTestStation in this level" docs/superpowers/notes/evidence | wc -l   # 0
grep -rn "TActorIterator<ASFTestStation>" Source/ | wc -l                         # 0
```

Сборка — ноль предупреждений: класс с интерфейсом собрался, значит все три метода определены. Проверить, что правило «не определил — не соберётся» и правда держит: временно закомментировать `GetStationFrame` в `SFTestStation.h`, собрать — обязана быть ошибка вида `cannot instantiate abstract class` или `pure virtual function … has no overrider`; вернуть строку и собрать снова.

Поиски на станции набора проверяет H1.6 (задача 12). Здесь — петля с `sf.BindFrame station` и `sf.AimAt station`, где цель поиска ещё `ASFTestStation`:

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=10 -ExecCmds="sf.FlightLog 1, sf.BindFrame station, sf.AimAt station" -log < /dev/null
grep -c "Frame bound by console to Station (sf.BindFrame)" $L        # 1
grep -c "Automation: \[Ship\] sf.AimAt 'station': tracking Station." $L   # 1
grep -c "belongs to neither a docking station nor a docking ship" $L # 0
```

Журнал этой проверки в улики не кладётся.

- [ ] **Step 6: Регрессия `kit-1.5`**

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-1.5
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-1.5; echo "exit $?"
```

`compare: 0 findings`: строки о порте-сироте нет ни на одном старом стенде, выбор станции по имени на единственной станции тот же. Строка в таблицу регрессий.

- [ ] **Step 7: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFDockingStation.h Source/StarForge/Private/Ship/SFDockingStation.cpp Source/StarForge/Public/Ship/SFTestStation.h Source/StarForge/Private/Ship/SFTestStation.cpp Source/StarForge/Private/Ship/SFDockingPortComponent.cpp Source/StarForge/Private/Ship/SFDockingComponent.cpp Source/StarForge/Private/Player/SFCharacter.cpp Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
feat(station): find stations by what they declare, not by the rig station's class

All three station searches iterated ASFTestStation, so a station of any other
class would be invisible to docking, to sf.BindFrame and to sf.AimAt. They now
ask for ISFDockingStation and take stations sorted by name, never by the order
the level happens to be walked.

An interface on the actor and not a flag on the port: two of the three searches
ask about the actor, and at boarding a port flag would call a ship a station.
The methods are pure, so a class that declares the interface and forgets one
does not build; a class with a port that forgets the declaration is named in the
log by the port itself. The rig station declares it and prints nothing new.
MSG
```

---

### Task 6: Держатель порта, уведомление на четырёх переходах и `MeasureRelease`

Строке `Station rig:` уже в шаге 1 нужно знать, кто держит порт, и слышать каждую смену: H1.2 требует четырёх перепечаток по порядку, а станция не тикает (§6.7, п. 4). Здесь же измерение отхода выносится в константный метод (п. 5), чтобы строка читала ровно то, что сделает отход. Обе правки проходят через стыковку и отход, то есть через то, на чём сняты цифры цикла `sf.Dock 2`, отхода вбок и крена, — поэтому у задачи своя регрессия.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFDockingPortComponent.h` (комментарий класса, строки 15–19; публичные методы и поле)
- Modify: `Source/StarForge/Private/Ship/SFDockingPortComponent.cpp` (новый метод)
- Modify: `Source/StarForge/Public/Ship/SFDockingComponent.h` (публичные методы после `CanDockNow`, строка 161)
- Modify: `Source/StarForge/Private/Ship/SFDockingComponent.cpp` (`EnterCapture` строки 316–319; `CompleteCapture` строка 386; `RequestUndock` строки 485–571; `LetGo` строка 646)

**Interfaces:**
- Потребляет из задачи 5: `ISFDockingStation::OnDockingPortChanged`.
- Отдаёт задачам 8, 9, 15, 17, 18:
  - `USFDockingComponent* USFDockingPortComponent::GetHolder() const`, `bool IsHeld() const`, `void NotifyHolder(USFDockingComponent* HolderOrNull)`
  - `static FBox USFDockingComponent::StationPhysicalBoundsLocalCm(const AActor& Station)`
  - `bool USFDockingComponent::MeasureRelease(double& OutStandoffMetres, double& OutPeakMetresPerSecSq) const`

- [ ] **Step 1: Держатель на порту**

`Source/StarForge/Public/Ship/SFDockingPortComponent.h`: добавить `class USFDockingComponent;` перед `UCLASS`; абзац комментария строк 15–19 («It knows nothing about docking…») заменить на:

```cpp
 * It does not move, latch, refuse or decide; it is a place with an orientation, and
 * USFDockingComponent is what reads it. It knows one fact since the station kit: WHO HOLDS it,
 * from the moment a hull begins its capture to the moment it is let go -- because spec 9 of the
 * second phase makes occupancy a state of the node, and a station that does not tick has to hear
 * of every change. The state of the hold (capturing, docked, releasing) is not duplicated here:
 * the holder knows it, and the pair keeps one opinion about who is attached to whom. The
 * separation is the one that keeps USFTurretComponent from knowing who is aiming.
```

После `GetPortTransform()` (строка 32):

```cpp
	/** The docking component of the hull that holds this port, from EnterCapture to LetGo; null when free. */
	USFDockingComponent* GetHolder() const { return Holder.Get(); }

	bool IsHeld() const { return Holder.IsValid(); }

	/**
	 * Called by a docking component on each of its four transitions -- the capture begins, the
	 * capture completes, the release begins, the hull is let go -- with itself, or with null when
	 * it lets go. Sets the holder and tells the owner, if the owner is a docking station.
	 *
	 * A destroyed holder leaves the weak pointer empty WITHOUT an event, and the station hears
	 * nothing. No run of the station kit's steps 1-2 can destroy a docked hull (nothing fires;
	 * spec 6.7 item 4), and a subscription no run reaches is mechanism nobody can test; the stage
	 * that makes a docked hull destructible adds it together with the run.
	 */
	void NotifyHolder(USFDockingComponent* HolderOrNull);

private:
	TWeakObjectPtr<USFDockingComponent> Holder;
```

`Source/StarForge/Private/Ship/SFDockingPortComponent.cpp`, в конец:

```cpp
void USFDockingPortComponent::NotifyHolder(USFDockingComponent* HolderOrNull)
{
	Holder = HolderOrNull;

	// Directly, not through a tick: a station need not tick to hear its port change.
	if (ISFDockingStation* Station = Cast<ISFDockingStation>(GetOwner()))
	{
		Station->OnDockingPortChanged(this);
	}
}
```

- [ ] **Step 2: Бокс станции и отход — функциями для двух вызывающих**

`Source/StarForge/Public/Ship/SFDockingComponent.h`, после `CanDockNow()` (строка 161):

```cpp
	/**
	 * Every one of Station's OWN primitives that takes part in physics, boxed in Station's own frame,
	 * in centimetres. The release measures against this, and so does a kit station's Station rig:
	 * line -- one function, so the line reads exactly the box the release will use. Neighbours docked
	 * to the station are other actors and are not in it; a visual mesh with collision is, which is
	 * rule 5 of the station kit.
	 */
	static FBox StationPhysicalBoundsLocalCm(const AActor& Station);

	/**
	 * What a release from the docked pose would do now: the standoff along the station port's axis,
	 * in metres, and the smoothstep peak over CaptureSeconds. False unless docked to a station port.
	 * RequestUndock releases by exactly these numbers. Asked from any other pose the question means
	 * something else -- ReleaseStandoff answers "how far out of this pose", and from outside the
	 * station that is zero -- so it is not asked.
	 */
	bool MeasureRelease(double& OutStandoffMetres, double& OutPeakMetresPerSecSq) const;
```

`Source/StarForge/Private/Ship/SFDockingComponent.cpp`: в `RequestUndock` заменить блок строк 485–531 (от комментария «The standoff, measured on the two bodies as they are» до вычисления `PeakMetresPerSecSq` включительно) на:

```cpp
	// The standoff, measured on the two bodies as they are, by the same method the kit station's
	// Station rig: line prints -- so the line reads exactly what this release is about to do.
	double StandoffMetres = 0.0;
	double PeakMetresPerSecSq = 0.0;
	MeasureRelease(StandoffMetres, PeakMetresPerSecSq);

	const FVector AwayWorld = StationPort->GetPortTransform().GetUnitAxis(EAxis::X);
```

Добавить в файл, перед `RequestUndock`:

```cpp
FBox USFDockingComponent::StationPhysicalBoundsLocalCm(const AActor& Station)
{
	// Boxed in the station's own frame, not the world's: world bounds grow with the station's yaw,
	// and a place can yaw it (three degrees asked 3.26 m where 2.93 is enough). Rigid, so the box is
	// in world lengths and a hull's radius means the same thing inside it.
	const FTransform StationFrameCm(Station.GetActorQuat(), Station.GetActorLocation());

	FBox Bounds(ForceInit);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(&Station);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive && CollisionEnabledHasPhysics(Primitive->GetCollisionEnabled()))
		{
			Bounds += Primitive->CalcBounds(
				Primitive->GetComponentTransform().GetRelativeTransform(StationFrameCm)).GetBox();
		}
	}
	return Bounds;
}

bool USFDockingComponent::MeasureRelease(double& OutStandoffMetres, double& OutPeakMetresPerSecSq) const
{
	const UPrimitiveComponent* Hull = GetHull();
	const AActor* Station = StationPort ? StationPort->GetOwner() : nullptr;
	if (State != ESFDockingState::Docked || !Hull || !Station)
	{
		return false;
	}

	// The hull is its root box, taken as the sphere through its corners so the answer does not
	// depend on which way the pilot turns first; the station is every one of its primitives that
	// takes part in physics. The interior volumes are query only and are left out: a hull may stand
	// in a volume, it may not stand in a wall. Centimetres in, metres into the maths, once each way.
	const UBoxComponent* HullBox = Cast<UBoxComponent>(Hull);
	const double HullRadiusCm = HullBox ? HullBox->GetScaledBoxExtent().Size() : Hull->Bounds.SphereRadius;

	const FTransform StationFrameCm(Station->GetActorQuat(), Station->GetActorLocation());
	const FBox StationLocalBoundsCm = StationPhysicalBoundsLocalCm(*Station);

	const auto ToMetres = [](const FVector& Cm)
	{
		return FVector(
			StarForge::Units::CmToMetres(Cm.X),
			StarForge::Units::CmToMetres(Cm.Y),
			StarForge::Units::CmToMetres(Cm.Z));
	};

	const FBox StationLocalBoundsMetres = StationLocalBoundsCm.IsValid
		? FBox(ToMetres(StationLocalBoundsCm.Min), ToMetres(StationLocalBoundsCm.Max))
		: FBox(ForceInit);

	OutStandoffMetres = StarForge::Docking::ReleaseStandoff(
		ToMetres(Hull->GetComponentLocation()),
		StarForge::Units::CmToMetres(HullRadiusCm),
		StationPort->GetPortTransform().GetUnitAxis(EAxis::X),
		FTransform(StationFrameCm.GetRotation(), ToMetres(StationFrameCm.GetTranslation())),
		StationLocalBoundsMetres);
	OutPeakMetresPerSecSq = StarForge::Docking::SmoothstepPeakAcceleration(OutStandoffMetres, CaptureSeconds);
	return true;
}
```

Порядок вызовов и все выражения — те же, что стояли в `RequestUndock`; меняется только место, где они записаны. Регрессия обязана это подтвердить строкой `Dock: [Ship] releasing, 2.93 m …`, неизменной на каждом повторе.

- [ ] **Step 3: Четыре уведомления**

В `EnterCapture`, после `State = ESFDockingState::Capturing;` (строка 317), перед `return true;`:

```cpp
	// Held from here until LetGo: while a capture runs, no second hull may choose this port. The
	// port tells its station; DockImmediately comes through here too and holds it the same way.
	StationPort->NotifyHolder(this);
```

В `CompleteCapture`, после `UE_LOG(… TEXT("Dock: [%s] docked."), …)` (строка 386):

```cpp
	StationPort->NotifyHolder(this);
```

В `RequestUndock`, в самом конце ветки отхода — после двух `Warning` о стоянке и о потолке, перед `return true;` (строка 571):

```cpp
	StationPort->NotifyHolder(this);
```

В `LetGo`, после `UE_LOG(… TEXT("Dock: [%s] undocked."), …)` (строка 646):

```cpp
	// One function for both ways out of Docked, so the release's end and sf.DockRelease 0's instant
	// let-go free the port by the same line, and a control cannot drift from what it controls.
	if (StationPort)
	{
		StationPort->NotifyHolder(nullptr);
	}
```

- [ ] **Step 4: Собрать и убедиться, что ничего не напечаталось нового**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
```

Держатель наблюдаем только через станцию набора (задача 9) — `ASFTestStation::OnDockingPortChanged` пуст намеренно. Проверка этой задачи — регрессия.

- [ ] **Step 5: Регрессия `kit-1.6`**

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-1.6
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-1.6; echo "exit $?"
```

`compare: 0 findings`. Особенно — повторы групп цикла `sf.Dock 2`, отхода вбок, крена и возврата в тоннель: там стоят и держатель, и вынесенное измерение. Строка в таблицу регрессий.

- [ ] **Step 6: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFDockingPortComponent.h Source/StarForge/Private/Ship/SFDockingPortComponent.cpp Source/StarForge/Public/Ship/SFDockingComponent.h Source/StarForge/Private/Ship/SFDockingComponent.cpp docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
feat(docking): a port knows who holds it, and its station hears every change

A kit station has to say in its own line who holds each of its ports and must
say it again at every capture, docking, release and let-go -- and a station
does not tick. So the port gains exactly one fact, its holder, set when a
capture begins and cleared by LetGo, and the docking component tells the port
at each of its four transitions, which tells its station directly.

The state of the hold is not copied onto the port: the holder knows it, and the
pair keeps one opinion about who is attached to whom.

The release's measurement moves into a const method, and the station's physical
box into a static one, so the station's line can print exactly what a release
would do. The expressions are the ones RequestUndock used; the evidence replay
proves the release lines unchanged.
MSG
```

---

### Task 7: Геометрия правил 1 и 2 в `StarForgeCore` — восемь тестов шага 1

Строка `Station rig:` судит о правилах 1 и 2 тремя числами: насколько физическое выходит за плоскость порта, насколько объём выходит за плоскость, насколько объём выходит из своей оболочки (§6.8). Это арифметика боксов, и ошибка в ней не падает, а печатает спокойное `0.000` над сломанной станцией. Поэтому — чистые функции под тестами, каждый тест убит мутантом (§8.1).

**Files:**
- Modify: `Source/StarForgeCore/Public/DockingMath.h` (перед закрывающей `}` пространства, строка 175)
- Modify: `Source/StarForgeCore/Private/DockingMath.cpp` (безымянное пространство, строки 5–37; новые функции в конец пространства)
- Create: `Source/StarForgeCore/Private/Tests/StationKitDockingTest.cpp`

**Interfaces:**
- Потребляет: `FBox`, `FVector`, `FTransform`, `FMath` из `Core`; существующие `ReleaseStandoff`, `SmoothstepPeakAcceleration`, `FDockingLimits`; `StarForge::ShipFrame::GravPlateLimitMetersPerSecSq` (только чтение, `ShipFrameMath.h`).
- Отдаёт задачам 9 и 10, в `namespace StarForge::Docking`:
  - `STARFORGECORE_API double BoundsPastPortPlane(const FBox& Bounds, const FVector& PortOrigin, const FVector& PortOutwardAxis);`
  - `STARFORGECORE_API double BoxReachOutside(const FBox& Inner, const FBox& Envelope);`
  - `inline constexpr double PrintedResolutionMetres = 0.001;`
  - `inline constexpr double RedAtOrAboveMetres = 0.5 * PrintedResolutionMetres;`
  - `STARFORGECORE_API bool IsRedAtPrintedResolution(double Metres);`

- [ ] **Step 1: Написать падающие тесты**

`Source/StarForgeCore/Private/Tests/StationKitDockingTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "DockingMath.h"
#include "ShipFrameMath.h"

#include <limits>

namespace
{
namespace StationKitTests
{
	/**
	 * A station whose -Y face lies PastMetres past a port plane at Y = 0 facing -Y, with the
	 * station behind the plane: X +-30, Y -PastMetres..36, Z +-16. The berth's own proportions.
	 */
	FBox FaceOnThePlane(double PastMetres)
	{
		return FBox(FVector(-30.0, -PastMetres, -16.0), FVector(30.0, 36.0, 16.0));
	}

	/** The berth's physical box in its own frame, metres (station-kit spec 6.8), and Port0 on it. */
	FBox BerthBoxMetres()
	{
		return FBox(FVector(-18.38, -18.10, -16.0), FVector(41.62, 17.90, 16.0));
	}
	const FVector BerthPort0Metres(0.0, -18.10, -2.75);

	/** A kit port's +X: yawed -90 degrees, it points along the station's -Y. */
	const FVector AwayAxis(0.0, -1.0, 0.0);

	constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneIsZeroWhenFlushTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneIsZeroWhenFlush",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneIsZeroWhenFlushTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// Rule 1 held exactly: the face lies on the plane and the station is behind it. The support along
	// -Y is the face itself, -18 + 18 in the arithmetic. With the axis taken the wrong way round the
	// same box would read its whole depth, 36 m, and every station would be red.
	TestTrue(TEXT("a face on the plane reaches nothing past it"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(0.0), FVector::ZeroVector, AwayAxis), 0.0, 1e-12));

	TestTrue(TEXT("the berth's box ends on Port0's plane"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(BerthBoxMetres(), BerthPort0Metres, AwayAxis), 0.0, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneMeasuresAlongTheAxisTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneMeasuresAlongTheAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneMeasuresAlongTheAxisTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// Control N1's collider and N1b's, as the maths sees them.
	TestTrue(TEXT("a metre past the plane reads 1.0"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(1.0), FVector::ZeroVector, AwayAxis), 1.0, 1e-12));
	TestTrue(TEXT("twenty centimetres reads 0.2"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(0.2), FVector::ZeroVector, AwayAxis), 0.2, 1e-12));

	// The same metre with the port somewhere else: the answer is a length from the plane, not a coordinate.
	const FVector Elsewhere(5.0, -3.0, 1.0);
	const FBox Shifted = FaceOnThePlane(1.0).ShiftBy(Elsewhere);
	TestTrue(TEXT("measured from the port, wherever it is"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(Shifted, Elsewhere, AwayAxis), 1.0, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneIgnoresWhatIsBehindTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneIgnoresWhatIsBehind",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneIgnoresWhatIsBehindTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// A box five metres short of the plane is not "minus five past it". The instrument looks one way
	// (spec 5, rule 1), and a boom that stops short of its port is caught by another number.
	const FBox Behind(FVector(-30.0, 5.0, -16.0), FVector(30.0, 40.0, 16.0));
	const double Past = StarForge::Docking::BoundsPastPortPlane(Behind, FVector::ZeroVector, AwayAxis);
	TestTrue(TEXT("what is behind the plane is exactly nothing past it"), Past == 0.0);

	// A station with nothing physical in it has nothing past any plane.
	TestTrue(TEXT("an empty box reaches nothing"),
		StarForge::Docking::BoundsPastPortPlane(FBox(ForceInit), FVector::ZeroVector, AwayAxis) == 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneUsesTheSupportOnATurnedAxisTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneUsesTheSupportOnATurnedAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneUsesTheSupportOnATurnedAxisTest::RunTest(const FString& Parameters)
{
	// A unit-half cube about the port, the axis at 45 degrees: the corner (1, 1) is furthest along it,
	// at sqrt(2). A function that takes only the largest component of the axis reads 0.707 and calls a
	// protrusion half of what it is -- exact on the axes, wrong everywhere else.
	const FBox Cube(FVector(-1.0), FVector(1.0));
	TestTrue(TEXT("45 degrees: the corner, at sqrt(2)"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(Cube, FVector::ZeroVector, FVector(1.0, 1.0, 0.0)),
			FMath::Sqrt(2.0), 1e-12));

	// 30 degrees on a 2 x 1 x 1 half-box: 2 cos 30 + 1 sin 30.
	const double Rad = FMath::DegreesToRadians(30.0);
	const FBox Long(FVector(-2.0, -1.0, -1.0), FVector(2.0, 1.0, 1.0));
	TestTrue(TEXT("30 degrees: both components count"),
		FMath::IsNearlyEqual(
			StarForge::Docking::BoundsPastPortPlane(Long, FVector::ZeroVector, FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0)),
			2.0 * FMath::Cos(Rad) + FMath::Sin(Rad), 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneReadsZeroAtThePrintedResolutionTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneReadsZeroAtThePrintedResolution",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneReadsZeroAtThePrintedResolutionTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// A nanometre past the plane is past it, and it is not red: the line prints metres to 0.001, and
	// red starts where the printed figure stops being 0.000 (spec 6.8). An exact comparison with zero
	// would paint a turned station red on a healthy build.
	const double Nano = StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(1e-9), FVector::ZeroVector, AwayAxis);
	TestTrue(TEXT("a nanometre is past the plane"), Nano > 0.0);
	TestFalse(TEXT("and is not red"), StarForge::Docking::IsRedAtPrintedResolution(Nano));

	// How a turned station arrives: its box and its port carried into the world through 90 degrees of
	// yaw and an offset, and measured there. Flush in its own frame, it must stay calm here.
	const FTransform Turned(FRotator(0.0, 90.0, 0.0), FVector(1234.5, -678.9, 42.0));
	const double Turn = StarForge::Docking::BoundsPastPortPlane(
		BerthBoxMetres().TransformBy(Turned),
		Turned.TransformPositionNoScale(BerthPort0Metres),
		Turned.TransformVectorNoScale(AwayAxis));
	TestTrue(TEXT("turned 90 degrees, the flush berth is not negative"), Turn >= 0.0);
	TestFalse(TEXT("and is not red"), StarForge::Docking::IsRedAtPrintedResolution(Turn));

	// A centimetre is the smallest real protrusion a whole-centimetre builder can make: red.
	TestTrue(TEXT("a centimetre is red"), StarForge::Docking::IsRedAtPrintedResolution(
		StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(0.01), FVector::ZeroVector, AwayAxis)));

	TestFalse(TEXT("zero is not red"), StarForge::Docking::IsRedAtPrintedResolution(0.0));
	TestTrue(TEXT("and a figure nobody could compute is red, not calm"), StarForge::Docking::IsRedAtPrintedResolution(NaN));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoxReachOutsideIsZeroInsideTest,
	"StarForge.Core.Docking.BoxReachOutsideIsZeroInside",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoxReachOutsideIsZeroInsideTest::RunTest(const FString& Parameters)
{
	const FBox Envelope(FVector(-1.0), FVector(1.0));
	TestTrue(TEXT("a box inside its envelope reaches nothing out"),
		StarForge::Docking::BoxReachOutside(FBox(FVector(-0.5), FVector(0.5)), Envelope) == 0.0);
	TestTrue(TEXT("a box flush on every face reaches nothing out"),
		StarForge::Docking::BoxReachOutside(Envelope, Envelope) == 0.0);

	// Spec 6.8 on the boom's own numbers, block frame B, metres. The channel's +Y end is the hall-side
	// face of the front wall, -11.8, because the door in that wall belongs to the boom.
	const FBox Boom0(FVector(-12.62, -24.0, 12.0), FVector(-10.62, -11.8, 14.5));
	const FBox Channel(FVector(-12.62, -24.0, 12.0), FVector(-10.62, -11.8, 14.5));
	TestTrue(TEXT("Boom0 is within its channel"),
		FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Boom0, Channel), 0.0, 1e-12));

	// The trap spec 6.8 steps around: an envelope ended on the wall's outer face, -12, reads the
	// 20 cm of door as the volume leaving its channel, on a healthy build.
	const FBox ChannelToTheOuterFace(FVector(-12.62, -24.0, 12.0), FVector(-10.62, -12.0, 14.5));
	TestTrue(TEXT("an envelope ended at -12 reads 0.200"),
		FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Boom0, ChannelToTheOuterFace), 0.2, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoxReachOutsideTakesTheLargestExcessTest,
	"StarForge.Core.Docking.BoxReachOutsideTakesTheLargestExcess",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoxReachOutsideTakesTheLargestExcessTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	const FBox Envelope(FVector(-1.0), FVector(1.0));
	const FBox Out(FVector(-1.0, -1.0, -1.2), FVector(2.0, 1.0, 1.0));
	TestTrue(TEXT("a metre out of +X and 0.2 out of -Z reads the metre"),
		FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Out, Envelope), 1.0, 1e-12));

	// Every one of the six faces counts: a hall that leaves by its roof is as out as one that leaves by its front.
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FBox Probe(FVector(-1.0), FVector(1.0));
			if (Side == 0)
			{
				Probe.Min[Axis] -= 0.5;
			}
			else
			{
				Probe.Max[Axis] += 0.5;
			}
			TestTrue(FString::Printf(TEXT("half a metre out of the %s face of axis %d"), Side == 0 ? TEXT("min") : TEXT("max"), Axis),
				FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Probe, Envelope), 0.5, 1e-12));
		}
	}

	// An envelope nobody could read is not a calm zero.
	TestFalse(TEXT("an invalid envelope answers no number"),
		FMath::IsFinite(StarForge::Docking::BoxReachOutside(Out, FBox(ForceInit))));
	TestFalse(TEXT("nor does an envelope with a NaN in it"),
		FMath::IsFinite(StarForge::Docking::BoxReachOutside(Out, FBox(FVector(-1.0), FVector(NaN, 1.0, 1.0)))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffGrowsByTheProtrusionTest,
	"StarForge.Core.Docking.ReleaseStandoffGrowsByTheProtrusion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffGrowsByTheProtrusionTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// Spec 5, rule 1: d = R - h + p. The hull's box is 8 m, R = 4 sqrt(3); its +Y face lies on the port
	// plane, so its centre is h = 4 m out along the away axis; p is how far the station's box reaches
	// past the plane. Everything in metres and seconds, the release's own units.
	const double Radius = 4.0 * FMath::Sqrt(3.0);
	const FVector HullCentre(0.0, -4.0, 0.0);
	const auto Standoff = [&](double P)
	{
		return StarForge::Docking::ReleaseStandoff(HullCentre, Radius, AwayAxis, FaceOnThePlane(P));
	};
	const auto Peak = [](double D) { return StarForge::Docking::SmoothstepPeakAcceleration(D, 1.0); };
	const double Flush = Radius - 4.0;   // 2.928203

	TestTrue(TEXT("p = 0 stands the hull 2.928 m out"), FMath::IsNearlyEqual(Standoff(0.0), Flush, 1e-9));
	TestTrue(TEXT("p = 0.2 stands it 3.128 m out"), FMath::IsNearlyEqual(Standoff(0.2), Flush + 0.2, 1e-9));
	TestTrue(TEXT("p = 1.0 stands it 3.928 m out"), FMath::IsNearlyEqual(Standoff(1.0), Flush + 1.0, 1e-9));
	TestTrue(TEXT("and the peaks are 17.57, 18.77, 23.57 m/s2"),
		FMath::IsNearlyEqual(Peak(Standoff(0.0)), 6.0 * Flush, 1e-6)
		&& FMath::IsNearlyEqual(Peak(Standoff(0.2)), 6.0 * (Flush + 0.2), 1e-6)
		&& FMath::IsNearlyEqual(Peak(Standoff(1.0)), 6.0 * (Flush + 1.0), 1e-6));

	// The two thresholds, each from both sides, with the comparisons the warnings make (spec 5). The window
	// warns on d > 3.0, so p stays inside up to and including 7 - 4 sqrt(3) = 0.0717967697... m; the ceiling
	// warns on 6d >= 19.62, that is d >= 3.27, so p stays under it only below 7.27 - 4 sqrt(3) =
	// 0.3417967697... m. The six-decimal 0.071797 and 0.341797 are those bounds rounded up, by 2.3e-7 m, so
	// each already lies past its limit, as do the rounded 0.072 and 0.342. The probes bracket each bound
	// instead: 0.0717 and 0.3417 lie 9.7e-5 m on the calm side of it, 0.0719 and 0.3419 1.03e-4 m past it.
	const double Window = StarForge::Docking::FDockingLimits().DistanceMetres;
	const double Ceiling = StarForge::ShipFrame::GravPlateLimitMetersPerSecSq;
	TestTrue(TEXT("p = 0.0717 stays inside the 3.0 m window"), Standoff(0.0717) <= Window);
	TestTrue(TEXT("p = 0.0719 leaves it"), Standoff(0.0719) > Window);
	TestTrue(TEXT("p = 0.3417 peaks under the ceiling"), Peak(Standoff(0.3417)) < Ceiling);
	TestTrue(TEXT("p = 0.3419 peaks at or over it"), Peak(Standoff(0.3419)) >= Ceiling);

	return true;
}
```

- [ ] **Step 2: Увидеть, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
```

Ожидается ошибка компиляции: `BoundsPastPortPlane`, `BoxReachOutside`, `IsRedAtPrintedResolution` не объявлены. Это «красный» шаг.

- [ ] **Step 3: Заголовок**

`Source/StarForgeCore/Public/DockingMath.h`, перед закрывающей `}` пространства (строка 175):

```cpp
	/**
	 * How far a box reaches past a port's plane, along the port's OUTWARD axis: the box's support in
	 * that direction measured from the plane, and never less than zero (station-kit spec 5, rule 1).
	 *
	 * The support, not the centre's projection: a box is past a plane by its furthest corner. Zero for
	 * an invalid box -- a station with nothing physical in it has nothing past any plane. The axis is
	 * normalised here; a zero or non-finite axis, or a non-finite origin, answers NaN, which is red at
	 * the printed resolution: a question that cannot be asked must not read as a calm zero.
	 *
	 * It looks one way on purpose. A box that stops short of the plane reads zero, not a negative
	 * figure, and a boom shorter than its port is caught by BoxReachOutside against the boom's
	 * envelope instead (spec 6.8). Lengths in whatever unit the caller passes; the game passes metres.
	 */
	STARFORGECORE_API double BoundsPastPortPlane(const FBox& Bounds, const FVector& PortOrigin, const FVector& PortOutwardAxis);

	/**
	 * How far Inner reaches outside Envelope: the largest excess over all six faces, zero when Inner
	 * lies within it or flush on it (station-kit spec 5, rule 2). NaN when either box is invalid or
	 * not finite -- an envelope nobody could read is not an envelope anything is inside.
	 */
	STARFORGECORE_API double BoxReachOutside(const FBox& Inner, const FBox& Envelope);

	/**
	 * The Station rig: line prints these lengths in metres to three decimals. A figure is red when it
	 * would print as more than zero: from 0.0005 m up (spec 6.8). Every collider, volume and port the
	 * kit builds stands on whole centimetres, so a real protrusion is at least 0.01 m, and the noise a
	 * turned transform leaves is many orders below a millimetre. The threshold and the print agree by
	 * construction: there is no red 0.000 and no calm 0.001.
	 */
	inline constexpr double PrintedResolutionMetres = 0.001;
	inline constexpr double RedAtOrAboveMetres = 0.5 * PrintedResolutionMetres;

	/** True from RedAtOrAboveMetres up, and for NaN: an instrument that computed nothing is not calm. */
	STARFORGECORE_API bool IsRedAtPrintedResolution(double Metres);
```

- [ ] **Step 4: Реализация**

`Source/StarForgeCore/Private/DockingMath.cpp`: `#include <limits>` уже стоит после `#include "DockingMath.h"` — его добавило ревью 2026-10-01 (поправка П2); второй не добавлять. В безымянное пространство (после `SearchIterations`; на `bcdb849` строка 36, после ревью 45):

```cpp
		bool IsFiniteVector(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}

		bool IsFiniteBox(const FBox& Box)
		{
			return IsFiniteVector(Box.Min) && IsFiniteVector(Box.Max);
		}
```

В конец пространства `StarForge::Docking` (после `SmoothstepPeakAcceleration`, строка 243):

```cpp
	double BoundsPastPortPlane(const FBox& Bounds, const FVector& PortOrigin, const FVector& PortOutwardAxis)
	{
		const FVector Axis = PortOutwardAxis.GetSafeNormal();
		if (Axis.IsZero() || !IsFiniteVector(PortOrigin))
		{
			return std::numeric_limits<double>::quiet_NaN();
		}
		if (!Bounds.IsValid)
		{
			return 0.0;
		}

		// The support of a box along a unit axis: the centre's projection plus the half-extents, each
		// weighted by how much of the axis lies along it. A NaN anywhere in the box carries through,
		// and FMath::Max(0, NaN) answers the NaN, so a broken box stays red.
		const FVector Centre = Bounds.GetCenter();
		const FVector Extent = Bounds.GetExtent();
		const double Support = FVector::DotProduct(Centre - PortOrigin, Axis)
			+ FMath::Abs(Axis.X) * Extent.X
			+ FMath::Abs(Axis.Y) * Extent.Y
			+ FMath::Abs(Axis.Z) * Extent.Z;

		return FMath::Max(0.0, Support);
	}

	double BoxReachOutside(const FBox& Inner, const FBox& Envelope)
	{
		if (!Inner.IsValid || !Envelope.IsValid || !IsFiniteBox(Inner) || !IsFiniteBox(Envelope))
		{
			return std::numeric_limits<double>::quiet_NaN();
		}

		double Worst = 0.0;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Worst = FMath::Max(Worst, Envelope.Min[Axis] - Inner.Min[Axis]);
			Worst = FMath::Max(Worst, Inner.Max[Axis] - Envelope.Max[Axis]);
		}
		return Worst;
	}

	bool IsRedAtPrintedResolution(double Metres)
	{
		// Negated for CanDock's reason: a NaN fails every comparison, and "not under the threshold"
		// is the spelling that makes it red.
		return !(Metres < RedAtOrAboveMetres);
	}
```

- [ ] **Step 5: Собрать и прогнать — должно пройти**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log < /dev/null
grep -c "Test Completed. Result={Success}" $L   # N₀ + 8 (130 при N₀ = 122, база после ревью)
grep -c "Test Completed. Result={Fail}" $L      # 0
```

Сразу после зелёного прогона, до первого мутанта, снять эталон двух файлов, которые будут мутироваться. Второй `mkdir` стоит без `-p` намеренно. Если каталог уже есть, эталон снят раньше, и копировать поверх него, возможно с мутантом внутри, нельзя. Тогда `mkdir` падает, `cp` не выполняется, и это стоп.

```bash
mkdir -p Saved/Mutants && mkdir Saved/Mutants/task7 && cp Source/StarForgeCore/Private/DockingMath.cpp Source/StarForgeCore/Public/DockingMath.h Saved/Mutants/task7/
ls Saved/Mutants/task7    # DockingMath.cpp  DockingMath.h
```

- [ ] **Step 6: Коммит**

Коммит — здесь или после шага 8. Откат мутантов от него не зависит: шаг 7 откатывает копией из `Saved/Mutants/task7`, а не git (Global Constraints).

```bash
git status --short
git add Source/StarForgeCore/Public/DockingMath.h Source/StarForgeCore/Private/DockingMath.cpp Source/StarForgeCore/Private/Tests/StationKitDockingTest.cpp
git diff --cached --name-only
git commit -F - <<'MSG'
feat(core): measure a box past a port's plane and out of its envelope, to the printed millimetre

Rules 1 and 2 of the station kit are judged by three lengths in the station's
own line: what physical reaches past a port's plane, what interior volume does,
and how far a volume leaves the walls meant to enclose it. All three are box
arithmetic, and a wrong one prints a calm 0.000 over a broken station.

The support function and not the centre, because a box is past a plane by its
furthest corner, and it is tested on a turned axis where the two part.

Red starts where the printed figure stops being 0.000. Exact zero would paint a
turned station red on a healthy build; the kit builds on whole centimetres, so
the smallest real protrusion is twenty times the threshold.
MSG
```

- [ ] **Step 7: Убить каждую проверку мутантом**

Мутант вносится **по одному**. Затем сборка, прогон подветки `StarForge.Core.Docking`, запись упавших тестов и откат копией из `Saved/Mutants/task7` (ниже). Мутант, который никого не убил, — проверка, которую никто не проверяет, и это находка о тестах.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core.Docking;Quit" -unattended -nopause -nullrhi -nosplash -log < /dev/null
grep "Result={Fail}" $L | sed 's/.*Name={\([^}]*\)}.*/\1/'
```

| № | Мутант (в `DockingMath.cpp`) | Что написать вместо | Кто обязан упасть |
|---|---|---|---|
| K1 | ось со знаком минус | `const FVector Axis = -PortOutwardAxis.GetSafeNormal();` | `BoundsPastPortPlaneIsZeroWhenFlush` |
| K2 | опорная через `min` | в `Support` заменить три `+ FMath::Abs(…) * Extent.…` на `-` | `BoundsPastPortPlaneMeasuresAlongTheAxis` |
| K3 | проекция центра | `const double Support = FVector::DotProduct(Centre - PortOrigin, Axis);` | `BoundsPastPortPlaneMeasuresAlongTheAxis` |
| K4 | снято ограничение нулём | `return Support;` | `BoundsPastPortPlaneIgnoresWhatIsBehind` |
| K5 | одна наибольшая компонента | `const int32 I = FMath::Abs(Axis.X) >= FMath::Abs(Axis.Y) ? (FMath::Abs(Axis.X) >= FMath::Abs(Axis.Z) ? 0 : 2) : (FMath::Abs(Axis.Y) >= FMath::Abs(Axis.Z) ? 1 : 2); const double Support = FVector::DotProduct(Centre - PortOrigin, Axis) + FMath::Abs(Axis[I]) * Extent[I];` | `BoundsPastPortPlaneUsesTheSupportOnATurnedAxis` |
| K6 | порог — точный ноль | `return !(Metres <= 0.0);` | `BoundsPastPortPlaneReadsZeroAtThePrintedResolution` (нанометр краснеет) |
| K7 | порог выше сантиметра | в `DockingMath.h`: `RedAtOrAboveMetres = 0.02;` | `BoundsPastPortPlaneReadsZeroAtThePrintedResolution` (сантиметр не краснеет) |
| K8 | выход от центра объёма | `Worst = FMath::Max(Worst, Envelope.Min[Axis] - Inner.GetCenter()[Axis]); Worst = FMath::Max(Worst, Inner.GetCenter()[Axis] - Envelope.Max[Axis]);` | `BoxReachOutsideIsZeroInside` (ловушка −12 даёт 0 вместо 0.200) |
| K9 | через `min` | обе строки цикла: `FMath::Min` вместо `FMath::Max` | `BoxReachOutsideTakesTheLargestExcess` |
| K10 | три стороны из шести | удалить строку `Worst = FMath::Max(Worst, Inner.Max[Axis] - Envelope.Max[Axis]);` | `BoxReachOutsideTakesTheLargestExcess` |
| K11 | зазор до центра бокса станции | в `ReleaseStandoff` тело `GapSq`: `return FVector::DistSquared(HullCentre + T * Dir, StationBounds.GetCenter());` | `ReleaseStandoffGrowsByTheProtrusion` (и существующие тесты отхода) |
| K12 | пик через 4d/T² | в `SmoothstepPeakAcceleration`: `return 4.0 * Distance / (Seconds * Seconds);` | `ReleaseStandoffGrowsByTheProtrusion` (и `SmoothstepPeakAcceleration`) |

Откат после каждого — копией, и следующий мутант только после `restored`:

```bash
cp Saved/Mutants/task7/DockingMath.cpp Source/StarForgeCore/Private/DockingMath.cpp
cp Saved/Mutants/task7/DockingMath.h Source/StarForgeCore/Public/DockingMath.h
cmp Saved/Mutants/task7/DockingMath.cpp Source/StarForgeCore/Private/DockingMath.cpp && cmp Saved/Mutants/task7/DockingMath.h Source/StarForgeCore/Public/DockingMath.h && echo restored
```

Таблица «ожидалось / упало на самом деле» переносится в бланк точки 11. Превышение ожидания (упал ещё кто-то) — не находка. Недолёт (названный тест устоял) — находка, и задача не закрыта, пока тест не дополнен. Дополняется `StationKitDockingTest.cpp`, а не `DockingMath`, так что эталон остаётся в силе.

- [ ] **Step 8: Вернуть дерево и перепроверить**

```bash
cp Saved/Mutants/task7/DockingMath.cpp Source/StarForgeCore/Private/DockingMath.cpp
cp Saved/Mutants/task7/DockingMath.h Source/StarForgeCore/Public/DockingMath.h
cmp Saved/Mutants/task7/DockingMath.cpp Source/StarForgeCore/Private/DockingMath.cpp && cmp Saved/Mutants/task7/DockingMath.h Source/StarForgeCore/Public/DockingMath.h && echo restored   # restored
grep -c "BoundsPastPortPlane\|BoxReachOutside\|IsRedAtPrintedResolution" Source/StarForgeCore/Private/DockingMath.cpp   # >= 3: функции задачи на месте
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log < /dev/null
grep -c "Test Completed. Result={Success}" $L   # N₀ + 8
grep -n "Engine" Source/StarForgeCore/StarForgeCore.Build.cs   # только строка комментария "Intentionally no \"Engine\" dependency"
rm -rf Saved/Mutants/task7
```

Если шаг 6 уже закоммичен, `git status --short` не показывает изменённых файлов под `Source/`. Если нет, он показывает ровно три файла задачи.

---

### Task 8: `ASFKitStation` и `ASFBerthStation` — блок M01 как станция

Класс, который собирает себя в конструкторе, как `ASFTestStation`, только из таблиц §6.3–§6.5. Строки о себе он в этой задаче ещё не пишет — это задача 9; здесь проверяется, что станция стоит, корабль к ней пристыковывается, человек переходит, а отход меряется от её бокса.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFKitStation.h`
- Create: `Source/StarForge/Private/Ship/SFKitStation.cpp`
- Create: `Source/StarForge/Public/Ship/SFBerthStation.h`
- Create: `Source/StarForge/Private/Ship/SFBerthStation.cpp`
- Create: `Content/Places/DA_Place_Berth.uasset`

**Interfaces:**
- Потребляет: `ISFDockingStation` (задача 5), `USFDockingPortComponent`, `USFInteriorVolumeComponent`, `USFShipFrameComponent` (без правок), `StarForge::Units::CmPerMetre`.
- Отдаёт задачам 9, 10, 13:
  - `UCLASS(Abstract) class ASFKitStation : public AActor, public ISFDockingStation` с `protected: void BuildBlock(const FVector& AssemblyOriginCm);`
  - `struct FSFKitFaceSource { int32 Collider; bool bMaxSide; };`, `struct FSFKitVolumeRecord { int32 Volume; const TCHAR* EnvelopeWord; FVector AuthoredCentreCm; FVector AuthoredExtentCm; FVector OutwardAxis; TArray<FSFKitFaceSource> Faces[6]; };`
  - `UCLASS() class ASFBerthStation : public ASFKitStation` — `/Script/StarForge.SFBerthStation`
  - компоненты с именами `Block{i}Base` … `Block{i}BoomWallXPos`, `Hall{i}`, `Boom{i}`, `Port{i}`, `Block{i}Light`, `Block{i}MarkerSphere|Cone|Cylinder`

- [ ] **Step 1: «Красный» шаг — класса нет, ассет не создаётся**

```bash
KIT_PLACES_GROUP=berth "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "no class /Script/StarForge.SFBerthStation\|Python script executed" $L   # no class ...; executed with errors
```

- [ ] **Step 2: Заголовок базы**

`Source/StarForge/Public/Ship/SFKitStation.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ship/SFDockingStation.h"
#include "SFKitStation.generated.h"

class UPointLightComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USFDockingPortComponent;
class USFInteriorVolumeComponent;
class USFShipFrameComponent;

/** One face of an interior volume's envelope: which collider it is read from, and which side of its box. */
struct FSFKitFaceSource
{
	int32 Collider = INDEX_NONE;
	bool bMaxSide = false;
};

/**
 * An interior volume of a kit station and what the Station rig: line needs to judge it: the walls its
 * envelope is read from, face by face in the order XMin, XMax, YMin, YMax, ZMin, ZMax; the word the
 * line calls that envelope; and the axis sf.StationVolumeReach grows it along -- its block's port axis.
 * A face with several sources takes the tightest of them.
 */
struct FSFKitVolumeRecord
{
	int32 Volume = INDEX_NONE;
	const TCHAR* EnvelopeWord = TEXT("");
	FVector AuthoredCentreCm = FVector::ZeroVector;
	FVector AuthoredExtentCm = FVector::ZeroVector;
	FVector OutwardAxis = FVector::ZeroVector;
	TArray<FSFKitFaceSource> Faces[6];
};

/**
 * A station assembled from the kit's standard blocks (station-kit spec 6.1).
 *
 * Abstract, so it cannot be picked as a place's carrier: a kit station is a berth, a pier core, and
 * whatever the kit grows next -- each a subclass whose constructor calls BuildBlock. Stations are
 * described by code (spec 10.2 of the second phase, owner's decision 12), so a new assembly costs a
 * class and a rebuild, and this is the one place its blocks are built.
 *
 * A shared base does not contradict SFTestStation.h's refusal of one: that refused a base for the
 * ship and the station, which share a shape and not a life. Kit stations share both. ASFTestStation
 * stays outside the hierarchy -- every recorded figure was taken on it -- and only declares the same
 * interface.
 *
 * No physics body and no tick, like the rig station: every collider is its own kinematic body, and
 * the station hears about its ports through OnDockingPortChanged instead of looking.
 */
UCLASS(Abstract)
class STARFORGE_API ASFKitStation : public AActor, public ISFDockingStation
{
	GENERATED_BODY()

public:
	ASFKitStation();

	virtual void GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const override;
	virtual USFShipFrameComponent* GetStationFrame() const override { return Frame; }
	virtual void OnDockingPortChanged(USFDockingPortComponent* Port) override;

	USFShipFrameComponent* GetFrameComponent() const { return Frame; }

protected:
	/**
	 * Builds one M01 berth block. AssemblyOriginCm is the block's origin in spec.py's assembly frame,
	 * in CENTIMETRES (B_K3core: -3000 and +3000 along X). The first block built fixes the actor's root
	 * at the centre of its hall (spec 6.2); every later block is placed relative to the first.
	 * Constructor only: it creates default subobjects.
	 */
	void BuildBlock(const FVector& AssemblyOriginCm);

private:
	/** A box collider between two corners, in the actor's frame: a visible cube, or a pane with no mesh. */
	int32 AddCollider(FName Name, const FVector& MinCm, const FVector& MaxCm, bool bVisible);

	/** An interior volume between two corners, recorded with its authored size and outward axis. */
	int32 AddVolume(FName Name, const FVector& MinCm, const FVector& MaxCm, const TCHAR* EnvelopeWord, const FVector& OutwardAxis);

	UStaticMeshComponent* AddMarker(FName Name, UStaticMesh* Mesh, const FVector& LocationCm);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFShipFrameComponent> Frame;

	UPROPERTY()
	TArray<TObjectPtr<UPrimitiveComponent>> Colliders;

	UPROPERTY()
	TArray<TObjectPtr<USFInteriorVolumeComponent>> Volumes;

	/** In build order: GetDockingPorts hands them out in this order, and ties are broken by it. */
	UPROPERTY()
	TArray<TObjectPtr<USFDockingPortComponent>> Ports;

	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Markers;

	/** Parallel to Volumes. */
	TArray<FSFKitVolumeRecord> VolumeRecords;

	/** Each block's origin (spec.py's frame B) in the actor's frame, in build order. */
	TArray<FVector> BlockOriginsCm;

	bool bHasFirstBlock = false;
	FVector FirstAssemblyOriginCm = FVector::ZeroVector;
};
```

- [ ] **Step 3: Реализация базы**

`Source/StarForge/Private/Ship/SFKitStation.cpp`:

```cpp
#include "Ship/SFKitStation.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFInteriorVolumeComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"
#include "UObject/ConstructorHelpers.h"
#include "Units.h"

namespace
{
/**
 * Nested and named for the unity build: every file's anonymous namespace is one namespace in this
 * module, and SFTestShip.cpp already owns CubeEdgeCm and MarkerUniformScale at its top level. Inside
 * a function with `using namespace KitStation;` those two are therefore written qualified.
 */
namespace KitStation
{
	/** One engine cube is 100 cm on a side, so a mesh scale equals metres. */
	constexpr double CubeEdgeCm = 100.0;

	// The M01 berth block in spec.py's block frame B, in WHOLE CENTIMETRES: origin at the centre of the
	// bottom face, X along the berth face, -Y out of it, Z up. Every figure is spec.py's -- BLOCK, HALL,
	// HALL_WINDOW, BOOM_INNER, BOOM_WALL, BOOM_AXIS_X, HALL_FLOOR_Z -- or derived below. Whole
	// centimetres because control N6's tie is exact only if the ports are (station-kit spec 7.5), and
	// because the red threshold of the Station rig: line is derived from that grain (spec 6.8).
	constexpr double BlockHalfXCm = 3000.0;         // BLOCK x 60 [A]
	constexpr double BlockHalfYCm = 1200.0;         // BLOCK y 24 [A]
	constexpr double BlockHeightCm = 3200.0;        // BLOCK z 32 [A]
	constexpr double DeckZCm = 1200.0;              // HALL_FLOOR_Z: deck 4 [I]
	constexpr double HallTopZCm = 2000.0;           // HALL z 12..20 [A]
	constexpr double HallXNegCm = -1962.0;          // HALL x -19.62 [A]
	constexpr double HallXPosCm = -362.0;           // HALL x -3.62 [A]
	constexpr double HallBackYCm = 0.0;             // HALL y ..0 [A]
	constexpr double WallCm = 20.0;                 // BOOM_WALL 0.2, and the rig's panel thickness
	constexpr double WindowSillZCm = 1300.0;        // HALL_WINDOW sill_z [A]
	constexpr double WindowHeadZCm = 1900.0;        // HALL_WINDOW head_z [A]
	constexpr double BoomAxisXCm = -1162.0;         // BOOM_AXIS_X [I]
	constexpr double BoomHalfWidthCm = 100.0;       // BOOM_INNER 2.0 / 2
	constexpr double BoomInnerHeightCm = 250.0;     // BOOM_INNER 2.5
	constexpr double BoomLengthCm = 1200.0;         // BOOM_LEN 12 [I]

	// Derived, not authored.
	constexpr double FrontOuterYCm = -BlockHalfYCm;                         // -1200: the berth face
	constexpr double FrontInnerYCm = FrontOuterYCm + WallCm;                // -1180: inside the envelope (spec 6.3)
	constexpr double PortPlaneYCm = FrontOuterYCm - BoomLengthCm;           // -2400
	constexpr double BoomTopZCm = DeckZCm + BoomInnerHeightCm;             // 1450
	constexpr double PortZCm = DeckZCm + BoomInnerHeightCm * 0.5;           // 1325: floor + half the hatch, as both rig ports
	constexpr double DoorXNegCm = BoomAxisXCm - BoomHalfWidthCm;            // -1262
	constexpr double DoorXPosCm = BoomAxisXCm + BoomHalfWidthCm;            // -1062

	/** The hall's centre in B, which is the actor's root (spec 6.2): a pawn spawned at a place's origin stands in Hall0. */
	const FVector HallCentreB(
		(HallXNegCm + HallXPosCm) * 0.5, (FrontInnerYCm + HallBackYCm) * 0.5, (DeckZCm + HallTopZCm) * 0.5);   // (-1162, -590, 1600)

	// spec.py sets neither the light nor the markers; spec 6.3 asks for them derived as the rig does it.
	constexpr double MarkerUniformScale = 0.8;       // the rig station's: the size says which carrier you are in
	constexpr double MarkerWallClearanceXCm = 140.0; // the rig: inner half-X 590 less its marker offset 450
	constexpr double MarkerWallClearanceYCm = 90.0;  // the rig: inner half-Y 390 less its marker offset 300
	constexpr double LightIntensityCandelas = 2000.0;// the rig's, with auto-exposure off
}
}

ASFKitStation::ASFKitStation()
{
	// Nothing to do per frame. The frame component ticks itself to sample the transform, which is
	// constant here, and the station hears about its ports through OnDockingPortChanged.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));
}

void ASFKitStation::BuildBlock(const FVector& AssemblyOriginCm)
{
	using namespace KitStation;

	if (!bHasFirstBlock)
	{
		bHasFirstBlock = true;
		FirstAssemblyOriginCm = AssemblyOriginCm;
	}

	const int32 Block = BlockOriginsCm.Num();

	// The block's origin in the actor's frame: its offset from the first block, less the first hall's
	// centre, which is where the root is. Whole centimetres in, whole centimetres out.
	const FVector OriginCm = (AssemblyOriginCm - FirstAssemblyOriginCm) - HallCentreB;
	BlockOriginsCm.Add(OriginCm);

	const auto Name = [Block](const TCHAR* Part) { return FName(*FString::Printf(TEXT("Block%d%s"), Block, Part)); };
	const auto B = [&OriginCm](double X, double Y, double Z) { return OriginCm + FVector(X, Y, Z); };

	// The fifteen colliders of spec 6.3, in its table's order. The outer faces lie ON the block's
	// envelope, so two blocks side by side touch and do not overlap; the walked floors put their top
	// face on the deck line, which is the docked ship's cabin floor; the channel's walls stand outside
	// its 2.0 x 2.5 bore, as the rig tunnel's do; and the channel's end faces lie on the port plane.
	const int32 Base = AddCollider(Name(TEXT("Base")),
		B(-BlockHalfXCm, -BlockHalfYCm, 0.0), B(BlockHalfXCm, BlockHalfYCm, DeckZCm), true);
	const int32 Roof = AddCollider(Name(TEXT("Roof")),
		B(-BlockHalfXCm, -BlockHalfYCm, HallTopZCm), B(BlockHalfXCm, BlockHalfYCm, BlockHeightCm), true);
	const int32 SideXNeg = AddCollider(Name(TEXT("SideXNeg")),
		B(-BlockHalfXCm, -BlockHalfYCm, DeckZCm), B(HallXNegCm, BlockHalfYCm, HallTopZCm), true);
	const int32 SideXPos = AddCollider(Name(TEXT("SideXPos")),
		B(HallXPosCm, -BlockHalfYCm, DeckZCm), B(BlockHalfXCm, BlockHalfYCm, HallTopZCm), true);
	const int32 Back = AddCollider(Name(TEXT("Back")),
		B(HallXNegCm, HallBackYCm, DeckZCm), B(HallXPosCm, BlockHalfYCm, HallTopZCm), true);
	AddCollider(Name(TEXT("FrontSillXNeg")),
		B(HallXNegCm, FrontOuterYCm, DeckZCm), B(DoorXNegCm, FrontInnerYCm, WindowSillZCm), true);
	AddCollider(Name(TEXT("FrontSillXPos")),
		B(DoorXPosCm, FrontOuterYCm, DeckZCm), B(HallXPosCm, FrontInnerYCm, WindowSillZCm), true);
	const int32 FrontHead = AddCollider(Name(TEXT("FrontHead")),
		B(HallXNegCm, FrontOuterYCm, WindowHeadZCm), B(HallXPosCm, FrontInnerYCm, HallTopZCm), true);
	AddCollider(Name(TEXT("PaneXNeg")),
		B(HallXNegCm, FrontOuterYCm, WindowSillZCm), B(DoorXNegCm, FrontInnerYCm, WindowHeadZCm), false);
	AddCollider(Name(TEXT("PaneXPos")),
		B(DoorXPosCm, FrontOuterYCm, WindowSillZCm), B(HallXPosCm, FrontInnerYCm, WindowHeadZCm), false);
	AddCollider(Name(TEXT("PaneOverDoor")),
		B(DoorXNegCm, FrontOuterYCm, BoomTopZCm), B(DoorXPosCm, FrontInnerYCm, WindowHeadZCm), false);
	const int32 BoomFloor = AddCollider(Name(TEXT("BoomFloor")),
		B(DoorXNegCm, PortPlaneYCm, DeckZCm - WallCm), B(DoorXPosCm, FrontOuterYCm, DeckZCm), true);
	const int32 BoomCeiling = AddCollider(Name(TEXT("BoomCeiling")),
		B(DoorXNegCm, PortPlaneYCm, BoomTopZCm), B(DoorXPosCm, FrontOuterYCm, BoomTopZCm + WallCm), true);
	const int32 BoomWallXNeg = AddCollider(Name(TEXT("BoomWallXNeg")),
		B(DoorXNegCm - WallCm, PortPlaneYCm, DeckZCm), B(DoorXNegCm, FrontOuterYCm, BoomTopZCm), true);
	const int32 BoomWallXPos = AddCollider(Name(TEXT("BoomWallXPos")),
		B(DoorXPosCm, PortPlaneYCm, DeckZCm), B(DoorXPosCm + WallCm, FrontOuterYCm, BoomTopZCm), true);

	// The port on the channel's outboard mouth, on the port plane. A yaw of -90 takes local +X onto -Y
	// and leaves +Z alone, exactly the rig station's port (SFTestStation.cpp 139-142).
	USFDockingPortComponent* Port = CreateDefaultSubobject<USFDockingPortComponent>(FName(*FString::Printf(TEXT("Port%d"), Block)));
	Port->SetupAttachment(Root);
	Port->SetRelativeLocation(B(BoomAxisXCm, PortPlaneYCm, PortZCm));
	Port->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	Ports.Add(Port);
	const FVector OutwardAxis(0.0, -1.0, 0.0);

	// The hall first, then the boom: the order the Station rig: line reads them in (spec 6.8). The
	// hall is taken by its clear depth, 11.8 m, because the front wall stands inside the envelope.
	const int32 Hall = AddVolume(FName(*FString::Printf(TEXT("Hall%d"), Block)),
		B(HallXNegCm, FrontInnerYCm, DeckZCm), B(HallXPosCm, HallBackYCm, HallTopZCm), TEXT("hall"), OutwardAxis);
	VolumeRecords[Hall].Faces[0].Add({SideXNeg, true});
	VolumeRecords[Hall].Faces[1].Add({SideXPos, false});
	VolumeRecords[Hall].Faces[2].Add({FrontHead, true});      // the front wall's hall-side face, -11.8
	VolumeRecords[Hall].Faces[3].Add({Back, false});
	VolumeRecords[Hall].Faces[4].Add({Base, true});
	VolumeRecords[Hall].Faces[5].Add({Roof, false});

	// The boom is exactly its channel and no wider: outside the channel is open space where a 48.9 m
	// ship stands beside the boom (rule 2, form 1). Built from the port plane, the constant the port
	// stands on, so a port and a channel that disagree show in the line (spec 6.4). Its +Y end is the
	// hall-side face of the front wall: the door through that wall belongs to the boom.
	const int32 Boom = AddVolume(FName(*FString::Printf(TEXT("Boom%d"), Block)),
		B(DoorXNegCm, PortPlaneYCm, DeckZCm), B(DoorXPosCm, FrontInnerYCm, BoomTopZCm), TEXT("channel"), OutwardAxis);
	VolumeRecords[Boom].Faces[0].Add({BoomWallXNeg, true});
	VolumeRecords[Boom].Faces[1].Add({BoomWallXPos, false});
	// Its -Y end is read from the channel colliders' OUTER ends, not from the port plane: the tightest
	// of the four, so a channel shorter than its port leaves the boom's volume sticking out (spec 6.8).
	const int32 ChannelEnds[] = { BoomFloor, BoomCeiling, BoomWallXNeg, BoomWallXPos };
	for (const int32 End : ChannelEnds)
	{
		VolumeRecords[Boom].Faces[2].Add({End, false});
	}
	VolumeRecords[Boom].Faces[3].Add({FrontHead, true});
	VolumeRecords[Boom].Faces[4].Add({BoomFloor, true});
	VolumeRecords[Boom].Faces[5].Add({BoomCeiling, false});

	// Light at the hall's centre. Its reach is no shorter than the distance to the port, because a
	// 12 m channel is darker than the rig's 2 m tunnel (spec 6.3); the hall's own far corner is nearer,
	// 10.7 m. 18.31 m, rounded up to the next whole metre: 1900 cm.
	const FVector HallCentreCm = B(HallCentreB.X, HallCentreB.Y, HallCentreB.Z);
	UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name(TEXT("Light")));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(HallCentreCm);
	Light->SetMobility(EComponentMobility::Movable);
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->SetIntensity(static_cast<float>(LightIntensityCandelas));
	const double ToPortCm = FVector::Dist(HallCentreCm, Port->GetRelativeLocation());
	Light->AttenuationRadius = static_cast<float>(
		FMath::CeilToDouble(ToPortCm / StarForge::Units::CmPerMetre) * StarForge::Units::CmPerMetre);
	Light->SetCastShadows(false);
	Lights.Add(Light);

	// The rig station's three shapes at its size: sphere +X, cone +Y, cylinder -X. Walking in from the
	// boom is walking towards the cone, as walking out of the ship's hatch is on the rig.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const double MarkerXCm = (HallXPosCm - HallXNegCm) * 0.5 - MarkerWallClearanceXCm;      // 660
	const double MarkerYCm = (HallBackYCm - FrontInnerYCm) * 0.5 - MarkerWallClearanceYCm;  // 500
	AddMarker(Name(TEXT("MarkerSphere")), SphereMesh.Object, HallCentreCm + FVector(MarkerXCm, 0.0, 0.0));
	AddMarker(Name(TEXT("MarkerCone")), ConeMesh.Object, HallCentreCm + FVector(0.0, MarkerYCm, 0.0));
	AddMarker(Name(TEXT("MarkerCylinder")), CylinderMesh.Object, HallCentreCm + FVector(-MarkerXCm, 0.0, 0.0));
}

int32 ASFKitStation::AddCollider(FName Name, const FVector& MinCm, const FVector& MaxCm, bool bVisible)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	const FVector CentreCm = (MinCm + MaxCm) * 0.5;
	const FVector SizeCm = MaxCm - MinCm;

	UPrimitiveComponent* Collider = nullptr;
	if (bVisible)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetRelativeScale3D(SizeCm / KitStation::CubeEdgeCm);
		if (CubeMesh.Succeeded())
		{
			Mesh->SetStaticMesh(CubeMesh.Object);
		}
		Collider = Mesh;
	}
	else
	{
		// A pane: a box with no mesh at all, which is rule 5 in its plainest form. It blocks exactly
		// as a wall does and draws nothing -- and a hall whose windows draw nothing is also the
		// cheapest answer to checkpoint 7's blind cube: the ship is visible through them.
		UBoxComponent* Pane = CreateDefaultSubobject<UBoxComponent>(Name);
		Pane->SetBoxExtent(SizeCm * 0.5);
		Pane->SetHiddenInGame(true);
		Collider = Pane;
	}

	Collider->SetupAttachment(Root);
	Collider->SetRelativeLocation(CentreCm);
	Collider->SetMobility(EComponentMobility::Movable);
	Collider->SetCollisionObjectType(ECC_WorldStatic);
	Collider->SetCollisionResponseToAllChannels(ECR_Block);

	// Query AND physics, each its own kinematic collider, for the reason the rig station's panels are
	// (SFTestStation.cpp 202-224, finding F1): a hull must meet the walls and stop at them. There is no
	// simulating body on this actor for them to be welded into.
	Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	return Colliders.Add(Collider);
}

int32 ASFKitStation::AddVolume(
	FName Name, const FVector& MinCm, const FVector& MaxCm, const TCHAR* EnvelopeWord, const FVector& OutwardAxis)
{
	USFInteriorVolumeComponent* Volume = CreateDefaultSubobject<USFInteriorVolumeComponent>(Name);
	Volume->SetupAttachment(Root);

	FSFKitVolumeRecord Record;
	Record.AuthoredCentreCm = (MinCm + MaxCm) * 0.5;
	Record.AuthoredExtentCm = (MaxCm - MinCm) * 0.5;
	Record.EnvelopeWord = EnvelopeWord;
	Record.OutwardAxis = OutwardAxis;

	Volume->SetRelativeLocation(Record.AuthoredCentreCm);
	Volume->SetBoxExtent(Record.AuthoredExtentCm);

	Record.Volume = Volumes.Add(Volume);
	return VolumeRecords.Add(MoveTemp(Record));
}

UStaticMeshComponent* ASFKitStation::AddMarker(FName Name, UStaticMesh* Mesh, const FVector& LocationCm)
{
	UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Marker->SetupAttachment(Root);
	Marker->SetRelativeLocation(LocationCm);
	Marker->SetRelativeScale3D(FVector(KitStation::MarkerUniformScale));
	Marker->SetMobility(EComponentMobility::Movable);

	// Something to read direction by; it must not block a walk, and it must not join the release's
	// box: rule 5, a visual with collision would move the standoff in silence.
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Mesh)
	{
		Marker->SetStaticMesh(Mesh);
	}

	Markers.Add(Marker);
	return Marker;
}

void ASFKitStation::GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const
{
	Out.Reset();
	for (const TObjectPtr<USFDockingPortComponent>& Port : Ports)
	{
		Out.Add(Port.Get());
	}
}

void ASFKitStation::OnDockingPortChanged(USFDockingPortComponent* Port)
{
	// Nothing is said about ports until the station has a line to say it in; that line is the
	// Station rig: line, and it replaces this body (station-kit plan, task 9).
}
```

- [ ] **Step 4: Причальная секция**

`Source/StarForge/Public/Ship/SFBerthStation.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Ship/SFKitStation.h"
#include "SFBerthStation.generated.h"

/**
 * Step 1 of the station kit: one M01 berth block is a station (station-kit spec 1, 6). spec.py's
 * A_M01: one berth block at the assembly origin, yaw 0.
 */
UCLASS()
class STARFORGE_API ASFBerthStation : public ASFKitStation
{
	GENERATED_BODY()

public:
	ASFBerthStation();
};
```

`Source/StarForge/Private/Ship/SFBerthStation.cpp`:

```cpp
#include "Ship/SFBerthStation.h"

ASFBerthStation::ASFBerthStation()
{
	BuildBlock(FVector::ZeroVector);
}
```

- [ ] **Step 5: Собрать и создать `DA_Place_Berth`**

Ассет создаётся здесь, а не в пункте 10 §9: иначе станции нечем попасть в мир на её же задаче.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
KIT_PLACES_GROUP=berth "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET read /Game/Places/DA_Place_Berth\|Python script executed" $L
```

Ожидается `KIT-ASSET read /Game/Places/DA_Place_Berth: place 'Berth', carriers [SFBerthStation 'Station' at (0.00 0.00 0.00)], ship start … DOCKED at (0.00 0.00 0.00), occupant (0.00 0.00 0.00), …` и `Python script executed successfully`.

- [ ] **Step 6: Станция стоит, корабль стыкуется, отход меряется от её бокса**

```bash
kit DA_Place_Berth 60 25 "sf.FlightLog 1" task8-berth-60
grep -cF "Place carrier: 'Station' of class SFBerthStation in place 'Berth' at place-local (0.0 0.0 0.0) m" $L   # 1
grep -cF "Docking port on Station at hull-local (0 -1810 -275) cm, out along (0.00 -1.00 0.00), deck up (0.00 0.00 1.00)." $L   # 1
grep -c "belongs to neither a docking station" $L                  # 0
grep -cF "Dock: [Ship] docked." $L                                 # 1
grep -cF "Frame handover: Station -> Ship, first carry moved him 0.000 cm" $L   # 1
grep -o "Shipboard drift:.*" $L | tail -1                          # peak drift 0.000 cm ... skipped carries 0 ... footing intact

kit DA_Place_Berth 60 40 "sf.FlightLog 1, sf.Dock 2" task8-berth-cycle-60
grep -cF "Dock: [Ship] releasing, 2.93 m along the port axis over 1.0 s, peak 17.6 m/s2." $L   # 1
grep -cF "Dock: [Ship] docked." $L                                 # 2
```

Плюс общие ворота раздела «Как проверяется» на обоих журналах. `2.93` здесь — отход, измеренный от бокса **станции набора**: он равен рижскому только если торцы стрелы кончаются ровно на плоскости порта, а бокс по X и Z охватывает корпус (правило 1). Строка порта `(0 -1810 -275)` — это `Port0` из таблицы «Числа».

Журналы задачи — проверка, не улики: `rm docs/superpowers/notes/evidence/$(date +%F)-task8-*.log`.

- [ ] **Step 7: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFKitStation.h Source/StarForge/Private/Ship/SFKitStation.cpp Source/StarForge/Public/Ship/SFBerthStation.h Source/StarForge/Private/Ship/SFBerthStation.cpp Content/Places/DA_Place_Berth.uasset
git diff --cached --name-only
git commit -F - <<'MSG'
feat(station): build the kit's M01 berth block, and make one of them a station

Step 1 of the station kit: a berth section alone is a station. It is today's
rig station grown to the Krishna's scale -- a 60 x 24 x 32 m block, a 12 m boom
ending on its port plane, a hall -- and it needs no new mechanism of motion,
docking or handover.

The block is built from spec.py's table in whole centimetres, fifteen box
colliders, each its own kinematic body as the rig station's panels are since F1.
The panes are boxes with no mesh. The boom's volume is exactly its channel,
because outside the channel is space where a 48.9 m ship stands. The root sits at
the centre of the first hall, so a pawn spawned at a place's origin binds to the
station by volume and not by search.

The release measured from this station's box reads 2.93 m, the rig's figure,
which it can only do if the channel's end faces lie on the port plane.
MSG
```

---

### Task 9: Строка `Station rig:`

Строка, в которой станция говорит о себе всё, что проверяет правила 1 и 2, и каждое число прочитано обратно из мира (§6.8). Пишется для каждой станции, в порядке она или нет: строка, появляющаяся только при беде, неотличима от прибора, который не запустился.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFKitStation.h` (публичное `BeginPlay`; приватные методы)
- Modify: `Source/StarForge/Private/Ship/SFKitStation.cpp` (include; помощники в `KitStation`; новое тело `OnDockingPortChanged`; новые методы)

**Interfaces:**
- Потребляет: `BoundsPastPortPlane`, `BoxReachOutside`, `IsRedAtPrintedResolution` (задача 7); `USFDockingComponent::StationPhysicalBoundsLocalCm`, `MeasureRelease`, `GetState`, `DistanceMetres`, `GetCaptureSeconds` (задача 6); `USFDockingPortComponent::GetHolder` (задача 6); `StarForge::ShipFrame::GravPlateLimitMetersPerSecSq` (только чтение).
- Отдаёт задачам 10, 12, 13: `void ASFKitStation::ReportStationRig()`; строка `Station rig:` в формате §6.8; перепечатка на каждом `OnDockingPortChanged` после `BeginPlay`.

- [ ] **Step 1: «Красный» шаг — строки нет**

```bash
kit DA_Place_Berth 60 10 "sf.FlightLog 1" task9-red-60
grep -c "Station rig:" $L   # 0
rm docs/superpowers/notes/evidence/$(date +%F)-task9-red-60.log
```

- [ ] **Step 2: Объявления**

`Source/StarForge/Public/Ship/SFKitStation.h`, в `public:` после конструктора:

```cpp
	/** Says the Station rig: line once the station is in play (spec 6.8). */
	virtual void BeginPlay() override;

	/**
	 * The Station rig: line: blocks, ports, volumes and physical colliders, the box they make in the
	 * station's own frame, each volume against its envelope, and each port -- what reaches past its
	 * plane, which volume does, and who holds it. Every figure read back from the world. Written at
	 * BeginPlay, on every port change after it, and whenever a control is applied. An upper-case word
	 * is a state that did not hold, and the whole line then goes out as an Error.
	 */
	void ReportStationRig();
```

В `private:`:

```cpp
	/** One port's sentence of the line; sets bInOutRed when anything in it did not hold. */
	FString DescribePort(const USFDockingPortComponent& Port, const FTransform& StationFrameCm, const FBox& StationBoxM,
		const TArray<const UPrimitiveComponent*>& Physical, const TArray<FBox>& VolumeBoxesM, bool& bInOutRed) const;

	/** Who holds the port and in what state; the release only while the holder is docked (spec 6.8). */
	FString DescribeHolder(const USFDockingPortComponent& Port, bool& bInOutRed) const;

	/** A volume's envelope, in metres in the station's frame, read face by face from its walls' boxes. */
	FBox EnvelopeMetres(const FSFKitVolumeRecord& Record, const FTransform& StationFrameCm) const;
```

- [ ] **Step 3: Реализация**

`Source/StarForge/Private/Ship/SFKitStation.cpp`: добавить include `"DockingMath.h"`, `"Ship/SFDockingComponent.h"`, `"ShipFrameMath.h"`. В `namespace KitStation`, в конец:

```cpp
	FVector ToMetres(const FVector& Cm)
	{
		return FVector(StarForge::Units::CmToMetres(Cm.X), StarForge::Units::CmToMetres(Cm.Y), StarForge::Units::CmToMetres(Cm.Z));
	}

	FBox ToMetres(const FBox& Cm)
	{
		return Cm.IsValid ? FBox(ToMetres(Cm.Min), ToMetres(Cm.Max)) : FBox(ForceInit);
	}

	/**
	 * A primitive's box in the station's own frame, in metres: through the same relative transform the
	 * release boxes the station's colliders with (USFDockingComponent::StationPhysicalBoundsLocalCm),
	 * so the line and the release read the same geometry.
	 */
	FBox BoxInFrameMetres(const UPrimitiveComponent& Primitive, const FTransform& StationFrameCm)
	{
		return ToMetres(Primitive.CalcBounds(Primitive.GetComponentTransform().GetRelativeTransform(StationFrameCm)).GetBox());
	}

	/** "-Y" for an axis along one of the station's own, the vector otherwise. */
	FString AxisWord(const FVector& Axis)
	{
		static const TCHAR* const Names[3] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
		for (int32 i = 0; i < 3; ++i)
		{
			if (FMath::IsNearlyEqual(FMath::Abs(Axis[i]), 1.0, 1e-6))
			{
				return FString::Printf(TEXT("%s%s"), Axis[i] > 0.0 ? TEXT("+") : TEXT("-"), Names[i]);
			}
		}
		return FString::Printf(TEXT("(%.2f %.2f %.2f)"), Axis.X, Axis.Y, Axis.Z);
	}

	/**
	 * A coordinate as it is printed to two decimals, with a sign-only zero made plain. A turned
	 * station's round trip through GetRelativeTransform leaves -1e-12 where the builder put zero, and
	 * "-0.00" in a line that is compared character for character is a difference made of nothing.
	 */
	double Shown(double Metres)
	{
		return FMath::Abs(Metres) < 0.005 ? 0.0 : Metres;
	}

	const TCHAR* Plural(int32 Count)
	{
		return Count == 1 ? TEXT("") : TEXT("s");
	}
```

Заменить тело `OnDockingPortChanged` и добавить методы:

```cpp
void ASFKitStation::BeginPlay()
{
	Super::BeginPlay();
	ReportStationRig();
}

void ASFKitStation::OnDockingPortChanged(USFDockingPortComponent* Port)
{
	// Never before BeginPlay: on the place rig the whole start-up docking happens before the world
	// begins play, and the BeginPlay line reads the state as it stands by then (spec 6.8).
	if (HasActorBegunPlay())
	{
		ReportStationRig();
	}
}

void ASFKitStation::ReportStationRig()
{
	using namespace KitStation;

	const FTransform StationFrameCm(GetActorQuat(), GetActorLocation());
	const FBox StationBoxM = ToMetres(USFDockingComponent::StationPhysicalBoundsLocalCm(*this));

	// Counted from the world rather than from the builder's list: a control collider switched on is
	// one of them, and a mesh given collision by mistake would be one too (rule 5).
	TArray<const UPrimitiveComponent*> Physical;
	bool bSimulating = false;
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive)
			{
				continue;
			}
			if (CollisionEnabledHasPhysics(Primitive->GetCollisionEnabled()))
			{
				Physical.Add(Primitive);
			}
			bSimulating |= Primitive->IsSimulatingPhysics();
		}
	}

	bool bRed = !Frame || bSimulating || !StationBoxM.IsValid;

	FString Line = FString::Printf(
		TEXT("Station rig: '%s' (%s), frame %s, %s -- %d block%s, %d port%s, %d interior volume%s, %d physical collider%s "),
		*GetName(), *GetClass()->GetName(),
		Frame ? TEXT("present") : TEXT("MISSING"),
		bSimulating ? TEXT("a SIMULATING BODY") : TEXT("no simulating body"),
		BlockOriginsCm.Num(), Plural(BlockOriginsCm.Num()),
		Ports.Num(), Plural(Ports.Num()),
		Volumes.Num(), Plural(Volumes.Num()),
		Physical.Num(), Plural(Physical.Num()));

	Line += StationBoxM.IsValid
		? FString::Printf(TEXT("boxed in its own frame at X %.2f..%.2f Y %.2f..%.2f Z %.2f..%.2f m; "),
			Shown(StationBoxM.Min.X), Shown(StationBoxM.Max.X), Shown(StationBoxM.Min.Y), Shown(StationBoxM.Max.Y),
			Shown(StationBoxM.Min.Z), Shown(StationBoxM.Max.Z))
		: FString(TEXT("and NOTHING PHYSICAL to box; "));

	// Each volume against the walls that are meant to enclose it: rule 2, forms 1 and 2.
	TArray<FBox> VolumeBoxesM;
	TArray<FString> VolumeTexts;
	for (const FSFKitVolumeRecord& Record : VolumeRecords)
	{
		const USFInteriorVolumeComponent* Volume = Volumes[Record.Volume];
		const FBox VolumeM = BoxInFrameMetres(*Volume, StationFrameCm);
		VolumeBoxesM.Add(VolumeM);

		const double OutM = StarForge::Docking::BoxReachOutside(VolumeM, EnvelopeMetres(Record, StationFrameCm));
		if (!FMath::IsFinite(OutM))
		{
			// BoxReachOutside answers NaN for a broken box on EITHER side (task 7), so the line says which:
			// sent to the walls when the volume is what broke, the reader would look at the wrong thing
			// (amendment P20).
			const bool bVolumeReadable = VolumeM.IsValid
				&& FMath::IsFinite(VolumeM.Min.X) && FMath::IsFinite(VolumeM.Min.Y) && FMath::IsFinite(VolumeM.Min.Z)
				&& FMath::IsFinite(VolumeM.Max.X) && FMath::IsFinite(VolumeM.Max.Y) && FMath::IsFinite(VolumeM.Max.Z);
			bRed = true;
			VolumeTexts.Add(bVolumeReadable
				? FString::Printf(TEXT("%s's %s envelope could NOT BE READ"), *Volume->GetName(), Record.EnvelopeWord)
				: FString::Printf(TEXT("%s itself could NOT BE MEASURED"), *Volume->GetName()));
		}
		else if (StarForge::Docking::IsRedAtPrintedResolution(OutM))
		{
			bRed = true;
			VolumeTexts.Add(FString::Printf(TEXT("%s REACHES %.3f m OUT of its %s"), *Volume->GetName(), OutM, Record.EnvelopeWord));
		}
		else
		{
			VolumeTexts.Add(FString::Printf(TEXT("%s within its %s (%.3f m out)"), *Volume->GetName(), Record.EnvelopeWord, OutM));
		}
	}
	Line += FString::Join(VolumeTexts, TEXT(", ")) + TEXT(".");

	for (const TObjectPtr<USFDockingPortComponent>& Port : Ports)
	{
		if (Port)
		{
			Line += TEXT(" ") + DescribePort(*Port, StationFrameCm, StationBoxM, Physical, VolumeBoxesM, bRed);
		}
	}

	if (bRed)
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Line);
	}
	else
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *Line);
	}
}

FString ASFKitStation::DescribePort(const USFDockingPortComponent& Port, const FTransform& StationFrameCm, const FBox& StationBoxM,
	const TArray<const UPrimitiveComponent*>& Physical, const TArray<FBox>& VolumeBoxesM, bool& bInOutRed) const
{
	using namespace KitStation;

	const FTransform PortInFrame = Port.GetComponentTransform().GetRelativeTransform(StationFrameCm);
	const FVector OriginM = ToMetres(PortInFrame.GetLocation());
	const FVector Axis = PortInFrame.GetUnitAxis(EAxis::X);

	// Rule 1: p from the one box the release uses, and named by the collider that reaches furthest,
	// so the line says what to move. The box is one for the whole station: a protrusion anywhere on
	// the face costs every port its full p (spec 5).
	const double PastM = StarForge::Docking::BoundsPastPortPlane(StationBoxM, OriginM, Axis);
	FString PastText;
	if (StarForge::Docking::IsRedAtPrintedResolution(PastM))
	{
		bInOutRed = true;
		FString Worst = TEXT("nothing it could name");
		double WorstM = -1.0;
		for (const UPrimitiveComponent* Primitive : Physical)
		{
			const double M = StarForge::Docking::BoundsPastPortPlane(BoxInFrameMetres(*Primitive, StationFrameCm), OriginM, Axis);
			if (!(M <= WorstM))
			{
				WorstM = M;
				Worst = Primitive->GetName();
			}
		}
		PastText = FString::Printf(TEXT("%.3f m of something physical PAST its plane (%s)"), PastM, *Worst);
	}
	else
	{
		PastText = FString::Printf(TEXT("nothing physical past its plane (%.3f m)"), PastM);
	}

	// Rule 2, form 3: no interior volume past any port's plane.
	double VolumePastM = 0.0;
	FString VolumeName;
	for (int32 i = 0; i < VolumeBoxesM.Num(); ++i)
	{
		const double M = StarForge::Docking::BoundsPastPortPlane(VolumeBoxesM[i], OriginM, Axis);
		if (!(M <= VolumePastM))
		{
			VolumePastM = M;
			VolumeName = Volumes[VolumeRecords[i].Volume]->GetName();
		}
		// A NaN sticks. "!(M <= max)" takes a NaN in, and the next finite volume would then take its
		// place -- !(finite <= NaN) is true too -- and a volume nobody could measure would leave the
		// line calm (amendment P20).
		if (FMath::IsNaN(VolumePastM))
		{
			break;
		}
	}
	FString VolumeText;
	if (FMath::IsNaN(VolumePastM))
	{
		bInOutRed = true;
		VolumeText = FString::Printf(TEXT("how far an interior volume reaches past it could NOT BE MEASURED (%s)"), *VolumeName);
	}
	else if (StarForge::Docking::IsRedAtPrintedResolution(VolumePastM))
	{
		bInOutRed = true;
		VolumeText = FString::Printf(TEXT("an interior VOLUME REACHES %.3f m PAST it (%s)"), VolumePastM, *VolumeName);
	}
	else
	{
		VolumeText = FString::Printf(TEXT("no interior volume past it (%.3f m)"), VolumePastM);
	}

	return FString::Printf(TEXT("%s at (%.2f %.2f %.2f) m facing %s: %s, %s; %s."),
		*Port.GetName(), Shown(OriginM.X), Shown(OriginM.Y), Shown(OriginM.Z), *AxisWord(Axis),
		*PastText, *VolumeText, *DescribeHolder(Port, bInOutRed));
}

FString ASFKitStation::DescribeHolder(const USFDockingPortComponent& Port, bool& bInOutRed) const
{
	const USFDockingComponent* Holder = Port.GetHolder();
	if (!Holder)
	{
		return TEXT("free");
	}

	const FString Name = GetNameSafe(Holder->GetOwner());
	switch (Holder->GetState())
	{
	case ESFDockingState::Capturing:
		return FString::Printf(TEXT("capturing by %s"), *Name);
	case ESFDockingState::Releasing:
		return FString::Printf(TEXT("releasing by %s"), *Name);
	case ESFDockingState::Docked:
	{
		// Only here: ReleaseStandoff answers "how far out of the docked pose", and asked from any
		// other pose it answers another question beside a calm verdict (spec 6.8).
		double StandoffM = 0.0;
		double PeakMss = 0.0;
		if (!Holder->MeasureRelease(StandoffM, PeakMss))
		{
			bInOutRed = true;
			return FString::Printf(TEXT("held by %s, whose release could NOT BE MEASURED"), *Name);
		}

		// The very comparisons the release warns by (SFDockingComponent.cpp): the window on >, the
		// ceiling on >=.
		const bool bInside = StandoffM <= Holder->DistanceMetres;
		const bool bUnder = PeakMss < StarForge::ShipFrame::GravPlateLimitMetersPerSecSq;
		bInOutRed |= !bInside || !bUnder;

		const FString PeakText = (PeakMss >= TNumericLimits<double>::Max())
			? FString(TEXT("unbounded")) : FString::Printf(TEXT("%.1f m/s2"), PeakMss);

		return FString::Printf(
			TEXT("held by %s, whose release would be %.2f m in %.1f s, peak %s -- %s its %.1f m window, %s the %.2f m/s2 limit"),
			*Name, StandoffM, Holder->GetCaptureSeconds(), *PeakText,
			bInside ? TEXT("inside") : TEXT("OUTSIDE"), Holder->DistanceMetres,
			bUnder ? TEXT("under") : TEXT("OVER"), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq);
	}
	default:
		bInOutRed = true;
		return FString::Printf(TEXT("held by %s while it reads FREE"), *Name);
	}
}

FBox ASFKitStation::EnvelopeMetres(const FSFKitVolumeRecord& Record, const FTransform& StationFrameCm) const
{
	using namespace KitStation;

	FVector Min(0.0);
	FVector Max(0.0);
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2;
		const bool bMinFace = (Face % 2) == 0;

		if (Record.Faces[Face].IsEmpty())
		{
			return FBox(ForceInit);
		}

		// The tightest source: a min face as far in as its furthest-in wall, a max face likewise.
		double Value = bMinFace ? -TNumericLimits<double>::Max() : TNumericLimits<double>::Max();
		for (const FSFKitFaceSource& Source : Record.Faces[Face])
		{
			const UPrimitiveComponent* Collider = Colliders.IsValidIndex(Source.Collider) ? Colliders[Source.Collider].Get() : nullptr;
			if (!Collider)
			{
				return FBox(ForceInit);
			}
			const FBox Wall = BoxInFrameMetres(*Collider, StationFrameCm);
			const double Side = Source.bMaxSide ? Wall.Max[Axis] : Wall.Min[Axis];
			Value = bMinFace ? FMath::Max(Value, Side) : FMath::Min(Value, Side);
		}
		(bMinFace ? Min : Max)[Axis] = Value;
	}
	return FBox(Min, Max);
}
```

- [ ] **Step 4: Собрать и прочитать норму**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
kit DA_Place_Berth 60 25 "sf.FlightLog 1" task9-norm-60
grep -o "LogStarForge: Display: Station rig:.*" $L | head -1 | sed 's/^LogStarForge: Display: //' | tr -d '\r' > /tmp/kit-norm.txt
cat > /tmp/kit-norm-expected.txt <<'TXT'
Station rig: 'Station' (SFBerthStation), frame present, no simulating body -- 1 block, 1 port, 2 interior volumes, 15 physical colliders boxed in its own frame at X -18.38..41.62 Y -18.10..17.90 Z -16.00..16.00 m; Hall0 within its hall (0.000 m out), Boom0 within its channel (0.000 m out). Port0 at (0.00 -18.10 -2.75) m facing -Y: nothing physical past its plane (0.000 m), no interior volume past it (0.000 m); held by Ship, whose release would be 2.93 m in 1.0 s, peak 17.6 m/s2 -- inside its 3.0 m window, under the 19.62 m/s2 limit.
TXT
diff /tmp/kit-norm.txt /tmp/kit-norm-expected.txt && echo "norm: identical"
awk '/Station rig:/{if(!s)s=NR} /Flight: \[Ship\]/{if(!f)f=NR} END{print (s && f && s<f) ? "before the first Flight line" : "NOT before the first Flight line"}' $L
kit DA_Place_Berth 60 40 "sf.FlightLog 1, sf.Dock 2" task9-cycle-60
grep -o "Station rig:.*" $L | sed 's/.*past it (0.000 m); //'
rm docs/superpowers/notes/evidence/$(date +%F)-task9-*.log
```

Ворота: `norm: identical` и `before the first Flight line` — это H1.1 в части строки; и пять хвостов по порядку:

```
held by Ship, whose release would be 2.93 m in 1.0 s, peak 17.6 m/s2 -- inside its 3.0 m window, under the 19.62 m/s2 limit.
releasing by Ship.
free.
capturing by Ship.
held by Ship, whose release would be 2.93 m in 1.0 s, peak 17.6 m/s2 -- inside its 3.0 m window, under the 19.62 m/s2 limit.
```

Первый — строка `BeginPlay`, четыре остальных — перепечатки H1.2. Любое расхождение нормы с текстом §6.8 — находка задачи, а не повод поправить ожидание.

- [ ] **Step 5: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFKitStation.h Source/StarForge/Private/Ship/SFKitStation.cpp
git diff --cached --name-only
git commit -F - <<'MSG'
feat(station): a kit station says what its rules measure, in one line read back from the world

Rules 1 and 2 of the station kit cannot be enforced by code: TryHandover asks
only a relative speed, and the release asks only a box. They hold on geometry,
and only an instrument can see them broken. So a kit station writes the Station
rig line: its box in its own frame, each volume against the walls meant to
enclose it, and for each port what reaches past its plane, which volume does,
and who holds it -- with the release it would make, read from the holder's own
measurement and only while it is docked.

Written for every station, well or not, at BeginPlay and at every port change,
because a line that appears only in trouble is an instrument that did not run.
MSG
```

---

### Task 10: Контроли `sf.PortProtrusionM` и `sf.StationVolumeReach`

Строка, которую нельзя сломать нарочно, не доказывает, что умеет краснеть. N1/N1b ломают правило 1 контрольным коллайдером, N2/N2b — правило 2 выросшим объёмом (§8.3). Станция не тикает, поэтому переменная слушается через делегат изменения, по образцу `USFWorldBoundsSubsystem`, а команда с задержкой — через `FTimerManager`, по образцу `sf.KillThruster`.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFKitStation.h` (`EndPlay`, `ApplyVolumeReach`; приватные члены)
- Modify: `Source/StarForge/Private/Ship/SFKitStation.cpp` (include; объявления в `KitStation`; конструктор; `BeginPlay`; новые методы)

**Interfaces:**
- Потребляет из задачи 9: `ReportStationRig`, `VolumeRecords`, `BlockOriginsCm`.
- Отдаёт задачам 12, 20, 21:
  - строковая переменная `sf.PortProtrusionM` (пусто или 0 — нет коллайдера; иначе конечное ≥ 0.001 м)
  - команда `sf.StationVolumeReach <Volume> <Metres> [DelaySeconds]`
  - `bool ASFKitStation::ApplyVolumeReach(FName VolumeName, double Metres)`
  - строки `Station rig: sf.PortProtrusionM='…' is not 0 or a finite number of metres of at least 0.001; no control collider was placed.` (`Warning`), `Station rig: sf.StationVolumeReach grew Boom0 of 'Station' 4.000 m outward along -Y.` (`Display`)

- [ ] **Step 1: «Красный» шаг — контроль не ставится**

```bash
kit DA_Place_Berth 60 10 "sf.FlightLog 1" task10-red-60 "-ini:Engine:[ConsoleVariables]:sf.PortProtrusionM=1.0"
grep -c "PAST its plane (ControlProtrusion)" $L   # 0
grep -c "nothing physical past its plane (0.000 m)" $L   # 1: контроль задан, а строка спокойна
rm docs/superpowers/notes/evidence/$(date +%F)-task10-red-60.log
```

- [ ] **Step 2: Объявления**

`Source/StarForge/Public/Ship/SFKitStation.h`: в начало добавить `class IConsoleVariable;`. В `public:`:

```cpp
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * sf.StationVolumeReach's work on this station: grows the named interior volume by Metres
	 * outward along its block's port axis, or returns it to its authored size at 0, and says the
	 * Station rig: line again. False when this station has no volume of that name.
	 *
	 * Returned to its size, a volume that had taken a man from a cabin it reached into lets go of
	 * him, and the cabin's volume, which still holds him, takes him back: leaving the volume of the
	 * frame he is bound to while another carrier's volume holds him is entering that one
	 * (USFInteriorVolumeComponent::HandleEndOverlap, the review's N2). So 0 is control N2's whole
	 * return, with its own Frame handover: Station -> Ship line.
	 */
	bool ApplyVolumeReach(FName VolumeName, double Metres);
```

В `private:`:

```cpp
	/** Places, moves or withdraws control N1's collider for a raw sf.PortProtrusionM value. */
	void ApplyPortProtrusion(const FString& Raw);

	/** The change delegate's handler: a kit station does not tick, so it is told. */
	void OnPortProtrusionChanged(IConsoleVariable* Variable);

	/** Control N1's collider: created with no collision, switched on by sf.PortProtrusionM. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ControlProtrusion;

	FDelegateHandle PortProtrusionChangedHandle;

	/** What sf.PortProtrusionM last read, so a Set to the same value is not a new application. */
	FString LastProtrusionRaw;
```

- [ ] **Step 3: Переменная, команда и коллайдер**

`Source/StarForge/Private/Ship/SFKitStation.cpp`: добавить include `"EngineUtils.h"`, `"HAL/IConsoleManager.h"`, `"NumberText.h"`, `"TimerManager.h"`. **Не** `"Misc/DefaultValueHelper.h"`: с ревью 2026-10-01 каждое число из строковой переменной и из аргумента команды читает только `StarForge::Parse::TryParseFiniteNumber` (поправка П3 в начале плана). В `namespace KitStation`, в конец:

```cpp
	/**
	 * Control N1 and N1b of the station kit (spec 8.3): a 4 m collider at the far end of block 0's berth
	 * face, its -Y face this many metres past the port plane. A string, for sf.PlaceOffsetM's reason,
	 * read by StarForge::Parse::TryParseFiniteNumber (NumberText.h), which wants a digit in the mantissa
	 * and in any exponent: Atod answers zero for a string with no number in it, and
	 * FDefaultValueHelper::ParseDouble let ".", "-" and "e10" through as zero -- a control set with a
	 * typo would have placed nothing and read calm. Empty or 0: no collider. Read at the station's
	 * BeginPlay and again at every change of the value, through the variable's change delegate -- a kit
	 * station does not tick.
	 *
	 * Not put back when a world ends, unlike sf.Dock, sf.BindFrame and sf.AimAt: those act on a change,
	 * and a value left behind is a keypress in the next world; this is a level, as sf.PlaceOffsetM is,
	 * and a value left from the last Play is a control still set, which the Station rig: line says in red.
	 *
	 * Declared here because this is the file that reads it. The Station rig: line's Error is legitimate
	 * only while this is set.
	 */
	TAutoConsoleVariable<FString> CVarPortProtrusionM(TEXT("sf.PortProtrusionM"), TEXT(""),
		TEXT("Metres a control collider on block 0's berth face reaches past the port plane (station kit, controls N1/N1b).\n")
		TEXT("Empty or 0: none. Read at the kit station's BeginPlay and on every change."),
		ECVF_Default);

	/** Control N1's collider, spec 8.3: 4 m on a side, X 26..30 and Z 0..4 of block 0, far from the hull's path. */
	constexpr double ControlSizeCm = 400.0;
	constexpr double ControlXMinCm = 2600.0;
	constexpr double ControlZMinCm = 0.0;

	/**
	 * The smallest control accepted: the line's printed resolution (spec 8.3). Below
	 * StarForge::Docking::RedAtOrAboveMetres, half of it, the line would print 0.000 and stay calm; between
	 * the two it would print 0.001 and go red. A control is refused under a whole millimetre so that every
	 * accepted one prints at least what it was asked for (amendment P21).
	 */
	constexpr double MinimumControlMetres = 0.001;

	/** Metres and seconds alike: a finite number written out, or false and OutMetres untouched. */
	bool ParseFiniteMetres(const FString& Raw, double& OutMetres)
	{
		return StarForge::Parse::TryParseFiniteNumber(Raw, OutMetres);
	}

	void StationVolumeReachNow(UWorld* World, FName VolumeName, double Metres, const FString& Input)
	{
		int32 Applied = 0;
		for (TActorIterator<ASFKitStation> It(World); It; ++It)
		{
			if (It->ApplyVolumeReach(VolumeName, Metres))
			{
				++Applied;
			}
		}
		if (Applied == 0)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.StationVolumeReach '%s': no station in this world has an interior volume named %s; nothing was changed."),
				*Input, *VolumeName.ToString());
		}
	}

	void StationVolumeReachFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		const FString Input = FString::Join(Args, TEXT(" "));

		double Metres = 0.0;
		double AfterSeconds = 0.0;
		const bool bShape = (Args.Num() == 2 || Args.Num() == 3)
			&& ParseFiniteMetres(Args[1], Metres)
			&& (Args.Num() == 2 || ParseFiniteMetres(Args[2], AfterSeconds));

		// A negative figure accepted in silence would grow the volume inward, read 0.000 and look
		// like a control that found nothing; a positive one under a millimetre is under the line's
		// printed resolution, and under half of one it would print 0.000.
		if (!bShape || !(Metres == 0.0 || Metres >= MinimumControlMetres) || !(AfterSeconds >= 0.0))
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.StationVolumeReach '%s': needs a volume name, 0 or a number of metres of at least 0.001, and optionally a ")
				TEXT("non-negative delay in seconds; nothing was changed."),
				*Input);
			return;
		}

		const FName VolumeName(*Args[0]);
		if (AfterSeconds == 0.0 || !World)
		{
			StationVolumeReachNow(World, VolumeName, Metres, Input);
			return;
		}

		// Seconds of game time, not frames, and for the reason sf.KillThruster has one: -ExecCmds runs
		// its whole list on the first frame, before the start-up handover has been carried, and a
		// volume grown then would supersede it (spec 8.3).
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), VolumeName, Metres, Input]()
			{
				if (UWorld* LiveWorld = WeakWorld.Get())
				{
					StationVolumeReachNow(LiveWorld, VolumeName, Metres, Input);
				}
			}),
			static_cast<float>(AfterSeconds), false);

		UE_LOG(LogStarForge, Display,
			TEXT("sf.StationVolumeReach '%s': scheduled %.2f s of game time from now."), *Input, AfterSeconds);
	}

	/**
	 * Controls N2 and N2b (spec 8.3). A COMMAND and not a variable, as sf.KillThruster is: a command runs
	 * once per entry, and it takes a delay.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdStationVolumeReach(
		TEXT("sf.StationVolumeReach"),
		TEXT("sf.StationVolumeReach <Volume> <Metres> [Seconds]: grow a kit station's interior volume (Boom0, Hall1, ...) by that\n")
		TEXT("many metres outward along its block's port axis, now or after that many seconds of game time. 0 restores it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StationVolumeReachFromConsole));
```

В конструкторе `ASFKitStation`, после `Frame = …`:

```cpp
	// Control N1's collider, built now and switched off: default subobjects exist only in the
	// constructor, and it is placed at BeginPlay or at a change of sf.PortProtrusionM.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ControlCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ControlProtrusion = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ControlProtrusion"));
	ControlProtrusion->SetupAttachment(Root);
	ControlProtrusion->SetMobility(EComponentMobility::Movable);
	ControlProtrusion->SetCollisionObjectType(ECC_WorldStatic);
	ControlProtrusion->SetCollisionResponseToAllChannels(ECR_Block);
	ControlProtrusion->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ControlProtrusion->SetHiddenInGame(true);
	if (ControlCube.Succeeded())
	{
		ControlProtrusion->SetStaticMesh(ControlCube.Object);
	}
```

`BeginPlay` заменить и добавить методы:

```cpp
void ASFKitStation::BeginPlay()
{
	Super::BeginPlay();

	// Told of every change, not polled: this actor does not tick. The same pattern the world-bounds
	// line uses for WorldBounds.DisableChecks (SFWorldBoundsSubsystem.cpp 44-48).
	if (IConsoleVariable* Variable = KitStation::CVarPortProtrusionM.AsVariable())
	{
		PortProtrusionChangedHandle = Variable->OnChangedDelegate().AddUObject(this, &ASFKitStation::OnPortProtrusionChanged);
	}

	ApplyPortProtrusion(KitStation::CVarPortProtrusionM.GetValueOnGameThread());
	ReportStationRig();
}

void ASFKitStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IConsoleVariable* Variable = KitStation::CVarPortProtrusionM.AsVariable())
	{
		Variable->OnChangedDelegate().Remove(PortProtrusionChangedHandle);
	}
	PortProtrusionChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASFKitStation::OnPortProtrusionChanged(IConsoleVariable* Variable)
{
	// The delegate fires on a Set that leaves the value where it was, too; one line per real change.
	const FString Raw = Variable ? Variable->GetString() : FString();
	if (Raw == LastProtrusionRaw)
	{
		return;
	}
	ApplyPortProtrusion(Raw);
	ReportStationRig();
}

void ASFKitStation::ApplyPortProtrusion(const FString& Raw)
{
	using namespace KitStation;

	LastProtrusionRaw = Raw;

	double Metres = 0.0;
	bool bPlace = false;
	if (!Raw.IsEmpty())
	{
		if (ParseFiniteMetres(Raw, Metres) && (Metres == 0.0 || Metres >= MinimumControlMetres))
		{
			bPlace = Metres > 0.0;
		}
		else
		{
			// A negative protrusion taken in silence would read p = 0 and a calm line under a set control.
			UE_LOG(LogStarForge, Warning,
				TEXT("Station rig: sf.PortProtrusionM='%s' is not 0 or a finite number of metres of at least 0.001; no control collider was placed."),
				*Raw);
		}
	}

	if (!ControlProtrusion)
	{
		return;
	}

	if (!bPlace || BlockOriginsCm.IsEmpty())
	{
		ControlProtrusion->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ControlProtrusion->SetHiddenInGame(true);
		return;
	}

	// Block 0's berth face, X 26..30 and Z 0..4 in its frame B, its -Y face p past the port plane.
	// Placed and sized before it takes part in physics, so no step ever sees it anywhere else.
	const FVector MinCm = BlockOriginsCm[0]
		+ FVector(ControlXMinCm, PortPlaneYCm - StarForge::Units::MetresToCm(Metres), ControlZMinCm);
	ControlProtrusion->SetRelativeLocation(MinCm + FVector(ControlSizeCm * 0.5));
	// Qualified although KitStation is open here: SFTestShip.cpp has a constant of the same name in
	// the module's one anonymous namespace, and under the directive the bare name would be ambiguous
	// as soon as that file came first in the unity blob.
	ControlProtrusion->SetRelativeScale3D(FVector(ControlSizeCm / KitStation::CubeEdgeCm));
	ControlProtrusion->SetHiddenInGame(false);
	ControlProtrusion->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

bool ASFKitStation::ApplyVolumeReach(FName VolumeName, double Metres)
{
	using namespace KitStation;

	for (const FSFKitVolumeRecord& Record : VolumeRecords)
	{
		USFInteriorVolumeComponent* Volume = Volumes[Record.Volume];
		if (!Volume || Volume->GetFName() != VolumeName)
		{
			continue;
		}

		// Grown on its outward side only: the centre moves out by half, the extent grows by half along
		// the axis. Moved first and resized with overlaps updated, so a man the grown box now covers
		// is told in this frame (BoxComponent.cpp 28-47), without a tick.
		const double GrowCm = StarForge::Units::MetresToCm(Metres);
		Volume->SetRelativeLocation(Record.AuthoredCentreCm + Record.OutwardAxis * (GrowCm * 0.5));
		Volume->SetBoxExtent(Record.AuthoredExtentCm + Record.OutwardAxis.GetAbs() * (GrowCm * 0.5), /*bUpdateOverlaps=*/ true);

		if (Metres > 0.0)
		{
			UE_LOG(LogStarForge, Display,
				TEXT("Station rig: sf.StationVolumeReach grew %s of '%s' %.3f m outward along %s."),
				*Volume->GetName(), *GetName(), Metres, *AxisWord(Record.OutwardAxis));
		}
		else
		{
			UE_LOG(LogStarForge, Display,
				TEXT("Station rig: sf.StationVolumeReach returned %s of '%s' to its authored size."),
				*Volume->GetName(), *GetName());
		}
		ReportStationRig();
		return true;
	}
	return false;
}
```

- [ ] **Step 4: Собрать и увидеть каждый контроль красным**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
kit DA_Place_Berth 60 25 "sf.FlightLog 1, sf.Dock 0" task10-n1-60 "-ini:Engine:[ConsoleVariables]:sf.PortProtrusionM=1.0"
grep -c "1.000 m of something physical PAST its plane (ControlProtrusion)" $L   # >= 1
grep -cF "Dock: [Ship] releasing, 3.93 m along the port axis over 1.0 s, peak 23.6 m/s2." $L   # 1
kit DA_Place_Berth 60 20 "sf.FlightLog 1, sf.StationVolumeReach Boom0 4.0 1.0" task10-n2-60
grep -c "Boom0 REACHES 4.000 m OUT of its channel" $L    # >= 1
grep -c "an interior VOLUME REACHES 4.000 m PAST it (Boom0)" $L   # >= 1
kit DA_Place_Berth 60 10 "sf.FlightLog 1, sf.StationVolumeReach Nope 1.0, sf.StationVolumeReach Boom0 -1" task10-junk-60 "-ini:Engine:[ConsoleVariables]:sf.PortProtrusionM=-1"
grep -c "sf.PortProtrusionM='-1' is not 0 or a finite number of metres of at least 0.001; no control collider was placed." $L   # 1
grep -c "no station in this world has an interior volume named Nope" $L   # 1
grep -c "sf.StationVolumeReach 'Boom0 -1': needs a volume name" $L       # 1
kit DA_Place_Berth 60 10 "sf.FlightLog 1, sf.StationVolumeReach Boom0 ., sf.StationVolumeReach Boom0 4.0 -" task10-junk2-60 "-ini:Engine:[ConsoleVariables]:sf.PortProtrusionM=."
grep -c "sf.PortProtrusionM='.' is not 0 or a finite number of metres of at least 0.001; no control collider was placed." $L   # 1
grep -c "sf.StationVolumeReach 'Boom0 .': needs a volume name" $L        # 1
grep -c "sf.StationVolumeReach 'Boom0 4.0 -': needs a volume name" $L    # 1
grep -c "sf.StationVolumeReach grew" $L                                    # 0: ни одна опечатка не стала нулём
rm docs/superpowers/notes/evidence/$(date +%F)-task10-*.log
grep -nE "(^|[^:])\b(CubeEdgeCm|MarkerUniformScale)\b" Source/StarForge/Private/Ship/SFKitStation.cpp
```

Последняя команда — ворота правила unity-сборки (Global Constraints). Задача 10 последней правит `SFKitStation.cpp`. Ожидаются ровно три строки: комментарий над `namespace KitStation` («…already owns CubeEdgeCm and MarkerUniformScale…», исправлено при приёмке задачи 8) и два объявления в нём, `constexpr double CubeEdgeCm = 100.0;` и `constexpr double MarkerUniformScale = 0.8;`. Любая другая строка — имя без `KitStation::` в функции, которая открыла пространство директивой. Сегодня оно собирается только по алфавиту файлов в блобе.

Полные ворота N1, N1b, N2, N2b на обеих частотах — задача 12. Здесь — что каждый контроль вообще умеет покраснеть и что мусор в нём не проходит молча. Второй мусорный прогон (`task10-junk2`) добавлен 2026-10-14: `.` и `-` — ровно те строки, которые `FDefaultValueHelper::ParseDouble` пропускал нулём; на нём прежний код плана поставил бы `sf.PortProtrusionM=.` без коллайдера и без `Warning`, а `Boom0 4.0 -` применил бы на первом кадре.

- [ ] **Step 5: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFKitStation.h Source/StarForge/Private/Ship/SFKitStation.cpp
git diff --cached --name-only
git commit -F - <<'MSG'
feat(station): controls that break rules 1 and 2 on purpose, and must turn the line red

A line that cannot be broken on purpose has not shown it can go red. N1 and
N1b put a control collider past the port plane by a set distance; N2 and N2b
grow an interior volume out of its channel or its hall. Each is read back in the
Station rig line, and junk in any of them is said out loud rather than taken as
zero -- a control performed with a typo looks exactly like one that found
nothing.

The station does not tick, so the variable is heard through its change
delegate, as the world-bounds line hears its own, and the volume command takes
a delay in game seconds: -ExecCmds runs on the first frame, before the start-up
handover is carried, and a volume grown then would overwrite it.
MSG
```

---

### Task 11: Строка `SEATED` в `TryHandover`

Сидящий, отданный чужому носителю, — это и есть поломка F1, а сегодня она выходит в журнал спокойной строкой `Display` (§6.7, п. 3; решение 11). Прибор, а не отказ: передача не останавливается, и отклонённая владельцем правка «не передавать сидящего» не возвращается.

Headless проверить нельзя: посадить человека в кресло без рук нечем (ни одна `sf.*` не жмёт `E`). Ворота задачи — сборка и отсутствие строки во всех headless-прогонах; строка сама — стоп-ворота N2 руками на точке 11.

**Files:**
- Modify: `Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp` (include; `TryHandover` перед строкой `ForgetRefusalOnEveryVolumeOfThisCarrier(Character);`)

**Interfaces:**
- Потребляет: `ASFCharacter::IsSeated()`, `ASFCharacter::GetSeatedStation()`, `USFShipStationComponent::StationName`.
- Отдаёт задачам 12 и 21: строка `Warning` `Frame handover: <From> -> <To> took a SEATED man (<Owner>'s <post> post): a volume of <To> reaches into <From>'s cabin.`

- [ ] **Step 1: Строка**

`Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp`: добавить `#include "Ship/SFShipStationComponent.h"`. В `TryHandover`, перед `ForgetRefusalOnEveryVolumeOfThisCarrier(Character);`:

```cpp
	// An instrument, not a refusal (station-kit spec 6.7 item 3, owner's decision 11). A seated man
	// handed to another carrier is finding F1 itself -- a station's volume reaching into a cabin -- and
	// the handover line below would report it calmly. It goes through as before: refusing to hand over
	// a seated man was one of the fixes the owner declined.
	//
	// Only when the carrier taking him is not the one whose seat he holds. Since the review's N2,
	// leaving the volume of the frame he is bound to while another carrier's volume still holds him
	// is entering that one (HandleEndOverlap), and that is how control N2 returns him: the grown
	// volume goes back to its size and his own cabin takes him back. Handed BACK to his seat's
	// carrier is the fix, not F1, and a line saying "a volume of Ship reaches into Station's cabin"
	// there would be false.
	const USFShipStationComponent* Seat = Character->IsSeated() ? Character->GetSeatedStation() : nullptr;
	const AActor* SeatCarrier = Seat ? Seat->GetOwner() : nullptr;
	if (From && From != To && Character->IsSeated() && SeatCarrier != To->GetOwner())
	{
		const FString FromName = From->GetOwner() ? From->GetOwner()->GetActorNameOrLabel() : FString(TEXT("none"));
		const FString ToName = To->GetOwner() ? To->GetOwner()->GetActorNameOrLabel() : FString(TEXT("none"));
		const FString SeatOwner = (Seat && Seat->GetOwner()) ? Seat->GetOwner()->GetActorNameOrLabel() : FromName;
		UE_LOG(LogStarForge, Warning,
			TEXT("Frame handover: %s -> %s took a SEATED man (%s's %s post): a volume of %s reaches into %s's cabin."),
			*FromName, *ToName, *SeatOwner, Seat ? *Seat->StationName : TEXT("unknown"), *ToName, *FromName);
	}
```

- [ ] **Step 2: Собрать; строки нет в прогоне со стоящим человеком**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
kit DA_Place_Berth 60 20 "sf.FlightLog 1, sf.StationVolumeReach Boom0 4.0 1.0, sf.StationVolumeReach Boom0 0 3.0" task11-standing-60
grep -c "took a SEATED man" $L                          # 0: он стоит, кресло держит консоль
grep -cF "Frame handover: Ship -> Station, first carry moved him 0.000 cm" $L   # 1: передача та же
grep -cF "Frame handover: Station -> Ship, first carry moved him 0.000 cm" $L   # 2: стартовая и возврат N2
grep -cF "sf.StationVolumeReach returned Boom0 of" $L    # 1
grep -c "Nothing was rebound; the control did not run" $L   # 0: sf.BindFrame не нужен и не набран
rm docs/superpowers/notes/evidence/$(date +%F)-task11-*.log
```

Вторая строка `Station -> Ship` — возврат контроля N2 после ревью (Н2): объём `Boom0`, вернувшийся к размеру, отпускает человека, а объём каюты, который его по-прежнему держит, забирает его обратно сам (поправка П4). Строки `SEATED` на возврате нет и у сидящего: корабль, которому его вернули, — владелец его кресла. Это условие проверяется только руками (N2 на точке 11).

- [ ] **Step 3: Коммит**

```bash
git status --short
git add Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp
git diff --cached --name-only
git commit -F - <<'MSG'
feat(handover): say it out loud when a volume takes a seated man

A seated man handed to another carrier is F1 itself: a station's volume reaching
into a cabin. The handover line reported it at Display level, calmly. It now
goes out as a Warning naming the post, the carrier that took him and the cabin
it reached into.

An instrument and not a refusal. The handover still goes through: refusing to
hand over a seated man was one of the two fixes the owner declined after F1,
and this does not bring it back by the side door.
MSG
```

---

### Task 12: Прогоны шага 1 на обеих частотах и регрессия `kit-1.10`

Все ворота шага 1 из §8.2 и §8.3, headless, на 60 и 400 fps, каждый журнал — в улики. Затем регрессия: этот проход впервые повторяет и журналы набора.

**Files:**
- Create: `docs/superpowers/notes/evidence/<дата>-kit-*.log` (30 журналов: 28 с ровным шагом и 2 с переменным)
- Modify: `docs/superpowers/notes/<дата>-station-kit-baseline.md` (таблица регрессий)

**Interfaces:**
- Потребляет: всё шага 1.
- Отдаёт задачам 20 и 21: журналы с дословными строками ворот для бланка точки 11 и для `CLAUDE.md`.

- [ ] **Step 1: Двадцать восемь прогонов**

Для каждой строки — `kit` на `-fps=60` и на `-fps=400`, имя журнала `kit-<прогон>-<частота>`:

| Прогон | Команда |
|---|---|
| H1.1 | `kit DA_Place_Berth <fps> 70 "sf.FlightLog 1" kit-h1-1-<fps>` |
| H1.2 | `kit DA_Place_Berth <fps> 40 "sf.FlightLog 1, sf.Dock 2" kit-h1-2-<fps>` |
| H1.3 x | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.Dock 0, sf.Intent.X 1" kit-h1-3-x-<fps>` |
| H1.3 z | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.Dock 0, sf.Intent.Z 1" kit-h1-3-z-<fps>` |
| H1.3 −y | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.Dock 0, sf.Intent.Y -1" kit-h1-3-yneg-<fps>` |
| H1.4 | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.Knockdown 0, sf.Dock 0, sf.Intent.Roll 1" kit-h1-4-<fps>` |
| H1.5 | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.Dock 0, sf.Intent.Y 0.004" kit-h1-5-<fps>` |
| H1.6 | `kit DA_Place_Berth <fps> 20 "sf.FlightLog 1, sf.BindFrame station, sf.AimAt station" kit-h1-6-<fps>` |
| N1 | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.Dock 0" kit-n1-<fps> "-ini:Engine:[ConsoleVariables]:sf.PortProtrusionM=1.0"` |
| N1b | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.Dock 0" kit-n1b-<fps> "-ini:Engine:[ConsoleVariables]:sf.PortProtrusionM=0.2"` |
| N2 | `kit DA_Place_Berth <fps> 20 "sf.FlightLog 1, sf.StationVolumeReach Boom0 4.0 1.0" kit-n2-<fps>` |
| N2b | `kit DA_Place_Berth <fps> 20 "sf.FlightLog 1, sf.StationVolumeReach Hall0 1.0 1.0" kit-n2b-<fps>` |
| N5 | `kit DA_Place_Berth <fps> 25 "sf.FlightLog 1, sf.DockRelease 0, sf.Dock 0, sf.Intent.X 1" kit-n5-<fps>` |
| N7 | `kit DA_Place_Nope <fps> 25 "sf.FlightLog 1" kit-n7-<fps>` |

Плюс два прогона H1.2 **с переменным шагом** (поправка П9, добавлено 2026-10-14): отход и захват со стоящим человеком — ровно класс F4, который при ровном шаге провалиться не может (`CLAUDE.md`, рецепт под headless-командой). Без `-benchmark` и `-fps`; `-seconds` даёт чистый выход по реальному времени. Функция — рядом с `kit`, в той же оболочке:

```bash
kitvar() {
  local asset="$1" secs="$2" cmds="$3" name="$4"; shift 4
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge-station/StarForge.uproject" \
    "/Game/Maps/L_FrameSpike?game=/Script/StarForge.SFPlaceGameMode" \
    "-ini:Engine:[ConsoleVariables]:sf.PlaceAsset=$asset" "$@" \
    -game -nullrhi -unattended -nosplash -seconds="$secs" -ExecCmds="$cmds" -log < /dev/null
  cp Saved/Logs/StarForge.log "docs/superpowers/notes/evidence/$(date +%F)-$name.log"
}
kitvar DA_Place_Berth 40 "sf.FlightLog 1, sf.Dock 2" kit-h1-2-var-a
kitvar DA_Place_Berth 40 "sf.FlightLog 1, sf.Dock 2" kit-h1-2-var-b
```

Оба журнала — одна команда, и `regress_evidence.py` повторяет её дважды и судит по красным счётчикам против обоих (вердикт `STEADY`).

- [ ] **Step 2: Ворота каждого журнала**

`F=docs/superpowers/notes/evidence/$(date +%F)-kit-<прогон>-<fps>.log`. Общие ворота раздела «Как проверяется» — на каждом, N7 тоже. Строка `World bounds:` обязательна во всех прогонах без исключения, а незваный `Warning` — находка везде (§8.2). У N7 человека нет, и счётчики переноса читают 0, ничего не доказывая. Их заменяет отсутствие строк `Shipboard drift` и `Frame [` в его блоке. Вердикт опоры (§8.2) у каждого прогона — своей командой в его блоке. Норма строки `Station rig:` — текст §6.8, записанный в файл один раз:

```bash
cat > /tmp/kit-norm-expected.txt <<'TXT'
Station rig: 'Station' (SFBerthStation), frame present, no simulating body -- 1 block, 1 port, 2 interior volumes, 15 physical colliders boxed in its own frame at X -18.38..41.62 Y -18.10..17.90 Z -16.00..16.00 m; Hall0 within its hall (0.000 m out), Boom0 within its channel (0.000 m out). Port0 at (0.00 -18.10 -2.75) m facing -Y: nothing physical past its plane (0.000 m), no interior volume past it (0.000 m); held by Ship, whose release would be 2.93 m in 1.0 s, peak 17.6 m/s2 -- inside its 3.0 m window, under the 19.62 m/s2 limit.
TXT
```

Сверх общих ворот:

**H1.1:**
```bash
grep -F "Place size: 'Berth' (open space, system 'unnamed')" "$F" | grep -c -- "-- fits."   # 1
grep -cF "Place carrier: 'Station' of class SFBerthStation in place 'Berth' at place-local (0.0 0.0 0.0) m" "$F"   # 1; в строке "frame present -- motionless (open space)."
grep -o "LogStarForge: Display: Station rig:.*" "$F" | head -1 | sed 's/^LogStarForge: Display: //' | tr -d '\r' | diff - /tmp/kit-norm-expected.txt && echo norm   # norm
awk '/Station rig:/{if(!s)s=NR} /Flight: \[Ship\]/{if(!f)f=NR} END{print (s && f && s<f) ? "before Flight" : "NOT before Flight"}' "$F"
grep -cF "Place rig: scripted profile off; place 'Berth' read from DA_Place_Berth and assembled with 1 carrier; the ship spawns docked; the man stands in the cabin and is bound to its frame." "$F"   # 1
grep -cF "Frame handover: Station -> Ship, first carry moved him 0.000 cm" "$F"   # 1
grep -o "Shipboard drift:.*" "$F" | tail -1   # peak drift 0.000 cm ... -- footing intact
```
**H1.2:**
```bash
grep -oE "Dock: \[Ship\] (releasing, 2.93 m along the port axis over 1.0 s, peak 17.6 m/s2\.|undocked\.|capturing, 2.9 m, 0.0 m/s, fwd 0.0 deg, up 0.0 deg|docked\.)" "$F"
grep -o "Station rig:.*" "$F" | sed 's/.*past it (0.000 m); //'
grep -cF "Frame [Ship]: angular velocity read from the simulating body." "$F"      # >= 1
grep -cF "Frame [Ship]: angular velocity differenced from the transform -- no simulating body." "$F"   # >= 1
grep -o "Shipboard drift:.*" "$F" | tail -1 | grep -c "footing intact"   # 1
```
Ворота: `docked.` → `releasing, 2.93 m …` → `undocked.` → `capturing, 2.9 m, 0.0 m/s, fwd 0.0 deg, up 0.0 deg` → `docked.` по порядку; пять хвостов строки `Station rig:` — те же, что в шаге 4 задачи 9; вердикт `footing intact`.

**H1.2 с переменным шагом (`kit-h1-2-var-a`, `-b`):** та же последовательность строк `Dock:` по порядку; `grep -c "Footing LOST"` — `0`, `grep -c "skipped carries [1-9]"` — `0`, `grep -c "Carry NOT RUNNING"` — `0`; последняя строка `Shipboard drift:` — `footing intact`. Числа строк `Flight:` при переменном шаге не повторяются и воротами не являются. Сбитие здесь — находка класса F4, а не протокол.

**H1.3 (все три):**
```bash
grep "Dock: \[Ship\] free |" "$F" | grep -vc "fwd 0.0 deg, up 0.0 deg"   # 0
grep -o "Shipboard drift:.*" "$F" | tail -1                              # peak drift 0.000 cm ... footing intact
```

**H1.4:**
```bash
grep -oE "ang roll/pitch/yaw \([^)]*\)" "$F" | tail -15 | grep -vcE "\(180\.0 -?0\.0 -?0\.0\)"   # 0
grep -o "Shipboard drift:.*" "$F" | tail -1   # -- knockdown off (sf.Knockdown 0)
```

**H1.5:**
```bash
grep -c "Frame handover: Ship -> Station" "$F"   # 0
grep -c "Standing on" "$F"                       # 0
grep -o "Footing LOST.*" "$F"                     # протокол, класс O2: число — в бланк
```

**H1.6:**
```bash
grep -cF "Frame bound by console to Station (sf.BindFrame)." "$F"                 # 1
grep -cF "Automation: [Ship] sf.AimAt 'station': tracking Station." "$F"          # 1
W=$(grep -c "LogStarForge: Warning" "$F"); S=$(grep -c "Standing on Ship's floor while bound to Station's frame" "$F"); P=$(grep -c "was superseded by" "$F")
echo "warnings $W = standing $S + superseded $P; superseded <= 1"
grep -o "Shipboard drift:.*" "$F" | tail -1 | grep -c "footing intact"   # 1
```
Законно красное — только эти две строки (§8.2); `superseded` пишется или нет в зависимости от кадра, в котором применился список, и воротами не является.

**N1:**
```bash
grep -c "1.000 m of something physical PAST its plane (ControlProtrusion)" "$F"   # >= 1
grep -c "3.93 m in 1.0 s, peak 23.6 m/s2 -- OUTSIDE its 3.0 m window, OVER the 19.62 m/s2 limit." "$F"   # >= 1
grep -cF "Dock: [Ship] releasing, 3.93 m along the port axis over 1.0 s, peak 23.6 m/s2." "$F"   # 1
grep -cF "Dock: [Ship] the release stands the ship 3.93 m out, outside its own 3.0 m docking window; it cannot dock again without flying back in." "$F"   # 1
grep -cF "Dock: [Ship] the release peaks at 23.6 m/s2, at or over the 19.62 m/s2 grav-plate limit; a man standing aboard will be knocked down." "$F"   # 1
grep "LogStarForge: Error" "$F" | grep -vc "Station rig:"                           # 0
grep "LogStarForge: Warning" "$F" | grep -vc "the release stands the ship\|the release peaks at"   # 0
```
Вердикт опоры — протокол (пик выше потолка, сбитый обещан самим отходом).

**N1b:**
```bash
grep -c "0.200 m of something physical PAST its plane (ControlProtrusion)" "$F"   # >= 1
grep -cF "Dock: [Ship] releasing, 3.13 m along the port axis over 1.0 s, peak 18.8 m/s2." "$F"   # 1
grep -c "the release stands the ship 3.13 m out" "$F"                              # 1
grep -c "the release peaks at" "$F"                                                # 0: пределы разделены
grep -o "Shipboard drift:.*" "$F" | tail -1                                        # footing intact
```

**N2:**
```bash
grep -c "Boom0 REACHES 4.000 m OUT of its channel" "$F"              # >= 1
grep -c "an interior VOLUME REACHES 4.000 m PAST it (Boom0)" "$F"    # >= 1
grep -cF "Frame handover: Ship -> Station, first carry moved him 0.000 cm" "$F"   # 1
grep "Standing on Ship's floor while bound to Station's frame" "$F" | head -1   # ... (0.17 s).
awk '/Frame handover: Station -> Ship/{if(!h)h=NR} /sf.StationVolumeReach grew Boom0/{if(!g)g=NR} END{print (h && g && h<g) ? "start handover first" : "ORDER WRONG"}' "$F"
grep "LogStarForge: Error" "$F" | grep -vc "Station rig:"            # 0
grep "LogStarForge: Warning" "$F" | grep -vc "Standing on"           # 0
grep -o "Shipboard drift:.*" "$F" | tail -1 | grep -c "footing intact"   # 1
```

**N2b:**
```bash
grep "Hall0 REACHES 1.000 m OUT of its hall" "$F" | grep -c "Port0 at (0.00 -18.10 -2.75) m facing -Y: nothing physical past its plane (0.000 m), no interior volume past it (0.000 m)"   # 1
grep -c "Frame handover: Ship -> Station" "$F"                       # 0
grep "LogStarForge: Error" "$F" | grep -vc "Station rig:"            # 0
grep -o "Shipboard drift:.*" "$F" | tail -1 | grep -c "footing intact"   # 1
```

**N5:**
```bash
grep -o "Shipboard drift:.*" "$F" | grep -c "KNOCKED DOWN"   # >= 1
```
Если на обеих частотах `footing intact` — контроль про стрелу ничего не доказывает, и это находка про сам контроль (§8.3): записать и остановиться.

**N7:**
```bash
grep -cF "Place rig: sf.PlaceAsset='DA_Place_Nope' names no place asset at /Game/Places/DA_Place_Nope -- REFUSED before anything was measured; nothing will be assembled and the pawn class is withdrawn." "$F"   # 1
grep -cF "Place rig: place asset 'DA_Place_Nope' REFUSED (no such asset) -- NOTHING WAS ASSEMBLED. No carriers, no ship, and no pawn: the pawn class was withdrawn before the local player logged in" "$F"   # 1
for p in "Place size:" "Place carrier:" "Dock:" "Shipboard drift" "Frame \["; do printf "%s %s\n" "$(grep -c "$p" "$F")" "$p"; done   # все 0
grep "LogStarForge: Error" "$F" | grep -vc "Place rig:"   # 0
grep -c "LogStarForge: Error" "$F"                        # 2: обе строки отказа, и только они
grep -c "LogStarForge: Warning" "$F"                      # 0
grep -c "LogStarForge: Display: World bounds:" "$F"       # 1: и в мире, где не собрано ничего
```

**Стартовая передача в прогонах с отстыковкой на первом кадре** (H1.2–H1.5, N1, N1b, N5; поправка П11): по Н1 ревью первый перенос после приземления идёт от позы корабля на момент привязки, и строка `Frame handover: Station -> Ship` показывает не `0.000 cm`, а путь корпуса за кадры до приземления — d × (3s² − 2s³) при s = 5/60 и 27/400: ≈5.76 / 3.82 см при d = 2.93 м, ≈7.73 / 5.13 у N1, ≈6.16 / 4.08 у N1b; у N5 отхода нет (`sf.DockRelease 0`), и число — путь от тяги, ≈2.04 / 1.21 см. Это протокол, не находка: ворота `0.000 cm` стоят только у H1.1 и N2. Число — в бланк как есть.

Каждая строка ворот переписывается в бланк точки 11 дословно (задача 21 даёт ему форму). Провалившиеся ворота — находка: записать, не объяснять, остановиться.

- [ ] **Step 3: Регрессия `kit-1.10`**

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-1.10
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-1.10; echo "exit $?"
```

`compare: 0 findings`. Двадцать восемь новых журналов с ровным шагом — команды, которых нет в базе; каждая обязана повториться `SAME` или `VARIES` с неизменными счётчиками против своей опорной. Команда H1.2 с переменным шагом — новая, и обязана выйти `STEADY` (сравнитель пишет её заметкой `new in run, STEADY at a variable step`). Прогон с `VARIES` (удар в H1.5, сбитый в N1 и N5) — протокол, и его счётчики сравниваются так, как §6.7 велит. Строка в таблицу регрессий.

- [ ] **Step 4: Коммит**

```bash
git status --short
git add docs/superpowers/notes/evidence/*-kit-*.log docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
test(station): step 1 of the station kit at 60 and 400 fps, and the evidence replay after it

Every headless gate spec 8.2 and 8.3 set for the berth: the norm of the Station
rig line before the first flight line, the four reprints of a docking cycle in
order, clean releases along, across and up, roll after undocking, the bump into
the boom's end faces, both searches finding the kit station, and the controls
N1, N1b, N2, N2b, N5 and N7 each producing its stop-gate line.

The logs go into evidence, so the replay after this commit repeats them as
baselines of their own, and the pass against the base taken before any code
change still finds nothing.
MSG
```

---

## Шаг 2: ядро причала

### Task 13: `ASFPierCoreStation` и `DA_Place_PierCore`

Два вызова сборщика блока (§7.1). Строится **до** правила выбора: правилу нужна станция с двумя портами, чтобы на своей задаче его было на чём проверить. До правила корабль берёт первый порт первой станции по имени — `Port0`, то есть ровно то, что правило выберет из центра зала (§7.4).

**Files:**
- Create: `Source/StarForge/Public/Ship/SFPierCoreStation.h`
- Create: `Source/StarForge/Private/Ship/SFPierCoreStation.cpp`
- Create: `Content/Places/DA_Place_PierCore.uasset`

**Interfaces:**
- Потребляет из задачи 8: `ASFKitStation::BuildBlock`.
- Отдаёт задачам 15–19: `UCLASS() class ASFPierCoreStation : public ASFKitStation` — `/Script/StarForge.SFPierCoreStation`; порты `Port0` (0.00, −18.10, −2.75) м и `Port1` (60.00, −18.10, −2.75) м.

- [ ] **Step 1: «Красный» шаг**

```bash
KIT_PLACES_GROUP=piercore "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "no class /Script/StarForge.SFPierCoreStation\|Python script executed" $L   # no class ...; executed with errors
```

- [ ] **Step 2: Класс**

`Source/StarForge/Public/Ship/SFPierCoreStation.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Ship/SFKitStation.h"
#include "SFPierCoreStation.generated.h"

/**
 * Step 2 of the station kit: the pier core, spec.py's B_K3core -- two M01 blocks side by side, both
 * berths on -Y, two ports on one carrier (station-kit spec 7.1). Not sheet A16's full K3 pier, which
 * is built of spine blocks with a berth on each face (the mechanics note, contradiction 3).
 *
 * The two halls are not connected (owner's decision 5): no corridor, no back door, no walking joint.
 * Nothing in the port mechanism depends on them.
 */
UCLASS()
class STARFORGE_API ASFPierCoreStation : public ASFKitStation
{
	GENERATED_BODY()

public:
	ASFPierCoreStation();
};
```

`Source/StarForge/Private/Ship/SFPierCoreStation.cpp`:

```cpp
#include "Ship/SFPierCoreStation.h"

ASFPierCoreStation::ASFPierCoreStation()
{
	// spec.py B_K3core: berth blocks at (-30, 0, 0) and (30, 0, 0) m, yaw 0. In centimetres, whole, so
	// Port1 is Port0 plus exactly (6000, 0, 0) cm and control N6's tie is exact to the bit (spec 7.5).
	BuildBlock(FVector(-3000.0, 0.0, 0.0));
	BuildBlock(FVector(3000.0, 0.0, 0.0));
}
```

- [ ] **Step 3: Собрать, создать ассет, прочитать строку станции**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
KIT_PLACES_GROUP=piercore "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET read /Game/Places/DA_Place_PierCore:\|Python script executed" $L
kit DA_Place_PierCore 60 25 "sf.FlightLog 1" task13-piercore-60
cat > /tmp/kit-piercore-expected.txt <<'TXT'
Station rig: 'Station' (SFPierCoreStation), frame present, no simulating body -- 2 blocks, 2 ports, 4 interior volumes, 30 physical colliders boxed in its own frame at X -18.38..101.62 Y -18.10..17.90 Z -16.00..16.00 m; Hall0 within its hall (0.000 m out), Boom0 within its channel (0.000 m out), Hall1 within its hall (0.000 m out), Boom1 within its channel (0.000 m out). Port0 at (0.00 -18.10 -2.75) m facing -Y: nothing physical past its plane (0.000 m), no interior volume past it (0.000 m); held by Ship, whose release would be 2.93 m in 1.0 s, peak 17.6 m/s2 -- inside its 3.0 m window, under the 19.62 m/s2 limit. Port1 at (60.00 -18.10 -2.75) m facing -Y: nothing physical past its plane (0.000 m), no interior volume past it (0.000 m); free.
TXT
grep -o "LogStarForge: Display: Station rig:.*" $L | head -1 | sed 's/^LogStarForge: Display: //' | tr -d '\r' | diff - /tmp/kit-piercore-expected.txt && echo "piercore: identical"
grep -cF "Docking port on Station at hull-local (6000 -1810 -275) cm, out along (0.00 -1.00 0.00), deck up (0.00 0.00 1.00)." $L   # 1
rm docs/superpowers/notes/evidence/$(date +%F)-task13-*.log
```

Плюс общие ворота. Ворота задачи — `piercore: identical`: оба отхода `2.93` значат, что обе стрелы кончаются в одной плоскости (правило 1), а четыре `0.000 m out` — что оболочки второго блока собраны от его же стен.

- [ ] **Step 4: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFPierCoreStation.h Source/StarForge/Private/Ship/SFPierCoreStation.cpp Content/Places/DA_Place_PierCore.uasset
git diff --cached --name-only
git commit -F - <<'MSG'
feat(station): the pier core, two berth blocks and two ports on one carrier

Step 2 of the station kit needs one station with two ports before the rule that
chooses between them can be tried on anything. The pier core is spec.py's
B_K3core: two M01 blocks side by side, both berths on -Y, in whole centimetres,
so the second port is the first moved by exactly 60 m and a tie between them can
be exact to the bit.

Its Station rig line reads both booms ending on one plane and every volume
inside its own walls. Until the rule exists the ship takes the first port by
name, which is Port0 -- the port the rule will choose from the hall's centre.
MSG
```

---

### Task 14: `SelectNearestFreePort` и восемь тестов шага 2

Правило ближайшего свободного узла (§9 спеки второй фазы, §7.2), чистой функцией по образцу `ComputeDockingApproach` и `CanDock`.

**Files:**
- Modify: `Source/StarForgeCore/Public/DockingMath.h` (перед закрывающей `}`)
- Modify: `Source/StarForgeCore/Private/DockingMath.cpp` (в конец пространства)
- Create: `Source/StarForgeCore/Private/Tests/PortSelectionTest.cpp`

**Interfaces:**
- Отдаёт задаче 15, в `namespace StarForge::Docking`:
  - `struct STARFORGECORE_API FPortCandidate { FTransform PortWorld; bool bOccupied; };`
  - `enum class EPortRefusal : uint8 { None, NoCandidates, AllOccupied, Unmeasurable };`
  - `struct STARFORGECORE_API FPortChoice { EPortRefusal Refusal; int32 ChosenIndex; double ChosenDistance; int32 RunnerUpIndex; double RunnerUpDistance; bool bTie; TArray<double> Distances; };`
  - `STARFORGECORE_API FPortChoice SelectNearestFreePort(const FTransform& RequestingPortWorld, const TArray<FPortCandidate>& Candidates);`

- [ ] **Step 1: Падающие тесты**

`Source/StarForgeCore/Private/Tests/PortSelectionTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "DockingMath.h"

#include <limits>

namespace
{
namespace PortSelectionTests
{
	constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

	StarForge::Docking::FPortCandidate At(const FVector& Location, bool bOccupied = false)
	{
		StarForge::Docking::FPortCandidate Candidate;
		Candidate.PortWorld = FTransform(Location);
		Candidate.bOccupied = bOccupied;
		return Candidate;
	}

	const FTransform FromOrigin(FVector::ZeroVector);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortPicksTheNearestTest,
	"StarForge.Core.Docking.NearestFreePortPicksTheNearest",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortPicksTheNearestTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	// The near one second in the array, so an answer of "always the first" fails too.
	const StarForge::Docking::FPortChoice Choice = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { At(FVector(60.0, 0.0, 0.0)), At(FVector(5.0, 0.0, 0.0)) });
	TestTrue(TEXT("a port is chosen"), Choice.Refusal == StarForge::Docking::EPortRefusal::None);
	TestEqual(TEXT("the one at 5 m"), Choice.ChosenIndex, 1);
	TestTrue(TEXT("at 5 m"), FMath::IsNearlyEqual(Choice.ChosenDistance, 5.0, 1e-12));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortSkipsTheOccupiedTest,
	"StarForge.Core.Docking.NearestFreePortSkipsTheOccupied",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortSkipsTheOccupiedTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	// Occupancy is a state of the node (spec 9 of the second phase): a held port is not offered, however near.
	const StarForge::Docking::FPortChoice Choice = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { At(FVector(5.0, 0.0, 0.0), true), At(FVector(60.0, 0.0, 0.0)) });
	TestEqual(TEXT("the far free one"), Choice.ChosenIndex, 1);
	TestTrue(TEXT("at 60 m"), FMath::IsNearlyEqual(Choice.ChosenDistance, 60.0, 1e-12));
	// Still measured, so the log line can say why the near one was passed over.
	TestTrue(TEXT("the held one's distance is reported"), FMath::IsNearlyEqual(Choice.Distances[0], 5.0, 1e-12));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortRefusesWhenAllOccupiedTest,
	"StarForge.Core.Docking.NearestFreePortRefusesWhenAllOccupied",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortRefusesWhenAllOccupiedTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	const StarForge::Docking::FPortChoice Choice = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { At(FVector(5.0, 0.0, 0.0), true), At(FVector(60.0, 0.0, 0.0), true) });
	TestTrue(TEXT("refused: all occupied"), Choice.Refusal == StarForge::Docking::EPortRefusal::AllOccupied);
	TestEqual(TEXT("no index"), Choice.ChosenIndex, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("both distances reported for the line"),
		Choice.Distances.Num() == 2 && FMath::IsNearlyEqual(Choice.Distances[1], 60.0, 1e-12));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortRefusesWithNoCandidatesTest,
	"StarForge.Core.Docking.NearestFreePortRefusesWithNoCandidates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortRefusesWithNoCandidatesTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	const StarForge::Docking::FPortChoice Choice =
		StarForge::Docking::SelectNearestFreePort(FromOrigin, TArray<StarForge::Docking::FPortCandidate>());
	TestTrue(TEXT("refused: no candidates"), Choice.Refusal == StarForge::Docking::EPortRefusal::NoCandidates);
	TestEqual(TEXT("no index"), Choice.ChosenIndex, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no distances"), Choice.Distances.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortTieGoesToTheLowerIndexTest,
	"StarForge.Core.Docking.NearestFreePortTieGoesToTheLowerIndex",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortTieGoesToTheLowerIndexTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	// Control N6's own numbers, in the game's centimetres (spec 7.5): the ship's port at (3000, 400,
	// -265), the pier core's ports at (0, -1810, -275) and (6000, -1810, -275). Whole numbers, so both
	// distances are sqrt(13 884 200) to the bit -- no tolerance for "almost equal", which there is
	// nothing to derive from.
	const StarForge::Docking::FPortChoice Choice = StarForge::Docking::SelectNearestFreePort(
		FTransform(FVector(3000.0, 400.0, -265.0)),
		{ At(FVector(0.0, -1810.0, -275.0)), At(FVector(6000.0, -1810.0, -275.0)) });
	TestEqual(TEXT("the lower index"), Choice.ChosenIndex, 0);
	TestTrue(TEXT("a tie, said"), Choice.bTie);
	TestEqual(TEXT("the other is the runner-up"), Choice.RunnerUpIndex, 1);
	TestTrue(TEXT("and its distance equals the chosen one exactly"), Choice.RunnerUpDistance == Choice.ChosenDistance);
	TestTrue(TEXT("37.26 m"), FMath::IsNearlyEqual(Choice.ChosenDistance, FMath::Sqrt(13884200.0), 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortReportsTheRunnerUpTest,
	"StarForge.Core.Docking.NearestFreePortReportsTheRunnerUp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortReportsTheRunnerUpTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	// The second number in the log is the next FREE port: the held one at 10 m is not where the ship
	// would go next, and naming it would send a pilot to a port he cannot take.
	const StarForge::Docking::FPortChoice Choice = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { At(FVector(5.0, 0.0, 0.0)), At(FVector(10.0, 0.0, 0.0), true), At(FVector(60.0, 0.0, 0.0)) });
	TestEqual(TEXT("chosen: the one at 5 m"), Choice.ChosenIndex, 0);
	TestEqual(TEXT("runner-up: the free one at 60 m"), Choice.RunnerUpIndex, 2);
	TestTrue(TEXT("at 60 m"), FMath::IsNearlyEqual(Choice.RunnerUpDistance, 60.0, 1e-12));
	TestFalse(TEXT("not a tie"), Choice.bTie);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortNeverChoosesTheUnmeasurableTest,
	"StarForge.Core.Docking.NearestFreePortNeverChoosesTheUnmeasurable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortNeverChoosesTheUnmeasurableTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	// The unmeasurable one first, where "the first one seen" would take it.
	const StarForge::Docking::FPortChoice Finite = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { At(FVector(NaN, 0.0, 0.0)), At(FVector(60.0, 0.0, 0.0)) });
	TestEqual(TEXT("a port nobody can place is passed over"), Finite.ChosenIndex, 1);

	const StarForge::Docking::FPortChoice None = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { At(FVector(NaN, 0.0, 0.0)), At(FVector(0.0, NaN, 0.0)) });
	TestTrue(TEXT("every free one unmeasurable: refused as unmeasurable"),
		None.Refusal == StarForge::Docking::EPortRefusal::Unmeasurable && None.ChosenIndex == INDEX_NONE);

	const StarForge::Docking::FPortChoice HeldAndBroken = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { At(FVector(NaN, 0.0, 0.0)), At(FVector(5.0, 0.0, 0.0), true) });
	TestTrue(TEXT("the only free one unmeasurable, the other held: unmeasurable"),
		HeldAndBroken.Refusal == StarForge::Docking::EPortRefusal::Unmeasurable);

	// A sound origin and a rotation nobody can read: CanDock refuses its NaN angles for ever (the
	// review's N35), so a choice by distance alone would send the ship to it while a healthy port
	// stood further off. Nearer, and first, so both "the nearest" and "the first seen" would take it.
	StarForge::Docking::FPortCandidate Twisted;
	Twisted.PortWorld = FTransform(FQuat(NaN, NaN, NaN, NaN), FVector(5.0, 0.0, 0.0));
	const StarForge::Docking::FPortChoice Rotated = StarForge::Docking::SelectNearestFreePort(
		FromOrigin, { Twisted, At(FVector(60.0, 0.0, 0.0)) });
	TestEqual(TEXT("a port whose rotation is not finite is passed over"), Rotated.ChosenIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockingNearestFreePortRefusesAnUnmeasurableRequestTest,
	"StarForge.Core.Docking.NearestFreePortRefusesAnUnmeasurableRequest",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNearestFreePortRefusesAnUnmeasurableRequestTest::RunTest(const FString& Parameters)
{
	using namespace PortSelectionTests;
	const StarForge::Docking::FPortChoice Free = StarForge::Docking::SelectNearestFreePort(
		FTransform(FVector(NaN, 0.0, 0.0)), { At(FVector(5.0, 0.0, 0.0)), At(FVector(60.0, 0.0, 0.0)) });
	TestTrue(TEXT("a request from nowhere is unmeasurable"), Free.Refusal == StarForge::Docking::EPortRefusal::Unmeasurable);

	// And it says so before it says anything about occupancy: a ship that cannot be placed is not told
	// the ports are full, which would send whoever reads it to look at the wrong thing.
	const StarForge::Docking::FPortChoice Held = StarForge::Docking::SelectNearestFreePort(
		FTransform(FVector(NaN, 0.0, 0.0)), { At(FVector(5.0, 0.0, 0.0), true), At(FVector(60.0, 0.0, 0.0), true) });
	TestTrue(TEXT("unmeasurable, not all occupied"), Held.Refusal == StarForge::Docking::EPortRefusal::Unmeasurable);

	const StarForge::Docking::FPortChoice Infinite = StarForge::Docking::SelectNearestFreePort(
		FTransform(FVector(std::numeric_limits<double>::infinity(), 0.0, 0.0)), { At(FVector(5.0, 0.0, 0.0)) });
	TestTrue(TEXT("an infinite request is unmeasurable too"), Infinite.Refusal == StarForge::Docking::EPortRefusal::Unmeasurable);

	const StarForge::Docking::FPortChoice Twisted = StarForge::Docking::SelectNearestFreePort(
		FTransform(FQuat(NaN, NaN, NaN, NaN), FVector::ZeroVector), { At(FVector(5.0, 0.0, 0.0)) });
	TestTrue(TEXT("a request whose rotation is not finite is unmeasurable"),
		Twisted.Refusal == StarForge::Docking::EPortRefusal::Unmeasurable);
	return true;
}
```

- [ ] **Step 2: Увидеть, что не собирается**

Сборка — ошибка: `FPortCandidate`, `FPortChoice`, `SelectNearestFreePort` не объявлены.

- [ ] **Step 3: Заголовок**

`Source/StarForgeCore/Public/DockingMath.h`, перед закрывающей `}`:

```cpp
	/** One port a hull might dock to: where it is, and whether somebody holds it (spec 9 of the second phase). */
	struct STARFORGECORE_API FPortCandidate
	{
		FTransform PortWorld = FTransform::Identity;
		bool bOccupied = false;
	};

	/** Why no port was chosen, or None. A bool would throw the reason away, as it would for CanDock. */
	enum class EPortRefusal : uint8
	{
		None,
		NoCandidates,
		AllOccupied,
		Unmeasurable
	};

	/** The choice and everything the log line needs to say why (station-kit spec 7.2, 7.7). */
	struct STARFORGECORE_API FPortChoice
	{
		EPortRefusal Refusal = EPortRefusal::NoCandidates;
		int32 ChosenIndex = INDEX_NONE;
		double ChosenDistance = 0.0;

		/** The next FREE candidate by distance, or INDEX_NONE. On a tie, the one tied with the chosen. */
		int32 RunnerUpIndex = INDEX_NONE;
		double RunnerUpDistance = 0.0;

		/** Another free candidate at exactly the chosen distance. Never silent: the game prints it. */
		bool bTie = false;

		/** To every candidate, held ones too, so the line can say why the near one was passed over; NaN where an origin is not finite. */
		TArray<double> Distances;
	};

	/**
	 * The nearest free port (spec 9 of the second phase, station-kit spec 7.2): by the distance between
	 * the port origins -- the angles are CanDock's business for the port chosen. Exact ties go to the
	 * LOWER index, so the caller must build Candidates in a deterministic order; there is no tolerance
	 * for "almost equal", because nothing exists to derive one from.
	 *
	 * Refusals in this order: no candidates; a request that cannot be placed (an origin or a rotation
	 * that is not finite); every candidate held; every free candidate unmeasurable. A candidate whose
	 * origin or rotation is not finite is never chosen: one with a sound origin and a NaN rotation is a
	 * sound distance away and refused by CanDock for ever (the review's N35). Lengths in the caller's
	 * unit, as everywhere in this file; the game passes centimetres and converts the distances to metres
	 * once, for the log.
	 */
	STARFORGECORE_API FPortChoice SelectNearestFreePort(
		const FTransform& RequestingPortWorld, const TArray<FPortCandidate>& Candidates);
```

- [ ] **Step 4: Реализация**

`Source/StarForgeCore/Private/DockingMath.cpp`, в конец пространства:

```cpp
	namespace
	{
		/**
		 * A port whose pose can be read at all: its origin AND its rotation finite. Not the origin
		 * alone: a port with a sound origin and a NaN rotation is a sound distance away, and CanDock
		 * refuses its NaN angles for ever (the review's N35), so a choice by distance would send the
		 * ship to it while a healthy port stood further off.
		 */
		bool IsPlaceable(const FTransform& Port)
		{
			const FQuat Rotation = Port.GetRotation();
			return IsFiniteVector(Port.GetLocation())
				&& FMath::IsFinite(Rotation.X) && FMath::IsFinite(Rotation.Y)
				&& FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W);
		}
	}

	FPortChoice SelectNearestFreePort(const FTransform& RequestingPortWorld, const TArray<FPortCandidate>& Candidates)
	{
		FPortChoice Choice;
		Choice.Distances.Init(std::numeric_limits<double>::quiet_NaN(), Candidates.Num());

		if (Candidates.IsEmpty())
		{
			Choice.Refusal = EPortRefusal::NoCandidates;
			return Choice;
		}

		// Before occupancy: a ship that cannot be placed is not told the ports are full.
		const FVector From = RequestingPortWorld.GetLocation();
		if (!IsPlaceable(RequestingPortWorld))
		{
			Choice.Refusal = EPortRefusal::Unmeasurable;
			return Choice;
		}

		bool bAnyFree = false;
		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			const FVector To = Candidates[i].PortWorld.GetLocation();
			if (IsFiniteVector(To))
			{
				Choice.Distances[i] = FVector::Dist(From, To);
			}
			bAnyFree |= !Candidates[i].bOccupied;
		}

		if (!bAnyFree)
		{
			Choice.Refusal = EPortRefusal::AllOccupied;
			return Choice;
		}

		// Strictly less: an exact tie keeps the lower index (spec 7.5). The finiteness test is what keeps
		// a port nobody can place out of the choice -- the first candidate seen is taken unconditionally.
		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			if (Candidates[i].bOccupied || !IsPlaceable(Candidates[i].PortWorld))
			{
				continue;
			}
			if (Choice.ChosenIndex == INDEX_NONE || Choice.Distances[i] < Choice.ChosenDistance)
			{
				Choice.ChosenIndex = i;
				Choice.ChosenDistance = Choice.Distances[i];
			}
		}

		if (Choice.ChosenIndex == INDEX_NONE)
		{
			Choice.Refusal = EPortRefusal::Unmeasurable;
			return Choice;
		}

		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			if (i == Choice.ChosenIndex || Candidates[i].bOccupied || !IsPlaceable(Candidates[i].PortWorld))
			{
				continue;
			}
			if (Choice.RunnerUpIndex == INDEX_NONE || Choice.Distances[i] < Choice.RunnerUpDistance)
			{
				Choice.RunnerUpIndex = i;
				Choice.RunnerUpDistance = Choice.Distances[i];
			}
		}

		Choice.bTie = Choice.RunnerUpIndex != INDEX_NONE && Choice.RunnerUpDistance == Choice.ChosenDistance;
		Choice.Refusal = EPortRefusal::None;
		return Choice;
	}
```

- [ ] **Step 5: Собрать и прогнать — должно пройти**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log < /dev/null
grep -c "Test Completed. Result={Success}" $L   # N₀ + 16 (138 при N₀ = 122, база после ревью)
grep -c "Test Completed. Result={Fail}" $L      # 0
```

Эталон для мутантов снимается так же, как в задаче 7: сразу после зелёного прогона и до первого мутанта. Второй `mkdir` без `-p` — чтобы не снять копию поверх прежней.

```bash
mkdir -p Saved/Mutants && mkdir Saved/Mutants/task14 && cp Source/StarForgeCore/Private/DockingMath.cpp Source/StarForgeCore/Public/DockingMath.h Saved/Mutants/task14/
ls Saved/Mutants/task14    # DockingMath.cpp  DockingMath.h
```

- [ ] **Step 6: Коммит**

Здесь или после шага 7 — откат мутантов от коммита не зависит.

```bash
git status --short
git add Source/StarForgeCore/Public/DockingMath.h Source/StarForgeCore/Private/DockingMath.cpp Source/StarForgeCore/Private/Tests/PortSelectionTest.cpp
git diff --cached --name-only
git commit -F - <<'MSG'
feat(core): choose the nearest free port, with ties and refusals said rather than decided by nothing

Spec 9 of the second phase asks for the nearest free node, computed from pose
and occupancy, as a pure function under tests. The station kit brings it forward
from stage 7: on stations that do not move, the rule can be tried on its own.

It returns the choice and everything the log needs: every candidate's distance,
held ones included, so a line can say why the nearest was passed over; the next
free port, not merely the next; and a tie flag. Exact ties go to the lower index
of a deterministic order, and there is no tolerance for almost-equal, because
nothing exists to derive one from. A port nobody can place is never chosen, and
a ship that cannot be placed is told so before it is told the ports are full.
MSG
```

- [ ] **Step 7: Мутанты**

По одному, в `DockingMath.cpp`: сборка, прогон подветки `StarForge.Core.Docking`, запись упавших, откат копией. Следующий мутант — только после `restored`:

```bash
cp Saved/Mutants/task14/DockingMath.cpp Source/StarForgeCore/Private/DockingMath.cpp
cp Saved/Mutants/task14/DockingMath.h Source/StarForgeCore/Public/DockingMath.h
cmp Saved/Mutants/task14/DockingMath.cpp Source/StarForgeCore/Private/DockingMath.cpp && cmp Saved/Mutants/task14/DockingMath.h Source/StarForgeCore/Public/DockingMath.h && echo restored
```

| № | Мутант | Что написать вместо | Кто обязан упасть |
|---|---|---|---|
| P1 | возвращать дальний | в первом цикле выбора `Choice.Distances[i] > Choice.ChosenDistance` | `NearestFreePortPicksTheNearest` |
| P2 | снята проверка занятости | `bAnyFree = true;` вместо `bAnyFree \|= …`, и убрать `Candidates[i].bOccupied \|\|` из обоих циклов | `NearestFreePortSkipsTheOccupied`, `NearestFreePortRefusesWhenAllOccupied` |
| P3 | пустой массив даёт индекс 0 | в ветке `IsEmpty()`: `Choice.Refusal = EPortRefusal::None; Choice.ChosenIndex = 0;` | `NearestFreePortRefusesWithNoCandidates` |
| P4 | ничья к большему индексу | `Choice.Distances[i] <= Choice.ChosenDistance` в первом цикле | `NearestFreePortTieGoesToTheLowerIndex` |
| P5 | «второе число» — следующий вообще | убрать `Candidates[i].bOccupied \|\|` только из второго цикла | `NearestFreePortReportsTheRunnerUp` |
| P6 | «второе число» равно выбранному | убрать `i == Choice.ChosenIndex \|\|` из второго цикла | `NearestFreePortReportsTheRunnerUp` |
| P7 | снята конечность кандидата | убрать `\|\| !IsPlaceable(Candidates[i].PortWorld)` из первого цикла | `NearestFreePortNeverChoosesTheUnmeasurable` |
| P8 | снята конечность просящего | удалить блок `if (!IsPlaceable(RequestingPortWorld)) { … }` | `NearestFreePortRefusesAnUnmeasurableRequest` |
| P9 | поворот не проверяется (добавлен 2026-10-14, поправка П6) | тело `IsPlaceable` — `return IsFiniteVector(Port.GetLocation());` | `NearestFreePortNeverChoosesTheUnmeasurable`, `NearestFreePortRefusesAnUnmeasurableRequest` |

После всех — откат той же тройкой команд (`restored`), `grep -c "SelectNearestFreePort" Source/StarForgeCore/Private/DockingMath.cpp` ≥ 1 (функция задачи на месте), сборка, полный прогон `StarForge.Core`: **N₀ + 16 / 0**. Затем `rm -rf Saved/Mutants/task14`. Таблица «ожидалось / упало» — в бланк точки 12.

---

### Task 15: Выбор узла каждый кадр, «ещё не выбирал», строки выбора и `DA_Place_PierCoreTie`

Сегодня `ResolvePorts` запоминает станцию навсегда (`bPortsResolved`). Здесь выбор делается каждый кадр в `Free` и на каждом решении о стыковке, фиксируется от `EnterCapture` до `LetGo`, а `ResolvePorts` в `BeginPlay` станционного порта больше не касается (§7.4). Строки выбора — только когда кандидатов два и больше: на четырёх старых стендах не меняется ни одна строка (§7.7).

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFDockingComponent.h` (публичные `HasNoFreePort`, `DescribeHeldPorts`; приватные методы и поля)
- Modify: `Source/StarForge/Private/Ship/SFDockingComponent.cpp` (include `<limits>`; новое безымянное пространство `DockingKit`; `ResolvePorts` — блок задачи 5; `EnterCapture`; `RequestDock`; `DockImmediately`; `TickComponent`; новые методы)
- Create: `Content/Places/DA_Place_PierCoreTie.uasset`

**Interfaces:**
- Потребляет: `SelectNearestFreePort`, `FPortCandidate`, `FPortChoice`, `EPortRefusal` (задача 14); `ISFDockingStation::FindAll`, `GetDockingPorts` (задача 5); `USFDockingPortComponent::IsHeld`, `GetHolder` (задача 6).
- Отдаёт задачам 17–19: `bool USFDockingComponent::HasNoFreePort() const`; `FString DescribeHeldPorts(bool bWithDistances) const` → `2 of 2 held (Station.Port0 by Parked0 at 17.90 m, …)`; строки `Dock: [X] nearest free port is now …`.

- [ ] **Step 1: «Красный» шаг**

```bash
kit DA_Place_PierCore 60 40 "sf.FlightLog 1, sf.Dock 2" task15-red-60
grep -c "nearest free port is now" $L   # 0: ни на старте, ни после отхода
rm docs/superpowers/notes/evidence/$(date +%F)-task15-red-60.log
```

- [ ] **Step 2: Объявления**

`Source/StarForge/Public/Ship/SFDockingComponent.h`, публичные, после `MeasureRelease`:

```cpp
	/** Every candidate is held, as of the last choice (spec 7.7): the flight line and the panel say so. */
	bool HasNoFreePort() const { return bNoFreePort; }

	/** "2 of 2 held (Station.Port0 by Parked0 at 17.90 m, ...)", as of the last choice; distances optional. */
	FString DescribeHeldPorts(bool bWithDistances) const;
```

Приватные, после `ResolvePorts`:

```cpp
	/**
	 * Chooses this frame's station port by the nearest-free-port rule (spec 7.2) among every docking
	 * station's ports, in candidate order -- stations by name, then GetDockingPorts (spec 7.5) -- and
	 * logs a change of the choice or of any candidate's holder when there are two or more candidates.
	 * bDecision is true for RequestDock and DockImmediately, where a choice made on an exact tie is
	 * logged even if nothing changed.
	 */
	void SelectPort(bool bDecision);

	void LogPortChoice() const;

	FString HeldPortsList(bool bWithDistances, int32& OutHeld) const;

	int32 CountCandidates() const;

	int32 CandidateCount = 0;

	/**
	 * "Not yet chosen": false for a new ship and from EnterCapture to LetGo, and not the same thing as
	 * the answer "no free port". The first choice after it is always a change, whatever it says -- so
	 * a ship that starts free with every port held still prints its line (control N4), and a ship
	 * back from a release prints its first choice even when it is the port it left (H2.2).
	 */
	bool bHasChosen = false;

	bool bNoFreePort = false;

	TWeakObjectPtr<USFDockingPortComponent> LastChosenPort;
	TArray<TWeakObjectPtr<USFDockingPortComponent>> LastCandidatePorts;
	TArray<TWeakObjectPtr<USFDockingComponent>> LastCandidateHolders;
	StarForge::Docking::FPortChoice LastChoice;
```

Комментарий у `ResolvePorts` заменить на `/** Resolves this ship's own components and says whether any station exists; once. It never chooses the station's port. */`.

- [ ] **Step 3: Реализация**

`Source/StarForge/Private/Ship/SFDockingComponent.cpp`: добавить `#include <limits>`. После существующего безымянного пространства (строка 54):

```cpp
namespace
{
namespace DockingKit
{
	/** "Station.Port1": how every selection line names a port. */
	FString PortLabel(const USFDockingPortComponent* Port)
	{
		return Port ? FString::Printf(TEXT("%s.%s"), *GetNameSafe(Port->GetOwner()), *Port->GetName()) : FString(TEXT("none"));
	}

	/** Every docking station's ports: stations by name, ports in build order -- the candidate order of spec 7.5. */
	TArray<USFDockingPortComponent*> CollectCandidates(const UWorld* World, const AActor* Except)
	{
		TArray<USFDockingPortComponent*> Ports;
		for (AActor* Station : ISFDockingStation::FindAll(World, Except))
		{
			TArray<USFDockingPortComponent*> StationPorts;
			Cast<ISFDockingStation>(Station)->GetDockingPorts(StationPorts);
			Ports.Append(StationPorts);
		}
		return Ports;
	}
}
}
```

В `ResolvePorts` заменить блок задачи 5 (от комментария «Every actor that says it is a docking station…» до `bHasStation = StationPort != nullptr;`) на:

```cpp
	// Only this ship's own components are resolved here, once. The station's port is chosen by
	// SelectPort -- every frame while Free and at each decision to dock (station-kit spec 7.4) -- and
	// never here: BeginPlay calls this after the place rig has already docked the ship (the dock
	// before Super::StartPlay, BeginPlay inside it), and a choice made then would move StationPort off
	// the port the hull is attached to and leave that port held after LetGo.
	CandidateCount = CountCandidates();
	bHasStation = CandidateCount > 0;
```

В `EnterCapture`, после `State = ESFDockingState::Capturing;`:

```cpp
	// From here to LetGo the port is fixed: it is the one this hull holds, and the release is measured
	// along its axis. The memory of the last choice goes back to "not yet chosen", so the first choice
	// after the release is printed even when it is the port just left (spec 7.4).
	bHasChosen = false;
```

В `RequestDock`, после проверки `State != ESFDockingState::Free` и **перед** `if (!bHasStation)`:

```cpp
	// Asked at the decision, not remembered from the last frame; a choice made on an exact tie is said
	// here even when the frame's choice already said it (spec 7.5).
	SelectPort(true);
```

В `DockImmediately`, после `ResolvePorts();`:

```cpp
	SelectPort(true);
```

В `TickComponent`, ветка `case ESFDockingState::Free:` — перед `UpdateApproach();`:

```cpp
		SelectPort(false);
```

Новые методы, в конец файла:

```cpp
int32 USFDockingComponent::CountCandidates() const
{
	return DockingKit::CollectCandidates(GetWorld(), GetOwner()).Num();
}

void USFDockingComponent::SelectPort(bool bDecision)
{
	const TArray<USFDockingPortComponent*> Ports = DockingKit::CollectCandidates(GetWorld(), GetOwner());
	CandidateCount = Ports.Num();
	bHasStation = CandidateCount > 0;

	TArray<StarForge::Docking::FPortCandidate> Candidates;
	Candidates.Reserve(Ports.Num());
	for (const USFDockingPortComponent* Candidate : Ports)
	{
		StarForge::Docking::FPortCandidate& Entry = Candidates.AddDefaulted_GetRef();
		Entry.PortWorld = Candidate->GetPortTransform();
		Entry.bOccupied = Candidate->IsHeld();
	}

	// A ship without a port of its own asks from nowhere, and the rule refuses a request it cannot place.
	const FTransform Request = Port
		? Port->GetPortTransform()
		: FTransform(FVector(std::numeric_limits<double>::quiet_NaN()));
	LastChoice = StarForge::Docking::SelectNearestFreePort(Request, Candidates);

	USFDockingPortComponent* Chosen = (LastChoice.Refusal == StarForge::Docking::EPortRefusal::None)
		? Ports[LastChoice.ChosenIndex] : nullptr;
	StationPort = Chosen;
	bNoFreePort = (LastChoice.Refusal == StarForge::Docking::EPortRefusal::AllOccupied);

	// A change is a different port chosen, or any candidate changing hands. The second is what control
	// N8 needs: a port freed that does not become the nearest would otherwise leave no line at all, and
	// a remembered choice could not be told from a correct one.
	bool bChanged = !bHasChosen || LastChosenPort.Get() != Chosen || LastCandidatePorts.Num() != Ports.Num();
	for (int32 i = 0; !bChanged && i < Ports.Num(); ++i)
	{
		bChanged = LastCandidatePorts[i].Get() != Ports[i] || LastCandidateHolders[i].Get() != Ports[i]->GetHolder();
	}

	LastCandidatePorts.Reset();
	LastCandidateHolders.Reset();
	for (USFDockingPortComponent* Candidate : Ports)
	{
		LastCandidatePorts.Add(Candidate);
		LastCandidateHolders.Add(Candidate->GetHolder());
	}
	LastChosenPort = Chosen;
	bHasChosen = true;

	// With one candidate there is nothing to choose, and the four old rigs print not one new line.
	if (CandidateCount >= 2 && (bChanged || (bDecision && LastChoice.bTie)))
	{
		LogPortChoice();
	}
}

FString USFDockingComponent::HeldPortsList(bool bWithDistances, int32& OutHeld) const
{
	TArray<FString> Entries;
	for (int32 i = 0; i < LastCandidatePorts.Num(); ++i)
	{
		const USFDockingPortComponent* Candidate = LastCandidatePorts[i].Get();
		const USFDockingComponent* Holder = Candidate ? Candidate->GetHolder() : nullptr;
		if (!Holder)
		{
			continue;
		}
		FString Entry = FString::Printf(TEXT("%s by %s"), *DockingKit::PortLabel(Candidate), *GetNameSafe(Holder->GetOwner()));
		if (bWithDistances && LastChoice.Distances.IsValidIndex(i))
		{
			Entry += FString::Printf(TEXT(" at %.2f m"), StarForge::Units::CmToMetres(LastChoice.Distances[i]));
		}
		Entries.Add(Entry);
	}
	OutHeld = Entries.Num();
	return FString::Join(Entries, TEXT(", "));
}

FString USFDockingComponent::DescribeHeldPorts(bool bWithDistances) const
{
	int32 Held = 0;
	const FString List = HeldPortsList(bWithDistances, Held);
	return FString::Printf(TEXT("%d of %d held (%s)"), Held, LastCandidatePorts.Num(), *List);
}

void USFDockingComponent::LogPortChoice() const
{
	const FString ShipName = GetNameSafe(GetOwner());
	const auto Label = [this](int32 Index) { return DockingKit::PortLabel(LastCandidatePorts[Index].Get()); };
	const auto Metres = [this](int32 Index) { return StarForge::Units::CmToMetres(LastChoice.Distances[Index]); };

	switch (LastChoice.Refusal)
	{
	case StarForge::Docking::EPortRefusal::AllOccupied:
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] nearest free port is now none: %s."),
			*ShipName, *DescribeHeldPorts(true));
		return;

	case StarForge::Docking::EPortRefusal::None:
	{
		FString Text = FString::Printf(TEXT("nearest free port is now %s at %.2f m"),
			*Label(LastChoice.ChosenIndex), Metres(LastChoice.ChosenIndex));
		if (LastChoice.bTie)
		{
			// Never silent (spec 9 of the second phase): both numbers and the rule that decided.
			Text += FString::Printf(TEXT(", tied with %s at %.2f m and taken by the lower index"),
				*Label(LastChoice.RunnerUpIndex), Metres(LastChoice.RunnerUpIndex));
		}
		else if (LastChoice.RunnerUpIndex == INDEX_NONE)
		{
			Text += TEXT("; next free none");
		}
		else
		{
			Text += FString::Printf(TEXT("; next free %s at %.2f m"),
				*Label(LastChoice.RunnerUpIndex), Metres(LastChoice.RunnerUpIndex));
		}

		// With the held ones and their distances, so the line says why the nearer was passed over.
		int32 Held = 0;
		const FString List = HeldPortsList(true, Held);
		if (Held > 0)
		{
			Text += TEXT("; held ") + List;
		}
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] %s."), *ShipName, *Text);
		return;
	}

	default:
		UE_LOG(LogStarForge, Warning, TEXT("Dock: [%s] nearest free port could NOT BE MEASURED: %s; no port is chosen."),
			*ShipName, LastChoice.Refusal == StarForge::Docking::EPortRefusal::NoCandidates
				? TEXT("there are no candidates") : TEXT("a port position is not finite"));
		return;
	}
}
```

- [ ] **Step 4: Ассет ничьей, собрать, H2.1, H2.2 и N6 на 60**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
KIT_PLACES_GROUP=tie "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET read /Game/Places/DA_Place_PierCoreTie:\|Python script executed" $L   # carriers [SFPierCoreStation 'Station' at (-30.00 0.00 0.00)]

kit DA_Place_PierCore 60 25 "sf.FlightLog 1" task15-h21-60
grep -cF "Dock: [Ship] nearest free port is now Station.Port0 at 22.10 m; next free Station.Port1 at 63.94 m." $L   # 1
awk '/nearest free port is now Station.Port0 at 22.10 m/{if(!n)n=NR} /Station rig:/{if(!s)s=NR} END{print (n && s && n<s) ? "choice before the station line" : "ORDER WRONG"}' $L

kit DA_Place_PierCore 60 40 "sf.FlightLog 1, sf.Dock 2" task15-h22-60
awk '/Dock: \[Ship\] undocked\./{u=1} u && /nearest free port is now/{print; exit}' $L   # ... is now Station.Port0 at 2.93 m; next free Station.Port1 at 60.07 m.

kit DA_Place_PierCoreTie 60 25 "sf.FlightLog 1" task15-n6-60
grep -cF "Place carrier: 'Station' of class SFPierCoreStation in place 'PierCoreTie' at place-local (-30.0 0.0 0.0) m" $L   # 1
grep -cF "Dock: [Ship] nearest free port is now Station.Port0 at 37.26 m, tied with Station.Port1 at 37.26 m and taken by the lower index." $L   # >= 1
rm docs/superpowers/notes/evidence/$(date +%F)-task15-*.log
```

Плюс общие ворота; у N6 законна одна `Warning` — `SFCharacter bound to … by search, not by volume` (§8.3: начало мира лежит в сплошной массе блока 0). Если в N6 оба числа напечатаны одинаковыми, а хвоста `tied` нет, — находка о симметрии сборки, а не о правиле (§7.5).

- [ ] **Step 5: Регрессия `kit-2.2`**

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-2.2
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-2.2; echo "exit $?"
```

`compare: 0 findings`: на станциях с одним портом выбор каждый кадр даёт тот же порт, строк выбора нет, строка `Docking: [Ship] station found …` пишется там же и тем же текстом. Строка в таблицу регрессий.

- [ ] **Step 6: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFDockingComponent.h Source/StarForge/Private/Ship/SFDockingComponent.cpp Content/Places/DA_Place_PierCoreTie.uasset docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
feat(docking): choose the station port by the nearest-free rule every frame while free

ResolvePorts remembered the first station it found for the whole session. The
choice is now made every frame while the ship is free and again at every
decision to dock, from every docking station's ports in a deterministic order,
and it is fixed from the start of a capture to the let-go, because the release
is measured along the held port's axis.

ResolvePorts no longer touches the station's port at all. BeginPlay calls it
after the place rig has already docked the ship, and without the old cache a
choice made there would have moved the port off the one the hull is attached to.

"Not yet chosen" is its own state, so the first choice after a release or at a
free start is always printed. With a single candidate nothing is printed, and
the evidence replay of all four rigs is unchanged line for line.
MSG
```

---

### Task 16: Значение экипажа `Parked`

Порт шага 2 занимает настоящий пристыкованный корпус (решение 4). Ему нужно третье значение экипажа: «не слушает консоль» у автоматики и преследования сегодня значит «автоматический», и с третьим значением это чтение неверно — они сели бы (§7.6). Задача меняет корабль, операторов и `LetGo`, на которых стоят бой и пират; отсюда своя регрессия.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h` (перечисление, строки 45–50; getter после строки 139)
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp` (`BeginPlay`, строки 529–541 и 593–600)
- Modify: `Source/StarForge/Public/Ship/SFAutomationOperatorComponent.h` (после строки 64; поле после строки 70)
- Modify: `Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp` (строки 138 и 169)
- Modify: `Source/StarForge/Public/Ship/SFPursuitOperatorComponent.h` (после строки 103; поле после строки 119)
- Modify: `Source/StarForge/Private/Ship/SFPursuitOperatorComponent.cpp` (строка 41)
- Modify: `Source/StarForge/Public/Ship/SFConsoleOperatorComponent.h`, `SFPilotStationComponent.h`, `SFWeaponComponent.h` (getter рядом с `SetConsoleControlled`)
- Modify: `Source/StarForge/Public/Ship/SFDockingComponent.h`, `Source/StarForge/Private/Ship/SFDockingComponent.cpp` (`LetGo`; getter, флаг, `StateName`)
- Modify: `Source/StarForge/Public/Ship/SFThrusterArrayComponent.h` (комментарий к `SetLocked`; добавлено 2026-10-14, поправка П7)

**Interfaces:**
- Отдаёт задачам 17–19:
  - `ESFShipCrewing::Parked` (после `Automated`)
  - `void USFAutomationOperatorComponent::SetMayTakeSeat(bool)`, `void USFPursuitOperatorComponent::SetMayTakeSeat(bool)`
  - `bool IsConsoleControlled() const` у `USFPilotStationComponent`, `USFConsoleOperatorComponent`, `USFAutomationOperatorComponent`, `USFPursuitOperatorComponent`, `USFDockingComponent`, `USFWeaponComponent` (у `USFThrusterArrayComponent` он уже есть, строка 104)
  - `USFConsoleOperatorComponent* ASFTestShip::GetConsoleOperator() const`
  - `void USFDockingComponent::SetThrustersStayLocked(bool)`; строка `Dock: [X] undocked; its thrusters stay locked: it is parked, and nobody crews it.`
  - `static const TCHAR* USFDockingComponent::StateName(ESFDockingState)` → `free`, `capturing`, `docked`, `releasing`

- [ ] **Step 1: Значение**

`Source/StarForge/Public/Ship/SFTestShip.h`, перечисление (строки 45–50):

```cpp
UENUM()
enum class ESFShipCrewing : uint8
{
	PlayerShip,
	Automated,

	/**
	 * A hull standing docked at a port with nobody aboard (station-kit spec 7.6). No operator takes
	 * either post -- which "does not answer sf.*" can no longer be taken to mean, since Automated is
	 * not the only hull that does not. Its thrusters stay locked all its life. It answers no sf.*
	 * except three named ones: sf.Shields, which every hull answers; sf.UndockParked, which names it;
	 * and sf.DockRelease, through the RequestUndock sf.UndockParked calls. Appended, so the first two
	 * keep their numbers.
	 */
	Parked
};
```

После `GetAutomationOperator()` (строка 139):

```cpp
	USFConsoleOperatorComponent* GetConsoleOperator() const { return ConsoleOperator; }
```

- [ ] **Step 2: Кто может сесть**

`Source/StarForge/Public/Ship/SFAutomationOperatorComponent.h`, после `SetConsoleControlled` (строка 64):

```cpp
	bool IsConsoleControlled() const { return bConsoleControlled; }

	/**
	 * Whether this operator may take the gunner's seat at all. Its own question since the parked hull
	 * (station-kit spec 7.6): "does not answer sf.*" used to mean "automated", and a parked hull does
	 * not answer sf.* either. True by default, so a hull nobody told anything behaves as before.
	 */
	void SetMayTakeSeat(bool bIn) { bMayTakeSeat = bIn; }
```

поле после `bool bConsoleControlled = true;` (строка 70): `bool bMayTakeSeat = true;`

`Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp`, строка 138:

```cpp
	bWantedSeatLastTick = bMayTakeSeat && (!bConsoleControlled || CVarAutomationOperator.GetValueOnGameThread() != 0);
```

строка 169:

```cpp
	const bool bWantsSeat = bMayTakeSeat && (!bConsoleControlled || CVarAutomationOperator.GetValueOnGameThread() != 0);
```

`Source/StarForge/Public/Ship/SFPursuitOperatorComponent.h`, после `SetConsoleControlled` (строка 103):

```cpp
	bool IsConsoleControlled() const { return bConsoleControlled; }

	/** Whether this operator may take the pilot's seat at all: on an Automated hull only. See ESFShipCrewing::Parked. */
	void SetMayTakeSeat(bool bIn) { bMayTakeSeat = bIn; }
```

поле после `bool bConsoleControlled = true;` (строка 119): `bool bMayTakeSeat = true;`

`Source/StarForge/Private/Ship/SFPursuitOperatorComponent.cpp`, строка 41:

```cpp
	if (bConsoleControlled || !bMayTakeSeat)
```

Комментарий над ней («Silent on the player's ship…») дополнить: `And on a parked hull, where nobody flies at all.`

`SFConsoleOperatorComponent.h`, `SFPilotStationComponent.h`, `SFWeaponComponent.h` — после `SetConsoleControlled` в каждом:

```cpp
	bool IsConsoleControlled() const { return bConsoleControlled; }
```

Консольному оператору нового флага не нужно: он садится только при `bConsoleControlled`, а это и есть «только на `PlayerShip`» (`SFConsoleOperatorComponent.cpp:78-84`).

- [ ] **Step 3: Корабль раздаёт**

`Source/StarForge/Private/Ship/SFTestShip.cpp`, строки 529–541 заменить на:

```cpp
	const bool bAnswersConsole = (Crewing == ESFShipCrewing::PlayerShip);
	if (PilotStation) { PilotStation->SetConsoleControlled(bAnswersConsole); }
	if (Thrusters) { Thrusters->SetConsoleControlled(bAnswersConsole); }
	if (ConsoleOperator) { ConsoleOperator->SetConsoleControlled(bAnswersConsole); }

	// Who may take a seat is told apart from who answers the console, because since the parked hull
	// the two no longer coincide (station-kit spec 7.6). The automation takes the gunner's post on the
	// player's ship (by sf.AutomationOperator) and on an automated one (always), never on a parked one;
	// the pursuit flies an automated hull and nothing else. The console operator's own rule already is
	// "the player's ship only", because it sits only while it answers the console.
	if (AutomationOperator)
	{
		AutomationOperator->SetConsoleControlled(bAnswersConsole);
		AutomationOperator->SetMayTakeSeat(Crewing != ESFShipCrewing::Parked);
	}
	if (PursuitOperator)
	{
		PursuitOperator->SetConsoleControlled(bAnswersConsole);
		PursuitOperator->SetMayTakeSeat(Crewing == ESFShipCrewing::Automated);
	}
	if (Docking)
	{
		Docking->SetConsoleControlled(bAnswersConsole);
		Docking->SetThrustersStayLocked(Crewing == ESFShipCrewing::Parked);
	}
	if (Weapon)
	{
		Weapon->SetConsoleControlled(bAnswersConsole);
		Weapon->SetWeaponsFree(bWeaponsFree);
	}
```

Строки 593–600 (ветка `else` без дрона) заменить на:

```cpp
	else if (Crewing == ESFShipCrewing::Parked)
	{
		// Not "its automation will track whatever it is given": a parked hull has no automation seated.
		UE_LOG(LogStarForge, Display,
			TEXT("Ship [%s] spawns no target drone; it is parked, and nobody holds its gunner's post."), *GetName());
	}
	else
	{
		// Said out loud: a gunner post with no target looks exactly like a gunner post
		// that is not working, and this is the line that tells the two apart.
		UE_LOG(LogStarForge, Display,
			TEXT("Ship [%s] spawns no target drone; its gunner's automation will track whatever it is given."),
			*GetName());
	}
```

- [ ] **Step 4: `LetGo` не отпирает двигатели припаркованного**

`Source/StarForge/Public/Ship/SFDockingComponent.h`, после `SetConsoleControlled` (строка 188):

```cpp
	bool IsConsoleControlled() const { return bConsoleControlled; }

	/**
	 * A parked hull's array stays locked for life (station-kit spec 7.6): LetGo simulates it and leaves
	 * the lock on, and says so. Nobody commands it, and a locked array keeps that true the day an
	 * operator exists.
	 */
	void SetThrustersStayLocked(bool bIn) { bThrustersStayLocked = bIn; }

	/** "free", "capturing", "docked", "releasing": read inside a sentence. */
	static const TCHAR* StateName(ESFDockingState InState);
```

поле после `bool bConsoleControlled = true;`: `bool bThrustersStayLocked = false;`

`Source/StarForge/Private/Ship/SFDockingComponent.cpp`, в `LetGo` заменить блок `if (Thrusters) { … Thrusters->SetLocked(false); }` и строку `undocked.` на:

```cpp
	if (Thrusters)
	{
		// The array reads its mass and inertia back from Chaos, and a hull that spent the
		// start of the session kinematic may have handed it a kinematic body's properties.
		// This is the instant those numbers start meaning something again, and nobody is
		// flying in it.
		Thrusters->Rebuild();
		if (!bThrustersStayLocked)
		{
			Thrusters->SetLocked(false);
		}
	}

	State = ESFDockingState::Free;
	CaptureElapsedSeconds = 0.0;

	if (bThrustersStayLocked)
	{
		UE_LOG(LogStarForge, Display,
			TEXT("Dock: [%s] undocked; its thrusters stay locked: it is parked, and nobody crews it."), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] undocked."), *Owner->GetName());
	}
```

(строки `State = …` и `CaptureElapsedSeconds = …` в `LetGo` уже стоят — оставить одну пару).

`Source/StarForge/Public/Ship/SFThrusterArrayComponent.h`, в комментарии к `SetLocked` (добавлено 2026-10-14, поправка П7). Ревью 2026-10-01 развело замки: `SetLocked` — только стыковочный, обесточивание — свой (`IsLocked()` читает `bLocked || IsPoweredDown()`), и комментарий говорит «undocking clears it». У припаркованного `LetGo` его не снимает (§7.6), так что эта фраза станет ложью. Механизм спеки остаётся, правится текст. Строку

```cpp
	 * This is the docking lock, and undocking clears it. A powered-down ship is held by the
	 * second lock IsLocked asks after, which this does not touch.
```

заменить на

```cpp
	 * This is the docking lock, and undocking clears it -- except on a parked hull, whose LetGo
	 * leaves it on for the hull's life (station-kit spec 7.6; USFDockingComponent::
	 * SetThrustersStayLocked). A powered-down ship is held by the second lock IsLocked asks after,
	 * which this does not touch.
```

и добавить путь в `git add` шага 6. Новый метод:

```cpp
const TCHAR* USFDockingComponent::StateName(ESFDockingState InState)
{
	switch (InState)
	{
	case ESFDockingState::Free: return TEXT("free");
	case ESFDockingState::Capturing: return TEXT("capturing");
	case ESFDockingState::Docked: return TEXT("docked");
	case ESFDockingState::Releasing: return TEXT("releasing");
	}
	return TEXT("in no known state");
}
```

- [ ] **Step 5: Собрать, регрессия `kit-2.4`**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
python scripts/regress_evidence.py --out Saved/Regress/kit-2.4
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-2.4; echo "exit $?"
```

`compare: 0 findings`. Ворота этой задачи — ровно здесь: на `PlayerShip` и `Automated` кто садится и кто слушает консоль не изменилось ни в одном повторе боя и петли. Сам `Parked` впервые появляется в мире в задаче 17, и там его читает строка `Place parked hull:`. Строка в таблицу регрессий.

- [ ] **Step 6: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp Source/StarForge/Public/Ship/SFAutomationOperatorComponent.h Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp Source/StarForge/Public/Ship/SFPursuitOperatorComponent.h Source/StarForge/Private/Ship/SFPursuitOperatorComponent.cpp Source/StarForge/Public/Ship/SFConsoleOperatorComponent.h Source/StarForge/Public/Ship/SFPilotStationComponent.h Source/StarForge/Public/Ship/SFWeaponComponent.h Source/StarForge/Public/Ship/SFDockingComponent.h Source/StarForge/Private/Ship/SFDockingComponent.cpp Source/StarForge/Public/Ship/SFThrusterArrayComponent.h docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
feat(ship): a parked hull, crewed by nobody and locked for life

Step 2 of the station kit occupies a port with a real docked hull, and that
hull needs a third crewing value. The automation and the pursuit operator read
"does not answer sf.*" as "automated" and sit down; with a parked hull that
reading is wrong, so who may take a seat becomes its own question, told to each
operator by the ship before its components begin play.

A parked hull answers no sf.* except the three named in the enum. Its array stays
locked through LetGo, which says so in the undocked line, so a hull let go from
its port hangs there at rest rather than waiting for an operator to arrive.

The player's ship and the pirate seat exactly as before: the evidence replay of
the combat and loop rigs is unchanged.
MSG
```

---

### Task 17: `ParkedHulls`, `DockImmediatelyAt`, строка `Place parked hull:` и `sf.UndockParked`

Место просит припаркованный корпус у порта N (§7.6). Проверка записи, отложенное появление с экипажем `Parked`, стыковка без доводки к названному порту — до корабля игрока, чтобы правило видело занятость. Строка о корпусе — после `Super::StartPlay`, потому что раньше читать нечего: флаги раздаются в `BeginPlay`, и операторы решают там же.

**Files:**
- Modify: `Source/StarForge/Public/World/SFPlaceDataAsset.h` (структура после `FSFShipStart`; поле после `ShipStart`)
- Modify: `Source/StarForge/Public/SFPlaceGameMode.h` (forward-declaration; приватные методы и поля)
- Modify: `Source/StarForge/Private/SFPlaceGameMode.cpp` (include; `InitGame`; `StartPlay`; новые методы)
- Modify: `Source/StarForge/Public/Ship/SFDockingComponent.h`, `Source/StarForge/Private/Ship/SFDockingComponent.cpp` (`DockImmediatelyAt`)
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp` (include; команда в конце файла)
- Create: `Content/Places/DA_Place_PierCoreParked0.uasset`
- Create: `Content/Places/DA_Place_PierCoreBadRecords.uasset` (контроль проверки записи, шаг 7)

**Interfaces:**
- Потребляет из задачи 16: `ESFShipCrewing::Parked`, все `IsConsoleControlled()`, `GetConsoleOperator()`, `StateName`.
- Отдаёт задачам 18, 19:
  - `USTRUCT(BlueprintType) struct FSFParkedHull { FName HullName; FName StationCarrierName; int32 PortIndex; };`, `USFPlaceDataAsset::ParkedHulls`
  - `bool USFDockingComponent::DockImmediatelyAt(USFDockingPortComponent* Target)`
  - строки `Place parked hull: … -- NOT SPAWNED.` (`Error`) и `Place parked hull: 'Parked' (crewing parked) docked at Station.Port0 -- pilot's and gunner's posts unoccupied, thrusters locked, not simulating, deaf to sf.*.` (`Display`)
  - хвост строки `Place rig:` `; 1 parked hull`
  - команда `sf.UndockParked <Name> [DelaySeconds]`

- [ ] **Step 1: «Красный» шаг — поля нет, ассет не создаётся**

```bash
KIT_PLACES_GROUP=parked0 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "SFParkedHull\|parked_hulls\|Python script executed" $L   # AttributeError ...; executed with errors
```

- [ ] **Step 2: Запись в данных места**

`Source/StarForge/Public/World/SFPlaceDataAsset.h`, после `FSFShipStart`:

```cpp
/**
 * A hull a place asks to stand docked at a named port (station-kit spec 7.6). Its own array and not a
 * new field of FSFPlaceCarrier: a carrier is a class and a pose and nothing else, station-as-data is
 * deferred (owner's decision 12), and a docked hull's pose is not written down -- it is derived from
 * the two ports, as the rig derives the player's ship's.
 */
USTRUCT(BlueprintType)
struct STARFORGE_API FSFParkedHull
{
	GENERATED_BODY()

	/** Not empty, not "Ship", no carrier's and no earlier record's: NameMode Requested replaces a taken name in silence. */
	UPROPERTY(EditAnywhere, Category = "Parked hull")
	FName HullName;

	/** The CarrierName of a docking station in this place. */
	UPROPERTY(EditAnywhere, Category = "Parked hull")
	FName StationCarrierName;

	/** In GetDockingPorts order: 0 is Port0. */
	UPROPERTY(EditAnywhere, Category = "Parked hull")
	int32 PortIndex = 0;
};
```

В `USFPlaceDataAsset`, после `ShipStart`:

```cpp
	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	TArray<FSFParkedHull> ParkedHulls;
```

- [ ] **Step 3: `DockImmediatelyAt`**

`Source/StarForge/Public/Ship/SFDockingComponent.h`, после `DockImmediately()`:

```cpp
	/**
	 * DockImmediately to a port named by data rather than chosen by the rule: the place rig docks a
	 * parked hull to the port its record names (spec 7.4, 7.6). The same EnterCapture and
	 * CompleteCapture, so the port is held the same way. False, with an Error, if the port is held.
	 */
	bool DockImmediatelyAt(USFDockingPortComponent* Target);
```

`SFDockingComponent.cpp`, после `DockImmediately`:

```cpp
bool USFDockingComponent::DockImmediatelyAt(USFDockingPortComponent* Target)
{
	ResolvePorts();

	if (!Port || !Target)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Dock: [%s] cannot spawn docked at a named port -- %s. The ship will start adrift."),
			*GetNameSafe(GetOwner()), !Port ? TEXT("no port on this ship") : TEXT("no such port"));
		return false;
	}
	if (Target->IsHeld())
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Dock: [%s] cannot spawn docked at %s -- %s already holds it."),
			*GetNameSafe(GetOwner()), *DockingKit::PortLabel(Target), *GetNameSafe(Target->GetHolder()->GetOwner()));
		return false;
	}

	StationPort = Target;
	if (!EnterCapture())
	{
		return false;
	}
	CompleteCapture();
	return true;
}
```

- [ ] **Step 4: Стенд места**

`Source/StarForge/Public/SFPlaceGameMode.h`: `class USFDockingPortComponent;` рядом с `class ASFTestShip;`; в `private:`:

```cpp
	/** Checks one ParkedHulls record, spawns its hull deferred with crewing Parked and docks it to the named port. */
	void SpawnParkedHull(int32 Record, const TMap<FName, AActor*>& CarriersByName, const FVector& PlaceOriginCm);

	/** Its Place parked hull: line, read back from the hull's components after their BeginPlay. */
	void ReportParkedHull(int32 Index) const;

	UPROPERTY()
	TArray<FSFParkedHull> PlaceParkedHulls;

	/** Parallel arrays, one entry per hull that spawned. */
	UPROPERTY()
	TArray<TObjectPtr<ASFTestShip>> ParkedHulls;

	UPROPERTY()
	TArray<TObjectPtr<USFDockingPortComponent>> ParkedPorts;

	TArray<FName> ParkedRequestedNames;
	TArray<FString> ParkedPortLabels;
```

`Source/StarForge/Private/SFPlaceGameMode.cpp`: include `"Components/BoxComponent.h"`, `"Ship/SFAutomationOperatorComponent.h"`, `"Ship/SFConsoleOperatorComponent.h"`, `"Ship/SFDockingPortComponent.h"`, `"Ship/SFDockingStation.h"`, `"Ship/SFGunnerStationComponent.h"`, `"Ship/SFPilotStationComponent.h"`, `"Ship/SFPursuitOperatorComponent.h"`, `"Ship/SFThrusterArrayComponent.h"`, `"Ship/SFWeaponComponent.h"`.

В `InitGame`: к сбросу (`PlaceShipStart = FSFShipStart();`) добавить `PlaceParkedHulls.Reset();`; в ветку `if (bPlaceFromAsset)` — `PlaceParkedHulls = PlaceData->ParkedHulls;`.

В `StartPlay`: перед циклом носителей (`if (bPlaceFits) { for (const FSFPlaceCarrier& Carrier : PlaceCarriers)`) объявить `TMap<FName, AActor*> CarriersByName;`, а в цикле после `SpawnedCarriers.Add(Spawned);` — `CarriersByName.Add(Carrier.CarrierName, Spawned);`. После объявления `bStartsFree` (задача 2) и перед `bool bDocked = false;`:

```cpp
	// Parked hulls before the player's ship (spec 7.6): the rule that docks the player's ship sees the
	// ports these hold. Each is docked with no run-in, so there is no physics step between its spawn at
	// the place's origin -- inside the hall -- and its port, for the reason spec 6.2 gives for the ship.
	if (bPlaceFits)
	{
		for (int32 Record = 0; Record < PlaceParkedHulls.Num(); ++Record)
		{
			SpawnParkedHull(Record, CarriersByName, PlaceOriginCm);
		}
	}
```

После ветки отказа `if (!bPlaceFits) { … return; }` и перед блоком `bool bPawnInCabin = false;`:

```cpp
	// After Super::StartPlay and not before: the hulls begin play inside it, and until then every
	// component still reads its default bConsoleControlled = true and no operator has decided whether
	// to sit. A line written earlier would read "deaf" as "answers" and "unoccupied" on a build where
	// an operator sits a moment later (spec 7.6).
	for (int32 Index = 0; Index < ParkedHulls.Num(); ++Index)
	{
		ReportParkedHull(Index);
	}
```

После вычисления `CarriersText` (перед `const FString ManText`):

```cpp
	// Only when the place asks for parked hulls, so an asset without them reads as before, to the character.
	bool bParkedShort = false;
	if (!PlaceParkedHulls.IsEmpty())
	{
		const int32 Asked = PlaceParkedHulls.Num();
		const int32 Standing = ParkedHulls.Num();
		bParkedShort = (Standing != Asked);
		CarriersText += bParkedShort
			? FString::Printf(TEXT("; %d of %d parked hulls, %d NOT SPAWNED"), Standing, Asked, Asked - Standing)
			: FString::Printf(TEXT("; %d parked hull%s"), Standing, Standing == 1 ? TEXT("") : TEXT("s"));
	}
```

и в условии красной строки добавить `|| bParkedShort`. Новые методы, в конец файла:

```cpp
void ASFPlaceGameMode::SpawnParkedHull(int32 Record, const TMap<FName, AActor*>& CarriersByName, const FVector& PlaceOriginCm)
{
	const FSFParkedHull& Parked = PlaceParkedHulls[Record];
	const FString HullName = Parked.HullName.ToString();
	const FString StationName = Parked.StationCarrierName.ToString();

	// A record that fails its check spawns nothing: its hull would hang at the place's origin, which on
	// this rig is inside the hall. Said at the moment of the decision, which is here.
	const auto Refuse = [](const FString& Why)
	{
		UE_LOG(LogStarForge, Error, TEXT("Place parked hull: %s -- NOT SPAWNED."), *Why);
	};

	// The name first. Hulls spawn with NameMode Requested, and a taken name is replaced without a word:
	// a record named Ship would take the player's ship's name, and every gate that names [Ship] would
	// break with no finding of its own.
	if (Parked.HullName.IsNone())
	{
		Refuse(FString::Printf(TEXT("record %d has no hull name"), Record));
		return;
	}
	if (Parked.HullName == FName(TEXT("Ship")))
	{
		Refuse(FString::Printf(TEXT("record %d asks for the name 'Ship', which is the player's ship's"), Record));
		return;
	}
	if (PlaceCarriers.ContainsByPredicate([&Parked](const FSFPlaceCarrier& Carrier) { return Carrier.CarrierName == Parked.HullName; }))
	{
		Refuse(FString::Printf(TEXT("record %d asks for the name '%s', which is a carrier's"), Record, *HullName));
		return;
	}
	for (int32 Earlier = 0; Earlier < Record; ++Earlier)
	{
		if (PlaceParkedHulls[Earlier].HullName == Parked.HullName)
		{
			Refuse(FString::Printf(TEXT("record %d asks for the name '%s', which record %d already took"), Record, *HullName, Earlier));
			return;
		}
	}

	AActor* const* Found = CarriersByName.Find(Parked.StationCarrierName);
	if (!Found || !*Found)
	{
		Refuse(FString::Printf(TEXT("'%s' asks for carrier '%s', which is not in this place"), *HullName, *StationName));
		return;
	}
	ISFDockingStation* Station = Cast<ISFDockingStation>(*Found);
	if (!Station)
	{
		Refuse(FString::Printf(TEXT("'%s' asks for carrier '%s', which is not a docking station"), *HullName, *StationName));
		return;
	}

	TArray<USFDockingPortComponent*> Ports;
	Station->GetDockingPorts(Ports);
	if (!Ports.IsValidIndex(Parked.PortIndex))
	{
		Refuse(FString::Printf(TEXT("'%s' asks for port %d of %s, which has %d"), *HullName, Parked.PortIndex, *StationName, Ports.Num()));
		return;
	}
	USFDockingPortComponent* Port = Ports[Parked.PortIndex];
	const FString PortLabel = FString::Printf(TEXT("%s.%s"), *(*Found)->GetName(), *Port->GetName());
	if (Port->IsHeld())
	{
		Refuse(FString::Printf(TEXT("'%s' asks for %s, which %s already holds"),
			*HullName, *PortLabel, *GetNameSafe(Port->GetHolder()->GetOwner())));
		return;
	}

	// Deferred, as the combat rig spawns its pirate (SFCombatGameMode.cpp 49-74): crewing, drone and
	// trigger are on the hull before anything reads them (SFTestShip.h 101-107).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.Name = Parked.HullName;
	Params.bDeferConstruction = true;

	const FTransform At(PlaceOriginCm);
	ASFTestShip* Hull = GetWorld()->SpawnActor<ASFTestShip>(ASFTestShip::StaticClass(), At, Params);
	if (!Hull)
	{
		Refuse(FString::Printf(TEXT("'%s' did not spawn"), *HullName));
		return;
	}
	Hull->Crewing = ESFShipCrewing::Parked;
	Hull->bSpawnTargetDrone = false;
	Hull->bWeaponsFree = false;
	Hull->FinishSpawning(At);

	// DockImmediatelyAt says what went wrong itself; a hull left undocked is still read back, and its
	// line says NOT DOCKED.
	if (Hull->GetDocking())
	{
		Hull->GetDocking()->DockImmediatelyAt(Port);
	}

	ParkedHulls.Add(Hull);
	ParkedPorts.Add(Port);
	ParkedRequestedNames.Add(Parked.HullName);
	ParkedPortLabels.Add(PortLabel);
}

void ASFPlaceGameMode::ReportParkedHull(int32 Index) const
{
	const ASFTestShip* Hull = ParkedHulls[Index];
	const FString Asked = ParkedRequestedNames[Index].ToString();
	if (!Hull)
	{
		UE_LOG(LogStarForge, Error, TEXT("Place parked hull: '%s' is GONE before it could be read back."), *Asked);
		return;
	}

	// Every state read from a component, never from what was asked: an assertion of the request is not
	// a reading, and a mutant that seats the automation on a parked hull must show here.
	bool bRed = false;

	FString NameText = FString::Printf(TEXT("'%s'"), *Hull->GetName());
	if (Hull->GetFName() != ParkedRequestedNames[Index])
	{
		bRed = true;
		NameText += FString::Printf(TEXT(" (asked for as '%s', a DIFFERENT NAME)"), *Asked);
	}

	const bool bParked = (Hull->GetCrewing() == ESFShipCrewing::Parked);
	bRed |= !bParked;

	const USFDockingComponent* Docking = Hull->GetDocking();
	const USFDockingPortComponent* Port = ParkedPorts[Index];
	const bool bDocked = Docking && Docking->IsDocked() && Port && Port->GetHolder() == Docking;
	bRed |= !bDocked;
	const FString DockText = bDocked
		? FString::Printf(TEXT("docked at %s"), *ParkedPortLabels[Index])
		: FString::Printf(TEXT("NOT DOCKED at %s (it is %s)"), *ParkedPortLabels[Index],
			Docking ? USFDockingComponent::StateName(Docking->GetState()) : TEXT("without a docking component"));

	TArray<FString> Held;
	if (const USFPilotStationComponent* Pilot = Hull->GetPilotStation(); Pilot && Pilot->IsOccupied())
	{
		Held.Add(FString::Printf(TEXT("the pilot's post HELD by %s"), *Pilot->GetOperatorDisplayName()));
	}
	if (const USFGunnerStationComponent* Gunner = Hull->GetGunnerStation(); Gunner && Gunner->IsOccupied())
	{
		Held.Add(FString::Printf(TEXT("the gunner's post HELD by %s"), *Gunner->GetOperatorDisplayName()));
	}
	bRed |= !Held.IsEmpty();
	const FString PostsText = Held.IsEmpty()
		? FString(TEXT("pilot's and gunner's posts unoccupied")) : FString::Join(Held, TEXT(", "));

	const USFThrusterArrayComponent* Thrusters = Hull->GetThrusters();
	const bool bLocked = Thrusters && Thrusters->IsLocked();
	bRed |= !bLocked;

	const bool bSimulating = Hull->GetHull() && Hull->GetHull()->IsSimulatingPhysics();
	bRed |= bSimulating;

	// Deaf means every component ASFTestShip::BeginPlay hands the flag to reads false.
	const bool bAnswers =
		(Hull->GetPilotStation() && Hull->GetPilotStation()->IsConsoleControlled())
		|| (Thrusters && Thrusters->IsConsoleControlled())
		|| (Hull->GetConsoleOperator() && Hull->GetConsoleOperator()->IsConsoleControlled())
		|| (Hull->GetAutomationOperator() && Hull->GetAutomationOperator()->IsConsoleControlled())
		|| (Hull->GetPursuitOperator() && Hull->GetPursuitOperator()->IsConsoleControlled())
		|| (Docking && Docking->IsConsoleControlled())
		|| (Hull->GetWeapon() && Hull->GetWeapon()->IsConsoleControlled());
	bRed |= bAnswers;

	const FString Line = FString::Printf(TEXT("Place parked hull: %s (crewing %s) %s -- %s, %s, %s, %s."),
		*NameText, bParked ? TEXT("parked") : TEXT("NOT PARKED"), *DockText, *PostsText,
		bLocked ? TEXT("thrusters locked") : TEXT("thrusters UNLOCKED"),
		bSimulating ? TEXT("SIMULATING") : TEXT("not simulating"),
		bAnswers ? TEXT("ANSWERS sf.*") : TEXT("deaf to sf.*"));

	if (bRed)
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Line);
	}
	else
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *Line);
	}
}
```

- [ ] **Step 5: `sf.UndockParked`**

`Source/StarForge/Private/Ship/SFTestShip.cpp`: include `"EngineUtils.h"`, `"NumberText.h"`, `"TimerManager.h"` (не `"Misc/DefaultValueHelper.h"` — поправка П3: задержку читает `StarForge::Parse::TryParseFiniteNumber`, как у `sf.KillThruster`). В конец файла:

```cpp
namespace
{
namespace ParkedHullCommand
{
	const TCHAR* CrewingWord(ESFShipCrewing InCrewing)
	{
		switch (InCrewing)
		{
		case ESFShipCrewing::PlayerShip: return TEXT("the player's ship");
		case ESFShipCrewing::Automated: return TEXT("automated");
		case ESFShipCrewing::Parked: return TEXT("parked");
		}
		return TEXT("unknown");
	}

	void UndockParkedNow(UWorld* World, const FString& Name)
	{
		for (TActorIterator<ASFTestShip> It(World); It; ++It)
		{
			if (It->GetName() != Name)
			{
				continue;
			}
			if (It->GetCrewing() != ESFShipCrewing::Parked)
			{
				UE_LOG(LogStarForge, Warning,
					TEXT("sf.UndockParked '%s': it is not parked (its crewing is %s); the player's ship is undocked with sf.Dock. Nothing was undocked."),
					*Name, CrewingWord(It->GetCrewing()));
				return;
			}
			USFDockingComponent* Docking = It->GetDocking();
			if (!Docking || !Docking->IsDocked())
			{
				UE_LOG(LogStarForge, Warning, TEXT("sf.UndockParked '%s': it is not docked (it is %s); nothing was undocked."),
					*Name, Docking ? USFDockingComponent::StateName(Docking->GetState()) : TEXT("without a docking component"));
				return;
			}
			// The same RequestUndock as F, so sf.DockRelease acts here too, and control N8 frees the port
			// by the very LetGo the player's ship frees its own with.
			Docking->RequestUndock();
			return;
		}
		UE_LOG(LogStarForge, Warning, TEXT("sf.UndockParked '%s': no ship of that name in this world; nothing was undocked."), *Name);
	}

	void UndockParkedFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		// Read strictly, as sf.KillThruster's delay is: FDefaultValueHelper::ParseDouble let ".", "-"
		// and "e10" through as zero, and a delay of zero undocks on the first frame -- the one frame the
		// delay exists to keep the undock off (NumberText.h).
		double AfterSeconds = 0.0;
		const bool bParsed = (Args.Num() == 1 || Args.Num() == 2)
			&& (Args.Num() == 1
				|| (StarForge::Parse::TryParseFiniteNumber(Args[1], AfterSeconds) && AfterSeconds >= 0.0));
		if (!bParsed)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.UndockParked needs a hull's name and optionally a non-negative delay in seconds; nothing was undocked."));
			return;
		}

		const FString Name = Args[0];
		if (AfterSeconds == 0.0 || !World)
		{
			UndockParkedNow(World, Name);
			return;
		}

		// Game seconds, for sf.KillThruster's reason: -ExecCmds runs its whole list on the first frame.
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), Name]()
			{
				if (UWorld* LiveWorld = WeakWorld.Get())
				{
					UndockParkedNow(LiveWorld, Name);
				}
			}),
			static_cast<float>(AfterSeconds), false);

		UE_LOG(LogStarForge, Display, TEXT("sf.UndockParked '%s': scheduled %.2f s of game time from now."), *Name, AfterSeconds);
	}

	/**
	 * Undocks a parked hull by name (station-kit spec 7.6), the one sf.* a parked hull answers besides
	 * sf.Shields. A command and not a variable, like sf.KillThruster, because it names its hull and
	 * takes a delay.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdUndockParked(
		TEXT("sf.UndockParked"),
		TEXT("sf.UndockParked <Name> [Seconds]: undock the parked hull of that name through the same RequestUndock as F,\n")
		TEXT("now or after that many seconds of game time. Refused out loud for a hull that is not parked or not docked."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UndockParkedFromConsole));
}
}
```

- [ ] **Step 6: Собрать, ассет, N3 и N8 на 60**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
KIT_PLACES_GROUP=parked0 "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET read /Game/Places/DA_Place_PierCoreParked0:\|Python script executed" $L   # parked ['Parked' at Station.0]

kit DA_Place_PierCoreParked0 60 40 "sf.FlightLog 1" task17-n3-60
grep -cF "Place parked hull: 'Parked' (crewing parked) docked at Station.Port0 -- pilot's and gunner's posts unoccupied, thrusters locked, not simulating, deaf to sf.*." $L   # 1
grep -cF "Dock: [Ship] nearest free port is now Station.Port1 at 63.94 m; next free none; held Station.Port0 by Parked at 22.10 m." $L   # 1
grep -o "Place rig: scripted.*" $L   # ... assembled with 1 carrier; 1 parked hull; the ship spawns docked; ...
grep "Turret: \[Parked\]" $L | grep -vc "gunned by unoccupied |"   # 0 (и строк больше нуля)
grep "Flight: \[Parked\]" $L | grep -vc "flown by unoccupied |"    # 0 (и строк больше нуля)

kit DA_Place_PierCoreParked0 60 25 "sf.FlightLog 1, sf.Dock 0, sf.UndockParked Parked 2.0" task17-n8-60
grep -cF "Dock: [Ship] nearest free port is now Station.Port1 at 2.93 m; next free none; held Station.Port0 by Parked at 60.07 m." $L   # 1
grep -cF "Dock: [Parked] undocked; its thrusters stay locked: it is parked, and nobody crews it." $L   # 1
grep -cF "Dock: [Ship] nearest free port is now Station.Port1 at 2.93 m; next free Station.Port0 at 60.07 m." $L   # 1

kit DA_Place_PierCoreParked0 60 10 "sf.FlightLog 1, sf.UndockParked Parked ., sf.UndockParked Parked -" task17-junk-60
grep -cF "sf.UndockParked needs a hull's name and optionally a non-negative delay in seconds; nothing was undocked." $L   # 2
grep -cF "Dock: [Parked] releasing" $L   # 0: ни одна опечатка в задержке не стала нулём
rm docs/superpowers/notes/evidence/$(date +%F)-task17-*.log
```

Плюс общие ворота; у `task17-junk` — ровно два `Warning`, названные выше (поправка П3: `.` и `-` прежде проходили через `FDefaultValueHelper::ParseDouble` нулём и отстыковывали на первом кадре).

- [ ] **Step 7: Проверка записи — прогоном, а не доверием коду**

Ассеты набора корректны, и ни один прогон §8.2–§8.3 не даёт плохой записи. Без этого шага ветки проверки в `SpawnParkedHull` и красный хвост `K NOT SPAWNED` строки `Place rig:` не выполнил бы ни один прогон. Контрольный ассет — группа `badrecords` сценария (задача 3). В нём одна годная запись, `Parked0` у `Port0`, и семь плохих, по одной на каждую проверку, до которой доходит прогон, в том порядке, в каком их делает стенд. Две проверки остаются недостижимыми, и почему — сказано в «Решениях сверх спеки».

```bash
KIT_PLACES_GROUP=badrecords "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET read /Game/Places/DA_Place_PierCoreBadRecords:\|Python script executed" $L   # восемь записей в parked [...]; executed successfully

cat > /tmp/kit-badrecords-expected.txt <<'TXT'
Place parked hull: record 1 has no hull name -- NOT SPAWNED.
Place parked hull: record 2 asks for the name 'Ship', which is the player's ship's -- NOT SPAWNED.
Place parked hull: record 3 asks for the name 'Station', which is a carrier's -- NOT SPAWNED.
Place parked hull: record 4 asks for the name 'Parked0', which record 0 already took -- NOT SPAWNED.
Place parked hull: 'X' asks for carrier 'Nope', which is not in this place -- NOT SPAWNED.
Place parked hull: 'Y' asks for port 5 of Station, which has 2 -- NOT SPAWNED.
Place parked hull: 'Parked1' asks for Station.Port0, which Parked0 already holds -- NOT SPAWNED.
TXT
for fps in 60 400; do
  kit DA_Place_PierCoreBadRecords $fps 10 "sf.FlightLog 1" task17-badrecords-$fps
  grep -o "Place parked hull: .* -- NOT SPAWNED\." $L | tr -d '\r' | diff - /tmp/kit-badrecords-expected.txt && echo "$fps: seven refusals, in record order"
  grep -cF "Place parked hull: 'Parked0' (crewing parked) docked at Station.Port0 -- pilot's and gunner's posts unoccupied, thrusters locked, not simulating, deaf to sf.*." $L   # 1: годная запись встала
  grep -c "Place parked hull: '[^']*' (crewing" $L                                     # 1: и только она
  grep "LogStarForge: Error: Place rig:" $L | grep -c "1 of 8 parked hulls, 7 NOT SPAWNED"   # 1
  grep -cF "Dock: [Ship] nearest free port is now Station.Port1 at 63.94 m; next free none; held Station.Port0 by Parked0 at 22.10 m." $L   # 1
  grep -c "LogStarForge: Error" $L                                                     # 8: семь отказов и строка стенда
  grep -c "LogStarForge: Warning" $L                                                   # 0
  grep -c "LogStarForge: Display: World bounds:" $L                                    # 1
done
rm docs/superpowers/notes/evidence/$(date +%F)-task17-*.log
```

Ворота — строка `seven refusals, in record order` на обеих частотах и числа в комментариях. Журналы — проверка задачи, как все `task*`, в улики они не идут. Запись без проверки, которая всё же встала бы (кроме `Parked0`), ломает счёт `(crewing` и даёт вторую строку сборки корпуса. Это находка, а не протокол.

- [ ] **Step 8: Коммит**

```bash
git status --short
git add Source/StarForge/Public/World/SFPlaceDataAsset.h Source/StarForge/Public/SFPlaceGameMode.h Source/StarForge/Private/SFPlaceGameMode.cpp Source/StarForge/Public/Ship/SFDockingComponent.h Source/StarForge/Private/Ship/SFDockingComponent.cpp Source/StarForge/Private/Ship/SFTestShip.cpp Content/Places/DA_Place_PierCoreParked0.uasset Content/Places/DA_Place_PierCoreBadRecords.uasset
git diff --cached --name-only
git commit -F - <<'MSG'
feat(place): a place can park hulls at named ports, and read each one back

The port of step 2 is occupied by a real docked hull, not by a switch. A place
asks for it by name, station and port index; the rig checks the record -- the
name above all, because a taken name is replaced in silence and a record called
Ship would steal the player's ship's -- spawns it deferred with crewing Parked,
and docks it to that port before the player's ship, so the rule sees the port
held.

Its line is written after Super::StartPlay, because before it every component
still reads the default that it answers the console and no operator has decided
whether to sit. sf.UndockParked lets it go through the same RequestUndock as F.

The record check is run, not trusted: DA_Place_PierCoreBadRecords holds one
sound record and seven that each fail one check, and a run at 60 and 400 fps
must refuse all seven in order and turn the rig line red. No place of the kit
has a bad record, so without that asset no run would ever reach the check.
MSG
```

---

### Task 18: Отказ «свободных нет», строка полёта, панель и имена портов

Когда все порты заняты, сегодня строка полёта печатает неизмеренный подход — вердикт против числа внутри его же предела (`SFTestShip.cpp:713-733`, `SFDockingComponent.cpp:175-180`), а отказ говорит «could not be measured: no station port». Здесь у «свободных нет» свои строки (§7.7), а на станции с несколькими портами строки стыковки называют порт.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFDockingComponent.h` (приватный `LogPortInUse`)
- Modify: `Source/StarForge/Private/Ship/SFDockingComponent.cpp` (`RequestDock`, `LogRefusal`, `CompleteCapture`, `RequestUndock`, `LetGo`, `DockImmediately`)
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp` (`LogFlightState`, строки 713–734)
- Modify: `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp` (после строки 333)
- Create: `Content/Places/DA_Place_PierCoreParkedBoth.uasset`

**Interfaces:**
- Потребляет из задачи 15: `HasNoFreePort`, `DescribeHeldPorts`, `CandidateCount`, `DockingKit::PortLabel`.
- Отдаёт задаче 19: строки `Dock: [X] refused -- no free port: …` (`Warning`), `Dock: [X] free -- NO FREE PORT: …` (`Log`), `Dock: [X] the port is Station.PortN, one of M.` (`Display`), префикс `Station.PortN: ` у отказа по окну; строка панели `dock    free -- NO FREE PORT`.

- [ ] **Step 1: «Красный» шаг**

```bash
KIT_PLACES_GROUP=parkedboth "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge-station/StarForge.uproject" -run=pythonscript -script="F:/UEProject/StarForge-station/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log < /dev/null
grep "KIT-ASSET read /Game/Places/DA_Place_PierCoreParkedBoth:\|Python script executed" $L
kit DA_Place_PierCoreParkedBoth 60 10 "sf.FlightLog 1, sf.Dock 1" task18-red-60
grep -c "refused -- no free port" $L                                   # 0
grep -c "refused -- the approach could not be measured: no station port." $L   # 1 -- причина не та
grep -c "free | port 0.0 m" $L                                          # >= 1 -- вердикт против неизмеренного
rm docs/superpowers/notes/evidence/$(date +%F)-task18-red-60.log
```

- [ ] **Step 2: Отказ и строка порта**

`Source/StarForge/Public/Ship/SFDockingComponent.h`, приватные: 

```cpp
	/** "Dock: [X] the port is Station.Port1, one of 2." after a state line, only with two or more candidates (spec 7.7). */
	void LogPortInUse() const;
```

`Source/StarForge/Private/Ship/SFDockingComponent.cpp`, в `RequestDock` после блока `if (!bHasStation) { … }`:

```cpp
	if (bNoFreePort)
	{
		// Its own refusal, in the tone of the others (spec 7.7). Without it the refusal said the approach
		// could not be measured -- true, and the wrong reason: nothing is wrong with the instrument.
		UE_LOG(LogStarForge, Warning, TEXT("Dock: [%s] refused -- no free port: %s."),
			*GetNameSafe(GetOwner()), *DescribeHeldPorts(true));
		return false;
	}
```

В `LogRefusal`, последнюю строку (`UE_LOG(… TEXT("Dock: [%s] refused -- %s"), …)`) заменить на:

```cpp
	// On a station with several ports the refusal names the one it was judged against; with one port
	// the old text stays letter for letter (spec 7.7).
	const FString PortPrefix = (CandidateCount >= 2 && StationPort)
		? DockingKit::PortLabel(StationPort) + TEXT(": ") : FString();
	UE_LOG(LogStarForge, Warning, TEXT("Dock: [%s] refused -- %s%s"), *GetNameSafe(GetOwner()), *PortPrefix, *Detail);
```

Строку `LogPortInUse();` добавить: в `RequestDock` сразу после строки `capturing, …`; в `CompleteCapture` сразу после строки `docked.`; в `RequestUndock` сразу после строки `releasing, …`; в `LetGo` сразу после строки `undocked` (обеих её форм), перед `NotifyHolder(nullptr)`. Метод:

```cpp
void USFDockingComponent::LogPortInUse() const
{
	// A separate line, not a word in the state line: the state lines are quoted letter for letter by the
	// gates of H2.2 and N8 on a two-port station, and on a one-port station nothing may change at all.
	if (CandidateCount >= 2 && StationPort)
	{
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] the port is %s, one of %d."),
			*GetNameSafe(GetOwner()), *DockingKit::PortLabel(StationPort), CandidateCount);
	}
}
```

В `DockImmediately` текст ошибки `Port ? TEXT("no station port") : TEXT("no port on this ship")` заменить на:

```cpp
			!Port ? TEXT("no port on this ship") : bNoFreePort ? TEXT("every port is held") : TEXT("no station port")
```

- [ ] **Step 3: Строка полёта**

`Source/StarForge/Private/Ship/SFTestShip.cpp`, в `LogFlightState`, ветку `default:` (строки 713–734) — после `if (!Docking->HasStation()) { DockText = TEXT("free -- no station"); }` вставить:

```cpp
			else if (Docking->HasNoFreePort())
			{
				// Not the unmeasured approach: "port 0.0 m ... out of window: distance" is a verdict
				// against a figure inside its own limit, which is what this line used to print here.
				DockText = FString::Printf(TEXT("free -- NO FREE PORT: %s"), *Docking->DescribeHeldPorts(false));
			}
```

- [ ] **Step 4: Панель**

`Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`, после ветки `else if (!Docking->HasStation()) { … }` (строки 330–333):

```cpp
	else if (Docking->IsFree() && Docking->HasNoFreePort())
	{
		// A gate for the hand run only: headless nobody reads the panel (spec 7.7).
		DrawLine(TEXT("dock    free -- NO FREE PORT"), Y, FLinearColor::Red);
	}
```

- [ ] **Step 5: Собрать и N4 на 60**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge-station/StarForge.uproject" -WaitMutex
kit DA_Place_PierCoreParkedBoth 60 25 "sf.FlightLog 1, sf.Dock 1" task18-n4-60
grep -cF "the ship starts free at place-local (0.0 -40.0 0.0) m, as the place asks" $L   # 1
grep -cF "Dock: [Ship] nearest free port is now none: 2 of 2 held (Station.Port0 by Parked0 at 17.90 m, Station.Port1 by Parked1 at 62.61 m)." $L   # 1
grep -cF "Dock: [Ship] refused -- no free port: 2 of 2 held (Station.Port0 by Parked0 at 17.90 m, Station.Port1 by Parked1 at 62.61 m)." $L   # 1
grep -cF "Dock: [Ship] free -- NO FREE PORT: 2 of 2 held (Station.Port0 by Parked0, Station.Port1 by Parked1)" $L   # >= 20
grep -c "free | port 0.0 m" $L                           # 0
kit DA_Place_PierCore 60 40 "sf.FlightLog 1, sf.Dock 2" task18-h22-60
awk '/Dock: \[Ship\] docked\./{d++} d==2 && /the port is Station.Port0, one of 2/{print "second dock at Port0"; exit}' $L
rm docs/superpowers/notes/evidence/$(date +%F)-task18-*.log
```

Плюс общие ворота; у N4 законна одна `Warning` — отказ. Регрессии у задачи нет по §9, но она проходит через `LogRefusal` и строку полёта, на которых стоят старые стенды: регрессия `kit-2.7` задачи 19 это и проверит.

- [ ] **Step 6: Коммит**

```bash
git status --short
git add Source/StarForge/Public/Ship/SFDockingComponent.h Source/StarForge/Private/Ship/SFDockingComponent.cpp Source/StarForge/Private/Ship/SFTestShip.cpp Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp Content/Places/DA_Place_PierCoreParkedBoth.uasset
git diff --cached --name-only
git commit -F - <<'MSG'
feat(docking): say "no free port" in words, and name the port on a station with several

With every port held the dock refusal said the approach could not be measured,
and the flight line printed an unmeasured approach -- a verdict of "out of
window: distance" against a figure of 0.0 m, inside its own limit. Both now say
that there is no free port and who holds each one; the panel says it in red.

On a station with more than one port the window refusal names the port it was
judged against, and each state change gets a separate line naming the port. A
separate line, because the state lines are quoted letter for letter by the gates
of the two-port runs, and on a one-port station nothing changes at all.
MSG
```

---

### Task 19: Прогоны шага 2 на обеих частотах и регрессия `kit-2.7`

**Files:**
- Create: `docs/superpowers/notes/evidence/<дата>-kit-{h2-1,h2-2,n3,n4,n6,n8}-{60,400}.log` и `<дата>-kit-h2-2-var-{a,b}.log` (переменный шаг, поправка П9)
- Modify: `docs/superpowers/notes/<дата>-station-kit-baseline.md`

**Interfaces:**
- Потребляет: всё шага 2.
- Отдаёт задачам 20, 21: журналы с дословными строками ворот точки 12.

- [ ] **Step 1: Двенадцать прогонов**

| Прогон | Команда |
|---|---|
| H2.1 | `kit DA_Place_PierCore <fps> 70 "sf.FlightLog 1" kit-h2-1-<fps>` |
| H2.2 | `kit DA_Place_PierCore <fps> 40 "sf.FlightLog 1, sf.Dock 2" kit-h2-2-<fps>` |
| N3 | `kit DA_Place_PierCoreParked0 <fps> 40 "sf.FlightLog 1" kit-n3-<fps>` |
| N4 | `kit DA_Place_PierCoreParkedBoth <fps> 25 "sf.FlightLog 1, sf.Dock 1" kit-n4-<fps>` |
| N6 | `kit DA_Place_PierCoreTie <fps> 25 "sf.FlightLog 1" kit-n6-<fps>` |
| N8 | `kit DA_Place_PierCoreParked0 <fps> 25 "sf.FlightLog 1, sf.Dock 0, sf.UndockParked Parked 2.0" kit-n8-<fps>` |

Плюс H2.2 дважды с переменным шагом (поправка П9; `kitvar` — из задачи 12, объявить заново в новой оболочке): `kitvar DA_Place_PierCore 40 "sf.FlightLog 1, sf.Dock 2" kit-h2-2-var-a` и `… kit-h2-2-var-b`. Ворота — как у H1.2 с переменным шагом в задаче 12: последовательность строк `Dock:` по порядку, `Footing LOST` `0`, `skipped carries [1-9]` `0`, `Carry NOT RUNNING` `0`, `footing intact`; сбитие — находка класса F4.

- [ ] **Step 2: Ворота**

Общие — на каждом. Законно красное (§8.2): ничего, кроме `Dock: [Ship] refused -- no free port …` в N4 (одна строка) и `SFCharacter bound to … by search, not by volume` в N6 (одна строка). Вердикт опоры — `footing intact` у всех шести, на каждом из двенадцати журналов:

```bash
grep -o "Shipboard drift:.*" "$F" | tail -1 | grep -c "footing intact"   # 1
```

**H2.1:**
```bash
grep -cF "Dock: [Ship] nearest free port is now Station.Port0 at 22.10 m; next free Station.Port1 at 63.94 m." "$F"   # 1
awk '/nearest free port is now Station.Port0 at 22.10 m/{if(!n)n=NR} /Station rig:/{if(!s)s=NR} END{print (n && s && n<s) ? "choice first" : "ORDER WRONG"}' "$F"
grep -o "LogStarForge: Display: Station rig:.*" "$F" | head -1 | sed 's/^LogStarForge: Display: //' | tr -d '\r' | diff - /tmp/kit-piercore-expected.txt && echo "piercore norm"
```
`/tmp/kit-piercore-expected.txt` — текст нормы из шага 3 задачи 13; если файла нет, записать его тем же `cat` заново.

**H2.2:**
```bash
awk '/Dock: \[Ship\] undocked\./{u=1} u && /nearest free port is now/{print; exit}' "$F"   # ... is now Station.Port0 at 2.93 m; next free Station.Port1 at 60.07 m.
awk '/Dock: \[Ship\] docked\./{d++} d==2 && /the port is Station.Port0, one of 2/{print "second dock at Port0"; exit}' "$F"
```

**N3:**
```bash
grep -cF "Place parked hull: 'Parked' (crewing parked) docked at Station.Port0 -- pilot's and gunner's posts unoccupied, thrusters locked, not simulating, deaf to sf.*." "$F"   # 1
grep -cF "Dock: [Ship] nearest free port is now Station.Port1 at 63.94 m; next free none; held Station.Port0 by Parked at 22.10 m." "$F"   # 1
grep -o "Station rig:.*" "$F" | head -1 | grep -c "Port0 at .*held by Parked, whose release would be 2.93 m.*Port1 at .*held by Ship, whose release would be 2.93 m"   # 1
grep -cF "Frame handover: Station -> Ship, first carry moved him 0.000 cm" "$F"   # 1
printf "turret %s of %s\n" "$(grep 'Turret: \[Parked\]' "$F" | grep -c 'gunned by unoccupied |')" "$(grep -c 'Turret: \[Parked\]' "$F")"   # равны и больше нуля
printf "flight %s of %s\n" "$(grep 'Flight: \[Parked\]' "$F" | grep -c 'flown by unoccupied |')" "$(grep -c 'Flight: \[Parked\]' "$F")"   # равны и больше нуля
```

**N4:**
```bash
grep -cF "the ship starts free at place-local (0.0 -40.0 0.0) m, as the place asks" "$F"   # 1
grep -cF "Dock: [Ship] nearest free port is now none: 2 of 2 held (Station.Port0 by Parked0 at 17.90 m, Station.Port1 by Parked1 at 62.61 m)." "$F"   # 1
grep -cF "Dock: [Ship] refused -- no free port: 2 of 2 held (Station.Port0 by Parked0 at 17.90 m, Station.Port1 by Parked1 at 62.61 m)." "$F"   # 1
grep -cF "Dock: [Ship] free -- NO FREE PORT: 2 of 2 held (Station.Port0 by Parked0, Station.Port1 by Parked1)" "$F"   # >= 20
grep -c "free | port 0.0 m" "$F"   # 0
```

**N6:**
```bash
grep -cF "Place carrier: 'Station' of class SFPierCoreStation in place 'PierCoreTie' at place-local (-30.0 0.0 0.0) m" "$F"   # 1
grep -cF "Dock: [Ship] nearest free port is now Station.Port0 at 37.26 m, tied with Station.Port1 at 37.26 m and taken by the lower index." "$F"   # >= 1
grep -o "Station rig:.*" "$F" | head -1 | grep -c "Port0 at .*held by Ship.*Port1 at .*; free\."   # 1
```

**N8:**

Строк выбора у корабля игрока три, и ворота читают их последовательность, а не число:

1. Выбор `DockImmediately` на старте — та же строка, что у N3: `Port0` уже держит `Parked`, корабль встаёт к `Port1`.
2. Первый выбор после его собственного отхода. `EnterCapture` вернул выбор в «ещё не выбирал» (§7.4), и строка печатается, хотя порт тот же.
3. Смена держателя кандидата. `LetGo` припаркованного снимает его с `Port0`, и строка печатается, хотя ближний порт не сменился. Её отсутствие и есть провал N8 «запомненный выбор» (§8.3).

```bash
cat > /tmp/kit-n8-expected.txt <<'TXT'
Dock: [Ship] nearest free port is now Station.Port1 at 63.94 m; next free none; held Station.Port0 by Parked at 22.10 m.
Dock: [Ship] nearest free port is now Station.Port1 at 2.93 m; next free none; held Station.Port0 by Parked at 60.07 m.
Dock: [Ship] nearest free port is now Station.Port1 at 2.93 m; next free Station.Port0 at 60.07 m.
TXT
grep -o "Dock: \[Ship\] nearest free port is now.*" "$F" | tr -d '\r' | diff - /tmp/kit-n8-expected.txt && echo "three choices, in order"
awk '/Dock: \[Ship\] undocked\./{u=1} u && /Dock: \[Ship\] nearest free port is now/{print; exit}' "$F"   # вторая из трёх: первая после undocked.
awk '/Dock: \[Parked\] undocked;/{p=1} p && /Dock: \[Ship\] nearest free port is now/{print; exit}' "$F"   # третья: первая после отстыковки Parked
grep -cF "Dock: [Parked] releasing, 2.93 m along the port axis over 1.0 s, peak 17.6 m/s2." "$F"   # 1
grep -cF "Dock: [Parked] undocked; its thrusters stay locked: it is parked, and nobody crews it." "$F"   # 1
grep -o "Station rig:.*" "$F" | grep -c "Port0 at .*; free\."   # >= 1
```

Мигание выбора (§7.5, риск дребезга): у стоящего корабля больше одной строки смены в секунду — находка для плана. Число строк выбора у каждого прогона выведено из того, когда `SelectPort` видит смену (задача 15): старт, первый выбор после `EnterCapture`, смена держателя. Повторная стыковка H2.2 зовёт `SelectPort(true)` без смены и без ничьей и строки не даёт.

```bash
grep -c "Dock: \[Ship\] nearest free port is now" "$F"   # H2.1: 1, H2.2: 2, N3: 1, N4: 1, N6: 1, N8: 3
```

- [ ] **Step 3: Регрессия `kit-2.7`**

```bash
python scripts/regress_evidence.py --out Saved/Regress/kit-2.7
python scripts/compare_regress.py Saved/Regress/kit-0-before Saved/Regress/kit-2.7; echo "exit $?"
```

`compare: 0 findings`. Строка в таблицу регрессий.

- [ ] **Step 4: Коммит**

```bash
git status --short
git add docs/superpowers/notes/evidence/*-kit-h2-*.log docs/superpowers/notes/evidence/*-kit-n3-*.log docs/superpowers/notes/evidence/*-kit-n4-*.log docs/superpowers/notes/evidence/*-kit-n6-*.log docs/superpowers/notes/evidence/*-kit-n8-*.log docs/superpowers/notes/*-station-kit-baseline.md
git diff --cached --name-only
git commit -F - <<'MSG'
test(station): step 2 of the station kit at 60 and 400 fps, and the evidence replay after it

The pier core's gates from spec 8.2 and 8.3: the choice DockImmediately makes
from the hall's centre, printed before the station's own line; the first choice
after a release; a parked hull read back unoccupied, locked and deaf, with every
turret and flight line of it unoccupied for the whole run; both ports held and
refused in words; an exact tie taken by the lower index and said; and a port
freed by a parked hull's release, heard by the ship that did not choose it.

The replay against the pass taken before any code change still finds nothing.
MSG
```

---

## Документы

### Task 20: `CLAUDE.md` и спека второй фазы

Правки, которые §11 спеки велит внести после утверждения. Каждая — дословно ниже.

**Files:**
- Modify: `CLAUDE.md` (строки 27, 29; раздел стенда места после абзаца о `Place rig: … a place written in code`; таблица переменных после строки `sf.PlaceRadiusM`; строки `sf.BindFrame`, `sf.AimAt` и `sf.PirateAI`; «Things in the code that look like mistakes and are not»; «Current state»)
- Modify: `docs/superpowers/specs/2026-09-17-world-as-a-graph-of-places.md` (строки 23–24, 678, 716, 748–749)

**Interfaces:**
- Потребляет: журналы задач 4, 12, 19; бланк базы.
- Отдаёт задаче 21 и следующим сессиям: описание набора в `CLAUDE.md`.

- [ ] **Step 1: Семь отступлений**

`CLAUDE.md`, строка 27: `**The six deliberate departures in its own §9.2 stay departures**` → `**The seven deliberate departures in its own §9.2 stay departures**`.

Строка 29: `That is not a seventh disagreement — it is the arithmetic of the` → `That is not a separate departure — it is the arithmetic of the`.

После предложения `Section §10 of the digest lists what was thrown out on purpose; do not bring it back.` дописать:

```
The seventh, recorded on 2026-09-28 by the station-kit spec (its decision 9), is
  ours as well: in the source stations are built and dismantled by a special ship, and
  here the kit's standard is designed so that a player can build them later. Steps 1–2
  of the kit build nothing by hand.
```

- [ ] **Step 2: Стенд места читает другие места**

После абзаца `A third line, \`Place rig: … a place written in code\`, is a \`Warning\` …` вставить:

```markdown
**The place rig reads any place in `/Game/Places/` by name, and a place can start the ship free
or park hulls at its ports.** `sf.PlaceAsset` names the asset by its bare name (`DA_Place_Berth`),
read once in `InitGame`; empty keeps `DA_Place_OpenSpace`, so the rig's default has not moved.
`ShipStart` on the asset starts the player's ship docked (the default) or free at a pose in the
place's frame, and stands the man at an offset in the ship's axes. `ParkedHulls` asks for hulls
docked at named ports — hull name, station carrier name, port index in `GetDockingPorts` order —
checked, spawned before the player's ship with crewing `Parked`, and read back once play has begun:
`Place parked hull: 'Parked' (crewing parked) docked at Station.Port0 -- pilot's and gunner's
posts unoccupied, thrusters locked, not simulating, deaf to sf.*.` The station kit's places are
`DA_Place_Berth` (step 1), `DA_Place_PierCore`, `DA_Place_PierCoreTie`, `DA_Place_PierCoreParked0`,
`DA_Place_PierCoreParkedBoth` (step 2) and the rig station's reproductions `DA_Place_LatentF1Side`
and `DA_Place_LatentF1Roof`; `scripts/station_kit_places.py` writes and reads them back through the
editor's Python commandlet, with the group in `KIT_PLACES_GROUP`. `DA_Place_PierCoreBadRecords` is not
a place but the record check's control: one sound record and seven that each fail one check. Its seven
`Place parked hull: … -- NOT SPAWNED.` lines and the red `Place rig:` line with `1 of 8 parked hulls,
7 NOT SPAWNED` are correct on that asset and a finding on any other.

**A kit station writes its own line, `Station rig:`, at `BeginPlay` and at every change of a port.**
Its box in its own frame, each interior volume against the walls meant to enclose it, and for each
port what reaches past its plane, which volume does, who holds it and — only while that holder is
docked — the release it would make. An upper-case word is a state that did not hold and the line goes
out as an `Error`; red starts where a figure stops printing as `0.000`.

**Two more lines are red on this rig legitimately, and only in control N7 of the station kit.**
`Place rig: sf.PlaceAsset='…' names no place asset at /Game/Places/… -- REFUSED before anything was
measured; …` and `Place rig: place asset '…' REFUSED (no such asset) -- NOTHING WAS ASSEMBLED. …`,
both `Error`, with `not a bare asset name` for a value with a slash in it. They are correct only
while `sf.PlaceAsset` deliberately names an asset that does not exist; otherwise they mean a typo in
the name or an asset saved somewhere else. A refused asset assembles nothing and writes no
`Place size:` line: there is nothing to measure, and another place's size would be credited to it.

**`ESFShipCrewing::Parked`, a third crewing value beside `PlayerShip` and `Automated`.** No
operator takes either post, the thrusters stay locked all its life (`LetGo` says so), and it answers
no `sf.*` except three named ones: `sf.Shields`, which every hull answers; `sf.UndockParked`, which
names it; and `sf.DockRelease`, through the `RequestUndock` that command calls. Who may take a seat is
told to each operator separately since then: the automation sits on the player's ship by
`sf.AutomationOperator` and on an automated one always, the pursuit on an automated one only.
```

- [ ] **Step 3: Переменные**

После строки таблицы `sf.PlaceRadiusM`:

```markdown
| `sf.PlaceAsset` | *(empty)* | The place asset the place rig reads, by its bare name in `/Game/Places/` (`DA_Place_Berth`), not a path: MSYS rewrites an argument of the form `x=/path`. Empty keeps `DA_Place_OpenSpace`. **Read once in `InitGame`**, so `"-ini:Engine:[ConsoleVariables]:sf.PlaceAsset=DA_Place_Berth"`. A name with no asset behind it, or with a slash in it, refuses the place with two `Error` lines and assembles nothing; no fallback place is built in its stead |
| `sf.PortProtrusionM` | *(empty)* | Station-kit controls N1/N1b: a 4 m control collider at the far end of block 0's berth face, its −Y face this many metres past the port plane. A string read by `StarForge::Parse::TryParseFiniteNumber`, never `FDefaultValueHelper::ParseDouble`, which passes `.` and `-` as zero; empty or 0 places nothing; any other value that is not a finite number of at least 0.001 is a `Warning` and places nothing. Read at the kit station's `BeginPlay` and on every change, through the variable's change delegate — the kit station does not tick. A level like `sf.PlaceOffsetM`, so it is not put back when a world ends: a value left from the last Play is a control still set, and the line says so in red. The `Station rig:` `Error` is legitimate only while this is set |
| `sf.StationVolumeReach <Volume> <Metres> [DelaySeconds]` | *(a command)* | Station-kit controls N2/N2b: grows a kit station's interior volume (`Boom0`, `Hall1`, …) by that many metres outward along its block's port axis; 0 restores it. Both numbers read by `StarForge::Parse::TryParseFiniteNumber`. A command with a delay in game seconds, like `sf.KillThruster`, because `-ExecCmds` runs on the first frame, before the start-up handover is carried. The `Station rig:` `Error` is legitimate only while a volume is grown. **0 is control N2's whole return**: a volume going back to its size lets go of the man it took from a cabin, and the cabin's volume, which still holds him, takes him back (`HandleEndOverlap`, the review's N2) with `Frame handover: Station -> Ship, first carry moved him 0.000 cm`; wait for that line before the next `F`. `sf.BindFrame ship` is not needed there and warns that the control did not run |
| `sf.UndockParked <Name> [DelaySeconds]` | *(a command)* | Undocks the named `Parked` hull through the same `RequestUndock` as `F`, so `sf.DockRelease` acts on it too. Refuses with a `Warning` a name that is no ship, a hull that is not parked, a hull that is not docked, a bad delay — read by `StarForge::Parse::TryParseFiniteNumber`, so `.` and `-` are bad delays rather than zero |
```

В строке `sf.BindFrame` заменить `Docked, it is a legitimate handover and everything stays quiet;` на:

```
`station` is the first docking station by name (`ISFDockingStation`), whatever its class. Docked, it is a legitimate handover and nothing is carried wrongly, but it is not quiet: the floor-and-frame cross-check asks nothing about motion and warns `Standing on Ship's floor while bound to Station's frame` after 1/6 s whenever the floor under him is not the carrier he is bound to (checkpoint 6's form: 25 such lines at both rates; station-kit spec §11) — quiet only when he is bound to the carrier whose floor he stands on;
```

В строке `sf.AimAt` после `` `ship` means another ship, never its own. `` дописать `` `station` is the first docking station by name, whatever its class; a miss says `no docking station in this level`. ``

В строке `sf.PirateAI` — там, где описаны `Automated` и его исключение, как велит §11, — заменить `only automated hulls read it, because only they seat this operator |` на:

```
only automated hulls read it, because only they seat this operator. A third crewing value, `Parked` (the place rig's `ParkedHulls`), seats neither this operator nor the automation and answers no `sf.*` except three named ones, `sf.Shields`, `sf.UndockParked` and `sf.DockRelease`; the place rig's section says why |
```

- [ ] **Step 4: Запись в «Things in the code that look like mistakes and are not»**

**Пересмотрено 2026-10-14 (поправка П10).** Ревью 2026-10-01 сделало ту же починку на основной ветке (`633a6ad`, `c2f69b4`, по `2039633` набора) и уже записало её в `CLAUDE.md`: в записи о передаче на 1.0 м/с есть предложение `since 2026-10-01 the station's interior is two volumes on one frame, a room and the tunnel (the single box reached 2 m outside the room's −Y wall across the whole face)`. Отдельной записи с «Until 2026-09-28» не добавлять: рядом с «since 2026-10-01» это противоречие. Вместо этого дописать в конец той записи, после предложения о том, что эпизод принадлежит носителю:

```markdown
  The same fix was made first on `feat/station-kit` on 2026-09-28 (`2039633`), with the same two
  boxes from the same constants, and there the leak was measured wider than "2 m outside": 4.7 m past
  each tunnel wall and 5.1 m over the tunnel's roof, into open space, so a hull flush against that wall
  had the last 1.8 m of its cabin inside the station's volume, and a man walking to the hatch end of his
  cabin was bound to the station on his own floor — F1 with no hull inside the station. Reproduced
  headless before the fix at 60 and 400 fps, beside the tunnel and over its roof
  (`notes/evidence/*-latent-f1-{side,roof}-{60,400}-BEFORE-FIX.log`, the handover between the first
  and third flight lines, as derived); after it the same commands give no handover
  (`*-latent-f1-{side,roof}-{60,400}.log`). The 2026-10-14 merge kept the review's text of the fix.
```

Ворота: `grep -c "Until 2026-09-28 the station had one box" CLAUDE.md` — `0`; `grep -c "since 2026-10-01 the station's interior is two volumes" CLAUDE.md` — `1`.

- [ ] **Step 5: «Current state»**

Перед первым абзацем раздела «Current state» вставить абзац, заполнив числа **из журналов и бланка базы, а не из этого плана**:

```markdown
**Steps 1–2 of the station kit are written and measured on `feat/station-kit`, and checkpoints 11
and 12 are ready to be taken by hand** from `docs/superpowers/notes/<дата>-checkpoint-11-12-runbook.md`.
<N> headless tests (<N₀> before the kit, 16 added), and every regression pass compared against the
base taken before it with `compare: 0 findings`: `kit-1.2` and `kit-1.4` against `kit-0-pre-f4`,
`kit-1.5` against the base on the F4 merge (`kit-0-pre-review`), and every pass from `kit-1.6` against
`kit-0-before`, taken again on 2026-10-14 after the whole-project review of 2026-10-01 was merged into
the kit (the table is in `docs/superpowers/notes/<дата>-station-kit-baseline.md`). The spec is
`docs/superpowers/specs/2026-09-27-station-kit.md`; read it before anything structural about stations.
```

`<N>`, `<N₀>` и `<дата>` — фактические: число `Result={Success}` последнего прогона тестов, N₀ из шапки бланка базы и имена файлов, как они лежат на диске. `<N>` обязано равняться `<N₀> + 16` (122 + 16 = 138 на базе после ревью); иначе это находка, а не опечатка в абзаце.

Заодно в «Current state» `CLAUDE.md` абзац `**A whole-project review ran on 2026-10-01, and its fixes are on \`fix/review-2026-10-01\`, not yet merged.**` переписать по факту: ревью влито в `feat/place-as-data` (`1a3412d`, 2026-10-02) и в `feat/station-kit` (`2861140`, 2026-10-14) (поправка П10).

- [ ] **Step 6: Спека второй фазы**

`docs/superpowers/specs/2026-09-17-world-as-a-graph-of-places.md`:

- строки 23–24: `а шесть\nнамеренных расхождений в его §9.2 остаются расхождениями.` → `а семь\nнамеренных расхождений в его §9.2 остаются расхождениями (седьмое — строительство станций игроком — записано 2026-09-28 спекой станционного набора, решение 9).`
- строка 678: `Точки этой фазы — 8, 9 и 10, и каждая привязана к\nсвоему этапу в §14.` → `Точки этой фазы — 8, 9 и 10, и каждая привязана к\nсвоему этапу в §14. Станционный набор (спека 2026-09-27, решение 8) добавил к ним 11 и 12, по точке на свой шаг; этапы 6 и 7 номера 9 и 10 сохраняют.`
- строка 716: удалить строку таблицы `| Два кандидата на узел на равном расстоянии | строка с обоими числами и названным правилом разрешения |`, а после таблицы дописать абзац: `Контроль ничьей двух кандидатов на узел ушёл вместе с правилом ближайшего свободного узла (§9) на точку проверки 12 станционного набора — контроль N6. Правило читает мировые трансформы портов каждый кадр и о движении носителя не знает ничего: на движущемся носителе меняются его входы, а не функция. Разность скоростей, которую добавляет этап 7, правило не читает, и её контроли у этапа 7 свои.`
- строки 748–749: `Разность скоростей в окне стыковки (§8), орбита и поверхность, исключение орбитального ускорения,\nправило ближайшего свободного узла (§9).` → `Разность скоростей в окне стыковки (§8), орбита и поверхность, исключение орбитального ускорения.\nПравило ближайшего свободного узла (§9) сделано раньше, шагом 2 станционного набора (спека 2026-09-27, §1, решение 8): на неподвижных станциях оно проверяется само по себе, и перенос риска не добавляет.`

Строку 619 (выбор ближайшего свободного узла в перечне того, что идёт в `StarForgeCore`) не трогать — функция туда и пришла (§11). Короткий конспект `source/2026-09-10-novel-source-for-starforge.md` не трогать (§11).

- [ ] **Step 7: Коммит**

```bash
git status --short
git add CLAUDE.md docs/superpowers/specs/2026-09-17-world-as-a-graph-of-places.md
git diff --cached --name-only
git commit -F - <<'MSG'
docs: the station kit in the project guide, and what it moves in the second-phase spec

The guide learns the new variables and commands, the two refusal lines that are
red only in control N7, the parked crewing value and its three named exceptions,
the kit station's own line, and why the rig station now has two interior boxes.

Two corrections come with it. The departures from the source are seven now, and
the sentence that called the kilometres a second "not a seventh disagreement"
would read as denying the seventh. And the sf.BindFrame row promised silence for
a docked binding that the cross-check has never given: it warns whenever the
floor under him is not the carrier he is bound to, and checkpoint 6 recorded it.

The second-phase spec moves the nearest-free-port rule and its tie control from
stage 7 to the kit's step 2, and names checkpoints 11 and 12 beside its own.
MSG
```

---

### Task 21: Ранбук точек проверки 11 и 12 — по шагам в PowerShell — и бланк

Владелец проходит ручные точки сам и приносит журналы на разбор; перед каждой он просит пошаговый гайд команд **в PowerShell**. Образец структуры — `docs/superpowers/notes/2026-09-18-checkpoint-8-runbook.md`; отличие — все команды в PowerShell, а не в Git Bash.

**Files:**
- Create: `docs/superpowers/notes/<дата>-checkpoint-11-12-runbook.md`
- Create: `docs/superpowers/notes/<дата>-checkpoint-11-12-results.md`

**Interfaces:**
- Потребляет: ворота §8.5, законно красное §8.2 и §8.3, дословные строки из журналов задач 12 и 19.
- Отдаёт владельцу: ранбук и бланк.

- [ ] **Step 1: Ранбук — разделы**

1. **Что мы выясняем** — §8.5 списком: петля руками на причальной секции; N1, N1b, N2b, N5, N9, N9b руками; N2 с возвратом (поправка П4: последним он больше быть не обязан); ответы глазами (виден ли из зала свой корабль через стёкла без меша; читается ли направление в зале; ощущается ли канал в 12 м); для точки 12 — перелёт между портами со строкой смены выбора, вход в `Hall1`, N3, N4 (с красной строкой панели), N8, N6.
2. **Что уже проверено без рук** — ссылки на журналы задач 12 и 19 и на таблицу регрессий бланка базы.
3. **Что знать до первого запуска** — строка `Station rig:` и как её читать (норма §6.8 дословно, разбор каждой части); как N2 возвращается сам (§8.3 после поправки П4: `sf.StationVolumeReach Boom0 0`, затем ждать `Frame handover: Station -> Ship, first carry moved him 0.000 cm`; `sf.BindFrame ship` не набирать — он теперь даёт `Warning` `… Nothing was rebound; the control did not run.`, которого в списке нет); почему N5 руками стоя, а не из кресла (§8.3); что `sf.PlaceAsset` — стартовая переменная, только ключом `-ini`.
4. **Перед началом** — PowerShell, дословно:

```powershell
Set-Location F:\UEProject\StarForge-station
git branch --show-current            # feat/station-kit
git rev-parse --short HEAD           # в бланк
git status --short                   # только файлы владельца
Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object { $_.CommandLine -like '*StarForge-station*' } | Select-Object ProcessId   # пусто
$UE  = "C:\Program Files\Epic Games\UE_5.8\Engine"
$Log = "Saved\Logs\StarForge.log"
function Save-Log([string]$Name) { Copy-Item $Log ("docs\superpowers\notes\evidence\{0:yyyy-MM-dd}-{1}.log" -f (Get-Date), $Name) }
function Count([string]$Pattern) { (Select-String -Path $Log -Pattern $Pattern).Count }
function Show([string]$Pattern) { Select-String -Path $Log -Pattern $Pattern | ForEach-Object Line }
& "$UE\Binaries\Win64\UnrealEditor-Cmd.exe" --% "F:\UEProject\StarForge-station\StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
Count 'Test Completed. Result=\{Success\}'   # N₀ + 16: число из шапки бланка базы плюс шестнадцать (138 при N₀ = 122, база после ревью)
Count 'Test Completed. Result=\{Fail\}'      # 0
```

   Если сомнение в сборке — пересборка с очисткой:

```powershell
& "$UE\Build\BatchFiles\Rebuild.bat" StarForgeEditor Win64 Development -Project="F:\UEProject\StarForge-station\StarForge.uproject" -WaitMutex
```

   **Закрывать редактор только своего дерева** (фильтр по командной строке выше и `Stop-Process -Id`), никогда `Stop-Process -Name UnrealEditor`: в основном дереве работает другая сессия.

5. **Запуск** — форма для всех ручных прогонов, с `--%` после пути к программе (остаток строки уходит движку как есть, с кавычками и квадратными скобками):

```powershell
& "$UE\Binaries\Win64\UnrealEditor.exe" --% "F:\UEProject\StarForge-station\StarForge.uproject" /Game/Maps/L_Place "-ini:Engine:[ConsoleVariables]:sf.PlaceAsset=DA_Place_Berth" -game -windowed -ResX=1600 -ResY=900 -ExecCmds="sf.FlightLog 1, sf.DriftDebug 1" | Out-Null
```

   `| Out-Null` в конце — чтобы PowerShell ждал закрытия окна (поправка П18): без него `&` возвращает управление сразу, и `Save-Log` скопировал бы журнал ещё открытого окна. После каждого запуска, до следующего: `Save-Log '<имя>'`. Открытый редактор этого дерева уводит журнал прогона в `StarForge_2.log` — перед каждым запуском он закрыт.

6. **Управление** — таблица клавиш из ранбука точки 8 без изменений (`E`, `F` только из кресла пилота, `F1` или `~` — консоль, `sf.ConsoleOperator 0` перед `E`).

7. **Красные строки и законно красное** — таблица по прогонам из §8.2 и условия из §8.3, дословно. Отдельной строкой: строки `dock    free -- NO STATION` на панели с ревью 2026-10-01 нет вовсе. Без станции панель пишет белым `dock    free -- no station in this level`; станция есть, а порт не найден — красным `dock    free -- <станция> is in this level and the docking found NO STATION port`, и здесь это всегда находка. `dock    free -- NO FREE PORT` законна только в N4. Ещё красные строки панели, которых до ревью не было: `CARRY NOT RUNNING - bound to … never carried` и `footing  NONE …` (`SFFrameDriftHUD.cpp`) — находка в любом прогоне этого ранбука. В разделе 10 — оранжевая `PLACE REFUSED - sf.PlaceAsset '…': …; nothing was assembled …` (стык слияния, `42c90a6`): законна только в N7. Ещё одна строка про панель: припаркованный корпус она показывает блоком `combat` как другой корабль, белым, `flown by unoccupied` (§7.6). Это не красная строка, и красной она быть не должна.

8. **Точка 11** — по запуску на раздел, каждый с командой, шагами, стоп-воротами дословно и `Save-Log`:
   - **11-A** (`DA_Place_Berth`, имя журнала `checkpoint-11-A-berth-by-hand`): петля каюта → `sf.ConsoleOperator 0`, `E` → `F` (строка `Dock: [Ship] releasing, 2.93 m …`) → полёт → назад → `CAN DOCK` → `F` → `E` → хатч → канал 12 м → зал → обратно; ворота — `Count 'skipped carries [1-9]'` 0, `Count 'Carry NOT RUNNING'` 0, обе `Frame handover` с `0.000 cm`, норма `Station rig:`. Затем N1: `sf.PortProtrusionM 1.0`, `F` из кресла, стоп-ворота N1 дословно; `sf.PortProtrusionM 0`, подлёт в окно, `F`. N1b так же с `0.2`. N2b: `sf.StationVolumeReach Hall0 1.0`, ворота, возврат `sf.StationVolumeReach Hall0 0`. N2: `sf.ConsoleOperator 0`, `E`, `sf.StationVolumeReach Boom0 4.0`; стоп-ворота — строка `Station rig:` с `Boom0 REACHES 4.000 m OUT of its channel` и `VOLUME REACHES 4.000 m PAST it (Boom0)`, `Frame handover: Ship -> Station, first carry moved him 0.000 cm`, строка `Frame handover: Ship -> Station took a SEATED man (Ship's pilot post): a volume of Station reaches into Ship's cabin.`, строка `Standing on …`. Возврат (§8.3, поправка П4): `sf.StationVolumeReach Boom0 0`; ворота возврата — `Frame handover: Station -> Ship, first carry moved him 0.000 cm`, и строки `SEATED` на возврате **нет** (корабль — владелец его кресла); только после неё следующий `F`. `sf.BindFrame ship` не набирать.
   - **11-B** (`DA_Place_Berth`, `checkpoint-11-B-n5-standing`): N5 стоя — кресло держит консоль, в консоли `sf.DockRelease 0`, `sf.Dock 0`, `sf.Intent.X 1`; ворота — `Show 'Shipboard drift'` с `KNOCKED DOWN`.
   - **11-C** (петля, `checkpoint-11-C-latent-f1-by-hand`):
```powershell
& "$UE\Binaries\Win64\UnrealEditor.exe" --% "F:\UEProject\StarForge-station\StarForge.uproject" /Game/Maps/L_Loop -game -windowed -ResX=1600 -ResY=900 -ExecCmds="sf.FlightLog 1" | Out-Null
```
     N9: `sf.ConsoleOperator 0`, `E`, `F`, отойти, подвести корабль к стене −Y сбоку от тоннеля медленнее 1 м/с до касания, `E`, подойти к стене хатча; затем то же над крышей тоннеля (N9b). Ворота — `Count 'Frame handover: Ship -> Station'` 0 и `Count 'Standing on'` 0 при `dock free`.
   - **Глазами** — три вопроса §8.5, ответы словами в бланк.
9. **Точка 12**:
   - **12-A** (`DA_Place_PierCore`, `checkpoint-12-A-two-ports`): стыковка к `Port0` → отстыковка → перелёт на 60 м к `Port1`; при пересечении серединной плоскости строка `Dock: [Ship] nearest free port is now Station.Port1 at …` выходит **один раз** и с обоими числами (`Show 'nearest free port is now'`); `CAN DOCK` у `Port1` → `F` → через его канал в `Hall1`: `Frame handover: Ship -> Station, first carry moved him 0.000 cm`. Числа подхода — протокол.
   - **12-B** (`DA_Place_PierCoreParked0`, `checkpoint-12-B-n3`): стартовые строки N3 в бланк дословно. На панели блок `combat` показывает `Parked` другим кораблём — белым, `flown by unoccupied` (§7.6); записать словами, что видно. Затем `sf.ConsoleOperator 0`, `E`, `F`, отлёт и `F` в окне `Port1`.
   - **12-C** (`DA_Place_PierCoreParkedBoth`, `checkpoint-12-C-n4`): `sf.ConsoleOperator 0`, `E`, `F`: строка отказа `refused -- no free port`; на панели красным `dock    free -- NO FREE PORT` — записать словами, что видно.
   - **12-D** (`DA_Place_PierCoreParked0`, `checkpoint-12-D-n8`): `sf.ConsoleOperator 0`, `E`, `F` (отход от `Port1`); в консоли `sf.UndockParked Parked`. Ворота — три строки выбора N8 по тексту и по порядку (старт, после своего `undocked.`, после отстыковки `Parked`); расстояния — протокол. Блок `combat` панели показывает `Parked` белым, `flown by unoccupied` (§7.6); что он показывает после отстыковки `Parked`, спека не говорит — записать словами, что видно.
   - **12-E** (`DA_Place_PierCoreTie`, `checkpoint-12-E-n6`): строка ничьей дословно; `SFCharacter bound to … by search, not by volume` здесь законна и только здесь.
10. **Если пошло не так** — из ранбука точки 8 то, что относится к стенду места (нет строки `Place size:`; ключ `-ini` не подействовал — искать `CVar [[sf.PlaceAsset:…]]`; журнал 205 КБ без строк `LogStarForge`; консоль не открывается — `F1`), плюс: `Place rig: sf.PlaceAsset='…' names no place asset` вне N7 — ассет не найден; строка `Station rig:` отсутствует — станция не набора или класс не собран.
11. **Что дальше** — таблица решений по исходам, как в ранбуке точки 8: всё по ожиданию — `git merge --no-ff feat/station-kit` решает владелец; любой красный вне своего контроля — находка, этап стоит.

- [ ] **Step 2: Бланк**

По разделу на запуск (11-A … 12-E): дата, хэш, частота кадров из `sf.DriftDebug`, графа **«строка стоп-ворот, дословно»** у каждого контроля, счётчики `skipped carries` и `Carry NOT RUNNING`, расстояния-протокол; раздел «Глазами»; таблицы мутантов задач 7 и 14 («ожидалось / упало на самом деле»); таблица регрессий из бланка базы; решение в конце.

- [ ] **Step 3: Коммит**

```bash
git status --short
git add docs/superpowers/notes/*-checkpoint-11-12-runbook.md docs/superpowers/notes/*-checkpoint-11-12-results.md
git diff --cached --name-only
git commit -F - <<'MSG'
docs: checkpoints 11 and 12, step by step in PowerShell, with the form they fill

The owner takes the hand checkpoints himself and brings the logs back, and asks
for the commands in PowerShell before each. Every run here is a launch line with
--% after the program, the steps, the stop-gate lines word for word and the copy
of the log; the legitimately red lines are listed per control with the condition
under which each is a finding again.

N2 returns by itself since the 2026-10-01 review: a volume going back to its
size hands the man it took back to the cabin that still holds him, so the
runbook waits for that handover line instead of ending the session on N2, and
no longer types sf.BindFrame ship, which would now warn that it did nothing.
MSG
```

---

## Самопроверка по спеке

**Покрытие.** Каждый пункт §9 и каждое требование §6–§8 — в задаче:

| Спека | Задача |
|---|---|
| §9 шаг 1, п. 1 — база, пять групп команд | 1 (`--groups`, 19 команд; сравнитель и его контроль) |
| п. 2 — `sf.PlaceAsset`, строки отказа, `ShipStart`, смещение; `kit-1.2` | 2 |
| п. 3 — ассеты воспроизведения, N9/N9b до, `BEFORE-FIX`, окно ворот | 3 |
| п. 4 — две коробки, эпизод на носитель, N9/N9b после, `kit-1.4` | 4 |
| п. 5 — интерфейс, порт-сирота, три поиска, `kit-1.5` | 5 |
| п. 6 — держатель и четыре уведомления, `kit-1.6`; §6.7 п. 5 — `MeasureRelease` | 6 |
| п. 7 — `BoundsPastPortPlane`, порог, `BoxReachOutside`, 8 тестов и мутанты | 7 |
| п. 8 — `ASFKitStation`, `ASFBerthStation`; §6.3–§6.5 | 8 |
| п. 9 — строка `Station rig:`, контроли, `SEATED` | 9, 10, 11 |
| п. 10 — `DA_Place_Berth` (сделан в 8), H1.1–H1.6, N1, N1b, N2, N2b, N5, N7; `kit-1.10` | 12 |
| п. 11 — точка 11 | 21 (ранбук) |
| §9 шаг 2, п. 1 — `SelectNearestFreePort`, 8 тестов и мутанты | 14 |
| п. 2 — выбор каждый кадр, «ещё не выбирал», `ResolvePorts` вне `Free`, порядок кандидатов; `kit-2.2` | 15 |
| п. 3 — строки журнала, `NO FREE PORT`, панель | 15 (смена и ничья), 18 (отказ, полёт, панель, имена портов) |
| п. 4 — `Parked`, операторы, `LetGo`; `kit-2.4` | 16 |
| п. 5 — `ParkedHulls`, проверка записи, `DockImmediatelyAt`, `Place parked hull:`, `sf.UndockParked` | 17 (проверка записи — прогоном `DA_Place_PierCoreBadRecords` на 60 и 400, шаг 7) |
| п. 6 — `ASFPierCoreStation`, четыре ассета | 13, 15, 17, 18 (пятый, контрольный `DA_Place_PierCoreBadRecords`, — 17) |
| §8.2 — строка `World bounds:` во всех прогонах, вердикт опоры у каждого | «Как проверяется» (общие ворота, N7 тоже), 12 и 19 (вердикт — командой у каждого прогона) |
| §6.7, «Где снимаются цифры» — база заново после слияния F4 | Global Constraints (варианты а/б/в), 1 (шаг 1), 5 (шаг 0) |
| п. 7 — H2.1, H2.2, N3, N4, N6, N8; `kit-2.7` | 19 |
| п. 8 — точка 12 | 21 |
| §11 — правки `CLAUDE.md` и спеки второй фазы | 20 |
| §8.1 — шестнадцать тестов, мутант у каждого | 7 (K1–K12), 14 (P1–P9; P9 — поправка П6) |

**Заглушек нет.** В коде нет `TODO` и «аналогично задаче N»; единственное тело, которое в задаче 8 ничего не делает, — `OnDockingPortChanged`, и задача 9 его заменяет (в комментарии это сказано). Имена файлов улик и документов с `<дата>` — день выполнения, `$(date +%F)`; `<N>` и `<N₀>` в «Current state» — фактические числа из журнала тестов и бланка базы, а не выдуманные. Счета тестов в задачах записаны от N₀, а не числом, потому что F4 добавляет семь тестов и может влиться до базы.

**Согласованность типов.** `ISFDockingStation::FindAll(const UWorld*, const AActor*)` — одна сигнатура в задачах 5 и 15. `USFDockingPortComponent::NotifyHolder(USFDockingComponent*)`, `GetHolder()`, `IsHeld()` — задача 6, читаются в 9, 15, 17. `MeasureRelease(double&, double&) const` и `StationPhysicalBoundsLocalCm(const AActor&)` — задача 6, читаются в 9. `FSFKitVolumeRecord::Faces[6]` в порядке XMin, XMax, YMin, YMax, ZMin, ZMax — задачи 8 и 9. `FPortChoice::Distances` — в единицах вызывающего (сантиметры), в метры переводится только в `LogPortChoice` и `HeldPortsList`. `ESFShipStartMode` / `FSFShipStart` названы так, чтобы Python-имена не совпали (`unreal.SFShipStartMode` и `unreal.SFShipStart`). `ESFShipCrewing::Parked` дописан последним. `DockingKit::PortLabel` объявлен в задаче 15 и используется в 17 и 18 — после него.

**Что сверено с кодом, а не с памятью.** Номера строк ссылок — на дереве `bcdb849` + `70800f4`; каждая задача меняет файлы, и номера строк последующих задач в тех же файлах сдвинуты правками предыдущих: там, где это так, задача называет функцию и якорный текст, а не только строку. Правки `SFDockingComponent.cpp` в задачах 5, 6, 15–18 сверены с копией F4 в основном дереве (2026-09-28). Все их якоря — `ResolvePorts` с комментарием «Counted rather than taken…», `State = ESFDockingState::Capturing;` в `EnterCapture`, строка `docked.` в `CompleteCapture`, блок «The standoff, measured on the two bodies as they are» в `RequestUndock` (блок совпадает дословно), строка `undocked.` в `LetGo` — в ней есть. Вне якорей F4 меняет тела `CompleteCapture` и `AdvanceRelease` и номера строк.

**Строки выбора N8 пересчитаны по коду задачи 15:** три, а не две. Раньше ворота мигания ждали 2 и на исправной сборке дали бы ложную находку.

---

## Чего здесь нет

Всё из §10 спеки: кольцо и вращение, колыбель, карман ворот, кубооктаэдр; строительство игроком; станция как данные; импорт арт FBX (с ним понадобится `NoCollision` у меша, смещение меша от корня (11.62, 5.9, −16) м и проверка, что строка `Station rig:` не изменилась ни символом); коридор, задняя дверь зала, тамбуры, пеший стык, лифты; полный причал К3 листа `A16`; движущаяся станция и разность скоростей в окне; абордаж — интерфейс к нему готов, но порты кораблей кандидатами не становятся; «Кришна» как физический корпус (вопрос 7 открыт, шаги 1–2 от него не зависят); двери, замки, огни, номер причала, материал стекла; низкогравитационный порт. И подписки на уничтожение держателя порта: ни один прогон шагов 1–2 не уничтожает пристыкованный корпус (§6.7, п. 4).
