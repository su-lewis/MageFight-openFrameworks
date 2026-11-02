#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstdint>
#include <vector>
#include "GameState.h"

namespace {

constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

inline void mix_hash(uint64_t value, uint64_t & hash) {
    hash ^= value;
    hash *= kFnvPrime;
}

inline uint64_t to_u64(int32_t value) {
    return static_cast<uint64_t>(static_cast<int64_t>(value));
}

inline void mix_card(const Card & card, uint64_t & hash) {
    mix_hash(to_u64(card.id), hash);
    mix_hash(to_u64(static_cast<int32_t>(card.type)), hash);
    mix_hash(to_u64(card.cost), hash);
}

} // namespace

inline uint64_t fnv1a_checksum(const std::vector<uint8_t>& data) {
    uint32_t hash = 2166136261u;
    for (auto byte : data) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

inline uint64_t calculate_checksum(const GameState & state) {
    uint64_t hash = kFnvOffset;

    mix_hash(to_u64(state.globalTurnCounter), hash);
    mix_hash(to_u64(state.currentPlayerIndex), hash);
    mix_hash(to_u64(state.currentAP), hash);
    mix_hash(state.gameplayRngAdvanceCount, hash);
    mix_hash(to_u64(state.nextSummonOrder), hash);

    mix_hash(to_u64(state.syncedState), hash);
    mix_hash(state.isInGameDraft ? 1ULL : 0ULL, hash);
    mix_hash(state.initialDraftComplete ? 1ULL : 0ULL, hash);
    mix_hash(to_u64(state.draftStage), hash);
    mix_hash(to_u64(state.draftPlayerIndex), hash);
    mix_hash(to_u64(state.draftPicksRemaining), hash);
    mix_hash(to_u64(state.currentDraftClassTier), hash);

    mix_hash(static_cast<uint64_t>(state.draftQueue.size()), hash);
    for (int32_t val : state.draftQueue) {
        mix_hash(to_u64(val), hash);
    }

    mix_hash(static_cast<uint64_t>(state.floatingKeys.size()), hash);
    for (const auto & key : state.floatingKeys) {
        mix_hash(to_u64(key.x), hash);
        mix_hash(to_u64(key.y), hash);
        mix_hash(to_u64(key.set), hash);
    }

    mix_hash(static_cast<uint64_t>(state.graveyard.size()), hash);
    for (const auto & grave : state.graveyard) {
        mix_hash(to_u64(grave.x), hash);
        mix_hash(to_u64(grave.y), hash);
        mix_hash(static_cast<uint64_t>(grave.deckSize), hash);
    }

    for (int y = 0; y < GameState::BOARD_HEIGHT; ++y) {
        for (int x = 0; x < GameState::BOARD_WIDTH; ++x) {
            const BoardTile & tile = state.board[x][y];
            mix_hash(tile.hasWall ? 1ULL : 0ULL, hash);
            mix_hash(tile.isMagicWall ? 1ULL : 0ULL, hash);
            mix_hash(tile.hasPlayer ? 1ULL : 0ULL, hash);
        }
    }

    for (const auto & unit : state.units) {
        mix_hash(to_u64(unit.playerID), hash);
        mix_hash(to_u64(unit.ownerID), hash);
        mix_hash(unit.isMinion ? 1ULL : 0ULL, hash);
        mix_hash(to_u64(unit.summonOrder), hash);

        mix_hash(to_u64(unit.x), hash);
        mix_hash(to_u64(unit.y), hash);
        mix_hash(to_u64(unit.health), hash);
        mix_hash(to_u64(unit.maxHealth), hash);
        mix_hash(to_u64(unit.block), hash);
        mix_hash(to_u64(unit.ward), hash);
        mix_hash(to_u64(unit.fortification), hash);
        mix_hash(to_u64(unit.barrier), hash);
        mix_hash(to_u64(unit.holyBlock), hash);
        mix_hash(to_u64(unit.luck), hash);
        mix_hash(to_u64(unit.bonusTurns), hash);
        mix_hash(to_u64(unit.ap), hash);

        mix_hash(unit.onFire ? 1ULL : 0ULL, hash);
        mix_hash(unit.isParalyzed ? 1ULL : 0ULL, hash);
        mix_hash(to_u64(unit.paralysisHeadsCount), hash);
        mix_hash(unit.isPoisoned ? 1ULL : 0ULL, hash);
        mix_hash(to_u64(unit.poisonReduction), hash);
        mix_hash(to_u64(unit.nextTurnAPBonus), hash);
        mix_hash(to_u64(unit.nextAttackAddPoison), hash);
        mix_hash(to_u64(unit.nextTurnD10AP), hash);
        mix_hash(to_u64(unit.nextTurnExtraDraw), hash);
        mix_hash(unit.replicateQueued ? 1ULL : 0ULL, hash);
        mix_hash(unit.nextTurnBonusDiceFromMinions ? 1ULL : 0ULL, hash);
        mix_hash(to_u64(unit.strengthenElementsTurnsRemaining), hash);
        mix_hash(to_u64(unit.sleepTurnsRemaining), hash);
        mix_hash(to_u64(unit.freeHandCardTurns), hash);
        mix_hash(to_u64(unit.summonedOnTurnCycle), hash);
        mix_hash(unit.inTortoiseForm ? 1ULL : 0ULL, hash);
        mix_hash(to_u64(unit.tortoiseDamageTaken), hash);
        mix_hash(unit.inGhostForm ? 1ULL : 0ULL, hash);
        mix_hash(to_u64(unit.ghostDamageTaken), hash);
        mix_hash(to_u64(unit.freeKickTurns), hash);
        mix_hash(to_u64(unit.defenseCycle), hash);

        mix_hash(static_cast<uint64_t>(unit.deck.size()), hash);
        for (const auto & card : unit.deck) {
            mix_card(card, hash);
        }

        mix_hash(static_cast<uint64_t>(unit.hand.size()), hash);
        for (const auto & card : unit.hand) {
            mix_card(card, hash);
        }

        mix_hash(static_cast<uint64_t>(unit.discardPile.size()), hash);
        for (const auto & card : unit.discardPile) {
            mix_card(card, hash);
        }

        mix_hash(static_cast<uint64_t>(unit.playedCardsPile.size()), hash);
        for (const auto & card : unit.playedCardsPile) {
            mix_card(card, hash);
        }

        mix_hash(static_cast<uint64_t>(unit.cardsPlayedThisTurn.size()), hash);
        for (int32_t type : unit.cardsPlayedThisTurn) {
            mix_hash(to_u64(type), hash);
        }
    }

    return hash;
}

#endif // CHECKSUM_H

