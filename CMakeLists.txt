cmake_minimum_required(VERSION 3.10)

project(cpp-reactive VERSION 1.0.0 LANGUAGES CXX)
add_library(cpp-reactive STATIC lib.cpp)

target_include_directories(cpp-reactive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(cpp-reactive PUBLIC cxx_std_20)