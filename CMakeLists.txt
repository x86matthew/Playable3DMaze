cmake_minimum_required(VERSION 3.21)

project(3DMaze LANGUAGES CXX)

if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
  message(FATAL_ERROR "This project needs to be built with the Win32 target architecture.")
endif()

add_executable(Playable3DMazeLoader Playable3DMazeLoader/main.cpp)

add_library(Playable3DMaze SHARED Playable3DMaze/main.cpp)

file(GENERATE OUTPUT .gitignore CONTENT "*")

install(TARGETS Playable3DMazeLoader Playable3DMaze
RUNTIME DESTINATION .)
