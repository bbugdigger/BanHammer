#pragma once

#include "../Undefined.h"

#include "../common.h"
#include "../log.h"

PVOID g_ThreadInjectionRegistrationHandle;

OB_PREOP_CALLBACK_STATUS OnPreThreadHandle(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info);

NTSTATUS InitThreadInjectionPrevention();