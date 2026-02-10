#include "ofApp.h"
#include "GLFW/glfw3.h"
#include "SteamManager.h"
#include "ofAppGLFWWindow.h"
#include <algorithm>
#include <cstring>
#include <glm/gtx/intersect.hpp>
#include <limits>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>

//--------------------------------------------------------------
ofPixels scalePixelsNearest(ofPixels & src, int scale) {
	int w = src.getWidth();
	int h = src.getHeight();
	int newW = w * scale;
	int newH = h * scale;

	ofPixels dst;
	dst.allocate(newW, newH, OF_PIXELS_RGBA);

	for (int y = 0; y < newH; y++) {
		for (int x = 0; x < newW; x++) {
			// Sample the nearest original pixel
			dst.setColor(x, y, src.getColor(x / scale, y / scale));
		}
	}
	return dst;
}

//--------------------------------------------------------------
// Helper to create a GLFW cursor from a PNG file
GLFWcursor * createGLFWCursorFromPNG(const std::string & path, int xHot, int yHot) {
	ofImage img;
	if (!img.load(path)) {
		ofLogError("Cursor") << "Failed to load cursor: " << path;
		return nullptr;
	}
	img.setImageType(OF_IMAGE_COLOR_ALPHA);
	GLFWimage glfwImg;
	glfwImg.width = img.getWidth();
	glfwImg.height = img.getHeight();
	glfwImg.pixels = img.getPixels().getData();
	return glfwCreateCursor(&glfwImg, xHot, yHot);
}

// ----------------------------------
void drawStatText(ofTrueTypeFont & font, std::string text, float x, float y, float w, float h, ofColor color, float textScale = 0.6f) {
	if (text == "0") return;
	float scale = textScale;
	ofRectangle bounds = font.getStringBoundingBox(text, 0, 0);
	ofPushMatrix();
	// Center the text in the rect
	ofTranslate(x + (w - bounds.width * scale) / 2, y + (h + bounds.height * scale) / 2 - 2);
	ofScale(scale, scale);
	ofSetColor(color);
	font.drawString(text, 0, 0);
	ofPopMatrix();
}

//--------------------------------------------------------------
static std::string escapeField(const std::string & input) {
	std::string out;
	out.reserve(input.size());
	for (char c : input) {
		switch (c) {
		case '\\':
			out += "\\\\";
			break;
		case '\t':
			out += "\\t";
			break;
		case '\n':
			out += "\\n";
			break;
		case ',':
			out += "\\c";
			break;
		default:
			out += c;
			break;
		}
	}
	return out;
}

static std::string unescapeField(const std::string & input) {
	std::string out;
	out.reserve(input.size());
	bool esc = false;
	for (char c : input) {
		if (!esc) {
			if (c == '\\') {
				esc = true;
			} else {
				out += c;
			}
		} else {
			switch (c) {
			case '\\':
				out += '\\';
				break;
			case 't':
				out += '\t';
				break;
			case 'n':
				out += '\n';
				break;
			case 'c':
				out += ',';
				break;
			default:
				out += c;
				break;
			}
			esc = false;
		}
	}
	return out;
}

static std::vector<std::string> splitTabs(const std::string & line) {
	std::vector<std::string> out;
	std::string current;
	for (char c : line) {
		if (c == '\t') {
			out.push_back(current);
			current.clear();
		} else {
			current += c;
		}
	}
	out.push_back(current);
	return out;
}

static std::vector<std::string> splitEscapedList(const std::string & input) {
	std::vector<std::string> out;
	std::string current;
	bool esc = false;
	for (char c : input) {
		if (!esc) {
			if (c == '\\') {
				esc = true;
			} else if (c == ',') {
				out.push_back(unescapeField(current));
				current.clear();
			} else {
				current += c;
			}
		} else {
			current += '\\';
			current += c;
			esc = false;
		}
	}
	if (!current.empty() || input.empty()) {
		out.push_back(unescapeField(current));
	}
	return out;
}

//--------------------------------------------------------------
void ofApp::setup() {
	steamManager.setup();

	// Seed visual RNG (local-only randomness for UI/particles)
	std::random_device rd_visual;
	visualRNG.seed(rd_visual());
	ofLogNotice("Setup") << "Visual RNG seeded.";

	ofSetEscapeQuitsApp(false);
	ofSetVerticalSync(true);
	ofSetBackgroundColor(22);
	ofDisableArbTex();
	ofSetCircleResolution(64);

	// --- 1. UI & CONFIG ---

	// Load the UI Font (m6x11 scaled up 2x)
	ofTrueTypeFontSettings uiSettings("UI/m6x11plus.ttf", 22); // Was 11. Now 11 * 2 = 22
	uiSettings.antialiased = false;
	uiFont.load(uiSettings);

	// Load the Title Font (m6x11 scaled up 4x)
	ofTrueTypeFontSettings titleSettings("UI/m6x11plus.ttf", 44); // Was 33. Now 11 * 4 = 44
	titleSettings.antialiased = false;
	titleFont.load(titleSettings);

	cardBackImage.load("UI/card_back.png");
	cardSpriteSheet.load("UI/TTS_Sheet.png");

	// --- Load Player Model ---
	if (playerModel.load("Units/Player/model.glb")) {
		playerModel.disableMaterials();
		playerModel.setScale(0.0025f, 0.0025f, 0.0025f);
		playerModel.setRotation(0, 180, 0, 0, 1);
	}

	// Load Skeleton
	skeletonModel.load("Units/Skeleton/skeleton.fbx");
	ofLoadImage(skeletonTexture, "Units/Skeleton/base.png");
	skeletonModel.setRotation(0, 180, 1, 0, 0);
	skeletonModel.setRotation(1, 180, 0, 1, 0);
	skeletonModel.setScale(0.0021f, 0.0021f, 0.0021f);
	skeletonModel.disableMaterials();

	// Load Golem
	golemModel.load("Units/Golem/lava+golem+3d+model.fbx");
	golemModel.setScale(0.0036f, 0.0036f, 0.0036f);
	golemModel.disableMaterials();
	golemModel.disableTextures();
	// Load Golem Variants
	ofLoadImage(golemTexBase, "Units/Golem/texture_base.png");
	ofLoadImage(golemTexRock, "Units/Golem/texture_rock.png");
	ofLoadImage(golemTexFire, "Units/Golem/texture_fire.png");
	ofLoadImage(golemTexElectric, "Units/Golem/texture_electric.png");

	// Load Wolf (OBJ with manual texture binding)
	if (wolfModel.load("Units/Wolf/wolf.obj")) {
		wolfModel.disableMaterials();
		wolfModel.disableTextures();
		wolfModel.setScaleNormalization(false);

		// Load wolf textures manually
		ofLoadImage(wolfBodyTex, "Units/Wolf/body.png");
		ofLoadImage(wolfFaceTex, "Units/Wolf/face.png");
		ofLoadImage(wolfFurTex, "Units/Wolf/fur.png");

		// Smooth texture filtering
		wolfBodyTex.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
		wolfFaceTex.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
		wolfFurTex.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);

		ofLogNotice() << "Wolf model loaded with " << wolfModel.getMeshCount() << " meshes";
	}

	// --- Load Kobold ---
	// Try lowercase path first (some platforms/filesystems are case-sensitive)
	std::string koboldPath1 = "Units/kobold/kobold1/goblin_bastard.glb";
	std::string koboldPath2 = "Units/Kobold/Kobold1/goblin_bastard.glb";
	if (koboldModel.load(koboldPath1) || koboldModel.load(koboldPath2)) {
		koboldModel.disableMaterials();
		koboldModel.setRotation(0, 180, 0, 0, 1);
		// Scale down by ~30% to make kobold visually smaller
		koboldModel.setScale(0.00245f, 0.00245f, 0.00245f);
		ofLogNotice("Setup") << "Kobold model loaded.";
	} else {
		ofLogNotice("Setup") << "Kobold model failed to load (optional). Tried: " << koboldPath1 << " and " << koboldPath2;
	}

	// --- Load Kobold King ---
	if (koboldKingModel.load("Units/KoboldKing/goblin_king.fbx")) {
		koboldKingModel.disableMaterials();

		// 1. Rotation: Keep the Z flip if it was needed to make it upright
		koboldKingModel.setRotation(0, 180, 0, 0, 1);

		// 2. Scale: Reduced from 0.06 to 0.0042 (Approx 1.5x size of Player)
		// Reduce further by 15% to avoid clipping and better fit tile
		koboldKingModel.setScale(0.003f, 0.003f, 0.003f);

		if (ofLoadImage(koboldKingTexture, "Units/KoboldKing/01391eaa.dds")) {
			koboldKingTexture.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
			ofLogNotice("Setup") << "Kobold King texture loaded.";
		} else {
			ofLogError("Setup") << "Failed to load Units/KoboldKing/01391eaa.dds";
		}

		ofLogNotice("Setup") << "Kobold King model loaded.";
	}

	// --- Load Hellhound ---
	if (hellhoundModel.load("Units/Hellhound/hellhound.glb")) {
		hellhoundModel.disableMaterials();

		// FIX: Rotate -90 around X to lift face off the ground
		hellhoundModel.setRotation(0, 90, 1, 0, 0);

		// Scale: increase model scale by 15% for more presence
		hellhoundModel.setScale(0.005175f, 0.005175f, 0.005175f);

		ofLogNotice("Setup") << "Hellhound model loaded.";
	}

	// --- Load Demon ---
	if (demonModel.load("Units/Demon/demonic_horned_horror_knight.glb")) {
		demonModel.disableMaterials();
		// Standard GLB fix
		demonModel.setRotation(0, 180, 0, 0, 1);
		// Demon should be large
		demonModel.setScale(0.00575f, 0.00575f, 0.00575f);
		ofLogNotice("Setup") << "Demon model loaded.";
	} else {
		ofLogError("Setup") << "Failed to load demon model.";
	}

	// --- Load Tortoise ---
	if (tortoiseModel.load("Units/Tortoise/Turtle_Kaiju_01.fbx")) {
		tortoiseModel.disableMaterials();
		tortoiseModel.disableTextures();
		// Scale - reduced by 15% more (0.003 * 0.85 = 0.00255)
		tortoiseModel.setScale(0.00255f, 0.00255f, 0.00255f);
		// Load texture
		ofLoadImage(tortoiseTexture, "Units/Tortoise/Turtle_01_albedo.jpg");
		tortoiseTexture.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
		ofLogNotice("Setup") << "Tortoise model loaded.";
	} else {
		ofLogError("Setup") << "Failed to load tortoise model.";
	}

	// --- Load Ghost ---
	if (ghostModel.load("Units/Ghost/Halloween Ghost.fbx")) {
		ghostModel.disableMaterials();
		ghostModel.disableTextures(); // We will bind manually
		ghostModel.setScale(0.0025f, 0.0025f, 0.0025f);
		ghostModel.setRotation(0, 180, 0, 0, 1);

		// Load the texture provided in the zip
		ofLoadImage(ghostBaseTex, "Units/Ghost/Ghost_BaseColor.png");

		ofLogNotice("Setup") << "Ghost model loaded.";
	} else {
		ofLogError("Setup") << "Failed to load Ghost model.";
	}

	// --- Load Wall Unit ---
	// Prefer the GLB model (wallman.glb) and apply the pixel-art wall texture from Board/wall.png
	if (wallUnitModel.load("Units/Wall/wallman.glb")) {
		// Disable embedded materials/textures so we can bind our pixel-art texture
		wallUnitModel.disableMaterials();
		wallUnitModel.disableTextures();
		wallUnitModel.setScaleNormalization(false);
		wallUnitModel.setScale(0.035f, 0.035f, 0.035f);
		wallUnitModel.setRotation(0, 180, 0, 0, 1);
		wallUnitModel.setRotation(1, 180, 1, 0, 0);

		// Load pixel-art wall texture from Board and set nearest filtering to avoid blurring
		ofImage tmpImg;
		if (tmpImg.load("Board/wall.png")) {
			wallUnitTexture.loadData(tmpImg.getPixels());
			wallUnitTexture.generateMipmap();
			wallUnitTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
			wallUnitTexture.setTextureWrap(GL_REPEAT, GL_REPEAT);
		} else {
			ofLogError("Setup") << "Failed to load Board/wall.png for wall unit.";
		}

		ofLogNotice("Setup") << "Wall Unit GLB model loaded and texture applied.";
	} else {
		ofLogError("Setup") << "Wall Unit model failed to load.";
	}

	// --- Load Assistant ---
	if (assistantModel.load("Units/Assistant/free_battlemage_wizard.glb")) {
		assistantModel.disableMaterials();
		assistantModel.setRotation(0, 180, 0, 0, 1);
		// Adjust scale as needed, usually GLBs need around 0.0025 to 0.0045
		assistantModel.setScale(0.0035f, 0.0035f, 0.0035f);
		ofLogNotice("Setup") << "Assistant model loaded.";
	} else {
		ofLogError("Setup") << "Failed to load Assistant model.";
	}

	// --- Load Faerie ---
	if (faerieModel.load("Units/Faerie/Highly_detailed_3D_mo_1031064951_texture.glb")) {
		faerieModel.disableMaterials();
		faerieModel.setScale(0.0032f, 0.0032f, 0.0032f); // Adjust as needed for tile fit
		faerieModel.setRotation(0, 180, 0, 0, 1);
		if (ofLoadImage(faerieTexture, "Units/Faerie/gltf_embedded_0.jpeg")) {
			faerieTexture.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
			ofLogNotice("Setup") << "Faerie texture loaded.";
		} else {
			ofLogError("Setup") << "Failed to load Units/Faerie/gltf_embedded_0.jpeg for faerie.";
		}
		ofLogNotice("Setup") << "Faerie model loaded.";
	} else {
		ofLogError("Setup") << "Failed to load Faerie model.";
	}

	// --- 3. BOARD & SKYBOX ---
	ofLoadImage(wallTexture, "Board/wall.png");
	wallTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);

	// Dark variant used when there's a wall to the north within 2 tiles
	ofLoadImage(wallDarkTexture, "Board/wallDark.png");
	wallDarkTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);

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

	// --- Load Floating Key Frames (gold/silver/bronze sets) ---
	keyTextures.clear();
	keyTexturesSilver.clear();
	keyTexturesBronze.clear();
	keyAnimSequence.clear();
	{
		std::vector<std::string> goldFiles = { "Board/keys_1_1.png", "Board/keys_1_2.png", "Board/keys_1_3.png", "Board/keys_1_4.png" };
		for (const auto & f : goldFiles) {
			ofTexture t;
			if (ofLoadImage(t, f)) {
				t.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
				t.setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
				keyTextures.push_back(t);
				ofLogNotice("Setup") << "Loaded key frame: " << f;
			} else {
				ofLogError("Setup") << "Failed to load key frame: " << f;
			}
		}

		std::vector<std::string> silverFiles = { "Board/keys_2_1.png", "Board/keys_2_2.png", "Board/keys_2_3.png", "Board/keys_2_4.png" };
		for (const auto & f : silverFiles) {
			ofTexture t;
			if (ofLoadImage(t, f)) {
				t.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
				t.setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
				keyTexturesSilver.push_back(t);
				ofLogNotice("Setup") << "Loaded silver key frame: " << f;
			}
		}

		std::vector<std::string> bronzeFiles = { "Board/keys_3_1.png", "Board/keys_3_2.png", "Board/keys_3_3.png", "Board/keys_3_4.png" };
		for (const auto & f : bronzeFiles) {
			ofTexture t;
			if (ofLoadImage(t, f)) {
				t.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
				t.setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
				keyTexturesBronze.push_back(t);
				ofLogNotice("Setup") << "Loaded bronze key frame: " << f;
			}
		}

		// Sequence: 1_1, 1_2, 1_4, 1_3, 1_4, 1_2 (use gold frames count as reference)
		if (keyTextures.size() >= 4) {
			keyAnimSequence = { 0, 1, 3, 2, 3, 1 };
		} else {
			for (int i = 0; i < (int)keyTextures.size(); ++i)
				keyAnimSequence.push_back(i);
		}
		keyAnimSeqPos = 0;
		keyAnimTimer = 0.0f;

		// Initialize floating key instances: gold keys (set=1)
		floatingKeyInstances.clear();
		floatingKeyInstances.push_back({ glm::ivec2(4, 4), 1 });
		floatingKeyInstances.push_back({ glm::ivec2(6, 4), 1 });
		floatingKeyInstances.push_back({ glm::ivec2(8, 4), 1 });

		// Silver keys (set=2) per user request
		floatingKeyInstances.push_back({ glm::ivec2(0, 0), 2 });
		floatingKeyInstances.push_back({ glm::ivec2(6, 1), 2 });
		floatingKeyInstances.push_back({ glm::ivec2(12, 4), 2 });
		floatingKeyInstances.push_back({ glm::ivec2(12, 8), 2 });
		floatingKeyInstances.push_back({ glm::ivec2(6, 7), 2 });
		floatingKeyInstances.push_back({ glm::ivec2(0, 4), 2 });

		// Bronze keys (set=3) per user request
		floatingKeyInstances.push_back({ glm::ivec2(4, 0), 3 });
		floatingKeyInstances.push_back({ glm::ivec2(8, 0), 3 });
		floatingKeyInstances.push_back({ glm::ivec2(11, 3), 3 });
		floatingKeyInstances.push_back({ glm::ivec2(11, 5), 3 });
		floatingKeyInstances.push_back({ glm::ivec2(8, 8), 3 });
		floatingKeyInstances.push_back({ glm::ivec2(4, 8), 3 });
		floatingKeyInstances.push_back({ glm::ivec2(1, 3), 3 });
		floatingKeyInstances.push_back({ glm::ivec2(1, 5), 3 });

		// Keep legacy single-key coordinates in sync with first instance (if any)
		if (!floatingKeyInstances.empty()) {
			keyAnimTileX = floatingKeyInstances[0].pos.x;
			keyAnimTileY = floatingKeyInstances[0].pos.y;
		}
	}

	// --- 4. DICE TEXTURES & COIN ---
	// Note: Paths point to specific Dice/ subfolders
	ofLoadImage(d4Texture, "Dice/D4/Dice_d4_Albedo.png");
	ofLoadImage(d6Texture, "Dice/D6/dice_texture_d6.png");
	ofLoadImage(d10Texture, "Dice/D10/d10SilverAlbedo.png");
	ofLoadImage(d20Texture, "Dice/D20/d20_diffuse.png");

	ofLoadImage(coinFacesTexture, "Dice/Coin/CoinUKSilver.png");
	// Use linear filtering and mipmaps for a smooth coin appearance
	coinFacesTexture.generateMipmap();
	coinFacesTexture.setTextureMinMagFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);

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

	// 1. GLOBAL AMBIENT
	// Make the ambient slightly darker so the board isn't too bright
	ofSetGlobalAmbientColor(ofColor(50, 50, 50));

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

	// 6. UI LIGHT (For FBOs)
	uiLight.setup();
	uiLight.setPointLight();
	uiLight.setDiffuseColor(ofColor::white);
	uiLight.setPosition(0, 100, 200); // Positioned in front and above
	// END ADD

	// Adjust FOV based on aspect ratio to maintain consistent scale
	float aspectRatio = (float)ofGetWidth() / (float)ofGetHeight();
	float fov = 60.0f * (aspectRatio / 1.333f); // 1.333 is the original 1024/768 ratio
	cam.setupPerspective(false, fov, 0.1f, 100000);
	cam2.setupPerspective(false, fov, 0.1f, 100000); // Same settings for cam2

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

	// --- GENERATE PIXEL ART FIRE TEXTURE ---
	// Creating a 4-frame sprite sheet (128x32 pixels, 4 frames of 32x32)
	ofPixels firePix;
	firePix.allocate(128, 32, OF_PIXELS_RGBA);

	for (int f = 0; f < 4; f++) { // 4 Frames
		int xOffset = f * 32;
		for (int y = 0; y < 32; y++) {
			for (int x = 0; x < 32; x++) {
				// Procedural noise fire shape
				float n = ofNoise(x * 0.1, y * 0.1, f * 0.5, ofGetElapsedTimef());
				float centerDist = abs(x - 16) / 16.0f;
				float heightFade = (32 - y) / 32.0f;

				float alpha = 0;
				if (n > 0.4 + centerDist && y > 5) {
					alpha = 255;
				}

				// Pixel Art Colors (Yellow -> Orange -> Red)
				ofColor c;
				if (y > 20)
					c = ofColor(255, 50, 0); // Red bottom
				else if (y > 10)
					c = ofColor(255, 150, 0); // Orange mid
				else
					c = ofColor(255, 255, 0); // Yellow top

				firePix.setColor(xOffset + x, y, ofColor(c, alpha));
			}
		}
	}
	fireTexture.setFromPixels(firePix);
	fireTexture.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);

	// --- ALLOCATE FBO FOR MINION UI ---
	ofFbo::Settings fboSettings;
	fboSettings.width = 128; // Small texture size for UI
	fboSettings.height = 128;
	fboSettings.internalformat = GL_RGBA;
	fboSettings.useDepth = true; // We need a depth buffer to render a 3D model
	modelFbo.allocate(fboSettings);

	// --- FINAL APPLY SETTINGS ---
	isFullscreen = true;
	// Fullscreen is now set in main.cpp
	applySettings();

	// Load PNG cursors from UI folder
	glfwArrow = createGLFWCursorFromPNG("UI/pointer.png", 0, 0); // pointer.png, hotspot top-left
	glfwHandPoint = createGLFWCursorFromPNG("UI/link.png", 0, 0); // link.png, hotspot top-left
	glfwHandOpen = createGLFWCursorFromPNG("UI/grab_hover.png", 8, 8); // grab_hover.png, hotspot center
	glfwHandClosed = createGLFWCursorFromPNG("UI/grab.png", 8, 8); // grab.png, hotspot center

	// Set initial cursor
	GLFWwindow * window = (GLFWwindow *)ofGetWindowPtr()->getWindowContext();
	if (glfwArrow) glfwSetCursor(window, glfwArrow);
}
//--------------------------------------------------------------
Player * ofApp::getPlayer(int index) {
	if (index >= 0 && index < static_cast<int>(players.size())) {
		return &players[index];
	}
	return nullptr;
}
//--------------------------------------------------------------
// Build a human-friendly display name for a player/minion
std::string ofApp::getPlayerDisplayName(int index) {
	Player * p = getPlayer(index);
	if (!p) return "";

	// Non-minion players: "Player N" (1-based playerID)
	if (!p->isMinion) {
		return "Player " + ofToString(p->playerID + 1);
	}

	// Minions: try to pick a species prefix
	std::string prefix = "Minion";
	if (p->isFaerie)
		prefix = "Faerie";
	else if (p->isWallUnit)
		prefix = "Wall";
	else if (p->isKobold)
		prefix = "Kobold";
	else if (p->isAssistant)
		prefix = "Assistant";
	else if (p->isWolf)
		prefix = "Wolf";
	else if (p->isHellhound)
		prefix = "Hellhound";
	else if (p->isGolem)
		prefix = "Golem";
	else if (p->isSkeleton)
		prefix = "Skeleton";
	else if (p->isDemon)
		prefix = "Demon";

	// Derive a simple ordinal by counting same-type minions for the same owner
	int ord = 1;
	for (int i = 0; i < (int)players.size(); ++i) {
		if (i == index) break;
		Player & other = players[i];
		if (!other.isMinion) continue;
		if ((prefix == "Faerie" && other.isFaerie) || (prefix == "Kobold" && other.isKobold) || (prefix == "Wolf" && other.isWolf) || (prefix == "Hellhound" && other.isHellhound) || (prefix == "Golem" && other.isGolem) || (prefix == "Skeleton" && other.isSkeleton) || (prefix == "Demon" && other.isDemon) || (prefix == "Wall" && other.isWallUnit)) {
			if (other.ownerID == p->ownerID) ord++;
		}
	}

	return prefix + " " + ofToString(ord);
}
//--------------------------------------------------------------
void ofApp::update() {
	steamManager.update();

	// Cache Steam avatars for turn indicator
	if (isMultiplayer && steamManager.isConnected()) {
		if (!localAvatarReady) {
			localAvatarReady = steamManager.getAvatarImage(steamManager.getLocalSteamID(), localAvatarImage, 64);
		}
		if (!opponentAvatarReady && steamManager.getOpponentSteamID().IsValid()) {
			opponentAvatarReady = steamManager.getAvatarImage(steamManager.getOpponentSteamID(), opponentAvatarImage, 64);
		}
	}
	processNetworkPackets();

	// Check for disconnection/reconnection
	if (isMultiplayer) {
		// Check if opponent left the lobby (host quit to main menu)
		if (!steamManager.hasOpponent()) {
			ofLogNotice("Network") << "Opponent left the lobby. Resetting game and returning to main menu.";

			// Add message to chat
			ChatMessage msg;
			msg.playerName = "[SERVER]";
			msg.message = "Opponent left the game";
			msg.timestamp = ofGetElapsedTimef();
			chatHistory.push_back(msg);
			if (chatHistory.size() > maxChatMessages) {
				chatHistory.erase(chatHistory.begin());
			}

			// Reset all multiplayer state
			isMultiplayer = false;
			hasReceivedHandshake = false;
			waitingForTurnStartFromHost = false;
			initialDraftComplete = false;
			draftAcceptLocked = false;
			draftAcceptApplied = false;
			gameplaySeededByHost = false;
			handshakeRequestInterval = 1.0f;

			// Clean up game state
			cleanupGame();

			// Return to main menu
			currentState = STATE_MAIN_MENU;

			// Show notification
			lastChatInteractionTime = ofGetElapsedTimef();
			return; // Skip rest of update this frame
		}

		if (steamManager.checkAndClearDisconnectFlag()) {
			// Add disconnection message to chat
			ChatMessage msg;
			msg.playerName = "[SERVER]";
			std::string opponentName = steamManager.getOpponentName();
			msg.message = opponentName + " disconnected";
			msg.timestamp = ofGetElapsedTimef();
			chatHistory.push_back(msg);
			if (chatHistory.size() > maxChatMessages) {
				chatHistory.erase(chatHistory.begin());
			}
			// Show chat window for this message
			lastChatInteractionTime = ofGetElapsedTimef();
			ofLogNotice("Network") << opponentName << " disconnected - message added to chat";
		}

		if (steamManager.checkAndClearReconnectFlag()) {
			// Add reconnection message to chat
			ChatMessage msg;
			msg.playerName = "[SERVER]";
			std::string opponentName = steamManager.getOpponentName();
			msg.message = opponentName + " reconnected";
			msg.timestamp = ofGetElapsedTimef();
			chatHistory.push_back(msg);
			if (chatHistory.size() > maxChatMessages) {
				chatHistory.erase(chatHistory.begin());
			}
			// Show chat window for this message
			lastChatInteractionTime = ofGetElapsedTimef();
			ofLogNotice("Network") << opponentName << " reconnected - message added to chat";
			// Host sends a full state snapshot to resync the reconnecting client
			if (steamManager.isHost()) {
				sendSnapshotToClient();
			}
		}
	}

	// ============================================================
	// 1. STEAM CONNECTION TRIGGER (SYNCED)
	// ============================================================
	if (currentState == STATE_MAIN_MENU && steamManager.hasOpponent()) {

		// CASE A: I AM THE HOST
		// Note: Use steamManager.isHost() directly here since isMultiplayer isn't set yet
		if (steamManager.isHost()) {
			if (!isMultiplayer) { // Ensure we only run this once
				ofLogNotice("Network") << "Host: Opponent found. Starting game & sending seed.";
				isMultiplayer = true;
				myLocalPlayerID = 0; // Host is always Player 0

				// Reset sequence tracking for this new game
				lastReceivedSeqByPlayer[0] = 0;
				lastReceivedSeqByPlayer[1] = 0;

				setupGame(); // Generates seed and sends PKT_HANDSHAKE
			}
		}

		// CASE B: I AM THE CLIENT
		else {
			if (!isMultiplayer && !hasReceivedHandshake) {
				// 1. Log status (visual feedback)
				static bool loggedWait = false;
				if (!loggedWait) {
					ofLogNotice("Network") << "Client: Connected. Requesting Host seed...";
					loggedWait = true;
				}

				// 2. "KEEP ASKING" LOOP (Robust Fix)
				// Every 1.0 seconds, send a "REQ_SEED" packet to the Host.
				// This ensures that if the first packet was dropped, we ask again.
				// If the lobby advertises a seed, log it but DO NOT auto-initialize from it.
				// The authoritative seed must come via a PKT_HANDSHAKE from the host
				// to avoid mismatch races when lobby data is stale or the host regenerated
				// a seed during reconnects. We will keep requesting the seed until
				// a handshake packet arrives.
				if (steamManager.isMatchStarted()) {
					uint32_t seed = steamManager.getLobbySeed();
					if (seed != 0) {
						float now = ofGetElapsedTimef();
						if (seed != lastObservedLobbySeed || (now - lastLobbySeedLogTime) > 5.0f) {
							lastObservedLobbySeed = seed;
							lastLobbySeedLogTime = now;
							ofLogNotice("Network") << "Client: Detected lobby seed (observed)=" << seed << " — waiting for host handshake (authoritative).";
						}
					}
				}

				if (ofGetElapsedTimef() - lastHandshakeRequestTime > handshakeRequestInterval) {
					string req = "REQ_SEED";
					steamManager.sendPacket(req.c_str(), req.size());
					lastHandshakeRequestTime = ofGetElapsedTimef();
					handshakeRequestInterval = std::min(handshakeRequestInterval * 1.5f, 5.0f);
					ofLogNotice("Network") << "Sent Seed Request...";
				}
			}
		}
	}

	// --- LOADING LOGIC ---
	if (isLoadingGame) {
		setupGame();
		isLoadingGame = false;
		return;
	}

	// Check for any "waiting" state that should lock player input
	if (isWaitingForTimeVortexDice || isWaitingForMagicBoltRange) {
		updateGame();
		return;
	}

	switch (currentState) {
	case STATE_MAIN_MENU:
		break;
	case STATE_SETTINGS:
		break;

	// --- INITIATIVE ROLL STATE ---
	case STATE_INITIATIVE_ROLL: {
		// Wait for dice to finish spinning
		bool allFinished = true;
		if (activeDiceRolls.size() < 2) allFinished = false; // Waiting for start
		for (auto & d : activeDiceRolls)
			if (!d.isFinishedVisual) allFinished = false;

		if (allFinished) {
			initiativeTimer += ofGetLastFrameTime();
			if (initiativeTimer > 2.0f) {
				// Determine Winner
				int p1Roll = activeDiceRolls[0].result;
				int p2Roll = activeDiceRolls[1].result;

				activeDiceRolls.clear(); // Clear visual dice

				if (p1Roll > p2Roll) {
					// Lock camera before drafting starts
					draftingCameraLockedToClient = (isMultiplayer && myLocalPlayerID == 1);
					draftPlayerIndex = 0; // P1 Wins
					currentState = STATE_DRAFTING;
					draftStage = 0;
					generateDraftOptions(1); // Start Class 1
					ofLogNotice("Initiative") << "Player 1 Wins Initiative";
					if (isHost()) {
						DraftStatePacket sp = {};
						sp.type = PKT_DRAFT_STATE;
						sp.playerID = myLocalPlayerID;
						sp.classTier = 1;
						sp.draftPlayerIdx = draftPlayerIndex;
						sp.picksRemaining = draftPicksRemaining;
						sp.draftStage = draftStage;
						sp.isInGameDraft = isInGameDraft ? 1 : 0;
						sp.currentPlayerIndex = currentPlayerIndex;
						steamManager.sendPacket(&sp, sizeof(sp));
					}
				} else if (p2Roll > p1Roll) {
					draftPlayerIndex = 1; // P2 Wins
					currentState = STATE_DRAFTING;
					// Lock camera before drafting starts
					draftingCameraLockedToClient = (isMultiplayer && myLocalPlayerID == 1);
					draftStage = 0;
					generateDraftOptions(1);
					ofLogNotice("Initiative") << "Player 2 Wins Initiative";
					if (isHost()) {
						DraftStatePacket sp = {};
						sp.type = PKT_DRAFT_STATE;
						sp.playerID = myLocalPlayerID;
						sp.classTier = 1;
						sp.draftPlayerIdx = draftPlayerIndex;
						sp.picksRemaining = draftPicksRemaining;
						sp.draftStage = draftStage;
						sp.isInGameDraft = isInGameDraft ? 1 : 0;
						sp.currentPlayerIndex = currentPlayerIndex;
						steamManager.sendPacket(&sp, sizeof(sp));
					}
				} else {
					// TIE - Reroll
					startDiceRoll(1, 6, PURPOSE_DEBUG, "P1 Reroll");
					startDiceRoll(1, 6, PURPOSE_DEBUG, "P2 Reroll");
					initiativeTimer = 0.0f;
					ofLogNotice("Initiative") << "Tie! Rerolling...";
				}
			}
		}

		// Update dice visuals (Simple rotation)
		for (auto & d : activeDiceRolls) {
			d.currentRotation += diceSpinSpeed * ofGetLastFrameTime();
			if (ofGetElapsedTimef() - d.startTime > 1.0f) d.isFinishedVisual = true;
		}
		break;
	}

	// --- DRAFTING STATE ---
	case STATE_DRAFTING:
		// Logic is primarily handled in mousePressed (card selection)
		// Allow deck/discard hover view during drafting
		if (isHoveringPile && !isShowingPileView) {
			if (ofGetElapsedTimef() - pileHoverStartTime > 0.6f) { // Reduced hover time
				isShowingPileView = true;
				currentPileView = hoveredPileType;
				currentPileViewPlayerIndex = hoveredPilePlayerIndex;
			}
		}
		break;

	case STATE_GAMEPLAY:
		updateGame();
		break;
	case STATE_PAUSED:
		break;
	}

	// --- HARDWARE CURSOR UPDATE ---
	if (currentCursor != previousCursor) {
		GLFWwindow * window = (GLFWwindow *)ofGetWindowPtr()->getWindowContext();
		if (window) {
			switch (currentCursor) {
			case CURSOR_DEFAULT:
				if (glfwArrow) glfwSetCursor(window, glfwArrow);
				break;
			case CURSOR_CLICK:
				if (glfwHandPoint) glfwSetCursor(window, glfwHandPoint);
				break;
			case CURSOR_GRAB:
				if (glfwHandOpen) glfwSetCursor(window, glfwHandOpen);
				break;
			case CURSOR_HOLD:
				if (glfwHandClosed) glfwSetCursor(window, glfwHandClosed);
				break;
			}
		}
		previousCursor = currentCursor;
	}
}
//--------------------------------------------------------------
void ofApp::drawTileGlow(int gridX, int gridY, ofColor color, float thickness) {
	// Draw a glowing outline around the tile at (gridX, gridY)
	ofVec3f worldPos = gridToWorld(gridX, gridY);

	// Draw a quad outline at ground level around the tile edges
	float halfTile = 0.5f;
	float glowHeight = 0.02f; // Slightly above ground to avoid z-fighting

	ofSetColor(color);
	ofSetLineWidth(thickness);

	// Draw outline around tile
	ofPushMatrix();
	ofTranslate(worldPos.x, worldPos.y, glowHeight);

	// Draw four lines forming a square around the tile
	ofDrawLine(-halfTile, -halfTile, 0, halfTile, -halfTile, 0); // Bottom edge
	ofDrawLine(halfTile, -halfTile, 0, halfTile, halfTile, 0); // Right edge
	ofDrawLine(halfTile, halfTile, 0, -halfTile, halfTile, 0); // Top edge
	ofDrawLine(-halfTile, halfTile, 0, -halfTile, -halfTile, 0); // Left edge

	ofPopMatrix();
	ofSetLineWidth(1);
}
//--------------------------------------------------------------
void ofApp::draw() {
	// --- LOADING SCREEN ---
	if (isLoadingGame) {
		ofBackground(0);
		ofSetColor(255);
		// Minimal loading indicator
		uiFont.drawString("Loading...", ofGetWidth() / 2 - 60, ofGetHeight() / 2);
		return;
	}

	ofBackground(22);

	// REMOVED: The dark overlay block for STATE_INITIATIVE_ROLL / DRAFTING

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
		drawGame(); // Draw game underneath
		// If we paused during draft or initiative, draw those underneath the pause menu too
		if (pausedFromState == STATE_INITIATIVE_ROLL) drawInitiativeRoll();
		if (pausedFromState == STATE_DRAFTING) drawDraftScreen();
		drawPauseMenu();
		break;
	case STATE_INITIATIVE_ROLL:
		drawGame(); // Draw 3D world + dice
		drawInitiativeRoll(); // Draw labels on top
		break;
	case STATE_DRAFTING:
		drawGame(); // Draw 3D world background
		drawDraftScreen(); // Draw cards and text on top
		break;
	default:
		drawMainMenu();
		break;
	}
}

//--------------------------------------------------------------
void ofApp::drawMainMenu() {
	ofDisableLighting();
	ofSetColor(ofColor::white);

	// Draw Title
	string title = "Mage Fight";
	ofRectangle titleBox = titleFont.getStringBoundingBox(title, 0, 0);
	float titleX = round(ofGetWidth() / 2.0f - titleBox.getWidth() / 2.0f);
	float titleY = round(ofGetHeight() * 0.25f);
	titleFont.drawString(title, titleX, titleY);

	// --- RECALCULATE BUTTON POSITIONS (Do this here or in windowResized) ---
	float btnWidth = 400;
	float btnHeight = 80;
	float centerX = ofGetWidth() / 2.0f;
	float startY = ofGetHeight() / 2.0f - btnHeight;

	// Standard Buttons
	mainMenuPlayAIButton.set(centerX - btnWidth / 2, startY, btnWidth, btnHeight);

	// Split the Multiplayer slot into two buttons: Host and Invite
	float halfWidth = (btnWidth / 2) - 10;
	mainMenuHostButton.set(centerX - btnWidth / 2, startY + btnHeight + 20, halfWidth, btnHeight);
	mainMenuInviteButton.set(centerX + 10, startY + btnHeight + 20, halfWidth, btnHeight);

	mainMenuSettingsButton.set(centerX - btnWidth / 2, startY + (btnHeight + 20) * 2, btnWidth, btnHeight);
	mainMenuQuitButton.set(centerX - btnWidth / 2, startY + (btnHeight + 20) * 3, btnWidth, btnHeight);

	// --- DRAW BUTTONS ---
	auto drawButton = [&](const ofRectangle & rect, const string & text, bool isHovered) {
		ofSetColor(isHovered ? ofColor::lightGray : ofColor::white);
		ofFill();
		ofDrawRectRounded(rect, 15);

		ofSetColor(ofColor::black);
		ofNoFill();
		ofSetLineWidth(2);
		ofDrawRectRounded(rect, 15);
		ofFill();

		ofSetColor(ofColor::black); // Text color
		ofRectangle textBox = uiFont.getStringBoundingBox(text, 0, 0);
		float textX = round(rect.getCenter().x - textBox.getWidth() / 2.0f);
		float textY = round(rect.getCenter().y + textBox.getHeight() / 2.0f);
		uiFont.drawString(text, textX, textY);
	};

	drawButton(mainMenuPlayAIButton, "Play vs AI", mainMenuHoveredIndex == 0);

	// Logic: If we are already in a lobby, show "Invite", otherwise show "Host"
	if (!steamManager.isConnected()) {
		drawButton(mainMenuHostButton, "Host Steam", mainMenuHoveredIndex == 1);
		// Draw a grayed out invite button
		ofSetColor(100);
		ofDrawRectRounded(mainMenuInviteButton, 15);
	} else {
		// We are connected/hosting
		ofSetColor(ofColor::green); // Highlight that we are online
		ofDrawRectRounded(mainMenuHostButton, 15);
		ofSetColor(ofColor::black);
		uiFont.drawString("Lobby Active", mainMenuHostButton.x + 20, mainMenuHostButton.getCenter().y);

		drawButton(mainMenuInviteButton, "Invite Friend", mainMenuHoveredIndex == 4);
	}

	drawButton(mainMenuSettingsButton, "Settings", mainMenuHoveredIndex == 2);
	drawButton(mainMenuQuitButton, "Quit", mainMenuHoveredIndex == 3);
}
//--------------------------------------------------------------
void ofApp::drawSettingsMenu() {
	ofDisableLighting();
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
	const bool shaderVertOk = worldPostShader.setupShaderFromFile(GL_VERTEX_SHADER, "Shaders/post.vert");
	const bool shaderFragOk = worldPostShader.setupShaderFromFile(GL_FRAGMENT_SHADER, "Shaders/post.frag");
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
	cam2.setAspectRatio((float)w / (float)h);
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
	// --- MULTIPLAYER SYNC ---
	if (isHost()) {
		currentMapSeed = (uint32_t)time(nullptr);

		// FIX: Seed the gameplay RNG specifically
		gameplayRNG.seed(currentMapSeed);
		gameplaySeededByHost = true;

		// Get Steam names (host is player 0)
		player0SteamName = steamManager.getLocalPlayerName();
		player1SteamName = steamManager.getOpponentName();

		ofLogNotice("Setup") << "Host generated seed: " << currentMapSeed << " platform=" << MAGEFIGHT_PLATFORM;

		HandshakePacket pkt = {};
		pkt.type = PKT_HANDSHAKE;
		pkt.playerID = myLocalPlayerID;
		pkt.seq = 0;
		pkt.seed = currentMapSeed;
		ofLogNotice("Setup") << "Host sending handshake: type=" << (int)pkt.type << " playerID=" << pkt.playerID << " seq=" << pkt.seq << " seed=" << pkt.seed << " platform=" << MAGEFIGHT_PLATFORM;
		steamManager.sendPacket(&pkt, sizeof(pkt));

		// Publish seed and start flag to lobby so clients can begin as well
		steamManager.setLobbySeed(currentMapSeed);
		steamManager.setMatchStarted();
	}
	// SINGLE PLAYER:
	else if (!isMultiplayer) {
		std::random_device rd;
		gameplayRNG.seed(rd()); // FIX: Seed gameplay RNG locally
		ofLogNotice("Setup") << "Single Player: Randomly seeded Gameplay RNG.";
	}

	// Initialize common game state for both singleplayer and multiplayer clients
	initializeGameStateCommon();
}
//--------------------------------------------------------------

void ofApp::initializeGameStateCommon() {
	// --- RESET CORE GAME STATE ---
	players.clear();
	activeDiceRolls.clear();
	globalTurnCounter = 0; // Reset turn counter for new game
	draftGenerationCounter = 0; // Reset draft counter for new game
	initialDraftComplete = false;
	for (int x = 0; x < BOARD_WIDTH; ++x) {
		for (int y = 0; y < BOARD_HEIGHT; ++y) {
			board[x][y] = Tile();
		}
	}

	// --- CAMERA RESET ---
	cameraTargetZoom = 37.0f;
	cameraCurrentZoom = 37.0f;
	cameraTargetPan = glm::vec3(0, 0, 0);
	cameraCurrentPan = glm::vec3(0, 0, 0);
	isTopDownView = false;

	// Setup Player 0's camera (south side)
	cam.setPosition(0, cameraCurrentZoom * 1.18f, cameraCurrentZoom * 0.70f);
	cam.lookAt(cameraCurrentPan);

	// Setup Player 1's camera (north side, 180° opposite)
	cam2.setPosition(0, cameraCurrentZoom * 1.18f, -(cameraCurrentZoom * 0.70f));
	cam2.lookAt(glm::vec3(cameraCurrentPan.x, cameraCurrentPan.y, -cameraCurrentPan.z));
	cameraCurrentPos = cam.getPosition();
	cameraCurrentPos2 = cam2.getPosition();
	cameraCurrentLookAt = cameraCurrentPan;
	cameraCurrentLookAt2 = glm::vec3(cameraCurrentPan.x, cameraCurrentPan.y, -cameraCurrentPan.z);

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
	p1.deck.clear();
	players.push_back(p1);

	Player p2;
	p2.x = BOARD_WIDTH - 1;
	p2.y = 0;
	p2.playerID = 1;
	p2.deck.clear();
	players.push_back(p2);

	board[p1.x][p1.y].hasPlayer = true;
	board[p2.x][p2.y].hasPlayer = true;

	// --- START INITIATIVE PHASE ---
	currentState = STATE_INITIATIVE_ROLL;
	isInitiativeRolling = true;
	initiativeTimer = 0.0f;

	startDiceRoll(1, 6, PURPOSE_DEBUG, "");
	startDiceRoll(1, 6, PURPOSE_DEBUG, "");

	ofLogNotice("Game") << "--- INITIATIVE ROLL STARTED ---";
	cam.setAspectRatio((float)ofGetWidth() / (float)ofGetHeight());
	cam2.setAspectRatio((float)ofGetWidth() / (float)ofGetHeight());
}

void ofApp::initGameFromSeed(uint32_t seed) {
	if (isMultiplayer) return; // Already started

	ofLogNotice("Network") << "Initializing multiplayer client game from seed: " << seed;
	gameplayRNG.seed(seed);
	gameplaySeededByHost = true;
	currentMapSeed = seed;
	isMultiplayer = true;
	myLocalPlayerID = 1;

	// Reset sequence tracking for this new game
	lastReceivedSeqByPlayer[0] = 0;
	lastReceivedSeqByPlayer[1] = 0;

	// Initialize the same common state as host
	initializeGameStateCommon();
}
//--------------------------------------------------------------
void ofApp::updateGame() {

	// --- REBUILD MINION UI EVERY FRAME ---
	activeMinionUIs.clear();
	// --- LUCK AURA RECALCULATION ---
	// Keep temp luck accurate every frame
	recalcTempLuck();

	// --- Update floating key animation (advance by real time, tied to game update loop) ---
	if (!keyAnimSequence.empty() && !keyTextures.empty()) {
		// Use base interval scaled by the selected preset multiplier
		float effectiveInterval = keyAnimInterval * keyAnimSpeedPresets[std::clamp(keyAnimSpeedIndex, 0, (int)keyAnimSpeedPresets.size() - 1)];
		keyAnimTimer += ofGetLastFrameTime();
		if (keyAnimTimer >= effectiveInterval) {
			keyAnimTimer -= effectiveInterval;
			keyAnimSeqPos = (keyAnimSeqPos + 1) % (int)keyAnimSequence.size();
		}
	}

	if (!players.empty()) {
		float scale = ofGetHeight() / 1080.0f;
		float panelWidth = 260 * scale;

		// Standard (Max) size for a minion entry
		float standardEntryHeight = 95 * scale;
		float gap = 10 * scale;

		// 1. DEFINE VERTICAL BOUNDARIES FOR EACH PLAYER
		// Player 0 (left side): Below P1's HP bar (top left), above P0's AP counter (middle left)
		// P0 AP center is at: ofGetHeight() - cardHeight - 20 - cardHeight - 20 - 60 = ofGetHeight() - ~546 * scale
		// Luck text is above that, so bottom limit should be around ofGetHeight() - 600 * scale
		float p0_topLimitY = 140 * scale; // Below P1's HP bar (top left) - reduced gap
		float p0_bottomLimitY = ofGetHeight() - (600 * scale); // Above P0's AP counter and luck text - raised up

		// Player 1 (right side): Below P1's AP counter (and luck text), above P0's HP bar
		// P1 AP center is at: 20 + cardHeight + 20 + cardHeight + 60 = ~546 * scale
		// Plus half AP box height (~40) + luck text = ~620 * scale minimum
		float p1_topLimitY = 580 * scale; // Below P1's AP counter and luck text
		float p1_bottomLimitY = ofGetHeight() - (140 * scale); // Above P0's HP bar (slight gap reduction)

		// 2. SEPARATE MINIONS BY OWNER (Accounting for perspective in multiplayer)
		std::vector<int> p0_minionIndices;
		std::vector<int> p1_minionIndices;
		// Counters for minion types
		int p0_skeleton = 0, p0_golem = 0, p0_wolf = 0, p0_hound = 0, p0_demon = 0, p0_kobold = 0, p0_wall = 0;
		int p1_skeleton = 0, p1_golem = 0, p1_wolf = 0, p1_hound = 0, p1_demon = 0, p1_kobold = 0, p1_wall = 0;

		for (int i = 0; i < (int)players.size(); i++) {
			if (players[i].isMinion) {
				// Each player sees their own minions on the LEFT (p0) and opponent minions on the RIGHT (p1)
				// This works for both host (player 0) and client (player 1)
				int ownerID = players[i].ownerID;
				bool isLocalPlayerMinion = (ownerID == myLocalPlayerID);

				if (isLocalPlayerMinion) {
					p0_minionIndices.push_back(i); // My minions on LEFT
				} else {
					p1_minionIndices.push_back(i); // Opponent minions on RIGHT
				}
			}
		}

		// 3. HELPER LAMBDA TO BUILD UI LIST (now takes top/bottom limits)
		auto buildMinionList = [&](const std::vector<int> & indices, float startX, float topLimit, float bottomLimit, int & skelCount, int & golemCount, int & wolfCount, int & houndCount, int & demonCount, int & koboldCount, int & assistantCount, int & wallCount, int & faerieCount) {
			// A. Calculate Dynamic Scaling
			float localAvailableHeight = bottomLimit - topLimit;
			float totalRequiredHeight = indices.size() * (standardEntryHeight + gap);
			float actualEntryHeight = standardEntryHeight;
			float actualGap = gap;

			if (totalRequiredHeight > localAvailableHeight && !indices.empty()) {
				float shrinkFactor = localAvailableHeight / totalRequiredHeight;
				actualEntryHeight = standardEntryHeight * shrinkFactor;
				actualGap = gap * shrinkFactor;
			}

			// B. Create UIs
			for (int i = 0; i < indices.size(); ++i) {
				int pIndex = indices[i];
				MinionUI ui;
				ui.playerIndex = pIndex;
				ui.displayNumber = 0;

				if (players[pIndex].isSkeleton)
					ui.displayNumber = ++skelCount;
				else if (players[pIndex].isGolem)
					ui.displayNumber = ++golemCount;
				else if (players[pIndex].isWolf)
					ui.displayNumber = ++wolfCount;
				else if (players[pIndex].isHellhound)
					ui.displayNumber = ++houndCount;
				else if (players[pIndex].isDemon)
					ui.displayNumber = ++demonCount;
				else if (players[pIndex].isAssistant)
					ui.displayNumber = ++assistantCount;
				else if (players[pIndex].isKoboldKing)
					ui.displayNumber = ++koboldCount;
				else if (players[pIndex].isKobold)
					ui.displayNumber = ++koboldCount;
				else if (players[pIndex].isWallUnit)
					ui.displayNumber = ++wallCount;
				else if (players[pIndex].isFaerie)
					ui.displayNumber = ++faerieCount;

				float currentY = topLimit + (i * (actualEntryHeight + actualGap));

				ui.bounds.set(startX, currentY, panelWidth, actualEntryHeight);
				activeMinionUIs.push_back(ui);
			}
		};

		// 4. BUILD LISTS WITH PLAYER-SPECIFIC BOUNDARIES
		float p0_startX = 10 * scale;
		int p0_assistant = 0;
		int p0_faerie = 0;
		buildMinionList(p0_minionIndices, p0_startX, p0_topLimitY, p0_bottomLimitY, p0_skeleton, p0_golem, p0_wolf, p0_hound, p0_demon, p0_kobold, p0_assistant, p0_wall, p0_faerie);

		float p1_startX = ofGetWidth() - panelWidth - (10 * scale);
		int p1_assistant = 0;
		int p1_faerie = 0;
		buildMinionList(p1_minionIndices, p1_startX, p1_topLimitY, p1_bottomLimitY, p1_skeleton, p1_golem, p1_wolf, p1_hound, p1_demon, p1_kobold, p1_assistant, p1_wall, p1_faerie);
	}
	// --- END MINION UI REBUILD ---

	// 1. UPDATE UI POSITIONS
	updateDebugRects();

	// 2. Magic Blast / Dispel Freeze Check
	if (isMagicBlastChoiceActive || isDispelMenuOpen || isDispelTargeting || isDispelStatusSelectOpen || isBurstMenuOpen) {
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

	// --- DELTA TIME CLAMP FIX ---
	float deltaTime = ofGetLastFrameTime();
	// If we lagged more than 100ms (e.g. Alt-Tab), pretend it was just 16ms
	if (deltaTime > 0.1f) deltaTime = 0.016f;

	// --- Camera & Skybox Logic ---
	if (ofGetWidth() != lastWindowWidth || ofGetHeight() != lastWindowHeight) {
		cam.setAspectRatio((float)ofGetWidth() / (float)ofGetHeight());
		cam2.setAspectRatio((float)ofGetWidth() / (float)ofGetHeight());
		lastWindowWidth = ofGetWidth();
		lastWindowHeight = ofGetHeight();
	}

	float frame_independent_smoothing = 1.0 - pow(0.6, deltaTime * 60.0);
	cameraCurrentZoom = ofLerp(cameraCurrentZoom, cameraTargetZoom, frame_independent_smoothing);
	cameraCurrentPan = glm::mix(cameraCurrentPan, cameraTargetPan, frame_independent_smoothing);

	// Camera 2 mirrors Camera 1: same X and Y pan, but opposite Z pan
	glm::vec3 cameraCurrentPan2 = glm::vec3(cameraCurrentPan.x, cameraCurrentPan.y, -cameraCurrentPan.z);

	glm::vec3 targetPos;
	glm::vec3 targetPos2; // Second camera position (opposite side)
	glm::vec3 targetLookAt = cameraCurrentPan;
	glm::vec3 targetLookAt2 = cameraCurrentPan2; // Camera 2 looks at mirrored point

	if (isTopDownView) {
		// Top-down should be more zoomed-in: lower the camera height multiplier.
		targetPos = glm::vec3(cameraCurrentPan.x, cameraCurrentZoom * 0.6f, cameraCurrentPan.z);
		targetPos2 = glm::vec3(cameraCurrentPan2.x, cameraCurrentZoom * 0.6f, cameraCurrentPan2.z);
	} else {
		// Use updated multipliers at runtime target: raise Y a bit to look more
		// top-down while keeping the same Z back offset.
		targetPos = glm::vec3(cameraCurrentPan.x, cameraCurrentZoom * 1.18f, cameraCurrentPan.z + cameraCurrentZoom * 0.70f);
		targetPos2 = glm::vec3(cameraCurrentPan2.x, cameraCurrentZoom * 1.18f, cameraCurrentPan2.z - cameraCurrentZoom * 0.70f); // Opposite Z
	}
	cameraCurrentPos = glm::mix(cameraCurrentPos, targetPos, frame_independent_smoothing);
	cameraCurrentPos2 = glm::mix(cameraCurrentPos2, targetPos2, frame_independent_smoothing);
	cameraCurrentLookAt = glm::mix(cameraCurrentLookAt, targetLookAt, frame_independent_smoothing);
	cameraCurrentLookAt2 = glm::mix(cameraCurrentLookAt2, targetLookAt2, frame_independent_smoothing);

	// Update both cameras
	cam.setPosition(cameraCurrentPos);
	cam.lookAt(cameraCurrentLookAt);
	cam2.setPosition(cameraCurrentPos2);
	cam2.lookAt(cameraCurrentLookAt2);

	// --- TORCH FLICKER LOGIC (SLOWER) ---
	float time = ofGetElapsedTimef();
	// Lower the noise frequency so flicker is slower and less frantic
	float flicker = ofNoise(time * 0.6f); // Slower speed

	// 2. Intensity Mapping
	// Map noise to a safe range (0.8 to 1.3)
	float intensity = ofMap(flicker, 0, 1, 0.8f, 1.3f);

	// 3. Position Wiggle (Slow sway)
	float wiggleX = ofNoise(time * 0.4f, 0) * 15.0f - 7.5f;
	float wiggleY = ofNoise(time * 0.4f, 100) * 10.0f - 5.0f;

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

	// Show end turn button / turn indicator
	// In multiplayer: always show (either button or indicator)
	// In singleplayer: always show button
	bool myTurn = isMyTurn();

	// In multiplayer, always keep it visible (shows either button or turn indicator)
	// In singleplayer, always show button
	if (isMultiplayer || myTurn) {
		endTurnButtonTargetPos.set(ofGetWidth() / 2.0f - btnWidth / 2.0f, visibleY);
	} else {
		endTurnButtonTargetPos.set(ofGetWidth() / 2.0f - btnWidth / 2.0f, hiddenY);
	}
	endTurnButtonCurrentPos = endTurnButtonCurrentPos.getInterpolated(endTurnButtonTargetPos, 0.2f);

	// --- Delayed Attack Logic (Rock Crush, Stab with Dice, etc.) ---
	if (isWaitingForAttackDice && activeDiceRolls.empty()) {
		isWaitingForAttackDice = false;
		int baseDamage = pendingAttackRollResult;
		Player & attacker = players[currentPlayerIndex];

		// Preserve the card name for special-resolution effects (e.g., Shoot Arrow)
		string resolvedAttackCardName = pendingAttackCardName;

		// Double damage for Bash if Flurry of Fists is active
		if (resolvedAttackCardName == "Bash" && attacker.flurryOfFistsActive) {
			baseDamage *= 2;
		}
		pendingAttackCardName = ""; // Clear

		string typeLabel = "";
		switch (pendingAttackDamageType) {
		case DAMAGE_PHYSICAL:
			typeLabel = " Physical";
			break;
		case DAMAGE_PIERCING:
			typeLabel = " Piercing";
			break;
		case DAMAGE_MAGIC:
			typeLabel = " Magic";
			break;
		case DAMAGE_ELECTRIC:
			typeLabel = " Electric";
			break;
		case DAMAGE_FIRE:
			typeLabel = " Fire";
			break;
		case DAMAGE_HOLY:
			typeLabel = " Holy";
			break;
		case DAMAGE_POISON:
			typeLabel = " Poison";
			break;
		}

		bool applyPoisonBuff = attacker.nextAttackAddPoison && (pendingAttackDamageType == DAMAGE_PHYSICAL || pendingAttackDamageType == DAMAGE_PIERCING);

		if (applyPoisonBuff) {
			attacker.nextAttackAddPoison = false;
			pendingPoisonTargetIndices.clear();
		}

		for (size_t i = 0; i < pendingAttackTargetIndices.size(); i++) {
			int pIndex = pendingAttackTargetIndices[i];
			Player * target = getPlayer(pIndex);
			if (target) {
				int appliedDamage = baseDamage;

				if (pendingAttackDamageType == DAMAGE_PIERCING && i > 0) appliedDamage /= 2;

				// --- GHOST FORM CHECK ---
				if (target->inGhostForm) {
					if (pendingAttackDamageType == DAMAGE_PHYSICAL || pendingAttackDamageType == DAMAGE_PIERCING) {
						appliedDamage = 0;
						spawnFloatingText(gridToWorld(target->x, target->y), "Phased!", ofColor::cyan);
					}
					if (pendingAttackDamageType == DAMAGE_HOLY) {
						appliedDamage *= 2;
						spawnFloatingText(gridToWorld(target->x, target->y), "Ghost: x2 Holy", ofColor::orange);
					}
				}
				// --- VULNERABILITIES ---
				if ((target->isHellhound || target->isDemon || target->isSkeleton) && pendingAttackDamageType == DAMAGE_HOLY) {
					appliedDamage *= 2;
					spawnFloatingText(gridToWorld(target->x, target->y), "Vulnerable: Holy (x2)", ofColor::orange);
				}
				if (pendingAttackDamageType == DAMAGE_PIERCING) {
					bool hasWolfCall = false;
					for (const auto & c : target->deck)
						if (c.type == CARD_CALL_FOR_WOLVES) {
							hasWolfCall = true;
							break;
						}
					if (!hasWolfCall)
						for (const auto & c : target->discardPile)
							if (c.type == CARD_CALL_FOR_WOLVES) {
								hasWolfCall = true;
								break;
							}
					if (hasWolfCall) {
						appliedDamage *= 2;
						spawnFloatingText(gridToWorld(target->x, target->y), "Vulnerable: Piercing (x2)", ofColor::orange);
					}
				}
				// ------------------------

				// --- MITIGATION (Specific Order) ---

				// 1. Holy Block
				if (pendingAttackDamageType == DAMAGE_HOLY) {
					int absorb = std::min(target->holyBlock, appliedDamage);
					target->holyBlock -= absorb;
					appliedDamage -= absorb;
				}

				// 2. Block
				if (pendingAttackDamageType == DAMAGE_PHYSICAL) {
					int absorb = std::min(target->block, appliedDamage);
					target->block -= absorb;
					appliedDamage -= absorb;
				}

				// 3. Fortification
				if (pendingAttackDamageType == DAMAGE_PHYSICAL || pendingAttackDamageType == DAMAGE_PIERCING) {
					int absorb = std::min(target->fortification, appliedDamage);
					target->fortification -= absorb;
					appliedDamage -= absorb;
				}

				// 4. Barrier
				if (pendingAttackDamageType != DAMAGE_PHYSICAL) {
					int absorb = std::min(target->barrier, appliedDamage);
					target->barrier -= absorb;
					appliedDamage -= absorb;
				}

				// 5. Ward
				if (appliedDamage > 0) {
					int absorb = std::min(target->ward, appliedDamage);
					target->ward -= absorb;
					appliedDamage -= absorb;
				}

				// Apply & Text
				glm::vec3 tPos = gridToWorld(target->x, target->y);
				if (appliedDamage > 0) {
					target->health -= appliedDamage;
					spawnFloatingText(tPos, "-" + ofToString(appliedDamage) + typeLabel, ofColor::red);

					// Ghost Break Logic
					if (target->inGhostForm) {
						target->ghostDamageTaken += appliedDamage;
						if (target->ghostDamageTaken >= 4) {
							target->inGhostForm = false;
							target->ghostDamageTaken = 0;
							target->discardPile.push_back(target->ghostFormCard);
							spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Ghost Form Broken!", ofColor::white);

							if (board[target->x][target->y].hasWall) {
								target->health = 0;
								spawnFloatingText(tPos + glm::vec3(0, 1.0f, 0), "Materialized in Wall!", ofColor::red);
							}
						}
					}

					// Tortoise Break Logic (was missing in this block before)
					if (target->inTortoiseForm) {
						target->tortoiseDamageTaken += appliedDamage;
						if (target->tortoiseDamageTaken >= 5) {
							target->inTortoiseForm = false;
							target->tortoiseDamageTaken = 0;
							target->discardPile.push_back(target->tortoiseFormCard);
							spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Form Ended!", ofColor::darkGreen);
						}
					}

					// --- SPECIAL RESOLUTION FOR SHOOT ARROW ---
					if (resolvedAttackCardName == "Shoot Arrow") {
						Player & attackerRef = players[currentPlayerIndex];
						if (!attackerRef.deck.empty()) {
							Card revealed = attackerRef.deck.back();
							attackerRef.deck.pop_back();
							attackerRef.discardPile.push_back(revealed);

							// 1. Reveal Animation: Fly from Deck to Screen Center
							StolenCardAnimation newAnim;
							newAnim.card = revealed;
							newAnim.startTime = ofGetElapsedTimef();
							newAnim.startPos = gridToWorld(attackerRef.x, attackerRef.y);
							newAnim.targetPos = { ofGetWidth() / 2.0f, ofGetHeight() / 2.0f };
							newAnim.currentPos = cam.worldToScreen(newAnim.startPos);
							activeStolenCardAnimations.push_back(newAnim);

							// 2. Destroy Animation: Shrink/Fade at Screen Center (Starts sooner)
							RemovedCardAnimation rem;
							rem.card = revealed;
							rem.startPos = newAnim.targetPos; // Center screen
							rem.startTime = ofGetElapsedTimef() + 0.5f; // Start after fly arrival (shorter delay)
							rem.currentScale = 3.0f; // Start big (revealed size)
							rem.currentAlpha = 255;
							activeRemovedCardAnimations.push_back(rem);

							// 3. Conditional Effects (only host rolls in multiplayer)
							if (!isClient()) {
								std::uniform_int_distribution<int> d6(1, 6);
								int extra = d6(gameplayRNG);

								if (revealed.type == CARD_SHOCK) {
									applyDamageTo(*target, extra, DAMAGE_ELECTRIC, currentPlayerIndex);
									target->isParalyzed = true;
									target->paralysisHeadsCount = 0;
									spawnFloatingText(tPos + glm::vec3(0, 0.6f, 0), "-" + ofToString(extra) + " Electric", ofColor::orange);
									spawnFloatingText(tPos + glm::vec3(0, 1.0f, 0), "PARALYZED!", ofColor::yellow);
								} else if (revealed.type == CARD_FLAME_HIT) {
									applyDamageTo(*target, extra, DAMAGE_FIRE, currentPlayerIndex);
									target->onFire = true;
									spawnFloatingText(tPos + glm::vec3(0, 0.6f, 0), "-" + ofToString(extra) + " Fire", ofColor::red);
									spawnFloatingText(tPos + glm::vec3(0, 1.0f, 0), "ON FIRE!", ofColor::orange);
								} else if (revealed.type == CARD_ADD_POISON) {
									applyDamageTo(*target, extra, DAMAGE_POISON, currentPlayerIndex);
									target->isPoisoned = true;
									target->poisonReduction = 0;
									spawnFloatingText(tPos + glm::vec3(0, 0.6f, 0), "-" + ofToString(extra) + " Poison", ofColor::green);
									spawnFloatingText(tPos + glm::vec3(0, 1.0f, 0), "POISONED!", ofColor::green);
								}
							}
						} else {
							spawnFloatingText(gridToWorld(attackerRef.x, attackerRef.y), "Deck Empty", ofColor::gray);
							ofLogNotice("ShootArrow") << "Attacker had no cards to reveal.";
						}
					}

				} else {
					// Only show "Blocked" if they weren't phased
					if (!target->inGhostForm || appliedDamage > 0) {
						spawnFloatingText(tPos, "Blocked", ofColor::gray);
					}
				}

				// Apply poison if damage was dealt (or if logic allows poison on block, strictly appliedDamage > 0 is safer)
				if (applyPoisonBuff && !target->inGhostForm) {
					pendingPoisonTargetIndices.push_back(pIndex);
					target->isPoisoned = true;
					target->poisonReduction = 0;
					spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Poisoned!", ofColor::green);
				}
			}
		}
		pendingAttackTargetIndices.clear();

		if (applyPoisonBuff && !pendingPoisonTargetIndices.empty()) {
			// Poison applied from player's attack: use attacker's luck
			pendingPoisonAttackRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Poison Damage", currentPlayerIndex);
			isWaitingForPoisonAttackDice = true;
		}
	}

	// --- Poison Attack Damage Resolution ---
	if (isWaitingForPoisonAttackDice && activeDiceRolls.empty()) {
		isWaitingForPoisonAttackDice = false;
		int poisonDamage = pendingPoisonAttackRollResult;

		for (int pIndex : pendingPoisonTargetIndices) {
			Player * target = getPlayer(pIndex);
			if (target) {
				target->health -= poisonDamage;
				glm::vec3 tPos = gridToWorld(target->x, target->y);
				spawnFloatingText(tPos, "-" + ofToString(poisonDamage) + " Poison", ofColor::green);
				ofLogNotice("Poison") << "Dealt " << poisonDamage << " poison damage to Player " << target->playerID;

				// Add Form break checks here too for completeness?
				// Yes, poison breaks forms.
				if (target->inTortoiseForm) {
					target->tortoiseDamageTaken += poisonDamage;
					if (target->tortoiseDamageTaken >= 5) {
						target->inTortoiseForm = false;
						target->tortoiseDamageTaken = 0;
						target->discardPile.push_back(target->tortoiseFormCard);
						spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Form Ended!", ofColor::darkGreen);
					}
				}
				if (target->inGhostForm) {
					target->ghostDamageTaken += poisonDamage;
					if (target->ghostDamageTaken >= 4) {
						target->inGhostForm = false;
						target->ghostDamageTaken = 0;
						target->discardPile.push_back(target->ghostFormCard);
						spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Ghost Form Broken!", ofColor::white);
						if (board[target->x][target->y].hasWall) target->health = 0;
					}
				}
			}
		}
		pendingPoisonTargetIndices.clear();

		// --- Enhanced Death Cleanup: Faerie Resurrection Logic ---
		std::vector<int> removePoisoned;
		for (int i = 0; i < (int)players.size(); ++i) {
			if (players[i].health <= 0) removePoisoned.push_back(i);
		}
		if (!removePoisoned.empty()) {
			// For each dying unit, check if adjacent to ANY faerie (even if the faerie is dying)
			std::vector<int> finalRemove;
			for (int idx : removePoisoned) {
				bool resurrected = false;
				int x = players[idx].x;
				int y = players[idx].y;
				// Only non-faerie units can be resurrected by faerie
				// Only non-faerie units can be resurrected by faerie (no graveyard loot)
				// Graveyard loot is only for Raise Dead, not faerie resurrection
				if (!players[idx].isFaerie) {
					for (int dx = -1; dx <= 1 && !resurrected; ++dx) {
						for (int dy = -1; dy <= 1 && !resurrected; ++dy) {
							if (dx == 0 && dy == 0) continue;
							int nx = x + dx;
							int ny = y + dy;
							if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= BOARD_HEIGHT) continue;
							for (int j = 0; j < (int)players.size(); ++j) {
								if (players[j].isFaerie && players[j].x == nx && players[j].y == ny) {
									// In multiplayer, only host rolls for resurrection
									if (isClient()) {
										// Client should wait for host to sync this state
										continue;
									}
									int d4 = 1 + (gameplayRNG() % 4); // 1d4 roll
									float healF = players[idx].maxHealth * 0.25f * d4;
									int heal = (int)healF;
									if (heal > players[idx].maxHealth) heal = players[idx].maxHealth;
									if (heal >= 1) {
										players[idx].health = heal;
										spawnFloatingText(gridToWorld(players[idx].x, players[idx].y), "Faerie Resurrection! +" + ofToString(heal) + " HP", ofColor::aqua);
										resurrected = true;
									}
								}
							}
						}
					}
				}
				if (!resurrected) {
					finalRemove.push_back(idx);
				}
			}

			std::sort(finalRemove.begin(), finalRemove.end(), std::greater<int>());
			for (int idx : finalRemove) {
				if (idx < 0 || idx >= (int)players.size()) continue;

				DeathMarker death;
				death.x = players[idx].x;
				death.y = players[idx].y;
				death.turnDied = globalTurnCounter;
				death.deck = players[idx].deck;
				graveyard.push_back(death);

				if (players[idx].x >= 0 && players[idx].x < BOARD_WIDTH && players[idx].y >= 0 && players[idx].y < BOARD_HEIGHT) {
					board[players[idx].x][players[idx].y].hasPlayer = false;
				}

				// Update active dice associations
				for (auto & r : activeDiceRolls) {
					if (r.associatedUnit == idx)
						r.associatedUnit = -1;
					else if (r.associatedUnit > idx)
						r.associatedUnit -= 1;
				}

				// Remove earthquake unit entries referencing this index
				for (auto it = earthquakeUnits.begin(); it != earthquakeUnits.end();) {
					if (it->playerIndex == idx)
						it = earthquakeUnits.erase(it);
					else {
						if (it->playerIndex > idx) it->playerIndex -= 1;
						++it;
					}
				}

				players.erase(players.begin() + idx);

				if (players.empty()) {
					currentPlayerIndex = -1;
				} else {
					if (currentPlayerIndex == idx) {
						currentPlayerIndex = std::min<int>(idx, (int)players.size() - 1);
					} else if (currentPlayerIndex > idx) {
						currentPlayerIndex -= 1;
					}
				}
			}
			invalidateTargetCache();
		}
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

	// --- MAGIC HAND DAMAGE & DISPLACEMENT ---
	if (isWaitingForMagicHandDamage && activeDiceRolls.empty()) {
		isWaitingForMagicHandDamage = false;

		// 1. Execute Move of Caster and Wall (Visuals)
		Player & caster = players[currentPlayerIndex];
		glm::ivec2 wallOldPos = magicHandTargetTile;
		glm::ivec2 wallNewPos = magicHandTargetTile + magicHandPushDir;

		// Move Wall
		bool wasMagic = board[wallOldPos.x][wallOldPos.y].isMagicWall;
		board[wallOldPos.x][wallOldPos.y].hasWall = false;
		board[wallNewPos.x][wallNewPos.y].hasWall = true;
		board[wallNewPos.x][wallNewPos.y].isMagicWall = wasMagic;

		// Move Caster
		board[caster.x][caster.y].hasPlayer = false;
		caster.x = wallOldPos.x;
		caster.y = wallOldPos.y;
		board[caster.x][caster.y].hasPlayer = true;
		playerVisualPos = gridToWorld(caster.x, caster.y);

		buildLevelMesh();

		// 2. Handle Pushed Unit
		Player * victim = getPlayer(magicHandPushedUnitIndex);
		if (victim) {
			int dmg = pendingMagicHandRollResult;

			// --- GHOST IMMUNITY ---
			if (victim->inGhostForm) {
				dmg = 0;
				spawnFloatingText(gridToWorld(victim->x, victim->y), "Phased!", ofColor::cyan);
			}
			// ----------------------

			// Apply Physical Mitigation (Block, Fortification, Ward)
			int block = std::min(victim->block, dmg);
			victim->block -= block;
			dmg -= block;
			int fort = std::min(victim->fortification, dmg);
			victim->fortification -= fort;
			dmg -= fort;
			int ward = std::min(victim->ward, dmg);
			victim->ward -= ward;
			dmg -= ward;

			if (dmg > 0) {
				victim->health -= dmg;
				spawnFloatingText(gridToWorld(victim->x, victim->y), "-" + ofToString(dmg) + " Phys", ofColor::red);

				// Form break logic
				if (victim->inGhostForm) {
					victim->ghostDamageTaken += dmg;
					if (victim->ghostDamageTaken >= 4) {
						victim->inGhostForm = false;
						victim->ghostDamageTaken = 0;
						victim->discardPile.push_back(victim->ghostFormCard);
						spawnFloatingText(gridToWorld(victim->x, victim->y), "Form Broken!", ofColor::white);
					}
				}
				if (victim->inTortoiseForm) {
					victim->tortoiseDamageTaken += dmg;
					if (victim->tortoiseDamageTaken >= 5) {
						victim->inTortoiseForm = false;
						victim->tortoiseDamageTaken = 0;
						victim->discardPile.push_back(victim->tortoiseFormCard);
						spawnFloatingText(gridToWorld(victim->x, victim->y), "Form Broken!", ofColor::darkGreen);
					}
				}
			}

			// --- DISPLACEMENT LOGIC ---
			// Try pushing back: WallNewPos + Dir
			glm::ivec2 pushDest1 = wallNewPos + magicHandPushDir;
			glm::ivec2 side1, side2;

			// Calc sides relative to push dir
			if (magicHandPushDir.x != 0) { // Moving Horiz, sides are Vert
				side1 = wallNewPos + glm::ivec2(0, 1);
				side2 = wallNewPos + glm::ivec2(0, -1);
			} else { // Moving Vert, sides are Horiz
				side1 = wallNewPos + glm::ivec2(1, 0);
				side2 = wallNewPos + glm::ivec2(-1, 0);
			}

			auto isValid = [&](glm::ivec2 p) {
				if (p.x < 0 || p.x >= BOARD_WIDTH || p.y < 0 || p.y >= BOARD_HEIGHT) return false;
				if (board[p.x][p.y].hasWall || board[p.x][p.y].hasPlayer) return false;
				return true;
			};

			glm::ivec2 finalDest = { -1, -1 };

			if (isValid(pushDest1))
				finalDest = pushDest1;
			else if (isValid(side1))
				finalDest = side1;
			else if (isValid(side2))
				finalDest = side2;

			if (finalDest.x != -1) {
				// Move Victim
				board[victim->x][victim->y].hasPlayer = false;
				victim->x = finalDest.x;
				victim->y = finalDest.y;
				board[victim->x][victim->y].hasPlayer = true;
				spawnFloatingText(gridToWorld(victim->x, victim->y), "Pushed!", ofColor::yellow);
			} else {
				// SQUISH
				spawnFloatingText(gridToWorld(victim->x, victim->y), "CRUSHED!", ofColor::darkRed);
				victim->health = 0;
			}

			// Death Check
			if (victim->health <= 0) {
				// Standard death logic (create grave, remove from vector, fix indices)
				// (Copy existing death logic here or extract to function)
				DeathMarker death;
				death.x = victim->x;
				death.y = victim->y;
				death.turnDied = globalTurnCounter;
				death.deck = victim->deck;
				graveyard.push_back(death);
				board[victim->x][victim->y].hasPlayer = false;

				// Erase Logic... (See previous earthquake death logic for reference)
				players.erase(players.begin() + magicHandPushedUnitIndex);
				if (currentPlayerIndex == magicHandPushedUnitIndex)
					currentPlayerIndex = std::min<int>(magicHandPushedUnitIndex, (int)players.size() - 1);
				else if (currentPlayerIndex > magicHandPushedUnitIndex)
					currentPlayerIndex--;
				// Note: if multiple units die or indices shift, this gets complex.
				// For simplified single target push, this suffices.
			}
		}

		invalidateTargetCache();
	}

	// --- KOBOLD KING DYNAMIC HP LOGIC ---
	// 1. Count current Kobolds
	int globalKoboldCount = 0;
	for (const auto & p : players) {
		if (p.isKobold && p.health > 0) globalKoboldCount++;
	}

	// 2. Update Kings
	for (auto & p : players) {
		if (p.isKoboldKing) {
			int newMax = globalKoboldCount + 1;

			// Only update if changed
			if (p.maxHealth != newMax) {
				p.maxHealth = newMax;
				// If health is now higher than max, clamp it down.
				// Do NOT heal up if max increases.
				if (p.health > p.maxHealth) {
					p.health = p.maxHealth;
				}
			}
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
		int requiredFeet = (neededDist > 1000.0f) ? 999 : (int)round(neededDist * 5.0f);
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
		int requiredFeet = (neededDist > 1000.0f) ? 999 : (int)round(neededDist * 5.0f);

		ofLogNotice("Fireball") << "Rolled: " << pendingFireballRangeResult << "ft (" << maxDistUnits << "). Needed: " << requiredFeet << "ft.";

		// --- LOGIC FIX IS HERE ---
		if (maxDistUnits >= neededDist - 0.001f) {
			// SUCCESS PATH
			ofLogNotice("Fireball") << "Direct Hit!";
			fireballImpactTile = pendingFireballTargetTile;

			// Find the target on that tile
			fireballTargetPlayerIndex = -1;
			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].x == (int)fireballImpactTile.x && players[i].y == (int)fireballImpactTile.y) {
					fireballTargetPlayerIndex = (int)i;
					break;
				}
			}

			// If a player was actually there, roll for damage.
			if (fireballTargetPlayerIndex != -1) {
				ofLogNotice("Fireball") << "Hit Player " << players[fireballTargetPlayerIndex].playerID << "! Rolling Damage...";
				pendingFireballDamageResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Fireball: Damage", currentPlayerIndex);
				isWaitingForFireballDamageDice = true;
			} else {
				// This case should be rare since your targeting requires a unit, but it's good practice.
				ofLogNotice("Fireball") << "Hit the tile, but the target had moved!";
			}

		} else {
			// FAILURE PATH
			ofLogNotice("Fireball") << "Fell short! The spell fizzles.";
			// We do nothing else. The turn continues.
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

			// Apply Ward
			int wardDamage = std::min(target->ward, damage);
			target->ward -= wardDamage;
			damage -= wardDamage;

			// Apply Health Damage
			target->health -= damage;

			// --- VISUAL FEEDBACK START ---
			glm::vec3 targetPos = gridToWorld(target->x, target->y);

			// 1. Show Damage Number
			if (damage > 0) {
				spawnFloatingText(targetPos, "-" + ofToString(damage) + " Fire", ofColor::red);
			} else {
				spawnFloatingText(targetPos, "Absorbed", ofColor::gray);
			}

			// 2. Apply Fire Status (without the text)
			if (target->health < initialHealth) {
				target->onFire = true;
			}
			// --- VISUAL FEEDBACK END ---
		}
		fireballTargetPlayerIndex = -1;
	}
	// ---
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

		// Set owner and summoning sickness - use the tracked player index
		int summoner = (pendingSummonPlayerIndex >= 0 && pendingSummonPlayerIndex < (int)players.size()) ? pendingSummonPlayerIndex : currentPlayerIndex;
		minion.ownerID = players[summoner].isMinion ? players[summoner].ownerID : players[summoner].playerID;
		// All summoned minions wait until after opponent's next turn before acting
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

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

		// 3. Graveyard Interaction

		// Prevent multiple resurrections from the same graveyard entry in the same turn
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
			// Copy deck before erasing to avoid issues if multiple minions are summoned in the same frame
			std::vector<Card> graveDeck = graveyard[gIndex].deck;
			graveyard.erase(graveyard.begin() + gIndex);
			if (!graveDeck.empty() && !isClient()) {
				// Only host rolls for graveyard loot in multiplayer
				std::uniform_int_distribution<int> graveDist(0, (int)graveDeck.size() - 1);
				int r = graveDist(gameplayRNG);
				minion.deck.push_back(graveDeck[r]);
				ofLogNotice("Raise Dead") << "Looted a card from the grave!";
			}
		}

		// 4. Add to Board
		board[minion.x][minion.y].hasPlayer = true;
		players.push_back(minion);
		// Authoritative shuffle for the new minion
		int newSkeletonIdx = (int)players.size() - 1;
		shuffleGameVector(players[newSkeletonIdx].deck, newSkeletonIdx);

		// 5. SORT TURN ORDER
		int currentID = players[currentPlayerIndex].playerID;
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
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

	// --- DEATH RESOLUTION ---
	if (isWaitingForDeathDice && activeDiceRolls.empty()) {
		isWaitingForDeathDice = false;

		Player * target = getPlayer(pendingDeathTargetIndex);
		if (target) {
			int roll = pendingDeathRollResult;

			if (roll > target->health) {
				// SUCCESS: DEATH
				spawnFloatingText(gridToWorld(target->x, target->y), "Executed!", ofColor::red);

				DeathMarker death;
				death.x = target->x;
				death.y = target->y;
				death.turnDied = globalTurnCounter;
				death.deck = target->deck;
				graveyard.push_back(death);
				board[target->x][target->y].hasPlayer = false;
				target->x = -1000;
				target->health = 0;
			} else {
				// FAIL: SLEEP (Roll Duration)
				spawnFloatingText(gridToWorld(target->x, target->y), "Sleep...", ofColor::cyan);

				// Roll 1d6 for duration
				startDiceRoll(1, 6, PURPOSE_SLEEP_DURATION, "Sleep Duration");
				isWaitingForSleepDuration = true;
				// Note: pendingDeathTargetIndex is still valid
			}
		} else {
			pendingDeathTargetIndex = -1;
		}
	}

	// --- SLEEP DURATION RESOLUTION ---
	if (isWaitingForSleepDuration && activeDiceRolls.empty()) {
		isWaitingForSleepDuration = false;
	}

	// --- ETHEREAL JOLT RESOLUTION ---
	if (isWaitingForJoltRangeDice && activeDiceRolls.empty()) {
		isWaitingForJoltRangeDice = false;
		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile = { (float)caster.x, (float)caster.y };

		// 1. Calculate Max Range (5ft = 1.0 Unit)
		float maxDistUnits = pendingJoltRangeResult / 5.0f;

		// 2. Calculate Required Distance (IGNORING WALLS)
		// Jolt goes through walls, so we use pure Euclidean Edge-to-Edge distance.
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
				glm::vec3 targetPos = gridToWorld(target->x, target->y);

				// Effect 1: Deal 7 Magic Damage (with Magic Wall stacking)
				int baseDamage = 7;
				// Check both caster and target for adjacency to magic wall
				auto isAdjacentOrDiagonalToMagicWall = [&](int x, int y) {
					for (int dx = -1; dx <= 1; ++dx) {
						for (int dy = -1; dy <= 1; ++dy) {
							if (dx == 0 && dy == 0) continue;
							int nx = x + dx, ny = y + dy;
							if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
								if (board[nx][ny].hasWall && board[nx][ny].isMagicWall) return true;
							}
						}
					}
					return false;
				};
				int wallEffectCount = 0;
				// Caster is currentPlayerIndex
				if (isAdjacentOrDiagonalToMagicWall(target->x, target->y)) wallEffectCount++;
				if (isAdjacentOrDiagonalToMagicWall(players[currentPlayerIndex].x, players[currentPlayerIndex].y)) wallEffectCount++;
				int damage = baseDamage;
				if (wallEffectCount > 0) {
					damage *= (1 << wallEffectCount); // x2 for each
					for (int i = 0; i < wallEffectCount; ++i) {
						spawnFloatingText(targetPos, "Magic Wall: x2 Magic", ofColor::purple);
					}
				}

				// Barrier Check
				int barrierDmg = std::min(target->barrier, damage);
				target->barrier -= barrierDmg;
				damage -= barrierDmg;

				// Ward Check
				if (damage > 0) {
					int wardDmg = std::min(target->ward, damage);
					target->ward -= wardDmg;
					damage -= wardDmg;
				}

				// Health Damage & Text
				if (damage > 0) {
					target->health -= damage;
					// CHANGE: Red Text + " Magic"
					spawnFloatingText(targetPos, "-" + ofToString(damage) + " Magic", ofColor::red);
				} else {
					spawnFloatingText(targetPos, "Absorbed", ofColor::gray);
				}
				ofLogNotice("Jolt") << "Dealt Damage. Health now: " << target->health;

				// Effect 2: Paralyze
				target->isParalyzed = true;
				target->paralysisHeadsCount = 0;
				// Offset Y slightly so text doesn't overlap damage numbers
				spawnFloatingText(targetPos + glm::vec3(0, 0.6f, 0), "PARALYZED!", ofColor::yellow);
				ofLogNotice("Jolt") << "Target Paralyzed.";

				// Effect 3: Mill Top Card
				if (!target->deck.empty()) {
					target->deck.pop_back();
					// Offset Y even more
					spawnFloatingText(targetPos + glm::vec3(0, 1.2f, 0), "Mind Rot!", ofColor::purple);
					ofLogNotice("Jolt") << "Target's top card removed.";
				}
			}
		} else {
			ofLogNotice("Jolt") << "Fell short! (Rolled " << pendingJoltRangeResult << "ft, needed " << requiredFeet << "ft)";
			glm::vec3 failPos = gridToWorld(pendingJoltTargetTile.x, pendingJoltTargetTile.y);
			spawnFloatingText(failPos, "Out of Range", ofColor::white);
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

			// ADD THIS: Green Text
			glm::vec3 tPos = gridToWorld(target->x, target->y);
			spawnFloatingText(tPos, "+" + ofToString(healAmount) + " HP", ofColor::green);

			ofLogNotice("Heal") << "Player " << target->playerID << " healed.";
		}
		pendingHealTargetIndex = -1;
	}

	// --- PSIONIC WAVE: RANGE RESOLUTION ---
	if (isWaitingForPsionicRange && activeDiceRolls.empty()) {
		isWaitingForPsionicRange = false;

		// 1. Calculate Radius
		// "Automatically have at least 3 range" -> Roll + 3 feet
		// Note: 2d20 minimum is 2. 2+3 = 5ft. This covers adjacent squares (distance 0 to 5ft).
		int radiusFeet = pendingPsionicRangeResult + 3;
		float radiusUnits = radiusFeet / 5.0f;

		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile((float)caster.x, (float)caster.y);

		psionicWaveTargetIndices.clear();

		ofLogNotice("Psionic") << "Range Roll: " << pendingPsionicRangeResult << " + 3 = " << radiusFeet << "ft radius.";

		// 2. Identify Targets (Circular, Through Walls)
		for (size_t i = 0; i < players.size(); ++i) {
			// Skip Self
			if ((int)i == currentPlayerIndex) continue;

			// Check Distance
			// Psionic Wave goes through walls, so use standard Euclidean distance between tiles
			// Convert tiles to feet: * 5.0f
			float distFeet = glm::distance(casterTile, glm::vec2(players[i].x, players[i].y)) * 5.0f;

			// Allow hit if center-to-center distance is within radius
			// (Using 0.1 buffer for float errors)
			if (distFeet <= radiusFeet + 0.1f) {
				psionicWaveTargetIndices.push_back((int)i);

				// Visual feedback for being targeted
				glm::vec3 tPos = gridToWorld(players[i].x, players[i].y);
				spawnFloatingText(tPos, "Targeted!", ofColor::magenta);
			}
		}

		if (psionicWaveTargetIndices.empty()) {
			spawnFloatingText(gridToWorld(caster.x, caster.y), "No Targets in Range", ofColor::gray);
		} else {
			// 3. Roll for Effect (2d4 Cards)
			pendingPsionicAmountResult = startDiceRoll(2, 4, PURPOSE_PSIONIC_WAVE_AMOUNT, "Psionic Wave: Cards to Remove");
			isWaitingForPsionicAmount = true;
		}
	}

	// --- PSIONIC WAVE: EFFECT RESOLUTION ---
	if (isWaitingForPsionicAmount && activeDiceRolls.empty()) {
		isWaitingForPsionicAmount = false;

		int cardsToRemove = pendingPsionicAmountResult;
		ofLogNotice("Psionic") << "Removing " << cardsToRemove << " cards from " << psionicWaveTargetIndices.size() << " targets.";

		for (int pIndex : psionicWaveTargetIndices) {
			Player * target = getPlayer(pIndex);
			if (!target) continue;

			int removedCount = 0;
			// Remove top cards
			for (int k = 0; k < cardsToRemove; k++) {
				if (!target->deck.empty()) {
					// Logic to remove
					Card c = target->deck.back();
					target->deck.pop_back();

					// Spawn animation for visual feedback (Flying card disappearing)
					RemovedCardAnimation anim;
					anim.card = c;
					anim.startPos = gridToWorld(target->x, target->y); // Fly from unit
					anim.startTime = ofGetElapsedTimef();
					anim.currentScale = 1.0f;
					activeRemovedCardAnimations.push_back(anim);

					removedCount++;
				} // If deck is empty, do nothing (no card removed)
			}

			if (removedCount > 0) {
				spawnFloatingText(gridToWorld(target->x, target->y), "-" + ofToString(removedCount) + " Cards", ofColor::purple);
			} else {
				spawnFloatingText(gridToWorld(target->x, target->y), "Deck Empty!", ofColor::gray);
			}
		}
		psionicWaveTargetIndices.clear();
	}

	// --- Time Vortex Logic ---
	if (isWaitingForTimeVortexDice && activeDiceRolls.empty()) {
		isWaitingForTimeVortexDice = false; // Stop waiting

		Player & currentPlayer = players[currentPlayerIndex];
		int turnsGained = pendingTimeVortexResult;

		// Add the bonus turns to the current player/minion
		currentPlayer.bonusTurns += turnsGained;

		spawnFloatingText(
			gridToWorld(currentPlayer.x, currentPlayer.y),
			"+" + ofToString(turnsGained) + " Extra Turns!",
			ofColor::cyan);

		ofLogNotice("Time Vortex") << "Unit " << currentPlayer.playerID << " gained " << turnsGained << " bonus turns.";
	}

	// --- MAGIC BOLT RESOLUTION ---
	if (isWaitingForMagicBoltRange && activeDiceRolls.empty()) {
		isWaitingForMagicBoltRange = false;

		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile = { (float)caster.x, (float)caster.y };

		// 1. Calculate Distances
		// FIX: Use Face-To-Face distance. Adjacent squares now require 0ft range.
		float maxDistUnits = pendingMagicBoltRangeResult / 5.0f;
		float neededDistUnits = getFaceToFaceDistance(casterTile, pendingMagicBoltTargetTile);

		ofLogNotice("Magic Bolt") << "Rolled Range: " << pendingMagicBoltRangeResult << "ft. Needed: " << (neededDistUnits * 5.0f) << "ft.";

		// 2. Determine Impact Tile
		glm::vec2 impactTile;
		if (maxDistUnits >= neededDistUnits - 0.01f) {
			// SUCCESS
			impactTile = pendingMagicBoltTargetTile;
			ofLogNotice("Magic Bolt") << "Target Reached.";
		} else {
			// FAILURE: Fell short.
			glm::vec2 dir = pendingMagicBoltTargetTile - casterTile;
			if (glm::length(dir) > 0) dir = glm::normalize(dir);

			bool hitWall = false;
			std::vector<glm::vec2> path = getLineOfSightPath(casterTile + 0.5f, pendingMagicBoltTargetTile + 0.5f);
			for (const auto & step : path) {
				// Stop if we exceed max rolled distance
				float distToStep = getFaceToFaceDistance(casterTile, step);
				if (distToStep > maxDistUnits) break;

				if (isTileWall((int)step.x, (int)step.y)) {
					impactTile = step;
					hitWall = true;
					break;
				}
			}

			if (!hitWall) {
				// Landed on ground at max range
				glm::vec2 impactPos = casterTile + (dir * maxDistUnits);
				impactTile = { floor(impactPos.x), floor(impactPos.y) };
			}
			ofLogNotice("Magic Bolt") << "Fell short! Impact at (" << impactTile.x << ", " << impactTile.y << ")";
		}

		// 3. APPLY EFFECTS

		// Check if impact was inside a wall
		if (isTileWall((int)impactTile.x, (int)impactTile.y)) {
			ofLogNotice("Magic Bolt") << "Bolt fizzled inside a wall. No AOE.";
			spawnFloatingText(gridToWorld((int)impactTile.x, (int)impactTile.y), "Fizzle!", ofColor::gray);
		} else {
			// Primary Damage
			int primaryDamage = startDiceRoll(1, 20, PURPOSE_DAMAGE, "Magic Bolt: Primary Damage", currentPlayerIndex);

			// Find if a unit was on the impact tile
			Player * directHitTarget = nullptr;
			for (auto & p : players) {
				if (p.x == (int)impactTile.x && p.y == (int)impactTile.y) {
					directHitTarget = &p;
					break;
				}
			}

			if (directHitTarget) {
				ofLogNotice("Magic Bolt") << "Direct Hit! Dealing " << primaryDamage << " Magic damage.";

				int dmg = primaryDamage;
				int barrierDmg = std::min(directHitTarget->barrier, dmg);
				directHitTarget->barrier -= barrierDmg;
				dmg -= barrierDmg;
				int wardDmg = std::min(directHitTarget->ward, dmg);
				directHitTarget->ward -= wardDmg;
				dmg -= wardDmg;

				if (dmg > 0) {
					directHitTarget->health -= dmg;
					spawnFloatingText(gridToWorld(directHitTarget->x, directHitTarget->y), "-" + ofToString(dmg) + " Magic", ofColor::red);
				} else {
					spawnFloatingText(gridToWorld(directHitTarget->x, directHitTarget->y), "Absorbed", ofColor::gray);
				}
			} else {
				spawnFloatingText(gridToWorld((int)impactTile.x, (int)impactTile.y), ofToString(primaryDamage) + "!", ofColor::purple);
			}

			// --- SECONDARY AOE (3 Electric Damage) ---
			// FIX: Changed to 2d20 as requested
			// AOE Roll (2d20)
			int diceRoll = startDiceRoll(2, 20, PURPOSE_RANGE, "Magic Bolt: AOE Radius");

			// FIX: Add automatic 3ft buffer
			int aoeRadiusFeet = diceRoll + 3;

			ofLogNotice("Magic Bolt") << "AOE Roll: " << diceRoll << " + 3ft = " << aoeRadiusFeet << "ft Radius.";

			for (auto & p : players) {
				// Don't hit the direct target again
				if (&p == directHitTarget) continue;

				// Calculate Distance from Impact Center to Unit Center
				float distToTargetUnits = glm::distance(impactTile, glm::vec2(p.x, p.y));
				float distToTargetFeet = distToTargetUnits * 5.0f;

				// Check Radius
				if (distToTargetFeet <= aoeRadiusFeet + 0.01f) {
					ofLogNotice("Magic Bolt") << "AOE Hit on Unit " << p.playerID << " (Dist: " << distToTargetFeet << ")";

					int dmg = 3;
					int barrierDmg = std::min(p.barrier, dmg);
					p.barrier -= barrierDmg;
					dmg -= barrierDmg;
					int wardDmg = std::min(p.ward, dmg);
					p.ward -= wardDmg;
					dmg -= wardDmg;

					if (dmg > 0) {
						p.health -= dmg;
						spawnFloatingText(gridToWorld(p.x, p.y), "-" + ofToString(dmg) + " Electric", ofColor::yellow);
					} else {
						spawnFloatingText(gridToWorld(p.x, p.y), "Absorbed", ofColor::gray);
					}
				}
			}
		}
	}

	// --- SHOOT ARROW HIT RESOLUTION ---
	if (isWaitingForShootArrow && activeDiceRolls.empty()) {
		isWaitingForShootArrow = false;
		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile = { (float)caster.x, (float)caster.y };

		float maxDistUnits = pendingShootArrowHitResult / 5.0f;
		float neededDistUnits = getFaceToFaceDistance(casterTile, pendingShootArrowTargetTile);

		ofLogNotice("ShootArrow") << "Rolled Range: " << pendingShootArrowHitResult << "ft. Needed: " << (neededDistUnits * 5.0f) << "ft.";

		if (maxDistUnits >= neededDistUnits - 0.01f) {
			// Hit: start damage roll 1d6 piercing and queue attack resolution
			pendingAttackRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Shoot Arrow: Damage", currentPlayerIndex);
			isWaitingForAttackDice = true;
			pendingAttackDamageType = DAMAGE_PIERCING;
			pendingAttackTargetIndices.clear();
			pendingAttackTargetIndices.push_back(pendingShootArrowTargetIndex);
			pendingAttackCardName = "Shoot Arrow";
			ofLogNotice("ShootArrow") << "Hit confirmed. Rolling damage.";
		} else {
			// Miss: notify
			spawnFloatingText(gridToWorld((int)pendingShootArrowTargetTile.x, (int)pendingShootArrowTargetTile.y), "Missed!", ofColor::gray);
			ofLogNotice("ShootArrow") << "Shoot Arrow fell short.";
		}

		// Clear pending target
		pendingShootArrowTargetIndex = -1;
	}

	// --- HELLHOUND SUMMON RESOLUTION ---
	if (isWaitingForHellhoundHP && activeDiceRolls.empty()) {
		isWaitingForHellhoundHP = false;

		// 1. Create Unit
		Player minion;
		minion.playerID = 1000 + (int)players.size();
		minion.x = (int)pendingSummonTile.x;
		minion.y = (int)pendingSummonTile.y;
		minion.maxHealth = pendingSummonRollResult;
		minion.health = pendingSummonRollResult;

		minion.isMinion = true;
		minion.isHellhound = true;

		// Set owner and summoning sickness
		minion.ownerID = players[currentPlayerIndex].isMinion ? players[currentPlayerIndex].ownerID : players[currentPlayerIndex].playerID;
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

		// 2. Build Deck
		for (const auto & c : allCards) {
			if (c.name == "Slash") {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
			if (c.type == CARD_FLAME_HIT) {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
			if (c.type == CARD_FIREBALL) {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
			if (c.type == CARD_DARK_SHIELD) {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
		}

		// 3. Add to Board
		board[minion.x][minion.y].hasPlayer = true;
		players.push_back(minion);
		int newHellhoundIdx = (int)players.size() - 1;
		shuffleGameVector(players[newHellhoundIdx].deck, newHellhoundIdx);

		ofLogNotice("Summon") << "Hellhound summoned with " << minion.health << " HP.";

		// 4. Sort Turn Order
		int currentID = players[currentPlayerIndex].playerID;
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
		});

		// 5. Restore Index
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == currentID) {
				currentPlayerIndex = i;
				break;
			}
		}
		invalidateTargetCache();
	}

	// --- DEMON SUMMON RESOLUTION ---
	if (isWaitingForDemonHP && activeDiceRolls.empty()) {
		isWaitingForDemonHP = false;

		Player minion;
		minion.playerID = 2000 + (int)players.size();
		minion.x = (int)pendingSummonTile.x;
		minion.y = (int)pendingSummonTile.y;
		minion.maxHealth = pendingSummonRollResult;
		minion.health = pendingSummonRollResult;

		minion.isMinion = true;
		minion.isDemon = true; // Flag for drawing/AP/Weakness

		// Set owner and summoning sickness
		minion.ownerID = players[currentPlayerIndex].isMinion ? players[currentPlayerIndex].ownerID : players[currentPlayerIndex].playerID;
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

		// Deck: 2x Death, 2x Flail, 2x Fireball, 1x Summon Hellhound, 3x Dark Shield
		for (const auto & c : allCards) {
			if (c.type == CARD_DEATH) {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
			if (c.type == CARD_FLAIL) {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
			if (c.type == CARD_FIREBALL) {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
			if (c.type == CARD_SUMMON_HELLHOUND) {
				minion.deck.push_back(c);
			}
			if (c.type == CARD_DARK_SHIELD) {
				minion.deck.push_back(c);
				minion.deck.push_back(c);
				minion.deck.push_back(c);
			}
		}

		// 3. Add to Board
		board[minion.x][minion.y].hasPlayer = true;
		players.push_back(minion);
		int newDemonIdx = (int)players.size() - 1;
		shuffleGameVector(players[newDemonIdx].deck, newDemonIdx);

		ofLogNotice("Summon") << "Demon summoned with " << minion.health << " HP.";

		// 4. Sort Turn Order
		int currentID = players[currentPlayerIndex].playerID;
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
		});

		// 5. Restore Index
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == currentID) {
				currentPlayerIndex = i;
				break;
			}
		}

		invalidateTargetCache();
	}

	// --- CHAIN LIGHTNING: RANGE RESOLUTION ---
	if (isWaitingForChainLightningRange && activeDiceRolls.empty()) {
		isWaitingForChainLightningRange = false;

		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterTile(caster.x, caster.y);

		// 1. Check Range (Face-to-Face)
		float distFeet = getFaceToFaceDistance(casterTile, pendingChainLightningTargetTile) * 5.0f;
		float maxRange = (float)pendingChainLightningRangeResult;

		ofLogNotice("ChainLightning") << "Range Roll: " << maxRange << "ft. Needed: " << distFeet << "ft.";

		if (distFeet <= maxRange + 0.1f) {
			// SUCCESS: Roll Damage (1d10)
			pendingChainLightningDamageResult = startDiceRoll(1, 10, PURPOSE_DAMAGE, "Chain Lightning: Damage", currentPlayerIndex);
			isWaitingForChainLightningDamage = true;
		} else {
			// FAIL
			glm::vec3 failPos = gridToWorld(pendingChainLightningTargetTile.x, pendingChainLightningTargetTile.y);
			spawnFloatingText(failPos, "Fizzle (Range)", ofColor::gray);
		}
	}

	// --- CHAIN LIGHTNING: DAMAGE RESOLUTION ---
	if (isWaitingForChainLightningDamage && activeDiceRolls.empty()) {
		isWaitingForChainLightningDamage = false;

		Player & caster = players[currentPlayerIndex];
		int damage = pendingChainLightningDamageResult;
		int unitsHitCount = 0;

		// 1. Grant AP Bonus
		caster.nextTurnAPBonus += 3;
		spawnFloatingText(gridToWorld(caster.x, caster.y) + glm::vec3(0, 0.5f, 0), "+3 AP Next Turn", ofColor::limeGreen);

		// 2. Identify AOE Tiles (Center + 8 neighbors)
		std::vector<glm::vec2> validTiles;
		int tx = (int)pendingChainLightningTargetTile.x;
		int ty = (int)pendingChainLightningTargetTile.y;

		for (int dx = -1; dx <= 1; dx++) {
			for (int dy = -1; dy <= 1; dy++) {
				int nx = tx + dx;
				int ny = ty + dy;

				// Physics Check (Pinch) from Target Center to Neighbor
				// This prevents lightning "leaking" through blocked diagonals
				bool blocked = false;
				if (isTileWall(nx, ny)) blocked = true;

				if (!blocked && abs(dx) == 1 && abs(dy) == 1) {
					if (isTileWall(tx + dx, ty) && isTileWall(tx, ty + dy)) blocked = true;
				}

				if (!blocked) {
					validTiles.push_back({ nx, ny });
					// Visual Zap
					spawnFloatingText(gridToWorld(nx, ny), "ZAP!", ofColor::yellow);
				}
			}
		}

		// 3. Apply Damage
		std::set<int> hitPlayerIDs; // Track who we hit to apply paralysis later

		for (const auto & tile : validTiles) {
			for (auto & p : players) {
				if (p.x == tile.x && p.y == tile.y) {
					// Apply Electric Damage (using local damage logic since applyDamage is in playCard)
					int finalDmg = damage;
					// Barrier
					int barrierDmg = std::min(p.barrier, finalDmg);
					p.barrier -= barrierDmg;
					finalDmg -= barrierDmg;
					// Ward
					if (finalDmg > 0) {
						int wardDmg = std::min(p.ward, finalDmg);
						p.ward -= wardDmg;
						finalDmg -= wardDmg;
					}

					if (finalDmg > 0) {
						p.health -= finalDmg;
						spawnFloatingText(gridToWorld(p.x, p.y), "-" + ofToString(finalDmg) + " Electric", ofColor::cyan);
					} else {
						spawnFloatingText(gridToWorld(p.x, p.y), "Absorbed", ofColor::gray);
					}

					hitPlayerIDs.insert(p.playerID);
				}
			}
		}

		// 4. Conditional Paralysis (If > 1 unit hit)
		if (hitPlayerIDs.size() > 1) {
			for (auto & p : players) {
				if (hitPlayerIDs.count(p.playerID)) {
					if (!p.isParalyzed) {
						p.isParalyzed = true;
						p.paralysisHeadsCount = 0;
						spawnFloatingText(gridToWorld(p.x, p.y) + glm::vec3(0, 1.0f, 0), "Paralyzed!", ofColor::yellow);
					}
				}
			}
		}
	}

	// --- EARTHQUAKE LOGIC ---
	if (isEarthquakeActive) {

		// PHASE 1: WAIT FOR DICE
		if (isEarthquakeDiceRolling) {
			// Check per-unit dice indices recorded when we started the quake.
			bool ready = true;
			for (auto & unit : earthquakeUnits) {
				if (unit.diceIndex < 0 || unit.diceIndex >= (int)activeDiceRolls.size() || !activeDiceRolls[unit.diceIndex].isFinishedVisual) {
					ready = false;
					break;
				}
			}

			if (ready) {
				// Capture results and remove those dice from activeDiceRolls
				std::vector<int> toErase;
				for (auto & unit : earthquakeUnits) {
					if (unit.diceIndex >= 0 && unit.diceIndex < (int)activeDiceRolls.size()) {
						int result = activeDiceRolls[unit.diceIndex].result;
						unit.tilesToMove = result;
						unit.originalDistance = result;
						unit.nextGrid = unit.startGrid + unit.direction;
						unit.isMoving = (result > 0);
						toErase.push_back(unit.diceIndex);
						unit.diceIndex = -1;
					}
				}

				// Erase in descending order
				sort(toErase.begin(), toErase.end(), std::greater<int>());
				for (int idx : toErase)
					activeDiceRolls.erase(activeDiceRolls.begin() + idx);

				isEarthquakeDiceRolling = false;
				// Start a short delay before movement so results are visible
				isEarthquakeWaiting = true;
				earthquakeWaitTimer = 2.0f; // 2 seconds delay
				isEarthquakeAnimatingStep = false;
				earthquakeT = 0.0f;
			}
			// Do NOT return here; allow the main dice update/erasure loop to run this frame.
		}

		// PHASE 1.5: WAIT BEFORE ANIMATION
		if (isEarthquakeWaiting) {
			earthquakeWaitTimer -= ofGetLastFrameTime();
			if (earthquakeWaitTimer <= 0.0f) {
				isEarthquakeWaiting = false;
				isEarthquakeAnimatingStep = true;
				earthquakeT = 0.0f;
			}
		}

		// PHASE 2: ANIMATION STEP (Simultaneous Movement)
		if (isEarthquakeAnimatingStep) {
			// Scale earthquake animation speed (0.2 = one-fifth of previous speed)
			float earthquakeSpeedScale = 0.2f;
			float speed = 2.0f * ofGetLastFrameTime() * earthquakeSpeedScale;
			earthquakeT += speed;

			bool anyStillMoving = false; // Kept to silence warning, or remove it

			// --- PRE-STEP COLLISION RESOLUTION (Iterative Chain Solver) ---
			if (earthquakeT <= speed) {
				int n = (int)earthquakeUnits.size();

				// 1. Setup Simulation State
				std::vector<int> damageDiceCount(n, 0);
				std::vector<glm::ivec2> currentPos(n);
				std::vector<glm::ivec2> intendedPos(n);
				std::vector<bool> isStopped(n, false);

				// Initialize State
				for (int i = 0; i < n; ++i) {
					currentPos[i] = earthquakeUnits[i].startGrid;
					if (earthquakeUnits[i].isMoving && earthquakeUnits[i].tilesToMove > 0) {
						intendedPos[i] = currentPos[i] + earthquakeUnits[i].direction;
						isStopped[i] = false;
					} else {
						intendedPos[i] = currentPos[i];
						isStopped[i] = true;
					}
				}

				// 2. Iterative Solver
				bool newCrashFound = true;
				int iterations = 0;
				while (newCrashFound && iterations < 20) {
					newCrashFound = false;
					iterations++;

					for (int i = 0; i < n; ++i) {
						if (isStopped[i]) continue;

						bool crashThisLoop = false;
						glm::ivec2 target = intendedPos[i];

						// A. Wall / Edge Check
						bool hitWall = false;
						bool outOfBounds = (target.x < 0 || target.x >= BOARD_WIDTH || target.y < 0 || target.y >= BOARD_HEIGHT);
						if (!outOfBounds) hitWall = board[target.x][target.y].hasWall;

						// GHOST LOGIC
						bool isGhost = false;
						if (earthquakeUnits[i].playerIndex >= 0 && earthquakeUnits[i].playerIndex < (int)players.size()) {
							isGhost = players[earthquakeUnits[i].playerIndex].inGhostForm;
						}

						if (outOfBounds || (hitWall && !isGhost)) {
							if (!isGhost) damageDiceCount[i]++;
							crashThisLoop = true;
						}

						// B. Unit-to-Unit Checks
						if (!crashThisLoop) {
							for (int j = 0; j < n; ++j) {
								if (i == j) continue;
								if (currentPos[j] == target && isStopped[j]) {
									damageDiceCount[i]++;
									damageDiceCount[j]++;
									crashThisLoop = true;
									break;
								}
								if (intendedPos[i] == currentPos[j] && intendedPos[j] == currentPos[i]) {
									damageDiceCount[i]++;
									crashThisLoop = true;
									break;
								}
								if (intendedPos[i] == intendedPos[j]) {
									damageDiceCount[i]++;
									crashThisLoop = true;
									break;
								}
							}
						}

						if (crashThisLoop) {
							isStopped[i] = true;
							intendedPos[i] = currentPos[i];
							newCrashFound = true;
						}
					}
				}

				// 3. Apply Results
				for (int i = 0; i < n; ++i) {
					if (isStopped[i] && earthquakeUnits[i].isMoving && earthquakeUnits[i].tilesToMove > 0) {
						earthquakeUnits[i].crashed = true;
						earthquakeUnits[i].isMoving = false;
						earthquakeUnits[i].tilesToMove = 0;
						earthquakeUnits[i].nextGrid = earthquakeUnits[i].startGrid;

						if (earthquakeUnits[i].playerIndex == currentPlayerIndex) {
							playerAction = NONE;
							selectedPieceGridX = -1;
							selectedPieceGridY = -1;
							hoverPath.clear();
						}
					}

					if (damageDiceCount[i] > 0 && earthquakeUnits[i].crashDiceLastStep != earthquakeStep) {
						earthquakeUnits[i].crashDiceLastStep = earthquakeStep;
						int beforeIdx = (int)activeDiceRolls.size();
						startDiceRoll(damageDiceCount[i], 4, PURPOSE_EARTHQUAKE_DAMAGE, "");
						int afterIdx = (int)activeDiceRolls.size();

						if (afterIdx > beforeIdx) {
							int newIdx = afterIdx - 1;
							activeDiceRolls[newIdx].associatedUnit = earthquakeUnits[i].playerIndex;
							std::uniform_real_distribution<float> wobbleDist(-25.0f, 25.0f);
							// Visual wobble should use visualRNG, not gameplayRNG
							glm::quat wobble = glm::angleAxis(glm::radians(wobbleDist(visualRNG)), glm::vec3(0, 1, 0));
							activeDiceRolls[newIdx].finalQuat = wobble * matchFaceToCamera(glm::vec3(0, 1, 0));
						}

						FloatingText cft;
						cft.text = "CRASH x" + ofToString(damageDiceCount[i]);
						cft.worldPos = gridToWorld(earthquakeUnits[i].startGrid.x, earthquakeUnits[i].startGrid.y) + glm::vec3(0, 1.5f, 0);
						cft.velocity = glm::vec3(0, 0.8f, 0);
						cft.startTime = ofGetElapsedTimef();
						cft.duration = 0.8f;
						cft.color = ofColor::red;
						activeFloatingTexts.push_back(cft);
					}
				}
			}

			// Update Visuals
			for (auto & unit : earthquakeUnits) {
				if (!unit.isMoving && unit.tilesToMove <= 0 && !unit.crashed) continue;
				glm::vec3 pStart = gridToWorld(unit.startGrid.x, unit.startGrid.y);
				glm::vec3 pEnd = gridToWorld(unit.nextGrid.x, unit.nextGrid.y);

				if (unit.crashed) {
					float t = earthquakeT;
					if (t < 0.5f) {
						glm::vec3 dir3 = glm::vec3(unit.direction.x, 0, unit.direction.y);
						unit.visualPos = pStart + dir3 * (t * 0.8f * TILE_SIZE);
					} else {
						glm::vec3 dir3 = glm::vec3(unit.direction.x, 0, unit.direction.y);
						unit.visualPos = pStart + dir3 * ((1.0f - t) * 0.8f * TILE_SIZE);
					}
				} else {
					unit.visualPos = glm::mix(pStart, pEnd, earthquakeT);
				}
			}

			// --- END OF STEP ---
			if (earthquakeT >= 1.0f) {
				earthquakeT = 0.0f;
				earthquakeStep++;
				bool roundComplete = true;

				for (auto & unit : earthquakeUnits) {
					if (unit.crashed) {
						unit.crashed = false;
						unit.isMoving = false;
						unit.tilesToMove = 0;
					} else if (unit.isMoving) {
						int oldX = unit.startGrid.x;
						int oldY = unit.startGrid.y;
						int newX = unit.nextGrid.x;
						int newY = unit.nextGrid.y;

						if (oldX >= 0 && oldX < BOARD_WIDTH && oldY >= 0 && oldY < BOARD_HEIGHT) board[oldX][oldY].hasPlayer = false;
						if (newX >= 0 && newX < BOARD_WIDTH && newY >= 0 && newY < BOARD_HEIGHT) board[newX][newY].hasPlayer = true;

						unit.startGrid = unit.nextGrid;
						unit.nextGrid = unit.startGrid + unit.direction;
						unit.tilesToMove--;

						if (unit.playerIndex >= 0 && unit.playerIndex < (int)players.size()) {
							players[unit.playerIndex].x = unit.startGrid.x;
							players[unit.playerIndex].y = unit.startGrid.y;
						}

						if (unit.tilesToMove > 0)
							roundComplete = false;
						else
							unit.isMoving = false;
					}
				}

				if (roundComplete) {
					isEarthquakeActive = false;
					for (int bx = 0; bx < BOARD_WIDTH; ++bx)
						for (int by = 0; by < BOARD_HEIGHT; ++by)
							board[bx][by].hasPlayer = false;

					for (size_t pi = 0; pi < players.size(); ++pi) {
						players[pi].x = std::max(0, std::min(BOARD_WIDTH - 1, players[pi].x));
						players[pi].y = std::max(0, std::min(BOARD_HEIGHT - 1, players[pi].y));
						board[players[pi].x][players[pi].y].hasPlayer = true;
					}

					if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
						playerVisualPos = gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y);
					}
					animationPath.clear();
					currentPathIndex = 0;
					isPlayerAnimating = false;
					animatingPlayerIndex = -1;
					earthquakeUnits.clear();
					invalidateTargetCache();
				}
			}

			if (currentPlayerIndex >= 0) {
				for (auto & u : earthquakeUnits) {
					if (u.playerIndex == currentPlayerIndex) {
						playerVisualPos = u.visualPos;
						break;
					}
				}
			}

			// --- DICE RESOLUTION ---
			for (auto it = activeDiceRolls.begin(); it != activeDiceRolls.end();) {
				DiceRoll & roll = *it;
				float elapsedTime = ofGetElapsedTimef() - roll.startTime;
				float spinDuration = 1.0f;

				if (elapsedTime > spinDuration && !roll.isFinishedVisual) {
					roll.isFinishedVisual = true;
					if (roll.purpose == PURPOSE_EARTHQUAKE_DAMAGE) {
						int uidx = roll.associatedUnit;
						if (uidx >= 0 && uidx < (int)players.size()) {
							if (players[uidx].inGhostForm) {
								glm::vec3 textPos = gridToWorld(players[uidx].x, players[uidx].y);
								for (const auto & eu : earthquakeUnits) {
									if (eu.playerIndex == uidx) {
										textPos = eu.visualPos;
										break;
									}
								}
								spawnFloatingText(textPos + glm::vec3(0, 0.8f, 0), "Phased (0 Dmg)", ofColor::cyan);
							} else {
								players[uidx].health -= roll.result;
								glm::vec3 textPos = gridToWorld(players[uidx].x, players[uidx].y);
								for (const auto & eu : earthquakeUnits) {
									if (eu.playerIndex == uidx) {
										textPos = eu.visualPos;
										break;
									}
								}
								spawnFloatingText(textPos + glm::vec3(0, 0.8f, 0), "-" + ofToString(roll.result), ofColor::red);

								if (players[uidx].health <= 0) {
									DeathMarker death;
									death.x = players[uidx].x;
									death.y = players[uidx].y;
									death.turnDied = globalTurnCounter;
									death.deck = players[uidx].deck;
									graveyard.push_back(death);
									if (death.x >= 0 && death.x < BOARD_WIDTH && death.y >= 0 && death.y < BOARD_HEIGHT) {
										board[death.x][death.y].hasPlayer = false;
									}
									players.erase(players.begin() + uidx);
									for (auto eit = earthquakeUnits.begin(); eit != earthquakeUnits.end();) {
										if (eit->playerIndex == uidx)
											eit = earthquakeUnits.erase(eit);
										else {
											if (eit->playerIndex > uidx) eit->playerIndex--;
											++eit;
										}
									}
									for (auto & r : activeDiceRolls) {
										if (r.associatedUnit == uidx)
											r.associatedUnit = -1;
										else if (r.associatedUnit > uidx)
											r.associatedUnit--;
									}
									if (currentPlayerIndex == uidx)
										currentPlayerIndex = std::min<int>(uidx, (int)players.size() - 1);
									else if (currentPlayerIndex > uidx)
										currentPlayerIndex--;
								}
							}
						}
						it = activeDiceRolls.erase(it);
						continue;
					}
				}
				++it;
			}
			return;
		}
	}

	// --- FLAIL RESOLUTION ---
	if (isWaitingForFlailDice && activeDiceRolls.empty()) {
		isWaitingForFlailDice = false;

		Player & caster = players[currentPlayerIndex];
		int damage = pendingFlailRollResult + 2; // 1d6 + 2

		// Define local damage applier
		auto hitTarget = [&](Player & t, int dmg) {
			int finalDmg = dmg;

			// --- GHOST CHECK (Flail is Physical) ---
			if (t.inGhostForm) {
				finalDmg = 0;
				spawnFloatingText(gridToWorld(t.x, t.y), "Phased!", ofColor::cyan);
			}
			// ---------------------------------------

			// Block
			int blockDmg = std::min(t.block, finalDmg);
			t.block -= blockDmg;
			finalDmg -= blockDmg;
			// Barrier
			int barrierDmg = std::min(t.barrier, finalDmg);
			t.barrier -= barrierDmg;
			finalDmg -= barrierDmg;
			// Ward
			if (finalDmg > 0) {
				int wardDmg = std::min(t.ward, finalDmg);
				t.ward -= wardDmg;
				finalDmg -= wardDmg;
			}
			// Health
			if (finalDmg > 0) {
				t.health -= finalDmg;
				spawnFloatingText(gridToWorld(t.x, t.y), "-" + ofToString(finalDmg) + " Physical", ofColor::red);

				// Check Death (Simple check)
				if (t.health <= 0) {
					DeathMarker death;
					death.x = t.x;
					death.y = t.y;
					death.turnDied = globalTurnCounter;
					death.deck = t.deck;
					graveyard.push_back(death);
					board[t.x][t.y].hasPlayer = false;
					t.x = -1000;
				}
			} else {
				spawnFloatingText(gridToWorld(t.x, t.y), "Blocked", ofColor::gray);
			}
		};

		// Iterate all players to find neighbors
		// (We iterate players instead of tiles because it's slightly faster/safer)
		for (auto & target : players) {
			if (&target == &caster) continue; // Don't hit self
			if (target.health <= 0) continue; // Skip dead

			int dx = target.x - caster.x;
			int dy = target.y - caster.y;

			// Check if neighbor (Chebyshev distance == 1)
			if (std::max(abs(dx), abs(dy)) == 1) {

				bool isBlocked = false;

				// Diagonal Pinch Check
				if (abs(dx) == 1 && abs(dy) == 1) {
					// If both shared orthogonal tiles are walls, the diagonal is blocked
					if (isTileWall(caster.x + dx, caster.y) && isTileWall(caster.x, caster.y + dy)) {
						isBlocked = true;
					}
				}

				// Also check if target is inside a wall (shouldn't happen, but safety)
				if (isTileWall(target.x, target.y)) isBlocked = true;

				if (!isBlocked) {
					ofLogNotice("Flail") << "Hit unit at " << target.x << "," << target.y;
					hitTarget(target, damage);
				}
			}
		}
	}

	// --- SPARK OF GENIUS RESOLUTION ---
	if (isWaitingForSparkOfGeniusDice && activeDiceRolls.empty()) {
		isWaitingForSparkOfGeniusDice = false;

		int cardsToDraw = pendingSparkOfGeniusRollResult;
		Player & p = players[currentPlayerIndex];

		ofLogNotice("Spark of Genius") << "Rolled a " << cardsToDraw << ". Drawing cards...";

		// Loop to draw the specific number of cards
		// Your drawCard() function already handles deck reshuffling automatically.
		for (int i = 0; i < cardsToDraw; i++) {
			drawCard();
		}

		spawnFloatingText(gridToWorld(p.x, p.y), "Spark of Genius! +" + ofToString(cardsToDraw) + " Cards", ofColor::cyan);
	}

	// --- Dispel Barrier Dice ---
	if (isWaitingForBarrierDice && activeDiceRolls.empty()) {
		isWaitingForBarrierDice = false;
		Player & p = players[currentPlayerIndex];
		p.barrier += pendingDispelRollResult;

		// MATCH UI COLOR: Indigo/Deep Purple (0x480082)
		spawnFloatingText(gridToWorld(p.x, p.y),
			"+" + ofToString(pendingDispelRollResult) + " Barrier",
			ofColor::fromHex(0x480082));

		ofLogNotice("Dispel") << "Gained " << pendingDispelRollResult << " Barrier.";

		// Trigger Shell Spike if in Tortoise Form
		tryTriggerShellSpike();
	}
	// --- Teleport Logic: After dice roll, enter targeting mode ---
	if (isWaitingForTeleportDice && activeDiceRolls.empty()) {
		isWaitingForTeleportDice = false;
		// Now enter targeting mode - player will click where to teleport
		isTargetingTeleport = true;
		ofLogNotice("Teleport") << "Rolled: " << pendingTeleportRollResult << "ft. Choose destination. CardIdx=" << pendingTeleportCardIndex;
		calculateTargetHighlights(pendingTeleportCardIndex);

		// Debug: count highlighted tiles
		int targetableCount = 0;
		for (int x = 0; x < BOARD_WIDTH; x++) {
			for (int y = 0; y < BOARD_HEIGHT; y++) {
				if (board[x][y].isTargetable) targetableCount++;
			}
		}
		ofLogNotice("Teleport") << "Targetable tiles after calculateTargetHighlights: " << targetableCount;
	}

	// --- On Fire Logic ---
	if (isWaitingForOnFireDice && activeDiceRolls.empty()) {
		isWaitingForOnFireDice = false;
		int rollResult = pendingOnFireRollResult;
		Player & burningPlayer = players[currentPlayerIndex];

		// Apply Damage
		burningPlayer.health -= rollResult;
		spawnFloatingText(gridToWorld(burningPlayer.x, burningPlayer.y), "-" + ofToString(rollResult) + " Fire", ofColor::red);

		// --- FORM TRACKING (TORTOISE/GHOST) ---
		if (burningPlayer.inTortoiseForm) {
			burningPlayer.tortoiseDamageTaken += rollResult;
			if (burningPlayer.tortoiseDamageTaken >= 5) {
				burningPlayer.inTortoiseForm = false;
				burningPlayer.tortoiseDamageTaken = 0;
				burningPlayer.discardPile.push_back(burningPlayer.tortoiseFormCard);
				spawnFloatingText(gridToWorld(burningPlayer.x, burningPlayer.y) + glm::vec3(0, 0.5f, 0), "Form Ended!", ofColor::darkGreen);
			}
		}
		if (burningPlayer.inGhostForm) {
			burningPlayer.ghostDamageTaken += rollResult;
			if (burningPlayer.ghostDamageTaken >= 4) {
				burningPlayer.inGhostForm = false;
				burningPlayer.ghostDamageTaken = 0;
				burningPlayer.discardPile.push_back(burningPlayer.ghostFormCard);
				spawnFloatingText(gridToWorld(burningPlayer.x, burningPlayer.y) + glm::vec3(0, 0.5f, 0), "Ghost Form Broken!", ofColor::white);

				if (board[burningPlayer.x][burningPlayer.y].hasWall) {
					burningPlayer.health = 0;
					spawnFloatingText(gridToWorld(burningPlayer.x, burningPlayer.y) + glm::vec3(0, 1.0f, 0), "Materialized in Wall!", ofColor::red);
				}
			}
		}
		// --------------------------------------

		if (rollResult == 1 || rollResult == 2) {
			burningPlayer.onFire = false;
			spawnFloatingText(gridToWorld(burningPlayer.x, burningPlayer.y) + glm::vec3(0, 0.8f, 0), "Extinguished", ofColor::white);
		}

		// CHECK SLEEP AFTER FIRE
		if (burningPlayer.sleepTurnsRemaining > 0) {
			startNewTurn();
			return;
		}

		continueNewTurn();
		return;
	}
	// --- Poison Status Logic ---
	if (isWaitingForPoisonDice && activeDiceRolls.empty()) {
		isWaitingForPoisonDice = false;
		int rollResult = pendingPoisonRollResult;
		Player & poisonedPlayer = players[currentPlayerIndex];

		int actualDamage = std::max(0, rollResult - poisonedPlayer.poisonReduction);

		if (actualDamage > 0) {
			poisonedPlayer.health -= actualDamage;
			spawnFloatingText(gridToWorld(poisonedPlayer.x, poisonedPlayer.y), "-" + ofToString(actualDamage) + " Poison", ofColor::green);

			// --- FORM TRACKING (TORTOISE/GHOST) ---
			if (poisonedPlayer.inTortoiseForm) {
				poisonedPlayer.tortoiseDamageTaken += actualDamage;
				if (poisonedPlayer.tortoiseDamageTaken >= 5) {
					poisonedPlayer.inTortoiseForm = false;
					poisonedPlayer.tortoiseDamageTaken = 0;
					poisonedPlayer.discardPile.push_back(poisonedPlayer.tortoiseFormCard);
					spawnFloatingText(gridToWorld(poisonedPlayer.x, poisonedPlayer.y) + glm::vec3(0, 0.5f, 0), "Form Ended!", ofColor::darkGreen);
				}
			}
			if (poisonedPlayer.inGhostForm) {
				poisonedPlayer.ghostDamageTaken += actualDamage;
				if (poisonedPlayer.ghostDamageTaken >= 4) {
					poisonedPlayer.inGhostForm = false;
					poisonedPlayer.ghostDamageTaken = 0;
					poisonedPlayer.discardPile.push_back(poisonedPlayer.ghostFormCard);
					spawnFloatingText(gridToWorld(poisonedPlayer.x, poisonedPlayer.y) + glm::vec3(0, 0.5f, 0), "Ghost Form Broken!", ofColor::white);

					if (board[poisonedPlayer.x][poisonedPlayer.y].hasWall) {
						poisonedPlayer.health = 0;
						spawnFloatingText(gridToWorld(poisonedPlayer.x, poisonedPlayer.y) + glm::vec3(0, 1.0f, 0), "Materialized in Wall!", ofColor::red);
					}
				}
			}
			// --------------------------------------
		} else {
			spawnFloatingText(gridToWorld(poisonedPlayer.x, poisonedPlayer.y), "Poison Fading", ofColor::gray);
		}

		poisonedPlayer.poisonReduction++;

		if (poisonedPlayer.poisonReduction >= 6) {
			poisonedPlayer.isPoisoned = false;
			poisonedPlayer.poisonReduction = 0;
			spawnFloatingText(gridToWorld(poisonedPlayer.x, poisonedPlayer.y) + glm::vec3(0, 0.8f, 0), "Poison Cured!", ofColor::white);
		}

		continueNewTurn();
		return;
	}

	// --- CRITICAL FIX: DICE ROLL & ANIMATION UPDATES ---
	for (auto it = activeDiceRolls.begin(); it != activeDiceRolls.end();) {
		DiceRoll & roll = *it;
		float elapsedTime = ofGetElapsedTimef() - roll.startTime;
		float spinDuration = 1.0f;
		float hangTime = 2.5f;
		roll.currentRotation += diceSpinSpeed * ofGetLastFrameTime();

		if (elapsedTime > spinDuration && !roll.isFinishedVisual) {
			roll.isFinishedVisual = true;

			// Build dice result text when dice finish
			// Check if this is part of a group and if all in the group are finished
			bool allGroupFinished = true;
			std::vector<DiceRoll *> groupRolls;
			DicePurpose checkPurpose = roll.purpose;

			// Group dice rolls by purpose (combine AP and BONUS_AP together)
			for (auto & r : activeDiceRolls) {
				if (r.purpose == checkPurpose || (checkPurpose == PURPOSE_AP && r.purpose == PURPOSE_BONUS_AP) || (checkPurpose == PURPOSE_BONUS_AP && r.purpose == PURPOSE_AP)) {
					groupRolls.push_back(&r);
					if (!r.isFinishedVisual) allGroupFinished = false;
				}
			}

			// Build result text when all dice in group finish (show for ALL dice types)
			if (allGroupFinished && !groupRolls.empty()) {
				std::string resultText = "";
				int total = 0;
				int headsCount = 0;
				int tailsCount = 0;

				// Check if coins
				bool isCoins = (checkPurpose == PURPOSE_COIN_FLIP);

				if (isCoins) {
					// Count heads and tails
					for (auto * r : groupRolls) {
						if (r->result == 2)
							headsCount++;
						else if (r->result == 1)
							tailsCount++;
					}

					if (groupRolls.size() == 1) {
						resultText = (roll.result == 2) ? "Heads" : "Tails";
					} else {
						resultText = "Heads: " + ofToString(headsCount) + "  Tails: " + ofToString(tailsCount);
					}
				} else {
					// Regular dice - show results for ALL dice types (damage, HP, healing, range, etc.)
					if (groupRolls.size() == 1) {
						resultText = "Rolled " + ofToString(roll.result);
					} else {
						// Multiple dice - show individual rolls in order, then total
						resultText = "Rolled ";
						for (size_t i = 0; i < groupRolls.size(); i++) {
							resultText += ofToString(groupRolls[i]->result);
							total += groupRolls[i]->result;
							if (i < groupRolls.size() - 1) {
								resultText += " + ";
							}
						}
						resultText += " = " + ofToString(total);
					}
				}

				// Set the text for display (works for ALL dice purposes)
				diceRollResultText = resultText;
				diceRollResultStartTime = ofGetElapsedTimef();
			}

			// CORRECTED LOGIC: Check for DEBUG first. If it's not a debug roll,
			// THEN execute all the game-related logic inside this block.
			if (roll.purpose != PURPOSE_DEBUG && roll.purpose != PURPOSE_HP && roll.purpose != PURPOSE_HEALING) {
				if (roll.purpose == PURPOSE_AP) {
					// Sum all finished AP and BONUS_AP dice rolls belonging to the current unit
					int apSum = 0;
					for (const auto & r : activeDiceRolls) {
						if ((r.purpose == PURPOSE_AP || r.purpose == PURPOSE_BONUS_AP) && r.isFinishedVisual && r.associatedUnit == currentPlayerIndex) {
							apSum += r.result;
						}
					}
					currentAP = apSum;
					if (players[currentPlayerIndex].nextTurnAPBonus > 0) {
						currentAP += players[currentPlayerIndex].nextTurnAPBonus;
						players[currentPlayerIndex].nextTurnAPBonus = 0;
					}
					// Sync AP to player struct
					players[currentPlayerIndex].ap = currentAP;

					// If AP is zero, check for adjacent assistants belonging to this unit
					if (currentAP == 0) {
						Player & actor = players[currentPlayerIndex];
						for (auto & a : players) {
							if (a.isAssistant && a.health > 0 && a.directSummonerID == actor.playerID && !a.assistantRerollUsedThisTurn) {
								int dist = abs(a.x - actor.x) + abs(a.y - actor.y);
								if (dist <= 1) {
									// consume assistant's reroll and grant a bonus reroll matching the original AP dice
									a.assistantRerollUsedThisTurn = true;
									int rerollNum = lastAPDiceNum > 0 ? lastAPDiceNum : 1;
									int rerollSides = lastAPDiceSides > 0 ? lastAPDiceSides : 6;
									// Mark any previous AP dice as debug so they won't be included twice
									for (auto & oldR : activeDiceRolls) {
										if (oldR.purpose == PURPOSE_AP) oldR.purpose = PURPOSE_DEBUG;
									}
									// Start a bonus AP roll (added on top of the original result)
									startDiceRoll(rerollNum, rerollSides, PURPOSE_BONUS_AP, "Assistant Auto Reroll", currentPlayerIndex);
									spawnFloatingText(gridToWorld(a.x, a.y), "Assistant Reroll!", ofColor::gold);
								}
							}
						}
					}
					ofLogNotice("Game") << "AP Roll Finished: " << currentAP << " AP awarded (sum of all dice).";

					// HOST: Send TurnStart packet to client once AP dice are finished (for BOTH turns)
					bool allDiceFinished = true;
					for (const auto & d : activeDiceRolls) {
						if (!d.isFinishedVisual && (d.purpose == PURPOSE_AP || d.purpose == PURPOSE_BONUS_AP)) {
							allDiceFinished = false;
							break;
						}
					}
					static int lastTurnStartSentPlayer = -1;
					static int lastTurnStartSentCounter = -1;
					bool alreadySent = (lastTurnStartSentPlayer == currentPlayerIndex && lastTurnStartSentCounter == globalTurnCounter);
					if (isHost() && isMultiplayer && allDiceFinished && !alreadySent) {
						TurnStartPacket tpk = {};
						tpk.type = PKT_TURN_START;
						tpk.playerID = myLocalPlayerID;
						tpk.currentPlayerIndex = currentPlayerIndex;
						tpk.diceNum = 0;
						tpk.diceSides = (uint8_t)lastAPDiceSides;
						tpk.purpose = PURPOSE_AP;
						tpk.finalTotal = currentAP;
						// Count actual AP/BONUS_AP dice and populate packet
						for (const auto & d : activeDiceRolls) {
							if ((d.purpose == PURPOSE_AP || d.purpose == PURPOSE_BONUS_AP) && tpk.diceNum < 8) {
								tpk.rawResults[tpk.diceNum] = (uint8_t)d.rawResult;
								tpk.finalResults[tpk.diceNum] = (uint8_t)d.result;
								tpk.diceNum++;
							}
						}
						steamManager.sendPacket(&tpk, sizeof(tpk));
						lastTurnStartSentPlayer = currentPlayerIndex;
						lastTurnStartSentCounter = globalTurnCounter;
						ofLogNotice("Network") << "Host sent TurnStart (continueNewTurn): player=" << tpk.currentPlayerIndex << " dice=" << (int)tpk.diceNum << " total=" << tpk.finalTotal;
					}
				} else if (roll.purpose == PURPOSE_SLEEP_DURATION) {
					Player * t = getPlayer(pendingDeathTargetIndex);
					if (t) {
						t->sleepTurnsRemaining = roll.result;
						spawnFloatingText(gridToWorld(t->x, t->y), ofToString(roll.result) + " Turns Sleep", ofColor::cyan);
					}
					pendingDeathTargetIndex = -1;
				} else if (roll.purpose == PURPOSE_DEATH_CHECK) {
					pendingDeathRollResult = roll.result;
					// Logic is handled in the separate updateGame block
				} else if (roll.purpose == PURPOSE_EARTHQUAKE_DAMAGE) {
					// Find associated unit and apply damage now (during animation)
					int uidx = roll.associatedUnit;
					if (uidx >= 0 && uidx < (int)players.size()) {
						// If we already applied immediate crash damage for this earthquake unit,
						// don't apply again here (pre-spawn logic may have applied it).
						bool alreadyApplied = false;
						for (const auto & eu : earthquakeUnits) {
							if (eu.playerIndex == uidx && eu.crashDamageApplied) {
								alreadyApplied = true;
								break;
							}
						}
						if (!alreadyApplied) {
							players[uidx].health -= roll.result;
							// Prefer visualPos (unit may be mid-move). Find earthquake unit entry if present.
							glm::vec3 textPos = gridToWorld(players[uidx].x, players[uidx].y);
							for (const auto & eu : earthquakeUnits) {
								if (eu.playerIndex == uidx) {
									textPos = eu.visualPos;
									break;
								}
							}
							spawnFloatingText(textPos + glm::vec3(0, 0.8f, 0), "-" + ofToString(roll.result), ofColor::red);
							ofLogNotice("Earthquake") << "Player " << uidx << " took " << roll.result << " quake damage.";
						} else {
							ofLogNotice("Earthquake") << "Skipping duplicate quake damage for Player " << uidx << ".";
						}
					}
				} else if (roll.purpose == PURPOSE_BONUS_AP) {
					currentAP += roll.result;
					spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y),
						"+" + ofToString(roll.result) + " Bonus AP",
						ofColor::yellow);
					ofLogNotice("Game") << "Bonus Dice Finished: " << roll.result << " AP awarded.";
				} else if (roll.purpose == PURPOSE_BLOCKING_BOON_COIN) {
					// Result 2 = Heads, 1 = Tails
					// Heads: Raise Max HP
					// Tails: Lower Target Max HP
					if (roll.result >= 2) {
						// Heads
						players[currentPlayerIndex].maxHealth++;
						spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y), "+1 Max HP", ofColor::green);
					} else {
						// Tails
						Player * t = getPlayer(blockingBoonTargetIndex);
						if (t) {
							t->maxHealth = std::max(1, t->maxHealth - 1);
							// Clamp current health if it exceeds new max
							if (t->health > t->maxHealth) t->health = t->maxHealth;
							spawnFloatingText(gridToWorld(t->x, t->y), "-1 Max HP", ofColor::darkRed);
						}
					}

					// If we're resolving a staged Blocking Boon, decrement the outstanding coin count
					if (isWaitingForBlockingBoonCoins) {
						pendingBlockingBoonCoinsRemaining = std::max(0, pendingBlockingBoonCoinsRemaining - 1);
						// Decrement combined outstanding counter as well
						pendingBlockingBoonTotal = std::max(0, pendingBlockingBoonTotal - 1);

						// --- START FIX ---
						// This block is now simplified. It ONLY updates the state.
						// The startDiceRoll() call has been moved outside the loop.
						if (pendingBlockingBoonCoinsRemaining == 0) {
							// All coin flips have finished processing.
							// Set the flag to false so the next stage can be triggered outside this loop.
							isWaitingForBlockingBoonCoins = false;

							// If our combined counter says everything's done, clear the active guard.
							// This happens if there were only coin rolls and no D20s.
							if (pendingBlockingBoonTotal == 0) {
								blockingBoonActive = false;
							}
						}
						// --- END FIX ---
					}
				} else if (roll.purpose == PURPOSE_BLOCKING_BOON_D20) {
					// Result includes Luck
					int val = roll.result;
					int classReward = 0;

					if (val >= 20)
						classReward = 3;
					else if (val >= 16)
						classReward = 2;
					else if (val >= 10)
						classReward = 1;

					if (classReward > 0) {
						pendingDraftQueue.push_back(classReward);
						spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y), "Draft C" + ofToString(classReward), ofColor::cyan);
					} else {
						// Fail (1-9)
						spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y), "Fizzle", ofColor::gray);
					}

					// After processing a D20 result, clear active flag if there are no UNPROCESSED blocking-boon dice left
					// Decrement combined outstanding counter and clear guard if finished
					pendingBlockingBoonTotal = std::max(0, pendingBlockingBoonTotal - 1);
					if (pendingBlockingBoonTotal == 0) blockingBoonActive = false;
				} else if (roll.purpose == PURPOSE_SUMMON_KOBOLDS) {
					// Resolve Call for Kobolds roll
					isWaitingForKoboldDice = false;
					int count = roll.result;
					if (count <= 0) {
						spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y), "No Kobolds!", ofColor::gray);
						isPlacingKobolds = false;
					} else {
						// Count available adjacent empty tiles
						int avail = 0;
						glm::vec2 adj[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
						for (auto & d : adj) {
							int nx = koboldPlacementSourceX + (int)d.x;
							int ny = koboldPlacementSourceY + (int)d.y;
							if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
								if (!board[nx][ny].hasWall && !board[nx][ny].hasPlayer) avail++;
							}
						}
						int allowed = std::min<int>(count, std::min(avail, 4));
						if (allowed <= 0) {
							spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y), "No Space!", ofColor::red);
							isPlacingKobolds = false;
						} else {
							koboldsRemainingToPlace = allowed;
							koboldSummonCount = 0;
							isPlacingKobolds = true;
							// Instruction UI
							tooltipText = "Place Kobold: click an adjacent empty tile";
							isShowingTooltip = true;
							spawnFloatingText(gridToWorld(koboldPlacementSourceX, koboldPlacementSourceY), ofToString(koboldsRemainingToPlace) + " Kobolds!", ofColor::gold);
							invalidateTargetCache();
						}
					}
					return;
				} else if (roll.purpose == PURPOSE_COIN_FLIP) {
					// 1. Resolve Paralysis Flip
					if (isWaitingForParalysisCoin) {
						isWaitingForParalysisCoin = false;
						int flipResult = roll.result;
						Player & p = players[currentPlayerIndex];

						// Check for 2 (Heads)
						if (flipResult == 2) {
							// Increment heads count
							p.paralysisHeadsCount++;

							// If 2 heads in a row, cure paralysis
							if (p.paralysisHeadsCount >= 2) {
								ofLogNotice("Paralysis") << "2nd Heads! Paralysis is cured.";
								p.isParalyzed = false;
								p.paralysisHeadsCount = 0;
							} else {
								ofLogNotice("Paralysis") << "Heads! Can play this turn (" << p.paralysisHeadsCount << "/2 heads).";
							}

							// Continue turn...
							if (p.onFire) {
								isWaitingForOnFireDice = true;
								pendingOnFireRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE);
							} else {
								continueNewTurn();
							}
							return;
						} else { // Rolled 1 (Tails)
							ofLogNotice("Paralysis") << "Tails! Player remains paralyzed and skips turn.";
							// Reset heads count
							p.paralysisHeadsCount = 0;
							// End turn immediately
							startNewTurn();
							return;
						}
					}
					// 2. Resolve Wolf Summoning Flip
					else if (isWaitingForWolfCoin) {
						isWaitingForWolfCoin = false;
						int flipResult = roll.result; // 1=Tails, 2=Heads

						// Find Summoner for Text Position
						Player * summoner = nullptr;
						for (auto & p : players) {
							if (p.x == wolfPlacementSourceX && p.y == wolfPlacementSourceY) {
								summoner = &p;
								break;
							}
						}
						glm::vec3 textPos = summoner ? gridToWorld(summoner->x, summoner->y) : glm::vec3(0, 0, 0);

						if (flipResult == 1) {
							// TAILS: FAIL
							ofLogNotice("Wolves") << "Tails! The call fizzles.";
							spawnFloatingText(textPos, "Fizzles...", ofColor::gray);

							// End the sequence
							isPlacingWolves = false;
							wolfSummonStage = 0;

							// --- CRITICAL FIX: CAPTURE ID BEFORE SORT ---
							int myID = players[currentPlayerIndex].playerID;

							// Cleanup Turn Order
							std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
								int ownerA = a.isMinion ? a.ownerID : a.playerID;
								int ownerB = b.isMinion ? b.ownerID : b.playerID;
								if (ownerA != ownerB) return ownerA < ownerB;
								if (a.isMinion && !b.isMinion) return true;
								if (!a.isMinion && b.isMinion) return false;
								return a.summonOrder < b.summonOrder;
							});

							// Fix index
							for (size_t i = 0; i < players.size(); i++) {
								if (players[i].playerID == myID) {
									currentPlayerIndex = i;
									break;
								}
							}
						} else {
							// HEADS: Check if we have space for the 2nd wolf
							bool hasSpace = false;
							glm::vec2 adj[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
							for (auto & d : adj) {
								int nx = wolfPlacementSourceX + (int)d.x;
								int ny = wolfPlacementSourceY + (int)d.y;
								if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
									if (!board[nx][ny].hasWall && !board[nx][ny].hasPlayer) {
										hasSpace = true;
										break;
									}
								}
							}

							if (hasSpace) {
								// SUCCESS
								ofLogNotice("Wolves") << "Heads! You can place another wolf.";
								spawnFloatingText(textPos, "Double Summon!", ofColor::gold);
								wolfSummonStage = 2; // Advance stage to Wolf 2
							} else {
								// HEADS BUT BLOCKED
								ofLogNotice("Wolves") << "Heads, but no space for 2nd wolf.";
								spawnFloatingText(textPos, "No Space!", ofColor::red);
								isPlacingWolves = false;
								wolfSummonStage = 0;

								// --- CRITICAL FIX: CAPTURE ID BEFORE SORT ---
								int myID = players[currentPlayerIndex].playerID;

								std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
									int ownerA = a.isMinion ? a.ownerID : a.playerID;
									int ownerB = b.isMinion ? b.ownerID : b.playerID;
									if (ownerA != ownerB) return ownerA < ownerB;
									if (a.isMinion && !b.isMinion) return true;
									if (!a.isMinion && b.isMinion) return false;
									return a.summonOrder < b.summonOrder;
								});
								for (size_t i = 0; i < players.size(); i++) {
									if (players[i].playerID == myID) {
										currentPlayerIndex = i;
										break;
									}
								}
							}
						}
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
	} // This is the closing brace of the "for (auto it = activeDiceRolls.begin()..." loop

	// --- START FIX: Blocking Boon Stage Transition ---
	// After iterating through dice, check if the coin stage just finished and the D20 stage is queued.
	// This safely starts the next set of dice rolls without modifying the vector during iteration.
	if (!isWaitingForBlockingBoonCoins && pendingBlockingBoonNonPhys > 0) {
		ofLogNotice("Blocking Boon") << "Coins finished; now rolling " << pendingBlockingBoonNonPhys << " D20s for Non-Phys Block.";
		startDiceRoll(pendingBlockingBoonNonPhys, 20, PURPOSE_BLOCKING_BOON_D20, "Boon: Magic Roll", currentPlayerIndex);
		pendingBlockingBoonNonPhys = 0; // Mark as rolled
	}
	// --- END FIX ---

	// ================== PASTE YOUR NEW CODE HERE ==================
	// Check if we need to start a chained draft
	if (currentState == STATE_GAMEPLAY && activeDiceRolls.empty() && !pendingDraftQueue.empty()) {
		// Sort queue for better UX? (High class first?)
		std::sort(pendingDraftQueue.begin(), pendingDraftQueue.end(), std::greater<int>());

		int nextClass = pendingDraftQueue.front();
		pendingDraftQueue.erase(pendingDraftQueue.begin());

		isInGameDraft = true;
		draftPlayerIndex = currentPlayerIndex;
		generateDraftOptions(nextClass);
		draftPicksRemaining = 1;
		selectedDraftIndices.clear();
		draftStage = 0;
		currentState = STATE_DRAFTING;

		ofLogNotice("Blocking Boon") << "Starting chained draft for Class " << nextClass << ". Remaining in queue: " << pendingDraftQueue.size();
	}
	// ==============================================================

	// Update Floating Text
	for (auto it = activeFloatingTexts.begin(); it != activeFloatingTexts.end();) {

		float dt = ofGetLastFrameTime();
		it->worldPos += it->velocity * dt;
		it->velocity.y *= 0.95f; // Slow down upward movement (gravity drag)

		if (ofGetElapsedTimef() - it->startTime > it->duration) {
			it = activeFloatingTexts.erase(it);
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

	// Played Card Animation (appears at center, holds, then fades out)
	for (auto & anim : activePlayedCardAnimations) {
		float elapsedTime = ofGetElapsedTimef() - anim.startTime;
		// Keep it large and on the right-hand side
		anim.currentScale = 2.6f;
		float handBaseCardWidth = 120.0f;
		float w = handBaseCardWidth * anim.currentScale;
		anim.pos = glm::vec2(ofGetWidth() - (w / 2.0f) - 40.0f, ofGetHeight() / 2.0f);

		if (elapsedTime < 2.5f) {
			// Hold at full size/alpha
			anim.currentAlpha = 255.0f;
		} else if (elapsedTime < 3.0f) {
			// Fade out
			float t = ofMap(elapsedTime, 2.5f, 3.0f, 0.0f, 1.0f, true);
			anim.currentAlpha = ofLerp(255.0f, 0.0f, t);
		}
	}
	activePlayedCardAnimations.erase(std::remove_if(activePlayedCardAnimations.begin(), activePlayedCardAnimations.end(), [](const PlayedCardAnimation & anim) { return (ofGetElapsedTimef() - anim.startTime) >= 3.0f; }), activePlayedCardAnimations.end());

	for (auto & anim : activeRemovedCardAnimations) {
		float elapsedTime = ofGetElapsedTimef() - anim.startTime;

		// Only update if start time has passed
		if (elapsedTime >= 0.0f && elapsedTime < 0.5f) {
			float t = elapsedTime / 0.5f;
			anim.currentScale = ofLerp(1.6f, 0.1f, t); // Shrink
			// If it came from Shoot Arrow (Scale 3.0), we might want to scale down from that
			if (anim.currentScale > 1.6f) anim.currentScale = ofLerp(3.0f, 0.1f, t);

			anim.currentAlpha = ofLerp(255, 0, t);
		}
		// Keep hidden if waiting for delay
		if (elapsedTime < 0.0f) {
			anim.currentAlpha = 0;
		}
	}
	// Remove only if finished
	activeRemovedCardAnimations.erase(std::remove_if(activeRemovedCardAnimations.begin(), activeRemovedCardAnimations.end(), [](const RemovedCardAnimation & anim) { return (ofGetElapsedTimef() - anim.startTime) >= 0.5f; }), activeRemovedCardAnimations.end());

	// Card Display Animation (Appears, holds, then fades out)
	for (auto & disp : activeCardDisplays) {
		float elapsedTime = ofGetElapsedTimef() - disp.startTime;
		if (elapsedTime < 1.0f) {
			// Scale down from 1.5 to 1.0 while staying opaque
			disp.currentScale = ofLerp(1.5f, 1.0f, elapsedTime / 1.0f);
			disp.currentAlpha = 255.0f;
		} else if (elapsedTime < 2.5f) {
			// Hold at normal size
			disp.currentScale = 1.0f;
			disp.currentAlpha = 255.0f;
		} else if (elapsedTime < 3.0f) {
			// Fade out over 0.5 seconds
			float t = ofMap(elapsedTime, 2.5f, 3.0f, 0.0f, 1.0f, true);
			disp.currentAlpha = ofLerp(255.0f, 0.0f, t);
			disp.currentScale = 1.0f;
		}
	}
	// Remove when animation is done (3 seconds total)
	activeCardDisplays.erase(std::remove_if(activeCardDisplays.begin(), activeCardDisplays.end(), [](const PlayedCardDisplay & disp) { return (ofGetElapsedTimef() - disp.startTime) >= 3.0f; }), activeCardDisplays.end());

	// Card Hand Animation
	if (!players.empty() && currentPlayerIndex >= 0) {
		// In multiplayer, update BOTH local and opponent player's hands
		// In singleplayer, update current player's hand
		Player * handPlayer = nullptr;
		Player * opponentHandPlayer = nullptr;

		if (isMultiplayer) {
			int localID = myLocalPlayerID;
			int opponentID = (localID == 0) ? 1 : 0;

			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].playerID == localID && !players[i].isMinion) {
					handPlayer = &players[i];
				}
				if (players[i].playerID == opponentID && !players[i].isMinion) {
					opponentHandPlayer = &players[i];
				}
			}
		} else {
			handPlayer = &players[currentPlayerIndex];
		}

		if (handPlayer) {
			Player & currentPlayer = *handPlayer;
			bool showOpponentHand = (isMultiplayer && opponentHandPlayer && !isCurrentPlayerLocal());

			// Calculate total cards in shared hand area (local + opponent in multiplayer)
			size_t totalCards = currentPlayer.hand.size();
			if (showOpponentHand) {
				totalCards += opponentHandPlayer->hand.size();
			}

			float handCenterY = ofGetHeight() - 130;
			float handBaseCardWidth = 120;
			float handAreaWidth = ofGetWidth() * 0.6f; // Increased for both hands

			float totalCardWidths = totalCards * handBaseCardWidth;
			float padding = (totalCards > 1) ? (handAreaWidth - totalCardWidths) / (totalCards - 1) : 0;
			padding = std::min(padding, 20.0f);
			float totalHandWidth = (totalCards * handBaseCardWidth) + ((totalCards - 1) * padding);
			float startX = (ofGetWidth() - totalHandWidth) / 2.0f;

			// Position local player's cards
			size_t numCards = currentPlayer.hand.size();
			for (size_t i = 0; i < numCards; i++) {
				float cardCenterX = startX + i * (handBaseCardWidth + padding) + (handBaseCardWidth / 2.0f);
				currentPlayer.hand[i].targetPos = ofVec2f(cardCenterX, handCenterY);

				if (static_cast<int>(i) != draggedCardIndex) {
					currentPlayer.hand[i].currentScale = ofLerp(currentPlayer.hand[i].currentScale, currentPlayer.hand[i].targetScale, 0.25f);
					currentPlayer.hand[i].currentPos = currentPlayer.hand[i].currentPos.getInterpolated(currentPlayer.hand[i].targetPos, 0.25f);
				}
			}

			// Position opponent's cards (continuing from where local player's cards end)
			if (showOpponentHand) {
				for (size_t i = 0; i < opponentHandPlayer->hand.size(); i++) {
					size_t offset = numCards + i;
					float cardCenterX = startX + offset * (handBaseCardWidth + padding) + (handBaseCardWidth / 2.0f);
					opponentHandPlayer->hand[i].targetPos = ofVec2f(cardCenterX, handCenterY);

					opponentHandPlayer->hand[i].currentScale = ofLerp(opponentHandPlayer->hand[i].currentScale, opponentHandPlayer->hand[i].targetScale, 0.25f);
					opponentHandPlayer->hand[i].currentPos = opponentHandPlayer->hand[i].currentPos.getInterpolated(opponentHandPlayer->hand[i].targetPos, 0.25f);
				}
			}
		}
	}

	if (isPlayerAnimating && animatingPlayerIndex >= 0 && animatingPlayerIndex < (int)players.size()) {
		glm::vec3 targetPos = animationPath[currentPathIndex];
		// Cap deltaTime at 0.016f (60fps) to prevent instant movement on high framerates
		float clampedDeltaTime = std::min(deltaTime, 0.016f);
		// Smooth animation: 0.05 second movement per tile (faster movement)
		float player_speed = clampedDeltaTime / 0.05f;

		// Calculate facing direction
		glm::vec3 direction = targetPos - playerVisualPos;
		if (glm::length(glm::vec2(direction.x, direction.z)) > 0.01f) {
			playerFacingAngle = glm::degrees(atan2(direction.x, direction.z)) + 180.0f;
			players[animatingPlayerIndex].facingAngle = playerFacingAngle;
		}

		playerVisualPos = glm::mix(playerVisualPos, targetPos, player_speed);

		// Check if unit arrived at the center of the tile
		if (glm::distance(playerVisualPos, targetPos) < 0.05f) {
			playerVisualPos = targetPos; // Snap to exact position

			// --- KEY PICKUP LOGIC START ---
			// Only process key pickup on host - clients wait for PKT_KEY_PICKUP packet
			if (!isMultiplayer || isHost()) {
				// Use non-flipped world-to-grid conversion for key pickup
				int cx = (int)std::round((playerVisualPos.x - TILE_SIZE / 2.0f) / TILE_SIZE + BOARD_WIDTH / 2.0f);
				int cy = (int)std::round((playerVisualPos.z - TILE_SIZE / 2.0f) / TILE_SIZE + BOARD_HEIGHT / 2.0f);
				cx = std::clamp(cx, 0, BOARD_WIDTH - 1);
				cy = std::clamp(cy, 0, BOARD_HEIGHT - 1);

				for (int k = 0; k < (int)floatingKeyInstances.size(); ++k) {
					if (floatingKeyInstances[k].pos.x == cx && floatingKeyInstances[k].pos.y == cy) {
						// KEY FOUND
						int keySet = floatingKeyInstances[k].set;

						// Remove the key immediately
						floatingKeyInstances.erase(floatingKeyInstances.begin() + k);

						// Map Key Color to Card Class
						// Set 3 (Bronze) = Class 1
						// Set 2 (Silver) = Class 2
						// Set 1 (Gold)   = Class 3
						int classToDraft = 1;
						if (keySet == 3)
							classToDraft = 1;
						else if (keySet == 2)
							classToDraft = 2;
						else if (keySet == 1)
							classToDraft = 3;

						// Identify Owner (If minion steps on key, Summoner gets the card)
						Player & mover = players[animatingPlayerIndex];
						int ownerID = mover.isMinion ? mover.ownerID : mover.playerID;

						int ownerIndex = -1;
						for (int p = 0; p < (int)players.size(); ++p) {
							if (players[p].playerID == ownerID && !players[p].isMinion) {
								ownerIndex = p;
								break;
							}
						}

						if (ownerIndex != -1) {
							// HOST: Send key pickup packet to clients
							if (isHost()) {
								KeyPickupPacket kpkt = {};
								kpkt.type = PKT_KEY_PICKUP;
								kpkt.playerID = myLocalPlayerID;
								kpkt.playerIndex = ownerIndex;
								kpkt.classTier = classToDraft;
								kpkt.keyX = cx;
								kpkt.keyY = cy;
								steamManager.sendPacket(&kpkt, sizeof(kpkt));
								ofLogNotice("Network") << "Host sent KeyPickup: player=" << ownerIndex << " class=" << classToDraft;
							}

							// Setup In-Game Draft State
							isInGameDraft = true;
							draftPlayerIndex = ownerIndex;
							generateDraftOptions(classToDraft);
							draftPicksRemaining = 1; // Keys always give 1 pick
							selectedDraftIndices.clear(); // Reset UI selection
							currentState = STATE_DRAFTING;

							spawnFloatingText(gridToWorld(cx, cy), "Key Found!", ofColor::gold);
							ofLogNotice("Key") << "Player " << ownerID << " picked up key (Class " << classToDraft << ")";
							return; // Stop update to freeze game/animation until draft is done
						}
						break;
					}
				}
			} // End host-only key pickup logic
			// --- KEY PICKUP LOGIC END ---

			currentPathIndex++;

			// Play Footstep Sound
			if (currentPathIndex < animationPath.size() && !footstepSounds.empty()) {
				std::uniform_int_distribution<int> footIdx(0, (int)footstepSounds.size() - 1);
				int idx = footIdx(visualRNG);
				std::uniform_real_distribution<float> footSpeed(0.9f, 1.1f);
				footstepSounds[idx].setSpeed(footSpeed(visualRNG));
				footstepSounds[idx].play();
			}

			if (currentPathIndex >= static_cast<int>(animationPath.size())) {
				isPlayerAnimating = false;
				animatingPlayerIndex = -1;
				// Reset visual position to the current player so we don't display the wrong unit
				if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
					playerVisualPos = gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y);
				}
			}
		}
	}

	const float cardDisplayDuration = 2.5f;
	while (!activeCardDisplays.empty() && (ofGetElapsedTimef() - activeCardDisplays.front().startTime > cardDisplayDuration)) {
		activeCardDisplays.erase(activeCardDisplays.begin());
	}

	// --- IMMEDIATE DEATH / NO-CARDS CHECK ---
	// Remove units instantly if they have 0 HP or no cards anywhere (deck+discard+hand)
	std::vector<int> removeIndices;
	for (int i = 0; i < (int)players.size(); ++i) {
		bool noCards = players[i].deck.empty() && players[i].discardPile.empty() && players[i].hand.empty();
		if (players[i].health <= 0 || noCards) {
			// --- Faerie Resurrection Mechanic ---
			Player & dying = players[i];
			if (!dying.isFaerie && dying.x >= 0 && dying.y >= 0) {
				// Check for adjacent faerie
				bool resurrected = false;
				for (int dx = -1; dx <= 1 && !resurrected; ++dx) {
					for (int dy = -1; dy <= 1 && !resurrected; ++dy) {
						if ((dx != 0 || dy != 0) && abs(dx) + abs(dy) == 1) { // orthogonal only
							int nx = dying.x + dx, ny = dying.y + dy;
							if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
								for (auto & p : players) {
									if (p.isFaerie && p.x == nx && p.y == ny && p.health > 0) {
										// Roll 1d4, resurrect at 25% * roll * maxHealth (only host in multiplayer)
										if (!isClient()) {
											std::uniform_int_distribution<int> d4dist(1, 4);
											int roll = d4dist(gameplayRNG); // 1-4
											int hp = (int)std::floor(dying.maxHealth * 0.25f * roll);
											if (hp < 1) hp = 1;
											dying.health = hp;
											dying.onFire = false;
											dying.isPoisoned = false;
											dying.poisonReduction = 0;
											dying.isParalyzed = false;
											dying.paralysisHeadsCount = 0;
											dying.sleepTurnsRemaining = 0;
											dying.ward = 0;
											dying.block = 0;
											dying.fortification = 0;
											dying.barrier = 0;
											dying.holyBlock = 0;
											dying.luck = 0;
											dying.isReplicatePending = false;
											dying.nextTurnAPBonus = 0;
											dying.shocksPlayedThisTurn = 0;
											dying.flurryOfFistsActive = false;
											dying.isParalyzed = false;
											dying.isPoisoned = false;
											dying.poisonReduction = 0;
											dying.nextAttackAddPoison = false;
											dying.nextTurnD10AP = false;
											dying.nextTurnExtraDraw = false;
											dying.nextTurnBonusDiceFromMinions = false;
											dying.strengthenElementsTurnsRemaining = 0;
											dying.sleepTurnsRemaining = 0;
											dying.inTortoiseForm = false;
											dying.tortoiseDamageTaken = 0;
											dying.pendingTortoiseDamage = false;
											dying.pendingTortoiseDamageValue = 3;
											dying.inGhostForm = false;
											dying.ghostDamageTaken = 0;
											dying.cardsPlayedThisTurn.clear();
											dying.playedCardsPile.clear();
											dying.summonedOnTurnCycle = globalTurnCounter;
											spawnFloatingText(gridToWorld(dying.x, dying.y), "Faerie Resurrection!", ofColor::aqua);
											ofLogNotice("Faerie") << "Unit " << dying.playerID << " resurrected by faerie at " << nx << "," << ny << " for " << hp << " HP.";
											resurrected = true;
										}
									}
								}
							}
						}
					}
				}
				if (resurrected) continue; // skip normal death logic
			}
			removeIndices.push_back(i);
		}
	}

	if (!removeIndices.empty()) {
		std::sort(removeIndices.begin(), removeIndices.end(), std::greater<int>());
		for (int idx : removeIndices) {
			if (idx < 0 || idx >= (int)players.size()) continue;

			DeathMarker death;
			death.x = players[idx].x;
			death.y = players[idx].y;
			death.turnDied = globalTurnCounter;
			death.deck = players[idx].deck;
			graveyard.push_back(death);

			if (players[idx].x >= 0 && players[idx].x < BOARD_WIDTH && players[idx].y >= 0 && players[idx].y < BOARD_HEIGHT) {
				board[players[idx].x][players[idx].y].hasPlayer = false;
			}

			// Update active dice associations
			for (auto & r : activeDiceRolls) {
				if (r.associatedUnit == idx)
					r.associatedUnit = -1;
				else if (r.associatedUnit > idx)
					r.associatedUnit -= 1;
			}

			// Remove or adjust earthquake unit entries
			for (auto it = earthquakeUnits.begin(); it != earthquakeUnits.end();) {
				if (it->playerIndex == idx) {
					it = earthquakeUnits.erase(it);
				} else {
					if (it->playerIndex > idx) it->playerIndex -= 1;
					++it;
				}
			}

			players.erase(players.begin() + idx);

			if (players.empty()) {
				currentPlayerIndex = -1;
			} else {
				if (currentPlayerIndex == idx) {
					currentPlayerIndex = std::min<int>(idx, (int)players.size() - 1);
				} else if (currentPlayerIndex > idx) {
					currentPlayerIndex -= 1;
				}
			}
		}

		invalidateTargetCache();
	}

	if (hasUnlimitedAP) currentAP = 99;
}
//----------------------------------------------------
void ofApp::buildLevelMesh() {
	levelMesh.clear();
	levelMesh.setMode(OF_PRIMITIVE_TRIANGLES);
	levelMeshDark.clear();
	levelMeshDark.setMode(OF_PRIMITIVE_TRIANGLES);

	// 1. Settings
	float size = TILE_SIZE;
	float half = size / 2.0f;

	// --- CHANGE IS HERE ---
	float height = TILE_SIZE * 0.5f; // 0.5f = Half Height. Try 0.3f for low walls, 0.8f for tall.
	// ----------------------

	// 2. Helpers to add top or side faces into a specific mesh
	auto addTopTo = [&](ofMesh & meshTarget, float x, float y, float z) {
		int idx = meshTarget.getNumVertices();
		glm::vec3 offset(x, y, z);
		glm::vec3 p1(-half, height, -half);
		glm::vec3 p2(half, height, -half);
		glm::vec3 p3(half, height, half);
		glm::vec3 p4(-half, height, half);
		glm::vec2 t00(0, 0), t10(1, 0), t11(1, 1), t01(0, 1);
		meshTarget.addVertex(p1 + offset);
		meshTarget.addTexCoord(t00);
		meshTarget.addNormal({ 0, 1, 0 });
		meshTarget.addVertex(p2 + offset);
		meshTarget.addTexCoord(t10);
		meshTarget.addNormal({ 0, 1, 0 });
		meshTarget.addVertex(p3 + offset);
		meshTarget.addTexCoord(t11);
		meshTarget.addNormal({ 0, 1, 0 });
		meshTarget.addVertex(p4 + offset);
		meshTarget.addTexCoord(t01);
		meshTarget.addNormal({ 0, 1, 0 });
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 1);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx + 3);
	};

	auto addSidesTo = [&](ofMesh & meshTarget, float x, float y, float z) {
		int idx = meshTarget.getNumVertices();
		glm::vec3 offset(x, y, z);
		glm::vec3 p1(-half, height, -half);
		glm::vec3 p2(half, height, -half);
		glm::vec3 p3(half, height, half);
		glm::vec3 p4(-half, height, half);
		glm::vec3 p5(-half, 0, -half);
		glm::vec3 p6(half, 0, -half);
		glm::vec3 p7(half, 0, half);
		glm::vec3 p8(-half, 0, half);
		glm::vec2 t00(0, 0), t10(1, 0), t11(1, 1), t01(0, 1);

		// NORTH
		meshTarget.addVertex(p2 + offset);
		meshTarget.addTexCoord(t00);
		meshTarget.addNormal({ 0, 0, -1 });
		meshTarget.addVertex(p1 + offset);
		meshTarget.addTexCoord(t10);
		meshTarget.addNormal({ 0, 0, -1 });
		meshTarget.addVertex(p5 + offset);
		meshTarget.addTexCoord(t11);
		meshTarget.addNormal({ 0, 0, -1 });
		meshTarget.addVertex(p6 + offset);
		meshTarget.addTexCoord(t01);
		meshTarget.addNormal({ 0, 0, -1 });
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 1);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx + 3);
		idx += 4;

		// SOUTH
		meshTarget.addVertex(p4 + offset);
		meshTarget.addTexCoord(t00);
		meshTarget.addNormal({ 0, 0, 1 });
		meshTarget.addVertex(p3 + offset);
		meshTarget.addTexCoord(t10);
		meshTarget.addNormal({ 0, 0, 1 });
		meshTarget.addVertex(p7 + offset);
		meshTarget.addTexCoord(t11);
		meshTarget.addNormal({ 0, 0, 1 });
		meshTarget.addVertex(p8 + offset);
		meshTarget.addTexCoord(t01);
		meshTarget.addNormal({ 0, 0, 1 });
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 1);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx + 3);
		idx += 4;

		// EAST
		meshTarget.addVertex(p3 + offset);
		meshTarget.addTexCoord(t00);
		meshTarget.addNormal({ 1, 0, 0 });
		meshTarget.addVertex(p2 + offset);
		meshTarget.addTexCoord(t10);
		meshTarget.addNormal({ 1, 0, 0 });
		meshTarget.addVertex(p6 + offset);
		meshTarget.addTexCoord(t11);
		meshTarget.addNormal({ 1, 0, 0 });
		meshTarget.addVertex(p7 + offset);
		meshTarget.addTexCoord(t01);
		meshTarget.addNormal({ 1, 0, 0 });
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 1);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx + 3);
		idx += 4;

		// WEST
		meshTarget.addVertex(p1 + offset);
		meshTarget.addTexCoord(t00);
		meshTarget.addNormal({ -1, 0, 0 });
		meshTarget.addVertex(p4 + offset);
		meshTarget.addTexCoord(t10);
		meshTarget.addNormal({ -1, 0, 0 });
		meshTarget.addVertex(p8 + offset);
		meshTarget.addTexCoord(t11);
		meshTarget.addNormal({ -1, 0, 0 });
		meshTarget.addVertex(p5 + offset);
		meshTarget.addTexCoord(t01);
		meshTarget.addNormal({ -1, 0, 0 });
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 1);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx);
		meshTarget.addIndex(idx + 2);
		meshTarget.addIndex(idx + 3);
	};

	// 3. Loop through board and add cubes to appropriate mesh (dark when north wall within 2 tiles)
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			if (board[x][y].hasWall) {
				glm::vec3 pos = gridToWorld(x, y);

				// Always add sides into the dark mesh
				addSidesTo(levelMeshDark, pos.x, 0, pos.z);

				// Top face: dark only if an adjacent wall exists immediately to the north
				bool northAdjacent = false;
				int ny = y - 1;
				if (ny >= 0 && ny < BOARD_HEIGHT) northAdjacent = board[x][ny].hasWall;

				if (northAdjacent)
					addTopTo(levelMeshDark, pos.x, 0, pos.z);
				else
					addTopTo(levelMesh, pos.x, 0, pos.z);
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
	// If we detected a desync, display a message and abort gameplay rendering
	if (currentState == STATE_DESYNC) {
		ofPushStyle();
		ofSetColor(255, 30, 30);

		titleFont.drawString("DESYNC DETECTED", ofGetWidth() / 2.0f - 240, ofGetHeight() / 2.0f - 40);
		uiFont.drawString(desyncMessage, ofGetWidth() / 2.0f - 360, ofGetHeight() / 2.0f + 8);
		ofPopStyle();
		return;
	}

	// Render the 3D world (and 3D highlights) into an offscreen buffer so we can post-process it
	// without affecting the 2D UI.
	auto renderWorld3D = [&]() {
		// --- SETUP ---
		ofEnableDepthTest();
		ofSetColor(255);
		ofEnableLighting();

		ofCamera & activeCam = getActiveCamera();
		activeCam.begin();

		// --- LIGHTING ---
		uiLight.disable();
		keyLight.enable();
		rimLight.enable();
		headlight.enable();
		headlight.setPosition(activeCam.getPosition());

		// ===================================================================
		//  PASS 1: DRAW ALL OPAQUE OBJECTS (Models & Dice)
		// ===================================================================

		// --- OPAQUE GEOMETRY (Floor & Walls) ---
		for (size_t i = 0; i < floorMeshes.size(); i++) {
			if (i < floorTextures.size()) {
				floorTextures[i].bind();
				floorMeshes[i].draw();
				floorTextures[i].unbind();
			}
		}

		if (levelMesh.getNumVertices() > 0) {
			wallTexture.bind();
			levelMesh.draw();
			wallTexture.unbind();
		}
		if (levelMeshDark.getNumVertices() > 0) {
			wallDarkTexture.bind();
			levelMeshDark.draw();
			wallDarkTexture.unbind();
		}

		// --- DRAW FLOATING KEYS AS 3D VERTICAL BILLBOARDS ---
		// Draw after walls so depth buffer contains wall depths and keys are
		// correctly occluded when behind walls.
		if (!keyAnimSequence.empty() && !floatingKeyInstances.empty()) {
			int seqIdx = keyAnimSequence[keyAnimSeqPos];
			for (const auto & inst : floatingKeyInstances) {
				const std::vector<ofTexture> * setTex = nullptr;
				if (inst.set == 1)
					setTex = &keyTextures;
				else if (inst.set == 2)
					setTex = &keyTexturesSilver;
				else if (inst.set == 3)
					setTex = &keyTexturesBronze;
				if (!setTex || setTex->empty()) continue;
				if (seqIdx < 0 || seqIdx >= (int)setTex->size()) continue;
				glm::vec3 worldPos = gridToWorld(inst.pos.x, inst.pos.y);
				// Place keys closer to the floor so they sit visually nearer ground tiles
				glm::vec3 pos = worldPos + glm::vec3(0, 0.75f, 0);

				float texW = (float)(*setTex)[seqIdx].getWidth();
				float texH = (float)(*setTex)[seqIdx].getHeight();
				float aspect = (texH > 0.0f) ? (texW / texH) : 1.0f;

				// Apply configurable render scale to key world height (lowered to sit closer to floor)
				float heightWorld = TILE_SIZE * keyRenderScale;
				float widthWorld = heightWorld * aspect;
				float halfW = widthWorld * 0.5f;
				float halfH = heightWorld * 0.5f;

				glm::vec3 right = glm::vec3(1, 0, 0);

				// Use active camera so keys face the local player
				ofCamera & activeCam = getActiveCamera();
				ofVec3f camP = activeCam.getPosition();
				glm::vec3 camPos(camP.x, camP.y, camP.z);
				glm::vec3 forward = camPos - pos;
				float forwardLenXZ = sqrtf(forward.x * forward.x + forward.z * forward.z);
				if (forwardLenXZ < 1e-4f) forwardLenXZ = 1e-4f;
				float pitch = atan2f(forward.y, forwardLenXZ);

				float maxTilt = glm::radians(60.0f);
				float tilt = std::clamp(pitch * 0.8f, -maxTilt, maxTilt);
				glm::quat tiltQ = glm::angleAxis(-tilt, right);

				glm::vec3 upVec(0, 1, 0);
				glm::vec3 upTilt = tiltQ * upVec;
				glm::vec3 rightTilt = tiltQ * right;

				glm::vec3 p0 = pos - rightTilt * halfW - upTilt * halfH;
				glm::vec3 p1 = pos + rightTilt * halfW - upTilt * halfH;
				glm::vec3 p2 = pos + rightTilt * halfW + upTilt * halfH;
				glm::vec3 p3 = pos - rightTilt * halfW + upTilt * halfH;

				// Flip texture horizontally for camera 2 so keys face properly for second player
				bool flipTexture = shouldFlipCamera();
				float u0 = flipTexture ? 1.0f : 0.0f;
				float u1 = flipTexture ? 0.0f : 1.0f;

				ofMesh quad;
				quad.setMode(OF_PRIMITIVE_TRIANGLES);
				quad.addVertex(p0);
				quad.addTexCoord(glm::vec2(u0, 1));
				quad.addVertex(p1);
				quad.addTexCoord(glm::vec2(u1, 1));
				quad.addVertex(p2);
				quad.addTexCoord(glm::vec2(u1, 0));
				quad.addVertex(p3);
				quad.addTexCoord(glm::vec2(u0, 0));

				quad.addIndex(0);
				quad.addIndex(1);
				quad.addIndex(2);
				quad.addIndex(0);
				quad.addIndex(2);
				quad.addIndex(3);

				// Alpha-test: don't write depth for transparent pixels so occluders can show
				glEnable(GL_ALPHA_TEST);
				glAlphaFunc(GL_GREATER, 0.05f);

				// Draw the current frame only (no cross-fade). Draw after walls so
				// walls in front occlude keys; keep depth test but disable depth writes
				int setSize = (int)setTex->size();
				int curIdx = keyAnimSequence[keyAnimSeqPos] % setSize;

				// Ensure depth testing is enabled so keys are tested against wall depth
				glEnable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);
				ofDisableLighting();
				ofSetColor(255, 200);
				(*setTex)[curIdx].bind();
				quad.draw();
				(*setTex)[curIdx].unbind();
				ofEnableLighting();
				glDepthMask(GL_TRUE);
				glDisable(GL_ALPHA_TEST);
			}
		}

		// Earthquake arrows are now drawn above each unit's head within the transparent effects pass
		// --- OPAQUE DYNAMIC OBJECTS (Players) ---

		ofSetColor(255);
		for (const auto & player : players) {
			// 1. Determine Position
			glm::vec3 pos;
			bool foundEq = false;
			if (isEarthquakeActive) {
				for (const auto & eq : earthquakeUnits) {
					if (eq.playerIndex == &player - &players[0]) {
						pos = eq.visualPos;
						foundEq = true;
						break;
					}
				}
			}
			if (!foundEq) {
				if (isPlayerAnimating && animatingPlayerIndex >= 0 && &player == &players[animatingPlayerIndex]) {
					pos = playerVisualPos;
				} else if (currentPlayerIndex >= 0 && player.playerID == players[currentPlayerIndex].playerID && (!isPlayerAnimating || animatingPlayerIndex == currentPlayerIndex)) {
					pos = playerVisualPos;
				} else {
					pos = gridToWorld(player.x, player.y);
				}
			}

			ofPushMatrix();

			// --- 2. GHOST FORM (Overrides everything) ---
			if (player.inGhostForm) {
				ofTranslate(pos.x, 0.1f, pos.z);

				// Base floating height
				float floatY = 1.0f + sin(ofGetElapsedTimef() * 2.0f) * 0.2f;

				// CHECK IF IN WALL:
				if (player.x >= 0 && player.x < BOARD_WIDTH && player.y >= 0 && player.y < BOARD_HEIGHT) {
					if (board[player.x][player.y].hasWall) {
						floatY += 1.5f;
					}
				}

				ofTranslate(0, floatY, 0);

				// FLIP FIX: Changed +270 to +90 to flip it 180 degrees
				ofRotateYDeg(player.facingAngle + 90);

				ofRotateXDeg(0);

				// Enable transparency
				ofEnableBlendMode(OF_BLENDMODE_ALPHA);

				ofSetColor(255, 255, 255, 150);

				// Bind Texture
				if (ghostBaseTex.isAllocated()) ghostBaseTex.bind();
				ghostModel.drawFaces();
				if (ghostBaseTex.isAllocated()) ghostBaseTex.unbind();

				ofDisableBlendMode();
				ofSetColor(255);
			}

			// --- 3. STANDARD MODELS ---
			else {
				float unitFacingAngle = player.facingAngle;
				if (player.isSkeleton) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 2.0f, 0);
					skeletonTexture.bind();
					skeletonModel.drawFaces();
					skeletonTexture.unbind();
				} else if (player.isGolem) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 3.0f, 0);
					ofRotateXDeg(180);
					ofRotateYDeg(90);
					if (player.minionTexture) player.minionTexture->bind();
					golemModel.drawFaces();
					if (player.minionTexture) player.minionTexture->unbind();
				} else if (player.isWolf) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 0.4f, 0);
					ofScale(0.018f, 0.018f, 0.018f);
					for (unsigned int i = 6; i < wolfModel.getMeshCount(); i++) {
						ofTexture * tex = (i == 6 || i == 7) ? &wolfBodyTex : &wolfFaceTex;
						if (tex->isAllocated()) tex->bind();
						wolfModel.getMeshHelper(i).cachedMesh.drawFaces();
						if (tex->isAllocated()) tex->unbind();
					}
					// Draw fur layers (meshes 0..5) using fur texture with alpha blending
					glDepthMask(GL_FALSE);
					ofEnableAlphaBlending();
					if (wolfFurTex.isAllocated()) wolfFurTex.bind();
					for (unsigned int i = 0; i <= 5 && i < wolfModel.getMeshCount(); i++) {
						wolfModel.getMeshHelper(i).cachedMesh.drawFaces();
					}
					if (wolfFurTex.isAllocated()) wolfFurTex.unbind();
					ofDisableAlphaBlending();
					glDepthMask(GL_TRUE);
				} else if (player.isHellhound) {
					ofTranslate(pos.x, 0.1f, pos.z);
					// Face movement direction like other minions
					// Hellhound's model forward is reversed; add 180 degrees
					ofRotateYDeg(unitFacingAngle + 180.0f);
					// Slight vertical offset so paws/mesh clear the floor
					ofTranslate(0, 0.6f, 0);
					hellhoundModel.drawFaces();
				} else if (player.isDemon) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 3.5f, 0);
					ofRotateYDeg(90);
					demonModel.drawFaces();
				} else if (player.inTortoiseForm) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 0.5f, 0);
					ofRotateXDeg(180);
					if (tortoiseTexture.isAllocated()) tortoiseTexture.bind();
					tortoiseModel.drawFaces();
					if (tortoiseTexture.isAllocated()) tortoiseTexture.unbind();
				} else if (player.isKobold) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 0.6f, 0);
					koboldModel.drawFaces();
				}
				// --- KOBOLD KING ---
				else if (player.isKoboldKing) {
					ofTranslate(pos.x, 0.1f, pos.z);

					// 1. Apply Game Facing Logic
					ofRotateYDeg(unitFacingAngle);

					// 2. Apply Model Correction (West -> North)
					ofRotateYDeg(-90);

					// Raise the king so its base doesn't clip through the floor
					// Use TILE_SIZE so the offset scales with board size
					// Increased to 0.6 to ensure feet clear the board
					ofTranslate(0, TILE_SIZE * 0.6f, 0);

					// Ensure white color so texture isn't tinted
					ofSetColor(255);

					bool texBound = false;
					if (koboldKingTexture.isAllocated()) {
						koboldKingTexture.bind();
						texBound = true;
					}

					glDisable(GL_CULL_FACE);
					koboldKingModel.drawFaces();
					glEnable(GL_CULL_FACE);

					if (texBound) {
						koboldKingTexture.unbind();
					}
				}
				// --- FAERIE ---
				else if (player.isFaerie) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					// Model is already rotated in setup
					ofTranslate(0, 1.0f, 0); // Adjust vertical offset as needed
					if (faerieTexture.isAllocated()) faerieTexture.bind();
					faerieModel.drawFaces();
					if (faerieTexture.isAllocated()) faerieTexture.unbind();
				}
				// --- WALL UNIT ---
				else if (player.isWallUnit) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 0.0f, 0); // Adjust based on model pivot
					// Standard FBX upright correction (if needed)
					// ofRotateXDeg(0);

					// If we have an external wall unit texture, bind it; otherwise let the model's own textures render (GLB)
					if (wallUnitTexture.isAllocated()) {
						wallUnitTexture.bind();
						// Raise model so it sits on the ground and not intersect the floor
						ofTranslate(0, TILE_SIZE * 0.14f, 0);
						// Use flat shading while drawing the wall unit to avoid smooth shading
						glShadeModel(GL_FLAT);
						wallUnitModel.drawFaces();
						glShadeModel(GL_SMOOTH);
						wallUnitTexture.unbind();
					} else {
						// No external texture: draw model with its embedded textures (GLB) or material colors
						ofTranslate(0, TILE_SIZE * 0.14f, 0);
						// Draw GLB with flat shading to turn off smooth shading
						glShadeModel(GL_FLAT);
						wallUnitModel.drawFaces();
						glShadeModel(GL_SMOOTH);
					}

					// If Magic Wall Unit, apply a pulsing purple tint visual that matches tile sheen
					if (player.isMagicWallUnit) {
						// Pulse alpha in the same way the tile sheen does so the unit also glows
						float pulseAlpha = 90.0f + 60.0f * sin(ofGetElapsedTimef() * 2.0f + player.x * 0.7f + player.y * 0.5f);
						int alpha = static_cast<int>(ofClamp(pulseAlpha, 0.0f, 255.0f));
						ofEnableBlendMode(OF_BLENDMODE_ADD);
						ofSetColor(148, 0, 211, alpha);
						// Avoid z-fighting by offsetting polygons slightly
						glEnable(GL_POLYGON_OFFSET_FILL);
						glPolygonOffset(-1.0f, -1.0f);
						ofPushMatrix();
						// Draw the whole model again in purple so the glow follows model curves exactly
						wallUnitModel.drawFaces();
						ofPopMatrix();
						glDisable(GL_POLYGON_OFFSET_FILL);
						ofSetColor(255);
						ofDisableBlendMode();
					}
				}
				// --- ASSISTANT ---
				else if (player.isAssistant) {
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 1.7f, 0);
					ofScale(1.0f, 1.0f, 1.0f); // Adjust based on model size

					// Optional: Tint blue/purple to look magical
					ofSetColor(200, 200, 255);
					assistantModel.drawFaces();
					ofSetColor(255);
				} else {
					// Default Player
					ofTranslate(pos.x, 0.1f, pos.z);
					ofRotateYDeg(unitFacingAngle);
					ofTranslate(0, 2.0f, 0);
					if (playerTexture.isAllocated()) playerTexture.bind();
					playerModel.drawFaces();
					if (playerTexture.isAllocated()) playerTexture.unbind();
				}
			}
			ofPopMatrix();
		}

		// --- HOVER GLOW RENDERING ---
		// Draw white glow for local player's hover
		if (localHoverType == HOVER_UNIT && localHoverGridX >= 0 && localHoverGridX < BOARD_WIDTH && localHoverGridY >= 0 && localHoverGridY < BOARD_HEIGHT) {
			drawTileGlow(localHoverGridX, localHoverGridY, ofColor(255, 255, 255, 200), 4.0f);
		}

		// Draw red glow for opponent's hover
		if (opponentHoverType == HOVER_UNIT && opponentHoverGridX >= 0 && opponentHoverGridX < BOARD_WIDTH && opponentHoverGridY >= 0 && opponentHoverGridY < BOARD_HEIGHT) {
			drawTileGlow(opponentHoverGridX, opponentHoverGridY, ofColor(255, 0, 0, 200), 4.0f);
		}

		// --- DICE RENDERING ---
		diceMaterial.begin();

		// Helper to position dice
		auto setDiceTransform = [&](int i, DiceRoll & roll) {
			ofPushMatrix();

			bool placed = false;
			// EARTHQUAKE OVERRIDE: If this roll belongs to a unit (associatedUnit), place above that unit's visual position
			if (roll.associatedUnit >= 0) {
				for (const auto & u : earthquakeUnits) {
					if (u.playerIndex == roll.associatedUnit) {
						// Use visualPos so dice follow moving/bouncing units
						glm::vec3 unitPos = u.visualPos;

						// Default raise
						float raise = 3.0f;

						// Raise significantly higher for earthquake rolls to clear the head/text
						if (roll.purpose == PURPOSE_EARTHQUAKE_DISTANCE || roll.purpose == PURPOSE_EARTHQUAKE_DAMAGE) {
							// Determine head height dynamically based on unit type to ensure clearance
							Player & p = players[u.playerIndex];
							float headOffset = 4.0f;
							if (p.isGolem || p.isDemon)
								headOffset = 6.5f;
							else if (p.isWolf || p.isHellhound)
								headOffset = 3.5f;

							// Dice sits above head
							raise = headOffset + 2.5f;
						}

						ofTranslate(unitPos.x, unitPos.y + raise, unitPos.z);
						placed = true;
						break;
					}
				}
			}

			// Default placement: Grid layout for massive amounts of dice
			if (!placed) {
				int rowLength = 10; // max dice per row
				// Use TILE_SIZE-based spacing so dice scale with board scale
				float spacing = TILE_SIZE * 1.2f;

				int row = i / rowLength;
				int col = i % rowLength;

				// Special-case: Initiative roll shows exactly two dice; place them
				// so the local player's die is always on the left.
				if (currentState == STATE_INITIATIVE_ROLL && (int)activeDiceRolls.size() >= 2) {
					glm::vec3 leftPos(-6.0f, 7.0f, 0.0f);
					glm::vec3 rightPos(6.0f, 7.0f, 0.0f);

					// i==0 corresponds to player 0, i==1 corresponds to player 1
					// Keep P0 on the left in world space so each player sees their die on the left
					bool player0OnLeft = true;
					if (i == 0) {
						glm::vec3 pos = player0OnLeft ? leftPos : rightPos;
						ofTranslate(pos.x, pos.y, pos.z);
					} else if (i == 1) {
						glm::vec3 pos = player0OnLeft ? rightPos : leftPos;
						ofTranslate(pos.x, pos.y, pos.z);
					} else {
						// Fallback for extra dice: continue with normal grid
						int totalDice = (int)activeDiceRolls.size();
						int itemsInThisRow = std::min(rowLength, std::max(0, totalDice - row * rowLength));
						float totalW = itemsInThisRow * spacing;
						float startX = -(totalW / 2.0f) + (spacing / 2.0f);
						float offsetX = startX + (col * spacing);
						float offsetZ = (row * spacing);
						float offsetY = 4.5f;
						ofTranslate(offsetX, offsetY, offsetZ);
					}
				} else {
					// Calculate how many items are in this particular row so
					// we can center the row based on the actual dice count
					int totalDice = (int)activeDiceRolls.size();
					int itemsInThisRow = std::min(rowLength, std::max(0, totalDice - row * rowLength));

					float totalW = itemsInThisRow * spacing;
					// Start so that the row is centered around X=0. Add half-spacing
					// so a single die sits exactly at X=0.
					float startX = -(totalW / 2.0f) + (spacing / 2.0f);

					float offsetX = startX + (col * spacing);
					float offsetZ = (row * spacing); // Stack rows in depth
					float offsetY = 4.5f;

					ofTranslate(offsetX, offsetY, offsetZ);
				}
			}

			// Apply Rotation
			glm::quat finalDrawQuat;
			float t = (ofGetElapsedTimef() - roll.startTime);
			if (t < 1.0f) {
				float t_ease = 1.0f - pow(1.0f - t, 4.0f);
				float remainingSpin = (1.0f - t_ease) * 1080.0f; // Spin amount
				if (roll.sides == 4) remainingSpin *= 0.5f; // D4 spins less violently
				glm::quat spin = glm::angleAxis(glm::radians(remainingSpin), roll.rotationAxis);
				finalDrawQuat = spin * roll.finalQuat;
			} else {
				finalDrawQuat = roll.finalQuat;
			}
			ofMultMatrix(glm::toMat4(finalDrawQuat));
		};

		// 1. D4
		d4Texture.bind();
		for (int i = 0; i < activeDiceRolls.size(); i++) {
			if (activeDiceRolls[i].sides == 4) {
				setDiceTransform(i, activeDiceRolls[i]);
				ofScale(2.2f, 2.2f, 2.2f);
				d4Mesh.draw();
				ofPopMatrix();
			}
		}
		d4Texture.unbind();

		// 2. Coin
		coinFacesTexture.bind();
		for (int i = 0; i < activeDiceRolls.size(); i++) {
			if (activeDiceRolls[i].sides == 2) {
				setDiceTransform(i, activeDiceRolls[i]);
				coinMesh.draw();
				ofPopMatrix();
			}
		}
		coinFacesTexture.unbind();

		// 3. D6
		d6Texture.bind();
		for (int i = 0; i < activeDiceRolls.size(); i++) {
			if (activeDiceRolls[i].sides == 6) {
				setDiceTransform(i, activeDiceRolls[i]);
				ofScale(1.2f, 1.2f, 1.2f);
				d6Mesh.draw();
				ofPopMatrix();
			}
		}
		d6Texture.unbind();

		// 4. D10
		d10Texture.bind();
		for (int i = 0; i < activeDiceRolls.size(); i++) {
			if (activeDiceRolls[i].sides == 10) {
				setDiceTransform(i, activeDiceRolls[i]);
				ofScale(2.1f, 2.1f, 2.1f);
				d10Mesh.draw();
				ofPopMatrix();
			}
		}
		d10Texture.unbind();

		// 5. D20
		d20Texture.bind();
		for (int i = 0; i < activeDiceRolls.size(); i++) {
			if (activeDiceRolls[i].sides == 20) {
				setDiceTransform(i, activeDiceRolls[i]);
				ofScale(2.4f, 2.4f, 2.4f);
				d20Mesh.draw();
				ofPopMatrix();
			}
		}
		d20Texture.unbind();

		diceMaterial.end();

		// ===================================================================
		//  PASS 2: DRAW ALL TRANSPARENT EFFECTS
		//  (Disable depth writing to prevent artifacts)
		// ===================================================================
		glDepthMask(GL_FALSE);
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);

		for (const auto & player : players) {
			// 1. Determine Position
			glm::vec3 pos;
			bool foundEq = false;
			if (isEarthquakeActive) {
				int pIndex = (int)(&player - &players[0]);
				for (const auto & eq : earthquakeUnits) {
					if (eq.playerIndex == pIndex) {
						pos = eq.visualPos;
						foundEq = true;
						break;
					}
				}
			}

			if (!foundEq) {
				if (isPlayerAnimating && animatingPlayerIndex >= 0 && &player == &players[animatingPlayerIndex]) {
					pos = playerVisualPos;
				} else if (currentPlayerIndex >= 0 && player.playerID == players[currentPlayerIndex].playerID && (!isPlayerAnimating || animatingPlayerIndex == currentPlayerIndex)) {
					pos = playerVisualPos;
				} else {
					pos = gridToWorld(player.x, player.y);
				}
			}

			// 2. Determine Head Height for Status Effects (Used in this loop)
			float headHeight = 4.0f; // Default
			if (player.isGolem)
				headHeight = 5.5f;
			else if (player.isWolf)
				headHeight = 2.0f;
			else if (player.isHellhound)
				headHeight = 2.5f;
			else if (player.isDemon)
				headHeight = 6.0f;

			// 3. DRAW SHADOW
			ofPushMatrix();
			ofTranslate(pos.x, 0.02f, pos.z);
			ofRotateXDeg(90);
			float shadowSize = TILE_SIZE * 0.8f;
			if (player.isDemon) shadowSize *= 1.5f;
			shadowTexture.draw(-shadowSize / 2, -shadowSize / 2, shadowSize, shadowSize);
			ofPopMatrix();

			// --- REGENERATION (Tiny Pixel Heart) ---
			if (player.hasRegeneration) {
				ofPushMatrix();

				float bob = sin(ofGetElapsedTimef() * 1.5f) * 0.15f;
				ofTranslate(pos.x, headHeight + 1.4f + bob, pos.z);

				glm::vec3 camPos = cam.getPosition();
				float angle = atan2(camPos.x - pos.x, camPos.z - pos.z) * RAD_TO_DEG;
				ofRotateYDeg(angle);

				float pulse = 1.0f + 0.1f * sin(ofGetElapsedTimef() * 3.0f);
				float scale = 0.06f * pulse; // Slightly larger scale since grid is smaller
				ofScale(scale, -scale, scale);

				// Compact 5x5 Heart
				static const int grid[5][5] = {
					{ 0, 1, 0, 1, 0 }, // Lobes
					{ 1, 2, 1, 1, 1 }, // Upper body + Shine (2)
					{ 1, 1, 1, 1, 1 }, // Middle body
					{ 0, 1, 1, 1, 0 }, // Lower body
					{ 0, 0, 1, 0, 0 } // Tip
				};

				float pxSize = 1.0f;
				float startX = -(5 * pxSize) / 2.0f;
				float startY = -(5 * pxSize) / 2.0f;

				for (int r = 0; r < 5; r++) {
					for (int c = 0; c < 5; c++) {
						int val = grid[r][c];
						if (val != 0) {
							if (val == 2)
								ofSetColor(255, 200, 200, 255); // Highlight
							else
								ofSetColor(220, 20, 60, 240); // Red
							ofDrawRectangle(startX + c * pxSize, startY + r * pxSize, pxSize * 1.05f, pxSize * 1.05f);
						}
					}
				}
				ofPopMatrix();
			}

			// 4. DRAW FIRE
			if (player.onFire) {
				ofPushMatrix();
				ofTranslate(pos.x, 2.5f, pos.z);
				glm::vec3 camPos = cam.getPosition();
				float angle = atan2(camPos.x - pos.x, camPos.z - pos.z) * RAD_TO_DEG;
				ofRotateYDeg(angle);
				float time = ofGetElapsedTimef();
				int fireFrame = (int)(time * 10) % 4;
				float spriteSize = 4.0f;
				fireTexture.drawSubsection(-spriteSize / 2, -spriteSize / 2, spriteSize, spriteSize, fireFrame * 32, 0, 32, 32);
				ofPopMatrix();
			}

			// 5. DRAW SLEEP (Zs)
			if (player.sleepTurnsRemaining > 0) {
				ofPushMatrix();
				ofTranslate(pos.x, headHeight, pos.z);

				float time = ofGetElapsedTimef();
				float slowTime = time * 0.8f;
				for (int z = 0; z < 3; z++) {
					float offset = (z * 2.0f) + slowTime;
					float yFloat = fmod(offset, 1.5f);
					float alpha = 1.0f - (yFloat / 1.5f);
					ofPushMatrix();
					ofTranslate(sin(slowTime + z) * 0.2f, yFloat, 0);
					glm::vec3 camPos = cam.getPosition();
					float angle = atan2(camPos.x - pos.x, camPos.z - pos.z) * RAD_TO_DEG;
					ofRotateYDeg(angle);
					ofScale(0.02f, 0.02f, 0.02f);
					ofSetColor(0, 255, 255, alpha * 255);
					uiFont.drawString("z", 0, 0);
					ofPopMatrix();
				}
				ofPopMatrix();
			}

			// 6. DRAW PARALYSIS (Swirl)
			if (player.isParalyzed) {
				ofPushMatrix();
				ofTranslate(pos.x, headHeight - 0.5f, pos.z);
				ofPolyline swirl;
				float time = ofGetElapsedTimef();
				float swirlSpeed = time * 2.0f;
				for (int i = 0; i < 20; i++) {
					float t = i / 20.0f;
					float angle = (t * TWO_PI * 1.5f) + swirlSpeed;
					float radius = 0.3f;
					float height = t * 0.4f;
					swirl.addVertex(cos(angle) * radius, height, sin(angle) * radius);
				}
				ofSetColor(255, 255, 0);
				ofSetLineWidth(2);
				swirl.draw();
				ofSetLineWidth(1);
				ofPopMatrix();
			}

			// 7. DRAW POISON (Skull Icon)
			if (player.isPoisoned) {
				ofPushMatrix();
				ofTranslate(pos.x, headHeight + 0.3f, pos.z);
				glm::vec3 camPos = cam.getPosition();
				float angle = atan2(camPos.x - pos.x, camPos.z - pos.z) * RAD_TO_DEG;
				ofRotateYDeg(angle);

				// Draw a simple poison bottle icon using text
				float time = ofGetElapsedTimef();
				float pulse = 0.8f + 0.2f * sin(time * 3.0f);
				ofScale(0.015f * pulse, 0.015f * pulse, 0.015f * pulse);
				ofSetColor(0, 200, 0); // Green for poison
				uiFont.drawString("[X]", -20, 0); // Simple skull representation
				ofPopMatrix();
			}

			// 8. EARTHQUAKE DIRECTION ARROWS (above head, follow unit)
			{
				int pIndex = (int)(&player - &players[0]);
				if (isEarthquakeActive) {
					for (const auto & eq : earthquakeUnits) {
						if (eq.playerIndex == pIndex && (eq.originalDistance > 0 || eq.crashed)) {
							// Determine displayed remaining tiles: decrement once the step progress crosses halfway
							int displayedRemaining = 0;
							if (eq.crashed)
								displayedRemaining = 0;
							else {
								int base = std::max(0, eq.tilesToMove);
								if (isEarthquakeAnimatingStep && earthquakeT >= 0.5f) base = std::max(0, base - 1);
								displayedRemaining = base;
							}

							if (displayedRemaining > 0) {
								// Compute world-space forward direction for this unit
								glm::vec3 startWorld = gridToWorld(eq.startGrid.x, eq.startGrid.y);
								glm::vec3 nextWorld = gridToWorld(eq.startGrid.x + eq.direction.x, eq.startGrid.y + eq.direction.y);
								glm::vec3 dirWorld = nextWorld - startWorld;
								if (glm::length(dirWorld) > 0.0001f) dirWorld = glm::normalize(dirWorld);
								// Arrow spacing along direction (much smaller spacing so arrows are close together)
								float stepOffset = TILE_SIZE * 0.08f;
								// Bright yellow
								ofColor arrowCol = ofColor(255, 235, 59);

								// Draw arrows without lighting so they appear bright and unaffected by scene lights
								ofPushStyle();
								ofDisableLighting();
								for (int i = 0; i < displayedRemaining; ++i) {
									glm::vec3 arrowPos = pos + dirWorld * ((i + 1) * stepOffset);
									ofPushMatrix();
									// Position slightly above head
									ofTranslate(arrowPos.x, headHeight + 0.6f, arrowPos.z);
									// Lay flat on XZ
									ofRotateXDeg(90);
									// Rotate based on dirWorld to point correctly
									float dirAngle = 0.0f;
									if (eq.direction.x == 1) dirAngle = 90.0f;
									if (eq.direction.x == -1) dirAngle = 270.0f;
									if (eq.direction.y == 1) dirAngle = 180.0f;
									ofRotateZDeg(dirAngle);
									// Draw arrow
									ofSetColor(arrowCol);
									float w = 0.5f;
									float h = 0.35f;
									ofDrawTriangle(0, -h, -w / 2.0f, 0.0f, w / 2.0f, 0.0f);
									ofDrawRectangle(-w / 10.0f, 0.0f, w / 5.0f, h);
									ofPopMatrix();
								}
								ofPopStyle();
								ofEnableLighting();
							}
						}
					}
				}
			}
		}

		// Diable Lighting for Highlights
		ofDisableLighting();

		// --- DRAW TILE HIGHLIGHTS ---
		for (int x = 0; x < BOARD_WIDTH; x++) {
			for (int y = 0; y < BOARD_HEIGHT; y++) {
				glm::vec3 tileWorldPos = gridToWorld(x, y); // Use gridToWorld, not transformGridToWorld
				ofPushMatrix();
				ofTranslate(tileWorldPos.x, 0, tileWorldPos.z);

				// Calculate the top surface height for this tile
				float surfaceY = 0.06f; // Floor height
				if (board[x][y].hasWall) {
					surfaceY = (TILE_SIZE * 0.5f) + 0.06f; // Top of wall height
				}

				// 1. Draw Walls
				if (board[x][y].hasWall) {
					ofPushMatrix();
					ofTranslate(0, surfaceY - (TILE_SIZE * 0.4f), 0);
					// Choose dark texture based on camera perspective
					// Player 0 views from bottom-left (y increases away), Player 1 from top-right (y decreases away)
					// Check if there's a wall "behind" this one from the viewer's perspective
					bool wallBehind = false;
					int checkY = (myLocalPlayerID == 1) ? y + 1 : y - 1;
					if (checkY >= 0 && checkY < BOARD_HEIGHT) wallBehind = board[x][checkY].hasWall;
					ofTexture * tex = wallBehind ? &wallDarkTexture : &wallTexture;
					tex->bind();
					wallMesh.draw();
					tex->unbind();
					ofPopMatrix();

					// Magic Wall Sheen
					if (board[x][y].isMagicWall) {
						float sheenAlpha = 90 + 60 * sin(ofGetElapsedTimef() * 2.0f + x * 0.7f + y * 0.5f);
						ofFloatColor sheenColor(148.0f / 255.0f, 0.0f, 211.0f / 255.0f, sheenAlpha / 255.0f);
						float wallW = TILE_SIZE;
						float wallH = TILE_SIZE * 0.5f;
						float epsilon = 0.02f;
						float overlap = epsilon * 4.0f;

						ofTranslate(0, 0, 0); // Reset local for sheen
						ofPushMatrix();
						ofTranslate(0, wallH + epsilon, 0);
						ofRotateXDeg(90);
						ofSetColor(sheenColor);
						ofDrawRectangle(-(wallW + overlap) / 2, -(wallW + overlap) / 2, wallW + overlap, wallW + overlap);
						ofPopMatrix();

						for (int i = 0; i < 4; ++i) {
							ofPushMatrix();
							ofRotateYDeg(i * 90.0f);
							ofTranslate(0, wallH / 2, wallW / 2 + epsilon);
							ofSetColor(sheenColor);
							ofDrawRectangle(-(wallW + overlap) / 2, -(wallH + overlap) / 2, wallW + overlap, wallH + overlap);
							ofPopMatrix();
						}
					}
				}

				// 2. Draw Movement Highlight (White Joined Outlines)
				// We'll draw these after all tiles are processed, not per-tile

				// 3. Draw Target Previews - now handled by white outline system
				// (Red preview tiles are drawn as white outlines below)

				// 4. Draw Valid Targets (Green Separate Outlines) - These don't join
				if (board[x][y].isTargetable) {
					ofSetColor(ofColor::green, 220);
					ofNoFill();
					ofSetLineWidth(4);
					ofPushMatrix();
					ofTranslate(0, surfaceY + 0.02f, 0);
					ofRotateXDeg(90);
					ofDrawRectangle(-TILE_SIZE * 0.5f, -TILE_SIZE * 0.5f, TILE_SIZE, TILE_SIZE);
					ofPopMatrix();
					ofFill();
					ofSetLineWidth(1);
				}

				// 5. Active Player Selection Square
				if (!players.empty() && currentPlayerIndex >= 0) {
					int highlightX = players[currentPlayerIndex].x;
					int highlightY = players[currentPlayerIndex].y;

					// ... (Existing animation/earthquake check logic) ...
					if (isPlayerAnimating) {
						glm::vec2 g = worldToGrid(playerVisualPos);
						highlightX = (int)g.x;
						highlightY = (int)g.y;
					} else if (isEarthquakeActive) {
						for (const auto & eq : earthquakeUnits) {
							if (eq.playerIndex == currentPlayerIndex) {
								glm::vec2 g = worldToGrid(eq.visualPos);
								highlightX = (int)g.x;
								highlightY = (int)g.y;
								break;
							}
						}
					}

					if (x == highlightX && y == highlightY) {
						// Draw pulsing golden ring around current player's unit
						float pulseScale = 0.75f + 0.25f * sin(ofGetElapsedTimef() * 2.5f);
						ofPushMatrix();
						ofTranslate(0, surfaceY + 0.01f, 0);
						ofRotateXDeg(90);
						ofNoFill();
						ofSetLineWidth(3.5f);
						ofSetColor(255, 215, 0, 220); // Bright gold
						ofDrawCircle(0, 0, TILE_SIZE * (0.55f * pulseScale));
						// Inner accent ring
						ofSetLineWidth(1.5f);
						ofSetColor(255, 255, 150, 180); // Lighter gold
						ofDrawCircle(0, 0, TILE_SIZE * (0.40f * pulseScale));
						ofFill();
						ofPopMatrix();
					}
				}
				ofPopMatrix();
			}
		}

		// Draw white joined outlines for movement highlights and target previews (after all tiles processed)
		// Build a 2D array of which tiles should have white outlines
		bool highlightedTiles[BOARD_WIDTH][BOARD_HEIGHT];
		for (int x = 0; x < BOARD_WIDTH; x++) {
			for (int y = 0; y < BOARD_HEIGHT; y++) {
				// Include both movement highlights AND red preview tiles
				highlightedTiles[x][y] = board[x][y].isHighlighted || (board[x][y].isTargetPreview && !board[x][y].isTargetable);
			}
		}
		// Draw the white outlines (no pulse, solid white)
		ofColor whiteColor(255, 255, 255, 240);
		float avgSurfaceY = 0.05f; // Average surface height for flat tiles
		drawJoinedOutlines(highlightedTiles, whiteColor, avgSurfaceY);

		// 5. Draw Path Highlights (Green Circles) (FIXED HEIGHT)
		if ((playerAction == PIECE_SELECTED) && !hoverPath.empty()) {
			// Enable depth write so these don't overwrite outlines underneath
			glDepthMask(GL_TRUE);
			ofEnableDepthTest();
			for (size_t i = 1; i < hoverPath.size(); i++) {
				const auto & step = hoverPath[i];
				glm::vec3 pathWorldPos = gridToWorld(step.x, step.y); // Use gridToWorld, not transformGridToWorld

				// Calculate height for THIS specific step (slightly higher to sit above outlines)
				float pathY = 0.08f;
				if (board[(int)step.x][(int)step.y].hasWall) {
					pathY = (TILE_SIZE * 0.5f) + 0.08f;
				}

				ofSetColor(ofColor::green, 150);
				ofPushMatrix();
				ofTranslate(pathWorldPos.x, pathY, pathWorldPos.z);
				ofRotateXDeg(90);
				ofDrawCircle(0, 0, TILE_SIZE * 0.3f);
				ofPopMatrix();
			}
			ofDisableDepthTest();
		}

		glDepthMask(GL_TRUE);
		ofEnableLighting();
		cam.end();
		ofDisableLighting();
		ofDisableDepthTest();
	};

	// --- POST PROCESSING & 2D UI DRAWING ---
	const bool usePost = (enableWorldPostProcess && worldPostShaderLoaded);
	if (usePost) {
		allocateWorldFbo(ofGetWidth(), ofGetHeight());
		if (worldFbo.isAllocated()) {
			worldFbo.begin();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderWorld3D();
			worldFbo.end();

			ofDisableDepthTest();
			worldPostShader.begin();
			worldPostShader.setUniformTexture("tex0", worldFbo.getTexture(), 0);
			worldPostShader.setUniform2f("uResolution", ofGetWidth(), ofGetHeight());
			worldFbo.getTexture().draw(0, 0, ofGetWidth(), ofGetHeight());
			worldPostShader.end();
		} else {
			renderWorld3D();
		}
	} else {
		renderWorld3D();
	}

	ofEnableAlphaBlending();

	// === FIX 3: ENSURE LIGHTING IS OFF BEFORE ANY UI ===
	ofDisableLighting();

	// --- 8. DRAW UI ---

	// === NUCLEAR GRAPHICS RESET ===
	ofDisableLighting();
	ofDisableDepthTest();
	ofDisableBlendMode(); // Reset blend mode

	// 1. Reset Texture
	glBindTexture(GL_TEXTURE_2D, 0);

	// 2. Reset Colors
	ofSetColor(255, 255, 255, 255);

	// 3. Reset Materials (The likely cause of the grey UI)
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE); // Ensure culling is off for 2D

	// Reset standard material properties to defaults just in case
	float defaultAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	float defaultDiffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, defaultAmbient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, defaultDiffuse);

	// 4. Re-enable Alpha for UI
	ofEnableAlphaBlending();

	// (floating key is rendered as a vertical 3D billboard in the world pass so it can be occluded)

	drawMinionManagerUI();

	float designHeight = 1080.0f;
	float scale = ofGetHeight() / designHeight;
	float fontScale = scale * 1.0f;

	float handBaseCardWidth = 120;
	float handCardAspectRatio = 585.0f / 409.0f;
	float baseCardHeight = handBaseCardWidth * handCardAspectRatio;
	float staticUICardWidth = (handBaseCardWidth * 1.3f) * scale;
	float staticUICardHeight = (baseCardHeight * 1.3f) * scale;

	// Health bar dimensions (used both by drawHealthBar lambda and by anchored status text)
	float healthBarHeight = 65 * scale;

	// Form bar dimensions and spacing for stacking (shared with status positioning)
	float formBarHeight = 30 * scale;
	float formSpacing = 5 * scale;

	auto drawHealthBar = [&](Player & player, float x, float y, ofColor healthColor) {
		float healthBarWidth = 220 * scale;
		// Use outer healthBarHeight, formBarHeight, formSpacing for consistent stacking
		float nextBarY = y;

		// Determine layout direction
		bool isTopAligned = y < ofGetHeight() / 2;

		if (isTopAligned) {
			// Stack forms below the main bar
			float bottomOfMain = y + healthBarHeight;
			float nextY = bottomOfMain;
			if (player.inTortoiseForm) {
				float formY = nextY + formSpacing;
				int rem = 5 - player.tortoiseDamageTaken;
				ofSetColor(20, 40, 20);
				ofDrawRectangle(x, formY, healthBarWidth, formBarHeight);
				ofSetColor(ofColor::darkGreen);
				ofDrawRectangle(x, formY, healthBarWidth * (rem / 5.0f), formBarHeight);
				string txt = "Tortoise: " + ofToString(rem) + "/5";
				drawStatText(uiFont, txt, x, formY, healthBarWidth, formBarHeight, ofColor::white);
				if (ofRectangle(x, formY, healthBarWidth, formBarHeight).inside(ofGetMouseX(), ofGetMouseY())) {
					isShowingTooltip = true;
					tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
					tooltipText = "Tortoise Form: Buffer HP";
				}
				nextY = formY + formBarHeight;
			}
			if (player.inGhostForm) {
				float formY = nextY + formSpacing;
				int rem = 4 - player.ghostDamageTaken;
				ofSetColor(30, 30, 50);
				ofDrawRectangle(x, formY, healthBarWidth, formBarHeight);
				ofSetColor(150, 150, 255);
				ofDrawRectangle(x, formY, healthBarWidth * (rem / 4.0f), formBarHeight);
				string txt = "Ghost: " + ofToString(rem) + "/4";
				drawStatText(uiFont, txt, x, formY, healthBarWidth, formBarHeight, ofColor::white);
				if (ofRectangle(x, formY, healthBarWidth, formBarHeight).inside(ofGetMouseX(), ofGetMouseY())) {
					isShowingTooltip = true;
					tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
					tooltipText = "Ghost Form: Immune to Physical/Piercing";
				}
				nextY = formY + formBarHeight;
			}
		} else {
			// Stack forms above the main bar
			float topOfMain = y;
			float nextYUp = topOfMain;
			if (player.inTortoiseForm) {
				float formY = nextYUp - formSpacing - formBarHeight;
				int rem = 5 - player.tortoiseDamageTaken;
				ofSetColor(20, 40, 20);
				ofDrawRectangle(x, formY, healthBarWidth, formBarHeight);
				ofSetColor(ofColor::darkGreen);
				ofDrawRectangle(x, formY, healthBarWidth * (rem / 5.0f), formBarHeight);
				string txt = "Tortoise: " + ofToString(rem) + "/5";
				drawStatText(uiFont, txt, x, formY, healthBarWidth, formBarHeight, ofColor::white);
				if (ofRectangle(x, formY, healthBarWidth, formBarHeight).inside(ofGetMouseX(), ofGetMouseY())) {
					isShowingTooltip = true;
					tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
					tooltipText = "Tortoise Form: Buffer HP";
				}
				nextYUp = formY;
			}
			if (player.inGhostForm) {
				float formY = nextYUp - formSpacing - formBarHeight;
				int rem = 4 - player.ghostDamageTaken;
				ofSetColor(30, 30, 50);
				ofDrawRectangle(x, formY, healthBarWidth, formBarHeight);
				ofSetColor(150, 150, 255);
				ofDrawRectangle(x, formY, healthBarWidth * (rem / 4.0f), formBarHeight);
				string txt = "Ghost: " + ofToString(rem) + "/4";
				drawStatText(uiFont, txt, x, formY, healthBarWidth, formBarHeight, ofColor::white);
				if (ofRectangle(x, formY, healthBarWidth, formBarHeight).inside(ofGetMouseX(), ofGetMouseY())) {
					isShowingTooltip = true;
					tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
					tooltipText = "Ghost Form: Immune to Physical/Piercing";
				}
				nextYUp = formY;
			}
		}

		// --- MAIN COMBINED STATS BAR ---
		// 1. Calculate Widths
		float statW = healthBarWidth * 0.15f; // 15% width for shields
		float usedWidth = 0;
		if (player.block > 0) usedWidth += statW;
		if (player.fortification > 0) usedWidth += statW;
		if (player.barrier > 0) usedWidth += statW;
		if (player.holyBlock > 0) usedWidth += statW;
		if (player.ward > 0) usedWidth += statW;

		float hpW = healthBarWidth - usedWidth;
		float currentX = x;

		// 2. Health Segment
		ofSetColor(healthColor.getLerped(ofColor::black, 0.5));
		ofDrawRectangle(currentX, y, hpW, healthBarHeight);
		float hpPct = (float)player.health / player.maxHealth;
		ofSetColor(healthColor);
		ofDrawRectangle(currentX, y, hpW * hpPct, healthBarHeight);
		string hpText = ofToString(player.health) + "/" + ofToString(player.maxHealth);
		drawStatText(titleFont, hpText, currentX, y, hpW, healthBarHeight, ofColor::white, 1.0f);

		currentX += hpW;

		// 3. Shield Segments
		auto drawSeg = [&](int val, ofColor c, string label) {
			if (val > 0) {
				ofSetColor(c);
				ofDrawRectangle(currentX, y, statW, healthBarHeight);
				drawStatText(titleFont, ofToString(val), currentX, y, statW, healthBarHeight, (c.getBrightness() > 200 ? ofColor::black : ofColor::white), 1.0f);

				// Tooltip Check
				if (ofRectangle(currentX, y, statW, healthBarHeight).inside(ofGetMouseX(), ofGetMouseY())) {
					isShowingTooltip = true;
					tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
					tooltipText = label;
				}

				currentX += statW;
			}
		};

		drawSeg(player.block, ofColor::gray, "Block (Physical)");
		drawSeg(player.fortification, ofColor(50, 50, 50), "Fortification (Phys/Pierce)");
		drawSeg(player.barrier, ofColor::hotPink, "Barrier (Non-Physical)");
		drawSeg(player.holyBlock, ofColor::yellow, "Holy Block (Holy)");
		drawSeg(player.ward, ofColor::black, "Ward (All Damage)");
	};

	// Draw Floating Text
	for (const auto & ft : activeFloatingTexts) {
		glm::vec2 screenPos = cam.worldToScreen(ft.worldPos);

		// Fade out alpha
		float life = (ofGetElapsedTimef() - ft.startTime) / ft.duration;
		float alpha = 255 * (1.0f - pow(life, 3.0f));

		ofRectangle bounds = titleFont.getStringBoundingBox(ft.text, 0, 0);

		ofPushMatrix();
		ofTranslate(screenPos.x, screenPos.y);

		// Scale up slightly as it spawns (pop effect)
		float s = ofMap(life, 0.0, 0.1, 0.0, 0.5, true);
		if (life > 0.1) s = 0.5;
		ofScale(s, s);

		// Shadow (with vertical centering fix)
		ofSetColor(0, 0, 0, alpha);
		titleFont.drawString(ft.text, -bounds.width / 2 + 2, bounds.height / 2 + 2);

		// Main Text (with vertical centering fix)
		ofSetColor(ft.color, alpha);
		titleFont.drawString(ft.text, -bounds.width / 2, bounds.height / 2);

		ofPopMatrix();
	}
	// --- MAIN UI DRAWING ---
	// --- FIX: Find the main players to prevent UI bugs with minions ---
	Player * player0 = nullptr;
	Player * player1 = nullptr;
	for (auto & p : players) {
		if (p.playerID == 0) player0 = &p;
		if (p.playerID == 1) player1 = &p;
	}

	if (player0 && player1) {
		// In multiplayer, swap perspective so local player is always at bottom
		Player * localPlayer = player0;
		Player * opponentPlayer = player1;
		if (isMultiplayer && myLocalPlayerID == 1) {
			localPlayer = player1;
			opponentPlayer = player0;
		}

		// 1. Calculate positions - bottom = local player, top = opponent
		float p0_deckX = 20 * scale;
		float p0_deckY = ofGetHeight() - staticUICardHeight - (20 * scale) - staticUICardHeight - (20 * scale);
		p0_deckRect.set(p0_deckX, p0_deckY, staticUICardWidth, staticUICardHeight);

		float p0_discardX = p0_deckX;
		float p0_discardY = p0_deckY + staticUICardHeight + (20 * scale);
		p0_discardRect.set(p0_discardX, p0_discardY, staticUICardWidth, staticUICardHeight);

		// --- CHANGES ARE HERE ---
		float p1_discardX = ofGetWidth() - staticUICardWidth - (20 * scale); // Was 30 (Moved right)
		float p1_discardY = 20 * scale; // Was 40 (Moved up)
		p1_discardRect.set(p1_discardX, p1_discardY, staticUICardWidth, staticUICardHeight);

		float p1_deckX = p1_discardX;
		float p1_deckY = p1_discardY + staticUICardHeight + (20 * scale); // Was 40 (Reduced gap)
		p1_deckRect.set(p1_deckX, p1_deckY, staticUICardWidth, staticUICardHeight);

		// 2. Draw Player 0 (Bottom) UI - this is the LOCAL player
		float p0_healthX = ofGetWidth() - (220 * scale) - (50 * scale);
		float p0_healthY = ofGetHeight() - (65 * scale) - (40 * scale);
		drawHealthBar(*localPlayer, p0_healthX, p0_healthY, ofColor::green);

		// P0 Deck (LOCAL player's deck)
		if (!localPlayer->deck.empty()) {
			ofSetColor(ofColor::white);
			cardBackImage.draw(p0_deckRect);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p0_deckRect, 10 * scale);
		}

		// Draw hover glow for deck
		if (localHoverType == HOVER_DECK) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(255, 255, 255, 200); // White glow
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p0_deckRect);
			ofPopStyle();
		}

		// Only highlight deck if it's MY turn and I haven't drawn yet
		if (currentState == STATE_GAMEPLAY && isCurrentPlayerLocal() && !hasDrawnCardsThisTurn) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::green);
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p0_deckRect);
			ofPopStyle();
		}

		// P0 Discard (LOCAL player's discard)
		if (!localPlayer->discardPile.empty()) {
			ofSetColor(ofColor::white);
			const auto & discardRect = localPlayer->discardPile.back().textureRect;
			cardSpriteSheet.drawSubsection(p0_discardRect.x, p0_discardRect.y, p0_discardRect.width, p0_discardRect.height,
				discardRect.x, discardRect.y, discardRect.width, discardRect.height);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p0_discardRect, 10 * scale);
		}

		// Draw hover glow for discard
		if (localHoverType == HOVER_DISCARD) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(255, 255, 255, 200); // White glow
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p0_discardRect);
			ofPopStyle();
		}

		// 3. Draw Player 1 (Top) UI - this is the OPPONENT player
		float p1_healthX = 40 * scale;
		float p1_healthY = 40 * scale;
		drawHealthBar(*opponentPlayer, p1_healthX, p1_healthY, ofColor::red);

		// P1 Discard -- show opponent's top card face
		if (!opponentPlayer->discardPile.empty()) {
			ofSetColor(ofColor::white);
			const auto & discardRect = opponentPlayer->discardPile.back().textureRect;
			cardSpriteSheet.drawSubsection(p1_discardRect.x, p1_discardRect.y, p1_discardRect.width, p1_discardRect.height,
				discardRect.x, discardRect.y, discardRect.width, discardRect.height);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p1_discardRect, 10 * scale);
		}

		// P1 Deck (OPPONENT player's deck)
		if (!opponentPlayer->deck.empty()) {
			ofSetColor(ofColor::white);
			cardBackImage.draw(p1_deckRect);
		} else {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p1_deckRect, 10 * scale);
		}

		// Draw hover glow for opponent deck/discard
		if (opponentHoverType == HOVER_DECK) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(255, 0, 0, 200); // Red glow
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p1_deckRect);
			ofPopStyle();
		}
		if (opponentHoverType == HOVER_DISCARD) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(255, 0, 0, 200); // Red glow
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p1_discardRect);
			ofPopStyle();
		}

		// Show outline for opponent's deck when it's their turn and they haven't drawn yet
		if (currentState == STATE_GAMEPLAY && !isCurrentPlayerLocal() && !opponentHasDrawnCardsThisTurn) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::green);
			ofSetLineWidth(4 * scale);
			ofDrawRectangle(p1_deckRect);
			ofPopStyle();
		}

		// 4. Draw AP Displays & Statuses (UPDATED)
		string p0_apText = "0 AP";
		string p1_apText = "? AP";

		// Compute displayed AP for the active unit by summing finished AP/BONUS_AP rolls
		int displayedAP = 0;
		if (currentPlayerIndex >= 0) {
			for (const auto & r : activeDiceRolls) {
				if ((r.purpose == PURPOSE_AP || r.purpose == PURPOSE_BONUS_AP) && r.isFinishedVisual) {
					// Only count dice that belong to the current unit (associatedUnit)
					if (r.associatedUnit == currentPlayerIndex) displayedAP += r.result;
				}
			}
			if (players[currentPlayerIndex].nextTurnAPBonus > 0) displayedAP += players[currentPlayerIndex].nextTurnAPBonus;

			// Prefer the authoritative `currentAP` value, but if the player has spent AP
			// (currentAP < displayedAP), show the decreased value immediately to avoid
			// the AP counter appearing delayed while dice visuals linger.
			if (currentAP < displayedAP) {
				displayedAP = currentAP;
			} else {
				displayedAP = std::max(displayedAP, currentAP);
			}

			Player & currentPlayer = players[currentPlayerIndex];
			// In multiplayer, assign AP text based on local player perspective
			if (isMultiplayer) {
				// If current player is me, show my AP at bottom, otherwise at top
				if (isMyTurn()) {
					p0_apText = ofToString(displayedAP) + " AP";
				} else {
					p1_apText = ofToString(displayedAP) + " AP";
				}
			} else {
				// Single player: use original logic
				if (currentPlayer.playerID == 0 || currentPlayer.ownerID == 0) {
					p0_apText = ofToString(displayedAP) + " AP";
				} else if (currentPlayer.playerID == 1 || currentPlayer.ownerID == 1) {
					p1_apText = ofToString(displayedAP) + " AP";
				}
			}
		}

		// --- Draw P0 AP Box (BOTTOM - Local Player) ---
		float p0_apCenterX = 20 * scale + staticUICardWidth / 2;
		float p0_apCenterY = ofGetHeight() - staticUICardHeight - (20 * scale) - staticUICardHeight - (20 * scale) - 60 * scale;
		ofRectangle p0_apTextBox = titleFont.getStringBoundingBox(p0_apText, 0, 0);
		float p0_apRectWidth = (p0_apTextBox.width * fontScale) + (40 * scale);
		float p0_apRectHeight = (p0_apTextBox.height * fontScale) + (20 * scale);
		// In multiplayer, only show bottom AP counter when it's the local player's turn
		bool skipDrawP0AP = false;
		if (currentState == STATE_DRAFTING) skipDrawP0AP = true;
		if (currentState == STATE_INITIATIVE_ROLL) skipDrawP0AP = true; // Hide AP during initiative roll
		if (currentPlayerIndex >= 0) {
			// Bottom deck is always local player, so only show AP when current turn is local player
			if (isMultiplayer && players[currentPlayerIndex].playerID != myLocalPlayerID) {
				skipDrawP0AP = true;
			} else if (!isMultiplayer && players[currentPlayerIndex].playerID == 1) {
				// Single player: don't show P0 AP if current player is player 1
				skipDrawP0AP = true;
			}
		}
		if (!skipDrawP0AP) {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p0_apCenterX - p0_apRectWidth / 2, p0_apCenterY - p0_apRectHeight / 2, p0_apRectWidth, p0_apRectHeight, 10 * scale);
			ofSetColor(ofColor::cyan);
			ofPushMatrix();
			ofTranslate(p0_apCenterX, p0_apCenterY);
			ofScale(fontScale, fontScale);
			titleFont.drawString(p0_apText, -p0_apTextBox.getCenter().x, -p0_apTextBox.getCenter().y);
			ofPopMatrix();
		}

		// --- DRAW P0 STATUSES (BOTTOM - Local Player) ---
		// Position these relative to the local player's health bar: bottom-right, stacked above the HP counter
		float p0_statusXStart = p0_healthX + 5 * scale;
		// Account for stacked form bars above the health bar so statuses sit on top
		int p0_formsAbove = (localPlayer->inTortoiseForm ? 1 : 0) + (localPlayer->inGhostForm ? 1 : 0);
		float p0_totalFormsHeight = p0_formsAbove * (formBarHeight + formSpacing);
		float p0_statusY = p0_healthY - 10 * scale - p0_totalFormsHeight; // start above the topmost form
		float smallFontScale = fontScale * 0.8f;

		// --- NEW STATUSES ---

		// Draw combined Luck (permanent + passive)
		int localPlayerIndex = -1;
		for (int i = 0; i < (int)players.size(); ++i) {
			if (players[i].playerID == myLocalPlayerID) {
				localPlayerIndex = i;
				break;
			}
		}
		int p0Passive = (localPlayerIndex >= 0) ? computePassiveLuck(localPlayerIndex) : 0;
		int p0TotalLuck = localPlayer->luck + p0Passive;
		if (p0TotalLuck > 0) {
			string luckText = "+" + ofToString(p0TotalLuck) + " Luck";
			ofRectangle luckBox = titleFont.getStringBoundingBox(luckText, 0, 0);
			ofSetColor(ofColor::darkGreen);
			ofPushMatrix();
			ofTranslate(p0_statusXStart, p0_statusY);
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(luckText, 0, 0);
			ofPopMatrix();
			p0_statusY -= (luckBox.height * smallFontScale) + (5 * scale);
		}

		if (localPlayer->nextTurnAPBonus > 0) {
			string bonusText = "+" + ofToString(localPlayer->nextTurnAPBonus) + " AP Next Turn";
			ofRectangle bonusBox = titleFont.getStringBoundingBox(bonusText, 0, 0);
			ofSetColor(ofColor::green);
			ofPushMatrix();
			ofTranslate(p0_statusXStart, p0_statusY);
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(bonusText, 0, 0);
			ofPopMatrix();
			p0_statusY -= (bonusBox.height * smallFontScale) + (5 * scale);
		}

		if (localPlayer->strengthenElementsTurnsRemaining > 0) {
			string elemText = "Elem Buff (" + ofToString(localPlayer->strengthenElementsTurnsRemaining) + ")";
			ofRectangle elemBox = titleFont.getStringBoundingBox(elemText, 0, 0);
			ofSetColor(ofColor::orange);
			ofPushMatrix();
			ofTranslate(p0_statusXStart, p0_statusY);
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(elemText, 0, 0);
			ofPopMatrix();
			p0_statusY -= (elemBox.height * smallFontScale) + (5 * scale);
		}

		if (localPlayer->nextTurnD10AP) {
			string d10Text = "D10 AP";
			ofRectangle d10Box = titleFont.getStringBoundingBox(d10Text, 0, 0);
			ofSetColor(ofColor::white);
			ofPushMatrix();
			ofTranslate(p0_statusXStart, p0_statusY);
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(d10Text, 0, 0);
			ofPopMatrix();
			p0_statusY -= (d10Box.height * smallFontScale) + (5 * scale);
		}

		// Note: 'On Fire', 'Sleeping', 'Paralyzed', 'Poison Ready', and 'Poisoned'
		// are intentionally not shown next to the AP counter — those statuses
		// are represented with in-world effects/icons already.

		/// --- Draw P1 AP Box (TOP - Opponent in Multiplayer) ---
		float p1_apCenterX = ofGetWidth() - staticUICardWidth - (20 * scale) + staticUICardWidth / 2;
		float p1_apCenterY = 20 * scale + staticUICardHeight + (20 * scale) + staticUICardHeight + 60 * scale;
		ofRectangle p1_apTextBox = titleFont.getStringBoundingBox(p1_apText, 0, 0);
		float p1_apRectWidth = (p1_apTextBox.width * fontScale) + (40 * scale);
		float p1_apRectHeight = (p1_apTextBox.height * fontScale) + (20 * scale);
		// In multiplayer, only show top AP counter when it's the opponent's turn (not local player)
		bool skipDrawP1AP = false;
		if (currentState == STATE_DRAFTING) skipDrawP1AP = true;
		if (currentState == STATE_INITIATIVE_ROLL) skipDrawP1AP = true; // Hide AP during initiative roll
		if (currentPlayerIndex >= 0) {
			// Top deck is opponent in multiplayer, so only show AP when NOT local player's turn
			if (isMultiplayer && isCurrentPlayerLocal()) {
				skipDrawP1AP = true;
			} else if (!isMultiplayer && players[currentPlayerIndex].playerID == 0) {
				// Single player: don't show P1 AP if current player is player 0
				skipDrawP1AP = true;
			}
		}
		if (!skipDrawP1AP) {
			ofSetColor(0, 0, 0, 150);
			ofDrawRectRounded(p1_apCenterX - p1_apRectWidth / 2, p1_apCenterY - p1_apRectHeight / 2, p1_apRectWidth, p1_apRectHeight, 10 * scale);
			ofSetColor(ofColor::cyan);
			ofPushMatrix();
			ofTranslate(p1_apCenterX, p1_apCenterY);
			ofScale(fontScale, fontScale);
			titleFont.drawString(p1_apText, -p1_apTextBox.getCenter().x, -p1_apTextBox.getCenter().y);
			ofPopMatrix();
		}

		// --- DRAW P1 STATUSES (TOP - Opponent in Multiplayer) ---
		// Position these relative to the opponent's health bar: top-left, stacked below the HP counter
		// Left-align P1 statuses to the health bar start (top-left area)
		float p1_statusXStart = p1_healthX + 5 * scale;
		// Account for stacked form bars below the health bar so statuses sit on top of them
		int p1_formsBelow = (opponentPlayer->inTortoiseForm ? 1 : 0) + (opponentPlayer->inGhostForm ? 1 : 0);
		float p1_totalFormsHeight = p1_formsBelow * (formBarHeight + formSpacing);
		float p1_statusY = p1_healthY + healthBarHeight + 10 * scale + p1_totalFormsHeight;

		// Draw combined Luck (permanent + passive) for opponent
		int opponentPlayerIndex = -1;
		for (int i = 0; i < (int)players.size(); ++i) {
			if (isMultiplayer && players[i].playerID != myLocalPlayerID && !players[i].isMinion) {
				opponentPlayerIndex = i;
				break;
			} else if (!isMultiplayer && players[i].playerID == 1) {
				opponentPlayerIndex = i;
				break;
			}
		}
		int p1Passive = (opponentPlayerIndex >= 0) ? computePassiveLuck(opponentPlayerIndex) : 0;
		int p1TotalLuck = opponentPlayer->luck + p1Passive;
		if (p1TotalLuck > 0) {
			string luckText = "+" + ofToString(p1TotalLuck) + " Luck";
			ofRectangle luckBox = titleFont.getStringBoundingBox(luckText, 0, 0);
			ofSetColor(ofColor::darkGreen);
			ofPushMatrix();
			ofTranslate(p1_statusXStart, p1_statusY + (luckBox.height * smallFontScale));
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(luckText, 0, 0);
			ofPopMatrix();
			p1_statusY += (luckBox.height * smallFontScale) + (5 * scale);
		}

		// --- NEW STATUSES ---

		if (opponentPlayer->nextTurnD10AP) {
			string d10Text = "D10 AP";
			ofRectangle d10Box = titleFont.getStringBoundingBox(d10Text, 0, 0);
			ofSetColor(ofColor::white);
			ofPushMatrix();
			ofTranslate(p1_statusXStart, p1_statusY + (d10Box.height * smallFontScale));
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(d10Text, 0, 0);
			ofPopMatrix();
			p1_statusY += (d10Box.height * smallFontScale) + (5 * scale);
		}

		if (opponentPlayer->strengthenElementsTurnsRemaining > 0) {
			string elemText = "Elem Buff (" + ofToString(opponentPlayer->strengthenElementsTurnsRemaining) + ")";
			ofRectangle elemBox = titleFont.getStringBoundingBox(elemText, 0, 0);
			ofSetColor(ofColor::orange);
			ofPushMatrix();
			ofTranslate(p1_statusXStart, p1_statusY + (elemBox.height * smallFontScale));
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(elemText, 0, 0);
			ofPopMatrix();
			p1_statusY += (elemBox.height * smallFontScale) + (5 * scale);
		}

		if (opponentPlayer->nextTurnAPBonus > 0) {
			string bonusText = "+" + ofToString(opponentPlayer->nextTurnAPBonus) + " AP Next Turn";
			ofRectangle bonusBox = titleFont.getStringBoundingBox(bonusText, 0, 0);
			ofSetColor(ofColor::green);
			ofPushMatrix();
			ofTranslate(p1_statusXStart, p1_statusY + (bonusBox.height * smallFontScale));
			ofScale(smallFontScale, smallFontScale);
			titleFont.drawString(bonusText, 0, 0);
			ofPopMatrix();
			p1_statusY += (bonusBox.height * smallFontScale) + (5 * scale);
		}

		// Note: see comment above — remove in-AP status texts for these effects.
	}

	// End Turn Button / Turn Indicator
	// If UI wasn't snapped on resize (some platforms/window managers),
	// ensure the end turn button has a sensible initial position instead of (0,0)
	if (endTurnButtonCurrentPos.x == 0 && endTurnButtonCurrentPos.y == 0) {
		float btnWidth_tmp = 250 * scale;
		float visibleY = 20 * scale;
		float hiddenY = -100 * scale;
		bool myTurn = isMyTurn();
		if (myTurn)
			endTurnButtonCurrentPos.set(ofGetWidth() / 2.0f - btnWidth_tmp / 2.0f, visibleY);
		else
			endTurnButtonCurrentPos.set(ofGetWidth() / 2.0f - btnWidth_tmp / 2.0f, hiddenY);
		endTurnButtonTargetPos = endTurnButtonCurrentPos;
	}

	float btnWidth_end = 250 * scale;
	float btnHeight_end = 60 * scale;
	endTurnButtonRect.set(endTurnButtonCurrentPos.x, endTurnButtonCurrentPos.y, btnWidth_end, btnHeight_end);

	// Check if it's my turn
	bool myTurn = isMyTurn();

	if (myTurn) {
		// 1. Draw End Turn Button Background
		ofSetColor(isHoveringEndTurn ? ofColor::darkSlateGray : ofColor::slateGray);
		ofDrawRectRounded(endTurnButtonRect, 10 * scale);
	} else {
		// Draw turn indicator showing whose turn it is
		ofSetColor(60, 60, 80, 200);
		ofDrawRectRounded(endTurnButtonRect, 10 * scale);

		// Get current player's name (not "opponent" - use actual Steam name)
		std::string playerName = "Player";
		if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
			playerName = getPlayerSteamName(currentPlayerIndex);
		}

		// Draw profile picture on the left side
		float avatarSize = btnHeight_end * 0.7f;
		float avatarPadding = 10 * scale;
		float avatarX = endTurnButtonRect.x + avatarPadding;
		float avatarY = endTurnButtonRect.getCenter().y - avatarSize / 2;

		// Draw avatar background circle
		ofPushStyle();
		ofSetColor(80, 80, 100);
		ofDrawCircle(avatarX + avatarSize / 2, avatarY + avatarSize / 2, avatarSize / 2);

		// Choose avatar image based on whose turn it is
		bool isLocalTurn = false;
		if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
			isLocalTurn = (players[currentPlayerIndex].playerID == myLocalPlayerID);
		}

		bool drewAvatar = false;
		if (isLocalTurn && localAvatarReady) {
			ofSetColor(255);
			localAvatarImage.draw(avatarX, avatarY, avatarSize, avatarSize);
			drewAvatar = true;
		} else if (!isLocalTurn && opponentAvatarReady) {
			ofSetColor(255);
			opponentAvatarImage.draw(avatarX, avatarY, avatarSize, avatarSize);
			drewAvatar = true;
		}

		// Fallback: draw initials if avatar is unavailable
		if (!drewAvatar) {
			ofSetColor(ofColor::white);
			string initials = "";
			if (!playerName.empty()) {
				initials += playerName[0];
				// Find second initial after space
				size_t spacePos = playerName.find(' ');
				if (spacePos != string::npos && spacePos + 1 < playerName.length()) {
					initials += playerName[spacePos + 1];
				}
			}
			ofRectangle initialsBox = uiFont.getStringBoundingBox(initials, 0, 0);
			float initialsScale = avatarSize / std::max(initialsBox.width, initialsBox.height) * 0.6f;
			ofPushMatrix();
			ofTranslate(avatarX + avatarSize / 2 - (initialsBox.width * initialsScale / 2),
				avatarY + avatarSize / 2 + (initialsBox.height * initialsScale / 2));
			ofScale(initialsScale, initialsScale);
			uiFont.drawString(initials, 0, 0);
			ofPopMatrix();
		}
		ofPopStyle();

		// Draw text to the right of avatar
		ofSetColor(ofColor::gold);
		string turnText = playerName + "'s Turn";

		// Use smaller font and scale to fit in the button
		float turnFontScale = fontScale * 0.7f; // Make text smaller
		ofRectangle turnTextBox = uiFont.getStringBoundingBox(turnText, 0, 0);
		float textStartX = avatarX + avatarSize + avatarPadding;
		float availableWidth = endTurnButtonRect.width - (avatarSize + avatarPadding * 3);

		// Scale down further if text is still too wide
		float textWidth = turnTextBox.width * turnFontScale;
		if (textWidth > availableWidth) {
			turnFontScale *= (availableWidth / textWidth) * 0.95f; // Leave 5% margin
		}

		float textX = textStartX + (availableWidth - turnTextBox.width * turnFontScale) / 2;
		float textY = endTurnButtonRect.getCenter().y + (turnTextBox.height * turnFontScale / 2);
		ofPushMatrix();
		ofTranslate(textX, textY);
		ofScale(turnFontScale, turnFontScale);
		uiFont.drawString(turnText, 0, 0);
		ofPopMatrix();
	}

	// 2. Draw Yellow Highlight (New Logic)
	// If no AP left AND player has already used their draw, suggest ending turn.
	// But do not highlight if an assistant reroll is possible.
	// Determine displayed AP for current unit (again) and whether an AP roll animation is active
	int displayedAPForCurrent = 0;
	bool apRollActive = false;
	if (currentPlayerIndex >= 0) {
		for (const auto & r : activeDiceRolls) {
			if (r.purpose == PURPOSE_AP || r.purpose == PURPOSE_BONUS_AP) {
				if (!r.isFinishedVisual && r.associatedUnit == currentPlayerIndex) apRollActive = true;
				if (r.isFinishedVisual && r.associatedUnit == currentPlayerIndex) displayedAPForCurrent += r.result;
			}
		}
		if (players[currentPlayerIndex].nextTurnAPBonus > 0) displayedAPForCurrent += players[currentPlayerIndex].nextTurnAPBonus;
		// If `currentAP` has already been updated elsewhere (dice resolution) prefer
		// the current value. Also make sure that when the player spends AP, the
		// displayed value decreases immediately (avoid lingering higher display).
		if (currentAP < displayedAPForCurrent) {
			displayedAPForCurrent = currentAP;
		} else {
			displayedAPForCurrent = std::max(displayedAPForCurrent, currentAP);
		}
	}

	bool rerollAvailable = false;
	if (players.size() > 0 && currentPlayerIndex != -1 && displayedAPForCurrent == 0 && currentAP == 0) {
		Player & curr = players[currentPlayerIndex];
		for (const auto & p : players) {
			if (p.isAssistant && p.health > 0 && p.directSummonerID == (curr.isMinion ? curr.ownerID : curr.playerID) && !p.assistantRerollUsedThisTurn) {
				int dist = abs(p.x - curr.x) + abs(p.y - curr.y);
				if (dist <= 1) {
					rerollAvailable = true;
					break;
				}
			}
		}
	}

	if (displayedAPForCurrent <= 0 && hasDrawnCardsThisTurn && !rerollAvailable && myTurn) {
		ofPushStyle();
		ofNoFill();
		ofSetColor(ofColor::green);
		ofSetLineWidth(4 * scale);
		ofDrawRectRounded(endTurnButtonRect, 10 * scale);
		ofPopStyle();
	}

	// 3. Draw End Turn Button Text (only if it's my turn)
	if (myTurn) {
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
	}

	// --- ASSISTANT AP REROLL BUTTON ---
	if (players.size() > 0 && currentPlayerIndex != -1) {
		Player & curr = players[currentPlayerIndex];

		// Only show if 0 displayed AP, no active AP roll animation, and currentAP is 0
		if (displayedAPForCurrent == 0 && currentAP == 0 && !apRollActive) {
			bool canReroll = false;

			// Check for adjacent unused assistants owned by this unit
			for (const auto & p : players) {
				if (p.isAssistant && p.health > 0) {
					if (p.directSummonerID == curr.playerID && !p.assistantRerollUsedThisTurn) {
						int dist = abs(p.x - curr.x) + abs(p.y - curr.y);
						if (dist <= 1) {
							canReroll = true;
							break;
						}
					}
				}
			}

			if (canReroll) {
				float uiScale = ofGetHeight() / 1080.0f;
				float btnW = 130 * uiScale; // shorter button
				float btnH = 44 * uiScale;
				// Position to the right of Player0's AP counter
				// Recompute P0 AP box metrics (same as earlier) so we can anchor the reroll button
				float staticUICardWidth = (handBaseCardWidth * 1.3f) * scale;
				float staticUICardHeight = (baseCardHeight * 1.3f) * scale;
				float p0_apCenterX = 20 * scale + staticUICardWidth / 2;
				float p0_apCenterY = ofGetHeight() - staticUICardHeight - (20 * scale) - staticUICardHeight - (20 * scale) - 60 * scale;
				string p0_apText = "0 AP";
				if (currentPlayerIndex >= 0 && !players.empty()) {
					p0_apText = ofToString(displayedAPForCurrent) + " AP";
				}
				ofRectangle p0_apTextBox = titleFont.getStringBoundingBox(p0_apText, 0, 0);
				float p0_apRectWidth = (p0_apTextBox.width * fontScale) + (40 * scale);
				float margin = 10 * scale;
				float btnX = p0_apCenterX + p0_apRectWidth / 2 + margin;
				float btnY = p0_apCenterY - (btnH / 2);

				rerollButtonRect.set(btnX, btnY, btnW, btnH);

				// Dark background like End Turn, cyan text like AP counter
				ofSetColor(ofColor::darkSlateGray);
				ofDrawRectRounded(rerollButtonRect, 8);

				// Yellow outline to indicate availability
				ofPushStyle();
				ofNoFill();
				ofSetColor(ofColor::green);
				ofSetLineWidth(3 * scale);
				ofDrawRectRounded(rerollButtonRect, 8);
				ofPopStyle();

				ofSetColor(ofColor::cyan);
				string txt = "Reroll AP";
				ofRectangle b = uiFont.getStringBoundingBox(txt, 0, 0);
				uiFont.drawString(txt, btnX + (btnW - b.width) / 2, btnY + (btnH + b.height) / 2);
			} else {
				rerollButtonRect.set(-1000, -1000, 0, 0);
			}
		} else {
			rerollButtonRect.set(-1000, -1000, 0, 0);
		}
	}

	// --- OPTIMIsED HAND DRAWING ...
	if (!players.empty() && currentPlayerIndex >= 0) {
		// In multiplayer, show BOTH players' hands at bottom in a shared space
		// Get both local and opponent player
		Player * handPlayer = nullptr;
		Player * opponentHandPlayer = nullptr;

		if (isMultiplayer) {
			int localID = myLocalPlayerID;
			int opponentID = (localID == 0) ? 1 : 0;

			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].playerID == localID && !players[i].isMinion) {
					handPlayer = &players[i];
				}
				if (players[i].playerID == opponentID && !players[i].isMinion) {
					opponentHandPlayer = &players[i];
				}
			}
		} else {
			handPlayer = &players[currentPlayerIndex];
		}

		if (!handPlayer) return;
		Player & currentPlayer = *handPlayer;
		size_t numCards = currentPlayer.hand.size();

		static size_t lastLoggedHandSize = 9999;
		if (numCards != lastLoggedHandSize) {
			ofLogNotice("Hand") << "Displaying hand for player " << currentPlayer.playerID << ": " << numCards << " cards (isMultiplayer=" << isMultiplayer << " myLocalPlayerID=" << myLocalPlayerID << ")";
			lastLoggedHandSize = numCards;
		}

		bool isBottomPlayer = true;
		if (isMultiplayer) {
			isBottomPlayer = (currentPlayer.playerID == myLocalPlayerID);
		} else {
			isBottomPlayer = (currentPlayer.playerID == 0 || currentPlayer.ownerID == 0);
		}
		float hoverDirection = isBottomPlayer ? -120.0f : 120.0f;

		// 1. Determine which card should be drawn LAST (On Top)
		int indexToDrawLast = -1;
		if (draggedCardIndex != -1)
			indexToDrawLast = draggedCardIndex;
		else if (hoveredCardIndex != -1)
			indexToDrawLast = hoveredCardIndex;

		// --- HELPER LAMBDA TO DRAW CARD + OUTLINE ---
		auto drawHandCard = [&](int index, bool isTopCard) {
			Card & card = currentPlayer.hand[index];
			float w = handBaseCardWidth * card.currentScale;
			float h = baseCardHeight * card.currentScale;

			float drawX = card.currentPos.x - w / 2;
			float drawY = card.currentPos.y - h / 2;

			// Apply hover offsets
			if (isTopCard) {
				if (index == draggedCardIndex) {
					drawX = card.currentPos.x - w / 2;
					drawY = card.currentPos.y - h / 2;
				} else if (index == hoveredCardIndex) {
					drawY += hoverDirection;
				}
			}

			// A. Draw Sprite
			// Ghostly tint for copied cards in Renewed Inspiration mode
			if (isSelectingRenewedInspiration && card.isCopied) {
				ofSetColor(200, 200, 255); // Subtle Blue-White tint
			} else {
				ofSetColor(255); // Normal
			}

			cardSpriteSheet.drawSubsection(drawX, drawY, w, h, card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);

			// Draw hover glow (white for local, red for opponent)
			if (localHoverType == HOVER_HAND_CARD && localHoverCardIndex == index) {
				ofPushStyle();
				ofNoFill();
				ofSetColor(255, 255, 255, 200); // White glow
				ofSetLineWidth(4);
				ofDrawRectangle(drawX - 2, drawY - 2, w + 4, h + 4);
				ofPopStyle();
			}
			if (opponentHoverType == HOVER_HAND_CARD && opponentHoverCardIndex == index) {
				ofPushStyle();
				ofNoFill();
				ofSetColor(255, 0, 0, 200); // Red glow
				ofSetLineWidth(4);
				ofDrawRectangle(drawX - 2, drawY - 2, w + 4, h + 4);
				ofPopStyle();
			}

			// B. Draw Overlays (Outlines/Dims) at the same depth as the card
			if (isSelectingRenewedInspiration) {
				bool isEligible = (card.drawnThisTurn || card.isCopied);
				bool isSelected = false;
				for (int sel : renewedSelectedHandIndices)
					if (sel == index) isSelected = true;

				if (isSelected) {
					// MATCH NORMAL GAMEPLAY: Yellow Selection
					ofPushStyle();
					ofNoFill();
					ofSetColor(ofColor::green);
					ofSetLineWidth(4);
					ofDrawRectangle(drawX, drawY, w, h);
					ofPopStyle();
				} else if (!isEligible) {
					// Dim non-eligible cards
					ofSetColor(0, 0, 0, 180);
					ofDrawRectangle(drawX, drawY, w, h);
				}
			} else {
				// Normal Gameplay Selection (Yellow)
				if (index == selectedCardIndex || (isTopCard && index == draggedCardIndex)) {
					ofPushStyle();
					ofNoFill();
					ofSetColor(ofColor::green);
					ofSetLineWidth(4);
					ofDrawRectangle(drawX, drawY, w, h);
					ofPopStyle();
				}

				// If Add Poison is primed for this player, highlight only direct physical/piercing cards
				if (currentPlayer.nextAttackAddPoison && (card.damageType == DAMAGE_PHYSICAL || card.damageType == DAMAGE_PIERCING)) {
					bool isDirectDamageCard = (card.targeting == TARGET_ADJACENT_UNIT || card.targeting == TARGET_ADJACENT_OR_SELF_UNIT || card.targeting == TARGET_SELF || card.targeting == TARGET_LINEAR_PIERCE || card.targeting == TARGET_CLEAVE_ADJACENT || card.targeting == TARGET_ADJACENT_UNIT_OR_WALL);

					// Exclude specific non-damaging / indirect cards from being highlighted
					bool isExcluded = (card.type == CARD_SPARK_OF_GENIUS || card.type == CARD_GAIN_BLOCK || card.type == CARD_FORM_OF_TORTOISE || card.type == CARD_FORM_OF_GHOST || card.type == CARD_STRENGTHEN_ELEMENTS || card.type == CARD_DEMOLITION || card.type == CARD_PSIONIC_WAVE || card.type == CARD_EARTHQUAKE || card.type == CARD_DOUBLE_HANDED || card.type == CARD_ADD_POISON || card.type == CARD_RENEWED_INSPIRATION || card.type == CARD_REPLICATE || card.type == CARD_FULL_RESTORE || card.type == CARD_NECRO_BLESSING || card.type == CARD_HASTEN || card.type == CARD_CALL_FOR_WOLVES || card.type == CARD_AMNESIA || card.type == CARD_DARK_SHIELD || card.type == CARD_CONSUME_LARGE_HEALTH_POTION || card.type == CARD_CALL_FOR_KOBOLDS || card.type == CARD_TIME_VORTEX || card.type == CARD_GAIN_WARD || card.type == CARD_CONSUME_HEALTH_POTION || card.type == CARD_DISPEL || card.type == CARD_FORTIFY);

					if (isDirectDamageCard && !isExcluded) {
						ofPushStyle();
						ofNoFill();
						ofSetColor(255, 140, 0); // Orange glow
						ofSetLineWidth(4);
						ofDrawRectangle(drawX - 2, drawY - 2, w + 4, h + 4);
						ofPopStyle();
					}
				}
			}
		};

		// 2. PASS 1: Draw standard cards
		for (size_t i = 0; i < numCards; i++) {
			if (static_cast<int>(i) == indexToDrawLast) continue;
			drawHandCard(i, false);
		}

		// 3. PASS 2: Draw the "Top" card
		if (indexToDrawLast != -1 && indexToDrawLast < static_cast<int>(numCards)) {
			drawHandCard(indexToDrawLast, true);
		}

		// --- DRAW OPPONENT'S HAND IN SAME BOTTOM AREA (SHARED HAND SPACE) ---
		if (isMultiplayer && opponentHandPlayer && !opponentHandPlayer->hand.empty() && !isCurrentPlayerLocal()) {
			// Draw opponent's cards alongside local player's cards in the bottom hand area
			for (size_t i = 0; i < opponentHandPlayer->hand.size(); ++i) {
				Card & card = opponentHandPlayer->hand[i];
				float w = handBaseCardWidth * card.currentScale;
				float h = baseCardHeight * card.currentScale;

				float drawX = card.currentPos.x - w / 2;
				float drawY = card.currentPos.y - h / 2;

				// Draw the card face
				ofSetColor(255);
				cardSpriteSheet.drawSubsection(drawX, drawY, w, h,
					card.textureRect.x, card.textureRect.y, card.textureRect.width, card.textureRect.height);

				// Draw hover glow if opponent is hovering this card
				if (opponentHoverType == HOVER_HAND_CARD && opponentHoverCardIndex == static_cast<int>(i)) {
					ofPushStyle();
					ofNoFill();
					ofSetColor(255, 0, 0, 200); // Red glow for opponent
					ofSetLineWidth(4);
					ofDrawRectangle(drawX - 2, drawY - 2, w + 4, h + 4);
					ofPopStyle();
				}
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

			// Copy the discard pile
			cardsToShowInView = viewPlayer.discardPile;

			// FIX: Reverse the temporary list so the newest card (back of discard) is now at the front for drawing.
			std::reverse(cardsToShowInView.begin(), cardsToShowInView.end());
		}

		int vpIdx = -1;
		for (int i = 0; i < (int)players.size(); ++i) {
			if (players[i].playerID == viewPlayer.playerID && !players[i].isMinion) {
				vpIdx = i;
				break;
			}
		}
		viewTitle = (vpIdx != -1) ? getPlayerSteamName(vpIdx) : ("Player " + ofToString(viewPlayer.playerID)) + "'s " + viewTitle;

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
			bool viewingLocal = false;
			if (currentPileViewPlayerIndex >= 0 && currentPileViewPlayerIndex < (int)players.size()) {
				viewingLocal = (players[currentPileViewPlayerIndex].playerID == myLocalPlayerID);
			}
			// If viewing local player's (bottom/left) piles, show panel to the right
			if (viewingLocal) {
				startX = p0_deckRect.getRight() + 30.0f;
			}
			// If viewing opponent's (top/right) piles, show panel to the left
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

	// --- Draw Chain Lightning Targeting UI ---
	if (isTargetingChainLightning) {
		string msg = "Choose Target for Chain Lightning (2d10 Range)";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(ofColor::yellow); // Electric Color
		titleFont.drawString(msg, tx, ty);
	}

	// --- Draw Magic Hand UI ---
	if (isMagicHandMenuOpen) {
		drawMagicHandUI();
	}

	// --- Draw Train Menu UI ---
	if (isTrainMenuOpen) {
		drawTrainMenuUI();
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
				ofSetColor(ofColor::green);
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
		calculatedHeight += btnHeight + padding; // Skip Checksum (status indicator)
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
		std::string unlimitedAPLabel = hasUnlimitedAP ? "Unlimited AP: ON" : "Unlimited AP: OFF";
		drawDebugButton(debugUnlimitedAPButton, unlimitedAPLabel, true, hasUnlimitedAP);
		currentY += btnHeight + padding;

		// Skip Checksum Status Indicator (non-clickable, just shows state)
		ofRectangle checksumIndicator;
		checksumIndicator.set(panelX + padding, currentY, panelWidth - 2 * padding, btnHeight);
		std::string checksumLabel = skipChecksumValidation ? "Checksum: DISABLED" : "Checksum: ENABLED";
		drawDebugButton(checksumIndicator, checksumLabel, true, !skipChecksumValidation); // Green when enabled (safe), red when disabled
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

	// --- Draw Played Card Animation (Center of screen) ---
	for (const auto & anim : activePlayedCardAnimations) {
		ofSetColor(255, anim.currentAlpha);
		float w = handBaseCardWidth * anim.currentScale;
		float h = baseCardHeight * anim.currentScale;
		cardSpriteSheet.drawSubsection(anim.pos.x - w / 2, anim.pos.y - h / 2, w, h,
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

	// --- Draw Card Played Display (UI-based popup after card is played) ---
	for (const auto & disp : activeCardDisplays) {
		ofSetColor(255, disp.currentAlpha);
		float w = handBaseCardWidth * disp.currentScale;
		float h = baseCardHeight * disp.currentScale;
		cardSpriteSheet.drawSubsection(disp.currentPos.x - w / 2, disp.currentPos.y - h / 2, w, h,
			disp.card.textureRect.x, disp.card.textureRect.y,
			disp.card.textureRect.width, disp.card.textureRect.height);
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
	if (isDoubleHandedMenuOpen) {
		drawDoubleHandedUI();
	}
	if (isAmnesiaMenuOpen) {
		drawAmnesiaMenuUI();
	}
	// FIX: Added Burst UI call
	if (isBurstMenuOpen) {
		drawBurstUI();
	}

	// --- DRAW OPPONENT MENU (if they have one open) ---
	if (opponentMenuOpen && opponentMenuType > 0) {
		drawOpponentMenu();
	}

	// --- Chat System ---
	if (isMultiplayer) {
		float currentTime = ofGetElapsedTimef();
		bool shouldShowChat = isChatOpen || (currentTime - lastChatInteractionTime < chatVisibilityDuration);

		if (shouldShowChat) {
			// Position chat to the right of discard pile, aligned at bottom
			float chatX = p0_discardRect.x + staticUICardWidth + 30 * scale;
			// Align chat bottom with discard pile bottom
			float chatY = p0_discardRect.y + p0_discardRect.height;
			float chatMaxWidth = 450 * scale;

			// Determine size based on state: minimized = smaller, full = larger
			float chatBoxHeight = isChatMinimized ? 120 * scale : 250 * scale;
			float tabHeight = 25 * scale;
			float messageHeight = 18 * scale;

			// Store rect for click detection
			chatWindowRect.set(chatX, chatY - chatBoxHeight - tabHeight, chatMaxWidth, chatBoxHeight + tabHeight);

			// Draw main chat box background (50% opacity black with black outline)
			ofPushStyle();
			ofSetColor(0, 0, 0, 128); // 50% opacity
			ofDrawRectangle(chatX, chatY - chatBoxHeight - tabHeight, chatMaxWidth, chatBoxHeight + tabHeight);
			ofSetColor(0, 0, 0, 255); // Black outline
			ofNoFill();
			ofSetLineWidth(2);
			ofDrawRectangle(chatX, chatY - chatBoxHeight - tabHeight, chatMaxWidth, chatBoxHeight + tabHeight);
			ofFill();
			ofPopStyle();

			// Draw tabs at the top
			float tabWidth = 80 * scale;
			ofPushStyle();

			// Chat tab
			if (currentChatTab == ChatTab::CHAT) {
				ofSetColor(40, 40, 40, 200); // Active tab
			} else {
				ofSetColor(20, 20, 20, 150); // Inactive tab
			}
			ofDrawRectangle(chatX, chatY - chatBoxHeight - tabHeight, tabWidth, tabHeight);
			ofSetColor(255, 255, 255);
			uiFont.drawString("CHAT", chatX + 10, chatY - chatBoxHeight - 5);

			// Log tab
			if (currentChatTab == ChatTab::LOG) {
				ofSetColor(40, 40, 40, 200); // Active tab
			} else {
				ofSetColor(20, 20, 20, 150); // Inactive tab
			}
			ofDrawRectangle(chatX + tabWidth + 2, chatY - chatBoxHeight - tabHeight, tabWidth, tabHeight);
			ofSetColor(255, 255, 255);
			uiFont.drawString("LOG", chatX + tabWidth + 12, chatY - chatBoxHeight - 5);
			ofPopStyle();

			// Draw content based on active tab
			if (currentChatTab == ChatTab::CHAT) {
				// Lambda for text wrapping (shared between input and history)
				auto wrapText = [&](const std::string & text, float maxWidth) {
					std::vector<std::string> lines;
					std::string currentLine;
					for (char c : text) {
						if (c == '\n') {
							if (!currentLine.empty()) lines.push_back(currentLine);
							currentLine.clear();
							continue;
						}
						if (currentLine.empty() && c == ' ') continue;
						std::string testLine = currentLine + c;
						if (uiFont.stringWidth(testLine) <= maxWidth || currentLine.empty()) {
							currentLine = testLine;
						} else {
							lines.push_back(currentLine);
							currentLine = std::string(1, c);
						}
					}
					if (!currentLine.empty()) lines.push_back(currentLine);
					if (lines.empty()) lines.push_back(" ");
					return lines;
				};

				// Calculate input height to adjust chat history display area
				float inputExtraHeight = 0;
				if (isChatOpen && !isChatMinimized) {
					string displayText = "> " + chatInput;
					if (((int)(ofGetElapsedTimef() * 2)) % 2 == 0) {
						displayText += "_";
					}
					float maxWidth = chatMaxWidth - 20;
					std::vector<string> wrappedInputLines = wrapText(displayText, maxWidth);
					// If more than 1 line, push chat history up by the extra lines
					if (wrappedInputLines.size() > 1) {
						inputExtraHeight = (wrappedInputLines.size() - 1) * messageHeight;
					}
				}

				// Draw chat history - adjusted upward if input is multi-line
				// Moved up to chatY - 40 to create space for 11 messages and larger gap before input
				float messageY = chatY - 40 * scale - inputExtraHeight;
				int visibleMessages = 0;
				int maxVisible = isChatMinimized ? 4 : 11; // Increased from 10 to 11 for full mode
				for (int i = (int)chatHistory.size() - 1; i >= 0 && visibleMessages < maxVisible; i--) {
					ChatMessage & msg = chatHistory[i];
					float age = currentTime - msg.timestamp;

					// Fade out messages after chatMessageLifetime seconds (unless chat is open)
					float alpha = 255.0f;
					if (!isChatOpen && age > chatMessageLifetime) {
						continue; // Don't draw old messages when chat is closed
					} else if (!isChatOpen && age > chatMessageLifetime * 0.7f) {
						float fadeProgress = (age - chatMessageLifetime * 0.7f) / (chatMessageLifetime * 0.3f);
						alpha = 255.0f * (1.0f - fadeProgress);
					}

					// Draw message text with format "Steam name: message" with word wrapping
					ofPushStyle();
					ofSetColor(255, 255, 255, alpha);
					string fullMsg = msg.playerName + ": " + msg.message;

					// Word wrap the message to fit in chat box
					float maxWidth = chatMaxWidth - 20;
					std::vector<string> wrappedLines = wrapText(fullMsg, maxWidth);

					// Draw each line
					for (auto it = wrappedLines.rbegin(); it != wrappedLines.rend(); ++it) {
						uiFont.drawString(*it, chatX + 10, messageY);
						messageY -= messageHeight;
						visibleMessages++;
						if (visibleMessages >= maxVisible) break;
					}
					ofPopStyle();
				}

				// Draw chat input box when chat is open (only in full mode)
				if (isChatOpen && !isChatMinimized) {
					// Positioned to maintain gap between chat history and input
					float inputY = chatY - 10 * scale;

					// Draw input text with word wrapping
					ofPushStyle();
					ofSetColor(ofColor::white);
					string displayText = "> " + chatInput;
					if (((int)(ofGetElapsedTimef() * 2)) % 2 == 0) {
						displayText += "_"; // Blinking cursor
					}

					// Word wrap the input text
					float maxWidth = chatMaxWidth - 20;
					if (((int)(ofGetElapsedTimef() * 2)) % 2 == 0) {
						displayText += "_";
					}
					std::vector<string> wrappedLines = wrapText(displayText, maxWidth);

					// Draw each line
					float currentY = inputY - (wrappedLines.size() - 1) * messageHeight;
					for (const auto & line : wrappedLines) {
						uiFont.drawString(line, chatX + 10, currentY);
						currentY += messageHeight;
					}

					// Show character count
					string charCount = ofToString(chatInput.length()) + "/" + ofToString(maxChatInputLength);
					uiFont.drawString(charCount, chatX + chatMaxWidth - 60, inputY);
					ofPopStyle();
				}
			} else if (currentChatTab == ChatTab::LOG) {
				// Draw game log
				float logY = chatY - 35 * scale;
				int visibleLogs = 0;
				int maxVisible = isChatMinimized ? 5 : 12; // Fewer logs when minimized

				for (int i = (int)gameLog.size() - 1; i >= 0 && visibleLogs < maxVisible; i--) {
					GameLogEntry & entry = gameLog[i];

					ofPushStyle();
					ofSetColor(200, 200, 200);
					uiFont.drawString(entry.text, chatX + 10, logY);
					ofPopStyle();

					logY -= messageHeight;
					visibleLogs++;
				}
			}
		}
	}

	// --- Debug Card Spawner UI (KRunner-style) ---
	if (isCardSpawnerOpen) {
		drawCardSpawnerUI();
	}
	if (isCardEncyclopediaOpen) {
		drawCardEncyclopediaUI();
	}

	// --- TOP INSTRUCTION TEXT (Wolf Placement) ---
	if (isPlacingWolves && !isWaitingForWolfCoin) {
		string msg = "Choose Wolf Spawn Square";

		// Optional: Change text if it's the second wolf
		if (wolfSummonStage == 2) msg = "Heads! Choose 2nd Wolf Spawn Square";

		// Calculate center position
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);

		// MOVED LOWER: 25% down the screen
		float ty = ofGetHeight() * 0.25f;

		// Draw Text Shadow/Outline for visibility
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		titleFont.drawString(msg, tx - 2, ty - 2);
		titleFont.drawString(msg, tx + 2, ty - 2);
		titleFont.drawString(msg, tx - 2, ty + 2);

		// Draw Main Text
		ofSetColor(ofColor::white);
		titleFont.drawString(msg, tx, ty);
	}

	// --- MAGIC BOLT INSTRUCTION TEXT ---
	if (isTargetingMagicBolt) {
		string msg = "Choose Target Tile for Magic Bolt";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		// Shadow
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		// Text
		ofSetColor(ofColor::cyan);
		titleFont.drawString(msg, tx, ty);
	}

	// --- DEATH INSTRUCTION ---
	if (isTargetingDeath) {
		string msg = "Select Target for Death";
		// ... standard text drawing code (copy from magic bolt) ...
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(ofColor::red);
		titleFont.drawString(msg, tx, ty);
	}

	// --- HEAL INSTRUCTION ---
	if (isTargetingHeal) {
		string msg = "Select Target to Heal (Self or Ally)";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(ofColor::green);
		titleFont.drawString(msg, tx, ty);
	}

	// --- DOUBLE HANDED TARGETING INSTRUCTION TEXT ---
	if (isTargetingDoubleHanded) {
		string msg = "Choose Target for Double Handed (2x " + pendingDoubleHandedChoice + ")";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(ofColor::green);
		titleFont.drawString(msg, tx, ty);
	}

	// --- AMNESIA TARGETING INSTRUCTION TEXT ---
	if (isTargetingAmnesia) {
		string msg = "Choose Adjacent Unit for Amnesia";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(ofColor::magenta);
		titleFont.drawString(msg, tx, ty);
	}

	// --- TORTOISE DAMAGE TARGETING INSTRUCTION TEXT ---
	if (isTargetingTortoiseDamage) {
		string msg = "Shell Spike: Choose Adjacent Unit";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(ofColor::darkGreen);
		titleFont.drawString(msg, tx, ty);
	}

	// --- HELLHOUND INSTRUCTION TEXT ---
	if (isTargetingHellhound) {
		string msg = "Choose Adjacent Tile for Hellhound";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		// Shadow
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		// Text (Orange for fire/hell)
		ofSetColor(ofColor::orangeRed);
		titleFont.drawString(msg, tx, ty);
	}

	// --- TELEPORT TARGETING INSTRUCTION TEXT ---
	if (isTargetingTeleport) {
		string msg = "Choose Teleport Destination (Range: " + ofToString(pendingTeleportRollResult) + " ft)";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(ofColor::cyan);
		titleFont.drawString(msg, tx, ty);
	}

	// --- DICE ROLL RESULT TEXT ---
	if (!diceRollResultText.empty() && (ofGetElapsedTimef() - diceRollResultStartTime) < diceRollResultDuration) {
		ofRectangle bbox = titleFont.getStringBoundingBox(diceRollResultText, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		// Shadow
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(diceRollResultText, tx + 2, ty + 2);
		// Text (yellow/gold for dice results)
		ofSetColor(ofColor::gold);
		titleFont.drawString(diceRollResultText, tx, ty);
	}

	// FIX: Added Burst Targeting Instructions
	if (isTargetingBurst) {
		string msg = (burstChoice == 0) ? "Select Enemy to Damage (3 Holy)" : "Select Ally to Heal (3 HP)";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		ofSetColor(burstChoice == 0 ? ofColor::orange : ofColor::green);
		titleFont.drawString(msg, tx, ty);
	}

	// --- BONUS TURNS COUNTER ---
	if (currentPlayerIndex != -1) {
		Player & currentPlayer = players[currentPlayerIndex];
		if (currentPlayer.bonusTurns > 0) {
			string msg = "Extra Turns: " + ofToString(currentPlayer.bonusTurns);

			// Calculate position to the right of the End Turn button
			ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
			float tx = endTurnButtonRect.getRight() + 20 * scale;
			float ty = endTurnButtonRect.getCenter().y + bbox.height / 2;

			// Draw shadow/outline for visibility
			ofSetColor(0, 0, 0, 255);
			titleFont.drawString(msg, tx + 2, ty + 2);
			titleFont.drawString(msg, tx - 2, ty - 2);
			titleFont.drawString(msg, tx + 2, ty - 2);
			titleFont.drawString(msg, tx - 2, ty + 2);

			// Draw main text
			ofSetColor(ofColor::white);
			titleFont.drawString(msg, tx, ty);
		}
	}

	// --- DRAW DICE LABEL ---
	// Only draw the generic bottom label if NOT in initiative roll (since that has custom text)
	if (!activeDiceRolls.empty() && currentState != STATE_INITIATIVE_ROLL) {
		ofPushMatrix();

		// FIXED POSITION CALCULATION:
		// We calculate position based on the screen top, not the button.
		// Button sits at 20*scale. Height is 60. Padding 50.
		float fixedY = (20 * scale) + (60 * scale) + (50 * scale);
		float fixedX = ofGetWidth() / 2.0f;

		// Draw Shadow
		ofSetColor(0, 0, 0, 255);
		ofRectangle bounds = titleFont.getStringBoundingBox(currentDiceLabel, 0, 0);

		// Scale text
		float textScale = 0.8f;

		ofTranslate(fixedX, fixedY);
		ofScale(textScale, textScale);

		titleFont.drawString(currentDiceLabel, -bounds.width / 2 + 3, 3); // Shadow offset

		// Draw Main Text (Gold)
		ofSetColor(255, 215, 0);
		titleFont.drawString(currentDiceLabel, -bounds.width / 2, 0);

		ofPopMatrix();
	}

	// --- RENEWED INSPIRATION UI (Text & Buttons) ---
	if (isSelectingRenewedInspiration) {
		// 1. Draw Top Instruction Text
		string msg = "Select cards to discard (Draw 2 each)";
		ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
		float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
		float ty = ofGetHeight() * 0.25f;

		// Shadow
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(msg, tx + 2, ty + 2);
		// Text
		ofSetColor(ofColor::lightGreen);
		titleFont.drawString(msg, tx, ty);

		// 2. Draw Control Panel (Background for Buttons)
		float panelW = 240;
		float panelH = 70;
		float panelX = riConfirmBtn.x - 20;
		float panelY = riConfirmBtn.y - 10;

		ofSetColor(50, 50, 50, 240); // Grey background
		ofDrawRectRounded(panelX, panelY, panelW, panelH, 10);

		// 3. Draw Confirm Button
		ofSetColor(0, 180, 0); // Green
		if (riConfirmBtn.inside(ofGetMouseX(), ofGetMouseY())) ofSetColor(0, 220, 0);
		ofDrawRectRounded(riConfirmBtn, 8);

		ofSetColor(255);
		ofRectangle cBox = uiFont.getStringBoundingBox("Accept", 0, 0);
		uiFont.drawString("Accept", riConfirmBtn.getCenter().x - cBox.width / 2, riConfirmBtn.getCenter().y + cBox.height / 2);

		// 4. Draw Cancel Button
		ofSetColor(180, 0, 0); // Red
		if (riCancelBtn.inside(ofGetMouseX(), ofGetMouseY())) ofSetColor(220, 0, 0);
		ofDrawRectRounded(riCancelBtn, 8);

		ofSetColor(255);
		ofRectangle xBox = uiFont.getStringBoundingBox("Cancel", 0, 0);
		uiFont.drawString("Cancel", riCancelBtn.getCenter().x - xBox.width / 2, riCancelBtn.getCenter().y + xBox.height / 2);
	}
	// --- DEBUG: DRAW FPS ---
	ofDrawBitmapString("FPS: " + ofToString(ofGetFrameRate(), 2), 10, 20);
}
//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
	// Initialize hover tracking variables
	int newHoverType = HOVER_NONE;
	int newHoverGridX = -1;
	int newHoverGridY = -1;
	int newHoverCardIndex = -1;

	// 1. Reset to default at the start of the check
	currentCursor = CURSOR_DEFAULT;
	isShowingTooltip = false; // Reset tooltip state every frame

	// 2. Check for "Clickable" things (Buttons)
	bool overPauseMenuButton = false;
	bool overMainMenuButton = false;
	bool overSettingsButton = false;
	if (currentState == STATE_PAUSED) {
		overPauseMenuButton = pauseMenuResumeButton.inside(x, y) || pauseMenuSettingsButton.inside(x, y) || pauseMenuQuitButton.inside(x, y);
	}
	if (currentState == STATE_MAIN_MENU) {
		overMainMenuButton = mainMenuPlayAIButton.inside(x, y) || mainMenuMultiplayerButton.inside(x, y) || mainMenuSettingsButton.inside(x, y) || mainMenuQuitButton.inside(x, y);
	}
	if (currentState == STATE_SETTINGS) {
		overSettingsButton = settingsBackButton.inside(x, y) || settingsResLeftButton.inside(x, y) || settingsResRightButton.inside(x, y) || settingsFrameLeftButton.inside(x, y) || settingsFrameRightButton.inside(x, y) || settingsFullscreenButton.inside(x, y);
	}
	if (endTurnButtonRect.inside(x, y) || overMainMenuButton || overSettingsButton || overPauseMenuButton ||
		[&]() {
			for (const auto & ui : activeMinionUIs) {
				if (ui.deckRect.inside(x, y) || ui.discardRect.inside(x, y)) return true;
			}
			return false;
		}()) {
		currentCursor = CURSOR_CLICK;
	}

	// Check deck/discard hover for glow (for current player - works in both gameplay and drafting)
	if (!players.empty() && currentPlayerIndex >= 0 && (currentState == STATE_GAMEPLAY || currentState == STATE_DRAFTING)) {
		if (isCurrentPlayerLocal()) {
			if (p0_deckRect.inside(x, y)) {
				newHoverType = HOVER_DECK;
			} else if (p0_discardRect.inside(x, y)) {
				newHoverType = HOVER_DISCARD;
			}
		}
	}

	// 3. Check for "Draggable" things (Cards in hand)
	if (!players.empty() && currentPlayerIndex >= 0) {
		Player & p = players[currentPlayerIndex];
		float handBaseCardWidth = 120;
		float aspectRatio = 585.0f / 409.0f;
		float baseCardHeight = handBaseCardWidth * aspectRatio;

		for (size_t i = 0; i < p.hand.size(); i++) {
			Card & c = p.hand[i];
			float w = handBaseCardWidth * c.currentScale;
			float h = baseCardHeight * c.currentScale;
			ofRectangle cardRect(c.currentPos.x - w / 2, c.currentPos.y - h / 2, w, h);

			if (cardRect.inside(x, y)) {
				currentCursor = CURSOR_GRAB;
				if (p.playerID == myLocalPlayerID && newHoverType == HOVER_NONE) {
					newHoverType = HOVER_HAND_CARD;
					newHoverCardIndex = i;
				}
			}
		}
	}

	// 4. Check Board Interactions (Strict)
	if (currentState == STATE_GAMEPLAY && currentCursor == CURSOR_DEFAULT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x);
		int gy = floor(boardPos.y);

		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			bool isTargetingMode = (draggedCardIndex != -1) || (selectedCardIndex != -1) || isTargetingMagicBolt || isTargetingTeleport || isTargetingHellhound || isTargetingChainLightning || isTargetingAmnesia || isTargetingDoubleHanded || isTargetingTortoiseDamage;
			bool isMovingMode = (playerAction == PIECE_SELECTED);

			if (!isTargetingMode && !players.empty() && currentPlayerIndex >= 0) {
				Player & p = players[currentPlayerIndex];
				if (p.x == gx && p.y == gy) {
					currentCursor = CURSOR_CLICK;
					if (p.playerID == myLocalPlayerID && newHoverType == HOVER_NONE) {
						newHoverType = HOVER_UNIT;
						newHoverGridX = gx;
						newHoverGridY = gy;
					}
					goto cursor_check_done;
				}
			}
			if (isMovingMode && board[gx][gy].isHighlighted) {
				currentCursor = CURSOR_CLICK;
				goto cursor_check_done;
			}
			if (isTargetingMode && board[gx][gy].isTargetable) {
				currentCursor = CURSOR_CLICK;
				goto cursor_check_done;
			}
		}
	}
cursor_check_done:;

	// --- LOGIC UPDATES ---
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
		// Update target highlights on hover change (when not dragging)
		if (draggedCardIndex == -1) {
			if (hoveredCardIndex != lastHoveredCardIndex) {
				lastHoveredCardIndex = hoveredCardIndex;
				if (hoveredCardIndex != -1) {
					calculateTargetHighlights(hoveredCardIndex);
					// Send hover packet to show opponent the card targeting
					updateAndSendHover(HOVER_HAND_CARD, -1, -1, hoveredCardIndex);
				} else {
					// Hover cleared -> clear highlights
					clearHighlights();
					updateAndSendHover(HOVER_NONE);
				}
			}
		}
		// --- DYNAMIC CARD TOOLTIP LOGIC ---
		if (hoveredCardIndex != -1) {
			Card & c = currentPlayer.hand[hoveredCardIndex];

			if (c.type == CARD_MASTER_FIST) {
				// Calculate potential damage
				int dmg = 0;
				std::vector<std::string> handAttackNames = { "Punch", "Bash", "Drain Punch", "Master Fist", "Flurry of Fists", "Giant Magic Hand" };
				for (const auto & pileCard : currentPlayer.discardPile) {
					for (const auto & name : handAttackNames) {
						if (pileCard.name == name) {
							dmg += 2;
							break;
						}
					}
				}
				if (currentPlayer.flurryOfFistsActive) dmg *= 2;

				// Show as tooltip
				isShowingTooltip = true;
				tooltipPos = glm::vec2(x, y - 40); // Slightly above cursor
				tooltipText = "Current Damage: " + ofToString(dmg) + " Phys";
			}
		}

		for (size_t i = 0; i < currentPlayer.hand.size(); i++) {
			// Only enlarge cards when WE are hovering them, not when opponent hovers
			bool isLocallyHovered = (static_cast<int>(i) == hoveredCardIndex);
			currentPlayer.hand[i].targetScale = isLocallyHovered ? 2.0f : 1.5f;
		}

		int activeCardForHighlight = -1;
		if (isTargetingTeleport)
			activeCardForHighlight = pendingTeleportCardIndex;
		else if (isTargetingHellhound)
			activeCardForHighlight = hellhoundCardIndex;
		else if (isTargetingChainLightning)
			activeCardForHighlight = chainLightningCardIndex;
		else if (isTargetingAmnesia)
			activeCardForHighlight = pendingAmnesiaCardIndex;
		else if (isTargetingDoubleHanded)
			activeCardForHighlight = pendingDoubleHandedCardIndex;
		else if (isTargetingMagicBolt)
			activeCardForHighlight = magicBoltCardIndex;
		else if (isTargetingDeath)
			activeCardForHighlight = deathCardIndex;
		else if (isTargetingHeal)
			activeCardForHighlight = healCardIndex;
		// ---------------------
		else if (selectedCardIndex != -1)
			activeCardForHighlight = selectedCardIndex;
		else
			activeCardForHighlight = hoveredCardIndex;

		calculateTargetHighlights(activeCardForHighlight);
		isHoveringEndTurn = endTurnButtonRect.inside(x, y);

		if (rerollButtonRect.inside(x, y)) {
			currentCursor = CURSOR_CLICK;
		}

		// --- HOVER LOGIC (Piles & Tooltips) ---
		PileViewMode newHoveredPileType = VIEW_NONE;
		int newHoveredPileIndex = -1;

		int localId = myLocalPlayerID;
		int opponentId = (myLocalPlayerID == 0) ? 1 : 0;
		if (p0_deckRect.inside(x, y)) {
			newHoveredPileType = VIEW_DECK;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == localId) newHoveredPileIndex = i;
		} else if (p0_discardRect.inside(x, y)) {
			newHoveredPileType = VIEW_DISCARD;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == localId) newHoveredPileIndex = i;
		} else if (p1_deckRect.inside(x, y)) {
			newHoveredPileType = VIEW_DECK;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == opponentId) newHoveredPileIndex = i;
		} else if (p1_discardRect.inside(x, y)) {
			newHoveredPileType = VIEW_DISCARD;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == opponentId) newHoveredPileIndex = i;
		}

		if (newHoveredPileIndex == -1) {
			for (const auto & ui : activeMinionUIs) {
				if (ui.deckRect.inside(x, y)) {
					newHoveredPileType = VIEW_DECK;
					newHoveredPileIndex = ui.playerIndex;
					break;
				}
				if (ui.discardRect.inside(x, y)) {
					newHoveredPileType = VIEW_DISCARD;
					newHoveredPileIndex = ui.playerIndex;
					break;
				}
			}
		}

		if (newHoveredPileIndex != -1) {
			if (!isHoveringPile || newHoveredPileIndex != hoveredPilePlayerIndex || newHoveredPileType != hoveredPileType) {
				isHoveringPile = true;
				isShowingPileView = false;
				hoveredPileType = newHoveredPileType;
				hoveredPilePlayerIndex = newHoveredPileIndex;
				pileHoverStartTime = ofGetElapsedTimef();
			}
			currentCursor = CURSOR_CLICK;
		} else {
			isHoveringPile = false;
			if (isShowingPileView && !pileViewRect.inside(x, y)) {
				isShowingPileView = false;
				currentPileView = VIEW_NONE;
				currentPileViewPlayerIndex = -1;
			}
		}

		// --- TOOLTIP LOGIC ---
		ofVec2f boardPosForTooltip = mouseToBoard(x, y);
		int tooltipGX = floor(boardPosForTooltip.x);
		int tooltipGY = floor(boardPosForTooltip.y);
		int unitIndexAtMouse = -1;
		if (tooltipGX >= 0 && tooltipGX < BOARD_WIDTH && tooltipGY >= 0 && tooltipGY < BOARD_HEIGHT) {
			if (board[tooltipGX][tooltipGY].hasPlayer) {
				for (int i = 0; i < (int)players.size(); ++i) {
					if (players[i].x == tooltipGX && players[i].y == tooltipGY) {
						unitIndexAtMouse = i;
						break;
					}
				}
			}
		}

		if (unitIndexAtMouse != hoveredUnitIndex) {
			hoveredUnitIndex = unitIndexAtMouse;
			isHoveringUnit = (hoveredUnitIndex != -1);
			unitHoverStartTime = isHoveringUnit ? ofGetElapsedTimef() : 0.0f;
		}

		// 1. Pile Tooltip
		if (isHoveringPile && !isShowingPileView) {
			Player * hoveredPlayer = getPlayer(hoveredPilePlayerIndex);
			if (hoveredPlayer) {
				isShowingTooltip = true;
				tooltipPos = glm::vec2(x, y);
				tooltipText = ofToString(hoveredPileType == VIEW_DECK ? hoveredPlayer->deck.size() : hoveredPlayer->discardPile.size()) + " cards";
			}
		}
		// 2. Unit Tooltip
		else if (hoveredUnitIndex != -1 && !isHoveringPile) {
			Player * up = getPlayer(hoveredUnitIndex);
			if (up) {
				isShowingTooltip = true;
				tooltipPos = glm::vec2(x, y);
				tooltipText = getPlayerDisplayName(hoveredUnitIndex);
				if (up->inTortoiseForm) {
					int rem = 5 - up->tortoiseDamageTaken;
					tooltipText += " [Tortoise: " + ofToString(rem) + "/5 HP]";
				}
				if (up->inGhostForm) {
					int rem = 4 - up->ghostDamageTaken;
					tooltipText += " [Ghost: " + ofToString(rem) + "/4 HP]";
				}

				// Show AP-adjacent, status effects, and weaknesses for any unit (minions and players)
				{
					std::vector<std::string> unitStatusLines;
					// AP-adjacent/status buffs
					if (up->nextTurnAPBonus > 0) unitStatusLines.push_back(std::string("+") + ofToString(up->nextTurnAPBonus) + " AP Next Turn");
					if (up->nextTurnD10AP) unitStatusLines.push_back("D10 AP");
					if (up->strengthenElementsTurnsRemaining > 0) unitStatusLines.push_back(std::string("Elem Buff (") + ofToString(up->strengthenElementsTurnsRemaining) + ")");
					// Show combined luck (permanent + passive)
					int passive = computePassiveLuck(hoveredUnitIndex);
					int totalLuck = up->luck + passive;
					if (totalLuck > 0) unitStatusLines.push_back(std::string("+") + ofToString(totalLuck) + " Luck");

					// Sprint: Kick free indicator
					if (up->freeKickTurns > 0) unitStatusLines.push_back(std::string("Kick: Free (") + ofToString(up->freeKickTurns) + ")");

					// Minion/Unit AP roll hints
					if (up->isAssistant) {
						unitStatusLines.push_back("AP: Coinflip");
					} else if (up->isWolf) {
						unitStatusLines.push_back("AP: 1d10");
					} else if (up->isHellhound) {
						unitStatusLines.push_back("AP: 2d6");
					} else if (up->isDemon) {
						unitStatusLines.push_back("AP: 4d4");
					} else if (up->isKobold) {
						unitStatusLines.push_back("AP: 1d4");
					} else if (up->isWallUnit) {
						if (up->isMagicWallUnit)
							unitStatusLines.push_back("AP: 1d6 (Magic Wall)");
						else
							unitStatusLines.push_back("AP: 1d4 (Wall Unit)");
					} else if (up->isKoboldKing) {
						unitStatusLines.push_back("AP: 1d6 (Kobold King)");
					} else if (up->isMinion) {
						// Generic minion fallback
						unitStatusLines.push_back("AP: 1d6");
					}

					// Status effects
					if (up->sleepTurnsRemaining > 0) unitStatusLines.push_back(std::string("Sleep (") + ofToString(up->sleepTurnsRemaining) + ")");
					if (up->isParalyzed) unitStatusLines.push_back(std::string("Paralyzed"));
					if (up->onFire) unitStatusLines.push_back(std::string("On Fire"));
					if (up->isPoisoned) unitStatusLines.push_back(std::string("Poisoned"));
					if (up->summonedOnTurnCycle == globalTurnCounter) unitStatusLines.push_back(std::string("Summoning Sickness"));

					// Regeneration
					if (up->hasRegeneration) unitStatusLines.push_back(std::string("Regeneration"));

					// Weaknesses / vulnerabilities (aggregate, max 2x per damage type)
					bool vulnHoly = false;
					bool vulnPiercing = false;
					// Sources: unit types and cards in deck/discard
					if (up->isHellhound || up->isDemon || up->isSkeleton) vulnHoly = true;
					if (up->inGhostForm) vulnHoly = true;
					// Call for Wolves -> Piercing
					bool hasCallForWolves = false;
					for (const auto & c : up->deck)
						if (c.type == CARD_CALL_FOR_WOLVES) {
							hasCallForWolves = true;
							break;
						}
					if (!hasCallForWolves)
						for (const auto & c : up->discardPile)
							if (c.type == CARD_CALL_FOR_WOLVES) {
								hasCallForWolves = true;
								break;
							}
					if (hasCallForWolves) vulnPiercing = true;
					// Vampire Bite -> Holy
					bool hasVampireBite = false;
					for (const auto & c : up->deck)
						if (c.type == CARD_VAMPIRE_BITE) {
							hasVampireBite = true;
							break;
						}
					if (!hasVampireBite)
						for (const auto & c : up->discardPile)
							if (c.type == CARD_VAMPIRE_BITE) {
								hasVampireBite = true;
								break;
							}
					if (hasVampireBite) vulnHoly = true;

					if (vulnHoly || vulnPiercing) {
						std::vector<std::string> vulnNames;
						if (vulnPiercing) vulnNames.push_back("Piercing");
						if (vulnHoly) vulnNames.push_back("Holy");
						// Format: "Weakness 2x: Piercing, Holy"
						std::string s = std::string("Weakness 2x: ");
						for (size_t vi = 0; vi < vulnNames.size(); ++vi) {
							if (vi > 0) s += ", ";
							s += vulnNames[vi];
						}
						unitStatusLines.push_back(s);
					}

					if (!unitStatusLines.empty()) {
						tooltipText += " [";
						for (size_t si = 0; si < unitStatusLines.size(); ++si) {
							if (si > 0) tooltipText += ", ";
							tooltipText += unitStatusLines[si];
						}
						tooltipText += "]";
					}
				}

				// If unit (minion or player) has regeneration, inject into tooltip
				if (up->hasRegeneration) {
					// Avoid duplicate "Regeneration" if it was already added to unitStatusLines
					if (tooltipText.find("Regeneration") == std::string::npos) {
						size_t openPos = tooltipText.find('[');
						if (openPos != std::string::npos) {
							size_t closePos = tooltipText.rfind(']');
							if (closePos != std::string::npos) {
								if (closePos > openPos + 1) // already has contents
									tooltipText.insert(closePos, ", Regeneration");
								else // empty brackets
									tooltipText.insert(closePos, "Regeneration");
							} else {
								tooltipText += " [Regeneration]";
							}
						} // end check for existing Regeneration
					} // end hasRegeneration
				}
			}
		}
		// 3. SHIELD BAR SEGMENT TOOLTIP (Global Check for P0, P1, and Minions)
		else {
			// Helper to check hover against cached shield rects (requires rect storage in Player or finding UI coords)
			// Since Player struct is data-only, we rely on checking if mouse is inside the visual areas calculated in draw.
			// Ideally, store these rects in ofApp member variables during draw(), or recalculate them here.
			// For simplicity and performance, we'll iterate activeMinionUIs (minions) and manually check P0/P1 rects.

			// Note: This requires 'shieldRects' vector in MinionUI or similar storage.
			// Assuming we add a temporary check mechanism here using the same math as drawHealthBar/drawMinionStatusBars.

			float scale = ofGetHeight() / 1080.0f;

			// A. Check Main Players (P0/P1)
			auto checkMainPlayerShields = [&](Player & p, float x, float y) {
				float barW = 220 * scale;
				float barH = 50 * scale; // shield bar height
				float fontScale = 1.0f;

				// Reconstruct Position (Matches drawHealthBar)
				// P0 (Green) is bottom right: ofGetWidth() - (220 * scale) - (50 * scale), ofGetHeight() - (65 * scale) - (40 * scale)
				// P1 (Red) is top left: 40 * scale, 40 * scale

				float startX, startY;
				bool isTop = (p.playerID == 1 || (p.playerID > 1 && p.ownerID == 1 && !p.isMinion)); // Simple heuristic for P1
				if (p.playerID == 0) { // P0
					startX = ofGetWidth() - (220 * scale) - (50 * scale);
					float hpY = ofGetHeight() - (65 * scale) - (40 * scale);
					startY = hpY - 5 * scale; // Bar grows UP from HP
					// Need to account for form bar pushing it further?
					if (p.inGhostForm || p.inTortoiseForm) startY -= (45 * scale) + (5 * scale);
				} else { // P1
					startX = 40 * scale;
					float hpY = 40 * scale;
					startY = hpY + (65 * scale) + (5 * scale); // Bar grows DOWN from HP
					if (p.inGhostForm || p.inTortoiseForm) startY += (45 * scale) + (5 * scale);
				}

				// The 'combined' bar logic isn't in drawHealthBar yet (it draws separate bars).
				// We need to implement the COMBINED bar in drawHealthBar first to make this hover logic match.
				// Since we haven't updated drawHealthBar yet, this section is a placeholder.
				// See Step 2 below where we implement the combined bar drawing.
			};

			// B. Check Minions
			for (const auto & ui : activeMinionUIs) {
				if (ui.healthBar.inside(x, y)) { // Using the rect stored in drawMinionManagerUI
					// Calculate segments based on player stats
					Player & p = players[ui.playerIndex];
					float totalW = ui.healthBar.width;
					float segW = totalW * 0.20f;
					float localX = x - ui.healthBar.x;

					// Skip HP segment (first segment)
					float hpW = totalW - (segW * ((p.block > 0) + (p.fortification > 0) + (p.barrier > 0) + (p.ward > 0)));
					if (localX < hpW) continue; // Hovering HP

					float checkX = hpW;
					if (p.block > 0) {
						if (x >= ui.healthBar.x + checkX && x < ui.healthBar.x + checkX + segW) {
							isShowingTooltip = true;
							tooltipPos = { (float)x, (float)y };
							tooltipText = "Block (Physical)";
						}
						checkX += segW;
					}
					if (p.fortification > 0) {
						if (x >= ui.healthBar.x + checkX && x < ui.healthBar.x + checkX + segW) {
							isShowingTooltip = true;
							tooltipPos = { (float)x, (float)y };
							tooltipText = "Fortification (Physical/Piercing)";
						}
						checkX += segW;
					}
					if (p.barrier > 0) {
						if (x >= ui.healthBar.x + checkX && x < ui.healthBar.x + checkX + segW) {
							isShowingTooltip = true;
							tooltipPos = { (float)x, (float)y };
							tooltipText = "Barrier (Non-Physical)";
						}
						checkX += segW;
					}
					if (p.ward > 0) {
						if (x >= ui.healthBar.x + checkX && x < ui.healthBar.x + checkX + segW) {
							isShowingTooltip = true;
							tooltipPos = { (float)x, (float)y };
							tooltipText = "Ward (All Damage)";
						}
						checkX += segW;
					}
				}
			}
		}
		break;
	}
	case STATE_MAIN_MENU: {
		mainMenuHoveredIndex = -1;
		if (mainMenuPlayAIButton.inside(x, y))
			mainMenuHoveredIndex = 0;
		else if (mainMenuHostButton.inside(x, y))
			mainMenuHoveredIndex = 1;
		else if (mainMenuSettingsButton.inside(x, y))
			mainMenuHoveredIndex = 2;
		else if (mainMenuQuitButton.inside(x, y))
			mainMenuHoveredIndex = 3;
		else if (mainMenuInviteButton.inside(x, y))
			mainMenuHoveredIndex = 4;
		else
			mainMenuHoveredIndex = -1;
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
	case STATE_INITIATIVE_ROLL:
	case STATE_DRAFTING: {
		// Allow pile hovering during draft
		PileViewMode newHoveredPileType = VIEW_NONE;
		int newHoveredPileIndex = -1;

		int localId = myLocalPlayerID;
		int opponentId = (myLocalPlayerID == 0) ? 1 : 0;
		if (p0_deckRect.inside(x, y)) {
			newHoveredPileType = VIEW_DECK;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == localId) newHoveredPileIndex = i;
		} else if (p0_discardRect.inside(x, y)) {
			newHoveredPileType = VIEW_DISCARD;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == localId) newHoveredPileIndex = i;
		} else if (p1_deckRect.inside(x, y)) {
			newHoveredPileType = VIEW_DECK;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == opponentId) newHoveredPileIndex = i;
		} else if (p1_discardRect.inside(x, y)) {
			newHoveredPileType = VIEW_DISCARD;
			for (int i = 0; i < (int)players.size(); i++)
				if (players[i].playerID == opponentId) newHoveredPileIndex = i;
		}

		if (newHoveredPileIndex == -1) {
			for (const auto & ui : activeMinionUIs) {
				if (ui.deckRect.inside(x, y)) {
					newHoveredPileType = VIEW_DECK;
					newHoveredPileIndex = ui.playerIndex;
					break;
				}
				if (ui.discardRect.inside(x, y)) {
					newHoveredPileType = VIEW_DISCARD;
					newHoveredPileIndex = ui.playerIndex;
					break;
				}
			}
		}

		if (newHoveredPileIndex != -1) {
			if (!isHoveringPile || newHoveredPileIndex != hoveredPilePlayerIndex || newHoveredPileType != hoveredPileType) {
				isHoveringPile = true;
				isShowingPileView = false;
				hoveredPileType = newHoveredPileType;
				hoveredPilePlayerIndex = newHoveredPileIndex;
				pileHoverStartTime = ofGetElapsedTimef();
			}
			currentCursor = CURSOR_CLICK;
		} else {
			isHoveringPile = false;
			if (isShowingPileView && !pileViewRect.inside(x, y)) {
				isShowingPileView = false;
				currentPileView = VIEW_NONE;
				currentPileViewPlayerIndex = -1;
			}
		}
		break;
	}
	}

	// Update and send hover state to opponent if changed
	updateAndSendHover(static_cast<HoverType>(newHoverType), newHoverGridX, newHoverGridY, newHoverCardIndex);
}
// ----------------- FULL mousePressed FUNCTION -----------------
void ofApp::mousePressed(int x, int y, int button) {
	// Handle chat clicking (if chat is visible)
	if (currentState == STATE_GAMEPLAY && isMultiplayer && button == OF_MOUSE_BUTTON_LEFT) {
		float currentTime = ofGetElapsedTimef();
		bool shouldShowChat = isChatOpen || (currentTime - lastChatInteractionTime < chatVisibilityDuration);

		if (shouldShowChat) {
			// Check if clicking inside chat window
			if (chatWindowRect.inside(x, y)) {
				float scale = ofGetHeight() / 1080.0f;
				float chatBoxHeight = isChatMinimized ? 120 * scale : 250 * scale;
				float tabHeight = 25 * scale;
				float tabWidth = 80 * scale;
				float chatX = chatWindowRect.x;
				float chatY = chatWindowRect.y + chatWindowRect.height;

				// Check if clicking on Chat tab
				if (ofRectangle(chatX, chatY - chatBoxHeight - tabHeight, tabWidth, tabHeight).inside(x, y)) {
					currentChatTab = ChatTab::CHAT;
					lastChatInteractionTime = ofGetElapsedTimef();
					return;
				}
				// Check if clicking on Log tab
				if (ofRectangle(chatX + tabWidth + 2, chatY - chatBoxHeight - tabHeight, tabWidth, tabHeight).inside(x, y)) {
					currentChatTab = ChatTab::LOG;
					lastChatInteractionTime = ofGetElapsedTimef();
					return;
				}
				// Clicking inside chat window keeps it open
				lastChatInteractionTime = ofGetElapsedTimef();
				return;
			} else if (isChatOpen) {
				// Clicking outside chat window closes it
				isChatOpen = false;
				isChatMinimized = true;
				chatInput = "";
				return;
			}
		}
	}

	// Click-to-dismiss played card animation
	if (button == OF_MOUSE_BUTTON_LEFT && !activePlayedCardAnimations.empty()) {
		float handBaseCardWidth = 120.0f;
		float aspectRatio = 585.0f / 409.0f;
		float baseCardHeight = handBaseCardWidth * aspectRatio;
		for (auto it = activePlayedCardAnimations.begin(); it != activePlayedCardAnimations.end(); ++it) {
			float w = handBaseCardWidth * it->currentScale;
			float h = baseCardHeight * it->currentScale;
			ofRectangle animRect(it->pos.x - w / 2.0f, it->pos.y - h / 2.0f, w, h);
			if (animRect.inside(x, y)) {
				activePlayedCardAnimations.erase(it);
				return;
			}
		}
	}

	if (currentState == STATE_DRAFTING && button == OF_MOUSE_BUTTON_LEFT) {
		if (draftAcceptLocked) {
			ofLogNotice("Draft") << "DRAFT CLICK IGNORED: accept already sent.";
			return;
		}
		// Card Dimensions (Must match drawDraftScreen)
		float cardW = 340;
		float cardH = cardW * 1.4f;
		float spacing = 60;
		float startX = (ofGetWidth() - (3 * cardW + 2 * spacing)) / 2;
		float startY = ofGetHeight() / 2 - cardH / 2;

		// Determine logic for this draft phase
		int requiredPicks = 1;
		if (!isInGameDraft && draftStage == 0) requiredPicks = 2; // Setup Class 1 needs 2 picks

		ofLogNotice("Draft") << "DRAFT CLICK: mousePos=(" << x << "," << y << ") selections=" << (int)selectedDraftIndices.size() << " required=" << requiredPicks << " inGameDraft=" << (int)isInGameDraft << " draftStage=" << draftStage << " acceptRect=(" << draftAcceptButtonRect.x << "," << draftAcceptButtonRect.y << "," << draftAcceptButtonRect.width << "," << draftAcceptButtonRect.height << ")";

		// 1. Card Clicking
		// Only the drafting player may select cards (multiplayer)
		if (!(isMultiplayer && players[draftPlayerIndex].playerID != myLocalPlayerID)) {
			for (int i = 0; i < draftOptions.size(); ++i) {
				float cx = startX + i * (cardW + spacing);
				if (ofRectangle(cx, startY, cardW, cardH).inside(x, y)) {
					auto it = std::find(selectedDraftIndices.begin(), selectedDraftIndices.end(), i);
					bool nowSelected = false;
					if (it != selectedDraftIndices.end()) {
						selectedDraftIndices.erase(it); // Deselect
						nowSelected = false;
					} else {
						if ((int)selectedDraftIndices.size() < requiredPicks) {
							selectedDraftIndices.push_back(i);
							nowSelected = true;
						}
					}

					// If multiplayer, notify host (or forward to clients if host) about selection toggle
					if (isMultiplayer) {
						DraftActionPacket pkt = {};
						pkt.type = PKT_DRAFT_ACTION;
						pkt.playerID = myLocalPlayerID;
						pkt.actionType = 0; // Select / Toggle
						pkt.selectFlag = nowSelected ? 1 : 0;
						pkt.optionIndex = i;
						pkt.draftPlayerIdx = draftPlayerIndex;
						steamManager.sendPacket(&pkt, sizeof(pkt));
						ofLogNotice("Network") << "Sent draft select toggle: opt=" << i << " sel=" << (int)pkt.selectFlag;
					}

					return;
				}
			}
		}

		// 2. Accept Button Clicking
		if ((int)selectedDraftIndices.size() == requiredPicks && draftAcceptButtonRect.inside(x, y)) {
			ofLogNotice("Draft") << "ACCEPT BUTTON CLICKED: selections=" << (int)selectedDraftIndices.size() << " required=" << requiredPicks << " rect=(" << draftAcceptButtonRect.x << "," << draftAcceptButtonRect.y << "," << draftAcceptButtonRect.width << "," << draftAcceptButtonRect.height << ") mousePos=(" << x << "," << y << ")";

			if (draftAcceptLocked) {
				ofLogNotice("Draft") << "ACCEPT BLOCKED: already accepted for this draft screen.";
				return;
			}

			// Only allow the drafting player to accept
			if (isMultiplayer && players[draftPlayerIndex].playerID != myLocalPlayerID) {
				ofLogNotice("Draft") << "ACCEPT BLOCKED: Not drafting player (me=" << myLocalPlayerID << " drafting=" << players[draftPlayerIndex].playerID << ")";
				return;
			}

			draftAcceptLocked = true;

			// If client in multiplayer, send selection to host and return
			if (isClient()) {
				DraftActionPacket pkt = {};
				pkt.type = PKT_DRAFT_ACTION;
				pkt.playerID = myLocalPlayerID;
				pkt.actionType = 1; // AcceptDraft
				pkt.selectFlag = 0;
				pkt.draftPlayerIdx = draftPlayerIndex;
				pkt.classTier = currentDraftClassTier;
				pkt.numSelected = (int)selectedDraftIndices.size();
				pkt.selectedIdx0 = (pkt.numSelected > 0 && selectedDraftIndices[0] >= 0 && selectedDraftIndices[0] < 3) ? currentDraftOptionPoolIndices[selectedDraftIndices[0]] : -1;
				pkt.selectedIdx1 = (pkt.numSelected > 1 && selectedDraftIndices[1] >= 0 && selectedDraftIndices[1] < 3) ? currentDraftOptionPoolIndices[selectedDraftIndices[1]] : -1;
				pkt.selectedIdx2 = (pkt.numSelected > 2 && selectedDraftIndices[2] >= 0 && selectedDraftIndices[2] < 3) ? currentDraftOptionPoolIndices[selectedDraftIndices[2]] : -1;
				ofLogNotice("Draft") << "CLIENT: Sending AcceptDraft with picks: " << (int)pkt.selectedIdx0 << "," << (int)pkt.selectedIdx1 << "," << (int)pkt.selectedIdx2 << " classTier=" << (int)pkt.classTier;
				bool ok = steamManager.sendPacket(&pkt, sizeof(pkt));
				if (!ok) {
					ofLogError("Network") << "Failed to send AcceptDraft to host (packet not sent)";
				} else {
					ofLogNotice("Network") << "Client sent AcceptDraft to host (" << pkt.numSelected << " picks)";
				}
				return;
			}

			Player & p = players[draftPlayerIndex];

			// Determine copies per card
			int copiesPerCard = 1;
			if (!isInGameDraft && draftStage == 0) copiesPerCard = 2; // Setup Class 1 gets 2 copies

			// Add cards to deck
			for (int pickedIndex : selectedDraftIndices) {
				for (int k = 0; k < copiesPerCard; k++) {
					p.deck.push_back(draftOptions[pickedIndex]);
				}
			}

			// HOST: Send PKT_DRAFT_ACTION to inform clients BEFORE shuffling
			// This ensures clients receive Accept and add cards before shuffle packet arrives
			if (isHost()) {
				DraftActionPacket acceptPkt = {};
				acceptPkt.type = PKT_DRAFT_ACTION;
				acceptPkt.playerID = myLocalPlayerID;
				acceptPkt.actionType = 1; // Accept
				acceptPkt.draftPlayerIdx = draftPlayerIndex;
				acceptPkt.classTier = currentDraftClassTier;
				acceptPkt.numSelected = (uint8_t)selectedDraftIndices.size();
				acceptPkt.selectedIdx0 = (selectedDraftIndices.size() > 0 && selectedDraftIndices[0] >= 0 && selectedDraftIndices[0] < 3) ? currentDraftOptionPoolIndices[selectedDraftIndices[0]] : -1;
				acceptPkt.selectedIdx1 = (selectedDraftIndices.size() > 1 && selectedDraftIndices[1] >= 0 && selectedDraftIndices[1] < 3) ? currentDraftOptionPoolIndices[selectedDraftIndices[1]] : -1;
				acceptPkt.selectedIdx2 = (selectedDraftIndices.size() > 2 && selectedDraftIndices[2] >= 0 && selectedDraftIndices[2] < 3) ? currentDraftOptionPoolIndices[selectedDraftIndices[2]] : -1;
				acceptPkt.optionIndex = -1;
				acceptPkt.selectFlag = 0;
				steamManager.sendPacket(&acceptPkt, sizeof(acceptPkt));
				ofLogNotice("Network") << "Host: Sent local draft Accept to client (player=" << draftPlayerIndex << " picks=" << (int)acceptPkt.numSelected << ")";
			}

			// FIX: Shuffle new cards into deck immediately (sends shuffle packet AFTER Accept)
			shuffleGameVector(p.deck, draftPlayerIndex);

			// Cleanup UI state
			selectedDraftIndices.clear();
			draftOptions.clear();

			// Logic: In-Game Draft (Key Pickup)
			if (isInGameDraft) {
				isInGameDraft = false;
				currentState = STATE_GAMEPLAY;
				return;
			}

			// Logic: Setup Draft
			draftStage++;
			if (draftStage == 1) {
				generateDraftOptions(2); // Move to Class 2
			} else {
				// Current player finished
				int nextPlayerIdx = (draftPlayerIndex + 1) % 2;

				if (players[nextPlayerIdx].deck.empty()) {
					// Other player needs to draft
					draftPlayerIndex = nextPlayerIdx;
					draftStage = 0;
					generateDraftOptions(1); // Start Class 1
				} else {
					// Both done. Start gameplay.
					currentPlayerIndex = nextPlayerIdx;
					currentState = STATE_GAMEPLAY;

					// NOTE: Decks were already shuffled when each player accepted their picks.
					// No additional shuffle needed here to avoid desync.

					// For the first turn, call continueNewTurn() directly to avoid incrementing currentPlayerIndex
					// (There's no previous turn to end, so we skip the cleanup/advancement logic)
					continueNewTurn();

					// Inform clients that drafting has ended (AFTER continueNewTurn so currentPlayerIndex is correct)
					if (isHost()) {
						DraftStatePacket dsp;
						dsp.type = PKT_DRAFT_STATE;
						dsp.playerID = myLocalPlayerID;
						dsp.classTier = 0;
						dsp.draftPlayerIdx = -1;
						dsp.picksRemaining = draftPicksRemaining;
						dsp.draftStage = draftStage;
						dsp.isInGameDraft = isInGameDraft ? 1 : 0;
						dsp.currentPlayerIndex = currentPlayerIndex;
						steamManager.sendPacket(&dsp, sizeof(dsp));
						ofLogNotice("Network") << "Host sent DraftStatePacket (draft->gameplay): curPlayer=" << dsp.currentPlayerIndex;
					}

					// Send TurnStart packet with authoritative AP values
					if (isHost()) {
						std::vector<DiceRoll> newAP;
						for (size_t di = 0; di < activeDiceRolls.size(); ++di) {
							const DiceRoll & dr = activeDiceRolls[di];
							if (dr.associatedUnit == currentPlayerIndex && dr.purpose == PURPOSE_AP) {
								newAP.push_back(dr);
							}
						}
						TurnStartPacket tpk = {};
						tpk.type = PKT_TURN_START;
						tpk.playerID = myLocalPlayerID;
						tpk.currentPlayerIndex = currentPlayerIndex;
						int pkCount = 0;
						int32_t total = 0;
						for (size_t i = 0; i < newAP.size() && pkCount < 8; ++i) {
							tpk.rawResults[pkCount] = (uint8_t)newAP[i].rawResult;
							tpk.finalResults[pkCount] = (uint8_t)newAP[i].result;
							pkCount++;
							total += newAP[i].result;
						}
						tpk.diceNum = (uint8_t)pkCount;
						tpk.diceSides = (uint8_t)(pkCount > 0 ? newAP[0].sides : 6);
						tpk.purpose = (uint8_t)PURPOSE_AP;
						tpk.finalTotal = total;
						steamManager.sendPacket(&tpk, sizeof(tpk));
						ofLogNotice("Network") << "Host sent TurnStart (mousePressed): player=" << tpk.currentPlayerIndex << " dice=" << (int)tpk.diceNum << " total=" << tpk.finalTotal;
						for (int i = 0; i < pkCount; ++i) {
							ofLogNotice("Network") << "  Host sending dice[" << i << "]: raw=" << (int)tpk.rawResults[i] << " final=" << (int)tpk.finalResults[i];
						}

						// DEBUGGING: Log host's checksum at the same moment client will calculate theirs
						if (globalTurnCounter == 0) {
							int64_t hostChecksum = calculateChecksum();
							ofLogNotice("Checksum") << "Host checksum after TurnStart send (turn 0): " << hostChecksum;
						}
					}
				}
			}
			return;
		}
		return;
	}
	// Debug: Log all mouse presses when targeting teleport
	if (isTargetingTeleport) {
		ofLogNotice("Teleport") << "mousePressed called! x=" << x << " y=" << y << " button=" << button;
	}

	// --- BRANCH: IN-GAME KEY DRAFT / CHAINED DRAFT ---
	if (isInGameDraft) {
		// Reference drafting player
		Player & p = players[draftPlayerIndex];

		// Shuffle deck
		shuffleGameVector(p.deck, draftPlayerIndex);

		// CHECK QUEUE: Are there more drafts pending?
		if (!pendingDraftQueue.empty()) {
			// Pop next and stay in drafting
			int nextClass = pendingDraftQueue.front();
			pendingDraftQueue.erase(pendingDraftQueue.begin());

			generateDraftOptions(nextClass);
			draftPicksRemaining = 1;
			selectedDraftIndices.clear();
			// draftPlayerIndex stays same
			// currentState stays STATE_DRAFTING

			ofLogNotice("Draft") << "Continuing chain. Next Class: " << nextClass;
			return;
		}

		// No more drafts, return to game
		isInGameDraft = false;
		currentState = STATE_GAMEPLAY;
		return;
	}

	// If paused, handle pause-menu clicks immediately and ignore gameplay handlers.
	if (currentState == STATE_PAUSED) {
		if (button == OF_MOUSE_BUTTON_LEFT) {
			if (pauseMenuResumeButton.inside(x, y)) {
				currentState = pausedFromState;
				return;
			}
			if (pauseMenuSettingsButton.inside(x, y)) {
				stateBeforeSettings = STATE_PAUSED;
				currentState = STATE_SETTINGS;
				return;
			}
			if (pauseMenuQuitButton.inside(x, y)) {
				// 1. Disconnect from Steam (Stops the auto-join loop)
				steamManager.leaveLobby();
				isMultiplayer = false;
				cleanupGame();
				currentState = STATE_MAIN_MENU;
			}
		}
		// For other buttons or mouse buttons, swallow the click so gameplay handlers don't run
		return;
	}

	// --- EARLY HANDLER: KOBOLD PLACEMENT (take precedence like wolves) ---
	if (isPlacingKobolds && !isWaitingForKoboldDice) {
		// Only allow placement clicks; swallow all other clicks while placing kobolds
		if (button == OF_MOUSE_BUTTON_LEFT) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gx = floor(boardPos.x), gy = floor(boardPos.y);
			if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
				if (!board[gx][gy].hasWall && !board[gx][gy].hasPlayer) {
					int dist = abs(gx - koboldPlacementSourceX) + abs(gy - koboldPlacementSourceY);
					if (dist == 1) {
						// Spawn kobold (mirrors the placement logic in the main handler)
						koboldSummonCount++;
						Player kobold;
						kobold.playerID = 300 + (int)players.size();
						kobold.x = gx;
						kobold.y = gy;
						kobold.maxHealth = 1;
						kobold.health = 1;
						kobold.isMinion = true;
						kobold.isKobold = true;
						kobold.isSkeleton = false;
						kobold.ownerID = players[currentPlayerIndex].isMinion ? players[currentPlayerIndex].ownerID : players[currentPlayerIndex].playerID;
						// Give summoned kobolds summoning sickness this cycle and record ordering
						kobold.summonedOnTurnCycle = globalTurnCounter;
						kobold.summonOrder = ++nextSummonOrder;
						Card hb, pu, callCard;
						for (const auto & c : allCards) {
							if (c.name == "Hand Block") hb = c;
							if (c.name == "Punch") pu = c;
							if (c.type == CARD_CALL_FOR_KOBOLDS) callCard = c;
						}
						board[gx][gy].hasPlayer = true;
						players.push_back(kobold);
						int newKoboldIdx = (int)players.size() - 1;
						shuffleGameVector(players[newKoboldIdx].deck, newKoboldIdx);
						ofLogNotice("Summon") << "Summoned Kobold " << koboldSummonCount;
						koboldsRemainingToPlace--;
						if (koboldsRemainingToPlace <= 0) {
							isPlacingKobolds = false;
							int myID = players[currentPlayerIndex].playerID;
							std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
								int ownerA = a.isMinion ? a.ownerID : a.playerID;
								int ownerB = b.isMinion ? b.ownerID : b.playerID;
								if (ownerA != ownerB) return ownerA < ownerB;
								if (a.isMinion && !b.isMinion) return true;
								if (!a.isMinion && b.isMinion) return false;
								return a.summonOrder < b.summonOrder;
							});
							for (size_t i = 0; i < players.size(); i++) {
								if (players[i].playerID == myID) {
									currentPlayerIndex = i;
									break;
								}
							}
						}
						invalidateTargetCache();
						return; // Click handled
					}
				}
			}
			// Click ignored while placing kobolds
			return;
		}
	}

	// ==============================================================================
	// PHASE 1: MODAL UI INTERRUPTS
	// ==============================================================================

	// --- Card Encyclopedia UI ---
	if (isCardEncyclopediaOpen && button == OF_MOUSE_BUTTON_LEFT) {
		if (isMultiplayer && !isHost()) {
			addGameLog("Debug spawner is host-only in multiplayer.");
			isCardEncyclopediaOpen = false;
			isCardSpawnerOpen = false;
			return;
		}
		if (encyclopediaCloseButton.inside(x, y)) {
			isCardEncyclopediaOpen = false;
			return;
		}

		// Check if clicking on a card in the encyclopedia
		if (encyclopediaRect.inside(x, y)) {
			const float kBaseCardWidth = 120.0f;
			const float kCardAspectRatio = 1.4f;
			const float kBaseCardHeight = kBaseCardWidth * kCardAspectRatio;

			float panelX = encyclopediaRect.x;
			float panelY = encyclopediaRect.y;
			float contentY = panelY + 60;
			float contentHeight = encyclopediaRect.height - 70;
			float cardScale = 1.2f;
			float cardW = kBaseCardWidth * cardScale;
			float cardH = kBaseCardHeight * cardScale;
			float padding = 15.0f;

			int cols = std::max(1, (int)floor((encyclopediaRect.width - 2 * padding) / (cardW + padding)));
			float startX = panelX + padding + ((encyclopediaRect.width - 2 * padding) - (cols * (cardW + padding) - padding)) / 2.0f;

			// Sort cards same as in draw
			std::vector<Card> sortedCards = allCards;
			std::sort(sortedCards.begin(), sortedCards.end(), [](const Card & a, const Card & b) {
				if (a.cost != b.cost) return a.cost < b.cost;
				return a.name < b.name;
			});

			int row = 0;
			int col = 0;
			for (size_t i = 0; i < sortedCards.size(); i++) {
				float drawX = startX + col * (cardW + padding);
				float drawY = contentY + row * (cardH + padding) - encyclopediaScrollOffset;

				if (drawY >= contentY && drawY + cardH <= contentY + contentHeight) {
					ofRectangle cardRect(drawX, drawY, cardW, cardH);
					if (cardRect.inside(x, y)) {
						// Add this card to current player's hand
						for (int q = 0; q < cardSpawnerQuantity; q++) {
							players[currentPlayerIndex].hand.push_back(sortedCards[i]);
							players[currentPlayerIndex].hand.back().currentPos = ofVec2f(ofGetWidth() / 2, 0);
						}
						spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y),
							"+" + ofToString(cardSpawnerQuantity) + "x " + sortedCards[i].name, ofColor::cyan);
						if (isMultiplayer && isHost()) {
							sendSnapshotToClient();
						}
						return;
					}
				}

				col++;
				if (col >= cols) {
					col = 0;
					row++;
				}
			}
		}

		// Clicked outside - close
		if (!encyclopediaRect.inside(x, y)) {
			isCardEncyclopediaOpen = false;
		}
		return;
	}

	// --- Card Spawner UI ---
	if (isCardSpawnerOpen && button == OF_MOUSE_BUTTON_LEFT) {
		if (isMultiplayer && !isHost()) {
			addGameLog("Debug spawner is host-only in multiplayer.");
			isCardSpawnerOpen = false;
			return;
		}
		if (cardSpawnerCloseButton.inside(x, y)) {
			isCardSpawnerOpen = false;
			return;
		}

		if (cardSpawnerPlusButton.inside(x, y)) {
			cardSpawnerQuantity = std::min(cardSpawnerQuantity + 1, 99);
			return;
		}

		if (cardSpawnerMinusButton.inside(x, y)) {
			cardSpawnerQuantity = std::max(cardSpawnerQuantity - 1, 1);
			return;
		}

		if (cardSpawnerEncyclopediaButton.inside(x, y)) {
			isCardEncyclopediaOpen = true;
			encyclopediaScrollOffset = 0;
			return;
		}

		// Check if clicking on a suggestion
		if (!cardSpawnerInput.empty() && !filteredCards.empty()) {
			float barWidth = 600.0f;
			float barHeight = 50.0f;
			float barX = (ofGetWidth() - barWidth) / 2.0f;
			float barY = ofGetHeight() * 0.15f;
			float inputWidth = barWidth - 180.0f;
			float suggestionY = barY + barHeight + 5.0f;
			float suggestionHeight = 35.0f;
			int maxSuggestions = std::min((int)filteredCards.size(), 8);

			for (int i = 0; i < maxSuggestions; i++) {
				float itemY = suggestionY + 5 + i * suggestionHeight;
				ofRectangle itemRect(barX + 5, itemY, inputWidth - 10, suggestionHeight - 2);
				if (itemRect.inside(x, y)) {
					// Add this card
					for (int q = 0; q < cardSpawnerQuantity; q++) {
						players[currentPlayerIndex].hand.push_back(filteredCards[i]);
						players[currentPlayerIndex].hand.back().currentPos = ofVec2f(ofGetWidth() / 2, 0);
					}
					spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y),
						"+" + ofToString(cardSpawnerQuantity) + "x " + filteredCards[i].name, ofColor::cyan);

					// Send DrawCards packet to sync spawned cards with opponent
					if (isMultiplayer) {
						DrawCardsPacket dcpkt = {};
						dcpkt.type = PKT_DRAW_CARDS;
						dcpkt.playerID = myLocalPlayerID;
						dcpkt.playerIndex = currentPlayerIndex;
						dcpkt.numCards = std::min(cardSpawnerQuantity, 3);

						// Include the card names
						for (int q = 0; q < cardSpawnerQuantity && q < 3; q++) {
							strncpy(dcpkt.cardNames[q], filteredCards[i].name.c_str(), 63);
							dcpkt.cardNames[q][63] = '\0';
						}

						steamManager.sendPacket(&dcpkt, sizeof(dcpkt));
						ofLogNotice("Debug") << "Card Spawner: Sent DrawCards packet for " << cardSpawnerQuantity << "x " << filteredCards[i].name;
					}

					isCardSpawnerOpen = false;
					return;
				}
			}
		}

		// Don't close when clicking inside the bar area
		float barWidth = 600.0f;
		float barHeight = 50.0f;
		float barX = (ofGetWidth() - barWidth) / 2.0f;
		float barY = ofGetHeight() * 0.15f;
		float suggestionHeight = 35.0f;
		int maxSuggestions = std::min((int)filteredCards.size(), 8);
		float totalHeight = barHeight + (filteredCards.empty() ? 0 : suggestionHeight * maxSuggestions + 15);
		ofRectangle fullArea(barX, barY, barWidth, totalHeight);

		if (!fullArea.inside(x, y)) {
			isCardSpawnerOpen = false;
		}
		return;
	}

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
			int damage = 5;

			// --- MITIGATION LOGIC (Magic) ---
			// 1. Barrier (Blocks Magic)
			int barrierDamage = std::min(targetPlayer->barrier, damage);
			targetPlayer->barrier -= barrierDamage;
			damage -= barrierDamage;

			// 2. Ward (Blocks Everything)
			if (damage > 0) {
				int wardDamage = std::min(targetPlayer->ward, damage);
				targetPlayer->ward -= wardDamage;
				damage -= wardDamage;
			}

			// 3. Health
			if (damage > 0) {
				targetPlayer->health -= damage;
				spawnFloatingText(gridToWorld(targetPlayer->x, targetPlayer->y), "-" + ofToString(damage) + " Magic", ofColor::red);

				// Form tracking
				if (targetPlayer->inTortoiseForm) {
					targetPlayer->tortoiseDamageTaken += damage;
					if (targetPlayer->tortoiseDamageTaken >= 5) {
						targetPlayer->inTortoiseForm = false;
						targetPlayer->tortoiseDamageTaken = 0;
						targetPlayer->discardPile.push_back(targetPlayer->tortoiseFormCard);
						spawnFloatingText(gridToWorld(targetPlayer->x, targetPlayer->y) + glm::vec3(0, 0.5f, 0), "Form Ended!", ofColor::darkGreen);
					}
				}
				if (targetPlayer->inGhostForm) {
					targetPlayer->ghostDamageTaken += damage;
					if (targetPlayer->ghostDamageTaken >= 4) {
						targetPlayer->inGhostForm = false;
						targetPlayer->ghostDamageTaken = 0;
						targetPlayer->discardPile.push_back(targetPlayer->ghostFormCard);
						spawnFloatingText(gridToWorld(targetPlayer->x, targetPlayer->y) + glm::vec3(0, 0.5f, 0), "Ghost Form Broken!", ofColor::white);
						if (board[targetPlayer->x][targetPlayer->y].hasWall) targetPlayer->health = 0;
					}
				}
			} else {
				spawnFloatingText(gridToWorld(targetPlayer->x, targetPlayer->y), "Absorbed", ofColor::gray);
			}

			ofLogNotice("MagicBlast") << "Player " << targetPlayer->playerID << " chose Damage (Magic).";
			choiceMade = true;
		}

		if (magicBlastDiscardButton.inside(x, y)) {
			// Remove top card of deck (if any) and place into discard
			if (!targetPlayer->deck.empty()) {
				Card removed = targetPlayer->deck.back();
				targetPlayer->deck.pop_back();
				targetPlayer->discardPile.push_back(removed);
				spawnFloatingText(gridToWorld(targetPlayer->x, targetPlayer->y), "Discarded: " + removed.name, ofColor::white);
				ofLogNotice("MagicBlast") << "Player " << targetPlayer->playerID << " discarded top card: " << removed.name;
			} else {
				spawnFloatingText(gridToWorld(targetPlayer->x, targetPlayer->y), "No Cards", ofColor::gray);
				ofLogNotice("MagicBlast") << "Player " << targetPlayer->playerID << " attempted to discard but deck empty.";
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

			pendingDispelRollResult = startDiceRoll(1, 20, PURPOSE_BARRIER_GAIN, "Dispel: Barrier Amount");

			Player & p = players[currentPlayerIndex];
			Card dispelCard = p.hand[pendingDispelCardIndex];

			// Send network packet with menuChoice=1 (barrier)
			if (isMultiplayer) {
				sendActionPacket(pendingDispelCardIndex, -1, -1, dispelCard.cost, 1);
			}

			currentAP -= dispelCard.cost;
			p.discardPile.push_back(dispelCard);
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

	// --- Train Menu Interaction ---
	if (isTrainMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {
		Player & p = players[currentPlayerIndex];

		if (trainBtnAP.inside(x, y)) {
			int cardIndex = pendingTrainCardIndex;
			int cost = (cardIndex >= 0 && cardIndex < (int)p.hand.size()) ? p.hand[cardIndex].cost : 0;
			std::string cardName = (cardIndex >= 0 && cardIndex < (int)p.hand.size()) ? p.hand[cardIndex].name : "";
			if (isMultiplayer) {
				sendActionPacket(cardIndex, -1, -1, cost, 1, cardName);
			}

			// Option A: +3 AP Next Turn
			p.nextTurnAPBonus += 3;
			spawnFloatingText(gridToWorld(p.x, p.y), "Training: AP", ofColor::yellow);

			// Pay cost & cleanup
			currentAP -= cost;
			p.playedCardsPile.push_back(p.hand[cardIndex]);
			// (Replicate check could go here if standard logic isn't used)
			p.hand.erase(p.hand.begin() + cardIndex);

			isTrainMenuOpen = false;
			pendingTrainCardIndex = -1;
			calculateTargetHighlights();
		} else if (trainBtnDraft.inside(x, y)) {
			int cardIndex = pendingTrainCardIndex;
			int cost = (cardIndex >= 0 && cardIndex < (int)p.hand.size()) ? p.hand[cardIndex].cost : 0;
			std::string cardName = (cardIndex >= 0 && cardIndex < (int)p.hand.size()) ? p.hand[cardIndex].name : "";
			if (isMultiplayer) {
				sendActionPacket(cardIndex, -1, -1, cost, 2, cardName);
			}

			// Option B: Draft Class 1
			// Pay cost first
			currentAP -= cost;
			p.playedCardsPile.push_back(p.hand[cardIndex]);
			p.hand.erase(p.hand.begin() + cardIndex);

			isTrainMenuOpen = false;
			pendingTrainCardIndex = -1;

			// Setup Draft
			isInGameDraft = true;
			draftPlayerIndex = currentPlayerIndex;
			generateDraftOptions(1); // Class 1
			draftPicksRemaining = 1;
			selectedDraftIndices.clear();
			draftStage = 0;

			currentState = STATE_DRAFTING;
		}
		// Click outside does not cancel (must choose)
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
			glm::vec3 targetPos = gridToWorld(target->x, target->y);

			if (isSelfTarget) {
				ofLogNotice("Wisdom Boon") << "Granting " << effectValue << " Block to Self.";
				target->block += effectValue;

				// CHANGE: From Cyan to Gray
				spawnFloatingText(targetPos, "+" + ofToString(effectValue) + " Block", ofColor::gray);
			} else {
				ofLogNotice("Wisdom Boon") << "Dealing " << effectValue << " Magic Damage to Enemy.";
				int dmg = effectValue;

				// Barrier Interaction
				int barrierDmg = std::min(target->barrier, dmg);
				target->barrier -= barrierDmg;
				dmg -= barrierDmg;

				// Ward Interaction
				if (dmg > 0) {
					int wardDmg = std::min(target->ward, dmg);
					target->ward -= wardDmg;
					dmg -= wardDmg;
				}

				// Health Interaction & Text
				if (dmg > 0) {
					target->health -= dmg;
					// CHANGE: Red Text + " Magic"
					spawnFloatingText(targetPos, "-" + ofToString(dmg) + " Magic", ofColor::red);
				} else {
					spawnFloatingText(targetPos, "Absorbed", ofColor::gray);
				}
			}
			choiceMade = true;
		} else if (!wisdomMenuRect.inside(x, y)) {
			cancelWisdomBoon();
			return;
		}

		if (choiceMade) {
			int cardIndex = pendingWisdomBoonCardIndex;
			int cost = (cardIndex >= 0 && cardIndex < (int)caster.hand.size()) ? caster.hand[cardIndex].cost : 0;
			std::string cardName = (cardIndex >= 0 && cardIndex < (int)caster.hand.size()) ? caster.hand[cardIndex].name : "";
			if (isMultiplayer) {
				sendActionPacket(cardIndex, target->x, target->y, cost, 1, cardName);
			}

			currentAP -= cost;
			Card playedCard = caster.hand[cardIndex];
			caster.playedCardsPile.push_back(playedCard);

			if (caster.isReplicatePending) {
				Card dup = playedCard;
				caster.playedCardsPile.push_back(dup);
				ofLogNotice("Replicate") << "Wisdom Boon Replicated.";
				caster.isReplicatePending = false;
			}
			caster.hand.erase(caster.hand.begin() + pendingWisdomBoonCardIndex);

			// Trigger Shell Spike if in Tortoise Form (only for self-target which gives block)
			if (isSelfTarget) {
				tryTriggerShellSpike();
			}

			cancelWisdomBoon();
			calculateTargetHighlights();
		}
		return;
	}

	// --- 1e2. Burst of Light Menu ---
	if (isBurstMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {

		// Recalculate validity to prevent clicking the gray button
		bool hasValidEnemy = false;
		Player & caster = players[currentPlayerIndex];
		glm::vec2 casterPos(caster.x, caster.y);
		int casterOwner = caster.isMinion ? caster.ownerID : caster.playerID;

		for (const auto & p : players) {
			int pOwner = p.isMinion ? p.ownerID : p.playerID;
			if (pOwner == casterOwner) continue; // Skip allies
			TargetInfo info = isLosTargetValid(casterPos, glm::vec2(p.x, p.y), 9999.0f, CARD_BURST_OF_LIGHT);
			if (info.reason == VALID) {
				hasValidEnemy = true;
				break;
			}
		}

		if (burstBtnDamage.inside(x, y)) {
			// ONLY ALLOW IF VALID
			if (hasValidEnemy) {
				isBurstMenuOpen = false;
				isTargetingBurst = true;
				burstChoice = 0; // damage
				calculateTargetHighlights(pendingBurstCardIndex);
			} else {
				// Optional: Feedback sound or small text "No Valid Target"
			}
		} else if (burstBtnHeal.inside(x, y)) {
			isBurstMenuOpen = false;
			isTargetingBurst = true;
			burstChoice = 1; // heal
			calculateTargetHighlights(pendingBurstCardIndex);
		} else if (!burstMenuRect.inside(x, y)) {
			cancelBurst();
		}
		return;
	}
	// --- 1f. Double-Handed Menu ---
	if (isDoubleHandedMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {
		if (btnAddPunches.inside(x, y)) {
			// Store choice and enter targeting mode
			pendingDoubleHandedChoice = "Punch";
			isDoubleHandedMenuOpen = false;
			isTargetingDoubleHanded = true;
			calculateTargetHighlights(pendingDoubleHandedCardIndex);
		} else if (btnAddBlocks.inside(x, y)) {
			// Store choice and enter targeting mode
			pendingDoubleHandedChoice = "Hand Block";
			isDoubleHandedMenuOpen = false;
			isTargetingDoubleHanded = true;
			calculateTargetHighlights(pendingDoubleHandedCardIndex);
		} else if (!doubleHandedMenuRect.inside(x, y)) {
			// Clicked outside menu -> Cancel
			cancelDoubleHanded();
		}
		return; // Stop other mouse interactions
	}
	// --- 1f2. Double-Handed Targeting ---
	if (isTargetingDoubleHanded && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x), gy = floor(boardPos.y);
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			if (board[gx][gy].isTargetable) {
				// Find target player
				int targetIndex = -1;
				for (size_t i = 0; i < players.size(); i++) {
					if (players[i].x == gx && players[i].y == gy) {
						targetIndex = (int)i;
						break;
					}
				}
				if (targetIndex != -1) {
					pendingDoubleHandedTargetIndex = targetIndex;
					int cardIndex = pendingDoubleHandedCardIndex;
					Player & caster = players[currentPlayerIndex];
					int cost = (cardIndex >= 0 && cardIndex < (int)caster.hand.size()) ? caster.hand[cardIndex].cost : 0;
					std::string cardName = (cardIndex >= 0 && cardIndex < (int)caster.hand.size()) ? caster.hand[cardIndex].name : "";
					if (isMultiplayer) {
						int menuChoice = (pendingDoubleHandedChoice == "Punch") ? 1 : 2;
						sendActionPacket(cardIndex, gx, gy, cost, menuChoice, cardName);
					}
					resolveDoubleHanded(pendingDoubleHandedChoice);
				}
			}
		}
		return;
	}

	// --- 1f3. Burst Targeting ---
	if (isTargetingBurst && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x), gy = floor(boardPos.y);

		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			// Check board targetability (Green Highlight)
			if (board[gx][gy].isTargetable) {
				int targetIndex = -1;
				for (size_t i = 0; i < players.size(); i++) {
					if (players[i].x == gx && players[i].y == gy) {
						targetIndex = (int)i;
						break;
					}
				}

				Player & caster = players[currentPlayerIndex];
				if (targetIndex != -1) {
					Player * target = getPlayer(targetIndex);
					if (!target) {
						cancelBurst();
						return;
					}

					int casterOwner = caster.isMinion ? caster.ownerID : caster.playerID;
					int targetOwner = target->isMinion ? target->ownerID : target->playerID;

					// --- STRICT TARGET VALIDATION ---
					if (burstChoice == 0) { // DAMAGE
						if (casterOwner == targetOwner) {
							spawnFloatingText(gridToWorld(target->x, target->y), "Cannot damage ally", ofColor::red);
							return; // Don't cancel, let them pick again
						}
						applyDamageTo(*target, 3, DAMAGE_HOLY, currentPlayerIndex);
					} else { // HEAL
						if (casterOwner != targetOwner) {
							spawnFloatingText(gridToWorld(target->x, target->y), "Cannot heal enemy", ofColor::red);
							return; // Don't cancel, let them pick again
						}
						int healAmt = 3;
						int healed = std::min(healAmt, target->maxHealth - target->health);
						if (healed > 0) {
							target->health += healed;
							spawnFloatingText(gridToWorld(target->x, target->y), "+" + ofToString(healed) + " HP", ofColor::green);
						} else {
							spawnFloatingText(gridToWorld(target->x, target->y), "Full HP", ofColor::gray);
						}
					}

					int cardIndex = pendingBurstCardIndex;
					int cost = (cardIndex >= 0 && cardIndex < (int)caster.hand.size()) ? caster.hand[cardIndex].cost : 0;
					std::string cardName = (cardIndex >= 0 && cardIndex < (int)caster.hand.size()) ? caster.hand[cardIndex].name : "";
					if (isMultiplayer) {
						int menuChoice = (burstChoice == 0) ? 1 : 2;
						sendActionPacket(cardIndex, gx, gy, cost, menuChoice, cardName);
					}

					// Consume AP and cleanup
					currentAP -= cost;
					Card playedCard = caster.hand[pendingBurstCardIndex];
					caster.playedCardsPile.push_back(playedCard);
					if (caster.isReplicatePending) {
						caster.playedCardsPile.push_back(playedCard);
						caster.isReplicatePending = false;
					}
					caster.hand.erase(caster.hand.begin() + pendingBurstCardIndex);

					isTargetingBurst = false;
					pendingBurstCardIndex = -1;
					calculateTargetHighlights();
				}
			}
		}
		return;
	}

	// --- Giant Magic Hand Menu ---
	if (isMagicHandMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {
		float w = 500, h = 250;
		float mx = ofGetWidth() / 2 - w / 2, my = ofGetHeight() / 2 - h / 2;
		float btnW = 200, btnH = 80;
		float spacing = 40;
		float startX = mx + (w - (btnW * 2 + spacing)) / 2;
		float btnY = my + 120;

		ofRectangle btnPush(startX, btnY, btnW, btnH);
		ofRectangle btnPull(startX + btnW + spacing, btnY, btnW, btnH);

		if (btnPush.inside(x, y)) {
			resolveMagicHandPush();
		} else if (btnPull.inside(x, y)) {
			resolveMagicHandPull();
		} else if (!wisdomMenuRect.inside(x, y)) {
			cancelMagicHand();
		}
		return;
	}
	// --- 1f3. Amnesia Menu ---
	if (isAmnesiaMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {
		Player & caster = players[currentPlayerIndex];
		Card & amnesiaCard = caster.hand[pendingAmnesiaCardIndex];

		if (amnesiaBtnSelf.inside(x, y)) {
			// Use on self - directly start dice roll
			isAmnesiaMenuOpen = false;
			amnesiaTargetPlayerIndex = currentPlayerIndex;
			pendingAmnesiaRollResult = startDiceRoll(amnesiaCard.numDice, amnesiaCard.diceSides, PURPOSE_DEBUG, "Amnesia: Cards to Remove");
			isWaitingForAmnesiaDice = true;
			// Consume AP and discard card now
			currentAP -= amnesiaCard.cost;
			caster.playedCardsPile.push_back(amnesiaCard);
			if (caster.isReplicatePending) {
				caster.playedCardsPile.push_back(amnesiaCard);
				caster.isReplicatePending = false;
			}
			caster.hand.erase(caster.hand.begin() + pendingAmnesiaCardIndex);
			pendingAmnesiaCardIndex = -1;
		} else if (amnesiaBtnAdjacent.inside(x, y)) {
			// Enter targeting mode for adjacent units
			isAmnesiaMenuOpen = false;
			isTargetingAmnesia = true;
			calculateTargetHighlights(pendingAmnesiaCardIndex);
		} else if (!amnesiaMenuRect.inside(x, y)) {
			// Clicked outside menu -> Cancel
			isAmnesiaMenuOpen = false;
			pendingAmnesiaCardIndex = -1;
		}
		return;
	}

	// --- RENEWED INSPIRATION: REAL-TIME SELECTION ---
	if (isSelectingRenewedInspiration && button == OF_MOUSE_BUTTON_LEFT) {

		// 1. Check Confirm Button
		if (riConfirmBtn.inside(x, y)) {
			Player & p = players[currentPlayerIndex];

			if (isMultiplayer) {
				RenewedInspirationPacket rpk = {};
				rpk.type = PKT_RENEWED_INSPIRATION;
				rpk.playerID = myLocalPlayerID;
				rpk.playerIndex = currentPlayerIndex;
				rpk.count = std::min((int)renewedSelectedHandIndices.size(), 16);
				for (int i = 0; i < rpk.count; ++i) {
					rpk.indices[i] = renewedSelectedHandIndices[i];
				}
				steamManager.sendPacket(&rpk, sizeof(rpk));
			}

			// Sort indices descending so we can delete safely from back to front
			std::sort(renewedSelectedHandIndices.begin(), renewedSelectedHandIndices.end(), std::greater<int>());

			int cardsToDraw = 0;
			for (int idx : renewedSelectedHandIndices) {
				if (idx < (int)p.hand.size()) {
					// Discard
					p.discardPile.push_back(p.hand[idx]);
					p.hand.erase(p.hand.begin() + idx);
					cardsToDraw += 2;
				}
			}

			// Remember hand size before drawing to get the new cards
			size_t handSizeBefore = p.hand.size();

			// Draw new cards
			for (int i = 0; i < cardsToDraw; i++)
				drawCard();

			// Send action packet AFTER the menu resolves so the opponent sees the card as played
			if (isMultiplayer && !p.playedCardsPile.empty()) {
				const Card & playedCard = p.playedCardsPile.back();
				sendActionPacket(-1, -1, -1, playedCard.cost, 0, playedCard.name);
			}

			// Send DrawCards packet to opponent so they know what was drawn
			if (isMultiplayer && cardsToDraw > 0) {
				DrawCardsPacket dcpkt = {};
				dcpkt.type = PKT_DRAW_CARDS;
				dcpkt.playerID = myLocalPlayerID;
				dcpkt.playerIndex = currentPlayerIndex;
				dcpkt.numCards = cardsToDraw;

				// Include the card names that were just drawn
				for (int i = 0; i < cardsToDraw && i < 3; i++) {
					size_t cardIndex = handSizeBefore + i;
					if (cardIndex < p.hand.size()) {
						strncpy(dcpkt.cardNames[i], p.hand[cardIndex].name.c_str(), 63);
						dcpkt.cardNames[i][63] = '\0';
					} else {
						dcpkt.cardNames[i][0] = '\0';
					}
				}

				steamManager.sendPacket(&dcpkt, sizeof(dcpkt));
				ofLogNotice("Network") << "Sent DrawCards packet for Renewed Inspiration: " << cardsToDraw << " cards";
			}

			// Show played card animation now that the effect is confirmed
			if (!p.playedCardsPile.empty()) {
				createCardDisplay(p.playedCardsPile.back(), currentPlayerIndex);
			}

			// Visual feedback
			spawnFloatingText(gridToWorld(p.x, p.y), "+" + ofToString(cardsToDraw) + " Cards", ofColor::cyan);

			isSelectingRenewedInspiration = false;
			return;
		}

		// 2. Check Cancel Button (Undo)
		if (riCancelBtn.inside(x, y)) {
			Player & p = players[currentPlayerIndex];
			// Refund AP
			currentAP += 2;

			// Return card to hand (pop from played pile, push back to hand)
			if (!p.playedCardsPile.empty()) {
				Card c = p.playedCardsPile.back();
				p.playedCardsPile.pop_back();
				p.hand.push_back(c);
			}

			isSelectingRenewedInspiration = false;
			return;
		}

		// 3. Check Clicking Cards in Hand (Toggle Selection)
		Player & p = players[currentPlayerIndex];
		float handBaseCardWidth = 120;
		float aspectRatio = 585.0f / 409.0f;
		float baseCardHeight = handBaseCardWidth * aspectRatio;

		// Reverse loop to check top-most cards first (standard UI practice)
		for (int i = (int)p.hand.size() - 1; i >= 0; --i) {
			Card & card = p.hand[i];

			// Use the CURRENT position (includes hover animation) for accurate clicking
			float w = handBaseCardWidth * card.currentScale;
			float h = baseCardHeight * card.currentScale;
			ofRectangle cardRect(card.currentPos.x - w / 2, card.currentPos.y - h / 2, w, h);

			if (cardRect.inside(x, y)) {
				// Check eligibility (Drawn this turn OR Copied)
				if (!card.drawnThisTurn && !card.isCopied) {
					spawnFloatingText(gridToWorld(p.x, p.y), "Must be drawn this turn", ofColor::red);
					return;
				}

				// Toggle selection
				auto it = std::find(renewedSelectedHandIndices.begin(), renewedSelectedHandIndices.end(), i);
				if (it != renewedSelectedHandIndices.end()) {
					renewedSelectedHandIndices.erase(it); // Deselect
				} else {
					renewedSelectedHandIndices.push_back(i); // Select
				}
				return; // Stop checking other cards
			}
		}
		return; // Consume click so we don't move/attack while selecting
	}
	// --- 1f4b. Tortoise Damage Targeting ---
	if (isTargetingTortoiseDamage && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x), gy = floor(boardPos.y);
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			Player & caster = players[currentPlayerIndex];

			// Check if clicking on an adjacent unit (any unit, including allies)
			int dx = abs(gx - caster.x);
			int dy = abs(gy - caster.y);
			bool isAdjacent = (dx <= 1 && dy <= 1) && (dx + dy > 0);

			if (isAdjacent) {
				// Find target at this position
				for (size_t i = 0; i < players.size(); i++) {
					Player & target = players[i];
					if (target.x == gx && target.y == gy) {
						// Allow targeting ANY adjacent unit (including friendly units)
						// Deal 3 physical damage
						int damage = 3;

						// Block mitigation
						int blockAbsorb = std::min(target.block, damage);
						target.block -= blockAbsorb;
						damage -= blockAbsorb;

						// Ward mitigation
						int wardAbsorb = std::min(target.ward, damage);
						target.ward -= wardAbsorb;
						damage -= wardAbsorb;

						// Apply to health
						if (damage > 0) {
							target.health -= damage;
							spawnFloatingText(gridToWorld(target.x, target.y),
								"-" + ofToString(damage) + " Shell Spike", ofColor::red);

							// Tortoise form damage tracking on target if they have it
							if (target.inTortoiseForm) {
								target.tortoiseDamageTaken += damage;
								if (target.tortoiseDamageTaken >= 5) {
									target.inTortoiseForm = false;
									target.tortoiseDamageTaken = 0;
									target.discardPile.push_back(target.tortoiseFormCard);
									spawnFloatingText(gridToWorld(target.x, target.y) + glm::vec3(0, 0.5f, 0),
										"Form Ended!", ofColor::darkGreen);
								}
							}

							// Check death
							if (target.health <= 0) {
								DeathMarker death;
								death.x = target.x;
								death.y = target.y;
								death.turnDied = globalTurnCounter;
								death.deck = target.deck;
								graveyard.push_back(death);
								board[target.x][target.y].hasPlayer = false;
								target.x = -1000;
							}
						} else {
							spawnFloatingText(gridToWorld(target.x, target.y), "Blocked", ofColor::gray);
						}

						isTargetingTortoiseDamage = false;
						ofLogNotice("Tortoise Form") << "Shell Spike dealt damage to adjacent unit.";
						return;
					}
				}
			}
		}
		return;
	}

	// --- 1f4. Amnesia Targeting ---
	if (isTargetingAmnesia && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x), gy = floor(boardPos.y);
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			if (board[gx][gy].isTargetable) {
				// Play amnesia on target (this triggers the dice roll in playCard)
				int cardIndex = pendingAmnesiaCardIndex;
				std::string cardName = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].name
					: "";
				int cost = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].cost
					: 0;
				CardPlayResult result = playCard(cardIndex, gx, gy);
				if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
					players[currentPlayerIndex].ap = currentAP;
					sendActionPacket(cardIndex, gx, gy, cost, 0, cardName);
				}
				isTargetingAmnesia = false;
				pendingAmnesiaCardIndex = -1;
				calculateTargetHighlights();
			}
		}
		return;
	}

	// --- 1f5. Teleport Targeting ---
	if (isTargetingTeleport && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x), gy = floor(boardPos.y);
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {

			// Use the board flag which we set in calculateTargetHighlights
			if (board[gx][gy].isTargetable) {
				Player & caster = players[currentPlayerIndex];

				if (isMultiplayer && pendingTeleportCardIndex >= 0 && pendingTeleportCardIndex < (int)caster.hand.size()) {
					std::string cardName = caster.hand[pendingTeleportCardIndex].name;
					int cost = caster.hand[pendingTeleportCardIndex].cost;
					sendActionPacket(pendingTeleportCardIndex, gx, gy, cost, 0, cardName);
				}

				// Move player
				board[caster.x][caster.y].hasPlayer = false;
				caster.x = gx;
				caster.y = gy;
				board[gx][gy].hasPlayer = true; // Set occupancy (even if wall)
				playerVisualPos = gridToWorld(gx, gy);
				invalidateTargetCache();

				spawnFloatingText(gridToWorld(gx, gy), "Teleport!", ofColor::cyan);
				ofLogNotice("Teleport") << "Teleported to (" << gx << ", " << gy << ")";

				// Remove teleport card from hand
				if (pendingTeleportCardIndex >= 0 && pendingTeleportCardIndex < (int)players[currentPlayerIndex].hand.size()) {
					players[currentPlayerIndex].hand.erase(players[currentPlayerIndex].hand.begin() + pendingTeleportCardIndex);
				}

				// Clean up state (AP was already deducted when card was first clicked)
				isTargetingTeleport = false;
				pendingTeleportCardIndex = -1;
				pendingTeleportRollResult = 0;
				calculateTargetHighlights();
			}
		}
		return;
	}

	// --- 1g. Amnesia Selection UI ---
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

	// --- 1h. Magic Bolt Targeting Click ---
	if (isTargetingMagicBolt && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x);
		int gy = floor(boardPos.y);

		// Clicked valid target?
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			if (board[gx][gy].isTargetable) {
				int cardIndex = magicBoltCardIndex;
				std::string cardName = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].name
					: "";
				int cost = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].cost
					: 0;
				CardPlayResult result = playCard(cardIndex, gx, gy);
				if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
					players[currentPlayerIndex].ap = currentAP;
					sendActionPacket(cardIndex, gx, gy, cost, 0, cardName);
				}

				// Reset State
				isTargetingMagicBolt = false;
				magicBoltCardIndex = -1;
				clearHighlights();
				return;
			}
		}

		// Clicked invalid? Cancel.
		isTargetingMagicBolt = false;
		magicBoltCardIndex = -1;
		clearHighlights();
		return;
	}

	// --- 1i. Hellhound Targeting Click ---
	if (isTargetingHellhound && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x);
		int gy = floor(boardPos.y);

		// Clicked valid target?
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			if (board[gx][gy].isTargetable) {
				// This calls the playCard logic we wrote earlier (which rolls dice)
				int cardIndex = hellhoundCardIndex;
				std::string cardName = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].name
					: "";
				int cost = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].cost
					: 0;
				CardPlayResult result = playCard(cardIndex, gx, gy);
				if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
					players[currentPlayerIndex].ap = currentAP;
					sendActionPacket(cardIndex, gx, gy, cost, 0, cardName);
				}

				// Reset State
				isTargetingHellhound = false;
				hellhoundCardIndex = -1;
				clearHighlights();
				return;
			}
		}

		// Clicked invalid? Cancel.
		isTargetingHellhound = false;
		hellhoundCardIndex = -1;
		clearHighlights();
		return;
	}

	// --- 1j. Chain Lightning Click ---
	if (isTargetingChainLightning && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x);
		int gy = floor(boardPos.y);

		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			if (board[gx][gy].isTargetable) {
				// 1. Consume Resources
				Player & caster = players[currentPlayerIndex];
				Card & c = caster.hand[chainLightningCardIndex];

				if (isMultiplayer) {
					sendActionPacket(chainLightningCardIndex, gx, gy, c.cost, 0, c.name);
				}

				currentAP -= c.cost;
				caster.playedCardsPile.push_back(c);
				if (caster.isReplicatePending) {
					caster.playedCardsPile.push_back(c);
					caster.isReplicatePending = false;
				}
				caster.cardsPlayedThisTurn.push_back(c.type);
				caster.hand.erase(caster.hand.begin() + chainLightningCardIndex);

				// 2. Start Range Roll (2d10)
				pendingChainLightningTargetTile = glm::vec2(gx, gy);
				pendingChainLightningRangeResult = startDiceRoll(2, 10, PURPOSE_RANGE, "Chain Lightning: Range");
				isWaitingForChainLightningRange = true;

				// 3. Reset State
				isTargetingChainLightning = false;
				chainLightningCardIndex = -1;
				clearHighlights();
				return;
			}
		}
		// Cancel if clicked invalid
		isTargetingChainLightning = false;
		chainLightningCardIndex = -1;
		clearHighlights();
		return;
	}
	// --- 1k. Death Targeting Click ---
	if (isTargetingDeath && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x), gy = floor(boardPos.y);
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			if (board[gx][gy].isTargetable) {
				int cardIndex = deathCardIndex;
				std::string cardName = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].name
					: "";
				int cost = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].cost
					: 0;
				CardPlayResult result = playCard(cardIndex, gx, gy);
				if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
					players[currentPlayerIndex].ap = currentAP;
					sendActionPacket(cardIndex, gx, gy, cost, 0, cardName);
				}
				isTargetingDeath = false;
				deathCardIndex = -1;
				clearHighlights();
				return;
			}
		}
		// Cancel
		isTargetingDeath = false;
		deathCardIndex = -1;
		clearHighlights();
		return;
	}

	// --- 1l. Heal Targeting Click ---
	if (isTargetingHeal && button == OF_MOUSE_BUTTON_LEFT) {
		ofVec2f boardPos = mouseToBoard(x, y);
		int gx = floor(boardPos.x), gy = floor(boardPos.y);
		if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
			if (board[gx][gy].isTargetable || (gx == players[currentPlayerIndex].x && gy == players[currentPlayerIndex].y)) {
				int cardIndex = healCardIndex;
				std::string cardName = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].name
					: "";
				int cost = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].cost
					: 0;
				CardPlayResult result = playCard(cardIndex, gx, gy);
				if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
					players[currentPlayerIndex].ap = currentAP;
					sendActionPacket(cardIndex, gx, gy, cost, 0, cardName);
				}
				isTargetingHeal = false;
				healCardIndex = -1;
				clearHighlights();
				return;
			}
		}
		// Cancel
		isTargetingHeal = false;
		healCardIndex = -1;
		clearHighlights();
		return;
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

		// --- KOBOLD PLACEMENT LOGIC (MUST BE BEFORE WOLF LOGIC) ---
		if (isPlacingKobolds && !isWaitingForKoboldDice && button == OF_MOUSE_BUTTON_LEFT) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gx = floor(boardPos.x), gy = floor(boardPos.y);

			// Validation: In bounds, Empty, Adjacent to Summoner
			if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
				if (!board[gx][gy].hasWall && !board[gx][gy].hasPlayer) {
					int dist = abs(gx - koboldPlacementSourceX) + abs(gy - koboldPlacementSourceY);
					if (dist == 1) {

						// --- SPAWN THE KOBOLD ---
						koboldSummonCount++; // Increment name counter

						Player kobold;
						kobold.playerID = 300 + (int)players.size();
						kobold.x = gx;
						kobold.y = gy;
						kobold.maxHealth = 1;
						kobold.health = 1;
						kobold.isMinion = true;
						kobold.isKobold = true;
						kobold.isSkeleton = false;

						// Set owner and summoning sickness
						kobold.ownerID = players[currentPlayerIndex].isMinion ? players[currentPlayerIndex].ownerID : players[currentPlayerIndex].playerID;
						kobold.summonedOnTurnCycle = globalTurnCounter;
						kobold.summonOrder = ++nextSummonOrder;
						Card hb, pu, callCard;
						for (const auto & c : allCards) {
							if (c.name == "Hand Block") hb = c;
							if (c.name == "Punch") pu = c;
							if (c.type == CARD_CALL_FOR_KOBOLDS) callCard = c;
						}
						kobold.deck = { hb, hb, pu, callCard };

						// Add to board
						board[gx][gy].hasPlayer = true;
						players.push_back(kobold);
						int newKoboldIdx = (int)players.size() - 1;
						shuffleGameVector(players[newKoboldIdx].deck, newKoboldIdx);

						// --- HANDLE LOGIC FLOW --
						koboldsRemainingToPlace--;
						if (koboldsRemainingToPlace > 0) {
							// still placing
						} else {
							isPlacingKobolds = false;
							koboldSummonStage = 0;
							// --- CRITICAL FIX: CAPTURE ID BEFORE SORT ---
							int myID = players[currentPlayerIndex].playerID;

							// Re-sort turn order
							std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
								int ownerA = a.isMinion ? a.ownerID : a.playerID;
								int ownerB = b.isMinion ? b.ownerID : b.playerID;
								if (ownerA != ownerB) return ownerA < ownerB;
								if (a.isMinion && !b.isMinion) return true;
								if (!a.isMinion && b.isMinion) return false;
								return a.summonOrder < b.summonOrder;
							});

							// Fix current player index
							for (size_t i = 0; i < players.size(); i++) {
								if (players[i].playerID == myID) {
									currentPlayerIndex = i;
									break;
								}
							}
						}

						return; // Click handled
					}
				}
			}
		}

		// --- WOLF PLACEMENT LOGIC ---
		if (isPlacingWolves && !isWaitingForWolfCoin && button == OF_MOUSE_BUTTON_LEFT) {
			ofVec2f boardPos = mouseToBoard(x, y);
			int gx = floor(boardPos.x), gy = floor(boardPos.y);

			// Validation: In bounds, Empty, Adjacent to Summoner
			if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
				if (!board[gx][gy].hasWall && !board[gx][gy].hasPlayer) {
					int dist = abs(gx - wolfPlacementSourceX) + abs(gy - wolfPlacementSourceY);
					if (dist == 1) {

						// --- SPAWN THE WOLF ---
						wolfSummonCount++; // Increment name counter (Wolf 1, Wolf 2)

						Player wolf;
						wolf.playerID = 200 + (int)players.size();
						wolf.x = gx;
						wolf.y = gy;
						wolf.maxHealth = 4;
						wolf.health = 4;
						wolf.isMinion = true;
						wolf.isWolf = true;

						// Set owner and summoning sickness
						wolf.ownerID = players[currentPlayerIndex].isMinion ? players[currentPlayerIndex].ownerID : players[currentPlayerIndex].playerID;
						wolf.summonedOnTurnCycle = globalTurnCounter;
						Card slashCard, callCard;
						for (const auto & c : allCards) {
							if (c.name == "Slash") slashCard = c;
							if (c.type == CARD_CALL_FOR_WOLVES) callCard = c;
						}
						wolf.deck = { slashCard, slashCard, slashCard, callCard };

						// Add to board
						board[gx][gy].hasPlayer = true;
						players.push_back(wolf);
						int newWolfIdx = (int)players.size() - 1;
						shuffleGameVector(players[newWolfIdx].deck, newWolfIdx);

						// --- HANDLE LOGIC FLOW ---

						if (wolfSummonStage == 1) {
							// First wolf placed. Now flip the coin.
							ofLogNotice("Wolves") << "Wolf 1 placed. Flipping coin for 2nd...";

							startDiceRoll(1, 2, PURPOSE_COIN_FLIP, "Flip for 2nd Wolf");

							isWaitingForWolfCoin = true;
							// Do NOT turn off isPlacingWolves yet.
						} else if (wolfSummonStage == 2) {
							// Second wolf placed. We are done.
							isPlacingWolves = false;
							wolfSummonStage = 0;

							// --- CRITICAL FIX: CAPTURE ID BEFORE SORT ---
							int myID = players[currentPlayerIndex].playerID;

							// Re-sort turn order
							std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
								int ownerA = a.isMinion ? a.ownerID : a.playerID;
								int ownerB = b.isMinion ? b.ownerID : b.playerID;
								if (ownerA != ownerB) return ownerA < ownerB;
								if (a.isMinion && !b.isMinion) return true;
								if (!a.isMinion && b.isMinion) return false;
								return a.summonOrder < b.summonOrder;
							});

							// Fix current player index
							for (size_t i = 0; i < players.size(); i++) {
								if (players[i].playerID == myID) {
									currentPlayerIndex = i;
									break;
								}
							}
						}

						return; // Click handled
					}
				}
			}
		}

		// 3a. Debug Spawn Logic
		if (isSpawningUnit && button == OF_MOUSE_BUTTON_LEFT) {
			if (isMultiplayer && !isHost()) {
				addGameLog("Debug spawning is host-only in multiplayer.");
				isSpawningUnit = false;
				return;
			}
			ofVec2f boardPos = mouseToBoard(x, y);
			int gx = floor(boardPos.x), gy = floor(boardPos.y);
			if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
				if (!board[gx][gy].hasWall && !board[gx][gy].hasPlayer) {
					Player newPlayer;
					newPlayer.x = gx;
					newPlayer.y = gy;
					newPlayer.playerID = (int)players.size();
					newPlayer.deck = allCards;
					players.push_back(newPlayer);
					int newDebugIdx = (int)players.size() - 1;
					shuffleGameVector(players[newDebugIdx].deck, newDebugIdx);
					board[gx][gy].hasPlayer = true;
					ofLogNotice("Debug") << "Spawned new player.";
					if (isMultiplayer && isHost()) {
						sendSnapshotToClient();
					}
				}
			}
			isSpawningUnit = false;
			return;
		}

		// 3b. Debug UI Interactions (Buttons)
		if (isDebugMode && button == OF_MOUSE_BUTTON_LEFT) {
			if (debugPanel.inside(x, y)) {
				if (isMultiplayer && !isHost()) {
					addGameLog("Debug tools are host-only in multiplayer.");
					return;
				}
				// Handle specific debug buttons
				if (debugDrawCardButton.inside(x, y)) {
					drawCard();
					if (isMultiplayer && isHost()) {
						sendSnapshotToClient();
					}
					return;
				}

				if (debugSpawnCardButton.inside(x, y)) {
					// Open the in-game card spawner UI
					isCardSpawnerOpen = true;
					cardSpawnerInput = "";
					filteredCards.clear();
					return;
				}

				if (debugDiceDropdownButton.inside(x, y)) {
					isDebugDiceDropdownOpen = !isDebugDiceDropdownOpen;
					return;
				}

				// Only check dropdown buttons if open
				if (isDebugDiceDropdownOpen) {
					if (debugFlipCoinButton.inside(x, y)) {
						startDiceRoll(1, 2, PURPOSE_DEBUG, "Debug Coin");
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD4Button.inside(x, y)) {
						startDiceRoll(1, 4, PURPOSE_DEBUG, "Debug D4");
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD6Button.inside(x, y)) {
						startDiceRoll(1, 6, PURPOSE_DEBUG, "Debug D6");
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD10Button.inside(x, y)) {
						startDiceRoll(1, 10, PURPOSE_DEBUG, "Debug D10");
						isDebugDiceDropdownOpen = false;
						return;
					}
					if (debugRollD20Button.inside(x, y)) {
						startDiceRoll(1, 20, PURPOSE_DEBUG, "Debug D20");
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
					if (isMultiplayer && isHost()) {
						sendSnapshotToClient();
					}
					return;
				}
				if (debugForceEndTurnButton.inside(x, y)) {
					startNewTurn();
					return;
				}

				return; // Clicked panel background
			}
		}

		// --- RENEWED INSPIRATION: REAL-TIME SELECTION ---
		if (isSelectingRenewedInspiration && button == OF_MOUSE_BUTTON_LEFT) {

			// 1. Check Confirm Button
			if (riConfirmBtn.inside(x, y)) {
				Player & p = players[currentPlayerIndex];

				// Sort indices descending so we can delete safely
				std::sort(renewedSelectedHandIndices.begin(), renewedSelectedHandIndices.end(), std::greater<int>());

				int cardsToDraw = 0;
				for (int idx : renewedSelectedHandIndices) {
					if (idx >= 0 && idx < (int)p.hand.size()) {
						// Discard
						p.discardPile.push_back(p.hand[idx]);
						p.hand.erase(p.hand.begin() + idx);
						cardsToDraw += 2;
					}
				}

				// Draw new cards
				for (int i = 0; i < cardsToDraw; i++)
					drawCard();

				// Visual feedback
				spawnFloatingText(gridToWorld(p.x, p.y), "Spark of Genius! +" + ofToString(cardsToDraw) + " Cards", ofColor::cyan);

				isSelectingRenewedInspiration = false;
				return;
			}

			// 2. Check Cancel Button (Undo)
			if (riCancelBtn.inside(x, y)) {
				Player & p = players[currentPlayerIndex];
				// Refund AP
				currentAP += 2; // Assuming cost is 2

				// Return card to hand (pop from played, push to hand)
				if (!p.playedCardsPile.empty()) {
					Card c = p.playedCardsPile.back();
					p.playedCardsPile.pop_back();
					p.hand.push_back(c); // Put it back
				}

				isSelectingRenewedInspiration = false;
				return;
			}

			// 3. Check Clicking Cards in Hand (Toggle Selection)
			Player & p = players[currentPlayerIndex];
			float handBaseCardWidth = 120;
			float aspectRatio = 585.0f / 409.0f;
			float baseCardHeight = handBaseCardWidth * aspectRatio;

			// Reverse loop to check top-most cards first (same as draw order)
			for (int i = (int)p.hand.size() - 1; i >= 0; --i) {
				Card & card = p.hand[i];

				// Use the CURRENT position (which includes hover offset) for accurate clicking
				float w = handBaseCardWidth * card.currentScale;
				float h = baseCardHeight * card.currentScale;
				ofRectangle cardRect(card.currentPos.x - w / 2, card.currentPos.y - h / 2, w, h);

				if (cardRect.inside(x, y)) {
					// Check eligibility (Drawn this turn OR Copied)
					if (!card.drawnThisTurn && !card.isCopied) {
						spawnFloatingText(gridToWorld(p.x, p.y), "Not eligible", ofColor::red);
						return;
					}

					// Toggle logic
					auto it = std::find(renewedSelectedHandIndices.begin(), renewedSelectedHandIndices.end(), i);
					if (it != renewedSelectedHandIndices.end()) {
						renewedSelectedHandIndices.erase(it); // Deselect
					} else {
						renewedSelectedHandIndices.push_back(i); // Select
					}
					return; // Stop checking other cards
				}
			}
			return; // Consume click so we don't accidentally move/attack while selecting
		}

		// 3c. STATE CHECK: Only allow gameplay interactions in STATE_GAMEPLAY
		if (currentState != STATE_GAMEPLAY) return;

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

		// 3c. TURN VALIDATION: Only allow interactions if it's the local player's turn or a minion owned by the local player
		if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
			const Player & currentPlayer = players[currentPlayerIndex];
			int controlledPlayerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
			if (controlledPlayerID != myLocalPlayerID) {
				// Not our turn - ignore all gameplay clicks
				return;
			}
		}

		// 3d. Deck Clicking (Drawing Cards)
		if (button == OF_MOUSE_BUTTON_LEFT) {

			// --- FIX: Check Minion Type for Draw Count ---
			for (const auto & ui : activeMinionUIs) {
				if (ui.playerIndex == currentPlayerIndex && ui.deckRect.inside(x, y) && !hasDrawnCardsThisTurn) {
					Player & minion = players[ui.playerIndex];

					// Minions have their own decks, but cards go to the OWNER's hand
					// Find the owner player
					int ownerIndex = -1;
					for (size_t i = 0; i < players.size(); i++) {
						if (players[i].playerID == minion.ownerID && !players[i].isMinion) {
							ownerIndex = (int)i;
							break;
						}
					}

					if (ownerIndex < 0) {
						ofLogWarning("Game") << "Minion owner not found!";
						return;
					}

					Player & owner = players[ownerIndex];

					// Default 2, Demon 3
					int drawCount = minion.isDemon ? 3 : 2;

					// Handle Hasten buff if applicable
					if (minion.nextTurnExtraDraw) {
						drawCount++;
						minion.nextTurnExtraDraw = false;
					}

					// Draw from minion's deck to owner's hand
					for (int i = 0; i < drawCount; i++) {
						// Check if minion deck needs reshuffle
						if (minion.deck.empty()) {
							if (minion.discardPile.empty()) {
								ofLogNotice("Game") << "Minion cannot draw. Both Deck and Discard are empty.";
								break;
							}
							ofLogNotice("Game") << "Minion Deck is empty. Reshuffling Discard Pile into Deck...";
							minion.deck = minion.discardPile;
							minion.discardPile.clear();
							shuffleGameVector(minion.deck, ui.playerIndex);
						}

						// Draw from minion's deck
						if (!minion.deck.empty()) {
							Card newCard = minion.deck.back();
							minion.deck.pop_back();

							// Add to owner's hand
							newCard.currentScale = 1.5f;
							newCard.targetScale = 1.5f;
							owner.hand.push_back(newCard);

							ofLogNotice("Game") << "Drew " << newCard.name << " from minion's deck to owner's hand";
						}
					}

					hasDrawnCardsThisTurn = true;
					return;
				}
			}

			if (players.empty() || currentPlayerIndex < 0) return;

			// --- NEW LOGIC: FIND PLAYERS BY ID, NOT INDEX ---
			Player * p0 = nullptr;
			Player * p1 = nullptr;
			for (auto & p : players) {
				if (p.playerID == 0) p0 = &p;
				if (p.playerID == 1) p1 = &p;
			}

			// If we can't find the main players, exit
			if (!p0 || !p1) return;

			// In multiplayer, use local player perspective: bottom deck is always "mine"
			Player * localPlayer = nullptr;
			if (isMultiplayer) {
				localPlayer = (myLocalPlayerID == 0) ? p0 : p1;
			}

			Player & activePlayer = players[currentPlayerIndex];
			// Only consider it the player's turn for main-deck clicks when the active unit
			// is the actual player (not a minion). Minions must click their own UI deck.
			bool isLocalPlayersTurn = false;
			if (isMultiplayer) {
				// In multiplayer, check if it's the local player's turn
				isLocalPlayersTurn = (activePlayer.playerID == myLocalPlayerID && !activePlayer.isMinion);
			} else {
				// Single player: P0 clicks bottom deck
				isLocalPlayersTurn = (activePlayer.playerID == 0 && !activePlayer.isMinion);
			}

			// Bottom Deck Click (Always Local Player in Multiplayer)
			if (p0_deckRect.inside(x, y) && isLocalPlayersTurn && !hasDrawnCardsThisTurn) {
				Player * playerToDraw = isMultiplayer ? localPlayer : p0;
				int localPlayerIndex = currentPlayerIndex;
				if (isMultiplayer) {
					for (size_t i = 0; i < players.size(); i++) {
						if (players[i].playerID == myLocalPlayerID && !players[i].isMinion) {
							localPlayerIndex = (int)i;
							break;
						}
					}
				}
				int baseDraw = playerToDraw->isDemon ? 3 : 2;
				int cardsToDraw = playerToDraw->nextTurnExtraDraw ? (baseDraw + 1) : baseDraw;
				if (playerToDraw->nextTurnExtraDraw) ofLogNotice("Game") << "Hasten Effect: Drawing 3 cards!";

				// Remember hand size before drawing to get the new cards
				size_t handSizeBefore = playerToDraw->hand.size();

				// Draw cards locally (client-side prediction)
				for (int i = 0; i < cardsToDraw; i++)
					drawCard();

				// Send packet to opponent so they see the draw too
				if (isMultiplayer) {
					DrawCardsPacket dcpkt = {};
					dcpkt.type = PKT_DRAW_CARDS;
					dcpkt.playerID = myLocalPlayerID;
					dcpkt.playerIndex = localPlayerIndex;
					dcpkt.numCards = cardsToDraw;

					// Include the card names that were just drawn
					for (int i = 0; i < cardsToDraw && i < 3; i++) {
						size_t cardIndex = handSizeBefore + i;
						if (cardIndex < playerToDraw->hand.size()) {
							strncpy(dcpkt.cardNames[i], playerToDraw->hand[cardIndex].name.c_str(), 63);
							dcpkt.cardNames[i][63] = '\0'; // Ensure null termination
						} else {
							dcpkt.cardNames[i][0] = '\0';
						}
					}

					steamManager.sendPacket(&dcpkt, sizeof(dcpkt));
					ofLogNotice("Network") << (isClient() ? "Client" : "Host") << " sent DrawCards packet: " << cardsToDraw << " cards";
				}

				playerToDraw->nextTurnExtraDraw = false;
				hasDrawnCardsThisTurn = true;
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
				int cardIndex = selectedCardIndex;
				std::string cardName = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].name
					: "";
				int cost = (cardIndex >= 0 && cardIndex < (int)players[currentPlayerIndex].hand.size())
					? players[currentPlayerIndex].hand[cardIndex].cost
					: 0;
				CardPlayResult result = playCard(cardIndex, gridX, gridY);
				if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
					players[currentPlayerIndex].ap = currentAP;
					sendActionPacket(cardIndex, gridX, gridY, cost, 0, cardName);
				}
				selectedCardIndex = -1;
				calculateTargetHighlights();
				return;
			}
		}

		// 3g-ALT. Reroll Button
		if (rerollButtonRect.inside(x, y) && button == OF_MOUSE_BUTTON_LEFT) {
			Player & curr = players[currentPlayerIndex];

			// Find the assistant to consume
			int assistantIndex = -1;
			for (int i = 0; i < (int)players.size(); i++) {
				Player & p = players[i];
				if (p.isAssistant && p.health > 0 && p.directSummonerID == curr.playerID && !p.assistantRerollUsedThisTurn) {
					int dist = abs(p.x - curr.x) + abs(p.y - curr.y);
					if (dist <= 1) {
						assistantIndex = i;
						break;
					}
				}
			}

			if (assistantIndex != -1) {
				// Mark used
				players[assistantIndex].assistantRerollUsedThisTurn = true;

				// Assistant reroll: use same dice configuration as the original AP roll
				int rerollNum = lastAPDiceNum > 0 ? lastAPDiceNum : 1;
				int rerollSides = lastAPDiceSides > 0 ? lastAPDiceSides : 6;
				// Mark any previous AP dice as debug so they won't be included twice
				for (auto & oldR : activeDiceRolls) {
					if (oldR.purpose == PURPOSE_AP) oldR.purpose = PURPOSE_DEBUG;
				}
				// Start a bonus AP roll (added on top of the original result)
				startDiceRoll(rerollNum, rerollSides, PURPOSE_BONUS_AP, "Assistant Reroll", currentPlayerIndex);

				spawnFloatingText(gridToWorld(players[assistantIndex].x, players[assistantIndex].y), "Reroll!", ofColor::gold);
			}
			return;
		}
		// --- PASTE HERE END ---

		// 3g. End Turn Button
		if (endTurnButtonRect.inside(x, y) && button == OF_MOUSE_BUTTON_LEFT) {
			if (endTurnLocked) return;

			// --- GHOST FORM CHECK ---
			Player & p = players[currentPlayerIndex];
			if (p.inGhostForm && board[p.x][p.y].hasWall) {
				spawnFloatingText(gridToWorld(p.x, p.y), "Cannot end turn in wall!", ofColor::red);
				ofLogNotice("Game") << "Prevented ending turn inside wall (Ghost Form).";
				return;
			}
			// ------------------------

			endTurnLocked = true;
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

			// In multiplayer, client should always interact with their own player
			// In single player, use currentPlayer
			Player * controlledPlayer = nullptr;
			if (isMultiplayer && isClient()) {
				// Find the player with matching playerID
				for (size_t i = 0; i < players.size(); i++) {
					if (players[i].playerID == myLocalPlayerID && !players[i].isMinion) {
						controlledPlayer = &players[i];
						break;
					}
				}
			} else {
				controlledPlayer = &currentPlayer;
			}

			if (!controlledPlayer) return;

			// Clicked Self? Select for Movement.
			if (board[gridX][gridY].hasPlayer && gridX == controlledPlayer->x && gridY == controlledPlayer->y) {
				if (playerAction == PIECE_SELECTED) {
					playerAction = NONE;
					clearHighlights();
				} else {
					selectedPieceGridX = controlledPlayer->x;
					selectedPieceGridY = controlledPlayer->y;
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

					// --- GHOST WALL LOGIC: Check if destination is valid ---
					bool isWall = board[gridX][gridY].hasWall;
					bool canEnter = !isWall; // Normal units can't enter walls

					// Ghosts can enter walls IF they have enough AP to exit (AP > 1)
					// or if it's just a pass-through (handled by pathfinding).
					// But for the final click, we rely on isHighlighted (which already checks AP logic).
					if (controlledPlayer->inGhostForm) canEnter = true;

					if (canEnter) {
						int moveAPCost = static_cast<int>(hoverPath.size()) - 1;

						// Get the controlled player's index for AP calculations
						int controlledPlayerIndex = currentPlayerIndex;
						if (isMultiplayer && isClient()) {
							// Find the index of the controlled player
							for (size_t i = 0; i < players.size(); i++) {
								if (players[i].playerID == myLocalPlayerID && !players[i].isMinion) {
									controlledPlayerIndex = i;
									break;
								}
							}
						}

						// Recompute available AP from any dice that have finished spinning this frame
						int apNow = 0;
						for (const auto & r : activeDiceRolls) {
							if ((r.purpose == PURPOSE_AP || r.purpose == PURPOSE_BONUS_AP) && r.isFinishedVisual && r.associatedUnit == controlledPlayerIndex) {
								apNow += r.result;
							}
						}
						if (players[controlledPlayerIndex].nextTurnAPBonus > 0) {
							apNow += players[controlledPlayerIndex].nextTurnAPBonus;
							players[controlledPlayerIndex].nextTurnAPBonus = 0;
						}
						if (currentAP > 0) apNow = std::max(apNow, currentAP);

						if (apNow >= moveAPCost) {
							currentAP = apNow - moveAPCost;

							// Execute movement locally (client-side prediction)
							applyMovement(controlledPlayerIndex, gridX, gridY, currentAP, &hoverPath);

							// Send packet to opponent so they see the movement too
							if (isMultiplayer) {
								ActionPacket movePkt = {};
								movePkt.type = PKT_ACTION;
								movePkt.playerID = myLocalPlayerID;
								movePkt.actorIndex = currentPlayerIndex;
								movePkt.cardIndex = -1; // -1 indicates movement, not card play
								movePkt.targetX = gridX;
								movePkt.targetY = gridY;
								movePkt.cost = currentAP; // Send current AP so opponent sees the cost
								steamManager.sendPacket(&movePkt, sizeof(movePkt));
								ofLogNotice("Network") << (isClient() ? "Client" : "Host") << " sent movement to (" << gridX << "," << gridY << ") with AP=" << currentAP;
							}
						}
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
			isMultiplayer = false; // Ensure single player mode
		}
		// ADD STEAM HOST LOGIC
		else if (mainMenuHostButton.inside(x, y)) {
			if (!steamManager.isConnected()) steamManager.createLobby();
		}
		// ADD STEAM INVITE LOGIC
		else if (mainMenuInviteButton.inside(x, y)) {
			if (steamManager.isConnected()) steamManager.openFriendOverlay();
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
	// If we are actively dragging a card, change to the closed fist
	if (draggedCardIndex != -1) {
		currentCursor = CURSOR_HOLD;
	}
	// Allow camera panning during draft
	if (currentState == STATE_DRAFTING && button == OF_MOUSE_BUTTON_RIGHT) {
		float dx = ofGetPreviousMouseX() - x, dy = ofGetPreviousMouseY() - y;
		float panMultX = shouldFlipCamera() ? -1.0f : 1.0f;
		float panMultZ = 1.0f;
		cameraTargetPan.x += dx * 0.05f * (TILE_SIZE / 4.0f) * panMultX;
		cameraTargetPan.z += dy * 0.05f * (TILE_SIZE / 4.0f) * panMultZ;
		return;
	}
	if (currentState != STATE_GAMEPLAY) return;

	// TURN VALIDATION: Only allow dragging if it's the local player's turn or a minion owned by the local player
	if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
		const Player & currentPlayer = players[currentPlayerIndex];
		int controlledPlayerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		if (controlledPlayerID != myLocalPlayerID) {
			// Not our turn - only allow camera movement
			if (button == OF_MOUSE_BUTTON_RIGHT) {
				float dx = ofGetPreviousMouseX() - x, dy = ofGetPreviousMouseY() - y;
				// For camera2: invert both X and Z to mirror the view
				float panMultX = shouldFlipCamera() ? -1.0f : 1.0f;
				float panMultZ = 1.0f;
				cameraTargetPan.x += dx * 0.05f * (TILE_SIZE / 4.0f) * panMultX;
				cameraTargetPan.z += dy * 0.05f * (TILE_SIZE / 4.0f) * panMultZ;
			}
			return;
		}
	}

	if (button == OF_MOUSE_BUTTON_RIGHT) {
		float dx = ofGetPreviousMouseX() - x, dy = ofGetPreviousMouseY() - y;
		// For camera2: invert both X and Z to mirror the view (180° rotation)
		float panMultX = shouldFlipCamera() ? -1.0f : 1.0f;
		float panMultZ = 1.0f;
		cameraTargetPan.x += dx * 0.05f * (TILE_SIZE / 4.0f) * panMultX;
		cameraTargetPan.z += dy * 0.05f * (TILE_SIZE / 4.0f) * panMultZ;
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

		if (draggedCardIndex == -1 && (selectedCardIndex != -1 || hoveredCardIndex != -1)) {
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

			int sourceIndex = (selectedCardIndex != -1) ? selectedCardIndex : hoveredCardIndex;
			if (sourceIndex < 0 || sourceIndex >= numCards) return;
			Card & card = currentPlayer.hand[sourceIndex];
			float detectionWidth = handBaseCardWidth;
			float detectionHeight = baseCardHeight * 1.6f;
			float detectionX = startX + sourceIndex * (handBaseCardWidth + padding);
			float detectionY = card.targetPos.y - detectionHeight / 2;

			if (ofRectangle(detectionX, detectionY, detectionWidth, detectionHeight).inside(ofGetPreviousMouseX(), ofGetPreviousMouseY())) {
				draggedCardIndex = sourceIndex;
				dragOffset = ofVec2f(x, y) - card.currentPos;
			}
		}

		if (draggedCardIndex != -1) {
			if (selectedCardIndex != -1) {
				selectedCardIndex = -1;
				calculateTargetHighlights();
			}
			players[currentPlayerIndex].hand[draggedCardIndex].currentPos = ofVec2f(x, y) - dragOffset;
			// While dragging, keep target highlights up-to-date so swipe shows previews without hovering
			calculateTargetHighlights(draggedCardIndex);
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
	// Recompute hover state immediately so cursor stays correct while stationary
	mouseMoved(x, y);

	if (currentState != STATE_GAMEPLAY) return;

	// TURN VALIDATION: Only allow releasing if it's the local player's turn or a minion owned by the local player
	if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
		const Player & currentPlayer = players[currentPlayerIndex];
		int controlledPlayerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		if (controlledPlayerID != myLocalPlayerID) {
			// Not our turn - ignore all interactions to avoid interrupting opponent
			return;
		}
	}

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

			// Cancel Magic Bolt Mode
			if (isTargetingMagicBolt) {
				isTargetingMagicBolt = false;
				magicBoltCardIndex = -1;
			}
			if (isBurstMenuOpen || isTargetingBurst) {
				isBurstMenuOpen = false;
				isTargetingBurst = false;
				pendingBurstCardIndex = -1;
				burstChoice = 0;
				ofLogNotice("Burst") << "Cancelled via Right Click.";
			}
			if (isTargetingDeath) {
				isTargetingDeath = false;
				deathCardIndex = -1;
			}
			if (isTargetingHeal) {
				isTargetingHeal = false;
				healCardIndex = -1;
			}

			clearHighlights();
			calculateTargetHighlights();
		}
		return;
	}

	if (button == OF_MOUSE_BUTTON_LEFT) {
		if (players.empty() || currentPlayerIndex < 0) return;

		Player & currentPlayer = players[currentPlayerIndex];
		const float dragThreshold = 5.0f;
		float dist = mouseDownPos.distance(ofVec2f(x, y));

		if (draggedCardIndex != -1) {
			if (dist > dragThreshold) {
				Card & playedCard = currentPlayer.hand[draggedCardIndex];
				float playZoneY = ofGetHeight() * 0.7f;

				if (y < playZoneY) {
					if (currentAP >= playedCard.cost) {

						// --- A. MAGIC BOLT: ENTER TARGETING MODE ---
						if (playedCard.type == CARD_MAGIC_BOLT) {
							isTargetingMagicBolt = true;
							magicBoltCardIndex = draggedCardIndex;
							draggedCardIndex = -1; // Stop dragging
							selectedCardIndex = -1;
							// Show highlights for the bolt immediately
							calculateTargetHighlights(magicBoltCardIndex);
							return; // Wait for next click
						}

						// --- B. DOUBLE HANDED: SHOW MENU FIRST ---
						if (playedCard.type == CARD_DOUBLE_HANDED) {
							pendingDoubleHandedCardIndex = draggedCardIndex;
							isDoubleHandedMenuOpen = true;
							pendingDoubleHandedChoice = "";
							// Setup UI Geometry
							float w = 500, h = 250;
							float mx = ofGetWidth() / 2 - w / 2, my = ofGetHeight() / 2 - h / 2;
							doubleHandedMenuRect.set(mx, my, w, h);
							float btnW = 200, btnH = 80;
							float spacing = 40;
							btnAddPunches.set(mx + (w - (btnW * 2 + spacing)) / 2, my + 120, btnW, btnH);
							btnAddBlocks.set(btnAddPunches.getRight() + spacing, my + 120, btnW, btnH);
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							return;
						}

						// --- C. AMNESIA: SHOW MENU (Self vs Adjacent) ---
						if (playedCard.type == CARD_AMNESIA) {
							pendingAmnesiaCardIndex = draggedCardIndex;
							isAmnesiaMenuOpen = true;
							// Setup UI Geometry
							float w = 500, h = 250;
							float mx = ofGetWidth() / 2 - w / 2, my = ofGetHeight() / 2 - h / 2;
							amnesiaMenuRect.set(mx, my, w, h);
							float btnW = 200, btnH = 80;
							float spacing = 40;
							amnesiaBtnSelf.set(mx + (w - (btnW * 2 + spacing)) / 2, my + 120, btnW, btnH);
							amnesiaBtnAdjacent.set(amnesiaBtnSelf.getRight() + spacing, my + 120, btnW, btnH);
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							return;
						}

						// --- D. TELEPORT: ROLL DICE FIRST, THEN TARGET ---
						if (playedCard.type == CARD_TELEPORT) {
							ofLogNotice("Teleport") << "Triggered! draggedCardIndex=" << draggedCardIndex << " handSize=" << currentPlayer.hand.size();
							pendingTeleportCardIndex = draggedCardIndex;
							// Roll dice - result IS the range in feet (3d6 = 3-18ft)
							pendingTeleportRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE, "Teleport: Range");
							isWaitingForTeleportDice = true;
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							return;
						}

						// --- BURST OF LIGHT: SHOW CHOICE MENU ON DRAG-RELEASE ---
						if (playedCard.type == CARD_BURST_OF_LIGHT) {
							pendingBurstCardIndex = draggedCardIndex;
							isBurstMenuOpen = true;

							// Menu geometry (similar to Wisdom Boon)
							float w = 520, h = 260;
							float mx = ofGetWidth() / 2 - w / 2, my = ofGetHeight() / 2 - h / 2;
							burstMenuRect.set(mx, my, w, h);
							float btnW = 300, btnH = 80;
							burstBtnDamage.set(mx + (w - btnW) / 2, my + 110, btnW, btnH);
							burstBtnHeal.set(0, 0, 0, 0);
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							return;
						}
						// --- E. DEATH TARGETING ---
						if (playedCard.type == CARD_DEATH) {
							isTargetingDeath = true;
							deathCardIndex = draggedCardIndex;
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							calculateTargetHighlights(deathCardIndex);
							return;
						}

						// --- F. HEAL / LESSER HEAL TARGETING ---
						if (playedCard.type == CARD_HEAL || playedCard.type == CARD_LESSER_HEAL) {
							isTargetingHeal = true;
							healCardIndex = draggedCardIndex;
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							calculateTargetHighlights(healCardIndex);
							return;
						}

						// --- CHAIN LIGHTNING ---
						if (playedCard.type == CARD_CHAIN_LIGHTNING) {
							isTargetingChainLightning = true;
							chainLightningCardIndex = draggedCardIndex;
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							calculateTargetHighlights(chainLightningCardIndex);
							return;
						}

						// --- HELLHOUND TARGETING ---
						if (playedCard.type == CARD_SUMMON_HELLHOUND) {
							isTargetingHellhound = true;
							hellhoundCardIndex = draggedCardIndex;
							draggedCardIndex = -1;
							selectedCardIndex = -1;
							calculateTargetHighlights(hellhoundCardIndex);
							return;
						}

						// --- STANDARD PLAY ---
						if (playedCard.targeting == TARGET_SELF) {
							const std::string playedCardName = playedCard.name;
							CardPlayResult result = playCard(draggedCardIndex, -1, -1);
							if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
								players[currentPlayerIndex].ap = currentAP;
								sendActionPacket(draggedCardIndex, -1, -1, playedCard.cost, 0, playedCardName);
							}
						} else {
							ofVec2f boardPos = mouseToBoard(x, y);
							int gx = floor(boardPos.x);
							int gy = floor(boardPos.y);

							if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
								// Special-case: allow drag-release to play Blocking Boon even without a highlighted tile
								if (playedCard.type == CARD_BLOCKING_BOON) {
									const std::string playedCardName = playedCard.name;
									CardPlayResult result = playCard(draggedCardIndex, -1, -1);
									if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
										players[currentPlayerIndex].ap = currentAP;
										sendActionPacket(draggedCardIndex, -1, -1, playedCard.cost, 0, playedCardName);
									}
									// Clear drag/selection state like other handlers
									draggedCardIndex = -1;
									selectedCardIndex = -1;
									calculateTargetHighlights();
									return;
								}

								// Only play if green highlight is active
								if (board[gx][gy].isTargetable) {
									ofLogNotice("CardPlay") << "Playing " << playedCard.name << " on tile (" << gx << "," << gy << ") isMultiplayer=" << isMultiplayer;
									const std::string playedCardName = playedCard.name;
									CardPlayResult result = playCard(draggedCardIndex, gx, gy);
									if (isMultiplayer && result == CARD_PLAYED_IMMEDIATELY) {
										players[currentPlayerIndex].ap = currentAP;
										sendActionPacket(draggedCardIndex, gx, gy, playedCard.cost, 0, playedCardName);
									}
								} else {
									ofLogWarning("CardPlay") << "Target not targetable! gx=" << gx << " gy=" << gy << " isTargetable=" << (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT ? board[gx][gy].isTargetable : false);
								}
							}
						}
					} else {
						// Show user feedback and clear drag state so they don't get stuck
						ofLogWarning("CardPlay") << "Not enough AP to play " << playedCard.name;
						spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Not enough AP", ofColor::red);
						draggedCardIndex = -1;
						selectedCardIndex = -1;
						calculateTargetHighlights();
						return;
					}
				}
			}

			// Cleanup if not Magic Bolt
			selectedCardIndex = -1;
			draggedCardIndex = -1;
			calculateTargetHighlights();
		}
	}
}
//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
	// Allow zooming during draft
	if (currentState == STATE_DRAFTING) {
		cameraTargetZoom -= scrollY * 4.0f;
		cameraTargetZoom = ofClamp(cameraTargetZoom, 10.0f, 100.0f);
		return;
	}
	if (currentState != STATE_GAMEPLAY) return;

	// Handle encyclopedia scrolling
	if (isCardEncyclopediaOpen && encyclopediaRect.inside(x, y)) {
		const float kBaseCardWidth = 120.0f;
		const float kCardAspectRatio = 1.4f;
		const float kBaseCardHeight = kBaseCardWidth * kCardAspectRatio;

		float cardScale = 1.2f;
		float cardH = kBaseCardHeight * cardScale;
		float padding = 15.0f;
		int cols = std::max(1, (int)floor((encyclopediaRect.width - 2 * padding) / (kBaseCardWidth * cardScale + padding)));
		int totalRows = (allCards.size() + cols - 1) / cols;
		float totalContentHeight = totalRows * (cardH + padding);
		float contentHeight = encyclopediaRect.height - 70;
		float maxScroll = std::max(0.0f, totalContentHeight - contentHeight);

		encyclopediaScrollOffset -= scrollY * 40.0f;
		encyclopediaScrollOffset = ofClamp(encyclopediaScrollOffset, 0.0f, maxScroll);
		return;
	}

	cameraTargetZoom -= scrollY * 4.0f;
	// Restrict how far the player can zoom out normally, but allow more
	// zoom-out when in top-down mode so the player can see more of the board.
	if (isTopDownView) {
		cameraTargetZoom = ofClamp(cameraTargetZoom, 10.0f, 100.0f);
	} else {
		cameraTargetZoom = ofClamp(cameraTargetZoom, 10.0f, 50.0f);
	}
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	// Handle Chat Input first (highest priority)
	if (isChatOpen && !isChatMinimized && currentState == STATE_GAMEPLAY) {
		if (key == OF_KEY_RETURN) {
			// Send message and close chat
			if (!chatInput.empty() && isMultiplayer) {
				ChatMessagePacket pkt = {};
				pkt.type = PKT_CHAT_MESSAGE;
				pkt.playerID = myLocalPlayerID;
				strncpy(pkt.message, chatInput.c_str(), 255);
				pkt.message[255] = '\0';
				steamManager.sendPacket(&pkt, sizeof(pkt));

				// Add to local chat history
				ChatMessage msg;
				msg.playerName = getPlayerSteamName(myLocalPlayerID == 0 ? 0 : 1);
				msg.message = chatInput;
				msg.timestamp = ofGetElapsedTimef();
				chatHistory.push_back(msg);
				if (chatHistory.size() > maxChatMessages) {
					chatHistory.erase(chatHistory.begin());
				}
			}
			chatInput = "";
			isChatOpen = false;
			isChatMinimized = true;
			lastChatInteractionTime = ofGetElapsedTimef();
			return;
		} else if (key == OF_KEY_ESC) {
			chatInput = "";
			isChatOpen = false;
			isChatMinimized = true;
			lastChatInteractionTime = ofGetElapsedTimef();
			return;
		} else if (key == OF_KEY_TAB) {
			// Switch tabs
			currentChatTab = (currentChatTab == ChatTab::CHAT) ? ChatTab::LOG : ChatTab::CHAT;
			return;
		} else if (key == OF_KEY_BACKSPACE) {
			if (!chatInput.empty()) {
				chatInput = chatInput.substr(0, chatInput.size() - 1);
			}
			return;
		} else if (key >= 32 && key <= 126) {
			// Printable ASCII characters
			if (chatInput.length() < maxChatInputLength) {
				chatInput += (char)key;
			}
			return;
		}
		return; // Consume all keys when chat is open
	}

	// Open chat with Enter key (only in gameplay and multiplayer)
	if (key == OF_KEY_RETURN && currentState == STATE_GAMEPLAY && isMultiplayer && !isCardSpawnerOpen) {
		isChatOpen = true;
		isChatMinimized = false; // Open in full mode for typing
		chatInput = "";
		lastChatInteractionTime = ofGetElapsedTimef();
		return;
	}

	// Handle Card Spawner text input
	if (isCardSpawnerOpen && !isCardEncyclopediaOpen) {
		if (key == OF_KEY_RETURN) {
			// Add the first matching card (or exact match)
			if (!filteredCards.empty()) {
				for (int q = 0; q < cardSpawnerQuantity; q++) {
					players[currentPlayerIndex].hand.push_back(filteredCards[0]);
					players[currentPlayerIndex].hand.back().currentPos = ofVec2f(ofGetWidth() / 2, 0);
				}
				spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y),
					"+" + ofToString(cardSpawnerQuantity) + "x " + filteredCards[0].name, ofColor::cyan);
				isCardSpawnerOpen = false;
			}
		} else if (key == OF_KEY_BACKSPACE) {
			if (!cardSpawnerInput.empty()) {
				cardSpawnerInput = cardSpawnerInput.substr(0, cardSpawnerInput.size() - 1);
				// Update filtered cards
				filteredCards.clear();
				if (!cardSpawnerInput.empty()) {
					std::string lowerInput = ofToLower(cardSpawnerInput);
					for (const auto & card : allCards) {
						if (ofToLower(card.name).find(lowerInput) != std::string::npos) {
							filteredCards.push_back(card);
						}
					}
					// Sort by how early the match appears
					std::sort(filteredCards.begin(), filteredCards.end(), [&lowerInput](const Card & a, const Card & b) {
						size_t posA = ofToLower(a.name).find(lowerInput);
						size_t posB = ofToLower(b.name).find(lowerInput);
						if (posA != posB) return posA < posB;
						return a.name < b.name;
					});
				}
			}
		} else if (key == OF_KEY_ESC) {
			isCardSpawnerOpen = false;
		} else if (key >= 32 && key <= 126) {
			// Printable ASCII characters
			cardSpawnerInput += (char)key;
			// Update filtered cards
			filteredCards.clear();
			std::string lowerInput = ofToLower(cardSpawnerInput);
			for (const auto & card : allCards) {
				if (ofToLower(card.name).find(lowerInput) != std::string::npos) {
					filteredCards.push_back(card);
				}
			}
			// Sort by how early the match appears
			std::sort(filteredCards.begin(), filteredCards.end(), [&lowerInput](const Card & a, const Card & b) {
				size_t posA = ofToLower(a.name).find(lowerInput);
				size_t posB = ofToLower(b.name).find(lowerInput);
				if (posA != posB) return posA < posB;
				return a.name < b.name;
			});
		}
		return; // Consume all keys when spawner is open
	}

	// Handle Encyclopedia scrolling
	if (isCardEncyclopediaOpen) {
		if (key == OF_KEY_ESC) {
			isCardEncyclopediaOpen = false;
		}
		return;
	}

	// (key-based speed control removed; preset is fixed to 1.5x)
}
//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
	// Block all hotkeys when chat is open
	if (isChatOpen) {
		return;
	}

	// If the Card Spawner input is open, consume key releases so typing
	// (e.g. pressing 't') doesn't trigger global hotkeys like top-down view.
	if (isCardSpawnerOpen && !isCardEncyclopediaOpen) {
		return;
	}

	// 1. Debug Toggle
	if (key == '`') {
		if (isMultiplayer && !isHost()) {
			addGameLog("Debug mode is host-only in multiplayer.");
			return;
		}
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

	// 2c. Debug Hotkeys (when debug mode is enabled)
	if (isDebugMode && currentState == STATE_GAMEPLAY) {
		// 'u' - Toggle Unlimited AP (keeps checksums ON for realistic testing)
		if (key == 'u' || key == 'U') {
			hasUnlimitedAP = !hasUnlimitedAP;
			ofLogNotice("Debug") << "Unlimited AP: " << (hasUnlimitedAP ? "ON (checksums still active)" : "OFF");
			addGameLog("Unlimited AP: " + std::string(hasUnlimitedAP ? "ON (checksums still active)" : "OFF"));
			if (isMultiplayer && isHost()) {
				sendSnapshotToClient(); // Sync state
			}
			return;
		}

		// 'c' - Open Card Spawner
		if (key == 'c' || key == 'C') {
			isCardSpawnerOpen = true;
			cardSpawnerInput = "";
			cardSpawnerQuantity = 1;
			filteredCards.clear();
			ofLogNotice("Debug") << "Card Spawner opened (press ESC to close)";
			return;
		}

		// 's' - Skip Checksum Validation (for testing without unlimited AP)
		if (key == 's' || key == 'S') {
			skipChecksumValidation = !skipChecksumValidation;
			ofLogNotice("Debug") << "Skip Checksum: " << (skipChecksumValidation ? "ON" : "OFF");
			addGameLog("Checksum Validation: " + std::string(skipChecksumValidation ? "DISABLED" : "ENABLED"));
			return;
		}
	}

	// 3. Escape Key Logic
	if (key == OF_KEY_ESC) {
		switch (currentState) {
		case STATE_GAMEPLAY:
			// Check UI panels first
			if (isShowingPileView) {
				isShowingPileView = false;
				currentPileViewPlayerIndex = -1;
				currentPileView = VIEW_NONE;
			} else if (isAmnesiaSelectionActive) {
				isAmnesiaSelectionActive = false;
			} else {
				// Pause Game
				pausedFromState = STATE_GAMEPLAY; // Remember where we came from
				currentState = STATE_PAUSED;
			}
			break;

		case STATE_INITIATIVE_ROLL:
			pausedFromState = STATE_INITIATIVE_ROLL;
			currentState = STATE_PAUSED;
			break;
		case STATE_DRAFTING:
			pausedFromState = STATE_DRAFTING;
			currentState = STATE_PAUSED;
			break;

		case STATE_PAUSED:
			// Return to whatever state we paused from
			currentState = pausedFromState;
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
//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
	recalculateUI(w, h);
	allocateWorldFbo(w, h);

	// --- FIX: Snap UI elements immediately to prevent "flying in" visual glitches ---

	// 1. Snap End Turn Button
	float scale = h / 1080.0f;
	float btnWidth = 250 * scale;
	float visibleY = 20 * scale;
	float hiddenY = -100 * scale;

	bool showEndTurnButton = false;
	if (currentPlayerIndex >= 0 && !players.empty()) {
		int pid = players[currentPlayerIndex].playerID;
		int oid = players[currentPlayerIndex].ownerID;
		// Show end turn button when it's the current player's turn
		if (isMultiplayer) {
			// Multiplayer: only show when it's my turn
			if (pid == myLocalPlayerID || oid == myLocalPlayerID) showEndTurnButton = true;
		} else {
			// Single player: always show (any player can end their turn)
			showEndTurnButton = true;
		}
	}

	if (showEndTurnButton) {
		endTurnButtonTargetPos.set(w / 2.0f - btnWidth / 2.0f, visibleY);
	} else {
		endTurnButtonTargetPos.set(w / 2.0f - btnWidth / 2.0f, hiddenY);
	}
	// Force current to target
	endTurnButtonCurrentPos = endTurnButtonTargetPos;

	// 2. Snap Cards in Hand
	if (!players.empty() && currentPlayerIndex >= 0) {
		Player & currentPlayer = players[currentPlayerIndex];

		// Always show current player's hand at bottom (turn-based)
		// In multiplayer, only show local player's hand
		// In single player, show whichever player's turn it is
		float handCenterY = h - 130;
		float handBaseCardWidth = 120;
		float handAreaWidth = w * 0.4f;

		size_t numCards = currentPlayer.hand.size();
		float totalCardWidths = numCards * handBaseCardWidth;
		float padding = (numCards > 1) ? (handAreaWidth - totalCardWidths) / (numCards - 1) : 0;
		padding = std::min(padding, 20.0f);
		float totalHandWidth = (numCards * handBaseCardWidth) + ((numCards - 1) * padding);
		float startX = (w - totalHandWidth) / 2.0f;

		for (size_t i = 0; i < numCards; i++) {
			float cardCenterX = startX + i * (handBaseCardWidth + padding) + (handBaseCardWidth / 2.0f);
			currentPlayer.hand[i].targetPos = ofVec2f(cardCenterX, handCenterY);

			// FORCE SNAP
			currentPlayer.hand[i].currentPos = currentPlayer.hand[i].targetPos;
		}
	}
}
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) { }

//--------------------------------------------------------------
void ofApp::startNewTurn() {
	ofLogNotice("Turn") << "startNewTurn() called. isMultiplayer=" << isMultiplayer << " currentPlayerIndex=" << currentPlayerIndex << " myLocalPlayerID=" << myLocalPlayerID << " isCurrentPlayerLocal()=" << isCurrentPlayerLocal();

	// If it was MY turn and I am ending it:
	if (isMultiplayer && isCurrentPlayerLocal()) {
		ofLogNotice("Turn") << "Ending my turn (player " << myLocalPlayerID << "). Sending END_TURN packet.";

		// 1. Send End Turn
		PacketHeader pkt;
		pkt.type = PKT_END_TURN;
		pkt.playerID = myLocalPlayerID;
		steamManager.sendPacket(&pkt, sizeof(pkt));

		// 2. Send Checksum to verify we ended in the same state
		// Skip checksum on turn 0 (draft completion) to allow draft packets to sync first
		if (globalTurnCounter > 0) {
			// Anti-cheat: Log deck states before sending checksum
			logDeckStates("End Turn " + std::to_string(globalTurnCounter));

			ChecksumPacket sumPkt = {};
			sumPkt.type = PKT_CHECKSUM_CHECK;
			sumPkt.playerID = myLocalPlayerID;
			sumPkt.checksum = calculateChecksum();
			sumPkt.turnNumber = globalTurnCounter;
			steamManager.sendPacket(&sumPkt, sizeof(sumPkt));
		}

		// 2.5. CLEAN UP LOCAL PLAYER'S HAND & BUFFS BEFORE WAITING
		// Both host and client must do this so deck states stay in sync
		ofLogNotice("Turn") << "Cleaning up local player's (" << myLocalPlayerID << ") hand and buffs...";
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == myLocalPlayerID && !players[i].isMinion) {
				Player & localPlayer = players[i];
				ofLogNotice("Turn") << "Found local player at index " << i << ". Hand size: " << localPlayer.hand.size() << ", Played: " << localPlayer.playedCardsPile.size();

				// Move hand and played cards to discard
				localPlayer.discardPile.insert(localPlayer.discardPile.end(), localPlayer.hand.begin(), localPlayer.hand.end());
				localPlayer.hand.clear();
				localPlayer.discardPile.insert(localPlayer.discardPile.end(), localPlayer.playedCardsPile.begin(), localPlayer.playedCardsPile.end());
				localPlayer.playedCardsPile.clear();

				ofLogNotice("Turn") << "After cleanup: Hand size=" << localPlayer.hand.size() << ", Discard size=" << localPlayer.discardPile.size();

				// Clear buffs
				localPlayer.shocksPlayedThisTurn = 0;
				localPlayer.nextAttackAddPoison = false;
				localPlayer.flurryOfFistsActive = false;

				// NOTE: Defensive stats (block, ward, etc.) are NOT cleared here!
				// They persist until the START of the player's NEXT turn (see continueNewTurn)

				// Reshuffle discard into deck if needed
				if (localPlayer.deck.empty() && !localPlayer.discardPile.empty()) {
					localPlayer.deck = localPlayer.discardPile;
					// Use player index for shuffle (find it again since we're in a loop)
					int playerIdx = -1;
					for (size_t j = 0; j < players.size(); j++) {
						if (players[j].playerID == myLocalPlayerID && !players[j].isMinion) {
							playerIdx = j;
							break;
						}
					}
					shuffleGameVector(localPlayer.deck, playerIdx);
					localPlayer.discardPile.clear();
					ofLogNotice("Deck") << "Client reshuffled discard into deck for player " << localPlayer.playerID;
				}

				// Decrement buff timers
				if (localPlayer.strengthenElementsTurnsRemaining > 0) {
					localPlayer.strengthenElementsTurnsRemaining--;
					if (localPlayer.strengthenElementsTurnsRemaining == 0) {
						spawnFloatingText(gridToWorld(localPlayer.x, localPlayer.y), "Elements Faded", ofColor::gray);
					}
				}

				// Decrement Sprint's Kick-free counter
				if (localPlayer.freeKickTurns > 0) {
					localPlayer.freeKickTurns--;
					if (localPlayer.freeKickTurns == 0) {
						spawnFloatingText(gridToWorld(localPlayer.x, localPlayer.y), "Kick Normal Cost", ofColor::white);
					}
				}

				break;
			}
		}

		// 3. CLIENT: Wait for host's next TurnStart packet instead of rolling locally
		if (isClient()) {
			ofLogNotice("Turn") << "Client: Early return after cleanup. Waiting for host's TurnStart.";
			// OPTIMISTIC PREDICTION: Advance to opponent's turn immediately for snappy UI
			// Host will confirm with PKT_TURN_START, which will re-run this function
			currentPlayerIndex = (currentPlayerIndex + 1) % players.size();
			if (currentPlayerIndex == 0) globalTurnCounter++;

			// Lock end turn button to prevent double-clicks
			endTurnLocked = true;

			ofLogNotice("Turn") << "Client: Optimistically advanced to player " << players[currentPlayerIndex].playerID << "'s turn";
			// Early return - wait for host's PKT_TURN_START to complete the turn initialization
			return;
		}
		ofLogNotice("Turn") << "Host: Continuing with turn advancement...";
	}

	if (players.empty()) return;

	// --- 1. Handle the ENDING player's state ---
	if (currentPlayerIndex != -1) {
		Player & endingPlayer = players[currentPlayerIndex];
		ofLogNotice("Turn") << "Processing ending player: index=" << currentPlayerIndex << " playerID=" << endingPlayer.playerID;

		// --- A. CLEANUP HAND & BUFFS ---
		// Always clean up the ending player's hand so all machines see it disappear
		// (Previously skipped for remote players, but that caused desync in opponent's view)
		ofLogNotice("Turn") << "Cleaning up ending player's hand. Hand size: " << endingPlayer.hand.size() << ", Played: " << endingPlayer.playedCardsPile.size();
		endingPlayer.discardPile.insert(endingPlayer.discardPile.end(), endingPlayer.hand.begin(), endingPlayer.hand.end());
		endingPlayer.hand.clear();
		endingPlayer.discardPile.insert(endingPlayer.discardPile.end(), endingPlayer.playedCardsPile.begin(), endingPlayer.playedCardsPile.end());
		endingPlayer.playedCardsPile.clear();

		endingPlayer.shocksPlayedThisTurn = 0;
		endingPlayer.nextAttackAddPoison = false; // Clear poison buff at end of turn
		endingPlayer.flurryOfFistsActive = false; // Clear flurry buff at end of turn

		// Reshuffle discard into deck if needed
		if (endingPlayer.deck.empty() && !endingPlayer.discardPile.empty()) {
			endingPlayer.deck = endingPlayer.discardPile;
			shuffleGameVector(endingPlayer.deck, currentPlayerIndex);
			endingPlayer.discardPile.clear();
			ofLogNotice("Deck") << "Reshuffled discard into deck for player " << endingPlayer.playerID;
		}

		// Decrement buff timers
		if (endingPlayer.strengthenElementsTurnsRemaining > 0) {
			endingPlayer.strengthenElementsTurnsRemaining--;
			if (endingPlayer.strengthenElementsTurnsRemaining == 0) {
				spawnFloatingText(gridToWorld(endingPlayer.x, endingPlayer.y), "Elements Faded", ofColor::gray);
			}
		}

		// Decrement Sprint's Kick-free counter
		if (endingPlayer.freeKickTurns > 0) {
			endingPlayer.freeKickTurns--;
			if (endingPlayer.freeKickTurns == 0) {
				spawnFloatingText(gridToWorld(endingPlayer.x, endingPlayer.y), "Kick Normal Cost", ofColor::white);
			}
		}

		// --- B. CHECK FOR BONUS TURNS ---
		if (endingPlayer.bonusTurns > 0) {
			endingPlayer.bonusTurns--;
			ofLogNotice("Time Vortex") << "Bonus Turn! " << (endingPlayer.isMinion ? "Minion " : "Player ") << endingPlayer.playerID << " goes again. " << endingPlayer.bonusTurns << " remaining.";

			// The current player is STILL the ending player. We just reset their state.
			Player & startingPlayer = endingPlayer;

			// Reset Stats for Bonus Turn
			if (startingPlayer.hasRegeneration) {
				if (startingPlayer.health < startingPlayer.maxHealth) {
					startingPlayer.health++;
					spawnFloatingText(gridToWorld(startingPlayer.x, startingPlayer.y), "+1 Regen", ofColor::green);
				}
			}
			// Tortoise form: ALL defensive stats don't expire
			if (!startingPlayer.inTortoiseForm) {
				startingPlayer.block = 0;
				startingPlayer.holyBlock = 0;
				startingPlayer.ward = 0;
				startingPlayer.fortification = 0;
				startingPlayer.barrier = 0;
			}

			// Check Status Effects
			if (startingPlayer.isParalyzed) {
				startDiceRoll(1, 2, PURPOSE_COIN_FLIP);
				isWaitingForParalysisCoin = true;
				return;
			}
			if (startingPlayer.onFire) {
				isWaitingForOnFireDice = true;
				pendingOnFireRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Fire Status Damage");
				return;
			}
			if (startingPlayer.isPoisoned) {
				isWaitingForPoisonDice = true;
				pendingPoisonRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Poison Status Damage");
				return;
			}

			continueNewTurn();
			return;
		}
	}

	// --- 2. ADVANCE TO THE NEXT PLAYER (NORMAL TURN) ---
	// FIX: Removed the while loop. We just increment once.
	// Sickness is now handled inside continueNewTurn to ensure correct timing.
	currentPlayerIndex = (currentPlayerIndex + 1) % players.size();

	if (currentPlayerIndex == 0) globalTurnCounter++;

	Player & startingPlayer = players[currentPlayerIndex];
	ofLogNotice("Game") << "--- START TURN: " << (startingPlayer.isMinion ? "Minion " : "Player ") << startingPlayer.playerID;

	// Add game log entry for turn start
	addGameLog("Turn " + ofToString(globalTurnCounter) + ": " + getPlayerSteamName(currentPlayerIndex) + "'s turn");

	// Ensure temp luck is correct for the starting player before AP is rolled
	recalcTempLuck();

	// --- C. RESET STATE FOR NORMAL TURN ---
	playerVisualPos = gridToWorld(startingPlayer.x, startingPlayer.y);
	animationPath.clear();
	isPlayerAnimating = false;
	animatingPlayerIndex = -1;

	if (startingPlayer.hasRegeneration) {
		if (startingPlayer.health < startingPlayer.maxHealth) {
			startingPlayer.health++;
			spawnFloatingText(gridToWorld(startingPlayer.x, startingPlayer.y), "+1 Regen", ofColor::green);
		}
	}
	// Tortoise form: ALL defensive stats don't expire
	if (!startingPlayer.inTortoiseForm) {
		startingPlayer.block = 0;
		startingPlayer.holyBlock = 0;
		startingPlayer.ward = 0;
		startingPlayer.fortification = 0;
		startingPlayer.barrier = 0;
	}

	if (startingPlayer.nextTurnBonusDiceFromMinions) {
		int minionCount = 0;
		for (const auto & p : players) {
			if (p.isSkeleton || p.isHellhound) minionCount++;
		}

		// Dark Shield: Roll Xd6 where X = total skeletons + hellhounds on board
		// This REPLACES the normal AP roll, not adds to it
		if (minionCount > 0) {
			startDiceRoll(minionCount, 6, PURPOSE_AP, "Dark Shield AP Roll", currentPlayerIndex);
		}
		// If minionCount is 0, no AP roll happens this turn!

		startingPlayer.nextTurnBonusDiceFromMinions = false;
		// Skip the normal AP roll section below
		return;
	}

	// --- D. CHECK STATUS EFFECTS FOR NORMAL TURN ---

	// 1. SLEEP CHECK
	if (startingPlayer.sleepTurnsRemaining > 0) {
		startingPlayer.sleepTurnsRemaining--;
		spawnFloatingText(gridToWorld(startingPlayer.x, startingPlayer.y), "Zzz...", ofColor::cyan);

		if (startingPlayer.onFire) {
			isWaitingForOnFireDice = true;
			pendingOnFireRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Sleeping Fire Damage");
			return;
		}

		startNewTurn(); // Skip turn immediately
		return;
	}

	// 2. PARALYSIS / FIRE / POISON
	if (startingPlayer.isParalyzed) {
		startDiceRoll(1, 2, PURPOSE_COIN_FLIP, "Paralysis Check");
		isWaitingForParalysisCoin = true;
		return;
	}
	if (startingPlayer.onFire) {
		isWaitingForOnFireDice = true;
		pendingOnFireRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE);
		return;
	}
	if (startingPlayer.isPoisoned) {
		isWaitingForPoisonDice = true;
		pendingPoisonRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Poison Status Damage");
		return;
	}

	continueNewTurn();
}
//--------------------------------------------------------------
void ofApp::continueNewTurn() {
	endTurnLocked = false;
	Player & startingPlayer = players[currentPlayerIndex];

	// Ensure temp luck is correct for the starting player before AP is rolled
	recalcTempLuck();

	// Reset assistant abilities if it's the assistant's turn
	if (startingPlayer.isAssistant) {
		startingPlayer.assistantRerollUsedThisTurn = false;
		// AP for assistants will be handled in the AP roll logic below.
	}

	// --- 0. SUMMONING SICKNESS CHECK .
	if (startingPlayer.summonedOnTurnCycle == globalTurnCounter) {
		ofLogNotice("Turn") << "Skipping Player " << startingPlayer.playerID << " (Summoning Sickness - Turn Cycle " << globalTurnCounter << ")";
		spawnFloatingText(gridToWorld(startingPlayer.x, startingPlayer.y), "Waiting...", ofColor::gray);

		// Immediately end this turn and go to the next unit
		startNewTurn();
		return;
	}

	ofLogNotice("Game") << "Player " << startingPlayer.playerID << "'s turn begins.";
	hasDrawnCardsThisTurn = false;
	opponentHasDrawnCardsThisTurn = false;
	selectedCardIndex = -1;
	draggedCardIndex = -1;
	playerAction = NONE;
	clearHighlights();
	calculateTargetHighlights();

	// FIX: Snap visual position instantly to the new unit so it doesn't "fly" across the board
	playerVisualPos = gridToWorld(startingPlayer.x, startingPlayer.y);
	animationPath.clear();
	isPlayerAnimating = false;
	animatingPlayerIndex = -1;

	activeDiceRolls.clear();
	currentAP = 0;

	// --- 1. SLEEP CHECK (New Status) ---
	if (startingPlayer.sleepTurnsRemaining > 0) {
		startingPlayer.sleepTurnsRemaining--;
		spawnFloatingText(gridToWorld(startingPlayer.x, startingPlayer.y), "Zzz...", ofColor::cyan);

		// If on fire while sleeping, roll damage first, then the update loop will end the turn
		if (startingPlayer.onFire) {
			isWaitingForOnFireDice = true;
			pendingOnFireRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Sleeping Fire Damage");
			return;
		}

		// If not on fire, skip turn immediately
		startNewTurn();
		return;
	}

	// --- CLIENT: Wait for host's TurnStart packet if transitioning from draft ---
	if (isClient() && waitingForTurnStartFromHost) {
		ofLogNotice("Network") << "Client: Skipping local AP roll, waiting for TurnStart from host";
		return;
	}

	// --- AP ROLL LOGIC ---
	// Wolf AP: 1d10
	if (startingPlayer.isWolf) {
		lastAPDiceNum = 1;
		lastAPDiceSides = 10;
		startDiceRoll(1, 10, PURPOSE_AP, "Wolf AP Roll", currentPlayerIndex);
	}
	// HELLHOUND AP: 2d6
	else if (startingPlayer.isHellhound) {
		lastAPDiceNum = 2;
		lastAPDiceSides = 6;
		startDiceRoll(2, 6, PURPOSE_AP, "Hellhound AP Roll", currentPlayerIndex);
	}
	// Demon AP: 4d4
	else if (startingPlayer.isDemon) {
		lastAPDiceNum = 4;
		lastAPDiceSides = 4;
		startDiceRoll(4, 4, PURPOSE_AP, "Demon AP Roll", currentPlayerIndex);
	}
	// Kobold AP: 1d4
	else if (startingPlayer.isKobold) {
		lastAPDiceNum = 1;
		lastAPDiceSides = 4;
		startDiceRoll(1, 4, PURPOSE_AP, "Kobold AP Roll", currentPlayerIndex);
	}
	// Wall Unit AP: 1d4 or 1d6
	else if (startingPlayer.isWallUnit) {
		if (startingPlayer.isMagicWallUnit) {
			lastAPDiceNum = 1;
			lastAPDiceSides = 6;
			startDiceRoll(1, 6, PURPOSE_AP, "Magic Wall Unit AP", currentPlayerIndex);
		} else {
			lastAPDiceNum = 1;
			lastAPDiceSides = 4;
			startDiceRoll(1, 4, PURPOSE_AP, "Wall Unit AP", currentPlayerIndex);
		}
	}
	// Kobold King AP: 1d6
	else if (startingPlayer.isKoboldKing) {
		lastAPDiceNum = 1;
		lastAPDiceSides = 6;
		startDiceRoll(1, 6, PURPOSE_AP, "Kobold King AP", currentPlayerIndex);
	}
	// Assistant AP: Coinflip (Heads=2, Tails=1)
	else if (startingPlayer.isAssistant) {
		// We define a custom roll logic here or use startDiceRoll
		// Since startDiceRoll handles the visual dice, let's use a Coin (1d2).
		// We will interpret 1 as 1 AP, 2 as 2 AP.
		lastAPDiceNum = 1;
		lastAPDiceSides = 2;
		startDiceRoll(1, 2, PURPOSE_AP, "Assistant AP (Coin)", currentPlayerIndex);
	}
	// Faerie AP: 1d4
	else if (startingPlayer.isFaerie) {
		lastAPDiceNum = 1;
		lastAPDiceSides = 4;
		startDiceRoll(1, 4, PURPOSE_AP, "Faerie AP Roll", currentPlayerIndex);
	}
	// Skeleton / generic minion AP: 1d6
	else if (startingPlayer.isMinion) {
		// Use the minion's display name (eg. "Golem 1") in the roll description
		lastAPDiceNum = 1;
		lastAPDiceSides = 6;
		startDiceRoll(1, 6, PURPOSE_AP, getPlayerDisplayName(currentPlayerIndex) + " AP Roll", currentPlayerIndex);
	} else {
		// Players
		int apDiceSides = 6;
		if (startingPlayer.nextTurnD10AP) {
			apDiceSides = 10;
			startingPlayer.nextTurnD10AP = false;
		}
		lastAPDiceNum = 1;
		lastAPDiceSides = apDiceSides;
		startDiceRoll(1, apDiceSides, PURPOSE_AP, "Player AP Roll", currentPlayerIndex);
	}

	// --- 3. OTHER STATUS CHECKS (Paralysis/Fire) ---
	// These are already handled in startNewTurn() before continueNewTurn() is called.
	// Fire/Paralysis checks would have returned early in startNewTurn() and resolved
	// before reaching here, so no need to check again.
}

void ofApp::recalcTempLuck() {
	// No-op: passive luck is computed on-demand via computePassiveLuck().
}

int ofApp::computePassiveLuck(int playerIndex) {
	if (playerIndex < 0 || playerIndex >= (int)players.size()) return 0;
	int count = 0;

	// Assistant auras: for each assistant alive adjacent to its summoner, add +1 to the summoner
	for (const auto & assistant : players) {
		if (!assistant.isAssistant || assistant.health <= 0) continue;
		// find summoner index
		for (const auto & summoner : players) {
			if (summoner.playerID == assistant.directSummonerID) {
				int dist = abs(assistant.x - summoner.x) + abs(assistant.y - summoner.y);
				if (dist <= 1 && summoner.playerID == players[playerIndex].playerID) count++;
			}
		}
	}

	// Four-leaf clover cards in deck: each copy grants +1 passive luck
	for (const auto & c : players[playerIndex].deck) {
		if (c.type == CARD_FOUR_LEAF_CLOVER) count++;
	}

	return count;
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

		// Shuffle the new Deck (authoritative via host in multiplayer)
		shuffleGameVector(currentPlayer.deck, currentPlayerIndex);
	}

	// --- PHASE 2: DRAW THE CARD ---
	// We check empty() again because we might have just refilled it in Phase 1.
	if (isClient() && currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
		if (hasPendingShuffleNonce[currentPlayerIndex]) {
			std::mt19937 shuffleRng(pendingShuffleNonce[currentPlayerIndex]);
			deterministic_shuffle(currentPlayer.deck, shuffleRng);
			lastAppliedShuffleNonce[currentPlayerIndex] = pendingShuffleNonce[currentPlayerIndex];
			hasPendingShuffleNonce[currentPlayerIndex] = false;
			pendingShuffleNonce[currentPlayerIndex] = 0;
			ofLogNotice("Network") << "Client: Applied pending shuffle nonce for player " << currentPlayerIndex << " before draw";
		}
	}
	if (!currentPlayer.deck.empty()) {
		Card newCard = currentPlayer.deck.back();
		currentPlayer.deck.pop_back();

		// --- Animation Setup ---
		// Optimistic UI: Start at full size for instant feedback
		newCard.currentScale = 1.5f;
		newCard.targetScale = 1.5f;

		// Calculate Spawn Position (from Deck UI)
		// In multiplayer, always place drawn cards at the local player's location (bottom)
		// In singleplayer, use currentPlayerIndex to determine placement
		float scale = ofGetHeight() / 1080.0f;
		float staticUICardWidth = (120 * 1.3f) * scale;
		float staticUICardHeight = ((120 * (585.0f / 409.0f)) * 1.3f) * scale;

		bool isLocalPlayer = false;
		if (isMultiplayer) {
			// In multiplayer, check if this player is the local player
			isLocalPlayer = (currentPlayer.playerID == myLocalPlayerID);
		} else {
			// In singleplayer, check if currentPlayerIndex is 0
			isLocalPlayer = (currentPlayerIndex == 0);
		}

		if (isLocalPlayer) {
			// Draw at bottom
			float deckX = 30 * scale;
			float deckY = ofGetHeight() - staticUICardHeight - (40 * scale) - staticUICardHeight - (40 * scale);
			newCard.currentPos.set(deckX + staticUICardWidth / 2, deckY + staticUICardHeight / 2);
		} else {
			// Draw at top
			float discardX = ofGetWidth() - staticUICardWidth - (30 * scale);
			float discardY = 40 * scale;
			float deckX = discardX;
			float deckY = discardY + staticUICardHeight + (40 * scale);
			newCard.currentPos.set(deckX + staticUICardWidth / 2, deckY + staticUICardHeight / 2);
		}

		// Add to Hand
		currentPlayer.hand.push_back(newCard);

		// ADD THIS LINE:
		currentPlayer.hand.back().drawnThisTurn = true;

		ofLogNotice("Game") << "Drew card: " << newCard.name;
		addGameLog(getPlayerSteamName(currentPlayerIndex) + " drew " + newCard.name);
	}
}
//--------------------------------------------------------------
CardPlayResult ofApp::playCard(int cardIndex, int targetX, int targetY) {
	Player & currentPlayer = players[currentPlayerIndex];
	if (cardIndex < 0 || cardIndex >= static_cast<int>(currentPlayer.hand.size())) return CARD_NOT_PLAYABLE;

	Card playedCard = currentPlayer.hand[cardIndex];
	// Determine effective cost (Kick may be free due to Sprint)
	int costToPay = playedCard.cost;
	if (playedCard.name == "Kick" && currentPlayer.freeKickTurns > 0) costToPay = 0;

	// DEBUG: Unlimited AP mode
	if (hasUnlimitedAP) {
		currentAP = 999; // Set to max for debug testing
	}

	if (currentAP < costToPay) return CARD_NOT_PLAYABLE;

	// Log card played
	addGameLog(getPlayerSteamName(currentPlayerIndex) + " played " + playedCard.name);

	// --- Magic Wall Placement/Transformation ---
	if (playedCard.type == CARD_CREATE_WALL && playedCard.name == "Summon Magic Wall") {
		// Allow placing on empty adjacent tile or transforming an adjacent wall
		if (targetX >= 0 && targetX < BOARD_WIDTH && targetY >= 0 && targetY < BOARD_HEIGHT) {
			Tile & tile = board[targetX][targetY];
			// Do not allow turning an already-magic wall into a magic wall again
			if (tile.hasWall && tile.isMagicWall) {
				spawnFloatingText(gridToWorld(targetX, targetY), "Already Magic Wall", ofColor::red);
				return CARD_NOT_PLAYABLE;
			}
			if (!tile.hasWall && !tile.hasPlayer) {
				tile.hasWall = true;
				tile.isMagicWall = true;
				buildLevelMesh();
				invalidateTargetCache();
			} else if (tile.hasWall) {
				tile.isMagicWall = true;
				buildLevelMesh();
				invalidateTargetCache();
			}
		}
		// Remove card from hand and pay cost
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);

		// Show played card animation (right side, no scale-up)
		PlayedCardAnimation cardAnim;
		cardAnim.card = playedCard;
		cardAnim.startTime = ofGetElapsedTimef();
		float handBaseCardWidth = 120.0f;
		float w = handBaseCardWidth * 2.6f;
		cardAnim.pos = glm::vec2(ofGetWidth() - (w / 2.0f) - 40.0f, ofGetHeight() / 2.0f);
		cardAnim.currentScale = 2.6f;
		cardAnim.currentAlpha = 255.0f;
		activePlayedCardAnimations.push_back(cardAnim);

		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		return CARD_PLAYED_IMMEDIATELY;
	}

	// Damage handling moved to member helper `applyDamageTo` to allow reuse from other handlers.

	bool playedSuccessfully = false;

	// --- 2. CARD LOGIC SWITCH ---
	switch (playedCard.type) {

	// --- CASE: FOUR-LEAF CLOVER ---
	case CARD_FOUR_LEAF_CLOVER: {
		// Normal TARGET_SELF behavior: apply to current player regardless of release coords
		currentPlayer.luck += 1;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+1 Luck!", ofColor::green);
		playedSuccessfully = true;
		break;
	}

		// --- CASE: BLOCKING BOON ---
	case CARD_BLOCKING_BOON: {
		// Prevent re-entry if a Blocking Boon is already resolving
		if (blockingBoonActive) {
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Blocking Boon already resolving", ofColor::gray);
			break;
		}
		// Allow playing without an adjacent target: target is optional for the Tails effect.
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		blockingBoonTargetIndex = targetIndex; // may be -1 (no target)

		// 1. Physical Block -> Coins (resolve first)
		// Note: `ward` counts toward BOTH physical and non-physical blocking.
		// Also include `fortification` (from Fortify) as a physical/piercing block source.
		int physBlock = currentPlayer.block + currentPlayer.fortification + currentPlayer.ward;
		// Fortification reduces both physical and piercing damage; count it for non-physical resolution too
		int nonPhys = currentPlayer.holyBlock + currentPlayer.barrier + currentPlayer.ward + currentPlayer.fortification;

		if (physBlock > 0) {
			ofLogNotice("Blocking Boon") << "Rolling " << physBlock << " coins for Physical Block (resolve first).";
			// Start coin flips and track them; D20s will be queued until coins complete.
			startDiceRoll(physBlock, 2, PURPOSE_BLOCKING_BOON_COIN, "Boon: Phys Flip", currentPlayerIndex);
			isWaitingForBlockingBoonCoins = true;
			pendingBlockingBoonCoinsRemaining = physBlock;
			pendingBlockingBoonNonPhys = nonPhys; // roll these after coins finish
			pendingBlockingBoonTotal = physBlock + nonPhys;
			blockingBoonActive = true;
			playedSuccessfully = true;
		} else {
			// No coins to flip, roll D20s immediately (if any)
			if (nonPhys > 0) {
				ofLogNotice("Blocking Boon") << "No physical block; Rolling " << nonPhys << " D20s for Non-Phys Block.";
				startDiceRoll(nonPhys, 20, PURPOSE_BLOCKING_BOON_D20, "Boon: Magic Roll", currentPlayerIndex);
				// Mark non-phys as already handled so coins finishing later won't re-roll them
				pendingBlockingBoonNonPhys = 0;
				pendingBlockingBoonTotal = nonPhys;
				blockingBoonActive = true;
				playedSuccessfully = true;
			} else {
				spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "No Block!", ofColor::gray);
			}
		}

		break;
	}

	// --- CASE: CONSTITUTION BOON ---
	case CARD_CONSTITUTION_BOON: {
		// When played, grants a chained draft based on the caster's max HP:
		// 16-20 => Class 1, 21-25 => Class 2, 26-30 => Class 3, >30 => immediate win
		int mh = currentPlayer.maxHealth;
		int tier = 0;
		if (mh >= 16 && mh <= 20)
			tier = 1;
		else if (mh >= 21 && mh <= 25)
			tier = 2;
		else if (mh >= 26 && mh <= 30)
			tier = 3;

		if (mh > 30) {
			// Win condition: show message and return to main menu
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "You Win!", ofColor::gold);
			ofLogNotice("Constitution Boon") << "Player " << currentPlayer.playerID << " triggered instant win via Constitution Boon.";
			currentState = STATE_MAIN_MENU;
			playedSuccessfully = true;
		} else if (tier > 0) {
			pendingDraftQueue.push_back(tier);
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Draft Class " + ofToString(tier), ofColor::cyan);
			ofLogNotice("Constitution Boon") << "Player " << currentPlayer.playerID << " queued draft Class " << tier;
			playedSuccessfully = true;
		} else {
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "No Effect", ofColor::gray);
		}

		break;
	}

	// --- CASE: SPRINT ---
	case CARD_SPRINT: {
		// Immediate +2 AP and +2 AP next turn. Also make Kick cost-free until end of next turn.
		currentAP += 2;
		currentPlayer.nextTurnAPBonus += 2;
		currentPlayer.freeKickTurns = 2; // Current turn + next turn
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+2 AP Now", ofColor::yellow);
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.5f, 0), "+2 AP Next Turn", ofColor::yellow);
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 1.0f, 0), "Kick is Free!", ofColor::cyan);
		playedSuccessfully = true;
		break;
	}

		// --- CASE: TRAIN ---
	case CARD_TRAIN: {
		pendingTrainCardIndex = cardIndex;
		isTrainMenuOpen = true;

		// Setup UI Geometry
		float w = 600, h = 300;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		trainMenuRect.set(x, y, w, h);

		// Button positioning handled in drawTrainMenuUI logic usually,
		// but we define rects here for mouse detection consistency
		float btnW = 260, btnH = 80, spacing = 30;
		float startX = x + (w - (btnW * 2 + spacing)) / 2;
		float btnY = y + 130;

		trainBtnAP.set(startX, btnY, btnW, btnH);
		trainBtnDraft.set(startX + btnW + spacing, btnY, btnW, btnH);

		// Don't set playedSuccessfully yet; waiting for menu choice
		return CARD_AWAITING_MENU_CHOICE;
	}

	// --- CASE: STUDY ---
	case CARD_STUDY: {
		// 1. Apply "Draw Extra Card Next Turn"
		// Note: We need to ensure this stacks or handles existing flags.
		// For now, setting it to true works.
		currentPlayer.nextTurnExtraDraw = true;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Studying...", ofColor::blue);

		// 2. Trigger Draft (Class 2)
		isInGameDraft = true;
		draftPlayerIndex = currentPlayerIndex; // The current unit gets the card
		generateDraftOptions(2); // Class 2
		draftPicksRemaining = 1;
		selectedDraftIndices.clear();
		draftStage = 0; // Context reset

		// 3. Switch State
		currentState = STATE_DRAFTING;

		playedSuccessfully = true;
		break;
	}

	// --- CASE: BURST OF LIGHT ---
	case CARD_BURST_OF_LIGHT: {
		// Open a small choice menu: Deal 3 Holy OR Heal 3 HP (Line of Sight)
		// Determine target tile validity first (we allow any LOS target)
		pendingBurstCardIndex = cardIndex;
		isBurstMenuOpen = true;
		sendMenuState(2, currentPlayerIndex, -1, cardIndex); // Notify opponent

		// Menu geometry (similar to Wisdom Boon)
		float w = 520, h = 260;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		burstMenuRect.set(x, y, w, h);
		float btnW = 300, btnH = 80;
		burstBtnDamage.set(x + (w - btnW) / 2, y + 110, btnW, btnH);
		burstBtnHeal.set(0, 0, 0, 0);
		return CARD_AWAITING_MENU_CHOICE;
	}

	// --- CASE: SMITE ---
	case CARD_SMITE: {
		// Target an adjacent unit and deal 5 holy damage
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex != -1) {
			Player * target = getPlayer(targetIndex);
			if (target) {
				bool did = applyDamageTo(*target, 5, DAMAGE_HOLY, currentPlayerIndex);
				playedSuccessfully = did;
			}
		}
		break;
	}

		// --- CASE: SHOOT ARROW ---
	case CARD_SHOOT_ARROW: {
		// Line-of-sight ranged attack (range = 2 * 20 = 40 ft). Choose target first (handled by click), then roll 1d6 piercing.
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };
		float maxRange = 2.0f * 20.0f; // 2d20 ft

		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, maxRange, playedCard.type);
		if (validationResult.reason != VALID) break;

		// Must target a unit tile
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		// Start hit roll first (2d20) to determine if arrow reaches target
		pendingShootArrowHitResult = startDiceRoll(2, 20, PURPOSE_RANGE, "Shoot Arrow: Range", currentPlayerIndex);
		isWaitingForShootArrow = true;
		pendingShootArrowTargetTile = targetTile;
		pendingShootArrowTargetIndex = targetIndex;
		playedSuccessfully = true;
		break;
	}

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
			targetPlayer->deck.pop_back();
			currentPlayer.deck.push_back(stolenCard);
			shuffleGameVector(currentPlayer.deck, currentPlayerIndex);

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
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		Player * targetP = getPlayer(targetIndex);
		if (targetP->deck.empty()) break;

		amnesiaTargetPlayerIndex = targetIndex;

		pendingAmnesiaRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DEBUG, "Amnesia: Cards to Remove");
		isWaitingForAmnesiaDice = true;
		return CARD_PLAYED_IMMEDIATELY; // We've already handled cleanup above
	}

	// --- CASE: MAGIC BLAST (Confirming fix from previous step) ---
	case CARD_MAGIC_BLAST: {
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };

		// FIX: Range is numDice * diceSides (e.g. 1 * 20 = 20ft)
		float maxRange = (float)(playedCard.numDice * playedCard.diceSides);

		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, maxRange, playedCard.type);

		if (validationResult.reason != VALID) break;

		// Validation: Must hit unit or have adjacent unit
		bool hasValidUnit = false;
		if (board[targetX][targetY].hasPlayer) hasValidUnit = true;
		if (!hasValidUnit) {
			glm::vec2 neighbors[4] = { { targetX + 1, targetY }, { targetX - 1, targetY }, { targetX, targetY + 1 }, { targetX, targetY - 1 } };
			for (const auto & n : neighbors) {
				if (n.x >= 0 && n.x < BOARD_WIDTH && n.y >= 0 && n.y < BOARD_HEIGHT && board[(int)n.x][(int)n.y].hasPlayer) {
					hasValidUnit = true;
					break;
				}
			}
		}
		if (!hasValidUnit) break;

		pendingMagicBlastRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE, "Magic Blast: Range Check");

		isWaitingForMagicBlastDice = true;
		pendingMagicBlastTargetTile = targetTile;
		playedSuccessfully = true;
		break;
	}

		// --- CASE: FIREBALL (Confirming fix from previous step) ---
	case CARD_FIREBALL: {
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };

		// FIX: Range is numDice * diceSides (e.g. 2 * 6 = 12ft)
		float maxRange = (float)(playedCard.numDice * playedCard.diceSides);

		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, maxRange, playedCard.type);

		if (validationResult.reason != VALID || !board[targetX][targetY].hasPlayer) break;

		pendingFireballRangeResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE, "Fireball: Range Check");
		isWaitingForFireballRangeDice = true;
		pendingFireballTargetTile = targetTile;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: ROCK CRUSH ---
	case CARD_ROCK_CRUSH: {
		if (board[targetX][targetY].hasWall) {
			board[targetX][targetY].hasWall = false;
			buildLevelMesh();
			playedSuccessfully = true;
		} else {
			int targetIndex = -1;
			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].x == targetX && players[i].y == targetY) {
					targetIndex = (int)i;
					break;
				}
			}
			if (targetIndex != -1) {

				pendingAttackRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DAMAGE, "Rock Crush: Damage", currentPlayerIndex);

				isWaitingForAttackDice = true;
				pendingAttackDamageType = playedCard.damageType;
				pendingAttackTargetIndices.clear();
				pendingAttackTargetIndices.push_back(targetIndex);
				playedSuccessfully = true;
			}
		}
		break;
	}

	// --- CASE: DEMOLITION ---
	case CARD_DEMOLITION: {
		// Only allow targeted adjacent walls (player must click an adjacent tile)
		int manhattan = abs(targetX - currentPlayer.x) + abs(targetY - currentPlayer.y);
		if (manhattan != 1) break;
		if (board[targetX][targetY].hasWall) {
			board[targetX][targetY].hasWall = false;
			buildLevelMesh();
			// Grant +6 AP next turn
			currentPlayer.nextTurnAPBonus += 6;
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+6 AP Next Turn", ofColor::yellow);
			playedSuccessfully = true;
		}
		break;
	}

	// --- CASE: DISPEL ---
	case CARD_DISPEL: {
		pendingDispelCardIndex = cardIndex;
		isDispelMenuOpen = true;

		// 1. Setup Panel Geometry (Standardized Size)
		float w = 600, h = 300;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		dispelMenuRect.set(x, y, w, h);

		// 2. Setup Button Geometry (Centered, wider buttons)
		float btnWidth = 260; // Wider to fit "Non-Phys Barrier"
		float btnHeight = 80;
		float spacing = 30;

		// Calculate X to center the group of buttons
		float totalBtnWidth = (btnWidth * 2) + spacing;
		float startX = x + (w - totalBtnWidth) / 2;
		float btnY = y + 130;

		dispelBtnBarrier.set(startX, btnY, btnWidth, btnHeight);
		dispelBtnPurge.set(startX + btnWidth + spacing, btnY, btnWidth, btnHeight);
		return CARD_AWAITING_MENU_CHOICE;
	}
	// --- CASE: TELEPORT (Now handled via drag -> dice roll -> targeting) ---
	case CARD_TELEPORT: {
		// Teleport is now initiated from mouseReleased, not playCard
		// This case should not be reached in normal gameplay
		break;
	}
	// --- CASE: HASTEN ---
	case CARD_HASTEN: {
		currentPlayer.nextTurnD10AP = true;
		currentPlayer.nextTurnExtraDraw = true;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: REPLICATE ---
	case CARD_REPLICATE: {
		currentPlayer.isReplicatePending = true;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: WISDOM BOON ---
	case CARD_WISDOM_BOON: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		Player * t = getPlayer(targetIndex);
		bool isSelf = (targetIndex == currentPlayerIndex);
		bool isAdjacent = (abs(t->x - currentPlayer.x) + abs(t->y - currentPlayer.y) == 1);

		if (!isSelf && !isAdjacent) break;

		pendingWisdomBoonCardIndex = cardIndex;
		pendingWisdomBoonTargetIndex = targetIndex;
		isWisdomBoonMenuOpen = true;
		sendMenuState(1, targetIndex, -1, cardIndex); // Notify opponent
		// Menu geometry
		float w = 600, h = 300;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		wisdomMenuRect.set(x, y, w, h);
		wisdomBtnDamage.set(x + (w - 300) / 2, y + 150, 300, 80);
		wisdomBtnBlock.set(0, 0, 0, 0);
		return CARD_AWAITING_MENU_CHOICE;
	}

		// --- CASE: HEAL ---
	case CARD_HEAL: {
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };

		// FIX: Heal has infinite range (Line of Sight only)
		float maxRange = 9999.0f;

		// This function will still fail if there is a Wall blocking the view
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, maxRange, playedCard.type);

		if (validationResult.reason != VALID) break;

		// Find Target Unit
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		// Friendly Check (Cannot heal enemies)
		Player * target = getPlayer(targetIndex);
		int casterOwner = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		int targetOwner = target->isMinion ? target->ownerID : target->playerID;

		if (casterOwner != targetOwner) {
			ofLogNotice("Heal") << "Cannot heal enemies!";
			break;
		}

		// Apply
		pendingHealTargetIndex = targetIndex;

		// Roll for Amount (Not Range)
		pendingHealRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_HEALING, "Heal: HP Amount");

		isWaitingForHealDice = true;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: ETHEREAL JOLT ---
	case CARD_ETHEREAL_JOLT: {
		glm::vec2 targetTile = { (float)targetX, (float)targetY };
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };

		// FIX: Range is numDice * diceSides (e.g. 1 * 20 = 20ft)
		float maxRange = (float)(playedCard.numDice * playedCard.diceSides);

		// Validation Check (Reduces red error messages if you click too far)
		// Pass the card type so it knows to ignore walls for LOS
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, maxRange, playedCard.type);

		if (validationResult.reason != VALID) break;

		pendingJoltRangeResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE, "Ethereal Jolt: Range Check");

		isWaitingForJoltRangeDice = true;
		pendingJoltTargetTile = targetTile;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: FLAME HIT ---
	case CARD_FLAME_HIT: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		Player * target = getPlayer(targetIndex);
		bool healthHit = applyDamageTo(*target, playedCard.value, DAMAGE_FIRE, currentPlayerIndex);
		if (healthHit) target->onFire = true;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: RAISE DEAD ---
	case CARD_RAISE_DEAD: {
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) break;

		// 1. Roll for HP
		pendingSummonTile = glm::vec2(targetX, targetY);
		pendingSummonPlayerIndex = currentPlayerIndex; // Track which player summoned

		pendingSummonRollResult = startDiceRoll(1, 6, PURPOSE_HP, "Raise Dead: Skeleton HP");

		isWaitingForSummonHealth = true;

		// --- CRASH PREVENTION FIX ---
		// Perform cleanup NOW before the players vector potentially changes
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);

		// Show played card animation (right side, no scale-up)
		PlayedCardAnimation cardAnim;
		cardAnim.card = playedCard;
		cardAnim.startTime = ofGetElapsedTimef();
		float handBaseCardWidth = 120.0f;
		float w = handBaseCardWidth * 2.6f;
		cardAnim.pos = glm::vec2(ofGetWidth() - (w / 2.0f) - 40.0f, ofGetHeight() / 2.0f);
		cardAnim.currentScale = 2.6f;
		cardAnim.currentAlpha = 255.0f;
		activePlayedCardAnimations.push_back(cardAnim);

		// Handle Replicate
		if (currentPlayer.isReplicatePending) {
			currentPlayer.playedCardsPile.push_back(playedCard);
			currentPlayer.isReplicatePending = false;
		}

		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		{
			PlayedCardDisplay disp;
			disp.card = playedCard;
			disp.startTime = ofGetElapsedTimef();
			disp.startPos = getCardDisplayUIPosition(currentPlayerIndex);
			disp.currentPos = disp.startPos;
			activeCardDisplays.push_back(disp);
		}
		invalidateTargetCache();
		// -----------------------------

		// We set this to false because we handled the cleanup manually above.
		// We don't want the bottom block to run again.
		return CARD_PLAYED_IMMEDIATELY;
	}

	// --- CASE: SUMMON KOBOLD KING ---
	case CARD_SUMMON_KOBOLD_KING: {
		// Validation: Must be empty adjacent tile
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) break;

		// 1. Calculate Stats based on existing Kobolds
		int koboldCount = 0;
		for (const auto & p : players) {
			if (p.isKobold) koboldCount++;
		}
		int kingHP = koboldCount + 1;

		// 2. Create Unit
		Player minion;
		minion.playerID = 4000 + (int)players.size(); // 4000 series for Kings
		minion.x = targetX;
		minion.y = targetY;
		minion.maxHealth = kingHP;
		minion.health = kingHP; // Starts full

		minion.isMinion = true;
		minion.isKoboldKing = true;
		minion.isKobold = false; // Explicitly NOT a kobold

		// Owner logic
		minion.ownerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

		// 3. Build Deck
		auto findCard = [&](string name, CardType type) -> Card {
			// Prefer exact name match first (so multiple cards sharing the same type
			// like Slash and Stab are distinguishable). Fall back to first matching
			// type if name isn't found.
			for (const auto & c : allCards) {
				if (c.name == name) return c;
			}
			for (const auto & c : allCards) {
				if (c.type == type) return c;
			}
			return Card();
		};

		Card slash = findCard("Slash", CARD_ATTACK_SINGLE_TILE);
		Card stab = findCard("Stab", CARD_ATTACK_SINGLE_TILE);
		Card fullRestore = findCard("Full Restore", CARD_FULL_RESTORE);
		Card callKobolds = findCard("Call for Kobolds", CARD_CALL_FOR_KOBOLDS);

		// Deck: 2x Slash, 2x Stab, 2x Full Restore, 1x Call for Kobolds
		minion.deck = { slash, slash, stab, stab, fullRestore, fullRestore, callKobolds };

		// --- CRITICAL FIX START ---
		// We must modify 'currentPlayer' BEFORE we push_back to 'players'.
		// Pushing back might resize the vector, invalidating the 'currentPlayer' reference.

		int myID = currentPlayer.playerID; // Save ID to find index later

		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);

		// Show played card animation (right side, no scale-up)
		PlayedCardAnimation cardAnim;
		cardAnim.card = playedCard;
		cardAnim.startTime = ofGetElapsedTimef();
		float handBaseCardWidth = 120.0f;
		float w = handBaseCardWidth * 2.6f;
		cardAnim.pos = glm::vec2(ofGetWidth() - (w / 2.0f) - 40.0f, ofGetHeight() / 2.0f);
		cardAnim.currentScale = 2.6f;
		cardAnim.currentAlpha = 255.0f;
		activePlayedCardAnimations.push_back(cardAnim);

		if (currentPlayer.isReplicatePending) {
			currentPlayer.playedCardsPile.push_back(playedCard);
			currentPlayer.isReplicatePending = false;
		}

		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);

		{
			PlayedCardDisplay disp;
			disp.card = playedCard;
			disp.startTime = ofGetElapsedTimef();
			disp.startPos = getCardDisplayUIPosition(currentPlayerIndex);
			disp.currentPos = disp.startPos;
			activeCardDisplays.push_back(disp);
		}
		invalidateTargetCache();
		// --- CRITICAL FIX END ---

		// 4. Add to Board (Now safe to resize vector)
		board[targetX][targetY].hasPlayer = true;
		players.push_back(minion);
		// Authoritative shuffle for the new minion deck
		int newKoboldKingIdx = (int)players.size() - 1;
		shuffleGameVector(players[newKoboldKingIdx].deck, newKoboldKingIdx);

		ofLogNotice("Summon") << "Kobold King summoned with " << kingHP << " HP.";

		// 5. Sort turn order
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
		});

		// 6. Restore Index (Find where the current player moved to after sorting)
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == myID) {
				currentPlayerIndex = i;
				break;
			}
		}

		return CARD_PLAYED_IMMEDIATELY; // Cleanup handled manually above
	}

	// --- CASE: SUMMON ASSISTANT ---
	case CARD_SUMMON_ASSISTANT: {
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) break;

		// 1. Create Unit
		Player minion;
		minion.playerID = 5000 + (int)players.size();
		minion.x = targetX;
		minion.y = targetY;
		minion.maxHealth = 1;
		minion.health = 1;
		minion.isMinion = true;
		minion.isAssistant = true;

		// 2. Link to Summoner (Critical for Luck Aura/Reroll)
		minion.directSummonerID = currentPlayer.playerID;

		// Standard Owner/Turn logic
		minion.ownerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

		// 3. Deck: 1x Lesser Heal, 4x Hand Block
		auto findCard = [&](string name, CardType type) -> Card {
			for (const auto & c : allCards) {
				if (c.type == type) return c;
				if (c.name == name) return c;
			}
			return Card();
		};
		Card lesserHeal = findCard("Lesser Heal", CARD_LESSER_HEAL);
		Card handBlock = findCard("Hand Block", CARD_GAIN_BLOCK);

		minion.deck = { lesserHeal, handBlock, handBlock, handBlock, handBlock };

		// 4. Cleanup & Add
		int myID = currentPlayer.playerID;
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);

		// Show played card animation (right side, no scale-up)
		PlayedCardAnimation cardAnim;
		cardAnim.card = playedCard;
		cardAnim.startTime = ofGetElapsedTimef();
		float handBaseCardWidth = 120.0f;
		float w = handBaseCardWidth * 2.6f;
		cardAnim.pos = glm::vec2(ofGetWidth() - (w / 2.0f) - 40.0f, ofGetHeight() / 2.0f);
		cardAnim.currentScale = 2.6f;
		cardAnim.currentAlpha = 255.0f;
		activePlayedCardAnimations.push_back(cardAnim);

		if (currentPlayer.isReplicatePending) {
			currentPlayer.playedCardsPile.push_back(playedCard);
			currentPlayer.isReplicatePending = false;
		}
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();

		board[targetX][targetY].hasPlayer = true;
		players.push_back(minion);
		int newAssistantIdx = (int)players.size() - 1;
		shuffleGameVector(players[newAssistantIdx].deck, newAssistantIdx);

		ofLogNotice("Summon") << "Assistant summoned.";

		// Sort & Restore Index
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
		});
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == myID) {
				currentPlayerIndex = i;
				break;
			}
		}

		return CARD_PLAYED_IMMEDIATELY;
	}

	// --- CASE: SUMMON FAERIE ---
	case CARD_SUMMON_FAERIE: {
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) break;

		// 1. Create Faerie Unit
		Player minion;
		minion.playerID = 6000 + (int)players.size();
		minion.x = targetX;
		minion.y = targetY;
		minion.maxHealth = 5;
		minion.health = 5;
		minion.isMinion = true;
		minion.isFaerie = true;
		minion.hasRegeneration = true;
		minion.originalModelType = "Faerie";

		// Faerie AP: 1d4 per turn (handled in AP logic)

		// 2. Link to Summoner
		minion.directSummonerID = currentPlayer.playerID;
		minion.ownerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

		// 3. Deck: 2x Dispel, 2x Lesser Heal, 1x Magic Blast
		auto findCard = [&](string name, CardType type) -> Card {
			for (const auto & c : allCards) {
				if (c.type == type && c.name == name) return c;
			}
			for (const auto & c : allCards) {
				if (c.type == type) return c;
			}
			for (const auto & c : allCards) {
				if (c.name == name) return c;
			}
			return Card();
		};
		Card dispel = findCard("Dispel", CARD_DISPEL);
		Card lesserHeal = findCard("Lesser Heal", CARD_LESSER_HEAL);
		Card magicBlast = findCard("Magic Blast", CARD_MAGIC_BLAST);
		minion.deck = { dispel, dispel, lesserHeal, lesserHeal, magicBlast };

		// 4. Cleanup & Add
		int myID = currentPlayer.playerID;
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);
		if (currentPlayer.isReplicatePending) {
			currentPlayer.playedCardsPile.push_back(playedCard);
			currentPlayer.isReplicatePending = false;
		}
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();

		board[targetX][targetY].hasPlayer = true;
		players.push_back(minion);
		int newFaerieIdx = (int)players.size() - 1;
		shuffleGameVector(players[newFaerieIdx].deck, newFaerieIdx);

		ofLogNotice("Summon") << "Faerie summoned.";

		// Sort & Restore Index
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
		});
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == myID) {
				currentPlayerIndex = i;
				break;
			}
		}

		return CARD_PLAYED_IMMEDIATELY;
	}

	// --- CASE: FULL RESTORE ---
	case CARD_FULL_RESTORE: {
		// Target is Self
		Player & target = currentPlayer; // Since targeting is TARGET_SELF

		// 1. Heal to Max
		target.health = target.maxHealth;

		// 2. Remove Status Effects
		target.onFire = false;
		target.isPoisoned = false;
		target.poisonReduction = 0;
		target.isParalyzed = false;
		target.paralysisHeadsCount = 0;
		target.sleepTurnsRemaining = 0;

		spawnFloatingText(gridToWorld(target.x, target.y), "Fully Restored!", ofColor::gold);
		ofLogNotice("Full Restore") << "Unit " << target.playerID << " healed to " << target.maxHealth << " and cured.";

		playedSuccessfully = true;
		break;
	}

	// --- CASE: SUMMON GOLEM ---
	case CARD_SUMMON_GOLEM: {
		// 1. Validation
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) break;

		// 2. Determine Golem Type
		bool isElectric = false;
		bool isFire = false;
		bool isRock = false;

		for (CardType t : currentPlayer.cardsPlayedThisTurn) {
			if (t == CARD_SHOCK || t == CARD_CHAIN_LIGHTNING) isElectric = true;
			if (t == CARD_FIREBALL || t == CARD_FLAME_HIT) isFire = true;
			if (t == CARD_ROCK_CRUSH) isRock = true;
		}

		// 3. Setup Minion Data
		Player minion;
		minion.playerID = 100 + (int)players.size();
		minion.x = targetX;
		minion.y = targetY;
		minion.isMinion = true;
		minion.isGolem = true;

		// Set owner and summoning sickness
		minion.ownerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

		// 4. Apply Variant Stats & Deck
		if (isElectric) {
			ofLogNotice("Summon") << "Combo! Summoning ELECTRIC Golem.";
			minion.minionTexture = &golemTexElectric;
			minion.maxHealth = startDiceRoll(1, 6, PURPOSE_HP, "Electric Golem: HP");
			for (const auto & c : allCards) {
				if (c.type == CARD_SHOCK) {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
				if (c.type == CARD_GAIN_BLOCK && c.name == "Hand Block") {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
			}
		} else if (isFire) {
			ofLogNotice("Summon") << "Combo! Summoning FIRE Golem.";
			minion.minionTexture = &golemTexFire;
			minion.maxHealth = startDiceRoll(1, 10, PURPOSE_HP, "Fire Golem: HP");
			for (const auto & c : allCards) {
				if (c.type == CARD_FIREBALL) minion.deck.push_back(c);
				if (c.type == CARD_FLAME_HIT) {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
				if (c.type == CARD_GAIN_BLOCK && c.name == "Hand Block") {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
			}
		} else if (isRock) {
			ofLogNotice("Summon") << "Combo! Summoning ROCK Golem.";
			minion.minionTexture = &golemTexRock;
			minion.maxHealth = startDiceRoll(1, 20, PURPOSE_HP, "Rock Golem: HP");
			for (const auto & c : allCards) {
				if (c.type == CARD_ROCK_CRUSH) {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
				if (c.name == "Bash") {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
				if (c.type == CARD_GAIN_BLOCK && c.name == "Hand Block") {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
			}
		} else {
			ofLogNotice("Summon") << "Summoning Standard Golem.";
			minion.minionTexture = &golemTexBase;
			minion.maxHealth = startDiceRoll(1, 10, PURPOSE_HP, "Standard Golem: HP");
			for (const auto & c : allCards) {
				if (c.name == "Bash") {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
				if (c.type == CARD_GAIN_BLOCK && c.name == "Hand Block") {
					minion.deck.push_back(c);
					minion.deck.push_back(c);
				}
			}
		}

		minion.health = minion.maxHealth;

		// --- CRASH FIX START ---
		int myID = currentPlayer.playerID;

		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type); // Track history
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);

		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();
		// --- CRASH FIX END ---

		// 5. Add to board
		board[targetX][targetY].hasPlayer = true;
		players.push_back(minion);
		int newGolemIdx = (int)players.size() - 1;
		shuffleGameVector(players[newGolemIdx].deck, newGolemIdx);

		// Sort turn order
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
		});

		// Find our new index
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == myID) {
				currentPlayerIndex = i;
				break;
			}
		}
		return CARD_PLAYED_IMMEDIATELY;
	}

		// --- CASE: TRANSFORM WALL ---
	case CARD_TRANSFORM_WALL: {
		// Debug: log attempted transform target and wall state
		ofLogNotice("Transform") << "Attempting Transform Wall at (" << targetX << "," << targetY << ") hasWall=" << (board[targetX][targetY].hasWall ? "true" : "false");
		// Must be a wall
		if (!board[targetX][targetY].hasWall) {
			spawnFloatingText(gridToWorld(targetX, targetY), "No wall to transform", ofColor::red);
			break;
		}

		// 1. Determine Type (Magic vs Normal)
		bool isMagic = board[targetX][targetY].isMagicWall;

		// Save current player's id safely (in case vector reallocates and indices shift)
		int savedCurrentID = (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) ? players[currentPlayerIndex].playerID : -1;

		// 2. Create Unit
		Player minion;
		minion.playerID = 3000 + (int)players.size();
		minion.x = targetX;
		minion.y = targetY;
		minion.isMinion = true;
		minion.isWallUnit = true;
		minion.isMagicWallUnit = isMagic;

		// Owner/Summon Logic
		minion.ownerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		minion.summonedOnTurnCycle = globalTurnCounter;
		minion.summonOrder = ++nextSummonOrder;

		// Attach wall unit diffuse so rendering uses model texture
		minion.minionTexture = wallUnitTexture.isAllocated() ? &wallUnitTexture : nullptr;

		// 3. Stats & Deck
		auto findCardByType = [&](CardType t) -> Card {
			for (const auto & c : allCards)
				if (c.type == t) return c;
			return Card();
		};

		if (isMagic) {
			minion.maxHealth = 7;
			minion.health = 7;
			// Deck: 2x Fortify, 2x Magic Blast, 1x Summon Wall
			Card fort = findCardByType(CARD_FORTIFY);
			Card mblast = findCardByType(CARD_MAGIC_BLAST);
			Card createWall = findCardByType(CARD_CREATE_WALL);
			if (fort.type != CARD_NONE) {
				minion.deck.push_back(fort);
				minion.deck.push_back(fort);
			}
			if (mblast.type != CARD_NONE) {
				minion.deck.push_back(mblast);
				minion.deck.push_back(mblast);
			}
			if (createWall.type != CARD_NONE) minion.deck.push_back(createWall);
			ofLogNotice("Transform") << "Created Magic Wall Unit (7 HP).";
		} else {
			minion.maxHealth = 5;
			minion.health = 5;
			// Deck: 2x Fortify, 2x Ward, 1x Summon Wall
			Card fort = findCardByType(CARD_FORTIFY);
			Card ward = findCardByType(CARD_GAIN_WARD);
			Card createWall = findCardByType(CARD_CREATE_WALL);
			if (fort.type != CARD_NONE) {
				minion.deck.push_back(fort);
				minion.deck.push_back(fort);
			}
			if (ward.type != CARD_NONE) {
				minion.deck.push_back(ward);
				minion.deck.push_back(ward);
			}
			if (createWall.type != CARD_NONE) minion.deck.push_back(createWall);
			ofLogNotice("Transform") << "Created Wall Unit (5 HP).";
		}

		// 4. Transform Board State
		board[targetX][targetY].hasWall = false; // Remove static wall
		board[targetX][targetY].isMagicWall = false; // Clear flag (unit carries property now)
		board[targetX][targetY].hasPlayer = true; // Add unit
		players.push_back(minion);
		int newWallUnitIdx = (int)players.size() - 1;
		shuffleGameVector(players[newWallUnitIdx].deck, newWallUnitIdx);

		// Update Mesh (to remove the static wall visually)
		buildLevelMesh();

		// 4. Add to board
		players.push_back(minion);
		int newWallUnitIdx = (int)players.size() - 1;
		shuffleGameVector(players[newWallUnitIdx].deck, newWallUnitIdx);

		// 5. Sort Turn Order
		int currentID = savedCurrentID;
		std::sort(players.begin(), players.end(), [](const Player & a, const Player & b) {
			int ownerA = a.isMinion ? a.ownerID : a.playerID;
			int ownerB = b.isMinion ? b.ownerID : b.playerID;
			if (ownerA != ownerB) return ownerA < ownerB;
			if (a.isMinion && !b.isMinion) return true;
			if (!a.isMinion && b.isMinion) return false;
			return a.summonOrder < b.summonOrder;
		});

		// Restore Index
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].playerID == currentID) {
				currentPlayerIndex = i;
				break;
			}
		}
		invalidateTargetCache();

		break;
	}

	// --- CASE: CALL FOR WOLVES ---
	case CARD_CALL_FOR_WOLVES: {
		// 1. Check for valid adjacent space BEFORE playing
		bool hasSpace = false;
		int cx = currentPlayer.x;
		int cy = currentPlayer.y;
		glm::vec2 adj[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (auto & d : adj) {
			int nx = cx + (int)d.x;
			int ny = cy + (int)d.y;
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
				if (!board[nx][ny].hasWall && !board[nx][ny].hasPlayer) {
					hasSpace = true;
					break;
				}
			}
		}

		if (!hasSpace) {
			ofLogNotice("Wolves") << "No adjacent space to summon wolves!";
			spawnFloatingText(gridToWorld(cx, cy), "No Space!", ofColor::red);
			break; // Cancel card play
		}

		// 2. Pay Cost & Cleanup Hand
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);
		if (currentPlayer.isReplicatePending) {
			currentPlayer.playedCardsPile.push_back(playedCard);
			currentPlayer.isReplicatePending = false;
		}
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);

		// 3. Setup State for Wolf #1
		wolfPlacementSourceX = currentPlayer.x;
		wolfPlacementSourceY = currentPlayer.y;

		isPlacingWolves = true;
		wolfSummonStage = 1; // Start with the first wolf

		invalidateTargetCache();
		return CARD_PLAYED_IMMEDIATELY; // Cleanup handled manually
	}

		// --- CASE: CALL FOR KOBOLDS ---
	case CARD_CALL_FOR_KOBOLDS: {
		// 1. Check for valid adjacent space BEFORE playing
		bool hasSpace = false;
		int cx = currentPlayer.x;
		int cy = currentPlayer.y;
		glm::vec2 adj[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (auto & d : adj) {
			int nx = cx + (int)d.x;
			int ny = cy + (int)d.y;
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
				if (!board[nx][ny].hasWall && !board[nx][ny].hasPlayer) {
					hasSpace = true;
					break;
				}
			}
		}

		if (!hasSpace) {
			ofLogNotice("Kobolds") << "No adjacent space to summon kobolds!";
			spawnFloatingText(gridToWorld(cx, cy), "No Space!", ofColor::red);
			break; // Cancel card play
		}

		// 2. Pay Cost & Cleanup Hand
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);
		if (currentPlayer.isReplicatePending) {
			currentPlayer.playedCardsPile.push_back(playedCard);
			currentPlayer.isReplicatePending = false;
		}
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);

		// 3. Setup State for Kobold roll and placement
		koboldPlacementSourceX = currentPlayer.x;
		koboldPlacementSourceY = currentPlayer.y;

		// Roll 1d4 for number of kobolds
		pendingSummonRollResult = startDiceRoll(1, 4, PURPOSE_SUMMON_KOBOLDS, "Call for Kobolds");
		isWaitingForKoboldDice = true;

		invalidateTargetCache();
		return CARD_PLAYED_IMMEDIATELY; // Cleanup handled manually
	}

		// --- CASE: SUMMON HELLHOUND ---
	case CARD_SUMMON_HELLHOUND: {
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) break;

		pendingSummonTile = glm::vec2(targetX, targetY);
		// Important: This must match the variable checked in updateGame
		pendingSummonRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_HP, "Hellhound HP");
		isWaitingForHellhoundHP = true;

		// Cleanup Logic
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);
		// ... (Replicate logic) ...
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();

		return CARD_PLAYED_IMMEDIATELY; // Prevent double cleanup
	}

		// --- CASE: SUMMON DEMON ---
	case CARD_SUMMON_DEMON: {
		if (board[targetX][targetY].hasWall || board[targetX][targetY].hasPlayer) break;

		pendingSummonTile = glm::vec2(targetX, targetY);
		// Roll 3d10 for HP
		pendingSummonRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_HP, "Demon HP");
		isWaitingForDemonHP = true;

		// Cleanup
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);
		if (currentPlayer.isReplicatePending) {
			currentPlayer.playedCardsPile.push_back(playedCard);
			currentPlayer.isReplicatePending = false;
		}
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();

		return CARD_PLAYED_IMMEDIATELY;
	}

		// --- CASE: DEATH ---
	case CARD_DEATH: {
		// Range Check (Infinite / LOS)
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, 9999.0f, playedCard.type);

		if (validationResult.reason != VALID) break;

		// Find Target
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		Player * target = getPlayer(targetIndex);
		pendingDeathTargetIndex = targetIndex;

		// 1. Check if already asleep -> INSTANT DEATH
		if (target->sleepTurnsRemaining > 0) {
			spawnFloatingText(gridToWorld(target->x, target->y), "Nightmare!", ofColor::darkRed);

			// Kill logic
			DeathMarker death;
			death.x = target->x;
			death.y = target->y;
			death.turnDied = globalTurnCounter;
			death.deck = target->deck;
			graveyard.push_back(death);
			board[target->x][target->y].hasPlayer = false;
			target->x = -1000;
			target->health = 0;

			playedSuccessfully = true;
		}
		// 2. Otherwise -> Roll Death Check
		else {
			// Roll 1d20
			pendingDeathRollResult = startDiceRoll(1, 20, PURPOSE_DEATH_CHECK, "Death Check");
			isWaitingForDeathDice = true;
			playedSuccessfully = true;
		}
		break;
	}

	// --- CASE: STRENGTHEN ELEMENTS ---
	case CARD_STRENGTHEN_ELEMENTS: {
		// Set duration to 3 (Current Turn + Next 2 Turns)
		currentPlayer.strengthenElementsTurnsRemaining = 3;

		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Elemental Flow!", ofColor::orange);
		ofLogNotice("Game") << "Player " << currentPlayer.playerID << " strengthened elements for 3 turns.";

		playedSuccessfully = true;
		break;
	}

		// --- CASE: SHIELD BASH ---
	case CARD_SHIELD_BASH: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		// Validation: Must have a valid target (and usually must have block, but we allow 0 dmg hit)
		if (targetIndex != -1) {
			Player * target = getPlayer(targetIndex);

			// 1. Calculate Total Block
			int totalBlock = currentPlayer.block + currentPlayer.barrier + currentPlayer.ward + currentPlayer.holyBlock;

			// Check if Add Poison buff is active
			bool applyPoisonBuff = currentPlayer.nextAttackAddPoison;
			if (applyPoisonBuff) {
				currentPlayer.nextAttackAddPoison = false;
			}

			// 2. Deal Damage based on Total Block
			// Note: Even if 0 block, the card plays (wasting AP), consistent with other mechanics
			if (totalBlock > 0) {
				ofLogNotice("Shield Bash") << "Converting " << totalBlock << " total block into damage.";
				applyDamageTo(*target, totalBlock, DAMAGE_PHYSICAL, currentPlayerIndex);

				// Apply poison if buff was active
				if (applyPoisonBuff) {
					target->isPoisoned = true;
					target->poisonReduction = 0;
					glm::vec3 tPos = gridToWorld(target->x, target->y);
					spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Poisoned!", ofColor::green);
					pendingPoisonTargetIndices.clear();
					pendingPoisonTargetIndices.push_back(targetIndex);
					pendingPoisonAttackRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Poison Damage", currentPlayerIndex);
					isWaitingForPoisonAttackDice = true;
				}
			} else {
				spawnFloatingText(gridToWorld(target->x, target->y), "0 Damage", ofColor::gray);
			}

			// 3. Remove All Block from Caster
			currentPlayer.block = 0;
			currentPlayer.barrier = 0;
			currentPlayer.ward = 0;
			currentPlayer.holyBlock = 0;

			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Shields Broken!", ofColor::yellow);

			playedSuccessfully = true;
		}
		break;
	}
		// --- CASE: CONSUME HEALTH POTION ---
	case CARD_CONSUME_HEALTH_POTION: {
		// Increase max HP by 1 (does not heal to it)
		currentPlayer.maxHealth++;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+1 Max HP", ofColor::cyan);
		ofLogNotice("Consume Health Potion") << "Player " << currentPlayer.playerID << " increased max HP to " << currentPlayer.maxHealth;
		playedSuccessfully = true;
		break;
	}
		// --- CASE: ADD POISON ---
	case CARD_ADD_POISON: {
		// Buff: next physical/piercing damage card this turn adds 1d6 poison damage
		currentPlayer.nextAttackAddPoison = true;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Poison Ready!", ofColor::green);
		ofLogNotice("Add Poison") << "Player " << currentPlayer.playerID << " primed next attack with poison.";
		playedSuccessfully = true;
		break;
	}
		// --- CASE: FLURRY OF FISTS ---
	case CARD_FLURRY_OF_FISTS: {
		// Find adjacent target
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		Player * target = getPlayer(targetIndex);

		// 1. Deal 2 physical damage (doubled if flurry already active)
		int damage = currentPlayer.flurryOfFistsActive ? 4 : 2;
		applyDamageTo(*target, damage, DAMAGE_PHYSICAL, currentPlayerIndex);

		// 2. Activate flurry buff for this turn
		currentPlayer.flurryOfFistsActive = true;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Flurry!", ofColor::orange);

		// 3. Draw a card for free (without using draw action)
		if (!currentPlayer.deck.empty()) {
			Card drawnCard = currentPlayer.deck.back();
			currentPlayer.deck.pop_back();

			// Check if drawn card is hand-related
			bool isHandRelated = (drawnCard.name == "Punch" || drawnCard.name == "Hand Block" || drawnCard.name == "Bash" || drawnCard.name == "Drain Punch" || drawnCard.name == "Double Handed" || drawnCard.name == "Master Fist" || drawnCard.name == "Flurry of Fists" || drawnCard.name == "Giant Magic Hand");

			if (isHandRelated) {
				// Make it cost 0 AP this turn
				drawnCard.cost = 0;
				spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.5f, 0),
					drawnCard.name + " (0 AP)!", ofColor::yellow);
			}

			currentPlayer.hand.push_back(drawnCard);
			ofLogNotice("Flurry of Fists") << "Drew " << drawnCard.name << (isHandRelated ? " (free this turn)" : "");
		} else if (!currentPlayer.discardPile.empty()) {
			// Reshuffle discard into deck first
			currentPlayer.deck = currentPlayer.discardPile;
			currentPlayer.discardPile.clear();
			shuffleGameVector(currentPlayer.deck, currentPlayerIndex);
			Card drawnCard = currentPlayer.deck.back();
			currentPlayer.deck.pop_back();

			bool isHandRelated = (drawnCard.name == "Punch" || drawnCard.name == "Hand Block" || drawnCard.name == "Bash" || drawnCard.name == "Drain Punch" || drawnCard.name == "Double Handed" || drawnCard.name == "Master Fist" || drawnCard.name == "Flurry of Fists" || drawnCard.name == "Giant Magic Hand");

			if (isHandRelated) {
				drawnCard.cost = 0;
				spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.5f, 0),
					drawnCard.name + " (0 AP)!", ofColor::yellow);
			}

			currentPlayer.hand.push_back(drawnCard);
		}

		playedSuccessfully = true;
		break;
	}

	// --- CASE: FORM OF TORTOISE ---
	case CARD_FORM_OF_TORTOISE: {
		// Cannot enter form if already in form
		if (currentPlayer.inTortoiseForm) {
			ofLogNotice("Form of Tortoise") << "Already in tortoise form!";
			break;
		}

		// 1. Store original model type for later restoration
		if (currentPlayer.isSkeleton)
			currentPlayer.originalModelType = "skeleton";
		else if (currentPlayer.isGolem)
			currentPlayer.originalModelType = "golem";
		else if (currentPlayer.isWolf)
			currentPlayer.originalModelType = "wolf";
		else if (currentPlayer.isHellhound)
			currentPlayer.originalModelType = "hellhound";
		else if (currentPlayer.isDemon)
			currentPlayer.originalModelType = "demon";
		else
			currentPlayer.originalModelType = "player";

		// 2. Activate tortoise form
		currentPlayer.inTortoiseForm = true;
		currentPlayer.tortoiseDamageTaken = 0;
		currentPlayer.tortoiseFormCard = playedCard;

		// 3. +5 max HP
		currentPlayer.maxHealth += 5;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+5 Max HP!", ofColor::limeGreen);

		// 4. Heal 5 HP
		int healAmount = std::min(5, currentPlayer.maxHealth - currentPlayer.health);
		currentPlayer.health += healAmount;
		if (healAmount > 0) {
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.3f, 0),
				"+" + ofToString(healAmount) + " HP", ofColor::green);
		}

		// 5. Shuffle 2x Dispel into deck
		Card dispelCard;
		bool foundDispel = false;
		for (const auto & c : allCards) {
			if (c.type == CARD_DISPEL) {
				dispelCard = c;
				foundDispel = true;
				break;
			}
		}
		if (foundDispel) {
			currentPlayer.deck.push_back(dispelCard);
			currentPlayer.deck.push_back(dispelCard);
			shuffleGameVector(currentPlayer.deck, currentPlayerIndex);
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.6f, 0),
				"+2 Dispel", ofColor::cyan);
		}

		// 6. Visual feedback
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.9f, 0),
			"TORTOISE FORM!", ofColor::darkGreen);

		ofLogNotice("Form of Tortoise") << "Player " << currentPlayer.playerID << " entered tortoise form.";

		// Card does NOT go to discard - stays "in play" until form ends
		// We handle this specially - don't add to playedCardsPile
		currentAP -= costToPay;
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		return CARD_PLAYED_IMMEDIATELY; // Skip normal cleanup since we handled AP and removal
	}

	// --- CASE: SUMMON WALL ---
	case CARD_CREATE_WALL: {
		if (!board[targetX][targetY].hasWall && !board[targetX][targetY].hasPlayer) {
			board[targetX][targetY].hasWall = true;
			buildLevelMesh();
			invalidateTargetCache();
			playedSuccessfully = true;
		}
		break;
	}

		// --- CASE: FORM OF GHOST ---
	case CARD_FORM_OF_GHOST: {
		if (currentPlayer.inGhostForm) {
			ofLogNotice("Form of Ghost") << "Already in ghost form!";
			break;
		}

		// 1. Activate Form
		currentPlayer.inGhostForm = true;
		currentPlayer.ghostDamageTaken = 0;
		currentPlayer.ghostFormCard = playedCard;

		// 2. Grant Regeneration (if not already active)
		if (!currentPlayer.hasRegeneration) {
			currentPlayer.hasRegeneration = true;
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Regeneration Gained", ofColor::green);
		}

		// 3. Visuals
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 1.0f, 0), "GHOST FORM!", ofColor::white);
		ofLogNotice("Form of Ghost") << "Player " << currentPlayer.playerID << " entered ghost form.";

		// 4. Handle "Keep in Play" (Do not add to played pile, just remove from hand)
		currentAP -= costToPay;
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();

		// Track play history
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);

		return CARD_PLAYED_IMMEDIATELY; // Skip standard cleanup
	}

	// --- CASE: FORTIFY ---
	case CARD_FORTIFY: {
		// Must target a wall tile
		if (!board[targetX][targetY].hasWall) break;

		// Flood-fill connected walls using 8-neighbor connectivity
		std::vector<glm::ivec2> stack;
		std::set<std::pair<int, int>> visited;
		stack.push_back({ targetX, targetY });

		while (!stack.empty()) {
			glm::ivec2 t = stack.back();
			stack.pop_back();
			int tx = t.x, ty = t.y;
			if (tx < 0 || tx >= BOARD_WIDTH || ty < 0 || ty >= BOARD_HEIGHT) continue;
			if (!board[tx][ty].hasWall) continue;
			if (visited.count({ tx, ty })) continue;
			visited.insert({ tx, ty });

			// push 8 neighbors
			for (int dx = -1; dx <= 1; dx++) {
				for (int dy = -1; dy <= 1; dy++) {
					if (dx == 0 && dy == 0) continue;
					int nx = tx + dx, ny = ty + dy;
					if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
						if (board[nx][ny].hasWall && !visited.count({ nx, ny })) {
							stack.push_back({ nx, ny });
						}
					}
				}
			}
		}

		int linkedCount = (int)visited.size();
		if (linkedCount <= 0) break;

		// Grant fortification equal to number of linked walls
		currentPlayer.fortification += linkedCount;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+" + ofToString(linkedCount) + " Fortify", ofColor::lightGray);
		ofLogNotice("Fortify") << "Player " << currentPlayer.playerID << " gained " << linkedCount << " fortification.";

		// Damage any other unit orthogonally adjacent to any of the linked walls (no diagonals, skip caster)
		std::set<int> damagedIndices;
		for (const auto & p : visited) {
			int wx = p.first, wy = p.second;
			// Only orthogonal directions
			const int ortho[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
			for (int d = 0; d < 4; ++d) {
				int ux = wx + ortho[d][0], uy = wy + ortho[d][1];
				if (ux < 0 || ux >= BOARD_WIDTH || uy < 0 || uy >= BOARD_HEIGHT) continue;
				for (size_t i = 0; i < players.size(); i++) {
					if (players[i].x == ux && players[i].y == uy) {
						if (players[i].playerID == currentPlayer.playerID) continue; // skip caster
						damagedIndices.insert((int)i);
					}
				}
			}
		}

		for (int idx : damagedIndices) {
			Player * target = getPlayer(idx);
			if (target) {
				applyDamageTo(*target, 3, DAMAGE_PHYSICAL, currentPlayerIndex);
			}
		}

		playedSuccessfully = true;
		break;
	}
		// --- CASE: DARK SHIELD ---
	case CARD_DARK_SHIELD: {
		currentPlayer.holyBlock += playedCard.value; // value is 7
		currentPlayer.nextTurnBonusDiceFromMinions = true;

		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y),
			"+" + ofToString(playedCard.value) + " Holy Block",
			ofColor::darkViolet);

		ofLogNotice("Game") << "Gained Dark Shield and bonus dice next turn.";
		playedSuccessfully = true;
		break;
	}

		// --- CASE: PSIONIC WAVE ---
	case CARD_PSIONIC_WAVE: {
		// Roll 2d20 for Range
		pendingPsionicRangeResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_PSIONIC_WAVE_RANGE, "Psionic Wave: Range");
		isWaitingForPsionicRange = true;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: DRAIN PUNCH ---
	case CARD_DRAIN_PUNCH: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		if (targetIndex != -1) {
			Player * target = getPlayer(targetIndex);

			// 1. Calculate Damage
			int damage = playedCard.value; // Base damage (2)

			// Check playedCardsPile for previous "hand-related" attacks this turn
			// Hand-related attack cards: Punch, Bash, Drain Punch, Master Fist, Flurry of Fists
			// (Double Handed and Hand Block are NOT attacks)
			for (const auto & c : currentPlayer.playedCardsPile) {
				if (c.name == "Punch" || c.name == "Bash" || c.name == "Drain Punch" || c.name == "Master Fist" || c.name == "Flurry of Fists" || c.name == "Giant Magic Hand") {
					damage += 2;
				}
			}

			// Double damage if Flurry of Fists is active
			if (currentPlayer.flurryOfFistsActive) {
				damage *= 2;
			}

			// Check if Add Poison buff is active
			bool applyPoisonBuff = currentPlayer.nextAttackAddPoison;
			if (applyPoisonBuff) {
				currentPlayer.nextAttackAddPoison = false;
			}

			// 2. Track Health Before Impact
			int hpBefore = target->health;

			// 3. Apply Damage
			applyDamageTo(*target, damage, DAMAGE_PHYSICAL, currentPlayerIndex);

			// 4. Apply poison if buff was active
			if (applyPoisonBuff) {
				target->isPoisoned = true;
				target->poisonReduction = 0;
				glm::vec3 tPos = gridToWorld(target->x, target->y);
				spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Poisoned!", ofColor::green);
				pendingPoisonTargetIndices.clear();
				pendingPoisonTargetIndices.push_back(targetIndex);
				pendingPoisonAttackRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Poison Damage", currentPlayerIndex);
				isWaitingForPoisonAttackDice = true;
			}

			// 5. Calculate Lifesteal (Actual HP lost by enemy)
			int hpAfter = target->health;
			int actualDamageDealt = hpBefore - hpAfter;

			// 5. Heal Caster
			if (actualDamageDealt > 0) {
				currentPlayer.health += actualDamageDealt;
				if (currentPlayer.health > currentPlayer.maxHealth) {
					currentPlayer.health = currentPlayer.maxHealth;
				}

				// Visual Feedback for Heal
				spawnFloatingText(
					gridToWorld(currentPlayer.x, currentPlayer.y),
					"+" + ofToString(actualDamageDealt) + " HP",
					ofColor::green);

				ofLogNotice("Drain Punch") << "Healed player for " << actualDamageDealt;
			}

			playedSuccessfully = true;
		}
		break;
	}

		// --- CASE: RENEWED INSPIRATION ---
	case CARD_RENEWED_INSPIRATION: {
		// 1. Pay Cost
		currentAP -= costToPay;

		// 2. Handle Replicate (BEFORE removing original from hand)
		// If Replicate is active, we create a copy, mark it as copied,
		// and put it in the hand immediately so it can be discarded for value.
		if (currentPlayer.isReplicatePending) {
			Card copy = playedCard; // Copy data
			copy.isCopied = true; // Mark as copied (Essential for eligibility)

			// Init position to match the card being played for a smooth visual pop-in
			copy.currentPos = currentPlayer.hand[cardIndex].currentPos;
			copy.targetPos = currentPlayer.hand[cardIndex].targetPos;
			copy.currentScale = currentPlayer.hand[cardIndex].currentScale;

			// Add to hand
			currentPlayer.hand.push_back(copy);
			currentPlayer.isReplicatePending = false;

			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "Replicated!", ofColor::cyan);
		}

		// 3. Move Original to Played Pile
		currentPlayer.playedCardsPile.push_back(playedCard);
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);

		// 4. Remove Original from Hand (Using iterator to be safe)
		if (cardIndex >= 0 && cardIndex < (int)currentPlayer.hand.size()) {
			currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		}

		// 5. Enter Selection Mode
		isSelectingRenewedInspiration = true;
		renewedSelectedHandIndices.clear();

		// 6. Setup UI Buttons
		float cx = ofGetWidth() / 2.0f;
		float cy = ofGetHeight() - 450.0f;
		if (currentPlayer.playerID == 1) cy = 350.0f;

		riConfirmBtn.set(cx - 110, cy, 100, 50);
		riCancelBtn.set(cx + 10, cy, 100, 50);

		// 7. Visuals & prevent auto-cleanup
		invalidateTargetCache();
		return CARD_AWAITING_MENU_CHOICE;
	}

	// --- CASE: VAMPIRE BITE ---
	case CARD_VAMPIRE_BITE: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		if (targetIndex != -1) {
			Player * target = getPlayer(targetIndex);

			// 1. Deal flat physical damage (value from card JSON)
			bool healthHit = applyDamageTo(*target, playedCard.value, DAMAGE_PHYSICAL, currentPlayerIndex);

			// 2. If the unit took HP damage, heal caster 2 HP and shuffle 1x Vampire Bite into that unit's deck
			if (healthHit) {
				currentPlayer.health += 2;
				if (currentPlayer.health > currentPlayer.maxHealth) currentPlayer.health = currentPlayer.maxHealth;
				spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+2 HP", ofColor::green);

				// Find the card template in allCards
				Card cardToAdd;
				bool found = false;
				for (const auto & c : allCards) {
					if (c.type == CARD_VAMPIRE_BITE) {
						cardToAdd = c;
						found = true;
						break;
					}
				}

				if (found) {
					target->deck.push_back(cardToAdd);
					shuffleGameVector(target->deck, targetIndex);
					spawnFloatingText(gridToWorld(target->x, target->y), "Shuffled 1x Vampire Bite", ofColor::magenta);
					ofLogNotice("Vampire Bite") << "Shuffled a Vampire Bite into Player " << target->playerID << "'s deck.";
				}
			}

			playedSuccessfully = true;
		}
		break;
	}

	// --- CASE: DOUBLE HANDED ---
	case CARD_DOUBLE_HANDED: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		// Safety check: Ensure target exists
		if (targetIndex == -1) break;

		// Open the Choice UI
		pendingDoubleHandedCardIndex = cardIndex;
		pendingDoubleHandedTargetIndex = targetIndex;
		isDoubleHandedMenuOpen = true;
		sendMenuState(3, targetIndex, -1, cardIndex); // Notify opponent

		// Setup UI Geometry
		float w = 500, h = 250;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		doubleHandedMenuRect.set(x, y, w, h);

		float btnW = 200, btnH = 80;
		float spacing = 40;
		btnAddPunches.set(x + (w - (btnW * 2 + spacing)) / 2, y + 120, btnW, btnH);
		btnAddBlocks.set(btnAddPunches.getRight() + spacing, y + 120, btnW, btnH);

		// Do NOT set playedSuccessfully = true yet.
		// We wait for the user to click a button.
		return CARD_AWAITING_MENU_CHOICE;
	}

		// --- CASE: NECROMANCER'S BLESSING ---
	case CARD_NECRO_BLESSING: {
		int skeletonCount = 0;

		// Determine the owner of the unit playing the card
		int ownerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;

		// Count skeletons owned by that player
		for (const auto & p : players) {
			if (p.isSkeleton) {
				int skeletonOwnerID = p.isMinion ? p.ownerID : p.playerID;
				if (skeletonOwnerID == ownerID) {
					skeletonCount++;
				}
			}
		}

		// Apply luck to the unit that played the card (even if it's 0)
		currentPlayer.luck += skeletonCount;

		if (skeletonCount > 0) {
			// Visual Feedback for gaining luck
			spawnFloatingText(
				gridToWorld(currentPlayer.x, currentPlayer.y),
				"+" + ofToString(skeletonCount) + " LUCK!",
				ofColor::purple);
			ofLogNotice("NecroBlessing") << "Unit " << currentPlayer.playerID << " gained " << skeletonCount << " luck.";
		} else {
			// Visual Feedback for gaining 0 luck
			spawnFloatingText(
				gridToWorld(currentPlayer.x, currentPlayer.y),
				"No Skeletons! (+0 Luck)",
				ofColor::gray);
			ofLogNotice("NecroBlessing") << "No skeletons found, no luck gained.";
		}

		playedSuccessfully = true;
		break;
	}

	// --- CASE: EARTHQUAKE ---
	case CARD_EARTHQUAKE: {
		// 2. Pay Cost & Cleanup Hand
		currentAP -= costToPay;
		currentPlayer.playedCardsPile.push_back(playedCard);
		// (Handle Replicate logic here if you want)
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);

		// 2. Initialize Earthquake System
		isEarthquakeActive = true;
		isEarthquakeDiceRolling = true;
		isEarthquakeAnimatingStep = false;
		earthquakeUnits.clear();

		// 3. Setup Units & Roll Dice
		// We assign a random direction NOW, but distance comes from dice later
		for (int i = 0; i < (int)players.size(); ++i) {
			EarthquakeState state;
			state.playerIndex = i;
			state.startGrid = { players[i].x, players[i].y };
			state.visualPos = gridToWorld(players[i].x, players[i].y);
			state.isMoving = true;
			state.crashed = false;
			state.tilesToMove = 0;
			state.originalDistance = 0;

			// Random Direction (N, E, S, W)
			std::uniform_int_distribution<int> dirDist(0, 3);
			int r = dirDist(gameplayRNG);
			if (r == 0)
				state.direction = { 0, 1 }; // South
			else if (r == 1)
				state.direction = { 0, -1 }; // North
			else if (r == 2)
				state.direction = { 1, 0 }; // East
			else
				state.direction = { -1, 0 }; // West

			// Roll 1d4 for this unit and record which dice slot we created
			int before = (int)activeDiceRolls.size();
			startDiceRoll(1, 4, PURPOSE_EARTHQUAKE_DISTANCE, "Quake Dist");
			int after = (int)activeDiceRolls.size();
			if (after > before) {
				state.diceIndex = after - 1;
				// mark associated unit on the dice so resolution can find it reliably
				activeDiceRolls[state.diceIndex].associatedUnit = i;
			} else {
				state.diceIndex = -1;
			}
			earthquakeUnits.push_back(state);
		}

		return CARD_PLAYED_IMMEDIATELY; // Cleanup handled above
	}

		// --- CASE: TIME VORTEX ---
	case CARD_TIME_VORTEX: {
		// Start the dice roll and set the waiting flag
		isWaitingForTimeVortexDice = true;

		pendingTimeVortexResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_TIME_VORTEX, "Time Vortex: Extra Turns");

		// Don't apply the turns yet. We wait for the dice animation.

		playedSuccessfully = true;
		break;
	}

		// --- CASE: GIANT MAGIC HAND ---
	case CARD_GIANT_MAGIC_HAND: {
		// Must target a wall
		if (!board[targetX][targetY].hasWall) break;

		// Save state and open UI
		pendingMagicHandCardIndex = cardIndex;
		magicHandTargetTile = { targetX, targetY };
		isMagicHandMenuOpen = true;

		// Define UI Rect
		float w = 500, h = 250;
		float mx = ofGetWidth() / 2 - w / 2, my = ofGetHeight() / 2 - h / 2;
		// Re-use wisdom menu rect variable or create new one. Let's reuse wisdomMenuRect for layout simplicity
		wisdomMenuRect.set(mx, my, w, h);

		// Do not set playedSuccessfully yet
		return CARD_AWAITING_MENU_CHOICE;
	}

		// --- CASE: MASTER FIST ---
	case CARD_MASTER_FIST: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}

		if (targetIndex != -1) {
			Player * target = getPlayer(targetIndex);

			// --- 1. Calculate Combo Damage ---
			int damage = 0; // Starts at 0 (Removed base 2)
			int handCardsInDiscard = 0;

			// Define which cards count as "hand-related" attack cards
			std::vector<std::string> handAttackNames = { "Punch", "Bash", "Drain Punch", "Master Fist", "Flurry of Fists", "Giant Magic Hand" };

			// Count matching cards in the caster's discard pile
			for (const auto & cardInPile : currentPlayer.discardPile) {
				for (const auto & name : handAttackNames) {
					if (cardInPile.name == name) {
						handCardsInDiscard++;
						break;
					}
				}
			}

			damage = (handCardsInDiscard * 2);
			ofLogNotice("Master Fist") << "Found " << handCardsInDiscard << " hand cards in discard. Total damage: " << damage;

			// Double damage if Flurry of Fists is active
			if (currentPlayer.flurryOfFistsActive) {
				damage *= 2;
				ofLogNotice("Master Fist") << "Flurry doubled damage to: " << damage;
			}

			// Check if Add Poison buff is active
			bool applyPoisonBuff = currentPlayer.nextAttackAddPoison;
			if (applyPoisonBuff) {
				currentPlayer.nextAttackAddPoison = false;
			}

			// --- 2. Apply Damage ---
			if (damage > 0) {
				applyDamageTo(*target, damage, DAMAGE_PHYSICAL, currentPlayerIndex);
			} else {
				spawnFloatingText(gridToWorld(target->x, target->y), "0 Damage (Empty Discard)", ofColor::gray);
			}

			// Apply poison if buff was active
			if (applyPoisonBuff) {
				target->isPoisoned = true;
				target->poisonReduction = 0;
				glm::vec3 tPos = gridToWorld(target->x, target->y);
				spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Poisoned!", ofColor::green);
				pendingPoisonTargetIndices.clear();
				pendingPoisonTargetIndices.push_back(targetIndex);
				pendingPoisonAttackRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Poison Damage");
				isWaitingForPoisonAttackDice = true;
			}

			// --- 3. Mill Target's Top Card ---
			if (!target->deck.empty()) {
				Card removedCard = target->deck.back();
				target->deck.pop_back();

				// Visuals for Mill
				RemovedCardAnimation anim;
				anim.card = removedCard;
				anim.startPos = gridToWorld(target->x, target->y);
				anim.startTime = ofGetElapsedTimef();
				anim.currentScale = 1.0f;
				activeRemovedCardAnimations.push_back(anim);

				spawnFloatingText(gridToWorld(target->x, target->y) + glm::vec3(0, 1.0f, 0), "Milled!", ofColor::purple);
			}
		}

		// --- 4. Buff the Caster ---
		currentPlayer.luck++;
		spawnFloatingText(
			gridToWorld(currentPlayer.x, currentPlayer.y),
			"+1 LUCK!",
			ofColor::gold);

		currentPlayer.maxHealth++;
		spawnFloatingText(
			gridToWorld(currentPlayer.x, currentPlayer.y),
			"+1 Max HP!",
			ofColor::limeGreen);

		playedSuccessfully = true;
		break;
	}

		// --- CASE: MAGIC BOLT ---
	case CARD_MAGIC_BOLT: {
		// This card targets through walls, so we don't need a Line of Sight check here.
		// We just need to check if the target tile is within the max possible range.

		// FIX: Range is numDice * diceSides (e.g. 2 * 20 = 40ft)
		float maxRangeFeet = (float)(playedCard.numDice * playedCard.diceSides);

		// Calculate Distance in Feet (Face-to-Face logic)
		// We calculate distance from Caster to Target Tile
		glm::vec2 cPos((float)currentPlayer.x, (float)currentPlayer.y);
		glm::vec2 tPos((float)targetX, (float)targetY);

		// Use standard Euclidean distance * 5 for Magic Bolt (since it flies over walls)
		// Or getFaceToFaceDistance if you want grid logic.
		// Using Euclidean here as it's a projectile over walls.
		float distFeet = glm::distance(cPos, tPos) * 5.0f;

		// Add 3ft buffer for center-to-edge calculation
		if (distFeet > maxRangeFeet + 3.0f) {
			ofLogNotice("Magic Bolt") << "Target is too far.";
			break;
		}

		// Start the range roll
		pendingMagicBoltTargetTile = glm::vec2(targetX, targetY);
		isWaitingForMagicBoltRange = true;

		pendingMagicBoltRangeResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_RANGE, "Magic Bolt: Range Check");

		playedSuccessfully = true;
		break;
	}

		// --- CASE: FLAIL ---
	case CARD_FLAIL: {
		// Roll 1d6. We will add +2 in the update loop.
		pendingFlailRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DAMAGE, "Flail: Swing Damage", currentPlayerIndex);
		isWaitingForFlailDice = true;
		playedSuccessfully = true;
		break;
	}

		// --- CASE: CONSUME LARGE HEALTH POTION ---
	case CARD_CONSUME_LARGE_HEALTH_POTION: {
		// +3 Max HP
		currentPlayer.maxHealth += 3;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+3 Max HP", ofColor::limeGreen);

		// Heal 1 HP
		if (currentPlayer.health < currentPlayer.maxHealth) {
			currentPlayer.health += 1;
			spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.5f, 0), "+1 HP", ofColor::green);
		}

		ofLogNotice("Potion") << "Player " << currentPlayer.playerID << " consumed large potion.";
		playedSuccessfully = true;
		break;
	}

	// --- CASE: LESSER HEAL ---
	case CARD_LESSER_HEAL: {
		// Range Check (Line of Sight)
		glm::vec2 casterTile = { (float)currentPlayer.x, (float)currentPlayer.y };
		glm::vec2 targetTile = { (float)targetX, (float)targetY };

		// Validate LOS (Infinite Range)
		TargetInfo validationResult = isLosTargetValid(casterTile, targetTile, 9999.0f, playedCard.type);

		// Allow SELF targeting for heals (isLosTargetValid usually blocks self)
		bool isSelf = (currentPlayer.x == targetX && currentPlayer.y == targetY);

		if (!isSelf && validationResult.reason != VALID) break;

		// Find Target
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex == -1) break;

		// Friendly Check
		Player * target = getPlayer(targetIndex);
		int casterOwner = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
		int targetOwner = target->isMinion ? target->ownerID : target->playerID;
		if (casterOwner != targetOwner) {
			ofLogNotice("Heal") << "Cannot heal enemies!";
			break;
		}

		// Start Dice Roll
		pendingHealTargetIndex = targetIndex;
		pendingHealRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_HEALING, "Lesser Heal"); // Reusing PURPOSE_HEALING logic
		isWaitingForHealDice = true;
		playedSuccessfully = true;
		break;
	}

	// --- CASE: STANDARD ATTACK (Stab, Cleave, Pierce, Punch) ---
	case CARD_ATTACK_SINGLE_TILE: {
		int px = players[currentPlayerIndex].x;
		int py = players[currentPlayerIndex].y;
		pendingAttackTargetIndices.clear();

		// 1. CLEAVE LOGIC
		if (playedCard.targeting == TARGET_CLEAVE_ADJACENT) {
			glm::vec2 dir = { (float)(targetX - px), (float)(targetY - py) };
			std::vector<Player *> targetsToHit = findCleaveTargets(dir);
			for (auto * targetPlayer : targetsToHit) {
				for (size_t i = 0; i < players.size(); i++) {
					if (&players[i] == targetPlayer) pendingAttackTargetIndices.push_back((int)i);
				}
			}
		}
		// 2. PIERCE LOGIC
		else if (playedCard.targeting == TARGET_LINEAR_PIERCE) {
			glm::vec2 dir = { (float)(targetX - px), (float)(targetY - py) };
			if (std::abs(dir.x) > std::abs(dir.y)) {
				dir.x = (dir.x > 0) ? 1.0f : -1.0f;
				dir.y = 0.0f;
			} else {
				dir.x = 0.0f;
				dir.y = (dir.y > 0) ? 1.0f : -1.0f;
			}

			glm::vec2 pos1 = { px + dir.x, py + dir.y };
			glm::vec2 pos2 = { px + dir.x * 2, py + dir.y * 2 };

			// A. Adjacent
			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].x == (int)pos1.x && players[i].y == (int)pos1.y) {
					pendingAttackTargetIndices.push_back((int)i);
					break;
				}
			}
			// B. Behind (only if not blocked by wall)
			if (!isTileWall((int)pos1.x, (int)pos1.y)) {
				for (size_t i = 0; i < players.size(); i++) {
					if (players[i].x == (int)pos2.x && players[i].y == (int)pos2.y) {
						pendingAttackTargetIndices.push_back((int)i);
						break;
					}
				}
			}
		}
		// 3. STANDARD SINGLE TARGET (e.g., Punch)
		else {
			if (playedCard.targeting == TARGET_ADJACENT_UNIT) {
				int dist = abs(targetX - px) + abs(targetY - py);
				if (dist == 1 && board[targetX][targetY].hasPlayer) {
					for (size_t i = 0; i < players.size(); i++) {
						if (players[i].x == targetX && players[i].y == targetY) {
							pendingAttackTargetIndices.push_back((int)i);
							break;
						}
					}
				}
			} else {
				glm::vec2 casterTile = { (float)px, (float)py };
				glm::vec2 targetTile = { (float)targetX, (float)targetY };
				TargetInfo info = isLosTargetValid(casterTile, targetTile, 5.0f, playedCard.type);

				if (info.reason == VALID && info.isTargetable) {
					for (size_t i = 0; i < players.size(); i++) {
						if (players[i].x == targetX && players[i].y == targetY) {
							pendingAttackTargetIndices.push_back((int)i);
							break;
						}
					}
				}
			}
		}

		if (pendingAttackTargetIndices.empty()) {
			ofLogNotice("Attack") << "No valid targets. Action Cancelled.";
			break;
		}

		// --- EXECUTE DAMAGE ---
		if (playedCard.numDice > 0 && playedCard.diceSides > 0) {
			// DICE PATH (Rock Crush, etc) -> Handled in updateGame loop
			pendingAttackRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_DAMAGE, playedCard.name + ": Damage", currentPlayerIndex);
			pendingAttackCardName = playedCard.name;

			isWaitingForAttackDice = true;
			pendingAttackDamageType = playedCard.damageType;
		} else {
			// INSTANT PATH (Punch, Kick) -> Handled immediately via applyDamage
			int damage = playedCard.value;

			if ((playedCard.name == "Punch") && currentPlayer.flurryOfFistsActive) {
				damage *= 2;
			}

			bool applyPoisonBuff = currentPlayer.nextAttackAddPoison && (playedCard.damageType == DAMAGE_PHYSICAL || playedCard.damageType == DAMAGE_PIERCING);

			if (applyPoisonBuff) {
				currentPlayer.nextAttackAddPoison = false;
				pendingPoisonTargetIndices.clear();
			}

			for (size_t i = 0; i < pendingAttackTargetIndices.size(); i++) {
				int pIndex = pendingAttackTargetIndices[i];
				Player * target = getPlayer(pIndex);
				if (target) {
					int finalDamage = damage;
					if (playedCard.damageType == DAMAGE_PIERCING && i > 0) finalDamage /= 2;

					// applyDamage handles Ghost immunity automatically
					applyDamageTo(*target, finalDamage, playedCard.damageType, currentPlayerIndex);

					if (applyPoisonBuff) {
						// Only apply poison if not phased (damage check handled inside dice logic usually,
						// but here we just check immunity directly for instant attacks)
						if (!target->inGhostForm) {
							pendingPoisonTargetIndices.push_back(pIndex);
							target->isPoisoned = true;
							target->poisonReduction = 0;
							glm::vec3 tPos = gridToWorld(target->x, target->y);
							spawnFloatingText(tPos + glm::vec3(0, 0.5f, 0), "Poisoned!", ofColor::green);
						}
					}
				}
			}

			if (applyPoisonBuff && !pendingPoisonTargetIndices.empty()) {
				pendingPoisonAttackRollResult = startDiceRoll(1, 6, PURPOSE_DAMAGE, "Poison Damage");
				isWaitingForPoisonAttackDice = true;
			}
		}
		playedSuccessfully = true;
		break;
	}

	// --- CASE: SHOCK ---
	case CARD_SHOCK: {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetX && players[i].y == targetY) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex != -1) {
			Player * target = getPlayer(targetIndex);
			applyDamageTo(*target, playedCard.value, DAMAGE_ELECTRIC, currentPlayerIndex);
			if (currentPlayer.shocksPlayedThisTurn > 0 && !target->isParalyzed) {
				target->isParalyzed = true;
				target->paralysisHeadsCount = 0;
			}
		}
		currentPlayer.nextTurnAPBonus += 2;
		currentPlayer.shocksPlayedThisTurn++;
		playedSuccessfully = true;
		break;
	}

		// --- CASE: SPARK OF GENIUS ---
	case CARD_SPARK_OF_GENIUS: {
		// Roll 1d4 (defined in JSON)
		pendingSparkOfGeniusRollResult = startDiceRoll(playedCard.numDice, playedCard.diceSides, PURPOSE_SPARK_OF_GENIUS_DRAW, "Spark of Genius: Draw Cards");
		isWaitingForSparkOfGeniusDice = true;
		playedSuccessfully = true;
		break;
	}

	case CARD_GAIN_AP:
		currentAP += playedCard.value;
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y), "+" + ofToString(playedCard.value) + " AP", ofColor::yellow);
		playedSuccessfully = true;
		break;

	case CARD_GAIN_BLOCK: {
		int blockValue = playedCard.value;
		// Double block for Hand Block if Flurry of Fists is active
		if (playedCard.name == "Hand Block" && currentPlayer.flurryOfFistsActive) {
			blockValue *= 2;
		}
		players[currentPlayerIndex].block += blockValue;
		spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y), "+" + ofToString(blockValue) + " Block", ofColor::gray);
		playedSuccessfully = true;
		break;
	}

	case CARD_GAIN_WARD:
		players[currentPlayerIndex].ward += playedCard.value;
		spawnFloatingText(gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y), "+" + ofToString(playedCard.value) + " Ward", ofColor::black);
		playedSuccessfully = true;
		break;

	default:
		break;
	}

	// --- 3. COMMON CLEANUP ---
	if (playedSuccessfully) {
		currentAP -= costToPay;

		// --- STRENGTHEN ELEMENTS TRIGGER ---
		// Check if buff is active AND card deals Fire or Electric damage
		if (currentPlayer.strengthenElementsTurnsRemaining > 0) {
			if (playedCard.damageType == DAMAGE_FIRE || playedCard.damageType == DAMAGE_ELECTRIC) {

				// Don't trigger on the Strengthen card itself (safety check, though types differ)
				if (playedCard.type != CARD_STRENGTHEN_ELEMENTS) {

					// Create a copy
					Card copy = playedCard;

					// Add to Deck
					currentPlayer.deck.push_back(copy);

					// Shuffle the deck to integrate the new card (authoritative)
					shuffleGameVector(currentPlayer.deck, currentPlayerIndex);

					spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 0.5, 0), "Element Copied!", ofColor::cyan);
					ofLogNotice("Game") << "Strengthen Elements triggered: Copied " << playedCard.name << " to deck.";
				}
			}
		}
		// -----------------------------------

		// --- TORTOISE FORM: Deal 3 damage to adjacent after block/heal/ward ---
		// Note: CARD_DISPEL and CARD_WISDOM_BOON trigger Shell Spike after their menu choice is made
		if (currentPlayer.inTortoiseForm) {
			bool isDefensiveCard = (playedCard.type == CARD_GAIN_BLOCK) || (playedCard.type == CARD_HEAL) || (playedCard.type == CARD_GAIN_WARD) || (playedCard.type == CARD_DARK_SHIELD) || (playedCard.type == CARD_FORTIFY);

			if (isDefensiveCard) {
				// Check if there are any adjacent units (ANY unit, including allies)
				bool hasAdjacentUnit = false;

				for (const auto & p : players) {
					if (p.x < 0) continue; // Dead
					if (&p == &currentPlayer) continue; // Self

					// Check adjacency (orthogonal only — no diagonals)
					int dx = abs(p.x - currentPlayer.x);
					int dy = abs(p.y - currentPlayer.y);
					if ((dx + dy) == 1) {
						hasAdjacentUnit = true;
						break;
					}
				}

				if (hasAdjacentUnit) {
					// Enter tortoise damage targeting mode
					isTargetingTortoiseDamage = true;
					calculateTargetHighlights(); // Show green highlights on valid targets
					spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 1.0f, 0),
						"Shell Spike!", ofColor::darkGreen);
					ofLogNotice("Tortoise Form") << "Triggered damage - choose adjacent target.";
				}
			}
		}
		// -----------------------------------

		currentPlayer.playedCardsPile.push_back(playedCard);

		if (currentPlayer.isReplicatePending && playedCard.type != CARD_REPLICATE) {
			Card duplicateCard = playedCard;
			currentPlayer.playedCardsPile.push_back(duplicateCard);
			currentPlayer.isReplicatePending = false;
		}

		currentPlayer.hand.erase(currentPlayer.hand.begin() + cardIndex);
		createCardDisplay(playedCard, currentPlayerIndex);
		invalidateTargetCache();
		currentPlayer.cardsPlayedThisTurn.push_back(playedCard.type);
		return CARD_PLAYED_IMMEDIATELY;
	}

	return CARD_NOT_PLAYABLE;
}
//--------------------------------------------------------------
glm::vec2 ofApp::getCardDisplayUIPosition(int playerIndex) {
	// Return the UI position where a card display should appear for the given player
	// Local player's cards appear at bottom center
	// Opponent's cards appear at top center

	float screenCenterX = ofGetWidth() / 2.0f;
	float screenCenterY = ofGetHeight() / 2.0f;

	if (isMultiplayer) {
		// In multiplayer, determine which player is "us" and which is "them"
		// Account for minions: get the owner ID if it's a minion
		if (playerIndex >= 0 && playerIndex < (int)players.size()) {
			const Player & p = players[playerIndex];
			int ownerID = p.isMinion ? p.ownerID : p.playerID;
			if (ownerID == myLocalPlayerID) {
				// Local player or local player's minion: bottom center
				return glm::vec2(screenCenterX, ofGetHeight() - 120.0f);
			} else {
				// Opponent or opponent's minion: top center
				return glm::vec2(screenCenterX, 120.0f);
			}
		}
		// Fallback: opponent side
		return glm::vec2(screenCenterX, 120.0f);
	} else {
		// In singleplayer, show at bottom center for both
		return glm::vec2(screenCenterX, ofGetHeight() - 120.0f);
	}
}
//--------------------------------------------------------------
void ofApp::createCardDisplay(const Card & card, int playerIndex) {
	PlayedCardDisplay disp;
	disp.card = card;
	disp.startTime = ofGetElapsedTimef();
	disp.startPos = getCardDisplayUIPosition(playerIndex);
	disp.currentPos = disp.startPos;
	disp.currentScale = 2.6f;
	disp.currentAlpha = 255.0f;
	activeCardDisplays.push_back(disp);
}
//--------------------------------------------------------------
ofVec2f ofApp::mouseToBoard(int x, int y) {
	// Use the appropriate camera for raycasting based on which player we are
	ofCamera & activeCam = getActiveCamera();

	glm::vec3 planePoint(0, 0, 0);
	glm::vec3 planeNormal(0, 1, 0);
	glm::vec3 rayOrigin = activeCam.screenToWorld(glm::vec3(x, y, 0));
	glm::vec3 rayDirection = activeCam.screenToWorld(glm::vec3(x, y, 1)) - rayOrigin;
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
	// 1. Reset Board
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			board[x][y].isTargetPreview = false; // Red
			board[x][y].isTargetable = false; // Green
		}
	}

	// --- WOLF PLACEMENT HIGHLIGHTING ---
	if (isPlacingWolves && !isWaitingForWolfCoin) {
		std::vector<glm::vec2> dirs = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };
		for (auto & dir : dirs) {
			int nx = wolfPlacementSourceX + (int)dir.x;
			int ny = wolfPlacementSourceY + (int)dir.y;
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
				if (!board[nx][ny].hasWall && !board[nx][ny].hasPlayer) {
					board[nx][ny].isTargetable = true;
					// Also make it green outline for visibility
					board[nx][ny].isTargetPreview = true;
				}
			}
		}
		return;
	}

	// --- KOBOLD PLACEMENT HIGHLIGHTING ---
	if (isPlacingKobolds && !isWaitingForKoboldDice) {
		std::vector<glm::vec2> dirs = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };
		for (auto & dir : dirs) {
			int nx = koboldPlacementSourceX + (int)dir.x;
			int ny = koboldPlacementSourceY + (int)dir.y;
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
				if (!board[nx][ny].hasWall && !board[nx][ny].hasPlayer) {
					board[nx][ny].isTargetable = true;
					board[nx][ny].isTargetPreview = true;
				}
			}
		}
		return;
	}

	// --- TORTOISE DAMAGE TARGETING HIGHLIGHTING ---
	if (isTargetingTortoiseDamage) {
		Player & caster = players[currentPlayerIndex];
		// Highlight all adjacent tiles with units
		for (int dx = -1; dx <= 1; dx++) {
			for (int dy = -1; dy <= 1; dy++) {
				if (dx == 0 && dy == 0) continue; // Skip self
				int nx = caster.x + dx;
				int ny = caster.y + dy;
				if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
					// Check if there's a unit here (any unit, including allies)
					for (const auto & p : players) {
						if (p.x == nx && p.y == ny) {
							board[nx][ny].isTargetable = true; // Green highlight
							break;
						}
					}
				}
			}
		}
		return;
	}

	if (players.empty() || currentPlayerIndex < 0) return;
	Player & currentPlayer = players[currentPlayerIndex];

	// Determine which card is active
	int activeCardIndex = (selectedCardIndex != -1) ? selectedCardIndex : ((draggedCardIndex != -1) ? draggedCardIndex : cardToCalculate);

	// OVERRIDE index if we are in a specific targeting mode
	if (isTargetingMagicBolt) activeCardIndex = magicBoltCardIndex;
	if (isTargetingTeleport) activeCardIndex = pendingTeleportCardIndex;
	if (isTargetingAmnesia) activeCardIndex = pendingAmnesiaCardIndex;
	if (isTargetingDoubleHanded) activeCardIndex = pendingDoubleHandedCardIndex;
	if (isTargetingHellhound) activeCardIndex = hellhoundCardIndex;
	if (isTargetingBurst) activeCardIndex = pendingBurstCardIndex;

	// Safety Check
	if (activeCardIndex < 0 || activeCardIndex >= (int)currentPlayer.hand.size()) return;

	Card & card = currentPlayer.hand[activeCardIndex];
	int px = currentPlayer.x;
	int py = currentPlayer.y;
	glm::vec2 casterPos(px, py);

	// Check AP (Targeting modes imply AP check passed already)
	bool inTargetingMode = isTargetingAmnesia || isTargetingDoubleHanded || isTargetingTeleport || isTargetingMagicBolt || isTargetingHellhound || isTargetingChainLightning || isTargetingBurst;

	bool hasEnoughAP = inTargetingMode || (currentAP >= card.cost);

	// --- MOUSE HOVER CALCULATION ---
	glm::vec2 mouseTile = mouseToBoard(ofGetMouseX(), ofGetMouseY());
	glm::vec2 aimDir = { 0, 0 };
	bool isAimingOnBoard = false;

	if (mouseTile.x >= 0 && mouseTile.x < BOARD_WIDTH && mouseTile.y >= 0 && mouseTile.y < BOARD_HEIGHT) {
		if (mouseTile != casterPos) {
			isAimingOnBoard = true;
			glm::vec2 rawDir = mouseTile - casterPos;
			if (std::abs(rawDir.x) > std::abs(rawDir.y)) {
				aimDir = { (rawDir.x > 0 ? 1.0f : -1.0f), 0.0f };
			} else {
				aimDir = { 0.0f, (rawDir.y > 0 ? 1.0f : -1.0f) };
			}
		}
	}

	// If a card is selected or hovered (and not dragging), force stable previews
	// so highlights don't change as the mouse moves.
	if (draggedCardIndex == -1 && (selectedCardIndex != -1 || hoveredCardIndex != -1)) {
		isAimingOnBoard = false;
	}

	// Small helper: some edge cases can leave `board[x][y].hasWall` false
	// while the level mesh still contains wall geometry. Provide a
	// cheap fallback check to detect wall geometry at a grid cell.
	auto meshHasWallAt = [&](int tx, int ty) {
		if (tx < 0 || tx >= BOARD_WIDTH || ty < 0 || ty >= BOARD_HEIGHT) return false;
		glm::vec3 center = gridToWorld(tx, ty);
		float thresh = TILE_SIZE * 0.4f; // area to consider
		for (const auto & v : levelMesh.getVertices()) {
			// Compare XZ distance only (ignore Y vertex height)
			float dx = v.x - center.x;
			float dz = v.z - center.z;
			if ((dx * dx + dz * dz) <= (thresh * thresh)) return true;
		}
		return false;
	};

	// --- ITERATE BOARD ---
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			bool isPreview = false;
			bool isValidTarget = false;
			glm::vec2 targetPos(x, y);

			// 1. Calculate Distance in Feet
			// Face-to-Face Units * 5 = Feet
			// Example: 4 tiles away -> 3 gaps -> 3.0 units -> 15 ft.
			float distFeet = getFaceToFaceDistance(casterPos, targetPos) * 5.0f;

			switch (card.targeting) {

			// --- FLAIL HIGHLIGHTING ---
			case TARGET_SELF: {
				// Special handling for Psionic Wave which uses TARGET_SELF but affects area
				if (card.type == CARD_PSIONIC_WAVE) {
					// 1. Calculate Max Possible Radius
					// Max roll on 2d20 is 40. +3 base = 43 feet.
					// Convert to grid units: 43 / 5 = 8.6 tiles radius.
					float maxRadiusFeet = 43.0f;

					// 2. Calculate Distance (Euclidean, ignores walls)
					// We use center-to-center distance for the circular wave check
					float distFeet = glm::distance(casterPos, targetPos) * 5.0f;

					// 3. Highlight Logic
					if (distFeet <= maxRadiusFeet + 0.1f) {
						isPreview = true; // Red Square (Potential Range)

						// If a unit is here (and not self), it's a valid target
						if (board[x][y].hasPlayer && (x != px || y != py)) {
							isValidTarget = true; // Green Outline (Potential Hit)
						}
					}

					// Ensure caster tile handles clicks correctly
					if (x == px && y == py) {
						if (hasEnoughAP) board[x][y].isTargetable = true;
						continue;
					}
				} else if (card.type == CARD_FLAIL) {
					int dx = x - px;
					int dy = y - py;

					// 1. Check Neighbors
					if (std::max(abs(dx), abs(dy)) == 1) {
						bool blocked = false;

						if (board[x][y].hasWall) blocked = true;

						// Diagonal Pinch Check
						if (!blocked && abs(dx) == 1 && abs(dy) == 1) {
							if (isTileWall(px + dx, py) && isTileWall(px, py + dy)) {
								blocked = true;
							}
						}

						if (!blocked) {
							isPreview = true; // Red Square on ground
							if (board[x][y].hasPlayer) {
								isValidTarget = true; // Green Outline for enemies
							}
						}
					}

					// 2. Handle Caster (Self)
					// We mark isTargetable = true so the click registers (to cast the spell),
					// BUT we explicitly do NOT set isValidTarget/isPreview here so it doesn't glow green/red.
					if (x == px && y == py) {
						if (hasEnoughAP) board[x][y].isTargetable = true;
						// Note: We deliberately skip setting 'isValidTarget = true'
						// because that triggers the Green drawing logic at the bottom of the loop.
						// Setting board[x][y].isTargetable directly allows the click logic to work
						// without the visual feedback.
						continue; // Skip the bottom drawing logic for this specific tile
					}
				}
				// Default TARGET_SELF behavior for other cards (Buffs, etc)
				else {
					if (x == px && y == py) {
						isPreview = true;
						isValidTarget = true;
					}
				}
				break;
			}

			case TARGET_CLEAVE_ADJACENT: {
				// List of directions to render previews for
				std::vector<glm::vec2> dirsToCheck;

				if (isAimingOnBoard) {
					dirsToCheck.push_back(aimDir);
				} else {
					dirsToCheck.push_back({ 0, 1 });
					dirsToCheck.push_back({ 0, -1 });
					dirsToCheck.push_back({ 1, 0 });
					dirsToCheck.push_back({ -1, 0 });
				}

				for (auto & dir : dirsToCheck) {
					std::vector<glm::vec2> arcTiles;
					glm::vec2 center = casterPos + dir;
					arcTiles.push_back(center);

					if (dir.x != 0) {
						arcTiles.push_back({ center.x, center.y - 1 });
						arcTiles.push_back({ center.x, center.y + 1 });
					} else {
						arcTiles.push_back({ center.x - 1, center.y });
						arcTiles.push_back({ center.x + 1, center.y });
					}

					for (auto & tile : arcTiles) {
						if (tile.x == x && tile.y == y) {
							isPreview = true;
							if (isAimingOnBoard && dir == aimDir) {
								if (board[x][y].hasPlayer && !board[x][y].hasWall) {
									bool blocked = false;
									if (px != x && py != y) {
										if (isTileWall(px, y) && isTileWall(x, py)) blocked = true;
									}
									if (!blocked) isValidTarget = true;
								}
							} else {
								// When not aiming, show green outlines for any valid adjacent unit
								if (board[x][y].hasPlayer && !board[x][y].hasWall) {
									bool blocked = false;
									if (px != x && py != y) {
										if (isTileWall(px, y) && isTileWall(x, py)) blocked = true;
									}
									if (!blocked) isValidTarget = true;
								}
							}
						}
					}
				}
				break;
			}

			case TARGET_ADJACENT_OR_SELF_UNIT: {
				int distGrid = abs(x - px) + abs(y - py);
				if (isTargetingAmnesia) {
					if (distGrid == 1 && board[x][y].hasPlayer && !board[x][y].hasWall) {
						isPreview = true;
						isValidTarget = true;
					}
				} else {
					if (distGrid == 0 || distGrid == 1) {
						if (board[x][y].hasPlayer && !board[x][y].hasWall) {
							isPreview = true;
							isValidTarget = true;
						}
					}
				}
				break;
			}

			case TARGET_ADJACENT_UNIT:
			case TARGET_ADJACENT_UNIT_OR_WALL:
			case TARGET_EMPTY_ADJACENT: {
				int distGrid = abs(x - px) + abs(y - py);
				if (distGrid == 1) {
					isPreview = true;

					// 1. Special Case: Summon Magic Wall
					// Can target empty tile OR existing wall to transform it
					if (card.name == "Summon Magic Wall") {
						if ((!board[x][y].hasWall && !board[x][y].hasPlayer) || board[x][y].hasWall) {
							isValidTarget = true;
						}
					}
					// 2. Wall Destruction / Fortification
					else if (card.type == CARD_ROCK_CRUSH || card.type == CARD_FORTIFY || card.type == CARD_DEMOLITION) {
						if (board[x][y].hasWall || board[x][y].hasPlayer) isValidTarget = true;
					}
					// 3. Standard Summoning / Creation (Must be empty)
					// ADD CARD_SUMMON_KOBOLD_KING TO THIS LIST:
					else if (card.type == CARD_CALL_FOR_WOLVES || card.type == CARD_SUMMON_GOLEM || card.type == CARD_RAISE_DEAD || card.type == CARD_CREATE_WALL || card.type == CARD_SUMMON_HELLHOUND || card.type == CARD_SUMMON_DEMON || card.type == CARD_SUMMON_KOBOLD_KING || card.type == CARD_SUMMON_ASSISTANT || card.type == CARD_SUMMON_FAERIE) // <--- Add this
					{
						if (!board[x][y].hasWall && !board[x][y].hasPlayer) isValidTarget = true;
					}
					// 4. Default Attack (Must have unit)
					else {
						if (board[x][y].hasPlayer && !board[x][y].hasWall) isValidTarget = true;
					}
				}
				break;
			}

			case TARGET_LINEAR_PIERCE: {
				glm::vec2 dirs[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
				for (auto & d : dirs) {
					glm::vec2 t1 = casterPos + d;
					glm::vec2 t2 = casterPos + (d * 2.0f);
					if ((x == (int)t1.x && y == (int)t1.y) || (x == (int)t2.x && y == (int)t2.y)) {
						isPreview = true;
						if (board[x][y].hasPlayer && !board[x][y].hasWall) {
							if (x == (int)t2.x && y == (int)t2.y) {
								if (!isTileWall((int)t1.x, (int)t1.y)) isValidTarget = true;
							} else {
								isValidTarget = true;
							}
						}
					}
				}
				break;
			}

			// --- TELEPORT LOGIC ---
			case TARGET_EMPTY_TILE: {
				float maxRangeFeet;
				if (isTargetingTeleport)
					maxRangeFeet = (float)pendingTeleportRollResult;
				else
					maxRangeFeet = (float)(card.numDice * card.diceSides);

				if (distFeet <= maxRangeFeet + 0.1f) {
					isPreview = true;

					bool isWall = board[x][y].hasWall;
					bool isOccupied = board[x][y].hasPlayer;

					if (!isOccupied) {
						if (!isWall) {
							isValidTarget = true;
						}
						// GHOST LOGIC: Can teleport into wall IF they have AP left after casting
						// Teleport cost is usually 5. If currentAP > 5, they have 1 left.
						else if (currentPlayer.inGhostForm) {
							// Check remaining AP (currentAP - cardCost)
							// Card cost is already deducted? No, playCard only deducts if played successfully.
							// But for Teleport, we ALREADY deducted AP in playCard before entering targeting mode.
							// So currentAP is the *remaining* AP.
							if (currentAP >= 1) {
								isValidTarget = true;
							}
						}
					}
				}
				break;
			}

			case TARGET_ADJACENT_WALL: {
				int dist = abs(x - px) + abs(y - py);

				// Red Highlight (Preview) for any adjacent tile to show range
				if (dist == 1) {
					isPreview = true;

					// Green Highlight (Valid Target) ONLY if it is a wall
					if (board[x][y].hasWall || meshHasWallAt(x, y)) {
						isValidTarget = true;
					}
				}
				break;
			}

				// --- FIXED SPELL RANGES (Magic Blast, Fireball, Jolt, Death, Heal) ---
			case TARGET_LINE_OF_SIGHT_TILE: {
				float maxRangeFeet;

				// --- Determine Max Range based on current card/state ---
				if (card.type == CARD_MAGIC_BOLT && isTargetingMagicBolt && pendingMagicBoltRangeResult > 0) {
					maxRangeFeet = (float)pendingMagicBoltRangeResult;
				}
				// Infinite Range Cards
				else if (card.type == CARD_HEAL || card.type == CARD_DEATH || card.type == CARD_LESSER_HEAL || card.type == CARD_BURST_OF_LIGHT) {
					maxRangeFeet = 9999.0f;
				} else {
					// Special-case Shoot Arrow: fixed max range = 2 * 20 = 40 ft
					if (card.type == CARD_SHOOT_ARROW) {
						maxRangeFeet = 2.0f * 20.0f;
					} else {
						// Default: Max potential roll (e.g. 1d20 -> 20ft)
						maxRangeFeet = (float)(card.numDice * card.diceSides);
					}
				}

				TargetInfo info = isLosTargetValid(casterPos, targetPos, maxRangeFeet, card.type);

				// --- Determine Red Preview ---
				// Burst of Light: line-of-sight targeting with NO range cap, but do not
				// preview wall tiles themselves. Show preview only if there's LOS and
				// the tile is not a wall.
				if (info.reason != INVALID_NO_LOS && info.reason != INVALID_OUT_OF_RANGE && !board[x][y].hasWall) {
					isPreview = true;
				}

				// --- Determine Green Outline (is it a valid final target?) ---
				bool canBeClicked = false;
				bool isOccupied = board[x][y].hasPlayer;

				// --- CHAIN LIGHTNING LOGIC ---
				if (card.type == CARD_CHAIN_LIGHTNING) {
					if (isPreview) {
						if (isOccupied) {
							canBeClicked = true;
						} else {
							// Check 8 neighbors
							for (int dx = -1; dx <= 1; dx++) {
								for (int dy = -1; dy <= 1; dy++) {
									if (dx == 0 && dy == 0) continue;
									int nx = x + dx;
									int ny = y + dy;
									if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && board[nx][ny].hasPlayer) {
										bool blocked = false;
										if (abs(dx) == 1 && abs(dy) == 1) { // Diagonal check
											if (isTileWall(x + dx, y) && isTileWall(x, y + dy)) blocked = true;
										}
										if (!blocked) {
											canBeClicked = true;
											break;
										}
									}
								}
								if (canBeClicked) break;
							}
						}
					}
				}
				// --- HEAL / LESSER HEAL / DEATH LOGIC ---
				else if (card.type == CARD_HEAL || card.type == CARD_LESSER_HEAL || card.type == CARD_DEATH) {
					// Must target a unit (Self included for Heal)
					// Only allow clicking if the tile is also a red preview (LOS & not a wall)
					if (isPreview && isOccupied) {
						canBeClicked = true;
					}
				}
				// --- MAGIC BOLT LOGIC ---
				else if (card.type == CARD_MAGIC_BOLT) {
					// Require that the tile is a preview (LOS & not wall) before allowing click
					if (isPreview) {
						if (isOccupied)
							canBeClicked = true;
						else if (info.isTargetable)
							canBeClicked = true;
					}
				}
				// --- DEFAULT LOGIC ---
				else {
					// Default: require preview (LOS & not wall) AND that the los check marks it targetable
					if (isPreview && info.isTargetable) {
						canBeClicked = true;
					}
				}

				if (canBeClicked) {
					isValidTarget = true;
					isPreview = true;
				}

				// Shoot Arrow: only allow unit tiles (occupied) to be valid click targets
				if (card.type == CARD_SHOOT_ARROW) {
					if (!(isPreview && isOccupied)) {
						// clear clickability if not an occupied preview tile
						isValidTarget = false;
					}
				}
				break;
			}

			default:
				break;
			}

			if (isPreview) board[x][y].isTargetPreview = true; // Red
			if (isValidTarget && hasEnoughAP) board[x][y].isTargetable = true; // Green
		}
	}
}
//--------------------------------------------------------------
void ofApp::spawnFloatingText(glm::vec3 pos, std::string text, ofColor color) {
	FloatingText ft;
	ft.text = text;
	// Start slightly above the unit
	ft.worldPos = pos + glm::vec3(0, 1.5f, 0);
	// Random slight drift left/right, consistent drift up
	std::uniform_real_distribution<float> driftDist(-1.0f, 1.0f);
	ft.velocity = glm::vec3(driftDist(visualRNG), 2.0f, driftDist(visualRNG));
	ft.startTime = ofGetElapsedTimef();
	ft.color = color;
	activeFloatingTexts.push_back(ft);
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
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			board[x][y].isHighlighted = false;
			board[x][y].isTargetPreview = false; // <--- ADD THIS
		}
	hoverPath.clear();
}

//--------------------------------------------------------------
std::vector<glm::vec2> ofApp::findShortestPath(glm::vec2 start, glm::vec2 end) {
	std::vector<glm::vec2> path;

	// Reset
	for (int i = 0; i < BOARD_WIDTH; i++)
		for (int j = 0; j < BOARD_HEIGHT; j++) {
			board[i][j].visited = false;
			board[i][j].parent = { -1, -1 };
		}

	std::queue<glm::vec2> q;
	q.push(start);
	board[(int)start.x][(int)start.y].visited = true;

	// Ghost Check
	Player & p = players[currentPlayerIndex];
	bool isGhost = p.inGhostForm;

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
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && !board[nx][ny].visited) {

				// --- WALL CHECK ---
				bool isBlocked = false;
				if (board[nx][ny].hasPlayer) isBlocked = true; // Still blocked by other units
				if (board[nx][ny].hasWall && !isGhost) isBlocked = true; // Blocked by wall if not Ghost

				if (!isBlocked) {
					board[nx][ny].visited = true;
					board[nx][ny].parent = current;
					q.push(neighbor);
				}
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
std::vector<glm::vec2> ofApp::findShortestPathForPlayer(int playerIndex, glm::vec2 start, glm::vec2 end) {
	std::vector<glm::vec2> path;

	// Reset
	for (int i = 0; i < BOARD_WIDTH; i++)
		for (int j = 0; j < BOARD_HEIGHT; j++) {
			board[i][j].visited = false;
			board[i][j].parent = { -1, -1 };
		}

	std::queue<glm::vec2> q;
	q.push(start);
	board[(int)start.x][(int)start.y].visited = true;

	// Ghost Check (use specified player if valid, otherwise fall back to current player)
	int safeIndex = playerIndex;
	if (safeIndex < 0 || safeIndex >= (int)players.size()) safeIndex = currentPlayerIndex;
	bool isGhost = false;
	if (safeIndex >= 0 && safeIndex < (int)players.size()) {
		isGhost = players[safeIndex].inGhostForm;
	}

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
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && !board[nx][ny].visited) {

				// --- WALL CHECK ---
				bool isBlocked = false;
				if (board[nx][ny].hasPlayer) isBlocked = true; // Still blocked by other units
				if (board[nx][ny].hasWall && !isGhost) isBlocked = true; // Blocked by wall if not Ghost

				if (!isBlocked) {
					board[nx][ny].visited = true;
					board[nx][ny].parent = current;
					q.push(neighbor);
				}
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
// Get player display name (Steam name in multiplayer, "Player 1"/"Player 2" in singleplayer)
std::string ofApp::getPlayerSteamName(int playerIndex) {
	if (playerIndex < 0 || playerIndex >= (int)players.size()) return "Unknown";
	int playerID = players[playerIndex].playerID;

	if (isMultiplayer) {
		return (playerID == 0) ? player0SteamName : player1SteamName;
	} else {
		return (playerID == 0) ? "Player 1" : "Player 2";
	}
}

//--------------------------------------------------------------
const Card * ofApp::findCardByName(const std::string & name) const {
	for (const auto & c : allCards) {
		if (c.name == name) return &c;
	}
	for (const auto & c : class1Cards) {
		if (c.name == name) return &c;
	}
	for (const auto & c : class2Cards) {
		if (c.name == name) return &c;
	}
	for (const auto & c : class3Cards) {
		if (c.name == name) return &c;
	}
	return nullptr;
}

//--------------------------------------------------------------
bool ofApp::isMyTurn() const {
	if (currentPlayerIndex < 0 || players.empty()) return false;
	int pid = players[currentPlayerIndex].playerID;
	int oid = players[currentPlayerIndex].ownerID;
	return (pid == myLocalPlayerID || oid == myLocalPlayerID);
}

//--------------------------------------------------------------
bool ofApp::isCurrentPlayerLocal() const {
	if (currentPlayerIndex < 0 || players.empty()) return false;
	return players[currentPlayerIndex].playerID == myLocalPlayerID;
}

//--------------------------------------------------------------
ofCamera & ofApp::getActiveCamera() {
	// During drafting phase, keep the camera that was being used before drafting
	if (currentState == STATE_DRAFTING) {
		// Client should always use camera 2 during drafting to avoid snap
		if (isMultiplayer && myLocalPlayerID == 1) {
			return cam2;
		}
		return cam;
	}
	// In multiplayer, Player 1 (client) uses cam2 positioned on opposite side
	// Player 0 (host) uses cam (default position)
	if (isMultiplayer && myLocalPlayerID == 1) {
		return cam2;
	}
	return cam;
}

void ofApp::addGameLog(const std::string & logText) {
	GameLogEntry entry;
	entry.text = logText;
	entry.timestamp = ofGetElapsedTimef();
	gameLog.push_back(entry);
	if (gameLog.size() > maxLogEntries) {
		gameLog.erase(gameLog.begin());
	}
	// Don't show chat window for log entries - only for chat messages
}

//--------------------------------------------------------------
std::string ofApp::buildSnapshotString() {
	std::ostringstream ss;
	ss << "V\t1\n";
	ss << "STATE\t" << (int)currentState
	   << "\t" << currentPlayerIndex
	   << "\t" << globalTurnCounter
	   << "\t" << (isInGameDraft ? 1 : 0)
	   << "\t" << draftStage
	   << "\t" << draftPlayerIndex
	   << "\t" << draftPicksRemaining
	   << "\t" << currentDraftClassTier
	   << "\t" << (hasDrawnCardsThisTurn ? 1 : 0)
	   << "\t" << (opponentHasDrawnCardsThisTurn ? 1 : 0)
	   << "\t" << currentAP
	   << "\t" << lastAPDiceNum
	   << "\t" << lastAPDiceSides
	   << "\t" << (hasUnlimitedAP ? 1 : 0)
	   << "\n";

	ss << "QUEUE\t" << pendingDraftQueue.size();
	for (int v : pendingDraftQueue)
		ss << "\t" << v;
	ss << "\n";

	std::string walls;
	std::string magicWalls;
	walls.reserve(BOARD_WIDTH * BOARD_HEIGHT);
	magicWalls.reserve(BOARD_WIDTH * BOARD_HEIGHT);
	for (int y = 0; y < BOARD_HEIGHT; ++y) {
		for (int x = 0; x < BOARD_WIDTH; ++x) {
			walls += board[x][y].hasWall ? '1' : '0';
			magicWalls += board[x][y].isMagicWall ? '1' : '0';
		}
	}
	ss << "BOARD\t" << walls << "\t" << magicWalls << "\n";

	ss << "DRAFTOPTS\t";
	for (size_t i = 0; i < draftOptions.size(); ++i) {
		if (i > 0) ss << ',';
		ss << escapeField(draftOptions[i].name);
	}
	ss << "\tSEL\t";
	for (size_t i = 0; i < selectedDraftIndices.size(); ++i) {
		if (i > 0) ss << ',';
		ss << selectedDraftIndices[i];
	}
	ss << "\n";

	ss << "PLAYERS\t" << players.size() << "\n";
	for (const auto & p : players) {
		ss << "P\t"
		   << p.playerID << "\t" << p.x << "\t" << p.y << "\t"
		   << p.health << "\t" << p.maxHealth << "\t" << p.block << "\t" << p.ward << "\t"
		   << p.fortification << "\t" << p.barrier << "\t" << p.holyBlock << "\t" << p.luck << "\t"
		   << p.bonusTurns << "\t" << p.facingAngle << "\t"
		   << (p.onFire ? 1 : 0) << "\t" << (p.hasRegeneration ? 1 : 0) << "\t"
		   << p.nextTurnAPBonus << "\t" << p.shocksPlayedThisTurn << "\t"
		   << (p.flurryOfFistsActive ? 1 : 0) << "\t" << (p.isParalyzed ? 1 : 0) << "\t"
		   << p.paralysisHeadsCount << "\t" << (p.isPoisoned ? 1 : 0) << "\t" << p.poisonReduction << "\t"
		   << (p.nextAttackAddPoison ? 1 : 0) << "\t" << (p.nextTurnD10AP ? 1 : 0) << "\t"
		   << (p.nextTurnExtraDraw ? 1 : 0) << "\t" << (p.isReplicatePending ? 1 : 0) << "\t"
		   << (p.nextTurnBonusDiceFromMinions ? 1 : 0) << "\t" << p.strengthenElementsTurnsRemaining << "\t"
		   << p.sleepTurnsRemaining << "\t" << p.summonedOnTurnCycle << "\t" << p.summonOrder << "\t"
		   << (p.isMinion ? 1 : 0) << "\t" << (p.isSkeleton ? 1 : 0) << "\t" << (p.isGolem ? 1 : 0) << "\t"
		   << (p.isHellhound ? 1 : 0) << "\t" << (p.isWolf ? 1 : 0) << "\t" << (p.isKobold ? 1 : 0) << "\t"
		   << (p.isDemon ? 1 : 0) << "\t" << (p.isWallUnit ? 1 : 0) << "\t" << (p.isMagicWallUnit ? 1 : 0) << "\t"
		   << (p.isKoboldKing ? 1 : 0) << "\t" << (p.isFaerie ? 1 : 0) << "\t" << (p.isAssistant ? 1 : 0) << "\t"
		   << p.directSummonerID << "\t" << (p.assistantRerollUsedThisTurn ? 1 : 0) << "\t" << p.freeKickTurns << "\t"
		   << (p.inTortoiseForm ? 1 : 0) << "\t" << p.tortoiseDamageTaken << "\t" << (p.pendingTortoiseDamage ? 1 : 0) << "\t"
		   << p.pendingTortoiseDamageValue << "\t" << p.ownerID << "\t" << (p.inGhostForm ? 1 : 0) << "\t"
		   << p.ghostDamageTaken << "\t" << escapeField(p.originalModelType) << "\t";

		auto encodeCards = [&](const std::vector<Card> & cards) {
			std::string out;
			for (size_t i = 0; i < cards.size(); ++i) {
				if (i > 0) out += ',';
				out += escapeField(cards[i].name);
			}
			return out;
		};

		ss << "DECK\t" << encodeCards(p.deck)
		   << "\tHAND\t" << encodeCards(p.hand)
		   << "\tDISCARD\t" << encodeCards(p.discardPile)
		   << "\tPLAYED\t" << encodeCards(p.playedCardsPile)
		   << "\tPLAYEDTYPES\t";
		for (size_t i = 0; i < p.cardsPlayedThisTurn.size(); ++i) {
			if (i > 0) ss << ',';
			ss << (int)p.cardsPlayedThisTurn[i];
		}
		ss << "\n";
	}

	// Save graveyard (dead units' decks)
	ss << "GRAVEYARD\t" << graveyard.size() << "\n";
	for (const auto & grave : graveyard) {
		auto encodeCards = [&](const std::vector<Card> & cards) {
			std::string out;
			for (size_t i = 0; i < cards.size(); ++i) {
				if (i > 0) out += ',';
				out += escapeField(cards[i].name);
			}
			return out;
		};
		ss << "GRAVE\t" << grave.x << "\t" << grave.y << "\t" << grave.turnDied << "\t" << encodeCards(grave.deck) << "\n";
	}

	// Save floating keys on the board
	ss << "KEYS\t" << floatingKeyInstances.size() << "\n";
	for (const auto & key : floatingKeyInstances) {
		ss << "KEY\t" << key.pos.x << "\t" << key.pos.y << "\t" << key.set << "\n";
	}

	return ss.str();
}

//--------------------------------------------------------------
void ofApp::applySnapshotString(const std::string & data) {
	std::istringstream ss(data);
	std::string line;

	isMultiplayer = true;
	hasReceivedHandshake = true;
	gameplaySeededByHost = true;

	// Reset transient visuals
	activeDiceRolls.clear();
	activeFloatingTexts.clear();
	particles.clear();
	activeCardDisplays.clear();
	activePlayedCardAnimations.clear();
	activeRemovedCardAnimations.clear();
	activeStolenCardAnimations.clear();
	animationPath.clear();
	isPlayerAnimating = false;
	animatingPlayerIndex = -1;
	endTurnLocked = false;
	waitingForTurnStartFromHost = false;
	pendingKeyDraftAccept = false;
	pendingKeyDraftPlayer = -1;
	pendingKeyDraftClass = 0;

	// Reset interaction state to avoid broken selections after restore
	playerAction = NONE;
	selectedPieceGridX = -1;
	selectedPieceGridY = -1;
	selectedCardIndex = -1;
	draggedCardIndex = -1;
	hoveredCardIndex = -1;
	lastHoveredCardIndex = -1;
	hoverPath.clear();
	lastHoverGridPos = { -1, -1 };
	isTargetingDeath = false;
	deathCardIndex = -1;
	isTargetingHeal = false;
	healCardIndex = -1;
	isTargetingMagicBolt = false;
	magicBoltCardIndex = -1;
	isTargetingTeleport = false;
	pendingTeleportCardIndex = -1;
	isTargetingHellhound = false;
	hellhoundCardIndex = -1;
	isTargetingChainLightning = false;
	chainLightningCardIndex = -1;
	isTargetingAmnesia = false;
	pendingAmnesiaCardIndex = -1;
	isTargetingDoubleHanded = false;
	pendingDoubleHandedCardIndex = -1;
	isTargetingTortoiseDamage = false;
	isShowingTooltip = false;
	isHoveringPile = false;
	currentPileView = VIEW_NONE;
	currentPileViewPlayerIndex = -1;
	isShowingPileView = false;
	isHoveringUnit = false;
	hoveredUnitIndex = -1;
	hoveredPilePlayerIndex = -1;

	// Reset camera to a stable position after restore
	cameraTargetZoom = 37.0f;
	cameraCurrentZoom = 37.0f;
	cameraTargetPan = glm::vec3(0, 0, 0);
	cameraCurrentPan = glm::vec3(0, 0, 0);
	isTopDownView = false;
	cam.setPosition(0, cameraCurrentZoom * 1.18f, cameraCurrentZoom * 0.70f);
	cam.lookAt(cameraCurrentPan);
	cam2.setPosition(0, cameraCurrentZoom * 1.18f, -(cameraCurrentZoom * 0.70f));
	cam2.lookAt(glm::vec3(cameraCurrentPan.x, cameraCurrentPan.y, -cameraCurrentPan.z));
	cameraCurrentPos = cam.getPosition();
	cameraCurrentPos2 = cam2.getPosition();
	cameraCurrentLookAt = cameraCurrentPan;
	cameraCurrentLookAt2 = glm::vec3(cameraCurrentPan.x, cameraCurrentPan.y, -cameraCurrentPan.z);

	players.clear();

	while (std::getline(ss, line)) {
		if (line.empty()) continue;
		auto parts = splitTabs(line);
		if (parts.empty()) continue;
		if (parts[0] == "STATE" && parts.size() >= 13) {
			currentState = (GameState)std::stoi(parts[1]);
			currentPlayerIndex = std::stoi(parts[2]);
			globalTurnCounter = std::stoi(parts[3]);
			isInGameDraft = (std::stoi(parts[4]) != 0);
			draftStage = std::stoi(parts[5]);
			draftPlayerIndex = std::stoi(parts[6]);
			draftPicksRemaining = std::stoi(parts[7]);
			currentDraftClassTier = std::stoi(parts[8]);
			hasDrawnCardsThisTurn = (std::stoi(parts[9]) != 0);
			opponentHasDrawnCardsThisTurn = (std::stoi(parts[10]) != 0);
			currentAP = std::stoi(parts[11]);
			lastAPDiceNum = std::stoi(parts[12]);
			lastAPDiceSides = (parts.size() > 13) ? std::stoi(parts[13]) : lastAPDiceSides;
			if (parts.size() > 14) {
				hasUnlimitedAP = (std::stoi(parts[14]) != 0);
			}
		} else if (parts[0] == "QUEUE" && parts.size() >= 2) {
			pendingDraftQueue.clear();
			for (size_t i = 2; i < parts.size(); ++i)
				pendingDraftQueue.push_back(std::stoi(parts[i]));
		} else if (parts[0] == "BOARD" && parts.size() >= 3) {
			const std::string & walls = parts[1];
			const std::string & magicWalls = parts[2];
			int idx = 0;
			for (int y = 0; y < BOARD_HEIGHT; ++y) {
				for (int x = 0; x < BOARD_WIDTH; ++x) {
					board[x][y].hasPlayer = false;
					board[x][y].hasWall = (idx < (int)walls.size() && walls[idx] == '1');
					board[x][y].isMagicWall = (idx < (int)magicWalls.size() && magicWalls[idx] == '1');
					idx++;
				}
			}
			buildLevelMesh();
			invalidateTargetCache();
		} else if (parts[0] == "DRAFTOPTS" && parts.size() >= 4) {
			draftOptions.clear();
			selectedDraftIndices.clear();
			auto optNames = splitEscapedList(parts[1]);
			for (const auto & name : optNames) {
				if (name.empty()) continue;
				const Card * c = findCardByName(name);
				if (c) draftOptions.push_back(*c);
			}
			if (parts[2] == "SEL") {
				auto selParts = splitEscapedList(parts[3]);
				for (const auto & s : selParts) {
					if (!s.empty()) selectedDraftIndices.push_back(std::stoi(s));
				}
			}
		} else if (parts[0] == "P" && parts.size() >= 45) {
			Player p;
			int idx = 1;
			p.playerID = std::stoi(parts[idx++]);
			p.x = std::stoi(parts[idx++]);
			p.y = std::stoi(parts[idx++]);
			p.health = std::stoi(parts[idx++]);
			p.maxHealth = std::stoi(parts[idx++]);
			p.block = std::stoi(parts[idx++]);
			p.ward = std::stoi(parts[idx++]);
			p.fortification = std::stoi(parts[idx++]);
			p.barrier = std::stoi(parts[idx++]);
			p.holyBlock = std::stoi(parts[idx++]);
			p.luck = std::stoi(parts[idx++]);
			p.bonusTurns = std::stoi(parts[idx++]);
			p.facingAngle = std::stof(parts[idx++]);
			p.onFire = (std::stoi(parts[idx++]) != 0);
			p.hasRegeneration = (std::stoi(parts[idx++]) != 0);
			p.nextTurnAPBonus = std::stoi(parts[idx++]);
			p.shocksPlayedThisTurn = std::stoi(parts[idx++]);
			p.flurryOfFistsActive = (std::stoi(parts[idx++]) != 0);
			p.isParalyzed = (std::stoi(parts[idx++]) != 0);
			p.paralysisHeadsCount = std::stoi(parts[idx++]);
			p.isPoisoned = (std::stoi(parts[idx++]) != 0);
			p.poisonReduction = std::stoi(parts[idx++]);
			p.nextAttackAddPoison = (std::stoi(parts[idx++]) != 0);
			p.nextTurnD10AP = (std::stoi(parts[idx++]) != 0);
			p.nextTurnExtraDraw = (std::stoi(parts[idx++]) != 0);
			p.isReplicatePending = (std::stoi(parts[idx++]) != 0);
			p.nextTurnBonusDiceFromMinions = (std::stoi(parts[idx++]) != 0);
			p.strengthenElementsTurnsRemaining = std::stoi(parts[idx++]);
			p.sleepTurnsRemaining = std::stoi(parts[idx++]);
			p.summonedOnTurnCycle = std::stoi(parts[idx++]);
			p.summonOrder = std::stoi(parts[idx++]);
			p.isMinion = (std::stoi(parts[idx++]) != 0);
			p.isSkeleton = (std::stoi(parts[idx++]) != 0);
			p.isGolem = (std::stoi(parts[idx++]) != 0);
			p.isHellhound = (std::stoi(parts[idx++]) != 0);
			p.isWolf = (std::stoi(parts[idx++]) != 0);
			p.isKobold = (std::stoi(parts[idx++]) != 0);
			p.isDemon = (std::stoi(parts[idx++]) != 0);
			p.isWallUnit = (std::stoi(parts[idx++]) != 0);
			p.isMagicWallUnit = (std::stoi(parts[idx++]) != 0);
			p.isKoboldKing = (std::stoi(parts[idx++]) != 0);
			p.isFaerie = (std::stoi(parts[idx++]) != 0);
			p.isAssistant = (std::stoi(parts[idx++]) != 0);
			p.directSummonerID = std::stoi(parts[idx++]);
			p.assistantRerollUsedThisTurn = (std::stoi(parts[idx++]) != 0);
			p.freeKickTurns = std::stoi(parts[idx++]);
			p.inTortoiseForm = (std::stoi(parts[idx++]) != 0);
			p.tortoiseDamageTaken = std::stoi(parts[idx++]);
			p.pendingTortoiseDamage = (std::stoi(parts[idx++]) != 0);
			p.pendingTortoiseDamageValue = std::stoi(parts[idx++]);
			p.ownerID = std::stoi(parts[idx++]);
			p.inGhostForm = (std::stoi(parts[idx++]) != 0);
			p.ghostDamageTaken = std::stoi(parts[idx++]);
			p.originalModelType = unescapeField(parts[idx++]);

			auto decodeCards = [&](const std::string & list, std::vector<Card> & outVec) {
				outVec.clear();
				if (list.empty()) return;
				auto names = splitEscapedList(list);
				for (const auto & n : names) {
					if (n.empty()) continue;
					const Card * c = findCardByName(n);
					if (c)
						outVec.push_back(*c);
					else {
						Card fallback;
						fallback.name = n;
						outVec.push_back(fallback);
					}
				}
			};

			// Remaining fields are tagged
			while (idx + 1 < (int)parts.size()) {
				std::string tag = parts[idx++];
				std::string value = parts[idx++];
				if (tag == "DECK")
					decodeCards(value, p.deck);
				else if (tag == "HAND")
					decodeCards(value, p.hand);
				else if (tag == "DISCARD")
					decodeCards(value, p.discardPile);
				else if (tag == "PLAYED")
					decodeCards(value, p.playedCardsPile);
				else if (tag == "PLAYEDTYPES") {
					p.cardsPlayedThisTurn.clear();
					if (!value.empty()) {
						auto types = splitEscapedList(value);
						for (const auto & t : types) {
							if (!t.empty()) p.cardsPlayedThisTurn.push_back((CardType)std::stoi(t));
						}
					}
				}
			}
			players.push_back(p);
		}
	}

	// Parse graveyard and keys from snapshot (restart from beginning with saved ss data)
	// Actually, we need to continue parsing the remaining lines from the stream
	// The graveyard and keys come after all players, so they'll be in subsequent getline calls
	graveyard.clear();
	floatingKeyInstances.clear();

	while (std::getline(ss, line)) {
		if (line.empty()) continue;
		auto parts = splitTabs(line);
		if (parts.empty()) continue;

		if (parts[0] == "GRAVE" && parts.size() >= 4) {
			// GRAVE\tx\ty\tturnDied\tdeck_list
			DeathMarker grave;
			grave.x = std::stoi(parts[1]);
			grave.y = std::stoi(parts[2]);
			grave.turnDied = std::stoi(parts[3]);

			// Decode deck
			if (parts.size() > 4) {
				grave.deck.clear();
				auto names = splitEscapedList(parts[4]);
				for (const auto & n : names) {
					if (n.empty()) continue;
					const Card * c = findCardByName(n);
					if (c) grave.deck.push_back(*c);
				}
			}
			graveyard.push_back(grave);
		} else if (parts[0] == "KEY" && parts.size() >= 4) {
			// KEY\tx\ty\tset
			FloatingKey key;
			key.pos.x = std::stoi(parts[1]);
			key.pos.y = std::stoi(parts[2]);
			key.set = std::stoi(parts[3]);
			floatingKeyInstances.push_back(key);
		}
	}

	// Rebuild hasPlayer from player positions
	for (int y = 0; y < BOARD_HEIGHT; ++y) {
		for (int x = 0; x < BOARD_WIDTH; ++x) {
			board[x][y].hasPlayer = false;
		}
	}
	for (const auto & p : players) {
		if (p.x >= 0 && p.x < BOARD_WIDTH && p.y >= 0 && p.y < BOARD_HEIGHT) {
			board[p.x][p.y].hasPlayer = true;
		}
	}

	if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
		playerVisualPos = gridToWorld(players[currentPlayerIndex].x, players[currentPlayerIndex].y);
	}

	// Sync visual positions for ALL players (critical for multiplayer)
	for (auto & p : players) {
		p.visualPos = gridToWorld(p.x, p.y);
	}

	// Keep draft camera consistent after restore
	draftingCameraLockedToClient = (currentState == STATE_DRAFTING && isMultiplayer && myLocalPlayerID == 1);

	// Rebuild all visual geometry
	buildLevelMesh();
	buildFloorMesh();
	invalidateTargetCache();

	// Clear any stale highlights from pre-restore state
	clearHighlights();
	calculateTargetHighlights();

	ofLogNotice("Snapshot") << "applySnapshotString complete. Players: " << players.size() << " CurrentPlayer: " << currentPlayerIndex;
}

//--------------------------------------------------------------
void ofApp::sendSnapshotToClient() {
	if (!isMultiplayer || !isHost()) return;
	std::string data = buildSnapshotString();
	uint32_t snapshotId = ++lastSnapshotId;

	SnapshotBeginPacket begin = {};
	begin.type = PKT_SNAPSHOT_BEGIN;
	begin.playerID = myLocalPlayerID;
	begin.snapshotId = snapshotId;
	begin.totalSize = (uint32_t)data.size();
	steamManager.sendPacket(&begin, sizeof(begin));

	const size_t chunkSize = sizeof(((SnapshotChunkPacket *)0)->data);
	for (size_t offset = 0; offset < data.size(); offset += chunkSize) {
		SnapshotChunkPacket chunk = {};
		chunk.type = PKT_SNAPSHOT_CHUNK;
		chunk.playerID = myLocalPlayerID;
		chunk.snapshotId = snapshotId;
		chunk.offset = (uint32_t)offset;
		chunk.chunkSize = (uint16_t)std::min(chunkSize, data.size() - offset);
		memcpy(chunk.data, data.data() + offset, chunk.chunkSize);
		steamManager.sendPacket(&chunk, sizeof(chunk));
	}

	SnapshotEndPacket end = {};
	end.type = PKT_SNAPSHOT_END;
	end.playerID = myLocalPlayerID;
	end.snapshotId = snapshotId;
	steamManager.sendPacket(&end, sizeof(end));
}

//--------------------------------------------------------------
glm::vec3 ofApp::gridToWorld(int gridX, int gridY) {
	float worldX = (gridX - BOARD_WIDTH / 2.0f) * TILE_SIZE + (TILE_SIZE / 2.0f);
	float worldZ = (gridY - BOARD_HEIGHT / 2.0f) * TILE_SIZE + (TILE_SIZE / 2.0f);
	return glm::vec3(worldX, 0, worldZ);
}

//--------------------------------------------------------------
// Transform grid coords with camera flip for multiplayer perspective
glm::vec3 ofApp::transformGridToWorld(int gx, int gy) {
	if (shouldFlipCamera()) {
		// Flip both X and Y for client to see opponent's side as "top"
		gx = (BOARD_WIDTH - 1) - gx;
		gy = (BOARD_HEIGHT - 1) - gy;
	}
	return gridToWorld(gx, gy);
}

//--------------------------------------------------------------
glm::ivec2 ofApp::transformWorldToGrid(glm::vec3 worldPos) {
	// Inverse of gridToWorld
	int gx = (int)std::round((worldPos.x - TILE_SIZE / 2.0f) / TILE_SIZE + BOARD_WIDTH / 2.0f);
	int gy = (int)std::round((worldPos.z - TILE_SIZE / 2.0f) / TILE_SIZE + BOARD_HEIGHT / 2.0f);

	if (shouldFlipCamera()) {
		gx = (BOARD_WIDTH - 1) - gx;
		gy = (BOARD_HEIGHT - 1) - gy;
	}

	return glm::ivec2(gx, gy);
}

//--------------------------------------------------------------
int ofApp::getVisualPlayerIndex(int actualPlayerIndex) {
	// For client (playerID=1), swap player indices visually so they always see themselves as "bottom left"
	if (shouldFlipCamera()) {
		return actualPlayerIndex == 0 ? 1 : 0;
	}
	return actualPlayerIndex;
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

	Player & p = players[currentPlayerIndex]; // Get current player

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

			// Basic Bounds Check
			if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && !board[nx][ny].visited && nextCost <= currentAP) {

				// --- GHOST WALL LOGIC ---
				bool isWall = board[nx][ny].hasWall;
				bool isOccupied = board[nx][ny].hasPlayer; // Occupied by another unit

				bool canEnter = false;

				if (!isOccupied) {
					if (!isWall) {
						canEnter = true; // Normal empty tile
					} else if (p.inGhostForm) {
						// Ghost entering wall: Allowed ONLY if they have > 1 AP remaining
						// This ensures they can move *out* of the wall next step
						// Current AP is total. nextCost is cost to reach *this* wall tile.
						// So remaining AP = currentAP - nextCost.
						if ((currentAP - nextCost) >= 1) {
							canEnter = true;
						}
					}
				}

				if (canEnter) {
					board[nx][ny].visited = true;
					q.push({ neighbor, nextCost });
				}
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
int ofApp::getGameRandom(int min, int max) {
	// std::uniform_int_distribution is inclusive for integers
	std::uniform_int_distribution<int> dist(min, max);
	return dist(gameplayRNG);
}
//--------------------------------------------------------------
int ofApp::startDiceRoll(int numDice, int sides, DicePurpose purpose, std::string label, int ownerIndex) {
	int totalRollResult = 0;
	int luckBonus = 0;

	// 1. Calculate Luck Bonus (Deterministic Logic)
	int luckOwner = (ownerIndex >= 0 && ownerIndex < (int)players.size()) ? ownerIndex : currentPlayerIndex;
	if (luckOwner != -1) {
		luckBonus = players[luckOwner].luck + computePassiveLuck(luckOwner);
	}

	// Earthquake rolls should not be affected by luck
	if (purpose == PURPOSE_EARTHQUAKE_DAMAGE || purpose == PURPOSE_EARTHQUAKE_DISTANCE) {
		luckBonus = 0;
	}

	// 2. Set UI Label
	if (label != "") {
		currentDiceLabel = label;
	} else {
		switch (purpose) {
		case PURPOSE_AP:
			currentDiceLabel = "Rolling for Action Points";
			break;
		case PURPOSE_DAMAGE:
			currentDiceLabel = "Rolling Damage";
			break;
		case PURPOSE_RANGE:
			currentDiceLabel = "Rolling Range";
			break;
		case PURPOSE_BARRIER_GAIN:
			currentDiceLabel = "Rolling Barrier";
			break;
		case PURPOSE_COIN_FLIP:
			currentDiceLabel = "Flipping Coin";
			break;
		case PURPOSE_HP:
			currentDiceLabel = "Rolling Health";
			break;
		case PURPOSE_HEALING:
			currentDiceLabel = "Rolling Heal Amount";
			break;
		case PURPOSE_BONUS_AP:
			currentDiceLabel = "Rolling Bonus AP";
			break;
		case PURPOSE_TIME_VORTEX:
			currentDiceLabel = "Rolling Extra Turns";
			break;
		default:
			currentDiceLabel = "Rolling Dice...";
			break;
		}
	}

	// 3. Loop through dice
	for (int i = 0; i < numDice; ++i) {
		DiceRoll newRoll;
		newRoll.purpose = purpose;
		newRoll.sides = sides;
		newRoll.startTime = ofGetElapsedTimef();
		newRoll.associatedUnit = ownerIndex;

		// --- CORE DETERMINISM (GAMEPLAY LOGIC) ---
		// 1. Get the synced random number
		int rawRoll = getGameRandom(1, sides);

		// 2. Apply game logic (Luck)
		int finalRoll;
		if (sides == 2) {
			finalRoll = rawRoll; // Luck doesn't change coin logic, just visual flair if needed
		} else {
			finalRoll = rawRoll + luckBonus;
		}

		totalRollResult += finalRoll;
		newRoll.result = finalRoll;
		newRoll.rawResult = rawRoll; // Visuals rely on raw result to show correct face

		// Log dice rolls for debugging multiplayer sync
		if (isMultiplayer && purpose == PURPOSE_AP) {
			ofLogNotice("Dice") << "Rolled dice[" << i << "]: rawResult=" << rawRoll << " finalResult=" << finalRoll << " (owner=" << ownerIndex << ")";
		}

		// --- VISUALS (MUST BE DECOUPLED FROM GAMEPLAY RNG) ---

		// Use visualRNG for visual axis generation (Unsynced)
		std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);
		glm::vec3 rndAxis(axisDist(visualRNG), axisDist(visualRNG), axisDist(visualRNG));
		if (glm::length(rndAxis) < 0.01f) rndAxis = glm::vec3(0, 1, 0);
		newRoll.rotationAxis = glm::normalize(rndAxis);

		// Use visualRNG for visual wobble (Unsynced)
		std::uniform_real_distribution<float> wobbleDist(-25.0f, 25.0f);
		float wobbleAmount = wobbleDist(visualRNG);

		// --- ROTATION MATH ---
		// This calculates the Quaternion needed to rotate the 'rawResult' face up towards the camera (0,1,0)

		if (sides == 2) {
			// Coin
			glm::quat faceRotation;
			glm::quat flip180X = glm::angleAxis(glm::radians(180.0f), glm::vec3(1, 0, 0));
			glm::quat rot180Y = glm::angleAxis(glm::radians(180.0f), glm::vec3(0, 1, 0));

			if (newRoll.rawResult == 1)
				faceRotation = glm::quat(1, 0, 0, 0); // Tails
			else
				faceRotation = flip180X * rot180Y; // Heads

			std::uniform_real_distribution<float> yawDist(-15.0f, 15.0f);
			glm::quat randomYaw = glm::angleAxis(glm::radians(yawDist(visualRNG)), glm::vec3(0, 1, 0));
			newRoll.finalQuat = randomYaw * faceRotation;

		} else if (sides == 4) {
			// D4
			glm::vec3 faceVec;
			float correctionDeg = 0.0f;
			int v = std::min(sides, newRoll.rawResult);
			switch (v) {
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
			default:
				faceVec = glm::vec3(0.943f, -0.333f, 0.0f);
				correctionDeg = 180.0f;
				break;
			}
			glm::quat align = matchFaceToCamera(faceVec);
			glm::quat manualRot = glm::angleAxis(glm::radians(correctionDeg), glm::vec3(0, 1, 0));
			glm::quat wobble = glm::angleAxis(glm::radians(wobbleAmount), glm::vec3(0, 1, 0));
			newRoll.finalQuat = wobble * manualRot * align;
		} else if (sides == 6) {
			// D6 (Fixed for Procedural Mesh: 1=+Z, 2=+Y, 3=+X, 4=-X, 5=-Y, 6=-Z)
			glm::vec3 faceVec;
			int v = std::min(sides, newRoll.rawResult);
			switch (v) {
			case 1:
				faceVec = glm::vec3(0, 0, 1);
				break; // Front
			case 2:
				faceVec = glm::vec3(0, 1, 0);
				break; // Top
			case 3:
				faceVec = glm::vec3(1, 0, 0);
				break; // Right
			case 4:
				faceVec = glm::vec3(-1, 0, 0);
				break; // Left
			case 5:
				faceVec = glm::vec3(0, -1, 0);
				break; // Bottom
			case 6:
				faceVec = glm::vec3(0, 0, -1);
				break; // Back
			default:
				faceVec = glm::vec3(0, 1, 0);
				break;
			}
			glm::quat align = matchFaceToCamera(faceVec);
			glm::quat wobble = glm::angleAxis(glm::radians(wobbleAmount), glm::vec3(0, 1, 0));
			newRoll.finalQuat = wobble * align;
		} else if (sides == 10) {
			// D10
			glm::vec3 faceVec;
			int v = std::min(sides, newRoll.rawResult);

			switch (v) {
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
			glm::quat wobble = glm::angleAxis(glm::radians(wobbleAmount), glm::vec3(0, 1, 0));
			newRoll.finalQuat = wobble * align;
		} else if (sides == 20) {
			// D20
			glm::vec3 v;
			int n = std::min(sides, newRoll.rawResult);

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
			glm::quat wobble = glm::angleAxis(glm::radians(wobbleAmount), glm::vec3(0, 1, 0));
			newRoll.finalQuat = wobble * align;
		}

		activeDiceRolls.push_back(newRoll);
	}

	// 4. Show Floating Text for Luck
	if (luckBonus > 0) {
		Player & caster = players[currentPlayerIndex];
		spawnFloatingText(
			gridToWorld(caster.x, caster.y),
			"+" + ofToString(luckBonus) + " Luck!",
			ofColor::gold);
	}

	ofLogNotice("Dice") << "Final total result for " << numDice << "d" << sides << ": " << totalRollResult;
	return totalRollResult;
}
//--------------------------------------------------------------
std::vector<Player *> ofApp::findCleaveTargets(glm::vec2 direction) {
	std::vector<Player *> hittablePlayers;
	if (players.empty() || currentPlayerIndex < 0) return hittablePlayers;

	int px = players[currentPlayerIndex].x;
	int py = players[currentPlayerIndex].y;

	// 1. Determine the 3 tiles in the swipe arc
	glm::vec2 centerTile = { px + direction.x, py + direction.y };
	std::vector<glm::vec2> cleaveTiles;

	if (direction.y != 0) { // Vertical Aim
		cleaveTiles.push_back({ centerTile.x - 1, centerTile.y }); // Left side
		cleaveTiles.push_back({ centerTile.x, centerTile.y }); // Center
		cleaveTiles.push_back({ centerTile.x + 1, centerTile.y }); // Right side
	} else { // Horizontal Aim
		cleaveTiles.push_back({ centerTile.x, centerTile.y - 1 }); // Top side
		cleaveTiles.push_back({ centerTile.x, centerTile.y }); // Center
		cleaveTiles.push_back({ centerTile.x, centerTile.y + 1 }); // Bottom side
	}

	// 2. Iterate through potential target tiles
	for (const auto & targetPos : cleaveTiles) {
		int tx = (int)targetPos.x;
		int ty = (int)targetPos.y;

		// A. Basic Validation: Bounds and Target-Tile-Is-Not-Wall
		if (tx < 0 || tx >= BOARD_WIDTH || ty < 0 || ty >= BOARD_HEIGHT) continue;
		if (board[tx][ty].hasWall) continue;

		// B. Check if a player is actually on this tile
		Player * foundTarget = nullptr;
		for (auto & p : players) {
			if (p.x == tx && p.y == ty) {
				foundTarget = &p;
				break;
			}
		}

		// If no player here, skip logic
		if (!foundTarget) continue;

		// C. PATH BLOCKING LOGIC (Merged from isSwipePathBlocked)
		bool isBlocked = false;

		// If target is Diagonal from caster (Side targets), check for Pinch
		if (px != tx && py != ty) {
			// Check the two shared neighbors (the corners)
			// Neighbor 1: (CasterX, TargetY)
			// Neighbor 2: (TargetX, CasterY)
			bool wall1 = isTileWall(px, ty);
			bool wall2 = isTileWall(tx, py);

			// If BOTH shared neighbors are walls, you cannot swing diagonally
			if (wall1 && wall2) {
				isBlocked = true;
			}
		}
		// If target is Orthogonal (Center target), it's reachable because
		// we already confirmed the target tile itself is not a wall.

		// D. Add to list if path is clear
		if (!isBlocked) {
			hittablePlayers.push_back(foundTarget);
		}
	}

	return hittablePlayers;
}
//--------------------------------------------------------------
void ofApp::tryTriggerShellSpike() {
	if (currentPlayerIndex < 0 || currentPlayerIndex >= (int)players.size()) return;
	Player & currentPlayer = players[currentPlayerIndex];

	if (!currentPlayer.inTortoiseForm) return;

	// Check if there are any adjacent units (ANY unit, including allies)
	bool hasAdjacentUnit = false;

	for (const auto & p : players) {
		if (p.x < 0) continue; // Dead
		if (&p == &currentPlayer) continue; // Self

		// Check adjacency
		int dx = abs(p.x - currentPlayer.x);
		int dy = abs(p.y - currentPlayer.y);
		if ((dx <= 1 && dy <= 1) && (dx + dy > 0)) {
			hasAdjacentUnit = true;
			break;
		}
	}

	if (hasAdjacentUnit) {
		// Enter tortoise damage targeting mode
		isTargetingTortoiseDamage = true;
		calculateTargetHighlights(); // Show green highlights on valid targets
		spawnFloatingText(gridToWorld(currentPlayer.x, currentPlayer.y) + glm::vec3(0, 1.0f, 0),
			"Shell Spike!", ofColor::darkGreen);
		ofLogNotice("Tortoise Form") << "Triggered Shell Spike damage - choose adjacent target.";
	}
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
		// 1. Dark Overlay
		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

		// Use standardized panel helper (Barrier = hotPink, Purge = cyan)
		drawCardChoicePanel(dispelMenuRect, "Choose Dispel Effect", "(1d20 vs Magic/Fire)",
			dispelBtnBarrier, dispelBtnPurge, "Non-Phys Barrier", "Remove Status",
			ofColor::hotPink, ofColor::cyan, true, true);
	}

	// --- PHASE 2: TARGETING (Top Prompt) ---
	if (isDispelTargeting) {
		float promptW = 600;
		float promptX = ofGetWidth() / 2 - promptW / 2;

		ofSetColor(0, 0, 0, 200);
		ofDrawRectRounded(promptX, 50, promptW, 60, 10);

		ofSetColor(ofColor::white);
		string text = "Select Self or Adjacent Unit to Cure";
		ofRectangle textBox = uiFont.getStringBoundingBox(text, 0, 0);
		uiFont.drawString(text, ofGetWidth() / 2 - textBox.width / 2, 90);
	}

	// --- PHASE 3: STATUS SELECTION ---
	if (isDispelStatusSelectOpen) {
		// Overlay
		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

		// Background
		ofSetColor(50, 50, 50, 255);
		ofDrawRectRounded(statusSelectMenuRect, 15);

		// Title
		ofSetColor(ofColor::white);
		string title = "Select Status to Remove";
		ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
		uiFont.drawString(title, statusSelectMenuRect.getCenter().x - titleBox.width / 2, statusSelectMenuRect.y + 45);

		// Buttons
		for (size_t i = 0; i < statusSelectButtons.size(); i++) {
			ofSetColor(ofColor::orange);
			ofDrawRectRounded(statusSelectButtons[i], 10);

			ofSetColor(ofColor::black);
			string label = statusSelectLabels[i];
			ofRectangle labelBox = uiFont.getStringBoundingBox(label, 0, 0);
			uiFont.drawString(label, statusSelectButtons[i].getCenter().x - labelBox.width / 2, statusSelectButtons[i].getCenter().y + labelBox.height / 2);
		}
	}
}
//--------------------------------------------------------------
void ofApp::drawDoubleHandedUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// Dark Overlay
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// Background
	ofSetColor(50, 50, 50, 255);
	ofDrawRectRounded(doubleHandedMenuRect, 15);

	// Title
	ofSetColor(ofColor::white);
	string title = "Double Handed: Choose Cards";
	ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
	uiFont.drawString(title, doubleHandedMenuRect.getCenter().x - titleBox.width / 2, doubleHandedMenuRect.y + 60);

	// Punch Button
	ofSetColor(ofColor::indianRed);
	ofDrawRectRounded(btnAddPunches, 10);
	ofSetColor(ofColor::white);
	uiFont.drawString("Add 2x Punch", btnAddPunches.x + 20, btnAddPunches.getCenter().y + 5);

	// Block Button
	ofSetColor(ofColor::slateGray);
	ofDrawRectRounded(btnAddBlocks, 10);
	ofSetColor(ofColor::white);
	uiFont.drawString("Add 2x Hand Block", btnAddBlocks.x + 10, btnAddBlocks.getCenter().y + 5);
}
//--------------------------------------------------------------
void ofApp::drawAmnesiaMenuUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// Dark Overlay
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// Background
	ofSetColor(50, 50, 50, 255);
	ofDrawRectRounded(amnesiaMenuRect, 15);

	// Title
	ofSetColor(ofColor::white);
	string title = "Amnesia: Choose Target";
	ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
	uiFont.drawString(title, amnesiaMenuRect.getCenter().x - titleBox.width / 2, amnesiaMenuRect.y + 60);

	// Self Button
	ofSetColor(ofColor::magenta);
	ofDrawRectRounded(amnesiaBtnSelf, 10);
	ofSetColor(ofColor::white);
	string selfLabel = "Self";
	ofRectangle selfBox = uiFont.getStringBoundingBox(selfLabel, 0, 0);
	uiFont.drawString(selfLabel, amnesiaBtnSelf.getCenter().x - selfBox.width / 2, amnesiaBtnSelf.getCenter().y + 5);

	// Adjacent Button
	ofSetColor(ofColor::purple);
	ofDrawRectRounded(amnesiaBtnAdjacent, 10);
	ofSetColor(ofColor::white);
	string adjLabel = "Adjacent Unit";
	ofRectangle adjBox = uiFont.getStringBoundingBox(adjLabel, 0, 0);
	uiFont.drawString(adjLabel, amnesiaBtnAdjacent.getCenter().x - adjBox.width / 2, amnesiaBtnAdjacent.getCenter().y + 5);
}
//--------------------------------------------------------------
void ofApp::drawMagicHandUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	// 1. Dark Overlay
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// 2. Menu Background (Matching Wisdom Boon theme)
	ofSetColor(40, 40, 80, 255);
	ofDrawRectRounded(wisdomMenuRect, 15);

	// 3. Title
	ofSetColor(ofColor::white);
	string title = "Giant Magic Hand";
	ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
	uiFont.drawString(title, wisdomMenuRect.getCenter().x - titleBox.width / 2, wisdomMenuRect.y + 50);

	// Calculate Buttons
	float btnW = 200, btnH = 80;
	float spacing = 40;
	float startX = wisdomMenuRect.x + (wisdomMenuRect.width - (btnW * 2 + spacing)) / 2;
	float btnY = wisdomMenuRect.y + 120;

	// Use temp rects for drawing (input handling uses same math in mousePressed)
	ofRectangle btnPush(startX, btnY, btnW, btnH);
	ofRectangle btnPull(startX + btnW + spacing, btnY, btnW, btnH);

	// Draw Push (Reddish)
	ofSetColor(ofColor::indianRed);
	ofDrawRectRounded(btnPush, 10);
	ofSetColor(ofColor::white);
	string pushTxt = "PUSH";
	ofRectangle pBox = uiFont.getStringBoundingBox(pushTxt, 0, 0);
	uiFont.drawString(pushTxt, btnPush.getCenter().x - pBox.width / 2, btnPush.getCenter().y + pBox.height / 2);

	// Draw Pull (Blueish)
	ofSetColor(ofColor::royalBlue);
	ofDrawRectRounded(btnPull, 10);
	ofSetColor(ofColor::white);
	string pullTxt = "PULL";
	ofRectangle plBox = uiFont.getStringBoundingBox(pullTxt, 0, 0);
	uiFont.drawString(pullTxt, btnPull.getCenter().x - plBox.width / 2, btnPull.getCenter().y + plBox.height / 2);

	// Subtext
	ofSetColor(200, 200, 200);
	string sub = "Push: Dmg units behind | Pull: Move back";
	ofRectangle sBox = uiFont.getStringBoundingBox(sub, 0, 0);
	ofPushMatrix();
	ofTranslate(wisdomMenuRect.getCenter().x - (sBox.width * 0.7) / 2, wisdomMenuRect.y + 90);
	ofScale(0.7, 0.7);
	uiFont.drawString(sub, 0, 0);
	ofPopMatrix();
}
//--------------------------------------------------------------
void ofApp::drawCardSpawnerUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// Semi-transparent dark overlay
	ofSetColor(0, 0, 0, 150);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// KRunner-style bar - centered horizontally, near top
	float barWidth = 600.0f;
	float barHeight = 50.0f;
	float barX = (ofGetWidth() - barWidth) / 2.0f;
	float barY = ofGetHeight() * 0.15f;

	// Main input bar background
	ofSetColor(30, 30, 30, 250);
	ofDrawRectRounded(barX, barY, barWidth, barHeight, 8);

	// Input area (left side)
	float inputWidth = barWidth - 180.0f; // Room for buttons
	cardSpawnerInputRect.set(barX, barY, inputWidth, barHeight);

	// Draw input border
	ofSetColor(80, 80, 80);
	ofNoFill();
	ofSetLineWidth(2);
	ofDrawRectRounded(barX + 2, barY + 2, inputWidth - 4, barHeight - 4, 6);
	ofFill();

	// Draw input text
	ofSetColor(ofColor::white);
	string displayText = cardSpawnerInput;
	if (displayText.empty()) {
		ofSetColor(120, 120, 120);
		displayText = "Type card name...";
	}
	// Add blinking cursor
	if (!cardSpawnerInput.empty() || (int)(ofGetElapsedTimef() * 2) % 2 == 0) {
		if (cardSpawnerInput.empty()) {
			displayText = "|";
			ofSetColor(ofColor::white);
		} else {
			displayText += "|";
		}
	}
	uiFont.drawString(displayText, barX + 15, barY + barHeight / 2 + 6);

	// Quantity display and +/- buttons
	float btnSize = 30.0f;
	float btnY = barY + (barHeight - btnSize) / 2.0f;
	float quantityX = barX + inputWidth + 10.0f;

	// Minus button
	cardSpawnerMinusButton.set(quantityX, btnY, btnSize, btnSize);
	ofSetColor(60, 60, 60);
	ofDrawRectRounded(cardSpawnerMinusButton, 5);
	ofSetColor(ofColor::white);
	uiFont.drawString("-", quantityX + 10, btnY + btnSize / 2 + 6);

	// Quantity number
	ofSetColor(ofColor::white);
	string qtyStr = ofToString(cardSpawnerQuantity);
	uiFont.drawString(qtyStr, quantityX + btnSize + 12, btnY + btnSize / 2 + 6);

	// Plus button
	cardSpawnerPlusButton.set(quantityX + btnSize + 35, btnY, btnSize, btnSize);
	ofSetColor(60, 60, 60);
	ofDrawRectRounded(cardSpawnerPlusButton, 5);
	ofSetColor(ofColor::white);
	uiFont.drawString("+", quantityX + btnSize + 44, btnY + btnSize / 2 + 6);

	// Encyclopedia button
	float encBtnWidth = 40.0f;
	cardSpawnerEncyclopediaButton.set(barX + barWidth - encBtnWidth - 45, btnY, encBtnWidth, btnSize);
	ofSetColor(70, 50, 100);
	ofDrawRectRounded(cardSpawnerEncyclopediaButton, 5);
	// Draw 3 horizontal lines (hamburger menu icon)
	ofSetColor(ofColor::white);
	float lineX = cardSpawnerEncyclopediaButton.x + 10;
	float lineWidth = 20;
	float lineY1 = btnY + 8;
	float lineY2 = btnY + btnSize / 2;
	float lineY3 = btnY + btnSize - 8;
	ofSetLineWidth(2);
	ofDrawLine(lineX, lineY1, lineX + lineWidth, lineY1);
	ofDrawLine(lineX, lineY2, lineX + lineWidth, lineY2);
	ofDrawLine(lineX, lineY3, lineX + lineWidth, lineY3);

	// Close button (X)
	cardSpawnerCloseButton.set(barX + barWidth - 40, btnY, btnSize, btnSize);
	ofSetColor(100, 40, 40);
	ofDrawRectRounded(cardSpawnerCloseButton, 5);
	ofSetColor(ofColor::white);
	uiFont.drawString("X", cardSpawnerCloseButton.x + 9, btnY + btnSize / 2 + 6);

	// Draw filtered card suggestions below the bar
	if (!cardSpawnerInput.empty() && !filteredCards.empty()) {
		float suggestionY = barY + barHeight + 5.0f;
		float suggestionHeight = 35.0f;
		int maxSuggestions = std::min((int)filteredCards.size(), 8);

		ofSetColor(40, 40, 40, 245);
		ofDrawRectRounded(barX, suggestionY, inputWidth, suggestionHeight * maxSuggestions + 10, 8);

		for (int i = 0; i < maxSuggestions; i++) {
			float itemY = suggestionY + 5 + i * suggestionHeight;

			// Highlight on hover - check if mouse is over this item
			ofRectangle itemRect(barX + 5, itemY, inputWidth - 10, suggestionHeight - 2);
			if (itemRect.inside(ofGetMouseX(), ofGetMouseY())) {
				ofSetColor(70, 70, 100);
				ofDrawRectRounded(itemRect, 4);
			}

			ofSetColor(ofColor::white);
			string cardInfo = filteredCards[i].name + " (Cost: " + ofToString(filteredCards[i].cost) + ")";
			uiFont.drawString(cardInfo, barX + 15, itemY + suggestionHeight / 2 + 5);
		}
	}

	// Instructions text
	ofSetColor(180, 180, 180);
	string helpText = "Press ENTER to add card | ESC to close";
	ofRectangle helpBox = uiFont.getStringBoundingBox(helpText, 0, 0);
	uiFont.drawString(helpText, (ofGetWidth() - helpBox.width) / 2, barY + barHeight + (filteredCards.empty() ? 30 : 35 * std::min((int)filteredCards.size(), 8) + 45));
}

//--------------------------------------------------------------
void ofApp::drawCardEncyclopediaUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// Full overlay
	ofSetColor(0, 0, 0, 200);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// Encyclopedia panel
	float panelWidth = ofGetWidth() * 0.85f;
	float panelHeight = ofGetHeight() * 0.85f;
	float panelX = (ofGetWidth() - panelWidth) / 2.0f;
	float panelY = (ofGetHeight() - panelHeight) / 2.0f;

	encyclopediaRect.set(panelX, panelY, panelWidth, panelHeight);

	ofSetColor(25, 25, 30, 250);
	ofDrawRectRounded(encyclopediaRect, 15);

	// Title bar
	ofSetColor(40, 40, 50);
	ofDrawRectRounded(panelX, panelY, panelWidth, 50, 15);
	// Fix bottom corners of title bar
	ofDrawRectangle(panelX, panelY + 35, panelWidth, 15);

	ofSetColor(ofColor::white);
	string title = "Card Encyclopedia - Click to Add (x" + ofToString(cardSpawnerQuantity) + ")";
	ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
	uiFont.drawString(title, panelX + (panelWidth - titleBox.width) / 2, panelY + 32);

	// Close button
	encyclopediaCloseButton.set(panelX + panelWidth - 45, panelY + 10, 30, 30);
	ofSetColor(100, 40, 40);
	ofDrawRectRounded(encyclopediaCloseButton, 5);
	ofSetColor(ofColor::white);
	uiFont.drawString("X", encyclopediaCloseButton.x + 9, encyclopediaCloseButton.y + 22);

	// Card grid
	const float kBaseCardWidth = 120.0f;
	const float kCardAspectRatio = 1.4f;
	const float kBaseCardHeight = kBaseCardWidth * kCardAspectRatio;

	float contentY = panelY + 60;
	float contentHeight = panelHeight - 70;
	float cardScale = 1.2f;
	float cardW = kBaseCardWidth * cardScale;
	float cardH = kBaseCardHeight * cardScale;
	float padding = 15.0f;

	int cols = std::max(1, (int)floor((panelWidth - 2 * padding) / (cardW + padding)));
	float startX = panelX + padding + ((panelWidth - 2 * padding) - (cols * (cardW + padding) - padding)) / 2.0f;

	// Sort cards by cost for display
	std::vector<Card> sortedCards = allCards;
	std::sort(sortedCards.begin(), sortedCards.end(), [](const Card & a, const Card & b) {
		if (a.cost != b.cost) return a.cost < b.cost;
		return a.name < b.name;
	});

	int row = 0;
	int col = 0;
	for (size_t i = 0; i < sortedCards.size(); i++) {
		float drawX = startX + col * (cardW + padding);
		float drawY = contentY + row * (cardH + padding) - encyclopediaScrollOffset;

		// Only draw if visible
		if (drawY + cardH > contentY && drawY < contentY + contentHeight) {
			const Card & card = sortedCards[i];

			// Check if mouse is hovering
			ofRectangle cardRect(drawX, drawY, cardW, cardH);
			bool isHovered = cardRect.inside(ofGetMouseX(), ofGetMouseY()) && drawY >= contentY;

			if (isHovered) {
				// Glow effect
				ofSetColor(100, 150, 255, 100);
				ofDrawRectRounded(drawX - 3, drawY - 3, cardW + 6, cardH + 6, 8);
			}

			ofSetColor(255);
			cardSpriteSheet.drawSubsection(drawX, drawY, cardW, cardH,
				card.textureRect.x, card.textureRect.y,
				card.textureRect.width, card.textureRect.height);

			// Draw card name below (for easier identification)
			if (isHovered) {
				ofSetColor(255, 255, 100);
			} else {
				ofSetColor(200, 200, 200);
			}
			string shortName = card.name;
			if (shortName.length() > 15) shortName = shortName.substr(0, 12) + "...";
			uiFont.drawString(shortName, drawX, drawY + cardH + 18);
		}

		col++;
		if (col >= cols) {
			col = 0;
			row++;
		}
	}

	// Scroll indicators
	int totalRows = (sortedCards.size() + cols - 1) / cols;
	float totalContentHeight = totalRows * (cardH + padding);
	if (totalContentHeight > contentHeight) {
		// Show scroll bar
		float scrollBarHeight = contentHeight * (contentHeight / totalContentHeight);
		float scrollBarY = contentY + (encyclopediaScrollOffset / (totalContentHeight - contentHeight)) * (contentHeight - scrollBarHeight);

		ofSetColor(80, 80, 80);
		ofDrawRectRounded(panelX + panelWidth - 15, scrollBarY, 10, scrollBarHeight, 5);
	}
}

// Cancel Helper
void ofApp::cancelDoubleHanded() {
	isDoubleHandedMenuOpen = false;
	isTargetingDoubleHanded = false;
	pendingDoubleHandedCardIndex = -1;
	pendingDoubleHandedTargetIndex = -1;
	pendingDoubleHandedChoice = "";
	sendMenuState(0, -1, -1, -1); // Notify opponent that menu is closed
	calculateTargetHighlights();
}

// Resolve Logic (Adds cards and consumes AP)
void ofApp::resolveDoubleHanded(std::string cardName) {
	Player * target = getPlayer(pendingDoubleHandedTargetIndex);
	Player & caster = players[currentPlayerIndex];

	if (target) {
		// 1. Find the Card Data
		Card cardToAdd;
		bool found = false;
		for (const auto & c : allCards) {
			if (c.name == cardName) {
				cardToAdd = c;
				found = true;
				break;
			}
		}

		if (found) {
			// 2. Add copies to deck (4 if Flurry of Fists is active, otherwise 2)
			int copiesToAdd = caster.flurryOfFistsActive ? 4 : 2;
			for (int i = 0; i < copiesToAdd; i++) {
				target->deck.push_back(cardToAdd);
			}

			// 3. Shuffle (authoritative)
			shuffleGameVector(target->deck, pendingDoubleHandedTargetIndex);

			// 4. Visual Feedback
			spawnFloatingText(gridToWorld(target->x, target->y), "Added " + ofToString(copiesToAdd) + "x " + cardName, ofColor::cyan);
			ofLogNotice("Double Handed") << "Shuffled " << copiesToAdd << "x " << cardName << " into Player " << target->playerID << "'s deck.";

			// 5. Finalize Play (Cost AP, Remove Card)
			if (pendingDoubleHandedCardIndex != -1) {
				Card & playedCard = caster.hand[pendingDoubleHandedCardIndex];
				int dhCost = playedCard.cost;
				if (playedCard.name == "Kick" && caster.freeKickTurns > 0) dhCost = 0;
				currentAP -= dhCost;
				caster.playedCardsPile.push_back(playedCard);
				// Handle Replicate if active
				if (caster.isReplicatePending) {
					caster.playedCardsPile.push_back(playedCard);
					caster.isReplicatePending = false;
				}
				caster.hand.erase(caster.hand.begin() + pendingDoubleHandedCardIndex);
				calculateTargetHighlights();
			}
		}
	}
	cancelDoubleHanded();
}
//--------------------------------------------------------------
void ofApp::determineStatusOptions(Player * target) {
	statusSelectLabels.clear();
	statusSelectButtons.clear();

	if (target->onFire) statusSelectLabels.push_back("Fire");
	if (target->isParalyzed) statusSelectLabels.push_back("Paralysis");
	// Add future statuses here

	if (statusSelectLabels.empty()) {
		// Use in-game floating text instead of a system dialog (preserves fullscreen)
		if (target) {
			spawnFloatingText(gridToWorld(target->x, target->y), "Target has no status effects!", ofColor::yellow);
		} else {
			spawnFloatingText(glm::vec3(ofGetWidth() / 2, ofGetHeight() / 2, 0), "Target has no status effects!", ofColor::yellow);
		}
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
		int cost = p.hand[pendingDispelCardIndex].cost;
		std::string cardName = p.hand[pendingDispelCardIndex].name;
		if (isMultiplayer) {
			// menuChoice >= 100 encodes purge status index
			sendActionPacket(pendingDispelCardIndex, target->x, target->y, cost, 100 + statusIndex, cardName);
		}
		currentAP -= cost;
		p.discardPile.push_back(p.hand[pendingDispelCardIndex]);
		p.hand.erase(p.hand.begin() + pendingDispelCardIndex);
		calculateTargetHighlights(); // refresh UI
	}

	// Trigger Shell Spike if in Tortoise Form
	tryTriggerShellSpike();

	cancelDispel(); // Close menus
}
// ----------------- WISDOM BOON HELPERS -----------------

void ofApp::cancelWisdomBoon() {
	isWisdomBoonMenuOpen = false;
	pendingWisdomBoonCardIndex = -1;
	pendingWisdomBoonTargetIndex = -1;
	sendMenuState(0, -1, -1, -1); // Notify opponent that menu is closed
	ofLogNotice("WisdomBoon") << "Cancelled.";
}
//--------------------------------------------------------------
void ofApp::cancelBurst() {
	isBurstMenuOpen = false;
	pendingBurstCardIndex = -1;
	isTargetingBurst = false;
	burstChoice = 0;
	sendMenuState(0, -1, -1, -1); // Notify opponent that menu is closed
	ofLogNotice("Burst") << "Cancelled.";
}
//--------------------------------------------------------------
void ofApp::drawWisdomBoonUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// 1. Dark Overlay
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// 2. Menu Background

	// 3. Determine Context
	bool isSelfTarget = (pendingWisdomBoonTargetIndex == currentPlayerIndex);
	int deckSize = 0;
	if (currentPlayerIndex >= 0) deckSize = players[currentPlayerIndex].deck.size();

	// 4. Title & Description
	string title = "Wisdom Boon";
	string desc = "Effect Strength: " + ofToString(deckSize) + " (Your Deck Size)";

	// Choose accent colors: block = slateGray, magic = lighter purple
	ofColor magicAccent(180, 140, 230);
	ofColor blockAccent(120, 120, 120);

	if (isSelfTarget) {
		drawCardChoicePanel(wisdomMenuRect, title, desc, wisdomBtnDamage, wisdomBtnBlock, "Gain Block", "", blockAccent, blockAccent, true, false);
	} else {
		drawCardChoicePanel(wisdomMenuRect, title, desc, wisdomBtnDamage, wisdomBtnBlock, "Deal Magic Dmg", "", magicAccent, magicAccent, true, false);
	}
}
//--------------------------------------------------------------
void ofApp::drawBurstUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	string title = "Burst of Light";
	string desc = "Choose an effect:";

	// Accent colors: holy = yellow, heal = light green
	ofColor holyAccent(255, 213, 79); // golden yellow
	ofColor healAccent(144, 238, 144); // light green

	ofRectangle panelRect = burstMenuRect;

	// --- CHECK FOR VALID ENEMIES ---
	bool hasValidEnemy = false;
	Player & caster = players[currentPlayerIndex];
	glm::vec2 casterPos(caster.x, caster.y);

	int casterOwner = caster.isMinion ? caster.ownerID : caster.playerID;

	for (const auto & p : players) {
		// Skip self and allies
		int pOwner = p.isMinion ? p.ownerID : p.playerID;
		if (pOwner == casterOwner) continue;

		// Check LOS (Infinite Range for Burst)
		TargetInfo info = isLosTargetValid(casterPos, glm::vec2(p.x, p.y), 9999.0f, CARD_BURST_OF_LIGHT);
		if (info.reason == VALID) {
			hasValidEnemy = true;
			break;
		}
	}

	// Use standardized helper to draw primary (Deal 3 Holy) and secondary (Heal 3 HP).
	drawCardChoicePanel(panelRect, title, desc, burstBtnDamage, burstBtnHeal,
		hasValidEnemy ? "Deal 3 Holy" : "No Enemy in Sight", "Heal 3 HP",
		holyAccent, healAccent, hasValidEnemy, true);
}

//--------------------------------------------------------------
// Helper function to draw white outlined tiles that join together when adjacent
void ofApp::drawJoinedOutlines(bool highlightedTiles[BOARD_WIDTH][BOARD_HEIGHT], ofColor color, float surfaceY) {
	// This function draws outlines around groups of adjacent highlighted tiles
	// such that the outlines merge to form larger connected shapes

	ofNoFill();
	ofSetLineWidth(6); // Thicker lines
	ofSetColor(color);

	// Enable depth test so outlines are occluded by walls
	ofEnableDepthTest();

	// For each tile, draw edges that are NOT adjacent to another highlighted tile
	for (int x = 0; x < BOARD_WIDTH; x++) {
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			if (!highlightedTiles[x][y]) continue;

			// Don't skip path tiles - they should keep their white outlines even when green circles are drawn

			glm::vec3 worldPos = gridToWorld(x, y);

			// Calculate height based on wall status - draw on top of walls
			float height = surfaceY + 0.01f;
			if (board[x][y].hasWall) {
				// Draw on top of wall (wall is TILE_SIZE * 0.5 tall)
				height = (TILE_SIZE * 0.5f) + 0.06f;
			}

			// Check each of the 4 edges: top, right, bottom, left
			bool drawTop = (y == 0 || !highlightedTiles[x][y - 1]);
			bool drawBottom = (y == BOARD_HEIGHT - 1 || !highlightedTiles[x][y + 1]);
			bool drawLeft = (x == 0 || !highlightedTiles[x - 1][y]);
			bool drawRight = (x == BOARD_WIDTH - 1 || !highlightedTiles[x + 1][y]);

			// For walls, also check if adjacent tile is NOT a wall (to draw vertical edges)
			bool drawTopVertical = false;
			bool drawBottomVertical = false;
			bool drawLeftVertical = false;
			bool drawRightVertical = false;

			if (board[x][y].hasWall) {
				if (drawTop && y > 0 && !board[x][y - 1].hasWall) drawTopVertical = true;
				if (drawBottom && y < BOARD_HEIGHT - 1 && !board[x][y + 1].hasWall) drawBottomVertical = true;
				if (drawLeft && x > 0 && !board[x - 1][y].hasWall) drawLeftVertical = true;
				if (drawRight && x < BOARD_WIDTH - 1 && !board[x + 1][y].hasWall) drawRightVertical = true;
			}

			ofPushMatrix();
			ofTranslate(worldPos.x, height, worldPos.z);
			ofRotateXDeg(90);

			float halfSize = TILE_SIZE * 0.5f;

			// Draw only the edges that border non-highlighted tiles
			if (drawTop) {
				ofDrawLine(-halfSize, -halfSize, halfSize, -halfSize);
			}
			if (drawBottom) {
				ofDrawLine(-halfSize, halfSize, halfSize, halfSize);
			}
			if (drawLeft) {
				ofDrawLine(-halfSize, -halfSize, -halfSize, halfSize);
			}
			if (drawRight) {
				ofDrawLine(halfSize, -halfSize, halfSize, halfSize);
			}

			ofPopMatrix();

			// Draw vertical edges on wall sides that meet non-wall tiles
			if (board[x][y].hasWall) {
				float wallHeight = TILE_SIZE * 0.5f;
				float floorY = surfaceY + 0.01f;
				float wallTopY = wallHeight + 0.06f;

				if (drawTopVertical) {
					// North face vertical edges
					ofDrawLine(worldPos.x - halfSize, floorY, worldPos.z - halfSize, worldPos.x - halfSize, wallTopY, worldPos.z - halfSize);
					ofDrawLine(worldPos.x + halfSize, floorY, worldPos.z - halfSize, worldPos.x + halfSize, wallTopY, worldPos.z - halfSize);
				}
				if (drawBottomVertical) {
					// South face vertical edges
					ofDrawLine(worldPos.x - halfSize, floorY, worldPos.z + halfSize, worldPos.x - halfSize, wallTopY, worldPos.z + halfSize);
					ofDrawLine(worldPos.x + halfSize, floorY, worldPos.z + halfSize, worldPos.x + halfSize, wallTopY, worldPos.z + halfSize);
				}
				if (drawLeftVertical) {
					// West face vertical edges
					ofDrawLine(worldPos.x - halfSize, floorY, worldPos.z - halfSize, worldPos.x - halfSize, wallTopY, worldPos.z - halfSize);
					ofDrawLine(worldPos.x - halfSize, floorY, worldPos.z + halfSize, worldPos.x - halfSize, wallTopY, worldPos.z + halfSize);
				}
				if (drawRightVertical) {
					// East face vertical edges
					ofDrawLine(worldPos.x + halfSize, floorY, worldPos.z - halfSize, worldPos.x + halfSize, wallTopY, worldPos.z - halfSize);
					ofDrawLine(worldPos.x + halfSize, floorY, worldPos.z + halfSize, worldPos.x + halfSize, wallTopY, worldPos.z + halfSize);
				}
			}
		}
	}

	ofFill();
	ofSetLineWidth(1);
}

//--------------------------------------------------------------
void ofApp::drawOpponentMenu() {
	// Reuse the existing menu draw functions but with opponent's menu state
	// This leverages the UI that's already built instead of duplicating it

	if (opponentMenuType == 1) {
		// Wisdom Boon - temporarily swap state, draw, then restore
		bool wasOpen = isWisdomBoonMenuOpen;
		int savedCardIdx = pendingWisdomBoonCardIndex;
		int savedTargetIdx = pendingWisdomBoonTargetIndex;

		isWisdomBoonMenuOpen = true;
		pendingWisdomBoonCardIndex = opponentMenuCardIndex;
		pendingWisdomBoonTargetIndex = opponentMenuTargetIndex;

		// Draw with semi-transparent overlay to indicate it's opponent's
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(0, 0, 0, 100); // Light overlay
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
		drawWisdomBoonUI();

		// Restore state
		isWisdomBoonMenuOpen = wasOpen;
		pendingWisdomBoonCardIndex = savedCardIdx;
		pendingWisdomBoonTargetIndex = savedTargetIdx;
	} else if (opponentMenuType == 2) {
		// Burst of Light
		bool wasOpen = isBurstMenuOpen;
		int savedCardIdx = pendingBurstCardIndex;

		isBurstMenuOpen = true;
		pendingBurstCardIndex = opponentMenuCardIndex;

		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(0, 0, 0, 100);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
		drawBurstUI();

		isBurstMenuOpen = wasOpen;
		pendingBurstCardIndex = savedCardIdx;
	} else if (opponentMenuType == 3) {
		// Double Handed
		bool wasOpen = isDoubleHandedMenuOpen;
		int savedCardIdx = pendingDoubleHandedCardIndex;
		int savedTargetIdx = pendingDoubleHandedTargetIndex;

		isDoubleHandedMenuOpen = true;
		pendingDoubleHandedCardIndex = opponentMenuCardIndex;
		pendingDoubleHandedTargetIndex = opponentMenuTargetIndex;

		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(0, 0, 0, 100);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
		drawDoubleHandedUI();

		isDoubleHandedMenuOpen = wasOpen;
		pendingDoubleHandedCardIndex = savedCardIdx;
		pendingDoubleHandedTargetIndex = savedTargetIdx;
	}
}

// Helper: member equivalent of the local applyDamage lambda used in playCard
bool ofApp::applyDamageTo(Player & target, int damage, DamageType type, int attackerIndex) {
	string typeLabel = "";
	switch (type) {
	case DAMAGE_PHYSICAL:
		typeLabel = " Physical";
		break;
	case DAMAGE_PIERCING:
		typeLabel = " Piercing";
		break;
	case DAMAGE_MAGIC:
		typeLabel = " Magic";
		break;
	case DAMAGE_ELECTRIC:
		typeLabel = " Electric";
		break;
	case DAMAGE_FIRE:
		typeLabel = " Fire";
		break;
	case DAMAGE_POISON:
		typeLabel = " Poison";
		break;
	case DAMAGE_HOLY:
		typeLabel = " Holy";
		break;
	}

	int calculatedDamage = damage;

	// Ghost Form
	if (target.inGhostForm) {
		if (type == DAMAGE_PHYSICAL || type == DAMAGE_PIERCING) {
			calculatedDamage = 0;
			spawnFloatingText(gridToWorld(target.x, target.y), "Phased!", ofColor::cyan);
		}
		if (type == DAMAGE_HOLY) {
			calculatedDamage *= 2;
			spawnFloatingText(gridToWorld(target.x, target.y), "Ghost: x2 Holy", ofColor::orange);
		}
	}

	auto isAdjacentOrDiagonalToMagicWall = [&](int x, int y) {
		if (board[x][y].hasWall && board[x][y].isMagicWall) return true;
		for (int dx = -1; dx <= 1; ++dx)
			for (int dy = -1; dy <= 1; ++dy) {
				if (dx == 0 && dy == 0) continue;
				int nx = x + dx, ny = y + dy;
				if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
					if (board[nx][ny].hasWall && board[nx][ny].isMagicWall) return true;
				}
			}
		return false;
	};

	int wallEffectCount = 0;
	Player * attackerPtr = nullptr;
	if (attackerIndex >= 0 && attackerIndex < (int)players.size())
		attackerPtr = &players[attackerIndex];
	else if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size())
		attackerPtr = &players[currentPlayerIndex];
	bool targetNearWall = isAdjacentOrDiagonalToMagicWall(target.x, target.y);
	bool attackerNearWall = false;
	if (attackerPtr && attackerPtr != &target) attackerNearWall = isAdjacentOrDiagonalToMagicWall(attackerPtr->x, attackerPtr->y);
	wallEffectCount = (targetNearWall ? 1 : 0) + (attackerNearWall ? 1 : 0);

	if (type == DAMAGE_MAGIC && wallEffectCount > 0) {
		calculatedDamage *= (1 << wallEffectCount);
		for (int i = 0; i < wallEffectCount; i++)
			spawnFloatingText(gridToWorld(target.x, target.y), "Magic Wall: x2 Magic", ofColor::purple);
	} else if (type == DAMAGE_PHYSICAL && wallEffectCount > 0) {
		for (int i = 0; i < wallEffectCount; i++) {
			calculatedDamage /= 2;
			if (i == 0 && targetNearWall)
				spawnFloatingText(gridToWorld(target.x, target.y), "Magic Wall: 1/2 Phys", ofColor::purple);
			else if (i == 1 && attackerNearWall)
				spawnFloatingText(gridToWorld(attackerPtr->x, attackerPtr->y), "Magic Wall: 1/2 Phys", ofColor::purple);
		}
	}

	if ((target.isHellhound || target.isDemon || target.isSkeleton) && type == DAMAGE_HOLY) {
		calculatedDamage *= 2;
		spawnFloatingText(gridToWorld(target.x, target.y), "Vulnerable: Holy (x2)", ofColor::orange);
	}
	if (type == DAMAGE_HOLY) {
		bool hasVampireBite = false;
		for (const auto & c : target.deck)
			if (c.type == CARD_VAMPIRE_BITE) {
				hasVampireBite = true;
				break;
			}
		if (!hasVampireBite)
			for (const auto & c : target.discardPile)
				if (c.type == CARD_VAMPIRE_BITE) {
					hasVampireBite = true;
					break;
				}
		if (hasVampireBite) {
			calculatedDamage *= 2;
			spawnFloatingText(gridToWorld(target.x, target.y), "Vampire Curse: x2 Holy", ofColor::orange);
		}
	}
	if (type == DAMAGE_PIERCING) {
		bool hasWolfCall = false;
		for (const auto & c : target.deck)
			if (c.type == CARD_CALL_FOR_WOLVES) {
				hasWolfCall = true;
				break;
			}
		if (!hasWolfCall)
			for (const auto & c : target.discardPile)
				if (c.type == CARD_CALL_FOR_WOLVES) {
					hasWolfCall = true;
					break;
				}
		if (hasWolfCall) {
			calculatedDamage *= 2;
			spawnFloatingText(gridToWorld(target.x, target.y), "Vulnerable: Piercing (x2)", ofColor::orange);
		}
	}

	ofLogNotice("Game") << "Dealing " << calculatedDamage << " damage to Player " << target.playerID;

	int initialHealth = target.health;
	int remainingDmg = calculatedDamage;

	// Absorb damage from the most-specific block types first.
	// Order is chosen per `DamageType` to prefer specific buffers:
	// - Holy: holyBlock -> barrier -> ward
	// - Physical: block -> fortification -> ward
	// - Piercing: fortification -> ward
	// - Other non-physical (magic, electric, fire, poison): barrier -> ward
	auto absorbFrom = [&](int & source, int & remaining, const char * name) {
		int a = std::min(source, remaining);
		source -= a;
		remaining -= a;
		if (a > 0) ofLogNotice("Game") << name << " absorbed " << a;
	};

	switch (type) {
	case DAMAGE_HOLY:
		absorbFrom(target.holyBlock, remainingDmg, "Holy Block");
		absorbFrom(target.barrier, remainingDmg, "Barrier");
		absorbFrom(target.ward, remainingDmg, "Ward");
		break;

	case DAMAGE_PHYSICAL:
		absorbFrom(target.block, remainingDmg, "Block");
		absorbFrom(target.fortification, remainingDmg, "Fortification");
		absorbFrom(target.ward, remainingDmg, "Ward");
		break;

	case DAMAGE_PIERCING:
		// Piercing bypasses normal `block`, but is reduced by `fortification`.
		absorbFrom(target.fortification, remainingDmg, "Fortification");
		absorbFrom(target.ward, remainingDmg, "Ward");
		break;

	default:
		// MAGIC, ELECTRIC, FIRE, POISON and others
		absorbFrom(target.barrier, remainingDmg, "Barrier");
		absorbFrom(target.ward, remainingDmg, "Ward");
		break;
	}

	glm::vec3 targetPos = gridToWorld(target.x, target.y);
	if (remainingDmg > 0) {
		target.health -= remainingDmg;
		spawnFloatingText(targetPos, "-" + ofToString(remainingDmg) + typeLabel, ofColor::red);
		if (target.inTortoiseForm) {
			target.tortoiseDamageTaken += remainingDmg;
			if (target.tortoiseDamageTaken >= 5) {
				target.inTortoiseForm = false;
				target.tortoiseDamageTaken = 0;
				target.discardPile.push_back(target.tortoiseFormCard);
				spawnFloatingText(targetPos + glm::vec3(0, 0.5f, 0), "Form Ended!", ofColor::darkGreen);
			}
		}
		if (target.inGhostForm) {
			target.ghostDamageTaken += remainingDmg;
			if (target.ghostDamageTaken >= 4) {
				target.inGhostForm = false;
				target.ghostDamageTaken = 0;
				target.discardPile.push_back(target.ghostFormCard);
				spawnFloatingText(targetPos + glm::vec3(0, 0.5f, 0), "Ghost Form Broken!", ofColor::white);
				if (board[target.x][target.y].hasWall) {
					target.health = 0;
					spawnFloatingText(targetPos + glm::vec3(0, 1.0f, 0), "Materialized in Wall!", ofColor::red);
				}
			}
		}
	} else {
		spawnFloatingText(targetPos, "Blocked", ofColor::gray);
	}

	if (target.health <= 0) {
		ofLogNotice("Game") << "Player " << target.playerID << " defeated!";

		// --- NEW: DEMON KILL REWARD ---
		if (target.isDemon && attackerIndex != -1) {
			// Attacker drafts a Class 3 Card
			isInGameDraft = true;
			draftPlayerIndex = attackerIndex;
			generateDraftOptions(3); // Class 3
			draftPicksRemaining = 1;
			selectedDraftIndices.clear();
			draftStage = 0;

			// Switch state immediately
			currentState = STATE_DRAFTING;

			// Visual feedback
			Player * attacker = getPlayer(attackerIndex);
			if (attacker) {
				spawnFloatingText(gridToWorld(attacker->x, attacker->y), "Demon Slayer!", ofColor::gold);
			}
		}
		// -----------------------------

		DeathMarker death;
		death.x = target.x;
		death.y = target.y;
		death.turnDied = globalTurnCounter;
		death.deck = target.deck;
		graveyard.push_back(death);
		board[target.x][target.y].hasPlayer = false;
		target.x = -1000;
	}

	return target.health < initialHealth;
}
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

	ofRectangle panelRect(panelX, panelY, panelWidth, panelHeight);

	// Title/desc strings
	string prompt = "Player " + ofToString(targetPlayer->playerID) + ", choose an effect:";
	string choicesLeft = "Choices remaining: " + ofToString(magicBlastChoicesRemaining);

	// Use standardized panel helper (Damage = red, Discard = slate blue)
	drawCardChoicePanel(panelRect, prompt, choicesLeft, magicBlastDamageButton, magicBlastDiscardButton,
		"Take 5 Damage", "Remove Top Card of Deck", ofColor::indianRed, ofColor::darkSlateBlue, true, true);
}

//------------------------------------------------------------------------
// Standardized card-choice panel helper
// Draws a panel and one or two buttons with consistent styling.
void ofApp::drawCardChoicePanel(const ofRectangle & panelRect,
	const std::string & title,
	const std::string & desc,
	ofRectangle & primaryRect,
	ofRectangle & secondaryRect,
	const std::string & primaryLabel,
	const std::string & secondaryLabel,
	ofColor primaryAccent,
	ofColor secondaryAccent,
	bool primaryEnabled,
	bool secondaryEnabled) {
	float pad = 24;
	float titleY = panelRect.y + 48;
	float descY = panelRect.y + 88;
	float btnH = 80;
	float spacing = 24;
	// Compute button layout: if no secondary label, center primary
	if (secondaryLabel.empty()) {
		float btnW = std::min(420.0f, panelRect.width - pad * 2);
		primaryRect.set(panelRect.getCenter().x - btnW / 2, panelRect.y + panelRect.getHeight() - pad - btnH, btnW, btnH);
	} else {
		float availableW = panelRect.width - pad * 2 - spacing;
		float btnW = std::min(420.0f, availableW / 2.0f);
		primaryRect.set(panelRect.x + pad, panelRect.y + panelRect.getHeight() - pad - btnH, btnW, btnH);
		secondaryRect.set(panelRect.x + pad + btnW + spacing, panelRect.y + panelRect.getHeight() - pad - btnH, btnW, btnH);
	}

	// Panel background (caller is expected to draw overlay if desired)
	ofSetColor(30, 30, 40, 240);
	ofDrawRectRounded(panelRect, 12);

	// Title
	ofSetColor(ofColor::white);
	ofRectangle titleBox = uiFont.getStringBoundingBox(title, 0, 0);
	uiFont.drawString(title, panelRect.getCenter().x - titleBox.getWidth() / 2, titleY);

	// Description
	if (!desc.empty()) {
		ofSetColor(ofColor::white);
		ofRectangle descBox = uiFont.getStringBoundingBox(desc, 0, 0);
		uiFont.drawString(desc, panelRect.getCenter().x - descBox.getWidth() / 2, descY);
	}

	// Primary Button
	if (primaryEnabled)
		ofSetColor(primaryAccent);
	else
		ofSetColor(90, 90, 90);
	ofDrawRectRounded(primaryRect, 10);
	ofSetColor((primaryEnabled && primaryAccent.getBrightness() > 200) ? ofColor::black : ofColor::white);
	ofRectangle pBox = uiFont.getStringBoundingBox(primaryLabel, 0, 0);
	uiFont.drawString(primaryLabel, primaryRect.getCenter().x - pBox.getWidth() / 2, primaryRect.getCenter().y + pBox.getHeight() / 2);

	// Secondary Button (if any)
	if (!secondaryLabel.empty()) {
		if (secondaryEnabled)
			ofSetColor(secondaryAccent);
		else
			ofSetColor(90, 90, 90);
		ofDrawRectRounded(secondaryRect, 10);
		ofSetColor((secondaryEnabled && secondaryAccent.getBrightness() > 200) ? ofColor::black : ofColor::white);
		ofRectangle sBox = uiFont.getStringBoundingBox(secondaryLabel, 0, 0);
		uiFont.drawString(secondaryLabel, secondaryRect.getCenter().x - sBox.getWidth() / 2, secondaryRect.getCenter().y + sBox.getHeight() / 2);
	}
}
//--------------------------------------------------------------
// --- NEW HELPER: Converts a specific point in world space back to grid coordinates ---
glm::vec2 ofApp::worldToGrid(glm::vec3 worldPos) {
	float gridX = (worldPos.x / TILE_SIZE) + (BOARD_WIDTH / 2.0f);
	float gridY = (worldPos.z / TILE_SIZE) + (BOARD_HEIGHT / 2.0f);
	return glm::vec2(floor(gridX), floor(gridY));
}

//--------------------------------------------------------------
// Returns true if the ray is clear, false if blocked
bool ofApp::checkRayPhysics(glm::vec2 rayStart, glm::vec2 rayEnd) {
	// 1. Get the list of tiles the ray passes through
	auto path = getLineOfSightPath(rayStart, rayEnd);
	if (path.empty()) return true;

	// 2. Iterate through the path
	for (size_t i = 0; i < path.size(); ++i) {
		glm::vec2 current = path[i];
		int cx = (int)current.x;
		int cy = (int)current.y;

		// SKIP start and end tiles (we don't block visibility based on where we stand or who we target)
		bool isStart = (cx == (int)rayStart.x && cy == (int)rayStart.y);
		bool isEnd = (cx == (int)rayEnd.x && cy == (int)rayEnd.y);

		// --- DIRECT BLOCKING ---
		// If the tile itself contains a Wall or a Unit (and isn't start/end), it blocks.
		if (!isStart && !isEnd) {
			if (isTileBlocked(cx, cy)) return false;
		}

		// --- DIAGONAL BARRIER (PINCH) CHECK ---
		// If we step diagonally, check if we are squeezing through two obstacles.
		if (i < path.size() - 1) {
			glm::vec2 next = path[i + 1];
			int nx = (int)next.x;
			int ny = (int)next.y;

			// Check if movement is diagonal
			if (cx != nx && cy != ny) {
				// Determine the two shared neighbors
				// e.g., moving (0,0) to (1,1), neighbors are (1,0) and (0,1)
				int n1x = nx;
				int n1y = cy;

				int n2x = cx;
				int n2y = ny;

				// RULE: If BOTH orthogonal neighbors are blocked, the diagonal gap is closed.
				// "if they are diagonal to another wall / unit... they form a barrier"
				bool block1 = isTileBlocked(n1x, n1y);
				bool block2 = isTileBlocked(n2x, n2y);

				if (block1 && block2) {
					return false; // Ray is pinched
				}
			}
		}
	}
	return true;
}

// ----------------- FIXED isLosTargetValid (With Ethereal Jolt Support) -----------------
TargetInfo ofApp::isLosTargetValid(glm::vec2 casterTile, glm::vec2 targetTile, float maxRangeFeet, CardType cardType) {
	TargetInfo result;
	result.reason = VALID;

	// --- 0. BASIC SANITY CHECKS ---
	if (casterTile == targetTile) {
		result.reason = INVALID_SELF;
		return result;
	}

	// --- CHECK IF TARGET IS A WALL ---
	if (isTileWall((int)targetTile.x, (int)targetTile.y)) {

		// EXCEPTION: If the card can target through walls (Jolt/Bolt/Wave/Death)
		// AND there is a player inside that wall (Ghost), allow it.
		bool allowWallTarget = false;
		if (cardType == CARD_ETHEREAL_JOLT || cardType == CARD_MAGIC_BOLT || cardType == CARD_PSIONIC_WAVE || cardType == CARD_DEATH) {
			if (board[(int)targetTile.x][(int)targetTile.y].hasPlayer) {
				allowWallTarget = true;
			}
		}

		if (!allowWallTarget) {
			result.reason = INVALID_OCCUPIED_BY_WALL;
			return result;
		}
	}

	// --- 1. DETERMINE FIRING ORIGINS (VISIBILITY) ---
	std::vector<glm::vec2> firingOrigins;
	glm::vec2 casterCenter = casterTile + 0.5f;

	// Neighbors: East, West, South, North
	glm::vec2 neighbors[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

	// --- GHOST FORM LOGIC START ---
	// Check if the caster is currently inside a wall (Ghost scenario)
	if (isTileWall((int)casterTile.x, (int)casterTile.y)) {
		// If inside a wall, we assume we can shoot out from the center
		// (Ghosts phase through their own cover)
		firingOrigins.push_back(casterCenter);
	} else {
		// --- STANDARD PEEKING LOGIC ---
		bool adjacentToWall = false;
		for (auto n : neighbors) {
			int nx = (int)casterTile.x + (int)n.x;
			int ny = (int)casterTile.y + (int)n.y;
			if (isTileWall(nx, ny)) {
				adjacentToWall = true;
				break;
			}
		}

		if (!adjacentToWall) {
			// Standard: Shoot from Center
			firingOrigins.push_back(casterCenter);
		} else {
			// Peeking: Shoot from centers of faces NOT blocked by walls
			glm::vec2 faceOffsets[] = { { 0.5f, 0 }, { -0.5f, 0 }, { 0, 0.5f }, { 0, -0.5f } };

			for (int i = 0; i < 4; i++) {
				int nx = (int)casterTile.x + (int)neighbors[i].x;
				int ny = (int)casterTile.y + (int)neighbors[i].y;

				// If this face is not pressed against a wall, we can shoot from it
				if (!isTileWall(nx, ny)) {
					firingOrigins.push_back(casterCenter + faceOffsets[i]);
				}
			}
		}
	}
	// --- GHOST FORM LOGIC END ---

	// --- 2. CHECK VISIBILITY (Raycast to Target Center) ---
	bool hasLineOfSight = false;
	glm::vec2 targetCenter = targetTile + 0.5f;

	// FIX: Magic Bolt and Ethereal Jolt ignore walls for visibility
	if (cardType == CARD_ETHEREAL_JOLT || cardType == CARD_MAGIC_BOLT) {
		hasLineOfSight = true;
	} else {
		for (const auto & origin : firingOrigins) {
			if (checkRayPhysics(origin, targetCenter)) {
				hasLineOfSight = true;
				break;
			}
		}
	}

	if (!hasLineOfSight) {
		result.reason = INVALID_NO_LOS;
		return result;
	}

	// --- 3. CHECK RANGE ---
	float distFeet = getFaceToFaceDistance(casterTile, targetTile) * 5.0f;

	// Magic Bolt uses direct Euclidean center-to-center for range check logic
	if (cardType == CARD_MAGIC_BOLT) {
		distFeet = glm::distance(casterTile, targetTile) * 5.0f;
	}

	if (distFeet > maxRangeFeet + 0.05f) {
		result.reason = INVALID_OUT_OF_RANGE;
		return result;
	}

	// --- 4. TARGET VALIDATION ---
	bool isOccupied = board[(int)targetTile.x][(int)targetTile.y].hasPlayer;

	if (cardType == CARD_MAGIC_BOLT) {
		// Magic Bolt: Can hit unit OR ground if it can splash a nearby unit
		if (isOccupied) {
			result.isTargetable = true;
		} else {
			bool hasNeighbor = false;
			for (auto n : neighbors) {
				int nx = (int)targetTile.x + (int)n.x;
				int ny = (int)targetTile.y + (int)n.y;
				if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && board[nx][ny].hasPlayer) {
					hasNeighbor = true;
					break;
				}
			}
			result.isTargetable = hasNeighbor;
		}
	} else if (cardType == CARD_HEAL || cardType == CARD_LESSER_HEAL) {
		result.isTargetable = isOccupied;
	} else {
		// Fireball / Attacks: Must target unit
		result.isTargetable = isOccupied;
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
	// Both Walls AND Players block Line of Sight
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
	// 1. Get Centers
	glm::vec2 cCenter = casterTile + 0.5f;
	glm::vec2 tCenter = targetTile + 0.5f;

	// 2. Define offsets from center to face midpoints
	glm::vec2 faceOffsets[] = {
		{ 0.5f, 0.0f }, // East
		{ -0.5f, 0.0f }, // West
		{ 0.0f, 0.5f }, // South
		{ 0.0f, -0.5f } // North
	};

	float shortestDist = std::numeric_limits<float>::max();

	// Loop through all 4 faces of the Caster
	for (int i = 0; i < 4; i++) {
		glm::vec2 cFace = cCenter + faceOffsets[i];
		for (int j = 0; j < 4; j++) {
			glm::vec2 tFace = tCenter + faceOffsets[j];

			float d = glm::distance(cFace, tFace);
			if (d < shortestDist) shortestDist = d;
		}
	}

	// Adjacent tiles share a face -> distance 0
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
	activePlayedCardAnimations.clear();
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
	animatingPlayerIndex = -1;
	isLoadingGame = false;
	hasReceivedHandshake = false;
	waitingForTurnStartFromHost = false;

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
	class1Cards.clear();
	class2Cards.clear();
	class3Cards.clear();

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

		// Parse Class (Default to 1 if missing)
		newCard.cardClass = cardJson.value("class", 1);

		// Calculate texture coordinates from the sprite sheet based on ID
		int index = cardId - 1;
		int row = index / numCols;
		int col = index % numCols;
		newCard.textureRect = ofRectangle(col * cardPixelWidth, row * cardPixelHeight, cardPixelWidth, cardPixelHeight);

		allCards.push_back(newCard);

		// Sort into Class Buckets
		if (newCard.cardClass == 1)
			class1Cards.push_back(newCard);
		else if (newCard.cardClass == 2)
			class2Cards.push_back(newCard);
		else if (newCard.cardClass == 3)
			class3Cards.push_back(newCard);
	}
	ofLogNotice("ofApp::loadCardData") << "Loaded " << allCards.size() << " cards from JSON.";
	ofLogNotice("ofApp::loadCardData") << "Class Distribution - C1: " << class1Cards.size() << ", C2: " << class2Cards.size() << ", C3: " << class3Cards.size();
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
	if (str == "CARD_DEMOLITION") return CARD_DEMOLITION;
	if (str == "CARD_DISPEL") return CARD_DISPEL;
	if (str == "CARD_TELEPORT") return CARD_TELEPORT;
	if (str == "CARD_HASTEN") return CARD_HASTEN;
	if (str == "CARD_REPLICATE") return CARD_REPLICATE;
	if (str == "CARD_WISDOM_BOON") return CARD_WISDOM_BOON;
	if (str == "CARD_ETHEREAL_JOLT") return CARD_ETHEREAL_JOLT;
	if (str == "CARD_FLAME_HIT") return CARD_FLAME_HIT;
	if (str == "CARD_HEAL") return CARD_HEAL;
	if (str == "CARD_RAISE_DEAD") return CARD_RAISE_DEAD;
	if (str == "CARD_SUMMON_GOLEM") return CARD_SUMMON_GOLEM;
	if (str == "CARD_STRENGTHEN_ELEMENTS") return CARD_STRENGTHEN_ELEMENTS;
	if (str == "CARD_CREATE_WALL") return CARD_CREATE_WALL;
	if (str == "CARD_FORTIFY") return CARD_FORTIFY;
	if (str == "CARD_VAMPIRE_BITE") return CARD_VAMPIRE_BITE;
	if (str == "CARD_DARK_SHIELD") return CARD_DARK_SHIELD;
	if (str == "CARD_DRAIN_PUNCH") return CARD_DRAIN_PUNCH;
	if (str == "CARD_DOUBLE_HANDED") return CARD_DOUBLE_HANDED;
	if (str == "CARD_CALL_FOR_WOLVES") return CARD_CALL_FOR_WOLVES;
	if (str == "CARD_CALL_FOR_KOBOLDS") return CARD_CALL_FOR_KOBOLDS;
	if (str == "CARD_NECRO_BLESSING") return CARD_NECRO_BLESSING;
	if (str == "CARD_TIME_VORTEX") return CARD_TIME_VORTEX;
	if (str == "CARD_MASTER_FIST") return CARD_MASTER_FIST;
	if (str == "CARD_MAGIC_BOLT") return CARD_MAGIC_BOLT;
	if (str == "CARD_FLAIL") return CARD_FLAIL;
	if (str == "CARD_SUMMON_HELLHOUND") return CARD_SUMMON_HELLHOUND;
	if (str == "CARD_DEATH") return CARD_DEATH;
	if (str == "CARD_SUMMON_DEMON") return CARD_SUMMON_DEMON;
	if (str == "CARD_SHIELD_BASH") return CARD_SHIELD_BASH;
	if (str == "CARD_CHAIN_LIGHTNING") return CARD_CHAIN_LIGHTNING;
	if (str == "CARD_CONSUME_HEALTH_POTION") return CARD_CONSUME_HEALTH_POTION;
	if (str == "CARD_ADD_POISON") return CARD_ADD_POISON;
	if (str == "CARD_FLURRY_OF_FISTS") return CARD_FLURRY_OF_FISTS;
	if (str == "CARD_FORM_OF_TORTOISE") return CARD_FORM_OF_TORTOISE;
	if (str == "CARD_RENEWED_INSPIRATION") return CARD_RENEWED_INSPIRATION;
	if (str == "CARD_SPARK_OF_GENIUS") return CARD_SPARK_OF_GENIUS;
	if (str == "CARD_PSIONIC_WAVE") return CARD_PSIONIC_WAVE;
	if (str == "CARD_EARTHQUAKE") return CARD_EARTHQUAKE;
	if (str == "CARD_FORM_OF_GHOST") return CARD_FORM_OF_GHOST;
	if (str == "CARD_GIANT_MAGIC_HAND") return CARD_GIANT_MAGIC_HAND;
	if (str == "CARD_CONSUME_LARGE_HEALTH_POTION") return CARD_CONSUME_LARGE_HEALTH_POTION;
	if (str == "CARD_LESSER_HEAL") return CARD_LESSER_HEAL;
	if (str == "CARD_TRANSFORM_WALL") return CARD_TRANSFORM_WALL;
	if (str == "CARD_SUMMON_KOBOLD_KING") return CARD_SUMMON_KOBOLD_KING; // Add
	if (str == "CARD_SUMMON_ASSISTANT") return CARD_SUMMON_ASSISTANT;
	if (str == "CARD_SUMMON_FAERIE") return CARD_SUMMON_FAERIE;
	if (str == "CARD_FOUR_LEAF_CLOVER") return CARD_FOUR_LEAF_CLOVER;
	if (str == "CARD_SPRINT") return CARD_SPRINT;
	if (str == "CARD_SMITE") return CARD_SMITE;
	if (str == "CARD_BURST_OF_LIGHT") return CARD_BURST_OF_LIGHT;
	if (str == "CARD_SHOOT_ARROW") return CARD_SHOOT_ARROW;
	if (str == "CARD_FULL_RESTORE") return CARD_FULL_RESTORE;
	if (str == "CARD_TRAIN") return CARD_TRAIN;
	if (str == "CARD_STUDY") return CARD_STUDY;
	if (str == "CARD_BLOCKING_BOON") return CARD_BLOCKING_BOON;
	if (str == "CARD_CONSTITUTION_BOON") return CARD_CONSTITUTION_BOON;

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
	if (str == "TARGET_EMPTY_TILE") return TARGET_EMPTY_TILE;
	if (str == "TARGET_ADJACENT_WALL") return TARGET_ADJACENT_WALL;
	return TARGET_NONE;
}

DamageType ofApp::stringToDamageType(const std::string & str) {
	if (str == "DAMAGE_PIERCING") return DAMAGE_PIERCING;
	if (str == "DAMAGE_MAGIC") return DAMAGE_MAGIC;
	if (str == "DAMAGE_FIRE") return DAMAGE_FIRE;
	if (str == "DAMAGE_ELECTRIC") return DAMAGE_ELECTRIC;
	if (str == "DAMAGE_HOLY") return DAMAGE_HOLY;
	if (str == "DAMAGE_POISON") return DAMAGE_POISON;
	return DAMAGE_PHYSICAL;
}

//--------------------------------------------------------------
void ofApp::drawMinionStatusBars(Player & minion, const std::string & name, float x, float y, float totalWidth) {
	float scale = ofGetHeight() / 1080.0f;
	float fontScale = 0.9f;

	// 1. Main Stats Bar
	float barHeight = 20 * scale;
	ofRectangle nameBounds = uiFont.getStringBoundingBox(name, 0, 0);
	float barY = y + (nameBounds.height * fontScale) + (4 * scale);

	float statW = totalWidth * 0.20f;
	float usedWidth = 0;

	// --- FIX: Add holyBlock to width calculation ---
	if (minion.block > 0) usedWidth += statW;
	if (minion.fortification > 0) usedWidth += statW;
	if (minion.barrier > 0) usedWidth += statW;
	if (minion.holyBlock > 0) usedWidth += statW; // <--- ADDED
	if (minion.ward > 0) usedWidth += statW;

	float hpW = totalWidth - usedWidth;
	float currentX = x;

	// --- HEALTH ---
	ofSetColor(40, 0, 0);
	ofDrawRectangle(currentX, barY, hpW, barHeight);
	float hpPct = (float)minion.health / minion.maxHealth;
	ofSetColor(ofColor::green);
	ofDrawRectangle(currentX, barY, hpW * hpPct, barHeight);
	string hpText = ofToString(minion.health) + "/" + ofToString(minion.maxHealth);
	drawStatText(uiFont, hpText, currentX, barY, hpW, barHeight, ofColor::white);
	currentX += hpW;

	// --- SHIELDS (With Tooltips) ---
	auto drawMinionSeg = [&](int val, ofColor c, string label) {
		if (val > 0) {
			ofSetColor(c);
			ofDrawRectangle(currentX, barY, statW, barHeight);
			drawStatText(uiFont, ofToString(val), currentX, barY, statW, barHeight, (c.getBrightness() > 200 ? ofColor::black : ofColor::white));

			// Tooltip Check
			if (ofRectangle(currentX, barY, statW, barHeight).inside(ofGetMouseX(), ofGetMouseY())) {
				isShowingTooltip = true;
				tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
				tooltipText = label;
			}
			currentX += statW;
		}
	};

	drawMinionSeg(minion.block, ofColor::gray, "Block (Physical)");
	drawMinionSeg(minion.fortification, ofColor(50, 50, 50), "Fortification (Phys/Pierce)");
	drawMinionSeg(minion.barrier, ofColor::hotPink, "Barrier (Non-Physical)");

	// --- FIX: Add Holy Block Drawing ---
	drawMinionSeg(minion.holyBlock, ofColor::yellow, "Holy Block (Holy)"); // <--- ADDED

	drawMinionSeg(minion.ward, ofColor::black, "Ward (All Damage)");

	// --- FORM BARS ---
	if (minion.inTortoiseForm || minion.inGhostForm) {
		float formY = barY + barHeight + (2 * scale);
		float formHeight = 15 * scale;

		if (minion.inTortoiseForm) {
			int rem = 5 - minion.tortoiseDamageTaken;
			ofSetColor(20, 40, 20);
			ofDrawRectangle(x, formY, totalWidth, formHeight);
			ofSetColor(ofColor::darkGreen);
			ofDrawRectangle(x, formY, totalWidth * (rem / 5.0f), formHeight);
			drawStatText(uiFont, "Tortoise: " + ofToString(rem) + "/5", x, formY, totalWidth, formHeight, ofColor::white);

			// Tooltip
			if (ofRectangle(x, formY, totalWidth, formHeight).inside(ofGetMouseX(), ofGetMouseY())) {
				isShowingTooltip = true;
				tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
				tooltipText = "Tortoise Form: Buffer HP";
			}

			// Stack next bar if needed
			formY += formHeight + (2 * scale);
		}

		if (minion.inGhostForm) {
			int rem = 4 - minion.ghostDamageTaken;
			ofSetColor(30, 30, 50);
			ofDrawRectangle(x, formY, totalWidth, formHeight);
			ofSetColor(150, 150, 255);
			ofDrawRectangle(x, formY, totalWidth * (rem / 4.0f), formHeight);
			drawStatText(uiFont, "Ghost: " + ofToString(rem) + "/4", x, formY, totalWidth, formHeight, ofColor::black);

			// Tooltip
			if (ofRectangle(x, formY, totalWidth, formHeight).inside(ofGetMouseX(), ofGetMouseY())) {
				isShowingTooltip = true;
				tooltipPos = { (float)ofGetMouseX(), (float)ofGetMouseY() };
				tooltipText = "Ghost Form: Immune to Physical/Piercing";
			}
		}
	}
}
//--------------------------------------------------------------
void ofApp::drawMinionManagerUI() {
	if (activeMinionUIs.empty()) return;

	float scale = ofGetHeight() / 1080.0f;

	for (size_t i = 0; i < activeMinionUIs.size(); i++) {
		auto & ui = activeMinionUIs[i];
		Player & minion = players[ui.playerIndex];

		// --- Render Model to FBO ---
		modelFbo.begin();
		ofClear(0, 0, 0, 0);
		ofEnableDepthTest();
		ofDisableLighting();
		uiLight.disable();
		ofSetColor(255);

		ofPushMatrix();

		// --- TORTOISE FORM PREVIEW (overrides normal model) ---
		if (minion.inTortoiseForm) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 10);
			ofScale(21, -21, 21);
			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);
			if (tortoiseTexture.isAllocated()) tortoiseTexture.bind();
			tortoiseModel.drawFaces();
			if (tortoiseTexture.isAllocated()) tortoiseTexture.unbind();
		} else if (minion.isGolem) {
			// GOLEM: Raised position (100 -> 80)
			ofTranslate(modelFbo.getWidth() / 2, 80);
			ofScale(27, 27, 27);
			ofRotateXDeg(-15);
			ofRotateYDeg(ofGetElapsedTimef() * 30);
			if (minion.minionTexture) minion.minionTexture->bind();
			golemModel.drawFaces();
			if (minion.minionTexture) minion.minionTexture->unbind();

		} else if (minion.isWolf) {
			// WOLF: Decreased scale by 50% (2.2 -> 1.1), Lowered position (+10 -> +30)
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 30);
			ofScale(1.1f, -1.1f, 1.1f);

			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);

			// Draw skin meshes
			for (unsigned int i = 6; i < wolfModel.getMeshCount(); i++) {
				ofTexture * tex = (i == 6 || i == 7) ? &wolfBodyTex : &wolfFaceTex;
				if (tex->isAllocated()) tex->bind();
				wolfModel.getMeshHelper(i).cachedMesh.drawFaces();
				if (tex->isAllocated()) tex->unbind();
			}

			// Draw fur
			glDepthMask(GL_FALSE);
			ofEnableAlphaBlending();
			wolfFurTex.bind();
			for (unsigned int i = 0; i <= 5; i++) {
				wolfModel.getMeshHelper(i).cachedMesh.drawFaces();
			}
			wolfFurTex.unbind();
			ofDisableAlphaBlending();
			glDepthMask(GL_TRUE);

		}
		// --- KOBOLD KING PREVIEW ---
		else if (minion.isKoboldKing) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 30);

			// Reduced from 30.0f to 2.5f (since model is now 0.0042f)
			ofScale(2.5f, -2.5f, 2.5f);

			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);

			// CORRECTION HERE TOO if needed in UI
			ofRotateYDeg(-90);

			ofSetColor(255);
			if (koboldKingTexture.isAllocated()) koboldKingTexture.bind();
			koboldKingModel.drawFaces();
			if (koboldKingTexture.isAllocated()) koboldKingTexture.unbind();
		}
		// --- KOBOLD PREVIEW ---
		else if (minion.isKobold) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 30);
			// Preview scale reduced by ~30%
			ofScale(4.55f, -4.55f, 4.55f);
			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);
			koboldModel.drawFaces();
		}
		// --- HELLHOUND PREVIEW ---
		else if (minion.isHellhound) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 10);

			// HELLHOUND: Increased scale (18 -> 22)
			ofScale(22, -22, 22);

			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);
			hellhoundModel.drawFaces();
		}
		// --- DEMON PREVIEW ---
		else if (minion.isDemon) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 10);
			// DEMON: Increased scale (12 -> 16)
			ofScale(16, -16, 16);
			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);
			demonModel.drawFaces();
		}
		// --- WALL PREVIEW ---
		else if (minion.isWallUnit) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 10);
			// Reasonable preview scale for wall unit (tweakable)
			ofScale(6.0f, -6.0f, 6.0f);
			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);
			if (minion.minionTexture && minion.minionTexture->isAllocated()) minion.minionTexture->bind();
			wallUnitModel.drawFaces();
			if (minion.minionTexture && minion.minionTexture->isAllocated()) minion.minionTexture->unbind();

			// If this wall unit was created from a Magic Wall, draw a mesh-based purple glow
			if (minion.isMagicWallUnit) {
				ofEnableBlendMode(OF_BLENDMODE_ADD);
				ofSetColor(148, 0, 211, 120);
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(-1.0f, -1.0f);
				ofPushMatrix();
				// Draw at the preview model's scale so the glow matches the mesh
				// Draw the whole preview model again in purple so the glow follows model curves exactly
				wallUnitModel.drawFaces();
				ofPopMatrix();
				glDisable(GL_POLYGON_OFFSET_FILL);
				ofSetColor(255);
				ofDisableBlendMode();
			}
		}
		// --- ASSISTANT PREVIEW ---
		else if (minion.isAssistant) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 20);
			ofScale(35.0f, -35.0f, 35.0f);
			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);
			assistantModel.drawFaces();
		}
		// --- FAERIE PREVIEW ---
		else if (minion.isFaerie) {
			ofTranslate(modelFbo.getWidth() / 2, modelFbo.getHeight() / 2 + 20);
			// Faerie is likely small, so scale up slightly more than standard units
			ofScale(30.0f, -30.0f, 30.0f);
			ofRotateXDeg(-15);
			ofRotateYDeg(180 + ofGetElapsedTimef() * 30);
			if (faerieTexture.isAllocated()) faerieTexture.bind();
			faerieModel.drawFaces();
			if (faerieTexture.isAllocated()) faerieTexture.unbind();
		}
		// --- SKELETON PREVIEW --- Default
		else {
			ofSetColor(255);
			ofTranslate(modelFbo.getWidth() / 2, 90);
			// SKELETON: Increased scale by 20% (18 -> 22)
			ofScale(22, -22, 22);
			ofRotateXDeg(-15);
			ofRotateYDeg(ofGetElapsedTimef() * 30);
			skeletonTexture.bind();
			skeletonModel.drawFaces();
			skeletonTexture.unbind();
		}

		ofPopMatrix();
		ofDisableDepthTest();
		modelFbo.end();

		// --- Draw UI Panel ---
		ofSetColor(0, 0, 0, 150);
		ofDrawRectRounded(ui.bounds, 10 * scale);

		// Outline the UI panel when the mouse is hovering over that minion (quick visual mapping)
		if (hoveredUnitIndex == ui.playerIndex) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::green);
			ofSetLineWidth(3 * scale);
			ofDrawRectRounded(ui.bounds, 10 * scale);
			ofPopStyle();
		}

		// --- DETERMINE NAME ---
		string name = "";
		if (minion.isGolem) {
			if (minion.minionTexture == &golemTexElectric)
				name = "Electric Golem ";
			else if (minion.minionTexture == &golemTexFire)
				name = "Fire Golem ";
			else if (minion.minionTexture == &golemTexRock)
				name = "Rock Golem ";
			else
				name = "Golem ";
		} else if (minion.isWolf) {
			name = "Wolf ";
		} else if (minion.isHellhound) {
			name = "Hellhound ";
		} else if (minion.isDemon) {
			name = "Demon ";
		} else if (minion.isKoboldKing) {
			name = "Kobold King ";
		} else if (minion.isKobold) {

			name = "Kobold ";
		} else if (minion.isWallUnit) {
			if (minion.isMagicWallUnit)
				name = "Magic Wall ";
			else
				name = "Wall ";
		} else if (minion.isAssistant) {
			name = "Assistant ";
		} else if (minion.isFaerie) {
			name = "Faerie ";
		} else {
			name = "Skeleton ";
		}
		name += ofToString(ui.displayNumber);

		// --- Draw Name Text ---
		float fontScale = 0.9f;
		float textBlockX = ui.bounds.x + 10 * scale;
		float textBlockY = ui.bounds.y + 5 * scale;
		ofRectangle nameBounds = uiFont.getStringBoundingBox(name, 0, 0);

		ofPushMatrix();
		ofTranslate(textBlockX, textBlockY + nameBounds.height * fontScale);
		ofScale(fontScale, fontScale);
		ofSetColor(ofColor::white);
		uiFont.drawString(name, 0, 0);
		ofPopMatrix();

		// (Minion luck/status moved to hover tooltip; no inline luck shown here)
		//
		// --- Draw Model FBO ---
		float textBlockBottom = textBlockY + (nameBounds.height * fontScale) + (25 * scale);
		float modelAreaHeight = ui.bounds.getBottom() - textBlockBottom - (5 * scale);

		ui.modelViewport.set(
			textBlockX + 15 * scale,
			textBlockBottom,
			modelAreaHeight,
			modelAreaHeight);
		ofSetColor(255);
		modelFbo.draw(ui.modelViewport);

		// --- Draw Icons ---
		float iconMargin = 8 * scale;
		float iconHeight = ui.bounds.height - (iconMargin * 2);
		float cardAspectRatio = cardBackImage.getWidth() / cardBackImage.getHeight();
		float iconWidth = iconHeight * cardAspectRatio;
		float iconsY = ui.bounds.y + iconMargin;

		ui.discardRect.set(ui.bounds.getRight() - (iconWidth + iconMargin), iconsY, iconWidth, iconHeight);
		ui.deckRect.set(ui.bounds.getRight() - (iconWidth * 2 + iconMargin + 5 * scale), iconsY, iconWidth, iconHeight);

		// --- Status Bars ---
		float availableWidth = ui.deckRect.x - textBlockX - (15 * scale);
		drawMinionStatusBars(minion, name, textBlockX, textBlockY, availableWidth);

		// Deck
		ofSetColor(255);
		if (!minion.deck.empty()) {
			cardBackImage.draw(ui.deckRect);
		} else {
			ofSetColor(20, 20, 20, 200);
			ofDrawRectRounded(ui.deckRect, 3);
		}
		if (ui.playerIndex == currentPlayerIndex && !hasDrawnCardsThisTurn) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::green);
			ofSetLineWidth(3 * scale);
			ofDrawRectRounded(ui.deckRect, 5);
			ofPopStyle();
		}

		// Discard
		if (!minion.discardPile.empty()) {
			cardSpriteSheet.getTexture().drawSubsection(ui.discardRect, minion.discardPile.back().textureRect);
		} else {
			ofSetColor(20, 20, 20, 200);
			ofDrawRectRounded(ui.discardRect, 3);
		}

		// (Minion status effects are shown only in the hover tooltip above the unit)
	} // End of loop
}
//--------------------------------------------------------------
void ofApp::cancelMagicHand() {
	isMagicHandMenuOpen = false;
	pendingMagicHandCardIndex = -1;
}

void ofApp::resolveMagicHandPull() {
	Player & caster = players[currentPlayerIndex];
	glm::ivec2 wallPos = magicHandTargetTile;
	glm::ivec2 casterPos = { caster.x, caster.y };

	// Direction from Caster -> Wall
	glm::ivec2 dir = wallPos - casterPos;

	// Position BEHIND caster
	glm::ivec2 backPos = casterPos - dir;

	// Check bounds and occupancy for backPos
	bool isValid = true;
	if (backPos.x < 0 || backPos.x >= BOARD_WIDTH || backPos.y < 0 || backPos.y >= BOARD_HEIGHT)
		isValid = false;
	else if (board[backPos.x][backPos.y].hasWall || board[backPos.x][backPos.y].hasPlayer)
		isValid = false;

	if (!isValid) {
		spawnFloatingText(gridToWorld(caster.x, caster.y), "Blocked Behind!", ofColor::red);
		return; // Don't close menu, allow retry or cancel
	}

	// Send resolution to opponent before applying locally
	sendMagicHandResolutionPacket(2);

	// Execute Pull
	// 1. Move Caster to BackPos
	board[caster.x][caster.y].hasPlayer = false;
	caster.x = backPos.x;
	caster.y = backPos.y;
	board[caster.x][caster.y].hasPlayer = true;
	playerVisualPos = gridToWorld(caster.x, caster.y);

	// 2. Move Wall to Caster's Old Pos
	board[wallPos.x][wallPos.y].hasWall = false;
	board[casterPos.x][casterPos.y].hasWall = true;
	if (board[wallPos.x][wallPos.y].isMagicWall) {
		board[casterPos.x][casterPos.y].isMagicWall = true;
		board[wallPos.x][wallPos.y].isMagicWall = false;
	}

	// 3. Finalize
	buildLevelMesh();

	currentAP -= players[currentPlayerIndex].hand[pendingMagicHandCardIndex].cost;
	players[currentPlayerIndex].playedCardsPile.push_back(players[currentPlayerIndex].hand[pendingMagicHandCardIndex]);
	players[currentPlayerIndex].hand.erase(players[currentPlayerIndex].hand.begin() + pendingMagicHandCardIndex);

	isMagicHandMenuOpen = false;
	invalidateTargetCache();
}

void ofApp::resolveMagicHandPush() {
	Player & caster = players[currentPlayerIndex];
	glm::ivec2 wallPos = magicHandTargetTile;
	glm::ivec2 casterPos = { caster.x, caster.y };
	glm::ivec2 dir = wallPos - casterPos;
	glm::ivec2 targetPos = wallPos + dir; // Where the wall goes

	// Check bounds
	if (targetPos.x < 0 || targetPos.x >= BOARD_WIDTH || targetPos.y < 0 || targetPos.y >= BOARD_HEIGHT) {
		spawnFloatingText(gridToWorld(wallPos.x, wallPos.y), "Edge of World!", ofColor::red);
		return;
	}

	// Check if target has another wall
	if (board[targetPos.x][targetPos.y].hasWall) {
		spawnFloatingText(gridToWorld(wallPos.x, wallPos.y), "Blocked by Wall!", ofColor::red);
		return;
	}

	// Check for Unit
	if (board[targetPos.x][targetPos.y].hasPlayer) {
		// Find the unit
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == targetPos.x && players[i].y == targetPos.y) {
				magicHandPushedUnitIndex = (int)i;
				break;
			}
		}

		// Send resolution to opponent before applying locally
		sendMagicHandResolutionPacket(1);

		// Start Damage Roll (2d4 Physical)
		magicHandPushDir = dir;
		pendingMagicHandRollResult = startDiceRoll(2, 4, PURPOSE_MAGIC_HAND_DAMAGE, "Magic Hand Crush");
		isWaitingForMagicHandDamage = true;

		// Pay cost now
		currentAP -= caster.hand[pendingMagicHandCardIndex].cost;
		caster.playedCardsPile.push_back(caster.hand[pendingMagicHandCardIndex]);
		caster.hand.erase(caster.hand.begin() + pendingMagicHandCardIndex);

		isMagicHandMenuOpen = false;

		// Move Caster and Wall happens AFTER dice logic to sync animations
		return;
	}

	// Send resolution to opponent before applying locally
	sendMagicHandResolutionPacket(1);

	// Empty Space: Just Move
	board[caster.x][caster.y].hasPlayer = false;

	// Move Caster to Wall Pos
	caster.x = wallPos.x;
	caster.y = wallPos.y;
	board[caster.x][caster.y].hasPlayer = true;
	playerVisualPos = gridToWorld(caster.x, caster.y);

	// Move Wall to Target Pos
	board[wallPos.x][wallPos.y].hasWall = false;
	board[targetPos.x][targetPos.y].hasWall = true;
	if (board[wallPos.x][wallPos.y].isMagicWall) {
		board[targetPos.x][targetPos.y].isMagicWall = true;
		board[wallPos.x][wallPos.y].isMagicWall = false;
	}

	buildLevelMesh();

	currentAP -= caster.hand[pendingMagicHandCardIndex].cost;
	caster.playedCardsPile.push_back(caster.hand[pendingMagicHandCardIndex]);
	caster.hand.erase(caster.hand.begin() + pendingMagicHandCardIndex);

	isMagicHandMenuOpen = false;
	invalidateTargetCache();
}
//--------------------------------------------------------------
void ofApp::generateDraftOptions(int classTier, const std::vector<int> * forcedIndices) {
	draftAcceptLocked = false;
	draftAcceptApplied = false;
	// Deterministic: always use shared gameplayRNG so both host and client
	// generate the same three options locally. Do NOT wait for host packets.
	draftOptions.clear();
	currentDraftClassTier = classTier;
	currentDraftOptionPoolIndices = { { -1, -1, -1 } };
	const std::vector<Card> * pool = &class1Cards;
	if (classTier == 2) pool = &class2Cards;
	if (classTier == 3) pool = &class3Cards;

	if (pool->empty()) return;

	std::vector<int> indices(pool->size());
	std::iota(indices.begin(), indices.end(), 0);

	// If forced indices were provided (legacy/explicit packet), use them directly
	if (forcedIndices && !forcedIndices->empty()) {
		int slot = 0;
		for (int idx : *forcedIndices) {
			if (idx >= 0 && idx < (int)pool->size()) {
				draftOptions.push_back((*pool)[idx]);
				if (slot < 3) currentDraftOptionPoolIndices[slot] = idx;
				slot++;
			}
		}
		// Ensure pick counts are still set below
		return;
	}

	// Deterministic draft generation derived from the shared map seed and draft context.
	// This makes draft options independent of the global `gameplayRNG` state so both
	// host and clients see identical options even if other RNG calls differ.
	// Increment the counter to ensure variety across different draft sessions
	draftGenerationCounter++;
	uint32_t derivedSeed = currentMapSeed;
	derivedSeed ^= (uint32_t)classTier * 2654435761u; // golden ratio mixing
	derivedSeed ^= ((uint32_t)draftPlayerIndex << 16);
	derivedSeed ^= ((uint32_t)draftStage << 24);
	derivedSeed ^= draftGenerationCounter * 1103515245u; // Add generation counter for variety

	std::mt19937 draftRng(derivedSeed);
	deterministic_shuffle(indices, draftRng);

	for (int i = 0; i < 3 && i < (int)indices.size(); ++i) {
		draftOptions.push_back((*pool)[indices[i]]);
		currentDraftOptionPoolIndices[i] = indices[i];
	}

	// Set pick count based on rules
	if (classTier == 1)
		draftPicksRemaining = 2; // Pick 2
	else
		draftPicksRemaining = 1; // Pick 1

	// Ensure we always log context for easier debugging
	ofLogNotice("Draft") << "generateDraftOptions called: classTier=" << classTier << " draftStage=" << draftStage << " draftPlayerIndex=" << draftPlayerIndex << " poolSize=" << pool->size();

	// If the pool is unexpectedly empty, log and bail early (avoid silent hang)
	if (pool->empty()) {
		ofLogError("Draft") << "generateDraftOptions: pool for classTier " << classTier << " is empty!";
		// Clear any stale UI state so user doesn't sit on an empty draft screen
		draftOptions.clear();
		selectedDraftIndices.clear();
		currentState = STATE_DRAFTING;
		return;
	}

	// Prepare local draft state so host doesn't rely on client-only apply path
	selectedDraftIndices.clear();
	currentState = STATE_DRAFTING; // Ensure UI draws
	waitingForDraftOptions = false; // Host shouldn't be waiting for itself

	// If we are the authoritative host in multiplayer, send the exact indices
	if (isHost()) {
		DraftOptionsPacket dp = {};
		dp.type = PKT_DRAFT_OPTIONS;
		dp.playerID = myLocalPlayerID;
		dp.classTier = classTier;
		dp.optionIndex0 = (indices.size() > 0) ? indices[0] : -1;
		dp.optionIndex1 = (indices.size() > 1) ? indices[1] : -1;
		dp.optionIndex2 = (indices.size() > 2) ? indices[2] : -1;
		dp.draftPlayerIdx = draftPlayerIndex;
		dp.picksRemaining = draftPicksRemaining;
		dp.draftStage = draftStage;
		dp.isInGameDraft = isInGameDraft ? 1 : 0;
		dp.draftGenCounter = draftGenerationCounter; // Send the counter so client matches
		steamManager.sendPacket(&dp, sizeof(dp));
		ofLogNotice("Network") << "Host sent DraftOptionsPacket: " << dp.optionIndex0 << "," << dp.optionIndex1 << "," << dp.optionIndex2 << " derivedSeed=" << derivedSeed << " draftGenCounter=" << draftGenerationCounter;

		// Debug: also log the exact card names for easier inspection across clients
		ofLogNotice("Draft") << "Options names: " << ((indices.size() > 0) ? (*pool)[indices[0]].name : "-") << ", " << ((indices.size() > 1) ? (*pool)[indices[1]].name : "-") << ", " << ((indices.size() > 2) ? (*pool)[indices[2]].name : "-");

		// Immediately send a DraftStatePacket as well so clients have authoritative
		// draft state right after receiving options (reduces race between forwarded
		// Accept and authoritative options/state).
		DraftStatePacket dsp;
		dsp.type = PKT_DRAFT_STATE;
		dsp.playerID = myLocalPlayerID;
		dsp.classTier = classTier;
		dsp.draftPlayerIdx = draftPlayerIndex;
		dsp.picksRemaining = draftPicksRemaining;
		dsp.draftStage = draftStage;
		dsp.isInGameDraft = isInGameDraft ? 1 : 0;
		dsp.currentPlayerIndex = currentPlayerIndex;
		steamManager.sendPacket(&dsp, sizeof(dsp));
		ofLogNotice("Network") << "Host sent DraftStatePacket (post-options): class=" << dsp.classTier << " player=" << dsp.draftPlayerIdx << " picks=" << dsp.picksRemaining << " stage=" << dsp.draftStage;
	}
}

// Apply authoritative option indices sent by host (clients call this when receiving DraftOptionsPacket)
void ofApp::applyDraftOptionsFromPool(int classTier, const std::vector<int> & indices, int picksRemaining, int draftingPlayerIdx) {
	draftOptions.clear();
	currentDraftClassTier = classTier;
	currentDraftOptionPoolIndices = { { -1, -1, -1 } };
	const std::vector<Card> * pool = &class1Cards;
	if (classTier == 2) pool = &class2Cards;
	if (classTier == 3) pool = &class3Cards;

	// If we've already transitioned to gameplay (race condition where a late
	// DraftOptions packet arrives after the host moved to gameplay), ignore
	// these late options so we don't re-enter drafting on the client.
	if (currentState == STATE_GAMEPLAY) {
		ofLogNotice("Draft") << "applyDraftOptionsFromPool: ignoring late DraftOptions (already in gameplay)";
		return;
	}

	int slot = 0;
	for (int idx : indices) {
		if (idx >= 0 && idx < (int)pool->size()) {
			draftOptions.push_back((*pool)[idx]);
			if (slot < 3) currentDraftOptionPoolIndices[slot] = idx;
			slot++;
		}
	}

	draftPicksRemaining = picksRemaining;
	draftPlayerIndex = draftingPlayerIdx;
	selectedDraftIndices.clear();
	currentState = STATE_DRAFTING;
	// We've applied authoritative options from host; stop waiting
	waitingForDraftOptions = false;
	draftAcceptLocked = false;
	draftAcceptApplied = false;

	// Log applied options for debugging (helps determine whether Windows client actually applied options)
	if ((int)draftOptions.size() != lastLoggedDraftOptionsCount) {
		lastLoggedDraftOptionsCount = (int)draftOptions.size();
		std::string names = "";
		for (size_t i = 0; i < draftOptions.size(); ++i) {
			if (i) names += ", ";
			names += draftOptions[i].name;
		}
		ofLogNotice("Draft") << "applyDraftOptionsFromPool: applied " << draftOptions.size() << " options for player=" << draftPlayerIndex << " stage=" << draftStage << " names=" << names;
	}
}

void ofApp::onCardPicked(int optionIndex) {
	if (optionIndex < 0 || optionIndex >= (int)draftOptions.size()) return;

	Player & p = players[draftPlayerIndex];
	Card picked = draftOptions[optionIndex];

	// IN-GAME DRAFT LOGIC
	if (isInGameDraft) {
		p.deck.push_back(picked);
		// Shuffle deck to include new card
		shuffleGameVector(p.deck, draftPlayerIndex);

		draftOptions.clear();
		isInGameDraft = false;

		// Return to game
		currentState = STATE_GAMEPLAY;
		return;
	}
	// Remove picked card from options so it can't be picked again this round
	draftOptions.erase(draftOptions.begin() + optionIndex);

	draftPicksRemaining--;

	if (draftPicksRemaining <= 0) {
		// Stage Complete
		draftStage++;
		if (draftStage == 1) {
			// Move to Class 2
			generateDraftOptions(2);
		} else {
			// Player Finished Drafting
			// Check if both players drafted
			// The logic: Winner goes first. If P1 went, check if P2 deck empty.
			int otherPlayer = (draftPlayerIndex + 1) % 2;
			if (players[otherPlayer].deck.empty()) {
				// Switch to other player
				draftPlayerIndex = otherPlayer;
				draftStage = 0;
				generateDraftOptions(1);
			} else {
				// Both done! Start Game.

				// Shuffle decks (host-authoritative per player)
				for (size_t pi = 0; pi < players.size(); ++pi) {
					shuffleGameVector(players[pi].deck, (int)pi);
				}

				// Determine who starts based on initiative winner (who drafted first)
				// The winner (highest roller) was the *first* to draft and should go first in gameplay.
				// After both finish drafting, draftPlayerIndex points to whoever drafted SECOND.
				// So the winner (who should go first) is (draftPlayerIndex + 1) % 2.

				currentPlayerIndex = (draftPlayerIndex + 1) % 2;

				// FINAL SETUP
				currentState = STATE_GAMEPLAY;
				initialDraftComplete = true;

				// CLIENT: Wait for host's TurnStart packet (contains authoritative first dice roll)
				if (isClient()) {
					ofLogNotice("Network") << "CLIENT: Draft complete, waiting for host's first TurnStart packet.";
					waitingForTurnStartFromHost = true;
					// Don't call startNewTurn() - host will send PKT_TURN_START
				} else {
					// Host: can proceed with local turn start
					ofLogNotice("Network") << "HOST: Draft complete, starting first turn.";
					startNewTurn();
				}
			}
		}
	}
}
//--------------------------------------------------------------
void ofApp::drawInitiativeRoll() {
	if (activeDiceRolls.size() >= 2) {

		// Position labels so that the local player is always on the left
		glm::vec3 leftPos(-6.0f, 11.0f, 0.0f);
		glm::vec3 rightPos(6.0f, 11.0f, 0.0f);

		// Player 0 is die index 0, player 1 is die index 1
		bool player0OnLeft = (myLocalPlayerID == 0);
		glm::vec3 player0Pos = player0OnLeft ? leftPos : rightPos;
		glm::vec3 player1Pos = player0OnLeft ? rightPos : leftPos;

		glm::vec2 localScreen = cam.worldToScreen((myLocalPlayerID == 0) ? player0Pos : player1Pos);
		glm::vec2 opponentScreen = cam.worldToScreen((myLocalPlayerID == 0) ? player1Pos : player0Pos);

		// Standardized Text Drawer (Smaller scale)
		auto drawLabel = [&](string text, float x, float y, ofColor col) {
			float fontScale = 0.7f; // Smaller size
			ofRectangle bbox = titleFont.getStringBoundingBox(text, 0, 0);
			float tx = x - (bbox.width * fontScale / 2.0f);
			float ty = y;

			ofPushMatrix();
			ofTranslate(tx, ty);
			ofScale(fontScale, fontScale);

			// Shadow
			ofSetColor(0, 0, 0, 255);
			titleFont.drawString(text, 3, 3);

			// Main Text (Standard UI White/Gold/Grey scheme)
			ofSetColor(col);
			titleFont.drawString(text, 0, 0);

			ofPopMatrix();
		};

		// Draw local player on left, opponent on right
		ofColor localPlayerColor = (myLocalPlayerID == 0) ? ofColor(70, 160, 255) : ofColor(255, 80, 80);
		ofColor opponentColor = (myLocalPlayerID == 0) ? ofColor(255, 80, 80) : ofColor(70, 160, 255);
		string localPlayerName = (myLocalPlayerID == 0) ? getPlayerSteamName(0) : getPlayerSteamName(1);
		string opponentPlayerName = (myLocalPlayerID == 0) ? getPlayerSteamName(1) : getPlayerSteamName(0);

		drawLabel(localPlayerName, localScreen.x, localScreen.y, localPlayerColor);
		drawLabel(opponentPlayerName, opponentScreen.x, opponentScreen.y, opponentColor);

		// Draw Result Message
		if (activeDiceRolls[0].isFinishedVisual && activeDiceRolls[1].isFinishedVisual) {
			string msg = "";

			if (activeDiceRolls[0].result > activeDiceRolls[1].result)
				msg = getPlayerSteamName(0) + " Wins!";
			else if (activeDiceRolls[1].result > activeDiceRolls[0].result)
				msg = getPlayerSteamName(1) + " Wins!";
			else
				msg = "Tie! Rerolling...";

			// Draw in Instruction Area (Top Center)
			ofRectangle mBox = titleFont.getStringBoundingBox(msg, 0, 0);
			float tx = (ofGetWidth() / 2.0f) - (mBox.width / 2.0f);
			float ty = ofGetHeight() * 0.25f;

			ofSetColor(0, 0, 0, 255);
			titleFont.drawString(msg, tx + 2, ty + 2);
			ofSetColor(ofColor::gold);
			titleFont.drawString(msg, tx, ty);
		}
	}
}
//--------------------------------------------------------------
void ofApp::drawDraftScreen() {
	// 1. Construct Specific Instruction Text
	string pName = (draftPlayerIndex == 0) ? player0SteamName : player1SteamName;
	string msg = "";

	if (isInGameDraft) {
		msg = pName + ": Key Found! Choose 1 Card (Get 1 Copy)";
	} else if (draftStage == 0) {
		msg = pName + " - Class 1: Choose 2 (Get 2 Copies)";
	} else {
		msg = pName + " - Class 2: Choose 1 (Get 1 Copy)";
	}

	// 2. Draw Instruction Text (Top Center, Shadowed)
	ofRectangle bbox = titleFont.getStringBoundingBox(msg, 0, 0);
	float tx = (ofGetWidth() / 2.0f) - (bbox.width / 2.0f);
	float ty = ofGetHeight() * 0.25f;

	ofSetColor(0, 0, 0, 255);
	titleFont.drawString(msg, tx + 2, ty + 2);
	ofSetColor(ofColor::white);
	titleFont.drawString(msg, tx, ty);

	// 2b. Draw Class Tier Text Below Prompt
	std::string classTierText = "";
	ofColor classTierColor = ofColor::white;
	// Predeclare so we can use values for layout later
	ofRectangle classBox;
	float classTx = 0, classTy = 0;
	if (!isInGameDraft) {
		if (draftStage == 0) {
			classTierText = "Class 1";
			classTierColor = ofColor(205, 127, 50); // Bronze
		} else if (draftStage == 1) {
			classTierText = "Class 2";
			classTierColor = ofColor(192, 192, 192); // Silver
		}
	}

	if (!classTierText.empty()) {
		classBox = titleFont.getStringBoundingBox(classTierText, 0, 0);
		classTx = (ofGetWidth() / 2.0f) - (classBox.width / 2.0f);
		classTy = ty + bbox.height + 18;
		ofSetColor(0, 0, 0, 255);
		titleFont.drawString(classTierText, classTx + 2, classTy + 2);
		ofSetColor(classTierColor);
		titleFont.drawString(classTierText, classTx, classTy);
	}

	// 3. Draw Cards
	float cardW = 340;
	float cardH = cardW * 1.4f;
	float spacing = 60;
	float startX = (ofGetWidth() - (3 * cardW + 2 * spacing)) / 2;
	float startY = ofGetHeight() / 2 - cardH / 2;

	// Prevent overlap: ensure the top text (prompt + class text if present) clears space above the cards
	float topTextBottom = ty + bbox.height;
	if (!classTierText.empty()) {
		topTextBottom = classTy + classBox.height;
	}
	float minStartY = topTextBottom + 24.0f; // small padding
	if (startY < minStartY) {
		startY = minStartY;
	}

	for (size_t i = 0; i < draftOptions.size(); ++i) {
		float x = startX + i * (cardW + spacing);
		ofRectangle cardRect(x, startY, cardW, cardH);

		// Check Selection
		bool isSelected = false;
		for (int sel : selectedDraftIndices) {
			if (sel == (int)i) isSelected = true;
		}

		// Selection Highlight (Yellow)
		if (isSelected) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::yellow);
			ofSetLineWidth(6);
			ofDrawRectRounded(x - 8, startY - 8, cardW + 16, cardH + 16, 12);
			ofPopStyle();
		}
		// Hover Highlight (White/Subtle)
		else if (cardRect.inside(ofGetMouseX(), ofGetMouseY())) {
			ofPushStyle();
			ofNoFill();
			ofSetColor(ofColor::white);
			ofSetLineWidth(3);
			ofDrawRectRounded(x - 5, startY - 5, cardW + 10, cardH + 10, 10);
			ofPopStyle();
		}

		// Draw Card Sprite
		ofSetColor(255);
		cardSpriteSheet.drawSubsection(x, startY, cardW, cardH,
			draftOptions[i].textureRect.x, draftOptions[i].textureRect.y,
			draftOptions[i].textureRect.width, draftOptions[i].textureRect.height);
	}

	// Throttled draw-time debug: log if we are drawing non-empty options but haven't logged recently
	float now = ofGetElapsedTimef();
	if (!draftOptions.empty() && (now - lastDraftDrawLogTime) > 1.0f) {
		lastDraftDrawLogTime = now;
		std::string names = "";
		for (size_t i = 0; i < draftOptions.size(); ++i) {
			if (i) names += ", ";
			names += draftOptions[i].name;
		}
		ofLogNotice("Draft") << "drawDraftScreen: drawing " << draftOptions.size() << " options (player=" << draftPlayerIndex << " stage=" << draftStage << ") names=" << names;
	}

	// 4. Draw Accept Button
	int required = 1;
	if (!isInGameDraft && draftStage == 0) required = 2;

	bool canAccept = ((int)selectedDraftIndices.size() == required);

	// Only show the Accept button to the drafting player (or in singleplayer)
	bool showAccept = true;
	if (isMultiplayer) {
		if (!players.empty() && players[draftPlayerIndex].playerID != myLocalPlayerID) showAccept = false;
	}

	if (showAccept) {
		float btnW = 220, btnH = 60;
		float btnX = (ofGetWidth() - btnW) / 2.0f;
		float btnY = startY + cardH + 40;

		draftAcceptButtonRect.set(btnX, btnY, btnW, btnH);

		ofSetColor(canAccept ? ofColor(70, 160, 255) : ofColor(100, 100, 100));
		ofDrawRectRounded(draftAcceptButtonRect, 12);

		ofSetColor(ofColor::white);
		ofRectangle acceptTextBox = uiFont.getStringBoundingBox("Accept", 0, 0);
		uiFont.drawString("Accept", btnX + (btnW - acceptTextBox.width) / 2, btnY + (btnH + acceptTextBox.height) / 2 - 6);
	} else {
		// Hide accept: clear the rect so hits are ignored
		draftAcceptButtonRect.set(0, 0, 0, 0);
	}
}
//--------------------------------------------------------------
void ofApp::drawTrainMenuUI() {
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	// Dark Overlay
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// Title & Desc
	string title = "Train";
	string desc = "Choose your training path:";

	// Colors: Yellow for AP, Bronze/Orange for Class 1 Draft
	ofColor apColor(255, 215, 0);
	ofColor draftColor(205, 127, 50);

	// Ensure buttons are positioned if not already set (safety check)
	if (trainMenuRect.width == 0) {
		float w = 600, h = 300;
		float x = ofGetWidth() / 2 - w / 2, y = ofGetHeight() / 2 - h / 2;
		trainMenuRect.set(x, y, w, h);
		float btnW = 260, btnH = 80, spacing = 30;
		float startX = x + (w - (btnW * 2 + spacing)) / 2;
		float btnY = y + 130;
		trainBtnAP.set(startX, btnY, btnW, btnH);
		trainBtnDraft.set(startX + btnW + spacing, btnY, btnW, btnH);
	}

	drawCardChoicePanel(trainMenuRect, title, desc, trainBtnAP, trainBtnDraft,
		"+3 AP Next Turn", "Draft Class 1",
		apColor, draftColor, true, true);
}
//--------------------------------------------------------------
void ofApp::exit() {
	steamManager.cleanup();
	// Ensure the Steam API is fully shut down on app exit
	steamManager.shutdownAPI();
}
// --------------------------------------------------------------
void ofApp::updateAndSendHover(HoverType type, int gridX, int gridY, int cardIndex) {
	// Check if hover state changed
	if (type != localHoverType || gridX != localHoverGridX || gridY != localHoverGridY || cardIndex != localHoverCardIndex) {

		// Update local hover state
		localHoverType = type;
		localHoverGridX = gridX;
		localHoverGridY = gridY;
		localHoverCardIndex = cardIndex;

		// Send hover packet to opponent if in multiplayer
		if (isMultiplayer && steamManager.isConnected()) {
			HoverPacket pkt = {};
			pkt.type = PKT_HOVER;
			pkt.playerID = myLocalPlayerID;
			pkt.hoverType = static_cast<uint8_t>(type);
			pkt.gridX = static_cast<int8_t>(gridX);
			pkt.gridY = static_cast<int8_t>(gridY);
			pkt.cardIndex = static_cast<int8_t>(cardIndex);
			steamManager.sendPacket(&pkt, sizeof(pkt));
		}
	}
}
// --------------------------------------------------------------
void ofApp::processNetworkPackets() {
	while (!steamManager.packetQueue.empty()) {
		std::vector<char> buffer = steamManager.packetQueue.front();
		steamManager.packetQueue.pop();

		// --- NEW BLOCK: CHECK FOR SEED REQUEST (Before casting to Header) ---
		// If we received the string "REQ_SEED", we must resend the handshake.
		if (buffer.size() == 8) {
			string msg(buffer.begin(), buffer.end());
			// Note: Use steamManager.isHost() directly since this might be called before isMultiplayer is set
			if (msg == "REQ_SEED" && steamManager.isHost()) {
				ofLogNotice("Network") << "Host: Received Seed Request. Resending Seed: " << currentMapSeed;

				HandshakePacket pkt = {};
				pkt.type = PKT_HANDSHAKE;
				pkt.playerID = myLocalPlayerID;
				pkt.seq = 0;
				pkt.seed = currentMapSeed; // Use the stored seed!
				ofLogNotice("Network") << "Host resending handshake: type=" << (int)pkt.type << " playerID=" << pkt.playerID << " seq=" << pkt.seq << " seed=" << pkt.seed;
				steamManager.sendPacket(&pkt, sizeof(pkt));
				continue; // Done with this packet
			}
		}
		// -------------------------------------------------------------------

		if (buffer.size() < sizeof(PacketHeader)) continue;

		PacketHeader * header = (PacketHeader *)buffer.data();
		// ACK handling removed; rely on SteamNetworkingSockets reliability.

		// Only check sequence numbers for PKT_ACTION (card plays) to prevent duplicate card plays
		// All other packets are either informational or handled by game state logic
		if (header->type == PKT_ACTION && header->seq > 0) {
			int sender = (header->playerID == 0 || header->playerID == 1) ? (int)header->playerID : -1;
			if (sender >= 0) {
				if (header->seq <= lastReceivedSeqByPlayer[sender]) {
					ofLogNotice("Network") << "DROPPED DUPLICATE PACKET: type=" << (int)header->type << " seq=" << header->seq << " lastReceivedSeq[sender=" << sender << "]=" << lastReceivedSeqByPlayer[sender];
					continue;
				}
				lastReceivedSeqByPlayer[sender] = header->seq;
			}
		}
		// Temporary reusable packet used for state syncs
		DraftStatePacket sp = {};
		// Handle Shuffle packets early so clients can deterministically apply them without touching gameplayRNG
		if (header->type == PKT_SHUFFLE) {
			ShufflePacket * spk = (ShufflePacket *)header;
			ofLogNotice("Network") << "Shuffle packet received: player=" << spk->playerIndex << " nonce=" << spk->nonce;
			if (spk->playerIndex >= 0 && spk->playerIndex < (int)players.size()) {
				if (isClient() && (currentState == STATE_DRAFTING || isInGameDraft) && !draftAcceptApplied) {
					pendingShuffleNonce[spk->playerIndex] = spk->nonce;
					hasPendingShuffleNonce[spk->playerIndex] = true;
					ofLogNotice("Network") << "Client: Deferring shuffle for player " << spk->playerIndex << " until draft accept applies";
					continue;
				}
				if (isClient() && lastAppliedShuffleNonce[spk->playerIndex] == spk->nonce) {
					ofLogNotice("Network") << "Client: Ignoring duplicate shuffle nonce for player " << spk->playerIndex;
					continue;
				}
				std::mt19937 shuffleRng(spk->nonce);
				deterministic_shuffle(players[spk->playerIndex].deck, shuffleRng);
				if (isClient()) {
					lastAppliedShuffleNonce[spk->playerIndex] = spk->nonce;
				}
				// Only clear the per-player skip guard if it was set for this player
				if (skipClientShuffleFor == spk->playerIndex) {
					skipClientShuffleFor = -1;
					ofLogNotice("Network") << "Client: Applied shuffle nonce for player " << spk->playerIndex << " and cleared skip flag";
				} else {
					ofLogNotice("Network") << "Client: Applied shuffle nonce for player " << spk->playerIndex;
				}
			}
			continue; // Done with this packet
		}

		// Handle Renewed Inspiration selection
		if (header->type == PKT_RENEWED_INSPIRATION) {
			RenewedInspirationPacket * rpk = (RenewedInspirationPacket *)header;
			if (rpk->playerIndex >= 0 && rpk->playerIndex < (int)players.size()) {
				Player & p = players[rpk->playerIndex];
				std::vector<int> indices;
				for (int i = 0; i < rpk->count && i < 16; ++i) {
					indices.push_back(rpk->indices[i]);
				}
				std::sort(indices.begin(), indices.end(), std::greater<int>());
				// Only discard cards - don't draw! The DrawCards packet will handle that.
				for (int idx : indices) {
					if (idx >= 0 && idx < (int)p.hand.size()) {
						p.discardPile.push_back(p.hand[idx]);
						p.hand.erase(p.hand.begin() + idx);
					}
				}
				ofLogNotice("Network") << "Renewed Inspiration: Opponent discarded " << indices.size() << " cards (will draw from DrawCards packet)";
			}
			continue;
		}

		// Handle TurnStart packets (Host -> Client): authoritative AP dice for starting player
		if (header->type == PKT_TURN_START) {
			TurnStartPacket * tpk = (TurnStartPacket *)header;
			ofLogNotice("Network") << "TurnStart packet received: player=" << tpk->currentPlayerIndex << " dice=" << (int)tpk->diceNum << " total=" << tpk->finalTotal;

			// Prevent duplicate processing: check if we're already on this turn
			static int lastProcessedTurnPlayer = -1;
			static int lastProcessedTurnCounter = -1;
			if (tpk->currentPlayerIndex == lastProcessedTurnPlayer && globalTurnCounter == lastProcessedTurnCounter) {
				ofLogNotice("Network") << "Ignoring duplicate TurnStart packet (already processed player=" << tpk->currentPlayerIndex << " turn=" << globalTurnCounter << ")";
				return;
			}
			lastProcessedTurnPlayer = tpk->currentPlayerIndex;
			lastProcessedTurnCounter = globalTurnCounter;

			if (tpk->currentPlayerIndex >= 0 && tpk->currentPlayerIndex < (int)players.size()) {
				// Set up turn state
				waitingForTurnStartFromHost = false;
				endTurnLocked = false;
				currentState = STATE_GAMEPLAY; // Transition to gameplay state
				currentPlayerIndex = tpk->currentPlayerIndex;
				lastAPDiceNum = (int)tpk->diceNum;
				lastAPDiceSides = (int)tpk->diceSides;

				// Complete turn setup (same as continueNewTurn does)
				Player & startingPlayer = players[currentPlayerIndex];
				ofLogNotice("Game") << "Player " << startingPlayer.playerID << "'s turn begins (from TurnStart).";
				ofLogNotice("Game") << "Client state: currentState=" << currentState << " myLocalPlayerID=" << myLocalPlayerID << " currentPlayerID=" << startingPlayer.playerID;
				hasDrawnCardsThisTurn = false;
				selectedCardIndex = -1;
				draggedCardIndex = -1;
				playerAction = NONE;
				clearHighlights();
				calculateTargetHighlights();
				playerVisualPos = gridToWorld(startingPlayer.x, startingPlayer.y);
				animationPath.clear();
				isPlayerAnimating = false;
				animatingPlayerIndex = -1;
				activeDiceRolls.clear();
				currentAP = 0;

				// CLIENT: Create visual dice rolls from host-provided results
				// This ensures the client sees the AP roll animation even though the host rolled it
				std::string diceLabel = "";
				if (startingPlayer.isWolf) {
					diceLabel = "Wolf AP Roll";
				} else if (startingPlayer.isHellhound) {
					diceLabel = "Hellhound AP Roll";
				} else if (startingPlayer.isDemon) {
					diceLabel = "Demon AP Roll";
				} else if (startingPlayer.isKobold) {
					diceLabel = "Kobold AP Roll";
				} else if (startingPlayer.isWallUnit) {
					diceLabel = startingPlayer.isMagicWallUnit ? "Magic Wall Unit AP" : "Wall Unit AP";
				} else if (startingPlayer.isKoboldKing) {
					diceLabel = "Kobold King AP";
				} else if (startingPlayer.isAssistant) {
					diceLabel = "Assistant AP (Coin)";
				} else if (startingPlayer.isFaerie) {
					diceLabel = "Faerie AP Roll";
				} else if (startingPlayer.isMinion) {
					diceLabel = getPlayerDisplayName(currentPlayerIndex) + " AP Roll";
				} else {
					diceLabel = "Player AP Roll";
				}

				currentDiceLabel = diceLabel;

				// Create DiceRoll objects with host-provided results
				for (int i = 0; i < (int)tpk->diceNum; ++i) {
					DiceRoll newRoll;
					newRoll.purpose = PURPOSE_AP;
					newRoll.sides = (int)tpk->diceSides;
					newRoll.rawResult = (int)tpk->rawResults[i];
					newRoll.result = (int)tpk->finalResults[i];
					newRoll.startTime = ofGetElapsedTimef();
					newRoll.isFinishedVisual = false;
					newRoll.associatedUnit = currentPlayerIndex;

					// Calculate the final quaternion to show the correct die face
					int sides = newRoll.sides;
					int rawRoll = newRoll.rawResult;
					glm::vec3 faceVec(0, 1, 0); // Default

					if (sides == 2) {
						// Coin
						glm::quat flip180X = glm::angleAxis(glm::radians(180.0f), glm::vec3(1, 0, 0));
						glm::quat rot180Y = glm::angleAxis(glm::radians(180.0f), glm::vec3(0, 1, 0));
						if (rawRoll == 1)
							newRoll.finalQuat = glm::quat(1, 0, 0, 0); // Tails
						else
							newRoll.finalQuat = flip180X * rot180Y; // Heads
					} else if (sides == 4) {
						// D4
						switch (std::min(sides, rawRoll)) {
						case 1:
							faceVec = glm::vec3(0, 1, 0);
							break;
						case 2:
							faceVec = glm::vec3(-0.471f, -0.333f, -0.816f);
							break;
						case 3:
							faceVec = glm::vec3(-0.471f, -0.333f, 0.816f);
							break;
						default:
							faceVec = glm::vec3(0.943f, -0.333f, 0.0f);
							break;
						}
						newRoll.finalQuat = matchFaceToCamera(faceVec);
					} else if (sides == 6) {
						// D6
						switch (std::min(sides, rawRoll)) {
						case 1:
							faceVec = glm::vec3(0, 0, 1);
							break;
						case 2:
							faceVec = glm::vec3(0, 1, 0);
							break;
						case 3:
							faceVec = glm::vec3(1, 0, 0);
							break;
						case 4:
							faceVec = glm::vec3(-1, 0, 0);
							break;
						case 5:
							faceVec = glm::vec3(0, -1, 0);
							break;
						case 6:
							faceVec = glm::vec3(0, 0, -1);
							break;
						default:
							faceVec = glm::vec3(0, 1, 0);
							break;
						}
						newRoll.finalQuat = matchFaceToCamera(faceVec);
					} else if (sides == 10) {
						// D10
						switch (std::min(sides, rawRoll)) {
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
						newRoll.finalQuat = matchFaceToCamera(faceVec);
					} else if (sides == 20) {
						// D20
						switch (std::min(sides, rawRoll)) {
						case 20:
							faceVec = glm::vec3(0, 1, 0);
							break;
						case 1:
							faceVec = glm::vec3(0, -1, 0);
							break;
						case 2:
							faceVec = glm::vec3(0.894, 0.447, 0.0);
							break;
						case 8:
							faceVec = glm::vec3(0.276, 0.447, 0.851);
							break;
						case 14:
							faceVec = glm::vec3(-0.724, 0.447, 0.526);
							break;
						case 12:
							faceVec = glm::vec3(-0.724, 0.447, -0.526);
							break;
						case 18:
							faceVec = glm::vec3(0.276, 0.447, -0.851);
							break;
						case 11:
							faceVec = glm::vec3(-0.894, -0.447, 0.0);
							break;
						case 5:
							faceVec = glm::vec3(-0.276, -0.447, -0.851);
							break;
						case 19:
							faceVec = glm::vec3(0.724, -0.447, -0.526);
							break;
						case 3:
							faceVec = glm::vec3(0.724, -0.447, 0.526);
							break;
						case 9:
							faceVec = glm::vec3(-0.276, -0.447, 0.851);
							break;
						case 4:
							faceVec = glm::vec3(0.0, 0.447, 0.894);
							break;
						case 16:
							faceVec = glm::vec3(0.0, 0.447, -0.894);
							break;
						case 7:
							faceVec = glm::vec3(0.851, -0.447, 0.276);
							break;
						case 13:
							faceVec = glm::vec3(-0.851, -0.447, -0.276);
							break;
						case 6:
							faceVec = glm::vec3(0.851, -0.447, -0.276);
							break;
						case 15:
							faceVec = glm::vec3(-0.851, -0.447, 0.276);
							break;
						case 10:
							faceVec = glm::vec3(-0.0, 0.894, 0.447);
							break;
						case 17:
							faceVec = glm::vec3(-0.0, -0.894, -0.447);
							break;
						default:
							faceVec = glm::vec3(0, 1, 0);
							break;
						}
						newRoll.finalQuat = matchFaceToCamera(faceVec);
					}

					// Also set rotation axis for the spinning animation
					newRoll.rotationAxis = glm::vec3(0, 1, 0); // Default Y-axis spin

					activeDiceRolls.push_back(newRoll);
				}

				int hostTotal = 0;
				for (int i = 0; i < (int)tpk->diceNum; ++i) {
					hostTotal += (int)tpk->finalResults[i];
				}
				ofLogNotice("Game") << "TurnStart applied locally: player=" << currentPlayerIndex << " AP total=" << hostTotal;
				currentAP = hostTotal;
				if (startingPlayer.nextTurnAPBonus > 0) {
					currentAP += startingPlayer.nextTurnAPBonus;
					startingPlayer.nextTurnAPBonus = 0;
				}
				// Sync AP to player struct for multiplayer
				if (currentPlayerIndex >= 0 && currentPlayerIndex < (int)players.size()) {
					players[currentPlayerIndex].ap = currentAP;
				}

				// Mark dice as DEBUG to prevent recalculation when animation finishes
				for (auto & roll : activeDiceRolls) {
					if (roll.purpose == PURPOSE_AP && roll.associatedUnit == currentPlayerIndex) {
						roll.purpose = PURPOSE_DEBUG;
					}
				}
			}
			continue; // Done with this packet
		}

		if (header->type == PKT_SNAPSHOT_BEGIN) {
			SnapshotBeginPacket * bp = (SnapshotBeginPacket *)header;
			incomingSnapshotId = bp->snapshotId;
			incomingSnapshotExpectedSize = bp->totalSize;
			incomingSnapshotReceivedSize = 0;
			incomingSnapshotBuffer.assign(bp->totalSize, '\0');
			ofLogNotice("Network") << "Snapshot begin (id=" << incomingSnapshotId << ", bytes=" << incomingSnapshotExpectedSize << ")";
			continue;
		}

		if (header->type == PKT_SNAPSHOT_CHUNK) {
			SnapshotChunkPacket * cp = (SnapshotChunkPacket *)header;
			if (cp->snapshotId == incomingSnapshotId && !incomingSnapshotBuffer.empty()) {
				uint32_t end = cp->offset + cp->chunkSize;
				if (end <= incomingSnapshotBuffer.size()) {
					memcpy(&incomingSnapshotBuffer[cp->offset], cp->data, cp->chunkSize);
					incomingSnapshotReceivedSize += cp->chunkSize;
				}
			}
			continue;
		}

		if (header->type == PKT_SNAPSHOT_END) {
			SnapshotEndPacket * ep = (SnapshotEndPacket *)header;
			if (ep->snapshotId == incomingSnapshotId && incomingSnapshotExpectedSize > 0) {
				ofLogNotice("Network") << "Snapshot end (id=" << incomingSnapshotId << ")";
				applySnapshotString(incomingSnapshotBuffer);
				incomingSnapshotBuffer.clear();
				incomingSnapshotExpectedSize = 0;
				incomingSnapshotReceivedSize = 0;
			}
			continue;
		}

		if (header->type == PKT_HANDSHAKE) {
			HandshakePacket * pkt = (HandshakePacket *)header;
			ofLogNotice("Net") << "Handshake received: type=" << (int)pkt->type << " playerID=" << pkt->playerID << " seq=" << pkt->seq << " seed=" << pkt->seed << " platform=" << MAGEFIGHT_PLATFORM;
			if (hasReceivedHandshake) {
				ofLogNotice("Net") << "Ignoring duplicate handshake (already initialized).";
				continue;
			}

			// FIX: Seed the gameplay RNG and store map seed so derived draft RNG matches the host
			gameplayRNG.seed(pkt->seed);
			currentMapSeed = pkt->seed;
			hasReceivedHandshake = true;
			gameplaySeededByHost = true;
			handshakeRequestInterval = 1.0f;
			isMultiplayer = true;
			myLocalPlayerID = 1;

			// Get Steam names
			player0SteamName = steamManager.getOpponentName(); // Host is opponent for client
			player1SteamName = steamManager.getLocalPlayerName(); // Client is player 1

			// Only initialize game if we're not already in a game (reconnection case)
			if (currentState == STATE_MAIN_MENU) {
				// Initialize game state for the client now that we have the seed.
				ofLogNotice("Network") << "Client: Handshake received. Initializing game. (seed=" << currentMapSeed << ")";
				setupGame();
			} else {
				ofLogNotice("Network") << "Client: Handshake received on reconnect. Staying in current game state: " << currentState;
			}

		} else if (header->type == PKT_ACTION) {
			ActionPacket * pkt = (ActionPacket *)header;

			// Check if this is a movement action (cardIndex < 0) or card play (cardIndex >= 0)
			if (pkt->cardIndex < 0) {
				// MOVEMENT ACTION: Receive opponent's movement and apply it locally
				ofLogNotice("Network") << "Received movement from opponent to (" << pkt->targetX << "," << pkt->targetY << ") with AP=" << pkt->cost;
				int actorIndex = (pkt->actorIndex >= 0 && pkt->actorIndex < (int)players.size()) ? pkt->actorIndex : -1;
				if (actorIndex < 0) {
					for (size_t i = 0; i < players.size(); i++) {
						if (static_cast<uint32_t>(players[i].playerID) == pkt->playerID) {
							actorIndex = (int)i;
							break;
						}
					}
				}
				if (actorIndex >= 0) {
					applyMovement(actorIndex, pkt->targetX, pkt->targetY, pkt->cost, nullptr);
					ofLogNotice("Network") << "Applied movement for player " << pkt->playerID << " to (" << pkt->targetX << "," << pkt->targetY << ")";
				}
			} else {
				// CARD PLAY ACTION: Execute card play
				ofLogNotice("Sync") << "Opponent played card index: " << pkt->cardIndex;
				// EXECUTE REMOTE MOVE
				// Because gameplayRNG is synced, if this card causes a dice roll,
				// it will roll the exact same number here as it did on the opponent's screen.
				executeAction(*pkt);
			}
		} else if (header->type == PKT_DRAW_CARDS) {
			DrawCardsPacket * dcpkt = (DrawCardsPacket *)header;
			ofLogNotice("Network") << "Received DrawCards from opponent: player=" << dcpkt->playerIndex << " num=" << dcpkt->numCards;
			if (isMultiplayer && dcpkt->playerID != static_cast<uint32_t>(myLocalPlayerID)) {
				opponentHasDrawnCardsThisTurn = true;
			}

			// Find the target player
			int targetPlayerIndex = -1;
			for (size_t i = 0; i < players.size(); ++i) {
				if (static_cast<uint32_t>(players[i].playerID) == dcpkt->playerID && !players[i].isMinion) {
					targetPlayerIndex = (int)i;
					break;
				}
			}
			if (targetPlayerIndex < 0) targetPlayerIndex = dcpkt->playerIndex;

			if (targetPlayerIndex >= 0 && targetPlayerIndex < (int)players.size()) {
				Player & p = players[targetPlayerIndex];

				// Add the specific cards to the player's hand using the names from the packet
				for (int i = 0; i < dcpkt->numCards && i < 3; ++i) {
					std::string cardName = dcpkt->cardNames[i];
					if (cardName.empty()) continue;

					// Remove the drawn card from the deck so the deck size stays in sync
					ofLogNotice("Network") << "Before removal: player " << targetPlayerIndex << " deck size=" << p.deck.size() << " searching for '" << cardName << "'";
					int deckSizeBefore = p.deck.size();
					bool foundInDeck = false;
					for (auto it = p.deck.begin(); it != p.deck.end(); ++it) {
						if (it->name == cardName) {
							p.deck.erase(it);
							foundInDeck = true;
							break;
						}
					}
					int deckSizeAfter = p.deck.size();
					if (foundInDeck) {
						ofLogNotice("Network") << "Removed '" << cardName << "' from deck. Size: " << deckSizeBefore << " -> " << deckSizeAfter;
					} else {
						ofLogWarning("Network") << "WARNING: Card '" << cardName << "' NOT FOUND in deck to remove! Deck size=" << deckSizeBefore;
					}

					// Find this card in the master card lists
					Card * foundCard = nullptr;
					for (auto & c : class1Cards) {
						if (c.name == cardName) {
							foundCard = &c;
							break;
						}
					}
					if (!foundCard) {
						for (auto & c : class2Cards) {
							if (c.name == cardName) {
								foundCard = &c;
								break;
							}
						}
					}
					if (!foundCard) {
						for (auto & c : class3Cards) {
							if (c.name == cardName) {
								foundCard = &c;
								break;
							}
						}
					}

					if (foundCard) {
						Card newCard = *foundCard;

						// Animation setup - start small and animate to target
						newCard.currentScale = 0.1f; // Start small
						newCard.targetScale = 1.5f; // Animate to full size

						// Calculate spawn position
						float scale = ofGetHeight() / 1080.0f;
						float staticUICardWidth = (120 * 1.3f) * scale;
						float staticUICardHeight = ((120 * (585.0f / 409.0f)) * 1.3f) * scale;

						// Position based on which player this is
						bool isLocalPlayer = (p.playerID == myLocalPlayerID);
						if (isLocalPlayer) {
							// Draw at bottom (shouldn't happen since this is opponent's packet)
							float deckX = 30 * scale;
							float deckY = ofGetHeight() - staticUICardHeight - (40 * scale) - staticUICardHeight - (40 * scale);
							newCard.currentPos.set(deckX + staticUICardWidth / 2, deckY + staticUICardHeight / 2);
						} else {
							// Draw at top (opponent's position)
							float discardX = ofGetWidth() - staticUICardWidth - (30 * scale);
							float discardY = 40 * scale;
							float deckX = discardX;
							float deckY = discardY + staticUICardHeight + (40 * scale);
							newCard.currentPos.set(deckX + staticUICardWidth / 2, deckY + staticUICardHeight / 2);
						}

						p.hand.push_back(newCard);
						ofLogNotice("Network") << "Added card to player " << targetPlayerIndex << "'s hand: " << cardName;
					} else {
						ofLogWarning("Network") << "Could not find card: " << cardName;
					}
				}
			}
		} else if (header->type == PKT_MENU_STATE) {
			// Opponent menu visualization disabled.
			continue;
		} else if (header->type == PKT_END_TURN) {
			ofLogNotice("Net") << "Opponent ended turn.";
			// CLIENT: Always wait for host's TurnStart packet (contains authoritative dice)
			// Never roll dice locally for any turn - host controls all RNG
			if (isClient()) {
				ofLogNotice("Network") << "CLIENT FIX ACTIVE: Waiting for host TurnStart packet (will NOT roll dice locally).";
				waitingForTurnStartFromHost = true;
				// Apply opponent hand cleanup locally so their hand disappears on our screen
				// NOTE: Do NOT clear defensive stats (block, ward, barrier, holyBlock, fortification) here
				// All defensive stats persist until the opponent's next turn starts in startNewTurn()
				for (size_t i = 0; i < players.size(); i++) {
					Player & opp = players[i];
					if (opp.playerID == static_cast<int>(header->playerID) && !opp.isMinion) {
						opp.discardPile.insert(opp.discardPile.end(), opp.hand.begin(), opp.hand.end());
						opp.hand.clear();
						opp.discardPile.insert(opp.discardPile.end(), opp.playedCardsPile.begin(), opp.playedCardsPile.end());
						opp.playedCardsPile.clear();
						break;
					}
				}
				// Don't call startNewTurn() - let PKT_TURN_START handle it
			} else {
				// Host: can proceed with local turn start
				ofLogNotice("Network") << "Host: Processing END_TURN, calling startNewTurn()";
				startNewTurn();

				// Note: TurnStart packet is now sent from the update loop after dice finish
				// (see line ~3914 where it checks allDiceFinished and sends TurnStartPacket)
				// This ensures the packet contains actual rolled results from continueNewTurn()
			}
		} else if (header->type == PKT_CHECKSUM_CHECK) {
			ChecksumPacket * pkt = (ChecksumPacket *)header;
			// If we're waiting for the host's TurnStart packet, skip checksum validation
			// because we haven't advanced our state yet (we're in a transient waiting state).
			if (isClient() && waitingForTurnStartFromHost) {
				ofLogNotice("Network") << "Client: Skipping checksum validation while waiting for TurnStart.";
				continue;
			}

			// DEBUG: Skip checksum validation when debug features are active
			if (skipChecksumValidation) {
				ofLogNotice("Debug") << "CHECKSUM VALIDATION DISABLED (debug mode)";
				continue;
			}

			long long mySum = calculateChecksum();

			// Log host's deck state for diagnostics
			if (isHost()) {
				ofLogNotice("Checksum") << "Host received checksum from player " << pkt->playerID << ": remote=" << pkt->checksum << " local=" << mySum;
				for (size_t i = 0; i < players.size(); ++i) {
					std::string deckSummary;
					for (const auto & c : players[i].deck) {
						if (!deckSummary.empty()) deckSummary += ",";
						deckSummary += ofToString((int)c.type) + "(" + ofToString((int)c.value) + ")";
					}
					ofLogNotice("Checksum") << "Host P" << i << " deck=[" << deckSummary << "]";
				}
			}

			if (mySum != pkt->checksum) {
				ofLogError("Net") << "DESYNC DETECTED! Local: " << mySum << " Remote: " << pkt->checksum << " Turn: " << pkt->turnNumber;
				// Diagnostic: log per-player deck state to help locate mismatch
				for (size_t i = 0; i < players.size(); ++i) {
					std::string deckSummary;
					for (const auto & c : players[i].deck) {
						if (!deckSummary.empty()) deckSummary += ",";
						deckSummary += ofToString((int)c.type) + "(" + ofToString((int)c.value) + ")";
					}
					ofLogError("Net") << "Player " << i << " id=" << players[i].playerID << " deck=[" << deckSummary << "] hand=" << players[i].hand.size() << " discard=" << players[i].discardPile.size();
				}

				// DESYNC RECOVERY: Try to restore from backup snapshot
				if (!backupSnapshot.empty()) {
					ofLogNotice("Backup") << "Desync detected! Attempting to restore from backup snapshot...";
					applySnapshotString(backupSnapshot);

					// After restoration, verify the state
					long long restoredSum = calculateChecksum();
					ofLogNotice("Backup") << "After restore checksum: " << restoredSum << " (expected to match previous good state)";

					// Reset any pending interaction states that might cause issues
					selectedPieceGridX = -1;
					selectedPieceGridY = -1;
					selectedCardIndex = -1;
					draggedCardIndex = -1;
					hoveredCardIndex = -1;
					playerAction = NONE;

					// Rebuild all visual elements
					buildLevelMesh();
					buildFloorMesh();
					invalidateTargetCache();
					calculateTargetHighlights();

					addGameLog("DESYNC DETECTED: Restored from backup snapshot at turn " + ofToString(globalTurnCounter));
					spawnFloatingText(glm::vec3(0, 5, 0), "Desync Recovered", ofColor::yellow);
					ofLogNotice("Backup") << "Successfully restored game state from backup and rebuilt UI";
				} else {
					// No backup available - show error and go to desync state
					ofLogError("Backup") << "No backup snapshot available for desync recovery!";
					desyncMessage = "DESYNC! Local:" + ofToString(mySum) + " Remote:" + ofToString(pkt->checksum) + " Turn:" + ofToString(pkt->turnNumber);
					currentState = STATE_DESYNC;
					spawnFloatingText(glm::vec3(0, 5, 0), "DESYNC DETECTED", ofColor::red);
				}
			} else {
				// Checksum validated successfully - save verified backup snapshot
				backupSnapshot = buildSnapshotString();
				ofLogNotice("Backup") << "Checksum verified for turn " << pkt->turnNumber << " - saved backup snapshot";
			}
		} else if (header->type == PKT_KEY_PICKUP) {
			KeyPickupPacket * kpkt = (KeyPickupPacket *)header;
			ofLogNotice("Network") << "KeyPickup packet received: player=" << kpkt->playerIndex << " class=" << kpkt->classTier << " pos=(" << kpkt->keyX << "," << kpkt->keyY << ")";

			// CLIENT: Apply key pickup from host
			if (isClient()) {
				// Remove the key from client's floatingKeyInstances
				for (size_t k = 0; k < floatingKeyInstances.size(); ++k) {
					FloatingKey & fk = floatingKeyInstances[k];
					if (fk.pos.x == kpkt->keyX && fk.pos.y == kpkt->keyY) {
						floatingKeyInstances.erase(floatingKeyInstances.begin() + k);
						break;
					}
				}

				// Setup In-Game Draft State (same as host does)
				isInGameDraft = true;
				draftPlayerIndex = kpkt->playerIndex;
				generateDraftOptions(kpkt->classTier);
				draftPicksRemaining = 1;
				selectedDraftIndices.clear();
				currentState = STATE_DRAFTING;

				// If an Accept arrived before this KeyPickup, close immediately
				if (pendingKeyDraftAccept && pendingKeyDraftPlayer == kpkt->playerIndex && pendingKeyDraftClass == kpkt->classTier) {
					pendingKeyDraftAccept = false;
					pendingKeyDraftPlayer = -1;
					pendingKeyDraftClass = 0;
					selectedDraftIndices.clear();
					draftOptions.clear();
					isInGameDraft = false;
					currentState = STATE_GAMEPLAY;
					return;
				}

				spawnFloatingText(gridToWorld(kpkt->keyX, kpkt->keyY), "Key Found!", ofColor::gold);
				ofLogNotice("Key") << "Client: Player " << kpkt->playerIndex << " picked up key (Class " << kpkt->classTier << ")";
			}
		} else if (header->type == PKT_CHAT_MESSAGE) {
			ChatMessagePacket * pkt = (ChatMessagePacket *)header;
			ofLogNotice("Net") << "Received chat message from player " << pkt->playerID << ": " << pkt->message;

			ChatMessage msg;
			// Find player index for this playerID
			int senderIndex = -1;
			for (size_t i = 0; i < players.size(); i++) {
				if (players[i].playerID == static_cast<int>(pkt->playerID) && !players[i].isMinion) {
					senderIndex = i;
					break;
				}
			}
			msg.playerName = (senderIndex >= 0) ? getPlayerSteamName(senderIndex) : ("Player " + ofToString(pkt->playerID));
			msg.message = pkt->message;
			msg.timestamp = ofGetElapsedTimef();
			chatHistory.push_back(msg);
			if (chatHistory.size() > maxChatMessages) {
				chatHistory.erase(chatHistory.begin());
			}
			// Show chat for 5 seconds when message received
			lastChatInteractionTime = ofGetElapsedTimef();
		} else if (header->type == PKT_HOVER) {
			HoverPacket * pkt = (HoverPacket *)header;
			int hoverTypeInt = static_cast<int>(pkt->hoverType);
			if (hoverTypeInt >= HOVER_NONE && hoverTypeInt <= HOVER_UNIT_SELECTED) {
				opponentHoverType = static_cast<HoverType>(hoverTypeInt);
			}
			opponentHoverGridX = static_cast<int>(pkt->gridX);
			opponentHoverGridY = static_cast<int>(pkt->gridY);
			opponentHoverCardIndex = static_cast<int>(pkt->cardIndex);

			// If opponent selected a unit for movement, show their movement highlights
			if (opponentHoverType == HOVER_UNIT_SELECTED) {
				// Store current player state to restore after
				int savedPlayerX = -1, savedPlayerY = -1;
				if (currentPlayerIndex >= 0 && currentPlayerIndex < players.size()) {
					savedPlayerX = players[currentPlayerIndex].x;
					savedPlayerY = players[currentPlayerIndex].y;
					// Temporarily move current player to opponent's selected position
					players[currentPlayerIndex].x = opponentHoverGridX;
					players[currentPlayerIndex].y = opponentHoverGridY;
					calculateHighlights();
					// Restore position
					players[currentPlayerIndex].x = savedPlayerX;
					players[currentPlayerIndex].y = savedPlayerY;
				}
			}
			// If opponent is hovering a card, show their targeting highlights
			else if (opponentHoverType == HOVER_HAND_CARD && opponentHoverCardIndex >= 0) {
				if (currentPlayerIndex >= 0 && currentPlayerIndex < players.size()) {
					Player & currentPlayer = players[currentPlayerIndex];
					if (opponentHoverCardIndex < currentPlayer.hand.size()) {
						calculateTargetHighlights(opponentHoverCardIndex);
					}
				}
			}
			// If opponent cleared hover, clear highlights
			else if (opponentHoverType == HOVER_NONE) {
				clearHighlights();
			}
		} else if (header->type == PKT_DRAFT_STATE) {
			DraftStatePacket * sp = (DraftStatePacket *)header;
			ofLogNotice("Network") << "Draft state received: class=" << sp->classTier << " player=" << sp->draftPlayerIdx << " picks=" << sp->picksRemaining << " stage=" << sp->draftStage << " ingame=" << (int)sp->isInGameDraft << " curPlayer=" << sp->currentPlayerIndex;

			// Ignore late normal-draft packets after the initial draft is complete
			if (initialDraftComplete && currentState == STATE_GAMEPLAY && sp->classTier > 0 && sp->isInGameDraft == 0) {
				ofLogNotice("Draft") << "Ignoring late normal DraftState (initial draft already complete).";
				continue;
			}

			// Client applies host state directly
			draftPlayerIndex = sp->draftPlayerIdx;
			draftStage = sp->draftStage;
			draftPicksRemaining = sp->picksRemaining;
			isInGameDraft = (sp->isInGameDraft != 0);

			if (sp->classTier > 0) {
				// Enter drafting with host-provided class tier
				currentState = STATE_DRAFTING;
				selectedDraftIndices.clear();
				// If multiplayer, wait for host authoritative DraftOptionsPacket; otherwise generate locally
				if (!isMultiplayer) {
					generateDraftOptions(sp->classTier);
				} else {
					// If we already have draftOptions applied that match this state, do not re-enter waiting.
					bool optionsMatch = false;
					if (!draftOptions.empty() && draftPlayerIndex == sp->draftPlayerIdx && draftStage == sp->draftStage && currentDraftClassTier == sp->classTier) {
						optionsMatch = true;
					}
					if (!optionsMatch) {
						// Clear stale options when the host advances the draft state (prevents showing previous class)
						draftOptions.clear();
					}
					if (optionsMatch) {
						waitingForDraftOptions = false;
						ofLogNotice("Draft") << "Client already has authoritative DraftOptions; not waiting (class=" << sp->classTier << ")";
					} else {
						waitingForDraftOptions = true;
						ofLogNotice("Draft") << "Client waiting for authoritative DraftOptionsPacket from host (class=" << sp->classTier << ")";
					}
				}
			} else {
				// classTier==0 => exit drafting and host tells us who is the active player
				draftOptions.clear();
				selectedDraftIndices.clear();
				currentState = STATE_GAMEPLAY;
				initialDraftComplete = true;
				// Host should include who starts; set it
				currentPlayerIndex = sp->currentPlayerIndex;
				// In multiplayer clients: DO NOT call startNewTurn(); wait for host TurnStart packet
				if (isClient()) {
					waitingForTurnStartFromHost = true;
					ofLogNotice("Network") << "Client: Drafting ended. Waiting for TurnStart packet from host (player=" << currentPlayerIndex << ")";
				}
				// Host handles transition in its own draft-accept logic and sends TurnStart
			}
		} else if (header->type == PKT_DRAFT_OPTIONS) {
			DraftOptionsPacket * dp = (DraftOptionsPacket *)header;
			ofLogNotice("Network") << "DraftOptions received: " << dp->optionIndex0 << "," << dp->optionIndex1 << "," << dp->optionIndex2 << " (class=" << dp->classTier << ") draftGenCounter=" << dp->draftGenCounter;

			// Sync the draft generation counter from host
			draftGenerationCounter = dp->draftGenCounter;

			std::vector<int> idxs;
			if (dp->optionIndex0 >= 0) idxs.push_back(dp->optionIndex0);
			if (dp->optionIndex1 >= 0) idxs.push_back(dp->optionIndex1);
			if (dp->optionIndex2 >= 0) idxs.push_back(dp->optionIndex2);
			applyDraftOptionsFromPool(dp->classTier, idxs, dp->picksRemaining, dp->draftPlayerIdx);
			draftStage = dp->draftStage;
			isInGameDraft = (dp->isInGameDraft != 0);
		} else if (header->type == PKT_DRAFT_ACTION) {
			DraftActionPacket * pkt = (DraftActionPacket *)header;
			ofLogNotice("Network") << "Draft action received: type=" << (int)pkt->actionType << " opt=" << pkt->optionIndex << " player=" << pkt->draftPlayerIdx << " sel=" << (int)pkt->selectFlag;

			if (isHost()) {
				// Ignore any draft inputs if we're not actively drafting
				if (currentState != STATE_DRAFTING && !isInGameDraft) {
					ofLogNotice("Network") << "Host: Ignoring draft input outside draft state (type=" << (int)pkt->actionType << ")";
					continue;
				}
				// Host: apply the client's input, then forward to the client(s)
				if (pkt->actionType == 0) {
					// Toggle selection for the current drafting player (in-game drafts wait for Accept)
					int requiredPicks = 1;
					if (!isInGameDraft && draftStage == 0) requiredPicks = 2;
					int opt = pkt->optionIndex;
					if (pkt->selectFlag) {
						auto it = std::find(selectedDraftIndices.begin(), selectedDraftIndices.end(), opt);
						if (it == selectedDraftIndices.end() && (int)selectedDraftIndices.size() < requiredPicks) {
							selectedDraftIndices.push_back(opt);
						}
					} else {
						auto it = std::find(selectedDraftIndices.begin(), selectedDraftIndices.end(), opt);
						if (it != selectedDraftIndices.end()) selectedDraftIndices.erase(it);
					}
					// Forward toggle to clients
					DraftActionPacket outPkt = *pkt;
					steamManager.sendPacket(&outPkt, sizeof(outPkt));
				} else if (pkt->actionType == 1) {
					// Client accepted draft with choices -> apply on host
					ofLogNotice("Draft") << "HOST: Received client AcceptDraft from player=" << pkt->playerID << " draftPlayerIdx=" << pkt->draftPlayerIdx << " picks=" << (int)pkt->numSelected << " indices=" << (int)pkt->selectedIdx0 << "," << (int)pkt->selectedIdx1 << "," << (int)pkt->selectedIdx2;
					int picks = pkt->numSelected;
					std::vector<int> sel;
					if (picks > 0) sel.push_back(pkt->selectedIdx0);
					if (picks > 1) sel.push_back(pkt->selectedIdx1);
					if (picks > 2) sel.push_back(pkt->selectedIdx2);

					Player & p = players[pkt->draftPlayerIdx];
					int copiesPerCard = 1;
					if (!isInGameDraft && pkt->classTier == 1) copiesPerCard = 2;
					const std::vector<Card> * pool = &class1Cards;
					if (pkt->classTier == 2) pool = &class2Cards;
					if (pkt->classTier == 3) pool = &class3Cards;
					for (int idx : sel) {
						if (idx >= 0 && idx < (int)pool->size()) {
							for (int k = 0; k < copiesPerCard; ++k) {
								p.deck.push_back((*pool)[idx]);
							}
						}
					}

					// Forward accept to clients BEFORE any new draft options/state are generated
					DraftActionPacket outPkt = *pkt;
					steamManager.sendPacket(&outPkt, sizeof(outPkt));

					// Now shuffle (sends shuffle packet AFTER Accept packet)
					shuffleGameVector(p.deck, pkt->draftPlayerIdx);

					// Advance host-side draft state
					selectedDraftIndices.clear();
					draftOptions.clear();
					if (isInGameDraft) {
						isInGameDraft = false;
						currentState = STATE_GAMEPLAY;
						return;
					}

					draftStage++;
					if (draftStage == 1) {
						generateDraftOptions(2);
					} else {
						int nextPlayerIdx = (draftPlayerIndex + 1) % 2;
						if (players[nextPlayerIdx].deck.empty()) {
							draftPlayerIndex = nextPlayerIdx;
							draftStage = 0;
							generateDraftOptions(1);
						} else {
							currentPlayerIndex = nextPlayerIdx;
							currentState = STATE_GAMEPLAY;
							// NOTE: Decks were already shuffled when each player accepted their picks.
							// No additional shuffle needed here to avoid desync.
							// For the first turn, call continueNewTurn() directly to avoid incrementing currentPlayerIndex
							continueNewTurn();
						}
					}
					// Also send an additional state sync with currentPlayerIndex to ensure clients transition
					DraftStatePacket sp2;
					sp2.type = PKT_DRAFT_STATE;
					sp2.playerID = myLocalPlayerID;
					if (currentState == STATE_GAMEPLAY)
						sp2.classTier = 0;
					else if (draftStage == 0)
						sp2.classTier = 1;
					else
						sp2.classTier = 2;
					sp2.draftPlayerIdx = (currentState == STATE_GAMEPLAY) ? -1 : draftPlayerIndex;
					sp2.picksRemaining = draftPicksRemaining;
					sp2.draftStage = draftStage;
					sp2.isInGameDraft = isInGameDraft ? 1 : 0;
					sp2.currentPlayerIndex = currentPlayerIndex;
					steamManager.sendPacket(&sp2, sizeof(sp2));
					sp.type = PKT_DRAFT_STATE;
					sp.playerID = myLocalPlayerID;
					// classTier: 0 == none, 1/2 == class tiers
					if (currentState == STATE_GAMEPLAY)
						sp.classTier = 0;
					else if (draftStage == 0)
						sp.classTier = 1;
					else
						sp.classTier = 2;
					sp.draftPlayerIdx = (currentState == STATE_GAMEPLAY) ? -1 : draftPlayerIndex;
					sp.picksRemaining = draftPicksRemaining;
					sp.draftStage = draftStage;
					sp.isInGameDraft = isInGameDraft ? 1 : 0;
					sp.currentPlayerIndex = currentPlayerIndex;
					steamManager.sendPacket(&sp, sizeof(sp));
					// Send TurnStart packet if we transitioned to gameplay
					if (currentState == STATE_GAMEPLAY) {
						std::vector<DiceRoll> newAP;
						for (size_t di = 0; di < activeDiceRolls.size(); ++di) {
							const DiceRoll & dr = activeDiceRolls[di];
							if (dr.associatedUnit == currentPlayerIndex && dr.purpose == PURPOSE_AP) {
								newAP.push_back(dr);
							}
						}
						TurnStartPacket tpk = {};
						tpk.type = PKT_TURN_START;
						tpk.playerID = myLocalPlayerID;
						tpk.currentPlayerIndex = currentPlayerIndex;
						int pkCount = 0;
						int32_t total = 0;
						for (size_t i = 0; i < newAP.size() && pkCount < 8; ++i) {
							tpk.rawResults[pkCount] = (uint8_t)newAP[i].rawResult;
							tpk.finalResults[pkCount] = (uint8_t)newAP[i].result;
							pkCount++;
							total += newAP[i].result;
						}
						tpk.diceNum = (uint8_t)pkCount;
						tpk.diceSides = (uint8_t)(pkCount > 0 ? newAP[0].sides : 6);
						tpk.purpose = (uint8_t)PURPOSE_AP;
						tpk.finalTotal = total;
						steamManager.sendPacket(&tpk, sizeof(tpk));
						ofLogNotice("Network") << "Host sent TurnStart (PKT_DRAFT_ACTION): player=" << tpk.currentPlayerIndex << " dice=" << (int)tpk.diceNum << " total=" << tpk.finalTotal;
						for (int i = 0; i < pkCount; ++i) {
							ofLogNotice("Network") << "  Host sending dice[" << i << "]: raw=" << (int)tpk.rawResults[i] << " final=" << (int)tpk.finalResults[i];
						}

						// DEBUGGING: Log host's checksum at the same moment client will calculate theirs
						if (globalTurnCounter == 0) {
							// Log deck state before checksum
							for (size_t pi = 0; pi < players.size(); ++pi) {
								std::string deckStr = "[";
								for (size_t ci = 0; ci < players[pi].deck.size(); ++ci) {
									if (ci > 0) deckStr += ",";
									deckStr += std::to_string((int)players[pi].deck[ci].type) + "(" + std::to_string(players[pi].deck[ci].value) + ")";
								}
								deckStr += "]";
								ofLogNotice("Checksum") << "Host P" << pi << " deck=" << deckStr;
							}
							int64_t hostChecksum = calculateChecksum();
							ofLogNotice("Checksum") << "Host checksum after TurnStart send (turn 0): " << hostChecksum;
						}
					}
				}
			} else {
				// Client: apply actions forwarded by host
				if (pkt->actionType == 0) {
					if (draftAcceptLocked) {
						return;
					}
					// Host forwarded selection toggle or in-game pick
					if (isInGameDraft) {
						// For in-game drafts, just toggle the selection and show/hide the card highlight
						int opt = pkt->optionIndex;
						if (pkt->selectFlag) {
							auto it = std::find(selectedDraftIndices.begin(), selectedDraftIndices.end(), opt);
							if (it == selectedDraftIndices.end()) selectedDraftIndices.push_back(opt);
						} else {
							auto it = std::find(selectedDraftIndices.begin(), selectedDraftIndices.end(), opt);
							if (it != selectedDraftIndices.end()) selectedDraftIndices.erase(it);
						}
						// Wait for the accept, don't clear draftOptions yet
					} else {
						int opt = pkt->optionIndex;
						if (pkt->selectFlag) {
							auto it = std::find(selectedDraftIndices.begin(), selectedDraftIndices.end(), opt);
							if (it == selectedDraftIndices.end()) selectedDraftIndices.push_back(opt);
						} else {
							auto it = std::find(selectedDraftIndices.begin(), selectedDraftIndices.end(), opt);
							if (it != selectedDraftIndices.end()) selectedDraftIndices.erase(it);
						}
					}
				} else if (pkt->actionType == 1) {
					// Client: host forwarded an Accept. Apply any cards and then WAIT for host authoritative state/options.
					ofLogNotice("Draft") << "CLIENT: Received forwarded AcceptDraft from host player=" << pkt->draftPlayerIdx << " picks=" << (int)pkt->numSelected << " indices=" << (int)pkt->selectedIdx0 << "," << (int)pkt->selectedIdx1 << "," << (int)pkt->selectedIdx2 << " classTier=" << (int)pkt->classTier;
					if (draftAcceptApplied) {
						ofLogNotice("Draft") << "CLIENT: Ignoring duplicate Accept (already applied).";
						return;
					}
					if (initialDraftComplete && currentState == STATE_GAMEPLAY && !isInGameDraft) {
						ofLogNotice("Draft") << "CLIENT: Ignoring late normal Accept after initial draft completed.";
						return;
					}
					draftAcceptApplied = true;
					int picks = pkt->numSelected;
					std::vector<int> sel;
					if (picks > 0) sel.push_back(pkt->selectedIdx0);
					if (picks > 1) sel.push_back(pkt->selectedIdx1);
					if (picks > 2) sel.push_back(pkt->selectedIdx2);

					Player & p = players[pkt->draftPlayerIdx];
					int copiesPerCard = 1;
					if (!isInGameDraft && pkt->classTier == 1) copiesPerCard = 2;
					const std::vector<Card> * pool = &class1Cards;
					if (pkt->classTier == 2) pool = &class2Cards;
					if (pkt->classTier == 3) pool = &class3Cards;
					ofLogNotice("Draft") << "CLIENT: Pool size=" << pool->size() << " copies=" << copiesPerCard;
					for (int idx : sel) {
						if (idx >= 0 && idx < (int)pool->size()) {
							ofLogNotice("Draft") << "CLIENT: Adding card index=" << idx << " name=" << (*pool)[idx].name;
							for (int k = 0; k < copiesPerCard; ++k) {
								p.deck.push_back((*pool)[idx]);
							}
						}
					}
					// In multiplayer clients, mark that we will skip the immediate local shuffle and
					// wait for the host's authoritative `PKT_SHUFFLE` for this player's deck.
					if (isClient()) {
						int pid = pkt->draftPlayerIdx;
						if (pid >= 0 && pid < (int)players.size() && hasPendingShuffleNonce[pid]) {
							std::mt19937 shuffleRng(pendingShuffleNonce[pid]);
							deterministic_shuffle(p.deck, shuffleRng);
							lastAppliedShuffleNonce[pid] = pendingShuffleNonce[pid];
							hasPendingShuffleNonce[pid] = false;
							pendingShuffleNonce[pid] = 0;
							if (skipClientShuffleFor == pid) {
								skipClientShuffleFor = -1;
							}
							ofLogNotice("Network") << "Client: Applied deferred shuffle nonce for player " << pid << " after AcceptDraft";
						} else {
							skipClientShuffleFor = pid;
							shuffleGameVector(p.deck, pid);
						}
					} else {
						shuffleGameVector(p.deck, pkt->draftPlayerIdx);
					}
					selectedDraftIndices.clear();
					if (isInGameDraft) {
						draftOptions.clear();
						isInGameDraft = false;
						currentState = STATE_GAMEPLAY;
						return;
					}

					// If we haven't received the KeyPickup yet, remember this accept so we can close on arrival
					if (currentState != STATE_DRAFTING) {
						pendingKeyDraftAccept = true;
						pendingKeyDraftPlayer = pkt->draftPlayerIdx;
						pendingKeyDraftClass = pkt->classTier;
					}

					waitingForDraftOptions = true;
					waitingForDraftOptionsStartTime = ofGetElapsedTimef();
					ofLogNotice("Draft") << "Client: Received forwarded Accept. Waiting for host state/options. (preserving local options until authoritative packet arrives)";
				}
			}
		}
	}
}

// Send menu state for opponent visualization
void ofApp::sendMenuState(int menuType, int targetIndex, int hoveredChoice, int cardIndex) {
	if (!isMultiplayer) return;
	// Opponent menu visualization is disabled.
	return;
}

// When I click a card
void ofApp::sendActionPacket(int cardIndex, int tx, int ty, int cost, int menuChoice, const std::string & cardNameOverride) {
	// 1. Check if it's my turn or my minion's turn
	if (currentPlayerIndex < 0 || currentPlayerIndex >= (int)players.size()) return;
	const Player & currentPlayer = players[currentPlayerIndex];
	int controlledPlayerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
	if (controlledPlayerID != myLocalPlayerID) return;

	// 2. Create Packet
	ActionPacket pkt = {};
	pkt.type = PKT_ACTION;
	pkt.playerID = myLocalPlayerID;
	pkt.actorIndex = currentPlayerIndex;
	pkt.cardIndex = cardIndex;
	pkt.targetX = tx;
	pkt.targetY = ty;
	pkt.cost = cost;
	pkt.menuChoice = menuChoice;

	// Include card name so opponent knows which card was played
	if (!cardNameOverride.empty()) {
		strncpy(pkt.cardName, cardNameOverride.c_str(), 63);
		pkt.cardName[63] = '\0';
	} else if (cardIndex >= 0 && cardIndex < (int)currentPlayer.hand.size()) {
		strncpy(pkt.cardName, currentPlayer.hand[cardIndex].name.c_str(), 63);
		pkt.cardName[63] = '\0';
	}
	if (pkt.cardName[0] != '\0') {
		ofLogNotice("Network") << "sendActionPacket: Sending card '" << pkt.cardName << "' (cardIndex=" << cardIndex << ") to target=(" << tx << "," << ty << ") cost=" << cost;
	}

	// 3. Send to Network
	steamManager.sendPacket(&pkt, sizeof(pkt));

	// Local execution is handled by the caller (playCard + result handling).
}

void ofApp::sendMagicHandResolutionPacket(int choice) {
	if (!isMultiplayer) return;
	if (currentPlayerIndex < 0 || currentPlayerIndex >= (int)players.size()) return;
	const Player & currentPlayer = players[currentPlayerIndex];
	int controlledPlayerID = currentPlayer.isMinion ? currentPlayer.ownerID : currentPlayer.playerID;
	if (controlledPlayerID != myLocalPlayerID) return;
	if (pendingMagicHandCardIndex < 0 || pendingMagicHandCardIndex >= (int)currentPlayer.hand.size()) return;

	ActionPacket pkt = {};
	pkt.type = PKT_ACTION;
	pkt.playerID = myLocalPlayerID;
	pkt.actorIndex = currentPlayerIndex;
	pkt.cardIndex = pendingMagicHandCardIndex;
	pkt.targetX = magicHandTargetTile.x;
	pkt.targetY = magicHandTargetTile.y;
	pkt.cost = currentPlayer.hand[pendingMagicHandCardIndex].cost;
	pkt.menuChoice = choice;
	strncpy(pkt.cardName, currentPlayer.hand[pendingMagicHandCardIndex].name.c_str(), 63);
	pkt.cardName[63] = '\0';

	ofLogNotice("Network") << "sendMagicHandResolutionPacket: Sending '" << pkt.cardName << "' choice=" << choice
						   << " target=(" << pkt.targetX << "," << pkt.targetY << ") cost=" << pkt.cost;
	steamManager.sendPacket(&pkt, sizeof(pkt));
}

void ofApp::executeAction(const ActionPacket & pkt) {
	// This function runs on BOTH computers.
	// On the sender's PC, it runs immediately via sendActionPacket.
	// On the receiver's PC, it runs via processNetworkPackets.

	// IMPORTANT: Ensure currentPlayerIndex is correct on both machines
	// before calling playCard.

	// If the action came from an opponent (different playerID than current player),
	// we don't have access to their hand, so we need special handling
	if (isMultiplayer && pkt.playerID != static_cast<uint32_t>(myLocalPlayerID) && strlen(pkt.cardName) > 0) {
		// Opponent's card play - apply effect based on card name
		executeOpponentCardPlay(pkt);
	} else {
		// Local player's card play (client-side prediction or single player)
		playCard(pkt.cardIndex, pkt.targetX, pkt.targetY);
	}
}

void ofApp::executeOpponentCardPlay(const ActionPacket & pkt) {
	// Handle opponent's card play by temporarily adding card to their hand and executing full logic
	std::string cardName = pkt.cardName;
	int tx = pkt.targetX;
	int ty = pkt.targetY;

	ofLogNotice("Network") << "executeOpponentCardPlay: Opponent played " << cardName << " at (" << tx << "," << ty << ") actorIndex=" << pkt.actorIndex;

	// Find the acting unit (player or minion)
	int opponentPlayerIndex = -1;
	if (pkt.actorIndex >= 0 && pkt.actorIndex < (int)players.size()) {
		opponentPlayerIndex = pkt.actorIndex;
	} else {
		for (size_t i = 0; i < players.size(); i++) {
			Player & p = players[i];
			if (static_cast<uint32_t>(p.playerID) == pkt.playerID && !p.isMinion) {
				opponentPlayerIndex = (int)i;
				break;
			}
		}
	}
	if (opponentPlayerIndex < 0) {
		ofLogWarning("Network") << "executeOpponentCardPlay: Opponent unit not found!";
		return;
	}

	// Find the card definition from allCards by name
	Card cardDef;
	bool found = false;
	for (const auto & c : allCards) {
		if (c.name == cardName) {
			cardDef = c;
			found = true;
			break;
		}
	}

	// --- ADD DIAGNOSTIC LOG ---
	if (!found) {
		ofLogError("Network") << "CRITICAL: Opponent played card '" << cardName << "' but it was not found in local allCards DB!";
		// Attempt fallback? Or just return to avoid crash.
		return;
	}

	ofLogNotice("Network") << "executeOpponentCardPlay: Card type=" << (int)cardDef.type << " cost=" << cardDef.cost;
	Player & opponentPlayer = players[opponentPlayerIndex];

	// Temporarily swap to opponent's player context
	int savedCurrentPlayerIndex = currentPlayerIndex;
	int savedCurrentAP = currentAP;
	currentPlayerIndex = opponentPlayerIndex;

	// Find the card in opponent's hand (for cards received via DrawCards packet)
	int tempCardIndex = -1;
	for (size_t i = 0; i < opponentPlayer.hand.size(); i++) {
		if (opponentPlayer.hand[i].name == cardName) {
			tempCardIndex = (int)i;
			break;
		}
	}

	// If card not found in hand, add it temporarily (for cards not synced via DrawCards)
	bool addedTemporaryCard = false;
	if (tempCardIndex < 0) {
		opponentPlayer.hand.push_back(cardDef);
		tempCardIndex = (int)opponentPlayer.hand.size() - 1;
		addedTemporaryCard = true;
	}

	// --- FIX START: FORCE AP FOR REMOTE ACTIONS ---
	// The opponent already paid the cost on their screen. We must ensure
	// playCard() doesn't reject it locally due to sync lag.
	currentAP = opponentPlayer.ap;
	if (currentAP < cardDef.cost) {
		ofLogNotice("Sync") << "Forcing AP for opponent action. Local: " << currentAP << " Cost: " << cardDef.cost;
		currentAP = cardDef.cost;
	}
	// Update the player struct so internal checks inside playCard pass
	opponentPlayer.ap = currentAP;
	// --- FIX END ---

	ofLogNotice("Network") << "executeOpponentCardPlay: Executing playCard with cardIndex=" << tempCardIndex << " currentPlayerIndex=" << currentPlayerIndex << " AP=" << currentAP;
	ofLogNotice("Network") << "executeOpponentCardPlay: Hand size before playCard = " << opponentPlayer.hand.size() << ", Played pile size = " << opponentPlayer.playedCardsPile.size();

	// Special-case: resolve Giant Magic Hand using the sender's menu choice
	if (cardDef.type == CARD_GIANT_MAGIC_HAND && pkt.menuChoice != 0) {
		// Track the card play
		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.cardsPlayedThisTurn.push_back(cardDef.type);

		pendingMagicHandCardIndex = tempCardIndex;
		magicHandTargetTile = { tx, ty };
		isMagicHandMenuOpen = false;
		if (pkt.menuChoice == 1) {
			resolveMagicHandPush();
		} else if (pkt.menuChoice == 2) {
			resolveMagicHandPull();
		}

		// Save the updated AP back to the player
		opponentPlayer.ap = currentAP;

		// Remove the card from opponent's hand
		if (tempCardIndex >= 0 && tempCardIndex < (int)opponentPlayer.hand.size()) {
			opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);
		}
		pendingMagicHandCardIndex = -1;

		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Dispel with menuChoice (1=barrier, 2=purge)
	if (cardDef.type == CARD_DISPEL && pkt.menuChoice == 1) {
		// Apply barrier directly without opening menu
		currentAP -= cardDef.cost;
		std::uniform_int_distribution<int> dist(1, 20);
		int rollResult = dist(gameplayRNG);
		opponentPlayer.barrier += rollResult;
		ofLogNotice("Dispel") << "Opponent gained " << rollResult << " barrier";

		opponentPlayer.ap = currentAP;
		// Add to playedCardsPile like normal playCard would, then move to discard at end of turn
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);

		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Train menuChoice (1=AP, 2=Draft)
	if (cardDef.type == CARD_TRAIN && (pkt.menuChoice == 1 || pkt.menuChoice == 2)) {
		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.cardsPlayedThisTurn.push_back(cardDef.type);
		opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);

		if (pkt.menuChoice == 1) {
			opponentPlayer.nextTurnAPBonus += 3;
		} else {
			isInGameDraft = true;
			draftPlayerIndex = currentPlayerIndex;
			generateDraftOptions(1);
			draftPicksRemaining = 1;
			selectedDraftIndices.clear();
			draftStage = 0;
			currentState = STATE_DRAFTING;
		}

		opponentPlayer.ap = currentAP;
		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Dispel purge with encoded status index (menuChoice >= 100)
	if (cardDef.type == CARD_DISPEL && pkt.menuChoice >= 100) {
		int statusIndex = pkt.menuChoice - 100;
		Player * target = nullptr;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == tx && players[i].y == ty) {
				target = &players[i];
				break;
			}
		}
		if (target) {
			std::vector<std::string> statuses;
			if (target->onFire) statuses.push_back("Fire");
			if (target->isParalyzed) statuses.push_back("Paralysis");
			if (statusIndex >= 0 && statusIndex < (int)statuses.size()) {
				const std::string & status = statuses[statusIndex];
				if (status == "Fire") target->onFire = false;
				if (status == "Paralysis") {
					target->isParalyzed = false;
					target->paralysisHeadsCount = 0;
				}
			}
		}

		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);
		opponentPlayer.ap = currentAP;

		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Wisdom Boon with menuChoice (1=confirm)
	if (cardDef.type == CARD_WISDOM_BOON && pkt.menuChoice == 1) {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == tx && players[i].y == ty) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex >= 0) {
			Player * target = getPlayer(targetIndex);
			bool isSelf = (targetIndex == currentPlayerIndex);
			bool isAdjacent = target && (abs(target->x - opponentPlayer.x) + abs(target->y - opponentPlayer.y) == 1);
			if (target && (isSelf || isAdjacent)) {
				int effectValue = (int)opponentPlayer.deck.size();
				if (isSelf) {
					target->block += effectValue;
					tryTriggerShellSpike();
				} else {
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
			}
		}

		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.ap = currentAP;
		opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);

		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Burst of Light with menuChoice (1=damage, 2=heal)
	if (cardDef.type == CARD_BURST_OF_LIGHT && (pkt.menuChoice == 1 || pkt.menuChoice == 2)) {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == tx && players[i].y == ty) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex >= 0) {
			Player * target = getPlayer(targetIndex);
			int casterOwner = opponentPlayer.isMinion ? opponentPlayer.ownerID : opponentPlayer.playerID;
			int targetOwner = target->isMinion ? target->ownerID : target->playerID;
			if (pkt.menuChoice == 1) {
				if (casterOwner != targetOwner) {
					applyDamageTo(*target, 3, DAMAGE_HOLY, currentPlayerIndex);
				}
			} else {
				if (casterOwner == targetOwner) {
					int healAmt = 3;
					target->health = std::min(target->maxHealth, target->health + healAmt);
				}
			}
		}

		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.ap = currentAP;
		opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);

		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Double Handed with menuChoice (1=Punch, 2=Hand Block)
	if (cardDef.type == CARD_DOUBLE_HANDED && (pkt.menuChoice == 1 || pkt.menuChoice == 2)) {
		int targetIndex = -1;
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].x == tx && players[i].y == ty) {
				targetIndex = (int)i;
				break;
			}
		}
		if (targetIndex >= 0) {
			currentAP -= cardDef.cost;
			opponentPlayer.playedCardsPile.push_back(cardDef);
			if (opponentPlayer.isReplicatePending) {
				opponentPlayer.playedCardsPile.push_back(cardDef);
				opponentPlayer.isReplicatePending = false;
			}
			opponentPlayer.cardsPlayedThisTurn.push_back(cardDef.type);
			// DO NOT erase here - resolveDoubleHanded() handles card removal
			pendingDoubleHandedCardIndex = tempCardIndex;
			pendingDoubleHandedTargetIndex = targetIndex;
			resolveDoubleHanded(pkt.menuChoice == 1 ? "Punch" : "Hand Block");
		}

		opponentPlayer.ap = currentAP;

		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Chain Lightning (target selected in packet)
	if (cardDef.type == CARD_CHAIN_LIGHTNING) {
		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.cardsPlayedThisTurn.push_back(cardDef.type);
		opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);

		pendingChainLightningTargetTile = glm::vec2(tx, ty);
		pendingChainLightningRangeResult = startDiceRoll(2, 10, PURPOSE_RANGE, "Chain Lightning: Range");
		isWaitingForChainLightningRange = true;

		opponentPlayer.ap = currentAP;
		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Teleport (apply move from packet)
	if (cardDef.type == CARD_TELEPORT) {
		// Track the card play
		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.cardsPlayedThisTurn.push_back(cardDef.type);

		// Move player
		board[opponentPlayer.x][opponentPlayer.y].hasPlayer = false;
		opponentPlayer.x = tx;
		opponentPlayer.y = ty;
		board[tx][ty].hasPlayer = true;
		playerVisualPos = gridToWorld(tx, ty);
		invalidateTargetCache();

		opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);
		opponentPlayer.ap = currentAP;
		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Special-case: Renewed Inspiration (menu already resolved on sender)
	if (cardDef.type == CARD_RENEWED_INSPIRATION) {
		currentAP -= cardDef.cost;
		opponentPlayer.playedCardsPile.push_back(cardDef);
		if (opponentPlayer.isReplicatePending) {
			opponentPlayer.playedCardsPile.push_back(cardDef);
			opponentPlayer.isReplicatePending = false;
		}
		opponentPlayer.cardsPlayedThisTurn.push_back(cardDef.type);
		if (tempCardIndex >= 0 && tempCardIndex < (int)opponentPlayer.hand.size()) {
			opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);
		}
		createCardDisplay(cardDef, opponentPlayerIndex);
		opponentPlayer.ap = currentAP;
		currentPlayerIndex = savedCurrentPlayerIndex;
		currentAP = savedCurrentAP;
		return;
	}

	// Execute the card play using the normal playCard logic
	// We rely on the result to know if we need to clean up manual AP/Hand state
	CardPlayResult result = playCard(tempCardIndex, tx, ty);

	ofLogNotice("Network") << "executeOpponentCardPlay: After playCard result=" << result << " Hand size=" << opponentPlayer.hand.size() << " Played pile size=" << opponentPlayer.playedCardsPile.size();

	if (result == CARD_NOT_PLAYABLE) {
		ofLogError("Network") << "Opponent playCard failed locally! Sync issue likely.";
		// Force cleanup since playCard didn't consume it
		if (opponentPlayer.hand.size() > static_cast<size_t>(tempCardIndex)) {
			opponentPlayer.hand.erase(opponentPlayer.hand.begin() + tempCardIndex);
		}
	} else {
		// playCard succeeded. It consumed the AP and removed the card from hand.
		// We just need to update our local tracker of the opponent's AP.
		opponentPlayer.ap = currentAP;
	}

	// Restore current player context
	currentPlayerIndex = savedCurrentPlayerIndex;
	currentAP = savedCurrentAP;
}

// Verify sync
long long ofApp::calculateChecksum() {
	// FNV-1a 64-bit
	const uint64_t FNV_OFFSET = 14695981039346656037ULL;
	const uint64_t FNV_PRIME = 1099511628211ULL;
	uint64_t h = FNV_OFFSET;
	auto mix = [&](uint64_t v) {
		h ^= v;
		h *= FNV_PRIME;
	};

	// Global counters
	mix((uint64_t)globalTurnCounter);
	mix((uint64_t)currentPlayerIndex);

	// Player state (only shared state - decks differ per player)
	for (const auto & p : players) {
		mix((uint64_t)p.playerID);
		mix((uint64_t)p.x);
		mix((uint64_t)p.y);
		mix((uint64_t)p.health);
		mix((uint64_t)p.maxHealth);
		mix((uint64_t)p.block);
		mix((uint64_t)p.ward);
		mix((uint64_t)p.fortification);
		mix((uint64_t)p.barrier);
		mix((uint64_t)p.holyBlock);
		mix((uint64_t)p.luck);
		mix((uint64_t)p.bonusTurns);
		mix((uint64_t)p.ap);
		mix((uint64_t)p.onFire);
		mix((uint64_t)p.isParalyzed);
		mix((uint64_t)p.paralysisHeadsCount);
		mix((uint64_t)p.isPoisoned);
		mix((uint64_t)p.poisonReduction);
		mix((uint64_t)p.nextTurnAPBonus);
		mix((uint64_t)p.nextAttackAddPoison);
		mix((uint64_t)p.nextTurnD10AP);
		mix((uint64_t)p.nextTurnExtraDraw);
		mix((uint64_t)p.isReplicatePending);
		mix((uint64_t)p.nextTurnBonusDiceFromMinions);
		mix((uint64_t)p.strengthenElementsTurnsRemaining);
		mix((uint64_t)p.sleepTurnsRemaining);
		mix((uint64_t)p.summonedOnTurnCycle);
		mix((uint64_t)p.inTortoiseForm);
		mix((uint64_t)p.tortoiseDamageTaken);
		mix((uint64_t)p.inGhostForm);
		mix((uint64_t)p.ghostDamageTaken);
		mix((uint64_t)p.freeKickTurns);

		// NOTE: We do NOT include deck/discard/hand sizes in checksum because:
		// 1. Network packet timing causes desyncs (DrawCards packets arrive after checksum)
		// 2. Opponent's deck/hand are hidden information anyway
		// 3. Cards are synchronized via explicit DrawCards/PlayCard packets
	}

	// Active dice (include resolved outcomes)
	// NOTE: During turn 0 (post-draft TurnStart), skip active dice in checksum
	// because the host rolls AP but client hasn't received the TurnStart packet yet.
	// Dice will be synchronized via explicit AP value in TurnStart packet.
	if (globalTurnCounter > 0) {
		mix((uint64_t)activeDiceRolls.size());
		for (const auto & d : activeDiceRolls) {
			mix((uint64_t)d.purpose);
			mix((uint64_t)d.result);
			mix((uint64_t)d.rawResult);
			mix((uint64_t)d.associatedUnit);
		}
	}

	return (long long)h;
}
//--------------------------------------------------------------
void ofApp::applyMovement(int playerIndex, int targetX, int targetY, int newAP, const std::vector<glm::vec2> * pathOverride) {
	if (playerIndex < 0 || playerIndex >= (int)players.size()) return;
	Player & p = players[playerIndex];
	if (targetX < 0 || targetX >= BOARD_WIDTH || targetY < 0 || targetY >= BOARD_HEIGHT) return;

	const int prevX = p.x;
	const int prevY = p.y;

	// Log the movement
	addGameLog(getPlayerSteamName(playerIndex) + " moved to (" + ofToString(targetX) + "," + ofToString(targetY) + ")");

	board[prevX][prevY].hasPlayer = false;

	if (newAP >= 0) {
		currentAP = newAP;
		p.ap = newAP;
	}

	// Build animation path
	animationPath.clear();
	currentPathIndex = 0;
	glm::vec3 startPos = gridToWorld(prevX, prevY);
	glm::vec3 endPos = gridToWorld(targetX, targetY);
	playerVisualPos = startPos;
	animationPath.push_back(startPos);

	if (pathOverride && pathOverride->size() > 1) {
		for (size_t i = 1; i < pathOverride->size(); ++i) {
			animationPath.push_back(gridToWorld((int)(*pathOverride)[i].x, (int)(*pathOverride)[i].y));
		}
	} else {
		std::vector<glm::vec2> path = findShortestPathForPlayer(playerIndex, { (float)prevX, (float)prevY }, { (float)targetX, (float)targetY });
		if (path.size() > 1) {
			for (size_t i = 1; i < path.size(); ++i) {
				animationPath.push_back(gridToWorld((int)path[i].x, (int)path[i].y));
			}
		} else {
			animationPath.push_back(endPos);
		}
	}

	if (!animationPath.empty()) {
		isPlayerAnimating = true;
		animatingPlayerIndex = playerIndex;
	}

	board[targetX][targetY].hasPlayer = true;
	p.x = targetX;
	p.y = targetY;

	invalidateTargetCache();
}
//--------------------------------------------------------------
// Anti-cheat: Get deck state as string for logging
std::string ofApp::getDeckStateString(const Player & p) {
	std::string result = "Player" + std::to_string(p.playerID) + " Deck[" + std::to_string(p.deck.size()) + "]: ";
	for (size_t i = 0; i < p.deck.size(); ++i) {
		if (i > 0) result += ", ";
		result += p.deck[i].name + "(" + std::to_string((int)p.deck[i].type) + ")";
	}
	result += " | Hand[" + std::to_string(p.hand.size()) + "]: ";
	for (size_t i = 0; i < p.hand.size(); ++i) {
		if (i > 0) result += ", ";
		result += p.hand[i].name + "(" + std::to_string((int)p.hand[i].type) + ")";
	}
	result += " | Discard[" + std::to_string(p.discardPile.size()) + "]: ";
	for (size_t i = 0; i < p.discardPile.size(); ++i) {
		if (i > 0) result += ", ";
		result += p.discardPile[i].name + "(" + std::to_string((int)p.discardPile[i].type) + ")";
	}
	return result;
}
//--------------------------------------------------------------
// Anti-cheat: Log all player deck states to file and console
void ofApp::logDeckStates(const std::string & reason) {
	std::string timestamp = ofGetTimestampString("%Y-%m-%d %H:%M:%S");
	std::string logEntry = "\n=== DECK STATE LOG ===\n";
	logEntry += "Time: " + timestamp + "\n";
	logEntry += "Reason: " + reason + "\n";
	logEntry += "Turn: " + std::to_string(globalTurnCounter) + "\n";
	logEntry += "Current Player: " + std::to_string(currentPlayerIndex) + "\n";
	logEntry += "Local Player ID: " + std::to_string(myLocalPlayerID) + "\n";
	logEntry += "Is Host: " + std::string(steamManager.isHost() ? "true" : "false") + "\n\n";

	for (const auto & p : players) {
		logEntry += getDeckStateString(p) + "\n";
	}
	logEntry += "=====================\n";

	// Log to console
	ofLogNotice("DeckState") << logEntry;

	// Append to file
	std::string filename = "deck_states_" + std::string(steamManager.isHost() ? "host" : "client") + ".log";
	ofBuffer buffer;
	buffer.set(logEntry.c_str(), logEntry.size());
	ofBufferToFile(filename, buffer, true); // true = append mode
}
//--------------------------------------------------------------