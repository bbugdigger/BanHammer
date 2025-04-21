#pragma once

#define IOCTL_BLOCKLIST	CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#include "log.h"

#define PROCESS_TERMINATE			0x0001
#define PROCESS_VM_READ				0x0010
#define PROCESS_VM_WRITE			0x0020
#define PROCESS_QUERY_INFORMATION 	0x0400
#define PROCESS_VM_OPERATION  		0x0008
#define PROCESS_CREATE_THREAD		0x0002
#define PROCESS_SUSPEND_RESUME 		0x0800

#define SYSTEM_MODULES_POOL            'halb'
#define POOL_TAG_DRIVER_LIST           'drvl'

#define DRIVER_PATH_LENGTH  256
#define MAX_MODULE_PATH  256
#define SHA_256_HASH_LENGTH 32

#define ABSOLUTE(wait) (wait)
#define RELATIVE(wait) (-(wait))
#define NANOSECONDS(nanos) (((signed __int64)(nanos)) / 100L)
#define MICROSECONDS(micros) (((signed __int64)(micros)) * NANOSECONDS(1000L))
#define MILLISECONDS(milli) (((signed __int64)(milli)) * MICROSECONDS(1000L))
#define SECONDS(seconds) (((signed __int64)(seconds)) * MILLISECONDS(1000L))

#define REPEAT_TIME_10_SEC 10000

#if !defined(BANHAMMER_COMMON_DBG_BREAK)
#define BANHAMMER_COMMON_DBG_BREAK() \
  if (KD_DEBUGGER_NOT_PRESENT) {         \
  } else {                               \
    __debugbreak();                      \
  }                                      \
  reinterpret_cast<void*>(0)
#endif

constexpr bool IsX64() {
#if defined(_AMD64_)
    return true;
#else
    return false;
#endif
}

constexpr bool IsReleaseBuild() {
#if defined(DBG)
    return false;
#else
    return true;
#endif
}

typedef struct _DRIVER_LIST_ENTRY {
    LIST_ENTRY      ListEntry;
    PVOID           ImageBase;
    ULONG           Size;
    BOOLEAN         IsHashed;
    BOOLEAN         IsX86;
    UNICODE_STRING  FullPath[DRIVER_PATH_LENGTH];
    CHAR            Hash[SHA_256_HASH_LENGTH];
} DRIVER_LIST_ENTRY, * PDRIVER_LIST_ENTRY;

typedef struct _DRIVER_LIST_HEAD {
    LIST_ENTRY       list_entry;
    volatile ULONG   count;
    volatile BOOLEAN active;
    KGUARDED_MUTEX   Mutex;

    /* modules that need to be hashed later. */
    PIO_WORKITEM     WorkItem;
    LIST_ENTRY       deferred_list;
    volatile BOOLEAN deferred_complete;
    volatile LONG    can_hash_x86;
} DRIVER_LIST_HEAD, * PDRIVER_LIST_HEAD;

typedef struct _RTL_MODULE_EXTENDED_INFO {
    PVOID  ImageBase;
    ULONG  Size;
    USHORT FileNameOffset;
    PWCH   FullPathName[DRIVER_PATH_LENGTH];
} RTL_MODULE_EXTENDED_INFO, * PRTL_MODULE_EXTENDED_INFO;

extern "C" {
    LPCSTR NTAPI PsGetProcessImageFileName(PEPROCESS Process);
}

typedef struct _MODULE_INFORMATION {
    PVOID  BaseAddress;
    UINT32 Size;
    CHAR   FullPath[MAX_MODULE_PATH];
    CHAR   Name[MAX_MODULE_PATH];
    CHAR   ModuleHash[SHA_256_HASH_LENGTH];
} MODULE_INFORMATION, * PMODULE_INFORMATION;

typedef struct _TIMER_OBJECT {
    /*
     * state = 1: callback in progress
     * state = 0: no callback in progress (i.e safe to free and unregister)
     */
    volatile LONG State;

    KTIMER       Timer;
    KDPC         Dpc;
    PIO_WORKITEM WorkItem;
} TIMER_OBJECT, * PTIMER_OBJECT;

typedef struct _DRIVER_CONFIG {
    UNICODE_STRING DriverName;
    PUNICODE_STRING DeviceName;
    PUNICODE_STRING DeviceSymLink;
    UNICODE_STRING DriverPath;
    PDRIVER_OBJECT DriverObject;
    PDEVICE_OBJECT DeviceObject;
    KGUARDED_MUTEX Mutex;
    TIMER_OBJECT IntegrityCheckTimer;
    MODULE_INFORMATION UsermodeModuleInfo;
    DRIVER_LIST_HEAD DriverList;
} DRIVER_CONFIG, * PDRIVER_CONFIG;

PDRIVER_CONFIG g_DriverConfig = NULL;

PDEVICE_OBJECT GetDriverDeviceObject() {
    PAGED_CODE();
    return g_DriverConfig->DeviceObject;
}

PDRIVER_LIST_HEAD GetDriverList() {
    PAGED_CODE();
    return &g_DriverConfig->DriverList;
}

PUNICODE_STRING GetDriverDeviceName() {
    PAGED_CODE();
    return g_DriverConfig->DeviceName;
}

PUNICODE_STRING GetDriverSymbolicLink() {
    PAGED_CODE();
    return g_DriverConfig->DeviceSymLink;
}

PUNICODE_STRING GetDriverPath()
{
    PAGED_CODE();
    return &g_DriverConfig->DriverPath;
}

PMODULE_INFORMATION GetModuleInformation()
{
    PAGED_CODE();
    return &g_DriverConfig->UsermodeModuleInfo;
}

NTSTATUS InitDriverList() {
    PAGED_CODE();

    //TODO: finish this function

    return STATUS_SUCCESS;
}