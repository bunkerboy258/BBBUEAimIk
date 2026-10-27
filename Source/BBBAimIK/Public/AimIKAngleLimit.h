#pragma once

#include "CoreMinimal.h"

/** 把目标方向限制在当前输入姿态前向周围的圆锥内 */
struct FAimIKAngleLimit final
{
    static FVector ClampDirection(
        const FVector& InputDirection,
        const FVector& DesiredDirection,
        float MaxAngleDegrees)
    {
        const FVector Forward = InputDirection.GetSafeNormal();
        const FVector Desired = DesiredDirection.GetSafeNormal();
        if (Forward.IsNearlyZero() || Desired.IsNearlyZero())
        {
            return Desired;
        }

        const float MaxAngleRadians = FMath::DegreesToRadians(
            FMath::Clamp(MaxAngleDegrees, 1.0f, 180.0f));
        const float DirectionDot = FMath::Clamp(
            FVector::DotProduct(Forward, Desired), -1.0f, 1.0f);
        if (DirectionDot >= FMath::Cos(MaxAngleRadians))
        {
            return Desired;
        }

        FVector RotationAxis = FVector::CrossProduct(Forward, Desired).GetSafeNormal();
        if (RotationAxis.IsNearlyZero())
        {
            // 正反方向重合时选取稳定的垂直轴
            const FVector ReferenceAxis = FMath::Abs(Forward.Z) < 0.99f
                ? FVector::UpVector
                : FVector::RightVector;
            RotationAxis = FVector::CrossProduct(Forward, ReferenceAxis).GetSafeNormal();
        }

        return FQuat(RotationAxis, MaxAngleRadians).RotateVector(Forward).GetSafeNormal();
    }
};
