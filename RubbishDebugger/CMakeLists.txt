cmake_minimum_required (VERSION 3.8)

add_executable (RubbishDebugger "main.cpp" "RubbishDebugger.cpp" "RubbishDebugger.h" "PortableExecutable.h" "PortableExecutable.cpp" "Misc.h" "ProgramDatabase.h" "ProgramDatabase.cpp" "ComHelper.h" "ComHelper.cpp")

target_link_libraries(RubbishDebugger PRIVATE Zydis)
string(REPLACE "/" "\\" TestFileSource "${PROJECT_SOURCE_DIR}/TestFiles")
string(REPLACE "/" "\\" TestFileDir "${PROJECT_BINARY_DIR}/RubbishDebugger")
message(STATUS "Copy test files from ${TestFileSource} to ${TestFileDir}")

execute_process(
	COMMAND cmd /C "copy ${TestFileSource} ${TestFileDir}"
	RESULT_VARIABLE CopyResult
)
