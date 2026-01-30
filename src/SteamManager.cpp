#include "SteamManager.h"

// --- WINDOWS LINKING FIX ---
#ifdef _WIN32
	#pragma comment(lib, "steam_api64.lib")
#endif
// ---------------------------

SteamManager::SteamManager()
	: m_bInitialized(false)
	, m_bIsHost(false) {
}

SteamManager::~SteamManager() {
	cleanup();
}

void SteamManager::setup() {
	if (SteamAPI_Init()) {
		m_bInitialized = true;
		ofLogNotice("Steam") << "API Initialized. User ID: " << SteamUser()->GetSteamID().ConvertToUint64();
	} else {
		ofLogError("Steam") << "Steam API failed to init. Is Steam running?";
	}
}

void SteamManager::update() {
	if (!m_bInitialized) return;

	// 1. Process Steam Events
	SteamAPI_RunCallbacks();

	// 2. Read Incoming Packets
	uint32 packetSize;
	while (SteamNetworking()->IsP2PPacketAvailable(&packetSize)) {

		std::vector<char> buffer(packetSize);
		uint32 bytesRead = 0;
		CSteamID senderID;

		if (SteamNetworking()->ReadP2PPacket(buffer.data(), packetSize, &bytesRead, &senderID)) {

			// If we don't have an opponent ID yet, save it now.
			if (!m_OpponentID.IsValid()) {
				m_OpponentID = senderID;
				ofLogNotice("Steam") << "Connection established with Opponent: " << m_OpponentID.ConvertToUint64();
			}

			packetQueue.push(buffer);
		}
	}
}

void SteamManager::cleanup() {
	if (m_bInitialized) {
		SteamAPI_Shutdown();
		m_bInitialized = false;
	}
}

// ----------------------------------------------------------------------------------
// GAME LOGIC HELPERS
// ----------------------------------------------------------------------------------

bool SteamManager::isConnected() const {
	// Only return true if we have actually connected to an opponent.
	// This prevents the Host from starting the game alone.
	return m_OpponentID.IsValid();
}

bool SteamManager::isHost() const {
	return m_bIsHost;
}

void SteamManager::createLobby() {
	if (!m_bInitialized) return;

	ofLogNotice("Steam") << "Requesting Lobby creation...";
	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby(k_ELobbyTypeFriendsOnly, 2);
	m_cbLobbyCreated.Set(hSteamAPICall, this, &SteamManager::OnLobbyCreated);
}

void SteamManager::leaveLobby() {
	if (m_LobbyID.IsValid()) {
		SteamMatchmaking()->LeaveLobby(m_LobbyID);
		ofLogNotice("Steam") << "Left Lobby: " << m_LobbyID.ConvertToUint64();
	}
	// Reset all networking state
	m_LobbyID = CSteamID();
	m_OpponentID = CSteamID(); // Clears isConnected() status
	m_bIsHost = false;

	// Clear any pending packets
	while (!packetQueue.empty())
		packetQueue.pop();
}

void SteamManager::openFriendOverlay() {
	if (!m_bInitialized) return;
	SteamFriends()->ActivateGameOverlay("LobbyInvite");
}

void SteamManager::sendPacket(const void * data, uint32_t size) {
	if (!m_bInitialized) return;

	if (m_OpponentID.IsValid()) {
		SteamNetworking()->SendP2PPacket(m_OpponentID, data, size, k_EP2PSendReliable);
	} else {
		ofLogWarning("Steam") << "Attempted to send packet, but no opponent connected yet.";
	}
}

// ----------------------------------------------------------------------------------
// CALLBACK IMPLEMENTATIONS
// ----------------------------------------------------------------------------------

void SteamManager::OnGameOverlayActivated(GameOverlayActivated_t * pCallback) {
	// Optional logic when overlay opens/closes
}

void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure) {
	if (pCallback->m_eResult != k_EResultOK || bIOFailure) {
		ofLogError("Steam") << "Lobby creation failed!";
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	m_bIsHost = true;
	// Reset opponent so isConnected() returns false until they join
	m_OpponentID = CSteamID();

	ofLogNotice("Steam") << "Lobby Created! ID: " << m_LobbyID.ConvertToUint64();

	// RICH PRESENCE FIX (Required for Spacewar AppID 480)
	string connectString = "+connect_lobby " + std::to_string(m_LobbyID.ConvertToUint64());
	SteamFriends()->SetRichPresence("connect", connectString.c_str());
}

void SteamManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback) {
	ofLogNotice("Steam") << "Join Request via Invite. Joining " << pCallback->m_steamIDLobby.ConvertToUint64();
	SteamMatchmaking()->JoinLobby(pCallback->m_steamIDLobby);
}

void SteamManager::OnGameJoinRequested(GameRichPresenceJoinRequested_t * pCallback) {
	ofLogNotice("Steam") << "Join Request via Rich Presence Overlay.";

	string cmd = pCallback->m_rgchConnect;
	size_t split = cmd.find(" ");

	if (split != string::npos) {
		string idStr = cmd.substr(split + 1);
		try {
			unsigned long long idLong = std::stoull(idStr);
			CSteamID lobbyID(idLong);
			ofLogNotice("Steam") << "Joining Lobby ID: " << lobbyID.ConvertToUint64();
			SteamMatchmaking()->JoinLobby(lobbyID);
		} catch (...) {
			ofLogError("Steam") << "Failed to parse Lobby ID.";
		}
	}
}

void SteamManager::OnLobbyEnter(LobbyEnter_t * pCallback) {
	if (pCallback->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
		ofLogError("Steam") << "Failed to enter lobby.";
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	CSteamID owner = SteamMatchmaking()->GetLobbyOwner(m_LobbyID);

	if (owner == SteamUser()->GetSteamID()) {
		m_bIsHost = true;
		ofLogNotice("Steam") << "Entered Lobby as Host.";
	} else {
		m_bIsHost = false;
		m_OpponentID = owner; // Clients know the opponent immediately
		ofLogNotice("Steam") << "Joined Lobby. Host is " << m_OpponentID.ConvertToUint64();

		// Send Hello packet to Host so they know our ID (P2P Handshake)
		std::string hello = "HELLO_HOST";
		sendPacket(hello.c_str(), hello.size());
	}
}

void SteamManager::OnP2PSessionRequest(P2PSessionRequest_t * pCallback) {
	CSteamID remoteID = pCallback->m_steamIDRemote;
	ofLogNotice("Steam") << "P2P Connection Requested by: " << remoteID.ConvertToUint64();
	SteamNetworking()->AcceptP2PSessionWithUser(remoteID);
}

void SteamManager::OnP2PSessionConnectFail(P2PSessionConnectFail_t * pCallback) {
	ofLogError("Steam") << "P2P Connection Failed.";
}