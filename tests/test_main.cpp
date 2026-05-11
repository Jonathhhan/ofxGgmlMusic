#include "ofxGgmlMusic.h"

#include <iostream>

int main() {
	ofxGgmlMusicRequest request;
	if (ofxGgmlMusicUtils::hasInput(request)) {
		std::cerr << "empty request reported as configured\n";
		return 1;
	}

	request.audioPath = "audio/example.wav";
	request.task = ofxGgmlMusicTask::BeatTracking;
	if (!ofxGgmlMusicUtils::hasInput(request)) {
		std::cerr << "configured request reported as empty\n";
		return 1;
	}

	const auto description = ofxGgmlMusicUtils::describe(request);
	if (description.find(request.audioPath) == std::string::npos ||
		description.find("beat tracking") == std::string::npos) {
		std::cerr << "description did not include request input/task\n";
		return 1;
	}

	if (ofxGgmlMusicUtils::getTaskName(ofxGgmlMusicTask::ChordRecognition) != "chord recognition") {
		std::cerr << "unexpected task name\n";
		return 1;
	}

	ofxGgmlMusicResult result;
	if (ofxGgmlMusicUtils::hasTempo(result) || ofxGgmlMusicUtils::hasKey(result)) {
		std::cerr << "empty result reported as configured\n";
		return 1;
	}
	result.tempo.bpm = 128.0f;
	result.tempo.confidence = 0.9f;
	result.key.tonic = "C";
	result.key.mode = "minor";
	result.beats.push_back({ 0.5, 0.8f, true });
	result.chords.push_back({ 0.5, "Cm7", 0.7f });
	result.embedding = { 0.1f, 0.2f, 0.3f };
	result.stems.push_back({ "drums", "stems/drums.wav", 1.0f });
	if (!ofxGgmlMusicUtils::hasTempo(result) ||
		!ofxGgmlMusicUtils::hasKey(result) ||
		ofxGgmlMusicUtils::formatKey(result.key) != "C minor" ||
		result.beats.empty() ||
		!result.beats.front().downbeat ||
		result.chords.front().label != "Cm7" ||
		result.embedding.size() != 3 ||
		result.stems.front().name != "drums") {
		std::cerr << "music result helpers failed\n";
		return 1;
	}

	return 0;
}
