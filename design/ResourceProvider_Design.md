# ResourceProvider 设计文档（内存加载 + JSON 配置）

> 对应 Phase 16n | 编制 2026-08-14 | 状态: **设计中（待审核）**

## 目录

1. [动机](#1-动机)
2. [现状盘点：内存加载能力矩阵](#2-现状盘点内存加载能力矩阵)
3. [设计一：内存资源注册表（MemoryResourceProvider + C ABI）](#3-设计一内存资源注册表memoryresourceprovider--c-abi)
4. [设计二：JSON 配置 resourceProviders 与控件内引用](#4-设计二json-配置-resourceproviders-与控件内引用)
5. [自动化测试设计](#5-自动化测试设计)
6. [实施清单](#6-实施清单)
7. [兼容性与风险](#7-兼容性与风险)

---

## 1. 动机

当前 `ResourceProvider` 仅有文件系统实现（`createFilesystem(basePath)`），资源一律按文件路径读取。存在两个诉求：

1. **内存加载**：游戏客户端等场景中资源已打包进内存（或自建包体格式），不能依赖磁盘路径；需支持在 UICornerstone 初始化之前把资源字节载入内存，并经 C ABI 交给引擎使用。
2. **JSON 集中配置**：资源（图片、字体、动画 JSON）应在布局 JSON 中一次性声明挂载，并在各控件 JSON 中按名字引用，避免散落的绝对路径。

---

## 2. 现状盘点：内存加载能力矩阵

| 资源 | 文件加载 | 内存加载能力 | 说明 |
|------|----------|--------------|------|
| 布局 JSON | ✅ `LoadLayoutFromFile` | ✅ | `LoadLayoutFromFile` 已经 `provider->readFile`（src/UICornerstoneAPI.cpp:944-951）；`UICornerstone_LoadLayout(jsonContent)` 直接收内存字符串 |
| 图片 | ✅ `Actor::loadFromFile`（fopen） | ✅ | `Actor::loadFromResource` → `provider->readFile` → `Surface::loadFromMemory`（src/Actor.cpp:101-117）；三后端均已注册内存工厂。**SVG 矩阵见 §5.3** |

### 2.1 后端差异：SVG 支持矩阵

| 加载途径 | SDL3（SDL_image 3.2.4） | SFML | Raylib |
|----------|-------------------------|------|--------|
| 文件路径 | ✅（内置 SVG 解码器，`IMG_Load`，subModules/SDL3_image/src/IMG.c:66） | ❌（`sf::Image(path)` 原生不支持） | ❌（`LoadImage` 按扩展名分派，无 SVG） |
| 内存路径 | ✅（`IMG_Load_IO` 共用解码器表） | ✅（SVG 签名嗅探 + nanosvg，src/backend/sfml/RenderDevice.cpp:194） | ✅（同款嗅探 + nanosvg，src/backend/raylib/RenderDevice.cpp:166） |

> **结论**：内存路径是三后端唯一一致的 SVG 途径；文件路径仅 SDL3 支持。内存加载测试选用 SVG 反而跨后端一致（见 §5.3）。
| 字体 | ✅ `loadFont` | ✅ | `Label::loadFromResource` → `provider->readFile` 得 `m_fontData` → `renderer->loadFontFromMemoryWithText`（src/Label.cpp:287-309；src/TextRenderer.h:15） |
| 动画描述 JSON | ✅ `loadAniDesc`（fopen） | ❌ **缺口** | `LuotiAni::loadFromResource` 直接 `fopen(filePath)` 读文件（src/Luotiani.cpp:143-160），未走 provider |
| JSON 布局中 `"image"` 属性 | ✅ `loadFromFile`（fopen） | ❌ **缺口** | 布局 JSON 中“ `"image"` 属性经 `UICornerstone_SetString` → `loadFromFile` 直接读文件（src/UICornerstoneAPI.cpp:1192） |
| ResourceProvider 实现 | 仅文件系统 | ❌ **缺口** | 无内存实现；C ABI 仅有 create/destroy/readFile/fileExists 四函数（include/UICornerstoneAPI.h:185-189） |

**结论**：控件侧的"资源 ID + readFile + 内存解码"管线已基本完备（图片/字体已可用），缺的只有三块——① 内存版 ResourceProvider 及其 C ABI；② 动画 JSON 与布局 `"image"` 属性改走 provider；③ JSON 挂载点与名字引用。工作量集中在 API 层与 LuotiAni 一处。

---

## 3. 设计一：内存资源注册表（MemoryResourceProvider + C ABI）

### 3.1 核心思路

新增 `MemoryResourceProvider`：内部维护 `name → bytes` 注册表，`readFile(name)` 从注册表命中返回。控件侧**零感知**——所有现有 `loadFromResource` 管线（图片/字体）保持不动，`readFile` 语义天然统一（"资源 ID"不再限于磁盘路径）。

### 3.2 新增 C ABI（追加进 API 函数表，保持既有声明顺序）

```c
UIResourceProviderHandle (*createMemoryResourceProvider)(void);
// 注册 name → 内存块（默认拷贝模式：内部复制一份字节，调用方可立即释放 data）。
int  (*memoryProviderRegister)(UIResourceProviderHandle h,
                               const char* name, const void* data, int len);
// 零拷贝注册（adopt 模式）：转移 data 所有权给引擎，引擎不再拷贝；
// 调用方此后不得再访问/释放 data；引擎析构或覆盖同名条目时经 freeFn 释放
// （freeFn 由调用方提供，保证跨 DLL 堆由原分配者释放，可为 NULL → 走引擎默认 free）。
int  (*memoryProviderAdopt)(UIResourceProviderHandle h,
                            const char* name, const void* data, int len,
                            void (*freeFn)(void*));
// 将指定 provider 挂到实例：替换 UIContext::resourceProvider（取 ControlBase 级联传播，
// 见 src/ControlBase.cpp:593-598；UIContext.cpp:80 从 mainWindow 继承）。
int  (*setResourceProvider)(UIInstance inst, UIResourceProviderHandle h);
```

调用时机（用户要求）：**`UICornerstone_Initialize` 之前**，先把资源文件读入堆内存 → `createMemoryResourceProvider` → 逐个 `memoryProviderRegister` / `memoryProviderAdopt` → 初始化实例后 `setResourceProvider`，布局即可按名字引用。

### 3.3 内存占用与零拷贝

资源在引擎中存在**两层拷贝**，需分别对待：

| 拷贝层 | 位置 | 能否消除 |
|--------|------|----------|
| ① provider 注册拷贝 | `memoryProviderRegister` 内部复制字节 | ✅ 可消除（adopt 模式） |
| ② 解码器拷贝 | `Surface::loadFromMemory` / `loadFontFromMemory` 内部（纹理上传、字形图集烘焙） | ❌ 由后端解码 API 决定，不可避免 |

即：adopt 模式最多消除第 ① 层（小资源约几十 KB 级），像素级资源在第 ② 层仍有纹理拷贝——**adopt 是内存优化手段而非全部**。

**注册模式选择建议**：

- **默认 `Register`（拷贝）**：动画 JSON、字体等小资源；代码路径简单、无生命周期契约。
- **`Adopt`（零拷贝）**：大纹理等体积敏感资源；调用方须承诺放弃 buffer（转移所有权），并配套 `freeFn` 保证跨 DLL 释放归属。
- **JSON 挂载（path 型）**：引擎懒加载后字节归引擎自有，天然无第 ① 层拷贝，不涉及本设计。

`readFile` 命中 adopt 条目时直接 move 进 `shared_ptr<vector<char>>` 返回（不复制）；命中 register 条目时拷贝返回。同名重复 adopt → 先 `freeFn` 释放旧块再接管新块。

### 3.4 失败语义

- `readFile` 未命中注册表 → 返回空（与现有文件系统 provider 找不到文件一致，控件层已打印 `'%s' not found` 并跳过，不崩溃）。
- 同名重复 `memoryProviderRegister` / `memoryProviderAdopt` → 覆盖旧条目（后注册优先；adopt 覆盖时先经 `freeFn` 释放旧块）。

### 3.5 缺口修复（控件侧，两处）

1. `LuotiAni::loadFromResource`：`fopen` 改为 `provider->readFile(resourceId)` 后走既有 `parseJsonDesc()`（src/Luotiani.cpp:143-160 改为复用 `m_pJsonFileContent` 内存构造路径；`loadFromFile` 保持文件路径不变）。
2. 布局 JSON 中“ `"image"`（及三态图片、`"font"` 等字符串资源属性）：`SetString` 分支中把 `"provider:xxx"` 前缀的字符串路由到 `loadFromResource`（而非 `loadFromFile`），见 §4.2 语法。

---

## 4. 设计二：JSON 配置 resourceProviders 与控件内引用

### 4.1 布局 JSON 顶层挂载点

`resourceProviders` 数组，作为布局的**第一个顶层键**（与 `defines`/`controls` 同级）；加载布局时遍历注册到实例 provider：

```jsonc
{
    "resourceProviders": [
        { "name": "up-image",            "path": "images/cross_up.png" },
        { "name": "maple-mono-regular-font", "path": "fonts/MapleMono-NF-CN-Regular.ttf" },
        { "name": "heart-ani",           "path": "animations/heart.jsonc" }
    ],
    "controls": [ /* ... 控件数组，见 4.2 ... */ ]
}
```

实现：`LoadLayout` 解析入口处（src/UICornerstoneAPI.cpp:872）先扫描顶层 `resourceProviders`，确认实例 provider 为内存型（或可写型）后，把 `name → path` 加入查找表；**读取采用懒加载 + 缓存**：首次被控件引用时 `provider->readFile(path)` 读入并缓存字节（内存 provider 命中 name 时直接命中，无需二次读盘）。

> 说明：`createFilesystem` 返回的 provider 只读无注册能力——布局里的 `path` 型条目由引擎懒加载缓存即可，不要求 provider 可写；`name` 与内存注册表同名时**内存注册表优先**（见 4.3）。

### 4.2 控件内引用语法（推荐与兼容两种写法）

**写法 A（推荐）——字符串内联前缀**，适用于一切字符串资源属性（`image`/三态图片/`font`/`animation`……）：

```jsonc
{ "type": "image-button", "id": "img1", "rect": { "x": 580, "y": 80, "w": 240, "h": 176 },
  "normal-image":  "provider:up-image",           // → loadFromResource("up-image")
  "hover-image":   "assets/images/down_hover.png", // 无前缀 → 维持文件路径语义
  "pressed-image": "assets/images/down_pressed.png" }
```

**写法 B（兼容用户示例）——`provider-name` 对象形式**，用于 `actors` 的分态缺省图（`actors.normal/hover/pressed`）：

```jsonc
{ "type": "image-button", "id": "img1x", "rect": { "x": 580, "y": 80, "w": 240, "h": 176 },
  "actors": {
      "normal":  { "provider-name": "up-image" },
      "hover":   "assets/images/down_hover.png",
      "pressed": "assets/images/down_pressed.png"
  } }
```

`actors` 解析器对"对象值"做归一化：读 `provider-name` 键 → 等价于 `"provider:<name>"`；字符串值维持现状（文件路径）。

统一规则：`SetString` 属性赋值时，值以 `"provider:"` 前缀开始 → 剥前缀后调 `loadFromResource(id)`；否则 `loadFromFile(path)`。前缀与名字之间不留空格。

### 4.3 作用域与优先级

1. `SetString("...", "provider:xxx")` 或 `actors.provider-name` → 内存注册表 → 未命中再查布局 `resourceProviders` 懒加载缓存 → 仍未命中则报 `not found` 并跳过（不崩溃）。
2. 未命中路径**不回退**到 `loadFromFile`——`provider:` 前缀是显式契约，避免静默落到磁盘。
3. `resourceProviders` 条目在 `LoadLayout` 时注册到实例级 provider，多视口共享；控件级 `setResourceProvider` 传播语义不变（ControlBase.cpp:593）。

---

## 5. 自动化测试设计

### 5.1 主测试 `test/test_resourceprovider_memory.cpp`（或 `_cabi` 版，SDK 构建）

流程严格按用户要求——**资源读盘发生在引擎初始化之前，之后引擎全程不碰磁盘**：

```
main()
 ├─ 读入 3 个资源文件到堆内存（图片 png / 字体 ttf / 动画描述 jsonc）
 ├─ createMemoryResourceProvider() → memoryProviderRegister(name, data, len) ×3
 ├─ CreateInstance（resourceRoot 故意指向不存在的目录，
 │    并用 createFilesystem("") 替换为文件系统 provider 时也读不到——
 │    以证明全部资源来自内存注册表）
 ├─ setResourceProvider(inst, h)
 ├─ LoadLayout(jsonContent)            // 布局同样来自内存字符串
 ├─ FindControl 断言：ImageButton/Image/Label/Animation 全部创建成功
 ├─ 控件属性断言：图片纹理尺寸、字体渲染成功、动画帧数>0、播放 tick
 ├─ 负用例：引用未注册名 → 创建成功但资源保持空（不崩溃）；日志含 not found
 └─ DestroyInstance / destroyResourceProvider
```

### 5.2 断言要点

| 资源 | 内存加载证明 |
|------|--------------|
| 图片 | `UICornerstone_GetRect` / 纹理 w/h 与源文件一致；`resourceRoot` 不存在也能成功 |
| 字体 | Label 渲染宽度 > 0（`UICornerstone_GetString` 或文本测量非零） |
| 动画 JSON | `UICornerstone_GetInt("frame-count")` > 0；两帧 AABB 与 jsonc 一致（覆盖 LuotiAni 内存路径修复） |
| 布局 JSON | `LoadLayout` 返回非 0、控件可 FindControl |

### 5.3 SVG 内存用例（跨后端一致）

按 §2.1 矩阵，三后端内存路径均支持 SVG：在用例中额外注册一个内存 SVG（如纯色圆），断言纹理尺寸与 SVG 视口一致。注意**不得**用文件路径加载 SVG 做跨后端断言（仅 SDL3 支持文件路径）。

### 5.4 失败路径用例

- `provider:not-exists` → 资源空、控件存活、日志含 `not found`；
- `memoryProviderRegister(h, "", ...)` 空名 → 拒绝注册（返回 0）；
- **adopt 释放契约**：adopt 后调用方释放原 buffer、正常渲染（零拷贝生效）；provider 析构时 `freeFn` 恰被调用一次（用计数断言）；adopt 覆盖同名条目时旧块先经 `freeFn` 释放。

---

## 6. 实施清单

| # | 内容 | 文件 |
|---|------|------|
| 1 | `MemoryResourceProvider` 类（注册表 + readFile/exists 命中 + 懒加载缓存表；Register 拷贝 / Adopt 零拷贝两种条目） | `src/ResourceProvider.cpp`、`include/ResourceProvider.h` |
| 2 | C ABI 四函数 + bridge 实现（createMemory/register/adopt/set） | `src/backend/BackendBridge.h`、三后端 `BackendPlugin.cpp`、`include/UICornerstoneAPI.h` |
| 3 | `LuotiAni::loadFromResource` 改走 provider（修 fopen 缺口） | `src/Luotiani.cpp` |
| 4 | `SetString` 属性值 `provider:` 前缀路由 + `actors` 对象式 `provider-name` 归一化 | `src/UICornerstoneAPI.cpp`、布局 actors 解析 |
| 5 | `LoadLayout` 顶层 `resourceProviders` 扫描 + 懒加载缓存 | `src/UICornerstoneAPI.cpp`（LoadLayout 入口） |
| 6 | 自动化测试 | `test/test_resourceprovider_memory.cpp` + CMake 注册 |
| 7 | 文档：控件 JSON 引用语法、布局键说明 | `docs/`（引用章节，随实施） |

## 7. 兼容性与风险

- **向后兼容**：新增 API 追加到函数表尾部（函数表版本号 +1），既有 `createFilesystem` 语义不变；`loadFromFile` 全部保留。
- **风险 1**：`provider:` 前缀与真实文件名冲突（极小概率）——约定文档声明该前缀为保留字。
- **风险 2**：`actors` 对象式值会影响现有字符串解析分支——归一化只增加"对象 → 取 provider-name"一路，字符串分支不动。
- **风险 3**：内存占用与生命周期分两种模式——默认 `Register` 拷贝持有字节（调用方可释放原 buffer，跨 DLL 安全，代价双份内存）；`Adopt` 零拷贝转移所有权（省第 ① 层拷贝，但调用方须放弃 buffer，且跨 DLL 必须经 `freeFn` 由原分配者释放，不存裸指针）。解码层第 ② 层拷贝（纹理上传/字形图集）由后端 API 决定，adopt 无法消除，属预期。