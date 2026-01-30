#pragma once

#include "ofMain.h"
#include "steam/steam_api.h"
#include <queue>

class SteamManager {
public:
	SteamManager();
	~SteamManager();

	void setup();
	void update();
	void cleanup();

	// Game Logic Methods
	bool isConnected();
	bool isHost();
	void createLobby();
	void openFriendOverlay();
	void sendPacket(void * data, int size);
	uint64_t getMySteamID();

	// Packet Queue for ofApp to read
	std::queue<std::vector<char>> packetQueue;

private:
	bool m_bInitialized;
	bool m_bIsHost;
	CSteamID m_LobbyId;
	CSteamID m_OpponentId;

	// ---------------------- STEAM CALLBACKS ----------------------

	// 1. Overlay Activated (Shift+Tab)
	STEAM_CALLBACK(SteamManager, OnGameOverlayActivated, GameOverlayActivated_t);

	// 2. Lobby Created (Async CallResult)
	// NOTE: This uses CCallResult, so we declare the handler function manually
	void OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure);
	CCallResult<SteamManager, LobbyCreated_t> m_cbLobbyCreated;

	// 3. Join Request (Standard "Invite Friend" Button)
	STEAM_CALLBACK(SteamManager, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t);

	// 4. Join Request (Rich Presence / Spacewar Fix)
	// This handles when someone clicks "Join Game" on your profile
	STEAM_CALLBACK(SteamManager, OnGameJoinRequested, GameRichPresenceJoinRequested_t);

	// 5. Lobby Entered (Success)
	STEAM_CALLBACK(SteamManager, OnLobbyEnter, LobbyEnter_t);

	// 6. Networking (P2P Request)
	STEAM_CALLBACK(SteamManager, OnP2PSessionRequest, P2PSessionRequest_t);
};