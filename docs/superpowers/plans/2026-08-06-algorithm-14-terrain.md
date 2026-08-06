# Algorithm 14 Terrain Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让精确矿量路径支持游戏的 `PlanetAlgorithm14` 地形，并继续复用基类矿物生成。

**Architecture:** 在现有 terrain dispatch 中增加一个按反编译代码逐句移植的 `generateTerrain14`。精确矿量入口只扩大支持范围，不改变通用矿脉生成和算法 `7`、`11`、`12`、`13` 的专用过滤。

**Tech Stack:** C++17、CMake、Ninja、`DotNet35Random`、`SimplexNoise`、固定种子 Oracle。

---

### Task 1: 补充算法 14 的红灯测试

**Files:**
- Modify: `tests/vein_amounts_test.cc`

- [x] **Step 1: 将 terrain dispatch 测试改为期望算法 14 成功**

在算法 `7–13` 的循环后增加算法 14，并要求 `heightData` 已写入；将不支持边界移到算法 15。

- [x] **Step 2: 将合成行星测试改为期望算法 14 生成矿量**

保留对算法 14 的合成覆盖，但断言：

```cpp
assert(planet->ensureVeinAmounts() ==
       dspugen::VeinAmountGenerationResult::Generated);
assert(planet->veinAmountSummary() != nullptr);
```

另将同一行星改为算法 15，验证它仍返回 `UnsupportedAlgorithm`。

- [x] **Step 3: 运行测试并确认失败原因**

```powershell
$env:PATH='C:\msys64\ucrt64\bin;' + $env:PATH
cmake --build cmake-build-gcc-debug-parity --target dspugen_vein_tests -j 1
cmake-build-gcc-debug-parity\bin\dspugen_vein_tests.exe
```

预期：测试因算法 14 仍返回 `UnsupportedAlgorithm` 而失败。

### Task 2: 移植算法 14 地形并扩大支持范围

**Files:**
- Modify: `dspugen/terrain/planet_algorithm.cc`
- Modify: `dspugen/vein/vein_amounts.cc`

- [x] **Step 1: 新增 `generateTerrain14`**

按 `PlanetAlgorithm14.GenerateTerrain` 保留以下内容：

- 4 次随机种子消费和 4 个噪声实例；
- `Levelize`、`Levelize2`、`Levelize4`；
- `Mathf.Abs`、`Mathf.Clamp01`、`Mathf.Lerp`、`Mathf.Clamp` 的单精度边界；
- `x -> y -> z` 顺序坐标扰动；
- 未使用结果的 `Noise3DFBM` 调用；
- `heightData` 与 `biomoData` 的窄化顺序。

- [x] **Step 2: 更新 dispatch 和矿量入口边界**

算法 14 调用 `generateTerrain14` 并返回 `Generated`。矿量入口接受 `1–14`，算法 15 及更大值继续返回 `UnsupportedAlgorithm`。

- [x] **Step 3: 运行测试并确认通过**

重复 Task 1 的构建与测试命令，预期退出码为 `0`。

### Task 3: 更新范围文档并验证 Oracle

**Files:**
- Modify: `docs/superpowers/specs/2026-08-06-precise-vein-amounts-design.md`
- Modify: `docs/superpowers/plans/2026-08-06-precise-vein-amounts.md`
- Modify: `docs/optimization-verification.md`

- [x] **Step 1: 将支持范围更新为算法 0–14**

删除算法 14 的排除说明，并记录它继承基类 `GenerateVeins`。

- [x] **Step 2: 运行默认与精确矿量 Oracle**

```powershell
python tools/verify_oracle.py cmake-build-gcc-debug-parity/bin/dspugen_oracle.exe
python tools/verify_vein_amounts.py cmake-build-gcc-debug-parity/bin/dspugen_vein_amount_oracle.exe
```

预期：默认 Oracle 保持现有 SHA-256；当前 Prototype 快照未引用算法 14，因此精确矿量 fixture 也应保持不变。

- [x] **Step 3: 运行 GCC Release 与 Clang Release 验证**

构建并运行 `dspugen_vein_tests`、默认 Oracle 和精确矿量 Oracle，预期全部返回 `0`。

- [ ] **Step 4: 提交修正**

```powershell
git add docs dspugen tests
git commit -m "fix: support algorithm 14 vein generation"
```
