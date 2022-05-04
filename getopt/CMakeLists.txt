PROJECT(wingetopt)
cmake_minimum_required(VERSION 3.2)

add_library(wingetopt getopt.c getopt.h)
target_include_directories(wingetopt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(wingetopt PRIVATE _CRT_SECURE_NO_WARNINGS)
