#include "ofxGgmlMusicUtils.h"

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
		case ofxGgmlMusicGenerationBackendFamily::GAN:
			return "gan";
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
}
