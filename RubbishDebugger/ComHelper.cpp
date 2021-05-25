#include "ComHelper.h"
#include <objbase.h>
#include <iostream>
ComHelper* ComHelper::Instance = nullptr;

ComHelper::ComHelper() {
	LastHResult = CoInitialize(nullptr);
	if (FAILED(LastHResult)) {
		printf("CoInitialize failed. HRESULT: %08X", LastHResult);
	}
}

ComHelper::~ComHelper() {
	CoUninitialize();
}

void* ComHelper::CreateSingleObject(const IID& rclsid, const IID& riid) {
	void* objPtr;
	LastHResult = CoCreateInstance(rclsid, nullptr, CLSCTX_INPROC_SERVER, riid,
	                               &objPtr);
	if (FAILED(LastHResult)) {
		return nullptr;
	}
	return objPtr;
}

bool ComHelper::Failed() const {
	return FAILED(LastHResult);
}
