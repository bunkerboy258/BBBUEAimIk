#pragma once

#include "CoreMinimal.h"

/** 分别限制目标相对当前输入姿态的水平与垂直方向 */
struct FAimIKAngleLimit final
{
    static FVector ClampDirection(
        const FVector& InputDirection,
        const FVector& DesiredDirection,
        const FVector& UpDirection,
        const FVector& ReferenceForward,
        float MaxHorizontalAngleDegrees,
        float MaxVerticalAngleDegrees,
        const FVector& PreviousDirection)
    {
        const FVector Forward = InputDirection.GetSafeNormal();
        const FVector Desired = DesiredDirection.GetSafeNormal();
        const FVector Up = UpDirection.GetSafeNormal();
        if (Forward.IsNearlyZero() || Desired.IsNearlyZero() || Up.IsNearlyZero())
        {
            return Desired;
        }

        FVector HorizontalForward = FVector::VectorPlaneProject(Forward, Up).GetSafeNormal();
        if (HorizontalForward.IsNearlyZero())
        {
            HorizontalForward = FVector::VectorPlaneProject(ReferenceForward, Up).GetSafeNormal();
        }
        if (HorizontalForward.IsNearlyZero())
        {
            HorizontalForward = FVector::VectorPlaneProject(FVector::ForwardVector, Up).GetSafeNormal();
        }
        if (HorizontalForward.IsNearlyZero())
        {
            HorizontalForward = FVector::VectorPlaneProject(FVector::RightVector, Up).GetSafeNormal();
        }

        const FVector Right = FVector::CrossProduct(Up, HorizontalForward).GetSafeNormal();
        if (Right.IsNearlyZero())
        {
            return Desired;
        }

        const float InputPitch = FMath::Asin(FMath::Clamp(FVector::DotProduct(Forward, Up), -1.0f, 1.0f));
        const float DesiredPitch = FMath::Asin(FMath::Clamp(FVector::DotProduct(Desired, Up), -1.0f, 1.0f));
        float DesiredYaw = FMath::Atan2(
            FVector::DotProduct(Desired, Right),
            FVector::DotProduct(Desired, HorizontalForward));
        const float MaxHorizontalRadians = FMath::DegreesToRadians(
            FMath::Clamp(MaxHorizontalAngleDegrees, 1.0f, 180.0f));
        const float MaxVerticalRadians = FMath::DegreesToRadians(
            FMath::Clamp(MaxVerticalAngleDegrees, 1.0f, 180.0f));
        if (FMath::Abs(DesiredYaw) <= MaxHorizontalRadians
            && FMath::Abs(DesiredPitch - InputPitch) <= MaxVerticalRadians)
        {
            return Desired;
        }

        if (FMath::Abs(DesiredYaw) > MaxHorizontalRadians)
        {
            const FVector PreviousHorizontal = FVector::VectorPlaneProject(PreviousDirection, Up).GetSafeNormal();
            if (!PreviousHorizontal.IsNearlyZero())
            {
                const float PreviousYaw = FMath::Atan2(
                    FVector::DotProduct(PreviousHorizontal, Right),
                    FVector::DotProduct(PreviousHorizontal, HorizontalForward));
                const float OtherYaw = DesiredYaw + (DesiredYaw < 0.0f ? 2.0f * UE_PI : -2.0f * UE_PI);
                if (FMath::Abs(OtherYaw - PreviousYaw) < FMath::Abs(DesiredYaw - PreviousYaw))
                {
                    // 跨过正后方时沿用距离上一帧更近的水平转向侧
                    DesiredYaw = OtherYaw;
                }
            }
        }

        const float LimitedYaw = FMath::Clamp(DesiredYaw, -MaxHorizontalRadians, MaxHorizontalRadians);
        const float LimitedPitch = FMath::Clamp(
            DesiredPitch, InputPitch - MaxVerticalRadians, InputPitch + MaxVerticalRadians);
        const FVector HorizontalDirection = HorizontalForward * FMath::Cos(LimitedYaw)
            + Right * FMath::Sin(LimitedYaw);
        return (HorizontalDirection * FMath::Cos(LimitedPitch)
            + Up * FMath::Sin(LimitedPitch)).GetSafeNormal();
    }
};
