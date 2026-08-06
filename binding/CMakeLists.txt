# UICornerstone C++ Binding — MIT 许可证，独立于核心库构建。
# 依赖：include/UICornerstoneAPI.h（C ABI）。
cmake_minimum_required(VERSION 3.16)
project(UICornerstoneBinding LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_options(/FS)
endif()

# ============================================================
# Binding 静态库（不链接核心库；符号由使用者链接 core 或 dll 提供）
# ============================================================
add_library(UICornerstoneBinding STATIC
    src/UICornerstone.cpp
    src/Control.cpp
    src/Event.cpp
)

target_include_directories(UICornerstoneBinding PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include          # UICornerstoneAPI.h
)

if(WIN32)
    target_link_libraries(UICornerstoneBinding PRIVATE kernel32)
endif()

# ============================================================
# Samples（仅当位于主工程构建树内且存在核心库 target 时构建）
# 核心库（静态 UICornerstone）内含后端，直接链接即可。
# ============================================================
if(TARGET UICornerstone)
    set(BINDING_SAMPLES
        sample_cpp_hosted
        sample_cpp_embed
    )
    foreach(sample IN LISTS BINDING_SAMPLES)
        add_executable(${sample} samples/${sample}.cpp)
        target_link_libraries(${sample} PRIVATE UICornerstoneBinding UICornerstone)
    endforeach()

    # 后端 DLL 拷贝到各样例输出目录（SDL3/SFML/raylib 运行库）
    if(WIN32 AND DEFINED BACKEND_DLLS)
        foreach(sample IN LISTS BINDING_SAMPLES)
            foreach(dll ${BACKEND_DLLS})
                add_custom_command(TARGET ${sample} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll}"
                        $<TARGET_FILE_DIR:${sample}>)
            endforeach()
        endforeach()
    endif()
endif()

message(STATUS "UICornerstoneBinding: sources registered")