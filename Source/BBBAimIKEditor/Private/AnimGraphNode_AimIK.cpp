#include "AnimGraphNode_AimIK.h"

#include "Animation/Skeleton.h"
#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "AimIKAnimNode"

FText UAnimGraphNode_AimIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "瞄准 IK");
}

//------------------------------------------------------------------------------

FText UAnimGraphNode_AimIK::GetTooltipText() const
{
    return LOCTEXT(
        "NodeTooltip",
        "旋转骨骼链 使当前姿态的局部瞄准源指向瞄准目标.");
}

//------------------------------------------------------------------------------

FText UAnimGraphNode_AimIK::GetMenuCategory() const
{
    return LOCTEXT("NodeCategory", "BBB|反向运动学");
}

//------------------------------------------------------------------------------

FLinearColor UAnimGraphNode_AimIK::GetNodeTitleColor() const
{
    return FLinearColor(0.75f, 0.35f, 0.15f);
}

//------------------------------------------------------------------------------

FText UAnimGraphNode_AimIK::GetControllerDescription() const
{
    return LOCTEXT("ControllerDescription", "瞄准 IK");
}

//------------------------------------------------------------------------------

FString UAnimGraphNode_AimIK::GetNodeCategory() const
{
    return TEXT("BBB 反向运动学");
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
            *LOCTEXT("NoBones", "@@ - 骨骼链为空 瞄准 IK 不会生效.").ToString());
    }

    if (Node.AimAxis.IsNearlyZero())
    {
        MessageLog.Warning(
            *LOCTEXT("NoAimAxis", "@@ - 瞄准轴为零向量 瞄准 IK 不会生效.").ToString());
    }

    if (Node.AimSourceBoneName.IsNone())
    {
        MessageLog.Warning(
            *LOCTEXT("NoAimSourceBone", "@@ - 未设置瞄准源骨骼 瞄准 IK 不会生效.").ToString());
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
                LOCTEXT("MissingAimSourceBone", "@@ - 骨架中找不到瞄准源骨骼 '{0}'."),
                FText::FromName(Node.AimSourceBoneName)).ToString());
    }

    // 逐节检查骨骼链配置是否都能在骨架中找到
    for (const FAimIKBoneRef& BoneReference : Node.BoneChain)
    {
        if (BoneReference.BoneName.IsNone())
        {
            MessageLog.Warning(
                *LOCTEXT("EmptyBone", "@@ - 骨骼链包含空骨骼引用.").ToString());
            continue;
        }

        if (ReferenceSkeleton.FindBoneIndex(BoneReference.BoneName) != INDEX_NONE)
        {
            continue;
        }

        MessageLog.Warning(
            *FText::Format(
                LOCTEXT("MissingBone", "@@ - 骨架中找不到骨骼 '{0}'."),
                FText::FromName(BoneReference.BoneName)).ToString());
    }

    // 骨骼链或瞄准源缺失时不做层级校验 前面的警告已覆盖
    if (Node.BoneChain.Num() == 0 || AimSourceIndex == INDEX_NONE)
    {
        return;
    }

    // 瞄准源必须是链尖端或其后代 否则求解无法影响瞄准方向
    if (Node.HasValidAimSourceHierarchy(ReferenceSkeleton))
    {
        return;
    }

    MessageLog.Warning(
        *FText::Format(
            LOCTEXT(
                "AimSourceNotChainDescendant",
                "@@ - 瞄准源骨骼 '{0}' 必须是链尖骨骼 '{1}' 或其后代 瞄准 IK 不会生效."),
            FText::FromName(Node.AimSourceBoneName),
            FText::FromName(Node.BoneChain.Last().BoneName)).ToString());
}

#undef LOCTEXT_NAMESPACE
