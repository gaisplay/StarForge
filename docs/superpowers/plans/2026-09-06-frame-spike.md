# Фундамент и замер системы отсчёта — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Поставить модуль `StarForgeCore` с работающим headless-каналом тестов и собрать тех-тест, который отвечает на главный вопрос дизайна: можно ли ходить внутри корабля, летящего на предельных скоростях, разложив движение на телепорт и свип.

**Architecture:** Вся математика систем отсчёта живёт в `StarForgeCore` без зависимости от `Engine` и покрыта юнит-тестами. В игровом модуле `USFShipboardMovementComponent` наследует `UCharacterMovementComponent` и переопределяет `UpdateBasedMovement`: перенос системой отсчёта выполняется телепортом без свипа, собственная ходьба остаётся штатной. Каждый пассажир хранит свой последний известный трансформ корабля, поэтому порядок тиков не влияет на результат.

**Tech Stack:** Unreal Engine 5.8.2, C++, Chaos, Automation Test Framework, MCP-канал в редактор (`127.0.0.1:8000`).

**Spec:** [docs/superpowers/specs/2026-09-06-starforge-vertical-slice-design.md](../specs/2026-09-06-starforge-vertical-slice-design.md)

## Global Constraints

- Движок — **Unreal Engine 5.8.2**, установлен в `C:\Program Files\Epic Games\UE_5.8`.
- `StarForgeCore` зависит **только** от `Core` и `CoreUObject`. Добавление `Engine` в этот модуль — ошибка, ломающая смысл разделения.
- Внутри `StarForgeCore` физические величины — в СИ. Геометрические преобразования единиц не имеют и работают в тех единицах, что передал вызывающий (игра передаёт сантиметры).
- Бюджет скорости из §7 спеки: маневрирование **200 м/с**, форсаж **400 м/с**, тангаж и рысканье **120°/с**, крен **180°/с**.
- Порог гравиплит из §6 спеки: **2 g = 19.62 м/с²**.
- Допуск на увод пассажира: **не более 1 см за минуту** при отсутствии ввода.
- В 5.8 `EAutomationTestFlags` — это `enum class`, а маска контекста вынесена в отдельную константу `EAutomationTestFlags_ApplicationContextMask`. Синтаксис `EAutomationTestFlags::ApplicationContextMask` из примеров под 5.3 **не соберётся**.
- В 5.8 `UCharacterMovementComponent::GetMovementBase()` помечен `UE_DEPRECATED(5.8)`. Использовать `GetMovementBaseObject()`.
- Коммит после каждой задачи. Сообщения на английском, тело — почему, а не что.

---

## Область этого плана

План покрывает **этапы 0 и 1** спеки (§13) — фундамент и замер.

Этапы 2–4 намеренно оставлены следующему плану. Причина в том, что этап 1 заканчивается развилкой: его результат либо подтверждает разложение движения, либо переводит проект на план Б из §12. Детальные задачи для этапов 2–4, написанные до этого ответа, частично отправились бы в мусор.

Этап 2 (симуляция двигателей) от результата замера не зависит и может планироваться параллельно.

---

## Структура файлов

```
Source/
  StarForgeCore/                          новый модуль, без Engine
    StarForgeCore.Build.cs
    Public/
      ShipFrameMath.h                     преобразования систем отсчёта
    Private/
      StarForgeCoreModule.cpp             точка входа модуля
      ShipFrameMath.cpp
      Tests/
        ShipFrameMathTest.cpp             юнит-тесты, headless

  StarForge/                              игровой модуль
    StarForge.Build.cs                    правка: зависимость от StarForgeCore
    Public/
      Ship/
        SFShipFrameComponent.h            фасад системы отсчёта
        SFTestShip.h                      куб-корабль с профилем манёвра
      Player/
        SFCharacter.h                     пассажир с камерой от первого лица
        SFShipboardMovementComponent.h    разложение движения
      Debug/
        SFFrameDriftHUD.h                 показания замера
      SFSpikeGameMode.h                   сборка сцены без ассетов
    Private/                              зеркально
```

Разделение по ответственности: `ShipFrameMath` знает только про трансформы и ничего про движок; `SFShipFrameComponent` — единственный носитель «где корабль сейчас»; `SFShipboardMovementComponent` — единственное место, где применяется разложение; `SFTestShip` создаёт худший случай; HUD только читает и рисует.

---

### Task 1: Модуль `StarForgeCore` и перенос пассажира

Модуль создаётся вместе с первой полезной функцией, потому что пустой модуль нечем проверить. Задача закрыта, когда тест прошёл в консоли без открытия редактора.

**Files:**
- Create: `Source/StarForgeCore/StarForgeCore.Build.cs`
- Create: `Source/StarForgeCore/Private/StarForgeCoreModule.cpp`
- Create: `Source/StarForgeCore/Public/ShipFrameMath.h`
- Create: `Source/StarForgeCore/Private/ShipFrameMath.cpp`
- Create: `Source/StarForgeCore/Private/Tests/ShipFrameMathTest.cpp`
- Modify: `StarForge.uproject`

**Interfaces:**
- Consumes: ничего.
- Produces: `StarForge::ShipFrame::WorldToLocal(const FTransform&, const FTransform&) -> FTransform`, `StarForge::ShipFrame::LocalToWorld(const FTransform&, const FTransform&) -> FTransform`, `StarForge::ShipFrame::CarryOccupant(const FTransform& PrevShipWorld, const FTransform& CurrShipWorld, const FTransform& OccupantWorld) -> FTransform`.

- [ ] **Step 1: Создать файлы модуля**

`Source/StarForgeCore/StarForgeCore.Build.cs`:

```csharp
using UnrealBuildTool;

public class StarForgeCore : ModuleRules
{
	public StarForgeCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Intentionally no "Engine" dependency: this module must stay unit-testable
		// without spinning up an editor or a world.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject"
		});
	}
}
```

`Source/StarForgeCore/Private/StarForgeCoreModule.cpp`:

```cpp
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, StarForgeCore);
```

- [ ] **Step 2: Зарегистрировать модуль в проекте**

В `StarForge.uproject` заменить массив `"Modules"` целиком на:

```json
	"Modules": [
		{
			"Name": "StarForgeCore",
			"Type": "Runtime",
			"LoadingPhase": "Default"
		},
		{
			"Name": "StarForge",
			"Type": "Runtime",
			"LoadingPhase": "Default",
			"AdditionalDependencies": [
				"Engine"
			]
		}
	],
```

`StarForgeCore` идёт первым, чтобы загружаться до зависящего от него игрового модуля.

- [ ] **Step 3: Написать падающий тест**

`Source/StarForgeCore/Private/Tests/ShipFrameMathTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "ShipFrameMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameCarryTranslationTest,
	"StarForge.Core.ShipFrame.CarryTranslation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameCarryTranslationTest::RunTest(const FString& Parameters)
{
	const FTransform PrevShip(FQuat::Identity, FVector(0.0, 0.0, 0.0));
	const FTransform CurrShip(FQuat::Identity, FVector(1000.0, 0.0, 0.0));
	const FTransform Occupant(FQuat::Identity, FVector(50.0, 0.0, 0.0));

	const FTransform Carried =
		StarForge::ShipFrame::CarryOccupant(PrevShip, CurrShip, Occupant);

	TestTrue(TEXT("occupant travels with the ship's translation"),
		Carried.GetLocation().Equals(FVector(1050.0, 0.0, 0.0), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameCarryRotationTest,
	"StarForge.Core.ShipFrame.CarryRotation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameCarryRotationTest::RunTest(const FString& Parameters)
{
	// Ship yaws 90 degrees about its own pivot; an occupant one metre ahead of the
	// pivot must end up one metre to the left of it.
	const FTransform PrevShip(FQuat::Identity, FVector::ZeroVector);
	const FTransform CurrShip(FRotator(0.0, 90.0, 0.0).Quaternion(), FVector::ZeroVector);
	const FTransform Occupant(FQuat::Identity, FVector(100.0, 0.0, 0.0));

	const FTransform Carried =
		StarForge::ShipFrame::CarryOccupant(PrevShip, CurrShip, Occupant);

	TestTrue(TEXT("occupant is swept around the ship's pivot"),
		Carried.GetLocation().Equals(FVector(0.0, 100.0, 0.0), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameLocalInvarianceTest,
	"StarForge.Core.ShipFrame.LocalInvariance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameLocalInvarianceTest::RunTest(const FString& Parameters)
{
	// The whole point of the carry: whatever the ship does, the occupant's
	// ship-local transform must come out unchanged.
	const FTransform PrevShip(FRotator(10.0, 20.0, 30.0).Quaternion(), FVector(500.0, -200.0, 75.0));
	const FTransform CurrShip(FRotator(-40.0, 155.0, 90.0).Quaternion(), FVector(-9000.0, 4000.0, 120.0));
	const FTransform Occupant(FRotator(0.0, 45.0, 0.0).Quaternion(), FVector(560.0, -150.0, 160.0));

	const FTransform LocalBefore = StarForge::ShipFrame::WorldToLocal(PrevShip, Occupant);
	const FTransform Carried = StarForge::ShipFrame::CarryOccupant(PrevShip, CurrShip, Occupant);
	const FTransform LocalAfter = StarForge::ShipFrame::WorldToLocal(CurrShip, Carried);

	TestTrue(TEXT("ship-local position is preserved"),
		LocalAfter.GetLocation().Equals(LocalBefore.GetLocation(), 0.01));
	TestTrue(TEXT("ship-local rotation is preserved"),
		LocalAfter.GetRotation().Equals(LocalBefore.GetRotation(), 0.0001));

	return true;
}
```

- [ ] **Step 4: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: ошибка компиляции `Cannot open include file: 'ShipFrameMath.h'`. Это и есть красный тест — заголовка ещё нет.

- [ ] **Step 5: Реализовать математику**

`Source/StarForgeCore/Public/ShipFrameMath.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

/**
 * Reference-frame conversions for occupants riding a moving ship.
 *
 * These functions are unit-agnostic: they operate on whatever length unit the
 * caller passes in consistently. The game passes centimetres, matching Unreal.
 */
namespace StarForge::ShipFrame
{
	/** Occupant's transform expressed in the ship's frame. */
	STARFORGECORE_API FTransform WorldToLocal(const FTransform& ShipWorld, const FTransform& OccupantWorld);

	/** Occupant's ship-local transform expressed in world space. */
	STARFORGECORE_API FTransform LocalToWorld(const FTransform& ShipWorld, const FTransform& OccupantLocal);

	/**
	 * World transform an occupant must take so that its ship-local transform is
	 * unchanged after the ship moves from PrevShipWorld to CurrShipWorld.
	 */
	STARFORGECORE_API FTransform CarryOccupant(
		const FTransform& PrevShipWorld,
		const FTransform& CurrShipWorld,
		const FTransform& OccupantWorld);
}
```

`Source/StarForgeCore/Private/ShipFrameMath.cpp`:

```cpp
#include "ShipFrameMath.h"

namespace StarForge::ShipFrame
{
	FTransform WorldToLocal(const FTransform& ShipWorld, const FTransform& OccupantWorld)
	{
		return OccupantWorld.GetRelativeTransform(ShipWorld);
	}

	FTransform LocalToWorld(const FTransform& ShipWorld, const FTransform& OccupantLocal)
	{
		return OccupantLocal * ShipWorld;
	}

	FTransform CarryOccupant(
		const FTransform& PrevShipWorld,
		const FTransform& CurrShipWorld,
		const FTransform& OccupantWorld)
	{
		return LocalToWorld(CurrShipWorld, WorldToLocal(PrevShipWorld, OccupantWorld));
	}
}
```

- [ ] **Step 6: Собрать и прогнать тесты в консоли**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается: в выводе три пройденных теста и `Test Completed. Result={Passed}`. Если консоль пишет `Test Completed. Result={Failed}` — читать строку с `Error:` выше, там указано, какое сравнение не сошлось.

Этот шаг важнее своего результата: он подтверждает, что канал «написал тест → увидел ответ, не открывая редактор» работает. Дальше на нём держится весь этап 2.

- [ ] **Step 7: Коммит**

```bash
git add Source/StarForgeCore StarForge.uproject
git commit -m "feat(core): add StarForgeCore module with reference-frame math

Flight and frame math needs to be verifiable without launching the editor:
an allocator that leaks a fraction of a percent of parasitic torque is
invisible in play and obvious in a test. Keeping this module free of an
Engine dependency is what makes that possible.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 2: Ощущаемое ускорение и порог гравиплит

Реализует §6 спеки: предел компенсации, за которым манёвр сбивает с ног. В замере используется, чтобы видеть, когда сцена вышла за проектные рамки.

**Files:**
- Modify: `Source/StarForgeCore/Public/ShipFrameMath.h`
- Modify: `Source/StarForgeCore/Private/ShipFrameMath.cpp`
- Modify: `Source/StarForgeCore/Private/Tests/ShipFrameMathTest.cpp`

**Interfaces:**
- Consumes: пространство имён `StarForge::ShipFrame` из Task 1.
- Produces: `StarForge::ShipFrame::GravPlateLimitMetersPerSecSq` (константа `double`), `StarForge::ShipFrame::ComputeFeltAcceleration(const FVector& ShipLinearAccel, const FVector& ShipAngularVelocityRadPerSec, const FVector& OccupantLocalOffset) -> FVector`, `StarForge::ShipFrame::ExceedsGravPlateLimit(const FVector& FeltAccelMetersPerSecSq) -> bool`.

- [ ] **Step 1: Написать падающие тесты**

Дописать в конец `Source/StarForgeCore/Private/Tests/ShipFrameMathTest.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameCentripetalTest,
	"StarForge.Core.ShipFrame.Centripetal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameCentripetalTest::RunTest(const FString& Parameters)
{
	// Spinning at 2 rad/s about Z, an occupant 5 m out along X feels
	// omega^2 * r = 20 m/s^2 pulling him outward, i.e. -20 along X.
	const FVector NoLinearAccel = FVector::ZeroVector;
	const FVector Spin(0.0, 0.0, 2.0);
	const FVector Offset(5.0, 0.0, 0.0);

	const FVector Felt =
		StarForge::ShipFrame::ComputeFeltAcceleration(NoLinearAccel, Spin, Offset);

	TestTrue(TEXT("centripetal term points back toward the axis"),
		Felt.Equals(FVector(-20.0, 0.0, 0.0), 0.001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameGravPlateLimitTest,
	"StarForge.Core.ShipFrame.GravPlateLimit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameGravPlateLimitTest::RunTest(const FString& Parameters)
{
	// The design fixes the compensation ceiling at 2 g. Just under it the
	// occupant keeps his footing; just over it he does not.
	TestFalse(TEXT("19.0 m/s^2 stays within compensation"),
		StarForge::ShipFrame::ExceedsGravPlateLimit(FVector(19.0, 0.0, 0.0)));

	TestTrue(TEXT("20.0 m/s^2 exceeds compensation"),
		StarForge::ShipFrame::ExceedsGravPlateLimit(FVector(20.0, 0.0, 0.0)));

	return true;
}
```

- [ ] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: `'ComputeFeltAcceleration': is not a member of 'StarForge::ShipFrame'`.

- [ ] **Step 3: Реализовать**

Дописать в `Source/StarForgeCore/Public/ShipFrameMath.h` внутрь namespace:

```cpp
	/**
	 * Ceiling of the ship's gravity-plate compensation, in m/s^2.
	 * Below it an occupant walks normally; above it he is knocked off his feet.
	 * Fixed at 2 g by the vertical slice design.
	 */
	inline constexpr double GravPlateLimitMetersPerSecSq = 19.62;

	/**
	 * Acceleration felt by an occupant standing at OccupantLocalOffset from the
	 * ship's pivot. Sums the ship's own linear acceleration with the centripetal
	 * term produced by its rotation.
	 *
	 * All three arguments and the result use metres and seconds.
	 */
	STARFORGECORE_API FVector ComputeFeltAcceleration(
		const FVector& ShipLinearAccel,
		const FVector& ShipAngularVelocityRadPerSec,
		const FVector& OccupantLocalOffset);

	/** True when the felt acceleration is beyond what the gravity plates absorb. */
	STARFORGECORE_API bool ExceedsGravPlateLimit(const FVector& FeltAccelMetersPerSecSq);
```

Дописать в `Source/StarForgeCore/Private/ShipFrameMath.cpp` внутрь namespace:

```cpp
	FVector ComputeFeltAcceleration(
		const FVector& ShipLinearAccel,
		const FVector& ShipAngularVelocityRadPerSec,
		const FVector& OccupantLocalOffset)
	{
		const FVector& Omega = ShipAngularVelocityRadPerSec;
		const FVector Centripetal =
			FVector::CrossProduct(Omega, FVector::CrossProduct(Omega, OccupantLocalOffset));

		return ShipLinearAccel + Centripetal;
	}

	bool ExceedsGravPlateLimit(const FVector& FeltAccelMetersPerSecSq)
	{
		return FeltAccelMetersPerSecSq.Size() > GravPlateLimitMetersPerSecSq;
	}
```

- [ ] **Step 4: Собрать и прогнать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается: пять пройденных тестов.

- [ ] **Step 5: Коммит**

```bash
git add Source/StarForgeCore
git commit -m "feat(core): add felt acceleration and grav-plate ceiling

The 2 g limit is what forces a pilot into his seat during combat manoeuvres,
which in turn is what makes crew stations a physical requirement rather than
a formality. Encoding it as one constant keeps that coupling explicit.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 3: Фасад системы отсчёта

`USFShipFrameComponent` — единственный носитель ответа на вопрос «где корабль сейчас». Он ничего не двигает: движением занимается Task 5. Такое разделение убирает вопрос порядка тиков.

**Files:**
- Modify: `Source/StarForge/StarForge.Build.cs`
- Create: `Source/StarForge/Public/Ship/SFShipFrameComponent.h`
- Create: `Source/StarForge/Private/Ship/SFShipFrameComponent.cpp`

**Interfaces:**
- Consumes: `StarForge::ShipFrame::WorldToLocal`, `LocalToWorld`, `CarryOccupant` из Task 1.
- Produces: класс `USFShipFrameComponent` с методами `GetFrameTransform() const -> FTransform`, `WorldToLocal(const FTransform&) const -> FTransform`, `LocalToWorld(const FTransform&) const -> FTransform`, `CarryOccupant(const FTransform& PrevFrame, const FTransform& OccupantWorld) const -> FTransform`.

- [ ] **Step 1: Подключить модуль**

В `Source/StarForge/StarForge.Build.cs` заменить блок `PublicDependencyModuleNames` на:

```csharp
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"StarForgeCore"
		});
```

- [ ] **Step 2: Написать компонент**

`Source/StarForge/Public/Ship/SFShipFrameComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFShipFrameComponent.generated.h"

/**
 * Marks its owner as a reference frame that occupants can ride.
 *
 * This component deliberately moves nothing. It answers "where is this frame
 * right now" and converts between world and frame coordinates; carrying
 * occupants is the movement component's job. Keeping the two apart means no
 * tick-order dependency exists between a ship and the people standing in it.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFShipFrameComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFShipFrameComponent();

	/** This frame's transform in world space, sampled at the moment of the call. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform GetFrameTransform() const;

	/** Expresses a world transform in this frame's coordinates. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform WorldToLocal(const FTransform& World) const;

	/** Expresses a frame-local transform in world space. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform LocalToWorld(const FTransform& Local) const;

	/**
	 * World transform an occupant must take to keep its frame-local transform
	 * after this frame moved from PrevFrame to where it is now.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform CarryOccupant(const FTransform& PrevFrame, const FTransform& OccupantWorld) const;

	/** Angular velocity of this frame in radians per second, world space. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetAngularVelocityRadPerSec() const;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Previous sample, used only to derive angular velocity for readouts. */
	FTransform LastSampledTransform;
	FVector AngularVelocityRadPerSec = FVector::ZeroVector;
	bool bHasSample = false;
};
```

`Source/StarForge/Private/Ship/SFShipFrameComponent.cpp`:

```cpp
#include "Ship/SFShipFrameComponent.h"

#include "GameFramework/Actor.h"
#include "ShipFrameMath.h"

USFShipFrameComponent::USFShipFrameComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

FTransform USFShipFrameComponent::GetFrameTransform() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform() : FTransform::Identity;
}

FTransform USFShipFrameComponent::WorldToLocal(const FTransform& World) const
{
	return StarForge::ShipFrame::WorldToLocal(GetFrameTransform(), World);
}

FTransform USFShipFrameComponent::LocalToWorld(const FTransform& Local) const
{
	return StarForge::ShipFrame::LocalToWorld(GetFrameTransform(), Local);
}

FTransform USFShipFrameComponent::CarryOccupant(const FTransform& PrevFrame, const FTransform& OccupantWorld) const
{
	return StarForge::ShipFrame::CarryOccupant(PrevFrame, GetFrameTransform(), OccupantWorld);
}

FVector USFShipFrameComponent::GetAngularVelocityRadPerSec() const
{
	return AngularVelocityRadPerSec;
}

void USFShipFrameComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const FTransform Current = GetFrameTransform();

	if (bHasSample && DeltaTime > UE_SMALL_NUMBER)
	{
		const FQuat Delta = Current.GetRotation() * LastSampledTransform.GetRotation().Inverse();

		FVector Axis;
		double Angle;
		Delta.ToAxisAndAngle(Axis, Angle);

		AngularVelocityRadPerSec = Axis * (Angle / DeltaTime);
	}

	LastSampledTransform = Current;
	bHasSample = true;
}
```

- [ ] **Step 3: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: сборка без ошибок.

- [ ] **Step 4: Коммит**

```bash
git add Source/StarForge
git commit -m "feat(ship): add ship frame component as the coordinate facade

Every implementation of shipboard movement we might end up with -- the base
teleport decomposition, an interior pocket, or a Mover mode -- swaps out
behind this component. Gameplay above it only ever speaks frame-local
coordinates, so a change of plan does not reach it.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 4: Куб-корабль с профилем манёвра

Корабль на этом этапе кинематический, а не физический: замер должен воспроизводиться от запуска к запуску, а физика Chaos это ломает. Профиль сразу выкручен на предельные значения из §7 спеки, потому что тест-кейс — худший случай, а не типичный.

**Files:**
- Create: `Source/StarForge/Public/Ship/SFTestShip.h`
- Create: `Source/StarForge/Private/Ship/SFTestShip.cpp`

**Interfaces:**
- Consumes: `USFShipFrameComponent` из Task 3.
- Produces: класс `ASFTestShip` со свойствами `PeakSpeedCms`, `PeakPitchYawRateDegPerSec`, `PeakRollRateDegPerSec`, `ProfilePeriodSeconds`, методами `GetFrameComponent() const -> USFShipFrameComponent*`, `GetInteriorSpawnWorldLocation() const -> FVector`, `GetLinearAccelMetersPerSecSq() const -> FVector`.

- [ ] **Step 1: Написать актор**

`Source/StarForge/Public/Ship/SFTestShip.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTestShip.generated.h"

class USFShipFrameComponent;
class UStaticMeshComponent;

/**
 * Throwaway measurement rig: a boxed room that flies a scripted manoeuvre at
 * the design's speed and rotation ceilings.
 *
 * Motion is kinematic rather than simulated, because the point of the rig is a
 * repeatable worst case. Chaos gets wired in during the flight phase, once the
 * question this rig exists to answer has been answered.
 */
UCLASS()
class STARFORGE_API ASFTestShip : public AActor
{
	GENERATED_BODY()

public:
	ASFTestShip();

	virtual void Tick(float DeltaSeconds) override;

	/** Boost ceiling from the design, converted to Unreal units: 400 m/s. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double PeakSpeedCms = 40000.0;

	/** Pitch and yaw ceiling from the design. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double PeakPitchYawRateDegPerSec = 120.0;

	/** Roll ceiling from the design. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double PeakRollRateDegPerSec = 180.0;

	/** Seconds for one full sweep of the manoeuvre profile. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double ProfilePeriodSeconds = 8.0;

	/** Interior half-extent in centimetres, measured from the pivot. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double InteriorHalfExtentCm = 400.0;

	USFShipFrameComponent* GetFrameComponent() const { return Frame; }

	/** Where a passenger should be placed so he starts standing on the floor. */
	FVector GetInteriorSpawnWorldLocation() const;

	/** This tick's linear acceleration in m/s^2, for the grav-plate readout. */
	FVector GetLinearAccelMetersPerSecSq() const { return LinearAccelMetersPerSecSq; }

	/** This tick's linear speed in m/s, so the readout shows where in the profile we are. */
	double GetSpeedMetersPerSec() const { return PreviousVelocityCms.Size() / 100.0; }

private:
	/** Builds floor and four walls out of engine cubes, so the rig needs no content. */
	UStaticMeshComponent* AddBoxPanel(FName Name, const FVector& RelativeLocation, const FVector& RelativeScale);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFShipFrameComponent> Frame;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Panels;

	double ElapsedSeconds = 0.0;
	FVector PreviousVelocityCms = FVector::ZeroVector;
	FVector LinearAccelMetersPerSecSq = FVector::ZeroVector;
};
```

`Source/StarForge/Private/Ship/SFTestShip.cpp`:

```cpp
#include "Ship/SFTestShip.h"

#include "Components/StaticMeshComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** One engine cube is 100 cm on a side, so scale equals metres. */
	constexpr double CubeEdgeCm = 100.0;
	constexpr double PanelThickness = 0.2;
}

ASFTestShip::ASFTestShip()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));

	const double Span = InteriorHalfExtentCm / CubeEdgeCm * 2.0;
	const double Half = InteriorHalfExtentCm;

	AddBoxPanel(TEXT("Floor"), FVector(0.0, 0.0, -Half), FVector(Span, Span, PanelThickness));
	AddBoxPanel(TEXT("Ceiling"), FVector(0.0, 0.0, Half), FVector(Span, Span, PanelThickness));
	AddBoxPanel(TEXT("WallXPos"), FVector(Half, 0.0, 0.0), FVector(PanelThickness, Span, Span));
	AddBoxPanel(TEXT("WallXNeg"), FVector(-Half, 0.0, 0.0), FVector(PanelThickness, Span, Span));
	AddBoxPanel(TEXT("WallYPos"), FVector(0.0, Half, 0.0), FVector(Span, PanelThickness, Span));
	AddBoxPanel(TEXT("WallYNeg"), FVector(0.0, -Half, 0.0), FVector(Span, PanelThickness, Span));
}

UStaticMeshComponent* ASFTestShip::AddBoxPanel(FName Name, const FVector& RelativeLocation, const FVector& RelativeScale)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Panel->SetupAttachment(Root);
	Panel->SetRelativeLocation(RelativeLocation);
	Panel->SetRelativeScale3D(RelativeScale);
	Panel->SetMobility(EComponentMobility::Movable);
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Panel->SetCollisionObjectType(ECC_WorldDynamic);
	Panel->SetCollisionResponseToAllChannels(ECR_Block);

	if (CubeMesh.Succeeded())
	{
		Panel->SetStaticMesh(CubeMesh.Object);
	}

	Panels.Add(Panel);
	return Panel;
}

FVector ASFTestShip::GetInteriorSpawnWorldLocation() const
{
	// Just above the floor plate, on the pivot.
	const FVector Local(0.0, 0.0, -InteriorHalfExtentCm + 100.0);
	return GetActorTransform().TransformPosition(Local);
}

void ASFTestShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	const double Phase = 2.0 * UE_DOUBLE_PI * ElapsedSeconds / ProfilePeriodSeconds;

	// A full-amplitude sinusoid along X: speed peaks at the design ceiling and
	// acceleration reverses hardest at the zero crossings, which is where a
	// passenger is most likely to be thrown or left behind.
	const FVector VelocityCms(PeakSpeedCms * FMath::Sin(Phase), 0.0, 0.0);
	AddActorWorldOffset(VelocityCms * DeltaSeconds, /*bSweep=*/false);

	LinearAccelMetersPerSecSq = (VelocityCms - PreviousVelocityCms) / DeltaSeconds / 100.0;
	PreviousVelocityCms = VelocityCms;

	// All three angular axes loaded at once, out of phase, so no accidental
	// symmetry hides a bug.
	const FRotator RatePerSecond(
		PeakPitchYawRateDegPerSec * FMath::Sin(Phase * 0.5),
		PeakPitchYawRateDegPerSec * FMath::Cos(Phase * 0.5),
		PeakRollRateDegPerSec);

	AddActorLocalRotation((RatePerSecond * DeltaSeconds).Quaternion());
}
```

- [ ] **Step 2: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: сборка без ошибок.

- [ ] **Step 3: Коммит**

```bash
git add Source/StarForge
git commit -m "feat(ship): add kinematic test rig flying the design's ceilings

Kinematic rather than simulated on purpose: the rig has to produce the same
worst case on every run, and Chaos would not. It also builds its own room out
of engine cubes so the measurement depends on no imported content.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 5: Разложение движения

Ядро замера. Переопределяет `UpdateBasedMovement`, чтобы перенос системой отсчёта выполнялся телепортом. Собственная ходьба не трогается вовсе и остаётся штатной.

Каждый пассажир хранит **свой** последний известный трансформ корабля. Общего состояния нет, поэтому порядок тиков не влияет на результат.

**Files:**
- Create: `Source/StarForge/Public/Player/SFShipboardMovementComponent.h`
- Create: `Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp`

**Interfaces:**
- Consumes: `USFShipFrameComponent` из Task 3.
- Produces: класс `USFShipboardMovementComponent` с методами `SetShipFrame(USFShipFrameComponent*)`, `GetShipFrame() const -> USFShipFrameComponent*`, `GetDriftCm() const -> double`, `GetPeakDriftCm() const -> double`, `GetPeakJitterCm() const -> double`, `GetLocalLocation() const -> FVector`.

- [ ] **Step 1: Написать компонент движения**

`Source/StarForge/Public/Player/SFShipboardMovementComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SFShipboardMovementComponent.generated.h"

class USFShipFrameComponent;

/**
 * Character movement that survives standing inside a fast, hard-turning ship.
 *
 * A character's motion in one frame is two unrelated things. Being carried by
 * the ship is zero motion relative to the ship, so nothing along that path can
 * legitimately block it -- stock UE sweeps it anyway and collides with the
 * ship's own hull, which at 400 m/s is a three-metre line through a wall. That
 * part is done as a teleport here. Walking is a few centimetres against
 * momentarily static geometry, and is left to the stock swept path untouched.
 */
UCLASS()
class STARFORGE_API USFShipboardMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USFShipboardMovementComponent();

	/** Starts riding a frame, or stops when passed null. */
	UFUNCTION(BlueprintCallable, Category = "StarForge|Frame")
	void SetShipFrame(USFShipFrameComponent* NewFrame);

	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	USFShipFrameComponent* GetShipFrame() const { return ShipFrame.Get(); }

	/** Ship-local displacement accumulated while standing still, in centimetres. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetDriftCm() const { return DriftCm; }

	/** Worst drift seen since the frame was set, in centimetres. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetPeakDriftCm() const { return PeakDriftCm; }

	/**
	 * Largest single-tick jump of the ship-local position while standing still,
	 * in centimetres. Drift and jitter fail differently: a rig can hold its
	 * average position perfectly and still shake hard enough to be unusable.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetPeakJitterCm() const { return PeakJitterCm; }

	/** Current position in the ship's coordinates, in centimetres. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetLocalLocation() const;

	virtual void UpdateBasedMovement(float DeltaSeconds) override;

private:
	void CarryWithFrame();
	void UpdateDriftMeasurement();

	TWeakObjectPtr<USFShipFrameComponent> ShipFrame;

	/** This occupant's own view of where the ship was last tick. */
	FTransform LastFrameTransform = FTransform::Identity;

	/** Baseline the drift is measured against, re-taken whenever the player moves. */
	FVector DriftReferenceLocal = FVector::ZeroVector;

	/** Previous tick's ship-local position, for the per-tick jitter figure. */
	FVector LastLocalLocation = FVector::ZeroVector;

	double DriftCm = 0.0;
	double PeakDriftCm = 0.0;
	double PeakJitterCm = 0.0;
};
```

`Source/StarForge/Private/Player/SFShipboardMovementComponent.cpp`:

```cpp
#include "Player/SFShipboardMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Ship/SFShipFrameComponent.h"

USFShipboardMovementComponent::USFShipboardMovementComponent()
{
	// The ship rotates the occupant explicitly in CarryWithFrame; letting the
	// stock yaw-only path run as well would apply the rotation twice.
	bIgnoreBaseRotation = true;
}

void USFShipboardMovementComponent::SetShipFrame(USFShipFrameComponent* NewFrame)
{
	ShipFrame = NewFrame;

	if (NewFrame)
	{
		LastFrameTransform = NewFrame->GetFrameTransform();
		DriftReferenceLocal = GetLocalLocation();
		LastLocalLocation = DriftReferenceLocal;
	}

	DriftCm = 0.0;
	PeakDriftCm = 0.0;
	PeakJitterCm = 0.0;
}

FVector USFShipboardMovementComponent::GetLocalLocation() const
{
	if (!ShipFrame.IsValid() || !UpdatedComponent)
	{
		return FVector::ZeroVector;
	}

	return ShipFrame->WorldToLocal(UpdatedComponent->GetComponentTransform()).GetLocation();
}

void USFShipboardMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	if (!ShipFrame.IsValid() || !UpdatedComponent)
	{
		Super::UpdateBasedMovement(DeltaSeconds);
		return;
	}

	CarryWithFrame();
}

void USFShipboardMovementComponent::CarryWithFrame()
{
	const FTransform CurrentFrameTransform = ShipFrame->GetFrameTransform();
	const FTransform OccupantWorld = UpdatedComponent->GetComponentTransform();

	const FTransform Carried =
		ShipFrame->CarryOccupant(LastFrameTransform, OccupantWorld);

	const FVector Delta = Carried.GetLocation() - OccupantWorld.GetLocation();

	// bSweep is false deliberately. Relative to the ship this displacement is
	// zero, so there is nothing along the path that could legitimately stop it.
	MoveUpdatedComponent(
		Delta,
		Carried.GetRotation(),
		/*bSweep=*/false,
		/*OutHit=*/nullptr,
		ETeleportType::TeleportPhysics);

	// The view is stored ship-local: the player keeps looking at the same
	// bulkhead no matter what the ship does, and the stars sweep past outside.
	if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
	{
		if (AController* Controller = Owner->GetController())
		{
			const FQuat FrameDelta =
				CurrentFrameTransform.GetRotation() * LastFrameTransform.GetRotation().Inverse();

			const FQuat NewView = FrameDelta * Controller->GetControlRotation().Quaternion();
			Controller->SetControlRotation(NewView.Rotator());
		}
	}

	LastFrameTransform = CurrentFrameTransform;

	// Keep the engine's own base bookkeeping consistent even though the stock
	// carry path was bypassed.
	SaveBaseLocation();

	UpdateDriftMeasurement();
}

void USFShipboardMovementComponent::UpdateDriftMeasurement()
{
	const FVector NowLocal = GetLocalLocation();

	const bool bMovingOnPurpose =
		!GetCurrentAcceleration().IsNearlyZero() || !Velocity.IsNearlyZero(1.0);

	if (bMovingOnPurpose)
	{
		// Walking legitimately changes the ship-local position, so re-baseline
		// instead of counting it as drift.
		DriftReferenceLocal = NowLocal;
		LastLocalLocation = NowLocal;
		DriftCm = 0.0;
		return;
	}

	DriftCm = FVector::Dist(NowLocal, DriftReferenceLocal);
	PeakDriftCm = FMath::Max(PeakDriftCm, DriftCm);

	// Jitter is what the eye notices even when the average position is perfect.
	const double JitterCm = FVector::Dist(NowLocal, LastLocalLocation);
	PeakJitterCm = FMath::Max(PeakJitterCm, JitterCm);

	LastLocalLocation = NowLocal;
}
```

- [ ] **Step 2: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: сборка без ошибок. Если компилятор ругается на `GetMovementBase` — значит куда-то попал устаревший вызов; в 5.8 нужен `GetMovementBaseObject()`.

- [ ] **Step 3: Коммит**

```bash
git add Source/StarForge
git commit -m "feat(player): decompose shipboard movement into carry and locomotion

Stock UE sweeps the base-carried displacement, which at the design's speed
ceiling traces a three-metre line through the ship's own hull and wedges the
character. The carry is zero motion relative to the ship, so it is a teleport;
only the player's own step is swept, against geometry that is static for that
instant.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 6: Пассажир

Персонаж от первого лица. Ввод сознательно сделан примитивным опросом клавиш, без Enhanced Input: замерочному стенду не нужны ассеты, а любой недостающий `InputMappingContext` превратился бы в час поиска причины, почему персонаж не двигается. На этапе 2 это заменяется нормальным вводом.

**Files:**
- Create: `Source/StarForge/Public/Player/SFCharacter.h`
- Create: `Source/StarForge/Private/Player/SFCharacter.cpp`

**Interfaces:**
- Consumes: `USFShipboardMovementComponent` из Task 5, `ASFTestShip` из Task 4.
- Produces: класс `ASFCharacter` с методом `GetShipboardMovement() const -> USFShipboardMovementComponent*`.

- [ ] **Step 1: Написать персонажа**

`Source/StarForge/Public/Player/SFCharacter.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SFCharacter.generated.h"

class UCameraComponent;
class USFShipboardMovementComponent;

/**
 * First-person occupant used by the measurement rig.
 *
 * Input is polled directly from the controller rather than routed through
 * Enhanced Input. The rig must run from a bare C++ build with no content at
 * all; a missing mapping context would present as "the character will not
 * move" and cost more time than the whole measurement is worth. Real input
 * arrives with the flight phase.
 */
UCLASS()
class STARFORGE_API ASFCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ASFCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	USFShipboardMovementComponent* GetShipboardMovement() const;

	/** Degrees of view rotation per second of held arrow key. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Input")
	double LookRateDegPerSec = 90.0;

private:
	void PollMovementInput();
	void PollLookInput(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UCameraComponent> Camera;
};
```

`Source/StarForge/Private/Player/SFCharacter.cpp`:

```cpp
#include "Player/SFCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/SFShipboardMovementComponent.h"

ASFCharacter::ASFCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USFShipboardMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0, 0.0, 64.0));
	Camera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = false;
}

USFShipboardMovementComponent* ASFCharacter::GetShipboardMovement() const
{
	return Cast<USFShipboardMovementComponent>(GetCharacterMovement());
}

void ASFCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The rig has exactly one ship, so wiring is unambiguous. Boarding volumes
	// arrive with docking in the world phase.
	if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (USFShipFrameComponent* Frame = It->FindComponentByClass<USFShipFrameComponent>())
			{
				Movement->SetShipFrame(Frame);
				break;
			}
		}
	}
}

void ASFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PollMovementInput();
	PollLookInput(DeltaSeconds);
}

void ASFCharacter::PollMovementInput()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const FRotator ViewYaw(0.0, PC->GetControlRotation().Yaw, 0.0);
	const FVector Forward = FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::Y);

	if (PC->IsInputKeyDown(EKeys::W)) { AddMovementInput(Forward, 1.0); }
	if (PC->IsInputKeyDown(EKeys::S)) { AddMovementInput(Forward, -1.0); }
	if (PC->IsInputKeyDown(EKeys::D)) { AddMovementInput(Right, 1.0); }
	if (PC->IsInputKeyDown(EKeys::A)) { AddMovementInput(Right, -1.0); }
}

void ASFCharacter::PollLookInput(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const double Step = LookRateDegPerSec * DeltaSeconds;

	if (PC->IsInputKeyDown(EKeys::Left))  { PC->AddYawInput(-Step); }
	if (PC->IsInputKeyDown(EKeys::Right)) { PC->AddYawInput(Step); }
	if (PC->IsInputKeyDown(EKeys::Up))    { PC->AddPitchInput(-Step); }
	if (PC->IsInputKeyDown(EKeys::Down))  { PC->AddPitchInput(Step); }
}
```

Добавить в начало `SFCharacter.cpp` после остальных `#include`:

```cpp
#include "EngineUtils.h"
#include "Ship/SFShipFrameComponent.h"
```

`AddYawInput` и `AddPitchInput` домножаются на чувствительность мыши, поэтому шаг здесь задан в градусах приблизительно — для стенда этого достаточно.

- [ ] **Step 2: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: сборка без ошибок.

- [ ] **Step 3: Коммит**

```bash
git add Source/StarForge
git commit -m "feat(player): add first-person occupant for the measurement rig

Input is polled straight off the controller rather than going through
Enhanced Input: the rig has to run from a bare build with no content, and a
missing mapping context would show up as an unmoving character and eat more
time than the measurement itself.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 7: Показания замера

HUD, ради которого весь стенд и собирается. Без чисел на экране «вроде нормально ходит» — не результат.

**Files:**
- Create: `Source/StarForge/Public/Debug/SFFrameDriftHUD.h`
- Create: `Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`

**Interfaces:**
- Consumes: `USFShipboardMovementComponent` (`GetDriftCm`, `GetPeakDriftCm`, `GetPeakJitterCm`, `GetLocalLocation`) из Task 5, `ASFTestShip` (`GetLinearAccelMetersPerSecSq`, `GetSpeedMetersPerSec`) из Task 4, `USFShipFrameComponent::GetAngularVelocityRadPerSec` из Task 3, `StarForge::ShipFrame::ComputeFeltAcceleration` и `ExceedsGravPlateLimit` из Task 2.
- Produces: класс `ASFFrameDriftHUD`.

- [ ] **Step 1: Написать HUD**

`Source/StarForge/Public/Debug/SFFrameDriftHUD.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SFFrameDriftHUD.generated.h"

/**
 * Readout for checkpoint 1. "It seems to walk fine" is not an answer; drift in
 * centimetres over a measured minute is.
 */
UCLASS()
class STARFORGE_API ASFFrameDriftHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawLine(const FString& Text, double& Y, const FLinearColor& Color = FLinearColor::White);

	double ElapsedSeconds = 0.0;
};
```

`Source/StarForge/Private/Debug/SFFrameDriftHUD.cpp`:

```cpp
#include "Debug/SFFrameDriftHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Player/SFCharacter.h"
#include "Player/SFShipboardMovementComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestShip.h"
#include "ShipFrameMath.h"

void ASFFrameDriftHUD::DrawLine(const FString& Text, double& Y, const FLinearColor& Color)
{
	if (Canvas)
	{
		Canvas->SetDrawColor(Color.ToFColor(true));
		Canvas->DrawText(GEngine->GetMediumFont(), Text, 32.0f, static_cast<float>(Y));
	}
	Y += 22.0;
}

void ASFFrameDriftHUD::DrawHUD()
{
	Super::DrawHUD();

	const ASFCharacter* Occupant = Cast<ASFCharacter>(GetOwningPawn());
	if (!Occupant)
	{
		return;
	}

	const USFShipboardMovementComponent* Movement = Occupant->GetShipboardMovement();
	if (!Movement)
	{
		return;
	}

	ElapsedSeconds += GetWorld()->GetDeltaSeconds();

	const USFShipFrameComponent* Frame = Movement->GetShipFrame();
	const ASFTestShip* Ship = Frame ? Cast<ASFTestShip>(Frame->GetOwner()) : nullptr;

	double Y = 48.0;

	DrawLine(FString::Printf(TEXT("elapsed              %.1f s"), ElapsedSeconds), Y);
	DrawLine(FString::Printf(TEXT("drift now            %.3f cm"), Movement->GetDriftCm()), Y);

	// This is the number checkpoint 1 turns on: the design allows 1 cm a minute.
	const double PeakDrift = Movement->GetPeakDriftCm();
	const bool bWithinBudget = PeakDrift <= 1.0;
	DrawLine(
		FString::Printf(TEXT("drift peak           %.3f cm   [budget 1.0 cm/min]"), PeakDrift),
		Y,
		bWithinBudget ? FLinearColor::Green : FLinearColor::Red);

	// Separate failure mode: the average position can be perfect while the
	// picture shakes too hard to aim through.
	DrawLine(FString::Printf(TEXT("jitter peak          %.3f cm/tick"),
		Movement->GetPeakJitterCm()), Y);

	const FVector Local = Movement->GetLocalLocation();
	DrawLine(FString::Printf(TEXT("ship-local position  %.1f  %.1f  %.1f cm"),
		Local.X, Local.Y, Local.Z), Y);

	if (Frame && Ship)
	{
		DrawLine(FString::Printf(TEXT("ship speed           %.1f m/s   [ceiling 400]"),
			Ship->GetSpeedMetersPerSec()), Y);

		const FVector Omega = Frame->GetAngularVelocityRadPerSec();
		DrawLine(FString::Printf(TEXT("angular rate         %.1f deg/s   [ceiling 180]"),
			FMath::RadiansToDegrees(Omega.Size())), Y);

		const FVector LinearAccel = Ship->GetLinearAccelMetersPerSecSq();
		const FVector Felt = StarForge::ShipFrame::ComputeFeltAcceleration(
			LinearAccel, Omega, Local / 100.0);

		const bool bKnockdown = StarForge::ShipFrame::ExceedsGravPlateLimit(Felt);
		DrawLine(
			FString::Printf(TEXT("felt acceleration    %.2f m/s2   [limit %.2f]"),
				Felt.Size(), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq),
			Y,
			bKnockdown ? FLinearColor::Yellow : FLinearColor::White);
	}
}
```

- [ ] **Step 2: Собрать**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: сборка без ошибок.

- [ ] **Step 3: Коммит**

```bash
git add Source/StarForge
git commit -m "feat(debug): add drift readout for checkpoint 1

The checkpoint turns on a number, not an impression, so the budget from the
design -- one centimetre a minute -- is on screen and goes red when missed.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 8: Сборка сцены и прогон замера

Режим игры собирает сцену кодом: пустая карта, корабль в нуле, пассажир внутри. Никаких размещённых вручную акторов — прогон должен воспроизводиться у любого, кто взял репозиторий.

**Files:**
- Create: `Source/StarForge/Public/SFSpikeGameMode.h`
- Create: `Source/StarForge/Private/SFSpikeGameMode.cpp`
- Modify: `Config/DefaultEngine.ini`
- Create: `docs/superpowers/notes/2026-09-06-checkpoint-1-results.md`

**Interfaces:**
- Consumes: `ASFTestShip` из Task 4, `ASFCharacter` из Task 6, `ASFFrameDriftHUD` из Task 7.
- Produces: класс `ASFSpikeGameMode`; заполненный отчёт о замере.

- [ ] **Step 1: Написать режим игры**

`Source/StarForge/Public/SFSpikeGameMode.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SFSpikeGameMode.generated.h"

class ASFTestShip;

/**
 * Builds the measurement scene in code so the run reproduces from a clean
 * checkout with an empty map and no placed actors.
 */
UCLASS()
class STARFORGE_API ASFSpikeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASFSpikeGameMode();

	virtual void StartPlay() override;

private:
	UPROPERTY()
	TObjectPtr<ASFTestShip> Ship;
};
```

`Source/StarForge/Private/SFSpikeGameMode.cpp`:

```cpp
#include "SFSpikeGameMode.h"

#include "Debug/SFFrameDriftHUD.h"
#include "GameFramework/PlayerController.h"
#include "Player/SFCharacter.h"
#include "Ship/SFTestShip.h"

ASFSpikeGameMode::ASFSpikeGameMode()
{
	DefaultPawnClass = ASFCharacter::StaticClass();
	HUDClass = ASFFrameDriftHUD::StaticClass();
}

void ASFSpikeGameMode::StartPlay()
{
	// The ship must exist before the default pawn spawns, so that the character's
	// BeginPlay finds a frame to ride.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Ship = GetWorld()->SpawnActor<ASFTestShip>(
		ASFTestShip::StaticClass(), FTransform::Identity, SpawnParams);

	Super::StartPlay();

	if (Ship)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->SetActorLocation(Ship->GetInteriorSpawnWorldLocation());
			}
		}
	}
}
```

- [ ] **Step 2: Создать пустую карту**

Через MCP-канал (редактор должен быть открыт, сервер на `127.0.0.1:8000`) создать пустой уровень `/Game/Maps/L_FrameSpike` и сохранить его.

Если MCP недоступен — в редакторе: `File > New Level > Empty Level`, затем `Save Current Level As` в `Content/Maps/L_FrameSpike`.

Уровень должен остаться пустым: всё содержимое сцены создаётся режимом игры.

- [ ] **Step 3: Прописать карту и режим в конфиге**

В `Config/DefaultEngine.ini` заменить секцию `[/Script/EngineSettings.GameMapsSettings]` на:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/Maps/L_FrameSpike
EditorStartupMap=/Game/Maps/L_FrameSpike
GlobalDefaultGameMode=/Script/StarForge.SFSpikeGameMode
```

- [ ] **Step 4: Собрать и запустить замер**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/UEProject/StarForge/StarForge.uproject" -game -windowed -ResX=1600 -ResY=900
```

Порядок проверки, **строго по шагам**:

1. Не трогая клавиатуру, простоять **одну полную минуту**. Записать `drift peak`. Это главное число замера.
2. Походить на WASD по каюте. Проверить, что персонаж не проваливается сквозь пол и не застревает в стенах.
3. Покрутить обзор стрелками. Проверить, что интерьер перед глазами стоит неподвижно, а не уезжает при вращении корабля.
4. Посмотреть на `felt acceleration`: он должен периодически превышать предел — профиль на то и выкручен на максимум.
5. Отметить субъективно: пригодна ли камера, можно ли целиться, не укачивает ли.

- [ ] **Step 5: Записать результат**

Создать `docs/superpowers/notes/2026-09-06-checkpoint-1-results.md` и заполнить фактическими числами:

```markdown
# Точка проверки 1 — результаты замера

**Дата прогона:**
**Сборка:** StarForgeEditor Win64 Development, UE 5.8.2

## Числа

| Показатель | Бюджет | Факт |
|---|---|---|
| Пиковый увод за минуту стояния | 1.0 см | |
| Пиковое дрожание за тик | заметно глазом — провал | |
| Провалы сквозь геометрию | нет | |
| Застревания при ходьбе | нет | |
| Интерьер неподвижен при вращении | да | |

## Субъективно

- Пригодность камеры:
- Прицеливание:
- Укачивание:

## Решение

- [ ] Зелёный свет: разложение движения подтверждено, идём на этап 2 по основному плану
- [ ] План Б: переходим на «карман» (§12 спеки)
- [ ] План В: переходим на `Mover` (§12 спеки)

**Обоснование:**
```

- [ ] **Step 6: Коммит**

```bash
git add Source/StarForge Config/DefaultEngine.ini Content/Maps docs/superpowers/notes
git commit -m "feat(spike): assemble measurement scene and record checkpoint 1

The scene is built in code against an empty map so the run reproduces from a
clean checkout. Checkpoint 1 decides whether the rest of the design stands as
written or moves to the interior-pocket fallback.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Точка проверки 1

После Task 8 работа **останавливается** до разбора результатов.

Зелёный свет — если измеренное время не меньше 55 секунд, пиковый увод укладывается в 1 см за минуту, дрожание не заметно глазом, красной строки о пропущенном переносе нет, провалов и застреваний нет, интерьер при вращении стоит и камерой можно пользоваться.

Порядок важен: **измеренное время проверяется первым**. Нулевой увод возникает и когда стенд отстоял минуту идеально, и когда он не сделал ни одного замера, и само число их не различает.

Если хотя бы один пункт не выполнен — разбирается, чинится ли это в текущем подходе или включается план Б из §12 спеки. Таблица уточнена по итогам финального ревью ветки: часть первоначальных предположений о причинах оказалась неверной.

| Симптом | Первый подозреваемый |
|---|---|
| Персонажа кувыркает, цифры замерли | Направление гравитации не сложилось с телепортом переноса. Это единственное допущение ветки, которое не проверено ничем, кроме самого прогона |
| Красная строка `CARRY SKIPPED n ticks` | Опора потеряна, перенос не вызывался. Всё, что выше на экране, относится к более раннему моменту |
| Интерьер уезжает из-под взгляда при вращении | Зажим крена и тангажа в менеджере камеры. Снят в `SFCharacter::BeginPlay` — проверить, что снятие подействовало. Флаг `bIgnoreBaseRotation` здесь **ни при чём**: на пути переноса родительская реализация не вызывается вовсе, и флаг влияет только на запасной путь |
| Заметный увод при чистом вращении | Потеря точности в накоплении поворота |
| Провалы сквозь пол при линейном разгоне | Телепорт где-то всё-таки идёт свипом |
| Ощущаемое ускорение почти всё время выше 2 g | Не дефект стенда. Крен 180°/с в каюте радиусом 4 м даёт около 4 g у стены — вдвое выше порога компенсации, который назначает сама спека. Противоречие внутри спеки, решать автору |
