cmake_minimum_required(VERSION 3.20)

project(bfcc LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bfcc src/main.cpp)