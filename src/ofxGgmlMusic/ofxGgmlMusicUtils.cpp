#include "ofxGgmlMusicUtils.h"

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request) {
		return !request.audioPath.empty();
	}

	bool hasTempo(const ofxGgmlMusicResult & result) {
		return result.tempo.bpm > 0.0f;
	}

	bool hasKey(const ofxGgmlMusicResult & result) {
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
}
