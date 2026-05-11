#pragma once

#include "ofxGgmlMusicTypes.h"

#include <memory>
#include <string>

class ofxGgmlMusicGenerationBackend {
public:
	virtual ~ofxGgmlMusicGenerationBackend() = default;

	virtual std::string getBackendName() const = 0;
	virtual ofxGgmlMusicGenerationBackendFamily getBackendFamily() const = 0;
	virtual bool isAvailable() const = 0;
	virtual bool isLoaded() const = 0;
	virtual ofxGgmlMusicGenerationResult setup(const ofxGgmlMusicGenerationRequest & request) = 0;
	virtual ofxGgmlMusicGenerationResult generate(const ofxGgmlMusicGenerationRequest & request) = 0;
	virtual void close() = 0;
};

class ofxGgmlMusicUnavailableGenerationBackend : public ofxGgmlMusicGenerationBackend {
public:
	explicit ofxGgmlMusicUnavailableGenerationBackend(
		ofxGgmlMusicGenerationBackendFamily family = ofxGgmlMusicGenerationBackendFamily::Auto,
		std::string name = "unavailable");

	std::string getBackendName() const override;
	ofxGgmlMusicGenerationBackendFamily getBackendFamily() const override;
	bool isAvailable() const override;
	bool isLoaded() const override;
	ofxGgmlMusicGenerationResult setup(const ofxGgmlMusicGenerationRequest & request) override;
	ofxGgmlMusicGenerationResult generate(const ofxGgmlMusicGenerationRequest & request) override;
	void close() override;

private:
	ofxGgmlMusicGenerationBackendFamily family;
	std::string name;
};

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeUnavailableMusicGenerationBackend(
	ofxGgmlMusicGenerationBackendFamily family = ofxGgmlMusicGenerationBackendFamily::Auto,
	const std::string & name = "unavailable");
