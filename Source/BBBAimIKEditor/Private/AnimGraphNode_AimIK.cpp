#include "AnimGraphNode_AimIK.h"

#include "Animation/AnimBlueprint.h"

#define LOCTEXT_NAMESPACE "AimIKAnimNode"

FText UAnimGraphNode_AimIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{return LOCTEXT("NodeTitle", "Aim IK");}
FText UAnimGraphNode_AimIK::GetTooltipText() const
{return LOCTEXT("NodeTooltip", "Aim IK: Rotates a bone chain so the pose-local aim source points toward AimTarget.");}
FText UAnimGraphNode_AimIK::GetMenuCategory() const
{return LOCTEXT("NodeCategory", "BBB|IK");}
FLinearColor UAnimGraphNode_AimIK::GetNodeTitleColor() const
{return FLinearColor(0.75f, 0.35f, 0.15f);}
FText UAnimGraphNode_AimIK::GetControllerDescription() const
{return LOCTEXT("ControllerDescription", "Aim IK");}
FString UAnimGraphNode_AimIK::GetNodeCategory() const
{return TEXT("BBB IK");}
void UAnimGraphNode_AimIK::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
    Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

    if (Node.BoneChain.Num() == 0)
    {MessageLog.Warning(*LOCTEXT("NoBones", "@@ - BoneChain is empty. AimIK will have no effect.").ToString());}
    if (Node.AimAxis.IsNearlyZero())
    {MessageLog.Warning(*LOCTEXT("NoAimAxis", "@@ - AimAxis is zero. AimIK will have no effect.").ToString());}
    if (Node.AimSourceBoneName.IsNone())
    {MessageLog.Warning(*LOCTEXT("NoAimSourceBone", "@@ - AimSourceBoneName is not set. AimIK will have no effect.").ToString());}
    else if (ForSkeleton && ForSkeleton->GetReferenceSkeleton().FindBoneIndex(Node.AimSourceBoneName) == INDEX_NONE)
    {MessageLog.Warning(*FText::Format(LOCTEXT("MissingAimSourceBone", "@@ - AimSourceBone '{0}' not found in skeleton."), FText::FromName(Node.AimSourceBoneName)).ToString());}
    for (const FAimIKBoneRef& BoneRef : Node.BoneChain)
    {
        if (BoneRef.BoneName.IsNone())
        {MessageLog.Warning(*LOCTEXT("EmptyBone", "@@ - BoneChain contains an empty bone reference.").ToString());}
        else if (ForSkeleton && ForSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneRef.BoneName) == INDEX_NONE)
        {MessageLog.Warning(*FText::Format(LOCTEXT("MissingBone", "@@ - Bone '{0}' not found in skeleton."), FText::FromName(BoneRef.BoneName)).ToString());}
    }

    if (ForSkeleton && Node.BoneChain.Num() > 0 && !Node.AimSourceBoneName.IsNone())
    {
        const FReferenceSkeleton& RefSkeleton = ForSkeleton->GetReferenceSkeleton();
        const int32 AimSourceIndex = RefSkeleton.FindBoneIndex(Node.AimSourceBoneName);
        const int32 ChainTipIndex = RefSkeleton.FindBoneIndex(Node.BoneChain.Last().BoneName);
        if (AimSourceIndex != INDEX_NONE && ChainTipIndex != INDEX_NONE)
        {
            bool bIsChainDescendant = false;
            int32 CurrentIndex = AimSourceIndex;
            while (CurrentIndex != INDEX_NONE)
            {
                if (CurrentIndex == ChainTipIndex)
                {
                    bIsChainDescendant = true;
                    break;
                }

                CurrentIndex = RefSkeleton.GetParentIndex(CurrentIndex);
            }

            if (!bIsChainDescendant)
            {
                MessageLog.Warning(*FText::Format(
                    LOCTEXT("AimSourceNotChainDescendant", "@@ - AimSourceBone '{0}' must be the chain tip or a descendant of chain tip '{1}'. AimIK will have no effect."),
                    FText::FromName(Node.AimSourceBoneName),
                    FText::FromName(Node.BoneChain.Last().BoneName)).ToString());
            }
        }
    }
}

void UAnimGraphNode_AimIK::CreateOutputPins()
{
    Super::CreateOutputPins();
    // 输出引脚由 AnimGraphNode_SkeletalControlBase 自动创建
}

#undef LOCTEXT_NAMESPACE
