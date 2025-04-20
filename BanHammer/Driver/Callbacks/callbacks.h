#pragma once

#include "../common.h"

#define DRIVER_PATH_LENGTH  256
#define SHA_256_HASH_LENGTH 32

VOID ImageLoadNotifyRoutineCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);

typedef struct _DRIVER_LIST_ENTRY {
    LIST_ENTRY      ListEntry;
    PVOID           ImageBase;
    ULONG           ImageSize;
    BOOLEAN         IsHashed;
    BOOLEAN         IsX86;
    UNICODE_STRING  FullImagePath[DRIVER_PATH_LENGTH];
    CHAR            Hash[SHA_256_HASH_LENGTH];
} DRIVER_LIST_ENTRY, * PDRIVER_LIST_ENTRY;

typedef struct _DRIVER_LIST_HEAD {
    LIST_ENTRY       list_entry;
    volatile ULONG   count;
    volatile BOOLEAN active;
    KGUARDED_MUTEX   lock;

    /* modules that need to be hashed later. */
    PIO_WORKITEM     work_item;
    LIST_ENTRY       deferred_list;
    volatile BOOLEAN deferred_complete;
    volatile LONG    can_hash_x86;
} DRIVER_LIST_HEAD, * PDRIVER_LIST_HEAD;

PVOID g_RemoteThreadInjectionRegistrationHandle;
OB_PREOP_CALLBACK_STATUS OnPreThreadHandle(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info);
NTSTATUS InitRemoteThreadInjectionPrevention();

PVOID g_HandleStripRegistrationHandle;
OB_PREOP_CALLBACK_STATUS OnPreOpenProcessHandle(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info);
NTSTATUS InitHandleStrip();