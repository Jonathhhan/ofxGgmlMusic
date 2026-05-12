#pragma once

#include "ofMain.h"
#include "ofxGgmlMusic.h"
#include "ofxImGui.h"

#include <array>
#include <memory>
#include <string>

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
	void loadWaveform();
	void drawWaveform(float x, float y, float width, float height);
	std::string getOutputPath() const;

	ofxImGui::Gui gui;
	std::unique_ptr<ofxGgmlMusicGenerationBackend> backend;
	ofxGgmlMusicGenerationRequest request;
	ofxGgmlMusicGenerationResult lastResult;
	ofxGgmlMusicAudioBuffer waveform;
	ofSoundPlayer player;
	std::array<char, 512> promptBuffer{};
	std::array<char, 64> styleBuffer{};
	std::string status;
	std::string detail;
	float tempo = 92.0f;
	float duration = 8.0f;
	int seed = 42;
	int presetIndex = 0;
	int tonicIndex = 0;
	int modeIndex = 0;
	bool loop = true;
	bool autoPlay = true;
	bool exportMelodyStem = true;
	bool exportBassStem = true;
	bool exportPulseStem = false;
};
