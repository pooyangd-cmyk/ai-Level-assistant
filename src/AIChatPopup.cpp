#include "AIChatPopup.hpp"
#include "BuildTasks.hpp"
#include <Geode/utils/file.hpp>
#include <algorithm>
#include <cctype>

using namespace geode::prelude;

AIChatPopup* AIChatPopup::create(LevelEditorLayer* lel) {
    auto ret = new AIChatPopup();
    if (ret->init(lel)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AIChatPopup::init(LevelEditorLayer* lel) {
    if (!Popup::init(460.f, 290.f)) return false;
    m_lel = lel;
    this->setTitle("AI Level Assistant");

    auto size = m_mainLayer->getContentSize();
    float chatW = 310.f, chatH = 175.f;

    // chat background + scroll
    auto bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    bg->setColor({0, 0, 0});
    bg->setOpacity(110);
    bg->setContentSize({chatW + 8.f, chatH + 8.f});
    bg->setAnchorPoint({0, 0});
    bg->setPosition({16.f, 66.f});
    m_mainLayer->addChild(bg);

    m_scroll = ScrollLayer::create({chatW, chatH});
    m_scroll->setPosition({20.f, 70.f});
    m_mainLayer->addChild(m_scroll);

    // input row
    m_input = TextInput::create(chatW - 70.f, "Ask the AI to build something...", "chatFont.fnt");
    m_input->setMaxCharCount(400);
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setPosition({20.f + (chatW - 70.f) / 2.f, 42.f});
    m_mainLayer->addChild(m_input);

    auto sendBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", 0.7f),
        [this](CCObject*) { this->onSend(); });
    sendBtn->setPosition({20.f + chatW - 30.f, 42.f});
    m_buttonMenu->addChild(sendBtn);

    // right panel: image picker + circle preview
    float px = size.width - 68.f;
    m_previewHolder = CCNode::create();
    m_previewHolder->setPosition({px, 205.f});
    m_mainLayer->addChild(m_previewHolder);

    m_previewLabel = CCLabelBMFont::create("No image", "chatFont.fnt");
    m_previewLabel->setScale(0.7f);
    m_previewLabel->setPosition({px, 160.f});
    m_mainLayer->addChild(m_previewLabel);

    auto imgBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Image", "goldFont.fnt", "GJ_button_02.png", 0.7f),
        [this](CCObject*) { this->onPickImage(); });
    imgBtn->setPosition({px, 125.f});
    m_buttonMenu->addChild(imgBtn);

    auto clearBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Clear", "goldFont.fnt", "GJ_button_06.png", 0.7f),
        [this](CCObject*) { this->onClear(); });
    clearBtn->setPosition({px, 90.f});
    m_buttonMenu->addChild(clearBtn);

    this->say(Who::AI, "Hi! Tell me what to build, or press Image to pick a picture. Type /help for commands.");
    return true;
}

void AIChatPopup::say(Who who, std::string text) {
    m_lines.emplace_back(who, std::move(text));
    this->rebuildChat();
}

void AIChatPopup::rebuildChat() {
    std::string markup;
    for (auto const& [who, text] : m_lines) {
        std::string safe = text;
        std::replace(safe.begin(), safe.end(), '<', '(');
        std::replace(safe.begin(), safe.end(), '>', ')');
        switch (who) {
            case Who::User:   markup += "<cj>You:</c> " + safe; break;
            case Who::AI:     markup += "<cy>AI:</c> " + safe; break;
            case Who::System: markup += "<cg>" + safe + "</c>"; break;
            case Who::Error:  markup += "<cr>" + safe + "</c>"; break;
        }
        markup += "\n";
    }

    if (m_chatText) m_chatText->removeFromParent();
    auto viewH = m_scroll->getContentSize().height;
    m_chatText = TextArea::create(markup, "chatFont.fnt", 1.f, m_scroll->getContentSize().width - 30.f,
                                  {0.f, 1.f}, 14.f, false);
    float h = std::max(viewH, m_chatText->getContentSize().height + 8.f);
    m_scroll->m_contentLayer->setContentSize({m_scroll->getContentSize().width, h});
    m_chatText->setAnchorPoint({0.f, 1.f});
    m_chatText->setPosition({4.f, h - 2.f});
    m_scroll->m_contentLayer->addChild(m_chatText);
    m_scroll->m_contentLayer->setPositionY(0.f); // show newest messages (bottom)
}

void AIChatPopup::onClear() {
    m_lines.clear();
    m_client.reset();
    this->say(Who::System, "Chat cleared.");
}

void AIChatPopup::onPickImage() {
    file::FilePickOptions opts;
    opts.filters = {{"Images", {"*.png", "*.jpg", "*.jpeg"}}};
    m_pick.spawn(file::pick(file::PickMode::OpenFile, opts),
        [this](Result<std::optional<std::filesystem::path>> res) {
            if (res.isErr()) return this->say(Who::Error, "File picker failed: " + res.unwrapErr());
            auto opt = std::move(res).unwrap();
            if (!opt) return; // cancelled
            this->loadImage(*opt);
        });
}

void AIChatPopup::loadImage(std::filesystem::path const& path) {
    auto loaded = ai::img::load(path);
    if (loaded.isErr()) return this->say(Who::Error, "Image error: " + loaded.unwrapErr());

    int size = static_cast<int>(Mod::get()->getSettingValue<int64_t>("circle-size"));
    m_circle = ai::img::circleCrop(loaded.unwrap(), size);
    m_circlePath = Mod::get()->getSaveDir() / "circle-image.png";

    auto saved = ai::img::savePng(*m_circle, m_circlePath);
    if (saved.isErr()) return this->say(Who::Error, "Could not save circle image: " + saved.unwrapErr());

    // show preview (bypass the texture cache so re-picking updates it)
    CCTextureCache::get()->removeTextureForKey(m_circlePath.string().c_str());
    m_previewHolder->removeAllChildren();
    if (auto spr = CCSprite::create(m_circlePath.string().c_str())) {
        float target = 64.f;
        spr->setScale(target / std::max(spr->getContentWidth(), 1.f));
        m_previewHolder->addChild(spr);
    }
    m_previewLabel->setString(fmt::format("{}x{} circle", size, size).c_str());

    m_imagePending = true;
    this->say(Who::System, fmt::format("Image ready: {} ({}px circle). Ask the AI what to do with it, or type /image.",
                                       path.filename().string(), size));
}

void AIChatPopup::onSend() {
    auto text = m_input->getString();
    if (text.empty()) return;
    m_input->setString("");

    // never echo API keys into the chat log
    if (text.rfind("/key ", 0) == 0 || text.rfind("/groq ", 0) == 0 || text.rfind("/anthropic ", 0) == 0) {
        return this->handleCommand(text);
    }
    this->say(Who::User, text);

    if (text[0] == '/') return this->handleCommand(text);

    if (m_client.busy()) return this->say(Who::Error, "Still thinking... one moment.");

    std::string png;
    if (m_imagePending && m_circle) png = ai::img::base64File(m_circlePath);

    this->say(Who::System, "AI is working...");
    m_client.send(text, png, [this](Result<std::string> res) {
        if (res.isErr()) return this->say(Who::Error, res.unwrapErr());
        this->handleReply(res.unwrap());
    });
    m_imagePending = false;
}

void AIChatPopup::handleCommand(std::string const& cmd) {
    auto mod = Mod::get();
    auto argOf = [&](size_t skip) {
        std::string v = cmd.size() > skip ? cmd.substr(skip) : "";
        v.erase(std::remove_if(v.begin(), v.end(), [](unsigned char c) { return std::isspace(c); }), v.end());
        return v;
    };

    if (cmd == "/help") {
        return this->say(Who::System,
            "/groq <key> - use a Groq key (sets model + endpoint too)\n"
            "/anthropic <key> - use an Anthropic key\n"
            "/key <key> - only change the key\n"
            "/model <name> - change the model\n"
            "/status - show current setup\n"
            "/image [cells] - turn the selected image into blocks (no key needed)\n"
            "/clear - clear chat\n"
            "Anything else goes to the AI.");
    }
    if (cmd.rfind("/groq ", 0) == 0 || cmd.rfind("/anthropic ", 0) == 0 || cmd.rfind("/key ", 0) == 0) {
        bool groq = cmd.rfind("/groq ", 0) == 0;
        bool anth = cmd.rfind("/anthropic ", 0) == 0;
        auto key = argOf(cmd.find(' ') + 1);
        if (key.size() < 10) return this->say(Who::Error, "That key looks too short. Paste the full key after the command.");
        if (groq) {
            (void)mod->setSettingValue<std::string>("endpoint", "https://api.groq.com/openai/v1/chat/completions");
            (void)mod->setSettingValue<std::string>("model", "llama-3.3-70b-versatile");
        } else if (anth) {
            (void)mod->setSettingValue<std::string>("endpoint", "https://api.anthropic.com/v1/messages");
            (void)mod->setSettingValue<std::string>("model", "claude-sonnet-5-5");
        }
        (void)mod->setSettingValue<std::string>("api-key", key);
        return this->say(Who::System, fmt::format("Key saved ({} characters). Ask me something!", key.size()));
    }
    if (cmd.rfind("/model ", 0) == 0) {
        auto name = argOf(7);
        if (name.empty()) return this->say(Who::Error, "Usage: /model <name>");
        (void)mod->setSettingValue<std::string>("model", name);
        return this->say(Who::System, "Model set to " + name);
    }
    if (cmd == "/status") {
        auto key = mod->getSettingValue<std::string>("api-key");
        return this->say(Who::System, fmt::format("endpoint: {}\nmodel: {}\nkey: {} characters, starts with '{}'",
            mod->getSettingValue<std::string>("endpoint"),
            mod->getSettingValue<std::string>("model"),
            key.size(), key.substr(0, 4)));
    }
    if (cmd == "/clear") return this->onClear();
    if (cmd.rfind("/image", 0) == 0) {
        int cells = 32;
        if (cmd.size() > 7) cells = std::atoi(cmd.c_str() + 7);
        auto task = matjson::makeObject({
            {"name", "Image to objects"},
            {"type", "image_to_objects"},
            {"params", matjson::makeObject({{"cells", cells > 0 ? cells : 32}})}
        });
        return this->runTask(task);
    }
    this->say(Who::Error, "Unknown command. Try /help");
}

void AIChatPopup::handleReply(std::string const& raw) {
    // Models sometimes wrap JSON in prose or fences - extract the outermost {...}.
    auto a = raw.find('{'), b = raw.rfind('}');
    if (a == std::string::npos || b == std::string::npos || b < a) return this->say(Who::AI, raw);

    auto parsed = matjson::parse(raw.substr(a, b - a + 1));
    if (parsed.isErr()) return this->say(Who::AI, raw);
    auto const& root = parsed.unwrap();

    if (root.contains("reply")) this->say(Who::AI, root["reply"].asString().unwrapOr(""));
    if (root.contains("tasks") && root["tasks"].isArray()) {
        for (auto const& task : root["tasks"]) this->runTask(task);
    }
}

void AIChatPopup::runTask(matjson::Value const& task) {
    std::string name = task.contains("name") ? task["name"].asString().unwrapOr("Unnamed") : "Unnamed";
    auto res = ai::tasks::run(m_lel, task, m_circle ? &*m_circle : nullptr);
    if (res.ok) {
        this->say(Who::System, fmt::format("Task [{}] completed ({})", name, res.message));
        Notification::create(fmt::format("Task [{}] completed", name), NotificationIcon::Success, 1.5f)->show();
    } else {
        this->say(Who::Error, fmt::format("Task [{}] failed: {}", name, res.message));
    }
}
