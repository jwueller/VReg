#include <VReg/native_registry.h>

#include "core/alloc.h"
#include "core/export_table.h"

typedef struct VReg__NativeRegistry
{
    VRegRegistry object;
    void* pAllocContext;
    VRegNativeFunctions functions;
} VReg__NativeRegistry;

static VReg__NativeRegistry* VReg__NativeOf(_In_ VRegRegistry* const pThis)
{
    return (VReg__NativeRegistry*)pThis;
}

static void WINAPI VReg__NativeDestroy(_In_ VRegRegistry* const pThis)
{
    VReg__NativeRegistry* const pRegistry = VReg__NativeOf(pThis);
    VReg__Free(pRegistry->pAllocContext, pRegistry);
}

static LSTATUS WINAPI VReg__NativeRegOpenKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VReg__NativeOf(pThis)->functions.RegOpenKeyExW(
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS WINAPI VReg__NativeRegCreateKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPWSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition
)
{
    return VReg__NativeOf(pThis)->functions.RegCreateKeyExW(
        hKey,
        lpSubKey,
        Reserved,
        lpClass,
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition
    );
}

static LSTATUS WINAPI VReg__NativeRegCloseKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    return VReg__NativeOf(pThis)->functions.RegCloseKey(
        hKey
    );
}

static LSTATUS WINAPI VReg__NativeRegQueryValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VReg__NativeOf(pThis)->functions.RegQueryValueExW(
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS WINAPI VReg__NativeRegSetValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VReg__NativeOf(pThis)->functions.RegSetValueExW(
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VReg__NativeRegDeleteValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VReg__NativeOf(pThis)->functions.RegDeleteValueW(
        hKey,
        lpValueName
    );
}

static LSTATUS WINAPI VReg__NativeRegDeleteKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VReg__NativeOf(pThis)->functions.RegDeleteKeyExW(
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS WINAPI VReg__NativeRegDeleteTreeW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    return VReg__NativeOf(pThis)->functions.RegDeleteTreeW(
        hKey,
        lpSubKey
    );
}

static LSTATUS WINAPI VReg__NativeRegEnumKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPWSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
)
{
    return VReg__NativeOf(pThis)->functions.RegEnumKeyExW(
        hKey,
        dwIndex,
        lpName,
        lpcchName,
        lpReserved,
        lpClass,
        lpcchClass,
        lpftLastWriteTime
    );
}

static LSTATUS WINAPI VReg__NativeRegEnumValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPWSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
)
{
    return VReg__NativeOf(pThis)->functions.RegEnumValueW(
        hKey,
        dwIndex,
        lpValueName,
        lpcchValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS WINAPI VReg__NativeRegQueryInfoKeyW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpcSubKeys,
    _Out_opt_ const LPDWORD lpcbMaxSubKeyLen,
    _Out_opt_ const LPDWORD lpcbMaxClassLen,
    _Out_opt_ const LPDWORD lpcValues,
    _Out_opt_ const LPDWORD lpcbMaxValueNameLen,
    _Out_opt_ const LPDWORD lpcbMaxValueLen,
    _Out_opt_ const LPDWORD lpcbSecurityDescriptor,
    _Out_opt_ const PFILETIME lpftLastWriteTime
)
{
    return VReg__NativeOf(pThis)->functions.RegQueryInfoKeyW(
        hKey,
        lpClass,
        lpcchClass,
        lpReserved,
        lpcSubKeys,
        lpcbMaxSubKeyLen,
        lpcbMaxClassLen,
        lpcValues,
        lpcbMaxValueNameLen,
        lpcbMaxValueLen,
        lpcbSecurityDescriptor,
        lpftLastWriteTime
    );
}

static LSTATUS WINAPI VReg__NativeRegLoadMUIStringW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    return VReg__NativeOf(pThis)->functions.RegLoadMUIStringW(
        hKey,
        pszValue,
        pszOutBuf,
        cbOutBuf,
        pcbData,
        Flags,
        pszDirectory
    );
}

// The slot's _Post_z_ on pvData holds on every return; the analyzer does not
// carry it through the function pointer.
#pragma warning(suppress: 6054)
static LSTATUS WINAPI VReg__NativeRegGetValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hkey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValue,
    _In_ const DWORD dwFlags,
    _Out_opt_ const LPDWORD pdwType,
    _When_(
        (dwFlags & 0x7F) == RRF_RT_REG_SZ
        || (dwFlags & 0x7F) == RRF_RT_REG_EXPAND_SZ
        || (dwFlags & 0x7F) == (RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ)
        || *pdwType == REG_SZ
        || *pdwType == REG_EXPAND_SZ,
        _Post_z_
    )
    _When_(
        (dwFlags & 0x7F) == RRF_RT_REG_MULTI_SZ
        || *pdwType == REG_MULTI_SZ,
        _Post_ _NullNull_terminated_
    )
    _Out_writes_bytes_to_opt_(*pcbData, *pcbData) const PVOID pvData,
    _Inout_opt_ const LPDWORD pcbData
)
{
    return VReg__NativeOf(pThis)->functions.RegGetValueW(
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS WINAPI VReg__NativeRegNotifyChangeKeyValue(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    return VReg__NativeOf(pThis)->functions.RegNotifyChangeKeyValue(
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

static LSTATUS WINAPI VReg__NativeRegFlushKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    return VReg__NativeOf(pThis)->functions.RegFlushKey(
        hKey
    );
}

static const VRegRegistryVtbl VReg__NativeRegistryVtbl = {
    .Destroy = &VReg__NativeDestroy,
    .RegOpenKeyExW = &VReg__NativeRegOpenKeyExW,
    .RegCreateKeyExW = &VReg__NativeRegCreateKeyExW,
    .RegCloseKey = &VReg__NativeRegCloseKey,
    .RegQueryValueExW = &VReg__NativeRegQueryValueExW,
    .RegSetValueExW = &VReg__NativeRegSetValueExW,
    .RegDeleteValueW = &VReg__NativeRegDeleteValueW,
    .RegDeleteKeyExW = &VReg__NativeRegDeleteKeyExW,
    .RegDeleteTreeW = &VReg__NativeRegDeleteTreeW,
    .RegEnumKeyExW = &VReg__NativeRegEnumKeyExW,
    .RegEnumValueW = &VReg__NativeRegEnumValueW,
    .RegQueryInfoKeyW = &VReg__NativeRegQueryInfoKeyW,
    .RegLoadMUIStringW = &VReg__NativeRegLoadMUIStringW,
    .RegGetValueW = &VReg__NativeRegGetValueW,
    .RegNotifyChangeKeyValue = &VReg__NativeRegNotifyChangeKeyValue,
    .RegFlushKey = &VReg__NativeRegFlushKey,
};

void WINAPI VRegResolveNativeFunctions(_Out_ VRegNativeFunctions* const pFunctions)
{
    ZeroMemory(pFunctions, sizeof(*pFunctions));

    // A resolved export is a plain code address, which only a data pointer
    // cast can carry into the typed slot.
#define VREG_RESOLVE(name) *(void**)&pFunctions->name = VReg__FindRegistryExport(#name);
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_RESOLVE)
#undef VREG_RESOLVE
}

LSTATUS WINAPI VRegCreateNativeRegistry(
    _In_opt_ const VRegNativeFunctions* const pFunctions,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegRegistry** const ppRegistry
)
{
    VReg__NativeRegistry* const pRegistry = (VReg__NativeRegistry*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__NativeRegistry));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->object.lpVtbl = &VReg__NativeRegistryVtbl;
    pRegistry->pAllocContext = pAllocContext;
    if (pFunctions)
        pRegistry->functions = *pFunctions;
    else
        VRegResolveNativeFunctions(&pRegistry->functions);

    *ppRegistry = &pRegistry->object;
    return ERROR_SUCCESS;
}

_Ret_notnull_ VRegNativeFunctions* WINAPI VRegGetNativeFunctions(_In_ VRegRegistry* const pRegistry)
{
    return &VReg__NativeOf(pRegistry)->functions;
}
