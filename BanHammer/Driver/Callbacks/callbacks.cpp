#include "callbacks.h"

VOID FindDriverEntryByBaseAddress(PVOID ImageBase, PDRIVER_LIST_ENTRY* Entry) {
    NT_ASSERT(ImageBase != NULL);
    NT_ASSERT(Entry != NULL);

    PDRIVER_LIST_HEAD head = GetDriverList();
    PLIST_ENTRY entry = NULL;
    PDRIVER_LIST_ENTRY driver = NULL;

    KeAcquireGuardedMutex(&head->Mutex);
    entry = head->list_entry.Flink;

    while (entry != &head->list_entry) {
        driver = CONTAINING_RECORD(entry, DRIVER_LIST_ENTRY, ListEntry);
        if (driver->ImageBase == ImageBase) {
            *Entry = driver;
            break;
        }
        entry = entry->Flink;
    }

    KeReleaseGuardedMutex(&head->Mutex);
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

    if (entry)
        return;

    //TODO: Finish integrity checking for kernel modules
}

OB_PREOP_CALLBACK_STATUS OnPreThreadHandle(PVOID /*RegistrationContext*/, POB_PRE_OPERATION_INFORMATION Info) {
    if (Info->KernelHandle)
        return OB_PREOP_SUCCESS;

    PEPROCESS TargetProcess = (PEPROCESS)Info->Object;
    PEPROCESS CallerProcess = PsGetCurrentProcess();
    LPCSTR TargetName = PsGetProcessImageFileName(TargetProcess);
    LPCSTR CallerName = PsGetProcessImageFileName(CallerProcess);

    if (_stricmp(TargetName, GetModuleInformation()->Name)) {
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
    if (_stricmp(processName, GetModuleInformation()->Name)) {

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

static VOID TimerObjectWorkItemRoutine(PDEVICE_OBJECT DeviceObject, PVOID Context) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PTIMER_OBJECT timer = (PTIMER_OBJECT)Context;
    PDRIVER_LIST_HEAD list = GetDriverList();
    
    UNREFERENCED_PARAMETER(DeviceObject);

    if (!ARGUMENT_PRESENT(Context))
        return;
    

}

static void TimerObjectCallbackRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2) {
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);
    NT_ASSERT(DeferredContext != NULL);

    if (!ARGUMENT_PRESENT(DeferredContext))
        return;

    PTIMER_OBJECT timer = (PTIMER_OBJECT)DeferredContext;

    /* we dont want to queue our work item if it hasnt executed */
    if (timer->State)
        return;

    InterlockedExchange(&timer->State, TRUE);
    IoQueueWorkItem(
        timer->WorkItem,
        TimerObjectWorkItemRoutine,
        BackgroundWorkQueue,
        timer);
}

NTSTATUS InitTimerObject(PTIMER_OBJECT Timer) {
    LARGE_INTEGER dueTime = { .QuadPart = -ABSOLUTE(SECONDS(5)) };

    Timer->WorkItem = IoAllocateWorkItem(GetDriverDeviceObject());

    if (!Timer->WorkItem)
        return STATUS_MEMORY_NOT_ALLOCATED;

    KeInitializeDpc(&Timer->Dpc, TimerObjectCallbackRoutine, Timer);
    KeInitializeTimer(&Timer->Timer);
    KeSetTimerEx(&Timer->Timer, dueTime, REPEAT_TIME_10_SEC, &Timer->Dpc);

    BANHAMMER_LOG_DEBUG("Successfully initialised global timer callback.");
    return STATUS_SUCCESS;
}
