#include "Misc/AutomationTest.h"
#include "SmallMatrix.h"

namespace
{
	/** Largest absolute difference between two matrices, for tolerance checks. */
	double MaxAbsDifference(const StarForge::FMatrix6& A, const StarForge::FMatrix6& B)
	{
		double Worst = 0.0;
		for (int32 Row = 0; Row < 6; ++Row)
		{
			for (int32 Col = 0; Col < 6; ++Col)
			{
				Worst = FMath::Max(Worst, FMath::Abs(A.At(Row, Col) - B.At(Row, Col)));
			}
		}
		return Worst;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatrix6IdentityInvertsToItselfTest,
	"StarForge.Core.Matrix6.IdentityInvertsToItself",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMatrix6IdentityInvertsToItselfTest::RunTest(const FString& Parameters)
{
	StarForge::FMatrix6 Inverse;
	const bool bInverted = StarForge::FMatrix6::Identity().TryInvert(Inverse);

	TestTrue(TEXT("identity is invertible"), bInverted);
	TestTrue(TEXT("identity inverts to identity"),
		MaxAbsDifference(Inverse, StarForge::FMatrix6::Identity()) < 1e-12);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatrix6RoundTripTest,
	"StarForge.Core.Matrix6.RoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMatrix6RoundTripTest::RunTest(const FString& Parameters)
{
	// A diagonally dominant matrix with no symmetry to hide a transposition bug:
	// every off-diagonal entry differs from its mirror.
	StarForge::FMatrix6 A = StarForge::FMatrix6::Zero();
	for (int32 Row = 0; Row < 6; ++Row)
	{
		for (int32 Col = 0; Col < 6; ++Col)
		{
			A.At(Row, Col) = (Row == Col) ? 10.0 + Row : 0.1 * (Row + 1) - 0.03 * (Col + 1);
		}
	}

	StarForge::FMatrix6 Inverse;
	TestTrue(TEXT("a diagonally dominant matrix is invertible"), A.TryInvert(Inverse));

	TestTrue(TEXT("A times its inverse is the identity"),
		MaxAbsDifference(A * Inverse, StarForge::FMatrix6::Identity()) < 1e-9);
	TestTrue(TEXT("the inverse times A is the identity too"),
		MaxAbsDifference(Inverse * A, StarForge::FMatrix6::Identity()) < 1e-9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatrix6SingularIsRejectedTest,
	"StarForge.Core.Matrix6.SingularIsRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMatrix6SingularIsRejectedTest::RunTest(const FString& Parameters)
{
	// Two identical rows: rank 5, no inverse. Reporting failure matters because a
	// ship whose thrusters cannot span all six axes produces exactly this, and the
	// caller has to know rather than receive silent nonsense.
	StarForge::FMatrix6 Singular = StarForge::FMatrix6::Identity();
	for (int32 Col = 0; Col < 6; ++Col)
	{
		Singular.At(3, Col) = Singular.At(2, Col);
	}

	StarForge::FMatrix6 Inverse;
	TestFalse(TEXT("a rank-deficient matrix reports failure"), Singular.TryInvert(Inverse));

	return true;
}
