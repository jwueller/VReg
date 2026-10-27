#pragma once

#include <Windows.h>

namespace VReg::Test
{
/// The real `Reg*` functions under the member names of `VReg::Registry`, so a
/// scenario template runs against the oracle and against any `Registry`
/// alike.
struct Win32Api
{
    LSTATUS OpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    )
    {
        return ::RegOpenKeyExW(
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
    }

    LSTATUS CreateKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD Reserved,
        LPWSTR lpClass,
        DWORD dwOptions,
        REGSAM samDesired,
        const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        PHKEY phkResult,
        LPDWORD lpdwDisposition
    )
    {
        return ::RegCreateKeyExW(
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

    LSTATUS CloseKey(
        HKEY hKey
    )
    {
        return ::RegCloseKey(
            hKey
        );
    }

    LSTATUS QueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    )
    {
        return ::RegQueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }

    LSTATUS SetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    )
    {
        return ::RegSetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS DeleteValueW(
        HKEY hKey,
        LPCWSTR lpValueName
    )
    {
        return ::RegDeleteValueW(
            hKey,
            lpValueName
        );
    }

    LSTATUS DeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    )
    {
        return ::RegDeleteKeyExW(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    LSTATUS DeleteTreeW(
        HKEY hKey,
        LPCWSTR lpSubKey
    )
    {
        return ::RegDeleteTreeW(
            hKey,
            lpSubKey
        );
    }

    LSTATUS EnumKeyExW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    )
    {
        return ::RegEnumKeyExW(
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

    LSTATUS EnumValueW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    )
    {
        return ::RegEnumValueW(
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

    LSTATUS QueryInfoKeyW(
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
    )
    {
        return ::RegQueryInfoKeyW(
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

    LSTATUS LoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    )
    {
        return ::RegLoadMUIStringW(
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
    }

    LSTATUS GetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    )
    {
        return ::RegGetValueW(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    }

    LSTATUS NotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    )
    {
        return ::RegNotifyChangeKeyValue(
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }

    LSTATUS FlushKey(
        HKEY hKey
    )
    {
        return ::RegFlushKey(
            hKey
        );
    }

    LSTATUS OpenKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    )
    {
        return ::RegOpenKeyExA(
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
    }

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
    )
    {
        return ::RegCreateKeyExA(
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

    LSTATUS QueryValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    )
    {
        return ::RegQueryValueExA(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }

    LSTATUS SetValueExA(
        HKEY hKey,
        LPCSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    )
    {
        return ::RegSetValueExA(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS DeleteValueA(
        HKEY hKey,
        LPCSTR lpValueName
    )
    {
        return ::RegDeleteValueA(
            hKey,
            lpValueName
        );
    }

    LSTATUS DeleteKeyExA(
        HKEY hKey,
        LPCSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    )
    {
        return ::RegDeleteKeyExA(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    LSTATUS DeleteTreeA(
        HKEY hKey,
        LPCSTR lpSubKey
    )
    {
        return ::RegDeleteTreeA(
            hKey,
            lpSubKey
        );
    }

    LSTATUS EnumKeyExA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    )
    {
        return ::RegEnumKeyExA(
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

    LSTATUS EnumValueA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    )
    {
        return ::RegEnumValueA(
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
    )
    {
        return ::RegQueryInfoKeyA(
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

    LSTATUS LoadMUIStringA(
        HKEY hKey,
        LPCSTR pszValue,
        LPSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCSTR pszDirectory
    )
    {
        return ::RegLoadMUIStringA(
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
    }

    LSTATUS GetValueA(
        HKEY hkey,
        LPCSTR lpSubKey,
        LPCSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    )
    {
        return ::RegGetValueA(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    }

    LSTATUS OpenKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    )
    {
        return ::RegOpenKeyA(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    LSTATUS OpenKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    )
    {
        return ::RegOpenKeyW(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    LSTATUS CreateKeyA(
        HKEY hKey,
        LPCSTR lpSubKey,
        PHKEY phkResult
    )
    {
        return ::RegCreateKeyA(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    LSTATUS CreateKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        PHKEY phkResult
    )
    {
        return ::RegCreateKeyW(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    LSTATUS QueryValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPSTR lpData,
        PLONG lpcbData
    )
    {
        return ::RegQueryValueA(
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );
    }

    LSTATUS QueryValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPWSTR lpData,
        PLONG lpcbData
    )
    {
        return ::RegQueryValueW(
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );
    }

    LSTATUS SetValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        DWORD dwType,
        LPCSTR lpData,
        DWORD cbData
    )
    {
        return ::RegSetValueA(
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS SetValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD dwType,
        LPCWSTR lpData,
        DWORD cbData
    )
    {
        return ::RegSetValueW(
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS DeleteKeyA(
        HKEY hKey,
        LPCSTR lpSubKey
    )
    {
        return ::RegDeleteKeyA(
            hKey,
            lpSubKey
        );
    }

    LSTATUS DeleteKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey
    )
    {
        return ::RegDeleteKeyW(
            hKey,
            lpSubKey
        );
    }

    LSTATUS EnumKeyA(
        HKEY hKey,
        DWORD dwIndex,
        LPSTR lpName,
        DWORD cchName
    )
    {
        return ::RegEnumKeyA(
            hKey,
            dwIndex,
            lpName,
            cchName
        );
    }

    LSTATUS EnumKeyW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        DWORD cchName
    )
    {
        return ::RegEnumKeyW(
            hKey,
            dwIndex,
            lpName,
            cchName
        );
    }

    LSTATUS SetKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    )
    {
        return ::RegSetKeyValueA(
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS SetKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName,
        DWORD dwType,
        LPCVOID lpData,
        DWORD cbData
    )
    {
        return ::RegSetKeyValueW(
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS DeleteKeyValueA(
        HKEY hKey,
        LPCSTR lpSubKey,
        LPCSTR lpValueName
    )
    {
        return ::RegDeleteKeyValueA(
            hKey,
            lpSubKey,
            lpValueName
        );
    }

    LSTATUS DeleteKeyValueW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValueName
    )
    {
        return ::RegDeleteKeyValueW(
            hKey,
            lpSubKey,
            lpValueName
        );
    }

    LSTATUS QueryMultipleValuesA(
        HKEY hKey,
        PVALENTA val_list,
        DWORD num_vals,
        LPSTR lpValueBuf,
        LPDWORD ldwTotsize
    )
    {
        return ::RegQueryMultipleValuesA(
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );
    }

    LSTATUS QueryMultipleValuesW(
        HKEY hKey,
        PVALENTW val_list,
        DWORD num_vals,
        LPWSTR lpValueBuf,
        LPDWORD ldwTotsize
    )
    {
        return ::RegQueryMultipleValuesW(
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );
    }
};
} // namespace VReg::Test
