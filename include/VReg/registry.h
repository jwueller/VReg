#pragma once

#include <Windows.h>

#include <cstddef>
#include <string>
#include <vector>

namespace VReg
{
/**
 * @brief A registry with the Win32 `Reg*` signatures.
 *
 *  The `Ex` W forms are the virtual surface every implementation provides.
 *  The A forms, the pre-`Ex` forms and the few W conveniences Win32 builds
 *  from other calls (`RegSetKeyValueW` and friends) are implemented once,
 *  here, over those virtuals; A forms widen and narrow through `CP_ACP` in
 *  both directions, as the native A functions do. Every one of them answers
 *  what the native function answers for the same input: status, out-sizes
 *  and bytes.
 *
 *  The A forms are virtual too, so a wrapper (one that traces each call, say)
 *  can see which form its caller used. One that overrides an A form forwards
 *  it to the A form of the registry it wraps; one that does not sees the call
 *  as the W form it becomes.
 *
 *  Members carry their Win32 names, so the `UNICODE` selector macros of
 *  `winreg.h` work on them: `registry.RegOpenKeyEx(...)` calls
 *  @ref RegOpenKeyExW or @ref RegOpenKeyExA. Only A and W names are ever
 *  declared; a member with the neutral name would be renamed by the macro.
 *  Queries are `const`. The reads that size and fill owned buffers are free
 *  functions in `owned_values.h`, not part of this interface.
 *
 * @note Every member is `noexcept`; an allocation failure answers
 *  `ERROR_OUTOFMEMORY`.
 */
class Registry
{
public:
    virtual ~Registry() = default;

    /// @name The virtual surface
    /// @{
    virtual LSTATUS RegOpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept = 0;

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

    virtual LSTATUS RegCloseKey(
        HKEY hKey
    ) noexcept = 0;

    virtual LSTATUS RegQueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept = 0;

    virtual LSTATUS RegSetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept = 0;

    virtual LSTATUS RegDeleteValueW(
        HKEY hKey,
        LPCWSTR lpValueName
    ) noexcept = 0;

    virtual LSTATUS RegDeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept = 0;

    virtual LSTATUS RegDeleteTreeW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept = 0;

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

    virtual LSTATUS RegLoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    ) const noexcept = 0;

    virtual LSTATUS RegGetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) const noexcept = 0;

    virtual LSTATUS RegNotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept = 0;

    virtual LSTATUS RegFlushKey(
        HKEY hKey
    ) noexcept = 0;
    /// @}

    /// @name A forms of the virtual surface
    /// @{
    virtual LSTATUS RegOpenKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept;

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

    virtual LSTATUS RegQueryValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept;

    virtual LSTATUS RegSetValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept;

    virtual LSTATUS RegDeleteValueA(
        HKEY hKey,
        LPCSTR lpValueName
    ) noexcept;

    virtual LSTATUS RegDeleteKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept;

    virtual LSTATUS RegDeleteTreeA(
        HKEY hKey,
        LPCSTR lpSubKey
    ) noexcept;

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
    virtual LSTATUS RegLoadMUIStringA(
        HKEY hKey,
        LPCSTR pszValue,
        LPSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCSTR pszDirectory
    ) const noexcept;

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
    virtual LSTATUS RegOpenKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    LSTATUS RegOpenKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    virtual LSTATUS RegCreateKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    LSTATUS RegCreateKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    virtual LSTATUS RegQueryValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPSTR lpData,
        PLONG lpcbData
    ) noexcept;

    LSTATUS RegQueryValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPWSTR lpData,
        PLONG lpcbData
    ) noexcept;

    virtual LSTATUS RegSetValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD dwType,
        LPCSTR lpData,
        DWORD cbData
    ) noexcept;

    LSTATUS RegSetValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD dwType,
        LPCWSTR lpData,
        DWORD cbData
    ) noexcept;

    virtual LSTATUS RegDeleteKeyA(
        HKEY hKey,
        LPCSTR lpSubKey
    ) noexcept;

    LSTATUS RegDeleteKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept;

    virtual LSTATUS RegEnumKeyA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpName,
        DWORD cchName
    ) const noexcept;

    LSTATUS RegEnumKeyW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        DWORD cchName
    ) const noexcept;
    /// @}

    /// @name Conveniences Win32 builds from the calls above
    /// @{
    virtual LSTATUS RegSetKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    ) noexcept;

    LSTATUS RegSetKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    ) noexcept;

    virtual LSTATUS RegDeleteKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName
    ) noexcept;

    LSTATUS RegDeleteKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName
    ) noexcept;

    virtual LSTATUS RegQueryMultipleValuesA(
        HKEY hKey,
        PVALENTA val_list,
        DWORD num_vals,
        LPSTR lpValueBuf,
        LPDWORD ldwTotsize
    ) const noexcept;

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
