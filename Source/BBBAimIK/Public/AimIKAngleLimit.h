#pragma once

#include "AimIKReferenceFrame.h"

/** 相对同一瞄准源参考系限制修正角度 */
struct FAimIKAngleLimit final
{
    /**
     * 取得方向表示允许的角度范围
     * @param HorizontalDegrees	最大水平偏角
     * @param VerticalDegrees	最大垂直偏角
     * @return 水平与垂直上限 单位为弧度
     */
    static FVector2D GetLimits(float HorizontalDegrees, float VerticalDegrees)
    {
        return FVector2D(
            FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(HorizontalDegrees), 1.0, 180.0)),
            FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(VerticalDegrees), 1.0, 90.0)));
    }

    /**
     * 选择连续的水平分支并限制到允许范围
     * @param DesiredYaw	目标水平角
     * @param PreviousYaw	上一响应在当前参考系中的水平角
     * @param Limit	水平上限
     * @return 可用于当前允许区间内插值的水平角
     */
    static double ResolveYaw(double DesiredYaw, double PreviousYaw, double Limit)
    {
        if (Limit < UE_DOUBLE_PI && FMath::Abs(DesiredYaw) <= Limit)
        {
            return DesiredYaw;
        }
        double Difference = FMath::UnwindRadians(DesiredYaw - PreviousYaw);
        if (FMath::IsNearlyEqual(FMath::Abs(Difference), UE_DOUBLE_PI, 1.e-8))
        {
            Difference = PreviousYaw < 0.0 ? -UE_DOUBLE_PI : UE_DOUBLE_PI;
        }
        const double ContinuousYaw = PreviousYaw + Difference;
        if (Limit >= UE_DOUBLE_PI)
        {
            return ContinuousYaw;
        }
        return FMath::Clamp(ContinuousYaw, -Limit, Limit);
    }

    /**
     * 限制目标方向 不建立第二套参考轴
     * @param Frame	求解前瞄准源参考系
     * @param DesiredDirection	期望方向
     * @param HorizontalDegrees	最大水平偏角
     * @param VerticalDegrees	最大垂直偏角
     * @param PreviousDirection	选择背后转向分支的历史方向
     * @return 限制后的单位方向
     */
    static FVector ClampDirection(const FAimIKReferenceFrame& Frame, const FVector& DesiredDirection,
        float HorizontalDegrees, float VerticalDegrees, const FVector& PreviousDirection)
    {
        const FVector2D Limits = GetLimits(HorizontalDegrees, VerticalDegrees);
        const FVector2D Previous = Frame.ToAngles(PreviousDirection);
        FVector2D Desired = Frame.ToAngles(DesiredDirection, Previous.X);
        Desired.X = ResolveYaw(Desired.X, Previous.X, Limits.X);
        Desired.Y = FMath::Clamp(Desired.Y, -Limits.Y, Limits.Y);
        return Frame.FromAngles(Desired);
    }
};
