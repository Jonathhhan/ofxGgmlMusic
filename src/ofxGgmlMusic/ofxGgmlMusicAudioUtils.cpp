#include "ofxGgmlMusicAudioUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace {
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

	std::uint16_t readLe16(std::istream & input) {
		const auto a = static_cast<std::uint8_t>(input.get());
		const auto b = static_cast<std::uint8_t>(input.get());
		return static_cast<std::uint16_t>(a | (b << 8));
	}

	std::uint32_t readLe32(std::istream & input) {
		const auto a = static_cast<std::uint32_t>(static_cast<std::uint8_t>(input.get()));
		const auto b = static_cast<std::uint32_t>(static_cast<std::uint8_t>(input.get()));
		const auto c = static_cast<std::uint32_t>(static_cast<std::uint8_t>(input.get()));
		const auto d = static_cast<std::uint32_t>(static_cast<std::uint8_t>(input.get()));
		return a | (b << 8) | (c << 16) | (d << 24);
	}

	bool readFourCc(std::istream & input, char (&id)[4]) {
		input.read(id, 4);
		return static_cast<bool>(input);
	}

	bool fourCcEquals(const char (&id)[4], const char * text) {
		return id[0] == text[0] && id[1] == text[1] && id[2] == text[2] && id[3] == text[3];
	}
}

double ofxGgmlMusicAudioBuffer::getDurationSeconds() const {
	if (sampleRate <= 0 || channels <= 0 || samples.empty()) {
		return 0.0;
	}
	return static_cast<double>(samples.size()) / static_cast<double>(sampleRate * channels);
}

float ofxGgmlMusicAudioBuffer::getPeakAbs() const {
	float peak = 0.0f;
	for (const auto sample : samples) {
		peak = std::max(peak, std::abs(sample));
	}
	return peak;
}

bool ofxGgmlMusicAudioBuffer::isAllocated() const {
	return sampleRate > 0 && channels > 0 && !samples.empty();
}

namespace ofxGgmlMusicAudioUtils {
	bool writeMonoWav16(
		const std::string & path,
		const std::vector<float> & samples,
		int sampleRate,
		std::string & error) {
		error.clear();
		if (path.empty()) {
			error = "wav output path is empty";
			return false;
		}
		if (samples.empty()) {
			error = "wav sample buffer is empty";
			return false;
		}
		if (sampleRate <= 0) {
			error = "wav sample rate must be positive";
			return false;
		}

		std::filesystem::path outputPath(path);
		if (outputPath.has_parent_path()) {
			std::error_code code;
			std::filesystem::create_directories(outputPath.parent_path(), code);
			if (code) {
				error = "could not create wav output directory";
				return false;
			}
		}

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output) {
			error = "could not open wav output path";
			return false;
		}

		const std::uint16_t channels = 1;
		const std::uint16_t bitsPerSample = 16;
		const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
		const std::uint32_t byteRate = static_cast<std::uint32_t>(sampleRate) * channels * bitsPerSample / 8;
		const std::uint16_t blockAlign = channels * bitsPerSample / 8;

		output.write("RIFF", 4);
		writeLe32(output, 36 + dataBytes);
		output.write("WAVE", 4);
		output.write("fmt ", 4);
		writeLe32(output, 16);
		writeLe16(output, 1);
		writeLe16(output, channels);
		writeLe32(output, static_cast<std::uint32_t>(sampleRate));
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

	bool loadWav16(
		const std::string & path,
		ofxGgmlMusicAudioBuffer & buffer,
		std::string & error) {
		buffer = {};
		error.clear();
		std::ifstream input(path, std::ios::binary);
		if (!input) {
			error = "could not open wav input path";
			return false;
		}

		char riff[4]{};
		char wave[4]{};
		if (!readFourCc(input, riff) || !fourCcEquals(riff, "RIFF")) {
			error = "wav file is missing RIFF header";
			return false;
		}
		readLe32(input);
		if (!readFourCc(input, wave) || !fourCcEquals(wave, "WAVE")) {
			error = "wav file is missing WAVE header";
			return false;
		}

		bool foundFormat = false;
		bool foundData = false;
		std::uint16_t audioFormat = 0;
		std::uint16_t channels = 0;
		std::uint32_t sampleRate = 0;
		std::uint16_t bitsPerSample = 0;
		std::vector<float> samples;

		while (input) {
			char chunkId[4]{};
			if (!readFourCc(input, chunkId)) {
				break;
			}
			const auto chunkSize = readLe32(input);
			const auto chunkStart = input.tellg();
			if (fourCcEquals(chunkId, "fmt ")) {
				audioFormat = readLe16(input);
				channels = readLe16(input);
				sampleRate = readLe32(input);
				readLe32(input);
				readLe16(input);
				bitsPerSample = readLe16(input);
				foundFormat = true;
			} else if (fourCcEquals(chunkId, "data")) {
				if (!foundFormat) {
					error = "wav data appeared before format chunk";
					return false;
				}
				if (audioFormat != 1 || bitsPerSample != 16 || channels == 0 || sampleRate == 0) {
					error = "only PCM16 wav files are supported";
					return false;
				}
				const auto sampleCount = chunkSize / sizeof(std::int16_t);
				samples.reserve(sampleCount);
				for (std::uint32_t i = 0; i < sampleCount; ++i) {
					const auto encoded = readLe16(input);
					const auto raw = encoded > 32767
						? static_cast<int>(encoded) - 65536
						: static_cast<int>(encoded);
					samples.push_back(static_cast<float>(raw) / 32768.0f);
				}
				foundData = true;
			}

			input.clear();
			input.seekg(chunkStart + static_cast<std::streamoff>(chunkSize));
			if (chunkSize % 2 != 0) {
				input.seekg(1, std::ios::cur);
			}
		}

		if (!foundFormat) {
			error = "wav format chunk was not found";
			return false;
		}
		if (!foundData || samples.empty()) {
			error = "wav data chunk was not found";
			return false;
		}

		buffer.sampleRate = static_cast<int>(sampleRate);
		buffer.channels = static_cast<int>(channels);
		buffer.samples = std::move(samples);
		return true;
	}
}
