// RubbishDebugger.cpp: 定义应用程序的入口点。
//
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <BeaEngine/BeaEngine.h>
#include <debugapi.h>
#include <unordered_map>

void DisassembleCode(DISASM& infos, char* start_offset, const int size) {
	char* end_offset = start_offset + size;
	infos.EIP = reinterpret_cast<UInt64>(start_offset);
	while (!infos.Error) {
		infos.SecurityBlock = reinterpret_cast<int>(end_offset) - infos.EIP;
		if (infos.SecurityBlock <= 0) break;
		const int len = Disasm(&infos);
		switch (infos.Error) {
		case OUT_OF_BLOCK:
			(void)printf("disasm engine is not allowed to read more memory \n");
			break;
		case UNKNOWN_OPCODE:
			(void)printf("%s\n", infos.CompleteInstr);
			infos.EIP += 1;
			infos.Error = 0;
			break;
		default:
			(void)printf("%s\n", infos.CompleteInstr);
			infos.EIP += len;
		}
	}
}

PROCESS_INFORMATION pInfo;

bool IsWin32;

bool PatchMem(void* address, void const* buffer, DWORD size) {
	return (WriteProcessMemory(pInfo.hProcess, address, buffer, size, nullptr) != FALSE);
}

bool ReadMem(void const* address, void* buffer, DWORD size) {
	return (ReadProcessMemory(pInfo.hProcess, address, buffer, size, nullptr) != FALSE);
}

int main() {
	std::ifstream file{"../debugee.exe", std::ios::binary};
	IMAGE_DOS_HEADER dosHdr;
	file.read(reinterpret_cast<char*>(&dosHdr), sizeof IMAGE_DOS_HEADER);
	file.seekg(dosHdr.e_lfanew, std::ios::beg);
	//MyNtHeaders ntHeaders;
	IMAGE_NT_HEADERS32* ntHeaders = new IMAGE_NT_HEADERS32;
	file.read(reinterpret_cast<char*>(ntHeaders), sizeof IMAGE_NT_HEADERS32);
	const auto optionalHeaderSize = ntHeaders->FileHeader.SizeOfOptionalHeader;
	IsWin32 = ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	std::cout << "parsing " << (IsWin32 ? "PE32" : "PE64") << " file\n";
	DISASM infos;
	memset(&infos, 0, sizeof(DISASM));
	auto imageBase = ntHeaders->OptionalHeader.ImageBase;
	auto entry = ntHeaders->OptionalHeader.AddressOfEntryPoint;
	auto codeSize = ntHeaders->OptionalHeader.SizeOfCode;
	STARTUPINFO startupInfo{sizeof(startupInfo)};
	std::unordered_map<DWORD, HANDLE> threads;
	auto r = CreateProcess(
		"../debugee.exe", nullptr, nullptr, nullptr, false,
		DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED,
		nullptr, nullptr, &startupInfo, &pInfo);
	printf("create process: %s\n", r ? "success" : "fail");
	char buffer;
	char INT3 = 0xCC;
	r = ReadMem(reinterpret_cast<void*>(entry + imageBase), &buffer, 1);
	printf("read: %s\n", r ? "success" : "fail");
	r = PatchMem(reinterpret_cast<void*>(entry + imageBase), &INT3, 1);
	printf("patch: %s\n\n", r ? "success" : "fail");
	DEBUG_EVENT dbgEvent;
	ResumeThread(pInfo.hThread);

	for (;;) {
		WaitForDebugEvent(&dbgEvent, INFINITE);
		if (dbgEvent.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) {
			threads.emplace(dbgEvent.dwThreadId, dbgEvent.u.CreateThread.hThread);
		}
		if (dbgEvent.dwDebugEventCode != EXCEPTION_DEBUG_EVENT) {
			ContinueDebugEvent(dbgEvent.dwProcessId, dbgEvent.dwThreadId, DBG_CONTINUE);
		}
		CONTEXT ctx;
		ctx.ContextFlags = CONTEXT_CONTROL;
		GetThreadContext(threads[dbgEvent.dwThreadId], &ctx);
		infos.VirtualAddr = ctx.Rip;
		infos.Options = MasmSyntax + PrefixedNumeral + ShowSegmentRegs;
		infos.Archi = 32;
		auto* data = new char[256];
		auto r = PatchMem(reinterpret_cast<void*>(entry + imageBase), &buffer, 1);
		printf("patch: %s\n", r ? "success" : "fail");
		r = ReadMem(reinterpret_cast<void*>(entry + imageBase), data, 128);
		printf("read: %s\n", r ? "success" : "fail");
		DisassembleCode(infos, data, 128);
		break;
	}
	return 0;
}
