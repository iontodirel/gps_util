cmake_minimum_required (VERSION 3.8)

project ("gps_util")

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

FetchContent_Declare(cxxopts GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git GIT_TAG v3.1.1)
FetchContent_GetProperties(cxxopts)
if(NOT cxxopts_POPULATED)
  FetchContent_Populate(cxxopts)
  add_subdirectory(${cxxopts_SOURCE_DIR} ${cxxopts_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG master)
FetchContent_GetProperties(fmt)
if(NOT fmt_POPULATED)
  FetchContent_Populate(fmt)
  add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(CMAKE_C_COMPILER "gcc-13")
set(CMAKE_CXX_COMPILER "g++-13")

add_executable (gps_util "gps.cpp" "gps.h" "main.cpp" "external/position.hpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET gps_util PROPERTY CXX_STANDARD 23)
endif()

target_link_libraries(gps_util PUBLIC gps cxxopts fmt::fmt)

file(DOWNLOAD
    https://raw.githubusercontent.com/iontodirel/position-lib/main/position.hpp
    ${CMAKE_SOURCE_DIR}/external/position.hpp
)