#pragma once

#include "CoreMinimal.h"

struct FReferenceSkeleton;

/**
 * AimIK 骨骼层级查询
 */
class FAimIKBoneHierarchy final
{
public:
    /**
     * 判断指定骨骼是否为祖先骨骼自身或其后代
     *
     * @param ReferenceSkeleton  参考骨架
     * @param DescendantIndex    待检查骨骼索引
     * @param AncestorIndex      祖先骨骼索引
     * @return 层级关系满足要求时返回 true
     */
    static bool IsDescendantOrSelf(
        const FReferenceSkeleton& ReferenceSkeleton,
        int32 DescendantIndex,
        int32 AncestorIndex);
};
