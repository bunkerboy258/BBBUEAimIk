#pragma once
#include "CoreMinimal.h"

/** 单次求值的只读世界空间调试快照 */
struct FAimIKDebugFrame
{
    FVector InputTarget;
    FVector FollowTarget;
    FVector SolveTarget;
    FTransform Before;
    FTransform Solved;
    FTransform Applied;
    FVector Axis;
    float Alpha = 0;
    float ResidualDegrees = 0;
    uint64 Frame = 0;
    uint64 Evaluation = 0;
};
