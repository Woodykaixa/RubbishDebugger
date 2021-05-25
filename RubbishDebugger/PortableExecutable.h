#pragma once

#include <cstdio>
#include <string>
#include <Windows.h>
#include <vector>
#include <tuple>

struct ImportThunkData {
	IMAGE_THUNK_DATA ThunkData;
	bool IsOrdinal;

	DWORD Address;

	DWORD Ordinal;

	std::string Name;
	WORD Word;
};

struct ImportedModule {
	IMAGE_IMPORT_DESCRIPTOR Desc;
	std::string Name;

	std::vector<ImportThunkData> Thunks;
};

class PortableExecutable {
public:
	explicit PortableExecutable(const char* filename);
	~PortableExecutable();
	[[nodiscard]] DWORD VirtualToRaw(DWORD dwAddress) const noexcept;
	[[nodiscard]] std::tuple<bool, std::string> FindImportFunction(DWORD address);
	[[nodiscard]] const IMAGE_SECTION_HEADER* FindSection(const char* name);
	IMAGE_DOS_HEADER DosHeader;
	IMAGE_NT_HEADERS NtHeaders;
	void* ImageBase;
	void* EntryPoint;
	bool Is32Bit;
	const char* PdbPath;
private:
	FILE* _file;
	std::vector<IMAGE_SECTION_HEADER> _sections;
	std::vector<ImportedModule> _imports;
#ifndef NDEBUG
	void DebugPrintInfo() const;
#endif
public:
};
