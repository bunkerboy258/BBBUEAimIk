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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "骨骼", meta = (DisplayName = "骨骼名称"))
    FName BoneName;

    /** 该骨骼每轮求解的旋转权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "骨骼", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "直接控制该骨骼每轮求解的旋转比例 0 为禁用 1 为完全应用", DisplayName = "旋转权重"))
    float Weight = 1.0f;
};

/**
 * 旋转骨骼链 使姿态局部的瞄准源指向目标点
 *
 * 瞄准源通过 AimSourceBoneName 与 AimSourceLocalTransform 从当前姿态重建
 */
USTRUCT(BlueprintInternalUseOnly)
struct BBBAIMIK_API FAnimNode_AimIK : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    /** 骨骼链 按骨骼层级从根到尖端排列 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "骨骼链", meta = (DisplayName = "骨骼链"))
    TArray<FAimIKBoneRef> BoneChain;

    ////

    /** 承载虚拟瞄准源的骨骼 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "瞄准", meta = (DisplayName = "瞄准源骨骼名"))
    FName AimSourceBoneName;

    /** 瞄准源相对承载骨骼的稳定局部变换 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "瞄准", meta = (PinShownByDefault, DisplayName = "瞄准源局部变换"))
    FTransform AimSourceLocalTransform = FTransform::Identity;

    /** 瞄准源上应指向目标的局部轴 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "瞄准", meta = (DisplayName = "瞄准轴"))
    FVector AimAxis = FVector::ForwardVector;

    ////

    /** 用于约束身体翻转的局部极轴 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "极轴", meta = (DisplayName = "极轴"))
    FVector PoleAxis = FVector::UpVector;

    /** 极轴目标的组件空间位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "极轴", meta = (DisplayName = "极轴目标"))
    FVector PoleTarget = FVector::ZeroVector;

    /** 极轴纠偏权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "极轴", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "极轴权重"))
    float PoleWeight = 0.0f;

    ////

    /** 目标方向钳制强度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "钳制", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "方向钳制权重"))
    float ClampWeight = 0.1f;

    /** 钳制结果的平滑迭代次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "钳制", meta = (ClampMin = "0", ClampMax = "2", DisplayName = "钳制平滑次数"))
    int32 ClampSmoothing = 2;

    ////

    /** CCD 最大迭代次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "求解器", meta = (ClampMin = "1", DisplayName = "最大迭代次数"))
    int32 MaxIterations = 4;

    /** 提前停止迭代的最小角度误差 单位为度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "求解器", meta = (ClampMin = "0.0", DisplayName = "角度容差"))
    float Tolerance = 0.0f;

    /** 目标的组件空间位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "求解器", meta = (PinShownByDefault, DisplayName = "瞄准目标"))
    FVector AimTarget = FVector::ZeroVector;

    /** 基础目标方向的指数跟随速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "瞄准", meta = (PinShownByDefault, ClampMin = "0.01", ToolTip = "数值越大枪口越快追上目标 额外角度在跟随计算之后叠加", DisplayName = "瞄准跟随速度"))
    float AimFollowSpeed = 18.0f;

    /** 跟随后叠加的向上与向右角度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "瞄准", meta = (PinShownByDefault, ToolTip = "X 向上 Y 向右 单位为度 不经过目标跟随平滑", DisplayName = "瞄准角度偏移"))
    FVector2D AimOffsetDegrees = FVector2D::ZeroVector;

    ////

    /** 是否启用最小目标距离防呆 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "安全", meta = (DisplayName = "启用最小目标距离保护"))
    bool bEnableMinTargetDistanceGuard = true;

    /** 目标与瞄准源之间允许的最小距离 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "安全", meta = (ClampMin = "0.0", DisplayName = "最小目标距离"))
    float MinTargetDistance = 30.0f;

    ////

    /** 是否输出求解诊断日志 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试", meta = (DisplayName = "启用调试日志"))
    bool bEnableDebugLogging = false;

    /** 求解诊断日志的采样间隔 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试", meta = (ClampMin = "1", DisplayName = "调试日志采样间隔"))
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
    virtual void UpdateComponentPose_AnyThread(const FAnimationUpdateContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(
        const USkeleton* Skeleton,
        const FBoneContainer& RequiredBones) override;
    //~ End FAnimNode_SkeletalControlBase Interface

private:
    /** 本帧动画更新间隔 */
    float FollowDeltaSeconds = 0.0f;

    /** 尚未应用额外偏移的世界空间方向 */
    FVector FollowDirectionWorld = FVector::ForwardVector;

    /** 是否已初始化跟随方向 */
    bool bHasFollowDirection = false;

    /** 骨骼链的紧凑姿态索引缓存 */
    TArray<int32> CachedBoneIndices;

    /** 当前求值使用的骨骼链组件空间变换工作缓冲 */
    TArray<FTransform> WorkingChainTransformsCS;

    /** 瞄准源骨骼的紧凑姿态索引 */
    int32 AimSourceBoneIndex = INDEX_NONE;

    /** 骨骼缓存是否全部有效 */
    bool bCachedBonesValid = false;

    /** 瞄准源是否为骨骼链尖端自身或其后代 */
    bool bAimSourceIsChainDescendant = false;

    /** 求解诊断采样使用的求值帧计数 */
    uint64 DebugSolveFrameCounter = 0;

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
