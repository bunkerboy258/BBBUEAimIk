# BBBAimIK

A standalone Unreal Engine 5.6+ plugin providing a **CCD-style AimIK solver** inspired by RootMotion FinalIK's `IKSolverAim`.

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
- A lightweight, data-driven AnimNode with minimal overhead
- Direct FinalIK-inspired semantics (`BoneChain` + `AimSource` + `Target`)
- Per-bone weighting for artistic control
- No additional asset dependencies beyond the AnimBlueprint

---

## Algorithm

### CCD (Cyclic Coordinate Descent) Chain Solver

```
For each iteration:
    For each bone in BoneChain (root → tip):
        1. Compute current aim forward in ComponentSpace
        2. Compute desired direction (target - aim position)
        3. Find rotation that aligns forward → desired
        4. Apply rotation with bone weight and Alpha blending
        5. Propagate rotation to all downstream bones
        6. Update aim source transform for next bone
```

### Singularity Handling

Two safeguards prevent instability:

1. **ClampWeight** — When the target is nearly 180° behind the aim direction, the solver smoothly clamps the effective target toward the current forward, preventing a violent body flip.
2. **Singularity Offset** — When the target lies exactly on the bone-chain extension line (linear singularity), a small perpendicular offset is applied to give the solver a non-degenerate rotation axis.

### Pole Constraint (Optional)

If `PoleWeight > 0`, the solver adds a secondary rotation that keeps a user-defined pole axis oriented toward a pole target. Useful for preventing the torso from rolling sideways during extreme vertical aiming.

---

## Key Design Decisions

### 1. Pose-Native Aim Source

The aim source is **not** an external world transform pushed into the node every frame. Instead, it is reconstructed from the current pose:

```cpp
FTransform AimSourceBoneCS = Output.Pose.GetComponentSpaceTransform(AimSourceBoneIndex);
FTransform CurrentAimTransformCS = AimSourceLocalTransform * AimSourceBoneCS;
```

This eliminates the feedback loop that caused frame-to-frame oscillation in earlier implementations:

```
Frame N:   IK rotates spine → muzzle moves
Frame N+1: New muzzle position fed back as input → IK over-corrects
```

### 2. Stable Binding Transform

`AimSourceLocalTransform` is a **stable binding relationship** (e.g. muzzle relative to `hand_r`). It should be computed **once** at equip time and never updated per frame.

### 3. Single Runtime Module with Conditional Editor Macros

Unlike many UE plugins that split `Runtime` and `Editor` modules, BBBAimIK keeps everything in one Runtime module:

- `FAnimNode_AimIK` — always compiled
- `UAnimGraphNode_AimIK` — wrapped in `#if WITH_EDITOR` / `#if WITH_EDITORONLY_DATA`

This avoids the "Editor Only module" blueprint warning while keeping Shipping builds safe (editor code is stripped).

### 4. Descendant Validation

At initialization, the solver walks the skeleton hierarchy to verify that `AimSourceBoneName` is a descendant of the chain tip bone. If not, evaluation is silently skipped. This hard constraint prevents misconfigured chains from producing garbage output.

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

---

## Credits

Algorithm inspired by RootMotion FinalIK's `IKSolverAim`. Implemented as a native UE AnimNode for performance and pipeline compatibility.
