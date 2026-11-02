#pragma once

#include <cstdint>
#include <string>

// Match the exact damage types from ofApp.cpp
enum class DamageType : uint8_t {
	PHYSICAL = 0,
	PIERCING,
	MAGIC,
	ELECTRIC,
	FIRE,
	HOLY,
	POISON
};

// Target types from ofApp.cpp
enum class TargetingType : uint8_t {
	TARGET_NONE = 0,
	TARGET_SELF,
	TARGET_ADJACENT_UNIT,
	TARGET_ADJACENT_UNIT_OR_WALL,
	TARGET_ADJACENT_OR_SELF_UNIT,
	TARGET_EMPTY_ADJACENT,
	TARGET_EMPTY_TILE,
	TARGET_ADJACENT_WALL,
	TARGET_LINE_OF_SIGHT_TILE,
	TARGET_ANY_TILE,
	TARGET_LINEAR_PIERCE,
	TARGET_CLEAVE_ADJACENT,
	TARGET_BURST_AREA
};

// Core Card Types from cards.json / ofApp.cpp
enum class CardType : uint8_t {
	CARD_NONE = 0,
	CARD_PUNCH,
	CARD_KICK,
	CARD_BASH,
	CARD_STAB,
	CARD_SLASH,
	CARD_HEAL,
	CARD_LESSER_HEAL,
	CARD_FIREBALL,
	CARD_SHOCK,
	CARD_ROCK_CRUSH,
	CARD_DEMOLITION,
	CARD_DISPEL,
	CARD_TELEPORT,
	CARD_HASTEN,
	CARD_REPLICATE,
	CARD_WISDOM_BOON,
	CARD_ETHEREAL_JOLT,
	CARD_FLAME_HIT,
	CARD_RAISE_DEAD,
	CARD_SUMMON_GOLEM,
	CARD_STRENGTHEN_ELEMENTS,
	CARD_CREATE_WALL,
	CARD_FORTIFY,
	CARD_VAMPIRE_BITE,
	CARD_DARK_SHIELD,
	CARD_DRAIN_PUNCH,
	CARD_DOUBLE_HANDED,
	CARD_CALL_FOR_WOLVES,
	CARD_CALL_FOR_KOBOLDS,
	CARD_NECROMANCER_S_BLESSING,
	CARD_TIME_VORTEX,
	CARD_MASTER_FIST,
	CARD_MAGIC_BOLT,
	CARD_FLAIL,
	CARD_SUMMON_HELLHOUND,
	CARD_DEATH,
	CARD_SUMMON_DEMON,
	CARD_SHIELD_BASH,
	CARD_CHAIN_LIGHTNING,
	CARD_CONSUME_HEALTH_POTION,
	CARD_ADD_POISON,
	CARD_FLURRY_OF_FISTS,
	CARD_FORM_OF_TORTOISE,
	CARD_RENEWED_INSPIRATION,
	CARD_SPARK_OF_GENIUS,
	CARD_PSIONIC_WAVE,
	CARD_EARTHQUAKE,
	CARD_FORM_OF_GHOST,
	CARD_GIANT_MAGIC_HAND,
	CARD_CONSUME_HEALTH_FLAGON,
	CARD_TRANSFORM_WALL,
	CARD_SUMMON_KOBOLD_KING,
	CARD_SUMMON_ASSISTANT,
	CARD_SUMMON_FAERIE,
	CARD_FOUR_LEAF_CLOVER,
	CARD_SPRINT,
	CARD_SMITE,
	CARD_BURST_OF_LIGHT,
	CARD_SHOOT_ARROW,
	CARD_FULL_RESTORE,
	CARD_TRAIN,
	CARD_STUDY,
	CARD_BLOCKING_BOON,
	CARD_CONSTITUTION_BOON,
	CARD_SUMMON_WALL,
	CARD_SUMMON_MAGIC_WALL
};

// Pure C++ Card model matching cards.json
struct Card {
	int32_t id { 0 };
	std::string name;
	CardType type { CardType::CARD_NONE };
	int32_t cost { 0 };
	int32_t cardClass { 1 }; // Class 1, 2, or 3

	TargetingType targeting { TargetingType::TARGET_NONE };
	DamageType damageType { DamageType::PHYSICAL };

	// Combat Stats
	int32_t baseDamage { 0 };
	int32_t damageDiceNum { 0 };
	int32_t damageDiceSides { 0 };
	int32_t rangeDiceNum { 0 };
	int32_t rangeDiceSides { 0 };

	// Defenses Granted
	int32_t blockAmount { 0 };
	int32_t fortificationAmount { 0 };
	int32_t barrierAmount { 0 };
	int32_t holyBlockAmount { 0 };
	int32_t wardAmount { 0 };

	// Healing & AP
	int32_t healAmount { 0 };
	int32_t baseHeal { 0 };
	int32_t apGain { 0 };
	int32_t drawCount { 0 };

	bool isHandRelated { false };
	bool isAoe { false };
};