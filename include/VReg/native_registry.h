#pragma once

#include "registry.h"

#include <Windows.h>

namespace VReg
{
/**
 * @brief The real registry, reached through a table of function pointers.
 *
 *  The default table is resolved from the export tables of `KernelBase.dll`
 *  and, for the pre-`Ex` forms only it exports, `advapi32.dll`, never through
 *  @ref GetProcAddress: an apphelp shim can make that answer a stub instead of
 *  the entry point. Resolution never loads a module (VReg links advapi32, so
 *  both are loaded before it runs), which makes it safe under the loader
 *  lock.
 *
 *  The table holds every function the @ref Registry surface mirrors, A and
 *  pre-`Ex` forms included, although only the virtual wide forms are called
 *  through it: the hooks attach to these fields, so once the hooks are
 *  committed the fields hold the trampolines and this registry is the one
 *  below them.
 */
class NativeRegistry final : public Registry
{
public:
    struct Functions
    {
        decltype(&::RegOpenKeyExW) RegOpenKeyExW;
        decltype(&::RegOpenKeyExA) RegOpenKeyExA;
        decltype(&::RegOpenKeyW) RegOpenKeyW;
        decltype(&::RegOpenKeyA) RegOpenKeyA;
        decltype(&::RegCreateKeyExW) RegCreateKeyExW;
        decltype(&::RegCreateKeyExA) RegCreateKeyExA;
        decltype(&::RegCreateKeyW) RegCreateKeyW;
        decltype(&::RegCreateKeyA) RegCreateKeyA;
        decltype(&::RegCloseKey) RegCloseKey;
        decltype(&::RegQueryValueExW) RegQueryValueExW;
        decltype(&::RegQueryValueExA) RegQueryValueExA;
        decltype(&::RegQueryValueW) RegQueryValueW;
        decltype(&::RegQueryValueA) RegQueryValueA;
        decltype(&::RegSetValueExW) RegSetValueExW;
        decltype(&::RegSetValueExA) RegSetValueExA;
        decltype(&::RegSetValueW) RegSetValueW;
        decltype(&::RegSetValueA) RegSetValueA;
        decltype(&::RegDeleteValueW) RegDeleteValueW;
        decltype(&::RegDeleteValueA) RegDeleteValueA;
        decltype(&::RegDeleteKeyExW) RegDeleteKeyExW;
        decltype(&::RegDeleteKeyExA) RegDeleteKeyExA;
        decltype(&::RegDeleteKeyW) RegDeleteKeyW;
        decltype(&::RegDeleteKeyA) RegDeleteKeyA;
        decltype(&::RegDeleteTreeW) RegDeleteTreeW;
        decltype(&::RegDeleteTreeA) RegDeleteTreeA;
        decltype(&::RegEnumKeyExW) RegEnumKeyExW;
        decltype(&::RegEnumKeyExA) RegEnumKeyExA;
        decltype(&::RegEnumKeyW) RegEnumKeyW;
        decltype(&::RegEnumKeyA) RegEnumKeyA;
        decltype(&::RegEnumValueW) RegEnumValueW;
        decltype(&::RegEnumValueA) RegEnumValueA;
        decltype(&::RegQueryInfoKeyW) RegQueryInfoKeyW;
        decltype(&::RegQueryInfoKeyA) RegQueryInfoKeyA;
        decltype(&::RegLoadMUIStringW) RegLoadMUIStringW;
        decltype(&::RegLoadMUIStringA) RegLoadMUIStringA;
        decltype(&::RegGetValueW) RegGetValueW;
        decltype(&::RegGetValueA) RegGetValueA;
        decltype(&::RegNotifyChangeKeyValue) RegNotifyChangeKeyValue;
        decltype(&::RegFlushKey) RegFlushKey;
        decltype(&::RegSetKeyValueW) RegSetKeyValueW;
        decltype(&::RegSetKeyValueA) RegSetKeyValueA;
        decltype(&::RegDeleteKeyValueW) RegDeleteKeyValueW;
        decltype(&::RegDeleteKeyValueA) RegDeleteKeyValueA;
        decltype(&::RegQueryMultipleValuesW) RegQueryMultipleValuesW;
        decltype(&::RegQueryMultipleValuesA) RegQueryMultipleValuesA;
    };

    /// The default table. A function neither module exports stays null.
    [[nodiscard]] static Functions ResolveFunctions() noexcept;

    /// Over @ref ResolveFunctions.
    NativeRegistry() noexcept;
    explicit NativeRegistry(const Functions& functions) noexcept;

    /// The live table: what the hooks attach to.
    [[nodiscard]] Functions& GetFunctions() noexcept;

    LSTATUS RegOpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept override;

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
    ) noexcept override;

    LSTATUS RegCloseKey(
        HKEY hKey
    ) noexcept override;

    LSTATUS RegQueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept override;

    LSTATUS RegSetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept override;

    LSTATUS RegDeleteValueW(
        HKEY hKey,
        LPCWSTR lpValueName
    ) noexcept override;

    LSTATUS RegDeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept override;

    LSTATUS RegDeleteTreeW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept override;

    LSTATUS RegEnumKeyExW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) const noexcept override;

    LSTATUS RegEnumValueW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) const noexcept override;

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
    ) const noexcept override;

    LSTATUS RegLoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    ) const noexcept override;

    LSTATUS RegGetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) const noexcept override;

    LSTATUS RegNotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept override;

    LSTATUS RegFlushKey(
        HKEY hKey
    ) noexcept override;

private:
    Functions m_functions;
};
} // namespace VReg
