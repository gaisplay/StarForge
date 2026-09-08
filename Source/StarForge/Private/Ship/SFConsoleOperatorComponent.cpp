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

	/**
	 * Whether the console operator takes the pilot seat at startup.
	 *
	 * On by default, because the headless flight checks depend on somebody flying.
	 * Turned off so a person can take the seat instead -- without it, testing a
	 * player sitting down would mean commenting out code and rebuilding, and a check
	 * that requires editing what it checks is not a check.
	 */
	TAutoConsoleVariable<int32> CVarConsoleOperator(TEXT("sf.ConsoleOperator"), 1,
		TEXT("1: the console operator takes the pilot seat at startup. 0: it leaves the seat free."),
		ECVF_Default);
}

USFConsoleOperatorComponent::USFConsoleOperatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFConsoleOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (CVarConsoleOperator.GetValueOnGameThread() == 0)
	{
		return;
	}

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
