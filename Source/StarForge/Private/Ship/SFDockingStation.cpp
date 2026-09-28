#include "Ship/SFDockingStation.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"

TArray<AActor*> ISFDockingStation::FindAll(const UWorld* World, const AActor* Except)
{
	TArray<AActor*> Stations;
	if (!World)
	{
		return Stations;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor != Except && Actor->Implements<USFDockingStation>())
		{
			Stations.Add(Actor);
		}
	}

	Stations.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	return Stations;
}
