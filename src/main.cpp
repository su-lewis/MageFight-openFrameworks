#include "ofApp.h"
#include "ofMain.h"

int main() {
	ofGLFWWindowSettings settings;
	// Request a programmable renderer / core profile for GLSL 150 shaders
	// Try a slightly newer core version if available.
	settings.setGLVersion(3, 3);
	settings.windowMode = OF_GAME_MODE;

	auto window = ofCreateWindow(settings);

	ofRunApp(new ofApp());
}