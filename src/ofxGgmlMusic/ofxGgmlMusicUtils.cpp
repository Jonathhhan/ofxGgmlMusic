#include "ofxGgmlMusicUtils.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
	std::string escapeJson(const std::string & text) {
		std::ostringstream escaped;
		for (const auto c : text) {
			switch (c) {
			case '\\':
				escaped << "\\\\";
				break;
			case '"':
				escaped << "\\\"";
				break;
			case '\n':
				escaped << "\\n";
				break;
			case '\r':
				escaped << "\\r";
				break;
			case '\t':
				escaped << "\\t";
				break;
			default:
				escaped << c;
				break;
			}
		}
		return escaped.str();
	}

	std::string quoteJson(const std::string & text) {
		return "\"" + escapeJson(text) + "\"";
	}
}

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request) {
		return !request.audioPath.empty();
	}

	bool hasPrompt(const ofxGgmlMusicGenerationRequest & request) {
		return !request.prompt.empty();
	}

	bool hasOutput(const ofxGgmlMusicGenerationResult & result) {
		return !result.outputPath.empty();
	}

	bool hasTempo(const ofxGgmlMusicResult & result) {
		return result.tempo.bpm > 0.0f;
	}

	bool hasTempo(const ofxGgmlMusicGenerationRequest & request) {
		return request.tempo.bpm > 0.0f;
	}

	bool hasTempo(const ofxGgmlMusicGenerationResult & result) {
		return result.tempo.bpm > 0.0f;
	}

	bool hasKey(const ofxGgmlMusicResult & result) {
		return !result.key.tonic.empty() || !result.key.mode.empty();
	}

	bool hasKey(const ofxGgmlMusicGenerationRequest & request) {
		return !request.key.tonic.empty() || !request.key.mode.empty();
	}

	bool hasKey(const ofxGgmlMusicGenerationResult & result) {
		return !result.key.tonic.empty() || !result.key.mode.empty();
	}

	std::string getTaskName(ofxGgmlMusicTask task) {
		switch (task) {
		case ofxGgmlMusicTask::Analysis:
			return "analysis";
		case ofxGgmlMusicTask::Tempo:
			return "tempo";
		case ofxGgmlMusicTask::BeatTracking:
			return "beat tracking";
		case ofxGgmlMusicTask::KeyDetection:
			return "key detection";
		case ofxGgmlMusicTask::ChordRecognition:
			return "chord recognition";
		case ofxGgmlMusicTask::Embedding:
			return "embedding";
		case ofxGgmlMusicTask::StemSeparation:
			return "stem separation";
		case ofxGgmlMusicTask::Generation:
			return "generation";
		default:
			return "music";
		}
	}

	std::string getGenerationBackendName(ofxGgmlMusicGenerationBackendFamily backend) {
		switch (backend) {
		case ofxGgmlMusicGenerationBackendFamily::Auto:
			return "auto";
		case ofxGgmlMusicGenerationBackendFamily::Diffusion:
			return "diffusion";
		case ofxGgmlMusicGenerationBackendFamily::Transformer:
			return "transformer";
		case ofxGgmlMusicGenerationBackendFamily::SampleRNN:
			return "sample-rnn";
		case ofxGgmlMusicGenerationBackendFamily::External:
			return "external";
		default:
			return "unknown";
		}
	}

	std::string formatKey(const ofxGgmlMusicKey & key) {
		if (key.tonic.empty() && key.mode.empty()) {
			return "";
		}
		if (key.mode.empty()) {
			return key.tonic;
		}
		if (key.tonic.empty()) {
			return key.mode;
		}
		return key.tonic + " " + key.mode;
	}

	std::string describe(const ofxGgmlMusicRequest & request) {
		if (!hasInput(request)) {
			return "music: empty request";
		}
		return getTaskName(request.task) + ": " + request.audioPath;
	}

	std::string describe(const ofxGgmlMusicGenerationRequest & request) {
		if (!hasPrompt(request)) {
			return "music generation: empty prompt";
		}
		auto description = "music generation: " + request.prompt +
			" [" + getGenerationBackendName(request.settings.backend) + "]";
		if (hasTempo(request)) {
			description += " @ " + std::to_string(static_cast<int>(request.tempo.bpm)) + " bpm";
		}
		const auto key = formatKey(request.key);
		if (!key.empty()) {
			description += " in " + key;
		}
		return description;
	}

	std::string getGenerationManifestPath(const std::string & outputPath) {
		if (outputPath.empty()) {
			return "";
		}
		return outputPath + ".json";
	}

	std::string serializeGenerationManifest(
		const ofxGgmlMusicGenerationRequest & request,
		const ofxGgmlMusicGenerationResult & result,
		const std::string & backendName) {
		std::ostringstream json;
		json << "{\n";
		json << "  \"backend\": " << quoteJson(backendName) << ",\n";
		json << "  \"backendFamily\": " << quoteJson(getGenerationBackendName(request.settings.backend)) << ",\n";
		json << "  \"prompt\": " << quoteJson(request.prompt) << ",\n";
		json << "  \"negativePrompt\": " << quoteJson(request.negativePrompt) << ",\n";
		json << "  \"style\": " << quoteJson(request.style) << ",\n";
		json << "  \"referenceAudioPath\": " << quoteJson(request.referenceAudioPath) << ",\n";
		json << "  \"outputPath\": " << quoteJson(result.outputPath) << ",\n";
		json << "  \"manifestPath\": " << quoteJson(result.manifestPath) << ",\n";
		json << "  \"seed\": " << result.seed << ",\n";
		json << "  \"durationSeconds\": " << result.durationSeconds << ",\n";
		json << "  \"sampleRate\": " << result.sampleRate << ",\n";
		json << "  \"channels\": " << result.channels << ",\n";
		json << "  \"peakAbs\": " << result.peakAbs << ",\n";
		json << "  \"tempo\": {\n";
		json << "    \"bpm\": " << result.tempo.bpm << ",\n";
		json << "    \"confidence\": " << result.tempo.confidence << "\n";
		json << "  },\n";
		json << "  \"key\": {\n";
		json << "    \"tonic\": " << quoteJson(result.key.tonic) << ",\n";
		json << "    \"mode\": " << quoteJson(result.key.mode) << ",\n";
		json << "    \"confidence\": " << result.key.confidence << "\n";
		json << "  },\n";
		json << "  \"beats\": [\n";
		for (std::size_t i = 0; i < result.beats.size(); ++i) {
			const auto & beat = result.beats[i];
			json << "    { \"timeSeconds\": " << beat.timeSeconds <<
				", \"confidence\": " << beat.confidence <<
				", \"downbeat\": " << (beat.downbeat ? "true" : "false") << " }";
			json << (i + 1 < result.beats.size() ? "," : "") << "\n";
		}
		json << "  ],\n";
		json << "  \"chords\": [\n";
		for (std::size_t i = 0; i < result.chords.size(); ++i) {
			const auto & chord = result.chords[i];
			json << "    { \"timeSeconds\": " << chord.timeSeconds <<
				", \"label\": " << quoteJson(chord.label) <<
				", \"confidence\": " << chord.confidence << " }";
			json << (i + 1 < result.chords.size() ? "," : "") << "\n";
		}
		json << "  ],\n";
		json << "  \"loop\": " << (request.settings.loop ? "true" : "false") << ",\n";
		json << "  \"targetStems\": [";
		for (std::size_t i = 0; i < request.targetStems.size(); ++i) {
			if (i > 0) {
				json << ", ";
			}
			json << quoteJson(request.targetStems[i]);
		}
		json << "],\n";
		json << "  \"stems\": [\n";
		for (std::size_t i = 0; i < result.stems.size(); ++i) {
			const auto & stem = result.stems[i];
			json << "    { \"name\": " << quoteJson(stem.name) <<
				", \"path\": " << quoteJson(stem.path) <<
				", \"gain\": " << stem.gain << " }";
			json << (i + 1 < result.stems.size() ? "," : "") << "\n";
		}
		json << "  ],\n";
		json << "  \"references\": [";
		for (std::size_t i = 0; i < result.references.size(); ++i) {
			if (i > 0) {
				json << ", ";
			}
			json << quoteJson(result.references[i]);
		}
		json << "]\n";
		json << "}\n";
		return json.str();
	}

	bool writeGenerationManifest(
		const ofxGgmlMusicGenerationRequest & request,
		const ofxGgmlMusicGenerationResult & result,
		const std::string & backendName,
		std::string & error) {
		error.clear();
		if (result.manifestPath.empty()) {
			error = "generation manifest path is empty";
			return false;
		}

		std::filesystem::path manifestPath(result.manifestPath);
		if (manifestPath.has_parent_path()) {
			std::error_code code;
			std::filesystem::create_directories(manifestPath.parent_path(), code);
			if (code) {
				error = "could not create generation manifest directory";
				return false;
			}
		}

		std::ofstream output(result.manifestPath, std::ios::out | std::ios::trunc);
		if (!output) {
			error = "could not open generation manifest path";
			return false;
		}
		output << serializeGenerationManifest(request, result, backendName);
		return true;
	}
}
