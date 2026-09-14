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
	/** A piece of text waiting for the panel behind it to be sized. See DrawHUD. */
	struct FReadoutText
	{
		FString Text;
		float X = 0.0f;
		float Y = 0.0f;
		FLinearColor Color = FLinearColor::White;
	};

	/** Every line of the readout, in order; queued, not drawn. */
	void DrawReadout(double& Y);

	/** Queues one line at the left margin and moves Y down a line. */
	void DrawLine(const FString& Text, double& Y, const FLinearColor& Color = FLinearColor::White);

	/** Queues text at X on the line at Y, without moving Y. For a line drawn in more than one colour. */
	void QueueText(const FString& Text, float X, double Y, const FLinearColor& Color);

	/** Flight lines: demand against achievement, throttles, saturation. */
	void DrawFlightBlock(double& Y);

	/** Gunner lines: who holds the post, and what state the turret is in. */
	void DrawGunnerBlock(double& Y);

	TArray<FReadoutText> PendingText;

	double ElapsedSeconds = 0.0;
};
