#pragma once
#include "NetworkData.h"
#include <vector>

// Pack/unpack per-die face values into InputCommandPacket::stringData.
// Binary layout: stringData[0] = uint8_t count (n), followed by n bytes of faces (uint8_t each).
// Maximum faces supported = 63 (fits in 64-byte buffer with count byte).
inline void packDiceFaces(InputCommandPacket & cmd, const std::vector<int> & faces) {
	size_t n = faces.size();
	if (n > 63) n = 63; // truncate
	memset(cmd.stringData, 0, sizeof(cmd.stringData));
	cmd.stringData[0] = (char)n;
	for (size_t i = 0; i < n; ++i) {
		cmd.stringData[1 + i] = (char)(faces[i] & 0xFF);
	}
}

inline std::vector<int> unpackDiceFaces(const InputCommandPacket & cmd) {
	std::vector<int> out;
	unsigned char n = (unsigned char)cmd.stringData[0];
	if (n == 0) return out;
	for (unsigned int i = 0; i < n && (1 + i) < sizeof(cmd.stringData); ++i) {
		out.push_back((unsigned char)cmd.stringData[1 + i]);
	}
	return out;
}
