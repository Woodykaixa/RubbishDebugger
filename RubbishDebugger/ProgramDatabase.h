#pragma once
#include <iostream>
#include <dia2.h>
#include <string>
#include<vector>

struct Symbol {
	std::wstring_view Name;
	DWORD Rva;
	DWORD Segment;
	DWORD Offset;
};

struct Compiland {
	std::wstring_view Name;
	std::vector<std::wstring_view> SourceFiles;
};

class ProgramDatabase {
public:
	explicit ProgramDatabase(std::wstring& filename);
	~ProgramDatabase();
private:
	IDiaDataSource* dataSource;
	IDiaSession* session;
	IDiaSymbol* global;
	std::vector<Symbol> _globSymbols;
	std::vector<Compiland> _compilands;

	const wchar_t* _filenameNoExt;

	bool DumpAllPublics();
	bool DumpAllLines(IDiaSession* pSession, IDiaSymbol* pGlobal);

	bool DumpAllLines(IDiaSession* pSession, DWORD dwRVA, DWORD dwRange);
	bool DumpAllFiles();
	HRESULT GetTable(IDiaSession* pSession, REFIID iid, void** ppUnk);
};


// Tags returned by Dia
const wchar_t* const rgTags[] =
{
	L"(SymTagNull)", // SymTagNull
	L"Executable (Global)", // SymTagExe
	L"Compiland", // SymTagCompiland
	L"CompilandDetails", // SymTagCompilandDetails
	L"CompilandEnv", // SymTagCompilandEnv
	L"Function", // SymTagFunction
	L"Block", // SymTagBlock
	L"Data", // SymTagData
	L"Annotation", // SymTagAnnotation
	L"Label", // SymTagLabel
	L"PublicSymbol", // SymTagPublicSymbol
	L"UserDefinedType", // SymTagUDT
	L"Enum", // SymTagEnum
	L"FunctionType", // SymTagFunctionType
	L"PointerType", // SymTagPointerType
	L"ArrayType", // SymTagArrayType
	L"BaseType", // SymTagBaseType
	L"Typedef", // SymTagTypedef
	L"BaseClass", // SymTagBaseClass
	L"Friend", // SymTagFriend
	L"FunctionArgType", // SymTagFunctionArgType
	L"FuncDebugStart", // SymTagFuncDebugStart
	L"FuncDebugEnd", // SymTagFuncDebugEnd
	L"UsingNamespace", // SymTagUsingNamespace
	L"VTableShape", // SymTagVTableShape
	L"VTable", // SymTagVTable
	L"Custom", // SymTagCustom
	L"Thunk", // SymTagThunk
	L"CustomType", // SymTagCustomType
	L"ManagedType", // SymTagManagedType
	L"Dimension", // SymTagDimension
	L"CallSite", // SymTagCallSite
	L"InlineSite", // SymTagInlineSite
	L"BaseInterface", // SymTagBaseInterface
	L"VectorType", // SymTagVectorType
	L"MatrixType", // SymTagMatrixType
	L"HLSLType", // SymTagHLSLType
	L"Caller", // SymTagCaller,
	L"Callee", // SymTagCallee,
	L"Export", // SymTagExport,
	L"HeapAllocationSite", // SymTagHeapAllocationSite
	L"CoffGroup", // SymTagCoffGroup
	L"Inlinee", // SymTagInlinee
};
