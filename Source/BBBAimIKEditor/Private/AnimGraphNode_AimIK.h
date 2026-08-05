#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_AimIK.h"
#include "AnimGraphNode_AimIK.generated.h"

/**
 * AimIK 动画蓝图节点
 */
UCLASS()
class BBBAIMIKEDITOR_API UAnimGraphNode_AimIK : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    /** 运行时节点配置 */
    UPROPERTY(EditAnywhere, Category = "Settings")
    FAnimNode_AimIK Node;

    //~ Begin UAnimGraphNode_SkeletalControlBase Interface
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override
    {
        return &Node;
    }

    virtual FText GetControllerDescription() const override;
    //~ End UAnimGraphNode_SkeletalControlBase Interface

    //~ Begin UEdGraphNode Interface
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override;
    virtual FString GetNodeCategory() const override;
    virtual void ValidateAnimNodeDuringCompilation(
        USkeleton* ForSkeleton,
        FCompilerResultsLog& MessageLog) override;
    //~ End UEdGraphNode Interface
};
