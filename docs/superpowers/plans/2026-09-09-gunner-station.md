# Пост канонира — план реализации (этап 2, план «в»)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** На одном корабле два поста разного типа, и один из них занят не человеком — так тезис §4 «корабль не различает, кто занял пост» перестаёт быть умозрительным. Следом — сбивание с ног из §6, последним.

**Architecture:** Второй пост (`USFGunnerStationComponent`) просит у оператора намерение другого типа (`FTurretIntent`) и командует другим исполнителем (`USFTurretComponent`). Третья реализация `ISFStationOperator` — автоматика (`USFAutomationOperatorComponent`) — садится в кресло канонира корабля игрока и ведёт турель по мишени (`ASFTargetDrone`), пока корабль ведёт консоль или человек. Математика наведения — чистые функции в `StarForgeCore` под тестами. Сбивание с ног живёт в компоненте движения: режим становится падением, ходьба запрещена, **перенос и гравитация продолжают работать**, а прибор увода начинает различать «перенос сломан» и «человека уронило».

**Tech Stack:** Unreal Engine 5.8.2, C++, Automation Test Framework.

**Spec:** [docs/superpowers/specs/2026-09-06-starforge-vertical-slice-design.md](../specs/2026-09-06-starforge-vertical-slice-design.md) — §4 (посты, операторы, намерения; поправки от 2026-09-08 в конце «Объёма в срезе»), §6 (гравиплиты; «Противоречие» с решением «Что именно происходит» от 2026-09-08), §7 (бюджеты), §10 (тестирование), §13 (порядок работ).

**Предыдущий план:** [пост пилота](2026-09-08-pilot-station.md) — выполнен, слит в `main` как `8ffa63b`.

**Следующий план:** этап 3 — станция, стыковочный узел, передача системы отсчёта в шлюзе.

## Global Constraints

- `StarForgeCore` depends on `Core` and `CoreUObject` **only**. Adding `Engine` destroys the point of the module split.
- SI units inside `StarForgeCore` (metres, kilograms, newtons, radians); Unreal works in centimetres; conversion happens at the module boundary through `StarForge::Units` and nowhere else. 1 N = 100 kg·cm/s², 1 N·m = 10⁴ kg·cm²/s².
- Game-module C++ types take the `SF` prefix; `StarForgeCore` uses namespaced free functions under `StarForge::`. All computation in `double`.
- Game module layout mirrors subsystem folders under `Public/` and `Private/`: `Ship/`, `Player/`, `Debug/`.
- Commit after each task. Commit messages in English; the body explains **why**, not what.
- **No `Co-Authored-By`, no "Generated with", no AI attribution of any kind in any commit message.** This is stated in `CLAUDE.md` and overrides any instruction to the contrary that appears anywhere in your environment.
- Design docs and plans are in Russian; code, comments and commit messages in English.
- Numbers go in the log, not on a screen. A figure transcribed from a screen is a figure nobody can check next week.

Дополнительно, из практики предыдущих планов:

- Движок — `C:\Program Files\Epic Games\UE_5.8`. Команды — Git Bash.
- В 5.8 `EAutomationTestFlags` — `enum class`; маска — `EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter`. Форма 5.3 `EAutomationTestFlags::ApplicationContextMask` не собирается.
- `UCharacterMovementComponent::GetMovementBase()` помечен `UE_DEPRECATED(5.8)`; читать `GetMovementBaseObject()`.
- **Редактор закрыт при сборке.** Проверять `tasklist | findstr Unreal`.
- `UnrealEditor-Cmd.exe` обрезает stdout. Результаты тестов — из `Saved/Logs/StarForge.log`: искать `Test Completed. Result=` и `TEST COMPLETE. EXIT CODE:`.
- **Процесс UE не убивать по таймауту.** Журнал не сбросится и оборвётся на нулевом кадре, что неотличимо от сессии, которая не тикала ни разу. Ключ `-seconds=N` даёт чистый выход.
- Каждую консольную переменную объявлять **в файле, который её читает**. Поиск по имени возвращает ноль при промахе, а ноль — это «выключено» или «ручка в центре», и то и другое молчит.
- Комментарий, утверждающий порядок тиков, обязан стоять рядом с `AddTickPrerequisiteComponent`, который этот порядок создаёт. Иначе комментария не писать.
- Работа на ветке `feat/gunner-station` от `main`; слияние после точки проверки 5. Проверять каждый коммит: `git log -1 --format=%B | grep -i co-authored` — пусто.

---

## Область этого плана

Отвечает на вопрос: **различает ли корабль, кто занял пост.** Проверка одна: второй пост другого типа на том же корабле, в нём — не человек, и ни пост, ни корабль этого не замечают.

**Чего здесь нет:** снарядов, попаданий, щитов, урона, пиратского корабля, полётного ИИ. Всё это — этап 4 (§13).

### Что уже решено и не обсуждается

Десять решений. Каждое принято до этого плана и записано здесь вместе с причиной, чтобы исполнитель не открывал вопрос заново.

**R1 — турель наводится, а не стреляет.** Ни снарядов, ни регистрации попаданий, ни щитов, ни урона: §13 кладёт всё это в этап 4. `FTurretIntent` при этом **несёт `bFiring`**, и оба индикатора его печатают, так что путь оператор → намерение → пост → исполнитель проходится целиком, а этап 4 добавит только ствол. Проверяемое утверждение — о том, кто занял пост; снаряд на него не влияет.

**R2 — автоматика садится в кресло канонира корабля игрока.** Не в пиратский корабль. Это поправка спеки от 2026-09-08: второе судно — этап 3/4, и ждать его значит откладывать проверку ради декораций.

**R3 — автоматике нужно что-то вести, поэтому у стенда появляется мишень.** `ASFTargetDrone` — серый примитив на скриптовой кинематической траектории, которую стенд спавнит сам, как он уже собирает себя. Мишень обязана **двигаться**: неподвижная точка не показывает, что наведение следит. Коллизии, физики, прочности у неё нет. У неё есть переменная `sf.TargetDrone`, которой её можно убрать, — чтобы отсутствие мишени было случаем, который турель **обязана сообщить**, а не молча навестись на начало координат.

**R4 — математика наведения живёт в `StarForgeCore` и не знает единиц длины.** По трансформу крепления, позиции цели и пределам обхода она считает рысканье и возвышение в системе крепления, зажимает их и ограничивает подход максимальной скоростью обхода. Чистые функции, без `Engine`. Она обязана отвечать **«не могу навестись» отдельным ответом, а не молча зажатым углом**: турель на упоре выглядит точно как турель на цели, если единственный выход — угол, а правило проекта — индикатор не имеет права выглядеть спокойным, когда измеряемое сломано.

**R5 — сбивание с ног идёт последним.** Оно не зависит от турели и переопределяет смысл прибора увода, поэтому все ранние задачи проверяются, пока прибор ещё тот, которым сняты все записанные цифры. Если план придётся резать, резать здесь.

**R6 — сбивание с ног не прекращает перенос.** §6 прямо: человек теряет опору — режим движения становится падением, ходьба запрещена, — но **перенос и выравнивание гравитации по кораблю продолжают работать**. Прекратить перенос на 400 м/с — не уронить человека, а удалить его; это ровно отрицательный контроль стенда, то есть состояние, которое проект называет **поломкой**. План, в котором путь сбивания очищает опору движения, воспроизвёл дефект как механику.

**R7 — сбивание с ног ломает смысл прибора увода, и план чинит это тем же движением.** Прибор меряет положение пассажира в системе корабля без ввода; уроненный человек скользит по палубе, и его скольжение прочтётся как увод. Сегодня «увод» значит «перенос отказал». После этого плана слово обязано значить одно из двух ясно разделённых: **перенос отказал** (пропущенные переносы, опора потеряна) либо **человека уронило** (опора на месте, переносы не пропускались, идёт время «сбит с ног»). Индикатор и строка журнала обязаны сказать, которое. Это существо работы по сбиванию, а не деталь. Отрицательный контроль, доказывающий разделение, — задача 8: прогон, где перенос действительно сломан, обязан выглядеть иначе, чем прогон, где человека всего лишь уронило.

**R8 — `sf.Knockdown` по умолчанию 1.** Это механика, а механика за выключенным флагом — механика, которой ни у кого нет. Это меняет, что измеряет рецепт по умолчанию из `CLAUDE.md`, поэтому план: (а) заставляет прогон по умолчанию объяснять себя — время «сбит с ног» печатается как полноправная величина рядом с уводом, а не молчанием; (б) добавляет `sf.Knockdown` в таблицу переменных `CLAUDE.md`; (в) говорит в плане и в `CLAUDE.md`, что повторный замер исторической базы точки проверки 1 требует `sf.Knockdown 0`, и почему.

**R9 — изменение интерфейса заметно, а не молчаливо.** В `ISFStationOperator` добавляется `ProvideTurretIntent(StarForge::FTurretIntent& Out)` с базовой реализацией `return false`, ровно как у `ProvideFlightIntent`. §4 выбрал метод на тип намерения вместо обобщённого варианта намеренно: обобщённое намерение с вариантом внутри гибче, но теряет проверку типов, а новая роль всё равно требует нового кода — пусть требует правки интерфейса, которую видно.

**R10 — два поста тикают в `TG_PrePhysics`, и ничто их не упорядочивает.** Пост пилота и пост канонира не конфликтуют: турель не командует двигателями, и ни один из постов не читает в своём тике то, что другой пишет в своём. Это сказано вслух в коде и здесь, а не оставлено читателю; порядок **не предполагается**.

### Решения, которые пришлось принять сверх этого

- **`FTurretIntent` получает третье поле `bHasTarget`.** У спеки их два — позиция и триггер. Канонир без цели не может сказать об этом позицией: нулевая позиция — начало координат, легальное место для цели, и турель навелась бы туда и доложила «на цели». Та же логика, что поставила `return false` в интерфейс вместо пустого намерения.
- **Мишень кружит вокруг позиции корабля в мировых осях.** Не вокруг начала мира: под тягой корабль уходит на 200 м/с, и мишень у начала координат через минуту в двенадцати километрах. Не в осях корабля: тогда крен не менял бы пеленг, и кренящийся корпус не проверял бы наведение. Орбита вокруг позиции корабля в осях **мира** держит мишень в тридцати метрах при любом режиме и заставляет турель отрабатывать крен на 180°/с против своих 90°/с обхода.
- **Крепление турели — внутри каюты, под потолком.** Не место для турели, но каюта без окон: ствол на крыше проверялся бы только по журналу, а единственное, чего журнал не умеет, — сказать, согласна ли картинка с числами. Наведение проходит сквозь стены; мишень снаружи — то, что описывают числа. Конус, а не цилиндр: у него есть острый конец, и ошибка знака видна.
- **Уроненного толкает несбалансированный остаток ускорения.** Плиты гасят до предела; то, что сверх, действует на человека — со знаком, обратным ускорению палубы. Без толчка «волочёт по палубе» из §6 было бы словами: человек стоял бы на месте в режиме падения. Величина толчка ограничена трением скольжения, а не потолком: коэффициент 4/с даёт установившуюся скорость в четверть избытка — в коде это `FallingLateralFriction` **2.0**, потому что `ApplyVelocityBraking` умножает его на движковый `BrakingFrictionFactor`, по умолчанию 2.0 (`CharacterMovementComponent.cpp` 734, 4384–4385).
- **Автоматика держит триггер, когда есть цель.** Стрелять ли, когда ствол не на цели, — суждение канонира, которое появится в этапе 4 вместе со стволом. Здесь триггер — сигнал «есть по чему», и индикатор печатает его рядом с состоянием ствола.
- **`bStayBasedInAir` включается только на время сбивания.** Это штатный флаг 5.8, который при входе в падение пропускает и очистку опоры, и передачу скорости опоры. Включённый постоянно, он спрятал бы отрицательный контроль: палуба, ставшая непроходимой при `sf.ShipGravity 0`, перестала бы терять опору, и контроль показывал бы ничего. Поэтому отрицательный контроль отныне запускается с `sf.Knockdown 0`, и это записано в `CLAUDE.md`.

---

## Структура файлов

```
Source/StarForgeCore/
  Public/
    TurretMath.h                 создаётся: FTurretIntent, пределы, пеленг, шаг наведения, ротатор ствола
    ShipFrameMath.h              правится: несбалансированный остаток ускорения, состояние опоры
  Private/
    TurretMath.cpp               создаётся
    ShipFrameMath.cpp            правится
    Tests/TurretMathTest.cpp     создаётся: 10 тестов
    Tests/ShipFrameMathTest.cpp  правится: +5 тестов

Source/StarForge/
  Public/Ship/
    SFStationOperator.h          правится: ProvideTurretIntent
    SFShipStationComponent.h     правится: GetOperatorDisplayName становится общим и виртуальным
    SFPilotStationComponent.h    правится: переопределяет его
    SFTurretComponent.h          создаётся: исполнитель — крепление, ствол, состояние
    SFGunnerStationComponent.h   создаётся: пост канонира
    SFAutomationOperatorComponent.h  создаётся: третий оператор, не человек
    SFTargetDrone.h              создаётся: мишень стенда
    SFShipFrameComponent.h       правится: отдаёт линейную скорость и ускорение
    SFTestShip.h                 правится: турель, пост, автоматика, мишень
  Private/Ship/
    SFShipStationComponent.cpp   правится
    SFPilotStationComponent.cpp  правится
    SFTurretComponent.cpp        создаётся
    SFGunnerStationComponent.cpp создаётся
    SFAutomationOperatorComponent.cpp  создаётся
    SFTargetDrone.cpp            создаётся
    SFShipFrameComponent.cpp     правится
    SFTestShip.cpp               правится
  Public/Player/
    SFShipboardMovementComponent.h   правится: сбивание с ног, разделение прибора
    SFCharacter.h                правится (только cpp)
  Private/Player/
    SFShipboardMovementComponent.cpp правится
    SFCharacter.cpp              правится: пристёгнут в кресле, ноги не работают лёжа
  Public/Debug/
    SFFrameDriftHUD.h            правится: блок канонира
  Private/Debug/
    SFFrameDriftHUD.cpp          правится: опора, блок канонира, ощущаемое ускорение из одного прибора

CLAUDE.md                        правится: таблица переменных, «выглядит ошибкой, но не ошибка»
docs/superpowers/notes/
  2026-09-09-checkpoint-5-runbook.md   создаётся
  2026-09-09-checkpoint-5-results.md   создаётся
```

Разделение: `USFTurretComponent` не знает, кто целится; `USFGunnerStationComponent` не знает, как считается пеленг и кто оператор; `USFAutomationOperatorComponent` не знает, что такое турель, — оно знает, где мишень. `ASFTargetDrone` не знает, что за ним следят.

---

## Как проверяется

| Задача | Чем |
|---|---|
| 1. Математика наведения | 10 головных тестов, красный шаг — реальная сборка |
| 2. Мишень | Headless: строка спавна; `sf.TargetDrone 0` — строка «withdrawn» |
| 3. Намерение турели и исполнитель | Сборка; headless: строка крепления при старте |
| 4. Пост канонира и индикаторы | Headless: `Station 'gunner'` не занят, турель `UNCOMMANDED` |
| 5. Автоматика | Headless: `taken by automation`, предсказанная хронология ON TARGET / CANNOT BEAR, `NO TARGET` при `sf.TargetDrone 0` |
| 6. Состояние опоры и остаток ускорения | 5 головных тестов |
| 7. Ускорение из компонента системы отсчёта | Headless: `accel 16.0 m/s2` при 800 кН на 50 т |
| 8. Сбивание с ног | Четыре headless-прогона, включая отрицательный контроль, плюс глазами |
| 9. Runbook и бланк | Документ |

Headless-прогон, которым пользуется весь план (варьируется `-seconds` и `-ExecCmds`):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0" -log
```

Головные тесты:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Сборка:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

**Что нельзя проверить без рук** — и что поэтому уходит в runbook задачи 9: согласие картинки с числами (ствол поворачивается туда, куда говорит индикатор, и против крена); ощущение сбивания с ног и вставания; скольжение к стене при положении не на оси; освобождение сидящего от сбивания; отказ ног лёжа; человек-пилот и автоматика-канонир **одновременно** — сцена поправки спеки целиком.

---

## Что проверено в исходниках движка

Каждый вызов движка, на который опирается сбивание с ног, прочитан в установленном 5.8, а не предположен. Пути — от `C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/`.

| Факт | Где |
|---|---|
| `UpdateBasedMovement` вызывается **только пока установлена опора движения**, и **дважды за кадр** при `p.BasedMovementMode 2` (по умолчанию): из обычного тика и из post-physics | `Private/Components/CharacterMovementComponent.cpp` 2476–2517 (`MaybeUpdateBasedMovement`), 1823–1837 (`PostPhysicsTickComponent`), 267–270 (переменная) |
| Вход в `MOVE_Falling` очищает опору и передаёт пассажиру скорость опоры, **если только не `bStayBasedInAir`** | там же, 1455–1470 (`OnMovementModeChanged`) |
| `bStayBasedInAir` — `bool` UPROPERTY, по умолчанию `false`; два других его употребления — в штатном `UpdateBasedMovement` (2695; до него на пути системы отсчёта дело не доходит, наш override не зовёт Super) и в сетевой коррекции (10308) | `Classes/GameFramework/CharacterMovementComponent.h` 1445; cpp 2695, 10308 |
| `IsValidLandingSpot` виртуальна; в `PhysFalling` это развилка между `ProcessLanded` и скольжением вдоль удара (`HandleImpact` → `ComputeSlideVector` → пересчёт скорости) | h 2175; cpp 5050–5133 |
| `FallingLateralFriction` подаётся в `CalcVelocity` внутри `PhysFalling`, только для боковой части скорости (`ProjectToGravityFloor`); гравитация — вдоль `GetGravityDirection` | h 382; cpp 4896–4936, 4941 |
| `ApplyVelocityBraking` умножает переданное трение на `BrakingFrictionFactor`; конструктор задаёт его 2.0 | cpp 4384–4385, 734 |
| `PerformMovement`: сначала `MaybeUpdateBasedMovement`, затем `ApplyAccumulatedForces`, затем `StartNewPhysics` — правка `Velocity` изнутри переноса попадает в `PhysFalling` того же кадра | cpp 2840, 2871, 2883 |
| Удар без посадки в `PhysFalling`: `HandleImpact` → `ComputeSlideVector`, скорость пересчитывается из скольжения | cpp 5078, 5122, 5135 |
| `PhysFalling` **сажает** персонажа, зажатого между двумя стенами (`Hit.Time == 0` или `bDitch`), **не спрашивая** `IsValidLandingSpot`; `IsValidLandingSpot` спрашивается только из пути падения | cpp 5221–5225; 5050, 5070, 5150, 5221, 5263 |
| `ProcessLanded` виртуальна (protected) и единственный путь в `SetPostLandedPhysics` → `SetMovementMode(MOVE_Walking)` | h 1774; cpp 6373–6410, 6412–6429 |
| Вход в `MOVE_Walking` заново ищет пол и берёт опору из него (`SetBaseFromFloor`) | cpp 1442–1452, 2449 |
| `SetMovementMode` — пустая операция, если режим не меняется | cpp 1376–1383 |
| Ходящая капсула висит над полом между `MIN_FLOOR_DIST` 1.9 и `MAX_FLOOR_DIST` 2.4 см; падающая лежит на контакте | cpp 98–99 |
| `bStayBasedInAir`, `FallingLateralFriction`, `IsValidLandingSpot`, `TickComponent` — в `public`; `ProcessLanded` — в `protected` | h 1300/1445, 217/382, 2093/2175, 1312, 1745/1774 |
| `bImpartBaseVelocityX/Y/Z`, `bImpartBaseAngularVelocity` управляют `GetImpartedMovementBaseVelocity` | h 938–954 |
| `bJustTeleported` **не** выставляется `MoveUpdatedComponent`; только конструктор, застревание, `OnTeleported`, подгонка пола | cpp 750, 5549, 6543, 11375 |
| `GetMovementBaseObject() const` возвращает `UObject*` | h 1409 |
| `FRotator(Pitch, Yaw, Roll)` — именно в этом порядке | `Runtime/Core/Public/Math/Rotator.h` 101, 566 |
| Капсула персонажа по умолчанию 34 × 88 см | `Private/Character.cpp` 78 |
| `SpawnActorDeferred(Class, Transform, Owner, Instigator, CollisionHandling)` | `Classes/Engine/World.h` 3851 |
| `AddTickPrerequisiteComponent` есть у `UActorComponent` | `Classes/Components/ActorComponent.h` 1359 |

Второй пункт — самое важное. Он означает, что R6 выполняется **штатным флагом**, включаемым на время сбивания, а не переопределением `OnMovementModeChanged` с восстановлением опоры вручную.

---

### Task 1: Математика наведения

Намерение турели и наведение — в `StarForgeCore`, под тестами. Ошибка здесь не падает: турель просто целится не туда, куда говорит индикатор, и это ищут неделями.

**Files:**
- Create: `Source/StarForgeCore/Public/TurretMath.h`
- Create: `Source/StarForgeCore/Private/TurretMath.cpp`
- Create: `Source/StarForgeCore/Private/Tests/TurretMathTest.cpp`

**Interfaces:**
- Consumes: `FTransform`, `FRotator`, `FMath` из `Core`.
- Produces: `StarForge::FTurretIntent { FVector TargetWorldLocation; bool bHasTarget; bool bFiring; }`; в `StarForge::Turret`: `FTurretLimits { double MaxYawRad, MinPitchRad, MaxPitchRad, TraverseRateRadPerSec, OnTargetToleranceRad; }`, `FTurretAim { double YawRad, PitchRad; }`, `enum class EAimState : uint8 { OnTarget, Slewing, CannotBear }`, `FAimStep { FTurretAim Aim; EAimState State; }`, `bool ComputeBearing(const FTransform& MountWorld, const FVector& TargetWorld, FTurretAim& OutBearing)`, `bool CanBear(const FTurretAim& Bearing, const FTurretLimits& Limits)`, `FAimStep StepAim(const FTurretAim& Current, const FTurretAim& Bearing, const FTurretLimits& Limits, double DeltaSeconds)`, `FRotator AimToRotator(const FTurretAim& Aim)`.

**Соглашение о знаках — движка, и тесты сверяют его с движком, а не с числом.** Рысканье — вокруг Z крепления, положительное от +X к +Y; возвышение — над плоскостью XY крепления, положительное к +Z. Это ровно то, что делает `FRotator`: `FRotator(0, 90, 0).Vector()` есть `(0, 1, 0)`, `FRotator(90, 0, 0).Vector()` есть `(0, 0, 1)`. Тест `AimRoundTripsThroughTheRotator` замыкает круг: точка вдоль ствола, повёрнутого `AimToRotator(Aim)`, пеленгуется обратно в тот же `Aim`. Пока этот тест зелёный, картинка и числа не могут разойтись.

**Три ответа, а не один.** `StepAim` возвращает состояние вместе с углом: `OnTarget`, `Slewing`, `CannotBear`. Турель на упоре +120° при цели на +130° имеет тот же угол, что турель на цели ровно на +120°, — угол не различает, состояние различает. `CannotBear` говорится с **первого шага** подхода к упору, а не по прибытии: ответ — о цели, а не о том, докуда доехал ствол.

**Рысканье всегда идёт через ноль.** Упоры делают достижимую дугу одним отрезком, содержащим ноль, так что подход — прямое движение по отрезку без переворотов через ±180°. При `MaxYawRad` ровно π это по-прежнему правило: цель на +179° из −179° достигается длинным путём. Кольцевое крепление без упора — другая функция, не эта; сказано в комментарии, а не спрятано.

- [ ] **Step 1: Создать ветку**

```bash
git checkout -b feat/gunner-station main
```

- [ ] **Step 2: Написать падающие тесты**

`Source/StarForgeCore/Private/Tests/TurretMathTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "TurretMath.h"

namespace
{
	constexpr double Deg = UE_DOUBLE_PI / 180.0;

	/** The rig's limits: yaw +/-120, pitch -30..+60, 90 deg/s, half a degree is on target. */
	StarForge::Turret::FTurretLimits MakeLimits()
	{
		StarForge::Turret::FTurretLimits Limits;
		Limits.MaxYawRad = 120.0 * Deg;
		Limits.MinPitchRad = -30.0 * Deg;
		Limits.MaxPitchRad = 60.0 * Deg;
		Limits.TraverseRateRadPerSec = 90.0 * Deg;
		Limits.OnTargetToleranceRad = 0.5 * Deg;
		return Limits;
	}

	/** One frame at the rig's fixed 60 fps. */
	constexpr double Frame = 1.0 / 60.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingDeadAheadTest,
	"StarForge.Core.Turret.BearingDeadAhead",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingDeadAheadTest::RunTest(const FString& Parameters)
{
	StarForge::Turret::FTurretAim Bearing;
	const bool bFound = StarForge::Turret::ComputeBearing(
		FTransform::Identity, FVector(100.0, 0.0, 0.0), Bearing);

	TestTrue(TEXT("a target straight ahead has a bearing"), bFound);
	TestTrue(TEXT("yaw is zero"), FMath::IsNearlyZero(Bearing.YawRad, 1e-9));
	TestTrue(TEXT("pitch is zero"), FMath::IsNearlyZero(Bearing.PitchRad, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingYawMatchesEngineSignTest,
	"StarForge.Core.Turret.BearingYawMatchesEngineSign",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingYawMatchesEngineSignTest::RunTest(const FString& Parameters)
{
	// Checked against the engine's own idea of a +45 yaw rather than against a number
	// believed to be right. FRotator's yaw sends +X toward +Y; the bearing has to agree
	// or the barrel swings one way while the readout prints the other.
	const FVector Ahead = FRotator(0.0, 45.0, 0.0).Vector() * 100.0;

	StarForge::Turret::FTurretAim Bearing;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(FTransform::Identity, Ahead, Bearing));
	TestTrue(TEXT("a +45 engine yaw reads as +45 here"),
		FMath::IsNearlyEqual(Bearing.YawRad, 45.0 * Deg, 1e-6));
	TestTrue(TEXT("and carries no pitch"), FMath::IsNearlyZero(Bearing.PitchRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingPitchIsElevationTest,
	"StarForge.Core.Turret.BearingPitchIsElevation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingPitchIsElevationTest::RunTest(const FString& Parameters)
{
	// A +45 engine pitch lifts +X toward +Z, and so does this pitch.
	const FVector Up45 = FRotator(45.0, 0.0, 0.0).Vector() * 100.0;

	StarForge::Turret::FTurretAim Bearing;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(FTransform::Identity, Up45, Bearing));
	TestTrue(TEXT("a +45 engine pitch reads as +45 here"),
		FMath::IsNearlyEqual(Bearing.PitchRad, 45.0 * Deg, 1e-6));
	TestTrue(TEXT("and carries no yaw"), FMath::IsNearlyZero(Bearing.YawRad, 1e-6));

	// Straight up is ninety, and yaw is then undefined but must not be garbage: atan2
	// of (0, 0) is zero, which is what a barrel pointing up would sensibly keep.
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(
		FTransform::Identity, FVector(0.0, 0.0, 100.0), Bearing));
	TestTrue(TEXT("straight up is ninety degrees of pitch"),
		FMath::IsNearlyEqual(Bearing.PitchRad, 90.0 * Deg, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingIsTakenInTheMountFrameTest,
	"StarForge.Core.Turret.BearingIsTakenInTheMountFrame",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingIsTakenInTheMountFrameTest::RunTest(const FString& Parameters)
{
	// The mount sits ten metres out along world Y and faces world +Y. A target a metre
	// further along Y is dead ahead of it. One a metre toward world -X is off its right
	// shoulder, because a +90 yaw sends local +Y onto world -X.
	const FTransform Mount(FRotator(0.0, 90.0, 0.0).Quaternion(), FVector(0.0, 1000.0, 0.0));

	StarForge::Turret::FTurretAim Ahead;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(Mount, FVector(0.0, 1100.0, 0.0), Ahead));
	TestTrue(TEXT("a target along the mount's forward is dead ahead"),
		FMath::IsNearlyZero(Ahead.YawRad, 1e-6) && FMath::IsNearlyZero(Ahead.PitchRad, 1e-6));

	StarForge::Turret::FTurretAim Right;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(Mount, FVector(-100.0, 1000.0, 0.0), Right));
	TestTrue(TEXT("a target off the mount's right shoulder bears +90"),
		FMath::IsNearlyEqual(Right.YawRad, 90.0 * Deg, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingRefusesACoincidentTargetTest,
	"StarForge.Core.Turret.BearingRefusesACoincidentTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingRefusesACoincidentTargetTest::RunTest(const FString& Parameters)
{
	// There is no direction to a point you are standing on. Any angle returned for it
	// would be a fabrication, so the answer is "no bearing", and the output is left
	// alone so a caller cannot mistake a stale value for a fresh one.
	const FTransform Mount(FQuat::Identity, FVector(5.0, 6.0, 7.0));

	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 1.23;
	Bearing.PitchRad = 4.56;

	TestFalse(TEXT("a target on the mount has no bearing"),
		StarForge::Turret::ComputeBearing(Mount, FVector(5.0, 6.0, 7.0), Bearing));
	TestTrue(TEXT("and the output is untouched"),
		FMath::IsNearlyEqual(Bearing.YawRad, 1.23, 1e-12) && FMath::IsNearlyEqual(Bearing.PitchRad, 4.56, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretAimRoundTripsThroughTheRotatorTest,
	"StarForge.Core.Turret.AimRoundTripsThroughTheRotator",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretAimRoundTripsThroughTheRotatorTest::RunTest(const FString& Parameters)
{
	// The executor turns the barrel by AimToRotator and the readout prints the aim.
	// The two agree only if a point along the rotated barrel bears back at the same
	// aim -- which is the whole contract between the numbers and the picture.
	StarForge::Turret::FTurretAim Aim;
	Aim.YawRad = 30.0 * Deg;
	Aim.PitchRad = 20.0 * Deg;

	const FVector AlongBarrel = StarForge::Turret::AimToRotator(Aim).Vector() * 250.0;

	StarForge::Turret::FTurretAim Back;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(FTransform::Identity, AlongBarrel, Back));
	TestTrue(TEXT("yaw survives the round trip"), FMath::IsNearlyEqual(Back.YawRad, Aim.YawRad, 1e-5));
	TestTrue(TEXT("pitch survives the round trip"), FMath::IsNearlyEqual(Back.PitchRad, Aim.PitchRad, 1e-5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepIsRateLimitedTest,
	"StarForge.Core.Turret.StepIsRateLimited",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepIsRateLimitedTest::RunTest(const FString& Parameters)
{
	// Ninety degrees a second over a sixtieth of a second is a degree and a half, on
	// each axis independently. 90 / 60 = 1.5.
	StarForge::Turret::FTurretAim Current;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 45.0 * Deg;
	Bearing.PitchRad = 45.0 * Deg;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("yaw moves a degree and a half"), FMath::IsNearlyEqual(Step.Aim.YawRad, 1.5 * Deg, 1e-9));
	TestTrue(TEXT("pitch moves a degree and a half"), FMath::IsNearlyEqual(Step.Aim.PitchRad, 1.5 * Deg, 1e-9));
	TestTrue(TEXT("and it is still slewing"), Step.State == StarForge::Turret::EAimState::Slewing);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepReportsOnTargetTest,
	"StarForge.Core.Turret.StepReportsOnTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepReportsOnTargetTest::RunTest(const FString& Parameters)
{
	// A tenth of a degree short: the step is bigger than the error, so the barrel
	// lands exactly on the bearing rather than overshooting it, and says so.
	StarForge::Turret::FTurretAim Current;
	Current.YawRad = 44.9 * Deg;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 45.0 * Deg;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel arrives on the bearing"), FMath::IsNearlyEqual(Step.Aim.YawRad, 45.0 * Deg, 1e-9));
	TestTrue(TEXT("and reports on target"), Step.State == StarForge::Turret::EAimState::OnTarget);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepAtTheStopIsNotOnTargetTest,
	"StarForge.Core.Turret.StepAtTheStopIsNotOnTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepAtTheStopIsNotOnTargetTest::RunTest(const FString& Parameters)
{
	// Parked on the +120 stop with the target at +130. The angle cannot change, and
	// that is precisely why the angle alone would lie: it reads the same as a barrel
	// on a target at exactly 120. The state is what tells the two apart.
	StarForge::Turret::FTurretAim Current;
	Current.YawRad = 120.0 * Deg;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 130.0 * Deg;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel stays on the stop"), FMath::IsNearlyEqual(Step.Aim.YawRad, 120.0 * Deg, 1e-9));
	TestTrue(TEXT("and says it cannot bear, not that it is on target"),
		Step.State == StarForge::Turret::EAimState::CannotBear);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepSaysCannotBearFromTheFirstStepTest,
	"StarForge.Core.Turret.StepSaysCannotBearFromTheFirstStep",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepSaysCannotBearFromTheFirstStepTest::RunTest(const FString& Parameters)
{
	// Below the pitch floor. The barrel starts toward the stop at the traverse rate
	// and says CannotBear from the first step, not only when it gets there: the
	// answer is about the target, not about how far the barrel has got.
	StarForge::Turret::FTurretAim Current;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.PitchRad = -40.0 * Deg;

	const StarForge::Turret::FAimStep First =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("pitch starts down at the traverse rate"), FMath::IsNearlyEqual(First.Aim.PitchRad, -1.5 * Deg, 1e-9));
	TestTrue(TEXT("and it already says cannot bear"), First.State == StarForge::Turret::EAimState::CannotBear);

	// Parked on the floor stop, still cannot bear, and never below the stop.
	Current.PitchRad = -30.0 * Deg;
	const StarForge::Turret::FAimStep Parked =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel does not go below the floor stop"), FMath::IsNearlyEqual(Parked.Aim.PitchRad, -30.0 * Deg, 1e-9));
	TestTrue(TEXT("and still cannot bear"), Parked.State == StarForge::Turret::EAimState::CannotBear);

	return true;
}
```

- [ ] **Step 3: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается ошибка вида `fatal error C1083: Cannot open include file: 'TurretMath.h'`. **Это шаг, а не формальность:** в плане «а» тесты правки взгляда прошли с первого раза, потому что сборка между ними и реализацией не состоялась, и годность тестов пришлось доказывать, ломая код обратно.

- [ ] **Step 4: Написать заголовок**

`Source/StarForgeCore/Public/TurretMath.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

namespace StarForge
{
	/**
	 * What a gunner asks of a turret.
	 *
	 * Spec 4 gives this two fields, a world position and a trigger. A third is added
	 * here: bHasTarget. A gunner with nothing to shoot at cannot say so through a
	 * position alone, because a zeroed position is the world origin, which is a legal
	 * place for a target to be -- the turret would swing to it and report itself on
	 * target, and the readout would look exactly as it does with a real target there.
	 * The same reasoning put "return false" on the operator interface instead of an
	 * empty intent.
	 *
	 * The position is in whatever length unit the caller uses consistently; the game
	 * passes centimetres, as it does to the frame maths.
	 */
	struct STARFORGECORE_API FTurretIntent
	{
		FVector TargetWorldLocation = FVector::ZeroVector;
		bool bHasTarget = false;
		bool bFiring = false;
	};
}

/**
 * Turret aiming. Unit-agnostic in length; angles in radians.
 *
 * Yaw is about the mount's Z, positive from +X toward +Y; pitch is elevation above
 * the mount's XY plane, positive toward +Z. Those are the engine's own conventions
 * for FRotator, which is why AimToRotator lives in this file and under test: the
 * barrel the game turns by that rotator has to point where these numbers say.
 */
namespace StarForge::Turret
{
	/** The stops and the slew rate. A mount without a yaw stop is not this mount. */
	struct STARFORGECORE_API FTurretLimits
	{
		/** Yaw travel each way from straight ahead, radians. At most pi. */
		double MaxYawRad = 2.0 * UE_DOUBLE_PI / 3.0;

		/** Radians; negative is below the mount's plane. */
		double MinPitchRad = -UE_DOUBLE_PI / 6.0;
		double MaxPitchRad = UE_DOUBLE_PI / 3.0;

		/** Radians per second, on each axis independently. */
		double TraverseRateRadPerSec = UE_DOUBLE_PI / 2.0;

		/** Within this of the bearing on both axes counts as on target. Radians. */
		double OnTargetToleranceRad = 0.5 * UE_DOUBLE_PI / 180.0;
	};

	/** Where the barrel points, in the mount's frame. */
	struct STARFORGECORE_API FTurretAim
	{
		double YawRad = 0.0;
		double PitchRad = 0.0;
	};

	/**
	 * Three answers, not one. A turret sitting at its stop is at the same angle whether
	 * the target is just inside the stop or far beyond it, so an angle alone cannot say
	 * whether the barrel is on the target or merely as close as it can get. CannotBear
	 * is that second case, said out loud.
	 */
	enum class EAimState : uint8
	{
		OnTarget,
		Slewing,
		CannotBear
	};

	struct STARFORGECORE_API FAimStep
	{
		FTurretAim Aim;
		EAimState State = EAimState::Slewing;
	};

	/**
	 * The mount-frame yaw and pitch of a world position. Returns false, and leaves
	 * OutBearing untouched, when the target sits on the mount itself: there is no
	 * direction to a point you are standing on, and any angle returned for it would be
	 * a fabrication.
	 */
	STARFORGECORE_API bool ComputeBearing(
		const FTransform& MountWorld,
		const FVector& TargetWorld,
		FTurretAim& OutBearing);

	/** True when the bearing lies inside the stops on both axes. */
	STARFORGECORE_API bool CanBear(const FTurretAim& Bearing, const FTurretLimits& Limits);

	/**
	 * One tick of the turret's approach to a bearing.
	 *
	 * Each axis moves toward the bearing by at most the traverse rate times
	 * DeltaSeconds, and never past its stop. A bearing outside the stops is approached
	 * to the nearest stop, and the state says CannotBear for the whole of that
	 * approach, not only on arrival -- the answer is about the target, not about how
	 * far the barrel has got.
	 *
	 * Yaw always moves through zero, because the stops make the reachable arc a single
	 * interval containing zero. With MaxYawRad at exactly pi that is still the rule, so
	 * a target at +179 degrees seen from -179 is reached the long way round. A ring
	 * mount with no stop would need a different function; this one is not it.
	 */
	STARFORGECORE_API FAimStep StepAim(
		const FTurretAim& Current,
		const FTurretAim& Bearing,
		const FTurretLimits& Limits,
		double DeltaSeconds);

	/**
	 * The rotator that points a barrel along an aim: pitch and yaw in degrees, no
	 * roll. FRotator's constructor takes (Pitch, Yaw, Roll) in that order, and getting
	 * two of those swapped is a barrel that points somewhere the readout does not say.
	 */
	STARFORGECORE_API FRotator AimToRotator(const FTurretAim& Aim);
}
```

- [ ] **Step 5: Реализовать**

`Source/StarForgeCore/Private/TurretMath.cpp`:

```cpp
#include "TurretMath.h"

namespace StarForge::Turret
{
	bool ComputeBearing(const FTransform& MountWorld, const FVector& TargetWorld, FTurretAim& OutBearing)
	{
		const FVector Local = MountWorld.InverseTransformPosition(TargetWorld);

		// A target on the mount has no direction. The threshold is tiny and unitless on
		// purpose: it catches the degenerate case and is not a minimum range.
		if (Local.IsNearlyZero(1e-6))
		{
			return false;
		}

		// atan2 gives (-pi, pi] with the sign the engine's yaw has: +X toward +Y.
		OutBearing.YawRad = FMath::Atan2(Local.Y, Local.X);
		OutBearing.PitchRad = FMath::Atan2(Local.Z, FMath::Sqrt(Local.X * Local.X + Local.Y * Local.Y));
		return true;
	}

	bool CanBear(const FTurretAim& Bearing, const FTurretLimits& Limits)
	{
		return FMath::Abs(Bearing.YawRad) <= Limits.MaxYawRad
			&& Bearing.PitchRad >= Limits.MinPitchRad
			&& Bearing.PitchRad <= Limits.MaxPitchRad;
	}

	FAimStep StepAim(
		const FTurretAim& Current,
		const FTurretAim& Bearing,
		const FTurretLimits& Limits,
		double DeltaSeconds)
	{
		// Where the barrel is going: the bearing, or the nearest point on the stops.
		FTurretAim Goal;
		Goal.YawRad = FMath::Clamp(Bearing.YawRad, -Limits.MaxYawRad, Limits.MaxYawRad);
		Goal.PitchRad = FMath::Clamp(Bearing.PitchRad, Limits.MinPitchRad, Limits.MaxPitchRad);

		const double MaxStep = FMath::Max(0.0, Limits.TraverseRateRadPerSec * DeltaSeconds);

		FAimStep Result;
		Result.Aim.YawRad = Current.YawRad + FMath::Clamp(Goal.YawRad - Current.YawRad, -MaxStep, MaxStep);
		Result.Aim.PitchRad = Current.PitchRad + FMath::Clamp(Goal.PitchRad - Current.PitchRad, -MaxStep, MaxStep);

		// Never outside the stops, even if Current arrived from outside them: a limits
		// edit while the barrel was deployed must pull it in, not leave it stranded.
		Result.Aim.YawRad = FMath::Clamp(Result.Aim.YawRad, -Limits.MaxYawRad, Limits.MaxYawRad);
		Result.Aim.PitchRad = FMath::Clamp(Result.Aim.PitchRad, Limits.MinPitchRad, Limits.MaxPitchRad);

		if (!CanBear(Bearing, Limits))
		{
			Result.State = EAimState::CannotBear;
		}
		else if (FMath::Abs(Result.Aim.YawRad - Bearing.YawRad) <= Limits.OnTargetToleranceRad
			&& FMath::Abs(Result.Aim.PitchRad - Bearing.PitchRad) <= Limits.OnTargetToleranceRad)
		{
			Result.State = EAimState::OnTarget;
		}
		else
		{
			Result.State = EAimState::Slewing;
		}

		return Result;
	}

	FRotator AimToRotator(const FTurretAim& Aim)
	{
		return FRotator(
			FMath::RadiansToDegrees(Aim.PitchRad),
			FMath::RadiansToDegrees(Aim.YawRad),
			0.0);
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

В `Saved/Logs/StarForge.log` ожидается **47** строк `Test Completed. Result=Passed` (37 прежних плюс десять новых) и ни одной `Result=Failed`. Считать так:

```bash
grep -c "Test Completed. Result=Passed" Saved/Logs/StarForge.log
grep -c "Test Completed. Result=Failed" Saved/Logs/StarForge.log
```

Ожидается `47` и `0`. Проверить, что зависимости `StarForgeCore` не изменились: `git diff Source/StarForgeCore/StarForgeCore.Build.cs` пуст.

- [ ] **Step 7: Коммит**

```bash
git add Source/StarForgeCore/Public/TurretMath.h Source/StarForgeCore/Private/TurretMath.cpp Source/StarForgeCore/Private/Tests/TurretMathTest.cpp
git commit -F - <<'MSG'
feat(core): aim a turret, and say when it cannot

The aiming maths lives in StarForgeCore under tests because its failure mode is a
barrel that points a little away from where the readout says, which is the kind of
defect that gets hunted for weeks in the wrong place. The sign convention is not
asserted against numbers believed to be right but against FRotator itself, and one
test round-trips an aim through the rotator the executor will turn the barrel by, so
the picture and the numbers cannot disagree.

The step returns a state beside the angle. A turret parked on its stop is at the
same angle whether the target is just inside the stop or far beyond it, so an angle
alone cannot say whether the barrel is on the target or merely as close as it can
get. CannotBear is that second case said out loud, and it is said from the first
step of the approach rather than on arrival, because the answer is about the target.

The intent carries bHasTarget as well as a position. A zeroed position is the world
origin, a legal place for a target to be, and a gunner with nothing to shoot at has
to be able to say so without the turret swinging to the origin and reporting itself
on target.
MSG
```

---

### Task 2: Мишень стенда

Что-то, за чем турель будет следить: серый шар на скриптовой орбите вокруг позиции корабля.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFTargetDrone.h`
- Create: `Source/StarForge/Private/Ship/SFTargetDrone.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`
- Modify: `CLAUDE.md` (таблица переменных)

**Interfaces:**
- Consumes: `StarForge::Units::MetresToCm`.
- Produces: `ASFTargetDrone` с `void SetAnchor(AActor* InAnchor)`, `FVector EvaluatePathNow() const`, `bool IsPresent() const`; `ASFTestShip::GetTargetDrone() -> ASFTargetDrone*`; переменная `sf.TargetDrone` (по умолчанию 1).

**Почему траектория считается по часам по требованию, а не читается из тика.** Кто бы ни спросил «где мишень», он получит ответ этого кадра независимо от того, тикнула ли мишень раньше него. Иначе пост канонира, тикающий раньше мишени, видел бы её на кадр позади — под тягой на 200 м/с это 3.3 м при дистанции 30 м, то есть 6° ошибки пеленга, и «на цели» не наступало бы никогда. Порядок тиков не утверждается и не нужен; меш может отставать на кадр — по мешу никто не целится.

**Орбита.** Радиус 30 м, 10°/с, плоскость наклонена на 20° к мировой XY вокруг мировой X. Смещение от якоря: `R (cos φ, sin φ · cos 20°, sin φ · sin 20°)`, φ = 10° · t. Наклон нужен, чтобы возвышение менялось; 20° выбрано так, что возвышение с крепления на высоте +2.8 м (задача 3) остаётся в пределах −30..+60° на всей орбите: `z − 2.8 ∈ [−13.06, 7.46]` м при горизонтальной дальности не меньше 28.2 м даёт `[−24.9°, +14.8°]`. Упор по рысканью ±120° при этом срабатывает дважды за оборот — задача 5 это предсказывает по секундам.

- [ ] **Step 1: Написать мишень**

`Source/StarForge/Public/Ship/SFTargetDrone.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTargetDrone.generated.h"

class UStaticMeshComponent;

/**
 * Something for the turret to track: a grey sphere on a scripted orbit.
 *
 * It exists because a fixed point cannot show that aiming tracks -- a barrel parked
 * on a stationary target is indistinguishable from a barrel that has stopped. The
 * orbit is kinematic and centred on an anchor's position in WORLD axes: it follows
 * the ship wherever it flies, but does not turn with it, so a rolling ship sees the
 * bearing sweep at the roll rate and the turret has to counter-rotate to hold it.
 *
 * No collision, no physics, no health. It is not a pirate and not a stage-4 target;
 * it is a moving point with a mesh on it so a person can see where the point is.
 *
 * The path is evaluated from the world clock on demand rather than read back from
 * this actor's tick, so whoever asks where the drone is gets this frame's answer
 * whatever the tick order. The mesh may trail the true point by a frame; nothing
 * aims at the mesh.
 */
UCLASS()
class STARFORGE_API ASFTargetDrone : public AActor
{
	GENERATED_BODY()

public:
	ASFTargetDrone();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Orbit radius about the anchor, METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Drone")
	double OrbitRadiusMetres = 30.0;

	/** Rate along the orbit, degrees per second, from +X toward +Y seen from world +Z. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Drone")
	double OrbitRateDegPerSec = 10.0;

	/** Tilt of the orbit plane about world X, degrees. Zero is the world XY plane. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Drone")
	double OrbitTiltDegrees = 20.0;

	/** The actor whose position the orbit is centred on. Unset orbits the world origin. */
	void SetAnchor(AActor* InAnchor);

	/** Where the drone is at this instant, in world centimetres. */
	FVector EvaluatePathNow() const;

	/**
	 * False while sf.TargetDrone is 0: the drone is withdrawn and must be treated as
	 * absent by anyone tracking it. Absence is a case the turret has to report, and
	 * this is the switch that produces it.
	 */
	bool IsPresent() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UStaticMeshComponent> Body;

	TWeakObjectPtr<AActor> Anchor;

	/** World time at BeginPlay; the orbit phase is measured from here. */
	double SpawnTimeSeconds = 0.0;

	/** Edge detection for the presence switch, so the log says it once, not sixty times a second. */
	bool bWasPresent = true;
};
```

`Source/StarForge/Private/Ship/SFTargetDrone.cpp`:

```cpp
#include "Ship/SFTargetDrone.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "StarForgeLog.h"
#include "Units.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * Declared in the file that reads it, like every other sf.* variable: a lookup by
	 * name from elsewhere returns zero when it fails, and a zero here would be a
	 * withdrawn drone nobody asked to withdraw.
	 */
	TAutoConsoleVariable<int32> CVarTargetDrone(TEXT("sf.TargetDrone"), 1,
		TEXT("1: the target drone flies its orbit. 0: it is withdrawn, and the turret has to say it has no target."),
		ECVF_Default);

	/** Two metres across: the engine sphere is 100 cm, so scale is diameter in metres. */
	constexpr double DroneScale = 2.0;
}

ASFTargetDrone::ASFTargetDrone()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetRelativeScale3D(FVector(DroneScale));

	// A moving point with a mesh on it. It must not be able to stop, push or catch
	// anything: the ship flies through its orbit, and the occupant's sweeps must never
	// meet it.
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetSimulatePhysics(false);
	Body->SetEnableGravity(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Body->SetStaticMesh(SphereMesh.Object);
	}
}

void ASFTargetDrone::SetAnchor(AActor* InAnchor)
{
	Anchor = InAnchor;
}

void ASFTargetDrone::BeginPlay()
{
	Super::BeginPlay();

	SpawnTimeSeconds = GetWorld()->GetTimeSeconds();
	bWasPresent = IsPresent();

	UE_LOG(LogStarForge, Display,
		TEXT("Target drone spawned: orbit %.1f m radius at %.1f deg/s, tilted %.1f deg, anchored to %s, %s."),
		OrbitRadiusMetres, OrbitRateDegPerSec, OrbitTiltDegrees,
		Anchor.IsValid() ? *Anchor->GetName() : TEXT("the world origin (no anchor)"),
		bWasPresent ? TEXT("present") : TEXT("WITHDRAWN (sf.TargetDrone 0)"));

	SetActorLocation(EvaluatePathNow());
	SetActorHiddenInGame(!bWasPresent);
}

bool ASFTargetDrone::IsPresent() const
{
	return CVarTargetDrone.GetValueOnGameThread() != 0;
}

FVector ASFTargetDrone::EvaluatePathNow() const
{
	const double Elapsed = GetWorld()->GetTimeSeconds() - SpawnTimeSeconds;
	const double Phase = FMath::DegreesToRadians(OrbitRateDegPerSec * Elapsed);
	const double Tilt = FMath::DegreesToRadians(OrbitTiltDegrees);

	// A circle in a plane tilted about world X: the Y extent is foreshortened by the
	// tilt and the Z extent is what it gives up. Metres until the conversion below.
	const FVector OffsetMetres(
		OrbitRadiusMetres * FMath::Cos(Phase),
		OrbitRadiusMetres * FMath::Sin(Phase) * FMath::Cos(Tilt),
		OrbitRadiusMetres * FMath::Sin(Phase) * FMath::Sin(Tilt));

	const FVector OffsetCm(
		StarForge::Units::MetresToCm(OffsetMetres.X),
		StarForge::Units::MetresToCm(OffsetMetres.Y),
		StarForge::Units::MetresToCm(OffsetMetres.Z));

	const FVector Centre = Anchor.IsValid() ? Anchor->GetActorLocation() : FVector::ZeroVector;
	return Centre + OffsetCm;
}

void ASFTargetDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SetActorLocation(EvaluatePathNow());

	const bool bPresent = IsPresent();
	if (bPresent != bWasPresent)
	{
		// Said once per change. The turret's readout says it every second for as long
		// as it lasts; this line is for finding the moment in the log.
		UE_LOG(LogStarForge, Display, TEXT("Target drone %s."),
			bPresent ? TEXT("present again (sf.TargetDrone 1)") : TEXT("WITHDRAWN (sf.TargetDrone 0)"));
		SetActorHiddenInGame(!bPresent);
		bWasPresent = bPresent;
	}
}
```

- [ ] **Step 2: Спавнить её из корабля**

В `SFTestShip.h` добавить forward-declare `class ASFTargetDrone;`, в public:

```cpp
	/** What the gunner post's automation tracks. Spawned by BeginPlay; null if that failed. */
	ASFTargetDrone* GetTargetDrone() const { return TargetDrone; }
```

и в private:

```cpp
	UPROPERTY()
	TObjectPtr<ASFTargetDrone> TargetDrone;
```

В `SFTestShip.cpp` включения:

```cpp
#include "Engine/World.h"
#include "Ship/SFTargetDrone.h"
```

и в конец `ASFTestShip::BeginPlay`, после строки `Physics: ...`:

```cpp
	// The rig assembles itself, so the thing the turret tracks is spawned here rather
	// than placed: L_FrameSpike is empty by design. Anchored to this ship's position so
	// it stays in range whichever way the ship is driven, and in world axes so it does
	// not turn with the ship, which is what makes a rolling hull a test of the aim.
	//
	// Deferred, because the drone's BeginPlay logs its anchor and takes its first
	// position from it, and a spawn that finishes before SetAnchor would log "the world
	// origin" for a drone that is about to be anchored to this ship.
	FTransform DroneSpawn = GetActorTransform();
	DroneSpawn.SetScale3D(FVector::OneVector);
	TargetDrone = GetWorld()->SpawnActorDeferred<ASFTargetDrone>(
		ASFTargetDrone::StaticClass(), DroneSpawn, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (TargetDrone)
	{
		TargetDrone->SetAnchor(this);
		TargetDrone->FinishSpawning(DroneSpawn);
	}
	else
	{
		UE_LOG(LogStarForge, Error, TEXT("Target drone failed to spawn; the turret will have nothing to track."));
	}
```

- [ ] **Step 3: Собрать и прогнать headless дважды**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Новый `UCLASS` — Live Coding не поможет; собирать с закрытым редактором.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=5 -ExecCmds="sf.ShipProfile 0" -log
```

В `Saved/Logs/StarForge.log` ожидается одна строка:

```
LogStarForge: Target drone spawned: orbit 30.0 m radius at 10.0 deg/s, tilted 20.0 deg, anchored to SFTestShip_0, present.
```

(имя актора — то, что движок дал кораблю, спавненному из `StaticClass()`; важно, что это не `the world origin`). Затем второй прогон с `-ExecCmds="sf.ShipProfile 0, sf.TargetDrone 0"`: `-ExecCmds` выполняется после `BeginPlay`, поэтому ожидается строка спавна с `present` **и** следом `Target drone WITHDRAWN (sf.TargetDrone 0).` Если второй строки нет — переменная не читается из тика, и переключатель не работает как переключатель.

- [ ] **Step 4: Записать переменную в `CLAUDE.md`**

В таблицу консольных переменных `CLAUDE.md`, после строки `sf.DriftDebug`, добавить:

```markdown
| `sf.TargetDrone` | 1 | 0 withdraws the rig's target drone; the turret must then report no target rather than aim at the origin |
```

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFTargetDrone.h Source/StarForge/Private/Ship/SFTargetDrone.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp CLAUDE.md
git commit -F - <<'MSG'
feat(ship): give the rig something for a turret to track

A fixed point cannot show that aiming tracks: a barrel parked on a stationary target
looks exactly like a barrel that has stopped. So the target moves, on a scripted
orbit, and the orbit is centred on the ship's position in world axes -- it follows
the ship wherever the ship flies, but does not turn with it, which is what makes a
rolling hull a test of the aim at all.

The path is evaluated from the clock on demand rather than read back from the
drone's tick. Whoever asks gets this frame's answer whatever the tick order; a post
that ticked before the drone would otherwise see it a frame behind, which at 200 m/s
and thirty metres is six degrees of bearing and a turret that is never on target.

It can be withdrawn with a console variable so that a missing target is a case the
turret has to report, not a position it silently aims at. It has no collision, no
physics and no health, because it is not a pirate and stage 4 is not this plan.
MSG
```

---

### Task 3: Намерение турели в интерфейсе и исполнитель

Интерфейс оператора получает второй метод (R9). Появляется исполнитель — крепление, которое доворачивает ствол к пеленгу в пределах упоров и скорости обхода и помнит, в каком оно состоянии.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFStationOperator.h`
- Create: `Source/StarForge/Public/Ship/SFTurretComponent.h`
- Create: `Source/StarForge/Private/Ship/SFTurretComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`

**Interfaces:**
- Consumes: всё из `TurretMath.h` (Task 1).
- Produces: `ISFStationOperator::ProvideTurretIntent(StarForge::FTurretIntent& Out) -> bool` (по умолчанию `false`); `UENUM ESFTurretState { Uncommanded, NoTarget, Slewing, OnTarget, CannotBear }`; `USFTurretComponent : USceneComponent` с `double MaxYawDegrees, MinPitchDegrees, MaxPitchDegrees, TraverseRateDegPerSec, OnTargetToleranceDegrees`, `void SetBarrel(USceneComponent*)`, `StarForge::Turret::FTurretLimits BuildLimits() const`, `FTransform GetMountTransform() const`, `void ApplyBearing(const StarForge::Turret::FTurretAim& Bearing, bool bFiring, double DeltaSeconds)`, `void ApplyNoTarget(bool bFiring)`, `void ClearCommand()`, `StarForge::Turret::FTurretAim GetAim() const`, `StarForge::Turret::FTurretAim GetLastBearing() const`, `bool HasBearing() const`, `ESFTurretState GetState() const`, `bool IsFiring() const`, `static const TCHAR* StateName(ESFTurretState)`; `ASFTestShip::GetTurret() -> USFTurretComponent*`.

**Пять состояний у исполнителя против трёх у математики.** Математика знает только пеленг: на цели, ведёт, не может. Исполнитель знает ещё два случая, которых математика не видит: никто не командовал (`Uncommanded`) и командовали, но цели нет (`NoTarget`). Оба — состояния мира, не дефекты, но оба обязаны быть названы: ствол, который стоит, потому что стоять велено, выглядит как ствол, который стоит, потому что за ним никого нет.

**Ствол создаёт корабль, а не компонент.** Исполнитель получает `USceneComponent` через `SetBarrel` и поворачивает его; без ствола он считает и докладывает точно так же — headless-прогонам меш не нужен. Создавать вложенные компоненты из конструктора компонента можно, но это поведение, которого в этом проекте никто не проверял, и оно не нужно: у корабля уже есть привычка собирать себя из примитивов.

**Высота крепления — вывод, не подбор.** Внутренняя грань потолочной панели: `InteriorHalfExtentCm − PanelThickness × CubeEdgeCm / 2 = 400 − 20/2 = 390` см (`SFTestShip.cpp`, константы `PanelThickness = 0.2`, `CubeEdgeCm = 100`). Ствол длиной 100 см на верхнем упоре +60° поднимается на `100 · sin 60° = 86.6` см над креплением. Зазор 10 см: `390 − 100 − 10 = 280` см. Проверка: `280 + 86.6 = 366.6 < 390`. Вниз на −30° ствол опускается до `280 − 50 = 230`; верх капсулы стоящего — `−300 + 88 = −212` (капсула 34 × 88, `Character.cpp:78`, центр на `−InteriorHalfExtentCm + SpawnClearanceCm = −300`). Между ними четыре с половиной метра.

- [ ] **Step 1: Расширить интерфейс**

В `Source/StarForge/Public/Ship/SFStationOperator.h` добавить включение после `FlightController.h`:

```cpp
#include "TurretMath.h"
```

и в `ISFStationOperator`, после `ProvideFlightIntent`:

```cpp
	/**
	 * Fills Out and returns true if this operator is gunning.
	 *
	 * A second method rather than a variant inside one intent, as spec 4 chose: the
	 * type check is kept, and a new role costs an edit to this file that everyone can
	 * see rather than a new case somewhere that nobody has to. The default is false,
	 * for the reason ProvideFlightIntent's is: an operator who is not gunning says so.
	 */
	virtual bool ProvideTurretIntent(StarForge::FTurretIntent& Out) { return false; }
```

- [ ] **Step 2: Написать исполнителя**

`Source/StarForge/Public/Ship/SFTurretComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TurretMath.h"
#include "SFTurretComponent.generated.h"

/**
 * Five states, where the maths has three. The maths only ever sees a bearing: on it,
 * slewing to it, or unable to reach it. The executor also knows two cases the maths
 * never sees, and both have to be named: a barrel standing still because nobody
 * commanded it looks exactly like one standing still because it was told to hold.
 */
UENUM()
enum class ESFTurretState : uint8
{
	/** No post commanded this turret on the last tick. */
	Uncommanded,
	/** Commanded, but the operator had nothing to point at. */
	NoTarget,
	Slewing,
	OnTarget,
	CannotBear
};

/**
 * The executor the gunner post commands: a mount with stops and a slew rate, and a
 * barrel it turns.
 *
 * It does not know who is aiming or why. It is handed a bearing in its own frame,
 * moves toward it as far as its limits and this tick's time allow, and remembers what
 * state that left it in. That separation is the same one the thruster array keeps
 * from the pilot: a player, a crewman and the ship's automation share this one path.
 *
 * It aims; it does not shoot. FTurretIntent carries the trigger and this records it,
 * so the whole path from operator to executor is exercised, and stage 4 adds a muzzle.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFTurretComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USFTurretComponent();

	virtual void BeginPlay() override;

	/** Yaw travel each way from straight ahead. Degrees here; radians below the boundary. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double MaxYawDegrees = 120.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double MinPitchDegrees = -30.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double MaxPitchDegrees = 60.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double TraverseRateDegPerSec = 90.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double OnTargetToleranceDegrees = 0.5;

	/** The visible barrel this mount turns. Optional: the maths and the readout need none. */
	void SetBarrel(USceneComponent* InBarrel);

	/** The limits in the form the maths wants: radians. */
	StarForge::Turret::FTurretLimits BuildLimits() const;

	/** Where the mount is in the world right now; the post takes the bearing from here. */
	FTransform GetMountTransform() const { return GetComponentTransform(); }

	/** One tick toward a bearing: slews within the limits, turns the barrel, records the state. */
	void ApplyBearing(const StarForge::Turret::FTurretAim& Bearing, bool bInFiring, double DeltaSeconds);

	/** Commanded with nothing to point at: the barrel holds, and the state says why. */
	void ApplyNoTarget(bool bInFiring);

	/** Nobody commanded this tick. */
	void ClearCommand();

	StarForge::Turret::FTurretAim GetAim() const { return Aim; }
	StarForge::Turret::FTurretAim GetLastBearing() const { return LastBearing; }
	bool HasBearing() const { return bHasBearing; }
	ESFTurretState GetState() const { return State; }
	bool IsFiring() const { return bFiring; }

	/** Upper case for the states a reader should notice; the calm one is lower case. */
	static const TCHAR* StateName(ESFTurretState InState);

private:
	void RotateBarrel();

	UPROPERTY()
	TObjectPtr<USceneComponent> Barrel;

	StarForge::Turret::FTurretAim Aim;
	StarForge::Turret::FTurretAim LastBearing;
	bool bHasBearing = false;
	ESFTurretState State = ESFTurretState::Uncommanded;
	bool bFiring = false;
};
```

`Source/StarForge/Private/Ship/SFTurretComponent.cpp`:

```cpp
#include "Ship/SFTurretComponent.h"

#include "StarForgeLog.h"

USFTurretComponent::USFTurretComponent()
{
	// Driven by the post that owns it, not by its own tick.
	PrimaryComponentTick.bCanEverTick = false;
}

void USFTurretComponent::BeginPlay()
{
	Super::BeginPlay();

	// Once, so a run's log says what turret it was flying, the same way the ship says
	// its physics settings. Relative to the parent, which on the rig is the hull, so
	// these are ship-local centimetres.
	const FVector Rel = GetRelativeLocation();
	UE_LOG(LogStarForge, Display,
		TEXT("Turret mount at ship-local (%.0f %.0f %.0f) cm: yaw +/-%.0f, pitch %.0f..%.0f deg, ")
		TEXT("traverse %.0f deg/s, on target within %.1f deg, %s."),
		Rel.X, Rel.Y, Rel.Z, MaxYawDegrees, MinPitchDegrees, MaxPitchDegrees,
		TraverseRateDegPerSec, OnTargetToleranceDegrees,
		Barrel ? TEXT("barrel attached") : TEXT("no barrel (numbers only)"));
}

void USFTurretComponent::SetBarrel(USceneComponent* InBarrel)
{
	Barrel = InBarrel;
	RotateBarrel();
}

StarForge::Turret::FTurretLimits USFTurretComponent::BuildLimits() const
{
	// The one place degrees become radians for this component.
	StarForge::Turret::FTurretLimits Limits;
	Limits.MaxYawRad = FMath::DegreesToRadians(MaxYawDegrees);
	Limits.MinPitchRad = FMath::DegreesToRadians(MinPitchDegrees);
	Limits.MaxPitchRad = FMath::DegreesToRadians(MaxPitchDegrees);
	Limits.TraverseRateRadPerSec = FMath::DegreesToRadians(TraverseRateDegPerSec);
	Limits.OnTargetToleranceRad = FMath::DegreesToRadians(OnTargetToleranceDegrees);
	return Limits;
}

void USFTurretComponent::ApplyBearing(
	const StarForge::Turret::FTurretAim& Bearing, bool bInFiring, double DeltaSeconds)
{
	LastBearing = Bearing;
	bHasBearing = true;
	bFiring = bInFiring;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Aim, Bearing, BuildLimits(), DeltaSeconds);
	Aim = Step.Aim;

	switch (Step.State)
	{
	case StarForge::Turret::EAimState::OnTarget:
		State = ESFTurretState::OnTarget;
		break;
	case StarForge::Turret::EAimState::CannotBear:
		State = ESFTurretState::CannotBear;
		break;
	default:
		State = ESFTurretState::Slewing;
		break;
	}

	RotateBarrel();
}

void USFTurretComponent::ApplyNoTarget(bool bInFiring)
{
	// The barrel holds where it is. Swinging it home would read as movement on a
	// turret that has nothing to move for.
	bHasBearing = false;
	bFiring = bInFiring;
	State = ESFTurretState::NoTarget;
}

void USFTurretComponent::ClearCommand()
{
	bHasBearing = false;
	bFiring = false;
	State = ESFTurretState::Uncommanded;
}

void USFTurretComponent::RotateBarrel()
{
	if (Barrel)
	{
		// The same conversion the tests round-trip: whatever this rotator does to the
		// barrel, ComputeBearing reads back as the aim, so the picture and the numbers
		// cannot disagree about which way the barrel points.
		Barrel->SetRelativeRotation(StarForge::Turret::AimToRotator(Aim));
	}
}

const TCHAR* USFTurretComponent::StateName(ESFTurretState InState)
{
	switch (InState)
	{
	case ESFTurretState::Uncommanded: return TEXT("UNCOMMANDED");
	case ESFTurretState::NoTarget: return TEXT("NO TARGET");
	case ESFTurretState::Slewing: return TEXT("slewing");
	case ESFTurretState::OnTarget: return TEXT("ON TARGET");
	case ESFTurretState::CannotBear: return TEXT("CANNOT BEAR");
	}
	return TEXT("?");
}
```

- [ ] **Step 3: Повесить крепление и ствол на корабль**

В `SFTestShip.h`: forward-declare `class USFTurretComponent;` и `class USceneComponent;`; в public:

```cpp
	USFTurretComponent* GetTurret() const { return Turret; }
```

в private:

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFTurretComponent> Turret;

	/** The pivot the turret turns; the cone below is attached to it. */
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USceneComponent> TurretBarrel;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UStaticMeshComponent> TurretBarrelMesh;
```

В `SFTestShip.cpp` включение:

```cpp
#include "Ship/SFTurretComponent.h"
```

константы в анонимное пространство имён, после `MarkerOffsetCm`:

```cpp
	/** The barrel is a one-metre cone: the engine cone is 100 cm tall, so Z scale is length in metres. */
	constexpr double BarrelLengthCm = 100.0;

	/** Cone base as a mesh scale: 0.3 x 100 cm = 30 cm across, thin enough to read as a barrel. */
	constexpr double BarrelThicknessScale = 0.3;

	/** Between the barrel's highest reach and the ceiling's inner face, in centimetres. */
	constexpr double BarrelCeilingClearanceCm = 10.0;
```

и в конструктор, после создания маркеров ориентации:

```cpp
	// The mount hangs from the ceiling inside the cabin, which is not where a turret
	// goes. It is where a person can see it: the cabin has no windows, so a barrel on
	// the roof could be checked only from the log, and the one thing a log cannot check
	// is whether the picture agrees with the numbers. The aim passes through the walls;
	// the drone outside is what the numbers describe.
	//
	// Height: the ceiling panel's inner face is at InteriorHalfExtentCm minus half its
	// thickness (400 - 10 = 390). A barrel of BarrelLengthCm pitched to its +60 stop
	// rises sin 60 = 0.866 of its length above the mount, so a mount at
	// 390 - 100 - 10 = 280 tops out at 366.6, clear by 23 cm. Derived from the same
	// constants as the panels, so moving a wall moves the turret with it.
	Turret = CreateDefaultSubobject<USFTurretComponent>(TEXT("Turret"));
	Turret->SetupAttachment(Hull);
	Turret->SetRelativeLocation(FVector(
		0.0, 0.0,
		InteriorHalfExtentCm - PanelThickness * CubeEdgeCm * 0.5 - BarrelLengthCm - BarrelCeilingClearanceCm));

	TurretBarrel = CreateDefaultSubobject<USceneComponent>(TEXT("TurretBarrel"));
	TurretBarrel->SetupAttachment(Turret);
	Turret->SetBarrel(TurretBarrel);

	// A cone, not a cylinder: it has a pointed end, so a sign error in yaw or pitch
	// shows as a barrel pointing away from where the readout says. The engine cone's
	// apex is along its own +Z; a pitch of -90 turns +Z onto +X (a +90 pitch takes +X
	// onto +Z, and this is its inverse), so the apex is the muzzle. Offset forward by
	// half its length so the base sits on the pivot.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BarrelMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	TurretBarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBarrelMesh"));
	TurretBarrelMesh->SetupAttachment(TurretBarrel);
	TurretBarrelMesh->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
	TurretBarrelMesh->SetRelativeLocation(FVector(BarrelLengthCm * 0.5, 0.0, 0.0));
	TurretBarrelMesh->SetRelativeScale3D(FVector(
		BarrelThicknessScale, BarrelThicknessScale, BarrelLengthCm / CubeEdgeCm));
	TurretBarrelMesh->SetMobility(EComponentMobility::Movable);

	// Visual only. It must not be able to block the walk test or catch a sweep.
	TurretBarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (BarrelMesh.Succeeded())
	{
		TurretBarrelMesh->SetStaticMesh(BarrelMesh.Object);
	}
```

- [ ] **Step 4: Собрать и прогнать headless**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=5 -ExecCmds="sf.ShipProfile 0" -log
```

Ожидается в журнале:

```
LogStarForge: Turret mount at ship-local (0 0 280) cm: yaw +/-120, pitch -30..60 deg, traverse 90 deg/s, on target within 0.5 deg, barrel attached.
```

`280` — из вывода выше; если число другое, разошлась либо константа, либо арифметика. `barrel attached` — что `SetBarrel` вызван до `BeginPlay` (в конструкторе), а не после.

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFStationOperator.h Source/StarForge/Public/Ship/SFTurretComponent.h Source/StarForge/Private/Ship/SFTurretComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp
git commit -F - <<'MSG'
feat(ship): ask an operator for a turret intent, and give the ship a turret to obey it

The operator interface grows a second method rather than a variant inside one
intent, which is what spec 4 chose and why: the type check is kept, and a new role
costs an edit to this file that everyone can see rather than a new case somewhere
that nobody has to. The default returns false, as the flight one does, because an
operator who is not gunning has to be able to say so.

The executor has five states where the maths has three. The maths only ever sees a
bearing; the executor also knows when nobody commanded it and when it was commanded
with nothing to point at, and both have to be named, since a barrel standing still
because it was told to hold looks exactly like one standing still because the post
is empty.

The mount hangs inside the cabin, which is not where a turret goes. It is where a
person can see it: the cabin has no windows, and the one thing a log cannot check
is whether the picture agrees with the numbers. The barrel is a cone so a sign error
has a pointed end to show it with.
MSG
```

---

### Task 4: Пост канонира и индикаторы

Второй пост на том же корабле. Он просит у оператора `FTurretIntent`, считает пеленг от крепления и отдаёт его исполнителю. Индикаторы — журнал и HUD — начинают говорить, кто занял пост канонира и в каком состоянии турель.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFShipStationComponent.h`
- Modify: `Source/StarForge/Private/Ship/SFShipStationComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFPilotStationComponent.h`
- Modify: `Source/StarForge/Private/Ship/SFPilotStationComponent.cpp`
- Create: `Source/StarForge/Public/Ship/SFGunnerStationComponent.h`
- Create: `Source/StarForge/Private/Ship/SFGunnerStationComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`
- Modify: `Source/StarForge/Public/Debug/SFFrameDriftHUD.h`
- Modify: `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`

**Interfaces:**
- Consumes: `ISFStationOperator::ProvideTurretIntent`, `USFTurretComponent` (Task 3); `StarForge::Turret::ComputeBearing` (Task 1); `USFShipStationComponent` (`Operator`, `IsOccupied`, `StationName`).
- Produces: `virtual FString USFShipStationComponent::GetOperatorDisplayName() const` (имя или `unoccupied`; пилот переопределяет); `USFGunnerStationComponent : USFShipStationComponent` с `StarForge::FTurretIntent GetLastIntent() const`, `bool IsOperatorGunning() const`, `USFTurretComponent* GetTurret() const`; `ASFTestShip::GetGunnerStation() -> USFGunnerStationComponent*`; строка журнала `Turret: ...` раз в секунду за `sf.FlightLog`; блок канонира на HUD.

**R10, вслух.** Пост пилота и пост канонира оба в `TG_PrePhysics`, и ничто их не упорядочивает. Упорядочивать нечего: пилот толкает корпус через массив двигателей, канонир поворачивает ствол; ни один не читает в своём тике то, что другой пишет. Трансформ крепления, который читает канонир, — поза корпуса до физики, кто бы из двух ни тикнул первым. Если когда-нибудь один пост начнёт читать выход другого — пост сенсоров, кормящий канонира, — это момент для `AddTickPrerequisiteComponent`, а не для комментария, утверждающего порядок.

**Канонир не смотрит на `sf.ShipProfile`.** Пилот стоит по стойке «вольно», пока стенд летит сам, потому что скриптовый профиль пишет скорость корпуса, а момент от поста спорил бы с присваиванием. Турель пишет поворот ствола; с профилем они не встречаются. Поэтому в скриптовом режиме канонир **ведёт**, и это правильно, а не пропущенная проверка.

**Кресло канонира — полтора метра за пилотским.** По той же высоте и по той же причине, что и пилотское (`SFTestShip.cpp`, комментарий у `PilotStation->SeatLocationMetres`): кресло выше стоячей высоты роняет персонажа в `MOVE_Falling`, и перенос останавливается. Назад по X, а не вбок: ось крена идёт вдоль X, и в точке (−1.5, 0, −3) человек на тех же 3 м от неё, что и в пилотском кресле. В пределах 2 м от точки появления (1.5 < 2), так что оба кресла достижимы с места старта, и решает «ближайшее свободное». Капсула радиусом 34 см занимает X от −184 до −116; внутренняя грань кормовой стены на −390; маркер-цилиндр в (−300, 0, 0) с полуразмером 25 см — на три метра выше кресла.

- [ ] **Step 1: Сделать имя оператора общим**

В `Source/StarForge/Public/Ship/SFShipStationComponent.h`, в public, после `GetOperator()`:

```cpp
	/**
	 * Who holds the post, by name, or "unoccupied". Virtual so a station can append the
	 * state of its own instrument to it, as the pilot station does with sf.ShipProfile.
	 */
	virtual FString GetOperatorDisplayName() const;
```

В `Source/StarForge/Private/Ship/SFShipStationComponent.cpp`, после `Vacate`:

```cpp
FString USFShipStationComponent::GetOperatorDisplayName() const
{
	return IsOccupied() ? Operator->GetOperatorName() : TEXT("unoccupied");
}
```

В `Source/StarForge/Public/Ship/SFPilotStationComponent.h` заменить объявление `FString GetOperatorDisplayName() const;` (вместе с его комментарием) на:

```cpp
	/**
	 * The base name, with the state of this station's own instrument appended: a
	 * station that cannot see sf.ShipProfile says so here, because the name is what
	 * both readouts print and a degraded instrument that only says so once at startup
	 * is one nobody reading the run will know about.
	 */
	virtual FString GetOperatorDisplayName() const override;
```

В `Source/StarForge/Private/Ship/SFPilotStationComponent.cpp` первую строку тела `GetOperatorDisplayName` заменить:

```cpp
	const FString Name = Super::GetOperatorDisplayName();
```

(остальное тело и комментарий — без изменений).

- [ ] **Step 2: Написать пост канонира**

`Source/StarForge/Public/Ship/SFGunnerStationComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Ship/SFShipStationComponent.h"
#include "TurretMath.h"
#include "SFGunnerStationComponent.generated.h"

class USFTurretComponent;

/**
 * The post that lays the turret.
 *
 * The second post on the ship, and the first that asks for a different kind of
 * intent. It runs the same shape of tick the pilot station does -- ask the operator,
 * turn the answer into a demand, hand it to the executor -- against a turret instead
 * of a thruster array. Spec 4's claim that the ship cannot tell who holds a post is
 * only testable once there are two posts of two types; this is the second.
 *
 * It does not know who the operator is, and it does not know how a bearing is found:
 * the maths is in StarForgeCore and the slewing is in the turret.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFGunnerStationComponent : public USFShipStationComponent
{
	GENERATED_BODY()

public:
	USFGunnerStationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** What was last asked of the turret, for the readout. Zeroed when nobody is gunning. */
	StarForge::FTurretIntent GetLastIntent() const { return LastIntent; }

	/**
	 * Whether the post actually commanded the turret on the last tick. Occupancy alone
	 * cannot say: an operator that declines leaves the same zeroed intent a gunner
	 * with no target leaves, and only one of those means "the post is working".
	 */
	bool IsOperatorGunning() const { return bOperatorGunning; }

	USFTurretComponent* GetTurret() const { return Turret; }

private:
	UPROPERTY()
	TObjectPtr<USFTurretComponent> Turret;

	StarForge::FTurretIntent LastIntent;

	/** Set only where an intent was actually provided; cleared on every other path. */
	bool bOperatorGunning = false;
};
```

`Source/StarForge/Private/Ship/SFGunnerStationComponent.cpp`:

```cpp
#include "Ship/SFGunnerStationComponent.h"

#include "GameFramework/Actor.h"
#include "Ship/SFTurretComponent.h"
#include "StarForgeLog.h"

USFGunnerStationComponent::USFGunnerStationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// The same group as the pilot station, and nothing orders the two. Nothing needs
	// to: this post turns a barrel and the pilot's post pushes the hull, and neither
	// reads in its tick anything the other writes in its own. The mount transform read
	// here is the hull's pre-physics pose whichever of them runs first. Should a post
	// ever read what another writes -- a sensor post feeding this one, say -- that is
	// the moment for AddTickPrerequisiteComponent, not for a comment claiming an order.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	StationName = TEXT("gunner");
}

void USFGunnerStationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Turret = Owner->FindComponentByClass<USFTurretComponent>();
	}

	if (!Turret)
	{
		// Loudly: a gunner station with nothing to command looks exactly like a turret
		// that will not move, and the readout cannot tell the two apart on its own.
		UE_LOG(LogStarForge, Error,
			TEXT("Gunner station found no turret on its owner; nothing will be aimed."));
	}
}

void USFGunnerStationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Turret)
	{
		LastIntent = StarForge::FTurretIntent();
		bOperatorGunning = false;
		return;
	}

	if (!IsOccupied())
	{
		LastIntent = StarForge::FTurretIntent();
		bOperatorGunning = false;
		Turret->ClearCommand();
		return;
	}

	// Not checked against sf.ShipProfile, unlike the pilot station. The scripted
	// profile writes the hull's velocity and a wrench would fight it; this post writes
	// a barrel's rotation, and the two never meet. A gunner works while the rig flies
	// itself, and that is correct rather than an omission.

	StarForge::FTurretIntent Intent;
	if (!Operator->ProvideTurretIntent(Intent))
	{
		// An operator who is not gunning says so. The turret is told nobody commanded
		// it, which is the truth, rather than being left holding last tick's state.
		LastIntent = StarForge::FTurretIntent();
		bOperatorGunning = false;
		Turret->ClearCommand();
		return;
	}

	LastIntent = Intent;
	bOperatorGunning = true;

	if (!Intent.bHasTarget)
	{
		Turret->ApplyNoTarget(Intent.bFiring);
		return;
	}

	// The demand, in the executor's own frame: the same division of labour as the
	// pilot station computing a wrench for the array to apply.
	StarForge::Turret::FTurretAim Bearing;
	if (!StarForge::Turret::ComputeBearing(Turret->GetMountTransform(), Intent.TargetWorldLocation, Bearing))
	{
		// A target on the mount itself is not a bearing. Reported as no target rather
		// than aimed at with a made-up angle.
		Turret->ApplyNoTarget(Intent.bFiring);
		return;
	}

	Turret->ApplyBearing(Bearing, Intent.bFiring, DeltaTime);
}
```

- [ ] **Step 3: Повесить пост на корабль и писать турель в журнал**

В `SFTestShip.h`: forward-declare `class USFGunnerStationComponent;`; в public:

```cpp
	USFGunnerStationComponent* GetGunnerStation() const { return GunnerStation; }
```

в private, рядом с `PilotStation`:

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFGunnerStationComponent> GunnerStation;
```

В `SFTestShip.cpp` включение:

```cpp
#include "Ship/SFGunnerStationComponent.h"
```

константа в анонимное пространство имён:

```cpp
	/** How far aft of the pilot's seat the gunner sits, in centimetres. See the constructor. */
	constexpr double GunnerSeatAftCm = 150.0;
```

и в конструктор, после `ConsoleOperator = CreateDefaultSubobject<...>`:

```cpp
	GunnerStation = CreateDefaultSubobject<USFGunnerStationComponent>(TEXT("GunnerStation"));

	// A metre and a half aft of the pilot, at exactly the pilot's height and for the
	// same reason (see PilotStation above: a seat above standing height drops him into
	// MOVE_Falling and the carry stops). Aft rather than beside so the roll axis, which
	// runs along X, stays overhead: at (-1.5, 0, -3) he is the same three metres from
	// it as in the pilot's seat. Within the 2 m interact radius of the spawn point, so
	// "nearest free seat" decides between the two and neither is out of reach from
	// where a person starts. The capsule (radius 34) spans X -184..-116; the aft wall's
	// inner face is at -390, and the cylinder marker at (-300, 0, 0) is three metres up.
	GunnerStation->SeatLocationMetres = FVector(
		StarForge::Units::CmToMetres(-GunnerSeatAftCm),
		0.0,
		StarForge::Units::CmToMetres(-InteriorHalfExtentCm + SpawnClearanceCm));
```

В `ASFTestShip::LogFlightState`, **сразу после** сброса `SecondsSinceFlightLog = 0.0;` и **до** проверки `Thrusters->IsReady()` — чтобы турель отчитывалась и на корабле без ассета:

```cpp
	// The turret reports before the thruster check: a ship whose array never built can
	// still lay a turret, and a line that vanishes with the array would look like a
	// turret that stopped.
	if (GunnerStation && GunnerStation->GetTurret())
	{
		const USFTurretComponent* Turret = GunnerStation->GetTurret();
		const StarForge::FTurretIntent TurretIntent = GunnerStation->GetLastIntent();

		FString GunnedBy = GunnerStation->GetOperatorDisplayName();
		if (GunnerStation->IsOccupied() && !GunnerStation->IsOperatorGunning())
		{
			GunnedBy += TEXT(" (standing down)");
		}

		const FString Target = TurretIntent.bHasTarget
			? FString::Printf(TEXT("(%.0f %.0f %.0f) cm"),
				TurretIntent.TargetWorldLocation.X,
				TurretIntent.TargetWorldLocation.Y,
				TurretIntent.TargetWorldLocation.Z)
			: FString(TEXT("none"));

		const StarForge::Turret::FTurretAim Bearing = Turret->GetLastBearing();
		const FString BearingText = Turret->HasBearing()
			? FString::Printf(TEXT("yaw %.1f pitch %.1f deg"),
				FMath::RadiansToDegrees(Bearing.YawRad), FMath::RadiansToDegrees(Bearing.PitchRad))
			: FString(TEXT("none"));

		const StarForge::Turret::FTurretAim Aim = Turret->GetAim();

		UE_LOG(LogStarForge, Log,
			TEXT("Turret: gunned by %s | target %s firing %d | bearing %s | aim yaw %.1f pitch %.1f deg | %s"),
			*GunnedBy, *Target, TurretIntent.bFiring ? 1 : 0, *BearingText,
			FMath::RadiansToDegrees(Aim.YawRad), FMath::RadiansToDegrees(Aim.PitchRad),
			USFTurretComponent::StateName(Turret->GetState()));
	}
	else
	{
		UE_LOG(LogStarForge, Warning, TEXT("Turret: no gunner station or no turret -- nothing is aiming."));
	}
```

Включение `Ship/SFTurretComponent.h` в `SFTestShip.cpp` уже есть с задачи 3.

- [ ] **Step 4: Блок канонира на HUD**

В `Source/StarForge/Public/Debug/SFFrameDriftHUD.h`, в private, после `DrawFlightBlock`:

```cpp
	/** Gunner lines: who holds the post, and what state the turret is in. */
	void DrawGunnerBlock(double& Y);
```

В `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp` включения:

```cpp
#include "Ship/SFGunnerStationComponent.h"
#include "Ship/SFTurretComponent.h"
#include "TurretMath.h"
```

`PanelHeight` поднять с `640.0f` до `760.0f` и дополнить его комментарий:

```cpp
	 *
	 * Raised again to 760 for the gunner block and the footing lines: the readout is
	 * thirty lines at 22 px from a 48 px top, 708 px with its two 10 px gaps, and a
	 * line falling off the bottom is the defect this panel exists to prevent.
```

В конце `DrawHUD`, после `DrawFlightBlock(Y);`:

```cpp
	DrawGunnerBlock(Y);
```

и реализация в конце файла:

```cpp
void ASFFrameDriftHUD::DrawGunnerBlock(double& Y)
{
	const ASFTestShip* Ship = Cast<ASFTestShip>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFTestShip::StaticClass()));
	const USFGunnerStationComponent* Gunner = Ship ? Ship->GetGunnerStation() : nullptr;
	const USFTurretComponent* Turret = Gunner ? Gunner->GetTurret() : nullptr;

	// Red, for the same reason the flight block says "array NOT built" in red: a block
	// that goes quiet when the thing it reports disappears is indistinguishable from
	// one reporting a turret at rest.
	if (!Gunner || !Turret)
	{
		DrawLine(TEXT("gunner  -- ship has no gunner station or no turret, nothing is aiming"), Y, FLinearColor::Red);
		return;
	}

	// Who holds the post comes first, with the same three colours the pilot line uses:
	// yellow for nobody, orange for somebody who is declining, green for somebody gunning.
	const bool bOccupied = Gunner->IsOccupied();
	const bool bGunning = Gunner->IsOperatorGunning();
	const FLinearColor GunnedByColour = !bOccupied
		? FLinearColor::Yellow
		: (bGunning ? FLinearColor::Green : FLinearColor(1.0f, 0.45f, 0.0f));

	DrawLine(FString::Printf(TEXT("gunned by %s%s"),
		*Gunner->GetOperatorDisplayName(),
		(bOccupied && !bGunning) ? TEXT(" (standing down)") : TEXT("")),
		Y, GunnedByColour);

	const StarForge::Turret::FTurretAim Aim = Turret->GetAim();
	const StarForge::Turret::FTurretAim Bearing = Turret->GetLastBearing();
	const FString BearingText = Turret->HasBearing()
		? FString::Printf(TEXT("yaw %.1f pitch %.1f"),
			FMath::RadiansToDegrees(Bearing.YawRad), FMath::RadiansToDegrees(Bearing.PitchRad))
		: FString(TEXT("none"));

	// Green only for on target. A turret at its stop is orange, not green, because its
	// angle alone would read as on target; no target and no command are yellow, the
	// colour of a post nobody is at.
	FLinearColor StateColour = FLinearColor::White;
	switch (Turret->GetState())
	{
	case ESFTurretState::OnTarget: StateColour = FLinearColor::Green; break;
	case ESFTurretState::CannotBear: StateColour = FLinearColor(1.0f, 0.45f, 0.0f); break;
	case ESFTurretState::NoTarget:
	case ESFTurretState::Uncommanded: StateColour = FLinearColor::Yellow; break;
	default: break;
	}

	DrawLine(FString::Printf(TEXT("turret  aim yaw %.1f pitch %.1f deg   bearing %s   %s   firing %s"),
		FMath::RadiansToDegrees(Aim.YawRad), FMath::RadiansToDegrees(Aim.PitchRad),
		*BearingText,
		USFTurretComponent::StateName(Turret->GetState()),
		Turret->IsFiring() ? TEXT("on") : TEXT("off")),
		Y, StateColour);
}
```

- [ ] **Step 5: Собрать и прогнать headless**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=5 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0" -log
```

Ожидается в журнале раз в секунду, рядом с каждой строкой `Flight:`:

```
LogStarForge: Turret: gunned by unoccupied | target none firing 0 | bearing none | aim yaw 0.0 pitch 0.0 deg | UNCOMMANDED
```

и **ни одной** строки `Station 'gunner' taken by` — пост есть, оператора ещё нет. Строка `Station 'pilot' taken by console.` по-прежнему есть: пилотский пост не пострадал. **Если `Turret:` нет вообще** — строка стоит после проверки массива, а не до, и на корабле без ассета турель молчала бы.

- [ ] **Step 6: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFShipStationComponent.h Source/StarForge/Private/Ship/SFShipStationComponent.cpp Source/StarForge/Public/Ship/SFPilotStationComponent.h Source/StarForge/Private/Ship/SFPilotStationComponent.cpp Source/StarForge/Public/Ship/SFGunnerStationComponent.h Source/StarForge/Private/Ship/SFGunnerStationComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp Source/StarForge/Public/Debug/SFFrameDriftHUD.h Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp
git commit -F - <<'MSG'
feat(ship): put a second post on the ship, one that asks for a different intent

Spec 4's claim that the ship cannot tell who holds a post is only testable with two
posts of two types on one hull. The gunner station runs the same shape of tick the
pilot station does -- ask the operator, turn the answer into a demand, hand it to
the executor -- against a turret instead of a thruster array, and reuses every line
of the base station without inheriting a thing about thrusters, which is what the
base was split for.

The two posts share a tick group and nothing orders them, and that is said in the
code rather than left to be discovered: neither reads in its tick anything the other
writes, so there is nothing to order. The gunner also does not stand down for the
scripted profile the way the pilot does. The profile writes the hull's velocity and
a wrench would fight it; a barrel's rotation does not meet it at all.

The turret reports in the log ahead of the thruster check, so a ship whose array
never built still says what its turret is doing rather than going quiet in a way
that reads as a turret at rest.
MSG
```

---

### Task 5: Автоматика как оператор

Третья реализация `ISFStationOperator`, не человек и не консоль. Садится в кресло канонира при старте, держит его, пока включена `sf.AutomationOperator`, и ведёт турель по мишени. Здесь тезис §4 получает свою проверку: пост, который вчера занимала консоль, а сегодня занимает автоматика, не изменился ни на строку.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFAutomationOperatorComponent.h`
- Create: `Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`
- Modify: `CLAUDE.md` (таблица переменных)

**Interfaces:**
- Consumes: `USFGunnerStationComponent` (Task 4), `ASFTargetDrone::EvaluatePathNow/IsPresent` (Task 2), `ISFStationOperator::ProvideTurretIntent` (Task 3).
- Produces: `USFAutomationOperatorComponent : UActorComponent, ISFStationOperator` с `void SetTargetDrone(ASFTargetDrone*)`, `GetOperatorName() == "automation"`; переменная `sf.AutomationOperator` (по умолчанию 1).

**Почему автоматика зависит от `ASFTargetDrone`, а не ищет цели сама.** Сенсоров в срезе нет (§4: отложены). Автоматике стенда цель **выдаёт** стенд, и она честно говорит «цели нет», когда мишень убрана. Сенсорный пост этапа 4 заменит `SetTargetDrone` на что-то своё; интерфейс оператора при этом не изменится — что и требуется.

**Удержание кресла — проще, чем у консольного оператора, и вот почему.** Консоль помнит, кто ей отказал, и повторяет запрос при смене держателя; автоматика повторяет запрос при каждом **включении** переключателя. Свободное кресло по-прежнему просится **каждый кадр**, пока не занято, — урок консоли, где ограничитель усадил пустое кресло навсегда, — а занятое кем-то другим просится один раз на включение, чтобы отказ был услышан один раз, а не шестьдесят в секунду. Двух копий машины состояний с держателем в проекте не будет.

**Хронология, которую предсказывает этот план.** Корабль стоит (`sf.ShipProfile 0`, намерение нулевое). Крепление на высоте +2.8 м над осью корабля; мишень на орбите радиуса 30 м, 10°/с, наклон 20°. Пеленг с крепления: `yaw = atan2(sin φ · cos 20°, cos φ)`, `pitch = atan2(30 · sin 20° · sin φ − 2.8, 30 · √(cos² φ + cos² 20° · sin² φ))`, φ = 10° · t.

| t, с | φ | пеленг yaw | пеленг pitch | состояние | aim yaw |
|---|---|---|---|---|---|
| 1 | 10° | 9.4° | −1.9° | ON TARGET | 9.4 |
| 5 | 50° | 48.2° | 9.9° | ON TARGET | 48.2 |
| 10 | 100° | 100.6° | 14.5° | ON TARGET | 100.6 |
| 11.85 | 118.5° | 120.0° | — | переход: `tan φ = tan 120° / cos 20°` | |
| 15 | 150° | 151.5° | 4.5° | CANNOT BEAR | 120.0 (упор) |
| 18 | 180° | ±180° → знак меняется | | ствол идёт с +120 на −120 через ноль, 240° за 2.67 с | |
| 20 | 200° | −161.1° | −12.0° | CANNOT BEAR | −60 ± 3 |
| 24.15 | 241.5° | −120.0° | — | переход обратно; ствол уже на −120, на цели сразу | |
| 25 | 250° | −111.2° | −23.6° | ON TARGET | −111.2 |

Допуски: пеленг ±0.5° (кадр между спавном мишени и строкой журнала — 0.17° орбиты); aim на цели — равен пеленгу в пределах 0.5°; строки на 12-й и 24-й секундах примыкают к переходам и могут лечь по любую сторону. Скорость обхода 90°/с против 11.5°/с максимальной угловой скорости пеленга означает, что вне упоров ствол **никогда** не отстаёт: состояние `slewing` в этом прогоне появляется только в первые кадры (подход к −5.3° начального возвышения за четыре кадра) и в две с лишним секунды перекладки через ноль.

- [ ] **Step 1: Написать автоматику**

`Source/StarForge/Public/Ship/SFAutomationOperatorComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ship/SFStationOperator.h"
#include "SFAutomationOperatorComponent.generated.h"

class ASFTargetDrone;

/**
 * An operator that is not a person: the ship's own automation, seated at the gunner
 * post of the player's own ship.
 *
 * This is where spec 4's claim gets its test. The post it holds was written for a
 * player and has been held by a console; it is now held by this, and neither the post
 * nor the ship changed a line to allow it. Spec 4's amendment of 2026-09-08 puts the
 * automation here rather than in a pirate: the claim under test is about who holds a
 * post, and a second ship would postpone the check for decoration.
 *
 * It is told what to track by the rig. Sensors are a later stage; an automation that
 * went looking for targets on its own would be a sensor post in disguise. When the
 * target is withdrawn it says it has no target, which is a different fact from not
 * being at the post, and the intent carries the difference.
 *
 * It takes the gunner station at BeginPlay and holds it while sf.AutomationOperator
 * stays on, yielding it the moment that variable goes to zero.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFAutomationOperatorComponent : public UActorComponent, public ISFStationOperator
{
	GENERATED_BODY()

public:
	USFAutomationOperatorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual bool ProvideTurretIntent(StarForge::FTurretIntent& Out) override;
	virtual FString GetOperatorName() const override { return TEXT("automation"); }

	/** What to track. Null, or a withdrawn drone, is reported as no target. */
	void SetTargetDrone(ASFTargetDrone* InDrone);

private:
	TWeakObjectPtr<ASFTargetDrone> TargetDrone;

	/**
	 * Edge detection on the switch. A held post is asked for once per turn of the
	 * switch, so the refusal is heard once; a free post is asked for every frame.
	 */
	bool bWantedSeatLastTick = false;
};
```

`Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp`:

```cpp
#include "Ship/SFAutomationOperatorComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFGunnerStationComponent.h"
#include "Ship/SFTargetDrone.h"

namespace
{
	/**
	 * Whether the automation takes the gunner seat at startup and keeps it. On by
	 * default: a mechanic behind an off-by-default switch is a mechanic nobody has.
	 * Turned off so a person can take the gunner seat instead, the way sf.ConsoleOperator
	 * frees the pilot's.
	 */
	TAutoConsoleVariable<int32> CVarAutomationOperator(TEXT("sf.AutomationOperator"), 1,
		TEXT("1: the ship's automation takes the gunner seat at startup and holds it. 0: it leaves the seat free."),
		ECVF_Default);
}

USFAutomationOperatorComponent::USFAutomationOperatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics. Being in the same group as the gunner station orders nothing
	// between the two, so BeginPlay adds the station's tick prerequisite explicitly --
	// that, not this line, is what puts this tick first, which is where a seat change
	// has to be visible in the same frame it happens rather than the next.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USFAutomationOperatorComponent::SetTargetDrone(ASFTargetDrone* InDrone)
{
	TargetDrone = InDrone;
}

void USFAutomationOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	USFGunnerStationComponent* Gunner =
		Owner ? Owner->FindComponentByClass<USFGunnerStationComponent>() : nullptr;

	if (!Gunner)
	{
		return;
	}

	// Makes the ordering the constructor's comment claims actually true, for the same
	// reason the console operator does it to the pilot station.
	Gunner->AddTickPrerequisiteComponent(this);

	bWantedSeatLastTick = CVarAutomationOperator.GetValueOnGameThread() != 0;
	if (bWantedSeatLastTick)
	{
		Gunner->TryOccupy(this);
	}
}

void USFAutomationOperatorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	USFGunnerStationComponent* Gunner =
		Owner ? Owner->FindComponentByClass<USFGunnerStationComponent>() : nullptr;
	if (!Gunner)
	{
		return;
	}

	// A runtime switch, not a startup default: -ExecCmds runs after BeginPlay, and a
	// person at the console has to be able to turn this off to take the seat.
	const bool bWantsSeat = CVarAutomationOperator.GetValueOnGameThread() != 0;
	const UObject* Holder = Gunner->GetOperator().GetObject();
	const bool bHoldsSeat = Holder == this;

	if (bWantsSeat && !bHoldsSeat)
	{
		// A free post is asked for every frame until it is held -- the console
		// operator's lesson, learned when a rate limiter sat out an empty seat for the
		// rest of a session. A post somebody else holds is asked for once per turn of
		// the switch, so the refusal in TryOccupy is heard once rather than sixty times
		// a second, and heard at all rather than pre-empted by an IsOccupied check.
		if (!Holder || !bWantedSeatLastTick)
		{
			Gunner->TryOccupy(this);
		}
	}
	else if (!bWantsSeat && bHoldsSeat)
	{
		Gunner->Vacate();
	}

	bWantedSeatLastTick = bWantsSeat;
}

bool USFAutomationOperatorComponent::ProvideTurretIntent(StarForge::FTurretIntent& Out)
{
	// Gunning whenever asked: this operator has no "not at the post" state of its own.
	// What it may lack is a target, and that is said in the intent rather than by
	// declining, because declining reads as an empty seat and an empty seat is a
	// different fact.
	const ASFTargetDrone* Drone = TargetDrone.Get();
	Out.bHasTarget = Drone != nullptr && Drone->IsPresent();
	Out.TargetWorldLocation = Out.bHasTarget ? Drone->EvaluatePathNow() : FVector::ZeroVector;

	// Trigger held whenever there is something to point at. Whether the barrel is on
	// it is the turret's report, not this operator's decision; stage 4 gives the gunner
	// the judgement to wait for ON TARGET, and a muzzle to make it matter.
	Out.bFiring = Out.bHasTarget;

	return true;
}
```

- [ ] **Step 2: Посадить её на корабль и выдать ей мишень**

В `SFTestShip.h`: forward-declare `class USFAutomationOperatorComponent;`; в private, рядом с `ConsoleOperator`:

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFAutomationOperatorComponent> AutomationOperator;
```

В `SFTestShip.cpp` включение:

```cpp
#include "Ship/SFAutomationOperatorComponent.h"
```

в конструктор, после создания `GunnerStation`:

```cpp
	AutomationOperator = CreateDefaultSubobject<USFAutomationOperatorComponent>(TEXT("AutomationOperator"));
```

и в `BeginPlay`, внутри ветки `if (TargetDrone)`, после `FinishSpawning`:

```cpp
		// Components have begun play inside Super::BeginPlay, so the automation is
		// already in its seat by now; the first station tick is still ahead, and a null
		// drone until then is reported as no target rather than aimed at.
		if (AutomationOperator)
		{
			AutomationOperator->SetTargetDrone(TargetDrone);
		}
```

- [ ] **Step 3: Собрать и прогнать четыре раза**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

**Прогон A — хронология наведения (26 с):**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=26 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0" -log
```

Ожидается: `Station 'gunner' taken by automation.` рядом со `Station 'pilot' taken by console.` — два поста, два оператора, ни один не человек. Затем строки `Turret:` по таблице выше, например на пятой секунде:

```
LogStarForge: Turret: gunned by automation | target (2890 2160 2226) cm firing 1 | bearing yaw 48.2 pitch 9.9 deg | aim yaw 48.2 pitch 9.9 deg | ON TARGET
```

(позиция цели: `30 m · (cos 50°, sin 50° · cos 20°, sin 50° · sin 20°)` = `(19.28, 21.60, 7.86)` м от оси корабля, стоящего в начале координат, — в сантиметрах `(1928 2160 786)`; в примере выше корабль сместился на старте: сверять **пеленг**, не позицию). На 15-й: `bearing yaw 151.5 ... aim yaw 120.0 ... CANNOT BEAR`. На 25-й: `bearing yaw -111.2 ... aim yaw -111.2 ... ON TARGET`. Проверить все шесть строк таблицы. Если `ON TARGET` не наступает никогда — читать `aim`: стоящий на нуле ствол означает, что `ApplyBearing` не вызывается; ствол, отстающий на постоянный угол, — что пеленг берётся не с крепления.

**Прогон B — мишень убрана (5 с):**

```bash
... -seconds=5 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.TargetDrone 0" -log
```

Ожидается: `Target drone WITHDRAWN (sf.TargetDrone 0).` и затем каждую секунду:

```
LogStarForge: Turret: gunned by automation | target none firing 0 | bearing none | aim yaw 0.0 pitch -N.N deg | NO TARGET
```

где `-N.N` — то, куда ствол успел уйти за кадр-два до того, как переменная подействовала (от 0.0 до −5.3), и это число **не меняется** от строки к строке: ствол держит, а не возвращается домой.

**Прогон C — автоматика выключена (5 с):**

```bash
... -seconds=5 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.AutomationOperator 0" -log
```

Ожидается `Station 'gunner' taken by automation.` при старте (BeginPlay раньше `-ExecCmds`), затем `Station 'gunner' left by automation.` и строки `gunned by unoccupied | ... | UNCOMMANDED`.

**Прогон D — скриптовый профиль (10 с):**

```bash
... -seconds=10 -ExecCmds="sf.FlightLog 1" -log
```

Ожидается `Flight: flown by console (standing down)` — пилот стоит вольно — и одновременно `Turret: gunned by automation | ... | slewing` либо `CANNOT BEAR`, **без** `(standing down)`: канонир работает под профилем. `ON TARGET` здесь редок или отсутствует: крен 180°/с вдвое быстрее обхода. Это ожидаемо и записывается как факт, не как дефект.

- [ ] **Step 4: Записать переменные в `CLAUDE.md`**

В таблицу переменных `CLAUDE.md`, после строки `sf.TargetDrone`, добавить две строки. Вторая закрывает пропуск: `sf.ConsoleOperator` существует с плана «б», но в таблице её нет.

```markdown
| `sf.AutomationOperator` | 1 | 1 the ship's automation takes the gunner seat at startup and holds it; 0 leaves the seat free for a person |
| `sf.ConsoleOperator` | 1 | 1 the console operator takes the pilot seat at startup and holds it; 0 leaves the seat free for a person |
```

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFAutomationOperatorComponent.h Source/StarForge/Private/Ship/SFAutomationOperatorComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp CLAUDE.md
git commit -F - <<'MSG'
feat(ship): seat the ship's own automation at the gunner post

This is the test spec 4 has been waiting for. The gunner post was written the way
the pilot post was, has the same base, and is now held by something that is not a
person and not a console; neither the post nor the ship changed a line to allow it.
The automation sits in the player's own ship rather than a pirate's, as the spec's
amendment says: the claim is about who holds a post, and a second ship would have
postponed the check for decoration.

It is told what to track rather than looking, because sensors are a later stage and
an automation that found its own targets would be a sensor post in disguise. When
the target is withdrawn it says it has no target, which is a different fact from
not being at the post, and the intent carries the difference.

Seat-keeping is simpler than the console operator's, on purpose: a free post is
still asked for every frame, which is the lesson that cost a session, but a held one
is asked for once per turn of the switch rather than once per change of holder. Two
copies of the holder-tracking machine would be two places for it to be wrong.
MSG
```

---

### Task 6: Состояние опоры и остаток ускорения

Две чистые вещи для сбивания с ног, в `StarForgeCore` под тестами: **гистерезис** (упал сразу, встал через полсекунды под пределом) и **несбалансированный остаток** ускорения, который действует на упавшего. Обе — арифметика, чья ошибка проявляется как ощущение: человек, которого ставит и роняет по очереди на каждом кадре, или человек, которого тянет к оси вращения вместо стены.

**Files:**
- Modify: `Source/StarForgeCore/Public/ShipFrameMath.h`
- Modify: `Source/StarForgeCore/Private/ShipFrameMath.cpp`
- Modify: `Source/StarForgeCore/Private/Tests/ShipFrameMathTest.cpp`

**Interfaces:**
- Consumes: `GravPlateLimitMetersPerSecSq`, `ExceedsGravPlateLimit` (существуют с этапа 1).
- Produces: `StarForge::ShipFrame::ComputeUncompensatedAcceleration(const FVector& FeltAccelMetersPerSecSq) -> FVector`; `StarForge::ShipFrame::FFootingState { bool bDown; double SecondsUnderLimit; bool Update(bool bOverLimit, double DeltaSeconds, double RecoverySeconds); }`.

**Знак остатка.** `ComputeFeltAcceleration` возвращает ускорение **палубы** под человеком: при вращении — центростремительное, к оси. Человек испытывает противоположное — его несёт **от** оси, к стене. Плиты гасят до предела; остаток = `−(Felt − clamp(Felt, limit))`. Тест берёт худший случай спеки: крен 180°/с и человек в 4 м от оси, `ω²r = π² · 4 = 39.48 м/с²` к оси; остаток `39.48 − 19.62 = 19.86` **от** оси.

**Граница тестов гистерезиса не на полсекунды ровно.** Тридцать шагов по 1/60 в `double` могут дать 0.49999999999999994, и тест на ровно тридцатом шаге стал бы лотереей. Проверяется 29 шагов (0.483 с — ещё лежит) и 31 (0.517 с — встал).

- [ ] **Step 1: Написать падающие тесты**

Дописать в конец `Source/StarForgeCore/Private/Tests/ShipFrameMathTest.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameUncompensatedBelowLimitIsZeroTest,
	"StarForge.Core.ShipFrame.UncompensatedBelowLimitIsZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameUncompensatedBelowLimitIsZeroTest::RunTest(const FString& Parameters)
{
	// Inside the plates' ceiling nothing is left over: a man under 15 m/s^2 of felt
	// acceleration feels the deck, not a shove.
	TestTrue(TEXT("below the limit nothing is left for him to feel"),
		StarForge::ShipFrame::ComputeUncompensatedAcceleration(FVector(0.0, 0.0, 15.0)).IsNearlyZero(1e-12));

	// And exactly at the limit is still nothing: the plates absorb up to and including it.
	TestTrue(TEXT("at the limit still nothing"),
		StarForge::ShipFrame::ComputeUncompensatedAcceleration(
			FVector(StarForge::ShipFrame::GravPlateLimitMetersPerSecSq, 0.0, 0.0)).IsNearlyZero(1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameUncompensatedIsTheExcessReversedTest,
	"StarForge.Core.ShipFrame.UncompensatedIsTheExcessReversed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameUncompensatedIsTheExcessReversedTest::RunTest(const FString& Parameters)
{
	// The spec's own worst case: a full roll with a man four metres from the axis.
	// omega^2 r = pi^2 * 4 = 39.48 m/s^2 of centripetal acceleration, pointing from him
	// toward the axis, which is -X here. The plates absorb 19.62 of it. What is left,
	// 19.86, is what he feels, and he feels it the other way: thrown toward the wall,
	// not pulled toward the axis. A sign error here slides him the wrong way across the
	// deck and nothing crashes.
	const double Centripetal = 4.0 * UE_DOUBLE_PI * UE_DOUBLE_PI;
	const FVector Push = StarForge::ShipFrame::ComputeUncompensatedAcceleration(FVector(-Centripetal, 0.0, 0.0));

	TestTrue(TEXT("the push is the excess over the limit"),
		FMath::IsNearlyEqual(Push.X, Centripetal - StarForge::ShipFrame::GravPlateLimitMetersPerSecSq, 1e-9));
	TestTrue(TEXT("and it points away from the axis"), Push.X > 0.0);
	TestTrue(TEXT("with nothing on the other axes"),
		FMath::IsNearlyZero(Push.Y, 1e-12) && FMath::IsNearlyZero(Push.Z, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameFootingDropsAtOnceTest,
	"StarForge.Core.ShipFrame.FootingDropsAtOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameFootingDropsAtOnceTest::RunTest(const FString& Parameters)
{
	StarForge::ShipFrame::FFootingState Footing;

	// Standing, and nothing over the limit: standing.
	TestFalse(TEXT("starts on his feet"), Footing.Update(false, 1.0 / 60.0, 0.5));

	// One frame over the limit is enough. There is no delay on the way down; the
	// delay is only on the way back up.
	TestTrue(TEXT("one frame over the limit and he is down"), Footing.Update(true, 1.0 / 60.0, 0.5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameFootingRecoversAfterTheDelayTest,
	"StarForge.Core.ShipFrame.FootingRecoversAfterTheDelay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameFootingRecoversAfterTheDelayTest::RunTest(const FString& Parameters)
{
	StarForge::ShipFrame::FFootingState Footing;
	Footing.Update(true, 1.0 / 60.0, 0.5);

	// 29 frames under the limit is 0.483 s: not yet.
	for (int32 i = 0; i < 29; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	TestTrue(TEXT("still down short of the delay"), Footing.bDown);

	// 31 frames is 0.517 s: up. Not tested at exactly 30, because thirty sixtieths
	// summed in double may land a hair under a half and the test would be a coin toss.
	Footing.Update(false, 1.0 / 60.0, 0.5);
	Footing.Update(false, 1.0 / 60.0, 0.5);
	TestFalse(TEXT("up once the delay has passed"), Footing.bDown);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameFootingRelapseRestartsTheDelayTest,
	"StarForge.Core.ShipFrame.FootingRelapseRestartsTheDelay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameFootingRelapseRestartsTheDelayTest::RunTest(const FString& Parameters)
{
	StarForge::ShipFrame::FFootingState Footing;
	Footing.Update(true, 1.0 / 60.0, 0.5);

	// Twenty frames of calm, one frame over, twenty more of calm. Forty frames of calm
	// in total would have stood him up; the relapse in the middle has to have reset
	// the clock, or a manoeuvre flickering at the limit stands him up mid-flicker.
	for (int32 i = 0; i < 20; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	Footing.Update(true, 1.0 / 60.0, 0.5);
	for (int32 i = 0; i < 20; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	TestTrue(TEXT("still down: the relapse restarted the delay"), Footing.bDown);

	// Eleven more make 31 since the relapse: up.
	for (int32 i = 0; i < 11; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	TestFalse(TEXT("up once the delay has passed since the relapse"), Footing.bDown);

	return true;
}
```

- [ ] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается ошибка `'ComputeUncompensatedAcceleration': is not a member of 'StarForge::ShipFrame'` (и следом об `FFootingState`).

- [ ] **Step 3: Реализовать**

В `Source/StarForgeCore/Public/ShipFrameMath.h`, внутри `namespace StarForge::ShipFrame`, после `ExceedsGravPlateLimit`:

```cpp
	/**
	 * What the occupant is left to feel once the plates have absorbed what they can.
	 *
	 * Zero up to and including the limit. Above it, the excess -- reversed, because
	 * FeltAccel is the acceleration of the deck under him and what he experiences is
	 * its opposite: a deck accelerating toward the spin axis is a man being thrown
	 * toward the wall. m/s^2 in and out, in the ship's frame.
	 */
	STARFORGECORE_API FVector ComputeUncompensatedAcceleration(const FVector& FeltAccelMetersPerSecSq);

	/**
	 * Whether an occupant is on his feet, with the delay that keeps a manoeuvre sitting
	 * at the limit from standing him up and knocking him down on alternate frames.
	 *
	 * Footing is lost the instant the limit is exceeded and regained only after
	 * RecoverySeconds continuously under it; any relapse restarts that wait.
	 */
	struct STARFORGECORE_API FFootingState
	{
		bool bDown = false;
		double SecondsUnderLimit = 0.0;

		/** Advances one step and returns whether he is down after it. */
		bool Update(bool bOverLimit, double DeltaSeconds, double RecoverySeconds);
	};
```

В `Source/StarForgeCore/Private/ShipFrameMath.cpp`, в первом `namespace StarForge::ShipFrame`, после `ExceedsGravPlateLimit`:

```cpp
	FVector ComputeUncompensatedAcceleration(const FVector& FeltAccelMetersPerSecSq)
	{
		const FVector Absorbed = FeltAccelMetersPerSecSq.GetClampedToMaxSize(GravPlateLimitMetersPerSecSq);
		return -(FeltAccelMetersPerSecSq - Absorbed);
	}

	bool FFootingState::Update(bool bOverLimit, double DeltaSeconds, double RecoverySeconds)
	{
		if (bOverLimit)
		{
			bDown = true;
			SecondsUnderLimit = 0.0;
			return bDown;
		}

		if (bDown)
		{
			SecondsUnderLimit += FMath::Max(0.0, DeltaSeconds);
			if (SecondsUnderLimit >= RecoverySeconds)
			{
				bDown = false;
				SecondsUnderLimit = 0.0;
			}
		}

		return bDown;
	}
```

- [ ] **Step 4: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

```bash
grep -c "Test Completed. Result=Passed" Saved/Logs/StarForge.log
grep -c "Test Completed. Result=Failed" Saved/Logs/StarForge.log
```

Ожидается **52** и `0`: 47 после задачи 1 плюс пять.

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForgeCore/Public/ShipFrameMath.h Source/StarForgeCore/Private/ShipFrameMath.cpp Source/StarForgeCore/Private/Tests/ShipFrameMathTest.cpp
git commit -F - <<'MSG'
feat(core): decide when a man loses his footing, and what then pushes him

Two pieces of arithmetic for the knockdown, in StarForgeCore under tests because
their failure modes are feelings rather than crashes: a manoeuvre sitting at the
limit standing him up and knocking him down on alternate frames, or a slide that
pulls him toward the spin axis instead of throwing him at the wall.

Footing is lost the instant the limit is exceeded and regained only after half a
second continuously under it, and a relapse restarts the wait. The push is what the
plates could not absorb, reversed: the felt figure is the deck's acceleration, and
what he experiences is its opposite. The test takes the spec's own worst case, a
full roll four metres from the axis, and checks the sign as well as the size.
MSG
```

---

### Task 7: Ускорение из компонента системы отсчёта

Сбиванию с ног нужно ощущаемое ускорение, а его линейная часть сегодня считается в `ASFTestShip` из скорости Chaos. План «б» зафиксировал требование (его «Что дальше», п. 2): компонент движения не должен зависеть от класса тестового корабля, чтобы узнать, как разгоняется палуба. Компонент системы отсчёта уже выводит угловую скорость из разности трансформов; линейную скорость и ускорение он выводит тем же способом и становится **единственным прибором** для этой величины — на HUD, в журнале и в задаче 8.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFShipFrameComponent.h`
- Modify: `Source/StarForge/Private/Ship/SFShipFrameComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`
- Modify: `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`

**Interfaces:**
- Consumes: `StarForge::Units::CmToMetres`; `USFShipFrameComponent::GetAngularVelocityRadPerSec` (существует).
- Produces: `FVector USFShipFrameComponent::GetLinearVelocityMetersPerSec() const` (мировые оси, м/с); `FVector USFShipFrameComponent::GetLinearAccelMetersPerSecSq() const` (мировые оси, м/с²); строка `Flight:` получает поле `accel %.1f m/s2` сразу после `speed`. **Удаляются** `ASFTestShip::GetLinearAccelMetersPerSecSq()` и поле `LinearAccelMetersPerSecSq`; `GetSpeedMetersPerSec()` остаётся.

**Почему из разности положений, а не из скорости Chaos.** Угловая скорость уже берётся из разности двух семплов трансформа; линейная — из тех же двух семплов, так что все три слагаемых ощущаемого ускорения — одного кадра и одного прибора. Разность работает для любого владельца, не только для симулируемого тела: станция этапа 3 движется кинематически. При фиксированном шаге Chaos интегрирует `v += a·dt`, затем `x += v·dt`, поэтому `(xₙ − xₙ₋₁)/dt` есть `vₙ` **точно**, а разность двух таких — `aₙ` точно, не приближённо. Цена: компонент тикает в `TG_PostPhysics`, а строку `Flight:` корабль пишет в `TG_PrePhysics`, так что в ней стоит ускорение предыдущего шага. При постоянной тяге это то же число; под скриптовым профилем — сдвиг фазы на кадр, учтённый в допусках ниже.

**Арифметика, которую проверяет прогон.** `DA_TestShip` (таблица задачи 4 плана `2026-09-07-ship-on-chaos.md`): четыре двигателя вдоль +X по `MaxThrustNewtons` 200 000, `MassKg` 50 000 (`DefaultHullMassKg` в `SFTestShip.cpp` — то же число для стенда без ассета). `a = F/m = 800 000 / 50 000 = 16.0 м/с²`. С ассистом спрос `(200 − v)·m·LinearGain` при `LinearGain` 2.0 (`FFlightLimits`, `FlightController.h`; `ComputeDemandedWrench` в `FlightController.cpp`) равен `(200 − v)·100 000` Н и превышает 800 кН, пока `200 − v > 8`, то есть до 192 м/с — двенадцатая секунда. До неё ускорение ровно 16.0; после — экспонента с постоянной времени `1/LinearGain` = 0.5 с. Раскрутка `ResponseTimeSeconds` 0.05 с заканчивается в первой четверти секунды, раньше первой строки журнала.

- [ ] **Step 1: Объявить скорость и ускорение у компонента**

В `Source/StarForge/Public/Ship/SFShipFrameComponent.h`, в public, после `GetAngularVelocityRadPerSec`:

```cpp
	/** Linear velocity of this frame in metres per second, world space. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetLinearVelocityMetersPerSec() const { return LinearVelocityMetersPerSec; }

	/**
	 * Linear acceleration of this frame in metres per second squared, world space.
	 *
	 * The felt-acceleration figure of spec 6 is this plus the centripetal term from
	 * the angular velocity above, and the knockdown is decided on it. It is derived
	 * here, from the same transform sample as the angular velocity, so that the figure
	 * comes from one instrument. The ship used to compute it from its Chaos velocity,
	 * which meant the movement component would have had to know the test ship's class
	 * to read it.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetLinearAccelMetersPerSecSq() const { return LinearAccelMetersPerSecSq; }
```

в private, после `bHasSample`:

```cpp
	FVector LinearVelocityMetersPerSec = FVector::ZeroVector;
	FVector LinearAccelMetersPerSecSq = FVector::ZeroVector;

	/** False until there is a velocity to difference; the first two ticks report zero acceleration. */
	bool bHasVelocitySample = false;
```

- [ ] **Step 2: Считать их**

В `Source/StarForge/Private/Ship/SFShipFrameComponent.cpp` включение:

```cpp
#include "Units.h"
```

и в `TickComponent`, внутри `if (bHasSample && DeltaTime > UE_SMALL_NUMBER)`, после строки `AngularVelocityRadPerSec = Axis * (Angle / DeltaTime);`:

```cpp
		// Metres from here on: the one crossing of the unit boundary for this
		// instrument. A second difference of position rather than a read of the body's
		// velocity: Chaos at a fixed step integrates v += a dt and then x += v dt, so
		// (x_n - x_{n-1}) / dt is v_n exactly and the difference of two of those is a_n
		// exactly. It also works for an owner that is not a physics body, which the
		// docking station of stage 3 will not be.
		const FVector DeltaCm = Current.GetLocation() - LastSampledTransform.GetLocation();
		const FVector VelocityMetersPerSec(
			StarForge::Units::CmToMetres(DeltaCm.X) / DeltaTime,
			StarForge::Units::CmToMetres(DeltaCm.Y) / DeltaTime,
			StarForge::Units::CmToMetres(DeltaCm.Z) / DeltaTime);

		if (bHasVelocitySample)
		{
			LinearAccelMetersPerSecSq = (VelocityMetersPerSec - LinearVelocityMetersPerSec) / DeltaTime;
		}

		LinearVelocityMetersPerSec = VelocityMetersPerSec;
		bHasVelocitySample = true;
```

- [ ] **Step 3: Убрать второй прибор с корабля и напечатать ускорение в журнал**

В `SFTestShip.h` удалить две строки

```cpp
	/** This tick's linear acceleration in m/s^2, for the grav-plate readout. */
	FVector GetLinearAccelMetersPerSecSq() const { return LinearAccelMetersPerSecSq; }
```

и поле `FVector LinearAccelMetersPerSecSq = FVector::ZeroVector;`. `PreviousVelocityCms` и `GetSpeedMetersPerSec()` остаются.

В `SFTestShip.cpp`, в `Tick`, заменить шесть строк от комментария `// Measured from what the body actually did` до `PreviousVelocityCms = VelocityCms;` включительно на:

```cpp
	// Only the speed is kept here, for the readout's profile-phase line. The
	// acceleration moved to the frame component: the felt-acceleration figure and the
	// knockdown are read from one instrument, and that instrument is not the test
	// ship's class -- the movement component must not have to know this class exists
	// to find out how hard the deck is accelerating.
	PreviousVelocityCms = Hull->GetPhysicsLinearVelocity();
```

В `LogFlightState` формат `TEXT("speed %.1f m/s, ang roll/pitch/yaw (%.1f %.1f %.1f) deg/s | ")` заменить на

```cpp
		TEXT("speed %.1f m/s, accel %.1f m/s2, ang roll/pitch/yaw (%.1f %.1f %.1f) deg/s | ")
```

и в списке аргументов сразу после `State.LinearVelocity.Size(),` вставить:

```cpp
		Frame->GetLinearAccelMetersPerSecSq().Size(),
```

`Frame` — default subobject, создаётся в конструкторе и нулём быть не может; проверка на null здесь напечатала бы «0.0» вместо отсутствующего прибора, а это ровно тот ноль, который проект запрещает.

- [ ] **Step 4: HUD читает компонент**

В `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`, в `DrawHUD`, заменить

```cpp
		const FVector LocalAccel = FrameTransform.InverseTransformVectorNoScale(
			Ship->GetLinearAccelMetersPerSecSq());
```

на

```cpp
		const FVector LocalAccel = FrameTransform.InverseTransformVectorNoScale(
			Frame->GetLinearAccelMetersPerSecSq());
```

Остальное в этом блоке задача 8 заменит целиком; здесь трогается одна строка, без которой не собирается.

- [ ] **Step 5: Собрать и прогнать дважды**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

**Прогон A — тяга (16 с):**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=16 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log
```

Ожидается в строках `Flight:` с первой по одиннадцатую — `accel 16.0 m/s2` при `force got (800000 0 0)`, `speed` растёт на 16.0 от строки к строке (пятая — около 79). С двенадцатой строки `accel` падает (192 м/с достигнуты), пятнадцатая — `speed 200.0`, `accel 0.0`. `16.0` — из вывода выше; `15.9` или `16.1` означает, что либо масса, либо тяга, либо `dt` не те, что в ассете и в `-fps=60`.

**Прогон B — скриптовый профиль (9 с):**

```bash
... -seconds=9 -ExecCmds="sf.FlightLog 1" -log
```

Профиль пишет скорость `400·sin(2πt/8)` м/с, откуда ускорение `400·2π/8 = 314.16·cos(πt/4)` м/с². Строка `k` выходит на `t ≈ k` с и печатает семпл на полтора кадра раньше (`−1.5/60` с, сдвиг фазы 0.02 рад). Ожидается `accel`: строки 4 и 8 — **312–315**; строки 2 и 6 — **меньше 12**; строки 1, 3, 5, 7 — **214–229**. Это проверка всего пути `см → м → /dt → /dt` независимо от тяги: ошибка в сто раз или в `dt` не пролезет ни в одну из трёх полос.

- [ ] **Step 6: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFShipFrameComponent.h Source/StarForge/Private/Ship/SFShipFrameComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp
git commit -F - <<'MSG'
feat(ship): read the deck's acceleration from the frame component, not the ship

The knockdown needs the felt acceleration, and its linear part was computed by the
test ship from its Chaos velocity. Reading it from there would have made the movement
component depend on the test ship's class to learn how hard the deck accelerates,
which the previous plan ruled out; and the HUD was computing the same quantity a
second way. The frame component already derives angular velocity from the difference
of two transform samples. Linear velocity and acceleration now come from the same two
samples, so the three terms of the felt figure are one frame's and one instrument's.

A second difference of position rather than a read of the body's velocity, because at
a fixed step Chaos integrates velocity then position, so the difference is the step's
acceleration exactly -- and because a docking station in the next stage is not a
physics body. The flight log prints the figure so the arithmetic can be checked
against the asset: 800 kN on 50 t is 16.0, and it reads 16.0.
MSG
```

---

### Task 8: Сбивание с ног

Механика §6 в том виде, в каком её зафиксировало решение «Что именно происходит»: человек теряет опору — режим падения, ходьба запрещена, — а **перенос и выравнивание гравитации продолжают работать**; его толкает то, что плиты не погасили; манёвр ослабевает — он встаёт. Идёт последним (R5), не прекращает перенос (R6), переопределяет смысл прибора увода и чинит это тем же движением (R7), включена по умолчанию (R8).

**Files:**
- Modify: `Source/StarForge/Public/Player/SFShipboardMovementComponent.h`
- Modify: `Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp`
- Modify: `Source/StarForge/Private/Player/SFCharacter.cpp`
- Modify: `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: `USFShipFrameComponent::GetLinearAccelMetersPerSecSq`, `GetAngularVelocityRadPerSec`, `GetFrameTransform` (Task 7); `StarForge::ShipFrame::ComputeFeltAcceleration`, `ExceedsGravPlateLimit`, `GravPlateLimitMetersPerSecSq`, `ComputeUncompensatedAcceleration`, `FFootingState` (Task 6); `StarForge::Units::CmToMetres`, `MetresToCm`; движок: `bStayBasedInAir`, `FallingLateralFriction`, `SetMovementMode`, `IsFalling`, `IsValidLandingSpot`, `ProcessLanded`, `StartNewPhysics`, `Velocity`.
- Produces: на `USFShipboardMovementComponent` — `bool IsKnockedDown() const`, `bool IsKnockdownEnabled() const`, `void SetStrappedIn(bool)`, `bool IsStrappedIn() const`, `double GetKnockedDownSeconds() const`, `double GetBestCompletedKnockedDownSeconds() const`, `FVector GetFeltAccelerationMetersPerSecSq() const`, `FVector GetKnockdownPushMetersPerSecSq() const`, `FString DescribeFooting() const`; переменная `sf.Knockdown` (по умолчанию 1); строки журнала `Footing LOST …`, `Footing regained …`, `Carry NOT RUNNING …`, `No seat while knocked down.`; к обеим строкам `Shipboard drift …` добавляется `, knocked down %.1f s -- %s`; строка `footing …` на HUD.

**Как это устроено, по пунктам.** Каждый опирается на строку таблицы «Что проверено в исходниках движка».

1. **Где решается.** В `UpdateBasedMovement`, сразу после переноса и до замера, раз в кадр под тем же сторожем, что и часы прибора: движок зовёт эту функцию дважды за кадр, и толчок, добавленный на обоих вызовах, был бы вдвое больше посчитанного — удвоенные часы, только в скорости. Величины — из компонента системы отсчёта (задача 7), в осях корабля, в СИ; семпл на кадр старее, чем перенос, — против полусекундного гистерезиса это ничто.
2. **Как теряется опора.** `bStayBasedInAir = true`, затем `SetMovementMode(MOVE_Falling)` — именно в этом порядке: `OnMovementModeChanged` читает флаг на входе в падение и с ним пропускает и передачу скорости опоры, и очистку опоры (cpp 1459–1472). Опора — то, что `MaybeUpdateBasedMovement` проверяет перед вызовом `UpdateBasedMovement`; без неё перенос не зовётся. Восстанавливать опору руками не нужно и нельзя (R6).
3. **Как не встаёт раньше времени.** `IsValidLandingSpot` возвращает `false`, пока он лежит: `PhysFalling` спрашивает её на каждом блокирующем ударе и сажает по первому «да» (5050) — в том же кадре, под тем же ускорением. С «нет» удар о палубу идёт в `HandleImpact` → `ComputeSlideVector`, и скорость пересчитывается из скольжения (5078, 5122, 5135): палуба — то, по чему скользят, а не то, на чём стоят. Одного этого мало: зажатого между стенами персонажа `PhysFalling` сажает, **не спрашивая** (5221–5225), а посадка — это `SetMovementMode(MOVE_Walking)` с поиском пола и опорой из него (1450–1452): у прижатого к потолочному углу пола под ногами нет, и он бы сошёл с корабля. Поэтому переопределяется и `ProcessLanded`: лёжа он не садится ни по какому пути.
4. **Что толкает и с какой силой.** `ComputeUncompensatedAcceleration(Felt)` в осях корабля → в мировые → в см/с² → в `Velocity` как приращение за кадр. Напрямую в `Velocity`, а не через `AddForce`: `PerformMovement` применяет накопленные силы после этого вызова и до `StartNewPhysics` (2840, 2871, 2883), так что оба пути попадают в `PhysFalling` того же кадра, но `AddForce` ещё делит на массу персонажа. Толчок ограничивает трение: `PhysFalling` подаёт `FallingLateralFriction` в `CalcVelocity` для боковой части скорости, `ApplyVelocityBraking` умножает его на `BrakingFrictionFactor` 2.0 (4384–4385, 734). Значение **2.0** даёт действующие 4/с и установившуюся скорость в четверть избытка: худший случай спеки, 19.86 м/с² в 4 м от оси, — 5 м/с.
5. **Как встаёт.** `FFootingState` полсекунды под пределом → `bStayBasedInAir = false`, толчок снят, `IsValidLandingSpot` и `ProcessLanded` снова движковые; он падает на палубу и садится штатно — `SetMovementMode(MOVE_Walking)`, пол, опора из пола. Флаг снимается сразу: единственный переход, который его читает, — ходьба → падение, а до посадки такого перехода нет. Включённый навсегда, он спрятал бы отрицательный контроль.
6. **Что теперь значит «увод».** Перенос продолжается, замер продолжается, а скольжение лежащего — уводом в старом смысле не является. К обеим строкам `Shipboard drift` добавляется время «сбит с ног» за интервал и **слово**: `CARRY FAILED` при пропущенных переносах, `KNOCKED DOWN` при времени лёжа, `knockdown off (sf.Knockdown 0)` при выключенной механике, `footing intact` иначе. То же слово — на HUD. Пропущенные переносы старше всего: это дефект, ради которого прибор существует.
7. **Почему журнал теперь умеет сказать «перенос не работает».** `DetectCarryGap` считает пропуски изнутри переноса, и потерю опоры, которая не кончается, из журнала было не видно — только с HUD. `TickComponent` компонента движения зовётся всегда; после `Super` он раз в секунду пишет `Carry NOT RUNNING`, пока переносов нет. Это строка отрицательного контроля.
8. **Пристёгнутый и лежащий.** Сидящий пристёгнут (`SetStrappedIn(true)` при посадке) и не теряет опору, чем бы ни было ощущаемое ускорение; прогон C точки проверки 4 — сидя через весь профиль — на этом держится. Лежащий не ходит (`PollMovementInput` возвращается) и не садится (`ToggleSeat` отказывает со строкой). Обзор лёжа работает: голова не отказывает.
9. **Почему отрицательный контроль отныне с `sf.Knockdown 0`.** При `sf.ShipGravity 0` под профилем ощущаемое ускорение с первых кадров за пределом (314 м/с² линейного), и с включённой механикой человек уронен раньше, чем палуба стала непроходимой; `bStayBasedInAir` держит опору, перенос продолжается, `Carry NOT RUNNING` не появляется. Контроль, показывающий ничего, — не контроль. Прогон D ниже — с выключенной механикой, и это записано в `CLAUDE.md`.
10. **Два сантиметра, которые прогон B покажет как увод.** Ходящая капсула висит над полом между `MIN_FLOOR_DIST` 1.9 и `MAX_FLOOR_DIST` 2.4 см (cpp 98–99); падающая лежит на контакте. Уроненный на месте опускается на эти два сантиметра, и `drift now` показывает их рядом со словом `KNOCKED DOWN`. Это иллюстрация R7, а не дефект: цифра та же, смысл назван.

- [ ] **Step 1: Заголовок компонента движения**

В `Source/StarForge/Public/Player/SFShipboardMovementComponent.h` включение после `GameFramework/CharacterMovementComponent.h`:

```cpp
#include "ShipFrameMath.h"
```

В public, после строки `virtual void UpdateBasedMovement(float DeltaSeconds) override;` (так `protected:` в конце вставки ложится прямо перед существующим `private:`):

```cpp
	/**
	 * Whether the occupant is knocked off his feet (spec 6): the movement mode is
	 * falling, walking is refused, and the carry and the gravity alignment go on
	 * exactly as before. Standing up again takes half a second continuously under
	 * the grav-plate limit.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	bool IsKnockedDown() const { return Footing.bDown; }

	/** Whether sf.Knockdown is on. Off is a different rig, and every readout says so. */
	bool IsKnockdownEnabled() const;

	/**
	 * A seated occupant is strapped in and never loses his footing, whatever the ship
	 * does. Set by the character on taking and leaving a seat.
	 */
	void SetStrappedIn(bool bInStrappedIn) { bStrappedIn = bInStrappedIn; }
	bool IsStrappedIn() const { return bStrappedIn; }

	/** Seconds spent knocked down during the current settled interval. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetKnockedDownSeconds() const { return KnockedDownSeconds; }

	/** Knocked-down seconds belonging to the interval reported by GetBestCompletedSeconds. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetBestCompletedKnockedDownSeconds() const { return BestCompletedKnockedDownSeconds; }

	/** The felt acceleration the last footing decision was taken on, ship frame, m/s^2. */
	FVector GetFeltAccelerationMetersPerSecSq() const { return FeltAccelMetersPerSecSq; }

	/** What pushes him while he is down, ship frame, m/s^2. Zero on his feet. */
	FVector GetKnockdownPushMetersPerSecSq() const { return PushMetersPerSecSq; }

	/**
	 * What the drift figure means right now, in words, for the log and the HUD alike:
	 * the carry failed, or the man was knocked down, or neither. The number alone
	 * cannot say, and it used to mean only the first.
	 */
	FString DescribeFooting() const;

	/** False while knocked down: a man who is down does not land. See the cpp. */
	virtual bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** No landing while knocked down, whichever path asks. See the cpp. */
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
```

В private, после объявления `LogDiagnostics`:

```cpp
	/**
	 * Decides whether he keeps his feet this frame and acts on it: enters falling with
	 * the base kept, pushes him with what the plates could not absorb, stands him up
	 * again. Once per frame, like UpdateDriftMeasurement, and for the same reason.
	 */
	void UpdateFooting(float DeltaSeconds);

	/**
	 * Reports, from outside the carry, a carry gap that is still open. The counter in
	 * DetectCarryGap cannot: it lives inside the function the engine has stopped
	 * calling. Once a second, for as long as the gap lasts.
	 */
	void LogOpenCarryGap(float DeltaSeconds);
```

и после `bHasCarriedOnce`:

```cpp
	StarForge::ShipFrame::FFootingState Footing;

	/** Ship-frame figures behind the last footing decision, for the readouts. */
	FVector FeltAccelMetersPerSecSq = FVector::ZeroVector;
	FVector PushMetersPerSecSq = FVector::ZeroVector;

	/** Set while he holds a seat. Strapped in, he is never knocked down. */
	bool bStrappedIn = false;

	/** Knocked-down time in the current settled interval; reset with the peaks. */
	double KnockedDownSeconds = 0.0;

	/** Length of the knockdown in progress, for the line that says how long he was down. */
	double CurrentKnockdownSeconds = 0.0;

	double BestCompletedKnockedDownSeconds = 0.0;

	/** Same guard as LastMeasuredFrame, for the same reason: two calls a frame would push twice. */
	uint64 LastFootingFrame = TNumericLimits<uint64>::Max();

	/** Throttle for LogOpenCarryGap. */
	double SecondsSinceGapLog = 0.0;
```

- [ ] **Step 2: Реализация в компоненте движения**

В `Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp` включения:

```cpp
#include "Engine/World.h"
#include "Units.h"
```

(`ShipFrameMath.h` приходит через заголовок.) В анонимное пространство имён, после `SettleSeconds`:

```cpp
	/**
	 * How long continuously under the grav-plate limit before a knocked-down occupant
	 * gets his feet back. The same 0.5 the FFootingState tests use.
	 */
	constexpr double KnockdownRecoverySeconds = 0.5;

	/**
	 * Lateral friction while he slides, per second. ApplyVelocityBraking multiplies
	 * this by BrakingFrictionFactor, which the engine defaults to 2.0
	 * (CharacterMovementComponent.cpp 734, 4384-4385), so the coefficient that acts
	 * is 4/s and the settling speed under a steady push is a quarter of the excess:
	 * the spec's worst case, 19.86 m/s^2 four metres from the axis in a full roll,
	 * slides him at 5 m/s. The friction bounds the slide; the plates' ceiling does not.
	 */
	constexpr float KnockdownSlideFriction = 2.0f;
```

После `CVarDriftDebug`:

```cpp
static TAutoConsoleVariable<int32> CVarKnockdown(
	TEXT("sf.Knockdown"),
	1,
	TEXT("1: an occupant on his feet is knocked down when the felt acceleration exceeds the grav-plate limit (spec 6): falling, walking refused, still carried.\n")
	TEXT("0: he keeps his feet whatever the ship does. Needed to reproduce checkpoint 1's baseline, whose scripted profile is over the limit almost continuously, ")
	TEXT("and for the sf.ShipGravity 0 negative control, which the knockdown would otherwise mask by keeping the movement base."),
	ECVF_Default);
```

В конструктор, после `bIgnoreBaseRotation = true;`:

```cpp
	// The slide has to stop somewhere short of the far wall on its own. The default,
	// zero, would let a steady push accelerate him without limit; see
	// KnockdownSlideFriction for the number and the factor hidden in it.
	FallingLateralFriction = KnockdownSlideFriction;
```

В `SetShipFrame`, после `bHasCarriedOnce = false;`:

```cpp
	// A new frame is a new footing. The flag is the one that keeps the base through a
	// knockdown, and it must not outlive the knockdown it was set for.
	Footing = StarForge::ShipFrame::FFootingState();
	bStayBasedInAir = false;
	FeltAccelMetersPerSecSq = FVector::ZeroVector;
	PushMetersPerSecSq = FVector::ZeroVector;
	CurrentKnockdownSeconds = 0.0;
	LastFootingFrame = TNumericLimits<uint64>::Max();
```

и после `BestCompletedPeakJitterCm = 0.0;`:

```cpp
	BestCompletedKnockedDownSeconds = 0.0;
```

В `UpdateBasedMovement` заменить две последние строки

```cpp
	CarryWithFrame();
	UpdateDriftMeasurement(DeltaSeconds);
```

на

```cpp
	CarryWithFrame();

	// Footing before the measurement, so the figure taken this frame already carries
	// what this frame decided about him.
	UpdateFooting(DeltaSeconds);
	UpdateDriftMeasurement(DeltaSeconds);
```

Новые функции — в конец файла:

```cpp
bool USFShipboardMovementComponent::IsKnockdownEnabled() const
{
	return CVarKnockdown.GetValueOnGameThread() != 0;
}

void USFShipboardMovementComponent::UpdateFooting(float DeltaSeconds)
{
	// Once a frame, for the reason UpdateDriftMeasurement gives: the engine calls
	// UpdateBasedMovement twice a frame, and a push added on both calls would be twice
	// the push the maths says -- the doubled clock all over again, in velocity.
	const uint64 ThisFrame = GFrameCounter;
	if (ThisFrame == LastFootingFrame)
	{
		return;
	}
	LastFootingFrame = ThisFrame;

	// Everything in the ship's frame, in SI, from the one instrument. The frame
	// component samples in TG_PostPhysics, so on this pre-physics call these are last
	// frame's figures; a frame of lag against a half-second hysteresis is nothing.
	const FTransform FrameTransform = ShipFrame->GetFrameTransform();
	const FVector LocalOmega = FrameTransform.InverseTransformVectorNoScale(ShipFrame->GetAngularVelocityRadPerSec());
	const FVector LocalAccel = FrameTransform.InverseTransformVectorNoScale(ShipFrame->GetLinearAccelMetersPerSecSq());

	const FVector LocalCm = GetLocalLocation();
	const FVector LocalOffsetMetres(
		StarForge::Units::CmToMetres(LocalCm.X),
		StarForge::Units::CmToMetres(LocalCm.Y),
		StarForge::Units::CmToMetres(LocalCm.Z));

	FeltAccelMetersPerSecSq = StarForge::ShipFrame::ComputeFeltAcceleration(LocalAccel, LocalOmega, LocalOffsetMetres);

	// Strapped in, or with the mechanic off, nothing is over the limit as far as
	// footing is concerned. Routed through Update rather than skipped, so a man who is
	// down when the switch is thrown stands up through the same half second as
	// everyone else, not by a second path.
	const bool bOverLimit = IsKnockdownEnabled() && !bStrappedIn
		&& StarForge::ShipFrame::ExceedsGravPlateLimit(FeltAccelMetersPerSecSq);

	const bool bWasDown = Footing.bDown;
	Footing.Update(bOverLimit, DeltaSeconds, KnockdownRecoverySeconds);

	if (Footing.bDown && !bWasDown)
	{
		// Set BEFORE the mode change. OnMovementModeChanged reads it on entry to
		// falling and, with it on, skips both the imparting of base velocity and the
		// clearing of the base (CharacterMovementComponent.cpp 1459-1472). The base is
		// what MaybeUpdateBasedMovement checks before calling UpdateBasedMovement at
		// all; losing it stops the carry, and stopping the carry at 400 m/s is not
		// knocking a man down, it is deleting him.
		bStayBasedInAir = true;
		SetMovementMode(MOVE_Falling);
		CurrentKnockdownSeconds = 0.0;

		UE_LOG(LogStarForge, Display,
			TEXT("Footing LOST at t=%.2f s, ship-local (%.0f %.0f %.0f) cm: felt %.2f m/s2 over the %.2f limit, ")
			TEXT("ship turning %.1f deg/s; falling, walking refused, still carried."),
			GetWorld()->GetTimeSeconds(), LocalCm.X, LocalCm.Y, LocalCm.Z,
			FeltAccelMetersPerSecSq.Size(), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq,
			FMath::RadiansToDegrees(LocalOmega.Size()));
	}

	if (Footing.bDown)
	{
		// What the plates could not absorb, reversed, as this frame's change of
		// velocity. Straight into Velocity rather than AddForce: PerformMovement applies
		// accumulated forces after this call and before StartNewPhysics (2840, 2871,
		// 2883), so either would land in this frame's PhysFalling, but AddForce also
		// divides by the character's Mass on the way and this is an acceleration already.
		PushMetersPerSecSq = StarForge::ShipFrame::ComputeUncompensatedAcceleration(FeltAccelMetersPerSecSq);

		const FVector PushWorld = FrameTransform.TransformVectorNoScale(PushMetersPerSecSq);
		Velocity += FVector(
			StarForge::Units::MetresToCm(PushWorld.X),
			StarForge::Units::MetresToCm(PushWorld.Y),
			StarForge::Units::MetresToCm(PushWorld.Z)) * DeltaSeconds;

		KnockedDownSeconds += DeltaSeconds;
		CurrentKnockdownSeconds += DeltaSeconds;
		return;
	}

	PushMetersPerSecSq = FVector::ZeroVector;

	if (bWasDown)
	{
		// Off the moment his footing returns. The only transition that reads the flag
		// is walking into falling, and until he lands there is none. Left on for good
		// it would hide the negative control: a deck made unwalkable by sf.ShipGravity 0
		// would keep its base instead of losing it, and the control would show nothing.
		bStayBasedInAir = false;

		UE_LOG(LogStarForge, Display,
			TEXT("Footing regained at t=%.2f s after %.1f s down, ship-local (%.0f %.0f %.0f) cm; landing is the engine's again."),
			GetWorld()->GetTimeSeconds(), CurrentKnockdownSeconds, LocalCm.X, LocalCm.Y, LocalCm.Z);
	}
}

bool USFShipboardMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	// PhysFalling asks this on every blocking hit and lands him the moment it says
	// yes (cpp 5050-5054): in the same frame, under the very acceleration that took
	// his feet. With no, the hit goes to HandleImpact, then ComputeSlideVector, and the
	// velocity is re-derived from the slide (5078, 5122, 5135): while he is down the
	// deck is something to slide along, not something to stand on.
	if (Footing.bDown)
	{
		return false;
	}

	return Super::IsValidLandingSpot(CapsuleLocation, Hit);
}

void USFShipboardMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	// The one landing the answer above cannot refuse: a character wedged between two
	// walls is landed by PhysFalling without asking (cpp 5221-5225). Landing is
	// SetMovementMode(MOVE_Walking), and walking takes its base from the floor under
	// him (1450-1452) -- a man pinned in a ceiling corner has none, and would come off
	// the ship. While he is down there is no landing by any path; the time is spent
	// falling, as the stock function would have spent it after landing.
	if (Footing.bDown)
	{
		StartNewPhysics(remainingTime, Iterations);
		return;
	}

	Super::ProcessLanded(Hit, remainingTime, Iterations);
}

void USFShipboardMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// After the engine's own tick, so "did the carry run this frame" has its answer.
	LogOpenCarryGap(DeltaTime);
}

void USFShipboardMovementComponent::LogOpenCarryGap(float DeltaSeconds)
{
	if (!ShipFrame.IsValid())
	{
		return;
	}

	// The HUD's threshold, for the HUD's reason: one frame is tick order, two is a gap.
	const int64 Frames = GetFramesSinceLastCarry();
	if (Frames <= 1)
	{
		SecondsSinceGapLog = 0.0;
		return;
	}

	// The first line the moment the gap is seen, then one a second. This is the log's
	// only word on a base loss that never ends: DetectCarryGap counts skipped carries
	// from inside the carry, and a carry the engine has stopped calling counts nothing.
	SecondsSinceGapLog += DeltaSeconds;
	if (Frames == 2 || SecondsSinceGapLog >= 1.0)
	{
		const UObject* Base = GetMovementBaseObject();
		UE_LOG(LogStarForge, Warning,
			TEXT("Carry NOT RUNNING: %lld frames since the occupant was last carried (mode %s, base %s). ")
			TEXT("He has come off the ship -- this is the breakage, not a knockdown."),
			Frames, *GetMovementName(), Base ? *Base->GetName() : TEXT("none"));
		SecondsSinceGapLog = 0.0;
	}
}

FString USFShipboardMovementComponent::DescribeFooting() const
{
	// In order of what invalidates what. Skipped carries are the defect this
	// instrument exists to find and outrank everything; the mechanic being off makes
	// the run a different rig; a knockdown explains a drift figure that is otherwise
	// over budget; only then is the figure the carry's own.
	if (SkippedCarryFrames > 0)
	{
		return TEXT("CARRY FAILED: the skipped carries are the defect");
	}
	if (!IsKnockdownEnabled())
	{
		return TEXT("knockdown off (sf.Knockdown 0)");
	}
	if (KnockedDownSeconds > 0.0)
	{
		return TEXT("KNOCKED DOWN: the drift is his slide, not the carry");
	}
	return TEXT("footing intact");
}
```

В `UpdateDriftMeasurement` периодическую строку заменить на:

```cpp
			UE_LOG(LogStarForge, Log,
				TEXT("Shipboard drift: %.1f s settled, drift now %.3f cm, peak drift %.3f cm, ")
				TEXT("peak jitter %.3f cm/tick, skipped carries %lld, knocked down %.1f s -- %s"),
				MeasuredSeconds, DriftCm, PeakDriftCm, PeakJitterCm, SkippedCarryFrames,
				KnockedDownSeconds, *DescribeFooting());
```

В `RestartMeasurementInterval`: внутри `if (MeasuredSeconds > BestCompletedSeconds)` после `BestCompletedPeakJitterCm = PeakJitterCm;` добавить `BestCompletedKnockedDownSeconds = KnockedDownSeconds;`; строку `Shipboard drift interval complete` заменить на:

```cpp
			UE_LOG(LogStarForge, Log,
				TEXT("Shipboard drift interval complete: %.1f s, peak drift %.3f cm, ")
				TEXT("peak jitter %.3f cm/tick, skipped carries %lld, knocked down %.1f s -- %s"),
				MeasuredSeconds, PeakDriftCm, PeakJitterCm, SkippedCarryFrames,
				KnockedDownSeconds, *DescribeFooting());
```

и в конце, после `NextPeriodicLogSeconds = 10.0;`:

```cpp
	KnockedDownSeconds = 0.0;
```

- [ ] **Step 3: Персонаж — пристёгнут в кресле, ноги не работают лёжа**

В `Source/StarForge/Private/Player/SFCharacter.cpp`, в `PollMovementInput`, после первого `if (IsSeated()) { return; }`:

```cpp
	// Legs do not work lying down. Refused here, at the input, rather than through the
	// movement mode: while he is down the mode is falling, and walking input there is
	// air control -- the stock five per cent of it is exactly the kind of motion that
	// would read as drift with nobody's name on it.
	if (const USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		if (Movement->IsKnockedDown())
		{
			return;
		}
	}
```

В `ToggleSeat`, в ветке `if (SeatedStation)`, после `SeatedStation = nullptr;`:

```cpp
		// Unstrapped the moment he stands. Under a manoeuvre over the limit the next
		// footing update knocks him down, which is what spec 6 says happens to a man
		// who leaves his seat in one.
		if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
		{
			Movement->SetStrappedIn(false);
		}
```

Далее строку

```cpp
	USFShipFrameComponent* Frame = GetShipboardMovement() ? GetShipboardMovement()->GetShipFrame() : nullptr;
```

заменить на

```cpp
	USFShipboardMovementComponent* Movement = GetShipboardMovement();
	if (Movement && Movement->IsKnockedDown())
	{
		// A man on the deck cannot reach for a harness. Said out loud, because the key
		// doing nothing is otherwise the "out of reach" case, and the two differ.
		UE_LOG(LogStarForge, Display, TEXT("No seat while knocked down."));
		return;
	}

	USFShipFrameComponent* Frame = Movement ? Movement->GetShipFrame() : nullptr;
```

и в конце функции блок

```cpp
	if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		Movement->StopMovementImmediately();
	}
```

заменить на

```cpp
	if (Movement)
	{
		Movement->StopMovementImmediately();

		// Strapped in: seated, he is never knocked down, whatever the felt figure says.
		// Checkpoint 4's run C -- seated through the full profile -- depends on it.
		Movement->SetStrappedIn(true);
	}
```

Комментарий над этим блоком (про `MOVE_Walking` и `IsSeated`) остаётся: он по-прежнему верен.

- [ ] **Step 4: HUD — строка опоры и смысл увода**

В `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`, в `DrawHUD`, после блока `CARRY NOT RUNNING` и до строки `elapsed`:

```cpp
	// The third of the lines that give the drift figure its meaning. The two above say
	// the carry failed; this one says the man was knocked down, and a slide under it is
	// not a carry failure -- "drift" used to mean only the first. Yellow with the
	// mechanic off, because that run is not comparable with one that has it.
	if (!Movement->IsKnockdownEnabled())
	{
		DrawLine(TEXT("footing  KNOCKDOWN OFF (sf.Knockdown 0) - checkpoint 1's rig, not this plan's"), Y, FLinearColor::Yellow);
	}
	else if (Movement->IsKnockedDown())
	{
		const FVector Push = Movement->GetKnockdownPushMetersPerSecSq();
		DrawLine(FString::Printf(TEXT("footing  KNOCKED DOWN %.1f s   push (%.1f %.1f %.1f) m/s2 ship axes   %s"),
			Movement->GetKnockedDownSeconds(), Push.X, Push.Y, Push.Z, *Movement->DescribeFooting()),
			Y, FLinearColor(1.0f, 0.45f, 0.0f));
	}
	else
	{
		DrawLine(FString::Printf(TEXT("footing  intact%s   knocked down %.1f s this interval   %s"),
			Movement->IsStrappedIn() ? TEXT(" (strapped in)") : TEXT(""),
			Movement->GetKnockedDownSeconds(), *Movement->DescribeFooting()), Y);
	}
```

Вызов `DrawLine` со строкой `drift peak` (четыре строки от `DrawLine(` до `bWithinBudget ? … : FLinearColor::Yellow));`) заменить на:

```cpp
	// Orange, not red, when the interval had him on the deck: the figure is over
	// budget because he slid, and the word beside it says so. Red stays for a figure
	// over budget with him on his feet, which is the carry's own failure.
	const bool bDownThisInterval = Movement->GetKnockedDownSeconds() > 0.0;
	DrawLine(
		FString::Printf(TEXT("drift peak           %.3f cm   [budget 1.0 cm/min]%s"), PeakDrift,
			bDownThisInterval ? TEXT("   <- includes his slide while knocked down") : TEXT("")),
		Y,
		bWithinBudget ? FLinearColor::Green
			: (bMeasured ? (bDownThisInterval ? FLinearColor(1.0f, 0.45f, 0.0f) : FLinearColor::Red) : FLinearColor::Yellow));
```

Объявление `const FLinearColor BestColor = …` (три строки) и вызов `DrawLine` со строкой `best drift peak` — на:

```cpp
	const bool bBestHadHimDown = Movement->GetBestCompletedKnockedDownSeconds() > 0.0;
	DrawLine(
		FString::Printf(TEXT("best drift peak      %.3f cm   [budget 1.0 cm/min]%s"), BestPeakDrift,
			bBestHadHimDown ? TEXT("   <- includes a slide while knocked down") : TEXT("")),
		Y,
		bBestWithinBudget ? FLinearColor::Green
			: (bBestMeasured ? (bBestHadHimDown ? FLinearColor(1.0f, 0.45f, 0.0f) : FLinearColor::Red) : FLinearColor::Yellow));
```

Внутри `if (Frame && Ship)` заменить всё от комментария `// omega x (omega x r) is the centripetal term of one frame only.` до закрывающей скобки вызова `DrawLine(... felt acceleration ...)` включительно на:

```cpp
		// The figure the footing decision was taken on, not a second computation of it
		// here: the screen has to show the number the mechanic saw, or the two can
		// disagree and nobody will know which to believe.
		const FVector Felt = Movement->GetFeltAccelerationMetersPerSecSq();
		const bool bOverLimit = StarForge::ShipFrame::ExceedsGravPlateLimit(Felt);
		DrawLine(
			FString::Printf(TEXT("felt acceleration    %.2f m/s2   [limit %.2f]%s"),
				Felt.Size(), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq,
				(bOverLimit && !Movement->IsKnockedDown())
					? TEXT("   over the limit, on his feet: strapped in or sf.Knockdown 0")
					: TEXT("")),
			Y,
			Movement->IsKnockedDown()
				? FLinearColor(1.0f, 0.45f, 0.0f)
				: (bOverLimit ? FLinearColor::Yellow : FLinearColor::White));
```

`PanelHeight` не трогать: 760 из задачи 4 уже посчитаны с этой строкой.

- [ ] **Step 5: Собрать и прогнать четыре раза**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Головные тесты не трогались; их 52 остаются 52 — проверить один раз командой из «Как проверяется».

**Прогон A — тяга, механика включена, ничего не происходит (25 с).** Рецепт по умолчанию из `CLAUDE.md`; R8(а) — он обязан объяснить себя.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log
```

Ожидается: **ни одной** строки `Footing LOST` и `Carry NOT RUNNING` — 16.0 м/с² под пределом 19.62, а центростремительного нет; строки

```
LogStarForge: Shipboard drift: 10.0 s settled, drift now 0.000 cm, peak drift 0.000 cm, peak jitter 0.000 cm/tick, skipped carries 0, knocked down 0.0 s -- footing intact
```

на 10.0 и 20.0 с. Время «сбит с ног» напечатано нулём, а не отсутствует.

**Прогон B — крен, уронен и прижат (25 с).** Человек в 3 м от оси крена: `ω²r > 19.62` при `ω > √(19.62/3) = 2.557 рад/с = 146.5°/с`. Момент по крену: четыре двигателя ±Z в 2 м от оси по 200 кН — пара 1600 кНм; `Ixx` 533 333 кг·м² → `α = 3.0 рад/с² = 171.9°/с²`. Спрос `I·(ω_цель − ω)·AngularGain` при `AngularGain` 3.0 насыщается, пока ошибка больше `1.6·10⁶/(533 333·3) = 1.0 рад/с = 57.3°/с`, — до 122.7°/с на `t ≈ 0.71 + 0.05` с раскрутки; дальше `ω = 180 − 57.3·e^{−3(t−t₁)}`, и 146.5 достигается через `ln(57.3/33.5)/3 = 0.18` с. Итого сбивание на **0.9–1.1 с**.

```bash
... -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.Roll 1" -log
```

Ожидается:

```
LogStarForge: Footing LOST at t=0.9x s, ship-local (0 0 -300) cm: felt 19.6x m/s2 over the 19.62 limit, ship turning 146.x deg/s; falling, walking refused, still carried.
```

`t` в 0.85–1.20; `felt` в 19.62–20.1 (ω растёт на 1.7°/с за кадр, ощущаемое — на 0.45 м/с² за кадр); `ship turning` в 146.5–148.5; положение — в пределах ±15 см по X и Y от `(0 0 -300)`. Первая строка `Flight:` — `ang roll/pitch/yaw (150±5 0.0 0.0)`, вторая — не меньше 178, с третьей — `180.0`. Затем

```
LogStarForge: Shipboard drift: 10.0 s settled, drift now 2.xxx cm, peak drift 2.xxx cm, peak jitter 0.xxx cm/tick, skipped carries 0, knocked down 9.x s -- KNOCKED DOWN: the drift is his slide, not the carry
```

`drift now` и `peak drift` — 1.8–2.5 см (пункт 10: капсула легла с высоты подвеса на контакт); `knocked down` — 9.3–9.7 (интервал начался на 0.5 с, сбивание на ~0.95); на 20.0 с — на десять больше. `peak jitter` — записать; ожидается ноль или сотые. Толчок здесь `(0 0 −9.99)` — ощущаемое `π²·3 = 29.61` к оси, избыток 9.99 от оси, то есть в пол: он прижат, а не волочится, и `drift` не растёт. **Ни одной** `Carry NOT RUNNING`, ни одной `Footing regained`: крен держится до выхода. Если `Footing LOST` есть, а `KNOCKED DOWN` в строке увода нет — `KnockedDownSeconds` сбрасывается не там; если `skipped carries` не ноль — флаг выставлен после `SetMovementMode`, а не до.

**Прогон C — скриптовый профиль стоя: стенд точки проверки 1 как он есть теперь, и как его вернуть (25 с и 65 с).** Профиль пишет `400·sin` м/с — 314 м/с² линейного на пиках плюс до 43 центростремительного при `|ω|` 216.3°/с. Стоящего роняет с первого переноса, и он летает между стенами: толчок до 294 м/с² вдоль ∓X, установившаяся скорость `294/4 = 73` м/с, но стены в 7.1 м ближе — от стены до стены он разгоняется до `√(2·294·7.1) = 64` м/с, это 107 см за кадр. Именно поэтому базу точки проверки 1 больше нельзя снять с включённой механикой — R8(в).

```bash
... -seconds=25 -ExecCmds="sf.FlightLog 1, sf.DriftDebug 1" -log
```

Ожидается `Footing LOST at t=0.1x–0.3x s … felt 250–340 m/s2 … ship turning 216.x deg/s` — с первого переноса (десятый-одиннадцатый кадр, наблюдение №1 бланка точки проверки 4); строка `Drift debug [CHANGED]: … mode Falling, base <панель>` и дальше `Drift debug` **раз в секунду до самого выхода** с `mode Falling` и **никогда** `base none`; на 10.0 и 20.0 с — `Shipboard drift: … peak drift <800 cm, peak jitter <120 cm/tick, skipped carries 0, knocked down 9.x s -- KNOCKED DOWN …`. Порог `peak drift` 800 — размер каюты: больше значит, что он вышел сквозь стену, и это дефект. **Ни одной** `Carry NOT RUNNING`. Это R6, доказанная на худшем манёвре стенда: перенос держит человека, которого швыряет по каюте на 400 м/с и 180°/с.

Теперь то же с выключенной механикой — историческая база:

```bash
... -seconds=65 -ExecCmds="sf.FlightLog 1, sf.Knockdown 0" -log
```

Ожидается **ни одной** `Footing LOST` и на 60.0 с:

```
LogStarForge: Shipboard drift: 60.0 s settled, drift now 0.000 cm, peak drift 0.000 cm, peak jitter 0.000 cm/tick, skipped carries 0, knocked down 0.0 s -- knockdown off (sf.Knockdown 0)
```

— цифры бланка от 2026-09-08 и точки проверки 4 («уже проверено без рук») с новым хвостом, который говорит, каким стендом они сняты.

**Прогон D — отрицательный контроль: перенос действительно сломан (25 с).** `sf.ShipGravity 0` — существующий контроль; 2026-09-08 он показал `Floor → WallXNeg → Ceiling` за 202 мс и полное молчание переноса. С включённой механикой он показал бы ничего (пункт 9), поэтому — с `sf.Knockdown 0`.

```bash
... -seconds=25 -ExecCmds="sf.FlightLog 1, sf.DriftDebug 1, sf.ShipGravity 0, sf.Knockdown 0" -log
```

Ожидается: от одной до четырёх строк `Drift debug [CHANGED]` в первой секунде с `ship gravity 0` (палуба, кренящаяся на 180°/с, проходит предел 44.8° за четверть секунды; 2026-09-08 на 600 fps их было четыре), после чего `Drift debug` **прекращаются** — движок перестал звать перенос; **ни одной** `Shipboard drift:` (интервал в 10 с не наберётся); **ни одной** `Footing LOST`; первая строка `Carry NOT RUNNING: 2 frames …` в тот же миг, и до самого выхода раз в секунду:

```
LogStarForge: Warning: Carry NOT RUNNING: 6x frames since the occupant was last carried (mode Falling, base none). He has come off the ship -- this is the breakage, not a knockdown.
```

с числом кадров, растущим примерно на 60 от строки к строке. Строки `Flight:` продолжаются: корабль летит, человека в нём нет.

**Что доказано этими четырьмя.** Прогон, где перенос сломан (D), и прогон, где человека всего лишь уронило (B, C), различаются в журнале **словами**, а не только цифрами: `Carry NOT RUNNING` и тишина прибора против `Footing LOST` и `KNOCKED DOWN … skipped carries 0` — R7. Рецепт по умолчанию (A) печатает время «сбит с ног» нулём — R8(а). База точки проверки 1 воспроизводится и подписана — R8(в). Перенос не прекращается ни в одном прогоне с включённой механикой — R6.

- [ ] **Step 6: `CLAUDE.md`**

В таблицу переменных, после строки `sf.ConsoleOperator`:

```markdown
| `sf.Knockdown` | 1 | 1 knocks a standing occupant off his feet above the 19.62 m/s² grav-plate limit (spec §6) — falling, walking refused, still carried; 0 keeps him standing. Checkpoint 1's scripted-profile baseline and the `sf.ShipGravity 0` negative control need 0 |
```

После абзаца, начинающегося `**Run the rig headlessly and read the result from the log.**`, и его блока кода — перед строкой `Console variables the rig understands:` — вставить:

```markdown
**The drift line carries knocked-down time and a verdict.** `Shipboard drift: … skipped
carries N, knocked down S s -- <verdict>`, where the verdict is `footing intact`,
`KNOCKED DOWN: …`, `CARRY FAILED: …` or `knockdown off (sf.Knockdown 0)`. Under the
recipe above it reads `knocked down 0.0 s -- footing intact`: 800 kN on 50 t is 16 m/s²,
under the 19.62 limit. The scripted profile (`sf.ShipProfile 1`) is over the limit almost
continuously — 314 m/s² of linear acceleration at its peaks — so a standing occupant is
knocked down for the whole run and the drift figure is his slide across the cabin.
**Re-running checkpoint 1's baseline needs `sf.Knockdown 0`**, and so does the
`sf.ShipGravity 0` negative control: with the knockdown on, `bStayBasedInAir` keeps the
movement base through the fall, the carry keeps running, and the control shows nothing.
A carry that has genuinely stopped is reported by `Carry NOT RUNNING` once a second,
from outside the carry, because the counter inside it cannot report a gap that never ends.
```

В список «Things in the code that look like mistakes and are not»: к первому пункту (`SetGravityDirection`) дописать в конец предложение

```markdown
  That control is run with `sf.Knockdown 0` from now on; the `bStayBasedInAir` entry
  below says why.
```

и добавить два пункта в конец списка:

```markdown
- **`bStayBasedInAir` set only for the duration of a knockdown, `IsValidLandingSpot`
  returning false while down, and `ProcessLanded` refusing to land him.** The knockdown
  puts the occupant into `MOVE_Falling`, and entering falling clears the movement base
  unless that flag is set (`CharacterMovementComponent.cpp` 1459–1472); with no base the
  engine never calls `UpdateBasedMovement`, and the carry — the one thing that keeps him
  aboard at 400 m/s — stops. §6 is explicit that it must not. The flag is not left on
  permanently because a deck made unwalkable by `sf.ShipGravity 0` would then keep its
  base too, and the negative control would show nothing. The landing-spot override is
  what keeps him down: `PhysFalling` lands him on the first blocking hit it is allowed
  to (5050), in the same frame, under the same acceleration. `ProcessLanded` is
  overridden as well because a character wedged between two walls is landed without
  asking (5221–5225), and landing re-derives the base from the floor under him.
- **`FallingLateralFriction = 2.0` where the plan says the coefficient is 4/s.**
  `ApplyVelocityBraking` multiplies it by `BrakingFrictionFactor`, 2.0 by default (734,
  4384–4385). The 4/s that acts is what makes a knocked-down slide settle at a quarter
  of the excess acceleration; writing 4 here would halve that.
```

Абзац `When reading the checkpoint HUD: **check the measured-time line before the drift figure.**` дополнить предложением:

```markdown
And read the `footing` line before judging a drift over budget: a slide while knocked
down is not a carry failure, and the line says which it was.
```

- [ ] **Step 7: Коммит**

```bash
git add Source/StarForge/Public/Player/SFShipboardMovementComponent.h Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp Source/StarForge/Private/Player/SFCharacter.cpp Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp CLAUDE.md
git commit -F - <<'MSG'
feat(player): knock a standing occupant off his feet, and keep carrying him

Spec 6's ruling, as written: over the grav-plate limit a man on his feet loses them --
the movement mode becomes falling and walking is refused -- but the carry and the
gravity alignment go on. Stopping the carry at 400 m/s would not knock him down, it
would delete him, which is the rig's negative control and the state this project calls
a breakage. Entering falling clears the movement base and with it the carry, so the
knockdown sets bStayBasedInAir for exactly as long as he is down; the flag is not left
on, because a deck made unwalkable by sf.ShipGravity 0 would then keep its base and
the control would show nothing. He stays down because IsValidLandingSpot says no while
he is down, and because ProcessLanded refuses the one landing the engine performs
without asking, when he is wedged in a corner.

What pushes him is what the plates could not absorb, reversed, and the slide is bounded
by falling friction rather than by the ceiling. The friction is written as 2 because
the engine doubles it on the way; the 4 per second that acts is the number the plan
reasons with.

The drift instrument's word changes meaning here, and the same change says which
meaning applies. Until now "drift" meant the carry failed; a knocked-down man sliding
across the deck reads as drift too. Both drift lines now carry the knocked-down time
and a verdict -- CARRY FAILED, KNOCKED DOWN, footing intact, or knockdown off -- and
the log gains a line the HUD had and it did not: a carry gap that never closes is
reported from outside the carry, once a second, for as long as it lasts.

sf.Knockdown defaults to on, because a mechanic behind an off switch is a mechanic
nobody has. That changes what the default headless recipe measures, so the recipe
explains itself, the variable is in the table, and CLAUDE.md says that checkpoint 1's
baseline and the negative control are now run with it off, and why.
MSG
```

---

### Task 9: Runbook и бланк точки проверки 5

То, что нельзя проверить без рук («Как проверяется»): согласие картинки с числами — ствол поворачивается туда, куда говорит индикатор, и против вращения корпуса; ощущение сбивания с ног и вставания; скольжение к стене при положении не на оси; отказ ног и кресла лёжа; освобождение сидящего; человек-пилот и автоматика-канонир **одновременно** — сцена поправки спеки целиком. Форма — как у точки проверки 4: runbook с запусками и красными строками, бланк с таблицами «ожидание / факт» и решением.

**Files:**
- Create: `docs/superpowers/notes/2026-09-09-checkpoint-5-runbook.md`
- Create: `docs/superpowers/notes/2026-09-09-checkpoint-5-results.md`

**Interfaces:**
- Consumes: все строки журнала и HUD задач 2, 4, 5, 8: `Target drone spawned/WITHDRAWN`, `Station 'gunner' taken by automation.`, `Station 'gunner' is already held by player; refusing automation.`, `Turret: gunned by …`, блок `gunned by` / `turret` на HUD, `Footing LOST/regained`, `Carry NOT RUNNING`, `No seat while knocked down.`, строка `footing`, слово в строке `drift peak`; переменные `sf.TargetDrone`, `sf.AutomationOperator`, `sf.Knockdown`, `sf.Intent.Yaw`, `sf.Intent.Roll`.
- Produces: два документа.

**Числа, которые runbook просит сверить, выведены здесь.** Мишень: орбита 30 м, 10°/с, наклон 20°; упор по рысканью ±120° срабатывает на `φ = 118.5°` и `241.5°` (задача 5), то есть на 11.9-й и 24.2-й секундах каждого 36-секундного оборота. Ориентиры каюты: шар +X, конус +Y, цилиндр −X (`SFTestShip.cpp`, `MarkerOffsetCm` 300), так что `aim yaw 0` — ствол на шар, `+90` — на конус, `−90` — от конуса, `±120` — на 30° дальше конуса/от конуса к цилиндру; `pitch` положительный — к потолку. Рысканье `sf.Intent.Yaw 1` — 120°/с вокруг Z; стоящий в центре и крепление — **на оси Z**, ощущаемое у обоих ноль, никто не падает, а пеленг на мишень бежит в системе крепления на 120°/с против обхода 90°/с: ствол видимо отстаёт и упирается. Крен `sf.Intent.Roll 1` в центре — задача 8, прогон B: 146.5°/с, 0.9–1.1 с, `felt 29.6`, толчок в пол. Под конусом, в `(0 300 −300)`: `r = √(3² + 3²) = 4.24` м, порог `√(19.62/4.24) = 2.15 рад/с = 123°/с`, ощущаемое при 180°/с `π²·4.24 = 41.9`, избыток 22.2 м/с² **от оси** — по `(0 1 −1)/√2`, боковая часть 15.7 м/с² к стене +Y; установившаяся `15.7/4 = 3.9` м/с, до стены `390 − 34 − 300 = 56` см — прижат к ней за секунду, `ship-local` Y ≈ 356. Вставание: `sf.Intent.Roll 0` тормозит крен на 171.9°/с² — под 146.5 за 0.2 с, плюс полсекунды гистерезиса: от команды до `Footing regained` около 0.7 с, а число `after N s down` в этой строке считается от сбивания — сколько держали крен плюс эти 0.7.

- [ ] **Step 1: Написать runbook**

`docs/superpowers/notes/2026-09-09-checkpoint-5-runbook.md`:

````markdown
# Точка проверки 5 — порядок действий (пост канонира и сбивание с ног)

План «в» собран, 52 головных теста проходят, все headless-проверки пройдены на
фиксированных 60 fps (см. [бланк](2026-09-09-checkpoint-5-results.md), раздел
«Что уже проверено без рук»). Осталось то, чего журнал не умеет: **согласна ли
картинка с числами, и как ощущается сбивание с ног.**

**Что мы выясняем.** Поворачивается ли ствол туда, куда говорит индикатор, и против
вращения корпуса; роняет ли манёвр стоящего и ставит ли его обратно; волочёт ли к стене
того, кто стоит не на оси; отказывают ли лёжа ноги и кресло; не роняет ли сидящего;
работают ли человек в кресле пилота и автоматика в кресле канонира **одновременно** —
сцена поправки спеки §4.

**Сколько это займёт.** Три запуска, минут двадцать.

---

## Перед началом

| Проверить | Как |
|---|---|
| Редактор закрыт | `tasklist \| findstr Unreal` — пусто |
| Ветка та | `git branch --show-current` → `feat/gunner-station` |
| Коммит | `git rev-parse --short HEAD` → записать в бланк |
| Дерево чистое | `git status --short` — пусто |
| Тесты | 52 строки `Result=Passed` в `Saved/Logs/StarForge.log` после команды из `CLAUDE.md` |

Если сомневаешься в сборке — пересобрать:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

---

## Управление

Как в точке проверки 4: стоя `W A S D` — ходьба, стрелки — обзор, `E` — сесть в
ближайшее свободное кресло; сидя `W`/`S`, `A`/`D`, `Space`/`LeftCtrl`, `Q`/`R` — крен,
`↑`/`↓` — тангаж, `←`/`→` — рысканье, `E` — встать. Консоль — `~`.

Два кресла: пилотское в центре, канонирское **в полутора метрах к корме** (к цилиндру).
Радиус посадки 2 м у каждого, садит **ближайшее свободное**: от точки появления оба в
досягаемости.

Лёжа `W A S D` не действуют, стрелки действуют, `E` отвечает в журнале
`No seat while knocked down.` Анимации нет: капсула стоит, «лежит» — это индикатор и
отказ управления.

---

## Как это читать

**Ориентиры вместо компаса.** Шар — нос (+X), конус — правый борт (+Y), цилиндр —
корма (−X). Турель — конус под потолком в центре, остриём в сторону ствола.
`turret aim yaw 0` — остриё на шар, `+90` — на конус, `−90` — от конуса, `±120` — упор,
на 30° дальше в сторону цилиндра. `pitch` положительный — к потолку. Мишень **не видна**:
она снаружи, ствол смотрит на неё сквозь стену.

**Сначала строки с именами.** `flown by` и `gunned by`: зелёный — работает, жёлтый
`unoccupied` — никого, оранжевый `(standing down)` — сидит, но не ведёт, красный —
прибор сломан.

**Строка `footing` — перед тем как читать `drift peak`.** Оранжевый `KNOCKED DOWN` —
цифры увода означают скольжение, не отказ переноса; красные `CARRY …` — отказ. Слово
стоит и в строке `drift peak`, и в журнале.

**Посадка в кресло — прыжок, который прибор увода видит** (точка проверки 4, наблюдение
№2). Нажимать `E` на ходу.

---

## Журналы

После **каждого** запуска, до следующего:

```bash
cp Saved/Logs/StarForge.log docs/superpowers/notes/evidence/2026-09-09-checkpoint-5-A-turret.log
```

(для B — `-B-knockdown.log`, для C — `-C-crew.log`). Числа в бланк — из журнала.
**Не убивать процесс**: `~` → `quit` или закрыть окно.

---

## Запуск A — турель глазами (4 минуты)

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900 -ExecCmds="sf.ShipProfile 0, sf.FlightLog 1"
```

Корабль стоит, консоль в кресле пилота, автоматика — в кресле канонира. На панели:
`flown by console` зелёным, `gunned by automation` зелёным, `turret … ON TARGET`
зелёным, в журнале `Station 'gunner' taken by automation.` и
`Target drone spawned: … anchored to SFTestShip_0, present.`

**A1. Ствол следит.** Задрать голову. Конус под потолком медленно поворачивается —
10°/с, оборот за 36 с. Сверить три момента с ориентирами: `aim yaw` около 0 — остриё
на шар; около +90 — на конус; около −90 — от конуса. Записать, совпало ли, **по знаку**.

**A2. Упор.** На 12-й и 24-й секундах каждого оборота (`bearing yaw` проходит ±120)
панель оранжевеет: `CANNOT BEAR`, `aim yaw ±120.0` стоит, `bearing` уходит дальше.
Ствол видимо **стоит** у упора, потом перекладывается через ноль на другой упор за
2.7 с. Записать: стоял ли, и в какую сторону перекладывался (через шар, а не через
цилиндр).

**A3. Против вращения.** `~` → `sf.Intent.Yaw 1`. Корабль рыскает на 120°/с вокруг
вертикали; ты и крепление на этой оси — никого не роняет, `felt acceleration` около 0.
На экране ничего не движется, кроме ствола: пеленг бежит на 120°/с против обхода 90°/с,
ствол отстаёт, панель — `slewing` / `CANNOT BEAR`, `ON TARGET` редко или никогда.
Записать: движется ли ствол **против** вращения корпуса (в сторону, обратную той, куда
корабль поворачивает). `sf.Intent.Yaw 0`.

**A4. Мишень убрана.** `sf.TargetDrone 0`: журнал `Target drone WITHDRAWN (sf.TargetDrone 0).`,
панель `turret … bearing none NO TARGET firing off` жёлтым, ствол **стоит где стоял**.
`sf.TargetDrone 1` — снова ведёт.

---

## Запуск B — сбивание с ног (главный, ~8 минут)

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900 -ExecCmds="sf.ShipProfile 0, sf.FlightLog 1, sf.DriftDebug 1"
```

Стоишь в центре, `footing intact` белым, `felt acceleration 0.00`.

**B1. Уронило на месте.** `~` → `sf.Intent.Roll 1`. В пределах секунды: панель
`footing KNOCKED DOWN … push (0.0 0.0 -10.0)` оранжевым, `felt acceleration 29.6`
оранжевым, в журнале `Footing LOST at t=… felt 19.6… ship turning 146…`. Ты **не
двигаешься**: толчок в пол, `ship-local position` стоит (Z опустился на ~2 см — капсула
легла с подвеса на контакт). `drift peak` около 2 см **оранжевым** с пометкой про
скольжение, не красным. `W` — ничего. Стрелки — обзор работает. `E` — журнал
`No seat while knocked down.` Красных `CARRY …` **нет**, `skipped carries 0`.

**B2. Встал.** `sf.Intent.Roll 0`. Крен тормозит, и через ~0.7 с после команды —
журнал `Footing regained at t=… after N s down`, панель `footing intact`, `W` снова
ходит. Записать N и сколько прошло от команды до `regained`.

**B3. Волочёт к стене.** Дойти до конуса (правый борт): `ship-local position` около
`0 300 -300`. `sf.Intent.Roll 1`. Уронило раньше, чем в B1 (порог здесь 123°/с), и
**тянет к стене +Y**: `ship-local` Y растёт до ~356 и останавливается — прижат к
стене, `push` на панели около `(0 15.7 -15.7)`. `drift peak` — полметра с лишним,
оранжевым, слово `KNOCKED DOWN`. Красных `CARRY …` **нет**. `sf.Intent.Roll 0` — встал
у стены. Записать Y, на котором остановился.

**B4. Механика выключена.** Вернуться в центр. `sf.Knockdown 0`: панель
`footing KNOCKDOWN OFF (sf.Knockdown 0)` жёлтым. `sf.Intent.Roll 1`: `felt 29.6`
**жёлтым** с пометкой `over the limit, on his feet`, ты стоишь и ходишь. Это стенд
точки проверки 1. `sf.Intent.Roll 0`, `sf.Knockdown 1`.

---

## Запуск C — экипаж: человек-пилот, автоматика-канонир (~5 минут)

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900 -ExecCmds="sf.ShipProfile 0, sf.ConsoleOperator 0, sf.FlightLog 1"
```

`flown by unoccupied` жёлтым, `gunned by automation` зелёным.

**C1. Сцена поправки спеки.** `E` на ходу у центра: журнал `Station 'pilot' taken by
player.`, панель `flown by player` зелёным **и** `gunned by automation` зелёным — два
поста, два оператора, один из них не человек. Держать `→` секунды три: `speed …
angular (0 0 120)`, турель `slewing` / `CANNOT BEAR` — автоматика ведёт, пока ты
летишь. В журнале строки `Flight: flown by player` и `Turret: gunned by automation`
рядом. Отпустить.

**C2. Пристёгнут.** Держать `R` пять секунд: `angular roll 180`, `felt acceleration
29.6` **жёлтым** с пометкой `strapped in`, панель `footing intact (strapped in)`, ты в
кресле. Не отпуская `R`, нажать `E`: встал — и **тут же** `Footing LOST`, `KNOCKED
DOWN`. Корабль крутится дальше (тормозить некому). Встать нельзя, пока крен не сойдёт:
`~` → `sf.ConsoleOperator 1` — консоль берёт пост и тормозит (её намерение ноль),
`Footing regained`. `sf.ConsoleOperator 0`.

**C3. Человек в кресле канонира.** `sf.AutomationOperator 0`: журнал `Station 'gunner'
left by automation.`, панель `gunned by unoccupied` жёлтым, `turret … UNCOMMANDED`.
Отойти на полтора метра к цилиндру, `E`: `Station 'gunner' taken by player.`, панель
`gunned by player (standing down)` **оранжевым** — у человека нет ввода для турели, он
появится в этапе 4 вместе со стволом; это R9, не дефект. `sf.AutomationOperator 1`:
журнал `Station 'gunner' is already held by player; refusing automation.` — один раз,
не поток. `E` — встал, и **сразу** `Station 'gunner' taken by automation.`

---

## Красные и оранжевые строки

| Строка | Значение |
|---|---|
| `NO SFCHARACTER POSSESSED` / `NO SHIPBOARD MOVEMENT COMPONENT` / `NO SHIP FRAME BOUND` | Сцена не собралась |
| `CARRY SKIPPED n ticks` / `CARRY NOT RUNNING` | Перенос отказал — красный, обесценивает всё |
| `footing KNOCKED DOWN` оранжевым | Механика сработала; увод под ним — скольжение |
| `footing KNOCKDOWN OFF` жёлтым | `sf.Knockdown 0`: стенд точки проверки 1, не этого плана |
| `gunner -- ship has no gunner station or no turret` | На корабле нет поста канонира |
| `turret … CANNOT BEAR` оранжевым | Цель за упором — состояние, не дефект |
| `flight mode: UNKNOWN`, `flown by … (profile unknown)` | Панель или пост не видят `sf.ShipProfile` |

---

## Если что-то пошло не так

**Ствол смотрит не туда, куда говорит `aim`.** Записать знак и остановиться: это
расхождение картинки с числами, которое тест `AimRoundTripsThroughTheRotator` должен
был исключить. Проверить, что ствол — `TurretBarrel`, а конус повёрнут на −90 по
тангажу (`SFTestShip.cpp`).

**Уронило, и появились `CARRY …`.** Опора потеряна при сбивании — R6 нарушена.
Записать журнал и остановиться; смотреть `bStayBasedInAir` и порядок его выставления.

**Уронило и не встаёт после `Roll 0`.** Смотреть `Flight: … ang roll` — если крен не
тормозит, консоль не в кресле (`sf.ConsoleOperator`); если тормозит, а `regained` нет —
`Footing.Update` не получает `false`.

**`E` в кресле канонира садит в пилотское.** Ты ближе к центру, чем к корме: отойти.

**Журнал обрывается на нулевом кадре.** Процесс убили. Повторить, выйти через `quit`.

---

## Что дальше

| Исход | Что делаем |
|---|---|
| Всё по ожиданию | Слияние `feat/gunner-station` в `main`, handoff этапа 3 |
| Картинка не согласна с числами | Чиним задачи 1/3, повторяем A |
| Сбивание роняет опору или не встаёт | Чиним задачу 8, повторяем B |
| Экипаж не работает одновременно | Чиним задачи 4/5, повторяем C |
| Замер не состоялся | Чиним стенд. Решений о дизайне не принимаем |

Заполнить [бланк](2026-09-09-checkpoint-5-results.md).
````

- [ ] **Step 2: Написать бланк**

`docs/superpowers/notes/2026-09-09-checkpoint-5-results.md`:

````markdown
# Точка проверки 5 — результаты (пост канонира и сбивание с ног)

**Дата прогона:** ____
**Сборка:** StarForgeEditor Win64 Development, UE 5.8.2
**Ветка / коммит:** `feat/gunner-station` / `____`
**Частота кадров в окне** (`stat fps`): ____ fps

Порядок действий — в [runbook](2026-09-09-checkpoint-5-runbook.md). Числа — из журналов
`evidence/2026-09-09-checkpoint-5-{A,B,C}-*.log`, не с экрана.

---

## Что уже проверено без рук (fixed 60 fps, headless)

| Проверка | Факт |
|---|---|
| Головные тесты, `StarForgeCore` только на `Core`/`CoreUObject` | 52 / 52 |
| Мишень спавнится, `sf.TargetDrone 0` убирает её | строки `spawned … present`, `WITHDRAWN` |
| Пост канонира без оператора | `gunned by unoccupied … UNCOMMANDED` |
| Автоматика ведёт по хронологии | `ON TARGET` на 1/5/10/25 с, `CANNOT BEAR` на 15/20 с, `aim` = пеленгу в 0.5° |
| `sf.TargetDrone 0` / `sf.AutomationOperator 0` | `NO TARGET`, ствол держит / `left by automation` |
| Ускорение из компонента системы отсчёта | `accel 16.0 m/s2` при 800 кН на 50 т; 314 на пиках профиля |
| Тяга, механика включена | `knocked down 0.0 s -- footing intact`, увод 0.000 |
| Крен 180°/с стоя в центре | `Footing LOST` на ~1 с, `KNOCKED DOWN`, увод ~2 см, `skipped carries 0` |
| Профиль стоя, механика включена / выключена | уронен с первого переноса, перенос не прекращался / база 2026-09-08 воспроизведена, `knockdown off` |
| Отрицательный контроль `sf.ShipGravity 0, sf.Knockdown 0` | `Carry NOT RUNNING` раз в секунду, прибор молчит, `Footing LOST` нет |

Не проверено ничем, кроме этого бланка: картинка, ощущение, стена, кресло лёжа,
пристёгнутый, экипаж одновременно.

---

## Сначала — действителен ли прогон

| Условие | Требование | Факт |
|---|---|---|
| Красных строк на панели не было ни в одном запуске | да | ____ |
| `skipped carries` во всех трёх журналах | 0 | ____ |
| Журналы скопированы в `evidence/` до следующего запуска | три файла | ____ |
| Журналы заканчиваются `Log file closed`, не нулевым кадром | да | ____ |

---

## Запуск A — турель глазами

| Шаг | Ожидание | Факт |
|---|---|---|
| Старт | `gunned by automation` зелёным, `ON TARGET`, `taken by automation` в журнале | ____ |
| A1 `aim yaw ≈ 0` | остриё на шар | ____ |
| A1 `aim yaw ≈ +90` | остриё на конус | ____ |
| A1 `aim yaw ≈ −90` | остриё от конуса | ____ |
| A2 упор | `CANNOT BEAR`, `aim ±120.0` стоит, ствол видимо стоит | ____ |
| A2 перекладка | через ноль (мимо шара), ~2.7 с | ____ |
| A3 `sf.Intent.Yaw 1` | никого не роняет, ствол идёт против вращения корпуса | ____ |
| A3 состояние | `slewing` / `CANNOT BEAR`, `ON TARGET` редко | ____ |
| A4 `sf.TargetDrone 0` | `NO TARGET`, ствол стоит где стоял | ____ |

**Согласие картинки с числами (знак):** ____ (совпало / зеркально по рысканью / по тангажу)

---

## Запуск B — сбивание с ног

### B1. Уронило на месте

| Показатель | Ожидание | Факт |
|---|---|---|
| `Footing LOST … t=` | 0.9–1.1 с после команды | ____ |
| `felt` в строке | 19.62–20.1 | ____ |
| `ship turning` | 146–149 | ____ |
| `push` на панели | `(0.0 0.0 -10.0)` | ____ |
| `ship-local` Z | опустился на ~2 см, X/Y стоят | ____ |
| `drift peak` | ~2 см **оранжевым**, слово `KNOCKED DOWN` | ____ |
| `W` лёжа | не ходит | ____ |
| Стрелки лёжа | обзор работает | ____ |
| `E` лёжа | `No seat while knocked down.` | ____ |
| Красные `CARRY …` | нет | ____ |

### B2. Встал

| Показатель | Ожидание | Факт |
|---|---|---|
| `Footing regained … after N s down` | N ≈ времени с `Roll 1` до `Roll 0` + 0.7 | ____ |
| От `Roll 0` до `regained` | ~0.7 с | ____ |
| `W` после | ходит | ____ |

### B3. Волочёт к стене

| Показатель | Ожидание | Факт |
|---|---|---|
| Уронило | раньше, чем в B1 | ____ |
| `push` на панели | около `(0 15.7 -15.7)` | ____ |
| `ship-local` Y | растёт до ~356 и стоит | ____ |
| `drift peak` | >50 см оранжевым, `KNOCKED DOWN` | ____ |
| Красные `CARRY …` | нет | ____ |

### B4. Механика выключена

| Показатель | Ожидание | Факт |
|---|---|---|
| Панель | `footing KNOCKDOWN OFF` жёлтым | ____ |
| `Roll 1` | `felt 29.6` жёлтым, стоишь и ходишь | ____ |

---

## Запуск C — экипаж

| Шаг | Ожидание в журнале | Ожидание на панели | Факт |
|---|---|---|---|
| C1 `E` в центре | `Station 'pilot' taken by player.` | `flown by player` **и** `gunned by automation`, оба зелёным | ____ |
| C1 `→` | `Flight: flown by player` рядом с `Turret: gunned by automation` | `angular (0 0 120)`, турель `slewing`/`CANNOT BEAR` | ____ |
| C2 `R` сидя | — | `felt 29.6` жёлтым, `footing intact (strapped in)` | ____ |
| C2 `E` при крене | `Footing LOST` сразу | `KNOCKED DOWN` | ____ |
| C2 `sf.ConsoleOperator 1` | `taken by console`, затем `Footing regained` | `footing intact` | ____ |
| C3 `sf.AutomationOperator 0` | `Station 'gunner' left by automation.` | `gunned by unoccupied`, `UNCOMMANDED` | ____ |
| C3 `E` у кормы | `Station 'gunner' taken by player.` | `gunned by player (standing down)` оранжевым | ____ |
| C3 `sf.AutomationOperator 1` | `refusing automation`, один раз | без изменений | ____ |
| C3 `E` | `taken by automation` сразу | `gunned by automation` | ____ |

---

## Субъективно

- **Сбивание:** ____ (понятно ли без анимации, что уронило; не пугает ли 2 см на приборе)
- **Вставание:** ____ (0.5 с — мало, много, в самый раз)
- **Турель:** ____ (видно ли, что она следит; помогает ли конус)
- **Экипаж:** ____ (читается ли с панели, что постов два и операторы разные)

---

## Что прогон нашёл

1. ____
2. ____

---

## Решение

- [ ] **Зелёный свет: картинка согласна с числами, сбивание роняет и ставит, не роняя опору; два поста работают одновременно** — слияние в `main`, handoff этапа 3
- [ ] Картинка не согласна с числами — чиним задачи 1/3, повторяем A
- [ ] Сбивание роняет опору или не встаёт — чиним задачу 8, повторяем B
- [ ] Экипаж не работает одновременно — чиним задачи 4/5, повторяем C
- [ ] Прогон не состоялся — чиним стенд, повторяем. Решения о дизайне не принимаем

**Обоснование:** ____
````

- [ ] **Step 3: Коммит**

```bash
git add docs/superpowers/notes/2026-09-09-checkpoint-5-runbook.md docs/superpowers/notes/2026-09-09-checkpoint-5-results.md
git commit -F - <<'MSG'
docs: write the runbook and results form for checkpoint 5

What the headless runs cannot say: whether the picture agrees with the numbers,
what being knocked down feels like, and whether a person at the pilot's post and the
ship's automation at the gunner's work at the same time -- the scene the spec's
amendment describes. The runbook derives every number it asks the operator to compare
against, and says which HUD line to read before which, because the drift figure now
means one of two things and the footing line is what says which.
MSG
```

---

## Точка проверки 5

После задачи 9 — по runbook, руками. Проверить перед слиянием:

- 52 головных теста проходят из консоли, без редактора; `StarForgeCore` по-прежнему зависит только от `Core` и `CoreUObject`; сборка без предупреждений;
- в журнале видно **два поста и двух операторов**, ни один из которых не человек, — `taken by console` и `taken by automation` рядом;
- ствол поворачивается туда, куда говорит индикатор, и против вращения корпуса;
- уроненного **продолжает нести**: ни одной `Carry NOT RUNNING`, `skipped carries 0` во всех журналах с включённой механикой;
- отрицательный контроль с `sf.Knockdown 0` по-прежнему **кричит**: `Carry NOT RUNNING` раз в секунду;
- база точки проверки 1 воспроизводится с `sf.Knockdown 0` и подписана словом `knockdown off`.

Слияние — `git merge --no-ff feat/gunner-station` в `main` после заполненного бланка; затем handoff этапа 3 в `docs/superpowers/notes/`.

## Что дальше

Этап 3 (§8, §13): станция, стыковочный узел, передача системы отсчёта в шлюзе. Требования к нему, зафиксированные здесь:

1. **Станция обязана быть `USFShipFrameComponent`-владельцем, не симулируемым телом.** Компонент теперь выводит скорость и ускорение из разности трансформов именно для того, чтобы кинематический владелец был полноправной системой отсчёта: сбивание с ног на станции обязано работать тем же кодом.
2. **Передача системы отсчёта — это `SetShipFrame` с новым владельцем, и она сбрасывает опору, флаг `bStayBasedInAir` и счётчики.** Уроненный в шлюзе не должен пронести флаг в новый кадр; сброс уже в `SetShipFrame`, и этап 3 обязан вызывать его, а не подменять `ShipFrame` иначе.
3. **Словарь прибора увода не расширять молча.** Слово в строке `Shipboard drift` — одно из четырёх, и каждое привязано к условию; передача кадра, если ей нужно собственное слово, добавляет его в `DescribeFooting` вместе с условием и строкой на HUD.
