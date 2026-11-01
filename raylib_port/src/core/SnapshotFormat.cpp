#include "core/SnapshotFormat.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mf::core {
namespace {

struct CardMeta {
	std::string typeName;
	int defaultCost = 0;
	int value = 0;
};

std::string ReadTextFile(const std::string & path, std::string & outError) {
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) {
		outError = "Failed to open file: " + path;
		return {};
	}

	std::ostringstream ss;
	ss << ifs.rdbuf();
	return ss.str();
}

std::vector<std::string> SplitTabs(const std::string & line) {
	std::vector<std::string> out;
	std::string current;
	for (char c : line) {
		if (c == '\t') {
			out.push_back(current);
			current.clear();
		} else {
			current.push_back(c);
		}
	}
	out.push_back(current);
	return out;
}

std::string UnescapeField(const std::string & input) {
	std::string out;
	out.reserve(input.size());
	bool esc = false;
	for (char c : input) {
		if (!esc) {
			if (c == '\\') {
				esc = true;
			} else {
				out.push_back(c);
			}
		} else {
			switch (c) {
			case '\\':
				out.push_back('\\');
				break;
			case 't':
				out.push_back('\t');
				break;
			case 'n':
				out.push_back('\n');
				break;
			case 'c':
				out.push_back(',');
				break;
			default:
				out.push_back(c);
				break;
			}
			esc = false;
		}
	}
	return out;
}

std::vector<std::string> SplitEscapedList(const std::string & input) {
	std::vector<std::string> out;
	std::string current;
	bool esc = false;
	for (char c : input) {
		if (!esc) {
			if (c == '\\') {
				esc = true;
			} else if (c == ',') {
				out.push_back(UnescapeField(current));
				current.clear();
			} else {
				current.push_back(c);
			}
		} else {
			current.push_back('\\');
			current.push_back(c);
			esc = false;
		}
	}

	if (!current.empty() || input.empty()) {
		out.push_back(UnescapeField(current));
	}

	return out;
}

std::string RemoveLineComment(const std::string & line) {
	const std::size_t pos = line.find("//");
	if (pos == std::string::npos) {
		return line;
	}
	return line.substr(0, pos);
}

std::string Trim(const std::string & s) {
	std::size_t b = 0;
	while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])) != 0) {
		++b;
	}

	std::size_t e = s.size();
	while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0) {
		--e;
	}

	return s.substr(b, e - b);
}

bool ParseCardTypeEnumMap(const std::string & ofAppHeaderText, std::unordered_map<std::string, int> & outTypeMap, std::string & outError) {
	const std::string marker = "enum CardType";
	const std::size_t markerPos = ofAppHeaderText.find(marker);
	if (markerPos == std::string::npos) {
		outError = "enum CardType not found in ofApp header";
		return false;
	}

	const std::size_t braceOpen = ofAppHeaderText.find('{', markerPos);
	const std::size_t braceClose = ofAppHeaderText.find("};", braceOpen);
	if (braceOpen == std::string::npos || braceClose == std::string::npos || braceClose <= braceOpen) {
		outError = "enum CardType block malformed in ofApp header";
		return false;
	}

	std::string body = ofAppHeaderText.substr(braceOpen + 1, braceClose - braceOpen - 1);
	std::replace(body.begin(), body.end(), '\n', ',');
	std::replace(body.begin(), body.end(), '\r', ',');

	std::stringstream ss(body);
	std::string token;
	int currentValue = 0;

	while (std::getline(ss, token, ',')) {
		token = Trim(RemoveLineComment(token));
		if (token.empty()) {
			continue;
		}

		std::size_t eqPos = token.find('=');
		std::string name = token;
		if (eqPos != std::string::npos) {
			name = Trim(token.substr(0, eqPos));
			std::string rhs = Trim(token.substr(eqPos + 1));
			if (!rhs.empty()) {
				try {
					currentValue = std::stoi(rhs);
				} catch (...) {
					outError = "Failed to parse CardType enum assignment: " + token;
					return false;
				}
			}
		}

		if (name.empty()) {
			continue;
		}

		if (name.rfind("CARD_", 0) == 0 || name.rfind("PSEUDO_", 0) == 0) {
			outTypeMap[name] = currentValue;
			++currentValue;
		}
	}

	if (outTypeMap.empty()) {
		outError = "CardType enum parsed but no CARD_* entries were found";
		return false;
	}

	return true;
}

std::vector<std::string> ExtractTopLevelJsonObjects(const std::string & jsonText) {
	std::vector<std::string> objects;
	bool inString = false;
	bool escape = false;
	int depth = 0;
	std::size_t currentStart = std::string::npos;

	for (std::size_t i = 0; i < jsonText.size(); ++i) {
		char c = jsonText[i];

		if (inString) {
			if (escape) {
				escape = false;
			} else if (c == '\\') {
				escape = true;
			} else if (c == '"') {
				inString = false;
			}
			continue;
		}

		if (c == '"') {
			inString = true;
			continue;
		}

		if (c == '{') {
			++depth;
			if (depth == 1) {
				currentStart = i;
			}
		} else if (c == '}') {
			if (depth == 1 && currentStart != std::string::npos) {
				objects.push_back(jsonText.substr(currentStart, i - currentStart + 1));
				currentStart = std::string::npos;
			}
			--depth;
		}
	}

	return objects;
}

std::string ExtractJsonString(const std::string & obj, const std::string & key) {
	std::regex re("\\\"" + key + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"])*)\\\"");
	std::smatch m;
	if (!std::regex_search(obj, m, re)) {
		return {};
	}

	std::string raw = m[1].str();
	std::string decoded;
	decoded.reserve(raw.size());

	bool esc = false;
	for (char c : raw) {
		if (!esc) {
			if (c == '\\') {
				esc = true;
			} else {
				decoded.push_back(c);
			}
		} else {
			switch (c) {
			case 'n':
				decoded.push_back('\n');
				break;
			case 't':
				decoded.push_back('\t');
				break;
			case 'r':
				decoded.push_back('\r');
				break;
			case '\\':
				decoded.push_back('\\');
				break;
			case '"':
				decoded.push_back('"');
				break;
			default:
				decoded.push_back(c);
				break;
			}
			esc = false;
		}
	}

	return decoded;
}

int ExtractJsonInt(const std::string & obj, const std::string & key, int fallback) {
	std::regex re("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+)");
	std::smatch m;
	if (!std::regex_search(obj, m, re)) {
		return fallback;
	}

	try {
		return std::stoi(m[1].str());
	} catch (...) {
		return fallback;
	}
}

bool ParseCardCatalog(const std::string & cardsJsonText, std::unordered_map<std::string, CardMeta> & outByName) {
	const std::vector<std::string> objects = ExtractTopLevelJsonObjects(cardsJsonText);
	for (const std::string & obj : objects) {
		const std::string name = ExtractJsonString(obj, "name");
		const std::string type = ExtractJsonString(obj, "type");
		if (name.empty() || type.empty()) {
			continue;
		}

		CardMeta meta;
		meta.typeName = type;
		meta.defaultCost = ExtractJsonInt(obj, "cost", 0);
		meta.value = ExtractJsonInt(obj, "value", 0);

		outByName[name] = meta;
	}

	return !outByName.empty();
}

CardState BuildCardStateFromSnapshotToken(
	const std::string & token,
	const std::unordered_map<std::string, CardMeta> & cardByName,
	const std::unordered_map<std::string, int> & cardTypeMap) {

	std::string name = token;
	int overrideCost = -1;

	const std::size_t pipePos = token.find('|');
	if (pipePos != std::string::npos) {
		name = token.substr(0, pipePos);
		try {
			overrideCost = std::stoi(token.substr(pipePos + 1));
		} catch (...) {
			overrideCost = -1;
		}
	}

	CardState out;

	auto itCard = cardByName.find(name);
	if (itCard != cardByName.end()) {
		const CardMeta & meta = itCard->second;
		out.value = meta.value;

		auto itType = cardTypeMap.find(meta.typeName);
		out.type = (itType != cardTypeMap.end()) ? itType->second : 0;

		out.cost = (overrideCost >= 0) ? overrideCost : meta.defaultCost;
	} else {
		out.type = 0;
		out.value = 0;
		out.cost = (overrideCost >= 0) ? overrideCost : 0;
	}

	return out;
}

void DecodeCardList(
	const std::string & field,
	const std::unordered_map<std::string, CardMeta> & cardByName,
	const std::unordered_map<std::string, int> & cardTypeMap,
	std::vector<CardState> & outCards) {

	outCards.clear();
	if (field.empty()) {
		return;
	}

	const std::vector<std::string> tokens = SplitEscapedList(field);
	for (const std::string & tok : tokens) {
		if (tok.empty()) {
			continue;
		}
		outCards.push_back(BuildCardStateFromSnapshotToken(tok, cardByName, cardTypeMap));
	}
}

bool ParsePlayerLine(
	const std::vector<std::string> & parts,
	const std::unordered_map<std::string, CardMeta> & cardByName,
	const std::unordered_map<std::string, int> & cardTypeMap,
	PlayerState & outPlayer,
	std::string & outError) {

	if (parts.size() < 45) {
		outError = "Malformed player line: expected at least 45 fields";
		return false;
	}

	auto readInt = [&](int & idx, int fallback = 0) -> int {
		if (idx >= static_cast<int>(parts.size())) {
			return fallback;
		}
		try {
			return std::stoi(parts[idx++]);
		} catch (...) {
			idx++;
			return fallback;
		}
	};

	auto readBool = [&](int & idx) -> bool {
		return readInt(idx, 0) != 0;
	};

	int idx = 1;

	outPlayer.playerID = readInt(idx);
	outPlayer.x = readInt(idx);
	outPlayer.y = readInt(idx);
	outPlayer.health = readInt(idx);
	outPlayer.maxHealth = readInt(idx);
	outPlayer.block = readInt(idx);
	outPlayer.ward = readInt(idx);
	outPlayer.fortification = readInt(idx);
	outPlayer.barrier = readInt(idx);
	outPlayer.holyBlock = readInt(idx);
	outPlayer.luck = readInt(idx);

	(void)readInt(idx); // baseLuck
	outPlayer.bonusTurns = readInt(idx);
	(void)readInt(idx); // facingAngle serialized as float in OF; checksum ignores it

	outPlayer.onFire = readBool(idx);
	(void)readBool(idx); // hasRegeneration
	outPlayer.nextTurnAPBonus = readInt(idx);
	(void)readInt(idx); // shocksPlayedThisTurn
	(void)readInt(idx); // flurryOfFistsStacks
	outPlayer.freeHandCardTurns = readInt(idx);
	outPlayer.isParalyzed = readBool(idx);
	outPlayer.paralysisHeadsCount = readInt(idx);
	outPlayer.isPoisoned = readBool(idx);
	outPlayer.poisonReduction = readInt(idx);
	outPlayer.nextAttackAddPoison = readBool(idx);
	outPlayer.nextTurnD10AP = readBool(idx);
	outPlayer.nextTurnExtraDraw = readBool(idx);
	outPlayer.replicateQueued = readBool(idx);
	outPlayer.nextTurnBonusDiceFromMinions = readBool(idx);
	outPlayer.strengthenElementsTurnsRemaining = readInt(idx);
	outPlayer.sleepTurnsRemaining = readInt(idx);
	outPlayer.summonedOnTurnCycle = readInt(idx);
	outPlayer.summonOrder = readInt(idx);

	outPlayer.isMinion = readBool(idx);
	(void)readBool(idx); // isSkeleton
	(void)readBool(idx); // isGolem
	(void)readBool(idx); // isHellhound
	(void)readBool(idx); // isWolf
	(void)readBool(idx); // isKobold
	(void)readBool(idx); // isDemon
	(void)readBool(idx); // isWallUnit
	(void)readBool(idx); // isMagicWallUnit
	(void)readBool(idx); // isKoboldKing
	(void)readBool(idx); // isFaerie
	(void)readBool(idx); // isAssistant
	(void)readInt(idx); // directSummonerID
	(void)readBool(idx); // assistantRerollUsedThisTurn
	outPlayer.freeKickTurns = readInt(idx);
	outPlayer.inTortoiseForm = readBool(idx);
	outPlayer.tortoiseDamageTaken = readInt(idx);
	(void)readInt(idx); // storedDarkShieldDice
	outPlayer.ownerID = readInt(idx);
	outPlayer.inGhostForm = readBool(idx);
	outPlayer.ghostDamageTaken = readInt(idx);

	if (idx < static_cast<int>(parts.size()) && parts[idx] != "DECK") {
		++idx; // originalModelType
	}

	if (idx < static_cast<int>(parts.size()) && parts[idx] != "DECK") {
		++idx; // nextTurnExtraDrawSetOnCycle
	}

	if (idx < static_cast<int>(parts.size()) && parts[idx] != "DECK") {
		++idx; // fireApplierPlayerID
	}

	if (idx < static_cast<int>(parts.size()) && parts[idx] != "DECK") {
		++idx; // poisonApplierPlayerID
	}

	if (idx < static_cast<int>(parts.size()) && parts[idx] != "DECK") {
		outPlayer.defenseCycle = readInt(idx);
	}

	if (idx < static_cast<int>(parts.size()) && parts[idx] != "DECK") {
		outPlayer.ap = readInt(idx);
	}

	if (idx < static_cast<int>(parts.size()) && parts[idx] != "DECK") {
		++idx; // hasDrawnThisTurn
	}

	while (idx + 1 < static_cast<int>(parts.size())) {
		const std::string tag = parts[idx++];
		const std::string value = parts[idx++];

		if (tag == "DECK") {
			DecodeCardList(value, cardByName, cardTypeMap, outPlayer.deck);
		} else if (tag == "HAND") {
			DecodeCardList(value, cardByName, cardTypeMap, outPlayer.hand);
		} else if (tag == "DISCARD") {
			DecodeCardList(value, cardByName, cardTypeMap, outPlayer.discardPile);
		} else if (tag == "PLAYED") {
			DecodeCardList(value, cardByName, cardTypeMap, outPlayer.playedCardsPile);
		} else if (tag == "PLAYEDTYPES") {
			outPlayer.cardsPlayedThisTurn.clear();
			if (!value.empty()) {
				const std::vector<std::string> values = SplitEscapedList(value);
				for (const std::string & v : values) {
					if (v.empty()) {
						continue;
					}
					try {
						outPlayer.cardsPlayedThisTurn.push_back(std::stoi(v));
					} catch (...) {
					}
				}
			}
		}
	}

	return true;
}

} // namespace

bool LoadDeterministicSnapshot(const SnapshotLoadPaths & paths, DeterministicSnapshot & outSnapshot, std::string & outError) {
	std::string cardsText = ReadTextFile(paths.cardsJsonPath, outError);
	if (cardsText.empty()) {
		return false;
	}

	std::string ofAppHeaderText = ReadTextFile(paths.ofAppHeaderPath, outError);
	if (ofAppHeaderText.empty()) {
		return false;
	}

	std::unordered_map<std::string, int> cardTypeMap;
	if (!ParseCardTypeEnumMap(ofAppHeaderText, cardTypeMap, outError)) {
		return false;
	}

	std::unordered_map<std::string, CardMeta> cardByName;
	if (!ParseCardCatalog(cardsText, cardByName)) {
		outError = "Failed to parse cards catalog from " + paths.cardsJsonPath;
		return false;
	}

	std::string snapshotText = ReadTextFile(paths.snapshotPath, outError);
	if (snapshotText.empty()) {
		return false;
	}

	DeterministicSnapshot snapshot;
	std::istringstream ss(snapshotText);
	std::string line;

	while (std::getline(ss, line)) {
		if (line.empty()) {
			continue;
		}

		std::vector<std::string> parts = SplitTabs(line);
		if (parts.empty()) {
			continue;
		}

		if (parts[0] == "RNGPOS" && parts.size() >= 2) {
			try {
				snapshot.gameplayRngAdvanceCount = static_cast<std::uint64_t>(std::stoull(parts[1]));
			} catch (...) {
			}
		} else if (parts[0] == "STATE" && parts.size() >= 12) {
			auto safeToInt = [](const std::string & s, int fallback) -> int {
				try {
					return std::stoi(s);
				} catch (...) {
					return fallback;
				}
			};

			snapshot.syncedState = safeToInt(parts[1], snapshot.syncedState);
			snapshot.currentPlayerIndex = safeToInt(parts[2], snapshot.currentPlayerIndex);
			snapshot.globalTurnCounter = safeToInt(parts[3], snapshot.globalTurnCounter);
			snapshot.isInGameDraft = safeToInt(parts[4], 0) != 0;
			snapshot.draftStage = safeToInt(parts[5], snapshot.draftStage);
			snapshot.draftPlayerIndex = safeToInt(parts[6], snapshot.draftPlayerIndex);
			snapshot.draftPicksRemaining = safeToInt(parts[7], snapshot.draftPicksRemaining);
			snapshot.currentDraftClassTier = safeToInt(parts[8], snapshot.currentDraftClassTier);
			snapshot.currentAP = safeToInt(parts[11], snapshot.currentAP);

			if (parts.size() > 16) {
				snapshot.nextSummonOrder = safeToInt(parts[16], snapshot.nextSummonOrder);
			}

			if (parts.size() > 25) {
				snapshot.initialDraftComplete = safeToInt(parts[25], 0) != 0;
			}
		} else if (parts[0] == "QUEUE" && parts.size() >= 2) {
			snapshot.draftQueue.clear();
			for (std::size_t i = 2; i < parts.size(); ++i) {
				try {
					snapshot.draftQueue.push_back(std::stoi(parts[i]));
				} catch (...) {
				}
			}
		} else if (parts[0] == "BOARD" && parts.size() >= 3) {
			const std::string walls = parts[1];
			const std::string magicWalls = parts[2];

			int index = 0;
			for (int y = 0; y < DeterministicSnapshot::kBoardHeight; ++y) {
				for (int x = 0; x < DeterministicSnapshot::kBoardWidth; ++x) {
					snapshot.board[x][y].hasWall = (index < static_cast<int>(walls.size()) && walls[index] == '1');
					snapshot.board[x][y].isMagicWall = (index < static_cast<int>(magicWalls.size()) && magicWalls[index] == '1');
					snapshot.board[x][y].hasPlayer = false;
					++index;
				}
			}
		} else if (parts[0] == "P") {
			PlayerState player;
			if (!ParsePlayerLine(parts, cardByName, cardTypeMap, player, outError)) {
				return false;
			}
			snapshot.players.push_back(player);
		} else if (parts[0] == "GRAVE" && parts.size() >= 5) {
			GraveyardState grave;
			try {
				grave.x = std::stoi(parts[1]);
				grave.y = std::stoi(parts[2]);
			} catch (...) {
				grave.x = 0;
				grave.y = 0;
			}

			const std::vector<std::string> tokens = SplitEscapedList(parts[4]);
			grave.deckSize = tokens.size();
			snapshot.graveyard.push_back(grave);
		} else if (parts[0] == "KEY" && parts.size() >= 4) {
			FloatingKeyState key;
			try {
				key.x = std::stoi(parts[1]);
				key.y = std::stoi(parts[2]);
				key.set = std::stoi(parts[3]);
			} catch (...) {
				key = {};
			}
			snapshot.floatingKeys.push_back(key);
		}
	}

	for (const PlayerState & player : snapshot.players) {
		if (player.x >= 0 && player.x < DeterministicSnapshot::kBoardWidth && player.y >= 0 && player.y < DeterministicSnapshot::kBoardHeight) {
			snapshot.board[player.x][player.y].hasPlayer = true;
		}
	}

	outSnapshot = std::move(snapshot);
	return true;
}

} // namespace mf::core
