#pragma once

#include "CoreMinimal.h"

/** 求解前瞄准源的正交参考系 同一次求解期间保持不变 */
struct FAimIKReferenceFrame final
{
    /** 参考原点 */
    FVector Origin = FVector::ZeroVector;
    /** 瞄准前向 */
    FVector Forward = FVector::ForwardVector;
    /** 正交右向 */
    FVector Right = FVector::RightVector;
    /** 正交上向 */
    FVector Up = FVector::UpVector;

    /**
     * 从输入姿态中的瞄准源建立参考系
     * @param SourceTransform	求解前瞄准源变换
     * @param AimAxis	瞄准源局部前轴
     * @param AimUpAxis	瞄准源局部参考上轴
     * @param OutFrame	成功时输出的参考系
     * @return 变换与参考轴有效时返回 true
     */
    static bool TryCreate(const FTransform& SourceTransform, const FVector& AimAxis,
        const FVector& AimUpAxis, FAimIKReferenceFrame& OutFrame)
    {
        if (!SourceTransform.IsValid() || AimAxis.ContainsNaN() || AimUpAxis.ContainsNaN())
        {
            return false;
        }
        const FVector ForwardAxis = SourceTransform.TransformVectorNoScale(AimAxis).GetSafeNormal();
        const FVector UpAxis = SourceTransform.TransformVectorNoScale(AimUpAxis).GetSafeNormal();
        const FVector RightAxis = FVector::CrossProduct(UpAxis, ForwardAxis).GetSafeNormal();
        if (ForwardAxis.IsNearlyZero() || UpAxis.IsNearlyZero() || RightAxis.IsNearlyZero())
        {
            return false;
        }
        OutFrame.Origin = SourceTransform.GetLocation();
        OutFrame.Forward = ForwardAxis;
        OutFrame.Right = RightAxis;
        OutFrame.Up = FVector::CrossProduct(ForwardAxis, RightAxis).GetSafeNormal();
        return true;
    }

    /**
     * 转换既有参考系 不重新选取参考轴
     * @param Transform	空间转换变换
     * @return 同一参考系在目标空间中的表示
     */
    FAimIKReferenceFrame TransformBy(const FTransform& Transform) const
    {
        FAimIKReferenceFrame Result;
        Result.Origin = Transform.TransformPosition(Origin);
        Result.Forward = Transform.TransformVectorNoScale(Forward).GetSafeNormal();
        Result.Right = Transform.TransformVectorNoScale(Right).GetSafeNormal();
        Result.Up = Transform.TransformVectorNoScale(Up).GetSafeNormal();
        return Result;
    }

    /**
     * 分解参考系中的水平与俯仰角
     * @param Direction	与参考系处于同一空间的有效方向
     * @param PoleYaw	方向位于参考上轴极点时沿用的水平角
     * @return X 为水平角 Y 为俯仰角 单位为弧度
     */
    FVector2D ToAngles(const FVector& Direction, double PoleYaw = 0.0) const
    {
        const FVector Unit = Direction.GetSafeNormal();
        const double X = FVector::DotProduct(Unit, Forward);
        const double Y = FVector::DotProduct(Unit, Right);
        const double Z = FVector::DotProduct(Unit, Up);
        const double Horizontal = FMath::Sqrt(X * X + Y * Y);
        const double Yaw = Horizontal > UE_SMALL_NUMBER ? FMath::Atan2(Y, X) : PoleYaw;
        return FVector2D(Yaw, FMath::Atan2(Z, Horizontal));
    }

    /**
     * 将参考角还原成方向
     * @param Angles	水平与俯仰弧度
     * @return 与参考系处于同一空间的单位方向
     */
    FVector FromAngles(const FVector2D& Angles) const
    {
        const FVector Horizontal = Forward * FMath::Cos(Angles.X) + Right * FMath::Sin(Angles.X);
        return (Horizontal * FMath::Cos(Angles.Y) + Up * FMath::Sin(Angles.Y)).GetSafeNormal();
    }
};
