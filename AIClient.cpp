#include "AIClient.hpp"

using namespace geode::prelude;

namespace {
constexpr char const* kSystem = R"(You are the AI assistant inside the Geometry Dash level editor (a Geode mod).
You build things by emitting tasks. Respond with ONLY one JSON object, no markdown fences:
{"reply": "<short friendly message>", "tasks": [ {"name": "<short task name>", "type": "<type>", "params": { ... }} ]}

Units: 30 units = 1 block. x/y in params are offsets from the center of the player's current view (+x right, +y up).
Task types and params (all optional, defaults in brackets):
- place_object: id[1], x, y, rotation
- row: id[1], count[5], spacing[30], direction["right"|"up"], x, y
- grid: id[1], cols[5], rows[5], spacing_x[30], spacing_y[30], x, y
- circle: id[1], radius[90], count[16], rotate_to_tangent[1], x, y
- staircase: id[1], steps[8], step_width[30], step_height[30], x, y
- image_to_objects: builds colored pixel art from the image the user selected (already cropped to a circle).
  cells[32] = pixels across (8-64), cell_size[6] = units per pixel (2-30), colors[12] = palette size (2-24), x, y.
Common object ids: 1 basic block, 8 spike, 35 yellow pad, 36 yellow orb, 67 blue pad, 40 slab.
If the user is only chatting or the request is unclear, return an empty tasks array and ask one short question.
If an image is attached, briefly describe what you see and choose cells/colors that preserve detail without exceeding ~2000 objects.
Never invent task types. Keep task names under 40 characters.)";
}

void AIClient::send(std::string userText, std::string pngBase64, Callback cb) {
    auto mod = Mod::get();
    auto key = mod->getSettingValue<std::string>("api-key");
    auto model = mod->getSettingValue<std::string>("model");
    auto endpoint = mod->getSettingValue<std::string>("endpoint");
    if (key.empty()) return cb(Err("No API key set. Open this mod's settings and paste your key."));

    m_busy = true;

    // Build the new user message (image only for this turn; history stores text only).
    matjson::Value userContent;
    if (!pngBase64.empty()) {
        std::vector<matjson::Value> blocks;
        blocks.push_back(matjson::makeObject({
            {"type", "image"},
            {"source", matjson::makeObject({
                {"type", "base64"}, {"media_type", "image/png"}, {"data", pngBase64}
            })}
        }));
        blocks.push_back(matjson::makeObject({{"type", "text"}, {"text", userText}}));
        userContent = matjson::Value(blocks);
    } else {
        userContent = matjson::Value(userText);
    }

    std::vector<matjson::Value> messages = m_history;
    messages.push_back(matjson::makeObject({{"role", "user"}, {"content", userContent}}));

    auto body = matjson::makeObject({
        {"model", model},
        {"max_tokens", 2000},
        {"system", std::string(kSystem)},
        {"messages", matjson::Value(messages)}
    });

    web::WebRequest req;
    req.header("x-api-key", key);
    req.header("anthropic-version", "2023-06-01");
    req.header("content-type", "application/json");
    req.timeout(std::chrono::seconds(90));
    req.bodyJSON(body);

    std::string historyText = pngBase64.empty() ? userText : userText + " [image attached]";

    m_holder.spawn(req.post(endpoint), [this, historyText, cb = std::move(cb)](web::WebResponse res) {
        m_busy = false;
        if (!res.ok()) {
            auto detail = res.string().unwrapOr("");
            if (detail.size() > 160) detail.resize(160);
            return cb(Err("HTTP {}: {}", res.code(), detail));
        }
        auto json = res.json();
        if (json.isErr()) return cb(Err("Bad response from API"));

        std::string text;
        auto const& root = json.unwrap();
        if (root.contains("content") && root["content"].isArray()) {
            for (auto const& block : root["content"]) {
                if (block.contains("text")) text += block["text"].asString().unwrapOr("");
            }
        }
        if (text.empty()) return cb(Err("Empty reply from API"));

        m_history.push_back(matjson::makeObject({{"role", "user"}, {"content", historyText}}));
        m_history.push_back(matjson::makeObject({{"role", "assistant"}, {"content", text}}));
        while (m_history.size() > 20) m_history.erase(m_history.begin());
        cb(Ok(text));
    });
}
