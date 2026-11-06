#pragma once

#include <VReg/registry.h>

#include "core/buffer.h"

#include <Windows.h>

/// @file
/// @brief Reads over a @ref VRegRegistry that size and fill owned storage
///  themselves, retrying with a larger buffer while the value grows between
///  the sizing call and the read. The storage is allocated with the
///  @p pAllocContext given and stays the caller's to free.

#ifdef __cplusplus
extern "C" {
#endif

VREGDEF LSTATUS VReg__QueryValueOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_z_ LPCWSTR lpValueName,
    _In_opt_ void* pAllocContext,
    _Out_opt_ LPDWORD lpType,
    _Inout_ VReg__Buffer* pData
);

VREGDEF LSTATUS VReg__GetValueOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_z_ LPCWSTR lpSubKey,
    _In_opt_z_ LPCWSTR lpValue,
    _In_ DWORD dwFlags,
    _In_opt_ void* pAllocContext,
    _Out_opt_ LPDWORD pdwType,
    _Inout_ VReg__Buffer* pData
);

VREGDEF LSTATUS VReg__EnumKeyOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pName
);

/// With a null @p pData only the name and type are read.
VREGDEF LSTATUS VReg__EnumValueOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pName,
    _Out_opt_ LPDWORD lpType,
    _Inout_opt_ VReg__Buffer* pData
);

#ifdef __cplusplus
} // extern "C"
#endif
