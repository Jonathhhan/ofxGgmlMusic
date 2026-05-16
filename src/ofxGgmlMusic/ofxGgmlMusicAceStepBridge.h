#pragma once

#include "ofxGgmlMusicGenerationBackend.h"

#include "ofJson.h"

#include <memory>
#include <string>
#include <vector>

struct ofxGgmlMusicAceStepRequest {
	std::string caption;
	std::string lyrics;
	int bpm = 0;
	float durationSeconds = 30.0f;
	std::string keyscale;
	std::string timeSignature = "4";
	std::string vocalLanguage;
	int seed = -1;
	int batchSize = 1;
	float lmTemperature = 0.85f;
	float lmCfgScale = 2.0f;
	float lmTopP = 0.9f;
	int lmTopK = 0;
	std::string negativePrompt;
	bool useCotCaption = true;
	bool instrumentalOnly = true;
	std::string audioCodes;
	int inferenceSteps = 0;
	float guidanceScale = 0.0f;
	float shift = 0.0f;
	float audioCoverStrength = 0.5f;
	int repaintingStart = -1;
	int repaintingEnd = -1;
	std::string lego;
	bool wavOutput = true;
	std::string outputDir;
	std::string outputPrefix = "acestep";
};

struct ofxGgmlMusicAceStepHealthResult {
	bool success = false;
	float elapsedMs = 0.0f;
	std::string usedServerUrl;
	std::string status;
	std::string error;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlMusicAceStepGenerateResult {
	bool success = false;
	float elapsedMs = 0.0f;
	std::string usedServerUrl;
	std::string requestJson;
	std::string enrichedRequestJson;
	std::string outputPath;
	std::string error;

	explicit operator bool() const {
		return success;
	}
};

class ofxGgmlMusicAceStepBridge {
public:
	explicit ofxGgmlMusicAceStepBridge(
		std::string serverUrl = "http://127.0.0.1:8085");

	void setServerUrl(const std::string & serverUrl);
	const std::string & getServerUrl() const;

	ofxGgmlMusicAceStepHealthResult healthCheck(
		const std::string & serverUrl = "",
		int timeoutSeconds = 2) const;

	ofxGgmlMusicAceStepGenerateResult generate(
		const ofxGgmlMusicAceStepRequest & request,
		const std::string & serverUrl = "") const;

	std::unique_ptr<ofxGgmlMusicGenerationBackend> createGenerationBackend(
		const std::string & serverUrl = "") const;

	static std::string normalizeServerUrl(
		const std::string & serverUrl,
		const std::string & endpoint = "");
	static ofJson buildRequestJson(const ofxGgmlMusicAceStepRequest & request);
	static std::string summarizeRequestJson(const ofJson & requestJson);

private:
	std::string m_serverUrl;
};

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeAceStepMusicGenerationBackend(
	const std::string & serverUrl = "http://127.0.0.1:8085");
