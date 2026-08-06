#ifndef UICORNERSTONE_API_H
#define UICORNERSTONE_API_H

#include <stdint.h>
#include <stddef.h>

/* DLL export/import macro */
#if defined(UICORNERSTONE_BUILD_SHARED)
#  if defined(UICORNERSTONE_API_EXPORT)
#    define UICORNERSTONE_API __declspec(dllexport)
#  else
#    define UICORNERSTONE_API __declspec(dllimport)
#  endif
#else
#  define UICORNERSTONE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 句柄类型 ============ */
typedef void* UIWindowHandle;
typedef void* UIRenderDeviceHandle;
typedef void* UIInputBackendHandle;
typedef void* UITextRendererHandle;
typedef void* UIResourceProviderHandle;
typedef void* UIControlHandle;
typedef void* UIFontHandle;
typedef void* UITextHandle;
typedef void* UITextureHandle;

/* 实例句柄：一个 UICornerstone 实例（窗口或视口）。不透明指针。 */
typedef struct UIContext* UIInstance;

/* 实例配置：可选项，全部可 NULL/0 表示默认。structSize 用于 C API 版本兼容检查。 */
typedef struct {
    uint32_t    structSize;         /* 必须填 sizeof(UIInstanceConfig) */
    const char* debugLabel;         /* 调试标签，null → "Instance_<id>" */
    const char* resourceRoot;       /* 资源根目录，null → 默认 */
    const char* windowTitle;        /* 窗口标题，null → "UICornerstone" */
    int         windowWidth;        /* 0 → 默认 1024 */
    int         windowHeight;       /* 0 → 默认 768 */
    uint32_t    windowFlags;        /* 跨后端统一窗口标志（UIWindowFlags，值对齐 SDL_WINDOW_*） */
    uint32_t    reserved[6];        /* 未来扩展预留 */
} UIInstanceConfig;

#define UI_INSTANCE_CONFIG_DEFAULT \
    { sizeof(UIInstanceConfig), NULL, NULL, NULL, 0, 0, 0, {0} }

/* ============ 基础类型 ============ */
typedef struct { float x, y, w, h; }   UIRect;
typedef struct { uint8_t r, g, b, a; } UIColor;

typedef struct {
    UIColor normal;
    UIColor hover;
    UIColor pressed;
    UIColor disabled;
} UIStateColor;

/* ============ 事件类型 ============ */
typedef enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_MOUSE_MOVE,
    UI_EVENT_MOUSE_DOWN,
    UI_EVENT_MOUSE_UP,
    UI_EVENT_MOUSE_WHEEL,
    UI_EVENT_KEY_DOWN,
    UI_EVENT_KEY_UP,
    UI_EVENT_TEXT_INPUT,
    UI_EVENT_WINDOW_RESIZE,
    UI_EVENT_WINDOW_CLOSE,
    UI_EVENT_FOCUS_GAINED,
    UI_EVENT_FOCUS_LOST,
} UIEventType;

#define UI_EVENT_BUF_SIZE 128
#define UI_TEXT_MAX 32

typedef struct {
    UIEventType type;
    uint8_t data[UI_EVENT_BUF_SIZE];
} UIEvent;

/* 便捷访问宏 */
#define UI_EVENT_MOUSE_X(ev)     (*(float*)(ev)->data)
#define UI_EVENT_MOUSE_Y(ev)     (*(float*)((ev)->data + 4))
#define UI_EVENT_BUTTON(ev)      (*(int*)((ev)->data + 8))
#define UI_EVENT_WHEEL_DELTA(ev) (*(float*)(ev)->data)
#define UI_EVENT_WHEEL_MOUSE_X(ev) (*(float*)((ev)->data + 4))
#define UI_EVENT_WHEEL_MOUSE_Y(ev) (*(float*)((ev)->data + 8))
#define UI_EVENT_KEY_CODE(ev)    (*(int*)(ev)->data)
#define UI_EVENT_KEY_MOD(ev)     (*(uint16_t*)((ev)->data + 4))
#define UI_EVENT_TEXT(ev)        ((const char*)(ev)->data)
#define UI_EVENT_RESIZE_W(ev)    (*(int*)(ev)->data)
#define UI_EVENT_RESIZE_H(ev)    (*(int*)((ev)->data + 4))

/* ============ 后端回调表 ============ */
typedef struct {
    int version;  // 必须设为 1

    // --- Window (可选，只为 NULL) ---
    UIWindowHandle  (*createWindow)(const char* title, int w, int h, uint32_t flags);
    void            (*destroyWindow)(UIWindowHandle);
    void            (*getWindowSize)(UIWindowHandle, float* w, float* h);
    void            (*getWindowPosition)(UIWindowHandle, float* x, float* y);
    float           (*getDisplayWidth)(UIWindowHandle);
    float           (*getDisplayHeight)(UIWindowHandle);
    float           (*getDpiScale)(UIWindowHandle);
    void            (*setWindowTitle)(UIWindowHandle, const char*);
    int             (*getMousePosition)(UIWindowHandle, float* x, float* y);

    // --- RenderDevice (必须) ---
    UIRenderDeviceHandle (*createRenderDevice)(void* nativeContext);
    void                 (*destroyRenderDevice)(UIRenderDeviceHandle);
    void                 (*setDrawColor)(UIRenderDeviceHandle, UIColor);
    void                 (*setBlendMode)(UIRenderDeviceHandle, int mode);
    void                 (*setClipRect)(UIRenderDeviceHandle, float x, float y, float w, float h);
    void                 (*clearClipRect)(UIRenderDeviceHandle);
    void                 (*fillRect)(UIRenderDeviceHandle, float x, float y, float w, float h);
    void                 (*drawRect)(UIRenderDeviceHandle, float x, float y, float w, float h);
    void                 (*drawLine)(UIRenderDeviceHandle, float x1, float y1, float x2, float y2);
    void                 (*drawPoint)(UIRenderDeviceHandle, float x, float y);
    void                 (*clear)(UIRenderDeviceHandle, UIColor);
    void                 (*present)(UIRenderDeviceHandle);
    void                 (*flush)(UIRenderDeviceHandle);
    void*                (*getNativeHandle)(UIRenderDeviceHandle);
    // 后端配置键值（可选，可 NULL — 未识别 key 返回 0）
    // type: 0=string 1=int 2=bool，value 为统一指针（const char* / int*）
    int                  (*setBackendConfig)(UIRenderDeviceHandle, const char* key, int type, const void* value);
    int                  (*getBackendConfig)(UIRenderDeviceHandle, const char* key, int type, void* value, int maxLen);
    // 纹理（可选，可 NULL—图片不可用时跳过）
    UITextureHandle      (*createTextureFromFile)(UIRenderDeviceHandle, const char* path);
    void                 (*destroyTexture)(UIRenderDeviceHandle, UITextureHandle);
    void                 (*drawTexture)(UIRenderDeviceHandle, UITextureHandle, const UIRect* src, const UIRect* dst);
    void                 (*getTextureSize)(UITextureHandle, int* w, int* h);

    // --- InputBackend (必须) ---
    UIInputBackendHandle (*createInputBackend)(void* nativeWindowHandle);
    void                 (*destroyInputBackend)(UIInputBackendHandle);
    int                  (*pollEvent)(UIInputBackendHandle, UIEvent* evt);
    void                 (*startTextInput)(UIInputBackendHandle);
    void                 (*stopTextInput)(UIInputBackendHandle);
    int                  (*getModState)(UIInputBackendHandle);
    void                 (*setClipboardText)(UIInputBackendHandle, const char* text);
    int                  (*getClipboardText)(UIInputBackendHandle, char* buf, int maxLen);

    // --- per-frame tick (可选，可为 NULL) ---
    // 在每个 ProcessEvents 开头被调用。raylib 后端用它调用 PollInputEvents()。
    void                 (*newFrame)(UIInputBackendHandle);

    // --- TextRenderer (必须) ---
    UITextRendererHandle (*createTextRenderer)(UIRenderDeviceHandle);
    void                 (*destroyTextRenderer)(UITextRendererHandle);
    UIFontHandle         (*loadFont)(UITextRendererHandle, const char* path, float size);
    UIFontHandle         (*loadFontFromMemory)(UITextRendererHandle, const void* data, int len, float size);
    void                 (*destroyFont)(UITextRendererHandle, UIFontHandle);
    float                (*measureTextWidth)(UITextRendererHandle, UIFontHandle, const char* text);
    float                (*getFontHeight)(UITextRendererHandle, UIFontHandle);
    void                 (*drawText)(UITextRendererHandle, UIFontHandle, const char* text, float x, float y, UIColor color);
    void                 (*drawTextWrapped)(UITextRendererHandle, UIFontHandle, const char* text, float x, float y, float wrapWidth, UIColor color);

    // --- filled triangle/quad (可选，可为 NULL — 退化到轮廓绘制) ---
    void (*fillTriangle)(UIRenderDeviceHandle, float x0, float y0, float x1, float y1, float x2, float y2, UIColor color);
    void (*fillQuad)(UIRenderDeviceHandle, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, UIColor color);

    // --- Cursor factories (可选 — 可为 NULL；禁用光标反馈) ---
    void* (*createSystemCursor)(int type);
    void* (*getDefaultCursor)();
    void  (*setCurrentCursor)(void* cursor);

    // --- ResourceProvider (可选，可为 NULL) ---
    UIResourceProviderHandle (*createResourceProvider)(const char* basePath);
    void                     (*destroyResourceProvider)(UIResourceProviderHandle);
    int                      (*readFile)(UIResourceProviderHandle, const char* path, void* buf, int maxLen);
    int                      (*fileExists)(UIResourceProviderHandle, const char* path);
} UIBackendCallbacks;

/* ============ 实例生命周期 ============ */
/* 创建实例：alloc + init 一次性完成。失败返回 NULL。 */
UICORNERSTONE_API UIInstance UICornerstone_CreateInstance(
    const UIBackendCallbacks* callbacks,
    const UIInstanceConfig* config);       /* NULL → 全默认 */

/* 销毁实例：级联销毁子视口，owner 才 shutdown BackendManager。NULL 安全。 */
UICORNERSTONE_API void UICornerstone_DestroyInstance(UIInstance instance);

/* 从后端插件 DLL（UIBackend_<pluginName>.dll）创建实例。
   静态链接路径回退 GetUIBackendCallbacks。 */
UICORNERSTONE_API UIInstance UICornerstone_CreateInstanceFromPlugin(
    const char* pluginName,
    const UIInstanceConfig* config);

/* ============ 后端配置 ============ */
/* 后端键值配置（vsync / swap-ratio / renderer-name 等，各后端支持子集见文档）。
   inst == NULL → 设置/查询全局后端默认值，在后续 CreateInstance 创建 renderer 前生效
                  （对应 raylib FLAG_VSYNC_HINT 这类须创建期生效的参数）；
   inst != NULL → 设置/查询当前实例的 connecter 运行期可调参数（sdl3/sfml 的 vsync 可运行期切换）。
   成功返回 1；未识别的 key / 后端不支持返回 0。 */
UICORNERSTONE_API int UICornerstone_SetBackendConfig(UIInstance inst, const char* key, const char* value);
UICORNERSTONE_API int UICornerstone_SetBackendConfigInt(UIInstance inst, const char* key, int value);
UICORNERSTONE_API int UICornerstone_SetBackendConfigBool(UIInstance inst, const char* key, int value);
UICORNERSTONE_API int UICornerstone_GetBackendConfig(UIInstance inst, const char* key, char* value, int maxLen);
UICORNERSTONE_API int UICornerstone_GetBackendConfigInt(UIInstance inst, const char* key, int* value);
UICORNERSTONE_API int UICornerstone_GetBackendConfigBool(UIInstance inst, const char* key, int* value);

/* 在父实例（owner）的窗口中创建子视口：共享后端，独立控制树/事件队列/DataContext。
   rect 为窗口坐标系下的视口区域。 */
UICORNERSTONE_API UIInstance UICornerstone_CreateViewport(
    UIInstance parent, UIRect rect);

/* ============ 视口控制 ============ */
UICORNERSTONE_API void UICornerstone_SetViewport(UIInstance instance, float x, float y, float w, float h);
UICORNERSTONE_API void UICornerstone_GetViewport(UIInstance instance, float* x, float* y, float* w, float* h);

/* ============ 帧循环 ============ */
UICORNERSTONE_API void UICornerstone_ProcessEvents(UIInstance instance);
UICORNERSTONE_API void UICornerstone_Update(UIInstance instance, double deltaTime);

// 注入外部事件（例如从 SDL_AppEvent 回调传入的 SDL 事件）。
// 事件会在下一次 UICornerstone_ProcessEvents 中被处理。
UICORNERSTONE_API void UICornerstone_PushUIEvent(UIInstance instance, const UIEvent* ue);

// 只渲染视口区域。不清除帧缓冲区、不 present。
// 调用者必须在外层自行 clear + render + present。
UICORNERSTONE_API void UICornerstone_Render(UIInstance instance);

// 清除帧缓冲区和翻转缓冲区（可选，仅在调用者不自行管理帧时使用）
UICORNERSTONE_API void UICornerstone_Clear(UIInstance instance);
UICORNERSTONE_API void UICornerstone_Present(UIInstance instance);

UICORNERSTONE_API int  UICornerstone_IsQuitRequested(UIInstance instance);

/* ============ Debug 辅助 ============ */
// 存活实例数（Release 构建返回 0）
UICORNERSTONE_API int      UICornerstone_Debug_GetAliveCount(void);
// 按索引取存活实例（Release 构建返回 NULL）
UICORNERSTONE_API UIInstance UICornerstone_Debug_GetAliveInstance(int index);
// 查询当前活动视口（子视口为 null；viewport 实例查询返回 owner 的活动视口）
UICORNERSTONE_API UIInstance UICornerstone_Debug_GetActiveViewport(UIInstance instance);
// 查询控件是否拥有焦点
UICORNERSTONE_API int      UICornerstone_Debug_IsControlFocused(UIInstance instance, UIControlHandle control);

/* ============ 布局系统 ============ */
UICORNERSTONE_API int               UICornerstone_LoadLayout(UIInstance instance, const char* jsonContent);
UICORNERSTONE_API int               UICornerstone_LoadLayoutFromFile(UIInstance instance, const char* filePath);
UICORNERSTONE_API UIControlHandle   UICornerstone_FindControl(UIInstance instance, const char* id);

typedef void (*UIActionCallback)(UIControlHandle ctl, void* userData);
UICORNERSTONE_API void UICornerstone_RegisterAction(UIInstance instance, const char* name, UIActionCallback cb, void* userData);

/* ============ 编程式控件创建 ============ */
UICORNERSTONE_API UIControlHandle UICornerstone_CreateButton(UIInstance instance, const char* text,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateLabel(UIInstance instance, const char* text, float fontSize,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateCheckBox(UIInstance instance, const char* text,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateEditBox(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateProgressBar(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateSlider(UIInstance instance,
    float x, float y, float w, float h, float min, float max, float value);
UICORNERSTONE_API UIControlHandle UICornerstone_CreatePanel(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateTextArea(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateWinFrame(UIInstance instance,
    const char* title, float x, float y, float w, float h);
/* ============ Menu 三件套（MenuBar / MenuPanel / MenuItem） ============ */
// Menu 是一组控件：MenuBar 为顶层菜单栏，MenuPanel 为下拉面板，
// MenuItem 为菜单项。组装顺序：
//   panel = CreateMenuPanel(inst);  item = CreateMenuItem(inst, "Open", 0);
//   MenuPanelAddItem(inst, panel, item);  MenuItemSetSubMenu(inst, item, subPanel);
//   bar = CreateMenuBar(inst, ...);  MenuBarAddMenu(inst, bar, "File", panel);
// type: 0=Normal, 1=Separator, 2=SubMenu
// MenuItem 的 caption/checked/shortcut/click 走统一属性系统
// （SetString/SetBool/SetCallback，事件名 "click"）
UICORNERSTONE_API UIControlHandle UICornerstone_CreateMenuBar(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateMenuPanel(UIInstance instance);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateMenuItem(UIInstance instance, const char* caption, int type);
UICORNERSTONE_API void UICornerstone_MenuBarAddMenu(UIInstance instance,
    UIControlHandle bar, const char* caption, UIControlHandle panel);
UICORNERSTONE_API void UICornerstone_MenuPanelAddItem(UIInstance instance,
    UIControlHandle panel, UIControlHandle item);
UICORNERSTONE_API void UICornerstone_MenuPanelAddSeparator(UIInstance instance, UIControlHandle panel);
UICORNERSTONE_API void UICornerstone_MenuItemSetSubMenu(UIInstance instance,
    UIControlHandle item, UIControlHandle panel);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateColorPicker(UIInstance instance,
    float x, float y, float w, float h, const char* color);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateNumericUpDown(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateComboBox(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateSplitter(UIInstance instance,
    float x, float y, float w, float h, int orientation);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateScrollBar(UIInstance instance,
    float x, float y, float w, float h, int orientation);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateTreeView(UIInstance instance,
    float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateHandleControl(UIInstance instance,
    UIControlHandle target, float x, float y, float w, float h);
UICORNERSTONE_API UIControlHandle UICornerstone_CreateImageButton(UIInstance instance,
    const char* normalImage,
    const char* hoverImage,
    const char* pressedImage,
    float x, float y, float w, float h);

/* ============ Image 图片控件 ============ */
// image 为文件路径（可为 NULL，之后经 UICornerstone_SetString(inst, ctl, "image", path) 设置；
// 资源 ID 经 "image-resource" 设置）。w/h 传 0 表示按纹理自然尺寸。
UICORNERSTONE_API UIControlHandle UICornerstone_CreateImage(
    UIInstance instance,
    const char* image,
    float x, float y, float w, float h);

/* ============ LuotiAni 动画控件 ============ */
// jsoncPath 为动画描述文件路径（相对路径经基路径拼接），可为 NULL（之后经
// UICornerstone_SetString(inst, ctl, "animation", path) 设置）；
// 创建后不自动播放（显式经 UICornerstone_SetBool(inst, ctl, "playing", 1) 控制）；
// w/h 传 0 → prepare 回退到 JSON overview.view 画布尺寸；加载失败返回 NULL。
UICORNERSTONE_API UIControlHandle UICornerstone_CreateAnimation(
    UIInstance instance,
    const char* jsoncPath,
    float x, float y, float w, float h);

/* ============ 控件通用操作 ============ */
UICORNERSTONE_API void UICornerstone_SetRect(UIInstance instance, UIControlHandle ctl, float x, float y, float w, float h);
UICORNERSTONE_API void UICornerstone_GetRect(UIInstance instance, UIControlHandle ctl, float* x, float* y, float* w, float* h);
UICORNERSTONE_API void UICornerstone_AddChildControl(UIInstance instance, UIControlHandle parent, UIControlHandle child);
UICORNERSTONE_API void UICornerstone_DestroyControl(UIInstance instance, UIControlHandle ctl);
UICORNERSTONE_API const char* UICornerstone_GetControlId(UIInstance instance, UIControlHandle ctl);

/* ============ Dialog/Popup ============ */
UICORNERSTONE_API UIControlHandle UICornerstone_CreateDialog(UIInstance instance,
    const char* confirmText, const char* cancelText,
    float x, float y, float w, float h);

/* ============ 属性系统 (统一字符串名 + 多类型入口) ============ */

/* ── Setter ── */
// 设置单色属性。prop 是属性名，如 "selected"、"hover"、"background"
// 返回 1 成功，0 不识别的属性
UICORNERSTONE_API int UICornerstone_SetColor(   UIInstance instance, UIControlHandle ctl, const char* prop, UIColor       value);

// 设置 4 态颜色属性。prop 如 "background"、"border"、"text"
UICORNERSTONE_API int UICornerstone_SetStateColor( UIInstance instance, UIControlHandle ctl, const char* prop, UIStateColor  value);

// 设置布尔属性。prop 如 "visible"、"enabled"，value: 0=假, 非0=真
UICORNERSTONE_API int UICornerstone_SetBool(     UIInstance instance, UIControlHandle ctl, const char* prop, int           value);

// 设置整数属性。prop 如 "font-size"
UICORNERSTONE_API int UICornerstone_SetInt(     UIInstance instance, UIControlHandle ctl, const char* prop, int           value);

// 设置浮点属性。prop 如 "indent-width"、"row-height"
UICORNERSTONE_API int UICornerstone_SetFloat(   UIInstance instance, UIControlHandle ctl, const char* prop, float         value);

// 设置字符串属性。prop 如 "text"、"font"
UICORNERSTONE_API int UICornerstone_SetString(  UIInstance instance, UIControlHandle ctl, const char* prop, const char*   value);

// 设置枚举属性。value 为枚举值名称字符串，如 "vertical"、"checked"
UICORNERSTONE_API int UICornerstone_SetEnum(    UIInstance instance, UIControlHandle ctl, const char* prop, const char*   value);

// 设置指针属性。prop 如 "content"、"first-linked"，value 为 UIControlHandle
UICORNERSTONE_API int UICornerstone_SetPtr(     UIInstance instance, UIControlHandle ctl, const char* prop, void*         value);

/* ── Getter ── */
// 读取属性值。返回 1 成功，0 属性不识别
UICORNERSTONE_API int UICornerstone_GetColor(   UIInstance instance, UIControlHandle ctl, const char* prop, UIColor*       out);
UICORNERSTONE_API int UICornerstone_GetStateColor( UIInstance instance, UIControlHandle ctl, const char* prop, UIStateColor*  out);
UICORNERSTONE_API int UICornerstone_GetBool(     UIInstance instance, UIControlHandle ctl, const char* prop, int*           out);
UICORNERSTONE_API int UICornerstone_GetInt(     UIInstance instance, UIControlHandle ctl, const char* prop, int*           out);
UICORNERSTONE_API int UICornerstone_GetFloat(   UIInstance instance, UIControlHandle ctl, const char* prop, float*         out);
UICORNERSTONE_API int UICornerstone_GetString(  UIInstance instance, UIControlHandle ctl, const char* prop, char* out, int maxLen);
UICORNERSTONE_API int UICornerstone_GetEnum(    UIInstance instance, UIControlHandle ctl, const char* prop, char* out, int maxLen);

// 读取指针属性。out 返回属性值。返回 1 成功，0 属性不识别
UICORNERSTONE_API int UICornerstone_GetPtr(     UIInstance instance, UIControlHandle ctl, const char* prop, void**        out);

/* ── Callback ── */
typedef struct {
    const char* eventName;
    union {
        float           floatVal;
        double          doubleVal;
        int             intVal;
        const char*     strVal;
        void*           ptrVal;
        struct { int idx; const char* val; } selection;
        struct { const char* id; void* userData; } treeNode;
        struct { uint8_t r,g,b,a; } color;
    } data;
} UIEventData;

typedef void (*UIEventCallback)(UIControlHandle ctl, const UIEventData* event, void* userData);

// 绑定事件回调。event 为事件名，如 "click"、"value-changed"
UICORNERSTONE_API int UICornerstone_SetCallback(UIInstance instance, UIControlHandle ctl, const char* event, UIEventCallback cb, void* userData);

#ifdef __cplusplus
}
#endif

#endif // UICORNERSTONE_API_H

