# DSPSeedCalc 性能优化计划

## 目标与不可变约束

本计划面向大规模种子扫描。允许重构数据结构、线程调度、插件宿主和中间计算流程，但任何优化都必须满足以下条件：

- 对同一游戏版本、Prototype 快照、算法版本、种子和星数，生成结果与原游戏完全一致。
- 随机数调用的数量、顺序、种子派生和 `double` / `float` 转换点不变。
- 恒星、行星的生成顺序不变；`HabitableCount`、主题去重和主题候选顺序不变。
- Unity 2022 Mono 的 `Mathf` 精度路径、舍入规则和表达式顺序不变。
- 不使用 `-ffast-math`、`/fp:fast`、近似数学函数、未验证的 FMA、SIMD 近似或现代随机数生成器。
- 地形、矿物、viewer 不纳入本阶段；`Planet::generateVeins()` 只保留随机消费和现有 API 语义。
- 每一项优化必须能够回退，并通过固定种子 oracle 后才能合入下一阶段。

当前参考版本：Dyson Sphere Program `0.10.34.28529`，Steam Build ID `23109513`，Unity `2022.3.62f3c1`，Mono，`UniverseGen.algoVersion = 20200403`。程序集指纹和构建约束见 `AGENTS.md`。

## 已知基线

当前主要阶段：

```text
输入范围
  -> 位置生成 RandomPoses / GenerateTempPoses
  -> Galaxy / Star 生成
  -> 可选 Planet 生成
  -> 插件过滤
  -> 输出
```

复杂度近似：

```text
Tseed = Tposes + S * Tstar + optional(Q * Tplanet)
      + Tfilters + Toutput
```

其中：

- `S` 是星数。
- `Q` 是行星总数。
- `Tposes` 当前接近 `O((4S)^2)`，因为每个候选位置都和既有位置做全量碰撞检测。
- `Tplanet` 包含 Unity 兼容的 `pow`、`log`、`sqrt`、主题线性扫描和容器分配。
- `Toutput` 在高命中率时由全局互斥锁串行化。

阶段差分基准：

- `-P`：位置阶段。
- 完整恒星模式减去 `-P`：恒星阶段近似成本。
- `-p` 减去完整恒星模式：行星阶段近似成本。
- `-n` 减去完整恒星模式：名称阶段近似成本。
- `-Z`：不生成位置时的恒星基线。

所有基准必须使用隔离运行目录、匹配的 `Prototypes`、单一目标插件和 `-t 1` 的正确性参数。插件宿主会加载 `filters` 目录下所有普通文件，不能把多个筛选插件放在同一基准目录中。

## 验证门禁

每个优化阶段都必须执行以下门禁：

### 门禁 A：构建

- GCC/UCRT64 Debug，viewer 关闭，C++17。
- GCC/UCRT64 Release，保留现有 LTO 和 strict-FP 选项。
- MSYS2 Clang64 Release，`C:\msys64\clang64\bin` 在 `PATH` 中。
- 记录编译器版本、构建类型、编译选项和 Prototype 快照。

### 门禁 B：固定 oracle

至少比较以下情况：

| 类别 | 种子 | 星数 | 模式 | 字段 |
|---|---:|---:|---|---|
| RNG | `0`、`1`、`12345678` | - | 直接调用 | 前 100 个 `Next` / `NextDouble` 位模式 |
| 位置 | `0`、`12345678` | 64 | `-P` | 星数、顺序、每个坐标 IEEE-754 位模式 |
| 恒星 | `0`、`12345678` | 64 | 完整恒星 | seed、type、spectr、质量、寿命、温度、光度、半径、资源系数 |
| 行星 | `0`、`12345678` | 64 | `-p` | 行星数、种子、轨道层级、类型、奇异状态、主题、算法 ID |
| 气体 | `0`、`12345678` | 64 | `-p` | gas item、gas speed 位模式 |
| 名称 | 固定名称样例 | 64 | `-p -n` | 名称和重复重试结果 |

整数必须完全相等；严格字段的浮点数必须比较 IEEE-754 位模式，而不是十进制文本或近似误差。

### 门禁 C：随机调用审计

对重构前后增加临时 RNG 计数器或 seed trace，确认：

- 每层 `DotNet35Random` 的构造 seed 相同。
- 每次 `next()` / `nextDouble()` 的调用点和顺序相同。
- 被省略的字段仍然消费原游戏需要的随机数。
- 任何提前过滤都不会跳过原本必须影响后续保留字段的随机消费。

### 门禁 D：性能

- 每个 case 预热 1 次，正式运行至少 7 次。
- 报告中位数和 MAD，不只报告最快一次。
- 同时记录 wall time、CPU time、处理种子数、每秒种子数、峰值 RSS 和输出字节数。
- 至少覆盖 16、32、64、96、128、256 星；线程数覆盖 1、8、16、24、31、32。
- 记录插件集合、命中率、是否生成名称、是否生成行星和当前工作目录。

任何 oracle 失败都必须停止性能优化，先定位 parity 差异。

## 阶段 0：建立可重复性能基准

状态：待实施。

### 工作项

1. 增加独立 benchmark 文档或脚本，不把 benchmark 逻辑混入生成器热路径。
2. 固定运行矩阵：
   - `-P`、完整恒星、`-Z`、`-b`、`-b -Z`、`-p`、`-n`。
   - 无插件、恒定拒绝插件、恒星过滤插件、行星过滤插件、高命中输出插件。
   - 16、32、64、96、128、256 星。
3. 增加可选的只读 instrumentation：
   - 位置候选数、单位球拒绝数、碰撞拒绝数、碰撞比较次数。
   - 内存池首次分配次数和高水位。
   - 行星数、主题扫描次数、Prototype 查找次数。
   - 各类插件回调次数和累计时间。
4. 不改变生成结果，不改变 RNG 调用，不改变默认输出。

### 验收

- instrumentation 开关关闭时，和当前 build 的 oracle 完全一致。
- 能够生成一张按阶段和星数分组的基准表。
- 后续每项优化均可复用同一矩阵。

## 阶段 1：低风险计算和容量复用

状态：待实施。建议优先完成。

### 1.1 删除无效行星累计

`Planet::create()` 当前遍历之前恒星的行星数量并写入未使用的局部变量。删除该循环，不改变任何字段、分支和 RNG 调用。

位置：`dspugen/planet.cc` 行星创建开头。

风险：低。需要通过完整行星 oracle。

### 1.2 复用恒星的相同数学结果

恒星生成中 `mathfPow(1.7f, p)` 同时用于 `habitableRadius` 和 `lightBalanceRadius`。只计算一次并保存同一个 `float` 结果。出生星路径同样处理。

约束：不能改变 `float` 转换点，不能把相邻表达式重新结合，不能把结果改成新的数学实现。

风险：低。结果应为同一位模式；仍需 Debug、GCC Release、Clang Release 验证。

### 1.3 移出气体循环中的不变量

`mathfPow(resourceCoef, 0.3f)` 对同一颗行星的所有气体条目相同。移到循环外，保留每个气体条目的随机值转换和逐步 `float` 赋值。

目标形式：

```cpp
float resourceScale = util::mathfPow(resourceCoef, 0.3f);
for (...) {
    float speed = themeProto->gasSpeeds[i];
    speed *= float(random.nextDouble()) * 0.19090915f + 0.9090909f;
    gasSpeeds[i] = speed * resourceScale;
}
```

风险：低到中。必须证明同一 `mathfPow` 结果只依赖当前 `resourceCoef`，并检查编译器没有重排严格浮点表达式。

### 1.4 降低内存池空闲指针预留

`MemPool<T, 65536>` 对 Galaxy、Star、Planet 统一预留过大的指针容量。改为按实际高水位增长，或提供类型级别的低默认容量。

约束：不改变对象构造和析构顺序，不跨线程共享池，不改变 pool LIFO 语义。

风险：低。检查长时间扫描中的峰值 RSS、首次分配次数和 oracle。

### 1.5 复用位置 scratch 容量

将 `Galaxy::create()` 中每个种子创建的局部 `tmpPoses` / `tmpDrunk` 改为线程局部 scratch，种子之间只 `clear()`。

约束：

- 每次调用开始都清空内容。
- 不能复用跨线程容器。
- 保持 `tmpPoses` 和 `tmpDrunk` 的元素顺序。
- 不改变 `GenerateTempPoses` 的删除语义。

风险：低到中。重点测试不同星数分组和异常返回路径。

## 阶段 2：优化 RandomPoses 的碰撞检测

状态：待实施。预计收益最高，但必须隔离验证。

### 2.1 先做严格轴向排除

在计算完整平方距离之前，使用严格的轴向条件排除：当 `abs(dx) >= minDist` 时，候选不可能满足平方距离 `< minDist²`。对 `dy`、`dz` 也可采用同样规则。

约束：

- `minDist` 仍是 `double`。
- 使用与原实现相同的 `<` / `>=` 边界。
- 只允许提前判定不可能碰撞，不允许改变接受条件。

风险：低。使用边界构造样例和完整位置 oracle。

### 2.2 使用均匀网格 broad phase

使用边长为 `minDist` 的均匀网格保存已接受点索引。候选只访问自身及相邻 26 个 cell，再使用原始双精度平方距离进行精确判断。

推荐流程：

1. 点被接受后，按其原始 `VectorLF3` 坐标插入 cell。
2. 候选只查询相邻 cell。
3. cell 内索引按插入顺序保存，尽量保持与旧全量扫描相同的点访问顺序。
4. 仍使用 `dx * dx + dy * dy + dz * dz < sqrDist`。
5. 不对候选进行排序，不并行生成候选，不使用近似距离。

需要特别处理：

- 负坐标到 cell 的 floor 语义。
- 坐标恰好落在 cell 边界的情况。
- 旧全量扫描的访问顺序和早停行为。
- `tmpPoses` 和 `tmpDrunk` 的原始顺序。

风险：中。虽然 broad phase 理论上只删除不可能碰撞的点，但索引错误会改变星位序列，必须对大量 seed 和多星数验证。

### 2.3 线性 compact 替代逆序 erase

`GenerateTempPoses()` 当前反复 `vector::erase`。改成等价的原地 compact，但必须保留「当临时点不足 `targetCount` 时，删除循环提前停止」的语义，不能简单只保留下标为 4 的倍数。

验收重点：

- `RandomPoses` 未达到 `targetCount * 4` 的失败路径。
- `targetCount` 为 0、1 和大星数。
- 返回点数量和每个坐标位模式。

风险：中。

## 阶段 3：按需生成和插件阶段拆分

状态：待设计。预期高淘汰率扫描收益最高，API 风险较大。

### 3.1 需求位协议

为插件增加明确的需求描述，例如：

```text
NeedsPositions
NeedsStars
NeedsNames
NeedsPlanetTopology
NeedsPlanetTheme
NeedsGas
NeedsVeinMetadata
```

保留旧插件回退为「需要完整当前行为」，避免旧 ABI 被静默误判。

### 3.2 两阶段过滤

推荐顺序：

```text
生成位置和恒星
  -> galaxy / star filters
  -> 仅对仍可能通过的星系生成行星
  -> planet filters
  -> gas / output
```

约束：

- 不能只生成后部某一颗行星而跳过前序行星。
- 如果后续字段依赖 `HabitableCount`、主题去重或前序 RNG，必须完整执行前序生成。
- 被早期恒星过滤淘汰的星系可以安全省略行星阶段，因为它不会产生输出。
- 旧插件调用 `GenerateAllPlanets` 时保持当前懒生成语义。

风险：高。需要设计插件兼容层和多种插件组合的 oracle。

### 3.3 修正宿主的并发状态

在性能优化前处理以下问题，否则基准会被错误行为污染：

- `FilterSet::userp` 不应由多个 worker 覆盖同一个全局字段；改成每次调用的局部上下文或线程局部上下文。
- 修正 `hasPlanetFilter` 的累积逻辑。
- 检查纯星体过滤路径中 `pass` 的每颗恒星初始化语义。

这些修复不应改变生成器本身，但可能改变错误插件的结果，需要记录为宿主行为修复并单独验证。

## 阶段 4：行星和 Prototype 热路径

状态：待实施。需要大量行星 fixture。

### 4.1 缓存恒定值

- 每个 Galaxy 的 `ceil(starCount * 0.29f)`。
- 每颗 Star 的 `sqrt(habitableRadius)`。
- 每颗气态行星的 `mathfPow(resourceCoef, 0.3f)`。

缓存必须保存原始 `float` 结果，且不能跨不同星、星系或 seed 误用。

### 4.2 主题候选预索引

在 Prototype 加载阶段预构建按 planet type、distribution 分类的候选索引，但每个分类内部必须保持原 `dataArray` 顺序。

运行时仍按原顺序执行：

- 温度条件。
- 出生星和普通星分布条件。
- 同恒星主题去重。
- Desert fallback。
- `rand1 * candidateCount` 的取模。

风险：中到高。主题 ID、候选数量或顺序变化都会改变 seed 结果。

### 4.3 Dense Prototype lookup

Theme ID 范围稳定时，可用连续索引表替代高频 `unordered_map` 查找。保留缺失 ID 返回 `nullptr` 的行为，不能改变 Prototype 数据顺序。

风险：中。必须覆盖所有现有主题 ID、无效 ID 和 Prototype 变体。

## 阶段 5：对象生命周期和分配

状态：待测量后实施。

当前对象池只复用对象存储，析构时会释放成员 `vector` / `string` 容量。可选方案：

1. 将高频 scratch 从对象移到线程局部结构。
2. 在 pool release 时保留容量并显式 reset 所有字段。
3. 使用线程局部 arena/slab 管理对象和成员缓冲区。

推荐先采用方案 1，再评估方案 2。方案 2 必须完整清零或重置：

- `Galaxy::stars`。
- `Star::planets`、`Star::name`。
- `Planet::gasItems`、`Planet::gasSpeeds`。
- 所有指针、枚举、seed、singularity、theme、algoId、vein 数组和公开字段。

严禁依赖 placement-new 后的默认成员初始化来恢复已保留容量的容器状态，除非有明确的 reset 代码和跨 seed fixture。

## 阶段 6：调度、插件状态和输出

状态：待实施。适合在计算热点降低后处理。

### 6.1 分块取种子

将每个 seed 一次获取的互斥锁改为原子 `fetch_add` 分配连续小块，例如 32～256 个 seed。跨区间时走慢路径。

约束：

- 生成结果不依赖处理顺序。
- 插件有副作用或统计顺序时，必须通过兼容模式保留旧调度。
- 记录 seed block 以便输出和失败重放。

风险：中。对纯过滤插件低风险，对有共享状态插件中风险。

### 6.2 插件线程局部归并

增加 `threadInit` / `threadFinish` 或等价机制，让插件在 worker 内累计状态，最后单线程归并。旧 ABI 使用锁兼容。

### 6.3 异步输出

将命中结果复制为已格式化的拥有数据，送入单 writer 线程；不能在 Galaxy 归还对象后继续引用对象内部字符串或容器。

如果要求稳定顺序，给 seed 分配序号并在 writer 重排；否则保持当前完成顺序语义，避免额外缓存。

## 阶段 7：编译器和 PGO

状态：最后实施。

可测试：

- Clang/GCC PGO。
- 保留 `-fno-fast-math -ffp-contract=off` 的 LTO。
- `-march=native` 仅作为单机 benchmark 配置，不作为默认发布配置。

训练集必须覆盖不同星数、`-p`、`-n`、命中率和插件类型。PGO 后必须重新运行完整 oracle 和 GCC/Clang 交叉验证。

禁止：

- `-Ofast`。
- `-ffast-math`。
- 改用 `powf` / `logf` / `sqrtf` 替代 Unity 2022 兼容包装。
- 近似 LUT 或不具备 bit-level 证明的向量数学。

## 推荐实施顺序

1. 阶段 0：建立基准和 instrumentation。
2. 阶段 1：低风险数学复用、无效循环删除、scratch 和池容量复用。
3. 阶段 2：RandomPoses 轴向排除、网格 broad phase、线性 compact。
4. 阶段 3：插件需求位和两阶段行星生成。
5. 阶段 4：主题候选和 Prototype 查找优化。
6. 阶段 5：成员容器容量保留和线程局部 arena。
7. 阶段 6：分块调度、插件线程局部归并、异步输出。
8. 阶段 7：PGO 和编译器特化。

每个阶段独立提交。提交前必须同时给出：

- 变更的 parity 风险说明。
- RNG 调用审计结果。
- Debug / GCC Release / Clang Release oracle 结果。
- 性能矩阵的中位数、MAD 和硬件/编译器信息。
- 未完成的场景和回滚方式。

## 不接受的优化

以下方案即使性能收益明显，也不接受：

- 改变任何随机数调用顺序或删除「看起来没有使用」的调用。
- 将 Unity `Mathf` 或 `System.Math` 运算改成未经验证的单精度/近似版本。
- 改变候选位置、恒星、行星的生成顺序。
- 并行接受同一星系内的位置候选。
- 跳过前序行星后直接生成目标行星。
- 用近似距离、float 坐标或容差替代原始双精度碰撞判断。
- 以现代 `.NET Random`、`std::random` 或其他 RNG 替换 `DotNet35Random`。
- 通过修改 Prototype 数据或排序来掩盖生成逻辑差异。
