#pragma once

#include <cstdint>
#include <vector>

namespace mf::core {

struct CardState {
	int type = 0;
	int cost = 0;
	int value = 0;
};

struct BoardTileState {
	bool hasWall = false;
	bool isMagicWall = false;
	bool hasPlayer = false;
};

struct FloatingKeyState {
	int x = 0;
	int y = 0;
	int set = 0;
};

struct GraveyardState {
	int x = 0;
	int y = 0;
	std::size_t deckSize = 0;
};

struct PlayerState {
	int playerID = 0;
	int ownerID = 0;
	bool isMinion = false;
	int summonOrder = 0;

	int x = 0;
	int y = 0;
	int health = 0;
	int maxHealth = 0;
	int block = 0;
	int ward = 0;
	int fortification = 0;
	int barrier = 0;
	int holyBlock = 0;
	int luck = 0;
	int bonusTurns = 0;
	int ap = 0;

	bool onFire = false;
	bool isParalyzed = false;
	int paralysisHeadsCount = 0;
	bool isPoisoned = false;
	int poisonReduction = 0;
	int nextTurnAPBonus = 0;
	int nextAttackAddPoison = 0;
	int nextTurnD10AP = 0;
	int nextTurnExtraDraw = 0;
	bool replicateQueued = false;
	bool nextTurnBonusDiceFromMinions = false;
	int strengthenElementsTurnsRemaining = 0;
	int sleepTurnsRemaining = 0;
	int freeHandCardTurns = 0;
	int summonedOnTurnCycle = 0;
	bool inTortoiseForm = false;
	int tortoiseDamageTaken = 0;
	bool inGhostForm = false;
	int ghostDamageTaken = 0;
	int freeKickTurns = 0;
	int defenseCycle = 0;

	std::vector<CardState> deck;
	std::vector<CardState> hand;
	std::vector<CardState> discardPile;
	std::vector<CardState> playedCardsPile;
	std::vector<int> cardsPlayedThisTurn;
};

struct DeterministicSnapshot {
	static constexpr int kBoardWidth = 13;
	static constexpr int kBoardHeight = 9;

	int globalTurnCounter = 0;
	int currentPlayerIndex = 0;
	int currentAP = 0;
	std::uint64_t gameplayRngAdvanceCount = 0;
	int nextSummonOrder = 0;

	int syncedState = 0;
	bool isInGameDraft = false;
	bool initialDraftComplete = false;
	int draftStage = 0;
	int draftPlayerIndex = 0;
	int draftPicksRemaining = 0;
	int currentDraftClassTier = 0;

	std::vector<int> draftQueue;
	std::vector<FloatingKeyState> floatingKeys;
	std::vector<GraveyardState> graveyard;

	BoardTileState board[kBoardWidth][kBoardHeight] = {};
	std::vector<PlayerState> players;
};

} // namespace mf::core
