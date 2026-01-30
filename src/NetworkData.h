#pragma once
#include <cstdint>

enum PacketType {
	PKT_HANDSHAKE,
	PKT_ACTION,
	PKT_END_TURN,
	PKT_CHECKSUM_CHECK
};

#pragma pack(push, 1)

struct PacketHeader {
	uint8_t type; // PacketType
	uint32_t playerID; // Who sent this?
};

struct HandshakePacket : PacketHeader {
	uint32_t seed; // The RNG seed (Host generates, Client receives)
};

struct ActionPacket : PacketHeader {
	int32_t cardIndex;
	int32_t targetX;
	int32_t targetY;
	int32_t cost;
};

struct ChecksumPacket : PacketHeader {
	int64_t checksum; // Compare game state
	int32_t turnNumber;
};

#pragma pack(pop)