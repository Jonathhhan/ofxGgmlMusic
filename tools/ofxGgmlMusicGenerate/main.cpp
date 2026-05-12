#include "ofxGgmlMusic.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	void printUsage() {
		std::cout
			<< "ofxGgmlMusicGenerate --prompt TEXT --output PATH [options]\n"
			<< "\n"
			<< "Options:\n"
			<< "  --preset NAME      Preset: ambient, lofi, pulse\n"
			<< "  --style TEXT       Style tag, for example ambient\n"
			<< "  --tempo BPM        Tempo in beats per minute\n"
			<< "  --duration SEC     Duration in seconds\n"
			<< "  --seed N           Deterministic seed\n"
			<< "  --key TONIC        Key tonic, for example C, D, Bb\n"
			<< "  --mode MODE        major or minor\n"
			<< "  --loop             Render loop-friendly audio\n"
			<< "  --stem NAME        Export a stem: melody, bass, pulse, mix\n";
	}

	bool readValue(int & index, int argc, char ** argv, std::string & value) {
		if (index + 1 >= argc) {
			return false;
		}
		value = argv[++index];
		return true;
	}
}

int main(int argc, char ** argv) {
	ofxGgmlMusicGenerationRequest request;
	request.outputPath = "ofxGgmlMusicGenerate.wav";
	request.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	request.settings.seed = 42;

	std::string presetName = "ambient";
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		std::string value;
		if (arg == "--preset" && readValue(i, argc, argv, value)) {
			presetName = value;
		}
	}
	if (!ofxGgmlMusicUtils::applyGenerationPreset(presetName, request)) {
		std::cerr << "Unknown preset: " << presetName << "\n";
		printUsage();
		return 2;
	}

	std::vector<std::string> explicitStems;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		std::string value;
		if (arg == "--help" || arg == "-h") {
			printUsage();
			return 0;
		} else if (arg == "--preset" && readValue(i, argc, argv, value)) {
			continue;
		} else if (arg == "--prompt" && readValue(i, argc, argv, value)) {
			request.prompt = value;
		} else if (arg == "--output" && readValue(i, argc, argv, value)) {
			request.outputPath = value;
		} else if (arg == "--style" && readValue(i, argc, argv, value)) {
			request.style = value;
		} else if (arg == "--tempo" && readValue(i, argc, argv, value)) {
			request.tempo.bpm = std::strtof(value.c_str(), nullptr);
		} else if (arg == "--duration" && readValue(i, argc, argv, value)) {
			request.settings.durationSeconds = std::strtod(value.c_str(), nullptr);
		} else if (arg == "--seed" && readValue(i, argc, argv, value)) {
			request.settings.seed = std::atoi(value.c_str());
		} else if (arg == "--key" && readValue(i, argc, argv, value)) {
			request.key.tonic = value;
		} else if (arg == "--mode" && readValue(i, argc, argv, value)) {
			request.key.mode = value;
		} else if (arg == "--stem" && readValue(i, argc, argv, value)) {
			explicitStems.push_back(value);
		} else if (arg == "--loop") {
			request.settings.loop = true;
		} else {
			std::cerr << "Unknown or incomplete argument: " << arg << "\n";
			printUsage();
			return 2;
		}
	}
	if (!explicitStems.empty()) {
		request.targetStems = explicitStems;
	}

	if (!ofxGgmlMusicUtils::hasPrompt(request)) {
		std::cerr << "Prompt is required.\n";
		return 2;
	}
	if (request.outputPath.empty()) {
		std::cerr << "Output path is required.\n";
		return 2;
	}

	auto backend = ofxGgmlMakeProceduralMusicGenerationBackend();
	const auto setup = backend->setup(request);
	if (!setup) {
		std::cerr << setup.error << "\n";
		return 1;
	}
	const auto result = backend->generate(request);
	if (!result) {
		std::cerr << result.error << "\n";
		return 1;
	}

	std::cout << "output: " << result.outputPath << "\n";
	std::cout << "preset: " << presetName << "\n";
	std::cout << "manifest: " << result.manifestPath << "\n";
	std::cout << "duration: " << result.durationSeconds << "\n";
	std::cout << "sample rate: " << result.sampleRate << "\n";
	std::cout << "peak: " << result.peakAbs << "\n";
	std::cout << "beats: " << result.beats.size() << "\n";
	std::cout << "chords: " << result.chords.size() << "\n";
	std::cout << "stems: " << result.stems.size() << "\n";
	for (const auto & stem : result.stems) {
		std::cout << "stem: " << stem.name << " " << stem.path << "\n";
	}
	return 0;
}
