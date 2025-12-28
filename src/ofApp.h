#pragma once

#include "ofMain.h"
#include "ofxAssimpModelLoader.h" // Requires ofxAssimpModelLoader addon
#include <glm/gtx/intersect.hpp>
#include <vector>
#include <queue>
#include <set>
#include <random>
#include <algorithm>

// --- CONSTANTS ---
#define BOARD_WIDTH 13
#define BOARD_HEIGHT 9
// TILE_SIZE is defined as a const member in the class, 
// but used in setup, so we define a default here or rely on the class member.

// --- ENUMS ---

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
    DAMAGE_FIRE
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
    CARD_RAISE_DEAD
};

enum DicePurpose {
    PURPOSE_AP,
    PURPOSE_DAMAGE,
    PURPOSE_RANGE,
    PURPOSE_COIN_FLIP,
    PURPOSE_DEBUG,
    PURPOSE_BARRIER_GAIN,
    PURPOSE_HP,
    PURPOSE_HEALING 
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

// --- STRUCTS ---

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
    ofRectangle textureRect; // Coordinates on sprite sheet
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
    int playerID = 0;
    bool onFire = false;

    // --- New State Variables ---
    int nextTurnAPBonus = 0;
    int shocksPlayedThisTurn = 0;
    bool isParalyzed = false;
    int paralysisHeadsCount = 0;
    bool nextTurnD10AP = false;
    bool nextTurnExtraDraw = false;
    bool isReplicatePending = false;
     // --- New Minion Fields ---
    bool isMinion = false;
    bool isSkeleton = false; // For visuals
    int ownerID = -1;        // Matches the playerID of the summoner
    bool hasRegeneration = false; 

    std::vector<Card> playedCardsPile;
    std::vector<Card> hand;
    std::vector<Card> deck;
    std::vector<Card> discardPile;
};

struct DeathMarker {
    int x;
    int y;
    int turnDied;
    std::vector<Card> deck;
};

// --- THE MAIN APP CLASS ---

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

    // --- GRAVEYARD & GLOBAL STATE ---
    int globalTurnCounter = 0;
    std::vector<DeathMarker> graveyard;
    
    // --- SUMMON STATE ---
    bool isWaitingForSummonHealth = false;
    int pendingSummonRollResult = 0;
    glm::vec2 pendingSummonTile;
    
    // --- ASSETS ---
    ofxAssimpModelLoader skeletonModel;
    ofTexture skeletonTexture; 


private:
    // --- GAME LOGIC & STATE FUNCTIONS ---
    void setupGame();
    void updateGame();
    void drawGame();
    void cleanupGame();
    void drawMainMenu();
    void drawSettingsMenu();
    void drawPauseMenu();
    void applySettings();
    void recalculateUI(int w, int h); 

    void startNewTurn();
    void continueNewTurn();
    void drawCard();
    void playCard(int cardIndex, int targetX, int targetY);
    int startDiceRoll(int numDice, int sides, DicePurpose purpose);

    // --- DATA LOADING HELPERS ---
    void loadCardData(const std::string& filePath);
    CardType stringToCardType(const std::string& str);
    TargetingType stringToTargetingType(const std::string& str);
    DamageType stringToDamageType(const std::string& str);

    // --- HELPER FUNCTIONS ---
    ofVec2f mouseToBoard(int x, int y);
    void calculateHighlights();
    void calculateTargetHighlights(int cardToCalculate = -1);
    void invalidateTargetCache();
    void clearHighlights();
    std::vector<glm::vec2> findShortestPath(glm::vec2 start, glm::vec2 end);
    glm::vec3 gridToWorld(int gridX, int gridY);
    std::vector<Player *> findCleaveTargets(glm::vec2 direction);
    void drawMagicBlastChoiceUI();

    // --- Line of Sight System ---
    TargetInfo isLosTargetValid(glm::vec2 casterTile, glm::vec2 targetTile, float maxRangeFeet, CardType cardType);
    bool checkRayPhysics(glm::vec2 rayStart, glm::vec2 rayEnd); // The real physics engine
    int isGapTile(glm::vec2 tile);
    bool isTileBlocked(int x, int y);
    bool isTileWall(int x, int y);
    bool isOrthogonalPathBlocked(glm::vec2 start, glm::vec2 end);
    float getFaceToFaceDistance(glm::vec2 casterTile, glm::vec2 targetTile);
    std::vector<glm::vec2> getLineOfSightPath(glm::vec2 start, glm::vec2 end);
    glm::vec2 getClosestPointOnLineSegment(glm::vec2 p, glm::vec2 start, glm::vec2 end);

    // --- Coordinate Conversion Helpers ---
    glm::vec2 worldToGrid(glm::vec3 worldPos);

    // --- Safe Pointer Access ---
    Player * getPlayer(int index);
    void updateDebugRects(); // Centralized UI layout

    // --- MEMBER VARIABLES ---

    // --- Game State Management ---
    GameState currentState = STATE_MAIN_MENU;
    GameState stateBeforeSettings = STATE_MAIN_MENU;

    // --- Active Animations ---
    std::vector<PlayedCardDisplay> activeCardDisplays;
    std::vector<StolenCardAnimation> activeStolenCardAnimations;
    std::vector<RemovedCardAnimation> activeRemovedCardAnimations;

    // --- Amnesia State ---
    bool isAmnesiaSelectionActive = false;
    bool isWaitingForAmnesiaDice = false;
    int pendingAmnesiaRollResult = 0;
    int amnesiaTargetPlayerIndex = -1; 
    int numCardsToRemove = 0;
    std::vector<Card> amnesiaDeckCopy;
    std::vector<int> amnesiaSelectedIndices;

    // --- Magic Blast State ---
    bool isWaitingForMagicBlastDice = false;
    int pendingMagicBlastRollResult = 0;
    glm::vec2 pendingMagicBlastTargetTile;
    bool isMagicBlastChoiceActive = false;
    int magicBlastTargetPlayerIndex = -1;
    int magicBlastChoicesRemaining = 0;
    std::vector<int> magicBlastSplashTargetIndices;
    ofRectangle magicBlastDamageButton;
    ofRectangle magicBlastDiscardButton;

    // --- Fireball State ---
    bool isWaitingForFireballRangeDice = false;
    int pendingFireballRangeResult = 0;
    glm::vec2 pendingFireballTargetTile;
    bool isWaitingForFireballDamageDice = false;
    int pendingFireballDamageResult = 0;
    glm::vec2 fireballImpactTile;
    int fireballTargetPlayerIndex = -1;

    // --- Ethereal Jolt State ---
    bool isWaitingForJoltRangeDice = false;
    int pendingJoltRangeResult = 0;
    glm::vec2 pendingJoltTargetTile;

    // --- Dispel & Barrier State ---
    // State Flags
    bool isDispelMenuOpen = false;       // Phase 1: Choose Barrier or Purge
    bool isDispelTargeting = false;      // Phase 2: Click a unit
    bool isDispelStatusSelectOpen = false;// Phase 3: Choose which status (if multiple)

    // --- Teleport State ---
    bool isWaitingForTeleportDice = false;
    int pendingTeleportRollResult = 0;
    glm::vec2 pendingTeleportTarget;

     // --- WISDOM BOON STATE ---
    bool isWisdomBoonMenuOpen = false;
    int pendingWisdomBoonCardIndex = -1;
    int pendingWisdomBoonTargetIndex = -1;

     // --- Heal State ---
    bool isWaitingForHealDice = false;
    int pendingHealRollResult = 0;
    int pendingHealTargetIndex = -1;
    
    // UI Rects
    ofRectangle wisdomMenuRect;
    ofRectangle wisdomBtnDamage;
    ofRectangle wisdomBtnBlock;

    // Helpers
    void drawWisdomBoonUI();
    void cancelWisdomBoon();

    // Data storage for the active action
    int pendingDispelCardIndex = -1;     // Which card in hand is being played
    int pendingDispelTargetIndex = -1;   // Which player is being targeted for cure
    int pendingDispelRollResult = 0;     // Dice result for Barrier
    bool isWaitingForBarrierDice = false;

    // UI Rectangles
    ofRectangle dispelMenuRect;
    ofRectangle dispelBtnBarrier;
    ofRectangle dispelBtnPurge;
    
    ofRectangle statusSelectMenuRect;
    std::vector<ofRectangle> statusSelectButtons;
    std::vector<std::string> statusSelectLabels;

    // Helper functions
    void drawDispelUI();
    void cancelDispel();
    void determineStatusOptions(Player* target);
    void applyDispelEffect(int statusIndex);

    // --- Status Effects ---
    bool isWaitingForOnFireDice = false;
    int pendingOnFireRollResult = 0; 
    bool isWaitingForParalysisCoin = false;

    // --- Pause Menu UI ---
    ofRectangle pauseMenuResumeButton;
    ofRectangle pauseMenuSettingsButton;
    ofRectangle pauseMenuQuitButton;
    int pauseMenuHoveredIndex = -1;

    // Board & Players
    const float TILE_SIZE = 5.0f;
    Tile board[BOARD_WIDTH][BOARD_HEIGHT];
    TargetInfo targetCache[BOARD_WIDTH][BOARD_HEIGHT];
    std::vector<Player> players;
    int currentPlayerIndex = -1;

    // --- VBO Optimization ---
    ofMesh levelMesh; 
    ofMesh floorMeshA; // <--- NEW
    ofMesh floorMeshB; // <--- NEW
    void buildLevelMesh();
    void buildFloorMesh();

    // Gameplay State
    bool isLoadingGame = false;

    // Variables for delayed attack damage
    bool isWaitingForAttackDice = false;
    int pendingAttackRollResult = 0;
    DamageType pendingAttackDamageType;
    std::vector<int> pendingAttackTargetIndices; // Store indices of players to hit

    // Initial
    int currentAP = 0;
    bool hasDrawnCardsThisTurn = false;
    PlayerActionState playerAction = NONE;
    int selectedPieceGridX = -1;
    int selectedPieceGridY = -1;
    int lastCachedPlayerX = -1, lastCachedPlayerY = -1;
    int lastCachedCardIndex = -1;

    // Camera
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

    // Animation
    bool isPlayerAnimating = false;
    glm::vec3 playerVisualPos;
    std::vector<glm::vec3> animationPath;
    int currentPathIndex = 0;
    std::vector<glm::vec2> hoverPath;
    glm::vec2 lastHoverGridPos = { -1, -1 };

    // Cards
    ofImage cardSpriteSheet;
    ofImage cardBackImage;
    std::vector<Card> allCards;
    std::vector<ofRectangle> amnesiaCardRects;

    // UI State
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

    // --- Tooltip & Pile Viewing State ---
    bool isShowingTooltip = false;
    ofVec2f tooltipPos;
    std::string tooltipText;

    // State for detecting the start of a hover
    bool isHoveringPile = false;
    PileViewMode hoveredPileType = VIEW_NONE;
    int hoveredPilePlayerIndex = -1; 
    float pileHoverStartTime = 0.0f;

    // State for showing the final view panel
    bool isShowingPileView = false;
    PileViewMode currentPileView = VIEW_NONE;
    int currentPileViewPlayerIndex = -1;
    std::vector<Card> cardsToShowInView;
    ofRectangle pileViewRect;

    // Rects for the deck/discard piles
    ofRectangle p0_deckRect, p0_discardRect;
    ofRectangle p1_deckRect, p1_discardRect;

    // Main Menu UI
    ofRectangle mainMenuPlayAIButton;
    ofRectangle mainMenuMultiplayerButton;
    ofRectangle mainMenuSettingsButton;
    ofRectangle mainMenuQuitButton;
    int mainMenuHoveredIndex = -1;

    // Settings UI
    ofRectangle settingsBackButton;
    ofRectangle settingsResLeftButton, settingsResRightButton;
    ofRectangle settingsFrameLeftButton, settingsFrameRightButton;
    ofRectangle settingsFullscreenButton;
    int settingsHoveredIndex = -1;

    // Settings Values
    std::vector<glm::vec2> availableResolutions;
    int currentResolutionIndex = 0;
    std::vector<int> availableFramerates;
    int currentFramerateIndex = 0;
    bool isFullscreen = false;

    // 3D Assets
    ofxAssimpModelLoader playerModel;
    ofMesh wallMesh;
    std::vector<ofLight> lights;
    ofCamera cam;
    ofLight headlight;
    ofMaterial modelMaterial;
    ofMaterial diceMaterial;
    
    // Textures
    ofTexture wallTexture;
    
    // REPLACE floorTextureA/B with a vector
    std::vector<ofTexture> floorTextures; 
    
    // REPLACE floorMeshA/B with a vector
    std::vector<ofMesh> floorMeshes;
    
    // Dice
    std::vector<DiceRoll> activeDiceRolls;
    float diceSpinSpeed = 1500.0f;
    ofMesh d6Mesh;
    ofMesh d4Mesh;
    ofMesh d20Mesh;
    ofMesh d10Mesh;             
    ofMesh coinMesh;             
    ofTexture d6Texture;
    ofTexture d4Texture;
    ofTexture d20Texture;
    ofTexture d10Texture;       
    ofTexture coinFacesTexture;  
    std::mt19937 rng;
    glm::quat matchFaceToCamera(glm::vec3 faceNormal);

    //Sound effects
    std::vector<ofSoundPlayer> footstepSounds;

    // --- DEBUG VARIABLES ---
    bool isDebugMode = false;
    bool isSpawningUnit = false;
    bool hasUnlimitedAP = false;

    // UI for debug panel
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