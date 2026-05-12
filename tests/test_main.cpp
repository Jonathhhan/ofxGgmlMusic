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
}

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
	generation.settings.backend = ofxGgmlMusicGenerationBackendFamily::Transformer;
	generation.settings.durationSeconds = 12.0;
	generation.settings.guidance = 4.0f;
	generation.settings.seed = 42;
	generation.settings.loop = true;
	generation.tempo.bpm = 92.0f;
	generation.key.tonic = "D";
	generation.key.mode = "major";
	generation.targetStems = { "piano", "texture" };
	auto presetNames = ofxGgmlMusicUtils::getGenerationPresetNames();
	if (presetNames.size() != 3 ||
		presetNames.front() != "ambient") {
		std::cerr << "generation preset names were unexpected\n";
		return 1;
	}
	ofxGgmlMusicGenerationRequest presetRequest;
	presetRequest.settings.seed = 7;
	if (!ofxGgmlMusicUtils::applyGenerationPreset("lofi", presetRequest) ||
		presetRequest.style != "lofi" ||
		presetRequest.tempo.bpm != 76.0f ||
		presetRequest.key.mode != "minor" ||
		presetRequest.targetStems.size() != 3 ||
		presetRequest.settings.seed != 7 ||
		!presetRequest.settings.loop ||
		ofxGgmlMusicUtils::applyGenerationPreset("unknown", presetRequest)) {
		std::cerr << "generation preset helper failed\n";
		return 1;
	}
	if (!ofxGgmlMusicUtils::hasPrompt(generation) ||
		!ofxGgmlMusicUtils::hasTempo(generation) ||
		!ofxGgmlMusicUtils::hasKey(generation)) {
		std::cerr << "generation request helpers failed\n";
		return 1;
	}
	const auto generationDescription = ofxGgmlMusicUtils::describe(generation);
	if (generationDescription.find("ambient piano") == std::string::npos ||
		generationDescription.find("transformer") == std::string::npos ||
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
	generationResult.manifestPath = ofxGgmlMusicUtils::getGenerationManifestPath(generation.outputPath);
	generationResult.historyPath = ofxGgmlMusicUtils::getGenerationHistoryPath(generation.outputPath);
	generationResult.durationSeconds = generation.settings.durationSeconds;
	generationResult.seed = generation.settings.seed;
	generationResult.sampleRate = 44100;
	generationResult.channels = 1;
	generationResult.peakAbs = 0.8f;
	generationResult.tempo = generation.tempo;
	generationResult.key = generation.key;
	generationResult.beats.push_back({ 0.0, 1.0f, true });
	generationResult.beats.push_back({ 60.0 / generation.tempo.bpm, 0.78f, false });
	generationResult.chords.push_back({ 0.0, "D", 0.84f });
	generationResult.stems.push_back({ "piano", "renders/piano.wav", 1.0f });
	if (ofxGgmlMusicUtils::getGenerationBackendName(generation.settings.backend) != "transformer" ||
		!generationResult ||
		!ofxGgmlMusicUtils::hasOutput(generationResult) ||
		generationResult.manifestPath.find(".wav.json") == std::string::npos ||
		generationResult.historyPath.find("ofxGgmlMusic-history.json") == std::string::npos ||
		!ofxGgmlMusicUtils::hasTempo(generationResult) ||
		!ofxGgmlMusicUtils::hasKey(generationResult) ||
		generationResult.beats.empty() ||
		!generationResult.beats.front().downbeat ||
		generationResult.chords.front().label != "D" ||
		generationResult.stems.front().path != "renders/piano.wav") {
		std::cerr << "generation result helpers failed\n";
		return 1;
	}
	const auto manifestText = ofxGgmlMusicUtils::serializeGenerationManifest(
		generation,
		generationResult,
		"transformer");
	if (manifestText.find("\"prompt\"") == std::string::npos ||
		manifestText.find("ambient piano") == std::string::npos ||
		manifestText.find("\"sampleRate\": 44100") == std::string::npos ||
		manifestText.find("\"historyPath\"") == std::string::npos ||
		manifestText.find("\"beats\"") == std::string::npos ||
		manifestText.find("\"chords\"") == std::string::npos ||
		manifestText.find("\"stems\"") == std::string::npos) {
		std::cerr << "generation manifest serialization failed\n";
		return 1;
	}

	auto backend = ofxGgmlMakeUnavailableMusicGenerationBackend(
		ofxGgmlMusicGenerationBackendFamily::Transformer,
		"transformer");
	if (!backend ||
		backend->getBackendName() != "transformer" ||
		backend->getBackendFamily() != ofxGgmlMusicGenerationBackendFamily::Transformer ||
		backend->isAvailable() ||
		backend->isLoaded()) {
		std::cerr << "unavailable generation backend reported unexpected state\n";
		return 1;
	}
	const auto unavailableSetup = backend->setup(generation);
	if (unavailableSetup ||
		unavailableSetup.error.find("not available") == std::string::npos ||
		unavailableSetup.seed != generation.settings.seed ||
		unavailableSetup.manifestPath.empty()) {
		std::cerr << "unavailable setup result was unexpected\n";
		return 1;
	}
	const auto unavailableGeneration = backend->generate(generation);
	if (unavailableGeneration ||
		unavailableGeneration.error.find("not available") == std::string::npos ||
		!ofxGgmlMusicUtils::hasTempo(unavailableGeneration) ||
		!ofxGgmlMusicUtils::hasKey(unavailableGeneration)) {
		std::cerr << "unavailable generation result was unexpected\n";
		return 1;
	}

	const auto tempOutput = std::filesystem::temp_directory_path() / "ofxGgmlMusic-procedural-test.wav";
	if (std::filesystem::exists(tempOutput)) {
		std::filesystem::remove(tempOutput);
	}
	const auto utilityOutput = std::filesystem::temp_directory_path() / "ofxGgmlMusic-audio-utils-test.wav";
	if (std::filesystem::exists(utilityOutput)) {
		std::filesystem::remove(utilityOutput);
	}
	std::string wavError;
	if (!ofxGgmlMusicAudioUtils::writeMonoWav16(
			utilityOutput.string(),
			{ 0.0f, 0.4f, -0.4f, 0.0f },
			44100,
			wavError)) {
		std::cerr << "audio utility failed to write wav: " << wavError << "\n";
		return 1;
	}
	ofxGgmlMusicAudioBuffer utilityBuffer;
	if (!ofxGgmlMusicAudioUtils::loadWav16(utilityOutput.string(), utilityBuffer, wavError) ||
		!utilityBuffer ||
		utilityBuffer.sampleRate != 44100 ||
		utilityBuffer.channels != 1 ||
		utilityBuffer.samples.size() != 4 ||
		utilityBuffer.getPeakAbs() < 0.39f) {
		std::cerr << "audio utility failed to read wav: " << wavError << "\n";
		return 1;
	}
	std::filesystem::remove(utilityOutput);

	generation.outputPath = tempOutput.string();
	generation.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	generation.targetStems = { "melody", "bass", "pulse" };
	auto procedural = ofxGgmlMakeProceduralMusicGenerationBackend();
	if (!procedural ||
		procedural->getBackendName() != "procedural-sketch" ||
		procedural->getBackendFamily() != ofxGgmlMusicGenerationBackendFamily::External ||
		!procedural->isAvailable() ||
		procedural->isLoaded()) {
		std::cerr << "procedural generation backend reported unexpected state\n";
		return 1;
	}
	const auto setupResult = procedural->setup(generation);
	if (!setupResult || !procedural->isLoaded()) {
		std::cerr << "procedural setup failed\n";
		return 1;
	}
	const auto proceduralResult = procedural->generate(generation);
	ofxGgmlMusicAudioBuffer generatedBuffer;
	if (!proceduralResult ||
		!ofxGgmlMusicUtils::hasOutput(proceduralResult) ||
		proceduralResult.outputPath != generation.outputPath ||
		proceduralResult.manifestPath != ofxGgmlMusicUtils::getGenerationManifestPath(generation.outputPath) ||
		proceduralResult.historyPath != ofxGgmlMusicUtils::getGenerationHistoryPath(generation.outputPath) ||
		proceduralResult.durationSeconds <= 0.0 ||
		proceduralResult.sampleRate != 44100 ||
		proceduralResult.channels != 1 ||
		proceduralResult.peakAbs <= 0.0f ||
		proceduralResult.beats.empty() ||
		!proceduralResult.beats.front().downbeat ||
		proceduralResult.chords.empty() ||
		proceduralResult.stems.size() != 3 ||
		proceduralResult.references.empty() ||
		!std::filesystem::exists(tempOutput) ||
		!std::filesystem::exists(proceduralResult.manifestPath) ||
		!std::filesystem::exists(proceduralResult.historyPath) ||
		std::filesystem::file_size(tempOutput) <= 44 ||
		!fileStartsWith(tempOutput, "RIFF") ||
		!ofxGgmlMusicAudioUtils::loadWav16(tempOutput.string(), generatedBuffer, wavError) ||
		!generatedBuffer ||
		generatedBuffer.getDurationSeconds() <= 0.0 ||
		generatedBuffer.getPeakAbs() <= 0.0f) {
		std::cerr << "procedural generation failed to write a wav file\n";
		return 1;
	}
	for (const auto & stem : proceduralResult.stems) {
		ofxGgmlMusicAudioBuffer stemBuffer;
		if (stem.path.empty() ||
			!std::filesystem::exists(stem.path) ||
			!fileStartsWith(stem.path, "RIFF") ||
			!ofxGgmlMusicAudioUtils::loadWav16(stem.path, stemBuffer, wavError) ||
			!stemBuffer ||
			stemBuffer.getPeakAbs() <= 0.0f) {
			std::cerr << "procedural generation failed to write readable stems\n";
			return 1;
		}
	}
	if (!fileStartsWith(proceduralResult.manifestPath, "{")) {
		std::cerr << "procedural generation failed to write a manifest file\n";
		return 1;
	}
	ofxGgmlMusicGenerationResult loadedManifest;
	std::string manifestError;
	if (!ofxGgmlMusicUtils::loadGenerationManifest(proceduralResult.manifestPath, loadedManifest, manifestError) ||
		!loadedManifest ||
		loadedManifest.outputPath != proceduralResult.outputPath ||
		loadedManifest.historyPath != proceduralResult.historyPath ||
		loadedManifest.sampleRate != proceduralResult.sampleRate ||
		loadedManifest.beats.size() != proceduralResult.beats.size() ||
		loadedManifest.chords.size() != proceduralResult.chords.size() ||
		loadedManifest.stems.size() != proceduralResult.stems.size() ||
		loadedManifest.references.empty()) {
		std::cerr << "generation manifest failed to load: " << manifestError << "\n";
		return 1;
	}
	std::vector<std::string> generationHistory;
	if (!ofxGgmlMusicUtils::loadGenerationHistory(proceduralResult.historyPath, generationHistory, manifestError) ||
		generationHistory.empty() ||
		generationHistory.front() != proceduralResult.manifestPath) {
		std::cerr << "generation history failed to load: " << manifestError << "\n";
		return 1;
	}
	procedural->close();
	if (procedural->isLoaded()) {
		std::cerr << "procedural backend did not unload\n";
		return 1;
	}
	std::filesystem::remove(tempOutput);
	std::filesystem::remove(proceduralResult.manifestPath);
	std::filesystem::remove(proceduralResult.historyPath);
	for (const auto & stem : proceduralResult.stems) {
		std::filesystem::remove(stem.path);
	}

	return 0;
}
