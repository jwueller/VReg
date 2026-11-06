#include <VReg/registry.h>

VREGDEF LSTATUS WINAPI VRegOpenKeyExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (!pRegistry->lpVtbl->RegOpenKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegOpenKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

VREGDEF LSTATUS WINAPI VRegCreateKeyExW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegCreateKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegCreateKeyExW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegCloseKey(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (!pRegistry->lpVtbl->RegCloseKey)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegCloseKey(
        pRegistry,
        hKey
    );
}

VREGDEF LSTATUS WINAPI VRegQueryValueExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (!pRegistry->lpVtbl->RegQueryValueExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegQueryValueExW(
        pRegistry,
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

VREGDEF LSTATUS WINAPI VRegSetValueExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (!pRegistry->lpVtbl->RegSetValueExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegSetValueExW(
        pRegistry,
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

VREGDEF LSTATUS WINAPI VRegDeleteValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (!pRegistry->lpVtbl->RegDeleteValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegDeleteValueW(
        pRegistry,
        hKey,
        lpValueName
    );
}

VREGDEF LSTATUS WINAPI VRegDeleteKeyExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (!pRegistry->lpVtbl->RegDeleteKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegDeleteKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

VREGDEF LSTATUS WINAPI VRegDeleteTreeW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    if (!pRegistry->lpVtbl->RegDeleteTreeW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegDeleteTreeW(
        pRegistry,
        hKey,
        lpSubKey
    );
}

VREGDEF LSTATUS WINAPI VRegEnumKeyExW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegEnumKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegEnumKeyExW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegEnumValueW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegEnumValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegEnumValueW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegQueryInfoKeyW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegQueryInfoKeyW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegQueryInfoKeyW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegLoadMUIStringW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    if (!pRegistry->lpVtbl->RegLoadMUIStringW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegLoadMUIStringW(
        pRegistry,
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
VREGDEF LSTATUS WINAPI VRegGetValueW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegGetValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegGetValueW(
        pRegistry,
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

VREGDEF LSTATUS WINAPI VRegNotifyChangeKeyValue(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    if (!pRegistry->lpVtbl->RegNotifyChangeKeyValue)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegNotifyChangeKeyValue(
        pRegistry,
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

VREGDEF LSTATUS WINAPI VRegFlushKey(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (!pRegistry->lpVtbl->RegFlushKey)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegFlushKey(
        pRegistry,
        hKey
    );
}

VREGDEF void WINAPI VRegDestroyRegistry(_In_opt_ VRegRegistry* const pRegistry)
{
    if (pRegistry && pRegistry->lpVtbl->Destroy)
        pRegistry->lpVtbl->Destroy(pRegistry);
}
