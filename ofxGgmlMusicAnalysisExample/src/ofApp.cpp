#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlMusic smoke example");
	gui.setup(nullptr, false);
	request.audioPath = "audio/example.wav";
	status = ofxGgmlMusicUtils::describe(request);
	ofLogNotice("ofxGgmlMusicAnalysisExample") << status;
}

void ofApp::draw() {
	ofBackground(18);
	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 220.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlMusic Analysis Example")) {
		ImGui::TextUnformatted("Audio Analysis Request");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
	}
	ImGui::End();
	gui.end();
	gui.draw();
}
