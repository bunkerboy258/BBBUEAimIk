# BBBAimIK

A standalone Unreal Engine 5.6+ plugin providing a **CCD-style AimIK solver** for skeletal mesh aiming.

Rotates a configurable bone chain so that a **pose-local aim source** (e.g. a virtual muzzle attached to `hand_r`) points toward a target in component space. No external per-frame transform feedback — the aim source is reconstructed entirely from the current skeletal pose.

---

## Table of Contents

- [Overview](#overview)
- [Algorithm](#algorithm)
- [Key Design Decisions](#key-design-decisions)
- [Installation](#installation)
- [AnimBlueprint Setup](#animblueprint-setup)
- [Parameter Reference](#parameter-reference)
- [Architecture](#architecture)
- [File Structure](#file-structure)
- [Compatibility](#compatibility)

---

## Overview

**BBBAimIK** is a single-runtime-module plugin. It adds one AnimGraph node: **BBB|IK → Aim IK**.

The solver works in **Component Space** and operates on a user-defined bone chain (typically the spine hierarchy). It iteratively rotates each bone so that a virtual aim source — defined by a bone reference plus a stable local transform — converges toward the target direction.

### Why not just use Control Rig?

Control Rig is powerful but heavy for simple upper-body aiming. BBBAimIK gives you:
- A lightweight data-driven AnimNode with minimal overhead
- Per-bone weighting for artistic control
- No additional asset dependencies beyond the AnimBlueprint

---

## Algorithm

### CCD (Cyclic Coordinate Descent) Chain Solver

BBBAimIK 使用 CCD 迭代算法逐步调整骨骼链 使瞄准源对准目标。核心流程如下：

```
对于每一次迭代：
    对于 BoneChain 中的每根骨骼（从根到末端）：
        1. 在当前 Pose 中重建瞄准源的 ComponentSpace 变换
        2. 计算当前瞄准方向（AimSource 前向向量）
        3. 计算期望方向（目标位置 - 瞄准源位置）
        4. 求出使当前方向对齐到期望方向的最小旋转
        5. 将该旋转乘以骨骼权重和 Alpha 混合系数后应用到当前骨骼
        6. 将旋转传播到该骨骼的所有下游子骨骼
        7. 更新瞄准源变换 供下一根骨骼使用
```

#### 详细求解步骤

**Step 1 — 重建瞄准源变换**

CCD 不是基于外部传入的实时世界坐标 而是从当前 Pose 内部重建：

```cpp
FTransform AimSourceBoneCS = Output.Pose.GetComponentSpaceTransform(AimSourceBoneIndex);
FTransform CurrentAimTransformCS = AimSourceLocalTransform * AimSourceBoneCS;
```

- `AimSourceBoneCS`：AimSource 所在骨骼在当前 Pose 中的 ComponentSpace 变换
- `AimSourceLocalTransform`：装备时缓存的局部偏移（如枪口相对 `hand_r` 的绑定关系）
- `CurrentAimTransformCS`：重建后的瞄准源 ComponentSpace 变换

**Step 2 — 计算旋转差**

```cpp
FVector CurrentForward = CurrentAimTransformCS.TransformVectorNoScale(AimAxis);
FVector DesiredForward = (AimTarget - CurrentAimTransformCS.GetLocation()).GetSafeNormal();
FQuat DeltaRotation = FQuat::FindBetweenNormals(CurrentForward, DesiredForward);
```

- `CurrentForward`：当前瞄准方向
- `DesiredForward`：期望瞄准方向（目标位置 - 瞄准源位置 单位化）
- `DeltaRotation`：将当前方向旋转到期望方向的最小四元数

**Step 3 — 应用权重与混合**

```cpp
float EffectiveWeight = BoneRef.Weight * Alpha;
FQuat BlendedRotation = FQuat::Slerp(FQuat::Identity, DeltaRotation, EffectiveWeight);
```

- `BoneRef.Weight`：该骨骼在 BoneChain 中配置的权重（艺术控制）
- `Alpha`：全局混合系数（如瞄准状态插值）
- `BlendedRotation`：权重限制后的旋转 避免单根骨骼转动过猛

**Step 4 — 传播与迭代**

将 `BlendedRotation` 应用到当前骨骼后 立即把旋转写入 Pose 并传播给所有子骨骼。下一根骨骼在**已被修改过的 Pose** 上继续求解。多次迭代后 整条骨骼链逐渐收敛到目标方向。

### Singularity Handling

两个安全措施防止求解不稳定：

1. **ClampWeight** — 当目标几乎在瞄准方向正后方（接近 180°）时 求解器将有效目标平滑钳制到当前前向附近 防止躯干剧烈翻转。
2. **Singularity Offset** — 当目标恰好落在骨骼链延长线上（线性奇异点）时 施加一个微小的垂直偏移 给求解器提供非退化的旋转轴。

### Pole Constraint (Optional)

当 `PoleWeight > 0` 时 求解器叠加一个二级旋转 使用户定义的极轴朝向极目标。用于防止躯干在极端垂直瞄准时发生侧翻。

---

## Key Design Decisions

### 1. Pose-Native Aim Source

瞄准源**不是**每帧从外部推入的世界坐标。而是从当前 Pose 内部重建：

```cpp
FTransform AimSourceBoneCS = Output.Pose.GetComponentSpaceTransform(AimSourceBoneIndex);
FTransform CurrentAimTransformCS = AimSourceLocalTransform * AimSourceBoneCS;
```

这消除了早期实现中的帧间反馈振荡：

```
第 N 帧：   IK 旋转脊柱 → 枪口位置移动
第 N+1 帧：新的枪口位置作为输入反馈 → IK 过度修正
```

### 2. Stable Binding Transform

`AimSourceLocalTransform` 是一个**稳定的绑定关系**（如枪口相对 `hand_r` 的局部偏移）。应在装备时计算**一次** 之后不再每帧更新。

### 3. Single Runtime Module with Conditional Editor Macros

与许多拆分 Runtime 和 Editor 模块的 UE 插件不同 BBBAimIK 将所有内容保留在一个 Runtime 模块中：

- `FAnimNode_AimIK` — 始终编译
- `UAnimGraphNode_AimIK` — 用 `#if WITH_EDITOR` / `#if WITH_EDITORONLY_DATA` 包裹

这避免了 "Editor Only module" 蓝图警告 同时保证 Shipping 构建安全（编辑器代码被剥离）。

### 4. Descendant Validation

初始化时 求解器遍历骨骼层级 验证 `AimSourceBoneName` 是否为骨骼链末端骨骼的后代。若不是 则静默跳过求值。这一硬约束防止错误配置的骨骼链产生无效输出。

---

## Installation

### 1. Copy the Plugin

Copy `BBBAimIK/` into your project's `Plugins/` directory:

```text
YourProject/
└── Plugins/
    └── BBBAimIK/
        ├── BBBAimIK.uplugin
        └── Source/
            └── BBBAimIK/
                ├── BBBAimIK.Build.cs
                ├── Private/
                │   ├── AnimNode_AimIK.cpp
                │   ├── AnimGraphNode_AimIK.cpp
                │   ├── AnimGraphNode_AimIK.h
                │   └── BBBAimIK.cpp
                └── Public/
                    └── AnimNode_AimIK.h
```

### 2. Add Module Dependency

In your project's main `.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    // ... existing modules ...
    "BBBAimIK"
});
```

> The plugin's editor dependencies (`AnimGraph`, `BlueprintGraph`, `UnrealEd`) are already handled conditionally inside `BBBAimIK.Build.cs`. You do **not** need to add them to your project.

### 3. Regenerate & Compile

1. Right-click `.uproject` → **Generate Visual Studio project files**
2. Compile your project (Editor target)

---

## AnimBlueprint Setup

### Step 1 — Place the Node

1. Open your character's AnimBlueprint
2. In the **AnimGraph**, right-click before **Final Animation Pose**
3. Search **"Aim IK"** → select **BBB|IK → Aim IK**

### Step 2 — Configure the Bone Chain

In the node's **Details** panel, expand **BoneChain** and add bones in order (root → tip):

| Index | Bone Name | Recommended Weight |
|-------|-----------|-------------------|
| 0 | `spine_01` | 0.2 |
| 1 | `spine_02` | 0.3 |
| 2 | `spine_03` | 0.5 |
| 3 | `spine_04` | 0.7 |
| 4 | `spine_05` | 0.8 |

> The order must be parent → child. Weights control how much each bone participates in the solve.

### Step 3 — Configure the Aim Source

| Property | Value | Source |
|----------|-------|--------|
| `AimSourceBoneName` | `hand_r` | The bone that carries the virtual muzzle |
| `AimSourceLocalTransform` | Your cached offset | Equipment system (computed once at equip) |
| `AimAxis` | `(1, 0, 0)` | Local axis of the aim source that points forward |

> `AimSourceBoneName` must be a descendant of the chain tip bone (e.g. `spine_05 → ... → hand_r`).

### Step 4 — Wire the Input Pins

Connect from your AnimInstance:

```
[AimSourceLocalTransform]    → [AimSourceLocalTransform] (AimIK Node)
[AimTargetComponentSpace]    → [AimTarget]               (AimIK Node)
[bHasValidAimTarget]         → [bHasValidAimTarget]      (AimIK Node)
[bIsAiming]                  → [Alpha]                   (AimIK Node)
```

`AimTargetComponentSpace` is a `FVector` in **Component Space** (not world space).

### Step 5 — Compile & Test

1. **Compile** the AnimBlueprint
2. **Play in Editor**
3. Hold aim and move the camera — the upper body should rotate to track the target

---

## Parameter Reference

### Bone Chain

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `BoneChain` | `TArray<FAimIKBoneRef>` | `[]` | Ordered bone list (root → tip) with per-bone weights |

### Aim

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `AimSourceBoneName` | `FName` | `None` | Bone carrying the virtual aim source |
| `AimSourceLocalTransform` | `FTransform` | `Identity` | Stable local offset of the aim source relative to `AimSourceBoneName` |
| `AimAxis` | `FVector` | `(1,0,0)` | Local axis on the aim source that should point toward the target |

### Solver

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `AimTarget` | `FVector` | `(0,0,0)` | Target position in **Component Space** |
| `bHasValidAimTarget` | `bool` | `false` | Explicit validity flag. When false, the solver is skipped |
| `MaxIterations` | `int32` | `4` | CCD iterations per frame. Higher = more accurate, more expensive |
| `Tolerance` | `float` | `0.0` | Early-exit angular threshold (degrees). `0` = always iterate full count |

### Clamp

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `ClampWeight` | `float` | `0.1` | How aggressively to clamp the target toward current forward when near 180° |
| `ClampSmoothing` | `int32` | `2` | Number of smoothing passes applied to the clamp blend |

### Pole

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `PoleAxis` | `FVector` | `(0,0,1)` | Local axis defining "up" for the aim source |
| `PoleTarget` | `FVector` | `(0,0,0)` | Target position in Component Space for the pole axis |
| `PoleWeight` | `float` | `0.0` | How strongly to enforce the pole constraint |

### Debug

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableDebugLogging` | `bool` | `false` | Enables `LogAnimation` output for solver diagnostics |

---

## Architecture

```
[Weapon Equip]
    ↓
[Cache AimSourceLocalTransform]  (Muzzle relative to hand_r, computed once)
    ↓
[AnimInstance::NativeUpdateAnimation]
    ↓
[AimTargetComponentSpace]  ←  WorldToMesh.TransformPosition(AimTargetWorld)
[AimSourceLocalTransform]  ←  WeaponComponent (cached stable binding)
[bHasValidAimTarget]       ←  bHasValidAimIKTarget && bHasValidAimSource
    ↓
[AnimGraph: Aim IK Node]
    ↓
[InitializeBoneReferences]
    - Resolve BoneChain indices
    - Resolve AimSourceBoneName index
    - Validate AimSourceBone is descendant of chain tip
    ↓
[EvaluateSkeletalControl_AnyThread]
    - Reconstruct CurrentAimTransformCS from Pose
    - Clamp target (180° guard)
    - Offset singularity
    - CCD iterate over BoneChain
    - Output sorted FBoneTransform array
    ↓
[Final Animation Pose]
```

---

## File Structure

```
Source/BBBAimIK/
├── BBBAimIK.Build.cs
├── Private/
│   ├── BBBAimIK.cpp              # Module entry point (IMPLEMENT_MODULE)
│   ├── AnimNode_AimIK.cpp        # CCD solver implementation
│   ├── AnimGraphNode_AimIK.h     # Editor node declaration (WITH_EDITOR)
│   └── AnimGraphNode_AimIK.cpp   # Editor node UI & validation (WITH_EDITOR)
└── Public/
    └── AnimNode_AimIK.h          # Runtime node definition (FAnimNode_AimIK)
```

---

## Compatibility

- **Unreal Engine**: 5.6+
- **Platforms**: All (runtime code is platform-agnostic; editor UI is Win64/macOS/Linux)
- **Skeleton**: Any skeleton with a standard hierarchy. Bone names are user-configurable.
