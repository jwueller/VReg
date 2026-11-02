#pragma once

#include <Windows.h>

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
 *  functions in `owned_values.h`, not part of this interface; neither are
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
    virtual ~Registry() = default;

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
        _Out_writes_to_opt_(*lpcchClass,*lpcchClass + 1) LPWSTR lpClass,
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
        _Out_writes_to_opt_(*lpcchClass,*lpcchClass + 1) LPSTR lpClass,
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
};
} // namespace VReg
