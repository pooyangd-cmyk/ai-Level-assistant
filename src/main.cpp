#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "AIChatPopup.hpp"

using namespace geode::prelude;

class $modify(AIEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto spr = ButtonSprite::create("AI", "goldFont.fnt", "GJ_button_01.png", 0.7f);
        spr->setScale(0.7f);
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCObject*) {
            if (auto popup = AIChatPopup::create(m_editorLayer)) popup->show();
        });
        btn->setID("ai-assistant-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("ai-assistant-menu"_spr);
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({34.f, win.height - 72.f});
        this->addChild(menu, 50);
        return true;
    }
};
