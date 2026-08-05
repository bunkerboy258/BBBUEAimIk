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

    const FVector InitialAimPositionCS = InOutAimTransformCS.GetLocation();
    const FVector InitialAimForwardCS = InOutAimTransformCS.TransformVectorNoScale(Input.AimAxis).GetSafeNormal();
    const FVector FirstBonePositionCS = InOutChainTransforms[0].GetLocation();

    FVector EffectiveTargetCS = Input.AimTargetCS;
    if (ChainCount >= 2)
    {
        EffectiveTargetCS += GetSingularityOffset(
            FirstBonePositionCS,
            InitialAimPositionCS,
            Input.AimTargetCS);
    }

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

        if (IterationIndex < 1 || Input.Tolerance <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

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
        const float TargetDistance = FVector::Dist(AimPositionCS, TargetCS);
        return AimPositionCS + AimForwardCS * TargetDistance;
    }

    const FVector ToTarget = TargetCS - AimPositionCS;
    const float TargetDistance = ToTarget.Size();
    if (TargetDistance < KINDA_SMALL_NUMBER)
    {
        return TargetCS;
    }

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
        return TargetCS;
    }

    const float TargetClampMultiplier = FMath::Clamp(
        1.0f - (Input.ClampWeight - NormalizedAngle) / OneMinusNormalizedAngle,
        0.0f,
        1.0f);
    float ClampMultiplier = FMath::Clamp(
        NormalizedAngle / Input.ClampWeight,
        0.0f,
        1.0f);

    for (int32 SmoothingIndex = 0; SmoothingIndex < Input.ClampSmoothing; ++SmoothingIndex)
    {
        ClampMultiplier = FMath::Sin(ClampMultiplier * UE_PI * 0.5f);
    }

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
        return FVector::ZeroVector;
    }

    const float DirectionDot = FVector::DotProduct(
        ToAim / AimDistance,
        ToTarget / TargetDistance);
    if (DirectionDot < 0.999f)
    {
        return FVector::ZeroVector;
    }

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

    const FVector ToTargetDirection = (TargetPositionCS - CurrentAimPositionCS).GetSafeNormal();
    if (ToTargetDirection.IsNearlyZero())
    {
        return;
    }

    const FQuat SwingRotation = FQuat::FindBetweenNormals(
        CurrentAimForwardCS,
        ToTargetDirection);
    const FQuat AppliedSwingRotation = Weight >= 1.0f - KINDA_SMALL_NUMBER
        ? SwingRotation
        : FQuat::Slerp(FQuat::Identity, SwingRotation, Weight);

    FQuat AppliedPoleRotation = FQuat::Identity;
    if (Input.PoleWeight > KINDA_SMALL_NUMBER)
    {
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

    const FQuat TotalRotation = AppliedPoleRotation * AppliedSwingRotation;
    if (TotalRotation.IsIdentity())
    {
        return;
    }

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

    InOutAimTransformCS.SetLocation(
        BonePositionCS
        + TotalRotation.RotateVector(InOutAimTransformCS.GetLocation() - BonePositionCS));
    InOutAimTransformCS.SetRotation(
        TotalRotation * InOutAimTransformCS.GetRotation());
}
