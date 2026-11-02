#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <cstdint>
#include <vector>
#include "UnitState.h"

struct BoardTile {
    bool hasWall{false};
    bool isMagicWall{false};
    bool hasPlayer{false};
};

struct FloatingKey {
    int32_t x{0};
    int32_t y{0};
    int32_t set{0};
};

struct GraveyardTile {
    int32_t x{0};
    int32_t y{0};
    std::size_t deckSize{0};
};

struct GameState {
    static constexpr int32_t BOARD_WIDTH = 13;
    static constexpr int32_t BOARD_HEIGHT = 9;

    int32_t globalTurnCounter{0};
    int32_t currentPlayerIndex{0};
    int32_t currentAP{0};
    uint64_t gameplayRngAdvanceCount{0};
    int32_t nextSummonOrder{0};

    int32_t syncedState{0};
    bool isInGameDraft{false};
    bool initialDraftComplete{false};
    int32_t draftStage{0};
    int32_t draftPlayerIndex{0};
    int32_t draftPicksRemaining{0};
    int32_t currentDraftClassTier{0};

    std::vector<int32_t> draftQueue;
    std::vector<FloatingKey> floatingKeys;
    std::vector<GraveyardTile> graveyard;

    BoardTile board[BOARD_WIDTH][BOARD_HEIGHT];
    std::vector<UnitState> units;
};

#endif // GAMESTATE_H

