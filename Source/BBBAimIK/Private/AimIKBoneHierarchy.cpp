#include "AimIKBoneHierarchy.h"

#include "ReferenceSkeleton.h"

bool FAimIKBoneHierarchy::IsDescendantOrSelf(
    const FReferenceSkeleton& ReferenceSkeleton,
    int32 DescendantIndex,
    int32 AncestorIndex)
{
    if (DescendantIndex == INDEX_NONE || AncestorIndex == INDEX_NONE)
    {
        return false;
    }

    int32 CurrentIndex = DescendantIndex;
    while (CurrentIndex != INDEX_NONE)
    {
        if (CurrentIndex == AncestorIndex)
        {
            return true;
        }

        CurrentIndex = ReferenceSkeleton.GetParentIndex(CurrentIndex);
    }

    return false;
}
