cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(sample)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

add_subdirectory(uapp)
add_subdirectory(klib)
add_subdirectory(kapp)