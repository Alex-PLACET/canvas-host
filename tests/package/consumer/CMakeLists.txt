# Minimal installed-package consumer used by the package test.

cmake_minimum_required(VERSION 3.16)
project(canvas-host-consumer LANGUAGES CXX)

find_package(canvas-host CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE canvas_host::canvas_host)
target_compile_features(consumer PRIVATE cxx_std_17)
