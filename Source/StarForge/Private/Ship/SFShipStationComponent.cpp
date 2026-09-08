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
	return SeatLocationMetres * StarForge::Units::CmPerMetre;
}

bool USFShipStationComponent::IsWithinReach(const FVector& OccupantWorldLocation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FVector SeatWorld = Owner->GetActorTransform().TransformPosition(GetSeatLocationCm());
	const double RadiusCm = InteractRadiusMetres * StarForge::Units::CmPerMetre;

	return FVector::DistSquared(SeatWorld, OccupantWorldLocation) <= RadiusCm * RadiusCm;
}
