cmake_minimum_required (VERSION 3.8)

project (RubbishDebugger)
include_directories(${PROJECT_SOURCE_DIR}/ThridParty/Header)
link_directories(${PROJECT_SOURCE_DIR}/ThridParty/Lib)

add_subdirectory ("RubbishDebugger")
