#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <functional>

// Talks to the Anthropic Messages API and keeps a short conversation history.
class AIClient {
public:
    using Callback = std::function<void(geode::Result<std::string>)>;

    // pngBase64 may be empty. The reply is the raw model text (JSON, see system prompt).
    void send(std::string userText, std::string pngBase64, Callback cb);
    void reset() { m_history.clear(); }
    bool busy() const { return m_busy; }

private:
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_holder;
    std::vector<matjson::Value> m_history;
    bool m_busy = false;
};
