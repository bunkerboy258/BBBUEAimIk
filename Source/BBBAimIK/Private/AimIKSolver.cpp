#include "AimIKSolver.h"

FVector FAimIKSolver::Solve(
    const FAimIKSolverInput& Input,
    TArray<FTransform>& InOutChainTransforms,
    FTransform& InOutAimTransformCS)
{
    const int32 ChainCount = InOutChainTransforms.Num();
    if (ChainCount == 0)
    {
        return Input.AimTargetCS;
    }

    // 记录求解前的瞄准状态，钳制与奇点检测都基于初始姿态
    const FVector InitialAimPositionCS = InOutAimTransformCS.GetLocation();
    const FVector InitialAimForwardCS = InOutAimTransformCS.TransformVectorNoScale(Input.AimAxis).GetSafeNormal();
    const FVector FirstBonePositionCS = InOutChainTransforms[0].GetLocation();

    // 多节链在目标与链根共线时触及线性奇点，先给目标施加微小侧向偏移
    FVector EffectiveTargetCS = Input.AimTargetCS;
    if (ChainCount >= 2)
    {
        EffectiveTargetCS += GetSingularityOffset(
            FirstBonePositionCS,
            InitialAimPositionCS,
            Input.AimTargetCS);
    }

    // 按钳制强度把目标方向收敛到瞄准前向附近，规避 180 度反向奇点
    const FVector ClampedTargetCS = GetClampedTargetCS(
        Input,
        InitialAimPositionCS,
        InitialAimForwardCS,
        EffectiveTargetCS);
    const float ChainStep = 1.0f / static_cast<float>(ChainCount);
    const int32 IterationCount = FMath::Clamp(Input.MaxIterations, 1, 20);

    for (int32 IterationIndex = 0; IterationIndex < IterationCount; ++IterationIndex)
    {
        for (int32 ChainIndex = 0; ChainIndex < ChainCount; ++ChainIndex)
        {
            // 非尖端骨骼的权重随链深度递增，让根部分摊较小旋转、末端分摊较大旋转
            const float BoneWeightMultiplier = ChainIndex < ChainCount - 1
                ? ChainStep * static_cast<float>(ChainIndex + 1) * Input.BoneChain[ChainIndex].Weight
                : Input.BoneChain[ChainIndex].Weight;
            const float Weight = FMath::Clamp(BoneWeightMultiplier, 0.0f, 1.0f);
            if (Weight <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            RotateBoneToTarget(
                Input,
                ChainIndex,
                ClampedTargetCS,
                Weight,
                InOutChainTransforms,
                InOutAimTransformCS);
        }

        // 首轮迭代尚未生效，且未启用容差时无需评估收敛
        if (IterationIndex < 1 || Input.Tolerance <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        // 计算当前瞄准前向与目标方向的夹角，达到容差即提前结束迭代
        const FVector CurrentAimForwardCS = InOutAimTransformCS.TransformVectorNoScale(Input.AimAxis).GetSafeNormal();
        const FVector ToTargetDirection = (ClampedTargetCS - InOutAimTransformCS.GetLocation()).GetSafeNormal();
        if (ToTargetDirection.IsNearlyZero())
        {
            continue;
        }

        const float Dot = FMath::Clamp(
            FVector::DotProduct(CurrentAimForwardCS, ToTargetDirection),
            -1.0f,
            1.0f);
        const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
        if (AngleDegrees < Input.Tolerance)
        {
            break;
        }
    }

    return ClampedTargetCS;
}

//------------------------------------------------------------------------------

FVector FAimIKSolver::GetClampedTargetCS(
    const FAimIKSolverInput& Input,
    const FVector& AimPositionCS,
    const FVector& AimForwardCS,
    const FVector& TargetCS)
{
    if (Input.ClampWeight <= KINDA_SMALL_NUMBER)
    {
        return TargetCS;
    }

    if (Input.ClampWeight >= 1.0f - KINDA_SMALL_NUMBER)
    {
        // 全强度钳制时目标方向完全贴合瞄准前向，仅保留原始距离
        const float TargetDistance = FVector::Dist(AimPositionCS, TargetCS);
        return AimPositionCS + AimForwardCS * TargetDistance;
    }

    const FVector ToTarget = TargetCS - AimPositionCS;
    const float TargetDistance = ToTarget.Size();
    if (TargetDistance < KINDA_SMALL_NUMBER)
    {
        return TargetCS;
    }

    // 计算目标方向相对瞄准前向的归一化夹角，0 为同向、1 为反向
    const FVector ToTargetDirection = ToTarget / TargetDistance;
    const float DirectionDot = FMath::Clamp(
        FVector::DotProduct(AimForwardCS, ToTargetDirection),
        -1.0f,
        1.0f);
    const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DirectionDot));
    const float NormalizedAngle = 1.0f - AngleDegrees / 180.0f;
    const float OneMinusNormalizedAngle = 1.0f - NormalizedAngle;
    if (OneMinusNormalizedAngle <= KINDA_SMALL_NUMBER)
    {
        // 目标已与瞄准前向同向，无需钳制
        return TargetCS;
    }

    // 按钳制强度与当前夹角计算本次允许转向的比例，夹角越小钳制越弱
    const float TargetClampMultiplier = FMath::Clamp(
        1.0f - (Input.ClampWeight - NormalizedAngle) / OneMinusNormalizedAngle,
        0.0f,
        1.0f);
    float ClampMultiplier = FMath::Clamp(
        NormalizedAngle / Input.ClampWeight,
        0.0f,
        1.0f);

    // 通过正弦曲线重复平滑钳制比例，让边界过渡更柔和
    for (int32 SmoothingIndex = 0; SmoothingIndex < Input.ClampSmoothing; ++SmoothingIndex)
    {
        ClampMultiplier = FMath::Sin(ClampMultiplier * UE_PI * 0.5f);
    }

    // 保持目标距离不变，把瞄准前向按钳制比例摆向目标方向
    const FQuat TargetRotation = FQuat::FindBetweenNormals(AimForwardCS, ToTargetDirection);
    const FQuat ClampedRotation = FQuat::Slerp(
        FQuat::Identity,
        TargetRotation,
        ClampMultiplier * TargetClampMultiplier);
    const FVector ClampedDirection = ClampedRotation.RotateVector(AimForwardCS);

    return AimPositionCS + ClampedDirection * TargetDistance;
}

//------------------------------------------------------------------------------

FVector FAimIKSolver::GetSingularityOffset(
    const FVector& FirstBonePositionCS,
    const FVector& AimPositionCS,
    const FVector& TargetPositionCS)
{
    const FVector ToAim = AimPositionCS - FirstBonePositionCS;
    const FVector ToTarget = TargetPositionCS - FirstBonePositionCS;
    const float AimDistance = ToAim.Size();
    const float TargetDistance = ToTarget.Size();
    if (AimDistance < KINDA_SMALL_NUMBER || TargetDistance < KINDA_SMALL_NUMBER || TargetDistance > AimDistance)
    {
        // 目标比瞄准源更远时不存在完全共线的不可达奇点
        return FVector::ZeroVector;
    }

    const float DirectionDot = FVector::DotProduct(
        ToAim / AimDistance,
        ToTarget / TargetDistance);
    if (DirectionDot < 0.999f)
    {
        // 目标方向与链方向未对齐时不存在奇点
        return FVector::ZeroVector;
    }

    // 由 IK 方向构造一个正交偏移方向，把目标推离共线位置
    const FVector IKDirection = ToTarget.GetSafeNormal();
    const FVector SecondaryDirection(
        IKDirection.Y,
        IKDirection.Z,
        IKDirection.X);
    const FVector OffsetDirection = FVector::CrossProduct(
        IKDirection,
        SecondaryDirection);

    return OffsetDirection.GetSafeNormal() * AimDistance * 0.05f;
}

//------------------------------------------------------------------------------

void FAimIKSolver::RotateBoneToTarget(
    const FAimIKSolverInput& Input,
    int32 ChainIndex,
    const FVector& TargetPositionCS,
    float Weight,
    TArray<FTransform>& InOutChainTransforms,
    FTransform& InOutAimTransformCS)
{
    FTransform& BoneTransformCS = InOutChainTransforms[ChainIndex];
    const FVector CurrentAimPositionCS = InOutAimTransformCS.GetLocation();
    const FVector CurrentAimForwardCS = InOutAimTransformCS.TransformVectorNoScale(Input.AimAxis).GetSafeNormal();
    if (CurrentAimForwardCS.IsNearlyZero())
    {
        return;
    }

    // 计算把当前瞄准前向摆向目标方向的最小旋转
    const FVector ToTargetDirection = (TargetPositionCS - CurrentAimPositionCS).GetSafeNormal();
    if (ToTargetDirection.IsNearlyZero())
    {
        return;
    }

    const FQuat SwingRotation = FQuat::FindBetweenNormals(
        CurrentAimForwardCS,
        ToTargetDirection);
    // 权重不满时按球面插值只应用部分摆动
    const FQuat AppliedSwingRotation = Weight >= 1.0f - KINDA_SMALL_NUMBER
        ? SwingRotation
        : FQuat::Slerp(FQuat::Identity, SwingRotation, Weight);

    FQuat AppliedPoleRotation = FQuat::Identity;
    if (Input.PoleWeight > KINDA_SMALL_NUMBER)
    {
        // 极轴方向投影到垂直于瞄准前向的平面，避免引入绕瞄准轴的额外扭转
        const FVector CurrentAimPoleAxis = InOutAimTransformCS.TransformVectorNoScale(Input.PoleAxis).GetSafeNormal();
        const FVector PoleDirection = Input.PoleTargetCS - CurrentAimPositionCS;
        const FVector OrthogonalPoleDirection = (
            PoleDirection
            - CurrentAimForwardCS * FVector::DotProduct(PoleDirection, CurrentAimForwardCS)).GetSafeNormal();
        if (!CurrentAimPoleAxis.IsNearlyZero() && !OrthogonalPoleDirection.IsNearlyZero())
        {
            const FQuat PoleRotation = FQuat::FindBetweenNormals(
                CurrentAimPoleAxis,
                OrthogonalPoleDirection);
            AppliedPoleRotation = FQuat::Slerp(
                FQuat::Identity,
                PoleRotation,
                Weight * Input.PoleWeight);
        }
    }

    // 极轴纠偏叠加在摆动旋转之后，一次性应用
    const FQuat TotalRotation = AppliedPoleRotation * AppliedSwingRotation;
    if (TotalRotation.IsIdentity())
    {
        return;
    }

    // 以当前骨骼位置为枢轴，把旋转增量传播到链上所有下游骨骼
    const FVector BonePositionCS = BoneTransformCS.GetLocation();
    for (int32 DownstreamIndex = ChainIndex; DownstreamIndex < InOutChainTransforms.Num(); ++DownstreamIndex)
    {
        FTransform& DownstreamTransform = InOutChainTransforms[DownstreamIndex];
        DownstreamTransform.SetLocation(
            BonePositionCS
            + TotalRotation.RotateVector(DownstreamTransform.GetLocation() - BonePositionCS));
        DownstreamTransform.SetRotation(
            TotalRotation * DownstreamTransform.GetRotation());
    }

    // 瞄准源随链同步旋转，供下一链节读取最新瞄准状态
    InOutAimTransformCS.SetLocation(
        BonePositionCS
        + TotalRotation.RotateVector(InOutAimTransformCS.GetLocation() - BonePositionCS));
    InOutAimTransformCS.SetRotation(
        TotalRotation * InOutAimTransformCS.GetRotation());
}
