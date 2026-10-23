#pragma once

#include "CoreMinimal.h"

/** 由输入姿势扰动与目标纠正共同维护的世界空间朝向 */
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
     * 接收输入姿势的旋转增量 再按时间连续纠正到真实目标
     * @param InputDirection	当前输入姿势的世界单位朝向
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
            PreviousInputDirection = InputDirection;
            bInitialized = true;
        }

        if (DeltaSeconds <= 0.0f)
        {
            return Direction;
        }

        // 姿势先受到扰动 再由持续的瞄准纠正消除误差
        const FQuat PoseDelta = FQuat::FindBetweenNormals(PreviousInputDirection, InputDirection);
        Direction = PoseDelta.RotateVector(Direction).GetSafeNormal();
        PreviousInputDirection = InputDirection;

        const float ResponseAlpha = 1.0f - FMath::Exp(-ResponseSpeed * DeltaSeconds);
        const FQuat Correction = FQuat::FindBetweenNormals(Direction, TargetDirection);
        Direction = FQuat::Slerp(FQuat::Identity, Correction, ResponseAlpha)
            .RotateVector(Direction).GetSafeNormal();
        return Direction;
    }

private:
    /** 上一次实际响应方向 */
    FVector Direction = FVector::ForwardVector;

    /** 上一次未经过 IK 的输入姿势方向 */
    FVector PreviousInputDirection = FVector::ForwardVector;

    /** 是否已经取得本轮输入姿势 */
    bool bInitialized = false;
};
