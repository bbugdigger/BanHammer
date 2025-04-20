#pragma once

#include "log.h"

#define SYSTEM_MODULES_POOL            'halb'
#define POOL_TAG_DRIVER_LIST           'drvl'

/// Sets a break point that works only when a debugger is present
#if !defined(BANHAMMER_COMMON_DBG_BREAK)
#define BANHAMMER_COMMON_DBG_BREAK() \
  if (KD_DEBUGGER_NOT_PRESENT) {         \
  } else {                               \
    __debugbreak();                      \
  }                                      \
  reinterpret_cast<void*>(0)
#endif

/// Checks if a system is x64
/// @return true if a system is x64
constexpr bool IsX64() {
#if defined(_AMD64_)
    return true;
#else
    return false;
#endif
}

/// Checks if the project is compiled as Release
/// @return true if the project is compiled as Release
constexpr bool IsReleaseBuild() {
#if defined(DBG)
    return false;
#else
    return true;
#endif
}

typedef struct _RTL_MODULE_EXTENDED_INFO {
    PVOID  ImageBase;
    ULONG  ImageSize;
    USHORT FileNameOffset;
    CHAR   FullPathName[0x100];
} RTL_MODULE_EXTENDED_INFO, * PRTL_MODULE_EXTENDED_INFO;