#pragma once

#include "../Undefined.h"

#include "../common.h"
#include "../log.h"

const wchar_t* blocklist[] = {
    L"ida.exe", L"x32dbg", L"gdb", L"x64_dbg", L"windbg", L"scyllahide", L"HxD", L"ollydbg", L"procmon64", L"ghidra", L"scyllaHide", L"binary ninja"
};

NTSTATUS IsProcRunning(const wchar_t* processName);
void BlockList();