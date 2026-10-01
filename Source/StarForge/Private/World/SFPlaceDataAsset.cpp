#include "World/SFPlaceDataAsset.h"

#include "Ship/SFTestStation.h"

void USFPlaceDataAsset::PopulateOpenSpaceWithOneStation()
{
	// PopulateSymmetricLayout's three calls, for its reason: the details panel wraps a CallInEditor
	// press in a transaction but never calls Modify() on the object, so without them Ctrl+Z does not
	// undo the fill and the package is never dirtied -- closing the editor throws the station away
	// without a save prompt, and an asset with Carriers empty is a 1.2 KB file that looks finished.
	Modify();

	Carriers.Empty();

	FSFPlaceCarrier Station;
	Station.CarrierClass = ASFTestStation::StaticClass();

	// The same name the loop rig gives it, so "Frame handover: Station -> Ship" reads
	// identically on both rigs and the two runs can be compared line for line.
	Station.CarrierName = TEXT("Station");
	Station.LocationInPlaceMetres = FVector::ZeroVector;
	Station.Rotation = FRotator::ZeroRotator;

	Carriers.Add(Station);

#if WITH_EDITOR
	// Nothing broadcasts a property change for a function call, so the panel would keep drawing
	// the empty array and the button would look as if it did nothing.
	PostEditChange();
#endif

	// Modify() dirties the package only inside a transaction; this does it whichever way the call
	// arrived, so the asterisk and the save prompt are there before the editor is closed.
	MarkPackageDirty();
}

const TCHAR* USFPlaceDataAsset::KindName(ESFPlaceKind InKind)
{
	switch (InKind)
	{
	case ESFPlaceKind::OpenSpace: return TEXT("open space");
	}

	return TEXT("unknown kind");
}
