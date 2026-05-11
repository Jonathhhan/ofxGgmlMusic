#pragma once

#include <string>
#include <vector>

struct ofxGgmlMusicAudioBuffer {
	int sampleRate = 0;
	int channels = 0;
	std::vector<float> samples;

	double getDurationSeconds() const;
	float getPeakAbs() const;
	bool isAllocated() const;

	explicit operator bool() const {
		return isAllocated();
	}
};

namespace ofxGgmlMusicAudioUtils {
	bool writeMonoWav16(
		const std::string & path,
		const std::vector<float> & samples,
		int sampleRate,
		std::string & error);

	bool loadWav16(
		const std::string & path,
		ofxGgmlMusicAudioBuffer & buffer,
		std::string & error);
}
