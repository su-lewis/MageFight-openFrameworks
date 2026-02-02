#pragma once

#include "ofMain.h"

// 1. USE THE DIRECT PATH TO FIX THE "FILE NOT FOUND" ERROR
// This bypasses the config.make issues completely.
#include "../libs/steam/include/steam_api.h"

#include <queue>
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

	// -- Connection Logic --
	void createLobby();
	void openFriendOverlay();
	void leaveLobby();

	// -- Data Logic --
	bool sendPacket(const void * data, uint32_t size);

	// -- State Getters --
	bool isHost() const;
	bool isConnected() const;
	bool hasOpponent() const;

	// Queue for ofApp
	std::queue<std::vector<char>> packetQueue;

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