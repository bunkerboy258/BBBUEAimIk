# BBBAimIK

BBBAimIK 是面向 Unreal Engine 5.6+ 的 Aim IK 插件。它提供一个组件空间动画蓝图节点，通过 CCD 迭代旋转指定骨骼链，使姿态中的局部瞄准源指向目标位置。

插件由两个模块组成：

- `BBBAimIK`：运行时节点、骨骼缓存和求解器。
- `BBBAimIKEditor`：动画蓝图节点外观与编译期校验，仅在未烘焙目标中加载。

## 设计边界

插件解决以下问题：

- 按配置权重旋转脊柱、颈部或手臂骨骼链。
- 从当前 Pose 和稳定局部绑定重建瞄准源。
- 将组件空间目标转换为骨骼链旋转结果。
- 通过目标钳制、线性奇点偏移和可选极轴约束减少翻转。

插件不负责：

- 获取摄像机或武器的世界空间目标。
- 每帧从场景组件反向读取求解后的枪口变换。
- 将世界空间坐标自动转换成 Skeletal Mesh 组件空间。

## 运行流程

```text
AnimBlueprint
    -> FAnimNode_AimIK
        -> 校验输入和缓存骨骼索引
        -> 从当前 Pose 重建瞄准源组件空间变换
        -> FAimIKSolver
            -> 目标钳制
            -> 线性奇点偏移
            -> CCD 迭代
            -> 极轴纠偏
        -> 输出排序后的 FBoneTransform
    -> Final Animation Pose
```

瞄准源按以下关系重建：

```cpp
const FTransform AimSourceBoneTransformCS = Output.Pose.GetComponentSpaceTransform(AimSourceBoneIndex);
const FTransform AimTransformCS = AimSourceLocalTransform * AimSourceBoneTransformCS;
```

`AimSourceLocalTransform` 表示瞄准源相对 `AimSourceBoneName` 的稳定绑定。它应在装备建立绑定时计算，而不是把求解结果作为下一帧输入。

## 文件结构

```text
BBBAimIK/
├── BBBAimIK.uplugin
├── README.md
└── Source/
    ├── BBBAimIK/
    │   ├── BBBAimIK.Build.cs
    │   ├── Public/
    │   │   └── AnimNode_AimIK.h
    │   └── Private/
    │       ├── AimIKBoneHierarchy.h
    │       ├── AimIKBoneHierarchy.cpp
    │       ├── AimIKSolver.h
    │       ├── AimIKSolver.cpp
    │       ├── AnimNode_AimIK.cpp
    │       └── BBBAimIK.cpp
    └── BBBAimIKEditor/
        ├── BBBAimIKEditor.Build.cs
        └── Private/
            ├── AnimGraphNode_AimIK.h
            ├── AnimGraphNode_AimIK.cpp
            └── BBBAimIKEditor.cpp
```

各文件职责：

- `AnimNode_AimIK`：公开配置、动画节点生命周期、骨骼缓存、诊断和结果输出。
- `AimIKSolver`：不依赖编辑器模块的组件空间 CCD 求解。
- `AimIKBoneHierarchy`：集中处理瞄准源与链尖端之间的层级判断。
- `AnimGraphNode_AimIK`：节点菜单、显示信息和编译期配置警告。

## 安装

1. 将 `BBBAimIK` 目录复制到工程的 `Plugins/` 下。
2. 在 `.uproject` 的 `Plugins` 数组中启用 `BBBAimIK`。
3. 重新生成工程文件并编译 Editor Target。

只有在其他 C++ 模块直接引用 `FAnimNode_AimIK` 时，才需要在该模块的 `.Build.cs` 中添加：

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    "BBBAimIK"
});
```

不要在游戏 Runtime 模块中依赖 `BBBAimIKEditor`。

## 动画蓝图配置

### 1. 添加节点

1. 打开角色的 Animation Blueprint。
2. 进入 `AnimGraph`。
3. 在需要进行上半身瞄准的位置右键。
4. 搜索 `Aim IK`。
5. 如果搜索结果中存在同名节点，选择分类为 `BBB > IK`、标题为 `Aim IK` 的节点。
6. 将上游 Pose 接到节点左侧的组件空间 Pose 输入，再将节点输出接到后续节点或 `Final Animation Pose`。

该节点继承自 Skeletal Control。若当前图中使用的是 Local Space Pose，需要按动画蓝图提示连接 `Convert Local to Component Space` 和 `Convert Component to Local Space`。

### 2. 配置 BoneChain

`BoneChain` 必须按照从根到尖端的骨骼层级排列。以五段脊柱为例：

| 顺序 | BoneName | Weight 示例 |
|---:|---|---:|
| 0 | `spine_01` | `0.2` |
| 1 | `spine_02` | `0.3` |
| 2 | `spine_03` | `0.5` |
| 3 | `spine_04` | `0.7` |
| 4 | `spine_05` | `0.8` |

然后配置`AimSourceBoneName`, 它的语义是“驱动骨骼链的最终目标骨骼”，比如你弯腰捡东西，最终是为了将手（Hand_r）伸到目标上,
在对于正常的右手持枪场景下，设置为hand_r即可。
`AimSourceBoneName` 必须是BoneChain的末骨骼自身或其后代。

### 3. 连接输入

在节点详情面板中，将需要动态驱动的属性暴露为引脚，然后按类型连接：

| 输入引脚 | 类型 | 连接内容 | 人话 |
|---|---|---|---|
| `Aim Source Local Transform` | Transform | 瞄准源相对 `AimSourceBoneName` 的稳定局部绑定 | 枪口socket对于hand_r的偏移 |
| `Aim Target` | Vector | Skeletal Mesh 组件空间中的目标位置 | 瞄准目标点对于角色mesh组件空间的坐标 |
| `Has Valid Aim Target` | Boolean | 当前目标是否有效 | 开关 |
| `Alpha` | Float | 整个 Skeletal Control 的混合权重 | 瞄准偏移的影响程度 1为完全叠加偏移结果 |

如果目标来自世界空间，先使用当前 Skeletal Mesh 组件的逆变换把目标位置转换到组件空间，再写入动画实例变量。不要把世界空间位置直接连接到 `Aim Target`。

### 4. 配置瞄准轴(一般保持默认即可)

- `AimAxis` 是瞄准源局部空间中指向前方的轴，默认是 `X+`。
- `PoleAxis` 是瞄准源局部空间中用于约束翻转的参考轴，默认是 `Z+`。
- `PoleTarget` 和 `AimTarget` 一样使用组件空间。
- 不需要极轴约束时，将 `PoleWeight` 保持为 `0`。

## 参数说明

### Aim

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `AimSourceBoneName` | `None` | 承载虚拟瞄准源的骨骼 |
| `AimSourceLocalTransform` | Identity | 瞄准源相对承载骨骼的稳定局部变换 |
| `AimAxis` | `(1, 0, 0)` | 应朝向目标的局部轴 |

### Solver

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `AimTarget` | `(0, 0, 0)` | 组件空间目标位置 |
| `bHasValidAimTarget` | `false` | 显式目标有效标志，零向量仍可作为合法目标 |
| `MaxIterations` | `4` | 每次评估的最大 CCD 迭代次数 |
| `Tolerance` | `0` | 角度收敛阈值，`0` 表示不提前停止 |

### Safety 与 Clamp

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `bEnableMinTargetDistanceGuard` | `true` | 是否跳过距离过近的目标 |
| `MinTargetDistance` | `30` | 允许求解的最小组件空间距离 |
| `ClampWeight` | `0.1` | 对接近反方向的目标进行钳制的强度 |
| `ClampSmoothing` | `2` | 钳制曲线的平滑次数 |

### Pole 与 Debug

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `PoleAxis` | `(0, 0, 1)` | 瞄准源局部极轴 |
| `PoleTarget` | `(0, 0, 0)` | 组件空间极轴目标 |
| `PoleWeight` | `0` | 极轴纠偏权重 |
| `bEnableDebugLogging` | `false` | 输出初始化、提前退出、姿态跳变和求解采样日志 |
| `DebugSolveLogInterval` | `60` | 求解日志采样间隔 |

动画评估可能运行在工作线程。调试日志仅用于定位问题，生产环境应保持关闭。

## 编译期检查

动画蓝图编译时会检查：

- `BoneChain` 是否为空。
- `AimAxis` 是否为零向量。
- `AimSourceBoneName` 是否已设置并存在于 Skeleton。
- 骨骼链中的名称是否存在于 Skeleton。
- 瞄准源是否为链尖端自身或其后代。

运行时初始化会使用同一层级判断再次防呆，骨骼配置无效时不会输出部分求解结果。

## 兼容性

- Unreal Engine：5.6+
- 插件内容：纯 C++，`CanContainContent` 为 `false`
- Runtime 模块：`BBBAimIK`
- Editor 模块：`BBBAimIKEditor`，类型为 `UncookedOnly`
