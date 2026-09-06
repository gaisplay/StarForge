#include "Debug/SFFrameDriftHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Player/SFCharacter.h"
#include "Player/SFShipboardMovementComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestShip.h"
#include "ShipFrameMath.h"

void ASFFrameDriftHUD::DrawLine(const FString& Text, double& Y, const FLinearColor& Color)
{
	if (Canvas)
	{
		Canvas->SetDrawColor(Color.ToFColor(true));
		Canvas->DrawText(GEngine->GetMediumFont(), Text, 32.0f, static_cast<float>(Y));
	}
	Y += 22.0;
}

void ASFFrameDriftHUD::DrawHUD()
{
	Super::DrawHUD();

	double Y = 48.0;

	const ASFCharacter* Occupant = Cast<ASFCharacter>(GetOwningPawn());
	if (!Occupant)
	{
		// A blank screen during a checkpoint run is indistinguishable from a rig that
		// is simply quiet, so every exit from this function has to say why.
		DrawLine(TEXT("NO SFCHARACTER POSSESSED - there is no occupant to measure"), Y, FLinearColor::Red);
		return;
	}

	const USFShipboardMovementComponent* Movement = Occupant->GetShipboardMovement();
	if (!Movement)
	{
		DrawLine(TEXT("NO SHIPBOARD MOVEMENT COMPONENT - nothing is being measured"), Y, FLinearColor::Red);
		return;
	}

	ElapsedSeconds += GetWorld()->GetDeltaSeconds();

	const USFShipFrameComponent* Frame = Movement->GetShipFrame();
	const ASFTestShip* Ship = Frame ? Cast<ASFTestShip>(Frame->GetOwner()) : nullptr;

	DrawLine(FString::Printf(TEXT("elapsed              %.1f s"), ElapsedSeconds), Y);
	DrawLine(FString::Printf(TEXT("drift now            %.3f cm"), Movement->GetDriftCm()), Y);

	// Green is a verdict, so it must not appear until there is something to have a
	// verdict about. A peak of zero over zero measured seconds is not a pass -- it
	// is an instrument that never ran, and it looks identical to a perfect result.
	const double MeasuredSeconds = Movement->GetMeasuredSeconds();
	const bool bMeasured = MeasuredSeconds > 1.0;
	const double PeakDrift = Movement->GetPeakDriftCm();
	const bool bWithinBudget = bMeasured && PeakDrift <= 1.0;

	DrawLine(
		FString::Printf(TEXT("drift peak           %.3f cm   [budget 1.0 cm/min]"), PeakDrift),
		Y,
		bWithinBudget ? FLinearColor::Green : (bMeasured ? FLinearColor::Red : FLinearColor::Yellow));

	DrawLine(
		FString::Printf(TEXT("measured             %.1f s of settled time%s"),
			MeasuredSeconds,
			bMeasured ? TEXT("") : TEXT("   <- NOTHING MEASURED YET")),
		Y,
		bMeasured ? FLinearColor::White : FLinearColor::Yellow);

	// Separate failure mode: the average position can be perfect while the
	// picture shakes too hard to aim through.
	DrawLine(FString::Printf(TEXT("jitter peak          %.3f cm/tick"),
		Movement->GetPeakJitterCm()), Y);

	const FVector Local = Movement->GetLocalLocation();
	DrawLine(FString::Printf(TEXT("ship-local position  %.1f  %.1f  %.1f cm"),
		Local.X, Local.Y, Local.Z), Y);

	if (Frame && Ship)
	{
		DrawLine(FString::Printf(TEXT("ship speed           %.1f m/s   [ceiling 400]"),
			Ship->GetSpeedMetersPerSec()), Y);

		const FVector Omega = Frame->GetAngularVelocityRadPerSec();
		DrawLine(FString::Printf(TEXT("angular rate         %.1f deg/s   [ceiling 180]"),
			FMath::RadiansToDegrees(Omega.Size())), Y);

		const FVector LinearAccel = Ship->GetLinearAccelMetersPerSecSq();
		const FVector Felt = StarForge::ShipFrame::ComputeFeltAcceleration(
			LinearAccel, Omega, Local / 100.0);

		const bool bKnockdown = StarForge::ShipFrame::ExceedsGravPlateLimit(Felt);
		DrawLine(
			FString::Printf(TEXT("felt acceleration    %.2f m/s2   [limit %.2f]"),
				Felt.Size(), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq),
			Y,
			bKnockdown ? FLinearColor::Yellow : FLinearColor::White);
	}
	else
	{
		DrawLine(
			TEXT("NO SHIP FRAME BOUND - the occupant is standing in world space"),
			Y,
			FLinearColor::Red);
		DrawLine(
			TEXT("every figure above is meaningless"),
			Y,
			FLinearColor::Red);
	}
}
