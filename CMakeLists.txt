cmake_minimum_required(VERSION 3.11.0)

project(ECAppLog LANGUAGES CXX VERSION 1.0.0)

SET (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})


add_subdirectory(src)
#add_subdirectory(samplecli)
