/* UICornerstone 用户手册 —— 全局侧栏导航注入脚本 */
(function () {
  var PREFIX = "";
  var SCRIPT = document.getElementById("nav-data");
  if (SCRIPT && SCRIPT.getAttribute("data-prefix")) PREFIX = SCRIPT.getAttribute("data-prefix");
  var CUR = (SCRIPT && SCRIPT.getAttribute("data-current")) || "";
  var CONTROLS = [
    ["controls/actor.html", "Actor（图片控件）"],
    ["controls/bench.html", "Bench（测试台/根）"],
    ["controls/button.html", "Button"],
    ["controls/checkbox.html", "CheckBox"],
    ["controls/colorpicker.html", "ColorPicker"],
    ["controls/combobox.html", "ComboBox"],
    ["controls/dialog.html", "Dialog / Popup"],
    ["controls/editbox.html", "EditBox"],
    ["controls/handlecontrol.html", "HandleControl"],
    ["controls/label.html", "Label"],
    ["controls/luotiani.html", "LuotiAni（动画）"],
    ["controls/menu.html", "Menu"],
    ["controls/numericupdown.html", "NumericUpDown"],
    ["controls/panel.html", "Panel"],
    ["controls/progressbar.html", "ProgressBar"],
    ["controls/scrollbar.html", "ScrollBar"],
    ["controls/slider.html", "Slider"],
    ["controls/splitter.html", "Splitter"],
    ["controls/textarea.html", "TextArea"],
    ["controls/winframe.html", "WinFrame"]
  ];
  var NAV = [
    ["", "首页 · 介绍", [
      ["", "1 介绍"]
    ]],
    ["quickstart.html", "快速起步", [
      ["quickstart.html", "2 快速起步"]
    ]],
    ["basics.html", "从基础入门", [
      ["basics.html", "3.1 使用 C++ Binding 搭建"],
      ["basics.html", "3.2 第一个 UICornerstone 应用"],
      ["property-system.html", "3.3 属性系统"],
      ["declarative-ui.html", "3.4 声明式 UI"],
      ["events-focus.html", "3.5 事件 · 3.6 焦点系统"],
      ["luotiani.html", "3.7 洛蒂动画"]
    ]],
    ["controls/index.html", "深入控件", [
      ["controls/index.html", "4.0 控件概念与共性"]
    ]],
    ["advanced.html", "进阶主题", [
      ["advanced.html", "5 进阶主题"],
      ["advanced/luotiani.html", "5.1 洛蒂动画引擎"],
      ["advanced/multiwindow.html", "5.2 多视口与多窗口"],
      ["advanced/tools.html", "5.3 非可视化控件"],
      ["advanced/cabi.html", "5.4 C ABI 程序化构建"]
    ]],
    ["backends.html", "其它后端", [
      ["backends.html", "6 使用其它后端（SFML/raylib）"]
    ]],
    ["integration.html", "嵌入逻辑回路", [
      ["integration.html", "7 嵌入到你的逻辑回路中"]
    ]],
    ["appendix/declarative-syntax.html", "附录", [
      ["appendix/declarative-syntax.html", "9.1 声明式 UI 语法速查"],
      ["appendix/properties.html", "9.2 属性速查表"],
      ["appendix/capi.html", "9.3 C ABI 速查表"],
      ["appendix/debugging.html", "9.4 问题定位手段"],
      ["appendix/backend-config.html", "9.5 后端差异及配置"]
    ]],
    ["faq.html", "FAQ", [
      ["faq.html", "10 常见问题（FAQ）"]
    ]]
  ];
  function esc(s) { return s.replace(/</g, "&lt;"); }
  var html = '<div class="brand"><img src="' + PREFIX + 'assets/UICornerstone_Logo_256.png" alt="logo"><span>UICornerstone</span></div>';
  for (var i = 0; i < NAV.length; i++) {
    var sec = NAV[i];
    html += '<div class="sec">' + esc(sec[1]) + "</div>";
    for (var j = 0; j < sec[2].length; j++) {
      var item = sec[2][j];
      var href = item[0] === "" ? PREFIX + "index.html" : PREFIX + item[0];
      var cls = "item" + (item[0] === CUR ? " active" : "");
      html += '<a class="' + cls + '" href="' + href + '">' + esc(item[2]) + "</a>";
    }
  }
  html += '<div class="sec">控件速查（4.1 - 4.20）</div>';
  for (var k = 0; k < CONTROLS.length; k++) {
    var c = CONTROLS[k];
    var cls2 = "item sub" + (c[0] === CUR ? " active" : "");
    html += '<a class="' + cls2 + '" href="' + PREFIX + c[0] + '">' + esc(c[1]) + "</a>";
  }
  document.getElementById("sidebar").innerHTML = html;
})();
