#pragma once

#include <string>

class GameApp {
public:
	bool initialize(int width, int height, const std::string & title);
	void run();
	void shutdown();

private:
	void update();
	void draw();

	bool initialized_ = false;
	bool running_ = true;
	int screenWidth_ = 1280;
	int screenHeight_ = 720;
};
