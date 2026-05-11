#pragma once

#include "ofxGgmlMusicTypes.h"

#include <string>

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request);
	bool hasPrompt(const ofxGgmlMusicGenerationRequest & request);
	bool hasOutput(const ofxGgmlMusicGenerationResult & result);
	bool hasTempo(const ofxGgmlMusicResult & result);
	bool hasTempo(const ofxGgmlMusicGenerationRequest & request);
	bool hasTempo(const ofxGgmlMusicGenerationResult & result);
	bool hasKey(const ofxGgmlMusicResult & result);
	bool hasKey(const ofxGgmlMusicGenerationRequest & request);
	bool hasKey(const ofxGgmlMusicGenerationResult & result);
	std::string getTaskName(ofxGgmlMusicTask task);
	std::string getGenerationBackendName(ofxGgmlMusicGenerationBackend backend);
	std::string formatKey(const ofxGgmlMusicKey & key);
	std::string describe(const ofxGgmlMusicRequest & request);
	std::string describe(const ofxGgmlMusicGenerationRequest & request);
}
