MageFight openFrameworks

        
→
→

      

Raylib Migration Plan

This document details the architectural migration plan for refactoring MageFight from openFrameworks (OF) to Raylib while preserving strict deterministic lockstep multiplayer compatibility, 64-bit FNV-1a state checksum parity, and full Steamworks P2P networking.
1. Project Context & Motivation

The original codebase couples simulation, graphics, audio, UI, and networking inside a monolithic controller:

    src/ofApp.cpp: ~49,800 lines

    src/ofApp.h: ~2,530 lines

    src/SteamManager.cpp: ~850 lines

A naive rewrite introduces game-breaking desyncs in lockstep simulation. Therefore, the migration follows a strict dual-layer architecture:

    src/core/ (Simulation Core): 100% headless, zero graphics/audio dependencies, zero OF headers. Driven by a deterministic simulation tick (

            
    60 Hz
    60 Hz

          

    or step-driven) with fixed-width integer coordinates and FNV-1a checksum validation.

    src/render/ & src/net/ (Raylib Client & Steam Networking): Consumes state from src/core/, generates input commands (Command), polls Steam P2P packets, renders the 3D isometric arena and 2D card UI, and triggers non-authoritative audio/visual effects.

2. Environment & Dependency Stack

    Target OS: Linux (Bazzite / Fedora Atomic, GCC 17+, CMake 3.25+), Windows (MSVC / MinGW-w64).

    Core Library: Raylib v5.5 via FetchContent (with -DCMAKE_POLICY_VERSION_MINIMUM=3.5 and CMP0135 set to NEW).

    3D Asset Pipeline: libassimp linked directly via CMake to retain existing skeletal .fbx and .obj meshes without lossy conversions.

    Serialization & JSON: nlohmann/json (header-only, replacing ofJson).

    Networking: Valve Steamworks SDK (Flat C API wrappers in SteamManager to bypass GCC/MinGW C++ VTable ABI mismatches).

3. Core Domain Architecture (src/core/)

    Architectural Guardrail: The temporary 2D prototype (Vec2 continuous movement, player.health = 100) is superseded by the actual MageFight grid and card rules:

A. Board & Spatial Representation

    Grid Coordinates: Fixed discrete grid

            
    13×9
    13×9

          

    (BOARD_WIDTH = 13, BOARD_HEIGHT = 9), tile size

            
    4.0 units
    4.0 units

          

    .

    Tile Properties:

        hasWall (Stone obstruction; blocks LoS and movement).

        isMagicWall (Amplifies Magic damage

                
        2×
        2×

              

        , halves Physical damage

                
        0.5×
        0.5×

              

        ).

        hasPlayer (Occupancy flag).

        FloatingKey instances (Bronze = Class 1 draft, Silver = Class 2 draft, Gold = Class 3 draft).

B. Unit & Minion Model (UnitState)

    Unit Types: Wizard players (IDs

            
    0
    0

          

    –

            
    3
    3

          

    ) and 10 Minion archetypes (Skeleton, Golem, Wolf, Hellhound, Demon, Kobold, Kobold King, Faerie, Assistant, WallUnit).

    Health & Defenses:

        health, maxHealth.

        5-Tier Mitigation Layer: block (Physical), fortification (Physical/Piercing), barrier (Non-Physical), holyBlock (Holy), ward (All damage).

    Turn & Action State:

        ap (Action Points rolled per unit via species dice: 1d6, 2d6, 4d4, 1d10, or coin flip).

        Status flags: onFire (1d6/turn), isPoisoned (decaying tick), isParalyzed (coinflip lock), sleepTurnsRemaining, inTortoiseForm (permanent defense, retaliation spikes), inGhostForm (phases through walls, immune to Physical/Piercing, vulnerable to Holy).

    Card Piles: Discrete std::vector<CardInstance> containers for deck, hand, discardPile, and playedCardsPile.

C. Deterministic Command Queue

All state mutations are driven by deterministic Command packets sequenced by commandId and validated against globalTurnCounter:
code C++

enum class CommandType : uint8_t {
    NONE = 0,
    MOVE_UNIT,           // Grid traversal along shortest path
    PLAY_CARD,           // Card play with target coordinates
    DRAW_CARDS,          // Deck draw execution
    ACCEPT_DRAFT,        // Class draft commitment
    DRAFT_ACTION,        // Draft card selection toggle
    MENU_CHOICE,         // Modal choices (Amnesia, Dispel, Burst of Light, etc.)
    STATUS_ACTION,       // Status purge/application
    RENEWED_INSPIRATION, // Discard & redraw mask
    END_TURN,            // Turn handoff
    PSEUDO_ACTION        // Placement, forfeits, rerolls, state transitions
};

D. Full FNV-1a Checksum Specification

The state hash must strictly reproduce ofApp::calculateChecksum():

    Global & RNG: globalTurnCounter, currentPlayerIndex, currentAP, gameplayRngAdvanceCount, nextSummonOrder.

    State & Draft: syncState, isInGameDraft, initialDraftComplete, draftStage, draftPlayerIndex, draftPicksRemaining, currentDraftClassTier, draftQueue, keys, graveyard.

    Board Grid:

            
    13×9
    13×9

          

    matrix of hasWall, isMagicWall, hasPlayer.

    Units: Sort order, IDs, coords, HP, all 5 defenses, luck, statuses, card counts, and card identifiers across all piles.

4. Subsystem Replacement Specifications
A. Window & Lifecycle
openFrameworks	Raylib	Implementation Notes
ofSetupOpenGL(), ofRunApp()	InitWindow(), SetTargetFPS()	Run headless when MAGEFIGHT_HEADLESS env var is present.
ofApp::update(), ofApp::draw()	while (!WindowShouldClose())	Fixed timestep accumulator loop (SIMULATION_TIMESTEP = 1.0f / 60.0f).
ofGetElapsedTimef()	GetTime()	Use std::chrono for internal network timers; keep GetTime() for render FX.
ofSetFullscreen()	ToggleFullscreen(), SetWindowSize()	Supports True Fullscreen, Borderless Windowed, and Windowed.
B. Input & Coordinate Raycasting
openFrameworks	Raylib	Implementation Notes
mouseToBoard() (GLM intersect ray-plane)	GetScreenToWorldRay() + GetRayCollisionQuad()	Cast ray against horizontal

        
Y=0
Y=0

      

board quad to get integer tile

        
(gx,gy)
(gx,gy)

      

.
mouseMoved(), mousePressed()	GetMousePosition(), IsMouseButtonPressed()	Frame-by-frame polling; map screen coords into virtual

        
1080p
1080p

      

reference canvas.
GLFW Hardware Cursors	SetMouseCursor() / Custom Texture Quads	Default, Click (Pointer), Grab, and Hold (Dragging) cursor states.
C. 3D Scene & Cameras
openFrameworks	Raylib	Implementation Notes
ofCamera	Camera3D	Mode CAMERA_PERSPECTIVE, fovy = 60.0f, up = {0, 1, 0}.
Orbiting Camera Angle	MatrixRotateY(), Vector3Transform()	

        
0∘
0∘

      

for P0/P2,

        
180∘
180∘

      

for P1/P3. Dynamic interpolation with exponential smoothing.
Camera Shake	Camera offset manipulation	Additive high-frequency vector noise decaying over cameraShakeDuration.
cam.worldToScreen()	GetWorldToScreen()	Anchor in-world unit HP/shield pips and damage numbers to 2D UI space.
D. Geometry & Models
openFrameworks	Raylib	Implementation Notes
ofMesh (Floor & Wall quads)	Mesh + UploadMesh() + Model	Static multi-tile batched mesh; textures bound to material.maps[MATERIAL_MAP_DIFFUSE].
ofxAssimpModelLoader	libassimp

        
→
→

      

Raylib Model	Load node hierarchy and meshes via Assimp; map bone transforms directly into Raylib meshes.
Dice Meshes (D4, D6, D10, D20, Coin)	Raylib procedural meshes / .obj models	Unlit textures; orient faces using quaternion rotations matching target results.
E. Render Pipeline & Shaders
openFrameworks	Raylib	Implementation Notes
Multi-pass ofFbo	RenderTexture2D	Pass 1: 3D scene + world FX to texture; Pass 2: Shaders; Pass 3: 2D UI on top.
s_foilShader (TF2-style sheen)	Raylib Shader (GLSL 330)	Sweeping metallic reflection for Class 1/2/3 card borders and metallic bars.
s_burnShader (Dissolve)	Raylib Shader (GLSL 330)	Procedural noise edge dissolve for card destruction and Amnesia discards.
Pixel Art / C64 Shaders	Fullscreen post-processing shaders	Downscaled render target pass with palette quantization and optional scanlines.
F. 2D User Interface
openFrameworks	Raylib	Implementation Notes
ofTrueTypeFont	Font, LoadFontEx()	Pre-bake font sizes (

        
22px
22px

      

,

        
24px
24px

      

,

        
40px
40px

      

) with Latin + CJK ranges from lang_*.json.
Hand Card Fan Layout	Mathematical Bezier Fan	Dynamic overlap calculation for

        
>3
>3

      

cards, hover lift

        
−350px
−350px

      

,

        
2×
2×

      

hover scale.
UI Boxes / Metallic Bars	DrawRectangleRounded(), custom shaders	Multi-layer beveled metallic health and shield segments.
G. Audio System
openFrameworks	Raylib	Implementation Notes
ofSoundPlayer	Sound, Music (InitAudioDevice())	Positional stereo panning based on tile coordinates; playlist shuffling for ambience.
Pause Audio Muffle	SetMusicPitch(), SetMusicVolume()	Smooth exponential dampening (

        
0.35×
0.35×

      

volume,

        
0.88×
0.88×

      

pitch) during menus.
H. Steam Networking & Platform

    Refactor SteamManager:

        Replace ofLogNotice / ofLogError with structured logger or std::cout.

        Replace ofImage / ofPixels with Raylib Image (GetImageRGBA).

        Replace ofJson with nlohmann::json.

        Replace ofGetElapsedTimef() with GetTime().

    Retain exact Flat C API wrappers to prevent GCC/MinGW VTable crashes.

5. Phased Implementation Roadmap
code Code

[Phase 1: Domain Core]       -> Completed (Base types extracted)
[Phase 2: Checksum Harness]   -> Completed (FNV-1a verification active)
[Phase 3: Command Queue]      -> Completed (Lockstep queue core implemented)
[Phase 4: Raylib Scaffold]    -> Completed (Window & build pipeline validated)
      │
      ▼
[Phase 5: Real Game Domain]   -> Replace toy types with full 13x9 Board, Units & Cards
[Phase 6: Raylib 3D Arena]    -> Camera3D, Raycast board picking, 13x9 Mesh generation
[Phase 7: Card Hand & 2D UI]  -> Hand fan math, card drawing, draft screen, Action History
[Phase 8: Shader Pipeline]    -> GLSL 330 Foil Sheen, Burn Dissolve, Pixel/C64 shaders
[Phase 9: Net & Steam Audio]  -> Decouple SteamManager, integrate P2P with CommandQueue
[Phase 10: Parity & Cleanup]  -> Lockstep verification, trace comparison, decommission OF

Phase 5: Real Game Domain Integration

    Upgrade src/core/GameState.h to store the

            
    13×9
    13×9

          

    tile grid (walls, magic walls, keys).

    Upgrade src/core/PlayerState.h to src/core/UnitState.h with combat stats, 5 defense tiers, statuses, and card piles.

    Port src/core/CardData.h: Load and parse cards.json and cards.md via nlohmann::json.

    Implement the full deterministic FNV-1a checksum calculation in src/core/Checksum.cpp.

    Update src/core/Harness.cpp to parse real MageFight snapshot files and verify checksum matches ofApp.cpp.

Phase 6: Raylib 3D Arena & Board Picking

    Implement src/render/ArenaCamera.cpp: 3D perspective camera with orbit orientation (

            
    0∘/180∘
    0∘/180∘

          

    ) and trauma-based camera shake.

    Implement src/render/BoardRenderer.cpp: Procedural Raylib Mesh generation for floor tiles and textured walls.

    Implement board tile picking via GetScreenToWorldRay() and GetRayCollisionQuad().

    Implement 3D unit rendering (load models via Assimp into Raylib Model structs, apply rotation and team rings).

    Add 3D dice rendering (D4, D6, D10, D20, Coin) with quaternion face alignment.

Phase 7: 2D Card Hand, UI & Drafting

    Implement src/render/HandRenderer.cpp: Smooth curve fan math, hovering, dragging, and card slot lerping.

    Port live card text formatting (outlines, scaled arced card titles, bold keyword tagging).

    Implement draft screen overlay: 3-card presentation, Class 1/2/3 banners, countdown timer, accept button.

    Implement HUD: Metallic health/shield segmented bars, Action Points counter, Action History icons, player profile cards.

Phase 8: Shader Pipeline & Special FX

    Port s_foilShader and s_burnShader to GLSL 330 core for Raylib.

    Create RenderTexture2D pipeline for post-processing shaders (Pixel Art & C64 scanline passes).

    Implement 3D particle debris for wall destruction (stone chunks with bounce physics) and spell tracers.

Phase 9: Steam Networking & Audio Integration

    Remove all openFrameworks headers from SteamManager.h and SteamManager.cpp.

    Convert Steam avatar loading to Raylib Image.

    Connect SteamManager packet polling loop directly to LockstepCommandQueue.

    Implement src/audio/AudioManager.cpp: Initialize Raylib audio, load SFX/music, implement ambient playlist shuffler and pause muffle effect.

Phase 10: Sync Verification & Decommissioning

    Run automated lockstep trace comparisons between Raylib client and openFrameworks client across 100 turns.

    Validate desync rollback handling and snapshot restoration.

    Remove ofApp.cpp, ofApp.h, and openFrameworks build scripts from the repository.

6. Build & Verification Commands
Build the Raylib Client
code Bash

cmake -S raylib_port -B raylib_port/build -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build raylib_port/build -j$(nproc)
./raylib_port/build/magefight_raylib

Run the Headless Simulation & Checksum Harness
code Bash

g++ -std=c++17 -Isrc/core -Ilibs/json/include src/core/Harness.cpp -o harness_test
./harness_test