# Математика полёта — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Построить в `StarForgeCore` полную математику полёта — паспорт манёвренности корабля, решатель распределения тяги по двигателям и контур управления IFCS — целиком под юнит-тестами, без единой зависимости от движка.

**Architecture:** Двигатель описывается геометрией, а не коэффициентами. Из набора двигателей собирается матрица эффективности 6×N, её демпфированное псевдообращение кэшируется один раз, и в кадре остаётся умножение матрицы на вектор. Над решателем сидит контур управления, переводящий намерение пилота в требуемое воздействие с учётом гироскопического члена.

**Tech Stack:** Unreal Engine 5.8.2, C++, Automation Test Framework. Модуль `StarForgeCore` зависит только от `Core` и `CoreUObject`.

**Spec:** [docs/superpowers/specs/2026-09-06-starforge-vertical-slice-design.md](../specs/2026-09-06-starforge-vertical-slice-design.md) — §5 целиком, §10 в части обязательных тестов.

## Global Constraints

- Движок — **Unreal Engine 5.8.2**, установлен в `C:\Program Files\Epic Games\UE_5.8`.
- `StarForgeCore` зависит **только** от `Core` и `CoreUObject`. Добавление `Engine` в этот модуль — ошибка, ломающая смысл разделения.
- Внутри `StarForgeCore` физические величины — **в СИ**: метры, килограммы, ньютоны, ньютон-метры, радианы, секунды. Конвертация в сантиметры происходит в игровом модуле, которого этот план не касается.
- Демпфирование решателя: **`lambda = 0.01`**. Итераций зажима и коррекции: **три**.
- Допуск на паразитные составляющие: **не более 1%** от максимума, который способен создать набор двигателей.
- В 5.8 `EAutomationTestFlags` — `enum class`, маска контекста вынесена в константу `EAutomationTestFlags_ApplicationContextMask`. Синтаксис `EAutomationTestFlags::ApplicationContextMask` **не соберётся**.
- Все вычисления в **двойной точности** (`double`). Одинарная точность на 6×6 обращении даёт накопление, сравнимое с допуском в 1%.
- Коммит после каждой задачи. Сообщения на английском, тело — почему, а не что.
- **Никаких строк `Co-Authored-By` и любой другой атрибуции в коммитах** — запрещено проектом, см. `CLAUDE.md`.

---

## Область этого плана

План покрывает **только математику** — то, что живёт в `StarForgeCore` и проверяется из консоли без запуска редактора.

Мост в Chaos, `UShipDataAsset`, компонент массива двигателей, пост пилота и маршрутизация ввода — **следующий план**. Он зависит от этого и не может быть написан раньше, потому что опирается на интерфейсы, которые здесь создаются.

Разделение не бюрократическое: всё в этом плане — чистые функции над структурами, и ошибка в них не падает и не логируется, а проявляется через недели как «корабль странно ведёт». Юнит-тест ловит её за секунду. Всё в следующем плане проверяется только глазами и руками.

### Риск, который следующий план обязан учесть

Точка проверки 1 подтвердила перенос пассажира **на кинематическом корабле**. Когда корабль станет физическим телом Chaos, его трансформ начнёт обновляться в другой момент кадра — с подшагами физики и, возможно, с интерполяцией. Перенос читает `GetActorTransform()` в момент своего вызова. Это может изменить результат, который мы уже засчитали. Следующий план должен содержать повторный замер увода после перевода корабля на физику, а не считать вопрос закрытым.

---

## Структура файлов

```
Source/StarForgeCore/
  Public/
    SmallMatrix.h          6x6 плотная матрица и её обращение — единственное,
                           чего нет в Core (FMatrix там 4x4)
    ThrusterDef.h          FThrusterDef, FShipMassProperties, FWrench — только данные
    ThrustAllocator.h      матрица эффективности и решатель распределения тяги
    FlightController.h     FFlightIntent и перевод намерения в воздействие
  Private/
    SmallMatrix.cpp
    ThrustAllocator.cpp
    FlightController.cpp
    Tests/
      SmallMatrixTest.cpp
      ThrustAllocatorTest.cpp
      FlightControllerTest.cpp
```

Разделение по ответственности: `SmallMatrix` не знает про двигатели, `ThrustAllocator` не знает про пилота, `FlightController` не знает, как решается распределение. Каждый файл тестируется отдельно.

---

### Task 1: Обращение матрицы 6×6

Решателю нужно обратить `B·Bᵀ + λI` — всегда 6×6, независимо от числа двигателей. В `Core` матриц крупнее 4×4 нет, поэтому это первое, что нужно построить.

**Files:**
- Create: `Source/StarForgeCore/Public/SmallMatrix.h`
- Create: `Source/StarForgeCore/Private/SmallMatrix.cpp`
- Create: `Source/StarForgeCore/Private/Tests/SmallMatrixTest.cpp`

**Interfaces:**
- Consumes: ничего.
- Produces: `StarForge::FMatrix6` со статическими `Identity()`, `Zero()`, методами `double& At(int32 Row, int32 Col)`, `double At(int32 Row, int32 Col) const`, `bool TryInvert(FMatrix6& Out) const`, и оператором `FMatrix6 operator*(const FMatrix6&) const`.

- [x] **Step 1: Написать падающие тесты**

`Source/StarForgeCore/Private/Tests/SmallMatrixTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "SmallMatrix.h"

namespace
{
	/** Largest absolute difference between two matrices, for tolerance checks. */
	double MaxAbsDifference(const StarForge::FMatrix6& A, const StarForge::FMatrix6& B)
	{
		double Worst = 0.0;
		for (int32 Row = 0; Row < 6; ++Row)
		{
			for (int32 Col = 0; Col < 6; ++Col)
			{
				Worst = FMath::Max(Worst, FMath::Abs(A.At(Row, Col) - B.At(Row, Col)));
			}
		}
		return Worst;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatrix6IdentityInvertsToItselfTest,
	"StarForge.Core.Matrix6.IdentityInvertsToItself",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMatrix6IdentityInvertsToItselfTest::RunTest(const FString& Parameters)
{
	StarForge::FMatrix6 Inverse;
	const bool bInverted = StarForge::FMatrix6::Identity().TryInvert(Inverse);

	TestTrue(TEXT("identity is invertible"), bInverted);
	TestTrue(TEXT("identity inverts to identity"),
		MaxAbsDifference(Inverse, StarForge::FMatrix6::Identity()) < 1e-12);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatrix6RoundTripTest,
	"StarForge.Core.Matrix6.RoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMatrix6RoundTripTest::RunTest(const FString& Parameters)
{
	// A diagonally dominant matrix with no symmetry to hide a transposition bug:
	// every off-diagonal entry differs from its mirror.
	StarForge::FMatrix6 A = StarForge::FMatrix6::Zero();
	for (int32 Row = 0; Row < 6; ++Row)
	{
		for (int32 Col = 0; Col < 6; ++Col)
		{
			A.At(Row, Col) = (Row == Col) ? 10.0 + Row : 0.1 * (Row + 1) - 0.03 * (Col + 1);
		}
	}

	StarForge::FMatrix6 Inverse;
	TestTrue(TEXT("a diagonally dominant matrix is invertible"), A.TryInvert(Inverse));

	TestTrue(TEXT("A times its inverse is the identity"),
		MaxAbsDifference(A * Inverse, StarForge::FMatrix6::Identity()) < 1e-9);
	TestTrue(TEXT("the inverse times A is the identity too"),
		MaxAbsDifference(Inverse * A, StarForge::FMatrix6::Identity()) < 1e-9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatrix6SingularIsRejectedTest,
	"StarForge.Core.Matrix6.SingularIsRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMatrix6SingularIsRejectedTest::RunTest(const FString& Parameters)
{
	// Two identical rows: rank 5, no inverse. Reporting failure matters because a
	// ship whose thrusters cannot span all six axes produces exactly this, and the
	// caller has to know rather than receive silent nonsense.
	StarForge::FMatrix6 Singular = StarForge::FMatrix6::Identity();
	for (int32 Col = 0; Col < 6; ++Col)
	{
		Singular.At(3, Col) = Singular.At(2, Col);
	}

	StarForge::FMatrix6 Inverse;
	TestFalse(TEXT("a rank-deficient matrix reports failure"), Singular.TryInvert(Inverse));

	return true;
}
```

- [x] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: `Cannot open include file: 'SmallMatrix.h'`. Это красное состояние цикла — заголовка ещё нет.

- [x] **Step 3: Реализовать**

`Source/StarForgeCore/Public/SmallMatrix.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

namespace StarForge
{
	/**
	 * A dense 6x6 matrix in double precision.
	 *
	 * Core stops at 4x4, and thrust allocation needs exactly six: three force axes
	 * and three torque axes. Six is also where it stops -- the matrix that has to be
	 * inverted is always 6x6 no matter how many thrusters a ship carries, because it
	 * is B times B transposed rather than B itself.
	 */
	struct STARFORGECORE_API FMatrix6
	{
		static constexpr int32 Size = 6;

		double M[Size][Size];

		static FMatrix6 Zero();
		static FMatrix6 Identity();

		double& At(int32 Row, int32 Col) { return M[Row][Col]; }
		double At(int32 Row, int32 Col) const { return M[Row][Col]; }

		FMatrix6 operator*(const FMatrix6& Other) const;

		/**
		 * Gauss-Jordan inversion with partial pivoting.
		 *
		 * Returns false and leaves Out untouched when the matrix is singular to
		 * working precision. Callers must check: a thruster layout that cannot
		 * produce all six axes lands here, and silently returning nonsense would
		 * surface much later as a ship that will not turn.
		 */
		bool TryInvert(FMatrix6& Out) const;
	};
}
```

`Source/StarForgeCore/Private/SmallMatrix.cpp`:

```cpp
#include "SmallMatrix.h"

namespace StarForge
{
	FMatrix6 FMatrix6::Zero()
	{
		FMatrix6 Result;
		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				Result.M[Row][Col] = 0.0;
			}
		}
		return Result;
	}

	FMatrix6 FMatrix6::Identity()
	{
		FMatrix6 Result = Zero();
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Result.M[Index][Index] = 1.0;
		}
		return Result;
	}

	FMatrix6 FMatrix6::operator*(const FMatrix6& Other) const
	{
		FMatrix6 Result = Zero();
		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				double Sum = 0.0;
				for (int32 Index = 0; Index < Size; ++Index)
				{
					Sum += M[Row][Index] * Other.M[Index][Col];
				}
				Result.M[Row][Col] = Sum;
			}
		}
		return Result;
	}

	bool FMatrix6::TryInvert(FMatrix6& Out) const
	{
		// Work on an augmented [A | I] and reduce the left half to the identity.
		double Work[Size][2 * Size];
		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				Work[Row][Col] = M[Row][Col];
				Work[Row][Size + Col] = (Row == Col) ? 1.0 : 0.0;
			}
		}

		for (int32 Pivot = 0; Pivot < Size; ++Pivot)
		{
			// Partial pivoting: without it a zero on the diagonal of an otherwise
			// perfectly invertible matrix would abort the whole solve.
			int32 BestRow = Pivot;
			for (int32 Row = Pivot + 1; Row < Size; ++Row)
			{
				if (FMath::Abs(Work[Row][Pivot]) > FMath::Abs(Work[BestRow][Pivot]))
				{
					BestRow = Row;
				}
			}

			if (FMath::Abs(Work[BestRow][Pivot]) < UE_DOUBLE_SMALL_NUMBER)
			{
				return false;
			}

			if (BestRow != Pivot)
			{
				for (int32 Col = 0; Col < 2 * Size; ++Col)
				{
					Swap(Work[Pivot][Col], Work[BestRow][Col]);
				}
			}

			const double PivotValue = Work[Pivot][Pivot];
			for (int32 Col = 0; Col < 2 * Size; ++Col)
			{
				Work[Pivot][Col] /= PivotValue;
			}

			for (int32 Row = 0; Row < Size; ++Row)
			{
				if (Row == Pivot)
				{
					continue;
				}

				const double Factor = Work[Row][Pivot];
				if (Factor == 0.0)
				{
					continue;
				}

				for (int32 Col = 0; Col < 2 * Size; ++Col)
				{
					Work[Row][Col] -= Factor * Work[Pivot][Col];
				}
			}
		}

		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				Out.M[Row][Col] = Work[Row][Size + Col];
			}
		}

		return true;
	}
}
```

- [x] **Step 4: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается девять пройденных тестов: шесть прежних плюс три новых. **`UnrealEditor-Cmd.exe` обрезает вывод в консоль** — результаты читать из `Saved/Logs/StarForge.log`, строки `Test Completed. Result=` и `TEST COMPLETE. EXIT CODE:`.

- [x] **Step 5: Коммит**

```bash
git add Source/StarForgeCore/Public/SmallMatrix.h Source/StarForgeCore/Private/SmallMatrix.cpp Source/StarForgeCore/Private/Tests/SmallMatrixTest.cpp
git commit -m "feat(core): add a 6x6 matrix with checked inversion

Thrust allocation inverts B times B transposed, which is 6x6 whatever the thruster
count, and Core's matrices stop at 4x4. Inversion reports failure rather than
returning nonsense because a thruster layout that cannot span all six axes lands
there, and a ship that silently will not turn is far harder to diagnose than one
that refuses to build its allocator."
```

---

### Task 2: Двигатель, масса и вклад в воздействие

Двигатель описывается геометрией. Его вклад на полной тяге — сила вдоль направления и момент от плеча относительно центра масс.

**Files:**
- Create: `Source/StarForgeCore/Public/ThrusterDef.h`
- Create: `Source/StarForgeCore/Private/Tests/ThrustAllocatorTest.cpp`

**Interfaces:**
- Consumes: ничего.
- Produces: `StarForge::FThrusterDef` (поля `Location`, `Direction`, `MaxThrust`, `ResponseTime`), `StarForge::FShipMassProperties` (поля `Mass`, `CentreOfMass`, `InertiaDiagonal`, `InertiaRotation`), `StarForge::FWrench` (поля `Force`, `Torque`, операторы `+`, `-`, `*`, метод `double Component(int32 Index) const`), функция `StarForge::ComputeThrusterWrench(const FThrusterDef&, const FVector& CentreOfMass) -> FWrench`.

- [x] **Step 1: Написать падающие тесты**

`Source/StarForgeCore/Private/Tests/ThrustAllocatorTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "ThrusterDef.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterWrenchThroughCentreTest,
	"StarForge.Core.Thruster.WrenchThroughCentre",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterWrenchThroughCentreTest::RunTest(const FString& Parameters)
{
	// A thruster whose line of action passes through the centre of mass produces
	// pure force and no torque, however far along that line it sits.
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector(-3.0, 0.0, 0.0);
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 1000.0;

	const StarForge::FWrench Wrench =
		StarForge::ComputeThrusterWrench(Thruster, FVector::ZeroVector);

	TestTrue(TEXT("force is thrust along the thruster's direction"),
		Wrench.Force.Equals(FVector(1000.0, 0.0, 0.0), 1e-9));
	TestTrue(TEXT("a line of action through the centre of mass makes no torque"),
		Wrench.Torque.IsNearlyZero(1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterWrenchOffAxisTest,
	"StarForge.Core.Thruster.WrenchOffAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterWrenchOffAxisTest::RunTest(const FString& Parameters)
{
	// Two metres out along +Y, pushing along +X. The lever arm is r = (0,2,0) and
	// the force F = (500,0,0), so r x F = (0*0 - 0*0, 0*500 - 0*0, 0*0 - 2*500)
	// = (0, 0, -1000) newton-metres: it yaws the ship nose-right.
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector(0.0, 2.0, 0.0);
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 500.0;

	const StarForge::FWrench Wrench =
		StarForge::ComputeThrusterWrench(Thruster, FVector::ZeroVector);

	TestTrue(TEXT("force is unchanged by the offset"),
		Wrench.Force.Equals(FVector(500.0, 0.0, 0.0), 1e-9));
	TestTrue(TEXT("torque is the lever arm crossed into the force"),
		Wrench.Torque.Equals(FVector(0.0, 0.0, -1000.0), 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterWrenchRespectsCentreOfMassTest,
	"StarForge.Core.Thruster.WrenchRespectsCentreOfMass",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterWrenchRespectsCentreOfMassTest::RunTest(const FString& Parameters)
{
	// Same thruster as above, but the centre of mass has moved onto its line of
	// action. The torque must vanish -- the lever arm is measured from the centre
	// of mass, not from the origin, and a ship whose load shifts must feel that.
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector(0.0, 2.0, 0.0);
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 500.0;

	const StarForge::FWrench Wrench =
		StarForge::ComputeThrusterWrench(Thruster, FVector(0.0, 2.0, 0.0));

	TestTrue(TEXT("moving the centre of mass onto the line of action kills the torque"),
		Wrench.Torque.IsNearlyZero(1e-9));

	return true;
}
```

- [x] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: `Cannot open include file: 'ThrusterDef.h'`.

- [x] **Step 3: Реализовать**

`Source/StarForgeCore/Public/ThrusterDef.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

namespace StarForge
{
	/**
	 * A thruster described by geometry rather than by tuning coefficients.
	 *
	 * Everything a ship can do follows from where its thrusters are and which way
	 * they push. Nothing here is a feel knob: change the layout and the handling
	 * changes with it, including after a thruster is shot away.
	 *
	 * SI throughout -- metres, newtons, seconds.
	 */
	struct STARFORGECORE_API FThrusterDef
	{
		/** Position in the ship's own frame, metres. */
		FVector Location = FVector::ZeroVector;

		/** Unit vector along which this thruster pushes the ship. */
		FVector Direction = FVector::ForwardVector;

		/** Thrust at full throttle, newtons. */
		double MaxThrust = 0.0;

		/** Seconds to spool from zero to full. Consumed by the engine bridge, not here. */
		double ResponseTime = 0.05;
	};

	/**
	 * Mass properties as the solver needs them.
	 *
	 * Chaos reports inertia as a diagonal in principal axes plus the rotation into
	 * those axes, so both halves are kept: using the diagonal without the rotation
	 * would quietly mispredict every turn on a ship whose principal axes are not
	 * aligned with its model.
	 */
	struct STARFORGECORE_API FShipMassProperties
	{
		/** Kilograms. */
		double Mass = 1.0;

		/** Centre of mass in the ship's frame, metres. */
		FVector CentreOfMass = FVector::ZeroVector;

		/** Principal moments of inertia, kg m^2. */
		FVector InertiaDiagonal = FVector(1.0, 1.0, 1.0);

		/** Rotation from the ship's frame into the principal axes. */
		FQuat InertiaRotation = FQuat::Identity;

		/** Applies the inertia tensor to an angular vector expressed in the ship's frame. */
		FVector TransformByInertia(const FVector& AngularShipFrame) const
		{
			const FVector InPrincipal = InertiaRotation.UnrotateVector(AngularShipFrame);
			const FVector Scaled = InPrincipal * InertiaDiagonal;
			return InertiaRotation.RotateVector(Scaled);
		}
	};

	/**
	 * A force and a torque acting together, both in the ship's frame.
	 *
	 * Six numbers is the natural size of everything in this system: what a pilot
	 * asks for, what one thruster contributes, and what the whole array achieves.
	 */
	struct STARFORGECORE_API FWrench
	{
		/** Newtons. */
		FVector Force = FVector::ZeroVector;

		/** Newton-metres. */
		FVector Torque = FVector::ZeroVector;

		/** Index 0-2 are the force axes, 3-5 the torque axes. */
		double Component(int32 Index) const
		{
			return (Index < 3) ? Force[Index] : Torque[Index - 3];
		}

		FWrench operator+(const FWrench& Other) const
		{
			FWrench Result;
			Result.Force = Force + Other.Force;
			Result.Torque = Torque + Other.Torque;
			return Result;
		}

		FWrench operator-(const FWrench& Other) const
		{
			FWrench Result;
			Result.Force = Force - Other.Force;
			Result.Torque = Torque - Other.Torque;
			return Result;
		}

		FWrench operator*(double Scale) const
		{
			FWrench Result;
			Result.Force = Force * Scale;
			Result.Torque = Torque * Scale;
			return Result;
		}
	};

	/** What one thruster contributes at full throttle, in the ship's frame. */
	STARFORGECORE_API FWrench ComputeThrusterWrench(
		const FThrusterDef& Thruster,
		const FVector& CentreOfMass);
}
```

Реализация `ComputeThrusterWrench` идёт в `ThrustAllocator.cpp` в следующей задаче. Чтобы этот шаг собрался и тесты покраснели по существу, добавь временно в новый файл `Source/StarForgeCore/Private/ThrusterDef.cpp`:

```cpp
#include "ThrusterDef.h"

namespace StarForge
{
	FWrench ComputeThrusterWrench(const FThrusterDef& Thruster, const FVector& CentreOfMass)
	{
		FWrench Result;
		Result.Force = Thruster.Direction.GetSafeNormal() * Thruster.MaxThrust;
		Result.Torque = FVector::CrossProduct(Thruster.Location - CentreOfMass, Result.Force);
		return Result;
	}
}
```

Нормализация направления не косметика: набор двигателей приходит из редактируемого ассета, и ненормированный вектор иначе умножил бы тягу на свою длину без всякого предупреждения.

- [x] **Step 4: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается двенадцать пройденных тестов. Результаты — из `Saved/Logs/StarForge.log`.

- [x] **Step 5: Коммит**

```bash
git add Source/StarForgeCore/Public/ThrusterDef.h Source/StarForgeCore/Private/ThrusterDef.cpp Source/StarForgeCore/Private/Tests/ThrustAllocatorTest.cpp
git commit -m "feat(core): describe thrusters by geometry and what they contribute

A thruster is where it sits and which way it pushes, and every handling
characteristic follows from that rather than from a feel coefficient. Torque is
measured from the centre of mass rather than the origin so a ship whose load
shifts handles differently, which is the same property that will make a thruster
lost to gunfire change how the ship flies without anyone programming it."
```

---

### Task 3: Решатель распределения тяги

Ядро этапа. По требуемому воздействию найти газ каждого двигателя.

**Files:**
- Create: `Source/StarForgeCore/Public/ThrustAllocator.h`
- Create: `Source/StarForgeCore/Private/ThrustAllocator.cpp`
- Delete: `Source/StarForgeCore/Private/ThrusterDef.cpp` — реализация переезжает сюда
- Modify: `Source/StarForgeCore/Private/Tests/ThrustAllocatorTest.cpp` — дописать в конец

**Interfaces:**
- Consumes: `StarForge::FMatrix6` из Task 1; `StarForge::FThrusterDef`, `FWrench`, `ComputeThrusterWrench` из Task 2.
- Produces: класс `StarForge::FThrustAllocator` с методами `bool Build(const TArray<FThrusterDef>& Thrusters, const FVector& CentreOfMass, double DampingLambda = 0.01)`, `void Solve(const FWrench& Desired, TArray<double>& OutThrottles) const`, `FWrench ComputeAchieved(const TArray<double>& Throttles) const`, `double GetAxisCapability(int32 Axis) const`, `int32 Num() const`, `bool IsBuilt() const`.

- [x] **Step 1: Написать падающие тесты**

Дописать в конец `Source/StarForgeCore/Private/Tests/ThrustAllocatorTest.cpp`:

```cpp
#include "ThrustAllocator.h"

namespace
{
	/**
	 * A symmetric twelve-thruster array: four positions around the hull, each
	 * carrying thrusters along +X, +Y and +Z. Symmetry is what makes the parasitic
	 * checks meaningful -- an asymmetric layout has honest cross-coupling and would
	 * fail them for the right reasons.
	 */
	TArray<StarForge::FThrusterDef> MakeSymmetricArray()
	{
		TArray<StarForge::FThrusterDef> Thrusters;

		const FVector Corners[4] = {
			FVector( 2.0,  2.0, 0.0),
			FVector( 2.0, -2.0, 0.0),
			FVector(-2.0,  2.0, 0.0),
			FVector(-2.0, -2.0, 0.0)
		};

		const FVector Axes[3] = {
			FVector(1.0, 0.0, 0.0),
			FVector(0.0, 1.0, 0.0),
			FVector(0.0, 0.0, 1.0)
		};

		for (const FVector& Corner : Corners)
		{
			for (const FVector& Axis : Axes)
			{
				StarForge::FThrusterDef Thruster;
				Thruster.Location = Corner;
				Thruster.Direction = Axis;
				Thruster.MaxThrust = 1000.0;
				Thrusters.Add(Thruster);
			}
		}

		return Thrusters;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorPureForceTest,
	"StarForge.Core.ThrustAllocator.PureForceMakesNoTorque",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorPureForceTest::RunTest(const FString& Parameters)
{
	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a symmetric array builds"),
		Allocator.Build(MakeSymmetricArray(), FVector::ZeroVector));

	StarForge::FWrench Desired;
	Desired.Force = FVector(2000.0, 0.0, 0.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);

	TestTrue(TEXT("the requested force is delivered"),
		Achieved.Force.Equals(Desired.Force, 1.0));

	// Parasitic torque is judged against what this array could produce on the same
	// axis at full throttle, not against an absolute number -- a bigger ship would
	// otherwise pass or fail the same test for the wrong reason.
	for (int32 Axis = 3; Axis < 6; ++Axis)
	{
		const double Budget = 0.01 * Allocator.GetAxisCapability(Axis);
		TestTrue(*FString::Printf(TEXT("parasitic torque on axis %d is within one percent"), Axis),
			FMath::Abs(Achieved.Component(Axis)) <= Budget);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorPureTorqueTest,
	"StarForge.Core.ThrustAllocator.PureTorqueMakesNoForce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorPureTorqueTest::RunTest(const FString& Parameters)
{
	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a symmetric array builds"),
		Allocator.Build(MakeSymmetricArray(), FVector::ZeroVector));

	StarForge::FWrench Desired;
	Desired.Torque = FVector(0.0, 0.0, 1000.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);

	TestTrue(TEXT("the requested torque is delivered"),
		Achieved.Torque.Equals(Desired.Torque, 1.0));

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Budget = 0.01 * Allocator.GetAxisCapability(Axis);
		TestTrue(*FString::Printf(TEXT("parasitic force on axis %d is within one percent"), Axis),
			FMath::Abs(Achieved.Component(Axis)) <= Budget);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorSaturationKeepsDirectionTest,
	"StarForge.Core.ThrustAllocator.SaturationKeepsDirection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorSaturationKeepsDirectionTest::RunTest(const FString& Parameters)
{
	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a symmetric array builds"),
		Allocator.Build(MakeSymmetricArray(), FVector::ZeroVector));

	// Ask for far more forward force than the array can ever deliver.
	StarForge::FWrench Desired;
	Desired.Force = FVector(1.0e9, 0.0, 0.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	for (double Throttle : Throttles)
	{
		TestTrue(TEXT("every throttle stays inside its physical range"),
			Throttle >= -1e-9 && Throttle <= 1.0 + 1e-9);
	}

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);

	TestTrue(TEXT("the ship still pushes forward"), Achieved.Force.X > 0.0);
	TestTrue(TEXT("an impossible request must not push the ship sideways"),
		FMath::Abs(Achieved.Force.Y) <= 0.01 * FMath::Abs(Achieved.Force.X));
	TestTrue(TEXT("an impossible request must not push the ship up"),
		FMath::Abs(Achieved.Force.Z) <= 0.01 * FMath::Abs(Achieved.Force.X));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorSurvivesThrusterLossTest,
	"StarForge.Core.ThrustAllocator.SurvivesThrusterLoss",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorSurvivesThrusterLossTest::RunTest(const FString& Parameters)
{
	// Rebuild without one thruster, as the ship will when one is shot away. The
	// array must still solve, and the degraded handling that follows is a
	// consequence of the geometry rather than anything written for the occasion.
	TArray<StarForge::FThrusterDef> Damaged = MakeSymmetricArray();
	Damaged.RemoveAt(0);

	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a damaged array still builds"),
		Allocator.Build(Damaged, FVector::ZeroVector));

	StarForge::FWrench Desired;
	Desired.Force = FVector(2000.0, 0.0, 0.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	TestEqual(TEXT("one throttle per surviving thruster"), Throttles.Num(), Damaged.Num());

	for (double Throttle : Throttles)
	{
		TestTrue(TEXT("every throttle stays inside its physical range"),
			Throttle >= -1e-9 && Throttle <= 1.0 + 1e-9);
	}

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);
	TestTrue(TEXT("the ship still makes most of the requested forward force"),
		Achieved.Force.X > 0.5 * Desired.Force.X);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorRejectsDegenerateArrayTest,
	"StarForge.Core.ThrustAllocator.RejectsDegenerateArray",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorRejectsDegenerateArrayTest::RunTest(const FString& Parameters)
{
	// One thruster cannot span six axes. Building must fail loudly: a ship that
	// silently refuses to turn is far harder to diagnose than one that refuses to
	// finish spawning.
	TArray<StarForge::FThrusterDef> Single;
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector::ZeroVector;
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 1000.0;
	Single.Add(Thruster);

	StarForge::FThrustAllocator Allocator;
	TestFalse(TEXT("an array that cannot span six axes is rejected"),
		Allocator.Build(Single, FVector::ZeroVector));
	TestFalse(TEXT("and reports itself unbuilt"), Allocator.IsBuilt());

	return true;
}
```

- [x] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: `Cannot open include file: 'ThrustAllocator.h'`.

- [x] **Step 3: Реализовать**

Удалить `Source/StarForgeCore/Private/ThrusterDef.cpp` — его единственная функция переезжает сюда.

`Source/StarForgeCore/Public/ThrustAllocator.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ThrusterDef.h"

namespace StarForge
{
	/**
	 * Distributes a demanded force and torque across a redundant thruster array.
	 *
	 * More than six thrusters means infinitely many ways to produce any achievable
	 * wrench, so the solver picks the one with the least total thrust: a damped
	 * pseudo-inverse of the effectiveness matrix. That matrix depends only on where
	 * the thrusters are and where the centre of mass is, so its pseudo-inverse is
	 * computed once in Build and reused every frame -- what remains per frame is a
	 * matrix times a vector.
	 *
	 * Rebuild when the thruster set changes, which in practice means when one is
	 * destroyed. The ship's handling then degrades on its own, because the geometry
	 * really did change.
	 */
	class STARFORGECORE_API FThrustAllocator
	{
	public:
		/**
		 * Builds the effectiveness matrix and caches its damped pseudo-inverse.
		 *
		 * Returns false when the array cannot span all six axes -- one thruster, or
		 * a set that is collectively unable to yaw, lands here. Callers must check.
		 */
		bool Build(
			const TArray<FThrusterDef>& Thrusters,
			const FVector& CentreOfMass,
			double DampingLambda = 0.01);

		/** Throttles in [0, 1], one per thruster, for the requested wrench. */
		void Solve(const FWrench& Desired, TArray<double>& OutThrottles) const;

		/** The wrench a given set of throttles actually produces. */
		FWrench ComputeAchieved(const TArray<double>& Throttles) const;

		/**
		 * The most this array could possibly produce on one axis: the sum of what
		 * every thruster contributes to it, taken in absolute value.
		 *
		 * Index 0-2 are force axes, 3-5 torque axes. This is the honest scale for
		 * judging a parasitic component, and it is not the same as running every
		 * thruster at once -- a symmetric array at full throttle cancels its own
		 * torque to nothing, so measuring capability that way would compare a
		 * tolerance against zero.
		 */
		double GetAxisCapability(int32 Axis) const;

		int32 Num() const { return Columns.Num(); }
		bool IsBuilt() const { return bBuilt; }

	private:
		/** One row of the cached pseudo-inverse: how one thruster responds to each axis. */
		struct FThrusterResponse
		{
			double Row[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		};

		/** Column i is what thruster i contributes at full throttle. */
		TArray<FWrench> Columns;

		/** Row i maps a demanded wrench to thruster i's throttle. */
		TArray<FThrusterResponse> Response;

		bool bBuilt = false;
	};
}
```

`Source/StarForgeCore/Private/ThrustAllocator.cpp`:

```cpp
#include "ThrustAllocator.h"

#include "SmallMatrix.h"

namespace StarForge
{
	FWrench ComputeThrusterWrench(const FThrusterDef& Thruster, const FVector& CentreOfMass)
	{
		FWrench Result;
		// Normalised deliberately: the layout arrives from an editable asset, and an
		// unnormalised direction would otherwise scale thrust by its own length with
		// nothing to warn anybody.
		Result.Force = Thruster.Direction.GetSafeNormal() * Thruster.MaxThrust;
		Result.Torque = FVector::CrossProduct(Thruster.Location - CentreOfMass, Result.Force);
		return Result;
	}

	bool FThrustAllocator::Build(
		const TArray<FThrusterDef>& Thrusters,
		const FVector& CentreOfMass,
		double DampingLambda)
	{
		bBuilt = false;
		Columns.Reset();
		Response.Reset();

		if (Thrusters.Num() == 0)
		{
			return false;
		}

		for (const FThrusterDef& Thruster : Thrusters)
		{
			Columns.Add(ComputeThrusterWrench(Thruster, CentreOfMass));
		}

		// Gram matrix B * B^T, always 6x6 however many thrusters there are. The
		// damping term keeps it invertible when the array is merely near-degenerate
		// rather than exactly so.
		FMatrix6 Gram = FMatrix6::Zero();
		for (int32 Row = 0; Row < 6; ++Row)
		{
			for (int32 Col = 0; Col < 6; ++Col)
			{
				double Sum = 0.0;
				for (const FWrench& Column : Columns)
				{
					Sum += Column.Component(Row) * Column.Component(Col);
				}
				Gram.At(Row, Col) = Sum;
			}
			Gram.At(Row, Row) += DampingLambda;
		}

		FMatrix6 GramInverse;
		if (!Gram.TryInvert(GramInverse))
		{
			return false;
		}

		// Response = B^T * (B B^T + lambda I)^-1, one row per thruster.
		Response.SetNum(Columns.Num());
		for (int32 Index = 0; Index < Columns.Num(); ++Index)
		{
			for (int32 Col = 0; Col < 6; ++Col)
			{
				double Sum = 0.0;
				for (int32 Row = 0; Row < 6; ++Row)
				{
					Sum += Columns[Index].Component(Row) * GramInverse.At(Row, Col);
				}
				Response[Index].Row[Col] = Sum;
			}
		}

		// An array that cannot span all six axes produces a Gram matrix that only the
		// damping made invertible, and the solution it yields is meaningless. Detect
		// it by asking for each axis in turn and seeing whether anything comes back.
		for (int32 Axis = 0; Axis < 6; ++Axis)
		{
			double Probe[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			Probe[Axis] = 1.0;

			TArray<double> Throttles;
			Throttles.SetNumZeroed(Columns.Num());
			for (int32 Index = 0; Index < Columns.Num(); ++Index)
			{
				double Sum = 0.0;
				for (int32 Col = 0; Col < 6; ++Col)
				{
					Sum += Response[Index].Row[Col] * Probe[Col];
				}
				Throttles[Index] = Sum;
			}

			const FWrench Achieved = ComputeAchieved(Throttles);
			if (FMath::Abs(Achieved.Component(Axis)) < 1e-6)
			{
				Columns.Reset();
				Response.Reset();
				return false;
			}
		}

		bBuilt = true;
		return true;
	}

	void FThrustAllocator::Solve(const FWrench& Desired, TArray<double>& OutThrottles) const
	{
		OutThrottles.Reset();
		OutThrottles.SetNumZeroed(Columns.Num());

		if (!bBuilt)
		{
			return;
		}

		FWrench Remaining = Desired;

		// Clamp and correct. The first pass is the unconstrained least-thrust
		// solution; clamping it into the physical range leaves a residual, and each
		// further pass asks the same solver for whatever is still missing. Three
		// passes is where the spec settled: enough to recover most of a saturated
		// request, few enough to stay a fixed cost per frame.
		for (int32 Iteration = 0; Iteration < 3; ++Iteration)
		{
			for (int32 Index = 0; Index < Columns.Num(); ++Index)
			{
				double Delta = 0.0;
				for (int32 Col = 0; Col < 6; ++Col)
				{
					Delta += Response[Index].Row[Col] * Remaining.Component(Col);
				}
				OutThrottles[Index] = FMath::Clamp(OutThrottles[Index] + Delta, 0.0, 1.0);
			}

			Remaining = Desired - ComputeAchieved(OutThrottles);
		}
	}

	double FThrustAllocator::GetAxisCapability(int32 Axis) const
	{
		double Total = 0.0;
		for (const FWrench& Column : Columns)
		{
			Total += FMath::Abs(Column.Component(Axis));
		}
		return Total;
	}

	FWrench FThrustAllocator::ComputeAchieved(const TArray<double>& Throttles) const
	{
		FWrench Total;
		const int32 Count = FMath::Min(Throttles.Num(), Columns.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Total = Total + Columns[Index] * Throttles[Index];
		}
		return Total;
	}
}
```

- [x] **Step 4: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается семнадцать пройденных тестов. Результаты — из `Saved/Logs/StarForge.log`.

Если тест на паразитные составляющие не проходит с небольшим превышением — **не ослабляй допуск**. Порог в 1% взят из §10 спеки, и его нарушение симметричным набором означает ошибку в сборке матрицы, а не слишком строгую проверку.

- [x] **Step 5: Коммит**

```bash
git add Source/StarForgeCore/Public/ThrustAllocator.h Source/StarForgeCore/Private/ThrustAllocator.cpp Source/StarForgeCore/Private/Tests/ThrustAllocatorTest.cpp
git rm Source/StarForgeCore/Private/ThrusterDef.cpp
git commit -m "feat(core): solve thrust distribution across a redundant array

More than six thrusters leaves infinitely many ways to produce a wrench, so the
solver takes the least-thrust one and clamps it into the physical range, asking
again for whatever the clamp cost. The pseudo-inverse depends only on geometry, so
it is built once and a frame costs a matrix times a vector.

Building refuses an array that cannot span all six axes rather than returning the
answer damping alone made possible, because a ship that silently will not turn is
the hardest kind of bug to find."
```

---

### Task 4: Контур управления IFCS

Переводит намерение пилота в требуемое воздействие. Отсюда берётся ощущение массы.

**Files:**
- Create: `Source/StarForgeCore/Public/FlightController.h`
- Create: `Source/StarForgeCore/Private/FlightController.cpp`
- Create: `Source/StarForgeCore/Private/Tests/FlightControllerTest.cpp`

**Interfaces:**
- Consumes: `StarForge::FWrench`, `FShipMassProperties` из Task 2.
- Produces: `StarForge::FFlightIntent` (поля `LinearAxis`, `AngularAxis`, `bFlightAssistEnabled`), `StarForge::FFlightLimits` (поля `MaxSpeed`, `MaxAngularSpeed`, `LinearGain`, `AngularGain`), `StarForge::FRigidBodyState` (поля `LinearVelocity`, `AngularVelocity`), функция `StarForge::ComputeDemandedWrench(const FFlightIntent&, const FRigidBodyState&, const FShipMassProperties&, const FFlightLimits&) -> FWrench`.

- [x] **Step 1: Написать падающие тесты**

`Source/StarForgeCore/Private/Tests/FlightControllerTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "FlightController.h"

namespace
{
	StarForge::FShipMassProperties MakeAsymmetricShip()
	{
		// Deliberately unequal principal moments: a symmetric ship would hide a
		// missing gyroscopic term, because omega cross I omega vanishes when the
		// inertia is a multiple of the identity.
		StarForge::FShipMassProperties Mass;
		Mass.Mass = 10000.0;
		Mass.CentreOfMass = FVector::ZeroVector;
		Mass.InertiaDiagonal = FVector(20000.0, 60000.0, 50000.0);
		Mass.InertiaRotation = FQuat::Identity;
		return Mass;
	}

	StarForge::FFlightLimits MakeLimits()
	{
		StarForge::FFlightLimits Limits;
		Limits.MaxSpeed = 200.0;
		Limits.MaxAngularSpeed = FMath::DegreesToRadians(120.0);
		Limits.LinearGain = 2.0;
		Limits.AngularGain = 3.0;
		return Limits;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAcceleratesFromRestTest,
	"StarForge.Core.FlightController.AcceleratesFromRest",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAcceleratesFromRestTest::RunTest(const FString& Parameters)
{
	StarForge::FFlightIntent Intent;
	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);

	StarForge::FRigidBodyState State;

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits());

	// Full forward from rest: mass times the whole speed target times the gain.
	TestTrue(TEXT("full forward demand is mass times target speed times gain"),
		FMath::IsNearlyEqual(Demand.Force.X, 10000.0 * 200.0 * 2.0, 1.0));
	TestTrue(TEXT("no sideways demand"), FMath::Abs(Demand.Force.Y) < 1e-6);
	TestTrue(TEXT("no torque demand"), Demand.Torque.IsNearlyZero(1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerBrakesToStopTest,
	"StarForge.Core.FlightController.BrakesToStop",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerBrakesToStopTest::RunTest(const FString& Parameters)
{
	// Stick released while moving: assist demands the force that kills the speed.
	// This is what makes releasing the stick stop the ship rather than coast.
	StarForge::FFlightIntent Intent;

	StarForge::FRigidBodyState State;
	State.LinearVelocity = FVector(50.0, 0.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits());

	TestTrue(TEXT("braking demand opposes the motion"), Demand.Force.X < 0.0);
	TestTrue(TEXT("braking demand is mass times the excess speed times gain"),
		FMath::IsNearlyEqual(Demand.Force.X, -10000.0 * 50.0 * 2.0, 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerGyroscopicTermTest,
	"StarForge.Core.FlightController.GyroscopicTermIsPresent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerGyroscopicTermTest::RunTest(const FString& Parameters)
{
	// A ship already spinning about two axes at its commanded rate has no angular
	// error at all, yet Euler's equation still demands torque to hold that spin.
	// Without omega cross I omega this comes out zero and heavy ships turn like
	// light ones.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	StarForge::FRigidBodyState State;
	State.AngularVelocity = FVector(Limits.MaxAngularSpeed, Limits.MaxAngularSpeed, 0.0);

	StarForge::FFlightIntent Intent;
	Intent.AngularAxis = FVector(1.0, 1.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, Mass, Limits);

	// omega x (I omega) with omega = (w, w, 0) and I = diag(Ix, Iy, Iz):
	//   x: w*0 - 0*Iy*w            = 0
	//   y: 0*Ix*w - w*0            = 0
	//   z: w*(Iy*w) - w*(Ix*w)     = w^2 * (Iy - Ix)
	// So a yaw demand of w^2 * (60000 - 20000) and nothing on the other two axes.
	const double W = Limits.MaxAngularSpeed;
	const double ExpectedZ = W * W * (60000.0 - 20000.0);

	TestTrue(TEXT("holding a spin still demands torque"),
		!Demand.Torque.IsNearlyZero(1.0));
	TestTrue(TEXT("the gyroscopic demand matches Euler's equation"),
		FMath::IsNearlyEqual(Demand.Torque.Z, ExpectedZ, 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAssistOffTest,
	"StarForge.Core.FlightController.AssistOffIsDirectDemand",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAssistOffTest::RunTest(const FString& Parameters)
{
	// With assist off the stick stops asking for a speed and asks for thrust, so a
	// ship already at full speed keeps being pushed instead of being held there.
	StarForge::FFlightIntent Intent;
	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);
	Intent.bFlightAssistEnabled = false;

	StarForge::FRigidBodyState State;
	State.LinearVelocity = FVector(200.0, 0.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits());

	TestTrue(TEXT("assist off keeps demanding forward thrust at top speed"),
		Demand.Force.X > 0.0);

	StarForge::FFlightIntent Assisted = Intent;
	Assisted.bFlightAssistEnabled = true;

	const StarForge::FWrench AssistedDemand = StarForge::ComputeDemandedWrench(
		Assisted, State, MakeAsymmetricShip(), MakeLimits());

	TestTrue(TEXT("assist on stops demanding once the target speed is reached"),
		FMath::Abs(AssistedDemand.Force.X) < 1.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerConvergesTest,
	"StarForge.Core.FlightController.ConvergesWithoutOscillation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerConvergesTest::RunTest(const FString& Parameters)
{
	// Closed loop with a plain integrator and no thruster limit: commanded from
	// rest, the ship must reach its target speed and stay there. Overshoot here
	// would be felt in play as a ship that lurches past what the pilot asked for.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	StarForge::FFlightIntent Intent;
	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);

	StarForge::FRigidBodyState State;

	const double Step = 1.0 / 120.0;
	double WorstOvershoot = 0.0;

	for (int32 Tick = 0; Tick < 2400; ++Tick)
	{
		const StarForge::FWrench Demand =
			StarForge::ComputeDemandedWrench(Intent, State, Mass, Limits);

		State.LinearVelocity += (Demand.Force / Mass.Mass) * Step;
		WorstOvershoot = FMath::Max(WorstOvershoot, State.LinearVelocity.X - Limits.MaxSpeed);
	}

	TestTrue(TEXT("the ship reaches its commanded speed"),
		FMath::IsNearlyEqual(State.LinearVelocity.X, Limits.MaxSpeed, 0.5));
	TestTrue(TEXT("and does not overshoot it"), WorstOvershoot < 0.5);

	return true;
}
```

- [x] **Step 2: Убедиться, что не собирается**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

Ожидается: `Cannot open include file: 'FlightController.h'`.

- [x] **Step 3: Реализовать**

`Source/StarForgeCore/Public/FlightController.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ThrusterDef.h"

namespace StarForge
{
	/**
	 * What an operator asks the ship to do, in dimensionless axes.
	 *
	 * Deliberately unitless, because the meaning depends on the mode: with assist on
	 * these are fractions of the speed limits, with assist off they are fractions of
	 * the wrench the thrusters can produce. The operator fills the same structure
	 * either way and the controller interprets it -- which is why turning assist off
	 * is a different reading of the stick rather than a separate flight model.
	 */
	struct STARFORGECORE_API FFlightIntent
	{
		/** Ship-frame axes, each component in [-1, 1]. */
		FVector LinearAxis = FVector::ZeroVector;

		/** Pitch, yaw and roll, each component in [-1, 1]. */
		FVector AngularAxis = FVector::ZeroVector;

		bool bFlightAssistEnabled = true;
	};

	/** The envelope the assist holds the ship inside. */
	struct STARFORGECORE_API FFlightLimits
	{
		/** Metres per second. */
		double MaxSpeed = 200.0;

		/** Radians per second. */
		double MaxAngularSpeed = 2.0;

		/** How hard the assist corrects a linear velocity error, per second. */
		double LinearGain = 2.0;

		/** How hard the assist corrects an angular velocity error, per second. */
		double AngularGain = 3.0;
	};

	/** Ship motion as the controller needs it, all in the ship's own frame. */
	struct STARFORGECORE_API FRigidBodyState
	{
		/** Metres per second. */
		FVector LinearVelocity = FVector::ZeroVector;

		/** Radians per second. */
		FVector AngularVelocity = FVector::ZeroVector;
	};

	/**
	 * Turns an intent into the force and torque the thrusters should be asked for.
	 *
	 * The torque includes Euler's gyroscopic term, omega cross I omega. It is what
	 * makes a heavy ship feel heavy: without it, holding an established spin costs
	 * nothing and every hull turns like a fighter.
	 */
	STARFORGECORE_API FWrench ComputeDemandedWrench(
		const FFlightIntent& Intent,
		const FRigidBodyState& State,
		const FShipMassProperties& Mass,
		const FFlightLimits& Limits);
}
```

`Source/StarForgeCore/Private/FlightController.cpp`:

```cpp
#include "FlightController.h"

namespace
{
	/** Clamps each component into [-1, 1] without changing the vector's direction. */
	FVector ClampAxes(const FVector& Axes)
	{
		return FVector(
			FMath::Clamp(Axes.X, -1.0, 1.0),
			FMath::Clamp(Axes.Y, -1.0, 1.0),
			FMath::Clamp(Axes.Z, -1.0, 1.0));
	}
}

namespace StarForge
{
	FWrench ComputeDemandedWrench(
		const FFlightIntent& Intent,
		const FRigidBodyState& State,
		const FShipMassProperties& Mass,
		const FFlightLimits& Limits)
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

			const FVector TargetAngular = AngularAxis * Limits.MaxAngularSpeed;
			const FVector AngularError = TargetAngular - State.AngularVelocity;
			Demand.Torque = Mass.TransformByInertia(AngularError * Limits.AngularGain);
		}
		else
		{
			// The stick asks for thrust directly. Nothing holds a speed, so the ship
			// keeps accelerating and keeps whatever spin it is given -- the same pipe
			// below, a different reading of the same numbers above.
			Demand.Force = LinearAxis * Mass.Mass * Limits.MaxSpeed * Limits.LinearGain;
			Demand.Torque = Mass.TransformByInertia(
				AngularAxis * Limits.MaxAngularSpeed * Limits.AngularGain);
		}

		// Euler's gyroscopic term. Present in both modes: it is a property of a
		// spinning rigid body, not of the assist.
		const FVector AngularMomentum = Mass.TransformByInertia(State.AngularVelocity);
		Demand.Torque += FVector::CrossProduct(State.AngularVelocity, AngularMomentum);

		return Demand;
	}
}
```

- [x] **Step 4: Собрать и прогнать тесты**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" StarForgeEditor Win64 Development -Project="F:/UEProject/StarForge/StarForge.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/UEProject/StarForge/StarForge.uproject" -ExecCmds="Automation RunTests StarForge.Core;Quit" -unattended -nopause -nullrhi -nosplash -log
```

Ожидается двадцать два пройденных теста. Результаты — из `Saved/Logs/StarForge.log`.

- [x] **Step 5: Коммит**

```bash
git add Source/StarForgeCore/Public/FlightController.h Source/StarForgeCore/Private/FlightController.cpp Source/StarForgeCore/Private/Tests/FlightControllerTest.cpp
git commit -m "feat(core): turn a pilot's intent into a demanded wrench

With assist on the stick asks for a velocity and the demand is whatever closes the
gap, which is why letting go stops the ship. With it off the same numbers are read
as a thrust fraction instead. One pipe, two readings, rather than two flight
models to keep in agreement.

The torque carries Euler's gyroscopic term in both modes because it belongs to the
spinning body rather than to the assist. Without it holding an established spin is
free and every hull turns like a fighter."
```

---

## Точка проверки 2

После Task 4 математика полёта закончена и полностью покрыта тестами, но **ни разу не двигала настоящий корабль**.

Проверить перед тем, как писать следующий план:

- двадцать два теста проходят из консоли, без редактора;
- `StarForgeCore` по-прежнему зависит только от `Core` и `CoreUObject`;
- сборка без предупреждений.

Дальше пишется второй план этапа 2 — мост в Chaos, `UShipDataAsset`, компонент массива двигателей, пост пилота и маршрутизация ввода. Его первой задачей должен быть **повторный замер увода пассажира** после перевода корабля с кинематики на физику: Точка проверки 1 подтверждала перенос на кинематическом корабле, и смена источника трансформа — единственное, что может обесценить тот результат.

## Отклонение от плана, найденное при выполнении

План выполнен целиком, все двадцать два теста проходят, но **Task 3 пришлось
исправить в двух местах**. Оба дефекта были в самом плане, а не в исполнении.

### Фикстура не могла пройти собственный тест

`MakeSymmetricArray` ставила двигатели только по **+X, +Y, +Z**. Газ физически
зажат в [0, 1] — двигатель не работает назад, — поэтому такой набор не способен
дать момент без силы вообще: чтобы обнулить продольную силу, все вносящие вклад
газы обязаны быть нулём, а вместе с ними обнуляется и момент. Тест
`PureTorqueMakesNoForce` выдавал 500 Н·м из требуемых 1000 при 125 Н паразитной
силы против бюджета в 40 Н.

Повернуться, не сдвинувшись, можно только парой сил, а паре нужны два двигателя,
смотрящих в противоположные стороны. Фикстура теперь несёт на каждом углу
встречную пару по каждой оси — двадцать четыре двигателя вместо двенадцати.
Паразитные составляющие при этом падают ровно в ноль, то есть исходный
комментарий про симметрию был верен, просто набор ему не соответствовал.

### Решатель недодавал восьмую часть тяги

Встречные пары вскрыли второй дефект, уже в алгоритме. Незажатое решение всегда
предлагает встречному двигателю отрицательный газ; зажим его отбрасывает.
Пересчёт остатка **по всем** двигателям предлагает то же самое снова, поэтому за
проход добирается ровно половина недостачи: 1000 → 1500 → 1750 из 2000. Три
прохода сходятся к семи восьмым любого запроса.

Это не тестовая придирка: пилот, запросивший полный вперёд, всегда получал бы
87.5 % тяги — по всем осям, без единого места в коде, на которое можно показать.

`Solve` теперь считает остаток **только по незажатым двигателям**. Три прохода из
спеки при этом дают точное попадание. Цена — обращение 6×6 за проход после того,
как что-то насытилось, поэтому кэш пседообращения по всему массиву сохранён для
случая, когда не насытилось ничего. Утверждение в шапке плана, что «в кадре
остаётся умножение матрицы на вектор», больше неверно и здесь исправлено.

Альтернативы, отклонённые автором спеки: поднять число проходов примерно до
одиннадцати (правка константы спеки, дороже за кадр) и ослабить тесты под
фактические 87.5 % (корабль навсегда остаётся недодающим).

---

## Открытый вопрос к автору спеки

Спека требует одновременно крена **180°/с**, каюты радиусом **4 м** и компенсации гравиплит до **2 g**. Измерение на стенде дало `felt acceleration` до **194.67 м/с²** — почти 20 g. Даже одно вращение без линейного разгона даёт ω²r ≈ 39 м/с², то есть около 4 g у стены: вдвое выше порога, который спека сама назначает.

Три числа несовместимы. На этот план это не влияет — решатель считает, что физически возможно, а пределы задаются данными корабля. Но **до настройки полётной модели** во втором плане решение нужно: поднять порог гравиплит, снизить крен, или принять, что экипаж вне кресел сбивает с ног при штатном манёвре, и сделать это механикой.
