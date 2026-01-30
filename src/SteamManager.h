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
    CSteamID m_OpponentId;
    bool m_bIsHost;

    // --- MANUAL CALLBACK DEFINITIONS (Fixes Constructor Error) ---
    
    // 1. Overlay Activated
    CCallback<SteamManager, GameOverlayActivated_t> m_cbGameOverlayActivated;
    void OnGameOverlayActivated(GameOverlayActivated_t * pCallback);

    // 2. Lobby Created
    CCallback<SteamManager, LobbyCreated_t> m_cbLobbyCreated;
    void OnLobbyCreated(LobbyCreated_t * pCallback);

    // 3. Join Request (Friend Invite)
    CCallback<SteamManager, GameLobbyJoinRequested_t> m_cbGameLobbyJoinRequested;
    void OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback);

    // 4. Lobby Entered
    CCallback<SteamManager, LobbyEnter_t> m_cbLobbyEnter;
    void OnLobbyEnter(LobbyEnter_t * pCallback);

    // 5. P2P Request
    CCallback<SteamManager, P2PSessionRequest_t> m_cbP2PSessionRequest;
    void OnP2PSessionRequest(P2PSessionRequest_t * pCallback);
};