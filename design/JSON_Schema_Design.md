# 声明式 UI JSON 语法限定表（Schema）设计

> 状态：**已拍板（决策点 1-7 全部通过，2026-08-19）**
> 关联：[LayoutSystem_Design.md](LayoutSystem_Design.md)（JSON 布局系统）、[CABI_Property_Design.md](CABI_Property_Design.md)（属性键体系）、`include/PropertyNames.h`（**键名唯一来源**）、各控件 `*_Design.md`/`*_Analysis.md`（JSON 语法散点）
> 消费方：① 自动化 JSON 语法检查（校验器）② CornerstoneCreator（后续 JSON 生成器）

## 1. 需求概述

建立**声明式 UI JSON 的机器可读语法限定表（Schema）**——单一权威规格，供：

1. **自动化语法检查**：对 JSON 布局文件做严格预检（未知属性键/类型错误/枚举非法/缺失必选/颜色与 rect 格式），在运行前/测试/CI 中发现问题
2. **CornerstoneCreator JSON 生成**（后续工具）：按同一规格生成合法 JSON（属性名/类型/枚举/默认值/示例自动正确）

**与现有体系的关系**：LayoutParser 保持**运行时宽容**（未知键忽略、非法值取默认 + `logWarn`）；Schema 是**开发期严格规格**（校验器负责，不改运行时行为）。

## 2. 现状调研（关键行号）

| 项目 | 现状 | 依据 |
|---|---|---|
| 解析入口 | `parseControl` 按 `"type"` 分发（未知 type `logWarn` 跳过），`LayoutParser.cpp:246/311` | 类型错误已检 |
| 属性读取 | 全部 `value(key, default)` 静默默认——**未知键/错误类型无警告** | 各 parseXxx |
| 错误报告 | `logError`/`logWarn`（JSON 语法错误 :32、rect 缺失 :2512/2552、颜色格式 :2580/2602、对齐枚举 :2682、字体名 :2662） | 已有部分校验 |
| 键的权威 | `PropertyNames.h`（755 行 `inline constexpr const char*`，C/C++ 双用）——**仅键名，无类型/枚举/默认值/控件归属** | — |
| 公共属性 | `parseCommonProperties`（`LayoutParser.cpp:2084`，21 处调用）——visible/enabled/transparent/背景/边框等 | 公共属性组已存在 |
| 文档 | 用户手册 `docs/index.html`（每控件 JSON 语法章节）+ 各控件设计文档 + 速查表——人类可读、分散 | 无机器可读源 |

## 3. 方案设计

### 3.1 规格载体：标准 JSON Schema（draft 2020-12）+ 项目扩展标注

单文件 `docs/schema/declarative-ui.schema.json`：

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://uicornerstone.dev/schema/declarative-ui.schema.json",
  "title": "UICornerstone Declarative UI",
  "oneOf": [ { "$ref": "#/$defs/label" }, { "$ref": "#/$defs/button" }, /* ...全部控件 */ ],
  "$defs": {
    "rect": {
      "type": "object",
      "properties": {
        "x": { "type": "integer", "description": "left" },
        "y": { "type": "integer" },
        "w": { "type": "integer" },
        "h": { "type": "integer" }
      },
      "required": ["x", "y", "w", "h"]
    },
    "color": { "type": "string", "pattern": "^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$" },
    "common": {
      "type": "object",
      "properties": {
        "id":        { "type": "string",  "x-default": "" },
        "visible":   { "type": "boolean", "x-default": "true" },
        "enabled":   { "type": "boolean", "x-default": "true" },
        "transparent": { "type": "boolean", "x-default": "false" },
        "background": { "$ref": "#/$defs/color", "x-default": "透明" },
        "border":      { "$ref": "#/$defs/color" },
        "borderVisible": { "type": "boolean" },
        "text":        { "$ref": "#/$defs/color" },
        "textShadow":  { "$ref": "#/$defs/color" }
      }
    },
    "label": {
      "type": "object",
      "allOf": [ { "$ref": "#/$defs/common" } ],
      "properties": {
        "type":  { "const": "label" },
        "rect":  { "$ref": "#/$defs/rect", "x-required": "必须（可缺省，缺省全 0）" },
        "text":  { "type": "string", "description": "显示文本" },
        "fontSize": { "type": "integer", "x-default": "14" },
        "fontName": { "type": "string" },
        "align": { "enum": ["top-left", "top-center", "top-right", "center-left", "center", "center-right", "bottom-left", "bottom-center", "bottom-right"] }
      },
      "required": ["type"],
      "additionalProperties": false
    }
  }
}
```

**关键决策**：
- `additionalProperties: false` + 组合引用——校验器可报**未知属性键**（LayoutParser 现不查，Schema 补上）
- 类型表达：标准 JSON Schema（string/integer/number/boolean/array/object）+ 项目语义类型用 `$defs` 组合（`color` 正则、`rect` 结构、枚举 `enum`）
- **`x-default` 扩展标注**：默认值（供生成器/补全；Schema 标准无默认值字段语义——`default` 仅注释性质，`x-` 前缀避免误用）
- **`x-required` 扩展标注**：语义必选说明（如 rect 缺省全 0 的边界）

### 3.2 与 PropertyNames.h 的同步机制（决策点 3：PropertyNames.h 为键名单源）

- **键名唯一来源 = PropertyNames.h**（现有约定不变）；Schema = 语义层（控件 × 键归属/类型/枚举/默认值/必选/描述）
- **校验器运行时强制一致性**：读取 PropertyNames.h 全量键集 → 全局未知键检查 + Schema 引用键必须已注册（Schema 非法退出码 2）——无生成链、无额外文件
- **新增属性流程**（固定三处）：PropertyNames.h（键名）→ Schema（语义）→ 校验器自动验证

### 3.3 消费方 ①：校验器（一期，C++ 实现，决策点 2）

`tools/validate_layout.cpp`（仅依赖既有 nlohmann/json 单头）+ 可复用模块 `tools/LayoutValidator.h/.cpp`：
- 用法：`validate_layout <layout.json...> [--schema docs/schema/declarative-ui.schema.json] [--property-names include/PropertyNames.h] [--strict]`
- 校验项：JSON 语法 / type 枚举 / 控件级未知键（additionalProperties）/ 全局未知键（PropertyNames.h 键集）/ 类型错误 / 枚举非法 / rect 缺失字段 / 颜色格式 / 必选缺失 / **Schema 合法性**（引用键未注册 → 退出码 2）
- 输出：`<文件>: <JSON Pointer 路径> <级别> <信息>`（如 `hello.json: controls[2].fontSize: ERROR 期望 integer, 实际 string`）+ 退出码（0=通过，1=有错，2=Schema 非法）；`--strict` 将未知键 warn 升级为 error
- **Schema 子集**：type/required/enum/pattern/properties/additionalProperties/oneOf/$ref/allOf/const（不实现 format/if-then/循环引用）
- 集成：CMake 目标 `validate_layout`；CI 对 `layouts/` 全部样本执行 `--strict` 断言退出码 0；新增控件验收时运行
- **Creator 复用**：LayoutValidator 头文件为 Creator 提供 `validateLayout(json, schema) / validateSchema(schema, propertyNamesKeys) / isUnknownKey(...)` API

### 3.4 消费方 ②：CornerstoneCreator（后续，接口约定，决策点 2）

- Creator（**C++ 实现，基于 UICornerstone C++ Binding**）直接**复用 LayoutValidator 模块**（`#include tools/LayoutValidator.h`）做生成结果自校验 + 用户编辑实时校验
- 消费同一 Schema：控件列表（`oneOf` 各 `$ref`）→ 模板生成；属性面板 → properties/type/enum/`x-default`/description 驱动
- **约定**：Creator 只读 Schema + PropertyNames.h 键集，不硬编码属性名——新增控件/属性只需更新 PropertyNames.h + Schema（+ 跑校验器），Creator 自动获得能力

### 3.5 与 LayoutParser 的关系（边界明确）

- LayoutParser：**运行时宽容不变**（未知键忽略、非法值默认 + `logWarn`）——不嵌入 Schema（嵌入成本高、运行时无收益）
- 校验器：**开发期严格**（CI/测试/工具调用）——发现问题在运行时之前
- 未来可选：`--strict-json` 调试开关让 LayoutParser 消费错误列表（列后续，需求出现再做）

## 4. 分期

| 阶段 | 内容 | 依赖 |
|---|---|---|
| 一期 | Schema（draft 2020-12 + `x-` 标注）+ **全部控件全量属性** + C++ 校验器（`tools/validate_layout` + `LayoutValidator` 模块）+ PropertyNames.h 键集一致性强制 + `layouts/` 样本 `--strict` 回归 + CI 集成 | 本设计拍板 |
| 二期 | `--strict` 细化（分级警告配置）；错误信息国际化？（否）；LayoutParser `--strict-json` 调试开关（可选，需求出现再做） | 一期 |
| 三期 | CornerstoneCreator 立项（复用 LayoutValidator + 消费 Schema 生成） | 二期 + Creator 立项 |

## 5. 决策点

### 决策点 1：规格载体（已拍板：标准 JSON Schema，标注遵循规范）

**拍板**：标准 JSON Schema（**遵循 draft 2020-12 规范**）——`$schema` 字段显式声明版本（`https://json-schema.org/draft/2020-12/schema`）；`$id` 声明唯一标识；类型表达全用标准关键字（`type`/`enum`/`pattern`/`required`/`additionalProperties`/`$ref`/`oneOf`/`allOf`），**不自定义校验语义关键字**；项目扩展仅用 **`x-` 前缀标注**（`x-default`/`x-required`，供生成器/补全，校验器不将其作为语义），并在 Schema 头部注释 + 本文档中声明遵循规范与扩展约定。
- 备选：自定义 JSON 定义——生态为零需自研解析器；纯 Markdown 表格——机器不可消费。均废弃。

### 决策点 2：校验器实现（已拍板：C++ 实现，Creator 同栈复用）

**拍板**：校验器用 **C++ 实现**（`tools/validate_layout.cpp`，仅依赖既有 nlohmann/json 单头）——**用户意见：CornerstoneCreator 为 C++（基于 UICornerstone C++ Binding），校验模块以 C++ 实现便于 Creator 直接复用/嵌入**（同栈、可链接、可嵌入）。
- 校验逻辑组织为**可复用模块**（`tools/LayoutValidator.h/.cpp` 独立编译单元）：可执行入口 + 头文件 API（Creator 后续 `#include` 直接调用，或调用可执行文件）
- 输出：JSON Pointer 风格错误定位（`controls[2].fontSize`——nlohmann 解析保留结构路径而非行号，与 LayoutParser 日志互补）+ 退出码（0=通过，1=有错，2=Schema 自身非法）
- 备选：Python 脚本——开发快但 Creator（C++）无法复用，语言栈分裂，废弃；第三方 C++ JSON Schema 库（如 valijson）——依赖管理成本，一期自研子集够用（决策点 3 的键一致性核对亦需定制逻辑）。

### 决策点 3：同步机制（已拍板：PropertyNames.h 为键名单源）

**拍板（用户意见：以 PropertyNames.h 为源）**：**键名唯一来源 = `PropertyNames.h`**（现有约定"所有 JSON 字段进 PropertyNames.h"不变），Schema 负责**语义层**（控件 × 键的归属、类型、枚举、默认值、必选、描述）。**一致性由校验器运行时强制**（无需生成链/生成文件）：
- 校验器加载布局文件时同时读取 `PropertyNames.h`（文本解析 `PROP_CONSTEXPR const char* kXxx = "..."` 提取全量键集）→ **全局未知键检查**（任何键不在 PropertyNames.h 键集 = 未知键，即使 Schema 未列——两层防御）
- **Schema 合法性检查**（校验器内建）：Schema 中出现的每个属性键必须已在 PropertyNames.h 注册，否则**Schema 非法**（退出码 2）——防止 Schema 引用未注册键
- **新增属性流程**（固定）：① 键名进 PropertyNames.h（唯一手写处）② 语义进 Schema ③ 校验器自动验证一致——三处缺一即测试/CI 失败
- 备选：双源并列 + 比对测试——键名手写两处仍会漂移（废弃）；PropertyNames.h 生成 Schema 骨架——控件归属信息不在 PropertyNames.h（仅键名无控件维度），生成器无法完成归属，仍须人工，废弃。

### 决策点 4：一期控件范围（已拍板：全部控件）

**拍板（用户意见：全部）**：一期覆盖**全部控件**（label/button/panel/.../Shape 新增在内）全量属性——按各控件设计文档 JSON 章节逐格核对录入 Schema；公共属性组集中定义（`$defs/common` 引用）；全部控件 `type` 枚举注册。工作量大，但机制一次成型，避免"部分控件不可校验"的断层。
- 分期仅保留：一期 = 机制 + 全部控件全量；二期 = 可选增强（`--strict` 细化、Creator 集成）；三期 = Creator 立项。

### 决策点 5：未知键处理（已拍板：默认 warn + `--strict` 报错）

**拍板**：校验器**默认 warn（退出码仍 0）+ `--strict` 报错（退出码 1）**——未知键通常是笔误，warn 不阻断现有布局文件；CI/测试用 `--strict` 强制。
- 全局未知键（不在 PropertyNames.h）与控件级未知键（不在该控件 properties）分级报告：控件级为高置信笔误（strict 必报）；全局级为高置信错误（strict 必报）。

### 决策点 6：LayoutParser 是否消费 Schema（已拍板：运行时不变）

**拍板**：**LayoutParser 运行时不变**（宽容 + logWarn 现状保持）；校验器独立负责开发期严格检查。`--strict-json` 调试开关列后续（需求出现再做）。

### 决策点 7：AGENTS.md 验收规则（已拍板：新增）

**拍板（用户指令）**：**AGENTS.md「新增控件验收清单」新增一条：`确认 JSON Schema 已刷新并验证完成`**——新增/修改控件属性时必须同步更新 `docs/schema/declarative-ui.schema.json` 并运行校验器（`validate_layout --strict` 对相关样本 + Schema 合法性检查）通过，否则视为验收不合格。同时适用于：新增 JSON 字段（键名进 PropertyNames.h + 语义进 Schema + 校验通过）。

## 6. 涉及文件清单

| 文件 | 改动 |
|---|---|
| `docs/schema/declarative-ui.schema.json`（新） | 语法限定表（权威规格，draft 2020-12 + `x-` 标注） |
| `tools/LayoutValidator.h/.cpp`（新） | 校验核心模块（可复用，Creator 消费） |
| `tools/validate_layout.cpp`（新） | 校验器可执行入口（CMake 目标） |
| `layouts/` 样本 | `--strict` 回归基线（现有样本应全部通过） |
| `design/JSON_Schema_Design.md`（本文档） | 拍板后刷新为已拍板状态 |
| 各控件设计文档 | 全部控件全量录入 Schema 的源（逐格核对 JSON 章节） |
| `AGENTS.md` | 新增验收规则：JSON Schema 刷新并验证完成（决策点 7） |

## 7. 现状限制（注明，后续增强）

- 校验器仅 Schema 子集（oneOf/$ref 嵌套深度受限，循环引用不做，format 不做）
- 键名一致性靠校验器运行时读取 PropertyNames.h 强制（无生成链）
- 不校验"语义正确性"（如事件引用存在性、组件名解析）——语法层仅
- CornerstoneCreator 为后续立项，一期仅约定接口（LayoutValidator 复用）