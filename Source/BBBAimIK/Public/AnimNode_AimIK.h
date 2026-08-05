#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_AimIK.generated.h"

/**
 * 骨骼链单节配置
 */
USTRUCT(BlueprintType)
struct FAimIKBoneRef
{
    GENERATED_BODY()

    /** 骨骼名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone")
    FName BoneName;

    /** 该骨骼在求解中分摊的旋转权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Weight = 1.0f;
};

/**
 * 旋转配置的骨骼链，使姿态局部的瞄准源指向目标点
 * 瞄准源通过 AimSourceBoneName 与 AimSourceLocalTransform 从当前姿态重建，不依赖外部每帧的变换回传
 *
 * 步枪瞄准推荐的纯脊柱链配置：
 * spine_01 (0.2) -> spine_02 (0.3) -> spine_03 (0.5) -> spine_04 (0.7) -> spine_05 (0.8)
 */
USTRUCT(BlueprintInternalUseOnly)
struct BBBAIMIK_API FAnimNode_AimIK : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    /** 骨骼链，按脊柱层级从根到尖端排列，支持逐骨骼权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoneChain")
    TArray<FAimIKBoneRef> BoneChain;

    ////

    /** 承载虚拟瞄准源的骨骼，等价于 FinalIK 的变换父级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FName AimSourceBoneName;

    /** 瞄准源相对 AimSourceBoneName 的局部变换，必须是稳定的绑定关系而非每帧外部快照 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
    FTransform AimSourceLocalTransform = FTransform::Identity;

    /** 瞄准源上应指向目标的局部轴，默认 X+ */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FVector AimAxis = FVector::ForwardVector;

    ////

    /** 极轴，用于防止身体翻转 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleAxis = FVector::UpVector;

    /** 极轴目标位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole")
    FVector PoleTarget = FVector::ZeroVector;

    /** 极轴纠偏权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pole", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PoleWeight = 0.0f;

    ////

    /** 目标钳制强度，用于规避 180 度奇点跳变 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ClampWeight = 0.1f;

    /** 钳制结果的平滑迭代次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0", ClampMax = "2"))
    int32 ClampSmoothing = 2;

    ////

    /** CCD 最大迭代次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "1"))
    int32 MaxIterations = 4;

    /** 提前停止迭代的最小角度误差，单位为度，0 表示始终迭代满 MaxIterations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0"))
    float Tolerance = 0.0f;

    /** 组件空间中的目标位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    FVector AimTarget = FVector::ZeroVector;

    /** 目标有效标志，零向量也是合法的组件空间目标，必须显式标记 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (PinShownByDefault))
    bool bHasValidAimTarget = false;

    ////

    /** 是否启用最小目标距离防呆 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety")
    bool bEnableMinTargetDistanceGuard = true;

    /** 目标与瞄准源的最小距离，小于该值时放弃求解 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0.0"))
    float MinTargetDistance = 30.0f;

    ////

    /** 调试日志开关，在 EvaluateSkeletalControl_AnyThread 中输出 UE_LOG，动画评估线程记日志有性能开销，生产环境禁止开启 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugLogging = false;

    /** 求解阶段日志的采样间隔，避免调试开启时逐帧刷屏 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "1"))
    int32 DebugSolveLogInterval = 60;

    //~ Begin FAnimNode_SkeletalControlBase Interface
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    //~ End FAnimNode_SkeletalControlBase Interface

private:
    /** 骨骼链对应的紧凑姿态索引缓存 */
    TArray<int32> CachedBoneIndices;

    /** 瞄准源骨骼的紧凑姿态索引 */
    int32 AimSourceBoneIndex = INDEX_NONE;

    /** 骨骼缓存是否全部有效 */
    bool bCachedBonesValid = false;

    /** 瞄准源骨骼是否为链尖端的子孙 */
    bool bAimSourceIsChainDescendant = false;

    /** 是否已缓存上一帧求解前的骨骼姿态 */
    bool bHasPreviousInputPose = false;

    /** 上一帧求解前的瞄准源骨骼组件空间变换 */
    FTransform PreviousAimSourceBoneTransformCS = FTransform::Identity;

    /** 上一帧求解前的骨骼链组件空间变换 */
    TArray<FTransform> PreviousChainTransformsCS;

    /**
     * 核心求解入口，执行 CCD 迭代并输出骨骼变换
     *
     * @param	Output				组件空间姿态上下文
     * @param	OutBoneTransforms	输出的骨骼变换列表
     */
    void SolveAimIK(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms);

    /**
     * 钳制目标方向，规避 180 度奇点
     *
     * @param	AimBonePosCS		瞄准源的组件空间位置
     * @param	AimBoneForwardCS	瞄准前向的组件空间方向
     * @param	TargetCS			原始目标的组件空间位置
     * @return	钳制后的组件空间目标位置
     */
    FVector GetClampedTargetCS(const FVector& AimBonePosCS, const FVector& AimBoneForwardCS, const FVector& TargetCS) const;

    /**
     * 当目标位于链延伸线上时计算偏移量，规避线性奇点
     *
     * @param	FirstBonePosCS	链根骨骼的组件空间位置
     * @param	AimPosCS		瞄准源的组件空间位置
     * @param	TargetPosCS		目标的组件空间位置
     * @return	奇点偏移向量，未触及奇点时返回零向量
     */
    FVector GetSingularityOffset(const FVector& FirstBonePosCS, const FVector& AimPosCS, const FVector& TargetPosCS) const;

    /**
     * 旋转单个链节，并将旋转增量传播到下游链节与瞄准变换
     *
     * @param	ChainIndex				当前求解的链节下标
     * @param	TargetPosCS				组件空间目标位置
     * @param	Weight					本次旋转的权重
     * @param	InOutChainTransforms	链节组件空间变换列表，就地修改
     * @param	InOutAimTransformCS		瞄准源的组件空间变换，就地修改
     */
    void RotateBoneToTarget(
        int32 ChainIndex,
        const FVector& TargetPosCS,
        float Weight,
        TArray<FTransform>& InOutChainTransforms,
        FTransform& InOutAimTransformCS);
};
