#pragma once
#include <cstdint>
#include <string>

enum class CommandType : uint8_t {
	NONE = 0,
	MOVE_UNIT,
	PLAY_CARD,
	DRAW_CARDS,
	ACCEPT_DRAFT,
	DRAFT_ACTION,
	MENU_CHOICE,
	END_TURN,
	PSEUDO_ACTION
};

struct Command {
	uint32_t tick { 0 };
	uint32_t commandId { 0 };
	uint32_t playerID { 0 };
	CommandType type { CommandType::NONE };
	int32_t params[8] { 0 };
	char stringData[64] { 0 };
};