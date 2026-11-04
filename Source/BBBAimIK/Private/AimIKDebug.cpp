#include "AimIKDebug.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/ScopeLock.h"
#include <atomic>

namespace
{
    FCriticalSection DebugLock;
    TMap<TWeakObjectPtr<const USkeletalMeshComponent>, FAimIKDebugFrame> Frames;
    std::atomic<bool> HasSubscribers{false};
    uint64 Evaluation = 0;
}

void FAimIKDebug::SetEnabled(USkeletalMeshComponent* Mesh, bool bEnabled)
{
    check(IsInGameThread());
    FScopeLock Lock(&DebugLock);
    for (auto It = Frames.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
    if (Mesh && bEnabled && !Frames.Contains(Mesh)
        && ensureMsgf(Frames.Num() < 64, TEXT("AimIK 调试订阅超出上限")))
    {
        Frames.Add(Mesh, FAimIKDebugFrame{});
    }
    if (!bEnabled)
    {
        Frames.Remove(Mesh);
    }
    HasSubscribers.store(!Frames.IsEmpty());
}

bool FAimIKDebug::IsEnabled(const USkeletalMeshComponent* Mesh)
{
    if (!HasSubscribers.load())
    {
        return false;
    }
    FScopeLock Lock(&DebugLock);
    return Frames.Contains(Mesh);
}

void FAimIKDebug::Publish(const USkeletalMeshComponent* Mesh, FAimIKDebugFrame Value)
{
    FScopeLock Lock(&DebugLock);
    if (auto* Slot = Frames.Find(Mesh))
    {
        Value.Evaluation = ++Evaluation;
        *Slot = Value;
    }
}

bool FAimIKDebug::Read(const USkeletalMeshComponent* Mesh, FAimIKDebugFrame& Value)
{
    check(IsInGameThread());
    FScopeLock Lock(&DebugLock);
    const auto* Slot = Frames.Find(Mesh);
    if (!Slot || !Slot->Evaluation || Slot->Frame != GFrameCounter)
    {
        return false;
    }
    Value = *Slot;
    return true;
}
