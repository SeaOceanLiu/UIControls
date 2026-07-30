#pragma once

#ifdef __cplusplus
#include <cstddef>
#define PROP_NS_BEGIN namespace PropertyNames {
#define PROP_NS_END }
#define PROP_CONSTEXPR inline constexpr
#else
#include <stddef.h>
#define PROP_NS_BEGIN
#define PROP_NS_END
#define PROP_CONSTEXPR static
#endif

PROP_NS_BEGIN

// -- 通用颜色 --
PROP_CONSTEXPR const char* kBackground          = "background";
PROP_CONSTEXPR const char* kBorder              = "border";
PROP_CONSTEXPR const char* kText                = "text";
PROP_CONSTEXPR const char* kTextShadow          = "text-shadow";

// StateColor 子键
PROP_CONSTEXPR const char* kStateHover          = "background.hover";
PROP_CONSTEXPR const char* kStatePressed        = "background.pressed";
PROP_CONSTEXPR const char* kStateDisabled       = "background.disabled";
PROP_CONSTEXPR const char* kBorderHover         = "border.hover";
PROP_CONSTEXPR const char* kBorderPressed       = "border.pressed";
PROP_CONSTEXPR const char* kBorderDisabled      = "border.disabled";
PROP_CONSTEXPR const char* kTextHover           = "text.hover";
PROP_CONSTEXPR const char* kTextPressed         = "text.pressed";
PROP_CONSTEXPR const char* kTextDisabled        = "text.disabled";
PROP_CONSTEXPR const char* kTextShadowHover     = "text-shadow.hover";
PROP_CONSTEXPR const char* kTextShadowPressed   = "text-shadow.pressed";
PROP_CONSTEXPR const char* kTextShadowDisabled  = "text-shadow.disabled";

// -- 控件特有颜色 --
PROP_CONSTEXPR const char* kTrack               = "track";
PROP_CONSTEXPR const char* kTrackFill           = "track-fill";
PROP_CONSTEXPR const char* kThumb               = "thumb";
PROP_CONSTEXPR const char* kThumbBorder         = "thumb-border";
PROP_CONSTEXPR const char* kThumbHover          = "thumb-hover";
PROP_CONSTEXPR const char* kThumbPressed        = "thumb-pressed";
PROP_CONSTEXPR const char* kTick                = "tick";
PROP_CONSTEXPR const char* kLabelColor          = "label";
PROP_CONSTEXPR const char* kArrow               = "arrow";
PROP_CONSTEXPR const char* kArrowHover          = "arrow-hover";
PROP_CONSTEXPR const char* kArrowPressed        = "arrow-pressed";
PROP_CONSTEXPR const char* kItemSelected        = "item-selected";
PROP_CONSTEXPR const char* kItemHover           = "item-hover";
PROP_CONSTEXPR const char* kItemDisabled        = "item-disabled";
PROP_CONSTEXPR const char* kListBg              = "list-bg";
PROP_CONSTEXPR const char* kListBorder          = "list-border";
PROP_CONSTEXPR const char* kCheck               = "check";
PROP_CONSTEXPR const char* kCross               = "cross";
PROP_CONSTEXPR const char* kIndeterminate       = "indeterminate";
PROP_CONSTEXPR const char* kBoxBorder           = "box-border";
PROP_CONSTEXPR const char* kProgress            = "progress";
PROP_CONSTEXPR const char* kLine                = "line";
PROP_CONSTEXPR const char* kLineHover           = "line-hover";
PROP_CONSTEXPR const char* kLineDrag            = "line-drag";
PROP_CONSTEXPR const char* kWinFrameBG          = "win-frame-bg";
PROP_CONSTEXPR const char* kWinFrameBorder      = "win-frame-border";
PROP_CONSTEXPR const char* kTitleBarBG          = "title-bar-bg";
PROP_CONSTEXPR const char* kTitleText           = "title-text";
PROP_CONSTEXPR const char* kClosedText          = "closed-text";
PROP_CONSTEXPR const char* kPopupBG             = "popup-bg";

// TreeView 颜色
PROP_CONSTEXPR const char* kTreeSelected        = "selected";
PROP_CONSTEXPR const char* kTreeHover           = "hover";

// -- 通用 Bool --
PROP_CONSTEXPR const char* kVisible             = "visible";
PROP_CONSTEXPR const char* kEnabled             = "enabled";
PROP_CONSTEXPR const char* kTransparent         = "transparent";
PROP_CONSTEXPR const char* kBorderVisible       = "border-visible";

// -- Bool 属性 --
PROP_CONSTEXPR const char* kTextShadowEnable    = "text-shadow-enable";
PROP_CONSTEXPR const char* kShadow              = "shadow";
PROP_CONSTEXPR const char* kExpand              = "expand";
PROP_CONSTEXPR const char* kClickable           = "clickable";
PROP_CONSTEXPR const char* kPasswordMode        = "password-mode";
PROP_CONSTEXPR const char* kWordWrap            = "word-wrap";
PROP_CONSTEXPR const char* kTriState            = "tri-state";
PROP_CONSTEXPR const char* kReverse             = "reverse";
PROP_CONSTEXPR const char* kShowValueLabel      = "show-value-label";
PROP_CONSTEXPR const char* kCycleEnabled        = "cycle-enabled";
PROP_CONSTEXPR const char* kCycleNavigation     = "cycle-navigation";
PROP_CONSTEXPR const char* kDefaultExpand       = "default-expand";
PROP_CONSTEXPR const char* kResizable           = "resizable";
PROP_CONSTEXPR const char* kCloseOnClickOutside = "close-on-click-outside";
PROP_CONSTEXPR const char* kCloseOnEsc          = "close-on-esc";
PROP_CONSTEXPR const char* kConfirmVisible      = "confirm-visible";
PROP_CONSTEXPR const char* kReadOnly            = "read-only";
PROP_CONSTEXPR const char* kChecked             = "checked";
PROP_CONSTEXPR const char* kHorizontal          = "horizontal";

// TreeView 树操作
PROP_CONSTEXPR const char* kExpandAll           = "expand-all";
PROP_CONSTEXPR const char* kCollapseAll         = "collapse-all";

// -- Int 属性 --
PROP_CONSTEXPR const char* kFontSize            = "font-size";
PROP_CONSTEXPR const char* kLineHeight          = "line-height";
PROP_CONSTEXPR const char* kScrollX             = "scroll-x";
PROP_CONSTEXPR const char* kScrollY             = "scroll-y";
PROP_CONSTEXPR const char* kLabelFontSize       = "label-font-size";
PROP_CONSTEXPR const char* kPresetCols          = "preset-cols";
PROP_CONSTEXPR const char* kPresetRows          = "preset-rows";
PROP_CONSTEXPR const char* kClosedFontSize      = "closed-font-size";
PROP_CONSTEXPR const char* kMaxVisibleItems     = "max-visible-items";
PROP_CONSTEXPR const char* kSelectedIndex       = "selected-index";
PROP_CONSTEXPR const char* kHoveredIndex        = "hovered-index";
PROP_CONSTEXPR const char* kDecimals            = "decimals";

// -- Float 属性 --
PROP_CONSTEXPR const char* kLineSpacingRatio    = "line-spacing-ratio";
PROP_CONSTEXPR const char* kSizeRatio           = "size-ratio";
PROP_CONSTEXPR const char* kStep                = "step";
PROP_CONSTEXPR const char* kValue               = "value";
PROP_CONSTEXPR const char* kTrackThickness      = "track-thickness";
PROP_CONSTEXPR const char* kThumbSize           = "thumb-size";
PROP_CONSTEXPR const char* kTickInterval        = "tick-interval";
PROP_CONSTEXPR const char* kTickLength          = "tick-length";
PROP_CONSTEXPR const char* kLabelGap            = "label-gap";
PROP_CONSTEXPR const char* kRangeMin            = "range-min";
PROP_CONSTEXPR const char* kRangeMax            = "range-max";
PROP_CONSTEXPR const char* kAnimationSpeed      = "animation-speed";
PROP_CONSTEXPR const char* kScrollbarThickness  = "scrollbar-thickness";
PROP_CONSTEXPR const char* kPageSize            = "page-size";
PROP_CONSTEXPR const char* kStepSize            = "step-size";
PROP_CONSTEXPR const char* kThickness           = "thickness";
PROP_CONSTEXPR const char* kEdgeMargin          = "edge-margin";
PROP_CONSTEXPR const char* kBarHeight           = "bar-height";
PROP_CONSTEXPR const char* kItemHeightRatio     = "item-height-ratio";
PROP_CONSTEXPR const char* kArrowWidth          = "arrow-width";
PROP_CONSTEXPR const char* kItemHeight          = "item-height";
PROP_CONSTEXPR const char* kButtonHeight        = "button-height";
PROP_CONSTEXPR const char* kButtonGap           = "button-gap";
PROP_CONSTEXPR const char* kPadding             = "padding";
PROP_CONSTEXPR const char* kIndentWidth         = "indent-width";
PROP_CONSTEXPR const char* kRowHeight           = "row-height";
PROP_CONSTEXPR const char* kLineSpacing         = "line-spacing";
PROP_CONSTEXPR const char* kArrowGap            = "arrow-gap";
PROP_CONSTEXPR const char* kRatio               = "ratio";
PROP_CONSTEXPR const char* kClosedSwatchSize    = "closed-swatch-size";
PROP_CONSTEXPR const char* kPageStep            = "page-step";
PROP_CONSTEXPR const char* kButtonWidth         = "button-width";
PROP_CONSTEXPR const char* kCaptionSize         = "caption-size";

// Splitter
PROP_CONSTEXPR const char* kFirstMin            = "first-min";
PROP_CONSTEXPR const char* kSecondMin           = "second-min";

// -- String 属性 --
PROP_CONSTEXPR const char* kCaption             = "caption";
PROP_CONSTEXPR const char* kTextContent         = "text";
PROP_CONSTEXPR const char* kPlaceholder         = "placeholder";
PROP_CONSTEXPR const char* kSelectedValue       = "selected-value";
PROP_CONSTEXPR const char* kCustomText          = "custom-text";
PROP_CONSTEXPR const char* kLabelFormat         = "label-format";
PROP_CONSTEXPR const char* kShortcut            = "shortcut";
PROP_CONSTEXPR const char* kTitle               = "title";
PROP_CONSTEXPR const char* kConfirmText         = "confirm-text";
PROP_CONSTEXPR const char* kCancelText          = "cancel-text";
PROP_CONSTEXPR const char* kColor               = "color";
PROP_CONSTEXPR const char* kAnimation           = "animation";
PROP_CONSTEXPR const char* kItems               = "items";

// TreeView
PROP_CONSTEXPR const char* kTreeExpand          = "expand";
PROP_CONSTEXPR const char* kTreeCollapse        = "collapse";
PROP_CONSTEXPR const char* kSelectedId          = "selected-id";
PROP_CONSTEXPR const char* kSelectedUserData    = "selected-user-data";

// -- Ptr 属性 --
PROP_CONSTEXPR const char* kContent             = "content";
PROP_CONSTEXPR const char* kFirstLinked         = "first-linked";
PROP_CONSTEXPR const char* kSecondLinked        = "second-linked";

// -- Enum 属性名 --
PROP_CONSTEXPR const char* kAlign               = "align";
PROP_CONSTEXPR const char* kFont                = "font";
PROP_CONSTEXPR const char* kCheckBoxStyle       = "style";
PROP_CONSTEXPR const char* kProgressBarStyle    = "style";
PROP_CONSTEXPR const char* kSliderStyle         = "style";
PROP_CONSTEXPR const char* kLayout              = "layout";
PROP_CONSTEXPR const char* kVerticalAlign       = "vertical-align";
PROP_CONSTEXPR const char* kTextMode            = "text-mode";
PROP_CONSTEXPR const char* kOrientation         = "orientation";
PROP_CONSTEXPR const char* kCheckState          = "check-state";
PROP_CONSTEXPR const char* kLabelFont           = "label-font";
PROP_CONSTEXPR const char* kCenteredMode        = "centered-mode";

// -- 枚举值常量 --

// CheckBoxStyle
PROP_CONSTEXPR const char* kStyleClassic        = "classic";
PROP_CONSTEXPR const char* kStyleCross          = "cross";
PROP_CONSTEXPR const char* kStyleCircle         = "circle";

// CheckState
PROP_CONSTEXPR const char* kCheckUnchecked      = "unchecked";
PROP_CONSTEXPR const char* kCheckChecked        = "checked";
PROP_CONSTEXPR const char* kCheckIndeterminate  = "indeterminate";

// ProgressBarStyle / SliderStyle / Orientation
PROP_CONSTEXPR const char* kOrientHorizontal    = "horizontal";
PROP_CONSTEXPR const char* kOrientVertical      = "vertical";

// ProgressBarTextMode
PROP_CONSTEXPR const char* kTextModeNone        = "none";
PROP_CONSTEXPR const char* kTextModePercent     = "percent";
PROP_CONSTEXPR const char* kTextModeCustom      = "custom";

// CheckBoxLayout
PROP_CONSTEXPR const char* kLayoutTextRight     = "text-right";
PROP_CONSTEXPR const char* kLayoutTextLeft      = "text-left";

// VerticalAlign
PROP_CONSTEXPR const char* kVAlignCenter        = "center";
PROP_CONSTEXPR const char* kVAlignTop           = "top";
PROP_CONSTEXPR const char* kVAlignBottom        = "bottom";

// TextAlign
PROP_CONSTEXPR const char* kAlignTopLeft        = "top-left";
PROP_CONSTEXPR const char* kAlignMidLeft        = "mid-left";
PROP_CONSTEXPR const char* kAlignBottomLeft     = "bottom-left";
PROP_CONSTEXPR const char* kAlignTopRight       = "top-right";
PROP_CONSTEXPR const char* kAlignMidRight       = "mid-right";
PROP_CONSTEXPR const char* kAlignBottomRight    = "bottom-right";
PROP_CONSTEXPR const char* kAlignTopCenter      = "top-center";
PROP_CONSTEXPR const char* kAlignCenter         = "center";
PROP_CONSTEXPR const char* kAlignBottomCenter   = "bottom-center";

// CenteredMode
PROP_CONSTEXPR const char* kCentered            = "centered";

// -- Font 枚举值 --
PROP_CONSTEXPR const char* kFontAsulBold                    = "asul-bold";
PROP_CONSTEXPR const char* kFontAsulRegular                 = "asul-regular";
PROP_CONSTEXPR const char* kFontHarmonySansCondensedRegular = "harmonyos-sans-condensed-regular";
PROP_CONSTEXPR const char* kFontHarmonySansCondensedThin    = "harmonyos-sans-condensed-thin";
PROP_CONSTEXPR const char* kFontHarmonySansSCBlack          = "harmonyos-sans-sc-black";
PROP_CONSTEXPR const char* kFontHarmonySansSCBold           = "harmonyos-sans-sc-bold";
PROP_CONSTEXPR const char* kFontHarmonySansSCLight          = "harmonyos-sans-sc-light";
PROP_CONSTEXPR const char* kFontHarmonySansSCMedium         = "harmonyos-sans-sc-medium";
PROP_CONSTEXPR const char* kFontHarmonySansSCRegular        = "harmonyos-sans-sc-regular";
PROP_CONSTEXPR const char* kFontHarmonySansSCThin           = "harmonyos-sans-sc-thin";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNBold           = "maplemono-nf-cn-bold";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNBoldItalic     = "maplemono-nf-cn-bolditalic";
PROP_CONSTEXPR const char* kFontMapleMonoCNFExtraBold       = "maplemono-nf-cn-extrabold";
PROP_CONSTEXPR const char* kFontMapleMonoCNFExtraBoldItalic = "maplemono-nf-cn-extrabolditalic";
PROP_CONSTEXPR const char* kFontMapleMonoCNFExtraLight      = "maplemono-nf-cn-extralight";
PROP_CONSTEXPR const char* kFontMapleMonoCNFExtraLightItalic= "maplemono-nf-cn-extralightitalic";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNItalic         = "maplemono-nf-cn-italic";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNLight          = "maplemono-nf-cn-light";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNLightItalic    = "maplemono-nf-cn-lightitalic";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNMedium         = "maplemono-nf-cn-medium";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNMediumItalic   = "maplemono-nf-cn-mediumitalic";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNRegular        = "maplemono-nf-cn-regular";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNSemiBold       = "maplemono-nf-cn-semibold";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNSemiBoldItalic = "maplemono-nf-cn-semibolditalic";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNThin           = "maplemono-nf-cn-thin";
PROP_CONSTEXPR const char* kFontMapleMonoNFCNThinItalic     = "maplemono-nf-cn-thinitalic";
PROP_CONSTEXPR const char* kFontMuYaoSoftBrush              = "muyao-softbrush";
PROP_CONSTEXPR const char* kFontQuandoRegular               = "quando-regular";

// -- Events --
PROP_CONSTEXPR const char* kEventClick            = "click";
PROP_CONSTEXPR const char* kEventEnter            = "enter";
PROP_CONSTEXPR const char* kEventTextChanged      = "text-changed";
PROP_CONSTEXPR const char* kEventClose            = "close";
PROP_CONSTEXPR const char* kEventConfirm          = "confirm";
PROP_CONSTEXPR const char* kEventCancel           = "cancel";
PROP_CONSTEXPR const char* kEventCheckChanged     = "check-changed";
PROP_CONSTEXPR const char* kEventValueChanged     = "value-changed";
PROP_CONSTEXPR const char* kEventPositionChanged  = "position-changed";
PROP_CONSTEXPR const char* kEventSelectionChanged = "selection-changed";
PROP_CONSTEXPR const char* kEventMoved            = "moved";
PROP_CONSTEXPR const char* kEventColorChanged     = "color-changed";
PROP_CONSTEXPR const char* kEventSelect           = "select";
PROP_CONSTEXPR const char* kEventExpand           = "expand";
PROP_CONSTEXPR const char* kEventCollapse         = "collapse";
PROP_CONSTEXPR const char* kEventInitial          = "initial";
PROP_CONSTEXPR const char* kEventPropertyChanged  = "property-changed";
PROP_CONSTEXPR const char* kEventNodeRemoved       = "node-removed";

PROP_NS_END

#undef PROP_CONSTEXPR
#undef PROP_NS_BEGIN
#undef PROP_NS_END
