# Viewer 响应式布局 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 Web 版 viewer 跟随浏览器窗口尺寸，并让详情面板中的长矿物列表在独立区域内滚动。

**Architecture:** 保留 raylib 的绘制模型和现有面板宽度，将窗口尺寸同步封装在 `Viewer` 的 Web 平台代码中。将面板几何和滚动状态放入 viewer UI 层，由 `Viewer` 负责把面板滚轮事件从场景相机输入中分流出去；矿物绘制使用 scissor 限制在滚动区域内。

**Tech Stack:** C++ 17、raylib 6.0、Emscripten Web API、现有 CMake 构建。

---

### Task 1: 添加可测试的面板滚动计算

**Files:**
- Create: `viewer/src/panel_scroll.hh`
- Create: `viewer/src/panel_scroll.cc`
- Create: `tests/viewer_panel_scroll_test.cc`
- Modify: `viewer/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

添加纯计算测试，覆盖内容高度小于视口、内容高度超过视口、滚轮上限和下限：

```cpp
#include "panel_scroll.hh"
#include <cassert>

int main() {
    using viewer::PanelScroll;
    assert(PanelScroll::MaxOffset(100.0f, 200.0f) == 0.0f);
    assert(PanelScroll::MaxOffset(300.0f, 200.0f) == 100.0f);
    assert(PanelScroll::ApplyWheel(0.0f, 3.0f, 300.0f, 200.0f) == 0.0f);
    assert(PanelScroll::ApplyWheel(0.0f, -3.0f, 300.0f, 200.0f) == 100.0f);
    assert(PanelScroll::ApplyWheel(100.0f, -3.0f, 300.0f, 200.0f) == 100.0f);
    assert(PanelScroll::ApplyWheel(0.0f, 3.0f, 100.0f, 200.0f) == 0.0f);
}
```

- [ ] **Step 2: 运行测试确认失败**

运行：

```powershell
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DBUILD_VIEWER=ON -DBUILD_VIEWER_TESTS=ON -DBUILD_VIEWER_WEB=OFF -DFMT_TEST=OFF
cmake --build cmake-build-debug --target viewer_panel_scroll_test
cmake-build-debug\bin\viewer_panel_scroll_test.exe
```

预期：配置或编译阶段失败，因为 `PanelScroll` 和测试目标尚未实现。

- [ ] **Step 3: 实现最小滚动计算**

在 `panel_scroll.hh` 声明：

```cpp
namespace viewer {
struct PanelScroll {
    static float MaxOffset(float contentHeight, float viewportHeight);
    static float ApplyWheel(float offset, float wheel, float contentHeight, float viewportHeight);
};
}
```

在 `panel_scroll.cc` 中使用 `std::clamp`，将 offset 限制在 `[0, maxOffset]`；列表向下滚动时增加 offset，因此 raylib 的绘制坐标使用 `y - offset`。

- [ ] **Step 4: 运行测试确认通过**

运行同一测试命令，预期进程退出码为 `0`。

- [ ] **Step 5: 将测试目标接入 CMake**

仅在 `BUILD_VIEWER` 和 `BUILD_VIEWER_TESTS` 开启时构建测试；viewer 和测试共用 `panel_scroll.cc`，不引入测试框架。若当前工程没有 `BUILD_VIEWER_TESTS`，在根 `CMakeLists.txt` 增加默认关闭的选项，并在 `viewer/CMakeLists.txt` 中添加测试可执行文件。

### Task 2: 实现面板分区与矿物列表滚动

**Files:**
- Modify: `viewer/src/viewer_ui.hh`
- Modify: `viewer/src/viewer_ui.cc`
- Modify: `viewer/src/viewer_app.cc`

- [ ] **Step 1: 写面板滚轮分流测试所需接口**

让 UI 层暴露以下不依赖 `Viewer` 的计算接口：

```cpp
struct ViewerPanelLayout {
    Rectangle contentBounds;
    Rectangle mineralBounds;
    Rectangle footerBounds;
};

ViewerPanelLayout CalculateViewerPanelLayout(int screenWidth, int screenHeight,
                                             int mineralContentHeight);
```

测试应验证：矿物区域位于 footer 上方，列表过长时不增加 footer 的 y 坐标，屏幕高度变化后区域仍在窗口内。

- [ ] **Step 2: 在 `DrawViewerPanel` 中使用分区布局**

顶部标题、名称、subtitle 和普通 key/value 行继续从现有坐标开始绘制。根据剩余空间计算 `mineralBounds` 与 `footerBounds`；矿物行在 `BeginScissorMode` / `EndScissorMode` 内以 `y - offset` 绘制。footer 使用 `GetScreenHeight()` 计算的固定底部位置。

- [ ] **Step 3: 增加滚动状态和事件分流**

在 `Viewer` 中添加 `float panelScrollOffset`，在切换星体、行星、系统和重建 galaxy 时清零或重新限制。添加 `UpdatePanelScroll()`：只有鼠标位于右侧面板、当前 panel 有超出视口的矿物内容且未拖动时间滑块时消费 `GetMouseWheelMove()`。将 `CameraController::Update` 接收的 wheel 改为由 `Viewer` 先消费或清零，确保面板滚轮不会缩放场景。

- [ ] **Step 4: 验证长列表与窗口变化**

使用一个矿物行数较多的 panel fixture 或临时调试数据检查：矿物首行和末行不会绘制到面板外，滚动 offset 始终在合法范围，底部提示保持可见。

### Task 3: 添加 Web canvas 尺寸同步

**Files:**
- Modify: `viewer/src/viewer_app.cc`
- Modify: `viewer/web/shell.html`

- [ ] **Step 1: 复用 raylib Web resize 回调并同步初始尺寸**

raylib Web 后端已经在 `rcore_web.c` 中注册 `emscripten_set_resize_callback`，并根据 `window.innerWidth/innerHeight` 更新 screen 和 canvas 尺寸。不要在 viewer 中重复注册回调；在 `Viewer::Init()` 的 `__EMSCRIPTEN__` 分支使用 `EM_ASM_INT` 读取 `document.documentElement.clientWidth/clientHeight`，把首次 `InitWindow` 的尺寸从固定值替换为 viewport 尺寸。

- [ ] **Step 2: 让 shell 明确提供可用尺寸**

保留 canvas 的 `100vw x 100vh` 铺满规则，补充 `min-width: 0; min-height: 0;` 和 `display: block` 所需的父级约束，确保 viewport 尺寸传递到 canvas；不引入页面滚动条。

- [ ] **Step 3: 验证 Web 编译和 resize**

使用可用 Emscripten 工具链配置并构建 viewer Web 目标。浏览器中打开生成的 `index.html`，检查初始 canvas 尺寸、改变窗口尺寸后 raylib screen 尺寸、详情栏边界和顶部栏宽度同步变化，并确认无 WebGL 或 JavaScript 错误。

### Task 4: 按项目要求完成构建和浏览器 QA

**Files:**
- No additional source files.

- [ ] **Step 1: Debug 构建**

```powershell
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DBUILD_VIEWER=ON -DBUILD_VIEWER_TESTS=ON -DFMT_TEST=OFF
cmake --build cmake-build-debug --target viewer_panel_scroll_test viewer
```

- [ ] **Step 2: Release 构建**

```powershell
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DBUILD_VIEWER=ON -DBUILD_VIEWER_TESTS=ON -DFMT_TEST=OFF
cmake --build cmake-build-release --target viewer
```

- [ ] **Step 3: 浏览器检查**

The flow under test is: generated Web viewer page -> galaxy/system panel -> resize browser viewport and scroll mineral list -> panel remains bounded and footer remains visible.

使用 Browser skill 检查页面标题、非空 DOM、截图、控制台错误；分别检查桌面尺寸和移动尺寸。若本地 Web 构建或 Browser 运行时不可用，记录具体阻塞和已完成的静态/编译检查。

- [ ] **Step 4: 查看工作区差异**

运行 `git diff --check`、`git status --short`，确认只包含 viewer 修改、测试和规格/计划文档；保留用户已有的 `filters/zz.cc` 与 `.superpowers/` 临时文件，不将它们加入本次提交。
