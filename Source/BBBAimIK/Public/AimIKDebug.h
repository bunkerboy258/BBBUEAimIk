#pragma once
#include "AimIKDebugFrame.h"
class USkeletalMeshComponent;

/** 插件调试订阅桥接 不持有项目玩法对象 */
class BBBAIMIK_API FAimIKDebug
{
public:
    /** @param Mesh 订阅组件 @param bEnabled 是否采集 @return 无 */
    static void SetEnabled(USkeletalMeshComponent* Mesh, bool bEnabled);
    /** @param Mesh 组件身份 @return 当前是否订阅 */
    static bool IsEnabled(const USkeletalMeshComponent* Mesh);
    /** @param Mesh 组件身份 @param Value 本次求值结果 @return 无 */
    static void Publish(const USkeletalMeshComponent* Mesh, FAimIKDebugFrame Value);
    /** @param Mesh 组件身份 @param Value 回读结果 @return 是否存在本帧结果 */
    static bool Read(const USkeletalMeshComponent* Mesh, FAimIKDebugFrame& Value);
};
