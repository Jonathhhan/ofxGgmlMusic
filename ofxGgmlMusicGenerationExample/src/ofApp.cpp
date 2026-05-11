#include "ofApp.h"

#include <cstdio>

namespace {
	const char* tonics[] = {
		"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
	};

	const char* modes[] = {
		"major", "minor"
	};
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlMusic generation example");
	gui.setup();
	backend = ofxGgmlMakeProceduralMusicGenerationBackend();
	std::snprintf(promptBuffer.data(), promptBuffer.size(), "%s", "loopable ambient piano motif with granular texture");
	std::snprintf(styleBuffer.data(), styleBuffer.size(), "%s", "ambient");
	rebuildRequest();
	status = "ready";
	detail = backend->getBackendName();
	ofLogNotice("ofxGgmlMusicGenerationExample") << ofxGgmlMusicUtils::describe(request);
}

void ofApp::update() {
}

void ofApp::keyPressed(int key) {
	if (key == 'r' || key == 'R') {
		runGeneration();
	}
	if (key == ' ') {
		if (player.isPlaying()) {
			player.stop();
		} else if (ofFile::doesFileExist(request.outputPath, false)) {
			player.play();
		}
	}
}

void ofApp::rebuildRequest() {
	request.prompt = promptBuffer.data();
	request.style = styleBuffer.data();
	request.outputPath = getOutputPath();
	request.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	request.settings.durationSeconds = duration;
	request.settings.seed = seed;
	request.settings.loop = loop;
	request.tempo.bpm = tempo;
	request.tempo.confidence = 1.0f;
	request.key.tonic = tonics[tonicIndex];
	request.key.mode = modes[modeIndex];
	request.key.confidence = 1.0f;
}

void ofApp::runGeneration() {
	rebuildRequest();
	if (!backend) {
		status = "backend missing";
		detail = "";
		return;
	}

	const auto setupResult = backend->setup(request);
	if (!setupResult) {
		status = "setup failed";
		detail = setupResult.error;
		ofLogWarning("ofxGgmlMusicGenerationExample") << detail;
		return;
	}

	const auto result = backend->generate(request);
	if (!result) {
		status = "generation failed";
		detail = result.error;
		ofLogWarning("ofxGgmlMusicGenerationExample") << detail;
		return;
	}

	status = "complete";
	detail = "Wrote " + result.outputPath;
	player.stop();
	player.load(result.outputPath);
	player.setLoop(loop);
	if (autoPlay) {
		player.play();
	}
	ofLogNotice("ofxGgmlMusicGenerationExample") << detail;
}

std::string ofApp::getOutputPath() const {
	const auto outputDir = ofToDataPath("outputs", true);
	ofDirectory::createDirectory(outputDir, false, true);
	return ofFilePath::join(outputDir, "ofxGgmlMusicGenerationExample.wav");
}

void ofApp::draw() {
	ofBackground(18);
	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_Once);
	ImGui::Begin("ofxGgmlMusic Generation");

	bool changed = false;
	changed |= ImGui::InputTextMultiline("Prompt", promptBuffer.data(), promptBuffer.size(), ImVec2(-1.0f, 84.0f));
	changed |= ImGui::InputText("Style", styleBuffer.data(), styleBuffer.size());
	changed |= ImGui::SliderFloat("Tempo", &tempo, 48.0f, 180.0f, "%.0f bpm");
	changed |= ImGui::SliderFloat("Duration", &duration, 1.0f, 30.0f, "%.1f s");
	changed |= ImGui::InputInt("Seed", &seed);
	changed |= ImGui::Combo("Tonic", &tonicIndex, tonics, 12);
	changed |= ImGui::Combo("Mode", &modeIndex, modes, 2);
	changed |= ImGui::Checkbox("Loop", &loop);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Auto-play", &autoPlay);
	if (changed) {
		rebuildRequest();
		player.setLoop(loop);
	}

	if (ImGui::Button("Generate")) {
		runGeneration();
	}
	ImGui::SameLine();
	if (ImGui::Button(player.isPlaying() ? "Stop" : "Play")) {
		if (player.isPlaying()) {
			player.stop();
		} else if (ofFile::doesFileExist(request.outputPath, false)) {
			player.play();
		}
	}

	ImGui::Separator();
	ImGui::Text("Backend: %s", backend ? backend->getBackendName().c_str() : "(none)");
	ImGui::Text("Status: %s", status.c_str());
	ImGui::TextWrapped("%s", detail.c_str());
	ImGui::TextWrapped("%s", ofxGgmlMusicUtils::describe(request).c_str());
	ImGui::TextWrapped("Output: %s", request.outputPath.c_str());

	ImGui::End();
	gui.end();
}
