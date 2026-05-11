#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlMusic smoke example");
	request.audioPath = "audio/example.wav";
	status = ofxGgmlMusicUtils::describe(request);
	ofLogNotice("ofxGgmlMusicAnalysisExample") << status;
}

void ofApp::draw() {
	ofBackground(18);
	ofSetColor(240);
	ofDrawBitmapString("ofxGgmlMusic", 32, 48);
	ofDrawBitmapString(status, 32, 78);
}