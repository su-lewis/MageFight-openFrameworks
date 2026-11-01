#pragma once

#include <string>

#include "core/DeterministicTypes.h"

namespace mf::core {

struct SnapshotLoadPaths {
	std::string snapshotPath;
	std::string cardsJsonPath;
	std::string ofAppHeaderPath;
};

bool LoadDeterministicSnapshot(const SnapshotLoadPaths & paths, DeterministicSnapshot & outSnapshot, std::string & outError);

} // namespace mf::core
