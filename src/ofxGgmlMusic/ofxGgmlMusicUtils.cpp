#include "ofxGgmlMusicUtils.h"

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request) {
		return !request.audioPath.empty();
	}

	std::string describe(const ofxGgmlMusicRequest & request) {
		if (!hasInput(request)) {
			return "music: empty request";
		}
		return "music: " + request.audioPath;
	}
}