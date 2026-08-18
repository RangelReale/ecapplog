cmake_minimum_required(VERSION 3.20.0)

# Must be set before project(), otherwise the compiler probe bakes in the host SDK version
# and the binary ends up requiring the exact macOS release it was built on. 11.0 matches the
# LC_BUILD_VERSION minos of the bundled Qt 5.15 frameworks.
# Tests for empty rather than DEFINED so that a pre-existing cache holding an empty value is
# corrected too; an explicit -DCMAKE_OSX_DEPLOYMENT_TARGET=... is still respected.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(ECAppLog LANGUAGES CXX VERSION 1.4.0)

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
    option(ECAPPLOG_NOTARIZE "Notarize and staple the package (needs network + credentials)" OFF)

    # A "Developer ID Application" identity is required to distribute outside the App Store.
    # List available ones with: security find-identity -v -p codesigning
    # Accepts either the full name or the SHA-1 hash.
    SET(ECAPPLOG_XCODE_CODE_SIGN_IDENTITY CACHE STRING "Deprecated alias for ECAPPLOG_MAC_SIGN_IDENTITY")
    set(ECAPPLOG_MAC_SIGN_IDENTITY "${ECAPPLOG_XCODE_CODE_SIGN_IDENTITY}" CACHE STRING
        "Developer ID Application identity (name or SHA-1 hash)")

    # Created once with:
    #   xcrun notarytool store-credentials "ecapplog-notary" \
    #       --apple-id <email> --team-id <TEAMID> --password <app-specific-password>
    set(ECAPPLOG_NOTARY_PROFILE "ecapplog-notary" CACHE STRING
        "notarytool keychain profile name")
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

    # Default would be "ECAppLog-1.0.6-Darwin.dmg", which tells a downloader nothing about which
    # Mac it runs on. The bundled Qt is single-architecture, so name the artifact accordingly.
    if(CMAKE_OSX_ARCHITECTURES)
        string(REPLACE ";" "-" _pkg_arch "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(_pkg_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-macOS-${_pkg_arch}")
    set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_SOURCE_DIR}/cmake/modules/SignMacApplications.cmake)
    set(CPACK_POST_BUILD_SCRIPTS ${CMAKE_SOURCE_DIR}/cmake/modules/NotarizeMacPackage.cmake)

    # The pre/post build scripts run in a separate CMake process that only inherits CPACK_* vars.
    set(CPACK_MAC_SIGN_IDENTITY "${ECAPPLOG_MAC_SIGN_IDENTITY}")
    set(CPACK_MAC_NOTARIZE "${ECAPPLOG_NOTARIZE}")
    set(CPACK_MAC_NOTARY_PROFILE "${ECAPPLOG_NOTARY_PROFILE}")
endif()

include(CPack)

#
# Release targets (make release / make release-check)
#
if (APPLE)
    # "make release-check" verifies an already-packaged DMG without publishing anything.
    add_custom_target(release-check
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/release-mac.sh"
                --build-dir "${CMAKE_BINARY_DIR}"
                --version "${PROJECT_VERSION}"
                --verify-only
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Verifying the macOS package is signed, notarized and stapled"
        VERBATIM)

    # "make release" builds, packages, verifies, tags and uploads to GitHub Releases.
    # USES_TERMINAL so the script's confirmation prompt is actually visible and answerable.
    add_custom_target(release
        COMMAND "${CMAKE_CPACK_COMMAND}"
                --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
                -B "${CMAKE_BINARY_DIR}"
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/release-mac.sh"
                --build-dir "${CMAKE_BINARY_DIR}"
                --version "${PROJECT_VERSION}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Packaging and publishing the macOS release"
        VERBATIM)
    # Depend on the app rather than shelling out to a nested build of this same tree.
    add_dependencies(release ecapplog)
endif()

if (WIN32)
    # PowerShell 7 if it is installed, otherwise the Windows PowerShell that ships with the OS.
    find_program(ECAPPLOG_POWERSHELL NAMES pwsh powershell)
    if (NOT ECAPPLOG_POWERSHELL)
        message(WARNING "Neither pwsh nor powershell was found; the release targets are unavailable")
    else()
        set(_release_win
            "${ECAPPLOG_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/release-win.ps1"
            -BuildDir "${CMAKE_BINARY_DIR}"
            -Version "${PROJECT_VERSION}")

        # "release-check" verifies an already-packaged installer without publishing anything.
        add_custom_target(release-check
            COMMAND ${_release_win} -VerifyOnly
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            USES_TERMINAL
            COMMENT "Verifying the Windows installer payload is complete and correctly versioned"
            VERBATIM)

        # "release" packages, verifies and uploads to GitHub Releases.
        # USES_TERMINAL so the script's confirmation prompt is actually visible and answerable.
        add_custom_target(release
            COMMAND "${CMAKE_CPACK_COMMAND}"
                    -C $<CONFIG>
                    --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
                    -B "${CMAKE_BINARY_DIR}"
            COMMAND ${_release_win}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            USES_TERMINAL
            COMMENT "Packaging and publishing the Windows release"
            VERBATIM)
        # Depend on the app rather than shelling out to a nested build of this same tree.
        add_dependencies(release ecapplog)
    endif()
endif()

endif()
