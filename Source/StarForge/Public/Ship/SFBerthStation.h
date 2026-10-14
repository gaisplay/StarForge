#pragma once

#include "CoreMinimal.h"
#include "Ship/SFKitStation.h"
#include "SFBerthStation.generated.h"

/**
 * Step 1 of the station kit: one M01 berth block is a station (station-kit spec 1, 6). spec.py's
 * A_M01: one berth block at the assembly origin, yaw 0.
 */
UCLASS()
class STARFORGE_API ASFBerthStation : public ASFKitStation
{
	GENERATED_BODY()

public:
	ASFBerthStation();
};
