#include "ofApp.h"
#include "ofMain.h"

int main() {

	// Check for checksum harness mode (no UI loop)
	bool checksumHarness = false;
	const char * envHarness = getenv("MAGEFIGHT_CHECKSUM_HARNESS");
	if (envHarness && std::string(envHarness) == "1") checksumHarness = true;

	// Also support command-line flag
	// (Note: argc/argv not available in this simplified main, use env var primarily)

	ofGLFWWindowSettings settings;
	// Request a programmable renderer / core profile for GLSL 150 shaders
	// Try a slightly newer core version if available.
	settings.setGLVersion(3, 3);
	settings.windowMode = OF_GAME_MODE;

	auto window = ofCreateWindow(settings);

	if (checksumHarness) {
		// In harness mode, instantiate the app, run setup, load autosave(s), print checksums, and exit.
		ofApp * app = new ofApp();
		app->setup();

		std::vector<std::string> candidates = { "autosave.json", "../Saves/autosave.json", "../autosave.json" };
		bool anyLoaded = false;
		for (const auto & path : candidates) {
			if (app->harnessLoadAndPrintChecksum(path)) {
				anyLoaded = true;
			}
		}

		// Optional scripted harness: auto-advance a few turns to exercise gameplay
		const char * envScript = getenv("MAGEFIGHT_CHECKSUM_HARNESS_SCRIPT");
		if (envScript && std::string(envScript) == "1" && anyLoaded) {
			// Default small script of 4 turns
			app->harnessAutoAdvanceTurns(4);
			long long chk = app->calculateChecksum();
			std::cout << "After scripted advances checksum=" << chk << std::endl;
		}
		if (!anyLoaded) {
			std::cout << "No autosave found; nothing loaded." << std::endl;
		}
		delete app;
		return 0;
	}

	ofRunApp(new ofApp());
}