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
#include <limits>
#include <queue>
#include <random>
#include <set>
#include <string>
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
	CARD_NECRO_BLESSING,
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
	CARD_SUMMON_KOBOLD_KING, // Add this
	CARD_SUMMON_ASSISTANT,
	CARD_SUMMON_FAERIE,
	CARD_FOUR_LEAF_CLOVER,
	CARD_SPRINT,
	CARD_SMITE,
	CARD_BURST_OF_LIGHT,
	CARD_SHOOT_ARROW,
	CARD_FULL_RESTORE,
	CARD_TRAIN, // <--- Add
	CARD_STUDY,
	CARD_BLOCKING_BOON,
	CARD_CONSTITUTION_BOON
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
	PURPOSE_SPARK_OF_GENIUS_DRAW,
	PURPOSE_PSIONIC_WAVE_RANGE,
	PURPOSE_PSIONIC_WAVE_AMOUNT,
	PURPOSE_EARTHQUAKE_DISTANCE,
	PURPOSE_EARTHQUAKE_DAMAGE,
	PURPOSE_MAGIC_HAND_DAMAGE,
	PURPOSE_LESSER_HEAL,
	PURPOSE_BLOCKING_BOON_COIN, // <--- Add
	PURPOSE_BLOCKING_BOON_D20
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
	bool isCopied = false;
	int cardClass = 1;
};

struct PlayedCardDisplay {
	Card card;
	float startTime;
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
	glm::vec2 pos; // Center of screen
	float currentScale = 2.0f;
	float currentAlpha = 255.0f;
};

struct RemovedCardAnimation {
	Card card;
	glm::vec2 startPos;
	float startTime;
	float currentScale;
	float currentAlpha;
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
};

struct Player {
	int x;
	int y;
	int health = 15;
	int maxHealth = 15;
	int block = 0;
	int ward = 0;
	int fortification = 0; // Temporary fortify from Fortify card (blocks physical/piercing)
	int barrier = 0;
	int holyBlock = 0;
	int luck = 0;
	int bonusTurns = 0;
	int playerID = 0;
	float facingAngle = 0.0f; // 0 = North, 90 = East, 180 = South, 270 = West
	bool onFire = false;
	bool hasRegeneration = false;

	// Status & Buffs
	int nextTurnAPBonus = 0;
	int shocksPlayedThisTurn = 0;
	bool flurryOfFistsActive = false; // Double hand-related damage, 0 AP cost for drawn hand cards
	bool isParalyzed = false;
	int paralysisHeadsCount = 0;
	bool isPoisoned = false;
	int poisonReduction = 0; // 0 = first turn (full 1d6), then 1, 2, 3, 4, 5, 6 (cured)
	bool nextAttackAddPoison = false; // Buff from Add Poison card
	bool nextTurnD10AP = false;
	bool nextTurnExtraDraw = false;
	bool isReplicatePending = false;
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
	bool pendingTortoiseDamage = false; // If we need to deal damage to adjacent after block/heal
	int pendingTortoiseDamageValue = 3;

	ofTexture * minionTexture = nullptr;
	int ownerID = -1;

	// Ghost Form
	bool inGhostForm = false;
	int ghostDamageTaken = 0;
	Card ghostFormCard;

	// Piles
	std::vector<CardType> cardsPlayedThisTurn;
	std::vector<Card> playedCardsPile;
	std::vector<Card> hand;
	std::vector<Card> deck;
	std::vector<Card> discardPile;
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
	glm::vec3 worldPos; // Where it started in 3D
	glm::vec3 velocity; // Upward drift
	float startTime;
	float duration = 1.5f;
	ofColor color;
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
	void setup();
	void update();
	void draw();
	void exit();

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
	void sendActionPacket(int cardIndex, int tx, int ty, int cost);
	void executeAction(const ActionPacket & pkt);
	long long calculateChecksum();

	// Anti-cheat: Log deck states for verification
	void logDeckStates(const std::string & reason);
	std::string getDeckStateString(const Player & p);

	float lastHandshakeRequestTime = 0.0f;
	uint32_t currentMapSeed = 0;

	// Steam
	SteamManager steamManager;
	bool isMultiplayer = false;
	int myLocalPlayerID = 0; // 0 = Host, 1 = Client
	bool hasReceivedHandshake = false;
	std::string player0SteamName = "Player 1";
	std::string player1SteamName = "Player 2";

	// Camera perspective: Each player sees themselves in bottom-left, opponent in top-right
	bool shouldFlipCamera() const { return isMultiplayer && myLocalPlayerID == 1; }
	ofCamera & getActiveCamera(); // Returns appropriate camera based on player (cam or cam2)
	glm::vec3 transformGridToWorld(int gx, int gy); // Applies camera flip if needed
	glm::ivec2 transformWorldToGrid(glm::vec3 worldPos); // Applies camera flip if needed
	int getVisualPlayerIndex(int actualPlayerIndex); // Converts actual player index to visual (flipped for client)

private:
	// -------------------------------------------------------------------------
	//                              CORE SYSTEMS
	// -------------------------------------------------------------------------

	// Last AP roll parameters (used for assistant auto-reroll)
	int lastAPDiceNum = 0;
	int lastAPDiceSides = 0;
	void setupGame();

	// Initialize shared game state (board, players, camera)
	void initializeGameStateCommon();

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

	// -------------------------------------------------------------------------
	//                              GAMEPLAY LOGIC
	// -------------------------------------------------------------------------
	void drawCard();
	void playCard(int cardIndex, int targetX, int targetY);
	std::string currentDiceLabel = "";
	int startDiceRoll(int numDice, int sides, DicePurpose purpose, std::string label = "", int ownerIndex = -1);
	void recalcTempLuck();

	// Returns passive luck (from assistant auras and cards in deck) for the given player index.
	int computePassiveLuck(int playerIndex);
	void spawnFloatingText(glm::vec3 pos, std::string text, ofColor color);
	void spawnExplosion(glm::vec3 pos, int count, ofColor color);
	void tryTriggerShellSpike(); // Tortoise Form: trigger 3 damage to adjacent unit

	void calculateHighlights();
	void calculateTargetHighlights(int cardToCalculate = -1);
	void invalidateTargetCache();
	void clearHighlights();

	// --- DETERMINISTIC RNG ---
	// The synced Random Number Generator
	std::mt19937 gameplayRNG;

	// Visual RNG (local only, not part of deterministic gameplay)
	std::mt19937 visualRNG;

	// Flag set when the host-provided gameplay seed has been applied
	bool gameplaySeededByHost = false;

	// Desync message shown when checksum fails
	std::string desyncMessage;

	// If true client should wait for host TurnStart packet before performing AP roll
	bool waitingForTurnStartFromHost = false;

	// Helper to get synced numbers
	int getGameRandom(int min, int max);

	// Shuffle a vector deterministically. If `ownerPlayerIndex` is >= 0 and we are
	// in multiplayer, the host will generate a nonce, shuffle with a local PRNG
	// seeded by that nonce and broadcast a `PKT_SHUFFLE` so clients reproduce the same
	// shuffle without consuming `gameplayRNG` on their side. Clients will skip shuffling
	// here when `ownerPlayerIndex >= 0` and wait for the shuffle packet.
	template <class T>
	void shuffleGameVector(std::vector<T> & vec, int ownerPlayerIndex = -1) {
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
			std::shuffle(vec.begin(), vec.end(), shuffleRng);

			// Broadcast shuffle to clients
			ShufflePacket sp = {};
			sp.type = PKT_SHUFFLE;
			sp.playerID = myLocalPlayerID;
			sp.playerIndex = ownerPlayerIndex;
			sp.nonce = nonce;
			steamManager.sendPacket(&sp, sizeof(sp));
			ofLogNotice("Network") << "Host sent Shuffle packet: player=" << sp.playerIndex << " nonce=" << sp.nonce;
			return;
		}

		// Singleplayer or generic shuffle: use gameplayRNG
		std::shuffle(vec.begin(), vec.end(), gameplayRNG);
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
	glm::vec2 worldToGrid(glm::vec3 worldPos);
	glm::vec3 gridToWorld(int gridX, int gridY);
	Player * getPlayer(int index);
	std::string getPlayerDisplayName(int index);
	std::string getPlayerSteamName(int playerIndex); // For player names (Steam)

	// Network helpers
	bool isClient() const { return isMultiplayer && !steamManager.isHost(); }
	bool isHost() const { return isMultiplayer && steamManager.isHost(); }
	bool isMyTurn() const;
	bool isCurrentPlayerLocal() const;

	std::vector<glm::vec2> findShortestPath(glm::vec2 start, glm::vec2 end);
	glm::quat matchFaceToCamera(glm::vec3 faceNormal);

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
	bool pendingDraftStateAvailable = false; // If a state packet arrives while we're waiting, stash it
	DraftStatePacket pendingDraftState;

	// Debug logging helpers: remember last logged draft options count so we only spam logs
	int lastLoggedDraftOptionsCount = -1;
	float lastDraftDrawLogTime = 0.0f;

	void generateDraftOptions(int classTier, const std::vector<int> * forcedIndices = nullptr);
	void applyDraftOptionsFromPool(int classTier, const std::vector<int> & indices, int picksRemaining, int draftingPlayerIdx);
	void onCardPicked(int optionIndex);
	void drawInitiativeRoll();
	void drawDraftScreen();

	// -------------------------------------------------------------------------
	//                          RENDERING & MESHES
	// -------------------------------------------------------------------------
	void buildLevelMesh();
	void buildFloorMesh();

	void allocateWorldFbo(int w, int h);

	// Specific UI Drawers
	void drawMagicBlastChoiceUI();
	void drawWisdomBoonUI();
	void cancelBurst();
	void drawBurstUI();

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
	void cancelWisdomBoon();
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
	GameState stateBeforeSettings = STATE_MAIN_MENU;
	GameState pausedFromState = STATE_GAMEPLAY; // Default fallback
	bool isLoadingGame = false;
	int globalTurnCounter = 0;
	int nextSummonOrder = 0; // Global counter for minion summon ordering

	// Menu UI Rectangles
	ofRectangle mainMenuHostButton;
	ofRectangle mainMenuInviteButton;

	// Helper to know if we are waiting in a lobby
	bool isInLobby = false;

	// --- BOARD & ENTITIES ---
	const float TILE_SIZE = 5.0f;
	Tile board[BOARD_WIDTH][BOARD_HEIGHT];
	TargetInfo targetCache[BOARD_WIDTH][BOARD_HEIGHT];
	std::vector<Player> players;
	int currentPlayerIndex = -1;
	std::vector<DeathMarker> graveyard;
	std::vector<FloatingText> activeFloatingTexts;
	std::vector<Particle> particles;
	float screenShake = 0.0f;

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
	ofImage fireTexture;

	// --- POST PROCESSING ---
	ofFbo worldFbo;
	ofShader worldPostShader;
	bool worldPostShaderLoaded = false;
	bool enableWorldPostProcess = true;
	bool showWorldFboPreview = false;

	// --- ANIMATIONS ---
	bool isPlayerAnimating = false;
	glm::vec3 playerVisualPos;
	float playerFacingAngle = 0.0f; // 0 = North, 90 = East, 180 = South, 270 = West
	std::vector<glm::vec3> animationPath;
	int currentPathIndex = 0;
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
	int hoveredCardIndex = -1;
	int lastHoveredCardIndex = -1;
	ofVec2f dragOffset;
	ofVec2f mouseDownPos;

	ofRectangle endTurnButtonRect;
	ofRectangle rerollButtonRect;
	ofVec2f endTurnButtonCurrentPos;
	ofVec2f endTurnButtonTargetPos;
	bool isHoveringEndTurn = false;

	ofTrueTypeFont uiFont;
	ofTrueTypeFont titleFont;

	ofFbo modelFbo;
	std::vector<MinionUI> activeMinionUIs;

	// --- Targeting States ---
	bool isTargetingDeath = false;
	int deathCardIndex = -1;

	bool isTargetingHeal = false;
	int healCardIndex = -1;

	// Tooltips & Piles
	bool isShowingTooltip = false;
	ofVec2f tooltipPos;
	std::string tooltipText;

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
	std::vector<Card> cardsToShowInView;
	ofRectangle pileViewRect;
	ofRectangle p0_deckRect, p0_discardRect;
	ofRectangle p1_deckRect, p1_discardRect;
	ofRectangle p0_apStatusRect;
	ofRectangle p1_apStatusRect;

	// --- CARD SPECIFIC VARIABLES ---

	// Attack
	bool isWaitingForAttackDice = false;
	int pendingAttackRollResult = 0;
	DamageType pendingAttackDamageType;
	std::vector<int> pendingAttackTargetIndices;
	std::string pendingAttackCardName = ""; // For flurry damage doubling

	// Poison (from Add Poison card)
	bool isWaitingForPoisonAttackDice = false;
	int pendingPoisonAttackRollResult = 0;
	std::vector<int> pendingPoisonTargetIndices;

	// Amnesia
	bool isAmnesiaSelectionActive = false;
	bool isWaitingForAmnesiaDice = false;
	int pendingAmnesiaRollResult = 0;
	int amnesiaTargetPlayerIndex = -1;
	int numCardsToRemove = 0;
	std::vector<Card> amnesiaDeckCopy;
	std::vector<int> amnesiaSelectedIndices;
	std::vector<ofRectangle> amnesiaCardRects;

	// Blocking Boon
	std::vector<int> pendingDraftQueue; // Stores class IDs (1, 2, or 3) for chained drafts
	int blockingBoonTargetIndex = -1; // Stores target for the "Tails" effect

	// Blocking Boon staged resolution
	bool isWaitingForBlockingBoonCoins = false; // true while coin flips are resolving
	int pendingBlockingBoonCoinsRemaining = 0; // number of coin flips outstanding
	int pendingBlockingBoonNonPhys = 0; // number of D20s to roll after coins
	int pendingBlockingBoonTotal = 0; // total outstanding blocking-boon dice (coins + D20s)

	// Prevent duplicate plays while a Blocking Boon is resolving
	bool blockingBoonActive = false;

	// Magic Blast
	bool isWaitingForMagicBlastDice = false;
	int pendingMagicBlastRollResult = 0;
	glm::vec2 pendingMagicBlastTargetTile;
	bool isMagicBlastChoiceActive = false;
	int magicBlastTargetPlayerIndex = -1;
	int magicBlastChoicesRemaining = 0;
	std::vector<int> magicBlastSplashTargetIndices;
	ofRectangle magicBlastDamageButton;
	ofRectangle magicBlastDiscardButton;

	// --- Psionic Wave ---
	bool isWaitingForPsionicRange = false;
	int pendingPsionicRangeResult = 0;
	bool isWaitingForPsionicAmount = false;
	int pendingPsionicAmountResult = 0;
	std::vector<int> psionicWaveTargetIndices; // Store who got hit by the range check

	// Fireball
	bool isWaitingForFireballRangeDice = false;
	int pendingFireballRangeResult = 0;
	glm::vec2 pendingFireballTargetTile;
	bool isWaitingForFireballDamageDice = false;
	int pendingFireballDamageResult = 0;
	glm::vec2 fireballImpactTile;
	int fireballTargetPlayerIndex = -1;

	// Ethereal Jolt
	bool isWaitingForJoltRangeDice = false;
	int pendingJoltRangeResult = 0;
	glm::vec2 pendingJoltTargetTile;

	// Chain Lightning
	bool isTargetingChainLightning = false;
	int chainLightningCardIndex = -1;
	bool isWaitingForChainLightningRange = false;
	bool isWaitingForChainLightningDamage = false;
	int pendingChainLightningRangeResult = 0;
	int pendingChainLightningDamageResult = 0;
	glm::vec2 pendingChainLightningTargetTile;

	// Train Menu UI
	bool isTrainMenuOpen = false;
	int pendingTrainCardIndex = -1;
	ofRectangle trainMenuRect;
	ofRectangle trainBtnAP;
	ofRectangle trainBtnDraft;

	// Function Declaration
	void drawTrainMenuUI();

	// Dispel
	bool isDispelMenuOpen = false;
	bool isDispelTargeting = false;
	bool isDispelStatusSelectOpen = false;
	int pendingDispelCardIndex = -1;
	int pendingDispelTargetIndex = -1;
	int pendingDispelRollResult = 0;
	bool isWaitingForBarrierDice = false;
	ofRectangle dispelMenuRect;
	ofRectangle dispelBtnBarrier;
	ofRectangle dispelBtnPurge;
	ofRectangle statusSelectMenuRect;
	std::vector<ofRectangle> statusSelectButtons;
	std::vector<std::string> statusSelectLabels;

	// Teleport
	bool isWaitingForTeleportDice = false;
	bool isTargetingTeleport = false; // After dice roll, waiting for target click
	int pendingTeleportCardIndex = -1;
	int pendingTeleportRollResult = 0;
	glm::vec2 pendingTeleportTarget;

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

	// Wisdom Boon
	bool isWisdomBoonMenuOpen = false;
	int pendingWisdomBoonCardIndex = -1;
	int pendingWisdomBoonTargetIndex = -1;
	ofRectangle wisdomMenuRect;
	ofRectangle wisdomBtnDamage;
	ofRectangle wisdomBtnBlock;

	// Burst of Light (choice UI + targeting)
	bool isBurstMenuOpen = false;
	int pendingBurstCardIndex = -1;
	bool isTargetingBurst = false; // true while choosing target after menu
	int burstChoice = 0; // 0 = Damage, 1 = Heal
	ofRectangle burstMenuRect;
	ofRectangle burstBtnDamage;
	ofRectangle burstBtnHeal;

	// Heal
	bool isWaitingForHealDice = false;
	int pendingHealRollResult = 0;
	int pendingHealTargetIndex = -1;

	// Summon (Raise Dead)
	bool isWaitingForSummonHealth = false;
	int pendingSummonRollResult = 0;
	glm::vec2 pendingSummonTile;

	// --- Double Handed State ---
	bool isDoubleHandedMenuOpen = false;
	bool isTargetingDoubleHanded = false;
	int pendingDoubleHandedCardIndex = -1;
	int pendingDoubleHandedTargetIndex = -1;
	std::string pendingDoubleHandedChoice = "";
	ofRectangle doubleHandedMenuRect;
	ofRectangle btnAddPunches;
	ofRectangle btnAddBlocks;

	// --- Amnesia State ---
	bool isAmnesiaMenuOpen = false; // Choosing Self vs Adjacent
	bool isTargetingAmnesia = false;
	int pendingAmnesiaCardIndex = -1;
	ofRectangle amnesiaMenuRect;
	ofRectangle amnesiaBtnSelf;
	ofRectangle amnesiaBtnAdjacent;

	// --- Renewed Inspiration State (REAL-TIME) ---
	bool isSelectingRenewedInspiration = false;
	std::vector<int> renewedSelectedHandIndices; // Indices of cards currently selected in hand
	ofRectangle riConfirmBtn;
	ofRectangle riCancelBtn;

	// --- Tortoise Form Targeting State ---
	bool isTargetingTortoiseDamage = false;

	// --- Call For Wolves State ---
	bool isWaitingForWolfCoin = false;
	bool isPlacingWolves = false;
	int wolvesRemainingToPlace = 0;
	int wolfPlacementSourceX = -1; // Where the summoner is standing
	int wolfPlacementSourceY = -1;
	int wolfSummonCount = 0; // To track "Wolf 1", "Wolf 2"
	int wolfSummonStage = 0; // 0=None, 1=First Wolf, 2=Second Wolf

	// --- Call For Kobolds State ---
	bool isWaitingForKoboldDice = false;
	bool isPlacingKobolds = false;
	int koboldsRemainingToPlace = 0;
	int koboldPlacementSourceX = -1;
	int koboldPlacementSourceY = -1;
	int koboldSummonCount = 0;
	int koboldSummonStage = 0;

	// Time Vortex
	bool isWaitingForTimeVortexDice = false;
	int pendingTimeVortexResult = 0;

	// --- Magic Bolt State ---
	bool isWaitingForMagicBoltRange = false;
	int pendingMagicBoltRangeResult = 0;
	bool isTargetingMagicBolt = false;
	int magicBoltCardIndex = -1;
	glm::vec2 pendingMagicBoltTargetTile;

	// Shoot Arrow State
	bool isWaitingForShootArrow = false;
	int pendingShootArrowHitResult = 0;
	glm::vec2 pendingShootArrowTargetTile;
	int pendingShootArrowTargetIndex = -1;

	// --- Giant Magic Hand ---
	bool isMagicHandMenuOpen = false;
	glm::ivec2 magicHandTargetTile;
	int pendingMagicHandCardIndex = -1;
	bool isWaitingForMagicHandDamage = false;
	int magicHandPushedUnitIndex = -1;
	glm::ivec2 magicHandPushDir;
	int pendingMagicHandRollResult = 0;

	// Flail
	bool isWaitingForFlailDice = false;
	int pendingFlailRollResult = 0;

	// Hellhound Summoning
	bool isWaitingForHellhoundHP = false;
	bool isTargetingHellhound = false;
	int hellhoundCardIndex = -1;

	// Demon Summoning
	bool isWaitingForDemonHP = false;

	// Death Card Logic
	bool isWaitingForDeathDice = false;
	bool isWaitingForSleepDuration = false;
	int pendingDeathTargetIndex = -1;
	int pendingDeathRollResult = 0;

	// --- Spark of genius Logic ---
	bool isWaitingForSparkOfGeniusDice = false;
	int pendingSparkOfGeniusRollResult = 0;

	// Helper functions
	void drawDoubleHandedUI();
	void cancelDoubleHanded();
	void resolveDoubleHanded(std::string cardName);
	void drawAmnesiaMenuUI();
	void drawCardSpawnerUI();
	void drawCardEncyclopediaUI();
	// UI Functions
	void drawMagicHandUI();
	void resolveMagicHandPull();
	void resolveMagicHandPush();
	void cancelMagicHand();

	// Status Effects
	bool isWaitingForOnFireDice = false;
	int pendingOnFireRollResult = 0;
	bool isWaitingForParalysisCoin = false;
	bool isWaitingForPoisonDice = false;
	int pendingPoisonRollResult = 0;

	// --- MENU UI VARIABLES ---
	ofRectangle mainMenuPlayAIButton;
	ofRectangle mainMenuMultiplayerButton;
	ofRectangle mainMenuSettingsButton;
	ofRectangle mainMenuQuitButton;
	int mainMenuHoveredIndex = -1;

	ofRectangle settingsBackButton;
	ofRectangle settingsResLeftButton, settingsResRightButton;
	ofRectangle settingsFrameLeftButton, settingsFrameRightButton;
	ofRectangle settingsFullscreenButton;
	int settingsHoveredIndex = -1;
	std::vector<glm::vec2> availableResolutions;
	int currentResolutionIndex = 0;
	std::vector<int> availableFramerates;
	int currentFramerateIndex = 0;
	bool isFullscreen = false;

	ofRectangle pauseMenuResumeButton;
	ofRectangle pauseMenuSettingsButton;
	ofRectangle pauseMenuQuitButton;
	int pauseMenuHoveredIndex = -1;

	// --- DICE & SOUND ---
	std::vector<DiceRoll> activeDiceRolls;
	float diceSpinSpeed = 1500.0f;
	ofMesh d6Mesh, d4Mesh, d20Mesh, d10Mesh, coinMesh;
	ofTexture d6Texture, d4Texture, d20Texture, d10Texture, coinFacesTexture;
	std::vector<ofSoundPlayer> footstepSounds;

	// --- DEBUG ---
	bool isDebugMode = false;
	bool isSpawningUnit = false;
	bool hasUnlimitedAP = false;
	ofRectangle debugPanel;
	ofRectangle debugDiceDropdownButton;
	ofRectangle debugRollD6Button;
	ofRectangle debugRollD4Button;
	ofRectangle debugRollD20Button;
	ofRectangle debugRollD10Button;
	ofRectangle debugFlipCoinButton;
	ofRectangle debugSpawnUnitButton;
	ofRectangle debugSpawnCardButton;
	ofRectangle debugDrawCardButton;
	ofRectangle debugUnlimitedAPButton;
	ofRectangle debugForceEndTurnButton;
	bool isDebugDiceDropdownOpen = false;

	// --- Debug Card Spawner UI (KRunner-style) ---
	bool isCardSpawnerOpen = false;
	bool isCardEncyclopediaOpen = false;
	std::string cardSpawnerInput = "";
	int cardSpawnerQuantity = 1;
	std::vector<Card> filteredCards; // Cards matching current input
	int encyclopediaScrollOffset = 0;
	ofRectangle cardSpawnerInputRect;
	ofRectangle cardSpawnerPlusButton;
	ofRectangle cardSpawnerMinusButton;
	ofRectangle cardSpawnerEncyclopediaButton;
	ofRectangle cardSpawnerCloseButton;
	ofRectangle encyclopediaCloseButton;
	ofRectangle encyclopediaRect;

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
		LOG };

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
};
