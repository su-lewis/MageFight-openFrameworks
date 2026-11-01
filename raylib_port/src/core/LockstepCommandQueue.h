#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

namespace mf::core {

struct InputCommand {
	std::uint32_t playerID = 0;
	std::uint32_t commandId = 0;
	std::uint32_t turnNumber = 0;
	std::uint8_t commandType = 0;
	std::array<std::int32_t, 8> params = {};
	std::array<char, 64> stringData = {};
	std::uint32_t clientActionID = 0;
};

class LockstepCommandQueue {
public:
	static constexpr std::uint32_t kOptimisticCommandFloor = 0x7FFFFFFF;

	void reset();

	bool enqueue(const InputCommand & cmd);
	void markOptimisticEchoAsSkipped(std::uint32_t playerID, std::uint32_t officialCommandId);

	void setLastProcessedCommandId(std::uint32_t commandId);
	std::uint32_t lastProcessedCommandId() const;

	std::size_t queueSize() const;

	void processReady(const std::function<void(const InputCommand &)> & executor);

private:
	static std::uint64_t makeCommandKey(std::uint32_t playerID, std::uint32_t commandId);
	static bool isOfficialCommand(const InputCommand & cmd);

	std::vector<InputCommand> commandQueue_;
	std::unordered_set<std::uint64_t> queuedCommandKeys_;
	std::unordered_set<std::uint64_t> executedCommandKeys_;
	std::unordered_set<std::uint64_t> skippedOptimisticKeys_;

	std::uint32_t lastProcessedCommandId_ = 0;
};

} // namespace mf::core
