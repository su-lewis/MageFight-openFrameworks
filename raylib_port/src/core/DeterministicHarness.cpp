#include "core/DeterministicHarness.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/DeterministicChecksum.h"
#include "core/LockstepCommandQueue.h"
#include "core/SnapshotFormat.h"

namespace mf::core {
namespace {

DeterministicSnapshot BuildSampleSnapshot() {
	DeterministicSnapshot snapshot;
	snapshot.globalTurnCounter = 7;
	snapshot.currentPlayerIndex = 1;
	snapshot.currentAP = 3;
	snapshot.gameplayRngAdvanceCount = 42;
	snapshot.nextSummonOrder = 5;

	snapshot.syncedState = 100;
	snapshot.isInGameDraft = false;
	snapshot.initialDraftComplete = true;
	snapshot.draftStage = 0;
	snapshot.draftPlayerIndex = 1;
	snapshot.draftPicksRemaining = 0;
	snapshot.currentDraftClassTier = 2;

	snapshot.draftQueue = { 2, 9, 12 };
	snapshot.floatingKeys.push_back({ 3, 4, 1 });
	snapshot.graveyard.push_back({ 5, 2, 3 });

	snapshot.board[2][3].hasWall = true;
	snapshot.board[4][2].hasPlayer = true;
	snapshot.board[8][6].isMagicWall = true;

	PlayerState p0;
	p0.playerID = 0;
	p0.ownerID = 0;
	p0.x = 1;
	p0.y = 1;
	p0.health = 24;
	p0.maxHealth = 30;
	p0.ap = 2;
	p0.deck = { { 1, 1, 0 }, { 2, 2, 0 } };
	p0.hand = { { 4, 1, 3 } };

	PlayerState p1;
	p1.playerID = 1;
	p1.ownerID = 1;
	p1.x = 10;
	p1.y = 7;
	p1.health = 19;
	p1.maxHealth = 30;
	p1.ap = 4;
	p1.onFire = true;
	p1.paralysisHeadsCount = 1;
	p1.hand = { { 8, 2, 2 }, { 12, 3, 1 } };
	p1.discardPile = { { 6, 1, 0 } };

	snapshot.players.push_back(p0);
	snapshot.players.push_back(p1);

	return snapshot;
}

InputCommand BuildTestCommand(std::uint32_t playerID, std::uint32_t commandId, std::uint8_t type = 1) {
	InputCommand cmd;
	cmd.playerID = playerID;
	cmd.commandId = commandId;
	cmd.commandType = type;
	cmd.turnNumber = 1;
	cmd.params[0] = static_cast<std::int32_t>(commandId);
	return cmd;
}

int RunLockstepQueueHarness() {
	LockstepCommandQueue queue;

	// Test deterministic ordering (by commandId then playerID)
	std::vector<std::uint32_t> executed;
	queue.enqueue(BuildTestCommand(1, 2));
	queue.enqueue(BuildTestCommand(0, 2));
	queue.enqueue(BuildTestCommand(0, 1));
	queue.processReady([&](const InputCommand & cmd) {
		executed.push_back(cmd.commandId * 10 + cmd.playerID);
	});

	if (executed.size() != 3 || executed[0] != 10 || executed[1] != 20 || executed[2] != 21) {
		std::cerr << "Lockstep queue harness failed: ordering mismatch\n";
		return 9;
	}

	// Test missing command pause behavior
	queue.reset();
	executed.clear();
	queue.enqueue(BuildTestCommand(0, 3));
	queue.processReady([&](const InputCommand & cmd) {
		executed.push_back(cmd.commandId);
	});
	if (!executed.empty()) {
		std::cerr << "Lockstep queue harness failed: queue did not block on missing command\n";
		return 10;
	}

	queue.enqueue(BuildTestCommand(0, 1));
	queue.enqueue(BuildTestCommand(0, 2));
	queue.processReady([&](const InputCommand & cmd) {
		executed.push_back(cmd.commandId);
	});
	if (executed.size() != 3 || executed[0] != 1 || executed[1] != 2 || executed[2] != 3) {
		std::cerr << "Lockstep queue harness failed: gap release ordering mismatch\n";
		return 11;
	}

	// Test optimistic skip behavior: official echo should advance sequence without execution
	queue.reset();
	executed.clear();
	queue.setLastProcessedCommandId(4);
	queue.markOptimisticEchoAsSkipped(1, 5);
	queue.enqueue(BuildTestCommand(1, 5));
	queue.processReady([&](const InputCommand & cmd) {
		executed.push_back(cmd.commandId);
	});

	if (!executed.empty() || queue.lastProcessedCommandId() != 5) {
		std::cerr << "Lockstep queue harness failed: optimistic skip mismatch\n";
		return 12;
	}

	std::cout << "Lockstep queue harness ok\n";
	return 0;
}

} // namespace

int RunDeterministicHarness() {
	const DeterministicSnapshot baseline = BuildSampleSnapshot();
	const DeterministicSnapshot same = BuildSampleSnapshot();

	std::uint64_t h0 = CalculateDeterministicChecksum(baseline);
	std::uint64_t h1 = CalculateDeterministicChecksum(same);

	if (h0 != h1) {
		std::cerr << "Deterministic harness failed: identical snapshots hashed differently\n";
		return 3;
	}

	DeterministicSnapshot changed = baseline;
	if (!changed.players.empty()) {
		changed.players[0].health -= 1;
	}

	std::uint64_t h2 = CalculateDeterministicChecksum(changed);
	if (h2 == h0) {
		std::cerr << "Deterministic harness failed: modified snapshot hash did not change\n";
		return 4;
	}

	std::cout << "Deterministic core harness ok\n";
	std::cout << "baseline=" << h0 << "\n";
	std::cout << "changed=" << h2 << "\n";

	int queueRc = RunLockstepQueueHarness();
	if (queueRc != 0) {
		return queueRc;
	}

	return 0;
}

int RunDeterministicSnapshotHarness(const std::string & snapshotPath) {
	namespace fs = std::filesystem;

	std::vector<std::string> cardsCandidates = {
		"bin/data/Config/cards.json",
		"data/Config/cards.json",
		"../bin/data/Config/cards.json",
	};
	std::vector<std::string> headerCandidates = {
		"src/ofApp.h",
		"../src/ofApp.h",
	};

	std::string cardsPath;
	std::string headerPath;
	for (const std::string & path : cardsCandidates) {
		if (fs::exists(path)) {
			cardsPath = path;
			break;
		}
	}

	for (const std::string & path : headerCandidates) {
		if (fs::exists(path)) {
			headerPath = path;
			break;
		}
	}

	if (cardsPath.empty()) {
		std::cerr << "Could not locate cards.json for snapshot parsing\n";
		return 5;
	}

	if (headerPath.empty()) {
		std::cerr << "Could not locate src/ofApp.h for CardType enum parsing\n";
		return 6;
	}

	SnapshotLoadPaths paths;
	paths.snapshotPath = snapshotPath;
	paths.cardsJsonPath = cardsPath;
	paths.ofAppHeaderPath = headerPath;

	DeterministicSnapshot snapshot;
	std::string error;
	if (!LoadDeterministicSnapshot(paths, snapshot, error)) {
		std::cerr << "Failed to load snapshot: " << error << "\n";
		return 7;
	}

	const std::uint64_t hash = CalculateDeterministicChecksum(snapshot);
	std::cout << "Loaded snapshot: " << snapshotPath << "\n";
	std::cout << "players=" << snapshot.players.size() << " turn=" << snapshot.globalTurnCounter << " checksum=" << hash << "\n";
	return 0;
}

} // namespace mf::core
