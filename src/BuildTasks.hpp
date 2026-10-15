#pragma once
#include <Geode/Geode.hpp>
#include "ImageTools.hpp"

struct TaskResult {
    bool ok = false;
    std::string message;
    int objects = 0;
};

namespace ai::tasks {
    // Runs one task object: {"name": "...", "type": "...", "params": {...}}
    // Coordinates in params are editor units (30 = 1 block) relative to the center of the current view.
    TaskResult run(LevelEditorLayer* lel, matjson::Value const& task, Bitmap const* circleImage);
}
