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

VOID ImageLoadNotifyRoutineCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo)
{
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
