#include "handlestrip.h"

#include "../common.h"
#include "../log.h"

#include "../BanHammerCommon.h"

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
		BANHAMMER_LOG_ERROR("Failed to register callbacks (status=%08X)\n", status);
		return status;
	}

    return STATUS_SUCCESS;
}
