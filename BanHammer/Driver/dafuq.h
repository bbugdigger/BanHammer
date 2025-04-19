#include <ntifs.h>
#include <ntimage.h>
#include "Undefined.h"

//
// Byte pattern used to locate Zw functions in the kernel.
//
UCHAR ZW_PATTERN[30] = {
    0x48, 0x8B, 0xC4,                         // mov rax, rsp
    0xFA,                                     // cli
    0x48, 0x83, 0xEC, 0x10,                   // sub rsp, 10h
    0x50,                                     // push rax
    0x9C,                                     // pushfq
    0x6A, 0x10,                               // push 10h
    0x48, 0x8D, 0x05, 0xCC, 0xCC, 0xCC, 0xCC, // lea rax, KiServiceLinkage
    0x50,                                     // push rax
    0xB8, 0xCC, 0xCC, 0xCC, 0xCC,             // mov eax, <SSN>
    0xE9, 0xCC, 0xCC, 0xCC, 0xCC              // jmp KiServiceInternal
};

NTSTATUS GetSsn(
    _In_  LPCSTR  Function,
    _Out_ PUSHORT Ssn
) {
    PVOID             BaseAddr = NULL;
    HANDLE            HSection = NULL;
    ULONGLONG         ViewSize = NULL;
    NTSTATUS          Status = STATUS_UNSUCCESSFUL;
    LARGE_INTEGER     Large = { 0 };
    OBJECT_ATTRIBUTES ObjAttr = { 0 };
    UNICODE_STRING    KnownNtdll = RTL_CONSTANT_STRING(L"\\KnownDlls\\ntdll.dll");

    InitializeObjectAttributes(&ObjAttr, &KnownNtdll, OBJ_CASE_INSENSITIVE, NULL, NULL);

    //
    // Open the section for ntdll.dll from \KnownDlls
    //
    Status = ZwOpenSection(&HSection, SECTION_MAP_READ | SECTION_QUERY, &ObjAttr);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("ZwOpenSection Failed With Status 0x%08X\n", Status);
        goto CLEANUP;
    }

    //
    // Map the section into memory for reading
    //
    Status = ZwMapViewOfSection(HSection, (HANDLE)-1, &BaseAddr, 0, 0, &Large, &ViewSize, ViewUnmap, 0, PAGE_READONLY);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("ZwMapViewOfSection Failed With Status 0x%08X\n", Status);
        goto CLEANUP;
    }

    //
    // Retrieve NT headers and locate the Export Directory
    //
    ULONG_PTR ModuleBase = (ULONG_PTR)BaseAddr;
    PIMAGE_NT_HEADERS64 NtHeader = (PIMAGE_NT_HEADERS64)(((PIMAGE_DOS_HEADER)ModuleBase)->e_lfanew + ModuleBase);
    if (NtHeader->Signature != IMAGE_NT_SIGNATURE) goto CLEANUP;

    PIMAGE_EXPORT_DIRECTORY ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(ModuleBase + NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    PULONG  Names = (PULONG)(ModuleBase + ExportDirectory->AddressOfNames);
    PULONG  Functions = (PULONG)(ModuleBase + ExportDirectory->AddressOfFunctions);
    PUSHORT Ordinals = (PUSHORT)(ModuleBase + ExportDirectory->AddressOfNameOrdinals);

    //
    // Iterate over exported functions
    //
    for (ULONG I = 0; I < ExportDirectory->NumberOfNames; I++) {
        PCHAR Name = (PCHAR)(ModuleBase + Names[I]);
        PVOID Address = (PVOID)(ModuleBase + Functions[Ordinals[I]]);

        //
        // Compare the name with the requested function
        //
        if (strcmp(Name, Function) == 0) {
            PUCHAR SyscallAddr = (PUCHAR)Address;

            //
            // Validate the expected syscall stub pattern
            //
            if (SyscallAddr[0] == 0x4C && SyscallAddr[1] == 0x8B &&
                SyscallAddr[2] == 0xD1 && SyscallAddr[3] == 0xB8 &&
                SyscallAddr[6] == 0x00 && SyscallAddr[7] == 0x00
                ) {
                *Ssn = (USHORT)(SyscallAddr[4] | (SyscallAddr[5] << 8));
                Status = STATUS_SUCCESS;
                break;
            }
        }
    }

CLEANUP:
    //
    // Clean up resources
    //
    if (BaseAddr) ZwUnmapViewOfSection((HANDLE)-1, BaseAddr);
    if (HSection) ZwClose(HSection);

    return Status;
}

NTSTATUS GetModule(
    _In_  PWCHAR ModuleName,
    _Out_ PVOID* Address
) {
    NTSTATUS Status = STATUS_NOT_FOUND;

    //
    // Validate input parameters.
    //
    if (!ModuleName || !Address) {
        DbgPrint("Invalid parameters.\n");
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Acquire the resource in shared mode (allow multiple readers)
    //
    ExAcquireResourceSharedLite(PsLoadedModuleResource, TRUE);

    __try {
        //
        // Traverse the module list.
        //
        PLIST_ENTRY CurrentEntry = PsLoadedModuleList->Flink;
        while (CurrentEntry != PsLoadedModuleList) {
            PLDR_DATA_TABLE_ENTRY Entry = CONTAINING_RECORD(CurrentEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            //
            // Compare the module name (case-insensitive).
            //
            if (Entry->BaseDllName.Buffer != NULL && _wcsicmp(Entry->BaseDllName.Buffer, ModuleName) == 0) {
                *Address = Entry->DllBase;
                Status = STATUS_SUCCESS;
                break;
            }

            //
            // Move to the next entry.
            //
            CurrentEntry = CurrentEntry->Flink;
        }
    }
    __finally {
        //
        // Ensure the lock is always released
        //
        ExReleaseResourceLite(PsLoadedModuleResource);
    }

    return Status;
}

PVOID FindZwFunction(
    _In_ LPCSTR Name
) {
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PVOID    NtoskrnlBase = NULL;
    USHORT   Ssn = NULL;

    //
    // Validate the input parameter
    //
    if (!Name) {
        DbgPrint("Invalid parameters.\n");
        return NULL;
    }

    //
    // Retrieve the base address of ntoskrnl.exe
    //
    Status = GetModule(L"ntoskrnl.exe", &NtoskrnlBase);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("GetModule Failed With Status 0x%08X\n", Status);
        return NULL;
    }

    //
    // Retrieve the NT headers from the kernel base
    //
    PIMAGE_NT_HEADERS64 NtHeader = (PIMAGE_NT_HEADERS64)(((PIMAGE_DOS_HEADER)NtoskrnlBase)->e_lfanew + (ULONG_PTR)NtoskrnlBase);
    if (NtHeader->Signature != IMAGE_NT_SIGNATURE) return NULL;

    //
    // Retrieve the syscall number (SSN) of the specified function
    //
    Status = GetSsn(Name, &Ssn);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("GetSsn Failed With Status 0x%08X\n", Status);
        return NULL;
    }

    //
    // Insert the retrieved syscall number into the pattern
    //
    PUCHAR SsnBytes = (PUCHAR)&Ssn;
    ZW_PATTERN[21] = SsnBytes[0]; // Low byte of the SSN
    ZW_PATTERN[22] = SsnBytes[1]; // High byte of the SSN

    //
    // Iterate over all sections to find the .text section
    //
    PIMAGE_SECTION_HEADER SectionHeader = (PIMAGE_SECTION_HEADER)((ULONG_PTR)&NtHeader->OptionalHeader + NtHeader->FileHeader.SizeOfOptionalHeader);
    for (ULONG I = 0; I < NtHeader->FileHeader.NumberOfSections; I++) {
        if (memcmp(SectionHeader[I].Name, ".text", 5) == 0) {
            ULONG_PTR TextStart = (ULONG_PTR)NtoskrnlBase + SectionHeader[I].VirtualAddress;
            ULONG_PTR TextEnd = TextStart + SectionHeader[I].Misc.VirtualSize;
            PUCHAR    Data = (PUCHAR)TextStart;
            SIZE_T    DataSize = TextEnd - TextStart;

            //
            // Scan the .text section for the known instruction pattern
            //
            for (SIZE_T Offset = 0; Offset <= DataSize - sizeof(ZW_PATTERN); Offset++) {
                BOOLEAN Found = TRUE;

                //
                // Compare each byte of the pattern
                //
                for (SIZE_T J = 0; J < sizeof(ZW_PATTERN); J++) {
                    if (ZW_PATTERN[J] != 0xCC && Data[Offset + J] != ZW_PATTERN[J]) {
                        Found = FALSE;
                        break;
                    }
                }

                //
                // Return the address if the pattern is found
                //
                if (Found) return (PVOID)(TextStart + Offset);
            }
        }
    }

    return NULL;
}