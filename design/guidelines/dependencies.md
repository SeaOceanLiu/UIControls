# Dependencies (Submodules)

| Submodule  | Path                  | Source                                    |
| ---------- | --------------------- | ----------------------------------------- |
| SDL3       | subModules/SDL3       | SeaOceanLiu/UICornerstone-sdl3            |
| SDL3_ttf   | subModules/SDL3_ttf   | SeaOceanLiu/UICornerstone-sdl3_ttf        |
| SDL3_image | subModules/SDL3_image | libsdl-org/SDL_image (tag: release-3.2.4) |
| json       | subModules/json       | nlohmann/json (tag: v3.12.0)              |
| assets     | subModules/assets     | SeaOceanLiu/UICornerstone-assets          |
| libs       | subModules/libs       | SeaOceanLiu/UICornerstone-libs            |
| SFML       | subModules/SFML       | SeaOceanLiu/UICornerstone-SFML (fork of SFML v3, Debug DLLs) |

## SFML fork 的修改说明（务必阅读，勿回退）

SFML 后端以**预编译产物**方式使用（不编译 SFML 源码）：链接 `subModules/SFML/lib/*.lib`，
运行时拷贝 `subModules/SFML/bin/*.dll`，编译期包含 `subModules/SFML/include` 头文件。

### 为什么 fork 了 SFML（区别于上游的唯一一处修改）

- 项目在 WIN32 下定义了 `_HAS_STD_BYTE=0`（见根 `CMakeLists.txt`，原因见下）→
  MSVC 的 `<cstddef>` 不再提供 `std::byte`。
- SFML 官方头 `include/SFML/System/MemoryInputStream.hpp` 的成员
  `const std::byte* m_data` 依赖 `std::byte` → 在该宏下**直接编译失败**。
- fork 补丁将该成员类型改为 `const unsigned char*`（**仅类型等价替换**：
  `sf::priv::MemoryInputStream` 是编译进 `sfml-system.dll` 的内部类，成员不跨 DLL 边界，
  无 ABI 影响）。
- 注意：**即使只使用 SFML 的 DLL 也必须保留此补丁**——项目源码（TextRenderer 等）编译时
  会经包含链包含 `MemoryInputStream.hpp`，官方头在 `_HAS_STD_BYTE=0` 下无法通过编译。

### 为什么不能删除 `_HAS_STD_BYTE=0`

- Windows SDK 头（`rpcndr.h`/`wtypesbase.h`/`wtypes.h`，经 `windows.h` 链）定义全局
  `::byte`；项目多个公共头（`ConstDef.h`、`ControlBase.h`、`EventQueue.h`、`Theme.h` 等）
  存在 `using namespace std;` 会把 `std::byte` 带入全局命名空间 → 实测删除宏后大量
  `C2872: "byte": 不明确的符号`（SFML/SDL3 头包含链均触发）。
- 修复该冲突需重构上述公共头（去 `using namespace std`），回归成本高，故维持宏 + fork 补丁方案。

### zlib 许可证合规

SFML 为 zlib 许可，**允许修改与再分发**，但条款 2 要求修改版须明确标记。
已满足：fork 提交 `7c06ed0`（"docs: mark MemoryInputStream.hpp as altered"）在修改点
内联注释说明原因并指向本文档；fork 提交历史保留原始提交。

### 排查指引

- Pages/CI 报 `upload-pack: not our ref`：说明主仓库 gitlink 指向了远端 fork 没有的提交，
  更新 fork 到包含该提交后再 push 主仓库（或回退 gitlink 到远端已有提交）。

