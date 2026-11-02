#include "Checksum.h"
#include "Command.h"
#include "GameState.h"
#include "PlayerState.h"
#include "Simulation.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

uint32_t serialize_and_checksum(const PlayerState & playerState) {
	std::vector<uint8_t> serializedData;
	serializedData.insert(serializedData.end(),
		reinterpret_cast<const uint8_t *>(&playerState),
		reinterpret_cast<const uint8_t *>(&playerState) + sizeof(PlayerState));
	return fnv1a_checksum(serializedData);
}

uint32_t serialize_and_checksum(const GameState & gameState) {
	std::vector<uint8_t> serializedData;
	serializedData.insert(serializedData.end(),
		reinterpret_cast<const uint8_t *>(&gameState),
		reinterpret_cast<const uint8_t *>(&gameState) + sizeof(GameState));
	return fnv1a_checksum(serializedData);
}

void load_and_checksum(const std::string & filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		std::cerr << "[Harness Error] Failed to open file: " << filePath << std::endl;
		return;
	}

	json data;
	file >> data;
	file.close();

	PlayerState playerState {};
	playerState.position.x = data["playerState"]["position"]["x"];
	playerState.position.y = data["playerState"]["position"]["y"];
	playerState.velocity.x = data["playerState"]["velocity"]["x"];
	playerState.velocity.y = data["playerState"]["velocity"]["y"];
	playerState.color.r = data["playerState"]["color"]["r"];
	playerState.color.g = data["playerState"]["color"]["g"];
	playerState.color.b = data["playerState"]["color"]["b"];
	playerState.color.a = data["playerState"]["color"]["a"];
	playerState.health = data["playerState"]["health"];
	playerState.mana = data["playerState"]["mana"];

	GameState gameState {};
	gameState.cameraPosition.x = data["gameState"]["cameraPosition"]["x"];
	gameState.cameraPosition.y = data["gameState"]["cameraPosition"]["y"];
	gameState.cameraPosition.z = data["gameState"]["cameraPosition"]["z"];
	gameState.gameBounds.x = data["gameState"]["gameBounds"]["x"];
	gameState.gameBounds.y = data["gameState"]["gameBounds"]["y"];
	gameState.gameBounds.width = data["gameState"]["gameBounds"]["width"];
	gameState.gameBounds.height = data["gameState"]["gameBounds"]["height"];
	gameState.isPaused = data["gameState"]["isPaused"];

	std::vector<Command> commands;
	if (data.contains("commands")) {
		for (const auto & cmdData : data["commands"]) {
			Command cmd {};
			cmd.tick = cmdData["tick"];
			std::string typeStr = cmdData["type"];
			if (typeStr == "MOVE") {
				cmd.type = CommandType::MOVE;
				cmd.target.x = cmdData["target"]["x"];
				cmd.target.y = cmdData["target"]["y"];
			} else if (typeStr == "CAST_SPELL") {
				cmd.type = CommandType::CAST_SPELL;
				std::string spellStr = cmdData["spellType"];
				if (spellStr == "FIREBALL") {
					cmd.spellType = SpellType::FIREBALL;
				}
			}
			commands.push_back(cmd);
		}
	}

	std::cout << "[Harness] Starting 10-tick deterministic simulation..." << std::endl;
	for (uint32_t tick = 0; tick < 10; ++tick) {
		simulate_tick(gameState, playerState, commands, tick);
		uint32_t pChecksum = serialize_and_checksum(playerState);
		uint32_t gChecksum = serialize_and_checksum(gameState);

		std::cout << "  [Tick " << tick << "] Player Health: " << playerState.health
				  << " | Pos: (" << playerState.position.x << ", " << playerState.position.y << ")"
				  << " | P-Checksum: 0x" << std::hex << pChecksum << std::dec << std::endl;
	}
}

int main() {
	load_and_checksum("autosave.json");
	return 0;
}