#pragma once

namespace PropertyNames {

// ── 通用颜色（全控件 StateColor + Color 入口）──
inline constexpr const char* kBackground      = "background";
inline constexpr const char* kBorder          = "border";
inline constexpr const char* kText            = "text";
inline constexpr const char* kTextShadow      = "text-shadow";

// ── 控件特有颜色 ──
inline constexpr const char* kTrack           = "track";
inline constexpr const char* kTrackFill       = "track-fill";
inline constexpr const char* kThumb           = "thumb";
inline constexpr const char* kThumbBorder     = "thumb-border";
inline constexpr const char* kThumbHover      = "thumb-hover";
inline constexpr const char* kTick            = "tick";
inline constexpr const char* kLabelColor      = "label";
inline constexpr const char* kArrow           = "arrow";
inline constexpr const char* kArrowHover      = "arrow-hover";
inline constexpr const char* kArrowPressed    = "arrow-pressed";
inline constexpr const char* kItemSelected    = "item-selected";
inline constexpr const char* kItemHover       = "item-hover";
inline constexpr const char* kItemDisabled    = "item-disabled";
inline constexpr const char* kListBg          = "list-bg";
inline constexpr const char* kListBorder      = "list-border";
inline constexpr const char* kCheck           = "check";
inline constexpr const char* kCross           = "cross";
inline constexpr const char* kIndeterminate   = "indeterminate";
inline constexpr const char* kBoxBorder       = "box-border";
inline constexpr const char* kProgress        = "progress";
inline constexpr const char* kLine            = "line";
inline constexpr const char* kLineHover       = "line-hover";
inline constexpr const char* kLineDrag        = "line-drag";
inline constexpr const char* kWinFrameBG      = "win-frame-bg";
inline constexpr const char* kWinFrameBorder  = "win-frame-border";
inline constexpr const char* kTitleBarBG      = "title-bar-bg";
inline constexpr const char* kTitleText       = "title-text";
inline constexpr const char* kClosedText      = "closed-text";
inline constexpr const char* kPopupBG         = "popup-bg";

// ── 通用 Bool（全控件）──
inline constexpr const char* kVisible         = "visible";
inline constexpr const char* kEnabled         = "enabled";
inline constexpr const char* kTransparent     = "transparent";
inline constexpr const char* kBorderVisible   = "border-visible";

// ── Bool ──
inline constexpr const char* kTextShadowEnable  = "text-shadow-enable";
inline constexpr const char* kShadow            = "shadow";
inline constexpr const char* kExpand            = "expand";
inline constexpr const char* kClickable         = "clickable";
inline constexpr const char* kPasswordMode      = "password-mode";
inline constexpr const char* kWordWrap          = "word-wrap";
inline constexpr const char* kTriState          = "tri-state";
inline constexpr const char* kReverse           = "reverse";
inline constexpr const char* kShowValueLabel    = "show-value-label";
inline constexpr const char* kCycleEnabled      = "cycle-enabled";
inline constexpr const char* kCycleNavigation   = "cycle-navigation";
inline constexpr const char* kDefaultExpand     = "default-expand";
inline constexpr const char* kResizable         = "resizable";
inline constexpr const char* kCloseOnClickOutside = "close-on-click-outside";
inline constexpr const char* kCloseOnEsc        = "close-on-esc";
inline constexpr const char* kConfirmVisible    = "confirm-visible";
inline constexpr const char* kReadOnly          = "read-only";
inline constexpr const char* kChecked           = "checked";
inline constexpr const char* kHorizontal        = "horizontal";

// ── Int ──
inline constexpr const char* kFontSize         = "font-size";
inline constexpr const char* kLineHeight       = "line-height";
inline constexpr const char* kScrollX          = "scroll-x";
inline constexpr const char* kScrollY          = "scroll-y";
inline constexpr const char* kLabelFontSize    = "label-font-size";
inline constexpr const char* kPresetCols       = "preset-cols";
inline constexpr const char* kPresetRows       = "preset-rows";
inline constexpr const char* kClosedFontSize   = "closed-font-size";
inline constexpr const char* kMaxVisibleItems  = "max-visible-items";
inline constexpr const char* kSelectedIndex    = "selected-index";
inline constexpr const char* kHoveredIndex     = "hovered-index";
inline constexpr const char* kDecimals         = "decimals";

// ── Float ──
inline constexpr const char* kLineSpacingRatio  = "line-spacing-ratio";
inline constexpr const char* kSizeRatio         = "size-ratio";
inline constexpr const char* kStep              = "step";
inline constexpr const char* kValue             = "value";
inline constexpr const char* kTrackThickness    = "track-thickness";
inline constexpr const char* kThumbSize         = "thumb-size";
inline constexpr const char* kTickInterval      = "tick-interval";
inline constexpr const char* kTickLength        = "tick-length";
inline constexpr const char* kLabelGap          = "label-gap";
inline constexpr const char* kRangeMin          = "range-min";
inline constexpr const char* kRangeMax          = "range-max";
inline constexpr const char* kAnimationSpeed    = "animation-speed";
inline constexpr const char* kScrollbarThickness = "scrollbar-thickness";
inline constexpr const char* kPageSize          = "page-size";
inline constexpr const char* kStepSize          = "step-size";
inline constexpr const char* kThickness         = "thickness";
inline constexpr const char* kEdgeMargin        = "edge-margin";
inline constexpr const char* kBarHeight         = "bar-height";
inline constexpr const char* kItemHeightRatio   = "item-height-ratio";
inline constexpr const char* kArrowWidth        = "arrow-width";
inline constexpr const char* kItemHeight        = "item-height";
inline constexpr const char* kButtonHeight      = "button-height";
inline constexpr const char* kButtonGap         = "button-gap";
inline constexpr const char* kPadding           = "padding";
inline constexpr const char* kIndentWidth       = "indent-width";
inline constexpr const char* kRowHeight         = "row-height";
inline constexpr const char* kLineSpacing       = "line-spacing";
inline constexpr const char* kArrowGap          = "arrow-gap";
inline constexpr const char* kRatio             = "ratio";
inline constexpr const char* kClosedSwatchSize  = "closed-swatch-size";
inline constexpr const char* kPageStep          = "page-step";
inline constexpr const char* kButtonWidth       = "button-width";
inline constexpr const char* kCaptionSize       = "caption-size";

// ── String ──
inline constexpr const char* kCaption           = "caption";
inline constexpr const char* kTextContent       = "text";
inline constexpr const char* kPlaceholder       = "placeholder";
inline constexpr const char* kSelectedValue     = "selected-value";
inline constexpr const char* kCustomText        = "custom-text";
inline constexpr const char* kLabelFormat       = "label-format";
inline constexpr const char* kShortcut          = "shortcut";
inline constexpr const char* kTitle             = "title";
inline constexpr const char* kConfirmText       = "confirm-text";
inline constexpr const char* kCancelText        = "cancel-text";
inline constexpr const char* kColor             = "color";

// ── Enum（同名值用于不同控件时加前缀区分）──
inline constexpr const char* kAlign             = "align";
inline constexpr const char* kFont              = "font";
inline constexpr const char* kCheckBoxStyle     = "style";
inline constexpr const char* kProgressBarStyle  = "style";
inline constexpr const char* kSliderStyle       = "style";
inline constexpr const char* kLayout            = "layout";
inline constexpr const char* kVerticalAlign     = "vertical-align";
inline constexpr const char* kTextMode          = "text-mode";
inline constexpr const char* kOrientation       = "orientation";
inline constexpr const char* kCheckState        = "check-state";
inline constexpr const char* kLabelFont         = "label-font";

// ── Events ──
inline constexpr const char* kEventClick            = "click";
inline constexpr const char* kEventEnter            = "enter";
inline constexpr const char* kEventTextChanged      = "text-changed";
inline constexpr const char* kEventClose            = "close";
inline constexpr const char* kEventConfirm          = "confirm";
inline constexpr const char* kEventCancel           = "cancel";
inline constexpr const char* kEventCheckChanged     = "check-changed";
inline constexpr const char* kEventValueChanged     = "value-changed";
inline constexpr const char* kEventPositionChanged  = "position-changed";
inline constexpr const char* kEventSelectionChanged = "selection-changed";
inline constexpr const char* kEventMoved            = "moved";
inline constexpr const char* kEventColorChanged     = "color-changed";
inline constexpr const char* kEventSelect           = "select";
inline constexpr const char* kEventExpand           = "expand";
inline constexpr const char* kEventCollapse         = "collapse";

} // namespace PropertyNames
