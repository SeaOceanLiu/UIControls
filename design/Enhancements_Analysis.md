# 增强需求分析（7 项 × 三后端）

> 状态：分析中 · 依据：2026-08 全仓库勘察（含子模块） · 用途：持续思考的结论依据
> 相关文档：[LuotiAni_Design.md](LuotiAni_Design.md)、[BackendAbstraction_Design.md](BackendAbstraction_Design.md)、[GraphTool_Design.md](GraphTool_Design.md)

## 1. 背景与范围

以下 7 项增强需求，逐一给出"现状 → 三后端（SDL3 / SFML / Raylib）支持度 → 实现要点 → 取舍点"：

1. 起线程做 LuotiAni 烘焙，避免影响主程序加载
2. LuotiAni 支持基本 Shape、text、线宽，最好将 SVG 能力整体迁入
3. Button 支持圆角矩形、圆形
4. 支持 RadioButton、状态栏
5. 支持触控滚动
6. 快捷键系统——菜单快捷键绑定 / 全局快捷键
7. JSON 解析是否放到窗体初始化完成后（以便有 renderer、叠加多线程烘焙提速）

另有两项独立小问题（ColorPicker 十六进制回车生效、两个 test 窗体尺寸），已由主设计开发 Session 完成，本分析不涉及。

---

## 2. 支持矩阵（结论先行）

| # | 增强 | SDL3 | SFML | Raylib | 关键差异 |
|---|---|---|---|---|---|
| 1 | 多线程烘焙 | ✅ 需改造 | ✅ 需改造 | ✅ 最顺 | sdl3/sfml 的 rotate 是 GPU 往返；raylib 已是纯 CPU 旋转 |
| 2 | Shape/Text/线宽/SVG | ✅ | ⚠️ TEXT 受限 | ✅ | sfml 无纯软件文字栅格化；SVG 三端均已内嵌 nanosvg |
| 3 | Button 圆角/圆形 | ✅ | ✅ | ✅ | 无差异——GraphTool 圆角/圆是三角剖分/逐像素，落到通用原语 |
| 4 | RadioButton/状态栏 | ✅ | ✅ | ✅ | 无差异（组合控件） |
| 5 | 触控滚动 | ✅ 桌面触屏可用 | ✅ 桌面触屏可用 | ❌ 桌面无触控 | raylib GetTouch* 仅移动平台；三端均需新接事件 |
| 6 | 快捷键系统 | ⚠️ 全局热键半支持 | ⚠️ 同左 | ⚠️ 同左 | 窗口内快捷键三端全支持；系统级热键无原生通路 |
| 7 | JSON 解析时机 | ✅ | ✅ | ✅ | 无差异（且已是现状） |

---

## 3. 逐项分析

### 3.1 多线程烘焙

**现状**：`prepare()` 全程同步单线程（`src/LuotiAni.cpp:597-761` 无任何线程构造）；`m_frames` 为"每帧一张纹理"模型（`include/LuotiAni.h:279`）。

**GPU/CPU 拆分**：

| 阶段 | 性质 | 线程约束 |
|---|---|---|
| Surface 解码（PNG/SVG） | CPU | 无约束，可后台 |
| blit 合成逐帧 canvas | CPU | 无约束，可后台 |
| **rotate（sdl3）** | GPU 往返（`sdl3/RenderDevice.cpp:449-478`，RenderPresent+ReadPixels） | 仅渲染线程 |
| **rotate（sfml）** | GPU 往返（RenderTexture 绘制+读回，`sfml/RenderDevice.cpp:155-170`） | 仅渲染线程 |
| **rotate（raylib）** | 纯 CPU 双线性（`raylib/RenderDevice.cpp:301-357`） | 无约束 ✓ |
| createTexture（sdl3/sfml/raylib） | GPU | 三端均仅渲染线程（SDL renderer / sfml setActive / raylib GL） |

**结论**：三端均可实现"CPU 合成后台化 + 纹理主线程上传"的两阶段模型；但 sdl3/sfml 必须先补 CPU 旋转实现（仿 raylib 双线性），否则 rotate 阶段无法离开渲染线程。

### 3.2 LuotiAni Shape / text / 线宽 / SVG

**现状**：解析层已支持 `type: shape/text`（`include/LuotiAni.h:113-124`），prepare 中硬跳过（`src/LuotiAni.cpp:622-625`）；无描边/线宽概念；SVG 仅在 image 图层作素材。

**SVG 能力（三端均已具备）**：
- sdl3：SDL3_image 静态内嵌 nanosvg（`subModules/SDL3_image/src/IMG_svg.c:74-76`，CMake 选项 `SDLIMAGE_SVG`），文件+内存路径均可
- raylib：后端自带 nanosvg（`raylib/RenderDevice.cpp:166-235`），仅内存路径支持 SVG
- sfml：同上（`sfml/RenderDevice.cpp:194-269`），仅内存路径

LuotiAni 素材走 `loadFromMemory` → 三端 SVG 素材路径均可用 ✓

**文字图层（三端差异点）**：
- sdl3：`TTF_RenderText_*` 软渲染（已具备未使用）✓ 纯 CPU
- raylib：`ImageDrawTextEx`（`raylib.h` 软件绘制到 Image，未使用）✓ 纯 CPU
- sfml：**无纯软件文字 API**，只能 RenderTexture（GPU）绘制后读回 → 后台烘焙受限
- 取舍：接受"sfml 文字图层主线程烘焙"限制，或引入软件字体栅格化依赖（如 stb_truetype，需评估库体积）

**Shape/线宽**：需新增纯 CPU 栅格器（覆盖率合成，可参照 GraphTool 的 coverage 思路但脱离 RenderDevice），后端无关 ✓

### 3.3 Button 圆角矩形 / 圆形

**现状**：Button 背景/边框均为直角矩形（`src/ControlBase.cpp:224-284`）；圆角/圆形绘制能力在 GraphTool（`drawRoundedRect` :247、`drawCircle` :491、`drawEllipse` :514）。

**跨后端可行性**：GraphTool 图元全部落到 RenderDevice 通用原语（`drawTriangle/drawLine/drawPoint/drawQuad`，`include/RenderDevice.h:31-47`），三端均已实现（sdl3 `SDL_RenderGeometry`、sfml 顶点批、raylib `DrawTriangle/DrawPixel`）→ **三端无差异**。

### 3.4 RadioButton / 状态栏

两控件均不存在；基于既有 Panel/Label/CheckBox 模式 + 第 3 项圆形能力即可组合实现 → **三端无差异**。需注册进 PropertyNames + LayoutParser 控件分发（现有 21 种控件，`src/LayoutParser.cpp:254-312`）。

### 3.5 触控滚动

**现状**：Event 层有 Finger 类型（`include/EventTypes.h:13`，兼容遗留）；三端 InputBackend 均未接线 touch：
- sdl3：`SDL_EVENT_FINGER_*` 已定义（`SDL_events.h:212-215`），后端 switch（`sdl3/InputBackend.cpp:286-367`）无分支
- sfml：`sf::Event::TouchBegan/Moved/Ended` 已定义（`Event.hpp:266-290`），后端零匹配
- raylib：`GetTouch*/GetGesture*` 已定义（`raylib.h:1238-1254`），后端零匹配

**能力不对称**：sdl3/sfml 桌面 Windows 触屏可产生触摸事件；**raylib 桌面端触摸 API 恒无效**（仅 Android/iOS/Web）。

**建议**：三端统一实现事件转换；raylib 侧利用既有"后端能力位机制"（`UICornerstone_GetBackendCapabilities`）声明触摸能力，调用方条件化。

### 3.6 快捷键系统

**现状**：`MenuItem::m_shortcut` 仅为显示文本（`src/Menu.cpp:146-153` 右对齐绘制），无键盘触发、无 mnemonic、无系统热键。

**窗口内快捷键**：三端键盘 + KeyMod 完整（sdl3 `SDLKeymodToKeyMod` `:150-153`、raylib 逐位拼装 `:234-248`、sfml bool 拼装 `:92-104`）→ **三端全支持**。

**系统级全局热键**：**三端均无原生通路**：
- sdl3：子模块快照无 `SDL_RegisterHotkey`（无 SDL_hotkeys.h）
- raylib：公共 API 无 WndProc 钩子（仅 TraceLog/文件 IO 回调类）
- 原生 HWND 均可得：sdl3 `SDL_PROP_WINDOW_WIN32_HWND_POINTER`、sfml `getNativeHandle()`（已有 GetDpiForWindow 先例，`sfml/Window.cpp:47`）、raylib `GetWindowHandle()`（`raylib.h:1014`，当前后端返回 nullptr 需补）

**方案**：统一走 win32 窗口子类化（`SetWindowLongPtr`）收 `WM_HOTKEY`——平台适配层，与后端正交，三端对称；Linux/移动端暂缓。

### 3.7 JSON 解析时机

**结论：已是现状，无需改动。** 勘察确认：
- C ABI 路径：`UICornerstone_CreateInstance` 先建窗体+设备并 dummy present 激活 GL（`src/UICornerstoneAPI.cpp:296-358`），`UICornerstone_LoadLayout`（:893-905）在后
- 旧式路径：解析发生在 `onInit`（窗口已建）
- 纹理延迟到挂树后、设备下发时创建（`src/ControlBase.cpp:344-363`）

真正的慢点是第 3.1 项同步 prepare + GPU 往返旋转，与本项无关。

---

## 4. GPU 多线程专项：约束与可选路径

**问题**："GPU 依赖无法多线程吗？"——GPU 本身可以多线程，限制来自三端**渲染上下文亲和性**（API 须从拥有上下文的线程调用，且各端未暴露底层 GPU 细节）：

- sdl3：`SDL_render.h:46` 明文 "must be called from the main thread"；纹理绑定创建它的 renderer，不可跨 renderer
- sfml：GL context 线程亲和，通过 `setActive` 切换（`sfml/RenderDevice.cpp:144-145`）
- raylib：rlgl 全局单 GL 状态，无 context 句柄/共享机制

| 方案 | 后端 | 做法 | 成本/风险 |
|---|---|---|---|
| A. 两阶段：CPU 后台合成 + 主线程上传 | 三端统一 | 后台算好全部帧 Surface，主线程逐纹理 createTexture（sdl3/sfml 的 rotate 改 CPU） | 低；60 帧 63MB 上传仍有一次主线程卡顿 |
| B. 分帧批上传（叠加于 A） | 三端统一 | 首次播放时每帧只上传 2-4 个小纹理，卡顿摊薄到多帧 | 低；需 prepare→增量 ready 状态机 |
| C. 后台共享 GL context（wglShareLists） | 仅 sfml | 后台线程建共享 context，纹理后台生成 | 中；win32 平台代码 + sfml 版本耦合 |
| D. 后端重写为 SDL_GPU | 仅 sdl3 | 命令缓冲可从任意线程提交（`SDL_SubmitGPUCommandBuffer`），真·异步 | 高；重写整个 sdl3 渲染层，架构级改造（`SDL_gpu.h` 已在子模块） |

**推荐**：近期 A+B 组合（三端统一，B 消除上传卡顿）；远期 D（唯一让 GPU 侧真异步的路径，顺带解决 rotate 往返）；C 仅当 sfml 成为主后端时考虑。

---

## 5. 实施顺序建议（含依赖）

1. 第 7 项：**不动作**（已是现状）
2. 第 3 项：最小、最独立，可立即做
3. 第 4 项：常规新控件
4. 第 2 项：SHAPE/TEXT/SVG 落 CPU 层 → 为第 1 项铺路
5. 第 1 项：CPU 合成后台 + 主线程增量上传（A+B）
6. 第 5 项、第 6 项：跨后端/跨平台，放最后，可各拆两期

---

## 6. 待拍板决策点

1. 第 1 项是否接受"rotate 改 CPU"的取舍（换取整体可后台化）？——建议接受
2. 第 2 项 sfml 文字图层：接受"主线程烘焙文字"限制，还是引入软件字体栅格化依赖（stb_truetype，需评估体积）？
3. 第 5 项 raylib 桌面触控缺口：接受"能力位声明 + 调用方条件化"方案？
4. 第 6 项全局热键：是否接受仅 Windows 平台实现（win32 子类化），其他平台暂缓？
5. 多线程方案：确认近期 A+B、远期 D 的路线？
