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

struct ofxGgmlMusicRequest {
	std::string audioPath;
	ofxGgmlMusicTask task = ofxGgmlMusicTask::Analysis;
	std::string prompt;
	std::vector<std::string> tags;
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
