# Корабль на физике — план реализации (этап 2, план «а»)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Превратить корабль в тело Chaos, которым управляет уже построенная математика полёта, не потеряв удержание пассажира на палубе.

**Architecture:** Корабль перестаёт двигаться сдвигом трансформа и становится симулируемым телом. Масса объявляется данными корабля, инерцию и центр масс считает Chaos, математика читает то, что посчитал Chaos. Требуемое воздействие раскладывается по двигателям решателем из первого плана и прикладывается к телу как сила и момент. Границу СИ ↔ сантиметры держат отдельные функции под тестами.

**Tech Stack:** Unreal Engine 5.8.2, C++, Chaos, Automation Test Framework.

**Spec:** [docs/superpowers/specs/2026-09-06-starforge-vertical-slice-design.md](../specs/2026-09-06-starforge-vertical-slice-design.md) — §4 (намерения), §5 целиком, §6 (гравиплиты), §7 (бюджеты), §9 (данные корабля).

**Предыдущий план:** [математика полёта](2026-09-07-flight-maths.md) — выполнен, даёт `FThrustAllocator` и `ComputeDemandedWrench`.

**Передача:** [handoff этапа 2](../notes/2026-09-07-handoff-stage-2.md) — читать до начала.

## Global Constraints

- Движок — **Unreal Engine 5.8.2**, установлен в `C:\Program Files\Epic Games\UE_5.8`.
- `StarForgeCore` зависит **только** от `Core` и `CoreUObject`. Добавление `Engine` в этот модуль — ошибка, ломающая смысл разделения.
- Внутри `StarForgeCore` физические величины — **в СИ**. Конвертация в сантиметры происходит только в функциях границы из Task 2 и нигде больше.
- Бюджеты из §7: маневрирование 200 м/с, форсаж 400 м/с, тангаж и рысканье 120°/с, крен 180°/с. Это константы спеки, а не ручки настройки.
- Потолок гравиплит — 2 g = 19.62 м/с². Сбивание с ног **в этот план не входит**, см. «Чего в этом плане нет».
- Все вычисления в **двойной точности** (`double`).
- В 5.8 `EAutomationTestFlags` — `enum class`, маска контекста вынесена в константу `EAutomationTestFlags_ApplicationContextMask`.
- **Редактор должен быть закрыт при сборке.** Проверять `tasklist | grep -i unreal`. Live Coding не умеет добавлять новые модули и классы — при их появлении редактор перезапускать.
- `UnrealEditor-Cmd.exe` обрезает вывод. Результаты тестов читать из `Saved/Logs/StarForge.log`: строки `Test Completed. Result=` и `TEST COMPLETE. EXIT CODE:`.
- Коммит после каждой задачи. Сообщения на английском, тело — почему, а не что.
- **Никаких строк `Co-Authored-By` и любой другой атрибуции в коммитах** — запрещено проектом, см. `CLAUDE.md`.

---

## Область этого плана

План «а» этапа 2 отвечает на вопрос **«летает ли корабль под собственной тягой, и держится ли на нём человек»**. Команды подаются консольными переменными.

**Чего в этом плане нет** и почему:

- **Пост пилота, `IStationOperator`, маршрутизация ввода** — план «б». Требование к нему записано уже сейчас: ввод обязан быть аналоговым по осям, потому что управление проектируется под физические манипуляторы, а не под клавиши.
- **Сбивание с ног при превышении порога гравиплит** — план «б», хотя решение принято и записано в §6. Причина техническая и жёсткая: **Task 1 меряет увод при крене 180°/с с пассажиром у стены — ровно там, где механика его и уронит.** Включённой она остановит прибор, а выглядеть это будет как провал разложения движения. Механику включать, когда мерить уже не надо.

### Риск, ради которого Task 1 стоит первой

Точка проверки 1 подтвердила перенос пассажира **на кинематическом корабле**. Перенос читает `GetActorTransform()` в момент своего вызова; у симулируемого тела трансформ обновляется в другой момент кадра и может оказаться интерполированным. Это единственное, что способно обесценить уже полученный результат, поэтому меряется до того, как на нём что-то построено.

---

## Структура файлов

```
Source/StarForgeCore/
  Public/
    Units.h                конвертация СИ <-> юниты Unreal, граница модулей
  Private/
    Units.cpp
    Tests/
      UnitsTest.cpp
      FlightControllerTest.cpp     дописывается: пределы по осям, ассист по возможностям

Source/StarForge/
  Public/Ship/
    SFShipDataAsset.h            паспорт корабля: масса, двигатели, пределы
    SFThrusterArrayComponent.h   владеет решателем, прикладывает тягу к телу
    SFTestShip.h                 правится: корень становится телом
  Private/Ship/
    SFShipDataAsset.cpp
    SFThrusterArrayComponent.cpp
    SFTestShip.cpp
  Public/Debug/
    SFFlightReadoutHUD.h         требуемое против достигнутого, газы, насыщение
  Private/Debug/
    SFFlightReadoutHUD.cpp
```

Разделение по ответственности: `Units` не знает про корабль, `SFShipDataAsset` — только данные без поведения, `SFThrusterArrayComponent` не знает про пилота, HUD не знает, как считается то, что он показывает.

---

## Как проверяется этот план

В отличие от плана математики, **головными тестами проверяется меньшая часть**. Это меняет цену ошибки: там дефект ловился за секунду, здесь проявится как «корабль странно себя ведёт». Поэтому у каждой задачи ниже явно назван способ проверки.

| Задача | Чем проверяется |
|---|---|
| 1. Тело Chaos, замер увода | Ручной замер по runbook точки проверки 1, сравнение с бланком |
| 2. Единицы | Головные тесты |
| 3. Пределы по осям и ассист | Головные тесты |
| 4. Паспорт корабля | Головные тесты на валидацию + глазами в редакторе |
| 5. Компонент двигателей | Головные тесты на сборку + полёт глазами |
| 6. Замкнутый контур | Полёт по консоли, показания прибора |
| 7. Прибор | Собой же: показания сверяются с известными величинами |

---

### Task 1: Корабль становится телом Chaos, увод меряется заново

Единственная переменная — источник трансформа. Профиль манёвра, прибор и runbook остаются прежними, чтобы числа сравнивались с бланком точки проверки 1 напрямую.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`

**Interfaces:**
- Consumes: ничего.
- Produces: `ASFTestShip::GetHull()` возвращает `UBoxComponent*` — симулируемое тело. Task 5 находит его сам, приведением корневого компонента, чтобы не зависеть от класса корабля; этот доступ нужен самому кораблю в Task 6.

**Почему корень приходится менять.** Сейчас `Root` — голый `USceneComponent`. Он не примитив, тела у него нет, и `SetSimulatePhysics` на нём не сделает ничего — молча. Корнем становится `UBoxComponent`.

**Почему панели переводятся в `QueryOnly`.** Сейчас у них `QueryAndPhysics`. Примитив с физической коллизией, прицепленный к симулируемому корню, может быть приварен к его телу, и форма тела станет зависеть от порядка инициализации. Панелям нужно только держать персонажа — это запросы. Физическую форму даёт корневой бокс.

**Почему бокс `PhysicsOnly`.** Он должен давать телу массу и инерцию, но не участвовать в запросах — иначе персонаж окажется замурован в сплошном кубе вместо полой комнаты.

- [ ] **Step 1: Заменить корень на симулируемый бокс**

В `SFTestShip.h` в список forward-declare добавить:

```cpp
class UBoxComponent;
```

Заменить объявление `Root` на `Hull` и добавить доступ к телу:

```cpp
	/** The simulated body. Task 5 applies thrust here. */
	UBoxComponent* GetHull() const { return Hull; }
```

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UBoxComponent> Hull;
```

Старое поле `Root` удалить, а все `SetupAttachment(Root)` заменить на `SetupAttachment(Hull)`.

- [ ] **Step 2: Собрать тело в конструкторе**

В `SFTestShip.cpp` добавить включение:

```cpp
#include "Components/BoxComponent.h"
```

Рядом с прочими константами в безымянном пространстве имён:

```cpp
	/**
	 * Stand-in mass until the ship data asset exists, in kilograms. Large enough
	 * that a character walking on the deck cannot perturb the measurement.
	 */
	constexpr double PlaceholderMassKg = 50000.0;
```

Заменить создание корня:

```cpp
	Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("Hull"));
	SetRootComponent(Hull);
	Hull->SetBoxExtent(FVector(InteriorHalfExtentCm));
	Hull->SetMobility(EComponentMobility::Movable);

	// Physics only: this box exists to give the body mass and inertia, not to be
	// walked into. With query collision on, the occupant would be sealed inside a
	// solid cube instead of the hollow room the panels make.
	Hull->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Hull->SetCollisionObjectType(ECC_WorldDynamic);
	Hull->SetSimulatePhysics(true);

	// A ship in space does not fall, and the rig has no other use for gravity.
	Hull->SetEnableGravity(false);

	// Stated rather than derived from the box volume, so the inertia the flight
	// maths later reads back from Chaos is reproducible instead of depending on a
	// default density. Task 4 replaces this with the ship data asset.
	Hull->SetMassOverrideInKg(NAME_None, static_cast<float>(PlaceholderMassKg), true);
```

В `AddBoxPanel` заменить строку коллизии:

```cpp
	// Query only. These panels hold the occupant up; the hull box is what the
	// physics solver sees. Leaving physics collision on them risks their being
	// welded into the hull body, which would make that body shape depend on
	// component initialisation order.
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
```

- [ ] **Step 3: Вести тело по прежнему профилю через скорости**

Заменить тело `Tick` ниже строки `const double Phase = ...`:

```cpp
	// Same profile as the kinematic rig, driven through the physics solver instead
	// of by moving the transform. Velocity is commanded every tick rather than
	// integrated from forces: the point of this task is to change one thing only --
	// where the transform comes from -- so the motion must stay the one the
	// checkpoint 1 figures were taken against.
	const FVector VelocityCms(PeakSpeedCms * FMath::Sin(Phase), 0.0, 0.0);
	Hull->SetPhysicsLinearVelocity(VelocityCms);

	LinearAccelMetersPerSecSq = (VelocityCms - PreviousVelocityCms) / DeltaSeconds / 100.0;
	PreviousVelocityCms = VelocityCms;

	// All three angular axes loaded at once so no accidental symmetry hides a bug.
	// Pitch and yaw sweep in quadrature; roll is held flat at its ceiling, which is
	// the harsher case because it never lets up.
	//
	// An angular velocity vector is (roll about X, pitch about Y, yaw about Z) --
	// not the order FRotator prints in. It is commanded in the ship own axes and
	// rotated into world space, because SetPhysicsAngularVelocityInRadians is world
	// space while the old AddActorLocalRotation was local.
	const FVector LocalRateRadPerSec(
		FMath::DegreesToRadians(PeakRollRateDegPerSec),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Sin(Phase * 0.5)),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Cos(Phase * 0.5)));

	const FVector WorldRateRadPerSec =
		GetActorTransform().TransformVectorNoScale(LocalRateRadPerSec);

	Hull->SetPhysicsAngularVelocityInRadians(WorldRateRadPerSec);
```

Это **не побитово тот же поворот**, что раньше: композиция приращений `FRotator` и интегрирование угловой скорости в связанных осях совпадают только в первом порядке по шагу. При 180°/с и шаге 1/60 с приращение около трёх градусов, расхождение мало. Для сравнимости замера этого достаточно, но знать об этом надо — иначе разница в третьем знаке будет выглядеть как эффект физики.

- [ ] **Step 4: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается `Result: Succeeded` без предупреждений. Головных тестов у этой задачи нет — вся суть в поведении в мире, и ловить их тут нечем.

- [ ] **Step 5: Убедиться, что физика синхронная**

В консоли запущенной игры выполнить:

```
p.Chaos.Solver.AsyncPhysicsBlockMode
```

Асинхронный солвер с интерполяцией — именно тот случай, в котором `GetActorTransform()` вернёт сглаженное значение, а не то, что посчитала физика. Если он включён, это надо знать **до** замера, иначе непонятно, что именно измерено. Записать значение в бланк.

- [ ] **Step 6: Замерить увод**

Провести прогон по [runbook точки проверки 1](../notes/2026-09-06-checkpoint-1-runbook.md) без единого изменения процедуры.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

**Сначала смотреть на строку измеренного времени, потом на увод.** Ноль увода — это одинаково и идеальная минута, и мёртвый прибор; отличает их только `measured`.

Критерий: увод и дрожание в пределах бюджета §10 (1 см/мин) при `measured` не меньше 55 секунд, то есть числа того же порядка, что в [бланке](../notes/2026-09-06-checkpoint-1-results.md).

- [ ] **Step 7: Если увод разъехался — читать трансформ у тела**

Выполнять **только если Step 6 не прошёл.** Наиболее вероятная причина названа в передаче: компонент движения персонажа тикает в `TG_PrePhysics`, а трансформ актора обновляется из результатов физики позже, поэтому перенос читает трансформ прошлого кадра.

Проверять по одному, в этом порядке, и записывать, что именно помогло:

1. Перевести тик персонажа за физику: в конструкторе `ASFCharacter` поставить `PrimaryActorTick.TickGroup = TG_PostPhysics;`.
2. Если не хватило — читать трансформ прямо у тела вместо актора. В `USFShipFrameComponent::GetFrameTransform()`:

```cpp
	// The actor transform is synced from the physics results at end of frame; a
	// caller running earlier in the frame sees last frame value. The body knows
	// where it is now.
	if (const UPrimitiveComponent* Primitive =
			Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		if (Primitive->IsSimulatingPhysics() && Primitive->GetBodyInstance())
		{
			return Primitive->GetBodyInstance()->GetUnrealWorldTransform();
		}
	}

	return GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
```

Для этого в `SFShipFrameComponent.cpp` нужны включения:

```cpp
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
```

- [ ] **Step 8: Записать результат и закоммитить**

Завести `docs/superpowers/notes/2026-09-07-checkpoint-1-rerun-results.md` по образцу прежнего бланка: дата, длительность, `measured`, увод, дрожание, значение из Step 5, и **что именно потребовалось** из Step 7, если потребовалось.

```bash
git add Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp docs/superpowers/notes/2026-09-07-checkpoint-1-rerun-results.md
git commit -F - <<'MSG'
feat(ship): put the rig on Chaos and re-measure occupant drift

Checkpoint 1 was measured against a ship that moved by setting its transform.
The carry reads the ship transform when it runs, and a simulated body updates
that transform at a different point in the frame, so the result it earned did
not automatically survive the change. Nothing about the profile, the instrument
or the procedure moved -- only where the transform comes from -- so the figures
compare directly against the original form.

The root had to become a primitive: SetSimulatePhysics on a bare SceneComponent
does nothing and reports nothing. The hull box is physics-only so the occupant
walks in a hollow room rather than inside a solid cube, and the panels drop to
query-only so they cannot be welded into the hull and make its shape depend on
component initialisation order.
MSG
```

---

### Task 2: Единицы на границе модулей

Chaos считает в сантиметрах и килограммах, `StarForgeCore` — в СИ. Перевод собирается в одном месте под тестами, потому что перепутанный множитель здесь не падает и не логируется: корабль просто летает в сто раз бодрее или вяло, и объяснение этому ищут в решателе.

**Files:**
- Create: `Source/StarForgeCore/Public/Units.h`
- Create: `Source/StarForgeCore/Private/Units.cpp`
- Create: `Source/StarForgeCore/Private/Tests/UnitsTest.cpp`

**Interfaces:**
- Consumes: `StarForge::FWrench` из `ThrusterDef.h`.
- Produces: `StarForge::Units::MetresToCm`, `CmToMetres`, `NewtonsToUu`, `NewtonMetresToUu`, `InertiaUuToSi`, `WrenchToUu(const FWrench&) -> FWrench`.

**Откуда берутся множители.** Unreal считает массу в килограммах, а длину в сантиметрах. Значит ньютон, то есть кг·м/с², в его единицах равен 100 кг·см/с², а ньютон-метр — 10000 кг·см²/с². Инерция приходит из Chaos в кг·см² и делится на 10000. Это не подгонка: сто в первой степени для силы и сто во второй для момента и инерции.

- [ ] **Step 1: Написать падающие тесты**

`Source/StarForgeCore/Private/Tests/UnitsTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Units.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsLengthTest,
	"StarForge.Core.Units.Length",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsLengthTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a metre is a hundred centimetres"),
		FMath::IsNearlyEqual(StarForge::Units::MetresToCm(1.0), 100.0, 1e-12));
	TestTrue(TEXT("and back again"),
		FMath::IsNearlyEqual(StarForge::Units::CmToMetres(100.0), 1.0, 1e-12));

	// Round trip on an awkward number, because the failure this guards against is a
	// factor applied twice rather than a factor got wrong.
	const double Original = 3.7;
	TestTrue(TEXT("length survives a round trip"),
		FMath::IsNearlyEqual(
			StarForge::Units::CmToMetres(StarForge::Units::MetresToCm(Original)),
			Original, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsForceTest,
	"StarForge.Core.Units.Force",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsForceTest::RunTest(const FString& Parameters)
{
	// One newton accelerates one kilogram at one metre per second squared. Unreal
	// measures that acceleration in centimetres, so the same force reads as 100.
	TestTrue(TEXT("a newton is a hundred Unreal force units"),
		FMath::IsNearlyEqual(StarForge::Units::NewtonsToUu(1.0), 100.0, 1e-9));

	// A newton-metre carries a length twice over -- once in the force, once in the
	// lever arm -- so its factor is a hundred squared, not a hundred.
	TestTrue(TEXT("a newton-metre is ten thousand Unreal torque units"),
		FMath::IsNearlyEqual(StarForge::Units::NewtonMetresToUu(1.0), 10000.0, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsInertiaTest,
	"StarForge.Core.Units.Inertia",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsInertiaTest::RunTest(const FString& Parameters)
{
	// Chaos reports the inertia tensor in kilogram centimetres squared.
	const FVector FromChaos(10000.0, 20000.0, 30000.0);
	const FVector InSi = StarForge::Units::InertiaUuToSi(FromChaos);

	TestTrue(TEXT("inertia divides by a hundred squared"),
		InSi.Equals(FVector(1.0, 2.0, 3.0), 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsWrenchTest,
	"StarForge.Core.Units.Wrench",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsWrenchTest::RunTest(const FString& Parameters)
{
	// Force and torque take different factors, so converting a wrench with one
	// factor for both is the mistake this exists to catch.
	StarForge::FWrench Si;
	Si.Force = FVector(1.0, 2.0, 3.0);
	Si.Torque = FVector(4.0, 5.0, 6.0);

	const StarForge::FWrench Uu = StarForge::Units::WrenchToUu(Si);

	TestTrue(TEXT("force scales by a hundred"),
		Uu.Force.Equals(FVector(100.0, 200.0, 300.0), 1e-6));
	TestTrue(TEXT("torque scales by ten thousand"),
		Uu.Torque.Equals(FVector(40000.0, 50000.0, 60000.0), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInertiaRotationIsHonouredTest,
	"StarForge.Core.Units.InertiaRotationIsHonoured",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInertiaRotationIsHonouredTest::RunTest(const FString& Parameters)
{
	// TransformByInertia has a rotation branch that no test has ever executed --
	// every fixture so far left InertiaRotation identity. Chaos is documented to
	// hand back an identity rotation too, so runtime will not cover it either, and
	// an untested branch that only wakes on an unusual hull is the worst kind.
	//
	// Principal moments (1, 2, 3) with the principal axes yawed 90 degrees from the
	// ship axes. A ship-frame +X vector lies along the principal +Y axis, whose
	// moment is 2, so the result is twice the input and still along ship +X.
	StarForge::FShipMassProperties Mass;
	Mass.InertiaDiagonal = FVector(1.0, 2.0, 3.0);
	Mass.InertiaRotation = FRotator(0.0, 90.0, 0.0).Quaternion();

	const FVector Result = Mass.TransformByInertia(FVector(1.0, 0.0, 0.0));

	TestTrue(TEXT("the principal-axis rotation is applied both ways"),
		Result.Equals(FVector(2.0, 0.0, 0.0), 1e-9));

	// With identity rotation the same call must be a plain componentwise scale, so
	// the rotation branch cannot be quietly wrong in the common case either.
	Mass.InertiaRotation = FQuat::Identity;
	TestTrue(TEXT("identity rotation is a plain componentwise scale"),
		Mass.TransformByInertia(FVector(1.0, 1.0, 1.0)).Equals(FVector(1.0, 2.0, 3.0), 1e-9));

	return true;
}
```

- [ ] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается `Cannot open include file: 'Units.h'`.

- [ ] **Step 3: Реализовать**

`Source/StarForgeCore/Public/Units.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ThrusterDef.h"

/**
 * The one place SI meets Unreal units.
 *
 * StarForgeCore works in metres, kilograms, newtons and radians; Unreal works in
 * centimetres with mass in kilograms. Every conversion between the two lives here
 * and nowhere else, because a factor of a hundred applied twice -- or not at all --
 * does not crash or log. The ship simply flies wrong, and the search for why
 * starts in the solver, which is innocent.
 */
namespace StarForge::Units
{
	/** Unreal centimetres in one metre. */
	inline constexpr double CmPerMetre = 100.0;

	STARFORGECORE_API double MetresToCm(double Metres);
	STARFORGECORE_API double CmToMetres(double Cm);

	/**
	 * Newtons to Unreal force units.
	 *
	 * A newton is a kilogram metre per second squared. Mass is already in
	 * kilograms, so only the length changes: one factor of a hundred.
	 */
	STARFORGECORE_API double NewtonsToUu(double Newtons);

	/**
	 * Newton-metres to Unreal torque units.
	 *
	 * A newton-metre carries length twice -- once in the force, once in the lever
	 * arm -- so the factor is a hundred squared.
	 */
	STARFORGECORE_API double NewtonMetresToUu(double NewtonMetres);

	/** Chaos reports the inertia tensor in kilogram centimetres squared. */
	STARFORGECORE_API FVector InertiaUuToSi(const FVector& InertiaUu);

	/** A whole wrench, with the two different factors its halves need. */
	STARFORGECORE_API FWrench WrenchToUu(const FWrench& Si);
}
```

`Source/StarForgeCore/Private/Units.cpp`:

```cpp
#include "Units.h"

namespace StarForge::Units
{
	double MetresToCm(double Metres)
	{
		return Metres * CmPerMetre;
	}

	double CmToMetres(double Cm)
	{
		return Cm / CmPerMetre;
	}

	double NewtonsToUu(double Newtons)
	{
		return Newtons * CmPerMetre;
	}

	double NewtonMetresToUu(double NewtonMetres)
	{
		return NewtonMetres * CmPerMetre * CmPerMetre;
	}

	FVector InertiaUuToSi(const FVector& InertiaUu)
	{
		return InertiaUu / (CmPerMetre * CmPerMetre);
	}

	FWrench WrenchToUu(const FWrench& Si)
	{
		FWrench Result;
		Result.Force = Si.Force * CmPerMetre;
		Result.Torque = Si.Torque * CmPerMetre * CmPerMetre;
		return Result;
	}
}
```

- [ ] **Step 4: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается двадцать семь пройденных тестов: двадцать два прежних плюс пять новых. Читать из `Saved/Logs/StarForge.log`.

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForgeCore/Public/Units.h Source/StarForgeCore/Private/Units.cpp Source/StarForgeCore/Private/Tests/UnitsTest.cpp
git commit -F - <<'MSG'
feat(core): put the SI to Unreal boundary in one tested place

Chaos measures length in centimetres and StarForgeCore in metres, so a force
carries one factor of a hundred and a torque carries two. Getting that wrong
neither crashes nor logs: the ship flies a hundred times too hard or too softly
and the search for the reason starts in the solver, which is innocent. Gathering
the conversions behind tests makes the factor visible once instead of guessed at
every call site.

The inertia-rotation test covers a branch nothing has ever run. Every fixture so
far left the rotation identity, and Chaos is documented to return identity too,
so runtime was never going to cover it either -- leaving a branch that wakes up
only on a hull whose principal axes are turned from its model.
MSG
```

---

### Task 3: Пределы по осям и ассист, пропорциональный тяге

Два расхождения со спекой, найденные при написании передачи. Оба меняют `StarForgeCore` и потому проверяются головными тестами целиком.

**Files:**
- Modify: `Source/StarForgeCore/Public/FlightController.h`
- Modify: `Source/StarForgeCore/Private/FlightController.cpp`
- Modify: `Source/StarForgeCore/Private/Tests/FlightControllerTest.cpp`

**Interfaces:**
- Consumes: `StarForge::FWrench`, `FShipMassProperties` из `ThrusterDef.h`.
- Produces: `FFlightLimits::MaxAngularSpeed` становится `FVector`; `ComputeDemandedWrench` получает пятый параметр `const FWrench& ArrayCapability`.

**Порядок осей — не косметика.** `AngularAxis` и `MaxAngularSpeed` суть векторы угловой скорости, а у такого вектора компоненты это **(X — крен, Y — тангаж, Z — рысканье)**. Спека в §4 перечисляет их словами как «тангаж / рысканье / крен» — это подпись назначения, а не порядок хранения. Перепутать их значит получить корабль, который кренится, когда просят тангаж, и найти это через недели. Ниже это закреплено тестом и комментарием.

- [ ] **Step 1: Переписать фикстуры и добавить падающие тесты**

В `FlightControllerTest.cpp` заменить `MakeLimits`:

```cpp
	StarForge::FFlightLimits MakeLimits()
	{
		StarForge::FFlightLimits Limits;
		Limits.MaxSpeed = 200.0;
		// Angular limits are an angular velocity vector: X roll, Y pitch, Z yaw.
		// Spec 7 asks for 180 degrees per second of roll and 120 of pitch and yaw,
		// which is three numbers and cannot be held by one.
		Limits.MaxAngularSpeed = FVector(
			FMath::DegreesToRadians(180.0),
			FMath::DegreesToRadians(120.0),
			FMath::DegreesToRadians(120.0));
		Limits.LinearGain = 2.0;
		Limits.AngularGain = 3.0;
		return Limits;
	}

	/** What the test ship array can produce on each axis, in newtons and newton-metres. */
	StarForge::FWrench MakeCapability()
	{
		StarForge::FWrench Capability;
		Capability.Force = FVector(8000.0, 8000.0, 8000.0);
		Capability.Torque = FVector(16000.0, 16000.0, 16000.0);
		return Capability;
	}
```

Каждый прежний вызов `ComputeDemandedWrench(...)` в этом файле получает шестым аргументом `MakeCapability()`.

В `FFlightControllerGyroscopicTermTest` заменить тело так, чтобы оно пользовалось разными пределами по осям:

```cpp
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	// Spinning about roll and pitch at exactly the commanded rate: no angular error
	// at all, yet Euler still demands torque to hold it.
	const double RollRate = Limits.MaxAngularSpeed.X;
	const double PitchRate = Limits.MaxAngularSpeed.Y;

	StarForge::FRigidBodyState State;
	State.AngularVelocity = FVector(RollRate, PitchRate, 0.0);

	StarForge::FFlightIntent Intent;
	Intent.AngularAxis = FVector(1.0, 1.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, Mass, Limits, MakeCapability());

	// omega x (I omega) with omega = (wx, wy, 0) and I = diag(Ix, Iy, Iz):
	//   x: wy*0 - 0*Iy*wy        = 0
	//   y: 0*Ix*wx - wx*0        = 0
	//   z: wx*(Iy*wy) - wy*(Ix*wx) = wx*wy*(Iy - Ix)
	const double ExpectedZ = RollRate * PitchRate * (60000.0 - 20000.0);

	TestTrue(TEXT("holding a spin still demands torque"),
		!Demand.Torque.IsNearlyZero(1.0));
	TestTrue(TEXT("the gyroscopic demand matches Euler equation"),
		FMath::IsNearlyEqual(Demand.Torque.Z, ExpectedZ, 1.0));

	return true;
```

Дописать в конец файла три новых теста:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAssistOffIsProportionalTest,
	"StarForge.Core.FlightController.AssistOffIsProportionalToThrust",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAssistOffIsProportionalTest::RunTest(const FString& Parameters)
{
	// Spec 4: with assist off the axis is a fraction of what the array can produce.
	// Half the stick must be half the thrust -- on a physical stick, a demand that
	// saturates the thrusters early leaves most of the travel doing nothing.
	const StarForge::FWrench Capability = MakeCapability();

	StarForge::FFlightIntent Intent;
	Intent.bFlightAssistEnabled = false;
	Intent.LinearAxis = FVector(0.5, 0.0, 0.0);

	StarForge::FRigidBodyState State;

	const StarForge::FWrench Half = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits(), Capability);

	TestTrue(TEXT("half the stick asks for half of what the array can produce"),
		FMath::IsNearlyEqual(Half.Force.X, 0.5 * Capability.Force.X, 1e-6));

	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);
	const StarForge::FWrench Full = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits(), Capability);

	TestTrue(TEXT("full stick asks for exactly what the array can produce"),
		FMath::IsNearlyEqual(Full.Force.X, Capability.Force.X, 1e-6));

	// Speed must not enter into it: the ship at its limit is still pushed.
	State.LinearVelocity = FVector(200.0, 0.0, 0.0);
	const StarForge::FWrench AtSpeed = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits(), Capability);

	TestTrue(TEXT("assist off ignores how fast the ship already goes"),
		FMath::IsNearlyEqual(AtSpeed.Force.X, Capability.Force.X, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerPerAxisAngularLimitTest,
	"StarForge.Core.FlightController.AngularLimitIsPerAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerPerAxisAngularLimitTest::RunTest(const FString& Parameters)
{
	// Spec 7 gives roll a higher ceiling than pitch. A single scalar cannot express
	// that, and the failure is silent: either roll is a third too slow or pitch is
	// half again too fast, and both feel like tuning rather than a bug.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	StarForge::FRigidBodyState State;

	StarForge::FFlightIntent Roll;
	Roll.AngularAxis = FVector(1.0, 0.0, 0.0);
	const StarForge::FWrench RollDemand = StarForge::ComputeDemandedWrench(
		Roll, State, Mass, Limits, MakeCapability());

	StarForge::FFlightIntent Pitch;
	Pitch.AngularAxis = FVector(0.0, 1.0, 0.0);
	const StarForge::FWrench PitchDemand = StarForge::ComputeDemandedWrench(
		Pitch, State, Mass, Limits, MakeCapability());

	// Demand is inertia times gain times the target rate, so comparing the two
	// axes means dividing out their different moments of inertia first.
	const double RollRateAsked = RollDemand.Torque.X / (20000.0 * Limits.AngularGain);
	const double PitchRateAsked = PitchDemand.Torque.Y / (60000.0 * Limits.AngularGain);

	TestTrue(TEXT("full roll asks for the roll ceiling"),
		FMath::IsNearlyEqual(RollRateAsked, FMath::DegreesToRadians(180.0), 1e-6));
	TestTrue(TEXT("full pitch asks for the pitch ceiling"),
		FMath::IsNearlyEqual(PitchRateAsked, FMath::DegreesToRadians(120.0), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAngularAxisOrderTest,
	"StarForge.Core.FlightController.AngularAxisOrderIsRollPitchYaw",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAngularAxisOrderTest::RunTest(const FString& Parameters)
{
	// AngularAxis is an angular velocity vector: X roll, Y pitch, Z yaw. Spec 4
	// lists the three by name in a different order, which is a description of what
	// they are for and not of how they are stored. A ship that rolls when the pilot
	// asks for pitch is the shape this mistake takes, and it surfaces weeks later.
	//
	// The inertia here is diagonal in ship axes, so a demand about one axis alone
	// must produce torque about that axis alone.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();
	StarForge::FRigidBodyState State;

	StarForge::FFlightIntent Intent;
	Intent.AngularAxis = FVector(1.0, 0.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, Mass, Limits, MakeCapability());

	TestTrue(TEXT("a roll command torques about X"), Demand.Torque.X > 0.0);
	TestTrue(TEXT("and not about Y"), FMath::Abs(Demand.Torque.Y) < 1e-9);
	TestTrue(TEXT("and not about Z"), FMath::Abs(Demand.Torque.Z) < 1e-9);

	return true;
}
```

- [ ] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидаются ошибки о числе аргументов `ComputeDemandedWrench` и о присваивании `FVector` полю типа `double`.

- [ ] **Step 3: Реализовать**

В `FlightController.h` заменить поле предела и сигнатуру:

```cpp
		/**
		 * Radians per second, as an angular velocity vector: X roll, Y pitch, Z yaw.
		 *
		 * Three numbers, not one. Spec 7 gives roll 180 degrees per second and pitch
		 * and yaw 120, and a single scalar silently picks one of them for all three.
		 */
		FVector MaxAngularSpeed = FVector(
			UE_DOUBLE_PI,
			2.0 * UE_DOUBLE_PI / 3.0,
			2.0 * UE_DOUBLE_PI / 3.0);
```

```cpp
	/**
	 * Turns an intent into the force and torque the thrusters should be asked for.
	 *
	 * The torque includes Euler gyroscopic term, omega cross I omega. It is what
	 * makes a heavy ship feel heavy: without it, holding an established spin costs
	 * nothing and every hull turns like a fighter.
	 *
	 * ArrayCapability is the most the thruster array can produce on each axis, and
	 * it is what the stick is a fraction of when assist is off. It arrives as six
	 * numbers rather than as a reference to the solver, so this loop stays ignorant
	 * of how thrust is distributed.
	 */
	STARFORGECORE_API FWrench ComputeDemandedWrench(
		const FFlightIntent& Intent,
		const FRigidBodyState& State,
		const FShipMassProperties& Mass,
		const FFlightLimits& Limits,
		const FWrench& ArrayCapability);
```

В `FFlightIntent` дописать к `AngularAxis` комментарий, закрывающий вопрос порядка:

```cpp
		/**
		 * Angular command as an angular velocity vector: X roll, Y pitch, Z yaw.
		 * Each component in [-1, 1].
		 *
		 * Spec 4 names these pitch, yaw and roll in its prose. That is what they do,
		 * not the order they sit in -- the order here is the one FVector uses for
		 * angular velocity everywhere else, including what Chaos hands back.
		 */
		FVector AngularAxis = FVector::ZeroVector;
```

В `FlightController.cpp` заменить сигнатуру и обе ветки:

```cpp
	FWrench ComputeDemandedWrench(
		const FFlightIntent& Intent,
		const FRigidBodyState& State,
		const FShipMassProperties& Mass,
		const FFlightLimits& Limits,
		const FWrench& ArrayCapability)
	{
		const FVector LinearAxis = ClampAxes(Intent.LinearAxis);
		const FVector AngularAxis = ClampAxes(Intent.AngularAxis);

		FWrench Demand;

		if (Intent.bFlightAssistEnabled)
		{
			// The stick asks for a velocity, and the demand is whatever closes the gap.
			// Releasing it therefore asks for zero velocity, which is why the ship
			// stops instead of coasting.
			const FVector TargetVelocity = LinearAxis * Limits.MaxSpeed;
			Demand.Force = (TargetVelocity - State.LinearVelocity) * Mass.Mass * Limits.LinearGain;

			// Componentwise: each axis has its own ceiling.
			const FVector TargetAngular = AngularAxis * Limits.MaxAngularSpeed;
			const FVector AngularError = TargetAngular - State.AngularVelocity;
			Demand.Torque = Mass.TransformByInertia(AngularError * Limits.AngularGain);
		}
		else
		{
			// The stick asks for thrust directly, as a fraction of what this array can
			// actually deliver. Scaling by anything else -- mass times a speed limit,
			// say -- leaves the thrusters saturated across most of the stick travel,
			// which a gamepad hides and a physical stick does not.
			Demand.Force = LinearAxis * ArrayCapability.Force;
			Demand.Torque = AngularAxis * ArrayCapability.Torque;
		}

		// Euler gyroscopic term. Present in both modes: it is a property of a
		// spinning rigid body, not of the assist.
		const FVector AngularMomentum = Mass.TransformByInertia(State.AngularVelocity);
		Demand.Torque += FVector::CrossProduct(State.AngularVelocity, AngularMomentum);

		return Demand;
	}
```

- [ ] **Step 4: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается тридцать пройденных тестов: двадцать семь после Task 2 плюс три новых.

Прежний `FFlightControllerAssistOffIsDirectDemandTest` остаётся и продолжает проверять, что при выключенном ассисте корабль на предельной скорости всё ещё толкают вперёд. Он больше не единственный про этот режим, но именно он ловит случай «ассист выключили, а он всё равно держит скорость».

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForgeCore/Public/FlightController.h Source/StarForgeCore/Private/FlightController.cpp Source/StarForgeCore/Private/Tests/FlightControllerTest.cpp
git commit -F - <<'MSG'
feat(core): give each axis its own ceiling and make the stick proportional

Spec 7 asks for 180 degrees per second of roll and 120 of pitch and yaw, which a
single scalar cannot hold: it silently picks one of them for all three, leaving
roll a third too slow or pitch half again too fast. Neither reads as a bug -- both
read as tuning.

With assist off the demand is now a fraction of what the array can actually
produce, as spec 4 says it should be. Computing it from mass and a speed limit
instead leaves the thrusters saturated for most of the stick travel, so the
remaining travel does nothing; a gamepad hides that and the physical controls this
is being designed for will not.

The array capability arrives as six numbers rather than as a reference to the
solver, so the control loop still knows nothing about how thrust gets distributed.

The axis order is now pinned by a test. These vectors are angular velocities --
X roll, Y pitch, Z yaw -- while the spec names them in a different order in prose,
and a ship that rolls when asked to pitch is what confusing the two produces.
MSG
```

---

### Task 4: Паспорт корабля

Спека §9 требует данных корабля, из которых берётся всё остальное. `FThrusterDef` — обычная структура без `USTRUCT`, потому что `StarForgeCore` не знает про рефлексию; редактируемый вид ей даёт обёртка в игровом модуле.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFShipDataAsset.h`
- Create: `Source/StarForge/Private/Ship/SFShipDataAsset.cpp`

**Interfaces:**
- Consumes: `StarForge::FThrusterDef`, `FFlightLimits` из `StarForgeCore`.
- Produces: `USFShipDataAsset` с полями `MassKg`, `Thrusters`, `MaxSpeed`, `BoostSpeed`, `MaxAngularSpeedDegPerSec`, `LinearGain`, `AngularGain`; методами `TArray<StarForge::FThrusterDef> BuildThrusters() const` и `StarForge::FFlightLimits BuildLimits() const`.

**Почему положения двигателей в метрах.** Ассет — паспорт корабля, а не сцена. Внутри `StarForgeCore` всё в СИ, и если писать сюда сантиметры, то конвертация расползётся по вызывающим. Имена полей это проговаривают: `LocationMetres`, `MaxThrustNewtons`.

**Почему угловые пределы в градусах.** Их задаёт человек, а §7 написана в градусах. Перевод в радианы происходит один раз в `BuildLimits`.

**Чего из §9 здесь нет.** §9 перечисляет также отсеки, стыковочные узлы, посты, прочность корпуса и щитов. Всё это обслуживает планы 2b и дальше; здесь ассет несёт ровно то, без чего корабль не полетит.

**Расхождение внутри спеки, разрешённое в пользу §5.** §9 говорит, что ассет описывает массу **и центр масс**, а §5 требует читать массу и тензор инерции из Chaos. Взято §5, потому что его довод сильнее: расходиться во мнениях о вращении решателю и физике нельзя. Ассет объявляет массу, всё остальное выводит Chaos, и читается то, по чему корабль реально движется.

- [ ] **Step 1: Написать заголовок**

`Source/StarForge/Public/Ship/SFShipDataAsset.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FlightController.h"
#include "ThrusterDef.h"
#include "SFShipDataAsset.generated.h"

/**
 * One thruster, in a form the editor can show.
 *
 * StarForgeCore has no UObjects and so no reflection, which is deliberate -- it is
 * what keeps the maths testable without an engine. This mirrors FThrusterDef for
 * authoring and converts across at load.
 */
USTRUCT(BlueprintType)
struct STARFORGE_API FSFThrusterSpec
{
	GENERATED_BODY()

	/** Position in the ship frame, in METRES. Not centimetres -- the maths is SI. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	FVector LocationMetres = FVector::ZeroVector;

	/** Direction this thruster pushes the ship. Normalised on load. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	FVector Direction = FVector::ForwardVector;

	/** Thrust at full throttle, in newtons. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	double MaxThrustNewtons = 0.0;

	/** Seconds to spool from zero to full. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	double ResponseTimeSeconds = 0.05;
};

/**
 * A ship as data: what it weighs, what pushes it, and how hard it may be flown.
 *
 * Handling is not written here, it follows from the thruster layout. That is the
 * whole point of describing thrusters by geometry: change where they sit and the
 * ship flies differently without anyone touching the flight code.
 *
 * A layout that cannot produce all six axes is rejected rather than flown, because
 * a ship that silently refuses to turn is the hardest kind of bug to find. Note
 * that six axes needs opposed pairs: an array pushing only along +X, +Y and +Z can
 * never turn without also shoving the ship, since a throttle cannot go negative.
 */
UCLASS(BlueprintType)
class STARFORGE_API USFShipDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Kilograms. Declared here; Chaos derives inertia and centre of mass from it. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Ship")
	double MassKg = 50000.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Ship")
	TArray<FSFThrusterSpec> Thrusters;

	/** Manoeuvring ceiling from spec 7, metres per second. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double MaxSpeed = 200.0;

	/** Boost ceiling from spec 7, metres per second. Consumed by plan 2b. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double BoostSpeed = 400.0;

	/**
	 * Angular ceilings in degrees per second, as an angular velocity vector:
	 * X roll, Y pitch, Z yaw. Spec 7 gives 180 for roll and 120 for the other two,
	 * which is three numbers and must stay three.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	FVector MaxAngularSpeedDegPerSec = FVector(180.0, 120.0, 120.0);

	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double LinearGain = 2.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double AngularGain = 3.0;

	/** The thruster set in the form the solver wants: SI, normalised directions. */
	TArray<StarForge::FThrusterDef> BuildThrusters() const;

	/** The flight envelope in the form the control loop wants: SI, radians. */
	StarForge::FFlightLimits BuildLimits() const;
};
```

- [ ] **Step 2: Реализовать**

`Source/StarForge/Private/Ship/SFShipDataAsset.cpp`:

```cpp
#include "Ship/SFShipDataAsset.h"

TArray<StarForge::FThrusterDef> USFShipDataAsset::BuildThrusters() const
{
	TArray<StarForge::FThrusterDef> Result;
	Result.Reserve(Thrusters.Num());

	for (const FSFThrusterSpec& Spec : Thrusters)
	{
		StarForge::FThrusterDef Def;
		Def.Location = Spec.LocationMetres;
		Def.Direction = Spec.Direction;
		Def.MaxThrust = Spec.MaxThrustNewtons;
		Def.ResponseTime = Spec.ResponseTimeSeconds;
		Result.Add(Def);
	}

	return Result;
}

StarForge::FFlightLimits USFShipDataAsset::BuildLimits() const
{
	StarForge::FFlightLimits Limits;
	Limits.MaxSpeed = MaxSpeed;

	// Degrees are what a person authors and what spec 7 is written in; radians are
	// what everything downstream uses. Converted once, here.
	Limits.MaxAngularSpeed = FVector(
		FMath::DegreesToRadians(MaxAngularSpeedDegPerSec.X),
		FMath::DegreesToRadians(MaxAngularSpeedDegPerSec.Y),
		FMath::DegreesToRadians(MaxAngularSpeedDegPerSec.Z));

	Limits.LinearGain = LinearGain;
	Limits.AngularGain = AngularGain;
	return Limits;
}
```

- [ ] **Step 3: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Новый `UCLASS` означает, что Live Coding не поможет — редактор перезапустить.

- [ ] **Step 4: Завести ассет тестового корабля**

Открыть редактор и создать `Content/Ships/DA_TestShip` типа `SFShipDataAsset`.

Раскладка — двадцать четыре двигателя: четыре угла по 2 м от оси, на каждом встречная пара по каждой оси. Она заведомо покрывает шесть осей и является той же, на которой проверялся решатель.

| Поле | Значение |
|---|---|
| MassKg | 50000 |
| Углы | (2, 2, 0), (2, -2, 0), (-2, 2, 0), (-2, -2, 0) |
| Направления на каждом углу | ±X, ±Y, ±Z |
| MaxThrustNewtons | 200000 у каждого |

Тяга выбрана из бюджета §7, а не на глаз: четыре двигателя вдоль +X дают 800 кН, что при 50 т разгоняет корабль до 200 м/с примерно за 12.5 с. Если после Task 6 разгон окажется вялым или резким, менять надо это число, а не усиления контура.

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFShipDataAsset.h Source/StarForge/Private/Ship/SFShipDataAsset.cpp Content/Ships/DA_TestShip.uasset
git commit -F - <<'MSG'
feat(ship): describe a ship as data rather than as code

Handling is not written anywhere: it follows from where the thrusters sit, which
is what lets a hull fly differently without the flight code knowing it exists.
Positions are authored in metres and thrust in newtons because the maths is SI and
putting centimetres here would scatter the conversion across every caller instead
of keeping it at the boundary.

The angular ceiling stays three numbers for the reason spec 7 gives three, and the
degrees a person authors are turned into radians once, here, rather than wherever
someone remembers to.
MSG
```

---

### Task 5: Компонент массива двигателей

Владеет решателем, читает масс-свойства у Chaos и прикладывает решённую тягу к телу. Ни про пилота, ни про ввод не знает.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFThrusterArrayComponent.h`
- Create: `Source/StarForge/Private/Ship/SFThrusterArrayComponent.cpp`
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`, `Source/StarForge/Private/Ship/SFTestShip.cpp` — добавить компонент

**Interfaces:**
- Consumes: `USFShipDataAsset` (Task 4); `StarForge::FThrustAllocator`, `FWrench`, `FShipMassProperties`; `StarForge::Units` (Task 2); `ASFTestShip::GetHull()` (Task 1).
- Produces: `USFThrusterArrayComponent` с `bool Rebuild()`, `bool IsReady() const`, `void ApplyDemand(const StarForge::FWrench& DemandShipFrame, double DeltaSeconds)`, `StarForge::FWrench GetArrayCapability() const`, `StarForge::FShipMassProperties GetMassProperties() const`, `const TArray<double>& GetLastThrottles() const`, `StarForge::FWrench GetLastDemand() const`, `StarForge::FWrench GetLastAchieved() const`.

**Расхождение со спекой, намеренное.** §5 говорит прикладывать силы через `AddForceAtLocation` — по силе на двигатель в его точке. Здесь прикладывается сразу суммарное воздействие: `AddForce` в центре масс плюс `AddTorqueInRadians`. Результат тот же с точностью до порядка сложения, вызовов два вместо двадцати четырёх, и главное — к телу попадает ровно тот wrench, который посчитал решатель, а не сумма, которую ещё надо мысленно свести. `ComputeThrusterWrench` считает момент относительно центра масс, а `AddForce` без имени кости прикладывает силу туда же, поэтому лишнего момента не возникает.

**Почему масс-свойства читаются у Chaos, а не берутся из ассета.** Массу объявляет ассет, но инерцию и центр масс из неё выводит Chaos — по форме тела. Если считать инерцию отдельно, два источника разойдутся, и корабль будет считать один поворот, а совершать другой. Читается то, по чему он реально движется.

- [ ] **Step 1: Написать заголовок**

`Source/StarForge/Public/Ship/SFThrusterArrayComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThrustAllocator.h"
#include "ThrusterDef.h"
#include "SFThrusterArrayComponent.generated.h"

class UPrimitiveComponent;
class USFShipDataAsset;

/**
 * The ship thruster array: turns a demanded wrench into forces on the body.
 *
 * It owns the allocator and nothing else. It does not know who is flying, what
 * they pressed, or why the demand is what it is -- that separation is what lets an
 * autopilot, an NPC and a player share one execution path.
 *
 * Mass properties are read from Chaos rather than from the data asset. The asset
 * declares the mass; Chaos derives inertia and centre of mass from it and from the
 * body shape, and it is Chaos that actually moves the ship. Computing them
 * separately would let the two disagree, and the ship would plan one turn while
 * performing another.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFThrusterArrayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFThrusterArrayComponent();

	UPROPERTY(EditAnywhere, Category = "StarForge|Ship")
	TObjectPtr<USFShipDataAsset> ShipData;

	virtual void BeginPlay() override;

	/**
	 * Rebuilds the allocator from the data asset and the body current mass
	 * properties. Call again when a thruster is lost -- the handling degrades on its
	 * own, because the geometry really did change.
	 *
	 * Returns false when the layout cannot span all six axes. The ship then does not
	 * fly, loudly, instead of refusing to turn quietly.
	 */
	bool Rebuild();

	bool IsReady() const { return bReady; }

	/**
	 * Solves the demand across the thrusters, spools them toward that solution, and
	 * applies what they actually managed to the body.
	 *
	 * DeltaSeconds is needed because a thruster does not reach its commanded throttle
	 * instantly; see ResponseTime on FThrusterDef and spec 5.
	 */
	void ApplyDemand(const StarForge::FWrench& DemandShipFrame, double DeltaSeconds);

	/** The most this array can produce on each axis. What assist-off is a fraction of. */
	StarForge::FWrench GetArrayCapability() const { return Capability; }

	StarForge::FShipMassProperties GetMassProperties() const { return MassProperties; }

	const TArray<double>& GetLastThrottles() const { return LastThrottles; }
	StarForge::FWrench GetLastDemand() const { return LastDemand; }
	StarForge::FWrench GetLastAchieved() const { return LastAchieved; }

	/** Ship-frame linear velocity in m/s and angular velocity in rad/s. */
	StarForge::FRigidBodyState ReadState() const;

private:
	/** The simulated body this array pushes. Resolved once at BeginPlay. */
	UPrimitiveComponent* ResolveBody() const;

	StarForge::FThrustAllocator Allocator;
	StarForge::FShipMassProperties MassProperties;
	StarForge::FWrench Capability;

	/** Where the solver wants each throttle. */
	TArray<double> TargetThrottles;

	/** Where each throttle actually is, having spooled toward the target. */
	TArray<double> LastThrottles;

	/** Seconds to spool from zero to full, per thruster, copied from the asset. */
	TArray<double> ResponseTimes;

	StarForge::FWrench LastDemand;
	StarForge::FWrench LastAchieved;

	bool bReady = false;
};
```

Для `FRigidBodyState` заголовку нужен `#include "FlightController.h"` — добавить его рядом с прочими включениями.

- [ ] **Step 2: Реализовать**

`Source/StarForge/Private/Ship/SFThrusterArrayComponent.cpp`:

```cpp
#include "Ship/SFThrusterArrayComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Ship/SFShipDataAsset.h"
#include "StarForgeLog.h"
#include "Units.h"

USFThrusterArrayComponent::USFThrusterArrayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFThrusterArrayComponent::BeginPlay()
{
	Super::BeginPlay();
	Rebuild();
}

UPrimitiveComponent* USFThrusterArrayComponent::ResolveBody() const
{
	AActor* Owner = GetOwner();
	return Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
}

bool USFThrusterArrayComponent::Rebuild()
{
	bReady = false;

	if (!ShipData)
	{
		UE_LOG(LogStarForge, Error, TEXT("Thruster array has no ship data asset; the ship will not fly."));
		return false;
	}

	UPrimitiveComponent* Body = ResolveBody();
	if (!Body || !Body->GetBodyInstance())
	{
		UE_LOG(LogStarForge, Error, TEXT("Thruster array found no physics body on the root component."));
		return false;
	}

	FBodyInstance* Instance = Body->GetBodyInstance();

	// Chaos is authoritative here. The asset says what the ship weighs; Chaos works
	// out how that mass is distributed, and it is Chaos that integrates the motion.
	const FTransform MassSpace = Instance->GetMassSpaceLocal();

	MassProperties.Mass = Instance->GetBodyMass();
	MassProperties.CentreOfMass = MassSpace.GetLocation() / StarForge::Units::CmPerMetre;
	MassProperties.InertiaDiagonal = StarForge::Units::InertiaUuToSi(Instance->GetBodyInertiaTensor());
	MassProperties.InertiaRotation = MassSpace.GetRotation();

	const TArray<StarForge::FThrusterDef> Thrusters = ShipData->BuildThrusters();

	if (!Allocator.Build(Thrusters, MassProperties.CentreOfMass))
	{
		// Six axes needs opposed pairs: with throttles confined to [0, 1] an array
		// pushing only along the positive axes cannot torque without also shoving.
		UE_LOG(LogStarForge, Error,
			TEXT("Thruster layout of %d thrusters cannot span all six axes; the ship will not fly. ")
			TEXT("Check that every axis has an opposed pair."),
			Thrusters.Num());
		return false;
	}

	for (int32 Axis = 0; Axis < 6; ++Axis)
	{
		const double Value = Allocator.GetAxisCapability(Axis);
		if (Axis < 3)
		{
			Capability.Force[Axis] = Value;
		}
		else
		{
			Capability.Torque[Axis - 3] = Value;
		}
	}

	LastThrottles.Reset();
	LastThrottles.SetNumZeroed(Thrusters.Num());
	TargetThrottles.Reset();
	TargetThrottles.SetNumZeroed(Thrusters.Num());

	ResponseTimes.Reset();
	ResponseTimes.Reserve(Thrusters.Num());
	for (const StarForge::FThrusterDef& Thruster : Thrusters)
	{
		ResponseTimes.Add(Thruster.ResponseTime);
	}

	bReady = true;

	UE_LOG(LogStarForge, Display,
		TEXT("Thruster array ready: %d thrusters, %.0f kg, inertia (%.0f, %.0f, %.0f) kg m^2."),
		Thrusters.Num(), MassProperties.Mass,
		MassProperties.InertiaDiagonal.X, MassProperties.InertiaDiagonal.Y,
		MassProperties.InertiaDiagonal.Z);

	return true;
}

StarForge::FRigidBodyState USFThrusterArrayComponent::ReadState() const
{
	StarForge::FRigidBodyState State;

	const UPrimitiveComponent* Body = ResolveBody();
	if (!Body)
	{
		return State;
	}

	const FTransform ShipToWorld = Body->GetComponentTransform();

	// Velocities come back in world space and centimetres. The control loop thinks
	// in the ship own axes and in metres, because that is where the thrusters are.
	State.LinearVelocity =
		ShipToWorld.InverseTransformVectorNoScale(Body->GetPhysicsLinearVelocity())
		/ StarForge::Units::CmPerMetre;

	// Radians are unitless, so this one only changes frame.
	State.AngularVelocity =
		ShipToWorld.InverseTransformVectorNoScale(Body->GetPhysicsAngularVelocityInRadians());

	return State;
}

void USFThrusterArrayComponent::ApplyDemand(const StarForge::FWrench& DemandShipFrame, double DeltaSeconds)
{
	if (!bReady)
	{
		return;
	}

	UPrimitiveComponent* Body = ResolveBody();
	if (!Body)
	{
		return;
	}

	LastDemand = DemandShipFrame;
	Allocator.Solve(DemandShipFrame, TargetThrottles);

	// Spec 5: a thruster spools, it does not snap. Without this the ship changes
	// direction on the exact frame the stick moves, which reads as weightless no
	// matter how much mass the maths was given.
	for (int32 Index = 0; Index < LastThrottles.Num(); ++Index)
	{
		const double Tau = ResponseTimes.IsValidIndex(Index) ? ResponseTimes[Index] : 0.0;

		if (Tau <= UE_DOUBLE_SMALL_NUMBER || DeltaSeconds <= 0.0)
		{
			LastThrottles[Index] = TargetThrottles[Index];
			continue;
		}

		// First-order lag: one ResponseTime covers about 63 per cent of a step. The
		// clamp matters at low frame rates, where DeltaSeconds can exceed the time
		// constant and an unclamped factor would overshoot and oscillate.
		const double Alpha = FMath::Clamp(DeltaSeconds / Tau, 0.0, 1.0);
		LastThrottles[Index] += (TargetThrottles[Index] - LastThrottles[Index]) * Alpha;
	}

	LastAchieved = Allocator.ComputeAchieved(LastThrottles);

	// What the thrusters actually manage, not what was asked for. Applying the
	// demand instead would give the ship thrust it does not have.
	const StarForge::FWrench Uu = StarForge::Units::WrenchToUu(LastAchieved);

	const FTransform ShipToWorld = Body->GetComponentTransform();

	// AddForce with no bone applies at the centre of mass, so it adds no torque of
	// its own; the torque below is the whole of it.
	Body->AddForce(ShipToWorld.TransformVectorNoScale(Uu.Force));
	Body->AddTorqueInRadians(ShipToWorld.TransformVectorNoScale(Uu.Torque));
}
```

- [ ] **Step 3: Повесить компонент на корабль**

В `SFTestShip.h` добавить forward-declare и поле:

```cpp
class USFThrusterArrayComponent;
```

```cpp
	USFThrusterArrayComponent* GetThrusters() const { return Thrusters; }
```

```cpp
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFThrusterArrayComponent> Thrusters;
```

В конструкторе `SFTestShip.cpp`, рядом с созданием `Frame`:

```cpp
	Thrusters = CreateDefaultSubobject<USFThrusterArrayComponent>(TEXT("Thrusters"));
```

и включение:

```cpp
#include "Ship/SFThrusterArrayComponent.h"
```

- [ ] **Step 4: Собрать и назначить ассет**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Перезапустить редактор, у экземпляра корабля на карте `L_FrameSpike` в компоненте `Thrusters` выставить `ShipData` в `DA_TestShip`, сохранить карту.

- [ ] **Step 5: Проверить сборку массива**

Запустить и посмотреть лог:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

Ожидается строка `Thruster array ready: 24 thrusters, 50000 kg, inertia (...)`. Инерция должна быть заметно ненулевой по всем трём осям — ноль означает, что тела нет и `GetBodyInertiaTensor` вернул пустое.

**Проверить раскрутку.** Выставить у одного двигателя в `DA_TestShip` `ResponseTimeSeconds` в 2.0, у остальных оставить 0.05. После Task 7 в строке газов будет видно, что он выходит на режим заметно позже прочих. Раскрутка — единственное в этом плане, что нельзя увидеть на стоящем корабле, и её легко забыть подключить, потому что поле `ResponseTime` существует с первого плана и до сих пор ничего не делало.

**Отрицательный контроль, обязательно.** Временно убрать из `DA_TestShip` все двигатели, кроме одного, и запустить снова. Ожидается `cannot span all six axes` и отсутствие строки `ready`. Без этой проверки неизвестно, работает ли отказ вообще: он выглядит одинаково и когда всё хорошо, и когда проверка не выполняется. Вернуть ассет обратно.

- [ ] **Step 6: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFThrusterArrayComponent.h Source/StarForge/Private/Ship/SFThrusterArrayComponent.cpp Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp
git commit -F - <<'MSG'
feat(ship): give the ship a thruster array that pushes its body

The component owns the allocator and knows nothing else -- not who is flying, not
what they pressed. That is what will let an autopilot, a crewman and a player share
one execution path instead of three.

Mass properties are read back from Chaos rather than recomputed from the asset.
The asset says what the ship weighs; Chaos decides how that mass is distributed and
then integrates the motion, so reading anything else would let the ship plan one
turn and perform another.

What gets applied is what the thrusters achieved, not what was demanded. Applying
the demand would hand the ship thrust it does not have, and the saturation the
solver works so hard to respect would be undone at the last step.

Throttles spool toward the solution rather than snapping to it, which is what
ResponseTime has been carrying since the maths was written without anything acting
on it. A ship that changes direction on the frame the stick moves reads as
weightless however much mass the equations were handed.
MSG
```

---

### Task 6: Замкнуть контур

Корабль перестаёт лететь по скрипту и начинает лететь по намерению, поданному из консоли. Пост пилота и настоящий ввод — план 2b; здесь важно только то, что цепочка «намерение → требование → раскладка → сила» работает целиком.

**Files:**
- Modify: `Source/StarForge/Public/Ship/SFTestShip.h`
- Modify: `Source/StarForge/Private/Ship/SFTestShip.cpp`

**Interfaces:**
- Consumes: `USFThrusterArrayComponent` (Task 5), `USFShipDataAsset` (Task 4), `StarForge::ComputeDemandedWrench` (Task 3).
- Produces: консольные переменные `sf.ShipProfile`, `sf.Intent.X/Y/Z`, `sf.Intent.Roll/Pitch/Yaw`, `sf.Intent.Assist`; `ASFTestShip::GetLastIntent()` для прибора.

**Почему переменные, а не одна команда с аргументами.** Их выставляют по одной, руками, в середине прогона, и каждая видна в консоли своим именем. Разбор аргументов здесь — лишний код, который может ошибиться молча.

- [ ] **Step 1: Завести переменные режима и намерения**

В безымянном пространстве имён `SFTestShip.cpp`:

```cpp
	/**
	 * Which way the ship is driven. On, it repeats the scripted checkpoint profile
	 * with commanded velocities -- the motion the drift figures were taken against.
	 * Off, it flies on thrust from the intent below.
	 *
	 * Defaults on, so the measurement rig stays the measurement rig until somebody
	 * deliberately asks it to be something else.
	 */
	TAutoConsoleVariable<int32> CVarShipProfile(
		TEXT("sf.ShipProfile"), 1,
		TEXT("1: fly the scripted checkpoint profile. 0: fly on thrust from sf.Intent.*"),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarIntentX(TEXT("sf.Intent.X"), 0.0f,
		TEXT("Linear intent along the ship forward axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentY(TEXT("sf.Intent.Y"), 0.0f,
		TEXT("Linear intent along the ship right axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentZ(TEXT("sf.Intent.Z"), 0.0f,
		TEXT("Linear intent along the ship up axis, -1 to 1."), ECVF_Default);

	// Named for what they do, stored in angular velocity order: X roll, Y pitch,
	// Z yaw. The names and the order agree here so nobody has to remember which.
	TAutoConsoleVariable<float> CVarIntentRoll(TEXT("sf.Intent.Roll"), 0.0f,
		TEXT("Roll intent about the ship X axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentPitch(TEXT("sf.Intent.Pitch"), 0.0f,
		TEXT("Pitch intent about the ship Y axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentYaw(TEXT("sf.Intent.Yaw"), 0.0f,
		TEXT("Yaw intent about the ship Z axis, -1 to 1."), ECVF_Default);

	TAutoConsoleVariable<int32> CVarIntentAssist(TEXT("sf.Intent.Assist"), 1,
		TEXT("1: the stick asks for a speed. 0: it asks for thrust."), ECVF_Default);
```

Включения:

```cpp
#include "FlightController.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFShipDataAsset.h"
```

- [ ] **Step 2: Разделить Tick на два режима**

В `SFTestShip.h` добавить хранение намерения для прибора:

```cpp
	/** What was last asked of the ship, for the readout. */
	StarForge::FFlightIntent GetLastIntent() const { return LastIntent; }
```

```cpp
	StarForge::FFlightIntent LastIntent;
```

и включение `#include "FlightController.h"`.

В `SFTestShip.cpp` перестроить `Tick` так, чтобы ускорение считалось по **настоящей** скорости тела в обоих режимах:

```cpp
void ASFTestShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= UE_SMALL_NUMBER || !Hull)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	if (CVarShipProfile.GetValueOnGameThread() != 0)
	{
		TickScriptedProfile();
	}
	else
	{
		TickFlight(DeltaSeconds);
	}

	// Measured from what the body actually did, not from what was commanded. In the
	// scripted mode the two agree; under thrust they do not, and the grav-plate
	// readout has to show the ship real acceleration or it is showing nothing.
	const FVector VelocityCms = Hull->GetPhysicsLinearVelocity();
	LinearAccelMetersPerSecSq = (VelocityCms - PreviousVelocityCms) / DeltaSeconds / 100.0;
	PreviousVelocityCms = VelocityCms;
}
```

Прежнее тело профиля переезжает в новый приватный метод целиком, **за вычетом** двух строк про `LinearAccelMetersPerSecSq` и `PreviousVelocityCms` — их теперь считает `Tick`:

```cpp
void ASFTestShip::TickScriptedProfile()
{
	const double Phase = 2.0 * UE_DOUBLE_PI * ElapsedSeconds / ProfilePeriodSeconds;

	const FVector VelocityCms(PeakSpeedCms * FMath::Sin(Phase), 0.0, 0.0);
	Hull->SetPhysicsLinearVelocity(VelocityCms);

	const FVector LocalRateRadPerSec(
		FMath::DegreesToRadians(PeakRollRateDegPerSec),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Sin(Phase * 0.5)),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Cos(Phase * 0.5)));

	Hull->SetPhysicsAngularVelocityInRadians(
		GetActorTransform().TransformVectorNoScale(LocalRateRadPerSec));
}

void ASFTestShip::TickFlight(double DeltaSeconds)
{
	if (!Thrusters || !Thrusters->IsReady() || !Thrusters->ShipData)
	{
		return;
	}

	LastIntent.LinearAxis = FVector(
		CVarIntentX.GetValueOnGameThread(),
		CVarIntentY.GetValueOnGameThread(),
		CVarIntentZ.GetValueOnGameThread());

	LastIntent.AngularAxis = FVector(
		CVarIntentRoll.GetValueOnGameThread(),
		CVarIntentPitch.GetValueOnGameThread(),
		CVarIntentYaw.GetValueOnGameThread());

	LastIntent.bFlightAssistEnabled = CVarIntentAssist.GetValueOnGameThread() != 0;

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		LastIntent,
		Thrusters->ReadState(),
		Thrusters->GetMassProperties(),
		Thrusters->ShipData->BuildLimits(),
		Thrusters->GetArrayCapability());

	Thrusters->ApplyDemand(Demand, DeltaSeconds);
}
```

Оба метода объявить приватными в `SFTestShip.h`:

```cpp
	/** Repeats the checkpoint manoeuvre by commanding velocities directly. */
	void TickScriptedProfile();

	/** Flies on thrust from the console intent. Needs the step for thruster spool-up. */
	void TickFlight(double DeltaSeconds);
```

- [ ] **Step 3: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

- [ ] **Step 4: Полетать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

В консоли:

```
sf.ShipProfile 0
sf.Intent.X 1
```

Ожидается разгон вперёд и выход на 200 м/с, после чего скорость держится. Затем:

```
sf.Intent.X 0
```

Ожидается торможение до нуля, а не выбег: с включённым ассистом отпущенная ручка просит нулевую скорость.

```
sf.Intent.X 0
sf.Intent.Roll 1
```

Ожидается выход на 180°/с крена и удержание.

**Отрицательный контроль.** Выставить `sf.Intent.X 1` при `sf.ShipProfile 1`. Ожидается, что ничего не изменится — скриптовый профиль намерение игнорирует. Если корабль отреагировал, значит режимы не разделены и всё, что меряется дальше, смешано.

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForge/Public/Ship/SFTestShip.h Source/StarForge/Private/Ship/SFTestShip.cpp
git commit -F - <<'MSG'
feat(ship): fly the ship on its own thrust from a console intent

This closes the chain the maths was built for: an intent becomes a demanded
wrench, the wrench is distributed across the thrusters, and what they achieve is
applied to the body. Nothing in that chain knows where the intent came from, which
is what will let the pilot station in plan 2b replace the console without touching
any of it.

The scripted profile stays and stays the default, because it is the motion the
drift figures were taken against and the rig has to remain able to reproduce them.

Felt acceleration is now measured from the body actual velocity rather than the
commanded one. Under scripted velocities the two agree; under thrust they do not,
and the grav-plate readout showing commanded acceleration would be showing a
number nobody is experiencing.
MSG
```

---

### Task 7: Прибор для полёта

Числа, без которых «корабль странно себя ведёт» невозможно превратить в диагноз. Отдельный HUD не заводится: показания увода нужны ровно тогда же, когда полётные, и два прибора на экране одновременно не живут.

**Files:**
- Modify: `Source/StarForge/Public/Debug/SFFrameDriftHUD.h`
- Modify: `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`

**Interfaces:**
- Consumes: `ASFTestShip::GetThrusters()`, `GetLastIntent()`; `USFThrusterArrayComponent::GetLastDemand/GetLastAchieved/GetLastThrottles/GetArrayCapability/IsReady`.
- Produces: ничего.

**Что должен показывать и почему именно это.** Требуемое против достигнутого ловит насыщение — самый частый источник «тяга есть, а корабль не идёт». Число полностью открытых газов ловит выродившуюся раскладку. Строка готовности массива обязательна: пустой полётный блок одинаково выглядит и когда всё хорошо, и когда компонент не собрался.

- [ ] **Step 1: Добавить полётный блок**

В `SFFrameDriftHUD.h` объявить метод:

```cpp
	/** Flight lines: demand against achievement, throttles, saturation. */
	void DrawFlightBlock(double& Y);
```

В `SFFrameDriftHUD.cpp` добавить включения:

```cpp
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFThrusterArrayComponent.h"
```

и увеличить `PanelHeight` с `480.0f` до `640.0f` — блок добавляет строки, а строка, упавшая ниже панели, это тот же дефект нечитаемости, ради которого панель и заводилась.

Реализация:

```cpp
void ASFFrameDriftHUD::DrawFlightBlock(double& Y)
{
	const ASFTestShip* Ship = Cast<ASFTestShip>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFTestShip::StaticClass()));

	if (!Ship || !Ship->GetThrusters())
	{
		DrawLine(TEXT("flight: no ship with a thruster array in the level"), Y, FLinearColor::Red);
		return;
	}

	const USFThrusterArrayComponent* Array = Ship->GetThrusters();

	// Said before anything else. An empty flight block looks identical whether the
	// ship is idle or the array never built, and only this line separates them.
	if (!Array->IsReady())
	{
		DrawLine(TEXT("flight: thruster array NOT built -- ship cannot fly"), Y, FLinearColor::Red);
		return;
	}

	const int32 ProfileOn =
		IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile"))
			? IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile"))->GetInt()
			: 1;

	DrawLine(FString::Printf(TEXT("flight mode: %s"),
		ProfileOn != 0 ? TEXT("scripted profile (sf.ShipProfile 1)") : TEXT("thrust (sf.ShipProfile 0)")),
		Y, ProfileOn != 0 ? FLinearColor::Yellow : FLinearColor::Green);

	const StarForge::FFlightIntent Intent = Ship->GetLastIntent();
	DrawLine(FString::Printf(TEXT("intent  lin (%.2f %.2f %.2f)  ang (%.2f %.2f %.2f)  assist %s"),
		Intent.LinearAxis.X, Intent.LinearAxis.Y, Intent.LinearAxis.Z,
		Intent.AngularAxis.X, Intent.AngularAxis.Y, Intent.AngularAxis.Z,
		Intent.bFlightAssistEnabled ? TEXT("on") : TEXT("off")), Y);

	const StarForge::FRigidBodyState State = Array->ReadState();
	DrawLine(FString::Printf(TEXT("speed   %.1f m/s   angular (%.1f %.1f %.1f) deg/s"),
		State.LinearVelocity.Size(),
		FMath::RadiansToDegrees(State.AngularVelocity.X),
		FMath::RadiansToDegrees(State.AngularVelocity.Y),
		FMath::RadiansToDegrees(State.AngularVelocity.Z)), Y);

	const StarForge::FWrench Demand = Array->GetLastDemand();
	const StarForge::FWrench Achieved = Array->GetLastAchieved();

	DrawLine(FString::Printf(TEXT("force   want (%.0f %.0f %.0f)  got (%.0f %.0f %.0f) N"),
		Demand.Force.X, Demand.Force.Y, Demand.Force.Z,
		Achieved.Force.X, Achieved.Force.Y, Achieved.Force.Z), Y);

	DrawLine(FString::Printf(TEXT("torque  want (%.0f %.0f %.0f)  got (%.0f %.0f %.0f) Nm"),
		Demand.Torque.X, Demand.Torque.Y, Demand.Torque.Z,
		Achieved.Torque.X, Achieved.Torque.Y, Achieved.Torque.Z), Y);

	// Saturation is the usual reason a ship will not do what it is told, and it is
	// invisible without this: the demand looks fine and the ship simply lags.
	int32 WideOpen = 0;
	for (double Throttle : Array->GetLastThrottles())
	{
		if (Throttle > 0.999)
		{
			++WideOpen;
		}
	}

	const int32 Count = Array->GetLastThrottles().Num();
	DrawLine(FString::Printf(TEXT("throttles %d of %d wide open%s"),
		WideOpen, Count, WideOpen > 0 ? TEXT("  -- SATURATED") : TEXT("")),
		Y, WideOpen > 0 ? FLinearColor::Yellow : FLinearColor::White);
}
```

Включение для `UGameplayStatics`:

```cpp
#include "Kismet/GameplayStatics.h"
```

Вызвать блок в конце `DrawHUD`, после всех строк увода:

```cpp
	DrawLine(TEXT(""), Y);
	DrawFlightBlock(Y);
```

- [ ] **Step 2: Собрать и проверить прибор собой же**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

Прибор проверяется сверкой с величинами, которые известны заранее:

| Действие | Что обязано показаться |
|---|---|
| `sf.ShipProfile 0`, `sf.Intent.X 1`, дождаться установившейся скорости | `speed` около 200 м/с, `force want` около нуля — ассист держит, а не толкает |
| В момент старта разгона | `force want` около массы на предел на усиление, `got` заметно меньше, `SATURATED` |
| `sf.Intent.Roll 1`, дождаться установившейся | `angular` X около 180 град/с |
| `sf.Intent.Pitch 1` вместо крена | `angular` **Y** около 120 град/с, X около нуля |

Последняя строка таблицы — та же проверка порядка осей, что и в головном тесте Task 3, но уже на настоящем корабле. Если крен и тангаж поменялись местами, это видно здесь и больше нигде.

- [ ] **Step 3: Коммит**

```bash
git add Source/StarForge/Public/Debug/SFFrameDriftHUD.h Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp
git commit -F - <<'MSG'
feat(debug): show what the ship was asked for against what it managed

Almost nothing in this plan is caught by a headless test, so a defect here shows
up as the ship behaving oddly rather than as a red line. Demand against
achievement is what turns that into a diagnosis: saturation is the usual reason a
ship will not do what it is told, and without these numbers the demand looks
correct and the ship merely lags.

The array-not-built line comes before any of it, for the reason the drift readout
already learned the hard way: an empty flight block looks the same whether the
ship is sitting still or the component never assembled.

The lines join the existing readout rather than forming a second HUD, because the
drift figures matter during flight for exactly the same reasons they mattered
standing still, and two HUDs cannot both be on screen.
MSG
```

---

## Точка проверки 3

После Task 7 корабль летает под собственной тягой, и на нём стоит человек.

Проверить перед тем, как писать план 2b:

- тридцать головных тестов проходят из консоли, без редактора;
- `StarForgeCore` по-прежнему зависит только от `Core` и `CoreUObject`;
- сборка без предупреждений;
- **увод пассажира на физическом корабле в пределах бюджета §10**, при `measured` не меньше 55 секунд, с записанным бланком — это главный результат плана, и он в Task 1;
- корабль выходит на 200 м/с и держит; отпущенная ручка тормозит его до нуля;
- корабль выходит на 180°/с крена и на 120°/с тангажа, и не путает их местами;
- отрицательный контроль на вырожденную раскладку даёт отказ сборки массива, а не молчаливый полёт;
- двигатель с увеличенным `ResponseTimeSeconds` выходит на режим заметно позже прочих, то есть раскрутка из §5 действительно подключена.

**Отдельно проверить то, чего этот план сознательно не делал:** пассажир при крене 180°/с у стены **не** падает, потому что механика сбивания с ног ещё не включена. Если он падает — значит что-то включило её раньше времени, и замер увода из Task 1 надо перечитать заново.

## Что дальше

План 2b: пост пилота (`UShipStationComponent`), интерфейс `IStationOperator`, маршрутизация ввода и сбивание с ног при превышении порога гравиплит.

Требования к нему, зафиксированные здесь:

1. **Ввод аналоговый по осям.** Управление проектируется под физические манипуляторы, поэтому ось не может быть «нажато или нет»: половина хода обязана давать половину величины. Именно ради этого в Task 3 выключенный ассист стал пропорционален возможностям набора двигателей.
2. **Сбивание с ног включается только после того, как замер увода принят.** Механика и прибор занимают одно и то же место: прибор меряет неподвижность пассажира, механика его роняет.
3. **Контур управления переезжает из `ASFTestShip::Tick` в пост пилота.** Здесь он живёт в тестовом стенде, потому что оператора ещё нет.
