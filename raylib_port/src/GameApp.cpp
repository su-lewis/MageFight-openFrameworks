#include "GameApp.h"

#include "raylib.h"

bool GameApp::initialize(int width, int height, const std::string & title) {
	screenWidth_ = width;
	screenHeight_ = height;

	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow(screenWidth_, screenHeight_, title.c_str());
	InitAudioDevice();
	SetTargetFPS(60);

	initialized_ = true;
	running_ = !WindowShouldClose();
	return initialized_;
}

void GameApp::run() {
	if (!initialized_) {
		return;
	}

	while (running_ && !WindowShouldClose()) {
		update();

		BeginDrawing();
		draw();
		EndDrawing();
	}
}

void GameApp::update() {
	if (IsKeyPressed(KEY_ESCAPE)) {
		running_ = false;
	}

	screenWidth_ = GetScreenWidth();
	screenHeight_ = GetScreenHeight();
}

void GameApp::draw() {
	ClearBackground((Color) { 20, 24, 32, 255 });

	DrawText("MageFight raylib migration scaffold", 32, 28, 30, RAYWHITE);
	DrawText("This binary is the new port target.", 32, 74, 22, LIGHTGRAY);
	DrawText("Current status: rendering/input loop is raylib.", 32, 112, 22, LIGHTGRAY);
	DrawText("Next: port deterministic game state from ofApp into engine modules.", 32, 150, 22, GOLD);

	DrawRectangleLines(24, 20, screenWidth_ - 48, screenHeight_ - 40, (Color) { 120, 140, 170, 255 });
}

void GameApp::shutdown() {
	if (!initialized_) {
		return;
	}

	CloseAudioDevice();
	CloseWindow();
	initialized_ = false;
}
