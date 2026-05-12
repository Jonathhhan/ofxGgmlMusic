#include "ofxGgmlMusicProceduralGenerationBackend.h"

#include "ofxGgmlMusicAudioUtils.h"
#include "ofxGgmlMusicMidiUtils.h"
#include "ofxGgmlMusicUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace {
	constexpr double pi = 3.14159265358979323846;
	constexpr int sampleRate = 44100;

	struct RenderedSketch {
		std::vector<float> mix;
		std::vector<float> melody;
		std::vector<float> bass;
		std::vector<float> pulse;
		std::vector<ofxGgmlMusicMidiNote> melodyNotes;
		std::vector<ofxGgmlMusicMidiNote> chordNotes;
	};

	int tonicToMidi(const ofxGgmlMusicKey & key) {
		const std::string tonic = key.tonic.empty() ? "C" : key.tonic;
		if (tonic == "C#" || tonic == "Db") return 61;
		if (tonic == "D") return 62;
		if (tonic == "D#" || tonic == "Eb") return 63;
		if (tonic == "E") return 64;
		if (tonic == "F") return 65;
		if (tonic == "F#" || tonic == "Gb") return 66;
		if (tonic == "G") return 67;
		if (tonic == "G#" || tonic == "Ab") return 68;
		if (tonic == "A") return 69;
		if (tonic == "A#" || tonic == "Bb") return 70;
		if (tonic == "B") return 71;
		return 60;
	}

	std::vector<int> makeScale(const ofxGgmlMusicKey & key) {
		const bool minor = key.mode == "minor" || key.mode == "aeolian";
		const std::vector<int> intervals = minor
			? std::vector<int>{ 0, 2, 3, 5, 7, 8, 10, 12 }
			: std::vector<int>{ 0, 2, 4, 5, 7, 9, 11, 12 };
		std::vector<int> scale;
		const int root = tonicToMidi(key);
		for (const auto interval : intervals) {
			scale.push_back(root + interval);
		}
		return scale;
	}

	double midiToHz(int midi) {
		return 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
	}

	std::string midiToName(int midi) {
		static const std::vector<std::string> names = {
			"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
		};
		return names[static_cast<std::size_t>((midi % 12 + 12) % 12)];
	}

	float envelope(double phase) {
		const double attack = 0.08;
		const double release = 0.22;
		if (phase < attack) {
			return static_cast<float>(phase / attack);
		}
		if (phase > 1.0 - release) {
			return static_cast<float>((1.0 - phase) / release);
		}
		return 1.0f;
	}

	int resolveSeed(const ofxGgmlMusicGenerationRequest & request) {
		if (request.settings.seed >= 0) {
			return request.settings.seed;
		}
		const auto text = request.prompt + "|" + request.style + "|" + request.negativePrompt;
		return static_cast<int>(std::hash<std::string>{}(text) & 0x7fffffff);
	}

	std::vector<ofxGgmlMusicBeat> makeBeatGrid(const ofxGgmlMusicGenerationRequest & request, double duration) {
		const float bpm = request.tempo.bpm > 0.0f ? request.tempo.bpm : 96.0f;
		const double beatSeconds = 60.0 / static_cast<double>(bpm);
		std::vector<ofxGgmlMusicBeat> beats;
		for (int i = 0; ; ++i) {
			const double time = static_cast<double>(i) * beatSeconds;
			if (time >= duration) {
				break;
			}
			beats.push_back({ time, i % 4 == 0 ? 1.0f : 0.78f, i % 4 == 0 });
		}
		return beats;
	}

	std::vector<ofxGgmlMusicChord> makeChordProgression(const ofxGgmlMusicGenerationRequest & request, double duration) {
		const bool minor = request.key.mode == "minor" || request.key.mode == "aeolian";
		const std::vector<int> intervals = minor
			? std::vector<int>{ 0, 8, 3, 10 }
			: std::vector<int>{ 0, 9, 5, 7 };
		const std::vector<std::string> qualities = minor
			? std::vector<std::string>{ "m", "", "", "" }
			: std::vector<std::string>{ "", "m", "", "" };
		const float bpm = request.tempo.bpm > 0.0f ? request.tempo.bpm : 96.0f;
		const double barSeconds = 4.0 * 60.0 / static_cast<double>(bpm);
		const int root = tonicToMidi(request.key);
		std::vector<ofxGgmlMusicChord> chords;
		for (int bar = 0; ; ++bar) {
			const double time = static_cast<double>(bar) * barSeconds;
			if (time >= duration) {
				break;
			}
			const auto index = static_cast<std::size_t>(bar % static_cast<int>(intervals.size()));
			chords.push_back({
				time,
				midiToName(root + intervals[index]) + qualities[index],
				0.84f
			});
		}
		return chords;
	}

	std::vector<ofxGgmlMusicSection> makeSections(
		const ofxGgmlMusicGenerationRequest & request,
		double duration) {
		std::vector<ofxGgmlMusicSection> sections;
		if (duration <= 0.0) {
			return sections;
		}
		if (duration < 6.0) {
			sections.push_back({ request.settings.loop ? "loop" : "sketch", 0.0, duration, 0.9f });
			return sections;
		}
		if (request.settings.loop) {
			const auto half = duration * 0.5;
			sections.push_back({ "loop-a", 0.0, half, 0.88f });
			sections.push_back({ "loop-b", half, duration - half, 0.88f });
			return sections;
		}

		const auto intro = std::min(2.0, duration * 0.25);
		const auto outro = std::min(2.0, duration * 0.20);
		const auto body = std::max(0.1, duration - intro - outro);
		sections.push_back({ "intro", 0.0, intro, 0.85f });
		sections.push_back({ "body", intro, body, 0.88f });
		sections.push_back({ "outro", intro + body, outro, 0.85f });
		return sections;
	}

	std::string normalizeStemName(const std::string & name) {
		std::string normalized;
		for (const auto c : name) {
			if (std::isalnum(static_cast<unsigned char>(c))) {
				normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			} else if (c == '-' || c == '_') {
				normalized.push_back(c);
			}
		}
		return normalized.empty() ? "stem" : normalized;
	}

	std::string makeStemOutputPath(const std::string & outputPath, const std::string & stemName) {
		const std::filesystem::path output(outputPath);
		const auto fileName = output.stem().string() + "-" + normalizeStemName(stemName) + ".wav";
		if (output.has_parent_path()) {
			return (output.parent_path() / fileName).string();
		}
		return fileName;
	}

	std::string makeMidiOutputPath(const std::string & outputPath, const std::string & suffix) {
		if (outputPath.empty()) {
			return "";
		}
		const std::filesystem::path output(outputPath);
		const auto fileName = output.stem().string() + "-" + suffix + ".mid";
		if (output.has_parent_path()) {
			return (output.parent_path() / fileName).string();
		}
		return fileName;
	}

	void appendChordNotes(
		std::vector<ofxGgmlMusicMidiNote> & notes,
		double startSeconds,
		double durationSeconds,
		int root,
		bool minor) {
		const std::vector<int> intervals = minor
			? std::vector<int>{ 0, 3, 7 }
			: std::vector<int>{ 0, 4, 7 };
		for (const auto interval : intervals) {
			notes.push_back({
				startSeconds,
				durationSeconds,
				root + interval,
				70
			});
		}
	}

	const std::vector<float> * getStemSamples(const RenderedSketch & rendered, const std::string & stemName) {
		const auto normalized = normalizeStemName(stemName);
		if (normalized == "melody" || normalized == "lead" || normalized == "piano") {
			return &rendered.melody;
		}
		if (normalized == "bass") {
			return &rendered.bass;
		}
		if (normalized == "pulse" || normalized == "drums" || normalized == "percussion" || normalized == "texture") {
			return &rendered.pulse;
		}
		if (normalized == "mix") {
			return &rendered.mix;
		}
		return nullptr;
	}

	RenderedSketch renderSketch(const ofxGgmlMusicGenerationRequest & request, int seed) {
		const double duration = std::max(0.5, std::min(120.0, request.settings.durationSeconds));
		const float bpm = request.tempo.bpm > 0.0f ? request.tempo.bpm : 96.0f;
		const double beatSeconds = 60.0 / static_cast<double>(bpm);
		const double noteSeconds = beatSeconds * 0.5;
		const auto scale = makeScale(request.key);
		const auto sampleCount = static_cast<std::size_t>(duration * sampleRate);
		RenderedSketch rendered;
		rendered.mix.resize(sampleCount, 0.0f);
		rendered.melody.resize(sampleCount, 0.0f);
		rendered.bass.resize(sampleCount, 0.0f);
		rendered.pulse.resize(sampleCount, 0.0f);
		const int stride = 1 + (seed % 5);
		const int offset = (seed / 7) % static_cast<int>(scale.size());
		const double warmth = request.style.find("ambient") != std::string::npos ? 0.35 : 0.22;
		const int noteCount = static_cast<int>(std::ceil(duration / noteSeconds));
		for (int step = 0; step < noteCount; ++step) {
			const int melodyIndex = (offset + step * stride + (step / 4)) % static_cast<int>(scale.size());
			rendered.melodyNotes.push_back({
				static_cast<double>(step) * noteSeconds,
				noteSeconds * 0.86,
				scale[melodyIndex],
				86
			});
		}
		const bool minor = request.key.mode == "minor" || request.key.mode == "aeolian";
		const std::vector<int> chordIntervals = minor
			? std::vector<int>{ 0, 8, 3, 10 }
			: std::vector<int>{ 0, 9, 5, 7 };
		const double barSeconds = beatSeconds * 4.0;
		const int root = tonicToMidi(request.key) - 12;
		for (int bar = 0; ; ++bar) {
			const double start = static_cast<double>(bar) * barSeconds;
			if (start >= duration) {
				break;
			}
			const auto interval = chordIntervals[static_cast<std::size_t>(bar % static_cast<int>(chordIntervals.size()))];
			const auto remaining = std::max(0.1, duration - start);
			appendChordNotes(
				rendered.chordNotes,
				start,
				std::min(barSeconds * 0.92, remaining),
				root + interval,
				minor);
		}

		for (std::size_t i = 0; i < sampleCount; ++i) {
			const double time = static_cast<double>(i) / sampleRate;
			const int step = static_cast<int>(time / noteSeconds);
			const double phaseInNote = (time - static_cast<double>(step) * noteSeconds) / noteSeconds;
			const int melodyIndex = (offset + step * stride + (step / 4)) % static_cast<int>(scale.size());
			const int bassIndex = (step / 8) % 4;
			const double melodyHz = midiToHz(scale[melodyIndex]);
			const double bassHz = midiToHz(scale[0] - 24 + bassIndex * 2);
			const float env = envelope(phaseInNote);
			const float melody = std::sin(2.0 * pi * melodyHz * time) * env * 0.34f;
			const float overtone = std::sin(2.0 * pi * melodyHz * 2.0 * time) * env * 0.08f;
			const float bass = std::sin(2.0 * pi * bassHz * time) * 0.24f;
			const float pulse = (std::sin(2.0 * pi * (1.0 / beatSeconds) * time) > 0.94 ? 0.10f : 0.0f);
			float fade = 1.0f;
			if (!request.settings.loop) {
				fade = static_cast<float>(std::min(1.0, std::min(time / 0.05, (duration - time) / 0.18)));
			}
			rendered.melody[i] = (melody + overtone) * fade;
			rendered.bass[i] = bass * static_cast<float>(warmth) * fade;
			rendered.pulse[i] = pulse * fade;
			rendered.mix[i] = rendered.melody[i] + rendered.bass[i] + rendered.pulse[i];
		}
		return rendered;
	}

	bool writeRequestedStems(
		const ofxGgmlMusicGenerationRequest & request,
		const RenderedSketch & rendered,
		ofxGgmlMusicGenerationResult & result,
		std::string & error) {
		error.clear();
		for (const auto & stemName : request.targetStems) {
			const auto samples = getStemSamples(rendered, stemName);
			if (samples == nullptr) {
				continue;
			}
			const auto stemPath = makeStemOutputPath(request.outputPath, stemName);
			if (!ofxGgmlMusicAudioUtils::writeMonoWav16(stemPath, *samples, sampleRate, error)) {
				return false;
			}
			result.stems.push_back({ normalizeStemName(stemName), stemPath, 1.0f });
		}
		return true;
	}
}

std::string ofxGgmlMusicProceduralGenerationBackend::getBackendName() const {
	return "procedural-sketch";
}

ofxGgmlMusicGenerationBackendFamily ofxGgmlMusicProceduralGenerationBackend::getBackendFamily() const {
	return ofxGgmlMusicGenerationBackendFamily::External;
}

bool ofxGgmlMusicProceduralGenerationBackend::isAvailable() const {
	return true;
}

bool ofxGgmlMusicProceduralGenerationBackend::isLoaded() const {
	return loaded;
}

ofxGgmlMusicGenerationResult ofxGgmlMusicProceduralGenerationBackend::setup(
	const ofxGgmlMusicGenerationRequest & request) {
	ofxGgmlMusicGenerationResult result;
	result.durationSeconds = request.settings.durationSeconds;
	result.seed = resolveSeed(request);
	result.tempo = request.tempo;
	result.key = request.key;
	result.manifestPath = ofxGgmlMusicUtils::getGenerationManifestPath(request.outputPath);
	result.historyPath = ofxGgmlMusicUtils::getGenerationHistoryPath(request.outputPath);
	result.midiPath = makeMidiOutputPath(request.outputPath, "melody");
	result.chordMidiPath = makeMidiOutputPath(request.outputPath, "chords");
	result.success = true;
	loaded = true;
	return result;
}

ofxGgmlMusicGenerationResult ofxGgmlMusicProceduralGenerationBackend::generate(
	const ofxGgmlMusicGenerationRequest & request) {
	ofxGgmlMusicGenerationResult result;
	result.durationSeconds = request.settings.durationSeconds;
	result.seed = resolveSeed(request);
	result.tempo = request.tempo;
	result.key = request.key;
	result.outputPath = request.outputPath;
	result.manifestPath = ofxGgmlMusicUtils::getGenerationManifestPath(request.outputPath);
	result.historyPath = ofxGgmlMusicUtils::getGenerationHistoryPath(request.outputPath);
	result.midiPath = makeMidiOutputPath(request.outputPath, "melody");
	result.chordMidiPath = makeMidiOutputPath(request.outputPath, "chords");
	result.references.push_back("procedural-sketch");

	if (!ofxGgmlMusicUtils::hasPrompt(request)) {
		result.error = "music generation prompt is empty";
		return result;
	}
	if (request.outputPath.empty()) {
		result.error = "music generation outputPath is empty";
		return result;
	}
	if (!loaded) {
		const auto setupResult = setup(request);
		if (!setupResult) {
			result.error = setupResult.error;
			return result;
		}
	}

	const auto rendered = renderSketch(request, result.seed);
	std::string writeError;
	if (!ofxGgmlMusicAudioUtils::writeMonoWav16(request.outputPath, rendered.mix, sampleRate, writeError)) {
		result.error = "could not write wav output: " + writeError;
		return result;
	}
	if (!writeRequestedStems(request, rendered, result, writeError)) {
		result.error = "could not write stem output: " + writeError;
		return result;
	}
	if (!ofxGgmlMusicMidiUtils::writeMidiFile(result.midiPath, rendered.melodyNotes, request.tempo.bpm, writeError)) {
		result.error = "could not write midi output: " + writeError;
		return result;
	}
	if (!ofxGgmlMusicMidiUtils::writeMidiFile(result.chordMidiPath, rendered.chordNotes, request.tempo.bpm, writeError)) {
		result.error = "could not write chord midi output: " + writeError;
		return result;
	}

	result.durationSeconds = static_cast<double>(rendered.mix.size()) / sampleRate;
	result.sampleRate = sampleRate;
	result.channels = 1;
	ofxGgmlMusicAudioBuffer buffer;
	buffer.sampleRate = sampleRate;
	buffer.channels = 1;
	buffer.samples = rendered.mix;
	result.peakAbs = buffer.getPeakAbs();
	result.beats = makeBeatGrid(request, result.durationSeconds);
	result.chords = makeChordProgression(request, result.durationSeconds);
	result.sections = makeSections(request, result.durationSeconds);
	std::string manifestError;
	if (!ofxGgmlMusicUtils::writeGenerationManifest(request, result, getBackendName(), manifestError)) {
		result.error = "could not write generation manifest: " + manifestError;
		return result;
	}
	if (!ofxGgmlMusicUtils::appendGenerationHistory(result.historyPath, result.manifestPath, manifestError)) {
		result.error = "could not write generation history: " + manifestError;
		return result;
	}
	result.success = true;
	return result;
}

void ofxGgmlMusicProceduralGenerationBackend::close() {
	loaded = false;
}

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeProceduralMusicGenerationBackend() {
	return std::make_unique<ofxGgmlMusicProceduralGenerationBackend>();
}
