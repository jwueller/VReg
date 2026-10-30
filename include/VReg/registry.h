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
 *  the functions VReg refuses on virtual keys or does not hook, which the
 *  README lists under "Supported functions".
 *
 * @note Every member is `noexcept`; an allocation failure answers
 *  @ref ERROR_OUTOFMEMORY.
 */
class Registry
{
public:
    virtual ~Registry() = default;

    /// @name The virtual surface
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
    virtual LSTATUS RegOpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
    virtual LSTATUS RegCreateKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD Reserved,
        LPWSTR lpClass,
        DWORD dwOptions,
        REGSAM samDesired,
        const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        PHKEY phkResult,
        LPDWORD lpdwDisposition
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regclosekey
    virtual LSTATUS RegCloseKey(
        HKEY hKey
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
    virtual LSTATUS RegQueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
    virtual LSTATUS RegSetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
    virtual LSTATUS RegDeleteValueW(
        HKEY hKey,
        LPCWSTR lpValueName
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
    virtual LSTATUS RegDeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
    virtual LSTATUS RegDeleteTreeW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
    virtual LSTATUS RegEnumKeyExW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluew
    virtual LSTATUS RegEnumValueW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeyw
    virtual LSTATUS RegQueryInfoKeyW(
        HKEY hKey,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        LPDWORD lpReserved,
        LPDWORD lpcSubKeys,
        LPDWORD lpcbMaxSubKeyLen,
        LPDWORD lpcbMaxClassLen,
        LPDWORD lpcValues,
        LPDWORD lpcbMaxValueNameLen,
        LPDWORD lpcbMaxValueLen,
        LPDWORD lpcbSecurityDescriptor,
        PFILETIME lpftLastWriteTime
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringw
    virtual LSTATUS RegLoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
    virtual LSTATUS RegGetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) const noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
    virtual LSTATUS RegNotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept = 0;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
    virtual LSTATUS RegFlushKey(
        HKEY hKey
    ) noexcept = 0;

    /// @}

    /// @name ANSI forms of the virtual surface
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
    virtual LSTATUS RegOpenKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
    virtual LSTATUS RegCreateKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD Reserved,
        LPSTR lpClass,
        DWORD dwOptions,
        REGSAM samDesired,
        const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        PHKEY phkResult,
        LPDWORD lpdwDisposition
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
    virtual LSTATUS RegQueryValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
    virtual LSTATUS RegSetValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
    virtual LSTATUS RegDeleteValueA(
        HKEY hKey,
        LPCSTR lpValueName
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
    virtual LSTATUS RegDeleteKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
    virtual LSTATUS RegDeleteTreeA(
        HKEY hKey,
        LPCSTR lpSubKey
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
    virtual LSTATUS RegEnumKeyExA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluea
    virtual LSTATUS RegEnumValueA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeya
    virtual LSTATUS RegQueryInfoKeyA(
        HKEY hKey,
        LPSTR lpClass,
        LPDWORD lpcchClass,
        LPDWORD lpReserved,
        LPDWORD lpcSubKeys,
        LPDWORD lpcbMaxSubKeyLen,
        LPDWORD lpcbMaxClassLen,
        LPDWORD lpcValues,
        LPDWORD lpcbMaxValueNameLen,
        LPDWORD lpcbMaxValueLen,
        LPDWORD lpcbSecurityDescriptor,
        PFILETIME lpftLastWriteTime
    ) const noexcept;

    /// Answers @ref ERROR_CALL_NOT_IMPLEMENTED on every input, as the native
    /// function does.
    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringa
    virtual LSTATUS RegLoadMUIStringA(
        HKEY hKey,
        LPCSTR pszValue,
        LPSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCSTR pszDirectory
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
    virtual LSTATUS RegGetValueA(
        HKEY hkey,
        LPCSTR lpSubKey,
        LPCSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) const noexcept;

    /// @}

    /// @name Pre-`Ex` forms
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeya
    virtual LSTATUS RegOpenKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
    LSTATUS RegOpenKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
    virtual LSTATUS RegCreateKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
    LSTATUS RegCreateKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
    virtual LSTATUS RegQueryValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPSTR lpData,
        PLONG lpcbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
    LSTATUS RegQueryValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPWSTR lpData,
        PLONG lpcbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
    virtual LSTATUS RegSetValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD dwType,
        LPCSTR lpData,
        DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
    LSTATUS RegSetValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD dwType,
        LPCWSTR lpData,
        DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
    virtual LSTATUS RegDeleteKeyA(
        HKEY hKey,
        LPCSTR lpSubKey
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
    LSTATUS RegDeleteKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
    virtual LSTATUS RegEnumKeyA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpName,
        DWORD cchName
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
    LSTATUS RegEnumKeyW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        DWORD cchName
    ) const noexcept;

    /// @}

    /// @name Conveniences Win32 builds from the calls above
    /// @{

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
    virtual LSTATUS RegSetKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
    LSTATUS RegSetKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
    virtual LSTATUS RegDeleteKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
    LSTATUS RegDeleteKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName
    ) noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
    virtual LSTATUS RegQueryMultipleValuesA(
        HKEY hKey,
        PVALENTA val_list,
        DWORD num_vals,
        LPSTR lpValueBuf,
        LPDWORD ldwTotsize
    ) const noexcept;

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
    LSTATUS RegQueryMultipleValuesW(
        HKEY hKey,
        PVALENTW val_list,
        DWORD num_vals,
        LPWSTR lpValueBuf,
        LPDWORD ldwTotsize
    ) const noexcept;

    /// @}
};
} // namespace VReg
