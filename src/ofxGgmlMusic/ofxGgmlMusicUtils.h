#pragma once

#include "ofxGgmlMusicTypes.h"

#include <string>

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request);
	std::string describe(const ofxGgmlMusicRequest & request);
}