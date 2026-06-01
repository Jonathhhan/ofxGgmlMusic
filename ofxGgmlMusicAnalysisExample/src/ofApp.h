#pragma once

#include "ofMain.h"
#include "ofxGgmlMusic.h"
#include "ofxImGui.h"

#include <array>
#include <string>
#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	void rebuildRequest();
	void refreshPreviewResult();
	void cycleTask();
	void drawPreviewTimeline(float x, float y, float width, float height);
	void updateStatus();
	void logRequest() const;
	std::string getRequestSummary() const;
	std::string getResultSummary() const;

	ofxGgmlMusicRequest request;
	ofxGgmlMusicResult previewResult;
	std::array<char, 512> audioPathBuffer{};
	std::array<char, 512> promptBuffer{};
	std::array<char, 256> tagsBuffer{};
	std::vector<ofxGgmlMusicTask> taskValues;
	std::vector<std::string> taskNames;
	std::string status;
	std::string detail;
	int taskIndex = 0;
	float previewDurationSeconds = 12.0f;
	ofxImGui::Gui gui;
};
