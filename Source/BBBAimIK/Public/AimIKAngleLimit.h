#pragma once

#include "CoreMinimal.h"

/** 把目标方向限制在当前输入姿态前向周围的圆锥内 */
struct FAimIKAngleLimit final
{
    static FVector ClampDirection(
        const FVector& InputDirection,
        const FVector& DesiredDirection,
        float MaxAngleDegrees,
        const FVector& PreviousDirection)
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
        const FVector PreviousAxis = FVector::CrossProduct(
            Forward, PreviousDirection.GetSafeNormal()).GetSafeNormal();
        if (DirectionDot < -0.98f && !PreviousAxis.IsNearlyZero())
        {
            // 目标靠近正后方时保留上一帧的转向侧
            RotationAxis = PreviousAxis;
        }
        if (RotationAxis.IsNearlyZero())
        {
            // 没有可继承的转向侧时保持当前姿态 不引入任意竖直翻转
            return Forward;
        }

        return FQuat(RotationAxis, MaxAngleRadians).RotateVector(Forward).GetSafeNormal();
    }
};
