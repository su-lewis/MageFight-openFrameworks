#include "core/Command.h"
#include "core/GameState.h"
#include "core/PlayerState.h"
#include "core/Simulation.h"
#include "raylib.h"
#include "raymath.h"

#include <cmath>
#include <vector>

// --- BOARD DEFINITIONS (from ofApp.cpp) ---
constexpr int BOARD_WIDTH = 13;
constexpr int BOARD_HEIGHT = 9;
constexpr float TILE_SIZE = 4.0f;

// Converts (x, y) grid coordinates to 3D world coordinates
Vector3 GridToWorld(int gx, int gy) {
	float wx = (gx - BOARD_WIDTH / 2.0f) * TILE_SIZE + (TILE_SIZE / 2.0f);
	float wz = (gy - BOARD_HEIGHT / 2.0f) * TILE_SIZE + (TILE_SIZE / 2.0f);
	return { wx, 0.0f, wz };
}

// Raycasts from mouse cursor through the 3D camera to find the board tile
bool GetTileUnderMouse(Camera3D camera, int & outX, int & outY) {
	Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);

	float halfW = (BOARD_WIDTH * TILE_SIZE) / 2.0f;
	float halfH = (BOARD_HEIGHT * TILE_SIZE) / 2.0f;

	Vector3 p1 = { -halfW, 0.0f, -halfH };
	Vector3 p2 = { -halfW, 0.0f, halfH };
	Vector3 p3 = { halfW, 0.0f, halfH };
	Vector3 p4 = { halfW, 0.0f, -halfH };

	RayCollision hit = GetRayCollisionQuad(ray, p1, p2, p3, p4);
	if (!hit.hit) hit = GetRayCollisionQuad(ray, p1, p4, p3, p2);

	if (hit.hit) {
		float gx = (hit.point.x / TILE_SIZE) + (BOARD_WIDTH / 2.0f);
		float gy = (hit.point.z / TILE_SIZE) + (BOARD_HEIGHT / 2.0f);
		outX = static_cast<int>(std::floor(gx));
		outY = static_cast<int>(std::floor(gy));
		return (outX >= 0 && outX < BOARD_WIDTH && outY >= 0 && outY < BOARD_HEIGHT);
	}
	return false;
}

int main() {
	const int screenWidth = 1280;
	const int screenHeight = 720;
	InitWindow(screenWidth, screenHeight, "MageFight - 3D Board Raylib");
	SetTargetFPS(60);

	// 1. Setup 3D Isometric Camera (mimicking ofApp.cpp setup)
	Camera3D camera = { 0 };
	camera.position = { 0.0f, 45.0f, 32.0f }; // Elevated and pulled back
	camera.target = { 0.0f, 0.0f, 2.0f }; // Looking toward board center
	camera.up = { 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;

	// 2. Core State
	PlayerState player;
	player.position = { 0.0f, 8.0f }; // Grid tile (0, 8) - Player 1 start
	player.health = 15;

	GameState gameState;
	uint32_t currentTick = 0;
	std::vector<Command> commandQueue;

	while (!WindowShouldClose()) {
		// --- A. INPUT PHASE ---
		int hoveredTileX = -1;
		int hoveredTileY = -1;
		bool isHoveringBoard = GetTileUnderMouse(camera, hoveredTileX, hoveredTileY);

		// Click to move unit to tile
		if (isHoveringBoard && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			Command moveCmd;
			moveCmd.type = CommandType::MOVE;
			moveCmd.tick = currentTick;
			moveCmd.target = { (float)hoveredTileX, (float)hoveredTileY };
			commandQueue.push_back(moveCmd);
		}

		// --- B. SIMULATION PHASE ---
		simulate_tick(gameState, player, commandQueue, currentTick);
		commandQueue.clear();
		currentTick++;

		// --- C. RENDER PHASE ---
		BeginDrawing();
		ClearBackground(Color { 22, 22, 25, 255 }); // Dark dungeon clear

		// =====================================================================
		// 3D WORLD RENDERING STARTS HERE
		// =====================================================================
		BeginMode3D(camera);

		// 1. Draw 13x9 Grid Tiles
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			for (int x = 0; x < BOARD_WIDTH; x++) {
				Vector3 pos = GridToWorld(x, y);
				// Base floor tile outline
				DrawCubeWires(pos, TILE_SIZE, 0.1f, TILE_SIZE, DARKGRAY);
			}
		}

		// 2. Highlight hovered tile (Yellow border)
		if (isHoveringBoard) {
			Vector3 hoverPos = GridToWorld(hoveredTileX, hoveredTileY);
			hoverPos.y += 0.05f; // Slight elevation to avoid z-fighting
			DrawCubeWires(hoverPos, TILE_SIZE, 0.2f, TILE_SIZE, YELLOW);
		}

		// 3. Draw Player on their current grid tile
		Vector3 playerWorldPos = GridToWorld((int)player.position.x, (int)player.position.y);
		playerWorldPos.y += 1.5f; // Raised above floor
		DrawCylinder(playerWorldPos, 1.2f, 1.2f, 2.5f, 16, BLUE);
		DrawCylinderWires(playerWorldPos, 1.2f, 1.2f, 2.5f, 16, WHITE);

		EndMode3D();
		// =====================================================================
		// 3D WORLD RENDERING ENDS HERE
		// =====================================================================

		// --- D. 2D UI OVERLAYS (Drawn on top of 3D world) ---
		DrawText("MageFight: 13x9 Board View", 10, 10, 20, GOLD);
		DrawText(TextFormat("Tick: %u", currentTick), 10, 35, 18, LIGHTGRAY);
		DrawText(TextFormat("HP: %d", player.health), 10, 60, 18, RED);

		if (isHoveringBoard) {
			DrawText(TextFormat("Tile: [%d, %d]", hoveredTileX, hoveredTileY), 10, 85, 18, YELLOW);
		} else {
			DrawText("Tile: [--, --]", 10, 85, 18, GRAY);
		}

		DrawText("Left Click on any tile to move the unit", 10, screenHeight - 30, 20, RAYWHITE);

		EndDrawing();
	}

	CloseWindow();
	return 0;
}