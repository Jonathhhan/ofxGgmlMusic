cmake_minimum_required(VERSION 3.18)
project(ofxGgmlMusic_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(OFXGGMLMUSIC_BUILD_EXTERNAL_GENERATION_CONTRACT
	"Build the external generation backend contract test" OFF)

add_executable(ofxGgmlMusic_tests
	test_main.cpp
	../src/ofxGgmlMusic/ofxGgmlMusicAudioUtils.cpp
	../src/ofxGgmlMusic/ofxGgmlMusicExternalGenerationBackend.cpp
	../src/ofxGgmlMusic/ofxGgmlMusicGenerationBackend.cpp
	../src/ofxGgmlMusic/ofxGgmlMusicMidiUtils.cpp
	../src/ofxGgmlMusic/ofxGgmlMusicProceduralGenerationBackend.cpp
	../src/ofxGgmlMusic/ofxGgmlMusicUtils.cpp)

target_include_directories(ofxGgmlMusic_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(OFXGGMLMUSIC_BUILD_EXTERNAL_GENERATION_CONTRACT)
	add_executable(music_external_contract
		test_external_generation_contract.cpp
		../src/ofxGgmlMusic/ofxGgmlMusicAudioUtils.cpp
		../src/ofxGgmlMusic/ofxGgmlMusicExternalGenerationBackend.cpp
		../src/ofxGgmlMusic/ofxGgmlMusicGenerationBackend.cpp
		../src/ofxGgmlMusic/ofxGgmlMusicUtils.cpp)

	target_include_directories(music_external_contract PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../src)
endif()

enable_testing()
add_test(NAME ofxGgmlMusic_tests COMMAND ofxGgmlMusic_tests)
