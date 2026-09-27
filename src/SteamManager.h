#pragma once
#pragma pack(push, 1)
#include "NetworkData.h"
#pragma pack(pop)
#include "ofMain.h"
#include "steam/steam_api.h"
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
	bool createLobby();
	void openFriendOverlay();
	void leaveLobby();
	bool hasLobbyBeenCreated() const { return m_lobbyCreationSucceeded; }
	bool hasLobbyCreationFailed() const { return m_lobbyCreationFailed; }
	void clearLobbyFlags() {
		m_lobbyCreationSucceeded = false;
		m_lobbyCreationFailed = false;
	}

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
	void becomeHost();
	CSteamID getLobbyID() const { return m_LobbyID; }
	CSteamID getLobbyOwner() const;
	bool isLocalLobbyOwner() const;

	// Elo Getters/Setters & Secure LeaverBuster
	int getLocalElo();
	void setLocalElo(int elo);
	bool areStatsLoaded() const { return m_bStatsLoaded; }
	void armLeaverBuster(int oppElo);
	void disarmLeaverBuster();
	int checkLeaverBuster();
	void updateRichPresence(const std::string & presenceText);

	// XP & Level Progression System
	struct XPGainResult {
		int xpEarned = 0;
		int oldXP = 0;
		int newXP = 0;
		int oldLevel = 1;
		int newLevel = 1;
		int xpRequiredForNext = 1000;
		bool leveledUp = false;
	};

	int getLocalXP();
	int getLocalLevel();
	int getXPRequiredForLevel(int level);
	XPGainResult addXP(int amount);

private:
	int m_cachedXP = -1;
	int m_cachedLevel = -1;
	void loadLocalProgressionBackup();
	void saveLocalProgressionBackup(int xp, int level);

	bool m_lobbyCreationSucceeded = false;
	bool m_lobbyCreationFailed = false;

	// ELO
	bool m_bStatsLoaded = false;

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

	// -------------------------------------------------------------
	// FLAT C CALLBACK TARGETS
	// All STEAM_CALLBACK and CCallResult macros have been removed.
	// These are now just standard C++ functions called by our Manual Dispatch router!
	// -------------------------------------------------------------
	void onUserStatsReceived(UserStatsReceived_t * pCallback);
	void OnGameLobbyJoinRequested(GameLobbyJoinRequested_t * pCallback);
	void OnGameJoinRequested(GameRichPresenceJoinRequested_t * pCallback);
	void OnNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t * pInfo);

	void OnLobbyMatchList(LobbyMatchList_t * pCallback, bool bIOFailure);
	void OnLeaderboardFindResult(LeaderboardFindResult_t * pCallback, bool bIOFailure);
	void OnLeaderboardScoresDownloaded(LeaderboardScoresDownloaded_t * pCallback, bool bIOFailure);
	void OnLobbyCreated(LobbyCreated_t * pCallback, bool bIOFailure);
	void OnLobbyEnter(LobbyEnter_t * pCallback, bool bIOFailure);

	void closeConnection();
};