#pragma once

#include <VReg/api.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Whether data of @p dwType is UTF-16 text: @ref REG_SZ, @ref REG_EXPAND_SZ
/// or @ref REG_MULTI_SZ.
VREGDEF BOOL VReg__IsStringType(DWORD dwType);

#ifdef __cplusplus
} // extern "C"
#endif
