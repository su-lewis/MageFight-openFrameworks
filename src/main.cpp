#include "ofMain.h"
#include "ofApp.h"

int main( ){
    // Start as a small window to ensure the app initializes safely.
    // We will switch to Fullscreen immediately in setup().
	ofSetupOpenGL(1024, 768, OF_WINDOW);
	ofRunApp(new ofApp());
}