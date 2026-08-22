// ============================================================================
// validate_layout.cpp -- 声明式 UI 布局 JSON 校验器可执行入口
// 用法:
//   validate_layout <layout.json...> [--schema=<path>] [--property-names=<path>]
//                   [--strict] [--schema-only]
// 退出码: 0=通过 / 1=有错 / 2=Schema 自身非法或加载失败
// 设计: design/JSON_Schema_Design.md §5.2-5.3 · 协议 MIT(tools/LICENSE)
// ============================================================================
#include "LayoutValidator.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using uic_tools::LayoutValidator;
using uic_tools::ValidationIssue;

int main(int argc, char* argv[]) {
    std::vector<std::string> layouts;
    std::string schemaPath    = "docs/schema/declarative-ui.schema.json";
    std::string propertyNames = "include/PropertyNames.h";
    bool strict = false;
    bool schemaOnly = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a.rfind("--schema=", 0) == 0)                 schemaPath = a.substr(9);
        else if (a.rfind("--property-names=", 0) == 0)    propertyNames = a.substr(17);
        else if (a == "--strict")                         strict = true;
        else if (a == "--schema-only")                    schemaOnly = true;
        else if (!a.empty() && a[0] != '-')               layouts.push_back(a);
        else { fprintf(stderr, "未知参数: %s\n", a.c_str()); return 2; }
    }

    // ① PropertyNames.h 键集（双层防御第一层的数据源）
    std::string err;
    LayoutValidator::Keys keys = LayoutValidator::extractPropertyKeys(propertyNames, err);
    if (!err.empty()) { fprintf(stderr, "ERROR: %s\n", err.c_str()); return 2; }
    printf("[keys] PropertyNames.h 键集: %zu 个\n", keys.size());

    // ② Schema 加载 + 合法性检查（引用未注册键 → 退出码 2）
    nlohmann::json schema;
    if (!LayoutValidator::loadSchema(schemaPath, schema, err)) {
        fprintf(stderr, "ERROR: %s\n", err.c_str());
        return 2;
    }
    auto sIssues = LayoutValidator::validateSchema(schema, keys);
    bool schemaInvalid = false;
    for (const auto& is : sIssues) {
        printf("%s: %s %s %s\n", schemaPath.c_str(), is.jsonPointer.c_str(),
               is.levelString(), is.message.c_str());
        if (is.level == uic_tools::Level::SchemaInvalid) schemaInvalid = true;
    }
    if (schemaInvalid) { fprintf(stderr, "Schema 非法（引用未注册键 / $ref 缺失）\n"); return 2; }
    if (schemaOnly) { printf("Schema OK\n"); return 0; }

    if (layouts.empty()) { fprintf(stderr, "未提供布局文件\n"); return 1; }

    // ③ 逐文件校验
    int totalErrors = 0, totalWarns = 0;
    for (const auto& file : layouts) {
        nlohmann::json layout;
        std::ifstream in(file);
        if (!in) { printf("%s: ERROR 无法打开文件\n", file.c_str()); ++totalErrors; continue; }
        try {
            in >> layout;
        } catch (const nlohmann::json::parse_error& e) {
            printf("%s: ERROR JSON 语法错误: %s\n", file.c_str(), e.what());
            ++totalErrors;
            continue;
        }
        auto issues = LayoutValidator::validateLayout(layout, schema, keys, strict);
        for (const auto& is : issues) {
            printf("%s: %s %s %s\n", file.c_str(), is.jsonPointer.c_str(),
                   is.levelString(), is.message.c_str());
            if (is.level == uic_tools::Level::Error) ++totalErrors;
            else if (is.level == uic_tools::Level::Warn) ++totalWarns;
        }
    }

    // ④ 汇总：有 ERROR，或 strict 下有 WARN → 退出码 1（决策 4.6）
    printf("[summary] errors=%d warns=%d%s -> %s\n", totalErrors, totalWarns,
           strict ? " (strict)" : "",
           (totalErrors > 0 || (strict && totalWarns > 0)) ? "FAIL" : "PASS");
    return (totalErrors > 0 || (strict && totalWarns > 0)) ? 1 : 0;
}
