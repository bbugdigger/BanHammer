#pragma once

#include <ntddk.h>

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