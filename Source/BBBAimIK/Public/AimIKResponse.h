#pragma once

#include "CoreMinimal.h"
#include "AimIKAngleLimit.h"

/** 按时间追随目标并相对输入姿势限制最大偏角的世界空间朝向 */
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
     * @param ResponseSpeed	指数纠正速率 单位为每秒 越大越快
     * @param DeltaSeconds	本次更新间隔 单位为秒 零间隔不重复积分
     * @param MaxAngleDegrees	相对当前输入姿势允许的最大修正角度
     * @return 本帧实际响应的世界单位方向 不修改目标
     */
    FVector Update(const FVector& InputDirection, const FVector& TargetDirection,
        float ResponseSpeed, float DeltaSeconds, float MaxAngleDegrees)
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
        const FVector PreviousWithinCone = FAimIKAngleLimit::ClampDirection(
            InputDirection, Direction, MaxAngleDegrees, PreviousSide);
        Direction = PreviousWithinCone;
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
                FVector TurnAxis = FVector::UpVector
                    - Direction * FVector::DotProduct(FVector::UpVector, Direction);
                if (TurnAxis.IsNearlyZero())
                {
                    TurnAxis = FVector::RightVector;
                }
                Correction = FQuat(TurnAxis.GetSafeNormal(), UE_PI);
            }
            Direction = FQuat::Slerp(FQuat::Identity, Correction, ResponseAlpha)
                .RotateVector(Direction).GetSafeNormal();
        }

        // 上限触发时同步更新保存的方向 避免下一帧继续追赶过旧的状态
        const FVector LimitedCandidate = FAimIKAngleLimit::ClampDirection(
            InputDirection, Direction, MaxAngleDegrees, PreviousWithinCone);
        const float DirectionChange = FMath::Acos(FMath::Clamp(
            FVector::DotProduct(PreviousWithinCone, LimitedCandidate), -1.0f, 1.0f));
        if (DirectionChange > FMath::DegreesToRadians(MaxAngleDegrees))
        {
            // 目标跨越圆锥两侧时沿圆锥内的短弧平滑过渡
            const FQuat Step = FQuat::FindBetweenNormals(
                PreviousWithinCone, LimitedCandidate);
            Direction = FQuat::Slerp(FQuat::Identity, Step, ResponseAlpha)
                .RotateVector(PreviousWithinCone).GetSafeNormal();
        }
        else
        {
            Direction = LimitedCandidate;
        }
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
