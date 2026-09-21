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

enum AIMode {
	AI_MODE_RULE_BASED, // Fast, deterministic C++ instructions
	AI_MODE_DEEP_LEARNING // Python / ZMQ socket model
};

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
	STATE_CUSTOMISATION,
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
	CARD_PLAY_RESULT_IMMEDIATELY,
	CARD_PLAY_RESULT_AWAITING_MENU_CHOICE,
	CARD_PLAY_RESULT_AWAITING_TARGETING,
	CARD_PLAY_RESULT_CANCELLED,
	CARD_PLAY_RESULT_NOT_PLAYABLE,
	CARD_PLAY_RESULT_AWAITING_DICE
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
	PSEUDO_CARD_GHOST_RELOCATE
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
	PURPOSE_BLOCKING_BOON_COIN,
	PURPOSE_BLOCKING_BOON_D20
};

enum CardInteractionState {
	CARD_INTERACTION_STATE_IDLE,
	CARD_INTERACTION_STATE_TARGETING,
	CARD_INTERACTION_STATE_MENU,
	CARD_INTERACTION_STATE_STATUS,
	CARD_INTERACTION_STATE_PLACING
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
	CURSOR_DEFAULT,
	CURSOR_CLICK,
	CURSOR_GRAB,
	CURSOR_HOLD
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
	float edgeInset = 0.0f;
	float stackYOffset = 0.0f;
	float stackVerticalGap = 0.0f;
	float minionEntryGapUnscaled = 12.0f;
	float minionIconGap = 0.0f;
	float timerBarHeight = 0.0f;
	float chatInset = 0.0f;
	float healthBarSideGap = 0.0f;
	float healthBarInwardNudge = 0.0f;
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
	int index = -1;
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
	ofRectangle effectRect = ofRectangle(96, 928, 864, 384);
	float nameScale = 13.75f;
	float nameMinScale = 1.0f;
	float nameCurveDropPx = 12.0f;
	float nameMiddleClampXMin = 416.0f;
	float nameMiddleClampXMax = 656.0f;
	float nameMiddleBottomMaxY = 864.0f;
	float costScale = 3.0f;
	float labelScale = 1.0f;
	float effectScale = 4.0f;
	float effectMinScale = 1.0f;
	float effectLineSpacing = 0.75f;
};

struct PreviewLabel {
	glm::vec2 screenPos;
	std::string text;
	ofColor color;
};

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
	int result = 0;
	int rawResult = 0;
	bool isFinishedVisual = false;
	float startTime = 0;
	glm::quat finalQuat;
	float currentRotation = 0;
	glm::vec3 rotationAxis;
	int associatedUnit = -1;
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

	int apGain = 0;
	int drawCount = 0;
	int discardHandCount = 0;
	int discardDeckCount = 0;
	int applyStatus = 0;
	bool isAoe = false;
	bool isHandRelated = false;

	int baseHeal = 0;
	int healDiceNum = 0;
	int healDiceSides = 0;

	int blockGain = 0;
	int wardGain = 0;
	int barrierGain = 0;
	int holyBlockGain = 0;
	int fortificationGain = 0;
	int maxHealthGain = 0;
	int luckGain = 0;
	int apGainThisTurn = 0;
	int apGainNextTurn = 0;

	int destroyDeckTargetCount = 0;

	int blockAmount = 0;
	int wardAmount = 0;
	int barrierAmount = 0;
	int holyBlockAmount = 0;
	int fortifyAmount = 0;

	ofJson variantDefs;
	std::string hpDerivedFromUnitType = "";
	int hpDerivedAdd = 0;

	TargetingType targeting = TARGET_ANY_TILE;
	bool drawnThisTurn = false;
	bool playedThisTurn = false;
	bool isAnimating = false;
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

	bool isMovement = false;
	int fromX = -1;
	int fromY = -1;
	int toX = -1;
	int toY = -1;
	std::vector<glm::vec2> movementPath;
	int actorPlayerID = -1;

	int casterX = -1;
	int casterY = -1;
	int targetX = -1;
	int targetY = -1;
	bool hasTracers = false;
};

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
	REMOVE_STATUS,
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

struct DamageData {
	int targetIndex;
	DamageType damageType;
	int fixedDamage;
	int damageFromSlot;
};

struct HealData {
	int targetIndex;
	int amount;
	int amountFromSlot;
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
	int statType;
	int delta;
	int deltaFromSlot;
};

struct SpawnUnitData {
	int toX;
	int toY;
	int summonKind;
	int ownerPlayerID;
	int maxHealth;
	int maxHealthFromSlot;
	int ap;
	int summonerPlayerID;
	int variant;
};

struct ModifyTileData {
	int toX;
	int toY;
	int setHasWall;
};

struct AddCardToDeckData {
	int targetIndex;
	int cardType;
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
	int deckChoice;
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
	STATUS_NEXT_TURN_EXTRA_DRAW = 10,
	STATUS_NEXT_TURN_D10AP = 11,
	STATUS_NEXT_TURN_BONUS_DICE = 12,
	STATUS_SLEEP = 13,
	STATUS_ASSISTANT_REROLL_USED = 14,
};

struct StatusData {
	int targetIndex;
	int statusType;
	int duration;
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
		struct {
			int targetIndex;
			int preHP;
			int healAmount;
			int addCardType;
		} vampireResolve;
	} data;

	bool visualStarted = false;
	int visualFrame = 0;
};

struct EffectSequence {
	std::vector<EffectOp> ops;
	size_t currentOp = 0;
	int blackboard[16] = { 0 };
	bool isComplete = false;
};

enum VisualEventType {
	VE_NONE = 0,
	VE_WAIT = 1,
	VE_DICE = 2,
	VE_TRACER = 3,
	VE_CUSTOM = 99
};

struct VisualEvent {
	VisualEventType type = VE_NONE;
	int targetIndex = -1;
	int purpose = 0;
	float startTime = 0.0f;
	float duration = 0.0f;
	bool completed = false;

	int diceNum = 0;
	int diceSides = 6;
	int diceResult = 0;
	int dicePurpose = 0;
	std::vector<int> diceRawResults;
	bool visualStarted = false;
	bool textSpawned = false;

	std::vector<glm::ivec2> tilePreviewAdds;
	bool clearTilePreviewsOnComplete = false;

	glm::vec3 startPos = { 0.0f, 0.0f, 0.0f };
	glm::vec3 endPos = { 0.0f, 0.0f, 0.0f };
	ofColor color = ofColor::white;

	std::string text = "";
	float textXOffset = 0.0f;
	bool spawned = false;
};

enum CardPlayState {
	CARD_PLAY_STATE_IDLE = 0,
	CARD_PLAY_STATE_MENU = 1,
	CARD_PLAY_STATE_TARGETING = 2,
	CARD_PLAY_STATE_DICE = 3,
	CARD_PLAY_STATE_EFFECT = 4,
	CARD_PLAY_STATE_EFFECT_SEQUENCE = 5,
	CARD_PLAY_STATE_OUTCOME = 6,
	CARD_PLAY_STATE_FINISHED = 7
};

struct CardOutcome {
	CardType cardType = CARD_NONE;
	int cardIndex = -1;
	int casterIndex = -1;

	std::string menuChoice = "";

	glm::ivec2 primaryTarget = { -1, -1 };
	std::vector<glm::ivec2> secondaryTargets;
	int targetPlayerIndex = -1;
	std::vector<int> targetedPlayers;

	std::vector<int> diceResults;
	std::map<std::string, int> namedDiceResults;

	int damageDealt = 0;
	int healingDealt = 0;
	int apGained = 0;
	int blockGained = 0;
	int wardGained = 0;
	int barrierGained = 0;
	int holyBlockGained = 0;
	int fortificationGained = 0;
	int maxHpGained = 0;
	std::vector<std::string> statusesApplied;
	std::vector<int> unitsMovedBy;
	std::vector<glm::ivec2> wallsCreated;
	std::vector<int> minionsSpawned;

	CardType destroyedCardType = CARD_NONE;

	DamageType attackDamageType = DAMAGE_PHYSICAL;
	std::vector<int> attackTargetIndices;
	std::vector<int> attackTargetPlayerIDs;

	std::vector<int> poisonTargetPlayerIDs;
	int summonKind = 0;
	int summonOwnerPlayerID = -1;

	bool isComplete = false;
	bool apPaid = false;
};

struct PlayedCardDisplay {
	Card card;
	float startTime;
	glm::vec2 startPos;
	glm::vec2 currentPos;
	float currentScale = 1.5f;
	float startScale = 1.5f;
	float currentAlpha = 255.0f;
};

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
	glm::vec2 pos;
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

struct DrawCardAnimation {
	Card card;
	glm::vec3 startPos;
	glm::vec3 endPos;
	glm::vec2 currentPos;
	glm::vec2 targetPos;
	int pendingHandIndex = -1;
	float startTime;
	float duration;
	float currentAlpha = 255.0f;
	float currentScale = 1.0f;
	float startScale = 1.0f;
	float endScale = 1.0f;
	int ownerIndex;
	int ownerPlayerID = -1;
	bool toMinionHand;
	bool startIsScreenSpace = false;
	bool commitOnFinish = true;
};

struct Tile {
	bool hasPlayer = false;
	bool hasWall = false;
	bool isMagicWall = false;
	bool isHighlighted = false;
	bool isTargetable = false;
	bool isTargetPreview = false;
	bool visited = false;
	glm::vec2 parent = { -1, -1 };

	int minRollRequired = 0;
	float hitChance = 0.0f;
	bool hasTooltipInfo = false;

	bool isAoeCenter = false;
	int aoeRadiusFeet = 0;
};

struct Player {
	int x;
	int y;
	glm::vec3 visualPos = { 0.0f, 0.0f, 0.0f };
	int health = 15;
	int maxHealth = 15;
	int block = 0;
	int ward = 0;
	int fortification = 0;
	int barrier = 0;
	int holyBlock = 0;
	int luck = 0;
	int baseLuck = 0;
	int bonusTurns = 0;
	int ap = 0;
	int playerID = 0;
	float facingAngle = 0.0f;
	bool onFire = false;
	bool hasRegeneration = false;
	int fireApplierPlayerID = -1;
	int poisonApplierPlayerID = -1;

	int nextTurnAPBonus = 0;
	int shocksPlayedThisTurn = 0;
	int flurryOfFistsStacks = 0;
	int freeHandCardTurns = 0;
	bool isParalyzed = false;
	int paralysisHeadsCount = 0;
	bool isPoisoned = false;
	int poisonReduction = 0;
	bool nextAttackAddPoison = false;
	bool nextTurnD10AP = false;
	bool nextTurnExtraDraw = false;
	int nextTurnExtraDrawSetOnCycle = -1;
	bool replicateQueued = false;
	bool nextTurnBonusDiceFromMinions = false;
	int strengthenElementsTurnsRemaining = 0;
	int sleepTurnsRemaining = 0;

	int summonedOnTurnCycle = -1;
	int summonOrder = 0;

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

	bool isAssistant = false;
	int directSummonerID = -1;
	bool assistantRerollUsedThisTurn = false;
	int freeKickTurns = 0;
	int defenseCycle = -1;

	bool inTortoiseForm = false;
	int tortoiseDamageTaken = 0;
	Card tortoiseFormCard;
	std::string originalModelType = "";
	int storedDarkShieldDice = 0;

	ofTexture * minionTexture = nullptr;
	int ownerID = -1;

	bool inGhostForm = false;
	int ghostDamageTaken = 0;
	Card ghostFormCard;

	bool enteredWallByClick = false;

	std::vector<CardType> cardsPlayedThisTurn;
	std::vector<Card> playedCardsPile;
	std::vector<Card> hand;
	std::vector<Card> deck;
	std::vector<Card> discardPile;

	bool hasDrawnThisTurn = false;
	bool deckNeedsShuffle = false;
};

struct MinionUI {
	int playerIndex;
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
	glm::vec3 worldPos;
	glm::vec3 anchorPos;
	glm::vec3 velocity;
	float startTime;
	float duration = 3.0f;
	ofColor color;
	std::string category;
	float xOffset = 0.0f;
};

struct Particle {
	glm::vec3 pos;
	glm::vec3 vel;
	float life;
	float decay;
	float size;
	ofColor color;
};

// =================================================================================================
//                                      MAIN APPLICATION CLASS
// =================================================================================================

class ofApp : public ofBaseApp {

public:
	// --- Lifecycle Methods ---
	void setup();
	void reloadFonts();
	void update();
	void draw();
	void exit();
	~ofApp();

	void loadNextModelBatch();
	void ensureModelsLoaded();

	// --- AI Configuration & Methods ---
	AIMode currentAIMode = AI_MODE_RULE_BASED;
	int aiActionStage = 0;
	float aiStageTimer = 0.0f;
	int aiPendingCardIdx = -1;
	int aiPendingTargetX = -1;
	int aiPendingTargetY = -1;
	int aiPendingMoveX = -1;
	int aiPendingMoveY = -1;
	bool aiDraftStaged = false;
	int aiLastAttemptedCardIdx = -1;
	int aiLastAP = -1;

	// NEW: Anti-Stuck Failsafe Memory
	int aiStuckCounter = 0;
	int aiLastStateHash = 0;

	// Tactical AI Helper Functions
	int getCardEstimatedDamage(const Card & c, const Player & caster, const Player & target);
	float getEnemyThreatScore(int enemyPlayerIdx);
	glm::ivec2 getBestKeyTarget(const Player & actor);
	void updateAI();
	void thinkRuleBasedAI();
	void thinkDeepLearningAI();
	float evaluateDraftCardScore(const Card & card, int classTier, int draftingPlayerIdx);
	glm::ivec2 aiLastMovedFromTile = glm::ivec2(-1, -1);
	// ZMQ / RL Helpers
	std::vector<float> extractGameStateForAI();
	int getAIActionFromModel(const std::vector<float> & state, float reward, bool done);
	void executeAIAction(int actionIndex);

	// =========================================================================
	// ADVANCED TACTICAL AI ENGINE STRUCTURES
	// =========================================================================
	struct AIActionStep {
		enum StepType { STEP_MOVE,
			STEP_PLAY_CARD,
			STEP_REROLL,
			STEP_END_TURN } type;
		int cardIdx;
		int tx, ty;
		int apCost;
		std::string label;
	};

	struct AITurnPlan {
		std::vector<AIActionStep> steps;
		float finalScore = -99999.0f;
		int expectedDamage = 0;
		int expectedHealing = 0;
		glm::ivec2 endTile = { -1, -1 };
	};

	AITurnPlan currentAIPlan;
	int currentAIPlanStepIndex = 0;

	void buildEnemyThreatMap(int enemyOwnerID, float outThreatMap[13][9]);
	AITurnPlan findBestTurnPlan(int actorIdx);

	struct TargetingContext {
		int sourceCardIndex = -1;
		int sourcePlayerIndex = -1;
		TargetingType type = TARGET_NONE;
		std::function<bool(int, int)> isValid = nullptr;
		std::function<void(int, int)> onSelected = nullptr;
		std::function<void()> onCancel = nullptr;
		std::string instruction;
	};

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

	bool draggingAudioMaster = false;
	bool draggingAudioMenu = false;
	bool draggingAudioSfx = false;
	bool draggingFramerateSlider = false;

	void drawMinionCard(int minionIndex, int ownerIndex);
	void updateGameLogic();
	void prepareGameVisualState();

	std::vector<VisualEvent> visualEvents;
	void queueVisualEvent(const VisualEvent & e);
	void processVisualEvents();
	void processWaitingFlags();
	void beginInitiativeDrafting(int winnerIndex);

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

	void processNetworkPackets();

	void sendPlaceSummonedBegin(int minionType, int ownerPlayerID, int sourceX, int sourceY, int numToPlace);
	void sendPlaceSummonedMinion(int minionType, int ownerPlayerID, int targetX, int targetY, int minionHP, int minionAP, int minionPlayerID);
	void sendCardActionBegin(int cardType, int actorIndex, int targetX, int targetY, int p0 = 0, int p1 = 0, int p2 = 0, int p3 = 0, const std::string & label = "");
	void sendActionPacket(int cardIndex, int tx, int ty, int cost, int menuChoice = 0, const std::string & cardNameOverride = "");
	void sendMagicHandResolutionPacket(int choice);
	void sendMenuState(int menuType, int targetIndex, int hoveredChoice, int cardIndex);

	long long calculateChecksum();
	bool harnessLoadAndPrintChecksum(const std::string & path);
	void harnessAutoAdvanceTurns(int turns);

	void sendSnapshotToClient(bool isRecoveryOrReconnect = false);
	std::string buildSnapshotString();
	void applySnapshotString(const std::string & data, bool fromNetworkSnapshot = true);

	void logDeckStates(const std::string & reason);
	std::string getDeckStateString(const Player & p);

	void addTimeBonusToTurn(int actorIndex, int seconds);
	float lastHandshakeRequestTime = 0.0f;
	float handshakeRequestInterval = 1.0f;
	uint32_t lastObservedLobbySeed = 0;
	float lastLobbySeedLogTime = 0.0f;
	uint32_t currentMapSeed = 0;

	SteamManager steamManager;

	bool isMultiplayer = false;
	bool isVsAI = false;
	bool isAIvsAI = false;
	float aiThinkTimer = 0.0f;
	int myLocalPlayerID = 0;
	int g_viewedOpponentID = 1; // Tracks which opponent's UI is visible on the right
	std::vector<ofRectangle> opponentViewTabs; // Hitboxes for the UI dropdown tabs
	std::vector<int> matchTurnOrder; // Strict 1st to 4th order for drafting and turn 1
	int currentDraftingOrderIndex = 0; // Tracks whose turn it is to draft
	std::vector<int> matchPlacementOrder; // Tracks who died in what order (1st to die = 4th place)

	zmq::context_t * zmqContext = nullptr;
	zmq::socket_t * zmqSocket = nullptr;
	bool zmqConnected = false;
	float cumulativeReward = 0.0f;

	uint32_t localSeedComponent = 0;
	bool waitingForClientHandshake = false;

	int myElo = 1000;
	int opponentElo = 1000;
	int eloChange = 0;
	bool eloCalculated = false;

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

	std::string turnStartBackupSnapshot;
	uint32_t lastReceivedSeqByPlayer[2] = { 0, 0 };

	ofImage localAvatarImage;
	ofImage opponentAvatarImage;
	bool localAvatarReady = false;
	bool opponentAvatarReady = false;

	bool shouldFlipCamera() const { return isMultiplayer && myLocalPlayerID == 1; }
	ofCamera & getActiveCamera();
	glm::vec3 transformGridToWorld(int gx, int gy);
	glm::ivec2 transformWorldToGrid(glm::vec3 worldPos);
	int getVisualPlayerIndex(int actualPlayerIndex);

	bool debugFlatSkeletonDraw = true;
	bool debugForceUnshadedDraw = false;

	glm::mat4 staffSocketOffset;
	glm::mat4 handBindPoseWorldMatrix;

private:
	int lastAPDiceNum = 0;
	int lastAPDiceSides = 0;
	std::vector<int> lastAPRawResults;
	bool apResolvedThisTurn = false;

	void setupGame();
	void startInitiativePhase();
	void initialiseGameStateCommon();
	void initGameFromSeed(uint32_t seed);
	void updateGame();
	void drawGame();
	void cleanupGame();

	void updateNetwork();
	void updateAudio();
	void updateVisuals();
	void updateStateMachine();

	void startNewTurn();
	void continueNewTurn();
	void performEndTurnAdvance();

	void drawMainMenu();
	void drawSettingsMenu();
	void drawPauseMenu();
	void drawLobby();
	void updateAudioVolumes();
	void applySettings();
	void recalculateUI(int w, int h);
	void updateDebugRects();
	void debugSkipDraftRandomCards();

	void drawCard(bool sendPacket = true);
	CardPlayResult playCard(int cardIndex, int targetX, int targetY);

	void processCardStateInput(int mouseX, int mouseY, int button);
	void applyAmnesiaSelectionLocal(int targetPlayerIndex, const std::vector<int> & selections);
	void updateCardStateMachine();
	void advanceCardState(CardPlayState newState);
	void applyCardOutcomeEffects();
	void handleCardTargetInput(int gridX, int gridY);
	void handleCardDiceResult(int result, DicePurpose purpose);
	bool executeCardByType(const Card & playedCard, int cardIndex, int targetX, int targetY, bool & playedSuccessfully, CardPlayResult & immediateResult);
	bool executeCardGeneric(const Card & playedCard, int cardIndex, int targetX, int targetY, bool & playedSuccessfully, CardPlayResult & immediateResult);

	void updateEarthquakeSimulation();
	int resolveDiceRoll(int numDice, int sides);

	void applyReplicateCopyToHand(Player & caster, const Card & playedCard);
	void finishPlayCard(Player & caster, const Card & playedCard, int handIndex);
	void completeCardPlayAnimation(const Card & playedCard, int playerIndex);
	int applyDamageWithMitigations(Player & target, int baseDamage, DamageType type, int attackerIndex);
	void applyDamageWithMitigationsQueued(Player & target, int baseDamage, DamageType type, int attackerIndex, int outputSlot);

	Player * spawnMinionDeterministically(int summonKind, int targetX, int targetY, int ownerID, int maxHP, int ap, int summonerPlayerID = -1);
	Player initMinionFromKind(int summonKind, int ownerID, int maxHP, int ap, int summonerPlayerID = -1);

	void updatePlayerAP(Player & player, int newAP);
	void applyMovement(int playerIndex, int targetX, int targetY, int newAP, const std::vector<glm::vec2> * pathOverride = nullptr);
	void createCardDisplay(const Card & card, int playerIndex, bool forceVisibleForAllPlayers = false);
	std::string currentDiceLabel = "";
	void startVisualDiceRoll(const VisualEvent & ev);
	int resolveDiceRollDetailed(int numDice, int sides, std::vector<int> & outRaw);

	void beginEffectSequence();
	void queueEffect(const EffectOp & op);
	void updateEffectSequence();
	bool isEffectSequenceComplete() const;

	bool hasFinishedDiceRollFor(DicePurpose purpose, int ownerIndex) const;
	bool hasValidTargetForGlow(int playerIndex, int cardIndex);
	bool diceVisualsFinishedAndLinger() const;
	void recalcTempLuck();
	void checkKeyPickupAndDraftAfterSummon(int x, int y, int minionOwnerID, int preferredPlayerIndex = -1);

	int computePassiveLuck(int playerIndex);
	void spawnFloatingText(glm::vec3 pos, std::string text, ofColor color, std::string category = "");
	void queueFloatingTextVisual(glm::vec3 pos, std::string text, ofColor color, float duration = 1.2f);
	void queueVisualDiceRoll(glm::vec3 pos, int numDice, int sides, const std::vector<int> & rawResults, int totalResult, int dicePurpose = 0, int ownerIndex = -1, float duration = 1.5f);
	void queueVisualTracer(glm::vec3 start, glm::vec3 end, ofColor color = ofColor::white, float duration = 1.2f);
	void queueVisualDelay(float seconds);
	void spawnExplosion(glm::vec3 pos, int count, ofColor color);
	void tryTriggerShellSpike();

	void calculateHighlights();
	void calculateTargetHighlights(int cardToCalculate = -1);
	bool hasValidNonSelfTargetForHandIndex(int handIndex);
	void invalidateTargetCache();
	void clearHighlights();

	std::vector<int> getTileOccupants(int tx, int ty);
	bool tileHasOtherThan(int tx, int ty, int excludePlayerIndex);
	TargetInfo computeTargetInfo(const Card & card, int casterIdx, int tx, int ty);
	bool wouldAffectOtherUnit(const Card & card, int casterIdx, int tx, int ty);

	void enterTargetingMode(const TargetingContext & ctx);
	void cancelTargetingMode();

	std::mt19937 gameplayRNG;
	uint64_t gameplayRngAdvanceCount = 0;

	uint32_t consumeGameplayRngRaw() {
		++gameplayRngAdvanceCount;
		return gameplayRNG();
	}

	std::mt19937 visualRNG;

	bool gameplaySeededByHost = false;
	std::string desyncMessage;
	float waitingForSnapshotStartTime = 0.0f;
	bool processingNetworkPacket = false;
	float lastSnapshotRequestTime = 0.0f;

	int getGameRandom(int min, int max);

	template <class T, class URBG>
	void deterministic_shuffle(std::vector<T> & vec, URBG & rng) {
		if (vec.size() <= 1) return;
		for (size_t i = vec.size() - 1; i > 0; --i) {
			auto r = rng();
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
		deterministic_shuffle_gameplay(vec);
		if (ownerPlayerIndex == 0 || ownerPlayerIndex == 1) {
			startShuffleVisual(ownerPlayerIndex, visualDelaySeconds);
		}
		if (ownerPlayerIndex >= 0 && ownerPlayerIndex < (int)players.size()) {
			players[ownerPlayerIndex].deckNeedsShuffle = false;
		}
	}

	inline void shuffleGameVector(std::vector<Card> & vec, int ownerPlayerIndex = -1, float visualDelaySeconds = 0.0f) {
		if (vec.size() <= 1) return;
		std::stable_sort(vec.begin(), vec.end(), [](const Card & a, const Card & b) {
			if (a.type != b.type) return a.type < b.type;
			return a.name < b.name;
		});
		deterministic_shuffle_gameplay(vec);
		if (ownerPlayerIndex == 0 || ownerPlayerIndex == 1) {
			startShuffleVisual(ownerPlayerIndex, visualDelaySeconds);
		}
		if (ownerPlayerIndex >= 0 && ownerPlayerIndex < (int)players.size()) {
			players[ownerPlayerIndex].deckNeedsShuffle = false;
		}
	}

	void loadCardData(const std::string & filePath);

	CardType stringToCardType(const std::string & str);
	TargetingType stringToTargetingType(const std::string & str);
	DamageType stringToDamageType(const std::string & str);
	int stringToStatusType(const std::string & str);

	ofVec2f mouseToBoard(int x, int y);
	glm::vec2 getCardDisplayUIPosition(int playerIndex);
	glm::vec2 worldToGrid(glm::vec3 worldPos);
	glm::vec3 gridToWorld(int gridX, int gridY);
	bool findNearestTargetableTile(int clickGridX, int clickGridY, int & outGridX, int & outGridY);
	Player * getPlayer(int index);
	int findPlayerIndexByID(int playerID);
	std::string getPlayerDisplayName(int index);
	std::string getPlayerSteamName(int playerIndex);
	const Card * findCardByName(const std::string & name) const;

	bool isClient() const { return isMultiplayer && !steamManager.isHost(); }
	bool isHost() const { return isMultiplayer && steamManager.isHost(); }
	bool isMyTurn() const;
	bool isCurrentPlayerLocal() const;

	std::vector<glm::vec2> findShortestPath(glm::vec2 start, glm::vec2 end);
	std::vector<glm::vec2> findShortestPathForPlayer(int playerIndex, glm::vec2 start, glm::vec2 end);
	glm::quat matchFaceToCamera(glm::vec3 faceNormal);
	glm::quat getDiceFaceRotation(int sides, int rawResult, float wobbleAmount);

	struct LosResult {
		bool hasLos;
		glm::vec2 start;
		glm::vec2 end;
	};

	std::vector<Player *> findCleaveTargets(glm::vec2 direction);
	TargetInfo isLosTargetValid(glm::vec2 casterTile, glm::vec2 targetTile, float maxRangeFeet, CardType cardType);
	LosResult getClearLosRay(glm::vec2 casterTile, glm::vec2 targetTile, CardType cardType);
	long long getFaceToFaceDistanceSquaredScaled(glm::vec2 casterTile, glm::vec2 targetTile);
	bool checkRayPhysics(glm::vec2 rayStart, glm::vec2 rayEnd);
	int isGapTile(glm::vec2 tile);
	bool isTileBlocked(int x, int y);
	bool isTileWall(int x, int y);
	bool isOrthogonalPathBlocked(glm::vec2 start, glm::vec2 end);
	float getFaceToFaceDistance(glm::vec2 casterTile, glm::vec2 targetTile);
	std::vector<glm::vec2> getLineOfSightPath(glm::vec2 start, glm::vec2 end);
	glm::vec2 getClosestPointOnLineSegment(glm::vec2 p, glm::vec2 start, glm::vec2 end);

	ofRectangle draftAcceptButtonRect;

	int initiativeRolls[2] = { 0, 0 };
	bool isInitiativeRolling = false;
	int initiativeTimerFrames = 0;
	bool inSimulationTick = false;
	int pendingStartNewTurnRequests = 0;

	void requestStartNewTurn();
	int draftPlayerIndex = 0;
	int inGameDraftTargetIdx = -1;
	int draftStage = 0;
	int draftPicksRemaining = 0;
	bool isInGameDraft = false;
	std::vector<int> selectedDraftIndices;
	std::vector<Card> draftOptions;
	int currentDraftClassTier = 0;
	std::array<int, 3> currentDraftOptionPoolIndices = { { -1, -1, -1 } };
	std::vector<Card> class1Cards;
	std::vector<Card> class2Cards;
	std::vector<Card> class3Cards;

	float waitingForDraftOptionsStartTime = 0.0f;
	float waitingForDraftOptionsTimeout = 0.75f;
	int skipClientShuffleFor = -1;
	bool draftAcceptLocked = false;
	bool draftAcceptApplied = false;
	uint32_t draftGenerationCounter = 0;
	bool initialDraftComplete = false;

	InputCommandPacket lastSentActionPacket;
	bool lastSentActionValid = false;
	float lastSentActionTime = 0.0f;
	int lastSentActionResendCount = 0;
	const float ACTION_RESEND_INTERVAL = 0.75f;
	const int ACTION_MAX_RESENDS = 3;
	uint32_t actionClientActionCounter = 0;

	uint32_t watchdogClientActionCounter = 0;
	uint32_t draftClientActionCounter = 0;
	uint32_t lastProcessedActionID[2] = { 0, 0 };

	int lastLoggedDraftOptionsCount = -1;
	float lastDraftDrawLogTime = 0.0f;

	void generateDraftOptions(int classTier, const std::vector<int> * forcedIndices = nullptr);
	void applyDraftOptionsFromPool(int classTier, const std::vector<int> & indices, int picksRemaining, int draftingPlayerIdx);
	void onCardPicked(int optionIndex);
	void drawInitiativeRoll();
	void drawDraftScreen();

	void getDraftCardMetrics(bool clampTop, float & outCardW, float & outCardH, float & outSpacing, float & outStartX, float & outStartY);

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

	std::vector<DraftOptionUI> draftOptionUI;
	int draftAnimAppearFrames = 0;
	int draftAnimHoldFrames = 0;

	DraftOptionUI draftAcceptUI;
	int draftAnimVanishFrames = 0;

	struct DraftPickedMove {
		Card card;
		int startFrame = 0;
		int delayFrames = 0;
		int durationFrames = 0;
		glm::vec2 startPos;
		glm::vec2 endPos;
		bool finished = false;
		int ownerIndex = -1;
		float endScale = 1.0f;
	};

	std::vector<DraftPickedMove> activeDraftPickedMoves;

	void scheduleDraftPickedMove(const DraftPickedMove & mv);

	int deckFlashStartFrame = 0;
	int deckFlashDurationFrames = 0;
	int deckFlashOwnerIndex = -1;

	bool draftNextScheduled = false;
	int draftNextClassTier = -1;
	float draftNextAt = 0.0f;

	bool draftEndScheduled = false;
	int draftEndNextPlayerIndex = -1;
	int lastDraftOptionsPlayer = -1;
	float draftEndAt = 0.0f;

	float draftDisplayStartTime = 0.0f;
	float draftDisplayDuration = 1.0f;
	bool draftDisplayInteractiveEnabled = true;
	int draftAutoSelectedIndex = -1;

	struct PendingVisualKeyDraft {
		int tileX = -1;
		int tileY = -1;
		int targetIndex = -1;
		int classTier = -1;
		std::array<int, 3> poolIndices = { -1, -1, -1 };
	};
	std::vector<PendingVisualKeyDraft> pendingVisualKeyDraftQueue;

	void startShuffleVisual(int playerIndex, float delaySeconds = 0.0f);
	void scheduleGenerateDraftOptions(int classTier, float delaySeconds);
	void playHandFeedbackSfx(float speed = 1.0f, float volumeMul = 0.16f);

	void drawActiveDraftPickedMoves();
	void updateDraftUiAnimations();

	void buildLevelMesh();
	void buildFloorMesh();
	void allocateWorldFbo(int w, int h);

	void drawGhostRelocateUI();
	void drawOpponentMenu();
	void drawJoinedOutlines(bool highlightedTiles[BOARD_WIDTH][BOARD_HEIGHT], ofColor color, float surfaceY);
	void drawExpandingAOERings(float surfaceY);

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

	void drawOptionCards(const ofRectangle & panelRect,
		const std::string & title,
		const std::string & desc,
		const std::vector<std::string> & labels,
		const std::vector<ofColor> & accents,
		const std::vector<bool> & enabled,
		std::vector<ofRectangle> & outRects);

	bool applyDamageTo(Player & target, int damage, DamageType type, int attackerIndex = -1);
	void drawDispelUI();
	void cancelDispel();
	void determineStatusOptions(Player * target);
	void applyDispelEffect(int statusID);
	void drawMinionManagerUI();
	void drawMinionStatusBars(Player & minion, const std::string & name, float x, float y, float totalWidth, float preferredHpWidth, bool alignRight = false);

	struct PlayerMatchStats {
		int totalDamageDealt = 0;
		int maxDamageInOneTurn = 0;
		int currentTurnDamage = 0;
		int totalHealing = 0;
		int minionsSpawned = 0;
		int cardsPlayed = 0;
	};
	PlayerMatchStats matchStats[4];
	std::array<int, 4> afkStrikeCounts = { 0, 0, 0, 0 };

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

	bool g_isGameOver = false;
	int g_winnerID = -1;
	GameState currentState = STATE_MAIN_MENU;
	GameState prevState = STATE_MAIN_MENU;
	GameState stateBeforeSettings = STATE_MAIN_MENU;
	GameState pausedFromState = STATE_GAMEPLAY;
	bool isLoadingGame = false;
	int globalTurnCounter = 0;
	int nextSummonOrder = 0;

	ofRectangle mainMenuHostButton;
	ofRectangle mainMenuInviteButton;
	ofRectangle saveGameButtonRect;
	ofRectangle loadGameButtonRect;
	ofRectangle gameOverReturnBtn;
	ofRectangle gameOverReplayBtn;

	void drawSingleplayerMenu();

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

	float settingsMasterVolume = 1.0f;
	float settingsMenuVolume = 0.6f;
	float settingsSfxVolume = 0.8f;

	std::vector<std::pair<std::string, int>> settingsKeyBindings;
	int settingsRebindingIndex = -1;

	ofRectangle settingsTabVideoRect;
	ofRectangle settingsTabAudioRect;
	ofRectangle settingsTabGameRect;
	ofRectangle settingsTabControlsRect;
	ofRectangle settingsAudioVolumeSlider;
	ofRectangle settingsAudioMasterSlider;
	ofRectangle settingsAudioSfxSlider;
	ofRectangle settingsGameShowFPSBox;
	ofRectangle settingsCameraSensitivitySlider;
	ofRectangle settingsInvertYBox;
	ofRectangle settingsUIScaleSlider;
	ofRectangle settingsVSyncBox;
	ofRectangle settingsShowHintsBox;

	bool settingsShowFPS = true;
	float settingsCameraSensitivity = 1.0f;
	bool settingsInvertCameraY = false;
	float settingsUIScale = 1.0f;
	bool settingsUseVSync = true;
	bool settingsShowHints = true;

	void loadSettings();
	void saveSettings();

	bool saveGameStateToFile(const std::string & path);
	bool loadGameStateFromFile(const std::string & path);
	void pruneOldSaves(int keepCount);

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

	const float TILE_SIZE = 5.0f;
	Tile board[BOARD_WIDTH][BOARD_HEIGHT];
	TargetInfo targetCache[BOARD_WIDTH][BOARD_HEIGHT];
	std::deque<Player> players;
	std::set<int> renderLoggedMinions;
	int currentPlayerIndex = -1;

	int turnTimerFramesPerSecond = 60;
	int turnStartFrame = 0;
	int turnDurationFrames = 90 * 60;
	bool turnTimerEnabled = true;

	bool turnStartDeferred = false;
	int turnStartDeferredAtFrame = 0;

	bool turnTimerPaused = false;
	int turnTimerPausedRemainingFrames = 0;
	bool reconnectTurnTimerPausedByDisconnect = false;
	int reconnectTurnTimerPausedRemainingFrames = 0;

	int pendingTurnStartVisuals = -1;
	bool waitingForReconnect = false;

	bool opponentDecisionTimerActive = false;
	uint32_t opponentDecisionStartFrame = 0;
	int opponentDecisionDurationFrames = 30 * 60;
	int opponentDecisionPlayerIndex = -1;

	int currentTurnOwnerID = -1;
	bool currentTurnHadMeaningfulAction = false;
	bool currentTurnTimeoutProcessed = false;
	float reconnectForfeitStartTime = -1.0f;
	float reconnectForfeitDuration = 60.0f;

	std::vector<DeathMarker> graveyard;
	std::vector<FloatingText> activeFloatingTexts;
	std::vector<Particle> particles;
	float screenShake = 0.0f;

	struct Tracer {
		glm::vec3 start;
		glm::vec3 end;
		glm::ivec2 impactTile;
		std::vector<glm::ivec2> adjacentTiles;
		float startTime = 0.0f;
		float duration = 5.0f;
		ofColor color = ofColor::white;
	};

	std::vector<Tracer> activeTracers;

	void spawnTracer(glm::vec3 start, glm::vec3 end, glm::ivec2 impactTile, ofColor color, float duration = 5.0f);
	void spawnTracerWithAdjacent(glm::vec3 start, glm::vec3 end, glm::ivec2 impactTile, const std::vector<glm::ivec2> & adjacentTiles, ofColor color, float duration = 5.0f);
	void computeTracerEndpoints(glm::vec2 casterTile, glm::vec2 hitGridFrac, glm::vec3 & outStart, glm::vec3 & outEnd);

	struct H2HRecord {
		std::string opponentName;
		int wins = 0;
		int losses = 0;
		int draws = 0;
	};

	std::map<std::string, H2HRecord> h2hStatsMap;
	std::vector<std::pair<int, ofRectangle>> assistantRerollButtons;

	void loadH2HStats();
	void saveH2HStats();
	void recordH2HOutcome(const std::string & opponentSteamID, const std::string & opponentName, int outcome);

	std::vector<ofTexture> keyTextures;
	std::vector<ofTexture> keyTexturesSilver;
	std::vector<ofTexture> keyTexturesBronze;
	std::vector<int> keyAnimSequence;
	int keyAnimSeqPos = 0;
	float keyAnimTimer = 0.0f;
	float keyAnimInterval = 1.0f / 6.0f;
	std::vector<float> keyAnimSpeedPresets = { 1.0f };
	int keyAnimSpeedIndex = 0;
	int keyAnimTileX = BOARD_WIDTH / 2;
	int keyAnimTileY = BOARD_HEIGHT / 2;

	struct FloatingKey {
		glm::ivec2 pos;
		int set;
	};
	std::vector<FloatingKey> floatingKeyInstances;
	float keyRenderScale = 0.75f;

	int currentAP = 0;
	bool hasDrawnCardsThisTurn = false;
	bool opponentHasDrawnCardsThisTurn = false;
	bool isHandlingTurnStartEffects = false;
	PlayerActionState playerAction = NONE;
	int selectedPieceGridX = -1;
	int selectedPieceGridY = -1;
	int lastCachedPlayerX = -1, lastCachedPlayerY = -1;

	ofCamera cam;
	ofCamera cam2;
	ofLight headlight;
	ofLight keyLight;
	ofLight rimLight;
	ofLight uiLight;
	std::vector<ofLight> lights;
	float lightNoiseOffset = 0.0f;
	float cameraTargetZoom = 35.0f;

	struct NetworkPending {
		bool keyDraftAccept = false;
		int keyDraftPlayer = -1;
		int keyDraftPlayerID = -1;
		int keyDraftClass = 0;
		float keyDraftTriggerTime = 0.0f;
		int keyDraftKeyX = -1;
		int keyDraftKeyY = -1;

		bool draftFinalize = false;
		bool draftShuffleNeeded = false;
		std::vector<int> draftQueue;

		bool draftStateAvailable = false;
		DraftStatePacket draftState;

		std::map<int, int> actionByActor;
		int networkActionActor = -1;
		int networkActionPrevPlayer = -1;

		int saveBrowserPendingIndex = -1;
		bool saveBrowserConfirmVisible = false;
	} networkPending;

	std::set<int> recentPlaceSentIndices;
	float cameraCurrentZoom = 35.0f;
	glm::vec3 cameraTargetPan = glm::vec3(0, 0, 0);
	glm::vec3 cameraCurrentPan = glm::vec3(0, 0, 0);
	bool isTopDownView = false;
	glm::vec3 cameraCurrentPos;
	glm::vec3 cameraCurrentPos2;
	glm::vec3 cameraCurrentLookAt;
	glm::vec3 cameraCurrentLookAt2;
	float last3DZoom = 35.0f;
	float lastWindowWidth = 0;
	float lastWindowHeight = 0;
	bool draftingCameraLockedToClient = false;

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
	ofxAssimpModelLoader faerieModel;

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
	ofTexture blobShadowTex;
	float blobShadowSize = 1.2f;
	ofImage fireTexture;

	ofFbo worldFbo;
	ofShader worldPostShader;
	bool worldPostShaderLoaded = false;
	bool enableWorldPostProcess = true;
	bool showWorldFboPreview = true;

	ofShader c64Shader;
	bool c64ShaderLoaded = false;
	bool enableC64Shader = false;
	float c64ScanlineIntensity = 0.25f;

	ofFbo bloomFboA;
	ofFbo bloomFboB;
	ofShader bloomExtractShader;
	ofShader bloomBlurShader;
	int bloomDownscale = 4;
	float bloomThreshold = 0.7f;
	int bloomBlurPasses = 2;
	bool bloomLoaded = false;
	bool bloomActiveNotified = false;

	ofFbo shadowFbo;
	ofShader shadowDepthShader;
	ofShader pbrShader;
	int shadowMapSize = 2048;
	glm::mat4 lightViewProj;
	bool shadowDepthShaderLoaded = false;
	bool pbrShaderLoaded = false;
	bool enableShaders = false;

	ofFbo pixelLowFbo;
	ofShader pixelArtShader;
	bool pixelArtShaderLoaded = false;
	bool enablePixelArt = true;
	bool pixelArtWarned = false;
	bool pixelArtActiveNotified = false;
	bool pixelArtDumpedPixels = false;
	bool worldPostActiveNotified = false;

	void applyPixelArtSettings();
	int pixelArtDownscale = 2;
	int pixelArtLevels = 20;
	bool pixelArtDither = true;

	bool disableAllGlow = false;

	bool isPlayerAnimating = false;
	int animatingPlayerIndex = -1;
	glm::vec3 playerVisualPos;
	float playerFacingAngle = 0.0f;
	std::vector<glm::vec3> animationPath;
	int currentPathIndex = 0;
	float animationSegmentStartTime = 0.0f;
	float movementHopHeight = 0.35f;

	float cameraShakeIntensity = 0.0f;
	float cameraShakeTimer = 0.0f;
	float cameraShakeDuration = 0.0f;
	glm::vec3 cameraShakeOffset = glm::vec3(0.0f);

	void triggerCameraShake(float intensity, float duration);
	int getOwnerIdForActorIndex(int actorIndex) const;
	int getNormalTurnDurationFramesForActorIndex(int actorIndex) const;
	void markMeaningfulActionOnCurrentTurn();
	void registerAfkTimeoutForCurrentOwner();
	void handleOwnerForfeit(int loserOwnerId, const std::string & reason);
	int getActiveTurnDurationFrames() const;
	void resetDraftPhaseTimerWindow();

	void pauseTurnTimerForOpponentDecision(int decidingPlayerIndex);
	void resumeTurnTimerIfPausedForOpponent(int decidingPlayerIndex);

	float menuOpenScale = 1.0f;
	float menuOpenStartTime = 0.0f;
	float menuOpenDuration = 0.2f;
	std::vector<glm::vec2> hoverPath;
	glm::vec2 lastHoverGridPos = { -1, -1 };

	std::vector<PlayedCardDisplay> activeCardDisplays;
	std::vector<StolenCardAnimation> activeStolenCardAnimations;
	std::vector<PlayedCardAnimation> activePlayedCardAnimations;
	std::vector<RemovedCardAnimation> activeRemovedCardAnimations;

	ofImage cardSpriteSheet;
	ofImage cardBackImage;
	std::vector<Card> allCards;

	int selectedCardIndex = -1;
	int draggedCardIndex = -1;
	int pressedCardIndex = -1;
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
	bool endTurnLocked = false;

	ofTrueTypeFont uiFont;
	ofTrueTypeFont titleFont;
	ofTrueTypeFont cardEffectFont;

	ofFbo modelFbo;
	std::vector<MinionUI> activeMinionUIs;

	CardInteractionState cardInteractionState = CARD_INTERACTION_STATE_IDLE;
	int interactingCardIndex = -1;
	int interactingCardType = CARD_NONE;
	int interactionTargetIndex = -1;
	std::string interactionMenuChoice;
	bool interactionNeedsStatusSelect = false;
	int interactionDiceRoll = 0;
	glm::vec2 interactionTargetTile;
	std::string interactingCardName;

	TargetingContext targetingContext;

	bool isShowingTooltip = false;
	ofVec2f tooltipPos;
	std::string tooltipText;
	bool isTooltipExpanded = false;
	std::string tooltipExpandedText;

	bool isHoveringPile = false;
	PileViewMode hoveredPileType = VIEW_NONE;

	bool isHoveringUnit = false;
	float unitHoverStartTime = 0.0f;
	int hoveredUnitIndex = -1;
	int hoveredPilePlayerIndex = -1;
	float pileHoverStartTime = 0.0f;

	bool isShowingPileView = false;
	PileViewMode currentPileView = VIEW_NONE;
	int currentPileViewPlayerIndex = -1;

	void drawPileViewFor(int viewPlayerIndex, PileViewMode viewMode);
	std::vector<Card> cardsToShowInView;
	ofRectangle pileViewRect;
	ofRectangle p0_deckRect, p0_discardRect;
	ofRectangle p1_deckRect, p1_discardRect;
	ofRectangle p0_apStatusRect;
	ofRectangle p1_apStatusRect;

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

	std::vector<InputCommandPacket> commandQueue;
	std::unordered_set<uint64_t> queuedCommandKeys;
	std::unordered_set<uint64_t> executedCommandKeys;
	uint32_t nextCommandId = 1;
	uint32_t lastProcessedCommandId = 0;
	int lastTurnStartSentPlayer = -1;
	int lastTurnStartSentCounter = -1;

	std::map<uint32_t, std::string> provisionalSnapshots;
	std::map<uint32_t, InputCommandPacket> provisionalCommands;

	const float SIMULATION_TIMESTEP = 1.0f / 60.0f;
	float simulationAccumulator = 0.0f;
	uint32_t simulationFrame = 0;

	EffectSequence currentEffectSequence;
	bool isProcessingEffect = false;
	bool isExecutingLockstepCommand = false;

	void queueInputCommand(const InputCommandPacket & cmd);
	void processCommandQueue();
	void simulationTick();
	bool sendInputCommand(InputCommandPacket & cmd, bool applyLocally = true);
	void executeInputCommand(const InputCommandPacket & cmd);
	bool processEffectOp(EffectOp & op);

	CardOutcome currentCardOutcome;
	CardPlayState cardPlayState = CARD_PLAY_STATE_IDLE;
	int activeCardIndex = -1;

	void resetCardState() {
		currentCardOutcome = CardOutcome();
		cardPlayState = CARD_PLAY_STATE_IDLE;
		activeCardIndex = -1;
		resetCardInteraction();
	}
	void resetCardInteraction(bool syncNetwork = true);

	int amnesiaTargetPlayerIndex = -1;
	int numCardsToRemove = 0;
	std::vector<Card> amnesiaDeckCopy;
	std::vector<int> amnesiaSelectedIndices;
	std::vector<ofRectangle> amnesiaCardRects;
	std::vector<int> amnesiaSelectionFromCmd;
	bool amnesiaSelectionFromCmdPresent = false;
	int amnesiaChooserPlayerID = -1;

	int blockingBoonTargetIndex = -1;
	int blockingBoonPendingCasterIndex = -1;
	std::vector<int> blockingBoonPendingCoinRawResults;
	std::vector<int> blockingBoonPendingD20RawResults;
	bool blockingBoonPendingPhysicalAfterDraft = false;
	bool blockingBoonActive = false;

	int magicBlastTargetPlayerIndex = -1;
	int magicBlastChoicesRemaining = 0;
	std::vector<int> magicBlastSplashTargetIndices;
	ofRectangle magicBlastDamageButton;
	ofRectangle magicBlastDiscardButton;

	std::vector<int> psionicWaveTargetIndices;
	glm::vec2 fireballImpactTile;

	ofRectangle trainMenuRect;
	ofRectangle trainBtnAP;
	ofRectangle trainBtnDraft;

	void drawTrainMenuUI();

	int dispelMode = 0;
	ofRectangle dispelMenuRect;
	ofRectangle dispelBtnBarrier;
	ofRectangle dispelBtnPurge;
	ofRectangle statusSelectMenuRect;
	std::vector<ofRectangle> statusSelectButtons;
	std::vector<std::string> statusSelectLabels;

	struct EarthquakeState {
		int playerIndex;
		int tilesToMove;
		int originalDistance;
		int diceIndex;
		glm::ivec2 direction;
		glm::ivec2 startGrid;
		glm::ivec2 nextGrid;
		glm::vec3 visualPos;
		bool isMoving;
		bool crashed;
		bool crashDamageApplied = false;
		int crashDiceLastStep = -1;
	};

	bool isEarthquakeActive = false;
	bool isEarthquakeDiceRolling = false;
	bool isEarthquakeAnimatingStep = false;
	float earthquakeT = 0.0f;
	std::vector<EarthquakeState> earthquakeUnits;
	int earthquakeStep = 0;
	float earthquakeWaitTimer = 0.0f;
	int earthquakeDiceAssignCounter = 0;

	struct EarthquakeDamageTarget {
		int playerID = -1;
		int playerIndex = -1;
		glm::vec3 visualPos = glm::vec3(0);
		int gridX = -1;
		int gridY = -1;
		int blackboardSlot = -1;
	};

	std::vector<EarthquakeDamageTarget> earthquakeDamageTargets;

	ofRectangle wisdomMenuRect;
	ofRectangle wisdomBtnDamage;
	ofRectangle wisdomBtnBlock;

	ofRectangle burstMenuRect;
	ofRectangle burstBtnDamage;
	ofRectangle burstBtnHeal;

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

	ofRectangle doubleHandedMenuRect;
	ofRectangle btnAddPunches;
	ofRectangle btnAddBlocks;

	struct OpponentInteraction {
		bool open = false;
		int type = 0;
		int targetIndex = -1;
		int hoveredChoice = -1;
		int cardIndex = -1;
	} opponentInteraction;

	std::vector<glm::ivec2> ghostRelocateChoices;
	std::vector<ofRectangle> ghostRelocateButtons;
	int ghostRelocateTargetIndex = -1;

	const int PSEUDO_CARD_MAGIC_HAND_RELOCATE = 998;
	const int MENU_MAGIC_HAND_RELOCATE = 999;

	ofRectangle amnesiaMenuRect;
	ofRectangle amnesiaBtnSelf;
	ofRectangle amnesiaBtnAdjacent;

	std::vector<int> renewedSelectedHandIndices;
	ofRectangle riConfirmBtn;
	ofRectangle riCancelBtn;

	int wolvesRemainingToPlace = 0;
	int wolfPlacementSourceX = -1;
	int wolfPlacementSourceY = -1;
	int wolfSummonCount = 0;
	int wolfSummonStage = 0;

	int koboldsRemainingToPlace = 0;
	int koboldPlacementSourceX = -1;
	int koboldPlacementSourceY = -1;
	int koboldSummonCount = 0;
	int koboldSummonStage = 0;

	int remoteKoboldPlacementSourceX = -1;
	int remoteKoboldPlacementSourceY = -1;
	int remoteKoboldsRemaining = 0;

	glm::ivec2 magicHandTargetTile;
	int magicHandPushedUnitIndex = -1;
	glm::ivec2 magicHandPushDir;

	void drawCardSpawnerUI();
	void drawCardEncyclopediaUI();

	void drawInstructionText(const std::string & message, ofColor color = ofColor::white);
	void drawDiceLabel(const std::string & message, ofColor color = ofColor(255, 215, 0), float yPos = 0);
	void drawMenuOverlay();
	void drawMenuBackground(const ofRectangle & menuRect, float cornerRadius = 15);
	void drawMenuTitle(const std::string & title, const ofRectangle & menuRect, float yOffset = 60);

	void resetCardToBaseStats(Card & card);
	void updateCardInteractionState(CardInteractionState newState, int cardIdx = -1, int cardType = CARD_NONE);
	void handleCardDragToPlay(int cardIndex);
	void handleCardTargetClick(int gridX, int gridY);
	void handleCardMenuClick(const std::string & buttonId);
	void drawActiveCardInteractionUI();
	void cancelAllTargeting();
	void cancelMagicHand();

	ofRectangle mainMenuOnlineButton;
	ofRectangle mainMenuSingleplayerButton;
	ofRectangle mainMenuCustomisationButton;
	ofRectangle mainMenuEncyclopediaButton;
	ofRectangle mainMenuSettingsButton;
	ofRectangle mainMenuQuitButton;

	ofRectangle singleplayerNewGameButton;
	ofRectangle singleplayerContinueButton;
	ofRectangle singleplayerLoadButton;
	ofRectangle singleplayerReplayButton;
	ofRectangle singleplayerBackButton;
	ofRectangle mainMenuLocalPvPButton;
	ofRectangle mainMenuVsAIButton;

	int mainMenuHoveredIndex = -1;

	glm::vec2 mainMenuCirclePos = { 6.0f, 6.0f };
	std::vector<glm::vec2> mainMenuCirclePath;
	float menuTileSize = 0;
	float menuStartX = 0;
	float menuStartY = 0;
	float baseMenuStartX = 0;

	void draw2DMenuBackground();
	void drawMenuPlaqueButton(const ofRectangle & rect, const std::string & text, bool isHovered, bool isOnline = false);
	void drawMenuPlaquePanel(const ofRectangle & rect);
	void triggerMenuTransition(bool fromLeft);

	float currentMenuPanX = 0.0f;
	float targetMenuPanX = 0.0f;
	bool isWaitingForMenuTransition = false;
	float pendingMenuPanX = 0.0f;
	GameState pendingMenuState = STATE_MAIN_MENU;
	int targetMenuScreen = 0;
	void navigateToMenu(int screenIndex, GameState newState);
	void updateMenuRects();
	std::vector<glm::vec2> getInfiniteMazePath(glm::vec2 start, glm::vec2 end);

	int encyclopediaMainTab = 0;
	float encyclopediaMainScroll = 0.0f;
	ofRectangle encyTabCards, encyTabMinions, encyTabRules, encyBtnBack;
	int encyclopediaMainHoveredIndex = -1;
	float encyclopediaMainHoverStartTime = 0.0f;
	bool encyclopediaMainHoverScaled = false;

	void drawEncyclopediaState();

	ofRectangle customisationBtnBack;
	void drawCustomisationState();

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
	float settingsFramerateSliderValue = 0.0f;
	int currentFramerateIndex = 0;
	bool isFullscreen = false;

	ofRectangle pauseMenuResumeButton;
	ofRectangle pauseMenuSettingsButton;
	ofRectangle pauseMenuQuitButton;
	int pauseMenuHoveredIndex = -1;

	ofRectangle pauseMenuSaveButton;
	ofRectangle pauseMenuLoadButton;

	void drawSaveBrowser();
	ofRectangle saveBrowserBackButton;
	int saveBrowserHoveredIndex = -1;
	std::vector<std::string> saveFilePaths;
	std::vector<ofRectangle> saveFileRects;
	ofRectangle saveBrowserConfirmLoadButton;
	ofRectangle saveBrowserConfirmCancelButton;
	GameState saveBrowserReturnState = STATE_PAUSED;

	std::vector<DiceRoll> activeDiceRolls;
	float hostWaitingForClientsReadyStartTime = 0.0f;
	std::set<uint32_t> clientsReady;
	bool clientSentReady = false;

	bool gameSuspendedDueToInactivity = false;
	float savedMasterVolume = 1.0f;
	bool savedMainMenuWasPlaying = false;
	bool musicMutedDueToMinimize = false;
	float savedMainMenuVolume = 0.6f;
	std::vector<float> savedFootstepVolumes;
	int savedMainMenuPositionMS = 0;
	float diceSpinSpeed = 1500.0f;
	ofMesh d6Mesh, d4Mesh, d20Mesh, d10Mesh, coinMesh;
	ofTexture d6Texture, d4Texture, d20Texture, d10Texture, coinFacesTexture;
	std::vector<ofSoundPlayer> footstepSounds;
	ofSoundPlayer cardHoverSound;
	ofSoundPlayer draggingHandLoop;
	bool draggingHandLoopPlaying = false;
	float draggingHandTargetVolume = 0.0f;
	float draggingHandFadeSpeed = 8.0f;
	bool draggingWasActive = false;
	std::string diceRollResultText = "";
	float diceRollResultStartTime = 0.0f;
	float diceRollResultDuration = 3.5f;

	bool isDebugMode = true;
	bool isSpawningUnit = false;
	bool hasUnlimitedAP = false;
	bool skipChecksumValidation = false;

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

	bool isCardSpawnerOpen = false;
	bool isCardEncyclopediaOpen = false;
	std::string cardSpawnerInput = "";
	int cardSpawnerQuantity = 1;
	std::vector<Card> filteredCards;
	int encyclopediaScrollOffset = 0;
	int settingsControlsScrollOffset = 0;
	float pileViewScrollOffset = 0.0f;
	float amnesiaScrollOffset = 0.0f;
	ofRectangle cardSpawnerInputRect;
	ofRectangle cardSpawnerPlusButton;
	ofRectangle cardSpawnerMinusButton;
	ofRectangle cardSpawnerEncyclopediaButton;
	ofRectangle cardSpawnerCloseButton;
	ofRectangle encyclopediaCloseButton;
	ofRectangle encyclopediaRect;

	enum EncyclopediaMode {
		ENC_NONE = 0,
		ENC_SPAWN_TO_HAND,
		ENC_ADD_FROM_ALL,
		ENC_REMOVE_FROM_PILE
	};
	EncyclopediaMode encyclopediaMode = ENC_NONE;
	int encyclopediaTargetPlayerIndex = -1;
	bool encyclopediaTargetIsDiscard = false;
	std::vector<int> encyclopediaSelectedIndices;
	ofRectangle encyclopediaAcceptButton;
	int encyclopediaHoveredIndex = -1;
	float encyclopediaHoverStartTime = 0.0f;
	bool encyclopediaHoverScaled = false;

	ofImage cursorSheet;

	GLFWcursor * glfwArrow = nullptr;
	GLFWcursor * glfwHandPoint = nullptr;
	GLFWcursor * glfwHandOpen = nullptr;
	GLFWcursor * glfwHandClosed = nullptr;

	CursorState currentCursor = CURSOR_DEFAULT;
	CursorState previousCursor = CURSOR_DEFAULT;

	std::vector<DrawCardAnimation> activeDrawCardAnimations;
	std::vector<DrawCardAnimation> activeDiscardCardAnimations;

	struct ShuffleAnimation {
		int playerIndex = -1;
		ofRectangle deckRect;
		float startTime = 0.0f;
		float duration = 0.9f;
		float currentAlpha = 255.0f;
		float currentScale = 1.0f;
		float rotation = 0.0f;
	};

	std::vector<ShuffleAnimation> activeShuffleAnimations;

	struct ExpandingAOERing {
		glm::ivec2 centerTile = { -1, -1 };
		int maxRadiusFeet = 0;
		float startTime = 0.0f;
		float duration = 1.5f;
		int cardType = -1;
	};

	ExpandingAOERing activeAOERing;
	std::vector<ExpandingAOERing> activeMagicBoltAOERings;
	float lastMagicBoltAOERingSpawnTime = -1000.0f;

	struct ChatMessage {
		std::string playerName;
		std::string message;
		float timestamp;
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
	bool isChatMinimized = true;
	std::string chatInput = "";
	float chatScrollOffset = 0;
	float lastChatInteractionTime = -999.0f;
	ChatTab currentChatTab = ChatTab::CHAT;
	ofRectangle chatWindowRect;
	const int maxChatMessages = 50;
	const int maxLogEntries = 100;
	const float chatMessageLifetime = 10.0f;
	const float chatVisibilityDuration = 5.0f;
	const int maxChatInputLength = 150;

	void addGameLog(const std::string & logText);

	enum HoverType {
		HOVER_NONE = 0,
		HOVER_UNIT = 1,
		HOVER_DECK = 2,
		HOVER_DISCARD = 3,
		HOVER_HAND_CARD = 4,
		HOVER_UNIT_SELECTED = 5
	};

	HoverType localHoverType = HOVER_NONE;
	int localHoverGridX = -1;
	int localHoverGridY = -1;
	int localHoverCardIndex = -1;

	HoverType opponentHoverType = HOVER_NONE;
	int opponentHoverGridX = -1;
	int opponentHoverGridY = -1;
	int opponentHoverCardIndex = -1;

	void updateAndSendHover(HoverType type, int gridX = -1, int gridY = -1, int cardIndex = -1);
	void drawTileGlow(int gridX, int gridY, ofColor color, float thickness = 0.15f);

	int getLocalPlayerIndex() const;
	bool isLocalDraftingPlayer(int draftIndex) const;
};