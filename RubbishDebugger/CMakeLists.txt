cmake_minimum_required (VERSION 3.8)

add_executable (RubbishDebugger "RubbishDebugger.cpp" )
target_link_libraries(RubbishDebugger PUBLIC BeaEngine)

string(REPLACE "/" "\\" TestFileSource "${PROJECT_SOURCE_DIR}/TestFiles")
string(REPLACE "/" "\\" TestFileDir "${PROJECT_BINARY_DIR}/RubbishDebugger")
message(STATUS "Copy test files from ${TestFileSource} to ${TestFileDir}")

execute_process(
	COMMAND cmd /C "copy ${TestFileSource} ${TestFileDir}"
	RESULT_VARIABLE CopyResult
)
