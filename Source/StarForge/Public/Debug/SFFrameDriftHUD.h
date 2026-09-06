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
