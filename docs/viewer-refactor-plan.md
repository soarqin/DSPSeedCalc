# Viewer 重构计划

## 背景

当前 Viewer 的生成数据适配、视图缓存、输入处理、相机、GPU 资源、后处理、场景绘制和 UI 集中在 `viewer/viewer.cc`。资源文件平铺在 `viewer/`，Native 复制列表与 Web preload 列表重复维护。重构目标是在保持现有生成结果和渲染证据不变的前提下，建立可独立维护、验证和扩展的模块边界。

本计划以 `docs/viewer-parity-evidence.md` 为约束。已经确认或排除的连接线、相机、Shader、纹理、MSAA、Bloom、Color Grading 和 FXAA 结论不因重构重新调查，也不通过截图拟合参数。

## 目标

1. 将单文件按数据、资源、后处理、场景、交互和 UI 职责拆分。
2. 建立统一的资源目录、清单、校验和 staging 流程。
3. 让 Native 和 Web 使用相同的逻辑资源路径。
4. 消除全局渲染资源、每帧 uniform 查询、重复几何上传和未使用的气体生成。
5. 去除 Viewer 对进程当前工作目录的修改。
6. 保持 `dspugen` 的随机数消费、浮点转换、对象顺序和位级 fixture 不变。

## 目标结构

```text
viewer/
    CMakeLists.txt
    src/
        main.cc
        viewer_app.hh/.cc
        view_model.hh/.cc
        camera_controller.hh/.cc
        material_catalog.hh/.cc
        render_resources.hh/.cc
        post_process.hh/.cc
        galaxy_renderer.hh/.cc
        system_renderer.hh/.cc
        viewer_ui.hh/.cc
        web_input.hh/.cc
    shaders/
        desktop/
        web/
    assets/
        textures/
        meshes/
        manifest.json
    tools/
        galaxy_line_texture.py
        verify_assets.py
        verify_shader_pairs.py
    web/
        shell.html
```

运行时统一使用以下逻辑布局：

```text
viewer/
    assets/
        shaders/
        textures/
        meshes/
    Prototypes/
        ThemeProtoSet.json
        ItemProtoSet.json
        VeinProtoSet.json
    viewer-font.ttf
```

## 模块边界

| 模块 | 职责 |
| --- | --- |
| `main` | 解析启动参数并运行应用 |
| `viewer_app` | 生命周期、模式切换、更新和绘制顺序 |
| `view_model` | 将 `dspugen` 对象转换为 Viewer 缓存，创建连接图和恒星系数据 |
| `material_catalog` | 恒星、行星和连接线视觉参数表及插值 |
| `camera_controller` | 银河和恒星系相机状态及输入 |
| `render_resources` | Font、Shader、uniform location、纹理、网格和材质的所有权 |
| `post_process` | HDR/MSAA target、Bloom、Color Grading 和 FXAA |
| `galaxy_renderer` | 银河网格、连接线和恒星点 |
| `system_renderer` | 背景星、恒星、轨道和行星 |
| `viewer_ui` | 面板、输入框、按钮、选择和悬停标签 |
| `web_input` | Emscripten 鼠标事件适配 |

依赖方向为 `dspugen -> view_model/material_catalog -> renderer/UI/camera -> viewer_app -> main`。`dspugen` 不依赖 Raylib，不接收 Viewer 专用颜色、材质、相机或 UI 逻辑。

## 资源策略

- Prototype、LUT、大纹理和提取网格继续作为文件保存，便于哈希校验、审查和独立缓存。
- Desktop 和 Web Shader 第一阶段保留为两套已确认文件，通过脚本检查配对，不自动重写表达式或精度限定。
- 两组字节一致的 Spot/Galaxy Point quad 改为一份静态几何。
- Native 使用专用运行目录和带 stamp 的 staging；无变化的构建不重复复制资源。
- Web 默认输出目录化的 HTML、JavaScript、Wasm 和 data 文件，不再使用名义上的单文件混合配置。
- Web 只打包三个实际使用的 Prototype 文件。

## `dspugen` 调整边界

允许增加带显式目录参数的 `loadProtoSets` 重载，并保留无参数版本兼容 CLI 和 oracle。除非存在独立公共需求，不恢复 `Planet` 中被省略的 Viewer 轨道字段，不把 Viewer 的连接图、坐标转换、面板文本或材质数据移入生成器。

对象生命周期保持：

```text
load Prototype
-> init Galaxy/Star/Planet thread pools
-> create/use/release Galaxy on the same thread
-> release Planet/Star/Galaxy thread pools
```

## 执行阶段

### 1. 资源与构建

- 建立资源目录和 `manifest.json`。
- 增加资源哈希、尺寸和 Shader 配对校验工具。
- 改为清单驱动的 Native staging 和 Web preload。
- 隔离 Viewer 运行目录，修正 shell 和字体的构建依赖。

### 2. Prototype 路径

- 为 `dspugen::loadProtoSets` 增加显式目录参数。
- Viewer 使用应用目录或 Web VFS 路径，不再调用 `ChangeDirectory`。
- 运行 GCC Debug、GCC Release 和 Clang Release 位级 oracle。

### 3. 数据和状态

- 迁移 ViewModel、连接图、面板数据、轨道参数和材质表。
- 将选择状态从 vector 元素裸指针改为索引，避免重建后悬空引用。
- Renderer 只读取 Viewer 缓存，不直接组织生成器生命周期。

### 4. GPU 资源和后处理

- 统一 GPU 资源所有权和失败回收。
- 初始化时缓存所有 uniform location。
- 将 FBO、MSAA resolve 和后处理 pass 封装到 `PostProcess`。
- 保持既有后处理顺序和常量不变。

### 5. 场景、相机和 UI

- 分别迁移银河和恒星系 Renderer。
- 封装相机状态及 Native/Web 输入差异。
- 迁移面板、输入框、选择和悬停绘制。
- `ViewerApp` 最终只保留应用级编排。

### 6. 低风险优化

- 缓存 uniform location 和静态材质数据。
- 复用 GeoSphere mesh 和 quad mesh。
- Viewer 只调用 `ensureAllPlanets`，不生成未使用的 gas 数据。
- 缓存轨道相对点，消除每帧三角函数和矩阵计算。
- 消除时间倍率和输入框测量中的无必要临时字符串。

背景星实例化、银河点实例化、连接线批量 VBO、轨道批处理和懒创建恒星系缓存只在基准证明必要后实施，不与结构重构混合。

## 验证

每个阶段执行适用检查：

1. Native Debug 和 Release 构建，C++17，Viewer 启用。
2. Emscripten Release 构建和 HTTP 启动检查。
3. 固定 seed `0`、64 星的 Galaxy/System smoke。
4. 从任意工作目录启动 Native Viewer。
5. 检查所有 Shader 未回退到 Raylib 默认 Shader，记录 HDR FBO 实际样本数。
6. 比较资源迁移前后的 SHA-256。
7. 修改 `dspugen` 后运行 GCC Debug、GCC Release、Clang Release oracle 和 FP flag audit。
8. 固定窗口、相机和时间进行重构前后截图比较。截图只用于回归验收，不用于修改常量。
9. 性能变更记录编译器、构建类型、分辨率、seed、星数、场景、采样次数、中位数和 MAD。

## 完成标准

- 原综合性 `viewer.cc` 被职责明确的模块替代。
- GPU 资源、生成数据、相机、后处理和 UI 具有明确所有者。
- Native 和 Web 使用相同逻辑资源路径，CMake 只维护一份资源映射。
- Viewer 不修改当前工作目录，不生成未使用的 gas 数据。
- 每帧路径不查询 uniform location。
- GeoSphere 和 quad 不再重复上传或保存重复文件。
- 选择状态不依赖 vector 元素裸指针。
- `dspugen` 位级 fixture 保持一致。
- 已确认的画面、交互和后处理结果不因重构改变。
