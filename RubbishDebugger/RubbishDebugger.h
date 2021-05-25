#pragma once
#include <string_view>
#include <unordered_map>
#include <vector>
#include <Windows.h>
#include "PortableExecutable.h"
#include "ProgramDatabase.h"

struct BreakpointInfo {
	DWORD Address;
	BYTE OriginCode;
};

class RubbishDebugger {

public:
	RubbishDebugger(const char* debugeeName, std::vector<std::string_view>& args);
	~RubbishDebugger();
	bool DebugBegin();
	bool Write(void* address, void const* buffer, DWORD size) const;
	bool Read(void const* address, void* buffer, DWORD size) const;
	bool SetBreakpoint(void* address);
	bool RemoveBreakpoint(void* address);
	bool HandleDebugEvent();
	DWORD HandleExceptionDebugEvent(DEBUG_EVENT& dbgEvent);
	bool HandleCommand();
private:
	void ShowAssembly() const;
	void ShowMemory(const void* address) const;
	PROCESS_INFORMATION _info;
	PortableExecutable* _pe;
	ProgramDatabase* _pdb;
	char* _cmdline;
	const char* _debugee;
	bool _entryBpPresent;

	std::unordered_map<void*, BYTE> _breakpointOriginalCodes;
	DWORD _lastEip;
	CONTEXT _context;
	DWORD _dbgThread;
};
