#include "SteamManager.h"

#ifdef _WIN32
	#pragma comment(lib, "steam_api64.lib")
#endif

SteamManager::SteamManager()
	: m_bInitialized(false)
	, m_bIsHost(false)
	, m_hListenSocket(k_HSteamListenSocket_Invalid)
	, m_hConnection(k_HSteamNetConnection_Invalid) {
}

SteamManager::~SteamManager() {
	cleanup();
}

void SteamManager::setup() {
	if (SteamAPI_Init()) {
		m_bInitialized = true;
		m_LocalID = SteamUser()->GetSteamID();

		// Initialize Sockets
		SteamNetworkingUtils()->InitRelayNetworkAccess();

		ofLogNotice("Steam") << "Initialized. LocalID: " << m_LocalID.ConvertToUint64();
	} else {
		ofLogError("Steam") << "Failed to init Steam API. Is Steam running?";
	}
}

void SteamManager::update() {
	if (!m_bInitialized) return;

	SteamAPI_RunCallbacks();

	// -- READ MESSAGES --
	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		ISteamNetworkingSockets * net = SteamNetworkingSockets();
		const int MAX_MSGS = 32;
		SteamNetworkingMessage_t * msgs[MAX_MSGS];

		int numMsgs = net->ReceiveMessagesOnConnection(m_hConnection, msgs, MAX_MSGS);

		for (int i = 0; i < numMsgs; i++) {
			auto * msg = msgs[i];

			// Copy data to vector
			std::vector<char> buffer((char *)msg->m_pData, (char *)msg->m_pData + msg->m_cbSize);
			packetQueue.push(buffer);

			msg->Release();
		}
	}
}

void SteamManager::cleanup() {
	// LEAVE THE LOBBY, BUT DO NOT SHUTDOWN THE API HERE
	leaveLobby();

	// CLOSE CONNECTIONS
	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		SteamNetworkingSockets()->CloseListenSocket(m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}

	// REMOVE THIS LINE if it exists here:
	// SteamAPI_Shutdown();  <-- DELETE THIS
}

// Add a new function specifically for App Exit if you want to be clean
void SteamManager::shutdownAPI() {
	if (m_bInitialized) {
		SteamAPI_Shutdown();
		m_bInitialized = false;
		ofLogNotice("SteamManager") << "Steam API Shutdown.";
	}
}

// ---------------------------------------------------------
//  CONNECTION LOGIC
// ---------------------------------------------------------

void SteamManager::createLobby() {
	if (m_bIsHost) return; // Already hosting?

	ofLogNotice("Steam") << "Requesting Lobby Creation...";
	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, 4);
	m_cbLobbyCreated.Set(hSteamAPICall, this, &SteamManager::OnLobbyCreated);
}

void SteamManager::leaveLobby() {
	closeConnection();

	if (m_LobbyID.IsValid()) {
		SteamMatchmaking()->LeaveLobby(m_LobbyID);
		m_LobbyID = CSteamID();
	}
	m_bIsHost = false;
}

void SteamManager::closeConnection() {
	ISteamNetworkingSockets * net = SteamNetworkingSockets();

	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		net->CloseConnection(m_hConnection, 0, "Closing", false);
		m_hConnection = k_HSteamNetConnection_Invalid;
	}

	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		net->CloseListenSocket(m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}

	while (!packetQueue.empty())
		packetQueue.pop();
}

bool SteamManager::sendPacket(const void * data, uint32_t size) {
	if (m_hConnection == k_HSteamNetConnection_Invalid) return false;

	// Use Reliable for game data
	EResult res = SteamNetworkingSockets()->SendMessageToConnection(
		m_hConnection, data, size, k_nSteamNetworkingSend_Reliable, nullptr);

	return (res == k_EResultOK);
}

bool SteamManager::isConnected() const {
	return m_hConnection != k_HSteamNetConnection_Invalid;
}

bool SteamManager::hasOpponent() const {
	return isConnected();
}

bool SteamManager::isHost() const {
	return m_bIsHost;
}

void SteamManager::openFriendOverlay() {
	if (m_bInitialized) SteamFriends()->ActivateGameOverlay("LobbyInvite");
}

// ---------------------------------------------------------
//  CALLBACKS
// ---------------------------------------------------------

// FIX: Standard function, NOT a STEAM_CALLBACK macro
void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure) {
	if (pCallback->m_eResult != k_EResultOK || bIOFailure) {
		ofLogError("Steam") << "Lobby Creation Failed. Result: " << pCallback->m_eResult;
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	m_bIsHost = true;
	ofLogNotice("Steam") << "Lobby Created. Creating Listen Socket...";

	// -- HOST LOGIC: OPEN LISTENING SOCKET --
	m_hListenSocket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, nullptr);
}

void SteamManager::OnLobbyEnter(LobbyEnter_t * pCallback) {
	if (pCallback->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) return;

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	CSteamID owner = SteamMatchmaking()->GetLobbyOwner(m_LobbyID);

	if (owner == m_LocalID) {
		m_bIsHost = true;
	} else {
		m_bIsHost = false;
		ofLogNotice("Steam") << "Joined Lobby. Connecting to Host: " << owner.ConvertToUint64();

		// -- CLIENT LOGIC: CONNECT TO HOST --
		SteamNetworkingIdentity identity;
		identity.SetSteamID(owner);
		m_hConnection = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, nullptr);
	}
}

// --- CONNECTION STATUS CHANGED ---
void SteamManager::OnNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t * pInfo) {

	switch (pInfo->m_info.m_eState) {

	case k_ESteamNetworkingConnectionState_None:
		break;

	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		ofLogNotice("Steam") << "Connection Closed/Failed.";

		if (pInfo->m_hConn == m_hConnection) {
			m_hConnection = k_HSteamNetConnection_Invalid;
		}
		SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		break;

	case k_ESteamNetworkingConnectionState_Connecting:
		ofLogNotice("Steam") << "Incoming Connection Request...";

		// SECURITY CHECK: Accept only if we are the host and listening
		if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
			if (SteamNetworkingSockets()->AcceptConnection(pInfo->m_hConn) == k_EResultOK) {
				m_hConnection = pInfo->m_hConn;
				ofLogNotice("Steam") << "Accepted Connection!";
			} else {
				SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			}
		}
		break;

	case k_ESteamNetworkingConnectionState_Connected:
		ofLogNotice("Steam") << "Connection Fully Active!";
		m_hConnection = pInfo->m_hConn;
		break;
	}
}

void SteamManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback) {
	SteamMatchmaking()->JoinLobby(pCallback->m_steamIDLobby);
}

void SteamManager::OnGameJoinRequested(GameRichPresenceJoinRequested_t * pCallback) {
	string cmd = pCallback->m_rgchConnect;
	size_t split = cmd.find(" ");
	if (split != string::npos) {
		CSteamID id(std::stoull(cmd.substr(split + 1)));
		SteamMatchmaking()->JoinLobby(id);
	}
}