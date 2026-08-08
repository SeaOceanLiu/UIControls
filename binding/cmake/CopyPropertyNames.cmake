# UICornerstone C++ Binding — 构建时同步核心字符串枚举头（PropertyNames.h）。
# 许可边界：核心头不进入 binding 源码树，仅复制为构建产物（生成 include）。
# 若源不可用（核心目录缺失/未配置），保留现有的（上次复制）副本，不中断构建。

if(EXISTS "${SRC_PROPERTY_NAMES}")
    file(COPY "${SRC_PROPERTY_NAMES}" DESTINATION "${DST_DIR}")
    message(STATUS "[Binding] PropertyNames.h copied from core")
else()
    message(STATUS "[Binding] PropertyNames.h source not found, keeping existing copy")
endif()
