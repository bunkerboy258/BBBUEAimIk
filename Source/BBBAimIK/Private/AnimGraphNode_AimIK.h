#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "AnimGraphNode_SkeletalControlBase.h"
#endif

#include "AnimNode_AimIK.h"
#include "AnimGraphNode_AimIK.generated.h"

#if WITH_EDITOR
UCLASS()
class BBBAIMIK_API UAnimGraphNode_AimIK : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

#if WITH_EDITORONLY_DATA
public:
    UPROPERTY(EditAnywhere, Category = Settings)
    FAnimNode_AimIK Node;
#endif

#if WITH_EDITOR
public:
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
#endif
};
#endif // WITH_EDITOR
