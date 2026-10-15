# AI Level Assistant

Adds an **AI** button to the level editor. It opens a chat where you can ask for things like
"build a 10-step staircase" or "make a ring of 24 blocks".

- Every finished job prints `Task [Name] completed` in the chat.
- Press **Image** to pick a picture. It is resized, cropped to a small circle, and previewed.
- The AI sees the circle image and can turn it into colored block pixel-art in your level.
- Offline commands (no API key needed): `/image [cells]`, `/clear`, `/help`.

Set your Anthropic API key in the mod settings.
