#include "ProgramDatabase.h"
#include <exception>
#include <dia2.h>
#include "ComHelper.h"

ProgramDatabase::ProgramDatabase(std::wstring& filename) {
	FILE* file = nullptr;
	_filenameNoExt = nullptr;
	std::wcout << L"PDB: " << filename << L"\n";
	_wfopen_s(&file, filename.c_str(), L"rb");
	if (file == nullptr) {
		throw std::exception("Cannot open pdb file");
	}
	fclose(file);
	_filenameNoExt = new wchar_t[filename.length()];

	_wsplitpath_s(filename.c_str(),
	              nullptr, 0,
	              nullptr, 0,
	              const_cast<wchar_t*>(_filenameNoExt), filename.length(),
	              nullptr, 0);
	wprintf(L"%s\n", _filenameNoExt);
	dataSource = CreateSingle(DiaSource, IDiaDataSource);

	if (ComHelper::Instance->Failed()) {
		throw std::exception("CoCreateInstance failed");
	}
	auto hr = dataSource->loadDataFromPdb(filename.data());
	if (FAILED(hr)) {
		throw std::exception("loadDataFromPdb failed");
	}
	hr = dataSource->openSession(&session);
	if (FAILED(hr)) {
		throw std::exception("openSession failed");
	}
	hr = session->get_globalScope(&global);
	if (FAILED(hr)) {
		throw std::exception("get_globalScope failed");
	}
	DumpAllPublics();
	// for(auto& sym:_globSymbols) {
	// 	std::wcout << sym.Name << L'\n';
	// }
	//DumpAllLines(session, 0x00401113, 0x100);
	// DumpAllFiles();
	DumpAllLines(session, global);
}

ProgramDatabase::~ProgramDatabase() {
	global->Release();
	session->Release();
	delete[] _filenameNoExt;
}

bool ProgramDatabase::DumpAllPublics() {

	// Retrieve all the public symbols

	IDiaEnumSymbols* pEnumSymbols;

	if (FAILED(global->findChildren(SymTagPublicSymbol, NULL, nsNone, &pEnumSymbols))) {
		return false;
	}

	IDiaSymbol* pSymbol;
	ULONG celt = 0;

	while (SUCCEEDED(pEnumSymbols->Next(1, &pSymbol, &celt)) && (celt == 1)) {
		DWORD symTag;
		DWORD rva;
		DWORD seg;
		DWORD offset;
		if (pSymbol->get_symTag(&symTag) != S_OK) {
			goto releaseSymbol;
		}
		if (pSymbol->get_relativeVirtualAddress(&rva) != S_OK) {
			rva = -1;
		}
		pSymbol->get_addressSection(&seg);
		pSymbol->get_addressOffset(&offset);
		if (symTag != SymTagThunk) {
			BSTR bstrUndname;
			BSTR bstrName;
			Symbol sym;
			sym.Offset = offset;
			sym.Rva = rva;
			sym.Segment = seg;
			sym.Name = L"Unknown Symbol";
			if (pSymbol->get_name(&bstrName) == S_OK) {
				if (pSymbol->get_undecoratedName(&bstrUndname) == S_OK) {

					sym.Name = bstrUndname;
				} else {
					sym.Name = bstrName;
				}


			}
			_globSymbols.emplace_back(sym);
		}

	releaseSymbol:
		pSymbol->Release();
	}

	pEnumSymbols->Release();


	return true;
}

void PrintSourceFile(IDiaSourceFile* pSource) {
	BSTR bstrSourceName;

	if (pSource->get_fileName(&bstrSourceName) == S_OK) {
		wprintf(L"\t%s", bstrSourceName);

		SysFreeString(bstrSourceName);
	} else {
		wprintf(L"ERROR - PrintSourceFile() get_fileName");
		return;
	}
}

void PrintLines(IDiaEnumLineNumbers* pLines) {
	IDiaLineNumber* pLine;
	DWORD celt;
	DWORD dwRVA;
	DWORD dwSeg;
	DWORD dwOffset;
	DWORD dwLinenum;
	DWORD dwSrcId;
	DWORD dwLength;

	DWORD dwSrcIdLast = (DWORD)(-1);

	while (SUCCEEDED(pLines->Next(1, &pLine, &celt)) && (celt == 1)) {
		if ((pLine->get_relativeVirtualAddress(&dwRVA) == S_OK) &&
			(pLine->get_addressSection(&dwSeg) == S_OK) &&
			(pLine->get_addressOffset(&dwOffset) == S_OK) &&
			(pLine->get_lineNumber(&dwLinenum) == S_OK) &&
			(pLine->get_sourceFileId(&dwSrcId) == S_OK) &&
			(pLine->get_length(&dwLength) == S_OK)) {
			wprintf(L"\tline %u at [%08X][%04X:%08X], len = 0x%X", dwLinenum, dwRVA, dwSeg, dwOffset, dwLength);

			if (dwSrcId != dwSrcIdLast) {
				IDiaSourceFile* pSource;

				if (pLine->get_sourceFile(&pSource) == S_OK) {
					PrintSourceFile(pSource);

					dwSrcIdLast = dwSrcId;

					pSource->Release();
				}
			}

			pLine->Release();

			putwchar(L'\n');
		}
	}
}

bool ProgramDatabase::DumpAllLines(IDiaSession* pSession, IDiaSymbol* pGlobal) {
	// Retrieve and print the lines that corresponds to a specified RVA

	wprintf(L"\n\n*** LINES\n\n");

	IDiaEnumSectionContribs* pEnumSecContribs;

	if (FAILED(GetTable(pSession, __uuidof(IDiaEnumSectionContribs), (void**)&pEnumSecContribs))) {
		return false;
	}

	IDiaSectionContrib* pSecContrib;
	ULONG celt = 0;

	while (SUCCEEDED(pEnumSecContribs->Next(1, &pSecContrib, &celt)) && (celt == 1)) {

		DWORD dwRVA;
		DWORD dwLength;

		if (SUCCEEDED(pSecContrib->get_relativeVirtualAddress(&dwRVA)) &&
			SUCCEEDED(pSecContrib->get_length(&dwLength))) {

			DumpAllLines(pSession, dwRVA, dwLength);
		}

		pSecContrib->Release();
	}

	pEnumSecContribs->Release();

	putwchar(L'\n');

	return true;
}

bool ProgramDatabase::DumpAllLines(IDiaSession* pSession, DWORD dwRVA, DWORD dwRange) {
	// Retrieve and print the lines that corresponds to a specified RVA

	IDiaEnumLineNumbers* pLines;

	if (FAILED(pSession->findLinesByRVA(dwRVA, dwRange, &pLines))) {
		return false;
	}

	PrintLines(pLines);

	pLines->Release();

	putwchar(L'\n');

	return true;
}

bool ProgramDatabase::DumpAllFiles() {

	// In order to find the source files, we have to look at the image's compilands/modules

	IDiaEnumSymbols* pEnumSymbols;

	if (FAILED(global->findChildren(SymTagCompiland, NULL, nsNone, &pEnumSymbols))) {
		return false;
	}

	IDiaSymbol* pCompiland;
	ULONG celt = 0;

	while (SUCCEEDED(pEnumSymbols->Next(1, &pCompiland, &celt)) && (celt == 1)) {
		BSTR bstrName;
		Compiland compiland;
		compiland.Name = L"Unknown compiland";
		if (pCompiland->get_name(&bstrName) == S_OK) {
			wprintf(L"\nCompiland = %s\n\n", bstrName);
			compiland.Name = bstrName;
		}

		// Every compiland could contain multiple references to the source files which were used to build it
		// Retrieve all source files by compiland by passing NULL for the name of the source file

		IDiaEnumSourceFiles* pEnumSourceFiles;

		if (SUCCEEDED(session->findFile(pCompiland, NULL, nsNone, &pEnumSourceFiles))) {
			IDiaSourceFile* pSourceFile;

			while (SUCCEEDED(pEnumSourceFiles->Next(1, &pSourceFile, &celt)) && (celt == 1)) {
				BSTR filename;
				if (pSourceFile->get_fileName(&filename) == S_OK) {
					compiland.SourceFiles.emplace_back(filename);
				}
				//putwchar(L'\n');

				pSourceFile->Release();
			}

			pEnumSourceFiles->Release();
		}

		_compilands.emplace_back(compiland);

		pCompiland->Release();
	}

	pEnumSymbols->Release();

	return true;
}

HRESULT ProgramDatabase::GetTable(IDiaSession* pSession, const IID& iid, void** ppUnk) {
	IDiaEnumTables* pEnumTables;

	if (FAILED(pSession->getEnumTables(&pEnumTables))) {
		wprintf(L"ERROR - GetTable() getEnumTables\n");

		return E_FAIL;
	}

	IDiaTable* pTable;
	ULONG celt = 0;

	while (SUCCEEDED(pEnumTables->Next(1, &pTable, &celt)) && (celt == 1)) {
		// There's only one table that matches the given IID

		if (SUCCEEDED(pTable->QueryInterface(iid, (void**)ppUnk))) {
			pTable->Release();
			pEnumTables->Release();

			return S_OK;
		}

		pTable->Release();
	}

	pEnumTables->Release();

	return E_FAIL;
}
