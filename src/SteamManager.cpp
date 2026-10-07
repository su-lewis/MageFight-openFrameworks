#include "SteamManager.h"
#include <algorithm>

#ifdef _WIN32
	#define S_CALLTYPE __cdecl
#else
	#define S_CALLTYPE
#endif

// Define the safe Flat C API functions manually to avoid MinGW C++ VTable/Stack alignment crashes
extern "C" {
uint64_t S_CALLTYPE SteamAPI_ISteamUser_GetSteamID(intptr_t instancePtr);

uint32_t S_CALLTYPE SteamAPI_ISteamUser_GetAuthSessionTicket(intptr_t instancePtr, void * pTicket, int cbMaxTicket, uint32_t * pcbTicket, void * pIdentityRemote);
void S_CALLTYPE SteamAPI_ISteamUser_CancelAuthTicket(intptr_t instancePtr, uint32_t hAuthTicket);
void S_CALLTYPE SteamAPI_ISteamUser_EndAuthSession(intptr_t instancePtr, uint64_t steamID);

uint64_t S_CALLTYPE SteamAPI_ISteamMatchmaking_CreateLobby(intptr_t instancePtr, int eLobbyType, int cMaxMembers);
void S_CALLTYPE SteamAPI_ISteamMatchmaking_LeaveLobby(intptr_t instancePtr, uint64_t steamIDLobby);
uint64_t S_CALLTYPE SteamAPI_ISteamMatchmaking_GetLobbyOwner(intptr_t instancePtr, uint64_t steamIDLobby);
bool S_CALLTYPE SteamAPI_ISteamMatchmaking_SetLobbyData(intptr_t instancePtr, uint64_t steamIDLobby, const char * pchKey, const char * pchValue);
const char * S_CALLTYPE SteamAPI_ISteamMatchmaking_GetLobbyData(intptr_t instancePtr, uint64_t steamIDLobby, const char * pchKey);
uint64_t S_CALLTYPE SteamAPI_ISteamMatchmaking_JoinLobby(intptr_t instancePtr, uint64_t steamIDLobby);
void S_CALLTYPE SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter(intptr_t instancePtr, int cMaxResults);
void S_CALLTYPE SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter(intptr_t instancePtr, const char * pchKeyToMatch, const char * pchValueToMatch, int eComparisonType);
void S_CALLTYPE SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter(intptr_t instancePtr, int eLobbyDistanceFilter);
uint64_t S_CALLTYPE SteamAPI_ISteamMatchmaking_RequestLobbyList(intptr_t instancePtr);
uint64_t S_CALLTYPE SteamAPI_ISteamMatchmaking_GetLobbyByIndex(intptr_t instancePtr, int iLobby);
int S_CALLTYPE SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(intptr_t instancePtr, uint64_t steamIDLobby);
int S_CALLTYPE SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit(intptr_t instancePtr, uint64_t steamIDLobby);

const char * S_CALLTYPE SteamAPI_ISteamFriends_GetPersonaName(intptr_t instancePtr);
const char * S_CALLTYPE SteamAPI_ISteamFriends_GetFriendPersonaName(intptr_t instancePtr, uint64_t steamIDFriend);
int S_CALLTYPE SteamAPI_ISteamFriends_GetSmallFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
int S_CALLTYPE SteamAPI_ISteamFriends_GetMediumFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
int S_CALLTYPE SteamAPI_ISteamFriends_GetLargeFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
void S_CALLTYPE SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog(intptr_t instancePtr, uint64_t steamIDLobby);
bool S_CALLTYPE SteamAPI_ISteamFriends_SetRichPresence(intptr_t instancePtr, const char * pchKey, const char * pchValue);

uint64_t S_CALLTYPE SteamAPI_ISteamUserStats_RequestUserStats(intptr_t instancePtr, uint64_t steamIDUser);
uint64_t S_CALLTYPE SteamAPI_ISteamUserStats_FindLeaderboard(intptr_t instancePtr, const char * pchLeaderboardName);
uint64_t S_CALLTYPE SteamAPI_ISteamUserStats_DownloadLeaderboardEntries(intptr_t instancePtr, uint64_t hSteamLeaderboard, int eLeaderboardDataRequest, int nRangeStart, int nRangeEnd);
bool S_CALLTYPE SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry(intptr_t instancePtr, uint64_t hSteamLeaderboardEntries, int index, void * pLeaderboardEntry, int32_t * pDetails, int cDetailsMax);
bool S_CALLTYPE SteamAPI_ISteamUserStats_GetStatInt32(intptr_t instancePtr, const char * pchName, int32_t * pData);
bool S_CALLTYPE SteamAPI_ISteamUserStats_SetStatInt32(intptr_t instancePtr, const char * pchName, int32_t nData);
bool S_CALLTYPE SteamAPI_ISteamUserStats_StoreStats(intptr_t instancePtr);
uint64_t S_CALLTYPE SteamAPI_ISteamUserStats_UploadLeaderboardScore(intptr_t instancePtr, uint64_t hSteamLeaderboard, int eLeaderboardUploadScoreMethod, int32_t nScore, const int32_t * pScoreDetails, int cScoreDetailsCount);

int S_CALLTYPE SteamAPI_ISteamApps_GetLaunchCommandLine(intptr_t instancePtr, char * pszCommandLine, int cubCommandLine);

bool S_CALLTYPE SteamAPI_ISteamUtils_GetImageSize(intptr_t instancePtr, int iImage, uint32_t * pnWidth, uint32_t * pnHeight);
bool S_CALLTYPE SteamAPI_ISteamUtils_GetImageRGBA(intptr_t instancePtr, int iImage, uint8_t * pubDest, int nDestBufferSize);

// --- CRITICAL FIX: Safe Networking Flat API ---
void S_CALLTYPE SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(intptr_t instancePtr);
HSteamListenSocket S_CALLTYPE SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P(intptr_t instancePtr, int nLocalVirtualPort, int nOptions, const void * pOptions);
HSteamNetConnection S_CALLTYPE SteamAPI_ISteamNetworkingSockets_ConnectP2P(intptr_t instancePtr, const SteamNetworkingIdentity * pIdentityRemote, int nRemoteVirtualPort, int nOptions, const void * pOptions);
int S_CALLTYPE SteamAPI_ISteamNetworkingSockets_SendMessageToConnection(intptr_t instancePtr, HSteamNetConnection hConn, const void * pData, uint32_t cbData, int nSendFlags, int64_t * pOutMessageNumber);
int S_CALLTYPE SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection(intptr_t instancePtr, HSteamNetConnection hConn, SteamNetworkingMessage_t ** ppOutMessages, int nMaxMessages);
int S_CALLTYPE SteamAPI_ISteamNetworkingSockets_AcceptConnection(intptr_t instancePtr, HSteamNetConnection hConn);
bool S_CALLTYPE SteamAPI_ISteamNetworkingSockets_CloseConnection(intptr_t instancePtr, HSteamNetConnection hPeer, int nReason, const char * pszDebug, bool bEnableLinger);
bool S_CALLTYPE SteamAPI_ISteamNetworkingSockets_CloseListenSocket(intptr_t instancePtr, HSteamListenSocket hSocket);
void S_CALLTYPE SteamAPI_SteamNetworkingMessage_t_Release(SteamNetworkingMessage_t * instancePtr);
}

#ifdef _WIN32
	#pragma comment(lib, "steam_api64.lib")
	#include <windows.h>
#endif
#include <fstream>
#include <vector>

extern bool g_isHostingLobby;
extern bool g_isConnectingToLobby;

std::vector<HSteamNetConnection> g_activeClientConnections;
std::vector<HSteamNetConnection> g_spectatorConnections;
bool g_isSpectator = false;

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

		// Tells Steam to hold callbacks in a Flat C queue instead of using C++ VTables
		SteamAPI_ManualDispatch_Init();

		if (!SteamAPI_IsSteamRunning()) {
			ofLogError("Steam") << "SteamAPI_Init() succeeded but SteamAPI_IsSteamRunning() is false.";
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
			SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess((intptr_t)SteamNetworkingUtils());
		}

		if (SteamUserStats()) {
			SteamAPI_ISteamUserStats_RequestUserStats((intptr_t)SteamUserStats(), m_LocalID.ConvertToUint64());
		}
		loadLocalProgressionBackup();

		ofLogNotice("Steam") << "Initialized. LocalID: " << m_LocalID.ConvertToUint64();

		if (SteamApps()) {
			char cmdLine[1024] = { 0 };
			SteamAPI_ISteamApps_GetLaunchCommandLine((intptr_t)SteamApps(), cmdLine, sizeof(cmdLine));
			std::string cmdStr(cmdLine);
			size_t pos = cmdStr.find("+connect_lobby");
			if (pos != std::string::npos) {
				std::string lobbyIdStr = cmdStr.substr(pos + 14);
				lobbyIdStr.erase(0, lobbyIdStr.find_first_not_of(" \t"));
				size_t endPos = lobbyIdStr.find_first_of(" \t");
				if (endPos != std::string::npos) lobbyIdStr.erase(endPos);

				try {
					uint64_t id = std::stoull(lobbyIdStr);
					if (id > 0) {
						joinLobbyByID(CSteamID((uint64)id));
						g_isConnectingToLobby = true;
					}
				} catch (...) { }
			}
		}
	} else {
		ofLogError("Steam") << "Failed to init Steam API.";
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

	// --- NEW FLAT C CALLBACK ROUTER ---
	int32_t hPipe = SteamAPI_GetHSteamPipe();
	SteamAPI_ManualDispatch_RunFrame(hPipe);

	CallbackMsg_t callbackMsg;
	while (SteamAPI_ManualDispatch_GetNextCallback(hPipe, &callbackMsg)) {

		// 1. Check for Async Call Results (Lobby Creation, Leaderboards, etc)
		if (callbackMsg.m_iCallback == SteamAPICallCompleted_t::k_iCallback) {
			SteamAPICallCompleted_t * pCallCompleted = (SteamAPICallCompleted_t *)callbackMsg.m_pubParam;
			void * pData = malloc(pCallCompleted->m_cubParam);
			bool bFailed = false;

			if (SteamAPI_ManualDispatch_GetAPICallResult(hPipe, pCallCompleted->m_hAsyncCall, pData, pCallCompleted->m_cubParam, pCallCompleted->m_iCallback, &bFailed)) {
				switch (pCallCompleted->m_iCallback) {
				case LobbyCreated_t::k_iCallback:
					OnLobbyCreated((LobbyCreated_t *)pData, bFailed);
					break;
				case LobbyEnter_t::k_iCallback:
					OnLobbyEnter((LobbyEnter_t *)pData, bFailed);
					break;
				case LobbyMatchList_t::k_iCallback:
					OnLobbyMatchList((LobbyMatchList_t *)pData, bFailed);
					break;
				case LeaderboardFindResult_t::k_iCallback:
					OnLeaderboardFindResult((LeaderboardFindResult_t *)pData, bFailed);
					break;
				case LeaderboardScoresDownloaded_t::k_iCallback:
					OnLeaderboardScoresDownloaded((LeaderboardScoresDownloaded_t *)pData, bFailed);
					break;
				}
			}
			free(pData);
		}
		// 2. Check for Standard Callbacks (Join Requests, Network Events, etc)
		else {
			switch (callbackMsg.m_iCallback) {
			case UserStatsReceived_t::k_iCallback:
				onUserStatsReceived((UserStatsReceived_t *)callbackMsg.m_pubParam);
				break;
			case GameLobbyJoinRequested_t::k_iCallback:
				OnGameLobbyJoinRequested((GameLobbyJoinRequested_t *)callbackMsg.m_pubParam);
				break;
			case GameRichPresenceJoinRequested_t::k_iCallback:
				OnGameJoinRequested((GameRichPresenceJoinRequested_t *)callbackMsg.m_pubParam);
				break;
			case SteamNetConnectionStatusChangedCallback_t::k_iCallback:
				OnNetConnectionStatusChanged((SteamNetConnectionStatusChangedCallback_t *)callbackMsg.m_pubParam);
				break;
			}
		}

		SteamAPI_ManualDispatch_FreeLastCallback(hPipe);
	}
	// --- END FLAT C ROUTER ---

	if (!m_bIsHost && m_LobbyID.IsValid() && m_hConnection == k_HSteamNetConnection_Invalid) {
		static float lastReconnectTime = 0.0f;
		if (ofGetElapsedTimef() - lastReconnectTime > 2.0f) {
			lastReconnectTime = ofGetElapsedTimef();
			if (SteamMatchmaking()) {
				CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
				if (owner.IsValid() && SteamNetworkingSockets()) {
					SteamNetworkingIdentity identity;
					memset(&identity, 0, sizeof(identity)); // <--- CRITICAL PADDING FIX
					identity.SetSteamID(owner);
					HSteamNetConnection conn = SteamAPI_ISteamNetworkingSockets_ConnectP2P((intptr_t)SteamNetworkingSockets(), &identity, 0, 0, nullptr);
					if (conn != k_HSteamNetConnection_Invalid) {
						m_hConnection = conn;
					}
				}
			}
		}
	}

	if (!SteamNetworkingSockets()) return;

	const int MAX_MSGS = 32;
	SteamNetworkingMessage_t * msgs[MAX_MSGS];

	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		int numMsgs = SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection((intptr_t)SteamNetworkingSockets(), m_hConnection, msgs, MAX_MSGS);
		for (int i = 0; i < numMsgs; i++) {
			auto * msg = msgs[i];
			std::vector<char> buffer((char *)msg->m_pData, (char *)msg->m_pData + msg->m_cbSize);
			packetQueue.push(buffer);
			SteamAPI_SteamNetworkingMessage_t_Release(msg);
		}
	}

	if (m_bIsHost) {
		for (auto conn : g_activeClientConnections) {
			int numMsgs = SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection((intptr_t)SteamNetworkingSockets(), conn, msgs, MAX_MSGS);
			for (int i = 0; i < numMsgs; i++) {
				auto * msg = msgs[i];
				std::vector<char> buffer((char *)msg->m_pData, (char *)msg->m_pData + msg->m_cbSize);
				packetQueue.push(buffer);
				SteamAPI_SteamNetworkingMessage_t_Release(msg);
			}
		}
	}

	for (auto conn : g_spectatorConnections) {
		int numMsgs = SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection((intptr_t)SteamNetworkingSockets(), conn, msgs, MAX_MSGS);
		for (int i = 0; i < numMsgs; i++) {
			auto * msg = msgs[i];
			std::vector<char> buffer((char *)msg->m_pData, (char *)msg->m_pData + msg->m_cbSize);
			packetQueue.push(buffer);
			SteamAPI_SteamNetworkingMessage_t_Release(msg);
		}
	}
}

void SteamManager::cleanup() {
	leaveLobby();
	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		SteamAPI_ISteamNetworkingSockets_CloseListenSocket((intptr_t)SteamNetworkingSockets(), m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}
}

void SteamManager::shutdownAPI() {
	if (m_bInitialized) {
		SteamAPI_Shutdown();
		m_bInitialized = false;
	}
}

bool SteamManager::createLobby() {
	if (!m_bInitialized || !SteamAPI_IsSteamRunning() || !SteamMatchmaking()) {
		m_bIsHost = true;
		return true;
	}
	if (m_bIsHost) return true;

	SteamAPI_ISteamMatchmaking_CreateLobby((intptr_t)SteamMatchmaking(), k_ELobbyTypePublic, 4);
	m_bIsHost = true;
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

	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		SteamAPI_ISteamNetworkingSockets_CloseConnection((intptr_t)SteamNetworkingSockets(), m_hConnection, 0, "Closing", true);
		m_hConnection = k_HSteamNetConnection_Invalid;
	}

	for (auto conn : g_activeClientConnections)
		SteamAPI_ISteamNetworkingSockets_CloseConnection((intptr_t)SteamNetworkingSockets(), conn, 0, "Closing", true);
	for (auto conn : g_spectatorConnections)
		SteamAPI_ISteamNetworkingSockets_CloseConnection((intptr_t)SteamNetworkingSockets(), conn, 0, "Closing", true);

	g_activeClientConnections.clear();
	g_spectatorConnections.clear();
	g_isSpectator = false;

	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		SteamAPI_ISteamNetworkingSockets_CloseListenSocket((intptr_t)SteamNetworkingSockets(), m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}
	m_OpponentID = CSteamID();
	while (!packetQueue.empty())
		packetQueue.pop();
}

bool SteamManager::sendPacket(const void * data, uint32_t size) {
	if (!m_bInitialized || !SteamNetworkingSockets()) return false;

	if (!m_bIsHost && m_hConnection == k_HSteamNetConnection_Invalid) {
		if (m_LobbyID.IsValid() && SteamMatchmaking()) {
			CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
			if (owner.IsValid()) {
				SteamNetworkingIdentity identity;
				memset(&identity, 0, sizeof(identity)); // <--- CRITICAL PADDING FIX
				identity.SetSteamID(owner);
				HSteamNetConnection conn = SteamAPI_ISteamNetworkingSockets_ConnectP2P((intptr_t)SteamNetworkingSockets(), &identity, 0, 0, nullptr);
				if (conn != k_HSteamNetConnection_Invalid) {
					m_hConnection = conn;
				}
			}
		}
		if (m_hConnection == k_HSteamNetConnection_Invalid) return false;
	}

	if (m_bIsHost) {
		for (auto conn : g_activeClientConnections) {
			SteamAPI_ISteamNetworkingSockets_SendMessageToConnection((intptr_t)SteamNetworkingSockets(), conn, data, size, k_nSteamNetworkingSend_Reliable, nullptr);
		}
		for (auto conn : g_spectatorConnections) {
			SteamAPI_ISteamNetworkingSockets_SendMessageToConnection((intptr_t)SteamNetworkingSockets(), conn, data, size, k_nSteamNetworkingSend_Reliable, nullptr);
		}
		return true;
	} else {
		return SteamAPI_ISteamNetworkingSockets_SendMessageToConnection((intptr_t)SteamNetworkingSockets(), m_hConnection, data, size, k_nSteamNetworkingSend_Reliable, nullptr) == k_EResultOK;
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

	if (!SteamAPI_ISteamUtils_GetImageSize((intptr_t)SteamUtils(), imageId, &width, &height) || width == 0 || height == 0) return false;

	ofPixels pixels;
	pixels.allocate(width, height, OF_PIXELS_RGBA);
	if (!SteamAPI_ISteamUtils_GetImageRGBA((intptr_t)SteamUtils(), imageId, pixels.getData(), width * height * 4)) return false;

	outImage.setFromPixels(pixels);
	return true;
}

void SteamManager::openFriendOverlay() {
	if (m_bInitialized && m_LobbyID.IsValid() && SteamFriends()) {
		SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog((intptr_t)SteamFriends(), m_LobbyID.ConvertToUint64());
	}
}

void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure) {
	if (!pCallback || bIOFailure || pCallback->m_eResult != k_EResultOK) {
		m_bIsHost = false;
		g_isHostingLobby = false;
		m_LobbyID = CSteamID();
		m_lobbyCreationFailed = true;
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	m_bIsHost = true;
	m_lobbyCreationFailed = false;
	m_lobbyCreationSucceeded = true;

	if (!SteamMatchmaking()) return;

	static char nameBuffer[256];
	std::string lobbyName = getLocalPlayerName() + "'s Game";
	snprintf(nameBuffer, sizeof(nameBuffer), "%s", lobbyName.c_str());

	SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "name", nameBuffer);
	SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "MageFightLobby", "Active");

	if (SteamNetworkingSockets()) {
		m_hListenSocket = SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P((intptr_t)SteamNetworkingSockets(), 0, 0, nullptr);
	}
}

void SteamManager::OnLobbyEnter(LobbyEnter_t * pCallback, bool bIOFailure) {
	if (bIOFailure || pCallback->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
		g_isConnectingToLobby = false;
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);

	if (m_bIsHost || g_isHostingLobby) {
		m_bIsHost = true;
		return;
	}

	CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
	if (owner == m_LocalID) {
		m_bIsHost = true;
	} else {
		m_bIsHost = false;
		if (owner.IsValid() && owner.ConvertToUint64() != 0) {
			SteamNetworkingIdentity identity;
			memset(&identity, 0, sizeof(identity)); // <--- CRITICAL PADDING FIX
			identity.SetSteamID(owner);
			m_hConnection = SteamAPI_ISteamNetworkingSockets_ConnectP2P((intptr_t)SteamNetworkingSockets(), &identity, 0, 0, nullptr);
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
		SteamAPI_ISteamNetworkingSockets_CloseConnection((intptr_t)SteamNetworkingSockets(), pInfo->m_hConn, 0, nullptr, false);
		break;

	case k_ESteamNetworkingConnectionState_Connecting: {
		if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
			if (SteamAPI_ISteamNetworkingSockets_AcceptConnection((intptr_t)SteamNetworkingSockets(), pInfo->m_hConn) == k_EResultOK) {
				ofLogNotice("Steam") << "Accepted Connection!";
			} else {
				SteamAPI_ISteamNetworkingSockets_CloseConnection((intptr_t)SteamNetworkingSockets(), pInfo->m_hConn, 0, nullptr, false);
			}
		}
		break;
	}

	case k_ESteamNetworkingConnectionState_Connected: {
		if (m_bIsHost) {
			if (std::find(g_activeClientConnections.begin(), g_activeClientConnections.end(), pInfo->m_hConn) == g_activeClientConnections.end()) {
				g_activeClientConnections.push_back(pInfo->m_hConn);
			}
			if (pInfo->m_info.m_identityRemote.GetSteamID64() != 0) {
				m_OpponentID = CSteamID(pInfo->m_info.m_identityRemote.GetSteamID64());
				opponentReconnected = true;
			}
		} else {
			m_hConnection = pInfo->m_hConn;
			if (pInfo->m_info.m_identityRemote.GetSteamID64() != 0) {
				m_OpponentID = CSteamID(pInfo->m_info.m_identityRemote.GetSteamID64());
				opponentReconnected = true;
			}
		}
		break;
	}
	default:
		break;
	}
}

void SteamManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback) {
	SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), pCallback->m_steamIDLobby.ConvertToUint64());
	g_isConnectingToLobby = true;
}

void SteamManager::OnGameJoinRequested(GameRichPresenceJoinRequested_t * pCallback) {
	std::string cmd = pCallback->m_rgchConnect;
	size_t split = cmd.find("+connect_lobby ");
	if (split != std::string::npos) {
		try {
			CSteamID id(std::stoull(cmd.substr(split + 15)));
			SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), id.ConvertToUint64());
			g_isConnectingToLobby = true;
		} catch (...) { }
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
		static char seedBuffer[64];
		snprintf(seedBuffer, sizeof(seedBuffer), "%u", seed);
		SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "seed", seedBuffer);
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
	SteamAPI_ISteamMatchmaking_RequestLobbyList((intptr_t)SteamMatchmaking());
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
		if (info.maxPlayers <= 0) info.maxPlayers = 4;
		currentLobbies.push_back(info);
	}
}
std::vector<SteamManager::LobbyInfo> SteamManager::getLobbyList() { return currentLobbies; }
void SteamManager::joinLobbyByID(CSteamID lobbyID) {
	if (!SteamMatchmaking()) return;
	SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), lobbyID.ConvertToUint64());
}
void SteamManager::fetchLeaderboard() {
	if (!SteamUserStats()) return;
	SteamAPI_ISteamUserStats_FindLeaderboard((intptr_t)SteamUserStats(), "Global_Rankings");
}
void SteamManager::OnLeaderboardFindResult(LeaderboardFindResult_t * pCallback, bool bIOFailure) {
	if (!bIOFailure && pCallback->m_bLeaderboardFound) {
		currentLeaderboardHandle = pCallback->m_hSteamLeaderboard;
		SteamAPI_ISteamUserStats_DownloadLeaderboardEntries((intptr_t)SteamUserStats(), currentLeaderboardHandle, k_ELeaderboardDataRequestGlobal, 0, 100);
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
		return -1;
	}
	return (int)elo;
}
void SteamManager::setLocalElo(int elo) {
	if (!m_bInitialized || !SteamUserStats()) return;
	if (elo <= 300) elo = 300;
	if (!SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "elo_rating", elo)) return;
	SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
	if (currentLeaderboardHandle != 0) {
		SteamAPI_ISteamUserStats_UploadLeaderboardScore((intptr_t)SteamUserStats(), currentLeaderboardHandle, 2, elo, nullptr, 0);
	}
}
void SteamManager::armLeaverBuster(int oppElo) {
	if (!m_bInitialized || !SteamUserStats()) return;
	if (!SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "leaver_opp_elo", oppElo)) return;
	SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
}
void SteamManager::disarmLeaverBuster() {
	if (!m_bInitialized || !SteamUserStats()) return;
	if (!SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "leaver_opp_elo", 0)) return;
	SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
}
int SteamManager::checkLeaverBuster() {
	if (!m_bInitialized || !SteamUserStats()) return 0;
	int32_t oppElo = 0;
	if (!SteamAPI_ISteamUserStats_GetStatInt32((intptr_t)SteamUserStats(), "leaver_opp_elo", &oppElo)) return 0;
	return (int)oppElo;
}
void SteamManager::updateRichPresence(const std::string & presenceText) {
	if (!m_bInitialized || !SteamAPI_IsSteamRunning() || !SteamFriends()) return;
	static char presenceBuffer[256];
	snprintf(presenceBuffer, sizeof(presenceBuffer), "%s", presenceText.c_str());
	SteamAPI_ISteamFriends_SetRichPresence((intptr_t)SteamFriends(), "status", presenceBuffer);
	SteamAPI_ISteamFriends_SetRichPresence((intptr_t)SteamFriends(), "steam_display", "#Status");
}
void SteamManager::onUserStatsReceived(UserStatsReceived_t * pCallback) {
	if (pCallback && pCallback->m_eResult == k_EResultOK) m_bStatsLoaded = true;
}

int SteamManager::getXPRequiredForLevel(int level) {
	if (level <= 0) level = 1;
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
	} catch (...) { }
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

	if (m_bInitialized && SteamUserStats()) {
		SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "player_xp", curXP);
		SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "player_level", curLvl);
		SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats());
	}

	saveLocalProgressionBackup(curXP, curLvl);
	return result;
}
void SteamManager::becomeHost() {
	m_bIsHost = true;
	g_isHostingLobby = true;
	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		SteamAPI_ISteamNetworkingSockets_CloseConnection((intptr_t)SteamNetworkingSockets(), m_hConnection, 0, nullptr, false);
		m_hConnection = k_HSteamNetConnection_Invalid;
	}
	if (m_hListenSocket == k_HSteamListenSocket_Invalid && SteamNetworkingSockets()) {
		m_hListenSocket = SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P((intptr_t)SteamNetworkingSockets(), 0, 0, nullptr);
	}
}
CSteamID SteamManager::getLobbyOwner() const {
	if (!m_bInitialized || !m_LobbyID.IsValid() || !SteamMatchmaking()) return CSteamID();
	return CSteamID((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
}

bool SteamManager::isLocalLobbyOwner() const {
	return getLobbyOwner() == m_LocalID;
}