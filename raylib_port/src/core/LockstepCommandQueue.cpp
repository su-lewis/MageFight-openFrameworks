#include "core/LockstepCommandQueue.h"

#include <algorithm>

namespace mf::core {

std::uint64_t LockstepCommandQueue::makeCommandKey(std::uint32_t playerID, std::uint32_t commandId) {
	return (static_cast<std::uint64_t>(playerID) << 32U) | static_cast<std::uint64_t>(commandId);
}

bool LockstepCommandQueue::isOfficialCommand(const InputCommand & cmd) {
	return cmd.commandId < kOptimisticCommandFloor;
}

void LockstepCommandQueue::reset() {
	commandQueue_.clear();
	queuedCommandKeys_.clear();
	executedCommandKeys_.clear();
	skippedOptimisticKeys_.clear();
	lastProcessedCommandId_ = 0;
}

bool LockstepCommandQueue::enqueue(const InputCommand & cmd) {
	const std::uint64_t key = makeCommandKey(cmd.playerID, cmd.commandId);

	if (executedCommandKeys_.find(key) != executedCommandKeys_.end()) {
		return false;
	}

	if (queuedCommandKeys_.find(key) != queuedCommandKeys_.end()) {
		return false;
	}

	auto it = std::lower_bound(commandQueue_.begin(), commandQueue_.end(), cmd,
		[](const InputCommand & a, const InputCommand & b) {
			if (a.commandId != b.commandId) {
				return a.commandId < b.commandId;
			}
			return a.playerID < b.playerID;
		});

	commandQueue_.insert(it, cmd);
	queuedCommandKeys_.insert(key);
	return true;
}

void LockstepCommandQueue::markOptimisticEchoAsSkipped(std::uint32_t playerID, std::uint32_t officialCommandId) {
	skippedOptimisticKeys_.insert(makeCommandKey(playerID, officialCommandId));
}

void LockstepCommandQueue::setLastProcessedCommandId(std::uint32_t commandId) {
	lastProcessedCommandId_ = commandId;
}

std::uint32_t LockstepCommandQueue::lastProcessedCommandId() const {
	return lastProcessedCommandId_;
}

std::size_t LockstepCommandQueue::queueSize() const {
	return commandQueue_.size();
}

void LockstepCommandQueue::processReady(const std::function<void(const InputCommand &)> & executor) {
	while (!commandQueue_.empty()) {
		const InputCommand cmd = commandQueue_.front();

		if (isOfficialCommand(cmd) && cmd.commandId > lastProcessedCommandId_ + 1U) {
			break;
		}

		commandQueue_.erase(commandQueue_.begin());
		const std::uint64_t key = makeCommandKey(cmd.playerID, cmd.commandId);
		queuedCommandKeys_.erase(key);

		if (executedCommandKeys_.find(key) != executedCommandKeys_.end()) {
			continue;
		}

		if (skippedOptimisticKeys_.find(key) != skippedOptimisticKeys_.end()) {
			skippedOptimisticKeys_.erase(key);
			executedCommandKeys_.insert(key);
			if (isOfficialCommand(cmd) && cmd.commandId > lastProcessedCommandId_) {
				lastProcessedCommandId_ = cmd.commandId;
			}
			continue;
		}

		executor(cmd);
		executedCommandKeys_.insert(key);

		if (isOfficialCommand(cmd)) {
			lastProcessedCommandId_ = cmd.commandId;
		}
	}
}

} // namespace mf::core
