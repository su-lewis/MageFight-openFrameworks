#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstdint>
#include <vector>

inline uint32_t fnv1a_checksum(const std::vector<uint8_t>& data) {
    uint32_t hash = 2166136261u;
    for (auto byte : data) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

#endif // CHECKSUM_H
