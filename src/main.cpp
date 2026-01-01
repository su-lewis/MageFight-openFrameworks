#include "ofApp.h"
#include "ofMain.h"

int main() {
	ofGLFWWindowSettings settings;
	settings.windowMode = OF_GAME_MODE;

	auto window = ofCreateWindow(settings);
	ofRunApp(new ofApp());
}