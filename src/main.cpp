#include "ofApp.h"
#include "ofMain.h"

int main(int argc, char * argv[]) {
	// Check for checksum harness mode
	bool checksumHarness = false;
	const char * envHarness = getenv("MAGEFIGHT_CHECKSUM_HARNESS");
	if (envHarness && std::string(envHarness) == "1") checksumHarness = true;

	// Support command-line flags
	for (int i = 0; i < argc; ++i) {
		if (std::string(argv[i]) == "--checksum-harness") checksumHarness = true;
	}

	ofGLFWWindowSettings settings;
	settings.setGLVersion(3, 3);
	// Start windowed. ofApp::applySettings() will instantly snap it to the saved state (Fullscreen/Borderless/etc)
	settings.windowMode = OF_WINDOW;
	settings.setSize(1280, 720);
	settings.title = "Mage Fight";

	auto window = ofCreateWindow(settings);

	if (checksumHarness) {
		ofApp * app = new ofApp();
		app->setup();

		std::vector<std::string> candidates = { "autosave.json", "../Saves/autosave.json", "../autosave.json" };
		bool anyLoaded = false;
		for (const auto & path : candidates) {
			if (app->harnessLoadAndPrintChecksum(path)) {
				anyLoaded = true;
			}
		}

		const char * envScript = getenv("MAGEFIGHT_CHECKSUM_HARNESS_SCRIPT");
		if (envScript && std::string(envScript) == "1" && anyLoaded) {
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