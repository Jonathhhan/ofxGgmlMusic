#pragma once

#include "ofxGgmlMusicTypes.h"

#include <string>
#include <vector>

namespace ofxGgmlMusicUtils {
	bool hasInput(const ofxGgmlMusicRequest & request);
	bool hasPrompt(const ofxGgmlMusicGenerationRequest & request);
	bool hasOutput(const ofxGgmlMusicGenerationResult & result);
	bool hasTempo(const ofxGgmlMusicResult & result);
	bool hasTempo(const ofxGgmlMusicGenerationRequest & request);
	bool hasTempo(const ofxGgmlMusicGenerationResult & result);
	bool hasKey(const ofxGgmlMusicResult & result);
	bool hasKey(const ofxGgmlMusicGenerationRequest & request);
	bool hasKey(const ofxGgmlMusicGenerationResult & result);
	std::string getTaskName(ofxGgmlMusicTask task);
	std::string getGenerationBackendName(ofxGgmlMusicGenerationBackendFamily backend);
	std::string formatKey(const ofxGgmlMusicKey & key);
	std::vector<std::string> getGenerationPresetNames();
	std::vector<std::string> getGenerationStemNames();
	std::vector<std::string> getGenerationKeyTonics();
	std::vector<std::string> getGenerationKeyModes();
	bool applyGenerationPreset(
		const std::string & presetName,
		ofxGgmlMusicGenerationRequest & request);
	std::string describe(const ofxGgmlMusicRequest & request);
	std::string describe(const ofxGgmlMusicGenerationRequest & request);
	std::string getGenerationManifestPath(const std::string & outputPath);
	std::string getGenerationHistoryPath(const std::string & outputPath);
	std::string serializeGenerationManifest(
		const ofxGgmlMusicGenerationRequest & request,
		const ofxGgmlMusicGenerationResult & result,
		const std::string & backendName);
	bool writeGenerationManifest(
		const ofxGgmlMusicGenerationRequest & request,
		const ofxGgmlMusicGenerationResult & result,
		const std::string & backendName,
		std::string & error);
	bool loadGenerationManifest(
		const std::string & path,
		ofxGgmlMusicGenerationResult & result,
		std::string & error);
	bool writeGenerationHistory(
		const std::string & historyPath,
		const std::vector<std::string> & manifestPaths,
		std::string & error);
	bool loadGenerationHistory(
		const std::string & historyPath,
		std::vector<std::string> & manifestPaths,
		std::string & error);
	bool appendGenerationHistory(
		const std::string & historyPath,
		const std::string & manifestPath,
		std::string & error);
}
