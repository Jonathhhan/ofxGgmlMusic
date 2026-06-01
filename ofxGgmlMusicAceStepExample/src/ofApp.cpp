#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <vector>

namespace {
	const std::string startServerHint =
		"Start the AceStep server with scripts\\start-acestep-server.ps1 "
		"or set OFXGGML_ACESTEP_SERVER_URL to the running AceStep-compatible server.";

	struct PromptPreset {
		std::string name;
		std::string caption;
		std::string negativePrompt;
		std::string keyscale;
		std::string timeSignature;
		float durationSeconds = 30.0f;
		int bpm = 0;
		float lmTemperature = 0.85f;
		float lmCfgScale = 2.0f;
		float lmTopP = 0.9f;
		int lmTopK = 0;
		bool instrumentalOnly = true;
		bool useCotCaption = true;
	};

	const std::vector<PromptPreset> & getPromptPresets() {
		static const std::vector<PromptPreset> presets = {
			{
				"Cinematic Pulse",
				"cinematic electronic instrumental, warm analog pads, plucked arpeggios, "
				"subtle pulse, hopeful nocturnal mood, polished stereo mix",
				"distorted vocals, harsh clipping, noisy mix",
				"C minor",
				"4",
				30.0f,
				96,
				0.85f,
				2.0f,
				0.9f,
				0,
				true,
				true
			},
			{
				"Lo-fi Keys",
				"warm lo-fi keys, mellow bass, brushed percussion, tape texture, "
				"late-night study loop, intimate and relaxed",
				"harsh cymbals, distorted bass, busy lead vocal",
				"D minor",
				"4",
				24.0f,
				76,
				0.75f,
				1.8f,
				0.88f,
				0,
				true,
				true
			},
			{
				"Club Hook",
				"bright dance-pop instrumental hook, punchy drums, sidechain synth bass, "
				"wide chorus energy, clean radio mix",
				"muddy kick, clipped master, spoken intro",
				"A minor",
				"4",
				32.0f,
				124,
				0.95f,
				2.4f,
				0.92f,
				64,
				true,
				true
			}
		};
		return presets;
	}

	void copyToBuffer(std::array<char, 2048> & buffer, const std::string & value) {
		std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
	}

	template <std::size_t N>
	void copyToBuffer(std::array<char, N> & buffer, const std::string & value) {
		std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
	}

	template <std::size_t N>
	void wrapTextBuffer(std::array<char, N> & buffer, float fieldWidth) {
		const float characterWidth = std::max(1.0f, ImGui::CalcTextSize("M").x);
		const int maxLineChars = std::max(
			24,
			static_cast<int>((std::max(96.0f, fieldWidth) - 12.0f) / characterWidth));
		const std::string source(buffer.data());
		std::istringstream sourceLines(source);
		std::ostringstream wrapped;
		std::string line;
		bool firstLine = true;

		while (std::getline(sourceLines, line)) {
			std::istringstream words(line);
			std::string word;
			std::string outputLine;

			while (words >> word) {
				if (static_cast<int>(word.size()) > maxLineChars) {
					if (!outputLine.empty()) {
						if (!firstLine) {
							wrapped << '\n';
						}
						wrapped << outputLine;
						outputLine.clear();
						firstLine = false;
					}
					for (std::size_t offset = 0; offset < word.size();) {
						const std::size_t chunkSize = std::min<std::size_t>(
							static_cast<std::size_t>(maxLineChars),
							word.size() - offset);
						if (!firstLine) {
							wrapped << '\n';
						}
						wrapped << word.substr(offset, chunkSize);
						firstLine = false;
						offset += chunkSize;
					}
					continue;
				}

				const int nextLength = static_cast<int>(
					outputLine.size() + word.size() + (outputLine.empty() ? 0 : 1));
				if (nextLength > maxLineChars && !outputLine.empty()) {
					if (!firstLine) {
						wrapped << '\n';
					}
					wrapped << outputLine;
					outputLine = word;
					firstLine = false;
				} else {
					if (!outputLine.empty()) {
						outputLine += ' ';
					}
					outputLine += word;
				}
			}

			if (!outputLine.empty() || line.empty()) {
				if (!firstLine) {
					wrapped << '\n';
				}
				wrapped << outputLine;
				firstLine = false;
			}
		}

		copyToBuffer(buffer, wrapped.str());
	}

	std::string getEnvOrEmpty(const char * name) {
		return ofGetEnv(name);
	}

	bool isEnvDisabled(const std::string & value) {
		std::string normalized = value;
		std::transform(
			normalized.begin(),
			normalized.end(),
			normalized.begin(),
			[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return normalized == "0" ||
			normalized == "false" ||
			normalized == "no" ||
			normalized == "off" ||
			normalized == "disabled";
	}

	bool shouldAutoStartServer() {
		return !isEnvDisabled(getEnvOrEmpty("OFXGGML_ACESTEP_AUTOSTART"));
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
	// Initialize the OF logger before the first example log call on this VS/OF tree.
	ofLogToConsole();
	ofSetWindowTitle("ofxGgmlMusic AceStep example");
	ofSetFrameRate(60);
	gui.setup(nullptr, false);

	const std::string initialServerUrl = getEnvOrEmpty("OFXGGML_ACESTEP_SERVER_URL");
	copyToBuffer(
		serverUrlBuffer,
		initialServerUrl.empty() ? std::string("http://127.0.0.1:8085") : initialServerUrl);
	copyToBuffer(serverExecutableBuffer, resolveDefaultServerExecutable());
	copyToBuffer(modelPathBuffer, resolveDefaultModelPath());
	copyToBuffer(lyricsBuffer, "[Instrumental]");
	copyToBuffer(outputPrefixBuffer, "ofxGgmlMusicAceStep");
	for (const auto & preset : getPromptPresets()) {
		promptPresetNames.push_back(preset.name);
	}
	applyPromptPreset(0);

	status = "ready";
	detail = "Server: " + std::string(serverUrlBuffer.data()) + ". " + startServerHint;
	ofLogNotice("ofxGgmlMusicAceStepExample") << detail;
	if (shouldAutoStartServer()) {
		requestServerStart();
	}
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
	if (key == 's' || key == 'S') {
		requestServerStart();
	} else if (key == 'p' || key == 'P') {
		cyclePromptPreset();
	} else if (key == 'h' || key == 'H') {
		requestHealth();
	} else if (key == 'g' || key == 'G') {
		requestGeneration();
	} else if (key == 'd' || key == 'D') {
		logRequest();
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

std::string ofApp::getRequestSummary() const {
	std::ostringstream summary;
	summary << "Server: " << ofxGgmlMusicAceStepBridge::normalizeServerUrl(serverUrlBuffer.data()) << "\n";
	summary << "Preset: ";
	if (!promptPresetNames.empty() &&
		promptPresetIndex >= 0 &&
		promptPresetIndex < static_cast<int>(promptPresetNames.size())) {
		summary << promptPresetNames[promptPresetIndex];
	} else {
		summary << "custom";
	}
	summary << "\n";
	summary << "Caption chars: " << std::string(captionBuffer.data()).size();
	summary << "  Lyrics chars: " << std::string(lyricsBuffer.data()).size() << "\n";
	summary << "Key: " << keyscaleBuffer.data();
	summary << "  Time: " << timeSignatureBuffer.data();
	summary << "  Duration: " << ofToString(durationSeconds, 1) << " s";
	summary << "  BPM: " << (bpm > 0 ? ofToString(bpm) : std::string("auto")) << "\n";
	summary << "Seed: " << (seed >= 0 ? ofToString(seed) : std::string("random"));
	summary << "  Batch: " << batchSize;
	summary << "  WAV: " << (wavOutput ? "yes" : "no");
	summary << "  Auto-play: " << (autoPlay ? "yes" : "no") << "\n";
	summary << "LM: temperature " << ofToString(lmTemperature, 2);
	summary << ", cfg " << ofToString(lmCfgScale, 2);
	summary << ", top-p " << ofToString(lmTopP, 2);
	summary << ", top-k " << lmTopK << "\n";
	summary << "Output prefix: " << outputPrefixBuffer.data();
	return summary.str();
}

std::string ofApp::getResultSummary() const {
	if (!lastGenerateResult && lastGenerateResult.outputPath.empty()) {
		return "No AceStep generation result loaded yet.";
	}
	std::ostringstream summary;
	summary << "Server: " << lastGenerateResult.usedServerUrl << "\n";
	summary << "Selected output: " << lastGenerateResult.outputPath << "\n";
	summary << "Returned files: " << lastGenerateResult.outputPaths.size();
	if (lastGenerateResult.elapsedMs > 0.0f) {
		summary << "  Elapsed: " << ofToString(lastGenerateResult.elapsedMs, 1) << " ms";
	}
	summary << "\n";
	summary << "Waveform: ";
	if (waveform) {
		summary << ofToString(waveform.getDurationSeconds(), 2) << " s";
		summary << "  " << waveform.sampleRate << " Hz";
		summary << "  peak " << ofToString(waveform.getPeakAbs(), 2);
	} else {
		summary << "(not loaded)";
	}
	return summary.str();
}

void ofApp::logRequest() const {
	const auto request = buildRequest();
	ofLogNotice("ofxGgmlMusicAceStepExample") << "AceStep request";
	ofLogNotice("ofxGgmlMusicAceStepExample") << getRequestSummary();
	ofLogNotice("ofxGgmlMusicAceStepExample")
		<< ofxGgmlMusicAceStepBridge::summarizeRequestJson(
			ofxGgmlMusicAceStepBridge::buildRequestJson(request));
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

void ofApp::applyPromptPreset(int index) {
	const auto & presets = getPromptPresets();
	if (index < 0 || index >= static_cast<int>(presets.size())) {
		return;
	}
	const auto & preset = presets[static_cast<std::size_t>(index)];
	promptPresetIndex = index;
	copyToBuffer(captionBuffer, preset.caption);
	copyToBuffer(negativePromptBuffer, preset.negativePrompt);
	copyToBuffer(keyscaleBuffer, preset.keyscale);
	copyToBuffer(timeSignatureBuffer, preset.timeSignature);
	durationSeconds = preset.durationSeconds;
	bpm = preset.bpm;
	lmTemperature = preset.lmTemperature;
	lmCfgScale = preset.lmCfgScale;
	lmTopP = preset.lmTopP;
	lmTopK = preset.lmTopK;
	instrumentalOnly = preset.instrumentalOnly;
	useCotCaption = preset.useCotCaption;
	detail = "Loaded prompt preset: " + preset.name;
}

void ofApp::cyclePromptPreset() {
	if (promptPresetNames.empty()) {
		return;
	}
	const int nextIndex = (promptPresetIndex + 1) % static_cast<int>(promptPresetNames.size());
	applyPromptPreset(nextIndex);
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
		refreshGeneratedOutputChoices();
		if (generated) {
			status = "generation complete";
			if (generated.outputPaths.size() > 1) {
				detail = "Wrote " + std::to_string(generated.outputPaths.size()) +
					" files. Previewing " + generated.outputPath;
			} else {
				detail = "Wrote " + generated.outputPath;
			}
			loadGeneratedAudio(generated.outputPath);
			ofLogNotice("ofxGgmlMusicAceStepExample") << detail;
		} else {
			status = "generation failed";
			detail = generated.error;
			ofLogWarning("ofxGgmlMusicAceStepExample") << detail;
		}
	}
}

void ofApp::refreshGeneratedOutputChoices() {
	generatedOutputChoices = lastGenerateResult.outputPaths;
	if (generatedOutputChoices.empty() && !lastGenerateResult.outputPath.empty()) {
		generatedOutputChoices.push_back(lastGenerateResult.outputPath);
	}
	generatedOutputIndex = 0;
	for (int i = 0; i < static_cast<int>(generatedOutputChoices.size()); ++i) {
		if (generatedOutputChoices[i] == lastGenerateResult.outputPath) {
			generatedOutputIndex = i;
			break;
		}
	}
}

void ofApp::selectGeneratedOutput(int index) {
	if (index < 0 || index >= static_cast<int>(generatedOutputChoices.size())) {
		return;
	}
	generatedOutputIndex = index;
	lastGenerateResult.outputPath = generatedOutputChoices[generatedOutputIndex];
	detail = "Previewing " + lastGenerateResult.outputPath;
	loadGeneratedAudio(lastGenerateResult.outputPath);
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
	if (!promptPresetNames.empty()) {
		if (promptPresetIndex < 0 || promptPresetIndex >= static_cast<int>(promptPresetNames.size())) {
			promptPresetIndex = 0;
		}
		const auto presetLabel = promptPresetNames[promptPresetIndex].c_str();
		if (ImGui::BeginCombo("Prompt preset", presetLabel)) {
			for (int i = 0; i < static_cast<int>(promptPresetNames.size()); ++i) {
				const bool selected = i == promptPresetIndex;
				if (ImGui::Selectable(promptPresetNames[i].c_str(), selected)) {
					applyPromptPreset(i);
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
	const float promptFieldWidth = ImGui::CalcItemWidth();
	if (ImGui::InputTextMultiline(
			"Caption",
			captionBuffer.data(),
			captionBuffer.size(),
			ImVec2(promptFieldWidth, 112.0f))) {
		wrapTextBuffer(captionBuffer, promptFieldWidth);
	}
	if (ImGui::InputTextMultiline(
			"Lyrics",
			lyricsBuffer.data(),
			lyricsBuffer.size(),
			ImVec2(promptFieldWidth, 80.0f))) {
		wrapTextBuffer(lyricsBuffer, promptFieldWidth);
	}
	ImGui::InputText("Negative", negativePromptBuffer.data(), negativePromptBuffer.size());
	ImGui::InputText("Keyscale", keyscaleBuffer.data(), keyscaleBuffer.size());
	ImGui::InputText("Time signature", timeSignatureBuffer.data(), timeSignatureBuffer.size());
	ImGui::InputText("Output prefix", outputPrefixBuffer.data(), outputPrefixBuffer.size());
	ImGui::SliderFloat("Duration", &durationSeconds, 4.0f, 240.0f, "%.1f s");
	ImGui::InputInt("BPM", &bpm);
	bpm = std::max(0, bpm);
	ImGui::InputInt("Seed", &seed);
	ImGui::SameLine();
	if (ImGui::Button("New seed")) {
		seed = static_cast<int>(ofRandom(0.0f, 1000000.0f));
	}
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
	if (ImGui::Button("Log request")) {
		logRequest();
	}
	ImGui::SameLine();
	const bool playing = player.isPlaying();
	if (ImGui::Button(playing ? "Stop" : "Play")) {
		if (playing) {
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
	if (ImGui::TreeNode("Shortcuts")) {
		ImGui::TextUnformatted("S: start server");
		ImGui::TextUnformatted("P: next prompt preset");
		ImGui::TextUnformatted("H: health check");
		ImGui::TextUnformatted("G: generate");
		ImGui::TextUnformatted("D: log request");
		ImGui::TextUnformatted("Space: play/stop");
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Current request")) {
		const auto summary = getRequestSummary();
		ImGui::TextWrapped("%s", summary.c_str());
		ImGui::TreePop();
	}
	if (!lastHealthResult.usedServerUrl.empty()) {
		ImGui::TextWrapped("Health URL: %s", lastHealthResult.usedServerUrl.c_str());
	}
	if (!lastGenerateResult.outputPath.empty()) {
		ImGui::TextWrapped("Audio: %s", lastGenerateResult.outputPath.c_str());
	}
	if (ImGui::TreeNode("Last result")) {
		const auto summary = getResultSummary();
		ImGui::TextWrapped("%s", summary.c_str());
		ImGui::TreePop();
	}
	if (!generatedOutputChoices.empty()) {
		if (generatedOutputIndex < 0 ||
			generatedOutputIndex >= static_cast<int>(generatedOutputChoices.size())) {
			generatedOutputIndex = 0;
		}
		const auto outputLabel = ofFilePath::getFileName(generatedOutputChoices[generatedOutputIndex]);
		if (ImGui::BeginCombo("Generated output", outputLabel.c_str())) {
			for (int i = 0; i < static_cast<int>(generatedOutputChoices.size()); ++i) {
				const bool selected = i == generatedOutputIndex;
				const auto label = ofToString(i + 1) + ": " + ofFilePath::getFileName(generatedOutputChoices[i]);
				if (ImGui::Selectable(label.c_str(), selected)) {
					selectGeneratedOutput(i);
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
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
	gui.draw();
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
	if (player.isPlaying()) {
		const float plotY = y + 18.0f;
		const float px = x + player.getPosition() * width;
		ofSetColor(230, 90, 84);
		ofDrawLine(px, plotY, px, plotY + height);
		ofDrawTriangle(px, plotY - 2.0f, px - 5.0f, plotY - 10.0f, px + 5.0f, plotY - 10.0f);
	}

	ofSetColor(210);
	ofDrawBitmapString(
		ofToString(waveform.getDurationSeconds(), 2) + " s  " +
		ofToString(waveform.sampleRate) + " Hz  peak " +
		ofToString(waveform.getPeakAbs(), 2),
		x,
		y + height + 44.0f);
}
