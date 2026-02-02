#pragma once
#include <cstdint>

enum PacketType {
	PKT_HANDSHAKE,
	PKT_ACTION,
	PKT_END_TURN,
	PKT_CHECKSUM_CHECK
};

#pragma pack(push, 1)

struct PacketHeader {
	uint8_t type; // PacketType
	uint32_t playerID; // Who sent this?
};

struct HandshakePacket : PacketHeader {
	uint32_t seed; // The RNG seed (Host generates, Client receives)
};

struct ActionPacket : PacketHeader {
	int32_t cardIndex;
	int32_t targetX;
	int32_t targetY;
	int32_t cost;
};

struct ChecksumPacket : PacketHeader {
	int64_t checksum; // Compare game state
	int32_t turnNumber;
};

// When a player moves their unit on the board
struct MoveUnitPacket : PacketHeader {
	int32_t fromX;
	int32_t fromY;
	int32_t toX;
	int32_t toY;
};

// When a player draws cards by clicking their deck
struct DrawCardsPacket : PacketHeader {
	int32_t playerIndex; // The index of the player/minion who drew
	int32_t numCards; // Number of cards drawn
};

// When a player uses an Assistant to reroll AP
struct AssistantRerollPacket : PacketHeader {
	int32_t assistantIndex; // Index of the assistant unit
	int32_t rerollNumDice; // Original number of dice rolled for AP
	int32_t rerollDiceSides; // Original sides of dice rolled for AP
};

// For drafting actions (selecting a card, accepting the draft)
struct DraftActionPacket : PacketHeader {
	uint8_t actionType; // 0 = SelectCard, 1 = AcceptDraft
	int32_t optionIndex; // Index of the card picked (-1 if accepting an empty selection)
	int32_t draftPlayerIdx; // The player (index) currently drafting
};

// When a player places a summoned minion that requires manual placement (e.g., Wolves, Kobolds)
struct PlaceSummonedMinionPacket : PacketHeader {
	uint8_t minionType; // Specific enum for minion type (e.g., WOLF, KOBOLD)
	int32_t targetX;
	int32_t targetY;
};

// For Amnesia card: when the player selects which cards to remove from deck
struct AmnesiaChoicePacket : PacketHeader {
	int32_t targetPlayerIndex;
	uint8_t numCardsToRemove;
	// For fixed-size packets, you'd typically send a fixed-size array or serialize/deserialize
	// a small vector. For simplicity here, assuming a max, or you can send multiple packets.
	int32_t selectedIndices[8]; // Assuming max 8 cards for a choice, adjust as needed
};

// For Magic Blast: player chooses Damage or Discard
struct MagicBlastChoicePacket : PacketHeader {
	int32_t targetPlayerIndex;
	uint8_t choiceType; // 0 = Damage, 1 = Discard
};

// For Double Handed: player chooses Punch or Hand Block
struct DoubleHandedChoicePacket : PacketHeader {
	int32_t targetPlayerIndex;
	uint8_t choiceType; // 0 = Punch, 1 = Hand Block
};

// For Burst of Light: player chooses Damage/Heal and then the target
struct BurstChoiceAndTargetPacket : PacketHeader {
	uint8_t choiceType; // 0 = Damage, 1 = Heal
	int32_t targetX;
	int32_t targetY;
};

// For Dispel: player chooses Barrier or Purge
struct DispelChoicePacket : PacketHeader {
	uint8_t choiceType; // 0 = Barrier, 1 = Purge
};

// For Dispel (Purge path): player selects target and then a specific status to remove
struct DispelTargetAndStatusPacket : PacketHeader {
	int32_t targetPlayerIndex;
	uint8_t statusIndex; // Index of the status from the `statusSelectLabels` list
};

// For Wisdom Boon: player chooses Damage or Block
struct WisdomBoonChoicePacket : PacketHeader {
	int32_t targetPlayerIndex;
	uint8_t choiceType; // 0 = Damage, 1 = Block
};

// For Train: player chooses AP or Draft
struct TrainChoicePacket : PacketHeader {
	uint8_t choiceType; // 0 = AP, 1 = Draft
};

// For Giant Magic Hand: player chooses Push or Pull
struct MagicHandChoicePacket : PacketHeader {
	int32_t targetWallX;
	int32_t targetWallY;
	uint8_t choiceType; // 0 = Push, 1 = Pull
};

// For Renewed Inspiration: player discards selected cards
struct RenewedInspireDiscardsPacket : PacketHeader {
	uint8_t numDiscards;
	int32_t discardedHandIndices[8]; // Max 8 cards to discard, adjust as needed
};

#pragma pack(pop)