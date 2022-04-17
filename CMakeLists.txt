cmake_minimum_required(VERSION 3.11.0)

project(ECAppLog LANGUAGES CXX VERSION 1.0.0)

#
# Basic cofiguration
#
SET (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(InstallRequiredSystemLibraries)

#
# Options
#
option(ECAPPLOG_BUILD_INSTALLER "Build installer" OFF)
option(ECAPPLOG_BUILD_CONSOLE_EXE "Build console exe debug version" OFF)

#
# Project information
#
set(COMPANY "Rangel Reale")
set(COPYRIGHT "Copyright (c) 2018 Rangel Reale. All rights reserved.")
set(IDENTIFIER ${PROJECT_NAME})

#
# Find Packages
#
find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

#
# External Dependencies
#
include(FetchContent)
FetchContent_Declare(
  ads
  GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System
  GIT_TAG        3.8.2
)

set(ADS_VERSION 3.8.2)
set(BUILD_STATIC ON CACHE BOOL "Build static")
set(BUILD_EXAMPLES OFF CACHE BOOL "Don't build dock examples")
FetchContent_GetProperties(ads)
if(NOT ads_POPULATED)
  FetchContent_Populate(ads)
  add_subdirectory(${ads_SOURCE_DIR} ${ads_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

#
# Application
#
add_subdirectory(src)

#
# Installer
#
if (ECAPPLOG_BUILD_INSTALLER)

set(CPACK_PACKAGE_NAME "ECAppLog")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ECAppLog summary")                    
set(CPACK_PACKAGE_VERSION "1.0.0") 
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)

endif()
