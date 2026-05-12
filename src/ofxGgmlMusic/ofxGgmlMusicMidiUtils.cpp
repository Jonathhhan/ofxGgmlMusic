#include "ofxGgmlMusicMidiUtils.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {
	constexpr std::uint16_t ticksPerQuarter = 480;

	void writeU16(std::ostream & output, std::uint16_t value) {
		output.put(static_cast<char>((value >> 8) & 0xff));
		output.put(static_cast<char>(value & 0xff));
	}

	void writeU32(std::ostream & output, std::uint32_t value) {
		output.put(static_cast<char>((value >> 24) & 0xff));
		output.put(static_cast<char>((value >> 16) & 0xff));
		output.put(static_cast<char>((value >> 8) & 0xff));
		output.put(static_cast<char>(value & 0xff));
	}

	void appendVarLen(std::vector<std::uint8_t> & bytes, std::uint32_t value) {
		std::uint8_t buffer[5]{};
		int count = 0;
		buffer[count++] = static_cast<std::uint8_t>(value & 0x7f);
		while ((value >>= 7) > 0) {
			buffer[count++] = static_cast<std::uint8_t>((value & 0x7f) | 0x80);
		}
		while (count > 0) {
			bytes.push_back(buffer[--count]);
		}
	}

	void appendEvent(
		std::vector<std::uint8_t> & bytes,
		std::uint32_t delta,
		std::uint8_t status,
		std::uint8_t a,
		std::uint8_t b) {
		appendVarLen(bytes, delta);
		bytes.push_back(status);
		bytes.push_back(a);
		bytes.push_back(b);
	}

	std::uint32_t secondsToTicks(double seconds, float bpm) {
		const auto safeBpm = bpm > 0.0f ? bpm : 120.0f;
		const auto quarters = seconds * static_cast<double>(safeBpm) / 60.0;
		return static_cast<std::uint32_t>(std::max(0.0, quarters * ticksPerQuarter + 0.5));
	}
}

namespace ofxGgmlMusicMidiUtils {
	bool writeMidiFile(
		const std::string & path,
		const std::vector<ofxGgmlMusicMidiNote> & notes,
		float bpm,
		std::string & error) {
		error.clear();
		if (path.empty()) {
			error = "midi output path is empty";
			return false;
		}
		if (notes.empty()) {
			error = "midi note list is empty";
			return false;
		}

		std::filesystem::path outputPath(path);
		if (outputPath.has_parent_path()) {
			std::error_code code;
			std::filesystem::create_directories(outputPath.parent_path(), code);
			if (code) {
				error = "could not create midi output directory";
				return false;
			}
		}

		struct Event {
			std::uint32_t tick = 0;
			std::uint8_t status = 0;
			std::uint8_t pitch = 0;
			std::uint8_t velocity = 0;
		};
		std::vector<Event> events;
		for (const auto & note : notes) {
			const auto startTick = secondsToTicks(note.startSeconds, bpm);
			const auto endTick = secondsToTicks(note.startSeconds + note.durationSeconds, bpm);
			const auto pitch = static_cast<std::uint8_t>(std::max(0, std::min(127, note.pitch)));
			const auto velocity = static_cast<std::uint8_t>(std::max(1, std::min(127, note.velocity)));
			events.push_back({ startTick, 0x90, pitch, velocity });
			events.push_back({ std::max(startTick + 1, endTick), 0x80, pitch, 0 });
		}
		std::sort(events.begin(), events.end(), [](const Event & a, const Event & b) {
			if (a.tick != b.tick) {
				return a.tick < b.tick;
			}
			return a.status < b.status;
		});

		std::vector<std::uint8_t> track;
		const auto safeBpm = bpm > 0.0f ? bpm : 120.0f;
		const auto microsPerQuarter = static_cast<std::uint32_t>(60000000.0f / safeBpm + 0.5f);
		appendVarLen(track, 0);
		track.insert(track.end(), {
			0xff, 0x51, 0x03,
			static_cast<std::uint8_t>((microsPerQuarter >> 16) & 0xff),
			static_cast<std::uint8_t>((microsPerQuarter >> 8) & 0xff),
			static_cast<std::uint8_t>(microsPerQuarter & 0xff)
		});
		appendVarLen(track, 0);
		track.insert(track.end(), { 0xc0, 0x04 });

		std::uint32_t cursor = 0;
		for (const auto & event : events) {
			appendEvent(track, event.tick - cursor, event.status, event.pitch, event.velocity);
			cursor = event.tick;
		}
		appendVarLen(track, 0);
		track.insert(track.end(), { 0xff, 0x2f, 0x00 });

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output) {
			error = "could not open midi output path";
			return false;
		}
		output.write("MThd", 4);
		writeU32(output, 6);
		writeU16(output, 0);
		writeU16(output, 1);
		writeU16(output, ticksPerQuarter);
		output.write("MTrk", 4);
		writeU32(output, static_cast<std::uint32_t>(track.size()));
		output.write(reinterpret_cast<const char *>(track.data()), static_cast<std::streamsize>(track.size()));
		return true;
	}
}
