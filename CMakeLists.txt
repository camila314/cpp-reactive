cmake_minimum_required(VERSION 3.10)

project(cpp-reactive VERSION 1.0.0 LANGUAGES CXX)

if (DEFINED CPP_REACTIVE_INTERFACE AND CPP_REACTIVE_INTERFACE)
	add_library(cpp-reactive INTERFACE)
	target_include_directories(cpp-reactive INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
	target_compile_features(cpp-reactive INTERFACE cxx_std_20)

	add_library(cpp-reactive-impl INTERFACE)
	target_include_directories(cpp-reactive-impl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
	target_compile_features(cpp-reactive-impl INTERFACE cxx_std_20)
	target_sources(cpp-reactive-impl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib.cpp)
else()
	add_library(cpp-reactive ${CMAKE_CURRENT_SOURCE_DIR}/lib.cpp)
	target_include_directories(cpp-reactive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
	target_compile_features(cpp-reactive PUBLIC cxx_std_20)
endif()
