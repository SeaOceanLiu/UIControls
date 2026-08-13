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
# Binding 静态库（纯动态加载模式：核心 DLL + 后端 DLL 均经
# LoadLibrary 显式加载；不链接 UICornerstone_dll.lib / UICornerstone.lib）
# ============================================================
add_library(UICornerstoneBinding STATIC
    src/UICornerstone.cpp
    src/Control.cpp
    src/Event.cpp
    src/DynamicApi.cpp
)

target_include_directories(UICornerstoneBinding PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include          # UICornerstoneAPI.h
)

# ============================================================
# 字符串枚举头（PropertyNames.h）：每次构建时从核心复制到
# binding 构建目录 include，供用户直接 include 使用（避免裸字符串）。
# 复制失败时保留现有的（上次复制）副本，不中断构建。
# 许可边界：核心头不进入 binding 源码树，仅作构建产物输出。
# ============================================================
set(BINDING_GEN_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
add_custom_command(TARGET UICornerstoneBinding PRE_BUILD
    COMMAND ${CMAKE_COMMAND}
        -D SRC_PROPERTY_NAMES=${CMAKE_CURRENT_SOURCE_DIR}/../include/PropertyNames.h
        -D DST_DIR=${BINDING_GEN_INCLUDE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyPropertyNames.cmake)
target_include_directories(UICornerstoneBinding PUBLIC
    ${BINDING_GEN_INCLUDE})

if(WIN32)
    target_link_libraries(UICornerstoneBinding PRIVATE kernel32)
endif()

# ============================================================
# Samples
# 构建条件：
#   - 主工程构建树内（存在核心库 target），或
#   - 独立配置时指定 UICORNERSTONE_CORE_OUTPUT（核心产物目录，
#     样例运行依赖 DLL 从此目录拷贝）。
# 纯动态加载：仅链接 UICornerstoneBinding（静态包装），不链接核心库。
# 运行依赖：UICornerstone.dll + UIBackend_<backend>.dll（POST_BUILD 拷贝）。
# ============================================================
set(UICORNERSTONE_CORE_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/../build/sdl3/Debug" CACHE PATH
    "核心库构建输出目录（独立配置 binding 时，样例依赖 DLL 的拷贝来源）")

if(TARGET UICornerstone OR UICORNERSTONE_CORE_OUTPUT)
    set(BINDING_SAMPLES
        sample_cpp_hosted
        sample_cpp_embed
        sample_cpp_multiview
        sample_cpp_multiinstance
        sample_scale
        sample_viewport_scale
    )
    foreach(sample IN LISTS BINDING_SAMPLES)
        add_executable(${sample} samples/${sample}.cpp)
        target_link_libraries(${sample} PRIVATE UICornerstoneBinding)
    endforeach()

    # 核心 DLL（UICornerstone.dll）拷贝到各样例输出目录
    # 主树模式：直接引用 UICornerstone_dll target；独立模式：从
    # UICORNERSTONE_CORE_OUTPUT 目录拷贝全部 DLL。
    if(WIN32)
        if(TARGET UICornerstone_dll)
            foreach(sample IN LISTS BINDING_SAMPLES)
                add_custom_command(TARGET ${sample} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:UICornerstone_dll>"
                        $<TARGET_FILE_DIR:${sample}>)
            endforeach()
        elseif(EXISTS "${UICORNERSTONE_CORE_OUTPUT}")
            file(GLOB BINDING_CORE_DLLS "${UICORNERSTONE_CORE_OUTPUT}/*.dll")
            foreach(sample IN LISTS BINDING_SAMPLES)
                foreach(dll IN LISTS BINDING_CORE_DLLS)
                    add_custom_command(TARGET ${sample} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${dll}"
                            $<TARGET_FILE_DIR:${sample}>)
                endforeach()
            endforeach()
        endif()
    endif()

    # 后端插件 DLL（UIBackend_<backend>.dll）拷贝到各样例输出目录
    if(WIN32 AND DEFINED UIBACKEND_TARGET AND TARGET ${UIBACKEND_TARGET})
        foreach(sample IN LISTS BINDING_SAMPLES)
            add_custom_command(TARGET ${sample} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${UIBACKEND_TARGET}>"
                    $<TARGET_FILE_DIR:${sample}>)
        endforeach()
    endif()

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

    # 资源目录拷贝（字体/图片/动画，与 test 目标一致）
    get_filename_component(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../subModules/assets" ABSOLUTE)
    if(EXISTS "${ASSETS_DIR}")
        foreach(sample IN LISTS BINDING_SAMPLES)
            add_custom_command(TARGET ${sample} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${ASSETS_DIR}"
                    $<TARGET_FILE_DIR:${sample}>/assets)
        endforeach()
    endif()
endif()

message(STATUS "UICornerstoneBinding: sources registered")