# 精确矿点与基础矿量设计

## 目标

在不改变现有默认扫描语义的前提下，为 `dspugen` 增加按行星、按矿脉类型统计真实矿点数和基础矿量的能力，并由 `DSPSeedCalc` CLI 以默认关闭的可选开关启用。

本设计采用以下已经确认的语义：

- 现有 `Planet::veinSpot[15]` 继续表示廉价的矿组估计，不改变字段含义。
- 精确结果同时包含每类矿脉的实际矿点数和实际加入矿脉数据的基础矿量。
- 基础矿量固定采用普通资源模式；它包含主题 `VeinOpacity`、恒星资源系数、出生星球修正、油井平方根修正、固定 `1.1f` 和第一次 `Mathf.RoundToInt`，不包含 `resourceMultiplier`、`oilAmountMultiplier` 或无限资源覆盖值。
- 精确开关自动包含行星生成，但不改变主 `seeds.csv` 格式。插件按需读取精确结果。
- 首版只实现 CPU 权威生成。GPU 只记录为后续非权威实验方向，不进入本次验收。

## 参考构建与逆向证据

参考游戏通过注册表和 Steam 清单定位，不启动游戏：

- `HKCU:\Software\Valve\Steam` 的 `SteamPath` 为 `d:/steam`。
- `D:\Steam\steamapps\libraryfolders.vdf` 的库路径为 `D:\Steam`，应用列表包含 `1366540`。
- `D:\Steam\steamapps\appmanifest_1366540.acf` 的 `installdir` 为 `Dyson Sphere Program`，Steam build ID 为 `23109513`。
- 推导出的游戏路径为 `D:\Steam\steamapps\common\Dyson Sphere Program`。
- 游戏版本为 `0.10.34.28529`；`Updates\Versions.txt` 的最后记录日期为 2026 年 5 月 6 日。
- Unity 为 `2022.3.62f3c1 (1623fc0bbb97)`，脚本后端为 Mono。
- `Assembly-CSharp.dll` SHA-256 为 `AE0BA95F75BD879A62AA4CE253B2AB78EAA4FB3C7C595F5E1FEE75EBE0E0EF85`。
- `Assembly-CSharp.dll` MVID 为 `ece4a40e-5e73-43f4-a9f8-4e74970b5942`。
- `UniverseGen.algoVersion` 为 `20200403`。
- `ThemeProtoSet.json` 的 SHA-256 为 `4C41A63256EBE6CFD08F5B0EDCBF6C2F3820CD76E89A75D779CDEE31A3D005D6`，当前原型的 `Algos` 并集为 `0–13`。
- `VeinProtoSet.json` 的 SHA-256 为 `B77D5C58BEC72BDADDB368140AE4D5B4B18C69528A30B7B447BA563DE9155DFD6`。

静态反编译输出位于：

- `C:\Users\Soar\AppData\Local\Temp\dsp-decompile-0.10.34.28529-build23109513`
- `C:\Users\Soar\AppData\Local\Temp\dsp-il-0.10.34.28529-build23109513`

核心调用顺序为：

```text
UniverseGen.CreateGalaxy
  → StarGen.CreateStarPlanets
  → PlanetGen.CreatePlanet
  → PlanetGen.SetPlanetTheme
  → PlanetModelingManager.Algorithm(algoId).Reset
  → PlanetRawData(precision)
  → CalcVerts
  → GenerateTerrain(mod_x, mod_y)
  → GenerateVeins
  → SummarizeVeinGroups
```

`GenerateVegetables` 不纳入精确矿量路径。反编译确认植被使用独立的行星种子随机流；矿脉重新从 `planet.seed` 建立随机流，植被删除不会改变矿脉随机消费。

基础矿量取自游戏 `PlanetAlgorithm.GenerateVeins` 中第一次写入 `VeinData.amount` 的值：

```text
Mathf.RoundToInt((float)rng.Next(minAmount, maxAmount) * 1.1f)
```

它位于应用游戏资源倍率之前。`Mathf.RoundToInt(float)` 在当前 Unity 核心库中使用中点取偶数，C++ 实现必须继续使用现有的 `util::mathfRoundToInt`，不能替换为 `std::round`。

## 组件与数据流

### `PlanetRawData` 层

新增独立模块，负责：

- 精度 `200` 的球面顶点缓存和索引映射；
- 每颗行星的 `heightData` 与 `biomoData` 工作缓冲；
- 游戏兼容的高度写入、截断、夹紧和 `QueryHeight`；
- 油井所需的 `PlanetAuxData.RawSnap` 等局部采样辅助。

单位球顶点缓存可在线程内复用；行星高度和生物群系缓冲不能跨行星复用未清理内容。所有浮点运算必须保留游戏的 `double`、`float` 和显式转换边界。

### 地形算法层

新增基础调度和 `PlanetAlgorithm0–13` 实现。算法 `1–13` 按游戏方法逐一移植，避免把算法公式合并成近似的通用噪声流程。算法 `7`、`11`、`12`、`13` 的矿脉生成存在专用覆盖，必须保留独立分支；算法 `14` 在当前原型未引用，首版不实现，遇到引用时报告明确错误。

### 矿脉汇总层

新增矿脉生成模块，保留游戏的以下顺序：

1. `DotNet35Random(planet.seed)` 前缀消费和第二随机流种子。
2. 恒星类型、光谱和稀有矿配置处理。
3. 矿组数量抖动。
4. 候选位置拒绝循环、地形高度检查和矿组间碰撞检查。
5. 矿组内部节点扩展、模型选择和节点数量随机值。
6. 高度／水面剔除后累计节点数和基础矿量。

生成器只保留当前算法需要的向量、临时节点和地形工作区，不保存完整 `VeinData` 数组、模型、碰撞体或渲染数据。

### `Planet` 状态

在 `Planet` 对象尾部增加惰性精确结果 sidecar，避免默认扫描永久携带固定的 `15 × 64` 位数组：

```cpp
struct VeinAmountSummary {
    std::uint32_t nodeCount[15];
    std::int64_t baseAmount[15];
};
```

内部提供 ready 状态和只读访问。对象池 `reset()` 必须使 ready 状态失效，并清除或完整覆盖 sidecar 内容。为算法 `7`、出生行星路径补齐的轨道、旋转、`mod_x`、`mod_y` 等字段只保存生成所必需的值，不改变现有随机调用顺序。

### Galaxy 入口

增加幂等接口：

```cpp
Galaxy::ensureAllVeinAmounts();
Planet::ensureVeinAmounts();
```

`Galaxy::ensureAllVeinAmounts()` 先确保行星拓扑和主题已经按原顺序完成，再按恒星、行星顺序调用单行星生成。气态行星返回 ready 的全零汇总，不构造地形缓冲。

## CLI 与插件接口

新增 `-V`／`--vein-amounts`：

- 默认关闭；启用时设置 `hasPlanets`，因此自动包含 `-p`。
- 与 `-P`、`-Z` 冲突并报告参数错误。
- 与 `-b` 兼容。
- 不打开额外的宿主 CSV，不改变 `seeds.csv` 的两列表头和命中行。

插件需求新增：

```cpp
NeedsVeinAmounts = 1ULL << 7U
```

现有 `NeedsVeinMetadata` 保持廉价 `veinSpot` 语义，不升级为精确数量。

为避免新插件在旧宿主上访问扩展后的 `Planet` 布局，新增带 `structSize` 和 `abiVersion` 的 `PluginAPIV2` 及 `init3`。旧宿主继续处理 `init`／`init2`；只导出 `init3` 的新插件在旧宿主上会被跳过，而不是越界运行。V2 API 提供：

- 显式生成全部行星精确结果的函数；
- 从 `Planet` 获取只读汇总的函数；
- 与现有需求位配合的描述结构。

插件声明 `NeedsVeinAmounts` 但 CLI 未启用时，主程序在扫描前报告错误并退出，避免插件目录中的普通文件意外触发慢路径。宿主仍按预过滤、正式过滤、格式化输出三个阶段满足需求位；插件只能在对应阶段读取已经 ready 的汇总。

## 错误和资源处理

- 不支持的 `algoId`、缺失原型字段或原型快照不匹配时，报告 seed、行星 ID、算法 ID 和原型哈希，并停止扫描。
- sidecar 仅在单行星生成完整成功后标记 ready；中途失败不会暴露部分数组。
- 总量用 `std::int64_t`，节点数用 `std::uint32_t`；累加前检查溢出。
- 线程局部的地形、噪声和矿脉临时对象随 worker 释放；不把行星工作区写入异步输出队列。
- 默认路径不分配 sidecar、不生成地形、不执行精确矿脉随机流。
- 新增字段只能追加在公开对象尾部；不得插入现有插件直接读取的成员之间。

## 验证计划

### TDD 单元与集成测试

按失败测试先行实现以下行为：

1. `VeinAmountSummary` 的普通模式基础量、油井基础量和实际节点数累计。
2. 水面剔除、出生星球、算法 `7/11/12/13` 专用高度条件。
3. `Mathf.RoundToInt` 中点取偶数和 `ushort` 截断边界。
4. 默认关闭时现有 oracle 摘要逐字节不变，且精确 sidecar 不可用。
5. 对象池先生成 seed A、释放，再生成 seed B；未请求精确结果时不会读取 A 的数据。
6. `-V` 自动包含 `-p`，与 `-P`／`-Z` 冲突，插件未启用开关时拒绝扫描。
7. V1 旧插件继续加载；V2 插件可声明需求并读取汇总；旧宿主不会加载仅导出 `init3` 的插件。

### 固定种子夹具

新增独立的精确矿量 oracle 输出，覆盖：

- 算法 `0–13`；
- 出生星球与非出生星球；
- 白矮星、中子星、黑洞的稀有矿分支；
- 油井、水面剔除和四个专用矿脉覆盖；
- 对象池复用和多线程顺序稳定性。

游戏运行时禁止启动，因此首版的游戏侧证据来自记录的 DLL、IL 和原型快照。为减少手工移植误差，测试应保留一份从反编译方法和 Unity 数学语义构建的独立参考夹具；不得把当前 C++ 实现自身再次调用作为唯一 oracle。

每次生成器改动都执行：

- Debug 构建，`BUILD_VIEWER=OFF`；
- `-t 1` 固定种子夹具；
- Release 构建并重复夹具；
- GCC 与 Clang 算术一致性；
- 默认关闭模式与现有 `tests/fixtures/oracle-v1.txt`、`galaxy_digest` 摘要比较；
- 新增精确模式的节点数、基础矿量和插件接口检查。

### 性能记录

性能测试记录编译器、构建类型、线程数、seed 区间、星数、插件集合、原型快照、耗时和 seeds/s。默认关闭模式必须保持现有扫描结果；精确模式单独报告地形与矿脉成本，不以默认模式的性能指标代替。

## GPU 边界

静态逆向表明，权威生成在 CPU 的行星计算线程中完成；GPU instancing 只消费生成后的矿脉数据。地形顶点之间具有并行性，但地形代码同时使用 `double`、`float`、超越函数、FMA 相关行为和整数截断，跨设备无法默认保证位级一致。矿脉候选使用串行 `DotNet35Random` 和拒绝采样，GPU 并行化会改变随机消费或节点顺序。

因此首版不引入 OpenCL、CUDA 或其他 GPU 运行时依赖。后续若开展实验，只允许 GPU 生成非权威地形预筛，最终由 CPU 重新计算并验证；不得让 GPU 结果直接成为精确矿量输出。

## 非目标

- 完整地形渲染、植被、碰撞体、模型实例化和物理数据。
- 长期保存每个矿点的位置、组索引和模型属性。
- 游戏资源倍率、原油倍率和无限资源覆盖值的最终存档级模拟。
- 当前原型未引用的算法 `14`。
- 修改现有 `seeds.csv` 结构或新增宿主级矿量 CSV。
