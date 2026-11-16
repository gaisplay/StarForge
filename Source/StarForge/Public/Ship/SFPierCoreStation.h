#pragma once

#include "CoreMinimal.h"
#include "Ship/SFKitStation.h"
#include "SFPierCoreStation.generated.h"

/**
 * Step 2 of the station kit: the pier core, spec.py's B_K3core -- two M01 blocks side by side, both
 * berths on -Y, two ports on one carrier (station-kit spec 7.1). Not sheet A16's full K3 pier, which
 * is built of spine blocks with a berth on each of their +Y and -Y faces (the mechanics note,
 * contradiction 3).
 *
 * The two halls are not connected (owner's decision 5): no corridor, no back door, no walking joint.
 * Nothing in the port mechanism depends on them.
 */
UCLASS()
class STARFORGE_API ASFPierCoreStation : public ASFKitStation
{
	GENERATED_BODY()

public:
	ASFPierCoreStation();
};
