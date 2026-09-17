#include "World/SFPlaceDataAsset.h"

#include "Ship/SFTestStation.h"

void USFPlaceDataAsset::PopulateOpenSpaceWithOneStation()
{
	Carriers.Empty();

	FSFPlaceCarrier Station;
	Station.CarrierClass = ASFTestStation::StaticClass();

	// The same name the loop rig gives it, so "Frame handover: Station -> Ship" reads
	// identically on both rigs and the two runs can be compared line for line.
	Station.CarrierName = TEXT("Station");
	Station.LocationInPlaceMetres = FVector::ZeroVector;
	Station.Rotation = FRotator::ZeroRotator;

	Carriers.Add(Station);
}

const TCHAR* USFPlaceDataAsset::KindName(ESFPlaceKind InKind)
{
	switch (InKind)
	{
	case ESFPlaceKind::OpenSpace: return TEXT("open space");
	}

	return TEXT("unknown kind");
}
