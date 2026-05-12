#include "ofxGgmlMusic.h"

#include <cerrno>
#include <climits>
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
			<< "  --list-presets     List built-in generation presets\n"
			<< "  --describe-preset NAME\n"
			<< "                     Print preset defaults without rendering audio\n"
			<< "  --list-stems       List canonical stem export names\n"
			<< "  --preset NAME      Preset: ambient, lofi, pulse\n"
			<< "  --style TEXT       Style tag, for example ambient\n"
			<< "  --tempo BPM        Tempo in beats per minute\n"
			<< "  --duration SEC     Duration in seconds\n"
			<< "  --seed N           Deterministic seed\n"
			<< "  --key TONIC        Key tonic, for example C, D, Bb\n"
			<< "  --mode MODE        major or minor\n"
			<< "  --loop             Render loop-friendly audio\n"
			<< "  --stem NAME        Export a stem: melody, bass, pulse, mix\n"
			<< "  --json             Print machine-readable JSON output\n";
	}

	bool readValue(int & index, int argc, char ** argv, std::string & value) {
		if (index + 1 >= argc) {
			return false;
		}
		value = argv[++index];
		return true;
	}

	bool hasFlag(int argc, char ** argv, const std::string & flag) {
		for (int i = 1; i < argc; ++i) {
			if (argv[i] == flag) {
				return true;
			}
		}
		return false;
	}

	bool parseDouble(const std::string & text, double & value) {
		if (text.empty()) {
			return false;
		}
		errno = 0;
		char * end = nullptr;
		const auto parsed = std::strtod(text.c_str(), &end);
		if (end != text.c_str() + text.size() || errno == ERANGE) {
			return false;
		}
		value = parsed;
		return true;
	}

	bool parseFloat(const std::string & text, float & value) {
		double parsed = 0.0;
		if (!parseDouble(text, parsed)) {
			return false;
		}
		value = static_cast<float>(parsed);
		return true;
	}

	bool parseInt(const std::string & text, int & value) {
		if (text.empty()) {
			return false;
		}
		errno = 0;
		char * end = nullptr;
		const auto parsed = std::strtol(text.c_str(), &end, 10);
		if (end != text.c_str() + text.size() ||
			errno == ERANGE ||
			parsed < INT_MIN ||
			parsed > INT_MAX) {
			return false;
		}
		value = static_cast<int>(parsed);
		return true;
	}

	std::string escapeJson(const std::string & text) {
		std::string escaped;
		for (const auto c : text) {
			switch (c) {
			case '\\':
				escaped += "\\\\";
				break;
			case '"':
				escaped += "\\\"";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\r':
				escaped += "\\r";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				escaped.push_back(c);
				break;
			}
		}
		return escaped;
	}

	std::string quoteJson(const std::string & text) {
		return "\"" + escapeJson(text) + "\"";
	}

	void printResultJson(
		const ofxGgmlMusicGenerationResult & result,
		const std::string & presetName = "") {
		std::cout << "{\n";
		std::cout << "  \"outputPath\": " << quoteJson(result.outputPath) << ",\n";
		if (!presetName.empty()) {
			std::cout << "  \"preset\": " << quoteJson(presetName) << ",\n";
		}
		std::cout << "  \"manifestPath\": " << quoteJson(result.manifestPath) << ",\n";
		std::cout << "  \"historyPath\": " << quoteJson(result.historyPath) << ",\n";
		std::cout << "  \"midiPath\": " << quoteJson(result.midiPath) << ",\n";
		std::cout << "  \"chordMidiPath\": " << quoteJson(result.chordMidiPath) << ",\n";
		std::cout << "  \"arrangementMidiPath\": " << quoteJson(result.arrangementMidiPath) << ",\n";
		std::cout << "  \"durationSeconds\": " << result.durationSeconds << ",\n";
		std::cout << "  \"sampleRate\": " << result.sampleRate << ",\n";
		std::cout << "  \"peakAbs\": " << result.peakAbs << ",\n";
		std::cout << "  \"beats\": " << result.beats.size() << ",\n";
		std::cout << "  \"chords\": " << result.chords.size() << ",\n";
		std::cout << "  \"sections\": " << result.sections.size() << ",\n";
		std::cout << "  \"stems\": " << result.stems.size() << "\n";
		std::cout << "}\n";
	}

	void printResultText(
		const ofxGgmlMusicGenerationResult & result,
		const std::string & presetName = "") {
		std::cout << "output: " << result.outputPath << "\n";
		if (!presetName.empty()) {
			std::cout << "preset: " << presetName << "\n";
		}
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
	}

	void printHistory(
		const std::string & historyPath,
		const std::vector<std::string> & manifests,
		bool json) {
		if (json) {
			std::cout << "{\n";
			std::cout << "  \"historyPath\": " << quoteJson(historyPath) << ",\n";
			std::cout << "  \"manifests\": [\n";
			for (std::size_t i = 0; i < manifests.size(); ++i) {
				std::cout << "    " << quoteJson(manifests[i]);
				std::cout << (i + 1 < manifests.size() ? "," : "") << "\n";
			}
			std::cout << "  ]\n";
			std::cout << "}\n";
			return;
		}
		std::cout << "history: " << historyPath << "\n";
		std::cout << "manifests: " << manifests.size() << "\n";
		for (const auto & manifest : manifests) {
			std::cout << "manifest: " << manifest << "\n";
		}
	}

	void printPresets(bool json) {
		const auto presets = ofxGgmlMusicUtils::getGenerationPresetNames();
		if (json) {
			std::cout << "{\n";
			std::cout << "  \"presets\": [\n";
			for (std::size_t i = 0; i < presets.size(); ++i) {
				std::cout << "    " << quoteJson(presets[i]);
				std::cout << (i + 1 < presets.size() ? "," : "") << "\n";
			}
			std::cout << "  ]\n";
			std::cout << "}\n";
			return;
		}

		std::cout << "presets: " << presets.size() << "\n";
		for (const auto & preset : presets) {
			std::cout << "preset: " << preset << "\n";
		}
	}

	void printStems(bool json) {
		const auto stems = ofxGgmlMusicUtils::getGenerationStemNames();
		if (json) {
			std::cout << "{\n";
			std::cout << "  \"stems\": [\n";
			for (std::size_t i = 0; i < stems.size(); ++i) {
				std::cout << "    " << quoteJson(stems[i]);
				std::cout << (i + 1 < stems.size() ? "," : "") << "\n";
			}
			std::cout << "  ]\n";
			std::cout << "}\n";
			return;
		}

		std::cout << "stems: " << stems.size() << "\n";
		for (const auto & stem : stems) {
			std::cout << "stem: " << stem << "\n";
		}
	}

	void printPresetDescription(
		const std::string & presetName,
		const ofxGgmlMusicGenerationRequest & request,
		bool json) {
		if (json) {
			std::cout << "{\n";
			std::cout << "  \"preset\": " << quoteJson(presetName) << ",\n";
			std::cout << "  \"prompt\": " << quoteJson(request.prompt) << ",\n";
			std::cout << "  \"style\": " << quoteJson(request.style) << ",\n";
			std::cout << "  \"durationSeconds\": " << request.settings.durationSeconds << ",\n";
			std::cout << "  \"tempoBpm\": " << request.tempo.bpm << ",\n";
			std::cout << "  \"key\": " << quoteJson(request.key.tonic) << ",\n";
			std::cout << "  \"mode\": " << quoteJson(request.key.mode) << ",\n";
			std::cout << "  \"loop\": " << (request.settings.loop ? "true" : "false") << ",\n";
			std::cout << "  \"stems\": [\n";
			for (std::size_t i = 0; i < request.targetStems.size(); ++i) {
				std::cout << "    " << quoteJson(request.targetStems[i]);
				std::cout << (i + 1 < request.targetStems.size() ? "," : "") << "\n";
			}
			std::cout << "  ]\n";
			std::cout << "}\n";
			return;
		}

		std::cout << "preset: " << presetName << "\n";
		std::cout << "prompt: " << request.prompt << "\n";
		std::cout << "style: " << request.style << "\n";
		std::cout << "duration: " << request.settings.durationSeconds << "\n";
		std::cout << "tempo: " << request.tempo.bpm << "\n";
		std::cout << "key: " << request.key.tonic << "\n";
		std::cout << "mode: " << request.key.mode << "\n";
		std::cout << "loop: " << (request.settings.loop ? "true" : "false") << "\n";
		for (const auto & stem : request.targetStems) {
			std::cout << "stem: " << stem << "\n";
		}
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

	int pruneHistory(const std::string & historyPath, int keepCount, bool json) {
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
			if (json) {
				std::cout << "{\n";
				std::cout << "  \"historyPath\": " << quoteJson(historyPath) << ",\n";
				std::cout << "  \"kept\": " << manifests.size() << ",\n";
				std::cout << "  \"pruned\": 0\n";
				std::cout << "}\n";
			} else {
				std::cout << "history: " << historyPath << "\n";
				std::cout << "kept: " << manifests.size() << "\n";
				std::cout << "pruned: 0\n";
			}
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
		if (json) {
			std::cout << "{\n";
			std::cout << "  \"historyPath\": " << quoteJson(historyPath) << ",\n";
			std::cout << "  \"kept\": " << manifests.size() << ",\n";
			std::cout << "  \"pruned\": " << removedFiles << "\n";
			std::cout << "}\n";
		} else {
			std::cout << "history: " << historyPath << "\n";
			std::cout << "kept: " << manifests.size() << "\n";
			std::cout << "pruned: " << removedFiles << "\n";
		}
		return 0;
	}
}

int main(int argc, char ** argv) {
	const bool jsonOutput = hasFlag(argc, argv, "--json");
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
			if (jsonOutput) {
				printResultJson(manifest);
			} else {
				printResultText(manifest);
			}
			return 0;
		} else if (arg == "--history" && readValue(i, argc, argv, value)) {
			std::vector<std::string> manifests;
			std::string error;
			if (!ofxGgmlMusicUtils::loadGenerationHistory(value, manifests, error)) {
				std::cerr << error << "\n";
				return 1;
			}
			printHistory(value, manifests, jsonOutput);
			return 0;
		} else if (arg == "--list-presets") {
			printPresets(jsonOutput);
			return 0;
		} else if (arg == "--list-stems") {
			printStems(jsonOutput);
			return 0;
		} else if (arg == "--describe-preset" && readValue(i, argc, argv, value)) {
			ofxGgmlMusicGenerationRequest request;
			if (!ofxGgmlMusicUtils::applyGenerationPreset(value, request)) {
				std::cerr << "Unknown preset: " << value << "\n";
				return 2;
			}
			printPresetDescription(value, request, jsonOutput);
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
			int keepCount = 0;
			if (!parseInt(keepValue, keepCount)) {
				std::cerr << "--keep requires an integer value.\n";
				return 2;
			}
			return pruneHistory(value, keepCount, jsonOutput);
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
		} else if (arg == "--json") {
			continue;
		} else if (arg == "--list-presets") {
			continue;
		} else if (arg == "--list-stems") {
			continue;
		} else if (arg == "--describe-preset" && readValue(i, argc, argv, value)) {
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
			if (!parseFloat(value, request.tempo.bpm)) {
				std::cerr << "--tempo requires a numeric BPM value.\n";
				return 2;
			}
			if (request.tempo.bpm <= 0.0f) {
				std::cerr << "--tempo must be greater than zero.\n";
				return 2;
			}
		} else if (arg == "--duration" && readValue(i, argc, argv, value)) {
			if (!parseDouble(value, request.settings.durationSeconds)) {
				std::cerr << "--duration requires a numeric seconds value.\n";
				return 2;
			}
			if (request.settings.durationSeconds <= 0.0) {
				std::cerr << "--duration must be greater than zero.\n";
				return 2;
			}
		} else if (arg == "--seed" && readValue(i, argc, argv, value)) {
			if (!parseInt(value, request.settings.seed)) {
				std::cerr << "--seed requires an integer value.\n";
				return 2;
			}
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

	if (jsonOutput) {
		printResultJson(result, presetName);
	} else {
		printResultText(result, presetName);
		for (const auto & stem : result.stems) {
			std::cout << "stem: " << stem.name << " " << stem.path << "\n";
		}
	}
	return 0;
}
