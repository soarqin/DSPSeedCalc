# 精确矿量参照与浮点结论

## 参考版本

Dyson Sphere Program `0.10.34.28529`、Steam build `23109513`、Unity `2022.3.62.1451004`、Mono、算法 `20200403`、BepInEx `5.4.22.0`。`Assembly-CSharp.dll` SHA-256 `AE0BA95F75BD879A62AA4CE253B2AB78EAA4FB3C7C595F5E1FEE75EBE0E0EF85`。

## 参照是怎么来的

`tests/fixtures/vein-amounts-v1.txt` 是 C++ 自己的输出，只能锁定回归，不能证明与游戏一致。真正的对照由 `tools/vein-oracle-mod` 产生：它是一个 BepInEx 插件，在游戏进程内运行，因此每个数值都出自游戏自己的托管代码、跑在游戏自己的 Mono 运行时上，包括三个只存在于引擎原生代码、无法从托管程序集还原的 Unity 数学函数。

插件不需要存档。它在主菜单直接调用 `UniverseGen.CreateGalaxy`，再按 `PlanetModelingManager` 工作线程的原顺序对每颗行星执行：

```
data = new PlanetRawData(precision) → InitModData → CalcVerts → aux = new PlanetAuxData
GenerateTerrain(mod_x, mod_y) → CalcWaterPercent
vegeCursor = 1 → GenerateVegetables
veinCursor  = 1 → GenerateVeins
```

随后按矿脉类型汇总 `data.veinPool[1 .. veinCursor)` 的数量与 `amount`。`resourceMultiplier` 设为 `1`，此时 `isRareResource`、`isInfiniteResource` 均为假、`oilAmountMultiplier` 为 `1`，游戏写入 `VeinData.amount` 的值与 C++ 汇总的定义完全一致。

产物：

- `tests/fixtures/game/vein-parity-game.txt`：seed `10` 的 64 星星系，共 241 颗行星，含每颗恒星的 `resourceCoef` 原始位和每颗行星的 `mod_x`、`mod_y`、`radius`、地形摘要、矿点数与基础矿量。101 seed 的扩展样本用于完整回归，不作为默认跟踪夹具。
- `tests/fixtures/game/vein-parity-game-protos.txt`：插件从运行时 LDB 读到的 theme 与 vein 原型字段。

`tools/verify_vein_parity.py` 先把原型指纹与仓库的 `Prototypes/` 逐字段比对，指纹不符即拒绝比较（防止其它 mod 改过 LDB，或快照与游戏版本不匹配），通过后再逐行星精确比对矿点数与矿量。

重新生成：

```powershell
dotnet build -c Release tools/vein-oracle-mod
# 复制 bin/Release/DspVeinOracle.dll 到 <游戏目录>/BepInEx/plugins/DspVeinOracle/
# 在 BepInEx/config/dspseedcalc.veinoracle.cfg 中设置 OutputDirectory，启动一次游戏
python tools/verify_vein_parity.py cmake-build-gcc-release-parity/bin/dspugen_vein_amount_oracle.exe
```

## 当前结果

装入游戏导出的顶点网格后，seed 0 的 **258 颗行星（227 颗生成地形 + 31 颗气态）零例外与游戏逐位完全一致**：`heightData` 摘要、`biomoData` 摘要、矿点数、基础矿量，以及恒星数与出生行星，全部相同。GCC Debug、GCC Release、Clang Release 三套工具链结果一致。

达成这一结果需要两条独立的根因修正，见后文两节：

1. Mono 的 float32 运算在双精度下求值，只在写入 float32 位置时收窄一次。
2. 算法 `4` 与 `10` 用的是 `Vector4.Normalize`，它与 `Vector3.Normalize` 相差一次舍入。

气态行星不再作为例外排除，其常量地形已按 `PlanetAlgorithm0` 直接生成。

大规模复核已完成：**矿脉路径 101 个 seed、24,586 颗行星（21,169 生成地形 + 3,417 气态，无排除）全部匹配；恒星与行星字段 256 个 seed、16,384 颗恒星、62,541 颗行星全部字段差异 0。**

过程中定位并修复的移植缺陷见后文「已修复的移植缺陷」。

## 根因：Mono 的 float32 运算在双精度下求值

这是本轮最重要的发现，它同时解释了地形、矿量和恒星字段的全部差异。

ECMA-335 允许 float32 运算使用精度更高的「内部表示」。Mono 在 x86-64 上把求值栈上的 float32 一律按 R8 保存：`ldfld float32` 载入即转双精度，算术用 `mulsd` / `addsd` 等双精度指令，只有在结果**存入 float32 位置**（字段、局部变量、函数实参、返回值）时才用 `conv.r4` 收回单精度。

因此 C# 源码里的

```csharp
double num6 = data.vertices[i].x * planet.radius;   // 两个 float 相乘
```

在 Mono 下是**双精度乘法**，不是单精度乘法。实测印证（seed 0 行星 103，precision 200 顶点网格取自游戏）：

| 表达式 | 结果 |
| --- | --- |
| `(double)(v.x * radius)`（原移植） | `4068FFCD80000000` |
| `(double)v.x * (double)radius` | `4068FFCD83000000` |
| 游戏 | `4068FFCD83000000` |

同一规则也解释了恒星 `mass`：`RandNormal` 的 `avg + sd * (float)(...)` 在 Mono 下整体按双精度求值、返回时舍入一次，而移植逐步按单精度舍入。

| 求值方式 | mass |
| --- | --- |
| 逐步单精度（原移植） | `0x3F0503D5` |
| Mono 语义（双精度求值、存储时舍入一次） | `0x3F0503D6` |
| 游戏 | `0x3F0503D6` |

**移植规则应为：float 表达式整体按 double 求值，只在参考代码把值写入 float32 位置（赋值给 float 变量或字段、作为 float 实参传入、作为 float 返回值）时收窄一次。** 原先「Mathf 运算按单精度处理」的做法只对最终结果成立，对中间步骤是错的。

按此规则修正后（配合游戏导出的顶点网格），seed 0 的 227 颗非气态行星地形高度、矿点数与基础矿量与游戏**逐位完全一致**。

## 浮点结论

`double` 运算两边一致：x86-64 上 Mono 与 C++ 都走 SSE 标量双精度指令，逐运算按 IEEE-754 binary64 正确舍入，没有 x87 扩展精度，也没有隐式 FMA 合并（仓库已施加 `-fno-fast-math -ffp-contract=off` 与 MSVC `/fp:strict`）。`float` 运算则**不是**逐运算单精度，见上节。

除此之外，差异出现在实现不在托管程序集里的函数上，分两类。

### 托管代码：可从 ILSpy 还原，必须逐句照抄

`UnityEngine.CoreModule` 里下列成员是托管实现，反编译即权威，本次据此修正了三处：

- `Quaternion.operator*(Quaternion, Quaternion)` 的求和顺序是 `w*rx + x*rw + y*rz - z*ry`；原实现照抄的是游戏自己的 `Maths.QMultiply`（`x*rw + y*rz - z*ry + w*rx`）。两者数学等价、浮点不等价。实测在本路径实际用到的输入（绕 `Vector3.up` / `forward` / `down` 的 `AngleAxis` 之积）上 129,600 组样本无差异，因此属于修正而非行为变更。
- 旋转向量有两个不能互换的例程：`GenerateVeins` 用 `UnityEngine.Quaternion.operator*(Quaternion, Vector3)`，`PlanetData.PredictPose` 用游戏的 `Maths.QRotate`。原实现两处都用了 `QRotate`。129,600 组样本中 82% 结果不同，现按调用点分别使用 `unityRotate` 与 `qRotate`。
- `Mathf.Sign(f)` 是 `(f >= 0f) ? 1f : -1f`，零返回 `+1`；原实现零返回 `0`。

`Vector3.Normalize` / `magnitude`、`Vector2.Normalize` / `magnitude`、`Mathf.Clamp`、`Clamp01`、`Lerp`、`Round`、`RoundToInt`、`FloorToInt`、`Pow`、`Abs`、`Sqrt` 的**算法**均已逐条核对无误；它们的**求值精度**按上节规则另行修正（`magnitude` 的平方和留在双精度、`Mathf.Lerp` 只在返回时收窄一次等）。

### 引擎原生代码：无法还原，也无法靠重写对齐

`Vector3.Slerp`、`Quaternion.FromToRotation`、`Quaternion.AngleAxis` 在托管层只是 `[FreeFunction]` + `MethodImplOptions.InternalCall` 的外部声明，实现位于 `UnityPlayer.dll`。

其中 `Vector3.Slerp` 是唯一有实测影响的一个：`PlanetRawData.CalcVerts` 对 precision 200 的 161,604 个顶点各调用三次，顶点网格是所有地形算法的输入。把游戏内导出的 precision 200 顶点网格与 C++ 逐位比对：

| 实现 | 不一致的分量 |
| --- | --- |
| 仓库现有 `slerp` | 217,846 / 484,812（44.93%） |
| Unity 公开的原生 `Vector3::Slerp` 写法（native `Lerp`） | 318,612（65.72%） |
| 同上，managed `Lerp` | 320,588（66.13%） |

差距集中在 1–2 ULP（分布 1:181778、2:23898，长尾至 8 以上），132,342 / 161,604 个顶点受影响。换用「正确」的代数写法反而更差，说明剩余差异来自 Unity 原生构建所用的 `acos` / `sin` 实现，而不是公式选择。**结论：重写无法逐位对齐 `Vector3.Slerp`。**

顶点的 ULP 级偏差经地形公式放大后，会在 `(ushort)((radius + height + bias) * 100.0)` 的截断处偶尔跨过整数边界。实测三颗行星的原始 `heightData`：

| 行星 | 算法 | 不同的格子 |
| --- | --- | --- |
| seed 0 / 102 | 1 | 16 / 161,604（0.0099%），全部 ±1 |
| seed 1 / 102 | 2 | 1 / 161,604（0.0006%），+1 |
| seed 1 / 1502 | 6 | 11 / 161,604（0.0068%），全部 ±1 |

## 256 seed 的恒星与行星字段对照

插件的 `Mode = meta` 只生成恒星与行星字段，不跑地形，因此 256 个 64 星 seed 只需 40 s。对照 `tools/vein-oracle-mod` 与 `dspugen_vein_amount_oracle --meta 0 255 64`，覆盖 16,384 颗恒星、62,541 颗行星。

**结果：全部 46 个字段逐位完全一致，差异 0。** 覆盖

`id`、`index`、`seed`、`type`、`spectr`、`name`、`posX/Y/Z`、`mass`、`lifetime`、`age`、`temperature`、`luminosity`、`radius`、`habitableRadius`、`lightBalanceRadius`、`orbitScaler`、`dysonRadius`、`color`、`classFactor`、`resourceCoef`、`theme`、`algo`、`number`、`singularity`、`orbitAround`、`orbitIndex`、`orbitRadius`、`sunDistance`、`scale`、`waterItemId`、`waterHeight`、`modX`、`modY`、`obliquity`、`orbitInclination`、`orbitLongitude`、`orbitPhase`、`orbitalPeriod`、`rotationPeriod`、`rotationPhase`、`temperatureBias`、`habitableBias`、`gas`、`birthStar`、`birthPlanet`、`habitable`、`stars`、`planets`。

对齐前 62.0% 的恒星与 73.3% 的行星存在差异，差值一律是 float 的最低几位。当初溯源到的源头是 `starData.mass = Mathf.Pow(2f, num12)` 的指数 `num12` 相差 1–3 ULP，其余字段由 `mass` 级联派生。

**成因不是 libm。** 用固定步长网格采样 `pow`、`log`、`log10`、`exp`、`sin`、`cos`（含 ±30 的宽区间）、`sqrt`、`asin`、`atan2` 以及 `RandNormal` 组合式（合计约 15 万个样本），只有 `pow`（0.16%）和 `asin`（1.2%）在 double 层面差 1 ULP，且 `pow` 的差异在 `(float)` 窄化后全部消失。真正的成因是 Mono 的双精度求值规则加上下文所述的 float 字面量 widen 问题，详见「已完成」一节。

## 顶点网格印证

`Vector3.Slerp` 无法重写对齐，唯一可行的办法是把游戏导出的 precision 200 顶点网格当作数据使用——参考实现本身也是这么做的（`verts200` / `indexMap200` 算一次后复制复用）。`indexMap` 由顶点纯整数推导，无需一并存储。

`PlanetRawData::installVertexGrid(precision, vertices)` 装入捕获的网格后，`calcVerts()` 直接使用它并重算索引映射；`dspugen_vein_amount_oracle --verts <file>` 用于验证。装入后顶点与游戏 161,604 个全部逐位相同。

单独装入网格的效果（seed 0、1，425 颗非气态行星）：算法 `12` 从 38/38 错降到 0/38 全对，算法 `2`、`4`、`11`、`13`、`6` 部分改善，算法 `1`、`3`、`5`、`7`、`8`、`9`、`10` 仍 100% 不一致。也就是说**网格是必要条件，但不是充分条件**——真正的主因是上面那条 Mono 双精度求值规则。

两者同时修正后，seed 0 的 227 颗非气态行星地形高度、矿点数、基础矿量与游戏逐位一致。

## 逐算法结果

seed 0，227 颗非气态行星，装入捕获顶点网格：

| 阶段 | 高度不一致的行星 |
| --- | --- |
| 修正前 | 全部 227 |
| 仅装入顶点网格 | 30（算法 `8` 17/17、算法 `3` 13/65） |
| 顶点坐标改为双精度乘法 | 30 → 13（算法 `3`） |
| 算法 `8` 与 `terrainCurve` 按 Mono 语义 | 13 |
| `Mathf.Lerp`、`Mathf.Abs` 实参按 Mono 语义 | 0 |

矿脉侧同样按该规则修正了 `2.1f / radius`、簇间距比较、`opacity * 100000f * coef`、`FloorToInt` 的 `* 0.9375f`、节点矿量的 `* 1.1f`，以及 `Vector3` / `Vector2` 的 `sqrMagnitude`、`magnitude` 和 `PlanetRawData.QueryHeight` 的累加，最终矿点数与矿量在 seed 0 全部一致。

## 已修复的移植缺陷

1. `PlanetAlgorithm10` 缺少外层 `Maths.Levelize`（`f = (float)Levelize(Levelize4(f))`），算法 `10` 的行星全部错误。
2. 全部地形算法把 `vertices[i].x * planet.radius` 当成单精度乘法。
3. `terrainCurve`、算法 `8` 的 `Mathf.Clamp` 实参、`Mathf.Lerp`、`Mathf.Abs` 实参逐步舍入。
4. 矿脉放置的间距、阈值比较与矿量表达式逐步舍入。
5. `Vector3` / `Vector2` 的 `sqrMagnitude`、`magnitude` 与 `QueryHeight` 的加法逐步舍入。
6. 算法 `4` 与 `10` 的干扰球归一化用了 `Vector3.Normalize` 的语义，参考实现用的是 `Vector4.Normalize`；而且原写法的平方和还是逐步单精度累加。
7. 气态行星的常量地形（`PlanetAlgorithm0`，precision 64）未生成，导致摘要只能作为例外排除。
8. 把 C# float 表达式改写成双精度时，`Nf` 字面量被写成了 C++ 的双精度字面量 `N`。影响 `temperatureBias`、`color`（两处）、`age`（五个分支）、`SetStarAge`（七处复合赋值）。
9. `dysonRadius` 把 IL 里全程双精度的 `physicsRadius * 1.5f` 收窄成 float，并把比较预先除成 float 边界。
10. `orbitRadius` 两个分支逐步舍入（`num17` 与卫星分支的整条乘加链）。
11. `rotationPeriod` 用 `powf` 代替了 `Mathf.Pow`（`(float)Math.Pow(double, double)`）。
12. `gas` 的 `num6 *= (float)rand * 0.19090915f + 0.9090909f` 逐步舍入；参考实现里的 `num6 *= gasCoef` 可省略，`gasCoef` 恒为 `1f`，乘法精确。
13. 出生恒星分支（`createBirthStar`）整段未按 Mono 语义修正：`num5`、`temperature`、`color`、`habitableRadius`、`dysonRadius`。

## 编译器与构建类型复核

按 `CLAUDE.md` 的验证顺序，三套工具链各自重新构建并运行：

| 构建目录 | 编译器 / 类型 | `dspugen_vein_tests` | `dspugen_oracle` | 矿量 oracle |
| --- | --- | --- | --- | --- |
| `cmake-build-gcc-debug-parity` | GCC UCRT64 Debug | 通过 | `abade2f14f3112ad` | `b67cee897dd8c01a` |
| `cmake-build-gcc-release-parity` | GCC UCRT64 Release | 通过 | `abade2f14f3112ad` | `b67cee897dd8c01a` |
| `cmake-build-clang-release-parity` | Clang64 Release | 通过 | `abade2f14f3112ad` | `b67cee897dd8c01a` |

`dspugen_oracle` 的摘要与 `tests/fixtures/oracle-v1.txt` 完全相同，再次确认本轮没有改动恒星与行星的既有结果；矿量 oracle 在三套工具链下逐位相同，说明改写后的表达式没有引入编译器相关的求值差异。

`dspugen_vein_tests` 基于 `assert`，通过时不输出内容；Release 下由 `target_compile_options` 追加的 `-UNDEBUG` 排在 `-DNDEBUG` 之后，断言确实生效（`ninja -t commands` 可确认）。

在 Git Bash 中直接运行这些可执行文件时，必须把对应工具链的 `bin` 目录加入 `PATH`，否则缺少 `libstdc++-6.dll` 等运行库会以退出码 127 静默失败、产出空文件——空输出的 SHA-256 是 `e3b0c44298fc1c14…`，看起来像一个正常摘要，很容易被误读成通过。

## 根因二：归一化的平方和被逐步单精度累加

算法 `4` 与 `10` 在建立干扰球 / 椭球时用的是 `Vector4`：

```csharp
Vector4 vector = new Vector4((float)vectorLF.x, (float)vectorLF.y, (float)vectorLF.z);
vector.Normalize();
```

移植的归一化把平方和写成了**逐步单精度累加**（`(x*x + y*y) + z*z`，三次舍入），这与 Unity 的任何一种写法都不同。

`Vector4` 与 `Vector3` 的求模形状确实不同，二者都出现在本仓库里，不能互换：

```csharp
Vector4.Magnitude(a) => (float)Math.Sqrt(Dot(a, a));                          // Dot 返回 float32，平方和先收窄
Vector3.Magnitude(v) => (float)Math.Sqrt(v.x * v.x + v.y * v.y + v.z * v.z);  // 平方和写在表达式里，留在 double
```

因此算法 `4`、`10` 的这处归一化应按 `Vector4` 语义：**平方和按 double 求值、收窄一次，再开方收窄一次**；而 `Vector3::normalized()`、`Vector2::normalized()` 应保持平方和在 double 直到 `Math.Sqrt`。

### 只用一颗行星做判据会得出错误结论

三种候选语义各自实测（6 个 seed、1479 颗行星）：

| 候选 | 平方和处理 | 算法 `4` 不一致 | 算法 `10` 不一致 |
| --- | --- | --- | --- |
| A 原移植 | 逐步单精度，三次舍入 | 3 | 1 |
| **B `Vector4` 语义** | double 求值后收窄一次，再开方 | **0** | 1 |
| C `Vector3` 语义 | double 求值，直接开方 | 6 | 0 |

B 正确：约 50 颗算法 `4` 行星零不一致。C 在算法 `10` 上看似更好，但这是样本太小造成的假象——最初只用了算法 `4` 的行星 `1403` 与算法 `10` 的行星 `2402` 各一颗做判据，B 与 C 各中一颗，据此一度改成了 C，扩大样本后才被推翻。两个算法的这段代码结构完全相同，不可能需要不同语义；B 下算法 `10` 剩余的那一颗（seed 26 行星 2402）是算法 `10` 自身的另一处问题，与归一化无关。

**教训**：这类差异只在数值贴近截断边界时才显形，单颗行星完全可能被错误规则蒙对。任何浮点语义的选择都必须用足够多的行星验证。

另外 `Vector2::normalized()` 原本走 `sqrt(double(sqrMagnitude()))`，而 `sqrMagnitude()` 会把平方和收窄成 float，多舍入了一次；`Vector2.magnitude` 是 `(float)Math.Sqrt(x * x + y * y)`，平方和留在 double，已按此修正。`Quaternion::normalized()` 走 `Mathf.Sqrt(Dot(q, q))`，`Dot` 返回 float32，属于会收窄的情形，保持原样。

排查过程与结论如下。

### 定位

对行星 `1403` 与对照行星 `1302`(同为算法 `4`、theme `11`、radius `200`、modX `0`、modY `1`,唯一变量是 `planet.seed`)逐字节导出并比对 `biomoData` 与 `heightData`:

| 行星 | `biomoData` 不一致 | `heightData` 不一致 |
| --- | --- | --- |
| `1302`(对照) | 0 / 161604 | 0 / 161604 |
| `1403` | **1** / 161604(索引 46463,游戏 `18`、移植 `19`) | 0 / 161604 |

### 排除"边界巧合"

导出该顶点的全部中间量后,`(float)(num11 * 100.0) = 19.0000324`,高出字节边界 `19.0` 约 `3.2e-5`,相当于 float32 在该量级下的约 17 个 ULP。这不是一次舍入平局,而是量级约 `3.2e-7` 的真实数值偏差。此前"极少数边界穿越"的判断由此被推翻。

### 缩小范围

该顶点上 `num14 * 2.0 + 0.8 = -0.229`,被 `Mathf.Clamp` 截到 `0`,因此 `num14` 完全不参与结果:

```
num11 = 0 + num17 * 0.5 + num13 * 0.63 = 0.19000032323251115
```

只剩 `num17`(干扰球循环)与 `num13`(噪声)两个来源,而干扰球的坐标正来自上述归一化。

### 验证

平方和改为双精度求值后:

| 量 | 修正前 | 修正后 |
| --- | --- | --- |
| `num17` | 0.30531998206011812 | 0.30531673878910226 |
| `num11 * 100` | 19.000032323251116 | 18.999870159700322 |
| `biomoData[46463]` | `19` | `18`(与游戏一致) |

对照行星 `1302` 仍为 0 处不一致,即修正没有破坏原本正确的结果。

另外 `Vector2::normalized()` 有同类问题:它走 `sqrt(double(sqrMagnitude()))`,而 `sqrMagnitude()` 会把平方和收窄成 float,等于多舍入一次。`Vector2.magnitude` 是 `(float)Math.Sqrt(x * x + y * y)`,平方和写在表达式里、留在 double,已一并修正。`Quaternion::normalized()` 走 `Mathf.Sqrt(Dot(q, q))`,`Mathf.Sqrt` 的形参是 float32 位置,属于会收窄的情形,保持原样。

## 根因三：托管数学例程按整表达式收窄，而不是逐运算收窄

前两节的规则同样适用于 `UnityEngine` 与游戏 `Maths` 里的托管数学例程。判据是**参考代码把值写进 float32 位置的次数**：写进 float 局部变量、float 字段、float 实参（含构造函数实参）、float 返回值各算一次，其余全部留在 double。

按此逐条核对后修正的例程：

| 例程 | 参考写法 | 应有的收窄次数 | 原移植 |
| --- | --- | --- | --- |
| `Vector3.Cross` | 三个分量直接作为 `Vector3` 构造函数实参 | 每分量 1 次 | 每分量 3 次 |
| `Vector3.Dot` | 返回 float32 | 1 次 | 5 次 |
| `Maths.QRotate` / `QInvRotate` | `num`、`num2` 是 float 局部；三个分量是构造函数实参 | 各 1 次 | 各 2–6 次 |
| `Quaternion.operator*` | 四个分量是构造函数实参 | 每分量 1 次 | 每分量 7 次 |
| `Quaternion.operator*(Q, Vector3)` | `num`..`num12` 是 float 局部（各一次乘法，与移植一致）；三个结果赋给 float 字段 | 结果各 1 次 | 各 6 次 |
| `Maths.QInvRotateLF` | `(double)(q.w * q.w)`——两个 float 字段相乘后转 double，从未写入 float32 位置 | 0 次 | 1 次 |
| 算法 `10` 的 `(double)(ellipses[k].w * ellipses[k].w)` | 同上 | 0 次 | 1 次 |
| 算法 `10` 的 `double num28 = 1f - Mathf.Sqrt(...)` | 赋给 double，只有 `Mathf.Sqrt` 的结果收窄 | 1 次 | 2 次 |

这批修正把出生行星的整条位姿链打通了。以 seed 0 的出生行星为例，逐阶段与游戏比对（`posePos`、`poseRot`、`starDirRaw`、`starDir`、`axis0`、`axis1`、`birthPt`、`birthResourcePoint0/1`）全部逐位一致。

出生行星之所以特别敏感：`PlanetData.GenBirthPoints` 用 `rawData.QueryHeight(...) > realRadius + 0.2f` 做 256 × 10 次接受/拒绝判定，任何 1 ULP 偏差都可能翻转某次判定，导致选中完全不同的出生点，进而整颗行星的矿脉布局改变。所以它要么完全一致，要么差得很远，没有中间状态。

## 已解决：用 IDA 复刻 `Quaternion.AngleAxis`

`Quaternion.AngleAxis` 是 `[FreeFunction("QuaternionScripting::AngleAxis")]`，实现在 `UnityPlayer.dll`。此前用 7 种半角写法配合正确舍入的 sin / cos 去拟合，最多只能命中 6 个实测角中的 4 个，一度判定「无法对齐」。**该结论是错的**，改用反汇编后已逐位复刻。

### 定位函数地址

不要去猜 icall 表的对齐关系（名字表在 `.rdata` 的 `0x181908BA8`，共 3239 项，但它没有交叉引用，旁边那段 3502 项的函数指针表怎么对齐都对不上）。可靠办法是在游戏进程内直接问 Mono——Mono 把 `MonoMethod*` 就放在 `RuntimeMethodHandle.Value` 里：

```csharp
var mi = typeof(Quaternion).GetMethod("AngleAxis_Injected", BindingFlags.NonPublic | BindingFlags.Static);
IntPtr fn = mono_lookup_internal_call(mi.MethodHandle.Value);   // mono-2.0-bdwgc.dll
long rva = fn.ToInt64() - GetModuleHandleA("UnityPlayer.dll").ToInt64();
```

插件的 `Mode = icall` 就是干这个的，实测结果（IDA 基址 `0x180000000`）：

| 函数 | RVA | IDA 地址 |
| --- | --- | --- |
| `Quaternion::AngleAxis_Injected` | `F3130` | `0x1800F3130` |
| `Quaternion::FromToRotation_Injected` | `F2D20` | `0x1800F2D20` |
| `Vector3::Slerp_Injected` | `F2C30` | `0x1800F2C30` |

### AngleAxis 的真实实现

```c
v8  = (x*x + y*y) + z*z;                      // 单精度逐步累加
v9  = sqrtf(v8);
if (v9 <= 1e-6f) return <identity>;
v10 = ((a/360f + a/360f) * 3.1415927f) * 0.5f;  // 半角
v11 = cosf(v10);
v12 = sinf(v10) / v9;
q   = (x*v12, y*v12, z*v12, v11);
```

两个关键点靠读源码是猜不出来的：

1. 半角用的是**单精度除法** `a / 360f` 再乘**单精度 pi**，因此它不等于 `a * C` 的任何常数形式——这正是之前所有拟合都失败、且解出的常数区间互相矛盾的原因。
2. 轴不预先归一化，而是用轴长去除 sin。

### sinf / cosf 也必须复刻

`AngleAxis` 调用的是 UCRT 的 `sinf` / `cosf`（`UnityPlayer.dll` 内 `0x18185F280` / `0x18185ED80`），它们是**多项式近似**，不是正确舍入，经常与 `std::sin` 差 1 ULP。且 CRT 按 CPU 特性分派，游戏走的是 **FMA3** 分支，多项式每一步都是 `vfmadd`，与普通双精度求值结果不同。

已按反汇编在 `dspugen/terrain/ucrt_trig.cc` 中复刻：`|x| <= pi/4` 直接用多项式；超出则用 Cody-Waite 二段式 pi/2 约简（`2/pi = 0.6366197723675814`、`hi = 1.5707963267341256`、`lo = 6.077100506506192e-11`），再按象限选 sin / cos 多项式并按 `(象限∈{2,3}) XOR (x<0)` 取反。

实测 6 个角度（seed 0 与 seed 16 出生行星的 `orbitLongitude` / `orbitInclination` / `obliquity`）的 sin 与 cos **全部逐位一致**。

### 规模验证

101 个 seed、24,586 颗行星：不一致的行星从 **8 颗降到 3 颗**，之前纯属 `AngleAxis` 引起的出生行星错误（seed 17、37、56、65）全部消失。

## 已解决：复刻 `Quaternion.FromToRotation`

`GenerateVeins` 给每个矿脉簇算朝向时用 `Quaternion.FromToRotation(Vector3.up, normalized)`，也是原生函数（`0x1800F2D20`），同样按反汇编复刻：

```c
v9  = sqrtf((fx*fx + fy*fy) + fz*fz);          // 单精度逐步累加
v14 = sqrtf((tx*tx + ty*ty) + tz*tz);
if (v9 < 1e-5f || v14 < 1e-5f) return <identity>;
from_n = from / v9;  to_n = to / v14;
sub_180599F70(m, from_n, to_n);                // Rodrigues 旋转矩阵
sub_18059D900(m, &q);                          // 矩阵转四元数 + 单精度归一化
```

关键点：它**不是**常见的「半程向量」写法，而是先构造 3x3 旋转矩阵再转四元数，两步的浮点舍入与半程向量法完全不同。矩阵按
`h = (1 - cos) / |cross|²` 的 Rodrigues 形式构造，转四元数时按 trace 正负分两支、最后再做一次单精度归一化。

修正后 seed 22 行星 3601 与 seed 85 行星 4502 均与游戏完全一致。

## 当前状态：101 seed / 24,586 颗行星零不一致

| 阶段 | 不一致的行星 |
| --- | --- |
| 复刻 `AngleAxis` 之前 | 8 |
| 复刻 `AngleAxis` 之后 | 3 |
| 复刻 `FromToRotation` 之后 | 1 |
| 恒星与行星字段按 Mono 语义对齐之后 | **0** |

最后残留的一处是 seed 10 的出生行星 104。它的 `orbitRadius` 与 `orbitalPeriod` 与游戏差 1 ULP，经
`PredictPose` 传导到出生点的 256 x 10 次接受/拒绝搜索并翻转了其中一次判定。**这不是矿量或地形代码的问题**，而是下面那一项，随字段对齐一并消失。

## 已完成：`star.cc` / `planet.cc` 的 Mono 语义

这是达成「零差异」的最后一项，现已完成：**256 个 seed、16,384 颗恒星、62,541 颗行星，全部字段逐位一致（差异 0）**。

### 前半程：按批次盲改，卡在 20,761

方法是反编译 `StarGen` 逐句核对 float32 存储点，每改一批用 `--meta 0 100 64` 对 101 个 seed 做全字段比对：

| 批次 | 差异字段总数 | `mass` |
| --- | --- | --- |
| 起点 | — | 4,000+（62%） |
| 1. `RandNormal` 整体双精度、返回时收窄一次 | 52,164 | 1,524 |
| 2. `num12` 的 clamp 与三个特殊星型的 `mass` 分支 | 38,757 | 624 |
| 3. `age`、`num13` 的 log 链、`num14`、`color`、`habitableRadius`、`temperature` 的 `Math.Log10(num14 + 4f)` | 32,962 | 691 |
| 4. `SetStarAge` 的复合赋值（`x *= a * b` 是 `x = x * (a*b)`，整体双精度、存储时收窄一次） | 32,662 | 652 |
| 5. `Mathf.Lerp` 的 float 特化 | 20,761 | 126 |

第 3 批让 `mass` 从 624 涨到 691 一度令人费解——`mass` 明明在那些语句之前就算完了。原因是 `SetStarAge` 会**再次修改 `mass`**，而它的分支由 `age` 决定：`age` 修对之后，部分恒星换了分支，暴露出 `SetStarAge` 里同样存在的逐步舍入问题。第 4 批修掉后回落到 652。**字段之间存在后续改写，单看一个指标的升降会误判。**

第 5 批的 `Mathf.Lerp` 收益最大：`util::lerp<T>` 是泛型模板，对 float 舍入三次（`b - a`、`* clamp01(t)`、`a + ...`），而 `Mathf.Lerp` 返回 float32，整个 `a + (b - a) * Clamp01(t)` 只在返回时收窄一次。它经 `num11`（恒星等级插值）直接影响 `RandNormal` 的均值，进而影响 `mass` 与所有派生量。

到此为止 27 个字段已 100% 一致，但剩下 19 个字段停在 20,761，且**无法判断下一步该改什么**——`temperatureBias` 占了 13,123，而按规则把它改成「整体双精度、收窄一次」反而涨到 17,250。当时的结论是「规则也会判断错」，其实不是。

### 突破口：用游戏自己的输入做纯离线判据

停滞的真正原因是**指标被上游污染**：`temperatureBias` 的输入 `num23 = sunDistance / habitableRadius` 本身还没对齐，在错误的输入上把舍入改「对」只会让误差重新分布。build-and-measure 循环分不清「公式错了」和「输入错了」。

解法是绕开 C++ 完全离线判定：`Mode = meta` 的导出里既有某个字段的**结果**，也有它的**全部输入**。于是可以在 Python 里用游戏自己的输入重算这个字段，逐个候选语义与游戏自己的结果比对——**上游误差按构造为零**，报 0 不一致的那个候选就是游戏的语义，不需要重新编译，也不需要先对齐上游。

工具在 `tools/parity/probe_fields.py`（配 `probe_common.py`，用 numpy float32 建模每一个收窄点）。第一次运行就定了 `temperatureBias`：

| 候选 | 不一致 / 21,169 |
| --- | --- |
| 逐步单精度（当时的实现） | 13,062 |
| `num23` 不收窄、保持双精度 | 10,376 |
| **`num23` 收窄成 float32，其余整体双精度、收窄一次** | **0** |

### 根因四：C# 的 `float` 字面量 widen 后不是十进制原值

上一轮「改成双精度反而更差」的真正原因在这里。把 C# 的 `1.2f / (num23 + 0.2f) - 1f` 移植成双精度时写了 C++ 的 `1.2` 和 `0.2`，但 Mono 广播的是 **`(double)(float)1.2 = 1.2000000476837158`** 和 **`(double)(float)0.2 = 0.20000000298023224`**，不是十进制的 `1.2` / `0.2`。同一个探针把这个写法也测了一遍：

| 常量写法 | 不一致 / 21,169 |
| --- | --- |
| `dbl(1.2f)` / `dbl(0.2f)` | 0 |
| C++ 双精度字面量 `1.2` / `0.2` | 17,197 |

17,197 与上一轮实测的 17,250 吻合。**所以「按规则改反而更差」不是规则错了，是常量写错了。**

这条同样出现在 `color`（`* 0.2f` 写成了 `* 0.2`，1,054 处不一致）、`age` 的全部五个分支（`* 0.7f + 0.2f` 等写成了 `* 0.7 + 0.2`，666 处）、`SetStarAge` 的七处复合赋值常量（`0.001f`、`0.4f`、`0.2f`、`0.1f`、`0.04f`、`0.15f`）。

**移植规则补充：把 C# 的 float 表达式改写成 double 求值时，表达式里的每一个 `Nf` 字面量必须写成 `dbl(Nf)`，不能写成 double 字面量 `N`。** 只有恰好二进制可精确表示的值（`0.5f`、`1f`、`2.5f`、`3f`、`1.5f`、整数）两者相同。

### 逐项修正与实测

| 修正 | 差异字段总数 |
| --- | --- |
| 起点 | 20,761 |
| `temperatureBias` 的常量与收窄点 | 7,803 |
| `color` 常量（两处）、`dysonRadius` 的 `physicsRadius * 1.5f`、出生恒星分支的 Mono 语义 | 6,622 |
| `age` 五个分支的常量、`SetStarAge` 七处复合赋值常量 | 3,870 |
| `orbitRadius` 两个分支的收窄点 | 1,458 |
| `gas` 的 `num6 *= (float)rand * 0.19090915f + 0.9090909f` | 0（101 seed） |
| `rotationPeriod` 的 `Mathf.Pow` | **0（256 seed）** |

其中三处不是常量问题：

- **`dysonRadius`**：`(double)(starData.physicsRadius * 1.5f)` 里的 `(double)` 是 no-op——这个乘积从没写进 float32 位置，IL 里全程是双精度。移植不但把它收窄成了 float，还把 `dysonRadius * 40000.0 < physicsScaled` 预先除成 `dysonRadius < float(physicsScaled / 40000.0)` 来比较，多了一次收窄。探针实测：精确 IL 形式 0/6,464，收窄版与预除版各 20。
- **`orbitRadius`**：卫星分支的 `(1600f * orbitIndex + 200f) * Mathf.Pow(...) * Mathf.Lerp(...) + realRadius` 整条链只在存入 `num16` 时收窄一次，移植每一步都收窄；主星分支的 `num17` 同理。
- **`rotationPeriod`**：`Mathf.Pow(num16, 0.25f)` 被移植成了 `std::pow(float, float)`，即 `powf`。`Mathf.Pow` 是 `(float)Math.Pow(double, double)`，先双精度再收窄；`powf` 是直接单精度求值。两者绝大多数时候相同，62,541 颗行星里差了 1 颗——**正好是双重舍入分歧应有的频率**。改用 `util::mathfPow` 后归零。

### 最终结果

`--meta 0 255 64`（256 seed、16,384 颗恒星、62,541 颗行星）与游戏导出逐字段比对：**差异 0**。GCC Debug、GCC Release、Clang Release 三套构建输出逐字节相同。

矿脉路径同步归零：`tools/verify_vein_parity.py` 对 101 个 seed、24,586 颗行星（21,169 生成地形 + 3,417 气态，无排除）**全部匹配**。此前唯一残留的 seed 10 出生行星 104 随 `orbitRadius` / `orbitalPeriod` 对齐一并消失，与预期一致。

### 基线更新

本项改动改变了既有输出，已一次性更新：

| 基线 | 旧值 | 新值 |
| --- | --- | --- |
| `tests/fixtures/oracle-v1.txt` | `abade2f1…` | `d48df98e29ff60f00658df2b5a5f4b8d57dd915c80bb8557498b913d6a63d8da` |
| `galaxy_digest` 16 星 | `63981f89400edae8` | `89abf279dec86713` |
| `galaxy_digest` 64 星 | `be0243dc0a38f6bd` | `3a20b6a07a8dca04` |

两个 `galaxy_digest` 的恒星数（32,000 / 128,000）与行星数（105,443 / 488,267）不变，`pose_sink` 三个校验和（`9d727cec36ceed84`、`3277ef00c03ce146`、`1dca6c7f8fab9af3`）不变——说明改动只移动浮点值，没有改变任何整数判定、对象数量或恒星位置。`tests/fixtures/vein-amounts-v1.txt` 保持 `5aade6936c1421af…`（seed 0、1 的矿量汇总不受影响）。

## 气态巨星:不再作为例外排除

`PlanetAlgorithm0` 的实现是:

```csharp
data.heightData[i] = (ushort)((double)planet.radius * 100.0);
data.biomoData[i] = 0;
```

`PlanetGen` 给气态行星 `precision = 64`(非气态为 `200`),即 `dataLength = 65 * 65 * 4 = 16900`。用这两个常量直接反算 FNV-1a 摘要,与游戏输出逐位吻合:

| 数据 | 反算值 | 游戏输出 |
| --- | --- | --- |
| 16900 个零字节 | `6E363895450CBBF5` | `6E363895450CBBF5` |
| 16900 个 `uint16(8000)`(`radius = 80`) | `F1FE04D9D3234535` | `F1FE04D9D3234535` |

也就是说气态行星的 `biomoData` 本来就是全零,与移植留空的缓冲区完全相同;只有 `heightData` 是常量填充。既然只有约 1.7 万个元素(真实算法是约 16 万),移植改为直接生成该常量地形并计算摘要,不再需要在比对中把 `algo = 0` 排除。seed 0 现在是 258 颗行星零例外全等。

## 算法 `4` 的单颗 `biomoData` 差异(已修复,记录留档)

seed 0 上算法 `4` 共 9 颗行星，只有行星 `1403` 的 `biomoData` 摘要不一致，其余 8 颗一致；该行星的 `heightData` 摘要与游戏逐位相同。

九颗行星的 `theme`（均为 `11`）、`radius`（均为 `200`）、`modX`（`0`）、`modY`（`1`）完全相同，唯一变量是 `planet.seed`，所以这不是算法级移植错误。

成因是敏感度差异。算法 `4` 末尾两个输出对同一批中间量的权重不同：

| 中间量 | 在 `heightData` 中的权重 | 在 `biomoData` 中的权重 | 比值 |
| --- | --- | --- | --- |
| `num17` | 1.0 | 0.5 | 0.5x |
| `num13` | 0.4 | 0.63 | 1.6x |
| `num14` | 0.08 | 2.0 | 25x |

`num14` 在 `biomoData` 中的权重是 `heightData` 的 25 倍，因此一个小到无法移动任何一个高度字节的偏差，仍可能让某个顶点的 `(float)(num11 * 100.0)` 跨过一次字节截断边界。九颗行星约 145 万个顶点里只出现一次，与"极少数边界穿越"一致，而非系统性偏差。

该差异不影响矿脉输出，有两条独立证据：

- 反编译的 `PlanetAlgorithm.GenerateVeins` 只读取 `data.QueryHeight(...)`（即 `heightData`），整个 `PlanetAlgorithm` 基类没有任何 `biomoData` 引用。
- 实测中包括 `1403` 在内的全部 258 颗行星，矿点数与矿量都完全一致。

按 `CLAUDE.md`，地形生成本身不在游戏一致性范围内，`biomoData` 在本仓库只作为对照摘要存在，没有其他读者。逐字节定位所需的 `biomo-<seed>-<id>.bin` 导出已在 mod 与 `--dump-height` 两侧就位，留待需要时使用。

## 尚未处理

- 顶点网格文件本身尚未加入仓库(约 1.9 MB)。不加入则精确矿量无法达到逐位一致——`Vector3.Slerp` 是引擎原生实现,无法重写对齐,见「可选的收尾方案」。这是目前唯一的未决项。

## 顶点网格随仓库分发（已决定）

要让顶点网格逐位正确，只有把游戏导出的 precision 200 网格作为数据随仓库分发一条路——这正是游戏自己的做法（`PlanetRawData.verts200` / `indexMap200` 计算一次后复制复用）。`indexMap` 可由顶点纯整数推导，无需一并存储。代价是约 1.9 MB 的二进制文件（仓库现有 `Prototypes/` 合计 1.8 MB）。

**已决定入库**：`tests/fixtures/game/verts-200-game.bin` 连同几个小参照文件（合计约 2.8 MB）随仓库分发，这样精确矿量的逐位一致可以从干净的 clone 复现。

`tests/fixtures/game/` 下仍在 `.gitignore` 中的是三类可再生的大文件：`meta-parity-game.txt`（38 MB，字段对照，`Mode = meta` 约 40 s 重新导出）、`libm-game.txt`（8.5 MB）、`vein-parity-game-0to96.txt`（6.6 MB，101 seed 矿脉参照，`Mode = veins` 约 44 s/seed）。跑完整回归前需要先重新导出后者。
