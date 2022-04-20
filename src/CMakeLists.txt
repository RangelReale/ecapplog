set(SRCS
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
	Util.h
)

set(EXE_SRCS resources/ecapplog.qrc)
set(LIB_LINK qtadvanceddocking)
if (UNIX AND NOT APPLE)
  LIST(APPEND LIB_LINK xcb)
endif()

if (WIN32)
	set(ICON_FILE resources/ecapplog.ico)
	SET( SRCS ${SRCS} resources/resources.h )
	SET( EXE_SRCS ${EXE_SRCS} resources/ecapplog.rc )
endif()

if (APPLE)
	set(ICON_FILE resources/ecapplog.icns)
	set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND EXE_SRCS ${ICON_FILE})

    # Identify MacOS bundle
    set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    set(MACOSX_BUNDLE_COPYRIGHT ${COPYRIGHT})
    set(MACOSX_BUNDLE_GUI_IDENTIFIER ${IDENTIFIER})
    set(MACOSX_BUNDLE_ICON_FILE ecapplog)
endif()

add_library(ecapploglib STATIC ${SRCS})
target_link_libraries(ecapploglib Qt5::Widgets Qt5::Network qtadvanceddocking)
target_compile_features(ecapploglib PRIVATE cxx_std_17)

if (ECAPPLOG_BUILD_CONSOLE_EXE)
	# Executable: console
	add_executable(ecapplog_console
		main.cpp
		${EXE_SRCS}
	)
	target_compile_features(ecapplog_console PRIVATE cxx_std_17)
	set_target_properties(ecapplog_console PROPERTIES
		XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
		XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
		RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin_console
		LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin_console
	)
	target_link_libraries(ecapplog_console ecapploglib Qt5::Widgets Qt5::Network ${LIB_LINK})
endif()

# Executable: main
add_executable(ecapplog WIN32 MACOSX_BUNDLE
	main.cpp
	${EXE_SRCS}
)
target_compile_features(ecapplog PRIVATE cxx_std_17)
set_target_properties(ecapplog PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
	MACOSX_BUNDLE ON
	MACOSX_RPATH ON
)
target_link_libraries(ecapplog ecapploglib Qt5::Widgets Qt5::Network ${LIB_LINK})
if (APPLE)
	# Hack for QtDebus dependency not being added in installer
	target_link_libraries(ecapplog ecapploglib Qt5::DBus)
endif()

# Install
if(UNIX AND NOT APPLE)
    install(TARGETS ecapplog RUNTIME DESTINATION bin CONFIGURATIONS Release)
elseif(WIN32)
    install(TARGETS ecapplog RUNTIME DESTINATION bin CONFIGURATIONS Release)
elseif(APPLE)
    install(TARGETS ecapplog BUNDLE DESTINATION . RUNTIME DESTINATION bin CONFIGURATIONS Release)
    set_source_files_properties(resources/ecapplog.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(ecapplog
        PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME} Copyright (c) 2022 Rangel Reale"
        MACOSX_BUNDLE_ICON_FILE ecapplog.icns
        MACOSX_BUNDLE_GUI_IDENTIFIER "${PROJECT_NAME}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2022 Rangel Reale"
    )
endif()

if (ECAPPLOG_BUILD_INSTALLER)

if (WIN32)
    include(DeployQt)
    windeployqt(ecapplog)
endif()

if (APPLE)
    include(DeployQt)
    macdeployqt(ecapplog)
endif()

endif()
