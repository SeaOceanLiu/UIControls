# 增强需求分析（8 项 × 三后端）

> 状态：分析中 · 依据：2026-08 全仓库勘察（含子模块） · 用途：持续思考的结论依据
> 相关文档：[LuotiAni_Design.md](LuotiAni_Design.md)、[BackendAbstraction_Design.md](BackendAbstraction_Design.md)、[GraphTool_Design.md](GraphTool_Design.md)

## 1. 背景与范围

以下 8 项增强需求，逐一给出"现状 → 三后端（SDL3 / SFML / Raylib）支持度 → 实现要点 → 取舍点"：

1. 起线程做 LuotiAni 烘焙，避免影响主程序加载
2. LuotiAni 支持基本 Shape、text、线宽，最好将 SVG 能力整体迁入
3. Button 支持圆角矩形、圆形
4. 支持 RadioButton、状态栏
5. 支持触控滚动
6. 快捷键系统——菜单快捷键绑定 / 全局快捷键
7. JSON 解析是否放到窗体初始化完成后（以便有 renderer、叠加多线程烘焙提速）
8. 跨平台移植（Linux / Android）——SDL3 能否"不编译源码"方式运行（预编译 vs 源码编译路线）

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
| 8 | 跨平台移植（Linux/Android） | ⚠️ 需改造 | ❌ Android 无移植 | ⚠️ 需改造 | SDL3/raylib 官方支持 Android（可编 .so 复用）；SFML 无官方 Android 移植（排除）；Linux 仅 SDL3 有发行版预编译 |

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

**平台无关性**（与 §7 呼应）：上述"渲染线程约束"（SDL renderer / GL setActive / raylib rlgl）在 Linux、Android 上同样成立——后台化方案不依赖宿主平台，跨平台移植时无需重做。

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

两控件均不存在；基于既有 Panel/Label/CheckBox 模式 + 第 3 项圆形能力即可组合实现 → **三端无差异**。需注册进 PropertyNames + LayoutParser 控件分发（现有 22 种控件，`src/LayoutParser.cpp:254-312`）。

### 3.5 触控滚动

**现状**：Event 层有 Finger 类型（`include/EventTypes.h:13`，兼容遗留）；三端 InputBackend 均未接线 touch：
- sdl3：`SDL_EVENT_FINGER_*` 已定义（`SDL_events.h:212-215`），后端 switch（`sdl3/InputBackend.cpp:286-367`）无分支
- sfml：`sf::Event::TouchBegan/Moved/Ended` 已定义（`Event.hpp:266-290`），后端零匹配
- raylib：`GetTouch*/GetGesture*` 已定义（`raylib.h:1238-1254`），后端零匹配

**能力不对称**：sdl3/sfml 桌面 Windows 触屏可产生触摸事件；**raylib 桌面端触摸 API 恒无效**（仅 Android/iOS/Web）。

**跨平台视角**（与 §7 呼应）：触控滚动不仅是桌面增强，更是 Android 移植（三后端均以触摸为主力输入）的前置条件——raylib 在桌面端的能力缺口通过能力位声明解决，在 Android 端则自然可用。

**建议**：三端统一实现事件转换；raylib 侧利用既有"后端能力位机制"（`UICornerstone_GetBackendCapabilities`）声明触摸能力，调用方条件化。

### 3.6 快捷键系统

**现状**：`MenuItem::m_shortcut` 仅为显示文本（`src/Menu.cpp:146-153` 右对齐绘制），无键盘触发、无 mnemonic、无系统热键。

**窗口内快捷键**：三端键盘 + KeyMod 完整（sdl3 `SDLKeymodToKeyMod` `:150-153`、raylib 逐位拼装 `:234-248`、sfml bool 拼装 `:92-104`）→ **三端全支持**。

**系统级全局热键**：**三端均无原生通路**：
- sdl3：子模块快照无 `SDL_RegisterHotkey`（无 SDL_hotkeys.h）
- raylib：公共 API 无 WndProc 钩子（仅 TraceLog/文件 IO 回调类）
- 原生 HWND 均可得：sdl3 `SDL_PROP_WINDOW_WIN32_HWND_POINTER`、sfml `getNativeHandle()`（已有 GetDpiForWindow 先例，`sfml/Window.cpp:47`）、raylib `GetWindowHandle()`（`raylib.h:1014`，当前后端返回 nullptr 需补）

**方案**：统一走 win32 窗口子类化（`SetWindowLongPtr`）收 `WM_HOTKEY`——平台适配层，与后端正交，三端对称；Linux/移动端暂缓（见 §7 跨平台可行性）。

### 3.7 JSON 解析时机

**结论：已是现状，无需改动。** 勘察确认：
- C ABI 路径：`UICornerstone_CreateInstance` 先建窗体+设备并 dummy present 激活 GL（`src/UICornerstoneAPI.cpp:296-358`），`UICornerstone_LoadLayout`（:893-905）在后
- 旧式路径：解析发生在 `onInit`（窗口已建）
- 纹理延迟到挂树后、设备下发时创建（`src/ControlBase.cpp:344-363`）

真正的慢点是第 3.1 项同步 prepare + GPU 往返旋转，与本项无关。

### 3.8 跨平台移植（Linux / Android）

**现状**：项目 Windows-first——预编译库仅 `subModules/libs/` 的 Windows x64；`subModules/SDL3/` 为裁剪头文件（85 个，无 src/、无 Android 头）；Win32 特有代码未条件化（sfml `GetDpiForWindow` `sfml/Window.cpp:47`、sdl3 HWND 属性、raylib 桌面输入）。

**三条路线**（详细结论见 §7）：
- Linux：SDL3 用发行版预编译包（apt `libsdl3-dev` 等，含 SDL3_image/ttf）✅——需系统头文件替换裁剪头文件、与项目 API 版本对齐
- Android：官方无预编译 .so，唯一可靠路径为 NDK 交叉编译（项目本体 + SDLActivity Java 壳 + JNI）⚠️
- Raylib：单文件源码分发，各端均源码编译，体量小、无负担

**与其它增强的耦合**：触控滚动（3.5）是 Android 移植前置（触摸为 Android 主力输入）；全局热键（3.6）仅 Windows 实现；多线程烘焙（3.1）平台无关，移植时无需重做。

**取舍**：确认 Linux 走发行版包还是源码子模块（版本可控性 vs 零系统依赖）；Android 是否排入规划。

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
6. 跨平台（§7）：确认 Linux 走发行版预编译包 vs 源码子模块的路线；Android NDK 移植是否排入规划？

---

## 7. 跨平台可行性（预编译 vs 源码编译）

> 问题："SDL3 能否不使用源码编译方式跑在 Android / Linux 上？"

### 7.1 现状：Windows 上本就是"预编译"模式

本项目 sdl3 后端**不编译 SDL3 源码**：`subModules/libs/` 提供预编译 `SDL3.lib/SDL3.dll`、`SDL3_image`、`SDL3_ttf`（Windows x64），`CMakeLists.txt` 直接链接。"不编译源码"在 Windows 上已成立。

### 7.2 三平台对比

| 平台 | "预编译 SDL3"形态 | 可行性 | 主要风险 |
|---|---|---|---|
| Windows | 子模块裁剪头文件 + `subModules/libs` .lib/.dll | ✅ 已成立 | — |
| Linux | 发行版包（apt `libsdl3-dev`、dnf `SDL3-devel`，含 SDL3_image/ttf） | ✅ 可行 | SDL3 版本与项目代码 API 版本错配（需系统头文件替换裁剪头文件） |
| Android | 官方不发布预编译 .so（仅源码 + Gradle 模板） | ⚠️ 高风险 | 社区包版本/ABI（arm64-v8a 等）对齐难 |

### 7.3 关键事实

- **SDL3 头文件快照是裁剪版**：`subModules/SDL3/` 仅 85 个头文件，无 src/、无 CMakeLists、无 `SDL_android.h`，`SDL_platform.h` 无 ANDROID 宏——第三方 .so 需先补头文件并精确对齐版本
- **SDL3 / SDL3_image / SDL3_ttf 源码均可在 GitHub 官方仓库获取**（`libsdl-org/SDL`、`libsdl-org/SDL_image`、`libsdl-org/SDL_ttf`，按 release tag 可精确对齐版本）——"外部拉源码"障碍可消除，但需将子模块结构从"裁剪头+预编译"切换为"完整源码+NDK 构建"
- **"不编译源码"仅对 SDL3 依赖成立**，项目本体（UI 库 + C ABI + 三端后端）无论任何平台都需源码编译（Linux 走 CMake、Android 走 NDK 交叉编译）
- **Android 集成另需** `SDLActivity` Java 壳 + AndroidManifest + JNI；Android 上触摸是主力输入，与第 5 项增强（触控滚动）强耦合
- **Raylib 例外**：单文件源码分发（发行版包滞后少见），各端均走源码编译，但体量小、无负担
- **Win32 特有代码需条件编译**：sfml `GetDpiForWindow`（`src/backend/sfml/Window.cpp:47`）、sdl3 HWND 属性、raylib 桌面输入等

### 7.4 结论

- Linux：SDL3 可用发行版预编译包 ✅（版本对齐后）；项目本体 CMake 源码编译
- Android：无官方预编译，唯一可靠路径是 **NDK 自编全套 .so**（SDL3 + SDL3_image + SDL3_ttf + 项目 C ABI）→ "不编译源码"不成立
- 任何平台移植的共性前提：源码平台编译 + Win32 代码条件化 + 平台差异（触摸/热键）适配

### 7.5 Android 自编 libSDL3.so 的问题清单

**编译本身无问题**（官方构建指南支持 Android：NDK toolchain + `-DANDROID_ABI`），风险集中在配套链：

1. **子模块缺源码**：`subModules/SDL3/` 无 src/——需自 GitHub 拉取完整源码树，release tag 与头文件快照版本对齐（否则项目调用的 API 与 .so 不一致）；子模块结构需改为"完整源码 + NDK 构建"
2. **SDL3_image / SDL3_ttf 同源编译**：项目 sdl3 后端同时依赖二者（`IMG_Load`、`TTF_*`），需与 SDL3 同版本同源编译；ttf 依赖 freetype/harfbuzz、image 依赖 libpng/jpeg/webp 等第三方库（Android 上需一并交叉编译或手动提供）
3. **多 ABI + 打包 + 加载链**：arm64-v8a + x86_64（模拟器）至少两份 .so；`System.loadLibrary` 顺序（先 SDL3 后项目库，沿用 SDLActivity 模板）；GLES/vulkan renderer 差异需实测
4. **版本迭代维护**：SDL3 快速迭代期每次升级三库都要重编——"编一次存档复用"仅在锁定版本时成立
5. **"编一次复用"成立前提（锁三样）**：锁版本（release tag + 头文件与 .so 同版本）、锁 ABI（各架构 .so 独立归档）、锁 API 用法（项目只用当前版本已有 API）；日常开发直接取用归档 .so，与 Windows 上直接用 `subModules/libs/SDL3.dll` 同手感；编译参数与 NDK 版本需文档化

### 7.6 Raylib / SFML 的 Android 复用可行性

按"编一次 .so 复用"思路（同 §7.5），两后端结论截然相反：

| 后端 | Android .so | 锁版复用 | 备注 |
|---|---|---|---|
| SDL3 | ✅ 官方支持 | ✅（三库联动） | 需连带编 image/ttf，见 §7.5 |
| Raylib | ✅ 官方支持 | ✅（最省事） | 单库无依赖，API 最稳 |
| SFML | ❌ 无官方移植 | — | 社区 fork 止步 2.x，排除 |

**Raylib ✅**：
- 官方支持 Android（自带 `rcore_android.c` 平台层，直连 EGL/JNI，不依赖 GLFW）
- 单文件自包含，无 SDL3_image/ttf 式多库联动与 freetype/libpng 等第三方依赖——交叉编译链最简
- API 稳定度高（迭代慢、改动小）→ "锁版后永久复用"可靠性高于 SDL3
- 桌面与 Android 为同源码不同平台产物，分别编译即可；`GetTouch*` 在 Android 端真实可用，恰好补上 §3.5 桌面触控缺口

**SFML ❌**：
- 官方不支持 Android：SFML 3.x 仅桌面（Win32/X11/Wayland），无 Android 平台层，不存在官方 Android 版
- 社区 fork（如 sfml-mobile）止步 2.x 且长期不维护、基于旧 GLES，与项目所用 SFML 3.x API 脱节
- 结论：Android 上直接排除，"编一次"的对象不存在
