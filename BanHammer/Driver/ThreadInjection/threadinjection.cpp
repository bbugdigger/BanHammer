#include "threadinjection.h"

#include "../BanHammerCommon.h"

OB_PREOP_CALLBACK_STATUS OnPreThreadHandle(PVOID /*RegistrationContext*/, POB_PRE_OPERATION_INFORMATION Info)
{
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

NTSTATUS InitThreadInjectionPrevention()
{
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

	NTSTATUS status = ObRegisterCallbacks(&Registration, &g_ThreadInjectionRegistrationHandle);
	if (!NT_SUCCESS(status)) {
		BANHAMMER_COMMON_DBG_BREAK();
		BANHAMMER_LOG_ERROR("Failed to register thread injection prevention callbacks (status=%08X)\n", status);
		return status;
	}

	return STATUS_SUCCESS;
}
