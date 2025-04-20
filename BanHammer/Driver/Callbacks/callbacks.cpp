#include "callbacks.h"

NTSTATUS InitialiseDriverList() {
    PAGED_CODE();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    SYSTEM_MODULES modules = { 0 };
    PDRIVER_LIST_ENTRY entry = NULL;
    PRTL_MODULE_EXTENDED_INFO module_entry = NULL;
    PDRIVER_LIST_HEAD head = GetDriverList();

    InterlockedExchange(&head->active, TRUE);
    InitializeListHead(&head->list_entry);
    InitializeListHead(&head->deferred_list);
    KeInitializeGuardedMutex(&head->lock);

    head->can_hash_x86 = FALSE;
    head->work_item = IoAllocateWorkItem(GetDriverDeviceObject());

    if (!head->work_item)
        return STATUS_INSUFFICIENT_RESOURCES;

    status = GetSystemModuleInformation(&modules);

    if (!NT_SUCCESS(status)) {
        BANHAMMER_LOG_ERROR("GetSystemModuleInformation failed with status %x", status);
        return status;
    }

    KeAcquireGuardedMutex(&head->lock);

    /* skip hal.dll and ntoskrnl.exe */
    for (UINT32 index = 2; index < modules.module_count; index++) {
        entry = ExAllocatePool2(
            POOL_FLAG_NON_PAGED,
            sizeof(DRIVER_LIST_ENTRY),
            POOL_TAG_DRIVER_LIST);

        if (!entry)
            continue;

        module_entry = &((PRTL_MODULE_EXTENDED_INFO)modules.address)[index];

        entry->IsHashed = TRUE;
        entry->ImageBase = module_entry->ImageBase;
        entry->ImageSize = module_entry->ImageSize;

        IntCopyMemory(entry->FullImagePath, module_entry->FullPathName, sizeof(module_entry->FullPathName));

        status = HashModule(module_entry, entry->Hash);

        if (status == STATUS_INVALID_IMAGE_WIN_32) {
            BANHAMMER_LOG_ERROR(
                "32 bit module not hashed, will hash later. %x",
                status);
            entry->IsHashed = FALSE;
            entry->IsX86 = TRUE;
            InsertHeadList(&head->deferred_list, &entry->deferred_entry);
        }
        else if (!NT_SUCCESS(status)) {
            BANHAMMER_LOG_ERROR("HashModule failed with status %x", status);
            entry->IsHashed = FALSE;
        }

        InsertHeadList(&head->list_entry, &entry->ListEntry);
    }

    KeReleaseGuardedMutex(&head->lock);
    head->active = TRUE;

    if (modules.address)
        ExFreePoolWithTag(modules.address, SYSTEM_MODULES_POOL);

    return STATUS_SUCCESS;
}

VOID FindDriverEntryByBaseAddress(PVOID ImageBase, PDRIVER_LIST_ENTRY* Entry) {
    NT_ASSERT(ImageBase != NULL);
    NT_ASSERT(Entry != NULL);

    PDRIVER_LIST_HEAD head = GetDriverList();
    PLIST_ENTRY entry = NULL;
    PDRIVER_LIST_ENTRY driver = NULL;

    KeAcquireGuardedMutex(&head->lock);
    entry = head->list_entry.Flink;

    while (entry != &head->list_entry) {
        driver = CONTAINING_RECORD(entry, DRIVER_LIST_ENTRY, ListEntry);
        if (driver->ImageBase == ImageBase) {
            *Entry = driver;
            break;
        }
        entry = entry->Flink;
    }

    KeReleaseGuardedMutex(&head->lock);
}

VOID ImageLoadNotifyRoutineCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PDRIVER_LIST_ENTRY entry = NULL;
    PDRIVER_LIST_HEAD head = GetDriverList();

    if (ImageInfo->SystemModeImage == FALSE) {
        //TODO: implement usermode modules integrity checking
        return;
    }

    FindDriverEntryByBaseAddress(ImageInfo->ImageBase, &entry);

    /* if we image exists, exit */
    if (entry)
        return;

    entry = ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(DRIVER_LIST_ENTRY),
        POOL_TAG_DRIVER_LIST);

    if (!entry)
        return;

    entry->IsHashed = TRUE;
    entry->IsX86 = FALSE;
    entry->ImageBase = ImageInfo->ImageBase;
    entry->ImageSize = ImageInfo->ImageSize;

    BANHAMMER_LOG_INFO("New system image ansi: %ls", entry->FullImagePath->Buffer);

    status = HashModule(&module, &entry->Hash);

    if (status == STATUS_INVALID_IMAGE_WIN_32) {
        BANHAMMER_LOG_ERROR("32 bit module not hashed, will hash later. %x", status);
        entry->IsX86 = TRUE;
        entry->IsHashed = FALSE;
    }
    else if (!NT_SUCCESS(status)) {
        BANHAMMER_LOG_ERROR("HashModule failed with status %x", status);
        entry->IsHashed = FALSE;
    }

    KeAcquireGuardedMutex(&head->lock);
    InsertHeadList(&head->list_entry, &entry->ListEntry);
    KeReleaseGuardedMutex(&head->lock);
}

OB_PREOP_CALLBACK_STATUS OnPreThreadHandle(PVOID /*RegistrationContext*/, POB_PRE_OPERATION_INFORMATION Info) {
    if (Info->KernelHandle)
        return OB_PREOP_SUCCESS;

    PEPROCESS TargetProcess = (PEPROCESS)Info->Object;
    PEPROCESS CallerProcess = PsGetCurrentProcess();
    LPCSTR TargetName = PsGetProcessImageFileName(TargetProcess);
    LPCSTR CallerName = PsGetProcessImageFileName(CallerProcess);

    if (_stricmp(TargetName, g_Game)) {
        Info->Parameters->CreateHandleInformation.DesiredAccess &= ~(
            PROCESS_CREATE_THREAD |
            PROCESS_SUSPEND_RESUME |
            PROCESS_TERMINATE |
            PROCESS_VM_READ |
            PROCESS_VM_WRITE |
            PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION
            );
    }

    if ((Info->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_CREATE_THREAD) &&
        !(Info->Parameters->CreateHandleInformation.DesiredAccess & PROCESS_CREATE_THREAD)) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("Blocked thread creation access from %s to %s\n", CallerName, TargetName);
    }

    return OB_PREOP_SUCCESS;
}

NTSTATUS InitRemoteThreadInjectionPrevention() {
    // Register Callback
    OB_OPERATION_REGISTRATION Operations[] = {
        {
            PsThreadType, // object type that triggers the callback routine
            OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
            OnPreThreadHandle, // The system calls this routine before the requested operation occurs. 
            nullptr // The system calls this routine after the requested operation occurs.
        }
    };
    OB_CALLBACK_REGISTRATION Registration = {
        OB_FLT_REGISTRATION_VERSION,
        1, // operation count
        RTL_CONSTANT_STRING(L"98765.4321"), // altitude
        nullptr, // context
        Operations
    };

    NTSTATUS status = ObRegisterCallbacks(&Registration, &g_RemoteThreadInjectionRegistrationHandle);
    if (!NT_SUCCESS(status)) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("Failed to register thread injection prevention callbacks (status=%08X)\n", status);
        return status;
    }

    return STATUS_SUCCESS;
}

OB_PREOP_CALLBACK_STATUS OnPreOpenProcessHandle(PVOID, POB_PRE_OPERATION_INFORMATION Info) {
    if (Info->KernelHandle)
        return OB_PREOP_SUCCESS;

    auto process = (PEPROCESS)Info->Object; //A pointer to the process or thread object that is the target of the handle operation
    LPCSTR processName = PsGetProcessImageFileName(process);
    if (_stricmp(processName, g_Game)) {

        PEPROCESS CallerProcess = PsGetCurrentProcess();
        LPCSTR callerName = PsGetProcessImageFileName(CallerProcess);
        if (_stricmp(callerName, "lsass.exe") == 0) {
            // we might wana be carefull about lsass case since its maybe protected by PatchGuard ?!?!
            BANHAMMER_COMMON_DBG_BREAK();
            Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
            Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
            return OB_PREOP_SUCCESS;
        }

        // if this is a handle to the game, strip access flags
        Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
        Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
        Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
        Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_QUERY_INFORMATION;
        Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_OPERATION;
    }

    return OB_PREOP_SUCCESS;
}

NTSTATUS InitHandleStrip() {
    // Register Callback
    OB_OPERATION_REGISTRATION Operations[] = {
        {
            PsProcessType, // object type that triggers the callback routine
            OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
            OnPreOpenProcessHandle, // The system calls this routine before the requested operation occurs. 
            nullptr // The system calls this routine after the requested operation occurs.
        }
    };
    OB_CALLBACK_REGISTRATION Registration = {
        OB_FLT_REGISTRATION_VERSION,
        1, // operation count
        RTL_CONSTANT_STRING(L"12345.6789"), // altitude
        nullptr, // context
        Operations
    };

    NTSTATUS status = ObRegisterCallbacks(&Registration, &g_HandleStripRegistrationHandle);
    if (!NT_SUCCESS(status)) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("Failed to register handle strip callbacks (status=%08X)\n", status);
        return status;
    }

    return STATUS_SUCCESS;
}
