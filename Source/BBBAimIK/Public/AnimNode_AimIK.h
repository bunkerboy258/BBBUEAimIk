#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_AimIK.generated.h"

struct FReferenceSkeleton;

/**
 * 骨骼链单节配置
 */
USTRUCT(BlueprintType)
struct FAimIKBoneRef
{
    GENERATED_BODY()

    /** 骨骼名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone")
    FName BoneName;

    /** 该骨骼在求解中分摊的旋转权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Weight = 1.0f;
};

/**
 * 旋转骨骼链，使姿态局部的瞄准源指向目标点
 *
 * 瞄准源通过 AimSourceBoneName 与 AimSourceLocalTransform 从当前姿态重建
 */
USTRUCT(BlueprintInternalUseOnly)
struct BBBAIMIK_API FAnimNode_AimIK : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    /** 骨骼链，按骨骼层级从根到尖端排列 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoneChain")
    TArray<FAimIKBoneRef> BoneChain;

    ////

    /** 承载虚拟瞄准源的骨骼 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FName AimSourceBoneName;

    /** 瞄准源相对承载骨骼的稳定局部变换 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
    FTransform AimSourceLocalTransform = FTransform::Identity;

    /** 瞄准源上应指向目标的局部轴 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FVector AimAxis = FVector::ForwardVector;

    ////

    /** 用于约束身体翻转的局部极轴 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleAxis = FVector::UpVector;

    /** 极轴目标的组件空间位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleTarget = FVector::ZeroVector;

    /** 极轴纠偏权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PoleWeight = 0.0f;

    ////

    /** 目标方向钳制强度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ClampWeight = 0.1f;

    /** 钳制结果的平滑迭代次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0", ClampMax = "2"))
    int32 ClampSmoothing = 2;

    ////

    /** CCD 最大迭代次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "1"))
    int32 MaxIterations = 4;

    /** 提前停止迭代的最小角度误差，单位为度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0"))
    float Tolerance = 0.0f;

    /** 目标的组件空间位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    FVector AimTarget = FVector::ZeroVector;

    /** 目标是否有效 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    bool bHasValidAimTarget = false;

    ////

    /** 是否启用最小目标距离防呆 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety")
    bool bEnableMinTargetDistanceGuard = true;

    /** 目标与瞄准源之间允许的最小距离 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0.0"))
    float MinTargetDistance = 30.0f;

    ////

    /** 是否输出求解诊断日志 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugLogging = false;

    /** 求解诊断日志的采样间隔 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "1"))
    int32 DebugSolveLogInterval = 60;

    /**
     * 判断瞄准源是否位于骨骼链尖端之下
     *
     * @param ReferenceSkeleton  参考骨架
     * @return 骨骼层级满足求解要求时返回 true
     */
    bool HasValidAimSourceHierarchy(const FReferenceSkeleton& ReferenceSkeleton) const;

    //~ Begin FAnimNode_SkeletalControlBase Interface
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(
        const USkeleton* Skeleton,
        const FBoneContainer& RequiredBones) override;
    //~ End FAnimNode_SkeletalControlBase Interface

private:
    /** 骨骼链的紧凑姿态索引缓存 */
    TArray<int32> CachedBoneIndices;

    /** 瞄准源骨骼的紧凑姿态索引 */
    int32 AimSourceBoneIndex = INDEX_NONE;

    /** 骨骼缓存是否全部有效 */
    bool bCachedBonesValid = false;

    /** 瞄准源是否为骨骼链尖端自身或其后代 */
    bool bAimSourceIsChainDescendant = false;

    /** 是否已经记录上一帧的输入姿态 */
    bool bHasPreviousInputPose = false;

    /** 上一帧求解前的瞄准源骨骼组件空间变换 */
    FTransform PreviousAimSourceBoneTransformCS = FTransform::Identity;

    /** 上一帧求解前的骨骼链组件空间变换 */
    TArray<FTransform> PreviousChainTransformsCS;

    /**
     * 执行求解并生成骨骼输出
     *
     * @param Output             组件空间姿态上下文
     * @param OutBoneTransforms  输出骨骼变换
     * @return 无
     */
    void SolveAimIK(
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms);

    /**
     * 重置输入姿态诊断历史
     *
     * @return 无
     */
    void ResetInputPoseDiagnostics();

    /**
     * 记录输入姿态并在跳变时输出诊断日志
     *
     * @param AimSourceBoneTransformCS  当前瞄准源骨骼组件空间变换
     * @param ChainTransformsCS         当前骨骼链组件空间变换
     * @param AimForwardCS              当前瞄准前向
     * @param AimPositionCS             当前瞄准源组件空间位置
     * @return 无
     */
    void UpdateInputPoseDiagnostics(
        const FTransform& AimSourceBoneTransformCS,
        const TArray<FTransform>& ChainTransformsCS,
        const FVector& AimForwardCS,
        const FVector& AimPositionCS);

    /** @return 当前帧是否应输出求解采样日志 */
    bool ShouldLogSolve() const;

    /**
     * 输出求解前的采样日志
     *
     * @param AimTransformCS  当前瞄准源组件空间变换
     * @param AimForwardCS    当前瞄准前向
     * @return 无
     */
    void LogSolveInput(
        const FTransform& AimTransformCS,
        const FVector& AimForwardCS) const;

    /**
     * 输出求解后的采样日志
     *
     * @param AimTransformCS    求解后的瞄准源组件空间变换
     * @param EffectiveTargetCS 求解使用的组件空间目标位置
     * @return 无
     */
    void LogSolveOutput(
        const FTransform& AimTransformCS,
        const FVector& EffectiveTargetCS) const;
};
