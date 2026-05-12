#include "ofxGgmlMusic.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
	void printUsage() {
		std::cout
			<< "ofxGgmlMusicGenerate --prompt TEXT --output PATH [options]\n"
			<< "\n"
			<< "Options:\n"
			<< "  --inspect PATH     Load and summarize a generated manifest\n"
			<< "  --history PATH     Load and list a generation history index\n"
			<< "  --prune-history PATH --keep N\n"
			<< "                     Keep the newest N history entries and delete older artifacts\n"
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

	bool removeFileIfPresent(const std::string & path) {
		if (path.empty()) {
			return false;
		}
		std::error_code code;
		if (!std::filesystem::exists(path, code) || code) {
			return false;
		}
		if (!std::filesystem::is_regular_file(path, code) || code) {
			return false;
		}
		return std::filesystem::remove(path, code) && !code;
	}

	int pruneHistory(const std::string & historyPath, int keepCount) {
		if (keepCount < 0) {
			std::cerr << "Keep count must be zero or greater.\n";
			return 2;
		}

		std::vector<std::string> manifests;
		std::string error;
		if (!ofxGgmlMusicUtils::loadGenerationHistory(historyPath, manifests, error)) {
			std::cerr << error << "\n";
			return 1;
		}

		const auto keep = static_cast<std::size_t>(keepCount);
		if (manifests.size() <= keep) {
			std::cout << "history: " << historyPath << "\n";
			std::cout << "kept: " << manifests.size() << "\n";
			std::cout << "pruned: 0\n";
			return 0;
		}

		std::size_t removedFiles = 0;
		for (std::size_t i = keep; i < manifests.size(); ++i) {
			ofxGgmlMusicGenerationResult manifest;
			std::string manifestError;
			if (ofxGgmlMusicUtils::loadGenerationManifest(manifests[i], manifest, manifestError)) {
				removedFiles += removeFileIfPresent(manifest.outputPath) ? 1 : 0;
				removedFiles += removeFileIfPresent(manifest.midiPath) ? 1 : 0;
				removedFiles += removeFileIfPresent(manifest.chordMidiPath) ? 1 : 0;
				removedFiles += removeFileIfPresent(manifest.arrangementMidiPath) ? 1 : 0;
				for (const auto & stem : manifest.stems) {
					removedFiles += removeFileIfPresent(stem.path) ? 1 : 0;
				}
			}
			removedFiles += removeFileIfPresent(manifests[i]) ? 1 : 0;
		}

		manifests.resize(keep);
		if (!ofxGgmlMusicUtils::writeGenerationHistory(historyPath, manifests, error)) {
			std::cerr << error << "\n";
			return 1;
		}
		std::cout << "history: " << historyPath << "\n";
		std::cout << "kept: " << manifests.size() << "\n";
		std::cout << "pruned: " << removedFiles << "\n";
		return 0;
	}
}

int main(int argc, char ** argv) {
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		std::string value;
		if (arg == "--inspect" && readValue(i, argc, argv, value)) {
			ofxGgmlMusicGenerationResult manifest;
			std::string error;
			if (!ofxGgmlMusicUtils::loadGenerationManifest(value, manifest, error)) {
				std::cerr << error << "\n";
				return 1;
			}
			std::cout << "output: " << manifest.outputPath << "\n";
			std::cout << "manifest: " << manifest.manifestPath << "\n";
			std::cout << "history: " << manifest.historyPath << "\n";
			std::cout << "midi: " << manifest.midiPath << "\n";
			std::cout << "chord midi: " << manifest.chordMidiPath << "\n";
			std::cout << "arrangement midi: " << manifest.arrangementMidiPath << "\n";
			std::cout << "duration: " << manifest.durationSeconds << "\n";
			std::cout << "sample rate: " << manifest.sampleRate << "\n";
			std::cout << "peak: " << manifest.peakAbs << "\n";
			std::cout << "beats: " << manifest.beats.size() << "\n";
			std::cout << "chords: " << manifest.chords.size() << "\n";
			std::cout << "sections: " << manifest.sections.size() << "\n";
			std::cout << "stems: " << manifest.stems.size() << "\n";
			return 0;
		} else if (arg == "--history" && readValue(i, argc, argv, value)) {
			std::vector<std::string> manifests;
			std::string error;
			if (!ofxGgmlMusicUtils::loadGenerationHistory(value, manifests, error)) {
				std::cerr << error << "\n";
				return 1;
			}
			std::cout << "history: " << value << "\n";
			std::cout << "manifests: " << manifests.size() << "\n";
			for (const auto & manifest : manifests) {
				std::cout << "manifest: " << manifest << "\n";
			}
			return 0;
		} else if (arg == "--prune-history" && readValue(i, argc, argv, value)) {
			std::string keepValue;
			bool foundKeep = false;
			for (int j = 1; j < argc; ++j) {
				const std::string pruneArg = argv[j];
				if (pruneArg == "--keep" && readValue(j, argc, argv, keepValue)) {
					foundKeep = true;
					break;
				}
			}
			if (!foundKeep) {
				std::cerr << "--prune-history requires --keep N.\n";
				return 2;
			}
			return pruneHistory(value, std::atoi(keepValue.c_str()));
		}
	}

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
		} else if (arg == "--inspect" && readValue(i, argc, argv, value)) {
			continue;
		} else if (arg == "--history" && readValue(i, argc, argv, value)) {
			continue;
		} else if (arg == "--prune-history" && readValue(i, argc, argv, value)) {
			continue;
		} else if (arg == "--keep" && readValue(i, argc, argv, value)) {
			continue;
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
	std::cout << "history: " << result.historyPath << "\n";
	std::cout << "midi: " << result.midiPath << "\n";
	std::cout << "chord midi: " << result.chordMidiPath << "\n";
	std::cout << "arrangement midi: " << result.arrangementMidiPath << "\n";
	std::cout << "duration: " << result.durationSeconds << "\n";
	std::cout << "sample rate: " << result.sampleRate << "\n";
	std::cout << "peak: " << result.peakAbs << "\n";
	std::cout << "beats: " << result.beats.size() << "\n";
	std::cout << "chords: " << result.chords.size() << "\n";
	std::cout << "sections: " << result.sections.size() << "\n";
	std::cout << "stems: " << result.stems.size() << "\n";
	for (const auto & stem : result.stems) {
		std::cout << "stem: " << stem.name << " " << stem.path << "\n";
	}
	return 0;
}
