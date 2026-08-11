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
		std::string lyrics;
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
				"[Instrumental]",
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
				"Berlin Motorik",
				"krautrock motorik groove, locked 4/4 drums, muted bass guitar, "
				"phased organ drones, tape delay guitar, hypnotic forward motion",
				"[Instrumental]",
				"arena rock solo, EDM riser, glossy pop vocal",
				"E minor",
				"4",
				42.0f,
				128,
				0.88f,
				2.2f,
				0.9f,
				32,
				true,
				true
			},
			{
				"Avant Jazz Cells",
				"avant-garde jazz quartet, fractured upright bass ostinato, brushed snare, "
				"prepared piano clusters, muted trumpet fragments, spacious room tone",
				"[Instrumental]",
				"smooth jazz, fusion shred, quantized club beat",
				"Bb minor",
				"5",
				36.0f,
				92,
				1.05f,
				2.6f,
				0.93f,
				48,
				true,
				true
			},
			{
				"Dub Techno Fog",
				"deep dub techno, sub kick, soft chord stabs, filtered noise wash, "
				"long tape delays, minimal evolving groove, smoky warehouse space",
				"[Instrumental]",
				"bright supersaw, busy lead melody, rock drums",
				"F minor",
				"4",
				48.0f,
				124,
				0.82f,
				2.0f,
				0.88f,
				24,
				true,
				true
			},
			{
				"Microscopic Ambient",
				"microscopic ambient electronica, granular piano dust, sine tone halos, "
				"subtle field recordings, slow spectral bloom, weightless and intimate",
				"[Instrumental]",
				"driving drums, pop chorus, aggressive bass",
				"C major",
				"4",
				60.0f,
				0,
				0.72f,
				1.7f,
				0.86f,
				0,
				true,
				true
			},
			{
				"Broken Beat Lab",
				"experimental broken beat, off-grid rim clicks, rubbery synth bass, "
				"glitch edits, warm Rhodes chords, left-field club pressure",
				"[Instrumental]",
				"straight rock groove, orchestral brass, lead vocal",
				"G minor",
				"4",
				34.0f,
				138,
				0.95f,
				2.3f,
				0.91f,
				64,
				true,
				true
			},
			{
				"Freeform Modular",
				"avant-garde modular synth study, pulsing low oscillators, unstable clocks, "
				"ring-modulated bells, feedback swells, abstract but musical contour",
				"[Instrumental]",
				"commercial EDM drop, pop vocal hook, acoustic strumming",
				"Db minor",
				"7",
				38.0f,
				108,
				1.15f,
				2.8f,
				0.95f,
				96,
				true,
				true
			},
			{
				"ECM Afterimage",
				"Nordic chamber jazz, lyrical piano, brushed drums, bowed bass, "
				"soft trumpet melody, icy reverb, restrained emotional arc",
				"[Instrumental]",
				"funk slap bass, distorted guitar, club kick",
				"D minor",
				"4",
				44.0f,
				68,
				0.78f,
				1.9f,
				0.88f,
				16,
				true,
				true
			},
			{
				"Minimal Techno Wire",
				"minimal techno tool, tight kick, metallic hats, one-note acid pulse, "
				"micro-edited percussion, dry and efficient late-night mix",
				"[Instrumental]",
				"lush pads, guitar solo, cinematic strings",
				"A minor",
				"4",
				32.0f,
				132,
				0.78f,
				2.1f,
				0.86f,
				32,
				true,
				true
			},
			{
				"Kosmische Drift",
				"kosmische ambient krautrock, slow sequencer pattern, airy Mellotron choir, "
				"clean bass pulses, shimmering guitar harmonics, cosmic travelogue mood",
				"[Instrumental]",
				"hard rock drums, diva vocal, trap hi hats",
				"A major",
				"4",
				58.0f,
				104,
				0.84f,
				2.0f,
				0.9f,
				32,
				true,
				true
			},
			{
				"Industrial Nocturne",
				"dark experimental electronica, distant factory percussion, bowed metal, "
				"sub drones, degraded cassette piano, tense nocturnal atmosphere",
				"[Instrumental]",
				"happy ukulele, bright pop claps, clean radio vocal",
				"F# minor",
				"4",
				40.0f,
				88,
				0.92f,
				2.4f,
				0.91f,
				48,
				true,
				true
			},
			{
				"Liquid Detroit",
				"melodic Detroit techno, soulful minor seventh chords, rolling 909 groove, "
				"warm analog bass, glassy lead motif, optimistic night-drive feeling",
				"[Instrumental]",
				"distorted hardcore kick, rock guitar, busy vocal",
				"C minor",
				"4",
				42.0f,
				126,
				0.86f,
				2.2f,
				0.9f,
				48,
				true,
				true
			},
			{
				"Post-Rock Nebula",
				"ambient post-rock electronica, tremolo guitar clouds, soft electronic drums, "
				"pulsing synth bass, slow crescendo, cinematic but intimate",
				"[Instrumental]",
				"metal breakdown, EDM snare build, lead vocal",
				"E major",
				"6",
				52.0f,
				82,
				0.8f,
				1.9f,
				0.88f,
				16,
				true,
				true
			},
			{
				"Fourth World Dub",
				"fourth world ambient dub, hand percussion, marimba-like FM tones, "
				"deep bass echoes, flute phrases, humid nocturnal space",
				"[Instrumental]",
				"stadium drums, distorted lead guitar, glossy EDM synth",
				"G minor",
				"4",
				46.0f,
				94,
				0.9f,
				2.1f,
				0.91f,
				32,
				true,
				true
			},
			{
				"No Wave Disco",
				"arty no wave disco, angular guitar scratches, dry funk bass, "
				"primitive drum machine, saxophone stabs, tense downtown energy",
				"[Instrumental]",
				"smooth lounge, big trance pads, clean pop vocal",
				"B minor",
				"4",
				34.0f,
				116,
				1.0f,
				2.5f,
				0.92f,
				64,
				true,
				true
			},
			{
				"Submerged Aria",
				"avant ambient vocal texture, breathy wordless voice, glass harmonics, "
				"submerged piano, slow electronic swells, dreamlike and abstract",
				"[verse]\n"
				"Under glass I hear the tide\n"
				"Every room becomes a sky",
				"belting, obvious pop chorus, harsh clipping",
				"Ab major",
				"4",
				44.0f,
				64,
				0.92f,
				2.2f,
				0.9f,
				32,
				false,
				true
			},
			{
				"Futurist Bossa",
				"future bossa electronica, syncopated nylon guitar, soft modular bass, "
				"brushed percussion, crystalline pads, elegant asymmetric groove",
				"[Instrumental]",
				"arena snare, distorted synth lead, trap bass",
				"D major",
				"4",
				38.0f,
				112,
				0.82f,
				2.0f,
				0.89f,
				24,
				true,
				true
			},
			{
				"Glitch Hymnal",
				"experimental electronica hymn, chopped choir grains, organ drones, "
				"clicking percussion, warm tape saturation, sacred but synthetic",
				"[Instrumental]",
				"rock power chords, EDM riser, bright pop drums",
				"Eb minor",
				"4",
				48.0f,
				72,
				0.94f,
				2.3f,
				0.91f,
				64,
				true,
				true
			},
			{
				"Afro-Kraut Circuit",
				"afro-krautrock jam, motorik drums, interlocking guitar ostinatos, "
				"rubbery synth bass, hand percussion, long trance-like build",
				"[Instrumental]",
				"arena chorus, heavy metal solo, glossy synthwave",
				"E minor",
				"4",
				54.0f,
				118,
				0.9f,
				2.2f,
				0.91f,
				48,
				true,
				true
			},
			{
				"Spectral Garage",
				"left-field UK garage, shuffled drums, spectral vocal chops, sub bass, "
				"icy pads, syncopated organ stab, intimate underground mix",
				"[Instrumental]",
				"four-on-floor techno only, rock drums, acoustic folk guitar",
				"F minor",
				"4",
				30.0f,
				136,
				0.9f,
				2.35f,
				0.92f,
				64,
				true,
				true
			},
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
	std::string readTextBuffer(const std::array<char, N> & buffer) {
		const auto end = std::find(buffer.begin(), buffer.end(), '\0');
		return std::string(buffer.begin(), end);
	}

	std::string trimText(const std::string & text) {
		const auto begin = std::find_if_not(
			text.begin(),
			text.end(),
			[](unsigned char ch) { return std::isspace(ch) != 0; });
		const auto end = std::find_if_not(
			text.rbegin(),
			text.rend(),
			[](unsigned char ch) { return std::isspace(ch) != 0; }).base();
		if (begin >= end) {
			return {};
		}
		return std::string(begin, end);
	}

	std::string lowerText(std::string text) {
		std::transform(
			text.begin(),
			text.end(),
			text.begin(),
			[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return text;
	}

	bool isInstrumentalLyrics(const std::string & text) {
		const auto normalized = lowerText(trimText(text));
		return normalized == "[instrumental]" || normalized == "instrumental";
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
	if (isEditingTextField()) {
		return;
	}
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
	summary << "Caption chars: " << readTextBuffer(captionBuffer).size();
	summary << "  Lyrics chars: " << readTextBuffer(lyricsBuffer).size() << "\n";
	summary << "Key: " << readTextBuffer(keyscaleBuffer);
	summary << "  Time: " << readTextBuffer(timeSignatureBuffer);
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
	summary << "Output prefix: " << readTextBuffer(outputPrefixBuffer);
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
	request.caption = readTextBuffer(captionBuffer);
	request.lyrics = readTextBuffer(lyricsBuffer);
	request.negativePrompt = readTextBuffer(negativePromptBuffer);
	request.keyscale = readTextBuffer(keyscaleBuffer);
	request.timeSignature = readTextBuffer(timeSignatureBuffer);
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
	request.outputPrefix = readTextBuffer(outputPrefixBuffer);
	if (request.instrumentalOnly) {
		request.lyrics = "[Instrumental]";
	} else if (isInstrumentalLyrics(request.lyrics)) {
		request.lyrics.clear();
	}
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
	copyToBuffer(lyricsBuffer, preset.lyrics);
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
	ImGui::SameLine();
	if (ImGui::Button("Browse...##acestep-model-folder")) {
		auto selected = ofSystemLoadDialog(
			"Choose ACE-Step model folder",
			true,
			modelPathBuffer.data());
		if (selected.bSuccess) {
			copyToBuffer(modelPathBuffer, selected.getPath());
			status = "Selected ACE-Step model folder";
		}
	}
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
	ImGui::InputTextMultiline(
		"Lyrics",
		lyricsBuffer.data(),
		lyricsBuffer.size(),
		ImVec2(promptFieldWidth, 80.0f));
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
	if (ImGui::Checkbox("Instrumental", &instrumentalOnly)) {
		if (instrumentalOnly) {
			copyToBuffer(lyricsBuffer, "[Instrumental]");
		} else if (isInstrumentalLyrics(readTextBuffer(lyricsBuffer))) {
			copyToBuffer(lyricsBuffer, "");
		}
	}
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
	updatePlaybackScrub(player, waveformScrubbing, x, y, width, height);

	if (!waveform) {
		ofSetColor(170);
		ofDrawBitmapString("Generate WAV audio to preview waveform", x + 16.0f, y + 48.0f);
		if (player.isPlaying() || waveformScrubbing) {
			drawPlaybackCursor(x, y, width, height, player.getPosition());
		}
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
