#include <fltKernel.h>

FLT_PREOP_CALLBACK_STATUS PreOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, _Flt_CompletionContext_Outptr_ PVOID* CompletionContext);

PFLT_FILTER gFilterHandle;

const FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE, 0, PreOperation, nullptr },
    { IRP_MJ_SET_INFORMATION, 0, PreOperation, nullptr },
    // Add other operations you need to monitor
    { IRP_MJ_OPERATION_END }
};

NTSTATUS DriverUnload(FLT_FILTER_UNLOAD_FLAGS Flags);

const FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),         // Size
    FLT_REGISTRATION_VERSION,         // Version
    0,                                // Flags
    NULL,                             // Context
    Callbacks,                        // Operation callbacks
    DriverUnload,                     // MiniFilterUnload
    NULL,                             // InstanceSetup
    NULL,                             // InstanceQueryTeardown
    NULL,                             // InstanceTeardownStart
    NULL,                             // InstanceTeardownComplete
    NULL,                             // GenerateFileName
    NULL,                             // NormalizeNameComponent
    NULL                              // NormalizeContextCleanup
};

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	NTSTATUS status;
	UNREFERENCED_PARAMETER(RegistryPath);

	status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
	if (!NT_SUCCESS(status)) {
		FltUnregisterFilter(gFilterHandle);
		return status;
	}
}

NTSTATUS DriverUnload(FLT_FILTER_UNLOAD_FLAGS Flags) {
    UNREFERENCED_PARAMETER(Flags);

    if (!gFilterHandle) {
        FltUnregisterFilter(gFilterHandle);
    }
}

FLT_PREOP_CALLBACK_STATUS PreOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, _Flt_CompletionContext_Outptr_ PVOID* CompletionContext) {
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(FltObjects);

    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    NTSTATUS status;

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &nameInfo);
    if (!NT_SUCCESS(status)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    if (nameInfo->FinalComponent.Length >= 4 &&
        _wcsnicmp(nameInfo->FinalComponent.Buffer + nameInfo->FinalComponent.Length - 4, L".sys", 4) == 0) {

        if (Data->Iopb->MajorFunction == IRP_MJ_CREATE ||
            (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION &&
                Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformation)) {

            // Check if this is an authorized driver
            if (!IsAuthorizedDriver(nameInfo->Name)) {
                FltReleaseFileNameInformation(nameInfo);
                Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                return FLT_PREOP_COMPLETE;
            }
        }
    }

    FltReleaseFileNameInformation(nameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}