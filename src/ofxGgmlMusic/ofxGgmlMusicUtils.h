#pragma once

#include "ofxGgmlMusicTypes.h"

#include <string>

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request);
	bool hasTempo(const ofxGgmlMusicResult & result);
	bool hasKey(const ofxGgmlMusicResult & result);
	std::string getTaskName(ofxGgmlMusicTask task);
	std::string formatKey(const ofxGgmlMusicKey & key);
	std::string describe(const ofxGgmlMusicRequest & request);
}
