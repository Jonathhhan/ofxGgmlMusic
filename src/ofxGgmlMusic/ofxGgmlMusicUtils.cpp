#include "ofxGgmlMusicUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
	constexpr std::size_t maxGenerationHistoryEntries = 64;

	std::string escapeJson(const std::string & text) {
		std::ostringstream escaped;
		for (const auto c : text) {
			switch (c) {
			case '\\':
				escaped << "\\\\";
				break;
			case '"':
				escaped << "\\\"";
				break;
			case '\n':
				escaped << "\\n";
				break;
			case '\r':
				escaped << "\\r";
				break;
			case '\t':
				escaped << "\\t";
				break;
			default:
				escaped << c;
				break;
			}
		}
		return escaped.str();
	}

	std::string quoteJson(const std::string & text) {
		return "\"" + escapeJson(text) + "\"";
	}

	std::string unescapeJson(const std::string & text) {
		std::string unescaped;
		bool escaping = false;
		for (const auto c : text) {
			if (escaping) {
				switch (c) {
				case 'n':
					unescaped.push_back('\n');
					break;
				case 'r':
					unescaped.push_back('\r');
					break;
				case 't':
					unescaped.push_back('\t');
					break;
				default:
					unescaped.push_back(c);
					break;
				}
				escaping = false;
			} else if (c == '\\') {
				escaping = true;
			} else {
				unescaped.push_back(c);
			}
		}
		return unescaped;
	}

	std::string normalizeName(const std::string & text) {
		std::string normalized;
		for (const auto c : text) {
			if (std::isalnum(static_cast<unsigned char>(c))) {
				normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			} else if (c == '-' || c == '_') {
				normalized.push_back(c);
			}
		}
		return normalized;
	}

	std::string readTextFile(const std::string & path, std::string & error) {
		std::ifstream input(path, std::ios::in);
		if (!input) {
			error = "could not open generation manifest";
			return "";
		}
		std::ostringstream text;
		text << input.rdbuf();
		return text.str();
	}

	std::size_t findMatching(
		const std::string & text,
		std::size_t open,
		char openChar,
		char closeChar) {
		int depth = 0;
		bool inString = false;
		bool escaping = false;
		for (std::size_t i = open; i < text.size(); ++i) {
			const auto c = text[i];
			if (inString) {
				if (escaping) {
					escaping = false;
				} else if (c == '\\') {
					escaping = true;
				} else if (c == '"') {
					inString = false;
				}
				continue;
			}
			if (c == '"') {
				inString = true;
			} else if (c == openChar) {
				++depth;
			} else if (c == closeChar) {
				--depth;
				if (depth == 0) {
					return i;
				}
			}
		}
		return std::string::npos;
	}

	std::size_t findKeyValueStart(const std::string & text, const std::string & key) {
		const auto keyText = quoteJson(key);
		const auto keyPos = text.find(keyText);
		if (keyPos == std::string::npos) {
			return std::string::npos;
		}
		const auto colon = text.find(':', keyPos + keyText.size());
		if (colon == std::string::npos) {
			return std::string::npos;
		}
		auto value = colon + 1;
		while (value < text.size() && std::isspace(static_cast<unsigned char>(text[value]))) {
			++value;
		}
		return value;
	}

	std::string extractString(const std::string & text, const std::string & key) {
		const auto start = findKeyValueStart(text, key);
		if (start == std::string::npos || start >= text.size() || text[start] != '"') {
			return "";
		}
		bool escaping = false;
		for (std::size_t i = start + 1; i < text.size(); ++i) {
			const auto c = text[i];
			if (escaping) {
				escaping = false;
			} else if (c == '\\') {
				escaping = true;
			} else if (c == '"') {
				return unescapeJson(text.substr(start + 1, i - start - 1));
			}
		}
		return "";
	}

	std::string extractToken(const std::string & text, const std::string & key) {
		const auto start = findKeyValueStart(text, key);
		if (start == std::string::npos) {
			return "";
		}
		auto end = start;
		while (end < text.size() &&
			text[end] != ',' &&
			text[end] != '\n' &&
			text[end] != '\r' &&
			text[end] != '}' &&
			text[end] != ']') {
			++end;
		}
		auto token = text.substr(start, end - start);
		while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) {
			token.pop_back();
		}
		return token;
	}

	double extractDouble(const std::string & text, const std::string & key, double fallback = 0.0) {
		try {
			const auto token = extractToken(text, key);
			return token.empty() ? fallback : std::stod(token);
		} catch (const std::exception &) {
			return fallback;
		}
	}

	int extractInt(const std::string & text, const std::string & key, int fallback = 0) {
		try {
			const auto token = extractToken(text, key);
			return token.empty() ? fallback : std::stoi(token);
		} catch (const std::exception &) {
			return fallback;
		}
	}

	bool extractBool(const std::string & text, const std::string & key) {
		return extractToken(text, key) == "true";
	}

	std::string extractObject(const std::string & text, const std::string & key) {
		const auto start = findKeyValueStart(text, key);
		if (start == std::string::npos || start >= text.size() || text[start] != '{') {
			return "";
		}
		const auto end = findMatching(text, start, '{', '}');
		return end == std::string::npos ? "" : text.substr(start, end - start + 1);
	}

	std::string extractArray(const std::string & text, const std::string & key) {
		const auto start = findKeyValueStart(text, key);
		if (start == std::string::npos || start >= text.size() || text[start] != '[') {
			return "";
		}
		const auto end = findMatching(text, start, '[', ']');
		return end == std::string::npos ? "" : text.substr(start + 1, end - start - 1);
	}

	std::vector<std::string> splitArrayObjects(const std::string & arrayText) {
		std::vector<std::string> objects;
		std::size_t cursor = 0;
		while (cursor < arrayText.size()) {
			const auto start = arrayText.find('{', cursor);
			if (start == std::string::npos) {
				break;
			}
			const auto end = findMatching(arrayText, start, '{', '}');
			if (end == std::string::npos) {
				break;
			}
			objects.push_back(arrayText.substr(start, end - start + 1));
			cursor = end + 1;
		}
		return objects;
	}

	std::vector<std::string> parseStringArray(const std::string & arrayText) {
		std::vector<std::string> values;
		std::size_t cursor = 0;
		while (cursor < arrayText.size()) {
			const auto start = arrayText.find('"', cursor);
			if (start == std::string::npos) {
				break;
			}
			bool escaping = false;
			for (std::size_t i = start + 1; i < arrayText.size(); ++i) {
				const auto c = arrayText[i];
				if (escaping) {
					escaping = false;
				} else if (c == '\\') {
					escaping = true;
				} else if (c == '"') {
					values.push_back(unescapeJson(arrayText.substr(start + 1, i - start - 1)));
					cursor = i + 1;
					break;
				}
			}
			if (cursor <= start) {
				break;
			}
		}
		return values;
	}
}

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request) {
		return !request.audioPath.empty();
	}

	bool hasPrompt(const ofxGgmlMusicGenerationRequest & request) {
		return !request.prompt.empty();
	}

	bool hasOutput(const ofxGgmlMusicGenerationResult & result) {
		return !result.outputPath.empty();
	}

	bool hasTempo(const ofxGgmlMusicResult & result) {
		return result.tempo.bpm > 0.0f;
	}

	bool hasTempo(const ofxGgmlMusicGenerationRequest & request) {
		return request.tempo.bpm > 0.0f;
	}

	bool hasTempo(const ofxGgmlMusicGenerationResult & result) {
		return result.tempo.bpm > 0.0f;
	}

	bool hasKey(const ofxGgmlMusicResult & result) {
		return !result.key.tonic.empty() || !result.key.mode.empty();
	}

	bool hasKey(const ofxGgmlMusicGenerationRequest & request) {
		return !request.key.tonic.empty() || !request.key.mode.empty();
	}

	bool hasKey(const ofxGgmlMusicGenerationResult & result) {
		return !result.key.tonic.empty() || !result.key.mode.empty();
	}

	std::string getTaskName(ofxGgmlMusicTask task) {
		switch (task) {
		case ofxGgmlMusicTask::Analysis:
			return "analysis";
		case ofxGgmlMusicTask::Tempo:
			return "tempo";
		case ofxGgmlMusicTask::BeatTracking:
			return "beat tracking";
		case ofxGgmlMusicTask::KeyDetection:
			return "key detection";
		case ofxGgmlMusicTask::ChordRecognition:
			return "chord recognition";
		case ofxGgmlMusicTask::Embedding:
			return "embedding";
		case ofxGgmlMusicTask::StemSeparation:
			return "stem separation";
		case ofxGgmlMusicTask::Generation:
			return "generation";
		default:
			return "music";
		}
	}

	std::string getGenerationBackendName(ofxGgmlMusicGenerationBackendFamily backend) {
		switch (backend) {
		case ofxGgmlMusicGenerationBackendFamily::Auto:
			return "auto";
		case ofxGgmlMusicGenerationBackendFamily::Diffusion:
			return "diffusion";
		case ofxGgmlMusicGenerationBackendFamily::Transformer:
			return "transformer";
		case ofxGgmlMusicGenerationBackendFamily::SampleRNN:
			return "sample-rnn";
		case ofxGgmlMusicGenerationBackendFamily::External:
			return "external";
		default:
			return "unknown";
		}
	}

	std::string formatKey(const ofxGgmlMusicKey & key) {
		if (key.tonic.empty() && key.mode.empty()) {
			return "";
		}
		if (key.mode.empty()) {
			return key.tonic;
		}
		if (key.tonic.empty()) {
			return key.mode;
		}
		return key.tonic + " " + key.mode;
	}

	std::vector<std::string> getGenerationPresetNames() {
		return { "ambient", "lofi", "pulse" };
	}

	bool applyGenerationPreset(
		const std::string & presetName,
		ofxGgmlMusicGenerationRequest & request) {
		const auto preset = normalizeName(presetName);
		if (preset == "ambient") {
			if (request.prompt.empty()) {
				request.prompt = "loopable ambient piano motif with granular texture";
			}
			request.style = "ambient";
			request.settings.durationSeconds = 8.0;
			request.settings.loop = true;
			request.tempo = { 92.0f, 1.0f };
			request.key = { "C", "major", 1.0f };
			request.targetStems = { "melody", "bass" };
			return true;
		}
		if (preset == "lofi" || preset == "lo-fi") {
			if (request.prompt.empty()) {
				request.prompt = "warm lofi keys with soft pulse and tape texture";
			}
			request.style = "lofi";
			request.settings.durationSeconds = 10.0;
			request.settings.loop = true;
			request.tempo = { 76.0f, 1.0f };
			request.key = { "D", "minor", 1.0f };
			request.targetStems = { "melody", "bass", "pulse" };
			return true;
		}
		if (preset == "pulse" || preset == "pulsed") {
			if (request.prompt.empty()) {
				request.prompt = "tight pulsing synth pattern with simple bass";
			}
			request.style = "pulse";
			request.settings.durationSeconds = 6.0;
			request.settings.loop = true;
			request.tempo = { 126.0f, 1.0f };
			request.key = { "A", "minor", 1.0f };
			request.targetStems = { "pulse", "bass" };
			return true;
		}
		return false;
	}

	std::string describe(const ofxGgmlMusicRequest & request) {
		if (!hasInput(request)) {
			return "music: empty request";
		}
		return getTaskName(request.task) + ": " + request.audioPath;
	}

	std::string describe(const ofxGgmlMusicGenerationRequest & request) {
		if (!hasPrompt(request)) {
			return "music generation: empty prompt";
		}
		auto description = "music generation: " + request.prompt +
			" [" + getGenerationBackendName(request.settings.backend) + "]";
		if (hasTempo(request)) {
			description += " @ " + std::to_string(static_cast<int>(request.tempo.bpm)) + " bpm";
		}
		const auto key = formatKey(request.key);
		if (!key.empty()) {
			description += " in " + key;
		}
		return description;
	}

	std::string getGenerationManifestPath(const std::string & outputPath) {
		if (outputPath.empty()) {
			return "";
		}
		return outputPath + ".json";
	}

	std::string getGenerationHistoryPath(const std::string & outputPath) {
		if (outputPath.empty()) {
			return "";
		}
		const std::filesystem::path output(outputPath);
		const auto historyName = "ofxGgmlMusic-history.json";
		if (output.has_parent_path()) {
			return (output.parent_path() / historyName).string();
		}
		return historyName;
	}

	std::string serializeGenerationManifest(
		const ofxGgmlMusicGenerationRequest & request,
		const ofxGgmlMusicGenerationResult & result,
		const std::string & backendName) {
		std::ostringstream json;
		json << "{\n";
		json << "  \"backend\": " << quoteJson(backendName) << ",\n";
		json << "  \"backendFamily\": " << quoteJson(getGenerationBackendName(request.settings.backend)) << ",\n";
		json << "  \"prompt\": " << quoteJson(request.prompt) << ",\n";
		json << "  \"negativePrompt\": " << quoteJson(request.negativePrompt) << ",\n";
		json << "  \"style\": " << quoteJson(request.style) << ",\n";
		json << "  \"referenceAudioPath\": " << quoteJson(request.referenceAudioPath) << ",\n";
		json << "  \"outputPath\": " << quoteJson(result.outputPath) << ",\n";
		json << "  \"manifestPath\": " << quoteJson(result.manifestPath) << ",\n";
		json << "  \"historyPath\": " << quoteJson(result.historyPath) << ",\n";
		json << "  \"midiPath\": " << quoteJson(result.midiPath) << ",\n";
		json << "  \"chordMidiPath\": " << quoteJson(result.chordMidiPath) << ",\n";
		json << "  \"seed\": " << result.seed << ",\n";
		json << "  \"durationSeconds\": " << result.durationSeconds << ",\n";
		json << "  \"sampleRate\": " << result.sampleRate << ",\n";
		json << "  \"channels\": " << result.channels << ",\n";
		json << "  \"peakAbs\": " << result.peakAbs << ",\n";
		json << "  \"tempo\": {\n";
		json << "    \"bpm\": " << result.tempo.bpm << ",\n";
		json << "    \"confidence\": " << result.tempo.confidence << "\n";
		json << "  },\n";
		json << "  \"key\": {\n";
		json << "    \"tonic\": " << quoteJson(result.key.tonic) << ",\n";
		json << "    \"mode\": " << quoteJson(result.key.mode) << ",\n";
		json << "    \"confidence\": " << result.key.confidence << "\n";
		json << "  },\n";
		json << "  \"beats\": [\n";
		for (std::size_t i = 0; i < result.beats.size(); ++i) {
			const auto & beat = result.beats[i];
			json << "    { \"timeSeconds\": " << beat.timeSeconds <<
				", \"confidence\": " << beat.confidence <<
				", \"downbeat\": " << (beat.downbeat ? "true" : "false") << " }";
			json << (i + 1 < result.beats.size() ? "," : "") << "\n";
		}
		json << "  ],\n";
		json << "  \"chords\": [\n";
		for (std::size_t i = 0; i < result.chords.size(); ++i) {
			const auto & chord = result.chords[i];
			json << "    { \"timeSeconds\": " << chord.timeSeconds <<
				", \"label\": " << quoteJson(chord.label) <<
				", \"confidence\": " << chord.confidence << " }";
			json << (i + 1 < result.chords.size() ? "," : "") << "\n";
		}
		json << "  ],\n";
		json << "  \"loop\": " << (request.settings.loop ? "true" : "false") << ",\n";
		json << "  \"targetStems\": [";
		for (std::size_t i = 0; i < request.targetStems.size(); ++i) {
			if (i > 0) {
				json << ", ";
			}
			json << quoteJson(request.targetStems[i]);
		}
		json << "],\n";
		json << "  \"stems\": [\n";
		for (std::size_t i = 0; i < result.stems.size(); ++i) {
			const auto & stem = result.stems[i];
			json << "    { \"name\": " << quoteJson(stem.name) <<
				", \"path\": " << quoteJson(stem.path) <<
				", \"gain\": " << stem.gain << " }";
			json << (i + 1 < result.stems.size() ? "," : "") << "\n";
		}
		json << "  ],\n";
		json << "  \"references\": [";
		for (std::size_t i = 0; i < result.references.size(); ++i) {
			if (i > 0) {
				json << ", ";
			}
			json << quoteJson(result.references[i]);
		}
		json << "]\n";
		json << "}\n";
		return json.str();
	}

	bool writeGenerationManifest(
		const ofxGgmlMusicGenerationRequest & request,
		const ofxGgmlMusicGenerationResult & result,
		const std::string & backendName,
		std::string & error) {
		error.clear();
		if (result.manifestPath.empty()) {
			error = "generation manifest path is empty";
			return false;
		}

		std::filesystem::path manifestPath(result.manifestPath);
		if (manifestPath.has_parent_path()) {
			std::error_code code;
			std::filesystem::create_directories(manifestPath.parent_path(), code);
			if (code) {
				error = "could not create generation manifest directory";
				return false;
			}
		}

		std::ofstream output(result.manifestPath, std::ios::out | std::ios::trunc);
		if (!output) {
			error = "could not open generation manifest path";
			return false;
		}
		output << serializeGenerationManifest(request, result, backendName);
		return true;
	}

	bool loadGenerationManifest(
		const std::string & path,
		ofxGgmlMusicGenerationResult & result,
		std::string & error) {
		result = {};
		error.clear();
		const auto text = readTextFile(path, error);
		if (!error.empty()) {
			return false;
		}
		if (text.empty()) {
			error = "generation manifest is empty";
			return false;
		}

		result.manifestPath = path;
		result.outputPath = extractString(text, "outputPath");
		const auto manifestPath = extractString(text, "manifestPath");
		if (!manifestPath.empty()) {
			result.manifestPath = manifestPath;
		}
		result.historyPath = extractString(text, "historyPath");
		result.midiPath = extractString(text, "midiPath");
		result.chordMidiPath = extractString(text, "chordMidiPath");
		result.seed = extractInt(text, "seed", -1);
		result.durationSeconds = extractDouble(text, "durationSeconds");
		result.sampleRate = extractInt(text, "sampleRate");
		result.channels = extractInt(text, "channels");
		result.peakAbs = static_cast<float>(extractDouble(text, "peakAbs"));

		const auto tempoText = extractObject(text, "tempo");
		result.tempo.bpm = static_cast<float>(extractDouble(tempoText, "bpm"));
		result.tempo.confidence = static_cast<float>(extractDouble(tempoText, "confidence"));

		const auto keyText = extractObject(text, "key");
		result.key.tonic = extractString(keyText, "tonic");
		result.key.mode = extractString(keyText, "mode");
		result.key.confidence = static_cast<float>(extractDouble(keyText, "confidence"));

		for (const auto & beatText : splitArrayObjects(extractArray(text, "beats"))) {
			ofxGgmlMusicBeat beat;
			beat.timeSeconds = extractDouble(beatText, "timeSeconds");
			beat.confidence = static_cast<float>(extractDouble(beatText, "confidence"));
			beat.downbeat = extractBool(beatText, "downbeat");
			result.beats.push_back(beat);
		}

		for (const auto & chordText : splitArrayObjects(extractArray(text, "chords"))) {
			ofxGgmlMusicChord chord;
			chord.timeSeconds = extractDouble(chordText, "timeSeconds");
			chord.label = extractString(chordText, "label");
			chord.confidence = static_cast<float>(extractDouble(chordText, "confidence"));
			result.chords.push_back(chord);
		}

		for (const auto & stemText : splitArrayObjects(extractArray(text, "stems"))) {
			ofxGgmlMusicStem stem;
			stem.name = extractString(stemText, "name");
			stem.path = extractString(stemText, "path");
			stem.gain = static_cast<float>(extractDouble(stemText, "gain", 1.0));
			result.stems.push_back(stem);
		}

		result.references = parseStringArray(extractArray(text, "references"));
		result.success = !result.outputPath.empty();
		if (!result.success) {
			error = "generation manifest did not contain an outputPath";
			return false;
		}
		return true;
	}

	bool writeGenerationHistory(
		const std::string & historyPath,
		const std::vector<std::string> & manifestPaths,
		std::string & error) {
		error.clear();
		if (historyPath.empty()) {
			error = "generation history path is empty";
			return false;
		}

		std::filesystem::path path(historyPath);
		if (path.has_parent_path()) {
			std::error_code code;
			std::filesystem::create_directories(path.parent_path(), code);
			if (code) {
				error = "could not create generation history directory";
				return false;
			}
		}

		std::ofstream output(historyPath, std::ios::out | std::ios::trunc);
		if (!output) {
			error = "could not open generation history path";
			return false;
		}

		output << "{\n";
		output << "  \"manifests\": [\n";
		for (std::size_t i = 0; i < manifestPaths.size(); ++i) {
			output << "    " << quoteJson(manifestPaths[i]);
			output << (i + 1 < manifestPaths.size() ? "," : "") << "\n";
		}
		output << "  ]\n";
		output << "}\n";
		return true;
	}

	bool loadGenerationHistory(
		const std::string & historyPath,
		std::vector<std::string> & manifestPaths,
		std::string & error) {
		manifestPaths.clear();
		error.clear();

		std::ifstream input(historyPath, std::ios::in);
		if (!input) {
			error = "could not open generation history";
			return false;
		}

		std::ostringstream text;
		text << input.rdbuf();
		const auto body = text.str();
		if (findKeyValueStart(body, "manifests") == std::string::npos) {
			error = "generation history did not contain manifests";
			return false;
		}
		const auto arrayText = extractArray(body, "manifests");
		manifestPaths = parseStringArray(arrayText);
		return true;
	}

	bool appendGenerationHistory(
		const std::string & historyPath,
		const std::string & manifestPath,
		std::string & error) {
		error.clear();
		if (manifestPath.empty()) {
			error = "generation manifest path is empty";
			return false;
		}

		std::vector<std::string> manifestPaths;
		if (std::filesystem::exists(historyPath) &&
			!loadGenerationHistory(historyPath, manifestPaths, error)) {
			return false;
		}

		manifestPaths.erase(
			std::remove(manifestPaths.begin(), manifestPaths.end(), manifestPath),
			manifestPaths.end());
		manifestPaths.insert(manifestPaths.begin(), manifestPath);
		if (manifestPaths.size() > maxGenerationHistoryEntries) {
			manifestPaths.resize(maxGenerationHistoryEntries);
		}
		return writeGenerationHistory(historyPath, manifestPaths, error);
	}
}
