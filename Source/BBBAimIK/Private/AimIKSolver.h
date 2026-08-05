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

    /** 骨骼链配置，按骨骼层级从根到尖端排列 */
    const TArray<FAimIKBoneRef>& BoneChain;

    /** 瞄准源上应指向目标的局部轴 */
    FVector AimAxis = FVector::ForwardVector;

    /** 目标的组件空间位置 */
    FVector AimTargetCS = FVector::ZeroVector;

    /** 用于约束身体翻转的局部极轴 */
    FVector PoleAxis = FVector::UpVector;

    /** 极轴目标的组件空间位置 */
    FVector PoleTargetCS = FVector::ZeroVector;

    /** 极轴纠偏权重 */
    float PoleWeight = 0.0f;

    /** 目标方向钳制强度 */
    float ClampWeight = 0.0f;

    /** 钳制结果的平滑迭代次数 */
    int32 ClampSmoothing = 0;

    /** CCD 最大迭代次数 */
    int32 MaxIterations = 1;

    /** 提前停止迭代的最小角度误差，单位为度 */
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
