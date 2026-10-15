# AI Level Assistant (Geode v5.11.1)

## Build the .geode file

### Option A - GitHub (no local setup, recommended)
1. Create a GitHub repo and push this folder.
2. Open the **Actions** tab -> *Build Binary*. When it finishes, download **Build Output**.
   It contains `aidev.ai-level-assistant.geode` (Windows, macOS, iOS, Android in one file).
3. Drop the file into your `geode/mods` folder (or install via the Geode mods menu).

### Option B - local
1. Install the Geode CLI: https://docs.geode-sdk.org/getting-started/geode-cli
2. `geode sdk install` (then `geode sdk update`, make sure it is v5.11.1 or newer)
3. `geode build` -> the `.geode` file appears in `build/`.

## Before you build
- Change `id` and `developer` in `mod.json` (the id must be `yourname.modname`).
- `gd` versions in `mod.json` must match the Geometry Dash version your Geode build supports.

## Usage
- Open the editor, press the **AI** button (top-left).
- Mod settings: paste your Anthropic API key, choose model, circle size, object limit.
- `Image` picks a PNG/JPG, crops it to a circle, shows a preview and saves it to the mod save dir.
- Ask: "turn my image into pixel art, 40 cells wide" or "make a staircase of 12 steps".
- Each finished task prints `Task [Name] completed`.
- Without an API key you can still use `/image [cells]`.

## Places most likely to need a tweak on first compile
These APIs were written against the v5 docs and Geode's headers from memory, and have not been compiled here:
- `file::FilePickOptions` / `file::pick` argument shape (AIChatPopup.cpp)
- `TextArea::create(...)` and `ScrollLayer` signatures (AIChatPopup.cpp)
- `matjson::parse`, `matjson::makeObject` (AIClient.cpp, AIChatPopup.cpp)
- `LevelEditorLayer::createObject` third parameter (BuildTasks.cpp)
- EffectGameObject member names (`m_targetColorID`, `m_triggerTargetColor`, ...) (BuildTasks.cpp)
Check the matching header in the Geode SDK / bindings if the compiler complains.
