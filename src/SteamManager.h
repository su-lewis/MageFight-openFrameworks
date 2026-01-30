#pragma once

#include "ofMain.h"
#include "steam/steam_api.h"
#include <queue>
#include <string>
#include <vector>

class SteamManager {
public:
	SteamManager();
	~SteamManager();

	void setup();
	void update();
	void cleanup();

	// --- State Queries ---
	bool isConnected() const; // For UI: Are we in a lobby?
	bool hasOpponent() const; // For Game Logic: Do we have a player 2?
	bool isHost() const;

	// --- Actions ---
	void createLobby();
	void leaveLobby(); // Cleans up networking state
	void openFriendOverlay();

	// Sends raw data to the opponent.
	void sendPacket(const void * data, uint32_t size);

	// --- Data Access ---
	std::queue<std::vector<char>> packetQueue;

private:
	bool m_bInitialized;
	bool m_bIsHost;

	// Standardized naming: Uppercase 'ID'
	CSteamID m_LobbyID;
	CSteamID m_OpponentID;

	// --- STEAM CALLBACKS ---

	// 1. Overlay Activation
	STEAM_CALLBACK(SteamManager, OnGameOverlayActivated, GameOverlayActivated_t);

	// 2. Lobby Creation
	void OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure);
	CCallResult<SteamManager, LobbyCreated_t> m_cbLobbyCreated;

	// 3. Join via Invite
	STEAM_CALLBACK(SteamManager, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t);

	// 4. Join via Rich Presence (Right-click -> Join Game)
	STEAM_CALLBACK(SteamManager, OnGameJoinRequested, GameRichPresenceJoinRequested_t);

	// 5. Entered Lobby
	STEAM_CALLBACK(SteamManager, OnLobbyEnter, LobbyEnter_t);

	// 6. P2P Connection Request
	STEAM_CALLBACK(SteamManager, OnP2PSessionRequest, P2PSessionRequest_t);

	// 7. P2P Failure
	STEAM_CALLBACK(SteamManager, OnP2PSessionConnectFail, P2PSessionConnectFail_t);
};