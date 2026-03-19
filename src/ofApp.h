#pragma once
#define GLFW_INCLUDE_NONE

#include "GLFW/glfw3.h"
#include "NetworkData.h"
#include "SteamManager.h"
#include "ofMain.h"
#include "ofxAssimpModelLoader.h"

// --- Standard Library Includes ---
#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// --- GLM Extensions (Required for Quaternions & Intersections) ---
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/quaternion.hpp>

// =================================================================================================
//                                      CONSTANTS & ENUMS
// =================================================================================================

#define BOARD_WIDTH 13
#define BOARD_HEIGHT 9
// TILE_SIZE is defined as a const member in the class

enum GameState {
	STATE_MAIN_MENU,
	STATE_SETTINGS,
	STATE_GAMEPLAY,
	STATE_PAUSED,
	STATE_INITIATIVE_ROLL, // <--- New
	STATE_DRAFTING, // <--- New
	STATE_SINGLEPLAYER_MENU,
	STATE_SAVE_BROWSER,
	STATE_DESYNC // Desync detected; abort match
};

enum PlayerActionState {
	NONE,
	PIECE_SELECTED,
	PIECE_DRAGGING
};

enum GameplayState {
	AWAITING_INPUT,
	ANIMATING,
	WAITING_FOR_DICE,
	MODAL_CHOICE,
};

enum CardPlayResult {
	CARD_PLAYED_IMMEDIATELY, // Send packet NOW in mouseReleased
	CARD_AWAITING_MENU_CHOICE, // Menu will send packet after user chooses
	CARD_AWAITING_TARGETING, // Targeting handler will send packet after player targets
	CARD_CANCELLED, // User cancelled, don't send packet
	CARD_NOT_PLAYABLE, // Cost/validation failed, don't send packet
	CARD_AWAITING_DICE // Dice will trigger packet when resolved
};

enum DamageType {
	DAMAGE_PHYSICAL,
	DAMAGE_PIERCING,
	DAMAGE_MAGIC,
	DAMAGE_ELECTRIC,
	DAMAGE_FIRE,
	DAMAGE_HOLY,
	DAMAGE_POISON
};

enum TargetingType {
	TARGET_NONE,
	TARGET_EMPTY_TILE,
	TARGET_ANY_TILE,
	TARGET_WALL,
	TARGET_ADJACENT_UNIT,
	TARGET_SELF,
	TARGET_LINEAR_PIERCE,
	TARGET_CLEAVE_ADJACENT,
	TARGET_ADJACENT_OR_SELF_UNIT,
	TARGET_LINE_OF_SIGHT_TILE,
	TARGET_BURST_AREA,
	TARGET_ADJACENT_UNIT_OR_WALL,
	TARGET_EMPTY_ADJACENT,
	TARGET_ADJACENT_WALL
};

enum CardType {
	CARD_NONE,
	CARD_MOVE,
	CARD_CREATE_WALL,
	CARD_FORTIFY,
	CARD_VAMPIRE_BITE,
	CARD_ATTACK_SINGLE_TILE,
	CARD_ATTACK_AREA,
	CARD_DESTROY_WALL,
	CARD_GAIN_AP,
	CARD_GAIN_BLOCK,
	CARD_GAIN_WARD,
	CARD_MAGIC_BLAST,
	CARD_FIREBALL,
	CARD_SHOCK,
	CARD_ROCK_CRUSH,
	CARD_DEMOLITION,
	CARD_MIND_THEFT,
	CARD_AMNESIA,
	CARD_DISPEL,
	CARD_TELEPORT,
	CARD_HASTEN,
	CARD_REPLICATE,
	CARD_WISDOM_BOON,
	CARD_ETHEREAL_JOLT,
	CARD_FLAME_HIT,
	CARD_HEAL,
	CARD_RAISE_DEAD,
	CARD_SUMMON_GOLEM,
	CARD_STRENGTHEN_ELEMENTS,
	CARD_DARK_SHIELD,
	CARD_DRAIN_PUNCH,
	CARD_DOUBLE_HANDED,
	CARD_CALL_FOR_WOLVES,
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
	CARD_CALL_FOR_KOBOLDS,
	CARD_RENEWED_INSPIRATION,
	CARD_SPARK_OF_GENIUS,
	CARD_PSIONIC_WAVE,
	CARD_EARTHQUAKE,
	CARD_FORM_OF_GHOST,
	CARD_GIANT_MAGIC_HAND,
	CARD_CONSUME_LARGE_HEALTH_POTION,
	CARD_LESSER_HEAL,
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
	CARD_PUNCH,
	CARD_KICK,
	CARD_HAND_BLOCK,
	CARD_BASH,
	CARD_WARD,
	CARD_STAB,
	CARD_SLASH,
	CARD_SUMMON_WALL,
	CARD_SUMMON_MAGIC_WALL
};

enum DicePurpose {
	PURPOSE_AP,
	PURPOSE_DAMAGE,
	PURPOSE_RANGE,
	PURPOSE_COIN_FLIP,
	PURPOSE_DEBUG,
	PURPOSE_BARRIER_GAIN,
	PURPOSE_HP,
	PURPOSE_HEALING,
	PURPOSE_BONUS_AP,
	PURPOSE_TIME_VORTEX,
	PURPOSE_DEATH_CHECK,
	PURPOSE_SLEEP_DURATION,
	PURPOSE_SUMMON_KOBOLDS,
	PURPOSE_SUMMON,
	PURPOSE_SPARK_OF_GENIUS_DRAW,
	PURPOSE_PSIONIC_WAVE_RANGE,
	PURPOSE_PSIONIC_WAVE_AMOUNT,
	PURPOSE_TELEPORT_RANGE,
	PURPOSE_EARTHQUAKE_DISTANCE,
	PURPOSE_EARTHQUAKE_DAMAGE,
	PURPOSE_MAGIC_HAND_DAMAGE,
	PURPOSE_LESSER_HEAL,
	PURPOSE_BLOCKING_BOON_COIN, // <--- Add
	PURPOSE_BLOCKING_BOON_D20
};

// Unified card interaction state machine (replaces per-card isTargeting*/is*MenuOpen flags)
enum CardInteractionState {
	CARD_INTERACTION_IDLE, // No card interaction in progress
	CARD_INTERACTION_TARGETING, // Waiting for player to click target on board
	CARD_INTERACTION_MENU, // Waiting for player to choose menu option
	CARD_INTERACTION_STATUS, // Waiting for status selection (Dispel only)
	CARD_INTERACTION_PLACING // Waiting for placement click (Wolves, Kobolds)
};

enum TargetValidity {
	VALID,
	INVALID_OUT_OF_RANGE,
	INVALID_NO_LOS,
	INVALID_HARD_COVER,
	INVALID_CHOKE_POINT,
	INVALID_OCCUPIED_BY_WALL,
	INVALID_SELF
};

enum PileViewMode {
	VIEW_NONE,
	VIEW_DECK,
	VIEW_DISCARD
};

enum CursorState {
	CURSOR_DEFAULT, // Standard arrow
	CURSOR_CLICK, // Pointing finger (hovering buttons/cards)
	CURSOR_GRAB, // Open hand (hovering draggable items)
	CURSOR_HOLD // Closed fist (actively dragging)
};

// =================================================================================================
//                                          STRUCTS
// =================================================================================================

struct TargetInfo {
	bool isTargetable = false;
	TargetValidity reason = INVALID_OUT_OF_RANGE;
};

struct DiceRoll {
	DicePurpose purpose;
	int sides = 6;
	int result = 0; // final result (includes luck when applicable)
	int rawResult = 0; // the raw die value before luck is added (used for visual face)
	bool isFinishedVisual = false;
	float startTime = 0;
	glm::quat finalQuat;
	float currentRotation = 0;
	glm::vec3 rotationAxis;
	int associatedUnit = -1; // optional: which unit this roll belongs to (earthquake)
};

struct Card {
	std::string name;
	ofRectangle textureRect;
	float currentScale = 1.0f;
	float targetScale = 1.0f;
	ofVec2f currentPos;
	ofVec2f targetPos;
	CardType type = CARD_NONE;
	int value = 0;
	int numDice = 0;
	int diceSides = 0;
	DamageType damageType = DAMAGE_PHYSICAL;
	int cost = 1;
	TargetingType targeting = TARGET_ANY_TILE;
	bool drawnThisTurn = false; // Add this
	bool isAnimating = false; // True while a visual-only animation is running for this card
	bool isCopied = false;
	int cardClass = 1;
};

// ===================================================================================================
// EFFECT QUEUE SYSTEM - centralized card execution pipeline
// ===================================================================================================
// ============================================================================
// DATA-ORIENTED DETERMINISTIC EFFECT SYSTEM
// ============================================================================

enum class EffectOpType : uint8_t {
	NONE = 0,
	ROLL_DICE,
	DAMAGE,
	HEAL,
	MOVE_UNIT,
	CREATE_WALL,
	SPAWN_UNIT,
	SPAWN_PLAYER,
	MODIFY_STAT,
	MODIFY_TILE,
	ADD_CARD_TO_DECK,
	RESHUFFLE_DISCARD_TO_DECK,
	REMOVE_TOP_CARD_FROM_DECK,
	DRAW_CARDS,
	DISCARD_CARDS,
	APPLY_STATUS,
	REMOVE_STATUS, // Added REMOVE_STATUS for effect operations
	CONDITIONAL_BRANCH,
	APPLY_FIREBALL,
	APPLY_POISON,
	APPLY_AMNESIA,
	APPLY_DEATH,
	APPLY_ON_FIRE,
	APPLY_PARALYSIS,
	APPLY_WOLF_COIN,
	APPLY_BLOCKING_BOON_COIN,
	APPLY_BLOCKING_BOON_D20,
	APPLY_INITIATIVE_REROLL,
	APPLY_SLEEP_DURATION,
	APPLY_MAGIC_BLAST,
	APPLY_MAGIC_BOLT,
	APPLY_MAGIC_BOLT_PRIMARY,
	APPLY_MAGIC_BOLT_AOE,
	APPLY_TIME_VORTEX,
	APPLY_ATTACK,
	APPLY_PSIONIC_WAVE,
	APPLY_TELEPORT,
	APPLY_SHOOT_ARROW,
	APPLY_SHOOT_ARROW_DAMAGE,
	APPLY_CHAIN_LIGHTNING,
	APPLY_CHAIN_LIGHTNING_DAMAGE,
	APPLY_MAGIC_HAND_DAMAGE,
	APPLY_EARTHQUAKE,
	WAIT_VISUAL
};

struct RollDiceData {
	int numDice;
	int sides;
	DicePurpose purpose;
	int ownerIndex;
	int outputSlot; // Where to store result in blackboard
	char label[32];
};

struct DamageData {
	int targetIndex;
	DamageType damageType;
	int fixedDamage;
	int damageFromSlot; // -1 = use fixedDamage, else read from blackboard
};

struct HealData {
	int targetIndex;
	int amount;
	int amountFromSlot; // -1 = use fixed amount
};

struct MoveUnitData {
	int unitIndex;
	int toX;
	int toY;
};

struct DrawCardsData {
	int playerIndex;
	int numCards;
};

struct CreateWallData {
	int x;
	int y;
	bool isMagic;
};

struct ModifyStatData {
	int targetIndex;
	int statType; // 0=HP, 1=MaxHP, 2=AP, 3=MaxAP, 4=Armor, etc
	int delta;
	int deltaFromSlot; // -1 = use fixed delta
};

struct SpawnUnitData {
	int toX;
	int toY;
	int summonKind; // PENDING_SUMMON_* value
	int ownerPlayerID;
	int maxHealth;
	int maxHealthFromSlot; // if >=0, read authoritative HP from EffectSequence.blackboard[slot]
	int ap;
	int summonerPlayerID; // optional: specific unit/player that summoned this minion
};

struct ModifyTileData {
	int toX;
	int toY;
	int setHasWall; // 0 = no change, 1 = set true, -1 = set false
};

struct AddCardToDeckData {
	int targetIndex;
	int cardType; // CardType enum value
};

struct RemoveTopCardData {
	int targetIndex;
};

struct ReshuffleDiscardData {
	int targetIndex;
};

struct SpawnPlayerData {
	int x;
	int y;
	int playerID;
	int deckChoice; // 0=empty, 1=allCards, 2=debugSavedDeckForPlayerID
};

enum StatusType : int {
	STATUS_NONE = 0,
	STATUS_ADD_POISON = 1,
	STATUS_STRENGTHEN_ELEMENTS = 2,
	STATUS_POISONED = 3,
	STATUS_ON_FIRE = 4,
	STATUS_PARALYZED = 5,
	STATUS_GHOST_FORM = 6,
	STATUS_TORTOISE_FORM = 7,
	STATUS_REGENERATING = 8,
	STATUS_REPLICATE_QUEUED = 9,
	// New centralized status flags (used by EffectOps)
	STATUS_NEXT_TURN_EXTRA_DRAW = 10,
	STATUS_NEXT_TURN_D10AP = 11,
	STATUS_NEXT_TURN_BONUS_DICE = 12,
	STATUS_SLEEP = 13,
};

struct StatusData {
	int targetIndex;
	int statusType;
	int duration; // optional
};

struct EffectOp {
	EffectOpType type;
	union {
		RollDiceData rollDice;
		DamageData damage;
		HealData heal;
		MoveUnitData moveUnit;
		DrawCardsData drawCards;
		ModifyStatData modifyStat;
		ModifyTileData modifyTile;
		AddCardToDeckData addCard;
		RemoveTopCardData removeTopCard;
		ReshuffleDiscardData reshuffle;
		StatusData status;
		SpawnUnitData spawnUnit;
		CreateWallData createWall;
		SpawnPlayerData spawnPlayer;
	} data;

	// Visual wait state (not serialized to network, computed locally)
	bool visualStarted = false;
	int visualFrame = 0;
};

struct EffectSequence {
	std::vector<EffectOp> ops;
	size_t currentOp = 0;
	int blackboard[16] = { 0 }; // Shared state between effects
	bool isComplete = false;
};

// Visual-only event queue (decouples visuals from deterministic simulation)
enum VisualEventType {
	VE_NONE = 0,
	VE_WAIT = 1, // generic wait (duration)
	VE_DICE = 2, // visual dice animation (associated with purpose/player)
	VE_TRACER = 3, // tracer line visual (ranged spells)
	VE_CUSTOM = 99
};

struct VisualEvent {
	VisualEventType type = VE_NONE;
	int targetIndex = -1; // player index or other target
	int purpose = 0; // optional purpose (e.g., dice purpose)
	float startTime = 0.0f;
	float duration = 0.0f; // seconds
	bool completed = false;
	// Extended payloads for common visual types
	// Dice visual
	int diceNum = 0;
	int diceSides = 6;
	int diceResult = 0; // deterministic total result (for display)
	int dicePurpose = 0; // DicePurpose
	std::vector<int> diceRawResults; // per-die raw faces (for visual spinner)
	bool visualStarted = false; // whether the visual spinner has been started
	bool textSpawned = false; // whether the result text has been spawned

	// Tracer / positional visuals
	glm::vec3 startPos = { 0.0f, 0.0f, 0.0f };
	glm::vec3 endPos = { 0.0f, 0.0f, 0.0f };
	ofColor color = ofColor::white;

	// Floating text
	std::string text = "";
	float textXOffset = 0.0f;
	bool spawned = false; // internal: whether visual has been spawned
};

// ===================================================================================================
// CARD STATE SYSTEM - Unified state machine for all 70 cards
// ===================================================================================================
// All cards flow through states: IDLE -> MENU -> TARGETING -> EFFECT -> OUTCOME -> PACKET -> IDLE
enum CardPlayState {
	CARD_STATE_IDLE = 0, // No card action in progress
	CARD_STATE_MENU = 1, // Waiting for user menu choice (Wisdom Boon, Dispel, etc)
	CARD_STATE_TARGETING = 2, // Waiting for user to select target
	CARD_STATE_DICE = 3, // Waiting for dice roll result
	CARD_STATE_EFFECT = 4, // Effect is being applied (animation/movement)
	CARD_STATE_EFFECT_SEQUENCE = 5, // Processing lockstep effect sequence
	CARD_STATE_OUTCOME = 6, // Effect complete, ready to send outcome packet
	CARD_STATE_FINISHED = 7 // Card action finished, sent to network
};

// Unified structure for all card outcomes (instead of scattered pending* variables)
struct CardOutcome {
	CardType cardType = CARD_NONE;
	int cardIndex = -1; // Index in hand that played this card
	int casterIndex = -1; // Who cast it

	// Menu choices
	std::string menuChoice = ""; // For Wisdom Boon (damage/block), Dispel (barrier/purge), etc

	// Targeting
	glm::ivec2 primaryTarget = { -1, -1 }; // Main target tile
	std::vector<glm::ivec2> secondaryTargets; // AOE, chain lightning chains, etc
	int targetPlayerIndex = -1; // If targeting a specific player
	std::vector<int> targetedPlayers; // For effects hitting multiple players

	// Dice results (stored as they complete)
	std::vector<int> diceResults; // All dice rolls for this card (damage, healing, etc)
	std::map<std::string, int> namedDiceResults; // Purpose -> result mapping (e.g. "barrier_gain" -> 15)

	// Effect outcomes (computed from dice/choices)
	int damageDealt = 0;
	int healingDealt = 0;
	int apGained = 0;
	int blockGained = 0;
	int barierGained = 0;
	std::vector<std::string> statusesApplied; // "onFire", "isParalyzed", etc
	std::vector<int> unitsMovedBy; // Distance each unit moved for earthquake, etc
	std::vector<glm::ivec2> wallsCreated; // Positions where walls were built
	std::vector<int> minionsSpawned; // Indices of newly spawned minions

	// For cards that destroy a card as part of resolution (e.g. Shoot Arrow)
	CardType destroyedCardType = CARD_NONE;

	// Attack-specific outcome data
	DamageType attackDamageType = DAMAGE_PHYSICAL;
	std::vector<int> attackTargetIndices; // indices into `players`

	// Poison targets stored as stable playerIDs for later resolution
	std::vector<int> poisonTargetPlayerIDs;

	// Summon kind for summon cards (mapped from PendingSummonKind)
	int summonKind = 0;

	// Owner playerID for summoned minions (captured at play-time)
	int summonOwnerPlayerID = -1;

	// State tracking
	bool isComplete = false; // Ready to send outcome packet
	CardPlayState currentState = CARD_STATE_IDLE;
};

struct PlayedCardDisplay {
	Card card;
	float startTime;
	glm::vec2 startPos; // UI screen position where the card appears
	glm::vec2 currentPos; // Current position during animation
	float currentScale = 1.5f; // Starts large, shrinks and fades
	float startScale = 1.5f;
	float currentAlpha = 255.0f; // Starts opaque, fades out
};

// Visual event queue members (declared in ofApp) - removed here; declared inside `ofApp` class

struct StolenCardAnimation {
	Card card;
	float startTime;
	glm::vec3 startPos;
	glm::vec2 targetPos;
	glm::vec2 currentPos;
	float currentScale = 0.1f;
	float currentAlpha = 0.0f;
};

struct PlayedCardAnimation {
	Card card;
	float startTime;
	glm::vec2 pos; // Center of screen
	float currentScale = 2.6f;
	float currentAlpha = 255.0f;
};

struct RemovedCardAnimation {
	Card card;
	glm::vec2 startPos;
	float startTime;
	float currentScale;
	float currentAlpha;
};
// Animation for drawing a card from deck to hand
struct DrawCardAnimation {
	Card card;
	glm::vec3 startPos; // world OR screen coords (z==0 for screen-space)
	glm::vec3 endPos;
	glm::vec2 currentPos;
	glm::vec2 targetPos; // 2D hand position for animation end
	float startTime;
	float duration;
	float currentAlpha = 255.0f;
	float currentScale = 1.0f; // visual scale multiplier used during animation
	int ownerIndex; // Player or minion index
	int ownerPlayerID = -1; // Stable playerID used to resolve owner after reordering
	bool toMinionHand; // True if animating to minion hand
	bool startIsScreenSpace = false; // true when startPos is already screen coordinates
	// If true, do not commit the card into the player's hand when the animation
	// finishes (we added the card to the hand immediately and the animation is
	// purely visual). Default true for existing animations that expect commit.
	bool commitOnFinish = true;
};

struct Tile {
	bool hasPlayer = false;
	bool hasWall = false;
	bool isMagicWall = false; // New: true if this wall is a magic wall
	bool isHighlighted = false;
	bool isTargetable = false;
	bool isTargetPreview = false;
	bool visited = false;
	glm::vec2 parent = { -1, -1 };

	// Target square tooltip info (for cards with range checks)
	int minRollRequired = 0; // Minimum dice roll needed to hit this square
	float hitChance = 0.0f; // Percentage chance to hit (0.0 to 1.0)
	bool hasTooltipInfo = false; // True if tooltip data is valid for this tile
};

struct Player {
	int x;
	int y;
	glm::vec3 visualPos = { 0.0f, 0.0f, 0.0f }; // For smooth animation / snapshot restore
	int health = 15;
	int maxHealth = 15;
	int block = 0;
	int ward = 0;
	int fortification = 0; // Temporary fortify from Fortify card (blocks physical/piercing)
	int barrier = 0;
	int holyBlock = 0;
	int luck = 0;
	int baseLuck = 0; // Base luck from cards/effects (excluding Assistant bonus)
	int bonusTurns = 0;
	int ap = 0; // Action Points for this player
	int playerID = 0;
	float facingAngle = 0.0f; // 0 = North, 90 = East, 180 = South, 270 = West
	bool onFire = false;
	bool hasRegeneration = false;

	// Status & Buffs
	int nextTurnAPBonus = 0;
	int shocksPlayedThisTurn = 0;
	int flurryOfFistsStacks = 0; // Number of Flurry stacks: each stack doubles hand-related effects and drawn-card counts
	bool isParalyzed = false;
	int paralysisHeadsCount = 0;
	bool isPoisoned = false;
	int poisonReduction = 0; // 0 = first turn (full 1d6), then 1, 2, 3, 4, 5, 6 (cured)
	bool nextAttackAddPoison = false; // Buff from Add Poison card
	bool nextTurnD10AP = false;
	bool nextTurnExtraDraw = false;
	int nextTurnExtraDrawSetOnCycle = -1; // Track when the extra draw flag was set
	bool replicateQueued = false;
	bool nextTurnBonusDiceFromMinions = false;
	int strengthenElementsTurnsRemaining = 0;
	int sleepTurnsRemaining = 0;

	// Turn Logic
	int summonedOnTurnCycle = -1; // -1 means not summoned this cycle, otherwise stores globalTurnCounter
	int summonOrder = 0; // Tracks the order minions were summoned for turn ordering

	// Minion Data
	bool isMinion = false;
	bool isSkeleton = false;
	bool isGolem = false;
	bool isHellhound = false;
	bool isWolf = false;
	bool isKobold = false;
	bool isDemon = false;
	bool isWallUnit = false;
	bool isMagicWallUnit = false;
	bool isKoboldKing = false;
	bool isFaerie = false;

	// --- ASSISTANT VARIABLES ---
	bool isAssistant = false;
	int directSummonerID = -1; // ID of the specific unit that summoned this minion
	bool assistantRerollUsedThisTurn = false; // Track the "once per turn" usage
	int freeKickTurns = 0; // Number of turns (including current) that Kick costs 0

	// Tortoise Form
	bool inTortoiseForm = false;
	int tortoiseDamageTaken = 0; // Tracks HP damage while in form, ends at 5
	Card tortoiseFormCard; // The card to discard when form ends
	std::string originalModelType = ""; // To restore original model
	// Accumulated tortoise pending damage (0 = none). Replaces prior
	// `pendingTortoiseDamage` (bool) + `pendingTortoiseDamageValue` (int).
	int tortoiseAccumulatedDamage = 0;

	ofTexture * minionTexture = nullptr;
	int ownerID = -1;

	// Ghost Form
	bool inGhostForm = false;
	int ghostDamageTaken = 0;
	Card ghostFormCard;

	// Flag set when this player clicked to enter a wall tile (prevents ending turn)
	// This is NOT set when an external effect (earthquake, push, etc.) places
	// the player inside a wall.
	bool enteredWallByClick = false;

	// Piles
	std::vector<CardType> cardsPlayedThisTurn;
	std::vector<Card> playedCardsPile;
	std::vector<Card> hand;
	std::vector<Card> deck;
	std::vector<Card> discardPile;

	// Track whether this actor (player or minion) has performed their "once-per-turn" draw
	bool hasDrawnThisTurn = false;

	// When true the player's deck has been modified and requires an authoritative shuffle
	// before the next draw to ensure randomness (host will perform/broadcast the shuffle).
	bool deckNeedsShuffle = false;
};

struct MinionUI {
	int playerIndex; // Which player in the main `players` vector this UI represents
	int displayNumber;
	ofRectangle bounds;
	ofRectangle modelViewport;
	ofRectangle healthBar;
	ofRectangle deckRect;
	ofRectangle discardRect;
};

struct DeathMarker {
	int x;
	int y;
	int turnDied;
	std::vector<Card> deck;
};

struct FloatingText {
	std::string text;
	glm::vec3 worldPos; // Current world position (may include offset)
	glm::vec3 anchorPos; // Base anchor position for grouping
	glm::vec3 velocity; // Upward drift
	float startTime;
	float duration = 3.0f; // Increased default lifetime for readability
	ofColor color;
	std::string category; // Optional category (e.g., "dice_AP", "dice_fire")
	float xOffset = 0.0f; // Horizontal offset in world units for side-by-side texts
};

struct Particle {
	glm::vec3 pos;
	glm::vec3 vel;
	float life; // 1.0 = born, 0.0 = dead
	float decay; // How fast it dies (e.g., 0.02)
	float size;
	ofColor color;
};

// =================================================================================================
//                                      MAIN APPLICATION CLASS
// =================================================================================================

class ofApp : public ofBaseApp {

public:
	// Public-facing TargetingContext struct so public methods can reference it
	struct TargetingContext {
		int sourceCardIndex = -1; // index in caster's hand
		int sourcePlayerIndex = -1; // which player's hand the source card belongs to
		TargetingType type = TARGET_NONE;
		// Validate whether a grid tile is acceptable for this targeting context
		std::function<bool(int, int)> isValid = nullptr;
		// Called when a valid target is selected (gx, gy)
		std::function<void(int, int)> onSelected = nullptr;
		// Optional cancel callback
		std::function<void()> onCancel = nullptr;
		// Instruction text to show while targeting
		std::string instruction;
	};
	// --- AUDIO SLIDER DRAG STATE ---
	bool draggingAudioMaster = false;
	bool draggingAudioMenu = false;
	bool draggingAudioSfx = false;
	bool draggingFramerateSlider = false;
	void drawMinionCard(int minionIndex, int ownerIndex);
	void setup();
	void update();
	void draw();

	// Visual event queue (visual-only events processed locally)
	std::vector<VisualEvent> visualEvents;
	void queueVisualEvent(const VisualEvent & e);
	void processVisualEvents();
	// Centralized checker that invokes resolve helpers when visuals complete
	void processWaitingFlags();
	void beginInitiativeDrafting(int winnerIndex);
	void exit();
	// Ensure vtable emission: declare destructor to define out-of-line in cpp
	~ofApp();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);
	void mouseScrolled(int x, int y, float scrollX, float scrollY);

	// Networking logic
	void processNetworkPackets();

	// Networking helpers for Begin/Resolve patterns
	void sendPlaceSummonedBegin(int minionType, int ownerPlayerID, int sourceX, int sourceY, int numToPlace);
	void sendEarthquakeBegin();

	// Generic card action begin helper
	void sendCardActionBegin(int cardType, int actorIndex, int targetX, int targetY, int p0 = 0, int p1 = 0, int p2 = 0, int p3 = 0, const std::string & label = "");
	void sendActionPacket(int cardIndex, int tx, int ty, int cost, int menuChoice = 0, const std::string & cardNameOverride = "");
	void sendMagicHandResolutionPacket(int choice);
	void sendMenuState(int menuType, int targetIndex, int hoveredChoice, int cardIndex);
	void executeAction(const ActionPacket & pkt);
	void executeOpponentCardPlay(const ActionPacket & pkt);

	// Host-side validation for client-submitted ActionPackets. Returns true
	// if the action is valid against the host's authoritative board; if
	// false, `reason` is filled with a short explanation for logging/feedback.
	bool validateActionPacketOnHost(const ActionPacket & pkt, std::string & reason);
	long long calculateChecksum();

	// Reliable send tracking for certain client-originated packets
	bool lastSentRenewedInspirationValid = false;
	RenewedInspirationPacket lastSentRenewedInspirationPacket;
	float lastSentRenewedInspirationTime = 0.0f;
	int lastSentRenewedInspirationAttempts = 0;

	bool lastSentDrawCardsValid = false;
	DrawCardsPacket lastSentDrawCardsPacket;
	float lastSentDrawCardsTime = 0.0f;
	int lastSentDrawCardsAttempts = 0;
	void sendSnapshotToClient();
	std::string buildSnapshotString();
	void applySnapshotString(const std::string & data);

	// Anti-cheat: Log deck states for verification
	void logDeckStates(const std::string & reason);
	std::string getDeckStateString(const Player & p);

	float lastHandshakeRequestTime = 0.0f;
	float handshakeRequestInterval = 1.0f;
	uint32_t lastObservedLobbySeed = 0;
	float lastLobbySeedLogTime = 0.0f;
	uint32_t currentMapSeed = 0;

	// Steam
	SteamManager steamManager;
	bool isMultiplayer = false;
	int myLocalPlayerID = 0; // 0 = Host, 1 = Client
	bool hasReceivedHandshake = false;
	std::string player0SteamName = "Player 1";
	std::string player1SteamName = "Player 2";
	uint32_t lastSnapshotId = 0;
	std::string incomingSnapshotBuffer;
	uint32_t incomingSnapshotId = 0;
	uint32_t incomingSnapshotExpectedSize = 0;
	uint32_t incomingSnapshotReceivedSize = 0;

	// Backup snapshot for desync recovery

	// Turn-start authoritative master backup for perfect rewind resyncs
	std::string turnStartBackupSnapshot;

	uint32_t lastReceivedSeqByPlayer[2] = { 0, 0 };

	// Steam avatar images for turn indicator
	ofImage localAvatarImage;
	ofImage opponentAvatarImage;
	bool localAvatarReady = false;
	bool opponentAvatarReady = false;

	// Camera perspective: Each player sees themselves in bottom-left, opponent in top-right
	bool shouldFlipCamera() const { return isMultiplayer && myLocalPlayerID == 1; }
	ofCamera & getActiveCamera(); // Returns appropriate camera based on player (cam or cam2)
	glm::vec3 transformGridToWorld(int gx, int gy); // Applies camera flip if needed
	glm::ivec2 transformWorldToGrid(glm::vec3 worldPos); // Applies camera flip if needed
	int getVisualPlayerIndex(int actualPlayerIndex); // Converts actual player index to visual (flipped for client)
	// Debug helpers
	bool debugFlatSkeletonDraw = true; // When true, draw a flat unshaded pass to verify visibility
	// When true, force an unshaded textured draw instead of the PBR shader (debug only)
	bool debugForceUnshadedDraw = false;

private:
	// -------------------------------------------------------------------------
	//                              CORE SYSTEMS
	// -------------------------------------------------------------------------

	// Last AP roll parameters (used for assistant auto-reroll)
	int lastAPDiceNum = 0;
	int lastAPDiceSides = 0;
	void setupGame();
	// Start the initiative phase (sets state + spawns initiative dice)
	void startInitiativePhase();

	// Initialize shared game state (board, players, camera)
	void initialiseGameStateCommon();

	// Initialize client-side game from a host-provided seed
	void initGameFromSeed(uint32_t seed);
	void updateGame();
	void drawGame();
	void cleanupGame();

	void startNewTurn();
	void continueNewTurn();

	void drawMainMenu();
	void drawSettingsMenu();
	void drawPauseMenu();
	void applySettings();
	void recalculateUI(int w, int h);
	void updateDebugRects();
	void debugSkipDraftRandomCards();

	// -------------------------------------------------------------------------
	//                              GAMEPLAY LOGIC
	// -------------------------------------------------------------------------
	void drawCard(bool sendPacket = true);
	CardPlayResult playCard(int cardIndex, int targetX, int targetY);

	// === CARD STATE MACHINE HANDLERS ===
	void updateMenuButtonRectangles(); // Update button rectangles for current menu
	void processCardStateInput(int mouseX, int mouseY, int button); // Handle clicks during card states
	void updateCardStateMachine(); // Called in update() to process state transitions
	void advanceCardState(CardPlayState newState); // Transition to new state
	void sendCardOutcomePacket(); // Send unified outcome packet to clients
	void applyCardOutcomeEffects(); // Apply the completed outcome to game state
	void handleCardTargetInput(int gridX, int gridY); // Target selected
	void handleCardDiceResult(int result, DicePurpose purpose); // Dice roll completed
	bool executeCardByType(const Card & playedCard, int cardIndex, int targetX, int targetY, bool & playedSuccessfully, CardPlayResult & immediateResult); // centralized execution entry (incremental migration)

	// --- Async Resolution Helpers (Centralized Dice/State Resolution) ---

	void resolveMagicHandDamage();
	void resolveSummonHealth();
	// Amnesia resolver removed; handled via EffectOpType::APPLY_AMNESIA
	void resolveMagicBlastDice();
	void resolveDeathDice();
	void resolveSleepDuration();
	// New small resolvers for inline dice handling sweep
	void resolveSleepDurationRoll(const DiceRoll & finishedRoll);
	void resolveDeathCheckRoll(const DiceRoll & finishedRoll);
	void resolveEarthquakeDamage(const DiceRoll & finishedRoll);
	void resolveAPRoll();
	// Blocking Boon resolution migrated to effect/op handlers (APPLY_BLOCKING_BOON_*)
	void resolveSummonKobolds(const DiceRoll & finishedRoll);
	void resolveJoltRangeDice();

	void resolveEarthquakeDistance();
	void resolveBonusAP(const DiceRoll & finishedRoll);

	// Earthquake simulation update (migrated from updateGame())
	void updateEarthquakeSimulation();

	// Time Vortex resolution migrated to effect/op pipeline
	void resolveMagicBoltRangeDice();
	void resolveMagicBoltPrimaryDice();
	void resolveMagicBoltAoeDice();
	void resolveShootArrowDice();
	void resolveChainLightningRangeDice();
	void resolveChainLightningDamageDice();
	void resolveFlailDice();
	void resolveSparkOfGeniusDice();
	void resolveBarrierDice();

	void resolveOnFireDice();
	void resolvePoisonStatusDice();
	// Paralysis/Wolf coin resolvers removed; handled via effect ops.

	// Simple inline dice resolver for legacy inline uses (sums N dS)
	int resolveDiceRoll(int numDice, int sides);

	void applyReplicateCopyToHand(Player & caster, const Card & playedCard);
	void finishPlayCard(Player & caster, const Card & playedCard, int handIndex);
	void completeCardPlayAnimation(const Card & playedCard, int playerIndex);
	int applyDamageWithMitigations(Player & target, int baseDamage, DamageType type, int attackerIndex);
	Player createSummonedMinion(CardType type, int targetX, int targetY, const Player & caster, int turnCounter, int & nextSummonID);
	void resolveMenuCardChoice(CardType cardType, int choiceIndex, Player & caster, Player * target);
	void updatePlayerAP(Player & player, int newAP);
	void applyMovement(int playerIndex, int targetX, int targetY, int newAP, const std::vector<glm::vec2> * pathOverride = nullptr);
	void createCardDisplay(const Card & card, int playerIndex); // Create card display animation
	std::string currentDiceLabel = "";
	int startDiceRoll(int numDice, int sides, DicePurpose purpose, std::string label = "", int ownerIndex = -1);
	// Start a purely-visual dice spinner using precomputed raw faces (does not consume gameplay RNG)
	void startVisualDiceRoll(const VisualEvent & ev);
	// Resolve dice but also return raw per-die faces
	int resolveDiceRollDetailed(int numDice, int sides, std::vector<int> & outRaw);

	// Data-oriented effect system
	void beginEffectSequence();
	void queueEffect(const EffectOp & op);
	void updateEffectSequence();
	bool isEffectSequenceComplete() const;

	bool hasFinishedDiceRollFor(DicePurpose purpose, int ownerIndex) const;

	// Return true when all active dice visuals are finished and the result linger time passed
	bool diceVisualsFinishedAndLinger() const;
	void recalcTempLuck();
	void checkKeyPickupAndDraftAfterSummon(int x, int y, int minionOwnerID);

	// Returns passive luck (from assistant auras and cards in deck) for the given player index.
	int computePassiveLuck(int playerIndex);
	void spawnFloatingText(glm::vec3 pos, std::string text, ofColor color, std::string category = "");
	// Queue a floating-text visual event (non-authoritative, does not change game state)
	void queueFloatingTextVisual(glm::vec3 pos, std::string text, ofColor color, float duration = 1.2f);
	// Queue a visual dice roll (non-authoritative). `rawResults` contains per-die face values.
	void queueVisualDiceRoll(glm::vec3 pos, int numDice, int sides, const std::vector<int> & rawResults, int totalResult, int dicePurpose = 0, int ownerIndex = -1, float duration = 1.5f);
	// Queue a tracer visual from world-space start -> end (non-authoritative)
	void queueVisualTracer(glm::vec3 start, glm::vec3 end, ofColor color = ofColor::white, float duration = 1.2f);
	// Queue a simple visual delay/wait
	void queueVisualDelay(float seconds);
	void spawnExplosion(glm::vec3 pos, int count, ofColor color);
	void tryTriggerShellSpike(); // Tortoise Form: trigger 3 damage to adjacent unit

	void calculateHighlights();
	void calculateTargetHighlights(int cardToCalculate = -1);
	// Returns true if there exists at least one possible target tile for the given
	// hand index such that the card could damage a unit other than the caster.
	bool hasValidNonSelfTargetForHandIndex(int handIndex);
	void invalidateTargetCache();
	void clearHighlights();

	// --- TARGETING HELPERS (consolidated logic) ---
	// Returns list of player indices present at tile (tx, ty)
	std::vector<int> getTileOccupants(int tx, int ty);
	// Returns true if tile contains at least one unit other than excludePlayerIndex
	bool tileHasOtherThan(int tx, int ty, int excludePlayerIndex);
	// Unified target info: computes validity for a card targeting (tx, ty) from caster
	// Handles LOS, range, wall rules, ghost-overlap, and card-specific logic.
	// Used by both client preview and host validation for consistency.
	TargetInfo computeTargetInfo(const Card & card, int casterIdx, int tx, int ty);
	// Returns true if a click on (tx, ty) would affect a unit other than caster
	// (used to determine if card play is legal without hitting only self)
	bool wouldAffectOtherUnit(const Card & card, int casterIdx, int tx, int ty);

	// Centralized targeting helpers
	void enterTargetingMode(const TargetingContext & ctx);
	void cancelTargetingMode();
	void resolveTargetAt(int gx, int gy);

	// --- DETERMINISTIC RNG ---
	// The synced Random Number Generator
	std::mt19937 gameplayRNG;

	// Visual RNG (local only, not part of deterministic gameplay)
	std::mt19937 visualRNG;

	// Networked multi-step action helpers
	// (Moved into `networkPending.actionByActor`, `networkPending.networkActionActor`, etc.)

	// Flag set when the host-provided gameplay seed has been applied
	bool gameplaySeededByHost = false;

	// Desync message shown when checksum fails
	std::string desyncMessage;
	// If true client should wait for host TurnStart packet before performing AP roll
	bool waitingForTurnStartFromHost = false;

	// If true client has requested a snapshot from host and is awaiting it
	bool waitingForSnapshot = false;
	// True while processing an incoming network packet; used to enforce "Zombie Client" rule
	bool processingNetworkPacket = false;
	// Timestamp of last snapshot request to avoid spamming (seconds)
	float lastSnapshotRequestTime = 0.0f;
	// Client: handle out-of-order shuffle packets during draft
	// Client: handle out-of-order shuffle packets during draft
	// (Shuffle nonce queue moved into `networkPending.shuffleNonces`)

	// Helper to get synced numbers
	int getGameRandom(int min, int max);

	// Shuffle a vector deterministically. If `ownerPlayerIndex` is >= 0 and we are
	// in multiplayer, the host will generate a nonce, shuffle with a local PRNG
	// seeded by that nonce and broadcast a `PKT_SHUFFLE` so clients reproduce the same
	// shuffle without consuming `gameplayRNG` on their side. Clients will skip shuffling
	// here when `ownerPlayerIndex >= 0` and wait for the shuffle packet.
	// Custom deterministic shuffle (Fisher-Yates) for cross-platform consistency.
	template <class T, class URBG>
	void deterministic_shuffle(std::vector<T> & vec, URBG & rng) {
		// Fisher–Yates using raw URBG output modulo (i+1).
		// Using the raw `rng()` result ensures identical consumption
		// of the underlying PRNG across platforms/toolchains.
		if (vec.size() <= 1) return;
		for (size_t i = vec.size() - 1; i > 0; --i) {
			auto r = rng(); // consume raw PRNG output
			size_t j = static_cast<size_t>(r % (i + 1));
			std::swap(vec[i], vec[j]);
		}
	}
	template <class T>
	void shuffleGameVector(std::vector<T> & vec, int ownerPlayerIndex = -1, float visualDelaySeconds = 0.0f) {
		// If the client was instructed to skip the next local shuffle for this player (e.g., due to a forwarded Accept),
		// consume the flag and do nothing. This prevents inadvertent consumption of `gameplayRNG`.
		if (isClient() && ownerPlayerIndex >= 0 && skipClientShuffleFor == ownerPlayerIndex) {
			skipClientShuffleFor = -1;
			ofLogNotice("Network") << "Client: Skipping local shuffle for player " << ownerPlayerIndex << " due to forwarded Accept";
			return;
		}

		// Client: defer to host's shuffle packet for player-owned decks
		if (isClient() && ownerPlayerIndex >= 0) {
			return;
		}

		// Host in multiplayer and owner specified: broadcast nonce-based shuffle
		if (isHost() && ownerPlayerIndex >= 0) {
			uint32_t nonce = gameplayRNG();
			std::mt19937 shuffleRng(nonce);
			deterministic_shuffle(vec, shuffleRng);

			// Broadcast shuffle to clients
			ShufflePacket sp = {};
			sp.type = PKT_SHUFFLE;
			sp.playerID = myLocalPlayerID;
			sp.playerIndex = ownerPlayerIndex;
			sp.nonce = nonce;
			steamManager.sendPacket(&sp, sizeof(sp));
			ofLogNotice("Network") << "Host sent Shuffle packet: player=" << sp.playerIndex << " nonce=" << sp.nonce;

			// Start visual shuffle on host for main players only (minions handle their own visuals)
			if (ownerPlayerIndex == 0 || ownerPlayerIndex == 1) {
				startShuffleVisual(ownerPlayerIndex, visualDelaySeconds);
			}

			// Clear dirty flag for this player's deck since we've just shuffled it authoritatively
			if (ownerPlayerIndex >= 0 && ownerPlayerIndex < (int)players.size()) {
				players[ownerPlayerIndex].deckNeedsShuffle = false;
			}
			return;
		}

		// Singleplayer or generic shuffle: use gameplayRNG
		deterministic_shuffle(vec, gameplayRNG);

		// Start visual shuffle only for main players (players 0 and 1).
		// Minions handle their own shuffle visuals in their draw code.
		if (ownerPlayerIndex == 0 || ownerPlayerIndex == 1) {
			startShuffleVisual(ownerPlayerIndex, visualDelaySeconds);
		}

		// If this shuffle was for a specific player's deck, clear the dirty flag
		if (ownerPlayerIndex >= 0 && ownerPlayerIndex < (int)players.size()) {
			players[ownerPlayerIndex].deckNeedsShuffle = false;
		}
	}

	// -------------------------------------------------------------------------
	//                          DATA & HELPERS
	// -------------------------------------------------------------------------
	void loadCardData(const std::string & filePath);

	// Enum Converters
	CardType stringToCardType(const std::string & str);
	TargetingType stringToTargetingType(const std::string & str);
	DamageType stringToDamageType(const std::string & str);

	// Math & Coordinates
	ofVec2f mouseToBoard(int x, int y);
	glm::vec2 getCardDisplayUIPosition(int playerIndex); // Get UI position for card display popup
	glm::vec2 worldToGrid(glm::vec3 worldPos);
	glm::vec3 gridToWorld(int gridX, int gridY);
	bool findNearestTargetableTile(int clickGridX, int clickGridY, int & outGridX, int & outGridY);
	Player * getPlayer(int index);
	int findPlayerIndexByID(int playerID);
	std::string getPlayerDisplayName(int index);
	std::string getPlayerSteamName(int playerIndex); // For player names (Steam)
	const Card * findCardByName(const std::string & name) const;

	// Network helpers
	bool isClient() const { return isMultiplayer && !steamManager.isHost(); }
	bool isHost() const { return isMultiplayer && steamManager.isHost(); }
	bool isMyTurn() const;
	bool isCurrentPlayerLocal() const;

	std::vector<glm::vec2> findShortestPath(glm::vec2 start, glm::vec2 end);
	std::vector<glm::vec2> findShortestPathForPlayer(int playerIndex, glm::vec2 start, glm::vec2 end);
	glm::quat matchFaceToCamera(glm::vec3 faceNormal);

	// Helper: compute the final face rotation quaternion for a die given
	// its `sides`, the `rawResult` (face index), and a visual `wobbleAmount`.
	glm::quat getDiceFaceRotation(int sides, int rawResult, float wobbleAmount);

	// Targeting Algorithms
	std::vector<Player *> findCleaveTargets(glm::vec2 direction);
	TargetInfo isLosTargetValid(glm::vec2 casterTile, glm::vec2 targetTile, float maxRangeFeet, CardType cardType);
	bool checkRayPhysics(glm::vec2 rayStart, glm::vec2 rayEnd);
	int isGapTile(glm::vec2 tile);
	bool isTileBlocked(int x, int y);
	bool isTileWall(int x, int y);
	bool isOrthogonalPathBlocked(glm::vec2 start, glm::vec2 end);
	float getFaceToFaceDistance(glm::vec2 casterTile, glm::vec2 targetTile);
	std::vector<glm::vec2> getLineOfSightPath(glm::vec2 start, glm::vec2 end);
	glm::vec2 getClosestPointOnLineSegment(glm::vec2 p, glm::vec2 start, glm::vec2 end);

	// --- DRAFTING & INITIATIVE ---
	ofRectangle draftAcceptButtonRect;

	int initiativeRolls[2] = { 0, 0 };
	bool isInitiativeRolling = false;
	float initiativeTimer = 0.0f;
	int draftPlayerIndex = 0; // The player currently drafting
	int inGameDraftTargetIdx = -1; // (HOST) During in-game key draft, which player index should receive cards
	int draftStage = 0; // 0 = Class 1 (Pick 2), 1 = Class 2 (Pick 1)
	int draftPicksRemaining = 0;
	bool isInGameDraft = false;
	std::vector<int> selectedDraftIndices; // Tracks indices of cards currently highlighted in draft
	std::vector<Card> draftOptions; // The 3 cards currently shown
	int currentDraftClassTier = 0; // 1/2/3 for the currently displayed options
	std::array<int, 3> currentDraftOptionPoolIndices = { { -1, -1, -1 } }; // Pool indices for current options
	std::vector<Card> class1Cards;
	std::vector<Card> class2Cards;
	std::vector<Card> class3Cards;

	// Networking/draft sync helpers
	bool waitingForDraftOptions = false; // Client waits for host's authoritative DraftOptionsPacket
	float waitingForDraftOptionsStartTime = 0.0f; // When we began waiting (for timeout/retry)
	float waitingForDraftOptionsTimeout = 0.75f; // seconds to wait for host before giving up/requesting
	int skipClientShuffleFor = -1; // When >=0, client will skip the next deck shuffle for this player index (avoids RNG divergence from forwarded Accepts)
	bool draftAcceptLocked = false; // Prevent double-accept clicks per draft screen
	bool draftAcceptApplied = false; // Prevent duplicate forwarded Accept application
	uint32_t draftGenerationCounter = 0; // Increments each time generateDraftOptions is called to ensure variety
	// (Moved into `networkPending.draftStateAvailable` / `networkPending.draftState`)
	bool initialDraftComplete = false; // True once the initial (pre-game) draft finishes

	// Reliability helpers for client-sent DraftAction packets (resend until host ACK/forward)
	DraftActionPacket lastSentDraftActionPacket; // Last DraftActionPacket the client sent (for resend)
	bool lastSentDraftActionValid = false; // True if the lastSentDraftActionPacket still needs ack/resend
	float lastSentDraftActionTime = 0.0f; // Timestamp of last send
	int lastSentDraftActionResendCount = 0; // How many times we've resent
	const float DRAFT_ACTION_RESEND_INTERVAL = 0.75f; // Retry interval (seconds)
	const int DRAFT_ACTION_MAX_RESENDS = 3; // Max resend attempts
	uint32_t draftClientActionCounter = 0; // client-local monotonic id for draft actions

	// Reliability helpers for client-sent Action packets (resend until host ACK)
	ActionPacket lastSentActionPacket; // Last ActionPacket the client sent (for resend)
	bool lastSentActionValid = false;
	float lastSentActionTime = 0.0f;
	int lastSentActionResendCount = 0;
	const float ACTION_RESEND_INTERVAL = 0.75f;
	const int ACTION_MAX_RESENDS = 3;
	uint32_t actionClientActionCounter = 0; // monotonic id for action ACK matching

	// Generic client-local monotonic counter used by lightweight watchdog packets
	uint32_t watchdogClientActionCounter = 0;

	// Host-side: last processed clientActionID per remote player (used to dedupe watchdog packets)
	uint32_t lastProcessedActionID[2] = { 0, 0 };

	// Debug logging helpers: remember last logged draft options count so we only spam logs
	int lastLoggedDraftOptionsCount = -1;
	float lastDraftDrawLogTime = 0.0f;

	void generateDraftOptions(int classTier, const std::vector<int> * forcedIndices = nullptr);
	void applyDraftOptionsFromPool(int classTier, const std::vector<int> & indices, int picksRemaining, int draftingPlayerIdx);
	void onCardPicked(int optionIndex);
	void drawInitiativeRoll();
	void drawDraftScreen();

	// --- Draft UI animation state (purely visual) ---
	enum DraftOptionAnimState {
		DRAFT_ANIM_IDLE = 0,
		DRAFT_ANIM_APPEARING,
		DRAFT_ANIM_HOLDING,
		DRAFT_ANIM_VANISHING
	};

	struct DraftOptionUI {
		float currentScale = 1.0f;
		float startScale = 1.0f;
		float targetScale = 1.0f;
		float startTime = 0.0f;
		DraftOptionAnimState state = DRAFT_ANIM_IDLE;
		bool hidden = false;
	};

	std::vector<DraftOptionUI> draftOptionUI; // per-slot purely-visual animation state
	float draftAnimAppearDuration = 0.06f; // seconds (very fast)
	float draftAnimHoldDuration = 0.55f; // seconds to hold selected card before it shrinks
	float draftAnimVanishDuration = 0.05f; // vanish duration for unselected (near-instant)

	// Visual scheduling for picked-card movement into deck
	struct DraftPickedMove {
		Card card;
		float startTime = 0.0f; // time when move was scheduled
		float delay = 0.55f; // hold before starting move
		float duration = 0.35f; // move duration
		glm::vec2 startPos; // screen-space center
		glm::vec2 endPos; // screen-space center (deck)
		bool finished = false;
		int ownerIndex = -1; // which player's deck we're animating into
		float endScale = 1.0f; // final scale when reaching destination (e.g., 0.3f for minion UI)
	};

	std::vector<DraftPickedMove> activeDraftPickedMoves;

	// Deck flash when a picked card lands (visual only)
	float deckFlashStartTime = 0.0f;
	float deckFlashDuration = 0.45f;

	int deckFlashOwnerIndex = -1;

	// Schedule next draft generation to allow animations to finish
	bool draftNextScheduled = false;
	int draftNextClassTier = -1;
	float draftNextAt = 0.0f; // epoch time when to run generateDraftOptions

	// Schedule end-of-draft transition (wait for visuals before returning to gameplay)
	bool draftEndScheduled = false;
	int draftEndNextPlayerIndex = -1;
	float draftEndAt = 0.0f;

	// Helpers
	void startShuffleVisual(int playerIndex, float delaySeconds = 0.0f);
	void scheduleGenerateDraftOptions(int classTier, float delaySeconds);

	// Draw helper for active picked-card animations
	void drawActiveDraftPickedMoves();

	// -------------------------------------------------------------------------
	//                          RENDERING & MESHES
	// -------------------------------------------------------------------------
	void buildLevelMesh();
	void buildFloorMesh();

	void allocateWorldFbo(int w, int h);

	// Specific UI Drawers
	void drawMagicBlastChoiceUI();
	void drawOpponentMenu(); // Draw opponent's active menu with red outlines

	// Board highlight helpers
	void drawJoinedOutlines(bool highlightedTiles[BOARD_WIDTH][BOARD_HEIGHT], ofColor color, float surfaceY);

	// Standardized card-choice panel helper
	void drawCardChoicePanel(const ofRectangle & panelRect,
		const std::string & title,
		const std::string & desc,
		ofRectangle & primaryRect,
		ofRectangle & secondaryRect,
		const std::string & primaryLabel,
		const std::string & secondaryLabel,
		ofColor primaryAccent,
		ofColor secondaryAccent,
		bool primaryEnabled = true,
		bool secondaryEnabled = true);

	// General damage application helper (used by multiple flows)
	bool applyDamageTo(Player & target, int damage, DamageType type, int attackerIndex = -1);
	void drawDispelUI();
	void cancelDispel();
	void determineStatusOptions(Player * target);
	void applyDispelEffect(int statusIndex);
	void drawMinionManagerUI();
	void drawMinionStatusBars(Player & minion, const std::string & name, float x, float y, float totalWidth);

	// =========================================================================
	//                            MEMBER VARIABLES
	// =========================================================================

	// --- GAME STATE ---
	GameState currentState = STATE_MAIN_MENU;
	GameState prevState = STATE_MAIN_MENU;
	GameState stateBeforeSettings = STATE_MAIN_MENU;
	GameState pausedFromState = STATE_GAMEPLAY; // Default fallback
	bool isLoadingGame = false;
	int globalTurnCounter = 0;
	int nextSummonOrder = 0; // Global counter for minion summon ordering

	// Menu UI Rectangles
	ofRectangle mainMenuHostButton;
	ofRectangle mainMenuInviteButton;

	// Save/Load UI buttons (gameplay HUD)
	ofRectangle saveGameButtonRect;
	ofRectangle loadGameButtonRect;

	// Singleplayer menu UI
	ofRectangle singleplayerContinueButton;
	ofRectangle singleplayerLoadButton;
	ofRectangle singleplayerNewGameButton;
	ofRectangle singleplayerBackButton;

	// Draw singleplayer menu
	void drawSingleplayerMenu();

	// Settings tabs
	enum SettingsTab {
		SETTINGS_TAB_VIDEO = 0,
		SETTINGS_TAB_AUDIO = 1,
		SETTINGS_TAB_GAME = 2,
		SETTINGS_TAB_CONTROLS = 3
	};
	int currentSettingsTab = SETTINGS_TAB_VIDEO;

	// Audio settings (controls shown in Settings -> Audio)
	float settingsMasterVolume = 1.0f; // 0.0 - 1.0
	float settingsMenuVolume = 0.6f; // per-menu music multiplier
	float settingsSfxVolume = 0.8f; // per-sfx multiplier

	// Controls tab: key bindings
	std::vector<std::pair<std::string, int>> settingsKeyBindings; // action, key
	int settingsRebindingIndex = -1; // -1 = not rebinding

	// Settings UI rects (tabs + audio controls)
	ofRectangle settingsTabVideoRect;
	ofRectangle settingsTabAudioRect;
	ofRectangle settingsTabGameRect;
	ofRectangle settingsTabControlsRect;
	ofRectangle settingsAudioVolumeSlider;
	ofRectangle settingsAudioMasterSlider;
	ofRectangle settingsAudioSfxSlider;
	// Game tab rects
	ofRectangle settingsGameShowFPSBox;
	ofRectangle settingsCameraSensitivitySlider;
	ofRectangle settingsInvertYBox;
	ofRectangle settingsUIScaleSlider;
	ofRectangle settingsVSyncBox;
	ofRectangle settingsShowHintsBox;

	// Game tab settings
	bool settingsShowFPS = true;
	float settingsCameraSensitivity = 1.0f;
	bool settingsInvertCameraY = false;
	float settingsUIScale = 1.0f; // 0.75..1.25
	bool settingsUseVSync = true;
	bool settingsShowHints = true;

	// Persistence helpers
	void loadSettings();
	void saveSettings();

	// Save / Load full game state (JSON)
	bool saveGameStateToFile(const std::string & path);
	bool loadGameStateFromFile(const std::string & path);
	void pruneOldSaves(int keepCount);

	// Helper to know if we are waiting in a lobby
	bool isInLobby = false;

	// --- BOARD & ENTITIES ---
	const float TILE_SIZE = 5.0f;
	Tile board[BOARD_WIDTH][BOARD_HEIGHT];
	TargetInfo targetCache[BOARD_WIDTH][BOARD_HEIGHT];
	std::deque<Player> players;
	// Track which minion playerIDs we've logged during render to avoid flooding logs
	std::unordered_set<int> renderLoggedMinions;
	int currentPlayerIndex = -1;

	// --- TURN TIMER ---
	float turnStartTime = 0.0f; // when the current turn began (ofGetElapsedTimef())
	float turnDurationSeconds = 90.0f; // 90 seconds for regular units, 60 for minions
	bool turnTimerEnabled = true; // whether to enforce auto-end-turn on timeout

	// Pause/resume support when modal choices are presented to other players
	bool turnTimerPaused = false;
	float turnTimerPausedRemaining = 0.0f; // seconds remaining when paused

	// Opponent decision timer (when a modal requires the opponent to choose)
	bool opponentDecisionTimerActive = false;
	float opponentDecisionStartTime = 0.0f;
	float opponentDecisionDuration = 20.0f; // default opponent decision window
	int opponentDecisionPlayerIndex = -1; // which player must decide

	std::vector<DeathMarker> graveyard;
	std::vector<FloatingText> activeFloatingTexts;
	std::vector<Particle> particles;
	float screenShake = 0.0f;

	// --- Tracer effects for ranged spells ---
	struct Tracer {
		glm::vec3 start;
		glm::vec3 end;
		glm::ivec2 impactTile; // tile being highlighted
		float startTime = 0.0f;
		float duration = 5.0f;
		ofColor color = ofColor::white;
	};

	std::vector<Tracer> activeTracers;

	// Spawn a tracer line from world-space start -> end and highlight impact tile
	void spawnTracer(glm::vec3 start, glm::vec3 end, glm::ivec2 impactTile, ofColor color, float duration = 5.0f);

	// Compute tracer world-space endpoints such that tracer starts at the caster's
	// closest face midpoint and ends at the center of the hit grid fraction.
	void computeTracerEndpoints(glm::vec2 casterTile, glm::vec2 hitGridFrac, glm::vec3 & outStart, glm::vec3 & outEnd);

	// --- KEY ANIMATION (Floating Key on Floor) ---
	std::vector<ofTexture> keyTextures; // loaded from Board/keys_1_*.png
	std::vector<ofTexture> keyTexturesSilver; // loaded from Board/keys_2_*.png
	std::vector<ofTexture> keyTexturesBronze; // loaded from Board/keys_3_*.png
	std::vector<int> keyAnimSequence; // order to play frames (indices into keyTextures)
	int keyAnimSeqPos = 0;
	float keyAnimTimer = 0.0f;
	float keyAnimInterval = 1.0f / 6.0f; // base interval (seconds) -> 6 fps
	// Speed presets: multiplier applied to base interval. >1.0 = slower, <1.0 = faster
	// Default to 1.5x (50% slower)
	// Multiplier applied to base interval; larger = slower animation
	// Increased to slow animation further because current speed was too fast
	std::vector<float> keyAnimSpeedPresets = { 4.0f };
	int keyAnimSpeedIndex = 0; // index into presets (only one preset)
	int keyAnimTileX = BOARD_WIDTH / 2;
	int keyAnimTileY = BOARD_HEIGHT / 2;

	// Multiple floating keys: list of grid coordinates and their type (1=gold,2=silver,3=bronze)
	struct FloatingKey {
		glm::ivec2 pos;
		int set;
	};
	std::vector<FloatingKey> floatingKeyInstances;

	// Render scale for floating keys (multiplies the base world height)
	// Render scale for floating keys. Slightly reduced for less prominence.
	float keyRenderScale = 0.75f;

	// --- LOGIC CACHE ---
	int currentAP = 0;
	bool hasDrawnCardsThisTurn = false;
	bool opponentHasDrawnCardsThisTurn = false;
	// Network/UI staging: moved into `networkPending` struct (see below)
	// When true, `updateGame()` should not send PKT_TURN_START until status effects
	// (paralysis/poison/onFire/etc.) that occur at the start of a turn have finished.
	bool isHandlingTurnStartEffects = false;
	PlayerActionState playerAction = NONE;
	int selectedPieceGridX = -1;
	int selectedPieceGridY = -1;
	int lastCachedPlayerX = -1, lastCachedPlayerY = -1;
	int lastCachedCardIndex = -1;

	// --- CAMERA ---
	ofCamera cam; // Player 0's camera
	ofCamera cam2; // Player 1's camera (opposite side)
	ofLight headlight;
	ofLight keyLight;
	ofLight rimLight;
	ofLight uiLight;
	std::vector<ofLight> lights;
	float lightNoiseOffset = 0.0f;

	float cameraTargetZoom = 35.0f;

	// Centralized container for network/UI pending state that was previously
	// scattered as top-level `pending*` variables. Use `networkPending` to
	// access/modify these fields instead of the old globals.
	struct NetworkPending {
		// Key-draft (scheduled in-game draft when a key pickup arrives)
		bool keyDraftAccept = false;
		int keyDraftPlayer = -1;
		int keyDraftPlayerID = -1; // stable playerID mapping
		int keyDraftClass = 0;
		float keyDraftTriggerTime = 0.0f;
		int keyDraftKeyX = -1;
		int keyDraftKeyY = -1;

		// Draft finalization/shuffle coordination (host/client)
		bool draftFinalize = false;
		bool draftShuffleNeeded = false;
		std::vector<int> draftQueue; // class IDs queue

		// Pending shuffle nonces per actor
		std::unordered_map<int, std::deque<uint32_t>> shuffleNonces;
		std::unordered_map<int, uint32_t> lastAppliedShuffleNonce;

		// Pending draft state packet (if host sends state while client is waiting)
		bool draftStateAvailable = false;
		DraftStatePacket draftState;

		// Network action staging
		std::unordered_map<int, int> actionByActor;
		int networkActionActor = -1;
		int networkActionPrevPlayer = -1;

		// Save browser pending selection/confirmation (migrated from top-level)
		int saveBrowserPendingIndex = -1;
		bool saveBrowserConfirmVisible = false;
	} networkPending;
	float cameraCurrentZoom = 35.0f;
	glm::vec3 cameraTargetPan = glm::vec3(0, 0, 0);
	glm::vec3 cameraCurrentPan = glm::vec3(0, 0, 0);
	bool isTopDownView = false;
	glm::vec3 cameraCurrentPos;
	glm::vec3 cameraCurrentPos2; // Smoothed position for cam2
	glm::vec3 cameraCurrentLookAt;
	glm::vec3 cameraCurrentLookAt2; // Smoothed lookAt for cam2
	float last3DZoom = 35.0f;
	float lastWindowWidth = 0;
	float lastWindowHeight = 0;
	bool draftingCameraLockedToClient = false; // Remember which camera was used before drafting

	// --- 3D ASSETS ---
	ofxAssimpModelLoader playerModel;
	ofxAssimpModelLoader skeletonModel;
	ofxAssimpModelLoader wolfModel;
	ofxAssimpModelLoader koboldModel;
	ofxAssimpModelLoader golemModel;
	ofxAssimpModelLoader hellhoundModel;
	ofxAssimpModelLoader demonModel;
	ofxAssimpModelLoader tortoiseModel;
	ofxAssimpModelLoader ghostModel;
	ofxAssimpModelLoader wallUnitModel;
	ofxAssimpModelLoader koboldKingModel;
	ofxAssimpModelLoader assistantModel;

	// Faerie assets
	ofxAssimpModelLoader faerieModel;

	// Audio: main menu music
	ofSoundPlayer mainMenuMusic;
	ofTexture faerieTexture;

	ofTexture playerTexture;
	ofTexture skeletonTexture;
	ofTexture wolfBodyTex, wolfFaceTex, wolfFurTex;
	ofTexture golemTexBase, golemTexRock, golemTexFire, golemTexElectric;
	ofTexture tortoiseTexture;
	ofTexture ghostBaseTex;
	ofTexture koboldKingTexture;

	ofMaterial modelMaterial;
	ofMaterial diceMaterial;

	// --- ENVIRONMENT MESHES & TEXTURES ---
	ofMesh levelMesh;
	ofMesh levelMeshDark;
	ofMesh wallMesh;
	ofMesh roomMesh;
	ofTexture roomTexture;
	std::vector<ofMesh> floorMeshes;
	ofTexture wallTexture;
	ofTexture wallUnitTexture;
	ofTexture wallDarkTexture;
	std::vector<ofTexture> floorTextures;
	ofImage shadowTexture;
	// Simple blob shadow texture (generated at startup)
	ofTexture blobShadowTex;
	float blobShadowSize = 1.8f; // world-space diameter multiplier for shadows
	ofImage fireTexture;

	// --- POST PROCESSING ---
	ofFbo worldFbo;
	ofShader worldPostShader;
	bool worldPostShaderLoaded = false;
	bool enableWorldPostProcess = true;
	bool showWorldFboPreview = true; // 'Y' preview on by default

	// --- COMMODORE64 POST PROCESS ---
	ofShader c64Shader;
	bool c64ShaderLoaded = false;
	bool enableC64Shader = false; // toggled with 'm'
	float c64ScanlineIntensity = 0.25f;

	// --- BLOOM ---
	// Two ping-pong FBOs for separable blur and an extract shader
	ofFbo bloomFboA;
	ofFbo bloomFboB;
	ofShader bloomExtractShader;
	ofShader bloomBlurShader;
	int bloomDownscale = 4; // render bloom at 1/downscale resolution
	float bloomThreshold = 0.7f; // brightness threshold
	int bloomBlurPasses = 2; // number of horizontal/vertical blur iterations
	bool bloomLoaded = false;
	bool bloomActiveNotified = false;

	// --- SHADOW MAPS & PBR ---
	ofFbo shadowFbo;
	ofShader shadowDepthShader;
	ofShader pbrShader;
	int shadowMapSize = 2048;
	glm::mat4 lightViewProj;
	bool shadowDepthShaderLoaded = false;
	bool pbrShaderLoaded = false;
	// Master switch to enable/disable default shaders (P toggles this)
	bool enableShaders = false; // default: shaders off

	// --- PIXEL ART RENDERING ---
	// Render the world to a low-res FBO and apply a posterize/dither shader
	ofFbo pixelLowFbo;
	ofShader pixelArtShader;
	bool pixelArtShaderLoaded = false;
	bool enablePixelArt = false; // toggle the effect (default OFF)
	bool pixelArtWarned = false; // set when we warn once about shader missing
	bool pixelArtActiveNotified = false; // set once when pixel-art branch runs
	bool pixelArtDumpedPixels = false; // set once when we read back low-res FBO for debugging

	// Notify once when world post-process runs
	bool worldPostActiveNotified = false;
	// Apply nearest filtering and other pixel-art settings to textures/FBOs
	void applyPixelArtSettings();
	int pixelArtDownscale = 3; // render at 1/downscale resolution (higher value => lower internal resolution)
	int pixelArtLevels = 12; // posterize levels per channel (higher => less posterize / preserve brightness)
	bool pixelArtDither = true;

	// Master switch to disable all glow/outline visual effects (for crisp visuals)
	// Default is false so outlines and target glows appear normally.
	bool disableAllGlow = false; // default: enable glows

	// --- ANIMATIONS ---
	bool isPlayerAnimating = false;
	int animatingPlayerIndex = -1; // Which player is currently animating
	glm::vec3 playerVisualPos;
	float playerFacingAngle = 0.0f; // 0 = North, 90 = East, 180 = South, 270 = West
	std::vector<glm::vec3> animationPath;
	int currentPathIndex = 0;
	float animationSegmentStartTime = 0.0f; // Time when current segment started
	// Visual hop amplitude for movement (world units)
	float movementHopHeight = 0.35f;

	// Camera shake (visual only)
	float cameraShakeIntensity = 0.0f; // current intensity
	float cameraShakeTimer = 0.0f; // remaining time
	float cameraShakeDuration = 0.0f; // total duration for decay calculations
	glm::vec3 cameraShakeOffset = glm::vec3(0.0f);

	// Trigger a camera shake: intensity in world units, duration in seconds
	void triggerCameraShake(float intensity, float duration);

	// Pause / resume helpers for opponent-driven decisions (keys, magic blast)
	void pauseTurnTimerForOpponentDecision(int decidingPlayerIndex);
	void resumeTurnTimerIfPausedForOpponent(int decidingPlayerIndex);

	// Menu open scale animation (scale from small to 1.0)
	float menuOpenScale = 1.0f;
	float menuOpenStartTime = 0.0f;
	float menuOpenDuration = 0.2f; // seconds
	std::vector<glm::vec2> hoverPath;
	glm::vec2 lastHoverGridPos = { -1, -1 };

	std::vector<PlayedCardDisplay> activeCardDisplays;
	std::vector<StolenCardAnimation> activeStolenCardAnimations;
	std::vector<PlayedCardAnimation> activePlayedCardAnimations;
	std::vector<RemovedCardAnimation> activeRemovedCardAnimations;

	// --- CARDS & DECK ---
	ofImage cardSpriteSheet;
	ofImage cardBackImage;
	std::vector<Card> allCards;

	// --- UI INTERACTION ---
	int selectedCardIndex = -1;
	int draggedCardIndex = -1;
	int pressedCardIndex = -1; // Card being pressed down (set in mousePressed)
	int hoveredCardIndex = -1;
	int lastHoveredCardIndex = -1;
	ofVec2f dragOffset;
	ofVec2f mouseDownPos;

	ofRectangle endTurnButtonRect;
	ofRectangle rerollButtonRect;
	ofVec2f endTurnButtonCurrentPos;
	ofVec2f endTurnButtonTargetPos;
	bool isHoveringEndTurn = false;
	bool endTurnLocked = false; // Prevent repeated end turn clicks before turn updates

	ofTrueTypeFont uiFont;
	ofTrueTypeFont titleFont;

	ofFbo modelFbo;
	std::vector<MinionUI> activeMinionUIs;

	// ===== UNIFIED CARD INTERACTION STATE (Replaces 20+ per-card bool flags) =====
	CardInteractionState cardInteractionState = CARD_INTERACTION_IDLE;
	int interactingCardIndex = -1; // Index in currentPlayer.hand of card being interacted with
	int interactingCardType = CARD_NONE; // Type of card being interacted with (cached)
	int interactionTargetIndex = -1; // Index of chosen target (if applicable)
	std::string interactionMenuChoice; // Selected menu option (Burst: damage/heal, Double-Handed: Punch/Block, etc.)
	bool interactionNeedsStatusSelect = false; // Special: Dispel status selection required
	int interactionDiceRoll = 0; // Cached dice result if interaction requires roll (Teleport range, etc.)
	glm::vec2 interactionTargetTile; // Tile target for interactions that require a grid position (Teleport)
	std::string interactingCardName; // Optional: store card name for resolution logging

	// --- Legacy Targeting States (to be deprecated after consolidation) ---
	int deathCardIndex = -1;

	// Centralized targeting state members (struct defined in public area)
	TargetingContext targetingContext;

	int healCardIndex = -1;

	// Punch targeting: migrated to centralized `interactingCardIndex` and `cardInteractionState`

	// Tooltips & Piles
	bool isShowingTooltip = false;
	ofVec2f tooltipPos;
	std::string tooltipText;
	// Expanded tooltip state: when true, show full wrapped tooltip until closed
	bool isTooltipExpanded = false;
	std::string tooltipExpandedText;

	bool isHoveringPile = false;
	PileViewMode hoveredPileType = VIEW_NONE;

	// Unit hover tooltip
	bool isHoveringUnit = false;
	float unitHoverStartTime = 0.0f;
	int hoveredUnitIndex = -1; // index into players vector or -1
	int hoveredPilePlayerIndex = -1;
	float pileHoverStartTime = 0.0f;

	bool isShowingPileView = false;
	PileViewMode currentPileView = VIEW_NONE;
	int currentPileViewPlayerIndex = -1;
	// Draft finalization/shuffle coordination moved into `networkPending`.

	// Draw the pile view panel for a given player and view mode
	void drawPileViewFor(int viewPlayerIndex, PileViewMode viewMode);
	std::vector<Card> cardsToShowInView;
	ofRectangle pileViewRect;
	ofRectangle p0_deckRect, p0_discardRect;
	ofRectangle p1_deckRect, p1_discardRect;
	ofRectangle p0_apStatusRect;
	ofRectangle p1_apStatusRect;

	// Minion manager panel layout/scroll metrics (computed in updateGame, consumed by draw/input)
	float p0_minionLeft = 0.0f;
	float p1_minionLeft = 0.0f;
	float p0_minionTop = 0.0f;
	float p1_minionTop = 0.0f;
	float p0_minionViewH = 0.0f;
	float p1_minionViewH = 0.0f;
	float p0_minionTotalH = 0.0f;
	float p1_minionTotalH = 0.0f;
	float p0_minionScroll = 0.0f;
	float p1_minionScroll = 0.0f;
	float minionPanelW = 260.0f;
	int lastAutoScrollTurnUnit = -1;
	int lastHoveredUnit = -1;

	// ============================================================================
	// LOCKSTEP DETERMINISTIC SIMULATION STATE
	// ============================================================================

	// Command queue for deterministic input processing
	std::vector<InputCommandPacket> commandQueue;
	uint32_t nextCommandId = 1;
	uint32_t lastProcessedCommandId = 0;

	// Fixed-step simulation
	const float SIMULATION_TIMESTEP = 1.0f / 60.0f; // 60Hz fixed tick
	float simulationAccumulator = 0.0f;
	uint32_t simulationFrame = 0;

	// Effect sequence for current card
	EffectSequence currentEffectSequence;
	bool isProcessingEffect = false;
	bool isExecutingLockstepCommand = false;

	// Lockstep functions
	void queueInputCommand(const InputCommandPacket & cmd);
	void processCommandQueue();
	void simulationTick();
	void executeInputCommand(const InputCommandPacket & cmd);
	void processEffectOp(EffectOp & op);

	// --- CARD SPECIFIC VARIABLES ---

	// === UNIFIED CARD STATE SYSTEM ===
	// All cards use this single outcome structure instead of scattered pending* variables
	CardOutcome currentCardOutcome; // Currently active card's outcome data
	CardPlayState cardPlayState = CARD_STATE_IDLE; // Current state of card action
	int activeCardIndex = -1; // Index of card currently being played (-1 if none)

	// Helper function to reset card state between plays
	void resetCardState() {
		currentCardOutcome = CardOutcome();
		cardPlayState = CARD_STATE_IDLE;
		activeCardIndex = -1;
	}

	// Attack
	bool isWaitingForAttackDice = false;
	// Attack dice/result now use centralized `interactionDiceRoll` and
	// `interactingCardName` for logging/special cases. Targets & damage type
	// are stored in `currentCardOutcome.attackTargetIndices` and
	// `currentCardOutcome.attackDamageType`.

	// Poison (from Add Poison card)
	bool isWaitingForPoisonAttackDice = false;
	// Poison damage rolls are stored in `currentCardOutcome.namedDiceResults["poison_attack"]`.
	// Targets are in `currentCardOutcome.poisonTargetPlayerIDs`.

	// Amnesia (migrated to effect/op handlers)
	int amnesiaTargetPlayerIndex = -1;
	int numCardsToRemove = 0;
	std::vector<Card> amnesiaDeckCopy;
	std::vector<int> amnesiaSelectedIndices;
	std::vector<ofRectangle> amnesiaCardRects;
	ofRectangle amnesiaAcceptButton;

	// Which local player ID is allowed to choose Amnesia removals (playerID, e.g., 0 or 1). -1 = none
	int amnesiaChooserPlayerID = -1;

	// Blocking Boon
	// Draft queue moved into `networkPending.draftQueue`.
	int blockingBoonTargetIndex = -1; // Stores target for the "Tails" effect

	// Blocking Boon staged resolution (migrated to effect/op system)
	// Counters moved into `currentCardOutcome.namedDiceResults`:
	//  - "blocking_boon_coins_remaining"
	//  - "blocking_boon_nonphys"
	//  - "blocking_boon_total"

	// Prevent duplicate plays while a Blocking Boon is resolving
	bool blockingBoonActive = false;

	// Magic Blast
	bool isWaitingForMagicBlastDice = false;
	// Magic Blast range/target resolved via centralized interaction fields:
	// `interactionDiceRoll` and `interactionTargetTile` are used instead of per-card pending variables.
	// Magic Blast choice UI now uses centralized cardInteractionState
	int magicBlastTargetPlayerIndex = -1;
	int magicBlastChoicesRemaining = 0;
	// Stores playerIDs (stable) for queued magic blast splash targets. Resolve to indices at processing time.
	std::vector<int> magicBlastSplashTargetIndices;
	ofRectangle magicBlastDamageButton;
	ofRectangle magicBlastDiscardButton;

	// --- Psionic Wave ---
	std::vector<int> psionicWaveTargetIndices; // Store who got hit by the range check

	// Fireball
	// (migrated to deterministic instant-resolve + visual queue)
	glm::vec2 fireballImpactTile;
	int fireballTargetPlayerIndex = -1;
	bool isWaitingForFireballRangeDice = false;
	bool isWaitingForFireballDamageDice = false;

	// Ethereal Jolt
	bool isWaitingForJoltRangeDice = false;
	// uses `interactionDiceRoll` and `interactionTargetTile`

	// Chain Lightning
	int chainLightningCardIndex = -1;
	bool isWaitingForChainLightningRange = false;
	bool isWaitingForChainLightningDamage = false;
	// Chain Lightning uses `interactionDiceRoll` and `interactionTargetTile`

	// Train Menu UI
	// Train menu now uses `interactingCardIndex` and `cardInteractionState`
	ofRectangle trainMenuRect;
	ofRectangle trainBtnAP;
	ofRectangle trainBtnDraft;

	// Function Declaration
	void drawTrainMenuUI();

	// Dispel (UI state migrated to centralized cardInteractionState)
	bool isWaitingForBarrierDice = false;
	int dispelMode = 0; // 0 = none, 1 = barrier, 2 = purge
	ofRectangle dispelMenuRect;
	ofRectangle dispelBtnBarrier;
	ofRectangle dispelBtnPurge;
	ofRectangle statusSelectMenuRect;
	std::vector<ofRectangle> statusSelectButtons;
	std::vector<std::string> statusSelectLabels;

	// Teleport now uses centralized interaction fields: `interactingCardIndex`, `interactionDiceRoll`, `interactionTargetTile`

	// --- EARTHQUAKE SYSTEM ---
	struct EarthquakeState {
		int playerIndex;
		int tilesToMove; // Result of d4
		int originalDistance; // The original dice distance rolled (for arrows/visuals)
		int diceIndex; // Index into activeDiceRolls for this unit's quake roll
		glm::ivec2 direction; // (0,1), (-1,0), etc.
		glm::ivec2 startGrid; // Where they started this step
		glm::ivec2 nextGrid; // Where they want to go
		glm::vec3 visualPos; // For smooth animation
		bool isMoving; // False if hit wall/unit
		bool crashed; // True if took damage
		bool crashDamageApplied = false; // True if we've applied immediate crash damage (for head-on)
		int crashDiceLastStep = -1; // Prevent spawning multiple crash dice in same step
	};

	bool isEarthquakeActive = false;
	bool isEarthquakeDiceRolling = false; // Phase 1: Dice
	bool isEarthquakeAnimatingStep = false; // Phase 2: Movement
	float earthquakeT = 0.0f; // 0.0 to 1.0 for interpolation
	std::vector<EarthquakeState> earthquakeUnits;
	int earthquakeStep = 0; // Incremented each earthquake animation step
	// Waiting state between dice resolution and movement
	bool isEarthquakeWaiting = false; // Phase between dice and movement
	float earthquakeWaitTimer = 0.0f; // seconds remaining

	// Network-assisted earthquake assignment counter (used when client receives dice)
	int earthquakeDiceAssignCounter = 0;

	// When a client plays earthquake, it waits for host to send EarthquakeBegin
	bool isWaitingForEarthquakeBegin = false;

	// Wisdom Boon (uses centralized interaction state)
	ofRectangle wisdomMenuRect;
	ofRectangle wisdomBtnDamage;
	ofRectangle wisdomBtnBlock;

	// Burst of Light (choice UI + targeting) — centralized via interaction state
	int burstChoice = -1; // -1 = undecided, 0 = Damage, 1 = Heal
	ofRectangle burstMenuRect;
	ofRectangle burstBtnDamage;
	ofRectangle burstBtnHeal;

	// Heal (handled via effect/op pipeline)

	// Summon (Raise Dead)
	enum PendingSummonKind {
		PENDING_SUMMON_NONE = 0,
		PENDING_SUMMON_SKELETON = 1,
		PENDING_SUMMON_HELLHOUND = 2,
		PENDING_SUMMON_DEMON = 3,
		PENDING_SUMMON_KOBOLD = 4,
		PENDING_SUMMON_WOLF = 5,
		PENDING_SUMMON_KOBOLD_KING = 6,
		PENDING_SUMMON_ASSISTANT = 7,
		PENDING_SUMMON_FAERIE = 8,
		PENDING_SUMMON_GOLEM = 9,
		PENDING_SUMMON_WALL = 10,
		PENDING_SUMMON_MAGIC_WALL = 11
	};
	bool isWaitingForSummonHealth = false;
	// Summon placement/HP use centralized interaction fields:
	// - `interactionDiceRoll` stores the rolled HP/count
	// - `interactionTargetTile` stores the target grid tile
	// - `interactionTargetIndex` stores the summoner player index

	// --- Double Handed State (centralized interaction state used) ---
	ofRectangle doubleHandedMenuRect;
	ofRectangle btnAddPunches;
	ofRectangle btnAddBlocks;

	// --- Opponent Interaction State (centralized visualization for remote players) ---
	struct OpponentInteraction {
		bool open = false;
		int type = 0; // 0=none, 1=wisdom, 2=burst, 3=doubleHanded
		int targetIndex = -1;
		int hoveredChoice = -1; // -1=none, 0=first option, 1=second option
		int cardIndex = -1;
	} opponentInteraction;

	// --- Amnesia State ---

	ofRectangle amnesiaMenuRect;
	ofRectangle amnesiaBtnSelf;
	ofRectangle amnesiaBtnAdjacent;

	// --- Renewed Inspiration State (REAL-TIME) ---
	// Uses centralized `cardInteractionState` (CARD_INTERACTION_MENU + CARD_RENEWED_INSPIRATION)
	std::vector<int> renewedSelectedHandIndices; // Indices of cards currently selected in hand
	ofRectangle riConfirmBtn;
	ofRectangle riCancelBtn;

	// --- Tortoise Form Targeting State ---

	// --- Call For Wolves State ---
	int wolvesRemainingToPlace = 0;
	int wolfPlacementSourceX = -1; // Where the summoner is standing
	int wolfPlacementSourceY = -1;
	int wolfSummonCount = 0; // To track "Wolf 1", "Wolf 2"
	int wolfSummonStage = 0; // 0=None, 1=First Wolf, 2=Second Wolf

	// --- Call For Kobolds State ---
	bool isWaitingForKoboldDice = false;
	int koboldsRemainingToPlace = 0;
	int koboldPlacementSourceX = -1;
	int koboldPlacementSourceY = -1;
	int koboldSummonCount = 0;
	int koboldSummonStage = 0;

	// Remote kobold placement visualization (when another player is placing kobolds)
	int remoteKoboldPlacementSourceX = -1;
	int remoteKoboldPlacementSourceY = -1;
	int remoteKoboldsRemaining = 0;

	// Time Vortex handled via effect/op pipeline

	// --- Magic Bolt State ---
	bool isWaitingForMagicBoltRange = false;
	// Magic Bolt range/target resolved via centralized interaction fields:
	// `interactionDiceRoll` and `interactionTargetTile` are used instead of per-card pending variables.
	int magicBoltCardIndex = -1;

	// Magic Bolt intermediate resolution state (primary damage and AOE)
	bool isWaitingForMagicBoltPrimary = false;
	bool isWaitingForMagicBoltAoe = false;
	// Impact tile and dice results are stored in `currentCardOutcome`:
	// - `currentCardOutcome.primaryTarget` stores the impact tile (grid coords)
	// - `currentCardOutcome.namedDiceResults["magicbolt_primary"]` stores primary damage
	// - `currentCardOutcome.namedDiceResults["magicbolt_aoe"]` stores AOE radius roll

	// Shoot Arrow State
	bool isWaitingForShootArrow = false;
	// Shoot Arrow uses centralized interaction fields for range/target and dice:
	// `interactionDiceRoll` and `interactionTargetTile`/`interactionTargetIndex` are used instead.
	// Destroyed card type for Shoot Arrow is stored in `currentCardOutcome.destroyedCardType`

	// --- Giant Magic Hand ---
	// Magic hand menu UI migrated to centralized `cardInteractionState`
	glm::ivec2 magicHandTargetTile;
	bool isWaitingForMagicHandDamage = false;
	int magicHandPushedUnitIndex = -1;
	glm::ivec2 magicHandPushDir;

	// Flail
	bool isWaitingForFlailDice = false;

	// Hellhound targeting
	int hellhoundCardIndex = -1;

	// Death Card Logic
	bool isWaitingForDeathDice = false;
	bool isWaitingForSleepDuration = false;
	// Death target and roll stored in `currentCardOutcome.targetPlayerIndex` and
	// `currentCardOutcome.namedDiceResults["death_check"]` respectively.

	// --- Spark of genius Logic ---
	bool isWaitingForSparkOfGeniusDice = false;

	// Helper functions
	void resolveDoubleHanded(std::string cardName);
	void drawCardSpawnerUI();
	void drawCardEncyclopediaUI();

	// === EXTRACTED CARD EFFECT FUNCTIONS (Phase 2 + 3: Consolidation) ===
	// These are called from executeCardByType(), resolveCardMenu(), and resolveCardDice() to eliminate duplication
	void applyBurstOfLight(int targetPlayerIndex, const std::string & choice);
	void applyWisdomBoon(int targetPlayerIndex, const std::string & choice);
	void applyDoubleHandedChoice(int targetPlayerIndex, const std::string & choice);
	void applyTrainCard(const std::string & choice);
	void applyDispelChoice(int targetPlayerIndex, const std::string & choice);
	void applyShockEffect(int targetPlayerIndex, int damageAmount, int casterIndex);
	void applyFireballHit(int targetPlayerIndex, int damageAmount);
	void applyDrainPunch(int targetPlayerIndex, int baseDamage, int casterIndex);
	// Helper to draw centered instruction text with shadow
	void drawInstructionText(const std::string & message, ofColor color = ofColor::white);
	// Helper to draw centered dice label text with shadow
	void drawDiceLabel(const std::string & message, ofColor color = ofColor(255, 215, 0), float yPos = 0);
	// Helpers to draw card menu overlays and titles
	void drawMenuOverlay();
	void drawMenuBackground(const ofRectangle & menuRect, float cornerRadius = 15);
	void drawMenuTitle(const std::string & title, const ofRectangle & menuRect, float yOffset = 60);

	// ===== CENTRALIZED CARD INTERACTION SYSTEM =====
	void updateCardInteractionState(CardInteractionState newState, int cardIdx = -1, int cardType = CARD_NONE);
	void resetCardInteraction();
	void handleCardDragToPlay(int cardIndex);
	void handleCardTargetClick(int gridX, int gridY);
	void handleCardMenuClick(const std::string & buttonId);
	void drawActiveCardInteractionUI();

	// Cancel any active targeting modes/menus and reset related state
	void cancelAllTargeting();
	void resolveMagicHandPull();
	void resolveMagicHandPush();
	void cancelMagicHand();

	// Status Effects
	bool isWaitingForOnFireDice = false;
	bool isWaitingForPoisonDice = false;
	// NOTE: status dice results (onFire/poison/etc) are stored in
	// `currentCardOutcome.namedDiceResults["status_onfire"]` and
	// `currentCardOutcome.namedDiceResults["status_poison"]` respectively.

	// --- MENU UI VARIABLES ---
	ofRectangle mainMenuPlayAIButton;
	ofRectangle mainMenuMultiplayerButton;
	ofRectangle mainMenuSettingsButton;
	ofRectangle mainMenuQuitButton;
	int mainMenuHoveredIndex = -1;

	ofRectangle settingsBackButton;
	ofRectangle settingsResLeftButton, settingsResRightButton;
	ofRectangle settingsFramerateSlider;
	ofRectangle settingsFullscreenButton;
	int settingsHoveredIndex = -1;
	std::vector<glm::vec2> availableResolutions;
	int currentResolutionIndex = 0;
	std::vector<int> availableFramerates;
	float settingsFramerateSliderValue = 0.0f; // 0.0 = left (min), 1.0 = right (max)
	int currentFramerateIndex = 0;
	bool isFullscreen = false;

	ofRectangle pauseMenuResumeButton;
	ofRectangle pauseMenuSettingsButton;
	ofRectangle pauseMenuQuitButton;
	int pauseMenuHoveredIndex = -1;

	// Pause menu Save/Load buttons
	ofRectangle pauseMenuSaveButton;
	ofRectangle pauseMenuLoadButton;

	// Save browser UI (lists previous saves)
	void drawSaveBrowser();
	ofRectangle saveBrowserBackButton;
	int saveBrowserHoveredIndex = -1;
	std::vector<std::string> saveFilePaths;
	std::vector<ofRectangle> saveFileRects;
	// Save Browser selection/confirmation (migrated into `networkPending`)
	ofRectangle saveBrowserConfirmLoadButton;
	ofRectangle saveBrowserConfirmCancelButton;
	// Where to return after closing the save browser (e.g., pause menu or singleplayer menu)
	GameState saveBrowserReturnState = STATE_PAUSED;

	// --- DICE & SOUND ---
	std::vector<DiceRoll> activeDiceRolls;
	// Host waits for connected clients to confirm they've loaded the board before starting initiative
	bool hostWaitingForClientsReady = false;
	std::set<uint32_t> clientsReady;
	// Client-side flag: have we sent our ready signal to the host?
	bool clientSentReady = false;

	// Suspend/resume game when window is inactive (singleplayer only)
	bool gameSuspendedDueToInactivity = false;
	float savedMasterVolume = 1.0f; // store previous master volume when suspending
	// Remember whether main menu music was playing when suspending
	bool savedMainMenuWasPlaying = false;
	bool musicMutedDueToMinimize = false; // Track if music is intentionally muted (not stopped)
	// Saved per-player audio volumes when suspending
	float savedMainMenuVolume = 0.6f;
	std::vector<float> savedFootstepVolumes;
	// Saved playback position (ms) for main menu music
	int savedMainMenuPositionMS = 0;
	float diceSpinSpeed = 1500.0f;
	ofMesh d6Mesh, d4Mesh, d20Mesh, d10Mesh, coinMesh;
	ofTexture d6Texture, d4Texture, d20Texture, d10Texture, coinFacesTexture;
	std::vector<ofSoundPlayer> footstepSounds;

	// Dice roll result display
	std::string diceRollResultText = "";
	float diceRollResultStartTime = 0.0f;
	float diceRollResultDuration = 3.5f; // How long to show the result

	// --- DEBUG ---
	bool isDebugMode = false;
	bool isSpawningUnit = false;
	bool hasUnlimitedAP = false;
	bool skipChecksumValidation = false; // When true, host/client don't validate checksums (for testing)
	enum DebugSpawnMode {
		DEBUG_SPAWN_NONE = 0,
		DEBUG_SPAWN_PLAYER1,
		DEBUG_SPAWN_PLAYER2,
		DEBUG_SPAWN_FULL_DECK
	};
	DebugSpawnMode debugSpawnMode = DEBUG_SPAWN_NONE;
	ofRectangle debugPanel;
	ofRectangle debugDiceDropdownButton;
	ofRectangle debugRollD6Button;
	ofRectangle debugRollD4Button;
	ofRectangle debugRollD20Button;
	ofRectangle debugRollD10Button;
	ofRectangle debugFlipCoinButton;
	ofRectangle debugSpawnUnitButton;
	ofRectangle debugSpawnCardButton;
	ofRectangle debugAddAllCardsButton;
	ofRectangle debugDrawCardButton;
	ofRectangle debugUnlimitedAPButton;
	ofRectangle debugUnlimitedTimeButton;
	ofRectangle debugSkipDraftButton;
	ofRectangle debugForceEndTurnButton;
	ofRectangle debugSpawnPlayer1Button;
	ofRectangle debugSpawnPlayer2Button;
	std::vector<ofRectangle> debugP1PlusButtons;
	std::vector<ofRectangle> debugP1MinusButtons;
	std::vector<ofRectangle> debugP2PlusButtons;
	std::vector<ofRectangle> debugP2MinusButtons;
	std::vector<Card> debugSavedP1Deck;
	std::vector<Card> debugSavedP1Discard;
	std::vector<Card> debugSavedP2Deck;
	std::vector<Card> debugSavedP2Discard;
	bool hasDebugSavedP1State = false;
	bool hasDebugSavedP2State = false;
	bool isDebugDiceDropdownOpen = false;

	// --- Debug Card Spawner UI (KRunner-style) ---
	bool isCardSpawnerOpen = false;
	bool isCardEncyclopediaOpen = false;
	std::string cardSpawnerInput = "";
	int cardSpawnerQuantity = 1;
	std::vector<Card> filteredCards; // Cards matching current input
	int encyclopediaScrollOffset = 0;
	int settingsControlsScrollOffset = 0;
	ofRectangle cardSpawnerInputRect;
	ofRectangle cardSpawnerPlusButton;
	ofRectangle cardSpawnerMinusButton;
	ofRectangle cardSpawnerEncyclopediaButton;
	ofRectangle cardSpawnerCloseButton;
	ofRectangle encyclopediaCloseButton;
	ofRectangle encyclopediaRect;

	// Encyclopedia modal mode for debug +/- carddeck/carddiscard
	enum EncyclopediaMode {
		ENC_NONE = 0,
		ENC_SPAWN_TO_HAND, // Card spawner encyclopedia: selected cards go to current hand on Accept
		ENC_ADD_FROM_ALL, // + : show allCards to add to target pile
		ENC_REMOVE_FROM_PILE // - : show target player's pile to remove cards from
	};
	EncyclopediaMode encyclopediaMode = ENC_NONE;
	// When the encyclopedia modal is opened for debug, which player index is targeted
	int encyclopediaTargetPlayerIndex = -1;
	// Whether the modal is operating on the player's discard (true) or deck (false)
	bool encyclopediaTargetIsDiscard = false;
	// indices (into the currently displayed list) selected by the user
	std::vector<int> encyclopediaSelectedIndices;
	// Accept button rect for the modal
	ofRectangle encyclopediaAcceptButton;
	// Hover tracking for encyclopedia cards
	int encyclopediaHoveredIndex = -1;
	float encyclopediaHoverStartTime = 0.0f;
	bool encyclopediaHoverScaled = false;

	// We still load the sheet to generate the data
	ofImage cursorSheet;

	// Store pointers to the OS cursors
	GLFWcursor * glfwArrow = nullptr;
	GLFWcursor * glfwHandPoint = nullptr;
	GLFWcursor * glfwHandOpen = nullptr;
	GLFWcursor * glfwHandClosed = nullptr;

	CursorState currentCursor = CURSOR_DEFAULT;
	CursorState previousCursor = CURSOR_DEFAULT; // To track changes

	// --- CHAT SYSTEM ---

	// Card draw animation system
	std::vector<DrawCardAnimation> activeDrawCardAnimations;
	// Animations for cards moving from hand -> discard (visual only)
	std::vector<DrawCardAnimation> activeDiscardCardAnimations;

	// Visual shuffle animation played when discard is reshuffled into deck
	struct ShuffleAnimation {
		int playerIndex = -1; // 0 or 1
		ofRectangle deckRect;
		float startTime = 0.0f;
		float duration = 0.9f;
		float currentAlpha = 255.0f;
		float currentScale = 1.0f;
		float rotation = 0.0f;
	};

	std::vector<ShuffleAnimation> activeShuffleAnimations;
	struct ChatMessage {
		std::string playerName;
		std::string message;
		float timestamp; // For fade out
	};
	struct GameLogEntry {
		std::string text;
		float timestamp;
	};
	enum class ChatTab { CHAT,
		LOG,
		DEBUG };

	std::vector<ChatMessage> chatHistory;
	std::vector<GameLogEntry> gameLog;
	bool isChatOpen = false;
	bool isChatMinimized = true; // True = minimized, False = full size
	std::string chatInput = "";
	float chatScrollOffset = 0;
	float lastChatInteractionTime = -999.0f; // When chat was last opened/closed/message received
	ChatTab currentChatTab = ChatTab::CHAT;
	ofRectangle chatWindowRect; // For click detection
	const int maxChatMessages = 50;
	const int maxLogEntries = 100;
	const float chatMessageLifetime = 10.0f; // Seconds before old messages fade
	const float chatVisibilityDuration = 5.0f; // Seconds to show chat after interaction
	const int maxChatInputLength = 150;

	void addGameLog(const std::string & logText);

	// --- HOVER GLOW SYSTEM ---
	enum HoverType { HOVER_NONE = 0,
		HOVER_UNIT = 1,
		HOVER_DECK = 2,
		HOVER_DISCARD = 3,
		HOVER_HAND_CARD = 4,
		HOVER_UNIT_SELECTED = 5 };

	// Local hover state
	HoverType localHoverType = HOVER_NONE;
	int localHoverGridX = -1;
	int localHoverGridY = -1;
	int localHoverCardIndex = -1;

	// Opponent hover state
	HoverType opponentHoverType = HOVER_NONE;
	int opponentHoverGridX = -1;
	int opponentHoverGridY = -1;
	int opponentHoverCardIndex = -1;

	void updateAndSendHover(HoverType type, int gridX = -1, int gridY = -1, int cardIndex = -1);
	void drawTileGlow(int gridX, int gridY, ofColor color, float thickness = 0.15f);

	// Networking / player helpers
	int getLocalPlayerIndex() const; // returns index in `players` or -1
	bool isLocalDraftingPlayer(int draftIndex) const;
};
