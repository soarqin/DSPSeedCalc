# 精确矿量对齐 · 交接说明

背景与全部证据在 `docs/vein-amount-parity-evidence.md`。本文只写**接手后要怎么继续**：环境怎么搭、测量循环怎么跑、下一步做什么、有哪些坑。

## 1. 当前状态

**两条线都已零差异。**

矿脉路径（真正的交付物）：

- 101 个 seed、24,586 颗行星（21,169 生成地形 + 3,417 气态，**无排除**）与游戏逐位全等：`heightData` 摘要、`biomoData` 摘要、矿点数、基础矿量。
- 前提是装入游戏导出的 precision 200 顶点网格（`tests/fixtures/game/verts-200-game.bin`）。

恒星/行星字段（`--meta` 比对）：

- 256 个 seed、16,384 颗恒星、62,541 颗行星，**全部字段差异 0**。

三套工具链（GCC Debug、GCC Release、Clang Release）输出逐字节相同，`dspugen_vein_tests` 通过。

基线已一次性更新并交叉验证：`tests/fixtures/oracle-v1.txt` = `d48df98e…`，`galaxy_digest` 16 星 `89abf279dec86713` / 64 星 `3a20b6a07a8dca04`。`pose_sink` 三个校验和与恒星/行星数量均未变化，`tests/fixtures/vein-amounts-v1.txt` 保持 `5aade6936c1421af…`。

## 2. 顶点网格已入库

`Vector3.Slerp` 的实现在 `UnityPlayer.dll` 原生代码里，重写无法逐位对齐（实测三种写法都比现有实现更差，差距来自 Unity 自己构建的 `acos` / `sin`）。唯一可行的办法是把游戏导出的网格当数据分发——参考实现自己也是这么做的（`verts200` 算一次后复制复用）。

`tests/fixtures/game/verts-200-game.bin`（1.9 MB）连同几个小参照文件已入库（合计约 2.8 MB），精确矿量的逐位一致可以从干净的 clone 复现。

**仍需重新导出才能跑的**：`meta-parity-game.txt`（38 MB，字段对照，`Mode = meta` 约 40 s）与 `vein-parity-game-0to96.txt`（6.6 MB，101 seed 矿脉参照，`Mode = veins` 约 44 s/seed，要 75 分钟）。两者都在 `.gitignore` 中。

## 3. 环境与工具

### 游戏内参照（BepInEx 插件）

源码 `tools/vein-oracle-mod/`，构建后复制部署：

```bash
cd tools/vein-oracle-mod && dotnet build -c Release
cp bin/Release/DspVeinOracle.dll "D:/Steam/steamapps/common/Dyson Sphere Program/BepInEx/plugins/DspVeinOracle/"
```

配置 `<游戏目录>/BepInEx/config/dspseedcalc.veinoracle.cfg`：

```
Seeds = 0-255          # 支持 "0"、"0-255"、"0,2,7,10" 三种写法
StarCount = 64
OutputDirectory = E:/Projects/DSPSeedCalc/tests/fixtures/game
QuitWhenDone = true
Mode = veins           # veins / meta / icall / noise / libm / algo3
DumpVertices = false   # 顶点网格已导出，不用重复导
HeightPlanets =        # 填行星 id 则额外导出该行星的 heightData 与 biomoData 原始字节
```

跑一次：

```bash
cd "D:/Steam/steamapps/common/Dyson Sphere Program"
rm -f BepInEx/LogOutput.log
./DSPGAME.exe -batchmode -nographics
```

`Mode = veins` 约 **44 s / seed**（256 seed 要 3 小时以上），`Mode = meta` 只生成恒星与行星字段，256 个 seed 只要 40 s——**做字段对齐时永远用 meta**。插件每跑完一个 seed 就把结果刷盘一次，中途杀进程不会丢已完成的部分。

### IDA（分析 UnityPlayer.dll 原生函数）

```bash
python -m pip install "C:/Software/IDAPro94/idalib/python/idapro-0.0.9-py3-none-any.whl"
python "C:/Software/IDAPro94/idalib/python/py-activate-idalib.py" -d "C:/Software/IDAPro94"
```

数据库已建好在 `ida-work/`（已 gitignore，约 300 MB，首次自动分析约 15 分钟）。脚本模板见 `ida-work/*.py`。

**注意：idalib 会吞掉 Python 的 `print`**，脚本里要把结果写进文件再 `cat`。

**找原生函数地址不要去猜 icall 表**，用插件的 `Mode = icall`——它通过 `mono_lookup_internal_call(mi.MethodHandle.Value)` 在游戏进程内直接问 Mono 要地址，输出 `tests/fixtures/game/icall-addresses.txt`（含可直接在 IDA 里跳转的地址，镜像基址 `0x180000000`）。已知：

| 函数 | IDA 地址 |
| --- | --- |
| `Quaternion::AngleAxis_Injected` | `0x1800F3130` |
| `Quaternion::FromToRotation_Injected` | `0x1800F2D20` |
| `Vector3::Slerp_Injected` | `0x1800F2C30` |
| UCRT `sinf` / `cosf` | `0x18185F280` / `0x18185ED80` |

### 反编译托管代码

```bash
D="D:/Steam/steamapps/common/Dyson Sphere Program/DSPGAME_Data/Managed"
ilspycmd --disable-updatecheck -t StarGen -r "$D" "$D/Assembly-CSharp.dll" > stargen.cs
```

常用类型名：`StarGen`、`PlanetGen`、`PlanetAlgorithm`、`PlanetAlgorithm1..14`、`PlanetData`、`Maths`、`UnityEngine.Vector3`（在 `UnityEngine.CoreModule.dll`）。

## 4. 两个验证循环

### 字段（快，秒级）

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake --build cmake-build-gcc-debug-parity --target dspugen_vein_amount_oracle
./cmake-build-gcc-debug-parity/bin/dspugen_vein_amount_oracle.exe --meta 0 255 64 > cur.txt
python tools/parity/cmpmeta.py cur.txt          # 按字段列出不一致计数，现在应输出 0
```

参照文件 `tests/fixtures/game/meta-parity-game.txt`（256 seed，38 MB，已 gitignore）。

### 矿脉（慢，约 14 s / seed，101 seed 要 24 分钟，放后台）

```bash
python tools/verify_vein_parity.py cmake-build-gcc-release-parity/bin/dspugen_vein_amount_oracle.exe \
  --game tests/fixtures/game/vein-parity-game-0to96.txt \
  --verts tests/fixtures/game/verts-200-game.bin
```

脚本用 `subprocess.run` 捕获输出，跑完才打印，中途没有进度可看。

## 5. 定位浮点语义：先用离线探针，不要 build-and-measure

`tools/parity/probe_fields.py` 是本轮最有效的工具，**遇到新的字段差异先用它**。

原理：`Mode = meta` 的导出里既有某个字段的结果，也有它的全部输入。于是可以在 Python 里用**游戏自己的输入**重算这个字段，把几个候选语义各测一遍，与**游戏自己的结果**比对。上游误差按构造为零，报 0 不一致的那个候选就是游戏的语义。不用重新编译，不用先对齐上游，一次运行就出结论。

```bash
python tools/parity/probe_fields.py                    # 默认 seed 0-100
python tools/parity/probe_fields.py <dump> <maxseed>
```

`probe_common.py` 里有按 Mono 语义建模的 `Mathf` 例程（`mpow`、`mlerp`、`clamp01`…）和 `w()`（float 字面量 widen 后的双精度值），加新字段照着现有 `check(...)` 写即可。

**为什么必须这样做**：build-and-measure 分不清「公式错了」和「输入错了」。上一轮因此把一个**正确**的 `temperatureBias` 修改判成回退（13,123 → 17,250），并写下「按反编译套规则也可能错」的错误结论；真正的原因是常量写错了（见下）。这个循环卡了整整一轮。

## 6. 四个必须知道的坑

1. **C# 的 `Nf` 字面量 widen 后不是十进制的 `N`。** 把 float 表达式改写成双精度求值时，`0.2f` 必须写成 `dbl(0.2f)` = `0.20000000298023224`，写成 C++ 的 `0.2` 就错。这一条造成了本轮 70% 以上的差异（`temperatureBias`、`color`、`age`、`SetStarAge`），也是上一轮误判的根源。只有二进制可精确表示的值（`0.5f`、`1f`、`1.5f`、`2.5f`、`3f`、整数）两者相同。

2. **`std::pow(float, float)` 不是 `Mathf.Pow`。** 前者是 `powf`（单精度求值），后者是 `(float)Math.Pow(double, double)`（双精度求值后收窄）。用 `util::mathfPow`。同理 `mathfLog` / `mathfSqrt`。这处差异 62,541 颗行星里只出 1 颗，很容易漏。

3. **单个指标变差不一定是改错了。** 除了上游污染，还有后续改写：`mass` 624→691 曾让人回退一个正确的修改，真正原因是 `SetStarAge` 会**再次改写 `mass`**，而它的分支由 `age` 决定，`age` 改对后暴露了 `SetStarAge` 里同样的问题。改一个字段前先 `grep` 它在别处是否被重新赋值。

4. **样本太小会选中错误的规则。** `Vector4` 归一化用 2 颗行星做判据得出了错误结论，扩到约 50 颗才纠正。任何浮点语义的选择都要用足够多的行星验证——探针天然覆盖两万颗以上，用它就不会踩这个坑。

## 7. 收尾提醒

- 用完把 mod 从 `<游戏目录>/BepInEx/plugins/DspVeinOracle/` 移除。
- `ida-work/`、`tests/fixtures/game/` 下的大文件已在 `.gitignore` 中。
- 改动了字段计算就要同步更新三个基线（`oracle-v1.txt` 与两个 `galaxy_digest`），并确认 `pose_sink` 校验和及恒星/行星数量**不变**——变了说明动到了整数判定或位置生成，那是另一类问题。
