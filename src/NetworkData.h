#pragma once
#include <cstdint>

#pragma pack(push, 1)

enum PacketType {
	PKT_HANDSHAKE,
	PKT_ACTION,
	PKT_END_TURN,
	PKT_CHECKSUM_CHECK,
	PKT_DRAFT_ACTION, // Draft selection / accept messages (sent by clients to host)
	PKT_DRAFT_STATE, // Host -> Client: draft state update (class, stage, player)
	PKT_DRAFT_OPTIONS, // Host -> Client: authoritative indices for options
	PKT_SHUFFLE, // Host -> Client: authoritative deck shuffle (playerIndex, nonce)
	PKT_TURN_START, // Host -> Client: authoritative turn start (current player, AP dice results)
	PKT_KEY_PICKUP, // Host -> Client: a player picked up a key (trigger in-game draft)
	PKT_CHAT_MESSAGE, // Chat message between players
	PKT_HOVER, // Hover state update for showing opponent's hover
	PKT_DRAW_CARDS, // Client -> Host: player drew cards from deck
	PKT_SNAPSHOT_BEGIN, // Host -> Client: begin state snapshot
	PKT_SNAPSHOT_CHUNK, // Host -> Client: snapshot data chunk
	PKT_SNAPSHOT_END, // Host -> Client: end state snapshot
	PKT_MENU_STATE, // Menu open/close/hover state for choice-based cards
	PKT_ACK // Acknowledge receipt of a reliable packet
};

struct PacketHeader {
	uint8_t type; // PacketType
	uint32_t playerID; // Who sent this?
	uint32_t seq; // Sequence number (monotonic per sender)
};

struct TurnStartPacket : PacketHeader {
	int32_t currentPlayerIndex; // who is starting
	uint8_t diceNum; // number of dice rolled (max 8)
	uint8_t diceSides; // sides per die
	uint8_t purpose; // DicePurpose (PURPOSE_AP or PURPOSE_BONUS_AP)
	int32_t finalTotal; // sum of final results (for immediate assignment)
	uint8_t rawResults[8]; // raw die faces (1..sides)
	uint8_t finalResults[8]; // final per-die results (raw + luck)
};

struct HandshakePacket : PacketHeader {
	uint32_t seed; // The RNG seed (Host generates, Client receives)
};

struct ActionPacket : PacketHeader {
	int32_t cardIndex;
	int32_t targetX;
	int32_t targetY;
	int32_t cost;
	char cardName[64]; // Card name for opponent to identify which card was played
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
	char cardNames[3][64]; // Names of up to 3 cards drawn (null-terminated strings)
};

// When a player uses an Assistant to reroll AP
struct AssistantRerollPacket : PacketHeader {
	int32_t assistantIndex; // Index of the assistant unit
	int32_t rerollNumDice; // Original number of dice rolled for AP
	int32_t rerollDiceSides; // Original sides of dice rolled for AP
};

// Menu state for choice-based cards (Wisdom Boon, Burst of Light, Double Handed)
struct MenuStatePacket : PacketHeader {
	int32_t menuType; // 0=none, 1=wisdom, 2=burst, 3=doubleHanded
	int32_t targetIndex; // Target player index
	int32_t hoveredChoice; // -1=none, 0=first option, 1=second option
	int32_t cardIndex; // Index of the card that opened the menu
};

// For drafting actions (selecting a card, accepting the draft)
struct DraftActionPacket : PacketHeader {
	uint8_t actionType; // 0 = SelectCard / ToggleSelect, 1 = AcceptDraft
	uint8_t selectFlag; // For SelectCard: 1 = select, 0 = deselect
	int32_t optionIndex; // Index of the card picked (-1 if accepting an empty selection)
	int32_t draftPlayerIdx; // The player (index) currently drafting
	int32_t classTier; // 1/2/3 for which pool the selection came from (for AcceptDraft)
	int32_t numSelected; // number of indices provided (for AcceptDraft)
	int32_t selectedIdx0; // up to 3 selections
	int32_t selectedIdx1;
	int32_t selectedIdx2;
};

// Host -> Client: Simple draft state update (no indices, just state)
struct DraftStatePacket : PacketHeader {
	int32_t classTier; // 1,2,3 or 0 when not drafting
	int32_t draftPlayerIdx; // which player is currently drafting
	int32_t picksRemaining; // how many picks left this stage
	int32_t draftStage; // 0 = class1, 1 = class2, etc
	uint8_t isInGameDraft; // 1 = in-game key draft, 0 = normal
	int32_t currentPlayerIndex; // Host tells clients who the active player is when drafting ends
};

// Host -> Client: send the indices in the pool for the options shown
struct DraftOptionsPacket : PacketHeader {
	int32_t classTier; // 1,2,3
	int32_t optionIndex0; // index into class pool
	int32_t optionIndex1;
	int32_t optionIndex2;
	int32_t draftPlayerIdx; // which player is currently drafting
	int32_t picksRemaining; // how many picks left for this stage
	int32_t draftStage; // 0 = class1, 1 = class2, etc
	uint8_t isInGameDraft; // 1 = in-game key draft, 0 = normal
};

// Host -> Client: Instruct client to apply a deterministic shuffle to a player's deck
struct ShufflePacket : PacketHeader {
	int32_t playerIndex; // which player's deck is being shuffled
	uint32_t nonce; // nonce used to seed local shuffle RNG
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

// For Key Pickup: Host tells client a player picked up a key
struct KeyPickupPacket : PacketHeader {
	int32_t playerIndex; // Which player picked up the key
	uint8_t classTier; // 1, 2, or 3
	int32_t keyX; // Grid position of the key
	int32_t keyY;
};

// For Chat Messages: Send text messages between players
struct ChatMessagePacket : PacketHeader {
	char message[256]; // Text message (null-terminated)
};

// For Hover State: Show what opponent is hovering over
struct HoverPacket : PacketHeader {
	uint8_t hoverType; // HoverType enum (HOVER_NONE, HOVER_UNIT, HOVER_DECK, HOVER_DISCARD, HOVER_HAND_CARD)
	int8_t gridX; // Grid X position (for units)
	int8_t gridY; // Grid Y position (for units)
	int8_t cardIndex; // Card index (for hand cards)
};

// Snapshot begin/end packets
struct SnapshotBeginPacket : PacketHeader {
	uint32_t snapshotId;
	uint32_t totalSize;
};

// Snapshot data chunk packet
struct SnapshotChunkPacket : PacketHeader {
	uint32_t snapshotId;
	uint32_t offset;
	uint16_t chunkSize;
	char data[512];
};

struct SnapshotEndPacket : PacketHeader {
	uint32_t snapshotId;
};

// Ack packet
struct AckPacket : PacketHeader {
	uint32_t ackSeq;
	uint8_t ackType;
};

#pragma pack(pop)