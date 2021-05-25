#include "RubbishDebugger.h"
#include "ComHelper.h"
#ifdef WIN64
#error "RubbishDebugger should be compiled in x86 mode"
#endif

auto main(const int argc, const char *argv[]) -> int
{ // just for fun
	try
	{
		ComHelper co; // Use stack variable to make sure CoUninitialize will be called.
		ComHelper::Instance = &co;
		std::vector<std::string_view> args;
		args.resize(0);
		for (auto i = 2; i < argc; i++)
		{
			args.emplace_back(argv[i]);
		}
		RubbishDebugger debugger{argv[1], args};
		debugger.DebugBegin();
		return 0;
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << '\n';
		return 1;
	}
}
