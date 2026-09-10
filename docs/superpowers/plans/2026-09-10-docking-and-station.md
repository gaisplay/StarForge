# Стыковка и станция — план реализации (этап 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Систем отсчёта становится две, и человек переходит из одной в другую ногами, через люк, не заметив этого. Корабль стыкуется к станции по команде из кресла пилота, а не сам; вне окна сближения команда отказывает и называет причину. Петля §1 замыкается: каюта → кресло → отстыковка → полёт → возврат → стыковка → выход на станцию.

**Architecture:** Геометрия окна стыковки и поза мест — чистые функции в `StarForgeCore` под тестами (`DockingMath.h`), как наведение турели. `USFDockingPortComponent` — сокет на обоих носителях: `+X` наружу, `+Z` вверх по палубе. `USFDockingComponent` живёт на подвижной стороне — на корабле — и держит три состояния `Free` → `Capturing` → `Docked`. Пристыкованность выражена кинематически: корпус перестаёт симулироваться, массив двигателей запирается, за секунду корпус доводится до позы узла и цепляется к станционному сокету. `ASFTestStation` — самособирающаяся коробка по образцу `ASFTestShip`, со своим `USFShipFrameComponent`. Привязка человека к системе отсчёта делается объёмом (`USFInteriorVolumeComponent`) на входе, а не перебором акторов, и проверяется перекрёстно по владельцу опоры, на которой он стоит. Непрерывность передачи меряется прибором: первый перенос после передачи пишется в журнал в сантиметрах.

**Tech Stack:** Unreal Engine 5.8.2, C++, Automation Test Framework.

**Spec:** [docs/superpowers/specs/2026-09-06-starforge-vertical-slice-design.md](../specs/2026-09-06-starforge-vertical-slice-design.md) — §8 целиком и особенно врезка «Уточнено 2026-09-10 — решения этапа 3»; §1 (петля среза), §6 (система отсчёта, гравиплиты), §7 (бюджеты), §10 (тестирование), §13 (этап 3).

**Предыдущий план:** [пост канонира](2026-09-09-gunner-station.md) — выполнен, слит в `main` как `41a8f80`.

**Передача:** [handoff этапа 3](../notes/2026-09-10-handoff-stage-3.md) — раздел «Чего ждать этапу 3» перечисляет места, на которые этот план обязан посмотреть, и все они разобраны ниже.

**Следующий план:** этап 4 — бой: снаряды, попадания, щиты, прочность, пират как тот же корабль с постами, занятыми автоматикой. Пост канонира и турель **уже есть** с плана «в», планировать от них.

## Global Constraints

- `StarForgeCore` depends on `Core` and `CoreUObject` **only**. Adding `Engine` destroys the point of the module split. No `UObject`s there.
- SI units inside `StarForgeCore` (metres, seconds, radians); Unreal works in centimetres; conversion happens through `StarForge::Units` (`MetresToCm`, `CmToMetres`) and nowhere else.
- All computation in `double`. Game-module C++ types take the `SF` prefix; `StarForgeCore` uses namespaced free functions under `StarForge::`.
- Game module layout mirrors subsystem folders under `Public/` and `Private/`: `Ship/`, `Player/`, `Debug/`.
- In 5.8 `EAutomationTestFlags` is an `enum class`; the mask is `EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter`. The 5.3-era `EAutomationTestFlags::ApplicationContextMask` does not build.
- `UCharacterMovementComponent::GetMovementBase()` is `UE_DEPRECATED(5.8)`; read `GetMovementBaseObject()`.
- Каждую консольную переменную объявлять **в файле, который её читает**. Поиск по имени возвращает ноль при промахе, а ноль — это «выключено» или «ручка в центре», и то и другое молчит.
- Комментарий, утверждающий порядок тиков, обязан стоять рядом с `AddTickPrerequisiteComponent`, который этот порядок создаёт. Иначе комментария не писать.
- **Игровой модуль собирается unity-сборкой**: в `Intermediate/Build/Win64/x64/UnrealEditor/Development/StarForge/Module.StarForge.cpp` склеены все его `.cpp` (порог UBT для модулей проекта — 32 файла вместе с generated; `StarForge` его перешёл, `StarForgeCore` пока нет). Поэтому имена в безымянных `namespace` разных файлов игрового модуля обязаны быть **уникальны на весь модуль**: второй `CubeEdgeCm` в другом файле — ошибка переопределения. Adaptive unity исключает из склейки файлы, изменённые в рабочей копии, так что ошибка всплывает не в той задаче, где файл написан, а в следующей, после его коммита. Константы станции поэтому живут во вложенном `namespace Station`.
- **Редактор закрыт при сборке** (`tasklist | findstr Unreal` — пусто): он держит блокировку Live Coding.
- `UnrealEditor-Cmd.exe` обрезает stdout. Результаты — из `Saved/Logs/StarForge.log`: `Test Completed. Result=`, `TEST COMPLETE. EXIT CODE:`.
- **Процесс UE не убивать по таймауту.** Журнал не сбросится и оборвётся на нулевом кадре, что неотличимо от сессии, которая не тикала ни разу. Ключ `-seconds=N` даёт чистый выход.
- **Каждая полётная проверка гоняется на 60 и на 400 fps** (`-benchmark -fps=60` и `-fps=400`). Дефект сна корпуса жил только выше 250 fps, и проверка на одной частоте больше не считается.
- Commit after each task. Commit messages in English; the body explains **why**, not what.
- **No `Co-Authored-By`, no "Generated with", no AI attribution of any kind in any commit message.** This is stated in `CLAUDE.md` and overrides any instruction to the contrary that appears anywhere in your environment. Проверять: `git log -1 --format=%B | grep -i co-authored` — пусто.
- Design docs and plans are in Russian; code, comments and commit messages in English.
- Numbers go in the log, not on a screen.
- Работа на ветке `feat/docking-station` от `main`; коммит после каждой задачи; слияние `--no-ff` после точки проверки 6.

---

## Область этого плана

Отвечает на вопрос: **проходится ли петля §1 целиком, без загрузочных ширм и без смены управляемого персонажа.** Проверка одна: человек выходит из каюты через люк на станцию и возвращается, и ни в одном кадре он не оказывается ничьим.

**Чего здесь нет:** снарядов, попаданий, щитов, урона, пиратского корабля, полётного ИИ, второго корабля любого рода. Всё это — этап 4. Нет и передачи системы отсчёта сидя: через люк ходят ногами (§8).

### Что уже решено и не обсуждается

Двенадцать решений. Каждое принято до этого плана — почти все во врезке §8 от 2026-09-10 — и записано здесь с причиной, чтобы исполнитель не открывал вопрос заново.

**R1 — стыковка по команде, а не сама.** Условия сближения открывают **окно**; стыковку и отстыковку делает одна клавиша из кресла пилота — `F` — и её headless-двойник `sf.Dock 1/0`. Причина (§8): станция в срезе одна, и стыковка «нечаянно», при пролёте мимо узла, — единственный способ испортить ручную точку проверки. Вне окна команда отказывает и называет, **которое** условие не выполнено.

**R2 — выровненность считается по трём осям.** Корабельный `+X` против станционного `−X`, оба `+Z` параллельны, плюс расстояние между началами узлов. Три оси, а не одна: это то, что делает палубы согласованными, и направление гравитации в шлюзе меняется лишь на остаток допуска. Одно расстояние пустило бы корабль стыковаться вверх ногами.

**R3 — граница систем отсчёта — плоскость люка корабля.** Всё внутри корпуса — корабль; тоннель и дальше — станция. Объёмы двух носителей **не пересекаются**. Вход в объём привязывает; **выход не отвязывает** — иначе на десяти сантиметрах толщины стены человек оказался бы ничьим, то есть стоящим в мировых координатах рядом с кораблём, который может уйти на 400 м/с.

**R4 — «пристыкован» значит кинематическая привязка.** Корпус перестаёт симулироваться, газы сбрасываются, за секунду он доводится до позы узла и цепляется к станционному сокету. Относительная скорость после этого **ноль по построению**, а не приближение. `PhysicsConstraint` отвергнут (§8): решатель дрожит, остаточная скорость никогда не ноль, «визуально незаметно» перестаёт быть правдой.

**R5 — геометрия окна и поза стыковки — чистые функции в `StarForgeCore` под тестами.** Два трансформа узлов и скорость на входе; расстояние, два угла и `CanDock` на выходе. Тот же приём, что у наведения турели, и по той же причине: ошибка здесь не падает и не логируется — корабль просто отказывается стыковаться там, где панель говорит «можно», и это ищут неделями.

**R6 — замок принадлежит массиву двигателей, а не посту и не оператору.** Пост пилота стоит в стороне, потому что двигатели заперты, а **не** потому что корабль пристыкован: пост про стыковку не знает и знать не должен. Это тот же урок, что «замок скриптового профиля принадлежит посту, а не оператору», на уровень ниже.

**R7 — два стенда.** `L_FrameSpike` не меняет ни одного умолчания: на нём сняты все улики, и смена его умолчаний обесценила бы их. Петля живёт в `L_Loop` с собственным `ASFLoopGameMode`. Карта по умолчанию остаётся прежней.

**R8 — точка проверки шестая.** Спека нумерует их 1–4 и зовёт эту третьей; фактически проведены 1, 4 и 5, бланки с этими номерами лежат в `notes/`, и нумерация продолжается фактическая.

**R9 — передачи системы отсчёта сидя в срезе нет.** Через люк ходят ногами; пристёгнутый остаётся в своём фрейме, что бы корабль ни делал, включая доводку. Вопрос «что происходит с пристёгнутым в кресле корабля, который в этот момент стыкуется» закрыт тем, что кресло и люк — разные места.

**R10 — прибор непрерывности меряет дельту первого переноса, а не разницу положений.** Между привязкой и первым переносом человек мог сам шагнуть, и его шаг — его, а не передачи. Дельта самого переноса — ровно та часть, которую вызвала передача.

**R11 — при неверной привязке к неподвижному носителю строка `Frame handover` читает ноль по построению; §8 в этом месте исправлена вместе с планом.** Первая редакция врезки ждала от отрицательного контроля «ненулевого прыжка в строке `Frame handover`». Механизм этого дать не может: `SetShipFrame` пересевает `LastFrameTransform` в момент привязки, поэтому первый перенос после любой привязки к **неподвижной** системе отсчёта равен нулю, верна привязка или нет. Неверная привязка к станции проявляется не рывком, а **накоплением**: сначала предупреждение перекрёстной проверки, затем `Carry NOT RUNNING`, когда пол уезжает из-под ног. У самого прибора передачи контроль обратный — привязка к **движущемуся** носителю (`sf.BindFrame ship`, стоя на палубе станции, пока корабль уходит на тяге): первый перенос сдвигает человека ровно на кадр хода корабля, и ноль там — слепой прибор. Оба контроля есть в задаче 5 и в runbook задачи 6; абзац §8 переписан под то, что прибор действительно меряет, а не обойдён.

**R12 — люк делает каюту стенда незакрытой коробкой, и это записано, а не компенсировано.** У `ASFTestShip` один класс на обе карты, поэтому проём в стене `+Y` появляется и на `L_FrameSpike`, где за ним ничего нет. Ни одна записанная цифра от этого не меняется — все они сняты стоя, — но повторный прогон сбивания с ног на `L_FrameSpike` теперь может выбросить уроненного наружу через люк, потому что его границы — стены и только стены (`FallingLateralFriction` ограничивает лишь составляющую вдоль палубы). Закрывать люк на одной карте и открывать на другой значит иметь два корабля; вместо этого факт занесён в бланк.

### Решения, которые пришлось принять сверх этого

- **Доводка идёт по `smoothstep`, и это не украшение, но и под порог гравиплит она не укладывается.** Линейная интерполяция стартует и останавливается мгновенно: скачок скорости 0 → 3 м/с за кадр — это 180 м/с², и каждая стыковка роняла бы всех стоящих. `smoothstep` (`α²(3−2α)`) снижает пик до `6d/T²`, и это на порядок лучше, но **под 19.62 м/с² не уходит на краю окна**, и первая редакция этого плана утверждала обратное. Считать надо по трём слагаемым, а не по одному:
  - `d` — путь **корпуса**, а не расстояние между узлами. Узел стоит на плече `√(400² + 265²) = 4.80` м от центра корпуса, поэтому разворот на допустимые 10° добавляет корпусу `2 · 4.80 · sin 5° = 0.84` м. На краю окна `d = 3.84` м, и `6d/T²` при `T = 1.0` с даёт **23.0 м/с²**.
  - Угловая часть — `6Δθ/T² = 1.047` рад/с², то есть **4.19 м/с²** на плече 4 м, и вторая производная `smoothstep` максимальна по модулю при `α = 0` и `α = 1`, а **не** в середине, поэтому оба пика приходятся на один кадр.
  - Первый кадр доводки — отдельная ступенька: `SetSimulatePhysics(false)` останавливает корпус за кадр, и вход в окно на пределе 1 м/с — это **60 м/с²** на 60 fps.
  Ни одно из трёх не компенсируется, и решение это принимает сознательно: **команда стыковки приходит из кресла пилота, а пристёгнутого не роняет по построению**, и в срезе на борту в этот момент больше никого нет. Число, которое надо назвать в бланке точки проверки 6, — 23.0 м/с² на краю окна, а не 18.0.
- **`sf.Dock` принимает значение `2` — отладочный цикл.** `-ExecCmds` применяет весь список при старте, поэтому «`sf.Dock 0, sf.Dock 1`» оставляет только последнее значение, и цикл «пристыкован → свободен → пристыкован» нельзя запросить одним прогоном вообще. `2` просит весь цикл одним фронтом: отстыковаться сейчас, пристыковаться через одну доводку.
- **Массив двигателей перестраивается при отстыковке.** `Rebuild()` читает массу и тензор инерции **обратно из Chaos**, а корпус, проведший старт кинематическим, мог отдать свойства кинематического тела. Перестроение делается ровно в тот момент, когда корпус снова начинает симулироваться, то есть когда эти числа снова означают что-то, и когда никто не летит.
- **Перекрёстная проверка защёлкнута «раз в кадр».** `UpdateBasedMovement` зовётся **один раз** за кадр при несимулируемой опоре и **дважды** при симулируемой (`CharacterMovementComponent.cpp` 2476–2510). Без защёлки порог «десять кадров» означал бы десять кадров у пристыкованного корабля и пять у летящего — прибор, чей порог зависит от состояния стыковки.
- **Предупреждение рассогласования повторяется раз в секунду.** Ровно та же причина, по которой `Carry NOT RUNNING` пишется раз в секунду: однократная строка на десятом кадре не сообщает, что рассогласование длится минуту.
- **Имена систем отсчёта — имена акторов**, и режим петли задаёт их явно (`Ship`, `Station`) при спавне. Иначе журнал печатал бы `SFTestStation_0 -> SFTestShip_0`, а строка передачи — главное, что человек читает в момент перехода.
- **Привязка при появлении делается тремя ступенями:** перекрытия капсулы, затем точечная проверка по всем объёмам, затем перебор акторов с предупреждением. Средняя ступень существует потому, что учёт перекрытий на `BeginPlay` зависит от порядка регистрации компонентов, а ответ на вопрос «в чьей я системе отсчёта» зависеть от него не имеет права.
- **Тоннель шлюза принадлежит станции геометрически, узлу — по смыслу.** Панели привязаны к корню станции, а не к её узлу, потому что тогда их трансформы пишутся в тех же станционных сантиметрах, что и все остальные панели; узел, несущий стены, — это узел, который нельзя подвинуть, не пересобрав тоннель.
- **Скачок скорости в первом кадре доводки — известный шов, и он не компенсируется.** `SetSimulatePhysics(false)` останавливает корпус за один кадр, а `smoothstep` стартует с нулевой скорости: при входе в окно на пределе 1 м/с это ступенька в 1 м/с за кадр — 60 м/с² на 60 fps, выше порога гравиплит. Стоящего в этот кадр уронило бы; сидящего — нет, а в момент `F` на борту сидит только пилот. Доводка с учётом начальной скорости (кубический Эрмит вместо `smoothstep`) — правка одной функции в `StarForgeCore`, и она понадобится, когда на борту появится стоящий пассажир; до тех пор шов записан в бланк.
- **Строка `dock` называет, где узел, а не только как далеко.** У каюты нет окон, а станция снаружи не освещена: пилот не видит, куда лететь. К расстоянию и двум углам добавлена тройка `at (x y z) ship` — вектор от корабельного узла к станционному в осях корабля, в метрах. Без неё ручная точка проверки 6 невыполнима: вернуться в трёхметровое окно по одному расстоянию нельзя, а лишний вектор в строке журнала ничего не стоит.

---

## Структура файлов

```
Source/StarForgeCore/
  Public/
    DockingMath.h                    создаётся: FDockingLimits, FDockingApproach, EDockingRefusal,
                                     ComputeDockingApproach, CanDock, TargetHullPose, CapturePose
  Private/
    DockingMath.cpp                  создаётся
    Tests/DockingMathTest.cpp        создаётся: 9 тестов

Source/StarForge/
  Public/Ship/
    SFDockingPortComponent.h         создаётся: сокет узла
    SFDockingComponent.h             создаётся: состояния, окно, захват, замок двигателей
    SFInteriorVolumeComponent.h      создаётся: объём интерьера, перепривязка при входе
    SFTestStation.h                  создаётся: станция
    SFTestShip.h                     правится: люк, узел, объём, компонент стыковки, GetDocking
    SFThrusterArrayComponent.h       правится: SetLocked / IsLocked
  Private/Ship/
    SFDockingPortComponent.cpp       создаётся
    SFDockingComponent.cpp           создаётся: переменная sf.Dock объявлена здесь
    SFInteriorVolumeComponent.cpp    создаётся
    SFTestStation.cpp                создаётся
    SFTestShip.cpp                   правится: три панели вместо WallYPos, узел, строка Dock в журнале
    SFThrusterArrayComponent.cpp     правится: замок в ApplyDemand
    SFPilotStationComponent.cpp      правится: стоит в стороне при замке
  Public/Player/
    SFCharacter.h                    правится: BindFrame, PollDockInput
    SFShipboardMovementComponent.h   правится: прибор передачи, перекрёстная проверка
  Private/Player/
    SFCharacter.cpp                  правится: привязка по объёму, клавиша F, переменная sf.BindFrame
    SFShipboardMovementComponent.cpp правится
  Private/Debug/
    SFFrameDriftHUD.cpp              правится: строки frame и dock
  Public/SFLoopGameMode.h            создаётся
  Private/SFLoopGameMode.cpp         создаётся

Content/Maps/L_Loop.umap             создаётся руками в редакторе (задача 4, шаг 3)
CLAUDE.md                            правится: sf.Dock, sf.BindFrame, рецепт запуска петли
docs/superpowers/notes/
  2026-09-10-checkpoint-6-runbook.md создаётся
  2026-09-10-checkpoint-6-results.md создаётся
```

Разделение: `DockingMath` не знает, что такое актор; `USFDockingPortComponent` не знает, что такое стыковка, — он сокет с осями; `USFDockingComponent` не знает, кто нажал клавишу; `USFThrusterArrayComponent` не знает, почему заперт; `USFPilotStationComponent` не знает про стыковку — он знает, что двигатели заперты; `USFInteriorVolumeComponent` не знает, кто в него вошёл, кроме того, что это `ASFCharacter`.

---

## Геометрия, одной таблицей

Все числа — сантиметры игрового модуля. Выведены из полуразмеров в конструкторах, а не вписаны: подвинув стену, подвинешь люк и узел вместе с ней.

| Что | Где |
|---|---|
| Каюта корабля | куб, полуразмер 400, панели толщиной 20; пол сверху на `−390` |
| Люк корабля | проём в стене `+Y`: X `±100` (200 см), Z от `−390` до `−140` (250 см) |
| Панели стены `+Y` | две боковые X `[±100 … ±400]` на полную высоту, одна верхняя X `±100`, Z `[−140 … +400]` |
| Узел корабля | hull-local `(0, 400, −265)`, `SetRelativeRotation(FRotator(0, 90, 0))`: `+X` = корабельный `+Y` наружу, `+Z` = `+Z` |
| Коробка станции | полуразмеры `(600, 400, 400)`, те же панели; пол сверху на `−390` |
| Люк станции | проём в стене `−Y`, зеркальный: X `±100`, Z от `−390` до `−140` |
| Тоннель шлюза | station-local Y `[−600 … −400]`, проём 200 × 250, пол/потолок/две стены толщиной 20 |
| Узел станции | station-local `(0, −600, −265)`, `SetRelativeRotation(FRotator(0, -90, 0))`: `+X` = станционный `−Y` наружу |
| Пристыкованный корабль | центр в station-local `(0, −1000, 0)` без поворота; **выводится `TargetHullPose`**, константа живёт только в тесте |
| Объём корабля | полуразмеры `(390, 390, 390)`, центр `(0, 0, 0)` — ровно до плоскости люка по Y |
| Объём станции | полуразмеры `(590, 500, 390)`, центр `(0, −100, 0)` — коробка Y `[−400 … +400]` плюс тоннель Y `[−600 … −400]` |
| Зазор между объёмами | корабельный кончается на station-local `−610`, станционный начинается на `−600`: 10 см толщины стены, где действует последняя привязка |

Проверка позы пристыкованного корабля вручную: узел станции на `−600`, полуразмер каюты 400, узел корабля на его `+400` ⇒ центр корпуса на `−600 − 400 = −1000`, и узел корабля оказывается на `−1000 + 400 = −600` — там же, где станционный. Оси: корабельный `+X` = `+Y` мира, станционный `+X` = `−Y` мира, встречно. Обе `+Z` совпадают.

---

## Как проверяется

| Задача | Чем |
|---|---|
| 1. Математика стыковки | 9 головных тестов; красный шаг — настоящая сборка с ожидаемой ошибкой |
| 2. Узлы, люк, станция | Сборка; headless на `L_FrameSpike`: строка `Docking port on …`, `Turret mount at …` на месте, **65** головных тестов |
| 3. Компонент стыковки, замок, `sf.Dock` | Headless на `L_FrameSpike`: ровно одна `Dock: refused -- no station`, полёт по-прежнему 200.0 м/с |
| 4. Режим петли, карта, пристыкованный старт | Headless на `L_Loop`, три прогона **на 60 и на 400 fps** |
| 5. Объёмы, передача, перекрёстная проверка | Headless на `L_Loop`, три прогона на двух частотах, два из них — отрицательные контроли |
| 6. Runbook и бланк | Документ; затем точка проверки 6 руками |

Сборка:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Головные тесты:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Прогон на стенде `L_FrameSpike` (карта по умолчанию, позиционный аргумент не нужен):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0" -log
```

Прогон петли — **карта первым позиционным аргументом**, карта по умолчанию не меняется:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1" -log
```

**Что нельзя проверить без рук** — и что поэтому уходит в runbook задачи 6: рывок на люке (у headless нет ног, и через люк ходят ногами); стоит ли интерьер станции неподвижно при доводке; согласие картинки с числами на панели `dock`; отлёт с разворотом и возврат в окно; ощущение перехода вообще.

---

## Что проверено в исходниках движка

Каждый вызов движка, на который опирается стыковка, прочитан в установленном 5.8. Пути — от `C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/`.

| Факт | Где |
|---|---|
| `virtual bool AttachToComponent(USceneComponent* InParent, const FAttachmentTransformRules&, FName InSocketName = NAME_None)` | `Classes/Components/SceneComponent.h:752` |
| `virtual void DetachFromComponent(const FDetachmentTransformRules&)` | там же, `:786` |
| `FAttachmentTransformRules::KeepWorldTransform` — статический пресет | `Classes/Engine/EngineTypes.h:79` |
| `FDetachmentTransformRules::KeepWorldTransform` — статический пресет | там же, `:126` |
| **`SetSimulatePhysics(true)` сама отцепляет прицепленный компонент, а `SetSimulatePhysics(false)` его обратно не прицепляет** — прямым текстом в документации функции | `Classes/Components/PrimitiveComponent.h:1648–1656`; объявление `:1661` |
| `virtual void SetPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent = false, FName BoneName = NAME_None)` | там же, `:1825` |
| `virtual void SetPhysicsAngularVelocityInRadians(FVector NewAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None)` | там же, `:1860` |
| `FComponentBeginOverlapSignature` — шесть параметров: `UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult` | там же, `:276`; поле `OnComponentBeginOverlap` `:1468` |
| `void GetOverlappingComponents(TArray<UPrimitiveComponent*>& OutOverlappingComponents) const` | там же, `:1380` |
| `AWorldSettings::DefaultGameMode` (`GameMode Override` в World Settings) — переопределение режима картой | `Classes/GameFramework/WorldSettings.h:634` |
| `FBodyInstance::SleepFamily` — `UPROPERTY`, переживает пересоздание состояния физики; **что переживает `SetSimulatePhysics(false/true)` — проверяется прогоном, а не выводится** | `Classes/PhysicsEngine/BodyInstance.h:410` |
| `FBodyInstance::CustomSleepThresholdMultiplier` — «если `SleepFamily` = `Custom`, умножить естественный порог сна на это» | там же, `:696` |
| **`MaybeUpdateBasedMovement`: у НЕсимулируемой опоры `UpdateBasedMovement` зовётся один раз за кадр, из обычного тика, и движку добавляется тик-зависимость персонажа от опоры (`AddTickDependency`, 2492); у симулируемой вызов откладывается в post-physics и при `p.BasedMovementMode 2` (по умолчанию) идёт ещё и из обычного тика — то есть дважды** | `Private/Components/CharacterMovementComponent.cpp:2476–2520` |
| `GetMovementBaseObject() const` возвращает `UObject*` | `Classes/GameFramework/CharacterMovementComponent.h:1409` |
| `FRotationMatrix::MakeFromXZ(X, Z)` — X остаётся, Z минимально доворачивается до ортогональности, Y вычисляется | `Runtime/Core/Public/Math/RotationMatrix.h:54`, реализация `:137` |
| `FRotator(Pitch, Yaw, Roll)` — именно в этом порядке | `Runtime/Core/Public/Math/Rotator.h:101` |
| `MovementBaseUtility::UseRelativeLocation` истинна только для **подвижной** опоры (`IsDynamicBase`: `Mobility == Movable`), и `UpdateBasedMovement` зовётся только при ней — панели станции поэтому `Movable` | `Classes/GameFramework/Character.h:160`, `:218`; `Private/Character.cpp:593`, `:814`; вызов `Private/Components/CharacterMovementComponent.cpp:2481` |
| `FActorSpawnParameters::ESpawnActorNameMode::Requested` — занятое имя заменяется уникальным, тогда как умолчание `Required_Fatal` роняет процесс | `Classes/Engine/World.h:496–512` |
| `FTransform::InverseTransformVectorNoScale(const FVector&)` — вектор из мира в локальные оси, без переноса и без масштаба | `Runtime/Core/Public/Math/TransformNonVectorized.h:546` (векторизованный вариант — то же имя) |

Предпоследний пункт — самый важный в таблице. Он означает, что расписание переноса **меняется при отстыковке**, и защёлки «раз в кадр» в переносе, в приборе увода, в опоре и в перекрёстной проверке — это то, что делает оба расписания эквивалентными. §8 требует, чтобы это было проверено, а не выведено; проверка — задача 4, прогон (а).

Про `A * B` у `FTransform`: **сначала A, потом B**, то есть `ChildLocal * ParentWorld` — мир ребёнка. Это ломается молча, и потому в задаче 1 есть тест, который стыкует узлы и проверяет, что они совпали.

---

### Task 1: Математика стыковки

Окно сближения и поза мест — в `StarForgeCore`, под тестами. Ошибка здесь не падает: корабль отказывается стыковаться там, где панель говорит «можно», либо приезжает в позу, развёрнутую на 180°, и это ищут в компоненте, который невиновен.

**Files:**
- Create: `Source/StarForgeCore/Public/DockingMath.h`
- Create: `Source/StarForgeCore/Private/DockingMath.cpp`
- Create: `Source/StarForgeCore/Private/Tests/DockingMathTest.cpp`

**Interfaces:**
- Consumes: `FTransform`, `FQuat`, `FVector`, `FMath` из `Core`.
- Produces, всё в `namespace StarForge::Docking`:
  - `struct FDockingLimits { double DistanceMetres = 3.0; double SpeedMetresPerSec = 1.0; double ForwardAngleRad = 10 deg; double UpAngleRad = 10 deg; }`
  - `struct FDockingApproach { double Distance = 0.0; double SpeedMetresPerSec = 0.0; double ForwardAngleRad = 0.0; double UpAngleRad = 0.0; }`
  - `enum class EDockingRefusal : uint8 { None, Distance, Speed, Forward, Up }`
  - `FDockingApproach ComputeDockingApproach(const FTransform& ShipPortWorld, const FTransform& StationPortWorld, double RelativeSpeedMetresPerSec)`
  - `EDockingRefusal CanDock(const FDockingApproach& ApproachSI, const FDockingLimits& Limits)`
  - `FTransform TargetHullPose(const FTransform& StationPortWorld, const FTransform& ShipPortLocalToHull)`
  - `FTransform CapturePose(const FTransform& Start, const FTransform& Target, double Alpha)`

**Единицы здесь разные у разных полей, и это сказано в самих именах.** `FDockingApproach::Distance` — в тех единицах, в которых пришли трансформы: игра передаёт сантиметры, как и математике системы отсчёта. `SpeedMetresPerSec` — СИ, потому что скорость приходит уже из СИ-прибора и её незачем гонять туда-обратно. `CanDock` берёт **уже переведённый** approach — конвертирует вызывающий, ровно в одном месте.

**Порядок множителей — то, что ломается молча.** Мировая поза узла есть `ShipPortLocalToHull * HullWorld`. Требуемая поза узла — станционный узел, развёрнутый на 180° вокруг своего `Z`: `FTransform(FQuat(FVector::UpVector, PI)) * StationPortWorld`. Отсюда `HullWorld = ShipPortLocalToHull.Inverse() * FlippedStationPort`, и любая перестановка сомножителей даёт правдоподобную позу, отличающуюся разворотом или смещением на длину плеча. Тест `TargetHullPoseMatesThePorts` замыкает круг: после применения формулы `ComputeDockingApproach` двух узлов обязан вернуть нули.

- [ ] **Step 1: Создать ветку**

```bash
git checkout -b feat/docking-station main
```

- [ ] **Step 2: Написать падающие тесты**

`Source/StarForgeCore/Private/Tests/DockingMathTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "DockingMath.h"

namespace
{
	constexpr double Deg = UE_DOUBLE_PI / 180.0;

	/** The ship's authored window: 3 m, 1 m/s, 10 degrees on each of the two axes. */
	StarForge::Docking::FDockingLimits MakeLimits()
	{
		StarForge::Docking::FDockingLimits Limits;
		Limits.DistanceMetres = 3.0;
		Limits.SpeedMetresPerSec = 1.0;
		Limits.ForwardAngleRad = 10.0 * Deg;
		Limits.UpAngleRad = 10.0 * Deg;
		return Limits;
	}

	/** An approach comfortably inside that window, for the refusal tests to spoil one field of. */
	StarForge::Docking::FDockingApproach MakeGoodApproach()
	{
		StarForge::Docking::FDockingApproach Approach;
		Approach.Distance = 2.0;
		Approach.SpeedMetresPerSec = 0.3;
		Approach.ForwardAngleRad = 4.0 * Deg;
		Approach.UpAngleRad = 2.0 * Deg;
		return Approach;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingDistanceBetweenPortsTest,
	"StarForge.Core.Docking.DistanceBetweenPorts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingDistanceBetweenPortsTest::RunTest(const FString& Parameters)
{
	// Two ports three metres apart along X, facing each other: the ship's port at the
	// origin looking along +X, the station's at 300 looking back along -X. Distance is
	// between the port origins and nothing else -- not between hulls, which are metres
	// of geometry away in both directions.
	const FTransform ShipPort(FQuat::Identity, FVector::ZeroVector);
	const FTransform StationPort(FRotator(0.0, 180.0, 0.0).Quaternion(), FVector(300.0, 0.0, 0.0));

	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(ShipPort, StationPort, 0.0);

	TestTrue(TEXT("distance is between the port origins"),
		FMath::IsNearlyEqual(Approach.Distance, 300.0, 1e-9));
	TestTrue(TEXT("facing each other is zero forward angle"),
		FMath::IsNearlyZero(Approach.ForwardAngleRad, 1e-6));
	TestTrue(TEXT("and zero up angle"), FMath::IsNearlyZero(Approach.UpAngleRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingSpeedIsCarriedThroughTest,
	"StarForge.Core.Docking.SpeedIsCarriedThrough",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingSpeedIsCarriedThroughTest::RunTest(const FString& Parameters)
{
	// The relative speed is carried, not computed. This function sees two poses at one
	// instant and cannot differentiate anything; the caller has a velocity instrument
	// and passes what it reads, in SI. Any arithmetic here would be a second, worse
	// answer to a question already answered.
	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(
			FTransform::Identity, FTransform(FVector(100.0, 0.0, 0.0)), 0.75);

	TestTrue(TEXT("the speed passed in is the speed reported"),
		FMath::IsNearlyEqual(Approach.SpeedMetresPerSec, 0.75, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingForwardAngleIsTheMisalignmentTest,
	"StarForge.Core.Docking.ForwardAngleIsTheMisalignment",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingForwardAngleIsTheMisalignmentTest::RunTest(const FString& Parameters)
{
	// The station's port is turned 15 degrees past facing the ship: 180 + 15. The angle
	// under test is between the ship's +X and the station's -X, so a station rotated
	// fifteen degrees off dead-ahead reads as fifteen, not as a hundred and ninety-five.
	const FTransform ShipPort(FQuat::Identity, FVector::ZeroVector);
	const FTransform StationPort(FRotator(0.0, 195.0, 0.0).Quaternion(), FVector(300.0, 0.0, 0.0));

	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(ShipPort, StationPort, 0.0);

	TestTrue(TEXT("fifteen degrees off dead-ahead reads as fifteen"),
		FMath::IsNearlyEqual(Approach.ForwardAngleRad, 15.0 * Deg, 1e-6));

	// A yaw cannot tilt Z, so the second axis has to stay clean. Two angles that move
	// together would be one angle wearing two names.
	TestTrue(TEXT("and a yaw leaves the up angle alone"),
		FMath::IsNearlyZero(Approach.UpAngleRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingUpAngleIsTheRollTest,
	"StarForge.Core.Docking.UpAngleIsTheRoll",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingUpAngleIsTheRollTest::RunTest(const FString& Parameters)
{
	// Built from the two axes it is meant to have rather than from a rotator, so the
	// fixture states the geometry instead of trusting a composition order -- and the
	// first two assertions check the fixture before the third checks the function.
	const double Tilt = 20.0 * Deg;
	const FVector StationForward(-1.0, 0.0, 0.0);
	const FVector StationUp(0.0, FMath::Sin(Tilt), FMath::Cos(Tilt));

	const FTransform StationPort(
		FRotationMatrix::MakeFromXZ(StationForward, StationUp).ToQuat(), FVector(300.0, 0.0, 0.0));

	TestTrue(TEXT("the fixture really faces back at the ship"),
		StationPort.GetUnitAxis(EAxis::X).Equals(StationForward, 1e-6));
	TestTrue(TEXT("and really carries the twenty-degree tilt"),
		StationPort.GetUnitAxis(EAxis::Z).Equals(StationUp, 1e-6));

	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(FTransform::Identity, StationPort, 0.0);

	TestTrue(TEXT("a deck rolled twenty degrees reads as twenty"),
		FMath::IsNearlyEqual(Approach.UpAngleRad, 20.0 * Deg, 1e-6));
	TestTrue(TEXT("and the forward axis is still dead-on"),
		FMath::IsNearlyZero(Approach.ForwardAngleRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingCanDockInsideTheWindowTest,
	"StarForge.Core.Docking.CanDockInsideTheWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingCanDockInsideTheWindowTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("an approach inside every limit is not refused"),
		StarForge::Docking::CanDock(MakeGoodApproach(), MakeLimits())
			== StarForge::Docking::EDockingRefusal::None);

	// Exactly on a limit is inside it. The window is a closed interval, so a pilot who
	// has flown to precisely three metres is not told to try again from three metres.
	StarForge::Docking::FDockingApproach OnTheEdge = MakeGoodApproach();
	OnTheEdge.Distance = 3.0;
	OnTheEdge.SpeedMetresPerSec = 1.0;
	OnTheEdge.ForwardAngleRad = 10.0 * Deg;
	OnTheEdge.UpAngleRad = 10.0 * Deg;

	TestTrue(TEXT("an approach exactly on every limit is still inside"),
		StarForge::Docking::CanDock(OnTheEdge, MakeLimits())
			== StarForge::Docking::EDockingRefusal::None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingRefusesEachConditionOutsideItTest,
	"StarForge.Core.Docking.RefusesEachConditionOutsideIt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingRefusesEachConditionOutsideItTest::RunTest(const FString& Parameters)
{
	const StarForge::Docking::FDockingLimits Limits = MakeLimits();

	// One field spoiled at a time. A refusal that named the wrong condition would send a
	// pilot to fix a thing that was never wrong, and the readout would agree with it.
	StarForge::Docking::FDockingApproach TooFar = MakeGoodApproach();
	TooFar.Distance = 3.5;
	TestTrue(TEXT("too far reads as distance"),
		StarForge::Docking::CanDock(TooFar, Limits) == StarForge::Docking::EDockingRefusal::Distance);

	StarForge::Docking::FDockingApproach TooFast = MakeGoodApproach();
	TooFast.SpeedMetresPerSec = 2.7;
	TestTrue(TEXT("too fast reads as speed"),
		StarForge::Docking::CanDock(TooFast, Limits) == StarForge::Docking::EDockingRefusal::Speed);

	StarForge::Docking::FDockingApproach Skewed = MakeGoodApproach();
	Skewed.ForwardAngleRad = 15.0 * Deg;
	TestTrue(TEXT("nose off reads as forward"),
		StarForge::Docking::CanDock(Skewed, Limits) == StarForge::Docking::EDockingRefusal::Forward);

	StarForge::Docking::FDockingApproach Rolled = MakeGoodApproach();
	Rolled.UpAngleRad = 15.0 * Deg;
	TestTrue(TEXT("deck rolled reads as up"),
		StarForge::Docking::CanDock(Rolled, Limits) == StarForge::Docking::EDockingRefusal::Up);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingRefusalOrderIsTheListedOrderTest,
	"StarForge.Core.Docking.RefusalOrderIsTheListedOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingRefusalOrderIsTheListedOrderTest::RunTest(const FString& Parameters)
{
	const StarForge::Docking::FDockingLimits Limits = MakeLimits();

	// With two conditions broken the answer is the FIRST of them in the listed order,
	// and the order is deliberate: distance, speed, forward, up. It is the order in
	// which a pilot can act on the answer. Told "your nose is fifteen degrees off" at
	// forty metres, he corrects an alignment that will be wrong again by the time he
	// arrives; told "too far", he closes, and the alignment is then a real question.
	StarForge::Docking::FDockingApproach FarAndFast = MakeGoodApproach();
	FarAndFast.Distance = 3.5;
	FarAndFast.SpeedMetresPerSec = 2.7;
	TestTrue(TEXT("far and fast reads as distance"),
		StarForge::Docking::CanDock(FarAndFast, Limits) == StarForge::Docking::EDockingRefusal::Distance);

	StarForge::Docking::FDockingApproach FastAndSkewed = MakeGoodApproach();
	FastAndSkewed.SpeedMetresPerSec = 2.7;
	FastAndSkewed.ForwardAngleRad = 15.0 * Deg;
	TestTrue(TEXT("fast and skewed reads as speed"),
		StarForge::Docking::CanDock(FastAndSkewed, Limits) == StarForge::Docking::EDockingRefusal::Speed);

	StarForge::Docking::FDockingApproach SkewedAndRolled = MakeGoodApproach();
	SkewedAndRolled.ForwardAngleRad = 15.0 * Deg;
	SkewedAndRolled.UpAngleRad = 15.0 * Deg;
	TestTrue(TEXT("skewed and rolled reads as forward"),
		StarForge::Docking::CanDock(SkewedAndRolled, Limits) == StarForge::Docking::EDockingRefusal::Forward);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingTargetHullPoseMatesThePortsTest,
	"StarForge.Core.Docking.TargetHullPoseMatesThePorts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingTargetHullPoseMatesThePortsTest::RunTest(const FString& Parameters)
{
	// The rig's real geometry, in the rig's centimetres. The station's port is 600 aft
	// along its own -Y and 265 below its pivot, facing out along -Y; the ship's port is
	// 400 forward along its own +Y at the same height, facing out along +Y. Neither port
	// sits on its hull's pivot, and that offset is precisely what a wrong multiplication
	// order gets wrong while still looking plausible.
	const FTransform StationPortWorld(
		FRotator(0.0, -90.0, 0.0).Quaternion(), FVector(0.0, -600.0, -265.0));
	const FTransform ShipPortLocalToHull(
		FRotator(0.0, 90.0, 0.0).Quaternion(), FVector(0.0, 400.0, -265.0));

	const FTransform HullWorld =
		StarForge::Docking::TargetHullPose(StationPortWorld, ShipPortLocalToHull);

	// Where the rig's hull is expected to end up. This constant lives here and nowhere
	// else: the component derives the pose from the formula, and a hard-coded pose in
	// the game module would stop tracking a wall that moved.
	TestTrue(TEXT("the hull lands a thousand centimetres aft of the station's pivot"),
		HullWorld.GetLocation().Equals(FVector(0.0, -1000.0, 0.0), 1e-6));
	TestTrue(TEXT("and unrotated with respect to it"),
		HullWorld.GetRotation().Rotator().IsNearlyZero(1e-4f));

	// The round trip: put the port back on that hull and the two ports must mate.
	const FTransform ShipPortWorld = ShipPortLocalToHull * HullWorld;
	const StarForge::Docking::FDockingApproach Mated =
		StarForge::Docking::ComputeDockingApproach(ShipPortWorld, StationPortWorld, 0.0);

	TestTrue(TEXT("the port origins coincide"), Mated.Distance < 1e-6);
	TestTrue(TEXT("the forward axes oppose"), Mated.ForwardAngleRad < 1e-6);
	TestTrue(TEXT("and the decks agree"), Mated.UpAngleRad < 1e-6);

	// Again with a station standing somewhere awkward, so nothing above can be passing
	// because the fixture happened to sit on the world axes.
	const FTransform TiltedStationPort(
		FRotationMatrix::MakeFromXZ(
			FVector(0.3, -0.5, 0.2).GetSafeNormal(), FVector(0.1, 0.4, 0.9)).ToQuat(),
		FVector(1234.0, -567.0, 890.0));

	const FTransform TiltedHull =
		StarForge::Docking::TargetHullPose(TiltedStationPort, ShipPortLocalToHull);
	const StarForge::Docking::FDockingApproach TiltedMated =
		StarForge::Docking::ComputeDockingApproach(
			ShipPortLocalToHull * TiltedHull, TiltedStationPort, 0.0);

	TestTrue(TEXT("origins coincide off the world axes too"), TiltedMated.Distance < 1e-6);
	TestTrue(TEXT("forward axes oppose off the world axes too"), TiltedMated.ForwardAngleRad < 1e-6);
	TestTrue(TEXT("decks agree off the world axes too"), TiltedMated.UpAngleRad < 1e-6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingCapturePoseIsExactAtItsEndsTest,
	"StarForge.Core.Docking.CapturePoseIsExactAtItsEnds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingCapturePoseIsExactAtItsEndsTest::RunTest(const FString& Parameters)
{
	const FTransform Start(FRotator(0.0, 20.0, 0.0).Quaternion(), FVector(0.0, 0.0, 0.0));
	const FTransform Target(FRotator(0.0, 40.0, 0.0).Quaternion(), FVector(200.0, 0.0, 0.0));

	// Exact, not nearly. The last frame of a capture is the pose the attachment freezes
	// forever, and a hull left a millimetre short is a hull whose port never quite mates
	// -- with nothing to correct it afterwards, because attaching is the end of the
	// motion. Interpolating to alpha 1.0 does not give that back in floating point, so
	// the ends are returned rather than computed.
	TestTrue(TEXT("alpha zero is exactly the start"),
		StarForge::Docking::CapturePose(Start, Target, 0.0).Equals(Start, 0.0));
	TestTrue(TEXT("alpha one is exactly the target"),
		StarForge::Docking::CapturePose(Start, Target, 1.0).Equals(Target, 0.0));

	// And out of range clamps rather than extrapolating: a frame long enough to overshoot
	// the capture must not fly the hull past the station.
	TestTrue(TEXT("past one is still exactly the target"),
		StarForge::Docking::CapturePose(Start, Target, 1.7).Equals(Target, 0.0));
	TestTrue(TEXT("before zero is still exactly the start"),
		StarForge::Docking::CapturePose(Start, Target, -0.4).Equals(Start, 0.0));

	const FTransform Half = StarForge::Docking::CapturePose(Start, Target, 0.5);
	TestTrue(TEXT("half way is half way in position"),
		Half.GetLocation().Equals(FVector(100.0, 0.0, 0.0), 1e-6));
	TestTrue(TEXT("and half way in rotation"),
		FMath::IsNearlyEqual(Half.GetRotation().Rotator().Yaw, 30.0, 1e-3));

	return true;
}
```

- [ ] **Step 3: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается `fatal error C1083: Cannot open include file: 'DockingMath.h'`. **Это шаг, а не формальность:** тесты, между которыми и реализацией не было настоящей сборки, проходят с первого раза, и их годность потом приходится доказывать, ломая код обратно.

- [ ] **Step 4: Написать заголовок**

`Source/StarForgeCore/Public/DockingMath.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

/**
 * Docking geometry: is the approach inside the window, and where does the hull have to
 * be for the two ports to mate.
 *
 * Length is unit-agnostic and follows whatever the caller passes, the same convention
 * the frame maths uses; the game passes centimetres. Angles are radians. The two fields
 * that name a unit -- DistanceMetres and SpeedMetresPerSec -- are SI, and the crossing
 * happens in the caller, once, as it does everywhere else in this module.
 *
 * A docking port is a socket whose +X points out of the hull and whose +Z is the deck's
 * up. Two ports mate when their origins coincide, their +X oppose and their +Z agree.
 * Three axes, not one: that is what makes the two decks line up, so the gravity
 * direction a person walks through in the airlock changes only by the leftover
 * tolerance (spec 8).
 */
namespace StarForge::Docking
{
	/**
	 * The approach window. Ship data, not constants: spec 8 gives 3 m, 1 m/s, 10 and 10
	 * degrees as starting tolerances, and the component authors them as properties.
	 */
	struct STARFORGECORE_API FDockingLimits
	{
		double DistanceMetres = 3.0;
		double SpeedMetresPerSec = 1.0;
		double ForwardAngleRad = 10.0 * UE_DOUBLE_PI / 180.0;
		double UpAngleRad = 10.0 * UE_DOUBLE_PI / 180.0;
	};

	/** What the approach is, measured. */
	struct STARFORGECORE_API FDockingApproach
	{
		/**
		 * Between the two port origins, in the length unit the transforms came in. The
		 * caller converts to metres before handing this to CanDock; the field is not
		 * named for a unit precisely because it does not have one until then.
		 */
		double Distance = 0.0;

		/**
		 * Carried, not computed. Two poses at one instant cannot say how fast they are
		 * closing; the caller has a velocity instrument and passes what it reads.
		 */
		double SpeedMetresPerSec = 0.0;

		/** Between the ship port's +X and the station port's -X. Zero is nose-on. */
		double ForwardAngleRad = 0.0;

		/** Between the two +Z. Zero is decks parallel. */
		double UpAngleRad = 0.0;
	};

	/**
	 * Which condition refused, or None.
	 *
	 * A bool would be the whole answer thrown away: a pilot refused without being told
	 * why corrects the wrong thing, and the readout that shows him a bool agrees with
	 * him. Same reasoning that gave the turret three states instead of an angle.
	 */
	enum class EDockingRefusal : uint8
	{
		None,
		Distance,
		Speed,
		Forward,
		Up
	};

	STARFORGECORE_API FDockingApproach ComputeDockingApproach(
		const FTransform& ShipPortWorld,
		const FTransform& StationPortWorld,
		double RelativeSpeedMetresPerSec);

	/**
	 * The first condition of ApproachSI that falls outside Limits, in the order
	 * distance, speed, forward, up -- the order a pilot can act on. Told his nose is
	 * fifteen degrees off at forty metres he corrects an alignment that will be wrong
	 * again on arrival; told he is too far, he closes, and the alignment then becomes a
	 * real question.
	 *
	 * Distance in ApproachSI must already be metres. The name says so because this is
	 * the one function in the file that would be silently wrong with centimetres in it:
	 * every approach would pass the angle checks and fail the distance one by a factor
	 * of a hundred, which reads exactly like a pilot who is simply too far away.
	 */
	STARFORGECORE_API EDockingRefusal CanDock(const FDockingApproach& ApproachSI, const FDockingLimits& Limits);

	/**
	 * Where the hull has to be for its port to mate with the station's.
	 *
	 * ShipPortLocalToHull is the port's transform relative to the hull, so the identity
	 * this inverts is PortWorld = ShipPortLocalToHull * HullWorld -- in Unreal's order,
	 * where A * B means A first and then B, and ChildLocal * ParentWorld is the child's
	 * world. Swap the two and the result is still a transform, still plausible, and
	 * wrong by a rotation and the length of the port's offset from the pivot. The test
	 * that mates the ports afterwards is what catches it.
	 */
	STARFORGECORE_API FTransform TargetHullPose(
		const FTransform& StationPortWorld, const FTransform& ShipPortLocalToHull);

	/**
	 * A pose along the run-in: 0 is Start, 1 is Target, between is linear in position
	 * and slerped in rotation.
	 *
	 * Alpha is clamped, and both ends are returned rather than interpolated to. The last
	 * frame of a capture is the pose the attachment freezes for as long as the ship stays
	 * docked, and nothing corrects it afterwards, so a hull left a float's width short
	 * would stay short.
	 *
	 * How alpha runs with time is the caller's business and deliberately not this
	 * function's: the component eases it, because a linear run-in starts and stops
	 * instantly and a frame that differentiates position twice reads that as a very
	 * large acceleration.
	 */
	STARFORGECORE_API FTransform CapturePose(const FTransform& Start, const FTransform& Target, double Alpha);
}
```

- [ ] **Step 5: Написать реализацию**

`Source/StarForgeCore/Private/DockingMath.cpp`:

```cpp
#include "DockingMath.h"

namespace StarForge::Docking
{
	namespace
	{
		/**
		 * The unsigned angle between two unit vectors.
		 *
		 * The clamp is not defensive decoration. A dot product of two normalised vectors
		 * comes back at 1.0000000000000002 often enough, and Acos of that is NaN -- which
		 * propagates into the approach, compares false against every limit, and presents
		 * as a dock command that refuses with no reason a person can see.
		 */
		double AngleBetween(const FVector& A, const FVector& B)
		{
			return FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0));
		}
	}

	FDockingApproach ComputeDockingApproach(
		const FTransform& ShipPortWorld,
		const FTransform& StationPortWorld,
		double RelativeSpeedMetresPerSec)
	{
		FDockingApproach Approach;

		Approach.Distance = FVector::Dist(ShipPortWorld.GetLocation(), StationPortWorld.GetLocation());
		Approach.SpeedMetresPerSec = RelativeSpeedMetresPerSec;

		// Mated means the two +X oppose, so the angle is taken against the station's
		// NEGATED forward. Measured against its forward instead, a perfect approach would
		// read 180 degrees and a ship reversing into the station at speed would read zero.
		Approach.ForwardAngleRad = AngleBetween(
			ShipPortWorld.GetUnitAxis(EAxis::X), -StationPortWorld.GetUnitAxis(EAxis::X));

		// The decks, on the other hand, agree rather than oppose: both +Z point away from
		// their own floor, and a person walking through the airlock has to keep his feet
		// on the same side of the world.
		Approach.UpAngleRad = AngleBetween(
			ShipPortWorld.GetUnitAxis(EAxis::Z), StationPortWorld.GetUnitAxis(EAxis::Z));

		return Approach;
	}

	EDockingRefusal CanDock(const FDockingApproach& ApproachSI, const FDockingLimits& Limits)
	{
		// Closed intervals throughout: exactly on a limit is inside it. A pilot who has
		// flown to precisely three metres should not be sent away from three metres.
		if (ApproachSI.Distance > Limits.DistanceMetres)
		{
			return EDockingRefusal::Distance;
		}
		if (ApproachSI.SpeedMetresPerSec > Limits.SpeedMetresPerSec)
		{
			return EDockingRefusal::Speed;
		}
		if (ApproachSI.ForwardAngleRad > Limits.ForwardAngleRad)
		{
			return EDockingRefusal::Forward;
		}
		if (ApproachSI.UpAngleRad > Limits.UpAngleRad)
		{
			return EDockingRefusal::Up;
		}

		return EDockingRefusal::None;
	}

	FTransform TargetHullPose(const FTransform& StationPortWorld, const FTransform& ShipPortLocalToHull)
	{
		// The pose the ship's port must take: the station's port turned half a turn about
		// its own Z. That sends the station's +X onto its -X and leaves its +Z alone,
		// which is the mating condition written as a transform. The origin is untouched,
		// because a rotation with no translation composed on the left contributes none.
		const FTransform Flip(FQuat(FVector::UpVector, UE_DOUBLE_PI));
		const FTransform MatedPortWorld = Flip * StationPortWorld;

		// PortWorld = ShipPortLocalToHull * HullWorld, so HullWorld is the port's local
		// transform undone from the left. Written this way round and no other; see the
		// header.
		return ShipPortLocalToHull.Inverse() * MatedPortWorld;
	}

	FTransform CapturePose(const FTransform& Start, const FTransform& Target, double Alpha)
	{
		if (Alpha <= 0.0)
		{
			return Start;
		}
		if (Alpha >= 1.0)
		{
			return Target;
		}

		FTransform Pose;
		Pose.SetLocation(FMath::Lerp(Start.GetLocation(), Target.GetLocation(), Alpha));
		Pose.SetRotation(FQuat::Slerp(Start.GetRotation(), Target.GetRotation(), Alpha).GetNormalized());

		// Scale comes from the start and is never interpolated. Nothing in this project
		// scales a hull, and a capture that quietly rescaled one would move every wall,
		// every seat and every spawn point with it.
		Pose.SetScale3D(Start.GetScale3D());

		return Pose;
	}
}
```

- [ ] **Step 6: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Считать в `Saved/Logs/StarForge.log`:

```bash
grep -c "Test Completed. Result={Success}" Saved/Logs/StarForge.log
grep -c "Test Completed. Result={Fail}" Saved/Logs/StarForge.log
```

Ожидается **65** и **0** (56 прежних плюс девять новых). Проверить, что зависимости не изменились: `git diff Source/StarForgeCore/StarForgeCore.Build.cs` пуст.

- [ ] **Step 7: Коммит**

```bash
git add Source/StarForgeCore/Public/DockingMath.h Source/StarForgeCore/Private/DockingMath.cpp Source/StarForgeCore/Private/Tests/DockingMathTest.cpp
git commit -F - <<'MSG'
feat(core): the docking window, and the pose that mates two ports

Spec 8 wants the approach window to be pure functions under test, for the reason the
turret's aiming maths is: the failure mode is a ship that refuses to dock where the
readout says it may, or arrives in a pose turned half a turn, and neither crashes nor
logs. Both are a minute's work to catch here and a week's in a component.

Alignment is judged on three axes rather than one distance. The two forward axes must
oppose and the two deck normals must agree, because that is what makes the decks line
up: a person walking through the airlock changes gravity direction by the leftover
tolerance and no more. A distance alone would let a ship dock upside down.

The refusal names which condition failed, in the order a pilot can act on -- distance,
speed, forward, up. Told his nose is off at forty metres he corrects an alignment that
will be wrong again on arrival; told he is too far, he closes first.

TargetHullPose inverts PortWorld = PortLocalToHull * HullWorld, and the multiplication
order is the thing that breaks silently: swapped, the result is still a transform and
still plausible, wrong by a rotation and the length of the port's offset from the
pivot. One test mates the ports afterwards and reads back three zeros, which no
permutation survives.

CapturePose returns its endpoints rather than interpolating to them. The last frame of
a capture is the pose the attachment freezes for as long as the ship stays docked, and
nothing corrects it afterwards.
MSG
```

---

### Task 2: Узлы, люк в корабле и станция

Два носителя вместо одного. Сокет-узел, проём в стене корабля, коробка станции с зеркальным проёмом и тоннелем.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFDockingPortComponent.h`
- Create: `Source/StarForge/Private/Ship/SFDockingPortComponent.cpp`
- Create: `Source/StarForge/Public/Ship/SFTestStation.h`
- Create: `Source/StarForge/Private/Ship/SFTestStation.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`

**Interfaces:**
- Consumes: `USFShipFrameComponent`, `UStaticMeshComponent`, `UPointLightComponent`, `StarForge::Units::CmToMetres`.
- Produces:
  - `USFDockingPortComponent : USceneComponent` с `FTransform GetPortTransform() const`.
  - `ASFTestStation : AActor` с `USFShipFrameComponent* GetFrameComponent() const`, `USFDockingPortComponent* GetDockingPort() const`.
  - `ASFTestShip::GetDockingPort() const -> USFDockingPortComponent*`.

**Почему станция — отдельный класс, а не корабль без двигателей.** Общий предок сделал бы каждую позднейшую правку одного корпуса правкой обоих, а у них общая форма, но не общая жизнь: у корабля есть физическое тело, посты, турель и профиль, у станции — ничего из этого и никогда не будет. Помощник `AddBoxPanel` дублируется сознательно: двадцать строк повтора дешевле, чем базовый класс, через который придётся протаскивать различия.

**Почему `Movable`, а не `Static`, и это несущее решение.** `MovementBaseUtility::UseRelativeLocation` истинна только для подвижной опоры (`IsDynamicBase` — мобильность `Movable`, `Character.cpp:593`), а `UpdateBasedMovement` зовётся только при ней (`CharacterMovementComponent.cpp:2481`). Панели `Static` означали бы, что человек на палубе станции не переносится вовсе, перекрёстная проверка на ней не работает, и `Carry NOT RUNNING` пишется раз в секунду про человека, которому ничто не грозит. Заодно один идиом на два корпуса — на одно различие между ними меньше. Неподвижной системой отсчёта станцию делает отсутствие физического тела и того, кто бы ей командовал, а не флаг мобильности.

- [ ] **Step 1: Написать узел**

`Source/StarForge/Public/Ship/SFDockingPortComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SFDockingPortComponent.generated.h"

/**
 * A docking socket, and nothing else.
 *
 * Its +X points out of the hull and its +Z is the deck's up. Two ports mate when their
 * origins coincide, their +X oppose and their +Z agree -- three axes, because that is
 * what makes the two decks line up and keeps the gravity direction a person walks
 * through in the airlock inside the leftover tolerance (spec 8).
 *
 * It knows nothing about docking. It does not move, latch, refuse or decide; it is a
 * place with an orientation, and USFDockingComponent is what reads it. The separation
 * is the same one that keeps USFTurretComponent from knowing who is aiming: the station
 * has a port and no docking component at all, because the station never docks with
 * anything -- the moving side owns the mechanism.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFDockingPortComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USFDockingPortComponent();

	virtual void BeginPlay() override;

	/** Where this port is in the world right now. */
	FTransform GetPortTransform() const { return GetComponentTransform(); }
};
```

`Source/StarForge/Private/Ship/SFDockingPortComponent.cpp`:

```cpp
#include "Ship/SFDockingPortComponent.h"

#include "GameFramework/Actor.h"
#include "StarForgeLog.h"

USFDockingPortComponent::USFDockingPortComponent()
{
	// Read by whoever needs it, on their tick. A socket has nothing to do on its own.
	PrimaryComponentTick.bCanEverTick = false;
}

void USFDockingPortComponent::BeginPlay()
{
	Super::BeginPlay();

	// Once at startup, like the turret mount does, so a run's log says what geometry it
	// was flown with. The axes are printed as vectors rather than as a rotator because
	// the mating condition is stated in axes, and a rotator has to be turned into axes
	// in a reader's head before it can be checked against that condition -- which is
	// exactly where a yaw of -90 and a yaw of +90 stop being distinguishable at a glance.
	const FQuat Relative = GetRelativeRotation().Quaternion();
	const FVector Out = Relative.GetAxisX();
	const FVector Up = Relative.GetAxisZ();
	const FVector Location = GetRelativeLocation();

	UE_LOG(LogStarForge, Display,
		TEXT("Docking port on %s at hull-local (%.0f %.0f %.0f) cm, out along (%.2f %.2f %.2f), deck up (%.2f %.2f %.2f)."),
		GetOwner() ? *GetOwner()->GetActorNameOrLabel() : TEXT("nothing"),
		Location.X, Location.Y, Location.Z,
		Out.X, Out.Y, Out.Z, Up.X, Up.Y, Up.Z);
}
```

- [ ] **Step 2: Прорезать люк в корабле и повесить узел**

В `Source/StarForge/Private/Ship/SFTestShip.cpp`, в безымянный `namespace` рядом с прочими константами, добавить:

```cpp
	/**
	 * The hatch in the +Y wall, in centimetres. Two hundred across and two hundred and
	 * fifty tall from the deck: wide enough for a capsule of radius 34 to walk through
	 * without brushing a jamb, and tall enough that a standing capsule (88 cm
	 * half-height, so 176 overall) has head-room instead of reading as a crawl space.
	 */
	constexpr double HatchHalfWidthCm = 100.0;
	constexpr double HatchHeightCm = 250.0;
```

Заменить одну строку `AddBoxPanel(TEXT("WallYPos"), ...)` на:

```cpp
	// The +Y wall is three panels with a hatch between them. Every figure is derived
	// from Half and the panel thickness, so moving a wall moves the hatch and the port
	// with it -- the same rule the turret mount and the seats already follow.
	//
	// Nothing is needed below the opening: the floor panel spans the whole footprint at
	// Z -410..-390 and the hatch starts at its top face.
	const double DeckTopZ = -Half + PanelThickness * CubeEdgeCm * 0.5;      // -390
	const double HatchTopZ = DeckTopZ + HatchHeightCm;                      // -140
	const double JambWidthCm = Half - HatchHalfWidthCm;                     //  300
	const double JambCentreXCm = (Half + HatchHalfWidthCm) * 0.5;           //  250
	const double LintelHeightCm = Half - HatchTopZ;                         //  540
	const double LintelCentreZCm = (HatchTopZ + Half) * 0.5;                //  130

	AddBoxPanel(TEXT("WallYPosJambXNeg"), FVector(-JambCentreXCm, Half, 0.0),
		FVector(JambWidthCm / CubeEdgeCm, PanelThickness, Span));
	AddBoxPanel(TEXT("WallYPosJambXPos"), FVector(JambCentreXCm, Half, 0.0),
		FVector(JambWidthCm / CubeEdgeCm, PanelThickness, Span));
	AddBoxPanel(TEXT("WallYPosLintel"), FVector(0.0, Half, LintelCentreZCm),
		FVector(HatchHalfWidthCm * 2.0 / CubeEdgeCm, PanelThickness, LintelHeightCm / CubeEdgeCm));

	// Centred in the hatch, on the wall's mid-plane: +X out along the ship's +Y, +Z the
	// deck's up. A yaw of 90 takes local +X onto +Y and leaves +Z alone.
	//
	// The cabin is no longer a closed box, and that is a real change to this rig, not
	// only to the loop. A standing occupant is bounded by the walls and by nothing else
	// -- falling friction limits only the component along the deck -- so on L_FrameSpike,
	// where there is no tunnel behind this opening, a knocked-down man now has a way out
	// of the room. Every figure already recorded was taken standing and is unaffected;
	// the fact is written down rather than compensated, because closing the hatch on one
	// map and opening it on another would mean having two ships.
	DockingPort = CreateDefaultSubobject<USFDockingPortComponent>(TEXT("DockingPort"));
	DockingPort->SetupAttachment(Hull);
	DockingPort->SetRelativeLocation(FVector(0.0, Half, DeckTopZ + HatchHeightCm * 0.5));
	DockingPort->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
```

В `Source/StarForge/Public/Ship/SFTestShip.h` добавить прямое объявление и член:

```cpp
class USFDockingPortComponent;
```

```cpp
	/** The ship's half of the airlock. Its +X points out through the hatch. */
	USFDockingPortComponent* GetDockingPort() const { return DockingPort; }
```

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFDockingPortComponent> DockingPort;
```

и включить заголовок в `SFTestShip.cpp`: `#include "Ship/SFDockingPortComponent.h"`.

- [ ] **Step 3: Написать станцию**

`Source/StarForge/Public/Ship/SFTestStation.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTestStation.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class USFDockingPortComponent;
class USFShipFrameComponent;

/**
 * The second reference frame: a boxed room that does not move.
 *
 * It carries the same USFShipFrameComponent the ship does. The name lies by half for a
 * station, and it is kept anyway: renaming the facade would touch every file in the
 * project for the sake of a word, and the component's contract -- "where is this frame,
 * and how do I convert into it" -- is exactly as true here. Spec 8 says so explicitly.
 *
 * No physics body of any kind. The frame component derives velocity and acceleration as
 * differences of its owner's transform precisely so that an owner which is not a rigid
 * body is still a full reference frame: the knockdown of spec 6 works on the station
 * through the same code, and reads zero, because a station that does not move cannot
 * knock anyone off his feet.
 *
 * Assembled in the constructor from engine cubes for the same reason the ship is: the
 * loop map is empty by design and the game mode spawns this from StaticClass(), so
 * there is no placed actor on which anything could be authored.
 */
UCLASS()
class STARFORGE_API ASFTestStation : public AActor
{
	GENERATED_BODY()

public:
	ASFTestStation();

	virtual void BeginPlay() override;

	USFShipFrameComponent* GetFrameComponent() const { return Frame; }

	/** The station's half of the airlock, at the outboard mouth of the tunnel. */
	USFDockingPortComponent* GetDockingPort() const { return DockingPort; }

private:
	/**
	 * Builds one wall out of an engine cube. Duplicated from ASFTestShip deliberately:
	 * the two hulls share a shape, not a life -- one has a rigid body, posts, a turret
	 * and a flight profile, and the other never will -- and a common base class would
	 * make every later change to one of them a change to both.
	 */
	UStaticMeshComponent* AddBoxPanel(FName Name, const FVector& RelativeLocation, const FVector& RelativeScale);

	/** Same as the ship's: a shape attached with no collision, purely to read direction by. */
	UStaticMeshComponent* AddOrientationMarker(
		FName Name, UStaticMesh* Mesh, const FVector& RelativeLocation, double UniformScale);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFShipFrameComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFDockingPortComponent> DockingPort;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UPointLightComponent> InteriorLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Panels;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> OrientationMarkers;
};
```

`Source/StarForge/Private/Ship/SFTestStation.cpp`:

```cpp
#include "Ship/SFTestStation.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
/**
 * Nested and named on purpose. The game module is unity-built, so every file's anonymous
 * namespace is one and the same anonymous namespace: a second CubeEdgeCm at that scope
 * would redefine the ship's, and the error would surface only after this file is
 * committed and rejoins the unity blob. The qualifier is what keeps the two hulls' figures
 * apart.
 */
namespace Station
{
	/** One engine cube is 100 cm on a side, so a mesh scale equals metres. */
	constexpr double CubeEdgeCm = 100.0;

	/** Panel thickness as a scale factor, not centimetres: 0.2 x 100 cm = 20 cm. */
	constexpr double PanelThickness = 0.2;

	/** The box is 12 x 8 x 8 m: half again as long as the ship's cabin along X. */
	constexpr double HalfXCm = 600.0;
	constexpr double HalfYCm = 400.0;
	constexpr double HalfZCm = 400.0;

	/** The hatch in the -Y wall, mirroring the ship's exactly. See SFTestShip.cpp. */
	constexpr double HatchHalfWidthCm = 100.0;
	constexpr double HatchHeightCm = 250.0;

	/**
	 * How far the airlock tunnel reaches out beyond the -Y wall, in centimetres.
	 *
	 * Two metres: long enough that a person is unmistakably inside it for several steps
	 * -- the whole point of the airlock is that the handover happens somewhere with a
	 * length, not on a line -- and short enough that a ship three metres out of position
	 * has not yet met it.
	 */
	constexpr double TunnelLengthCm = 200.0;

	/**
	 * The markers are the same three shapes as the ship's at a different size, and both
	 * halves of that carry information. The shape says which way you are facing -- sphere
	 * +X, cone +Y, cylinder -X in the hull's own axes; the size says which hull you are
	 * in. A person who has just walked out of the tunnel needs both at once, and the two
	 * rooms are otherwise the same grey box.
	 *
	 * Because the two hulls mate along Y, the ship's +Y and the station's -Y face each
	 * other: walking out of the ship's hatch is walking toward the station's cone.
	 */
	constexpr double MarkerUniformScale = 0.8;
	constexpr double MarkerOffsetXCm = 450.0;
	constexpr double MarkerOffsetYCm = 300.0;

	/**
	 * Attenuation radius in centimetres, raised from the ship's 2000 for a room half
	 * again as long. Intensity is the ship's 2000 cd unchanged: the project runs with
	 * auto-exposure off, so candelas map straight to screen brightness and a second
	 * number here would make the two rooms differently lit for no reason.
	 */
	constexpr double InteriorLightRadiusCm = 2500.0;
	constexpr double InteriorLightIntensityCandelas = 2000.0;
}
}

ASFTestStation::ASFTestStation()
{
	// Nothing to do per frame. The frame component ticks itself to sample the transform,
	// and that sample is constant here -- which is the point of a station.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));

	const double SpanX = Station::HalfXCm / Station::CubeEdgeCm * 2.0;
	const double SpanY = Station::HalfYCm / Station::CubeEdgeCm * 2.0;
	const double SpanZ = Station::HalfZCm / Station::CubeEdgeCm * 2.0;

	AddBoxPanel(TEXT("Floor"), FVector(0.0, 0.0, -Station::HalfZCm), FVector(SpanX, SpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("Ceiling"), FVector(0.0, 0.0, Station::HalfZCm), FVector(SpanX, SpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("WallXPos"), FVector(Station::HalfXCm, 0.0, 0.0), FVector(Station::PanelThickness, SpanY, SpanZ));
	AddBoxPanel(TEXT("WallXNeg"), FVector(-Station::HalfXCm, 0.0, 0.0), FVector(Station::PanelThickness, SpanY, SpanZ));
	AddBoxPanel(TEXT("WallYPos"), FVector(0.0, Station::HalfYCm, 0.0), FVector(SpanX, Station::PanelThickness, SpanZ));

	// The -Y wall carries the hatch, mirroring the ship's: three panels around an opening
	// 200 across and 250 tall from the deck, with the floor panel closing everything
	// below it. Both decks sit at -390 in their own hulls, which is what makes the two
	// floors continuous once the ports are mated.
	const double DeckTopZ = -Station::HalfZCm + Station::PanelThickness * Station::CubeEdgeCm * 0.5;   // -390
	const double HatchTopZ = DeckTopZ + Station::HatchHeightCm;                      // -140
	const double JambWidthCm = Station::HalfXCm - Station::HatchHalfWidthCm;                  //  500
	const double JambCentreXCm = (Station::HalfXCm + Station::HatchHalfWidthCm) * 0.5;        //  350
	const double LintelHeightCm = Station::HalfZCm - HatchTopZ;                      //  540
	const double LintelCentreZCm = (HatchTopZ + Station::HalfZCm) * 0.5;             //  130

	AddBoxPanel(TEXT("WallYNegJambXNeg"), FVector(-JambCentreXCm, -Station::HalfYCm, 0.0),
		FVector(JambWidthCm / Station::CubeEdgeCm, Station::PanelThickness, SpanZ));
	AddBoxPanel(TEXT("WallYNegJambXPos"), FVector(JambCentreXCm, -Station::HalfYCm, 0.0),
		FVector(JambWidthCm / Station::CubeEdgeCm, Station::PanelThickness, SpanZ));
	AddBoxPanel(TEXT("WallYNegLintel"), FVector(0.0, -Station::HalfYCm, LintelCentreZCm),
		FVector(Station::HatchHalfWidthCm * 2.0 / Station::CubeEdgeCm, Station::PanelThickness, LintelHeightCm / Station::CubeEdgeCm));

	// The airlock tunnel: a 200 x 250 corridor reaching two metres out along -Y, with a
	// floor flush with the station's and no doors at either end. It belongs to the port
	// in the sense spec 8 means -- it is the port's approach corridor and moves with the
	// station -- but its panels are attached to the root, so their transforms are written
	// in the same station centimetres as every other panel. A socket carrying walls is a
	// socket that cannot be moved without rebuilding the corridor.
	const double TunnelCentreYCm = -Station::HalfYCm - Station::TunnelLengthCm * 0.5;         // -500
	const double TunnelSpanY = Station::TunnelLengthCm / Station::CubeEdgeCm;                 //  2.0
	const double TunnelSpanX = Station::HatchHalfWidthCm * 2.0 / Station::CubeEdgeCm;         //  2.0
	const double TunnelSpanZ = Station::HatchHeightCm / Station::CubeEdgeCm;                  //  2.5
	const double TunnelPortZ = DeckTopZ + Station::HatchHeightCm * 0.5;              // -265

	AddBoxPanel(TEXT("TunnelFloor"), FVector(0.0, TunnelCentreYCm, -Station::HalfZCm),
		FVector(TunnelSpanX, TunnelSpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("TunnelCeiling"), FVector(0.0, TunnelCentreYCm, HatchTopZ + Station::PanelThickness * Station::CubeEdgeCm * 0.5),
		FVector(TunnelSpanX, TunnelSpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("TunnelWallXNeg"),
		FVector(-Station::HatchHalfWidthCm - Station::PanelThickness * Station::CubeEdgeCm * 0.5, TunnelCentreYCm, TunnelPortZ),
		FVector(Station::PanelThickness, TunnelSpanY, TunnelSpanZ));
	AddBoxPanel(TEXT("TunnelWallXPos"),
		FVector(Station::HatchHalfWidthCm + Station::PanelThickness * Station::CubeEdgeCm * 0.5, TunnelCentreYCm, TunnelPortZ),
		FVector(Station::PanelThickness, TunnelSpanY, TunnelSpanZ));

	// At the outboard mouth of the tunnel, facing out along -Y. A yaw of -90 takes local
	// +X onto -Y and leaves +Z alone, so the station's deck up and the ship's agree when
	// the two ports mate.
	DockingPort = CreateDefaultSubobject<USFDockingPortComponent>(TEXT("DockingPort"));
	DockingPort->SetupAttachment(Root);
	DockingPort->SetRelativeLocation(FVector(0.0, -Station::HalfYCm - Station::TunnelLengthCm, TunnelPortZ));
	DockingPort->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));

	InteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	InteriorLight->SetupAttachment(Root);
	InteriorLight->SetRelativeLocation(FVector::ZeroVector);
	InteriorLight->SetMobility(EComponentMobility::Movable);
	InteriorLight->IntensityUnits = ELightUnits::Candelas;
	InteriorLight->SetIntensity(static_cast<float>(Station::InteriorLightIntensityCandelas));
	InteriorLight->AttenuationRadius = static_cast<float>(Station::InteriorLightRadiusCm);
	InteriorLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	AddOrientationMarker(TEXT("MarkerSphere"), SphereMesh.Object,
		FVector(Station::MarkerOffsetXCm, 0.0, 0.0), Station::MarkerUniformScale);
	AddOrientationMarker(TEXT("MarkerCone"), ConeMesh.Object,
		FVector(0.0, Station::MarkerOffsetYCm, 0.0), Station::MarkerUniformScale);
	AddOrientationMarker(TEXT("MarkerCylinder"), CylinderMesh.Object,
		FVector(-Station::MarkerOffsetXCm, 0.0, 0.0), Station::MarkerUniformScale);
}

UStaticMeshComponent* ASFTestStation::AddBoxPanel(
	FName Name, const FVector& RelativeLocation, const FVector& RelativeScale)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Panel->SetupAttachment(Root);
	Panel->SetRelativeLocation(RelativeLocation);
	Panel->SetRelativeScale3D(RelativeScale);
	Panel->SetMobility(EComponentMobility::Movable);
	Panel->SetCollisionObjectType(ECC_WorldStatic);
	Panel->SetCollisionResponseToAllChannels(ECR_Block);

	// Query only, as on the ship. These panels hold a person up and nothing else; there
	// is no physics body anywhere on this actor for them to be welded into.
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	if (CubeMesh.Succeeded())
	{
		Panel->SetStaticMesh(CubeMesh.Object);
	}

	Panels.Add(Panel);
	return Panel;
}

UStaticMeshComponent* ASFTestStation::AddOrientationMarker(
	FName Name, UStaticMesh* Mesh, const FVector& RelativeLocation, double UniformScale)
{
	UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Marker->SetupAttachment(Root);
	Marker->SetRelativeLocation(RelativeLocation);
	Marker->SetRelativeScale3D(FVector(UniformScale));
	Marker->SetMobility(EComponentMobility::Movable);

	// Purely something to read direction by; it must not be able to block a walk.
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Mesh)
	{
		Marker->SetStaticMesh(Mesh);
	}

	OrientationMarkers.Add(Marker);
	return Marker;
}

void ASFTestStation::BeginPlay()
{
	Super::BeginPlay();

	const FVector Location = GetActorLocation();
	UE_LOG(LogStarForge, Display,
		TEXT("Station at world (%.0f %.0f %.0f) cm: %.0f x %.0f x %.0f m box, hatch in the -Y wall, ")
		TEXT("%.0f cm tunnel, frame %s, no physics body."),
		Location.X, Location.Y, Location.Z,
		Station::HalfXCm * 2.0 / Station::CubeEdgeCm, Station::HalfYCm * 2.0 / Station::CubeEdgeCm, Station::HalfZCm * 2.0 / Station::CubeEdgeCm,
		Station::TunnelLengthCm,
		Frame ? TEXT("present") : TEXT("MISSING"));
}
```

- [ ] **Step 4: Собрать, прогнать тесты и стенд**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

По-прежнему **65** `Result={Success}` и ноль `Result={Fail}` — задача 2 не трогает `StarForgeCore`, и падение здесь означало бы, что тронула.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log
```

В `Saved/Logs/StarForge.log` ожидается:

- ровно одна строка `Docking port on SFTestShip_0 at hull-local (0 400 -265) cm, out along (0.00 1.00 0.00), deck up (0.00 0.00 1.00).` — на этой карте станции нет, и второй строки быть не должно;
- строка `Turret mount at ship-local (0 0 280) cm: …` на месте — люк не задел турель;
- последняя строка `Flight:` даёт `speed 200.0 m/s` — стенд летает как летал.

`ASFTestStation` на этой карте не создаётся вообще: её спавнит только режим петли.

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFDockingPortComponent.h Source/StarForge/Private/Ship/SFDockingPortComponent.cpp Source/StarForge/Public/Ship/SFTestStation.h Source/StarForge/Private/Ship/SFTestStation.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp
git commit -F - <<'MSG'
feat(ship): a second reference frame, and a hatch to walk between them

The station carries the same frame component the ship does. The facade's name lies by
half for something that never moves, and renaming it would touch every file in the
project for a word; its contract is exactly as true here, and the component already
derives velocity and acceleration from transform differences so that an owner which is
not a rigid body is still a full frame.

It is a separate class rather than a ship without engines. The two hulls share a shape
and not a life -- one has a body, posts, a turret and a flight profile, the other never
will -- so the panel helper is duplicated on purpose. Twenty lines of repetition are
cheaper than a base class that every later difference has to be threaded through.

The hatch is three panels around an opening rather than a wall, and the port sits in
the middle of it, facing out. That makes the cabin an open box on the measurement rig
as well, where nothing is behind the opening: a knocked-down occupant is bounded by
walls and by nothing else, so he now has a way out of the room. Every figure recorded
so far was taken standing and is unaffected. The fact is written down rather than
compensated, because closing the hatch on one map and opening it on another would mean
having two ships.

Both decks sit at the same height in their own hulls, which is what makes the two
floors continuous once the ports mate, and both port poses are derived from the same
constants as the walls -- move a wall and the hatch and the port move with it.
MSG
```

---

### Task 3: Компонент стыковки, замок двигателей и `sf.Dock`

Механизм на подвижной стороне: окно каждый кадр, доводка за секунду, кинематическая привязка, замок на двигателях и одна консольная команда с фронтом вместо уровня.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFDockingComponent.h`
- Create: `Source/StarForge/Private/Ship/SFDockingComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFThrusterArrayComponent.h`
- Modify: `Source/StarForge/Private/Ship/SFThrusterArrayComponent.cpp`
- Modify: `Source/StarForge/Private/Ship/SFPilotStationComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`

**Interfaces:**
- Consumes: `StarForge::Docking::{FDockingLimits, FDockingApproach, EDockingRefusal, ComputeDockingApproach, CanDock, TargetHullPose, CapturePose}`; `StarForge::Units::CmToMetres`; `USFDockingPortComponent::GetPortTransform()`; `ASFTestStation::GetDockingPort()`; `USFShipFrameComponent::GetLinearVelocityMetersPerSec()`; `USFThrusterArrayComponent::{ClearDemand, Rebuild}`.
- Produces:
  - `enum class ESFDockingState : uint8 { Free, Capturing, Docked }`
  - `USFDockingComponent : UActorComponent` с `bool RequestDock()`, `bool RequestUndock()`, `void Toggle()`, `void DockImmediately()`, `ESFDockingState GetState() const`, `bool IsFree() const`, `bool IsDocked() const`, `bool HasStation() const`, `bool CanDockNow() const`, `StarForge::Docking::FDockingApproach GetLastApproach() const` (СИ), `StarForge::Docking::EDockingRefusal GetLastRefusal() const`, `FVector GetPortOffsetShipLocalMetres() const`, `double GetCaptureSeconds() const`, `double GetCaptureElapsedSeconds() const`, `StarForge::Docking::FDockingLimits BuildLimits() const`, `static const TCHAR* RefusalName(StarForge::Docking::EDockingRefusal)`; свойства `EditAnywhere`: `DistanceMetres = 3.0`, `SpeedMetresPerSec = 1.0`, `ForwardAngleDeg = 10.0`, `UpAngleDeg = 10.0`, `CaptureSeconds = 1.0`.
  - `USFThrusterArrayComponent::SetLocked(bool)`, `bool IsLocked() const`.
  - `ASFTestShip::GetDocking() const -> USFDockingComponent*`.
  - Консольная переменная `sf.Dock`, объявлена в `SFDockingComponent.cpp`, умолчание `-1`.

**Почему команда — фронт, а не уровень.** `sf.Dock 1`, читаемая как уровень, просила бы стыковку каждый кадр: один отказ превратился бы в поток по шестьдесят строк в секунду, а успешная стыковка — в попытку пристыковаться к самому себе. Компонент помнит последнее прочитанное значение и действует только на изменении, как `PollInteract` действует на нажатии, а не на удержании.

**Почему замок висит на массиве, а не на посте.** Пост пилота про стыковку не знает: он спрашивает у массива, заперт ли тот, и стоит в стороне ровно так же, как при скриптовом профиле. Массив запирается, потому что пристыкованный корпус не симулируется, и вектор силы, приложенный к нему, не идёт никуда, — а панель при этом печатала бы тягу, которой корабль не почувствовал.

**Почему `Rebuild()` при отстыковке.** Массив читает массу и тензор инерции **обратно из Chaos**, а корпус, проведший старт кинематическим, мог отдать свойства кинематического тела. Перестроение делается ровно в тот момент, когда корпус снова начинает симулироваться, и никто в этот момент не летит.

**Почему доводка сглажена — и почему под порог она всё равно не уходит.** Пиковое ускорение `smoothstep` за время `T` на пути `d` есть `6d/T²`, против мгновенной ступеньки у линейной интерполяции: 0 → 3 м/с за кадр, то есть 180 м/с² на 60 fps. Но `d` здесь — путь **корпуса**, а не расстояние между узлами: узел стоит на плече 4.80 м от центра, разворот на допустимые 10° добавляет корпусу 0.84 м, и на краю окна `6d/T²` даёт **23.0 м/с²** при пороге гравиплит 19.62. К этому прибавляется угловая часть — 1.047 рад/с², то есть 4.19 м/с² на плече 4 м, — и оба пика приходятся на **один** кадр, потому что вторая производная `smoothstep` максимальна по модулю при `α = 0` и `α = 1`, а не в середине. Плюс ступенька первого кадра: `SetSimulatePhysics(false)` гасит скорость корпуса за кадр, что на пределе 1 м/с есть 60 м/с².

Это принято сознательно, а не проглядено. **Команду стыковки подаёт пилот из кресла, а пристёгнутого не роняет по построению**, и второго человека на борту в срезе нет. Сглаживание оставлено потому, что оно снимает порядок величины даром; выводить `CaptureSeconds` из фактического пути — механизм ради случая, которого срез достичь не может. Числа обязаны быть названы в комментарии к `CaptureSeconds` и в бланке точки проверки 6; спрятать их за «18.0, запас 8%» нельзя — это и была ошибка первой редакции.

**Почему в строке `dock` есть тройка `at (x y z) ship`.** У каюты нет окон, а станция снаружи не освещена: пилот ведёт сближение по числам, и расстояние без направления — не то число, по которому можно действовать. Тройка — вектор от корабельного узла к станционному в осях корабля, в метрах: `+Y` — сторона люка, клавиша `D`; после отстыковки она читается `(+0.0 +0.0 +0.0)`, а после отхода клавишей `A` растёт её `Y`. Считается там же, где расстояние, и пишется рядом с ним, потому что должна совпадать с ним в единицах на глаз.

- [ ] **Step 1: Добавить замок на массив двигателей**

В `Source/StarForge/Public/Ship/SFThrusterArrayComponent.h`, после `ClearDemand()`:

```cpp
	/**
	 * Locks the array: ApplyDemand becomes a no-op that also clears the readout.
	 *
	 * A docked hull does not simulate, so a wrench applied to it goes nowhere. Without
	 * the lock the readout would print thrust the ship never felt, which is the stale
	 * reading this class already has one function to remove.
	 *
	 * The array refuses, not the post and not the operator: the post asks whether its
	 * engines are locked, and neither it nor whoever holds it has to know that docking
	 * exists. Same shape as the scripted-profile lock one level up.
	 */
	void SetLocked(bool bInLocked);

	bool IsLocked() const { return bLocked; }
```

и в приватной части:

```cpp
	/** True while something has locked the array. See SetLocked. */
	bool bLocked = false;
```

В `Source/StarForge/Private/Ship/SFThrusterArrayComponent.cpp` добавить определение рядом с `ClearDemand`:

```cpp
void USFThrusterArrayComponent::SetLocked(bool bInLocked)
{
	bLocked = bInLocked;
}
```

и первым в `ApplyDemand`, **до** проверки `bReady`:

```cpp
	// Before the readiness check, so a locked array reads as quiet whether or not it
	// ever built. Clearing rather than merely returning: the last demand anybody made
	// would otherwise sit in these accessors for as long as the ship stays docked, and
	// the readout would print a live-looking wrench beside the word "docked".
	if (bLocked)
	{
		ClearDemand();
		return;
	}
```

- [ ] **Step 2: Научить пост пилота стоять в стороне при замке**

В `Source/StarForge/Private/Ship/SFPilotStationComponent.cpp::TickComponent`, сразу **после** блока `if (bScriptedProfile) { … }`:

```cpp
	// Locked engines, for whatever reason locked them. The post does not know that
	// docking exists and must not: it knows that the thing it commands is refusing
	// commands, and the honest report of that is the same one it makes when the rig is
	// flying itself -- nobody in that seat is flying this ship. The same lesson as the
	// scripted-profile lock, one level down: the lock belongs to the executor, and the
	// post reads it rather than being told about the reason for it.
	if (Thrusters->IsLocked())
	{
		LastIntent = StarForge::FFlightIntent();
		bOperatorFlying = false;
		Thrusters->ClearDemand();
		return;
	}
```

- [ ] **Step 3: Написать компонент стыковки — заголовок**

`Source/StarForge/Public/Ship/SFDockingComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DockingMath.h"
#include "SFDockingComponent.generated.h"

class UPrimitiveComponent;
class USFDockingPortComponent;
class USFShipFrameComponent;
class USFThrusterArrayComponent;

/**
 * Three states and one step between each pair. Free to Docked always goes through
 * Capturing; Docked to Free is one step, because undocking is letting go.
 */
UENUM()
enum class ESFDockingState : uint8
{
	Free,
	Capturing,
	Docked
};

/**
 * Docking, on the moving side.
 *
 * It lives on the ship because the ship is what moves. The station has a port and no
 * component of this kind at all: giving both sides a mechanism would give the pair two
 * opinions about who is attached to whom.
 *
 * Docking is commanded, not automatic. Spec 8's first paragraphs read as automatic and
 * were decided otherwise on 2026-09-10: the approach conditions open a WINDOW, and one
 * key from the pilot's seat -- F, or sf.Dock for a headless run -- goes through it. With
 * one station in the slice, docking by accident while flying past the port is the only
 * way to spoil a manual checkpoint.
 *
 * "Docked" means a kinematic attachment. The hull stops simulating, the thrusters are
 * locked, the pose is run in over a second and the root is attached to the station's
 * socket. The relative velocity afterwards is zero BY CONSTRUCTION, which is what spec 8
 * requires of the airlock handover, rather than an approximation of zero. A physics
 * constraint was rejected for exactly that: its solver jitters, the residual velocity is
 * never zero, and "visually unnoticeable" stops being true.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFDockingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFDockingComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Spec 8's starting tolerances. Ship data, so they are authored, not compiled in. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double DistanceMetres = 3.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double SpeedMetresPerSec = 1.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double ForwardAngleDeg = 10.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double UpAngleDeg = 10.0;

	/**
	 * How long the run-in takes, in seconds.
	 *
	 * The run-in is eased, so its peak acceleration is 6d/T^2 -- an order of magnitude
	 * below a linear run-in, which would step the velocity from zero to full inside one
	 * frame (180 m/s^2 over three metres at sixty frames). It does NOT bring the capture
	 * under the 19.62 m/s^2 grav-plate ceiling of spec 6, and the arithmetic is written out
	 * here rather than rounded off, because an earlier version of this comment claimed a
	 * margin that does not exist:
	 *
	 *   - d is the HULL's path, not the distance between the ports. The port sits
	 *     sqrt(400^2 + 265^2) = 4.80 m off the hull's centre, so turning through the
	 *     window's 10 degrees of allowed misalignment adds 2 * 4.80 * sin(5 deg) = 0.84 m.
	 *     At the window's edge d is 3.84 m and 6d/T^2 is 23.0 m/s^2.
	 *   - The angular term is 6 * 10 deg / T^2 = 1.047 rad/s^2, which is 4.19 m/s^2 at a
	 *     four-metre radius, and it peaks in the SAME frame: smoothstep's second derivative
	 *     is largest at alpha 0 and alpha 1, not in the middle.
	 *   - The first frame is a step of its own: stopping the simulation kills the hull's
	 *     velocity in one frame, which at the window's 1 m/s limit is 60 m/s^2 at 60 fps.
	 *
	 * None of that is compensated, deliberately. The command comes from the pilot's seat and
	 * a strapped-in occupant cannot be knocked down; the slice has nobody else aboard.
	 * Deriving this figure from the actual path would be mechanism for a case the slice
	 * cannot reach -- the moment a standing passenger exists, it stops being optional.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double CaptureSeconds = 1.0;

	/** The window in the form the maths wants: SI and radians. One conversion, one place. */
	StarForge::Docking::FDockingLimits BuildLimits() const;

	/** Starts a capture if the approach is inside the window. False, with a reason logged, if not. */
	bool RequestDock();

	/** Lets go: detach, simulate, zero the velocities, unlock. False if not docked. */
	bool RequestUndock();

	/** What the F key does: dock if free, undock if docked, ignore during a capture. */
	void Toggle();

	/**
	 * Docks with no run-in at all, for a ship that is spawned already attached.
	 *
	 * Used by the loop rig, which must not have a single frame in which the ship is
	 * adrift: the pawn's very first tick has to find him in a docked cabin. It resolves
	 * its own ports rather than relying on BeginPlay, because the game mode spawns and
	 * docks before the world has begun play at all.
	 */
	void DockImmediately();

	ESFDockingState GetState() const { return State; }
	bool IsFree() const { return State == ESFDockingState::Free; }
	bool IsDocked() const { return State == ESFDockingState::Docked; }

	/** False when no station was found at all -- a legal state, and not a refusal reason. */
	bool HasStation() const { return bHasStation; }

	/** True when a dock command issued this instant would be accepted. */
	bool CanDockNow() const;

	/** The last approach measured, with Distance already in METRES. */
	StarForge::Docking::FDockingApproach GetLastApproach() const { return LastApproachSI; }

	StarForge::Docking::EDockingRefusal GetLastRefusal() const { return LastRefusal; }

	/**
	 * From the ship's port to the station's, in the ship's own axes and in metres, as of
	 * the last approach. The cabin has no windows and the station is unlit from outside,
	 * so this triple is the only thing that tells a pilot which way to fly: +Y is the side
	 * the hatch is on. Zero when the ports are mated.
	 */
	FVector GetPortOffsetShipLocalMetres() const { return LastPortOffsetShipLocalMetres; }

	double GetCaptureSeconds() const { return CaptureSeconds; }
	double GetCaptureElapsedSeconds() const { return CaptureElapsedSeconds; }

	/** Lower case: these words are read inside a sentence, not scanned for in a log. */
	static const TCHAR* RefusalName(StarForge::Docking::EDockingRefusal Refusal);

private:
	/** Idempotent; called from BeginPlay and from DockImmediately, whichever comes first. */
	void ResolvePorts();

	/** Measures the approach and stores it in SI. Only meaningful while Free. */
	void UpdateApproach();

	/** Locks, stops simulating, and takes the two ends of the run-in. */
	void EnterCapture();

	/** Sets the exact mated pose, attaches, and says so. */
	void CompleteCapture();

	void AdvanceCapture(double DeltaSeconds);

	/** Acts on a CHANGE of sf.Dock, and runs the debug cycle's timer. */
	void PollConsoleCommand(double DeltaSeconds);

	void LogRefusal() const;

	/** The simulated body: the owner's root, same idiom as the thruster array's. */
	UPrimitiveComponent* GetHull() const;

	UPROPERTY()
	TObjectPtr<USFDockingPortComponent> Port;

	UPROPERTY()
	TObjectPtr<USFDockingPortComponent> StationPort;

	UPROPERTY()
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	UPROPERTY()
	TObjectPtr<USFShipFrameComponent> Frame;

	ESFDockingState State = ESFDockingState::Free;
	bool bHasStation = false;
	bool bPortsResolved = false;

	StarForge::Docking::FDockingApproach LastApproachSI;

	/** From the ship's port to the station's, ship axes, metres. See the getter. */
	FVector LastPortOffsetShipLocalMetres = FVector::ZeroVector;

	/**
	 * Starts at Distance rather than None: before the first measurement nothing is known,
	 * and None would mean "you may dock" -- the readout's green, on an instrument that
	 * has never read anything. The same defect as a drift of zero with no measured time.
	 */
	StarForge::Docking::EDockingRefusal LastRefusal = StarForge::Docking::EDockingRefusal::Distance;

	FTransform CaptureStart = FTransform::Identity;
	FTransform CaptureTarget = FTransform::Identity;
	double CaptureElapsedSeconds = 0.0;

	/** Last value read from sf.Dock, so the command acts on an edge and not on a level. */
	int32 LastDockCommand = -1;

	/** Countdown for sf.Dock 2's re-dock; negative when no cycle is running. */
	double SecondsUntilAutoDock = -1.0;
};
```

- [ ] **Step 4: Написать компонент стыковки — реализация**

`Source/StarForge/Private/Ship/SFDockingComponent.cpp`:

```cpp
#include "Ship/SFDockingComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestStation.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * Docking from the console: the headless twin of the F key.
	 *
	 * Declared here because this is the file that reads it. A lookup by name from
	 * somewhere else returns null on a miss, and a missing variable reads as zero --
	 * which on this variable would mean "undock", issued by nobody.
	 *
	 * -1 is "no command". The component acts on a CHANGE of the value, not on its level:
	 * read as a level, a 1 would ask for a dock sixty times a second, turning one refusal
	 * into a torrent and one success into an attempt to dock with itself.
	 *
	 * 2 exists for headless runs and for nothing else. -ExecCmds applies its whole list at
	 * startup, so "sf.Dock 0, sf.Dock 1" leaves only the last value behind and the cycle
	 * docked -> free -> docked cannot be asked for in one run at all. 2 asks for the whole
	 * cycle on a single edge: undock now, dock again one capture later.
	 */
	TAutoConsoleVariable<int32> CVarDock(
		TEXT("sf.Dock"), -1,
		TEXT("1: dock, if the approach is inside the window. 0: undock. 2: undock and dock again after CaptureSeconds (the headless cycle). -1: no command.\n")
		TEXT("Acted on when the value CHANGES, so this is a keypress and not a state."),
		ECVF_Default);
}

USFDockingComponent::USFDockingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics, with the pilot's post. Nothing orders the two against each other
	// and nothing needs to: the worst case is one frame in which the post still holds a
	// demand for a hull that has just stopped simulating, and locking the array clears
	// that demand out of the readout in the same call.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

StarForge::Docking::FDockingLimits USFDockingComponent::BuildLimits() const
{
	// The one place degrees become radians for this component, as USFTurretComponent
	// does it. Two conversions of the same authored number is how a tolerance ends up
	// applied in degrees to a value in radians without anything crashing.
	StarForge::Docking::FDockingLimits Limits;
	Limits.DistanceMetres = DistanceMetres;
	Limits.SpeedMetresPerSec = SpeedMetresPerSec;
	Limits.ForwardAngleRad = FMath::DegreesToRadians(ForwardAngleDeg);
	Limits.UpAngleRad = FMath::DegreesToRadians(UpAngleDeg);
	return Limits;
}

UPrimitiveComponent* USFDockingComponent::GetHull() const
{
	AActor* Owner = GetOwner();
	return Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
}

void USFDockingComponent::ResolvePorts()
{
	if (bPortsResolved)
	{
		return;
	}
	bPortsResolved = true;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Port = Owner->FindComponentByClass<USFDockingPortComponent>();
	Thrusters = Owner->FindComponentByClass<USFThrusterArrayComponent>();
	Frame = Owner->FindComponentByClass<USFShipFrameComponent>();

	// One station in the slice, so the first one found is the one. A second would need a
	// choice, and a choice needs a rule nobody has written yet -- said here rather than
	// left for whoever adds the second station to discover.
	for (TActorIterator<ASFTestStation> It(GetWorld()); It; ++It)
	{
		StationPort = It->GetDockingPort();
		break;
	}
	bHasStation = StationPort != nullptr;

	if (!Port)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Docking component found no port on its owner; this ship can never dock."));
	}

	// Not an error. L_FrameSpike has no station on purpose, and a ship that cannot dock
	// there is correct rather than broken -- it just has to say so when asked.
	UE_LOG(LogStarForge, Display,
		TEXT("Docking: %s, window %.1f m, %.1f m/s, fwd %.0f deg, up %.0f deg, capture %.1f s."),
		bHasStation ? TEXT("station found") : TEXT("no station in this level"),
		DistanceMetres, SpeedMetresPerSec, ForwardAngleDeg, UpAngleDeg, CaptureSeconds);
}

void USFDockingComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolvePorts();
}

const TCHAR* USFDockingComponent::RefusalName(StarForge::Docking::EDockingRefusal Refusal)
{
	switch (Refusal)
	{
	case StarForge::Docking::EDockingRefusal::None: return TEXT("none");
	case StarForge::Docking::EDockingRefusal::Distance: return TEXT("distance");
	case StarForge::Docking::EDockingRefusal::Speed: return TEXT("speed");
	case StarForge::Docking::EDockingRefusal::Forward: return TEXT("forward");
	case StarForge::Docking::EDockingRefusal::Up: return TEXT("up");
	}
	return TEXT("?");
}

bool USFDockingComponent::CanDockNow() const
{
	return bHasStation
		&& State == ESFDockingState::Free
		&& LastRefusal == StarForge::Docking::EDockingRefusal::None;
}

void USFDockingComponent::UpdateApproach()
{
	const AActor* Owner = GetOwner();
	if (!Port || !StationPort || !Owner)
	{
		LastApproachSI = StarForge::Docking::FDockingApproach();
		LastPortOffsetShipLocalMetres = FVector::ZeroVector;
		LastRefusal = StarForge::Docking::EDockingRefusal::Distance;
		return;
	}

	// The station does not move, so the relative speed is the ship's own. Read from the
	// frame component rather than from Chaos: the frame differences its owner's transform
	// and so gives the same answer for a hull that is simulating and one that is not,
	// which is what makes this line work in every state. When a second moving carrier
	// arrives, this is the line that has to become a difference of two velocities.
	//
	// The frame samples in TG_PostPhysics and this runs pre-physics, so the figure is one
	// frame old: 1.6 cm of travel at the 1 m/s limit and sixty frames a second.
	const double SpeedSI = Frame ? Frame->GetLinearVelocityMetersPerSec().Size() : 0.0;

	const StarForge::Docking::FDockingApproach InCentimetres =
		StarForge::Docking::ComputeDockingApproach(
			Port->GetPortTransform(), StationPort->GetPortTransform(), SpeedSI);

	// The one crossing of the unit boundary in this component. Everything the maths is
	// then asked is asked in SI.
	LastApproachSI = InCentimetres;
	LastApproachSI.Distance = StarForge::Units::CmToMetres(InCentimetres.Distance);

	// Where the station's port is from the ship's, in the ship's own axes. The cabin has
	// no windows and the station is unlit from outside, so a pilot flies the approach on
	// numbers alone, and a distance without a direction is not a number he can act on:
	// +Y in this triple is the side the hatch is on, which is the D key. Metres, because
	// it is read beside the distance and the two have to agree in unit at a glance.
	const FVector PortToPortWorldCm =
		StationPort->GetPortTransform().GetLocation() - Port->GetPortTransform().GetLocation();
	const FVector PortToPortShipCm =
		Owner->GetActorTransform().InverseTransformVectorNoScale(PortToPortWorldCm);
	LastPortOffsetShipLocalMetres = FVector(
		StarForge::Units::CmToMetres(PortToPortShipCm.X),
		StarForge::Units::CmToMetres(PortToPortShipCm.Y),
		StarForge::Units::CmToMetres(PortToPortShipCm.Z));

	LastRefusal = StarForge::Docking::CanDock(LastApproachSI, BuildLimits());
}

void USFDockingComponent::LogRefusal() const
{
	FString Detail;
	switch (LastRefusal)
	{
	case StarForge::Docking::EDockingRefusal::Distance:
		Detail = FString::Printf(TEXT("distance %.1f m over %.1f"), LastApproachSI.Distance, DistanceMetres);
		break;
	case StarForge::Docking::EDockingRefusal::Speed:
		Detail = FString::Printf(TEXT("speed %.1f m/s over %.1f"), LastApproachSI.SpeedMetresPerSec, SpeedMetresPerSec);
		break;
	case StarForge::Docking::EDockingRefusal::Forward:
		Detail = FString::Printf(TEXT("forward %.1f deg over %.0f"),
			FMath::RadiansToDegrees(LastApproachSI.ForwardAngleRad), ForwardAngleDeg);
		break;
	case StarForge::Docking::EDockingRefusal::Up:
		Detail = FString::Printf(TEXT("up %.1f deg over %.0f"),
			FMath::RadiansToDegrees(LastApproachSI.UpAngleRad), UpAngleDeg);
		break;
	default:
		Detail = TEXT("nothing, which should be impossible here");
		break;
	}

	// One line per command, and it names WHICH condition and by how much. A refusal a
	// pilot cannot act on is a refusal that sends him to correct the wrong thing.
	UE_LOG(LogStarForge, Warning, TEXT("Dock: refused -- %s"), *Detail);
}

void USFDockingComponent::EnterCapture()
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Hull = GetHull();
	if (!Owner || !Hull || !Port || !StationPort)
	{
		return;
	}

	// Order matters and is this: lock, clear, stop simulating, then take the two ends.
	//
	// Locked first so that nothing can apply a wrench to a body that is about to become
	// kinematic; cleared immediately after so the readout stops reporting the last demand
	// in the same frame rather than a frame later, in which it would print live thrust on
	// a ship that has begun docking.
	if (Thrusters)
	{
		Thrusters->SetLocked(true);
		Thrusters->ClearDemand();
	}

	Hull->SetSimulatePhysics(false);

	CaptureStart = Owner->GetActorTransform();

	// The port's transform relative to the hull, taken as world-relative-to-actor rather
	// than through GetRelativeTransform(). The two are equal today, because the port is
	// attached straight to the root; they stop being equal the moment anything is
	// inserted between them, and nothing would announce that. This form asks the question
	// the maths actually needs: where is the port in the hull's frame.
	const FTransform PortLocalToHull =
		Port->GetPortTransform().GetRelativeTransform(Owner->GetActorTransform());

	CaptureTarget = StarForge::Docking::TargetHullPose(StationPort->GetPortTransform(), PortLocalToHull);

	CaptureElapsedSeconds = 0.0;
	State = ESFDockingState::Capturing;
}

void USFDockingComponent::AdvanceCapture(double DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	CaptureElapsedSeconds += DeltaSeconds;

	const double Alpha = (CaptureSeconds > UE_DOUBLE_SMALL_NUMBER)
		? CaptureElapsedSeconds / CaptureSeconds
		: 1.0;

	if (Alpha >= 1.0)
	{
		CompleteCapture();
		return;
	}

	// Eased, not linear, though it does not buy as much as it looks. The frame component
	// differences its owner's position twice to get acceleration, and a linear run-in
	// steps the velocity from zero to its full value inside one frame: over three metres
	// in a second that is 180 m/s^2 at sixty frames. Smoothstep peaks at 6d/T^2 instead
	// -- 23.0 m/s^2 at the window's edge once the port's 4.80 m lever arm is counted into
	// d, which is still over spec 6's 19.62 ceiling. See CaptureSeconds for the whole
	// arithmetic and for why that is accepted rather than fixed.
	const double Eased = Alpha * Alpha * (3.0 - 2.0 * Alpha);

	// A teleport, like the carry itself. The hull is kinematic and the occupant rides it
	// through the frame's transform difference; a swept move here would drag the whole
	// ship against its own station.
	Owner->SetActorTransform(
		StarForge::Docking::CapturePose(CaptureStart, CaptureTarget, Eased),
		/*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);
}

void USFDockingComponent::CompleteCapture()
{
	AActor* Owner = GetOwner();
	if (!Owner || !StationPort)
	{
		return;
	}

	// Exactly the target, from the maths, not the last interpolated step. This pose is
	// what the attachment freezes for as long as the ship stays docked.
	Owner->SetActorTransform(
		CaptureTarget, /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);

	// Attached to the socket, keeping the world transform we have just set. From here the
	// relative velocity is zero by construction rather than by convergence -- which is
	// exactly what spec 8 asks of the handover, and what a physics constraint could not
	// give.
	Owner->GetRootComponent()->AttachToComponent(
		StationPort, FAttachmentTransformRules::KeepWorldTransform);

	State = ESFDockingState::Docked;
	CaptureElapsedSeconds = CaptureSeconds;

	UE_LOG(LogStarForge, Display, TEXT("Dock: docked."));
}

bool USFDockingComponent::RequestDock()
{
	if (State != ESFDockingState::Free)
	{
		return false;
	}

	if (!bHasStation)
	{
		UE_LOG(LogStarForge, Warning, TEXT("Dock: refused -- no station"));
		return false;
	}

	UpdateApproach();

	if (LastRefusal != StarForge::Docking::EDockingRefusal::None)
	{
		LogRefusal();
		return false;
	}

	EnterCapture();

	UE_LOG(LogStarForge, Display,
		TEXT("Dock: capturing, %.1f m, %.1f m/s, fwd %.1f deg, up %.1f deg"),
		LastApproachSI.Distance, LastApproachSI.SpeedMetresPerSec,
		FMath::RadiansToDegrees(LastApproachSI.ForwardAngleRad),
		FMath::RadiansToDegrees(LastApproachSI.UpAngleRad));

	return true;
}

bool USFDockingComponent::RequestUndock()
{
	// Docked and nothing else. A capture is not cancellable (see Toggle), and letting go
	// of a hull half way between two poses would hand Chaos a body nobody positioned.
	if (State != ESFDockingState::Docked)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	UPrimitiveComponent* Hull = GetHull();
	if (!Owner || !Hull)
	{
		return false;
	}

	// Detach FIRST, then simulate. SetSimulatePhysics(true) detaches an attached
	// component itself and says so in its own documentation (PrimitiveComponent.h
	// 1648-1656), and stopping simulation does not put it back -- which is also why
	// docking attaches explicitly. Doing the detach here means the code states what
	// happens instead of depending on a side effect that a later engine version could
	// reasonably drop.
	Owner->GetRootComponent()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	Hull->SetSimulatePhysics(true);

	// Zeroed after the body is live, because a kinematic body has no velocity to set.
	// Spec 8: relative velocity after docking is zero, so it is zero at the moment of
	// letting go too -- the ship does not spring off the station.
	Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Hull->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);

	if (Thrusters)
	{
		// The array reads its mass and inertia back from Chaos, and a hull that spent the
		// start of the session kinematic may have handed it a kinematic body's properties.
		// This is the instant those numbers start meaning something again, and nobody is
		// flying in it.
		Thrusters->Rebuild();
		Thrusters->SetLocked(false);
	}

	State = ESFDockingState::Free;
	CaptureElapsedSeconds = 0.0;

	UE_LOG(LogStarForge, Display, TEXT("Dock: undocked."));
	return true;
}

void USFDockingComponent::Toggle()
{
	switch (State)
	{
	case ESFDockingState::Free:
		RequestDock();
		break;
	case ESFDockingState::Docked:
		RequestUndock();
		break;
	default:
		// A capture is not cancellable. Half way through, the hull is between two poses
		// with nothing owning it: not simulating, not attached, and not where either side
		// expects. Said out loud, because a key that does nothing is otherwise
		// indistinguishable from a key that is not bound.
		UE_LOG(LogStarForge, Display, TEXT("Dock: capture in progress; the command is ignored."));
		break;
	}
}

void USFDockingComponent::DockImmediately()
{
	ResolvePorts();

	if (!Port || !StationPort)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Dock: cannot spawn docked -- %s. The ship will start adrift."),
			Port ? TEXT("no station port") : TEXT("no port on this ship"));
		return;
	}

	// No run-in: the loop rig must not have a single frame in which the ship is adrift,
	// because the pawn's first tick has to find him standing in a docked cabin.
	EnterCapture();
	CompleteCapture();
}

void USFDockingComponent::PollConsoleCommand(double DeltaSeconds)
{
	const int32 Command = CVarDock.GetValueOnGameThread();

	if (Command != LastDockCommand)
	{
		LastDockCommand = Command;

		switch (Command)
		{
		case 1:
			RequestDock();
			break;
		case 0:
			RequestUndock();
			break;
		case 2:
			if (RequestUndock())
			{
				SecondsUntilAutoDock = CaptureSeconds;
			}
			break;
		default:
			// -1 and anything unrecognised: no command. Deliberately silent -- this runs
			// every frame, and a line here would be sixty a second on the default value.
			break;
		}
	}

	if (SecondsUntilAutoDock > 0.0)
	{
		SecondsUntilAutoDock -= DeltaSeconds;
		if (SecondsUntilAutoDock <= 0.0)
		{
			SecondsUntilAutoDock = -1.0;
			RequestDock();
		}
	}
}

void USFDockingComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	PollConsoleCommand(DeltaTime);

	switch (State)
	{
	case ESFDockingState::Free:
		// Every frame, whether or not anybody intends to dock. The readout has to be able
		// to say CAN DOCK before the key is pressed, or a pilot learns whether he was in
		// the window only by being refused.
		UpdateApproach();
		break;
	case ESFDockingState::Capturing:
		AdvanceCapture(DeltaTime);
		break;
	default:
		break;
	}
}
```

- [ ] **Step 5: Повесить компонент на корабль и добавить строку в журнал**

В `Source/StarForge/Public/Ship/SFTestShip.h` — прямое объявление `class USFDockingComponent;`, геттер и член:

```cpp
	/** Docking lives on the moving side. The station has a port and no mechanism. */
	USFDockingComponent* GetDocking() const { return Docking; }
```

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFDockingComponent> Docking;
```

В `Source/StarForge/Private/Ship/SFTestShip.cpp` — включить `#include "Ship/SFDockingComponent.h"` и создать компонент в конструкторе рядом с прочими:

```cpp
	Docking = CreateDefaultSubobject<USFDockingComponent>(TEXT("Docking"));
```

В `LogFlightState`, **первым** блоком, до строки турели:

```cpp
	// Docking first, because it gates everything below it: a docked ship is a ship whose
	// engines are locked, and the flight numbers that follow are then correctly zero
	// rather than suspiciously zero. The variable behind this line is sf.FlightLog, which
	// is declared in this file -- the docking component's own sf.Dock is read there and
	// nowhere else, and this line reads the component's accessors instead.
	if (Docking)
	{
		FString DockText;

		switch (Docking->GetState())
		{
		case ESFDockingState::Docked:
			DockText = TEXT("docked");
			break;
		case ESFDockingState::Capturing:
			DockText = FString::Printf(TEXT("capturing %.1f of %.1f s"),
				Docking->GetCaptureElapsedSeconds(), Docking->GetCaptureSeconds());
			break;
		default:
			if (!Docking->HasStation())
			{
				DockText = TEXT("free -- no station");
			}
			else
			{
				const StarForge::Docking::FDockingApproach Approach = Docking->GetLastApproach();
				const FString Verdict = Docking->CanDockNow()
					? FString(TEXT("CAN DOCK"))
					: FString::Printf(TEXT("out of window: %s"),
						USFDockingComponent::RefusalName(Docking->GetLastRefusal()));

				const FVector At = Docking->GetPortOffsetShipLocalMetres();
				DockText = FString::Printf(
					TEXT("free | port %.1f m at (%+.1f %+.1f %+.1f) ship, %.1f m/s, fwd %.1f deg, up %.1f deg -- %s"),
					Approach.Distance, At.X, At.Y, At.Z, Approach.SpeedMetresPerSec,
					FMath::RadiansToDegrees(Approach.ForwardAngleRad),
					FMath::RadiansToDegrees(Approach.UpAngleRad),
					*Verdict);
			}
			break;
		}

		UE_LOG(LogStarForge, Log, TEXT("Dock: %s"), *DockText);
	}
```

- [ ] **Step 6: Собрать и проверить на стенде**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1, sf.Dock 1" -log
```

В `Saved/Logs/StarForge.log` ожидается:

- `Docking: no station in this level, window 3.0 m, 1.0 m/s, fwd 10 deg, up 10 deg, capture 1.0 s.` — один раз;
- **ровно одна** строка `Dock: refused -- no station`. Считать: `grep -c "refused -- no station" Saved/Logs/StarForge.log` → `1`. Больше одной означало бы, что команда читается как уровень, а не как фронт, и это дефект, а не шум;
- строки `Dock: free -- no station` раз в секунду — примерно 24 штуки;
- последняя `Flight:` даёт `speed 200.0 m/s` — замок не тронул полёт там, где стыковаться не с чем.

Тесты по-прежнему **65** / **0**.

- [ ] **Step 7: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFDockingComponent.h Source/StarForge/Private/Ship/SFDockingComponent.cpp Source/StarForge/Public/Ship/SFThrusterArrayComponent.h Source/StarForge/Private/Ship/SFThrusterArrayComponent.cpp Source/StarForge/Private/Ship/SFPilotStationComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp
git commit -F - <<'MSG'
feat(ship): dock by command, and lock the engines while attached

Docking lives on the ship because the ship is what moves; the station has a port and no
mechanism at all, since two mechanisms would give the pair two opinions about who is
attached to whom.

It is commanded, not automatic. Spec 8's opening paragraphs read as automatic and were
decided otherwise: the approach conditions open a window, and one key goes through it.
With a single station in the slice, docking by accident while flying past the port is
the only way to spoil a manual checkpoint. The command acts on a change of sf.Dock
rather than on its value, because a level read every frame turns one refusal into sixty
a second and one success into an attempt to dock with itself.

Attached means kinematic. The hull stops simulating, the array is locked, the pose runs
in over a second and the root is attached to the socket, so the relative velocity
afterwards is zero by construction rather than by convergence -- which is what the
airlock handover needs and what a physics constraint cannot give.

The run-in is eased and that is arithmetic, not taste -- but the arithmetic does not end
where it is comfortable. Smoothstep peaks at six times the path over the time squared
instead of stepping the velocity from zero in a single frame, which is an order of
magnitude, and it still does not come under the grav-plate ceiling: the path that matters
is the hull's, the port sits 4.80 m off the hull's centre, and the window's ten degrees of
allowed misalignment carry the hull 0.84 m further than the ports are apart. That is
23.0 m/s2 against a ceiling of 19.62, with the angular term's 4.19 landing in the same
frame. It is accepted rather than fixed because the command comes from the pilot's seat
and a strapped-in occupant cannot be knocked down, and the slice has nobody else aboard.
The figures are written out in the property's comment so the next person inherits the
truth rather than a margin that was never there.

The lock belongs to the thruster array, not to the post and not to the operator. The
post asks whether its engines are locked and stands down exactly as it does under the
scripted profile; nothing about docking reaches it. The array clears its demand as it
locks, so the readout cannot print thrust the ship never felt, and rebuilds itself as it
unlocks, because it reads mass and inertia back from Chaos and a hull that spent the
session kinematic may have handed it a kinematic body's numbers.

Undocking detaches before it simulates. Enabling simulation detaches the component by
itself and disabling it does not reattach; doing both explicitly means the code says
what happens rather than leaning on a side effect.
MSG
```

---

### Task 4: Режим петли, карта `L_Loop` и старт пристыкованным

Второй стенд. Станция в начале координат, корабль появляется **уже пристыкованным**, пешка в каюте, скриптовый профиль выключен и об этом сказано вслух.

**Files:**
- Create: `Source/StarForge/Public/SFLoopGameMode.h`
- Create: `Source/StarForge/Private/SFLoopGameMode.cpp`
- Create: `Content/Maps/L_Loop.umap` (руками в редакторе, шаг 3)

**Interfaces:**
- Consumes: `ASFTestStation`, `ASFTestShip::{GetDocking, GetInteriorSpawnWorldLocation}`, `USFDockingComponent::DockImmediately()`, `ASFCharacter`, `ASFFrameDriftHUD`.
- Produces: `ASFLoopGameMode : AGameModeBase`; карта `/Game/Maps/L_Loop` с `GameMode Override = SFLoopGameMode`.

**Почему стыковка до `Super::StartPlay()`, а не после.** `AGameModeBase::StartPlay` — это и есть момент, когда мир начинает игру и рассылает `BeginPlay`; пешка появляется внутри него. Если пристыковать корабль **после**, то в момент появления пешки корабль стоит в начале координат — там же, где станция, — и объёмы двух носителей накрывают одну точку. Кто из них заявит права на человека, не определено ничем. Поэтому корабль стыкуется до того, как хоть что-нибудь начало играть, и `DockImmediately()` разрешает свои узлы сам, не полагаясь на `BeginPlay`, который ещё не случился.

**Почему `sf.ShipProfile` ищется по имени здесь — и почему это единственное такое место.** Правило проекта: переменную объявляет тот, кто её читает, потому что промах поиска возвращает ноль, а ноль молчит. Здесь переменная не читается, а **записывается**, и записывающему деваться некуда: `sf.ShipProfile` принадлежит `SFTestShip.cpp`. Промах поэтому обязан быть громкой ошибкой в журнале, а не тишиной, — иначе петля молча полетела бы по скриптовому профилю, то есть корабль дёргался бы на 400 м/с прямо у станции.

**Почему имена акторов задаются явно.** Строка передачи системы отсчёта — главное, что человек читает в момент перехода, и `SFTestStation_0 -> SFTestShip_0` читается вдвое хуже, чем `Station -> Ship`. Имена задаются здесь, потому что это единственное место, где оба актора создаются вместе.

- [ ] **Step 1: Написать режим — заголовок**

`Source/StarForge/Public/SFLoopGameMode.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SFLoopGameMode.generated.h"

class ASFTestShip;
class ASFTestStation;

/**
 * The loop rig: a station, a ship already docked to it, and a person standing in the
 * ship's cabin.
 *
 * A second rig rather than a change to the first. L_FrameSpike does not move a single
 * default: every figure this project has recorded was taken on it, and changing what it
 * spawns would devalue all of them at once. Spec 8 says so in as many words.
 *
 * The scene is assembled in code for the reason the spike's is: the map is empty by
 * design, so there is no placed actor on which anything could be authored, and a clean
 * checkout reproduces the run exactly.
 */
UCLASS()
class STARFORGE_API ASFLoopGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASFLoopGameMode();

	virtual void StartPlay() override;

private:
	UPROPERTY()
	TObjectPtr<ASFTestStation> Station;

	UPROPERTY()
	TObjectPtr<ASFTestShip> Ship;
};
```

- [ ] **Step 2: Написать режим — реализация**

`Source/StarForge/Private/SFLoopGameMode.cpp`:

```cpp
#include "SFLoopGameMode.h"

#include "Debug/SFFrameDriftHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/SFCharacter.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFTestShip.h"
#include "Ship/SFTestStation.h"
#include "StarForgeLog.h"

ASFLoopGameMode::ASFLoopGameMode()
{
	DefaultPawnClass = ASFCharacter::StaticClass();
	HUDClass = ASFFrameDriftHUD::StaticClass();
}

void ASFLoopGameMode::StartPlay()
{
	// The scripted profile is a measurement instrument and this is not a measurement:
	// left on, the ship would fly the checkpoint manoeuvre at 400 m/s while attached to a
	// station, which is not a state anything here is written for.
	//
	// This is the only place in the project that reaches a console variable by name, and
	// it does so because it WRITES one that belongs to another file. The rule everywhere
	// else -- declare it where you read it -- exists because a failed lookup returns null
	// and a missing value reads as zero, silently. Here the miss is said out loud, in
	// error, because the alternative is a loop rig quietly flying the wrong profile.
	if (IConsoleVariable* Profile = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile")))
	{
		Profile->Set(0);
	}
	else
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Loop rig: sf.ShipProfile not found, so the scripted profile could not be turned off. ")
			TEXT("If the ship starts flying the checkpoint manoeuvre at the station, this line is why."));
	}

	FActorSpawnParameters StationParams;
	StationParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Requested rather than the default Required_Fatal: a taken name is not worth a crash,
	// and nothing depends on the name beyond the readability of one log line.
	StationParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	// Named, because the frame-handover line is the thing a person reads at the moment of
	// the transition, and "Station -> Ship" reads twice as well as "SFTestStation_0 ->
	// SFTestShip_0". Nothing else depends on these names.
	StationParams.Name = TEXT("Station");
	Station = GetWorld()->SpawnActor<ASFTestStation>(
		ASFTestStation::StaticClass(), FTransform::Identity, StationParams);

	FActorSpawnParameters ShipParams;
	ShipParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ShipParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	ShipParams.Name = TEXT("Ship");

	// Spawned at the origin and immediately moved by the docking maths. The pose is not
	// authored here on purpose: it is derived from the two ports, so moving a wall or a
	// port moves the docked ship with it, and a constant here would silently stop
	// tracking them.
	Ship = GetWorld()->SpawnActor<ASFTestShip>(
		ASFTestShip::StaticClass(), FTransform::Identity, ShipParams);

	// Before Super::StartPlay, and that ordering is the whole trick. Super is what begins
	// play on the world and spawns the pawn; dock after it and the pawn appears while the
	// ship is still at the origin, inside the station's interior volume as well as its
	// own, with nothing deciding which of the two claims him. Docking here means there is
	// no frame in which the ship is adrift, and DockImmediately resolves its own ports
	// because BeginPlay has not run yet.
	if (Ship && Ship->GetDocking())
	{
		Ship->GetDocking()->DockImmediately();
	}
	else
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Loop rig: no ship or no docking component; the loop cannot start docked."));
	}

	Super::StartPlay();

	if (Ship)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				// He appears at the world origin, which on this map is inside the station,
				// and is moved into the cabin here. That move is a genuine crossing of a
				// frame boundary and produces the run's first Frame handover line --
				// Station -> Ship, zero centimetres, because neither carrier is moving.
				// It is the mechanism working, not a defect, and the log says so.
				Pawn->SetActorLocation(Ship->GetInteriorSpawnWorldLocation());
			}
		}
	}

	UE_LOG(LogStarForge, Display,
		TEXT("Loop rig: scripted profile forced off; the ship spawns docked."));
}
```

- [ ] **Step 3: Создать карту `L_Loop` — один раз, руками**

Собрать и убедиться, что режим компилируется, **прежде** чем открывать редактор:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Открыть редактор:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject"
```

В редакторе:

```
File → New Level → Empty Level
Window → World Settings → GameMode Override = SFLoopGameMode
File → Save Current Level As → папка Maps, имя L_Loop
```

Уровень должен остаться **пустым**: станция, корабль, пешка и панель создаются кодом при запуске, как и на `L_FrameSpike`. `GameMode Override` — это `AWorldSettings::DefaultGameMode` (`WorldSettings.h:634`), и он единственный, что отличает эту карту от `L_FrameSpike`.

**Карту по умолчанию не менять.** В `Config/DefaultEngine.ini` по-прежнему `L_FrameSpike`: петля запускается позиционным аргументом, и всё, что снято на стенде, продолжает сниматься тем же рецептом без изменений.

Закрыть редактор. Закоммитить карту отдельно от кода — она идёт через LFS:

```bash
git add Content/Maps/L_Loop.umap
git commit -m "feat(loop): add the empty level the loop rig assembles itself into"
```

- [ ] **Step 4: Прогон (а) — старт пристыкованным, увод стоя, обе частоты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log
```

и то же самое с `-fps=400`.

Ожидается в `Saved/Logs/StarForge.log`:

- `Station at world (0 0 0) cm: 12 x 8 x 8 m box, hatch in the -Y wall, 200 cm tunnel, frame present, no physics body.`
- две строки `Docking port on …`: `Ship` с `(0 400 -265)` / `out along (0.00 1.00 0.00)` и `Station` с `(0 -600 -265)` / `out along (0.00 -1.00 0.00)`;
- `Docking: station found, window 3.0 m, 1.0 m/s, fwd 10 deg, up 10 deg, capture 1.0 s.`
- `Dock: docked.` — один раз, при старте;
- `Loop rig: scripted profile forced off; the ship spawns docked.`
- строка `Dock: docked` в **первой же** секундной записи журнала;
- `Flight: flown by console (standing down) …` — пост стоит в стороне, потому что двигатели заперты;
- `Thruster array ready: 24 thrusters, 50000 kg, inertia (…)` — **записать три числа инерции**. Если они читаются `533333, 533333, 533333`, кинематический корпус отдаёт свои свойства массы и всё в порядке; если нули или что-то иное, это Chaos, отвечающий за кинематическое тело, и годным считается число в строке, которая появится **после** `Dock: undocked.` в прогоне (б). Оба исхода приемлемы; проверка полётом — прогон (б);
- `Shipboard drift: 60.0 s settled, drift now 0.000 cm, peak drift 0.000 cm, peak jitter 0.000 cm/tick, skipped carries 0, knocked down 0.0 s -- footing intact`.

Последняя строка — существо этого прогона. **Ноль здесь не «ничего не произошло», а результат:** несимулируемая опора означает, что `UpdateBasedMovement` зовётся раз в кадр, из обычного тика, а не дважды из post-physics (`CharacterMovementComponent.cpp` 2476–2510), — расписание, при котором перенос ещё ни разу не работал. Строка `measured` обязана перевалить за 55 секунд, иначе цифра увода не значит ничего.

Любой ненулевой увод здесь — находка: на этой сцене не движется ничто.

- [ ] **Step 5: Прогон (б) — отстыковка и крен на обеих частотах**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.Knockdown 0, sf.Dock 0, sf.Intent.Roll 1" -log
```

и то же самое с `-fps=400`.

`sf.Knockdown 0` здесь — условие чистоты прогона, а не послабление. Стоящий в центре каюты находится в 3.0 м от оси крена, и 180°/с дают ему 29.8 м/с² — выше порога, так что с включённым сбиванием его роняет и швыряет по каюте, как на стенде. Но здесь каюта крутится **вплотную к станции**: её углы (5.5 м от центра) проходят сквозь тоннель и панели станции (от 4.0 м), и уроненный, брошенный в угол, может встретить станционную панель, которой в каюте быть не должно. Стоящий, чьи ступни в 3.9 м от оси, до них не дотягивается. Прогон проверяет крен и инерцию; строки увода и `footing` в нём уликами не считаются.

Ожидается:

- `Dock: undocked.` в первых кадрах;
- `Thruster array ready: 24 thrusters, 50000 kg, inertia (533333, 533333, 533333)` — перестроение при отстыковке. **Это число обязано быть здесь на обеих частотах**: `m(a²+b²)/12` для куба 8 м в 50 т;
- последняя строка `Flight:` даёт `ang roll/pitch/yaw (180.0 ~0 ~0) deg/s`.

**Крен обязан выйти на 180.0 и на 400 fps.** Это единственная проверка того, что `SleepFamily = Custom` и `CustomSleepThresholdMultiplier = 0` пережили выключение и включение симуляции: поля живут на `FBodyInstance` (`BodyInstance.h:410`, `:696`), но переживают ли они пересоздание состояния физики — вопрос к прогону, а не к заголовку. Крен `1.7 deg/s` вместо `180.0` означает, что дефект сна вернулся через отстыковку, и чинится он там же, где чинился: в конструкторе `ASFTestShip`, куда придётся вернуть оба поля после `SetSimulatePhysics(true)` в `RequestUndock`.

- [ ] **Step 6: Прогон (в) — цикл «пристыкован → свободен → пристыкован», обе частоты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.Dock 2, sf.FlightLog 1" -log
```

и то же самое с `-fps=400`.

Ожидается по порядку:

- `Dock: undocked.`
- примерно секундой позже `Dock: capturing, 0.0 m, 0.0 m/s, fwd 0.0 deg, up 0.0 deg` — корабль никуда не улетел, потому что тяги никто не просил, и заново стыкуется с нулевого расстояния;
- `Dock: docked.`
- в секундных записях: `Dock: docked` → `Dock: free | port 0.0 m at (+0.0 +0.0 +0.0) ship, 0.0 m/s, … -- CAN DOCK` → `Dock: docked`; одна из секундных записей может прийтись на доводку и прочитаться `Dock: capturing 0.N of 1.0 s` — зависит от того, куда попала секунда, и не требуется;
- **ни одной** строки `Frame handover` — границу объёмов никто не пересекал (сам прибор появляется в задаче 5; здесь проверяется, что цикл проходит целиком, а после задачи 5 этот же прогон повторяется и подтверждает отсутствие строки);
- `skipped carries 0` во всех строках увода и **ни одной** `Carry NOT RUNNING`. Перенос переживает смену расписания в обе стороны — это то самое, что §8 требует проверить, а не вывести.

- [ ] **Step 7: Коммит**

```bash
git add Source/StarForge/Public/SFLoopGameMode.h Source/StarForge/Private/SFLoopGameMode.cpp
git commit -F - <<'MSG'
feat(loop): a second rig, where the ship starts attached to a station

L_FrameSpike does not move a single default. Every figure this project has recorded was
taken on it, and changing what it spawns would devalue all of them at once, so the loop
lives on its own map with its own game mode and the default map stays where it was.

The ship is docked before Super::StartPlay, and the ordering is the whole trick. Super
is what begins play on the world and spawns the pawn; docking after it would put the
pawn into a world where the ship still sits at the origin, inside the station's interior
volume as well as its own, with nothing deciding which of the two claims him. Docking
first means there is no frame in which the ship is adrift, and the immediate dock
resolves its own ports because BeginPlay has not happened yet.

The docked pose is derived from the two ports rather than authored here, so moving a
wall moves the docked ship with it; the only place that constant exists is a test.

This is the one place in the project that reaches a console variable by name, because it
writes one that belongs to another file. The miss is an error in the log rather than
silence: the rule that a variable is declared where it is read exists because a failed
lookup returns null and a missing value reads as zero, and a zero here would leave the
loop rig flying the checkpoint manoeuvre at 400 m/s beside a station.

The two actors are given names so that the frame-handover line reads "Station -> Ship"
rather than "SFTestStation_0 -> SFTestShip_0". It is the line a person reads at the
moment of the transition, and nothing else depends on those names.
MSG
```

---

### Task 5: Объёмы, передача системы отсчёта, прибор непрерывности и перекрёстная проверка

Главная задача плана. Человек переходит из одного носителя в другой ногами; передача меряется в сантиметрах; неверная привязка кричит.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFInteriorVolumeComponent.h`
- Create: `Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`, `Source/StarForge/Private/Ship/SFTestShip.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestStation.h`, `Source/StarForge/Private/Ship/SFTestStation.cpp`
- Modify: `Source/StarForge/Public/Player/SFCharacter.h`, `Source/StarForge/Private/Player/SFCharacter.cpp`
- Modify: `Source/StarForge/Public/Player/SFShipboardMovementComponent.h`, `Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp`
- Modify: `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`

**Interfaces:**
- Consumes: `USFShipFrameComponent`, `USFShipboardMovementComponent::SetShipFrame`, `USFPilotStationComponent`, `USFDockingComponent::Toggle()`, `USFDockingComponent::{GetState, GetLastApproach, GetPortOffsetShipLocalMetres, GetLastRefusal, CanDockNow, HasStation, GetCaptureElapsedSeconds, GetCaptureSeconds, RefusalName}`.
- Produces:
  - `USFInteriorVolumeComponent : UBoxComponent` с `USFShipFrameComponent* GetOwnerFrame() const`, `bool ContainsPoint(const FVector& WorldPoint) const`.
  - `ASFTestShip::GetInterior() const -> USFInteriorVolumeComponent*`, `ASFTestStation::GetInterior() const -> USFInteriorVolumeComponent*`.
  - `ASFCharacter::BindFrame(USFShipFrameComponent* Frame)` — публичный.
  - `USFShipboardMovementComponent`: `FString GetFrameName() const`, `double GetLastHandoverJumpCm() const`, `double GetLastHandoverGravityTurnDeg() const`, `const FString& GetLastHandoverText() const`, `int32 GetFrameMismatchFrames() const`.
  - Консольная переменная `sf.BindFrame` (`FString`, умолчание пусто), объявлена в `SFCharacter.cpp`.

**Почему вход привязывает, а выход не отвязывает.** Между объёмами лежит десять сантиметров стены — корабельный кончается на плоскости люка, станционный начинается на внешнем торце тоннеля. Отвязка на выходе сделала бы человека на этих десяти сантиметрах ничьим, то есть стоящим в мировых координатах рядом с кораблём, который вправе уйти на 400 м/с. Вход — единственное событие, которое что-то меняет, и поэтому «выход из одного всегда есть вход в другой» (§8) выполняется с точностью до толщины стены, а не буквально.

**Почему привязка при появлении делается тремя ступенями.** Учёт перекрытий на `BeginPlay` зависит от порядка регистрации компонентов, а ответ на вопрос «в чьей я системе отсчёта» зависеть от него не имеет права. Первая ступень — ответ самого движка, она бесплатна; вторая — точечная проверка по всем объёмам, детерминированная; третья — прежний перебор акторов, с предупреждением, потому что человек вне всех объёмов всё равно должен кем-то нестись, и громко.

**Почему меряется дельта переноса, а не разница положений.** Между привязкой и первым переносом человек мог сам шагнуть, и его шаг — его. Дельта самого переноса — ровно та часть, которую вызвала передача, и при неподвижных друг относительно друга носителях она обязана быть нулём. Это и есть «визуально незаметно», выраженное числом (§8).

**Чего этот прибор не может, и что §8 теперь говорит прямо (R11).** Первая редакция врезки ждала от неверной привязки «ненулевого прыжка» в той же строке. Механизм этого дать не может: `SetShipFrame` пересевает `LastFrameTransform` в момент привязки, поэтому первый перенос после привязки к неподвижной станции равен нулю независимо от того, верна привязка или нет. Неверная привязка к станции проявляется **накоплением**: сначала предупреждение перекрёстной проверки, затем `Carry NOT RUNNING`, когда пол уезжает из-под ног; отрицательные контроли (б) и (в) ниже требуют именно этих двух, а строку `Frame handover` читают как справочную. У прибора передачи контроль свой и обратный: привязка к **движущемуся** носителю с неподвижного пола — первый перенос обязан сдвинуть человека на кадр хода носителя. Headless до палубы станции не доходит, поэтому этот контроль делается руками: запуск B4–B6 в runbook задачи 6.

**Почему перекрёстная проверка защёлкнута «раз в кадр».** `UpdateBasedMovement` зовётся один раз за кадр при несимулируемой опоре и дважды при симулируемой (`CharacterMovementComponent.cpp` 2476–2510). Без защёлки порог «десять кадров» означал бы десять кадров у пристыкованного корабля и пять у летящего — прибор, чей порог меняется вместе с состоянием стыковки, то есть ровно тот класс дефекта, который этот проект уже ловил в удвоенных часах.

- [ ] **Step 1: Написать объём интерьера**

`Source/StarForge/Public/Ship/SFInteriorVolumeComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SFInteriorVolumeComponent.generated.h"

class USFShipFrameComponent;

/**
 * The inside of one carrier, as a volume: whoever walks in is bound to its frame.
 *
 * Entering binds; leaving does NOT unbind, and that asymmetry is the design. The two
 * volumes abut across the ship's hatch plane with the wall's thickness between them --
 * ten centimetres in which neither claims anybody. Unbinding on exit would make a person
 * nobody's for those ten centimetres, which means standing in world coordinates beside a
 * ship entitled to leave at 400 m/s. Binding on entry alone means the last binding holds
 * until a new one replaces it, and spec 8's "leaving one is always entering the other"
 * is true to within the thickness of a wall.
 *
 * It knows nothing about docking, and must not: an airlock handover and a boarding
 * action are the same event, which is what spec 8 says this mechanism buys.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFInteriorVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	USFInteriorVolumeComponent();

	virtual void BeginPlay() override;

	/** The frame this volume hands out; the owner's, resolved once at BeginPlay. */
	USFShipFrameComponent* GetOwnerFrame() const { return OwnerFrame; }

	/**
	 * Whether a world point is inside this box, computed rather than looked up.
	 *
	 * The engine's overlap bookkeeping is the right answer at any settled moment and an
	 * unreliable one at BeginPlay, where it depends on the order components registered.
	 * "Whose reference frame am I in" is not allowed to depend on that, so the character's
	 * first binding has this to fall back on.
	 */
	bool ContainsPoint(const FVector& WorldPoint) const;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY()
	TObjectPtr<USFShipFrameComponent> OwnerFrame;
};
```

`Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp`:

```cpp
#include "Ship/SFInteriorVolumeComponent.h"

#include "GameFramework/Actor.h"
#include "Player/SFCharacter.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"

USFInteriorVolumeComponent::USFInteriorVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Query only, and it answers to pawns alone. A volume that blocked anything would be
	// a second, invisible hull inside the real one; a volume that overlapped everything
	// would fire on every panel, marker and drone that passed through it.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldStatic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);

	// Not drawn, not hit by traces, and not something the eye or the aim can find.
	SetHiddenInGame(true);
	SetCanEverAffectNavigation(false);
}

void USFInteriorVolumeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		OwnerFrame = Owner->FindComponentByClass<USFShipFrameComponent>();
	}

	if (!OwnerFrame)
	{
		// A volume with no frame behind it would silently do nothing: people would walk
		// through it and stay bound to whatever they were bound to before, which is the
		// exact symptom of a handover that never fires.
		UE_LOG(LogStarForge, Error,
			TEXT("Interior volume on %s found no frame component; nobody entering it will be bound."),
			GetOwner() ? *GetOwner()->GetActorNameOrLabel() : TEXT("nothing"));
		return;
	}

	OnComponentBeginOverlap.AddDynamic(this, &USFInteriorVolumeComponent::HandleBeginOverlap);
}

bool USFInteriorVolumeComponent::ContainsPoint(const FVector& WorldPoint) const
{
	// Unscaled on both sides: InverseTransformPosition divides the scale out, so the
	// local point is in unscaled units and the unscaled extent is what it belongs beside.
	// Mixing the two is how a box quietly grows by whatever its owner is scaled to.
	const FVector Local = GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector Extent = GetUnscaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z;
}

void USFInteriorVolumeComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (ASFCharacter* Character = Cast<ASFCharacter>(OtherActor))
	{
		Character->BindFrame(OwnerFrame);
	}
}
```

- [ ] **Step 2: Повесить объёмы на оба носителя**

В `Source/StarForge/Public/Ship/SFTestShip.h` — прямое объявление `class USFInteriorVolumeComponent;`, геттер и член:

```cpp
	/** The cabin as a volume. Whoever walks in rides this ship's frame. */
	USFInteriorVolumeComponent* GetInterior() const { return Interior; }
```

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFInteriorVolumeComponent> Interior;
```

В конструкторе `ASFTestShip` (после панелей, чтобы `DeckTopZ` и прочие уже были посчитаны):

```cpp
	// The cabin, out to the inner faces of the panels: half-extent 390 on every axis,
	// which along +Y is exactly the hatch plane. Everything inside the hull is the ship;
	// the tunnel and beyond are the station (spec 8), and the two volumes meet at this
	// plane with the wall's twenty centimetres between them -- ten to each side of the
	// pair's midline. Nobody is claimed in that gap, and the last binding holds through it.
	Interior = CreateDefaultSubobject<USFInteriorVolumeComponent>(TEXT("Interior"));
	Interior->SetupAttachment(Hull);
	Interior->SetRelativeLocation(FVector::ZeroVector);
	Interior->SetBoxExtent(FVector(Half - PanelThickness * CubeEdgeCm * 0.5));
```

и включить `#include "Ship/SFInteriorVolumeComponent.h"`.

В `Source/StarForge/Public/Ship/SFTestStation.h` — то же прямое объявление, геттер `GetInterior()` и член `Interior`. В конструкторе `ASFTestStation`, после тоннеля:

```cpp
	// The box plus the tunnel: Y from -600 (the tunnel's outboard mouth) to +400, so the
	// centre is at -100 and the half-extent 500. X and Z reach the inner faces of the
	// walls. The volume is wider than the tunnel along X and Z, which costs nothing --
	// the extra is inside solid panels, where a pawn cannot be.
	//
	// It does not touch the ship's. Docked, the ship's cabin ends at station-local -610
	// and this begins at -600: ten centimetres of wall, deliberately unclaimed.
	// The span runs from the tunnel's outboard mouth at -600 to the far wall at +400, so it
	// is 2*HalfYCm + TunnelLengthCm long -- a thousand centimetres, half-extent 500, centred
	// at -100. Writing it as (HalfYCm + TunnelLengthCm)/2 gives 300 and looks close enough to
	// pass a reading; it leaves the whole tunnel and two metres of the station's own deck
	// inside no volume at all, which is a man who stays bound to the ship for four metres
	// after walking out of it.
	const double InteriorHalfYCm = (2.0 * Station::HalfYCm + Station::TunnelLengthCm) * 0.5;  // 500
	const double InteriorCentreYCm = Station::HalfYCm - InteriorHalfYCm;                      // -100

	Interior = CreateDefaultSubobject<USFInteriorVolumeComponent>(TEXT("Interior"));
	Interior->SetupAttachment(Root);
	Interior->SetRelativeLocation(FVector(0.0, InteriorCentreYCm, 0.0));
	Interior->SetBoxExtent(FVector(
		Station::HalfXCm - Station::PanelThickness * Station::CubeEdgeCm * 0.5,
		InteriorHalfYCm,
		Station::HalfZCm - Station::PanelThickness * Station::CubeEdgeCm * 0.5));
```

- [ ] **Step 3: Прибор передачи и перекрёстная проверка в компоненте движения**

В `Source/StarForge/Public/Player/SFShipboardMovementComponent.h`, к публичным геттерам:

```cpp
	/** The bound frame's owner, by name, or "none". What the readout prints. */
	FString GetFrameName() const;

	/** How far the first carry after the last handover moved him, in centimetres. */
	double GetLastHandoverJumpCm() const { return LastHandoverJumpCm; }

	/** By how much the deck's down turned across that handover, in degrees. */
	double GetLastHandoverGravityTurnDeg() const { return LastHandoverGravityTurnDeg; }

	/** The whole handover as one phrase, empty before the first one. */
	const FString& GetLastHandoverText() const { return LastHandoverText; }

	/**
	 * Consecutive frames on which the floor under him belonged to a different carrier
	 * than the frame he is bound to. Zero is the only healthy value.
	 *
	 * An instrument that costs one line, because the engine already knows what he is
	 * standing on: the movement base has an owner, and that owner either carries the
	 * frame he rides or it does not.
	 */
	int32 GetFrameMismatchFrames() const { return MismatchFrames; }
```

и к приватным полям:

```cpp
	/** Set by SetShipFrame, consumed by the first carry that follows it. */
	bool bHandoverPending = false;
	FString PendingHandoverOldName;
	FString PendingHandoverNewName;
	double PendingHandoverGravityTurnDeg = 0.0;

	double LastHandoverJumpCm = 0.0;
	double LastHandoverGravityTurnDeg = 0.0;
	FString LastHandoverText;

	/** Cross-check state; see UpdateFrameCrossCheck. */
	int32 MismatchFrames = 0;
	uint64 LastCrossCheckFrame = TNumericLimits<uint64>::Max();
	double SecondsSinceMismatchLog = 0.0;
```

и объявление приватного метода рядом с `DetectCarryGap`:

```cpp
	/**
	 * Compares the owner of the floor he is standing on against the frame he is bound to,
	 * and complains when the two disagree for ten frames running.
	 *
	 * Latched to one call per frame like the drift measurement, and for a related reason:
	 * the engine calls UpdateBasedMovement once a frame for a base that is not simulating
	 * and twice for one that is (CharacterMovementComponent.cpp 2476-2510). Unlatched,
	 * "ten frames" would mean ten frames docked and five flying -- an instrument whose
	 * threshold moves with the state of the ship.
	 */
	void UpdateFrameCrossCheck(float DeltaSeconds);
```

- [ ] **Step 4: Реализовать прибор передачи**

В `Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp`.

Включить `#include "GameFramework/Actor.h"` (если не включён) и переписать начало `SetShipFrame`:

```cpp
void USFShipboardMovementComponent::SetShipFrame(USFShipFrameComponent* NewFrame)
{
	USFShipFrameComponent* OldFrame = ShipFrame.Get();

	// A handover is a change of carrier, not a rebinding to the same one. Walking back
	// into a volume he never left fires the overlap again, and treating that as a
	// handover would reset the drift instrument every time he crossed his own doorway.
	if (OldFrame && NewFrame && OldFrame != NewFrame)
	{
		// The angle between the two decks' down vectors. It is the number spec 8 means by
		// "the gravity direction changes only by the leftover tolerance": mated ports
		// hold both +Z parallel to within the up-angle limit, so a person walking through
		// the airlock should read a fraction of a degree here and never ten.
		const FVector OldDown = -OldFrame->GetFrameTransform().GetUnitAxis(EAxis::Z);
		const FVector NewDown = -NewFrame->GetFrameTransform().GetUnitAxis(EAxis::Z);

		PendingHandoverOldName = OldFrame->GetOwner()
			? OldFrame->GetOwner()->GetActorNameOrLabel() : TEXT("none");
		PendingHandoverNewName = NewFrame->GetOwner()
			? NewFrame->GetOwner()->GetActorNameOrLabel() : TEXT("none");
		PendingHandoverGravityTurnDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(OldDown, NewDown), -1.0, 1.0)));
		bHandoverPending = true;
	}

	ShipFrame = NewFrame;

	// ... остальная часть функции без изменений ...
```

и добавить в конец `SetShipFrame`, рядом со сбросом защёлкнутого результата:

```cpp
	// The cross-check belongs to the binding it was taken against.
	MismatchFrames = 0;
	LastCrossCheckFrame = TNumericLimits<uint64>::Max();
	SecondsSinceMismatchLog = 0.0;
```

Добавить `GetFrameName`:

```cpp
FString USFShipboardMovementComponent::GetFrameName() const
{
	const USFShipFrameComponent* Frame = ShipFrame.Get();
	if (!Frame || !Frame->GetOwner())
	{
		return TEXT("none");
	}

	return Frame->GetOwner()->GetActorNameOrLabel();
}
```

В `CarryWithFrame()`, сразу **после** `MoveUpdatedComponent(...)` и до вычисления `FrameDelta`:

```cpp
	// The instrument spec 8 asks for, and it measures the carry's own delta rather than
	// the difference between two positions. Between the binding and this call the man may
	// have taken a step of his own, and his step is his; the carry's delta is exactly the
	// part the handover caused.
	//
	// With both carriers docked it is required to be zero, because neither is moving with
	// respect to the other -- which is "visually unnoticeable" written as a number. It is
	// zero for a WRONG binding to a still carrier too, and that is why the negative
	// controls are judged on the cross-check and on the carry, not on this line.
	if (bHandoverPending)
	{
		LastHandoverJumpCm = Delta.Size();
		LastHandoverGravityTurnDeg = PendingHandoverGravityTurnDeg;
		LastHandoverText = FString::Printf(TEXT("%s -> %s, %.3f cm, %.1f deg"),
			*PendingHandoverOldName, *PendingHandoverNewName,
			LastHandoverJumpCm, LastHandoverGravityTurnDeg);

		UE_LOG(LogStarForge, Display,
			TEXT("Frame handover: %s -> %s, first carry moved him %.3f cm, gravity turned %.1f deg"),
			*PendingHandoverOldName, *PendingHandoverNewName,
			LastHandoverJumpCm, LastHandoverGravityTurnDeg);

		bHandoverPending = false;
	}
```

Добавить перекрёстную проверку и вызвать её из `UpdateBasedMovement` сразу после `DetectCarryGap()`:

```cpp
void USFShipboardMovementComponent::UpdateFrameCrossCheck(float DeltaSeconds)
{
	const uint64 ThisFrame = GFrameCounter;
	if (ThisFrame == LastCrossCheckFrame)
	{
		return;
	}
	LastCrossCheckFrame = ThisFrame;

	// The engine already knows what he is standing on. The base is a component, its owner
	// is an actor, and that actor either carries the frame he rides or it does not.
	const UActorComponent* Base = Cast<UActorComponent>(GetMovementBaseObject());
	const AActor* BaseOwner = Base ? Base->GetOwner() : nullptr;
	const USFShipFrameComponent* BaseFrame =
		BaseOwner ? BaseOwner->FindComponentByClass<USFShipFrameComponent>() : nullptr;

	// No base, or a base whose owner carries no frame at all, is not a mismatch. The
	// first is a carry failure and has two instruments of its own; the second is a crate
	// or a drone, and standing on one says nothing about which ship you belong to.
	if (!BaseFrame || BaseFrame == ShipFrame.Get())
	{
		MismatchFrames = 0;
		SecondsSinceMismatchLog = 0.0;
		return;
	}

	++MismatchFrames;
	SecondsSinceMismatchLog += DeltaSeconds;

	// The first line at ten frames -- a sixth of a second, long enough that a single
	// frame of tick order cannot produce it -- and then one a second for as long as it
	// lasts. The same reason Carry NOT RUNNING repeats: a single line at frame ten does
	// not say that the disagreement has been going on for a minute.
	if (MismatchFrames == 10 || SecondsSinceMismatchLog >= 1.0)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Standing on %s's floor while bound to %s's frame for %d frames. ")
			TEXT("One of the two is wrong, and the carry is following the frame."),
			BaseOwner ? *BaseOwner->GetActorNameOrLabel() : TEXT("nothing"),
			*GetFrameName(), MismatchFrames);
		SecondsSinceMismatchLog = 0.0;
	}
}
```

- [ ] **Step 5: `BindFrame`, привязка по объёму, клавиша `F` и `sf.BindFrame`**

В `Source/StarForge/Public/Player/SFCharacter.h`:

```cpp
	/**
	 * Rides the given frame from now on. Called by an interior volume on entry and by
	 * sf.BindFrame; nothing else has business changing it.
	 */
	void BindFrame(USFShipFrameComponent* Frame);
```

```cpp
	/** F, from the pilot's seat: docks or undocks. Says so from anywhere else. */
	void PollDockInput();

	/** sf.BindFrame, acted on when the value changes. The negative control's door. */
	void PollBindFrameCommand();
```

```cpp
	/** Edge detection for the dock key: held is not pressed. */
	bool bDockWasDown = false;

	/** Last value seen on sf.BindFrame, so it acts on a change and not on a level. */
	FString LastBindFrameCommand;
```

и прямое объявление `class USFShipFrameComponent;`.

В `Source/StarForge/Private/Player/SFCharacter.cpp` — включить `#include "Ship/SFDockingComponent.h"`, `#include "Ship/SFInteriorVolumeComponent.h"`, `#include "Ship/SFPilotStationComponent.h"`, `#include "Ship/SFTestShip.h"`, `#include "Ship/SFTestStation.h"`, и добавить переменную в безымянный `namespace`:

```cpp
namespace
{
	/**
	 * Rebinds the occupant to a named frame regardless of where he is standing.
	 *
	 * The negative control for the handover, and the same trick as sf.ShipGravity 0 for
	 * the carry. With the ship docked it is a legitimate handover and everything stays
	 * quiet; with the ship undocked and under thrust it is a knowingly wrong binding, and
	 * the cross-check has to complain within ten frames and the carry has to stop shortly
	 * after. An instrument that stays calm through that is blind.
	 *
	 * Declared here because this is the file that reads it. Empty means "no command", and
	 * it acts on a change of the value rather than on its level.
	 */
	TAutoConsoleVariable<FString> CVarBindFrame(
		TEXT("sf.BindFrame"), TEXT(""),
		TEXT("ship|station: bind the occupant to that carrier's frame wherever he is standing. Empty: no command.\n")
		TEXT("A negative control. Bound to the wrong carrier in flight, the cross-check must warn and the carry must stop."),
		ECVF_Default);
}
```

`BindFrame` и привязка в `BeginPlay`:

```cpp
void ASFCharacter::BindFrame(USFShipFrameComponent* Frame)
{
	USFShipboardMovementComponent* Movement = GetShipboardMovement();
	if (!Movement || !Frame)
	{
		return;
	}

	// Re-entering the same volume must not restart the drift instrument. The volume fires
	// on every entry, and a man stepping in and out of his own doorway would otherwise
	// erase a good measurement with each step.
	if (Movement->GetShipFrame() == Frame)
	{
		return;
	}

	Movement->SetShipFrame(Frame);
}
```

Заменить тело привязки в `BeginPlay` (блок «The rig has exactly one ship…») на:

```cpp
	if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		USFShipFrameComponent* FoundFrame = nullptr;

		// First, the engine's own answer. Free, and right at any settled moment.
		TArray<UPrimitiveComponent*> Overlaps;
		GetCapsuleComponent()->GetOverlappingComponents(Overlaps);
		for (UPrimitiveComponent* Component : Overlaps)
		{
			if (const USFInteriorVolumeComponent* Volume = Cast<USFInteriorVolumeComponent>(Component))
			{
				FoundFrame = Volume->GetOwnerFrame();
				break;
			}
		}

		// Second, the same question asked geometrically. Overlap bookkeeping at BeginPlay
		// depends on the order components registered, and "whose reference frame am I in"
		// is not allowed to depend on that. A point in a box is the same answer without
		// the timing.
		if (!FoundFrame)
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				TArray<USFInteriorVolumeComponent*> Volumes;
				It->GetComponents<USFInteriorVolumeComponent>(Volumes);
				for (USFInteriorVolumeComponent* Volume : Volumes)
				{
					if (Volume->ContainsPoint(GetActorLocation()))
					{
						FoundFrame = Volume->GetOwnerFrame();
						break;
					}
				}
				if (FoundFrame)
				{
					break;
				}
			}
		}

		if (FoundFrame)
		{
			Movement->SetShipFrame(FoundFrame);
		}
		else
		{
			// Third and last: any frame at all, loudly. A man outside every volume still
			// has to be carried by something, and the warning is what says the binding was
			// a guess rather than an answer.
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (USFShipFrameComponent* Frame = It->FindComponentByClass<USFShipFrameComponent>())
				{
					Movement->SetShipFrame(Frame);
					UE_LOG(LogStarForge, Warning,
						TEXT("SFCharacter bound to %s by search, not by volume: he spawned outside every ")
						TEXT("interior volume. The binding is a guess."),
						*It->GetActorNameOrLabel());
					FoundFrame = Frame;
					break;
				}
			}
		}

		if (!FoundFrame)
		{
			// A rig that silently measures nothing is worse than one that fails.
			UE_LOG(LogStarForge, Warning,
				TEXT("SFCharacter: found no USFShipFrameComponent in the world. The occupant will ")
				TEXT("stand in world space while the ship flies away, and every drift reading will be ")
				TEXT("a zero that means nothing."));
		}
	}
```

Клавиша `F` и переменная — добавить два вызова в `Tick` после `PollInteract()`:

```cpp
	PollDockInput();
	PollBindFrameCommand();
```

Оба метода:

```cpp
void ASFCharacter::PollDockInput()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// On the edge, like the interact key: held, it would dock and undock every frame.
	const bool bDown = PC->IsInputKeyDown(EKeys::F);
	if (bDown && !bDockWasDown)
	{
		// Docking is commanded from the pilot's seat and from nowhere else. Spec 8 puts it
		// there because it is a flight decision: the pilot is the one who can see the
		// approach, and a passenger who could dock from the back of the cabin would be
		// commanding a manoeuvre he has no instrument for.
		USFPilotStationComponent* Pilot = Cast<USFPilotStationComponent>(GetSeatedStation());
		if (!Pilot)
		{
			// Said out loud, because a key that does nothing is otherwise the same as a
			// key that is not bound at all -- and this one was pressed on purpose.
			UE_LOG(LogStarForge, Display, TEXT("Docking is commanded from the pilot's seat."));
		}
		else if (USFDockingComponent* Docking = Pilot->GetOwner()->FindComponentByClass<USFDockingComponent>())
		{
			Docking->Toggle();
		}
		else
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("F pressed in the pilot's seat, but this ship has no docking component."));
		}
	}
	bDockWasDown = bDown;
}

void ASFCharacter::PollBindFrameCommand()
{
	const FString Command = CVarBindFrame.GetValueOnGameThread();
	if (Command == LastBindFrameCommand)
	{
		return;
	}
	LastBindFrameCommand = Command;

	if (Command.IsEmpty())
	{
		return;
	}

	USFShipFrameComponent* Frame = nullptr;
	if (Command.Equals(TEXT("ship"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<ASFTestShip> It(GetWorld()); It; ++It)
		{
			Frame = It->GetFrameComponent();
			break;
		}
	}
	else if (Command.Equals(TEXT("station"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<ASFTestStation> It(GetWorld()); It; ++It)
		{
			Frame = It->GetFrameComponent();
			break;
		}
	}

	if (!Frame)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("sf.BindFrame '%s': no such carrier in this level. Nothing was rebound."), *Command);
		return;
	}

	BindFrame(Frame);

	UE_LOG(LogStarForge, Display,
		TEXT("Frame bound by console to %s (sf.BindFrame). This ignores where he is standing, which is "
			 "the point: it is a negative control."),
		Frame->GetOwner() ? *Frame->GetOwner()->GetActorNameOrLabel() : TEXT("nothing"));
}
```

- [ ] **Step 6: Две строки на панель**

В `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp` поднять `PanelHeight` до `820.0f`, дописав к его комментарию:

```cpp
	 * Raised again to 820 for the frame and dock lines: two more at 22 px, plus the
	 * margin a line needs so it does not sit on the panel's edge. A line falling off the
	 * bottom is the defect this panel exists to prevent, and it has now been prevented
	 * three times by the same arithmetic.
```

и включить `#include "Ship/SFDockingComponent.h"`.

В `DrawHUD`, сразу после строки `ship-local position` и до блока `if (Frame && Ship)`:

```cpp
	// Who carries him, and what the last handover cost. Yellow until there has been one,
	// because "no handover yet" and "a handover of zero centimetres" are the same text
	// otherwise and mean opposite things. Red when the floor under him has belonged to
	// somebody else for ten frames: that outranks every figure above it, since the carry
	// is following the frame and the frame is wrong.
	const int32 Mismatch = Movement->GetFrameMismatchFrames();
	const bool bNoHandoverYet = Movement->GetLastHandoverText().IsEmpty();
	DrawLine(
		FString::Printf(TEXT("frame   bound to %s   last handover: %s"),
			*Movement->GetFrameName(),
			bNoHandoverYet ? TEXT("none yet") : *Movement->GetLastHandoverText()),
		Y,
		(Mismatch >= 10) ? FLinearColor::Red : (bNoHandoverYet ? FLinearColor::Yellow : FLinearColor::White));

	// The dock line is about the ship, not about whichever carrier he happens to be
	// standing in: a man on the station still needs to see whether his ship is attached
	// to it. Found the same way the flight block finds it, for the same reason.
	const ASFTestShip* DockShip = Cast<ASFTestShip>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFTestShip::StaticClass()));
	const USFDockingComponent* Docking = DockShip ? DockShip->GetDocking() : nullptr;

	if (!Docking)
	{
		DrawLine(TEXT("dock    -- no ship with a docking component in the level"), Y, FLinearColor::Red);
	}
	else if (!Docking->HasStation())
	{
		DrawLine(TEXT("dock    free -- NO STATION in this level"), Y, FLinearColor::Red);
	}
	else
	{
		switch (Docking->GetState())
		{
		case ESFDockingState::Docked:
			DrawLine(TEXT("dock    docked"), Y, FLinearColor::Yellow);
			break;
		case ESFDockingState::Capturing:
			DrawLine(FString::Printf(TEXT("dock    capturing %.1f of %.1f s"),
				Docking->GetCaptureElapsedSeconds(), Docking->GetCaptureSeconds()),
				Y, FLinearColor(1.0f, 0.45f, 0.0f));
			break;
		default:
		{
			const StarForge::Docking::FDockingApproach Approach = Docking->GetLastApproach();
			const FVector At = Docking->GetPortOffsetShipLocalMetres();
			const bool bCanDock = Docking->CanDockNow();

			// Green only when the key would be accepted this instant. A window that
			// reported itself open while the command would refuse is the same defect as a
			// turret at its stop reporting itself on target.
			DrawLine(
				FString::Printf(TEXT("dock    free | port %.1f m at (%+.1f %+.1f %+.1f) ship  %.1f m/s  fwd %.1f  up %.1f  -- %s"),
					Approach.Distance, At.X, At.Y, At.Z, Approach.SpeedMetresPerSec,
					FMath::RadiansToDegrees(Approach.ForwardAngleRad),
					FMath::RadiansToDegrees(Approach.UpAngleRad),
					bCanDock ? TEXT("CAN DOCK")
						: *FString::Printf(TEXT("out of window: %s"),
							USFDockingComponent::RefusalName(Docking->GetLastRefusal()))),
				Y,
				bCanDock ? FLinearColor::Green : FLinearColor::White);
			break;
		}
		}
	}
```

- [ ] **Step 7: Собрать и прогнать три проверки на обеих частотах**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Тесты: по-прежнему **65** / **0**.

**(а) Старт: одна передача, ноль сантиметров.** `-fps=60` и `-fps=400`:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log
```

Ожидается **ровно одна** строка:

```
Frame handover: Station -> Ship, first carry moved him 0.000 cm, gravity turned 0.0 deg
```

Пешка появляется в начале координат — внутри станции — и режим переносит её в каюту; это штатное пересечение границы при спавне, а не дефект. Кроме неё: ни одной `Standing on`, ни одной `Carry NOT RUNNING`, `skipped carries 0`, `peak drift 0.000 cm` при `measured` больше 55 с.

Считать: `grep -c "Frame handover" Saved/Logs/StarForge.log` → `1`; `grep -c "Standing on" Saved/Logs/StarForge.log` → `0`.

**(б) Отрицательный контроль №1: неверная привязка без движения.** Обе частоты:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.BindFrame station" -log
```

Ожидается:

- `Frame bound by console to Station (sf.BindFrame)`;
- вторая строка `Frame handover: Ship -> Station, first carry moved him 0.000 cm, gravity turned 0.0 deg` — ноль, потому что оба носителя стоят;
- **первая** `Standing on Ship's floor while bound to Station's frame for 10 frames` в течение первых кадров после команды, и затем примерно по одной в секунду до конца прогона;
- `Shipboard drift … peak drift 0.000 cm … skipped carries 0` — увод остаётся нулём.

Это существо контроля: **прибор рассогласования видит ошибку привязки там, где ни увод, ни перенос её увидеть не могут**, потому что оба носителя неподвижны и относительно них человек стоит одинаково хорошо. Отсутствие `Standing on` здесь — дефект прибора, а не хороший результат.

**(в) Отрицательный контроль №2: неверная привязка в полёте.** Обе частоты:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.BindFrame station, sf.Dock 0, sf.Intent.X 1" -log
```

Корабль уходит из-под человека, привязанного к станции. Обязаны сработать две вещи:

1. `Standing on Ship's floor while bound to Station's frame` — в первые кадры, и повторно раз в секунду, пока у него ещё есть пол;
2. `Carry NOT RUNNING: N frames since the occupant was last carried (mode Falling, base none)` — примерно через секунду, когда корабль ушёл на восемь метров и пола под ним не осталось. Тяга 800 кН на 50 т даёт 16 м/с²: половина каюты, 4 м, проходится за `sqrt(2·4/16) = 0.71` с.

**Увод в строке `Shipboard drift` при этом расти не обязан, и это выведено, а не наблюдение:** увод меряется в системе отсчёта, к которой человек привязан, а привязан он к неподвижной станции и относительно неё не двигается. Судить контроль по уводу здесь нельзя. **Если не сработало ни предупреждение, ни `Carry NOT RUNNING` — прибор слеп, и это находка первого ранга:** значит перекрёстная проверка не видит владельца опоры, либо перенос продолжает работать там, где носителя под ногами уже нет.

**(г) Повторить прогон (в) задачи 4** — `sf.Dock 2` — и убедиться, что строк `Frame handover` в нём **ноль**: цикл стыковки границ объёмов не пересекает, и передача, появившаяся в этом цикле, означала бы, что объёмы наложились.

- [ ] **Step 8: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFInteriorVolumeComponent.h Source/StarForge/Private/Ship/SFInteriorVolumeComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp Source/StarForge/Public/Ship/SFTestStation.h Source/StarForge/Private/Ship/SFTestStation.cpp Source/StarForge/Public/Player/SFCharacter.h Source/StarForge/Private/Player/SFCharacter.cpp Source/StarForge/Public/Player/SFShipboardMovementComponent.h Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp
git commit -F - <<'MSG'
feat(player): hand a man from one reference frame to another, and measure the seam

Binding is by volume now, not by finding the first frame in the world. Entering binds;
leaving deliberately does not unbind. The two volumes abut across the ship's hatch plane
with the wall between them, and unbinding on exit would make a person nobody's for those
ten centimetres -- standing in world coordinates beside a ship entitled to leave at 400
m/s. The last binding holds until a new one replaces it.

The first binding is taken three ways in order: the engine's overlaps, then a point-in-box
test over every volume, then any frame at all with a warning. The middle step exists
because overlap bookkeeping at BeginPlay depends on the order components registered, and
"whose reference frame am I in" is not allowed to depend on that.

The handover instrument measures the carry's own delta, not the difference between two
positions: between the binding and the first carry the man may have taken a step of his
own, and his step is his. With both carriers docked the figure is required to be zero,
which is spec 8's "visually unnoticeable" written as a number.

It is zero for a wrong binding to a still carrier too, and cannot be otherwise:
SetShipFrame reseeds the last frame transform as it binds, so the first carry after any
binding to something motionless is zero whether the binding was right or wrong. A wrong
binding to the station shows as accumulation instead -- the cross-check first, then the
carry stopping -- and the instrument's own negative control is the reverse binding, to a
carrier that is moving, where the first carry has to move him by one frame of that
carrier's travel. Spec 8 first asked for a jump from the still case and was corrected
alongside this plan.

The cross-check costs one line, because the engine already knows what he is standing on:
the movement base has an owner, and that owner either carries the frame he rides or it
does not. Ten frames of disagreement warn, and then once a second, for the reason the
carry gap repeats -- a single line at frame ten does not say the disagreement has lasted
a minute. It is latched to one call per frame because the engine calls UpdateBasedMovement
once for a base that is not simulating and twice for one that is, and an unlatched
threshold would mean ten frames docked and five flying.

F docks from the pilot's seat and says so from anywhere else, because a key that does
nothing is indistinguishable from a key that is not bound.
MSG
```

---

### Task 6: Документы и точка проверки 6

То, чего headless не умеет: рывок на люке, согласие панели `dock` с картинкой, отлёт с возвратом в окно и ощущение перехода вообще. У headless нет ног, а через люк ходят ногами.

**Files:**
- Modify: `CLAUDE.md`
- Create: `docs/superpowers/notes/2026-09-10-checkpoint-6-runbook.md`
- Create: `docs/superpowers/notes/2026-09-10-checkpoint-6-results.md`

**Interfaces:**
- Consumes: все строки журнала и панели задач 3–5: `Station at world …`, `Docking port on …`, `Docking: station found …`, `Dock: docked.` / `undocked.` / `capturing, …` / `refused -- …`, секундная `Dock: …`, `Frame handover: …`, `Standing on … while bound to …`, `Carry NOT RUNNING`, `Shipboard drift`, строки панели `frame` и `dock`; переменные `sf.Dock`, `sf.BindFrame`, `sf.FlightLog`, `sf.DriftDebug`, `sf.ConsoleOperator`, `sf.Knockdown`.
- Produces: два документа и две строки в таблице переменных `CLAUDE.md`.

**Абзац «Current state» в `CLAUDE.md` этой задачей не трогается.** Он описывает то, что на `main`, а этот план ещё на ветке; переписывает его слияние после точки проверки 6.

**Числа, которые runbook просит сверить, выведены здесь.** Пристыкованный корабль стоит центром в station-local `(0, −1000, 0)`; проём люка 200 × 250 см, тоннель 200 см, между объёмами 10 см. Порог гравиплит `sqrt(19.62/r)` рад/с при радиусе `r` от оси — в пристыкованном состоянии ничего не вращается, поэтому на станции и в пристыкованной каюте никого не роняет вообще, и это первое, что стоит заметить глазами. Доводка **выше порога гравиплит на краю окна, и это известно заранее**: пик `6d/T²` считается по пути корпуса, а узел стоит на плече 4.80 м, поэтому разворот на допустимые 10° добавляет 0.84 м и даёт `d = 3.84` м — **23.0 м/с²** против 19.62. Сверх того угловая часть 4.19 м/с² на плече 4 м приходится на тот же кадр (вторая производная `smoothstep` максимальна на концах, а не в середине), и первый кадр доводки — отдельная ступенька в 60 м/с² на 60 fps, потому что `SetSimulatePhysics(false)` гасит скорость корпуса за кадр. Ни одно не компенсируется: команду подаёт пилот из кресла, а пристёгнутого не роняет по построению, и второго человека на борту нет. Поэтому в бланке спрашивается не «уронило ли», а `footing` пилота — если пристёгнутого всё-таки уронило, это находка совсем другого рода. Отстыковка обнуляет скорости, поэтому сразу после `F` корабль стоит **в окне**: `port 0.0 m … CAN DOCK`, и повторное `F` пристыкует обратно с нулевой доводкой. Тяга 800 кН на 50 т — 16 м/с², половина каюты 4 м проходится за 0.71 с: столько есть у человека, привязанного не к тому носителю, прежде чем под ним кончится пол.

- [ ] **Step 1: Две строки в таблицу переменных и рецепт запуска петли**

В `CLAUDE.md`, в таблицу «Console variables the rig understands», добавить две строки:

```markdown
| `sf.Dock` | -1 | Docking from the console, the headless twin of the `F` key. `1` docks if the approach is inside the window, `0` undocks, `-1` is no command. Acted on when the value **changes**, not on its level. `2` is the headless cycle — undock now and dock again one capture later — and it exists because `-ExecCmds` applies its whole list at startup, so `sf.Dock 0, sf.Dock 1` leaves only the last value and "docked → free → docked" cannot be asked for in one run at all |
| `sf.BindFrame` | *(empty)* | `ship` or `station`: binds the occupant to that carrier's frame **wherever he is standing**. The negative control for the handover, and the same trick as `sf.ShipGravity 0` for the carry. Docked, it is a legitimate handover and everything stays quiet; undocked and under thrust it is knowingly wrong, and the cross-check must warn within ten frames and the carry must stop about a second later; bound the other way round — to the moving ship from the station's floor — the `Frame handover` line must show one frame of the ship's travel, never zero |
```

Ниже рецепта headless-прогона добавить абзац:

```markdown
**The loop rig lives on a second map, and the default map has not changed.** `L_FrameSpike`
does not move a single default: every figure this project has recorded was taken on it.
The loop — station, ship spawned already docked, person in the cabin — is `L_Loop`, and
it is reached by naming the map as the **first positional argument**:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=70 -ExecCmds="sf.FlightLog 1" -log
```

`ASFLoopGameMode` forces `sf.ShipProfile 0` and says so in the log; the ship is docked
before the world begins play, so there is no frame in which it is adrift. **Run every
flight check on this map at `-fps=400` as well as `-fps=60`**: undocking recreates the
hull's physics state, and whether `SleepFamily` survives that is a question for a run, not
for a header.
```

- [ ] **Step 2: Написать runbook**

`docs/superpowers/notes/2026-09-10-checkpoint-6-runbook.md`:

````markdown
# Точка проверки 6 — порядок действий (петля целиком)

Этап 3 собран, 65 головных тестов проходят, все headless-проверки пройдены на 60 и на
400 fps (см. [бланк](2026-09-10-checkpoint-6-results.md), раздел «Что уже проверено без
рук»). Осталось то, чего журнал не умеет: **проходится ли петля §1 ногами, и заметен ли
переход.**

**Что мы выясняем.** Виден ли рывок при переходе через люк; стоит ли интерьер станции
неподвижно, пока корабль доводится; согласна ли панель `dock` с тем, что происходит на
экране; можно ли отлететь, развернуться, вернуться и попасть в окно руками.

**Сколько это займёт.** Три запуска, минут двадцать.

---

## Перед началом

| Проверить | Как |
|---|---|
| Редактор закрыт | `tasklist \| findstr Unreal` — пусто |
| Ветка та | `git branch --show-current` → `feat/docking-station` |
| Коммит | `git rev-parse --short HEAD` → записать в бланк |
| Дерево чистое | `git status --short` — пусто |
| Тесты | 65 строк `Result={Success}` в `Saved/Logs/StarForge.log` |

Если сомневаешься в сборке — пересобрать:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Запуск — **с картой петли первым аргументом**:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" /Game/Maps/L_Loop -game -windowed -ResX=1600 -ResY=900
```

---

## Управление

Стоя: `W A S D` — ходьба, стрелки — обзор, `E` — сесть в ближайшее свободное кресло.
Сидя: `W`/`S`, `A`/`D`, `Space`/`LeftCtrl` — тяга, `Q`/`R` — крен, `↑`/`↓` — тангаж,
`←`/`→` — рысканье, `E` — встать, **`F` — стыковка и отстыковка**.

`F` действует **только из кресла пилота**. Нажатая где угодно ещё, она пишет в журнал
`Docking is commanded from the pilot's seat.` — это ответ, а не отказ железа.

Консоль — `~` или **`F1`** (вторая клавиша существует ради нелатинских раскладок; на
них `~` не открывает консоль вообще и молча).

Кресло пилота по умолчанию занято консольным оператором: чтобы сесть, сначала
`sf.ConsoleOperator 0`.

---

## Как это читать

**Корабля снаружи не видно, и это правильно.** Ты внутри него. Единственное, что
говорит, где корабль относительно станции, — строка `dock` на панели и строка `Dock:` в
журнале. Не ищи корпус глазами, читай панель.

**Сначала `frame` и `dock`, потом цифры.** Строка `frame` говорит, кто тебя несёт, и
краснеет, когда пол под ногами принадлежит не тому носителю. Пока она красная, всё
ниже неё описывает не то, что происходит.

**При переходе через люк рывка быть не должно.** Строка передачи обязана прочитаться
`0.000 cm`, и картинка обязана согласиться: ни толчка, ни подпрыгивания, ни поворота
взгляда. Видимый рывок при `0.000 cm` в журнале — находка, и находка серьёзная: значит
прибор меряет не то, что видит глаз.

**Ориентиры.** В обоих помещениях шар — `+X`, конус — `+Y`, цилиндр — `−X` в осях
своего корпуса, и **размер маркера говорит, где ты**: у корабля они полметра, у станции
восемьдесят сантиметров. Корпуса состыкованы по Y навстречу, поэтому выход из
корабельного люка — это движение к станционному конусу.

**Тройка `at (x y z) ship` в строке `dock` — где узел станции относительно твоего, в осях
корабля, в метрах.** `+Y` — сторона люка, клавиша `D`; `−Y` — `A`; `+X` — нос, `W`; `−X` —
`S`; `+Z` — `Space`, `−Z` — `LeftCtrl`. Сразу после отстыковки она читается
`(+0.0 +0.0 +0.0)`; отойдёшь клавишей `A` — вырастет `Y`. Возвращаться — по ней, а не по
расстоянию: расстояние не говорит, в какую сторону.

---

## Журналы

`Saved/Logs/StarForge.log` перезаписывается каждым запуском. **Копировать после каждого
запуска, до следующего:**

```bash
cp Saved/Logs/StarForge.log docs/superpowers/notes/evidence/2026-09-10-checkpoint-6-A-loop.log
```

Полезно включить в начале каждого запуска: `sf.FlightLog 1`. Строку `Dock:` раз в
секунду пишет он.

---

## Запуск A — петля целиком (~10 минут)

**A1. Старт.** Ты стоишь в каюте. Проверить на панели:

- `frame  bound to Ship   last handover: Station -> Ship, 0.000 cm, 0.0 deg`;
- `dock   docked` (жёлтым);
- `flown by console (standing down)` — двигатели заперты, пост стоит в стороне.

Записать обе строки дословно.

**A2. Сесть.** Открыть консоль, `sf.ConsoleOperator 0`, закрыть. Подойти к креслу и
нажать `E` **на ходу**, а не с места: посадка — один телепорт, и с места прибор увода
запишет пройденное до кресла расстояние как увод. Панель: `flown by player`.

**A3. Отстыковаться.** Нажать `F`. В журнале обязано появиться `Dock: undocked.`,
панель — `dock free | port 0.0 m …  -- CAN DOCK` зелёным. **Записать, что видно на
экране в этот момент:** ничего двигаться не должно, скорости обнулены.

**A4. Отойти и развернуться.** Станция — по правую руку от носа: люк в стене `+Y`, и
корабль стоит к ней бортом. Отходить — клавишей `A` (это `−Y`, от станции), держать её
секунды две: 16 м/с² дают около 30 м, ассистент сам погасит скорость, когда отпустишь.
Смотреть на строку `dock`: расстояние растёт, тройка `at` показывает растущий `+Y`,
вердикт становится `out of window: distance`. Затем **полный оборот рысканьем** — `←`
около трёх секунд при 120°/с, глядя на `fwd`: он обязан пройти через 180 и вернуться;
дотримовать короткими нажатиями, пока `fwd` не станет меньше 10. **Записать, какие
причины отказа успели побывать в строке** — `distance` обязательно, `forward` во время
оборота.

**A5. Попробовать пристыковаться из-за окна.** Не подводя корабль, нажать `F`. В журнале
обязана появиться **одна** строка `Dock: refused -- …` с причиной и числом. Записать её.

**A6. Подвести и пристыковаться.** Возвращаться клавишей `D` по тройке `at`: нажатие на
секунду — примерно 16 м с торможением, дальше короткие нажатия по полметра, и ждать,
пока ассистент погасит скорость ниже 1 м/с. Условия: 3 м, 1 м/с, 10° и 10°; строка `dock`
называет, какое не выполнено. Дождаться зелёного `CAN DOCK` на панели и нажать `F`.
Ожидается:

- `Dock: capturing, N м, N м/с, fwd N deg, up N deg` — записать все четыре числа;
- панель `dock capturing 0.x of 1.0 s` оранжевым примерно секунду;
- `Dock: docked.` и `dock docked` жёлтым.

**Смотреть в этот момент на интерьер.** Он должен ехать плавно и не дёргаться.

**Доводка с края окна разгоняет корпус до 23.0 м/с², то есть выше порога гравиплит
19.62 — это посчитано заранее и не дефект.** Стоящего в каюте она уронила бы; тебя не
роняет потому, что ты пристёгнут в кресле, а больше на борту никого нет. Если панель
`footing` всё же показала `KNOCKED DOWN` **у пристёгнутого** — записать дословно, это
находка другого рода: пристёгнутость перестала работать.

**A7. Встать и выйти на станцию.** `E`, дойти до люка (стена с конусом, `+Y`), пройти
тоннель. В журнале обязана появиться:

```
Frame handover: Ship -> Station, first carry moved him 0.000 cm, gravity turned 0.0 deg
```

**Записать оба числа.** И записать, **был ли рывок глазами** — это главный вопрос
запуска.

**A8. Вернуться.** Пройти тоннель обратно в каюту. Вторая строка передачи,
`Station -> Ship`, с теми же числами. Панель `frame` обязана перещёлкнуться на `Ship`.

**A9. Постоять минуту.** Не касаться клавиатуры. `measured` должна дойти до 55 секунд,
`drift peak` остаться `0.000`. Записать обе.

---

## Запуск B — отрицательный контроль руками (~5 минут)

**B1.** Заново запустить петлю. Сесть, `F`, отлететь от станции метров на пятьдесят и
**держать тягу**.

**B2.** Открыть консоль (`~` или `F1`), набрать `sf.BindFrame station`, закрыть.

**B3.** Смотреть, что происходит. Обязаны сработать по порядку:

- строка `frame` на панели становится `bound to Station` и краснеет в течение доли
  секунды;
- в журнале `Standing on Ship's floor while bound to Station's frame for 10 frames`;
- примерно через секунду `Carry NOT RUNNING`, и каюта уезжает из-под тебя.

**Записать, что из этого случилось и в каком порядке.** Если панель осталась белой и в
журнале нет ни одной из двух строк — **это находка первого ранга**: прибор слеп.

**Увод при этом расти не обязан**, и судить контроль по нему нельзя: увод меряется в
той системе отсчёта, к которой ты привязан, а станция неподвижна.

**B4. Обратный контроль — для прибора передачи.** Заново запустить петлю. **Не садясь**,
выйти через люк на станцию и остановиться посреди коробки. Панель: `frame bound to
Station`, белая.

**B5.** Открыть консоль, набрать `sf.Dock 0`, затем `sf.Intent.X 1`, закрыть. Корабль
уводит консольный оператор; ты стоишь на станции и к ней привязан — всё тихо, и должно
быть тихо: строка `frame` белая, увод `0.000`.

**B6.** Секунд через пять набрать `sf.BindFrame ship`. Теперь ты привязан к носителю,
который **движется**, стоя на полу, который нет. Обязано случиться:

- `Frame handover: Station -> Ship, first carry moved him N cm` с `N` **больше нуля** —
  ровно кадр хода корабля: на 100 м/с и 300 fps это 33 см. **Ноль здесь — слепой прибор
  передачи**, и это находка первого ранга;
- строка `frame` краснеет, в журнале `Standing on Station's floor while bound to Ship's
  frame`;
- дальше тебя уносит вслед за кораблём сквозь стену станции, пол кончается, и пишется
  `Carry NOT RUNNING`. Это ожидаемо: контроль заведомо ломает привязку.

**Записать число из строки передачи.** Журнал этого запуска — отдельный файл
`evidence/2026-09-10-checkpoint-6-B2-bindship.log`.

---

## Красные строки

Любая из них — прогон останавливается, находка записывается:

- `CARRY NOT RUNNING` в **запуске A** (в B она ожидаема);
- `CARRY SKIPPED … ticks`;
- `Standing on … while bound to …` в запуске A;
- `Frame handover` с числом больше `0.010 cm` при пристыкованных носителях;
- видимый рывок на люке при `0.000 cm` в журнале;
- `dock` красным: `NO STATION in this level`;
- `flight: thruster array NOT built`.

---

## Если пошло не так

**`F` ничего не делает.** Ты не в кресле пилота, или в кресле канонира. В журнале должно
быть `Docking is commanded from the pilot's seat.`; если нет вообще ничего — клавиша не
дошла, проверь фокус окна.

**Консоль не открывается.** Раскладка. Жать `F1`.

**`E` не сажает.** Кресло занято консольным оператором: `sf.ConsoleOperator 0`.

**Не получается попасть в окно.** Отпустить всё и подождать: ассистент сам сбрасывает
скорость. Окно — 3 м, 1 м/с, 10° и 10°; строка `dock` называет, какое из четырёх условий
мешает, и чинить надо именно его.

**Провалился сквозь пол или застрял в люке.** Записать место в `ship-local` координатах
с панели и продолжить. Это находка, а не помеха прогону.

---

## Что дальше

Заполнить [бланк](2026-09-10-checkpoint-6-results.md) числами **из журналов**, не с экрана,
скопировать журналы в `notes/evidence/`, и принять решение в конце бланка. Зелёный
свет — слияние `--no-ff` в `main` и передача этапа 4.
````

- [ ] **Step 3: Написать бланк**

`docs/superpowers/notes/2026-09-10-checkpoint-6-results.md`:

````markdown
# Точка проверки 6 — результаты (петля целиком)

**Дата прогона:** ____
**Сборка:** StarForgeEditor Win64 Development, UE 5.8.2
**Ветка / коммит:** `feat/docking-station` / ____
**Частота кадров в окне:** ____ (`dt` из строк `Drift debug`, если включался `sf.DriftDebug`)

Порядок действий — в [runbook](2026-09-10-checkpoint-6-runbook.md). Числа берутся из
журналов в `evidence/`, не с экрана.

| Запуск | Журнал |
|---|---|
| A — петля целиком | `evidence/2026-09-10-checkpoint-6-A-loop.log` |
| B — отрицательный контроль руками | `evidence/2026-09-10-checkpoint-6-B-bindframe.log` |
| B4–B6 — обратный контроль, прибор передачи | `evidence/2026-09-10-checkpoint-6-B2-bindship.log` |

---

## Что уже проверено без рук (headless, 60 и 400 fps)

| Проверка | 60 fps | 400 fps |
|---|---|---|
| Сборка без предупреждений, `StarForgeCore` только на `Core`/`CoreUObject` | ____ | — |
| Головные тесты | ____ / 65 | — |
| Старт пристыкованным: `Dock: docked` в первой полётной строке | ____ | ____ |
| Увод стоя за 60 с при пристыкованном корабле | ____ см | ____ см |
| `skipped carries` в прогоне (а) | ____ | ____ |
| Инерция при старте (`Thruster array ready`) | ____ | ____ |
| Инерция после `Dock: undocked.` | ____ | ____ |
| Полный крен после отстыковки | ____ °/с | ____ °/с |
| Цикл `sf.Dock 2` проходит `undocked → capturing → docked` | ____ | ____ |
| Строк `Frame handover` в цикле `sf.Dock 2` | ____ (ожидается 0) | ____ |
| Одна передача `Station -> Ship` при спавне | ____ см | ____ см |
| Контроль №1: `Standing on …` при `sf.BindFrame station` пристыкованным | ____ | ____ |
| Контроль №2: `Carry NOT RUNNING` при `sf.BindFrame station` в полёте | ____ | ____ |

**Инерция.** `533333 kg m²` — это `m(a²+b²)/12` для куба 8 м в 50 т. Если стартовое число
иное, а число после отстыковки правильное, это Chaos, отвечающий за кинематическое тело,
и перестроение при отстыковке работает как задумано. Записать оба.

**Крен на 400 fps.** `180.0` — норма. `1.7` означает, что дефект сна корпуса вернулся
через выключение и включение симуляции, и это блокирует слияние.

---

## Сначала — действителен ли прогон

| Условие | Требование | Факт |
|---|---|---|
| Журналы скопированы в `evidence/` до следующего запуска | три файла | ____ |
| Журналы заканчиваются `Log file closed` | да | ____ |
| `skipped carries` в запуске A | 0 | ____ |
| `Carry NOT RUNNING` в запуске A | ни одной | ____ |
| `Standing on …` в запуске A | ни одной | ____ |
| `measured` в шаге A9 | ≥ 55 с | ____ |

Прогон действителен: ____

---

## Запуск A — петля целиком

| Шаг | Ожидание | Факт |
|---|---|---|
| A1 строка `frame` | `bound to Ship … Station -> Ship, 0.000 cm, 0.0 deg` | ____ |
| A1 строка `dock` | `docked`, жёлтым | ____ |
| A1 `flown by` | `console (standing down)` | ____ |
| A2 посадка | `flown by player`, увод не всплеск от телепорта | ____ |
| A3 `F` | `Dock: undocked.` | ____ |
| A3 сразу после отстыковки | `port 0.0 m … CAN DOCK`, зелёным | ____ |
| A4 вердикты при отлёте | побывали `distance` и `forward` | ____ |
| A5 отказ из-за окна | **ровно одна** строка `Dock: refused -- …` | ____ |
| A5 причина и число в ней | записать дословно | ____ |
| A6 `Dock: capturing, …` | четыре числа | ____ |
| A6 доводка на экране | плавно, без дёрганья | ____ |
| A6 `footing` пилота | сидит — `KNOCKED DOWN` невозможен; иное записать дословно | ____ |
| A6 `Dock: docked.` | есть | ____ |
| A7 передача `Ship -> Station` | `0.000 cm`, `0.0 deg` | ____ |
| **A7 рывок глазами** | **нет** | ____ |
| A8 передача `Station -> Ship` | `0.000 cm`, `0.0 deg` | ____ |
| A8 строка `frame` | перещёлкнулась на `Ship` | ____ |
| A9 `measured` | ≥ 55 с | ____ |
| A9 `drift peak` | `0.000` см | ____ |

**Согласие панели с картинкой:** ____

---

## Запуск B — отрицательный контроль руками

| Показатель | Ожидание | Факт |
|---|---|---|
| Строка `frame` после `sf.BindFrame station` | `bound to Station`, краснеет | ____ |
| `Standing on Ship's floor while bound to Station's frame` | в первые кадры | ____ |
| Повтор предупреждения | примерно раз в секунду | ____ |
| `Carry NOT RUNNING` | примерно через секунду | ____ |
| Каюта уехала из-под ног | да | ____ |
| Увод в строке `Shipboard drift` | **расти не обязан** — судить не по нему | ____ |
| B6 `Frame handover: Station -> Ship` | `N cm`, **N > 0** — кадр хода корабля | ____ |
| B6 строка `frame` | краснеет; `Standing on Station's floor while bound to Ship's frame` | ____ |
| B6 `Carry NOT RUNNING` после ухода пола | есть — ожидаемо | ____ |

**Прибор не слеп:** ____ (в B3 сработало хотя бы одно из первых четырёх, и в B6 число передачи больше нуля — да / нет)

---

## Глазами

| Вопрос | Ответ |
|---|---|
| Заметен ли переход через люк вообще | ____ |
| Стоит ли интерьер станции неподвижно при доводке | ____ |
| Читается ли, где ты, по размеру маркеров | ____ |
| Понятно ли из строки `dock`, что мешает пристыковаться | ____ |
| Ощущается ли петля как одно место, а не как две сцены | ____ |

---

## Долги, которые этот прогон закрывает или переносит

| Долг | Состояние |
|---|---|
| §8 ждала «ненулевого прыжка» в `Frame handover` при неверной привязке к неподвижной станции; механизм даёт ноль по построению (R11) | **закрыт**: §8 переписана 2026-09-10 вместе с планом — этот контроль судится по перекрёстной проверке и переносу, у прибора передачи свой контроль (B4–B6) |
| Доводка превышает порог гравиплит: 23.0 м/с² на краю окна (плечо узла 4.80 м добавляет 0.84 м к пути корпуса), плюс 4.19 м/с² угловой части в тот же кадр, плюс ступенька первого кадра 60 м/с² на 60 fps | записано как шов, не как дефект: команду подаёт пристёгнутый пилот, второго человека на борту в срезе нет. Чинится выводом `CaptureSeconds` из фактического пути и кубическим Эрмитом в `CapturePose` — тогда, когда на борту появится стоящий пассажир, и не раньше |
| Люк сделал каюту `L_FrameSpike` незакрытой (R12): уроненного может выбросить наружу | ____ |
| Геймпада нет; аналоговый путь по-прежнему не наблюдался | переносится в этап 4 |
| §6 «волочёт по палубе» помечена неточной | остаётся отложенной |

---

## Что прогон нашёл

1. ____
2. ____

---

## Решение

- [ ] **Зелёный свет: петля проходится целиком, переход незаметен, отрицательный контроль кричит** — слияние `--no-ff` в `main`, затем handoff этапа 4
- [ ] Рывок на люке виден — чинить задачу 5, повторять A
- [ ] Отрицательный контроль молчит — прибор слеп, чинить перекрёстную проверку, повторять B
- [ ] Стыковка не даётся руками — пересмотреть окно в данных корабля, повторять A
- [ ] Крен после отстыковки не выходит на 180.0 на 400 fps — дефект сна вернулся, чинить задачу 3, повторять headless (б)
- [ ] Прогон не состоялся — чинить стенд, повторять. Решения о дизайне не принимаем

**Обоснование:** ____
````

- [ ] **Step 4: Коммит**

```bash
git add CLAUDE.md docs/superpowers/notes/2026-09-10-checkpoint-6-runbook.md docs/superpowers/notes/2026-09-10-checkpoint-6-results.md
git commit -F - <<'MSG'
docs: write the runbook and results form for checkpoint 6

What the headless runs cannot say: whether the handover through the hatch is visible,
whether the picture agrees with the dock panel, and whether flying away and coming back
into a three-metre window is something a person can actually do. A headless run has no
legs, and the airlock is walked through on foot.

The runbook derives every number it asks the operator to compare against, and says which
line to read before which: the frame line outranks every figure below it, because while
it is red the carry is following a frame that is wrong and nothing under it describes
what is happening.

It also says what the negative control may NOT be judged on. Drift is measured in the
frame the occupant is bound to, and a man wrongly bound to a motionless station does not
move with respect to it, so his drift stays at zero while the ship leaves without him.
The two instruments that see it are the cross-check and the carry, and the form asks for
those.

The negative control comes in two halves because the two instruments are blind to
different things. Bound to the motionless station from a moving ship, the handover line
reads zero by construction and the cross-check and the carry are what see the error;
bound to the moving ship from the station's floor, the handover line must show one frame
of the ship's travel, and a zero there is the handover instrument asleep. Section 8 said
the first case would show a jump, and was corrected alongside the plan.

CLAUDE.md gains the two variables and the loop rig's recipe. The default map does not
change: L_FrameSpike is where every recorded figure was taken, and the loop is reached
by naming its map instead.
MSG
```

---

## Точка проверки 6

После задачи 6 — по runbook, руками. Проверить перед слиянием:

- **65** головных тестов проходят из консоли, без редактора; `StarForgeCore` по-прежнему зависит только от `Core` и `CoreUObject`; сборка без предупреждений;
- корабль появляется **уже пристыкованным**, и в журнале это видно двумя строками рядом — `Dock: docked.` и `Loop rig: scripted profile forced off; the ship spawns docked.`;
- перенос переживает смену расписания в обе стороны: `skipped carries 0` и ни одной `Carry NOT RUNNING` в цикле `sf.Dock 2`, на 60 и на 400 fps. Это и есть то, что §8 требует **проверить, а не вывести**;
- крен после отстыковки выходит на **180.0 °/с на 400 fps** — `SleepFamily` пережил выключение и включение симуляции;
- передача системы отсчёта через люк читается `0.000 cm` и **не видна глазом**;
- отрицательный контроль `sf.BindFrame station` в полёте **кричит**: предупреждение перекрёстной проверки в первые кадры и `Carry NOT RUNNING` следом; обратный контроль — `sf.BindFrame ship` с палубы станции при уходящем корабле — показывает в строке `Frame handover` ненулевой кадр хода. Молчание в любом из двух — слепой прибор, и это блокирует слияние;
- отказ вне окна называет **которое** условие не выполнено и на сколько.

Слияние — `git merge --no-ff feat/docking-station` в `main` после заполненного бланка; затем handoff этапа 4 в `docs/superpowers/notes/`.

## Что дальше

Этап 4 (§13): бой — снаряды, попадания, щиты, прочность, пират. Планировать **от того, что есть**, а не от текста §13: пост канонира, турель, автоматика-оператор и мишень сделаны планом «в», и §13 кладёт их в этап 4 по недоразумению даты, а не по существу. Требования к этапу 4, зафиксированные здесь:

1. **Пират — тот же `ASFTestShip` с постами, занятыми автоматикой.** Второй корабль впервые сделает `sf.*`-переменные двусмысленными: `sf.Intent.*`, `sf.ShipProfile`, `sf.Dock`, `sf.ConsoleOperator` — все безымянные и все предполагают один корабль. Решать это надо **до** того, как их станет двадцать, и первый же второй корабль — тот момент.
2. **`USFDockingComponent` предполагает ровно одну станцию.** `ResolvePorts` берёт первую найденную и говорит об этом в комментарии. Абордаж — а он получается тем же механизмом, что выход на станцию, и §8 на это прямо рассчитывает — потребует выбирать **ближайший свободный узел**, а не первый попавшийся, и это правило ещё никем не написано.
3. **Относительная скорость сближения считается как модуль скорости корабля**, потому что станция неподвижна. Первый подвижный носитель, к которому придётся стыковаться, превращает эту строку в разность двух скоростей; место помечено комментарием в `UpdateApproach`.
4. **Объёмы интерьеров не пересекаются по построению, и это не проверяется кодом.** Два корабля, стыкующиеся бортами, могут дать пересечение, и тогда человек будет привязан к тому, чей объём выстрелил последним. Правило «вход привязывает, выход не отвязывает» при пересечении объёмов перестаёт быть однозначным.
5. **Замок двигателей — общий механизм, а не деталь стыковки.** Повреждённый двигатель, обесточенный корабль и захваченный пиратом корпус — это тот же `SetLocked`, и пост уже умеет стоять в стороне, не зная причины. Этап 4 должен пользоваться им, а не заводить второй.
