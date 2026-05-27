#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_AimIK.generated.h"

USTRUCT(BlueprintType)
struct FAimIKBoneRef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone")
    FName BoneName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Weight = 1.0f;
};

/**
 * AimIK AnimNode - CCD-style chain solver inspired by RootMotion FinalIK.
 *
 * Rotates a configured chain so the pose-local aim source points at the target.
 * The aim source is reconstructed from the current pose using AimSourceBoneName
 * and AimSourceLocalTransform, eliminating external real-time transform feedback.
 *
 * Recommended spine-only chain for rifle aiming:
 *   spine_01 (0.2) -> spine_02 (0.3) -> spine_03 (0.5) -> spine_04 (0.7) -> spine_05 (0.8)
 */
USTRUCT(BlueprintInternalUseOnly)
struct BBBAIMIK_API FAnimNode_AimIK : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    // === 骨骼链（脊柱层级，从根到尖端），带每骨骼权重 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoneChain")
    TArray<FAimIKBoneRef> BoneChain;

    // 承载虚拟瞄准源的骨骼。等价于 FinalIK 的变换父级。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FName AimSourceBoneName;

    // 瞄准源相对于 AimSourceBoneName 的局部变换。
    // 必须是稳定的绑定关系，而不是每帧的外部快照。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
    FTransform AimSourceLocalTransform = FTransform::Identity;

    // 瞄准源上应指向目标的局部轴（默认：X+）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FVector AimAxis = FVector::ForwardVector;

    // === 极轴（防止身体翻转）===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleTarget = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PoleWeight = 0.0f;

    // === 钳制（防止 180 度奇点跳变）===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ClampWeight = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0", ClampMax = "2"))
    int32 ClampSmoothing = 2;

    // === 求解器设置 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "1"))
    int32 MaxIterations = 4;

    // 提前停止迭代的最小角度误差（度）。0 = 始终迭代 MaxIterations。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0"))
    float Tolerance = 0.0f;

    // 组件空间中的目标位置。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    FVector AimTarget = FVector::ZeroVector;

    // 显式有效标志。零是有效的组件空间目标。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    bool bHasValidAimTarget = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety")
    bool bEnableMinTargetDistanceGuard = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0.0"))
    float MinTargetDistance = 30.0f;

    // 调试日志开关。在 EvaluateSkeletalControl_AnyThread 中使用 UE_LOG。
    // 避免在生产环境中启用；在动画评估循环中记录日志有性能开销。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugLogging = false;

    // 求解阶段日志采样间隔，避免 Debug 开启时逐帧刷屏。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "1"))
    int32 DebugSolveLogInterval = 60;

    // === FAnimNode_SkeletalControlBase 接口 ===
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
    TArray<int32> CachedBoneIndices;
    int32 AimSourceBoneIndex = INDEX_NONE;
    bool bCachedBonesValid = false;
    bool bAimSourceIsChainDescendant = false;

    // 核心求解器
    void SolveAimIK(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms);
    
    // 钳制目标以避免 180 度奇点
    FVector GetClampedTargetCS(const FVector& AimBonePosCS, const FVector& AimBoneForwardCS, const FVector& TargetCS) const;
    
    // 当目标位于链延伸线上时进行偏移（线性奇点）。
    FVector GetSingularityOffset(const FVector& FirstBonePosCS, const FVector& AimPosCS, const FVector& TargetPosCS) const;
    
    // 旋转一个链节，并将该增量传播到下游链节和瞄准变换。
    void RotateBoneToTarget(
        int32 ChainIndex,
        const FVector& TargetPosCS,
        float Weight,
        TArray<FTransform>& InOutChainTransforms,
        FTransform& InOutAimTransformCS);
};
