#include "AnimGraphNode_AimIK.h"

#include "Animation/Skeleton.h"
#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "AimIKAnimNode"

FText UAnimGraphNode_AimIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "Aim IK");
}

//------------------------------------------------------------------------------

FText UAnimGraphNode_AimIK::GetTooltipText() const
{
    return LOCTEXT(
        "NodeTooltip",
        "Rotates a bone chain so the pose-local aim source points toward AimTarget.");
}

//------------------------------------------------------------------------------

FText UAnimGraphNode_AimIK::GetMenuCategory() const
{
    return LOCTEXT("NodeCategory", "BBB|IK");
}

//------------------------------------------------------------------------------

FLinearColor UAnimGraphNode_AimIK::GetNodeTitleColor() const
{
    return FLinearColor(0.75f, 0.35f, 0.15f);
}

//------------------------------------------------------------------------------

FText UAnimGraphNode_AimIK::GetControllerDescription() const
{
    return LOCTEXT("ControllerDescription", "Aim IK");
}

//------------------------------------------------------------------------------

FString UAnimGraphNode_AimIK::GetNodeCategory() const
{
    return TEXT("BBB IK");
}

//------------------------------------------------------------------------------

void UAnimGraphNode_AimIK::ValidateAnimNodeDuringCompilation(
    USkeleton* ForSkeleton,
    FCompilerResultsLog& MessageLog)
{
    Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

    // 先做与骨架无关的基础配置检查
    if (Node.BoneChain.Num() == 0)
    {
        MessageLog.Warning(
            *LOCTEXT("NoBones", "@@ - BoneChain is empty. AimIK will have no effect.").ToString());
    }

    if (Node.AimAxis.IsNearlyZero())
    {
        MessageLog.Warning(
            *LOCTEXT("NoAimAxis", "@@ - AimAxis is zero. AimIK will have no effect.").ToString());
    }

    if (Node.AimSourceBoneName.IsNone())
    {
        MessageLog.Warning(
            *LOCTEXT("NoAimSourceBone", "@@ - AimSourceBoneName is not set. AimIK will have no effect.").ToString());
    }

    if (!ForSkeleton)
    {
        return;
    }

    const FReferenceSkeleton& ReferenceSkeleton = ForSkeleton->GetReferenceSkeleton();

    // 检查瞄准源骨骼是否存在于当前骨架
    const int32 AimSourceIndex = ReferenceSkeleton.FindBoneIndex(Node.AimSourceBoneName);
    if (!Node.AimSourceBoneName.IsNone() && AimSourceIndex == INDEX_NONE)
    {
        MessageLog.Warning(
            *FText::Format(
                LOCTEXT("MissingAimSourceBone", "@@ - AimSourceBone '{0}' was not found in the skeleton."),
                FText::FromName(Node.AimSourceBoneName)).ToString());
    }

    // 逐节检查骨骼链配置是否都能在骨架中找到
    for (const FAimIKBoneRef& BoneReference : Node.BoneChain)
    {
        if (BoneReference.BoneName.IsNone())
        {
            MessageLog.Warning(
                *LOCTEXT("EmptyBone", "@@ - BoneChain contains an empty bone reference.").ToString());
            continue;
        }

        if (ReferenceSkeleton.FindBoneIndex(BoneReference.BoneName) != INDEX_NONE)
        {
            continue;
        }

        MessageLog.Warning(
            *FText::Format(
                LOCTEXT("MissingBone", "@@ - Bone '{0}' was not found in the skeleton."),
                FText::FromName(BoneReference.BoneName)).ToString());
    }

    // 骨骼链或瞄准源缺失时不做层级校验，前面的警告已覆盖
    if (Node.BoneChain.Num() == 0 || AimSourceIndex == INDEX_NONE)
    {
        return;
    }

    // 瞄准源必须是链尖端或其后代，否则求解无法影响瞄准方向
    if (Node.HasValidAimSourceHierarchy(ReferenceSkeleton))
    {
        return;
    }

    MessageLog.Warning(
        *FText::Format(
            LOCTEXT(
                "AimSourceNotChainDescendant",
                "@@ - AimSourceBone '{0}' must be the chain tip or a descendant of chain tip '{1}'. AimIK will have no effect."),
            FText::FromName(Node.AimSourceBoneName),
            FText::FromName(Node.BoneChain.Last().BoneName)).ToString());
}

#undef LOCTEXT_NAMESPACE
