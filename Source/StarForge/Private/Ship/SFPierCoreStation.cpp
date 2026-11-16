#include "Ship/SFPierCoreStation.h"

ASFPierCoreStation::ASFPierCoreStation()
{
	// spec.py B_K3core: berth blocks at (-30, 0, 0) and (30, 0, 0) m, yaw 0. In centimetres, whole, so
	// Port1 is Port0 plus exactly (6000, 0, 0) cm: control N6's tie can be exact to the bit only if it
	// is (spec 7.5).
	BuildBlock(FVector(-3000.0, 0.0, 0.0));
	BuildBlock(FVector(3000.0, 0.0, 0.0));
}
