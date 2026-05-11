#pragma once

#include "ofxGgmlMusicGenerationBackend.h"

class ofxGgmlMusicProceduralGenerationBackend : public ofxGgmlMusicGenerationBackend {
public:
	std::string getBackendName() const override;
	ofxGgmlMusicGenerationBackendFamily getBackendFamily() const override;
	bool isAvailable() const override;
	bool isLoaded() const override;
	ofxGgmlMusicGenerationResult setup(const ofxGgmlMusicGenerationRequest & request) override;
	ofxGgmlMusicGenerationResult generate(const ofxGgmlMusicGenerationRequest & request) override;
	void close() override;

private:
	bool loaded = false;
};

std::unique_ptr<ofxGgmlMusicGenerationBackend> ofxGgmlMakeProceduralMusicGenerationBackend();
