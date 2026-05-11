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

	ofxGgmlMusicGenerationRequest generation;
	if (ofxGgmlMusicUtils::hasPrompt(generation)) {
		std::cerr << "empty generation request reported as configured\n";
		return 1;
	}
	generation.prompt = "loopable ambient piano with soft granular texture";
	generation.negativePrompt = "vocals, drums";
	generation.style = "ambient";
	generation.outputPath = "renders/ambient.wav";
	generation.settings.backend = ofxGgmlMusicGenerationBackend::GAN;
	generation.settings.durationSeconds = 12.0;
	generation.settings.guidance = 4.0f;
	generation.settings.seed = 42;
	generation.settings.loop = true;
	generation.tempo.bpm = 92.0f;
	generation.key.tonic = "D";
	generation.key.mode = "major";
	generation.targetStems = { "piano", "texture" };
	if (!ofxGgmlMusicUtils::hasPrompt(generation) ||
		!ofxGgmlMusicUtils::hasTempo(generation) ||
		!ofxGgmlMusicUtils::hasKey(generation)) {
		std::cerr << "generation request helpers failed\n";
		return 1;
	}
	const auto generationDescription = ofxGgmlMusicUtils::describe(generation);
	if (generationDescription.find("ambient piano") == std::string::npos ||
		generationDescription.find("gan") == std::string::npos ||
		generationDescription.find("92 bpm") == std::string::npos ||
		generationDescription.find("D major") == std::string::npos) {
		std::cerr << "generation description missing prompt/tempo/key\n";
		return 1;
	}

	ofxGgmlMusicGenerationResult generationResult;
	if (ofxGgmlMusicUtils::hasOutput(generationResult)) {
		std::cerr << "empty generation result reported as configured\n";
		return 1;
	}
	generationResult.success = true;
	generationResult.outputPath = generation.outputPath;
	generationResult.durationSeconds = generation.settings.durationSeconds;
	generationResult.seed = generation.settings.seed;
	generationResult.tempo = generation.tempo;
	generationResult.key = generation.key;
	generationResult.stems.push_back({ "piano", "renders/piano.wav", 1.0f });
	if (ofxGgmlMusicUtils::getGenerationBackendName(generation.settings.backend) != "gan" ||
		!generationResult ||
		!ofxGgmlMusicUtils::hasOutput(generationResult) ||
		!ofxGgmlMusicUtils::hasTempo(generationResult) ||
		!ofxGgmlMusicUtils::hasKey(generationResult) ||
		generationResult.stems.front().path != "renders/piano.wav") {
		std::cerr << "generation result helpers failed\n";
		return 1;
	}

	return 0;
}
