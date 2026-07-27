# Viewer 复现证据记录

本文记录 `UIVirtualStarmap` 和「Galaxy Select」渲染复现中已经调查过的内容，避免在相同问题上重复执行资产导出、反编译、Shader 反汇编和 Unity 原生代码分析。

## 使用规则

开始新一轮调查前，先按以下顺序处理：

1. 检查本文的「已确认结论」「已排除方向」和「待解决问题」。
2. 同一参考版本下，不重新调查已经确认或排除的内容。
3. 只有版本变化、证据缺失、新证据冲突或待解决问题明确依赖该区域时，才允许重新打开调查。
4. 重新调查前，先记录重开原因和预期新增信息；完成后补充证据位置、结论变化和实现状态。
5. 截图只用于识别问题类别和最终验收，不用于反推宽度、比例或 Shader 常量。

状态定义：

- 「已确认」：有游戏资产、托管代码、GPU 程序或 Unity 原生实现作为直接证据。
- 「已排除」：证据表明该项不存在差异，或实测变更不能解释目标问题。
- 「待实现」：已经找到明确差异，但当前代码尚未修正。
- 「待验证」：实现已完成，但还缺少针对目标问题的运行结果。
- 「未调查」：没有足够证据，不得写成结论。

## 参考版本

- 游戏：Dyson Sphere Program `0.10.34.28529`
- Steam build ID：`23109513`
- Unity：`2022.3.62f3c1`
- 脚本后端：Mono
- `Assembly-CSharp.dll` SHA-256：`AE0BA95F75BD879A62AA4CE253B2AB78EAA4FB3C7C595F5E1FEE75EBE0E0EF85`
- `Assembly-CSharp.dll` MVID：`ece4a40e-5e73-43f4-a9f8-4e74970b5942`
- `UnityPlayer.dll` SHA-256：`B07814A510EEA3F8B66A25117AB602237037D74668E6CE1F8DE92F60C0EF28C4`
- `level0` SHA-256：`6D9C2ABBCAC25C06D0A0F15E284858E394E98BC04AE75618D5081B8130C35170`
- `sharedassets0.assets` SHA-256：`1752470557875D23B7D99202D5867187B469A7D0339B3DB02067433A42115B26`

以上任一核心版本或文件哈希变化后，相关结论必须标记为需要重新验证，不能直接沿用。

## 结论索引

| 区域 | 状态 | 结论 | 是否可能解释当前线芯偏粗 |
| --- | --- | --- | --- |
| `UIVirtualStarmap` 连接逻辑 | 已确认 | 只创建一条无向边、设置颜色、裁剪端点和写入两个位置 | 否，当前实现已对应 |
| LineRenderer 序列化宽度 | 已确认 | 完整世界宽度为 `1`，宽度曲线恒为 `1` | 否，不应经验缩放 |
| Transform 层级缩放 | 已确认 | 整条父级链均为单位缩放 | 否 |
| Unity 两点线宽计算 | 已确认 | 完整宽度乘 `0.5` 后向两侧偏移 | 否，当前半宽系数正确 |
| Unity `Alignment=View` 侧向量 | 已确认 | 相机空间计算，横向量只按 XY 长度归一化 | 部分；当前主体公式正确 |
| 两点线端点侧向量选择 | 已确认并实现 | Unity 最终对两个端点都使用终点候选侧向量；`11.png` 证明修正无可见宽度效果 | 已排除为线芯偏粗主因 |
| 两点线 emitter UV / POSITION.w / 索引 | 已确认 | 四个顶点使用 `V=1/0`、`POSITION.w=1`，索引为 `(0,1,2)`、`(1,3,2)` | 否，当前输入值和拓扑等价 |
| 顶点 Shader 矩阵 | 已确认 | 官方的 ObjectToWorld 在当前单位 Transform 下为 identity；viewer 在 CPU 生成世界坐标后应用等价 VP | 否 |
| 顶点输入和 Unity 内建量替代 | 已确认 | 首条 line batch 已捕获 program、attribute、uniform、FBO、采样数和纹理绑定；均与 viewer 的预期值一致 | 否 |
| HDR scene MSAA | 已确认并实现 | 官方为 8 样本；viewer 现用 8-sample RGBA16F / depth24 FBO 并在 Bloom 前 resolve；`12.png` 证明它未解决线芯偏粗 | 已排除为主因 |
| 片元 Shader 和材质 | 已确认 | profile、距离缩放、裁剪、颜色和混合常量均已恢复 | 否 |
| `star-line` 纹理 | 已确认 | 尺寸、BC1 mip、方向、采样和重建数据已验证 | 否 |
| Galaxy 相机 | 已确认 | FOV、距离、目标、俯仰范围均来自官方对象和运行路径 | 否 |
| Bloom / Color Grading | 已确认并实现 | 属于 HDR 后处理，不改变 ribbon 几何 | 否 |
| FXAA | 已确认并实现 | 官方启用 Default preset 39；加入后 `10.png` 线宽基本不变 | 已排除为主因 |
| Dithering | 已确认但未实现 | 最终低幅度颜色扰动，不参与 FXAA 边缘搜索和几何 | 否 |

### 星系内视图

- 普通恒星后处理：已实现，待截图验收。星系内视图现进入与银河视图相同的 8-sample HDR、Bloom、Color Grading 和 FXAA 管线；恒星仍按官方距离公式切换表面与 Spot，并按 Black、Mass、Spot 顺序绘制。
- 行星实体几何：已实现，待截图验收。`UIStarmapPlanet` 使用固定 `GeoSphere`；viewer 复用字节一致的 `star_mass_*` 网格，使用官方 `realRadius * 2 * 0.00025` Transform 缩放，并移除常驻线框层。
- 行星主题材质：部分实现。viewer 已按 `theme` 和 `themeSeed % 60` 选择官方 thumb 材质的 `_Color.rgb` 与 `_RimColor`，并移除临时程序化条纹。当前 Shader 为近似实现，尚未移植完整 `Planet Thumb New` 和黑色 `_HeightMap` 路径。
- 相机操作：已实现，待交互验收。viewer 已按 `StarmapCamera.CameraControl()` 改为中键旋转、右键滚转、滚轮指数缩放；中键的纵向灵敏度为横向的一半，右键滚转灵敏度为横向的 `0.7`。缩放范围对应官方 `0.0375` 至 `45000`，但为适配离散滚轮输入，单级缩放倍率调整为 `1.2`。双击行星会把视图目标切到该行星，`Z` 恢复恒星系中心。

星图行星证据来自参考构建中的 `UIStarmapPlanet`、`ThemeProto`、`PlanetGen`、`level0`、`resources.assets` 和 `sharedassets0.assets`。Prefab 关系为 `UIStarmap:46333 -> planetUIPrefab:46827 -> starmap-planet-obj:2667`，实体网格为 `sharedassets0.assets:1153`（`GeoSphere`，2617 个顶点、5120 个三角形），材质 Shader 为 `resources.assets:10085`（`VF Shaders/Forward/Planet Thumb New`）。官方 thumb 材质不序列化 `_HeightMap`，远程未加载行星也可以没有高度 Cubemap；因此完整 Shader 移植时应绑定黑色 Cubemap，不应虚构静态地形纹理。

## 已确认结论

### 托管运行逻辑

`UIVirtualStarmap` 的直接证据为：

- MonoBehaviour：`level0` path ID `49504`
- `connLinePrefab`：LineRenderer path ID `16284`
- 反编译文件：`C:\Users\soar\AppData\Local\Temp\opencode\dsp-decompile\UIVirtualStarmap.cs`

连接线运行逻辑已经确定：

- 遍历 `StarGraphNode.lines`，只在相邻节点索引更大时创建连接，因此每条无向边只创建一次。
- 通过 `.material` 为实例设置 `_LineColorA` 和 `_LineColorB`。
- 端点颜色为 `Color.Lerp(starColor, Color.white, 0.65f)`。
- 只调用两次 `SetPosition`，未在运行时修改 `widthMultiplier`、`startWidth`、`endWidth` 或 Transform 缩放。
- 起点裁剪比例为 `(sizeA * 0.25f + 0.35f) / distance`。
- 终点裁剪比例为 `1 - (sizeB * 0.25f + 0.35f) / distance`。

对应反编译位置为 `UIVirtualStarmap.cs` 的连接遍历和 `SetPosition` 代码。当前实现位于 `viewer/viewer.cc` 的 `DrawGalaxyLines(...)`。

### LineRenderer 资产状态

官方对象链：

- `UI Root` GameObject `4899` / Transform `14657`
- `Galaxy Select Starmap` GameObject `5120` / Transform `14670`
- `virtual-starmap` GameObject `2244` / Transform `14492`
- `star-conn` GameObject `4212` / Transform `14611`
- LineRenderer `16284`

整条 Transform 链的 local position 和 rotation 为单位变换，local scale 均为 `(1,1,1)`。LineRenderer 的序列化状态为：

- `widthMultiplier = 1`
- width curve 为单个常量键 `value = 1`
- `alignment = 0`，即 `View`
- `textureMode = 0`，即 `Stretch`
- `textureScale = (1,1)`
- `useWorldSpace = true`
- `loop = false`
- `numCornerVertices = 0`
- `numCapVertices = 0`

因此，官方输入给几何生成器的完整世界宽度就是 `1`。不存在隐藏的 `1/2`、`1/3`、父级缩放或托管代码宽度覆盖。

主要证据：

- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-connline-research\connline.json`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-connline-research\README.md`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-virtual-starmap\exact-findings.json`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-virtual-starmap\camera\camera-findings.json`

### Unity 原生 LineRenderer 几何

官方 `UnityPlayer.dll` 中已经恢复的调用关系：

- `0x1810819C0`：LineRenderer 批处理入口
- `0x181080F70`：单个 renderer 准备
- `0x1810825E0`：worker callback
- `0x180487AB0`：适用于当前四顶点、16 位索引路径的 builder
- `0x180483D90`：相机空间侧向量预处理
- `0x180489420`：最终顶点发射

宽度公式已经确认：

```text
fullWidth = widthCurve(t) * widthMultiplier * perPointScale
edgeOffset = fullWidth * 0.5
```

相关原生位置：

- `0x1804882CC`：求值 width curve
- `0x1804882D9`：乘 width multiplier
- `0x1804882FE`：乘可选 per-point scale；当前调用为 `1`
- `0x180489441`、`0x18048945D`：乘原生常量 `0.5`
- `0x1804896AF` 至 `0x18048971F`：发射两侧顶点

`Alignment=View` 的候选侧向量算法已经确认：

1. 将端点相对相机的位置和线段方向变换到相机空间。
2. 对射线三个分量分别执行 `copysign(max(abs(component), 0.01f), component)`。
3. 计算相机平面内的叉积分量。
4. 只使用这两个 XY 分量的长度归一化，不使用完整三维向量长度。
5. 将二维侧向量通过相机 right/up 基向量转换回世界空间。

相关原生位置：

- `0x1804842BC` 至 `0x1804842DF`：带符号的 `0.01f` 下限
- `0x1804842E2` 至 `0x180484330`：叉积分量和 XY 归一化
- `0x180484333`：保存当前端点的候选侧向量
- `0x180484580`：两点线跳过 join averaging

最新确认的关键差异是最终候选选择：

```text
sideIndex = min(pointIndex + 1, pointCount - 1)
```

该逻辑位于 `0x1804894C2` 至 `0x1804894F2`。当 `pointCount == 2` 时，起点和终点都读取 `sideCandidate[1]`，即终点射线计算出的候选侧向量。Unity 会计算起点候选，但当前两点发射路径不会使用它。

`viewer/viewer.cc` 的 `DrawGalaxyLineQuad(...)` 已改为使用 `end - camera.position` 计算唯一侧向量。正确差异不是宽度系数，也不是给两个端点分别使用不同侧向量。`11.png` 与 `10.png` 的线芯宽度没有可见变化，因此该差异保留为准确性修正，但已排除为当前问题的主因。

该区域曾经因第一次原生分析只恢复了候选侧向量计算、没有继续追踪最终 emitter 的候选索引而得到不完整结论。本次重开原因是 `9.png` 证明第一轮几何修正无效，属于新运行证据与「几何差异已完整复现」这一假设冲突。新增信息是 `0x180489420` 中的最终候选选择；后续不得再从几何入口重复追踪，而应直接从以上地址和公式继续。

对同一 emitter 的有界补充检查还确认了以下字段：

- `0x1804894E7` 至 `0x18048951E`：两侧横向 UV 由 `0.5 + 0.5 * 1` 和 `0.5 - 0.5 * 1` 生成，即精确为 `1` 和 `0`；Stretch 模式的纵向值由每个点的归一化累计距离提供，两点线精确为 `0` 和 `1`。
- `0x180488E30` 至 `0x180488ECB`：每个顶点步长为 `0x18`，布局为 `POSITION.xyzw` 后接 `TEXCOORD.xy`；位置变换明确使用常量 `1.0` 作为第四分量，因此 emitter 写入 `POSITION.w=1`。
- `0x18048972E` 至 `0x18048977C`：两点线新增顶点 `v0,v1` 后写入 16 位索引 `(v0-2,v0-1,v1-2)` 和 `(v0-1,v1-1,v1-2)`；四顶点实例即 `(0,1,2)`、`(1,3,2)`。
- 当前 `DrawGalaxyLineQuad(...)` 提交的四个顶点虽然 V 顺序相反，但片元公式对 `V-0.5` 取绝对值；raylib 6.0 的 `RL_QUADS` 固定索引为 `(0,1,2)`、`(0,2,3)`。按当前顶点次序，它与 Unity 使用同一条几何对角线，且两个三角形内的 UV 和每顶点距离插值等价。

因此，非 `[0,1]` UV、重复端点、`POSITION.w != 1`、不同 quad 对角线和默认 GLSL `smooth` 插值都已排除为当前线芯平台偏粗的原因。直接证据保存在 `C:\Users\soar\AppData\Local\Temp\opencode\ghidra-emitter-detail.log`；该文件来自既有 Ghidra 工程的只读、有界反编译，没有重新追踪原生入口。

### 材质和 GPU 程序

官方资源：

- Material path ID `115`，名称 `galaxy-select-line`
- Shader path ID `1402`，名称 `Universe/Starmap/Line`
- Texture path ID `531`，名称 `star-line`

材质参数：

- `_Multiplier = 0.3499999940395355`
- `_TileScale = 6`
- `_DistScale = 0.029999999329447746`
- `_DistPower = 0.8999999761581421`
- `_MinDistScale = 0.10000000149011612`
- `_MaxDistScale = 10`

固定渲染状态：

- `Blend SrcAlpha One`
- blend operation 为 Add
- `ZWrite Off`
- `ZTest LEqual`
- `Cull Off`
- render queue `2999`

顶点程序不会扩展线宽。它只执行对象到世界变换、VP 变换、传递 UV，并计算顶点到 `_WorldSpaceCameraPos` 的距离。权威证据：

- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-connline-research\1402_starmap_line_vertex_blob1.asm.txt`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-connline-research\1402_starmap_line_fragment_blob3.asm.txt`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-connline-research\line-shader.json`

片元横向 profile 公式为：

```text
distanceScale = clamp(pow(cameraDistance * 0.03, 0.9), 0.1, 10)
profileCoord = 2 * abs(uv.y - 0.5) / distanceScale * 6
discard when profileCoord > 1
```

当前 `viewer/galaxy_line.fs` 和 `viewer/galaxy_line_web.fs` 已按该公式实现。`_TileScale` 只控制片元 profile 坐标，不是 LineRenderer 世界几何宽度。

### 顶点矩阵和运行输入

根据新的外部审阅意见，本区域做过一次有界复查，原因是需要排除「官方依赖 Unity 的 GameObject 位置转换或相机内建量，而移植环境没有提供」这一具体假设。结论如下：

- 官方顶点程序的两级变换是 `ObjectToWorld * POSITION`，再由 `unity_MatrixVP` 转到 clip space。
- LineRenderer 使用 world space，且官方对象父级链为单位 Transform；因此该对象的 `ObjectToWorld` 是 identity。
- viewer 在 `DrawGalaxyLineQuad(...)` 之前已经生成世界空间顶点，`galaxy_line.vs` 再用 `matViewProjection` 转到 clip space。这是官方两级公式在 identity model matrix 下的合并形式，不是漏掉变换。
- raylib 的 `MatrixMultiply(view, projection)`、矩阵内存布局和 `glUniformMatrix4fv(..., false, ...)` 与 raylib 自身 DrawModel 的 MVP 路径相同。
- `_WorldSpaceCameraPos` 不依赖 Unity 运行时方法。viewer 将 `camera.position` 作为 `SHADER_UNIFORM_VEC3` 显式上传，顶点 Shader 用世界空间顶点计算相同的欧氏距离。
- position 和 UV 分别固定在 attribute location `0` 和 `1`。raylib 的 immediate batch 使用独立的 3-float position VBO 和 2-float UV VBO；Shader 显式补 `w=1`。
- Desktop GLSL 330 和 WebGL2 GLSL ES 300 都原生支持 `gl_Position`、矩阵乘法和 `distance(...)`。Shader 编译或链接失败时 raylib 会回退到默认 Shader，而 viewer 会拒绝该回退并初始化失败；现有 Native 烟雾日志显示 line Shader program 已加载成功。

因此，没有发现「Unity 专用顶点方法在当前环境不支持」的源码级缺口，也不应仅为形式一致增加一个 identity `matModel`。`12.png` 仍未解决问题后，按预先限定的范围捕获了首条 line batch 的运行态。`C:\Users\soar\AppData\Local\Temp\opencode\viewer-linecap-smoke2.log` 记录：活动 program `10` 与 line Shader `10` 一致；position / UV attribute location 为 `0/1`；活动 FBO 为 multisample scene FBO `1`；`GL_SAMPLES=8`、`GL_SAMPLE_BUFFERS=1`、`GL_MULTISAMPLE=1`；viewport 为 `1280x720`；sampler `0` 绑定纹理 `4`，与 `galaxyLineTexture` 一致；材质常量、相机位置、screen 参数、线性场景标记和 VP 矩阵均已上传。该捕获在 batch 提交后显式重新绑定同一 program 和纹理读取状态，因为 raylib 提交结束会执行 `glUseProgram(0)`、解绑纹理和 VAO；这不改变已执行 draw 的参数。运行态绑定方向至此关闭。

### `star-line` 纹理

官方纹理状态：

- 尺寸 `256 x 128`
- TextureFormat `10`，即 BC1 / DXT1
- 9 个 mip
- trilinear filter
- clamp wrap
- alpha 全为 `255`，RGB 提供横向 profile
- stream payload SHA-256：`D31145FB70867CB6FAD69E0A10BDE37A9F10DEF6D6302DE39036848FA09A397A`

当前重建文件为 `viewer/galaxy_line_texture.bin`，SHA-256 为 `10FDA98434A6D8C1165D0C914BB9C095AAA5EACE66D39D78F3A350C4C090AD37`。纹理方向、mip 数据、trilinear 和 clamp 已验证。该方向已经关闭，不再通过翻转纹理、固定 LOD 或修改采样模式试验线宽。

### 相机和投影

Galaxy Select 使用 ThirdPersonPoser path ID `42877`。已确认参数：

- FOV：`60`
- near：`0.2`
- far：`10000`
- distance min/max：`80.9000015258789`
- yaw begin：`0`
- pitch begin：`-22`
- pitch min：`-81.19999694824219`
- pitch max：`-4.900000095367432`
- target：`(0,11,0)`

官方运行时使用主相机当前 aspect 构建普通 perspective projection。当前 viewer 使用同样的垂直 FOV、距离、目标和俯仰范围。near/far 的不同只影响裁剪和深度映射，不构成普通可见线段的 XY 宽度比例差异。

viewer 的 Galaxy Select 视图按新增交互要求允许有限缩放，范围为 `45` 至 `160`，单级缩放倍率为 `1.25`。缩放时只改变相机到固定目标 `(0,11,0)` 的距离，因此锚点始终是宇宙中心。该行为是 viewer 扩展，不属于上方固定距离的官方 Galaxy Select 相机结论。

主要证据：

- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-virtual-starmap\camera\camera-findings.json`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-decompile\GameCamera.cs`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-decompile\ThirdPersonPoser.cs`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-decompile\CameraPose.cs`

### 后处理顺序

官方 Galaxy Select 顺序已经确认：

```text
HDR scene -> Bloom / Color Grading Uber -> FXAA and Dithering final pass -> display
```

FXAA 配置：

- method `0`：FXAA
- preset `2`：Default
- subpixel aliasing removal：`0.75`
- edge detection threshold：`0.166`
- minimum required luminance：`0.0833`
- PC quality preset：`39`
- 使用绿色通道作为 luma

当前 `viewer/galaxy_fxaa.fs` 和 `viewer/galaxy_fxaa_web.fs` 已实现该阶段。Debug、Release 和 Emscripten 构建通过，Release 烟雾测试确认 Shader 加载成功。加入 FXAA 后的新截图 `10.png` 与 `9.png` 的线芯宽度基本无变化，因此 FXAA 已排除为当前宽度问题的主因。

官方 Dithering 使用 `LDR_LLL1_0` 至 `LDR_LLL1_63`，共 64 张 `64 x 64 Alpha8` 纹理。它在最终 pass 中添加低幅度颜色扰动，不参与 FXAA luma 分类、边缘搜索、span 选择、subpixel offset 或 ribbon 几何。当前尚未实现，但不再作为线芯几何偏粗的候选原因。

主要证据：

- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-decompile\UnityEngine.PostProcessing\FxaaComponent.cs`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-decompile\UnityEngine.PostProcessing\AntialiasingModel.cs`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-decompile\UnityEngine.PostProcessing\PostProcessingBehaviour.cs`
- `C:\Users\soar\AppData\Local\Temp\opencode\PostProcessing-v1\PostProcessing\Resources\Shaders\FXAA3.cginc`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-fxaa-research\fxaa-blob-10.asm.txt`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-fxaa-research\dithering-assets.json`
- `C:\Users\soar\AppData\Local\Temp\opencode\dsp-galaxy-lut-research\installed-dsp-decompiled\Galaxy Select profile dump.txt`

## 已排除方向

同一参考版本下，除非出现冲突证据，不再重复以下尝试：

- 将官方 width `1` 解释成半宽。原生代码明确再乘 `0.5`，所以资产值是完整宽度。
- 将 viewer 宽度经验缩放到 `1/2` 或 `1/3`。没有官方证据支持。
- 修改 `_TileScale` 以补偿几何宽度。官方值和片元公式均已确认。
- 通过父级 Transform scale 解释宽度。整条对象链均为单位缩放。
- 在托管代码中搜索运行时 `startWidth`、`endWidth` 或 `widthMultiplier` 覆盖。`UIVirtualStarmap` 没有这些写入。
- 认为顶点 Shader 在 GPU 中二次扩线。DXBC 证明其只做普通顶点变换。
- 使用完整三维长度归一化侧向量。Unity 原生实现只归一化相机空间 XY 分量。
- 为两端分别使用不同侧向量。当前两点原生发射路径对两端都读取终点候选。
- 通过纹理翻转、固定 mip / LOD 或修改过滤方式解决线芯宽度。
- 通过 Bloom、Color Grading、FXAA 或 Dithering 解释几何线宽差异。
- 依据 `2.png`、`8.png`、`9.png` 或 `10.png` 直接反推常量。截图尺寸和裁剪不同，只能做定性验收。

已经撤销的无证据实验包括：宽度乘 `1/3`、`_TileScale = 18`、纹理反向、固定 LOD、临时零输出和禁用 MSAA。这些实验不得重新引入。

## 待解决问题

### P0：检查线条实际栅格化目标的采样数

状态：「已确认并实现；已排除为线芯偏粗主因」。

修正前，这是尚未关闭的 rasterization 区域。旧 viewer 只请求 `FLAG_MSAA_4X_HINT`，但 raylib 6.0 明确只把该提示应用到主 framebuffer；当时的 `galaxySceneTarget` 是通过 `rlLoadTexture` 和普通深度 renderbuffer 创建的用户 FBO，没有 multisample attachment。连接线实际绘制到该 FBO，因此旧 viewer 的 line pass 为单采样，窗口的 x4 请求不作用于该 pass。

官方本机配置为 `<MSAA>8</MSAA>`，`GameOption.cs` 把该值赋给 `QualitySettings.antiAliasing`，Main Camera 序列化为 `m_AllowMSAA=true`。Unity 2022.3 文档明确规定 `QualitySettings.antiAliasing` 的值就是每像素样本数，且 `Camera.allowMSAA` 控制相机是否使用该质量级别的 MSAA render target；在当前 Forward 渲染路径下，官方线 pass 因此使用 8 样本。对应文档为 `https://docs.unity3d.com/2022.3/Documentation/ScriptReference/QualitySettings-antiAliasing.html` 和 `https://docs.unity3d.com/2022.3/Documentation/ScriptReference/Camera-allowMSAA.html`。

由此定位 viewer 与官方的直接管线差异：官方连接线在 8 样本相机目标中栅格化，旧 viewer 连接线在单采样 HDR FBO 中栅格化。viewer 现已增加 multisample HDR scene FBO：color attachment 为 8-sample `RGBA16F` renderbuffer，depth attachment 为同样采样数的 `DEPTH_COMPONENT24` renderbuffer。场景完成后通过 `glBlitFramebuffer(..., GL_COLOR_BUFFER_BIT, GL_NEAREST)` resolve 到原有单采样 `RGBA16F` 纹理，再进入 Bloom、Color Grading 和 FXAA。没有调整线宽或 Shader 常量。

实现会先请求 8 样本；若平台不支持该 framebuffer 组合，则依次尝试 4、2、1 样本并写出 warning。当前 Windows / NVIDIA Release 烟雾日志明确记录：

```text
FBO: [ID 1] Multisample HDR target loaded successfully (1280x720 | 8 samples)
```

Debug、Release 和 Emscripten 均已构建成功；Release Native 运行日志没有 `ERROR`、`WARNING` 或 Shader / FBO failure。`12.png` 是启用该目标后的运行结果：与 `11.png` 相比，连接线亮芯没有朝官方 `2.png` 的窄亮芯产生实质变化。因此保留 8 样本实现作为官方管线准确性修正，但不再把 MSAA 视为当前线芯偏粗的主因。

逻辑尺寸与物理 framebuffer 尺寸已排除。运行日志显示 `Display=2560x1440`、`Screen=Render=1280x720`，HDR scene 也是 `1280x720`；当前最终 blit 没有把 1280 宽场景再放大到 2560 宽物理 framebuffer。

### P1：完整最终画面一致性

状态：「非当前线宽问题」。

官方 Dithering 尚未接入。该项只用于最终像素级画面一致性，不作为连接线几何宽度修复的一部分。

## 截图记录

- `C:\Users\soar\Pictures\2.png`：官方参考。
- `C:\Users\soar\Pictures\8.png`：修正 Unity 原生 LineRenderer 几何前的 viewer 结果。
- `C:\Users\soar\Pictures\9.png`：第一轮 LineRenderer 相机空间公式修正后的结果；线芯基本未变化。
- `C:\Users\soar\Pictures\10.png`：加入官方 FXAA 后的结果；线芯仍基本未变化。
- `C:\Users\soar\Pictures\11.png`：改用 Unity 两点 emitter 的终点候选侧向量后；线芯仍无可见变化。
- `C:\Users\soar\Pictures\12.png`：连接线改为 8-sample HDR scene target 后；亮芯仍未朝官方结果产生实质变化。

这些截图证明对应修改没有解决目标问题，但不证明任何具体宽度系数。

### P0 后续状态

MSAA 视觉验证和首条 line batch 运行态捕获完成后，当前证据清单内没有剩余的、已经定位且尚未实现的线芯差异。几何、材质常量、Shader、纹理、相机、后处理、采样数以及 viewer 运行态绑定均已关闭，但结果仍不匹配。此时继续修改宽度、profile、LOD、后处理或 MSAA 参数只会成为截图拟合，不符合本记录的证据规则。

若继续调查，只接受新的直接证据，例如能同时捕获官方和 viewer 对应 line draw 的图形调试记录，或能证明此前资产、原生 emitter、GPU 程序解释存在错误的冲突证据。在获得这类证据前，当前问题应标记为「证据不足，停止参数实验」，而不是继续添加候选修正。

## 调查记录模板

后续新增调查必须按以下格式追加，不能只留在临时目录或会话摘要中：

```text
日期：
参考版本和文件哈希：
问题：
重开既有区域的原因：无 / 具体冲突证据
调查范围：
直接证据：对象 ID、原生地址、反编译符号、Shader 指令或资产字段
结论：
状态：已确认 / 已排除 / 待实现 / 待验证
与既有结论的变化：
代码位置：
验证命令和结果：
剩余问题：
```

临时研究文件位于 `C:\Users\soar\AppData\Local\Temp\opencode`，可能被清理。本文必须保留足以判断「是否已经调查」的结论、版本、哈希、对象 ID、原生地址和关键公式；需要长期复现的原始证据应另行纳入仓库，而不能只依赖临时目录。
