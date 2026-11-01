#include "core/DeterministicChecksum.h"

namespace mf::core {
namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

inline void Mix(std::uint64_t value, std::uint64_t & hash) {
	hash ^= value;
	hash *= kFnvPrime;
}

inline std::uint64_t ToU64(int value) {
	return static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
}

inline void MixCard(const CardState & card, std::uint64_t & hash) {
	Mix(ToU64(card.type), hash);
	Mix(ToU64(card.cost), hash);
	Mix(ToU64(card.value), hash);
}

} // namespace

std::uint64_t CalculateDeterministicChecksum(const DeterministicSnapshot & snapshot) {
	std::uint64_t hash = kFnvOffset;

	Mix(ToU64(snapshot.globalTurnCounter), hash);
	Mix(ToU64(snapshot.currentPlayerIndex), hash);
	Mix(ToU64(snapshot.currentAP), hash);
	Mix(snapshot.gameplayRngAdvanceCount, hash);
	Mix(ToU64(snapshot.nextSummonOrder), hash);

	Mix(ToU64(snapshot.syncedState), hash);
	Mix(snapshot.isInGameDraft ? 1ULL : 0ULL, hash);
	Mix(snapshot.initialDraftComplete ? 1ULL : 0ULL, hash);
	Mix(ToU64(snapshot.draftStage), hash);
	Mix(ToU64(snapshot.draftPlayerIndex), hash);
	Mix(ToU64(snapshot.draftPicksRemaining), hash);
	Mix(ToU64(snapshot.currentDraftClassTier), hash);

	Mix(static_cast<std::uint64_t>(snapshot.draftQueue.size()), hash);
	for (int value : snapshot.draftQueue) {
		Mix(ToU64(value), hash);
	}

	Mix(static_cast<std::uint64_t>(snapshot.floatingKeys.size()), hash);
	for (const auto & key : snapshot.floatingKeys) {
		Mix(ToU64(key.x), hash);
		Mix(ToU64(key.y), hash);
		Mix(ToU64(key.set), hash);
	}

	Mix(static_cast<std::uint64_t>(snapshot.graveyard.size()), hash);
	for (const auto & grave : snapshot.graveyard) {
		Mix(ToU64(grave.x), hash);
		Mix(ToU64(grave.y), hash);
		Mix(static_cast<std::uint64_t>(grave.deckSize), hash);
	}

	for (int y = 0; y < DeterministicSnapshot::kBoardHeight; ++y) {
		for (int x = 0; x < DeterministicSnapshot::kBoardWidth; ++x) {
			const BoardTileState & tile = snapshot.board[x][y];
			Mix(tile.hasWall ? 1ULL : 0ULL, hash);
			Mix(tile.isMagicWall ? 1ULL : 0ULL, hash);
			Mix(tile.hasPlayer ? 1ULL : 0ULL, hash);
		}
	}

	for (const auto & player : snapshot.players) {
		Mix(ToU64(player.playerID), hash);
		Mix(ToU64(player.ownerID), hash);
		Mix(player.isMinion ? 1ULL : 0ULL, hash);
		Mix(ToU64(player.summonOrder), hash);

		Mix(ToU64(player.x), hash);
		Mix(ToU64(player.y), hash);
		Mix(ToU64(player.health), hash);
		Mix(ToU64(player.maxHealth), hash);
		Mix(ToU64(player.block), hash);
		Mix(ToU64(player.ward), hash);
		Mix(ToU64(player.fortification), hash);
		Mix(ToU64(player.barrier), hash);
		Mix(ToU64(player.holyBlock), hash);
		Mix(ToU64(player.luck), hash);
		Mix(ToU64(player.bonusTurns), hash);
		Mix(ToU64(player.ap), hash);

		Mix(player.onFire ? 1ULL : 0ULL, hash);
		Mix(player.isParalyzed ? 1ULL : 0ULL, hash);
		Mix(ToU64(player.paralysisHeadsCount), hash);
		Mix(player.isPoisoned ? 1ULL : 0ULL, hash);
		Mix(ToU64(player.poisonReduction), hash);
		Mix(ToU64(player.nextTurnAPBonus), hash);
		Mix(ToU64(player.nextAttackAddPoison), hash);
		Mix(ToU64(player.nextTurnD10AP), hash);
		Mix(ToU64(player.nextTurnExtraDraw), hash);
		Mix(player.replicateQueued ? 1ULL : 0ULL, hash);
		Mix(player.nextTurnBonusDiceFromMinions ? 1ULL : 0ULL, hash);
		Mix(ToU64(player.strengthenElementsTurnsRemaining), hash);
		Mix(ToU64(player.sleepTurnsRemaining), hash);
		Mix(ToU64(player.freeHandCardTurns), hash);
		Mix(ToU64(player.summonedOnTurnCycle), hash);
		Mix(player.inTortoiseForm ? 1ULL : 0ULL, hash);
		Mix(ToU64(player.tortoiseDamageTaken), hash);
		Mix(player.inGhostForm ? 1ULL : 0ULL, hash);
		Mix(ToU64(player.ghostDamageTaken), hash);
		Mix(ToU64(player.freeKickTurns), hash);
		Mix(ToU64(player.defenseCycle), hash);

		Mix(static_cast<std::uint64_t>(player.deck.size()), hash);
		for (const auto & card : player.deck) {
			MixCard(card, hash);
		}

		Mix(static_cast<std::uint64_t>(player.hand.size()), hash);
		for (const auto & card : player.hand) {
			MixCard(card, hash);
		}

		Mix(static_cast<std::uint64_t>(player.discardPile.size()), hash);
		for (const auto & card : player.discardPile) {
			MixCard(card, hash);
		}

		Mix(static_cast<std::uint64_t>(player.playedCardsPile.size()), hash);
		for (const auto & card : player.playedCardsPile) {
			MixCard(card, hash);
		}

		Mix(static_cast<std::uint64_t>(player.cardsPlayedThisTurn.size()), hash);
		for (int type : player.cardsPlayedThisTurn) {
			Mix(ToU64(type), hash);
		}
	}

	return hash;
}

} // namespace mf::core
