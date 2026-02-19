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

bool SteamManager::isConnected() const {
	// Consider us "connected" for UI purposes if we are hosting (lobby created)
	// or if we have an active peer connection.
	return m_bIsHost || m_LobbyID.IsValid() || (m_hConnection != k_HSteamNetConnection_Invalid);
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
	// Optimistically mark as host so UI updates immediately. If creation fails, we'll clear it.
	m_bIsHost = true;
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
	// If we don't have a connection handle yet, attempt to establish one
	// (clients may click during lobby join before the P2P connection becomes fully active).
	if (m_hConnection == k_HSteamNetConnection_Invalid) {
		// If we're a client and we have a valid lobby, try to connect to the lobby owner
		if (!m_bIsHost && m_LobbyID.IsValid() && SteamMatchmaking()) {
			CSteamID owner = SteamMatchmaking()->GetLobbyOwner(m_LobbyID);
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

		// Verbose send tracing for key packets
		if (outHdr->type == PKT_ACTION || outHdr->type == PKT_DRAFT_ACTION || outHdr->type == PKT_DRAFT_ACK || outHdr->type == PKT_RENEWED_INSPIRATION || outHdr->type == PKT_DRAW_CARDS || outHdr->type == PKT_SHUFFLE || outHdr->type == PKT_TURN_START || outHdr->type == PKT_PLACE_SUMMONED_MINION || outHdr->type == PKT_DICE_ROLL || outHdr->type == PKT_CHECKSUM_CHECK || outHdr->type == PKT_SNAPSHOT_BEGIN || outHdr->type == PKT_SNAPSHOT_CHUNK || outHdr->type == PKT_SNAPSHOT_END || outHdr->type == PKT_MOVE_UNIT || outHdr->type == PKT_AMNESIA_CHOICE || outHdr->type == PKT_PLACE_SUMMONED_BEGIN) {
			ofLogNotice("NetTrace") << "SEND pkt type=" << (int)outHdr->type << " player=" << outHdr->playerID << " seq=" << outHdr->seq << " size=" << size;
			if (outHdr->type == PKT_DRAFT_ACTION && size >= sizeof(DraftActionPacket)) {
				DraftActionPacket * dap = (DraftActionPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  DRAFT_ACTION actionType=" << (int)dap->actionType << " clientActionID=" << dap->clientActionID << " draftPlayerIdx=" << dap->draftPlayerIdx << " classTier=" << (int)dap->classTier << " numSelected=" << (int)dap->numSelected << " opt=" << dap->optionIndex << " sel=" << (int)dap->selectFlag;
			} else if (outHdr->type == PKT_DRAFT_ACK && size >= sizeof(DraftAckPacket)) {
				DraftAckPacket * dak = (DraftAckPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  DRAFT_ACK clientActionID=" << dak->clientActionID << " actionType=" << (int)dak->actionType << " draftPlayer=" << dak->draftPlayerIdx << " opt=" << dak->optionIndex << " sel=" << (int)dak->selectFlag;
			}
			if (outHdr->type == PKT_ACTION && size >= sizeof(ActionPacket)) {
				ActionPacket * ap = (ActionPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  ACTION card='" << ap->cardName << "' actor=" << ap->actorIndex << " target=(" << ap->targetX << "," << ap->targetY << ") menu=" << ap->menuChoice << " updatedAP=" << ap->updatedAP;
			} else if (outHdr->type == PKT_SHUFFLE && size >= sizeof(ShufflePacket)) {
				ShufflePacket * spk = (ShufflePacket *)buffer.data();
				ofLogNotice("NetTrace") << "  SHUFFLE playerIndex=" << spk->playerIndex << " nonce=" << spk->nonce;
			} else if (outHdr->type == PKT_TURN_START && size >= sizeof(TurnStartPacket)) {
				TurnStartPacket * tsp = (TurnStartPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  TURN_START currentPlayerIndex=" << tsp->currentPlayerIndex << " diceNum=" << (int)tsp->diceNum << " diceSides=" << (int)tsp->diceSides << " finalTotal=" << tsp->finalTotal;
			} else if (outHdr->type == PKT_DRAW_CARDS && size >= sizeof(DrawCardsPacket)) {
				DrawCardsPacket * dcp = (DrawCardsPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  DRAW_CARDS playerIndex=" << dcp->playerIndex << " numCards=" << (int)dcp->numCards;
			} else if (outHdr->type == PKT_RENEWED_INSPIRATION && size >= sizeof(RenewedInspirationPacket)) {
				RenewedInspirationPacket * rip = (RenewedInspirationPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  RINSP playerIndex=" << rip->playerIndex << " count=" << (int)rip->count;
			} else if (outHdr->type == PKT_PLACE_SUMMONED_MINION && size >= sizeof(PlaceSummonedMinionPacket)) {
				PlaceSummonedMinionPacket * psp = (PlaceSummonedMinionPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  PLACE_SUMMONED minionType=" << (int)psp->minionType << " ownerID=" << psp->ownerPlayerID << " target=(" << psp->targetX << "," << psp->targetY << ") HP=" << psp->minionHP << " AP=" << psp->minionAP;
			} else if (outHdr->type == PKT_DICE_ROLL && size >= sizeof(DiceRollPacket)) {
				DiceRollPacket * drp = (DiceRollPacket *)buffer.data();
				ofLogNotice("NetTrace") << "  DICE_ROLL owner=" << drp->ownerIndex << " numDice=" << (int)drp->numDice << " sides=" << (int)drp->sides;
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
			} else if (outHdr->type == PKT_AMNESIA_CHOICE && size >= sizeof(AmnesiaChoicePacket)) {
				AmnesiaChoicePacket * apc = (AmnesiaChoicePacket *)buffer.data();
				ofLogNotice("NetTrace") << "  AMNESIA targetPlayer=" << apc->targetPlayerIndex << " numRemove=" << (int)apc->numCardsToRemove;
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
	const char * name = SteamFriends()->GetPersonaName();
	return name ? std::string(name) : "Player";
}

std::string SteamManager::getOpponentName() const {
	if (!m_bInitialized || !SteamFriends() || !m_OpponentID.IsValid()) return "Opponent";
	const char * name = SteamFriends()->GetFriendPersonaName(m_OpponentID);
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
		imageId = SteamFriends()->GetSmallFriendAvatar(id);
	} else if (size <= 64) {
		imageId = SteamFriends()->GetMediumFriendAvatar(id);
	} else {
		imageId = SteamFriends()->GetLargeFriendAvatar(id);
	}

	// -1 means not yet loaded; 0 means no avatar available
	if (imageId <= 0) return false;

	uint32 width = 0;
	uint32 height = 0;
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
	if (m_bInitialized) SteamFriends()->ActivateGameOverlay("LobbyInvite");
}

// ---------------------------------------------------------
//  CALLBACKS
// ---------------------------------------------------------

// FIX: Standard function, NOT a STEAM_CALLBACK macro
void SteamManager::OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure) {
	if (pCallback->m_eResult != k_EResultOK || bIOFailure) {
		ofLogError("Steam") << "Lobby Creation Failed. Result: " << pCallback->m_eResult;
		// Clear optimistic host flag on failure
		m_bIsHost = false;
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
			// Mark as disconnected but don't fully close - allow reconnection
			opponentDisconnected = true;
			m_hConnection = k_HSteamNetConnection_Invalid;
			ofLogNotice("Steam") << "Opponent disconnected - awaiting reconnection";
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

// Match start / lobby-seed helpers
void SteamManager::setMatchStarted() {
	if (m_LobbyID.IsValid()) {
		SteamMatchmaking()->SetLobbyData(m_LobbyID, "match_started", "1");
	}
}

bool SteamManager::isMatchStarted() const {
	if (!m_LobbyID.IsValid()) return false;
	const char * val = SteamMatchmaking()->GetLobbyData(m_LobbyID, "match_started");
	if (!val) return false;
	return std::string(val) == "1";
}

void SteamManager::setLobbySeed(uint32_t seed) {
	if (m_LobbyID.IsValid()) {
		SteamMatchmaking()->SetLobbyData(m_LobbyID, "seed", std::to_string(seed).c_str());
	}
}

uint32_t SteamManager::getLobbySeed() const {
	if (!m_LobbyID.IsValid()) return 0;
	const char * v = SteamMatchmaking()->GetLobbyData(m_LobbyID, "seed");
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