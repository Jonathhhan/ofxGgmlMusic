#include "ofApp.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace {
	template <std::size_t N>
	void copyToBuffer(std::array<char, N> & buffer, const std::string & value) {
		std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
	}

	std::vector<std::string> splitTags(const std::string & text) {
		std::vector<std::string> tags;
		std::stringstream input(text);
		std::string tag;
		while (std::getline(input, tag, ',')) {
			tag.erase(tag.begin(), std::find_if(tag.begin(), tag.end(), [](unsigned char ch) {
				return !std::isspace(ch);
			}));
			tag.erase(std::find_if(tag.rbegin(), tag.rend(), [](unsigned char ch) {
				return !std::isspace(ch);
			}).base(), tag.end());
			if (!tag.empty()) {
				tags.push_back(tag);
			}
		}
		return tags;
	}

	std::string joinTags(const std::vector<std::string> & tags) {
		std::string text;
		for (std::size_t i = 0; i < tags.size(); ++i) {
			if (i > 0) {
				text += ", ";
			}
			text += tags[i];
		}
		return text;
	}

	ofxGgmlMusicResult makePreviewResult(float durationSeconds) {
		ofxGgmlMusicResult result;
		result.success = true;
		result.text = "deterministic preview result";
		result.tempo = { 96.0f, 0.86f };
		result.key = { "C", "minor", 0.72f };
		const double beatSeconds = 60.0 / result.tempo.bpm;
		for (int i = 0; ; ++i) {
			const double time = static_cast<double>(i) * beatSeconds;
			if (time >= durationSeconds) {
				break;
			}
			result.beats.push_back({ time, i % 4 == 0 ? 0.95f : 0.72f, i % 4 == 0 });
		}
		const std::vector<std::string> chords = { "Cm", "Ab", "Eb", "Bb" };
		for (std::size_t i = 0; i < chords.size(); ++i) {
			result.chords.push_back({
				static_cast<double>(i) * 3.0,
				chords[i],
				0.78f
			});
		}
		result.embedding = { 0.18f, 0.42f, 0.66f, 0.33f, 0.58f, 0.21f };
		result.stems = {
			{ "mix", "stems/mix.wav", 1.0f },
			{ "music", "stems/music.wav", 0.86f },
			{ "percussion", "stems/percussion.wav", 0.72f }
		};
		result.references = { "preview-only", "analysis-output-shape" };
		return result;
	}
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlMusic analysis example");
	gui.setup(nullptr, false);

	taskValues = {
		ofxGgmlMusicTask::Analysis,
		ofxGgmlMusicTask::Tempo,
		ofxGgmlMusicTask::BeatTracking,
		ofxGgmlMusicTask::KeyDetection,
		ofxGgmlMusicTask::ChordRecognition,
		ofxGgmlMusicTask::Embedding,
		ofxGgmlMusicTask::StemSeparation
	};
	taskNames.reserve(taskValues.size());
	for (const auto task : taskValues) {
		taskNames.push_back(ofxGgmlMusicUtils::getTaskName(task));
	}

	request.audioPath = "audio/example.wav";
	request.task = ofxGgmlMusicTask::BeatTracking;
	request.prompt = "find tempo, downbeats, key, and chord hints";
	request.tags = { "demo", "analysis" };
	taskIndex = 2;
	copyToBuffer(audioPathBuffer, request.audioPath);
	copyToBuffer(promptBuffer, request.prompt);
	copyToBuffer(tagsBuffer, joinTags(request.tags));
	refreshPreviewResult();
	updateStatus();
	logRequest();
}

void ofApp::keyPressed(int key) {
	if (key == 'l' || key == 'L') {
		logRequest();
	} else if (key == 't' || key == 'T') {
		cycleTask();
	}
}

void ofApp::rebuildRequest() {
	request.audioPath = audioPathBuffer.data();
	request.prompt = promptBuffer.data();
	request.tags = splitTags(tagsBuffer.data());
	if (!taskValues.empty()) {
		if (taskIndex < 0 || taskIndex >= static_cast<int>(taskValues.size())) {
			taskIndex = 0;
		}
		request.task = taskValues[taskIndex];
	}
	refreshPreviewResult();
	updateStatus();
}

void ofApp::cycleTask() {
	if (taskValues.empty()) {
		return;
	}
	taskIndex = (taskIndex + 1) % static_cast<int>(taskValues.size());
	rebuildRequest();
}

void ofApp::refreshPreviewResult() {
	previewResult = makePreviewResult(previewDurationSeconds);
	previewResult.text = ofxGgmlMusicUtils::getTaskName(request.task) + " preview result";
	if (request.task == ofxGgmlMusicTask::Tempo) {
		previewResult.key = {};
		previewResult.beats.clear();
		previewResult.chords.clear();
		previewResult.embedding.clear();
		previewResult.stems.clear();
	} else if (request.task == ofxGgmlMusicTask::BeatTracking) {
		previewResult.key = {};
		previewResult.chords.clear();
		previewResult.embedding.clear();
		previewResult.stems.clear();
	} else if (request.task == ofxGgmlMusicTask::KeyDetection) {
		previewResult.tempo = {};
		previewResult.beats.clear();
		previewResult.chords.clear();
		previewResult.embedding.clear();
		previewResult.stems.clear();
	} else if (request.task == ofxGgmlMusicTask::ChordRecognition) {
		previewResult.tempo = {};
		previewResult.beats.clear();
		previewResult.embedding.clear();
		previewResult.stems.clear();
	} else if (request.task == ofxGgmlMusicTask::Embedding) {
		previewResult.tempo = {};
		previewResult.key = {};
		previewResult.beats.clear();
		previewResult.chords.clear();
		previewResult.stems.clear();
	} else if (request.task == ofxGgmlMusicTask::StemSeparation) {
		previewResult.tempo = {};
		previewResult.key = {};
		previewResult.beats.clear();
		previewResult.chords.clear();
		previewResult.embedding.clear();
	}
}

std::string ofApp::getRequestSummary() const {
	std::ostringstream summary;
	summary << "Audio path: " << request.audioPath << "\n";
	summary << "Task: " << ofxGgmlMusicUtils::getTaskName(request.task) << "\n";
	summary << "Prompt chars: " << request.prompt.size() << "\n";
	summary << "Tags:";
	if (request.tags.empty()) {
		summary << " none";
	} else {
		for (const auto & tag : request.tags) {
			summary << " " << tag;
		}
	}
	summary << "\n";
	summary << "Preview duration: " << ofToString(previewDurationSeconds, 1) << " s";
	return summary.str();
}

std::string ofApp::getResultSummary() const {
	if (!previewResult) {
		return "No preview result loaded.";
	}
	std::ostringstream summary;
	summary << "Result: " << previewResult.text << "\n";
	summary << "Tempo: ";
	if (ofxGgmlMusicUtils::hasTempo(previewResult)) {
		summary << ofToString(previewResult.tempo.bpm, 0) << " bpm";
		summary << " confidence " << ofToString(previewResult.tempo.confidence, 2);
	} else {
		summary << "(none)";
	}
	summary << "\n";
	summary << "Key: ";
	if (ofxGgmlMusicUtils::hasKey(previewResult)) {
		summary << ofxGgmlMusicUtils::formatKey(previewResult.key);
		summary << " confidence " << ofToString(previewResult.key.confidence, 2);
	} else {
		summary << "(none)";
	}
	summary << "\n";
	summary << "Beats: " << previewResult.beats.size();
	summary << "  Chords: " << previewResult.chords.size();
	summary << "  Embedding values: " << previewResult.embedding.size();
	summary << "  Stems: " << previewResult.stems.size() << "\n";
	summary << "References:";
	if (previewResult.references.empty()) {
		summary << " none";
	} else {
		for (const auto & reference : previewResult.references) {
			summary << " " << reference;
		}
	}
	return summary.str();
}

void ofApp::updateStatus() {
	status = ofxGgmlMusicUtils::describe(request);
	if (ofxGgmlMusicUtils::hasInput(request)) {
		detail = "Configured request is ready for a future analysis backend.";
	} else {
		detail = "Set an audio path before handing this request to an analysis backend.";
	}
}

void ofApp::logRequest() const {
	ofLogNotice("ofxGgmlMusicAnalysisExample") << status;
	if (!request.prompt.empty()) {
		ofLogNotice("ofxGgmlMusicAnalysisExample") << "prompt: " << request.prompt;
	}
	if (!request.tags.empty()) {
		ofLogNotice("ofxGgmlMusicAnalysisExample") << "tags: " << joinTags(request.tags);
	}
}

void ofApp::draw() {
	ofBackground(18);
	drawPreviewTimeline(24.0f, 480.0f, std::max(280.0f, ofGetWidth() - 48.0f), 118.0f);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(620.0f, 430.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlMusic Analysis Example")) {
		bool changed = false;
		changed |= ImGui::InputText("Audio path", audioPathBuffer.data(), audioPathBuffer.size());
		if (!taskNames.empty()) {
			if (taskIndex < 0 || taskIndex >= static_cast<int>(taskNames.size())) {
				taskIndex = 0;
			}
			const auto taskLabel = taskNames[taskIndex].c_str();
			if (ImGui::BeginCombo("Task", taskLabel)) {
				for (int i = 0; i < static_cast<int>(taskNames.size()); ++i) {
					const bool selected = i == taskIndex;
					if (ImGui::Selectable(taskNames[i].c_str(), selected)) {
						taskIndex = i;
						changed = true;
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		changed |= ImGui::InputTextMultiline("Prompt", promptBuffer.data(), promptBuffer.size(), ImVec2(-1.0f, 76.0f));
		changed |= ImGui::InputText("Tags", tagsBuffer.data(), tagsBuffer.size());
		changed |= ImGui::SliderFloat("Preview duration", &previewDurationSeconds, 4.0f, 30.0f, "%.1f s");
		if (changed) {
			rebuildRequest();
		}
		if (ImGui::Button("Update")) {
			rebuildRequest();
		}
		ImGui::SameLine();
		if (ImGui::Button("Next task")) {
			cycleTask();
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh preview")) {
			rebuildRequest();
		}
		ImGui::SameLine();
		if (ImGui::Button("Log request")) {
			rebuildRequest();
			logRequest();
		}
		ImGui::Separator();
		ImGui::Text("Status: %s", ofxGgmlMusicUtils::hasInput(request) ? "ready" : "missing input");
		ImGui::TextWrapped("%s", status.c_str());
		ImGui::TextWrapped("%s", detail.c_str());
		if (ImGui::TreeNode("Shortcuts")) {
			ImGui::TextUnformatted("T: next task");
			ImGui::TextUnformatted("L: log request");
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Current request")) {
			const auto summary = getRequestSummary();
			ImGui::TextWrapped("%s", summary.c_str());
			ImGui::TreePop();
		}
		if (!request.tags.empty()) {
			ImGui::TextWrapped("Tags: %s", joinTags(request.tags).c_str());
		}
		ImGui::Separator();
		ImGui::Text("Preview: %s", previewResult.text.c_str());
		const auto tempoText = ofxGgmlMusicUtils::hasTempo(previewResult)
			? ofToString(previewResult.tempo.bpm, 0) + " bpm"
			: std::string("(none)");
		const auto keyText = ofxGgmlMusicUtils::hasKey(previewResult)
			? ofxGgmlMusicUtils::formatKey(previewResult.key)
			: std::string("(none)");
		ImGui::Text("Tempo: %s  Key: %s  Beats: %d  Chords: %d",
			tempoText.c_str(),
			keyText.c_str(),
			static_cast<int>(previewResult.beats.size()),
			static_cast<int>(previewResult.chords.size()));
		ImGui::Text("Embedding: %d values  Stems: %d",
			static_cast<int>(previewResult.embedding.size()),
			static_cast<int>(previewResult.stems.size()));
		if (ImGui::TreeNode("Last result")) {
			const auto summary = getResultSummary();
			ImGui::TextWrapped("%s", summary.c_str());
			ImGui::TreePop();
		}
	}
	ImGui::End();
	gui.end();
	gui.draw();
}

void ofApp::drawPreviewTimeline(float x, float y, float width, float height) {
	ofSetColor(240);
	ofDrawBitmapString("Preview result timeline", x, y);
	const float plotY = y + 18.0f;
	ofSetColor(70);
	ofNoFill();
	ofDrawRectangle(x, plotY, width, height);
	ofFill();

	if (!previewResult || previewDurationSeconds <= 0.0f) {
		ofSetColor(170);
		ofDrawBitmapString("No preview result", x + 16.0f, plotY + 34.0f);
		return;
	}

	ofSetColor(45, 52, 64, 130);
	ofDrawRectangle(x, plotY, width, height);
	for (const auto & beat : previewResult.beats) {
		const float px = x + static_cast<float>(beat.timeSeconds / previewDurationSeconds) * width;
		ofSetColor(beat.downbeat ? ofColor(245, 176, 65) : ofColor(120));
		ofDrawLine(px, plotY, px, plotY + height);
	}
	for (const auto & chord : previewResult.chords) {
		const float px = x + static_cast<float>(chord.timeSeconds / previewDurationSeconds) * width;
		ofSetColor(105, 205, 185);
		ofDrawCircle(px, plotY + 30.0f, 4.0f);
		ofSetColor(230);
		ofDrawBitmapString(chord.label, px + 6.0f, plotY + 34.0f);
	}
	if (!previewResult.embedding.empty()) {
		const float barWidth = width / static_cast<float>(previewResult.embedding.size());
		for (std::size_t i = 0; i < previewResult.embedding.size(); ++i) {
			const float value = std::max(0.0f, std::min(1.0f, previewResult.embedding[i]));
			ofSetColor(105, 205, 185, 160);
			ofDrawRectangle(
				x + static_cast<float>(i) * barWidth,
				plotY + height - value * 42.0f,
				std::max(2.0f, barWidth - 2.0f),
				value * 42.0f);
		}
	}
	if (!previewResult.stems.empty()) {
		std::string stems = "Stems:";
		for (const auto & stem : previewResult.stems) {
			stems += " " + stem.name;
		}
		ofSetColor(210);
		ofDrawBitmapString(stems, x + 12.0f, plotY + height - 12.0f);
	}
}
