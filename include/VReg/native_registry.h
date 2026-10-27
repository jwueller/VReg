#pragma once

#include <VReg/registry.h>

#include <Windows.h>

namespace VReg
{
/**
 * @brief The real registry, reached through a table of function pointers.
 *
 *  The default table is resolved from the export tables of `KernelBase.dll`
 *  and, for the pre-`Ex` forms only it exports, `advapi32.dll`, never through
 *  `GetProcAddress`: an apphelp shim can make that answer a stub instead of
 *  the entry point. Resolution never loads a module (VReg links advapi32, so
 *  both are loaded before it runs), which makes it safe under the loader
 *  lock.
 *
 *  The table holds every function the @ref Registry surface mirrors, A and
 *  pre-`Ex` forms included, although only the virtual W forms are called
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

    LSTATUS OpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) noexcept
        override;
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
    ) noexcept override;
    LSTATUS CloseKey(HKEY hKey) noexcept override;
    LSTATUS QueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept override;
    LSTATUS SetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept override;
    LSTATUS DeleteValueW(HKEY hKey, LPCWSTR lpValueName) noexcept override;
    LSTATUS DeleteKeyExW(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved) noexcept override;
    LSTATUS DeleteTreeW(HKEY hKey, LPCWSTR lpSubKey) noexcept override;
    LSTATUS EnumKeyExW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) noexcept override;
    LSTATUS EnumValueW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept override;
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
    ) noexcept override;
    LSTATUS LoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    ) noexcept override;
    LSTATUS GetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) noexcept override;
    LSTATUS NotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept override;
    LSTATUS FlushKey(HKEY hKey) noexcept override;

private:
    Functions m_functions;
};
} // namespace VReg
