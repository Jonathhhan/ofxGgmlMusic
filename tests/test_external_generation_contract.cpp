#include "ofxGgmlMusic.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
	bool fileStartsWith(const std::filesystem::path & path, const std::string & text) {
		std::ifstream input(path, std::ios::binary);
		if (!input) {
			return false;
		}
		std::string bytes(text.size(), '\0');
		input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		return bytes == text;
	}

	void removeIfPresent(const std::string & path) {
		if (path.empty()) {
			return;
		}
		std::error_code code;
		std::filesystem::remove(std::filesystem::path(path), code);
	}
}

int main(int argc, char ** argv) {
	if (argc < 2) {
		std::cerr << "usage: music_external_contract <ofxGgmlMusicGenerate executable>\n";
		return 2;
	}

	const std::filesystem::path generatorPath(argv[1]);
	if (!std::filesystem::exists(generatorPath)) {
		std::cerr << "generator executable was not found: " << generatorPath.string() << "\n";
		return 2;
	}

	const auto outputPath =
		std::filesystem::temp_directory_path() / "ofxGgmlMusic-external-contract.wav";
	removeIfPresent(outputPath.string());
	removeIfPresent(outputPath.string() + ".json");
	removeIfPresent((std::filesystem::temp_directory_path() / "ofxGgmlMusic-history.json").string());

	ofxGgmlMusicGenerationRequest request;
	request.prompt = "short deterministic ambient contract tone";
	request.outputPath = outputPath.string();
	request.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	request.settings.durationSeconds = 1.0;
	request.settings.seed = 123;
	request.tempo.bpm = 96.0f;
	request.key.tonic = "C";
	request.key.mode = "major";
	request.targetStems = { "melody", "bass" };
	request.external.executablePath = generatorPath.string();
	request.external.modelPath = "mock-musicgen-model-id";
	request.external.requireModelPathExists = false;
	request.external.modelFlag.clear();
	request.external.extraArguments = {
		"--preset",
		"ambient",
		"--tempo",
		"96",
		"--key",
		"C",
		"--mode",
		"major",
		"--stem",
		"melody",
		"--stem",
		"bass"
	};

	auto backend = ofxGgmlMakeExternalMusicGenerationBackend();
	const auto setupResult = backend->setup(request);
	if (!setupResult || !backend->isLoaded()) {
		std::cerr << "external backend setup failed: " << setupResult.error << "\n";
		return 1;
	}

	const auto result = backend->generate(request);
	if (!result) {
		std::cerr << "external backend generation failed: " << result.error << "\n";
		return 1;
	}

	ofxGgmlMusicAudioBuffer buffer;
	std::string wavError;
	if (result.outputPath != request.outputPath ||
		result.manifestPath != ofxGgmlMusicUtils::getGenerationManifestPath(request.outputPath) ||
		result.historyPath != ofxGgmlMusicUtils::getGenerationHistoryPath(request.outputPath) ||
		result.durationSeconds <= 0.0 ||
		result.sampleRate != 44100 ||
		result.channels != 1 ||
		result.peakAbs <= 0.0f ||
		result.references.empty() ||
		!std::filesystem::exists(outputPath) ||
		!std::filesystem::exists(result.manifestPath) ||
		!std::filesystem::exists(result.historyPath) ||
		!fileStartsWith(outputPath, "RIFF") ||
		!ofxGgmlMusicAudioUtils::loadWav16(outputPath.string(), buffer, wavError) ||
		!buffer ||
		buffer.getPeakAbs() <= 0.0f) {
		std::cerr << "external backend did not produce a readable generation artifact\n";
		return 1;
	}

	ofxGgmlMusicGenerationResult manifest;
	std::string manifestError;
	if (!ofxGgmlMusicUtils::loadGenerationManifest(result.manifestPath, manifest, manifestError) ||
		!manifest ||
		manifest.outputPath != result.outputPath ||
		manifest.sampleRate != result.sampleRate ||
		manifest.beats.empty() ||
		manifest.chords.empty() ||
		manifest.sections.empty() ||
		manifest.stems.size() != 2) {
		std::cerr << "external generation manifest did not round-trip: " << manifestError << "\n";
		return 1;
	}

	bool hasExecutableReference = false;
	bool hasModelReference = false;
	for (const auto & reference : result.references) {
		if (reference.find(generatorPath.filename().string()) != std::string::npos ||
			reference.find(generatorPath.string()) != std::string::npos) {
			hasExecutableReference = true;
		}
		if (reference.find("mock-musicgen-model-id") != std::string::npos) {
			hasModelReference = true;
		}
	}
	if (!hasExecutableReference || !hasModelReference) {
		std::cerr << "external generation result did not reference the executable and model id\n";
		return 1;
	}

	for (const auto & stem : result.stems) {
		if (stem.path.empty() ||
			!std::filesystem::exists(stem.path) ||
			!fileStartsWith(stem.path, "RIFF")) {
			std::cerr << "external backend did not preserve readable stem artifacts\n";
			return 1;
		}
		removeIfPresent(stem.path);
	}
	removeIfPresent(result.midiPath);
	removeIfPresent(result.chordMidiPath);
	removeIfPresent(result.arrangementMidiPath);
	removeIfPresent(result.manifestPath);
	removeIfPresent(result.historyPath);
	removeIfPresent(result.outputPath);
	return 0;
}
