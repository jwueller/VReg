#pragma once

#include "registry.h"

#include <Windows.h>

/// Applies `X` to the name of every function the @ref VReg::Registry surface
/// mirrors, A and pre-`Ex` forms included: the fields of
/// @ref VReg::NativeRegistry::Functions, and the functions the hooks emulate.
/// It serves VReg's own sources and tests; it is not part of the API.
#define VREG_INTERNAL_REGISTRY_FUNCTIONS(X)                                    \
    X(RegOpenKeyExW)                                                           \
    X(RegOpenKeyExA)                                                           \
    X(RegOpenKeyW)                                                             \
    X(RegOpenKeyA)                                                             \
    X(RegCreateKeyExW)                                                         \
    X(RegCreateKeyExA)                                                         \
    X(RegCreateKeyW)                                                           \
    X(RegCreateKeyA)                                                           \
    X(RegCloseKey)                                                             \
    X(RegQueryValueExW)                                                        \
    X(RegQueryValueExA)                                                        \
    X(RegQueryValueW)                                                          \
    X(RegQueryValueA)                                                          \
    X(RegSetValueExW)                                                          \
    X(RegSetValueExA)                                                          \
    X(RegSetValueW)                                                            \
    X(RegSetValueA)                                                            \
    X(RegDeleteValueW)                                                         \
    X(RegDeleteValueA)                                                         \
    X(RegDeleteKeyExW)                                                         \
    X(RegDeleteKeyExA)                                                         \
    X(RegDeleteKeyW)                                                           \
    X(RegDeleteKeyA)                                                           \
    X(RegDeleteTreeW)                                                          \
    X(RegDeleteTreeA)                                                          \
    X(RegEnumKeyExW)                                                           \
    X(RegEnumKeyExA)                                                           \
    X(RegEnumKeyW)                                                             \
    X(RegEnumKeyA)                                                             \
    X(RegEnumValueW)                                                           \
    X(RegEnumValueA)                                                           \
    X(RegQueryInfoKeyW)                                                        \
    X(RegQueryInfoKeyA)                                                        \
    X(RegLoadMUIStringW)                                                       \
    X(RegLoadMUIStringA)                                                       \
    X(RegGetValueW)                                                            \
    X(RegGetValueA)                                                            \
    X(RegNotifyChangeKeyValue)                                                 \
    X(RegFlushKey)                                                             \
    X(RegSetKeyValueW)                                                         \
    X(RegSetKeyValueA)                                                         \
    X(RegDeleteKeyValueW)                                                      \
    X(RegDeleteKeyValueA)                                                      \
    X(RegQueryMultipleValuesW)                                                 \
    X(RegQueryMultipleValuesA)

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
#define VREG_FIELD(name) decltype(&::name) name;
    struct Functions
    {
        VREG_INTERNAL_REGISTRY_FUNCTIONS(VREG_FIELD)
    };
#undef VREG_FIELD

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
