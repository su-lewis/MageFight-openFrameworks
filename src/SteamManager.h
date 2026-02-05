#pragma once

#include "ofMain.h"

#include "steam_api.h"

#include <queue>
#include <unordered_map>
#include <vector>

// 2. INCLUDE YOUR EXISTING DATA HEADER
// This fixes the "redefinition of struct PacketHeader" error.
#include "NetworkData.h"

class SteamManager {
public:
	SteamManager();
	~SteamManager();

	void setup();
	void update();
	void cleanup();
	void shutdownAPI();

	// Lobby helpers (match start signaling)
	void setMatchStarted();
	bool isMatchStarted() const;
	void setLobbySeed(uint32_t seed);
	uint32_t getLobbySeed() const;

	// -- Connection Logic --
	void createLobby();
	void openFriendOverlay();
	void leaveLobby();

	// -- Data Logic --
	bool sendPacket(const void * data, uint32_t size);
	void handleAck(uint32_t seq);

	// -- State Getters --
	bool isHost() const;
	bool isConnected() const;
	bool hasOpponent() const;

	// -- Player Info --
	std::string getLocalPlayerName() const;
	std::string getOpponentName() const;
	CSteamID getOpponentSteamID() const;
	CSteamID getLocalSteamID() const;
	bool getAvatarImage(const CSteamID & id, ofImage & outImage, int size = 64) const;

	// -- Connection Status --
	bool checkAndClearDisconnectFlag();
	bool checkAndClearReconnectFlag();
	bool opponentDisconnected = false;
	bool opponentReconnected = false;

	// Queue for ofApp
	std::queue<std::vector<char>> packetQueue;

private:
	CSteamID m_OpponentID; // Track opponent's Steam ID
	uint32_t m_nextSeq = 1;
	struct PendingPacket {
		std::vector<char> data;
		float lastSendTime = 0.0f;
		int retries = 0;
		uint8_t type = 0;
	};
	std::unordered_map<uint32_t, PendingPacket> m_pending;
	float m_resendInterval = 0.5f;
	int m_maxRetries = 10;

public:
private:
	bool m_bInitialized;
	bool m_bIsHost;

	// -- Steam Identifiers --
	CSteamID m_LobbyID;
	CSteamID m_LocalID;

	// -- New Networking API Handles --
	HSteamListenSocket m_hListenSocket;
	HSteamNetConnection m_hConnection;

	// -- Callbacks --
	STEAM_CALLBACK(SteamManager, OnLobbyEnter, LobbyEnter_t);
	STEAM_CALLBACK(SteamManager, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t);
	STEAM_CALLBACK(SteamManager, OnGameJoinRequested, GameRichPresenceJoinRequested_t);
	STEAM_CALLBACK(SteamManager, OnNetConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);

	// -- CallResults --
	void OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure);
	CCallResult<SteamManager, LobbyCreated_t> m_cbLobbyCreated;

	void closeConnection();
};