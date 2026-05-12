#pragma once

#include "ofMain.h"
#include "ofxGgmlMusic.h"
#include "ofxImGui.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	void rebuildRequest();
	void runGeneration();
	void applyPreset(int index);
	void syncControlsFromRequest();
	void loadExistingRender();
	void loadRenderManifest(const std::string & manifestPath);
	void refreshGenerationHistory();
	void loadWaveform();
	void drawWaveform(float x, float y, float width, float height);
	std::string getOutputDirectory() const;
	std::string getOutputPath() const;
	std::string getNextOutputPath();
	std::string getManifestPath() const;
	std::string getHistoryPath() const;
	std::string getPlayablePath() const;

	ofxImGui::Gui gui;
	std::unique_ptr<ofxGgmlMusicGenerationBackend> backend;
	ofxGgmlMusicGenerationRequest request;
	ofxGgmlMusicGenerationResult lastResult;
	ofxGgmlMusicAudioBuffer waveform;
	ofSoundPlayer player;
	std::array<char, 512> promptBuffer{};
	std::array<char, 64> styleBuffer{};
	std::vector<std::string> presetNames;
	std::vector<std::string> historyManifestPaths;
	std::string currentOutputPath;
	std::string status;
	std::string detail;
	float tempo = 92.0f;
	float duration = 8.0f;
	int seed = 42;
	int presetIndex = 0;
	int historyIndex = 0;
	int renderSerial = 1;
	int tonicIndex = 0;
	int modeIndex = 0;
	bool loop = true;
	bool autoPlay = true;
	bool exportMelodyStem = true;
	bool exportBassStem = true;
	bool exportPulseStem = false;
};
