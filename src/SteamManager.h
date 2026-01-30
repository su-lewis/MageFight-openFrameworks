#pragma once
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

	bool isConnected();
	bool isHost();
	uint64_t getMySteamID();

	void createLobby();
	void openFriendOverlay();

	// P2P Networking
	void sendPacket(void * data, int size);
	std::queue<std::vector<char>> packetQueue;

private:
	bool m_bInitialized;
	CSteamID m_LobbyId;
	CSteamID m_OpponentId; // The person we are playing against
	bool m_bIsHost;

	// Steam Callbacks
	STEAM_CALLBACK(SteamManager, OnGameOverlayActivated, GameOverlayActivated_t);
	STEAM_CALLBACK(SteamManager, OnLobbyCreated, LobbyCreated_t);
	STEAM_CALLBACK(SteamManager, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t);
	STEAM_CALLBACK(SteamManager, OnLobbyEnter, LobbyEnter_t);
	STEAM_CALLBACK(SteamManager, OnP2PSessionRequest, P2PSessionRequest_t);
};