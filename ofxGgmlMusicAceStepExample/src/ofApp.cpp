#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <vector>

namespace {
	const std::string startServerHint =
		"Start the AceStep server with scripts\\start-acestep-server.ps1 "
		"or set OFXGGML_ACESTEP_SERVER_URL to the running AceStep-compatible server.";

	void copyToBuffer(std::array<char, 2048> & buffer, const std::string & value) {
		std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
	}

	template <std::size_t N>
	void copyToBuffer(std::array<char, N> & buffer, const std::string & value) {
		std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
	}

	std::string getEnvOrEmpty(const char * name) {
		return ofGetEnv(name);
	}

	std::string normalizePath(const std::filesystem::path & path) {
		std::error_code ec;
		const auto absolute = std::filesystem::absolute(path, ec);
		if (ec) {
			return path.lexically_normal().string();
		}
		const auto canonical = std::filesystem::weakly_canonical(absolute, ec);
		return (ec ? absolute : canonical).lexically_normal().string();
	}

	std::string findExistingFile(const std::vector<std::filesystem::path> & candidates) {
		for (const auto & candidate : candidates) {
			std::error_code ec;
			if (std::filesystem::is_regular_file(candidate, ec)) {
				return normalizePath(candidate);
			}
		}
		return {};
	}

	std::string findExistingModelDirectory(const std::vector<std::filesystem::path> & candidates) {
		for (const auto & candidate : candidates) {
			std::error_code ec;
			if (!std::filesystem::is_directory(candidate, ec)) {
				continue;
			}
			for (const auto & entry : std::filesystem::directory_iterator(candidate, ec)) {
				if (ec) {
					break;
				}
				if (entry.is_regular_file(ec) && entry.path().extension() == ".gguf") {
					return normalizePath(candidate);
				}
			}
		}
		return {};
	}

	std::filesystem::path getExeDir() {
		return std::filesystem::path(ofFilePath::getCurrentExeDir());
	}

	std::string resolveStartServerScript() {
		const auto current = std::filesystem::current_path();
		const auto exeDir = getExeDir();
		return findExistingFile({
			current / "scripts" / "start-acestep-server.ps1",
			current / ".." / "scripts" / "start-acestep-server.ps1",
			exeDir / ".." / ".." / "scripts" / "start-acestep-server.ps1",
			std::filesystem::path(ofToDataPath("../../../scripts/start-acestep-server.ps1", true))
		});
	}

	std::string resolveDefaultServerExecutable() {
		const std::string fromEnv = getEnvOrEmpty("OFXGGML_ACESTEP_SERVER_EXE");
		if (!fromEnv.empty()) {
			return fromEnv;
		}
		const auto current = std::filesystem::current_path();
		const auto exeDir = getExeDir();
		return findExistingFile({
			current / "libs" / "acestep" / "bin" / "ace-server.exe",
			current / "libs" / "acestep" / "bin" / "ace-server",
			current / ".." / "libs" / "acestep" / "bin" / "ace-server.exe",
			current / ".." / "libs" / "acestep" / "bin" / "ace-server",
			exeDir / ".." / ".." / "libs" / "acestep" / "bin" / "ace-server.exe",
			exeDir / ".." / ".." / "libs" / "acestep" / "bin" / "ace-server"
		});
	}

	std::string resolveDefaultModelPath() {
		const std::string fromEnv = getEnvOrEmpty("OFXGGML_ACESTEP_MODEL_PATH");
		if (!fromEnv.empty()) {
			return fromEnv;
		}
		const auto current = std::filesystem::current_path();
		const auto exeDir = getExeDir();
		return findExistingModelDirectory({
			current / "ofxGgmlMusicAceStepExample" / "bin" / "data" / "models",
			current / "bin" / "data" / "models",
			current / "models" / "acestep",
			current / "data" / "models" / "acestep",
			current / ".." / "ofxGgmlMusicAceStepExample" / "bin" / "data" / "models",
			exeDir / "data" / "models",
			exeDir / ".." / ".." / "ofxGgmlMusicAceStepExample" / "bin" / "data" / "models",
			std::filesystem::path(ofToDataPath("models", true))
		});
	}

	std::string quoteCommandArgument(const std::string & value) {
		std::string quoted = "\"";
		for (char ch : value) {
			if (ch == '"') {
				quoted += "\\\"";
			} else {
				quoted += ch;
			}
		}
		quoted += "\"";
		return quoted;
	}

	std::string buildStartServerCommand(
		const std::string & scriptPath,
		const std::string & serverUrl,
		const std::string & serverExecutable,
		const std::string & modelPath) {
		std::ostringstream command;
#if defined(TARGET_WIN32)
		command << "powershell -ExecutionPolicy Bypass -File "
			<< quoteCommandArgument(scriptPath);
#else
		command << "pwsh -NoProfile -ExecutionPolicy Bypass -File "
			<< quoteCommandArgument(scriptPath);
#endif
		command << " -ServerUrl " << quoteCommandArgument(serverUrl);
		if (!serverExecutable.empty()) {
			command << " -ServerExecutable " << quoteCommandArgument(serverExecutable);
		}
		if (!modelPath.empty()) {
			command << " -ModelPath " << quoteCommandArgument(modelPath);
		}
		command << " -StartupTimeoutSeconds 60";
		return command.str();
	}

	std::string makeServerUnavailableDetail(
		const std::string & serverUrl,
		const std::string & error) {
		std::string detail = "AceStep server is not reachable at " +
			ofxGgmlMusicAceStepBridge::normalizeServerUrl(serverUrl);
		if (!error.empty()) {
			detail += ": " + error;
		}
		detail += ". " + startServerHint;
		return detail;
	}
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlMusic AceStep example");
	ofSetFrameRate(60);
	gui.setup();

	const std::string initialServerUrl = getEnvOrEmpty("OFXGGML_ACESTEP_SERVER_URL");
	copyToBuffer(
		serverUrlBuffer,
		initialServerUrl.empty() ? std::string("http://127.0.0.1:8085") : initialServerUrl);
	copyToBuffer(serverExecutableBuffer, resolveDefaultServerExecutable());
	copyToBuffer(modelPathBuffer, resolveDefaultModelPath());
	copyToBuffer(captionBuffer,
		"cinematic electronic instrumental, warm analog pads, plucked arpeggios, "
		"subtle pulse, hopeful nocturnal mood, polished stereo mix");
	copyToBuffer(lyricsBuffer, "[Instrumental]");
	copyToBuffer(negativePromptBuffer, "distorted vocals, harsh clipping, noisy mix");
	copyToBuffer(keyscaleBuffer, "C minor");
	copyToBuffer(timeSignatureBuffer, "4");
	copyToBuffer(outputPrefixBuffer, "ofxGgmlMusicAceStep");

	status = "ready";
	detail = "Server: " + std::string(serverUrlBuffer.data()) + ". " + startServerHint;
	ofLogNotice("ofxGgmlMusicAceStepExample") << detail;
}

void ofApp::exit() {
	if (workerThread.joinable()) {
		workerThread.join();
	}
}

void ofApp::update() {
	collectWorkerResult();
}

void ofApp::keyPressed(int key) {
	if (key == 'h' || key == 'H') {
		requestHealth();
	} else if (key == 'g' || key == 'G') {
		requestGeneration();
	} else if (key == ' ') {
		if (player.isPlaying()) {
			player.stop();
		} else if (!lastGenerateResult.outputPath.empty()) {
			player.play();
		}
	}
}

std::string ofApp::getOutputDirectory() const {
	const auto outputDir = ofToDataPath("generated/acestep", true);
	ofDirectory::createDirectory(outputDir, false, true);
	return outputDir;
}

ofxGgmlMusicAceStepRequest ofApp::buildRequest() const {
	ofxGgmlMusicAceStepRequest request;
	request.caption = captionBuffer.data();
	request.lyrics = lyricsBuffer.data();
	request.negativePrompt = negativePromptBuffer.data();
	request.keyscale = keyscaleBuffer.data();
	request.timeSignature = timeSignatureBuffer.data();
	request.durationSeconds = durationSeconds;
	request.bpm = bpm;
	request.seed = seed;
	request.batchSize = batchSize;
	request.lmTemperature = lmTemperature;
	request.lmCfgScale = lmCfgScale;
	request.lmTopP = lmTopP;
	request.lmTopK = lmTopK;
	request.instrumentalOnly = instrumentalOnly;
	request.useCotCaption = useCotCaption;
	request.wavOutput = wavOutput;
	request.outputDir = getOutputDirectory();
	request.outputPrefix = outputPrefixBuffer.data();
	return request;
}

void ofApp::requestServerStart() {
	if (workerRunning.load()) {
		return;
	}
	if (workerThread.joinable()) {
		workerThread.join();
	}
	status = "starting server";
	detail = "Starting AceStep server at " +
		ofxGgmlMusicAceStepBridge::normalizeServerUrl(serverUrlBuffer.data());
	ofLogNotice("ofxGgmlMusicAceStepExample") << detail;
	workerRunning.store(true);
	workerThread = std::thread(
		&ofApp::runServerStartWorker,
		this,
		std::string(serverUrlBuffer.data()),
		std::string(serverExecutableBuffer.data()),
		std::string(modelPathBuffer.data()));
}

void ofApp::requestHealth() {
	if (workerRunning.load()) {
		return;
	}
	if (workerThread.joinable()) {
		workerThread.join();
	}
	status = "checking health";
	detail.clear();
	workerRunning.store(true);
	workerThread = std::thread(&ofApp::runHealthWorker, this, std::string(serverUrlBuffer.data()));
}

void ofApp::requestGeneration() {
	if (workerRunning.load()) {
		return;
	}
	if (workerThread.joinable()) {
		workerThread.join();
	}
	const auto request = buildRequest();
	status = "generation running";
	detail = ofxGgmlMusicAceStepBridge::summarizeRequestJson(
		ofxGgmlMusicAceStepBridge::buildRequestJson(request));
	ofLogNotice("ofxGgmlMusicAceStepExample") << "generation running";
	workerRunning.store(true);
	workerThread = std::thread(
		&ofApp::runGenerationWorker,
		this,
		request,
		std::string(serverUrlBuffer.data()));
}

void ofApp::runServerStartWorker(
	std::string serverUrl,
	std::string serverExecutable,
	std::string modelPath) {
	bool success = false;
	std::string resultDetail;
	const std::string scriptPath = resolveStartServerScript();
	if (scriptPath.empty()) {
		resultDetail = "Could not find scripts\\start-acestep-server.ps1 from the example.";
	} else {
		const std::string command =
			buildStartServerCommand(scriptPath, serverUrl, serverExecutable, modelPath);
		const std::string output = ofSystem(command + " 2>&1");
		const auto health = bridge.healthCheck(serverUrl, 2);
		if (health) {
			success = true;
			resultDetail = "AceStep server ready at " +
				ofxGgmlMusicAceStepBridge::normalizeServerUrl(serverUrl);
			{
				std::lock_guard<std::mutex> lock(workerMutex);
				pendingHealthResult = health;
				pendingHealth = true;
			}
		} else {
			resultDetail = makeServerUnavailableDetail(serverUrl, health.error);
			if (!output.empty()) {
				resultDetail += "\nLauncher output:\n" + output;
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(workerMutex);
		pendingServerStartSuccess = success;
		pendingServerStartDetail = resultDetail;
		pendingServerStart = true;
	}
	workerRunning.store(false);
}

void ofApp::runHealthWorker(std::string serverUrl) {
	const auto result = bridge.healthCheck(serverUrl, 2);
	{
		std::lock_guard<std::mutex> lock(workerMutex);
		pendingHealthResult = result;
		pendingHealth = true;
	}
	workerRunning.store(false);
}

void ofApp::runGenerationWorker(ofxGgmlMusicAceStepRequest request, std::string serverUrl) {
	const auto health = bridge.healthCheck(serverUrl, 2);
	if (!health) {
		ofxGgmlMusicAceStepGenerateResult result;
		result.usedServerUrl = ofxGgmlMusicAceStepBridge::normalizeServerUrl(serverUrl);
		result.error = makeServerUnavailableDetail(serverUrl, health.error);
		{
			std::lock_guard<std::mutex> lock(workerMutex);
			pendingGenerateResult = result;
			pendingGenerate = true;
		}
		workerRunning.store(false);
		return;
	}

	const auto result = bridge.generate(request, serverUrl);
	{
		std::lock_guard<std::mutex> lock(workerMutex);
		pendingGenerateResult = result;
		pendingGenerate = true;
	}
	workerRunning.store(false);
}

void ofApp::collectWorkerResult() {
	bool hasHealth = false;
	bool hasGenerate = false;
	bool hasServerStart = false;
	bool serverStartSuccess = false;
	std::string serverStartDetail;
	ofxGgmlMusicAceStepHealthResult health;
	ofxGgmlMusicAceStepGenerateResult generated;
	{
		std::lock_guard<std::mutex> lock(workerMutex);
		if (pendingServerStart) {
			serverStartSuccess = pendingServerStartSuccess;
			serverStartDetail = pendingServerStartDetail;
			pendingServerStart = false;
			hasServerStart = true;
		}
		if (pendingHealth) {
			health = pendingHealthResult;
			pendingHealth = false;
			hasHealth = true;
		}
		if (pendingGenerate) {
			generated = pendingGenerateResult;
			pendingGenerate = false;
			hasGenerate = true;
		}
	}

	if (hasServerStart) {
		status = serverStartSuccess ? "server ready" : "server start failed";
		detail = serverStartDetail;
		if (serverStartSuccess) {
			ofLogNotice("ofxGgmlMusicAceStepExample") << detail;
		} else {
			ofLogWarning("ofxGgmlMusicAceStepExample") << detail;
		}
	}

	if (hasHealth) {
		lastHealthResult = health;
		if (health) {
			status = "server reachable";
			detail = health.status.empty() ? health.usedServerUrl : health.status;
			ofLogNotice("ofxGgmlMusicAceStepExample") << "AceStep health OK: " << health.usedServerUrl;
		} else {
			status = "server unavailable";
			detail = health.error;
			ofLogWarning("ofxGgmlMusicAceStepExample") << detail;
		}
	}

	if (hasGenerate) {
		lastGenerateResult = generated;
		if (generated) {
			status = "generation complete";
			detail = "Wrote " + generated.outputPath;
			loadGeneratedAudio(generated.outputPath);
			ofLogNotice("ofxGgmlMusicAceStepExample") << detail;
		} else {
			status = "generation failed";
			detail = generated.error;
			ofLogWarning("ofxGgmlMusicAceStepExample") << detail;
		}
	}
}

void ofApp::loadGeneratedAudio(const std::string & path) {
	player.stop();
	if (!path.empty() && ofFile::doesFileExist(path, false)) {
		player.load(path);
		player.setLoop(false);
	}

	std::string error;
	if (!ofxGgmlMusicAudioUtils::loadWav16(path, waveform, error)) {
		waveform = {};
		if (wavOutput) {
			ofLogWarning("ofxGgmlMusicAceStepExample") << error;
		}
	}
	if (autoPlay && player.isLoaded()) {
		player.play();
	}
}

void ofApp::draw() {
	ofBackground(14);
	drawWaveform(612.0f, 54.0f, std::max(300.0f, ofGetWidth() - 652.0f), 260.0f);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(560, 610), ImGuiCond_Once);
	ImGui::Begin("ofxGgmlMusic AceStep");

	ImGui::InputText("Server", serverUrlBuffer.data(), serverUrlBuffer.size());
	ImGui::InputText("Server exe", serverExecutableBuffer.data(), serverExecutableBuffer.size());
	ImGui::InputText("Model path", modelPathBuffer.data(), modelPathBuffer.size());
	ImGui::InputTextMultiline("Caption", captionBuffer.data(), captionBuffer.size(), ImVec2(-1.0f, 112.0f));
	ImGui::InputTextMultiline("Lyrics", lyricsBuffer.data(), lyricsBuffer.size(), ImVec2(-1.0f, 80.0f));
	ImGui::InputText("Negative", negativePromptBuffer.data(), negativePromptBuffer.size());
	ImGui::InputText("Keyscale", keyscaleBuffer.data(), keyscaleBuffer.size());
	ImGui::InputText("Time signature", timeSignatureBuffer.data(), timeSignatureBuffer.size());
	ImGui::InputText("Output prefix", outputPrefixBuffer.data(), outputPrefixBuffer.size());
	ImGui::SliderFloat("Duration", &durationSeconds, 4.0f, 240.0f, "%.1f s");
	ImGui::InputInt("BPM", &bpm);
	bpm = std::max(0, bpm);
	ImGui::InputInt("Seed", &seed);
	ImGui::SliderInt("Batch", &batchSize, 1, 9);
	ImGui::SliderFloat("LM temperature", &lmTemperature, 0.0f, 2.0f, "%.2f");
	ImGui::SliderFloat("LM cfg", &lmCfgScale, 0.0f, 8.0f, "%.2f");
	ImGui::SliderFloat("LM top p", &lmTopP, 0.0f, 1.0f, "%.2f");
	ImGui::InputInt("LM top k", &lmTopK);
	lmTopK = std::max(0, lmTopK);
	ImGui::Checkbox("Instrumental", &instrumentalOnly);
	ImGui::SameLine();
	ImGui::Checkbox("Use CoT caption", &useCotCaption);
	ImGui::SameLine();
	ImGui::Checkbox("WAV", &wavOutput);
	ImGui::Checkbox("Auto-play", &autoPlay);

	const bool busy = workerRunning.load();
	if (busy) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Start server")) {
		requestServerStart();
	}
	ImGui::SameLine();
	if (ImGui::Button("Health")) {
		requestHealth();
	}
	ImGui::SameLine();
	if (ImGui::Button("Generate")) {
		requestGeneration();
	}
	ImGui::SameLine();
	if (ImGui::Button(player.isPlaying() ? "Stop" : "Play")) {
		if (player.isPlaying()) {
			player.stop();
		} else if (!lastGenerateResult.outputPath.empty()) {
			player.play();
		}
	}
	if (busy) {
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	ImGui::Text("Status: %s", status.c_str());
	ImGui::TextWrapped("%s", detail.c_str());
	if (!lastHealthResult.usedServerUrl.empty()) {
		ImGui::TextWrapped("Health URL: %s", lastHealthResult.usedServerUrl.c_str());
	}
	if (!lastGenerateResult.outputPath.empty()) {
		ImGui::TextWrapped("Audio: %s", lastGenerateResult.outputPath.c_str());
	}
	if (lastGenerateResult.elapsedMs > 0.0f) {
		ImGui::Text("Elapsed: %.1f ms", lastGenerateResult.elapsedMs);
	}
	if (!lastGenerateResult.requestJson.empty()) {
		if (ImGui::TreeNode("Request JSON")) {
			ImGui::TextWrapped("%s", lastGenerateResult.requestJson.c_str());
			ImGui::TreePop();
		}
	}
	if (!lastGenerateResult.enrichedRequestJson.empty()) {
		if (ImGui::TreeNode("LM result")) {
			ImGui::TextWrapped("%s", lastGenerateResult.enrichedRequestJson.c_str());
			ImGui::TreePop();
		}
	}

	ImGui::End();
	gui.end();
}

void ofApp::drawWaveform(float x, float y, float width, float height) {
	ofSetColor(240);
	ofDrawBitmapString("AceStep output", x, y);
	ofSetColor(70);
	ofNoFill();
	ofDrawRectangle(x, y + 18.0f, width, height);
	ofFill();

	if (!waveform) {
		ofSetColor(170);
		ofDrawBitmapString("Generate WAV audio to preview waveform", x + 16.0f, y + 48.0f);
		return;
	}

	const float midY = y + 18.0f + height * 0.5f;
	const int columns = std::max(1, static_cast<int>(width));
	const auto samplesPerColumn =
		std::max<std::size_t>(1, waveform.samples.size() / static_cast<std::size_t>(columns));
	ofSetColor(105, 205, 185);
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

	ofSetColor(210);
	ofDrawBitmapString(
		ofToString(waveform.getDurationSeconds(), 2) + " s  " +
		ofToString(waveform.sampleRate) + " Hz  peak " +
		ofToString(waveform.getPeakAbs(), 2),
		x,
		y + height + 44.0f);
}
