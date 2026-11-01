# MageFight openFrameworks -> raylib Migration Plan

This folder is the in-repo raylib target for the migration.

## Why this is staged

The current game is deeply coupled to openFrameworks in a single app class:

- `src/ofApp.cpp`: 49,779 lines
- `src/ofApp.h`: 2,534 lines
- `src/SteamManager.cpp`: 849 lines

A one-shot rewrite would likely break deterministic multiplayer lockstep behavior.

## Priority conversion map (from symbol frequency)

Highest-impact openFrameworks usage in core files:

- `ofColor` (1034)
- `ofSetColor` (467)
- `ofRectangle` (442)
- `ofGetHeight` (238)
- `ofGetWidth` (194)
- `ofGetElapsedTimef` (175)
- `ofDrawRectRounded` (175)
- `ofImage` (44)
- `ofTexture` (37)
- `ofTrueTypeFont` (28)
- `ofxAssimpModelLoader` (24)
- `ofMesh` (19)
- `ofJson` (18)
- `ofFbo` (15)
- `ofSoundPlayer` (14)
- `ofCamera` (11)
- `ofShader` (9)

## Subsystem replacement targets

- Window + game loop:
  - OF: `ofRunApp`, `ofRunMainLoop`
  - raylib: `InitWindow`, `WindowShouldClose`, `BeginDrawing`, `EndDrawing`
- Input:
  - OF mouse/key events
  - raylib polling (`IsMouseButton...`, `GetMousePosition`, `IsKey...`)
- 2D draw/UI:
  - OF immediate drawing APIs
  - raylib shapes + textures + text
- 3D models:
  - OF addon `ofxAssimpModelLoader`
  - raylib `LoadModel`/`DrawModel` (or custom assimp integration where needed)
- Render pipeline:
  - OF `ofFbo`, `ofShader`
  - raylib `RenderTexture2D`, `Shader`, explicit pass scheduling
- Audio:
  - OF `ofSoundPlayer`
  - raylib audio streams/sounds
- Files + JSON:
  - OF `ofFile`, `ofJson`
  - C++17 filesystem + nlohmann json (already used in OF as alias)

## Deterministic lockstep caution

- Keep gameplay RNG independent from visual RNG.
- Keep gameplay state updates independent from render timing.
- Preserve strict tie-break sorting.
- Prefer integer math for gameplay-critical decisions.

## Next implementation phases

1. Extract deterministic gameplay domain from `ofApp` into framework-agnostic modules.
  - Progress (2026-11-01): Added a first extracted module (`src/core/*`) containing deterministic snapshot data types and FNV-1a checksum logic used by headless harness mode.
2. Port headless checksum harness to raylib target using extracted core.
  - Progress (2026-11-01): Headless mode now loads real snapshot save files (`Saves/autosave.json` format), resolves card metadata from `cards.json`, resolves `CardType` enum values from `src/ofApp.h`, and computes deterministic checksums.
3. Port input command queue and network packet processing.
  - Progress (2026-11-01): Extracted lockstep command queue core (`src/core/LockstepCommandQueue.*`) with deterministic ordering, missing-command blocking, duplicate filtering, and optimistic-echo skip handling. Added harness coverage for these behaviors.
4. Port UI rendering to raylib while preserving interaction behavior.
5. Port 3D scene and post-processing stack.
6. Remove OF build once feature parity and multiplayer sync checks are complete.

## Build commands

From project root:

```bash
cmake -S raylib_port -B raylib_port/build
cmake --build raylib_port/build -j
./raylib_port/build/magefight_raylib
```
