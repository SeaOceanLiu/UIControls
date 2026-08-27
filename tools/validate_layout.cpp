// ============================================================================
// validate_layout.cpp -- 声明式 UI 布局 JSON 校验器可执行入口
// 用法:
//   validate_layout <layout.json...> [选项]
//   选项:
//     --schema=<path>          指定 schema 文件（缺省: 本程序同目录 <exeDir>/schema/declarative-ui.schema.json）
//     --property-names=<path>  指定 PropertyNames.h（缺省: 本程序同目录 <exeDir>/PropertyNames.h）
//     --strict                 将 WARN 视作 ERROR（有任一 WARN 也返回 1）
//     --schema-only            只校验 schema 自身，不校验布局
//     --help, -h               显示本帮助
//   说明:
//     - 缺省路径基于可执行文件所在目录解析（发布包 layout: exe 与 PropertyNames.h 同目录、
//       schema 位于下一级 schema/ 子目录）
//     - 可同时传入多个布局文件依次校验
//   退出码: 0=通过 / 1=有错（或 --strict 下有 WARN）/ 2=Schema 自身非法或加载失败 / 参数错误
// 设计: design/JSON_Schema_Design.md §5.2-5.3 · 协议 MIT(tools/LICENSE)
// ============================================================================
#include "LayoutValidator.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#undef min
#undef max
#endif

using uic_tools::LayoutValidator;
using uic_tools::ValidationIssue;

// 可执行文件所在目录（不含尾部分隔符；非 Windows 回退当前目录）
static std::string exeDirectory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n == 0) return ".";
    std::string p(buf, n);
    size_t pos = p.find_last_of("\\/");
    if (pos != std::string::npos) p = p.substr(0, pos);
    return p;
#else
    return ".";
#endif
}

static void printHelp(const char* prog) {
    printf("validate_layout - 声明式 UI 布局 JSON 语法校验器\n");
    printf("\n");
    printf("用法:\n");
    printf("  %s <layout.json...> [选项]\n", prog);
    printf("\n");
    printf("选项:\n");
    printf("  --schema=<path>          指定 schema 文件\n");
    printf("                          缺省: <exeDir>/schema/declarative-ui.schema.json\n");
    printf("  --property-names=<path>  指定 PropertyNames.h\n");
    printf("                          缺省: <exeDir>/PropertyNames.h\n");
    printf("  --strict                 将 WARN 视作 ERROR（任一 WARN 也返回 1）\n");
    printf("  --schema-only            只校验 schema 自身合法性，不校验布局\n");
    printf("  --help, -h               显示本帮助并退出\n");
    printf("\n");
    printf("缺省路径说明:\n");
    printf("  基于可执行文件所在目录解析——发布包布局: exe 与 PropertyNames.h 同目录,\n");
    printf("  schema 位于下一级 schema/ 子目录。也可用 --schema=/--property-names= 覆盖。\n");
    printf("\n");
    printf("退出码:\n");
    printf("  0 = 全部通过\n");
    printf("  1 = 存在 ERROR（或 --strict 下有 WARN）\n");
    printf("  2 = Schema 自身非法/加载失败，或参数错误\n");
}

int main(int argc, char* argv[]) {
    std::vector<std::string> layouts;
    const std::string exeDir = exeDirectory();
    std::string schemaPath    = exeDir + "/schema/declarative-ui.schema.json";
    std::string propertyNames = exeDir + "/PropertyNames.h";
    bool strict = false;
    bool schemaOnly = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a.rfind("--schema=", 0) == 0)                 schemaPath = a.substr(9);
        else if (a.rfind("--property-names=", 0) == 0)    propertyNames = a.substr(17);
        else if (a == "--strict")                         strict = true;
        else if (a == "--schema-only")                    schemaOnly = true;
        else if (a == "--help" || a == "-h")              { printHelp(argv[0]); return 0; }
        else if (!a.empty() && a[0] != '-')               layouts.push_back(a);
        else { fprintf(stderr, "未知参数: %s（用 --help 查看用法）\n", a.c_str()); return 2; }
    }

    // ① PropertyNames.h 键集（双层防御第一层的数据源）
    std::string err;
    LayoutValidator::Keys keys = LayoutValidator::extractPropertyKeys(propertyNames, err);
    if (!err.empty()) { fprintf(stderr, "ERROR: 加载 PropertyNames.h 失败: %s\n", err.c_str()); return 2; }
    printf("[keys] PropertyNames.h 键集: %zu 个 (%s)\n", keys.size(), propertyNames.c_str());

    // ② Schema 加载 + 合法性检查（引用未注册键 → 退出码 2）
    nlohmann::json schema;
    if (!LayoutValidator::loadSchema(schemaPath, schema, err)) {
        fprintf(stderr, "ERROR: 加载 Schema 失败: %s\n", err.c_str());
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

    if (layouts.empty()) { fprintf(stderr, "未提供布局文件（用 --help 查看用法）\n"); return 1; }

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