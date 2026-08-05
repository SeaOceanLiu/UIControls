#include <iostream>
#include <memory>
#include <fstream>
#include <cmath>
#include <string>
#include "LuotiAni.h"
#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "TestUtils.h"
#include "PlatformUtils.h"
#include "TestInstance.h"

using namespace std;

static ofstream g_logFile;

void logOutput(const string& message) {
    if (!g_logFile.is_open()) {
        g_logFile.open("luotiani_log.txt", ios::out);
    }
    g_logFile << message << endl;
    g_logFile.flush();
    cout << message << endl;
}

static int g_caseIndex = 0;
static int g_failCount = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { g_failCount++; logOutput(string("FAIL [") + to_string(g_caseIndex) + "] " + msg); } \
    } while (0)

static float g_tolerance = 0.01f;

static bool nearEqual(float a, float b) {
    return fabsf(a - b) <= g_tolerance;
}

static void sleepMs(uint32_t ms) {
    uint64_t start = Platform::GetTicks();
    while (Platform::GetTicks() - start < ms) {
    }
}

// ── 期望值模型（与设计文档 §5.2/§5.3/§5.4 公式一致） ──

// easeType: 0=linear 1=ease-in 2=ease-out 3=ease-in-out 4=quad 5=sine 6=cubic-bezier
static float modelEase(int easeType, float b1, float b2, float b3, float b4, float t) {
    switch (easeType) {
        case 1: return t * t;
        case 2: { float u = 1.0f - t; return 1.0f - u * u; }
        case 3: return 3.0f * t * t - 2.0f * t * t * t;
        case 4: return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        case 5: return 1.0f - cosf(t * (float)M_PI / 2.0f);
        case 6:
        {
            float lo = 0.0f, hi = 1.0f;
            for (int i = 0; i < 24; i++) {
                float mid = (lo + hi) / 2.0f;
                float u = 1.0f - mid;
                float bx = 3.0f * u * u * mid * b1 + 3.0f * u * mid * mid * b3 + mid * mid * mid;
                if (bx < t) lo = mid; else hi = mid;
            }
            float tm = (lo + hi) / 2.0f;
            float u = 1.0f - tm;
            return 3.0f * u * u * tm * b2 + 3.0f * u * tm * tm * b4 + tm * tm * tm;
        }
        default: return t;
    }
}

static SPoint bezier2(const SPoint& p0, const SPoint& c1, const SPoint& p1, float t) {
    float u = 1.0f - t;
    return SPoint(u * u * p0.x + 2.0f * u * t * c1.x + t * t * p1.x,
                  u * u * p0.y + 2.0f * u * t * c1.y + t * t * p1.y);
}

static SPoint bezier3(const SPoint& p0, const SPoint& c1, const SPoint& c2, const SPoint& p1, float t) {
    float u = 1.0f - t;
    return SPoint(u * u * u * p0.x + 3.0f * u * u * t * c1.x + 3.0f * u * t * t * c2.x + t * t * t * p1.x,
                  u * u * u * p0.y + 3.0f * u * u * t * c1.y + 3.0f * u * t * t * c2.y + t * t * t * p1.y);
}

// ── JSON 构造工具 ──

static json makeOverview(int totalFrames) {
    return {
        {"name", "test_luotiani"},
        {"version", "0.0.1"},
        {"view", {{"width", 100}, {"height", 100}}},
        {"frameRate", 10},
        {"totalFrames", totalFrames},
        {"loop", false}
    };
}

static json makeTranslateOp(float tx, float ty, const string& easing = "", const string& pathName = "") {
    json op = {{"type", "translate"}, {"tx", tx}, {"ty", ty}};
    if (!easing.empty()) {
        op["easing"] = easing;
    }
    if (pathName == "bezier2") {
        op["path"] = {{"type", "bezier"}, {"c1x", 50}, {"c1y", -80}};
    } else if (pathName == "bezier3") {
        op["path"] = {{"type", "bezier"}, {"c1x", 50}, {"c1y", -80}, {"c2x", 150}, {"c2y", 120}};
    } else if (pathName == "bezierC") {
        op["path"] = {{"type", "bezier"}, {"c1x", 100}, {"c1y", -60}};
    } else if (pathName == "parabola") {
        op["path"] = {{"type", "parabola"}, {"vx", 60}, {"vy", -30}};
    } else if (pathName == "catmull") {
        json points = json::array();
        points.push_back({{"x", 50}, {"y", -40}});
        points.push_back({{"x", 150}, {"y", 80}});
        points.push_back({{"x", 250}, {"y", -60}});
        op["path"] = {{"type", "catmull-rom"}, {"points", points}};
    } else if (pathName == "badType") {
        op["path"] = {{"type", "spline"}};
    } else if (pathName == "missingCtrl") {
        op["path"] = {{"type", "bezier"}};
    }
    return op;
}

static json makeLayerJson(const json& keyFrames) {
    return {
        {"name", "layer1"},
        {"type", "image"},
        {"src", "animations/rotateBtn/rotateBtn.svg"},
        {"opacity", 100},
        {"blendMode", "normal"},
        {"keyFrames", keyFrames}
    };
}

static json makeDoc(int totalFrames, const json& layer) {
    return {
        {"overview", makeOverview(totalFrames)},
        {"layers", json::array({layer})}
    };
}

static void writeCaseJson(const string& filename, const json& doc) {
    ofstream out(filename, ios::out);
    out << doc.dump();
    out.close();
}

static shared_ptr<LuotiAni> loadAndPrepare(const string& filename, const json& doc) {
    g_caseIndex++;
    writeCaseJson(filename, doc);
    shared_ptr<LuotiAni> ani = make_shared<LuotiAni>(BENCH);
    try {
        ani->loadAniDesc(fs::path(filename));
        ani->prepare();
    } catch (const char* e) {
        g_failCount++;
        logOutput(string("FAIL [") + to_string(g_caseIndex) + "] loadAndPrepare threw: " + e);
        return nullptr;
    }
    return ani;
}

// ── 用例：L1  easing 各值（translate 线性路径） ──

void testL1Easing(void) {
    struct EaseCase { const char* name; int type; float p1, p2, p3, p4; };
    EaseCase cases[] = {
        {"linear",                      0, 0,    0,    0,    0},
        {"ease-in",                     1, 0,    0,    0,    0},
        {"ease-out",                    2, 0,    0,    0,    0},
        {"ease-in-out",                 3, 0,    0,    0,    0},
        {"quad",                        4, 0,    0,    0,    0},
        {"sine",                        5, 0,    0,    0,    0},
        {"cubic-bezier(0.42,0,0.58,1)", 6, 0.42f, 0.0f, 0.58f, 1.0f},
    };
    for (const auto& c : cases) {
        json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
        json kfN = {{"frame", 10}, {"operation", json::array({makeTranslateOp(100, 60, c.name)})}};
        string file = string("tl_L1_") + c.name + ".jsonc";
        shared_ptr<LuotiAni> ani = loadAndPrepare(file, makeDoc(10, makeLayerJson(json::array({kf0, kfN}))));
        if (ani == nullptr) continue;
        for (int f = 1; f < 10; f++) {
            float t = (float)f / 10.0f;
            float t1 = modelEase(c.type, c.p1, c.p2, c.p3, c.p4, t);
            LuotiAni::OpData data = ani->getFrameOpData(0, f);
            CHECK(nearEqual(data.translate.x, 100.0f * t1), string("L1 ") + c.name + " f" + to_string(f) + " x");
            CHECK(nearEqual(data.translate.y, 60.0f * t1), string("L1 ") + c.name + " f" + to_string(f) + " y");
        }
        LuotiAni::OpData end = ani->getFrameOpData(0, 10);
        CHECK(nearEqual(end.translate.x, 100.0f) && nearEqual(end.translate.y, 60.0f), string("L1 ") + c.name + " end exact");
    }
}

// ── 用例：L2  bezier 二次/三次（无 easing） ──

void testL2Bezier(void) {
    {
        json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
        json kfN = {{"frame", 10}, {"operation", json::array({makeTranslateOp(100, 60, "", "bezier2")})}};
        shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L2_quad.jsonc", makeDoc(10, makeLayerJson(json::array({kf0, kfN}))));
        if (ani == nullptr) return;
        for (int f = 1; f < 10; f++) {
            float t = (float)f / 10.0f;
            SPoint e = bezier2(SPoint(0, 0), SPoint(50, -80), SPoint(100, 60), t);
            LuotiAni::OpData data = ani->getFrameOpData(0, f);
            CHECK(nearEqual(data.translate.x, e.x) && nearEqual(data.translate.y, e.y),
                  string("L2 quad f") + to_string(f));
        }
        LuotiAni::OpData end = ani->getFrameOpData(0, 10);
        CHECK(nearEqual(end.translate.x, 100.0f) && nearEqual(end.translate.y, 60.0f), "L2 quad end exact");
    }
    {
        json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
        json kfN = {{"frame", 10}, {"operation", json::array({makeTranslateOp(100, 60, "", "bezier3")})}};
        shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L2_cubic.jsonc", makeDoc(10, makeLayerJson(json::array({kf0, kfN}))));
        if (ani == nullptr) return;
        for (int f = 1; f < 10; f++) {
            float t = (float)f / 10.0f;
            SPoint e = bezier3(SPoint(0, 0), SPoint(50, -80), SPoint(150, 120), SPoint(100, 60), t);
            LuotiAni::OpData data = ani->getFrameOpData(0, f);
            CHECK(nearEqual(data.translate.x, e.x) && nearEqual(data.translate.y, e.y),
                  string("L2 cubic f") + to_string(f));
        }
        LuotiAni::OpData end = ani->getFrameOpData(0, 10);
        CHECK(nearEqual(end.translate.x, 100.0f) && nearEqual(end.translate.y, 60.0f), "L2 cubic end exact");
    }
}

// ── 用例：L3  parabola（无 easing） ──

void testL3Parabola(void) {
    json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
    json kfN = {{"frame", 20}, {"operation", json::array({makeTranslateOp(200, 0, "", "parabola")})}};
    shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L3.jsonc", makeDoc(20, makeLayerJson(json::array({kf0, kfN}))));
    if (ani == nullptr) return;
    float vx = 60.0f, vy = -30.0f;
    float gx = 2.0f * (200.0f - vx);
    float gy = 2.0f * (0.0f - vy);
    for (int f = 1; f < 20; f++) {
        float t = (float)f / 20.0f;
        SPoint e(vx * t + 0.5f * gx * t * t, vy * t + 0.5f * gy * t * t);
        LuotiAni::OpData data = ani->getFrameOpData(0, f);
        CHECK(nearEqual(data.translate.x, e.x) && nearEqual(data.translate.y, e.y),
              string("L3 parabola f") + to_string(f));
    }
    LuotiAni::OpData end = ani->getFrameOpData(0, 20);
    CHECK(nearEqual(end.translate.x, 200.0f) && fabsf(end.translate.y) <= g_tolerance, "L3 parabola end exact");
}

// ── 用例：L4  catmull-rom（无 easing） ──

void testL4CatmullRom(void) {
    json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
    json kfN = {{"frame", 20}, {"operation", json::array({makeTranslateOp(300, 0, "", "catmull")})}};
    shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L4.jsonc", makeDoc(20, makeLayerJson(json::array({kf0, kfN}))));
    if (ani == nullptr) return;
    SPoint pts[] = {SPoint(50, -40), SPoint(150, 80), SPoint(250, -60)};
    float at[3] = {0.25f, 0.50f, 0.75f};
    for (int i = 0; i < 3; i++) {
        int f = (int)(at[i] * 20.0f);
        LuotiAni::OpData data = ani->getFrameOpData(0, f);
        CHECK(nearEqual(data.translate.x, pts[i].x) && nearEqual(data.translate.y, pts[i].y),
              string("L4 catmull through point ") + to_string(i));
    }
    LuotiAni::OpData end = ani->getFrameOpData(0, 20);
    CHECK(nearEqual(end.translate.x, 300.0f) && nearEqual(end.translate.y, 0.0f), "L4 catmull end exact");
}

// ── 用例：L5  向后兼容（旧 JSON 无 easing/path） ──

void testL5Backward(void) {
    json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
    json kfN = {{"frame", 10}, {"operation", json::array({makeTranslateOp(100, 60)})}};
    shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L5.jsonc", makeDoc(10, makeLayerJson(json::array({kf0, kfN}))));
    if (ani == nullptr) return;
    for (int f = 1; f < 10; f++) {
        float t = (float)f / 10.0f;
        LuotiAni::OpData data = ani->getFrameOpData(0, f);
        CHECK(nearEqual(data.translate.x, 100.0f * t) && nearEqual(data.translate.y, 60.0f * t),
              string("L5 linear f") + to_string(f));
        CHECK(nearEqual(data.m.scaleX, 1.0f) && nearEqual(data.m.scaleY, 1.0f), "L5 scale default");
        CHECK(nearEqual(data.rotate, 0.0f), "L5 rotate default");
        CHECK(data.visible, "L5 visible default");
    }
    CHECK(nearEqual(ani->getFrameOpData(0, 0).translate.x, 0.0f), "L5 frame0 origin");
    CHECK(nearEqual(ani->getFrameOpData(0, 10).translate.x, 100.0f), "L5 frameN end");
}

// ── 用例：L6  easing+path 组合 ──

void testL6Combined(void) {
    json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
    json kfN = {{"frame", 10}, {"operation", json::array({makeTranslateOp(100, 60, "ease-in-out", "bezier2")})}};
    shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L6.jsonc", makeDoc(10, makeLayerJson(json::array({kf0, kfN}))));
    if (ani == nullptr) return;
    for (int f = 1; f < 10; f++) {
        float t = (float)f / 10.0f;
        float t1 = modelEase(3, 0, 0, 0, 0, t);
        SPoint e = bezier2(SPoint(0, 0), SPoint(50, -80), SPoint(100, 60), t1);
        LuotiAni::OpData data = ani->getFrameOpData(0, f);
        CHECK(nearEqual(data.translate.x, e.x) && nearEqual(data.translate.y, e.y),
              string("L6 easing+path f") + to_string(f));
    }
}

// ── 用例：L7  easing 用于 scale/rotate/opacity ──

void testL7OtherProps(void) {
    json kf0Ops = json::array({
        {{"type", "translate"}, {"tx", 0}, {"ty", 0}},
        {{"type", "scale"}, {"sx", 1}, {"sy", 1}},
        {{"type", "rotate"}, {"angle", 0}, {"cx", 0}, {"cy", 0}},
        {{"type", "opacity"}, {"opacity", 100}},
    });
    json kfNOps = json::array({
        {{"type", "translate"}, {"tx", 0}, {"ty", 0}},
        {{"type", "scale"}, {"sx", 2}, {"sy", 0.5}, {"easing", "ease-in"}},
        {{"type", "rotate"}, {"angle", 90}, {"cx", 30}, {"cy", 20}},
        {{"type", "opacity"}, {"opacity", 50}},
    });
    json kf0 = {{"frame", 0}, {"operation", kf0Ops}};
    json kfN = {{"frame", 10}, {"operation", kfNOps}};
    shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L7.jsonc", makeDoc(10, makeLayerJson(json::array({kf0, kfN}))));
    if (ani == nullptr) return;
    for (int f = 1; f < 10; f++) {
        float t = (float)f / 10.0f;
        float t1 = t * t;
        LuotiAni::OpData data = ani->getFrameOpData(0, f);
        CHECK(nearEqual(data.m.scaleX, 1.0f + t1), string("L7 scaleX f") + to_string(f));
        CHECK(nearEqual(data.m.scaleY, 1.0f - 0.5f * t1), string("L7 scaleY f") + to_string(f));
        CHECK(nearEqual(data.rotate, 90.0f * t1), string("L7 rotate f") + to_string(f));
        CHECK(nearEqual(data.centerPos.x, 30.0f * t1), string("L7 centerX f") + to_string(f));
        CHECK(nearEqual(data.centerPos.y, 20.0f * t1), string("L7 centerY f") + to_string(f));
        CHECK(data.opacity == (uint8_t)(255.0f - 128.0f * t1), string("L7 opacity f") + to_string(f));
    }
}

// ── 用例：L8  多关键帧多段混合 ──

void testL8MultiSegment(void) {
    json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
    json kf5 = {{"frame", 5}, {"operation", json::array({makeTranslateOp(100, 50, "ease-in")})}};
    json kf10 = {{"frame", 10}, {"operation", json::array({makeTranslateOp(200, -50, "", "bezierC")})}};
    shared_ptr<LuotiAni> ani = loadAndPrepare("tl_L8.jsonc", makeDoc(10, makeLayerJson(json::array({kf0, kf5, kf10}))));
    if (ani == nullptr) return;
    // 段 1：0→5，ease-in 线性
    for (int f = 1; f < 5; f++) {
        float t = (float)f / 5.0f;
        float t1 = t * t;
        LuotiAni::OpData data = ani->getFrameOpData(0, f);
        CHECK(nearEqual(data.translate.x, 100.0f * t1) && nearEqual(data.translate.y, 50.0f * t1),
              string("L8 seg1 f") + to_string(f));
    }
    CHECK(nearEqual(ani->getFrameOpData(0, 5).translate.x, 100.0f) && nearEqual(ani->getFrameOpData(0, 5).translate.y, 50.0f),
          "L8 kf5 exact");
    // 段 2：5→10，bezier（控制点相对段起点 (100,50)）
    SPoint start(100, 50);
    SPoint c1(100 + 100, 50 - 60);
    for (int f = 6; f < 10; f++) {
        float t = (float)(f - 5) / 5.0f;
        SPoint e = bezier2(start, c1, SPoint(300, 0), t);
        LuotiAni::OpData data = ani->getFrameOpData(0, f);
        CHECK(nearEqual(data.translate.x, e.x) && nearEqual(data.translate.y, e.y),
              string("L8 seg2 f") + to_string(f));
    }
    LuotiAni::OpData end = ani->getFrameOpData(0, 10);
    CHECK(nearEqual(end.translate.x, 300.0f) && nearEqual(end.translate.y, 0.0f), "L8 kf10 exact");
}

// ── 用例：L9  非法字段容错 ──

void testL9Tolerance(void) {
    struct BadCase { const char* file; const char* easing; const char* path; };
    BadCase cases[] = {
        {"tl_L9_badEase.jsonc",   "bounce",                ""},
        {"tl_L9_badBezier.jsonc", "cubic-bezier(2,0,1,1)", ""},
        {"tl_L9_badPath.jsonc",   "",                      "badType"},
        {"tl_L9_missCtrl.jsonc",  "",                      "missingCtrl"},
    };
    for (const auto& c : cases) {
        json kf0 = {{"frame", 0}, {"operation", json::array({makeTranslateOp(0, 0)})}};
        json kfN = {{"frame", 10}, {"operation", json::array({makeTranslateOp(100, 60, c.easing, c.path)})}};
        shared_ptr<LuotiAni> ani = loadAndPrepare(c.file, makeDoc(10, makeLayerJson(json::array({kf0, kfN}))));
        if (ani == nullptr) continue;
        // 全部回退 linear（未知 easing / 非法 cubic-bezier / 未知 path）
        if (string(c.path) != "missingCtrl") {
            for (int f = 1; f < 10; f++) {
                float t = (float)f / 10.0f;
                LuotiAni::OpData data = ani->getFrameOpData(0, f);
                CHECK(nearEqual(data.translate.x, 100.0f * t), string("L9 ") + c.file + " f" + to_string(f) + " x");
                CHECK(nearEqual(data.translate.y, 60.0f * t), string("L9 ") + c.file + " f" + to_string(f) + " y");
            }
        } else {
            // bezier 缺控制点：补零后控制点=段起点，轨迹仍在直线段上
            for (int f = 1; f < 10; f++) {
                LuotiAni::OpData data = ani->getFrameOpData(0, f);
                CHECK(fabsf(data.translate.x - 100.0f * (data.translate.y / 60.0f)) <= g_tolerance,
                      string("L9 ") + c.file + " trajectory on straight line f" + to_string(f));
            }
        }
    }
}

// ── 用例：L10  全量现有资源回归 ──

void testL10Resources(void) {
    const char* ids[] = {
        "bombBlock", "cyanBlock", "darkGreenBlock", "deepBlueBlock", "grayBlock",
        "greenBlock", "pierceBlock", "purpleBlock", "redBlock", "rotateBtn", "yellowBlock"
    };
    for (const char* id : ids) {
        g_caseIndex++;
        string resourceId = string("animations/") + id + "/" + id + ".jsonc";
        string assetPath = string("assets/") + resourceId;
        ifstream in(assetPath, ios::binary);
        if (!in.is_open()) {
            g_failCount++;
            logOutput(string("FAIL [") + to_string(g_caseIndex) + "] L10 cannot open asset " + assetPath);
            continue;
        }
        string content((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
        in.close();
        json doc;
        try {
            doc = json::parse(content.c_str(), nullptr, false, true);
        } catch (...) {
            g_failCount++;
            logOutput(string("FAIL [") + to_string(g_caseIndex) + "] L10 parse asset " + resourceId);
            continue;
        }
        json overview = doc["overview"];
        int expectFrames = overview.value("totalFrames", 0);
        float expectW = overview.at("view").at("width").get<float>();
        float expectH = overview.at("view").at("height").get<float>();

        shared_ptr<LuotiAni> ani = make_shared<LuotiAni>(BENCH);
        try {
            ani->loadAniDesc(resourceId);
            ani->prepare();
        } catch (const char* e) {
            g_failCount++;
            logOutput(string("FAIL [") + to_string(g_caseIndex) + "] L10 " + resourceId + " load/prepare threw: " + e);
            continue;
        }
        CHECK(ani->getTotalFrames() == (uint32_t)expectFrames, string("L10 ") + resourceId + " totalFrames");
        CHECK(nearEqual(ani->getRect().width, expectW) && nearEqual(ani->getRect().height, expectH),
              string("L10 ") + resourceId + " canvas size");
        bool finiteOk = true;
        for (uint32_t l = 0; l < 16; l++) {
            LuotiAni::OpData d0 = ani->getFrameOpData(l, 0);
            if (d0.surface == nullptr) break;
            for (uint32_t f = 0; f < (uint32_t)expectFrames; f++) {
                LuotiAni::OpData d = ani->getFrameOpData(l, f);
                if (!(isfinite(d.translate.x) && isfinite(d.translate.y) &&
                      isfinite(d.m.scaleX) && isfinite(d.m.scaleY) &&
                      isfinite(d.rotate) && isfinite(d.centerPos.x) && isfinite(d.centerPos.y))) {
                    finiteOk = false;
                    break;
                }
            }
        }
        CHECK(finiteOk, string("L10 ") + resourceId + " opdata finite");
        // 冒烟播放不崩溃
        try {
            ani->play();
            for (int i = 0; i < 60; i++) {
                sleepMs(20);
                ani->update();
            }
            ani->draw(0.0f, 0.0f);
        } catch (const char* e) {
            g_failCount++;
            logOutput(string("FAIL [") + to_string(g_caseIndex) + "] L10 " + resourceId + " smoke threw: " + e);
        }
    }
}

void testLuotianiInitialize(shared_ptr<Bench>) {
    TestUtil::log("testLuotianiInitialize");

    testL1Easing();
    testL2Bezier();
    testL3Parabola();
    testL4CatmullRom();
    testL5Backward();
    testL6Combined();
    testL7OtherProps();
    testL8MultiSegment();
    testL9Tolerance();
    testL10Resources();

    if (g_failCount == 0) {
        logOutput(u8"test_luotiani: ALL PASS");
    } else {
        logOutput(u8"test_luotiani: FAILURES = " + to_string(g_failCount));
    }
}

class LuotianiApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle("test_luotiani");
        logOutput(u8"LuotianiApp::onInit");
        BENCH->setOnInitial(testLuotianiInitialize);
        return true;
    }

    void onUpdate() override {
        BENCH->eventLoopEntry();
        BENCH->update();
    }

    void onRender() override {
        GET_RENDERDEVICE->setDrawColor(SColor(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f));
        GET_RENDERDEVICE->clear();
        BENCH->draw();
    }

    void onQuit() override {
        logOutput(u8"程序结束");
    }
};

int main(int argc, char* argv[]) {
    return TestRunMain<LuotianiApp>(argc, argv);
}