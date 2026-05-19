#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_AimIK.h"
#include "AnimGraphNode_AimIK.generated.h"

UCLASS()
class BBBAIMIKEDITOR_API UAnimGraphNode_AimIK : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = Settings)
    FAnimNode_AimIK Node;

    // UAnimGraphNode_SkeletalControlBase interface
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }

    // UEdGraphNode interface
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override;
    virtual FText GetControllerDescription() const override;
    virtual FString GetNodeCategory() const override;
    virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
    virtual void CreateOutputPins() override;
};
