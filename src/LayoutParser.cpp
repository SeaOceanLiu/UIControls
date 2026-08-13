// 由AI(DeepSeek V4 Flash)生成，可能不完整或有错误，请自行检查和修改
#include "LayoutParser.h"
#include "Bench.h"
#include "WinFrame.h"
#include "Dialog.h"
#include "ComboBox.h"
#include "NumericUpDown.h"
#include "LuotiAni.h"
#include "LayoutEngine.h"
#include "PropertyNames.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include "PlatformUtils.h"

LayoutParser::LayoutParser(DataContext* dataContext)
    : m_dataContext(dataContext), m_currentLineNo(0)
{
}

// ==================== 布局加载 ====================

shared_ptr<Control> LayoutParser::parseLayout(const string& jsonContent) {
    m_rawJsonContent = jsonContent;
    m_currentLineNo = 1;
    m_currentJsonPath = "root";

    json j;
    try {
        j = json::parse(jsonContent);
    } catch (const json::parse_error& e) {
        int lineNo = byteOffsetToLineNo(jsonContent, e.byte);
        Platform::Log("[LayoutParser] [Line %d] [root] ERROR: JSON parse error: %s",
            lineNo, e.what());
        return nullptr;
    }

    if (j.contains(PropertyNames::kJsonTheme) && j[PropertyNames::kJsonTheme].is_object()) {
        m_theme.parse(j[PropertyNames::kJsonTheme]);
    }

    // 顶层 viewport 键：显式基准画布 + 初始缩放模式（先 canvas 后 mode，
    // mode 应用时按 canvas 重算根变换）；无实例目标（m_viewportTarget）时忽略
    if (m_viewportTarget != nullptr &&
        j.contains(PropertyNames::kJsonViewport) && j[PropertyNames::kJsonViewport].is_object()) {
        const json& vp = j[PropertyNames::kJsonViewport];
        if (vp.contains(PropertyNames::kJsonWidth) && vp.contains(PropertyNames::kJsonHeight)) {
            float w = vp[PropertyNames::kJsonWidth].get<float>();
            float h = vp[PropertyNames::kJsonHeight].get<float>();
            if (w > 0.0f && h > 0.0f) {
                m_viewportTarget->canvasWidth = w;
                m_viewportTarget->canvasHeight = h;
                // 三层模型：画布基准写入 bench rect（布局空间）；
                // off 下画布恒跟随视口，仅记录声明不生效
                if (m_viewportTarget->bench &&
                    m_viewportTarget->bench->getViewportScaleMode() != Bench::ViewportScaleMode::Off) {
                    m_viewportTarget->bench->setRect(SRect(0, 0, w, h));
                }
            }
        }
        if (m_viewportTarget->bench &&
            vp.contains(PropertyNames::kJsonViewportScaleMode) &&
            vp[PropertyNames::kJsonViewportScaleMode].is_string()) {
            std::string sm = vp[PropertyNames::kJsonViewportScaleMode].get<std::string>();
            int mode = 0;
            if (sm == "fit") mode = 1;
            else if (sm == "stretch") mode = 2;
            m_viewportTarget->bench->setViewportScaleMode(
                static_cast<Bench::ViewportScaleMode>(mode));
        }
    }

    // Component system: parse components before layouts
    if (j.contains(PropertyNames::kJsonComponents)) {
        parseComponents(j);
    }

    // Support both "layouts" (component system) and "controls" (original)
    const json* controlsArray = nullptr;
    if (j.contains(PropertyNames::kJsonLayouts) && j[PropertyNames::kJsonLayouts].is_array()) {
        controlsArray = &j[PropertyNames::kJsonLayouts];
    } else if (j.contains(PropertyNames::kJsonControls) && j[PropertyNames::kJsonControls].is_array()) {
        controlsArray = &j[PropertyNames::kJsonControls];
    }

    if (!controlsArray) {
        logError("'controls' or 'layouts' array is required");
        return nullptr;
    }

    const json& controls = *controlsArray;
    if (controls.empty()) {
        logWarn("'controls' or 'layouts' array is empty");
        return nullptr;
    }

    shared_ptr<Control> root = nullptr;
    for (size_t i = 0; i < controls.size(); ++i) {
        auto ctrl = parseControl(controls[i], nullptr, (int)i);
        if (ctrl) {
            root = ctrl;
        }
    }

    // 解析独立的 Dialogs（从 controls 树外管理）
    if (j.contains(PropertyNames::kJsonDialogs) && j[PropertyNames::kJsonDialogs].is_array()) {
        const json& dialogs = j[PropertyNames::kJsonDialogs];
        for (size_t i = 0; i < dialogs.size(); ++i) {
            auto dlg = parseControl(dialogs[i], nullptr, (int)i);
            if (auto pop = dynamic_pointer_cast<Popup>(dlg)) {
                m_dialogs.push_back(pop);
            }
        }
    }
    return root;
}

shared_ptr<Control> LayoutParser::parseLayoutFile(const fs::path& jsonPath) {
    if (!fs::exists(jsonPath)) {
        Platform::Log("[LayoutParser] ERROR: Layout file not found: %s",
            jsonPath.string().c_str());
        return nullptr;
    }

    ifstream file(jsonPath);
    if (!file.is_open()) {
        Platform::Log("[LayoutParser] ERROR: Failed to open layout file: %s",
            jsonPath.string().c_str());
        return nullptr;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    file.close();

    return parseLayout(content);
}

// ==================== ID 查找 ====================

shared_ptr<Control> LayoutParser::findControlById(const string& id) {
    auto it = m_controlsById.find(id);
    if (it != m_controlsById.end()) {
        return it->second;
    }
    return nullptr;
}

vector<string> LayoutParser::getAllControlIds() const {
    vector<string> ids;
    ids.reserve(m_controlsById.size());
    for (const auto& pair : m_controlsById) {
        ids.push_back(pair.first);
    }
    return ids;
}

// ==================== 处理器注册 ====================

void LayoutParser::registerHandler(const string& name,
                                    function<void(shared_ptr<Control>)> handler) {
    m_handlers[name] = move(handler);
}

void LayoutParser::unregisterHandler(const string& name) {
    m_handlers.erase(name);
}

void LayoutParser::clearHandlers() {
    m_handlers.clear();
}

// ==================== 状态管理 ====================

void LayoutParser::clear() {
    if (m_dataContext) m_dataContext->unwatchAll();
    m_controlsById.clear();
    m_menuBars.clear();
    m_dialogs.clear();
    m_currentJsonPath.clear();
    m_currentLineNo = 0;
    m_rawJsonContent.clear();
    m_components.clear();
    m_componentSourceLines.clear();
    m_instantiationStack.clear();
}

void LayoutParser::reset() {
    clear();
    m_handlers.clear();
}

const vector<shared_ptr<MenuBar>>& LayoutParser::getMenuBars() const {
    return m_menuBars;
}

// ==================== 错误追踪 ====================

void LayoutParser::logError(const string& message) const {
    Platform::Log("[LayoutParser] [Line %d] [%s] ERROR: %s",
        m_currentLineNo, m_currentJsonPath.c_str(), message.c_str());
}

void LayoutParser::logWarn(const string& message) const {
    Platform::Log("[LayoutParser] [Line %d] [%s] WARN: %s",
        m_currentLineNo, m_currentJsonPath.c_str(), message.c_str());
}

void LayoutParser::pushJsonPath(const string& segment) {
    if (m_currentJsonPath.empty() || m_currentJsonPath == "root") {
        m_currentJsonPath = segment;
    } else {
        m_currentJsonPath += "." + segment;
    }
}

void LayoutParser::popJsonPath() {
    auto pos = m_currentJsonPath.rfind('.');
    if (pos != string::npos) {
        m_currentJsonPath = m_currentJsonPath.substr(0, pos);
    } else {
        m_currentJsonPath.clear();
    }
}

int LayoutParser::byteOffsetToLineNo(const string& content, size_t byteOffset) const {
    int line = 1;
    size_t limit = min(byteOffset, content.size());
    for (size_t i = 0; i < limit; ++i) {
        if (content[i] == '\n') {
            line++;
        }
    }
    return line;
}

// ==================== 控件工厂 ====================

shared_ptr<Control> LayoutParser::parseControl(const json& j, Control* parent, int index) {
    string indexPath = "controls[" + to_string(index) + "]";
    pushJsonPath(indexPath);

    if (!j.contains(PropertyNames::kJsonType) || !j[PropertyNames::kJsonType].is_string()) {
        logError("'type' field is required");
        popJsonPath();
        return nullptr;
    }

    string type = j[PropertyNames::kJsonType].get<string>();

    shared_ptr<Control> result = nullptr;
    if (type == PropertyNames::kControlTypeLabel) {
        result = parseLabel(j, parent);
    } else if (type == PropertyNames::kControlTypeButton) {
        result = parseButton(j, parent);
    } else if (type == PropertyNames::kControlTypeImageButton) {
        // image-button：与 button 同语法（caption/actors/styles/scale/events），
        // 图片经 "actors" 或状态图属性设置；别名避免与普通按钮语义混淆
        result = parseButton(j, parent);
    } else if (type == PropertyNames::kControlTypeAnimation) {
        result = parseAnimation(j, parent);
    } else if (type == PropertyNames::kControlTypeEditBox) {
        result = parseEditBox(j, parent);
    } else if (type == PropertyNames::kControlTypeComboBox) {
        result = parseComboBox(j, parent);
    } else if (type == PropertyNames::kControlTypeTextArea) {
        result = parseTextArea(j, parent);
    } else if (type == PropertyNames::kControlTypeCheckBox) {
        result = parseCheckBox(j, parent);
    } else if (type == PropertyNames::kControlTypeProgressBar) {
        result = parseProgressBar(j, parent);
    } else if (type == PropertyNames::kControlTypeSlider) {
        result = parseSlider(j, parent);
    } else if (type == PropertyNames::kControlTypeScrollBar) {
        result = parseScrollBar(j, parent);
    } else if (type == PropertyNames::kControlTypePanel) {
        result = parsePanel(j, parent);
    } else if (type == PropertyNames::kControlTypeWinFrame) {
        result = parseWinFrame(j, parent);
    } else if (type == PropertyNames::kControlTypeColorPicker) {
        result = parseColorPicker(j, parent);
    } else if (type == PropertyNames::kControlTypePopup) {
        result = parsePopup(j, parent);
    } else if (type == PropertyNames::kControlTypeConfirmPopup) {
        result = parseConfirmPopup(j, parent);
    } else if (type == PropertyNames::kControlTypeDialog) {
        result = parseDialog(j, parent);
    } else if (type == PropertyNames::kControlTypeMenuBar) {
        // MenuBar 不加入控件树（会被父容器裁剪），独立存储后再由调用方加入 BENCH 顶层
        auto menuBar = parseMenuBar(j, parent);
        if (menuBar) {
            m_menuBars.push_back(menuBar);
        }
        result = nullptr;
        popJsonPath();
        return nullptr;
    } else if (type == PropertyNames::kControlTypeNumericUpDown) {
        result = parseNumericUpDown(j, parent);
    } else if (type == PropertyNames::kControlTypeSplitter) {
        result = parseSplitter(j, parent);
    } else if (type == PropertyNames::kControlTypeTreeView) {
        result = parseTreeView(j, parent);
    } else if (m_components.find(type) != m_components.end()) {
        // Component type: instantiate from template
        result = instantiateComponent(type, j, parent, index);
    } else {
        logWarn("unknown control type \"" + type + "\", skipping");
        popJsonPath();
        return nullptr;
    }

    popJsonPath();
    return result;
}

// ==================== Label ====================

shared_ptr<Label> LayoutParser::parseLabel(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto label = make_shared<Label>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(label, PropertyNames::kThemeCatLabel);
    m_theme.applyFont(label, PropertyNames::kThemeCatLabel);
    parseCommonProperties(label, j);

    // caption
    if (j.contains(PropertyNames::kJsonCaption) && j[PropertyNames::kJsonCaption].is_string()) {
        label->setCaption(j[PropertyNames::kJsonCaption].get<string>());
    }

    // alignment
    if (j.contains(PropertyNames::kJsonAlignment) && j[PropertyNames::kJsonAlignment].is_string()) {
        label->setAlignmentMode(parseAlignment(j[PropertyNames::kJsonAlignment].get<string>()));
    }

    // font
    if (j.contains(PropertyNames::kJsonFont) && j[PropertyNames::kJsonFont].is_object()) {
        pushJsonPath(PropertyNames::kJsonFont);
        const json& font = j[PropertyNames::kJsonFont];
        if (font.contains(PropertyNames::kJsonName) && font[PropertyNames::kJsonName].is_string()) {
            label->setFont(parseFontName(font[PropertyNames::kJsonName].get<string>()));
        }
        if (font.contains(PropertyNames::kJsonSize) && font[PropertyNames::kJsonSize].is_number()) {
            label->setFontSize(font[PropertyNames::kJsonSize].get<int>());
        }
        if (font.contains(PropertyNames::kJsonStyle) && font[PropertyNames::kJsonStyle].is_string()) {
            label->SetFontStyle(parseFontStyle(font[PropertyNames::kJsonStyle].get<string>()));
        }
        popJsonPath();
    }

    // shadow
    if (j.contains(PropertyNames::kJsonShadow) && j[PropertyNames::kJsonShadow].is_object()) {
        pushJsonPath(PropertyNames::kJsonShadow);
        const json& shadow = j[PropertyNames::kJsonShadow];
        label->setShadow(shadow.value(PropertyNames::kJsonEnabled, false));
        if (shadow.contains(PropertyNames::kJsonOffset) && shadow[PropertyNames::kJsonOffset].is_object()) {
            float ox = shadow[PropertyNames::kJsonOffset].value(PropertyNames::kJsonX, 1.0f);
            float oy = shadow[PropertyNames::kJsonOffset].value(PropertyNames::kJsonY, 1.0f);
            label->setShadowOffset(SPoint(ox, oy));
        }
        popJsonPath();
    }

    // lineHeight
    if (j.contains(PropertyNames::kJsonLineHeight) && j[PropertyNames::kJsonLineHeight].is_number()) {
        label->setLineHeight(j[PropertyNames::kJsonLineHeight].get<int>());
    }

    // lineSpacingRatio
    if (j.contains(PropertyNames::kJsonLineSpacingRatio) && j[PropertyNames::kJsonLineSpacingRatio].is_number()) {
        label->setLineSpacingRatio(j[PropertyNames::kJsonLineSpacingRatio].get<float>());
    }

    // enableExpand
    if (j.contains(PropertyNames::kJsonEnableExpand) && j[PropertyNames::kJsonEnableExpand].is_boolean()) {
        label->setEnableExpand(j[PropertyNames::kJsonEnableExpand].get<bool>());
    }

    // debugDraw
    if (j.contains(PropertyNames::kJsonDebugDraw) && j[PropertyNames::kJsonDebugDraw].is_boolean()) {
        label->setDebugDraw(j[PropertyNames::kJsonDebugDraw].get<bool>());
    }

    // events
    parseEvents(label, j);
    parseBindings(label, j);

    // id
    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = label;
    }

    label->create();

    return label;
}

// ==================== Animation (LuotiAni) ====================

shared_ptr<Control> LayoutParser::parseAnimation(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto ani = make_shared<LuotiAni>(parent, xScale, yScale);
    ani->setRect(rect);
    m_theme.applyCommonColors(ani, PropertyNames::kThemeCatPanel);
    parseCommonProperties(ani, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = ani;
    }

    // 动画描述文件路径（"path"）；w/h 传 0 → prepare 回退到画布尺寸。
    // parse 阶段尚无渲染设备：只解析描述（loadFromFile），挂树后由
    // LuotiAni::setRenderDevice 补 prepare（与 C ABI CreateAnimation 一致：
    // 创建后不自动播放，SetBool "playing" 控制）
    if (j.contains(PropertyNames::kJsonPath) && j[PropertyNames::kJsonPath].is_string()) {
        string p = j[PropertyNames::kJsonPath].get<string>();
        fs::path fp(p);
        if (fp.is_relative()) fp = fs::path(Platform::GetBasePath()) / fp;
        try {
            ani->loadFromFile(fp);
        } catch (...) {
            logWarn("animation load failed: " + p);
            return nullptr;
        }
    }
    return ani;
}

// ==================== Button ====================

shared_ptr<Button> LayoutParser::parseButton(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto btn = make_shared<Button>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(btn, PropertyNames::kThemeCatButton);
    parseCommonProperties(btn, j);

    // captionLabel embedding (Phase 2): 使用完整 Label 配置
    if (j.contains("captionLabel") && j[PropertyNames::kJsonCaptionLabel].is_object()) {
        pushJsonPath(PropertyNames::kJsonCaptionLabel);

        const json& cl = j[PropertyNames::kJsonCaptionLabel];

        auto builder = LabelBuilder(btn.get(), SRect(0, 0, rect.width, rect.height));

        builder.setFont(m_theme.getFontName(PropertyNames::kThemeCatButton));
        builder.setFontSize(m_theme.getFontSize(PropertyNames::kThemeCatButton));

        if (cl.contains(PropertyNames::kJsonCaption) && cl[PropertyNames::kJsonCaption].is_string()) {
            builder.setCaption(cl[PropertyNames::kJsonCaption].get<string>());
        }
        if (cl.contains(PropertyNames::kJsonFont) && cl[PropertyNames::kJsonFont].is_object()) {
            const json& font = cl[PropertyNames::kJsonFont];
            if (font.contains(PropertyNames::kJsonName) && font[PropertyNames::kJsonName].is_string()) {
                builder.setFont(parseFontName(font[PropertyNames::kJsonName].get<string>()));
            }
            if (font.contains(PropertyNames::kJsonSize) && font[PropertyNames::kJsonSize].is_number()) {
                builder.setFontSize(font[PropertyNames::kJsonSize].get<int>());
            }
            if (font.contains(PropertyNames::kJsonStyle) && font[PropertyNames::kJsonStyle].is_string()) {
                builder.SetFontStyle(parseFontStyle(font[PropertyNames::kJsonStyle].get<string>()));
            }
        }

        if (cl.contains(PropertyNames::kJsonAlignment) && cl[PropertyNames::kJsonAlignment].is_string()) {
            builder.setAlignmentMode(parseAlignment(cl[PropertyNames::kJsonAlignment].get<string>()));
        }

        if (cl.contains(PropertyNames::kJsonShadow) && cl[PropertyNames::kJsonShadow].is_object()) {
            const json& shadow = cl[PropertyNames::kJsonShadow];
            if (shadow.contains(PropertyNames::kJsonEnabled) && shadow[PropertyNames::kJsonEnabled].is_boolean()) {
                builder.setShadow(shadow[PropertyNames::kJsonEnabled].get<bool>());
            }
            if (shadow.contains(PropertyNames::kJsonOffset) && shadow[PropertyNames::kJsonOffset].is_object()) {
                SPoint offset;
                offset.x = shadow[PropertyNames::kJsonOffset].value(PropertyNames::kJsonX, 1);
                offset.y = shadow[PropertyNames::kJsonOffset].value(PropertyNames::kJsonY, 1);
                builder.setShadowOffset(offset);
            }
        }

        if (cl.contains(PropertyNames::kJsonColors) && cl[PropertyNames::kJsonColors].is_object()) {
            const json& colors = cl[PropertyNames::kJsonColors];
            if (colors.contains(PropertyNames::kJsonText) && colors[PropertyNames::kJsonText].is_object()) {
                builder.setTextStateColor(parseStateColor(colors[PropertyNames::kJsonText], StateColor::Type::Text));
            }
            if (colors.contains(PropertyNames::kJsonTextShadow) && colors[PropertyNames::kJsonTextShadow].is_object()) {
                builder.setTextShadowStateColor(parseStateColor(colors[PropertyNames::kJsonTextShadow], StateColor::Type::TextShadow));
            }
        }

        auto label = builder.build();
        btn->setCaptionLabel(label);

        popJsonPath();
    } else {
        // 简单方式 (Phase 1): 仅 caption / captionSize / enableTextShadow
        int themeFontSize = m_theme.getFontSize(PropertyNames::kThemeCatButton);
        if (themeFontSize != 16) {
            btn->setCaptionSize((float)themeFontSize);
        }

        if (j.contains(PropertyNames::kJsonCaption) && j[PropertyNames::kJsonCaption].is_string()) {
            btn->setCaption(j[PropertyNames::kJsonCaption].get<string>());
        }

        if (j.contains(PropertyNames::kJsonCaptionSize) && j[PropertyNames::kJsonCaptionSize].is_number()) {
            btn->setCaptionSize(j[PropertyNames::kJsonCaptionSize].get<float>());
        }

        if (j.contains("enableTextShadow") && j[PropertyNames::kJsonEnableTextShadow].is_boolean()) {
            btn->setTextShadowEnable(j[PropertyNames::kJsonEnableTextShadow].get<bool>());
        }
    }

    // Actors (state images)
    if (j.contains(PropertyNames::kJsonActors) && j[PropertyNames::kJsonActors].is_object()) {
        pushJsonPath(PropertyNames::kJsonActors);
        const json& actors = j[PropertyNames::kJsonActors];

        bool matchRect = actors.value(PropertyNames::kJsonMatchParentRect, false);

        auto createActor = [&](const json& v) -> shared_ptr<Actor> {
            if (v.is_null()) return nullptr;
            string filePath;
            string resourceId;
            ScaleType scaleType = ScaleType::STRETCH;
            if (v.is_string()) {
                filePath = v.get<string>();
            } else if (v.is_object()) {
                if (v.contains(PropertyNames::kJsonFile) && v[PropertyNames::kJsonFile].is_string()) {
                    filePath = v[PropertyNames::kJsonFile].get<string>();
                }
                if (v.contains(PropertyNames::kJsonResourceId) && v[PropertyNames::kJsonResourceId].is_string()) {
                    resourceId = v[PropertyNames::kJsonResourceId].get<string>();
                }
                if (v.contains(PropertyNames::kJsonScaleType) && v[PropertyNames::kJsonScaleType].is_string()) {
                    string st = v[PropertyNames::kJsonScaleType].get<string>();
                    if (st == PropertyNames::kScaleTypeFitCenter)      scaleType = ScaleType::FIT_CENTER;
                    else if (st == PropertyNames::kScaleTypeCenterCrop) scaleType = ScaleType::CENTER_CROP;
                    else if (st == PropertyNames::kScaleTypeNone)        scaleType = ScaleType::NONE;
                }
            }
            shared_ptr<Actor> actor = nullptr;
            if (!filePath.empty()) {
                actor = make_shared<Actor>(btn.get(), fs::path(filePath), matchRect, 1.0f, 1.0f);
            } else if (!resourceId.empty()) {
                actor = make_shared<Actor>(btn.get(), resourceId, matchRect, 1.0f, 1.0f);
            }
            if (actor) {
                actor->setScaleType(scaleType);
            }
            return actor;
        };

        if (actors.contains(PropertyNames::kStateKeyNormal) && !actors[PropertyNames::kStateKeyNormal].is_null()) {
            auto actor = createActor(actors[PropertyNames::kStateKeyNormal]);
            if (actor) btn->setNormalStateActor(actor);
        }
        if (actors.contains(PropertyNames::kStateKeyHover) && !actors[PropertyNames::kStateKeyHover].is_null()) {
            auto actor = createActor(actors[PropertyNames::kStateKeyHover]);
            if (actor) btn->setHoverStateActor(actor);
        }
        if (actors.contains(PropertyNames::kStateKeyPressed) && !actors[PropertyNames::kStateKeyPressed].is_null()) {
            auto actor = createActor(actors[PropertyNames::kStateKeyPressed]);
            if (actor) btn->setPressedStateActor(actor);
        }
        if (actors.contains(PropertyNames::kStateKeyDisabled) && !actors[PropertyNames::kStateKeyDisabled].is_null()) {
            auto actor = createActor(actors[PropertyNames::kStateKeyDisabled]);
            if (actor) btn->setDisabledStateActor(actor);
    }

    

    popJsonPath();
    }

    // LuotiAni (particle animation)
    if (j.contains(PropertyNames::kJsonLuotiAni) && !j[PropertyNames::kJsonLuotiAni].is_null()) {
        pushJsonPath(PropertyNames::kJsonLuotiAni);
        const json& la = j[PropertyNames::kJsonLuotiAni];
        string filePath;
        string resourceId;
        if (la.is_string()) {
            filePath = la.get<string>();
        } else if (la.is_object()) {
            if (la.contains(PropertyNames::kJsonFile) && la[PropertyNames::kJsonFile].is_string()) {
                filePath = la[PropertyNames::kJsonFile].get<string>();
            }
            if (la.contains(PropertyNames::kJsonResourceId) && la[PropertyNames::kJsonResourceId].is_string()) {
                resourceId = la[PropertyNames::kJsonResourceId].get<string>();
            }
        }
        if (!filePath.empty()) {
            try {
                // 构造 scale 恒为 1.0：按钮 scale 经 setParent 复合缩放作用于
                // 内嵌动画（setParent 中 m_xxScale = m_xScale * parent scale），
                // 传按钮 scale 会造成双重缩放
                auto luotiAni = make_shared<LuotiAni>(btn.get(), 1.0f, 1.0f);
                fs::path rp(filePath);
                if (rp.is_relative()) rp = fs::path(Platform::GetBasePath()) / rp;
                luotiAni->loadAniDesc(rp);
                luotiAni->setRect(SRect(0, 0, rect.width, rect.height));
                // parse 阶段尚无渲染设备：不 prepare/play，挂树后由
                // LuotiAni::setRenderDevice 补 prepare（同 CreateAnimation 模式），
                // 播放由 SetBool "playing" 控制
                btn->setLuotiAni(luotiAni);
            } catch (const char* e) {
                logWarn("failed to load luotiAni from file: " + filePath + " (" + e + ")");
            } catch (...) {
                logWarn("failed to load luotiAni from file: " + filePath);
            }
        } else if (!resourceId.empty()) {
            try {
                auto luotiAni = make_shared<LuotiAni>(btn.get(), btn->getScaleXX(), btn->getScaleYY());
                luotiAni->loadAniDesc(resourceId);
                luotiAni->setRect(SRect(0, 0, rect.width, rect.height));
                luotiAni->prepare(0);
                luotiAni->play();
                btn->setLuotiAni(luotiAni);
            } catch (...) {
                logWarn("failed to load luotiAni from resource: " + resourceId);
            }
        }
        popJsonPath();
    }

    parseEvents(btn, j);
    parseBindings(btn, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = btn;
    }

    btn->create();
    return btn;
}

// ==================== EditBox ====================

shared_ptr<EditBox> LayoutParser::parseEditBox(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto editBox = make_shared<EditBox>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(editBox, PropertyNames::kThemeCatEditBox);
    editBox->setFont(m_theme.getFontName(PropertyNames::kThemeCatEditBox));
    editBox->setFontSize(m_theme.getFontSize(PropertyNames::kThemeCatEditBox));
    parseCommonProperties(editBox, j);

    if (j.contains(PropertyNames::kJsonText) && j[PropertyNames::kJsonText].is_string()) {
        editBox->setText(j[PropertyNames::kJsonText].get<string>());
    }

    if (j.contains(PropertyNames::kJsonPlaceholder) && j[PropertyNames::kJsonPlaceholder].is_string()) {
        editBox->setPlaceholder(j[PropertyNames::kJsonPlaceholder].get<string>());
    }

    if (j.contains(PropertyNames::kJsonPasswordMode) && j[PropertyNames::kJsonPasswordMode].is_boolean()) {
        editBox->setPasswordMode(j[PropertyNames::kJsonPasswordMode].get<bool>());
    }

    if (j.contains(PropertyNames::kJsonPasswordChar) && j[PropertyNames::kJsonPasswordChar].is_string()) {
        string pc = j[PropertyNames::kJsonPasswordChar].get<string>();
        if (!pc.empty()) {
            editBox->setPasswordChar(pc[0]);
        }
    }

    if (j.contains(PropertyNames::kJsonFont) && j[PropertyNames::kJsonFont].is_object()) {
        pushJsonPath(PropertyNames::kJsonFont);
        const json& font = j[PropertyNames::kJsonFont];
        if (font.contains(PropertyNames::kJsonName) && font[PropertyNames::kJsonName].is_string()) {
            editBox->setFont(parseFontName(font[PropertyNames::kJsonName].get<string>()));
        }
        if (font.contains(PropertyNames::kJsonSize) && font[PropertyNames::kJsonSize].is_number()) {
            editBox->setFontSize(font[PropertyNames::kJsonSize].get<int>());
        }
        popJsonPath();
    }

    if (j.contains(PropertyNames::kJsonAlignment) && j[PropertyNames::kJsonAlignment].is_string()) {
        editBox->setAlignmentMode(parseAlignment(j[PropertyNames::kJsonAlignment].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonMargin)) {
        editBox->setMargin(parseMargin(j[PropertyNames::kJsonMargin]));
    }

    parseEvents(editBox, j);
    parseBindings(editBox, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = editBox;
    }

    editBox->create();
    return editBox;
}

// ==================== ComboBox ====================

shared_ptr<ComboBox> LayoutParser::parseComboBox(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto combo = make_shared<ComboBox>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(combo, PropertyNames::kThemeCatEditBox);
    combo->setFont(m_theme.getFontName(PropertyNames::kThemeCatEditBox));
    combo->setFontSize(m_theme.getFontSize(PropertyNames::kThemeCatEditBox));
    parseCommonProperties(combo, j);

    if (j.contains(PropertyNames::kJsonText) && j[PropertyNames::kJsonText].is_string()) {
        combo->setText(j[PropertyNames::kJsonText].get<string>());
    }

    if (j.contains(PropertyNames::kJsonEditable) && j[PropertyNames::kJsonEditable].is_boolean()) {
        combo->setEditable(j[PropertyNames::kJsonEditable].get<bool>());
    }

    if (j.contains(PropertyNames::kJsonPlaceholder) && j[PropertyNames::kJsonPlaceholder].is_string()) {
        combo->setPlaceholder(j[PropertyNames::kJsonPlaceholder].get<string>());
    }

    if (j.contains(PropertyNames::kJsonItems) && j[PropertyNames::kJsonItems].is_array()) {
        pushJsonPath(PropertyNames::kJsonItems);
        vector<ComboBoxItem> items;
        for (size_t i = 0; i < j[PropertyNames::kJsonItems].size(); ++i) {
            const json& ji = j[PropertyNames::kJsonItems][i];
            ComboBoxItem item;
            item.label = ji.value(PropertyNames::kJsonLabel, "");
            item.value = ji.value(PropertyNames::kJsonValue, item.label);
            item.disabled = ji.value(PropertyNames::kJsonDisabled, false);
            items.push_back(item);
        }
        combo->setItems(items);
        popJsonPath();
    }

    if (j.contains(PropertyNames::kJsonSelectedIndex) && j[PropertyNames::kJsonSelectedIndex].is_number()) {
        combo->setSelectedIndex(j[PropertyNames::kJsonSelectedIndex].get<int>());
    }

    if (j.contains(PropertyNames::kJsonArrowWidth) && j[PropertyNames::kJsonArrowWidth].is_number()) {
        combo->setArrowWidth(j[PropertyNames::kJsonArrowWidth].get<float>());
    }

    if (j.contains(PropertyNames::kJsonItemHeight) && j[PropertyNames::kJsonItemHeight].is_number()) {
        combo->setItemHeight(j[PropertyNames::kJsonItemHeight].get<float>());
    }

    if (j.contains(PropertyNames::kJsonMaxVisibleItems) && j[PropertyNames::kJsonMaxVisibleItems].is_number()) {
        combo->setMaxVisibleItems(j[PropertyNames::kJsonMaxVisibleItems].get<int>());
    }

    if (j.contains(PropertyNames::kJsonCycleEnabled) && j[PropertyNames::kJsonCycleEnabled].is_boolean()) {
        combo->setCycleEnabled(j[PropertyNames::kJsonCycleEnabled].get<bool>());
    }

    if (j.contains(PropertyNames::kJsonFont) && j[PropertyNames::kJsonFont].is_object()) {
        pushJsonPath(PropertyNames::kJsonFont);
        const json& font = j[PropertyNames::kJsonFont];
        if (font.contains(PropertyNames::kJsonName) && font[PropertyNames::kJsonName].is_string()) {
            combo->setFont(parseFontName(font[PropertyNames::kJsonName].get<string>()));
        }
        if (font.contains(PropertyNames::kJsonSize) && font[PropertyNames::kJsonSize].is_number()) {
            combo->setFontSize(font[PropertyNames::kJsonSize].get<int>());
        }
        popJsonPath();
    }

    if (j.contains(PropertyNames::kJsonAlignment) && j[PropertyNames::kJsonAlignment].is_string()) {
        combo->setAlignmentMode(parseAlignment(j[PropertyNames::kJsonAlignment].get<string>()));
    }

    parseEvents(combo, j);
    parseBindings(combo, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = combo;
    }

    combo->create();
    return combo;
}

// ==================== Panel ====================

shared_ptr<Panel> LayoutParser::parsePanel(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto panel = make_shared<Panel>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(panel, PropertyNames::kThemeCatPanel);
    parseCommonProperties(panel, j);

    if (j.contains(PropertyNames::kJsonTransparent) && j[PropertyNames::kJsonTransparent].is_boolean()) {
        panel->setTransparent(j[PropertyNames::kJsonTransparent].get<bool>());
    }

    if (j.contains(PropertyNames::kJsonBgColor)) {
        SColor bgColor = parseColor(j[PropertyNames::kJsonBgColor]);
        StateColor sc(StateColor::Type::Background);
        sc.setNormal(bgColor);
        panel->setBackgroundStateColor(sc);
    }

    if (j.contains(PropertyNames::kJsonBorderColor)) {
        SColor borderColor = parseColor(j[PropertyNames::kJsonBorderColor]);
        StateColor sc(StateColor::Type::Border);
        sc.setNormal(borderColor);
        panel->setBorderStateColor(sc);
    }

    // Panel has no events in Phase 1

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = panel;
    }

    parseChildren(panel, j);

    // Layout engine
    if (j.contains(PropertyNames::kJsonLayout) && j[PropertyNames::kJsonLayout].is_object()) {
        pushJsonPath(PropertyNames::kJsonLayout);
        const json& layoutJson = j[PropertyNames::kJsonLayout];
        string layoutType = layoutJson.value(PropertyNames::kJsonType, PropertyNames::kLayoutTypeHFlow);
        float gap = layoutJson.value(PropertyNames::kJsonGap, 0.0f);

        Margin padding{0,0,0,0};
        if (layoutJson.contains(PropertyNames::kJsonPadding) && layoutJson[PropertyNames::kJsonPadding].is_object()) {
            padding = parseMargin(layoutJson[PropertyNames::kJsonPadding]);
        }

        shared_ptr<LayoutEngine> engine;
        if (layoutType == PropertyNames::kLayoutTypeVFlow) {
            engine = make_shared<VFlowLayout>(gap, padding);
        } else if (layoutType == PropertyNames::kLayoutTypeAnchor) {
            engine = make_shared<AnchorLayout>(padding);
        } else if (layoutType == PropertyNames::kLayoutTypeGrid) {
            auto gridEngine = make_shared<GridLayout>(gap, padding);
            if (layoutJson.contains(PropertyNames::kJsonColumns) && layoutJson[PropertyNames::kJsonColumns].is_array()) {
                vector<GridSize> cols;
                for (const auto& c : layoutJson[PropertyNames::kJsonColumns]) {
                    cols.push_back(parseGridSize(c));
                }
                gridEngine->setColumns(cols);
            }
            if (layoutJson.contains(PropertyNames::kJsonRows) && layoutJson[PropertyNames::kJsonRows].is_array()) {
                vector<GridSize> rows;
                for (const auto& r : layoutJson[PropertyNames::kJsonRows]) {
                    rows.push_back(parseGridSize(r));
                }
                gridEngine->setRows(rows);
            }
            engine = gridEngine;
        } else {
            engine = make_shared<HFlowLayout>(gap, padding);
        }
        panel->setLayoutEngine(engine);

        // Per-child layout properties
        if (j.contains(PropertyNames::kJsonChildren) && j[PropertyNames::kJsonChildren].is_array()) {
            const json& children = j[PropertyNames::kJsonChildren];
            auto& panelChildren = panel->getChildren();
            for (size_t i = 0; i < children.size() && i < panelChildren.size(); ++i) {
                if (children[i].contains(PropertyNames::kJsonFlowWeight) && children[i][PropertyNames::kJsonFlowWeight].is_number()) {
                    float fw = children[i][PropertyNames::kJsonFlowWeight].get<float>();
                    FlowItemProps props;
                    props.flexWeight = fw;
                    panel->setChildFlowProps(panelChildren[i].get(), props);
                }
                if (children[i].contains(PropertyNames::kJsonAnchor) && children[i][PropertyNames::kJsonAnchor].is_string()) {
                    AnchorInfo info;
                    info.anchor = children[i][PropertyNames::kJsonAnchor].get<string>();
                    if (children[i].contains(PropertyNames::kJsonAnchorOffset) && children[i][PropertyNames::kJsonAnchorOffset].is_object()) {
                        info.offset = parseMargin(children[i][PropertyNames::kJsonAnchorOffset]);
                    }
                    panel->setChildAnchorProps(panelChildren[i].get(), info);
                }
                if (children[i].contains(PropertyNames::kJsonGrid) && children[i][PropertyNames::kJsonGrid].is_object()) {
                    const json& g = children[i][PropertyNames::kJsonGrid];
                    GridItemProps props;
                    props.row = g.value(PropertyNames::kJsonRow, 0);
                    props.col = g.value(PropertyNames::kJsonCol, 0);
                    props.rowSpan = g.value(PropertyNames::kJsonRowSpan, 1);
                    props.colSpan = g.value(PropertyNames::kJsonColSpan, 1);
                    panel->setChildGridProps(panelChildren[i].get(), props);
                }
            }
        }

        panel->resolveChildPercentages();
        panel->reflowChildren();
        popJsonPath();
    } else {
        panel->resolveChildPercentages();
    }

    panel->create();
    return panel;
}

// ==================== ColorPicker ====================

shared_ptr<ColorPicker> LayoutParser::parseColorPicker(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    auto cp = make_shared<ColorPicker>(parent, rect);

    m_theme.applyCommonColors(cp, PropertyNames::kThemeCatColorPicker);
    parseCommonProperties(cp, j);

    if (j.contains(PropertyNames::kJsonColor))
        cp->setColor(parseColor(j[PropertyNames::kJsonColor]));

    if (j.contains(PropertyNames::kJsonPresets) && j[PropertyNames::kJsonPresets].is_array()) {
        vector<SColor> colors;
        for (auto& c : j[PropertyNames::kJsonPresets])
            colors.push_back(parseColor(c));
        if (!colors.empty())
            cp->setPresetColors(colors);
    }

    if (j.contains(PropertyNames::kJsonPresetLayout) && j[PropertyNames::kJsonPresetLayout].is_object()) {
        const json& pl = j[PropertyNames::kJsonPresetLayout];
        int cols = pl.value(PropertyNames::kJsonPresetCols, ConstDef::COLORPICKER_PRESET_COLS);
        int rows = pl.value(PropertyNames::kJsonPresetRows, ConstDef::COLORPICKER_PRESET_ROWS);
        cp->setPresetLayout(cols, rows);
    }

    if (j.contains(PropertyNames::kJsonSwatchSize) && j[PropertyNames::kJsonSwatchSize].is_number())
        cp->setClosedSwatchSize(j[PropertyNames::kJsonSwatchSize].get<float>());

    if (j.contains(PropertyNames::kJsonClosedFontSize) && j[PropertyNames::kJsonClosedFontSize].is_number())
        cp->setClosedFontSize(j[PropertyNames::kJsonClosedFontSize].get<int>());

    if (j.contains(PropertyNames::kJsonClosedTextColor))
        cp->setClosedTextColor(parseColor(j[PropertyNames::kJsonClosedTextColor]));

    if (j.contains(PropertyNames::kJsonPopupBGColor))
        cp->setPopupBGColor(parseColor(j[PropertyNames::kJsonPopupBGColor]));

    parseEvents(cp, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = cp;

    cp->create();
    return cp;
}

// ==================== WinFrame ====================

shared_ptr<WinFrame> LayoutParser::parseWinFrame(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto winFrame = make_shared<WinFrame>(parent, rect, xScale, yScale);

    if (j.contains(PropertyNames::kJsonTitle) && j[PropertyNames::kJsonTitle].is_string()) {
        winFrame->setTitle(j[PropertyNames::kJsonTitle].get<string>());
    }

    if (j.contains(PropertyNames::kJsonEdgeMargin) && j[PropertyNames::kJsonEdgeMargin].is_number()) {
        winFrame->setEdgeMargin(j[PropertyNames::kJsonEdgeMargin].get<float>());
    }

    if (j.contains(PropertyNames::kJsonResizable) && j[PropertyNames::kJsonResizable].is_boolean()) {
        winFrame->setResizable(j[PropertyNames::kJsonResizable].get<bool>());
    }

    // Color parsing
    if (j.contains(PropertyNames::kJsonColors) && j[PropertyNames::kJsonColors].is_object()) {
        const json& colors = j[PropertyNames::kJsonColors];
        if (colors.contains(PropertyNames::kBackground) && colors[PropertyNames::kBackground].is_object()) {
            winFrame->setBackgroundStateColor(parseStateColor(colors[PropertyNames::kBackground], StateColor::Type::Background));
        }
        if (colors.contains(PropertyNames::kBorder) && colors[PropertyNames::kBorder].is_object()) {
            winFrame->setBorderStateColor(parseStateColor(colors[PropertyNames::kBorder], StateColor::Type::Border));
        }
        if (colors.contains(PropertyNames::kJsonTitleBar) && colors[PropertyNames::kJsonTitleBar].is_object()) {
            const json& tb = colors[PropertyNames::kJsonTitleBar];
            if (tb.contains(PropertyNames::kJsonBg) && tb[PropertyNames::kJsonBg].is_object()) {
                winFrame->getTitleBar()->setBackgroundStateColor(
                    parseStateColor(tb[PropertyNames::kJsonBg], StateColor::Type::Background));
            }
        }
        if (colors.contains(PropertyNames::kJsonTitleText) && colors[PropertyNames::kJsonTitleText].is_object()) {
            winFrame->getTitleLabel()->setTextStateColor(
                parseStateColor(colors[PropertyNames::kJsonTitleText], StateColor::Type::Text));
        }
    }

    // Children go into ClientPanel
    if (j.contains(PropertyNames::kJsonChildren)) {
        parseChildren(static_pointer_cast<Control>(winFrame->getClientPanel()), j);
    }

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = winFrame;
    }

    // Events
    parseEvents(static_pointer_cast<ControlImpl>(winFrame), j);

    winFrame->create();
    winFrame->hide();
    return winFrame;
}

// ==================== MenuBar ====================

shared_ptr<MenuBar> LayoutParser::parseMenuBar(const json& j, Control* parent) {
    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    // 使用 nullptr parent：MenuBar 独立于控件树，调用方负责添加到 BENCH 顶层
    auto menuBar = make_shared<MenuBar>(nullptr, xScale, yScale);

    m_theme.applyCommonColors(menuBar, PropertyNames::kThemeCatMenuBar);
    parseCommonProperties(menuBar, j);

    // font.size (static global setting)
    if (j.contains(PropertyNames::kJsonFont) && j[PropertyNames::kJsonFont].is_object()) {
        pushJsonPath(PropertyNames::kJsonFont);
        if (j[PropertyNames::kJsonFont].contains(PropertyNames::kJsonSize) && j[PropertyNames::kJsonFont][PropertyNames::kJsonSize].is_number()) {
            float fontSize = (float)j[PropertyNames::kJsonFont][PropertyNames::kJsonSize].get<int>();
            menuBar->setFontSize(fontSize);
            // auto-recalculate barHeight if not explicitly set
            if (!j.contains(PropertyNames::kJsonBarHeight)) {
                menuBar->setBarHeight(fontSize * 1.6f);
            }
        }
        popJsonPath();
    }

    // barHeight (overrides auto-calculation from font.size)
    if (j.contains(PropertyNames::kJsonBarHeight) && j[PropertyNames::kJsonBarHeight].is_number()) {
        menuBar->setBarHeight(j[PropertyNames::kJsonBarHeight].get<float>());
    }

    // menus array
    if (j.contains(PropertyNames::kJsonMenus) && j[PropertyNames::kJsonMenus].is_array()) {
        pushJsonPath(PropertyNames::kJsonMenus);
        const json& menus = j[PropertyNames::kJsonMenus];
        for (size_t i = 0; i < menus.size(); ++i) {
            const json& menuJson = menus[i];
            string caption = menuJson.value(PropertyNames::kJsonCaption, PropertyNames::kDefaultMenuTitle);

            if (menuJson.contains(PropertyNames::kJsonItems) && menuJson[PropertyNames::kJsonItems].is_array()) {
                auto panel = make_shared<MenuPanel>(nullptr, xScale, yScale);
                populateMenuPanel(panel, menuJson[PropertyNames::kJsonItems], xScale, yScale);
                menuBar->addMenu(caption, panel);
            } else {
                pushJsonPath("menus[" + to_string(i) + "]");
                logWarn("menu entry \"" + caption + "\" has no 'items' array, skipping");
                popJsonPath();
            }
        }
        popJsonPath();
    }

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = menuBar;
    }

    return menuBar;
}

void LayoutParser::populateMenuPanel(shared_ptr<MenuPanel> panel, const json& items, float xScale, float yScale) {
    for (const auto& itemJson : items) {
        // Separator
        if (itemJson.contains(PropertyNames::kJsonType) && itemJson[PropertyNames::kJsonType].is_string() &&
            itemJson[PropertyNames::kJsonType].get<string>() == PropertyNames::kMenuTypeSeparator) {
            panel->addSeparator();
            continue;
        }

        // Determine type: SubMenu if has nested "items"
        MenuItemType type = MenuItemType::Normal;
        if (itemJson.contains(PropertyNames::kJsonItems) && itemJson[PropertyNames::kJsonItems].is_array()) {
            type = MenuItemType::SubMenu;
        }

        auto item = make_shared<MenuItem>(panel.get(), type, xScale, yScale);

        // Caption
        if (itemJson.contains(PropertyNames::kJsonCaption) && itemJson[PropertyNames::kJsonCaption].is_string()) {
            item->setCaption(itemJson[PropertyNames::kJsonCaption].get<string>());
        }

        // Shortcut
        if (itemJson.contains(PropertyNames::kJsonShortcut) && itemJson[PropertyNames::kJsonShortcut].is_string()) {
            item->setShortcut(itemJson[PropertyNames::kJsonShortcut].get<string>());
        }

        // Checked
        if (itemJson.contains(PropertyNames::kJsonChecked) && itemJson[PropertyNames::kJsonChecked].is_boolean()) {
            item->setChecked(itemJson[PropertyNames::kJsonChecked].get<bool>());
        }

        // Enabled
        if (itemJson.contains(PropertyNames::kJsonEnabled) && itemJson[PropertyNames::kJsonEnabled].is_boolean()) {
            item->setEnable(itemJson[PropertyNames::kJsonEnabled].get<bool>());
        }

        // SubMenu (recursive)
        if (itemJson.contains(PropertyNames::kJsonItems) && itemJson[PropertyNames::kJsonItems].is_array()) {
            auto subPanel = make_shared<MenuPanel>(nullptr, xScale, yScale);
            populateMenuPanel(subPanel, itemJson[PropertyNames::kJsonItems], xScale, yScale);
            item->setSubMenu(subPanel);
        }

        // Events (onClick)
        if (itemJson.contains(PropertyNames::kJsonEvents) && itemJson[PropertyNames::kJsonEvents].is_object()) {
            parseEvents(item, itemJson);
        }

        item->create();

        panel->addItem(item);
    }
}

// ==================== TextArea ====================

shared_ptr<TextArea> LayoutParser::parseTextArea(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto textArea = make_shared<TextArea>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(textArea, PropertyNames::kThemeCatTextArea);
    textArea->setFont(m_theme.getFontName(PropertyNames::kThemeCatTextArea));
    textArea->setFontSize(m_theme.getFontSize(PropertyNames::kThemeCatTextArea));
    parseCommonProperties(textArea, j);

    if (j.contains(PropertyNames::kJsonText) && j[PropertyNames::kJsonText].is_string()) {
        textArea->setText(j[PropertyNames::kJsonText].get<string>());
    }

    if (j.contains(PropertyNames::kJsonPlaceholder) && j[PropertyNames::kJsonPlaceholder].is_string()) {
        textArea->setPlaceholder(j[PropertyNames::kJsonPlaceholder].get<string>());
    }

    if (j.contains(PropertyNames::kJsonWordWrap) && j[PropertyNames::kJsonWordWrap].is_boolean()) {
        textArea->setWordWrap(j[PropertyNames::kJsonWordWrap].get<bool>());
    }

    if (j.contains(PropertyNames::kJsonLineHeight) && j[PropertyNames::kJsonLineHeight].is_number()) {
        textArea->setLineHeight(j[PropertyNames::kJsonLineHeight].get<int>());
    }

    if (j.contains(PropertyNames::kJsonScrollBarThickness) && j[PropertyNames::kJsonScrollBarThickness].is_number()) {
        textArea->setScrollBarThickness(j[PropertyNames::kJsonScrollBarThickness].get<float>());
    }

    if (j.contains(PropertyNames::kJsonFont) && j[PropertyNames::kJsonFont].is_object()) {
        pushJsonPath(PropertyNames::kJsonFont);
        const json& font = j[PropertyNames::kJsonFont];
        if (font.contains(PropertyNames::kJsonName) && font[PropertyNames::kJsonName].is_string()) {
            textArea->setFont(parseFontName(font[PropertyNames::kJsonName].get<string>()));
        }
        if (font.contains(PropertyNames::kJsonSize) && font[PropertyNames::kJsonSize].is_number()) {
            textArea->setFontSize(font[PropertyNames::kJsonSize].get<int>());
        }
        popJsonPath();
    }

    if (j.contains(PropertyNames::kJsonAlignment) && j[PropertyNames::kJsonAlignment].is_string()) {
        textArea->setAlignmentMode(parseAlignment(j[PropertyNames::kJsonAlignment].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonMargin)) {
        textArea->setMargin(parseMargin(j[PropertyNames::kJsonMargin]));
    }

    parseEvents(textArea, j);
    parseBindings(textArea, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = textArea;
    }

    textArea->create();
    return textArea;
}

// ==================== CheckBox ====================

static CheckState parseCheckState(const string& s) {
    if (s == PropertyNames::kCheckChecked)        return CheckState::Checked;
    if (s == PropertyNames::kCheckIndeterminate)  return CheckState::Indeterminate;
    return CheckState::Unchecked;
}

static CheckBoxStyle parseCheckBoxStyle(const string& s) {
    if (s == PropertyNames::kStyleCross)   return CheckBoxStyle::Cross;
    if (s == PropertyNames::kStyleCircle)  return CheckBoxStyle::Circle;
    return CheckBoxStyle::Classic;
}

static CheckBoxLayout parseCheckBoxLayout(const string& s) {
    if (s == PropertyNames::kLayoutTextLeft) return CheckBoxLayout::TextLeft;
    return CheckBoxLayout::TextRight;
}

static CheckBoxVerticalAlign parseCheckBoxVerticalAlign(const string& s) {
    if (s == PropertyNames::kVAlignTop)    return CheckBoxVerticalAlign::Top;
    if (s == PropertyNames::kVAlignBottom) return CheckBoxVerticalAlign::Bottom;
    return CheckBoxVerticalAlign::Center;
}

shared_ptr<CheckBox> LayoutParser::parseCheckBox(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto checkBox = make_shared<CheckBox>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(checkBox, PropertyNames::kThemeCatCheckBox);
    parseCommonProperties(checkBox, j);

    if (j.contains(PropertyNames::kJsonCaption) && j[PropertyNames::kJsonCaption].is_string()) {
        checkBox->getCaption()->setCaption(j[PropertyNames::kJsonCaption].get<string>());
    }

    int cbFontSize = m_theme.getFontSize(PropertyNames::kThemeCatCheckBox);
    checkBox->getCaption()->setFontSize(cbFontSize);

    if (j.contains(PropertyNames::kJsonCaptionSize) && j[PropertyNames::kJsonCaptionSize].is_number()) {
        checkBox->getCaption()->setFontSize(j[PropertyNames::kJsonCaptionSize].get<int>());
    }

    if (j.contains(PropertyNames::kJsonCheckState) && j[PropertyNames::kJsonCheckState].is_string()) {
        checkBox->setCheckState(parseCheckState(j[PropertyNames::kJsonCheckState].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonStyle) && j[PropertyNames::kJsonStyle].is_string()) {
        checkBox->setStyle(parseCheckBoxStyle(j[PropertyNames::kJsonStyle].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonLayout) && j[PropertyNames::kJsonLayout].is_string()) {
        checkBox->setLayout(parseCheckBoxLayout(j[PropertyNames::kJsonLayout].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonVerticalAlign) && j[PropertyNames::kJsonVerticalAlign].is_string()) {
        checkBox->setVerticalAlign(parseCheckBoxVerticalAlign(j[PropertyNames::kJsonVerticalAlign].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonSizeRatio) && j[PropertyNames::kJsonSizeRatio].is_number()) {
        checkBox->setSizeRatio(j[PropertyNames::kJsonSizeRatio].get<float>());
    }

    if (j.contains(PropertyNames::kJsonTriState) && j[PropertyNames::kJsonTriState].is_boolean()) {
        checkBox->setTriStateEnabled(j[PropertyNames::kJsonTriState].get<bool>());
    }

    // Theme CheckBox-specific colors (defaults)
    SColor themeCheck;
    if (m_theme.getColorOpt(PropertyNames::kThemeCheckboxCheck, themeCheck))
        checkBox->setCheckColor(themeCheck);
    SColor themeCross;
    if (m_theme.getColorOpt(PropertyNames::kThemeCheckboxCross, themeCross))
        checkBox->setCrossColor(themeCross);
    SColor themeIndet;
    if (m_theme.getColorOpt(PropertyNames::kThemeCheckboxIndeterminate, themeIndet))
        checkBox->setIndeterminateColor(themeIndet);
    SColor themeBoxBorder;
    if (m_theme.getColorOpt(PropertyNames::kThemeCheckboxBoxBorder, themeBoxBorder))
        checkBox->setBoxBorderColor(themeBoxBorder);

    // CheckBox-specific colors (JSON overrides)
    if (j.contains(PropertyNames::kJsonColors) && j[PropertyNames::kJsonColors].is_object()) {
        const json& colors = j[PropertyNames::kJsonColors];
        if (colors.contains(PropertyNames::kJsonCheckColor)) {
            checkBox->setCheckColor(parseColor(colors[PropertyNames::kJsonCheckColor]));
        }
        if (colors.contains(PropertyNames::kJsonCrossColor)) {
            checkBox->setCrossColor(parseColor(colors[PropertyNames::kJsonCrossColor]));
        }
        if (colors.contains(PropertyNames::kJsonIndeterminateColor)) {
            checkBox->setIndeterminateColor(parseColor(colors[PropertyNames::kJsonIndeterminateColor]));
        }
        if (colors.contains(PropertyNames::kJsonBoxBorderColor)) {
            checkBox->setBoxBorderColor(parseColor(colors[PropertyNames::kJsonBoxBorderColor]));
        }
    }

    parseEvents(checkBox, j);
    parseBindings(checkBox, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = checkBox;
    }

    checkBox->create();
    return checkBox;
}

// ==================== ProgressBar ====================

static ProgressBarStyle parseProgressBarStyle(const string& s) {
    if (s == PropertyNames::kOrientVertical) return ProgressBarStyle::Vertical;
    return ProgressBarStyle::Horizontal;
}

static ProgressBarTextMode parseProgressBarTextMode(const string& s) {
    if (s == PropertyNames::kTextModeNone)    return ProgressBarTextMode::None;
    if (s == PropertyNames::kTextModeCustom)  return ProgressBarTextMode::Custom;
    return ProgressBarTextMode::Percent;
}

shared_ptr<ProgressBar> LayoutParser::parseProgressBar(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto progressBar = make_shared<ProgressBar>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(progressBar, PropertyNames::kThemeCatProgressBar);
    progressBar->setFont(m_theme.getFontName(PropertyNames::kThemeCatProgressBar));
    progressBar->setFontSize(m_theme.getFontSize(PropertyNames::kThemeCatProgressBar));
    parseCommonProperties(progressBar, j);

    if (j.contains(PropertyNames::kJsonValue) && j[PropertyNames::kJsonValue].is_number()) {
        progressBar->setValue(j[PropertyNames::kJsonValue].get<float>());
    }

    if (j.contains(PropertyNames::kJsonRange) && j[PropertyNames::kJsonRange].is_object()) {
        float minVal = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMin, 0.0f);
        float maxVal = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMax, 100.0f);
        progressBar->setRange(minVal, maxVal);
    }

    if (j.contains(PropertyNames::kJsonStyle) && j[PropertyNames::kJsonStyle].is_string()) {
        progressBar->setStyle(parseProgressBarStyle(j[PropertyNames::kJsonStyle].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonTextMode) && j[PropertyNames::kJsonTextMode].is_string()) {
        progressBar->setTextMode(parseProgressBarTextMode(j[PropertyNames::kJsonTextMode].get<string>()));
    }

    if (j.contains(PropertyNames::kJsonCustomText) && j[PropertyNames::kJsonCustomText].is_string()) {
        progressBar->setCustomText(j[PropertyNames::kJsonCustomText].get<string>());
    }

    if (j.contains(PropertyNames::kJsonAnimationSpeed) && j[PropertyNames::kJsonAnimationSpeed].is_number()) {
        progressBar->setAnimationSpeed(j[PropertyNames::kJsonAnimationSpeed].get<float>());
    }

    if (j.contains(PropertyNames::kJsonFont) && j[PropertyNames::kJsonFont].is_object()) {
        pushJsonPath(PropertyNames::kJsonFont);
        const json& font = j[PropertyNames::kJsonFont];
        if (font.contains(PropertyNames::kJsonName) && font[PropertyNames::kJsonName].is_string()) {
            progressBar->setFont(parseFontName(font[PropertyNames::kJsonName].get<string>()));
        }
        if (font.contains(PropertyNames::kJsonSize) && font[PropertyNames::kJsonSize].is_number()) {
            progressBar->setFontSize(font[PropertyNames::kJsonSize].get<int>());
        }
        popJsonPath();
    }

    if (j.contains(PropertyNames::kJsonAlignment) && j[PropertyNames::kJsonAlignment].is_string()) {
        progressBar->setAlignmentMode(parseAlignment(j[PropertyNames::kJsonAlignment].get<string>()));
    }

    // Theme ProgressBar-specific colors (defaults)
    SColor themeProgress;
    if (m_theme.getColorOpt(PropertyNames::kThemeProgressbarProgress, themeProgress))
        progressBar->setProgressColor(themeProgress);
    SColor themeTrack;
    if (m_theme.getColorOpt(PropertyNames::kThemeProgressbarTrack, themeTrack))
        progressBar->setBackgroundColor(themeTrack);

    // ProgressBar-specific colors (JSON overrides)
    if (j.contains(PropertyNames::kJsonColors) && j[PropertyNames::kJsonColors].is_object()) {
        const json& colors = j[PropertyNames::kJsonColors];
        if (colors.contains(PropertyNames::kJsonProgressColor)) {
            progressBar->setProgressColor(parseColor(colors[PropertyNames::kJsonProgressColor]));
        }
        if (colors.contains(PropertyNames::kJsonBackgroundColor)) {
            progressBar->setBackgroundColor(parseColor(colors[PropertyNames::kJsonBackgroundColor]));
        }
    }

    parseEvents(progressBar, j);
    parseBindings(progressBar, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = progressBar;
    }

    progressBar->create();
    return progressBar;
}

// ==================== Slider ====================

static SliderStyle parseSliderStyle(const string& s) {
    if (s == PropertyNames::kOrientVertical) return SliderStyle::Vertical;
    return SliderStyle::Horizontal;
}

shared_ptr<Slider> LayoutParser::parseSlider(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    auto slider = make_shared<Slider>(parent, rect);

    if (j.contains(PropertyNames::kJsonRange) && j[PropertyNames::kJsonRange].is_object()) {
        float minVal = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMin, 0.0f);
        float maxVal = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMax, 100.0f);
        slider->setRange(minVal, maxVal);
    }

    if (j.contains(PropertyNames::kJsonValue) && j[PropertyNames::kJsonValue].is_number())
        slider->setValue(j[PropertyNames::kJsonValue].get<float>());

    if (j.contains(PropertyNames::kJsonStep) && j[PropertyNames::kJsonStep].is_number())
        slider->setStep(j[PropertyNames::kJsonStep].get<float>());

    if (j.contains(PropertyNames::kJsonStyle) && j[PropertyNames::kJsonStyle].is_string())
        slider->setStyle(parseSliderStyle(j[PropertyNames::kJsonStyle].get<string>()));

    if (j.contains(PropertyNames::kJsonReverse) && j[PropertyNames::kJsonReverse].is_boolean())
        slider->setReverse(j[PropertyNames::kJsonReverse].get<bool>());

    if (j.contains(PropertyNames::kJsonTrack) && j[PropertyNames::kJsonTrack].is_object()) {
        const json& track = j[PropertyNames::kJsonTrack];
        if (track.contains(PropertyNames::kJsonThickness) && track[PropertyNames::kJsonThickness].is_number())
            slider->setTrackThickness(track[PropertyNames::kJsonThickness].get<float>());
        if (track.contains(PropertyNames::kJsonColor) && track[PropertyNames::kJsonColor].is_object())
            slider->setTrackColor(parseStateColor(track[PropertyNames::kJsonColor], StateColor::Type::Background).getNormal());
        if (track.contains(PropertyNames::kJsonFillColor) && track[PropertyNames::kJsonFillColor].is_object())
            slider->setTrackFillColor(parseStateColor(track[PropertyNames::kJsonFillColor], StateColor::Type::Background).getNormal());
    }

    if (j.contains(PropertyNames::kJsonThumb) && j[PropertyNames::kJsonThumb].is_object()) {
        const json& thumb = j[PropertyNames::kJsonThumb];
        if (thumb.contains(PropertyNames::kJsonSize) && thumb[PropertyNames::kJsonSize].is_number())
            slider->setThumbSize(thumb[PropertyNames::kJsonSize].get<float>());
        if (thumb.contains(PropertyNames::kJsonColor) && thumb[PropertyNames::kJsonColor].is_object())
            slider->setThumbColor(parseStateColor(thumb[PropertyNames::kJsonColor], StateColor::Type::Background).getNormal());
        if (thumb.contains(PropertyNames::kJsonBorderColor) && thumb[PropertyNames::kJsonBorderColor].is_object())
            slider->setThumbBorderColor(parseStateColor(thumb[PropertyNames::kJsonBorderColor], StateColor::Type::Background).getNormal());
        if (thumb.contains(PropertyNames::kJsonHoverColor) && thumb[PropertyNames::kJsonHoverColor].is_object())
            slider->setThumbHoverColor(parseStateColor(thumb[PropertyNames::kJsonHoverColor], StateColor::Type::Background).getNormal());
    }

    if (j.contains(PropertyNames::kJsonShowValueLabel) && j[PropertyNames::kJsonShowValueLabel].is_boolean())
        slider->setShowValueLabel(j[PropertyNames::kJsonShowValueLabel].get<bool>());

    if (j.contains(PropertyNames::kJsonLabelFormat) && j[PropertyNames::kJsonLabelFormat].is_string())
        slider->setLabelFormat(j[PropertyNames::kJsonLabelFormat].get<string>());

    if (j.contains(PropertyNames::kJsonLabelGap) && j[PropertyNames::kJsonLabelGap].is_number())
        slider->setLabelGap(j[PropertyNames::kJsonLabelGap].get<float>());

    if (j.contains(PropertyNames::kJsonTick) && j[PropertyNames::kJsonTick].is_object()) {
        const json& tick = j[PropertyNames::kJsonTick];
        if (tick.contains(PropertyNames::kJsonInterval) && tick[PropertyNames::kJsonInterval].is_number())
            slider->setTickInterval(tick[PropertyNames::kJsonInterval].get<float>());
        if (tick.contains(PropertyNames::kJsonLength) && tick[PropertyNames::kJsonLength].is_number())
            slider->setTickLength(tick[PropertyNames::kJsonLength].get<float>());
        if (tick.contains(PropertyNames::kJsonColor) && tick[PropertyNames::kJsonColor].is_object())
            slider->setTickColor(parseStateColor(tick[PropertyNames::kJsonColor], StateColor::Type::Background).getNormal());
    }

    parseCommonProperties(std::static_pointer_cast<ControlImpl>(slider), j);
    parseEvents(std::static_pointer_cast<ControlImpl>(slider), j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = slider;

    slider->create();
    return slider;
}

// ==================== NumericUpDown ====================

shared_ptr<NumericUpDown> LayoutParser::parseNumericUpDown(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto nud = make_shared<NumericUpDown>(parent, rect, xScale, yScale);

    m_theme.applyCommonColors(nud, PropertyNames::kThemeCatNumericUpDown);
    parseCommonProperties(nud, j);

    if (j.contains(PropertyNames::kJsonValue))
        nud->setValue(j[PropertyNames::kJsonValue].get<double>());

    if (j.contains(PropertyNames::kJsonRange) && j[PropertyNames::kJsonRange].is_object()) {
        double mn = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMin, 0.0);
        double mx = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMax, 100.0);
        nud->setRange(mn, mx);
    }

    if (j.contains(PropertyNames::kJsonStep))
        nud->setStep(j[PropertyNames::kJsonStep].get<double>());

    if (j.contains(PropertyNames::kJsonPageStep))
        nud->setPageStep(j[PropertyNames::kJsonPageStep].get<double>());

    if (j.contains(PropertyNames::kJsonDecimals))
        nud->setDecimals(j[PropertyNames::kJsonDecimals].get<int>());

    if (j.contains(PropertyNames::kJsonPlaceholder))
        nud->setPlaceholder(j[PropertyNames::kJsonPlaceholder].get<string>());

    if (j.contains(PropertyNames::kJsonReadOnly))
        nud->setReadOnly(j[PropertyNames::kJsonReadOnly].get<bool>());

    if (j.contains(PropertyNames::kJsonButtonWidth))
        nud->setButtonWidth(j[PropertyNames::kJsonButtonWidth].get<float>());

    parseEvents(nud, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = nud;

    nud->create();
    return nud;
}

// ==================== Splitter ====================

shared_ptr<Splitter> LayoutParser::parseSplitter(const json& j, Control* parent) {
    SRect rect = {0, 0, 10, 10};
    if (j.contains(PropertyNames::kJsonRect)) rect = parseRect(j[PropertyNames::kJsonRect]);

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto sp = make_shared<Splitter>(parent, rect, xScale, yScale);
    m_theme.applyCommonColors(sp, PropertyNames::kThemeCatSplitter);
    parseCommonProperties(sp, j);

    if (j.contains(PropertyNames::kJsonOrientation)) {
        string orient = j[PropertyNames::kJsonOrientation].get<string>();
        sp->setOrientation(orient == PropertyNames::kOrientVertical);
    }

    if (j.contains(PropertyNames::kJsonFirstPanel) && j.contains(PropertyNames::kJsonSecondPanel)) {
        string firstId = j[PropertyNames::kJsonFirstPanel].get<string>();
        string secondId = j[PropertyNames::kJsonSecondPanel].get<string>();
        auto first = findControlById(firstId);
        auto second = findControlById(secondId);
        if (first && second) {
            sp->setLinkedControls(first, second);
            SRect fr = first->getRect();
            if (sp->isHorizontal()) {
                sp->setRect({fr.left + fr.width, fr.top,
                             sp->getThickness(), fr.height});
            } else {
                sp->setRect({fr.left, fr.top + fr.height,
                             fr.width, sp->getThickness()});
            }
        }
    }

    if (j.contains(PropertyNames::kJsonMinFirst))
        sp->setMinSize(j[PropertyNames::kJsonMinFirst].get<float>(), sp->getMinSecond());
    if (j.contains(PropertyNames::kJsonMinSecond))
        sp->setMinSize(sp->getMinFirst(), j[PropertyNames::kJsonMinSecond].get<float>());
    if (j.contains(PropertyNames::kJsonThickness))
        sp->setThickness(j[PropertyNames::kJsonThickness].get<float>());
    if (j.contains(PropertyNames::kJsonRatio))
        sp->setSplitRatio(j[PropertyNames::kJsonRatio].get<float>());

    parseEvents(sp, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = sp;

    sp->create();
    return sp;
}

// ==================== TreeView ====================

shared_ptr<TreeView> LayoutParser::parseTreeView(const json& j, Control* parent) {
    SRect rect = {0, 0, 200, 300};
    if (j.contains(PropertyNames::kJsonRect)) rect = parseRect(j[PropertyNames::kJsonRect]);

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto tv = make_shared<TreeView>(parent, rect, xScale, yScale);
    m_theme.applyCommonColors(tv, PropertyNames::kThemeCatTreeView);
    parseCommonProperties(tv, j);

    if (j.contains(PropertyNames::kJsonIndentWidth))
        tv->setIndentWidth(j[PropertyNames::kJsonIndentWidth].get<float>());
    if (j.contains(PropertyNames::kJsonRowHeight))
        tv->setRowHeight(j[PropertyNames::kJsonRowHeight].get<float>());
    if (j.contains(PropertyNames::kJsonCycleNavigation))
        tv->setCycleNavigation(j[PropertyNames::kJsonCycleNavigation].get<bool>());
    if (j.contains(PropertyNames::kJsonDefaultExpand))
        tv->setDefaultExpand(j[PropertyNames::kJsonDefaultExpand].get<bool>());

    if (j.contains(PropertyNames::kJsonItems) && j[PropertyNames::kJsonItems].is_array()) {
        vector<shared_ptr<TreeNode>> items;
        function<void(const json&, vector<shared_ptr<TreeNode>>&)> parseItems;
        parseItems = [&](const json& arr, vector<shared_ptr<TreeNode>>& out) {
            for (const auto& item : arr) {
                auto node = make_shared<TreeNode>();
                node->id = item.value(PropertyNames::kJsonId, "");
                node->label = item.value(PropertyNames::kJsonLabel, "");
                node->expanded = item.value(PropertyNames::kJsonExpanded, false);
                if (item.contains(PropertyNames::kJsonUserData) && item[PropertyNames::kJsonUserData].is_string()) {
                    node->userData = static_cast<void*>(new std::string(item[PropertyNames::kJsonUserData].get<string>()));
                }
                if (item.contains(PropertyNames::kJsonChildren) && item[PropertyNames::kJsonChildren].is_array()) {
                    parseItems(item[PropertyNames::kJsonChildren], node->children);
                }
                out.push_back(node);
            }
        };
        parseItems(j[PropertyNames::kJsonItems], items);
        tv->setItems(items);
        tv->setOnClearNode([](shared_ptr<TreeView>, void* ud) {
            delete static_cast<std::string*>(ud);
        });
    }

    parseEvents(tv, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = tv;

    tv->create();
    return tv;
}

// ==================== ScrollBar ====================

static ScrollBarOrientation parseScrollBarOrientation(const string& s) {
    if (s == PropertyNames::kOrientHorizontal) return ScrollBarOrientation::Horizontal;
    return ScrollBarOrientation::Vertical;
}

shared_ptr<ScrollBar> LayoutParser::parseScrollBar(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    ScrollBarOrientation orientation = ScrollBarOrientation::Vertical;
    if (j.contains(PropertyNames::kJsonOrientation) && j[PropertyNames::kJsonOrientation].is_string()) {
        orientation = parseScrollBarOrientation(j[PropertyNames::kJsonOrientation].get<string>());
    }

    auto scrollBar = make_shared<ScrollBar>(parent, rect, orientation, xScale, yScale);

    m_theme.applyCommonColors(scrollBar, PropertyNames::kThemeCatScrollBar);
    parseCommonProperties(scrollBar, j);

    if (j.contains(PropertyNames::kJsonValue) && j[PropertyNames::kJsonValue].is_number()) {
        scrollBar->setValue(j[PropertyNames::kJsonValue].get<float>());
    }

    if (j.contains(PropertyNames::kJsonRange) && j[PropertyNames::kJsonRange].is_object()) {
        float minVal = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMin, 0.0f);
        float maxVal = j[PropertyNames::kJsonRange].value(PropertyNames::kJsonMax, 100.0f);
        scrollBar->setRange(minVal, maxVal);
    }

    if (j.contains(PropertyNames::kJsonPageSize) && j[PropertyNames::kJsonPageSize].is_number()) {
        scrollBar->setPageSize(j[PropertyNames::kJsonPageSize].get<float>());
    }

    if (j.contains(PropertyNames::kJsonStepSize) && j[PropertyNames::kJsonStepSize].is_number()) {
        scrollBar->setStepSize(j[PropertyNames::kJsonStepSize].get<float>());
    }

    if (j.contains(PropertyNames::kJsonThickness) && j[PropertyNames::kJsonThickness].is_number()) {
        scrollBar->setThickness(j[PropertyNames::kJsonThickness].get<float>());
    }

    parseEvents(scrollBar, j);
    parseBindings(scrollBar, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string()) {
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = scrollBar;
    }

    scrollBar->create();
    return scrollBar;
}

// ==================== Popup ====================

shared_ptr<Popup> LayoutParser::parsePopup(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto popup = make_shared<Popup>(parent, rect, xScale, yScale);
    m_theme.applyCommonColors(popup, PropertyNames::kThemeCatPopup);
    parseCommonProperties(popup, j);

    // Popup-specific
    if (j.contains(PropertyNames::kJsonCentered) && j[PropertyNames::kJsonCentered].is_boolean())
        popup->setCentered();
    if (j.contains(PropertyNames::kJsonCloseOnEsc) && j[PropertyNames::kJsonCloseOnEsc].is_boolean())
        popup->setCloseOnEsc(j[PropertyNames::kJsonCloseOnEsc].get<bool>());
    if (j.contains(PropertyNames::kJsonCloseOnClickOutside) && j[PropertyNames::kJsonCloseOnClickOutside].is_boolean())
        popup->setCloseOnClickOutside(j[PropertyNames::kJsonCloseOnClickOutside].get<bool>());

    parseEvents(popup, j);
    parseBindings(popup, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = popup;

    parseChildren(popup, j);
    popup->create();
    return popup;
}

// ==================== ConfirmPopup ====================

shared_ptr<ConfirmPopup> LayoutParser::parseConfirmPopup(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto cp = make_shared<ConfirmPopup>(parent, rect, xScale, yScale);
    m_theme.applyCommonColors(cp, PropertyNames::kThemeCatConfirmPopup);
    parseCommonProperties(cp, j);

    if (j.contains(PropertyNames::kJsonCentered) && j[PropertyNames::kJsonCentered].is_boolean())
        cp->setCentered();
    if (j.contains(PropertyNames::kJsonCloseOnEsc) && j[PropertyNames::kJsonCloseOnEsc].is_boolean())
        cp->setCloseOnEsc(j[PropertyNames::kJsonCloseOnEsc].get<bool>());
    if (j.contains(PropertyNames::kJsonCloseOnClickOutside) && j[PropertyNames::kJsonCloseOnClickOutside].is_boolean())
        cp->setCloseOnClickOutside(j[PropertyNames::kJsonCloseOnClickOutside].get<bool>());

    // confirm button
    if (j.contains(PropertyNames::kJsonConfirmButton) && j[PropertyNames::kJsonConfirmButton].is_object()) {
        const json& btn = j[PropertyNames::kJsonConfirmButton];
        if (btn.contains(PropertyNames::kJsonText) && btn[PropertyNames::kJsonText].is_string())
            cp->setConfirmButtonText(btn[PropertyNames::kJsonText].get<string>());
        if (btn.contains(PropertyNames::kJsonRect) && btn[PropertyNames::kJsonRect].is_object())
            cp->setConfirmButtonRect(parseRect(btn[PropertyNames::kJsonRect]));
        if (btn.contains(PropertyNames::kJsonVisible) && btn[PropertyNames::kJsonVisible].is_boolean())
            cp->setConfirmButtonVisible(btn[PropertyNames::kJsonVisible].get<bool>());
    }

    if (j.contains(PropertyNames::kJsonButtonHeight) && j[PropertyNames::kJsonButtonHeight].is_number())
        cp->setButtonHeight(j[PropertyNames::kJsonButtonHeight].get<float>());
    if (j.contains(PropertyNames::kJsonButtonGap) && j[PropertyNames::kJsonButtonGap].is_number())
        cp->setButtonGap(j[PropertyNames::kJsonButtonGap].get<float>());
    if (j.contains(PropertyNames::kJsonPadding) && j[PropertyNames::kJsonPadding].is_number())
        cp->setPadding(j[PropertyNames::kJsonPadding].get<float>());

    parseEvents(cp, j);
    parseBindings(cp, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = cp;

    parseChildren(cp, j);
    cp->create();
    return cp;
}

// ==================== Dialog ====================

shared_ptr<Dialog> LayoutParser::parseDialog(const json& j, Control* parent) {
    pushJsonPath(PropertyNames::kJsonRect);
    SRect rect = parseRect(j[PropertyNames::kJsonRect]);
    popJsonPath();

    float xScale = 1.0f, yScale = 1.0f;
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        xScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        yScale = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
    }

    auto dlg = make_shared<Dialog>(parent, rect, xScale, yScale);
    m_theme.applyCommonColors(dlg, PropertyNames::kThemeCatDialog);
    parseCommonProperties(dlg, j);

    if (j.contains(PropertyNames::kJsonCentered) && j[PropertyNames::kJsonCentered].is_boolean())
        dlg->setCentered();
    if (j.contains(PropertyNames::kJsonCloseOnEsc) && j[PropertyNames::kJsonCloseOnEsc].is_boolean())
        dlg->setCloseOnEsc(j[PropertyNames::kJsonCloseOnEsc].get<bool>());
    if (j.contains(PropertyNames::kJsonCloseOnClickOutside) && j[PropertyNames::kJsonCloseOnClickOutside].is_boolean())
        dlg->setCloseOnClickOutside(j[PropertyNames::kJsonCloseOnClickOutside].get<bool>());

    // confirm button
    if (j.contains(PropertyNames::kJsonConfirmButton) && j[PropertyNames::kJsonConfirmButton].is_object()) {
        const json& btn = j[PropertyNames::kJsonConfirmButton];
        if (btn.contains(PropertyNames::kJsonText) && btn[PropertyNames::kJsonText].is_string())
            dlg->setConfirmButtonText(btn[PropertyNames::kJsonText].get<string>());
        if (btn.contains(PropertyNames::kJsonRect) && btn[PropertyNames::kJsonRect].is_object())
            dlg->setConfirmButtonRect(parseRect(btn[PropertyNames::kJsonRect]));
        if (btn.contains(PropertyNames::kJsonVisible) && btn[PropertyNames::kJsonVisible].is_boolean())
            dlg->setConfirmButtonVisible(btn[PropertyNames::kJsonVisible].get<bool>());
    }

    // cancel button
    if (j.contains(PropertyNames::kJsonCancelButton) && j[PropertyNames::kJsonCancelButton].is_object()) {
        const json& btn = j[PropertyNames::kJsonCancelButton];
        if (btn.contains(PropertyNames::kJsonText) && btn[PropertyNames::kJsonText].is_string())
            dlg->setCancelButtonText(btn[PropertyNames::kJsonText].get<string>());
        if (btn.contains(PropertyNames::kJsonRect) && btn[PropertyNames::kJsonRect].is_object())
            dlg->setCancelButtonRect(parseRect(btn[PropertyNames::kJsonRect]));
    }

    if (j.contains(PropertyNames::kJsonButtonHeight) && j[PropertyNames::kJsonButtonHeight].is_number())
        dlg->setButtonHeight(j[PropertyNames::kJsonButtonHeight].get<float>());
    if (j.contains(PropertyNames::kJsonButtonGap) && j[PropertyNames::kJsonButtonGap].is_number())
        dlg->setButtonGap(j[PropertyNames::kJsonButtonGap].get<float>());
    if (j.contains(PropertyNames::kJsonPadding) && j[PropertyNames::kJsonPadding].is_number())
        dlg->setPadding(j[PropertyNames::kJsonPadding].get<float>());

    parseEvents(dlg, j);
    parseBindings(dlg, j);

    if (j.contains(PropertyNames::kJsonId) && j[PropertyNames::kJsonId].is_string())
        m_controlsById[j[PropertyNames::kJsonId].get<string>()] = dlg;

    parseChildren(dlg, j);
    dlg->create();
    // Dialog 默认隐藏：布局加载时不弹出，由事件驱动 open() 挂树显示；
    // 显式 visible:true 时立即打开（open() 以 getVisible()==false 为前提）
    dlg->setVisible(false);
    if (j.value(PropertyNames::kJsonVisible, false)) {
        dlg->open();
    }
    return dlg;
}

// ==================== 通用属性解析 ====================

void LayoutParser::parseCommonProperties(shared_ptr<ControlImpl> ctrl, const json& j) {
    if (!ctrl) return;

    // scale
    if (j.contains(PropertyNames::kJsonScale) && j[PropertyNames::kJsonScale].is_object()) {
        float sx = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonX, 1.0f);
        float sy = j[PropertyNames::kJsonScale].value(PropertyNames::kJsonY, 1.0f);
        ctrl->setScaleX(sx);
        ctrl->setScaleY(sy);
    }

    // margin
    if (j.contains(PropertyNames::kJsonMargin)) {
        ctrl->setMargin(parseMargin(j[PropertyNames::kJsonMargin]));
    }

    // visible
    ctrl->setVisible(j.value(PropertyNames::kJsonVisible, true));

    // enabled
    ctrl->setEnable(j.value(PropertyNames::kJsonEnabled, true));

    // borderVisible
    if (j.contains(PropertyNames::kJsonBorderVisible) && j[PropertyNames::kJsonBorderVisible].is_boolean())
        ctrl->setBorderVisible(j[PropertyNames::kJsonBorderVisible].get<bool>());

    // colors
    if (j.contains(PropertyNames::kJsonColors) && j[PropertyNames::kJsonColors].is_object()) {
        pushJsonPath(PropertyNames::kJsonColors);
        const json& colors = j[PropertyNames::kJsonColors];
        if (colors.contains(PropertyNames::kBackground)) {
            ctrl->setBackgroundStateColor(
                parseStateColor(colors[PropertyNames::kBackground], StateColor::Type::Background));
        }
        if (colors.contains(PropertyNames::kBorder)) {
            ctrl->setBorderStateColor(
                parseStateColor(colors[PropertyNames::kBorder], StateColor::Type::Border));
        }
        if (colors.contains(PropertyNames::kJsonText)) {
            ctrl->setTextStateColor(
                parseStateColor(colors[PropertyNames::kJsonText], StateColor::Type::Text));
        }
        if (colors.contains(PropertyNames::kJsonTextShadow)) {
            ctrl->setTextShadowStateColor(
                parseStateColor(colors[PropertyNames::kJsonTextShadow], StateColor::Type::TextShadow));
        }
        popJsonPath();
    }
}

void LayoutParser::parseEvents(shared_ptr<ControlImpl> ctrl, const json& j) {
    if (!j.contains(PropertyNames::kJsonEvents) || j[PropertyNames::kJsonEvents].is_null() || !j[PropertyNames::kJsonEvents].is_object()) return;

    pushJsonPath(PropertyNames::kJsonEvents);
    const json& events = j[PropertyNames::kJsonEvents];

    // Button: onClick
    if (auto btn = dynamic_pointer_cast<Button>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyClick) && events[PropertyNames::kEventKeyClick].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyClick].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                btn->setOnClick([handler](shared_ptr<Button> sender) {
                    handler(sender);
                });
            }
        }
    }

    // Label: onClick (supports hyperlink mode)
    if (auto label = dynamic_pointer_cast<Label>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyClick) && events[PropertyNames::kEventKeyClick].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyClick].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                label->setOnClick([handler](shared_ptr<Label> sender) {
                    handler(sender);
                });
            }
        }
    }

    // EditBox & TextArea: onTextChanged, onEnter
    if (auto editBox = dynamic_pointer_cast<EditBox>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyTextChanged) && events[PropertyNames::kEventKeyTextChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyTextChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                editBox->setOnTextChanged([handler](shared_ptr<Control> sender, string) {
                    handler(sender);
                });
            }
        }

        if (events.contains(PropertyNames::kEventKeyEnter) && events[PropertyNames::kEventKeyEnter].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyEnter].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                editBox->setOnEnter([handler](shared_ptr<Control> sender) {
                    handler(sender);
                });
            }
        }
    }

    // ComboBox: onSelectionChanged
    if (auto combo = dynamic_pointer_cast<ComboBox>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeySelectionChanged) && events[PropertyNames::kEventKeySelectionChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeySelectionChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                combo->setOnSelectionChanged([handler](shared_ptr<ComboBox> sender, int, const string&) {
                    handler(sender);
                });
            }
        }
    }

    // CheckBox: onCheckChanged
    if (auto cb = dynamic_pointer_cast<CheckBox>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyCheckChanged) && events[PropertyNames::kEventKeyCheckChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyCheckChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                cb->setOnCheckChanged([handler](shared_ptr<CheckBox> sender, CheckState, CheckState) {
                    handler(sender);
                });
            }
        }
    }

    // TreeView: onSelect
    if (auto tv = dynamic_pointer_cast<TreeView>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeySelect) && events[PropertyNames::kEventKeySelect].is_string()) {
            string handlerName = events[PropertyNames::kEventKeySelect].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                tv->setOnSelect([handler, tv](shared_ptr<TreeView>, const string&) {
                    handler(tv);
                });
            }
        }
    }

    // ProgressBar: onValueChanged
    if (auto pb = dynamic_pointer_cast<ProgressBar>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyValueChanged) && events[PropertyNames::kEventKeyValueChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyValueChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                pb->setOnValueChanged([handler](shared_ptr<ProgressBar> sender, float, float) {
                    handler(sender);
                });
            }
        }
    }

    // ScrollBar: onPositionChanged
    if (auto sb = dynamic_pointer_cast<ScrollBar>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyPositionChanged) && events[PropertyNames::kEventKeyPositionChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyPositionChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                sb->setOnPositionChanged([handler](shared_ptr<ScrollBar> sender, float, float, float, float) {
                    handler(sender);
                });
            }
        }
    }

    // Slider: onValueChanged
    if (auto sl = dynamic_pointer_cast<Slider>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyValueChanged) && events[PropertyNames::kEventKeyValueChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyValueChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                sl->setOnValueChanged([handler](shared_ptr<Slider> sender, float) {
                    handler(sender);
                });
            }
        }
    }

    // ColorPicker: onColorChanged
    if (auto cp = dynamic_pointer_cast<ColorPicker>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyColorChanged) && events[PropertyNames::kEventKeyColorChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyColorChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                cp->setOnColorChanged([handler](shared_ptr<ColorPicker>, const SColor&) {
                    handler(nullptr);
                });
            }
        }
    }

    // Dialog/ConfirmPopup/Popup: onConfirm, onCancel, onClose
    if (auto dlg = dynamic_pointer_cast<Dialog>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyConfirm) && events[PropertyNames::kEventKeyConfirm].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyConfirm].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                dlg->setOnConfirm([handler](shared_ptr<ConfirmPopup> sender) {
                    handler(sender);
                });
            }
        }
        if (events.contains(PropertyNames::kEventKeyCancel) && events[PropertyNames::kEventKeyCancel].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyCancel].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                dlg->setOnCancel([handler](shared_ptr<Dialog> sender) {
                    handler(sender);
                });
            }
        }
        if (events.contains(PropertyNames::kEventKeyClose) && events[PropertyNames::kEventKeyClose].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyClose].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                dlg->setOnClose([handler](shared_ptr<Popup> sender, DialogResult) {
                    handler(sender);
                });
            }
        }
    } else if (auto cp = dynamic_pointer_cast<ConfirmPopup>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyConfirm) && events[PropertyNames::kEventKeyConfirm].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyConfirm].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                cp->setOnConfirm([handler](shared_ptr<ConfirmPopup> sender) {
                    handler(sender);
                });
            }
        }
    } else if (auto pop = dynamic_pointer_cast<Popup>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyClose) && events[PropertyNames::kEventKeyClose].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyClose].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                pop->setOnClose([handler](shared_ptr<Popup> sender, DialogResult) {
                    handler(sender);
                });
            }
        }
    }

    // MenuItem: onClick
    if (auto mi = dynamic_pointer_cast<MenuItem>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyClick) && events[PropertyNames::kEventKeyClick].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyClick].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                mi->setOnClick([handler](shared_ptr<MenuItem> sender) {
                    handler(sender);
                });
            }
        }
    }

    // NumericUpDown: onValueChanged
    if (auto nud = dynamic_pointer_cast<NumericUpDown>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeyValueChanged) && events[PropertyNames::kEventKeyValueChanged].is_string()) {
            string handlerName = events[PropertyNames::kEventKeyValueChanged].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                nud->setOnValueChanged([handler](shared_ptr<NumericUpDown>, double) {
                    handler(nullptr);
                });
            }
        }
    }

    // Splitter: onSplitterMoved
    if (auto sp = dynamic_pointer_cast<Splitter>(ctrl)) {
        if (events.contains(PropertyNames::kEventKeySplitterMoved) && events[PropertyNames::kEventKeySplitterMoved].is_string()) {
            string handlerName = events[PropertyNames::kEventKeySplitterMoved].get<string>();
            auto it = m_handlers.find(handlerName);
            if (it != m_handlers.end()) {
                auto handler = it->second;
                sp->setOnSplitterMoved([handler](shared_ptr<Splitter>, float) {
                    handler(nullptr);
                });
            }
        }
    }

    popJsonPath();
}

static void applyBinding(shared_ptr<ControlImpl> ctrl, const string& prop, const DataValue& val) {
    if (prop == PropertyNames::kJsonVisible) { ctrl->setVisible(val.asBool()); return; }
    if (prop == PropertyNames::kJsonEnabled) { ctrl->setEnable(val.asBool()); return; }

    if (prop == PropertyNames::kJsonCaption) {
        if (auto label = dynamic_pointer_cast<Label>(ctrl)) { label->setCaption(val.asString()); return; }
        if (auto btn = dynamic_pointer_cast<Button>(ctrl)) { btn->setCaption(val.asString()); return; }
    }
    if (prop == PropertyNames::kJsonText) {
        if (auto eb = dynamic_pointer_cast<EditBox>(ctrl)) { eb->setText(val.asString()); return; }
        if (auto ta = dynamic_pointer_cast<TextArea>(ctrl)) { ta->setText(val.asString()); return; }
    }
    if (prop == PropertyNames::kJsonPlaceholder) {
        if (auto eb = dynamic_pointer_cast<EditBox>(ctrl)) { eb->setPlaceholder(val.asString()); return; }
    }
    if (prop == PropertyNames::kJsonValue) {
        if (auto pb = dynamic_pointer_cast<ProgressBar>(ctrl)) { pb->setValue((float)val.asDouble()); return; }
        if (auto sb = dynamic_pointer_cast<ScrollBar>(ctrl)) { sb->setValue((float)val.asDouble()); return; }
    }
    if (prop == PropertyNames::kJsonCheckState) {
        if (auto cb = dynamic_pointer_cast<CheckBox>(ctrl)) {
            CheckState s = CheckState::Unchecked;
            string vs = val.asString();
            if (vs == PropertyNames::kCheckChecked) s = CheckState::Checked;
            else if (vs == PropertyNames::kCheckIndeterminate) s = CheckState::Indeterminate;
            cb->setCheckState(s);
            return;
        }
    }
}

static void bindProperty(DataContext* dataContext, shared_ptr<ControlImpl> ctrl, const string& prop, const string& source, const string& mode) {
    if (!dataContext) return;
    weak_ptr<ControlImpl> weakCtrl = ctrl;
    if (mode == PropertyNames::kBindModeOneWay || mode == PropertyNames::kBindModeTwoWay) {
        dataContext->watch(source, [weakCtrl, prop](const DataValue& val) {
            auto locked = dynamic_pointer_cast<ControlImpl>(weakCtrl.lock());
            if (locked) applyBinding(locked, prop, val);
        });
    }
    if (mode == PropertyNames::kBindModeTwoWay) {
        if (prop == PropertyNames::kJsonText) {
            if (auto eb = dynamic_pointer_cast<EditBox>(ctrl)) {
                auto s = source;
                eb->setOnTextChanged([s, dataContext](shared_ptr<Control>, string text) {
                    dataContext->set(s, text);
                });
            }
        }
    if (prop == PropertyNames::kJsonCheckState) {
            if (auto cb = dynamic_pointer_cast<CheckBox>(ctrl)) {
                auto s = source;
                shared_ptr<CheckBox> weakCB = cb;
                cb->setOnCheckChanged([s, dataContext, weakCB](shared_ptr<CheckBox>, CheckState, CheckState newState) {
                    string vs = PropertyNames::kCheckUnchecked;
                    if (newState == CheckState::Checked) vs = PropertyNames::kCheckChecked;
                    else if (newState == CheckState::Indeterminate) vs = PropertyNames::kCheckIndeterminate;
                    dataContext->set(s, vs);
                });
            }
        }
        if (prop == PropertyNames::kJsonValue) {
            if (auto sb = dynamic_pointer_cast<ScrollBar>(ctrl)) {
                auto s = source;
                sb->setOnPositionChanged([s, dataContext](shared_ptr<ScrollBar>, float, float newValue, float, float) {
                    dataContext->set(s, (double)newValue);
                });
            }
        }
    }

    // (bindProperty end)
}

void LayoutParser::parseBindings(shared_ptr<ControlImpl> ctrl, const json& j) {
    if (!j.contains(PropertyNames::kJsonBind) || !j[PropertyNames::kJsonBind].is_object()) return;
    pushJsonPath(PropertyNames::kJsonBind);
    const json& bind = j[PropertyNames::kJsonBind];
    for (auto it = bind.begin(); it != bind.end(); ++it) {
        string prop = it.key();
        string source;
        string mode = PropertyNames::kBindModeOneWay;
        if (it.value().is_string()) {
            source = it.value().get<string>();
        } else if (it.value().is_object()) {
            source = it.value().value(PropertyNames::kJsonSource, "");
            mode = it.value().value(PropertyNames::kJsonMode, PropertyNames::kBindModeOneWay);
        } else {
            continue;
        }
        bindProperty(m_dataContext, ctrl, prop, source, mode);
    }
    popJsonPath();
}

void LayoutParser::parseChildren(shared_ptr<Control> container, const json& j) {
    if (!j.contains(PropertyNames::kJsonChildren) || !j[PropertyNames::kJsonChildren].is_array()) return;

    pushJsonPath(PropertyNames::kJsonChildren);
    const json& children = j[PropertyNames::kJsonChildren];
    for (size_t i = 0; i < children.size(); ++i) {
        auto child = parseControl(children[i], container.get(), (int)i);
        if (child) {
            auto containerImpl = dynamic_pointer_cast<ControlImpl>(container);
            if (containerImpl) {
                containerImpl->addControl(child);
            }
        }
    }
    popJsonPath();
}

// ==================== 基础类型解析 ====================

SRect LayoutParser::parseRect(const json& j) {
    SRect rect;
    vector<string> missing;

    if (!j.is_object()) {
        pushJsonPath(PropertyNames::kJsonRect);
        logError("'rect' must be an object with {x, y, w, h}");
        popJsonPath();
        return rect;
    }

    auto parseField = [&](const string& key, float& pixel, bool& isPct, float& pct, float defaultVal) {
        if (!j.contains(key)) {
            pixel = defaultVal;
            return;
        }
        const json& v = j[key];
        if (v.is_string()) {
            string s = v.get<string>();
            if (s.size() > 1 && s.back() == '%') {
                isPct = true;
                pct = stof(s.substr(0, s.size() - 1));
                return;
            }
        }
        if (v.is_number()) {
            pixel = v.get<float>();
        }
    };

    parseField(PropertyNames::kJsonX, rect.left,   rect.leftIsPct,   rect.leftPct,   0.0f);
    parseField(PropertyNames::kJsonY, rect.top,    rect.topIsPct,    rect.topPct,    0.0f);
    parseField(PropertyNames::kJsonW, rect.width,  rect.widthIsPct,  rect.widthPct,  0.0f);
    parseField(PropertyNames::kJsonH, rect.height, rect.heightIsPct, rect.heightPct, 0.0f);

    if (!j.contains(PropertyNames::kJsonX)) missing.push_back(PropertyNames::kJsonX);
    if (!j.contains(PropertyNames::kJsonY)) missing.push_back(PropertyNames::kJsonY);
    if (!j.contains(PropertyNames::kJsonW)) missing.push_back(PropertyNames::kJsonW);
    if (!j.contains(PropertyNames::kJsonH)) missing.push_back(PropertyNames::kJsonH);

    if (!missing.empty()) {
        string fields;
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) fields += ", ";
            fields += missing[i];
        }
        logWarn("rect missing field(s): " + fields + ", defaulting to 0");
    }

    return rect;
}

Margin LayoutParser::parseMargin(const json& j) {
    Margin margin;

    if (!j.is_object()) {
        pushJsonPath(PropertyNames::kJsonMargin);
        logWarn("'margin' must be an object, using defaults");
        popJsonPath();
        return margin;
    }

    margin.left   = j.value(PropertyNames::kJsonLeft,   0.0f);
    margin.top    = j.value(PropertyNames::kJsonTop,    0.0f);
    margin.right  = j.value(PropertyNames::kJsonRight,  0.0f);
    margin.bottom = j.value(PropertyNames::kJsonBottom, 0.0f);

    return margin;
}

SColor LayoutParser::parseColor(const json& j) {
    if (j.is_string()) {
        string hex = j.get<string>();
        if (hex.empty() || hex[0] != '#') {
            logWarn("invalid color format \"" + hex + "\", using default white");
            return SColor(255, 255, 255, 255);
        }
        hex = hex.substr(1);

        if (hex.length() == 3) {
            uint8_t r = (uint8_t)stoi(string(2, hex[0]), nullptr, 16);
            uint8_t g = (uint8_t)stoi(string(2, hex[1]), nullptr, 16);
            uint8_t b = (uint8_t)stoi(string(2, hex[2]), nullptr, 16);
            return SColor(r, g, b, 255);
        } else if (hex.length() == 6) {
            uint8_t r = (uint8_t)stoi(hex.substr(0, 2), nullptr, 16);
            uint8_t g = (uint8_t)stoi(hex.substr(2, 2), nullptr, 16);
            uint8_t b = (uint8_t)stoi(hex.substr(4, 2), nullptr, 16);
            return SColor(r, g, b, 255);
        } else if (hex.length() == 8) {
            uint8_t r = (uint8_t)stoi(hex.substr(0, 2), nullptr, 16);
            uint8_t g = (uint8_t)stoi(hex.substr(2, 2), nullptr, 16);
            uint8_t b = (uint8_t)stoi(hex.substr(4, 2), nullptr, 16);
            uint8_t a = (uint8_t)stoi(hex.substr(6, 2), nullptr, 16);
            return SColor(r, g, b, a);
        } else {
            logWarn("invalid hex color length \"" + hex + "\", using default white");
        }
    } else if (j.is_object()) {
        uint8_t r = (uint8_t)j.value(PropertyNames::kChannelR, 255);
        uint8_t g = (uint8_t)j.value(PropertyNames::kChannelG, 255);
        uint8_t b = (uint8_t)j.value(PropertyNames::kChannelB, 255);
        uint8_t a = (uint8_t)j.value(PropertyNames::kChannelA, 255);
        return SColor(r, g, b, a);
    }

    return SColor(255, 255, 255, 255);
}

StateColor LayoutParser::parseStateColor(const json& j, StateColor::Type type) {
    StateColor stateColor(type);

    if (j.is_null()) return stateColor;

    if (j.contains(PropertyNames::kStateKeyNormal)) {
        pushJsonPath(PropertyNames::kStateKeyNormal);
        stateColor.setNormal(parseColor(j[PropertyNames::kStateKeyNormal]));
        popJsonPath();
    }
    if (j.contains(PropertyNames::kStateKeyHover)) {
        pushJsonPath(PropertyNames::kStateKeyHover);
        stateColor.setHover(parseColor(j[PropertyNames::kStateKeyHover]));
        popJsonPath();
    }
    if (j.contains(PropertyNames::kStateKeyPressed)) {
        pushJsonPath(PropertyNames::kStateKeyPressed);
        stateColor.setPressed(parseColor(j[PropertyNames::kStateKeyPressed]));
        popJsonPath();
    }
    if (j.contains(PropertyNames::kStateKeyDisabled)) {
        pushJsonPath(PropertyNames::kStateKeyDisabled);
        stateColor.setDisabled(parseColor(j[PropertyNames::kStateKeyDisabled]));
        popJsonPath();
    }

    return stateColor;
}

FontName LayoutParser::parseFontName(const string& name) {
    static const unordered_map<string, FontName> nameMap = {
        {"HarmonyOS_Sans_SC_Regular",   FontName::HarmonyOS_Sans_SC_Regular},
        {"HarmonyOS_Sans_SC_Bold",      FontName::HarmonyOS_Sans_SC_Bold},
        {"HarmonyOS_Sans_SC_Light",     FontName::HarmonyOS_Sans_SC_Light},
        {"HarmonyOS_Sans_SC_Thin",      FontName::HarmonyOS_Sans_SC_Thin},
        {"HarmonyOS_Sans_SC_Medium",    FontName::HarmonyOS_Sans_SC_Medium},
        {"HarmonyOS_Sans_SC_Black",     FontName::HarmonyOS_Sans_SC_Black},
        {"MapleMono_NF_CN_Regular",     FontName::MapleMono_NF_CN_Regular},
        {"MapleMono_NF_CN_Bold",        FontName::MapleMono_NF_CN_Bold},
        {"Muyao_Softbrush",             FontName::Muyao_Softbrush},
        {"Asul_Bold",                   FontName::Asul_Bold},
        {"Quando_Regular",              FontName::Quando_Regular},
    };

    auto it = nameMap.find(name);
    if (it != nameMap.end()) return it->second;

    logWarn("unknown font name \"" + name + "\", using default");
    return FontName::HarmonyOS_Sans_SC_Regular;
}

AlignmentMode LayoutParser::parseAlignment(const string& align) {
    static const unordered_map<string, AlignmentMode> alignMap = {
        {PropertyNames::kAlignLowerTopLeft,     AlignmentMode::AM_TOP_LEFT},
        {PropertyNames::kAlignLowerTopCenter,   AlignmentMode::AM_TOP_CENTER},
        {PropertyNames::kAlignLowerTopRight,    AlignmentMode::AM_TOP_RIGHT},
        {PropertyNames::kAlignLowerMidLeft,     AlignmentMode::AM_MID_LEFT},
        {PropertyNames::kAlignLowerCenter,      AlignmentMode::AM_CENTER},
        {PropertyNames::kAlignLowerMidRight,    AlignmentMode::AM_MID_RIGHT},
        {PropertyNames::kAlignLowerBottomLeft,  AlignmentMode::AM_BOTTOM_LEFT},
        {PropertyNames::kAlignLowerBottomCenter,AlignmentMode::AM_BOTTOM_CENTER},
        {PropertyNames::kAlignLowerBottomRight, AlignmentMode::AM_BOTTOM_RIGHT},
    };

    auto it = alignMap.find(align);
    if (it != alignMap.end()) return it->second;

    logWarn("unknown alignment \"" + align + "\", using TOP_LEFT");
    return AlignmentMode::AM_TOP_LEFT;
}

int LayoutParser::parseFontStyle(const string& style) {
    static const unordered_map<string, int> styleMap = {
        {PropertyNames::kFontStyleNormal,        0},
        {PropertyNames::kFontStyleBold,          1},
        {PropertyNames::kFontStyleItalic,        2},
        {PropertyNames::kFontStyleUnderline,     4},
        {PropertyNames::kFontStyleStrikethrough, 8},
    };

    auto it = styleMap.find(style);
    if (it != styleMap.end()) return it->second;

    return 0;
}

GridSize LayoutParser::parseGridSize(const json& j) {
    GridSize gs;
    if (j.is_string()) {
        string s = j.get<string>();
        if (s == PropertyNames::kGridSizeAuto) {
            gs.type = GridSize::Auto;
        } else if (s.size() > 2 && s.substr(s.size() - 2) == PropertyNames::kGridSizeFrSuffix) {
            gs.type = GridSize::Flex;
            gs.value = stof(s.substr(0, s.size() - 2));
        } else if (s.size() > 2 && s.substr(s.size() - 2) == PropertyNames::kGridSizePxSuffix) {
            gs.type = GridSize::Fixed;
            gs.value = stof(s.substr(0, s.size() - 2));
        } else {
            gs.type = GridSize::Fixed;
            gs.value = stof(s);
        }
    } else if (j.is_number()) {
        gs.type = GridSize::Fixed;
        gs.value = j.get<float>();
    }
    return gs;
}

// ==================== 组件系统 ====================

void LayoutParser::parseComponents(const json& j) {
    if (!j.contains(PropertyNames::kJsonComponents) || !j[PropertyNames::kJsonComponents].is_object()) return;

    pushJsonPath(PropertyNames::kJsonComponents);
    const json& comps = j[PropertyNames::kJsonComponents];

    // Known control type names to check against
    unordered_set<string> knownTypes = {
        PropertyNames::kControlTypeLabel, PropertyNames::kControlTypeButton, PropertyNames::kControlTypeEditBox,
        PropertyNames::kControlTypeComboBox, PropertyNames::kControlTypeTextArea, PropertyNames::kControlTypeCheckBox,
        PropertyNames::kControlTypeProgressBar, PropertyNames::kControlTypeScrollBar, PropertyNames::kControlTypePanel,
        PropertyNames::kControlTypeWinFrame, PropertyNames::kControlTypeMenuBar,
        PropertyNames::kControlTypeColorPicker, PropertyNames::kControlTypeSlider, PropertyNames::kControlTypePopup,
        PropertyNames::kControlTypeConfirmPopup, PropertyNames::kControlTypeDialog,
        PropertyNames::kControlTypeTreeView
    };

    for (auto it = comps.begin(); it != comps.end(); ++it) {
        const string& name = it.key();
        const json& def = it.value();

        if (knownTypes.find(name) != knownTypes.end()) {
            logWarn("component name \"" + name + "\" conflicts with built-in control type");
            continue;
        }

        if (!def.contains(PropertyNames::kJsonTemplate) || !def[PropertyNames::kJsonTemplate].is_object()) {
            logWarn("component \"" + name + "\" is missing 'template' object");
            continue;
        }

        if (def.contains(PropertyNames::kJsonProps) && !def[PropertyNames::kJsonProps].is_object()) {
            logWarn("component \"" + name + "\" 'props' must be an object");
            continue;
        }

        // Validate props
        if (def.contains(PropertyNames::kJsonProps)) {
            const json& props = def[PropertyNames::kJsonProps];
            bool propsValid = true;
            for (auto p = props.begin(); p != props.end(); ++p) {
                if (!p.value().contains(PropertyNames::kJsonType) || !p.value()[PropertyNames::kJsonType].is_string()) {
                    logWarn("component \"" + name + "\" prop \"" + p.key() + "\" missing 'type' field");
                    propsValid = false;
                } else {
                    string ptype = p.value()[PropertyNames::kJsonType].get<string>();
                    if (ptype != PropertyNames::kPropTypeString && ptype != PropertyNames::kPropTypeNumber && ptype != PropertyNames::kPropTypeBool) {
                        logWarn("component \"" + name + "\" prop \"" + p.key() + "\" invalid type \"" + ptype + "\"");
                        propsValid = false;
                    }
                }
            }
            if (!propsValid) continue;
        }

        m_components[name] = def;

        // Record source line number (approximate from JSON content position)
        ComponentSource src;
        src.name = name;
        // Try to get the component's approximate position in the raw content
        string searchStr = "\"" + name + "\"";
        size_t pos = m_rawJsonContent.find(searchStr);
        if (pos != string::npos) {
            src.lineStart = byteOffsetToLineNo(m_rawJsonContent, pos);
            src.lineEnd = src.lineStart;  // approximate
        } else {
            src.lineStart = 0;
            src.lineEnd = 0;
        }
        m_componentSourceLines[name] = src;
    }

    popJsonPath();
}

void LayoutParser::replacePlaceholders(json& node, const json& props, const json& instanceJ) {
    if (node.is_string()) {
        string val = node.get<string>();
        size_t start = val.find("{{");
        if (start != string::npos) {
            size_t end = val.find("}}", start);
            if (end != string::npos) {
                string propName = val.substr(start + 2, end - start - 2);
                // Look up in instanceJ first, then props default
                if (instanceJ.contains(propName)) {
                    const json& propVal = instanceJ[propName];
                    if (propVal.is_string()) {
                        val.replace(start, end - start + 2, propVal.get<string>());
                        node = val;
                    } else if (propVal.is_number()) {
                        val.replace(start, end - start + 2, to_string(propVal.get<double>()));
                        node = val;
                    } else if (propVal.is_boolean()) {
                        val.replace(start, end - start + 2, propVal.get<bool>() ? "true" : "false");
                        node = val;
                    }
                } else if (props.contains(propName) && props[propName].contains(PropertyNames::kJsonDefault)) {
                    // Use default value
                    const json& def = props[propName][PropertyNames::kJsonDefault];
                    if (def.is_string()) {
                        val.replace(start, end - start + 2, def.get<string>());
                        node = val;
                    } else if (def.is_number()) {
                        val.replace(start, end - start + 2, to_string(def.get<double>()));
                        node = val;
                    } else if (def.is_boolean()) {
                        val.replace(start, end - start + 2, def.get<bool>() ? "true" : "false");
                        node = val;
                    }
                } else {
                    logWarn("placeholder \"{{" + propName + "}}\" not provided and no default");
                }
            }
        }
    } else if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            replacePlaceholders(it.value(), props, instanceJ);
        }
    } else if (node.is_array()) {
        for (auto& item : node) {
            replacePlaceholders(item, props, instanceJ);
        }
    }
}

void LayoutParser::remapEvents(json& node, const json& instanceEvents) {
    if (node.is_object()) {
        // Check if this node has events
        if (node.contains(PropertyNames::kJsonEvents) && node[PropertyNames::kJsonEvents].is_object()) {
            json& events = node[PropertyNames::kJsonEvents];
            vector<string> keysToRemove;
            for (auto it = events.begin(); it != events.end(); ++it) {
                if (it.value().is_string()) {
                    string handlerName = it.value().get<string>();
                    if (handlerName.find("_comp_") == 0) {
                        // Internal event: _comp_onSearch -> look for onSearch in instanceEvents
                        string eventKey = handlerName.substr(6);  // Remove "_comp_"
                        if (instanceEvents.contains(eventKey) && instanceEvents[eventKey].is_string()) {
                            it.value() = instanceEvents[eventKey].get<string>();
                        } else {
                            logWarn("event \"_comp_" + eventKey + "\" has no mapping in instance events");
                        }
                    }
                }
            }
        }

        // Recurse into children
        for (auto it = node.begin(); it != node.end(); ++it) {
            remapEvents(it.value(), instanceEvents);
        }
    } else if (node.is_array()) {
        for (auto& item : node) {
            remapEvents(item, instanceEvents);
        }
    }
}

void LayoutParser::prefixIds(json& node, const string& prefix) {
    if (node.is_object()) {
        // Prefix the id if present
        if (node.contains(PropertyNames::kJsonId) && node[PropertyNames::kJsonId].is_string()) {
            string originalId = node[PropertyNames::kJsonId].get<string>();
            node[PropertyNames::kJsonId] = prefix + "__" + originalId;
        }

        // Recurse
        for (auto it = node.begin(); it != node.end(); ++it) {
            prefixIds(it.value(), prefix);
        }
    } else if (node.is_array()) {
        for (auto& item : node) {
            prefixIds(item, prefix);
        }
    }
}

shared_ptr<Control> LayoutParser::instantiateComponent(const string& name, const json& instanceJ, Control* parent, int index) {
    string indexPath = "controls[" + to_string(index) + "]";
    pushJsonPath(indexPath);

    // Detect circular reference
    if (find(m_instantiationStack.begin(), m_instantiationStack.end(), name) != m_instantiationStack.end()) {
        logWarn("circular component reference detected: \"" + name + "\"");
        popJsonPath();
        return nullptr;
    }

    m_instantiationStack.push_back(name);

    auto compIt = m_components.find(name);
    if (compIt == m_components.end()) {
        logWarn("component \"" + name + "\" not found");
        m_instantiationStack.pop_back();
        popJsonPath();
        return nullptr;
    }

    const json& compDef = compIt->second;
    const json& templateJ = compDef[PropertyNames::kJsonTemplate];
    const json props = compDef.contains(PropertyNames::kJsonProps) ? compDef[PropertyNames::kJsonProps] : json::object();

    // Get source line info for better error messages
    auto srcIt = m_componentSourceLines.find(name);
    string srcInfo = "";
    if (srcIt != m_componentSourceLines.end() && srcIt->second.lineStart > 0) {
        srcInfo = " [defined at line " + to_string(srcIt->second.lineStart) + "]";
    }

    pushJsonPath("component:" + name + srcInfo + " instantiated at");

    // Deep copy the template JSON
    json expanded = templateJ;

    // Replace placeholders
    replacePlaceholders(expanded, props, instanceJ);

    // Remap events: _comp_xxx -> instance events
    json instanceEvents = instanceJ.contains(PropertyNames::kJsonEvents) ? instanceJ[PropertyNames::kJsonEvents] : json::object();
    remapEvents(expanded, instanceEvents);

    // Prefix IDs for uniqueness (BEFORE injecting instance id/rect, so root ID isn't double-prefixed)
    string idPrefix = instanceJ.contains(PropertyNames::kJsonId) && instanceJ[PropertyNames::kJsonId].is_string()
        ? instanceJ[PropertyNames::kJsonId].get<string>()
        : PropertyNames::kCompEventPrefix + name;
    prefixIds(expanded, idPrefix);

    // Inject instance attributes LAST so they override template/defaults without double-prefixing
    if (instanceJ.contains(PropertyNames::kJsonId) && instanceJ[PropertyNames::kJsonId].is_string()) {
        expanded[PropertyNames::kJsonId] = instanceJ[PropertyNames::kJsonId].get<string>();
    }
    if (instanceJ.contains(PropertyNames::kJsonRect) && instanceJ[PropertyNames::kJsonRect].is_object()) {
        expanded[PropertyNames::kJsonRect] = instanceJ[PropertyNames::kJsonRect];
    }
    // Forward other standard control attributes
    for (const string& attr : {"xScale", "yScale", "enable", "visible"}) {
        if (instanceJ.contains(attr)) {
            expanded[attr] = instanceJ[attr];
        }
    }

    // Parse the expanded template
    shared_ptr<Control> result = parseControl(expanded, parent, index);

    m_instantiationStack.pop_back();
    popJsonPath();
    popJsonPath();
    return result;
}
