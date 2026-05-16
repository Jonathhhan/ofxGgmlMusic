#pragma once

#include "ofMain.h"
#include "ofxGgmlMusic.h"
#include "ofxImGui.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	ofxGgmlMusicRequest request;
	std::string status;
	ofxImGui::Gui gui;
};
