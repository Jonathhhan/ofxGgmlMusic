#include "ofApp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
	const char* tonics[] = {
		"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
	};

	const char* modes[] = {
		"major", "minor"
	};

	const char* presets[] = {
		"ambient", "lofi", "pulse"
	};
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlMusic generation example");
	gui.setup();
	backend = ofxGgmlMakeProceduralMusicGenerationBackend();
	ofxGgmlMusicUtils::applyGenerationPreset("ambient", request);
	syncControlsFromRequest();
	rebuildRequest();
	status = "ready";
	detail = backend->getBackendName();
	loadExistingRender();
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
		} else if (ofFile::doesFileExist(getPlayablePath(), false)) {
			player.play();
		}
	}
}

void ofApp::applyPreset(int index) {
	if (index < 0 || index >= 3) {
		return;
	}
	ofxGgmlMusicGenerationRequest presetRequest;
	presetRequest.outputPath = getOutputPath();
	presetRequest.settings.seed = seed;
	presetRequest.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	if (!ofxGgmlMusicUtils::applyGenerationPreset(presets[index], presetRequest)) {
		return;
	}
	request = presetRequest;
	syncControlsFromRequest();
	rebuildRequest();
}

void ofApp::syncControlsFromRequest() {
	std::snprintf(promptBuffer.data(), promptBuffer.size(), "%s", request.prompt.c_str());
	std::snprintf(styleBuffer.data(), styleBuffer.size(), "%s", request.style.c_str());
	tempo = request.tempo.bpm > 0.0f ? request.tempo.bpm : tempo;
	duration = static_cast<float>(request.settings.durationSeconds);
	loop = request.settings.loop;
	for (int i = 0; i < 12; ++i) {
		if (request.key.tonic == tonics[i]) {
			tonicIndex = i;
			break;
		}
	}
	for (int i = 0; i < 2; ++i) {
		if (request.key.mode == modes[i]) {
			modeIndex = i;
			break;
		}
	}
	exportMelodyStem = false;
	exportBassStem = false;
	exportPulseStem = false;
	for (const auto & stem : request.targetStems) {
		exportMelodyStem = exportMelodyStem || stem == "melody";
		exportBassStem = exportBassStem || stem == "bass";
		exportPulseStem = exportPulseStem || stem == "pulse";
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
	request.targetStems.clear();
	if (exportMelodyStem) {
		request.targetStems.push_back("melody");
	}
	if (exportBassStem) {
		request.targetStems.push_back("bass");
	}
	if (exportPulseStem) {
		request.targetStems.push_back("pulse");
	}
}

void ofApp::runGeneration() {
	currentOutputPath = getNextOutputPath();
	rebuildRequest();
	if (!backend) {
		status = "backend missing";
		detail = "";
		return;
	}

	const auto setupResult = backend->setup(request);
	if (!setupResult) {
		lastResult = setupResult;
		status = "setup failed";
		detail = setupResult.error;
		ofLogWarning("ofxGgmlMusicGenerationExample") << detail;
		return;
	}

	const auto result = backend->generate(request);
	if (!result) {
		lastResult = result;
		status = "generation failed";
		detail = result.error;
		ofLogWarning("ofxGgmlMusicGenerationExample") << detail;
		return;
	}

	lastResult = result;
	status = "complete";
	detail = "Wrote " + result.outputPath;
	refreshGenerationHistory();
	player.stop();
	player.load(result.outputPath);
	player.setLoop(loop);
	loadWaveform();
	if (autoPlay) {
		player.play();
	}
	ofLogNotice("ofxGgmlMusicGenerationExample") << detail;
}

std::string ofApp::getOutputDirectory() const {
	const auto outputDir = ofToDataPath("outputs", true);
	ofDirectory::createDirectory(outputDir, false, true);
	return outputDir;
}

std::string ofApp::getOutputPath() const {
	if (!currentOutputPath.empty()) {
		return currentOutputPath;
	}
	return ofFilePath::join(getOutputDirectory(), "ofxGgmlMusicGenerationExample.wav");
}

std::string ofApp::getNextOutputPath() {
	const auto outputDir = getOutputDirectory();
	for (;;) {
		const auto name = "ofxGgmlMusicGenerationExample-" +
			ofToString(ofGetUnixTime()) + "-" +
			ofToString(renderSerial++) + ".wav";
		const auto path = ofFilePath::join(outputDir, name);
		if (!ofFile::doesFileExist(path, false)) {
			return path;
		}
	}
}

std::string ofApp::getManifestPath() const {
	return ofxGgmlMusicUtils::getGenerationManifestPath(getOutputPath());
}

std::string ofApp::getHistoryPath() const {
	return ofxGgmlMusicUtils::getGenerationHistoryPath(getOutputPath());
}

std::string ofApp::getPlayablePath() const {
	if (!lastResult.outputPath.empty()) {
		return lastResult.outputPath;
	}
	return request.outputPath;
}

void ofApp::loadRenderManifest(const std::string & manifestPath) {
	std::string error;
	ofxGgmlMusicGenerationResult loaded;
	if (!ofxGgmlMusicUtils::loadGenerationManifest(manifestPath, loaded, error)) {
		status = "manifest load failed";
		detail = error;
		ofLogWarning("ofxGgmlMusicGenerationExample") << detail;
		return;
	}

	lastResult = loaded;
	currentOutputPath = loaded.outputPath;
	if (ofFile::doesFileExist(lastResult.outputPath, false)) {
		player.stop();
		player.load(lastResult.outputPath);
		player.setLoop(loop);
		loadWaveform();
		status = "loaded";
		detail = "Loaded " + lastResult.outputPath;
	} else {
		status = "manifest loaded";
		detail = "Audio file missing: " + lastResult.outputPath;
	}
}

void ofApp::refreshGenerationHistory() {
	historyManifestPaths.clear();
	const auto historyPath = getHistoryPath();
	if (!ofFile::doesFileExist(historyPath, false)) {
		historyIndex = 0;
		return;
	}

	std::string error;
	if (!ofxGgmlMusicUtils::loadGenerationHistory(historyPath, historyManifestPaths, error)) {
		historyIndex = 0;
		ofLogWarning("ofxGgmlMusicGenerationExample") << error;
		return;
	}
	if (historyIndex >= static_cast<int>(historyManifestPaths.size())) {
		historyIndex = 0;
	}
}

void ofApp::loadExistingRender() {
	refreshGenerationHistory();
	if (!historyManifestPaths.empty()) {
		loadRenderManifest(historyManifestPaths[historyIndex]);
		return;
	}

	const auto manifestPath = getManifestPath();
	if (ofFile::doesFileExist(manifestPath, false)) {
		loadRenderManifest(manifestPath);
	}
}

void ofApp::loadWaveform() {
	std::string error;
	const auto path = lastResult.outputPath.empty() ? request.outputPath : lastResult.outputPath;
	if (!ofxGgmlMusicAudioUtils::loadWav16(path, waveform, error)) {
		waveform = {};
		ofLogWarning("ofxGgmlMusicGenerationExample") << error;
	}
}

void ofApp::draw() {
	ofBackground(18);

	drawWaveform(608.0f, 48.0f, std::max(280.0f, ofGetWidth() - 640.0f), 220.0f);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_Once);
	ImGui::Begin("ofxGgmlMusic Generation");

	bool changed = false;
	if (ImGui::Combo("Preset", &presetIndex, presets, 3)) {
		applyPreset(presetIndex);
		changed = false;
	}
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
	changed |= ImGui::Checkbox("Melody stem", &exportMelodyStem);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Bass stem", &exportBassStem);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Pulse stem", &exportPulseStem);
	if (changed) {
		rebuildRequest();
		player.setLoop(loop);
	}

	if (ImGui::Button("Generate")) {
		runGeneration();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload")) {
		loadExistingRender();
	}
	ImGui::SameLine();
	if (ImGui::Button(player.isPlaying() ? "Stop" : "Play")) {
		if (player.isPlaying()) {
			player.stop();
		} else if (ofFile::doesFileExist(getPlayablePath(), false)) {
			player.play();
		}
	}

	ImGui::Separator();
	ImGui::Text("Backend: %s", backend ? backend->getBackendName().c_str() : "(none)");
	ImGui::Text("Status: %s", status.c_str());
	ImGui::TextWrapped("%s", detail.c_str());
	ImGui::TextWrapped("%s", ofxGgmlMusicUtils::describe(request).c_str());
	ImGui::TextWrapped("Output: %s", request.outputPath.c_str());
	if (!lastResult.manifestPath.empty()) {
		ImGui::TextWrapped("Manifest: %s", lastResult.manifestPath.c_str());
	}
	if (!lastResult.midiPath.empty()) {
		ImGui::TextWrapped("MIDI: %s", lastResult.midiPath.c_str());
	}
	if (!lastResult.chordMidiPath.empty()) {
		ImGui::TextWrapped("Chord MIDI: %s", lastResult.chordMidiPath.c_str());
	}
	if (!lastResult.arrangementMidiPath.empty()) {
		ImGui::TextWrapped("Arrangement MIDI: %s", lastResult.arrangementMidiPath.c_str());
	}
	if (!historyManifestPaths.empty()) {
		const auto recentLabel = ofFilePath::getFileName(historyManifestPaths[historyIndex]);
		if (ImGui::BeginCombo("Recent", recentLabel.c_str())) {
			for (int i = 0; i < static_cast<int>(historyManifestPaths.size()); ++i) {
				const bool selected = i == historyIndex;
				const auto label = ofToString(i + 1) + ": " + ofFilePath::getFileName(historyManifestPaths[i]);
				if (ImGui::Selectable(label.c_str(), selected)) {
					historyIndex = i;
					loadRenderManifest(historyManifestPaths[historyIndex]);
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
	if (lastResult.sampleRate > 0) {
		ImGui::Text("Audio: %.2f s, %d Hz, peak %.2f",
			lastResult.durationSeconds,
			lastResult.sampleRate,
			lastResult.peakAbs);
	}
	if (!lastResult.beats.empty() || !lastResult.chords.empty()) {
		ImGui::Text("Timing: %d beats, %d chords, %d sections",
			static_cast<int>(lastResult.beats.size()),
			static_cast<int>(lastResult.chords.size()),
			static_cast<int>(lastResult.sections.size()));
	}
	if (!lastResult.stems.empty()) {
		ImGui::Text("Stems: %d", static_cast<int>(lastResult.stems.size()));
		for (const auto & stem : lastResult.stems) {
			ImGui::TextWrapped("%s: %s", stem.name.c_str(), stem.path.c_str());
		}
	}

	ImGui::End();
	gui.end();
}

void ofApp::drawWaveform(float x, float y, float width, float height) {
	ofSetColor(240);
	ofDrawBitmapString("Waveform", x, y);
	ofSetColor(70);
	ofNoFill();
	ofDrawRectangle(x, y + 18.0f, width, height);
	ofFill();

	if (!waveform) {
		ofSetColor(170);
		ofDrawBitmapString("Generate a sketch to preview audio", x + 16.0f, y + 48.0f);
		return;
	}

	const float midY = y + 18.0f + height * 0.5f;
	const float plotY = y + 18.0f;
	ofSetColor(105, 205, 185);
	const int columns = std::max(1, static_cast<int>(width));
	const auto samplesPerColumn = std::max<std::size_t>(1, waveform.samples.size() / static_cast<std::size_t>(columns));
	for (int column = 0; column < columns; ++column) {
		const auto begin = static_cast<std::size_t>(column) * samplesPerColumn;
		const auto end = std::min(waveform.samples.size(), begin + samplesPerColumn);
		float peak = 0.0f;
		for (auto i = begin; i < end; ++i) {
			peak = std::max(peak, std::abs(waveform.samples[i]));
		}
		const float px = x + static_cast<float>(column);
		const float py = peak * height * 0.46f;
		ofDrawLine(px, midY - py, px, midY + py);
	}

	if (lastResult.durationSeconds > 0.0) {
		for (std::size_t i = 0; i < lastResult.sections.size(); ++i) {
			const auto & section = lastResult.sections[i];
			const float px = x + static_cast<float>(section.startSeconds / lastResult.durationSeconds) * width;
			const float sectionWidth = static_cast<float>(section.durationSeconds / lastResult.durationSeconds) * width;
			ofSetColor(i % 2 == 0 ? ofColor(55, 65, 78, 120) : ofColor(45, 52, 64, 120));
			ofDrawRectangle(px, plotY, sectionWidth, height);
			ofSetColor(210);
			ofDrawBitmapString(section.name, px + 4.0f, plotY + height - 8.0f);
		}
		for (const auto & beat : lastResult.beats) {
			const float px = x + static_cast<float>(beat.timeSeconds / lastResult.durationSeconds) * width;
			ofSetColor(beat.downbeat ? ofColor(245, 176, 65) : ofColor(130));
			ofDrawLine(px, plotY, px, plotY + height);
		}
		for (const auto & chord : lastResult.chords) {
			const float px = x + static_cast<float>(chord.timeSeconds / lastResult.durationSeconds) * width;
			ofSetColor(245, 176, 65);
			ofDrawBitmapString(chord.label, px + 4.0f, plotY + 16.0f);
		}
	}

	ofSetColor(210);
	ofDrawBitmapString(
		ofToString(waveform.getDurationSeconds(), 2) + " s  " +
		ofToString(waveform.sampleRate) + " Hz  peak " +
		ofToString(waveform.getPeakAbs(), 2),
		x,
		y + height + 44.0f);
}
