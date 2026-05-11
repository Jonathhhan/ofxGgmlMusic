#include "ofxGgmlMusicProceduralGenerationBackend.h"

#include "ofxGgmlMusicUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <vector>

namespace {
	constexpr double pi = 3.14159265358979323846;
	constexpr int sampleRate = 44100;

	void writeLe16(std::ostream & output, std::uint16_t value) {
		output.put(static_cast<char>(value & 0xff));
		output.put(static_cast<char>((value >> 8) & 0xff));
	}

	void writeLe32(std::ostream & output, std::uint32_t value) {
		output.put(static_cast<char>(value & 0xff));
		output.put(static_cast<char>((value >> 8) & 0xff));
		output.put(static_cast<char>((value >> 16) & 0xff));
		output.put(static_cast<char>((value >> 24) & 0xff));
	}

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

	bool writeWav(const std::string & path, const std::vector<float> & samples) {
		if (path.empty() || samples.empty()) {
			return false;
		}
		std::filesystem::path outputPath(path);
		if (outputPath.has_parent_path()) {
			std::error_code code;
			std::filesystem::create_directories(outputPath.parent_path(), code);
			if (code) {
				return false;
			}
		}

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output) {
			return false;
		}

		const std::uint16_t channels = 1;
		const std::uint16_t bitsPerSample = 16;
		const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
		const std::uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
		const std::uint16_t blockAlign = channels * bitsPerSample / 8;

		output.write("RIFF", 4);
		writeLe32(output, 36 + dataBytes);
		output.write("WAVE", 4);
		output.write("fmt ", 4);
		writeLe32(output, 16);
		writeLe16(output, 1);
		writeLe16(output, channels);
		writeLe32(output, sampleRate);
		writeLe32(output, byteRate);
		writeLe16(output, blockAlign);
		writeLe16(output, bitsPerSample);
		output.write("data", 4);
		writeLe32(output, dataBytes);

		for (const auto sample : samples) {
			const auto clamped = std::max(-1.0f, std::min(1.0f, sample));
			const auto value = static_cast<std::int16_t>(clamped * static_cast<float>(std::numeric_limits<std::int16_t>::max()));
			writeLe16(output, static_cast<std::uint16_t>(value));
		}
		return true;
	}

	std::vector<float> renderSketch(const ofxGgmlMusicGenerationRequest & request, int seed) {
		const double duration = std::max(0.5, std::min(120.0, request.settings.durationSeconds));
		const float bpm = request.tempo.bpm > 0.0f ? request.tempo.bpm : 96.0f;
		const double beatSeconds = 60.0 / static_cast<double>(bpm);
		const double noteSeconds = beatSeconds * 0.5;
		const auto scale = makeScale(request.key);
		const auto sampleCount = static_cast<std::size_t>(duration * sampleRate);
		std::vector<float> samples(sampleCount, 0.0f);
		const int stride = 1 + (seed % 5);
		const int offset = (seed / 7) % static_cast<int>(scale.size());
		const double warmth = request.style.find("ambient") != std::string::npos ? 0.35 : 0.22;

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
			samples[i] = (melody + overtone + bass * warmth + pulse) * fade;
		}
		return samples;
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

	const auto samples = renderSketch(request, result.seed);
	if (!writeWav(request.outputPath, samples)) {
		result.error = "could not write wav output: " + request.outputPath;
		return result;
	}

	result.durationSeconds = static_cast<double>(samples.size()) / sampleRate;
	result.success = true;
	return result;
}

void ofxGgmlMusicProceduralGenerationBackend::close() {
	loaded = false;
}

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeProceduralMusicGenerationBackend() {
	return std::make_unique<ofxGgmlMusicProceduralGenerationBackend>();
}
