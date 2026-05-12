#pragma once

#include <string>
#include <vector>

struct ofxGgmlMusicMidiNote {
	double startSeconds = 0.0;
	double durationSeconds = 0.0;
	int pitch = 60;
	int velocity = 96;
};

struct ofxGgmlMusicMidiTrack {
	std::string name;
	int program = 0;
	std::vector<ofxGgmlMusicMidiNote> notes;
};

namespace ofxGgmlMusicMidiUtils {
	bool writeMidiFile(
		const std::string & path,
		const std::vector<ofxGgmlMusicMidiNote> & notes,
		float bpm,
		std::string & error);

	bool writeMidiFile(
		const std::string & path,
		const std::vector<ofxGgmlMusicMidiTrack> & tracks,
		float bpm,
		std::string & error);
}
