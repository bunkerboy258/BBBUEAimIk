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

    // === Bone Chain (spine hierarchy, root to tip) with per-bone weights ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoneChain")
    TArray<FAimIKBoneRef> BoneChain;

    // Bone that carries the virtual aim source. Equivalent to FinalIK's transform parent.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FName AimSourceBoneName;

    // Local transform of the aim source relative to AimSourceBoneName.
    // Must be a stable binding relationship, not a per-frame external snapshot.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
    FTransform AimSourceLocalTransform = FTransform::Identity;

    // Local axis on the aim source that should point to target (default: X+).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FVector AimAxis = FVector::ForwardVector;

    // === Pole (prevents body from flipping) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleTarget = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PoleWeight = 0.0f;

    // === Clamp (prevents 180-degree singularity jump) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ClampWeight = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0", ClampMax = "2"))
    int32 ClampSmoothing = 2;

    // === Solver Settings ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "1"))
    int32 MaxIterations = 4;

    // Minimum angular error (degrees) to stop iterating early. 0 = always iterate MaxIterations.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0"))
    float Tolerance = 0.0f;

    // Target position in component space.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    FVector AimTarget = FVector::ZeroVector;

    // Explicit validity flag. Zero is a valid component-space target.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    bool bHasValidAimTarget = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety")
    bool bEnableMinTargetDistanceGuard = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0.0"))
    float MinTargetDistance = 30.0f;

    // Debug logging toggle. Uses UE_LOG inside EvaluateSkeletalControl_AnyThread.
    // Avoid enabling in production; logging inside the anim evaluation loop has overhead.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugLogging = false;

    // === FAnimNode_SkeletalControlBase Interface ===
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
    TArray<int32> CachedBoneIndices;
    int32 AimSourceBoneIndex = INDEX_NONE;
    bool bCachedBonesValid = false;
    bool bAimSourceIsChainDescendant = false;

    // Core solver
    void SolveAimIK(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms);
    
    // Clamp target to avoid 180-degree singularity
    FVector GetClampedTargetCS(const FVector& AimBonePosCS, const FVector& AimBoneForwardCS, const FVector& TargetCS) const;
    
    // Offset target when it lies on the chain extension line (linear singularity).
    FVector GetSingularityOffset(const FVector& FirstBonePosCS, const FVector& AimPosCS, const FVector& TargetPosCS) const;
    
    // Rotate one chain link and propagate that delta through downstream links and the aim transform.
    void RotateBoneToTarget(
        int32 ChainIndex,
        const FVector& TargetPosCS,
        float Weight,
        TArray<FTransform>& InOutChainTransforms,
        FTransform& InOutAimTransformCS);
};
