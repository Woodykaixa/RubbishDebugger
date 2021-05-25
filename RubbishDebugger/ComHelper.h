#pragma once
#include <Windows.h>

class ComHelper {
public:
	ComHelper();
	~ComHelper();

	void* CreateSingleObject(const IID& rclsid, const IID& riid);
	[[nodiscard]] bool Failed() const;

	static ComHelper* Instance;
	HRESULT LastHResult;

};

#define CreateSingle(ctype, itype) reinterpret_cast<itype*>(ComHelper::Instance->CreateSingleObject(__uuidof(ctype), __uuidof(itype)))
