#include "ofApp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <iterator>
#include <sstream>

namespace {
	int clampIndex(int index, std::size_t size) {
		if (size == 0) {
			return 0;
		}
		if (index < 0 || index >= static_cast<int>(size)) {
			return 0;
		}
		return index;
	}

	template <std::size_t Size>
	std::string readTextBuffer(const std::array<char, Size> & buffer) {
		const auto end = std::find(buffer.begin(), buffer.end(), '\0');
		return std::string(buffer.begin(), end);
	}

	bool isEditingTextField() {
		return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
	}

	bool isGuiCapturingMouse() {
		return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
	}

	void drawPlaybackCursor(float x, float y, float width, float height, float position) {
		const float plotY = y + 18.0f;
		const float cursorX = x + std::clamp(position, 0.0f, 1.0f) * width;
		const float markerY = std::max(4.0f, plotY - 10.0f);

		ofSetLineWidth(5.0f);
		ofSetColor(0, 0, 0, 210);
		ofDrawLine(cursorX, plotY, cursorX, plotY + height);
		ofDrawTriangle(cursorX, markerY - 2.0f, cursorX - 7.0f, markerY - 12.0f, cursorX + 7.0f, markerY - 12.0f);

		ofSetLineWidth(2.0f);
		ofSetColor(255, 244, 120);
		ofDrawLine(cursorX, plotY, cursorX, plotY + height);
		ofDrawTriangle(cursorX, markerY, cursorX - 5.0f, markerY - 8.0f, cursorX + 5.0f, markerY - 8.0f);
		ofSetLineWidth(1.0f);
	}

	void updatePlaybackScrub(
		ofSoundPlayer & player,
		bool & scrubbing,
		float x,
		float y,
		float width,
		float height) {
		if (!player.isLoaded()) {
			scrubbing = false;
			return;
		}

		const float plotY = y + 18.0f;
		const bool pressed = ofGetMousePressed(OF_MOUSE_BUTTON_LEFT);
		const float mouseX = static_cast<float>(ofGetMouseX());
		const float mouseY = static_cast<float>(ofGetMouseY());
		const bool inside =
			mouseX >= x && mouseX <= x + width &&
			mouseY >= plotY && mouseY <= plotY + height;

		if (pressed && (inside || scrubbing) && (scrubbing || !isGuiCapturingMouse())) {
			scrubbing = true;
			const float position = std::clamp((mouseX - x) / std::max(1.0f, width), 0.0f, 1.0f);
			player.setPosition(position);
		} else if (!pressed) {
			scrubbing = false;
		}
	}
}

void ofApp::GenerationWorker::start() {
	if (!isThreadRunning()) {
		startThread();
	}
}

void ofApp::GenerationWorker::stop() {
	jobs.close();
	waitForThread(true);
	completedJobs.close();
}

bool ofApp::GenerationWorker::submit(GenerationJob job) {
	if (busy.exchange(true)) {
		return false;
	}
	const bool sent = jobs.send(std::move(job));
	if (!sent) {
		busy.store(false);
	}
	return sent;
}

bool ofApp::GenerationWorker::tryReceive(GenerationCompleted & completed) {
	return completedJobs.tryReceive(completed);
}

bool ofApp::GenerationWorker::isBusy() const {
	return busy.load();
}

void ofApp::GenerationWorker::threadedFunction() {
	GenerationJob job;
	while (jobs.receive(job)) {
		GenerationCompleted completed;
		completed.loop = job.request.settings.loop;
		const auto startedAt = std::chrono::steady_clock::now();
		try {
			if (!backend) {
				backend = ofxGgmlMakeProceduralMusicGenerationBackend();
			}
			if (!backend) {
				completed.status = "backend missing";
				completed.result.error = "Could not create the procedural music backend.";
			} else {
				completed.backendName = backend->getBackendName();
				ofLogNotice("ofxGgmlMusicGenerationExample")
					<< "generation executing on ofThread worker with " << completed.backendName;
				const auto setupResult = backend->setup(job.request);
				if (!setupResult) {
					completed.status = "setup failed";
					completed.result = setupResult;
				} else {
					completed.result = backend->generate(job.request);
					completed.status = completed.result ? "complete" : "generation failed";
				}
			}
		} catch (const std::exception & error) {
			completed.status = "generation worker failed";
			completed.result.error = error.what();
		} catch (...) {
			completed.status = "generation worker failed";
			completed.result.error = "Unknown generation error.";
		}
		completed.elapsedMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - startedAt).count();
		if (completed.result) {
			ofLogNotice("ofxGgmlMusicGenerationExample")
				<< "generation completed on ofThread worker in "
				<< ofToString(completed.elapsedMs, 1) << " ms";
		} else {
			ofLogWarning("ofxGgmlMusicGenerationExample")
				<< completed.status << ": " << completed.result.error;
		}
		completedJobs.send(std::move(completed));
		busy.store(false);
	}
	if (backend) {
		backend->close();
		backend.reset();
	}
	busy.store(false);
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlMusic generation example");
	gui.setup(nullptr, false);
	presetNames = ofxGgmlMusicUtils::getGenerationPresetNames();
	stemNames = ofxGgmlMusicUtils::getGenerationStemNames();
	keyTonics = ofxGgmlMusicUtils::getGenerationKeyTonics();
	keyModes = ofxGgmlMusicUtils::getGenerationKeyModes();
	generationWorker.start();
	ofxGgmlMusicUtils::applyGenerationPreset("ambient", request);
	syncControlsFromRequest();
	rebuildRequest();
	status = "ready";
	detail = backendName + " ready on ofThread worker";
	loadExistingRender();
	ofLogNotice("ofxGgmlMusicGenerationExample") << ofxGgmlMusicUtils::describe(request);
}

void ofApp::update() {
	GenerationCompleted completed;
	while (generationWorker.tryReceive(completed)) {
		backendName = completed.backendName;
		lastResult = std::move(completed.result);
		status = completed.status;
		if (!lastResult) {
			detail = lastResult.error;
			continue;
		}

		detail = "Wrote " + lastResult.outputPath + " in " +
			ofToString(completed.elapsedMs, 1) + " ms";
		refreshGenerationHistory();
		player.stop();
		player.load(lastResult.outputPath);
		player.setLoop(completed.loop);
		loadWaveform();
		if (autoPlay) {
			player.play();
		}
		ofLogNotice("ofxGgmlMusicGenerationExample") << detail;
	}
}

void ofApp::exit() {
	generationWorker.stop();
}

void ofApp::keyPressed(int key) {
	if (isEditingTextField()) {
		return;
	}
	if (key == 'r' || key == 'R') {
		runGeneration();
	} else if (key == 'p' || key == 'P') {
		cyclePreset();
	} else if (key == 'n' || key == 'N') {
		assignRandomSeed();
		rebuildRequest();
	} else if (key == 'l' || key == 'L') {
		loadExistingRender();
	} else if (key == 'd' || key == 'D') {
		logRequest();
	} else if (key == ' ') {
		if (player.isPlaying()) {
			player.stop();
		} else if (ofFile::doesFileExist(getPlayablePath(), false)) {
			player.play();
		}
	}
}

void ofApp::assignRandomSeed() {
	seed = static_cast<int>(ofRandom(0.0f, 1000000.0f));
}

void ofApp::applyPreset(int index) {
	if (index < 0 || index >= static_cast<int>(presetNames.size())) {
		return;
	}
	ofxGgmlMusicGenerationRequest presetRequest;
	presetRequest.outputPath = getOutputPath();
	presetRequest.settings.seed = seed;
	presetRequest.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	if (!ofxGgmlMusicUtils::applyGenerationPreset(presetNames[index], presetRequest)) {
		return;
	}
	request = presetRequest;
	syncControlsFromRequest();
	rebuildRequest();
}

void ofApp::cyclePreset() {
	if (presetNames.empty()) {
		return;
	}
	presetIndex = (presetIndex + 1) % static_cast<int>(presetNames.size());
	applyPreset(presetIndex);
}

void ofApp::syncControlsFromRequest() {
	tonicIndex = clampIndex(tonicIndex, keyTonics.size());
	modeIndex = clampIndex(modeIndex, keyModes.size());
	std::snprintf(promptBuffer.data(), promptBuffer.size(), "%s", request.prompt.c_str());
	std::snprintf(
		negativePromptBuffer.data(),
		negativePromptBuffer.size(),
		"%s",
		request.negativePrompt.c_str());
	std::snprintf(styleBuffer.data(), styleBuffer.size(), "%s", request.style.c_str());
	tempo = request.tempo.bpm > 0.0f ? request.tempo.bpm : tempo;
	duration = static_cast<float>(request.settings.durationSeconds);
	loop = request.settings.loop;
	for (int i = 0; i < static_cast<int>(keyTonics.size()); ++i) {
		if (request.key.tonic == keyTonics[i]) {
			tonicIndex = i;
			break;
		}
	}
	for (int i = 0; i < static_cast<int>(keyModes.size()); ++i) {
		if (request.key.mode == keyModes[i]) {
			modeIndex = i;
			break;
		}
	}
	stemEnabled.assign(stemNames.size(), false);
	for (const auto & stem : request.targetStems) {
		const auto found = std::find(stemNames.begin(), stemNames.end(), stem);
		if (found != stemNames.end()) {
			stemEnabled[static_cast<std::size_t>(std::distance(stemNames.begin(), found))] = true;
		}
	}
}

void ofApp::rebuildRequest() {
	request.prompt = readTextBuffer(promptBuffer);
	request.negativePrompt = readTextBuffer(negativePromptBuffer);
	request.style = readTextBuffer(styleBuffer);
	request.outputPath = getOutputPath();
	request.settings.backend = ofxGgmlMusicGenerationBackendFamily::External;
	request.settings.durationSeconds = duration;
	request.settings.seed = seed;
	request.settings.loop = loop;
	request.tempo.bpm = tempo;
	request.tempo.confidence = 1.0f;
	tonicIndex = clampIndex(tonicIndex, keyTonics.size());
	modeIndex = clampIndex(modeIndex, keyModes.size());
	request.key.tonic = keyTonics.empty() ? "C" : keyTonics[tonicIndex];
	request.key.mode = keyModes.empty() ? "major" : keyModes[modeIndex];
	request.key.confidence = 1.0f;
	request.targetStems.clear();
	if (stemEnabled.size() != stemNames.size()) {
		stemEnabled.resize(stemNames.size(), false);
	}
	for (std::size_t i = 0; i < stemNames.size(); ++i) {
		if (stemEnabled[i]) {
			request.targetStems.push_back(stemNames[i]);
		}
	}
}

void ofApp::runGeneration() {
	if (generationWorker.isBusy()) {
		status = "generation already running";
		detail = "Wait for the current ofThread worker job to finish.";
		return;
	}
	currentOutputPath = getNextOutputPath();
	rebuildRequest();
	GenerationJob job;
	job.request = request;
	status = "generation running";
	detail = "Procedural render executing on ofThread worker...";
	if (!generationWorker.submit(std::move(job))) {
		status = "generation unavailable";
		detail = "Could not submit work to the ofThread worker.";
	}
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

std::string ofApp::getRequestSummary() const {
	std::ostringstream summary;
	summary << "Preset: ";
	if (!presetNames.empty() &&
		presetIndex >= 0 &&
		presetIndex < static_cast<int>(presetNames.size())) {
		summary << presetNames[presetIndex];
	} else {
		summary << "custom";
	}
	summary << "\n";
	summary << "Prompt chars: " << readTextBuffer(promptBuffer).size();
	summary << "  Negative chars: " << readTextBuffer(negativePromptBuffer).size();
	summary << "  Style: " << readTextBuffer(styleBuffer) << "\n";
	const auto keyTonic =
		(!keyTonics.empty() && tonicIndex >= 0 && tonicIndex < static_cast<int>(keyTonics.size()))
			? keyTonics[tonicIndex]
			: std::string("C");
	const auto keyMode =
		(!keyModes.empty() && modeIndex >= 0 && modeIndex < static_cast<int>(keyModes.size()))
			? keyModes[modeIndex]
			: std::string("major");
	summary << "Tempo: " << ofToString(tempo, 0) << " bpm";
	summary << "  Key: " << keyTonic << " " << keyMode;
	summary << "  Duration: " << ofToString(duration, 1) << " s\n";
	summary << "Seed: " << seed;
	summary << "  Loop: " << (loop ? "yes" : "no");
	summary << "  Auto-play: " << (autoPlay ? "yes" : "no") << "\n";
	summary << "Target stems:";
	bool anyStem = false;
	for (std::size_t i = 0; i < stemNames.size() && i < stemEnabled.size(); ++i) {
		if (stemEnabled[i]) {
			summary << " " << stemNames[i];
			anyStem = true;
		}
	}
	if (!anyStem) {
		summary << " none";
	}
	summary << "\nOutput: " << request.outputPath;
	return summary.str();
}

std::string ofApp::getResultSummary() const {
	if (!lastResult && lastResult.outputPath.empty()) {
		return "No generation result loaded yet.";
	}
	std::ostringstream summary;
	summary << "Output: " << lastResult.outputPath << "\n";
	if (!lastResult.manifestPath.empty()) {
		summary << "Manifest: " << lastResult.manifestPath << "\n";
	}
	summary << "Duration: " << ofToString(lastResult.durationSeconds, 2) << " s";
	if (lastResult.sampleRate > 0) {
		summary << "  Sample rate: " << lastResult.sampleRate << " Hz";
	}
	if (lastResult.channels > 0) {
		summary << "  Channels: " << lastResult.channels;
	}
	summary << "\n";
	summary << "Peak: " << ofToString(lastResult.peakAbs, 2);
	summary << "  Seed: " << lastResult.seed << "\n";
	summary << "Tempo: ";
	if (ofxGgmlMusicUtils::hasTempo(lastResult)) {
		summary << ofToString(lastResult.tempo.bpm, 0) << " bpm";
	} else {
		summary << "(none)";
	}
	summary << "  Key: ";
	if (ofxGgmlMusicUtils::hasKey(lastResult)) {
		summary << ofxGgmlMusicUtils::formatKey(lastResult.key);
	} else {
		summary << "(none)";
	}
	summary << "\n";
	summary << "Beats: " << lastResult.beats.size();
	summary << "  Chords: " << lastResult.chords.size();
	summary << "  Sections: " << lastResult.sections.size();
	summary << "  Stems: " << lastResult.stems.size();
	return summary.str();
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
	historyIndex = clampIndex(historyIndex, historyManifestPaths.size());
}

void ofApp::loadExistingRender() {
	refreshGenerationHistory();
	if (!historyManifestPaths.empty()) {
		historyIndex = clampIndex(historyIndex, historyManifestPaths.size());
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

void ofApp::logRequest() const {
	ofLogNotice("ofxGgmlMusicGenerationExample") << ofxGgmlMusicUtils::describe(request);
	ofLogNotice("ofxGgmlMusicGenerationExample") << getRequestSummary();
}

void ofApp::draw() {
	ofBackground(18);

	drawWaveform(608.0f, 48.0f, std::max(280.0f, ofGetWidth() - 640.0f), 220.0f);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_Once);
	ImGui::Begin("ofxGgmlMusic Generation");

	bool changed = false;
	presetIndex = clampIndex(presetIndex, presetNames.size());
	const auto presetLabel = presetNames.empty() ? "(none)" : presetNames[presetIndex].c_str();
	if (ImGui::BeginCombo("Preset", presetLabel)) {
		for (int i = 0; i < static_cast<int>(presetNames.size()); ++i) {
			const bool selected = i == presetIndex;
			if (ImGui::Selectable(presetNames[i].c_str(), selected)) {
				presetIndex = i;
				applyPreset(presetIndex);
				changed = false;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	changed |= ImGui::InputTextMultiline("Prompt", promptBuffer.data(), promptBuffer.size(), ImVec2(-1.0f, 84.0f));
	changed |= ImGui::InputTextMultiline(
		"Negative prompt",
		negativePromptBuffer.data(),
		negativePromptBuffer.size(),
		ImVec2(-1.0f, 44.0f));
	changed |= ImGui::InputText("Style", styleBuffer.data(), styleBuffer.size());
	changed |= ImGui::SliderFloat("Tempo", &tempo, 48.0f, 180.0f, "%.0f bpm");
	changed |= ImGui::SliderFloat("Duration", &duration, 1.0f, 30.0f, "%.1f s");
	changed |= ImGui::InputInt("Seed", &seed);
	ImGui::SameLine();
	if (ImGui::Button("New seed")) {
		assignRandomSeed();
		changed = true;
	}
	tonicIndex = clampIndex(tonicIndex, keyTonics.size());
	const auto tonicLabel = keyTonics.empty() ? "(none)" : keyTonics[tonicIndex].c_str();
	if (ImGui::BeginCombo("Tonic", tonicLabel)) {
		for (int i = 0; i < static_cast<int>(keyTonics.size()); ++i) {
			const bool selected = i == tonicIndex;
			if (ImGui::Selectable(keyTonics[i].c_str(), selected)) {
				tonicIndex = i;
				changed = true;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	modeIndex = clampIndex(modeIndex, keyModes.size());
	const auto modeLabel = keyModes.empty() ? "(none)" : keyModes[modeIndex].c_str();
	if (ImGui::BeginCombo("Mode", modeLabel)) {
		for (int i = 0; i < static_cast<int>(keyModes.size()); ++i) {
			const bool selected = i == modeIndex;
			if (ImGui::Selectable(keyModes[i].c_str(), selected)) {
				modeIndex = i;
				changed = true;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	const bool loopChanged = ImGui::Checkbox("Loop", &loop);
	changed |= loopChanged;
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Auto-play", &autoPlay);
	if (stemEnabled.size() != stemNames.size()) {
		stemEnabled.resize(stemNames.size(), false);
	}
	for (std::size_t i = 0; i < stemNames.size(); ++i) {
		bool selected = stemEnabled[i];
		const auto label = stemNames[i] + " stem";
		if (ImGui::Checkbox(label.c_str(), &selected)) {
			stemEnabled[i] = selected;
			changed = true;
		}
		if (i + 1 < stemNames.size()) {
			ImGui::SameLine();
		}
	}
	if (changed) {
		rebuildRequest();
		if (loopChanged) {
			player.setLoop(loop);
		}
	}

	const bool generationRunning = generationWorker.isBusy();
	if (generationRunning) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button(generationRunning ? "Generating..." : "Generate")) {
		runGeneration();
	}
	if (generationRunning) {
		ImGui::EndDisabled();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload")) {
		loadExistingRender();
	}
	ImGui::SameLine();
	if (ImGui::Button("Log request")) {
		logRequest();
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
	ImGui::Text("Backend: %s", backendName.c_str());
	ImGui::Text("Execution: %s", generationRunning ? "ofThread worker (busy)" : "ofThread worker (idle)");
	ImGui::Text("Status: %s", status.c_str());
	ImGui::TextWrapped("%s", detail.c_str());
	if (ImGui::TreeNode("Shortcuts")) {
		ImGui::TextUnformatted("R: generate");
		ImGui::TextUnformatted("P: next preset");
		ImGui::TextUnformatted("N: new seed");
		ImGui::TextUnformatted("L: reload recent output");
		ImGui::TextUnformatted("D: log request");
		ImGui::TextUnformatted("Space: play/stop");
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Current request")) {
		const auto summary = getRequestSummary();
		ImGui::TextWrapped("%s", summary.c_str());
		ImGui::TreePop();
	}
	ImGui::TextWrapped("%s", ofxGgmlMusicUtils::describe(request).c_str());
	ImGui::TextWrapped("Output: %s", request.outputPath.c_str());
	if (!lastResult.manifestPath.empty()) {
		ImGui::TextWrapped("Manifest: %s", lastResult.manifestPath.c_str());
	}
	if (ImGui::TreeNode("Last result")) {
		const auto summary = getResultSummary();
		ImGui::TextWrapped("%s", summary.c_str());
		ImGui::TreePop();
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
		historyIndex = clampIndex(historyIndex, historyManifestPaths.size());
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
	gui.draw();
}

void ofApp::drawWaveform(float x, float y, float width, float height) {
	ofSetColor(240);
	ofDrawBitmapString("Waveform", x, y);
	ofSetColor(70);
	ofNoFill();
	ofDrawRectangle(x, y + 18.0f, width, height);
	ofFill();
	updatePlaybackScrub(player, waveformScrubbing, x, y, width, height);

	if (!waveform) {
		ofSetColor(170);
		ofDrawBitmapString("Generate a sketch to preview audio", x + 16.0f, y + 48.0f);
		if (player.isPlaying() || waveformScrubbing) {
			drawPlaybackCursor(x, y, width, height, player.getPosition());
		}
		return;
	}

	const float midY = y + 18.0f + height * 0.5f;
	const float plotY = y + 18.0f;
	if (lastResult.durationSeconds > 0.0) {
		for (std::size_t i = 0; i < lastResult.sections.size(); ++i) {
			const auto & section = lastResult.sections[i];
			const float px = x + static_cast<float>(section.startSeconds / lastResult.durationSeconds) * width;
			const float sectionWidth =
				std::max(1.0f, static_cast<float>(section.durationSeconds / lastResult.durationSeconds) * width);
			ofSetColor(i % 2 == 0 ? ofColor(55, 65, 78, 120) : ofColor(45, 52, 64, 120));
			ofDrawRectangle(px, plotY, sectionWidth, height);
			ofSetColor(210);
			ofDrawBitmapString(section.name, px + 4.0f, plotY + height - 8.0f);
		}
	}

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

	if (player.isPlaying() || waveformScrubbing) {
		drawPlaybackCursor(x, y, width, height, player.getPosition());
	}

	ofSetColor(210);
	ofDrawBitmapString(
		ofToString(waveform.getDurationSeconds(), 2) + " s  " +
		ofToString(waveform.sampleRate) + " Hz  peak " +
		ofToString(waveform.getPeakAbs(), 2),
		x,
		y + height + 44.0f);
}
