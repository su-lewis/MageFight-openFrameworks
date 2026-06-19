#pragma once
#include <cstdint>
#include <cstring>

// Cross-platform packet layout (1-byte alignment, no padding)
// All data types are platform-independent:
// - uint8_t, uint32_t, int32_t, int64_t (fixed-size integers)
// - char arrays (already packed)
// Both Linux and Windows use little-endian, so no byte-swap needed.

#pragma pack(push, 1)

enum PacketType {
	PKT_HANDSHAKE,
	PKT_CLIENT_READY,
	PKT_CHECKSUM_CHECK,

	PKT_KEY_PICKUP,
	PKT_CHAT_MESSAGE,
	PKT_HOVER,
	PKT_INPUT_COMMAND,
	PKT_SNAPSHOT_REQUEST,
	PKT_SNAPSHOT_BEGIN,
	PKT_SNAPSHOT_CHUNK,
	PKT_SNAPSHOT_END,
	PKT_MENU_STATE,
	PKT_PLACE_SUMMONED_MINION,
	PKT_MOVE_UNIT,
	PKT_PLACE_SUMMONED_BEGIN,

	PKT_DRAFT_ACTION,
	PKT_DRAFT_ACK,
	PKT_DRAFT_STATE,
	PKT_DRAFT_OPTIONS,

	PKT_ACK
};

// --- Define the Header as a Macro to eliminate C++ Inheritance padding issues ---
#define PACKET_HEADER  \
	uint8_t type;      \
	uint32_t playerID; \
	uint32_t seq;

struct PacketHeader {
	PACKET_HEADER
};

struct HandshakePacket {
	PACKET_HEADER
	uint32_t seed;
	int32_t elo;
};

struct PlaceSummonedMinionPacket {
	PACKET_HEADER
	uint8_t minionType;
	int32_t ownerPlayerID;
	int32_t targetX;
	int32_t targetY;
	int32_t minionHP;
	int32_t minionAP;
};

struct PlaceSummonedBeginPacket {
	PACKET_HEADER
	uint8_t minionType;
	int32_t ownerPlayerID;
	int32_t sourceX;
	int32_t sourceY;
	int32_t numToPlace;
};

struct AppliedDamagePacket {
	PACKET_HEADER
	int32_t targetPlayerIndex;
	int32_t damageAmount;
	uint8_t damageType;
	int32_t attackerIndex;
};

struct ChecksumPacket {
	PACKET_HEADER
	int64_t checksum;
	int32_t turnNumber;
};

struct MoveUnitPacket {
	PACKET_HEADER
	int32_t fromX;
	int32_t fromY;
	int32_t toX;
	int32_t toY;
};

struct AssistantRerollPacket {
	PACKET_HEADER
	int32_t assistantIndex;
	int32_t rerollNumDice;
	int32_t rerollDiceSides;
};

struct MenuStatePacket {
	PACKET_HEADER
	int32_t menuType;
	int32_t targetIndex;
	int32_t hoveredChoice;
	int32_t cardIndex;
};

// --- ADD THIS ENUM BLOCK BACK IN ---
enum InputCommandType : uint8_t {
	CMD_NONE = 0,
	CMD_PLAY_CARD = 1,
	CMD_MOVE_UNIT = 2,
	CMD_DRAW_CARDS = 3,
	CMD_MENU_CHOICE = 4,
	CMD_END_TURN = 5,
	CMD_DRAFT_ACTION = 6,
	CMD_ASSISTANT_REROLL = 7,
	CMD_RENEWED_INSPIRATION = 8,
	CMD_PSEUDO_ACTION = 9,
	CMD_STATUS_ACTION = 10,
	CMD_RESOLVE_DICE = 11,
	CMD_ACCEPT_DRAFT = 12
};
// ------------------------------------

struct InputCommandPacket {
	PACKET_HEADER
	uint32_t commandId;
	uint32_t turnNumber;
	uint8_t commandType;
	int32_t params[8];
	char stringData[64];
	uint32_t clientActionID;
};

struct DraftActionPacket {
	PACKET_HEADER
	uint8_t actionType;
	uint8_t selectFlag;
	int32_t optionIndex;
	int32_t draftPlayerIdx;
	int32_t classTier;
	int32_t numSelected;
	int32_t selectedIdx0;
	int32_t selectedIdx1;
	int32_t selectedIdx2;
	uint32_t clientActionID;
};

struct DraftStatePacket {
	PACKET_HEADER
	int32_t classTier;
	int32_t draftPlayerIdx;
	int32_t draftPlayerID;
	int32_t picksRemaining;
	int32_t draftStage;
	uint8_t isInGameDraft;
	int32_t currentPlayerIndex;
};

struct DraftOptionsPacket {
	PACKET_HEADER
	int32_t classTier;
	int32_t draftPlayerIdx;
	int32_t draftPlayerID;
	int32_t picksRemaining;
	int32_t draftStage;
	uint8_t isInGameDraft;
	uint32_t draftGenCounter;
	uint32_t mapSeed;
	int32_t optionIdx0;
	int32_t optionIdx1;
	int32_t optionIdx2;
};

struct DraftAckPacket {
	PACKET_HEADER
	uint32_t clientActionID;
	uint8_t actionType;
	int32_t optionIndex;
	int32_t draftPlayerIdx;
	uint8_t selectFlag;
	uint8_t numSelected;
	int32_t selectedIdx0;
	int32_t selectedIdx1;
	int32_t selectedIdx2;
};

struct ClientReadyPacket {
	PACKET_HEADER
	uint8_t ready;
};

struct MagicBlastChoicePacket {
	PACKET_HEADER
	int32_t targetPlayerIndex;
	uint8_t choiceType;
};

struct DoubleHandedChoicePacket {
	PACKET_HEADER
	int32_t targetPlayerIndex;
	uint8_t choiceType;
};

struct BurstChoiceAndTargetPacket {
	PACKET_HEADER
	uint8_t choiceType;
	int32_t targetX;
	int32_t targetY;
};

struct DispelChoicePacket {
	PACKET_HEADER
	uint8_t choiceType;
};

struct DispelTargetAndStatusPacket {
	PACKET_HEADER
	int32_t targetPlayerIndex;
	uint8_t statusIndex;
};

struct WisdomBoonChoicePacket {
	PACKET_HEADER
	int32_t targetPlayerIndex;
	uint8_t choiceType;
};

struct TrainChoicePacket {
	PACKET_HEADER
	uint8_t choiceType;
};

struct MagicHandChoicePacket {
	PACKET_HEADER
	int32_t targetWallX;
	int32_t targetWallY;
	uint8_t choiceType;
};

struct RenewedInspireDiscardsPacket {
	PACKET_HEADER
	uint8_t numDiscards;
	int32_t discardedHandIndices[8];
};

struct KeyPickupPacket {
	PACKET_HEADER
	int32_t playerIndex;
	int32_t pickingPlayerID; // <--- Renamed to avoid collision with the Header!
	uint8_t classTier;
	int32_t keyX;
	int32_t keyY;
};

struct ChatMessagePacket {
	PACKET_HEADER
	char message[256];
};

struct HoverPacket {
	PACKET_HEADER
	uint8_t hoverType;
	int8_t gridX;
	int8_t gridY;
	int8_t cardIndex;
};

struct SnapshotBeginPacket {
	PACKET_HEADER
	uint32_t snapshotId;
	uint32_t totalSize;
};

struct SnapshotRequestPacket {
	PACKET_HEADER
	uint32_t requestedTurn;
};

struct SnapshotChunkPacket {
	PACKET_HEADER
	uint32_t snapshotId;
	uint32_t offset;
	uint16_t chunkSize;
	char data[512];
};

struct SnapshotEndPacket {
	PACKET_HEADER
	uint32_t snapshotId;
};

struct AckPacket {
	PACKET_HEADER
	uint32_t ackSeq;
	uint8_t ackType;
};

#pragma pack(pop)

// Cross-platform platform identification
#if defined(_WIN32) || defined(_WIN64)
	#define MAGEFIGHT_PLATFORM "Windows"
#elif defined(__linux__)
	#define MAGEFIGHT_PLATFORM "Linux"
#elif defined(__APPLE__)
	#define MAGEFIGHT_PLATFORM "macOS"
#else
	#define MAGEFIGHT_PLATFORM "Unknown"
#endif