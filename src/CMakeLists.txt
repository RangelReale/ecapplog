set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()


option(ECAPPLOG_BUILD_INSTALLER "Build installer" OFF)

SET(EXTERNAL_SOURCE_DIR external)
set(COMPONENT_NAME_MAIN main)

include(FetchContent)
FetchContent_Declare(
  ADS
  GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System
  GIT_TAG        3.8.2
)

set(ADS_VERSION 3.8.2)
set(BUILD_STATIC ON CACHE BOOL "Build static")
set(BUILD_EXAMPLES OFF CACHE BOOL "Don't build dock examples")
FetchContent_MakeAvailable(ADS)

# Set additional project information
set(COMPANY "Rangel Reale")
set(COPYRIGHT "Copyright (c) 2018 Rangel Reale. All rights reserved.")
set(IDENTIFIER ${PROJECT_NAME})

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

set(SRCS
	main.cpp
	MainWindow.h
	MainWindow.cpp
	DetailWindow.h
	DetailWindow.cpp
	Config.h
	Config.cpp
	Server.h
	Server.cpp
	Data.h
	Data.cpp
	Widgets.h
	Widgets.cpp
	LogModel.h
	LogModel.cpp
	LogDelegate.h
	LogDelegate.cpp
)

set(SRCS ${SRCS} resources/ecapplog.qrc)

get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

if (WIN32)
	set(ICON_FILE resources/ecapplog.ico)
	SET( SRCS ${SRCS} resources/resources.h resources/ecapplog.rc )
endif()

if (APPLE)
	set(ICON_FILE resources/ecapplog.icns)
	set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND SRCS ${ICON_FILE})

    # Identify MacOS bundle
    set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    set(MACOSX_BUNDLE_COPYRIGHT ${COPYRIGHT})
    set(MACOSX_BUNDLE_GUI_IDENTIFIER ${IDENTIFIER})
    set(MACOSX_BUNDLE_ICON_FILE ecapplog)
endif()

# Executable: console
add_executable(ecapplog_console
	${SRCS}
)
set_target_properties(ecapplog_console PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/console
	LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/console
)
target_link_libraries(ecapplog_console Qt6::Widgets Qt6::Network qtadvanceddocking)

# Executable: main
add_executable(ecapplog WIN32 MACOSX_BUNDLE
	${SRCS}
)
set_target_properties(ecapplog PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
)
target_link_libraries(ecapplog Qt6::Widgets Qt6::Network qtadvanceddocking)

if (ECAPPLOG_BUILD_INSTALLER)
	install(TARGETS ecapplog 
		DESTINATION ${CMAKE_INSTALL_BINDIR}
		BUNDLE DESTINATION bin
		COMPONENT ${COMPONENT_NAME_MAIN}
	)

	set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
	set(CPACK_COMPONENTS_ALL main)

	include(InstallRequiredSystemLibraries)
	set(CPACK_GENERATOR "IFW")
	set(CPACK_PACKAGE_VERSION_MAJOR "1")
	set(CPACK_PACKAGE_VERSION_MINOR "0")
	include(CPACK)
	include(CPackIFW)

	CPACK_ADD_COMPONENT(${COMPONENT_NAME_MAIN})

	if(WIN32)
		configure_file("${PROJECT_SOURCE_DIR}/cmake/deploy-qt-windows.cmake.in" "${CMAKE_BINARY_DIR}/deploy-qt-windows.cmake" @ONLY)
		set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_BINARY_DIR}/deploy-qt-windows.cmake)
	endif()

	if(APPLE)
		configure_file("${PROJECT_SOURCE_DIR}/cmake/deploy-qt-mac.cmake.in" "${CMAKE_BINARY_DIR}/deploy-qt-mac.cmake" @ONLY)
		set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_BINARY_DIR}/deploy-qt-windows.cmake)
	endif()
endif()

if (WIN32)
	add_custom_command(TARGET ecapplog POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E
			env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
				"$<TARGET_FILE:ecapplog>"
		COMMENT "Running windeployqt..."
	)
endif()

if (APPLE)
	add_custom_command(TARGET ecapplog POST_BUILD
		COMMAND "${MACDEPLOYQT_EXECUTABLE}"
			"$<TARGET_FILE_DIR:ecapplog>/../.."
			-always-overwrite
		COMMENT "Running macdeployqt..."
	)
endif()
