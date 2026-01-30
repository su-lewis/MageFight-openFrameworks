#include "SteamManager.h"
#include "ofMain.h"
// --- ADD THIS BLOCK FOR WINDOWS BUILDS ---
#ifdef _WIN32
	#pragma comment(lib, "steam_api64.lib")
#endif
// Initialize the callbacks manually. This tells Steam "Call THIS function on THIS object"
SteamManager::SteamManager()
	: m_bInitialized(false)
	, m_bIsHost(false)
	, m_cbGameOverlayActivated(this, &SteamManager::OnGameOverlayActivated)
	, m_cbLobbyCreated(this, &SteamManager::OnLobbyCreated)
	, m_cbGameLobbyJoinRequested(this, &SteamManager::OnGameLobbyJoinRequested)
	, m_cbLobbyEnter(this, &SteamManager::OnLobbyEnter)
	, m_cbP2PSessionRequest(this, &SteamManager::OnP2PSessionRequest) {
}

SteamManager::~SteamManager() { cleanup(); }

void SteamManager::setup() {
	if (SteamAPI_Init()) {
		m_bInitialized = true;
		ofLogNotice("Steam") << "Steam API Initialized. My ID: " << SteamUser()->GetSteamID().ConvertToUint64();
	} else {
		ofLogError("Steam") << "Steam API failed to init. Is Steam running? Is steam_appid.txt present?";
	}
}

void SteamManager::update() {
	if (!m_bInitialized) return;

	// Run callbacks (This triggers OnLobbyCreated etc.)
	SteamAPI_RunCallbacks();

	// Read incoming packets
	uint32 packetSize;
	while (SteamNetworking()->IsP2PPacketAvailable(&packetSize)) {
		std::vector<char> buffer(packetSize);
		CSteamID sender;

		if (SteamNetworking()->ReadP2PPacket(buffer.data(), packetSize, &packetSize, &sender)) {
			if (m_OpponentId == CSteamID()) {
				m_OpponentId = sender;
				ofLogNotice("Steam") << "Opponent Connected: " << sender.ConvertToUint64();
			}
			packetQueue.push(buffer);
		}
	}
}

void SteamManager::cleanup() {
	if (m_bInitialized) SteamAPI_Shutdown();
}

// Logic: Connected if we have an opponent OR if we are hosting a valid lobby
bool SteamManager::isConnected() {
	return m_OpponentId.IsValid() || (m_bIsHost && m_LobbyId.IsValid());
}

bool SteamManager::isHost() { return m_bIsHost; }

uint64_t SteamManager::getMySteamID() {
	if (!m_bInitialized) return 0;
	return SteamUser()->GetSteamID().ConvertToUint64();
}

void SteamManager::createLobby() {
	if (!m_bInitialized) return;
	ofLogNotice("Steam") << "Requesting Lobby creation...";
	SteamMatchmaking()->CreateLobby(k_ELobbyTypeFriendsOnly, 2);
}

void SteamManager::openFriendOverlay() {
	if (!m_bInitialized) return;
	SteamFriends()->ActivateGameOverlay("LobbyInvite");
}

void SteamManager::sendPacket(void * data, int size) {
	if (!m_OpponentId.IsValid()) return;
	SteamNetworking()->SendP2PPacket(m_OpponentId, data, size, k_EP2PSendReliable);
}

// ---------------------- CALLBACK IMPLEMENTATIONS ----------------------

void SteamManager::OnGameOverlayActivated(GameOverlayActivated_t * pCallback) { }

void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback) {
	if (pCallback->m_eResult == k_EResultOK) {
		m_LobbyId = pCallback->m_ulSteamIDLobby;
		m_bIsHost = true;
		ofLogNotice("Steam") << "Lobby Created! ID: " << m_LobbyId.ConvertToUint64();
	} else {
		ofLogError("Steam") << "Lobby Creation Failed. Error: " << pCallback->m_eResult;
	}
}

void SteamManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback) {
	ofLogNotice("Steam") << "Join Request received. Joining " << pCallback->m_steamIDLobby.ConvertToUint64();
	SteamMatchmaking()->JoinLobby(pCallback->m_steamIDLobby);
}

void SteamManager::OnLobbyEnter(LobbyEnter_t * pCallback) {
	m_LobbyId = pCallback->m_ulSteamIDLobby;

	// Determine if we are owner
	m_bIsHost = (SteamMatchmaking()->GetLobbyOwner(m_LobbyId) == SteamUser()->GetSteamID());

	if (m_bIsHost) {
		ofLogNotice("Steam") << "Entered Lobby as Host.";
	} else {
		// We are the client, send a hello packet to Host so they know our ID
		m_OpponentId = SteamMatchmaking()->GetLobbyOwner(m_LobbyId);

		string hello = "Hello";
		sendPacket((void *)hello.c_str(), hello.size());

		ofLogNotice("Steam") << "Joined Lobby. Host is " << m_OpponentId.ConvertToUint64();
	}
}

void SteamManager::OnP2PSessionRequest(P2PSessionRequest_t * pCallback) {
	// Auto-accept connection from anyone
	ofLogNotice("Steam") << "P2P Connection request from " << pCallback->m_steamIDRemote.ConvertToUint64();
	SteamNetworking()->AcceptP2PSessionWithUser(pCallback->m_steamIDRemote);
}