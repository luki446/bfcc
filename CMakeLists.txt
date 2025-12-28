cmake_minimum_required(VERSION 3.25)

project(bfcc LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(ext/CLI11)

set(BOOST_INCLUDE_LIBRARIES process filesystem system)

add_subdirectory(ext/boost SYSTEM)

add_executable(bfcc src/main.cc src/utils.cc src/runner.cc src/builder.cc src/compiler.cc src/target/clang_windows_x64.cc)

target_compile_definitions(bfcc PRIVATE BOOST_ALL_NO_LIB)

target_compile_definitions(bfcc PRIVATE WIN32_LEAN_AND_MEAN)

target_link_libraries(bfcc PRIVATE Boost::process)