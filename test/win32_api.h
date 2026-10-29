#pragma once

#include <Windows.h>

namespace VReg::Test
{
/// The real `Reg*` functions under the member names of `VReg::Registry`, so a
/// scenario template runs against the oracle and against any `Registry`
/// alike.
struct Win32Api
{
    LSTATUS RegOpenKeyExW(
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

    LSTATUS RegCreateKeyExW(
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

    LSTATUS RegCloseKey(
        HKEY hKey
    )
    {
        return ::RegCloseKey(
            hKey
        );
    }

    LSTATUS RegQueryValueExW(
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

    LSTATUS RegSetValueExW(
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

    LSTATUS RegDeleteValueW(
        HKEY hKey,
        LPCWSTR lpValueName
    )
    {
        return ::RegDeleteValueW(
            hKey,
            lpValueName
        );
    }

    LSTATUS RegDeleteKeyExW(
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

    LSTATUS RegDeleteTreeW(
        HKEY hKey,
        LPCWSTR lpSubKey
    )
    {
        return ::RegDeleteTreeW(
            hKey,
            lpSubKey
        );
    }

    LSTATUS RegEnumKeyExW(
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

    LSTATUS RegEnumValueW(
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

    LSTATUS RegQueryInfoKeyW(
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

    LSTATUS RegLoadMUIStringW(
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

    LSTATUS RegGetValueW(
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

    LSTATUS RegNotifyChangeKeyValue(
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

    LSTATUS RegFlushKey(
        HKEY hKey
    )
    {
        return ::RegFlushKey(
            hKey
        );
    }

    LSTATUS RegOpenKeyExA(
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

    LSTATUS RegCreateKeyExA(
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

    LSTATUS RegQueryValueExA(
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

    LSTATUS RegSetValueExA(
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

    LSTATUS RegDeleteValueA(
        HKEY hKey,
        LPCSTR lpValueName
    )
    {
        return ::RegDeleteValueA(
            hKey,
            lpValueName
        );
    }

    LSTATUS RegDeleteKeyExA(
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

    LSTATUS RegDeleteTreeA(
        HKEY hKey,
        LPCSTR lpSubKey
    )
    {
        return ::RegDeleteTreeA(
            hKey,
            lpSubKey
        );
    }

    LSTATUS RegEnumKeyExA(
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

    LSTATUS RegEnumValueA(
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

    LSTATUS RegQueryInfoKeyA(
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

    LSTATUS RegLoadMUIStringA(
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

    LSTATUS RegGetValueA(
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

    LSTATUS RegOpenKeyA(
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

    LSTATUS RegOpenKeyW(
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

    LSTATUS RegCreateKeyA(
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

    LSTATUS RegCreateKeyW(
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

    LSTATUS RegQueryValueA(
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

    LSTATUS RegQueryValueW(
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

    LSTATUS RegSetValueA(
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

    LSTATUS RegSetValueW(
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

    LSTATUS RegDeleteKeyA(
        HKEY hKey,
        LPCSTR lpSubKey
    )
    {
        return ::RegDeleteKeyA(
            hKey,
            lpSubKey
        );
    }

    LSTATUS RegDeleteKeyW(
        HKEY hKey,
        LPCWSTR lpSubKey
    )
    {
        return ::RegDeleteKeyW(
            hKey,
            lpSubKey
        );
    }

    LSTATUS RegEnumKeyA(
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

    LSTATUS RegEnumKeyW(
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

    LSTATUS RegSetKeyValueA(
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

    LSTATUS RegSetKeyValueW(
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

    LSTATUS RegDeleteKeyValueA(
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

    LSTATUS RegDeleteKeyValueW(
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

    LSTATUS RegQueryMultipleValuesA(
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

    LSTATUS RegQueryMultipleValuesW(
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
