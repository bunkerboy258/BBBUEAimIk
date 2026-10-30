#pragma once

#include "AimIKAngleLimit.h"

/** 使用瞄准源参考系响应目标 世界方向历史由节点独立持有 */
struct FAimIKResponse final
{
    /** @return 清除历史 下次从当前输入瞄准源前向开始 */
    void Reset()
    {
        bInitialized = false;
    }

    /**
     * 在同一参考系中分别响应水平与俯仰目标
     * @param FrameWorld	求解前瞄准源的世界参考系
     * @param TargetDirectionWorld	真实目标的世界单位方向
     * @param ResponseSpeed	指数响应速率 单位为每秒
     * @param DeltaSeconds	仅消费一次的动画更新时间
     * @param HorizontalDegrees	相对瞄准源允许的最大水平偏角
     * @param VerticalDegrees	相对瞄准源允许的最大垂直偏角
     * @return 本次实际响应的世界单位方向 不修改输入目标
     */
    FVector Update(const FAimIKReferenceFrame& FrameWorld, const FVector& TargetDirectionWorld,
        float ResponseSpeed, float DeltaSeconds, float HorizontalDegrees, float VerticalDegrees)
    {
        if (!bInitialized)
        {
            DirectionWorld = FrameWorld.Forward;
            bInitialized = true;
        }

        // NOTE: 重大技术路线修正 参考系统一来自输入瞄准源 禁止重新混入组件轴或三维短弧追随
        const FVector2D Limits = FAimIKAngleLimit::GetLimits(HorizontalDegrees, VerticalDegrees);
        FVector2D Previous = FrameWorld.ToAngles(DirectionWorld);
        Previous.X = FMath::Clamp(Previous.X, -Limits.X, Limits.X);
        Previous.Y = FMath::Clamp(Previous.Y, -Limits.Y, Limits.Y);
        FVector2D Desired = FrameWorld.ToAngles(TargetDirectionWorld, Previous.X);
        Desired.X = FAimIKAngleLimit::ResolveYaw(Desired.X, Previous.X, Limits.X);
        Desired.Y = FMath::Clamp(Desired.Y, -Limits.Y, Limits.Y);

        const double Alpha = DeltaSeconds > 0.0f
            ? 1.0 - FMath::Exp(-static_cast<double>(ResponseSpeed) * DeltaSeconds)
            : 0.0;
        DirectionWorld = FrameWorld.FromAngles(Previous + (Desired - Previous) * Alpha);
        return DirectionWorld;
    }

private:
    /** 上次实际输出的世界方向 不随本帧参考骨重复旋转 */
    FVector DirectionWorld = FVector::ForwardVector;

    /** 当前是否存在响应历史 */
    bool bInitialized = false;
};
