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
            Direction = InputDirection;
            bInitialized = true;
        }

        if (DeltaSeconds > 0.0f)
        {
            // 只平滑目标方向 当前输入姿势由本帧骨骼求解独立纠正
            const float ResponseAlpha = 1.0f - FMath::Exp(-ResponseSpeed * DeltaSeconds);
            const FQuat Correction = FQuat::FindBetweenNormals(Direction, TargetDirection);
            Direction = FQuat::Slerp(FQuat::Identity, Correction, ResponseAlpha)
                .RotateVector(Direction).GetSafeNormal();
        }

        // 上限触发时同步更新保存的方向 避免下一帧继续追赶过旧的状态
        Direction = FAimIKAngleLimit::ClampDirection(
            InputDirection, Direction, MaxAngleDegrees);
        return Direction;
    }

private:
    /** 上一次实际响应方向 */
    FVector Direction = FVector::ForwardVector;

    /** 是否已经取得本轮输入姿势 */
    bool bInitialized = false;
};
