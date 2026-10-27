#pragma once

#include "CoreMinimal.h"
#include "AimIKAngleLimit.h"

/** 按时间追随目标并分别限制水平与垂直偏角的世界空间朝向 */
struct FAimIKResponse
{
    /**
     * 清除上一轮响应 在重新启用时从当前输入姿势开始
     * @return 无
     */
    void Reset()
    {
        bInitialized = false;
    }

    /**
     * 从首次输入姿势开始按时间连续追随真实目标 超出角度上限时将状态移到边界
     * @param InputDirection	当前输入姿势的世界单位朝向
     * @param TargetDirection	真实瞄准目标的世界单位方向
     * @param UpDirection	角色组件的世界竖直方向
     * @param ReferenceForward	输入方向竖直时使用的角色组件世界前向
     * @param ResponseSpeed	指数纠正速率 单位为每秒 越大越快
     * @param DeltaSeconds	本次更新间隔 单位为秒 零间隔不重复积分
     * @param MaxHorizontalAngleDegrees	相对当前输入姿势允许的最大水平偏角
     * @param MaxVerticalAngleDegrees	相对当前输入姿势允许的最大垂直偏角
     * @return 本帧实际响应的世界单位方向 不修改目标
     */
    FVector Update(const FVector& InputDirection, const FVector& TargetDirection,
        const FVector& UpDirection, const FVector& ReferenceForward,
        float ResponseSpeed, float DeltaSeconds,
        float MaxHorizontalAngleDegrees, float MaxVerticalAngleDegrees)
    {
        if (!bInitialized)
        {
            Direction = InputDirection.GetSafeNormal();
            PreviousInputDirection = Direction;
            bInitialized = true;
        }

        const FVector PreviousSide = FVector::CrossProduct(InputDirection, Direction).IsNearlyZero()
            ? PreviousInputDirection
            : Direction;
        const FVector PreviousWithinLimits = FAimIKAngleLimit::ClampDirection(
            InputDirection, Direction, UpDirection, ReferenceForward,
            MaxHorizontalAngleDegrees, MaxVerticalAngleDegrees, PreviousSide);
        Direction = PreviousWithinLimits;
        float ResponseAlpha = 0.0f;
        if (DeltaSeconds > 0.0f)
        {
            // 只平滑目标方向 当前输入姿势由本帧骨骼求解独立纠正
            ResponseAlpha = 1.0f - FMath::Exp(-ResponseSpeed * DeltaSeconds);
            const float TargetDot = FVector::DotProduct(Direction, TargetDirection);
            FQuat Correction = FQuat::FindBetweenNormals(Direction, TargetDirection);
            if (TargetDot < -1.0f + KINDA_SMALL_NUMBER)
            {
                // 正后方没有唯一转向侧 优先绕世界竖直轴转向
                FVector TurnAxis = UpDirection
                    - Direction * FVector::DotProduct(UpDirection, Direction);
                if (TurnAxis.IsNearlyZero())
                {
                    TurnAxis = ReferenceForward
                        - Direction * FVector::DotProduct(ReferenceForward, Direction);
                }
                Correction = FQuat(TurnAxis.GetSafeNormal(), UE_PI);
            }
            Direction = FQuat::Slerp(FQuat::Identity, Correction, ResponseAlpha)
                .RotateVector(Direction).GetSafeNormal();
        }

        // 上限触发时同步更新保存的方向 避免下一帧继续追赶过旧的状态
        const FVector LimitedCandidate = FAimIKAngleLimit::ClampDirection(
            InputDirection, Direction, UpDirection, ReferenceForward,
            MaxHorizontalAngleDegrees, MaxVerticalAngleDegrees, PreviousWithinLimits);
        const float DirectionChange = FMath::Acos(FMath::Clamp(
            FVector::DotProduct(PreviousWithinLimits, LimitedCandidate), -1.0f, 1.0f));
        if (DirectionChange > FMath::DegreesToRadians(
            FMath::Min(MaxHorizontalAngleDegrees, MaxVerticalAngleDegrees)))
        {
            // 目标跨越水平或垂直边界时沿短弧平滑过渡
            const FQuat Step = FQuat::FindBetweenNormals(
                PreviousWithinLimits, LimitedCandidate);
            Direction = FQuat::Slerp(FQuat::Identity, Step, ResponseAlpha)
                .RotateVector(PreviousWithinLimits).GetSafeNormal();
        }
        else
        {
            Direction = LimitedCandidate;
        }
        Direction = FAimIKAngleLimit::ClampDirection(
            InputDirection, Direction, UpDirection, ReferenceForward,
            MaxHorizontalAngleDegrees, MaxVerticalAngleDegrees, PreviousWithinLimits);
        PreviousInputDirection = InputDirection.GetSafeNormal();
        return Direction;
    }

private:
    /** 上一次实际响应方向 */
    FVector Direction = FVector::ForwardVector;

    /** 上次输入姿势朝向 用于姿势突然翻转时继承转向侧 */
    FVector PreviousInputDirection = FVector::ForwardVector;

    /** 是否已经取得本轮输入姿势 */
    bool bInitialized = false;
};
