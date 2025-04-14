#include "common.h"
#include "log.h"
#include "BanHammerCommon.h"

#include "Blocklist/blocklist.h"
#include "HandleStrip/handlestrip.h"

void BanHammerUnload(PDRIVER_OBJECT driverObject);
NTSTATUS BanHammerCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS BanHammerDeviceControl(PDEVICE_OBJECT, PIRP Irp);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registry_path) {
    UNREFERENCED_PARAMETER(registry_path);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    static const wchar_t kLogFilePath[] = L"\\SystemRoot\\BanHammer.log";
    static const auto kLogLevel =
        (IsReleaseBuild()) ? kLogPutLevelInfo | kLogOptDisableFunctionName
                            : kLogPutLevelDebug | kLogOptDisableFunctionName;

    BANHAMMER_COMMON_DBG_BREAK();

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
        LogRegisterReinitialization(driverObject);
    }

    PDEVICE_OBJECT deviceObject = nullptr;
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BanHammer");

    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\BanHammer");
    status = IoCreateDevice(driverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);
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

    status = InitHandleStrip();
    if (!NT_SUCCESS(status)) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("handle strip init failed (0x%08X)\n", status);
    }

    driverObject->DriverUnload = BanHammerUnload;
    driverObject->MajorFunction[IRP_MJ_CREATE] = driverObject->MajorFunction[IRP_MJ_CLOSE] = BanHammerCreateClose;
    driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = BanHammerDeviceControl;
}

void BanHammerUnload(PDRIVER_OBJECT driverObject) {
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BanHammer");
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(driverObject->DeviceObject);
}

NTSTATUS BanHammerCreateClose(PDEVICE_OBJECT, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, 0);
    return STATUS_SUCCESS;
}

NTSTATUS BanHammerDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
    auto stack = IoGetCurrentIrpStackLocation(Irp);
    auto status = STATUS_SUCCESS;
    auto len = 0;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_BLOCKLIST:
        {
            BlockList();
        }
    }
}