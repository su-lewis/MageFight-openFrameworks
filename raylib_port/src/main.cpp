#include "GameApp.h"

#include "core/DeterministicHarness.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool IsHeadlessRequested(int argc, char * argv[]) {
	if (std::getenv("MAGEFIGHT_HEADLESS") != nullptr) {
		return true;
	}

	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "--headless") {
			return true;
		}
	}

	return false;
}

int RunHeadlessHarness() {
	std::cout << "Running deterministic core harness...\n";
	int baseRc = mf::core::RunDeterministicHarness();
	if (baseRc != 0) {
		return baseRc;
	}

	std::string snapshotPath;
	if (const char * envPath = std::getenv("MAGEFIGHT_SNAPSHOT_PATH")) {
		snapshotPath = envPath;
	}

	if (snapshotPath.empty()) {
		std::vector<std::string> candidates = {
			"Saves/autosave.json",
			"autosave.json",
			"../Saves/autosave.json",
			"bin/Saves/autosave.json",
		};
		for (const auto & candidate : candidates) {
			if (std::filesystem::exists(candidate)) {
				snapshotPath = candidate;
				break;
			}
		}
	}

	if (snapshotPath.empty()) {
		std::cerr << "No snapshot file found. Set MAGEFIGHT_SNAPSHOT_PATH or place autosave at Saves/autosave.json\n";
		return 8;
	}

	return mf::core::RunDeterministicSnapshotHarness(snapshotPath);
}

} // namespace

int main(int argc, char * argv[]) {
	if (IsHeadlessRequested(argc, argv)) {
		return RunHeadlessHarness();
	}

	GameApp app;
	if (!app.initialize(1280, 720, "Mage Fight (raylib port)")) {
		std::cerr << "Failed to initialize raylib app\n";
		return 1;
	}

	app.run();
	app.shutdown();
	return 0;
}
