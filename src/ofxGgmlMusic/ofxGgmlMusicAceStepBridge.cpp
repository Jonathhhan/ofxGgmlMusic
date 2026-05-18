#include "ofxGgmlMusicAceStepBridge.h"

#include "ofMain.h"
#include "ofxGgmlMusicUtils.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace {
	struct HttpResponse {
		bool started = false;
		int status = 0;
		ofBuffer data;
		std::string error;
	};

	std::string trimCopy(const std::string & text) {
		const auto begin = std::find_if_not(
			text.begin(),
			text.end(),
			[](unsigned char ch) { return std::isspace(ch) != 0; });
		const auto end = std::find_if_not(
			text.rbegin(),
			text.rend(),
			[](unsigned char ch) { return std::isspace(ch) != 0; }).base();
		if (begin >= end) {
			return {};
		}
		return std::string(begin, end);
	}

	std::string safeLyrics(const ofxGgmlMusicAceStepRequest & request) {
		const std::string lyrics = trimCopy(request.lyrics);
		if (!lyrics.empty()) {
			return lyrics;
		}
		return request.instrumentalOnly ? std::string("[Instrumental]") : std::string();
	}

	std::string sanitizeFileStem(const std::string & text) {
		std::string stem;
		stem.reserve(text.size());
		bool lastWasDash = false;
		for (unsigned char ch : text) {
			if (std::isalnum(ch) != 0) {
				stem.push_back(static_cast<char>(std::tolower(ch)));
				lastWasDash = false;
			} else if (!lastWasDash) {
				stem.push_back('-');
				lastWasDash = true;
			}
		}
		while (!stem.empty() && stem.front() == '-') {
			stem.erase(stem.begin());
		}
		while (!stem.empty() && stem.back() == '-') {
			stem.pop_back();
		}
		return stem.empty() ? std::string("acestep") : stem;
	}

	std::string normalizePrefix(const ofxGgmlMusicAceStepRequest & request) {
		const std::string prefix = trimCopy(request.outputPrefix);
		if (!prefix.empty()) {
			return sanitizeFileStem(prefix);
		}
		const std::string caption = trimCopy(request.caption);
		return caption.empty() ? std::string("acestep") : sanitizeFileStem(caption);
	}

	std::string defaultOutputDir() {
		return ofToDataPath("generated/acestep", true);
	}

	std::string detectAudioExtension(const ofBuffer & buffer, bool wavRequested) {
		if (buffer.size() >= 4) {
			const char * data = buffer.getData();
			if (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') {
				return ".wav";
			}
			if (static_cast<unsigned char>(data[0]) == 0xff ||
				(data[0] == 'I' && data[1] == 'D' && data[2] == '3')) {
				return ".mp3";
			}
		}
		return wavRequested ? ".wav" : ".mp3";
	}

	std::string toLowerCopy(std::string text) {
		std::transform(
			text.begin(),
			text.end(),
			text.begin(),
			[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return text;
	}

	bool startsWith(const std::string & text, const std::string & prefix) {
		return text.size() >= prefix.size() &&
			text.compare(0, prefix.size(), prefix) == 0;
	}

	size_t findBoundaryLine(
		const std::string & body,
		const std::string & delimiter,
		size_t cursor) {
		while (cursor < body.size()) {
			const size_t boundary = body.find(delimiter, cursor);
			if (boundary == std::string::npos) {
				return std::string::npos;
			}
			if (boundary == 0 || body[boundary - 1] == '\n') {
				return boundary;
			}
			cursor = boundary + delimiter.size();
		}
		return std::string::npos;
	}

	void trimBoundaryLineBreak(const std::string & body, size_t & partEnd) {
		if (partEnd >= 2 && body[partEnd - 2] == '\r' && body[partEnd - 1] == '\n') {
			partEnd -= 2;
		} else if (partEnd >= 1 && body[partEnd - 1] == '\n') {
			partEnd -= 1;
		}
	}

	bool extractAudioParts(const ofBuffer & response, std::vector<ofBuffer> & audioParts) {
		audioParts.clear();
		if (response.size() == 0) {
			return false;
		}
		const std::string body(response.getData(), response.size());
		if (!startsWith(body, "--")) {
			audioParts.push_back(response);
			return true;
		}

		size_t lineEnd = body.find("\r\n");
		size_t lineBreakSize = 2;
		if (lineEnd == std::string::npos) {
			lineEnd = body.find('\n');
			lineBreakSize = 1;
		}
		if (lineEnd == std::string::npos) {
			return false;
		}

		const std::string delimiter = body.substr(0, lineEnd);
		if (delimiter.size() <= 2) {
			return false;
		}

		size_t cursor = 0;
		while (cursor < body.size()) {
			size_t partBegin = findBoundaryLine(body, delimiter, cursor);
			if (partBegin == std::string::npos) {
				break;
			}
			partBegin += delimiter.size();
			if (partBegin + 1 < body.size() && body.compare(partBegin, 2, "--") == 0) {
				break;
			}
			if (partBegin + lineBreakSize <= body.size() &&
				body.compare(partBegin, lineBreakSize, lineBreakSize == 2 ? "\r\n" : "\n") == 0) {
				partBegin += lineBreakSize;
			} else if (partBegin < body.size() &&
				(body[partBegin] == '\r' || body[partBegin] == '\n')) {
				++partBegin;
			}

			size_t nextPart = findBoundaryLine(body, delimiter, partBegin);
			if (nextPart == std::string::npos) {
				break;
			}
			size_t partEnd = nextPart;
			trimBoundaryLineBreak(body, partEnd);
			const std::string part = partEnd > partBegin
				? body.substr(partBegin, partEnd - partBegin)
				: std::string();

			size_t headerEnd = part.find("\r\n\r\n");
			size_t bodyBegin = std::string::npos;
			if (headerEnd != std::string::npos) {
				bodyBegin = headerEnd + 4;
			} else {
				headerEnd = part.find("\n\n");
				if (headerEnd != std::string::npos) {
					bodyBegin = headerEnd + 2;
				}
			}
			if (bodyBegin == std::string::npos) {
				cursor = nextPart;
				continue;
			}

			const std::string headers = toLowerCopy(part.substr(0, headerEnd));
			if (headers.find("content-type: audio/") != std::string::npos) {
				const std::string audio = part.substr(bodyBegin);
				ofBuffer audioData;
				audioData.set(audio.data(), audio.size());
				if (audioData.size() > 0) {
					audioParts.push_back(audioData);
				}
			}
			cursor = nextPart;
		}
		return !audioParts.empty();
	}

	std::string summarizeCore(
		const std::string & caption,
		const std::string & lyrics,
		int bpm,
		float durationSeconds,
		const std::string & keyscale,
		const std::string & timeSignature) {
		std::ostringstream summary;
		if (!caption.empty()) {
			summary << "Caption: " << caption;
		}
		if (!lyrics.empty()) {
			if (summary.tellp() > 0) {
				summary << "\n";
			}
			summary << "Lyrics: " << lyrics;
		}
		if (bpm > 0 || durationSeconds > 0.0f || !keyscale.empty() || !timeSignature.empty()) {
			if (summary.tellp() > 0) {
				summary << "\n";
			}
			summary << "Meta:";
			if (bpm > 0) {
				summary << " " << bpm << " BPM";
			}
			if (durationSeconds > 0.0f) {
				summary << " | " << durationSeconds << " s";
			}
			if (!keyscale.empty()) {
				summary << " | " << keyscale;
			}
			if (!timeSignature.empty()) {
				summary << " | " << timeSignature << "/4";
			}
		}
		return summary.str();
	}

	HttpResponse performRequest(
		const std::string & url,
		const std::string & method,
		const std::string & body,
		const std::string & accept,
		int timeoutSeconds) {
		HttpResponse result;
#if defined(OF_VERSION_MAJOR)
		ofHttpRequest request(url, "ofxGgmlMusicAceStep");
		request.method = method == "POST" ? ofHttpRequest::POST : ofHttpRequest::GET;
		request.timeoutSeconds = timeoutSeconds;
		if (!body.empty()) {
			request.body = body;
			request.contentType = "application/json";
			request.headers["Content-Type"] = "application/json";
		}
		if (!accept.empty()) {
			request.headers["Accept"] = accept;
		}

		ofURLFileLoader loader;
		const ofHttpResponse response = loader.handleRequest(request);
		result.started = true;
		result.status = response.status;
		result.data = response.data;
		result.error = response.error;
#else
		(void)url;
		(void)method;
		(void)body;
		(void)accept;
		(void)timeoutSeconds;
		result.error = "AceStep requests require openFrameworks HTTP runtime";
#endif
		return result;
	}

	bool isSuccessStatus(int status) {
		return status >= 200 && status < 300;
	}

	bool parseAsyncJobId(const std::string & responseBody, std::string & jobId) {
		const ofJson parsed = ofJson::parse(responseBody, nullptr, false);
		if (!parsed.is_object() || !parsed.contains("id") || !parsed["id"].is_string()) {
			return false;
		}
		jobId = trimCopy(parsed["id"].get<std::string>());
		return !jobId.empty();
	}

	bool parseJobStatusPayload(
		const std::string & responseBody,
		std::string & status,
		std::string & errorMessage) {
		const ofJson parsed = ofJson::parse(responseBody, nullptr, false);
		if (!parsed.is_object()) {
			return false;
		}
		if (parsed.contains("status") && parsed["status"].is_string()) {
			status = trimCopy(parsed["status"].get<std::string>());
		}
		if (parsed.contains("error") && parsed["error"].is_string()) {
			errorMessage = trimCopy(parsed["error"].get<std::string>());
		}
		return !status.empty() || !errorMessage.empty();
	}

	bool fetchJobResult(
		const std::string & baseUrl,
		const std::string & jobId,
		const std::string & accept,
		int timeoutSeconds,
		ofBuffer & resultData,
		std::string & error) {
		const std::string statusUrl = baseUrl + "/job?id=" + jobId;
		const auto deadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(timeoutSeconds);
		while (std::chrono::steady_clock::now() < deadline) {
			const HttpResponse statusResponse =
				performRequest(statusUrl, "GET", {}, "application/json", 30);
			const std::string statusBody = statusResponse.data.getText();
			if (!statusResponse.started || !isSuccessStatus(statusResponse.status)) {
				error = "AceStep /job status failed";
				if (!statusResponse.error.empty()) {
					error += ": " + statusResponse.error;
				} else if (!statusBody.empty()) {
					error += ": " + trimCopy(statusBody);
				}
				return false;
			}

			std::string status;
			std::string statusError;
			if (!parseJobStatusPayload(statusBody, status, statusError)) {
				error = "AceStep /job status returned invalid JSON";
				return false;
			}
			if (status == "done") {
				const HttpResponse resultResponse =
					performRequest(statusUrl + "&result=1", "GET", {}, accept, timeoutSeconds);
				if (!resultResponse.started || !isSuccessStatus(resultResponse.status)) {
					error = "AceStep /job result failed";
					if (!resultResponse.error.empty()) {
						error += ": " + resultResponse.error;
					} else if (!resultResponse.data.getText().empty()) {
						error += ": " + trimCopy(resultResponse.data.getText());
					}
					return false;
				}
				resultData = resultResponse.data;
				return true;
			}
			if (status == "failed" || status == "cancelled") {
				error = statusError.empty()
					? "AceStep job " + status
					: "AceStep job " + status + ": " + statusError;
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		error = "AceStep job timed out";
		return false;
	}

	std::string extractLikelyJsonPayload(const std::string & responseBody) {
		const std::string trimmed = trimCopy(responseBody);
		if (trimmed.empty()) {
			return {};
		}
		const size_t firstObject = trimmed.find('{');
		const size_t firstArray = trimmed.find('[');
		size_t firstJson = std::string::npos;
		if (firstObject == std::string::npos) {
			firstJson = firstArray;
		} else if (firstArray == std::string::npos) {
			firstJson = firstObject;
		} else {
			firstJson = std::min(firstObject, firstArray);
		}
		if (firstJson == std::string::npos) {
			return trimmed;
		}
		const size_t lastObject = trimmed.rfind('}');
		const size_t lastArray = trimmed.rfind(']');
		const size_t lastJson = std::max(
			lastObject == std::string::npos ? size_t(0) : lastObject,
			lastArray == std::string::npos ? size_t(0) : lastArray);
		if (lastJson < firstJson) {
			return trimmed.substr(firstJson);
		}
		return trimCopy(trimmed.substr(firstJson, lastJson - firstJson + 1));
	}

	ofJson parseJsonPayload(const std::string & responseBody) {
		ofJson parsed = ofJson::parse(responseBody, nullptr, false);
		if (!parsed.is_discarded()) {
			return parsed;
		}
		const std::string extracted = extractLikelyJsonPayload(responseBody);
		if (extracted != trimCopy(responseBody)) {
			parsed = ofJson::parse(extracted, nullptr, false);
		}
		return parsed;
	}

	bool buildSynthRequestJsonValue(const ofJson & parsed, ofJson & synthRequest, std::string & error) {
		if (parsed.is_discarded() || parsed.is_null()) {
			error = "AceStep /lm returned invalid JSON";
			return false;
		}
		if (parsed.is_array()) {
			const auto invalid = std::find_if(
				parsed.begin(),
				parsed.end(),
				[](const ofJson & item) { return !item.is_object(); });
			if (parsed.empty() || invalid != parsed.end()) {
				error = "AceStep /lm result did not contain a usable request payload";
				return false;
			}
			synthRequest = parsed;
			return true;
		}
		if (parsed.is_object()) {
			const std::vector<std::string> nestedKeys = {
				"result", "request", "requests", "data", "payload"
			};
			for (const auto & key : nestedKeys) {
				if (!parsed.contains(key)) {
					continue;
				}
				const ofJson & nested = parsed[key];
				if (nested.is_object()) {
					synthRequest = nested;
					return true;
				}
				if (nested.is_array()) {
					const auto invalid = std::find_if(
						nested.begin(),
						nested.end(),
						[](const ofJson & item) { return !item.is_object(); });
					if (nested.empty() || invalid != nested.end()) {
						error = "AceStep /lm result did not contain a usable request payload";
						return false;
					}
					synthRequest = nested;
					return true;
				}
				if (nested.is_string()) {
					if (buildSynthRequestJsonValue(
							parseJsonPayload(nested.get<std::string>()),
							synthRequest,
							error)) {
						return true;
					}
				}
			}
			synthRequest = parsed;
			return true;
		}
		if (parsed.is_string()) {
			return buildSynthRequestJsonValue(
				parseJsonPayload(parsed.get<std::string>()),
				synthRequest,
				error);
		}
		error = "AceStep /lm result did not contain a usable request payload";
		return false;
	}

	std::string buildSynthRequestBodyFromLmResult(const std::string & lmResponseBody, std::string & error) {
		ofJson synthRequest;
		if (!buildSynthRequestJsonValue(parseJsonPayload(lmResponseBody), synthRequest, error)) {
			if (!trimCopy(lmResponseBody).empty()) {
				error += ": " + trimCopy(extractLikelyJsonPayload(lmResponseBody));
			}
			return {};
		}
		return synthRequest.dump();
	}

	std::string applyOutputFormatToSynthRequestBody(
		const std::string & synthRequestBody,
		bool wavOutput) {
		ofJson synthRequest = parseJsonPayload(synthRequestBody);
		if (synthRequest.is_object()) {
			synthRequest["output_format"] = wavOutput ? "wav16" : "mp3";
			return synthRequest.dump();
		}
		if (synthRequest.is_array()) {
			for (auto & item : synthRequest) {
				if (item.is_object()) {
					item["output_format"] = wavOutput ? "wav16" : "mp3";
				}
			}
			return synthRequest.dump();
		}
		return synthRequestBody;
	}

	ofxGgmlMusicGenerationResult makeBaseMusicResult(const ofxGgmlMusicGenerationRequest & request) {
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
}

ofxGgmlMusicAceStepBridge::ofxGgmlMusicAceStepBridge(std::string serverUrl)
	: m_serverUrl(std::move(serverUrl)) {
}

void ofxGgmlMusicAceStepBridge::setServerUrl(const std::string & serverUrl) {
	m_serverUrl = trimCopy(serverUrl);
}

const std::string & ofxGgmlMusicAceStepBridge::getServerUrl() const {
	return m_serverUrl;
}

std::string ofxGgmlMusicAceStepBridge::normalizeServerUrl(
	const std::string & serverUrl,
	const std::string & endpoint) {
	std::string normalized = trimCopy(serverUrl);
	if (normalized.empty()) {
		normalized = "http://127.0.0.1:8085";
	}
	while (!normalized.empty() && normalized.back() == '/') {
		normalized.pop_back();
	}
	const std::vector<std::string> endpointSuffixes = {
		"/health", "/props", "/lm", "/synth", "/understand", "/job"
	};
	if (endpoint.empty()) {
		for (const auto & suffix : endpointSuffixes) {
			if (normalized.size() > suffix.size() &&
				normalized.compare(normalized.size() - suffix.size(), suffix.size(), suffix) == 0) {
				normalized.erase(normalized.size() - suffix.size());
				break;
			}
		}
		return normalized;
	}
	const std::string root = normalizeServerUrl(normalized);
	return endpoint.front() == '/' ? root + endpoint : root + "/" + endpoint;
}

ofJson ofxGgmlMusicAceStepBridge::buildRequestJson(const ofxGgmlMusicAceStepRequest & request) {
	ofJson json;
	json["caption"] = trimCopy(request.caption);
	json["lyrics"] = safeLyrics(request);
	json["bpm"] = std::max(0, request.bpm);
	json["duration"] = std::max(0.0f, request.durationSeconds);
	json["keyscale"] = trimCopy(request.keyscale);
	json["timesignature"] = trimCopy(request.timeSignature);
	json["vocal_language"] = trimCopy(request.vocalLanguage);
	json["seed"] = request.seed;
	json["batch_size"] = std::clamp(request.batchSize, 1, 9);
	json["lm_temperature"] = std::clamp(request.lmTemperature, 0.0f, 2.0f);
	json["lm_cfg_scale"] = std::max(0.0f, request.lmCfgScale);
	json["lm_top_p"] = std::clamp(request.lmTopP, 0.0f, 1.0f);
	json["lm_top_k"] = std::max(0, request.lmTopK);
	json["lm_negative_prompt"] = trimCopy(request.negativePrompt);
	json["use_cot_caption"] = request.useCotCaption;
	json["audio_codes"] = trimCopy(request.audioCodes);
	json["inference_steps"] = std::max(0, request.inferenceSteps);
	json["guidance_scale"] = std::max(0.0f, request.guidanceScale);
	json["shift"] = std::max(0.0f, request.shift);
	json["audio_cover_strength"] = std::clamp(request.audioCoverStrength, 0.0f, 1.0f);
	json["repainting_start"] = request.repaintingStart;
	json["repainting_end"] = request.repaintingEnd;
	json["lego"] = trimCopy(request.lego);
	json["output_format"] = request.wavOutput ? "wav16" : "mp3";
	return json;
}

std::string ofxGgmlMusicAceStepBridge::summarizeRequestJson(const ofJson & requestJson) {
	if (!requestJson.is_object()) {
		return {};
	}
	return summarizeCore(
		trimCopy(requestJson.value("caption", std::string())),
		trimCopy(requestJson.value("lyrics", std::string())),
		requestJson.value("bpm", 0),
		requestJson.value("duration", 0.0f),
		trimCopy(requestJson.value("keyscale", std::string())),
		trimCopy(requestJson.value("timesignature", std::string())));
}

ofxGgmlMusicAceStepHealthResult ofxGgmlMusicAceStepBridge::healthCheck(
	const std::string & serverUrl,
	int timeoutSeconds) const {
	ofxGgmlMusicAceStepHealthResult result;
	const auto start = std::chrono::steady_clock::now();
	result.usedServerUrl = normalizeServerUrl(serverUrl.empty() ? m_serverUrl : serverUrl, "/health");
	const HttpResponse response =
		performRequest(result.usedServerUrl, "GET", {}, "application/json", timeoutSeconds);
	result.elapsedMs = std::chrono::duration<float, std::milli>(
		std::chrono::steady_clock::now() - start).count();
	result.status = trimCopy(response.data.getText());
	if (!response.started) {
		result.error = response.error.empty() ? "AceStep health request did not start" : response.error;
		return result;
	}
	if (!isSuccessStatus(response.status)) {
		result.error = "AceStep health returned HTTP " + std::to_string(response.status);
		if (!response.error.empty()) {
			result.error += ": " + response.error;
		} else if (!result.status.empty()) {
			result.error += ": " + result.status;
		}
		return result;
	}
	result.success = true;
	return result;
}

ofxGgmlMusicAceStepGenerateResult ofxGgmlMusicAceStepBridge::generate(
	const ofxGgmlMusicAceStepRequest & request,
	const std::string & serverUrl) const {
	ofxGgmlMusicAceStepGenerateResult result;
	const std::string baseUrl = normalizeServerUrl(serverUrl.empty() ? m_serverUrl : serverUrl);
	result.usedServerUrl = baseUrl;
	if (trimCopy(request.caption).empty()) {
		result.error = "AceStep generation requires a caption";
		return result;
	}

	const auto start = std::chrono::steady_clock::now();
	const std::string requestBody = buildRequestJson(request).dump();
	result.requestJson = requestBody;
	HttpResponse lmResponse = performRequest(
		normalizeServerUrl(baseUrl, "/lm"),
		"POST",
		requestBody,
		"application/json",
		240);
	if (!lmResponse.started || !isSuccessStatus(lmResponse.status)) {
		result.error = "AceStep /lm failed";
		if (!lmResponse.error.empty()) {
			result.error += ": " + lmResponse.error;
		} else if (!lmResponse.data.getText().empty()) {
			result.error += ": " + trimCopy(lmResponse.data.getText());
		}
		return result;
	}

	std::string lmBody = lmResponse.data.getText();
	std::string jobId;
	if (parseAsyncJobId(lmBody, jobId)) {
		ofBuffer jobData;
		if (!fetchJobResult(baseUrl, jobId, "application/json", 600, jobData, result.error)) {
			return result;
		}
		lmBody = jobData.getText();
	}
	result.enrichedRequestJson = lmBody;

	std::string synthRequestError;
	const std::string lmSynthRequestBody =
		buildSynthRequestBodyFromLmResult(lmBody, synthRequestError);
	if (lmSynthRequestBody.empty()) {
		result.error = synthRequestError;
		return result;
	}
	const std::string synthRequestBody =
		applyOutputFormatToSynthRequestBody(lmSynthRequestBody, request.wavOutput);

	HttpResponse synthResponse = performRequest(
		normalizeServerUrl(baseUrl, "/synth"),
		"POST",
		synthRequestBody,
		request.wavOutput ? "audio/wav" : "audio/mpeg",
		900);
	if (!synthResponse.started || !isSuccessStatus(synthResponse.status)) {
		result.error = "AceStep /synth failed";
		if (!synthResponse.error.empty()) {
			result.error += ": " + synthResponse.error;
		} else if (!synthResponse.data.getText().empty()) {
			result.error += ": " + trimCopy(synthResponse.data.getText());
		}
		return result;
	}

	ofBuffer audioData = synthResponse.data;
	if (parseAsyncJobId(audioData.getText(), jobId)) {
		if (!fetchJobResult(
				baseUrl,
				jobId,
				request.wavOutput ? "audio/wav" : "audio/mpeg",
				1200,
				audioData,
				result.error)) {
			return result;
		}
	}
	std::vector<ofBuffer> audioParts;
	if (!extractAudioParts(audioData, audioParts)) {
		result.error = "AceStep /synth returned multipart data without an audio part";
		return result;
	}
	if (audioParts.empty()) {
		result.error = "AceStep /synth returned empty audio";
		return result;
	}

	const std::filesystem::path outputDir(
		trimCopy(request.outputDir).empty() ? defaultOutputDir() : trimCopy(request.outputDir));
	std::error_code ec;
	std::filesystem::create_directories(outputDir, ec);
	if (ec) {
		result.error = "Could not create AceStep output directory: " + outputDir.string();
		return result;
	}
	const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	const std::string outputPrefix = normalizePrefix(request) + "-" + std::to_string(nowMs);
	for (size_t i = 0; i < audioParts.size(); ++i) {
		const std::string suffix = audioParts.size() == 1 ? std::string() : "-" + std::to_string(i + 1);
		const std::filesystem::path outputPath =
			outputDir /
			(outputPrefix + suffix + detectAudioExtension(audioParts[i], request.wavOutput));
		if (!ofBufferToFile(outputPath.string(), audioParts[i], true)) {
			result.error = "Could not write AceStep audio: " + outputPath.string();
			return result;
		}
		result.outputPaths.push_back(outputPath.lexically_normal().string());
	}

	result.outputPath = result.outputPaths.front();
	result.elapsedMs = std::chrono::duration<float, std::milli>(
		std::chrono::steady_clock::now() - start).count();
	result.success = true;
	return result;
}

namespace {
	class ofxGgmlMusicAceStepGenerationBackend : public ofxGgmlMusicGenerationBackend {
	public:
		explicit ofxGgmlMusicAceStepGenerationBackend(std::string serverUrl)
			: serverUrl(std::move(serverUrl)) {
			bridge.setServerUrl(this->serverUrl);
		}

		std::string getBackendName() const override {
			return "acestep-server";
		}

		ofxGgmlMusicGenerationBackendFamily getBackendFamily() const override {
			return ofxGgmlMusicGenerationBackendFamily::Transformer;
		}

		bool isAvailable() const override {
			return true;
		}

		bool isLoaded() const override {
			return loaded;
		}

		ofxGgmlMusicGenerationResult setup(const ofxGgmlMusicGenerationRequest & request) override {
			loaded = false;
			auto result = makeBaseMusicResult(request);
			const auto health = bridge.healthCheck(serverUrl, 2);
			if (!health) {
				result.error = health.error.empty() ? "AceStep server is not reachable" : health.error;
				return result;
			}
			loaded = true;
			result.success = true;
			result.references.push_back("AceStep server: " + ofxGgmlMusicAceStepBridge::normalizeServerUrl(serverUrl));
			return result;
		}

		ofxGgmlMusicGenerationResult generate(const ofxGgmlMusicGenerationRequest & request) override {
			if (!ofxGgmlMusicUtils::hasPrompt(request)) {
				auto result = makeBaseMusicResult(request);
				result.error = "music generation prompt is empty";
				return result;
			}
			ofxGgmlMusicAceStepRequest aceRequest;
			aceRequest.caption = request.prompt;
			aceRequest.durationSeconds = static_cast<float>(request.settings.durationSeconds);
			aceRequest.seed = request.settings.seed;
			aceRequest.instrumentalOnly = true;
			aceRequest.negativePrompt = request.negativePrompt;
			aceRequest.outputDir = request.outputPath.empty()
				? std::string()
				: std::filesystem::path(request.outputPath).parent_path().string();
			aceRequest.outputPrefix = request.outputPath.empty()
				? std::string("acestep")
				: std::filesystem::path(request.outputPath).stem().string();
			aceRequest.wavOutput = true;

			const auto aceResult = bridge.generate(aceRequest, serverUrl);
			auto result = makeBaseMusicResult(request);
			result.success = aceResult.success;
			result.outputPath = aceResult.outputPath;
			result.manifestPath = ofxGgmlMusicUtils::getGenerationManifestPath(result.outputPath);
			result.historyPath = ofxGgmlMusicUtils::getGenerationHistoryPath(result.outputPath);
			result.error = aceResult.error;
			result.references.push_back("AceStep server: " + aceResult.usedServerUrl);
			result.references.push_back("AceStep request: " + aceResult.requestJson);
			if (result.success) {
				std::string manifestError;
				ofxGgmlMusicUtils::writeGenerationManifest(request, result, getBackendName(), manifestError);
				ofxGgmlMusicUtils::appendGenerationHistory(result.historyPath, result.manifestPath, manifestError);
			}
			return result;
		}

		void close() override {
			loaded = false;
		}

	private:
		std::string serverUrl;
		ofxGgmlMusicAceStepBridge bridge;
		bool loaded = false;
	};
}

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMusicAceStepBridge::createGenerationBackend(
	const std::string & serverUrl) const {
	const std::string configuredUrl = serverUrl.empty() ? m_serverUrl : serverUrl;
	return std::make_unique<ofxGgmlMusicAceStepGenerationBackend>(configuredUrl);
}

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeAceStepMusicGenerationBackend(
	const std::string & serverUrl) {
	return std::make_unique<ofxGgmlMusicAceStepGenerationBackend>(serverUrl);
}
