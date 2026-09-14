#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "ShipDamage.h"
#include "ThrustAllocator.h"
#include "ThrusterDef.h"

#include <limits>

namespace
{
	using StarForge::FThrusterDef;
	using StarForge::FThrustAllocator;
	using StarForge::FWrench;

	/** Newtons per thruster: the asset's StandardThrustNewtons. Every figure below is in it. */
	constexpr double LossRigThrustNewtons = 200000.0;

	/** The asset's MassKg, for turning a force ceiling into an acceleration ceiling. */
	constexpr double LossRigMassKg = 50000.0;

	/** The asset's kill radius. See USFShipDataAsset::ThrusterKillRadiusMetres for its derivation. */
	constexpr double LossRigKillRadiusMetres = 2.5;

	/**
	 * The rig's twenty-four thrusters, restated in the order USFShipDataAsset::PopulateSymmetricLayout
	 * writes them: corners (2,2), (2,-2), (-2,2), (-2,-2), each carrying +X, -X, +Y, -Y, +Z, -Z. So
	 * corner c holds indices 6c to 6c+5, and index 0 is the +X thruster at (2, 2, 0).
	 *
	 * Restated rather than read, because this module has no UObjects and cannot open the asset. The
	 * tests pin indices on this order; if the asset's order ever changes, this fixture has to change
	 * with it, and the rig's destruction line -- "Thruster 0 ... at (2.0 2.0 0.0) m", forward thrust
	 * 800 -> 600 kN -- is the check that the two still agree.
	 */
	TArray<FThrusterDef> MakeLossRigLayout()
	{
		const FVector Corners[4] = {
			FVector( 2.0,  2.0, 0.0),
			FVector( 2.0, -2.0, 0.0),
			FVector(-2.0,  2.0, 0.0),
			FVector(-2.0, -2.0, 0.0)
		};

		const FVector Directions[6] = {
			FVector( 1.0,  0.0,  0.0),
			FVector(-1.0,  0.0,  0.0),
			FVector( 0.0,  1.0,  0.0),
			FVector( 0.0, -1.0,  0.0),
			FVector( 0.0,  0.0,  1.0),
			FVector( 0.0,  0.0, -1.0)
		};

		TArray<FThrusterDef> Thrusters;
		for (const FVector& Corner : Corners)
		{
			for (const FVector& Direction : Directions)
			{
				FThrusterDef Thruster;
				Thruster.Location = Corner;
				Thruster.Direction = Direction;
				Thruster.MaxThrust = LossRigThrustNewtons;
				Thrusters.Add(Thruster);
			}
		}
		return Thrusters;
	}

	/** Every index below Count that is not in Destroyed, ascending. */
	TArray<int32> IndicesExcept(int32 Count, const TArray<int32>& Destroyed)
	{
		TArray<int32> Result;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Destroyed.Contains(Index))
			{
				Result.Add(Index);
			}
		}
		return Result;
	}

	/** The thrusters at Indices, in that order: what the game builds its allocator from. */
	TArray<FThrusterDef> PickThrusters(const TArray<FThrusterDef>& All, const TArray<int32>& Indices)
	{
		TArray<FThrusterDef> Result;
		for (int32 Index : Indices)
		{
			Result.Add(All[Index]);
		}
		return Result;
	}

	/** All six thrusters of each listed corner. */
	TArray<FThrusterDef> ThrustersOnCorners(const TArray<FThrusterDef>& All, const TArray<int32>& Corners)
	{
		TArray<int32> Indices;
		for (int32 Corner : Corners)
		{
			for (int32 Slot = 0; Slot < 6; ++Slot)
			{
				Indices.Add(Corner * 6 + Slot);
			}
		}
		return PickThrusters(All, Indices);
	}

	/** Solves a pure forward request and reports what came of it. */
	FWrench SolveForward(const FThrustAllocator& Allocator, double ForceX, double& OutThrottleSum)
	{
		FWrench Desired;
		Desired.Force = FVector(ForceX, 0.0, 0.0);

		TArray<double> Throttles;
		Allocator.Solve(Desired, Throttles);

		OutThrottleSum = 0.0;
		for (double Throttle : Throttles)
		{
			OutThrottleSum += Throttle;
		}
		return Allocator.ComputeAchieved(Throttles);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterLossNearestWithinRadiusTest,
	"StarForge.Core.ThrusterLoss.NearestWithinRadius",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterLossNearestWithinRadiusTest::RunTest(const FString& Parameters)
{
	using StarForge::Damage::FindThrusterHitBy;

	const TArray<FThrusterDef> Layout = MakeLossRigLayout();
	const TArray<int32> AllAlive = IndicesExcept(Layout.Num(), {});

	// The fixture before the behaviour: the index this test expects is only the +X thruster if the
	// order is the asset's.
	TestTrue(TEXT("index 0 of the fixture is the +X thruster at (2, 2, 0), as the asset writes it"),
		Layout[0].Location.Equals(FVector(2.0, 2.0, 0.0), 0.0) && Layout[0].Direction.Equals(FVector(1.0, 0.0, 0.0), 0.0));

	// Every search starts from a sentinel no search can return, so a refusal or a miss that wrote
	// the index anyway cannot pass.
	const int32 Untouched = 99;

	// On the +X face in line with the corner (2, 2): 4.1 - 2.0 = 2.1 m, inside 2.5. Six thrusters
	// share that corner's position exactly, so this is a six-way tie, and ties go to the lowest
	// surviving index -- which in the asset's order is the corner's +X thruster.
	int32 Index = Untouched;
	TestTrue(TEXT("a hit on the +X face in line with a corner finds a thruster"),
		FindThrusterHitBy(Layout, AllAlive, FVector(4.1, 2.0, 0.0), LossRigKillRadiusMetres, Index));
	TestEqual(TEXT("and it is index 0, the lowest of the six tied at that corner"), Index, 0);

	// The rule is the lowest index, not the first one listed: the same search over the survivors
	// listed backwards gives the same answer.
	TArray<int32> Backwards = AllAlive;
	Algo::Reverse(Backwards);
	Index = Untouched;
	TestTrue(TEXT("the same hit with the survivors listed backwards finds a thruster"),
		FindThrusterHitBy(Layout, Backwards, FVector(4.1, 2.0, 0.0), LossRigKillRadiusMetres, Index));
	TestEqual(TEXT("and it is still index 0"), Index, 0);

	// Once 0 is gone the same hit takes the next of the six.
	Index = Untouched;
	TestTrue(TEXT("with index 0 destroyed the same hit still finds a thruster"),
		FindThrusterHitBy(Layout, IndicesExcept(Layout.Num(), { 0 }), FVector(4.1, 2.0, 0.0), LossRigKillRadiusMetres, Index));
	TestEqual(TEXT("and it is index 1, the lowest survivor at that corner"), Index, 1);

	// Nearest first, and the index decides only between equals. (2, -0.3, 0) is 2.3 m from the
	// corner (2, 2) and 1.7 m from (2, -2): both inside the radius, and the farther one holds the
	// lower indices.
	Index = Untouched;
	TestTrue(TEXT("a hit inside the radius of two corners finds a thruster"),
		FindThrusterHitBy(Layout, AllAlive, FVector(2.0, -0.3, 0.0), LossRigKillRadiusMetres, Index));
	TestEqual(TEXT("and it is the nearer corner's lowest, index 6, not index 0"), Index, 6);

	// The centre of the +X face is sqrt(2.1^2 + 2^2) = 2.90 m from the nearest corner: nothing.
	Index = Untouched;
	TestFalse(TEXT("a hit on the centre of the +X face finds nothing"),
		FindThrusterHitBy(Layout, AllAlive, FVector(4.1, 0.0, 0.0), LossRigKillRadiusMetres, Index));
	TestEqual(TEXT("and leaves the index untouched"), Index, Untouched);

	// The ceiling. Every thruster sits on z = 0, so the nearest any point of the ceiling comes to one
	// is 4.1 m, directly above a corner; its centre is sqrt(2^2 + 2^2 + 4.1^2) = 4.98 m from all
	// four. This is the negative control the geometry gives for free, not a tuning: nothing fired
	// at the roof can reach a thruster while the radius stays below 4.1.
	Index = Untouched;
	TestFalse(TEXT("a hit on the centre of the ceiling finds nothing"),
		FindThrusterHitBy(Layout, AllAlive, FVector(0.0, 0.0, 4.1), LossRigKillRadiusMetres, Index));
	TestFalse(TEXT("nor does a hit on the ceiling directly above a corner, 4.1 m away"),
		FindThrusterHitBy(Layout, AllAlive, FVector(2.0, 2.0, 4.1), LossRigKillRadiusMetres, Index));
	TestEqual(TEXT("and neither touches the index"), Index, Untouched);

	// Nothing left, nothing found.
	TestFalse(TEXT("with no survivors a hit on a corner finds nothing"),
		FindThrusterHitBy(Layout, TArray<int32>(), FVector(4.1, 2.0, 0.0), LossRigKillRadiusMetres, Index));
	TestEqual(TEXT("and leaves the index untouched"), Index, Untouched);

	// Refusals. Each fixture is wrong in one respect only, and where a guard carries two conditions,
	// the fixture is one that only that condition refuses: a zero radius on a point exactly at a
	// thruster is within reach of it (0 <= 0), and an infinite radius reaches everything.
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	TestTrue(TEXT("the NaN really is not finite"), !FMath::IsFinite(NaN));
	TestTrue(TEXT("and the infinity really is not finite"), !FMath::IsFinite(Inf));

	const auto ExpectRefused = [this, &Layout, Untouched](const TCHAR* What, const TArray<int32>& Alive, const FVector& Point, double Radius)
	{
		int32 Out = Untouched;
		TestFalse(FString(What) + TEXT(" is refused"), FindThrusterHitBy(Layout, Alive, Point, Radius, Out));
		TestEqual(FString(What) + TEXT(" leaves the index untouched"), Out, Untouched);
	};

	ExpectRefused(TEXT("a zero radius on a point exactly at a thruster"), AllAlive, FVector(2.0, 2.0, 0.0), 0.0);
	ExpectRefused(TEXT("an infinite radius"), AllAlive, FVector(4.1, 0.0, 0.0), Inf);
	ExpectRefused(TEXT("a negative radius"), AllAlive, FVector(2.0, 2.0, 0.0), -1.0);
	ExpectRefused(TEXT("a NaN radius"), AllAlive, FVector(2.0, 2.0, 0.0), NaN);

	// A non-finite point is refused by name. Every distance from it is NaN or infinite and fails
	// the reach comparison as well, so these two fixtures pin the refusal rather than the guard.
	ExpectRefused(TEXT("a NaN in the point"), AllAlive, FVector(NaN, 2.0, 0.0), LossRigKillRadiusMetres);
	ExpectRefused(TEXT("an infinity in the point"), AllAlive, FVector(2.0, Inf, 0.0), LossRigKillRadiusMetres);

	// A survivor list that names a thruster the array does not have is refused whole, not skipped:
	// the index 0 beside it is in reach of the point, so a search that skipped the bad one would
	// answer 0. Past the end and before the start.
	ExpectRefused(TEXT("a survivor index past the end"), { 0, Layout.Num() }, FVector(4.1, 2.0, 0.0), LossRigKillRadiusMetres);
	ExpectRefused(TEXT("a negative survivor index"), { -1, 0 }, FVector(4.1, 2.0, 0.0), LossRigKillRadiusMetres);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterLossLosingOneForwardThrusterCostsExactlyAQuarterTest,
	"StarForge.Core.ThrusterLoss.LosingOneForwardThrusterCostsExactlyAQuarter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterLossLosingOneForwardThrusterCostsExactlyAQuarterTest::RunTest(const FString& Parameters)
{
	// Equality with a tolerance of zero. Every column is a direction of exact unit length times
	// 200000, so each figure is a sum of 200000s and zeros, and there is no free parameter anywhere.
	const TArray<FThrusterDef> Layout = MakeLossRigLayout();

	FThrustAllocator Whole;
	TestTrue(TEXT("the whole array builds"), Whole.Build(Layout, FVector::ZeroVector));

	// Along +X one thruster per corner pushes: 4 x 200 kN. The capability sum counts the -X four
	// as well: 8 x 200 kN.
	TestEqual(TEXT("the whole array achieves 800 kN along X"), Whole.GetAxisAchievable(0), 800000.0, 0.0);
	TestEqual(TEXT("and its X capability sum is 1600 kN"), Whole.GetAxisCapability(0), 1600000.0, 0.0);
	TestEqual(TEXT("so its ceiling on longitudinal acceleration is 16.0 m/s^2"),
		Whole.GetAxisAchievable(0) / LossRigMassKg, 16.0, 0.0);

	TestTrue(TEXT("the thruster removed below pushes along +X"), Layout[0].Direction.Equals(FVector(1.0, 0.0, 0.0), 0.0));

	FThrustAllocator Damaged;
	TestTrue(TEXT("the array without index 0 builds"),
		Damaged.Build(PickThrusters(Layout, IndicesExcept(Layout.Num(), { 0 })), FVector::ZeroVector));

	// A quarter of the achievable, an eighth of the capability sum: the one lost thruster was one of
	// four that could push forward and one of eight the sum counts. The two numbers answer different
	// questions, and this is the case that shows it most plainly.
	TestEqual(TEXT("without it the array achieves exactly 600 kN along X"), Damaged.GetAxisAchievable(0), 600000.0, 0.0);
	TestEqual(TEXT("and its X capability sum is exactly 1400 kN"), Damaged.GetAxisCapability(0), 1400000.0, 0.0);
	TestEqual(TEXT("so its ceiling on longitudinal acceleration is exactly 12.0 m/s^2"),
		Damaged.GetAxisAchievable(0) / LossRigMassKg, 12.0, 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterLossTwoCornersCannotSpanSixAxesTest,
	"StarForge.Core.ThrusterLoss.TwoCornersCannotSpanSixAxes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterLossTwoCornersCannotSpanSixAxesTest::RunTest(const FString& Parameters)
{
	// Two full corners have every axis in both senses and still cannot span six. A force at one
	// corner along the line joining the two and its opposite at the other make zero force and zero
	// torque together, so the forces the two corners command reach only five dimensions. The one
	// they cannot reach is not a coordinate axis: for (2,2) and (2,-2) it is a vertical force and a
	// pitch torque together, (Fz, Ty) in proportion (2, 1); for the diagonal pair it is a torque along
	// (1, 1, 0). Every axis on its own still gets something back, so a check that asks only that
	// answers "ready" on rank five, and on a single corner's rank three as well.
	const TArray<FThrusterDef> Layout = MakeLossRigLayout();

	FThrustAllocator Three;
	TestTrue(TEXT("three full corners span six axes"), Three.Build(ThrustersOnCorners(Layout, { 0, 1, 2 }), FVector::ZeroVector));
	TestTrue(TEXT("and report themselves built"), Three.IsBuilt());

	FThrustAllocator Adjacent;
	TestFalse(TEXT("two adjacent corners, (2,2) and (2,-2), cannot"),
		Adjacent.Build(ThrustersOnCorners(Layout, { 0, 1 }), FVector::ZeroVector));
	TestFalse(TEXT("and report themselves unbuilt"), Adjacent.IsBuilt());

	FThrustAllocator Diagonal;
	TestFalse(TEXT("two diagonal corners, (2,2) and (-2,-2), cannot either"),
		Diagonal.Build(ThrustersOnCorners(Layout, { 0, 3 }), FVector::ZeroVector));
	TestFalse(TEXT("and report themselves unbuilt"), Diagonal.IsBuilt());

	FThrustAllocator One;
	TestFalse(TEXT("one full corner, rank three, cannot"),
		One.Build(ThrustersOnCorners(Layout, { 0 }), FVector::ZeroVector));
	TestFalse(TEXT("and reports itself unbuilt"), One.IsBuilt());

	// The rig's step: sf.KillThruster 0 to 11 takes the two corners that hold those indices one
	// thruster at a time. Eleven kills leave the -Z thruster at (2,-2) beside two full corners, and
	// that one thruster is enough to restore the missing direction; the twelfth takes it, and the
	// first refusal is exactly there.
	for (int32 Killed = 0; Killed < 12; ++Killed)
	{
		TArray<int32> Destroyed;
		for (int32 Index = 0; Index <= Killed; ++Index)
		{
			Destroyed.Add(Index);
		}

		FThrustAllocator Survivors;
		const bool bBuilt = Survivors.Build(PickThrusters(Layout, IndicesExcept(Layout.Num(), Destroyed)), FVector::ZeroVector);
		const bool bExpected = Killed < 11;
		TestTrue(*FString::Printf(TEXT("after destroying 0..%d the array %s"), Killed,
				bExpected ? TEXT("still builds") : TEXT("is refused")),
			bBuilt == bExpected);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterLossSingleLossYawsOnlyAboveTheCeilingTest,
	"StarForge.Core.ThrusterLoss.SingleLossYawsOnlyAboveTheCeiling",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterLossSingleLossYawsOnlyAboveTheCeilingTest::RunTest(const FString& Parameters)
{
	// Losing index 0 leaves three +X thrusters whose line of action is off the centre of mass, and
	// whether the ship then yaws depends on how hard it is asked to push forward.
	//
	// Below the reduced 600 kN ceiling it flies straight and spends more throttle: a +Y thruster at
	// x = -2 and a -Y thruster at x = +2 make zero force and -800 kNm between them, and the solver
	// spends that couple against the survivors' moment. (Within the last seventy-odd kN below the
	// ceiling three clamp passes leave a residue of a few kNm, 9.4 at the ceiling itself; that one
	// shrinks with more passes, and the stick's edge below pins it.) Far above the ceiling -- the
	// 20 MN assist asks for from rest -- it is straight again.
	//
	// In the band between, from about 610 kN to somewhere between 900 kN (measured at 600 kNm) and
	// 950 kN (measured at zero), the force pins at the ceiling and the yaw torque grows as
	// 2 x (demand - 600 kN). That band is the allocator's metric, not the geometry: a least-squares
	// answer in newtons and newton-metres alike trades the force the array cannot deliver for a
	// torque it can, and in a replica of Solve more clamp passes flip the sign of that torque without
	// shrinking it. The intact array is asserted straight in the same band, so this test tells the
	// damage apart from the solver in general.
	//
	// The assist drives the demand through the band on every approach to its target speed, which is
	// why a damaged ship yaws briefly as it nears 200 m/s under assist and cruises straight after.
	const TArray<FThrusterDef> Layout = MakeLossRigLayout();

	FThrustAllocator Whole;
	TestTrue(TEXT("the whole array builds"), Whole.Build(Layout, FVector::ZeroVector));

	FThrustAllocator Damaged;
	TestTrue(TEXT("the array without index 0 builds"),
		Damaged.Build(PickThrusters(Layout, IndicesExcept(Layout.Num(), { 0 })), FVector::ZeroVector));

	// Half the reduced ceiling: straight, and dearer.
	double WholeSum = 0.0;
	double DamagedSum = 0.0;
	SolveForward(Whole, 300000.0, WholeSum);
	const FWrench Half = SolveForward(Damaged, 300000.0, DamagedSum);

	TestTrue(TEXT("300 kN asked of the damaged array is 300 kN delivered, within 1 N on every axis"),
		Half.Force.Equals(FVector(300000.0, 0.0, 0.0), 1.0));
	TestTrue(TEXT("with no torque, under 1 Nm"), Half.Torque.Size() < 1.0);
	TestTrue(TEXT("and more throttle in total than the whole array spends on the same request"), DamagedSum > WholeSum);
	AddInfo(FString::Printf(TEXT("300 kN: throttle sum whole %.4f, damaged %.4f; damaged force (%.3f %.3f %.3f) N, torque (%.3f %.3f %.3f) Nm"),
		WholeSum, DamagedSum, Half.Force.X, Half.Force.Y, Half.Force.Z, Half.Torque.X, Half.Torque.Y, Half.Torque.Z));

	// Inside the band: 800 kN asked, 200 kN more than the damaged array can deliver, and that 200 kN
	// comes back as 400 kNm of yaw. The intact array delivers the same request whole and straight.
	double BandSum = 0.0;
	const FWrench Band = SolveForward(Damaged, 800000.0, BandSum);
	TestTrue(TEXT("800 kN asked of the damaged array delivers the 600 kN ceiling, within 1 N on every axis"),
		Band.Force.Equals(FVector(600000.0, 0.0, 0.0), 1.0));
	TestEqual(TEXT("and yaws at +400 kNm, twice the 200 kN it cannot deliver"), Band.Torque.Z, 400000.0, 1.0);
	AddInfo(FString::Printf(TEXT("800 kN: damaged force (%.3f %.3f %.3f) N, torque (%.3f %.3f %.3f) Nm, throttle sum %.4f"),
		Band.Force.X, Band.Force.Y, Band.Force.Z, Band.Torque.X, Band.Torque.Y, Band.Torque.Z, BandSum));

	double WholeBandSum = 0.0;
	const FWrench WholeBand = SolveForward(Whole, 800000.0, WholeBandSum);
	TestTrue(TEXT("the intact array on the same 800 kN makes no torque, under 1 Nm"), WholeBand.Torque.Size() < 1.0);

	// Assist asks for twenty meganewtons when the stick goes forward from rest: far above the band,
	// exactly the ceiling, and no torque.
	double DeepSum = 0.0;
	const FWrench Deep = SolveForward(Damaged, 2.0e7, DeepSum);
	TestTrue(TEXT("20 MN asked is exactly the 600 kN ceiling delivered, within 1 N"),
		Deep.Force.Equals(FVector(600000.0, 0.0, 0.0), 1.0));
	TestTrue(TEXT("with no torque, under 1 Nm"), Deep.Torque.Size() < 1.0);
	AddInfo(FString::Printf(TEXT("20 MN: damaged force (%.3f %.3f %.3f) N, torque (%.3f %.3f %.3f) Nm, throttle sum %.4f"),
		Deep.Force.X, Deep.Force.Y, Deep.Force.Z, Deep.Torque.X, Deep.Torque.Y, Deep.Torque.Z, DeepSum));

	// The stick's edge with assist off asks for exactly the ceiling, and here a second, smaller effect
	// is what yaws: three clamp-and-correct passes do not quite converge, and what they leave
	// undelivered comes back as torque. Unlike the band, this one does shrink with more passes.
	double EdgeSum = 0.0;
	const FWrench Edge = SolveForward(Damaged, 600000.0, EdgeSum);
	TestTrue(TEXT("600 kN asked at the stick's edge is not quite delivered"), Edge.Force.X < 600000.0);
	TestTrue(TEXT("and the shortfall yaws the ship toward +Z"), Edge.Torque.Z > 0.0);
	TestTrue(TEXT("with no roll and no pitch, under 1 Nm: every thruster sits on z = 0"),
		FMath::Abs(Edge.Torque.X) < 1.0 && FMath::Abs(Edge.Torque.Y) < 1.0);

	// And the figures themselves, as this solver gives them. A Python replica of Solve predicted
	// them to the last digit before this ran: 4687.5 N short, which is 1/128 of the request, and
	// 9375 Nm of yaw -- 9375 / 533333 kg m^2 is the +1.0 deg/s^2 the rig is expected to show with
	// assist off. A fourth pass, or a different clamp, moves both, and moves the band's sign; this is
	// where that is noticed before the rig reading is.
	TestEqual(TEXT("the stick's edge delivers 595312.5 N"), Edge.Force.X, 595312.5, 0.01);
	TestEqual(TEXT("and yaws at 9375.0 Nm"), Edge.Torque.Z, 9375.0, 0.01);
	AddInfo(FString::Printf(TEXT("600 kN: damaged force (%.3f %.3f %.3f) N, torque (%.3f %.3f %.3f) Nm, throttle sum %.4f"),
		Edge.Force.X, Edge.Force.Y, Edge.Force.Z, Edge.Torque.X, Edge.Torque.Y, Edge.Torque.Z, EdgeSum));

	return true;
}
