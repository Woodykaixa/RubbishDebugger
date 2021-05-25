#include "RubbishDebugger.h"
#include <Windows.h>
#include <debugapi.h>
#include <sstream>
#include <iostream>
#include "Misc.h"
#include <Zydis/Zydis.h>
#include <dia2.h>
#include <regex>
const static char INT3 = 0xCC;

RubbishDebugger::RubbishDebugger(const char* debugeeName, std::vector<std::string_view>& args) {
	_pe = new PortableExecutable{debugeeName};
	_debugee = debugeeName;
	_cmdline = nullptr;
	_lastEip = 0;
	_pdb = nullptr;
	if (args.empty()) {
		return;
	}
	std::stringstream cmd;
	auto size = 0;
	for (const auto& arg : args) {
		cmd << arg.data() << ' ';
		size += arg.size() + 1;
	}

	_cmdline = new char[size];
	cmd.getline(_cmdline, size);
}

RubbishDebugger::~RubbishDebugger() {
	delete _pe;
	delete[] _cmdline;
}

bool RubbishDebugger::DebugBegin() {
	STARTUPINFO startupInfo{sizeof STARTUPINFO};
	if (CreateProcess(
		_debugee,
		_cmdline,
		nullptr,
		nullptr,
		false,
		CREATE_SUSPENDED | DEBUG_PROCESS | CREATE_NEW_CONSOLE,
		nullptr,
		nullptr,
		&startupInfo,
		&_info
	) == false) {
		PrintLastError("Create process failed");
		return false;
	}

	if (ResumeThread(_info.hThread) == static_cast<DWORD>(-1)) {
		PrintLastError("Resume thread failed");
	}
	return HandleDebugEvent();
}

bool RubbishDebugger::Write(void* address, void const* buffer, const DWORD size) const {
	return WriteProcessMemory(_info.hProcess, address, buffer, size, nullptr);
}

bool RubbishDebugger::Read(void const* address, void* buffer, const DWORD size) const {
	return ReadProcessMemory(_info.hProcess, address, buffer, size, nullptr);
}

bool RubbishDebugger::SetBreakpoint(void* address) {
	if (const auto it = _breakpointOriginalCodes.find(address); it != _breakpointOriginalCodes.end()) {
		return true;
	}
	BYTE buffer;

	if (!Read(address, &buffer, 1)) {
		PrintLastErrorVa(__FUNCTION__ " Read memory at %p failed", address);
		return false;
	}
	if (!Write(address, &INT3, 1)) {
		PrintLastErrorVa(__FUNCTION__ " Write memory at %p failed", address);
		return false;
	}
	_breakpointOriginalCodes.emplace(address, buffer);
	return true;
}

bool RubbishDebugger::RemoveBreakpoint(void* address) {
	auto it = _breakpointOriginalCodes.find(address);
	if (it == _breakpointOriginalCodes.end()) {
		return true;
	}

	if (!Write(address, &it->second, 1)) {
		PrintLastErrorVa(__FUNCTION__ " Write memory at %p failed", address);
		return false;
	}
	_breakpointOriginalCodes.erase(it);
	if (reinterpret_cast<unsigned long>(address) == _context.Eip - 1) {
		_context.Eip--;
		_lastEip--;
		SetThreadContext(_info.hThread, &_context);
	}
	return true;
}

bool RubbishDebugger::HandleDebugEvent() {
	DEBUG_EVENT dbgEvent;
	while (true) {
		WaitForDebugEvent(&dbgEvent, INFINITE);
		auto code = DBG_CONTINUE;
		switch (dbgEvent.dwDebugEventCode) {
		case CREATE_PROCESS_DEBUG_EVENT:
			_info.hProcess = dbgEvent.u.CreateProcessInfo.hProcess;
			_info.dwThreadId = dbgEvent.dwProcessId;
			_info.hThread = dbgEvent.u.CreateProcessInfo.hThread;
			_info.dwThreadId = dbgEvent.dwThreadId;
			CloseHandle(dbgEvent.u.CreateProcessInfo.hFile);
			_pe->EntryPoint = dbgEvent.u.CreateProcessInfo.lpStartAddress;
			_pe->ImageBase = dbgEvent.u.CreateProcessInfo.lpBaseOfImage;
			_lastEip = reinterpret_cast<DWORD>(dbgEvent.u.CreateProcessInfo.lpStartAddress);
			SetBreakpoint(_pe->EntryPoint);
			printf("Base loading address of debugee: %p\n", _pe->ImageBase);
			printf("Set breakpoint at entry point: %p\n", _pe->EntryPoint);
			_entryBpPresent = true;
			_dbgThread = dbgEvent.dwThreadId;
			break;
		case EXIT_PROCESS_DEBUG_EVENT:
			printf("Process exited: %08X\n", dbgEvent.u.ExitProcess.dwExitCode);
			CloseHandle(_info.hProcess);
			return true;
		case EXCEPTION_DEBUG_EVENT:
			if (_info.dwProcessId != dbgEvent.dwProcessId && _info.dwThreadId != dbgEvent.dwThreadId) {
				printf("Exception: %08X, on process: %lu, thread: %lu\n",
				       dbgEvent.u.Exception.ExceptionRecord.ExceptionCode, dbgEvent.dwProcessId, dbgEvent.dwThreadId
				);
			}
			code = HandleExceptionDebugEvent(dbgEvent);
			break;
		default:
			break;
		}
		ContinueDebugEvent(dbgEvent.dwProcessId, dbgEvent.dwThreadId, code);

	}
}

DWORD RubbishDebugger::HandleExceptionDebugEvent(DEBUG_EVENT& dbgEvent) {
	auto const exceptCode = dbgEvent.u.Exception.ExceptionRecord.ExceptionCode;
	auto const exceptAddr = dbgEvent.u.Exception.ExceptionRecord.ExceptionAddress;
	_context.ContextFlags = CONTEXT_FULL;
	GetThreadContext(_info.hThread, &_context);
	_lastEip = _context.Eip;
	if (exceptCode == EXCEPTION_BREAKPOINT) {
		RemoveBreakpoint(exceptAddr);
	}
	while (!HandleCommand()) { }
	if (exceptCode == EXCEPTION_BREAKPOINT || exceptCode == EXCEPTION_SINGLE_STEP) {
		return DBG_CONTINUE;
	}

	printf("Exception %08X at address %p\n", exceptCode, exceptAddr);
	return DBG_EXCEPTION_NOT_HANDLED;

}

#define ClearAndReturn(b) \
	std::getline(std::cin, command); \
	return (b)

void* ToAddress(const std::string& str) {
	DWORD addressValue = 0lu;
	auto addrStr = trim(str);
	if (!std::regex_match(addrStr, std::regex("^0[xX][0-9a-fA-F]{1,8}$"))) {
		printf("Error - addr format: %s does not match format: 0[xX][0-9a-fA-F]{1,8}\n", addrStr.c_str());
		return nullptr;
	}
	for (auto ch : addrStr) {
		addressValue <<= 4;
		addressValue += to_int(ch);
	}
	if (addressValue) {
		return reinterpret_cast<void*>(addressValue);

	}
	printf("Error - addr is nullptr\n");
	return nullptr;
}

char ToPrintableChar(const char c) {
	if (32 < c && c <= 126) {
		return c;
	}
	return '.';
}

bool RubbishDebugger::HandleCommand() {
	printf(">> ");
	std::string command;
	std::cin >> command;
	if (command == "help") {
		printf("asm       - show asm\n");
		printf("b [addr]  - add breakpoint at addr\n");
		printf("go        - run to next breakpoint\n");
		printf("help      - print help\n");
		printf("k         - kill process\n");
		printf("m [addr]  - show memory at addr\n");
		printf("rb [addr] - remove breakpoint at addr\n");
		printf("reg       - show regs\n");
		printf("s         - single step\n");
		printf("v [count] - show stack variables and parameters\n");
		ClearAndReturn(false);
	}
	if (command == "asm") {
		ShowAssembly();
		ClearAndReturn(false);
	}
	if (command == "b") {
		std::string rest;
		std::getline(std::cin, rest);
		if (const auto address = ToAddress(rest); address) {
			printf("Breakpoint added: 0x%p\n", address);
			SetBreakpoint(address);
		}

		return false;
	}
	if (command == "go") {
		ClearAndReturn(true);
	}
	if (command == "k") {
		TerminateProcess(_info.hProcess, 0);
		ClearAndReturn(true);
	}
	if (command == "m") {
		std::string rest;
		std::getline(std::cin, rest);
		if (const auto address = ToAddress(rest); address) {
			ShowMemory(address);
		}
		return false;
	}

	if (command == "rb") {
		std::string rest;
		std::getline(std::cin, rest);
		if (const auto address = ToAddress(rest); address) {
			printf("Breakpoint removed: 0x%p\n", address);
			RemoveBreakpoint(address);
		}

		return false;
	}
	if (command == "reg") {
		printf("\n");
		printf("EIP: %08X\tESP: %08X\tEBP: %08X\n", _context.Eip, _context.Esp, _context.Ebp);
		printf("EAX: %08X\tEBX: %08X\tECX: %08X\n", _context.Eax, _context.Ebx, _context.Ecx);
		printf("EDX: %08X\tESI: %08X\tEDI: %08X\n", _context.Edx, _context.Esi, _context.Edi);
		printf("CS : %04X\tDS : %04X\tES : %04X\n", _context.SegCs, _context.SegDs, _context.SegEs);
		printf("FS : %04X\tGS : %04X\tSS : %04X\n", _context.SegFs, _context.SegGs, _context.SegSs);
		ClearAndReturn(false);
	}
	if (command == "s") {
		_context.EFlags |= 0x100;
		SetThreadContext(_info.hThread, &_context);
		ClearAndReturn(true);
	}
	if (command == "v") {
		int count;
		std::cin >> count;
		ShowStack(count);
		ClearAndReturn(false);

	}

	std::cout << "Unknown command: " << command << ' ';
	std::getline(std::cin, command);
	std::cout << command << '\n';
	return false;

}

void RubbishDebugger::ShowAssembly() const {
	printf("\n");
	ZydisDecoder decoder;
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_ADDRESS_WIDTH_32);

	ZydisFormatter formatter;
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

	ZyanU32 runtime_address = _lastEip;
	ZyanUSize offset = 0;
	int lineCounter = 0;
	const ZyanUSize length = 32;
	char data[32] = {0};
	if (!Read(reinterpret_cast<void const*>(_lastEip), data, length)) {
		PrintLastErrorVa("Failed to read memory at address %08X", _lastEip);
		return;
	}
	const std::regex regAddr("0x[0-9a-fA-F]{8}");
	ZydisDecodedInstruction instruction;
	while (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&decoder, data + offset, length - offset,
		&instruction)) && lineCounter < 5) {
		lineCounter++;
		for (auto j = 0; j < instruction.length; j++) {
			printf("%02X", data[offset + j] & 0xFF);
		}
		printf("\n");
		printf("%08X  ", runtime_address);

		char buffer[256];
		ZydisFormatterFormatInstruction(&formatter, &instruction, buffer, sizeof(buffer),
		                                runtime_address);

		printf("%s %s\n", buffer, (runtime_address == _lastEip ? "<-- EIP" : ""));
		if (std::cmatch match; std::regex_search(buffer, match, std::regex("0x[0-9a-fA-F]{8}"))) {
			auto str = match.str();
			char* out;
			DWORD address = strtol(str.c_str() + 2, &out, 16);
			if (auto findResult = _pe->FindImportFunction(address); std::get<0>(findResult)) {
				std::cout << str << " = " << std::get<1>(findResult) << "\n";

			}
		}
		offset += instruction.length;
		runtime_address += instruction.length;
	}
}

void RubbishDebugger::ShowMemory(const void* address) const {
	constexpr auto MemBufferLines = 5;
	constexpr auto MemBufferSize = 16 * MemBufferLines;
	BYTE memBuffer[MemBufferSize];
	memset(memBuffer, 0, MemBufferSize);
	if (!Read(address, memBuffer, MemBufferSize)) {
		PrintLastErrorVa("Read memory at %p failed", address);
		return;
	}
	RemoveCodeBreakpoints(address, memBuffer, MemBufferSize);
	for (auto i = 0; i < MemBufferLines; i++) {
		for (auto j = 0; j < 16; j++) {
			if (j + 1 % 4 == 0) {
				printf(" ");
			}
			printf("%02X ", memBuffer[i * 16 + j] & 0xFF);

		}
		printf("\t");
		for (auto j = 0; j < 16; j++) {
			if (j % 4 == 0) {
				printf(" ");
			}
			printf("%c", ToPrintableChar(memBuffer[i * 16 + j]));
		}
		printf("\n");
	}
}

void RubbishDebugger::ShowStack(int parameterCount) const {
	const auto size = (_context.Ebp - _context.Esp) / 4 + parameterCount + 1;

	for (auto i = 0; i < size; i++) {
		auto* currentAddr = reinterpret_cast<DWORD*>(_context.Ebp + (parameterCount - i) * 4);
		DWORD memBuffer = 0;
		if (Read(currentAddr, &memBuffer, 4)) {
			printf("%p: %08X", currentAddr, memBuffer);
		} else {
			printf("%p: Error code: %08X", currentAddr, GetLastError());
		}
		const auto diff = static_cast<long>(reinterpret_cast<DWORD>(currentAddr) - _context.Ebp);
		if (diff > 0) {
			printf("\t<- arg%d\n", diff / 4);
		} else if (diff == 0) {
			printf("\t<- end of stack(ebp)\n");
		} else if (reinterpret_cast<DWORD>(currentAddr) == _context.Esp) {
			printf("\t<- top of stack(esp)\n");
		} else {
			printf("\n");
		}
	}
}

void RubbishDebugger::RemoveCodeBreakpoints(const void* address, BYTE* buffer, const int bufferSize) const {
	for (auto i = 0; i < bufferSize; i++) {
		const auto currentAddr = reinterpret_cast<DWORD>(address) + i;
		auto it = _breakpointOriginalCodes.find(reinterpret_cast<void*>(currentAddr));
		if (it == _breakpointOriginalCodes.end()) {
			continue;
		}
		buffer[i] = it->second;
	}
}
