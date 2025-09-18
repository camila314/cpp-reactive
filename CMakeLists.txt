cmake_minimum_required(VERSION 3.10)

project(cpp-reactive VERSION 1.0.0 LANGUAGES CXX)

# Create interface library
add_library(cpp-reactive INTERFACE)

# Set the include directory to the root directory
target_include_directories(cpp-reactive INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Require C++11 or later
target_compile_features(cpp-reactive INTERFACE cxx_std_11)