#pragma once

#define OFXGGML_MUSIC_VERSION_MAJOR 1
#define OFXGGML_MUSIC_VERSION_MINOR 0
#define OFXGGML_MUSIC_VERSION_PATCH 1
#define OFXGGML_MUSIC_VERSION_STRING "1.0.1"

inline const char * ofxGgmlMusicGetVersionString() {
	return OFXGGML_MUSIC_VERSION_STRING;
}
