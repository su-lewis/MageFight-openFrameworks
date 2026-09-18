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

		ofApp * app = new ofApp();
		ofRunApp(app);
		return 0;
	}

	// 3. Borderless Fullscreen Window Setup (Wayland/Bazzite Safe)
	ofGLFWWindowSettings settings;
	settings.setGLVersion(3, 3);
	// OF_FULLSCREEN defaults to a borderless window at native resolution
	// matching the primary monitor's refresh rate. No manual GLFW required.
	settings.windowMode = OF_FULLSCREEN;
	settings.title = "Mage Fight";

	auto window = ofCreateWindow(settings);

	// 4. Sound Setup
	ofSoundStreamSettings soundSettings;
	soundSettings.numOutputChannels = 2;
	soundSettings.numInputChannels = 0;
	soundSettings.sampleRate = 44100;
	soundSettings.bufferSize = 256;

	// Centralized cross-platform audio API selection
#if defined(_WIN32)
	soundSettings.setApi(ofSoundDevice::Api::MS_WASAPI);
#elif defined(__APPLE__)
	soundSettings.setApi(ofSoundDevice::Api::OSX_CORE);
#elif defined(__linux__)
	// Bazzite uses PipeWire. ALSA or UNSPECIFIED route perfectly through PipeWire.
	// Forcing PULSE here is known to crash RtAudio on PipeWire systems.
	soundSettings.setApi(ofSoundDevice::Api::UNSPECIFIED);
#else
	soundSettings.setApi(ofSoundDevice::Api::UNSPECIFIED);
#endif

	// NOTE: If audio still crashes, move ofSoundStreamSetup into ofApp::setup()
	ofSoundStreamSetup(soundSettings);

	// 5. Checksum Harness Logic
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

	// 6. Run Normal App
	auto app = std::make_shared<ofApp>();
	ofRunApp(window, app);
	return ofRunMainLoop();
}