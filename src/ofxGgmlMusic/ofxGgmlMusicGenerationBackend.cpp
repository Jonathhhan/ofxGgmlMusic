#include "ofxGgmlMusicGenerationBackend.h"

#include "ofxGgmlMusicUtils.h"

#include <utility>

namespace {
	ofxGgmlMusicGenerationResult makeUnavailableResult(
		const ofxGgmlMusicGenerationRequest & request,
		const std::string & name) {
		ofxGgmlMusicGenerationResult result;
		result.success = false;
		result.error = name + " music generation backend is not available";
		result.seed = request.settings.seed;
		result.durationSeconds = request.settings.durationSeconds;
		result.tempo = request.tempo;
		result.key = request.key;
		return result;
	}
}

ofxGgmlMusicUnavailableGenerationBackend::ofxGgmlMusicUnavailableGenerationBackend(
	ofxGgmlMusicGenerationBackendFamily family,
	std::string name)
	: family(family)
	, name(std::move(name)) {
}

std::string ofxGgmlMusicUnavailableGenerationBackend::getBackendName() const {
	return name;
}

ofxGgmlMusicGenerationBackendFamily ofxGgmlMusicUnavailableGenerationBackend::getBackendFamily() const {
	return family;
}

bool ofxGgmlMusicUnavailableGenerationBackend::isAvailable() const {
	return false;
}

bool ofxGgmlMusicUnavailableGenerationBackend::isLoaded() const {
	return false;
}

ofxGgmlMusicGenerationResult ofxGgmlMusicUnavailableGenerationBackend::setup(
	const ofxGgmlMusicGenerationRequest & request) {
	return makeUnavailableResult(request, name);
}

ofxGgmlMusicGenerationResult ofxGgmlMusicUnavailableGenerationBackend::generate(
	const ofxGgmlMusicGenerationRequest & request) {
	if (!ofxGgmlMusicUtils::hasPrompt(request)) {
		auto result = makeUnavailableResult(request, name);
		result.error = "music generation prompt is empty";
		return result;
	}
	return makeUnavailableResult(request, name);
}

void ofxGgmlMusicUnavailableGenerationBackend::close() {
}

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeUnavailableMusicGenerationBackend(
	ofxGgmlMusicGenerationBackendFamily family,
	const std::string & name) {
	return std::make_unique<ofxGgmlMusicUnavailableGenerationBackend>(family, name);
}
