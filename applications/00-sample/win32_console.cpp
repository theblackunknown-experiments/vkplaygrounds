
#include "./console.hpp"
#include "./win32_console.hpp"

#include "./charconv.hpp"

#include <cassert>
#include <cstdio>

#include <iostream>

#include <Windows.h>
#include <shellapi.h>

namespace blk
{
ConsoleHolder::ConsoleHolder(const char* title)
{
	auto success = AllocConsole();
	assert(success);
	SetConsoleTitle(TEXT(title));

	// std::cout, std::clog, std::cerr, std::cin
	FILE* dummy;
	freopen_s(&dummy, "CONIN$", "r", stdin);
	freopen_s(&dummy, "CONOUT$", "w", stdout);
	freopen_s(&dummy, "CONOUT$", "w", stderr);

	std::cout.clear();
	std::clog.clear();
	std::cerr.clear();
	std::cin.clear();

	// std::wcout, std::wclog, std::wcerr, std::wcin
	HANDLE hConOut = CreateFile(
		"CONOUT$",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	HANDLE hConIn = CreateFile(
		"CONIN$",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	SetStdHandle(STD_INPUT_HANDLE, hConIn);
	SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
	SetStdHandle(STD_ERROR_HANDLE, hConOut);
	std::wcout.clear();
	std::wclog.clear();
	std::wcerr.clear();
	std::wcin.clear();
}

ConsoleHolder::~ConsoleHolder()
{
	FreeConsole();
}

std::vector<std::string> cmdline_to_args(PWSTR pCmdLine)
{
	std::vector<std::string> args;
	int argc;
	LPWSTR* wargv = CommandLineToArgvW(pCmdLine, &argc);
	assert(wargv != nullptr);
	args.reserve(argc);
	for (int i{0}; i < argc; ++i)
		args.emplace_back(blk::utf8_encode(wargv[i]));
	LocalFree(wargv);
	return args;
}
} // namespace blk
