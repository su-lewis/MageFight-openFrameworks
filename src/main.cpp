#include "ofApp.h"
#include "ofAppNoWindow.h" // Required for Headless Mode
#include "ofMain.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
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

	// 3. Query Primary Monitor Native Resolution and Max Refresh Rate (Hz)
	glfwInit();
	GLFWmonitor * primaryMonitor = glfwGetPrimaryMonitor();
	int screenWidth = 1920;
	int screenHeight = 1080;
	int monitorHz = 60; // Default fallback

	if (primaryMonitor) {
		const GLFWvidmode * mode = glfwGetVideoMode(primaryMonitor);
		if (mode) {
			screenWidth = mode->width;
			screenHeight = mode->height;
			monitorHz = mode->refreshRate;
		}
	}

	// 4. Borderless Fullscreen Window Setup
	ofGLFWWindowSettings settings;
	settings.setGLVersion(3, 3);
	settings.decorated = false; // Strips window borders / title bar for true borderless mode
	settings.windowMode = OF_WINDOW;
	settings.setPosition(glm::vec2(0, 0));
	settings.setSize(screenWidth, screenHeight);
	settings.title = "Mage Fight";
	auto window = ofCreateWindow(settings);

	// Cap max FPS strictly to the user's monitor refresh rate (Hz)
	ofSetFrameRate(monitorHz);

	// Sound Setup
	ofSoundStreamSettings soundSettings;
	soundSettings.numOutputChannels = 2;
	soundSettings.numInputChannels = 0;
	soundSettings.sampleRate = 44100;
	soundSettings.bufferSize = 256;

	// Centralized cross-platform audio API selection using ofSoundDevice::Api
#if defined(__linux__)
	// Native Linux builds use PulseAudio
	soundSettings.setApi(ofSoundDevice::Api::PULSE);
#elif defined(_WIN32)
	// Windows and CrossOver/Wine translation layers on macOS use WASAPI
	soundSettings.setApi(ofSoundDevice::Api::MS_WASAPI);
#elif defined(__APPLE__)
	// Native macOS builds use CoreAudio
	soundSettings.setApi(ofSoundDevice::Api::OSX_CORE);
#else
	// Fallback to openFrameworks default selection
	soundSettings.setApi(ofSoundDevice::Api::UNSPECIFIED);
#endif

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

	// Run Normal App
	auto app = std::make_shared<ofApp>();
	ofRunApp(window, app);
	return ofRunMainLoop();
}