#include "ofxGgmlMusicProceduralGenerationBackend.h"

#include "ofxGgmlMusicAudioUtils.h"
#include "ofxGgmlMusicMidiUtils.h"
#include "ofxGgmlMusicUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace {
	constexpr double pi = 3.14159265358979323846;
	constexpr int sampleRate = 44100;

	float sampleFloat(double value) {
		return static_cast<float>(value);
	}

	struct RenderedSketch {
		std::vector<float> mix;
		std::vector<float> melody;
		std::vector<float> bass;
		std::vector<float> pulse;
		std::vector<ofxGgmlMusicMidiNote> melodyNotes;
		std::vector<ofxGgmlMusicMidiNote> chordNotes;
		std::vector<ofxGgmlMusicMidiNote> bassNotes;
		std::vector<ofxGgmlMusicMidiNote> pulseNotes;
	};

	struct RenderProfile {
		float melodyGain = 0.32f;
		float harmonyGain = 0.07f;
		float bassGain = 0.24f;
		float pulseGain = 0.09f;
		float padGain = 0.07f;
		float textureGain = 0.0f;
		double bassWarmth = 0.24;
		double pulseToneHz = 90.0;
		double pulseDecay = 24.0;
		int melodyOctaveOffset = 0;
	};

	struct LayerGains {
		float melody = 1.0f;
		float pad = 1.0f;
		float bass = 1.0f;
		float pulse = 1.0f;
	};

	struct ChordStep {
		int interval = 0;
		bool minor = false;
		const char * quality = "";
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

	double clamp01(double value) {
		return std::max(0.0, std::min(1.0, value));
	}

	double smooth01(double value) {
		const auto x = clamp01(value);
		return x * x * (3.0 - 2.0 * x);
	}

	float barEnvelope(double phase) {
		const double attack = 0.06;
		const double release = 0.16;
		if (phase < attack) {
			return static_cast<float>(phase / attack);
		}
		if (phase > 1.0 - release) {
			return static_cast<float>((1.0 - phase) / release);
		}
		return 1.0f;
	}

	LayerGains makeLayerGains(const ofxGgmlMusicGenerationRequest & request, double duration, double time) {
		LayerGains gains;
		if (duration <= 0.0) {
			return gains;
		}
		if (request.settings.loop) {
			const double transitionSeconds = std::min(0.35, duration * 0.1);
			const double lift = transitionSeconds > 0.0
				? smooth01((time - duration * 0.5) / transitionSeconds)
				: 0.0;
			gains.melody = static_cast<float>(1.0 + 0.06 * lift);
			gains.pad = static_cast<float>(1.0 - 0.05 * lift);
			gains.bass = static_cast<float>(1.0 - 0.02 * lift);
			gains.pulse = static_cast<float>(1.0 + 0.12 * lift);
			return gains;
		}

		const double intro = std::min(2.0, duration * 0.25);
		const double outro = std::min(2.0, duration * 0.20);
		if (intro > 0.0 && time < intro) {
			const auto rise = smooth01(time / intro);
			gains.melody = static_cast<float>(0.72 + 0.28 * rise);
			gains.pad = static_cast<float>(0.82 + 0.18 * rise);
			gains.bass = static_cast<float>(0.35 + 0.65 * rise);
			gains.pulse = static_cast<float>(0.12 + 0.88 * rise);
			return gains;
		}
		if (outro > 0.0 && time > duration - outro) {
			const auto fall = smooth01((duration - time) / outro);
			gains.melody = static_cast<float>(0.68 + 0.32 * fall);
			gains.pad = static_cast<float>(0.86 + 0.14 * fall);
			gains.bass = static_cast<float>(0.42 + 0.58 * fall);
			gains.pulse = static_cast<float>(0.10 + 0.90 * fall);
		}
		return gains;
	}

	std::string lowerText(std::string text) {
		std::transform(
			text.begin(),
			text.end(),
			text.begin(),
			[](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
		return text;
	}

	bool containsAny(const std::string & text, std::initializer_list<const char *> words) {
		for (const auto & word : words) {
			if (text.find(word) != std::string::npos) {
				return true;
			}
		}
		return false;
	}

	RenderProfile makeRenderProfile(const ofxGgmlMusicGenerationRequest & request) {
		const auto text = lowerText(request.prompt + " " + request.style);
		const auto negative = lowerText(request.negativePrompt);
		RenderProfile profile;
		if (containsAny(text, { "ambient", "pad", "drone", "texture", "granular" })) {
			profile.melodyGain = 0.27f;
			profile.harmonyGain = 0.09f;
			profile.bassGain = 0.20f;
			profile.pulseGain = 0.035f;
			profile.padGain = 0.12f;
			profile.textureGain = 0.018f;
			profile.bassWarmth = 0.36;
			profile.pulseDecay = 18.0;
		}
		if (containsAny(text, { "lofi", "lo-fi", "tape", "warm", "keys" })) {
			profile.melodyGain = 0.30f;
			profile.harmonyGain = 0.08f;
			profile.bassGain = 0.28f;
			profile.pulseGain = 0.08f;
			profile.padGain = 0.06f;
			profile.textureGain = 0.020f;
			profile.bassWarmth = 0.30;
			profile.pulseToneHz = 72.0;
		}
		if (containsAny(text, { "pulse", "beat", "drum", "percussion", "rhythm" })) {
			profile.melodyGain = 0.26f;
			profile.harmonyGain = 0.05f;
			profile.bassGain = 0.30f;
			profile.pulseGain = 0.15f;
			profile.padGain = 0.045f;
			profile.pulseToneHz = 104.0;
			profile.pulseDecay = 32.0;
		}
		if (containsAny(text, { "bass", "sub", "low" })) {
			profile.bassGain = std::max(profile.bassGain, 0.34f);
			profile.bassWarmth = std::max(profile.bassWarmth, 0.34);
		}
		if (containsAny(text, { "bright", "lead", "sparkle" })) {
			profile.melodyGain = std::min(profile.melodyGain + 0.04f, 0.36f);
			profile.harmonyGain = std::min(profile.harmonyGain + 0.03f, 0.12f);
			profile.melodyOctaveOffset = 12;
		}
		if (containsAny(negative, { "drum", "drums", "percussion", "beat", "pulse", "rhythm" })) {
			profile.pulseGain *= 0.28f;
			profile.textureGain *= 0.65f;
		}
		if (containsAny(negative, { "bass", "sub", "low" })) {
			profile.bassGain *= 0.55f;
			profile.bassWarmth *= 0.65;
		}
		if (containsAny(negative, { "pad", "drone", "texture", "granular" })) {
			profile.padGain *= 0.45f;
			profile.textureGain *= 0.35f;
		}
		if (containsAny(negative, { "bright", "lead", "sparkle", "melody" })) {
			profile.melodyGain *= 0.75f;
			profile.harmonyGain *= 0.75f;
			profile.melodyOctaveOffset = std::min(profile.melodyOctaveOffset, 0);
		}
		const auto guidance = static_cast<float>(std::max(0.0f, std::min(8.0f, request.settings.guidance)));
		const auto energy = 0.82f + guidance * 0.06f;
		const auto focus = 0.88f + guidance * 0.04f;
		profile.melodyGain *= focus;
		profile.harmonyGain *= focus;
		profile.bassGain *= energy;
		profile.pulseGain *= energy;
		profile.padGain *= 0.94f + guidance * 0.02f;
		profile.textureGain *= 0.85f + guidance * 0.05f;
		return profile;
	}

	std::vector<ChordStep> makeChordPattern(const ofxGgmlMusicGenerationRequest & request) {
		const auto text = lowerText(request.prompt + " " + request.style);
		const bool minorMode = request.key.mode == "minor" || request.key.mode == "aeolian";
		if (minorMode) {
			if (containsAny(text, { "ambient", "pad", "drone", "texture", "granular" })) {
				return { { 0, true, "m" }, { 10, false, "" }, { 8, false, "" }, { 3, false, "" } };
			}
			if (containsAny(text, { "pulse", "beat", "drum", "percussion", "rhythm" })) {
				return { { 0, true, "m" }, { 10, false, "" }, { 0, true, "m" }, { 8, false, "" } };
			}
			return { { 0, true, "m" }, { 8, false, "" }, { 3, false, "" }, { 10, false, "" } };
		}
		if (containsAny(text, { "ambient", "pad", "drone", "texture", "granular" })) {
			return { { 0, false, "" }, { 7, false, "" }, { 9, true, "m" }, { 5, false, "" } };
		}
		if (containsAny(text, { "pulse", "beat", "drum", "percussion", "rhythm" })) {
			return { { 0, false, "" }, { 7, false, "" }, { 9, true, "m" }, { 7, false, "" } };
		}
		return { { 0, false, "" }, { 9, true, "m" }, { 5, false, "" }, { 7, false, "" } };
	}

	const ChordStep & chordForBar(const std::vector<ChordStep> & pattern, int bar) {
		return pattern[static_cast<std::size_t>(bar % static_cast<int>(pattern.size()))];
	}

	int chordToneForStep(
		const std::vector<int> & scale,
		int melodyIndex,
		int step,
		int chordRoot,
		bool chordMinor,
		int octaveOffset) {
		if (step % 8 == 0) {
			return chordRoot + octaveOffset;
		}
		if (step % 4 == 0) {
			return chordRoot + (chordMinor ? 3 : 4) + octaveOffset;
		}
		if (step % 4 == 2) {
			return chordRoot + 7 + octaveOffset;
		}
		return scale[static_cast<std::size_t>(melodyIndex)] + octaveOffset;
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
		const auto pattern = makeChordPattern(request);
		const float bpm = request.tempo.bpm > 0.0f ? request.tempo.bpm : 96.0f;
		const double barSeconds = 4.0 * 60.0 / static_cast<double>(bpm);
		const int root = tonicToMidi(request.key);
		std::vector<ofxGgmlMusicChord> chords;
		for (int bar = 0; ; ++bar) {
			const double time = static_cast<double>(bar) * barSeconds;
			if (time >= duration) {
				break;
			}
			const auto & chord = chordForBar(pattern, bar);
			chords.push_back({
				time,
				midiToName(root + chord.interval) + chord.quality,
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

	void scaleSamples(std::vector<float> & samples, float gain) {
		for (auto & sample : samples) {
			sample *= gain;
		}
	}

	void normalizeRendered(RenderedSketch & rendered) {
		float peak = 0.0f;
		for (const auto sample : rendered.mix) {
			peak = std::max(peak, std::abs(sample));
		}
		if (peak <= 0.0f) {
			return;
		}
		float gain = 1.0f;
		if (peak > 0.92f) {
			gain = 0.92f / peak;
		} else if (peak < 0.32f) {
			gain = 0.32f / peak;
		}
		if (gain == 1.0f) {
			return;
		}
		scaleSamples(rendered.mix, gain);
		scaleSamples(rendered.melody, gain);
		scaleSamples(rendered.bass, gain);
		scaleSamples(rendered.pulse, gain);
	}

	void rebuildMix(RenderedSketch & rendered) {
		const auto sampleCount = std::min({
			rendered.mix.size(),
			rendered.melody.size(),
			rendered.bass.size(),
			rendered.pulse.size()
		});
		for (std::size_t i = 0; i < sampleCount; ++i) {
			rendered.mix[i] = rendered.melody[i] + rendered.bass[i] + rendered.pulse[i];
		}
	}

	void smoothLoopBoundary(std::vector<float> & samples) {
		if (samples.size() < 2) {
			return;
		}
		const auto fadeSamples = std::min<std::size_t>(
			std::max<std::size_t>(32, static_cast<std::size_t>(sampleRate * 0.01)),
			samples.size() / 4);
		if (fadeSamples == 0) {
			return;
		}
		const float correction = samples.back() - samples.front();
		const auto start = samples.size() - fadeSamples;
		for (std::size_t i = 0; i < fadeSamples; ++i) {
			const float amount = static_cast<float>(i + 1) / static_cast<float>(fadeSamples);
			samples[start + i] -= correction * amount;
		}
	}

	void smoothLoopBoundaries(RenderedSketch & rendered) {
		smoothLoopBoundary(rendered.melody);
		smoothLoopBoundary(rendered.bass);
		smoothLoopBoundary(rendered.pulse);
		rebuildMix(rendered);
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
		const auto profile = makeRenderProfile(request);
		const int stride = 1 + (seed % 5);
		const int offset = (seed / 7) % static_cast<int>(scale.size());
		const auto chordPattern = makeChordPattern(request);
		const double barSeconds = beatSeconds * 4.0;
		const int tonic = tonicToMidi(request.key);
		const int chordMidiRoot = tonic - 12;
		const int noteCount = static_cast<int>(std::ceil(duration / noteSeconds));
		for (int step = 0; step < noteCount; ++step) {
			const int barIndex = static_cast<int>((static_cast<double>(step) * noteSeconds) / barSeconds);
			const auto & chord = chordForBar(chordPattern, barIndex);
			const int melodyIndex = (offset + step * stride + (step / 4)) % static_cast<int>(scale.size());
			rendered.melodyNotes.push_back({
				static_cast<double>(step) * noteSeconds,
				noteSeconds * 0.86,
				chordToneForStep(
					scale,
					melodyIndex,
					step,
					tonic + chord.interval,
					chord.minor,
					profile.melodyOctaveOffset),
				86
			});
		}
		for (int bar = 0; ; ++bar) {
			const double start = static_cast<double>(bar) * barSeconds;
			if (start >= duration) {
				break;
			}
			const auto & chord = chordForBar(chordPattern, bar);
			const auto remaining = std::max(0.1, duration - start);
			appendChordNotes(
				rendered.chordNotes,
				start,
				std::min(barSeconds * 0.92, remaining),
				chordMidiRoot + chord.interval,
				chord.minor);
			rendered.bassNotes.push_back({
				start,
				std::min(barSeconds * 0.88, remaining),
				chordMidiRoot + chord.interval - 12,
				82
			});
		}
		for (int beat = 0; ; ++beat) {
			const double start = static_cast<double>(beat) * beatSeconds;
			if (start >= duration) {
				break;
			}
			rendered.pulseNotes.push_back({
				start,
				std::min(beatSeconds * 0.18, std::max(0.02, duration - start)),
				beat % 4 == 0 ? 36 : 38,
				beat % 4 == 0 ? 96 : 72
			});
		}

		for (std::size_t i = 0; i < sampleCount; ++i) {
			const double time = static_cast<double>(i) / sampleRate;
			const int step = static_cast<int>(time / noteSeconds);
			const double phaseInNote = (time - static_cast<double>(step) * noteSeconds) / noteSeconds;
			const int melodyIndex = (offset + step * stride + (step / 4)) % static_cast<int>(scale.size());
			const int barIndex = static_cast<int>(time / barSeconds);
			const auto & chord = chordForBar(chordPattern, barIndex);
			const int chordRoot = tonic + chord.interval;
			const bool chordMinor = chord.minor;
			const int melodyMidi = chordToneForStep(
				scale,
				melodyIndex,
				step,
				chordRoot,
				chordMinor,
				profile.melodyOctaveOffset);
			const double melodyVibrato =
				1.0 + std::sin(2.0 * pi * (5.0 + static_cast<double>(seed % 7) * 0.15) * time) * 0.0025;
			const double melodyHz = midiToHz(melodyMidi) * melodyVibrato;
			const int harmonyIndex = (melodyIndex + 2 + (barIndex % 2)) % static_cast<int>(scale.size());
			const double harmonyHz = midiToHz(scale[harmonyIndex] - 12);
			const double bassHz = midiToHz(chordRoot - 24);
			const float env = envelope(phaseInNote);
			const float melody = sampleFloat(std::sin(2.0 * pi * melodyHz * time) * env * profile.melodyGain);
			const float overtone = sampleFloat(std::sin(2.0 * pi * melodyHz * 2.0 * time) * env * 0.07f);
			const float harmony = sampleFloat(std::sin(2.0 * pi * harmonyHz * time) * env * profile.harmonyGain);
			const float bassFundamental = sampleFloat(std::sin(2.0 * pi * bassHz * time) * profile.bassGain);
			const float bassHarmonic =
				sampleFloat(std::sin(2.0 * pi * bassHz * 2.0 * time) * profile.bassGain * profile.bassWarmth);
			const double barStart = std::floor(time / barSeconds) * barSeconds;
			const double phaseInBar = (time - barStart) / barSeconds;
			float pad = 0.0f;
			const int third = chordMinor ? 3 : 4;
			pad += sampleFloat(std::sin(2.0 * pi * midiToHz(chordRoot - 12) * time) * profile.padGain / 3.0);
			pad += sampleFloat(std::sin(2.0 * pi * midiToHz(chordRoot - 12 + third) * time) * profile.padGain / 3.0);
			pad += sampleFloat(std::sin(2.0 * pi * midiToHz(chordRoot - 5) * time) * profile.padGain / 3.0);
			pad *= barEnvelope(phaseInBar);
			const double beatPosition = time / beatSeconds;
			const double phaseInBeat = beatPosition - std::floor(beatPosition);
			const int beatIndex = static_cast<int>(beatPosition);
			const float pulseAccent = beatIndex % 4 == 0 ? 1.0f : 0.64f;
			const float pulseBody =
				sampleFloat(std::sin(2.0 * pi * profile.pulseToneHz * time) *
					std::exp(-phaseInBeat * profile.pulseDecay) *
					profile.pulseGain *
					pulseAccent);
			const float texture =
				sampleFloat(std::sin(2.0 * pi * 17.3 * time + static_cast<double>(seed % 31)) *
					std::sin(2.0 * pi * 0.23 * time) *
					profile.textureGain);
			const auto layerGains = makeLayerGains(request, duration, time);
			float fade = 1.0f;
			if (!request.settings.loop) {
				fade = static_cast<float>(std::min(1.0, std::min(time / 0.05, (duration - time) / 0.18)));
			}
			rendered.melody[i] =
				((melody + overtone + harmony) * layerGains.melody + pad * layerGains.pad) * fade;
			rendered.bass[i] = (bassFundamental + bassHarmonic) * layerGains.bass * fade;
			rendered.pulse[i] = (pulseBody + texture) * layerGains.pulse * fade;
			rendered.mix[i] = rendered.melody[i] + rendered.bass[i] + rendered.pulse[i];
		}
		if (request.settings.loop) {
			smoothLoopBoundaries(rendered);
		}
		normalizeRendered(rendered);
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
	result.arrangementMidiPath = makeMidiOutputPath(request.outputPath, "arrangement");
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
	result.arrangementMidiPath = makeMidiOutputPath(request.outputPath, "arrangement");
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
	if (!ofxGgmlMusicMidiUtils::writeMidiFile(
			result.arrangementMidiPath,
			{
				{ "melody", 4, rendered.melodyNotes },
				{ "chords", 0, rendered.chordNotes },
				{ "bass", 32, rendered.bassNotes },
				{ "pulse", 118, rendered.pulseNotes }
			},
			request.tempo.bpm,
			writeError)) {
		result.error = "could not write arrangement midi output: " + writeError;
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
