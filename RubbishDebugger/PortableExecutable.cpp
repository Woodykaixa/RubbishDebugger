#include "PortableExecutable.h"
#include <exception>
#include "Misc.h"

PortableExecutable::PortableExecutable(const char* filename) {
	_file = nullptr;
	fopen_s(&_file, filename, "rb");
	if (_file == nullptr) {
		throw std::exception("Cannot open pe file");
	}
	fread(&DosHeader, sizeof IMAGE_DOS_HEADER, 1, _file);
	if (DosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
		throw std::exception("Invalid Dos header");
	}
	fseek(_file, DosHeader.e_lfanew, SEEK_SET);
	fread(&NtHeaders, sizeof IMAGE_NT_HEADERS, 1, _file);
	if (NtHeaders.Signature != IMAGE_NT_SIGNATURE) {
		throw std::exception("Invalid NT headers");
	}
	ImageBase = reinterpret_cast<void*>(NtHeaders.OptionalHeader.ImageBase);
	EntryPoint = reinterpret_cast<void*>(NtHeaders.OptionalHeader.AddressOfEntryPoint
		+ NtHeaders.OptionalHeader.ImageBase);
	Is32Bit = NtHeaders.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	_sections.resize(NtHeaders.FileHeader.NumberOfSections);
	PdbPath = nullptr;
	fread(&_sections[0], sizeof IMAGE_SECTION_HEADER, _sections.size(), _file);

	// imports
	auto const& importTable = NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (importTable.Size) {
		fseek(_file, static_cast<long>(VirtualToRaw(importTable.VirtualAddress)), SEEK_SET);

		for (;;) {
			IMAGE_IMPORT_DESCRIPTOR import_desc;
			fread(&import_desc, sizeof(IMAGE_IMPORT_DESCRIPTOR), 1, _file);

			if (!import_desc.Characteristics) {
				break;
			}

			_imports.emplace_back().Desc = import_desc;
		}
	}

	for (auto& current_import : _imports) {
		constexpr auto Size = 0x100;
		char name_buf[Size];
		fseek(_file, static_cast<long>(VirtualToRaw(current_import.Desc.Name)), SEEK_SET);
		fgets(name_buf, Size, _file);

		current_import.Name = name_buf;

		// thunks
		fseek(_file, static_cast<long>(VirtualToRaw(current_import.Desc.FirstThunk)), SEEK_SET);

		for (;;) {
			IMAGE_THUNK_DATA thunk_data;
			fread(&thunk_data, sizeof(IMAGE_THUNK_DATA), 1, _file);

			if (!thunk_data.u1.AddressOfData) {
				break;
			}

			current_import.Thunks.emplace_back().ThunkData = thunk_data;
		}

		auto thunk_addr = reinterpret_cast<IMAGE_THUNK_DATA*>(current_import.Desc.FirstThunk);
		for (auto& thunk : current_import.Thunks) {
			thunk.Address = reinterpret_cast<DWORD>(thunk_addr++);
			thunk.IsOrdinal = IMAGE_SNAP_BY_ORDINAL(thunk.ThunkData.u1.Ordinal);

			if (thunk.IsOrdinal) {
				thunk.Ordinal = IMAGE_ORDINAL(thunk.ThunkData.u1.Ordinal);
			} else {
				fseek(_file, static_cast<long>(VirtualToRaw(thunk.ThunkData.u1.AddressOfData)), SEEK_SET);
				fread(&thunk.Word, 2, 1, _file);
				fgets(name_buf, Size, _file);
				thunk.Name = name_buf;
			}
		}
	}

	// pdb
	const auto& debug = NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
	if (debug.Size) {
		fseek(_file, static_cast<long>(VirtualToRaw(debug.VirtualAddress)), SEEK_SET);
		IMAGE_DEBUG_DIRECTORY debugDir;
		fread(&debugDir, sizeof IMAGE_DEBUG_DIRECTORY, 1, _file);
		if (debugDir.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
			fseek(_file, VirtualToRaw(debugDir.AddressOfRawData + 0x18), SEEK_SET);
			PdbPath = new char[debugDir.SizeOfData - 0x18];
			fread(const_cast<char*>(PdbPath), 1, debugDir.SizeOfData - 0x18, _file);
		}
	}

	// DebugCall(DebugPrintInfo);
}

PortableExecutable::~PortableExecutable() {
	if (_file) {
		fclose(_file);
	}

}

DWORD PortableExecutable::VirtualToRaw(DWORD const dwAddress) const noexcept {
	for (auto const& section : _sections) {

		if (auto const diff = dwAddress - section.VirtualAddress;
			diff < section.SizeOfRawData) {
			return section.PointerToRawData + diff;
		}
	}

	return 0;
}

std::tuple<bool, std::string> PortableExecutable::FindImportFunction(DWORD address) {
	for (const auto& currentImport : _imports) {
		for (const auto& thunk : currentImport.Thunks) {
			if ((thunk.Address + reinterpret_cast<DWORD>(ImageBase) + 0x00001000) == address) {
				std::string name = currentImport.Name;
				name += "::";
				name += thunk.Name;
				return std::make_tuple(true, name);
			}
		}
	}

	return std::make_tuple(false, std::string());
}


#ifndef NDEBUG
void PortableExecutable::DebugPrintInfo() const {
	printf("Is PE32 program: %s\n", Bool2String(Is32Bit));
	printf("ImageBase: %p\n", ImageBase);
	printf("EntryPoint: %p\n", EntryPoint);
	printf("%d Sections:\n", _sections.size());
	for (auto& section : _sections) {
		printf("\tName: %s\n", section.Name);
		printf("\tVirtual Size: %d\n", section.Misc.VirtualSize);
		printf("\tVirtual Address: %08X\n\n", section.VirtualAddress);
	}
	printf("ImportTable:\n");
	printf("\t%d modules imported\n", _imports.size());
	for (const auto& import : _imports) {
		printf("\tName: %s\n", import.Name.data());
		for (const auto& thunk : import.Thunks) {
			printf("\t\t%s : %08X\n", thunk.Name.data(), thunk.Address);
		}
	}
}
#endif
