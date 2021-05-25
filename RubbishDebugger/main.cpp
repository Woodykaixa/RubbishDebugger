// RubbishDebugger.cpp: 定义应用程序的入口点。
//

#include "RubbishDebugger.h"
#include "ComHelper.h"
#ifdef WIN64
#error "RubbishDebugger is designed for x86 program"
#endif

auto main(const int argc, const char* argv[]) -> int { // just for fun
	try {
		ComHelper co; // Use stack variable to make sure CoUninitialize will be called.
		ComHelper::Instance = &co;
		std::vector<std::string_view> args;
		args.resize(0);
		RubbishDebugger debugger{"../debugee.exe", args};
		debugger.DebugBegin();
		return 0;
	}
	catch (std::exception& e) {
		std::cout << e.what() << '\n';
		return 1;
	}

}
