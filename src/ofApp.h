#pragma once
#define GLFW_INCLUDE_NONE

#include "GLFW/glfw3.h"
#include "NetworkData.h"
#include "SteamManager.h"
#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include <zmq.hpp>
// --- Standard Library Includes ---
#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

enum CardGlowState {
	CARD_GLOW_NONE,
	CARD_GLOW_GREEN,
	CARD_GLOW_YELLOW
};

enum GameState {
	STATE_MAIN_MENU,
	STATE_MULTIPLAYER_MENU,
	STATE_SETTINGS,
	STATE_GAMEPLAY,
	STATE_WAITING_FOR_RECONNECT,
	STATE_PAUSED,
	STATE_INITIATIVE_ROLL,
	STATE_DRAFTING,
	STATE_SINGLEPLAYER_MENU,
	STATE_SAVE_BROWSER,
	STATE_ENCYCLOPEDIA,
	STATE_CUSTOMISATION, // <--- New
	STATE_DESYNC
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
	CARD_PLAY_RESULT_IMMEDIATELY, // Send packet NOW in mouseReleased
	CARD_PLAY_RESULT_AWAITING_MENU_CHOICE, // Menu will send packet after user chooses
	CARD_PLAY_RESULT_AWAITING_TARGETING, // Targeting handler will send packet after player targets
	CARD_PLAY_RESULT_CANCELLED, // User cancelled, don't send packet
	CARD_PLAY_RESULT_NOT_PLAYABLE, // Cost/validation failed, don't send packet
	CARD_PLAY_RESULT_AWAITING_DICE // Dice will trigger packet when resolved
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

// Forward-declare StatusType so `struct Card` can reference it before the
// full enum definition appears later in this header.
enum StatusType : int;

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
	CARD_CONSUME_HEALTH_FLAGON,
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
	CARD_SUMMON_MAGIC_WALL,
	PSEUDO_CARD_GHOST_RELOCATE // pseudo-card for ghost relocation UI
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
	CARD_INTERACTION_STATE_IDLE, // No card interaction in progress
	CARD_INTERACTION_STATE_TARGETING, // Waiting for player to click target on board
	CARD_INTERACTION_STATE_MENU, // Waiting for player to choose menu option
	CARD_INTERACTION_STATE_STATUS, // Waiting for status selection (Dispel only)
	CARD_INTERACTION_STATE_PLACING // Waiting for placement click (Wolves, Kobolds)
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

inline constexpr float kCardPixelWidth = 409.0f;
inline constexpr float kCardPixelHeight = 585.0f;
inline constexpr float kHandCardVisualScale = 0.55f;

struct DefenseRecord {
	int statType;
	int amount;
	int expirationCycle;
};

struct Token {
	std::string text;
	bool bold;
	float w;
};
struct Line {
	std::vector<Token> toks;
	float width = 0;
};

struct UILayoutSpacing {
	float edgeInset = 0.0f; // outer edge inset for gameplay HUD anchoring
	float stackYOffset = 0.0f; // downward visual nudge for deck/discard stack
	float stackVerticalGap = 0.0f; // vertical spacing between discard/deck cards
	float minionEntryGapUnscaled = 12.0f; // logical row gap before per-resolution scale
	float minionIconGap = 0.0f; // spacing between minion deck/discard icons
	float timerBarHeight = 0.0f; // reserved top band for turn timer
	float chatInset = 0.0f; // default chat inset from top/left edge
	float healthBarSideGap = 0.0f; // gap between deck and health bar
	float healthBarInwardNudge = 0.0f; // extra nudge to avoid overlap
};

struct HandLayout {
	float cardW = kCardPixelWidth * kHandCardVisualScale;
	float cardH = kCardPixelHeight * kHandCardVisualScale;
	float spacing = 0.0f;
	float totalWidth = 0.0f;
	float startX = 0.0f;
	float restY = 0.0f;
	ofRectangle handAreaRect;
};

struct CardTemplateRecord {
	std::string name;
	int index = -1; // numeric index parsed from markdown heading (e.g., '## 4. Bash')
	std::string apCost;
	std::string damageType;
	std::string targeting;
	std::string classLabel;
	std::string effectText;
	std::string picture;
	std::string summonAP;
	std::string summonHP;
};

struct CardTemplateLayout {
	ofRectangle pictureRect = ofRectangle(80, 96, 896, 704);
	ofRectangle nameRect = ofRectangle(192, 740, 672, 144);
	ofRectangle costRect = ofRectangle(32, 32, 128, 128);
	ofRectangle damageTypeRect = ofRectangle(26, 84, 176, 24);
	ofRectangle targetingRect = ofRectangle(384, 1344, 320, 80);
	ofRectangle summonAPRect = ofRectangle(384, 1328, 136, 80);
	ofRectangle summonHPRect = ofRectangle(552, 1328, 136, 80);

	ofRectangle classRect = ofRectangle(26, 112, 150, 24);
	ofRectangle effectRect = ofRectangle(96, 928, 864, 384); // Restored strictly to original visual bounds
	float nameScale = 13.75f;
	float nameMinScale = 1.0f;
	float nameCurveDropPx = 12.0f;
	float nameMiddleClampXMin = 416.0f;
	float nameMiddleClampXMax = 656.0f;
	float nameMiddleBottomMaxY = 864.0f;
	float costScale = 3.0f;
	float labelScale = 1.0f;
	float effectScale = 4.0f; // max preferred scale; auto-fit may reduce per card
	float effectMinScale = 1.0f; // floor for very long text
	float effectLineSpacing = 0.75f; // Keep tight line spacing so text can still scale up within bounds
};

struct PreviewLabel {
	glm::vec2 screenPos;
	std::string text;
	ofColor color;
};

// --- INTEGER MATH GRID (Multiply by 1000) ---
struct TileFaceInt {
	long long x, y;
	int nx, ny;
};

struct CandidatePlay {
	int cardIndex;
	int tx;
	int ty;
	double score;
};
struct SavedTileState {
	bool isTargetPreview;
	bool isTargetable;
	bool hasTooltipInfo;
	int minRollRequired;
	float hitChance;
	bool isAoeCenter;
	int aoeRadiusFeet;
};

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
	int rangeDiceNum = 0;
	int rangeDiceSides = 0;
	int utilityDiceNum = 0;
	int utilityDiceSides = 0;
	int summonDiceNum = 0;
	int summonDiceSides = 0;
	int summonKind = 0;
	int baseDamage = 0;
	int damageDiceNum = 0;
	int damageDiceSides = 0;
	int aoeRadiusDiceNum = 0;
	int aoeRadiusDiceSides = 0;
	int statusDuration = 0;
	int healAmount = 0;
	DamageType damageType = DAMAGE_PHYSICAL;
	int cost = 1;

	// Data-driven effect fields (Phase 1 additions)
	int apGain = 0;
	int drawCount = 0;
	int discardHandCount = 0; // number of random cards to discard from hand
	int discardDeckCount = 0; // number of top cards to burn from deck
	int applyStatus = 0; // Will hold StatusType enum
	bool isAoe = false;
	bool isHandRelated = false;

	// --- Additional data-driven fields (aligned with Master AI spec)
	// Damage & Healing
	int baseHeal = 0;
	int healDiceNum = 0;
	int healDiceSides = 0;

	// Stat Buffs
	int blockGain = 0;
	int wardGain = 0;
	int barrierGain = 0;
	int holyBlockGain = 0;
	int fortificationGain = 0;
	int maxHealthGain = 0;
	int luckGain = 0;
	int apGainThisTurn = 0;
	int apGainNextTurn = 0;

	// Deck Manipulation
	int destroyDeckTargetCount = 0; // number of top cards to destroy from target's deck

	// Generic defensive stat gains (data-driven)
	int blockAmount = 0;
	int wardAmount = 0;
	int barrierAmount = 0;
	int holyBlockAmount = 0;
	int fortifyAmount = 0; // increases fortification stat

	// Data-driven variant / HP derivation support (Phase 2+)
	ofJson variantDefs; // optional: array of variant objects loaded from JSON
	std::string hpDerivedFromUnitType = ""; // e.g. "KOBOLD"
	int hpDerivedAdd = 0; // amount to add to derived HP

	TargetingType targeting = TARGET_ANY_TILE;
	bool drawnThisTurn = false; // Add this
	bool playedThisTurn = false; // True if this specific card instance was played this turn
	bool isAnimating = false; // True while a visual-only animation is running for this card
	bool isCopied = false;
	int cardClass = 1;
};

struct ActionHistoryEntry {
	std::string cardName;
	Card card;
	int playerID;
	int rangeRoll = -1;
	int damageRoll = -1;
	int utilityRoll = -1;
	std::string menuChoice = "";
	std::vector<std::string> destroyedCardNames;

	// Movement tracking parameters
	bool isMovement = false;
	int fromX = -1;
	int fromY = -1;
	int toX = -1;
	int toY = -1;
	std::vector<glm::vec2> movementPath;
	int actorPlayerID = -1; // Use stable playerID instead of transient actorIndex

	// Spell coordinate parameters
	int casterX = -1;
	int casterY = -1;
	int targetX = -1;
	int targetY = -1;
	bool hasTracers = false;
};

// ===================================================================================================
// EFFECT QUEUE SYSTEM - centralized card execution pipeline
// ===================================================================================================
// ============================================================================
// DATA-ORIENTED DETERMINISTIC EFFECT SYSTEM
// ============================================================================

enum class EffectOpType : uint8_t {
	NONE = 0,
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
	SET_REPLICATE_QUEUED,
	CONDITIONAL_BRANCH,
	APPLY_FIREBALL,
	APPLY_POISON,
	APPLY_AMNESIA,
	APPLY_DEATH,
	APPLY_ON_FIRE,
	APPLY_ON_FIRE_RESOLVE,
	APPLY_FIRE_HIT_RESOLVE,
	APPLY_PARALYSIS,
	APPLY_WOLF_COIN,
	APPLY_BLOCKING_BOON_COIN,
	APPLY_BLOCKING_BOON_D20,
	APPLY_INITIATIVE_REROLL,
	APPLY_SLEEP_DURATION,
	APPLY_BONUS_AP,
	APPLY_MAGIC_BLAST,
	APPLY_MAGIC_BLAST_MENU,
	APPLY_MAGIC_BOLT,
	APPLY_MAGIC_BOLT_PRIMARY,
	APPLY_MAGIC_BOLT_AOE,
	APPLY_MAGIC_BOLT_PRIMARY_RESOLVE,
	APPLY_MAGIC_BOLT_AOE_RESOLVE,
	APPLY_TIME_VORTEX,
	APPLY_SPARK_OF_GENIUS,
	APPLY_BARRIER,
	APPLY_ATTACK,
	APPLY_ATTACK_RESOLVE,
	APPLY_PSIONIC_WAVE,
	APPLY_ETHEREAL_JOLT,
	APPLY_TELEPORT,
	APPLY_SHOOT_ARROW,
	APPLY_SHOOT_ARROW_DAMAGE,
	APPLY_CHAIN_LIGHTNING,
	APPLY_CHAIN_LIGHTNING_DAMAGE,
	APPLY_CHAIN_LIGHTNING_DAMAGE_RESOLVE,
	APPLY_MAGIC_HAND_DAMAGE,
	APPLY_DRAIN_PUNCH_RESOLVE,
	APPLY_VAMPIRE_BITE_RESOLVE,
	APPLY_FLAIL_DAMAGE_RESOLVE,
	APPLY_EARTHQUAKE,
	APPLY_EARTHQUAKE_DAMAGE,
	APPLY_GENERIC_DAMAGE,
	APPLY_GENERIC_HEAL,
	APPLY_GENERIC_STATUS,
	APPLY_GENERIC_DISCARD,
	WAIT_VISUAL
};

// NOTE: `RollDiceData` removed — dice are resolved at decision time and
// written into `EffectSequence.blackboard` directly. The ROLL_DICE effect (legacy) is gone.
// op and related input command were removed as part of the deterministic
// lockstep migration; do not reintroduce deferred roll ops.

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
	int variant; // optional variant flag (e.g., golem type: 0=base,1=rock,2=fire,3=electric)
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
	bool isSteal;
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
	STATUS_ASSISTANT_REROLL_USED = 14,
};

struct StatusData {
	int targetIndex;
	int statusType;
	int duration; // optional
};

struct ReplicateQueuedData {
	int targetIndex;
	bool enabled;
};

struct EffectOp {
	EffectOpType type;
	union {
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
		ReplicateQueuedData replicateQueued;
		SpawnUnitData spawnUnit;
		CreateWallData createWall;
		SpawnPlayerData spawnPlayer;
		// Vampire bite resolver: compare pre-damage HP and queue follow-up ops
		struct {
			int targetIndex;
			int preHP;
			int healAmount;
			int addCardType; // CardType enum or -1
		} vampireResolve;
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

	// Tile preview payloads (visual-only; do NOT modify gameplay state)
	std::vector<glm::ivec2> tilePreviewAdds; // tiles to add to visual-only preview
	bool clearTilePreviewsOnComplete = false; // clear previews when this event completes

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
	CARD_PLAY_STATE_IDLE = 0, // No card action in progress
	CARD_PLAY_STATE_MENU = 1, // Waiting for user menu choice (Wisdom Boon, Dispel, etc)
	CARD_PLAY_STATE_TARGETING = 2, // Waiting for user to select target
	CARD_PLAY_STATE_DICE = 3, // Waiting for dice roll result
	CARD_PLAY_STATE_EFFECT = 4, // Effect is being applied (animation/movement)
	CARD_PLAY_STATE_EFFECT_SEQUENCE = 5, // Processing lockstep effect sequence
	CARD_PLAY_STATE_OUTCOME = 6, // Effect complete, ready to send outcome packet
	CARD_PLAY_STATE_FINISHED = 7 // Card action finished, sent to network
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
	int wardGained = 0;
	int barrierGained = 0;
	int holyBlockGained = 0;
	int fortificationGained = 0;
	int maxHpGained = 0;
	std::vector<std::string> statusesApplied; // "onFire", "isParalyzed", etc
	std::vector<int> unitsMovedBy; // Distance each unit moved for earthquake, etc
	std::vector<glm::ivec2> wallsCreated; // Positions where walls were built
	std::vector<int> minionsSpawned; // Indices of newly spawned minions

	// For cards that destroy a card as part of resolution (e.g. Shoot Arrow)
	CardType destroyedCardType = CARD_NONE;

	// Attack-specific outcome data
	DamageType attackDamageType = DAMAGE_PHYSICAL;
	std::vector<int> attackTargetIndices; // indices into `players`
	std::vector<int> attackTargetPlayerIDs; // stable playerIDs for queued resolve

	// Poison targets stored as stable playerIDs for later resolution
	std::vector<int> poisonTargetPlayerIDs;

	// Summon kind for summon cards (mapped from PendingSummonKind)
	int summonKind = 0;

	// Owner playerID for summoned minions (captured at play-time)
	int summonOwnerPlayerID = -1;

	// State tracking
	bool isComplete = false; // Ready to send outcome packet
	// Whether AP cost for this card has already been deducted (pre-paid)
	bool apPaid = false;
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
	int pendingHandIndex = -1; // index of the already-added hand card when commitOnFinish == false
	float startTime;
	float duration;
	float currentAlpha = 255.0f;
	float currentScale = 1.0f; // visual scale multiplier used during animation
	float startScale = 1.0f; // scale at animation start (deck-size visual)
	float endScale = 1.0f; // scale at animation end (hand-size visual)
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
	// Deprecated: visual-only previews are now stored in `activeYellowPreviewTiles`.
	bool visited = false;
	glm::vec2 parent = { -1, -1 };

	// Target square tooltip info (for cards with range checks)
	int minRollRequired = 0; // Minimum dice roll needed to hit this square
	float hitChance = 0.0f; // Percentage chance to hit (0.0 to 1.0)
	bool hasTooltipInfo = false; // True if tooltip data is valid for this tile

	// AOE preview helpers
	bool isAoeCenter = false; // True if this tile is treated as an AOE center for preview
	int aoeRadiusFeet = 0; // Radius in feet used for tooltip/preview calculations
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
	int fireApplierPlayerID = -1;
	int poisonApplierPlayerID = -1;

	// Status & Buffs
	int nextTurnAPBonus = 0;
	int shocksPlayedThisTurn = 0;
	int flurryOfFistsStacks = 0; // Number of Flurry stacks: each stack doubles hand-related effects and drawn-card counts
	int freeHandCardTurns = 0; // Number of turns remaining where hand-related cards cost 0 AP
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
	int defenseCycle = -1; // Tracks when shields were applied so they last a full round!

	// Tortoise Form
	bool inTortoiseForm = false;
	int tortoiseDamageTaken = 0; // Tracks HP damage while in form, ends at 5
	Card tortoiseFormCard; // The card to discard when form ends
	std::string originalModelType = ""; // To restore original model
	// Accumulated tortoise pending damage (0 = none). Replaces prior
	// `pendingTortoiseDamage` (bool) + `pendingTortoiseDamageValue` (int).
	int storedDarkShieldDice = 0;

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

struct ActionHistoryEntry;
struct DefenseRecord;

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

	// --- MOVED FROM ofApp.cpp ---
	std::map<int, long long> s_pendingRemoteChecksums;
	std::map<int, long long> s_pendingLocalChecksums;
	std::map<int, int> s_playerDeathDelayMap;
	std::vector<std::string> s_fullMatchLog;
	std::deque<int> g_opponentDecisionQueue;
	std::set<uint64_t> skippedOptimisticCommands;
	int s_winStreak = 0;
	float s_gameOverScreenStartTime = 0.0f;
	int s_startingLevel = 1;
	int s_startingXP = 0;
	int s_xpGained = 0;
	bool s_hasCachedGameOverVisuals = false;
	int magicHandRelocateTargetIndex = -1;
	std::vector<glm::ivec2> magicHandRelocateChoices;
	std::vector<glm::ivec2> activeYellowPreviewTiles;
	std::vector<std::pair<glm::ivec2, glm::ivec2>> activeCombinedPierceTargets;
	std::vector<ActionHistoryEntry> g_actionHistory;
	int s_hoveredHistoryIndex = -1;
	float g_menuAlphaMult = 1.0f;
	uint32_t s_opponentRiMask = 0;
	int g_windowModeState = 1;
	ofxAssimpModelLoader staffModel;
	aiNode * cachedPlayerHandNode = nullptr;

	// --- AUDIO SLIDER DRAG STATE ---
	bool draggingAudioMaster = false;
	bool draggingAudioMenu = false;
	bool draggingAudioSfx = false;
	bool draggingFramerateSlider = false;
	void drawMinionCard(int minionIndex, int ownerIndex);
	void setup();
	void update();
	void draw();

	void updateGameLogic();
	void prepareGameVisualState();

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
	void sendPlaceSummonedMinion(int minionType, int ownerPlayerID, int targetX, int targetY, int minionHP, int minionAP, int minionPlayerID);

	// Generic card action begin helper
	void sendCardActionBegin(int cardType, int actorIndex, int targetX, int targetY, int p0 = 0, int p1 = 0, int p2 = 0, int p3 = 0, const std::string & label = "");
	void sendActionPacket(int cardIndex, int tx, int ty, int cost, int menuChoice = 0, const std::string & cardNameOverride = "");
	void sendMagicHandResolutionPacket(int choice);
	void sendMenuState(int menuType, int targetIndex, int hoveredChoice, int cardIndex);

	long long calculateChecksum();

	// Helper for external harness to load a save and print its checksum
	bool harnessLoadAndPrintChecksum(const std::string & path);

	// Harness helper: auto-advance `turns` turns by submitting deterministic END_TURN commands.
	// Useful for checksum simulations in headless mode.
	void harnessAutoAdvanceTurns(int turns);

	// Reliable send tracking for legacy packets removed: lockstep commands used instead
	void sendSnapshotToClient(bool useTurnStartBackup = false);
	std::string buildSnapshotString();
	void applySnapshotString(const std::string & data, bool fromNetworkSnapshot = true);

	// Anti-cheat: Log deck states for verification
	void logDeckStates(const std::string & reason);
	std::string getDeckStateString(const Player & p);

	void addTimeBonusToTurn(int actorIndex, int seconds);
	float lastHandshakeRequestTime = 0.0f;
	float handshakeRequestInterval = 1.0f;
	uint32_t lastObservedLobbySeed = 0;
	float lastLobbySeedLogTime = 0.0f;
	uint32_t currentMapSeed = 0;

	// Steam
	SteamManager steamManager;

	// Multiplayer & AI state
	bool isMultiplayer = false;
	bool isVsAI = false; // True if playing against the bot
	bool isAIvsAI = false; // True for hyper-speed self-play training
	float aiThinkTimer = 0.0f; // Creates a delay so the AI doesn't play 5 cards in 1 frame
	int myLocalPlayerID = 0; // 0 for host/player1, 1 for client/player2

	// AI DRL Integration Functions
	void updateAI();
	std::vector<float> extractGameStateForAI();
	int getAIActionFromModel(const std::vector<float> & state, float reward, bool done);
	void executeAIAction(int actionIndex);

	// ZeroMQ connection
	zmq::context_t * zmqContext = nullptr;
	zmq::socket_t * zmqSocket = nullptr;
	bool zmqConnected = false;
	float cumulativeReward = 0.0f; // Tracks reward to send to Python

	// --- Phase 2: XOR Handshake & Elo ---
	uint32_t localSeedComponent = 0;
	bool waitingForClientHandshake = false;

	int myElo = 1000;
	int opponentElo = 1000;
	int eloChange = 0;
	bool eloCalculated = false;

	// When true, skip all rendering/texture operations for headless smoke tests
	bool headless = false;
	bool hasReceivedHandshake = false;
	std::string player0SteamName = "Player 1";
	std::string player1SteamName = "Player 2";
	uint32_t lastSnapshotId = 0;
	std::string incomingSnapshotBuffer;
	uint32_t incomingSnapshotId = 0;
	uint32_t incomingSnapshotExpectedSize = 0;
	uint32_t incomingSnapshotReceivedSize = 0;

	bool hasAssistantRerollAvailable(int playerIndex, int atX, int atY);

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

	// Pre-calculated weapon socket offset
	glm::mat4 staffSocketOffset;
	// Caches the hand's un-animated position from Blender
	glm::mat4 handBindPoseWorldMatrix;

private:
	// -------------------------------------------------------------------------
	//                              CORE SYSTEMS
	// -------------------------------------------------------------------------

	// Last AP roll parameters (used for assistant auto-reroll)
	int lastAPDiceNum = 0;
	int lastAPDiceSides = 0;
	// Raw faces from the most recent AP roll (host stores these to publish visuals)
	std::vector<int> lastAPRawResults;
	// True when AP for the current player has already been deterministically resolved
	bool apResolvedThisTurn = false;
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

	// Decoupled update helpers (extracted from monolithic `update()`)
	void updateNetwork();
	void updateAudio();
	void updateVisuals();
	void updateStateMachine();

	void startNewTurn();
	void continueNewTurn();

	// Apply end-of-turn processing for the current player and advance to next
	void performEndTurnAdvance();

	void drawMainMenu();
	void drawSettingsMenu();
	void drawPauseMenu();
	void updateAudioVolumes();
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
	void processCardStateInput(int mouseX, int mouseY, int button); // Handle clicks during card states
	void applyAmnesiaSelectionLocal(int targetPlayerIndex, const std::vector<int> & selections);
	void updateCardStateMachine(); // Called in update() to process state transitions
	void advanceCardState(CardPlayState newState); // Transition to new state
	void applyCardOutcomeEffects(); // Apply the completed outcome to game state
	void handleCardTargetInput(int gridX, int gridY); // Target selected
	void handleCardDiceResult(int result, DicePurpose purpose); // Dice roll completed
	bool executeCardByType(const Card & playedCard, int cardIndex, int targetX, int targetY, bool & playedSuccessfully, CardPlayResult & immediateResult); // centralized execution entry (incremental migration)
	// Generic handler for data-driven cards. Returns true if handled.
	bool executeCardGeneric(const Card & playedCard, int cardIndex, int targetX, int targetY, bool & playedSuccessfully, CardPlayResult & immediateResult);

	// --- Async Resolution Helpers (centralized) ---
	// Centralized handlers for dice/state resolution after card play. Most legacy
	// per-frame resolver functions have been migrated into the EffectOp pipeline
	// (see EffectOpType handlers in `src/ofApp.cpp`). The remaining helpers are
	// implemented inline at call sites or as EffectOp handlers; legacy prototypes
	// were removed during the "Big Cleanup" migration.
	void updateEarthquakeSimulation();

	// Simple inline dice resolver for legacy inline uses (sums N dS)
	int resolveDiceRoll(int numDice, int sides);

	void applyReplicateCopyToHand(Player & caster, const Card & playedCard);
	void finishPlayCard(Player & caster, const Card & playedCard, int handIndex);
	void completeCardPlayAnimation(const Card & playedCard, int playerIndex);
	int applyDamageWithMitigations(Player & target, int baseDamage, DamageType type, int attackerIndex);
	// Queue-only variant: queues MODIFY_STAT ops for absorptions/HP and writes applied amount into currentEffectSequence.blackboard[outputSlot]
	void applyDamageWithMitigationsQueued(Player & target, int baseDamage, DamageType type, int attackerIndex, int outputSlot);

	// Deterministic centralized minion spawn helper. Creates and inserts a minion
	// with flags, deck, and deterministic shuffle so host/client stay in lockstep.
	// Returns a pointer to the inserted Player in `players` or nullptr on failure.
	Player * spawnMinionDeterministically(int summonKind, int targetX, int targetY, int ownerID, int maxHP, int ap, int summonerPlayerID = -1);

	// Build a Player template for a summoned minion of `summonKind`.
	// Does not insert into `players` or place on board. Uses `maxHP` when >0,
	// otherwise picks sensible defaults. Consumes gameplay RNG when needed.
	Player initMinionFromKind(int summonKind, int ownerID, int maxHP, int ap, int summonerPlayerID = -1);

	void updatePlayerAP(Player & player, int newAP);
	void applyMovement(int playerIndex, int targetX, int targetY, int newAP, const std::vector<glm::vec2> * pathOverride = nullptr);
	void createCardDisplay(const Card & card, int playerIndex, bool forceVisibleForAllPlayers = false); // Create card display animation
	std::string currentDiceLabel = "";
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
	bool hasValidTargetForGlow(int playerIndex, int cardIndex);
	// Return true when all active dice visuals are finished and the result linger time passed
	bool diceVisualsFinishedAndLinger() const;
	void recalcTempLuck();
	void checkKeyPickupAndDraftAfterSummon(int x, int y, int minionOwnerID, int preferredPlayerIndex = -1);

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

	// --- DETERMINISTIC RNG ---
	// The synced Random Number Generator
	std::mt19937 gameplayRNG;
	uint64_t gameplayRngAdvanceCount = 0;

	uint32_t consumeGameplayRngRaw() {
		++gameplayRngAdvanceCount;
		return gameplayRNG();
	}

	// Visual RNG (local only, not part of deterministic gameplay)
	std::mt19937 visualRNG;

	// Networked multi-step action helpers
	// (Moved into `networkPending.actionByActor`, `networkPending.networkActionActor`, etc.)

	// Flag set when the host-provided gameplay seed has been applied
	bool gameplaySeededByHost = false;

	// Desync message shown when checksum fails
	std::string desyncMessage;

	// If >0, client has requested a snapshot from host and is awaiting it.
	// Store the request start time so we can implement timeouts/retries.
	float waitingForSnapshotStartTime = 0.0f;
	// True while processing an incoming network packet; used to enforce "Zombie Client" rule
	bool processingNetworkPacket = false;
	// Timestamp of last snapshot request to avoid spamming (seconds)
	float lastSnapshotRequestTime = 0.0f;
	// Client: draft handling updated for deterministic local shuffles.

	// Helper to get synced numbers
	int getGameRandom(int min, int max);

	// Shuffle a vector deterministically using the synchronized `gameplayRNG`.
	// In deterministic lockstep mode both peers perform the same local shuffle
	// so there is no reliance on host-authoritative shuffle packets or nonces.
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
	void deterministic_shuffle_gameplay(std::vector<T> & vec) {
		if (vec.size() <= 1) return;
		for (size_t i = vec.size() - 1; i > 0; --i) {
			auto r = consumeGameplayRngRaw();
			size_t j = static_cast<size_t>(r % (i + 1));
			std::swap(vec[i], vec[j]);
		}
	}
	template <class T>
	void shuffleGameVector(std::vector<T> & vec, int ownerPlayerIndex = -1, float visualDelaySeconds = 0.0f) {
		// Unified deterministic shuffle: always use the synchronized `gameplayRNG`
		// so host and client consume RNG in the same order. Prior hybrid
		// behavior that broadcast nonces or skipped local shuffles has been
		// removed to enforce pure lockstep.
		deterministic_shuffle_gameplay(vec);

		// Start visual shuffle only for main players (players 0 and 1).
		if (ownerPlayerIndex == 0 || ownerPlayerIndex == 1) {
			startShuffleVisual(ownerPlayerIndex, visualDelaySeconds);
		}

		// If this shuffle was for a specific player's deck, clear the dirty flag
		if (ownerPlayerIndex >= 0 && ownerPlayerIndex < (int)players.size()) {
			players[ownerPlayerIndex].deckNeedsShuffle = false;
		}
	}

	// Card-specialized stable shuffle: sort by type/name before deterministic shuffle
	inline void shuffleGameVector(std::vector<Card> & vec, int ownerPlayerIndex = -1, float visualDelaySeconds = 0.0f) {
		if (vec.size() <= 1) return;
		// Stable sort to normalize deck ordering across clients before consuming RNG
		std::stable_sort(vec.begin(), vec.end(), [](const Card & a, const Card & b) {
			if (a.type != b.type) return a.type < b.type;
			return a.name < b.name;
		});

		// Now perform deterministic shuffle using gameplay RNG
		deterministic_shuffle_gameplay(vec);

		// Start visual shuffle only for main players (players 0 and 1).
		if (ownerPlayerIndex == 0 || ownerPlayerIndex == 1) {
			startShuffleVisual(ownerPlayerIndex, visualDelaySeconds);
		}

		// Clear dirty flag for specific player's deck
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
	int stringToStatusType(const std::string & str);

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

	struct LosResult {
		bool hasLos;
		glm::vec2 start;
		glm::vec2 end;
	};

	// Targeting Algorithms
	std::vector<Player *> findCleaveTargets(glm::vec2 direction);
	TargetInfo isLosTargetValid(glm::vec2 casterTile, glm::vec2 targetTile, float maxRangeFeet, CardType cardType);
	LosResult getClearLosRay(glm::vec2 casterTile, glm::vec2 targetTile, CardType cardType);
	// Integer-scaled squared face-to-face distance to avoid floating-point edge cases.
	// Coordinates are scaled by 2 (half-tile units) so face midpoints become integers.
	long long getFaceToFaceDistanceSquaredScaled(glm::vec2 casterTile, glm::vec2 targetTile);
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
	int initiativeTimerFrames = 0; // frame-based timer for deterministic initiative wait
	// Simulation/tick helpers for scheduling authoritative turn starts
	bool inSimulationTick = false;
	int pendingStartNewTurnRequests = 0;

	void requestStartNewTurn();
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
	// `waitingForDraftOptionsStartTime` > 0.0f indicates the client is waiting
	// for the host's authoritative DraftOptionsPacket; store the start time so
	// we can implement timeouts/retries without a per-frame boolean poll.
	float waitingForDraftOptionsStartTime = 0.0f; // When we began waiting (for timeout/retry)
	float waitingForDraftOptionsTimeout = 0.75f; // seconds to wait for host before giving up/requesting
	int skipClientShuffleFor = -1; // When >=0, client will skip the next deck shuffle for this player index (avoids RNG divergence from forwarded Accepts)
	bool draftAcceptLocked = false; // Prevent double-accept clicks per draft screen
	bool draftAcceptApplied = false; // Prevent duplicate forwarded Accept application
	uint32_t draftGenerationCounter = 0; // Increments each time generateDraftOptions is called to ensure variety
	// (Moved into `networkPending.draftStateAvailable` / `networkPending.draftState`)
	bool initialDraftComplete = false; // True once the initial (pre-game) draft finishes

	// Draft ACK/resend legacy helpers removed — draft actions now use deterministic commands.

	// Reliability helpers for client-sent input commands (resend until host ACK)
	InputCommandPacket lastSentActionPacket; // Last input command the client sent (for resend)
	bool lastSentActionValid = false;
	float lastSentActionTime = 0.0f;
	int lastSentActionResendCount = 0;
	const float ACTION_RESEND_INTERVAL = 0.75f;
	const int ACTION_MAX_RESENDS = 3;
	uint32_t actionClientActionCounter = 0; // monotonic id for action ACK matching

	// Generic client-local monotonic counter used by lightweight watchdog packets
	uint32_t watchdogClientActionCounter = 0;

	// Monotonic counter for client-originated draft actions (clientActionID)
	uint32_t draftClientActionCounter = 0;

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

	// UI helpers
	void getDraftCardMetrics(bool clampTop,
		float & outCardW, float & outCardH,
		float & outSpacing, float & outStartX, float & outStartY);

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
		int startFrame = 0;
		DraftOptionAnimState state = DRAFT_ANIM_IDLE;
		bool hidden = false;
	};

	std::vector<DraftOptionUI> draftOptionUI; // per-slot purely-visual animation state
	// Draft animation timings converted to frame counts for deterministic lockstep
	int draftAnimAppearFrames = 0; // computed at setup() from seconds * framesPerSecond
	int draftAnimHoldFrames = 0;

	// Accept button UI animation (uses same DraftOptionUI for simplicity)
	DraftOptionUI draftAcceptUI;
	int draftAnimVanishFrames = 0; // small number of frames for vanish

	// Visual scheduling for picked-card movement into deck (frame-based)
	struct DraftPickedMove {
		Card card;
		int startFrame = 0; // frame when move was scheduled
		int delayFrames = 0; // hold before starting move
		int durationFrames = 0; // move duration in frames
		glm::vec2 startPos; // screen-space center
		glm::vec2 endPos; // screen-space center (deck)
		bool finished = false;
		int ownerIndex = -1; // which player's deck we're animating into
		float endScale = 1.0f; // final scale when reaching destination (e.g., 0.3f for minion UI)
	};

	std::vector<DraftPickedMove> activeDraftPickedMoves;

	// Schedule a visual draft-picked move with duplicate protection
	void scheduleDraftPickedMove(const DraftPickedMove & mv);

	// Deck flash when a picked card lands (visual only) - frame-based
	int deckFlashStartFrame = 0;
	int deckFlashDurationFrames = 0; // computed at setup()

	int deckFlashOwnerIndex = -1;

	// Schedule next draft generation to allow animations to finish
	bool draftNextScheduled = false;
	int draftNextClassTier = -1;
	float draftNextAt = 0.0f; // epoch time when to run generateDraftOptions

	// Schedule end-of-draft transition (wait for visuals before returning to gameplay)
	bool draftEndScheduled = false;
	int draftEndNextPlayerIndex = -1;
	// Last player index for which `draftOptions` were generated/applied
	int lastDraftOptionsPlayer = -1;
	float draftEndAt = 0.0f;

	// Draft display timing: show draft non-interactively with auto-select
	float draftDisplayStartTime = 0.0f; // When draft options appeared
	float draftDisplayDuration = 1.0f; // Show for ~1 second before auto-accepting
	bool draftDisplayInteractiveEnabled = true; // Can click draft options?
	int draftAutoSelectedIndex = -1; // Which option was RNG-selected (for visual feedback)

	// Visual-deferred key draft queue: key pickups are consumed during command
	// execution, but each draft UI opens when the moving unit visually reaches
	// the corresponding key tile.
	struct PendingVisualKeyDraft {
		int tileX = -1;
		int tileY = -1;
		int targetIndex = -1;
		int classTier = -1;
		std::array<int, 3> poolIndices = { -1, -1, -1 };
	};
	std::vector<PendingVisualKeyDraft> pendingVisualKeyDraftQueue;

	// Helpers
	void startShuffleVisual(int playerIndex, float delaySeconds = 0.0f);
	void scheduleGenerateDraftOptions(int classTier, float delaySeconds);
	void playHandFeedbackSfx(float speed = 1.0f, float volumeMul = 0.16f);

	// Draw helper for active picked-card animations
	void drawActiveDraftPickedMoves();
	// Keep draft option animations advancing even if the game is paused from drafting.
	void updateDraftUiAnimations();

	// -------------------------------------------------------------------------
	//                          RENDERING & MESHES
	// -------------------------------------------------------------------------
	void buildLevelMesh();
	void buildFloorMesh();

	void allocateWorldFbo(int w, int h);

	// Specific UI Drawers
	void drawGhostRelocateUI();
	void drawOpponentMenu(); // Draw opponent's active menu with red outlines

	// Board highlight helpers
	void drawJoinedOutlines(bool highlightedTiles[BOARD_WIDTH][BOARD_HEIGHT], ofColor color, float surfaceY);
	void drawExpandingAOERings(float surfaceY);

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
	void drawAcceptButtonShared(const ofRectangle & buttonRect, bool canAccept, float scale = 1.0f, float alpha = 1.0f);

	// Draw multiple option cards (draft-style). Fills outRects for hit-testing.
	void drawOptionCards(const ofRectangle & panelRect,
		const std::string & title,
		const std::string & desc,
		const std::vector<std::string> & labels,
		const std::vector<ofColor> & accents,
		const std::vector<bool> & enabled,
		std::vector<ofRectangle> & outRects);

	// General damage application helper (used by multiple flows)
	bool applyDamageTo(Player & target, int damage, DamageType type, int attackerIndex = -1);
	void drawDispelUI();
	void cancelDispel();
	void determineStatusOptions(Player * target);
	void applyDispelEffect(int statusID);
	void drawMinionManagerUI();
	void drawMinionStatusBars(Player & minion, const std::string & name, float x, float y, float totalWidth, float preferredHpWidth, bool alignRight = false);

	// =========================================================================
	//                            MEMBER VARIABLES
	// =========================================================================

	// --- REPLAYS & STATS ---
	struct PlayerMatchStats {
		int totalDamageDealt = 0;
		int maxDamageInOneTurn = 0;
		int currentTurnDamage = 0;
		int totalHealing = 0;
		int minionsSpawned = 0;
		int cardsPlayed = 0;
	};
	PlayerMatchStats matchStats[2];

	struct ReplayCommand {
		uint32_t frame;
		InputCommandPacket cmd;
	};
	std::vector<ReplayCommand> matchReplayLog;
	std::vector<ReplayCommand> replayPlaybackQueue;
	size_t replayPlaybackIndex = 0;
	bool isReplayMode = false;
	bool replaySavedThisMatch = false;

	void saveReplay(const std::string & filename);
	void loadReplay(const std::string & filename);

	// --- GAME STATE ---
	bool g_isGameOver = false;
	int g_winnerID = -1;
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

	// Game Over UI
	ofRectangle gameOverReturnBtn;
	ofRectangle gameOverReplayBtn;

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

	CardGlowState getCardGlowState(int playerIndex, int cardIndex);

	float turnBannerStartTime = 0.0f;
	std::string turnBannerText = "";
	ofColor turnBannerColor = ofColor::gold;
	int lastBannerTurnOwnerID = -1;
	int lastBannerTurnCycle = -1;

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

	enum class MenuChoiceID {
		None = 0,
		Damage,
		Heal,
		Block,
		PunchX2,
		BlockX2,
		Draft,
		AP,
		Purge,
		Barrier,
		Push,
		Pull,
		Discard,
		Self
	};

	// --- BOARD & ENTITIES ---
	const float TILE_SIZE = 5.0f;
	Tile board[BOARD_WIDTH][BOARD_HEIGHT];
	TargetInfo targetCache[BOARD_WIDTH][BOARD_HEIGHT];
	std::deque<Player> players;
	// Track which minion playerIDs we've logged during render to avoid flooding logs
	std::set<int> renderLoggedMinions;
	int currentPlayerIndex = -1;

	// --- TURN TIMER ---
	int turnTimerFramesPerSecond = 60; // lockstep timer resolution
	int turnStartFrame = 0; // when the current turn began (frame counter)
	int turnDurationFrames = 90 * 60; // 90 seconds for regular units, 60 for minions
	bool turnTimerEnabled = true; // whether to enforce auto-end-turn on timeout

	// When true, the new turn's timer start is deferred until visuals finish.
	bool turnStartDeferred = false;
	int turnStartDeferredAtFrame = 0;

	// Pause/resume support when modal choices are presented to other players
	bool turnTimerPaused = false;
	int turnTimerPausedRemainingFrames = 0; // frames remaining when paused
	bool reconnectTurnTimerPausedByDisconnect = false;
	int reconnectTurnTimerPausedRemainingFrames = 0;

	// Visual-only: pending visuals to spawn at next visible update (set by deterministic logic)
	int pendingTurnStartVisuals = -1;
	bool waitingForReconnect = false;

	// Opponent decision timer (when a modal requires the opponent to choose)
	bool opponentDecisionTimerActive = false;
	uint32_t opponentDecisionStartFrame = 0;
	int opponentDecisionDurationFrames = 30 * 60; // default opponent decision window (30s for menus)
	int opponentDecisionPlayerIndex = -1; // which player must decide

	// Competitive anti-stall / reconnect-forfeit state
	std::array<int, 2> afkStrikeCounts = { 0, 0 }; // indexed by owner/player ID (0/1)
	int currentTurnOwnerID = -1; // owner ID of the active unit for this turn
	bool currentTurnHadMeaningfulAction = false; // true once a valid action is made this turn
	bool currentTurnTimeoutProcessed = false; // avoid double-strike from multiple timer checks
	float reconnectForfeitStartTime = -1.0f; // real-time countdown anchor while waiting
	float reconnectForfeitDuration = 60.0f; // seconds before disconnect forfeit

	std::vector<DeathMarker> graveyard;
	std::vector<FloatingText> activeFloatingTexts;
	std::vector<Particle> particles;
	float screenShake = 0.0f;

	// --- Tracer effects for ranged spells ---
	struct Tracer {
		glm::vec3 start;
		glm::vec3 end;
		glm::ivec2 impactTile; // tile being highlighted
		std::vector<glm::ivec2> adjacentTiles; // optional adjacent tiles to also highlight (for AOE effects)
		float startTime = 0.0f;
		float duration = 5.0f;
		ofColor color = ofColor::white;
	};

	std::vector<Tracer> activeTracers;

	// Spawn a tracer line from world-space start -> end and highlight impact tile
	void spawnTracer(glm::vec3 start, glm::vec3 end, glm::ivec2 impactTile, ofColor color, float duration = 5.0f);

	// Spawn a tracer with adjacent tiles highlighted (for AOE effects like Magic Blast)
	void spawnTracerWithAdjacent(glm::vec3 start, glm::vec3 end, glm::ivec2 impactTile, const std::vector<glm::ivec2> & adjacentTiles, ofColor color, float duration = 5.0f);

	// Compute tracer world-space endpoints such that tracer starts at the caster's
	// closest face midpoint and ends at the center of the hit grid fraction.
	void computeTracerEndpoints(glm::vec2 casterTile, glm::vec2 hitGridFrac, glm::vec3 & outStart, glm::vec3 & outEnd);

	// Individual player H2H stats (wins/losses/draws) against each opponent by SteamID
	struct H2HRecord {
		std::string opponentName;
		int wins = 0;
		int losses = 0;
		int draws = 0;
	};

	std::map<std::string, H2HRecord> h2hStatsMap;
	std::vector<std::pair<int, ofRectangle>> assistantRerollButtons; // <AssistantPlayerIndex, ButtonRect>

	void loadH2HStats();
	void saveH2HStats();
	void recordH2HOutcome(const std::string & opponentSteamID, const std::string & opponentName, int outcome);

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
	// Default preset (1.0 = normal speed)
	std::vector<float> keyAnimSpeedPresets = { 1.0f };
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
	// When true, `updateGame()` should not send a separate turn-start packet until status effects
	// (paralysis/poison/onFire/etc.) that occur at the start of a turn have finished.
	bool isHandlingTurnStartEffects = false;
	PlayerActionState playerAction = NONE;
	int selectedPieceGridX = -1;
	int selectedPieceGridY = -1;
	int lastCachedPlayerX = -1, lastCachedPlayerY = -1;

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

		// Pending draft state packet (if host sends state while client is waiting)
		bool draftStateAvailable = false;
		DraftStatePacket draftState;

		// Network action staging
		std::map<int, int> actionByActor;
		int networkActionActor = -1;
		int networkActionPrevPlayer = -1;

		// Save browser pending selection/confirmation (migrated from top-level)
		int saveBrowserPendingIndex = -1;
		bool saveBrowserConfirmVisible = false;
	} networkPending;

	// Recent place notifications sent by host: tracks player indices for which
	// a PKT_PLACE_SUMMONED_MINION was emitted but whose authoritative shuffle
	// may follow shortly. This helps detect/validate packet ordering.
	std::set<int> recentPlaceSentIndices;
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
	float blobShadowSize = 1.2f; // world-space diameter multiplier for shadows (reduced)
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
	bool enableC64Shader = false; // toggled via 'P' cycle (pixel -> C64 -> off)
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
	bool enablePixelArt = true; // toggle the effect (default ON)
	bool pixelArtWarned = false; // set when we warn once about shader missing
	bool pixelArtActiveNotified = false; // set once when pixel-art branch runs
	bool pixelArtDumpedPixels = false; // set once when we read back low-res FBO for debugging

	// Notify once when world post-process runs
	bool worldPostActiveNotified = false;
	// Apply nearest filtering and other pixel-art settings to textures/FBOs
	void applyPixelArtSettings();
	int pixelArtDownscale = 2; // render at 1/downscale resolution (higher value => lower internal resolution)
	int pixelArtLevels = 20; // posterize levels per channel (higher => less posterize / preserve brightness)
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
	int getOwnerIdForActorIndex(int actorIndex) const;
	int getNormalTurnDurationFramesForActorIndex(int actorIndex) const;
	void markMeaningfulActionOnCurrentTurn();
	void registerAfkTimeoutForCurrentOwner();
	void handleOwnerForfeit(int loserOwnerId, const std::string & reason);
	int getActiveTurnDurationFrames() const;
	void resetDraftPhaseTimerWindow();

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
	float mouseDownTimeSec = 0.0f;
	ofVec2f handDragVelocity = ofVec2f(0.0f, 0.0f);
	bool handDragInValidPlayZone = false;
	float nextHandSfxAt = 0.0f;

	ofRectangle endTurnButtonRect;
	ofRectangle rerollButtonRect;
	ofVec2f endTurnButtonCurrentPos;
	ofVec2f endTurnButtonTargetPos;
	bool isHoveringEndTurn = false;
	bool endTurnLocked = false; // Prevent repeated end turn clicks before turn updates

	ofTrueTypeFont uiFont;
	ofTrueTypeFont titleFont;
	ofTrueTypeFont cardEffectFont;

	ofFbo modelFbo;
	std::vector<MinionUI> activeMinionUIs;

	// ===== UNIFIED CARD INTERACTION STATE (Replaces 20+ per-card bool flags) =====
	CardInteractionState cardInteractionState = CARD_INTERACTION_STATE_IDLE;
	int interactingCardIndex = -1; // Index in currentPlayer.hand of card being interacted with
	int interactingCardType = CARD_NONE; // Type of card being interacted with (cached)
	int interactionTargetIndex = -1; // Index of chosen target (if applicable)
	std::string interactionMenuChoice; // Selected menu option (Burst: damage/heal, Double-Handed: Punch/Block, etc.)
	bool interactionNeedsStatusSelect = false; // Special: Dispel status selection required
	int interactionDiceRoll = 0; // Cached dice result if interaction requires roll (Teleport range, etc.)
	glm::vec2 interactionTargetTile; // Tile target for interactions that require a grid position (Teleport)
	std::string interactingCardName; // Optional: store card name for resolution logging

	// --- Legacy Targeting States (to be deprecated after consolidation) ---

	// Centralized targeting state members (struct defined in public area)
	TargetingContext targetingContext;

	// legacy healCardIndex removed; healing is handled by data-driven engine

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
	std::unordered_set<uint64_t> queuedCommandKeys;
	std::unordered_set<uint64_t> executedCommandKeys;
	uint32_t nextCommandId = 1;
	uint32_t lastProcessedCommandId = 0;
	int lastTurnStartSentPlayer = -1;
	int lastTurnStartSentCounter = -1;

	// Provisional/optimistic command support (client-side prediction)
	// Stores the snapshot taken immediately before applying a provisional command
	std::map<uint32_t, std::string> provisionalSnapshots;
	// Stores the provisional InputCommandPacket keyed by commandId
	std::map<uint32_t, InputCommandPacket> provisionalCommands;

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
	// Send an input command: optionally apply locally (optimistic) and send over network
	bool sendInputCommand(InputCommandPacket & cmd, bool applyLocally = true);
	void executeInputCommand(const InputCommandPacket & cmd);
	bool processEffectOp(EffectOp & op);

	// --- CARD SPECIFIC VARIABLES ---

	// === UNIFIED CARD STATE SYSTEM ===
	// All cards use this single outcome structure instead of scattered pending* variables
	CardOutcome currentCardOutcome; // Currently active card's outcome data
	CardPlayState cardPlayState = CARD_PLAY_STATE_IDLE; // Current state of card action
	int activeCardIndex = -1; // Index of card currently being played (-1 if none)

	// Helper function to reset card state between plays
	void resetCardState() {
		currentCardOutcome = CardOutcome();
		cardPlayState = CARD_PLAY_STATE_IDLE;
		activeCardIndex = -1;
		resetCardInteraction();
	}
	void resetCardInteraction(bool syncNetwork = true);
	// Attack
	// Attack dice/result now use centralized `interactionDiceRoll` and
	// `interactingCardName` for logging/special cases. Targets & damage type
	// are stored in `currentCardOutcome.attackTargetIndices` and
	// `currentCardOutcome.attackDamageType`.

	// Amnesia (migrated to effect/op handlers)
	int amnesiaTargetPlayerIndex = -1;
	int numCardsToRemove = 0;
	std::vector<Card> amnesiaDeckCopy;
	std::vector<int> amnesiaSelectedIndices;
	std::vector<ofRectangle> amnesiaCardRects;
	// Temporary storage for amnesia selections received via lockstep command
	std::vector<int> amnesiaSelectionFromCmd;
	bool amnesiaSelectionFromCmdPresent = false;

	// Which local player ID is allowed to choose Amnesia removals (playerID, e.g., 0 or 1). -1 = none
	int amnesiaChooserPlayerID = -1;

	// Blocking Boon
	// Draft queue moved into `networkPending.draftQueue`.
	int blockingBoonTargetIndex = -1; // Stores target for the "Tails" effect
	int blockingBoonPendingCasterIndex = -1;
	std::vector<int> blockingBoonPendingCoinRawResults;
	std::vector<int> blockingBoonPendingD20RawResults;
	bool blockingBoonPendingPhysicalAfterDraft = false;

	// Blocking Boon staged resolution (migrated to effect/op system)
	// Counters moved into `currentCardOutcome.namedDiceResults`:
	//  - "blocking_boon_coins_remaining"
	//  - "blocking_boon_nonphys"
	//  - "blocking_boon_total"

	// Prevent duplicate plays while a Blocking Boon is resolving
	bool blockingBoonActive = false;

	// Magic Blast (handled via effect/op pipeline)
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

	// Ethereal Jolt handled via EffectOpType::APPLY_ETHEREAL_JOLT
	// uses `interactionDiceRoll` and `interactionTargetTile`

	// Chain Lightning
	// (migrated to centralized interaction state)
	// Chain Lightning uses `interactionDiceRoll` and `interactionTargetTile`

	// Train Menu UI
	// Train menu now uses `interactingCardIndex` and `cardInteractionState`
	ofRectangle trainMenuRect;
	ofRectangle trainBtnAP;
	ofRectangle trainBtnDraft;

	// Function Declaration
	void drawTrainMenuUI();

	// Dispel (UI state migrated to centralized cardInteractionState)
	// Barrier handled via effect/op pipeline
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
	// Waiting state between dice resolution and movement (use earthquakeWaitTimer > 0)
	float earthquakeWaitTimer = 0.0f; // seconds remaining

	// Network-assisted earthquake assignment counter (used when client receives dice)
	int earthquakeDiceAssignCounter = 0;

	// Earthquake damage targets stored until APPLY_EARTHQUAKE_DAMAGE runs
	struct EarthquakeDamageTarget {
		int playerID = -1;
		int playerIndex = -1;
		glm::vec3 visualPos = glm::vec3(0);
		int gridX = -1;
		int gridY = -1;
		int blackboardSlot = -1;
	};

	std::vector<EarthquakeDamageTarget> earthquakeDamageTargets;

	// When a client plays earthquake, it waits for host to send EarthquakeBegin

	// Wisdom Boon (uses centralized interaction state)
	ofRectangle wisdomMenuRect;
	ofRectangle wisdomBtnDamage;
	ofRectangle wisdomBtnBlock;

	// Burst of Light (choice UI + targeting) — centralized via interaction state
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

	// --- Ghost Relocate Interaction State ---
	std::vector<glm::ivec2> ghostRelocateChoices;
	std::vector<ofRectangle> ghostRelocateButtons;
	int ghostRelocateTargetIndex = -1;

	// --- Magic Hand Relocate State ---
	const int PSEUDO_CARD_MAGIC_HAND_RELOCATE = 998;
	const int MENU_MAGIC_HAND_RELOCATE = 999;

	// --- Amnesia State ---

	ofRectangle amnesiaMenuRect;
	ofRectangle amnesiaBtnSelf;
	ofRectangle amnesiaBtnAdjacent;

	// --- Renewed Inspiration State (REAL-TIME) ---
	// Uses centralized `cardInteractionState` (CARD_INTERACTION_STATE_MENU + CARD_RENEWED_INSPIRATION)
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
	// Magic Bolt is now fully handled by the effect/op pipeline (APPLY_MAGIC_BOLT
	// and follow-up APPLY_* ops). The impact tile and authoritative rolls are
	// stored in `currentCardOutcome` and EffectSequence.blackboard slots.
	// (migrated to centralized interaction state)

	// Shoot Arrow handled via effect/op pipeline (APPLY_SHOOT_ARROW)

	// --- Giant Magic Hand ---
	// Magic hand menu UI migrated to centralized `cardInteractionState`
	glm::ivec2 magicHandTargetTile;
	int magicHandPushedUnitIndex = -1;
	glm::ivec2 magicHandPushDir;

	// Flail handled via effect/op pipeline

	// Hellhound targeting
	// (migrated to centralized interaction state)

	// Death Card Logic handled via EffectOpType::APPLY_DEATH and APPLY_SLEEP_DURATION

	// --- Spark of genius Logic ---
	// Spark of Genius handled via effect/op pipeline

	// Helper functions
	void drawCardSpawnerUI();
	void drawCardEncyclopediaUI();

	// === EXTRACTED CARD EFFECT FUNCTIONS (Phase 2 + 3: Consolidation) ===
	// These are called from executeCardByType(), resolveCardMenu(), and resolveCardDice() to eliminate duplication
	// Legacy per-card effect helpers removed (now handled via EffectOp pipeline)
	// Legacy per-card helpers removed (handled via EffectOp pipeline)
	// Helper to draw centered instruction text with shadow
	void drawInstructionText(const std::string & message, ofColor color = ofColor::white);
	// Helper to draw centered dice label text with shadow
	void drawDiceLabel(const std::string & message, ofColor color = ofColor(255, 215, 0), float yPos = 0);
	// Helpers to draw card menu overlays and titles
	void drawMenuOverlay();
	void drawMenuBackground(const ofRectangle & menuRect, float cornerRadius = 15);
	void drawMenuTitle(const std::string & title, const ofRectangle & menuRect, float yOffset = 60);

	// ===== CENTRALIZED CARD INTERACTION SYSTEM =====
	void resetCardToBaseStats(Card & card);
	void updateCardInteractionState(CardInteractionState newState, int cardIdx = -1, int cardType = CARD_NONE);
	void handleCardDragToPlay(int cardIndex);
	void handleCardTargetClick(int gridX, int gridY);
	void handleCardMenuClick(const std::string & buttonId);
	void drawActiveCardInteractionUI();
	// Cancel any active targeting modes/menus and reset related state
	void cancelAllTargeting();
	void cancelMagicHand();

	// Status Effects
	// NOTE: status dice results (onFire/poison/etc) are stored in
	// `currentCardOutcome.namedDiceResults["status_onfire"]` and
	// `currentCardOutcome.namedDiceResults["status_poison"]` respectively.

	// Main Menu
	ofRectangle mainMenuOnlineButton;
	ofRectangle mainMenuSingleplayerButton;
	ofRectangle mainMenuCustomisationButton;
	ofRectangle mainMenuEncyclopediaButton;
	ofRectangle mainMenuSettingsButton;
	ofRectangle mainMenuQuitButton;

	// Singleplayer Menu
	ofRectangle singleplayerNewGameButton;
	ofRectangle singleplayerContinueButton;
	ofRectangle singleplayerLoadButton;
	ofRectangle singleplayerReplayButton;
	ofRectangle singleplayerBackButton;
	ofRectangle mainMenuLocalPvPButton;
	ofRectangle mainMenuVsAIButton;

	int mainMenuHoveredIndex = -1;

	// Main Menu Mini-Game
	glm::vec2 mainMenuCirclePos = { 6.0f, 6.0f };
	std::vector<glm::vec2> mainMenuCirclePath;
	float menuTileSize = 0;
	float menuStartX = 0;
	float menuStartY = 0;
	float baseMenuStartX = 0; // Used to anchor the infinite panning math

	// Menu Styling Helpers
	void draw2DMenuBackground();
	void drawMenuPlaqueButton(const ofRectangle & rect, const std::string & text, bool isHovered, bool isOnline = false);
	void drawMenuPlaquePanel(const ofRectangle & rect);
	void triggerMenuTransition(bool fromLeft); // <--- Fixed missing declaration

	// Infinite Screen Panning
	float currentMenuPanX = 0.0f;
	float targetMenuPanX = 0.0f;
	bool isWaitingForMenuTransition = false;
	float pendingMenuPanX = 0.0f;
	GameState pendingMenuState = STATE_MAIN_MENU;
	int targetMenuScreen = 0; // 0=Main, -1=Online, 1=Single, 2=Ency, -2=Settings
	void navigateToMenu(int screenIndex, GameState newState);
	void updateMenuRects(); // Dynamically slides hitboxes during transitions
	std::vector<glm::vec2> getInfiniteMazePath(glm::vec2 start, glm::vec2 end);

	// Encyclopedia State
	int encyclopediaMainTab = 0; // 0=Cards, 1=Minions, 2=How to Play
	float encyclopediaMainScroll = 0.0f;
	ofRectangle encyTabCards, encyTabMinions, encyTabRules, encyBtnBack;
	int encyclopediaMainHoveredIndex = -1;
	float encyclopediaMainHoverStartTime = 0.0f;
	bool encyclopediaMainHoverScaled = false;

	void drawEncyclopediaState();

	// Customisation State
	ofRectangle customisationBtnBack;
	void drawCustomisationState();

	// Multiplayer Menu UI
	ofRectangle mpLobbiesPanelRect;
	ofRectangle mpLeaderboardPanelRect;
	ofRectangle mpRefreshButton;
	ofRectangle mpHostButton;
	ofRectangle mpBackButton;
	std::vector<ofRectangle> mpLobbyButtons;
	void drawMultiplayerMenu();

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
	// If >0, host is waiting for clients to signal readiness; stores start time
	float hostWaitingForClientsReadyStartTime = 0.0f;
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
	ofSoundPlayer cardHoverSound;
	// Dice roll result display
	// Looping sound while dragging a card in hand
	ofSoundPlayer draggingHandLoop;
	bool draggingHandLoopPlaying = false;
	// Fade controls for dragging loop (volume units per second)
	float draggingHandTargetVolume = 0.0f;
	float draggingHandFadeSpeed = 8.0f; // default: fades to zero in ~0.125s
	// Track whether we were dragging in the previous frame to detect drag end
	bool draggingWasActive = false;
	std::string diceRollResultText = "";
	float diceRollResultStartTime = 0.0f;
	float diceRollResultDuration = 3.5f; // How long to show the result

	// --- DEBUG ---
	bool isDebugMode = true;
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
	float pileViewScrollOffset = 0.0f; // Track scroll offset for deck/discard hover panels
	float amnesiaScrollOffset = 0.0f; // Track scroll offset for Amnesia inspection panels
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

	// Expanding AOE ring visualization for Magic Bolt / Psionic Wave preview
	struct ExpandingAOERing {
		glm::ivec2 centerTile = { -1, -1 };
		int maxRadiusFeet = 0; // Maximum AOE radius in feet
		float startTime = 0.0f;
		float duration = 1.5f; // Duration of expand animation in seconds
		int cardType = -1; // CARD_MAGIC_BOLT or CARD_PSIONIC_WAVE
	};

	ExpandingAOERing activeAOERing; // Current expanding AOE (if any)
	std::vector<ExpandingAOERing> activeMagicBoltAOERings; // Overlapping Magic Bolt preview rings
	float lastMagicBoltAOERingSpawnTime = -1000.0f;

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
