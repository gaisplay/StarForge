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

	/** Flight lines: demand against achievement, throttles, saturation. */
	void DrawFlightBlock(double& Y);

	/** Gunner lines: who holds the post, and what state the turret is in. */
	void DrawGunnerBlock(double& Y);

	double ElapsedSeconds = 0.0;
};
