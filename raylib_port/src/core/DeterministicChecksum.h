#pragma once

#include <cstdint>

#include "DeterministicTypes.h"

namespace mf::core {

std::uint64_t CalculateDeterministicChecksum(const DeterministicSnapshot & snapshot);

} // namespace mf::core
