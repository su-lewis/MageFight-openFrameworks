#include "SteamManager.h"

// Define the safe Flat C API functions manually to avoid Valve's broken include paths
// This completely fixes the MinGW vtable crash without needing steam_api_flat.h
extern "C" {
uint64_t SteamAPI_ISteamUser_GetSteamID(intptr_t instancePtr);
uint64_t SteamAPI_ISteamMatchmaking_CreateLobby(intptr_t instancePtr, int eLobbyType, int cMaxMembers);
void SteamAPI_ISteamMatchmaking_LeaveLobby(intptr_t instancePtr, uint64_t steamIDLobby);
uint64_t SteamAPI_ISteamMatchmaking_GetLobbyOwner(intptr_t instancePtr, uint64_t steamIDLobby);
bool SteamAPI_ISteamMatchmaking_SetLobbyData(intptr_t instancePtr, uint64_t steamIDLobby, const char * pchKey, const char * pchValue);
const char * SteamAPI_ISteamMatchmaking_GetLobbyData(intptr_t instancePtr, uint64_t steamIDLobby, const char * pchKey);
uint64_t SteamAPI_ISteamMatchmaking_JoinLobby(intptr_t instancePtr, uint64_t steamIDLobby);
void SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter(intptr_t instancePtr, int cMaxResults);
void SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter(intptr_t instancePtr, const char * pchKeyToMatch, const char * pchValueToMatch, int eComparisonType);
void SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter(intptr_t instancePtr, int eLobbyDistanceFilter);
uint64_t SteamAPI_ISteamMatchmaking_RequestLobbyList(intptr_t instancePtr);
uint64_t SteamAPI_ISteamMatchmaking_GetLobbyByIndex(intptr_t instancePtr, int iLobby);
int SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(intptr_t instancePtr, uint64_t steamIDLobby);
int SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit(intptr_t instancePtr, uint64_t steamIDLobby);

const char * SteamAPI_ISteamFriends_GetPersonaName(intptr_t instancePtr);
#include <algorithm>
const char * SteamAPI_ISteamFriends_GetFriendPersonaName(intptr_t instancePtr, uint64_t steamIDFriend);
int SteamAPI_ISteamFriends_GetSmallFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
int SteamAPI_ISteamFriends_GetMediumFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
int SteamAPI_ISteamFriends_GetLargeFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
void SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog(intptr_t instancePtr, uint64_t steamIDLobby);
bool SteamAPI_ISteamFriends_SetRichPresence(intptr_t instancePtr, const char * pchKey, const char * pchValue);

uint64_t SteamAPI_ISteamUserStats_RequestUserStats(intptr_t instancePtr, uint64_t steamIDUser);
uint64_t SteamAPI_ISteamUserStats_FindLeaderboard(intptr_t instancePtr, const char * pchLeaderboardName);
uint64_t SteamAPI_ISteamUserStats_DownloadLeaderboardEntries(intptr_t instancePtr, uint64_t hSteamLeaderboard, int eLeaderboardDataRequest, int nRangeStart, int nRangeEnd);
bool SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry(intptr_t instancePtr, uint64_t hSteamLeaderboardEntries, int index, void * pLeaderboardEntry, int32_t * pDetails, int cDetailsMax);
bool SteamAPI_ISteamUserStats_GetStatInt32(intptr_t instancePtr, const char * pchName, int32_t * pData);
bool SteamAPI_ISteamUserStats_SetStatInt32(intptr_t instancePtr, const char * pchName, int32_t nData);
bool SteamAPI_ISteamUserStats_StoreStats(intptr_t instancePtr);
uint64_t SteamAPI_ISteamUserStats_UploadLeaderboardScore(intptr_t instancePtr, uint64_t hSteamLeaderboard, int eLeaderboardUploadScoreMethod, int32_t nScore, const int32_t * pScoreDetails, int cScoreDetailsCount);
int SteamAPI_ISteamApps_GetLaunchCommandLine(intptr_t instancePtr, char * pszCommandLine, int cubCommandLine);
}

#ifdef _WIN32
	#pragma comment(lib, "steam_api64.lib")
	#include <windows.h>
#endif
#include <fstream>
#include <vector>

extern bool g_isHostingLobby;
extern bool g_isConnectingToLobby;

// --- MULTIPLAYER STAR TOPOLOGY TRACKERS ---
std::vector<HSteamNetConnection> g_activeClientConnections;
std::vector<HSteamNetConnection> g_spectatorConnections;
bool g_isSpectator = false;

SteamManager::SteamManager()
	: m_bInitialized(false)
	, m_bIsHost(false)
	, m_CallbackUserStatsReceived(this, &SteamManager::onUserStatsReceived)
	, m_hListenSocket(k_HSteamListenSocket_Invalid)
	, m_hConnection(k_HSteamNetConnection_Invalid) {
}

SteamManager::~SteamManager() {
	cleanup();
}

void SteamManager::setup() {
	std::ofstream appidFile("steam_appid.txt");
	if (appidFile.is_open()) {
		appidFile << "4329880";
		appidFile.close();
	}

#ifdef _WIN32
	SetEnvironmentVariableA("SteamAppId", "4329880");
	SetEnvironmentVariableA("SteamGameId", "4329880");
#endif

	if (SteamAPI_Init()) {
		m_bInitialized = true;

		// CRASH FIX: SteamAPI_Init() only validates the *SDK* can talk to the
		// *Steam client pipe*. If Steam isn't running, Init may still succeed on
		// some platforms, but every subsequent call returns IPC failure 12.
		// Verify the client is alive here so we fail early instead of crashing.
		if (!SteamAPI_IsSteamRunning()) {
			ofLogError("Steam") << "SteamAPI_Init() succeeded but SteamAPI_IsSteamRunning() is false. "
								   "The game was likely launched outside Steam, or Steam is in Offline Mode.";
			m_bInitialized = false;
			SteamAPI_Shutdown();
			return;
		}

		if (!SteamUser()) {
			ofLogError("Steam") << "SteamUser() returned null.";
			m_bInitialized = false;
			SteamAPI_Shutdown();
			return;
		}

		m_LocalID = CSteamID((uint64)SteamAPI_ISteamUser_GetSteamID((intptr_t)SteamUser()));
		if (SteamNetworkingUtils()) {
			SteamNetworkingUtils()->InitRelayNetworkAccess();
		}

		// Request user stats from Steamworks backend for local player
		if (SteamUserStats()) {
			SteamAPI_ISteamUserStats_RequestUserStats((intptr_t)SteamUserStats(), m_LocalID.ConvertToUint64());
		}
		loadLocalProgressionBackup();

		ofLogNotice("Steam") << "Initialized. LocalID: " << m_LocalID.ConvertToUint64();

		// Handle Cold-Boot Invites (+connect_lobby)
		if (SteamApps()) {
			char cmdLine[1024] = { 0 };
			SteamAPI_ISteamApps_GetLaunchCommandLine((intptr_t)SteamApps(), cmdLine, sizeof(cmdLine));
			std::string cmdStr(cmdLine);
			size_t pos = cmdStr.find("+connect_lobby");
			if (pos != std::string::npos) {
				std::string lobbyIdStr = cmdStr.substr(pos + 14); // Length of "+connect_lobby"
				// Trim leading spaces
				lobbyIdStr.erase(0, lobbyIdStr.find_first_not_of(" \t"));
				// Trim trailing garbage data
				size_t endPos = lobbyIdStr.find_first_of(" \t");
				if (endPos != std::string::npos) lobbyIdStr.erase(endPos);

				try {
					uint64_t id = std::stoull(lobbyIdStr);
					if (id > 0) {
						ofLogNotice("Steam") << "Cold-boot invite detected! Auto-joining lobby: " << id;
						joinLobbyByID(CSteamID((uint64)id));
						g_isConnectingToLobby = true; // Tell UI to show "Connecting" screen
					}
				} catch (...) {
					ofLogError("Steam") << "Failed to parse cold-boot lobby ID.";
				}
			}
		}
	} else {
		ofLogError("Steam") << "Failed to init Steam API. Is Steam running?";
	}
}

bool SteamManager::isConnected() const {
	return m_bInitialized;
}

bool SteamManager::hasOpponent() const {
	if (m_bIsHost) return g_activeClientConnections.size() > 0;
	return m_hConnection != k_HSteamNetConnection_Invalid;
}

void SteamManager::update() {
	if (!m_bInitialized) return;
	SteamAPI_RunCallbacks();

	if (!m_bIsHost && m_LobbyID.IsValid() && m_hConnection == k_HSteamNetConnection_Invalid) {
		static float lastReconnectTime = 0.0f;
		if (ofGetElapsedTimef() - lastReconnectTime > 2.0f) {
			lastReconnectTime = ofGetElapsedTimef();
			if (SteamMatchmaking()) {
				CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
				if (owner.IsValid()) {
					SteamNetworkingIdentity identity;
					identity.SetSteamID(owner);
					HSteamNetConnection conn = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, nullptr);
					if (conn != k_HSteamNetConnection_Invalid) {
						m_hConnection = conn;
						ofLogNotice("Steam") << "Auto-reconnecting to host: " << owner.ConvertToUint64();
					}
				}
			}
		}
	}

	ISteamNetworkingSockets * net = SteamNetworkingSockets();
	const int MAX_MSGS = 32;
	SteamNetworkingMessage_t * msgs[MAX_MSGS];

	// Read Host connection (if Client)
	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		int numMsgs = net->ReceiveMessagesOnConnection(m_hConnection, msgs, MAX_MSGS);
		for (int i = 0; i < numMsgs; i++) {
			auto * msg = msgs[i];
			std::vector<char> buffer((char *)msg->m_pData, (char *)msg->m_pData + msg->m_cbSize);
			packetQueue.push(buffer);
			msg->Release();
		}
	}

	// Read Client connections (if Host)
	if (m_bIsHost) {
		for (auto conn : g_activeClientConnections) {
			int numMsgs = net->ReceiveMessagesOnConnection(conn, msgs, MAX_MSGS);
			for (int i = 0; i < numMsgs; i++) {
				auto * msg = msgs[i];
				std::vector<char> buffer((char *)msg->m_pData, (char *)msg->m_pData + msg->m_cbSize);
				packetQueue.push(buffer);
				msg->Release();
			}
		}
	}

	// Read Spectator connections
	for (auto conn : g_spectatorConnections) {
		int numMsgs = net->ReceiveMessagesOnConnection(conn, msgs, MAX_MSGS);
		for (int i = 0; i < numMsgs; i++) {
			auto * msg = msgs[i];
			std::vector<char> buffer((char *)msg->m_pData, (char *)msg->m_pData + msg->m_cbSize);
			packetQueue.push(buffer);
			msg->Release();
		}
	}
}

void SteamManager::cleanup() {
	leaveLobby();
	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		SteamNetworkingSockets()->CloseListenSocket(m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}
}

void SteamManager::shutdownAPI() {
	if (m_bInitialized) {
		SteamAPI_Shutdown();
		m_bInitialized = false;
		ofLogNotice("SteamManager") << "Steam API Shutdown.";
	}
}

bool SteamManager::createLobby() {
	if (!m_bInitialized || !SteamAPI_IsSteamRunning() || !SteamMatchmaking()) {
		ofLogNotice("Steam") << "createLobby: Steam offline/unavailable. Hosting offline bot lobby.";
		m_bIsHost = true;
		return true;
	}
	if (m_bIsHost) {
		ofLogWarning("Steam") << "createLobby: Already hosting.";
		return true;
	}

	ofLogNotice("Steam") << "Requesting Lobby Creation...";

	SteamAPICall_t hSteamAPICall = SteamAPI_ISteamMatchmaking_CreateLobby(
		(intptr_t)SteamMatchmaking(), k_ELobbyTypePublic, 4);

	if (hSteamAPICall == k_uAPICallInvalid) {
		ofLogWarning("Steam") << "createLobby: Steam rejected online lobby. Falling back to offline bot lobby.";
		m_bIsHost = true;
		return true;
	}

	m_bIsHost = true;
	m_cbLobbyCreated.Set(hSteamAPICall, this, &SteamManager::OnLobbyCreated);
	return true;
}

void SteamManager::leaveLobby() {
	if (!m_bInitialized) {
		m_bIsHost = false;
		m_LobbyID = CSteamID();
		closeConnection();
		return;
	}
	closeConnection();
	if (m_LobbyID.IsValid()) {
		SteamAPI_ISteamMatchmaking_LeaveLobby((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64());
		m_LobbyID = CSteamID();
	}
	m_bIsHost = false;
}

void SteamManager::closeConnection() {
	if (!m_bInitialized) {
		m_hConnection = k_HSteamNetConnection_Invalid;
		m_hListenSocket = k_HSteamListenSocket_Invalid;
		m_OpponentID = CSteamID();
		g_activeClientConnections.clear();
		g_spectatorConnections.clear();
		g_isSpectator = false;
		while (!packetQueue.empty())
			packetQueue.pop();
		return;
	}

	ISteamNetworkingSockets * net = SteamNetworkingSockets();
	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		net->CloseConnection(m_hConnection, 0, "Closing", true);
		m_hConnection = k_HSteamNetConnection_Invalid;
	}

	for (auto conn : g_activeClientConnections)
		net->CloseConnection(conn, 0, "Closing", true);
	for (auto conn : g_spectatorConnections)
		net->CloseConnection(conn, 0, "Closing", true);

	g_activeClientConnections.clear();
	g_spectatorConnections.clear();
	g_isSpectator = false;

	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		net->CloseListenSocket(m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}
	m_OpponentID = CSteamID();
	while (!packetQueue.empty())
		packetQueue.pop();
}

bool SteamManager::sendPacket(const void * data, uint32_t size) {
	if (!m_bInitialized) return false;

	if (!m_bIsHost && m_hConnection == k_HSteamNetConnection_Invalid) {
		if (m_LobbyID.IsValid() && SteamMatchmaking()) {
			CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
			if (owner.IsValid()) {
				SteamNetworkingIdentity identity;
				identity.SetSteamID(owner);
				HSteamNetConnection conn = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, nullptr);
				if (conn != k_HSteamNetConnection_Invalid) {
					m_hConnection = conn;
				}
			}
		}
		if (m_hConnection == k_HSteamNetConnection_Invalid) return false;
	}

	if (m_bIsHost) {
		// HOST BROADCASTS TO ALL CONNECTED CLIENTS
		for (auto conn : g_activeClientConnections) {
			SteamNetworkingSockets()->SendMessageToConnection(conn, data, size, k_nSteamNetworkingSend_Reliable, nullptr);
		}
		for (auto conn : g_spectatorConnections) {
			SteamNetworkingSockets()->SendMessageToConnection(conn, data, size, k_nSteamNetworkingSend_Reliable, nullptr);
		}
		return true;
	} else {
		// CLIENT SENDS DIRECTLY TO HOST
		return SteamNetworkingSockets()->SendMessageToConnection(m_hConnection, data, size, k_nSteamNetworkingSend_Reliable, nullptr) == k_EResultOK;
	}
}

bool SteamManager::isHost() const { return m_bIsHost; }

std::string SteamManager::getLocalPlayerName() const {
	if (!m_bInitialized || !SteamFriends()) return "Player";
	const char * name = SteamAPI_ISteamFriends_GetPersonaName((intptr_t)SteamFriends());
	return name ? std::string(name) : "Player";
}

std::string SteamManager::getOpponentName() const {
	if (!m_bInitialized || !SteamFriends() || !m_OpponentID.IsValid()) return "Opponent";
	const char * name = SteamAPI_ISteamFriends_GetFriendPersonaName((intptr_t)SteamFriends(), m_OpponentID.ConvertToUint64());
	return name ? std::string(name) : "Opponent";
}

CSteamID SteamManager::getOpponentSteamID() const { return m_OpponentID; }
CSteamID SteamManager::getLocalSteamID() const { return m_LocalID; }

bool SteamManager::getAvatarImage(const CSteamID & id, ofImage & outImage, int size) const {
	if (!m_bInitialized || !SteamFriends() || !SteamUtils() || !id.IsValid()) return false;
	int imageId = 0;
	if (size <= 32)
		imageId = SteamAPI_ISteamFriends_GetSmallFriendAvatar((intptr_t)SteamFriends(), id.ConvertToUint64());
	else if (size <= 64)
		imageId = SteamAPI_ISteamFriends_GetMediumFriendAvatar((intptr_t)SteamFriends(), id.ConvertToUint64());
	else
		imageId = SteamAPI_ISteamFriends_GetLargeFriendAvatar((intptr_t)SteamFriends(), id.ConvertToUint64());

	if (imageId <= 0) return false;
	uint32_t width = 0, height = 0;
	if (!SteamUtils()->GetImageSize(imageId, &width, &height) || width == 0 || height == 0) return false;

	ofPixels pixels;
	pixels.allocate(width, height, OF_PIXELS_RGBA);
	if (!SteamUtils()->GetImageRGBA(imageId, pixels.getData(), width * height * 4)) return false;

	outImage.setFromPixels(pixels);
	return true;
}

void SteamManager::openFriendOverlay() {
	if (m_bInitialized && m_LobbyID.IsValid() && SteamFriends()) {
		SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog((intptr_t)SteamFriends(), m_LobbyID.ConvertToUint64());
	}
}

void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure) {
	// CRASH FIX: pCallback can be null if the call failed before dispatch.
	if (!pCallback || bIOFailure || pCallback->m_eResult != k_EResultOK) {
		ofLogError("Steam") << "OnLobbyCreated: FAILED. bIOFailure=" << (bIOFailure ? 1 : 0)
							<< " result=" << (pCallback ? (int)pCallback->m_eResult : -1);
		m_bIsHost = false;
		g_isHostingLobby = false; // extern in SteamManager.cpp — OK
		// NOTE: Do NOT touch g_inLobby here — it's a static in ofApp.cpp.
		//       ofApp will poll m_lobbyCreationFailed and reset its own state.
		m_LobbyID = CSteamID();
		m_lobbyCreationFailed = true;
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	m_bIsHost = true;
	m_lobbyCreationFailed = false;
	m_lobbyCreationSucceeded = true;

	if (!SteamMatchmaking()) return;

	std::string lobbyName = getLocalPlayerName() + "'s Game";
	SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "name", lobbyName.c_str());
	SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "MageFightLobby", "Active");

	// CRASH FIX: Verify SteamNetworkingSockets() is valid before using it.
	if (SteamNetworkingSockets()) {
		m_hListenSocket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, nullptr);
	}
}

void SteamManager::OnLobbyEnter(LobbyEnter_t * pCallback, bool bIOFailure) {
	if (bIOFailure || pCallback->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
		g_isConnectingToLobby = false;
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);

	// BULLETPROOF HOST FIX:
	// If we initiated the lobby creation, we are 100% the Host. Never let Steam demote us!
	if (m_bIsHost || g_isHostingLobby) {
		m_bIsHost = true;
		ofLogNotice("Steam") << "Entered our own lobby as authoritative Host.";
		return;
	}

	CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
	if (owner == m_LocalID) {
		m_bIsHost = true;
	} else {
		m_bIsHost = false;
		if (owner.IsValid() && owner.ConvertToUint64() != 0) {
			SteamNetworkingIdentity identity;
			identity.SetSteamID(owner);
			m_hConnection = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, nullptr);
		} else {
			m_hConnection = k_HSteamNetConnection_Invalid;
		}
	}
}

void SteamManager::OnNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t * pInfo) {
	switch (pInfo->m_info.m_eState) {
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		if (m_bIsHost) {
			auto removeConnection = [&](std::vector<HSteamNetConnection> & connections) {
				auto newEnd = std::remove(connections.begin(), connections.end(), pInfo->m_hConn);
				bool removed = (newEnd != connections.end());
				connections.erase(newEnd, connections.end());
				return removed;
			};

			bool removedClient = removeConnection(g_activeClientConnections);
			bool removedSpectator = removeConnection(g_spectatorConnections);
			if (removedClient || removedSpectator) {
				opponentDisconnected = true;
				if (g_activeClientConnections.empty() && g_spectatorConnections.empty()) {
					m_OpponentID = CSteamID();
				}
			}
		} else if (pInfo->m_hConn == m_hConnection) {
			opponentDisconnected = true;
			m_hConnection = k_HSteamNetConnection_Invalid;
		}
		SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		break;

	case k_ESteamNetworkingConnectionState_Connecting: {
		if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
			if (SteamNetworkingSockets()->AcceptConnection(pInfo->m_hConn) == k_EResultOK) {
				ofLogNotice("Steam") << "Accepted Connection!";
			} else {
				SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			}
		}
		break;
	}

	case k_ESteamNetworkingConnectionState_Connected: {
		if (m_bIsHost) {
			// Add to our list of active clients
			if (std::find(g_activeClientConnections.begin(), g_activeClientConnections.end(), pInfo->m_hConn) == g_activeClientConnections.end()) {
				g_activeClientConnections.push_back(pInfo->m_hConn);
				ofLogNotice("Steam") << "Client fully connected: " << pInfo->m_hConn;
			}
		} else {
			m_hConnection = pInfo->m_hConn;
			if (pInfo->m_info.m_identityRemote.GetSteamID64() != 0) {
				m_OpponentID = CSteamID(pInfo->m_info.m_identityRemote.GetSteamID64());
			}
		}
		break;
	}
	default:
		break;
	}
}

void SteamManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback) {
	SteamAPICall_t hSteamAPICall = SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), pCallback->m_steamIDLobby.ConvertToUint64());
	m_cbLobbyEntered.Set(hSteamAPICall, this, &SteamManager::OnLobbyEnter);
	g_isConnectingToLobby = true; // Tell UI to show "Connecting" screen
}

void SteamManager::OnGameJoinRequested(GameRichPresenceJoinRequested_t * pCallback) {
	std::string cmd = pCallback->m_rgchConnect;
	size_t split = cmd.find("+connect_lobby ");
	if (split != std::string::npos) {
		try {
			CSteamID id(std::stoull(cmd.substr(split + 15))); // +15 steps past "+connect_lobby "
			SteamAPICall_t hSteamAPICall = SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), id.ConvertToUint64());
			m_cbLobbyEntered.Set(hSteamAPICall, this, &SteamManager::OnLobbyEnter);
			g_isConnectingToLobby = true; // Tell UI to show "Connecting" screen
		} catch (...) {
			ofLogError("Steam") << "Failed to parse Rich Presence lobby ID.";
		}
	}
}

void SteamManager::setMatchStarted() {
	if (m_bInitialized && SteamMatchmaking() && m_LobbyID.IsValid()) {
		SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "match_started", "1");
	}
}
bool SteamManager::isMatchStarted() const {
	if (!m_LobbyID.IsValid()) return false;
	const char * val = SteamAPI_ISteamMatchmaking_GetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "match_started");
	if (!val) return false;
	return std::string(val) == "1";
}
void SteamManager::setLobbySeed(uint32_t seed) {
	if (m_bInitialized && SteamMatchmaking() && m_LobbyID.IsValid()) {
		SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "seed", std::to_string(seed).c_str());
	}
}
uint32_t SteamManager::getLobbySeed() const {
	if (!m_LobbyID.IsValid()) return 0;
	const char * v = SteamAPI_ISteamMatchmaking_GetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "seed");
	if (!v || strlen(v) == 0) return 0;
	try {
		return (uint32_t)std::stoul(std::string(v));
	} catch (...) {
		return 0;
	}
}
bool SteamManager::checkAndClearDisconnectFlag() {
	bool result = opponentDisconnected;
	opponentDisconnected = false;
	return result;
}
bool SteamManager::checkAndClearReconnectFlag() {
	bool result = opponentReconnected;
	opponentReconnected = false;
	return result;
}
void SteamManager::refreshLobbies() {
	if (!SteamMatchmaking()) return;
	SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter((intptr_t)SteamMatchmaking(), 50);
	SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter((intptr_t)SteamMatchmaking(), 3);
	SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter((intptr_t)SteamMatchmaking(), "MageFightLobby", "Active", 0);
	SteamAPICall_t hSteamAPICall = SteamAPI_ISteamMatchmaking_RequestLobbyList((intptr_t)SteamMatchmaking());
	m_LobbyMatchListCallResult.Set(hSteamAPICall, this, &SteamManager::OnLobbyMatchList);
}
void SteamManager::OnLobbyMatchList(LobbyMatchList_t * pCallback, bool bIOFailure) {
	currentLobbies.clear();
	if (bIOFailure) return;
	for (uint32_t i = 0; i < pCallback->m_nLobbiesMatching; i++) {
		CSteamID lobbyID((uint64)SteamAPI_ISteamMatchmaking_GetLobbyByIndex((intptr_t)SteamMatchmaking(), i));
		LobbyInfo info;
		info.lobbyID = lobbyID;
		const char * name = SteamAPI_ISteamMatchmaking_GetLobbyData((intptr_t)SteamMatchmaking(), lobbyID.ConvertToUint64(), "name");
		info.name = (name && name[0]) ? name : "Mage Fight Match";
		info.numPlayers = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers((intptr_t)SteamMatchmaking(), lobbyID.ConvertToUint64());
		info.maxPlayers = SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit((intptr_t)SteamMatchmaking(), lobbyID.ConvertToUint64());
		if (info.maxPlayers <= 0) info.maxPlayers = 4; // Expanded for 4-Player Lobbies
		currentLobbies.push_back(info);
	}
}
std::vector<SteamManager::LobbyInfo> SteamManager::getLobbyList() { return currentLobbies; }
void SteamManager::joinLobbyByID(CSteamID lobbyID) {
	if (!SteamMatchmaking()) return;
	SteamAPICall_t hSteamAPICall = SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), lobbyID.ConvertToUint64());
	m_cbLobbyEntered.Set(hSteamAPICall, this, &SteamManager::OnLobbyEnter);
}
void SteamManager::fetchLeaderboard() {
	if (!SteamUserStats()) return;
	SteamAPICall_t hSteamAPICall = SteamAPI_ISteamUserStats_FindLeaderboard((intptr_t)SteamUserStats(), "Global_Rankings");
	m_LeaderboardFindCallResult.Set(hSteamAPICall, this, &SteamManager::OnLeaderboardFindResult);
}
void SteamManager::OnLeaderboardFindResult(LeaderboardFindResult_t * pCallback, bool bIOFailure) {
	if (!bIOFailure && pCallback->m_bLeaderboardFound) {
		currentLeaderboardHandle = pCallback->m_hSteamLeaderboard;
		SteamAPICall_t hSteamAPICall = SteamAPI_ISteamUserStats_DownloadLeaderboardEntries((intptr_t)SteamUserStats(), currentLeaderboardHandle, k_ELeaderboardDataRequestGlobal, 0, 100);
		m_LeaderboardScoresDownloadedCallResult.Set(hSteamAPICall, this, &SteamManager::OnLeaderboardScoresDownloaded);
	}
}
void SteamManager::OnLeaderboardScoresDownloaded(LeaderboardScoresDownloaded_t * pCallback, bool bIOFailure) {
	currentLeaderboard.clear();
	if (bIOFailure) return;
	for (int index = 0; index < pCallback->m_cEntryCount; index++) {
		LeaderboardEntry_t leaderboardEntry;
		SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry((intptr_t)SteamUserStats(), pCallback->m_hSteamLeaderboardEntries, index, &leaderboardEntry, NULL, 0);
		LeaderboardEntry entry;
		entry.rank = leaderboardEntry.m_nGlobalRank;
		entry.score = leaderboardEntry.m_nScore;
		const char * name = SteamAPI_ISteamFriends_GetFriendPersonaName((intptr_t)SteamFriends(), leaderboardEntry.m_steamIDUser.ConvertToUint64());
		entry.name = name ? name : "Unknown";
		currentLeaderboard.push_back(entry);
	}
}
std::vector<SteamManager::LeaderboardEntry> SteamManager::getLeaderboardEntries() { return currentLeaderboard; }
int SteamManager::getLocalElo() {
	if (!m_bInitialized || !SteamUserStats()) return -1;
	int32_t elo = 1000;
	if (!SteamAPI_ISteamUserStats_GetStatInt32((intptr_t)SteamUserStats(), "elo_rating", &elo)) {
		// CRASH FIX: Return -1 (not 1000) so callers can detect IPC failure.
		// The game treats -1 as "still loading" and retries, which is correct
		// when Steam Cloud stats haven't downloaded yet.
		return -1;
	}
	return (int)elo;
}
void SteamManager::setLocalElo(int elo) {
	if (!m_bInitialized || !SteamUserStats()) return;
	if (elo <= 300) elo = 300;
	if (!SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "elo_rating", elo)) {
		ofLogWarning("Steam") << "setLocalElo: SetStat failed (IPC unavailable?).";
		return;
	}
	SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
	if (currentLeaderboardHandle != 0) {
		SteamAPI_ISteamUserStats_UploadLeaderboardScore((intptr_t)SteamUserStats(), currentLeaderboardHandle, 2, elo, nullptr, 0);
	}
}
void SteamManager::armLeaverBuster(int oppElo) {
	if (!m_bInitialized || !SteamUserStats()) return;
	if (!SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "leaver_opp_elo", oppElo)) {
		ofLogWarning("Steam") << "armLeaverBuster: SetStat failed (IPC unavailable?).";
		return;
	}
	SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
}
void SteamManager::disarmLeaverBuster() {
	if (!m_bInitialized || !SteamUserStats()) return;
	if (!SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "leaver_opp_elo", 0)) {
		ofLogWarning("Steam") << "disarmLeaverBuster: SetStat failed (IPC unavailable?).";
		return;
	}
	SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
}
int SteamManager::checkLeaverBuster() {
	if (!m_bInitialized || !SteamUserStats()) return 0;
	int32_t oppElo = 0;
	if (!SteamAPI_ISteamUserStats_GetStatInt32((intptr_t)SteamUserStats(), "leaver_opp_elo", &oppElo)) {
		ofLogWarning("Steam") << "checkLeaverBuster: GetStat failed (IPC unavailable?).";
		return 0;
	}
	return (int)oppElo;
}
void SteamManager::updateRichPresence(const std::string & presenceText) {
	if (!SteamAPI_IsSteamRunning() || !SteamFriends()) return;
	SteamFriends()->SetRichPresence("status", presenceText.c_str());
	SteamFriends()->SetRichPresence("steam_display", "#Status");
}
void SteamManager::onUserStatsReceived(UserStatsReceived_t * pCallback) {
	if (pCallback && pCallback->m_eResult == k_EResultOK) {
		m_bStatsLoaded = true;
	}
}

// --- XP & LEVEL SYSTEM IMPLEMENTATION ---
int SteamManager::getXPRequiredForLevel(int level) {
	if (level <= 0) level = 1;
	// Smooth progression: Level 1 requires 1000 XP, Level 2 requires 1500 XP, Level 3 requires 2000 XP, etc.
	return 500 + (level * 500);
}

void SteamManager::loadLocalProgressionBackup() {
	std::string path = "Saves/progression.json";
	if (ofFile(path).exists()) {
		try {
			ofJson j = ofLoadJson(path);
			m_cachedXP = j.value("xp", 0);
			m_cachedLevel = j.value("level", 1);
		} catch (...) {
			m_cachedXP = 0;
			m_cachedLevel = 1;
		}
	} else {
		m_cachedXP = 0;
		m_cachedLevel = 1;
	}
}

void SteamManager::saveLocalProgressionBackup(int xp, int level) {
	try {
		ofJson j;
		j["xp"] = xp;
		j["level"] = level;
		ofSaveJson("Saves/progression.json", j);
	} catch (...) {
		ofLogWarning("Steam") << "Failed to save local progression backup.";
	}
}

int SteamManager::getLocalXP() {
	if (m_bInitialized && SteamUserStats()) {
		int32_t xpVal = 0;
		if (SteamAPI_ISteamUserStats_GetStatInt32((intptr_t)SteamUserStats(), "player_xp", &xpVal)) {
			m_cachedXP = (int)xpVal;
			return m_cachedXP;
		}
	}
	if (m_cachedXP < 0) loadLocalProgressionBackup();
	return std::max(0, m_cachedXP);
}

int SteamManager::getLocalLevel() {
	if (m_bInitialized && SteamUserStats()) {
		int32_t lvlVal = 1;
		if (SteamAPI_ISteamUserStats_GetStatInt32((intptr_t)SteamUserStats(), "player_level", &lvlVal)) {
			m_cachedLevel = std::max(1, (int)lvlVal);
			return m_cachedLevel;
		}
	}
	if (m_cachedLevel < 1) loadLocalProgressionBackup();
	return std::max(1, m_cachedLevel);
}

SteamManager::XPGainResult SteamManager::addXP(int amount) {
	XPGainResult result;
	if (amount < 0) amount = 0;
	result.xpEarned = amount;

	int curXP = getLocalXP();
	int curLvl = getLocalLevel();

	result.oldXP = curXP;
	result.oldLevel = curLvl;

	curXP += amount;
	int req = getXPRequiredForLevel(curLvl);

	// Process potential level-ups
	while (curXP >= req) {
		curXP -= req;
		curLvl++;
		result.leveledUp = true;
		req = getXPRequiredForLevel(curLvl);
	}

	result.newXP = curXP;
	result.newLevel = curLvl;
	result.xpRequiredForNext = req;

	m_cachedXP = curXP;
	m_cachedLevel = curLvl;

	// Save to Steamworks backend if available
	if (m_bInitialized && SteamUserStats()) {
		SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "player_xp", curXP);
		SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "player_level", curLvl);
		SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
	}

	// Always update local disk backup
	saveLocalProgressionBackup(curXP, curLvl);

	ofLogNotice("Progression") << "Awarded " << amount << " XP. Current: Level " << curLvl << " (" << curXP << "/" << req << " XP)";
	return result;
}
void SteamManager::becomeHost() {
	m_bIsHost = true;
	g_isHostingLobby = true;
	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		SteamNetworkingSockets()->CloseConnection(m_hConnection, 0, nullptr, false);
		m_hConnection = k_HSteamNetConnection_Invalid;
	}
	if (m_hListenSocket == k_HSteamListenSocket_Invalid && SteamNetworkingSockets()) {
		m_hListenSocket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, nullptr);
	}
}
CSteamID SteamManager::getLobbyOwner() const {
	if (!m_bInitialized || !m_LobbyID.IsValid() || !SteamMatchmaking()) return CSteamID();
	return CSteamID((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
}

bool SteamManager::isLocalLobbyOwner() const {
	return getLobbyOwner() == m_LocalID;
}