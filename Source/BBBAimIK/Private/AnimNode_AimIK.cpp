#include "AnimNode_AimIK.h"

#include "Animation/AnimInstanceProxy.h"
#include "AnimationCoreLibrary.h"
#include "AnimationRuntime.h"

//------------------------------------------------------------------------------

void FAnimNode_AimIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	bHasPreviousInputPose = false;
	PreviousAimSourceBoneTransformCS = FTransform::Identity;
	PreviousChainTransformsCS.Reset();

	const USkeleton* SkeletonAsset = RequiredBones.GetSkeletonAsset();
	if (!SkeletonAsset)
	{
		CachedBoneIndices.Reset();
		AimSourceBoneIndex = INDEX_NONE;
		bCachedBonesValid = false;
		bAimSourceIsChainDescendant = false;
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Init] Invalid skeleton asset."));}
		return;
	}

	const FReferenceSkeleton& ReferenceSkeleton = SkeletonAsset->GetReferenceSkeleton();
	CachedBoneIndices.Reset(BoneChain.Num());
	for (const FAimIKBoneRef& BoneRef : BoneChain)
	{
		const int32 SkeletonIndex = ReferenceSkeleton.FindBoneIndex(BoneRef.BoneName);
		const FCompactPoseBoneIndex CompactPoseIndex = SkeletonIndex != INDEX_NONE
			? RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonIndex)
			: FCompactPoseBoneIndex(INDEX_NONE);
		if (CompactPoseIndex.GetInt() != INDEX_NONE)
		{CachedBoneIndices.Add(CompactPoseIndex.GetInt());}
		else
		{
			CachedBoneIndices.Add(INDEX_NONE);
			if (bEnableDebugLogging)
			{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Init] Bone unavailable for current skeleton/required bones: %s RefSkeletonIndex=%d"), *BoneRef.BoneName.ToString(), SkeletonIndex);}
		}
	}

	// 存在任何无效骨骼时整体判定为不可用
	bCachedBonesValid = CachedBoneIndices.Num() > 0;
	for (int32 BoneIndex : CachedBoneIndices)
	{
		if (BoneIndex == INDEX_NONE)
		{
			bCachedBonesValid = false;
			break;
		}
	}

	const int32 AimSourceSkeletonIndex = ReferenceSkeleton.FindBoneIndex(AimSourceBoneName);
	const FCompactPoseBoneIndex AimSourceCompactPoseIndex = AimSourceSkeletonIndex != INDEX_NONE
		? RequiredBones.GetCompactPoseIndexFromSkeletonIndex(AimSourceSkeletonIndex)
		: FCompactPoseBoneIndex(INDEX_NONE);
	if (AimSourceCompactPoseIndex.GetInt() != INDEX_NONE)
	{AimSourceBoneIndex = AimSourceCompactPoseIndex.GetInt();}
	else
	{
		AimSourceBoneIndex = INDEX_NONE;
		bCachedBonesValid = false;
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Init] AimSourceBone unavailable for current skeleton/required bones: %s RefSkeletonIndex=%d"), *AimSourceBoneName.ToString(), AimSourceSkeletonIndex);}
	}

	// 沿父链向上回溯，确认瞄准源骨骼挂在链尖端之下，否则旋转链条无法带动瞄准源
	bAimSourceIsChainDescendant = false;
	if (bCachedBonesValid && AimSourceSkeletonIndex != INDEX_NONE)
	{
		const int32 ChainTipSkeletonIndex = ReferenceSkeleton.FindBoneIndex(BoneChain.Last().BoneName);
		if (ChainTipSkeletonIndex != INDEX_NONE)
		{
			int32 CurrentSkeletonIndex = AimSourceSkeletonIndex;
			if (bEnableDebugLogging)
			{
				FString ParentChain;
				int32 TraceIdx = AimSourceSkeletonIndex;
				while (TraceIdx != INDEX_NONE)
				{
					FName TraceName = ReferenceSkeleton.GetBoneName(TraceIdx);
					if (!ParentChain.IsEmpty()) {ParentChain += TEXT(" -> ");}
					ParentChain += FString::Printf(TEXT("%s[%d]"), *TraceName.ToString(), TraceIdx);
					TraceIdx = ReferenceSkeleton.GetParentIndex(TraceIdx);
				}
				UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Init] AimSourceBone '%s' parent chain: %s"), *AimSourceBoneName.ToString(), *ParentChain);
				UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Init] ChainTip '%s' RefSkeletonIndex=%d"), *BoneChain.Last().BoneName.ToString(), ChainTipSkeletonIndex);
			}
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
		if (bEnableDebugLogging)
		{
			UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Init] AimSourceBone '%s' is NOT a descendant of ChainTip '%s'. Skeleton mismatch or wrong chain order."),
				*AimSourceBoneName.ToString(),
				BoneChain.Num() > 0 ? *BoneChain.Last().BoneName.ToString() : TEXT("(empty chain)"));
		}
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Init] Result: bCachedBonesValid=%s, AimSourceRefSkeletonIndex=%d, AimSourceCompactPoseIndex=%d, bAimSourceIsChainDescendant=%s"),
			bCachedBonesValid ? TEXT("true") : TEXT("false"),
			AimSourceSkeletonIndex,
			AimSourceBoneIndex,
			bAimSourceIsChainDescendant ? TEXT("true") : TEXT("false"));
	}
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{Super::CacheBones_AnyThread(Context);}

//------------------------------------------------------------------------------

bool FAnimNode_AimIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{return bCachedBonesValid && AimSourceBoneIndex != INDEX_NONE && bAimSourceIsChainDescendant;}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	if (!bCachedBonesValid)
	{
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Eval] Early exit: bCachedBonesValid=false (check BoneChain names & AimSourceBoneName)"));}
		return;
	}
	if (AimSourceBoneIndex == INDEX_NONE)
	{
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Eval] Early exit: AimSourceBoneIndex=INDEX_NONE"));}
		return;
	}
	if (!bAimSourceIsChainDescendant)
	{
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Eval] Early exit: bAimSourceIsChainDescendant=false"));}
		return;
	}
	if (MaxIterations <= 0)
	{
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Eval] Early exit: MaxIterations=%d"), MaxIterations);}
		return;
	}
	if (!bHasValidAimTarget)
	{
		bHasPreviousInputPose = false;
		PreviousChainTransformsCS.Reset();

		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Eval] Early exit: bHasValidAimTarget=false"));}
		return;
	}
	if (!AimSourceLocalTransform.IsValid())
	{
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Eval] Early exit: AimSourceLocalTransform invalid"));}
		return;
	}
	if (AimAxis.IsNearlyZero())
	{
		if (bEnableDebugLogging)
		{UE_LOG(LogAnimation, Warning, TEXT("[AimIK][Eval] Early exit: AimAxis is zero"));}
		return;
	}
	SolveAimIK(Output, OutBoneTransforms);
}

//------------------------------------------------------------------------------

void FAnimNode_AimIK::SolveAimIK(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const int32 ChainCount = CachedBoneIndices.Num();
	if (ChainCount == 0) 
	{return;}

	// 缓存各链节在当前姿态下的组件空间变换，后续全部基于该快照迭代
	TArray<FTransform> ChainTransformsCS;
	ChainTransformsCS.Reserve(ChainCount);
	for (int32 BoneIndex : CachedBoneIndices)
	{
		ChainTransformsCS.Add(Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex)));
	}

	// 由骨骼姿态与稳定局部绑定重建瞄准源，不依赖外部每帧回传
	const FTransform AimSourceBoneCS = Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(AimSourceBoneIndex));
	FTransform CurrentAimTransformCS = AimSourceLocalTransform * AimSourceBoneCS;

	const FVector InitialAimPosCS = CurrentAimTransformCS.GetLocation();
	if (bEnableMinTargetDistanceGuard && FVector::Dist(InitialAimPosCS, AimTarget) <= MinTargetDistance)
	{return;}

	const FVector InitialAimForwardCS = CurrentAimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
	if (InitialAimForwardCS.IsNearlyZero())
	{return;}

	const float InputPositionDelta = FVector::Dist(
		PreviousAimSourceBoneTransformCS.GetLocation(),
		AimSourceBoneCS.GetLocation());
	const float InputRotationDelta = FMath::RadiansToDegrees(
		PreviousAimSourceBoneTransformCS.GetRotation().AngularDistance(AimSourceBoneCS.GetRotation()));
	const bool bInputPoseJumped = bHasPreviousInputPose
		&& (InputPositionDelta > 30.0f || InputRotationDelta > 45.0f);
	if (bEnableDebugLogging && bInputPoseJumped)
	{
		const FVector TargetDirectionCS = (AimTarget - InitialAimPosCS).GetSafeNormal();
		const float TargetAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(InitialAimForwardCS, TargetDirectionCS),
			-1.0f,
			1.0f)));

		UE_LOG(
			LogAnimation,
			Warning,
			TEXT("[AimIK][PoseJump] SourceBone=%s PositionDelta=%.3f RotationDelta=%.3f TargetDistance=%.3f TargetAngle=%.3f"),
			*AimSourceBoneName.ToString(),
			InputPositionDelta,
			InputRotationDelta,
			FVector::Dist(InitialAimPosCS, AimTarget),
			TargetAngle);
		UE_LOG(
			LogAnimation,
			Warning,
			TEXT("[AimIK][PoseJump] PreviousSource Loc=%s Rot=%s CurrentSource Loc=%s Rot=%s"),
			*PreviousAimSourceBoneTransformCS.GetLocation().ToString(),
			*PreviousAimSourceBoneTransformCS.Rotator().ToString(),
			*AimSourceBoneCS.GetLocation().ToString(),
			*AimSourceBoneCS.Rotator().ToString());

		for (int32 ChainIndex = 0; ChainIndex < ChainTransformsCS.Num(); ++ChainIndex)
		{
			const FTransform &CurrentChainTransform = ChainTransformsCS[ChainIndex];
			const bool bHasPreviousChainTransform = PreviousChainTransformsCS.IsValidIndex(ChainIndex);
			const FTransform PreviousChainTransform = bHasPreviousChainTransform
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
	PreviousAimSourceBoneTransformCS = AimSourceBoneCS;
	PreviousChainTransformsCS = ChainTransformsCS;

	// 按周期间隔采样输出调试日志，避免逐帧刷屏
	const uint64 DebugInterval = static_cast<uint64>(FMath::Max(DebugSolveLogInterval, 1));
	const bool bShouldLogSolve = bEnableDebugLogging && ((FPlatformTime::Cycles64() % DebugInterval) == 0);
	if (bShouldLogSolve)
	{
		FString ChainDescription;
		for (int32 BoneIdx = 0; BoneIdx < BoneChain.Num(); ++BoneIdx)
		{
			if (!ChainDescription.IsEmpty())
			{ChainDescription += TEXT(" -> ");}
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
		// 目标与链接近共线时施加法向微移，规避线性奇点导致的旋转不稳定
		const FVector SingularityOffset = GetSingularityOffset(FirstBonePosCS, InitialAimPosCS, AimTarget);
		if (!SingularityOffset.IsNearlyZero())
		{
			EffectiveTargetCS += SingularityOffset;
		}
	}

	// 钳制目标方向，防止躯干或手臂出现反关节式过度弯折
	const FVector ClampedTargetCS = GetClampedTargetCS(InitialAimPosCS, InitialAimForwardCS, EffectiveTargetCS);
	const float Step = 1.0f / static_cast<float>(ChainCount);
	const int32 Iterations = FMath::Clamp(MaxIterations, 1, 20);

	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		for (int32 ChainIndex = 0; ChainIndex < ChainCount; ++ChainIndex)
		{
			// 非尖端链节按离根距离递增分摊权重，尖端链节使用完整权重以保证末端瞄准精度
			const float BoneWeightMultiplier = (ChainIndex < ChainCount - 1)
				? Step * (ChainIndex + 1) * BoneChain[ChainIndex].Weight
				: BoneChain[ChainIndex].Weight;
			const float Weight = FMath::Clamp(BoneWeightMultiplier, 0.0f, 1.0f);

			if (Weight > KINDA_SMALL_NUMBER)
			{
				RotateBoneToTarget(ChainIndex, ClampedTargetCS, Weight, ChainTransformsCS, CurrentAimTransformCS);
			}
		}

		// 首轮迭代后按角度误差判断是否提前收敛
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
	// UE 要求输出按骨骼索引排序后再应用回姿态
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

//------------------------------------------------------------------------------

FVector FAnimNode_AimIK::GetSingularityOffset(const FVector& FirstBonePosCS, const FVector& AimPosCS, const FVector& TargetPosCS) const
{
	const FVector ToAim = AimPosCS - FirstBonePosCS;
	const FVector ToTarget = TargetPosCS - FirstBonePosCS;

	const float DistAim = ToAim.Size();
	const float DistTarget = ToTarget.Size();

	// 距离过近无法判定共线，或目标已超出链伸展范围时无需偏移
	if (DistAim < KINDA_SMALL_NUMBER || DistTarget < KINDA_SMALL_NUMBER || DistTarget > DistAim)
	{
		return FVector::ZeroVector;
	}

	// 点积即两方向夹角余弦，接近 1 表示链根、瞄准源与目标几乎共线
	const float Dot = FVector::DotProduct(ToAim / DistAim, ToTarget / DistTarget);
	if (Dot < 0.999f)
	{return FVector::ZeroVector;}

	const FVector IKDirection = ToTarget.GetSafeNormal();

	// 错位分量构造与目标方向不共线的辅助向量，作为叉乘基准
	const FVector SecondaryDir(IKDirection.Y, IKDirection.Z, IKDirection.X);

	// 叉乘得到同时垂直于两者的侧向法向，作为偏移方向
	const FVector OffsetDir = FVector::CrossProduct(IKDirection, SecondaryDir);

	// 偏移量取链长的 5%，仅做轻微扰动
	return OffsetDir.GetSafeNormal() * (DistAim * 0.05f);
}

//------------------------------------------------------------------------------

FVector FAnimNode_AimIK::GetClampedTargetCS(const FVector& AimBonePosCS, const FVector& AimBoneForwardCS, const FVector& TargetCS) const
{
	if (ClampWeight <= KINDA_SMALL_NUMBER)
	{return TargetCS;}
	if (ClampWeight >= 1.0f - KINDA_SMALL_NUMBER)
	{
		const float Dist = FVector::Dist(AimBonePosCS, TargetCS);
		return AimBonePosCS + AimBoneForwardCS * Dist;
	}

	const FVector ToTarget = TargetCS - AimBonePosCS;
	const float TargetDist = ToTarget.Size();
	if (TargetDist < KINDA_SMALL_NUMBER)
	{return TargetCS;}

	// 将目标方向与瞄准前向的夹角归一化到 [0,1]，0 为正前方，1 为正后方
	const FVector ToTargetDir = ToTarget / TargetDist;
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(AimBoneForwardCS, ToTargetDir), -1.0f, 1.0f)));
	const float NormalizedAngle = 1.0f - (AngleDeg / 180.0f);
	const float OneMinusNormalizedAngle = 1.0f - NormalizedAngle;
	if (OneMinusNormalizedAngle <= KINDA_SMALL_NUMBER)
	{return TargetCS;}

	float TargetClampMlp = 1.0f;
	if (ClampWeight > KINDA_SMALL_NUMBER)
	{TargetClampMlp = FMath::Clamp(1.0f - ((ClampWeight - NormalizedAngle) / OneMinusNormalizedAngle), 0.0f, 1.0f);}
	float ClampMlp = 1.0f;
	if (ClampWeight > KINDA_SMALL_NUMBER)
	{ClampMlp = FMath::Clamp(NormalizedAngle / ClampWeight, 0.0f, 1.0f);}

	// 用半正弦曲线多次平滑钳制系数，让过渡更柔和
	for (int32 Index = 0; Index < ClampSmoothing; ++Index)
	{ClampMlp = FMath::Sin(ClampMlp * UE_PI * 0.5f);}

	const FQuat RotQuat = FQuat::FindBetweenNormals(AimBoneForwardCS, ToTargetDir);
	const FQuat SlerpedQuat = FQuat::Slerp(FQuat::Identity, RotQuat, ClampMlp * TargetClampMlp);
	const FVector SlerpedDir = SlerpedQuat.RotateVector(AimBoneForwardCS);
	return AimBonePosCS + SlerpedDir * TargetDist;
}

//------------------------------------------------------------------------------

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
	{return;}

	const uint64 DebugInterval = static_cast<uint64>(FMath::Max(DebugSolveLogInterval, 1));
	if (bEnableDebugLogging && ((FPlatformTime::Cycles64() % DebugInterval) == 0))
	{
		UE_LOG(LogAnimation, Warning, TEXT("[AimIK] RotateBoneToTarget CurrentAimPosCS=%s CurrentAimForwardCS=%s TargetPosCS=%s Weight=%.3f"),
			*CurrentAimPosCS.ToString(),
			*CurrentAimForwardCS.ToString(),
			*TargetPosCS.ToString(),
			Weight);
	}

	const FVector ToTargetDir = (TargetPosCS - CurrentAimPosCS).GetSafeNormal();
	if (ToTargetDir.IsNearlyZero())
	{return;}

	// 摆动旋转按权重插值，权重越低单次修正越小
	const FQuat SwingRot = FQuat::FindBetweenNormals(CurrentAimForwardCS, ToTargetDir);
	const FQuat AppliedSwingRot = Weight >= 1.0f - KINDA_SMALL_NUMBER
		? SwingRot
		: FQuat::Slerp(FQuat::Identity, SwingRot, Weight);

	// 极轴纠偏：将极轴拉向极目标在垂直于瞄准前向平面上的投影，防止身体翻转
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
	{return;}

	// 以当前骨骼位置为支点，将旋转增量传播到所有下游链节
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
