#pragma once

#include <string>
#include <vector>

struct ofxGgmlMusicRequest {
	std::string audioPath;
	std::string task;
	std::vector<std::string> tags;
};

struct ofxGgmlMusicResult {
	bool success = false;
	std::string text;
	std::string error;
	std::vector<std::string> references;

	explicit operator bool() const {
		return success;
	}
};