#pragma once

#include "CoreMinimal.h"

/** 独立于输入姿势变化并按时间追随目标的世界空间朝向 */
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
     * 从首次输入姿势开始按时间连续追随真实目标 后续姿势变化不累积到响应方向
     * @param InputDirection	仅初始化时读取的当前输入姿势世界单位朝向
     * @param TargetDirection	真实瞄准目标的世界单位方向
     * @param ResponseSpeed	指数纠正速率 单位为每秒 越大越快
     * @param DeltaSeconds	本次更新间隔 单位为秒 零间隔不重复积分
     * @return 本帧实际响应的世界单位方向 不修改目标
     */
    FVector Update(const FVector& InputDirection, const FVector& TargetDirection,
        float ResponseSpeed, float DeltaSeconds)
    {
        if (!bInitialized)
        {
            Direction = InputDirection;
            bInitialized = true;
        }

        if (DeltaSeconds <= 0.0f)
        {
            return Direction;
        }

        // 只平滑目标方向 当前输入姿势由本帧骨骼求解独立纠正
        const float ResponseAlpha = 1.0f - FMath::Exp(-ResponseSpeed * DeltaSeconds);
        const FQuat Correction = FQuat::FindBetweenNormals(Direction, TargetDirection);
        Direction = FQuat::Slerp(FQuat::Identity, Correction, ResponseAlpha)
            .RotateVector(Direction).GetSafeNormal();
        return Direction;
    }

private:
    /** 上一次实际响应方向 */
    FVector Direction = FVector::ForwardVector;

    /** 是否已经取得本轮输入姿势 */
    bool bInitialized = false;
};
