#include "AnimNode_AimIK.h"

#include "AimIKBoneHierarchy.h"
#include "AimIKSolver.h"
#include "AnimationRuntime.h"
#include "HAL/PlatformTime.h"
#include "ReferenceSkeleton.h"

void FAnimNode_AimIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    ResetInputPoseDiagnostics();

    const USkeleton* SkeletonAsset = RequiredBones.GetSkeletonAsset();
    if (!SkeletonAsset)
    {
        CachedBoneIndices.Reset();
        AimSourceBoneIndex = INDEX_NONE;
        bCachedBonesValid = false;
        bAimSourceIsChainDescendant = false;

        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Init] Invalid skeleton asset."));
        }

        return;
    }

    const FReferenceSkeleton& ReferenceSkeleton = SkeletonAsset->GetReferenceSkeleton();
    CachedBoneIndices.Reset(BoneChain.Num());
    for (const FAimIKBoneRef& BoneReference : BoneChain)
    {
        const int32 SkeletonIndex = ReferenceSkeleton.FindBoneIndex(BoneReference.BoneName);
        const FCompactPoseBoneIndex CompactPoseIndex = SkeletonIndex != INDEX_NONE
            ? RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonIndex)
            : FCompactPoseBoneIndex(INDEX_NONE);
        if (CompactPoseIndex.GetInt() != INDEX_NONE)
        {
            CachedBoneIndices.Add(CompactPoseIndex.GetInt());
            continue;
        }

        CachedBoneIndices.Add(INDEX_NONE);
        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Init] Bone unavailable for current skeleton/required bones: %s RefSkeletonIndex=%d"),
                *BoneReference.BoneName.ToString(),
                SkeletonIndex);
        }
    }

    bCachedBonesValid = CachedBoneIndices.Num() > 0;
    for (int32 BoneIndex : CachedBoneIndices)
    {
        if (BoneIndex != INDEX_NONE)
        {
            continue;
        }

        bCachedBonesValid = false;
        break;
    }

    const int32 AimSourceSkeletonIndex = ReferenceSkeleton.FindBoneIndex(AimSourceBoneName);
    const FCompactPoseBoneIndex AimSourceCompactPoseIndex = AimSourceSkeletonIndex != INDEX_NONE
        ? RequiredBones.GetCompactPoseIndexFromSkeletonIndex(AimSourceSkeletonIndex)
        : FCompactPoseBoneIndex(INDEX_NONE);
    AimSourceBoneIndex = AimSourceCompactPoseIndex.GetInt();
    if (AimSourceBoneIndex == INDEX_NONE)
    {
        bCachedBonesValid = false;

        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Init] AimSourceBone unavailable for current skeleton/required bones: %s RefSkeletonIndex=%d"),
                *AimSourceBoneName.ToString(),
                AimSourceSkeletonIndex);
        }
    }

    bAimSourceIsChainDescendant = HasValidAimSourceHierarchy(ReferenceSkeleton);
    if (!bAimSourceIsChainDescendant)
    {
        bCachedBonesValid = false;

        if (bEnableDebugLogging)
        {
            const FString ChainTipName = BoneChain.Num() > 0
                ? BoneChain.Last().BoneName.ToString()
                : TEXT("(empty chain)");
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Init] AimSourceBone '%s' is not the chain tip or its descendant. ChainTip='%s'."),
                *AimSourceBoneName.ToString(),
                *ChainTipName);
        }
    }

    if (bEnableDebugLogging)
    {
        UE_LOG(
            LogAnimation,
            Warning,
            TEXT("[AimIK][Init] Result: bCachedBonesValid=%s AimSourceRefSkeletonIndex=%d AimSourceCompactPoseIndex=%d bAimSourceIsChainDescendant=%s"),
            bCachedBonesValid ? TEXT("true") : TEXT("false"),
            AimSourceSkeletonIndex,
            AimSourceBoneIndex,
            bAimSourceIsChainDescendant ? TEXT("true") : TEXT("false"));
    }
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    Super::CacheBones_AnyThread(Context);
}

//------------------------------------------------------------------------------

bool FAnimNode_AimIK::IsValidToEvaluate(
    const USkeleton* Skeleton,
    const FBoneContainer& RequiredBones)
{
    return bCachedBonesValid
        && AimSourceBoneIndex != INDEX_NONE
        && bAimSourceIsChainDescendant;
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms)
{
    check(OutBoneTransforms.Num() == 0);

    if (!bCachedBonesValid)
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Eval] Early exit: invalid cached bones."));
        }

        return;
    }

    if (AimSourceBoneIndex == INDEX_NONE)
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Eval] Early exit: AimSourceBoneIndex is invalid."));
        }

        return;
    }

    if (!bAimSourceIsChainDescendant)
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Eval] Early exit: invalid aim source hierarchy."));
        }

        return;
    }

    if (MaxIterations <= 0)
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Eval] Early exit: MaxIterations=%d."),
                MaxIterations);
        }

        return;
    }

    if (!bHasValidAimTarget)
    {
        ResetInputPoseDiagnostics();

        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Eval] Early exit: AimTarget is invalid."));
        }

        return;
    }

    if (!AimSourceLocalTransform.IsValid())
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Eval] Early exit: AimSourceLocalTransform is invalid."));
        }

        return;
    }

    if (AimAxis.IsNearlyZero())
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][Eval] Early exit: AimAxis is zero."));
        }

        return;
    }

    SolveAimIK(Output, OutBoneTransforms);
}

//------------------------------------------------------------------------------

bool FAnimNode_AimIK::HasValidAimSourceHierarchy(const FReferenceSkeleton& ReferenceSkeleton) const
{
    if (BoneChain.Num() == 0 || AimSourceBoneName.IsNone())
    {
        return false;
    }

    const int32 AimSourceIndex = ReferenceSkeleton.FindBoneIndex(AimSourceBoneName);
    const int32 ChainTipIndex = ReferenceSkeleton.FindBoneIndex(BoneChain.Last().BoneName);

    return FAimIKBoneHierarchy::IsDescendantOrSelf(
        ReferenceSkeleton,
        AimSourceIndex,
        ChainTipIndex);
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::SolveAimIK(
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms)
{
    const int32 ChainCount = CachedBoneIndices.Num();
    if (ChainCount == 0)
    {
        return;
    }

    TArray<FTransform> ChainTransformsCS;
    ChainTransformsCS.Reserve(ChainCount);
    for (int32 BoneIndex : CachedBoneIndices)
    {
        ChainTransformsCS.Add(
            Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex)));
    }

    const FTransform AimSourceBoneTransformCS = Output.Pose.GetComponentSpaceTransform(
        FCompactPoseBoneIndex(AimSourceBoneIndex));
    FTransform AimTransformCS = AimSourceLocalTransform * AimSourceBoneTransformCS;
    if (bEnableMinTargetDistanceGuard
        && FVector::Dist(AimTransformCS.GetLocation(), AimTarget) <= MinTargetDistance)
    {
        return;
    }

    const FVector AimForwardCS = AimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
    if (AimForwardCS.IsNearlyZero())
    {
        return;
    }

    UpdateInputPoseDiagnostics(
        AimSourceBoneTransformCS,
        ChainTransformsCS,
        AimForwardCS,
        AimTransformCS.GetLocation());

    const bool bShouldLogSolve = ShouldLogSolve();
    if (bShouldLogSolve)
    {
        LogSolveInput(AimTransformCS, AimForwardCS);
    }

    FAimIKSolverInput SolverInput(BoneChain);
    SolverInput.AimAxis = AimAxis;
    SolverInput.AimTargetCS = AimTarget;
    SolverInput.PoleAxis = PoleAxis;
    SolverInput.PoleTargetCS = PoleTarget;
    SolverInput.PoleWeight = PoleWeight;
    SolverInput.ClampWeight = ClampWeight;
    SolverInput.ClampSmoothing = ClampSmoothing;
    SolverInput.MaxIterations = MaxIterations;
    SolverInput.Tolerance = Tolerance;

    const FVector EffectiveTargetCS = FAimIKSolver::Solve(
        SolverInput,
        ChainTransformsCS,
        AimTransformCS);
    if (bShouldLogSolve)
    {
        LogSolveOutput(AimTransformCS, EffectiveTargetCS);
    }

    OutBoneTransforms.Reserve(ChainCount);
    for (int32 ChainIndex = 0; ChainIndex < ChainCount; ++ChainIndex)
    {
        OutBoneTransforms.Emplace(
            FCompactPoseBoneIndex(CachedBoneIndices[ChainIndex]),
            ChainTransformsCS[ChainIndex]);
    }

    OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::ResetInputPoseDiagnostics()
{
    bHasPreviousInputPose = false;
    PreviousAimSourceBoneTransformCS = FTransform::Identity;
    PreviousChainTransformsCS.Reset();
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::UpdateInputPoseDiagnostics(
    const FTransform& AimSourceBoneTransformCS,
    const TArray<FTransform>& ChainTransformsCS,
    const FVector& AimForwardCS,
    const FVector& AimPositionCS)
{
    const float PositionDelta = FVector::Dist(
        PreviousAimSourceBoneTransformCS.GetLocation(),
        AimSourceBoneTransformCS.GetLocation());
    const float RotationDelta = FMath::RadiansToDegrees(
        PreviousAimSourceBoneTransformCS.GetRotation().AngularDistance(
            AimSourceBoneTransformCS.GetRotation()));
    const bool bInputPoseJumped = bHasPreviousInputPose
        && (PositionDelta > 30.0f || RotationDelta > 45.0f);
    if (bEnableDebugLogging && bInputPoseJumped)
    {
        const FVector TargetDirectionCS = (AimTarget - AimPositionCS).GetSafeNormal();
        const float TargetDirectionDot = FMath::Clamp(
            FVector::DotProduct(AimForwardCS, TargetDirectionCS),
            -1.0f,
            1.0f);
        const float TargetAngle = FMath::RadiansToDegrees(FMath::Acos(TargetDirectionDot));

        UE_LOG(
            LogAnimation,
            Warning,
            TEXT("[AimIK][PoseJump] SourceBone=%s PositionDelta=%.3f RotationDelta=%.3f TargetDistance=%.3f TargetAngle=%.3f"),
            *AimSourceBoneName.ToString(),
            PositionDelta,
            RotationDelta,
            FVector::Dist(AimPositionCS, AimTarget),
            TargetAngle);
        UE_LOG(
            LogAnimation,
            Warning,
            TEXT("[AimIK][PoseJump] PreviousSource Loc=%s Rot=%s CurrentSource Loc=%s Rot=%s"),
            *PreviousAimSourceBoneTransformCS.GetLocation().ToString(),
            *PreviousAimSourceBoneTransformCS.Rotator().ToString(),
            *AimSourceBoneTransformCS.GetLocation().ToString(),
            *AimSourceBoneTransformCS.Rotator().ToString());

        for (int32 ChainIndex = 0; ChainIndex < ChainTransformsCS.Num(); ++ChainIndex)
        {
            const FTransform& CurrentChainTransform = ChainTransformsCS[ChainIndex];
            const FTransform PreviousChainTransform = PreviousChainTransformsCS.IsValidIndex(ChainIndex)
                ? PreviousChainTransformsCS[ChainIndex]
                : FTransform::Identity;
            UE_LOG(
                LogAnimation,
                Warning,
                TEXT("[AimIK][PoseJump] ChainBone=%s PreviousLoc=%s PreviousRot=%s CurrentLoc=%s CurrentRot=%s"),
                *BoneChain[ChainIndex].BoneName.ToString(),
                *PreviousChainTransform.GetLocation().ToString(),
                *PreviousChainTransform.Rotator().ToString(),
                *CurrentChainTransform.GetLocation().ToString(),
                *CurrentChainTransform.Rotator().ToString());
        }
    }

    bHasPreviousInputPose = true;
    PreviousAimSourceBoneTransformCS = AimSourceBoneTransformCS;
    PreviousChainTransformsCS = ChainTransformsCS;
}

//------------------------------------------------------------------------------

bool FAnimNode_AimIK::ShouldLogSolve() const
{
    const uint64 DebugInterval = static_cast<uint64>(
        FMath::Max(DebugSolveLogInterval, 1));

    return bEnableDebugLogging
        && FPlatformTime::Cycles64() % DebugInterval == 0;
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::LogSolveInput(
    const FTransform& AimTransformCS,
    const FVector& AimForwardCS) const
{
    FString ChainDescription;
    for (const FAimIKBoneRef& BoneReference : BoneChain)
    {
        if (!ChainDescription.IsEmpty())
        {
            ChainDescription += TEXT(" -> ");
        }

        ChainDescription += FString::Printf(
            TEXT("%s(%.2f)"),
            *BoneReference.BoneName.ToString(),
            BoneReference.Weight);
    }

    UE_LOG(
        LogAnimation,
        Warning,
        TEXT("[AimIK] Chain=%s Alpha=%.3f"),
        *ChainDescription,
        ActualAlpha);
    UE_LOG(
        LogAnimation,
        Warning,
        TEXT("[AimIK] AimSourceBone=%s AimSourceLocalTransform Loc=%s Rot=%s"),
        *AimSourceBoneName.ToString(),
        *AimSourceLocalTransform.GetLocation().ToString(),
        *AimSourceLocalTransform.GetRotation().ToString());
    UE_LOG(
        LogAnimation,
        Warning,
        TEXT("[AimIK] CurrentAimTransform Loc=%s Rot=%s Forward=%s Target=%s"),
        *AimTransformCS.GetLocation().ToString(),
        *AimTransformCS.GetRotation().ToString(),
        *AimForwardCS.ToString(),
        *AimTarget.ToString());
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::LogSolveOutput(
    const FTransform& AimTransformCS,
    const FVector& EffectiveTargetCS) const
{
    const FVector FinalAimForwardCS = AimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
    const FVector FinalTargetDirectionCS = (
        EffectiveTargetCS - AimTransformCS.GetLocation()).GetSafeNormal();
    const float FinalDirectionDot = FMath::Clamp(
        FVector::DotProduct(FinalAimForwardCS, FinalTargetDirectionCS),
        -1.0f,
        1.0f);
    const float FinalAngleDegrees = FMath::RadiansToDegrees(
        FMath::Acos(FinalDirectionDot));

    UE_LOG(
        LogAnimation,
        Warning,
        TEXT("[AimIK] FinalAimTransform Loc=%s Forward=%s ResidualAngle=%.3f"),
        *AimTransformCS.GetLocation().ToString(),
        *FinalAimForwardCS.ToString(),
        FinalAngleDegrees);
}
