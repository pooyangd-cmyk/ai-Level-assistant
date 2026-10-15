#include "BuildTasks.hpp"
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {

struct Ctx {
    LevelEditorLayer* lel;
    CCPoint origin;
    int budget;
    int created = 0;
};

float num(matjson::Value const& p, char const* k, float d) {
    if (!p.contains(k)) return d;
    return static_cast<float>(p[k].asDouble().unwrapOr(d));
}

std::string str(matjson::Value const& p, char const* k, std::string d = "") {
    if (!p.contains(k)) return d;
    return p[k].asString().unwrapOr(d);
}

GameObject* put(Ctx& c, int id, CCPoint pos) {
    if (c.created >= c.budget) return nullptr;
    auto obj = c.lel->createObject(id, pos, false);
    if (obj) c.created++;
    return obj;
}

TaskResult done(Ctx const& c, std::string msg) {
    return TaskResult{true, std::move(msg), c.created};
}

TaskResult fail(std::string msg) {
    return TaskResult{false, std::move(msg), 0};
}

TaskResult placeObject(Ctx& c, matjson::Value const& p) {
    int id = static_cast<int>(num(p, "id", 1));
    auto obj = put(c, id, c.origin + CCPoint{num(p, "x", 0), num(p, "y", 0)});
    if (!obj) return fail("Object limit reached");
    obj->setRotation(num(p, "rotation", 0));
    return done(c, "1 object");
}

TaskResult row(Ctx& c, matjson::Value const& p) {
    int id = static_cast<int>(num(p, "id", 1));
    int count = std::clamp(static_cast<int>(num(p, "count", 5)), 1, 1000);
    float sp = num(p, "spacing", 30);
    bool vertical = str(p, "direction", "right") == "up";
    CCPoint start = c.origin + CCPoint{num(p, "x", 0), num(p, "y", 0)};
    for (int i = 0; i < count; i++) {
        auto pos = start + (vertical ? CCPoint{0, sp * i} : CCPoint{sp * i, 0});
        if (!put(c, id, pos)) break;
    }
    return done(c, fmt::format("{} objects", c.created));
}

TaskResult grid(Ctx& c, matjson::Value const& p) {
    int id = static_cast<int>(num(p, "id", 1));
    int cols = std::clamp(static_cast<int>(num(p, "cols", 5)), 1, 200);
    int rows = std::clamp(static_cast<int>(num(p, "rows", 5)), 1, 200);
    float sx = num(p, "spacing_x", 30), sy = num(p, "spacing_y", 30);
    CCPoint start = c.origin + CCPoint{num(p, "x", 0), num(p, "y", 0)};
    for (int r = 0; r < rows; r++)
        for (int q = 0; q < cols; q++)
            if (!put(c, id, start + CCPoint{sx * q, sy * r})) return done(c, "stopped at object limit");
    return done(c, fmt::format("{} objects", c.created));
}

TaskResult circle(Ctx& c, matjson::Value const& p) {
    int id = static_cast<int>(num(p, "id", 1));
    int count = std::clamp(static_cast<int>(num(p, "count", 16)), 3, 720);
    float radius = num(p, "radius", 90);
    bool rotate = num(p, "rotate_to_tangent", 1) != 0;
    CCPoint center = c.origin + CCPoint{num(p, "x", 0), num(p, "y", 0)};
    for (int i = 0; i < count; i++) {
        float a = 2.f * static_cast<float>(M_PI) * i / count;
        auto obj = put(c, id, center + CCPoint{std::cos(a), std::sin(a)} * radius);
        if (!obj) break;
        if (rotate) obj->setRotation(-a * 180.f / static_cast<float>(M_PI) + 90.f);
    }
    return done(c, fmt::format("{} objects", c.created));
}

TaskResult staircase(Ctx& c, matjson::Value const& p) {
    int id = static_cast<int>(num(p, "id", 1));
    int steps = std::clamp(static_cast<int>(num(p, "steps", 8)), 1, 200);
    float w = num(p, "step_width", 30), h = num(p, "step_height", 30);
    CCPoint start = c.origin + CCPoint{num(p, "x", 0), num(p, "y", 0)};
    for (int i = 0; i < steps; i++) {
        // each step is a column of (i+1) blocks
        for (int j = 0; j <= i; j++)
            if (!put(c, id, start + CCPoint{w * i, h * j})) return done(c, "stopped at object limit");
    }
    return done(c, fmt::format("{} objects", c.created));
}

TaskResult imageToObjects(Ctx& c, matjson::Value const& p, Bitmap const* image) {
    if (!image || image->empty()) return fail("No image selected - press the Image button first");

    auto mod = Mod::get();
    int id = static_cast<int>(mod->getSettingValue<int64_t>("pixel-object-id"));
    int firstCh = static_cast<int>(mod->getSettingValue<int64_t>("first-color-channel"));

    int cells = std::clamp(static_cast<int>(num(p, "cells", 32)), 8, image->w);
    float cell = std::clamp(num(p, "cell_size", 6.f), 2.f, 30.f);
    int ncol = std::clamp(static_cast<int>(num(p, "colors", 12)), 2, 24);
    int h = std::max(1, static_cast<int>(std::round(cells * static_cast<float>(image->h) / image->w)));

    Bitmap small = ai::img::resize(*image, cells, h);
    Quantized q = ai::img::quantize(small, ncol);
    if (q.palette.empty()) return fail("Image is fully transparent");

    float s = cell / 30.f;
    float x0 = c.origin.x + num(p, "x", 0) - cells * cell / 2.f;
    float y0 = c.origin.y + num(p, "y", 0) + h * cell / 2.f;

    // One color trigger per palette entry, in a column just left of the art.
    for (size_t i = 0; i < q.palette.size(); i++) {
        auto t = static_cast<EffectGameObject*>(put(c, 899, {x0 - 45.f, y0 - 30.f * i}));
        if (!t) break;
        t->m_targetColorID = firstCh + static_cast<int>(i);
        t->m_triggerTargetColor = q.palette[i];
        t->m_duration = 0.f;
        t->m_opacity = 1.f;
        t->m_usesBlending = false;
    }

    // Horizontal run-length merge: one stretched block per run of the same color.
    for (int r = 0; r < h; r++) {
        int x = 0;
        while (x < cells) {
            int pi = q.idx[static_cast<size_t>(r) * cells + x];
            if (pi < 0) { x++; continue; }
            int run = 1;
            while (x + run < cells && q.idx[static_cast<size_t>(r) * cells + x + run] == pi) run++;
            CCPoint pos{x0 + (x + run / 2.f) * cell, y0 - (r + 0.5f) * cell};
            auto obj = put(c, id, pos);
            if (!obj) return done(c, "stopped at object limit (raise it in settings)");
            obj->updateCustomScaleX(s * run);
            obj->updateCustomScaleY(s);
            if (obj->m_baseColor) obj->m_baseColor->m_colorID = firstCh + pi;
            x += run;
        }
    }
    return done(c, fmt::format("{} objects, {} colors", c.created, q.palette.size()));
}

} // namespace

namespace ai::tasks {

TaskResult run(LevelEditorLayer* lel, matjson::Value const& task, Bitmap const* circleImage) {
    if (!lel) return fail("Editor not available");

    Ctx c{lel, {}, static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-objects"))};
    auto win = CCDirector::get()->getWinSize();
    c.origin = lel->m_objectLayer->convertToNodeSpace(win / 2);

    auto type = task.contains("type") ? task["type"].asString().unwrapOr("") : std::string();
    matjson::Value params = task.contains("params") ? task["params"] : matjson::Value::object();

    if (type == "place_object") return placeObject(c, params);
    if (type == "row") return row(c, params);
    if (type == "grid") return grid(c, params);
    if (type == "circle") return circle(c, params);
    if (type == "staircase") return staircase(c, params);
    if (type == "image_to_objects") return imageToObjects(c, params, circleImage);
    return fail(fmt::format("Unknown task type '{}'", type));
}

} // namespace ai::tasks
