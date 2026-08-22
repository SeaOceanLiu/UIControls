// ============================================================================
// LayoutValidator.h -- 声明式 UI 布局 JSON 校验核心模块（可复用）
// 设计：design/JSON_Schema_Design.md §5.2
// 协议：MIT（tools/LICENSE）——仅依赖 nlohmann/json(MIT)，不链接引擎库
// PropertyNames.h 经构建期拷贝注入 generated/（决策 4.3），本模块只做文本解析
// ============================================================================
#pragma once

#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <vector>

namespace uic_tools {

enum class Level { Error, Warn, SchemaInvalid };

struct ValidationIssue {
    std::string jsonPointer;  // 如 /controls/2/fontSize
    Level        level;
    std::string  message;

    const char* levelString() const {
        switch (level) {
        case Level::Error:         return "ERROR";
        case Level::Warn:          return "WARN";
        case Level::SchemaInvalid: return "SCHEMA_INVALID";
        }
        return "?";
    }
};

class LayoutValidator {
public:
    using Keys   = std::set<std::string>;
    using Issues = std::vector<ValidationIssue>;

    /// 文本解析 PropertyNames.h，提取全量属性键集
    /// 匹配：PROP_CONSTEXPR const char* kXxx = "..."（宏未展开形态，捕获字面量）
    static Keys extractPropertyKeys(const std::string& headerPath, std::string& err);

    /// 加载 Schema 文件（nlohmann 解析，语法错误经 err 返回）
    static bool loadSchema(const std::string& schemaPath, nlohmann::json& out, std::string& err);

    /// Schema 合法性检查：
    /// ① 全树 properties 下出现的键必须已在 keys 注册（幽灵键 → SCHEMA_INVALID）
    /// ② $ref 目标 "#/$defs/NAME" 必须存在于 $defs
    static Issues validateSchema(const nlohmann::json& schema, const Keys& keys);

    /// 布局校验。strict=true 时未知键 warn 升级为 error（决策 4.6）。
    /// 动态语法节点（components/template/bind/{{}}）整节点跳过（决策 4.5）。
    static Issues validateLayout(const nlohmann::json& layout,
                                 const nlohmann::json& schema,
                                 const Keys&           keys,
                                 bool                  strict);

    /// 全局未知键判定（双层防御第一层）
    static bool isUnknownKey(const std::string& key, const Keys& keys);
};

} // namespace uic_tools
