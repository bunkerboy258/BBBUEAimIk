#include "AnimNode_AimIK.h"

#include "Animation/AnimInstanceProxy.h"
#include "AnimationCoreLibrary.h"
#include "AnimationRuntime.h"

void FAnimNode_AimIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	CachedBoneIndices.Reset(BoneChain.Num());
	for (const FAimIKBoneRef& BoneRef : BoneChain)
	{
		const int32 SkeletonIndex = RequiredBones.GetPoseBoneIndexForBoneName(BoneRef.BoneName);
		if (SkeletonIndex != INDEX_NONE)
		{
			CachedBoneIndices.Add(RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonIndex).GetInt());
		}
		else
		{
			CachedBoneIndices.Add(INDEX_NONE);
		}
	}

	bCachedBonesValid = CachedBoneIndices.Num() > 0;
	for (int32 BoneIndex : CachedBoneIndices)
	{
		if (BoneIndex == INDEX_NONE)
		{
			bCachedBonesValid = false;
			break;
		}
	}

	const int32 AimSourceSkeletonIndex = RequiredBones.GetPoseBoneIndexForBoneName(AimSourceBoneName);
	if (AimSourceSkeletonIndex != INDEX_NONE)
	{
		AimSourceBoneIndex = RequiredBones.GetCompactPoseIndexFromSkeletonIndex(AimSourceSkeletonIndex).GetInt();
	}
	else
	{
		AimSourceBoneIndex = INDEX_NONE;
		bCachedBonesValid = false;
	}

	bAimSourceIsChainDescendant = false;
	if (bCachedBonesValid && AimSourceSkeletonIndex != INDEX_NONE)
	{
		const int32 ChainTipSkeletonIndex = RequiredBones.GetPoseBoneIndexForBoneName(BoneChain.Last().BoneName);
		if (ChainTipSkeletonIndex != INDEX_NONE)
		{
			int32 CurrentSkeletonIndex = AimSourceSkeletonIndex;
			const FReferenceSkeleton& ReferenceSkeleton = RequiredBones.GetSkeletonAsset()->GetReferenceSkeleton();
			while (CurrentSkeletonIndex != INDEX_NONE)
			{
				if (CurrentSkeletonIndex == ChainTipSkeletonIndex)
				{
					bAimSourceIsChainDescendant = true;
					break;
				}

				CurrentSkeletonIndex = ReferenceSkeleton.GetParentIndex(CurrentSkeletonIndex);
			}
		}
	}

	if (!bAimSourceIsChainDescendant)
	{
		bCachedBonesValid = false;
	}
}

void FAnimNode_AimIK::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Super::CacheBones_AnyThread(Context);
}

bool FAnimNode_AimIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return bCachedBonesValid && AimSourceBoneIndex != INDEX_NONE && bAimSourceIsChainDescendant;
}

void FAnimNode_AimIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	if (!bCachedBonesValid ||
		AimSourceBoneIndex == INDEX_NONE ||
		!bAimSourceIsChainDescendant ||
		MaxIterations <= 0 ||
		!bHasValidAimTarget ||
		!AimSourceLocalTransform.IsValid() ||
		AimAxis.IsNearlyZero())
	{
		return;
	}

	SolveAimIK(Output, OutBoneTransforms);
}

void FAnimNode_AimIK::SolveAimIK(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
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
		ChainTransformsCS.Add(Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex)));
	}

	const FTransform AimSourceBoneCS = Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(AimSourceBoneIndex));
	FTransform CurrentAimTransformCS = AimSourceLocalTransform * AimSourceBoneCS;

	const FVector InitialAimPosCS = CurrentAimTransformCS.GetLocation();
	const FVector InitialAimForwardCS = CurrentAimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
	if (InitialAimForwardCS.IsNearlyZero())
	{
		return;
	}

	const bool bShouldLogSolve = bEnableDebugLogging && ((FPlatformTime::Cycles64() % 60) == 0);
	if (bShouldLogSolve)
	{
		FString ChainDescription;
		for (int32 BoneIdx = 0; BoneIdx < BoneChain.Num(); ++BoneIdx)
		{
			if (!ChainDescription.IsEmpty())
			{
				ChainDescription += TEXT(" -> ");
			}
			ChainDescription += FString::Printf(TEXT("%s(%.2f)"), *BoneChain[BoneIdx].BoneName.ToString(), BoneChain[BoneIdx].Weight);
		}

		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] Chain=%s, Alpha=%.3f"),
			*ChainDescription,
			ActualAlpha);
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] AimSourceBone=%s, AimSourceLocalTransform: Loc=%s, Rot=%s"),
			*AimSourceBoneName.ToString(),
			*AimSourceLocalTransform.GetLocation().ToString(),
			*AimSourceLocalTransform.GetRotation().ToString());
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] CurrentAimTransform: Loc=%s, Rot=%s"),
			*CurrentAimTransformCS.GetLocation().ToString(),
			*CurrentAimTransformCS.GetRotation().ToString());
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] AimForwardCS: %s"), *InitialAimForwardCS.ToString());
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] AimTarget: %s"), *AimTarget.ToString());
	}

	const FVector FirstBonePosCS = ChainTransformsCS[0].GetLocation();

	FVector EffectiveTargetCS = AimTarget;
	if (ChainCount >= 2)
	{
		const FVector SingularityOffset = GetSingularityOffset(FirstBonePosCS, InitialAimPosCS, AimTarget);
		if (!SingularityOffset.IsNearlyZero())
		{
			EffectiveTargetCS += SingularityOffset;
		}
	}

	const FVector ClampedTargetCS = GetClampedTargetCS(InitialAimPosCS, InitialAimForwardCS, EffectiveTargetCS);
	const float Step = 1.0f / static_cast<float>(ChainCount);
	const int32 Iterations = FMath::Clamp(MaxIterations, 1, 20);

	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		for (int32 ChainIndex = 0; ChainIndex < ChainCount; ++ChainIndex)
		{
			const float BoneWeightMultiplier = (ChainIndex < ChainCount - 1)
				? Step * (ChainIndex + 1) * BoneChain[ChainIndex].Weight
				: BoneChain[ChainIndex].Weight;
			const float Weight = FMath::Clamp(BoneWeightMultiplier, 0.0f, 1.0f);

			if (Weight > KINDA_SMALL_NUMBER)
			{
				RotateBoneToTarget(ChainIndex, ClampedTargetCS, Weight, ChainTransformsCS, CurrentAimTransformCS);
			}
		}

		if (Iter >= 1 && Tolerance > KINDA_SMALL_NUMBER)
		{
			const FVector CurrentAimForwardCS = CurrentAimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
			const FVector ToTargetDir = (ClampedTargetCS - CurrentAimTransformCS.GetLocation()).GetSafeNormal();
			if (!ToTargetDir.IsNearlyZero())
			{
				const float Dot = FMath::Clamp(FVector::DotProduct(CurrentAimForwardCS, ToTargetDir), -1.0f, 1.0f);
				const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
				if (AngleDeg < Tolerance)
				{
					break;
				}
			}
		}
	}

	if (bShouldLogSolve)
	{
		const FVector FinalAimForwardCS = CurrentAimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
		const FVector FinalToTargetDir = (ClampedTargetCS - CurrentAimTransformCS.GetLocation()).GetSafeNormal();
		const float FinalDot = FMath::Clamp(FVector::DotProduct(FinalAimForwardCS, FinalToTargetDir), -1.0f, 1.0f);
		const float FinalAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FinalDot));
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] FinalAimTransform: Loc=%s, Forward=%s, ResidualAngle=%.3f"),
			*CurrentAimTransformCS.GetLocation().ToString(),
			*FinalAimForwardCS.ToString(),
			FinalAngleDeg);
	}

	OutBoneTransforms.Reserve(ChainCount);
	for (int32 ChainIndex = 0; ChainIndex < ChainCount; ++ChainIndex)
	{
		OutBoneTransforms.Emplace(FCompactPoseBoneIndex(CachedBoneIndices[ChainIndex]), ChainTransformsCS[ChainIndex]);
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

FVector FAnimNode_AimIK::GetSingularityOffset(const FVector& FirstBonePosCS, const FVector& AimPosCS, const FVector& TargetPosCS) const
{
	const FVector ToAim = AimPosCS - FirstBonePosCS;
	const FVector ToTarget = TargetPosCS - FirstBonePosCS;
	const float DistAim = ToAim.Size();
	const float DistTarget = ToTarget.Size();

	if (DistAim < KINDA_SMALL_NUMBER || DistTarget < KINDA_SMALL_NUMBER || DistTarget > DistAim)
	{
		return FVector::ZeroVector;
	}

	const float Dot = FVector::DotProduct(ToAim / DistAim, ToTarget / DistTarget);
	if (Dot < 0.999f)
	{
		return FVector::ZeroVector;
	}

	const FVector IKDirection = ToTarget.GetSafeNormal();
	const FVector SecondaryDir(IKDirection.Y, IKDirection.Z, IKDirection.X);
	const FVector OffsetDir = FVector::CrossProduct(IKDirection, SecondaryDir);
	return OffsetDir.GetSafeNormal() * (DistAim * 0.05f);
}

FVector FAnimNode_AimIK::GetClampedTargetCS(const FVector& AimBonePosCS, const FVector& AimBoneForwardCS, const FVector& TargetCS) const
{
	if (ClampWeight <= KINDA_SMALL_NUMBER)
	{
		return TargetCS;
	}
	if (ClampWeight >= 1.0f - KINDA_SMALL_NUMBER)
	{
		const float Dist = FVector::Dist(AimBonePosCS, TargetCS);
		return AimBonePosCS + AimBoneForwardCS * Dist;
	}

	const FVector ToTarget = TargetCS - AimBonePosCS;
	const float TargetDist = ToTarget.Size();
	if (TargetDist < KINDA_SMALL_NUMBER)
	{
		return TargetCS;
	}

	const FVector ToTargetDir = ToTarget / TargetDist;
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(AimBoneForwardCS, ToTargetDir), -1.0f, 1.0f)));
	const float NormalizedAngle = 1.0f - (AngleDeg / 180.0f);
	const float OneMinusNormalizedAngle = 1.0f - NormalizedAngle;
	if (OneMinusNormalizedAngle <= KINDA_SMALL_NUMBER)
	{
		return TargetCS;
	}

	float TargetClampMlp = 1.0f;
	if (ClampWeight > KINDA_SMALL_NUMBER)
	{
		TargetClampMlp = FMath::Clamp(1.0f - ((ClampWeight - NormalizedAngle) / OneMinusNormalizedAngle), 0.0f, 1.0f);
	}

	float ClampMlp = 1.0f;
	if (ClampWeight > KINDA_SMALL_NUMBER)
	{
		ClampMlp = FMath::Clamp(NormalizedAngle / ClampWeight, 0.0f, 1.0f);
	}

	for (int32 Index = 0; Index < ClampSmoothing; ++Index)
	{
		ClampMlp = FMath::Sin(ClampMlp * UE_PI * 0.5f);
	}

	const FQuat RotQuat = FQuat::FindBetweenNormals(AimBoneForwardCS, ToTargetDir);
	const FQuat SlerpedQuat = FQuat::Slerp(FQuat::Identity, RotQuat, ClampMlp * TargetClampMlp);
	const FVector SlerpedDir = SlerpedQuat.RotateVector(AimBoneForwardCS);
	return AimBonePosCS + SlerpedDir * TargetDist;
}

void FAnimNode_AimIK::RotateBoneToTarget(
	int32 ChainIndex,
	const FVector& TargetPosCS,
	float Weight,
	TArray<FTransform>& InOutChainTransforms,
	FTransform& InOutAimTransformCS)
{
	FTransform& BoneCS = InOutChainTransforms[ChainIndex];
	const FVector CurrentAimPosCS = InOutAimTransformCS.GetLocation();
	const FVector CurrentAimForwardCS = InOutAimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
	if (CurrentAimForwardCS.IsNearlyZero())
	{
		return;
	}

	if (bEnableDebugLogging && ((FPlatformTime::Cycles64() % 60) == 0))
	{
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] RotateBoneToTarget CurrentAimPosCS=%s CurrentAimForwardCS=%s TargetPosCS=%s Weight=%.3f"),
			*CurrentAimPosCS.ToString(),
			*CurrentAimForwardCS.ToString(),
			*TargetPosCS.ToString(),
			Weight);
	}

	const FVector ToTargetDir = (TargetPosCS - CurrentAimPosCS).GetSafeNormal();
	if (ToTargetDir.IsNearlyZero())
	{
		return;
	}

	const FQuat SwingRot = FQuat::FindBetweenNormals(CurrentAimForwardCS, ToTargetDir);
	const FQuat AppliedSwingRot = Weight >= 1.0f - KINDA_SMALL_NUMBER
		? SwingRot
		: FQuat::Slerp(FQuat::Identity, SwingRot, Weight);

	FQuat AppliedPoleRot = FQuat::Identity;
	if (PoleWeight > KINDA_SMALL_NUMBER)
	{
		const FVector CurrentAimPoleAxis = InOutAimTransformCS.TransformVectorNoScale(PoleAxis).GetSafeNormal();
		const FVector PoleDir = PoleTarget - CurrentAimPosCS;
		const FVector PoleDirOrtho = (PoleDir - CurrentAimForwardCS * FVector::DotProduct(PoleDir, CurrentAimForwardCS)).GetSafeNormal();

		if (!CurrentAimPoleAxis.IsNearlyZero() && !PoleDirOrtho.IsNearlyZero())
		{
			const FQuat PoleRot = FQuat::FindBetweenNormals(CurrentAimPoleAxis, PoleDirOrtho);
			AppliedPoleRot = FQuat::Slerp(FQuat::Identity, PoleRot, Weight * PoleWeight);
		}
	}

	const FQuat TotalRot = AppliedPoleRot * AppliedSwingRot;
	if (TotalRot.IsIdentity())
	{
		return;
	}

	const FVector BonePos = BoneCS.GetLocation();
	for (int32 DownstreamIndex = ChainIndex; DownstreamIndex < InOutChainTransforms.Num(); ++DownstreamIndex)
	{
		FTransform& DownstreamTransform = InOutChainTransforms[DownstreamIndex];
		DownstreamTransform.SetLocation(BonePos + TotalRot.RotateVector(DownstreamTransform.GetLocation() - BonePos));
		DownstreamTransform.SetRotation(TotalRot * DownstreamTransform.GetRotation());
	}

	InOutAimTransformCS.SetLocation(BonePos + TotalRot.RotateVector(InOutAimTransformCS.GetLocation() - BonePos));
	InOutAimTransformCS.SetRotation(TotalRot * InOutAimTransformCS.GetRotation());
}
