#include "ofApp.h"
#include "GLFW/glfw3.h"
#include "ofAppGLFWWindow.h"
#include <algorithm>
#include <glm/gtx/intersect.hpp>
#include <limits>
#include <queue>
#include <random>
#include <set>

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetEscapeQuitsApp(false);
	ofSetVerticalSync(true);
	ofSetBackgroundColor(22);
	ofDisableArbTex();
	ofSetCircleResolution(64);

	// --- 1. UI & CONFIG ---
	// Note: Paths now point to UI/ folder
	uiFont.load("UI/Roboto-Regular.ttf", 24);
	titleFont.load("UI/Roboto-Bold.ttf", 72);
	cardBackImage.load("UI/card_back.png");
	cardSpriteSheet.load("UI/TTS_Sheet.png");

	// --- 2. UNITS ---
	playerModel.load("Units/Player/player.obj");
	playerModel.setRotation(0, -90, 1, 0, 0);
	playerModel.setScale(0.008f, 0.008f, 0.008f);

	skeletonModel.load("Units/Skeleton/skeleton.fbx");
	ofLoadImage(skeletonTexture, "Units/Skeleton/base.png");
	skeletonModel.setRotation(0, 180, 1, 0, 0);
	skeletonModel.setRotation(1, 180, 0, 1, 0);

	skeletonModel.setScale(0.008f, 0.008f, 0.008f);
	skeletonModel.disableMaterials();

	// --- 3. BOARD & SKYBOX ---
	ofLoadImage(wallTexture, "Board/wall.png");
	wallTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);

	// Load Floor Textures (Floor1.PNG to Floor6.PNG)
	floorTextures.clear();
	floorMeshes.clear();

	// Loop from 1 to 6
	for (int i = 1; i <= 6; i++) {
		ofTexture tex;
		// Construct filename: "Board/Floor1.PNG", etc.
		// Note: .PNG is case-sensitive on some systems
		string filename = "Board/Floor" + ofToString(i) + ".PNG";

		if (ofLoadImage(tex, filename)) {
			tex.generateMipmap();
			tex.setTextureMinMagFilter(GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST);
			floorTextures.push_back(tex);

			ofMesh m;
			m.setMode(OF_PRIMITIVE_TRIANGLES);
			floorMeshes.push_back(m);
		} else {
			ofLogError("Setup") << "Failed to load " << filename;
		}
	}

	// --- 4. DICE TEXTURES & COIN ---
	// Note: Paths point to specific Dice/ subfolders
	ofLoadImage(d4Texture, "Dice/D4/Dice_d4_Albedo.png");
	ofLoadImage(d6Texture, "Dice/D6/dice_texture_d6.png");
	ofLoadImage(d10Texture, "Dice/D10/d10SilverAlbedo.png");
	ofLoadImage(d20Texture, "Dice/D20/d20_diffuse.png");

	ofLoadImage(coinFacesTexture, "Dice/Coin/CoinUKSilver.png");
	coinFacesTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);

	// --- 5. SOUNDS ---
	// Note: Path points to Sounds/Player/
	for (int i = 1; i <= 6; i++) {
		ofSoundPlayer step;
		if (step.load("Sounds/Player/step" + ofToString(i) + ".wav")) {
			step.setMultiPlay(true);
			step.setVolume(0.5f);
			footstepSounds.push_back(step);
		} else {
			ofLogError("Sound") << "Could not load Sounds/Player/step" << i << ".wav";
		}
	}

	// --- 6. MESH GENERATION (Walls & Floor) ---
	// (This code remains unchanged as it generates geometry programmatically)
	float wallSize = TILE_SIZE * 0.8f;
	wallMesh.clear();
	wallMesh.setMode(OF_PRIMITIVE_TRIANGLES);
	wallMesh.addVertex(ofPoint(-wallSize / 2, 0, -wallSize / 2));
	wallMesh.addVertex(ofPoint(wallSize / 2, 0, -wallSize / 2));
	wallMesh.addVertex(ofPoint(wallSize / 2, 0, wallSize / 2));
	wallMesh.addVertex(ofPoint(-wallSize / 2, 0, wallSize / 2));
	wallMesh.addTexCoord(ofVec2f(0.4f, 0.4f));
	wallMesh.addTexCoord(ofVec2f(0.6f, 0.4f));
	wallMesh.addTexCoord(ofVec2f(0.6f, 0.6f));
	wallMesh.addTexCoord(ofVec2f(0.4f, 0.6f));
	for (int i = 0; i < 4; i++)
		wallMesh.addNormal(ofPoint(0, 1, 0));
	wallMesh.addIndex(0);
	wallMesh.addIndex(1);
	wallMesh.addIndex(2);
	wallMesh.addIndex(0);
	wallMesh.addIndex(2);
	wallMesh.addIndex(3);

	// D6 Mesh Gen
	d6Mesh.clear();
	d6Mesh.setMode(OF_PRIMITIVE_TRIANGLES);
	float size = 1.0f;
	const float atlasWidth = 333.0f, atlasHeight = 225.0f;
	glm::vec2 uv_1_min(0.0f / atlasWidth, 0.0f / atlasHeight), uv_1_max(104.0f / atlasWidth, 104.0f / atlasHeight);
	glm::vec2 uv_2_min(114.0f / atlasWidth, 0.0f / atlasHeight), uv_2_max(218.0f / atlasWidth, 104.0f / atlasHeight);
	glm::vec2 uv_3_min(228.0f / atlasWidth, 0.0f / atlasHeight), uv_3_max(332.0f / atlasWidth, 104.0f / atlasHeight);
	glm::vec2 uv_4_min(0.0f / atlasWidth, 120.0f / atlasHeight), uv_4_max(104.0f / atlasWidth, 224.0f / atlasHeight);
	glm::vec2 uv_5_min(114.0f / atlasWidth, 120.0f / atlasHeight), uv_5_max(218.0f / atlasWidth, 224.0f / atlasHeight);
	glm::vec2 uv_6_min(228.0f / atlasWidth, 120.0f / atlasHeight), uv_6_max(332.0f / atlasWidth, 224.0f / atlasHeight);
	auto addFace = [&](glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 v4, glm::vec2 t_min, glm::vec2 t_max, glm::vec3 normal) {
		int baseIndex = d6Mesh.getNumVertices();
		d6Mesh.addVertex(v1 * size);
		d6Mesh.addTexCoord({ t_min.x, t_max.y });
		d6Mesh.addVertex(v2 * size);
		d6Mesh.addTexCoord({ t_max.x, t_max.y });
		d6Mesh.addVertex(v3 * size);
		d6Mesh.addTexCoord({ t_max.x, t_min.y });
		d6Mesh.addVertex(v4 * size);
		d6Mesh.addTexCoord({ t_min.x, t_min.y });
		for (int i = 0; i < 4; i++)
			d6Mesh.addNormal(normal);
		d6Mesh.addIndex(baseIndex);
		d6Mesh.addIndex(baseIndex + 1);
		d6Mesh.addIndex(baseIndex + 2);
		d6Mesh.addIndex(baseIndex);
		d6Mesh.addIndex(baseIndex + 2);
		d6Mesh.addIndex(baseIndex + 3);
	};
	addFace({ -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }, uv_1_min, uv_1_max, { 0, 0, 1 });
	addFace({ 1, -1, -1 }, { -1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 }, uv_6_min, uv_6_max, { 0, 0, -1 });
	addFace({ -1, 1, 1 }, { 1, 1, 1 }, { 1, 1, -1 }, { -1, 1, -1 }, uv_2_min, uv_2_max, { 0, 1, 0 });
	addFace({ -1, -1, -1 }, { 1, -1, -1 }, { 1, -1, 1 }, { -1, -1, 1 }, uv_5_min, uv_5_max, { 0, -1, 0 });
	addFace({ 1, -1, 1 }, { 1, -1, -1 }, { 1, 1, -1 }, { 1, 1, 1 }, uv_3_min, uv_3_max, { 1, 0, 0 });
	addFace({ -1, -1, -1 }, { -1, -1, 1 }, { -1, 1, 1 }, { -1, 1, -1 }, uv_4_min, uv_4_max, { -1, 0, 0 });

	// --- 7. DICE MODELS ---
	ofxAssimpModelLoader tempLoader;

	// Load D4
	if (tempLoader.load("Dice/D4/Dice_d4.obj")) {
		d4Mesh = tempLoader.getMesh(0);
		glm::vec3 meshCenter = d4Mesh.getCentroid();
		for (auto & v : d4Mesh.getVertices())
			v -= meshCenter;
		float maxSize = 0.0f;
		for (auto & v : d4Mesh.getVertices())
			maxSize = std::max(maxSize, glm::length(v));
		if (maxSize > 0) {
			float scaleFactor = 1.0f / maxSize;
			for (auto & v : d4Mesh.getVertices())
				v *= scaleFactor;
		}
	}
	// Load D10
	if (tempLoader.load("Dice/D10/d10.obj")) {
		d10Mesh = tempLoader.getMesh(0);
		glm::vec3 meshCenter = d10Mesh.getCentroid();
		for (auto & v : d10Mesh.getVertices())
			v -= meshCenter;
		float maxSize = 0.0f;
		for (auto & v : d10Mesh.getVertices())
			maxSize = std::max(maxSize, glm::length(v));
		if (maxSize > 0) {
			float scaleFactor = 1.0f / maxSize;
			for (auto & v : d10Mesh.getVertices())
				v *= scaleFactor;
		}
	}
	// Load D20
	if (tempLoader.load("Dice/D20/d20.obj")) {
		d20Mesh = tempLoader.getMesh(0);
		glm::vec3 meshCenter = d20Mesh.getCentroid();
		for (auto & v : d20Mesh.getVertices())
			v -= meshCenter;
		float maxSize = 0.0f;
		for (auto & v : d20Mesh.getVertices())
			maxSize = std::max(maxSize, glm::length(v));
		if (maxSize > 0) {
			float scaleFactor = 1.0f / maxSize;
			for (auto & v : d20Mesh.getVertices())
				v *= scaleFactor;
		}
	}

	// Coin Mesh Gen
	coinMesh.clear();
	coinMesh.setMode(OF_PRIMITIVE_TRIANGLES);
	const float coinRadius = 2.0f;
	const float coinThickness = 0.2f;
	const int coinResolution = 32;
	ofRectangle headsUV(0.0f, 0.0f, 0.5f, 1.0f);
	ofRectangle tailsUV(0.5f, 0.0f, 0.5f, 1.0f);

	int topCenterIndex = coinMesh.getNumVertices();
	coinMesh.addVertex({ 0, coinThickness / 2.0f, 0 });
	coinMesh.addNormal({ 0, 1, 0 });
	coinMesh.addTexCoord({ headsUV.getCenter().x, headsUV.getCenter().y });
	for (int i = 0; i <= coinResolution; i++) {
		float angle = (float)i / coinResolution * TWO_PI;
		coinMesh.addVertex({ cos(angle) * coinRadius, coinThickness / 2.0f, sin(angle) * coinRadius });
		coinMesh.addNormal({ 0, 1, 0 });
		coinMesh.addTexCoord({ headsUV.x + headsUV.width * (0.5f + 0.5f * cos(angle)), headsUV.y + headsUV.height * (0.5f + 0.5f * sin(angle)) });
	}
	for (int i = 0; i < coinResolution; i++) {
		coinMesh.addIndex(topCenterIndex);
		coinMesh.addIndex(topCenterIndex + 1 + i);
		coinMesh.addIndex(topCenterIndex + 1 + i + 1);
	}

	int bottomCenterIndex = coinMesh.getNumVertices();
	coinMesh.addVertex({ 0, -coinThickness / 2.0f, 0 });
	coinMesh.addNormal({ 0, -1, 0 });
	coinMesh.addTexCoord({ tailsUV.getCenter().x, tailsUV.getCenter().y });
	for (int i = 0; i <= coinResolution; i++) {
		float angle = (float)i / coinResolution * TWO_PI;
		coinMesh.addVertex({ cos(angle) * coinRadius, -coinThickness / 2.0f, sin(angle) * coinRadius });
		coinMesh.addNormal({ 0, -1, 0 });
		coinMesh.addTexCoord({ tailsUV.x + tailsUV.width * (0.5f + 0.5f * cos(angle)), tailsUV.y + tailsUV.height * (0.5f + 0.5f * sin(angle)) });
	}
	for (int i = 0; i < coinResolution; i++) {
		coinMesh.addIndex(bottomCenterIndex);
		coinMesh.addIndex(bottomCenterIndex + 1 + i + 1);
		coinMesh.addIndex(bottomCenterIndex + 1 + i);
	}

	ofColor edgeColor = ofColor::goldenRod;
	int edgeStartIndex = coinMesh.getNumVertices();
	for (int i = 0; i <= coinResolution; i++) {
		float angle = (float)i / coinResolution * TWO_PI;
		glm::vec3 normal = glm::normalize(glm::vec3(cos(angle), 0, sin(angle)));
		coinMesh.addVertex({ cos(angle) * coinRadius, coinThickness / 2.0f, sin(angle) * coinRadius });
		coinMesh.addNormal(normal);
		coinMesh.addColor(edgeColor);
		coinMesh.addVertex({ cos(angle) * coinRadius, -coinThickness / 2.0f, sin(angle) * coinRadius });
		coinMesh.addNormal(normal);
		coinMesh.addColor(edgeColor);
	}
	for (int i = 0; i < coinResolution; i++) {
		int current = edgeStartIndex + i * 2;
		int next = edgeStartIndex + (i + 1) * 2;
		coinMesh.addIndex(current);
		coinMesh.addIndex(next);
		coinMesh.addIndex(current + 1);
		coinMesh.addIndex(next);
		coinMesh.addIndex(next + 1);
		coinMesh.addIndex(current + 1);
	}

	// --- 8. MATERIALS & LIGHTS ---

	// 1. Material Settings
	modelMaterial.setShininess(10);
	modelMaterial.setSpecularColor(ofColor(50, 50, 50));
	modelMaterial.setDiffuseColor(ofColor(255, 255, 255));
	modelMaterial.setAmbientColor(ofColor(255, 255, 255));

	// 1. GLOBAL AMBIENT (Brightness Fix)
	// Was (60, 60, 80). Changed to (100, 100, 100).
	// This is a neutral grey (no purple tint) and significantly brighter.
	ofSetGlobalAmbientColor(ofColor(70, 70, 70));

	lights.clear();

	// 2. KEY LIGHT (Main Illumination)
	keyLight.setup();
	keyLight.setPointLight();
	keyLight.setPosition(50, 150, 50); // Raised Y to 150 for better spread
	// Neutral white light, boosted brightness
	keyLight.setDiffuseColor(ofColor(140, 140, 140));
	keyLight.setSpecularColor(ofColor(50, 50, 50));
	keyLight.setAttenuation(1.0f, 0.005f, 0.0f);
	lights.push_back(keyLight);

	// 3. RIM LIGHT (Backlight)
	rimLight.setup();
	rimLight.setPointLight();
	rimLight.setPosition(-50, 30, -50);
	// Very subtle warm glow, not deep red
	rimLight.setDiffuseColor(ofColor(80, 60, 50));
	rimLight.setSpecularColor(ofColor(50, 0, 0));
	lights.push_back(rimLight);

	// 5. HEADLIGHT (Torch)
	headlight.setup();
	headlight.setPointLight();
	headlight.setDiffuseColor(ofColor(220, 170, 100));
	headlight.setSpecularColor(ofColor(255, 200, 150));
	headlight.setAttenuation(1.0f, 0.001f, 0.0f);

	cam.setupPerspective(false, 60, 0.1f, 100000);
	cam.setFarClip(100000);

	// --- SHADOW TEXTURE GENERATION ---
	ofPixels pix;
	pix.allocate(64, 64, OF_PIXELS_RGBA);
	for (int x = 0; x < 64; x++) {
		for (int y = 0; y < 64; y++) {
			float dist = ofDist(x, y, 32, 32);
			float alpha = ofMap(dist, 0, 32, 200, 0, true);
			pix.setColor(x, y, ofColor(0, 0, 0, alpha));
		}
	}
	shadowTexture.setFromPixels(pix);

	// --- 9. LOAD CARD DATA ---
	// Note: Path points to Config/ folder
	loadCardData("Config/cards.json");

	// --- 10. SCREEN SETTINGS ---
	availableResolutions = { { 1024, 768 }, { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 } };
	int screenW = ofGetScreenWidth();
	int screenH = ofGetScreenHeight();
	bool found = false;
	for (size_t i = 0; i < availableResolutions.size(); ++i) {
		if (availableResolutions[i].x == screenW && availableResolutions[i].y == screenH) {
			currentResolutionIndex = static_cast<int>(i);
			found = true;
			break;
		}
	}
	if (!found) {
		availableResolutions.push_back(glm::vec2(screenW, screenH));
		currentResolutionIndex = availableResolutions.size() - 1;
	}

	int monitorRefreshRate = 60;
	GLFWmonitor * primary = glfwGetPrimaryMonitor();
	if (primary) {
		const GLFWvidmode * mode = glfwGetVideoMode(primary);
		monitorRefreshRate = mode->refreshRate;
	}

	availableFramerates.clear();
	availableFramerates.push_back(30);
	availableFramerates.push_back(60);
	if (monitorRefreshRate != 30 && monitorRefreshRate != 60) {
		availableFramerates.push_back(monitorRefreshRate);
	}
	availableFramerates.push_back(0);

	for (size_t i = 0; i < availableFramerates.size(); i++) {
		if (availableFramerates[i] == monitorRefreshRate) {
			currentFramerateIndex = i;
			break;
		}
	}

	isFullscreen = true;
	ofSetFullscreen(true);
	applySettings();
}

//--------------------------------------------------------------
Player * ofApp::getPlayer(int index) {
	if (index >= 0 && index < static_cast<int>(players.size())) {
		return &players[index];
	}
	return nullptr;
}
//--------------------------------------------------------------
void ofApp::update() {
	// --- LOADING LOGIC ---
	if (isLoadingGame) {
		setupGame();
		isLoadingGame = false;
		currentState = STATE_GAMEPLAY;
		return;
	}

	switch (currentState) {
	case STATE_MAIN_MENU:
		break;
	case STATE_SETTINGS:
		break;
	case STATE_GAMEPLAY:
		updateGame();
		break;
	case STATE_PAUSED:
		break;
	}
}
//--------------------------------------------------------------
void ofApp::draw() {
	// --- LOADING SCREEN ---
	if (isLoadingGame) {
		ofBackground(0);
		ofSetColor(255);
		string loadText = "Loading...";
		ofRectangle bbox = titleFont.getStringBoundingBox(loadText, 0, 0);
		titleFont.drawString(loadText, ofGetWidth() / 2 - bbox.width / 2, ofGetHeight() / 2);
		return;
	}

	ofBackground(22);
	switch (currentState) {
	case STATE_MAIN_MENU:
		drawMainMenu();
		break;
	case STATE_SETTINGS:
		drawSettingsMenu();
		break;
	case STATE_GAMEPLAY:
		drawGame();
		break;
	case STATE_PAUSED:
		drawGame();
		drawPauseMenu();
		break;
	}
}

//--------------------------------------------------------------
void ofApp::drawMainMenu() {
	ofSetColor(ofColor::white);
	string title = "Mage Fight";
	ofRectangle titleBox = titleFont.getStringBoundingBox(title, 0, 0);
	// --- FIX: Round the drawing position to avoid blurry text ---
	float titleX = round(ofGetWidth() / 2.0f - titleBox.getWidth() / 2.0f);
	float titleY = round(ofGetHeight() * 0.25f);
	titleFont.drawString(title, titleX, titleY);

	// --- Draw Buttons ---
	auto drawButton = [&](const ofRectangle & rect, const string & text, bool isHovered) {
		ofSetColor(isHovered ? ofColor::lightGray : ofColor::white);
		ofFill();
		ofDrawRectRounded(rect, 15);

		ofSetColor(ofColor::black);
		ofNoFill();
		ofSetLineWidth(2);
		ofDrawRectRounded(rect, 15);
		ofFill();

		ofRectangle textBox = uiFont.getStringBoundingBox(text, 0, 0);
		// --- FIX: Round the drawing position for button text too ---
		float textX = round(rect.getCenter().x - textBox.getWidth() / 2.0f);
		float textY = round(rect.getCenter().y + textBox.getHeight() / 2.0f);
		uiFont.drawString(text, textX, textY);
	};

	drawButton(mainMenuPlayAIButton, "Play vs AI", mainMenuHoveredIndex == 0);
	drawButton(mainMenuMultiplayerButton, "Multiplayer (Disabled)", mainMenuHoveredIndex == 1);
	drawButton(mainMenuSettingsButton, "Settings", mainMenuHoveredIndex == 2);
	drawButton(mainMenuQuitButton, "Quit", mainMenuHoveredIndex == 3);
}

//--------------------------------------------------------------
void ofApp::drawSettingsMenu() {
	// Draw Title
	ofSetColor(ofColor::white);
	string title = "Settings";
	ofRectangle titleBox = titleFont.getStringBoundingBox(title, 0, 0);
	titleFont.drawString(title, ofGetWidth() / 2 - titleBox.getWidth() / 2, ofGetHeight() * 0.15);

	// --- Settings UI Positions ---
	float settingY = ofGetHeight() * 0.3f;
	float settingSpacing = 100;
	float centerX = ofGetWidth() / 2.0f;
	float labelOffset = 350;
	float controlWidth = 250;

	// --- Helper for drawing a setting row ---
	auto drawSettingRow = [&](string label, string value, ofRectangle & leftBtn, ofRectangle & rightBtn, float yPos) {
		// Draw Label
		ofSetColor(ofColor::white);
		uiFont.drawString(label, centerX - labelOffset, yPos + 25);

		// Draw Left/Right buttons
		leftBtn.set(centerX - (controlWidth / 2) - 45, yPos, 40, 40);
		rightBtn.set(centerX + (controlWidth / 2) + 5, yPos, 40, 40);
		ofDrawRectRounded(leftBtn, 5);
		ofDrawRectRounded(rightBtn, 5);

		// Draw Background for the value text
		ofRectangle bgRect(centerX - (controlWidth / 2), yPos - 5, controlWidth, 50);
		ofDrawRectangle(bgRect);

		// --- FIX: Draw TEXT AFTER the background and set its color to BLACK ---
		ofSetColor(ofColor::black);
		uiFont.drawString(value, bgRect.x + 10, bgRect.y + 30);
		uiFont.drawString("<", leftBtn.getCenter().x - 5, leftBtn.getCenter().y + 10);
		uiFont.drawString(">", rightBtn.getCenter().x - 5, rightBtn.getCenter().y + 10);
	};

	// --- Draw Resolution ---
	string resText = ofToString((int)availableResolutions[currentResolutionIndex].x) + " x " + ofToString((int)availableResolutions[currentResolutionIndex].y);
	drawSettingRow("Resolution", resText, settingsResLeftButton, settingsResRightButton, settingY);

	// --- Draw Framerate ---
	settingY += settingSpacing;
	string frameText = (availableFramerates[currentFramerateIndex] == 0) ? "Unlocked" : ofToString(availableFramerates[currentFramerateIndex]);
	drawSettingRow("Framerate", frameText, settingsFrameLeftButton, settingsFrameRightButton, settingY);

	// --- Draw Fullscreen ---
	settingY += settingSpacing;
	ofSetColor(ofColor::white);
	uiFont.drawString("Display Mode", centerX - labelOffset, settingY + 25);
	string fsText = isFullscreen ? "Fullscreen" : "Windowed";
	settingsFullscreenButton.set(centerX - (controlWidth / 2), settingY - 5, controlWidth, 50);
	ofDrawRectangle(settingsFullscreenButton);
	// --- FIX: Draw TEXT AFTER the background and set its color to BLACK ---
	ofSetColor(ofColor::black);
	uiFont.drawString(fsText, settingsFullscreenButton.x + 10, settingsFullscreenButton.y + 30);

	// --- Draw Back Button ---
	settingsBackButton.set(centerX - 150, ofGetHeight() * 0.8, 300, 70);
	ofSetColor(settingsHoveredIndex == 0 ? ofColor::lightGray : ofColor::white);
	ofFill();
	ofDrawRectRounded(settingsBackButton, 15);
	ofSetColor(ofColor::black);
	ofNoFill();
	ofSetLineWidth(2);
	ofDrawRectRounded(settingsBackButton, 15);
	ofFill();
	ofRectangle backBox = uiFont.getStringBoundingBox("Back", 0, 0);
	uiFont.drawString("Back", settingsBackButton.getCenter().x - backBox.getWidth() / 2, settingsBackButton.getCenter().y + backBox.getHeight() / 2);
}
//--------------------------------------------------------------
void ofApp::applySettings() {
	glm::vec2 res = availableResolutions[currentResolutionIndex];

	if (isFullscreen) {
		if (ofGetWindowMode() != OF_FULLSCREEN) {
			ofSetFullscreen(true);
		}
	} else {
		if (ofGetWindowMode() == OF_FULLSCREEN) {
			ofSetFullscreen(false);
		}
		ofSetWindowShape(res.x, res.y);
		int screenW = ofGetScreenWidth();
		int screenH = ofGetScreenHeight();
		ofSetWindowPosition((screenW - res.x) / 2, (screenH - res.y) / 2);
	}

	// Get target FPS (e.g., 180, 60, or 0)
	int targetFPS = availableFramerates[currentFramerateIndex];

	if (targetFPS == 0) {
		// Unlimited Mode
		ofSetVerticalSync(false); // Must be OFF to go unlimited
		ofSetFrameRate(0);
	} else {
		// Capped Mode (Native or 60/30)
		ofSetVerticalSync(true); // Enforce monitor sync
		ofSetFrameRate(targetFPS); // Also cap CPU loop to avoid spins
	}

	recalculateUI(ofGetWidth(), ofGetHeight());

	// --- RECALCULATE MAIN MENU BUTTONS ---
	float btnWidth = 400;
	float btnHeight = 80;
	float centerX = ofGetWidth() / 2.0f;
	float startY = ofGetHeight() / 2.0f - btnHeight;

	mainMenuPlayAIButton.set(centerX - btnWidth / 2, startY, btnWidth, btnHeight);
	mainMenuMultiplayerButton.set(centerX - btnWidth / 2, startY + btnHeight + 20, btnWidth, btnHeight);
	mainMenuSettingsButton.set(centerX - btnWidth / 2, startY + (btnHeight + 20) * 2, btnWidth, btnHeight);
	mainMenuQuitButton.set(centerX - btnWidth / 2, startY + (btnHeight + 20) * 3, btnWidth, btnHeight);

	// --- 11. POST PROCESSING (Optional, used for 3D world only) ---
	{
		const GLubyte * vendor = glGetString(GL_VENDOR);
		const GLubyte * renderer = glGetString(GL_RENDERER);
		const GLubyte * version = glGetString(GL_VERSION);
		const GLubyte * glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
		ofLogNotice("GL")
			<< "Vendor: " << (vendor ? reinterpret_cast<const char *>(vendor) : "(null)")
			<< " | Renderer: " << (renderer ? reinterpret_cast<const char *>(renderer) : "(null)")
			<< " | Version: " << (version ? reinterpret_cast<const char *>(version) : "(null)")
			<< " | GLSL: " << (glsl ? reinterpret_cast<const char *>(glsl) : "(null)");
	}

	worldPostShaderLoaded = false;
	worldPostShader.unload();
	const bool shaderVertOk = worldPostShader.setupShaderFromFile(GL_VERTEX_SHADER, "shaders/post.vert");
	const bool shaderFragOk = worldPostShader.setupShaderFromFile(GL_FRAGMENT_SHADER, "shaders/post.frag");
	if (shaderVertOk && shaderFragOk) {
		// Critical on some systems: bind OF's default attribute locations (position/texcoord/etc)
		// BEFORE linking, otherwise our fullscreen quad can end up with no valid attributes.
		worldPostShader.bindDefaults();
		worldPostShaderLoaded = worldPostShader.linkProgram();
	}
	if (!worldPostShaderLoaded) {
		ofLogWarning("Setup") << "Post shader failed to compile/link. Continuing without post-processing.";
	}
	allocateWorldFbo(ofGetWidth(), ofGetHeight());
}

//--------------------------------------------------------------
void ofApp::allocateWorldFbo(int w, int h) {
	if (w <= 0 || h <= 0) return;

	const int currentW = static_cast<int>(worldFbo.getWidth());
	const int currentH = static_cast<int>(worldFbo.getHeight());
	if (worldFbo.isAllocated() && currentW == w && currentH == h) return;

	ofFbo::Settings settings;
	settings.width = w;
	settings.height = h;
	settings.internalformat = GL_RGBA8;
	settings.textureTarget = GL_TEXTURE_2D;
	settings.useDepth = true;
	settings.useStencil = false;
	settings.depthStencilAsTexture = false;
	settings.minFilter = GL_LINEAR;
	settings.maxFilter = GL_LINEAR;

	worldFbo.allocate(settings);
	if (!worldFbo.isAllocated()) {
		ofLogWarning("FBO") << "worldFbo failed to allocate at " << w << "x" << h;
	} else {
		ofLogNotice("FBO") << "worldFbo allocated " << w << "x" << h;
	}
}
//--------------------------------------------------------------
void ofApp::drawPauseMenu() {
	// Draw a semi-transparent overlay
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// --- Update button positions (in case of resize) ---
	float btnWidth = 350;
	float btnHeight = 70;
	float centerX = ofGetWidth() / 2.0f;
	float centerY = ofGetHeight() / 2.0f;
	pauseMenuResumeButton.set(centerX - btnWidth / 2, centerY - btnHeight * 1.5 - 20, btnWidth, btnHeight);
	pauseMenuSettingsButton.set(centerX - btnWidth / 2, centerY - btnHeight / 2, btnWidth, btnHeight);
	pauseMenuQuitButton.set(centerX - btnWidth / 2, centerY + btnHeight / 2 + 20, btnWidth, btnHeight);

	// --- Draw Buttons ---
	auto drawButton = [&](const ofRectangle & rect, const string & text, bool isHovered) {
		ofSetColor(isHovered ? ofColor::lightGray : ofColor::white);
		ofFill();
		ofDrawRectRounded(rect, 15);
		ofSetColor(ofColor::black);
		ofNoFill();
		ofSetLineWidth(2);
		ofDrawRectRounded(rect, 15);
		ofFill();
		ofRectangle textBox = uiFont.getStringBoundingBox(text, 0, 0);
		float textX = round(rect.getCenter().x - textBox.getWidth() / 2.0f);
		float textY = round(rect.getCenter().y + textBox.getHeight() / 2.0f);
		uiFont.drawString(text, textX, textY);
	};

	drawButton(pauseMenuResumeButton, "Resume", pauseMenuHoveredIndex == 0);
	drawButton(pauseMenuSettingsButton, "Settings", pauseMenuHoveredIndex == 1);
	drawButton(pauseMenuQuitButton, "Quit to Main Menu", pauseMenuHoveredIndex == 2);
}
// Recalculate ui
void ofApp::recalculateUI(int w, int h) {
	// 1. Update Camera Aspect Ratio
	cam.setAspectRatio((float)w / (float)h);
	lastWindowWidth = w;
	lastWindowHeight = h;

	// 2. Recalculate Main Menu Buttons
	float btnWidth = 400;
	float btnHeight = 80;
	float centerX = w / 2.0f;
	float startY = h / 2.0f - btnHeight;

	mainMenuPlayAIButton.set(centerX - btnWidth / 2, startY, btnWidth, btnHeight);
	mainMenuMultiplayerButton.set(centerX - btnWidth / 2, startY + btnHeight + 20, btnWidth, btnHeight);
	mainMenuSettingsButton.set(centerX - btnWidth / 2, startY + (btnHeight + 20) * 2, btnWidth, btnHeight);
	mainMenuQuitButton.set(centerX - btnWidth / 2, startY + (btnHeight + 20) * 3, btnWidth, btnHeight);
}
//--------------------------------------------------------------
void ofApp::setupGame() {
	// --- RESET CORE GAME STATE (Fast Operations Only) ---
	players.clear();
	activeDiceRolls.clear();
	for (int x = 0; x < BOARD_WIDTH; ++x) {
		for (int y = 0; y < BOARD_HEIGHT; ++y) {
			board[x][y] = Tile(); // Reset each tile
		}
	}

	// Reseed the random number generator for a new game
	std::random_device rd;
	rng.seed(rd());

	// --- CAMERA RESET ---
	// More zoomed out (45 vs 35) and a higher angle (0.8 vs 0.5)
	cameraTargetZoom = 45.0f;
	cameraCurrentZoom = 45.0f;
	cameraTargetPan = glm::vec3(0, 0, 0);
	cameraCurrentPan = glm::vec3(0, 0, 0);
	isTopDownView = false;
	cam.setPosition(0, cameraCurrentZoom, cameraCurrentZoom * 0.8f);
	cam.lookAt(cameraCurrentPan);
	cameraCurrentPos = cam.getPosition();
	cameraCurrentLookAt = cameraCurrentPan;

	// --- BOARD WALLS SETUP ---
	board[2][2].hasWall = true;
	board[2][1].hasWall = true;
	board[3][1].hasWall = true;
	board[4][1].hasWall = true;
	board[10][2].hasWall = true;
	board[10][1].hasWall = true;
	board[9][1].hasWall = true;
	board[8][1].hasWall = true;
	board[2][6].hasWall = true;
	board[2][7].hasWall = true;
	board[3][7].hasWall = true;
	board[4][7].hasWall = true;
	board[10][6].hasWall = true;
	board[10][7].hasWall = true;
	board[9][7].hasWall = true;
	board[8][7].hasWall = true;
	board[1][4].hasWall = true;
	board[2][4].hasWall = true;
	board[3][4].hasWall = true;
	board[11][4].hasWall = true;
	board[10][4].hasWall = true;
	board[9][4].hasWall = true;
	board[6][3].hasWall = true;
	board[5][4].hasWall = true;
	board[6][5].hasWall = true;
	board[7][4].hasWall = true;

	buildLevelMesh();
	buildFloorMesh();

	// --- PLAYER CREATION ---
	Player p1;
	p1.x = 0;
	p1.y = BOARD_HEIGHT - 1;
	p1.playerID = 0;
	p1.deck = allCards; // Use the pre-loaded allCards vector
	std::shuffle(p1.deck.begin(), p1.deck.end(), rng);
	players.push_back(p1);

	Player p2;
	p2.x = BOARD_WIDTH - 1;
	p2.y = 0;
	p2.playerID = 1;
	p2.deck = allCards; // Use the pre-loaded allCards vector
	std::shuffle(p2.deck.begin(), p2.deck.end(), rng);
	players.push_back(p2);

	board[p1.x][p1.y].hasPlayer = true;
	board[p2.x][p2.y].hasPlayer = true;

	// --- FINAL GAME STATE INITIALIZATION ---
	ofLogNotice("Game") << "--- GAME SESSION STARTING ---";
	currentPlayerIndex = -1;
	startNewTurn();

	// Set initial camera viewport
	cam.setAspectRatio((float)ofGetWidth() / (float)ofGetHeight());
}

//--------------------------------------------------------------
void ofApp::updateGame() {
	// 1. UPDATE UI POSITIONS
	updateDebugRects();

	// 2. Magic Blast / Dispel Freeze Check
	if (isMagicBlastChoiceActive || isDispelMenuOpen || isDispelTargeting || isDispelStatusSelectOpen) {
		return;
	}

	// --- Pile View Hover Logic ---
	if (isHoveringPile && !isShowingPileView) {
		if (ofGetElapsedTimef() - pileHoverStartTime > 0.6f) { // Reduced hover time
			isShowingPileView = true;
			currentPileView = hoveredPileType;
			currentPileViewPlayerIndex = hoveredPilePlayerIndex;
		}
	}

	float deltaTime = ofGetLastFrameTime();
	if (deltaTime > 0.1f) deltaTime = 1.0f / 60.0f; // CORRECTED: Proper delta time cap

	// --- Camera & Skybox Logic ---
	if (ofGetWidth() != lastWindowWidth || ofGetHeight() != lastWindowHeight) {
		cam.setAspectRatio((float)ofGetWidth() / (float)ofGetHeight());
		lastWindowWidth = ofGetWidth();
		lastWindowHeight = ofGetHeight();
	}

	float frame_independent_smoothing = 1.0 - pow(0.6, deltaTime * 60.0);
	cameraCurrentZoom = ofLerp(cameraCurrentZoom, cameraTargetZoom, frame_independent_smoothing);
	cameraCurrentPan = glm::mix(cameraCurrentPan, cameraTargetPan, frame_independent_smoothing);
	glm::vec3 targetPos;
	glm::vec3 targetLookAt = cameraCurrentPan;
	if (isTopDownView) {
		targetPos = glm::vec3(cameraCurrentPan.x, cameraCurrentZoom, cameraCurrentPan.z);
	} else {
		targetPos = glm::vec3(cameraCurrentPan.x, cameraCurrentZoom, cameraCurrentPan.z + cameraCurrentZoom * 0.5f);
	}
	cameraCurrentPos = glm::mix(cameraCurrentPos, targetPos, frame_independent_smoothing);
	cameraCurrentLookAt = glm::mix(cameraCurrentLookAt, targetLookAt, frame_independent_smoothing);
	cam.setPosition(cameraCurrentPos);
	cam.lookAt(cameraCurrentLookAt);

	// --- TORCH FLICKER LOGIC (SLOWER) ---
	float time = ofGetElapsedTimef();
	float flicker = ofNoise(time * 3.0f); // Speed

	// 2. Intensity Mapping
	// Map noise to a safe range (0.8 to 1.3)
	float intensity = ofMap(flicker, 0, 1, 0.8f, 1.3f);

	// 3. Position Wiggle (Slow sway)
	float wiggleX = ofNoise(time * 1.0f, 0) * 15.0f - 7.5f;
	float wiggleY = ofNoise(time * 1.0f, 100) * 10.0f - 5.0f;

	// 4. Apply Color
	// Base color is a warm orange/yellow.
	// We multiply by intensity, then clamp with std::min to prevent color wrapping.
	headlight.setDiffuseColor(ofColor(
		std::min(255.0f, 220.0f * intensity),
		std::min(255.0f, 160.0f * intensity),
		std::min(255.0f, 100.0f * intensity)));

	headlight.setSpecularColor(ofColor(255, 255, 255));

	headlight.setPosition(cam.getPosition() + glm::vec3(wiggleX, wiggleY, 0));

	// Ensure long range
	headlight.setAttenuation(1.0f, 0.001f, 0.0f);

	// --- UI Button Interpolation ---
	float scale = ofGetHeight() / 1080.0f;
	float btnWidth = 250 * scale;
	float visibleY = 20 * scale;
	float hiddenY = -100 * scale;

	// FIX: Check if it is Player 1's turn OR a Minion owned by Player 1
	bool isPlayer1Turn = false;
	if (currentPlayerIndex >= 0 && !players.empty()) {
		int pid = players[currentPlayerIndex].playerID;
		int oid = players[currentPlayerIndex].ownerID;
		// Assuming Player 1 is ID 0. Minions owned by P1 have ownerID 0.
		if (pid == 0 || oid == 0) isPlayer1Turn = true;
	}

	if (isPlayer1Turn) {
		endTurnButtonTargetPos.set(ofGetWidth() / 2.0f - btnWidth / 2.0f, visibleY);
	} else {
		endTurnButtonTargetPos.set(ofGetWidth() / 2.0f - btnWidth / 2.0f, hiddenY);
	}
	endTurnButtonCurrentPos = endTurnButtonCurrentPos.getInterpolated(endTurnButtonTargetPos, 0.2f);

	// --- Delayed Attack Logic ---
	if (isWaitingForAttackDice && activeDiceRolls.empty()) {
		isWaitingForAttackDice = false;
		int baseDamage = pendingAttackRollResult;
		for (size_t i = 0; i < pendingAttackTargetIndices.size(); i++) {
			int pIndex = pendingAttackTargetIndices[i];
			Player * target = getPlayer(pIndex);
			if (target) {
				int appliedDamage = baseDamage;
				if (pendingAttackDamageType == DAMAGE_PIERCING && i > 0) appliedDamage /= 2;
				int wardDmg = std::min(target->ward, appliedDamage);
				target->ward -= wardDmg;
				appliedDamage -= wardDmg;
				if (pendingAttackDamageType == DAMAGE_PHYSICAL) {
					int blockDmg = std::min(target->block, appliedDamage);
					target->block -= blockDmg;
					appliedDamage -= blockDmg;
				}
				target->health -= appliedDamage;
				ofLogNotice("Combat") << "Hit Player " << target->playerID << " for " << appliedDamage << " damage.";
			}
		}
		pendingAttackTargetIndices.clear();
	}

	// --- Amnesia Logic ---
	if (isWaitingForAmnesiaDice && activeDiceRolls.empty()) {
		isWaitingForAmnesiaDice = false;
		Player * amnesiaTarget = getPlayer(amnesiaTargetPlayerIndex);
		if (amnesiaTarget) {
			numCardsToRemove = std::min(pendingAmnesiaRollResult, (int)amnesiaTarget->deck.size());
			if (numCardsToRemove > 0) {
				isAmnesiaSelectionActive = true;
				amnesiaDeckCopy = amnesiaTarget->deck;
				amnesiaSelectedIndices.clear();
			} else {
				amnesiaTargetPlayerIndex = -1;
			}
		} else {
			amnesiaTargetPlayerIndex = -1;
		}
	}

	// --- MAGIC BLAST RESOLUTION ---
	if (isWaitingForMagicBlastDice && activeDiceRolls.empty()) {
		isWaitingForMagicBlastDice = false;
		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile = { (float)caster.x, (float)caster.y };

		// 1. Calculate Max Range (5ft = 1.0 Unit)
		float maxDistUnits = pendingMagicBlastRollResult / 5.0f;

		// 2. Calculate Required Distance (Face-to-Face)
		float neededDist = getFaceToFaceDistance(casterTile, pendingMagicBlastTargetTile);

		// Log
		int requiredFeet = (neededDist > 1000.0f) ? 999 : (int)ceil(neededDist * 5.0f);
		ofLogNotice("MagicBlast") << "Rolled: " << pendingMagicBlastRollResult << "ft (" << maxDistUnits << "). Needed: " << requiredFeet << "ft.";

		glm::vec2 impactTile;

		// 3. Determine Impact Location
		if (maxDistUnits >= neededDist - 0.001f) {
			impactTile = pendingMagicBlastTargetTile;
			ofLogNotice("MagicBlast") << "Target Reached.";
		} else {
			ofLogNotice("MagicBlast") << "Fell short!";
			glm::vec2 dir = pendingMagicBlastTargetTile - casterTile;
			if (glm::length(dir) > 0) dir = glm::normalize(dir);
			glm::vec2 impactPos = casterTile + (dir * (maxDistUnits + 1.0f));
			impactTile = { round(impactPos.x), round(impactPos.y) };
		}

		// 4. Identify Targets
		magicBlastTargetPlayerIndex = -1;
		magicBlastSplashTargetIndices.clear();

		// A. Check Direct Hit
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == (int)impactTile.x && players[i].y == (int)impactTile.y) {
				magicBlastTargetPlayerIndex = (int)i;
				break;
			}
		}

		// B. Check Splash Neighbors
		glm::vec2 neighbors[4] = { { impactTile.x + 1, impactTile.y }, { impactTile.x - 1, impactTile.y }, { impactTile.x, impactTile.y + 1 }, { impactTile.x, impactTile.y - 1 } };
		for (const auto & n : neighbors) {
			for (size_t i = 0; i < players.size(); i++) {
				// Don't add the direct target to the splash list (they are handled separately)
				if ((int)i != magicBlastTargetPlayerIndex && players[i].x == (int)n.x && players[i].y == (int)n.y) {
					magicBlastSplashTargetIndices.push_back((int)i);
				}
			}
		}

		// 5. Initialize Choice Queue
		if (magicBlastTargetPlayerIndex != -1) {
			// Scenario A: Direct Hit exists.
			// Start with Direct Target -> 3 Choices.
			isMagicBlastChoiceActive = true;
			magicBlastChoicesRemaining = 3;
		} else if (!magicBlastSplashTargetIndices.empty()) {
			// Scenario B: No Direct Hit (hit empty ground), but Splash targets exist.
			// Pop the first splash target -> 1 Choice.
			isMagicBlastChoiceActive = true;
			magicBlastTargetPlayerIndex = magicBlastSplashTargetIndices.front();
			magicBlastSplashTargetIndices.erase(magicBlastSplashTargetIndices.begin());
			magicBlastChoicesRemaining = 1;
		} else {
			ofLogNotice("MagicBlast") << "No targets hit.";
		}
	}

	// --- FIREBALL RANGE RESOLUTION ---
	if (isWaitingForFireballRangeDice && activeDiceRolls.empty()) {
		isWaitingForFireballRangeDice = false;
		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile = { (float)caster.x, (float)caster.y };

		float maxDistUnits = pendingFireballRangeResult / 5.0f;
		float neededDist = getFaceToFaceDistance(casterTile, pendingFireballTargetTile);

		int requiredFeet = (neededDist > 1000.0f) ? 999 : (int)ceil(neededDist * 5.0f);

		ofLogNotice("Fireball") << "Rolled: " << pendingFireballRangeResult << "ft (" << maxDistUnits << "). Needed: " << requiredFeet << "ft.";

		if (maxDistUnits >= neededDist - 0.001f) {
			fireballImpactTile = pendingFireballTargetTile;
			ofLogNotice("Fireball") << "Direct Hit!";
		} else {
			ofLogNotice("Fireball") << "Fell short!";
			glm::vec2 dir = pendingFireballTargetTile - casterTile;
			if (glm::length(dir) > 0) dir = glm::normalize(dir);
			glm::vec2 impactPos = casterTile + (dir * (maxDistUnits + 1.0f));
			fireballImpactTile = { round(impactPos.x), round(impactPos.y) };
		}

		// 4. Check for Impact on Unit
		fireballTargetPlayerIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == (int)fireballImpactTile.x && players[i].y == (int)fireballImpactTile.y) {
				fireballTargetPlayerIndex = (int)i;
				break;
			}
		}

		// 5. Trigger Damage Roll
		if (fireballTargetPlayerIndex != -1) {
			ofLogNotice("Fireball") << "Hit Player " << players[fireballTargetPlayerIndex].playerID << "! Rolling Damage...";
			pendingFireballDamageResult = startDiceRoll(2, 6, PURPOSE_DAMAGE);
			isWaitingForFireballDamageDice = true;
		} else {
			ofLogNotice("Fireball") << "Missed! Landed on empty tile (" << fireballImpactTile.x << ", " << fireballImpactTile.y << ").";
		}
	}

	// [Keep Fireball Damage Dice Completion Logic]
	if (isWaitingForFireballDamageDice && activeDiceRolls.empty()) {
		isWaitingForFireballDamageDice = false;
		ofLogNotice("Fireball") << "Damage roll result: " << pendingFireballDamageResult;
		Player * target = getPlayer(fireballTargetPlayerIndex);
		if (target) {
			int damage = pendingFireballDamageResult;
			int initialHealth = target->health;
			int wardDamage = std::min(target->ward, damage);
			target->ward -= wardDamage;
			damage -= wardDamage;
			target->health -= damage;
			if (target->health < initialHealth) {
				target->onFire = true;
			}
		}
		fireballTargetPlayerIndex = -1;
	} // <--- Paste AFTER this closing brace

	// --- PASTE HERE ---
	if (isWaitingForSummonHealth && activeDiceRolls.empty()) {
		isWaitingForSummonHealth = false;

		// 1. Create Minion
		Player minion;
		minion.playerID = 100 + (int)players.size(); // Simple ID generation
		minion.x = (int)pendingSummonTile.x;
		minion.y = (int)pendingSummonTile.y;
		minion.maxHealth = pendingSummonRollResult; // Result of the dice roll
		minion.health = pendingSummonRollResult;
		minion.isMinion = true;
		minion.isSkeleton = true;
		minion.hasRegeneration = true;
		minion.ownerID = players[currentPlayerIndex].playerID;

		// 2. Build Minion Deck
		for (const auto & c : allCards) {
			if (c.name == "Punch") {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
			if (c.name == "Hand Block") {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
		}
		std::shuffle(minion.deck.begin(), minion.deck.end(), rng);

		// 3. Graveyard Interaction
		int gIndex = -1;
		for (size_t i = 0; i < graveyard.size(); i++) {
			if (graveyard[i].x == minion.x && graveyard[i].y == minion.y) {
				if (globalTurnCounter - graveyard[i].turnDied <= 1) {
					gIndex = i;
					break;
				}
			}
		}

		if (gIndex != -1) {
			if (!graveyard[gIndex].deck.empty()) {
				int r = (int)ofRandom(0, graveyard[gIndex].deck.size());
				minion.deck.push_back(graveyard[gIndex].deck[r]);
				ofLogNotice("Raise Dead") << "Looted a card from the grave!";
			}
			graveyard.erase(graveyard.begin() + gIndex);
		}

		// 4. Add to Board
		board[minion.x][minion.y].hasPlayer = true;
		players.push_back(minion);

		// 5. SORT TURN ORDER
		int currentID = players[currentPlayerIndex].playerID;
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.playerID < b.playerID;
		});

		// 6. Fix CurrentPlayerIndex
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == currentID) {
				currentPlayerIndex = i;
				break;
			}
		}

		ofLogNotice("Raise Dead") << "Skeleton risen with " << minion.health << " HP.";
		invalidateTargetCache();
	}

	// --- ETHEREAL JOLT RESOLUTION ---
	if (isWaitingForJoltRangeDice && activeDiceRolls.empty()) {
		isWaitingForJoltRangeDice = false;
		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile = { (float)caster.x, (float)caster.y };

		// 1. Calculate Max Range (5ft = 1.0 Unit)
		float maxDistUnits = pendingJoltRangeResult / 5.0f;

		// 2. Calculate Required Distance (IGNORING WALLS)
		// FIX: Do not use getFaceToFaceDistance here, because it checks for wall blocking.
		// Jolt goes through walls, so we use pure Euclidean Edge-to-Edge distance.
		// Distance = Center-to-Center minus 1.0 (The two half-tiles of radius)
		float centerDist = glm::distance(casterTile, pendingJoltTargetTile);
		float neededDist = std::max(0.0f, centerDist - 1.0f);

		int requiredFeet = (int)ceil(neededDist * 5.0f);

		ofLogNotice("Jolt") << "Rolled: " << pendingJoltRangeResult << "ft (" << maxDistUnits << "). Needed: " << requiredFeet << "ft.";

		if (maxDistUnits >= neededDist - 0.001f) {
			ofLogNotice("Jolt") << "Target Reached!";

			// Find Target
			Player * target = nullptr;
			for (auto & p : players) {
				if (p.x == (int)pendingJoltTargetTile.x && p.y == (int)pendingJoltTargetTile.y) {
					target = &p;
					break;
				}
			}

			if (target) {
				// Effect 1: Deal 7 Magic Damage
				int damage = 7;
				int barrierDmg = std::min(target->barrier, damage);
				target->barrier -= barrierDmg;
				damage -= barrierDmg;

				if (damage > 0) {
					int wardDmg = std::min(target->ward, damage);
					target->ward -= wardDmg;
					damage -= wardDmg;
				}

				if (damage > 0) target->health -= damage;
				ofLogNotice("Jolt") << "Dealt Damage. Health now: " << target->health;

				// Effect 2: Paralyze
				target->isParalyzed = true;
				target->paralysisHeadsCount = 0;
				ofLogNotice("Jolt") << "Target Paralyzed.";

				// Effect 3: Mill Top Card
				if (!target->deck.empty()) {
					target->deck.pop_back();
					ofLogNotice("Jolt") << "Target's top card removed.";
				}
			}
		} else {
			ofLogNotice("Jolt") << "Fell short! (Rolled " << pendingJoltRangeResult << "ft, needed " << requiredFeet << "ft)";
		}
	}

	// --- HEAL RESOLUTION ---
	if (isWaitingForHealDice && activeDiceRolls.empty()) {
		isWaitingForHealDice = false;

		Player * target = getPlayer(pendingHealTargetIndex);
		if (target) {
			int healAmount = pendingHealRollResult;

			// Apply Heal
			target->health += healAmount;

			// Cap at Max Health
			if (target->health > target->maxHealth) {
				target->health = target->maxHealth;
			}

			ofLogNotice("Heal") << "Player " << target->playerID
								<< " healed for " << healAmount
								<< ". Current HP: " << target->health;
		}
		pendingHealTargetIndex = -1;
	}

	// --- Dispel Barrier Dice ---
	if (isWaitingForBarrierDice && activeDiceRolls.empty()) {
		isWaitingForBarrierDice = false;
		Player & p = players[currentPlayerIndex];
		p.barrier += pendingDispelRollResult;
		ofLogNotice("Dispel") << "Gained " << pendingDispelRollResult << " Barrier.";
	}

	// --- Teleport Logic ---
	if (isWaitingForTeleportDice && activeDiceRolls.empty()) {
		isWaitingForTeleportDice = false;
		Player & p = players[currentPlayerIndex];
		glm::vec2 startPos = glm::vec2(p.x, p.y);

		float maxDistUnits = pendingTeleportRollResult / 5.0f;
		float distUnits = getFaceToFaceDistance(startPos, pendingTeleportTarget);
		int requiredFeet = (distUnits > 1000.0f) ? 999 : (int)ceil(distUnits * 5.0f);

		ofLogNotice("Teleport") << "Rolled: " << pendingTeleportRollResult << "ft. Required: " << requiredFeet << "ft.";

		if (maxDistUnits >= distUnits - 0.001f) {
			// ... (Success logic) ...
			ofLogNotice("Teleport") << "Success!";
			board[p.x][p.y].hasPlayer = false;
			p.x = (int)pendingTeleportTarget.x;
			p.y = (int)pendingTeleportTarget.y;
			board[p.x][p.y].hasPlayer = true;
			playerVisualPos = gridToWorld(p.x, p.y);
			invalidateTargetCache();
		} else {
			ofLogNotice("Teleport") << "Failed! Range too short or Blocked.";
		}
	}

	// --- On Fire Logic ---
	if (isWaitingForOnFireDice && activeDiceRolls.empty()) {
		isWaitingForOnFireDice = false;
		int rollResult = pendingOnFireRollResult;
		Player & burningPlayer = players[currentPlayerIndex];
		burningPlayer.health -= rollResult;
		if (rollResult == 1 || rollResult == 2) burningPlayer.onFire = false;
		continueNewTurn();
	}

	// --- CRITICAL FIX: DICE ROLL & ANIMATION UPDATES ---
	for (auto it = activeDiceRolls.begin(); it != activeDiceRolls.end();) {
		DiceRoll & roll = *it;
		float elapsedTime = ofGetElapsedTimef() - roll.startTime;
		float spinDuration = 1.0f;
		float hangTime = 1.0f;
		roll.currentRotation += diceSpinSpeed * ofGetLastFrameTime();

		if (elapsedTime > spinDuration && !roll.isFinishedVisual) {
			roll.isFinishedVisual = true;

			// CORRECTED LOGIC: Check for DEBUG first. If it's not a debug roll,
			// THEN execute all the game-related logic inside this block.
			if (roll.purpose != PURPOSE_DEBUG && roll.purpose != PURPOSE_HP && roll.purpose != PURPOSE_HEALING) {
				if (roll.purpose == PURPOSE_AP) {
					currentAP = roll.result;
					if (players[currentPlayerIndex].nextTurnAPBonus > 0) {
						currentAP += players[currentPlayerIndex].nextTurnAPBonus;
						players[currentPlayerIndex].nextTurnAPBonus = 0;
					}
					ofLogNotice("Game") << "AP Roll Finished: " << currentAP << " AP awarded.";
				} else if (roll.purpose == PURPOSE_COIN_FLIP) {
					isWaitingForParalysisCoin = false;
					int flipResult = roll.result;
					Player & p = players[currentPlayerIndex];

					// Check for 2 (Heads) to escape paralysis
					if (flipResult == 2) {
						ofLogNotice("Paralysis") << "Heads! Paralysis is cured.";
						p.isParalyzed = false;
						p.paralysisHeadsCount = 0;

						// Continue turn...
						if (p.onFire) {
							isWaitingForOnFireDice = true;
							pendingOnFireRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE);
						} else {
							continueNewTurn();
						}
						return;
					} else { // Rolled 1 (Tails)
						ofLogNotice("Paralysis") << "Tails! Player remains paralyzed.";
						// End turn immediately
						startNewTurn();
						return;
					}
				}
			}
		}

		// This part correctly removes the dice after their hang time.
		if (elapsedTime > spinDuration + hangTime) {
			it = activeDiceRolls.erase(it);
		} else {
			++it;
		}
	}

	// --- Animation Updates ---
	for (auto & anim : activeStolenCardAnimations) {
		float elapsedTime = ofGetElapsedTimef() - anim.startTime;
		if (elapsedTime < 0.8f) {
			float t = ofMap(elapsedTime, 0, 0.8f, 0.0, 1.0, true);
			anim.currentPos = glm::mix(glm::vec2(cam.worldToScreen(anim.startPos)), anim.targetPos, t);
			anim.currentScale = ofLerp(0.1f, 3.0f, t);
			anim.currentAlpha = ofLerp(0, 255, t);
		} else {
			anim.currentPos = anim.targetPos;
			anim.currentScale = 3.0f;
			anim.currentAlpha = 255;
		}
	}
	activeStolenCardAnimations.erase(std::remove_if(activeStolenCardAnimations.begin(), activeStolenCardAnimations.end(), [](const StolenCardAnimation & anim) { return (ofGetElapsedTimef() - anim.startTime) >= 3.3f; }), activeStolenCardAnimations.end());

	for (auto & anim : activeRemovedCardAnimations) {
		float elapsedTime = ofGetElapsedTimef() - anim.startTime;
		if (elapsedTime < 0.5f) {
			float t = elapsedTime / 0.5f;
			anim.currentScale = ofLerp(1.6f, 0.1f, t);
			anim.currentAlpha = ofLerp(255, 0, t);
		}
	}
	activeRemovedCardAnimations.erase(std::remove_if(activeRemovedCardAnimations.begin(), activeRemovedCardAnimations.end(), [](const RemovedCardAnimation & anim) { return (ofGetElapsedTimef() - anim.startTime) >= 0.5f; }), activeRemovedCardAnimations.end());

	// Card Hand Animation
	if (!players.empty() && currentPlayerIndex >= 0) {
		Player & currentPlayer = players[currentPlayerIndex];

		// FIX: Determine Y position based on ownership (P1 vs P2)
		// If it is Player 1 OR P1's Minion, draw hand at bottom. Otherwise top.
		bool isP1 = (currentPlayer.playerID == 0 || currentPlayer.ownerID == 0);
		float handCenterY = isP1 ? ofGetHeight() - 130 : 130;

		float handBaseCardWidth = 120;
		float handAreaWidth = ofGetWidth() * 0.4f;

		size_t numCards = currentPlayer.hand.size();
		float totalCardWidths = numCards * handBaseCardWidth;
		float padding = (numCards > 1) ? (handAreaWidth - totalCardWidths) / (numCards - 1) : 0;
		padding = std::min(padding, 20.0f);
		float totalHandWidth = (numCards * handBaseCardWidth) + ((numCards - 1) * padding);
		float startX = (ofGetWidth() - totalHandWidth) / 2.0f;

		for (size_t i = 0; i < numCards; i++) {
			float cardCenterX = startX + i * (handBaseCardWidth + padding) + (handBaseCardWidth / 2.0f);
			currentPlayer.hand[i].targetPos = ofVec2f(cardCenterX, handCenterY);

			if (i != draggedCardIndex) {
				currentPlayer.hand[i].currentScale = ofLerp(currentPlayer.hand[i].currentScale, currentPlayer.hand[i].targetScale, 0.25f);
				currentPlayer.hand[i].currentPos = currentPlayer.hand[i].currentPos.getInterpolated(currentPlayer.hand[i].targetPos, 0.25f);
			}
		}
	}

	if (isPlayerAnimating) {
		glm::vec3 targetPos = animationPath[currentPathIndex];
		float player_speed = 1.0 - pow(0.65, deltaTime * 60.0);
		playerVisualPos = glm::mix(playerVisualPos, targetPos, player_speed);

		// Check if unit arrived at the center of the tile (Distance < 0.05)
		if (glm::distance(playerVisualPos, targetPos) < 0.05f) {
			playerVisualPos = targetPos;
			currentPathIndex++;

			// --- PLAY RANDOM FOOTSTEP ---
			// Only play if we are moving to another tile (not the final destination)
			// and we successfully loaded sounds.
			if (currentPathIndex < animationPath.size() && !footstepSounds.empty()) {

				// Pick a random index from 0 to 5
				int idx = (int)ofRandom(0, footstepSounds.size());

				// Slight pitch variation (0.9 to 1.1) makes it sound more natural/less robotic
				footstepSounds[idx].setSpeed(ofRandom(0.9f, 1.1f));
				footstepSounds[idx].play();
			}
			// ---------------------------

			if (currentPathIndex >= static_cast<int>(animationPath.size())) isPlayerAnimating = false;
		}
	}

	const float cardDisplayDuration = 2.5f;
	while (!activeCardDisplays.empty() && (ofGetElapsedTimef() - activeCardDisplays.front().startTime > cardDisplayDuration)) {
		activeCardDisplays.erase(activeCardDisplays.begin());
	}

	if (hasUnlimitedAP) currentAP = 99;
}

//----------------------------------------------------
void ofApp::buildLevelMesh() {
	levelMesh.clear();
	levelMesh.setMode(OF_PRIMITIVE_TRIANGLES);

	// 1. Settings
	float size = TILE_SIZE;
	float half = size / 2.0f;

	// --- CHANGE IS HERE ---
	float height = TILE_SIZE * 0.5f; // 0.5f = Half Height. Try 0.3f for low walls, 0.8f for tall.
	// ----------------------

	// 2. Helper to add a 3D Block
	auto addCube = [&](float x, float y, float z) {
		int idx = levelMesh.getNumVertices();

		// Coordinates relative to center
		// Base is at y=0, Top is at y=height
		glm::vec3 p1(-half, height, -half); // Top Left Back
		glm::vec3 p2(half, height, -half); // Top Right Back
		glm::vec3 p3(half, height, half); // Top Right Front
		glm::vec3 p4(-half, height, half); // Top Left Front

		glm::vec3 p5(-half, 0, -half); // Bot Left Back
		glm::vec3 p6(half, 0, -half); // Bot Right Back
		glm::vec3 p7(half, 0, half); // Bot Right Front
		glm::vec3 p8(-half, 0, half); // Bot Left Front

		glm::vec3 offset(x, y, z);

		// Standard UVs
		glm::vec2 t00(0, 0), t10(1, 0), t11(1, 1), t01(0, 1);

		// --- TOP FACE ---
		levelMesh.addVertex(p1 + offset);
		levelMesh.addTexCoord(t00);
		levelMesh.addNormal({ 0, 1, 0 });
		levelMesh.addVertex(p2 + offset);
		levelMesh.addTexCoord(t10);
		levelMesh.addNormal({ 0, 1, 0 });
		levelMesh.addVertex(p3 + offset);
		levelMesh.addTexCoord(t11);
		levelMesh.addNormal({ 0, 1, 0 });
		levelMesh.addVertex(p4 + offset);
		levelMesh.addTexCoord(t01);
		levelMesh.addNormal({ 0, 1, 0 });
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 1);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx + 3);
		idx += 4;

		// --- NORTH FACE ---
		levelMesh.addVertex(p2 + offset);
		levelMesh.addTexCoord(t00);
		levelMesh.addNormal({ 0, 0, -1 });
		levelMesh.addVertex(p1 + offset);
		levelMesh.addTexCoord(t10);
		levelMesh.addNormal({ 0, 0, -1 });
		levelMesh.addVertex(p5 + offset);
		levelMesh.addTexCoord(t11);
		levelMesh.addNormal({ 0, 0, -1 });
		levelMesh.addVertex(p6 + offset);
		levelMesh.addTexCoord(t01);
		levelMesh.addNormal({ 0, 0, -1 });
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 1);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx + 3);
		idx += 4;

		// --- SOUTH FACE ---
		levelMesh.addVertex(p4 + offset);
		levelMesh.addTexCoord(t00);
		levelMesh.addNormal({ 0, 0, 1 });
		levelMesh.addVertex(p3 + offset);
		levelMesh.addTexCoord(t10);
		levelMesh.addNormal({ 0, 0, 1 });
		levelMesh.addVertex(p7 + offset);
		levelMesh.addTexCoord(t11);
		levelMesh.addNormal({ 0, 0, 1 });
		levelMesh.addVertex(p8 + offset);
		levelMesh.addTexCoord(t01);
		levelMesh.addNormal({ 0, 0, 1 });
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 1);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx + 3);
		idx += 4;

		// --- EAST FACE ---
		levelMesh.addVertex(p3 + offset);
		levelMesh.addTexCoord(t00);
		levelMesh.addNormal({ 1, 0, 0 });
		levelMesh.addVertex(p2 + offset);
		levelMesh.addTexCoord(t10);
		levelMesh.addNormal({ 1, 0, 0 });
		levelMesh.addVertex(p6 + offset);
		levelMesh.addTexCoord(t11);
		levelMesh.addNormal({ 1, 0, 0 });
		levelMesh.addVertex(p7 + offset);
		levelMesh.addTexCoord(t01);
		levelMesh.addNormal({ 1, 0, 0 });
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 1);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx + 3);
		idx += 4;

		// --- WEST FACE ---
		levelMesh.addVertex(p1 + offset);
		levelMesh.addTexCoord(t00);
		levelMesh.addNormal({ -1, 0, 0 });
		levelMesh.addVertex(p4 + offset);
		levelMesh.addTexCoord(t10);
		levelMesh.addNormal({ -1, 0, 0 });
		levelMesh.addVertex(p8 + offset);
		levelMesh.addTexCoord(t11);
		levelMesh.addNormal({ -1, 0, 0 });
		levelMesh.addVertex(p5 + offset);
		levelMesh.addTexCoord(t01);
		levelMesh.addNormal({ -1, 0, 0 });
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 1);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx);
		levelMesh.addIndex(idx + 2);
		levelMesh.addIndex(idx + 3);
	};

	// 3. Loop through board
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			if (board[x][y].hasWall) {
				glm::vec3 pos = gridToWorld(x, y);
				addCube(pos.x, 0, pos.z);
			}
		}
	}
}
//--------------------------------------------------------------
void ofApp::buildFloorMesh() {
	// 1. Clear all floor meshes
	for (auto & mesh : floorMeshes) {
		mesh.clear();
		mesh.setMode(OF_PRIMITIVE_TRIANGLES);
	}

	if (floorTextures.empty()) return;

	float size = TILE_SIZE;
	float half = size / 2.0f;
	float thickness = 1.0f;

	// Helper to add a block (Rotation removed, standard UVs used)
	auto addBlock = [&](ofMesh & mesh, float x, float y, float z) {
		int idx = mesh.getNumVertices();

		// Geometry Coordinates
		glm::vec3 p1(-half, 0, -half); // Top Left Back
		glm::vec3 p2(half, 0, -half); // Top Right Back
		glm::vec3 p3(half, 0, half); // Top Right Front
		glm::vec3 p4(-half, 0, half); // Top Left Front

		glm::vec3 p5(-half, -thickness, -half);
		glm::vec3 p6(half, -thickness, -half);
		glm::vec3 p7(half, -thickness, half);
		glm::vec3 p8(-half, -thickness, half);

		glm::vec3 offset(x, y, z);

		// Standard UVs (0 to 1) for Non-Rotated Textures
		glm::vec2 t00(0, 0), t10(1, 0), t11(1, 1), t01(0, 1);

		// --- TOP FACE (The textured part) ---
		mesh.addVertex(p1 + offset);
		mesh.addTexCoord(t00);
		mesh.addNormal({ 0, 1, 0 });
		mesh.addVertex(p2 + offset);
		mesh.addTexCoord(t10);
		mesh.addNormal({ 0, 1, 0 });
		mesh.addVertex(p3 + offset);
		mesh.addTexCoord(t11);
		mesh.addNormal({ 0, 1, 0 });
		mesh.addVertex(p4 + offset);
		mesh.addTexCoord(t01);
		mesh.addNormal({ 0, 1, 0 });

		mesh.addIndex(idx + 0);
		mesh.addIndex(idx + 1);
		mesh.addIndex(idx + 2);
		mesh.addIndex(idx + 0);
		mesh.addIndex(idx + 2);
		mesh.addIndex(idx + 3);
		idx += 4;

		// --- SIDES ---
		auto addSide = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
			mesh.addVertex(a + offset);
			mesh.addTexCoord(t00);
			mesh.addNormal(n);
			mesh.addVertex(b + offset);
			mesh.addTexCoord(t10);
			mesh.addNormal(n);
			mesh.addVertex(c + offset);
			mesh.addTexCoord(t11);
			mesh.addNormal(n);
			mesh.addVertex(d + offset);
			mesh.addTexCoord(t01);
			mesh.addNormal(n);
			mesh.addIndex(idx);
			mesh.addIndex(idx + 1);
			mesh.addIndex(idx + 2);
			mesh.addIndex(idx);
			mesh.addIndex(idx + 2);
			mesh.addIndex(idx + 3);
			idx += 4;
		};
		addSide(p2, p1, p5, p6, { 0, 0, -1 }); // North
		addSide(p4, p3, p7, p8, { 0, 0, 1 }); // South
		addSide(p3, p2, p6, p7, { 1, 0, 0 }); // East
		addSide(p1, p4, p8, p5, { -1, 0, 0 }); // West
	};

	// 2. Loop through board
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {

			// Deterministic Randomness based on coordinate
			unsigned int seed = (x * 73856093) ^ (y * 19349663);
			std::mt19937 tileRng(seed);

			// Pick Random Texture Index (0 to 5)
			std::uniform_int_distribution<int> texDist(0, (int)floorTextures.size() - 1);
			int texIndex = texDist(tileRng);

			glm::vec3 pos = gridToWorld(x, y);

			// Add block (No rotation passed)
			addBlock(floorMeshes[texIndex], pos.x, 0, pos.z);
		}
	}
}
//-----------------------------
void ofApp::drawGame() {
	// Render the 3D world (and 3D highlights) into an offscreen buffer so we can post-process it
	// without affecting the 2D UI.
	auto renderWorld3D = [&]() {
		static bool loggedWorldStats = false;
		if (!loggedWorldStats) {
			ofLogNotice("World")
				<< "players=" << players.size()
				<< " floorMeshes=" << floorMeshes.size()
				<< " floorTextures=" << floorTextures.size()
				<< " levelMeshVerts=" << levelMesh.getNumVertices();
			loggedWorldStats = true;
		}

		// --- 1. SETUP RENDER STATES ---
		ofEnableDepthTest();
		ofDisableAlphaBlending();
		ofDisableBlendMode();
		ofSetColor(255);

		cam.begin();

		// --- 3. LIGHTING ---
		ofEnableLighting();

		// Explicitly enable your defined lights
		keyLight.enable();
		rimLight.enable();
		headlight.enable(); // Camera torch

		// If you still have other lights in the vector:
		for (auto & light : lights) {
			light.enable();
		}

		// Update Headlight Position to follow camera
		headlight.setPosition(cam.getPosition());

		// --- 4. OPAQUE GEOMETRY (Floor & Walls) ---
		for (size_t i = 0; i < floorMeshes.size(); i++) {
			if (i < floorTextures.size()) {
				floorTextures[i].bind();
				floorMeshes[i].draw();
				floorTextures[i].unbind();
			}
		}
		wallTexture.bind();
		levelMesh.draw();
		wallTexture.unbind();

		// Inside renderWorld3D, AFTER drawing floor/walls but BEFORE drawing players:

		ofDisableLighting(); // Shadows don't need to be lit
		ofEnableAlphaBlending();
		ofSetColor(255); // Reset color for texture drawing

		for (const auto & player : players) {
			ofPushMatrix();

			// Position based on animation or grid
			glm::vec3 drawPos;
			if (currentPlayerIndex >= 0 && player.playerID == players[currentPlayerIndex].playerID) {
				drawPos = playerVisualPos;
			} else {
				drawPos = gridToWorld(player.x, player.y);
			}

			// Draw slightly above floor (y=0.02) to avoid z-fighting
			ofTranslate(drawPos.x, 0.02f, drawPos.z);
			ofRotateXDeg(90); // Lay flat on floor

			// Scale shadow based on unit type (D20 vs Player)
			float shadowSize = TILE_SIZE * 0.8f;

			shadowTexture.draw(-shadowSize / 2, -shadowSize / 2, shadowSize, shadowSize);

			ofPopMatrix();
		}
		ofEnableLighting(); // Turn lighting back on for models
		diceMaterial.setShininess(120); // Very shiny
		diceMaterial.setSpecularColor(ofColor(255, 255, 255)); // Sharp white highlights

		// --- 5. OPAQUE DYNAMIC OBJECTS (Players & Dice) ---
		for (const auto & player : players) {
			ofSetColor(255);
			ofPushMatrix();
			if (currentPlayerIndex >= 0 && player.playerID == players[currentPlayerIndex].playerID) {
				ofTranslate(playerVisualPos.x, playerVisualPos.y, playerVisualPos.z);
			} else {
				glm::vec3 staticPos = gridToWorld(player.x, player.y);
				ofTranslate(staticPos.x, staticPos.y, staticPos.z);
			}
			if (player.isSkeleton) {
				ofSetColor(255);
				ofTranslate(0, 2.5f, 0);
				skeletonTexture.bind();
				skeletonModel.drawFaces();
				skeletonTexture.unbind();
			} else {
				ofTranslate(0, 0.1f, 0);
				playerModel.drawFaces();
			}
			ofPopMatrix();
		}

		// Dice (Batched)
		diceMaterial.begin();

		// --- Coin ---
		coinFacesTexture.bind();
		for (auto & roll : activeDiceRolls) {
			if (roll.sides != 2) continue;
			ofPushMatrix();
			int idx = &roll - &activeDiceRolls[0]; // Safer way to get index
			float xOffset = (idx * 4.0f) - ((activeDiceRolls.size() - 1) * 2.0f);

			// --- FIX 1: Increased height from 2.0 to 4.5 ---
			ofTranslate(xOffset, 4.5f, 0);

			glm::quat finalDrawQuat;
			float t = (ofGetElapsedTimef() - roll.startTime);
			if (t < 1.0f) {
				float t_ease = 1.0f - pow(1.0f - t, 4.0f);
				float remainingSpin = (1.0f - t_ease) * 1080.0f;
				glm::quat spin = glm::angleAxis(glm::radians(remainingSpin), roll.rotationAxis);
				finalDrawQuat = spin * roll.finalQuat;
			} else {
				finalDrawQuat = roll.finalQuat;
			}
			ofMultMatrix(glm::toMat4(finalDrawQuat));
			coinMesh.draw();
			ofPopMatrix();
		}
		coinFacesTexture.unbind();

		// --- D6 ---
		d6Texture.bind();
		for (auto & roll : activeDiceRolls) {
			if (roll.sides != 6) continue;
			ofPushMatrix();
			int idx = &roll - &activeDiceRolls[0];
			float xOffset = (idx * 4.0f) - ((activeDiceRolls.size() - 1) * 2.0f);

			// --- FIX 1: Increased height from 2.0 to 4.5 ---
			ofTranslate(xOffset, 4.5f, 0);

			glm::quat finalDrawQuat;
			float t = (ofGetElapsedTimef() - roll.startTime);
			if (t < 1.0f) {
				float t_ease = 1.0f - pow(1.0f - t, 4.0f);
				float remainingSpin = (1.0f - t_ease) * 550.0f;
				glm::quat spin = glm::angleAxis(glm::radians(remainingSpin), roll.rotationAxis);
				finalDrawQuat = spin * roll.finalQuat;
			} else {
				finalDrawQuat = roll.finalQuat;
			}

			ofMultMatrix(glm::toMat4(finalDrawQuat));
			ofScale(1.2f, 1.2f, 1.2f);
			d6Mesh.draw();
			ofPopMatrix();
		}
		d6Texture.unbind();

		// --- D4 ---
		d4Texture.bind();
		for (auto & roll : activeDiceRolls) {
			if (roll.sides != 4) continue;

			ofPushMatrix();
			int idx = &roll - &activeDiceRolls[0];
			float xOffset = (idx * 4.0f) - ((activeDiceRolls.size() - 1) * 2.0f);

			// --- FIX 1: Increased height from 2.0 to 4.5 ---
			ofTranslate(xOffset, 4.5f, 0);

			glm::quat finalDrawQuat;
			float t = (ofGetElapsedTimef() - roll.startTime);
			if (t < 1.0f) {
				float t_ease = 1.0f - pow(1.0f - t, 4.0f);
				float remainingSpin = (1.0f - t_ease) * 550.0f;
				glm::quat spin = glm::angleAxis(glm::radians(remainingSpin), roll.rotationAxis);
				finalDrawQuat = spin * roll.finalQuat;
			} else {
				finalDrawQuat = roll.finalQuat;
			}

			ofMultMatrix(glm::toMat4(finalDrawQuat));
			ofScale(2.2f, 2.2f, 2.2f);
			d4Mesh.draw();
			ofPopMatrix();
		}
		d4Texture.unbind();

		// --- D10 ---
		d10Texture.bind();
		for (auto & roll : activeDiceRolls) {
			if (roll.sides != 10) continue;
			ofPushMatrix();
			int idx = &roll - &activeDiceRolls[0];
			float xOffset = (idx * 4.0f) - ((activeDiceRolls.size() - 1) * 2.0f);

			// --- FIX 1: Increased height from 2.0 to 4.5 ---
			ofTranslate(xOffset, 4.5f, 0);

			glm::quat finalDrawQuat;
			float t = (ofGetElapsedTimef() - roll.startTime);
			if (t < 1.0f) {
				float t_ease = 1.0f - pow(1.0f - t, 4.0f);
				float remainingSpin = (1.0f - t_ease) * 550.0f;
				glm::quat spin = glm::angleAxis(glm::radians(remainingSpin), roll.rotationAxis);
				finalDrawQuat = spin * roll.finalQuat;
			} else {
				finalDrawQuat = roll.finalQuat;
			}

			ofMultMatrix(glm::toMat4(finalDrawQuat));
			ofScale(2.1f, 2.1f, 2.1f);
			d10Mesh.draw();
			ofPopMatrix();
		}
		d10Texture.unbind();

		// --- D20 ---
		d20Texture.bind();
		for (auto & roll : activeDiceRolls) {
			if (roll.sides != 20) continue;
			ofPushMatrix();
			int idx = &roll - &activeDiceRolls[0];
			float xOffset = (idx * 4.0f) - ((activeDiceRolls.size() - 1) * 2.0f);

			// --- FIX 1: Increased height from 2.0 to 4.5 ---
			ofTranslate(xOffset, 4.5f, 0);

			glm::quat finalDrawQuat;
			float t = (ofGetElapsedTimef() - roll.startTime);
			if (t < 1.0f) {
				float t_ease = 1.0f - pow(1.0f - t, 4.0f);
				float remainingSpin = (1.0f - t_ease) * 550.0f;
				glm::quat spin = glm::angleAxis(glm::radians(remainingSpin), roll.rotationAxis);
				finalDrawQuat = spin * roll.finalQuat;
			} else {
				finalDrawQuat = roll.finalQuat;
			}

			ofNode d20Node;
			d20Node.setOrientation(finalDrawQuat);
			ofMultMatrix(d20Node.getGlobalTransformMatrix());
			ofScale(2.4f, 2.4f, 2.4f);
			d20Mesh.draw();
			ofPopMatrix();
		}
		d20Texture.unbind();

		diceMaterial.end();

		// --- 6. TRANSPARENT GEOMETRY (Highlights) ---
		ofEnableAlphaBlending();

		for (int x = 0; x < BOARD_WIDTH; x++) {
			for (int y = 0; y < BOARD_HEIGHT; y++) {
				glm::vec3 tileWorldPos = gridToWorld(x, y);
				ofPushMatrix();
				ofTranslate(tileWorldPos.x, 0, tileWorldPos.z);

				if (board[x][y].isHighlighted) {
					bool isOnPath = false;
					for (const auto & step : hoverPath) {
						if (step.x == x && step.y == y) {
							isOnPath = true;
							break;
						}
					}
					if (!isOnPath) {
						ofDisableLighting();
						ofSetColor(ofColor::yellow, 102);
						ofPushMatrix();
						ofTranslate(0, 0.05f, 0);
						ofRotateXDeg(90);
						ofDrawCircle(0, 0, TILE_SIZE * 0.30f);
						ofPopMatrix();
						ofEnableLighting();
					}
				}

				// Target Highlights
				bool isLosCardSelected = false;
				bool isLosCardHovered = false;
				// --- FIX: Removed unused 'cardToCheck' variable ---

				if (!players.empty() && currentPlayerIndex >= 0 && selectedCardIndex >= 0 && static_cast<size_t>(selectedCardIndex) < players[currentPlayerIndex].hand.size()) {
					CardType type = players[currentPlayerIndex].hand[selectedCardIndex].type;
					if (type == CARD_MAGIC_BLAST || type == CARD_FIREBALL) {
						isLosCardSelected = true;
					}
				} else if (!players.empty() && currentPlayerIndex >= 0 && hoveredCardIndex >= 0 && static_cast<size_t>(hoveredCardIndex) < players[currentPlayerIndex].hand.size()) {
					CardType type = players[currentPlayerIndex].hand[hoveredCardIndex].type;
					if (type == CARD_MAGIC_BLAST || type == CARD_FIREBALL) {
						isLosCardHovered = true;
					}
				}

				if (isLosCardSelected || isLosCardHovered) {
					TargetInfo info = targetCache[x][y];
					if (info.reason == VALID) {
						ofColor highlightColor;
						bool shouldDraw = false;
						if (info.isTargetable) {
							highlightColor = ofColor(255, 0, 0, 180);
							shouldDraw = true;
						} else if (isLosCardSelected) {
							highlightColor = ofColor(0, 255, 0, 150);
							shouldDraw = true;
						}
						if (shouldDraw) {
							ofDisableLighting();
							ofSetColor(highlightColor);
							ofNoFill();
							ofSetLineWidth(3);
							ofPushMatrix();
							ofTranslate(0, 0.06f, 0);
							ofRotateXDeg(90);
							ofDrawRectangle(-TILE_SIZE * 0.45f, -TILE_SIZE * 0.45f, TILE_SIZE * 0.9f, TILE_SIZE * 0.9f);
							ofPopMatrix();
							ofFill();
							ofSetLineWidth(1);
							ofEnableLighting();
						}
					}
				} else if (board[x][y].isTargetable) {
					ofDisableLighting();
					ofSetColor(ofColor::red, 180);
					ofNoFill();
					ofSetLineWidth(3);
					ofPushMatrix();
					ofTranslate(0, 0.06f, 0);
					ofRotateXDeg(90);
					ofDrawRectangle(-TILE_SIZE * 0.4f, -TILE_SIZE * 0.4f, TILE_SIZE * 0.8f, TILE_SIZE * 0.8f);
					ofPopMatrix();
					ofFill();
					ofSetLineWidth(1);
					ofEnableLighting();
				}

				if (!players.empty() && currentPlayerIndex >= 0 && x == players[currentPlayerIndex].x && y == players[currentPlayerIndex].y) {
					ofDisableLighting();
					ofSetColor(ofColor::fromHex(0x9400D3));
					ofNoFill();
					ofSetLineWidth(4);
					ofPushMatrix();
					ofTranslate(0, 0.07f, 0);
					ofRotateXDeg(90);
					ofDrawCircle(0, 0, TILE_SIZE * 0.45f);
					ofPopMatrix();
					ofFill();
					ofSetLineWidth(1);
					ofEnableLighting();
				}
				ofPopMatrix();
			}
		}

		if ((playerAction == PIECE_SELECTED) && !hoverPath.empty()) {
			// --- FIX: Changed loop counter to size_t to resolve signed/unsigned warning ---
			for (size_t i = 1; i < hoverPath.size(); i++) {
				const auto & step = hoverPath[i];
				glm::vec3 pathWorldPos = gridToWorld(step.x, step.y);

				ofDisableLighting();
				ofSetColor(ofColor::green, 150);
				ofPushMatrix();
				ofTranslate(pathWorldPos.x, 0.05f, pathWorldPos.z);
				ofRotateXDeg(90);
				ofDrawCircle(0, 0, TILE_SIZE * 0.3f);
				ofPopMatrix();
				ofEnableLighting();
			}
		}

		// --- 7. TEARDOWN 3D / SETUP 2D ---
		for (auto & light : lights)
			light.disable();
		headlight.disable();
		ofDisableLighting();

		cam.end();
		ofDisableDepthTest();
	};

	const bool usePost = (enableWorldPostProcess && worldPostShaderLoaded);
	if (usePost) {
		allocateWorldFbo(ofGetWidth(), ofGetHeight());
		if (worldFbo.isAllocated()) {
			worldFbo.begin();
			// Some drivers/container stacks leave depth write disabled; glClear respects glDepthMask.
			glDepthMask(GL_TRUE);
			glClearDepth(1.0);
			glClearColor(22.0f / 255.0f, 22.0f / 255.0f, 22.0f / 255.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// If the FBO is incomplete, fall back to direct rendering.
			GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
				worldFbo.end();
				ofLogWarning("FBO") << "worldFbo incomplete (status=" << (int)fbStatus << "). Falling back to direct rendering.";
				worldPostShaderLoaded = false;
				renderWorld3D();
			} else {
				renderWorld3D();
				if (isDebugMode) {
					// Diagnostic: if you see this marker, the FBO -> postprocess path is working.
					// If you see it but still no board/units, the issue is inside the 3D render pass.
					ofDisableDepthTest();
					ofDisableLighting();
					ofSetColor(255, 0, 0, 255);
					ofDrawRectangle(20, 20, 220, 40);
				}
				worldFbo.end();

				// 2D draw setup for presenting the world
				ofDisableAlphaBlending();
				ofDisableBlendMode();
				ofSetColor(255);
				ofDisableDepthTest();
				ofPushView();
				ofViewport(0, 0, ofGetWidth(), ofGetHeight(), false);
				ofSetupScreenOrtho(ofGetWidth(), ofGetHeight(), -1, 1);

				if (showWorldFboPreview) {
					// Raw FBO debug (no shader)
					worldFbo.getTexture().draw(0, 0, ofGetWidth(), ofGetHeight());
				} else {
					worldPostShader.begin();
					worldPostShader.setUniformTexture("tex0", worldFbo.getTexture(), 0);
					worldPostShader.setUniform2f("uResolution", ofGetWidth(), ofGetHeight());
					// Draw using OF's texture quad (works on your setup in preview mode)
					worldFbo.getTexture().draw(0, 0, ofGetWidth(), ofGetHeight());
					worldPostShader.end();
				}

				ofPopView();
			}
		} else {
			renderWorld3D();
		}
	} else {
		renderWorld3D();
	}

	ofEnableAlphaBlending();

	// --- 8. DRAW UI ---
	float designHeight = 1080.0f;
	float scale = ofGetHeight() / designHeight;
	float fontScale = scale * 0.5f;

	float handBaseCardWidth = 120;
	float handCardAspectRatio = 585.0f / 409.0f;
	float baseCardHeight = handBaseCardWidth * handCardAspectRatio;
	float staticUICardWidth = (handBaseCardWidth * 1.3f) * scale;
	float staticUICardHeight = (baseCardHeight * 1.3f) * scale;

	auto drawHealthBar = [&](Player & player, float x, float y, ofColor healthColor) {
		float healthBarWidth = 220 * scale;
		float healthBarHeight = 65 * scale;
		float nextBarY = y;

		// 1. Health Bar
		ofSetColor(healthColor.getLerped(ofColor::black, 0.5));
		ofDrawRectangle(x, y, healthBarWidth, healthBarHeight);
		float healthPercent = (float)player.health / player.maxHealth;
		ofSetColor(healthColor);
		ofDrawRectangle(x, y, healthBarWidth * healthPercent, healthBarHeight);

		// Health Text
		ofSetColor(ofColor::white);
		string healthText = ofToString(player.health) + " / " + ofToString(player.maxHealth);
		ofRectangle healthTextBox = titleFont.getStringBoundingBox(healthText, 0, 0);
		float textX = x + (healthBarWidth / 2) - (healthTextBox.width * fontScale / 2);
		float textY = y + (healthBarHeight / 2) + (healthTextBox.height * fontScale / 2);
		ofPushMatrix();
		ofTranslate(textX, textY);
		ofScale(fontScale, fontScale);
		titleFont.drawString(healthText, 0, 0);
		ofPopMatrix();

		bool isTopAligned = y < ofGetHeight() / 2;
		if (isTopAligned)
			nextBarY += healthBarHeight + (5 * scale);
		else
			nextBarY -= 5 * scale;

		// 2. Block Bar (Grey)
		if (player.block > 0) {
			float blockBarHeight = 50 * scale;
			float blockBarY = isTopAligned ? nextBarY : nextBarY - blockBarHeight;
			ofSetColor(ofColor::lightSlateGray);
			ofDrawRectangle(x, blockBarY, healthBarWidth, blockBarHeight);
			ofSetColor(ofColor::white);
			string blockText = ofToString(player.block);
			ofRectangle blockTextBox = titleFont.getStringBoundingBox(blockText, 0, 0);
			float bTextX = x + (healthBarWidth / 2) - (blockTextBox.width * fontScale / 2);
			float bTextY = blockBarY + (blockBarHeight / 2) + (blockTextBox.height * fontScale / 2);
			ofPushMatrix();
			ofTranslate(bTextX, bTextY);
			ofScale(fontScale, fontScale);
			titleFont.drawString(blockText, 0, 0);
			ofPopMatrix();

			if (isTopAligned)
				nextBarY += blockBarHeight + (5 * scale);
			else
				nextBarY -= blockBarHeight + (5 * scale);
		}

		// 3. BARRIER BAR (Now takes Ward's old color: Deep Purple/Indigo)
		if (player.barrier > 0) {
			float barrierHeight = 50 * scale;
			float barrierY = isTopAligned ? nextBarY : nextBarY - barrierHeight;

			// Old Ward Color (Indigo)
			ofSetColor(ofColor::fromHex(0x480082));
			ofDrawRectangle(x, barrierY, healthBarWidth, barrierHeight);

			ofSetColor(ofColor::white);
			string barrierText = ofToString(player.barrier);
			ofRectangle box = titleFont.getStringBoundingBox(barrierText, 0, 0);
			float bx = x + (healthBarWidth / 2) - (box.width * fontScale / 2);
			float by = barrierY + (barrierHeight / 2) + (box.height * fontScale / 2);
			ofPushMatrix();
			ofTranslate(bx, by);
			ofScale(fontScale, fontScale);
			titleFont.drawString(barrierText, 0, 0);
			ofPopMatrix();

			if (isTopAligned)
				nextBarY += barrierHeight + (5 * scale);
			else
				nextBarY -= barrierHeight + (5 * scale);
		}

		// 4. WARD BAR (Now Black)
		if (player.ward > 0) {
			float wardBarHeight = 50 * scale;
			float wardBarY = isTopAligned ? nextBarY : nextBarY - wardBarHeight;

			// New Black Color
			ofSetColor(ofColor::black);
			ofDrawRectangle(x, wardBarY, healthBarWidth, wardBarHeight);

			ofSetColor(ofColor::white);
			string wardText = ofToString(player.ward);
			ofRectangle wardTextBox = titleFont.getStringBoundingBox(wardText, 0, 0);
			float wTextX = x + (healthBarWidth / 2) - (wardTextBox.width * fontScale / 2);
			float wTextY = wardBarY + (wardBarHeight / 2) + (wardTextBox.height * fontScale / 2);
			ofPushMatrix();
			ofTranslate(wTextX, wTextY);
			ofScale(fontScale, fontScale);
			titleFont.drawString(wardText, 0, 0);
			ofPopMatrix();
		}
	};

	// --- MAIN UI DRAWING ---
	// --- FIX: Find the main players to prevent UI bugs with minions ---
	Player * player0 = nullptr;
	Player * player1 = nullptr;
	for (auto & p : players) {
		if (p.playerID == 0) player0 = &p;
		if (p.playerID == 1) player1 = &p;
	}

	if (player0 && player1) {
		// 1. Calculate positions first
		float p0_deckX = 30 * scale;
		float p0_deckY = ofGetHeight() - staticUICardHeight - (40 * scale) - staticUICardHeight - (40 * scale);
		p0_deckRect.set(p0_deckX, p0_deckY, staticUICardWidth, staticUICardHeight);

		float p0_discardX = p0_deckX;
		float p0_discardY = p0_deckY + staticUICardHeight + (40 * scale);
		p0_discardRect.set(p0_discardX, p0_discardY, staticUICardWidth, staticUICardHeight);

		float p1_discardX = ofGetWidth() - staticUICardWidth - (30 * scale);
		float p1_discardY = 40 * scale;
		p1_discardRect.set(p1_discardX, p1_discardY, staticUICardWidth, staticUICardHeight);

		float p1_deckX = p1_discardX;
		float p1_deckY = p1_discardY + staticUICardHeight + (40 * scale);
		p1_deckRect.set(p1_deckX, p1_deckY, staticUICardWidth, staticUICardHeight);

		// 2. Draw Player 0 (Bottom) UI
		drawHealthBar(*player0, ofGetWidth() - (220 * scale) - (50 * scale), ofGetHeight() - (65 * scale) - (40 * scale), ofColor::green);

		// P0 Deck
		if (!player0->deck.empty()) {
			ofSetColor(ofColor::white);
			cardBackImage.draw(p0_deckRect);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p0_deckRect, 10 * scale);
		}

		if (players[currentPlayerIndex].playerID == 0 && !hasDrawnCardsThisTurn) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::yellow);
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p0_deckRect);
			ofPopStyle();
		}

		// P0 Discard
		if (!player0->discardPile.empty()) {
			const auto & discardRect = player0->discardPile.back().textureRect;
			cardSpriteSheet.drawSubsection(p0_discardRect.x, p0_discardRect.y, p0_discardRect.width, p0_discardRect.height,
				discardRect.x, discardRect.y, discardRect.width, discardRect.height);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p0_discardRect, 10 * scale);
		}

		// 3. Draw Player 1 (Top) UI
		drawHealthBar(*player1, 40 * scale, 40 * scale, ofColor::red);

		// P1 Discard
		if (!player1->discardPile.empty()) {
			const auto & discardRect = player1->discardPile.back().textureRect;
			cardSpriteSheet.drawSubsection(p1_discardRect.x, p1_discardRect.y, p1_discardRect.width, p1_discardRect.height,
				discardRect.x, discardRect.y, discardRect.width, discardRect.height);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p1_discardRect, 10 * scale);
		}

		// P1 Deck
		if (!player1->deck.empty()) {
			ofSetColor(ofColor::white);
			cardBackImage.draw(p1_deckRect);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p1_deckRect, 10 * scale);
		}

		if (players[currentPlayerIndex].playerID == 1 && !hasDrawnCardsThisTurn) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::yellow);
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p1_deckRect);
			ofPopStyle();
		}

		// 4. Draw AP Displays (Corrected Logic)
		string p0_apText = "0 AP";
		string p1_apText = "? AP";

		if (currentPlayerIndex >= 0 && !players.empty()) {
			Player & currentPlayer = players[currentPlayerIndex];
			// If it's Player 0's turn OR one of their minions' turns
			if (currentPlayer.playerID == 0 || currentPlayer.ownerID == 0) {
				p0_apText = ofToString(currentAP) + " AP";
			}
			// If it's Player 1's turn OR one of their minions' turns
			else if (currentPlayer.playerID == 1 || currentPlayer.ownerID == 1) {
				p1_apText = ofToString(currentAP) + " AP";
			}
		}

		// Draw P0 AP Box
		float p0_apCenterX = 30 * scale + staticUICardWidth / 2;
		float p0_apCenterY = ofGetHeight() - staticUICardHeight - (40 * scale) - staticUICardHeight - (40 * scale) - 60 * scale;
		ofRectangle p0_apTextBox = titleFont.getStringBoundingBox(p0_apText, 0, 0);
		float p0_apRectWidth = (p0_apTextBox.width * fontScale) + (40 * scale);
		float p0_apRectHeight = (p0_apTextBox.height * fontScale) + (20 * scale);
		ofSetColor(0, 0, 0, 150);
		ofDrawRectRounded(p0_apCenterX - p0_apRectWidth / 2, p0_apCenterY - p0_apRectHeight / 2, p0_apRectWidth, p0_apRectHeight, 10 * scale);
		ofSetColor(ofColor::cyan);
		ofPushMatrix();
		ofTranslate(p0_apCenterX, p0_apCenterY);
		ofScale(fontScale, fontScale);
		titleFont.drawString(p0_apText, -p0_apTextBox.getCenter().x, -p0_apTextBox.getCenter().y);
		ofPopMatrix();

		// Draw P1 AP Box
		float p1_apCenterX = ofGetWidth() - staticUICardWidth - (30 * scale) + staticUICardWidth / 2;
		float p1_apCenterY = 40 * scale + staticUICardHeight + (40 * scale) + staticUICardHeight + 60 * scale;
		ofRectangle p1_apTextBox = titleFont.getStringBoundingBox(p1_apText, 0, 0);
		float p1_apRectWidth = (p1_apTextBox.width * fontScale) + (40 * scale);
		float p1_apRectHeight = (p1_apTextBox.height * fontScale) + (20 * scale);
		ofSetColor(0, 0, 0, 150);
		ofDrawRectRounded(p1_apCenterX - p1_apRectWidth / 2, p1_apCenterY - p1_apRectHeight / 2, p1_apRectWidth, p1_apRectHeight, 10 * scale);
		ofSetColor(ofColor::cyan);
		ofPushMatrix();
		ofTranslate(p1_apCenterX, p1_apCenterY);
		ofScale(fontScale, fontScale);
		titleFont.drawString(p1_apText, -p1_apTextBox.getCenter().x, -p1_apTextBox.getCenter().y);
		ofPopMatrix();
	}
	// End Turn Button
	float btnWidth_end = 250 * scale;
	float btnHeight_end = 60 * scale;
	endTurnButtonRect.set(endTurnButtonCurrentPos.x, endTurnButtonCurrentPos.y, btnWidth_end, btnHeight_end);

	// 1. Draw Button Background
	ofSetColor(isHoveringEndTurn ? ofColor::darkSlateGray : ofColor::slateGray);
	ofDrawRectRounded(endTurnButtonRect, 10 * scale);

	// 2. Draw Yellow Highlight (New Logic)
	// If no AP left AND player has already used their draw, suggest ending turn.
	if (currentAP <= 0 && hasDrawnCardsThisTurn) {
		ofPushStyle();
		ofNoFill();
		ofSetColor(ofColor::yellow);
		ofSetLineWidth(4 * scale);
		ofDrawRectRounded(endTurnButtonRect, 10 * scale);
		ofPopStyle();
	}

	// 3. Draw Button Text
	ofSetColor(ofColor::white);
	string endTurnButtonText = "End Turn";
	ofRectangle buttonTextBox = titleFont.getStringBoundingBox(endTurnButtonText, 0, 0);

	float etX = endTurnButtonRect.getCenter().x - (buttonTextBox.width * fontScale / 2);
	float etY = endTurnButtonRect.getCenter().y + (buttonTextBox.height * fontScale / 2);

	ofPushMatrix();
	ofTranslate(etX, etY);
	ofScale(fontScale, fontScale);
	titleFont.drawString(endTurnButtonText, 0, 0);
	ofPopMatrix();

	// --- OPTIMIZED HAND DRAWING (No Matrix Ops) ---
	if (!players.empty() && currentPlayerIndex >= 0) {
		Player & currentPlayer = players[currentPlayerIndex];
		size_t numCards = currentPlayer.hand.size();

		for (size_t i = 0; i < numCards; i++) {
			if (i == selectedCardIndex || i == hoveredCardIndex || i == draggedCardIndex) continue;

			Card & card = currentPlayer.hand[i];
			float w = handBaseCardWidth * card.currentScale;
			float h = baseCardHeight * card.currentScale;
			float x = card.currentPos.x - w / 2;
			float y = card.currentPos.y - h / 2;

			cardSpriteSheet.drawSubsection(x, y, w, h, card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);
		}

		if (selectedCardIndex != -1 && selectedCardIndex < numCards && selectedCardIndex != hoveredCardIndex && selectedCardIndex != draggedCardIndex) {
			Card & card = currentPlayer.hand[selectedCardIndex];
			float w = handBaseCardWidth * card.currentScale;
			float h = baseCardHeight * card.currentScale;
			float x = card.currentPos.x - w / 2;
			float y = card.currentPos.y - h / 2;

			cardSpriteSheet.drawSubsection(x, y, w, h, card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::yellow);
			ofSetLineWidth(4);
			ofDrawRectangle(x, y, w, h);
			ofPopStyle();
		}

		if (hoveredCardIndex != -1 && hoveredCardIndex < numCards && hoveredCardIndex != draggedCardIndex) {
			Card & card = currentPlayer.hand[hoveredCardIndex];
			float w = handBaseCardWidth * card.currentScale;
			float h = baseCardHeight * card.currentScale;
			float hoverYOffset = (currentPlayerIndex == 0) ? -120.0f : 120.0f;
			float drawY = (card.currentPos.y - h / 2) + hoverYOffset;
			float drawX = card.currentPos.x - w / 2;

			cardSpriteSheet.drawSubsection(drawX, drawY, w, h, card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);
			if (hoveredCardIndex == selectedCardIndex) {
				ofPushStyle();
				ofNoFill();
				ofSetColor(ofColor::yellow);
				ofSetLineWidth(4);
				ofDrawRectangle(drawX, drawY, w, h);
				ofPopStyle();
			}
		}

		if (draggedCardIndex != -1 && draggedCardIndex < numCards) {
			Card & card = currentPlayer.hand[draggedCardIndex];
			float w = handBaseCardWidth * card.currentScale;
			float h = baseCardHeight * card.currentScale;
			float dragYOffset = (currentPlayerIndex == 0) ? -120.0f : 120.0f;
			float drawX = card.currentPos.x - w / 2;
			float drawY = (card.currentPos.y - h / 2) + dragYOffset;

			cardSpriteSheet.drawSubsection(drawX, drawY, w, h, card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::yellow);
			ofSetLineWidth(4);
			ofDrawRectangle(drawX, drawY, w, h);
			ofPopStyle();
		}

		int otherPlayerIndex = (currentPlayerIndex + 1) % 2;
		Player & otherPlayer = players[otherPlayerIndex];
		if (!otherPlayer.hand.empty()) {
			float p_staticCardWidth = staticUICardWidth * 0.6f;
			float p_staticCardHeight = staticUICardHeight * 0.6f;
			float p_cardOverlap = p_staticCardWidth * 0.75f;
			float handY = (otherPlayerIndex == 0) ? ofGetHeight() - p_staticCardHeight - 20 * scale : 20 * scale;
			size_t otherNumCards = otherPlayer.hand.size();
			float totalHandWidth = p_staticCardWidth + (otherNumCards - 1) * (p_staticCardWidth - p_cardOverlap);
			float startX = (ofGetWidth() - totalHandWidth) / 2.0f;
			for (size_t i = 0; i < otherNumCards; ++i) {
				cardBackImage.draw(startX + i * (p_staticCardWidth - p_cardOverlap), handY, p_staticCardWidth, p_staticCardHeight);
			}
		}
	}

	// --- NEW: DRAW DECK/DISCARD HOVER VIEW ---
	if (isShowingPileView && currentPileViewPlayerIndex != -1) {
		Player & viewPlayer = players[currentPileViewPlayerIndex];
		string viewTitle;

		// 1. Get the correct cards and apply sorting rules
		if (currentPileView == VIEW_DECK) {
			viewTitle = "Deck";
			cardsToShowInView = viewPlayer.deck;

			// CHANGED: Sort by Cost (Low to High)
			std::sort(cardsToShowInView.begin(), cardsToShowInView.end(), [](const Card & a, const Card & b) {
				// Primary sort: Cost
				if (a.cost != b.cost) {
					return a.cost < b.cost;
				}
				// Secondary sort: Name (keeps it tidy if costs are equal)
				return a.name < b.name;
			});
		} else { // VIEW_DISCARD
			viewTitle = "Discard Pile";
			// The discard pile is already in chronological order, just copy it
			cardsToShowInView = viewPlayer.discardPile;
		}

		viewTitle = "Player " + ofToString(viewPlayer.playerID) + "'s " + viewTitle;

		if (!cardsToShowInView.empty()) {
			float panelPadding = 20.0f;
			float titleHeight = 40.0f;

			// 2. Dynamically calculate layout to fit cards on screen
			float viewCardScale = 1.6f;
			float availableHeight = ofGetHeight() - (2 * panelPadding) - titleHeight;
			float availableWidth = ofGetWidth() * 0.7f; // Use up to 70% of screen width

			// Iteratively scale down cards until they fit
			while (viewCardScale > 0.5f) {
				float cardW = handBaseCardWidth * viewCardScale;
				float cardH = baseCardHeight * viewCardScale;
				float padding = 15.0f * (viewCardScale / 1.6f);
				int cols = std::max(1, (int)floor((availableWidth - padding) / (cardW + padding)));
				int rows = ceil((float)cardsToShowInView.size() / cols);
				if (rows * (cardH + padding) - padding <= availableHeight) {
					break; // This scale and layout fits
				}
				viewCardScale -= 0.1f;
			}

			float viewCardWidth = handBaseCardWidth * viewCardScale;
			float viewCardHeight = baseCardHeight * viewCardScale;
			float padding = 15.0f * (viewCardScale / 1.6f);
			int gridWidthInCards = std::max(1, (int)floor((availableWidth - padding) / (viewCardWidth + padding)));
			int gridHeightInCards = ceil((float)cardsToShowInView.size() / gridWidthInCards);
			float totalContentWidth = (gridWidthInCards * viewCardWidth) + ((gridWidthInCards - 1) * padding);
			float totalContentHeight = (gridHeightInCards * viewCardHeight) + ((gridHeightInCards - 1) * padding);

			// 3. Intelligently position the panel
			float startX;
			// If viewing player 0's (left side) piles, show panel to the right
			if (currentPileViewPlayerIndex == 0) {
				startX = p0_deckRect.getRight() + 30.0f;
			}
			// If viewing player 1's (right side) piles, show panel to the left
			else {
				startX = p1_deckRect.getLeft() - totalContentWidth - 30.0f - (2 * panelPadding);
			}
			float startY = ofGetHeight() / 2.0f - totalContentHeight / 2.0f;

			// 4. Update the main pileViewRect for hit-testing in mouseMoved
			pileViewRect.set(startX, startY - titleHeight - panelPadding, totalContentWidth + 2 * panelPadding, totalContentHeight + titleHeight + 2 * panelPadding);

			// 5. Draw the panel and its contents
			ofSetColor(20, 20, 20, 220);
			ofDrawRectRounded(pileViewRect, 15);

			ofSetColor(ofColor::white);
			uiFont.drawString(viewTitle + " (" + ofToString(cardsToShowInView.size()) + " cards)", startX + panelPadding, startY - 15);

			for (size_t i = 0; i < cardsToShowInView.size(); ++i) {
				int row = i / gridWidthInCards;
				int col = i % gridWidthInCards;
				float drawX = startX + panelPadding + col * (viewCardWidth + padding);
				float drawY = startY + row * (viewCardHeight + padding);
				const Card & card = cardsToShowInView[i];
				cardSpriteSheet.drawSubsection(drawX, drawY, viewCardWidth, viewCardHeight, card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);
			}
		} else {
			// If the pile is empty, ensure the view is hidden
			isShowingPileView = false;
		}
	} else {
		// If view is not active, reset the rect to prevent accidental mouse interaction
		pileViewRect.set(0, 0, 0, 0);
	}

	// --- Draw Amnesia Selection UI ---
	if (isAmnesiaSelectionActive) {
		// This uses the same dynamic layout logic
		string title = "Choose " + ofToString(numCardsToRemove) + " card(s) to remove permanently.";

		float panelPadding = 20.0f;
		float titleHeight = 60.0f;
		float viewCardScale = 1.6f;
		float availableHeight = ofGetHeight() - (2 * panelPadding) - titleHeight;
		float availableWidth = ofGetWidth() * 0.8f;

		while (viewCardScale > 0.5f) {
			float cardW = handBaseCardWidth * viewCardScale;
			float cardH = baseCardHeight * viewCardScale;
			float padding = 15.0f * (viewCardScale / 1.6f);
			int cols = std::max(2, (int)floor((availableWidth - padding) / (cardW + padding)));
			int rows = ceil((float)amnesiaDeckCopy.size() / cols);
			if (rows * (cardH + padding) - padding <= availableHeight) {
				break;
			}
			viewCardScale -= 0.1f;
		}

		float viewCardWidth = handBaseCardWidth * viewCardScale;
		float viewCardHeight = baseCardHeight * viewCardScale;
		float padding = 15.0f * (viewCardScale / 1.6f);

		int gridWidthInCards = std::max(2, (int)floor((availableWidth - padding) / (viewCardWidth + padding)));

		// CRITICAL FIX: The next line was likely missing or commented out in your file, causing the error.
		int gridHeightInCards = ceil((float)amnesiaDeckCopy.size() / gridWidthInCards);

		float totalContentWidth = (gridWidthInCards * viewCardWidth) + ((gridWidthInCards - 1) * padding);
		float totalContentHeight = (gridHeightInCards * viewCardHeight) + ((gridHeightInCards - 1) * padding);

		float panelWidth = totalContentWidth + 2 * panelPadding;
		float panelHeight = totalContentHeight + titleHeight + 2 * panelPadding;
		float panelX = ofGetWidth() / 2.0f - panelWidth / 2.0f;
		float panelY = ofGetHeight() / 2.0f - panelHeight / 2.0f;

		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

		ofSetColor(20, 20, 20, 240);
		ofDrawRectRounded(panelX, panelY, panelWidth, panelHeight, 15);

		ofSetColor(ofColor::white);
		uiFont.drawString(title, panelX + panelPadding, panelY + panelPadding + uiFont.getLineHeight() * 0.8f);

		for (size_t i = 0; i < amnesiaDeckCopy.size(); ++i) {
			int row = i / gridWidthInCards;
			int col = i % gridWidthInCards;
			float drawX = panelX + panelPadding + col * (viewCardWidth + padding);
			float drawY = panelY + panelPadding + titleHeight + row * (viewCardHeight + padding);
			const Card & card = amnesiaDeckCopy[i];
			cardSpriteSheet.drawSubsection(drawX, drawY, viewCardWidth, viewCardHeight, card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);

			bool isSelected = false;
			for (int selectedIdx : amnesiaSelectedIndices) {
				if (selectedIdx == static_cast<int>(i)) { // Cast i to int for safe comparison
					isSelected = true;
					break;
				}
			}
			if (isSelected) {
				ofPushStyle();
				ofNoFill();
				ofSetColor(ofColor::yellow);
				ofSetLineWidth(5);
				ofDrawRectangle(drawX, drawY, viewCardWidth, viewCardHeight);
				ofPopStyle();
			}
		}
	}

	if (isDebugMode) {
		ofPushStyle();

		float panelWidth = 220;
		float panelX = ofGetWidth() - panelWidth - 20;
		float panelY = 40;

		if (players.size() >= 2 && currentPlayerIndex != -1) {
			panelY = p1_deckRect.getBottom() + 20;
		}

		float btnHeight = 45;
		float padding = 10;

		// --- STEP 1: Calculate Height First (Don't draw yet) ---
		// We calculate how tall the menu WILL be based on current state
		float calculatedHeight = padding + uiFont.getLineHeight() + padding; // Title space

		calculatedHeight += btnHeight + padding; // Draw Card
		calculatedHeight += btnHeight + padding; // Spawn Card
		calculatedHeight += btnHeight + padding; // Dropdown toggle

		if (isDebugDiceDropdownOpen) {
			calculatedHeight += (btnHeight + padding) * 5; // CHANGED from 3 to 5
		}

		calculatedHeight += btnHeight + padding; // Spawn Unit
		calculatedHeight += btnHeight + padding; // Unlimited AP
		calculatedHeight += btnHeight + padding; // Force End Turn

		// --- STEP 2: Draw Background ---
		debugPanel.set(panelX, panelY, panelWidth, calculatedHeight);
		ofSetColor(20, 20, 20, 255); // Fully opaque background (or 240 for slight transparency)
		ofDrawRectRounded(debugPanel, 10);

		// --- STEP 3: Draw Content on Top ---
		float currentY = panelY + padding;

		// Draw Title
		ofSetColor(ofColor::white);
		uiFont.drawString("Debug", panelX + padding, currentY + uiFont.getLineHeight() * 0.8f);
		currentY += uiFont.getLineHeight() + padding;

		// Lambda to draw buttons
		auto drawDebugButton = [&](const ofRectangle & rect, const string & label, bool isToggled = false, bool toggledState = false) {
			ofSetColor(isToggled ? (toggledState ? ofColor::green : ofColor::darkRed) : ofColor::slateGray);
			ofDrawRectRounded(rect, 5);
			ofSetColor(ofColor::white);
			ofRectangle textBox = uiFont.getStringBoundingBox(label, 0, 0);
			uiFont.drawString(label, rect.getCenter().x - textBox.width / 2, rect.getCenter().y + textBox.height / 2);
		};

		// Draw Buttons
		debugDrawCardButton.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
		drawDebugButton(debugDrawCardButton, "Draw Card");
		currentY += btnHeight + padding;

		debugSpawnCardButton.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
		drawDebugButton(debugSpawnCardButton, "Spawn Card...");
		currentY += btnHeight + padding;

		string diceLabel = isDebugDiceDropdownOpen ? "Roll Dice \xE2\x96\xB2" : "Roll Dice \xE2\x96\xBC";
		debugDiceDropdownButton.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
		drawDebugButton(debugDiceDropdownButton, diceLabel);
		currentY += btnHeight + padding;

		// Dice Dropdown Options
		if (isDebugDiceDropdownOpen) {

			debugFlipCoinButton.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
			drawDebugButton(debugFlipCoinButton, "Flip Coin");
			currentY += btnHeight + padding;

			debugRollD4Button.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
			drawDebugButton(debugRollD4Button, "Roll 1D4");
			currentY += btnHeight + padding;

			debugRollD6Button.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
			drawDebugButton(debugRollD6Button, "Roll 1D6");
			currentY += btnHeight + padding;

			debugRollD10Button.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
			drawDebugButton(debugRollD10Button, "Roll 1D10");
			currentY += btnHeight + padding;

			debugRollD20Button.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
			drawDebugButton(debugRollD20Button, "Roll 1D20");
			currentY += btnHeight + padding;
		}

		debugSpawnUnitButton.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
		drawDebugButton(debugSpawnUnitButton, "Spawn Player", true, isSpawningUnit);
		currentY += btnHeight + padding;

		debugUnlimitedAPButton.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
		drawDebugButton(debugUnlimitedAPButton, "Unlimited AP", true, hasUnlimitedAP);
		currentY += btnHeight + padding;

		debugForceEndTurnButton.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
		drawDebugButton(debugForceEndTurnButton, "Force End Turn");
		currentY += btnHeight + padding;

		ofPopStyle();
	}

	// --- Draw Stolen Card Animation (On top of most UI) ---
	for (const auto & anim : activeStolenCardAnimations) {
		ofSetColor(255, anim.currentAlpha);
		float w = handBaseCardWidth * anim.currentScale;
		float h = baseCardHeight * anim.currentScale;
		cardSpriteSheet.drawSubsection(anim.currentPos.x - w / 2, anim.currentPos.y - h / 2, w, h,
			anim.card.textureRect.x, anim.card.textureRect.y,
			anim.card.textureRect.width, anim.card.textureRect.height);
	}

	// --- Draw Amnesia Removal Animation ---
	for (const auto & anim : activeRemovedCardAnimations) {
		ofSetColor(255, anim.currentAlpha);
		float w = handBaseCardWidth * anim.currentScale;
		float h = baseCardHeight * anim.currentScale;
		cardSpriteSheet.drawSubsection(anim.startPos.x - w / 2, anim.startPos.y - h / 2, w, h,
			anim.card.textureRect.x, anim.card.textureRect.y,
			anim.card.textureRect.width, anim.card.textureRect.height);
	}

	// --- Draw Tooltip (drawn last to be on top of everything) ---
	if (isShowingTooltip) {
		ofRectangle textBox = uiFont.getStringBoundingBox(tooltipText, 0, 0);
		float textWidth = textBox.getWidth();
		float textHeight = textBox.getHeight();
		float padding = 8.0f;
		float tooltipX = tooltipPos.x + 20;
		float tooltipY = tooltipPos.y;
		if (tooltipX + textWidth + 2 * padding > ofGetWidth()) {
			tooltipX = tooltipPos.x - textWidth - 2 * padding - 20;
		}
		ofSetColor(10, 10, 10, 200);
		ofDrawRectRounded(tooltipX, tooltipY, textWidth + 2 * padding, textHeight + 2 * padding, 5);
		ofSetColor(ofColor::white);
		uiFont.drawString(tooltipText, tooltipX + padding, tooltipY + textHeight + padding / 2.0f);
	}
	// --- DRAW OVERLAY UIs ---
	if (isMagicBlastChoiceActive) {
		drawMagicBlastChoiceUI();
	}
	if (isDispelMenuOpen || isDispelTargeting || isDispelStatusSelectOpen) {
		drawDispelUI();
	}
	// ADD THIS:
	if (isWisdomBoonMenuOpen) {
		drawWisdomBoonUI();
	}
	ofDrawBitmapString("FPS: " + ofToString(ofGetFrameRate(), 2), 10, 20);
}
//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
	switch (currentState) {
	case STATE_GAMEPLAY: {
		bool isDiceSpinning = false;
		for (const auto & roll : activeDiceRolls) {
			if (!roll.isFinishedVisual) {
				isDiceSpinning = true;
				break;
			}
		}
		if (isDiceSpinning) {
			hoverPath.clear();
			return;
		}

		if (playerAction == PIECE_SELECTED) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gridX = floor(boardPos.x), gridY = floor(boardPos.y);
			if (gridX != lastHoverGridPos.x || gridY != lastHoverGridPos.y) {
				lastHoverGridPos = glm::vec2(gridX, gridY);
				if (gridX >= 0 && gridX < BOARD_WIDTH && gridY >= 0 && gridY < BOARD_HEIGHT) {
					if (board[gridX][gridY].isHighlighted) {
						hoverPath = findShortestPath({ (float)selectedPieceGridX, (float)selectedPieceGridY }, { (float)gridX, (float)gridY });
					} else {
						hoverPath.clear();
					}
				} else {
					hoverPath.clear();
				}
			}
		} else {
			lastHoverGridPos = glm::vec2(-1, -1);
		}

		if (players.empty() || currentPlayerIndex < 0) return;
		Player & currentPlayer = players[currentPlayerIndex];
		int foundHoverIndex = -1;
		float handBaseCardWidth = 120;
		float aspectRatio = 585.0f / 409.0f;
		float baseCardHeight = handBaseCardWidth * aspectRatio;

		if (draggedCardIndex == -1) {
			// FIX: Loop backwards safely from size()-1
			for (int i = static_cast<int>(currentPlayer.hand.size()) - 1; i >= 0; i--) {
				Card & card = currentPlayer.hand[i];
				ofRectangle cardRect(card.currentPos.x - handBaseCardWidth / 2, card.currentPos.y - baseCardHeight / 2, handBaseCardWidth, baseCardHeight);
				if (cardRect.inside(x, y)) {
					foundHoverIndex = i;
					break;
				}
			}
		}
		hoveredCardIndex = foundHoverIndex;
		for (size_t i = 0; i < currentPlayer.hand.size(); i++) {
			currentPlayer.hand[i].targetScale = (static_cast<int>(i) == hoveredCardIndex) ? 2.5f : 1.5f;
		}

		int activeCardForHighlight = (selectedCardIndex != -1) ? selectedCardIndex : hoveredCardIndex;
		if (currentPlayerIndex == 0) {
			calculateTargetHighlights(activeCardForHighlight);
		}

		isHoveringEndTurn = endTurnButtonRect.inside(x, y);

		isShowingTooltip = false;

		// --- REVISED: PILE HOVERING LOGIC (with Delay) ---
		PileViewMode newHoveredPileType = VIEW_NONE;
		int newHoveredPilePlayer = -1;

		if (p0_deckRect.inside(x, y)) {
			newHoveredPileType = VIEW_DECK;
			newHoveredPilePlayer = 0;
		} else if (p0_discardRect.inside(x, y)) {
			newHoveredPileType = VIEW_DISCARD;
			newHoveredPilePlayer = 0;
		} else if (p1_deckRect.inside(x, y)) {
			newHoveredPileType = VIEW_DECK;
			newHoveredPilePlayer = 1;
		} else if (p1_discardRect.inside(x, y)) {
			newHoveredPileType = VIEW_DISCARD;
			newHoveredPilePlayer = 1;
		}

		if (newHoveredPilePlayer != -1) {
			if (!isHoveringPile || newHoveredPileType != hoveredPileType || newHoveredPilePlayer != hoveredPilePlayerIndex) {
				isHoveringPile = true;
				isShowingPileView = false; // Reset view flag on new hover
				hoveredPileType = newHoveredPileType;
				hoveredPilePlayerIndex = newHoveredPilePlayer;
				pileHoverStartTime = ofGetElapsedTimef();
			}
		} else {
			isHoveringPile = false;
			// If mouse is not over a pile AND not over the view panel, hide the view
			if (isShowingPileView && !pileViewRect.inside(x, y)) {
				isShowingPileView = false;
				currentPileViewPlayerIndex = -1;
				currentPileView = VIEW_NONE;
			}
		}

		// Tooltip logic
		isShowingTooltip = false;
		if (!isShowingPileView && !isHoveringPile && players.size() >= 2) {
			if (p0_deckRect.inside(x, y)) {
				isShowingTooltip = true;
				// FIX 2: Use direct assignment for glm::vec2 instead of .set()
				tooltipPos = glm::vec2(x, y);
				tooltipText = ofToString(players[0].deck.size()) + " cards";
			} else if (p0_discardRect.inside(x, y)) {
				isShowingTooltip = true;
				// FIX 2: Use direct assignment
				tooltipPos = glm::vec2(x, y);
				tooltipText = ofToString(players[0].discardPile.size()) + " cards";
			} else if (p1_deckRect.inside(x, y)) {
				isShowingTooltip = true;
				// FIX 2: Use direct assignment
				tooltipPos = glm::vec2(x, y);
				tooltipText = ofToString(players[1].deck.size()) + " cards";
			} else if (p1_discardRect.inside(x, y)) {
				isShowingTooltip = true;
				// FIX 2: Use direct assignment
				tooltipPos = glm::vec2(x, y);
				tooltipText = ofToString(players[1].discardPile.size()) + " cards";
			}
		}
		break; // End of case STATE_GAMEPLAY
	}
	case STATE_MAIN_MENU: {
		mainMenuHoveredIndex = -1;
		if (mainMenuPlayAIButton.inside(x, y)) mainMenuHoveredIndex = 0;
		if (mainMenuMultiplayerButton.inside(x, y)) mainMenuHoveredIndex = 1;
		if (mainMenuSettingsButton.inside(x, y)) mainMenuHoveredIndex = 2;
		if (mainMenuQuitButton.inside(x, y)) mainMenuHoveredIndex = 3;
		break;
	}

	case STATE_SETTINGS: {
		settingsHoveredIndex = -1;
		if (settingsBackButton.inside(x, y)) settingsHoveredIndex = 0;
		break;
	}
	case STATE_PAUSED: {
		pauseMenuHoveredIndex = -1;
		if (pauseMenuResumeButton.inside(x, y)) pauseMenuHoveredIndex = 0;
		if (pauseMenuSettingsButton.inside(x, y)) pauseMenuHoveredIndex = 1;
		if (pauseMenuQuitButton.inside(x, y)) pauseMenuHoveredIndex = 2;
		break;
	}
	}
}
// ----------------- FULL mousePressed FUNCTION -----------------
void ofApp::mousePressed(int x, int y, int button) {

	// ==============================================================================
	// PHASE 1: MODAL UI INTERRUPTS
	// ==============================================================================

	// --- 1a. Magic Blast Choice Menu ---
	if (isMagicBlastChoiceActive && button == OF_MOUSE_BUTTON_LEFT) {
		bool choiceMade = false;
		Player * targetPlayer = getPlayer(magicBlastTargetPlayerIndex);

		if (!targetPlayer) {
			// Logic to try and find the next splash target if the current one became invalid
			if (!magicBlastSplashTargetIndices.empty()) {
				magicBlastTargetPlayerIndex = magicBlastSplashTargetIndices.front();
				magicBlastSplashTargetIndices.erase(magicBlastSplashTargetIndices.begin());
				magicBlastChoicesRemaining = 1;
				return;
			}
			isMagicBlastChoiceActive = false;
			return;
		}

		if (magicBlastDamageButton.inside(x, y)) {
			// --- FIX: Correctly apply magic damage, including Barrier check ---
			int damage = 5;

			// 1. Barrier absorbs non-physical damage
			int barrierDamage = std::min(targetPlayer->barrier, damage);
			targetPlayer->barrier -= barrierDamage;
			damage -= barrierDamage;

			// 2. Ward absorbs any remaining damage
			if (damage > 0) {
				int wardDamage = std::min(targetPlayer->ward, damage);
				targetPlayer->ward -= wardDamage;
				damage -= wardDamage;
			}

			// 3. Health takes the final damage
			if (damage > 0) {
				targetPlayer->health -= damage;
			}

			ofLogNotice("MagicBlast") << "Player " << targetPlayer->playerID << " chose Damage.";
			choiceMade = true;
		} else if (magicBlastDiscardButton.inside(x, y)) {
			if (!targetPlayer->deck.empty()) {
				targetPlayer->deck.pop_back();
				ofLogNotice("MagicBlast") << "Player " << targetPlayer->playerID << " chose Discard.";
			} else {
				ofLogNotice("MagicBlast") << "Player " << targetPlayer->playerID << " has no cards to discard!";
			}
			choiceMade = true;
		}

		if (choiceMade) {
			magicBlastChoicesRemaining--;
			if (magicBlastChoicesRemaining <= 0) {
				if (!magicBlastSplashTargetIndices.empty()) {
					magicBlastTargetPlayerIndex = magicBlastSplashTargetIndices.front();
					magicBlastSplashTargetIndices.erase(magicBlastSplashTargetIndices.begin());
					magicBlastChoicesRemaining = 1;
					ofLogNotice("MagicBlast") << "Moving to splash target...";
				} else {
					isMagicBlastChoiceActive = false;
					magicBlastTargetPlayerIndex = -1;
					ofLogNotice("MagicBlast") << "Sequence Complete.";
				}
			}
		}
		return;
	}

	// --- 1b. Dispel Menu ---
	if (isDispelMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {
		if (dispelBtnBarrier.inside(x, y)) {
			isWaitingForBarrierDice = true;
			pendingDispelRollResult = startDiceRoll(1, 20, PURPOSE_BARRIER_GAIN);
			Player & p = players[currentPlayerIndex];
			currentAP -= p.hand[pendingDispelCardIndex].cost;
			p.discardPile.push_back(p.hand[pendingDispelCardIndex]);
			p.hand.erase(p.hand.begin() + pendingDispelCardIndex);
			ofLogNotice("Dispel") << "Rolling for Non-Physical Barrier...";
			cancelDispel();
		} else if (dispelBtnPurge.inside(x, y)) {
			isDispelMenuOpen = false;
			isDispelTargeting = true;
		} else if (!dispelMenuRect.inside(x, y)) {
			cancelDispel();
		}
		return;
	}

	// --- 1c. Dispel Status Selection ---
	if (isDispelStatusSelectOpen && button == OF_MOUSE_BUTTON_LEFT) {
		bool clickedOption = false;
		for (size_t i = 0; i < statusSelectButtons.size(); i++) {
			if (statusSelectButtons[i].inside(x, y)) {
				applyDispelEffect((int)i);
				clickedOption = true;
				break;
			}
		}
		if (!clickedOption && !statusSelectMenuRect.inside(x, y)) {
			cancelDispel();
		}
		return;
	}

	// --- 1d. Dispel Targeting ---
	if (isDispelTargeting && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x);
		int gy = floor(boardPos.y);
		bool foundTarget = false;
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].x == gx && players[i].y == gy) {
					Player & curr = players[currentPlayerIndex];
					if (abs(curr.x - gx) + abs(curr.y - gy) <= 1) {
						pendingDispelTargetIndex = (int)i;
						determineStatusOptions(&players[i]);
						foundTarget = true;
					}
					break;
				}
			}
		}
		if (!foundTarget) cancelDispel();
		return;
	}

	// --- 1e. Wisdom Boon Menu ---
	if (isWisdomBoonMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {
		Player * target = getPlayer(pendingWisdomBoonTargetIndex);
		Player & caster = players[currentPlayerIndex];
		if (!target) {
			cancelWisdomBoon();
			return;
		}

		bool choiceMade = false;
		int effectValue = (int)caster.deck.size();
		bool isSelfTarget = (pendingWisdomBoonTargetIndex == currentPlayerIndex);

		if (wisdomBtnDamage.inside(x, y)) {
			if (isSelfTarget) {
				ofLogNotice("Wisdom Boon") << "Granting " << effectValue << " Block to Self.";
				target->block += effectValue;
			} else {
				ofLogNotice("Wisdom Boon") << "Dealing " << effectValue << " Magic Damage to Enemy.";
				int dmg = effectValue;
				int barrierDmg = std::min(target->barrier, dmg);
				target->barrier -= barrierDmg;
				dmg -= barrierDmg;
				if (dmg > 0) {
					int wardDmg = std::min(target->ward, dmg);
					target->ward -= wardDmg;
					dmg -= wardDmg;
				}
				if (dmg > 0) target->health -= dmg;
			}
			choiceMade = true;
		} else if (!wisdomMenuRect.inside(x, y)) {
			cancelWisdomBoon();
			return;
		}

		if (choiceMade) {
			currentAP -= caster.hand[pendingWisdomBoonCardIndex].cost;
			Card playedCard = caster.hand[pendingWisdomBoonCardIndex];
			caster.playedCardsPile.push_back(playedCard);

			if (caster.isReplicatePending) {
				Card dup = playedCard;
				caster.playedCardsPile.push_back(dup);
				ofLogNotice("Replicate") << "Wisdom Boon Replicated.";
				caster.isReplicatePending = false;
			}
			caster.hand.erase(caster.hand.begin() + pendingWisdomBoonCardIndex);
			cancelWisdomBoon();
			calculateTargetHighlights();
		}
		return;
	}

	// --- 1f. Amnesia Selection UI ---
	if (isAmnesiaSelectionActive && button == OF_MOUSE_BUTTON_LEFT) {
		float amnesiaCardWidth = 120;
		float amnesiaCardHeight = amnesiaCardWidth * (585.0f / 409.0f);
		float panelPadding = 20.0f;
		float titleHeight = 60.0f;
		float viewCardScale = 1.6f;
		float availableHeight = ofGetHeight() - (2 * panelPadding) - titleHeight;
		float availableWidth = ofGetWidth() * 0.8f;

		while (viewCardScale > 0.5f) {
			float cardW = amnesiaCardWidth * viewCardScale;
			float cardH = amnesiaCardHeight * viewCardScale;
			float padding = 15.0f * (viewCardScale / 1.6f);
			int cols = std::max(2, (int)floor((availableWidth - padding) / (cardW + padding)));
			int rows = ceil((float)amnesiaDeckCopy.size() / cols);
			if (rows * (cardH + padding) - padding <= availableHeight) break;
			viewCardScale -= 0.1f;
		}

		float viewCardWidth = amnesiaCardWidth * viewCardScale;
		float viewCardHeight = amnesiaCardHeight * viewCardScale;
		float padding = 15.0f * (viewCardScale / 1.6f);
		int gridWidthInCards = std::max(2, (int)floor((availableWidth - padding) / (viewCardWidth + padding)));
		int gridHeightInCards = ceil((float)amnesiaDeckCopy.size() / gridWidthInCards);
		float totalContentWidth = (gridWidthInCards * viewCardWidth) + ((gridWidthInCards - 1) * padding);
		float panelWidth = totalContentWidth + 2 * panelPadding;
		float panelHeight = (gridHeightInCards * (viewCardHeight + padding)) + titleHeight + 2 * panelPadding;
		float panelX = ofGetWidth() / 2.0f - panelWidth / 2.0f;
		float panelY = ofGetHeight() / 2.0f - panelHeight / 2.0f;

		for (size_t i = 0; i < amnesiaDeckCopy.size(); ++i) {
			int row = (int)i / gridWidthInCards;
			int col = (int)i % gridWidthInCards;
			ofRectangle cardRect(
				panelX + panelPadding + col * (viewCardWidth + padding),
				panelY + panelPadding + titleHeight + row * (viewCardHeight + padding),
				viewCardWidth, viewCardHeight);

			if (cardRect.inside(x, y)) {
				int cardIndex = static_cast<int>(i);
				auto it = std::find(amnesiaSelectedIndices.begin(), amnesiaSelectedIndices.end(), cardIndex);
				if (it != amnesiaSelectedIndices.end()) {
					amnesiaSelectedIndices.erase(it);
				} else {
					if (amnesiaSelectedIndices.size() < static_cast<size_t>(numCardsToRemove)) {
						amnesiaSelectedIndices.push_back(cardIndex);
					}
				}

				if (amnesiaSelectedIndices.size() == static_cast<size_t>(numCardsToRemove)) {
					Player * targetPlayer = getPlayer(amnesiaTargetPlayerIndex);
					if (targetPlayer) {
						std::sort(amnesiaSelectedIndices.rbegin(), amnesiaSelectedIndices.rend());
						for (int selectedIdx : amnesiaSelectedIndices) {
							if (static_cast<size_t>(selectedIdx) < targetPlayer->deck.size()) {
								Card selectedCard = targetPlayer->deck[selectedIdx];
								RemovedCardAnimation anim;
								anim.card = selectedCard;
								anim.startPos = {
									panelX + panelPadding + (selectedIdx % gridWidthInCards) * (viewCardWidth + padding) + viewCardWidth / 2,
									panelY + panelPadding + titleHeight + (selectedIdx / gridWidthInCards) * (viewCardHeight + padding) + viewCardHeight / 2
								};
								anim.startTime = ofGetElapsedTimef();
								activeRemovedCardAnimations.push_back(anim);
								targetPlayer->deck.erase(targetPlayer->deck.begin() + selectedIdx);
							}
						}
					}
					isAmnesiaSelectionActive = false;
					amnesiaSelectedIndices.clear();
					amnesiaDeckCopy.clear();
					amnesiaTargetPlayerIndex = -1;
				}
				return;
			}
		}
		if (ofRectangle(panelX, panelY, panelWidth, panelHeight).inside(x, y)) return;
	}

	// ==============================================================================
	// PHASE 2: GLOBAL MOUSE TRACKING
	// ==============================================================================
	if (button != OF_MOUSE_BUTTON_LEFT && button != OF_MOUSE_BUTTON_RIGHT) return;
	mouseDownPos.set(x, y);

	// ==============================================================================
	// PHASE 3: STATE-DEPENDENT LOGIC
	// ==============================================================================
	switch (currentState) {

	case STATE_GAMEPLAY: {

		// 3a. Debug Spawn Logic
		if (isSpawningUnit && button == OF_MOUSE_BUTTON_LEFT) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gx = floor(boardPos.x), gy = floor(boardPos.y);
			if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
				if (!board[gx][gy].hasWall && !board[gx][gy].hasPlayer) {
					Player newPlayer;
					newPlayer.x = gx;
					newPlayer.y = gy;
					newPlayer.playerID = (int)players.size();
					newPlayer.deck = allCards;
					std::shuffle(newPlayer.deck.begin(), newPlayer.deck.end(), rng);
					players.push_back(newPlayer);
					board[gx][gy].hasPlayer = true;
					ofLogNotice("Debug") << "Spawned new player.";
				}
			}
			isSpawningUnit = false;
			return;
		}

		// 3b. Debug UI Interactions (Buttons)
		if (isDebugMode && button == OF_MOUSE_BUTTON_LEFT) {
			if (debugPanel.inside(x, y)) {
				// Handle specific debug buttons
				if (debugDrawCardButton.inside(x, y)) {
					drawCard();
					return;
				}

				if (debugSpawnCardButton.inside(x, y)) {
					string cardName = ofSystemTextBoxDialog("Enter Card Name", "");
					if (!cardName.empty()) {
						bool foundCard = false;
						for (const auto & card : allCards) {
							if (ofToLower(card.name) == ofToLower(cardName)) {
								players[currentPlayerIndex].hand.push_back(card);
								players[currentPlayerIndex].hand.back().currentPos = ofVec2f(ofGetWidth() / 2, 0);
								foundCard = true;
								break;
							}
						}
						if (!foundCard) ofSystemAlertDialog("Card '" + cardName + "' not found!");
					}
					return;
				}

				if (debugDiceDropdownButton.inside(x, y)) {
					isDebugDiceDropdownOpen = !isDebugDiceDropdownOpen;
					return;
				}

				// Only check dropdown buttons if open
				if (isDebugDiceDropdownOpen) {
					if (debugFlipCoinButton.inside(x, y)) {
						startDiceRoll(1, 2, PURPOSE_DEBUG);
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD4Button.inside(x, y)) {
						startDiceRoll(1, 4, PURPOSE_DEBUG);
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD6Button.inside(x, y)) {
						startDiceRoll(1, 6, PURPOSE_DEBUG);
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD10Button.inside(x, y)) {
						startDiceRoll(1, 10, PURPOSE_DEBUG);
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD20Button.inside(x, y)) {
						startDiceRoll(1, 20, PURPOSE_DEBUG);
						isDebugDiceDropdownOpen = false;
						return;
					}
				}

				if (debugSpawnUnitButton.inside(x, y)) {
					isSpawningUnit = !isSpawningUnit;
					return;
				}
				if (debugUnlimitedAPButton.inside(x, y)) {
					hasUnlimitedAP = !hasUnlimitedAP;
					return;
				}
				if (debugForceEndTurnButton.inside(x, y)) {
					startNewTurn();
					return;
				}

				return; // Clicked panel background
			}
		}

		// 3c. Safety Checks (Input Lock)
		if (players.empty() || currentPlayerIndex < 0) return;
		Player & currentPlayer = players[currentPlayerIndex];

		bool isDiceSpinning = false;
		for (const auto & roll : activeDiceRolls) {
			if (!roll.isFinishedVisual) {
				isDiceSpinning = true;
				break;
			}
		}
		if (isPlayerAnimating || isDiceSpinning) return;

		// 3d. Deck Clicking (Drawing Cards) - INCLUDES HASTEN LOGIC
		if (button == OF_MOUSE_BUTTON_LEFT) {
			if (players.empty() || currentPlayerIndex < 0) return; // Safety check

			Player & activePlayer = players[currentPlayerIndex];

			// --- FIX: Find player pointers locally and check affiliation ---
			Player * player0 = nullptr;
			Player * player1 = nullptr;
			for (auto & p : players) {
				if (p.playerID == 0) player0 = &p;
				if (p.playerID == 1) player1 = &p;
			}

			// If we can't find the main players for some reason, exit
			if (!player0 || !player1) return;

			// Player 0's Deck
			bool isPlayer0sTurn = (activePlayer.playerID == 0 || activePlayer.ownerID == 0);
			if (p0_deckRect.inside(x, y) && isPlayer0sTurn && !hasDrawnCardsThisTurn) {
				int cardsToDraw = player0->nextTurnExtraDraw ? 3 : 2;
				if (player0->nextTurnExtraDraw) ofLogNotice("Game") << "Hasten Effect: Drawing 3 cards!";
				for (int i = 0; i < cardsToDraw; i++)
					drawCard();
				player0->nextTurnExtraDraw = false;
				hasDrawnCardsThisTurn = true;
				return;
			}

			// Player 1's Deck
			bool isPlayer1sTurn = (activePlayer.playerID == 1 || activePlayer.ownerID == 1);
			if (p1_deckRect.inside(x, y) && isPlayer1sTurn && !hasDrawnCardsThisTurn) {
				int cardsToDraw = player1->nextTurnExtraDraw ? 3 : 2;
				if (player1->nextTurnExtraDraw) ofLogNotice("Game") << "Hasten Effect: Drawing 3 cards!";
				for (int i = 0; i < cardsToDraw; i++)
					drawCard();
				player1->nextTurnExtraDraw = false;
				hasDrawnCardsThisTurn = true;
				return;
			}
		}

		// 3e. Card Selection
		if (button == OF_MOUSE_BUTTON_LEFT) {
			int foundClickIndex = hoveredCardIndex;
			if (foundClickIndex != -1) {
				playerAction = NONE;
				clearHighlights();
				selectedCardIndex = (selectedCardIndex == foundClickIndex) ? -1 : foundClickIndex;
				calculateTargetHighlights(selectedCardIndex);
				return;
			}
		}

		// 3f. Casting Spells (Card Selected + Click Board)
		if (selectedCardIndex != -1 && button == OF_MOUSE_BUTTON_LEFT) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gridX = floor(boardPos.x), gridY = floor(boardPos.y);
			if (gridX >= 0 && gridX < BOARD_WIDTH && gridY >= 0 && gridY < BOARD_HEIGHT && board[gridX][gridY].isTargetable) {
				playCard(selectedCardIndex, gridX, gridY);
				selectedCardIndex = -1;
				calculateTargetHighlights();
				return;
			}
		}

		// 3g. End Turn Button
		if (endTurnButtonRect.inside(x, y) && button == OF_MOUSE_BUTTON_LEFT) {
			startNewTurn();
			return;
		}

		// 3h. Unit Movement Selection
		if (button == OF_MOUSE_BUTTON_LEFT) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gridX = floor(boardPos.x), gridY = floor(boardPos.y);

			// Clicked Outside? Deselect everything.
			if (gridX < 0 || gridX >= BOARD_WIDTH || gridY < 0 || gridY >= BOARD_HEIGHT) {
				selectedCardIndex = -1;
				playerAction = NONE;
				clearHighlights();
				calculateTargetHighlights();
				return;
			}

			// Clicked Self? Select for Movement.
			if (board[gridX][gridY].hasPlayer && gridX == currentPlayer.x && gridY == currentPlayer.y) {
				if (playerAction == PIECE_SELECTED) {
					playerAction = NONE;
					clearHighlights();
				} else {
					selectedPieceGridX = currentPlayer.x;
					selectedPieceGridY = currentPlayer.y;
					playerAction = PIECE_SELECTED;
					selectedCardIndex = -1;
					calculateTargetHighlights();
					calculateHighlights();
				}
				return;
			}

			// Clicked Move Destination?
			if (playerAction == PIECE_SELECTED) {
				if (board[gridX][gridY].isHighlighted && !hoverPath.empty()) {
					int moveAPCost = static_cast<int>(hoverPath.size()) - 1;
					if (currentAP >= moveAPCost) {
						currentAP -= moveAPCost;
						board[currentPlayer.x][currentPlayer.y].hasPlayer = false;
						board[gridX][gridY].hasPlayer = true;
						currentPlayer.x = gridX;
						currentPlayer.y = gridY;

						// Start Animation
						animationPath.clear();
						currentPathIndex = 0;
						animationPath.push_back(playerVisualPos);
						for (size_t p = 1; p < hoverPath.size(); ++p) {
							animationPath.push_back(gridToWorld((int)hoverPath[p].x, (int)hoverPath[p].y));
						}
						if (!animationPath.empty()) isPlayerAnimating = true;
						invalidateTargetCache();
					}
				}
				playerAction = NONE;
				clearHighlights();
				return;
			}

			// Deselect if clicking random empty tile
			selectedCardIndex = -1;
			calculateTargetHighlights();
		}
		break;
	}

	case STATE_MAIN_MENU: {
		if (mainMenuPlayAIButton.inside(x, y)) {
			isLoadingGame = true;
		} else if (mainMenuSettingsButton.inside(x, y)) {
			stateBeforeSettings = STATE_MAIN_MENU;
			currentState = STATE_SETTINGS;
		} else if (mainMenuQuitButton.inside(x, y)) {
			ofExit();
		}
		break;
	}

	case STATE_SETTINGS: {
		if (settingsBackButton.inside(x, y)) {
			currentState = stateBeforeSettings;
		}
		if (settingsResLeftButton.inside(x, y)) {
			currentResolutionIndex = std::max(0, currentResolutionIndex - 1);
			applySettings();
		}
		if (settingsResRightButton.inside(x, y)) {
			currentResolutionIndex = std::min(static_cast<int>(availableResolutions.size()) - 1, currentResolutionIndex + 1);
			applySettings();
		}
		if (settingsFrameLeftButton.inside(x, y)) {
			currentFramerateIndex = std::max(0, currentFramerateIndex - 1);
			applySettings();
		}
		if (settingsFrameRightButton.inside(x, y)) {
			currentFramerateIndex = std::min(static_cast<int>(availableFramerates.size()) - 1, currentFramerateIndex + 1);
			applySettings();
		}
		if (settingsFullscreenButton.inside(x, y)) {
			isFullscreen = !isFullscreen;
			applySettings();
		}
		break;
	}

	case STATE_PAUSED: {
		if (pauseMenuResumeButton.inside(x, y)) {
			currentState = STATE_GAMEPLAY;
		}
		if (pauseMenuSettingsButton.inside(x, y)) {
			stateBeforeSettings = STATE_PAUSED;
			currentState = STATE_SETTINGS;
		}
		if (pauseMenuQuitButton.inside(x, y)) {
			cleanupGame();
			currentState = STATE_MAIN_MENU;
		}
		break;
	}
	}
}
//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
	if (currentState != STATE_GAMEPLAY) return;

	if (button == OF_MOUSE_BUTTON_RIGHT) {
		float dx = ofGetPreviousMouseX() - x, dy = ofGetPreviousMouseY() - y;
		cameraTargetPan.x += dx * 0.05f * (TILE_SIZE / 4.0f);
		cameraTargetPan.z += dy * 0.05f * (TILE_SIZE / 4.0f);
		return;
	}

	bool isDiceSpinning = false;
	for (const auto & roll : activeDiceRolls) {
		if (!roll.isFinishedVisual) {
			isDiceSpinning = true;
			break;
		}
	}
	if (isPlayerAnimating || isDiceSpinning) return;

	const float dragThreshold = 5.0f;
	if (mouseDownPos.distance(ofVec2f(x, y)) > dragThreshold) {

		if (draggedCardIndex == -1 && selectedCardIndex != -1) {
			if (players.empty() || currentPlayerIndex < 0) return;
			Player & currentPlayer = players[currentPlayerIndex];
			int numCards = static_cast<int>(currentPlayer.hand.size());
			float handBaseCardWidth = 120;

			float handCardAspectRatio = 585.0f / 409.0f;
			float baseCardHeight = handBaseCardWidth * handCardAspectRatio;

			float handAreaWidth = ofGetWidth() * 0.4f;
			float totalCardWidths = numCards * handBaseCardWidth;
			float padding = (numCards > 1) ? (handAreaWidth - totalCardWidths) / (numCards - 1) : 0;
			padding = std::min(padding, 20.0f);
			float totalHandWidth = (numCards * handBaseCardWidth) + ((numCards - 1) * padding);
			float startX = (ofGetWidth() - totalHandWidth) / 2.0f;

			Card & card = currentPlayer.hand[selectedCardIndex];
			float detectionWidth = handBaseCardWidth;
			float detectionHeight = baseCardHeight * 1.6f;
			float detectionX = startX + selectedCardIndex * (handBaseCardWidth + padding);
			float detectionY = card.targetPos.y - detectionHeight / 2;

			if (ofRectangle(detectionX, detectionY, detectionWidth, detectionHeight).inside(ofGetPreviousMouseX(), ofGetPreviousMouseY())) {
				draggedCardIndex = selectedCardIndex;
				dragOffset = ofVec2f(x, y) - card.currentPos;
			}
		}

		if (draggedCardIndex != -1) {
			if (selectedCardIndex != -1) {
				selectedCardIndex = -1;
				calculateTargetHighlights();
			}
			players[currentPlayerIndex].hand[draggedCardIndex].currentPos = ofVec2f(x, y) - dragOffset;
		} else if (playerAction == PIECE_SELECTED) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gridX = floor(boardPos.x);
			int gridY = floor(boardPos.y);
			if (gridX >= 0 && gridX < BOARD_WIDTH && gridY >= 0 && gridY < BOARD_HEIGHT) {
				if (board[gridX][gridY].isHighlighted) {
					hoverPath = findShortestPath({ (float)selectedPieceGridX, (float)selectedPieceGridY }, { (float)gridX, (float)gridY });
				} else {
					hoverPath.clear();
				}
			} else {
				hoverPath.clear();
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
	if (currentState != STATE_GAMEPLAY) return;

	bool isDiceSpinning = false;
	for (const auto & roll : activeDiceRolls) {
		if (!roll.isFinishedVisual) {
			isDiceSpinning = true;
			break;
		}
	}
	if (isDiceSpinning) return;

	if (button == OF_MOUSE_BUTTON_RIGHT) {
		if (mouseDownPos.distance(ofVec2f(x, y)) < 5.0f) {
			selectedCardIndex = -1;
			draggedCardIndex = -1;
			playerAction = NONE;
			clearHighlights();
			calculateTargetHighlights();
		}
	}

	if (button == OF_MOUSE_BUTTON_LEFT) {
		if (players.empty() || currentPlayerIndex < 0) return;
		const float dragThreshold = 5.0f;
		float dist = mouseDownPos.distance(ofVec2f(x, y));
		Player & currentPlayer = players[currentPlayerIndex];

		if (draggedCardIndex != -1) {
			if (dist > dragThreshold) {
				Card & playedCard = currentPlayer.hand[draggedCardIndex];
				float playZoneY = ofGetHeight() * 0.7f;
				if (y < playZoneY) {
					if (currentAP >= playedCard.cost) {
						if (playedCard.targeting == TARGET_SELF) {
							playCard(draggedCardIndex, -1, -1);
						} else {
							std::vector<glm::vec2> validTargets;
							for (int tx = 0; tx < BOARD_WIDTH; tx++) {
								for (int ty = 0; ty < BOARD_HEIGHT; ty++) {
									if (board[tx][ty].isTargetable) {
										validTargets.push_back({ tx, ty });
									}
								}
							}
							if (validTargets.size() == 1) {
								playCard(draggedCardIndex, validTargets[0].x, validTargets[0].y);
							}
						}
					} else {
						ofLogNotice("Game") << "Not enough AP!";
					}
				}
			}
			selectedCardIndex = -1;
			calculateTargetHighlights();
			draggedCardIndex = -1;
		}
	}
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
	if (currentState != STATE_GAMEPLAY) return;

	cameraTargetZoom -= scrollY * 4.0f;
	cameraTargetZoom = ofClamp(cameraTargetZoom, 20.0f, 150.0f);
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key) { }
//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
	// 1. Debug Toggle
	if (key == '`') {
		isDebugMode = !isDebugMode;
	}

	// 2. Top-Down Camera Toggle
	if (key == 't' || key == 'T') {
		if (currentState == STATE_GAMEPLAY) {
			isTopDownView = !isTopDownView;
			if (isTopDownView) {
				ofLogNotice("Camera") << "Toggled top-down view ON";
				last3DZoom = cameraTargetZoom;
				cameraTargetZoom = 60.0f;
			} else {
				ofLogNotice("Camera") << "Toggled top-down view OFF";
				cameraTargetZoom = last3DZoom;
			}
		}
	}

	// 2b. Post-processing toggles (debug)
	if (key == 'p' || key == 'P') {
		enableWorldPostProcess = !enableWorldPostProcess;
		ofLogNotice("Post") << "enableWorldPostProcess=" << (enableWorldPostProcess ? "true" : "false");
	}
	if (key == 'o' || key == 'O') {
		showWorldFboPreview = !showWorldFboPreview;
		ofLogNotice("Post") << "showWorldFboPreview=" << (showWorldFboPreview ? "true" : "false");
	}

	// 3. Escape Key Logic (Merged back inside the function)
	if (key == OF_KEY_ESC) {
		switch (currentState) {
		case STATE_GAMEPLAY:
			// First, check if any UI panel is active and close it.
			if (isShowingPileView) {
				isShowingPileView = false;
				currentPileViewPlayerIndex = -1;
				currentPileView = VIEW_NONE;
			} else if (isAmnesiaSelectionActive) {
				isAmnesiaSelectionActive = false; // Allow cancelling Amnesia
			}
			// If no UI was open, then pause the game.
			else {
				currentState = STATE_PAUSED;
			}
			break;
		case STATE_PAUSED:
			currentState = STATE_GAMEPLAY;
			break;
		case STATE_SETTINGS:
			currentState = stateBeforeSettings;
			break;
		default:
			break;
		}
	}
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) { }

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) { }

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
	recalculateUI(w, h);
	allocateWorldFbo(w, h);
}
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) { }

//--------------------------------------------------------------
void ofApp::startNewTurn() {
	if (players.empty()) return;

	// --- 1. Handle the ENDING player's state ---
	if (currentPlayerIndex != -1) {
		Player & endingPlayer = players[currentPlayerIndex];

		// A. Move Hand to Discard
		endingPlayer.discardPile.insert(endingPlayer.discardPile.end(), endingPlayer.hand.begin(), endingPlayer.hand.end());
		endingPlayer.hand.clear();

		// B. Move "Grey Area/Played" cards to Discard (Cleanup Step)
		endingPlayer.discardPile.insert(endingPlayer.discardPile.end(), endingPlayer.playedCardsPile.begin(), endingPlayer.playedCardsPile.end());
		endingPlayer.playedCardsPile.clear();

		// Reset turn-specific counters
		endingPlayer.shocksPlayedThisTurn = 0;

		// NOTE: We do NOT reset isReplicatePending here.
		// If they played Replicate but nothing else, it waits for the next card play.
	}

	// --- 2. Advance to the NEXT player ---
	// Cycle index
	currentPlayerIndex = (currentPlayerIndex + 1) % players.size();

	// Increment Global Turn Counter if we wrapped back to start (or P0)
	if (currentPlayerIndex == 0) globalTurnCounter++;

	Player & startingPlayer = players[currentPlayerIndex];
	ofLogNotice("Game") << "--- START TURN: " << (startingPlayer.isMinion ? "Minion " : "Player ") << startingPlayer.playerID;

	// --- REGENERATION ---
	if (startingPlayer.hasRegeneration) {
		if (startingPlayer.health < startingPlayer.maxHealth) {
			startingPlayer.health++;
			ofLogNotice("Regen") << "Regenerated 1 HP.";
		}
	}

	// --- 3. Expiry Checks (Reset Armor) ---
	startingPlayer.block = 0;
	startingPlayer.ward = 0;
	startingPlayer.barrier = 0;

	// --- 4. NEW: PARALYSIS CHECK ---
	if (startingPlayer.isParalyzed) {
		ofLogNotice("Status") << "Player is Paralyzed! Flipping coin...";
		startDiceRoll(1, 2, PURPOSE_COIN_FLIP);
		isWaitingForParalysisCoin = true;
		return;
	}

	// --- 5. Fire Check ---
	if (startingPlayer.onFire) {
		ofLogNotice("On Fire") << "Player is burning! Rolling 1D6 for damage...";
		isWaitingForOnFireDice = true;
		pendingOnFireRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE);
		return;
	}

	continueNewTurn();
}
//--------------------------------------------------------------
void ofApp::continueNewTurn() {
	Player & startingPlayer = players[currentPlayerIndex];

	ofLogNotice("Game") << "Player " << startingPlayer.playerID << "'s turn begins.";
	hasDrawnCardsThisTurn = false;
	selectedCardIndex = -1;
	draggedCardIndex = -1;
	playerAction = NONE;
	clearHighlights();
	calculateTargetHighlights();

	// FIX: Snap visual position instantly to the new unit so it doesn't "fly" across the board
	playerVisualPos = gridToWorld(startingPlayer.x, startingPlayer.y);
	animationPath.clear();
	isPlayerAnimating = false;

	activeDiceRolls.clear();
	currentAP = 0;

	// --- AP ROLL LOGIC ---
	// Minions roll 1d6. Players roll 1d6 (or 1d10 if Hastened).
	if (startingPlayer.isMinion) {
		startDiceRoll(1, 6, PURPOSE_AP);
	} else {
		int apDiceSides = 6;
		if (startingPlayer.nextTurnD10AP) {
			apDiceSides = 10;
			startingPlayer.nextTurnD10AP = false;
		}
		startDiceRoll(1, apDiceSides, PURPOSE_AP);
	}
}
//--------------------------------------------------------------
void ofApp::drawCard() {
	if (players.empty() || currentPlayerIndex < 0) return;
	Player & currentPlayer = players[currentPlayerIndex];

	// --- PHASE 1: CHECK IF DECK NEEDS RESHUFFLE ---
	if (currentPlayer.deck.empty()) {
		if (currentPlayer.discardPile.empty()) {
			ofLogNotice("Game") << "Cannot draw. Both Deck and Discard are empty.";
			return;
		}

		ofLogNotice("Game") << "Deck is empty. Reshuffling Discard Pile into Deck...";

		// Move Discard -> Deck
		currentPlayer.deck = currentPlayer.discardPile;

		// Clear Discard (This causes the discard pile visual to disappear, which is correct)
		currentPlayer.discardPile.clear();

		// Shuffle the new Deck
		std::shuffle(currentPlayer.deck.begin(), currentPlayer.deck.end(), rng);
	}

	// --- PHASE 2: DRAW THE CARD ---
	// We check empty() again because we might have just refilled it in Phase 1.
	if (!currentPlayer.deck.empty()) {
		Card newCard = currentPlayer.deck.back();
		currentPlayer.deck.pop_back();

		// --- Animation Setup ---
		newCard.currentScale = 0.1f;
		newCard.targetScale = 1.5f;

		// Calculate Spawn Position (from Deck UI)
		float scale = ofGetHeight() / 1080.0f;
		float staticUICardWidth = (120 * 1.3f) * scale;
		float staticUICardHeight = ((120 * (585.0f / 409.0f)) * 1.3f) * scale;

		if (currentPlayerIndex == 0) {
			float deckX = 30 * scale;
			float deckY = ofGetHeight() - staticUICardHeight - (40 * scale) - staticUICardHeight - (40 * scale);
			newCard.currentPos.set(deckX + staticUICardWidth / 2, deckY + staticUICardHeight / 2);
		} else {
			float discardX = ofGetWidth() - staticUICardWidth - (30 * scale);
			float discardY = 40 * scale;
			float deckX = discardX;
			float deckY = discardY + staticUICardHeight + (40 * scale);
			newCard.currentPos.set(deckX + staticUICardWidth / 2, deckY + staticUICardHeight / 2);
		}

		// Add to Hand
		currentPlayer.hand.push_back(newCard);
		ofLogNotice("Game") << "Drew card for Player " << currentPlayer.playerID << ": " << newCard.name;
	}
}
//--------------------------------------------------------------
void ofApp::playCard(int cardIndex, int targetX, int targetY) {
	Player & currentPlayer = players[currentPlayerIndex];
	if (cardIndex < 0 || cardIndex >= static_cast<int>(currentPlayer.hand.size())) return;

	Card playedCard = currentPlayer.hand[cardIndex];
	if (currentAP < playedCard.cost) return;

	// --- 1. DEFINE DAMAGE HELPER LAMBDA ---
	auto applyDamage = [&](Player & target, int damage, DamageType type) -> bool {
		string typeStr = "Physical";
		if (type == DAMAGE_PIERCING) typeStr = "Piercing";
		if (type == DAMAGE_MAGIC) typeStr = "Magic";
		if (type == DAMAGE_ELECTRIC) typeStr = "Electric";
		if (type == DAMAGE_FIRE) typeStr = "Fire";

		ofLogNotice("Game") << "Dealing " << damage << " damage (" << typeStr << ") to Player " << target.playerID;

		int initialHealth = target.health;
		int remainingDmg = damage;

		// --- LAYER 1: SPECIFIC MITIGATION ---
		// Block stops Physical. Barrier stops Non-Physical (Magic/Fire/Electric).

		if (type == DAMAGE_PHYSICAL) {
			// Physical hits BLOCK
			int absorb = std::min(target.block, remainingDmg);
			target.block -= absorb;
			remainingDmg -= absorb;
			if (absorb > 0) ofLogNotice("Game") << "Block absorbed " << absorb;
		} else if (type != DAMAGE_PIERCING) {
			// Non-Physical (and not Piercing) hits BARRIER
			int absorb = std::min(target.barrier, remainingDmg);
			target.barrier -= absorb;
			remainingDmg -= absorb;
			if (absorb > 0) ofLogNotice("Game") << "Non-Phys Barrier absorbed " << absorb;
		}
		// Note: DAMAGE_PIERCING bypasses Layer 1 entirely.

		// --- LAYER 2: GENERIC MITIGATION (FIXED) ---
		// Ward stops EVERYTHING (Least specific, always last shield)
		if (remainingDmg > 0) {
			int wardAbsorb = std::min(target.ward, remainingDmg);
			target.ward -= wardAbsorb;
			remainingDmg -= wardAbsorb;
			if (wardAbsorb > 0) ofLogNotice("Game") << "Ward absorbed " << wardAbsorb;
		}

		// --- FINAL HEALTH DAMAGE ---
		if (remainingDmg > 0) {
			target.health -= remainingDmg;
			ofLogNotice("Game") << remainingDmg << " damage taken to Health! (HP: " << target.health << ")";
		}

		// --- REPLACE THE OLD "if (target.health <= 0)" WITH THIS ---
		if (target.health <= 0) {
			ofLogNotice("Game") << "Player " << target.playerID << " defeated!";

			// Graveyard Logic
			DeathMarker death;
			death.x = target.x;
			death.y = target.y;
			death.turnDied = globalTurnCounter;
			death.deck = target.deck; // Save their deck for looting
			graveyard.push_back(death);

			// Note: We don't remove them from the 'players' vector immediately
			// because that shifts indices and breaks the loop currently running.
			// Usually, you mark them as dead and clean up at the start of the next turn,
			// or move them to a "dead" state (x = -100).
			board[target.x][target.y].hasPlayer = false; // Free up the tile immediately
			target.x = -1000; // Move off screen
		}
		// -----------------------------------------------------------

		return target.health < initialHealth;
	};

	bool playedSuccessfully = false;

	// --- 2. CARD LOGIC SWITCH ---
	switch (playedCard.type) {

	// --- CASE: MIND THEFT ---
	case CARD_MIND_THEFT: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		Player * targetPlayer = getPlayer(targetIndex);

		if (targetPlayer && !targetPlayer->deck.empty()) {
			Card stolenCard = targetPlayer->deck.back();
			ofLogNotice("Game") << "Mind Theft! Revealed '" << stolenCard.name << "'";
			targetPlayer->deck.pop_back();
			currentPlayer.deck.push_back(stolenCard);
			std::shuffle(currentPlayer.deck.begin(), currentPlayer.deck.end(), rng);

			// Animation
			StolenCardAnimation newAnim;
			newAnim.card = stolenCard;
			newAnim.startTime = ofGetElapsedTimef();
			newAnim.startPos = gridToWorld(targetPlayer->x, targetPlayer->y);
			newAnim.targetPos = { ofGetWidth() / 2.0f, ofGetHeight() / 2.0f };
			newAnim.currentPos = cam.worldToScreen(newAnim.startPos);
			activeStolenCardAnimations.push_back(newAnim);

			playedSuccessfully = true;
		}
		break;
	}

	// --- CASE: AMNESIA ---
	case CARD_AMNESIA: {
		// 1. Find the target unit
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		// 2. Validate Target Presence
		if (targetIndex == -1) {
			ofLogNotice("Amnesia") << "Cast failed! No unit at target.";
			break;
		}

		// 3. Validate Range (Self or Adjacent)
		Player * targetP = getPlayer(targetIndex);
		int dist = abs(targetP->x - currentPlayer.x) + abs(targetP->y - currentPlayer.y);

		if (dist > 1) {
			ofLogNotice("Amnesia") << "Cast failed! Target must be Self or Adjacent.";
			break;
		}

		// 4. Validate Deck Size
		if (targetP->deck.empty()) {
			ofLogNotice("Amnesia") << "Cast failed! Target has no cards in deck.";
			break;
		}

		// 5. Execute
		ofLogNotice("Amnesia") << "Targeting Player " << targetP->playerID << ". Rolling to see how many cards to remove...";

		// Store the target index so we know whose deck to modify later
		amnesiaTargetPlayerIndex = targetIndex;

		// Start Dice Roll for CARD COUNT REMOVAL (Not healing)
		pendingAmnesiaRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DAMAGE);
		isWaitingForAmnesiaDice = true;

		playedSuccessfully = true;
		break;
	}

		// --- CASE: MAGIC BLAST ---
	case CARD_MAGIC_BLAST: {
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };

		// FIX: Added the 4th argument (playedCard.type) to the function call
		// Also updated range to be based on the card's dice (e.g., 2d10 = 20ft max)
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, playedCard.numDice * 10.0f, playedCard.type);

		if (validationResult.reason != VALID) {
			ofLogNotice("Magic Blast") << "Cast failed! LOS Blocked or invalid target.";
			break;
		}

		// Re-validate that there is a unit to hit (directly or via splash)
		bool hasValidUnit = false;
		if (board[targetX][targetY].hasPlayer) {
			hasValidUnit = true;
		}
		if (!hasValidUnit) {
			glm::vec2 neighbors[4] = { { targetX + 1, targetY }, { targetX - 1, targetY }, { targetX, targetY + 1 }, { targetX, targetY - 1 } };
			for (const auto & n : neighbors) {
				if (n.x >= 0 && n.x < BOARD_WIDTH && n.y >= 0 && n.y < BOARD_HEIGHT) {
					if (board[(int)n.x][(int)n.y].hasPlayer) {
						hasValidUnit = true;
						break;
					}
				}
			}
		}

		if (!hasValidUnit) {
			ofLogNotice("Magic Blast") << "Cast failed! No valid unit targets at that location.";
			break;
		}

		ofLogNotice("Magic Blast") << "Casting! Rolling for range...";
		pendingMagicBlastRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE);
		isWaitingForMagicBlastDice = true;
		pendingMagicBlastTargetTile = targetTile;

		playedSuccessfully = true;
		break;
	}

	// --- CASE: FIREBALL ---
	case CARD_FIREBALL: {
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };

		// FIX: Added the 4th argument (playedCard.type) to the function call
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, playedCard.numDice * 6.0f, playedCard.type);

		if (validationResult.reason != VALID) {
			ofLogNotice("Fireball") << "Cast failed! Target invalid.";
			break;
		}

		if (!board[targetX][targetY].hasPlayer) {
			ofLogNotice("Fireball") << "Cast failed! Must target a unit.";
			break;
		}

		ofLogNotice("Fireball") << "Casting! Rolling for range...";
		pendingFireballRangeResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE);
		isWaitingForFireballRangeDice = true;
		pendingFireballTargetTile = targetTile;

		playedSuccessfully = true;
		break;
	}

	// --- CASE: ROCK CRUSH ---
	case CARD_ROCK_CRUSH: {
		if (board[targetX][targetY].hasWall) {
			ofLogNotice("Rock Crush") << "SMASH! Wall destroyed.";
			board[targetX][targetY].hasWall = false;
			buildLevelMesh(); // Rebuild visuals
			playedSuccessfully = true;
		} else {
			// Check for Unit Hit
			int targetIndex = -1;
			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].x == targetX && players[i].y == targetY) {
					targetIndex = (int)i;
					break;
				}
			}
			if (targetIndex != -1) {
				ofLogNotice("Rock Crush") << "Crushing Player " << players[targetIndex].playerID;
				pendingAttackRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DAMAGE);
				isWaitingForAttackDice = true;
				pendingAttackDamageType = playedCard.damageType;
				pendingAttackTargetIndices.clear();
				pendingAttackTargetIndices.push_back(targetIndex);
				playedSuccessfully = true;
			}
		}
		break;
	}

		// --- CASE: DISPEL ---
	case CARD_DISPEL: {
		// Don't execute yet. Just open the menu.
		pendingDispelCardIndex = cardIndex;
		isDispelMenuOpen = true;

		// Calculate menu geometry once
		float w = 600, h = 300;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		dispelMenuRect.set(x, y, w, h);
		dispelBtnBarrier.set(x + 40, y + 100, 250, 100);
		dispelBtnPurge.set(x + w - 290, y + 100, 250, 100);

		// Do NOT set playedSuccessfully = true yet.
		// We wait for user input.
		break;
	}

	// --- CASE: TELEPORT ---
	case CARD_TELEPORT: {
		// Double check target is empty
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) {
			ofLogNotice("Teleport") << "Invalid destination.";
			break;
		}

		ofLogNotice("Teleport") << "Initiating Teleport... Rolling 3d6 for range.";

		// Setup State
		pendingTeleportTarget = glm::vec2(targetX, targetY);
		pendingTeleportRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE);
		isWaitingForTeleportDice = true;

		playedSuccessfully = true;
		break;
	}

	// --- CASE: HASTEN ---
	case CARD_HASTEN: {
		// Apply buffs for the NEXT turn
		currentPlayer.nextTurnD10AP = true;
		currentPlayer.nextTurnExtraDraw = true;

		ofLogNotice("Hasten") << "Player " << currentPlayer.playerID << " feels faster! Next turn: D10 AP & Draw 3.";
		playedSuccessfully = true;
		break;
	}

	// --- CASE: REPLICATE ---
	case CARD_REPLICATE: {
		// Just set the flag. The cleanup step handles the card movement.
		currentPlayer.isReplicatePending = true;
		ofLogNotice("Replicate") << "Replicate Active! Next spell will be doubled.";
		playedSuccessfully = true;
		break;
	}

	// --- CASE: WISDOM BOON ---
	case CARD_WISDOM_BOON: {
		// 1. Find the target unit
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		// 2. Validate Target
		if (targetIndex == -1) {
			ofLogNotice("Wisdom Boon") << "Failed: No target selected.";
			break;
		}

		Player * t = getPlayer(targetIndex);
		bool isSelf = (targetIndex == currentPlayerIndex);
		bool isAdjacent = (abs(t->x - currentPlayer.x) + abs(t->y - currentPlayer.y) == 1);

		// Enforce Rules: Must be Self OR Adjacent
		if (!isSelf && !isAdjacent) {
			ofLogNotice("Wisdom Boon") << "Failed: Target must be Self or Adjacent.";
			break;
		}

		// 3. Setup Menu State
		pendingWisdomBoonCardIndex = cardIndex;
		pendingWisdomBoonTargetIndex = targetIndex;
		isWisdomBoonMenuOpen = true;

		// 4. Calculate Layout
		float w = 600, h = 300;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		wisdomMenuRect.set(x, y, w, h);

		// Single big button for the context-sensitive action
		float btnW = 300, btnH = 80;
		// We'll reuse 'wisdomBtnDamage' as the main action button
		wisdomBtnDamage.set(x + (w - btnW) / 2, y + 150, btnW, btnH);
		// Hide the other button
		wisdomBtnBlock.set(0, 0, 0, 0);

		break; // <--- WISDOM BOON ENDS HERE
	}

	// --- CASE: HEAL ---
	case CARD_HEAL: {
		// 1. Validate LoS again (using the 1000.0f range for unlimited)
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, 1000.0f, playedCard.type);

		if (validationResult.reason != VALID) {
			ofLogNotice("Heal") << "Cast failed! No Line of Sight.";
			break;
		}

		// 2. Find Target Unit
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		if (targetIndex == -1) {
			ofLogNotice("Heal") << "Cast failed! No unit at target.";
			break;
		}

		// --- NEW FRIENDLY CHECK (VALIDATION) ---
		Player * target = getPlayer(targetIndex);
		int casterOwner = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		int targetOwner = target->isMinion ? target->ownerID : target->playerID;

		if (casterOwner != targetOwner) {
			ofLogNotice("Heal") << "Cast failed! Cannot heal an enemy unit.";
			break; // Exit the case
		}
		// --- END NEW FRIENDLY CHECK ---

		// 3. Start Dice Roll (The rest of the function is correct)
		ofLogNotice("Heal") << "Casting Heal on Player " << players[targetIndex].playerID << "... Rolling 2d6.";

		pendingHealTargetIndex = targetIndex;
		// We reuse PURPOSE_DAMAGE just so the number pops up visually
		pendingHealRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DAMAGE);
		isWaitingForHealDice = true;

		playedSuccessfully = true;
		break; // <--- HEAL ENDS HERE
	}

		// --- CASE: ETHEREAL JOLT ---
	case CARD_ETHEREAL_JOLT: {
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };

		// Validate (using the special ignore-walls logic inside isLosTargetValid)
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, 20.0f, playedCard.type);

		if (validationResult.reason != VALID) {
			ofLogNotice("Jolt") << "Cast failed! Target invalid.";
			break;
		}

		if (!board[targetX][targetY].hasPlayer) {
			ofLogNotice("Jolt") << "Cast failed! Must target a unit.";
			break;
		}

		ofLogNotice("Jolt") << "Casting Ethereal Jolt! Rolling 1d20 for range...";
		pendingJoltRangeResult = startDiceRoll(1, 20, PURPOSE_RANGE);
		isWaitingForJoltRangeDice = true;
		pendingJoltTargetTile = targetTile;

		playedSuccessfully = true;
		break;
	}

	// --- CASE: FLAME HIT ---
	case CARD_FLAME_HIT: {
		// 1. Find Target Unit
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		// 2. Validate
		if (targetIndex == -1) {
			ofLogNotice("Flame Hit") << "Cast failed! No unit at target.";
			break;
		}

		// 3. Execute Damage
		Player * target = getPlayer(targetIndex);

		// applyDamage returns TRUE if Health was reduced (bypassed shields)
		bool healthHit = applyDamage(*target, playedCard.value, DAMAGE_FIRE);

		if (healthHit) {
			target->onFire = true;
			ofLogNotice("Flame Hit") << "Target took health damage and caught Fire!";
		} else {
			ofLogNotice("Flame Hit") << "Damage absorbed by armor. Fire status NOT applied.";
		}

		playedSuccessfully = true;
		break;
	}

		// --- CASE: RAISE DEAD ---
	case CARD_RAISE_DEAD: {
		// Double check target
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) {
			ofLogNotice("Raise Dead") << "Invalid target.";
			break;
		}

		ofLogNotice("Raise Dead") << "Raising Skeleton! Rolling 1d6 for HP...";

		pendingSummonTile = glm::vec2(targetX, targetY);
		pendingSummonRollResult = startDiceRoll(1, 6, PURPOSE_HP); // Ensure this is NOT PURPOSE_COIN_FLIP or PURPOSE_AP
		isWaitingForSummonHealth = true;

		playedSuccessfully = true;
		break;
	}

	// --- CASE: STANDARD ATTACK (Stab, Cleave, Pierce) ---
	case CARD_ATTACK_SINGLE_TILE: {
		int px = players[currentPlayerIndex].x;
		int py = players[currentPlayerIndex].y;
		pendingAttackTargetIndices.clear();

		// 1. CLEAVE LOGIC (Arc)
		if (playedCard.targeting == TARGET_CLEAVE_ADJACENT) {
			glm::vec2 dir = { (float)(targetX - px), (float)(targetY - py) };
			std::vector<Player *> targetsToHit = findCleaveTargets(dir);
			for (auto * targetPlayer : targetsToHit) {
				for (size_t i = 0; i < players.size(); i++) {
					if (&players[i] == targetPlayer) pendingAttackTargetIndices.push_back((int)i);
				}
			}
		}
		// 2. PIERCE LOGIC (Line - e.g. Stab)
		else if (playedCard.targeting == TARGET_LINEAR_PIERCE) {
			// Calculate Direction based on where the player clicked relative to self
			glm::vec2 dir = { (float)(targetX - px), (float)(targetY - py) };

			// Normalize to get strict cardinal direction (1 tile step)
			if (std::abs(dir.x) > std::abs(dir.y)) {
				dir.x = (dir.x > 0) ? 1.0f : -1.0f;
				dir.y = 0.0f;
			} else {
				dir.x = 0.0f;
				dir.y = (dir.y > 0) ? 1.0f : -1.0f;
			}

			// Define the two tiles in the straight line
			glm::vec2 pos1 = { px + dir.x, py + dir.y }; // Adjacent
			glm::vec2 pos2 = { px + dir.x * 2, py + dir.y * 2 }; // Behind

			// --- ADD TARGETS IN ORDER ---
			// The updateGame logic uses the index to determine damage.
			// Index 0 = Full Damage. Index 1+ = Half Damage (if type is PIERCING).

			// A. Check Position 1 (Closest)
			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].x == (int)pos1.x && players[i].y == (int)pos1.y) {
					pendingAttackTargetIndices.push_back((int)i);
					ofLogNotice("Attack") << "Hit front target: Player " << players[i].playerID;
					break;
				}
			}

			// B. Check Position 2 (Behind)
			// Only check if Position 1 is NOT a wall (walls block the stab)
			if (!isTileWall((int)pos1.x, (int)pos1.y)) {
				for (size_t i = 0; i < players.size(); i++) {
					if (players[i].x == (int)pos2.x && players[i].y == (int)pos2.y) {
						pendingAttackTargetIndices.push_back((int)i);
						ofLogNotice("Attack") << "Hit back target: Player " << players[i].playerID;
						break;
					}
				}
			}
		}
		// 3. STANDARD SINGLE TARGET
		else {
			// Use the standard LoS check to ensure valid reach (e.g. 5ft range)
			glm::vec2 casterTile = { (float)px, (float)py };
			glm::vec2 targetTile = { (float)targetX, (float)targetY };

			// Use range 5.0f (2 squares) for standard melee, or whatever the card says
			TargetInfo info = isLosTargetValid(casterTile, targetTile, 5.0f, playedCard.type);

			if (info.reason == VALID && info.isTargetable) {
				for (size_t i = 0; i < players.size(); i++) {
					if (players[i].x == targetX && players[i].y == targetY) {
						pendingAttackTargetIndices.push_back((int)i);
						break;
					}
				}
			} else {
				ofLogNotice("Attack") << "Invalid Target (Blocked or Out of Range)";
			}
		}

		if (pendingAttackTargetIndices.empty()) {
			ofLogNotice("Attack") << "No valid targets hit. Action Cancelled.";
			break; // Don't spend AP/Card if we hit nothing
		}

		// --- EXECUTE DAMAGE ---
		// If the card has dice, we roll for damage.
		if (playedCard.numDice > 0 && playedCard.diceSides > 0) {
			pendingAttackRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DAMAGE);
			isWaitingForAttackDice = true;
			pendingAttackDamageType = playedCard.damageType;
		} else {
			// If flat damage (no dice), apply immediately
			int damage = playedCard.value;
			for (size_t i = 0; i < pendingAttackTargetIndices.size(); i++) {
				int pIndex = pendingAttackTargetIndices[i];
				Player * target = getPlayer(pIndex);
				if (target) {
					// Handle Piercing Halving (First target full, others half)
					int finalDamage = damage;
					if (playedCard.damageType == DAMAGE_PIERCING && i > 0) {
						finalDamage /= 2;
					}
					applyDamage(*target, finalDamage, playedCard.damageType);
				}
			}
		}

		playedSuccessfully = true;
		break;
	}

	// --- CASE: SHOCK ---
	case CARD_SHOCK: {
		Player * target = nullptr;
		for (auto & p : players) {
			if (p.x == targetX && p.y == targetY) {
				target = &p;
				break;
			}
		}

		if (target) {
			applyDamage(*target, playedCard.value, DAMAGE_ELECTRIC);
			// Combo Check: If played 2nd time this turn -> Paralyze
			if (currentPlayer.shocksPlayedThisTurn > 0 && !target->isParalyzed) {
				ofLogNotice("Shock") << "Combo Triggered! Player " << target->playerID << " is now Paralyzed.";
				target->isParalyzed = true;
				target->paralysisHeadsCount = 0; // Reset the counter for their turn
			}
		}

		currentPlayer.nextTurnAPBonus += 2;
		currentPlayer.shocksPlayedThisTurn++;
		playedSuccessfully = true;
		break;
	}

	case CARD_GAIN_AP:
		currentAP += playedCard.value;
		playedSuccessfully = true;
		break;

	case CARD_GAIN_BLOCK:
		players[currentPlayerIndex].block += playedCard.value;
		playedSuccessfully = true;
		break;

	case CARD_GAIN_WARD:
		players[currentPlayerIndex].ward += playedCard.value;
		playedSuccessfully = true;
		break;

	default:
		break;
	}

	// --- 3. COMMON CLEANUP ---
	if (playedSuccessfully) {
		// Pay Cost
		currentAP -= playedCard.cost;

		// 1. Move played card to "Grey Area" (playedCardsPile)
		// It will stay here until end of turn.
		currentPlayer.playedCardsPile.push_back(playedCard);

		// 2. Handle REPLICATE Effect
		// If Replicate is pending (and we aren't just casting Replicate itself)
		if (currentPlayer.isReplicatePending && playedCard.type != CARD_REPLICATE) {

			// Create a duplicate
			Card duplicateCard = playedCard;

			// The duplicate ALSO goes into the Grey Area
			currentPlayer.playedCardsPile.push_back(duplicateCard);

			ofLogNotice("Replicate") << "Replicated " << playedCard.name << ". Copy added to played pile.";

			// Consume the effect
			currentPlayer.isReplicatePending = false;
		}

		// 3. Remove from Hand
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);

		// 4. Visuals & Cache
		activeCardDisplays.push_back({ playedCard, ofGetElapsedTimef() });
		invalidateTargetCache();
	}
}
//--------------------------------------------------------------
ofVec2f ofApp::mouseToBoard(int x, int y) {
	// No special hacks needed here anymore since the camera's state is always correct.
	glm::vec3 planePoint(0, 0, 0);
	glm::vec3 planeNormal(0, 1, 0);
	glm::vec3 rayOrigin = cam.screenToWorld(glm::vec3(x, y, 0));
	glm::vec3 rayDirection = cam.screenToWorld(glm::vec3(x, y, 1)) - rayOrigin;
	float distance;
	bool intersects = glm::intersectRayPlane(rayOrigin, rayDirection, planePoint, planeNormal, distance);
	if (intersects) {
		glm::vec3 intersectionPoint = rayOrigin + rayDirection * distance;
		float gridX = (intersectionPoint.x / TILE_SIZE) + (BOARD_WIDTH / 2.0f);
		float gridY = (intersectionPoint.z / TILE_SIZE) + (BOARD_HEIGHT / 2.0f);
		return ofVec2f(gridX, gridY);
	}
	return ofVec2f(-1, -1);
}

//--------------------------------------------------------------
void ofApp::calculateTargetHighlights(int cardToCalculate) {
	// --- NEW CACHING LOGIC ---
	// If nothing has changed that would affect targets, don't recalculate.
	if (players.empty() || currentPlayerIndex < 0) return;
	Player & currentPlayer = players[currentPlayerIndex];
	if (currentPlayer.x == lastCachedPlayerX && currentPlayer.y == lastCachedPlayerY && cardToCalculate == lastCachedCardIndex) {
		return; // The cache is still valid, do nothing.
	}

	// If we're here, something changed. Update the cache state.
	lastCachedPlayerX = currentPlayer.x;
	lastCachedPlayerY = currentPlayer.y;
	lastCachedCardIndex = cardToCalculate;
	// --- END CACHING LOGIC ---

	// Clear old highlights
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			board[x][y].isTargetable = false;
			targetCache[x][y] = TargetInfo();
		}
	}

	if (cardToCalculate < 0 || cardToCalculate >= static_cast<int>(currentPlayer.hand.size())) return;

	Card & card = currentPlayer.hand[cardToCalculate];
	int px = currentPlayer.x;
	int py = currentPlayer.y;

	// --- 1. SPECIAL LOS LOGIC (Magic Blast / Fireball / Jolt / Heal) ---
	// Added CARD_HEAL to this condition
	if (card.type == CARD_MAGIC_BLAST || card.type == CARD_FIREBALL || card.type == CARD_ETHEREAL_JOLT || card.type == CARD_HEAL) {

		float maxRange = 0.0f;
		if (card.type == CARD_MAGIC_BLAST)
			maxRange = 20.0f;
		else if (card.type == CARD_FIREBALL)
			maxRange = 12.0f;
		else if (card.type == CARD_ETHEREAL_JOLT)
			maxRange = 20.0f;
		else if (card.type == CARD_HEAL) {
			// Heal is unlimited range, but ONLY for self or friendly units.
			glm::vec2 casterTile = { (float)px, (float)py };
			for (int x = 0; x < BOARD_WIDTH; x++) {
				for (int y = 0; y < BOARD_HEIGHT; y++) {
					// Find the unit on this tile, if any
					Player * target = nullptr;
					for (auto & p : players) {
						if (p.x == x && p.y == y) {
							target = &p;
							break;
						}
					}

					if (target) {
						// --- NEW FRIENDLY CHECK ---
						// Determine owner of caster and target
						int casterOwner = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
						int targetOwner = target->isMinion ? target->ownerID : target->playerID;

						if (casterOwner == targetOwner) {
							// It's a friendly target, now check Line of Sight
							TargetInfo validationResult = isLosTargetValid(casterTile, { (float)x, (float)y }, 1000.0f, card.type);
							if (validationResult.reason == VALID) {
								board[x][y].isTargetable = true;
							}
						}
					}
				}
			}
			return; // Exit the function early as we handled this card type completely
		}

		glm::vec2 casterTile = { (float)px, (float)py };

		for (int x = 0; x < BOARD_WIDTH; x++) {
			for (int y = 0; y < BOARD_HEIGHT; y++) {
				// Determine if target is valid based on LoS and Range
				targetCache[x][y] = isLosTargetValid(casterTile, { (float)x, (float)y }, maxRange, card.type);

				if (targetCache[x][y].reason == VALID) {
					board[x][y].isTargetable = targetCache[x][y].isTargetable;
				} else {
					board[x][y].isTargetable = false;
				}
			}
		}
	}
	// --- 2. DIRECTIONAL LOGIC (Cleave / Pierce) ---
	else if (card.targeting == TARGET_CLEAVE_ADJACENT) {
		glm::vec2 directions[4] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };
		for (const auto & dir : directions) {
			std::vector<Player *> targets = findCleaveTargets(dir);
			if (!targets.empty()) {
				glm::vec2 centerTile = { (float)px + dir.x, (float)py + dir.y };
				if (centerTile.x >= 0 && centerTile.x < BOARD_WIDTH && centerTile.y >= 0 && centerTile.y < BOARD_HEIGHT) {
					board[(int)centerTile.x][(int)centerTile.y].isTargetable = true;
				}
			}
		}
	} else if (card.targeting == TARGET_LINEAR_PIERCE) {
		// 4 Cardinal Directions
		glm::vec2 directions[4] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };

		for (const auto & dir : directions) {
			// Check Spot 1 (Adjacent)
			int x1 = px + (int)dir.x;
			int y1 = py + (int)dir.y;

			bool pos1Blocked = false;

			if (x1 >= 0 && x1 < BOARD_WIDTH && y1 >= 0 && y1 < BOARD_HEIGHT) {
				if (board[x1][y1].hasWall) {
					pos1Blocked = true; // Wall blocks line
				} else {
					// It's open space or unit.
					// Mark as valid target if it has a player (Red)
					// Or if clicked/selected, we can mark it Green to show range
					if (board[x1][y1].hasPlayer) {
						board[x1][y1].isTargetable = true;
					} else if (selectedCardIndex != -1) {
						// Optional: Mark empty path as targetable to show range
						// board[x1][y1].isTargetable = true; // Uncomment if you want to click empty space to stab
					}
				}
			}

			// Check Spot 2 (2 Squares / 5ft away)
			// Can only hit Spot 2 if Spot 1 wasn't a wall
			if (!pos1Blocked) {
				int x2 = px + (int)dir.x * 2;
				int y2 = py + (int)dir.y * 2;

				if (x2 >= 0 && x2 < BOARD_WIDTH && y2 >= 0 && y2 < BOARD_HEIGHT) {
					if (!board[x2][y2].hasWall) {
						if (board[x2][y2].hasPlayer) {
							board[x2][y2].isTargetable = true;
						}
					}
				}
			}
		}
	}
	// --- SPECIAL LOGIC: TELEPORT ---
	else if (card.type == CARD_TELEPORT) {
		// Max range of 3d6 is 18.
		// We use Edge-to-Edge distance:
		// Adjacent = 0ft, 1 Gap = 5ft, 2 Gaps = 10ft, 3 Gaps = 15ft.

		for (int x = 0; x < BOARD_WIDTH; x++) {
			for (int y = 0; y < BOARD_HEIGHT; y++) {
				if (x == px && y == py) continue; // Can't teleport to self

				// Calculate Edge-to-Edge Distance
				float delta_x = std::max(0.0f, std::max(px - (x + 1.0f), x - (px + 1.0f)));
				float delta_y = std::max(0.0f, std::max(py - (y + 1.0f), y - (py + 1.0f)));
				float distFeet = sqrt(delta_x * delta_x + delta_y * delta_y) * 5.0f;

				// 1. Check Distance (Max 18ft)
				if (distFeet <= 18.0f) {
					// 2. Check Valid Destination (Must be empty)
					if (!board[x][y].hasWall && !board[x][y].hasPlayer) {
						board[x][y].isTargetable = true;
					}
				}
			}
		}
	}
	// --- 3. GENERAL TILE LOGIC (Everything else including Rock Crush) ---
	else {
		for (int x = 0; x < BOARD_WIDTH; x++) {
			for (int y = 0; y < BOARD_HEIGHT; y++) {
				bool isValid = false;

				switch (card.targeting) {
				case TARGET_ANY_TILE:
					isValid = !board[x][y].hasWall;
					break;
				case TARGET_EMPTY_TILE:
					isValid = !board[x][y].hasWall && !board[x][y].hasPlayer;
					break;
				case TARGET_WALL:
					isValid = board[x][y].hasWall;
					break;
				case TARGET_ADJACENT_UNIT: {
					bool hasUnit = board[x][y].hasPlayer;
					int distance = abs(x - px) + abs(y - py);
					isValid = hasUnit && (distance == 1);
					break;
				}
				case TARGET_ADJACENT_OR_SELF_UNIT: {
					bool hasUnit = board[x][y].hasPlayer;
					int distance = abs(x - px) + abs(y - py);
					isValid = hasUnit && (distance <= 1);
					break;
				}
					// --- Rock Crush ---
				case TARGET_ADJACENT_UNIT_OR_WALL: {
					int distance = abs(x - px) + abs(y - py);
					if (distance == 1) {
						if (board[x][y].hasWall || board[x][y].hasPlayer) {
							isValid = true;
						}
					}
					break;
				}

				// --- PASTE HERE ---
				case TARGET_EMPTY_ADJACENT: {
					int dist = abs(x - px) + abs(y - py);
					if (dist == 1 && !board[x][y].hasWall && !board[x][y].hasPlayer) {
						isValid = true;
					}
					break;
				}
					// ------------------

				case TARGET_SELF:
					isValid = false;
					break;
				default:
					break;
				}

				if (isValid) {
					board[x][y].isTargetable = true;
				}
			}
		}
	}
}
//--------------------------------------------------------------
void ofApp::invalidateTargetCache() {
	lastCachedPlayerX = -1;
	lastCachedPlayerY = -1;
	lastCachedCardIndex = -1;
}
//--------------------------------------------------------------
void ofApp::clearHighlights() {
	for (int x = 0; x < BOARD_WIDTH; x++)
		for (int y = 0; y < BOARD_HEIGHT; y++)
			board[x][y].isHighlighted = false;
	hoverPath.clear();
}

//--------------------------------------------------------------
std::vector<glm::vec2> ofApp::findShortestPath(glm::vec2 start, glm::vec2 end) {
	std::vector<glm::vec2> path;
	for (int i = 0; i < BOARD_WIDTH; i++)
		for (int j = 0; j < BOARD_HEIGHT; j++) {
			board[i][j].visited = false;
			board[i][j].parent = { -1, -1 };
		}
	std::queue<glm::vec2> q;
	q.push(start);
	board[(int)start.x][(int)start.y].visited = true;
	bool found = false;
	while (!q.empty()) {
		glm::vec2 current = q.front();
		q.pop();
		if (current.x == end.x && current.y == end.y) {
			found = true;
			break;
		}
		glm::vec2 neighbors[4] = { { current.x, current.y + 1 }, { current.x, current.y - 1 }, { current.x + 1, current.y }, { current.x - 1, current.y } };
		for (auto & neighbor : neighbors) {
			int nx = neighbor.x, ny = neighbor.y;
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && !board[nx][ny].visited && !board[nx][ny].hasWall && !board[nx][ny].hasPlayer) {
				board[nx][ny].visited = true;
				board[nx][ny].parent = current;
				q.push(neighbor);
			}
		}
	}
	if (found) {
		glm::vec2 current = end;
		while (current.x != -1) {
			path.push_back(current);
			current = board[(int)current.x][(int)current.y].parent;
		}
		std::reverse(path.begin(), path.end());
	}
	return path;
}

//--------------------------------------------------------------
glm::vec3 ofApp::gridToWorld(int gridX, int gridY) {
	float worldX = (gridX - BOARD_WIDTH / 2.0f) * TILE_SIZE + (TILE_SIZE / 2.0f);
	float worldZ = (gridY - BOARD_HEIGHT / 2.0f) * TILE_SIZE + (TILE_SIZE / 2.0f);
	return glm::vec3(worldX, 0, worldZ);
}

//--------------------------------------------------------------
void ofApp::calculateHighlights() {
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			board[x][y].isHighlighted = false;
			board[x][y].visited = false;
		}
	}

	std::queue<std::pair<glm::vec2, int>> q;
	glm::vec2 startPos = { (float)selectedPieceGridX, (float)selectedPieceGridY };

	q.push({ startPos, 0 });
	board[(int)startPos.x][(int)startPos.y].visited = true;

	while (!q.empty()) {
		auto current = q.front();
		q.pop();
		glm::vec2 currentPos = current.first;
		int currentCost = current.second;

		if (currentPos != startPos) {
			board[(int)currentPos.x][(int)currentPos.y].isHighlighted = true;
		}

		glm::vec2 neighbors[4] = { { currentPos.x + 1, currentPos.y }, { currentPos.x - 1, currentPos.y }, { currentPos.x, currentPos.y + 1 }, { currentPos.x, currentPos.y - 1 } };
		for (auto & neighbor : neighbors) {
			int nx = neighbor.x, ny = neighbor.y;
			int nextCost = currentCost + 1;
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && !board[nx][ny].hasWall && !board[nx][ny].hasPlayer && !board[nx][ny].visited && nextCost <= currentAP) {
				board[nx][ny].visited = true;
				q.push({ neighbor, nextCost });
			}
		}
	}
}
//--------------------------------------------------------------
glm::quat ofApp::matchFaceToCamera(glm::vec3 faceNormal) {
	// The target direction is UP (0, 1, 0)
	glm::vec3 target(0, 1, 0);

	// Normalize just in case
	faceNormal = glm::normalize(faceNormal);

	// Get rotation from Face Normal -> Target (Up)
	return glm::rotation(faceNormal, target);
}
//--------------------------------------------------------------
int ofApp::startDiceRoll(int numDice, int sides, DicePurpose purpose) {
	int totalRollResult = 0;

	for (int i = 0; i < numDice; ++i) {
		DiceRoll newRoll;
		newRoll.purpose = purpose;
		newRoll.sides = sides;

		std::uniform_int_distribution<int> dist(1, sides);
		newRoll.result = dist(rng);
		totalRollResult += newRoll.result;
		newRoll.startTime = ofGetElapsedTimef();

		// --- LOGGING ---
		string diceName = (sides == 2) ? "Coin" : "D" + ofToString(sides);
		string outcome = ofToString(newRoll.result);
		if (sides == 2) {
			outcome += (newRoll.result == 1) ? " (Tails)" : " (Heads)";
		}
		ofLogNotice("Dice") << diceName << " landed on: " << outcome;

		// SAFE AXIS GENERATION
		std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);
		glm::vec3 rndAxis(axisDist(rng), axisDist(rng), axisDist(rng));
		if (glm::length(rndAxis) < 0.01f) rndAxis = glm::vec3(0, 1, 0);
		newRoll.rotationAxis = glm::normalize(rndAxis);

		// RANDOM GENERATORS
		std::uniform_real_distribution<float> spinDist(0.0f, 360.0f);
		std::uniform_real_distribution<float> stableDist(-25.0f, 25.0f);

		// --- 1. COIN FLIP (SIDES == 2) ---
		if (sides == 2) {
			glm::quat faceRotation;

			// Helpers
			glm::quat flip180X = glm::angleAxis(glm::radians(180.0f), glm::vec3(1, 0, 0)); // Flip to Bottom
			glm::quat rot180Y = glm::angleAxis(glm::radians(180.0f), glm::vec3(0, 1, 0)); // Spin 180

			if (newRoll.result == 1) { // TAILS

				faceRotation = glm::quat(1, 0, 0, 0);
			} else { // HEADS

				faceRotation = flip180X * rot180Y;
			}

			glm::quat randomYaw = glm::angleAxis(glm::radians(ofRandom(-15, 15)), glm::vec3(0, 1, 0));
			newRoll.finalQuat = randomYaw * faceRotation;
		}

		// --- 2. D4 LOGIC ---
		else if (sides == 4) {
			glm::vec3 faceVec;
			float correctionDeg = 0.0f;

			switch (newRoll.result) {
			case 1:
				faceVec = glm::vec3(0, 1, 0);
				correctionDeg = 0.0f;
				break;
			case 2:
				faceVec = glm::vec3(-0.471f, -0.333f, -0.816f);
				correctionDeg = 180.0f;
				break;
			case 3:
				faceVec = glm::vec3(-0.471f, -0.333f, 0.816f);
				correctionDeg = 0.0f;
				break;
			case 4:
				faceVec = glm::vec3(0.943f, -0.333f, 0.0f);
				correctionDeg = 180.0f;
				break;
			}

			glm::quat align = matchFaceToCamera(faceVec);
			glm::quat manualRot = glm::angleAxis(glm::radians(correctionDeg), glm::vec3(0, 1, 0));
			glm::quat wobble = glm::angleAxis(glm::radians(stableDist(rng)), glm::vec3(0, 1, 0));
			newRoll.finalQuat = wobble * manualRot * align;
		}
		// --- 3. D6 LOGIC ---
		else if (sides == 6) {
			glm::quat faceRotation;
			switch (newRoll.result) {
			case 1:
				faceRotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1, 0, 0));
				break;
			case 6:
				faceRotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0));
				break;
			case 2:
				faceRotation = glm::quat(1, 0, 0, 0);
				break;
			case 5:
				faceRotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(1, 0, 0));
				break;
			case 3:
				faceRotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1));
				break;
			case 4:
				faceRotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 0, 1));
				break;
			}
			glm::quat randomYaw = glm::angleAxis(glm::radians(spinDist(rng)), glm::vec3(0, 1, 0));
			newRoll.finalQuat = randomYaw * faceRotation;
		}
		// --- 4. D10 LOGIC ---
		else if (sides == 10) {
			int n = newRoll.result;
			glm::vec3 faceVec;
			switch (n) {
			case 2:
				faceVec = glm::vec3(cos(glm::radians(0.0f)), 1.0f, sin(glm::radians(0.0f)));
				break;
			case 4:
				faceVec = glm::vec3(cos(glm::radians(72.0f)), 1.0f, sin(glm::radians(72.0f)));
				break;
			case 6:
				faceVec = glm::vec3(cos(glm::radians(144.0f)), 1.0f, sin(glm::radians(144.0f)));
				break;
			case 8:
				faceVec = glm::vec3(cos(glm::radians(216.0f)), 1.0f, sin(glm::radians(216.0f)));
				break;
			case 10:
				faceVec = glm::vec3(cos(glm::radians(288.0f)), 1.0f, sin(glm::radians(288.0f)));
				break;
			case 1:
				faceVec = glm::vec3(cos(glm::radians(36.0f)), -1.0f, sin(glm::radians(36.0f)));
				break;
			case 3:
				faceVec = glm::vec3(cos(glm::radians(108.0f)), -1.0f, sin(glm::radians(108.0f)));
				break;
			case 5:
				faceVec = glm::vec3(cos(glm::radians(180.0f)), -1.0f, sin(glm::radians(180.0f)));
				break;
			case 7:
				faceVec = glm::vec3(cos(glm::radians(252.0f)), -1.0f, sin(glm::radians(252.0f)));
				break;
			case 9:
				faceVec = glm::vec3(cos(glm::radians(324.0f)), -1.0f, sin(glm::radians(324.0f)));
				break;
			default:
				faceVec = glm::vec3(0, 1, 0);
				break;
			}
			glm::quat align = matchFaceToCamera(faceVec);
			glm::quat randomYaw = glm::angleAxis(glm::radians(stableDist(rng)), glm::vec3(0, 1, 0));
			newRoll.finalQuat = randomYaw * align;
		}
		// --- 5. D20 LOGIC ---
		else if (sides == 20) {
			int n = newRoll.result;
			glm::vec3 v;
			switch (n) {
			case 20:
				v = glm::vec3(0, 1, 0);
				break;
			case 1:
				v = glm::vec3(0, -1, 0);
				break;
			case 2:
				v = glm::vec3(0.894, 0.447, 0.0);
				break;
			case 8:
				v = glm::vec3(0.276, 0.447, 0.851);
				break;
			case 14:
				v = glm::vec3(-0.724, 0.447, 0.526);
				break;
			case 12:
				v = glm::vec3(-0.724, 0.447, -0.526);
				break;
			case 18:
				v = glm::vec3(0.276, 0.447, -0.851);
				break;
			case 4:
				v = glm::vec3(0.724, 0.1, 0.526);
				break;
			case 6:
				v = glm::vec3(-0.276, 0.1, 0.851);
				break;
			case 10:
				v = glm::vec3(-0.894, 0.1, 0.0);
				break;
			case 16:
				v = glm::vec3(-0.276, 0.1, -0.851);
				break;
			case 19:
				v = glm::vec3(0.724, 0.1, -0.526);
				break;
			case 17:
				v = glm::vec3(0.724, -0.1, 0.526);
				break;
			case 15:
				v = glm::vec3(-0.276, -0.1, 0.851);
				break;
			case 11:
				v = glm::vec3(-0.894, -0.1, 0.0);
				break;
			case 5:
				v = glm::vec3(-0.276, -0.1, -0.851);
				break;
			case 3:
				v = glm::vec3(0.724, -0.1, -0.526);
				break;
			case 13:
				v = glm::vec3(0.276, -0.447, 0.851);
				break;
			case 9:
				v = glm::vec3(-0.724, -0.447, 0.526);
				break;
			case 7:
				v = glm::vec3(-0.724, -0.447, -0.526);
				break;
			default:
				v = glm::vec3(0.894, -0.447, 0.0);
				break;
			}
			glm::quat align = matchFaceToCamera(v);
			glm::quat randomYaw = glm::angleAxis(glm::radians(stableDist(rng)), glm::vec3(0, 1, 0));
			newRoll.finalQuat = randomYaw * align;
		}

		activeDiceRolls.push_back(newRoll);
	}
	return totalRollResult;
}
//--------------------------------------------------------------
std::vector<Player *> ofApp::findCleaveTargets(glm::vec2 direction) {
	std::vector<Player *> hittablePlayers;
	if (players.empty() || currentPlayerIndex < 0) return hittablePlayers;

	int px = players[currentPlayerIndex].x;
	int py = players[currentPlayerIndex].y;

	glm::vec2 centerTile = { px + direction.x, py + direction.y };
	std::vector<glm::vec2> cleaveTiles;

	if (direction.y != 0) { // Vertical
		cleaveTiles.push_back({ centerTile.x - 1, centerTile.y });
		cleaveTiles.push_back({ centerTile.x, centerTile.y });
		cleaveTiles.push_back({ centerTile.x + 1, centerTile.y });
	} else { // Horizontal
		cleaveTiles.push_back({ centerTile.x, centerTile.y - 1 });
		cleaveTiles.push_back({ centerTile.x, centerTile.y });
		cleaveTiles.push_back({ centerTile.x, centerTile.y + 1 });
	}

	for (const auto & targetPos : cleaveTiles) {
		int tx = targetPos.x;
		int ty = targetPos.y;

		bool isPotentialTarget = false;
		if (tx >= 0 && tx < BOARD_WIDTH && ty >= 0 && ty < BOARD_HEIGHT && !board[tx][ty].hasWall) {
			for (auto & p : players) {
				if (p.x == tx && p.y == ty) {
					isPotentialTarget = true;
					break;
				}
			}
		}
		if (!isPotentialTarget) {
			continue;
		}

		bool pathFound = false;
		std::queue<glm::vec2> q;
		auto comp = [](const glm::vec2 & a, const glm::vec2 & b) {
			return a.x < b.x || (a.x == b.x && a.y < b.y);
		};
		std::set<glm::vec2, decltype(comp)> visited(comp);

		q.push({ (float)px, (float)py });
		visited.insert({ (float)px, (float)py });

		while (!q.empty()) {
			glm::vec2 current = q.front();
			q.pop();

			if (current.x == tx && current.y == ty) {
				pathFound = true;
				break;
			}

			int dist = abs(current.x - px) + abs(current.y - py);
			if (dist >= 2) {
				continue;
			}

			glm::vec2 neighbors[4] = { { current.x + 1, current.y }, { current.x - 1, current.y }, { current.x, current.y + 1 }, { current.x, current.y - 1 } };
			for (const auto & neighbor : neighbors) {
				int nx = neighbor.x;
				int ny = neighbor.y;
				if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && !board[nx][ny].hasWall && visited.find(neighbor) == visited.end()) {
					visited.insert(neighbor);
					q.push(neighbor);
				}
			}
		}

		if (pathFound) {
			for (auto & p : players) {
				if (p.x == tx && p.y == ty) {
					hittablePlayers.push_back(&p);
					break;
				}
			}
		}
	}
	return hittablePlayers;
}
//--------------------------------------------------------------
void ofApp::cancelDispel() {
	isDispelMenuOpen = false;
	isDispelTargeting = false;
	isDispelStatusSelectOpen = false;
	pendingDispelCardIndex = -1;
	pendingDispelTargetIndex = -1;
	ofLogNotice("Dispel") << "Cancelled.";
}

//--------------------------------------------------------------
void ofApp::drawDispelUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// --- PHASE 1: INITIAL CHOICE ---
	if (isDispelMenuOpen) {
		// Dark Overlay
		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

		// Menu Background
		ofSetColor(30, 30, 60, 255);
		ofDrawRectRounded(dispelMenuRect, 15);

		// Text
		ofSetColor(ofColor::white);
		string title = "Choose Dispel Effect";
		ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
		uiFont.drawString(title, dispelMenuRect.getCenter().x - titleBox.width / 2, dispelMenuRect.y + 60);

		// Barrier Button
		ofSetColor(ofColor::hotPink);
		ofDrawRectRounded(dispelBtnBarrier, 10);
		ofSetColor(ofColor::black);

		// CHANGED: Renamed to Non-Physical Barrier
		string bText = "Gain Non-Phys Barrier\n(1d20 vs Magic/Fire/etc)";

		// Simple centering logic...
		uiFont.drawString("Non-Phys Barrier", dispelBtnBarrier.x + 20, dispelBtnBarrier.y + 40);

		// Purge Button
		ofSetColor(ofColor::cyan);
		ofDrawRectRounded(dispelBtnPurge, 10);
		ofSetColor(ofColor::black);
		uiFont.drawString("Remove Status", dispelBtnPurge.x + 20, dispelBtnPurge.y + 40);
	}

	// --- PHASE 2: TARGETING ---
	if (isDispelTargeting) {
		// Just a text prompt at top of screen
		ofSetColor(0, 0, 0, 200);
		ofDrawRectRounded(ofGetWidth() / 2 - 300, 50, 600, 60, 10);
		ofSetColor(ofColor::white);
		string text = "Select Self or Adjacent Unit to Cure";
		uiFont.drawString(text, ofGetWidth() / 2 - 200, 90);
	}

	// --- PHASE 3: STATUS SELECTION ---
	if (isDispelStatusSelectOpen) {
		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

		ofSetColor(30, 30, 60, 255);
		ofDrawRectRounded(statusSelectMenuRect, 15);

		ofSetColor(ofColor::white);
		uiFont.drawString("Which status to remove?", statusSelectMenuRect.x + 20, statusSelectMenuRect.y + 40);

		for (size_t i = 0; i < statusSelectButtons.size(); i++) {
			ofSetColor(ofColor::orange);
			ofDrawRectRounded(statusSelectButtons[i], 8);
			ofSetColor(ofColor::black);
			uiFont.drawString(statusSelectLabels[i], statusSelectButtons[i].x + 10, statusSelectButtons[i].y + 30);
		}
	}
}

//--------------------------------------------------------------
void ofApp::determineStatusOptions(Player * target) {
	statusSelectLabels.clear();
	statusSelectButtons.clear();

	if (target->onFire) statusSelectLabels.push_back("Fire");
	if (target->isParalyzed) statusSelectLabels.push_back("Paralysis");
	// Add future statuses here

	if (statusSelectLabels.empty()) {
		ofSystemAlertDialog("Target has no status effects!");
		cancelDispel();
		return;
	}

	// If only one, apply immediately
	if (statusSelectLabels.size() == 1) {
		applyDispelEffect(0);
		return;
	}

	// Otherwise, build menu
	float w = 400;
	float h = 60 + (statusSelectLabels.size() * 60);
	float x = ofGetWidth() / 2 - w / 2;
	float y = ofGetHeight() / 2 - h / 2;
	statusSelectMenuRect.set(x, y, w, h);

	for (size_t i = 0; i < statusSelectLabels.size(); i++) {
		statusSelectButtons.push_back(ofRectangle(x + 20, y + 60 + (i * 60), w - 40, 50));
	}

	isDispelTargeting = false;
	isDispelStatusSelectOpen = true;
}

//--------------------------------------------------------------
void ofApp::applyDispelEffect(int statusIndex) {
	Player * target = getPlayer(pendingDispelTargetIndex);
	if (!target) return;

	string statusToRemove = statusSelectLabels[statusIndex];
	if (statusToRemove == "Fire") target->onFire = false;
	if (statusToRemove == "Paralysis") {
		target->isParalyzed = false;
		target->paralysisHeadsCount = 0;
	}

	ofLogNotice("Dispel") << "Removed " << statusToRemove;

	// FINALIZATION: Deduct AP and Card
	if (pendingDispelCardIndex != -1) {
		Player & p = players[currentPlayerIndex];
		currentAP -= p.hand[pendingDispelCardIndex].cost;
		p.discardPile.push_back(p.hand[pendingDispelCardIndex]);
		p.hand.erase(p.hand.begin() + pendingDispelCardIndex);
		calculateTargetHighlights(); // refresh UI
	}

	cancelDispel(); // Close menus
}
// ----------------- WISDOM BOON HELPERS -----------------

void ofApp::cancelWisdomBoon() {
	isWisdomBoonMenuOpen = false;
	pendingWisdomBoonCardIndex = -1;
	pendingWisdomBoonTargetIndex = -1;
	ofLogNotice("WisdomBoon") << "Cancelled.";
}
//--------------------------------------------------------------
void ofApp::drawWisdomBoonUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// 1. Dark Overlay
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// 2. Menu Background
	ofSetColor(40, 40, 80, 255);
	ofDrawRectRounded(wisdomMenuRect, 15);

	// 3. Determine Context
	bool isSelfTarget = (pendingWisdomBoonTargetIndex == currentPlayerIndex);
	int deckSize = 0;
	if (currentPlayerIndex >= 0) deckSize = players[currentPlayerIndex].deck.size();

	// 4. Title & Description
	ofSetColor(ofColor::white);
	string title = "Wisdom Boon";
	string desc = "Effect Strength: " + ofToString(deckSize) + " (Your Deck Size)";

	ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
	uiFont.drawString(title, wisdomMenuRect.getCenter().x - titleBox.width / 2, wisdomMenuRect.y + 50);

	ofRectangle descBox = uiFont.getStringBoundingBox(desc, 0, 0);
	uiFont.drawString(desc, wisdomMenuRect.getCenter().x - descBox.width / 2, wisdomMenuRect.y + 90);

	// 5. Draw Context-Sensitive Button
	string btnText = "";
	if (isSelfTarget) {
		// BLOCK MODE
		ofSetColor(ofColor::slateGray); // Grey for Block
		btnText = "Gain Block";
	} else {
		// DAMAGE MODE
		ofSetColor(ofColor::purple); // Purple for Magic
		btnText = "Deal Magic Dmg";
	}

	ofDrawRectRounded(wisdomBtnDamage, 10);

	ofSetColor(ofColor::white);
	ofRectangle btnBox = uiFont.getStringBoundingBox(btnText, 0, 0);
	uiFont.drawString(btnText, wisdomBtnDamage.getCenter().x - btnBox.width / 2, wisdomBtnDamage.getCenter().y + btnBox.height / 2);
}
//--------------------------------------------------------------
void ofApp::drawMagicBlastChoiceUI() {
	// Check if player index is valid
	Player * targetPlayer = getPlayer(magicBlastTargetPlayerIndex);
	if (!targetPlayer) return;

	// Draw a semi-transparent overlay to focus the player
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// Panel properties
	float panelWidth = 800;
	float panelHeight = 350;
	float panelX = ofGetWidth() / 2.0f - panelWidth / 2.0f;
	float panelY = ofGetHeight() / 2.0f - panelHeight / 2.0f;

	// Draw the panel background
	ofSetColor(30, 30, 40, 240);
	ofDrawRectRounded(panelX, panelY, panelWidth, panelHeight, 15);

	// Draw the title/prompt
	ofSetColor(ofColor::white);
	string prompt = "Player " + ofToString(targetPlayer->playerID) + ", choose an effect:";
	string choicesLeft = "Choices remaining: " + ofToString(magicBlastChoicesRemaining);

	ofRectangle promptBox = uiFont.getStringBoundingBox(prompt, 0, 0);
	uiFont.drawString(prompt, panelX + panelWidth / 2 - promptBox.getWidth() / 2, panelY + 60);

	ofRectangle choicesBox = uiFont.getStringBoundingBox(choicesLeft, 0, 0);
	uiFont.drawString(choicesLeft, panelX + panelWidth / 2 - choicesBox.getWidth() / 2, panelY + 100);

	// Button properties
	float btnWidth = 350;
	float btnHeight = 100;
	float btnY = panelY + panelHeight - btnHeight - 40;
	float btnSpacing = 20;

	// Define button rects for mouse interaction
	magicBlastDamageButton.set(panelX + panelWidth / 2.0f - btnWidth - btnSpacing / 2.0f, btnY, btnWidth, btnHeight);
	magicBlastDiscardButton.set(panelX + panelWidth / 2.0f + btnSpacing / 2.0f, btnY, btnWidth, btnHeight);

	// Draw Damage Button
	ofSetColor(ofColor::indianRed);
	ofDrawRectRounded(magicBlastDamageButton, 10);
	ofSetColor(ofColor::white);
	string damageText = "Take 5 Damage";
	ofRectangle damageTextBox = uiFont.getStringBoundingBox(damageText, 0, 0);
	uiFont.drawString(damageText, magicBlastDamageButton.getCenter().x - damageTextBox.getWidth() / 2, magicBlastDamageButton.getCenter().y + damageTextBox.getHeight() / 2);

	// Draw Discard Button
	ofSetColor(ofColor::darkSlateBlue);
	ofDrawRectRounded(magicBlastDiscardButton, 10);
	ofSetColor(ofColor::white);
	string discardText = "Remove Top Card of Deck";
	ofRectangle discardTextBox = uiFont.getStringBoundingBox(discardText, 0, 0);
	uiFont.drawString(discardText, magicBlastDiscardButton.getCenter().x - discardTextBox.getWidth() / 2, magicBlastDiscardButton.getCenter().y + discardTextBox.getHeight() / 2);
}
//--------------------------------------------------------------
// --- NEW HELPER: Converts a specific point in world space back to grid coordinates ---
glm::vec2 ofApp::worldToGrid(glm::vec3 worldPos) {
	float gridX = (worldPos.x / TILE_SIZE) + (BOARD_WIDTH / 2.0f);
	float gridY = (worldPos.z / TILE_SIZE) + (BOARD_HEIGHT / 2.0f);
	return glm::vec2(floor(gridX), floor(gridY));
}

//--------------------------------------------------------------
bool ofApp::checkRayPhysics(glm::vec2 rayStart, glm::vec2 rayEnd) {
	auto path = getLineOfSightPath(rayStart, rayEnd);
	if (path.empty()) return true;

	for (size_t i = 0; i < path.size(); ++i) {
		glm::vec2 current = path[i];
		int cx = (int)current.x;
		int cy = (int)current.y;

		// --- 1. CIRCULAR WALL COLLISION ---
		// This handles everything: adjacent walls, grazing shots, etc.
		if (board[cx][cy].hasWall) {
			glm::vec2 wallCenter = current + 0.5f;
			glm::vec2 closest = getClosestPointOnLineSegment(wallCenter, rayStart, rayEnd);
			float dist = glm::distance(closest, wallCenter);

			// If the ray passes inside the circle (radius 0.5), it hits.
			// 0.499f allows shooting exactly along the edge.
			if (dist < 0.499f) return false;
		}

		bool isStart = (glm::distance(current, glm::floor(rayStart)) < 0.5f);
		bool isEnd = (glm::distance(current, glm::floor(rayEnd)) < 0.5f);

		// --- 2. DIAGONAL PINCH CHECK ---
		if (i < path.size() - 1) {
			glm::vec2 next = path[i + 1];
			if ((int)current.x != (int)next.x && (int)current.y != (int)next.y) {
				// Only block if BOTH corners are solid walls.
				// (Squeezing between a Unit and a Wall is technically allowed by this logic,
				// but Squeezing between two Walls is impossible).
				if (isTileWall((int)current.x, (int)next.y) && isTileWall((int)next.x, (int)current.y)) {
					return false;
				}

				// Diagonal Gap Check (Choke Points)
				glm::vec2 neighbors[] = { { current.x, next.y }, { next.x, current.y } };
				for (auto & n : neighbors) {
					if (isTileWall((int)n.x, (int)n.y)) continue;
					if (n == glm::floor(rayStart) || n == glm::floor(rayEnd)) continue;

					int gapType = isGapTile(n);
					if (gapType == 1 && std::abs(rayStart.x - rayEnd.x) > 0.01f) return false;
					if (gapType == 2 && std::abs(rayStart.y - rayEnd.y) > 0.01f) return false;
				}
			}
		}

		// --- 3. STANDARD CHOKE POINT CHECK ---
		if (!isStart && !isEnd && !board[cx][cy].hasWall) {
			int gapType = isGapTile(current);
			if (gapType == 1 && std::abs(rayStart.x - rayEnd.x) > 0.01f) return false;
			if (gapType == 2 && std::abs(rayStart.y - rayEnd.y) > 0.01f) return false;
		}

		// --- 4. UNIT BLOCKING CHECK ---
		if (!isStart && !isEnd) {
			if (board[cx][cy].hasPlayer) {
				glm::vec2 unitCenter = current + 0.5f;
				glm::vec2 closest = getClosestPointOnLineSegment(unitCenter, rayStart, rayEnd);
				// Units are slightly smaller (0.45) to be forgiving
				if (glm::distance(closest, unitCenter) < 0.45f) return false;
			}
		}
	}
	return true;
}

// ----------------- FIXED isLosTargetValid (With Ethereal Jolt Support) -----------------
TargetInfo ofApp::isLosTargetValid(glm::vec2 casterTile, glm::vec2 targetTile, float maxRangeFeet, CardType cardType) {
	TargetInfo result;

	// --- RULE 0: Basic Checks ---
	if (targetTile == casterTile) {
		result.reason = INVALID_SELF;
		return result;
	}
	// Cannot aim inside a wall
	if (board[(int)targetTile.x][(int)targetTile.y].hasWall) {
		result.reason = INVALID_OCCUPIED_BY_WALL;
		return result;
	}

	int dx = abs((int)casterTile.x - (int)targetTile.x);
	int dy = abs((int)casterTile.y - (int)targetTile.y);

	// --- RULE 0.2: ORTHOGONAL NEIGHBOR EXCEPTION (FIX) ---
	// If the tile is directly adjacent (Up/Down/Left/Right), it is ALWAYS physically reachable.
	// We skip the complex face/ray math because the faces are touching.
	if (dx + dy == 1) {
		// Valid physical path. Now check target contents at bottom.
		goto DETERMINE_TARGET_TYPE;
	}

	// --- RULE 0.5: DIAGONAL NEIGHBOR EXCEPTION ---
	if (dx == 1 && dy == 1) {
		if (isTileBlocked(casterTile.x, targetTile.y) && isTileBlocked(targetTile.x, casterTile.y)) {
			result.reason = INVALID_HARD_COVER;
			return result;
		}
		goto DETERMINE_TARGET_TYPE;
	}

	// --- RULE 0.1: STRICT ORTHOGONAL BLOCK (Long Distance) ---
	// If further away but in same row/col, check for walls in between.
	if (isOrthogonalPathBlocked(casterTile, targetTile) && cardType != CARD_ETHEREAL_JOLT) {
		result.reason = INVALID_HARD_COVER;
		return result;
	}

	// --- MAIN LOGIC ---
	{
		float maxFaceDist = maxRangeFeet / 5.0f;

		// *** SPECIAL CASE: ETHEREAL JOLT ***
		// Ignores Walls/Cover.
		if (cardType == CARD_ETHEREAL_JOLT) {
			// Use simple Euclidian edge-to-edge for range check
			float centerDist = glm::distance(casterTile, targetTile);
			float edgeDist = std::max(0.0f, centerDist - 1.0f);

			if (edgeDist > maxFaceDist + 0.001f) {
				result.reason = INVALID_OUT_OF_RANGE;
				return result;
			}
			goto DETERMINE_TARGET_TYPE;
		}

		glm::vec2 faceOffsets[] = { { 0.5f, 0.0f }, { 0.5f, 1.0f }, { 0.0f, 0.5f }, { 1.0f, 0.5f } };
		glm::vec2 faceDirs[] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };

		// --- PHASE 1: THE COVER RULE ---
		// Find absolute minimum distance
		float minGeoDist = std::numeric_limits<float>::max();
		for (int j = 0; j < 4; j++) {
			float d = glm::distance(casterTile + 0.5f, targetTile + faceOffsets[j]);
			if (d < minGeoDist) minGeoDist = d;
		}

		// Check if ALL shortest faces are blocked
		bool isCovered = true;
		const float TOLERANCE = 0.001f;

		for (int j = 0; j < 4; j++) {
			float d = glm::distance(casterTile + 0.5f, targetTile + faceOffsets[j]);
			if (d <= minGeoDist + TOLERANCE) {
				int tx = (int)targetTile.x + (int)faceDirs[j].x;
				int ty = (int)targetTile.y + (int)faceDirs[j].y;
				// If at least one shortest face is NOT a wall, the target is exposed.
				if (!isTileWall(tx, ty)) {
					isCovered = false;
					break;
				}
			}
		}

		if (isCovered) {
			result.reason = INVALID_NO_LOS;
			return result;
		}

		// --- PHASE 2: FIND BEST PATH ---
		float shortestPathDist = std::numeric_limits<float>::max();
		glm::vec2 bestOrigin = { -1, -1 }, bestDest = { -1, -1 };
		bool foundPath = false;

		for (int i = 0; i < 4; i++) {
			int cx = (int)casterTile.x + (int)faceDirs[i].x;
			int cy = (int)casterTile.y + (int)faceDirs[i].y;
			if (isTileWall(cx, cy)) continue;

			glm::vec2 origin = casterTile + faceOffsets[i];

			for (int j = 0; j < 4; j++) {
				int tx = (int)targetTile.x + (int)faceDirs[j].x;
				int ty = (int)targetTile.y + (int)faceDirs[j].y;
				if (isTileWall(tx, ty)) continue;

				glm::vec2 dest = targetTile + faceOffsets[j];
				float d = glm::distance(origin, dest);

				if (d <= maxFaceDist + 0.001f) {
					if (d < shortestPathDist) {
						if (checkRayPhysics(origin, dest)) {
							shortestPathDist = d;
							bestOrigin = origin;
							bestDest = dest;
							foundPath = true;
						}
					}
				}
			}
		}

		if (!foundPath) {
			float rawDist = getFaceToFaceDistance(casterTile, targetTile);
			result.reason = (rawDist > maxFaceDist + 0.001f) ? INVALID_OUT_OF_RANGE : INVALID_HARD_COVER;
			return result;
		}
	}

DETERMINE_TARGET_TYPE:
	// --- RULE 5: VALIDATE TARGET CONTENTS ---
	result.reason = VALID;
	bool isOccupied = board[(int)targetTile.x][(int)targetTile.y].hasPlayer;

	if (isOccupied) {
		result.isTargetable = true;
	} else {
		// Only Magic Blast targets empty squares if neighbors exist
		if (cardType == CARD_MAGIC_BLAST) {
			bool hasNeighbor = false;
			glm::vec2 neighbors[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
			for (auto n : neighbors) {
				int nx = (int)targetTile.x + n.x, ny = (int)targetTile.y + n.y;
				if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && board[nx][ny].hasPlayer) {
					hasNeighbor = true;
					break;
				}
			}
			result.isTargetable = hasNeighbor;
		} else {
			result.isTargetable = false;
		}
	}
	return result;
}
//--------------------------------------------------------------
std::vector<glm::vec2> ofApp::getLineOfSightPath(glm::vec2 startPoint, glm::vec2 endPoint) {
	std::vector<glm::vec2> path;

	glm::vec2 startTile = { floor(startPoint.x), floor(startPoint.y) };
	glm::vec2 endTile = { floor(endPoint.x), floor(endPoint.y) };

	path.push_back(startTile);

	if (startTile.x == endTile.x && startTile.y == endTile.y) {
		return path;
	}

	glm::vec2 dir = endPoint - startPoint;
	if (dir.x == 0 && dir.y == 0) return path;

	glm::vec2 step = { (dir.x >= 0) ? 1.0f : -1.0f, (dir.y >= 0) ? 1.0f : -1.0f };

	float tMaxX, tMaxY, tDeltaX, tDeltaY;

	if (dir.x == 0) {
		tMaxX = std::numeric_limits<float>::infinity();
		tDeltaX = std::numeric_limits<float>::infinity();
	} else {
		float nextBoundaryX = (step.x > 0) ? (startTile.x + 1.0f) : startTile.x;
		tMaxX = (nextBoundaryX - startPoint.x) / dir.x;
		tDeltaX = abs(1.0f / dir.x);
	}

	if (dir.y == 0) {
		tMaxY = std::numeric_limits<float>::infinity();
		tDeltaY = std::numeric_limits<float>::infinity();
	} else {
		float nextBoundaryY = (step.y > 0) ? (startTile.y + 1.0f) : startTile.y;
		tMaxY = (nextBoundaryY - startPoint.y) / dir.y;
		tDeltaY = abs(1.0f / dir.y);
	}

	glm::vec2 currentTile = startTile;

	while (true) {
		if (tMaxX < tMaxY) {
			currentTile.x += step.x;
			tMaxX += tDeltaX;
		} else {
			currentTile.y += step.y;
			tMaxY += tDeltaY;
		}

		path.push_back(currentTile);

		if (currentTile.x == endTile.x && currentTile.y == endTile.y) {
			break;
		}

		if (path.size() > (BOARD_WIDTH + BOARD_HEIGHT)) break;
	}
	return path;
}
// ----------------- HELPERS -----------------

// Returns true if the tile is a Wall, Unit, or Out of Bounds
bool ofApp::isTileBlocked(int x, int y) {
	if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) return true;
	if (board[x][y].hasWall) return true;
	if (board[x][y].hasPlayer) return true;
	return false;
}

// Returns true ONLY if the tile is a Wall or Out of Bounds (ignores Units)
bool ofApp::isTileWall(int x, int y) {
	if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) return true;
	return board[x][y].hasWall;
}

// 0 = Open
// 1 = Vertical Choke (Blocked Left & Right) -> Requires Vertical Shot
// 2 = Horizontal Choke (Blocked Up & Down) -> Requires Horizontal Shot
int ofApp::isGapTile(glm::vec2 tile) {
	int x = (int)tile.x;
	int y = (int)tile.y;
	if (board[x][y].hasWall) return 0;

	bool blockLeft = isTileBlocked(x - 1, y);
	bool blockRight = isTileBlocked(x + 1, y);
	bool blockUp = isTileBlocked(x, y - 1);
	bool blockDown = isTileBlocked(x, y + 1);

	if (blockLeft && blockRight) return 1; // Vertical Choke
	if (blockUp && blockDown) return 2; // Horizontal Choke

	return 0;
}
//--------------------------------------------------------------
glm::vec2 ofApp::getClosestPointOnLineSegment(glm::vec2 p, glm::vec2 start, glm::vec2 end) {
	glm::vec2 lineVec = end - start;
	float lenSq = glm::dot(lineVec, lineVec);
	if (lenSq < 1e-5f) return start;

	glm::vec2 pVec = p - start;
	float t = glm::dot(pVec, lineVec) / lenSq;
	t = std::max(0.0f, std::min(1.0f, t));

	return start + lineVec * t;
}
//--------------------------------------------------------------
float ofApp::getFaceToFaceDistance(glm::vec2 casterTile, glm::vec2 targetTile) {
	glm::vec2 faceOffsets[] = { { 0.5f, 0.0f }, { 0.5f, 1.0f }, { 0.0f, 0.5f }, { 1.0f, 0.5f } };
	glm::vec2 faceDirs[] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };

	// 1. Identify Valid Target Faces (Using isTileBlocked)
	float minGeoDist = std::numeric_limits<float>::max();
	for (int j = 0; j < 4; j++) {
		float d = glm::distance(casterTile + 0.5f, targetTile + faceOffsets[j]);
		if (d < minGeoDist) minGeoDist = d;
	}

	std::vector<int> validTargetFaces;
	for (int j = 0; j < 4; j++) {
		float d = glm::distance(casterTile + 0.5f, targetTile + faceOffsets[j]);
		if (d <= minGeoDist + 0.001f) {
			int tx = (int)targetTile.x + (int)faceDirs[j].x;
			int ty = (int)targetTile.y + (int)faceDirs[j].y;
			if (!isTileBlocked(tx, ty)) validTargetFaces.push_back(j);
		}
	}

	if (validTargetFaces.empty()) return std::numeric_limits<float>::max();

	// 2. Find shortest path
	float shortestDist = std::numeric_limits<float>::max();
	bool foundPath = false;

	for (int i = 0; i < 4; i++) {
		int cx = (int)casterTile.x + (int)faceDirs[i].x;
		int cy = (int)casterTile.y + (int)faceDirs[i].y;
		if (isTileBlocked(cx, cy)) continue; // Blocked by Wall OR Unit

		glm::vec2 origin = casterTile + faceOffsets[i];

		for (int tIdx : validTargetFaces) {
			glm::vec2 dest = targetTile + faceOffsets[tIdx];
			float d = glm::distance(origin, dest);
			if (d < shortestDist) {
				shortestDist = d;
				foundPath = true;
			}
		}
	}

	if (!foundPath) return std::numeric_limits<float>::max();
	return shortestDist;
}
//--------------------------------------------------------------
bool ofApp::isOrthogonalPathBlocked(glm::vec2 start, glm::vec2 end) {
	int x1 = (int)start.x;
	int y1 = (int)start.y;
	int x2 = (int)end.x;
	int y2 = (int)end.y;

	// 1. Vertical Check (Same Column)
	if (x1 == x2) {
		int minY = std::min(y1, y2);
		int maxY = std::max(y1, y2);
		// Check every tile strictly between start and end
		for (int y = minY + 1; y < maxY; y++) {
			if (isTileWall(x1, y)) return true; // Blocked!
		}
		return false; // Path clear
	}

	// 2. Horizontal Check (Same Row)
	if (y1 == y2) {
		int minX = std::min(x1, x2);
		int maxX = std::max(x1, x2);
		// Check every tile strictly between start and end
		for (int x = minX + 1; x < maxX; x++) {
			if (isTileWall(x, y1)) return true; // Blocked!
		}
		return false; // Path clear
	}

	// Not orthogonal (Diagonal or Angled), so this check doesn't apply
	return false;
}
//--------------------------------------------------------------
void ofApp::updateDebugRects() {
	if (!isDebugMode) return;
	float panelWidth = 220;
	float panelX = ofGetWidth() - panelWidth - 20;
	float panelY = 40;
	if (players.size() >= 2 && currentPlayerIndex != -1) {
		panelY = p1_deckRect.getBottom() + 20;
	}
	float btnH = 45;
	float pad = 10;
	float curY = panelY + pad + uiFont.getLineHeight() + pad;

	debugDrawCardButton.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
	curY += btnH + pad;
	debugSpawnCardButton.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
	curY += btnH + pad;
	debugDiceDropdownButton.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
	curY += btnH + pad;

	if (isDebugDiceDropdownOpen) {
		debugRollD6Button.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
		curY += btnH + pad;
		debugRollD4Button.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
		curY += btnH + pad;
		debugRollD20Button.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
		curY += btnH + pad;
	}

	debugSpawnUnitButton.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
	curY += btnH + pad;
	debugUnlimitedAPButton.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
	curY += btnH + pad;
	debugForceEndTurnButton.set(panelX + pad, curY, panelWidth - 2 * pad, btnH);
	curY += btnH + pad;

	debugPanel.set(panelX, panelY, panelWidth, curY - panelY);
}
//--------------------------------------------------------------
void ofApp::cleanupGame() {
	players.clear();
	activeDiceRolls.clear();
	activeCardDisplays.clear();
	activeStolenCardAnimations.clear();
	activeRemovedCardAnimations.clear();

	for (int x = 0; x < BOARD_WIDTH; ++x) {
		for (int y = 0; y < BOARD_HEIGHT; ++y) {
			board[x][y] = Tile(); // Reset each tile
		}
	}

	currentPlayerIndex = -1;
	playerAction = NONE;
	selectedCardIndex = -1;
	draggedCardIndex = -1;
	isPlayerAnimating = false;
	isLoadingGame = false;

	ofLogNotice("Game") << "--- GAME SESSION CLEANED UP ---";
}
//--------------------------------------------------------------
void ofApp::loadCardData(const std::string & filePath) {
	ofJson json;
	if (!ofFile(filePath).exists()) {
		ofLogError("ofApp::loadCardData") << "Could not find card data file: " << filePath;
		return;
	}
	json = ofLoadJson(filePath);

	allCards.clear();

	const int cardPixelWidth = 409, cardPixelHeight = 585;
	const int numCols = 10;

	for (const auto & cardJson : json) {
		Card newCard;
		int cardId = cardJson.value("id", 0);
		if (cardId == 0) continue;

		newCard.name = cardJson.value("name", "Unnamed");
		newCard.type = stringToCardType(cardJson.value("type", "CARD_NONE"));
		newCard.cost = cardJson.value("cost", 0);
		newCard.value = cardJson.value("value", 0);
		newCard.targeting = stringToTargetingType(cardJson.value("targeting", "TARGET_NONE"));
		newCard.damageType = stringToDamageType(cardJson.value("damageType", "DAMAGE_PHYSICAL"));
		newCard.numDice = cardJson.value("numDice", 0);
		newCard.diceSides = cardJson.value("diceSides", 0);

		// Calculate texture coordinates from the sprite sheet based on ID
		int index = cardId - 1;
		int row = index / numCols;
		int col = index % numCols;
		newCard.textureRect = ofRectangle(col * cardPixelWidth, row * cardPixelHeight, cardPixelWidth, cardPixelHeight);

		allCards.push_back(newCard);
	}
	ofLogNotice("ofApp::loadCardData") << "Loaded " << allCards.size() << " cards from JSON.";
}

CardType ofApp::stringToCardType(const std::string & str) {
	if (str == "CARD_ATTACK_SINGLE_TILE") return CARD_ATTACK_SINGLE_TILE;
	if (str == "CARD_MIND_THEFT") return CARD_MIND_THEFT;
	if (str == "CARD_AMNESIA") return CARD_AMNESIA;
	if (str == "CARD_GAIN_BLOCK") return CARD_GAIN_BLOCK;
	if (str == "CARD_GAIN_WARD") return CARD_GAIN_WARD;
	if (str == "CARD_MAGIC_BLAST") return CARD_MAGIC_BLAST;
	if (str == "CARD_FIREBALL") return CARD_FIREBALL;
	if (str == "CARD_SHOCK") return CARD_SHOCK;
	if (str == "CARD_ROCK_CRUSH") return CARD_ROCK_CRUSH;
	if (str == "CARD_DISPEL") return CARD_DISPEL;
	if (str == "CARD_TELEPORT") return CARD_TELEPORT;
	if (str == "CARD_HASTEN") return CARD_HASTEN;
	if (str == "CARD_REPLICATE") return CARD_REPLICATE;
	if (str == "CARD_WISDOM_BOON") return CARD_WISDOM_BOON;
	if (str == "CARD_ETHEREAL_JOLT") return CARD_ETHEREAL_JOLT;
	if (str == "CARD_FLAME_HIT") return CARD_FLAME_HIT;
	if (str == "CARD_HEAL") return CARD_HEAL;
	if (str == "CARD_RAISE_DEAD") return CARD_RAISE_DEAD;
	return CARD_NONE;
}

TargetingType ofApp::stringToTargetingType(const std::string & str) {
	if (str == "TARGET_ADJACENT_UNIT") return TARGET_ADJACENT_UNIT;
	if (str == "TARGET_SELF") return TARGET_SELF;
	if (str == "TARGET_LINEAR_PIERCE") return TARGET_LINEAR_PIERCE;
	if (str == "TARGET_CLEAVE_ADJACENT") return TARGET_CLEAVE_ADJACENT;
	if (str == "TARGET_ADJACENT_OR_SELF_UNIT") return TARGET_ADJACENT_OR_SELF_UNIT;
	if (str == "TARGET_LINE_OF_SIGHT_TILE") return TARGET_LINE_OF_SIGHT_TILE;
	if (str == "TARGET_ADJACENT_UNIT_OR_WALL") return TARGET_ADJACENT_UNIT_OR_WALL;
	if (str == "TARGET_EMPTY_ADJACENT") return TARGET_EMPTY_ADJACENT;
	return TARGET_NONE;
}

DamageType ofApp::stringToDamageType(const std::string & str) {
	if (str == "DAMAGE_PIERCING") return DAMAGE_PIERCING;
	if (str == "DAMAGE_MAGIC") return DAMAGE_MAGIC;
	if (str == "DAMAGE_FIRE") return DAMAGE_FIRE;
	if (str == "DAMAGE_ELECTRIC") return DAMAGE_ELECTRIC;
	return DAMAGE_PHYSICAL;
}