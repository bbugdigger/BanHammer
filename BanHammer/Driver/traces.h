#pragma once
#include <ntddk.h>
#include <ntstrsafe.h>

#include "Undefined.h"

//Win10 19045
#define PiDDBCacheTable_offset 0xD2F000
#define PiDDBLock_offset 0xC44900

ULONG timestamp = 0;

typedef struct _PiDDBCacheEntry
{
    LIST_ENTRY		List;
    UNICODE_STRING	DriverName;
    ULONG			TimeDateStamp;
    NTSTATUS		LoadStatus;
    char			_0x0028[16]; // data from the shim engine, or uninitialized memory for custom drivers
} PiDDBCacheEntry, * PPiDDBCacheEntry;

typedef struct _KLDR_DATA_TABLE_ENTRY
{
    struct _LIST_ENTRY InLoadOrderLinks;                                    //0x0
    VOID* ExceptionTable;                                                   //0x10
    ULONG ExceptionTableSize;                                               //0x18
    VOID* GpValue;                                                          //0x20
    struct _NON_PAGED_DEBUG_INFO* NonPagedDebugInfo;                        //0x28
    VOID* DllBase;                                                          //0x30
    VOID* EntryPoint;                                                       //0x38
    ULONG SizeOfImage;                                                      //0x40
    struct _UNICODE_STRING FullDllName;                                     //0x48
    struct _UNICODE_STRING BaseDllName;                                     //0x58
    ULONG Flags;                                                            //0x68
    USHORT LoadCount;                                                       //0x6c
    union
    {
        USHORT SignatureLevel : 4;                                            //0x6e
        USHORT SignatureType : 3;                                             //0x6e
        USHORT Frozen : 2;                                                    //0x6e
        USHORT HotPatch : 1;                                                  //0x6e
        USHORT Unused : 6;                                                    //0x6e
        USHORT EntireField;                                                 //0x6e
    } u1;                                                                   //0x6e
    VOID* SectionPointer;                                                   //0x70
    ULONG CheckSum;                                                         //0x78
    ULONG CoverageSectionSize;                                              //0x7c
    VOID* CoverageSection;                                                  //0x80
    VOID* LoadedImports;                                                    //0x88
    union
    {
        VOID* Spare;                                                        //0x90
        struct _KLDR_DATA_TABLE_ENTRY* NtDataTableEntry;                    //0x90
    };
    ULONG SizeOfImageNotRounded;                                            //0x98
    ULONG TimeDateStamp;                                                    //0x9c
} _KLDR_DATA_TABLE_ENTRY, * PKLDR_DATA_TABLE_ENTRY;

extern POBJECT_TYPE* IoDriverObjectType;

typedef NTSTATUS(*OB_REF_OBJ_BY_NAME)(
    PUNICODE_STRING,
    ULONG,
    PACCESS_STATE,
    ACCESS_MASK,
    POBJECT_TYPE,
    KPROCESSOR_MODE,
    PVOID,
    PVOID*
    );

__forceinline PVOID GetNtKernelExport(PCWSTR export_name)
{
    UNICODE_STRING export_string;
    RtlInitUnicodeString(&export_string, export_name);
    return MmGetSystemRoutineAddress(&export_string);
}

__forceinline PKLDR_DATA_TABLE_ENTRY get_ldr_entry(PCWSTR base_dll_name)
{
    UNICODE_STRING base_dll_name_string;
    RtlInitUnicodeString(&base_dll_name_string, base_dll_name);

    PLIST_ENTRY PsLoadedModuleList = (PLIST_ENTRY)GetNtKernelExport(L"PsLoadedModuleList");

    /* Is PsLoadedModuleList null? */
    if (!PsLoadedModuleList)
    {
        return NULL;
    }

    /* Start iterating at LIST_ENTRY.Flink */
    PKLDR_DATA_TABLE_ENTRY iter_ldr_entry = (PKLDR_DATA_TABLE_ENTRY)PsLoadedModuleList->Flink;

    /* If LIST_ENTRY.Flink = beginning, then it's the last entry */
    while ((PLIST_ENTRY)iter_ldr_entry != PsLoadedModuleList)
    {
        if (!RtlCompareUnicodeString(&iter_ldr_entry->BaseDllName, &base_dll_name_string, TRUE))
        {
            return iter_ldr_entry;
        }

        /* Move on to the next entry */
        iter_ldr_entry = (PKLDR_DATA_TABLE_ENTRY)iter_ldr_entry->InLoadOrderLinks.Flink;
    }

    return NULL;
}

NTSTATUS GetDriverDeviceObject(PCUNICODE_STRING DriverName, PFILE_OBJECT* OutFileObject) {
    
    NT_ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
    
    UNICODE_STRING devicePath;
    WCHAR deviceNameBuffer[256];

    //https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntstrsafe/nf-ntstrsafe-rtlstringcbprintfw
    RtlStringCbPrintfW(deviceNameBuffer, sizeof(deviceNameBuffer), L"\\Device\\%wZ", DriverName);
    RtlInitUnicodeString(&devicePath, deviceNameBuffer);

    PFILE_OBJECT fileObject = NULL;
    PDEVICE_OBJECT deviceObject = NULL;
    NTSTATUS status = IoGetDeviceObjectPointer(&devicePath, FILE_READ_DATA, &fileObject, &deviceObject);
    if (NT_SUCCESS(status)) {
        *OutFileObject = fileObject;
        ObDereferenceObject(deviceObject);
        return status;
    }

    DbgPrint("IoGetDeviceObjectPointer failed: 0x%X\n", status);
    return status;
}

void IsDriverBlacklisted(PUNICODE_STRING driverName) {
    PFILE_OBJECT driverDeviceObject;
    NTSTATUS status = GetDriverDeviceObject(driverName, &driverDeviceObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("GetDriverDeviceObject failed: 0x%X\n", status);
        return;
    }


}

//Drivers that communicate with user mode (via IOCTL) create at least one device object!!!
//Unloaded drivers (still in PiDDBCacheTable) wontt have active devices!?!?!
void Check_PiDDBCacheTable() {
    PVOID PiDDBLock = (PVOID)((ULONG64)get_ldr_entry(L"ntosknrl.exe") + (ULONG64)PiDDBLock_offset);
    PRTL_AVL_TABLE PiDDBCacheTable = (PRTL_AVL_TABLE)((ULONG64)get_ldr_entry(L"ntosknrl.exe") + (ULONG64)PiDDBCacheTable_offset);

    NT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
    
    if (!ExAcquireResourceExclusiveLite((PERESOURCE)PiDDBLock, TRUE))
    {
        DbgPrint("[Anti-Cheat] Failed to acquire PiDDBLock\n");
        return;
    }

    PPiDDBCacheEntry currentEntry = (PPiDDBCacheEntry)&PiDDBCacheTable->BalancedRoot;
    while (currentEntry) {
        if (currentEntry->DriverName.Buffer) {
            DbgPrint("[Check_PiDDBCacheTable] Cached Driver: %wZ (TimeDateStamp: 0x%X)\n", currentEntry->DriverName.Buffer, currentEntry->TimeDateStamp);
            IsDriverBlacklisted(&currentEntry->DriverName);
        }
        currentEntry = (PPiDDBCacheEntry)currentEntry->List.Flink;
    }

    ExReleaseResourceLite((PERESOURCE)PiDDBLock);
}