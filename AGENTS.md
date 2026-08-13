# AGENTS.md - UICornerstone

## 交互语言
始终使用中文进行交互。

## Compulsory Reading Before Each Session

### [编码规范 →](design/guidelines/coding.md)
编码前思考、简洁优先、精准修改、目标驱动执行。**每次编码前必须通读。**

### [文件编码](design/guidelines/coding.md)
所有源代码文件（.h/.cpp）必须保存为 **UTF-8 with BOM** 编码格式。不要使用 UTF-8 without BOM 或其他编码。

### [临时文件](design/guidelines/coding.md)
测试临时文件与中间产物必须放入 **`Temp/`** 目录（`.gitignore` 已忽略，不会入库）。**禁止直接在项目根目录或源码目录下创建临时文件。**

### [设计规则 →](design/guidelines/design-rules.md)
位置数据存储规则、魔鬼数字规范。

### [设计文档规范 →](design/guidelines/design-docs.md)
架构决策原则、设计文档编写规范。

## Reference (Read as Needed)

| Topic | Document |
|-------|----------|
| 构建与测试 | [build.md](design/guidelines/build.md) |
| RGBA8888 像素格式（关键） | [pixel-format.md](design/guidelines/pixel-format.md) |
| 子模块依赖 | [dependencies.md](design/guidelines/dependencies.md) |
| Session 历史记录 | [history.md](design/guidelines/history.md) |
| 测试用例规范 | [testing.md](design/guidelines/testing.md) |
| C ABI 属性系统 | [CABI_Property_Design.md](design/CABI_Property_Design.md) |

## Quick Links

- [开发指南索引](design/guidelines/README.md) — 全部规范文档目录
