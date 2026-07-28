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
| 顶点输入和 Unity 内建量替代 | 已确认 | program、attribute、uniform、FBO 和采样数与 viewer 的预期值一致 | 否 |
| 连接线纹理绑定 | 已确认并实现 | raylib batch flush 会把纹理槽重置为默认白色纹理，除首条线外所有连接线都采样到 1×1 白纹理 | 是，这是线芯偏粗的主因；同分辨率下仍有 19% 至 31% 残留，见 2026-07-29 第六条 |
| 连接线颜色输入 | 已确认 | `starColors` 渐变八个键、三个特殊类型颜色与资产逐位一致；`Color.Lerp` 在 gamma 空间做、再转线性的顺序也一致 | 否 |
| Galaxy Select 网格 | 已确认并实现 | 官方为五个平面：两层 500×500 外网格、两层 67×67 内网格、一个 67×67 白色边框 | 否，属于独立的画面差异 |
| Galaxy Select 背景星空 | 已确认并实现 | 官方为六个 5000 单位天空盒 Quad，材质 `bgstars-galaxy-select`，每个噪声纹素一颗星的程序化星场 | 否 |
| Galaxy Select 星点 Shader | 已确认 | `Universe/Starmap/Point` 片元程序与 viewer 的 `galaxy_star_point.fs` 逐项等价 | 否 |
| Galaxy Select 相机 | 已确认 | 直接从 `ThirdPersonPoser` 序列化字节复核，距离恒为 `80.9`，俯仰可由玩家在 `[-81.2, -4.9]` 内调整 | 否 |
| HDR scene MSAA | 已确认并实现 | 官方为 8 样本；viewer 现用 8-sample RGBA16F / depth24 FBO 并在 Bloom 前 resolve；`12.png` 证明它未解决线芯偏粗 | 已排除为主因 |
| 片元 Shader 和材质 | 已确认 | profile、距离缩放、裁剪、颜色和混合常量均已恢复 | 否 |
| `star-line` 纹理 | 已确认 | 尺寸、BC1 mip、方向、采样和重建数据已验证 | 否 |
| Galaxy 相机 | 已确认 | FOV、距离、目标、俯仰范围均来自官方对象和运行路径 | 否 |
| Bloom / Color Grading | 已确认并实现 | 属于 HDR 后处理，不改变 ribbon 几何 | 否 |
| FXAA | 已确认并实现 | 官方启用 Default preset 39；加入后 `10.png` 线宽基本不变 | 已排除为主因 |
| Dithering | 已确认但未实现 | 最终低幅度颜色扰动，不参与 FXAA 边缘搜索和几何。幅度为 sRGB `±1/255`，不是官方截图天区里观测到的 `±4` 字节噪声，后者是有损压缩 | 否 |

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
- 用各向异性过滤解释线芯宽度。官方 `QualitySettings.anisotropicTextures = 2`（ForceEnable）确实与 viewer 默认的各向同性采样不同，但在 viewer 上把线纹理设为 16x 各向异性后画面无可测量变化。
- 用纹理 wrap 模式解释线芯宽度。官方为 Clamp、raylib 默认为 Repeat，改为 Clamp 后画面无可测量变化。
- 用渲染分辨率解释线芯宽度。同一相机下把 viewer 从 1280×720 提高到 2560×1440，`FWHM / discard 带宽` 仍为 1.00，未向官方的 0.28 靠拢。

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

## 未完成事项

本节汇总当前所有已定位但尚未完成的工作，按可开工程度排列。每项的完整证据见下方对应的调查记录。

| 项 | 状态 | 证据是否齐备 | 记录位置 |
| --- | --- | --- | --- |
| 连接线同分辨率残留展宽 | 未定位 | 是，1:1 逐像素量测已完成 | 2026-07-29 第六条 |
| 星名标签文案与光谱型后缀 | 未实现 | 是 | 2026-07-29 第五条 |
| Dithering | 未实现 | 部分，缺 64 张 `LDR_LLL1_*` 的提取 | P1 与「后处理顺序」 |
| 选中环鼠标点击的人工验收 | 已实现并截图验收 | 不适用，仅缺人工点击确认 | 2026-07-29 第六条 |
| Emscripten 构建验证 | 未执行 | 不适用，本机缺 emsdk | 2026-07-29 第三条 |
| 两个同姿态 67×67 Quad 的用途 | 未确认 | 否 | 2026-07-29 第二条 |
| 无损官方截图 | 未获取 | 现有官方截图均带 4:2:0 有损压缩，限制测光精度 | 2026-07-29 第六条 |
| 行星主题材质 | 部分实现 | 否，未移植完整 `Planet Thumb New` 与黑色 `_HeightMap` 路径 | 「星系内视图」 |
| 星系内视图后处理与行星几何 | 已实现待验收 | 不适用，缺截图验收 | 「星系内视图」 |
| 星系内视图相机操作 | 已实现待验收 | 不适用，缺交互验收 | 「星系内视图」 |

`starPointSelection` 与 `starPointBirth` 已于 2026-07-29 第五条完成调查与实现。

以下三项已于 2026-07-29 第六条关闭，不再是待办：银河视图背景星点亮度（等条件下匹配，原「3 倍」为测量方法造成）、银河区域偏红底光来源（viewer 清屏色红色通道被 8 位量化抹掉，已修复）、连接线逐像素亮度比对（已在 1080p 对 1080p 下执行）。

## 截图记录

- `C:\Users\soar\Pictures\2.png`：官方参考。
- `C:\Users\soar\Pictures\8.png`：修正 Unity 原生 LineRenderer 几何前的 viewer 结果。
- `C:\Users\soar\Pictures\9.png`：第一轮 LineRenderer 相机空间公式修正后的结果；线芯基本未变化。
- `C:\Users\soar\Pictures\10.png`：加入官方 FXAA 后的结果；线芯仍基本未变化。
- `C:\Users\soar\Pictures\11.png`：改用 Unity 两点 emitter 的终点候选侧向量后；线芯仍无可见变化。
- `C:\Users\soar\Pictures\12.png`：连接线改为 8-sample HDR scene target 后；亮芯仍未朝官方结果产生实质变化。

这些截图证明对应修改没有解决目标问题，但不证明任何具体宽度系数。

### P0 后续状态

状态：「已关闭」。原因见下方 2026-07-29 的调查记录：连接线线芯偏粗与几何、材质常量、Shader、纹理数据、相机、后处理和采样数都无关，唯一原因是 viewer 侧的纹理绑定在 raylib batch flush 后失效。

此前把该问题标记为「证据不足」的判断建立在一条循环证据上：那次运行态捕获是在 batch 提交后由捕获代码自己重新绑定 program 和纹理再读取的，因此它反映的是重绑定后的状态，不是 draw 实际使用的状态。后续做绑定类验证时，必须在 draw 之前读取状态，或用 Shader 侧的可观测输出反推，不能在提交后重新绑定再读。

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

## 调查记录

### 2026-07-29：连接线线芯偏粗

日期：2026-07-29

参考版本和文件哈希：与「参考版本」一节相同，本次未发生变化。`Assembly-CSharp.dll` SHA-256 `AE0BA95F75BD879A62AA4CE253B2AB78EAA4FB3C7C595F5E1FEE75EBE0E0EF85`、MVID `ece4a40e-5e73-43f4-a9f8-4e74970b5942`；`sharedassets0.assets` 中 Shader `1402`、Material `115`、Texture `531` 与既有记录一致。

问题：Galaxy Select 连接线线芯明显比官方粗，且呈平顶硬边，而官方为单峰软边。

重开既有区域的原因：有具体冲突证据。「顶点输入和 Unity 内建量替代」一节的运行态捕获是在 batch 提交后由捕获代码重新绑定 program 和纹理再读取的，因此它证明的是重绑定后的状态，不能证明 draw 实际使用了 `galaxyLineTexture`。该结论属于循环证据，必须重开。

调查范围：官方 Shader 字节码与材质常量的独立复核、`star-line` 纹理导入设置与色彩空间、Galaxy Select 场景层级中的世界尺度基准、viewer 侧的采样器状态与纹理绑定。

直接证据：

- Shader path ID `1402`（`Universe/Starmap/Line`），platform `4`（D3D11），LZ4 解压后含两个 DXBC。片元程序 `ps_4_0` 逐条复核：`add r0.y, v1.y, l(-0.5)` → `div r0.x, |r0.y|, r0.x` → `dp2 r0.y, r0.xxxx, cb0[2].yyyy` → `discard_nz`（`profileCoord > 1`）→ `mov r0.x, v1.x` → `sample r0.xyzw, r0.xyxx, t0.xyzw, s0`。`viewer/shaders/desktop/galaxy_line.fs` 与之逐项等价，profile 公式、采样坐标 `(uv.x, profileCoord)`、颜色 lerp、`_Multiplier` 和 edge fade 均无差异。
- Material `115` 常量与既有记录一致；Shader 属性名显示 `_TileScale` 的显示名为 `Width Scale`。
- Texture `531`：`256 x 128`、TextureFormat `10`（BC1）、`m_MipCount = 9`、`m_ColorSpace = 1`、`m_FilterMode = 2`（Trilinear）、`m_Aniso = 1`、`m_MipBias = 0`、`m_WrapU/V = 1`（Clamp）。
- `globalgamemanagers`：PlayerSettings `m_ActiveColorSpace = 1`（Linear），因此 `viewer/tools/galaxy_line_texture.py` 的 sRGB 转线性重建方向正确；QualitySettings `antiAliasing = 2`、`anisotropicTextures = 2`（ForceEnable）。
- `level0` 中 `Galaxy Select Starmap`（GO `5120`）层级：`grid-0`/`grid-1` 为 `500 x 500` 平面（y `-15`，Material `110`/`111`，`_Tiling` `0.2`/`0.4`），`grid-2` 为 `67 x 67` 平面（y `-10`，Material `112`），`star-point`（GO `5344`，Material `117`）与三个 grid 共用 Unity 内建 Quad 网格（`m_FileID 3`，`m_PathID 10210`，`1 x 1`）。因此星点四边形的世界尺寸就是 `UIVirtualStarmap` 写入的 `localScale`，主序星为 `1.2`。
- raylib 6.0 `rlgl.h` 的 `rlDrawRenderBatch(...)`：重置段落对 `i` 从 `0` 开始，把 `batch->draws[i].textureId` 全部写成 `RLGL.State.defaultTextureId`，随后 `batch->drawCounter = 1`。
- GPU 侧判定实验：把 viewer 片元采样临时改为 `textureLod(texture0, vec2(fragTexCoord.x, 0.90), 0.0)`。若绑定的是 `star-line`，该坐标返回 `0`，连接线应当消失。实测输出与原实现逐字节相同；改为固定 `V = 0.30` 或强制 LOD `0`、LOD bias `-6` 同样逐字节相同。说明采样结果与坐标和 LOD 无关，即绑定的是 1×1 常量白色纹理。

结论：`DrawGalaxyLines(...)` 只在循环外调用一次 `rlSetTexture(...)`，而循环内每条线之后都调用 `rlDrawRenderBatchActive()` 以隔离逐线的 `_LineColorA` / `_LineColorB`。batch flush 会把所有 draw 槽的纹理重置为 raylib 默认白色纹理，因此除第一条线外，所有连接线都在采样白色纹理。`star-line` 的横向 profile 完全没有参与渲染，片元只剩 `profileCoord > 1` 的 discard 边界，线被渲染成整条 discard 带均匀点亮的平顶实心带。这就是线芯偏粗、边缘发硬的唯一原因。

量化结果（seed `1`、64 星、`galaxyDistance = 80.9`、pitch `-22`、`2560 x 1440`、同一条连接线、线性光度量）：

```text
理论 discard 带宽            5.67 px
修正前 FWHM 5.69 px          FWHM / 带宽 = 1.00   平顶
修正后 FWHM 1.81 px          FWHM / 带宽 = 0.32   单峰
官方 2.png FWHM 1.25-1.38 px W25 2.00-2.12 px     单峰
```

状态：已确认并实现。

与既有结论的变化：撤销「viewer 运行态绑定方向至此关闭」。该结论来自提交后重新绑定再读取的捕获，属于循环证据。其余已确认结论（几何、宽度公式、侧向量、UV 与索引、材质常量、片元公式、纹理数据、相机、后处理、采样数）本次全部复核通过，不变。

代码位置：`viewer/src/scene_renderer.cc` 的 `DrawGalaxyLines(...)`，把 `rlSetTexture(resources.galaxyLineTexture.id)` 移入逐线循环。

验证命令和结果：

```powershell
cmake --build cmake-build-viewer-debug --target viewer
cmake-build-viewer-debug\bin\viewer\viewer.exe 1 64
```

Debug 构建通过；运行日志无 `ERROR` 与 `WARNING`，`TEXTURE: [ID 4] Texture loaded successfully (256x128 | R32G32B32A32 | 9 mipmaps)`，line Shader 加载成功。同一受控截图的横向剖面由平顶 `FWHM / 带宽 = 1.00` 变为单峰 `0.32`，与官方的单峰形状一致。

剩余问题：修正后线芯峰值线性亮度约为修正前的一半，与官方总体一致，但尚未在同一 seed、同一星数、同一相机和同一分辨率下与官方做逐像素亮度比对。Dithering 仍未实现。

### 2026-07-29：Galaxy Select 网格、星点 Shader 与相机复核

日期：2026-07-29

参考版本和文件哈希：与「参考版本」一节相同，本次未发生变化。

问题：连接线修正后，画面中最明显的剩余差异是网格——官方为极淡的蓝色细网格加一圈白色边框，viewer 为一层明显偏亮的白色网格。

重开既有区域的原因：无。相机属于既有结论，本次是首次直接从资产字节复核，不是推翻性重开；网格属于「未调查」。

调查范围：`Galaxy Select Starmap` 层级下全部网格渲染对象及其材质、Shader、贴图；星点 Shader；相机 poser 的序列化字段。

#### 常量缓冲区映射方法

Unity 会剥离 DXBC 的 RDEF 反射块，因此不能直接从字节码得到常量名。但 `Shader.m_ParsedForm.m_SubShaders[].m_Passes[].m_NameIndices` 给出名称到索引的映射，`progFragment.m_CommonParameters.m_ConstantBuffers[].m_VectorParams[]` 给出每个成员的 `m_NameIndex` 和字节偏移。两者合并即可得到确定的 `cb0[i].c` 到属性名的对应，不需要靠语义猜测。后续做 Shader 复刻时应优先用这条路径。

用该方法复核 `Universe/Starmap/Line`（path ID `1402`）的结果与既有记录一致：`cb0[2]` 为 `_Multiplier`、`_TileScale`、`_DistScale`、`_DistPower`，`cb0[3].xy` 为 `_MinDistScale`、`_MaxDistScale`，`cb0[4]`、`cb0[5]` 为两个线端颜色，`cb0[6].x` 为 `_Global_GalaxyFadeRadius`。同时从 pass state 复核到 `srcBlend = 5`、`destBlend = 1`，即 `Blend SrcAlpha One`，与既有记录一致。

#### 相机

直接解析 `level0` 中 MonoBehaviour `42877`（`ThirdPersonPoser`）的 192 字节序列化数据：

```text
fov 60   near 0.2   far 10000
pitchMin -81.1999969   pitchMax -4.9000001
distMin 80.9000015     distMax 80.9000015
pitchDistCurve  两个常量键，值均为 0.5
yawBegin 0   pitchBegin -22
useGlobal false   lockCursor false   distRatio 1
```

因此 `dist = Lerp(distMin, distMax, 0.5) * distRatio = 80.9`，与俯仰无关。`weight` 是 `[NonSerialized]`、默认 `1`，所以玩家可以在 `[-81.2, -4.9]` 内改变俯仰。用 67×67 边框在 `2.png` 中的两条侧边斜率（左边 `x = -0.19996y + 144.976`，拟合残差 `0.08` px）反解，该截图的俯仰约为 `-57.7`，不是默认的 `-22`。**因此 `2.png` 不能当作默认相机姿态的参考截图。**

#### 网格对象

`Galaxy Select Starmap` 下共有五个网格渲染对象，全部使用 Unity 内建 Quad（`m_FileID 3`，`m_PathID 10210`）：

| GameObject | Transform | 平面 y | 世界尺寸 | Material | Shader | 贴图 | `_Tiling` | 线间距 | `_TintColor` | `_AlphaMultiplier` | `_NearFade` | `_AreaMin` / `_AreaMax` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `grid-0` `2562` | `14519` | −15 | 500×500 | `110` | `1373` | `602` `grid-256-dot` | 0.2 | 5 | (0.5519, 0.6917, 1) | 0.03 | 35 | 60 / 250 |
| `grid-1` `520` | `14362` | −15 | 500×500 | `111` | `1373` | `666` `grid-256` | 0.4 | 2.5 | (1, 1, 1) | 0.03 | 35 | 60 / 250 |
| `grid-2` `1086` | `14410` | −10 | 67×67 | `113` | `1373` | `602` | 0.2 | 5 | (0.5708, 0.6625, 1) | 0.10 | −10 | 50 / 250 |
| `grid-2` `2798` | `14532` | −10 | 67×67 | `114` | `1373` | `666` | 0.4 | 2.5 | (0.6368, 0.739, 1) | 0.05 | −10 | 50 / 250 |
| `grid-2` `252` | `14348` | −10 | 67×67 | `112` | `1439` | `823` `galaxy-select-frame` | — | — | (1, 1, 1) | 1.0 | — | — |

外层两个 500×500 平面的 `_NearFade = 35` 会在 `max(|x|, |z|) <= 35` 的方形区域内完全隐藏网格；内层两个 67×67 平面的 `_NearFade = -10` 使该项恒为 1。两者正好互补：内区由 67×67 平面绘制且更亮，外区由 500×500 平面绘制且更淡，交界处与白色边框重合。这解释了 `2.png` 中边框内网格明显比边框外强（同一扫描行上高通标准差约 `3.1` 对 `0.65`）。

#### 网格 Shader

`VF Shaders/Gizmos/Galaxy Select Grid`（path ID `1373`）。渲染状态：`Blend SrcAlpha OneMinusSrcAlpha`、`ZTest Off`、`ZWrite Off`、`Cull Off`、`ColorMask RGB`、queue `Transparent`。顶点程序把世界坐标的 X 与 Z 通过 `TEXCOORD2.zw` 传给片元。片元逻辑为：

```text
r        = length(worldXZ)
areaFade = saturate( saturate((_AreaMax - r) / (_AreaMax - _AreaMin)) - _AreaMin / (_AreaMax - _AreaMin) )
nearFade = saturate( (max(|worldX|, |worldZ|) - _NearFade) * 0.2 )
uv       = worldXZ * _Tiling
c        = tex2D(_MainTex, uv) * _TintColor
c.rgb   *= _Multiplier
c.a      = saturate(c.a * _AlphaMultiplier)
c       *= vertexColor
out.rgb  = c.rgb
out.a    = c.a * areaFade * nearFade
```

注意 `areaFade` 的上限不是 1：外层为 `1 - 60/190 = 0.6842`，内层为 `1 - 50/200 = 0.75`。67×67 平面的最大半径为 `47.4`，小于其 `_AreaMin = 50`，因此内层网格的 `areaFade` 恒为 `0.75`。

`_MainTex` 的 wrap 为 Repeat，`grid-256`（`666`）与 `grid-256-dot`（`602`）都是 256×256 BC7、9 mip、sRGB、Trilinear、`m_Aniso = 5`；RGB 全为白，alpha 只在第 0 行、第 0 列、第 255 行、第 255 列为 255，其余为 0。`grid-256-dot` 另在四角 `1..4` / `251..254` 范围内多出 52 个不透明像素，即交叉点上的加粗点。`galaxy-select-frame`（`823`）为 1024×1024 RGBA32、11 mip，仅在第 2、3 与 1020、1021 行列为白色，内部 alpha 全 0，即一圈两像素宽的方框。

因此单条内层 5 单位网格线的输出 alpha 为 `1 * 0.10 * 0.75 = 0.075`，2.5 单位层为 `0.0375`；外层两层在 `areaFade` 取最大值时为 `0.0205`。

#### 与 viewer 的差异

viewer 当前在 `viewer/src/viewer_app.cc` 里构造 `galaxyGridLines`：8 等分、间距 `10`、范围 `±40`、高度 `-15`，在 `viewer/src/scene_renderer.cc` 的 `DrawGalaxy(...)` 中用 `DrawLine3D` 以 `RGB(105, 153, 181)`、alpha `clamp((camDist - 22) / 34, 0, 1) * 0.3` 绘制。差异为：

- 只有一层，官方有四层网格加一个边框；
- 线间距 `10`，官方为 `5` 与 `2.5` 两种；
- 范围 `±40`，官方内层 `±33.5`、外层 `±250`；
- 用 1 像素 `RL_LINES` 而不是带贴图的平面，因此没有 mip 过滤、没有交叉点加粗、也没有随距离变细；
- alpha `0.3` 远高于官方的 `0.0205` 至 `0.075`，这是网格显得过亮的直接原因；
- 没有 `areaFade` 半径衰减、没有 `nearFade` 内外分区、没有白色边框。

#### 星点

`Universe/Starmap/Point`（path ID `1384`）：`Blend SrcAlpha OneMinusSrcAlpha`、`ZTest Off`、`ZWrite Off`、`Cull Off`、`ColorMask RGB`。片元为 `fade = smoothstep01 * 0.9 + 0.1`，`c = tex * _TintColor`，`c.rgb *= _Multiplier`，`c.a = saturate(c.a * _AlphaMultiplier)`，`c *= vertexColor`，`out = fade * c`。与 `viewer/shaders/desktop/galaxy_star_point.fs` 逐项等价，`DrawGalaxyStarPoint(...)` 的混合因子与 `ColorMask RGB` 也一致。星点网格为内建 Quad，`UIVirtualStarmap` 写入的 `localScale` 即世界尺寸（主序星 `1.2`、巨星 `3`、白矮星与中子星 `0.6`、黑洞 `0.8`），与 viewer 的 `GalaxyStarScale(...)` 一致。贴图 `950` `galaxy-select-starpoint` 为 64×64 RGBA32、**单 mip**、wrap Repeat。

结论：连接线修正后剩余的主要画面差异在网格，且已定位到可直接实现的完整参数集。星点与相机复核通过。

状态：网格为「已确认但未实现」；星点与相机为「已确认」。

与既有结论的变化：新增网格与星点区域。相机由「来自既有记录」升级为「已从资产字节直接复核」，并新增一条重要限定：`2.png` 的俯仰约为 `-57.7`，不是默认值，做几何比对时必须先还原姿态。

代码位置：待实现部分位于 `viewer/src/viewer_app.cc` 的网格构造与 `viewer/src/scene_renderer.cc` 的 `DrawGalaxy(...)`。

验证命令和结果：本轮为只读调查，未改动代码。度量基于 `2.png` 与受控截图，方法与上一条记录相同。

剩余问题：

- 网格尚未实现，需要把 `602`、`666`、`823` 三张贴图纳入 `viewer/assets` 并新增一个网格 Shader。
- 背景星空未调查。官方在 `grids` 组下有六个 5000 单位的天空盒 Quad（Transform `14461`、`14549`、`14701`、`14563`、`14415`、`14432`），viewer 的 `DrawGalaxy(...)` 以 `bloomVisible = false` 调用 `DrawBackgroundStars(...)`，即银河视图完全不画背景星。
- 三个 67×67 Quad 中有两个的旋转相同，均把面法线转到 −Y；网格 Shader 由世界坐标取 UV，因此面内旋转不影响图案，但尚未确认为何需要两个同姿态实例。

### 2026-07-29：连接线颜色输入复核、网格实现、背景星空定位

日期：2026-07-29

参考版本和文件哈希：与「参考版本」一节相同，本次未发生变化。

问题：连接线宽度修正后，闭合连接线剩余的一致性问题；顺带处理已定位的网格差异。

重开既有区域的原因：无。

#### 连接线颜色输入

直接解析 `level0` 中 MonoBehaviour `49504`（`UIVirtualStarmap`）的 364 字节序列化数据，取出 `starColors` 渐变与三个特殊类型颜色：

```text
key0 t=0            rgb=(1.000000000, 0.229039833, 0.174528301)
key1 t=0.06999313   rgb=(1.000000000, 0.437254906, 0.176470578)
key2 t=0.31999695   rgb=(1.000000000, 0.951527596, 0.155660391)
key3 t=0.50000763   rgb=(0.987870634, 1.000000000, 0.617924571)
key4 t=0.72353704   rgb=(0.976415098, 1.000000000, 0.977961540)
key5 t=0.82000458   rgb=(0.580000043, 0.896296144, 1.000000000)
key6 t=0.91999695   rgb=(0.117647052, 0.654785156, 1.000000000)
key7 t=1            rgb=(0.099056602, 0.340047508, 1.000000000)
neutronStarColor (0.502143919, 0.382075489, 1.000000000)
whiteDwarfColor  (0.462264180, 0.462264180, 0.462264180)
blackholeColor   (0, 0, 0)
```

与 `viewer/src/material_catalog.cc` 的 `GalaxyStarColor(...)` 表逐位一致，渐变键时间也与 UInt16 原值吻合（例如 `4587 / 65535 = 0.06999313`）。`GalaxyLineColor(...)` 先在 gamma 空间做 `Lerp(starColor, white, 0.65)` 再转线性，与 Unity 对 `Color` 属性「先在 sRGB 数值上插值、上传时转线性」的行为一致。

至此连接线的全部输入都已逐项对齐：几何与端点裁剪、颜色与色彩空间、材质常量、片元公式、混合状态、纹理数据与 mip、纹理绑定。**连接线宽度问题闭合。**

#### 网格实现

按上一条记录的参数实现，代码位于 `viewer/src/scene_renderer.cc` 的 `DrawGalaxyGrid(...)`、`DrawGalaxyGridPlane(...)` 与 `DrawGalaxyFrame(...)`，Shader 为 `viewer/shaders/{desktop,web}/galaxy_grid.*` 和 `galaxy_frame.*`。

三张贴图的 RGB 全为 255、图案只在 alpha 上，mip 链经检验就是 alpha 的 box 滤波（BC7 两张的最大偏差 `7.5/255`，属压缩噪声；RGBA32 那张最大偏差 `0.5`）。因此 viewer 只保存 alpha 通道，格式为紧凑排列的 8 位 mip 链：

| 文件 | 来源 path ID | 尺寸 | mip | 字节 |
| --- | --- | --- | --- | --- |
| `viewer/assets/textures/galaxy_grid_dot.bin` | 602 | 256×256 | 9 | 87381 |
| `viewer/assets/textures/galaxy_grid_line.bin` | 666 | 256×256 | 9 | 87381 |
| `viewer/assets/textures/galaxy_grid_frame.bin` | 823 | 1024×1024 | 11 | 1398101 |

提取与校验工具为 `viewer/tools/galaxy_grid_textures.py`，三个文件的 SHA-256 已登记进 `viewer/assets/manifest.json`。

已知的有意偏差：官方边框材质为 `Cull Back`，viewer 用 `Cull Off`。Galaxy Select 相机俯仰被限制在 `[-81.2, -4.9]`，始终位于平面上方，因此该差异不可见。

绘制顺序按官方渲染队列排列：边框 `2499`、连接线 `2999`、网格平面与星点 `3000`，其中网格平面在星点之前。

验证：seed `1`、64 星、默认相机、`2560 x 1440` 截图，与官方 `2.png` 在各自边框内取无恒星区域做水平高通，`p99` 分别为 viewer `22.05`、官方 `23.38`，量级一致。修正前 viewer 用的是 alpha `0.3` 的不透明白线，官方单条内层网格线的输出 alpha 只有 `0.075`。

#### 背景星空

官方在 `grids` 组下有六个 Quad 组成天空盒，Transform `14461`、`14549`、`14701`、`14563`、`14415`、`14432`，位置为各轴 `±2500`、缩放 `5000`，全部使用 Material `102` `bgstars-galaxy-select`，Shader path ID `1391` `Universe/Star/Star Mono-Layer`。

- 渲染状态：`Blend SrcAlpha OneMinusSrcAlpha`、`ZTest LEqual`、`ZWrite Off`、`Cull Off`、queue `Background`。
- 贴图：`_NoiseMap` = path ID `605` `random-colors-128-2`，128×128 RGBA32，单 mip，65536 字节。
- 片元程序使用的常量（其余 `BG` 与 `Shine` 系列属性未被 Mono-Layer 变体使用）：

```text
_NoiseMap_TexelSize  cb0[2].xy
_StarSetting         cb0[3]  = (0.2, 0.2, 1.1, 0.4)
_RefValueA           cb0[4]  = (0.2745098, 0.4901961, 0.3607843, 1)
_RefValueB           cb0[5]  = (0.6039216, 0.2705882, 0.3174, 1)
_StarColor           cb0[6]  = (0.6557, 0.7479, 1, 1)
_StarColorChange     cb0[7]  = (0.5, 0.7, 0.3, 0)
_Time                cb1[0]
```

片元逻辑为按 `_NoiseMap_TexelSize` 划分的格点星场：用 `round_z` 取格点、以噪声值抖动星心、按到星心的距离生成核，`_StarSetting.x/.y` 控制星半径范围、`.z` 为强度、`.w` 为颜色带宽；用 `sin(_Time.y * 1.5)` 在 `_RefValueA` 与 `_RefValueB` 之间做闪烁；两处 `discard` 阈值均为 `0.01`。共约 42 条指令。

viewer 目前在 `DrawGalaxy(...)` 中以 `bloomVisible = false` 调用 `DrawBackgroundStars(...)`，即银河视图完全不画背景星，且现有实现是程序化点阵而非官方星场。

状态：连接线颜色输入为「已确认」；网格为「已确认并实现」；背景星空为「已确认但未实现」。

与既有结论的变化：连接线区域全部关闭。网格由「已确认但未实现」转为「已确认并实现」。新增背景星空区域。

代码位置：`viewer/src/scene_renderer.cc`、`viewer/src/app_resources.cc`、`viewer/src/render_resources.{hh,cc}`、`viewer/src/material_catalog.{hh,cc}`、`viewer/src/viewer_app.cc`、`viewer/shaders/{desktop,web}/galaxy_grid.*`、`viewer/shaders/{desktop,web}/galaxy_frame.*`、`viewer/tools/galaxy_grid_textures.py`。

验证命令和结果：

```powershell
python viewer\tools\verify_assets.py          # verified 15 viewer assets
python viewer\tools\verify_shader_pairs.py    # verified 23 desktop/web shader pairs
python viewer\tools\galaxy_grid_textures.py   # 三个文件 SHA-256 与登记值一致
cmake --build cmake-build-viewer-debug --target viewer
cmake-build-viewer-debug\bin\viewer\viewer.exe 1 64
```

Debug 构建通过、无编译警告；运行日志无 `ERROR` 与 `WARNING`，网格 Shader 与三张贴图均加载成功。

剩余问题：

- 背景星空未实现，参数已完整记录，可直接开工。
- 未做 Emscripten 构建验证。新增的 web Shader 已通过 `verify_shader_pairs.py` 配对检查，但本机未配置 emsdk。
- 官方画面在银河区域有一层偏红的底光（同一取样区内官方背景 RGB 约 `(7.4, 20.2, 26.6)`，viewer 约 `(1.4, 19.5, 27.5)`），来源尚未定位，可能来自背景星空或未实现的 Dithering。
- Dithering 仍未实现。

### 2026-07-29：银河视图背景星空

日期：2026-07-29

参考版本和文件哈希：与「参考版本」一节相同，本次未发生变化。

问题：viewer 银河视图完全不画背景星，官方有一层程序化星场。

重开既有区域的原因：无，属于上一条记录中登记的未实现项。

调查范围：`Galaxy Select Starmap/grids` 下六个天空盒 Quad 的几何、材质、Shader 与噪声贴图。

#### 几何

六个 Quad 组成半边长 `2500` 的立方体，全部使用 Unity 内建 Quad（`m_FileID 3`，`m_PathID 10210`），缩放 `5000`，法线朝外，材质 `Cull Off` 因此从内部可见：

| Transform | 位置 | 四元数 `(x,y,z,w)` |
| --- | --- | --- |
| `14461` | `(0, 0, 2500)` | `(0, 0, 0, 1)` |
| `14549` | `(-2500, 0, 0)` | `(0, -0.7071068, 0, 0.7071068)` |
| `14701` | `(2500, 0, 0)` | `(0, 0.7071068, 0, 0.7071068)` |
| `14563` | `(0, 0, -2500)` | `(0, 1, 0, 0)` |
| `14415` | `(0, -2500, 0)` | `(0.7071068, 0, 0, 0.7071068)` |
| `14432` | `(0, 2500, 0)` | `(-0.7071068, 0, 0, 0.7071068)` |

#### 材质与 Shader

Material `102` `bgstars-galaxy-select`，Shader path ID `1391` `Universe/Star/Star Mono-Layer`。渲染状态为 `Blend SrcAlpha OneMinusSrcAlpha`、`ZTest LEqual`、`ZWrite Off`、`Cull Off`、queue `Background`。

属性声明类型决定了色彩空间处理，这一点必须按声明区分：

| 属性 | 声明类型 | 序列化值 | 上传前是否转线性 |
| --- | --- | --- | --- |
| `_StarSetting` | Vector | `(0.2, 0.2, 1.1, 0.4)` | 否 |
| `_RefValueA` | Color | `(0.274509804, 0.490196078, 0.360784314)` | 是 |
| `_RefValueB` | Color | `(0.603921569, 0.270588235, 0.317400008)` | 是 |
| `_StarColor` | Color | `(0.6557, 0.7479, 1)` | 是 |
| `_StarColorChange` | Vector | `(0.5, 0.7, 0.3)` | 否 |

`_StarSetting` 的官方描述为 `Star Setting (Size Min, Size Max, Intensity, Density)`；`_RefValueA` 与 `_RefValueB` 的描述为 `Reference Value ... (not star color)`，虽然语义不是颜色，但因为声明为 `Color`，Unity 在线性色彩空间下仍会转换。

片元逻辑（`_NoiseMapTexelSize` 为 `(1/128, 1/128)`）：

```text
scaledY  = uv.y / texel.y ; scaledX = uv.x / texel.x
cellY    = trunc(scaledY) * texel.y ; cellX = trunc(scaledX) * texel.x
noise    = tex2D(_NoiseMap, uv)                       // 点采样
jitter   = frac(float2(cellY, cellX) * 37 + noise.xy) // 官方在此处交叉了两个轴
local    = frac(float2(scaledX, scaledY))
radius   = lerp(_StarSetting.x, _StarSetting.y, noise.a)
center   = jitter * (1 - 2 * radius) + radius
core     = saturate((radius - length(local - center)) / radius) * _StarSetting.z
core     = core^4 ;  discard if core^2 < 0.01
phase    = sin(_Time.y * 1.5) * 0.5 + 0.5
ref      = lerp(_RefValueA.rgb, _RefValueB.rgb, phase)
dist     = |noise.r - ref.r| + |noise.g - ref.g| + |noise.b - ref.b|
band     = saturate((_StarSetting.w - dist * 0.5) / _StarSetting.w) * _StarSetting.z
discard if band^2 < 0.01
out.rgb  = lerp(_StarColor.rgb, noise.rgb, _StarColorChange.rgb)
out.a    = band^2 * core^2
```

即每个噪声纹素一颗星，星的存在与亮度由该纹素的噪声颜色与一个随时间在 `_RefValueA` 和 `_RefValueB` 之间摆动的参考色的曼哈顿距离决定，这就是闪烁来源。

#### 贴图

`_NoiseMap` = path ID `605` `random-colors-128-2`，128×128 RGBA32、单 mip、**线性色彩空间**、**Point 过滤**、Repeat。Point 过滤是必需的：一个纹素就是一个星格，双线性会让相邻格互相渗透。

viewer 侧原样保存为 `viewer/assets/textures/galaxy_bgstars_noise.bin`（65536 字节 RGBA8），工具为 `viewer/tools/galaxy_bgstars_texture.py`，SHA-256 已登记进 manifest。

#### 裁剪面

天空盒半径 `2500`、对角 `4330`，超过 raylib 默认的远裁剪 `4000`。banner 视图现按官方相机设置 `rlSetClipPlanes(0.2, 10000)`，星系内视图仍用 raylib 默认值。这同时消除了既有记录中「near/far 与官方不同」的差异。

#### 验证

seed `1`、64 星、默认相机、`2560 x 1440`。与官方 `2.png` 的无星系天区做小亮斑统计：

```text
官方 clean sky     密度  6.70 /10k px   峰值中位 19.8   斑块面积中位 3.5 px
官方 left strip    密度 10.28 /10k px   峰值中位 12.7   斑块面积中位 2.0 px
viewer             密度  5.73 /10k px   峰值中位 58.7   斑块面积中位 6.0 px
```

密度落在官方两个取样区之间，量级一致。峰值亮度比官方高约 3 倍，原因未定位。

**本段结论已于 2026-07-29 第六条撤回，勿据此开工。**该比对使用的官方截图带 4:2:0 有损压缩，俯仰与像素尺度也与 viewer 不同；在 1080p 对 1080p 的等条件量测下峰值比值为 0.92 至 1.04，不存在亮度偏高。

为排除色彩空间假设，做了一次 A/B：把 `_RefValueA` 与 `_RefValueB` 改成不转线性后，密度升到 `10.91 /10k px`、峰值中位降到 `46.7`。两种取法的峰值都远高于官方，因此**颜色空间假设不是亮度偏高的原因**。当前实现保留 Unity 对 `Color` 类型属性的既定规则，即转线性。

状态：已确认并实现；亮度偏高为待解决项。

与既有结论的变化：新增背景星空区域并实现。银河视图裁剪面改为官方值。

代码位置：`viewer/src/scene_renderer.cc` 的 `DrawGalaxyBackgroundStars(...)`、`viewer/shaders/{desktop,web}/galaxy_bgstars.*`、`viewer/src/viewer_app.cc` 的裁剪面设置。

验证命令和结果：

```powershell
python viewer\tools\verify_assets.py          # verified 16 viewer assets
python viewer\tools\verify_shader_pairs.py    # verified 25 desktop/web shader pairs
python viewer\tools\galaxy_bgstars_texture.py # SHA-256 与登记值一致
cmake --build cmake-build-viewer-debug --target viewer
cmake-build-viewer-debug\bin\viewer\viewer.exe 1 64
```

Debug 构建通过、无编译警告；运行日志无 `ERROR` 与 `WARNING`，噪声贴图以 `128x128 | R8G8B8A8` 加载成功。

剩余问题：

- ~~背景星点峰值亮度约为官方的 3 倍，斑块也更大。~~ 已于 2026-07-29 第六条关闭：等条件下峰值、颜色与密度均一致，`core⁸` 幂次链经 DXBC 复核无误。
- 官方 `2.png` 的俯仰约为 `-57.7`，与 viewer 默认的 `-22` 不同，上面的天区统计因此只能做量级比较。

### 2026-07-29：同 seed A/B 复核与星图标记

日期：2026-07-29

参考版本和文件哈希：与「参考版本」一节相同，本次未发生变化。

问题：用同一 seed、同一机位的官方截图 `13.png` 与 viewer 截图 `14.png` 做等条件比对，确认连接线是否还有残留差异，并补齐缺失的星图标记。

重开既有区域的原因：无。这是首次拿到可配准的同 seed A/B。

#### 配准

两张图用星点质心做尺度加平移配准，23 个内点、残差小于 3 px，尺度为 `1.3110`（viewer 像素 → 官方像素）。能用纯相似变换配上说明两张图的相机距离与俯仰相同，只是渲染分辨率不同。viewer 侧为 `1280 x 720`。

**更正（2026-07-29 第六条）**：`1.3110` 不能反推官方分辨率。`14.png` 到同机位 `1678 x 944` viewer 截图的尺度为 `1.1420`，而真实 720p 源必须为 `1.3111`，因此 `14.png` 被缩放过约 `1.148` 倍。官方截图的真实分辨率为 `1920 x 1080`。

#### 等尺度比对结果

| 项 | viewer / 官方 |
| --- | --- |
| 星点核心大小 `r25` | 1.01 |
| 星点光晕，`r = 8` 至 `44` px | 1.03 至 1.08 |
| 连接线 FWHM | 1.60 |
| 连接线峰值 | 0.56 |
| 连接线 FWHM × 峰值 | 0.94 |

星点大小与 Bloom 光晕已经匹配。连接线宽度乘峰值接近 1，说明总能量正确，差异是「被抹开」而不是几何变宽。

#### 连接线的残留差异来自渲染分辨率

profile 纹理 V 方向只有 128 texel，低分辨率下每像素的纹理足迹会跨到 mip 6（V 方向只剩两个 texel），双线性重建把 profile 拍平。实测 viewer 在三个分辨率下的 `FWHM / 理论带宽`：

```text
1280 x 720    带宽 2.82 px   FWHM 2.75 px   比值 0.977
1920 x 1080   带宽 4.24 px   FWHM 4.00 px   比值 0.942
2560 x 1440   带宽 5.64 px   FWHM 1.94 px   比值 0.344
```

1080p 与 1440p 之间存在悬崖。官方 `13.png` 的同一比值为 `0.334`，与 viewer 在 `2560 x 1440` 的 `0.344` 相差 3%。

**本段结论已于 2026-07-29 第六条撤回，勿据此开工。**上面这组比值用的是解析带宽而非官方实测宽度，且 `14.png` 的像素尺度被证明缩放过 `1.148` 倍。在 1080p 对 1080p 下直接量测像素宽度，FWHM 为 viewer `1.50 px` 对官方 `1.26 px`，比值 `1.19`；分辨率悬崖不是解释，同分辨率下仍有 19% 至 31% 的残留展宽。

同时排除了各向异性过滤：官方 `QualitySettings.anisotropicTextures = 2`（ForceEnable），viewer 默认无各向异性。给连接线纹理设 9 与 16 倍并用 `glGetTexParameterfv` 回读确认生效后，720p 下 `FWHM / 带宽` 保持 `0.977` 不变，峰值仅由 `0.1641` 变为 `0.1685`。**各向异性不解释该差异，也不需要为此偏离官方设置。**

#### 星点四边形不是billboard

`Universe/Starmap/Point` 的顶点程序只做 `ObjectToWorld` 与 `unity_MatrixVP` 两级变换，没有任何朝向相机的展开；`star-point`、`starPointBirth`、`starPointSelection` 三个 Transform 的旋转都是单位四元数。因此官方的星点与标记都是固定在世界 XY 平面上的 Quad。

**本段结论已于 2026-07-29 第七条撤回，勿据此开工。** 三个对象都挂着 `Billboard` 组件，它在 `LateUpdate` 里覆盖 Transform 旋转，所以序列化的单位四元数不能作为「不是 billboard」的证据。顶点程序不展开这一点仍然成立，朝向来自 Transform 而不是 Shader。

#### 星图标记

`UIVirtualStarmap._OnLateUpdate` 的逻辑：

- 出生星标记 `starPointBirth`（GameObject `3945`、Transform `14601`）始终跟随 `stars[0]`，缩放固定为序列化值 `1.4`，`_TintColor` 取 `starColors.Evaluate(star.color)`，**不套用中子星、白矮星、黑洞的颜色覆盖**。
- 选中标记 `starPointSelection`（GameObject `259`、Transform `14349`）跟随鼠标射线命中的星，缩放为 `size * 0.6 + 0.6`，`_TintColor` **套用**颜色覆盖。
- 命中判定为：射线到星心距离小于 `1.7`，且在 `scale * 0.25` 内时距离记为 `0`。viewer 既有的 `PickGalaxyStar(...)` 与之逐项一致，无需改动。
- 出生星的名字标签常驻，其余星只在悬停时显示；标签位置为星点屏幕坐标偏移 `(18, 6)`。

`stars[0]` 就是出生星：`UniverseGen.CreateGalaxy` 用 `stars[0]` 决定 `birthStarId`，dspugen 的 `galaxy.cc` 同样在 `i == 0` 时调用 `createBirthStar`。

两个标记共用 Material `116` `galaxy-select-star-2`，Shader path ID `1369` `VF Shaders/Particle/Alpha Blend`，`_Multiplier = 2.5`、`_AlphaMultiplier = 1`。渲染状态为 `Blend SrcAlpha OneMinusSrcAlpha`、`ZTest Off`、`ZWrite Off`、`Cull Off`、`ColorMask RGB`、queue `Transparent`。片元为 `c = tex * _TintColor`，`c.rgb *= _Multiplier`，`c.a = saturate(c.a * _AlphaMultiplier)`，再乘顶点色。

贴图 `_MainTex` = path ID `903` `circle-thin`，256×256 BC7、9 mip、sRGB、Bilinear、Clamp。RGB 在 `251` 至 `255`（BC7 量化噪声，原始应为纯白），图案在 alpha：一圈位于半径 `120/128` 的细环。viewer 只保存 alpha，存为 `viewer/assets/textures/galaxy_star_ring.bin`，提取工具并入 `viewer/tools/galaxy_grid_textures.py`。已知偏差为把 RGB 当作恒等于 1，误差不超过 `4/255`。

状态：连接线为「已确认，无残留差异」；星点朝向与星图标记为「已确认并实现」。（星点朝向的结论已于 2026-07-29 第七条更正为 billboard。）

与既有结论的变化：连接线宽度问题最终关闭，残留 1.6 倍已归因为渲染分辨率而非实现差异（**该归因已于 2026-07-29 第六条撤回**）。新增星点朝向、出生星标记、选中标记与标签位置。

代码位置：`viewer/src/scene_renderer.cc` 的 `DrawGalaxyStarMarker(...)` 与 `DrawGalaxyStarPoint(...)`、`viewer/src/viewer_app.cc` 的 `DrawStarLabel(...)`、`viewer/shaders/{desktop,web}/galaxy_marker.*`、`viewer/src/material_catalog.cc` 的 `GalaxyStarGradient(...)`。

验证命令和结果：

```powershell
python viewer\tools\verify_assets.py          # verified 17 viewer assets
python viewer\tools\verify_shader_pairs.py    # verified 27 desktop/web shader pairs
python viewer\tools\galaxy_grid_textures.py   # 四个文件 SHA-256 与登记值一致
cmake --build cmake-build-viewer-debug --target viewer
cmake-build-viewer-debug\bin\viewer\viewer.exe 1 64
```

Debug 构建通过、无编译警告；运行日志无 `ERROR` 与 `WARNING`。出生星环的截图与 `13.png` 对照，形状与颜色一致，直径比约 `1.64`，与两张图 `1.52` 的像素尺度比在裁剪测量误差内。

剩余问题：

- 选中标记只走了与出生星标记相同的代码路径，尚未做鼠标悬停的交互验收。
- 官方标签为两行，出生星额外带一行本地化的「即将登陆」，正文还包含光谱型后缀；viewer 目前只画星名。这属于本地化与文案范围，未纳入本次改动。
- ~~viewer 默认窗口为 `1280 x 720`，连接线在该分辨率下必然是平顶带，是否超采样尚未决定。~~ 已于 2026-07-29 第六条作废：默认分辨率已改为 `1920 x 1080`，与官方截图一致，不需要超采样。

### 2026-07-29：截图有损压缩的方法论修正、背景星点结案、默认分辨率与交互标记

日期：2026-07-29

参考版本和文件哈希：与「参考版本」一节相同，本次未发生变化。

问题：定位并修复银河视图背景星点亮度偏高；把选中环改为交互选择驱动；默认分辨率改到 1080p。

重开既有区域的原因：新的运行态证据与既有结论矛盾。既有记录把背景星点亮度定为「约为官方 3 倍」、把连接线残留 1.6 倍归因为「渲染分辨率悬崖」，两者都建立在对官方截图的小斑块测光上。本次先复核了测量本身，结论是两条记录都需要修正。

#### 官方截图带有损压缩，`14.png` 还被缩放过

对天区做编码取证：

```text
                Cb 水平 |diff| 偶数对 / 奇数对    (y%8, x%8) 的 mean |G 残差| 波动   G 字节残差 std
官方 13.png     0.749 / 1.197  =  1.60 倍不对称   52.5%                             3.98
viewer 截图     1.024 / 1.022  =  1.00 倍         80.0%（幅度仅 0.39 至 0.70）        1.35
```

偶奇不对称是色度亚采样的直接特征，配合 8×8 块结构，说明 `13.png` 经过 4:2:0 有损编码。viewer 截图没有该特征。

交叉配准进一步发现 `14.png` 的像素尺度不可信：

```text
14.png    -> 1678x944 viewer 截图   尺度 1.1420   29 内点   残差 1.19 px   （真实 720p 源必须为 944/720 = 1.3111）
14.png    -> 13.png                 尺度 1.3100   23 内点   残差 1.56 px
1920x1080 -> 13.png                 尺度 0.9955   32 内点   残差 1.69 px
```

第一行与 `1.3111` 差 `1.148` 倍，因此 `14.png` 被缩放过，不能用它的像素尺度做测光。第三行给出正确基准：**官方截图为 `1920 x 1080`**，与 viewer 新的默认分辨率尺度比为 `0.9955`。同一配准还确认 `13.png` 对应 seed `0`（33 内点，seed `1` 只有 5 内点）。

#### 背景星点亮度：不存在 3 倍偏高

先复核移植是否忠实。`shader_1391_1.asm.txt` 的片元链为 `div_sat` 后乘 `_StarSetting.z`，两次平方得 `core⁴`，`discard` 判据是 `core⁸ < 0.01`，最终 `o0.w = band² · core⁸`，峰值为 `1.1² · 1.1⁸ = 2.594`；顶点程序为 `mov o0.xy, v1.xyxx`，没有任何 UV 变换，因此每个 Quad 恒为 128×128 个星格。viewer 的 `galaxy_bgstars.fs` 与之逐项一致，**指数链与格点数都没有错**。

再在尺度 1.0 下量测（viewer `1920 x 1080`，六个相位覆盖 `sin(_Time.y · 1.5)` 的整个周期）：

```text
最亮背景星峰值（线性亮度）   viewer 0.0219 至 0.0247   官方 0.0238   比值 0.92 至 1.04
顶 25 星通量 B/G            viewer 原始 2.44 至 3.68   官方 1.10
顶 25 星通量 B/G            viewer 经同等 4:2:0 往返 1.23 至 1.30   官方 1.10
```

原始 B/G 与 shader 的解析预测一致：`out.rgb = lerp(_StarColor, noise.rgb, _StarColorChange)` 在 `noise = ref` 时给出 B/G `2.45`（相位 0）至 `3.67`（相位 1）。把**未修改的 viewer 截图**做一次 4:2:0 往返，B/G 立刻落到 `1.23 至 1.30`，与官方一致——色度亚采样会把 1 px 亮点的色度摊到 2×2 块里，使其向中性偏移。因此官方看起来更白是编码造成的。

密度用阈值扫描判定，单位为每 10k 官方像素：

```text
残差阈值      0.004    0.006    0.010    0.015    0.020    0.030
官方         173.46    57.35    13.72     3.07     1.06     0.00
viewer        12.35    10.63     6.13     3.64     1.05     0.00
比值           14.05     5.40     2.24     0.85     1.01      --
```

比值在阈值越过官方噪声底后收敛到 1。官方天区字节残差 std 为 `3.98`（约 `0.0018` 线性亮度），阈值 `0.015` 与 `0.020` 已在其 8 至 11 倍之外，此时两侧密度一致。低阈值处的巨大比值全部来自官方截图的压缩噪声。

结论：**背景星点的峰值亮度、颜色和密度在等条件下都与官方一致，不存在需要修复的亮度问题。** 既有记录里的「约 3 倍」来自对有损压缩、且俯仰与像素尺度都不同的截图做小斑块峰值统计。

#### 修复：清屏色的红色通道被 8 位量化抹掉

`rlClearColor()` 只接受字节。相机清屏色 sRGB `(5, 21, 26)` 转线性为 `(0.0015176, 0.0074877, 0.010316)`，原实现再乘 `255` 取整得 `(0, 2, 3)`：**红色整通道归零**，绿色抬高 `4.7%`，蓝色抬高 `14.0%`。改为直接用 `glClearColor()` 上传浮点值。

```text
天区底色 sRGB   修复前 (2.9, 20.1, 27.3)   修复后 (5, 19, 24)   官方 (6, 19, 25)
```

三个通道都向官方收敛，绿蓝已一致。这同时关闭了既有记录中「官方画面在银河区域有一层偏红的底光，来源尚未定位」：官方的红不是额外底光，而是 viewer 丢掉了清屏色的红色通道。

#### 连接线：原「分辨率悬崖」解释不成立，残留 19% 至 31%

既有记录的 `FWHM / 理论带宽` 比值链是在 `14.png` 的错误像素尺度下算出的，并且用的是解析带宽而不是官方的实测宽度。在真实 1:1（两侧均 `1920 x 1080`，配准尺度 `0.9955`）下直接量测像素宽度，10 条可用线段的中位数：

```text
FWHM          viewer 1.50 px   官方 1.26 px   比值 1.19
四分之一宽     viewer 2.50 px   官方 1.88 px   比值 1.31
峰值                                          比值 0.79
宽度 × 峰值                                    比值 0.96
```

总能量仍然一致（`0.96`），差异仍是「被抹开」而非几何变宽。但**「1080p 与 1440p 之间存在悬崖、官方在 720p 下会得到同样粗的线」的说法不成立**：官方与 viewer 在同一 1080p 下相差 `1.19` 倍，不是 1.6 倍，也不需要分辨率悬崖来解释。官方截图的有损编码只会让窄特征变钝，因此这 19% 残留不是压缩造成的，属于尚未定位的真实差异。修正后的量级远小于原先认为的 1.6 倍。

#### 星图标记与标签的官方语义补正

`UIVirtualStarmap._OnLateUpdate` 的三处细节需要补正或明确：

- 选中标记的缩放是 `typeScale * 0.6 + 0.6`，其中 `typeScale` 是**类型常量**（默认 `1.2`、巨星 `3`、白矮星与中子星 `0.6`、黑洞 `0.8`），不是星点的世界缩放。viewer 的 `GalaxyStarScale(...)` 返回同一组常量，因此现有的 `galaxyScale * 0.6 + 0.6` 等价，无需改动。
- 命中候选循环从 `i = 0` 开始，包含出生星，因此官方悬停出生星时出生环与选中环会叠加。
- 标签循环从 `j = 1` 开始（出生星标签常驻，不受该循环控制），并且只用命中判定 `j == num` 作为开关，与是否为选中项无关。官方还在按住确认键时给命中星追加「安全系数」与 `clickText` 两行。

#### 本次的 viewer 行为改动

以下三项是 viewer 行为，其中前两项与官方星图语义不同，原因是官方星图没有点击选中：

- 选中环改由**点击选中项**驱动。官方用射线命中驱动；viewer 有点击选中与右侧详情面板，按需求改为选中驱动。
- 悬停仍只显示星名。去掉了原先 `hoveredStar != selectedStar` 的排除，使选中项在被悬停时也显示星名，这一点反而与官方「只按命中判定开关」一致。
- 默认窗口从 `1280 x 720` 改为 `1920 x 1080`，与官方截图的 1080p 一致。

状态：背景星点亮度为「已关闭，等条件下匹配」；清屏色为「已确认并修复」；连接线残留展宽为「已量化，未定位」；标记与标签语义为「已确认」。

与既有结论的变化：撤回「背景星点亮度约为官方 3 倍」和「连接线残留 1.6 倍源于渲染分辨率悬崖」两条结论，前者不存在，后者的真实数值是同分辨率下 1.19 倍。关闭「银河区域偏红底光来源未定位」。新增官方截图带 4:2:0 有损压缩这一测量约束。

代码位置：`viewer/src/post_process.cc` 的 `ClearLinearBackground(...)`、`viewer/src/viewer_app.cc` 的 `kScreenWidth` / `kScreenHeight`、`DrawSceneCallback(...)` 与 `DrawHoverLabel(...)`、`viewer/src/scene_renderer.cc` 的 `DrawGalaxy(...)`。

验证命令和结果：

```powershell
python viewer\tools\verify_assets.py            # verified 17 viewer assets
python viewer\tools\verify_shader_pairs.py      # verified 27 desktop/web shader pairs
python viewer\tools\galaxy_grid_textures.py     # 四个文件 SHA-256 与登记值一致
python viewer\tools\galaxy_bgstars_texture.py   # SHA-256 与登记值一致
cmake --build cmake-build-viewer-debug --target viewer
cmake-build-viewer-debug\bin\viewer\viewer.exe 0 64
```

Debug 构建通过、无编译警告；运行日志无 `ERROR` 与 `WARNING`，`Screen size` 与 `Render size` 均为 `1920 x 1080`。强制选中第 `5` 颗星的截图显示出生环仍在 `stars[0]`、选中环落在该星、右侧面板同步为该星，1080p 下 UI 版面无错位。所有测量用的临时插桩（窗口尺寸覆盖、指定帧截图、强制选中）均已回滚，`viewer_app.cc` 的 md5 与插桩前一致。

剩余问题：

- 连接线在同分辨率下仍有 19% 至 31% 的额外展宽，来源未定位。已排除几何、材质常量、Shader、纹理数据、相机、采样数、纹理绑定、各向异性过滤和渲染分辨率。
- 选中环只做了强制选中路径的截图验收，鼠标点击的人工验收未做。
- 官方标签为两行，出生星额外带一行本地化的「即将登陆」，正文还包含光谱型后缀；按需求 viewer 保持只画星名。
- Dithering 仍未实现。其幅度为 sRGB `±1/255`，不要与官方截图天区里 `±4` 字节的压缩噪声混为一谈。
- 现有官方截图全部带 4:2:0 有损压缩。后续任何测光比对都必须先把 viewer 侧做同等编码，或获取无损官方截图。

### 2026-07-29：星点与标记的朝向来自 Billboard 组件

日期：2026-07-29

参考版本和文件哈希：与「参考版本」一节相同，本次未发生变化。

问题：实测反馈银河视图的恒星变成纸片，圆点不再始终面向镜头。

重开既有区域的原因：新的运行态证据与既有结论矛盾。第五条记录判定官方星点「不是 billboard、固定在世界 XY 平面」，据此移除了 viewer 的 billboard。该判定错误。

#### 原判定为什么站不住

原判定用了两条证据：顶点程序不做朝向展开，以及三个 Transform 的序列化旋转都是单位四元数。第一条正确，第二条**不能推出结论**——如果对象挂了运行时改写旋转的组件，序列化值就只是初始值。

而且原判定与官方自己的相机范围自相矛盾：`Galaxy Select` 的俯仰可在 `[-81.2, -4.9]` 内调整，固定在世界 XY 平面的 Quad 在 `-81.2` 下会被压成 `cos(81.2) = 0.153` 的细条。官方画面不是这样，所以朝向必然由别处提供。

#### 证据

反编译中存在 `Billboard.cs`：

```csharp
private void LateUpdate() {
    Camera main = lookCamera;
    if (main == null) main = Camera.main;
    if (!(main == null)) {
        if (vertical) {
            Vector3 forward = main.transform.forward;
            forward.y = 0f; forward.Normalize();
            base.transform.rotation = Quaternion.LookRotation(forward, Vector3.up);
        } else {
            base.transform.rotation = main.transform.rotation;
        }
    }
}
```

在 `level0` 里按组件枚举，三个星点对象都挂着它：

| GameObject | GameObject path ID | MonoBehaviour path ID | `m_Enabled` | `lookCamera` | `vertical` |
| --- | --- | --- | --- | --- | --- |
| `star-point` | `5344` | `48916` | `1` | `PPtr(0, 0)` 空 | `0` |
| `birth-star-point` | `3945` | `47273` | `1` | `PPtr(0, 0)` 空 | `0` |
| `selected-star-point` | `259` | `42968` | `1` | `PPtr(0, 0)` 空 | `0` |

三者的 `m_Script` 都指向 `globalgamemanagers.assets` 的 path ID `2473`，其 `MonoScript` 的 `m_ClassName` 为 `Billboard`、`m_AssemblyName` 为 `Assembly-CSharp`。`lookCamera` 为空因此回落到 `Camera.main`；`vertical` 为 `0` 因此走 `transform.rotation = main.transform.rotation`，即**整体对齐相机朝向的屏幕对齐 billboard**，不是逐个朝相机位置的 look-at。

这三个 MonoBehaviour 的序列化长度都是 48 字节，与 `Billboard` 的字段布局一致：`MonoBehaviour` 基类 32 字节（`m_GameObject` PPtr 12、`m_Enabled` 1 加 3 对齐、`m_Script` PPtr 12、`m_Name` 长度 4），加 `Camera lookCamera` PPtr 12 与 `bool vertical` 1 加 3 对齐，共 16 字节。

`level0` 中共有 10 个 `Billboard` 组件，全部 `vertical = 0`：`star-point`、`birth-star-point`、`selected-star-point`、`Point A`、`Point B`、`center-1`、`center-2`、`dot`、`text`、`text2`。**网格平面、白色边框和六个天空盒 Quad 都不在其中**，因此这些对象的朝向仍然只由序列化旋转决定，第二条与第四条记录中它们的实现不受影响。

#### 实现

viewer 恢复了 billboard，两处共用 `CameraBillboardRotation(...)`：星点与两个标记环。基向量按右手系构造为 `right = normalize(cross(forward, up))`、`up = cross(right, forward)`；此前的实现用的是 `cross(up, forward)`，方向相反，对径向对称的贴图不可见，但已一并改正。

#### 验证

seed `0`、64 星、`1920 x 1080`，用强度二阶矩量测星点核心的轴向长宽比。`height / width` 为轴对齐量度，直接反映压缩：

```text
capture                        n    height/width   若不做 billboard 的预测值
修复前  yaw   0  pitch -22     54       0.873       0.927
修复后  yaw   0  pitch -22     54       1.006        --
修复后  yaw  70  pitch -22     68       1.000       0.342
修复后  yaw   0  pitch -78     56       1.000       0.208
修复后  yaw  45  pitch -60     77       0.995       0.707
```

修复后四个朝向全部为 `1.00`。修复前在默认俯仰下已有 `0.873`，与 `cos(22) = 0.927` 的量级一致；`yaw 70` 与 `pitch -78` 两种情况下不做 billboard 会分别压到 `0.342` 与 `0.208`，后者就是实测反馈的「纸片」。`pitch -78` 的截图另经目视确认：所有星点与出生星环均为正圆。

状态：已确认并实现。

与既有结论的变化：撤回第五条的「星点与标记不是 billboard、固定在世界 XY 平面」。顶点程序不做朝向展开这一点仍然成立；朝向由 `Billboard` 组件在 `LateUpdate` 中提供。

代码位置：`viewer/src/scene_renderer.cc` 的 `CameraBillboardRotation(...)`、`DrawGalaxyStarPoint(...)` 与 `DrawGalaxyStarMarker(...)`。

验证命令和结果：

```powershell
python viewer\tools\verify_assets.py         # verified 17 viewer assets
python viewer\tools\verify_shader_pairs.py   # verified 27 desktop/web shader pairs
cmake --build cmake-build-viewer-debug --target viewer
cmake-build-viewer-debug\bin\viewer\viewer.exe 0 64
```

Debug 构建通过、无编译警告；运行日志无 `ERROR` 与 `WARNING`。量测用的临时插桩（指定帧截图、相机 yaw/pitch 覆盖）已回滚，`viewer_app.cc` 的 md5 与提交版本一致，`camera_controller.cc` 无改动。

方法论教训：**序列化的 Transform 值只能作为初始值使用。判断朝向前必须枚举对象的组件列表，确认没有运行时改写旋转的 MonoBehaviour。** 本次的直接线索是组件枚举时出现「Expected to read 48 bytes, but only read 32 bytes」——那是一个类型树缺失、未被识别的 MonoBehaviour，不应忽略。
