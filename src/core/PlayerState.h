#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum DamageType : uint8_t {
	DAMAGE_PHYSICAL = 0,
	DAMAGE_PIERCING,
	DAMAGE_MAGIC,
	DAMAGE_ELECTRIC,
	DAMAGE_FIRE,
	DAMAGE_HOLY,
	DAMAGE_POISON
};

struct CardInstance {
	int32_t id { 0 };
	uint8_t type { 0 };
	int32_t cost { 0 };
	int32_t baseDamage { 0 };
};

struct UnitState {
	int32_t playerID { 0 };
	int32_t ownerID { 0 };
	bool isMinion { false };

	// Grid Coordinates (0 to 12, 0 to 8)
	int32_t x { 0 };
	int32_t y { 0 };

	// Health & Mitigation Shields
	int32_t health { 15 };
	int32_t maxHealth { 15 };
	int32_t block { 0 };
	int32_t fortification { 0 };
	int32_t barrier { 0 };
	int32_t holyBlock { 0 };
	int32_t ward { 0 };

	// Turn State
	int32_t ap { 0 };
	int32_t luck { 0 };
	bool onFire { false };
	bool isPoisoned { false };
	bool isParalyzed { false };
	int32_t sleepTurnsRemaining { 0 };
	bool inTortoiseForm { false };
	bool inGhostForm { false };

	// Card Piles
	std::vector<CardInstance> deck;
	std::vector<CardInstance> hand;
	std::vector<CardInstance> discardPile;
};