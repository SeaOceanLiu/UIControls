// ============================================================================
// LayoutValidator.cpp -- 校验核心实现（设计：design/JSON_Schema_Design.md §5.2）
// Schema 子集（10 关键字）：type/required/enum/const/pattern/properties/
//   additionalProperties/oneOf/allOf/$ref（限 $defs 内 + 自引用，深度上限 32）
// ============================================================================
#include "LayoutValidator.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace uic_tools {

using json = nlohmann::json;
// 类内嵌套类型的命名空间级别名（供本 TU 自由函数使用）
using Keys   = LayoutValidator::Keys;
using Issues = LayoutValidator::Issues;

namespace {

// 结构性容器键白名单：跨控件出现、内部结构由专用分支处理，不做控件级未知键检查
// （children 下钻控件、items/menus 走数据节点校验、events/bind/template/actors 动态或嵌套描述；
//   flowWeight/anchor/grid/row/col 等为布局子属性——写在任意子控件上、由父 panel 布局引擎解释）
const std::set<std::string>& structuralKeys() {
    static const std::set<std::string> kSet = {
        "children", "items", "menus", "events",
        "actors", "bind", "template", "luotiAni", "colors", "titleBar",
        "confirmButton", "cancelButton", "range", "presets",
        "flowWeight", "anchor", "anchorOffset", "grid",
        "row", "col", "rowSpan", "colSpan",
    };
    return kSet;
}

// 动态语法节点键（决策 4.5）：整节点跳过
bool isDynamicNodeKey(const std::string& key) {
    return key == "components" || key == "template" || key == "bind";
}

// 占位符值 {{xxx}} 通配合法（决策 4.5）
bool isPlaceholderValue(const json& v) {
    if (!v.is_string()) return false;
    const std::string s = v.get<std::string>();
    return s.size() >= 4 && s.rfind("{{", 0) == 0 && s.rfind("}}") == s.size() - 2;
}

int g_depth = 0;
constexpr int kMaxDepth = 32;
bool depthExceeded() { return g_depth >= kMaxDepth; }
struct DepthGuard {
    bool exceeded_;
    DepthGuard() : exceeded_(++g_depth > kMaxDepth) {}
    ~DepthGuard() { --g_depth; }
};

void addIssue(Issues& out, Level lv, const std::string& ptr, const std::string& msg) {
    out.push_back({ptr, lv, msg});
}

// ── JSON 值类型 vs schema type 关键字 ──
bool typeMatches(const json& v, const std::string& t) {
    if (t == "string")  return v.is_string();
    if (t == "boolean") return v.is_boolean();
    if (t == "array")   return v.is_array();
    if (t == "object")  return v.is_object();
    if (t == "number")  return v.is_number();
    if (t == "integer") return v.is_number_integer() || v.is_number_unsigned();
    return true; // 未知类型关键字不判错（子集外交由 Schema 合法性阶段约束）
}

std::string typeName(const json& v) {
    if (v.is_string())  return "string";
    if (v.is_boolean()) return "boolean";
    if (v.is_array())   return "array";
    if (v.is_object())  return "object";
    if (v.is_number_integer() || v.is_number_unsigned()) return "integer";
    if (v.is_number_float())  return "number";
    if (v.is_null())    return "null";
    return "unknown";
}

// 颜色："#RRGGBB[AA]" 字符串 或 {r,g,b,a} 对象
bool isColorValue(const json& v) {
    if (isPlaceholderValue(v)) return true;
    if (v.is_string()) {
        static const std::regex re("^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$");
        return std::regex_match(v.get<std::string>(), re);
    }
    if (v.is_object()) {
        for (const char* k : {"r", "g", "b"}) {
            if (!v.contains(k) || !v.at(k).is_number_integer()) return false;
        }
        return true; // a 可选
    }
    return false;
}

// 收集一个 def 的有效属性键（含 allOf $ref 展开，深度 2 层足够：def→common）
Keys collectDefPropsInto(const json& defs, const json& def, int depth = 0) {
    Keys out;
    if (depth > 4 || !def.is_object()) return out;
    if (def.contains("properties") && def["properties"].is_object()) {
        for (auto it = def["properties"].begin(); it != def["properties"].end(); ++it)
            out.insert(it.key());
    }
    if (def.contains("allOf") && def["allOf"].is_array()) {
        for (const auto& sub : def["allOf"]) {
            if (!sub.is_object() || !sub.contains("$ref")) continue;
            const std::string ref = sub["$ref"].get<std::string>();
            static const std::string kPrefix = "#/$defs/";
            if (ref.rfind(kPrefix, 0) == 0) {
                const std::string name = ref.substr(kPrefix.size());
                if (defs.contains(name)) {
                    Keys subKeys = collectDefPropsInto(defs, defs[name], depth + 1);
                    out.insert(subKeys.begin(), subKeys.end());
                }
            }
        }
    }
    return out;
}

// 在 def（含 allOf $ref 展开）中查找某属性的类型规则；未找到返回 null
json findRule(const json& defs, const json& def, const std::string& key, int depth = 0) {
    if (depth > 4 || !def.is_object()) return json();
    if (def.contains("properties") && def["properties"].is_object() &&
        def["properties"].contains(key))
        return def["properties"][key];
    if (def.contains("allOf") && def["allOf"].is_array()) {
        for (const auto& sub : def["allOf"]) {
            if (!sub.is_object() || !sub.contains("$ref")) continue;
            static const std::string kPrefix = "#/$defs/";
            const std::string ref = sub["$ref"].get<std::string>();
            if (ref.rfind(kPrefix, 0) == 0) {
                const std::string name = ref.substr(kPrefix.size());
                if (defs.contains(name)) {
                    json r = findRule(defs, defs[name], key, depth + 1);
                    if (!r.is_null()) return r;
                }
            }
        }
    }
    return json();
}

// 控件级未知键等级
Level unknownKeyLevel(bool strict) { return strict ? Level::Error : Level::Warn; }

// ── 数据节点校验（items / menus 元素：非控件，浅校验）──
void validateDataNode(const json& node, const Keys& keys, bool strict,
                      Issues& issues, const std::string& ptr) {
    DepthGuard dg;
    if (depthExceeded()) return;
    if (!node.is_object()) {
        addIssue(issues, Level::Error, ptr, "期望 object 的数据节点");
        return;
    }
    for (auto it = node.begin(); it != node.end(); ++it) {
        const std::string& k = it.key();
        if (isDynamicNodeKey(k)) continue;
        if (LayoutValidator::isUnknownKey(k, keys)) {
            addIssue(issues, unknownKeyLevel(strict), ptr + "/" + k,
                     "全局未知键（不在 PropertyNames.h）");
            continue;
        }
        // children 递归（tree items / menu 子项均经 children 嵌套）
        if ((k == "children" || k == "items") && it.value().is_array()) {
            int i = 0;
            for (const auto& child : it.value())
                validateDataNode(child, keys, strict, issues, ptr + "/" + k + "/" + std::to_string(i++));
        }
    }
}

} // namespace

// ────────────────────────── 公共 API ──────────────────────────

bool LayoutValidator::isUnknownKey(const std::string& key, const Keys& keys) {
    return keys.find(key) == keys.end();
}

Keys LayoutValidator::extractPropertyKeys(const std::string& headerPath, std::string& err) {
    Keys keys;
    std::ifstream in(headerPath);
    if (!in) { err = "无法打开 PropertyNames.h: " + headerPath; return keys; }

    // 匹配宏未展开形态：PROP_CONSTEXPR const char* kXxx = "...";（C 模式为 PROP_CONSTEXPR static，同前缀）
    static const std::regex re(
        R"re(PROP_CONSTEXPR\s+(?:inline\s+constexpr\s+|static\s+)?const\s+char\*\s+k\w+\s*=\s*"([^"]+)")re");
    std::string line;
    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) keys.insert(m[1].str());
    }
    if (keys.empty()) err = "未从 PropertyNames.h 提取到任何键: " + headerPath;
    return keys;
}

bool LayoutValidator::loadSchema(const std::string& schemaPath, json& out, std::string& err) {
    std::ifstream in(schemaPath);
    if (!in) { err = "无法打开 Schema 文件: " + schemaPath; return false; }
    try {
        in >> out;
    } catch (const json::parse_error& e) {
        err = std::string("Schema JSON 解析失败: ") + e.what();
        return false;
    }
    if (!out.is_object()) { err = "Schema 根必须是 object"; return false; }
    return true;
}

// 递归收集 properties 键并检查注册 + $ref 目标存在性
static void validateSchemaWalk(const json& node, const Keys& keys,
                               const json* defs, Issues& issues, const std::string& ptr) {
    if (!node.is_object()) return;

    // $defs 内每个 def 同样受检（根入口唯一可达 $defs 的路径）
    if (node.contains("$defs") && node["$defs"].is_object()) {
        for (auto it = node["$defs"].begin(); it != node["$defs"].end(); ++it)
            validateSchemaWalk(it.value(), keys, defs, issues, ptr + "/$defs/" + it.key());
    }

    if (node.contains("$ref") && node["$ref"].is_string() && defs) {
        const std::string ref = node["$ref"].get<std::string>();
        static const std::string kPrefix = "#/$defs/";
        if (ref.rfind(kPrefix, 0) == 0) {
            const std::string name = ref.substr(kPrefix.size());
            if (!defs->contains(name))
                addIssue(issues, Level::SchemaInvalid, ptr + "/$ref",
                         "$ref 目标不存在: " + ref);
        }
    }
    if (node.contains("properties") && node["properties"].is_object()) {
        for (auto it = node["properties"].begin(); it != node["properties"].end(); ++it) {
            if (LayoutValidator::isUnknownKey(it.key(), keys))
                addIssue(issues, Level::SchemaInvalid, ptr + "/properties/" + it.key(),
                         "Schema 引用未注册键: " + it.key());
            validateSchemaWalk(it.value(), keys, defs, issues, ptr + "/properties/" + it.key());
        }
    }
    for (const char* member : {"allOf", "oneOf", "items"}) {
        if (node.contains(member)) {
            const json& arr = node[member];
            if (arr.is_array()) {
                int i = 0;
                for (const auto& sub : arr)
                    validateSchemaWalk(sub, keys, defs, issues, ptr + "/" + member + "/" + std::to_string(i++));
            } else if (arr.is_object()) {
                validateSchemaWalk(arr, keys, defs, issues, ptr + "/" + member);
            }
        }
    }
}

Issues LayoutValidator::validateSchema(const json& schema, const Keys& keys) {
    Issues issues;
    if (!schema.is_object()) {
        addIssue(issues, Level::SchemaInvalid, "", "Schema 根不是 object");
        return issues;
    }
    const json* defs = schema.contains("$defs") ? &schema["$defs"] : nullptr;
    validateSchemaWalk(schema, keys, defs, issues, "");
    return issues;
}

// ── 控件对象校验 ──
// componentNames：顶层 components 定义的组件名（实例 type=组件名 → 动态机制，跳过）
static void validateControl(const json& ctrl, const json& defs, const Keys& keys,
                            bool strict, Issues& issues, const std::string& ptr,
                            const Keys& componentNames) {
    DepthGuard dg;
    if (depthExceeded()) {
        addIssue(issues, Level::Warn, ptr, "嵌套深度超上限，停止下钻（上限 32）");
        return;
    }
    if (!ctrl.is_object()) {
        addIssue(issues, Level::Error, ptr, "控件定义必须是 object");
        return;
    }

    // required: type（决策：全部控件 type 必填）
    if (!ctrl.contains("type") || !ctrl.at("type").is_string()) {
        addIssue(issues, Level::Error, ptr + "/type", "缺少必选字段 type（string）");
        return;
    }
    const std::string type = ctrl.at("type").get<std::string>();
    if (componentNames.count(type)) return;  // 组件实例（决策 4.5 动态机制，宽松放行）
    if (!defs.contains(type)) {
        addIssue(issues, Level::Error, ptr + "/type", "未知控件类型: " + type);
        return;
    }
    const json def = defs[type];

    // 有效属性集 = 本控件 properties ∪ allOf 展开(common)
    Keys valid = collectDefPropsInto(defs, def);

    for (auto it = ctrl.begin(); it != ctrl.end(); ++it) {
        const std::string& k = it.key();
        const json& v = it.value();
        const std::string kPtr = ptr + "/" + k;

        if (k == "type") continue;
        if (isDynamicNodeKey(k)) continue;                       // 决策 4.5
        if (isPlaceholderValue(v)) continue;                     // {{占位符}}

        // 双层防御第一层：全局未知键
        if (LayoutValidator::isUnknownKey(k, keys)) {
            addIssue(issues, unknownKeyLevel(strict), kPtr,
                     "全局未知键（不在 PropertyNames.h）");
            continue;
        }

        // 结构性容器：children 下钻控件；其余(items/menus/events/actors…)浅处理
        if (structuralKeys().count(k)) {
            if (k == "children" && v.is_array()) {
                int i = 0;
                for (const auto& child : v)
                    validateControl(child, defs, keys, strict, issues,
                                    kPtr + "/" + std::to_string(i++), componentNames);
            } else if ((k == "items" || k == "menus") && v.is_array()) {
                int i = 0;
                for (const auto& node : v)
                    validateDataNode(node, keys, strict, issues,
                                     kPtr + "/" + std::to_string(i++));
            }
            continue;
        }

        // 双层防御第二层：控件级未知键（已注册但非本控件所有）
        if (!valid.count(k)) {
            addIssue(issues, unknownKeyLevel(strict), kPtr,
                     "控件级未知键（type=" + type + " 不支持此属性）");
            continue;
        }

        // 类型 / 枚举 / const / pattern 校验（Schema 子集解释器；$ref/allOf 展开一层）
        json rule = findRule(defs, def, k);
        if (rule.is_null()) continue;
        if (rule.is_object() && rule.contains("$ref") && rule["$ref"].is_string()) {
            static const std::string kRefPrefix = "#/$defs/";
            const std::string ref = rule["$ref"].get<std::string>();
            if (ref.rfind(kRefPrefix, 0) == 0) {
                const std::string name = ref.substr(kRefPrefix.size());
                if (defs.contains(name)) rule = defs[name];
            }
        }
        if (!rule.is_object()) continue;
        if (rule.contains("x-color")) {
            if (!isColorValue(v))
                addIssue(issues, Level::Error, kPtr, "颜色格式非法（#RRGGBB[AA] 或 {r,g,b,a}）");
            continue;
        }
        if (rule.contains("type") && rule["type"].is_string()) {
            const std::string t = rule["type"].get<std::string>();
            if (!typeMatches(v, t)) {
                addIssue(issues, Level::Error, kPtr,
                         "期望 " + t + ", 实际 " + typeName(v));
                continue;
            }
        }
        if (rule.contains("enum") && rule["enum"].is_array() && v.is_string()) {
            bool ok = false;
            for (const auto& e : rule["enum"])
                if (e.is_string() && e.get<std::string>() == v.get<std::string>()) { ok = true; break; }
            if (!ok)
                addIssue(issues, Level::Error, kPtr,
                         "枚举非法: \"" + v.get<std::string>() + "\"");
            continue;
        }
        if (rule.contains("const")) {
            if (rule["const"] != v)
                addIssue(issues, Level::Error, kPtr, "const 不匹配");
            continue;
        }
        if (rule.contains("pattern") && rule["pattern"].is_string() && v.is_string()) {
            try {
                static std::unordered_map<std::string, std::regex> cache;
                auto itc = cache.find(rule["pattern"].get<std::string>());
                if (itc == cache.end())
                    itc = cache.emplace(rule["pattern"].get<std::string>(),
                                        std::regex(rule["pattern"].get<std::string>())).first;
                if (!std::regex_match(v.get<std::string>(), itc->second))
                    addIssue(issues, Level::Error, kPtr, "不匹配 pattern");
            } catch (const std::regex_error&) { /* Schema 正则非法：合法性阶段不查 pattern，此处静默 */ }
        }
    }
}

Issues LayoutValidator::validateLayout(const json& layout, const json& schema,
                                       const Keys& keys, bool strict) {
    Issues issues;
    if (!layout.is_object()) {
        addIssue(issues, Level::Error, "", "布局根必须是 object");
        return issues;
    }
    const json defs = schema.value("$defs", json::object());

    // 收集组件名（components 定义键）——实例 type=组件名的节点跳过严格校验（决策 4.5）
    Keys componentNames;
    if (layout.contains("components") && layout["components"].is_object()) {
        for (auto it = layout["components"].begin(); it != layout["components"].end(); ++it)
            componentNames.insert(it.key());
    }

    for (auto it = layout.begin(); it != layout.end(); ++it) {
        const std::string& k = it.key();
        const json& v = it.value();
        if (isDynamicNodeKey(k)) continue;                       // components/template/bind
        if (LayoutValidator::isUnknownKey(k, keys)) {
            addIssue(issues, unknownKeyLevel(strict), "/" + k, "全局未知键（不在 PropertyNames.h）");
            continue;
        }
        if ((k == "controls" || k == "dialogs") && v.is_array()) {
            int i = 0;
            for (const auto& ctl : v)
                validateControl(ctl, defs, keys, strict, issues,
                                "/" + k + "/" + std::to_string(i++), componentNames);
        }
        // 其余顶层成员（theme/layouts/viewport/resourceProviders…）一期放行（§9 语法层 only）
    }
    return issues;
}

} // namespace uic_tools
