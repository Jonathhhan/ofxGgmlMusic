#include "ofxGgmlMusicExternalGenerationBackend.h"

#include "ofxGgmlMusicUtils.h"

#include <filesystem>
#include <sstream>
#include <vector>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#else
	#include <sys/wait.h>
	#include <unistd.h>
#endif

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

	void appendFlagValue(
		std::vector<std::string> & arguments,
		const std::string & flag,
		const std::string & value) {
		if (!flag.empty() && !value.empty()) {
			arguments.push_back(flag);
			arguments.push_back(value);
		}
	}

	std::vector<std::string> buildArguments(const ofxGgmlMusicGenerationRequest & request) {
		const auto & external = request.external;
		std::vector<std::string> arguments;
		appendFlagValue(arguments, external.promptFlag, request.prompt);
		appendFlagValue(arguments, external.outputFlag, request.outputPath);
		appendFlagValue(arguments, external.modelFlag, external.modelPath);
		if (!external.durationFlag.empty() && request.settings.durationSeconds > 0.0) {
			std::ostringstream duration;
			duration << request.settings.durationSeconds;
			arguments.push_back(external.durationFlag);
			arguments.push_back(duration.str());
		}
		if (!external.seedFlag.empty() && request.settings.seed >= 0) {
			arguments.push_back(external.seedFlag);
			arguments.push_back(std::to_string(request.settings.seed));
		}
		for (const auto & argument : external.extraArguments) {
			if (!argument.empty()) {
				arguments.push_back(argument);
			}
		}
		return arguments;
	}

	struct ProcessResult {
		int exitCode = -1;
		std::string error;
	};

#if defined(_WIN32)
	std::wstring widenUtf8(const std::string & text) {
		if (text.empty()) {
			return {};
		}
		const int size = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			text.data(),
			static_cast<int>(text.size()),
			nullptr,
			0);
		if (size <= 0) {
			return std::wstring(text.begin(), text.end());
		}
		std::wstring wide(size, L'\0');
		MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			text.data(),
			static_cast<int>(text.size()),
			wide.data(),
			size);
		return wide;
	}

	std::wstring quoteWindowsArgument(const std::wstring & value) {
		if (value.empty()) {
			return L"\"\"";
		}
		if (value.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
			return value;
		}

		std::wstring quoted = L"\"";
		size_t backslashes = 0;
		for (const wchar_t ch : value) {
			if (ch == L'\\') {
				++backslashes;
				continue;
			}
			if (ch == L'"') {
				quoted.append(backslashes * 2 + 1, L'\\');
				quoted.push_back(ch);
				backslashes = 0;
				continue;
			}
			quoted.append(backslashes, L'\\');
			backslashes = 0;
			quoted.push_back(ch);
		}
		quoted.append(backslashes * 2, L'\\');
		quoted.push_back(L'"');
		return quoted;
	}

	ProcessResult executeProcess(
		const std::string & executablePath,
		const std::vector<std::string> & arguments,
		const std::string & workingDirectory) {
		const std::wstring executable = widenUtf8(executablePath);
		std::wstring commandLine = quoteWindowsArgument(executable);
		for (const auto & argument : arguments) {
			commandLine += L" ";
			commandLine += quoteWindowsArgument(widenUtf8(argument));
		}
		std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};
		std::wstring workingDirectoryWide = widenUtf8(workingDirectory);
		const wchar_t * workingDirectoryPtr = workingDirectoryWide.empty()
			? nullptr
			: workingDirectoryWide.c_str();

		if (!CreateProcessW(
				executable.c_str(),
				mutableCommandLine.data(),
				nullptr,
				nullptr,
				FALSE,
				0,
				nullptr,
				workingDirectoryPtr,
				&startupInfo,
				&processInfo)) {
			return {
				-1,
				"could not launch external music generator: Windows error " +
					std::to_string(GetLastError())
			};
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE);
		DWORD exitCode = 1;
		if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
			exitCode = 1;
		}
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return { static_cast<int>(exitCode), {} };
	}
#else
	ProcessResult executeProcess(
		const std::string & executablePath,
		const std::vector<std::string> & arguments,
		const std::string & workingDirectory) {
		std::vector<std::string> ownedArguments;
		ownedArguments.reserve(arguments.size() + 1);
		ownedArguments.push_back(executablePath);
		ownedArguments.insert(ownedArguments.end(), arguments.begin(), arguments.end());

		std::vector<char *> argv;
		argv.reserve(ownedArguments.size() + 1);
		for (auto & argument : ownedArguments) {
			argv.push_back(const_cast<char *>(argument.c_str()));
		}
		argv.push_back(nullptr);

		const pid_t pid = fork();
		if (pid < 0) {
			return { -1, "could not fork external music generator process" };
		}
		if (pid == 0) {
			if (!workingDirectory.empty()) {
				chdir(workingDirectory.c_str());
			}
			execv(executablePath.c_str(), argv.data());
			_exit(127);
		}

		int status = 0;
		if (waitpid(pid, &status, 0) < 0) {
			return { -1, "could not wait for external music generator process" };
		}
		if (WIFEXITED(status)) {
			return { WEXITSTATUS(status), {} };
		}
		if (WIFSIGNALED(status)) {
			return { 128 + WTERMSIG(status), {} };
		}
		return { -1, "external music generator ended with an unknown process status" };
	}
#endif
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

	const auto processResult =
		executeProcess(request.external.executablePath, buildArguments(request), request.external.workingDirectory);
	if (processResult.exitCode != 0) {
		const std::string detail = processResult.error.empty()
			? "exit code " + std::to_string(processResult.exitCode)
			: processResult.error;
		return makeError(request, "external music generator failed with " + detail);
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
