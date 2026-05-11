#pragma once

#include <string>
#include <vector>

enum class ofxGgmlMusicTask {
	Analysis,
	Tempo,
	BeatTracking,
	KeyDetection,
	ChordRecognition,
	Embedding,
	StemSeparation,
	Generation
};

enum class ofxGgmlMusicGenerationBackend {
	Auto,
	GAN,
	Diffusion,
	Transformer,
	SampleRNN,
	External
};

struct ofxGgmlMusicBeat {
	double timeSeconds = 0.0;
	float confidence = 0.0f;
	bool downbeat = false;
};

struct ofxGgmlMusicTempo {
	float bpm = 0.0f;
	float confidence = 0.0f;
};

struct ofxGgmlMusicKey {
	std::string tonic;
	std::string mode;
	float confidence = 0.0f;
};

struct ofxGgmlMusicChord {
	double timeSeconds = 0.0;
	std::string label;
	float confidence = 0.0f;
};

struct ofxGgmlMusicStem {
	std::string name;
	std::string path;
	float gain = 1.0f;
};

struct ofxGgmlMusicGenerationSettings {
	ofxGgmlMusicGenerationBackend backend = ofxGgmlMusicGenerationBackend::Auto;
	double durationSeconds = 8.0;
	float guidance = 3.0f;
	int seed = -1;
	bool loop = false;
};

struct ofxGgmlMusicRequest {
	std::string audioPath;
	ofxGgmlMusicTask task = ofxGgmlMusicTask::Analysis;
	std::string prompt;
	std::vector<std::string> tags;
};

struct ofxGgmlMusicGenerationRequest {
	std::string prompt;
	std::string negativePrompt;
	std::string style;
	std::string referenceAudioPath;
	std::string outputPath;
	ofxGgmlMusicTempo tempo;
	ofxGgmlMusicKey key;
	ofxGgmlMusicGenerationSettings settings;
	std::vector<std::string> targetStems;
};

struct ofxGgmlMusicResult {
	bool success = false;
	std::string text;
	std::string error;
	ofxGgmlMusicTempo tempo;
	ofxGgmlMusicKey key;
	std::vector<ofxGgmlMusicBeat> beats;
	std::vector<ofxGgmlMusicChord> chords;
	std::vector<float> embedding;
	std::vector<ofxGgmlMusicStem> stems;
	std::vector<std::string> references;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlMusicGenerationResult {
	bool success = false;
	std::string outputPath;
	std::string error;
	double durationSeconds = 0.0;
	int seed = -1;
	ofxGgmlMusicTempo tempo;
	ofxGgmlMusicKey key;
	std::vector<ofxGgmlMusicStem> stems;
	std::vector<std::string> references;

	explicit operator bool() const {
		return success;
	}
};
