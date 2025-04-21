#include "blocklist.h"

NTSTATUS IsProcRunning(const wchar_t* processName) {
    NTSTATUS status = STATUS_NOT_FOUND;
    ULONG bufferSize = 0;
    PVOID buffer = nullptr;

    if (!processName) {
        BANHAMMER_COMMON_DBG_BREAK();
        BANHAMMER_LOG_ERROR("ProcessName is NULL\n");
        return STATUS_INVALID_PARAMETER;
    }

    for (int Attempt = 0; Attempt < 5; Attempt++) {
        buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferSize, 'Proc');
        if (!buffer) {
            BANHAMMER_COMMON_DBG_BREAK();
            BANHAMMER_LOG_ERROR("Failed to allocate %lu bytes for process information\n", bufferSize);
            bufferSize >>= 1;
            continue;
        }

        status = ZwQuerySystemInformation(SystemProcessInformation, buffer, bufferSize, &bufferSize);

        if (NT_SUCCESS(status)) {
            break;
        }
        else if (status == STATUS_INFO_LENGTH_MISMATCH) {
            ExFreePoolWithTag(buffer, 'Proc');
            buffer = NULL;
            bufferSize += 16 * 1024;
        }
        else {
            ExFreePoolWithTag(buffer, 'Proc');
            BANHAMMER_COMMON_DBG_BREAK();
            BANHAMMER_LOG_ERROR("ZwQuerySystemInformation failed: 0x%X\n", status);
            return status;
        }
    }

    PSYSTEM_PROCESS_INFORMATION processInfo = (PSYSTEM_PROCESS_INFORMATION)buffer;
    while (processInfo->NextEntryOffset != 0) {
        if (processInfo->Name.Buffer != NULL && _wcsicmp(processInfo->Name.Buffer, processName) == 0) {
            status = STATUS_SUCCESS;
            break;
        }
        processInfo = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)processInfo + processInfo->NextEntryOffset);
    }

    ExFreePoolWithTag(buffer, 'Proc');
    return status;
}

void BlockList()
{
    for (int i = 0; i < sizeof(blocklist)/sizeof(blocklist[0]); i++)
    {
        if (IsProcRunning(blocklist[i])) {
            KdPrintEx((0, 0, "\t[!] Something that shouldnt is running!\n"));
            break;
        }
    }
}
