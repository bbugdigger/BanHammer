#pragma once

#include "../common.h"

VOID ImageLoadNotifyRoutineCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);

PVOID g_RemoteThreadInjectionRegistrationHandle;
OB_PREOP_CALLBACK_STATUS OnPreThreadHandle(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info);
NTSTATUS InitRemoteThreadInjectionPrevention();

PVOID g_HandleStripRegistrationHandle;
OB_PREOP_CALLBACK_STATUS OnPreOpenProcessHandle(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info);
NTSTATUS InitHandleStrip();

NTSTATUS InitTimerObject(PTIMER_OBJECT Timer);