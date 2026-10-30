#include <VReg/native_registry.h>

#include "export_table.h"

namespace VReg
{
namespace
{
/// Makes every module linking VReg import advapi32, so the loader maps it
/// before VReg runs and resolution finds it without loading anything.
// ReSharper disable once CppVariableCanBeMadeConstexpr
[[maybe_unused]] const volatile auto g_pfnImportAdvapi32 = &::RegOpenKeyW;
} // namespace

void* FindRegistryExport(const char* const pszName) noexcept
{
    static const HMODULE hKernelBase = GetModuleHandleW(L"KernelBase.dll");
    static const HMODULE hAdvapi32 = GetModuleHandleW(L"advapi32.dll");

    if (const auto pFunction = FindExport(hKernelBase, pszName))
        return pFunction;

    return FindExport(hAdvapi32, pszName);
}

NativeRegistry::Functions NativeRegistry::ResolveFunctions() noexcept
{
    Functions functions{};

#define VREG_RESOLVE(name) functions.name = reinterpret_cast<decltype(functions.name)>(FindRegistryExport(#name));
    VREG_REGISTRY_FUNCTIONS(VREG_RESOLVE)
#undef VREG_RESOLVE

    return functions;
}

NativeRegistry::NativeRegistry() noexcept :
    m_functions(ResolveFunctions())
{
}

NativeRegistry::NativeRegistry(const Functions& functions) noexcept :
    m_functions(functions)
{
}

NativeRegistry::Functions& NativeRegistry::GetFunctions() noexcept
{
    return m_functions;
}

LSTATUS NativeRegistry::RegOpenKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const DWORD ulOptions,
    const REGSAM samDesired,
    const PHKEY phkResult
) noexcept
{
    return m_functions.RegOpenKeyExW(
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

LSTATUS NativeRegistry::RegCreateKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const DWORD Reserved,
    const LPWSTR lpClass,
    const DWORD dwOptions,
    const REGSAM samDesired,
    const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    const PHKEY phkResult,
    const LPDWORD lpdwDisposition
) noexcept
{
    return m_functions.RegCreateKeyExW(
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

LSTATUS NativeRegistry::RegCloseKey(
    const HKEY hKey
) noexcept
{
    return m_functions.RegCloseKey(
        hKey
    );
}

LSTATUS NativeRegistry::RegQueryValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) const noexcept
{
    return m_functions.RegQueryValueExW(
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

LSTATUS NativeRegistry::RegSetValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const DWORD Reserved,
    const DWORD dwType,
    const BYTE* const lpData,
    const DWORD cbData
) noexcept
{
    return m_functions.RegSetValueExW(
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS NativeRegistry::RegDeleteValueW(
    const HKEY hKey,
    const LPCWSTR lpValueName
) noexcept
{
    return m_functions.RegDeleteValueW(
        hKey,
        lpValueName
    );
}

LSTATUS NativeRegistry::RegDeleteKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const REGSAM samDesired,
    const DWORD Reserved
) noexcept
{
    return m_functions.RegDeleteKeyExW(
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

LSTATUS NativeRegistry::RegDeleteTreeW(
    const HKEY hKey,
    const LPCWSTR lpSubKey
) noexcept
{
    return m_functions.RegDeleteTreeW(
        hKey,
        lpSubKey
    );
}

LSTATUS NativeRegistry::RegEnumKeyExW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpName,
    const LPDWORD lpcchName,
    const LPDWORD lpReserved,
    const LPWSTR lpClass,
    const LPDWORD lpcchClass,
    const PFILETIME lpftLastWriteTime
) const noexcept
{
    return m_functions.RegEnumKeyExW(
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

LSTATUS NativeRegistry::RegEnumValueW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpValueName,
    const LPDWORD lpcchValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) const noexcept
{
    return m_functions.RegEnumValueW(
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

LSTATUS NativeRegistry::RegQueryInfoKeyW(
    const HKEY hKey,
    const LPWSTR lpClass,
    const LPDWORD lpcchClass,
    const LPDWORD lpReserved,
    const LPDWORD lpcSubKeys,
    const LPDWORD lpcbMaxSubKeyLen,
    const LPDWORD lpcbMaxClassLen,
    const LPDWORD lpcValues,
    const LPDWORD lpcbMaxValueNameLen,
    const LPDWORD lpcbMaxValueLen,
    const LPDWORD lpcbSecurityDescriptor,
    const PFILETIME lpftLastWriteTime
) const noexcept
{
    return m_functions.RegQueryInfoKeyW(
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

LSTATUS NativeRegistry::RegLoadMUIStringW(
    const HKEY hKey,
    const LPCWSTR pszValue,
    const LPWSTR pszOutBuf,
    const DWORD cbOutBuf,
    const LPDWORD pcbData,
    const DWORD Flags,
    const LPCWSTR pszDirectory
) const noexcept
{
    return m_functions.RegLoadMUIStringW(
        hKey,
        pszValue,
        pszOutBuf,
        cbOutBuf,
        pcbData,
        Flags,
        pszDirectory
    );
}

LSTATUS NativeRegistry::RegGetValueW(
    const HKEY hkey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    const PVOID pvData,
    const LPDWORD pcbData
) const noexcept
{
    return m_functions.RegGetValueW(
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

LSTATUS NativeRegistry::RegNotifyChangeKeyValue(
    const HKEY hKey,
    const BOOL bWatchSubtree,
    const DWORD dwNotifyFilter,
    const HANDLE hEvent,
    const BOOL fAsynchronous
) noexcept
{
    return m_functions.RegNotifyChangeKeyValue(
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

LSTATUS NativeRegistry::RegFlushKey(
    const HKEY hKey
) noexcept
{
    return m_functions.RegFlushKey(
        hKey
    );
}
} // namespace VReg
