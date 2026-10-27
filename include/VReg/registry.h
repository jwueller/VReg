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
 *  Members are named after their Win32 function without the `Reg` prefix.
 *  The unsuffixed members that have no Win32 counterpart (@ref QueryValue,
 *  @ref EnumKey, ...) are the owned-value forms: they size and fill a
 *  `std::wstring` or `std::vector<std::byte>` themselves.
 *
 *  Agent's choice: member names drop `Reg` so a call reads
 *  `registry.OpenKeyExW(...)`; keeping the full Win32 name was passed over as
 *  redundant next to the object.
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
    virtual LSTATUS OpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept = 0;

    virtual LSTATUS CreateKeyExW(
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

    virtual LSTATUS CloseKey(
        HKEY hKey
    ) noexcept = 0;

    virtual LSTATUS QueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept = 0;

    virtual LSTATUS SetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept = 0;

    virtual LSTATUS DeleteValueW(
        HKEY hKey,
        LPCWSTR lpValueName
    ) noexcept = 0;

    virtual LSTATUS DeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept = 0;

    virtual LSTATUS DeleteTreeW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept = 0;

    virtual LSTATUS EnumKeyExW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) noexcept = 0;

    virtual LSTATUS EnumValueW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept = 0;

    virtual LSTATUS QueryInfoKeyW(
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
    ) noexcept = 0;

    virtual LSTATUS LoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    ) noexcept = 0;

    virtual LSTATUS GetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) noexcept = 0;

    virtual LSTATUS NotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept = 0;

    virtual LSTATUS FlushKey(
        HKEY hKey
    ) noexcept = 0;
    /// @}

    /// @name A forms of the virtual surface
    /// @{
    LSTATUS OpenKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept;

    LSTATUS CreateKeyExA(
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

    LSTATUS QueryValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept;

    LSTATUS SetValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept;

    LSTATUS DeleteValueA(
        HKEY hKey,
        LPCSTR lpValueName
    ) noexcept;

    LSTATUS DeleteKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept;

    LSTATUS DeleteTreeA(
        HKEY hKey,
        LPCSTR lpSubKey
    ) noexcept;

    LSTATUS EnumKeyExA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) noexcept;

    LSTATUS EnumValueA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept;

    LSTATUS QueryInfoKeyA(
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
    ) noexcept;

    /// Answers `ERROR_CALL_NOT_IMPLEMENTED` on every input, as the native
    /// function does.
    LSTATUS LoadMUIStringA(
        HKEY hKey,
        LPCSTR pszValue,
        LPSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCSTR pszDirectory
    ) noexcept;

    LSTATUS GetValueA(
        HKEY hkey,
        LPCSTR lpSubKey,
        LPCSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) noexcept;
    /// @}

    /// @name Pre-`Ex` forms
    /// @{
    LSTATUS OpenKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    LSTATUS OpenKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    LSTATUS CreateKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    LSTATUS CreateKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    ) noexcept;

    LSTATUS QueryValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPSTR lpData,
        PLONG lpcbData
    ) noexcept;

    LSTATUS QueryValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPWSTR lpData,
        PLONG lpcbData
    ) noexcept;

    LSTATUS SetValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD dwType,
        LPCSTR lpData,
        DWORD cbData
    ) noexcept;

    LSTATUS SetValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD dwType,
        LPCWSTR lpData,
        DWORD cbData
    ) noexcept;

    LSTATUS DeleteKeyA(
        HKEY hKey,
        LPCSTR lpSubKey
    ) noexcept;

    LSTATUS DeleteKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept;

    LSTATUS EnumKeyA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpName,
        DWORD cchName
    ) noexcept;

    LSTATUS EnumKeyW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        DWORD cchName
    ) noexcept;
    /// @}

    /// @name Conveniences Win32 builds from the calls above
    /// @{
    LSTATUS SetKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    ) noexcept;

    LSTATUS SetKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    ) noexcept;

    LSTATUS DeleteKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName
    ) noexcept;

    LSTATUS DeleteKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName
    ) noexcept;

    LSTATUS QueryMultipleValuesA(
        HKEY hKey,
        PVALENTA val_list,
        DWORD num_vals,
        LPSTR lpValueBuf,
        LPDWORD ldwTotsize
    ) noexcept;

    LSTATUS QueryMultipleValuesW(
        HKEY hKey,
        PVALENTW val_list,
        DWORD num_vals,
        LPWSTR lpValueBuf,
        LPDWORD ldwTotsize
    ) noexcept;
    /// @}

    /// @name Owned-value forms
    /// Each retries with a larger buffer while the value grows between the
    /// sizing call and the read.
    /// @{
    LSTATUS QueryValue(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpType,
        std::vector<std::byte>& data
    ) noexcept;

    LSTATUS GetValue(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        std::vector<std::byte>& data
    ) noexcept;

    /// A string value without its terminator. @p dwFlags takes `RRF_*` flags
    /// except the type restriction, which this adds: `REG_SZ`, and with
    /// `RRF_NOEXPAND` also `REG_EXPAND_SZ` (otherwise `GetValueW` expands it
    /// into a `REG_SZ`).
    LSTATUS GetString(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        std::wstring& text
    ) noexcept;

    LSTATUS EnumKey(
        HKEY hKey,
        DWORD dwIndex,
        std::wstring& name
    ) noexcept;

    LSTATUS EnumValue(
        HKEY hKey,
        DWORD dwIndex,
        std::wstring& name,
        LPDWORD lpType,
        std::vector<std::byte>* pData
    ) noexcept;

    LSTATUS LoadMUIString(
        HKEY hKey,
        LPCWSTR pszValue,
        DWORD Flags,
        LPCWSTR pszDirectory,
        std::wstring& text
    ) noexcept;
    /// @}
};
} // namespace VReg
