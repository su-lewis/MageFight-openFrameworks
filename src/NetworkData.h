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
	PKT_CLIENT_READY, // Client -> Host: client finished local setup and is ready to start
	PKT_CHECKSUM_CHECK,

	// Turn-starts are delivered via deterministic commands
	PKT_KEY_PICKUP, // Host -> Client: a player picked up a key (trigger in-game draft)
	PKT_CHAT_MESSAGE, // Chat message between players
	PKT_HOVER, // Hover state update for showing opponent's hover
	PKT_INPUT_COMMAND, // Client/Host: deterministic input command (replaces async ActionPacket)
	// PKT_DRAW_CARDS removed: drawing is now triggered by lockstep commands
	PKT_SNAPSHOT_REQUEST, // Client -> Host: request authoritative snapshot from host
	PKT_SNAPSHOT_BEGIN, // Host -> Client: begin state snapshot
	PKT_SNAPSHOT_CHUNK, // Host -> Client: snapshot data chunk
	PKT_SNAPSHOT_END, // Host -> Client: end state snapshot
	PKT_MENU_STATE, // Menu open/close/hover state for choice-based cards
	// PKT_AMNESIA_CHOICE removed: handled by `CMD_MENU_CHOICE`/lockstep
	// PKT_RENEWED_INSPIRATION removed: handled by `CMD_RENEWED_INSPIRATION`
	PKT_PLACE_SUMMONED_MINION, // Host -> Client: inform clients a summoned minion was placed
	PKT_MOVE_UNIT, // Host/Client: unit movement (fromX,fromY -> toX,toY)
	PKT_PLACE_SUMMONED_BEGIN, // Host -> Client: begin remote placement preview (e.g., Kobolds/Wolves)
	// PKT_EARTHQUAKE_BEGIN removed: earthquake is deterministic in executeCardByType

	// Draft packets moved to the end to avoid enum collisions with legacy packet numbers.
	PKT_DRAFT_ACTION, // Draft selection / accept messages (sent by clients to host)
	PKT_DRAFT_ACK, // Host -> Client: explicit ack for client-sent draft actions
	PKT_DRAFT_STATE, // Host -> Client: draft state update (class, stage, player)
	PKT_DRAFT_OPTIONS, // Host -> Client: authoritative indices for options

	PKT_ACK // Acknowledge receipt of a reliable packet
};

struct PacketHeader {
	uint8_t type; // PacketType
	uint32_t playerID; // Who sent this?
	uint32_t seq; // Sequence number (monotonic per sender)
};
static_assert(sizeof(PacketHeader) == 9, "PacketHeader has unexpected size (cross-platform packing issue)");

// TurnStartPacket removed; deterministic lockstep uses `InputCommandPacket` to
// publish authoritative turn-start and AP results. Do not reintroduce legacy
// TurnStartPacket or rely on host TurnStart packets for gameplay logic.

struct HandshakePacket : PacketHeader {
	uint32_t seed; // The RNG seed (Host generates, Client receives)
};

// Host -> Client: inform clients when a summoned minion is placed (manual placement like Kobolds/Wolves)
struct PlaceSummonedMinionPacket : PacketHeader {
	uint8_t minionType; // 1=KOBOLD, 2=WOLF, ...
	int32_t ownerPlayerID; // playerID of the summoner
	int32_t targetX;
	int32_t targetY;
	int32_t minionHP; // rolled HP for the summoned minion (host authoritative)
	int32_t minionAP; // rolled AP / bonus AP for the summoned minion
};

// Host -> Client: notify clients that a manual placement sequence is beginning
struct PlaceSummonedBeginPacket : PacketHeader {
	uint8_t minionType; // 1=KOBOLD, 2=WOLF
	int32_t ownerPlayerID;
	int32_t sourceX;
	int32_t sourceY;
	int32_t numToPlace;
};

// CardActionBegin and DiceRoll packets removed: deterministic lockstep uses
// `InputCommandPacket` and the authoritative effect pipeline instead of
// broadcasting these visual-only packets.

struct AppliedDamagePacket : PacketHeader {
	int32_t targetPlayerIndex; // who takes damage
	int32_t damageAmount; // how much damage
	uint8_t damageType; // DamageType enum value
	int32_t attackerIndex; // who dealt it
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


// ============================================================================
// LOCKSTEP DETERMINISTIC INPUT SYSTEM
// ============================================================================

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
	CMD_RESOLVE_DICE = 11, // New: Host packs per-die faces into InputCommandPacket.stringData
	CMD_ACCEPT_DRAFT = 12,
	// CMD_ROLL_DICE fully removed: dice resolved deterministically at decision time
};

// Canonical deterministic input packet - replaces ActionPacket for lockstep
struct InputCommandPacket : PacketHeader {
	uint32_t commandId; // Global monotonic command ID for ordering
	uint32_t turnNumber; // Turn when command was issued
	uint8_t commandType; // InputCommandType enum
	int32_t params[8]; // Generic parameter array for all command types
	char stringData[64]; // For card names or labels
	uint32_t clientActionID; // Client-local ID for ACK matching

	// Parameter layouts by command type:
	// CMD_PLAY_CARD: params[0]=cardIndex, params[1]=targetX, params[2]=targetY, params[3]=menuChoice, stringData=cardName
	// CMD_MOVE_UNIT: params[0]=fromX, params[1]=fromY, params[2]=toX, params[3]=toY
	// CMD_DRAW_CARDS: params[0]=playerIndex, params[1]=numCards
	// CMD_MENU_CHOICE: params[0]=menuType, params[1]=targetIndex, params[2]=choice, params[3]=cardIndex
	// CMD_END_TURN: no params
	// CMD_DRAFT_ACTION: params[0]=actionType, params[1]=optionIndex, params[2]=draftPlayerIdx, params[3]=classTier
	// CMD_ASSISTANT_REROLL: params[0]=assistantIndex, params[1]=numDice, params[2]=diceSides
	// CMD_RENEWED_INSPIRATION: params[0]=playerIndex, params[1]=count, stringData contains concatenated card names
	// CMD_PSEUDO_ACTION: params[0]=targetX, params[1]=targetY, stringData=actionName (e.g. "Shell Spike")
	// CMD_STATUS_ACTION: params[0]=cardIndex, params[1]=targetX, params[2]=targetY, params[3]=statusIndex, params[4]=cost, stringData=cardName
};

static_assert(sizeof(InputCommandPacket) <= 128, "InputCommandPacket too large for efficient networking");

// For drafting actions (selecting a card, accepting the draft)
// NOTE: `clientActionID` is a client-local monotonic id that the host will
// echo back in `PKT_DRAFT_ACK` so clients can reliably match ACKs.
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
	uint32_t clientActionID; // client-local id for reliable ACK matching
};

// Compile-time sanity checks for packet sizes to catch cross-platform packing issues
static_assert(sizeof(DraftActionPacket) == 43, "DraftActionPacket size mismatch - packing/fields may be incorrect");

// Host -> Client: Simple draft state update (no indices, just state)
struct DraftStatePacket : PacketHeader {
	int32_t classTier; // 1,2,3 or 0 when not drafting
	int32_t draftPlayerIdx; // which player is currently drafting (host-side index)
	int32_t draftPlayerID; // playerID of the drafting player (for client mapping)
	int32_t picksRemaining; // how many picks left this stage
	int32_t draftStage; // 0 = class1, 1 = class2, etc
	uint8_t isInGameDraft; // 1 = in-game key draft, 0 = normal
	int32_t currentPlayerIndex; // Host tells clients who the active player is when drafting ends
};

// Host -> Client: send the indices in the pool for the options shown
struct DraftOptionsPacket : PacketHeader {
	int32_t classTier; // 1,2,3
	int32_t draftPlayerIdx; // which player is currently drafting (host-side index)
	int32_t draftPlayerID; // playerID of the drafting player (for client mapping)
	int32_t picksRemaining; // how many picks left for this stage
	int32_t draftStage; // 0 = class1, 1 = class2, etc
	uint8_t isInGameDraft; // 1 = in-game key draft, 0 = normal
	uint32_t draftGenCounter; // The draft generation counter value used by host
	uint32_t mapSeed; // The map/game seed used for deterministic draft
	// Optional: explicit pool indices for the three option slots. If set to -1,
	// clients should fall back to deterministic generation using mapSeed/draftGenCounter.
	int32_t optionIdx0;
	int32_t optionIdx1;
	int32_t optionIdx2;
};

// ------------------------- NEW: Draft ACK -------------------------
// Host -> Client: explicit acknowledgement for client-sent DraftAction
struct DraftAckPacket : PacketHeader {
	uint32_t clientActionID; // echoes client-generated id for matching
	uint8_t actionType; // echoed actionType (0=tgl,1=accept)
	int32_t optionIndex; // echoed optionIndex (if applicable)
	int32_t draftPlayerIdx; // echoed draft player index
	uint8_t selectFlag; // echoed selectFlag
	uint8_t numSelected; // echoed numSelected (for Accept)
	int32_t selectedIdx0; // echoed indices
	int32_t selectedIdx1;
	int32_t selectedIdx2;
};

// DraftAckPacket layout: PacketHeader (9) + payload (27) == 36 bytes when packed
static_assert(sizeof(DraftAckPacket) == 36, "DraftAckPacket size mismatch - packing/fields may be incorrect");

// ShufflePacket removed: decks are shuffled deterministically locally.

// Client -> Host: signal that the client has finished local setup and is ready
struct ClientReadyPacket : PacketHeader {
	uint8_t ready; // set to 1
};

static_assert(sizeof(ClientReadyPacket) == 10, "ClientReadyPacket size mismatch - packing/fields may be incorrect");

// Legacy async packet structs removed: lockstep `InputCommandPacket` and
// deterministic effect pipeline provide canonical handling for these effects.

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
	int32_t playerID; // Stable playerID of the picking player (helps client map actor index)
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

// Client -> Host: Request the authoritative snapshot from the host
struct SnapshotRequestPacket : PacketHeader {
	uint32_t requestedTurn; // optional: turn number client expects
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