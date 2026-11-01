#pragma once

#include <string>

namespace mf::core {

int RunDeterministicHarness();
int RunDeterministicSnapshotHarness(const std::string & snapshotPath);

} // namespace mf::core
