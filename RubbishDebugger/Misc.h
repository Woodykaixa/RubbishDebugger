#pragma once
#include <string_view>
#include <unordered_map>
#define Bool2String(b) ((b) ? "true" : "false")
#define PrintLastError(msg) printf(msg ": %08X\n", GetLastError())
#define PrintLastErrorVa(msg, ...) printf(msg ": %08X\n", __VA_ARGS__, GetLastError())
#ifndef NDEBUG
#define DebugCall(func, ...) 	func(__VA_ARGS__);
#else
#define DebugCall(func, ...) 
#endif

inline auto trim(const std::string& string) noexcept {
	auto const first = string.find_first_not_of(' ');
	if (first != std::string::npos) {
		auto const last = string.find_last_not_of(' ');
		return string.substr(first, last - first + 1);
	}
	return string;
}

inline auto to_int(const char c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	}
	if ('a' <= c && c <= 'f') {
		return c - 'a' + 10;
	}
	if ('A' <= c && c <= 'F') {
		return c - 'A' + 10;
	}
	return 0;
}
