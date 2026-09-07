#include "Debug/SFFrameDriftHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Player/SFCharacter.h"
#include "Player/SFShipboardMovementComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestShip.h"
#include "ShipFrameMath.h"

namespace
{
	/**
	 * Settled seconds required before any verdict is meaningful. Matches the validity
	 * gate on the checkpoint results form -- if these two ever disagree, the screen
	 * wins and the operator is misled.
	 */
	constexpr double MinimumValidMeasurementSeconds = 55.0;

	/**
	 * Backing panel geometry for the readout. At 50000 cd (see SFTestShip.cpp) the
	 * scene blew to pure white and every white or green line -- which is most of
	 * them, since white and green are what a healthy run looks like -- disappeared
	 * against it. A dark panel behind the text keeps it legible whatever the scene
	 * does. Sized generously: a panel slightly too large costs nothing, a line
	 * falling off the bottom of a too-small one is the same defect all over again.
	 */
	constexpr float PanelX = 20.0f;
	constexpr float PanelY = 36.0f;
	constexpr float PanelWidth = 760.0f;
	constexpr float PanelHeight = 480.0f;
}

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

	// Behind everything else: white and green, which is what most lines are on a
	// healthy run, vanished against the blown-out scene at the old light level.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), PanelX, PanelY, PanelWidth, PanelHeight);

	double Y = 48.0;

	// sf.ShipGravity is the negative-control switch (see SFShipboardMovementComponent):
	// with it off, gravity is left at world down and the carry breaks on its own. This
	// line has to be the first thing drawn, unconditionally, so a control run can never
	// be mistaken afterward for a real measurement.
	if (const IConsoleVariable* CVarShipGravity = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipGravity")))
	{
		if (CVarShipGravity->GetInt() == 0)
		{
			DrawLine(TEXT("NEGATIVE CONTROL - ship gravity disabled, this run is not a measurement"), Y, FLinearColor::Red);
		}
	}

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

	// Ahead of every number, because it is the one line that invalidates all of them:
	// while the occupant was unbased the carry did not run, so nothing below advanced
	// and the readout froze on stale values without going red anywhere else.
	const int64 Skipped = Movement->GetSkippedCarryFrames();
	if (Skipped > 0)
	{
		DrawLine(
			FString::Printf(TEXT("CARRY SKIPPED %lld ticks - the occupant came unattached"), Skipped),
			Y,
			FLinearColor::Red);
	}

	// The counter above is incremented from inside the carry, so it can never report
	// a gap that is still open. This is read from out here, every frame, and does.
	const int64 FramesSinceCarry = Movement->GetFramesSinceLastCarry();
	if (FramesSinceCarry > 1)
	{
		DrawLine(
			FString::Printf(TEXT("CARRY NOT RUNNING - %lld frames since the occupant was carried"), FramesSinceCarry),
			Y,
			FLinearColor::Red);
	}

	DrawLine(FString::Printf(TEXT("elapsed              %.1f s"), ElapsedSeconds), Y);
	DrawLine(FString::Printf(TEXT("drift now            %.3f cm"), Movement->GetDriftCm()), Y);

	// Green is a verdict, so it must not appear until there is something to have a
	// verdict about. A peak of zero over zero measured seconds is not a pass -- it
	// is an instrument that never ran, and it looks identical to a perfect result.
	// The bar is the same one the results form applies, and for the same reason: a
	// peak drawn from four seconds says nothing about a budget written per minute.
	const double MeasuredSeconds = Movement->GetMeasuredSeconds();
	const bool bMeasured = MeasuredSeconds >= MinimumValidMeasurementSeconds;
	const double PeakDrift = Movement->GetPeakDriftCm();
	const bool bWithinBudget = bMeasured && PeakDrift <= 1.0;

	DrawLine(
		FString::Printf(TEXT("drift peak           %.3f cm   [budget 1.0 cm/min]"), PeakDrift),
		Y,
		bWithinBudget ? FLinearColor::Green : (bMeasured ? FLinearColor::Red : FLinearColor::Yellow));

	const FString MeasuredWarning = bMeasured
		? FString()
		: FString::Printf(
			TEXT("   <- NOTHING MEASURED YET (need %.0f s unbroken)"),
			MinimumValidMeasurementSeconds);

	DrawLine(
		FString::Printf(TEXT("measured             %.1f s of settled time%s"),
			MeasuredSeconds,
			*MeasuredWarning),
		Y,
		bMeasured ? FLinearColor::White : FLinearColor::Yellow);

	// Separate failure mode: the average position can be perfect while the
	// picture shakes too hard to aim through.
	DrawLine(FString::Printf(TEXT("jitter peak          %.3f cm/tick"),
		Movement->GetPeakJitterCm()), Y);

	// The live figures above reset the instant the operator commands any movement,
	// which erases a good result before it can be written down. This block reports
	// the longest settled interval completed so far instead, and nothing but a new
	// SetShipFrame call ever clears it, so it survives him walking around afterward.
	Y += 10.0;
	DrawLine(TEXT("--- completed result (survives walking) ---"), Y);

	const double BestSeconds = Movement->GetBestCompletedSeconds();
	const double BestPeakDrift = Movement->GetBestCompletedPeakDriftCm();
	const bool bBestMeasured = BestSeconds >= MinimumValidMeasurementSeconds;
	const bool bBestWithinBudget = bBestMeasured && BestPeakDrift <= 1.0;

	const FLinearColor BestColor = bBestWithinBudget
		? FLinearColor::Green
		: (bBestMeasured ? FLinearColor::Red : FLinearColor::Yellow);

	DrawLine(
		FString::Printf(TEXT("best drift peak      %.3f cm   [budget 1.0 cm/min]"), BestPeakDrift),
		Y,
		BestColor);

	DrawLine(
		FString::Printf(TEXT("best measured        %.1f s of settled time"), BestSeconds),
		Y,
		bBestMeasured ? FLinearColor::White : FLinearColor::Yellow);

	DrawLine(
		FString::Printf(TEXT("best jitter peak     %.3f cm/tick"),
			Movement->GetBestCompletedPeakJitterCm()),
		Y);

	const FVector Local = Movement->GetLocalLocation();
	DrawLine(FString::Printf(TEXT("ship-local position  %.1f  %.1f  %.1f cm"),
		Local.X, Local.Y, Local.Z), Y);

	if (Frame && Ship)
	{
		DrawLine(FString::Printf(TEXT("ship speed           %.1f m/s   [ceiling 400]"),
			Ship->GetSpeedMetersPerSec()), Y);

		const FVector Omega = Frame->GetAngularVelocityRadPerSec();

		// The design's 180 deg/s is the roll ceiling alone, but the profile loads all
		// three axes at once, so |omega| sits above it permanently and a correctly
		// flown run read as forever over budget. Pitch and yaw sweep in quadrature and
		// so contribute their full amplitude at every phase; the honest ceiling for a
		// magnitude is the two combined.
		const double CombinedCeilingDegPerSec = FMath::Sqrt(
			FMath::Square(Ship->PeakPitchYawRateDegPerSec) +
			FMath::Square(Ship->PeakRollRateDegPerSec));

		DrawLine(FString::Printf(TEXT("angular rate         %.1f deg/s   [combined ceiling %.1f]"),
			FMath::RadiansToDegrees(Omega.Size()), CombinedCeilingDegPerSec), Y);

		// omega x (omega x r) is the centripetal term of one frame only. The angular
		// velocity and the ship's linear acceleration are both world-space while the
		// offset is ship-local, so mixing them produced a figure wrong by an amount
		// that swung with the ship's attitude. Bring the two vectors into the ship.
		const FTransform FrameTransform = Frame->GetFrameTransform();
		const FVector LocalOmega = FrameTransform.InverseTransformVectorNoScale(Omega);
		const FVector LocalAccel = FrameTransform.InverseTransformVectorNoScale(
			Ship->GetLinearAccelMetersPerSecSq());

		const FVector Felt = StarForge::ShipFrame::ComputeFeltAcceleration(
			LocalAccel, LocalOmega, Local / 100.0);

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
