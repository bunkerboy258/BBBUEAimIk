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
	// 获取 IK 骨骼链上的骨骼节点数量
	const int32 ChainCount = CachedBoneIndices.Num();
	if (ChainCount == 0) 
	{
		return;
	}

	// 提前准备一个数组，存各骨骼的组件空间Transform
	TArray<FTransform> ChainTransformsCS;
	ChainTransformsCS.Reserve(ChainCount);
	for (int32 BoneIndex : CachedBoneIndices)
	{
		// 从当前姿态(Pose)中读取组件空间的 Transform 并记录
		ChainTransformsCS.Add(Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex)));
	}

	// 提取瞄准源参考骨骼 (AimSourceBone) 的组件空间 Transform
	const FTransform AimSourceBoneCS = Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(AimSourceBoneIndex));
	// 结合局部偏移，计算真实的当前瞄准基准点，完全基于初始姿态 (Pose-native) 重建
	FTransform CurrentAimTransformCS = AimSourceLocalTransform * AimSourceBoneCS;

	// 获取初始的瞄准位置
	const FVector InitialAimPosCS = CurrentAimTransformCS.GetLocation();
	if (bEnableMinTargetDistanceGuard && FVector::Dist(InitialAimPosCS, AimTarget) <= MinTargetDistance)
	{
		return;
	}

	// 提取瞄准前向轴（根据 AimAxis 指定），并转换为组件空间的方向向量
	const FVector InitialAimForwardCS = CurrentAimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
	if (InitialAimForwardCS.IsNearlyZero()) // 如果方向异常（接近零向量），则取消计算
	{
		return;
	}

	// 通过时间和周期判断是否需要输出调试日志，避免日志刷屏
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

	// IK骨链第一根骨骼（根侧起点）的位置，用于判断目标是否过近或是否拉成直线（奇点）
	const FVector FirstBonePosCS = ChainTransformsCS[0].GetLocation();

	// 以外界传入的 AimTarget 作为基础目标点
	FVector EffectiveTargetCS = AimTarget;
	if (ChainCount >= 2)
	{
		// 若骨链超过两个节点，尝试计算奇点偏移 (防止骨骼刚好在一条直线上导致打死结)
		const FVector SingularityOffset = GetSingularityOffset(FirstBonePosCS, InitialAimPosCS, AimTarget);
		if (!SingularityOffset.IsNearlyZero())
		{
			// 如果触及奇点情况，给目标点施加一个法向微调
			EffectiveTargetCS += SingularityOffset;
		}
	}

	// 对目标点进行钳制修正，防止躯干或手腕向后对折等不符合人体解剖学的过度弯曲
	const FVector ClampedTargetCS = GetClampedTargetCS(InitialAimPosCS, InitialAimForwardCS, EffectiveTargetCS);
	// CCD (循环坐标下降法) 中计算骨链权重的递增步长分配
	const float Step = 1.0f / static_cast<float>(ChainCount);
	// 限制迭代次数在 [1, 20] 之间，防止死循环或过度降低性能
	const int32 Iterations = FMath::Clamp(MaxIterations, 1, 20);

	// 开启 CCD 多轮迭代逼近
	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		// 沿骨链逐一向目标求旋转解
		for (int32 ChainIndex = 0; ChainIndex < ChainCount; ++ChainIndex)
		{
			// 计算当前受算骨骼的权重分布：离根越近分摊得越少（利用 Step），以此提升末端瞄准精度
			const float BoneWeightMultiplier = (ChainIndex < ChainCount - 1)
				? Step * (ChainIndex + 1) * BoneChain[ChainIndex].Weight
				: BoneChain[ChainIndex].Weight;
			// 钳制在 [0, 1] 区间
			const float Weight = FMath::Clamp(BoneWeightMultiplier, 0.0f, 1.0f);

			if (Weight > KINDA_SMALL_NUMBER)
			{
				// 执行核心旋转：向目标靠拢，并把带来的位移/旋转递归作用于下游所有节点与 CurrentAimTransformCS
				RotateBoneToTarget(ChainIndex, ClampedTargetCS, Weight, ChainTransformsCS, CurrentAimTransformCS);
			}
		}

		// 如果不是首轮迭代，且设定了误差容忍度 (Tolerance) 参数
		if (Iter >= 1 && Tolerance > KINDA_SMALL_NUMBER)
		{
			// 检测当前经过本轮修正后的最新瞄准方向
			const FVector CurrentAimForwardCS = CurrentAimTransformCS.TransformVectorNoScale(AimAxis).GetSafeNormal();
			// 指向所需目标的理想方向向量
			const FVector ToTargetDir = (ClampedTargetCS - CurrentAimTransformCS.GetLocation()).GetSafeNormal();
			if (!ToTargetDir.IsNearlyZero())
			{
				// 计算当前前向与理想目标的误差角度 (从点积转回角度)
				const float Dot = FMath::Clamp(FVector::DotProduct(CurrentAimForwardCS, ToTargetDir), -1.0f, 1.0f);
				const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
				if (AngleDeg < Tolerance)
				{
					// 误差落入容忍范围内，提前结束所有迭代
					break;
				}
			}
		}
	}

	// （如果有需要）记录最终运算结果的日志
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

	// 确保输出数组预留足够的空间
	OutBoneTransforms.Reserve(ChainCount);
	for (int32 ChainIndex = 0; ChainIndex < ChainCount; ++ChainIndex)
	{
		// 将链上每段变换转为紧凑骨骼索引对应的更新结果
		OutBoneTransforms.Emplace(FCompactPoseBoneIndex(CachedBoneIndices[ChainIndex]), ChainTransformsCS[ChainIndex]);
	}
	// 按 UE 的规范要求对将要应用到 Pose 上的骨骼进行排序
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

FVector FAnimNode_AimIK::GetSingularityOffset(const FVector& FirstBonePosCS, const FVector& AimPosCS, const FVector& TargetPosCS) const
{
	// C++语法：const FVector& 表示"常引用"，直接读取外部传入的向量数据，不发生内存拷贝，也不允许被修改（保证安全和性能）。

	// C++运算符重载：FVector 类重载了减号（-），这里表示两个三维向量相减，计算从起始点到终点的方向向量。
	const FVector ToAim = AimPosCS - FirstBonePosCS;
	const FVector ToTarget = TargetPosCS - FirstBonePosCS;

	// UE专属API：.Size() 是 FVector 的成员函数，用于计算该向量的长度（即两点间的距离）。
	const float DistAim = ToAim.Size();
	const float DistTarget = ToTarget.Size();

	// 逻辑判断与宏：
	// || 是逻辑或（OR），如果满足任何一个条件就进入花括号。
	// KINDA_SMALL_NUMBER 是 UE 定义的一个极小的常量（比如 0.0001），用于浮点数比较，防止因为浮点精度误差导致崩溃（如除以0）。
	// 判断：如果瞄准距离极短，或者目标距离极短，或者目标物理上已经超出了手臂/骨链的最大伸展范围，就不进行偏移计算。
	if (DistAim < KINDA_SMALL_NUMBER || DistTarget < KINDA_SMALL_NUMBER || DistTarget > DistAim)
	{
		// 返回预定义的零向量常量 (0,0,0)
		return FVector::ZeroVector;
	}

	// 线性代数：FVector::DotProduct 静态方法用于计算两个向量的点积。
	// (ToAim / DistAim) 等价于将向量标准化（归一化为长度为 1 的方向向量）。
	// 点积的几何意义是两个方向向量夹角的余弦值（Cosine）。
	const float Dot = FVector::DotProduct(ToAim / DistAim, ToTarget / DistTarget);

	// 浮点比较：Cos接近1表示夹角接近0度，也就是三个点（骨链起点、瞄准起始点、目标点）几乎连成一条直线了。
	// 如果 Dot < 0.999f (也就是说没有趋近于完全水平共线)，就不属于"奇点"（即骨链不会打死结），直接退出。
	if (Dot < 0.999f)
	{
		return FVector::ZeroVector;
	}

	// UE专属API：.GetSafeNormal() 也是 FVector 的方法，它会将向量安全地归一化（转成长度1的方向向量），内部自带了防除零保护。
	const FVector IKDirection = ToTarget.GetSafeNormal();

	// C++构造函数：利用已有向量的 Y, Z, X 分量（按错位顺序）构建一个新的向量。
	// 这样做的目的是人为制造一个与原方向不共线的辅助向量，专门用来作为接下来的叉乘基准。
	const FVector SecondaryDir(IKDirection.Y, IKDirection.Z, IKDirection.X);

	// 线性代数：FVector::CrossProduct 静态方法用于计算叉积。
	// 两个向量叉乘的结果，一定会产生一个同时垂直于这两个向量的第三个全新向量（法向量）。
	// 在这里，它造出了一个向外偏移侧向推力的方向。
	const FVector OffsetDir = FVector::CrossProduct(IKDirection, SecondaryDir);

	// 返回结果：拿着这个侧推法向量，限制为长度1，然后乘以手臂/骨骼链总长(DistAim)的 5% 作为轻微偏移量，返回出去。
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
