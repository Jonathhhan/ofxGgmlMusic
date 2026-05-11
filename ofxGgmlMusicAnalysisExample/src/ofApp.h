#pragma once

#include "ofMain.h"
#include "ofxGgmlMusic.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	ofxGgmlMusicRequest request;
	std::string status;
};