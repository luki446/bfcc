cmake_minimum_required(VERSION 3.20)

project(bfcc LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(ext/CLI11)

add_executable(bfcc src/main.cc src/utils.cc src/runner.cc src/builder.cc src/compiler.cc src/target/clang_windows_x64.cc)