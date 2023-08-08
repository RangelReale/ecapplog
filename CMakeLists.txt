cmake_minimum_required(VERSION 3.20.0)

project(ECAppLog LANGUAGES CXX VERSION 1.0.5)

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
option(ECAPPLOG_WITH_DEBUG_MENUS "Enable debug menus" OFF)

if(APPLE)
    option(ECAPPLOG_DISABLE_XCODE_CODESIGNING "Disable codesigning in XCode" OFF)
    # list available ones with command "security find-identity -v -p codesignin"
    SET(ECAPPLOG_XCODE_CODE_SIGN_IDENTITY CACHE STRING "Xcode code sign identidy")
endif()

#
# Project information
#
set(APP_NAME ${PROJECT_NAME})
set(APP_URL "https://github.com/RangelReale/ecapplog")
set(AUTHOR_NAME "Rangel Reale")
set(AUTHOR_EMAIL "rangelreale@gmail.com")
set(ORGANIZATION_NAME "rangelreale.com")
set(INVERSE_ORGANIZATION_NAME "com.rangelreale")
set(COMPANY "Rangel Reale")
set(COPYRIGHT "Copyright (c) 2018 Rangel Reale. All rights reserved.")
set(IDENTIFIER ${PROJECT_NAME})


#
# Find Packages
#
set(QT5_COMPONENTS Widgets Network)
if (UNIX AND NOT APPLE)
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()
if (APPLE)
    # Hack for QtDebus dependency not being added in installer
    LIST(APPEND QT5_COMPONENTS DBus)
endif()
find_package(Qt5 COMPONENTS ${QT5_COMPONENTS} REQUIRED)

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

include(DeployQt)

set(CPACK_PACKAGE_NAME "ECAppLog")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ECAppLog is a networked logging GUI")                    
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}") 
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "${AUTHOR_NAME} <${AUTHOR_EMAIL}>")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
if (NOT APPLE)
    set(CPACK_COMPONENTS_ALL Main)
    set(CPACK_COMPONENT_main_REQUIRED ON)
endif()

if (UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")

    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${APP_URL})
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

    set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
    set(CPACK_RPM_PACKAGE_URL ${APP_URL})
endif()

if (WIN32)
    set(CPACK_GENERATOR "NSIS")

    set(CPACK_NSIS_PACKAGE_NAME ${APP_NAME})
    set(CPACK_NSIS_DISPLAY_NAME ${APP_NAME})
    set(CPACK_NSIS_URL_INFO_ABOUT ${APP_URL})
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\ecapplog.exe")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ecapplog.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${APP_NAME}.lnk' '$INSTDIR\\\\bin\\\\${APP_NAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU\\\\${APP_NAME}.lnk'")
endif()

if (APPLE)
    set(CPACK_GENERATOR "DragNDrop")

    set(CPACK_DMG_VOLUME_NAME ${APP_NAME})
    set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_SOURCE_DIR}/cmake/modules/SignMacApplications.cmake)
endif()

include(CPack)

endif()
