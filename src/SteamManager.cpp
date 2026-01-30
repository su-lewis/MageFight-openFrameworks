#include "SteamManager.h"

// --- WINDOWS LINKING FIX ---
#ifdef _WIN32
	#pragma comment(lib, "steam_api64.lib")
#endif
// ---------------------------

SteamManager::SteamManager()
	: m_bInitialized(false)
	, m_bIsHost(false) {
	// CCallResult members (like m_cbLobbyCreated) don't need init in constructor list
	// STEAM_CALLBACK macros handle their own initialization automatically
}

SteamManager::~SteamManager() {
	cleanup();
}

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

	// Run callbacks (This triggers OnLobbyCreated, OnGameJoinRequested, etc.)
	SteamAPI_RunCallbacks();

	// Read incoming P2P packets
	uint32 packetSize;
	while (SteamNetworking()->IsP2PPacketAvailable(&packetSize)) {
		std::vector<char> buffer(packetSize);
		CSteamID sender;

		if (SteamNetworking()->ReadP2PPacket(buffer.data(), packetSize, &packetSize, &sender)) {
			// Auto-detect opponent if we don't have one yet
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

bool SteamManager::isHost() {
	return m_bIsHost;
}

uint64_t SteamManager::getMySteamID() {
	if (!m_bInitialized) return 0;
	return SteamUser()->GetSteamID().ConvertToUint64();
}

void SteamManager::createLobby() {
	if (!m_bInitialized) return;

	ofLogNotice("Steam") << "Requesting Lobby creation...";

	// CreateLobby is an async call. We get a handle, and set the callback.
	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby(k_ELobbyTypeFriendsOnly, 2);
	m_cbLobbyCreated.Set(hSteamAPICall, this, &SteamManager::OnLobbyCreated);
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

void SteamManager::OnGameOverlayActivated(GameOverlayActivated_t * pCallback) {
	// Optional: Pause game here if needed
}

// NOTE: This signature now matches the Header (includes bIOFailure)
void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure) {
	if (pCallback->m_eResult != k_EResultOK || bIOFailure) {
		ofLogNotice() << "Steam: Lobby creation failed!";
		return;
	}

	CSteamID lobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	ofLogNotice() << "Steam: Lobby Created! ID: " << lobbyID.ConvertToUint64();

	// --- RICH PRESENCE FIX FOR SPACEWAR (AppID 480) ---
	// This tells Steam: "If someone clicks Join on my profile, send them this Lobby ID"
	// Without this, 'Join Game' does nothing on AppID 480.
	string connectString = "+connect_lobby " + to_string(lobbyID.ConvertToUint64());
	SteamFriends()->SetRichPresence("connect", connectString.c_str());
	// --------------------------------------------------
}

void SteamManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback) {
	ofLogNotice("Steam") << "Join Request received (Invite). Joining " << pCallback->m_steamIDLobby.ConvertToUint64();
	SteamMatchmaking()->JoinLobby(pCallback->m_steamIDLobby);
}

// This handles the "Right Click -> Join Game" action
void SteamManager::OnGameJoinRequested(GameRichPresenceJoinRequested_t * pCallback) {
	ofLogNotice() << "Steam: Join Game Requested via Overlay (Rich Presence)!";

	// The "connect" string we set in OnLobbyCreated comes back to us here
	// It looks like "+connect_lobby 12345..."
	string cmd = pCallback->m_rgchConnect;

	// Parse the ID out of the string
	size_t split = cmd.find(" ");
	if (split != string::npos) {
		string idStr = cmd.substr(split + 1);

		// Use stoull to safely convert string to uint64
		CSteamID lobbyID(std::stoull(idStr));

		ofLogNotice() << "Steam: Joining Lobby " << lobbyID.ConvertToUint64();
		SteamMatchmaking()->JoinLobby(lobbyID);
	}
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