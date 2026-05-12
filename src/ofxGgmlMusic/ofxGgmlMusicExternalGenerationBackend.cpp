#include "ofxGgmlMusicExternalGenerationBackend.h"

#include "ofxGgmlMusicUtils.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace {
	ofxGgmlMusicGenerationResult makeResultBase(const ofxGgmlMusicGenerationRequest & request) {
		ofxGgmlMusicGenerationResult result;
		result.outputPath = request.outputPath;
		result.manifestPath = ofxGgmlMusicUtils::getGenerationManifestPath(request.outputPath);
		result.historyPath = ofxGgmlMusicUtils::getGenerationHistoryPath(request.outputPath);
		result.seed = request.settings.seed;
		result.durationSeconds = request.settings.durationSeconds;
		result.tempo = request.tempo;
		result.key = request.key;
		return result;
	}

	ofxGgmlMusicGenerationResult makeError(
		const ofxGgmlMusicGenerationRequest & request,
		const std::string & error) {
		auto result = makeResultBase(request);
		result.success = false;
		result.error = error;
		return result;
	}

	bool fileExists(const std::string & path) {
		return !path.empty() && std::filesystem::exists(std::filesystem::path(path));
	}

	std::string quoteShellArgument(const std::string & value) {
		std::string quoted = "\"";
		for (const auto c : value) {
			if (c == '"') {
				quoted += "\\\"";
			} else {
				quoted.push_back(c);
			}
		}
		quoted += "\"";
		return quoted;
	}

	void appendFlagValue(
		std::ostringstream & command,
		const std::string & flag,
		const std::string & value) {
		if (!flag.empty() && !value.empty()) {
			command << " " << quoteShellArgument(flag) << " " << quoteShellArgument(value);
		}
	}

	std::string buildCommand(const ofxGgmlMusicGenerationRequest & request) {
		const auto & external = request.external;
		std::ostringstream command;
		if (!external.workingDirectory.empty()) {
#if defined(_WIN32)
			command << "cd /d " << quoteShellArgument(external.workingDirectory) << " && ";
#else
			command << "cd " << quoteShellArgument(external.workingDirectory) << " && ";
#endif
		}
#if defined(_WIN32)
		command << "call ";
#endif
		command << quoteShellArgument(external.executablePath);
		appendFlagValue(command, external.promptFlag, request.prompt);
		appendFlagValue(command, external.outputFlag, request.outputPath);
		appendFlagValue(command, external.modelFlag, external.modelPath);
		if (!external.durationFlag.empty() && request.settings.durationSeconds > 0.0) {
			command << " " << quoteShellArgument(external.durationFlag) << " " << request.settings.durationSeconds;
		}
		if (!external.seedFlag.empty() && request.settings.seed >= 0) {
			command << " " << quoteShellArgument(external.seedFlag) << " " << request.settings.seed;
		}
		for (const auto & argument : external.extraArguments) {
			if (!argument.empty()) {
				command << " " << quoteShellArgument(argument);
			}
		}
		return command.str();
	}
}

std::string ofxGgmlMusicExternalGenerationBackend::getBackendName() const {
	return "external-generator";
}

ofxGgmlMusicGenerationBackendFamily ofxGgmlMusicExternalGenerationBackend::getBackendFamily() const {
	return ofxGgmlMusicGenerationBackendFamily::External;
}

bool ofxGgmlMusicExternalGenerationBackend::isAvailable() const {
	return true;
}

bool ofxGgmlMusicExternalGenerationBackend::isLoaded() const {
	return loaded;
}

ofxGgmlMusicGenerationResult ofxGgmlMusicExternalGenerationBackend::setup(
	const ofxGgmlMusicGenerationRequest & request) {
	settings = request.external;
	loaded = false;
	if (!settings.isConfigured()) {
		return makeError(request, "external music generator executable is not configured");
	}
	if (!fileExists(settings.executablePath)) {
		return makeError(request, "external music generator executable was not found: " + settings.executablePath);
	}
	if (!settings.modelPath.empty() && settings.requireModelPathExists && !fileExists(settings.modelPath)) {
		return makeError(request, "external music generator model was not found: " + settings.modelPath);
	}
	loaded = true;
	auto result = makeResultBase(request);
	result.success = true;
	result.references.push_back("external executable: " + settings.executablePath);
	if (!settings.modelPath.empty()) {
		result.references.push_back("external model: " + settings.modelPath);
	}
	return result;
}

ofxGgmlMusicGenerationResult ofxGgmlMusicExternalGenerationBackend::generate(
	const ofxGgmlMusicGenerationRequest & request) {
	if (!ofxGgmlMusicUtils::hasPrompt(request)) {
		return makeError(request, "music generation prompt is empty");
	}
	if (request.outputPath.empty()) {
		return makeError(request, "music generation output path is empty");
	}
	if (!loaded || settings.executablePath != request.external.executablePath) {
		const auto setupResult = setup(request);
		if (!setupResult) {
			return setupResult;
		}
	}

	const std::filesystem::path outputPath(request.outputPath);
	if (outputPath.has_parent_path()) {
		std::error_code code;
		std::filesystem::create_directories(outputPath.parent_path(), code);
		if (code) {
			return makeError(request, "could not create external generator output directory");
		}
	}

	const auto command = buildCommand(request);
	const auto exitCode = std::system(command.c_str());
	if (exitCode != 0) {
		return makeError(request, "external music generator failed with exit code " + std::to_string(exitCode));
	}
	if (!fileExists(request.outputPath)) {
		return makeError(request, "external music generator did not write the expected output: " + request.outputPath);
	}

	auto result = makeResultBase(request);
	result.success = true;
	result.references.push_back("external executable: " + settings.executablePath);
	if (!settings.modelPath.empty()) {
		result.references.push_back("external model: " + settings.modelPath);
	}

	std::string manifestError;
	if (fileExists(result.manifestPath)) {
		ofxGgmlMusicGenerationResult manifestResult;
		if (ofxGgmlMusicUtils::loadGenerationManifest(result.manifestPath, manifestResult, manifestError)) {
			manifestResult.references.insert(
				manifestResult.references.end(),
				result.references.begin(),
				result.references.end());
			return manifestResult;
		}
	}
	ofxGgmlMusicUtils::writeGenerationManifest(request, result, getBackendName(), manifestError);
	ofxGgmlMusicUtils::appendGenerationHistory(result.historyPath, result.manifestPath, manifestError);
	return result;
}

void ofxGgmlMusicExternalGenerationBackend::close() {
	loaded = false;
	settings = {};
}

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeExternalMusicGenerationBackend() {
	return std::make_unique<ofxGgmlMusicExternalGenerationBackend>();
}
