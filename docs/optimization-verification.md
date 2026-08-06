# 优化计划验证记录

参考环境：Dyson Sphere Program `0.10.34.28529`，Unity `2022.3.62f3c1`，Mono，算法 `20200403`，Prototype 快照为仓库 `Prototypes/`。

## 位级 oracle

固定 fixture：`tests/fixtures/oracle-v1.txt`

SHA-256：`D48DF98E29FF60F00658DF2B5A5F4B8D57DD915C80BB8557498B913D6A63D8DA`

覆盖内容：

- `DotNet35Random` 种子 `0`、`1`、`12345678` 的前 100 个 `next()` 与独立的前 100 个 `nextDouble()` 位模式。
- 种子 `0`、`12345678`，64 星的位置顺序与坐标位模式。
- 同一组星系的恒星字段、完整行星拓扑、主题、算法、矿脉元数据、气体条目和气体速度位模式。
- 名称生成，以及同线程对象池复用后重复生成 seed 0 的结果。

已验证构建：

| 构建 | 编译器 | 结果 |
|---|---|---|
| `gcc-debug-parity` | MSYS2 UCRT64 GCC 16.1.0 | oracle 匹配，FP flag audit 通过 |
| `gcc-release-parity` | MSYS2 UCRT64 GCC 16.1.0，LTO | oracle 匹配，FP flag audit 通过 |
| `clang-release-parity` | MSYS2 Clang64 22.1.8，LTO | oracle 匹配，FP flag audit 通过 |
| `gcc-release-instrumented` | MSYS2 UCRT64 GCC 16.1.0，LTO | oracle 匹配，FP flag audit 通过 |
| GCC PGO generate smoke | MSYS2 UCRT64 GCC 16.1.0，LTO | 构建成功，oracle 匹配 |
| Clang PGO generate smoke | MSYS2 Clang64 22.1.8，LTO | 构建成功，oracle 匹配 |
| GCC PGO use | MSYS2 UCRT64 GCC 16.1.0，LTO | 完整训练 manifest 执行成功，PGO use 重建成功，oracle 匹配，FP flag audit 通过 |

名称实现另行用当前参考 DLL 的 ILSpy 反编译结果复核。巨星星座字母分支中的 `ushort + char` 按 C# 规则先提升为整数后再与字符串拼接，因此当前十进制数字前缀是参考实现行为，不是字符拼接。

检查命令：

```powershell
cmake --preset gcc-debug-parity
cmake --build --preset gcc-debug-parity
python tools/verify_oracle.py cmake-build-gcc-debug-parity/bin/dspugen_oracle.exe
python tools/audit_fp_flags.py cmake-build-gcc-debug-parity/compile_commands.json

cmake --preset gcc-release-parity
cmake --build --preset gcc-release-parity
python tools/verify_oracle.py cmake-build-gcc-release-parity/bin/dspugen_oracle.exe
python tools/audit_fp_flags.py cmake-build-gcc-release-parity/compile_commands.json

cmake --preset clang-release-parity
cmake --build --preset clang-release-parity
python tools/verify_oracle.py cmake-build-clang-release-parity/bin/dspugen_oracle.exe
python tools/audit_fp_flags.py cmake-build-clang-release-parity/compile_commands.json
```

## 全流程摘要验证

固定 fixture 只覆盖两个 seed。为了验证跨全流程的改动，`benchmarks/plugins/` 增加了两个校验插件，配合改造后的 `pose_sink`：

| 插件 | 覆盖内容 |
|---|---|
| `galaxy_digest` | 声明全部需求位，对每颗恒星与行星的所有字段（含名字、气体、`veinSpot`、全部浮点字段）取位级摘要，按 seed 异或折叠，因此与线程调度和 seed 顺序无关 |
| `owned_output` | 实现 `formatOutput` 并写两个通道，覆盖宿主的 owned-output 路径——仓库内没有任何插件实现 `formatOutput`，该路径此前无覆盖 |
| `pose_sink` | 对 x/y/z 三个坐标取位模式并在卸载时打印，作为大范围位置 parity 检查 |

已记录的参考值（`Prototypes/` 当前快照，seed `0-1999`，`-t 4`）：

| 星数 | `galaxy_digest` | 恒星数 | 行星数 |
|---:|---|---:|---:|
| 16 | `89abf279dec86713` | 32000 | 105443 |
| 64 | `3a20b6a07a8dca04` | 128000 | 488267 |

这两个摘要在「恒星与行星字段按 Mono 语义对齐」一轮中更新过（旧值 `63981f89400edae8` / `be0243dc0a38f6bd`）。恒星数与行星数不变，说明改动只移动了浮点值，没有改变任何整数判定或对象数量；`pose_sink` 的三个校验和同样保持不变。

`pose_sink` 参考值（seed `0-19999`，`-P`）：16 星 `9d727cec36ceed84`、64 星 `3277ef00c03ce146`、256 星 `1dca6c7f8fab9af3`。

已验证：GCC Debug、GCC Release、Clang Release 三种构建的摘要与上表逐位相同；优化前后的摘要相同；新增 `Star::planetGenSeed` 之前构建的插件 DLL 配新宿主仍得到相同摘要（字段追加在末尾，未移动既有偏移）。

## Smoke 验证

- 合成基准插件全部构建成功。
- smoke tier 覆盖位置、恒星和行星三种模式；runner 完成预热和正式采样，输出中位数与 MAD。
- `-b -n -t 1 "0-10,64"` 完成 11 个 seed，退出码 0。
- instrumentation JSON 成功输出位置候选、单位球拒绝、碰撞比较、碰撞拒绝、对象池、行星、稀有矿、主题、Prototype、插件和 RNG trace 计数。

## 精确矿量验证

精确矿量有两层检查，作用不同，不要混为一谈。

`tests/fixtures/vein-amounts-v1.txt` 由 C++ oracle 自己生成，**只是回归锁，不构成与游戏一致的证据**。它覆盖 seed `0`、`1` 的 64 星星系，按恒星和行星顺序记录 `veinSpot`、矿点数和普通资源模式的基础矿量。

与游戏的一致性由 `tools/verify_vein_parity.py` 对照 `tests/fixtures/game/` 下的游戏内导出结果检查，导出工具、方法、当前结果和浮点结论见 `docs/vein-amount-parity-evidence.md`。扩展样本覆盖 101 个 seed、24,586 颗行星，当前差异为 0。

地形 dispatch 支持算法 `0–14`。当前 Prototype 快照的 `Algos` 并集仍为 `0–13`，因此固定 seed fixture 不会自然覆盖算法 `14`；测试使用合成行星验证 `PlanetAlgorithm14.GenerateTerrain`，随后复用基类 `PlanetAlgorithm.GenerateVeins`。算法 `15` 继续返回明确的不支持结果。

参考构建为 Dyson Sphere Program `0.10.34.28529`、Steam build `23109513`、Unity `2022.3.62f3c1`、Mono、算法 `20200403`。生成游戏侧对照时会启动一次游戏并加载 `tools/vein-oracle-mod`；未使用存档，也未附加调试器。

检查命令：

```powershell
cmake --build cmake-build-gcc-release-parity --target dspugen_vein_amount_oracle
# 回归锁
python tools/verify_vein_amounts.py `
  cmake-build-gcc-release-parity/bin/dspugen_vein_amount_oracle.exe
# 与游戏对照
python tools/verify_vein_parity.py `
  cmake-build-gcc-release-parity/bin/dspugen_vein_amount_oracle.exe
```

CLI 与插件 smoke 使用隔离目录，仅保留匹配的 `Prototypes/` 和 `vein_amount_v2_test.dll`：

- ABI v2 插件先用 `birthPlanetId` 初筛，只对每个 seed 的出生行星调用 `GeneratePlanetVeinAmounts()`；调用前后遍历所有行星，确认汇总数量从 0 变为 1；
- `-t 1 -V -o vein.csv "0-1,64"` 生成 2 行两列结果，GCC Release parity 构建连续 5 次为 12、12、12、12、13 ms；
- 不带 `-V` 时，声明 `NeedsVeinAmounts` 的插件在扫描前被拒绝；
- 测试插件省略 `NeedsVeinAmounts` 后调用精算接口时，宿主返回 `ServiceUnavailable`、停止扫描并以非零状态退出，且不会加载未校验的顶点网格；
- `-V -P` 和 `-V -Z` 在加载插件之前返回冲突错误，因此被拒绝的命令行不会触发插件 `init`；
- 导出 `init3` 但缺少有效 ABI 2 `describePlugin` 的插件被显式拒绝，并打印原因、调用 `uninit` 后卸载；
- 默认 oracle SHA-256 保持 `D48DF98E29FF60F00658DF2B5A5F4B8D57DD915C80BB8557498B913D6A63D8DA`；
- 精确矿量回归锁 SHA-256 保持 `5AADE6936C1421AF1A7F6BAB63EA1CF2E74A784E4C94A094A003AFE9CFF9DC52`。

### 成本

每个被插件显式请求的非气态行星都会运行精确地形采样与矿脉放置。GCC Release parity 构建、单线程历史测量：

| 阶段 | 每行星 |
| --- | --- |
| `PlanetRawData` 构造 | 0.43 ms |
| `calcVerts()`（已消除） | 19.5 ms |
| `dispatchTerrain()` | 58.1 ms |

`Galaxy::ensureAllVeinAmounts()` 与 `dspugen_vein_amount_oracle` 仍可用于全星系校验。生产插件不再因为声明 `NeedsVeinAmounts` 而自动进入该路径；实际成本由插件请求的候选行星数量决定。

顶点网格与索引映射只由 precision 决定，参考实现把它们缓存为 `verts200` 与 `indexMap200` 后复制复用。C++ 侧同样按 precision 做线程局部缓存，并在同一线程内复用一个 `PlanetRawData` 实例，仅在行星之间清空高度与生物群系数据。改动前后 fixture 完全一致，用时从 41.1 s 降到 28.9 s。

GPU 研究结论见 `docs/vein-amounts-gpu-research.md`。当前版本只使用 CPU 权威生成，不增加 GPU 依赖。

### 已失效的历史基准数据

`benchmark-results-core.json` 与 `benchmark-results-scaling.json` 是在 `CMAKE_BUILD_TYPE` 为空的 `cmake-build-benchmark` 目录下测得的，即无优化构建，同一负载比正确的 Release 慢 3.8 倍（853 对 224 µs/seed）。**这两份数据不可用作基线。** 现已增加 `benchmark` preset，`tools/benchmark.py` 会读取可执行文件旁的 `CMakeCache.txt`、把构建配置写进结果，并拒绝在非优化构建上运行。

同时 runner 改用程序自身的 `Nms used` 作为主指标：原 100 个 seed 的区间使内部计时只有 4–19 ms，而进程 wall 是 28–40 ms，被启动与 Prototype 解析主导。区间规模现在由 `benchmarks/workloads.json` 的 `seedsAt64` 按星数与线程数缩放。

完整性能矩阵复现命令：

```powershell
cmake --preset benchmark
cmake --build --preset benchmark
python tools/benchmark.py cmake-build-benchmark/bin/DSPSeedCalc.exe `
  --plugin-dir cmake-build-benchmark/bin/benchmark-plugins `
  --tier scaling --runs 7 --compiler "GCC 16.1.0"
```

该矩阵会覆盖 16、32、64、96、128、256 星和 1、8、16、24、31、32 线程，并分别运行位置、恒星拒绝、star callback、planet callback、无插件高命中和高命中输出场景。

## 优化轮次记录

本轮实施的优化项、实测提升与被实测推翻的预期见 `docs/performance-opportunities.md`。所有项均通过上述 oracle、FP flag audit 与全流程摘要验证；单线程各模式提升 1.65×–2.82×，31 线程扩展性从 11.2×（16 线程）提升到 21.0×（31 线程）。
