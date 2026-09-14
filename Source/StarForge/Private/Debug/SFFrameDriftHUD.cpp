#include "Debug/SFFrameDriftHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SFCharacter.h"
#include "Player/SFShipboardMovementComponent.h"
#include "Ship/SFDamageableComponent.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFGunnerStationComponent.h"
#include "Ship/SFPilotStationComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestShip.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "Ship/SFTurretComponent.h"
#include "ShipFrameMath.h"
#include "TurretMath.h"
#include "Units.h"

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
	 * does.
	 *
	 * The height is not among these, and it used to be. It was 480, then 640 for the
	 * flight block, 760 for the gunner block and 820 for the frame and dock lines --
	 * each time raised by hand, by the same arithmetic, after the last line had already
	 * fallen off the bottom or was about to. Every block still to come would have needed
	 * it done again. The lines are queued instead, and the panel is sized to where the
	 * last of them ended (see DrawHUD), so a line added to the readout cannot outgrow it.
	 */
	constexpr float PanelX = 20.0f;
	constexpr float PanelY = 36.0f;
	constexpr float PanelWidth = 760.0f;

	namespace ReadoutLayout
	{
		/** Where the first line's text starts; the gap above it is also left below the last. */
		constexpr double FirstLineY = 48.0;
		constexpr float TextX = 32.0f;
		constexpr double LineHeight = 22.0;

		/**
		 * The shields line is drawn in columns so a face at zero can be coloured on its own.
		 * Fixed columns rather than text measured and laid end to end: a face regenerating from
		 * 640 to 1000 would otherwise shift every face after it on every frame. A hundred pixels
		 * a column holds "+X 1000" with room to spare in the medium font; six from 120 end
		 * before 720, inside the panel's right edge at 780.
		 */
		constexpr float ShieldFirstColumnX = 120.0f;
		constexpr float ShieldColumnWidth = 100.0f;
	}
}

void ASFFrameDriftHUD::QueueText(const FString& Text, float X, double Y, const FLinearColor& Color)
{
	FReadoutText& Queued = PendingText.AddDefaulted_GetRef();
	Queued.Text = Text;
	Queued.X = X;
	Queued.Y = static_cast<float>(Y);
	Queued.Color = Color;
}

void ASFFrameDriftHUD::DrawLine(const FString& Text, double& Y, const FLinearColor& Color)
{
	QueueText(Text, ReadoutLayout::TextX, Y, Color);
	Y += ReadoutLayout::LineHeight;
}

void ASFFrameDriftHUD::DrawHUD()
{
	Super::DrawHUD();

	PendingText.Reset();
	double Y = ReadoutLayout::FirstLineY;
	DrawReadout(Y);

	// Behind everything else: white and green, which is what most lines are on a
	// healthy run, vanished against the blown-out scene at the old light level. Drawn
	// after the readout has been laid out and before any of it is put on the canvas,
	// which is the whole reason the lines are queued: the panel's bottom is where the
	// last line ended, on every exit from DrawReadout, including the early ones.
	const double PanelBottom = Y + (ReadoutLayout::FirstLineY - PanelY);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), PanelX, PanelY, PanelWidth,
		static_cast<float>(PanelBottom - PanelY));

	if (!Canvas)
	{
		return;
	}

	for (const FReadoutText& Queued : PendingText)
	{
		Canvas->SetDrawColor(Queued.Color.ToFColor(true));
		Canvas->DrawText(GEngine->GetMediumFont(), Queued.Text, Queued.X, Queued.Y);
	}
}

void ASFFrameDriftHUD::DrawReadout(double& Y)
{
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

	// The third of the lines that give the drift figure its meaning. The two above say
	// the carry failed; this one says the man was knocked down, and a slide under it is
	// not a carry failure -- "drift" used to mean only the first. Yellow with the
	// mechanic off, because that run is not comparable with one that has it.
	if (!Movement->IsKnockdownEnabled())
	{
		DrawLine(TEXT("footing  KNOCKDOWN OFF (sf.Knockdown 0) - checkpoint 1's rig, not this plan's"), Y, FLinearColor::Yellow);
	}
	else if (Movement->IsKnockedDown())
	{
		const FVector Push = Movement->GetKnockdownPushMetersPerSecSq();

		// The length of this fall, not the interval's tally. The tally is dropped with
		// the peaks on every frame of the settle window, so a man lying on the deck
		// would be printed as down for 0.0 s through the whole half second the
		// instrument spends restarting -- a readout looking broken while it works.
		DrawLine(FString::Printf(TEXT("footing  KNOCKED DOWN %.1f s   push (%.1f %.1f %.1f) m/s2 ship axes   %s"),
			Movement->GetCurrentKnockdownSeconds(), Push.X, Push.Y, Push.Z, *Movement->DescribeFooting()),
			Y, FLinearColor(1.0f, 0.45f, 0.0f));
	}
	else
	{
		DrawLine(FString::Printf(TEXT("footing  intact%s   knocked down %.1f s this interval   %s"),
			Movement->IsStrappedIn() ? TEXT(" (strapped in)") : TEXT(""),
			Movement->GetKnockedDownSeconds(), *Movement->DescribeFooting()), Y);
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

	// Orange, not red, when the interval had him on the deck: the figure is over
	// budget because he slid, and the word beside it says so. Red stays for a figure
	// over budget with him on his feet, which is the carry's own failure.
	//
	// Read from the state as well as the tally, for the reason the footing line gives:
	// through the settle window the tally is zero on every frame, and this colour would
	// go red for the very spec 6 case the mechanic was written to produce.
	const bool bDownThisInterval = Movement->IsKnockedDown() || Movement->GetKnockedDownSeconds() > 0.0;
	DrawLine(
		FString::Printf(TEXT("drift peak           %.3f cm   [budget 1.0 cm/min]%s"), PeakDrift,
			bDownThisInterval ? TEXT("   <- includes his slide while knocked down") : TEXT("")),
		Y,
		bWithinBudget ? FLinearColor::Green
			: (bMeasured ? (bDownThisInterval ? FLinearColor(1.0f, 0.45f, 0.0f) : FLinearColor::Red) : FLinearColor::Yellow));

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

	const bool bBestHadHimDown = Movement->GetBestCompletedKnockedDownSeconds() > 0.0;
	DrawLine(
		FString::Printf(TEXT("best drift peak      %.3f cm   [budget 1.0 cm/min]%s"), BestPeakDrift,
			bBestHadHimDown ? TEXT("   <- includes a slide while knocked down") : TEXT("")),
		Y,
		bBestWithinBudget ? FLinearColor::Green
			: (bBestMeasured ? (bBestHadHimDown ? FLinearColor(1.0f, 0.45f, 0.0f) : FLinearColor::Red) : FLinearColor::Yellow));

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

	// Who carries him, and what the last handover cost. Yellow until there has been one,
	// because "no handover yet" and "a handover of zero centimetres" are the same text
	// otherwise and mean opposite things. Red when the floor under him has belonged to
	// somebody else for ten frames: that outranks every figure above it, since the carry
	// is following the frame and the frame is wrong.
	// The same threshold the log uses, in the same unit, so the line does not go red on
	// a crossing the log stayed silent about.
	const bool bMismatch = Movement->IsFrameMismatchReported();
	const bool bNoHandoverYet = Movement->GetLastHandoverText().IsEmpty();
	DrawLine(
		FString::Printf(TEXT("frame   bound to %s   last handover: %s"),
			*Movement->GetFrameName(),
			bNoHandoverYet ? TEXT("none yet") : *Movement->GetLastHandoverText()),
		Y,
		bMismatch ? FLinearColor::Red : (bNoHandoverYet ? FLinearColor::Yellow : FLinearColor::White));

	// The dock line is about the ship, not about whichever carrier he happens to be
	// standing in: a man on the station still needs to see whether his ship is attached
	// to it. Found the same way the flight block finds it, for the same reason.
	const ASFTestShip* DockShip = Cast<ASFTestShip>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFTestShip::StaticClass()));
	const USFDockingComponent* Docking = DockShip ? DockShip->GetDocking() : nullptr;

	if (!Docking)
	{
		DrawLine(TEXT("dock    -- no ship with a docking component in the level"), Y, FLinearColor::Red);
	}
	else if (!Docking->HasStation())
	{
		DrawLine(TEXT("dock    free -- NO STATION in this level"), Y, FLinearColor::Red);
	}
	else
	{
		switch (Docking->GetState())
		{
		case ESFDockingState::Docked:
			DrawLine(TEXT("dock    docked"), Y, FLinearColor::Yellow);
			break;
		case ESFDockingState::Capturing:
			DrawLine(FString::Printf(TEXT("dock    capturing %.1f of %.1f s"),
				Docking->GetCaptureElapsedSeconds(), Docking->GetCaptureSeconds()),
				Y, FLinearColor(1.0f, 0.45f, 0.0f));
			break;
		default:
		{
			const StarForge::Docking::FDockingApproach Approach = Docking->GetLastApproach();
			const FVector At = Docking->GetPortOffsetShipLocalMetres();
			const bool bCanDock = Docking->CanDockNow();

			// Named, and built before the line rather than inside it, exactly as
			// LogFlightState builds the same verdict. A Printf temporary inside a ternary
			// arm does survive to the end of the full expression, so the shorter form is
			// correct -- but it is one refactor away from a dangling TCHAR*, and the two
			// places that print this verdict should not differ in how safe they are.
			const FString Verdict = bCanDock
				? FString(TEXT("CAN DOCK"))
				: FString::Printf(TEXT("out of window: %s"),
					USFDockingComponent::RefusalName(Docking->GetLastRefusal()));

			// Green only when the key would be accepted this instant. A window that
			// reported itself open while the command would refuse is the same defect as a
			// turret at its stop reporting itself on target.
			DrawLine(
				FString::Printf(TEXT("dock    free | port %.1f m at (%+.1f %+.1f %+.1f) ship  %.1f m/s  fwd %.1f  up %.1f  -- %s"),
					Approach.Distance, At.X, At.Y, At.Z, Approach.SpeedMetresPerSec,
					FMath::RadiansToDegrees(Approach.ForwardAngleRad),
					FMath::RadiansToDegrees(Approach.UpAngleRad),
					*Verdict),
				Y,
				bCanDock ? FLinearColor::Green : FLinearColor::White);
			break;
		}
		}
	}

	// Below frame, like dock and for the same reason: while the frame line is red, whatever is
	// under it describes something other than what is happening. Read from the same ship as the
	// dock line -- the ship, not whichever carrier he is standing in.
	const USFDamageableComponent* Damageable = DockShip ? DockShip->GetDamageable() : nullptr;
	if (!Damageable)
	{
		DrawLine(TEXT("shields -- no ship with a damageable component in the level"), Y, FLinearColor::Red);
	}
	else if (!Damageable->IsReady())
	{
		// Not six red zeros and a hull of 0.0 / 0.0: that is what a ship shot to pieces looks like.
		DrawLine(TEXT("shields -- damageable component NOT ready, no shield or hull figures (see the log)"), Y, FLinearColor::Red);
	}
	else
	{
		// A face on its own in red when it is at zero, because a hit there goes straight to the
		// hull, and one red number among six says which side that is.
		QueueText(TEXT("shields"), ReadoutLayout::TextX, Y, FLinearColor::White);
		for (int32 i = 0; i < StarForge::Damage::FaceCount; ++i)
		{
			const StarForge::Damage::EShipFace Face = static_cast<StarForge::Damage::EShipFace>(i);
			const double Charge = Damageable->GetFaceCharge(Face);
			QueueText(FString::Printf(TEXT("%s %.0f"), StarForge::Damage::FaceName(Face), Charge),
				ReadoutLayout::ShieldFirstColumnX + ReadoutLayout::ShieldColumnWidth * static_cast<float>(i),
				Y,
				Charge <= 0.0 ? FLinearColor::Red : FLinearColor::White);
		}
		Y += ReadoutLayout::LineHeight;

		const bool bPoweredDown = Damageable->IsPoweredDown();
		DrawLine(FString::Printf(TEXT("hull    %.1f / %.1f%s"),
			Damageable->GetHullStrength(), Damageable->GetHullCapacity(),
			bPoweredDown ? TEXT("   powered down") : TEXT("")),
			Y,
			bPoweredDown ? FLinearColor(1.0f, 0.45f, 0.0f) : FLinearColor::White);
	}

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

		// The figure the footing decision was taken on, not a second computation of it
		// here: the screen has to show the number the mechanic saw, or the two can
		// disagree and nobody will know which to believe.
		const FVector Felt = Movement->GetFeltAccelerationMetersPerSecSq();
		const bool bOverLimit = StarForge::ShipFrame::ExceedsGravPlateLimit(Felt);
		DrawLine(
			FString::Printf(TEXT("felt acceleration    %.2f m/s2   [limit %.2f]%s"),
				Felt.Size(), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq,
				(bOverLimit && !Movement->IsKnockedDown())
					? TEXT("   over the limit, on his feet: strapped in or sf.Knockdown 0")
					: TEXT("")),
			Y,
			Movement->IsKnockedDown()
				? FLinearColor(1.0f, 0.45f, 0.0f)
				: (bOverLimit ? FLinearColor::Yellow : FLinearColor::White));
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

	// The flight numbers share this readout rather than forming a second HUD: the
	// drift figures matter during flight for the same reasons they mattered standing
	// still, and two HUDs cannot both be on screen at once.
	DrawLine(TEXT(""), Y);
	DrawFlightBlock(Y);
	DrawGunnerBlock(Y);

	// The other ship -- on the combat rig, the pirate. It is the ship that is not the man's own:
	// the one whose frame carries him, or failing that the one the dock line reports.
	const ASFTestShip* OwnShip = Ship ? Ship : DockShip;
	const ASFTestShip* Enemy = nullptr;
	for (TActorIterator<ASFTestShip> It(GetWorld()); It; ++It)
	{
		if (*It != OwnShip)
		{
			Enemy = *It;
			break;
		}
	}

	if (!Enemy)
	{
		DrawLine(TEXT("combat  no other ship in this level"), Y);
		return;
	}

	const double EnemyDistanceMetres = OwnShip
		? StarForge::Units::CmToMetres(FVector::Dist(OwnShip->GetActorLocation(), Enemy->GetActorLocation()))
		: 0.0;

	// Who flies it, with "standing down" when the post is held and declining: sf.PirateAI 0 and a
	// powered-down pirate both read that way, and a pirate that simply sits still does not.
	const USFPilotStationComponent* EnemyPilot = Enemy->GetPilotStation();
	const FString EnemyFlownBy = EnemyPilot
		? EnemyPilot->GetOperatorDisplayName()
			+ ((EnemyPilot->IsOccupied() && !EnemyPilot->IsOperatorFlying()) ? TEXT(" (standing down)") : TEXT(""))
		: FString(TEXT("no pilot station"));

	const USFDamageableComponent* EnemyDamage = Enemy->GetDamageable();
	if (!EnemyDamage || !EnemyDamage->IsReady())
	{
		DrawLine(FString::Printf(TEXT("combat  [%s] %.1f m   flown by %s   damageable component NOT ready"),
			*Enemy->GetName(), EnemyDistanceMetres, *EnemyFlownBy), Y, FLinearColor::Red);
		return;
	}

	int32 FacesDown = 0;
	for (int32 i = 0; i < StarForge::Damage::FaceCount; ++i)
	{
		if (EnemyDamage->GetFaceCharge(static_cast<StarForge::Damage::EShipFace>(i)) <= 0.0)
		{
			++FacesDown;
		}
	}

	const bool bEnemyDown = EnemyDamage->IsPoweredDown();
	DrawLine(FString::Printf(TEXT("combat  [%s] %.1f m   flown by %s   hull %.1f / %.1f   faces down %d of %d   %s"),
		*Enemy->GetName(), EnemyDistanceMetres, *EnemyFlownBy,
		EnemyDamage->GetHullStrength(), EnemyDamage->GetHullCapacity(),
		FacesDown, StarForge::Damage::FaceCount,
		bEnemyDown ? TEXT("powered down") : TEXT("powered up")),
		Y,
		bEnemyDown ? FLinearColor(1.0f, 0.45f, 0.0f) : FLinearColor::White);
}

void ASFFrameDriftHUD::DrawFlightBlock(double& Y)
{
	const ASFTestShip* Ship = Cast<ASFTestShip>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFTestShip::StaticClass()));

	if (!Ship || !Ship->GetThrusters())
	{
		DrawLine(TEXT("flight: no ship with a thruster array in the level"), Y, FLinearColor::Red);
		return;
	}

	const USFThrusterArrayComponent* Array = Ship->GetThrusters();

	// Said before anything else. An empty flight block looks identical whether the
	// ship is idle or the array never built, and only this line separates them.
	if (!Array->IsReady())
	{
		DrawLine(TEXT("flight: thruster array NOT built -- ship cannot fly"), Y, FLinearColor::Red);
		return;
	}

	// A missing variable used to be reported as its default, so a build without
	// sf.ShipProfile printed "scripted profile (sf.ShipProfile 1)" -- a value nobody
	// had read, in the confident shape of one that had been. It is said as unknown
	// instead, in red, because not knowing which mode the ship is in makes every
	// figure below it unattributable to a mode.
	const IConsoleVariable* Profile =
		IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile"));

	if (!Profile)
	{
		DrawLine(TEXT("flight mode: UNKNOWN -- sf.ShipProfile not found"), Y, FLinearColor::Red);
	}
	else
	{
		const bool bProfileOn = Profile->GetInt() != 0;
		DrawLine(FString::Printf(TEXT("flight mode: %s"),
			bProfileOn ? TEXT("scripted profile (sf.ShipProfile 1)") : TEXT("thrust (sf.ShipProfile 0)")),
			Y, bProfileOn ? FLinearColor::Yellow : FLinearColor::Green);
	}

	// The angular triples carry their axis order on screen. This readout is the only
	// place a roll/pitch swap becomes visible, and three bare numbers invite reading the
	// wrong component -- either missing a real swap or inventing one that is not there.
	//
	// The intent comes from the pilot station now, because the ship no longer runs the
	// control loop and no longer knows what was asked of it. A missing station is said
	// in red rather than skipped, for the same reason the "array NOT built" line above
	// is: a readout that goes quiet when the thing it measures disappears is
	// indistinguishable from one reporting a ship at rest.
	const USFPilotStationComponent* Pilot = Ship->GetPilotStation();

	if (!Pilot)
	{
		DrawLine(TEXT("intent  -- ship has no pilot station, nothing is flying it"), Y, FLinearColor::Red);
	}
	else
	{
		// Who is flying comes first, and an unoccupied post says so in yellow. A stick
		// position printed without a name beside it is the same defect as a drift of
		// zero printed without a measured time: it reads calm whether or not anything
		// is happening.
		const StarForge::FFlightIntent Intent = Pilot->GetLastIntent();
		const bool bOccupied = Pilot->IsOccupied();
		const bool bFlying = Pilot->IsOperatorFlying();

		// Four states, four colours, and the first of them outranks the rest: a station
		// that cannot see sf.ShipProfile does not know whether the rig is flying itself,
		// so whatever else this line would have said about it is said by an instrument
		// that is admittedly broken. Red, because it is the only one of the four that
		// is a defect rather than a state of the ship.
		//
		// The other three: an occupied post that is declining leaves a zeroed intent
		// behind, which is the same reading a centred stick leaves on a ship at rest;
		// printed green with no qualifier the two are indistinguishable, and the one
		// that means "the loop is not running" is the one worth seeing. Orange, so it is
		// neither the green of a post that is flying nor the yellow of one nobody is at.
		const FLinearColor FlownByColour = Pilot->IsScriptedProfileStateUnknown()
			? FLinearColor::Red
			: (!bOccupied
				? FLinearColor::Yellow
				: (bFlying ? FLinearColor::Green : FLinearColor(1.0f, 0.45f, 0.0f)));

		DrawLine(FString::Printf(TEXT("flown by %s%s"),
			*Pilot->GetOperatorDisplayName(),
			(bOccupied && !bFlying) ? TEXT(" (standing down)") : TEXT("")),
			Y, FlownByColour);

		DrawLine(FString::Printf(TEXT("intent  lin (%.2f %.2f %.2f)  ang (roll pitch yaw) (%.2f %.2f %.2f)  assist %s"),
			Intent.LinearAxis.X, Intent.LinearAxis.Y, Intent.LinearAxis.Z,
			Intent.AngularAxis.X, Intent.AngularAxis.Y, Intent.AngularAxis.Z,
			Intent.bFlightAssistEnabled ? TEXT("on") : TEXT("off")), Y);
	}

	const StarForge::FRigidBodyState State = Array->ReadState();
	DrawLine(FString::Printf(TEXT("speed   %.1f m/s   angular (roll pitch yaw) (%.1f %.1f %.1f) deg/s"),
		State.LinearVelocity.Size(),
		FMath::RadiansToDegrees(State.AngularVelocity.X),
		FMath::RadiansToDegrees(State.AngularVelocity.Y),
		FMath::RadiansToDegrees(State.AngularVelocity.Z)), Y);

	const StarForge::FWrench Demand = Array->GetLastDemand();
	const StarForge::FWrench Achieved = Array->GetLastAchieved();

	DrawLine(FString::Printf(TEXT("force   want (%.0f %.0f %.0f)  got (%.0f %.0f %.0f) N"),
		Demand.Force.X, Demand.Force.Y, Demand.Force.Z,
		Achieved.Force.X, Achieved.Force.Y, Achieved.Force.Z), Y);

	DrawLine(FString::Printf(TEXT("torque  want (%.0f %.0f %.0f)  got (%.0f %.0f %.0f) Nm"),
		Demand.Torque.X, Demand.Torque.Y, Demand.Torque.Z,
		Achieved.Torque.X, Achieved.Torque.Y, Achieved.Torque.Z), Y);

	// Saturation is the usual reason a ship will not do what it is told, and it is
	// invisible without this: the demand looks fine and the ship simply lags.
	//
	// Counted over the commanded throttles, not the spooled ones. Saturation is where
	// the solver's clamp lands, and the spooled value is a first-order lag toward it
	// that only crosses this threshold after some seven response times -- so reading
	// the spooled array would leave this line calm through the whole onset of a launch,
	// which is exactly the moment it exists to catch. An instrument that reads quiet
	// while the thing it measures is pinned is the drift-of-zero defect again.
	int32 Commanded = 0;
	for (double Throttle : Array->GetTargetThrottles())
	{
		if (Throttle > 0.999)
		{
			++Commanded;
		}
	}

	// The spooled count alongside it is not redundant: the gap between the two is the
	// spool-up itself, and seeing it move is how the response time from spec 5 is
	// confirmed to be wired in at all.
	int32 Spooled = 0;
	for (double Throttle : Array->GetLastThrottles())
	{
		if (Throttle > 0.999)
		{
			++Spooled;
		}
	}

	const int32 Count = Array->GetTargetThrottles().Num();
	DrawLine(FString::Printf(TEXT("throttles  commanded %d of %d wide open   spooled %d%s"),
		Commanded, Count, Spooled, Commanded > 0 ? TEXT("  -- SATURATED") : TEXT("")),
		Y, Commanded > 0 ? FLinearColor::Yellow : FLinearColor::White);
}

void ASFFrameDriftHUD::DrawGunnerBlock(double& Y)
{
	const ASFTestShip* Ship = Cast<ASFTestShip>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFTestShip::StaticClass()));
	const USFGunnerStationComponent* Gunner = Ship ? Ship->GetGunnerStation() : nullptr;
	const USFTurretComponent* Turret = Gunner ? Gunner->GetTurret() : nullptr;

	// Red, for the same reason the flight block says "array NOT built" in red: a block
	// that goes quiet when the thing it reports disappears is indistinguishable from
	// one reporting a turret at rest.
	if (!Gunner || !Turret)
	{
		DrawLine(TEXT("gunner  -- ship has no gunner station or no turret, nothing is aiming"), Y, FLinearColor::Red);
		return;
	}

	// Who holds the post comes first, with the same three colours the pilot line uses:
	// yellow for nobody, orange for somebody who is declining, green for somebody gunning.
	const bool bOccupied = Gunner->IsOccupied();
	const bool bGunning = Gunner->IsOperatorGunning();
	const FLinearColor GunnedByColour = !bOccupied
		? FLinearColor::Yellow
		: (bGunning ? FLinearColor::Green : FLinearColor(1.0f, 0.45f, 0.0f));

	DrawLine(FString::Printf(TEXT("gunned by %s%s"),
		*Gunner->GetOperatorDisplayName(),
		(bOccupied && !bGunning) ? TEXT(" (standing down)") : TEXT("")),
		Y, GunnedByColour);

	const StarForge::Turret::FTurretAim Aim = Turret->GetAim();
	const StarForge::Turret::FTurretAim Bearing = Turret->GetLastBearing();
	const FString BearingText = Turret->HasBearing()
		? FString::Printf(TEXT("yaw %.1f pitch %.1f"),
			FMath::RadiansToDegrees(Bearing.YawRad), FMath::RadiansToDegrees(Bearing.PitchRad))
		: FString(TEXT("none"));

	// Green only for on target. A turret at its stop is orange, not green, because its
	// angle alone would read as on target; no target and no command are yellow, the
	// colour of a post nobody is at.
	FLinearColor StateColour = FLinearColor::White;
	switch (Turret->GetState())
	{
	case ESFTurretState::OnTarget: StateColour = FLinearColor::Green; break;
	case ESFTurretState::CannotBear: StateColour = FLinearColor(1.0f, 0.45f, 0.0f); break;
	case ESFTurretState::NoTarget:
	case ESFTurretState::Uncommanded: StateColour = FLinearColor::Yellow; break;
	default: break;
	}

	DrawLine(FString::Printf(TEXT("turret  aim yaw %.1f pitch %.1f deg   bearing %s   %s   firing %s"),
		FMath::RadiansToDegrees(Aim.YawRad), FMath::RadiansToDegrees(Aim.PitchRad),
		*BearingText,
		USFTurretComponent::StateName(Turret->GetState()),
		Turret->IsFiring() ? TEXT("on") : TEXT("off")),
		Y, StateColour);
}
