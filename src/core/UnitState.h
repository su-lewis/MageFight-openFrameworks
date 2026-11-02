#ifndef UNITSTATE_H
#define UNITSTATE_H

#include <cstdint>
#include <string>
#include <vector>
#include "Cards.h"

struct UnitState {
    int32_t playerID{0};
    int32_t ownerID{0};
    bool isMinion{false};
    int32_t summonOrder{0};

    // Grid Coordinates (0..12, 0..8)
    int32_t x{0};
    int32_t y{0};

    // Health & Mitigation Shields
    int32_t health{15};
    int32_t maxHealth{15};
    int32_t block{0};
    int32_t ward{0};
    int32_t fortification{0};
    int32_t barrier{0};
    int32_t holyBlock{0};
    int32_t luck{0};
    int32_t bonusTurns{0};
    int32_t ap{0};

    // Status Flags
    bool onFire{false};
    bool isParalyzed{false};
    int32_t paralysisHeadsCount{0};
    bool isPoisoned{false};
    int32_t poisonReduction{0};
    int32_t nextTurnAPBonus{0};
    int32_t nextAttackAddPoison{0};
    int32_t nextTurnD10AP{0};
    int32_t nextTurnExtraDraw{0};
    bool replicateQueued{false};
    bool nextTurnBonusDiceFromMinions{false};
    int32_t strengthenElementsTurnsRemaining{0};
    int32_t sleepTurnsRemaining{0};
    int32_t freeHandCardTurns{0};
    int32_t summonedOnTurnCycle{0};
    bool inTortoiseForm{false};
    int32_t tortoiseDamageTaken{0};
    bool inGhostForm{false};
    int32_t ghostDamageTaken{0};
    int32_t freeKickTurns{0};
    int32_t defenseCycle{0};

    // Card Piles
    std::vector<Card> deck;
    std::vector<Card> hand;
    std::vector<Card> discardPile;
    std::vector<Card> playedCardsPile;
    std::vector<int32_t> cardsPlayedThisTurn;
};

#endif // UNITSTATE_H
