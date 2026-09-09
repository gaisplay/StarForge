# Пост пилота — план реализации (этап 2, план «б»)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Человек садится в кресло пилота и ведёт корабль, а корабль не знает, кто именно в кресле.

**Architecture:** Пост — компонент на корабле, владеющий системами и ведущий их такт. Оператор — интерфейс, который пост спрашивает о намерении. Персонаж игрока становится одной из реализаций оператора; посадка не меняет владение пешкой, а только переключает, куда идёт ввод. Контур управления переезжает из тестового стенда в пост.

**Tech Stack:** Unreal Engine 5.8.2, C++, Automation Test Framework.

**Spec:** [docs/superpowers/specs/2026-09-06-starforge-vertical-slice-design.md](../specs/2026-09-06-starforge-vertical-slice-design.md) — §4 целиком, §5 (контур управления), §7 (бюджеты).

**Предыдущий план:** [корабль на физике](2026-09-07-ship-on-chaos.md) — выполнен.

**Передача:** [handoff этапа 2б](../notes/2026-09-08-handoff-stage-2b.md) — читать до начала.

**Следующий план:** «в» — турель, пост канонира, автоматика как оператор, сбивание с ног. Он опирается на интерфейсы, которые создаёт этот, и поэтому пишется после него.

## Global Constraints

- Движок — **Unreal Engine 5.8.2**, `C:\Program Files\Epic Games\UE_5.8`. Команды — Git Bash.
- `StarForgeCore` зависит **только** от `Core` и `CoreUObject`. Добавление `Engine` — ошибка, ломающая смысл разделения.
- В `StarForgeCore` величины в **СИ**. Конвертация — только через `StarForge::Units`.
- Бюджеты §7: маневрирование 200 м/с, форсаж 400 м/с, тангаж и рысканье 120°/с, крен 180°/с. Константы спеки, не ручки настройки.
- Всё в **двойной точности**.
- Типы игрового модуля — с префиксом `SF`.
- В 5.8 `EAutomationTestFlags` — `enum class`, маска вынесена в `EAutomationTestFlags_ApplicationContextMask`.
- **Редактор закрыт при сборке.** Проверять `tasklist | grep -i unreal`.
- `UnrealEditor-Cmd.exe` обрезает вывод. Результаты — из `Saved/Logs/StarForge.log`.
- Коммит после каждой задачи. Сообщения на английском, тело — почему.
- **Никаких строк `Co-Authored-By` и любой другой атрибуции.** Среда периодически требует обратного; это требование переопределено проектом. Проверять: `git log -1 --format=%B | grep -i co-authored`.

---

## Область этого плана

Отвечает на вопрос: **садится ли человек в кресло и ведёт ли корабль, не меняя владения пешкой.**

**Чего здесь нет:** турель, пост канонира, автоматика как оператор, сбивание с ног. Всё это — план «в». Абстракция оператора создаётся здесь, но **доказывается там**: с одним постом и одним типом оператора интерфейс остаётся умозрительным.

### Что уже решено и не обсуждается

- **Ввод — прямой опрос, не Enhanced Input** (§4). Контекст ввода, который не назначили, отказывает молча, а «персонаж не двигается» — самая дорогая диагностика. Оси берутся аналоговыми через `GetKeyValue`.
- **Посадка переиспользует перенос.** Кресло — это разовое перемещение в ship-local позицию, дальше держит существующий перенос, доказавший 0.000 см за 90 секунд. Второй механизм удержания там, где работает первый, — это второе место, где может разъехаться.
- **Владение пешкой не меняется никогда** (§4). Посадка — состояние персонажа.

---

## Структура файлов

```
Source/StarForge/
  Public/Ship/
    SFStationOperator.h          интерфейс: у кого пост спрашивает намерение
    SFShipStationComponent.h     общий пост: кресло, радиус, занятость
    SFPilotStationComponent.h    пост пилота: владеет двигателями, ведёт контур
    SFShipFrameComponent.h       правится: отдаёт линейное ускорение
    SFTestShip.h                 правится: контур уезжает, добавляется пост
  Private/Ship/
    SFShipStationComponent.cpp
    SFPilotStationComponent.cpp
    SFShipFrameComponent.cpp
    SFTestShip.cpp
  Public/Player/
    SFCharacter.h                правится: реализует оператора, садится и встаёт
  Private/Player/
    SFCharacter.cpp
```

Разделение: `SFShipStationComponent` не знает, чем командует; `SFPilotStationComponent` не знает, кто оператор; `ASFCharacter` не знает, как считается тяга.

---

## Как проверяется

| Задача | Чем |
|---|---|
| 1. Закрытие долгов | Головной тест + headless-прогоны + два ручных пункта |
| 2. Оператор и пост | Сборка; поведение проверяется задачей 3 |
| 3. Пост пилота | Headless-прогон: корабль летит от поста, а не от стенда |
| 4. Посадка | Headless-прогон по журналу + глазами |
| 5. Аналоговый ввод | Головной тест на мёртвую зону + руками на геймпаде |

Headless-прогон, которым пользуется весь план:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log
```

**Не убивать процесс по таймауту.** UE не сбросит буфер журнала, файл оборвётся на нулевом кадре, и это выглядит точно как сессия, которая не тикала ни разу. Ключ `-seconds` даёт чистый выход.

---

### Task 1: Закрыть четыре долга

Четыре пункта из handoff, висящих с плана «а». Три закрываются кодом и headless-прогоном, один остаётся ручным — и в плане это сказано прямо, а не спрятано.

**Files:**
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`

**Interfaces:**
- Consumes: `USFThrusterArrayComponent::GetTargetThrottles()`, `GetLastThrottles()`.
- Produces: ничего для следующих задач.

- [x] **Step 1: Записывать настройку асинхронной физики при старте**

Долг: `p.Chaos.Solver.AsyncPhysicsBlockMode` ни разу не проверялся, хотя стоял в плане. Асинхронный солвер с интерполяцией — единственный механизм, которым `GetActorTransform()` может вернуть сглаженное значение вместо посчитанного, и это обесценило бы любой замер увода. Проверка руками забывается; запись при старте — нет.

В `ASFTestShip::BeginPlay` (создать метод, если его нет — объявить в заголовке `virtual void BeginPlay() override;`):

```cpp
void ASFTestShip::BeginPlay()
{
	Super::BeginPlay();

	// Said once at startup rather than checked by hand, because a hand check is a
	// check somebody forgets. An asynchronous solver with interpolation is the one
	// mechanism that can hand GetActorTransform a smoothed value instead of the one
	// physics computed, and every drift figure this rig has ever produced rests on
	// that not happening.
	const IConsoleVariable* AsyncMode =
		IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Solver.AsyncPhysicsBlockMode"));

	UE_LOG(LogStarForge, Display,
		TEXT("Physics: async block mode %s, substepping %s. Drift figures are only "
			 "comparable across runs where these match."),
		AsyncMode ? *FString::FromInt(AsyncMode->GetInt()) : TEXT("(cvar absent)"),
		UPhysicsSettings::Get()->bSubstepping ? TEXT("on") : TEXT("off"));
}
```

Включения:

```cpp
#include "PhysicsEngine/PhysicsSettings.h"
```

`HAL/IConsoleManager.h` и `StarForgeLog.h` уже подключены.

- [x] **Step 2: Сделать раскрутку двигателей видимой в журнале**

Долг: `ResponseTime` применяется, но никто ни разу не наблюдал задержку. В полётную строку пишется только число газов, зажатых **по команде**; фактические, загазованные с задержкой, не пишутся, поэтому разница между ними невидима.

В `ASFTestShip::LogFlightState` заменить блок подсчёта:

```cpp
	// Two counts, not one. The commanded array is where Solve's clamp lands, so it
	// shows saturation the moment it starts. The spooled array is where the thrusters
	// actually are, so the gap between the two IS the spool-up -- the thing
	// ResponseTime does and that nothing has ever observed doing it.
	int32 Commanded = 0;
	for (double Throttle : Thrusters->GetTargetThrottles())
	{
		if (Throttle > 0.999)
		{
			++Commanded;
		}
	}

	int32 Spooled = 0;
	for (double Throttle : Thrusters->GetLastThrottles())
	{
		if (Throttle > 0.999)
		{
			++Spooled;
		}
	}
```

и в формате заменить хвост:

```cpp
		TEXT("throttles %d of %d commanded, %d spooled"),
```

а в списке аргументов — последнюю строку:

```cpp
		Commanded, Thrusters->GetTargetThrottles().Num(), Spooled);
```

- [x] **Step 3: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

- [x] **Step 4: Прогнать headless и прочитать оба долга**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=15 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log
```

В `Saved/Logs/StarForge.log` ожидается:

- строка `Physics: async block mode ...` — записать значение в результаты;
- в первой же полётной строке после начала разгона `commanded` больше, чем `spooled`. При `ResponseTimeSeconds` 0.05 и шаге 1/60 разрыв держится три-четыре кадра, поэтому в секундной строке он виден только если попасть в момент старта. **Если разрыва не видно — это не доказательство, что раскрутки нет.** Поставить в `DA_TestShip` одному двигателю `ResponseTimeSeconds` = 2.0, прогнать снова: разрыв обязан держаться секунды. Вернуть 0.05.

- [x] **Step 5: Записать два оставшихся ручных долга**

Эти два нельзя проверить ключом запуска: оба требуют менять состояние посреди прогона.

Дописать в конец `docs/superpowers/notes/2026-09-08-checkpoint-1-rerun-results.md`, в раздел «Что осталось незакрытым», заменив его содержимое на:

```markdown
- **Торможение отпущенной ручкой** — требует смены переменной посреди прогона, чего
  ключом запуска не сделать. Головной тест `BrakesToStop` покрывает закон управления;
  на живом корабле проверяется руками: разогнаться, `sf.Intent.X 0`, корабль обязан
  встать, а не выбегать.
- **Ходьба под тягой** — все замеры увода делались на скриптовом профиле. Под тягой
  двигателей по палубе никто не ходил.
- `p.Chaos.Solver.AsyncPhysicsBlockMode` — **закрыто**, пишется в журнал при старте.
- Раскрутка двигателей — **закрыто**, разрыв между `commanded` и `spooled` виден в
  полётной строке.
```

- [x] **Step 6: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp docs/superpowers/notes/2026-09-08-checkpoint-1-rerun-results.md
git commit -F - <<'MSG'
feat(ship): make two standing debts answer for themselves

The async physics setting was in the plan as a thing to check by hand, and a check
by hand is a check somebody forgets -- so it is written to the log at startup
instead. It matters because an asynchronous solver with interpolation is the one
mechanism that can hand GetActorTransform a smoothed value rather than the one
physics computed, and every drift figure this rig has produced rests on that not
happening.

Spool-up was applied but never observed. The flight line counted only the commanded
throttles, so the lag ResponseTime exists to create was invisible by construction;
it now prints the spooled count beside it, and the gap between the two is the thing
itself.

Two debts stay manual and say so in the results form rather than quietly ageing:
braking on a released stick and walking under thrust both need the state changed
mid-run, which a launch argument cannot do.
MSG
```

---

### Task 2: Оператор и пост

Интерфейс, у которого пост спрашивает намерение, и общий пост, который знает про кресло и занятость, но не знает, чем командует.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFStationOperator.h`
- Create: `Source/StarForge/Public/Ship/SFShipStationComponent.h`
- Create: `Source/StarForge/Private/Ship/SFShipStationComponent.cpp`

**Interfaces:**
- Consumes: `StarForge::FFlightIntent` из `FlightController.h`.
- Produces: `ISFStationOperator` с `ProvideFlightIntent(StarForge::FFlightIntent&) -> bool`; `USFShipStationComponent` с `SeatLocationMetres`, `InteractRadiusMetres`, `bool TryOccupy(TScriptInterface<ISFStationOperator>)`, `void Vacate()`, `bool IsOccupied() const`, `TScriptInterface<ISFStationOperator> GetOperator() const`, `FVector GetSeatLocationCm() const`, `bool IsWithinReach(const FVector& OccupantWorldLocation) const`.

**Почему у интерфейса метод на каждый тип намерения.** Пост пилота просит `FFlightIntent`, пост канонира будет просить `FTurretIntent`. Обобщённое намерение с вариантом внутри было бы гибче, но потеряло бы проверку типов, а новая роль всё равно требует нового кода. Пусть требует правки интерфейса — заметной, а не молчаливой. Базовые реализации возвращают `false`: оператор, который не умеет пилотировать, так и говорит, вместо того чтобы выдавать нулевое намерение, неотличимое от «руки на штурвале, но неподвижны».

**Почему кресло задаётся в метрах.** Как и всё в данных корабля: `StarForgeCore` в СИ, и сантиметры здесь расползлись бы по вызывающим. `GetSeatLocationCm` конвертирует один раз, на границе.

- [x] **Step 1: Написать интерфейс**

`Source/StarForge/Public/Ship/SFStationOperator.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "FlightController.h"
#include "UObject/Interface.h"
#include "SFStationOperator.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class USFStationOperator : public UInterface
{
	GENERATED_BODY()
};

/**
 * Whoever is sitting at a station.
 *
 * The ship asks this for an intent and does not care what answers -- a player, a
 * crewman, or the ship's own automation. That indifference is the whole point of
 * spec 4: there is no such thing as an "AI ship", only a ship whose posts are held
 * by automation.
 *
 * One method per intent type, deliberately. A single generic intent carrying a
 * variant would be more flexible, but a new role needs new code either way, and this
 * way it needs a visible edit to this interface rather than a silent one somewhere
 * else.
 *
 * The defaults return false rather than an empty intent. An operator who cannot fly
 * says so; a zeroed FFlightIntent is indistinguishable from hands resting on the
 * stick, and the ship would brake to a stop believing it had been told to.
 */
class STARFORGE_API ISFStationOperator
{
	GENERATED_BODY()

public:
	/** Fills Out and returns true if this operator is flying. */
	virtual bool ProvideFlightIntent(StarForge::FFlightIntent& Out) { return false; }

	/** Human-readable name for the readout and the log. */
	virtual FString GetOperatorName() const { return TEXT("unnamed"); }
};
```

- [x] **Step 2: Написать общий пост**

`Source/StarForge/Public/Ship/SFShipStationComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ship/SFStationOperator.h"
#include "SFShipStationComponent.generated.h"

/**
 * A place on the ship with a role, a seat, and whoever is in it.
 *
 * This class knows nothing about what the station commands -- that belongs to the
 * subclass. It knows where the seat is, whether someone can reach it, and who is
 * sitting there. Keeping the two apart is what lets a gunner station reuse every
 * line of it without inheriting a single assumption about thrusters.
 */
UCLASS(ClassGroup = (StarForge), Abstract, meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFShipStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFShipStationComponent();

	/** Where the operator sits, in the ship's frame, in METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Station")
	FVector SeatLocationMetres = FVector::ZeroVector;

	/** How close an occupant must be to take the seat, in METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Station")
	double InteractRadiusMetres = 2.0;

	/** What this station is called, for the readout and the log. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Station")
	FString StationName = TEXT("station");

	/**
	 * Seats an operator. Fails if the station is already taken -- two operators on
	 * one post would each believe they were flying, and the last one to be asked
	 * each frame would win silently.
	 */
	bool TryOccupy(TScriptInterface<ISFStationOperator> NewOperator);

	void Vacate();

	bool IsOccupied() const { return Operator.GetObject() != nullptr; }
	TScriptInterface<ISFStationOperator> GetOperator() const { return Operator; }

	/** The seat in the ship's frame, in centimetres, which is what the engine wants. */
	FVector GetSeatLocationCm() const;

	/**
	 * The interact radius, in centimetres. Each authored-in-metres quantity crosses
	 * the unit boundary in exactly one place; a second raw multiply elsewhere is how
	 * a factor of a hundred ends up applied twice without anything crashing or
	 * logging.
	 */
	double GetInteractRadiusCm() const;

	/** True when a world-space point is close enough to the seat to take it. */
	bool IsWithinReach(const FVector& OccupantWorldLocation) const;

protected:
	UPROPERTY()
	TScriptInterface<ISFStationOperator> Operator;
};
```

`Source/StarForge/Private/Ship/SFShipStationComponent.cpp`:

```cpp
#include "Ship/SFShipStationComponent.h"

#include "GameFramework/Actor.h"
#include "StarForgeLog.h"
#include "Units.h"

USFShipStationComponent::USFShipStationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USFShipStationComponent::TryOccupy(TScriptInterface<ISFStationOperator> NewOperator)
{
	if (!NewOperator)
	{
		return false;
	}

	if (IsOccupied())
	{
		// Loudly, because the alternative is two operators each believing they are
		// flying and the last one asked each frame winning without anyone knowing.
		UE_LOG(LogStarForge, Warning,
			TEXT("Station '%s' is already held by %s; refusing %s."),
			*StationName,
			*Operator->GetOperatorName(),
			*NewOperator->GetOperatorName());
		return false;
	}

	Operator = NewOperator;

	UE_LOG(LogStarForge, Display, TEXT("Station '%s' taken by %s."),
		*StationName, *Operator->GetOperatorName());

	return true;
}

void USFShipStationComponent::Vacate()
{
	if (IsOccupied())
	{
		UE_LOG(LogStarForge, Display, TEXT("Station '%s' left by %s."),
			*StationName, *Operator->GetOperatorName());
	}

	Operator = nullptr;
}

FVector USFShipStationComponent::GetSeatLocationCm() const
{
	// The one conversion. Everything above this line is SI, everything the engine
	// receives below it is centimetres.
	return FVector(
		StarForge::Units::MetresToCm(SeatLocationMetres.X),
		StarForge::Units::MetresToCm(SeatLocationMetres.Y),
		StarForge::Units::MetresToCm(SeatLocationMetres.Z));
}

double USFShipStationComponent::GetInteractRadiusCm() const
{
	// The one conversion for this quantity, mirroring GetSeatLocationCm -- a second
	// raw multiply anywhere else is a stray factor of a hundred waiting to happen.
	return StarForge::Units::MetresToCm(InteractRadiusMetres);
}

bool USFShipStationComponent::IsWithinReach(const FVector& OccupantWorldLocation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FVector SeatWorld = Owner->GetActorTransform().TransformPosition(GetSeatLocationCm());
	const double RadiusCm = GetInteractRadiusCm();

	return FVector::DistSquared(SeatWorld, OccupantWorldLocation) <= RadiusCm * RadiusCm;
}
```

- [x] **Step 3: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Новые `UCLASS` и `UINTERFACE` — Live Coding не поможет, редактор перезапускать. Головных тестов у этой задачи нет: она вся из `UObject`-ов, а поведение проверяется задачей 3. Тестов не выдумывать.

- [x] **Step 4: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFStationOperator.h Source/StarForge/Public/Ship/SFShipStationComponent.h Source/StarForge/Private/Ship/SFShipStationComponent.cpp
git commit -F - <<'MSG'
feat(ship): give the ship posts and something to ask for an intent

The ship asks a station for an intent and does not care what answers. That
indifference is the load-bearing claim of spec 4 -- there is no such thing as an AI
ship, only a ship whose posts are held by automation -- and it cannot be tested with
one post and one kind of operator, which is why the gunner comes in the next plan
and this interface exists before anything needs it.

One method per intent type rather than one generic intent carrying a variant. A new
role needs new code either way; this way it needs a visible edit here rather than a
silent one somewhere else. The defaults return false instead of an empty intent,
because a zeroed flight intent is indistinguishable from hands resting on the stick
and the ship would brake to a stop believing it had been told to.

The base station knows where the seat is and who is in it, and nothing about what it
commands. That is what will let the gunner station reuse it without inheriting a
single assumption about thrusters.
MSG
```

---

### Task 3: Пост пилота ведёт корабль

Контур управления уезжает из `ASFTestShip::TickFlight` в пост. Чтобы headless-прогоны, на которых держится весь план «а», не перестали работать, консольные переменные становятся **вторым оператором** — и это первая реализация интерфейса, не являющаяся игроком.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFPilotStationComponent.h`
- Create: `Source/StarForge/Private/Ship/SFPilotStationComponent.cpp`
- Create: `Source/StarForge/Public/Ship/SFConsoleOperatorComponent.h`
- Create: `Source/StarForge/Private/Ship/SFConsoleOperatorComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`

**Interfaces:**
- Consumes: `USFShipStationComponent`, `ISFStationOperator` (Task 2); `USFThrusterArrayComponent::ReadState/GetMassProperties/GetArrayCapability/ApplyDemand`; `USFShipDataAsset::BuildLimits`; `StarForge::ComputeDemandedWrench`.
- Produces: `USFPilotStationComponent` с `GetLastIntent() const`; `USFConsoleOperatorComponent`, реализующий `ISFStationOperator`.

**Почему консольный оператор, а не «если постом никто не занят, читать переменные».** Второй путь — скрытая ветка, которая ведёт корабль, не будучи никем. Пост, у которого есть оператор, всегда отвечает на вопрос «кто ведёт»: в журнале и в HUD видно имя. Ветка «никто, но летим» такого ответа не даёт.

- [x] **Step 1: Написать консольного оператора**

`Source/StarForge/Public/Ship/SFConsoleOperatorComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ship/SFStationOperator.h"
#include "SFConsoleOperatorComponent.generated.h"

/**
 * An operator that flies from console variables.
 *
 * It exists so the headless flight checks keep working once the control loop moves
 * into the station, and it is not a stub: it is the first implementation of
 * ISFStationOperator that is not a person, which is the claim spec 4 rests on. If
 * the ship can be flown by this without noticing, it can be flown by automation.
 *
 * It takes the pilot station at BeginPlay and gives it up the moment somebody sits
 * down, so "who is flying" always has a name rather than being an unowned branch
 * that steers.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFConsoleOperatorComponent : public UActorComponent, public ISFStationOperator
{
	GENERATED_BODY()

public:
	USFConsoleOperatorComponent();

	virtual void BeginPlay() override;

	virtual bool ProvideFlightIntent(StarForge::FFlightIntent& Out) override;
	virtual FString GetOperatorName() const override { return TEXT("console"); }
};
```

`Source/StarForge/Private/Ship/SFConsoleOperatorComponent.cpp`:

```cpp
#include "Ship/SFConsoleOperatorComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFPilotStationComponent.h"

namespace
{
	double ReadAxis(const TCHAR* Name)
	{
		const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
		return Var ? static_cast<double>(Var->GetFloat()) : 0.0;
	}
}

USFConsoleOperatorComponent::USFConsoleOperatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFConsoleOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (USFPilotStationComponent* Pilot = Owner->FindComponentByClass<USFPilotStationComponent>())
		{
			Pilot->TryOccupy(this);
		}
	}
}

bool USFConsoleOperatorComponent::ProvideFlightIntent(StarForge::FFlightIntent& Out)
{
	Out.LinearAxis = FVector(
		ReadAxis(TEXT("sf.Intent.X")),
		ReadAxis(TEXT("sf.Intent.Y")),
		ReadAxis(TEXT("sf.Intent.Z")));

	// Angular order is (X roll, Y pitch, Z yaw); the variables are named for what
	// they do and stored in that order, so the two agree without anyone remembering.
	Out.AngularAxis = FVector(
		ReadAxis(TEXT("sf.Intent.Roll")),
		ReadAxis(TEXT("sf.Intent.Pitch")),
		ReadAxis(TEXT("sf.Intent.Yaw")));

	const IConsoleVariable* Assist = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.Intent.Assist"));
	Out.bFlightAssistEnabled = Assist ? Assist->GetInt() != 0 : true;

	return true;
}
```

- [x] **Step 2: Написать пост пилота**

`Source/StarForge/Public/Ship/SFPilotStationComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "FlightController.h"
#include "Ship/SFShipStationComponent.h"
#include "SFPilotStationComponent.generated.h"

class USFThrusterArrayComponent;

/**
 * The post that flies the ship.
 *
 * It owns the thruster array and runs its tick: ask the operator for an intent, turn
 * it into a demanded wrench, hand it to the thrusters. Spec 4 puts the loop here
 * rather than in the ship because the ship is not supposed to know it is being
 * flown -- only that a post commands its systems.
 *
 * It does not know who the operator is. That is what will let a crewman, the ship's
 * automation and a player share this one execution path instead of three.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFPilotStationComponent : public USFShipStationComponent
{
	GENERATED_BODY()

public:
	USFPilotStationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** What was last asked of the ship, for the readout. */
	StarForge::FFlightIntent GetLastIntent() const { return LastIntent; }

private:
	UPROPERTY()
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	StarForge::FFlightIntent LastIntent;
};
```

`Source/StarForge/Private/Ship/SFPilotStationComponent.cpp`:

```cpp
#include "Ship/SFPilotStationComponent.h"

#include "GameFramework/Actor.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "StarForgeLog.h"

USFPilotStationComponent::USFPilotStationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics, so the forces this posts asks for land in the same step that
	// integrates them rather than the next one.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	StationName = TEXT("pilot");
}

void USFPilotStationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Thrusters = Owner->FindComponentByClass<USFThrusterArrayComponent>();
	}

	if (!Thrusters)
	{
		// Loudly: a pilot station with nothing to command looks exactly like a ship
		// that will not respond, and the difference matters at three in the morning.
		UE_LOG(LogStarForge, Error,
			TEXT("Pilot station found no thruster array on its owner; the ship cannot be flown."));
	}
}

void USFPilotStationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Thrusters || !Thrusters->IsReady() || !Thrusters->ShipData || !IsOccupied())
	{
		return;
	}

	// An operator who is not flying says so rather than handing back a zeroed intent:
	// zero means "stick centred", which with assist on is a command to stop.
	StarForge::FFlightIntent Intent;
	if (!Operator->ProvideFlightIntent(Intent))
	{
		return;
	}

	LastIntent = Intent;

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent,
		Thrusters->ReadState(),
		Thrusters->GetMassProperties(),
		Thrusters->ShipData->BuildLimits(),
		Thrusters->GetArrayCapability());

	Thrusters->ApplyDemand(Demand, DeltaTime);
}
```

- [x] **Step 3: Убрать контур из корабля и повесить пост**

В `SFTestShip.h` удалить объявление `void TickFlight(double DeltaSeconds);` и поле `StarForge::FFlightIntent LastIntent;` вместе с методом `GetLastIntent()`. Добавить:

```cpp
	USFPilotStationComponent* GetPilotStation() const { return PilotStation; }
```

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFPilotStationComponent> PilotStation;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFConsoleOperatorComponent> ConsoleOperator;
```

и forward-declare `class USFPilotStationComponent;`, `class USFConsoleOperatorComponent;`.

В конструкторе `SFTestShip.cpp`, рядом с созданием `Thrusters`:

```cpp
	PilotStation = CreateDefaultSubobject<USFPilotStationComponent>(TEXT("PilotStation"));

	// Exactly the height a standing occupant already occupies, from the same two
	// constants as GetInteriorSpawnWorldLocation. A seat above standing height leaves
	// no walkable floor under the teleported capsule, so PhysWalking drops him into
	// MOVE_Falling, which clears the movement base, and with no base the engine stops
	// calling UpdateBasedMovement -- the ship leaves without him. It hides at rest,
	// where he just falls the gap and lands.
	PilotStation->SeatLocationMetres = FVector(
		0.0, 0.0, StarForge::Units::CmToMetres(-InteriorHalfExtentCm + SpawnClearanceCm));

	ConsoleOperator = CreateDefaultSubobject<USFConsoleOperatorComponent>(TEXT("ConsoleOperator"));
```

Сюда же в `SFTestShip.cpp` нужно включение `#include "Units.h"` — кресло выводится из сантиметровых констант корабля через `CmToMetres`, а без этой строки буквальный повтор плана не соберётся.

В `Tick` удалить ветку `TickFlight` целиком — остаётся:

```cpp
	if (CVarShipProfile.GetValueOnGameThread() != 0)
	{
		TickScriptedProfile();
	}
```

и `TickFlight` удалить вместе с телом. Расчёт `LinearAccelMetersPerSecSq` по настоящей скорости тела остаётся в `Tick` без изменений.

В `LogFlightState` заменить источник намерения: вместо `LastIntent` использовать `PilotStation->GetLastIntent()`, а условие вызова в `Tick` — на:

```cpp
	if (CVarFlightLog.GetValueOnGameThread() != 0)
	{
		LogFlightState(DeltaSeconds);
	}
```

то есть журнал пишется в обоих режимах, а не только в полётном: в скриптовом он покажет, что пост никем не командует, и это полезно.

- [x] **Step 4: Собрать и прогнать headless**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -nullrhi -unattended -nosplash -benchmark -fps=60 -seconds=25 -ExecCmds="sf.FlightLog 1, sf.ShipProfile 0, sf.Intent.X 1" -log
```

Ожидается в журнале: строка `Station 'pilot' taken by console.` и разгон до **200.0 м/с** — те же числа, что до переезда контура. **Если скорость изменилась, переезд что-то сломал**: контур обязан считать ровно то же самое, только из другого места.

- [x] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFPilotStationComponent.h Source/StarForge/Private/Ship/SFPilotStationComponent.cpp Source/StarForge/Public/Ship/SFConsoleOperatorComponent.h Source/StarForge/Private/Ship/SFConsoleOperatorComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp
git commit -F - <<'MSG'
feat(ship): move the flight loop into the post that owns the thrusters

Spec 4 puts the loop in the station rather than in the ship, because the ship is not
supposed to know it is being flown -- only that a post commands its systems. The
station asks an operator for an intent and knows nothing else about it, which is
what will let a crewman, the ship's automation and a player share one execution path
instead of three.

The console variables became an operator rather than a branch. A station that has an
operator can always answer "who is flying" with a name, in the log and on screen; an
unowned branch that steers cannot. It also means the headless flight checks the last
plan depends on keep working unchanged, and that the interface has a second
implementation from the day it exists -- one that is not a person, which is the
claim being tested.

Top speed after the move is 200.0 m/s, the same figure as before it. The loop was
meant to compute exactly what it computed, only from somewhere else, and that is the
one thing worth checking about a relocation.
MSG
```

---

### Task 4: Посадка в кресло

Персонаж становится оператором. Владение пешкой не меняется — меняется только то, куда идёт ввод.

**Files:**
- Modify: `Source/StarForge/Public/Player/SFCharacter.h`
- Modify: `Source/StarForge/Private/Player/SFCharacter.cpp`

**Interfaces:**
- Consumes: `ISFStationOperator`, `USFShipStationComponent`, `USFPilotStationComponent` (Tasks 2–3); `USFShipboardMovementComponent::GetShipFrame`.
- Produces: `ASFCharacter` реализует `ISFStationOperator`; `bool IsSeated() const`, `USFShipStationComponent* GetSeatedStation() const`.

**Почему посадка — это перемещение, а не привязка.** Кресло задаётся позицией в системе отсчёта корабля, и перенос уже держит эту позицию с точностью 0.000 см за 90 секунд. Привязка к трансформу была бы вторым механизмом удержания рядом с первым — то есть вторым местом, где может разъехаться, и двумя разными ответами на вопрос «почему пассажир там, где он есть».

- [x] **Step 1: Объявить персонажа оператором**

В `SFCharacter.h` включить `#include "Ship/SFStationOperator.h"`, forward-declare `class USFShipStationComponent;` и изменить объявление класса:

```cpp
class STARFORGE_API ASFCharacter : public ACharacter, public ISFStationOperator
```

Добавить в public:

```cpp
	virtual bool ProvideFlightIntent(StarForge::FFlightIntent& Out) override;
	virtual FString GetOperatorName() const override { return TEXT("player"); }

	bool IsSeated() const { return SeatedStation != nullptr; }
	USFShipStationComponent* GetSeatedStation() const { return SeatedStation; }
```

и в private:

```cpp
	/** Tries to take the nearest station within reach, or leaves the current one. */
	void ToggleSeat();

	UPROPERTY()
	TObjectPtr<USFShipStationComponent> SeatedStation;

	/** Edge detection for the interact key: held is not pressed. */
	bool bInteractWasDown = false;
```

- [x] **Step 2: Реализовать посадку**

В `SFCharacter.cpp` включения:

```cpp
#include "Ship/SFShipStationComponent.h"
#include "StarForgeLog.h"
```

Добавить в `Tick`, после `PollLookInput`:

```cpp
	PollInteract();
```

и реализовать:

```cpp
void ASFCharacter::PollInteract()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// On the edge, not while held: a held key would seat and unseat him every frame,
	// and the log would fill with a station changing hands sixty times a second.
	const bool bDown = PC->IsInputKeyDown(EKeys::E);
	if (bDown && !bInteractWasDown)
	{
		ToggleSeat();
	}
	bInteractWasDown = bDown;
}

void ASFCharacter::ToggleSeat()
{
	if (SeatedStation)
	{
		SeatedStation->Vacate();
		SeatedStation = nullptr;

		// Walking resumes exactly where he was. He is already in the ship's frame and
		// the carry has been holding him all along; standing up changes what reads his
		// input, not where he is.
		if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
		return;
	}

	USFShipFrameComponent* Frame = GetShipboardMovement() ? GetShipboardMovement()->GetShipFrame() : nullptr;
	if (!Frame || !Frame->GetOwner())
	{
		return;
	}

	// The nearest station within reach, so two seats side by side do not fight.
	USFShipStationComponent* Best = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();

	TArray<USFShipStationComponent*> Stations;
	Frame->GetOwner()->GetComponents<USFShipStationComponent>(Stations);

	for (USFShipStationComponent* Station : Stations)
	{
		if (Station->IsOccupied() || !Station->IsWithinReach(GetActorLocation()))
		{
			continue;
		}

		const FVector SeatWorld =
			Frame->GetOwner()->GetActorTransform().TransformPosition(Station->GetSeatLocationCm());
		const double DistanceSq = FVector::DistSquared(SeatWorld, GetActorLocation());

		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Station;
		}
	}

	if (!Best)
	{
		UE_LOG(LogStarForge, Display, TEXT("No free station within reach."));
		return;
	}

	if (!Best->TryOccupy(this))
	{
		return;
	}

	SeatedStation = Best;

	// Moved into the seat once; from here the carry holds him there, the same carry
	// that held him standing. A second holding mechanism beside a proven one is a
	// second place for the two to disagree.
	const FVector SeatWorld =
		Frame->GetOwner()->GetActorTransform().TransformPosition(Best->GetSeatLocationCm());
	SetActorLocation(SeatWorld, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// Not walking while seated. Flying, not the legs, is what his input drives now.
	if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_None);
	}
}
```

Объявить `void PollInteract();` в private рядом с `PollLookInput`.

- [x] **Step 3: Отдать намерение и не ходить сидя**

Реализовать в `SFCharacter.cpp`:

```cpp
bool ASFCharacter::ProvideFlightIntent(StarForge::FFlightIntent& Out)
{
	// Only while seated. Standing at the console and pressing W is walking, not
	// flying, and an operator who has left his post must say he is not flying rather
	// than hand back a centred stick -- with assist on that is an order to stop.
	if (!IsSeated())
	{
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}

	double Forward = 0.0;
	double Right = 0.0;
	double Up = 0.0;

	if (PC->IsInputKeyDown(EKeys::W)) { Forward += 1.0; }
	if (PC->IsInputKeyDown(EKeys::S)) { Forward -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::D)) { Right += 1.0; }
	if (PC->IsInputKeyDown(EKeys::A)) { Right -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::SpaceBar)) { Up += 1.0; }
	if (PC->IsInputKeyDown(EKeys::LeftControl)) { Up -= 1.0; }

	Out.LinearAxis = FVector(Forward, Right, Up);

	double Roll = 0.0;
	double Pitch = 0.0;
	double Yaw = 0.0;

	if (PC->IsInputKeyDown(EKeys::Q)) { Roll -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::R))  { Roll += 1.0; }
	if (PC->IsInputKeyDown(EKeys::Up)) { Pitch += 1.0; }
	if (PC->IsInputKeyDown(EKeys::Down)) { Pitch -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::Left)) { Yaw -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::Right)) { Yaw += 1.0; }

	// (X roll, Y pitch, Z yaw), the same order everything else uses.
	Out.AngularAxis = FVector(Roll, Pitch, Yaw);
	Out.bFlightAssistEnabled = true;

	return true;
}
```

Пара крена — `Q` и `R`: `E` занята посадкой.

В `PollMovementInput` и `PollLookInput` добавить первым делом:

```cpp
	if (IsSeated())
	{
		return;
	}
```

иначе сидящий пилот будет одновременно вести корабль и пытаться идти.

- [x] **Step 4: Собрать и проверить прогоном**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

Порядок проверки:

1. `sf.ShipProfile 0` — корабль ведёт консольный оператор, в журнале `Station 'pilot' taken by console.`
2. Подойти к центру каюты и нажать **E**. Ожидается отказ: `Station 'pilot' is already held by console; refusing player.` **Это правильное поведение**, а не дефект: два оператора на одном посту — то, ради чего отказ и написан.
3. `sf.Intent.X 0`, затем в консоли освободить пост нельзя — поэтому для проверки посадки временно закомментировать `Pilot->TryOccupy(this)` в `USFConsoleOperatorComponent::BeginPlay`, пересобрать, и нажать **E** у кресла. Ожидается `Station 'pilot' taken by player.`, персонаж перемещается в кресло, **W** больше не ходит, а ведёт корабль.
4. Вернуть строку обратно.

**Отрицательный контроль:** отойти к стене и нажать **E**. Ожидается `No free station within reach.` — радиус работает, а не «садится откуда угодно».

- [x] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Player/SFCharacter.h Source/StarForge/Private/Player/SFCharacter.cpp
git commit -F - <<'MSG'
feat(player): let the occupant take a post without changing pawns

Spec 4 rejects the genre's usual trick of swapping the controller to a ship pawn:
it breaks the seamlessness the whole design is for, and it bakes in the assumption
that a ship has as many functions as fit on one keyboard. Sitting down is a state of
the character. Possession never changes; what changes is where his input goes.

Taking the seat is a single move into the station's ship-frame position, after which
the same carry that held him standing holds him sitting. Attaching him to a seat
transform would have been a second holding mechanism next to a proven one -- a second
place for the two to disagree about why he is where he is.

Seating is on the key's edge rather than while held, because a held key would seat
and unseat him sixty times a second and fill the log with a post changing hands. The
station refuses a second operator rather than accepting one, since two operators
would each believe they were flying and whichever was asked last would win in
silence.
MSG
```

---

### Task 5: Аналоговый ввод

Оси читаются аналоговыми. Мёртвая зона считается в `StarForgeCore` и проверяется головным тестом: это чистая арифметика, и ошибка в ней проявится как «ручка немного ведёт сама» — то, что ищут неделями.

**Files:**
- Modify: `Source/StarForgeCore/Public/Units.h`
- Modify: `Source/StarForgeCore/Private/Units.cpp`
- Modify: `Source/StarForgeCore/Private/Tests/UnitsTest.cpp`
- Modify: `Source/StarForge/Private/Player/SFCharacter.cpp`

**Interfaces:**
- Consumes: `ASFCharacter::ProvideFlightIntent` (Task 4).
- Produces: `StarForge::Units::ApplyDeadZone(double Raw, double DeadZone) -> double`.

**Почему мёртвая зона пересчитывает остаток, а не просто обнуляет.** Если за порогом отдавать сырое значение, ручка прыгает с нуля сразу на величину порога — на геймпаде это заметно, на ручке с длинным ходом это ступенька в самом чувствительном месте. Пересчёт даёт непрерывность: чуть за порогом — чуть больше нуля.

- [x] **Step 1: Написать падающие тесты**

Дописать в конец `Source/StarForgeCore/Private/Tests/UnitsTest.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsDeadZoneTest,
	"StarForge.Core.Units.DeadZone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsDeadZoneTest::RunTest(const FString& Parameters)
{
	// Inside the zone is nothing at all: a stick resting off-centre must not fly the
	// ship, and on a physical stick "resting off-centre" is the normal case.
	TestTrue(TEXT("noise inside the dead zone is discarded"),
		FMath::IsNearlyZero(StarForge::Units::ApplyDeadZone(0.05, 0.1), 1e-12));
	TestTrue(TEXT("and it is discarded either way round"),
		FMath::IsNearlyZero(StarForge::Units::ApplyDeadZone(-0.05, 0.1), 1e-12));

	// Just outside it, the output starts from zero rather than jumping to the
	// threshold. Returning the raw value would put a step at the most sensitive part
	// of the travel.
	TestTrue(TEXT("just past the zone the output is near zero, not near the threshold"),
		StarForge::Units::ApplyDeadZone(0.11, 0.1) < 0.02);

	// Full travel still means full travel, in both directions.
	TestTrue(TEXT("full deflection is unchanged"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(1.0, 0.1), 1.0, 1e-12));
	TestTrue(TEXT("and full deflection the other way too"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(-1.0, 0.1), -1.0, 1e-12));

	// Halfway between the threshold and full travel is halfway out, not 0.55.
	TestTrue(TEXT("the remaining travel is rescaled, not offset"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(0.55, 0.1), 0.5, 1e-9));

	// A zero dead zone must be a pass-through, so turning the feature off cannot
	// itself distort the axis.
	TestTrue(TEXT("a zero dead zone changes nothing"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(0.37, 0.0), 0.37, 1e-12));

	return true;
}
```

- [x] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается ошибка о том, что `ApplyDeadZone` не является членом `StarForge::Units`.

- [x] **Step 3: Реализовать**

В `Units.h`, внутри `namespace StarForge::Units`:

```cpp
	/**
	 * Removes a stick's centre slop without putting a step where it was.
	 *
	 * Inside the zone the result is zero. Outside it, the remaining travel is
	 * rescaled back onto the full range, so a stick a hair past the threshold reads a
	 * hair above zero. Returning the raw value instead would jump straight to the
	 * threshold — a step at the most sensitive part of the travel, which a gamepad
	 * hides and a long-throw stick does not.
	 *
	 * Raw and the result are both in [-1, 1]; DeadZone is a fraction of that range.
	 */
	STARFORGECORE_API double ApplyDeadZone(double Raw, double DeadZone);
```

В `Units.cpp`:

```cpp
	double ApplyDeadZone(double Raw, double DeadZone)
	{
		const double Zone = FMath::Clamp(DeadZone, 0.0, 0.99);
		const double Magnitude = FMath::Abs(Raw);

		if (Magnitude <= Zone)
		{
			return 0.0;
		}

		// Rescale what is left of the travel back onto [0, 1], then put the sign back.
		const double Rescaled = (Magnitude - Zone) / (1.0 - Zone);
		return FMath::Sign(Raw) * FMath::Min(Rescaled, 1.0);
	}
```

- [x] **Step 4: Прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается **37** пройденных тестов: 36 прежних плюс один новый.

- [x] **Step 5: Читать оси аналогово**

В `ASFCharacter::ProvideFlightIntent` заменить чтение линейных и угловых осей на сумму цифровых клавиш и аналоговых осей:

```cpp
	const UPlayerInput* Input = PC->PlayerInput;

	auto Axis = [Input](const FKey& Key) -> double
	{
		// GetKeyValue gives the analog value for an axis key and 0 or 1 for a button,
		// so one call covers both without the caller knowing which it asked for.
		return Input ? static_cast<double>(Input->GetKeyValue(Key)) : 0.0;
	};

	const double StickX = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_LeftY), StickDeadZone);
	const double StickY = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_LeftX), StickDeadZone);
	const double StickPitch = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_RightY), StickDeadZone);
	const double StickYaw = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_RightX), StickDeadZone);

	Out.LinearAxis = FVector(
		FMath::Clamp(Forward + StickX, -1.0, 1.0),
		FMath::Clamp(Right + StickY, -1.0, 1.0),
		FMath::Clamp(Up, -1.0, 1.0));

	Out.AngularAxis = FVector(
		FMath::Clamp(Roll, -1.0, 1.0),
		FMath::Clamp(Pitch + StickPitch, -1.0, 1.0),
		FMath::Clamp(Yaw + StickYaw, -1.0, 1.0));
```

Включения в `SFCharacter.cpp`:

```cpp
#include "GameFramework/PlayerInput.h"
#include "Units.h"
```

и поле в `SFCharacter.h`:

```cpp
	/** Fraction of stick travel treated as centre. Physical sticks rest off-centre. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Input")
	double StickDeadZone = 0.1;
```

**Клавиши и стик складываются, а не заменяют друг друга.** Так клавиатура и геймпад работают одновременно, и не нужно решать, кто «главный», — а зажим не даёт сумме выйти за предел.

- [x] **Step 6: Проверить руками**

Если есть геймпад — сесть в кресло и повести левым стиком. Ожидается **плавный** отклик: половина хода даёт половину скорости, и корабль не ползёт при отпущенном стике.

Если геймпада нет, проверяется только головным тестом, и это надо записать: аналоговый путь скомпилирован, но не наблюдался.

- [x] **Step 7: Коммит**

```bash
git add Source/StarForgeCore/Public/Units.h Source/StarForgeCore/Private/Units.cpp Source/StarForgeCore/Private/Tests/UnitsTest.cpp Source/StarForge/Public/Player/SFCharacter.h Source/StarForge/Private/Player/SFCharacter.cpp
git commit -F - <<'MSG'
feat(player): read the stick as an axis rather than as a switch

The controls are being designed for physical manipulators, so half the travel has to
mean half the command. Buttons and stick add rather than replace each other, which
lets a keyboard and a pad work at once without anyone deciding which is in charge.

The dead zone rescales the remaining travel instead of discarding the slop and
passing the rest through raw. Passing it through puts a step exactly at the
threshold, at the most sensitive part of the movement -- a gamepad hides that and a
long-throw stick will not. It lives in StarForgeCore behind tests because it is pure
arithmetic whose failure mode is "the stick drifts a little on its own", which is
the kind of thing that gets hunted for weeks in the wrong place.
MSG
```

---

## Точка проверки 4

После задачи 5 человек садится в кресло и ведёт корабль.

Проверить перед тем, как писать план «в»:

- тридцать семь головных тестов проходят из консоли, без редактора;
- `StarForgeCore` по-прежнему зависит только от `Core` и `CoreUObject`;
- сборка без предупреждений;
- **корабль под консольным оператором выходит на 200.0 м/с** — то же число, что до переезда контура в пост;
- в журнале видно, **кто занял пост**, по имени, в обоих режимах;
- посадка работает у кресла и отказывает у стены;
- сидящий пилот не ходит, стоящий не летает;
- отказ второго оператора на занятом посту записывается, а не проходит молча.

**Отдельно проверить то, чего этот план сознательно не делал:** пассажир при крене 180°/с у стены **не** падает — сбивание с ног появится только в плане «в». Если падает, значит что-то включило его раньше времени.

## Что дальше

План «в»: `FTurretIntent`, наведение турели (математика — в `StarForgeCore` под тестами), `USFTurretComponent`, пост канонира, автоматика как оператор в кресле канонира, сбивание с ног при превышении порога гравиплит.

Требования к нему, зафиксированные здесь:

1. **Автоматика садится в кресло канонира своего корабля, а не пиратского.** Проверяемое утверждение — «корабль не различает, кто занял пост»; пиратский корабль для него не нужен и откладывает проверку ради декораций.
2. **Компонент системы отсчёта должен начать отдавать линейное ускорение.** Сбиванию с ног нужно `ComputeFeltAcceleration`, а ускорение сейчас считает `ASFTestShip` — иначе компонент движения станет зависеть от класса тестового корабля, чего он не должен.
3. **Сбивание с ног не прекращает перенос.** Человек теряет опору, но остаётся на борту; прекращение переноса оставило бы его позади корабля за один кадр, а это отрицательный контроль, а не механика.
