# 优化计划验证记录

参考环境：Dyson Sphere Program `0.10.34.28529`，Unity `2022.3.62f3c1`，Mono，算法 `20200403`，Prototype 快照为仓库 `Prototypes/`。

## 位级 oracle

固定 fixture：`tests/fixtures/oracle-v1.txt`

SHA-256：`ABADE2F14F3112ADB20D37E4AD29BDC90203C4174F908749A0EC59EBAEA73B6F`

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

## Smoke 验证

- 合成基准插件全部构建成功。
- smoke tier 覆盖位置、恒星和行星三种模式；runner 完成预热和正式采样，输出中位数与 MAD。
- GCC Release LTO 已完成 core 的 42 个 case 和 scaling 的 216 个 case；每个 case 预热 1 次并正式运行 7 次。core 全 case wall median 的中位数为 `0.04064345 s`，scaling 全 case wall median 的中位数为 `0.04418915 s`。原始结果由 runner 写入忽略的 `benchmark-results-core.json` 和 `benchmark-results-scaling.json`。
- `-b -n -t 1 "0-10,64"` 完成 11 个 seed，退出码 0。
- instrumentation JSON 成功输出位置候选、单位球拒绝、碰撞比较、碰撞拒绝、对象池、行星、主题、Prototype、插件和 RNG trace 计数。

完整 7 次、全星数和全线程性能矩阵可通过以下命令复现：

```powershell
python tools/benchmark.py cmake-build-benchmark/bin/DSPSeedCalc.exe `
  --plugin-dir cmake-build-benchmark/bin/benchmark-plugins `
  --tier scaling --runs 7 --compiler "GCC 16.1.0"
```

该矩阵会覆盖 16、32、64、96、128、256 星和 1、8、16、24、31、32 线程，并分别运行位置、恒星拒绝、star callback、planet callback、无插件高命中和高命中输出场景。
