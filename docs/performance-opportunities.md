# 剩余性能优化机会

本文记录 `docs/performance-optimization-plan.md` 阶段 0–7 全部实施之后，CLI 宿主（`cli/`）与生成器库（`dspugen/`）中的优化机会，以及本轮实施结果。该计划的不可变约束与验证门禁在本文中继续生效，不再重复：随机调用数量与顺序不变、`double` 到 `float` 转换点不变、生成顺序不变、禁止 `-ffast-math` 与近似数学、禁止替换 `DotNet35Random`。

每一项都标注收益依据与实测结果。**通过实测被推翻或被排除的候选项集中在最后两节**，避免后续重复评估。

## 测量环境

- 构建：`benchmark` preset（MSYS2 UCRT64 GCC 16.1.0，Release，LTO，`-fno-fast-math -ffp-contract=off`）。
- 计数：`gcc-release-instrumented` preset，`DSP_INSTRUMENTATION_OUTPUT` 输出 JSON。
- 机器：Intel64 Family 6 Model 183（Raptor Lake，大小核混合），32 逻辑线程，Windows 11。
- Prototype 快照：仓库 `Prototypes/`。参考游戏版本见 `AGENTS.md`。
- 计时来源：程序自身 stderr 的 `Nms used` 计数，不含进程启动与 Prototype 解析。
- 插件：`reject_galaxy`（常量拒绝，声明完整描述符），单线程除特别说明外。

## 实施结果

单线程、每 seed 微秒，与本轮开始前的基线对比：

| 星数 | 模式 | 优化前 | 优化后 | 提升 |
|---:|---|---:|---:|---:|
| 64 | `-P` 仅位置 | 186.7 | 66.2 | 2.82× |
| 64 | 位置 + 恒星 | 224.9 | 89.3 | 2.52× |
| 64 | `-Z` 恒星 | 36.7 | 21.1 | 1.74× |
| 64 | `-b` 仅出生星 | 187.3 | 67.2 | 2.79× |
| 64 | `-n` 名称 | 270.7 | 109.3 | 2.48× |
| 64 | `-p` 行星 | 387.6 | 174.4 | 2.22× |
| 256 | `-P` 仅位置 | 670.8 | 260.0 | 2.58× |
| 256 | 位置 + 恒星 | 827.0 | 369.9 | 2.24× |
| 256 | `-Z` 恒星 | 145.9 | 88.7 | 1.65× |
| 256 | `-b` 仅出生星 | 668.5 | 268.4 | 2.49× |
| 256 | `-n` 名称 | 1040.7 | 470.0 | 2.21× |
| 256 | `-p` 行星 | 1493.2 | 728.4 | 2.05× |

多线程扩展性（64 星，10 万 seed，`reject_galaxy`）：

| 线程 | 优化前 | 优化后 |
|---:|---|---|
| 1 | 11383 ms | 9035 ms |
| 8 | 1670 ms（6.8×） | 1192 ms（7.58×） |
| 16 | 1017 ms（11.2×） | 708 ms（12.76×） |
| 31 | — | 430 ms（21.01×） |

宿主命中路径开销（`-Z` 模式，0% 与 100% 命中之差，20 万 seed）：

| 线程 | 优化前 | 优化后 |
|---:|---:|---:|
| 1 | 2.43 µs/seed | 0.10 µs/seed |
| 31 | 0.55 µs/seed | 0.02 µs/seed |

## 关键单位成本

在判断任何一项优化是否值得实施之前，先用微基准测过这三个单位成本。**本轮最大的教训是：不测单位成本就排优先级会排错。**

| 操作 | 成本 | 微基准 |
|---|---:|---|
| `util::mathfPow`（本项目实际用到的底数与指数） | 8.4 ns | 500 万次调用，底数 64 个取值轮转，指数 9 个取值轮转 |
| `DotNet35Random` 构造（优化前） | 175 ns | 500 万次构造，减去 `nextDouble()` 基线 |
| `DotNet35Random` 构造（优化后） | 67 ns（GCC）/ 72 ns（Clang） | 同上 |
| `DotNet35Random::sample()` | 1.47 ns | 同一对象 500 万次 `nextDouble()` |

构造与采样相差 119 倍，这是整个优化方向的核心事实：**热点不在随机数的消费，而在随机数生成器的构造**。

## 已实施

### 1. `CheckCollision` 去掉邻域收集与排序

位置：`dspugen/galaxy.cc`。

原实现把 27 个 cell 的点索引全部拷进一个 scratch 向量、`std::sort`、再逐点判距。该函数的返回值是存在量词——「已接受点中是否存在与候选距离小于 `minDist` 的点」——与访问顺序无关，排序不影响返回值。改为遍历 cell 时直接判距并在命中时立即返回，同时删掉了 `PositionScratch::nearby`。

实测：位置阶段 +24%（5302 → 6596 seeds/s）。

副作用：instrumentation 的 `collisionComparisons` 语义从「候选邻域内的点数」变成「实际比较次数」，两个版本之间不可直接比较。

### 2. 扁平开放寻址位置网格

位置：`dspugen/galaxy.cc`（`GridSlot`、`ResetGrid`、`AddToGrid`、`cellHash`、`cellCoord`）。

原实现用 `std::unordered_map<GridCell, std::vector<std::size_t>>`。实测每 seed 做 18754 次 `find`，只换来 1328 次精确距离比较（14:1），并且每 seed 的 `grid.clear()` 会释放全部桶向量、下一个 seed 重新分配。

改为跨 seed 复用的开放寻址表：槽位保存**精确的 cell 坐标**（不是哈希值，因此两个不同 cell 不可能被混淆，哈希只决定探测顺序），点按下标链接在一个扁平 `int32` 数组里，世代戳取消了每 seed 清表。装填因子固定为 1/2，保证探测串一定终止在过期槽位。

实测：位置阶段再 +131%（6596 → 15244 seeds/s）。这两项合计把位置阶段从 186.7 降到 65.6 µs/seed。

### 3. `DotNet35Random` 构造去掉死初始化

位置：`dspugen/util/dotnet35random.hh`、`dotnet35random.cc`。

`int seedArray[56] = {}` 的零初始化是死工作。构造函数第一段循环写入下标 `21 * i % 55`，`i` 取 1..54；因为 `gcd(21, 55) = 1`，该式恰好取遍 1..54，再加上直接赋值的下标 55，即 1..55 全部在被读之前写过；下标 0 永远不被访问，因为 `sample()` 用前置自增且只回绕到 1。

实测：GCC 下构造函数从 74 条指令降到 55 条，`pxor`/`jb` 清零循环消失。

### 4. 拆分 scrambling 循环使其可向量化

位置：`dspugen/util/dotnet35random.cc`。**本轮收益最大的单项。**

参考实现是 `for (k = 1; k < 56; k++) seedArray[k] -= seedArray[1 + (k + 30) % 55];`。读下标可以按两段常量步长展开：`k <= 24` 时 `k + 30 < 55` 不回绕，读下标为 `k + 31`；`k >= 25` 时回绕，读下标为 `k - 24`。写成两个常量步长的循环后，`k` 的升序与每一次减法都保持不变，状态逐位相同，但编译器可以向量化——第一段只读本轮不写的 32..55，第二段的循环携带依赖距离为 24，宽于任何在用的向量宽度。

实测：构造从 175 ns 降到 67 ns（2.6×）。GCC 生成 `paddd`/`psubd`/`pand`，Clang 生成 52 条 `paddd` + 52 条 `psubd`。按 `-p` 模式每 seed 711 次构造计算，约省 77 µs/seed。恒星阶段 +15%，行星阶段 +46%。

### 5. 复用恒星层随机流，省掉每颗恒星一次构造

位置：`dspugen/star.hh`（`Star::planetGenSeed`）、`star.cc`。

`Star::createStarPlanets()` 原本用 `Star::seed` 重新构造生成器、丢弃三个 `next()`、用第四个作为行星生成器的种子。而 `createStar()` / `createBirthStar()` 已经用同一个 `seed` 构造过同一个生成器并取走前两个值。改为在恒星创建时多取两个 `next()`（约 3 ns）把第四个值存下来，`createStarPlanets()` 直接用它。`-p` 模式每 seed 省 64 次构造。

`planetGenSeed` **追加在现有成员之后**：插件内联这个头文件，在中间插入字段会移动 `planets` 与 `name` 的偏移，静默破坏所有已构建的插件 DLL。本轮实施时先把字段放在中间，立刻在 `galaxy_digest` 插件上复现出 segfault，改为追加后旧 DLL 无需重编即可继续工作。

### 6. 名称查重改哈希列表

位置：`dspugen/galaxy.hh`（`starNameHashes`、`hasStarName`、`addStarName`）、`galaxy.cc`、`namegen.cc`。

原实现对每个候选名字扫描 `galaxy->stars` 全量做字符串比较，整体 O(S²)。改为在 `Galaxy` 上维护与 `stars` 同序的名字哈希列表，先比哈希，命中时再比真实字符串——判定谓词完全相同，因此重试次数、生成序列与最终名字都不变。哈希碰撞不会改变结果。「XStar」兜底路径同样登记，因为原来的扫描也会看到已赋值的「XStar」。

实测：名称增量在 64 星从 22.6 降到 20.0 µs/seed（−11%），256 星从 118.6 降到 85.2 µs/seed（−28%）；星数倍率从 5.25 降到 4.26。

### 7. `roundToEven` 换 `std::nearbyint`

位置：`dspugen/util/maths.hh`。

`std::nearbyint` 按当前舍入模式取整，本项目中恒为 round-half-to-even：没有任何代码调用 `fesetround`，且 `tests/oracle.cc:145` 已经断言 `std::fegetround() == FE_TONEAREST`。因此它与原来的 `isfinite` + `floor` + 分数比较 + `fmod` + `copysign` 序列逐位等价，包括原代码逐个特判的符号零、非有限值和 `|x| >= 2^52`。

验证：独立穷举测试比对两种实现的 IEEE-754 位模式，覆盖 1220 万个取值（四分之一步长扫过 ±100 万、八分之一步长扫过 ±12500、0.001 步长扫过 ±1000、生成器实际会取整的 `luminosity * 100` 取值带，以及 ±0、±inf、NaN、`2^52` 边界、denormal、最大最小值），0 处不一致。

收益低于噪声（每 seed 约 243 次调用），保留的理由是 14 行特判换成 1 行标准库调用。

### 8. 宿主：需求位与派发列表外提

位置：`cli/main.cc`、`cli/filter.cc`。

- `prePlanetRequirements()` / `filterRequirements()` / `outputRequirements()` 原本每 seed 各调一次，值在 `loadFilters()` 之后固定，外提到循环外。
- `FilterInvocation` 与 `ownedOutput` 原本每 seed 各构造一次（各一次堆分配），改为循环外复用。
- `hasFinalFilter()` 与空回调判断原本在「恒星 × 行星 × 插件」三层循环内反复求值，改为装载期构建 `galaxyFilterList` / `starFilterList` / `planetFilterList` / `seedEndList` 四个紧凑派发数组。
- `runOutput` 原本无条件获取 `mutex2`，现在只在「需要串行化且确有 `output()` 回调」时获取；`found` 计数本身是原子操作，不需要锁。
- 当插件集要求串行化时，`ensureAllPlanets()` / `ensureAllGas()` 提到锁外执行。旧插件不声明需求位，这条路径对它们无变化；只有「描述符插件与旧插件混装」时才会生效，而那正是原来会把行星生成拖进全局锁的情形。

### 9. 宿主：命中行批量提交

位置：`cli/main.cc`。

每个命中一次 `enqueue`（一次 mutex 加一次条件变量通知）在生成变快之后成了新瓶颈：去掉输出锁之后，31 线程 100% 命中场景反而从 0.55 µs/seed 恶化到 3.22 µs/seed。改为在 worker 内累积命中行，按 16 KB 或 4096 个 seed 之一先到即刷出。用 seed 计数而不是只用字节阈值，是为了让低命中率扫描的结果仍然及时落盘，而不是攒在 worker 缓冲里直到扫描结束。

实测：命中路径开销 t=1 从 2.43 降到 0.10 µs/seed，t=31 从 0.55 降到 0.02 µs/seed。

### 10. 严格浮点选项作用域（parity 修复，非性能）

位置：`dspugen/CMakeLists.txt`、`cli/CMakeLists.txt`、`tools/audit_fp_flags.py`。

`-fno-fast-math -ffp-contract=off` / `/fp:strict` 原先只加在 `dspugen` 的 PRIVATE 上，但 `util/maths.hh` 与 `vectors.hh` 是内联头文件，被 CLI、21 个插件、5 个基准插件和 `tests/oracle.cc` 编译进各自 TU 时**不带**这些选项，理论上可被 FMA 收缩。改为 PUBLIC，并在 `dsp_plugin_abi` INTERFACE 目标上同样声明（插件不链接 `dspugen`，否则每个 DLL 会得到一份自己的 Prototype 全局状态）。

`audit_fp_flags.py` 原先只覆盖 dspugen 的 6 个 `.cc`，看不到这些 TU；现在覆盖 30 个，并区分「必须存在」与「存在即审计」两类，同时补上 MSVC 的 `/fp:strict` 检查与未识别编译器的报错。

### 11. 基准方法学

位置：`benchmarks/workloads.json`、`tools/benchmark.py`、`CMakePresets.json`。

- 原 `core` / `scaling` 层用 100 个 seed 的区间，程序内部计时只有 4–19 ms，而进程 wall 是 28–40 ms——启动与 Prototype JSON 解析占了大头。改为按 `seedsAt64` 声明预算，runner 按 `64 / 星数` 与线程数缩放，使每个 case 的内部计时落在同一量级；`--seed-scale` 可整体缩放。
- runner 现在解析 stderr 的 `Nms used` 作为主指标（`generateMedianSeconds` / `seedsPerSecond`），wall 时间降级为辅助指标；解析不到时显式标注 `seedsPerSecondSource: "wall"`，而不是静默报告一个被启动开销主导的数字。
- **发现并修复了一个使既有基准数据失效的问题**：仓库里既有的 `cmake-build-benchmark` 目录 `CMAKE_BUILD_TYPE` 为空，即无优化构建，同一负载比正确的 Release 慢 3.8 倍（853 对 224 µs/seed）。已提交的 `benchmark-results-core.json` / `benchmark-results-scaling.json` 就是在该目录下测的，因此那两份数据不可用作基线。现在新增 `benchmark` preset，并让 runner 读取可执行文件旁的 `CMakeCache.txt`、把构建配置写进结果、非优化构建直接拒绝运行（`--allow-unoptimized` 可强制）。

### 12. 验证基础设施

位置：`benchmarks/plugins/galaxy_digest.cc`（新增）、`owned_output.cc`（新增）、`pose_sink.cc`。

固定 fixture 只覆盖两个 seed，对本轮这类跨全流程的改动不够。新增：

- `galaxy_digest`：type-0 插件，声明全部需求位，对每颗恒星与行星的所有字段（含名字、气体、`veinSpot`、全部浮点字段）算位级摘要，按 seed 异或折叠所以与线程调度和 seed 顺序无关，卸载时打印。用来对比「改动前后」和「GCC 与 Clang」。
- `owned_output`：type-1 插件，实现 `formatOutput` 并写两个通道。仓库里**没有任何插件实现 `formatOutput`**，宿主的 owned-output 路径此前完全没有覆盖，而本轮恰好改动了它。
- `pose_sink`：原来算了坐标校验和却从不输出，改为对 x/y/z 三个坐标都取位模式并在卸载时打印，于是它同时是一个大范围的位置 parity 检查器。
- instrumentation 新增 `rareVeinRolls` 计数器（稀有矿循环的实际迭代次数），本轮用它证否了一项预期收益。

## 本轮验证结果

| 检查 | 结果 |
|---|---|
| `gcc-debug-parity` oracle | 匹配 `oracle-v1.txt`，`sha256=abade2f1…` |
| `gcc-release-parity` oracle | 匹配 |
| `clang-release-parity` oracle | 匹配 |
| `gcc-release-instrumented` oracle | 匹配 |
| 四种构建的 `audit_fp_flags.py` | 各审计 30 个 TU，通过 |
| `galaxy_digest`：改动前 vs 改动后 | 16 星与 64 星各 2000 seed，16 万恒星、59.4 万行星，摘要逐位相同 |
| `galaxy_digest`：GCC vs Clang | 同上负载摘要逐位相同 |
| `pose_sink`：原始 / 仅第 1 项 / 第 1+2 项 | 16、64、256 星各 2 万 seed，670 万坐标，校验和三者相同 |
| `roundToEven` 穷举比对 | 1220 万取值，0 处不一致 |
| `high_output`（`output()` 回调）改动前后 | 3000 seed 全命中，`benchmark-output.csv` 逐字节相同 |
| `owned_output`（`formatOutput`）改动前后 | 3000 seed 全命中，两个通道分别 22890 与 1821960 字节，逐字节相同 |
| 旧插件 DLL（新字段之前构建）配新宿主 | 摘要相同，无需重编 |
| 旧宿主 vs 新宿主的 `seeds.csv` | 排序后逐行相同 |

## 实测推翻的预期

### 稀有矿脉概率预表（原 P3，已回退）

原判断：`generateVeins` 里 `1.0f - mathfPow(1.0f - rareSettings[...], p)` 的两个输入都是常量域（Prototype 表 + 9 个离散指数），可在装载期全量预计算，是「行星路径上收益最高的单项」。

实施后实测：**收益为零**。原因有两层：

1. `rareVeinRolls` 计数器确认每 seed 481 次调用，与预估一致；但 `mathfPow` 实测只要 8.4 ns，而不是假设的 30–50 ns。481 × 8.4 ns = 4.0 µs，占 `-p` 全程 261 µs 的 1.5%，本身就在噪声边缘。
2. 预表放在 `ThemeProto` 的 `std::vector<float>` 里，每颗行星要多访问一个独立堆分配的向量，新增的 cache miss 与省掉的 `pow` 大致相抵。把概率与重复系数交错存放、让循环只读一段连续内存之后，实测仍与未优化版本完全相同（3834 seeds/s 对 3834 seeds/s，同一次会话里 `poses` 对照组无漂移）。

结论：已回退，只保留 `rareVeinRolls` 计数器。若要重新尝试，唯一可能奏效的形态是把所有主题的曲线压平进 `ThemeProtoSet` 的一块连续数组、并在 `ThemeProto` 里存偏移，使 11 KB 工作集常驻 L1/L2；但收益上限仍是 1.5%，不建议优先。

### P5 的数学缓存项（未实施）

同样因为 `mathfPow` 只有 8.4 ns，以下各项的收益上限都在 0.3 µs/seed 以下，不值得为它们给 `Star` 增加可变缓存状态（对象池重置遗漏风险 + 插件 ABI 风险）：

| 项 | 每 seed 调用次数 | 收益上限 |
|---|---:|---:|
| `mathfPow(orbitScaler, 0.3f)` 按恒星缓存（仅卫星路径） | 约 32 | 0.27 µs |
| `mathfPow(resourceCoef, 0.3f)` 上提到 `Star` | 约 34 | 0.29 µs |
| `select(theme)` 从每行星 2 次减到 1 次 | 243.5 | 稠密数组下标，可忽略 |

已实施的只有巨星 `lifetime` 的重复计算（原来默认分支先算一遍、巨星分支立刻用 0.58 重算，第一次结果是死值）与 `mass >= 2.0f` 时复用已有的 `log10_5` 静态缓存——这两项不引入新状态，属于纯简化。

### 无分支符号回绕（等价改写，无收益）

原判断：构造函数与 `sample()` 里的 `if (x < 0) x += MBIG` 是不可预测分支，改成掩码形式有收益。

实测：**GCC 在改动前就已经生成 `cmovs`**，改写在 `-O2` 下是等价改写而非优化。Clang 同样生成 `cmovnsl`。保留改写的理由只是不再依赖编译器做这个判断，收益记为 0。

## 实测排除的候选项

| 候选 | 排除依据 |
|---|---|
| 碰撞判距的 SIMD 批量化 | 每个候选平均只有 1.9 次距离比较（1328.1 / 694.6），没有可向量化的批量 |
| 主题候选扫描优化 | `themeRowsExamined` 每行星 5.2 行，阶段 4.2 的预索引已经把它压到很低 |
| Prototype 查找优化 | `prototypeLookups` 每行星 1.86 次，且已是稠密数组下标；主题 ID 为 1..25 稠密 |
| 对象池分配 | `-p` 模式 100 个 seed 只有 326 次首次分配、30524 次复用；`poolFreeListHighWater` 261 远低于上限 |
| 分块取种子（针对现有插件） | 有无分块调度的扩展性一致（`reject_galaxy` 与 `bluegiant` 在 t=1/8/16 下逐点相当），因为每 seed 生成成本远大于锁内区段 |
| 给现有插件补描述符以提升扩展性 | 同上；补描述符的正当理由是让宿主跳过不需要的生成阶段，不是解锁并行 |
| `-b` 跳过位置生成 | 见下节，属于未解决问题而非可实施优化 |

## 未解决问题

### `-b` 模式的位置生成

`-b` 与 `-P` 仍然同价（67.2 对 66.2 µs/seed）：出生星模式完整生成位置。在 `Galaxy::create()` 里，位置结果对 birthOnly 路径的唯一作用是确定最终 `starCount`——出生星自身的种子来自另一条随机流，与位置无关。

实测证据：`acceptedPositions` 恒为 255.0/seed；16、64、256 星各 10 万 seed 共 30 万 seed，输出的 `starCount` 全部等于目标值，无一例外。即 `RandomPoses` 总能达到 `targetCount * 4` 上限。

但这只是经验证据。`RandomPoses` 的 256 次重试上限构成一条真实的失败路径，`GenerateTempPoses` 的提前停止语义正是为它准备的。在没有失败率上界证明之前，不应基于「计数总是等于目标星数」跳过位置生成。

要推进这一项，需要先在 instrumentation 里加一个「`RandomPoses` 未达 `maxCount`」计数器，跨大范围 seed 与全部支持星数统计；若在合法星数范围内可证明为零，再考虑为 birthOnly 提供快路径，否则关闭此方向。

### `next()` 的整数域直算

`next()` 是 `sample() * 2147483647.0` 再截断，而 `sample()` 本身是 `num * 4.6566128752457969E-10`。理论上可从整数 `num` 直接得到结果，省掉两次浮点乘法与一次截断。但 `sample()` 现在只有 1.47 ns，而且需要在整个 `int` 范围上位级证明两条路径一致（各一次舍入）。收益小、风险高，保持关闭。

### 位置生成的进一步优化

位置阶段仍是最大单项（64 星 66.2 µs/seed，占纯恒星模式 74%）。剩余可能方向：

- 利用 `FLATTEN = 0.18` 把 27 个 cell 的查询降到 9 个（y 方向跨度远小于 x/z）。需要先用 instrumentation 统计 y 跨度确认收益。
- `positionCandidates` 每 seed 694.6 次，其中 154.5 次因单位球判定被拒、285.1 次因碰撞被拒。这些拒绝都必须消耗随机数，无法跳过。

## 下一步建议

按现有数据，剩余收益最大的方向依次是：

1. 位置阶段的 9-cell 查询（需先量化 y 跨度）。
2. `DotNet35Random` 构造的第一段循环——54 次迭代的串行依赖链，目前占构造成本的约 20%，暂无明显可并行结构。
3. `-b` 快路径（需先证明失败率为零）。

新增任何一项之前，先用微基准测出单位成本再排优先级；本轮有三项预期收益因为跳过这一步而排错。
