#include "ofApp.h"
#include "ofAppNoWindow.h" // Required for Headless Mode
#include "ofMain.h"
#include <cstdlib>

int main(int argc, char * argv[]) {
	// 1. Check if we are running in Headless Mode
	bool headless = (std::getenv("MAGEFIGHT_HEADLESS") != nullptr);

	// 2. Setup based on mode
	if (headless) {
		ofAppNoWindow window;
		ofSetupOpenGL(&window, 1024, 768, OF_WINDOW);

		// 🟢 FIX: Use a raw pointer for headless app, not shared_ptr
		ofApp * app = new ofApp();
		ofRunApp(app);
		return 0;
	}

	// 3. Normal Mode Setup (Audio and Windows)
	ofGLFWWindowSettings settings;
	settings.setGLVersion(3, 3);
	settings.windowMode = OF_WINDOW;
	settings.setSize(1280, 720);
	settings.title = "Mage Fight";
	auto window = ofCreateWindow(settings);

	// Sound Setup
	ofSoundStreamSettings soundSettings;
	soundSettings.numOutputChannels = 2;
	soundSettings.numInputChannels = 0;
	soundSettings.sampleRate = 44100;
	soundSettings.bufferSize = 256;
	soundSettings.setApi(ofSoundDevice::Api::PULSE);
	ofSoundStreamSetup(soundSettings);

	// Checksum Harness Logic
	bool checksumHarness = false;
	const char * envHarness = getenv("MAGEFIGHT_CHECKSUM_HARNESS");
	if (envHarness && std::string(envHarness) == "1") checksumHarness = true;
	for (int i = 0; i < argc; ++i) {
		if (std::string(argv[i]) == "--checksum-harness") checksumHarness = true;
	}

	if (checksumHarness) {
		ofApp * app = new ofApp();
		app->setup();
		std::vector<std::string> candidates = { "autosave.json", "../Saves/autosave.json", "../autosave.json" };
		bool anyLoaded = false;
		for (const auto & path : candidates) {
			if (app->harnessLoadAndPrintChecksum(path)) anyLoaded = true;
		}
		const char * envScript = getenv("MAGEFIGHT_CHECKSUM_HARNESS_SCRIPT");
		if (envScript && std::string(envScript) == "1" && anyLoaded) {
			app->harnessAutoAdvanceTurns(4);
			long long chk = app->calculateChecksum();
			std::cout << "After scripted advances checksum=" << chk << std::endl;
		}
		delete app;
		return 0;
	}

	// 🟢 Run Normal App
	auto app = std::make_shared<ofApp>();
	ofRunApp(window, app);
	return ofRunMainLoop();
}