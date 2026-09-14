# Точка проверки 7: результаты (бой и пират)

**Дата прогона:**
**Сборка:** StarForgeEditor Win64 Development, UE 5.8.2
**Ветка / коммит:** `feat/combat-and-the-pirate` /
**Карта:** `L_Combat` по карте / подмена режима по URL на `L_FrameSpike` (нужное отметить); строка `Bringing World` из журнала:
**Частота кадров в окне:** (`stat fps`, или `dt` из строк `Drift debug`, если включался `sf.DriftDebug`)

Порядок действий описан в [runbook](2026-09-14-checkpoint-7-runbook.md). Числа берутся из журналов
в `evidence/`, не с экрана.

| Запуск | Журнал |
|---|---|
| A1 — дуэль стоя | `evidence/<дата>-checkpoint-7-A1-duel-standing.log` |
| A1б — дуэль у стены удара (необязательно) | `evidence/<дата>-checkpoint-7-A1b-duel-at-the-wall.log` |
| A2 — бой из кресла | `evidence/<дата>-checkpoint-7-A2-fight-seated.log` |
| B1 — C1, щиты выключены | `evidence/<дата>-checkpoint-7-B1-shields-off.log` |
| B2 — C2, ИИ пирата выключен | `evidence/<дата>-checkpoint-7-B2-pirateai-off.log` |
| B3 — C3, стрельба без цели | `evidence/<дата>-checkpoint-7-B3-no-pirate.log` |
| B4 — C4 днище, C5 борт, C5б второй угол | `evidence/<дата>-checkpoint-7-B4-floor-and-flank.log` |
| B5 — C6, частота кадров (headless) | `evidence/<дата>-checkpoint-7-B5-instrument-{60,400}.log`, `-B5-nosweep-{60,400}.log` |

---

## Что уже проверено без рук (headless, 60 и 400 fps)

Пути ниже временные, как в бланке точки 6: при закрытии этапа контроллер перенесёт журналы из `.superpowers/sdd/2026-09-14-combat-and-the-pirate/evidence/` в `notes/evidence/` и поправит ссылки.

Всё в таблице снято до ручного прогона и взято из отчётов исполнителей задач 3–7 и из их журналов.
Прочерков здесь нет: это измерения, а не ожидания. Ниже `…/evidence/` означает
`.superpowers/sdd/2026-09-14-combat-and-the-pirate/evidence/`.

| Проверка | 60 fps | 400 fps |
|---|---|---|
| Головные тесты | **83 / 0**, `TEST COMPLETE. EXIT CODE: 0` (`…/evidence/task7-r4-tests.log`) | Без мира (`-nullrhi`, `Automation RunTests`): частоты кадров у этого прогона нет |
| Границы модулей | `StarForgeCore.Build.cs`: `PublicDependencyModuleNames` — `Core`, `CoreUObject`. Прочитано 2026-09-14 | Не частотная проверка: тот же файл |
| Точка попадания в станцию, выстрелы в `ON TARGET` | `hull-local (-0.000 -410.000 116.200) cm`, 95 из 95 одинаковы, `WallYNegLintel (ECC_WorldStatic)` (`…/evidence/task3-r1-instrument-60.log`) | `hull-local (0.000 -410.000 116.200) cm`, 95 из 95 (`…/evidence/task3-r1-instrument-400.log`). Расхождение **0.000 см** по каждой оси |
| Выстрелов за 10 с после первого `ON TARGET` | **50**, станция (`…/evidence/task3-r1-instrument-60.log`); **50**, пират по кораблю (`…/evidence/task7-r2-combat-60.log`) | **50** (`…/evidence/task3-r1-instrument-400.log`); **50** (`…/evidence/task7-r2-combat-400.log`) |
| Контроль `sf.ProjectileSweep 0` | **0** попаданий, 100 выстрелов, 76 истекли (`…/evidence/task3-control-nosweep-60.log`) | **0** попаданий, 101 выстрел, 76 истекли (`…/evidence/task3-r1-control-nosweep-400.log`) |
| Потеря одного `+X`: тяга и ускорение | `Thruster 0 on [Ship] destroyed at (2.0 2.0 0.0) m by sf.KillThruster. 23 of 24 left; fore-and-aft thrust 800.0 -> 600.0 kN, 16.0 -> 12.0 m/s^2.` (`…/evidence/task6-rig-a-60.log`) | Та же строка слово в слово (`…/evidence/task6-rig-a-400.log`). Головной тест `LosingOneForwardThrusterCostsExactlyAQuarter`: 800000 → 600000 Н и 16.0 → 12.0 точным равенством |
| Рыскание без ассиста, край ручки (`sf.Intent.X 1`) | **1.0069** °/с², `torque got (0 0 9375) Nm`, крена и тангажа нет (`…/evidence/task6-rig-a-60.log`) | **1.0046** °/с² (`…/evidence/task6-rig-a-400.log`). Расчёт `9375 / 533333` = 1.0072 |
| Рыскание без ассиста, вполгаза (`sf.Intent.X 0.5`) | **0.0** во всех 30 строках, `torque got (0 0 0)` (`…/evidence/task6-rig-b-60.log`) | **0.0** во всех 29 строках (`…/evidence/task6-rig-b-400.log`) |
| Рыскание с ассистом (`sf.Intent.X 1`) | Пик **0.8 °/с** на разгоне, у 191.4 м/с; затем **0.0** при `200.0 m/s` (`…/evidence/task6-rig-c-60.log`) | Пик **1.3 °/с** у 191.9 м/с; затем **0.0** при `200.0 m/s` (`…/evidence/task6-rig-c-400.log`) |
| Головной тест полосы `SingleLossYawsOnlyAboveTheCeiling` | `800 kN: damaged force (600000.000 0.000 0.000) N, torque (0.000 0.000 400000.000) Nm`: 800 кН → 600 кН и **+400 кН·м**; целый массив на 800 кН без момента (`…/evidence/task6-r0b-tests.log`) | Без мира: частоты кадров нет |
| Отказ `Build` при потере охвата | Ровно на **двенадцатом** уничтожении (индекс 11), `LogStarForge: Error` 1, `Combat: … thrusters 12/24 NOT built` (`…/evidence/task6-rig-d-60.log`; после правки раскрутки то же, `…/evidence/task6-r1-rig-d-60.log`) | То же, на двенадцатом, `Error` 1 (`…/evidence/task6-rig-d-400.log`) |
| Перенос раскрутки через `Rebuild`: двигатель 3 уничтожен в полёте | Через 3 кадра `accel 16.0`, `got 800000 N`, `4 spooled`; скорость 79.2 → 95.2 → 111.2 → 127.2 м/с (`…/evidence/task6-r1-kill3-60.log`). До правки было `accel 8.9`, `got 444444 N`, `0 spooled`, 94.9 м/с (`…/evidence/task6-r1-kill3-prefix-60.log`) | `accel 16.0` до и после, `got 800000 N`, `4 spooled`; 79.4 → 95.4 → 111.5 → 127.5 м/с, 0 ошибок (`…/evidence/controller-task6-spool-kill3-400.log`) |
| Уничтоженный набор переживает отстыковку | `ready: 23 thrusters` после `Dock: [Ship] undocked.`, `thrusters 23/24` (`…/evidence/task6-undock-kill-60.log`) | То же (`…/evidence/task6-undock-kill-400.log`) |
| Преследование: вход в 300 ± 50 м | Около **12.3 с** (361.2 м на 12.02 с, 330.6 на 13.02); минимум **300.0 м**; 300.0 с 19.02 до 40.02 с (`…/evidence/task7-r2-combat-60.log`) | Около **12.3 с** (360.1 на 12.03, 329.7 на 13.03); минимум **300.0 м**; 300.0 с 19.05 с (`…/evidence/task7-r2-combat-400.log`) |
| Преследование: пеленг по тангажу < 2° | С **2.02 с**: −8.5 на спавне, +2.2 → −0.5 → +0.1 | С **2.01 с**: +2.4 → −0.6 → +0.2 |
| Грань садится за 10 попаданий | 10-е попадание → `face 0.0/1000.0`, `Shield face +X is down on [Ship].` на **2.967 с** (`…/evidence/task7-r2-combat-60.log`) | На **2.950 с** (`…/evidence/task7-r2-combat-400.log`) |
| Обесточивание | `[Ship] is powered down …` на **6.700 с**; в дуэли оба в одном кадре на 6.700 с (`…/evidence/task7-r4-duel-60.log`). Далее `Combat: … shields +X 0 -X 0 +Y 0 -Y 0 +Z 0 -Z 0, hull 0.0/2000.0, … powered down.` и `Turret: [Ship] gunned by automation (standing down) \| … \| UNCOMMANDED`. Выстрелов после: **0 / 0** | **6.6725 с**; в дуэли оба на 6.6725 с (`…/evidence/task7-r4-duel-400.log`); выстрелов после **0 / 0** |
| Столкновение корпусов в дуэли | Контакт `Footing LOST at t=14.93 s`. Пират 80.9 → 28.9 м/с, корабль 0.0 → 52.0 м/с. `skipped carries 0` в 8 из 8 строк, `Frame handover` 0, `Carry NOT RUNNING` 0, сбит с ног на **0.5 с** (`…/evidence/task7-r4-duel-60.log`). Минимум между центрами 844.7 см при первом касании на 854.9 (`…/evidence/task7-r3-diag-contact-60.log`) | `t=14.88 s`. Пират 81.2 → 29.5, корабль 0.0 → 51.7 м/с. Пропусков 0, передач 0, сбит на **0.5 с** (`…/evidence/task7-r4-duel-400.log`). Минимум 817.6 см (`…/evidence/task7-r3-diag-contact-400.log`) |
| Условие передачи 1.0 м/с: отказ | `Frame handover REFUSED: Ship -> Pirate, … 53.28 m/s …`. Человек у стены удара, `Frame handover` нет, пропусков 0 (`…/evidence/task7-r4-diag-wall-floor-60.log`) | `… 81.19 m/s …`, передачи нет (`…/evidence/task7-r4-diag-board-400.log`). Место снаружи стены: внутри кабины объём пирата не дотягивается ни в одной точке (`…/evidence/task7-r4-diag-reach-400.log`) |
| Условие передачи: стыкованная пара | `relative at man 0.000000 m/s` во всех строках (`…/evidence/task7-r4-diag-docked-60.log`) | `0.000000 m/s` (`…/evidence/task7-r4-diag-docked-400.log`) |
| `sf.PirateAI 0` | **1011.2 м** в 40 строках из 40, скорость пирата `0.0`, пират стреляет: 36 выстрелов до своего обесточивания на 7.033 с (`…/evidence/task7-r1-pirateai0-60.log`; на HEAD `task7-r2-pirateai0-60.log`, 0 различий) | **1011.2 м** в 39 строках из 39, 36 выстрелов до 7.010 с (`…/evidence/task7-r1-pirateai0-400.log`) |
| `sf.Pirate 0` | **75** выстрелов за 15 с, все `turret NO TARGET`; 51 `expired after 5.00 s, 5000.0 m travelled`; 0 попаданий, 0 `Error` (`…/evidence/task7-r1-pirate0-60.log`) | **Не измерено**: прогон сделан только на 60 fps |
| Увод стоя в бою (пират стреляет, корабль игрока стоит) | `30.0 s settled, drift now 0.000 cm … skipped carries 0 … footing intact` (`…/evidence/task7-r2-combat-60.log`) | То же (`…/evidence/task7-r2-combat-400.log`) |
| R11: попадание в панель `ECC_WorldDynamic` | 198 попаданий, все `component WallXPos (ECC_WorldDynamic)` (`…/evidence/task7-r1-combat-60.log`) | 198, все `WallXPos (ECC_WorldDynamic)` (`…/evidence/task7-r1-combat-400.log`) |
| Регрессия: спайк вперёд | 111 = 111 строк `LogStarForge`, **IDENTICAL**; `0.000 cm`, `200.0 m/s` (`…/evidence/task7-r4-spike-fwd-60.log`) | 107 = 107, **IDENTICAL** (`…/evidence/task7-r3-spike-fwd-400.log`) |
| Регрессия: крен на спайке | Строки полёта, увода и готовности массива 28 = 28, идентичны; `ang roll/pitch/yaw (180.0 0.0 0.0)` (`…/evidence/task7-r2-spike-roll-60.log`) | 108 = 108, **IDENTICAL**; `(180.0 0.0 0.0)`, `inertia (533333, 533333, 533333)` (`…/evidence/task7-r3-spike-roll-400.log`) |
| Регрессия: петля | 300 = 300, **IDENTICAL**; `60.0 s settled, drift now 0.000 cm`; `Frame handover: Station -> Ship, first carry moved him 0.000 cm over 5 frame(s)`; `REFUSED` 0 (`…/evidence/task7-r4-loop-60.log`) | 296 = 296, **IDENTICAL**; `over 27 frame(s)`; `REFUSED` 0 (`…/evidence/task7-r4-loop-400.log`) |

**Рыскание после потери одного двигателя — находка о §5 и о метрике решателя, а не
подтверждение §5 как геометрии.** Без двигателя `+X` на угле `(2, 2, 0)` корабль ведёт себя
по-разному в трёх полосах запроса.

- **Ниже сниженного потолка** (до 600 кН) он идёт прямо, только тратит больше газа (1.8333 против
  1.5000).
- **Примерно от 610 до 940 кН** сила упирается в 600 кН, а момент по `Z` растёт как
  `2 · (запрос − 600 кН)`.
- **Выше примерно 950 кН** момент снова нулевой.

Десять проходов вместо трёх меняют знак момента в полосе, но не величину. Значит, дело не в
недосходимости: наименьшие квадраты в смешанных единицах (Н и Н·м) обменивают недостижимую силу на
момент. Ассист проходит полосу при каждом разгоне к заданной скорости. Отсюда пик на разгоне и
ноль на крейсере в строке «с ассистом». Около +1.0 °/с² на краю ручки без ассиста — это второй,
меньший эффект: недосходимость трёх проходов (`595312.5 Н`, `9375 Н·м`). Источники: отчёт
задачи 6, F1 и «Pre-review fix (F1)»; `…/evidence/task6-finding-band-sweep.log`.

**Строка «условие передачи» проверена на 400 fps не там, где сидит человек.** Внутри кабины на
400 fps объём пирата не дотягивается до человека ни в одной точке: самый глубокий угол объёма
лежит на 402.1 см по `x`, выше крыши. Отказ на 81.19 м/с получен на доске снаружи стены. Там
человека в том же прогоне подхватил корпус пирата: 15 строк `Carry NOT RUNNING`. Это цена
придуманного места, а не защиты. Внутри кабины защита подтверждена только на 60 fps.

**`sf.Pirate 0` на 400 fps не прогонялся.** Контроль не полётный: частота кадров меняет в нём
только число секундных строк. Но правило «каждая проверка на 60 и 400» он нарушает, и это
записано.

---

## Сначала — действителен ли прогон

| Условие | Требование | Факт |
|---|---|---|
| Журналы скопированы в `evidence/` до следующего запуска | все запуски из таблицы в шапке | |
| Журналы заканчиваются `Log file closed` | да, каждый | |
| Стартовые строки `Set CVar [[p.Chaos.CCD.EnableThresholdBoundsScale:0.01]]` и `…AllowedDepthBoundsScale:0.005]]` | есть в каждом ручном журнале | |
| Строка `Combat rig:` | `Display` в A1, A1б, A2, B1, B2, B4; `Warning` с `withheld by sf.Pirate 0` в B3; ни одного `Error` | |
| Строк `LogStarForge: Error` | 0 во всех журналах, кроме B4: там 0, а если сделан C5б, одна и больше `Thruster layout of 12 thrusters on [Pirate] cannot span all six axes` | |
| `skipped carries` во всех строках `Shipboard drift` | 0 | |
| `Carry NOT RUNNING` | ни одной | |
| `Frame handover: Ship -> Pirate` / `Pirate -> Ship` без `REFUSED` | ни одной | |
| `measured` в A1 | ≥ 55 с | |

Прогон действителен: **да / нет**

(Про шум: строки `LogPython: Error` из `AllToolsets`, `LogPhysics: Error … CDO construction`,
`LogTemp: Error test: UE::UnifiedErrorTest` и предупреждения окружения есть в каждом прогоне и к
проекту отношения не имеют. Считается только `LogStarForge: Error`.)

---

## Запуск A — бой руками

### A1: дуэль стоя

| Шаг | Ожидание | Факт |
|---|---|---|
| A1.1 стартовые строки, набор, порядок любой | Две `Set CVar` CCD, две `Weapon: … muzzle`, `Console operator on [Pirate] stands down`, `Station 'pilot' on [Pirate] taken by pursuit.`, `Station 'gunner' on [Pirate] taken by automation.`, `Docking: [Ship] no station in this level, …`, `Combat rig: …` на `Display` | |
| A1.1 строка `frame` на панели | Записать дословно. В headless-журналах боевого стенда `Frame handover` нет ни одной | |
| A1.1 строка `combat` на панели | `combat  [Pirate] 1011.2 m   flown by pursuit   hull 2000.0 / 2000.0   faces down 0 of 6   powered up` | |
| A1.2 `Shield face +X is down on [Ship].` и `on [Pirate].`, `t` | Headless: оба 2.967 с (60); 2.950 и 2.942 с (400) | |
| A1.2 колонка `+X` в строке `shields` краснеет | Да, в тот же момент | |
| A1.2 `[Ship] is powered down …` и `[Pirate] is powered down …`, `t` | Headless: оба в одном кадре, 6.700 с (60) / 6.6725 с (400) | |
| A1.2 выстрелов после обесточивания, корабль / пират | 0 / 0 | |
| A1.2 панель в момент обесточивания | Шесть красных колонок `shields`; `hull  0.0 / 2000.0   powered down` оранжевым; `flown by console (standing down)` оранжевым; `turret … UNCOMMANDED` жёлтым; `combat … powered down` оранжевым | |
| A1.2 `Footing LOST at t=…`, строка дословно | Headless: 14.93 с (60) / 14.88 с (400) | |
| A1.2 скорость пирата и корабля до → после, из строк `Flight:` | Headless: 80.9 → 28.9 и 0.0 → 52.0 м/с (60); 81.2 → 29.5 и 0.0 → 51.7 (400) | |
| A1.2 `Footing regained at t=… after N s down, ship-local (…)`, строка дословно | Headless: 0.5 с, у задней стены `−X`: `(-355 3 -302)` (60) / `(-355 6 -296)` (400) | |
| A1.2 вынос в проём люка (стена `+Y`) | Нет. Headless: `y` не больше 6.2 см | |
| A1.2 `Frame handover REFUSED` в центре каюты | Нет. Headless: 0 | |
| A1.3 строка `Shipboard drift` после `measured` ≥ 55 с, дословно | Около 355 см, `skipped carries 0`, `KNOCKED DOWN: the drift is his slide, not the carry` | |

### A1б: дуэль у стены удара (необязательно)

| Шаг | Ожидание | Факт |
|---|---|---|
| Выполнялся | да / нет | |
| A1б.1 место, где стоял, `ship-local` | У шара, стена `+X`, около `(350 0 -300)` | |
| A1б.2 `Frame handover REFUSED: Ship -> Pirate, … N m/s …`, дословно | Может быть, а может не быть. Headless: 53.28 м/с на 60 fps; на 400 объём до кабины не дотянулся | |
| A1б.2 `Frame handover: Ship -> Pirate` без `REFUSED` | **Ни одной** | |
| A1б.2 `skipped carries` | 0 | |

### A2: бой из кресла

| Шаг | Ожидание | Факт |
|---|---|---|
| A2.1 посадка | `Station 'pilot' on [Ship] taken by player.`, `flown by player` зелёным | |
| A2.2 `t` первой команды `sf.Fire 1` (по первому `Hit on [Pirate]`) | Записать как есть | |
| A2.3 грани в строках `Hit on [Pirate]` | Преследующий пират держит нос на тебе, поэтому ожидается `+X` у середины. Записать все грани, которые встретились | |
| A2.3 грани в строках `Hit on [Ship]` | Записать как есть | |
| A2.3 кто обесточен первым, `t` | Не измерено: записать | |
| A2.3 строки `Thruster N on [...] destroyed …` | Записать дословно, если были | |
| A2.4 рыскание своего корабля после потери двигателя | Только кратко на разгоне с ассистом; 0 на крейсере | |
| A2.5 обесточенный пират: скорость из `Flight: [Pirate]`, куда идёт | Записать как есть | |
| A2.6 обесточен ты: пират продолжает попадать | `Hit on [Ship] … 100.0 to hull; face 0.0/1000.0, hull 0.0/2000.0.`; `Pursuit: … hold 300 m` | |
| A2.7 уход на скорости: наибольшее расстояние в `Pursuit:` и скорость корабля | Отставание до `Hold + v²/16`; при 200 м/с 2.8 км | |

**Панель нарисовалась, и все строки читаемы (ни одного сырого `%.1f` / `%s`):** да / нет

**Строки панели, которые рисовались впервые** (красная грань, оранжевые `hull … powered down` и
`combat … powered down`, `faces down N of 6` при N > 0), и видны ли они:

---

## Запуск B — отрицательные контроли

Контроль засчитан, только если строка стоп-ворот переписана **дословно из журнала**.

| Контроль | Что сделано | Строка стоп-ворот, дословно | Засчитан | Замечания |
|---|---|---|---|---|
| **C1.** Щиты выключены (B1) | `-ExecCmds="sf.FlightLog 1, sf.Shields 0, sf.Fire 1"`, стоять 10 с | 1) `Hit on [Pirate] +X face: shields OFF (sf.Shields 0), 100.0 straight to hull.`<br>2) две соседние строки `Combat: [Pirate]`, где `hull` убывает: | да / нет | |
| **C2.** ИИ пирата выключен (B2) | `-ExecCmds="sf.FlightLog 1, sf.PirateAI 0"`, стоять 25 с | 1) `Pursuit: [Pirate] sf.PirateAI 0 -- the pursuit operator declines the pilot's post; nobody flies this ship and it coasts.`<br>2) первая и двадцатая строки `Pursuit: … at N m … declining (sf.PirateAI 0)`:<br>3) одна строка `Hit on [Ship]` из тех же секунд: | да / нет | |
| **C3.** Стрельба без цели (B3) | `"-ini:Engine:[ConsoleVariables]:sf.Pirate=0"`, `-ExecCmds="sf.FlightLog 1, sf.WeaponDebug 1, sf.Fire 1"`, стоять 10 с | 1) `Combat rig: … no pirate, withheld by sf.Pirate 0, …` (`Warning`)<br>2) `Weapon: [Ship] shot … turret NO TARGET …`<br>3) `Projectile: [Ship] shot … expired after 5.00 s, 5000.0 m travelled.` | да / нет | |
| **C4.** Днище (B4.2–B4.5) | `-ExecCmds="sf.FlightLog 1, sf.PirateAI 0, sf.ConsoleOperator 0"`; вниз, вперёд; `bearing … pitch` на панели = ___; `sf.Fire 1` | 1) первая строка `Hit on [Pirate] -Z face …` (или `+Z`)<br>2) строка `[Pirate] is powered down: …`<br>3) число строк `Thruster N on [Pirate] destroyed` между ними (обязано быть **0**): | да / нет | |
| **C5.** Борт (B4.6–B4.8) | `sf.Fire 0`; вровень, `bearing yaw` на панели = ___, `bearing pitch` = ___; `sf.Fire 1` на 2 с | 1) `Thruster N on [Pirate] destroyed at (…) m by a hit D m away. …`, `D ≤ 2.50`<br>2) следующая за ней строка `Hit on [Pirate] … face …` | да / нет | Число строк уничтожения (ожидается 6, одно `at (…)`): |
| **C5б.** Второй угол (B4.9, необязательно) | Другая сторона, `bearing yaw` = ___ | 1) `Thruster layout of 12 thrusters on [Pirate] cannot span all six axes; …` (`Error`)<br>2) `Thruster N on [Pirate] … 12 of 24 left, and the array is NOT built over them -- the ship will not fly.`<br>3) `Warning: Flight: [Pirate] thruster array NOT built -- no flight numbers to report.` | да / нет / не выполнялся | |
| **C6.** Частота кадров (B5, headless) | Прибор задачи 3 на 60 и 400, контроль `sf.ProjectileSweep 0` на 60 и 400 | 1) `uniq -c` точки попадания, 60:<br>2) то же, 400:<br>3) выстрелов в окне `[1.0, 11.0)`, 60 / 400:<br>4) `grep -c "Projectile: .* hit "` в контроле, 60 / 400: | да / нет | Расхождение точек по каждой оси (≤ 1.0 см): |

**Строка `Pursuit: [Pirate]` журнала B4 против панели.** В разделе B4 ранбука выведено, что
пеленг панели равен пеленгу пирата: `yaw` тот же, `pitch` с обратным знаком. Сверить на моменте
перед каждым `sf.Fire 1`:

| Момент | Панель `turret … bearing yaw / pitch` | Журнал `Pursuit: … bearing yaw / pitch` | Сходится |
|---|---|---|---|
| Перед C4 | | | |
| Перед C5 | | | |

**Прибор не слеп:** да / нет — C1, C2, C3 кричат; C4 молчит там, где обязан молчать, а C5 говорит
там, где обязан говорить; C6 совпадает, а контроль промахивается

---

## Глазами

| Вопрос | Ответ |
|---|---|
| Понятно ли с панели, в какую грань пирата пришло попадание? (Панель пишет только `faces down N of 6`; ожидание — нет, только из журнала) | |
| Понятно ли по колонкам `shields`, с какой стороны стреляют по тебе | |
| Читается ли своё обесточивание как событие, а не как «игра зависла» | |
| Читается ли обесточивание пирата по строке `combat` | |
| Понятно ли, кто стреляет и кто попадает | |
| Как выглядит удар корпусов для стоящего (A1) | |
| Отскок от стены: заметен ли, куда вынесло | |
| Хватило ли строки `turret … bearing`, чтобы занять позицию C4 и C5 | |
| Ощущается ли бой боем | |
| Укачивание | |

---

## Долги, которые этот прогон закрывает или переносит

| Долг | Состояние |
|---|---|
| Геймпада нет; аналоговый путь по-прежнему не наблюдался | |
| §6 «волочёт по палубе» помечена неточной | |
| Люк сделал каюту незакрытой (R12 этапа 3): сбитого с ног может вынести наружу. Этап 4 добавил способ сбить с ног (удар корпусов) и отскок от стены почти без потери скорости | |
| Человек не может занять пост канонира: `ASFCharacter` не реализует `ProvideTurretIntent`, спуск только консольной переменной | |
| При потере охвата корабль громко отказывается лететь; полёт массива ранга 5 показал бы «ведёт в сторону» из §5. Выбор между отказом и полётом с деградацией оставлен владельцу проекта для этапа 5 | |
| Защита передачи внутри кабины на 400 fps не проверена: объём пирата туда не заходит | |
| `sf.Pirate 0` снят только на 60 fps | |
| C4 в плане («150 м над пиратом на 300 м», 26.6°) приводит в грань `+X`, а не `+Z`; ранбук ведёт C4 снизу, под углом 45–60° | |

---

## Что прогон нашёл

1.

---

## Решение

- [ ] **Зелёный свет: контроли кричат и молчат там, где обязаны, переносу не помешал ни удар, ни чужой объём, панель читается** — слияние `git merge --no-ff feat/combat-and-the-pirate` в `main`, затем передача этапа 5
- [ ] C1 или C2 молчит — прибор слеп: чинить задачу 5 (`sf.Shields`) или 7 (`sf.PirateAI`), повторять B1 или B2
- [ ] C3: оружие без цели не стреляет — чинить задачу 3, повторять B3
- [ ] C4 уничтожает двигатель или C5 в окне не уничтожает — чинить задачу 6, повторять B4
- [ ] C6: точки расходятся больше чем на 1.0 см или контроль попадает — чинить задачу 3, повторять B5
- [ ] `Frame handover` без `REFUSED`, `skipped carries` больше нуля или `Carry NOT RUNNING` — чинить задачу 7, повторять A1 и A1б
- [ ] Панель не рисуется или показывает сырой формат — чинить задачи 5 и 7 (панель), повторять A1
- [ ] Прогон не состоялся — чинить стенд, повторять. Решений о дизайне не принимаем

**Обоснование:**
