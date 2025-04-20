#include "Blocklist/blocklist.h"

#include "Callbacks/callbacks.h"

void BanHammerUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS BanHammerCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS BanHammerClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS BanHammerDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS NotifyRoutines();

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    PAGED_CODE()

    static const wchar_t kLogFilePath[] = L"\\SystemRoot\\BanHammer.log";
    static const auto kLogLevel = (IsReleaseBuild()) ? kLogPutLevelInfo | kLogOptDisableFunctionName
                                                    : kLogPutLevelDebug | kLogOptDisableFunctionName;

    BANHAMMER_COMMON_DBG_BREAK();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    // Initialize log functions
    bool need_reinitialization = false;
    status = LogInitialization(kLogLevel, kLogFilePath);
    if (status == STATUS_REINITIALIZATION_NEEDED) {
        need_reinitialization = true;
    }
    else if (!NT_SUCCESS(status)) {
        return status;
    }

    // Register re-initialization for the log functions if needed
    if (need_reinitialization) {
        LogRegisterReinitialization(DriverObject);
    }

    PDEVICE_OBJECT deviceObject = nullptr;
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BanHammer");
    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\BanHammer");
    status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);
    if (!NT_SUCCESS(status)) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("failed to create device (0x%08X)\n", status);
    }
    deviceObject->Flags |= DO_DIRECT_IO;

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("failed to create sym link (0x%08X)\n", status);
    }

    status = NotifyRoutines();
    if (!NT_SUCCESS(status)) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("failed to setup NotifyRoutines (0x%08X)\n", status);
    }

    DriverObject->DriverUnload = BanHammerUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = BanHammerCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = BanHammerClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = BanHammerDeviceControl;

    return status;
}

void BanHammerUnload(PDRIVER_OBJECT driverObject) {
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BanHammer");
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(driverObject->DeviceObject);
}

NTSTATUS BanHammerCreate(PDEVICE_OBJECT, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, 0);
    return STATUS_SUCCESS;
}

NTSTATUS BanHammerClose(PDEVICE_OBJECT, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, 0);
    return STATUS_SUCCESS;
}

NTSTATUS BanHammerDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
    PAGED_CODE();

    auto stack = IoGetCurrentIrpStackLocation(Irp);
    auto status = STATUS_SUCCESS;
    
    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_BLOCKLIST:
        {
            BlockList();
        }
    }

    return status;
}

NTSTATUS NotifyRoutines() {
    PAGED_CODE();

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    BANHAMMER_LOG_INFO("Enabling driver wide callback and notify routines.");

    status = PsSetLoadImageNotifyRoutine(ImageLoadNotifyRoutineCallback);

    if (!NT_SUCCESS(status)) {
        BANHAMMER_LOG_ERROR("PsSetLoadImageNotifyRoutine failed with status %x", status);
        return status;
    }

    BANHAMMER_LOG_INFO("Successfully enabled driver wide callback and notify routines.");
    return status;
}