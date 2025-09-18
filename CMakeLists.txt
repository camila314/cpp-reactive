cmake_minimum_required(VERSION 3.10)

project(cpp-reactive VERSION 1.0.0 LANGUAGES CXX)
add_library(cpp-reactive INTERFACE)

target_include_directories(cpp-reactive INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(cpp-reactive INTERFACE cxx_std_20)
