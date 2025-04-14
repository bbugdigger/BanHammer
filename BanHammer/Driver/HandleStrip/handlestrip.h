#pragma once

#include "../Undefined.h"

PVOID g_HandleStripRegistrationHandle;

OB_PREOP_CALLBACK_STATUS OnPreOpenProcessHandle(PVOID /*RegistrationContext*/, POB_PRE_OPERATION_INFORMATION Info);

NTSTATUS InitHandleStrip();