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
uint64_t SteamAPI_ISteamMatchmaking_RequestLobbyList(intptr_t instancePtr);
uint64_t SteamAPI_ISteamMatchmaking_GetLobbyByIndex(intptr_t instancePtr, int iLobby);
int SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(intptr_t instancePtr, uint64_t steamIDLobby);
int SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit(intptr_t instancePtr, uint64_t steamIDLobby);

const char * SteamAPI_ISteamFriends_GetPersonaName(intptr_t instancePtr);
const char * SteamAPI_ISteamFriends_GetFriendPersonaName(intptr_t instancePtr, uint64_t steamIDFriend);
int SteamAPI_ISteamFriends_GetSmallFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
int SteamAPI_ISteamFriends_GetMediumFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
int SteamAPI_ISteamFriends_GetLargeFriendAvatar(intptr_t instancePtr, uint64_t steamIDFriend);
void SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog(intptr_t instancePtr, uint64_t steamIDLobby);

uint64_t SteamAPI_ISteamUserStats_FindLeaderboard(intptr_t instancePtr, const char * pchLeaderboardName);
uint64_t SteamAPI_ISteamUserStats_DownloadLeaderboardEntries(intptr_t instancePtr, uint64_t hSteamLeaderboard, int eLeaderboardDataRequest, int nRangeStart, int nRangeEnd);
bool SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry(intptr_t instancePtr, uint64_t hSteamLeaderboardEntries, int index, void * pLeaderboardEntry, int32_t * pDetails, int cDetailsMax);
bool SteamAPI_ISteamUserStats_GetStatInt32(intptr_t instancePtr, const char * pchName, int32_t * pData);
bool SteamAPI_ISteamUserStats_SetStatInt32(intptr_t instancePtr, const char * pchName, int32_t nData);
bool SteamAPI_ISteamUserStats_StoreStats(intptr_t instancePtr);
}

#ifdef _WIN32
	#pragma comment(lib, "steam_api64.lib")
	#include <windows.h>
#endif
#include <fstream>

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
	// --- BULLETPROOF STEAM APP ID FIX ---
	std::ofstream appidFile("steam_appid.txt");
	if (appidFile.is_open()) {
		appidFile << "480";
		appidFile.close();
	}

#ifdef _WIN32
	// _putenv doesn't always sync to the Win32 environment block which steam_api64.dll reads.
	// Force it using the native Windows API just to be absolutely certain.
	SetEnvironmentVariableA("SteamAppId", "480");
	SetEnvironmentVariableA("SteamGameId", "480");
#endif

	if (SteamAPI_Init()) {
		m_bInitialized = true;
		m_LocalID = CSteamID((uint64)SteamAPI_ISteamUser_GetSteamID((intptr_t)SteamUser()));

		// Initialize Sockets
		SteamNetworkingUtils()->InitRelayNetworkAccess();

		ofLogNotice("Steam") << "Initialized. LocalID: " << m_LocalID.ConvertToUint64();
	} else {
		ofLogError("Steam") << "Failed to init Steam API. Is Steam running?";
	}
}

bool SteamManager::isConnected() const {
	// This tells the UI if the Steam Client is running and the API successfully initialized.
	return m_bInitialized;
}

bool SteamManager::hasOpponent() const {
	// There is an opponent only when a direct connection is active.
	return m_hConnection != k_HSteamNetConnection_Invalid;
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

			// Verbose receive tracing for debugging cross-platform drops
			if (buffer.size() >= sizeof(PacketHeader)) {
				PacketHeader * ph = (PacketHeader *)buffer.data();
				ofLogNotice("NetTrace") << "RECV pkt type=" << (int)ph->type << " seq=" << ph->seq << " size=" << buffer.size();
				// If this is a draft action, dump decoded fields for cross-platform debugging
				if (ph->type == PKT_DRAFT_ACTION && buffer.size() >= sizeof(DraftActionPacket)) {
					DraftActionPacket * dap = (DraftActionPacket *)buffer.data();
					ofLogNotice("NetTrace") << "  DRAFT_ACTION recv: actionType=" << (int)dap->actionType
											<< " clientActionID=" << dap->clientActionID
											<< " draftPlayerIdx=" << dap->draftPlayerIdx
											<< " classTier=" << (int)dap->classTier
											<< " numSelected=" << (int)dap->numSelected
											<< " opt=" << dap->optionIndex
											<< " sel=" << (int)dap->selectFlag
											<< " pkt.playerID=" << dap->playerID;
				}
			} else {
				ofLogNotice("NetTrace") << "RECV raw size=" << buffer.size();
			}
			packetQueue.push(buffer);

			msg->Release();
		}
	}

	// Rely on SteamNetworkingSockets reliability for reliable sends.
}

void SteamManager::cleanup() {
	// LEAVE THE LOBBY, BUT DO NOT SHUTDOWN THE API HERE
	leaveLobby();

	// CLOSE CONNECTIONS
	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		SteamNetworkingSockets()->CloseListenSocket(m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}
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
	if (!m_bInitialized) {
		ofLogWarning("Steam") << "createLobby() called but Steam API not initialized.";
		return;
	}
	if (m_bIsHost) return; // Already hosting?

	ofLogNotice("Steam") << "Requesting Lobby Creation...";
	m_bIsHost = true;

	// Change to Public so it shows up in the Lobby Browser using Flat API
	SteamAPICall_t hSteamAPICall = SteamAPI_ISteamMatchmaking_CreateLobby((intptr_t)SteamMatchmaking(), k_ELobbyTypePublic, 2);
	m_cbLobbyCreated.Set(hSteamAPICall, this, &SteamManager::OnLobbyCreated);
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
		while (!packetQueue.empty())
			packetQueue.pop();
		return;
	}

	ISteamNetworkingSockets * net = SteamNetworkingSockets();

	if (m_hConnection != k_HSteamNetConnection_Invalid) {
		net->CloseConnection(m_hConnection, 0, "Closing", true); // Better		m_hConnection = k_HSteamNetConnection_Invalid;
	}

	if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
		net->CloseListenSocket(m_hListenSocket);
		m_hListenSocket = k_HSteamListenSocket_Invalid;
	}

	while (!packetQueue.empty())
		packetQueue.pop();
}

bool SteamManager::sendPacket(const void * data, uint32_t size) {
	if (!m_bInitialized) return false;

	// If we don't have a connection handle yet, attempt to establish one
	if (m_hConnection == k_HSteamNetConnection_Invalid) {
		// If we're a client and we have a valid lobby, try to connect to the lobby owner
		if (!m_bIsHost && m_LobbyID.IsValid() && SteamMatchmaking()) {
			CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));
			if (owner.IsValid()) {
				SteamNetworkingIdentity identity;
				identity.SetSteamID(owner);
				HSteamNetConnection conn = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, nullptr);
				if (conn != k_HSteamNetConnection_Invalid) {
					m_hConnection = conn;
					ofLogNotice("Steam") << "sendPacket: initiated ConnectP2P to host: " << owner.ConvertToUint64();
				} else {
					ofLogWarning("Steam") << "sendPacket: ConnectP2P returned invalid handle when trying to reach host";
				}
			}
		}
		// If still invalid, cannot send
		if (m_hConnection == k_HSteamNetConnection_Invalid) return false;
	}

	// Stamp sequence number on packets that have a header
	const PacketHeader * hdr = (const PacketHeader *)data;
	if (size >= sizeof(PacketHeader) && hdr->type <= PKT_ACK) {
		std::vector<char> buffer((const char *)data, (const char *)data + size);
		PacketHeader * outHdr = (PacketHeader *)buffer.data();
		if (outHdr->seq == 0) {
			outHdr->seq = m_nextSeq++;
		}

		// Verbose send tracing for canonical packets only (legacy async packets removed)
		if (outHdr->type == PKT_INPUT_COMMAND || outHdr->type == PKT_DRAFT_ACTION || outHdr->type == PKT_DRAFT_ACK || outHdr->type == PKT_CHECKSUM_CHECK || outHdr->type == PKT_SNAPSHOT_BEGIN || outHdr->type == PKT_SNAPSHOT_CHUNK || outHdr->type == PKT_SNAPSHOT_END || outHdr->type == PKT_MOVE_UNIT || outHdr->type == PKT_PLACE_SUMMONED_BEGIN) {
			ofLogNotice("NetTrace") << "SEND pkt type=" << (int)outHdr->type << " player=" << outHdr->playerID << " seq=" << outHdr->seq << " size=" << size;
			if (outHdr->type == PKT_INPUT_COMMAND && size >= sizeof(InputCommandPacket)) {
				InputCommandPacket * ic = (InputCommandPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  INPUT_CMD type=" << (int)ic->commandType << " clientActionID=" << ic->clientActionID << " cmdId=" << ic->commandId;
			} else if (outHdr->type == PKT_DRAFT_ACTION && size >= sizeof(DraftActionPacket)) {
				DraftActionPacket * dap = (DraftActionPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  DRAFT_ACTION actionType=" << (int)dap->actionType << " clientActionID=" << dap->clientActionID << " draftPlayerIdx=" << dap->draftPlayerIdx;
			} else if (outHdr->type == PKT_DRAFT_ACK && size >= sizeof(DraftAckPacket)) {
				DraftAckPacket * dak = (DraftAckPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  DRAFT_ACK clientActionID=" << dak->clientActionID << " actionType=" << (int)dak->actionType << " draftPlayer=" << dak->draftPlayerIdx;
			} else if (outHdr->type == PKT_CHECKSUM_CHECK && size >= sizeof(ChecksumPacket)) {
				ChecksumPacket * ckp = (ChecksumPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  CHECKSUM turn=" << ckp->turnNumber << " value=" << ckp->checksum;
			} else if (outHdr->type == PKT_SNAPSHOT_BEGIN && size >= sizeof(SnapshotBeginPacket)) {
				SnapshotBeginPacket * sb = (SnapshotBeginPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  SNAPSHOT_BEGIN id=" << sb->snapshotId << " totalSize=" << sb->totalSize;
			} else if (outHdr->type == PKT_SNAPSHOT_CHUNK && size >= sizeof(SnapshotChunkPacket)) {
				SnapshotChunkPacket * sc = (SnapshotChunkPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  SNAPSHOT_CHUNK id=" << sc->snapshotId << " offset=" << sc->offset << " chunkSize=" << sc->chunkSize;
			} else if (outHdr->type == PKT_SNAPSHOT_END && size >= sizeof(SnapshotEndPacket)) {
				SnapshotEndPacket * se = (SnapshotEndPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  SNAPSHOT_END id=" << se->snapshotId;
			} else if (outHdr->type == PKT_MOVE_UNIT && size >= sizeof(MoveUnitPacket)) {
				MoveUnitPacket * mup = (MoveUnitPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  MOVE from=(" << mup->fromX << "," << mup->fromY << ") to=(" << mup->toX << "," << mup->toY << ")";
			} else if (outHdr->type == PKT_PLACE_SUMMONED_BEGIN && size >= sizeof(PlaceSummonedBeginPacket)) {
				PlaceSummonedBeginPacket * psb = (PlaceSummonedBeginPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  PLACE_SUMMON_BEGIN minionType=" << (int)psb->minionType << " ownerID=" << psb->ownerPlayerID << " numToPlace=" << psb->numToPlace;
			}
		}
		EResult res = SteamNetworkingSockets()->SendMessageToConnection(
			m_hConnection, buffer.data(), size, k_nSteamNetworkingSend_Reliable, nullptr);
		if (res != k_EResultOK) {
			ofLogWarning("Steam") << "SendMessageToConnection failed (pktType=" << (int)outHdr->type << ") res=" << (int)res;
		}
		return (res == k_EResultOK);
	}

	// Use Reliable for game data
	EResult res = SteamNetworkingSockets()->SendMessageToConnection(
		m_hConnection, data, size, k_nSteamNetworkingSend_Reliable, nullptr);

	return (res == k_EResultOK);
}

bool SteamManager::isHost() const {
	return m_bIsHost;
}

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

CSteamID SteamManager::getOpponentSteamID() const {
	return m_OpponentID;
}

CSteamID SteamManager::getLocalSteamID() const {
	return m_LocalID;
}

bool SteamManager::getAvatarImage(const CSteamID & id, ofImage & outImage, int size) const {
	if (!m_bInitialized || !SteamFriends() || !SteamUtils() || !id.IsValid()) return false;

	int imageId = 0;
	if (size <= 32) {
		imageId = SteamAPI_ISteamFriends_GetSmallFriendAvatar((intptr_t)SteamFriends(), id.ConvertToUint64());
	} else if (size <= 64) {
		imageId = SteamAPI_ISteamFriends_GetMediumFriendAvatar((intptr_t)SteamFriends(), id.ConvertToUint64());
	} else {
		imageId = SteamAPI_ISteamFriends_GetLargeFriendAvatar((intptr_t)SteamFriends(), id.ConvertToUint64());
	}

	// -1 means not yet loaded; 0 means no avatar available
	if (imageId <= 0) return false;

	uint32_t width = 0;
	uint32_t height = 0;
	if (!SteamUtils()->GetImageSize(imageId, &width, &height) || width == 0 || height == 0) {
		return false;
	}

	ofPixels pixels;
	pixels.allocate(width, height, OF_PIXELS_RGBA);
	if (!SteamUtils()->GetImageRGBA(imageId, pixels.getData(), width * height * 4)) {
		return false;
	}

	outImage.setFromPixels(pixels);
	return true;
}

void SteamManager::openFriendOverlay() {
	if (m_bInitialized && m_LobbyID.IsValid() && SteamFriends()) {
		SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog((intptr_t)SteamFriends(), m_LobbyID.ConvertToUint64());
	}
}
// ---------------------------------------------------------
//  CALLBACKS
// ---------------------------------------------------------

void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure) {
	if (pCallback->m_eResult != k_EResultOK || bIOFailure) {
		ofLogError("Steam") << "Lobby Creation Failed. Result: " << pCallback->m_eResult;
		m_bIsHost = false;
		return;
	}

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	m_bIsHost = true;

	std::string lobbyName = std::string(SteamAPI_ISteamFriends_GetFriendPersonaName((intptr_t)SteamFriends(), m_LocalID.ConvertToUint64())) + "'s Game";
	SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "name", lobbyName.c_str());
	SteamAPI_ISteamMatchmaking_SetLobbyData((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64(), "MageFightLobby", "Active");

	ofLogNotice("Steam") << "Lobby Created. Creating Listen Socket...";

	// -- HOST LOGIC: OPEN LISTENING SOCKET --
	m_hListenSocket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, nullptr);
}

void SteamManager::OnLobbyEnter(LobbyEnter_t * pCallback) {
	if (pCallback->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) return;

	m_LobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
	CSteamID owner((uint64)SteamAPI_ISteamMatchmaking_GetLobbyOwner((intptr_t)SteamMatchmaking(), m_LobbyID.ConvertToUint64()));

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
			// Mark as disconnected but don't fully close - allow reconnection
			opponentDisconnected = true;
			m_hConnection = k_HSteamNetConnection_Invalid;
			ofLogNotice("Steam") << "Opponent disconnected - awaiting reconnection";
		}
		SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		break;

	case k_ESteamNetworkingConnectionState_Connecting: {
		ofLogNotice("Steam") << "Incoming Connection Request...";

		// SECURITY CHECK: Accept only if we are the host and listening
		if (m_hListenSocket != k_HSteamListenSocket_Invalid) {
			// NEW: Reject connection if we already have an active opponent
			if (m_hConnection != k_HSteamNetConnection_Invalid) {
				ofLogWarning("Steam") << "Rejecting 3rd party connection. Match is full.";
				SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			}
			// Otherwise, accept normally
			else if (SteamNetworkingSockets()->AcceptConnection(pInfo->m_hConn) == k_EResultOK) {
				m_hConnection = pInfo->m_hConn;
				ofLogNotice("Steam") << "Accepted Connection!";
			} else {
				SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			}
		}
		break;
	} // <-- Added closing brace

	case k_ESteamNetworkingConnectionState_Connected: { // <-- Added opening brace
		ofLogNotice("Steam") << "Connection Fully Active!";
		// Check if this is a reconnection
		bool wasDisconnected = (m_hConnection == k_HSteamNetConnection_Invalid && m_OpponentID.IsValid());
		m_hConnection = pInfo->m_hConn;

		// Store opponent Steam ID for name lookup
		if (pInfo->m_info.m_identityRemote.GetSteamID64() != 0) {
			m_OpponentID = CSteamID(pInfo->m_info.m_identityRemote.GetSteamID64());
			ofLogNotice("Steam") << "Opponent ID: " << m_OpponentID.ConvertToUint64();
		}

		// Set reconnection flag if opponent was previously disconnected
		if (wasDisconnected) {
			opponentReconnected = true;
			ofLogNotice("Steam") << "Opponent reconnected!";
		}
		break;
	} // <-- Added closing brace

	default:
		// Ignore other connection states (FindingRoute, FinWait, etc.)
		break;
	}
}

void SteamManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback) {
	// Re-added your original logic that got cut off
	SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), pCallback->m_steamIDLobby.ConvertToUint64());
}

void SteamManager::OnGameJoinRequested(GameRichPresenceJoinRequested_t * pCallback) {
	std::string cmd = pCallback->m_rgchConnect;
	size_t split = cmd.find(" ");
	if (split != std::string::npos) {
		CSteamID id(std::stoull(cmd.substr(split + 1)));
		SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), id.ConvertToUint64());
	}
}

// Match start / lobby-seed helpers
void SteamManager::setMatchStarted() {
	if (m_LobbyID.IsValid()) {
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
	if (m_LobbyID.IsValid()) {
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
		currentLobbies.push_back(info);
	}
}

std::vector<SteamManager::LobbyInfo> SteamManager::getLobbyList() {
	return currentLobbies;
}

void SteamManager::joinLobbyByID(CSteamID lobbyID) {
	if (!SteamMatchmaking()) return;
	SteamAPI_ISteamMatchmaking_JoinLobby((intptr_t)SteamMatchmaking(), lobbyID.ConvertToUint64());
}

void SteamManager::fetchLeaderboard() {
	if (!SteamUserStats()) return;
	SteamAPICall_t hSteamAPICall = SteamAPI_ISteamUserStats_FindLeaderboard((intptr_t)SteamUserStats(), "Global_Rankings");
	m_LeaderboardFindCallResult.Set(hSteamAPICall, this, &SteamManager::OnLeaderboardFindResult);
}

void SteamManager::OnLeaderboardFindResult(LeaderboardFindResult_t * pCallback, bool bIOFailure) {
	if (!bIOFailure && pCallback->m_bLeaderboardFound) {
		currentLeaderboardHandle = pCallback->m_hSteamLeaderboard;
		SteamAPICall_t hSteamAPICall = SteamAPI_ISteamUserStats_DownloadLeaderboardEntries(
			(intptr_t)SteamUserStats(), currentLeaderboardHandle, k_ELeaderboardDataRequestGlobal, 0, 10);
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

std::vector<SteamManager::LeaderboardEntry> SteamManager::getLeaderboardEntries() {
	return currentLeaderboard;
}

int SteamManager::getLocalElo() {
	if (!SteamUserStats()) return 1000;
	int32_t elo = 1000;
	SteamAPI_ISteamUserStats_GetStatInt32((intptr_t)SteamUserStats(), "elo_rating", &elo);
	return (int)elo;
}

void SteamManager::setLocalElo(int elo) {
	if (!SteamUserStats()) return;
	SteamAPI_ISteamUserStats_SetStatInt32((intptr_t)SteamUserStats(), "elo_rating", elo);
	SteamAPI_ISteamUserStats_StoreStats((intptr_t)SteamUserStats()); // Uploads immediately to Steam
}