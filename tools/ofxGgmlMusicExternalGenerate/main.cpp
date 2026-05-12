#include "ofxGgmlMusic.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	void printUsage() {
		std::cout
			<< "ofxGgmlMusicExternalGenerate --executable PATH --prompt TEXT --output PATH [options]\n"
			<< "\n"
			<< "Options:\n"
			<< "  --model TEXT              Local model path or model id\n"
			<< "  --allow-model-id          Do not require --model to be an existing file\n"
			<< "  --working-directory PATH  Run the generator from this directory\n"
			<< "  --duration SEC            Generation duration in seconds\n"
			<< "  --seed N                  Generation seed\n"
			<< "  --style TEXT              Style tag recorded in the manifest\n"
			<< "  --tempo BPM               Tempo recorded in the manifest\n"
			<< "  --key TONIC               Key tonic recorded in the manifest\n"
			<< "  --mode MODE               Key mode recorded in the manifest\n"
			<< "  --prompt-flag FLAG        Default: --prompt\n"
			<< "  --output-flag FLAG        Default: --output\n"
			<< "  --model-flag FLAG         Default: --model\n"
			<< "  --duration-flag FLAG      Default: --duration\n"
			<< "  --seed-flag FLAG          Default: --seed\n"
			<< "  --extra ARG               Append one generator-specific argument\n"
			<< "  --json                    Print machine-readable output\n";
	}

	bool readValue(int & index, int argc, char ** argv, std::string & value) {
		if (index + 1 >= argc) {
			return false;
		}
		value = argv[++index];
		return true;
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

	void printResult(const ofxGgmlMusicGenerationResult & result, bool json) {
		if (json) {
			std::cout << "{\n";
			std::cout << "  \"outputPath\": " << quoteJson(result.outputPath) << ",\n";
			std::cout << "  \"manifestPath\": " << quoteJson(result.manifestPath) << ",\n";
			std::cout << "  \"historyPath\": " << quoteJson(result.historyPath) << ",\n";
			std::cout << "  \"durationSeconds\": " << result.durationSeconds << ",\n";
			std::cout << "  \"sampleRate\": " << result.sampleRate << ",\n";
			std::cout << "  \"peakAbs\": " << result.peakAbs << ",\n";
			std::cout << "  \"references\": [";
			for (std::size_t i = 0; i < result.references.size(); ++i) {
				if (i > 0) {
					std::cout << ", ";
				}
				std::cout << quoteJson(result.references[i]);
			}
			std::cout << "]\n";
			std::cout << "}\n";
			return;
		}

		std::cout << "output: " << result.outputPath << "\n";
		std::cout << "manifest: " << result.manifestPath << "\n";
		std::cout << "history: " << result.historyPath << "\n";
		std::cout << "duration: " << result.durationSeconds << "\n";
		std::cout << "sample rate: " << result.sampleRate << "\n";
		std::cout << "peak: " << result.peakAbs << "\n";
		for (const auto & reference : result.references) {
			std::cout << "reference: " << reference << "\n";
		}
	}
}

int main(int argc, char ** argv) {
	ofxGgmlMusicGenerationRequest request;
	request.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	request.settings.durationSeconds = 8.0;
	request.settings.seed = 42;

	bool json = false;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		std::string value;
		if (arg == "--help" || arg == "-h") {
			printUsage();
			return 0;
		} else if (arg == "--json") {
			json = true;
		} else if (arg == "--allow-model-id") {
			request.external.requireModelPathExists = false;
		} else if (arg == "--executable" && readValue(i, argc, argv, value)) {
			request.external.executablePath = value;
		} else if (arg == "--prompt" && readValue(i, argc, argv, value)) {
			request.prompt = value;
		} else if (arg == "--output" && readValue(i, argc, argv, value)) {
			request.outputPath = value;
		} else if (arg == "--model" && readValue(i, argc, argv, value)) {
			request.external.modelPath = value;
		} else if (arg == "--working-directory" && readValue(i, argc, argv, value)) {
			request.external.workingDirectory = value;
		} else if (arg == "--style" && readValue(i, argc, argv, value)) {
			request.style = value;
		} else if (arg == "--key" && readValue(i, argc, argv, value)) {
			request.key.tonic = value;
			request.key.confidence = 1.0f;
		} else if (arg == "--mode" && readValue(i, argc, argv, value)) {
			request.key.mode = value;
			request.key.confidence = 1.0f;
		} else if (arg == "--prompt-flag" && readValue(i, argc, argv, value)) {
			request.external.promptFlag = value;
		} else if (arg == "--output-flag" && readValue(i, argc, argv, value)) {
			request.external.outputFlag = value;
		} else if (arg == "--model-flag" && readValue(i, argc, argv, value)) {
			request.external.modelFlag = value;
		} else if (arg == "--duration-flag" && readValue(i, argc, argv, value)) {
			request.external.durationFlag = value;
		} else if (arg == "--seed-flag" && readValue(i, argc, argv, value)) {
			request.external.seedFlag = value;
		} else if (arg == "--extra" && readValue(i, argc, argv, value)) {
			request.external.extraArguments.push_back(value);
		} else if (arg == "--duration" && readValue(i, argc, argv, value)) {
			if (!parseDouble(value, request.settings.durationSeconds) ||
				request.settings.durationSeconds <= 0.0) {
				std::cerr << "invalid duration: " << value << "\n";
				return 2;
			}
		} else if (arg == "--tempo" && readValue(i, argc, argv, value)) {
			double tempo = 0.0;
			if (!parseDouble(value, tempo) || tempo <= 0.0) {
				std::cerr << "invalid tempo: " << value << "\n";
				return 2;
			}
			request.tempo.bpm = static_cast<float>(tempo);
			request.tempo.confidence = 1.0f;
		} else if (arg == "--seed" && readValue(i, argc, argv, value)) {
			if (!parseInt(value, request.settings.seed)) {
				std::cerr << "invalid seed: " << value << "\n";
				return 2;
			}
		} else {
			std::cerr << "unknown or incomplete argument: " << arg << "\n";
			printUsage();
			return 2;
		}
	}

	auto backend = ofxGgmlMakeExternalMusicGenerationBackend();
	const auto result = backend->generate(request);
	if (!result) {
		std::cerr << "external generation failed: " << result.error << "\n";
		return 1;
	}
	printResult(result, json);
	return 0;
}
