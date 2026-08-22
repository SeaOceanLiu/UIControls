# validate_layout -- 声明式 UI JSON 语法校验器（独立子工程，与后端无关）
# 设计：design/JSON_Schema_Design.md（§4.9 归档/协议/产物、§5.3 CMake 集成）
# 构建：cmake -S tools -B build/tools && cmake --build build/tools --config Debug
cmake_minimum_required(VERSION 3.15)
project(validate_layout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── PropertyNames.h 头文件注入（决策 4.3/4.9）──
# 主仓 include/PropertyNames.h 构建期拷贝至 generated/，DEPENDS 监视变更自动重拷；
# 拷贝件为构建产物非第二源（不入库），tools 对主仓零路径耦合、可整体拎走。
set(PN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../include/PropertyNames.h")
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")
add_custom_command(
    OUTPUT "${GEN_DIR}/PropertyNames.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PN_SRC}" "${GEN_DIR}/PropertyNames.h"
    DEPENDS "${PN_SRC}"
    COMMENT "[tools] sync PropertyNames.h from main repo")
add_custom_target(sync_property_names DEPENDS "${GEN_DIR}/PropertyNames.h")

add_executable(validate_layout
    validate_layout.cpp
    LayoutValidator.cpp)
add_dependencies(validate_layout sync_property_names)
target_include_directories(validate_layout PRIVATE
    "${GEN_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../subModules/json/single_include")

if(WIN32)
    target_compile_options(validate_layout PRIVATE /W4 /utf-8)
else()
    target_compile_options(validate_layout PRIVATE -Wall -Wextra)
endif()

# ── ctest 自包含注册（与三棵 UI 后端树零耦合）──
file(GLOB LAYOUT_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/../layouts/*.json")
enable_testing()
# ① Schema 合法性自检：对 schema 文件本身跑 validateSchema（引用幽灵键 → 退出码 2）
add_test(NAME schema_selfcheck
    COMMAND validate_layout --schema-only
        "--schema=${CMAKE_CURRENT_SOURCE_DIR}/../docs/schema/declarative-ui.schema.json"
        "--property-names=${GEN_DIR}/PropertyNames.h")
# ② 样本回归：layouts/ 全部样本 --strict 断言退出码 0（WORKING_DIRECTORY 使相对路径生效）
add_test(NAME layouts_strict
    COMMAND validate_layout --strict
        "--schema=${CMAKE_CURRENT_SOURCE_DIR}/../docs/schema/declarative-ui.schema.json"
        "--property-names=${GEN_DIR}/PropertyNames.h"
        ${LAYOUT_SAMPLES})
set_tests_properties(layouts_strict PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

# ③ 负例回归：坏布局必须被抓（validate_layout 退出码非 0 → WILL_FAIL 反转为通过）
set(BAD_LAYOUTS
    bad_missing_type.json   # 缺 type
    bad_unknown_key.json    # 全局/控件级未知键
    bad_enum.json           # 枚举非法
    bad_color.json          # 颜色非 hex（"red"）
    bad_typemismatch.json   # size 应 integer 实际 string
    bad_colorformat.json)   # #12345 位数不足
foreach(bad ${BAD_LAYOUTS})
    add_test(NAME "negative_${bad}"
        COMMAND validate_layout --strict
            "--schema=${CMAKE_CURRENT_SOURCE_DIR}/../docs/schema/declarative-ui.schema.json"
            "--property-names=${GEN_DIR}/PropertyNames.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/${bad}")
    set_tests_properties("negative_${bad}" PROPERTIES WILL_FAIL TRUE)
endforeach()
# 幽灵键 Schema → 退出码 2（--schema-only）
add_test(NAME negative_schema_ghost_key
    COMMAND validate_layout --schema-only
        "--schema=${CMAKE_CURRENT_SOURCE_DIR}/tests/bad_schema_ref.json"
        "--property-names=${GEN_DIR}/PropertyNames.h")
set_tests_properties(negative_schema_ghost_key PROPERTIES WILL_FAIL TRUE)
