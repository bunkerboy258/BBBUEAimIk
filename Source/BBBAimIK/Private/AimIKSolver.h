#pragma once

#include "AnimNode_AimIK.h"

/**
 * AimIK 求解参数
 */
struct FAimIKSolverInput
{
    /**
     * 创建求解参数
     *
     * @param InBoneChain  骨骼链配置
     */
    explicit FAimIKSolverInput(const TArray<FAimIKBoneRef>& InBoneChain)
        : BoneChain(InBoneChain)
    {
    }

    const TArray<FAimIKBoneRef>& BoneChain;
    FVector AimAxis = FVector::ForwardVector;
    FVector AimTargetCS = FVector::ZeroVector;
    FVector PoleAxis = FVector::UpVector;
    FVector PoleTargetCS = FVector::ZeroVector;
    float PoleWeight = 0.0f;
    float ClampWeight = 0.0f;
    int32 ClampSmoothing = 0;
    int32 MaxIterations = 1;
    float Tolerance = 0.0f;
};

/**
 * 基于组件空间变换的 AimIK 求解器
 */
class FAimIKSolver final
{
public:
    /**
     * 求解骨骼链并原地更新组件空间变换
     *
     * @param Input                 求解参数
     * @param InOutChainTransforms  骨骼链组件空间变换
     * @param InOutAimTransformCS   瞄准源组件空间变换
     * @return 求解使用的组件空间目标位置
     */
    static FVector Solve(
        const FAimIKSolverInput& Input,
        TArray<FTransform>& InOutChainTransforms,
        FTransform& InOutAimTransformCS);

private:
    /**
     * 钳制目标方向以规避 180 度奇点
     *
     * @param Input             求解参数
     * @param AimPositionCS     瞄准源组件空间位置
     * @param AimForwardCS      瞄准前向组件空间方向
     * @param TargetCS          原始目标组件空间位置
     * @return 钳制后的组件空间目标位置
     */
    static FVector GetClampedTargetCS(
        const FAimIKSolverInput& Input,
        const FVector& AimPositionCS,
        const FVector& AimForwardCS,
        const FVector& TargetCS);

    /**
     * 计算线性奇点偏移
     *
     * @param FirstBonePositionCS  链根骨骼组件空间位置
     * @param AimPositionCS        瞄准源组件空间位置
     * @param TargetPositionCS     目标组件空间位置
     * @return 未触及奇点时返回零向量，否则返回偏移向量
     */
    static FVector GetSingularityOffset(
        const FVector& FirstBonePositionCS,
        const FVector& AimPositionCS,
        const FVector& TargetPositionCS);

    /**
     * 旋转单个链节并传播旋转增量
     *
     * @param Input                 求解参数
     * @param ChainIndex            当前链节下标
     * @param TargetPositionCS      目标组件空间位置
     * @param Weight                当前旋转权重
     * @param InOutChainTransforms  骨骼链组件空间变换
     * @param InOutAimTransformCS   瞄准源组件空间变换
     * @return 无
     */
    static void RotateBoneToTarget(
        const FAimIKSolverInput& Input,
        int32 ChainIndex,
        const FVector& TargetPositionCS,
        float Weight,
        TArray<FTransform>& InOutChainTransforms,
        FTransform& InOutAimTransformCS);
};
