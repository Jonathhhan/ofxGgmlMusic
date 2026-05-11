#include "ofxGgmlMusic.h"

#include <iostream>

int main() {
	ofxGgmlMusicRequest request;
	if (ofxGgmlMusicUtils::hasInput(request)) {
		std::cerr << "empty request reported as configured\n";
		return 1;
	}

	request.audioPath = "audio/example.wav";
	if (!ofxGgmlMusicUtils::hasInput(request)) {
		std::cerr << "configured request reported as empty\n";
		return 1;
	}

	const auto description = ofxGgmlMusicUtils::describe(request);
	if (description.find(request.audioPath) == std::string::npos) {
		std::cerr << "description did not include request input\n";
		return 1;
	}

	return 0;
}