#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextArea.hpp>
#include <Geode/utils/async.hpp>
#include <optional>
#include "AIClient.hpp"
#include "ImageTools.hpp"

class AIChatPopup : public geode::Popup {
public:
    static AIChatPopup* create(LevelEditorLayer* lel);

protected:
    bool init(LevelEditorLayer* lel);

    void onSend();
    void onPickImage();
    void onClear();

    void handleCommand(std::string const& cmd);
    void handleReply(std::string const& raw);
    void runTask(matjson::Value const& task);
    void loadImage(std::filesystem::path const& path);

    enum class Who { User, AI, System, Error };
    void say(Who who, std::string text);
    void rebuildChat();

    LevelEditorLayer* m_lel = nullptr;
    geode::TextInput* m_input = nullptr;
    geode::ScrollLayer* m_scroll = nullptr;
    geode::TextArea* m_chatText = nullptr;
    cocos2d::CCNode* m_previewHolder = nullptr;
    cocos2d::CCLabelBMFont* m_previewLabel = nullptr;

    std::vector<std::pair<Who, std::string>> m_lines;
    std::optional<Bitmap> m_circle;       // current circle-cropped image
    std::filesystem::path m_circlePath;   // where the preview PNG was saved
    bool m_imagePending = false;          // attach image to next AI message
    AIClient m_client;
    geode::async::TaskHolder<geode::Result<std::optional<std::filesystem::path>>> m_pick;
};
