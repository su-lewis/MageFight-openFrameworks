#pragma once

#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include <algorithm>
#include <glm/gtx/intersect.hpp>
#include <queue>
#include <random>
#include <set>
#include <vector>

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
	STATE_PAUSED
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
	MODAL_CHOICE
};

enum DamageType {
	DAMAGE_PHYSICAL,
	DAMAGE_PIERCING,
	DAMAGE_MAGIC,
	DAMAGE_ELECTRIC,
	DAMAGE_FIRE,
	DAMAGE_HOLY
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
	TARGET_EMPTY_ADJACENT
};

enum CardType {
	CARD_NONE,
	CARD_MOVE,
	CARD_CREATE_WALL,
	CARD_ATTACK_SINGLE_TILE,
	CARD_ATTACK_AREA,
	CARD_DESTROY_WALL,
	CARD_GAIN_AP,
	CARD_GAIN_BLOCK,
	CARD_GAIN_WARD,
	CARD_MAGIC_BLAST,
	CARD_FIREBALL,
	CARD_ARCANE_BURST,
	CARD_SHOCK,
	CARD_ROCK_CRUSH,
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
	CARD_MAGIC_BOLT
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
	PURPOSE_TIME_VORTEX
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
	int result = 0;
	bool isFinishedVisual = false;
	float startTime = 0;
	glm::quat finalQuat;
	float currentRotation = 0;
	glm::vec3 rotationAxis;
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
	int barrier = 0;
	int holyBlock = 0;
	int luck = 0;
	int bonusTurns = 0;
	int playerID = 0;
	bool onFire = false;

	// Status & Buffs
	int nextTurnAPBonus = 0;
	int shocksPlayedThisTurn = 0;
	bool isParalyzed = false;
	int paralysisHeadsCount = 0;
	bool nextTurnD10AP = false;
	bool nextTurnExtraDraw = false;
	bool isReplicatePending = false;
	bool nextTurnBonusDiceFromMinions = false;
	int strengthenElementsTurnsRemaining = 0;
	// Minion Data
	bool isMinion = false;
	bool isSkeleton = false;
	bool isGolem = false;
	bool isHellhound = false;
	bool isWolf = false;
	ofTexture * minionTexture = nullptr;
	int ownerID = -1;
	bool hasRegeneration = false;

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

private:
	// -------------------------------------------------------------------------
	//                              CORE SYSTEMS
	// -------------------------------------------------------------------------
	void setupGame();
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
	int startDiceRoll(int numDice, int sides, DicePurpose purpose, std::string label = "");
	void spawnFloatingText(glm::vec3 pos, std::string text, ofColor color);
	void spawnExplosion(glm::vec3 pos, int count, ofColor color);

	void calculateHighlights();
	void calculateTargetHighlights(int cardToCalculate = -1);
	void invalidateTargetCache();
	void clearHighlights();

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

	// -------------------------------------------------------------------------
	//                          RENDERING & MESHES
	// -------------------------------------------------------------------------
	void buildLevelMesh();
	void buildFloorMesh();

	void allocateWorldFbo(int w, int h);

	// Specific UI Drawers
	void drawMagicBlastChoiceUI();
	void drawWisdomBoonUI();
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
	bool isLoadingGame = false;
	int globalTurnCounter = 0;

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

	// --- LOGIC CACHE ---
	int currentAP = 0;
	bool hasDrawnCardsThisTurn = false;
	PlayerActionState playerAction = NONE;
	int selectedPieceGridX = -1;
	int selectedPieceGridY = -1;
	int lastCachedPlayerX = -1, lastCachedPlayerY = -1;
	int lastCachedCardIndex = -1;

	// --- CAMERA ---
	ofCamera cam;
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
	glm::vec3 cameraCurrentLookAt;
	float last3DZoom = 35.0f;
	float lastWindowWidth = 0;
	float lastWindowHeight = 0;

	// --- 3D ASSETS ---
	ofxAssimpModelLoader playerModel;
	ofxAssimpModelLoader skeletonModel;
	ofxAssimpModelLoader wolfModel;
	ofxAssimpModelLoader golemModel;

	ofTexture skeletonTexture;
	ofTexture wolfBodyTex, wolfFaceTex, wolfFurTex;
	ofTexture golemTexBase, golemTexRock, golemTexFire, golemTexElectric;

	ofMaterial modelMaterial;
	ofMaterial diceMaterial;

	// --- ENVIRONMENT MESHES & TEXTURES ---
	ofMesh levelMesh;
	ofMesh wallMesh;
	ofMesh roomMesh;
	ofTexture roomTexture;
	std::vector<ofMesh> floorMeshes;
	ofTexture wallTexture;
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
	std::vector<glm::vec3> animationPath;
	int currentPathIndex = 0;
	std::vector<glm::vec2> hoverPath;
	glm::vec2 lastHoverGridPos = { -1, -1 };

	std::vector<PlayedCardDisplay> activeCardDisplays;
	std::vector<StolenCardAnimation> activeStolenCardAnimations;
	std::vector<RemovedCardAnimation> activeRemovedCardAnimations;

	// --- CARDS & DECK ---
	ofImage cardSpriteSheet;
	ofImage cardBackImage;
	std::vector<Card> allCards;

	// --- UI INTERACTION ---
	int selectedCardIndex = -1;
	int draggedCardIndex = -1;
	int hoveredCardIndex = -1;
	ofVec2f dragOffset;
	ofVec2f mouseDownPos;

	ofRectangle endTurnButtonRect;
	ofVec2f endTurnButtonCurrentPos;
	ofVec2f endTurnButtonTargetPos;
	bool isHoveringEndTurn = false;

	ofTrueTypeFont uiFont;
	ofTrueTypeFont titleFont;

	ofFbo modelFbo;
	std::vector<MinionUI> activeMinionUIs;

	// Tooltips & Piles
	bool isShowingTooltip = false;
	ofVec2f tooltipPos;
	std::string tooltipText;

	bool isHoveringPile = false;
	PileViewMode hoveredPileType = VIEW_NONE;
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

	// Amnesia
	bool isAmnesiaSelectionActive = false;
	bool isWaitingForAmnesiaDice = false;
	int pendingAmnesiaRollResult = 0;
	int amnesiaTargetPlayerIndex = -1;
	int numCardsToRemove = 0;
	std::vector<Card> amnesiaDeckCopy;
	std::vector<int> amnesiaSelectedIndices;
	std::vector<ofRectangle> amnesiaCardRects;

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

	// Wisdom Boon
	bool isWisdomBoonMenuOpen = false;
	int pendingWisdomBoonCardIndex = -1;
	int pendingWisdomBoonTargetIndex = -1;
	ofRectangle wisdomMenuRect;
	ofRectangle wisdomBtnDamage;
	ofRectangle wisdomBtnBlock;

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
	bool isTargetingDoubleHanded = false; // Waiting for target click
	int pendingDoubleHandedCardIndex = -1;
	int pendingDoubleHandedTargetIndex = -1;
	std::string pendingDoubleHandedChoice = ""; // "Punch" or "Hand Block"
	ofRectangle doubleHandedMenuRect;
	ofRectangle btnAddPunches;
	ofRectangle btnAddBlocks;

	// --- Amnesia State ---
	bool isAmnesiaMenuOpen = false; // Choosing Self vs Adjacent
	bool isTargetingAmnesia = false; // Waiting for adjacent target click
	int pendingAmnesiaCardIndex = -1;
	ofRectangle amnesiaMenuRect;
	ofRectangle amnesiaBtnSelf;
	ofRectangle amnesiaBtnAdjacent;

	// --- Call For Wolves State ---
	bool isWaitingForWolfCoin = false;
	bool isPlacingWolves = false;
	int wolvesRemainingToPlace = 0;
	int wolfPlacementSourceX = -1; // Where the summoner is standing
	int wolfPlacementSourceY = -1;
	int wolfSummonCount = 0; // To track "Wolf 1", "Wolf 2"
	int wolfSummonStage = 0; // 0=None, 1=First Wolf, 2=Second Wolf

	// Time Vortex
	bool isWaitingForTimeVortexDice = false;
	int pendingTimeVortexResult = 0;

	// --- Magic Bolt State ---
	bool isWaitingForMagicBoltRange = false;
	int pendingMagicBoltRangeResult = 0;
	bool isTargetingMagicBolt = false;
	int magicBoltCardIndex = -1; // To remember which card in hand is being used
	glm::vec2 pendingMagicBoltTargetTile;

	// Helper functions
	void drawDoubleHandedUI();
	void cancelDoubleHanded();
	void resolveDoubleHanded(std::string cardName);
	void drawAmnesiaMenuUI();

	// Status Effects
	bool isWaitingForOnFireDice = false;
	int pendingOnFireRollResult = 0;
	bool isWaitingForParalysisCoin = false;

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
	std::mt19937 rng;
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
};
