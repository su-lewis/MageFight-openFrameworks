#pragma once
#pragma pack(push, 1)
#include "NetworkData.h"
#pragma pack(pop)
#include "ofMain.h"
#include "steam_api.h"
#include <queue>
#include <vector>

class SteamManager {
public:
	// --- NEW STRUCTS ---
	struct LobbyInfo {
		CSteamID lobbyID;
		std::string name;
		int numPlayers;
		int maxPlayers;
	};

	struct LeaderboardEntry {
		int rank;
		std::string name;
		int score;
	};

	SteamManager();
	~SteamManager();

	void setup();
	void update();

	void cleanup();
	void shutdownAPI();

	// Lobby / Match helpers
	void setMatchStarted();
	bool isMatchStarted() const;
	void setLobbySeed(uint32_t seed);
	uint32_t getLobbySeed() const;

	// Connection Logic
	void createLobby();
	void openFriendOverlay();
	void leaveLobby();

	// Data Logic
	bool sendPacket(const void * data, uint32_t size);

	// State Getters
	bool isHost() const;
	bool isConnected() const;
	bool hasOpponent() const;

	// Player Info
	std::string getLocalPlayerName() const;
	std::string getOpponentName() const;
	CSteamID getOpponentSteamID() const;
	CSteamID getLocalSteamID() const;
	bool getAvatarImage(const CSteamID & id, ofImage & outImage, int size = 64) const;

	// Connection Flags
	bool checkAndClearDisconnectFlag();
	bool checkAndClearReconnectFlag();
	bool opponentDisconnected = false;
	bool opponentReconnected = false;

	// Packet Queue for ofApp
	std::queue<std::vector<char>> packetQueue;

	// --- NEW PUBLIC FUNCTIONS ---
	void refreshLobbies();
	std::vector<LobbyInfo> getLobbyList();
	void joinLobbyByID(CSteamID lobbyID);

	void fetchLeaderboard();
	std::vector<LeaderboardEntry> getLeaderboardEntries();

	// Elo Getters/Setters
	int getLocalElo();
	void setLocalElo(int elo);

private:
	CSteamID m_OpponentID;
	uint32_t m_nextSeq = 1;
	bool m_bInitialized;
	bool m_bIsHost;

	// Steam Identifiers
	CSteamID m_LobbyID;
	CSteamID m_LocalID;

	// Networking API Handles
	HSteamListenSocket m_hListenSocket;
	HSteamNetConnection m_hConnection;

	std::vector<LobbyInfo> currentLobbies;
	std::vector<LeaderboardEntry> currentLeaderboard;
	SteamLeaderboard_t currentLeaderboardHandle;

	CCallResult<SteamManager, LobbyMatchList_t> m_LobbyMatchListCallResult;
	void OnLobbyMatchList(LobbyMatchList_t * pCallback, bool bIOFailure);

	CCallResult<SteamManager, LeaderboardFindResult_t> m_LeaderboardFindCallResult;
	void OnLeaderboardFindResult(LeaderboardFindResult_t * pCallback, bool bIOFailure);

	CCallResult<SteamManager, LeaderboardScoresDownloaded_t> m_LeaderboardScoresDownloadedCallResult;
	void OnLeaderboardScoresDownloaded(LeaderboardScoresDownloaded_t * pCallback, bool bIOFailure);

	// Callbacks
	STEAM_CALLBACK(SteamManager, OnLobbyEnter, LobbyEnter_t);
	STEAM_CALLBACK(SteamManager, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t);
	STEAM_CALLBACK(SteamManager, OnGameJoinRequested, GameRichPresenceJoinRequested_t);
	STEAM_CALLBACK(SteamManager, OnNetConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);

	// CallResults
	void OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure);
	CCallResult<SteamManager, LobbyCreated_t> m_cbLobbyCreated;

	void closeConnection();
};