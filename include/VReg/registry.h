#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VRegRegistry VRegRegistry;
typedef struct VRegRegistryVtbl VRegRegistryVtbl;

/**
 * @brief A registry: the Win32 `Reg*` family behind a function table, in the
 *  shape of a COM object without `IUnknown`.
 *
 *  An implementation puts this struct first in its own and fills a
 *  @ref VRegRegistryVtbl; a wrapper over another registry does the same and
 *  keeps a pointer to the one it wraps. Every slot is `WINAPI` and takes the
 *  object first. Callers go through the flat functions (`VRegOpenKeyExW` and
 *  so on), never through the table directly.
 *
 * @note Once hooked, every slot may run under a foreign loader lock: an
 *  implementation MUST NOT load a module, create a thread or wait on a lock a
 *  loader-lock holder could own.
 */
struct VRegRegistry
{
    const VRegRegistryVtbl* lpVtbl;

    /// What the registry allocates with, passed to `VREG_MALLOC` and its
    /// companions; the flat functions allocate their temporaries with it too.
    void* pAllocContext;
};

/**
 * @brief One slot per form of the `Reg*` family, with the object first.
 *
 *  The wide `Ex` forms are the surface every implementation fills; the flat
 *  function of one that is null answers @ref ERROR_CALL_NOT_IMPLEMENTED. Every
 *  other slot (the ANSI forms, the pre-`Ex` forms and the conveniences Win32
 *  builds from other calls) MAY be null, in which case its flat function
 *  derives the call from the wide `Ex` forms: ANSI forms widen and narrow
 *  through @ref CP_ACP as the native ones do. A wrapper that fills such a slot
 *  sees the form its caller used, which is what a tracing wrapper needs.
 */
struct VRegRegistryVtbl
{
    /// Frees the registry and everything it owns. Null for one whose creator
    /// frees it some other way.
    void (WINAPI* Destroy)(_In_ VRegRegistry* pThis);

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
    LSTATUS (WINAPI* RegOpenKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
    LSTATUS (WINAPI* RegCreateKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPWSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regclosekey
    LSTATUS (WINAPI* RegCloseKey)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
    LSTATUS (WINAPI* RegQueryValueExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
    LSTATUS (WINAPI* RegSetValueExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
    LSTATUS (WINAPI* RegDeleteValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
    LSTATUS (WINAPI* RegDeleteKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
    LSTATUS (WINAPI* RegDeleteTreeW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
    LSTATUS (WINAPI* RegEnumKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluew
    LSTATUS (WINAPI* RegEnumValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeyw
    LSTATUS (WINAPI* RegQueryInfoKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringw
    LSTATUS (WINAPI* RegLoadMUIStringW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
    LSTATUS (WINAPI* RegGetValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hkey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
    LSTATUS (WINAPI* RegNotifyChangeKeyValue)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
    LSTATUS (WINAPI* RegFlushKey)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
    LSTATUS (WINAPI* RegOpenKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
    LSTATUS (WINAPI* RegCreateKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
    LSTATUS (WINAPI* RegQueryValueExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
    LSTATUS (WINAPI* RegSetValueExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
    LSTATUS (WINAPI* RegDeleteValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
    LSTATUS (WINAPI* RegDeleteKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
    LSTATUS (WINAPI* RegDeleteTreeA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
    LSTATUS (WINAPI* RegEnumKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluea
    LSTATUS (WINAPI* RegEnumValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeya
    LSTATUS (WINAPI* RegQueryInfoKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringa
    LSTATUS (WINAPI* RegLoadMUIStringA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCSTR pszDirectory
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
    LSTATUS (WINAPI* RegGetValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hkey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeya
    LSTATUS (WINAPI* RegOpenKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
    LSTATUS (WINAPI* RegOpenKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
    LSTATUS (WINAPI* RegCreateKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
    LSTATUS (WINAPI* RegCreateKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
    LSTATUS (WINAPI* RegQueryValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPSTR lpData,
        _Inout_opt_ PLONG lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
    LSTATUS (WINAPI* RegQueryValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPWSTR lpData,
        _Inout_opt_ PLONG lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
    LSTATUS (WINAPI* RegSetValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCSTR lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
    LSTATUS (WINAPI* RegSetValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCWSTR lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
    LSTATUS (WINAPI* RegDeleteKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
    LSTATUS (WINAPI* RegDeleteKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
    LSTATUS (WINAPI* RegEnumKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_opt_(cchName) LPSTR lpName,
        _In_ DWORD cchName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
    LSTATUS (WINAPI* RegEnumKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_opt_(cchName) LPWSTR lpName,
        _In_ DWORD cchName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
    LSTATUS (WINAPI* RegSetKeyValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValueName,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCVOID lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
    LSTATUS (WINAPI* RegSetKeyValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValueName,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCVOID lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
    LSTATUS (WINAPI* RegDeleteKeyValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
    LSTATUS (WINAPI* RegDeleteKeyValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
    LSTATUS (WINAPI* RegQueryMultipleValuesA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_(num_vals) PVALENTA val_list,
        _In_ DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPSTR lpValueBuf,
        _Inout_opt_ LPDWORD ldwTotsize
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
    LSTATUS (WINAPI* RegQueryMultipleValuesW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_(num_vals) PVALENTW val_list,
        _In_ DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPWSTR lpValueBuf,
        _Inout_opt_ LPDWORD ldwTotsize
    );
};

/// Calls the `RegOpenKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
LSTATUS WINAPI VRegOpenKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
LSTATUS WINAPI VRegCreateKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _Reserved_ DWORD Reserved,
    _In_opt_ LPWSTR lpClass,
    _In_ DWORD dwOptions,
    _In_ REGSAM samDesired,
    _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ PHKEY phkResult,
    _Out_opt_ LPDWORD lpdwDisposition
);

/// Calls the `RegCloseKey` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regclosekey
LSTATUS WINAPI VRegCloseKey(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey
);

/// Calls the `RegQueryValueExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
LSTATUS WINAPI VRegQueryValueExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
);

/// Calls the `RegSetValueExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
LSTATUS WINAPI VRegSetValueExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Reserved_ DWORD Reserved,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteValueW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
LSTATUS WINAPI VRegDeleteValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName
);

/// Calls the `RegDeleteKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
LSTATUS WINAPI VRegDeleteKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// Calls the `RegDeleteTreeW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
LSTATUS WINAPI VRegDeleteTreeW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey
);

/// Calls the `RegEnumKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
LSTATUS WINAPI VRegEnumKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
    _Inout_ LPDWORD lpcchName,
    _Reserved_ LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegEnumValueW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluew
LSTATUS WINAPI VRegEnumValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
    _Inout_ LPDWORD lpcchValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _Inout_opt_ LPDWORD lpcbData
);

/// Calls the `RegQueryInfoKeyW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeyw
LSTATUS WINAPI VRegQueryInfoKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpcSubKeys,
    _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
    _Out_opt_ LPDWORD lpcbMaxClassLen,
    _Out_opt_ LPDWORD lpcValues,
    _Out_opt_ LPDWORD lpcbMaxValueNameLen,
    _Out_opt_ LPDWORD lpcbMaxValueLen,
    _Out_opt_ LPDWORD lpcbSecurityDescriptor,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegLoadMUIStringW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringw
LSTATUS WINAPI VRegLoadMUIStringW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
    _In_ DWORD cbOutBuf,
    _Out_opt_ LPDWORD pcbData,
    _In_ DWORD Flags,
    _In_opt_ LPCWSTR pszDirectory
);

/// Calls the `RegGetValueW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
LSTATUS WINAPI VRegGetValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hkey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValue,
    _In_ DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
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
    _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
    _Inout_opt_ LPDWORD pcbData
);

/// Calls the `RegNotifyChangeKeyValue` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
LSTATUS WINAPI VRegNotifyChangeKeyValue(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ BOOL bWatchSubtree,
    _In_ DWORD dwNotifyFilter,
    _In_opt_ HANDLE hEvent,
    _In_ BOOL fAsynchronous
);

/// Calls the `RegFlushKey` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
LSTATUS WINAPI VRegFlushKey(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey
);

/// Calls the `RegOpenKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
LSTATUS WINAPI VRegOpenKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
LSTATUS WINAPI VRegCreateKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey,
    _Reserved_ DWORD Reserved,
    _In_opt_ LPSTR lpClass,
    _In_ DWORD dwOptions,
    _In_ REGSAM samDesired,
    _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ PHKEY phkResult,
    _Out_opt_ LPDWORD lpdwDisposition
);

/// Calls the `RegQueryValueExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
LSTATUS WINAPI VRegQueryValueExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
);

/// Calls the `RegSetValueExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
LSTATUS WINAPI VRegSetValueExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName,
    _Reserved_ DWORD Reserved,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
LSTATUS WINAPI VRegDeleteValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName
);

/// Calls the `RegDeleteKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
LSTATUS WINAPI VRegDeleteKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// Calls the `RegDeleteTreeA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
LSTATUS WINAPI VRegDeleteTreeA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey
);

/// Calls the `RegEnumKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
LSTATUS WINAPI VRegEnumKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPSTR lpName,
    _Inout_ LPDWORD lpcchName,
    _Reserved_ LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegEnumValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluea
LSTATUS WINAPI VRegEnumValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPSTR lpValueName,
    _Inout_ LPDWORD lpcchValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _Inout_opt_ LPDWORD lpcbData
);

/// Calls the `RegQueryInfoKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeya
LSTATUS WINAPI VRegQueryInfoKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpcSubKeys,
    _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
    _Out_opt_ LPDWORD lpcbMaxClassLen,
    _Out_opt_ LPDWORD lpcValues,
    _Out_opt_ LPDWORD lpcbMaxValueNameLen,
    _Out_opt_ LPDWORD lpcbMaxValueLen,
    _Out_opt_ LPDWORD lpcbSecurityDescriptor,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegLoadMUIStringA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringa
LSTATUS WINAPI VRegLoadMUIStringA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) LPSTR pszOutBuf,
    _In_ DWORD cbOutBuf,
    _Out_opt_ LPDWORD pcbData,
    _In_ DWORD Flags,
    _In_opt_ LPCSTR pszDirectory
);

/// Calls the `RegGetValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
LSTATUS WINAPI VRegGetValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hkey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValue,
    _In_ DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
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
    _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
    _Inout_opt_ LPDWORD pcbData
);

/// Calls the `RegOpenKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeya
LSTATUS WINAPI VRegOpenKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegOpenKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
LSTATUS WINAPI VRegOpenKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
LSTATUS WINAPI VRegCreateKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
LSTATUS WINAPI VRegCreateKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegQueryValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
LSTATUS WINAPI VRegQueryValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPSTR lpData,
    _Inout_opt_ PLONG lpcbData
);

/// Calls the `RegQueryValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
LSTATUS WINAPI VRegQueryValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPWSTR lpData,
    _Inout_opt_ PLONG lpcbData
);

/// Calls the `RegSetValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
LSTATUS WINAPI VRegSetValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCSTR lpData,
    _In_ DWORD cbData
);

/// Calls the `RegSetValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
LSTATUS WINAPI VRegSetValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCWSTR lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
LSTATUS WINAPI VRegDeleteKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey
);

/// Calls the `RegDeleteKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
LSTATUS WINAPI VRegDeleteKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey
);

/// Calls the `RegEnumKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
LSTATUS WINAPI VRegEnumKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_opt_(cchName) LPSTR lpName,
    _In_ DWORD cchName
);

/// Calls the `RegEnumKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
LSTATUS WINAPI VRegEnumKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_opt_(cchName) LPWSTR lpName,
    _In_ DWORD cchName
);

/// Calls the `RegSetKeyValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
LSTATUS WINAPI VRegSetKeyValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValueName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCVOID lpData,
    _In_ DWORD cbData
);

/// Calls the `RegSetKeyValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
LSTATUS WINAPI VRegSetKeyValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValueName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCVOID lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteKeyValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
LSTATUS WINAPI VRegDeleteKeyValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValueName
);

/// Calls the `RegDeleteKeyValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
LSTATUS WINAPI VRegDeleteKeyValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValueName
);

/// Calls the `RegQueryMultipleValuesA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
LSTATUS WINAPI VRegQueryMultipleValuesA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_(num_vals) PVALENTA val_list,
    _In_ DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPSTR lpValueBuf,
    _Inout_opt_ LPDWORD ldwTotsize
);

/// Calls the `RegQueryMultipleValuesW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
LSTATUS WINAPI VRegQueryMultipleValuesW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_(num_vals) PVALENTW val_list,
    _In_ DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPWSTR lpValueBuf,
    _Inout_opt_ LPDWORD ldwTotsize
);

/// The neutral names select the wide or the ANSI form by @ref UNICODE, as
/// `winreg.h` does for the functions they mirror.
#ifdef UNICODE
#define VRegCreateKey VRegCreateKeyW
#define VRegCreateKeyEx VRegCreateKeyExW
#define VRegDeleteKey VRegDeleteKeyW
#define VRegDeleteKeyEx VRegDeleteKeyExW
#define VRegDeleteKeyValue VRegDeleteKeyValueW
#define VRegDeleteTree VRegDeleteTreeW
#define VRegDeleteValue VRegDeleteValueW
#define VRegEnumKey VRegEnumKeyW
#define VRegEnumKeyEx VRegEnumKeyExW
#define VRegEnumValue VRegEnumValueW
#define VRegGetValue VRegGetValueW
#define VRegLoadMUIString VRegLoadMUIStringW
#define VRegOpenKey VRegOpenKeyW
#define VRegOpenKeyEx VRegOpenKeyExW
#define VRegQueryInfoKey VRegQueryInfoKeyW
#define VRegQueryMultipleValues VRegQueryMultipleValuesW
#define VRegQueryValue VRegQueryValueW
#define VRegQueryValueEx VRegQueryValueExW
#define VRegSetKeyValue VRegSetKeyValueW
#define VRegSetValue VRegSetValueW
#define VRegSetValueEx VRegSetValueExW
#else
#define VRegCreateKey VRegCreateKeyA
#define VRegCreateKeyEx VRegCreateKeyExA
#define VRegDeleteKey VRegDeleteKeyA
#define VRegDeleteKeyEx VRegDeleteKeyExA
#define VRegDeleteKeyValue VRegDeleteKeyValueA
#define VRegDeleteTree VRegDeleteTreeA
#define VRegDeleteValue VRegDeleteValueA
#define VRegEnumKey VRegEnumKeyA
#define VRegEnumKeyEx VRegEnumKeyExA
#define VRegEnumValue VRegEnumValueA
#define VRegGetValue VRegGetValueA
#define VRegLoadMUIString VRegLoadMUIStringA
#define VRegOpenKey VRegOpenKeyA
#define VRegOpenKeyEx VRegOpenKeyExA
#define VRegQueryInfoKey VRegQueryInfoKeyA
#define VRegQueryMultipleValues VRegQueryMultipleValuesA
#define VRegQueryValue VRegQueryValueA
#define VRegQueryValueEx VRegQueryValueExA
#define VRegSetKeyValue VRegSetKeyValueA
#define VRegSetValue VRegSetValueA
#define VRegSetValueEx VRegSetValueExA
#endif

/// Frees a registry through its `Destroy` slot; null, and a registry whose
/// slot is null, are ignored.
void WINAPI VRegDestroyRegistry(_In_opt_ VRegRegistry* pRegistry);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <cstddef>
#include <string>
#include <vector>

namespace VReg
{
/**
 * @brief The Win32 `Reg*` family of function signatures.
 *
 *  The `Ex` wide forms are the virtual surface every implementation provides.
 *  The ANSI forms, the pre-`Ex` forms and the few wide conveniences Win32
 *  builds from other calls (@ref RegSetKeyValueW and friends) are implemented
 *  once, here, over those virtuals; ANSI forms widen and narrow through
 *  @ref CP_ACP in both directions, as the native ANSI functions do. Every one
 *  of them answers what the native function answers for the same input:
 *  status, out-sizes and bytes.
 *
 *  The ANSI forms are virtual too, so a wrapper (one that traces each call,
 *  say) can see which form its caller used. One that overrides an ANSI form
 *  forwards it to the ANSI form of the registry it wraps; one that does not
 *  sees the call as the wide form it becomes.
 *
 *  Members carry their Win32 names, so the @ref UNICODE selector macros of
 *  `winreg.h` work on them: `registry.RegOpenKeyEx(...)` calls
 *  @ref RegOpenKeyExW or @ref RegOpenKeyExA. Only ANSI and wide names are ever
 *  declared; a member with the neutral name would be renamed by the macro.
 *  Queries are `const`. The reads that size and fill owned buffers are free
 *  functions the tests keep in `test/owned_values.h`, not part of this interface; neither are
 *  the functions VReg refuses on virtual handles or does not hook, which the
 *  README lists under "Supported functions".
 *
 * @note Every member is `noexcept`; an allocation failure answers
 *  @ref ERROR_OUTOFMEMORY.
 *
 * @note Once hooked (@ref Detours::Install), every member may run under a
 *  foreign loader lock: an implementation MUST NOT load a module, create a
 *  thread or wait on a lock a loader-lock holder could own.
 */
class Registry
{
public:
    Registry() noexcept;
    virtual ~Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    /// The registry as a C object. Here, one whose table forwards the wide
    /// surface to the virtuals below, with the other slots null; a class
    /// that stands for a C registry answers that registry instead.
    [[nodiscard]] virtual VRegRegistry* CObject() noexcept;

    /// The C object with the way back to its owner, which the table's
    /// forwarders read.
    struct CObjectData
    {
        VRegRegistry object;
        Registry* pOwner;
    };

    /// @name The virtual surface
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
    virtual LSTATUS RegOpenKeyExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
    virtual LSTATUS RegCreateKeyExW(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPWSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regclosekey
    virtual LSTATUS RegCloseKey(
        _In_ HKEY hKey
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
    virtual LSTATUS RegQueryValueExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
    virtual LSTATUS RegSetValueExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
    virtual LSTATUS RegDeleteValueW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
    virtual LSTATUS RegDeleteKeyExW(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
    virtual LSTATUS RegDeleteTreeW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
    virtual LSTATUS RegEnumKeyExW(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluew
    virtual LSTATUS RegEnumValueW(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeyw
    virtual LSTATUS RegQueryInfoKeyW(
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringw
    virtual LSTATUS RegLoadMUIStringW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
    virtual LSTATUS RegGetValueW(
        _In_ HKEY hkey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
    virtual LSTATUS RegNotifyChangeKeyValue(
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
    virtual LSTATUS RegFlushKey(
        _In_ HKEY hKey
    ) noexcept = 0;

    /// @}

    /// @name ANSI forms of the virtual surface
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
    virtual LSTATUS RegOpenKeyExA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
    virtual LSTATUS RegCreateKeyExA(
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
    virtual LSTATUS RegQueryValueExA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
    virtual LSTATUS RegSetValueExA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
    virtual LSTATUS RegDeleteValueA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
    virtual LSTATUS RegDeleteKeyExA(
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
    virtual LSTATUS RegDeleteTreeA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
    virtual LSTATUS RegEnumKeyExA(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluea
    virtual LSTATUS RegEnumValueA(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeya
    virtual LSTATUS RegQueryInfoKeyA(
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept;

    /// Answers @ref ERROR_CALL_NOT_IMPLEMENTED on every input, as the native
    /// function does.
    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringa
    virtual LSTATUS RegLoadMUIStringA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCSTR pszDirectory
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
    virtual LSTATUS RegGetValueA(
        _In_ HKEY hkey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    ) const noexcept;

    /// @}

    /// @name Pre-`Ex` forms
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeya
    virtual LSTATUS RegOpenKeyA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_ PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
    LSTATUS RegOpenKeyW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_ PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
    virtual LSTATUS RegCreateKeyA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_ PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
    LSTATUS RegCreateKeyW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_ PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
    virtual LSTATUS RegQueryValueA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPSTR lpData,
        _Inout_opt_ PLONG lpcbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
    LSTATUS RegQueryValueW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPWSTR lpData,
        _Inout_opt_ PLONG lpcbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
    virtual LSTATUS RegSetValueA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCSTR lpData,
        _In_ DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
    LSTATUS RegSetValueW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCWSTR lpData,
        _In_ DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
    virtual LSTATUS RegDeleteKeyA(
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
    LSTATUS RegDeleteKeyW(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
    virtual LSTATUS RegEnumKeyA(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_opt_(cchName) LPSTR lpName,
        _In_ DWORD cchName
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
    LSTATUS RegEnumKeyW(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_opt_(cchName) LPWSTR lpName,
        _In_ DWORD cchName
    ) const noexcept;

    /// @}

    /// @name Conveniences Win32 builds from the calls above
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
    virtual LSTATUS RegSetKeyValueA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValueName,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCVOID lpData,
        _In_ DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
    LSTATUS RegSetKeyValueW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValueName,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCVOID lpData,
        _In_ DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
    virtual LSTATUS RegDeleteKeyValueA(
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValueName
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
    LSTATUS RegDeleteKeyValueW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValueName
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
    virtual LSTATUS RegQueryMultipleValuesA(
        _In_ HKEY hKey,
        _Out_writes_(num_vals) PVALENTA val_list,
        _In_ DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPSTR lpValueBuf,
        _Inout_opt_ LPDWORD ldwTotsize
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
    LSTATUS RegQueryMultipleValuesW(
        _In_ HKEY hKey,
        _Out_writes_(num_vals) PVALENTW val_list,
        _In_ DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPWSTR lpValueBuf,
        _Inout_opt_ LPDWORD ldwTotsize
    ) const noexcept;

    /// @}

private:
    static const VRegRegistryVtbl s_cVtbl;

    CObjectData m_cObject;
};
} // namespace VReg
#endif // __cplusplus
