#pragma once

#include "pch.h"

#include "dig_sig.h"

void NonSignedDll() {
	uintptr_t pPeb = __readgsqword(PEBOffset);
	pPeb = *reinterpret_cast<decltype(pPeb)*>(pPeb + LdrOffset);
	PLDR_DATA_TABLE_ENTRY pModuleList = *reinterpret_cast<PLDR_DATA_TABLE_ENTRY*>(pPeb + ListOffset);
	while (pModuleList->DllBase) {
		if (VerifyPESignature(pModuleList->FullDllName.Buffer)) {
			wprintf_s(L"\t[+] %s Dll Signature Verified\n", pModuleList->BaseDllName.Buffer);
		}
		else {
			wprintf_s(L"\t[+] %s Dll Is Unsigned\n", pModuleList->BaseDllName.Buffer);
		}

		pModuleList = reinterpret_cast<PLDR_DATA_TABLE_ENTRY>(pModuleList->InLoadOrderLinks.Flink);
	}
}