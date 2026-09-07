#include "Ship/SFShipDataAsset.h"

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
