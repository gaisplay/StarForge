#include "Ship/SFShipDataAsset.h"

namespace
{
	/**
	 * Corner offsets of the standard layout, in METRES -- this asset is authored in
	 * SI and the field names say so, so nothing is converted here.
	 *
	 * All four corners sit on the Z = 0 plane. That is enough to span all six axes
	 * once each corner carries the full signed set below, and it keeps the layout
	 * readable as a flat square rather than a cube nobody can picture.
	 */
	constexpr double CornerArmMetres = 2.0;

	/** Thrust per thruster at full throttle, in newtons. */
	constexpr double StandardThrustNewtons = 200000.0;
}

void USFShipDataAsset::PopulateSymmetricLayout()
{
	// Replaces rather than appends. Pressing the button twice must leave the asset in
	// the same state as pressing it once, or a second press silently doubles every
	// thruster and with it the ship's whole thrust budget.
	Thrusters.Reset();

	const TArray<FVector> Corners = {
		FVector(CornerArmMetres, CornerArmMetres, 0.0),
		FVector(CornerArmMetres, -CornerArmMetres, 0.0),
		FVector(-CornerArmMetres, CornerArmMetres, 0.0),
		FVector(-CornerArmMetres, -CornerArmMetres, 0.0)
	};

	// Every axis in both senses. The negatives are what make torque possible at all;
	// see the comment on the declaration.
	const TArray<FVector> Directions = {
		FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0),
		FVector(0.0, 1.0, 0.0), FVector(0.0, -1.0, 0.0),
		FVector(0.0, 0.0, 1.0), FVector(0.0, 0.0, -1.0)
	};

	Thrusters.Reserve(Corners.Num() * Directions.Num());

	for (const FVector& Corner : Corners)
	{
		for (const FVector& Direction : Directions)
		{
			FSFThrusterSpec Spec;
			Spec.LocationMetres = Corner;
			Spec.Direction = Direction;
			Spec.MaxThrustNewtons = StandardThrustNewtons;
			// ResponseTimeSeconds is left at its default: spool-up is a property of the
			// thruster hardware, not of the layout this button describes.
			Thrusters.Add(Spec);
		}
	}
}

TArray<StarForge::FThrusterDef> USFShipDataAsset::BuildThrusters() const
{
	TArray<StarForge::FThrusterDef> Result;
	Result.Reserve(Thrusters.Num());

	for (const FSFThrusterSpec& Spec : Thrusters)
	{
		StarForge::FThrusterDef Def;
		Def.Location = Spec.LocationMetres;
		Def.Direction = Spec.Direction;
		Def.MaxThrust = Spec.MaxThrustNewtons;
		Def.ResponseTime = Spec.ResponseTimeSeconds;
		Result.Add(Def);
	}

	return Result;
}

StarForge::FFlightLimits USFShipDataAsset::BuildLimits() const
{
	StarForge::FFlightLimits Limits;
	Limits.MaxSpeed = MaxSpeed;

	// Degrees are what a person authors and what spec 7 is written in; radians are
	// what everything downstream uses. Converted once, here.
	Limits.MaxAngularSpeed = FVector(
		FMath::DegreesToRadians(MaxAngularSpeedDegPerSec.X),
		FMath::DegreesToRadians(MaxAngularSpeedDegPerSec.Y),
		FMath::DegreesToRadians(MaxAngularSpeedDegPerSec.Z));

	Limits.LinearGain = LinearGain;
	Limits.AngularGain = AngularGain;
	return Limits;
}
