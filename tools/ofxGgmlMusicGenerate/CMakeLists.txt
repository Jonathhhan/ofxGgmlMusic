cmake_minimum_required(VERSION 3.18)
project(ofxGgmlMusicGenerate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ofxGgmlMusicGenerate
	main.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicAudioUtils.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicGenerationBackend.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicProceduralGenerationBackend.cpp
	../../src/ofxGgmlMusic/ofxGgmlMusicUtils.cpp)

target_include_directories(ofxGgmlMusicGenerate PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../src)
