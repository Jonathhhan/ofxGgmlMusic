cmake_minimum_required(VERSION 3.18)
project(ofxGgmlMusic_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ofxGgmlMusic_tests
	test_main.cpp
	../src/ofxGgmlMusic/ofxGgmlMusicUtils.cpp)

target_include_directories(ofxGgmlMusic_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../src)

enable_testing()
add_test(NAME ofxGgmlMusic_tests COMMAND ofxGgmlMusic_tests)