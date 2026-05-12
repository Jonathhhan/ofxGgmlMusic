cmake_minimum_required(VERSION 3.18)
project(ofxGgmlMusicExternalGenerate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ofxGgmlMusicExternalGenerate
	main.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicAudioUtils.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicExternalGenerationBackend.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicGenerationBackend.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicUtils.cpp)

target_include_directories(ofxGgmlMusicExternalGenerate PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../src)
