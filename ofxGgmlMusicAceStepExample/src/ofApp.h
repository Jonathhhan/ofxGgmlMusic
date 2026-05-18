#pragma once

#include "ofMain.h"
#include "ofxGgmlMusic.h"
#include "ofxGgmlMusic/ofxGgmlMusicAceStepBridge.h"
#include "ofxImGui.h"

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;
	void keyPressed(int key) override;

private:
	void requestServerStart();
	void requestHealth();
	void requestGeneration();
	void runServerStartWorker(
		std::string serverUrl,
		std::string serverExecutable,
		std::string modelPath);
	void runHealthWorker(std::string serverUrl);
	void runGenerationWorker(ofxGgmlMusicAceStepRequest request, std::string serverUrl);
	void collectWorkerResult();
	void loadGeneratedAudio(const std::string & path);
	void drawWaveform(float x, float y, float width, float height);
	std::string getOutputDirectory() const;
	ofxGgmlMusicAceStepRequest buildRequest() const;

	ofxImGui::Gui gui;
	ofxGgmlMusicAceStepBridge bridge;
	ofxGgmlMusicAceStepGenerateResult lastGenerateResult;
	ofxGgmlMusicAceStepHealthResult lastHealthResult;
	ofxGgmlMusicAudioBuffer waveform;
	ofSoundPlayer player;

	std::array<char, 256> serverUrlBuffer{};
	std::array<char, 512> serverExecutableBuffer{};
	std::array<char, 512> modelPathBuffer{};
	std::array<char, 2048> captionBuffer{};
	std::array<char, 2048> lyricsBuffer{};
	std::array<char, 256> negativePromptBuffer{};
	std::array<char, 64> keyscaleBuffer{};
	std::array<char, 64> timeSignatureBuffer{};
	std::array<char, 128> outputPrefixBuffer{};
	float durationSeconds = 30.0f;
	int bpm = 0;
	int seed = -1;
	int batchSize = 1;
	float lmTemperature = 0.85f;
	float lmCfgScale = 2.0f;
	float lmTopP = 0.9f;
	int lmTopK = 0;
	bool instrumentalOnly = true;
	bool useCotCaption = true;
	bool wavOutput = true;
	bool autoPlay = true;

	std::thread workerThread;
	std::mutex workerMutex;
	std::atomic<bool> workerRunning{false};
	bool pendingServerStart = false;
	bool pendingServerStartSuccess = false;
	std::string pendingServerStartDetail;
	bool pendingHealth = false;
	bool pendingGenerate = false;
	ofxGgmlMusicAceStepHealthResult pendingHealthResult;
	ofxGgmlMusicAceStepGenerateResult pendingGenerateResult;
	std::string status;
	std::string detail;
};
