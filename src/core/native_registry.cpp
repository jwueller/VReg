#include <VReg/native_registry.h>

#include "export_table.h"

namespace VReg
{
namespace
{
// Makes every module linking VReg import advapi32, so the loader maps it
// before VReg runs and resolution finds it without loading anything.
[[maybe_unused]] const volatile auto g_pfnImportAdvapi32 = &::RegOpenKeyW;

const void* FindRegistryExport(const char* pszName) noexcept
{
    static const HMODULE hKernelBase = GetModuleHandleW(L"KernelBase.dll");
    static const HMODULE hAdvapi32 = GetModuleHandleW(L"advapi32.dll");

    if (const auto pFunction = FindExport(hKernelBase, pszName))
        return pFunction;

    return FindExport(hAdvapi32, pszName);
}
} // namespace

NativeRegistry::Functions NativeRegistry::ResolveFunctions() noexcept
{
    Functions functions{};

#define VREG_RESOLVE(name) functions.name = reinterpret_cast<decltype(functions.name)>(FindRegistryExport(#name))
    VREG_RESOLVE(RegOpenKeyExW);
    VREG_RESOLVE(RegOpenKeyExA);
    VREG_RESOLVE(RegOpenKeyW);
    VREG_RESOLVE(RegOpenKeyA);
    VREG_RESOLVE(RegCreateKeyExW);
    VREG_RESOLVE(RegCreateKeyExA);
    VREG_RESOLVE(RegCreateKeyW);
    VREG_RESOLVE(RegCreateKeyA);
    VREG_RESOLVE(RegCloseKey);
    VREG_RESOLVE(RegQueryValueExW);
    VREG_RESOLVE(RegQueryValueExA);
    VREG_RESOLVE(RegQueryValueW);
    VREG_RESOLVE(RegQueryValueA);
    VREG_RESOLVE(RegSetValueExW);
    VREG_RESOLVE(RegSetValueExA);
    VREG_RESOLVE(RegSetValueW);
    VREG_RESOLVE(RegSetValueA);
    VREG_RESOLVE(RegDeleteValueW);
    VREG_RESOLVE(RegDeleteValueA);
    VREG_RESOLVE(RegDeleteKeyExW);
    VREG_RESOLVE(RegDeleteKeyExA);
    VREG_RESOLVE(RegDeleteKeyW);
    VREG_RESOLVE(RegDeleteKeyA);
    VREG_RESOLVE(RegDeleteTreeW);
    VREG_RESOLVE(RegDeleteTreeA);
    VREG_RESOLVE(RegEnumKeyExW);
    VREG_RESOLVE(RegEnumKeyExA);
    VREG_RESOLVE(RegEnumKeyW);
    VREG_RESOLVE(RegEnumKeyA);
    VREG_RESOLVE(RegEnumValueW);
    VREG_RESOLVE(RegEnumValueA);
    VREG_RESOLVE(RegQueryInfoKeyW);
    VREG_RESOLVE(RegQueryInfoKeyA);
    VREG_RESOLVE(RegLoadMUIStringW);
    VREG_RESOLVE(RegLoadMUIStringA);
    VREG_RESOLVE(RegGetValueW);
    VREG_RESOLVE(RegGetValueA);
    VREG_RESOLVE(RegNotifyChangeKeyValue);
    VREG_RESOLVE(RegFlushKey);
    VREG_RESOLVE(RegSetKeyValueW);
    VREG_RESOLVE(RegSetKeyValueA);
    VREG_RESOLVE(RegDeleteKeyValueW);
    VREG_RESOLVE(RegDeleteKeyValueA);
    VREG_RESOLVE(RegQueryMultipleValuesW);
    VREG_RESOLVE(RegQueryMultipleValuesA);
#undef VREG_RESOLVE

    return functions;
}

NativeRegistry::NativeRegistry() noexcept
    : m_functions(ResolveFunctions())
{
}

NativeRegistry::NativeRegistry(const Functions& functions) noexcept
    : m_functions(functions)
{
}

NativeRegistry::Functions& NativeRegistry::GetFunctions() noexcept
{
    return m_functions;
}

LSTATUS NativeRegistry::OpenKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const DWORD ulOptions,
    const REGSAM samDesired,
    const PHKEY phkResult
) noexcept
{
    return m_functions.RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS NativeRegistry::CreateKeyExW(
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

LSTATUS NativeRegistry::CloseKey(const HKEY hKey) noexcept
{
    return m_functions.RegCloseKey(hKey);
}

LSTATUS NativeRegistry::QueryValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) noexcept
{
    return m_functions.RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LSTATUS NativeRegistry::SetValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const DWORD Reserved,
    const DWORD dwType,
    const BYTE* const lpData,
    const DWORD cbData
) noexcept
{
    return m_functions.RegSetValueExW(hKey, lpValueName, Reserved, dwType, lpData, cbData);
}

LSTATUS NativeRegistry::DeleteValueW(const HKEY hKey, const LPCWSTR lpValueName) noexcept
{
    return m_functions.RegDeleteValueW(hKey, lpValueName);
}

LSTATUS NativeRegistry::DeleteKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const REGSAM samDesired,
    const DWORD Reserved
) noexcept
{
    return m_functions.RegDeleteKeyExW(hKey, lpSubKey, samDesired, Reserved);
}

LSTATUS NativeRegistry::DeleteTreeW(const HKEY hKey, const LPCWSTR lpSubKey) noexcept
{
    return m_functions.RegDeleteTreeW(hKey, lpSubKey);
}

LSTATUS NativeRegistry::EnumKeyExW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpName,
    const LPDWORD lpcchName,
    const LPDWORD lpReserved,
    const LPWSTR lpClass,
    const LPDWORD lpcchClass,
    const PFILETIME lpftLastWriteTime
) noexcept
{
    return m_functions.RegEnumKeyExW(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime);
}

LSTATUS NativeRegistry::EnumValueW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpValueName,
    const LPDWORD lpcchValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) noexcept
{
    return m_functions.RegEnumValueW(hKey, dwIndex, lpValueName, lpcchValueName, lpReserved, lpType, lpData, lpcbData);
}

LSTATUS NativeRegistry::QueryInfoKeyW(
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
) noexcept
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

LSTATUS NativeRegistry::LoadMUIStringW(
    const HKEY hKey,
    const LPCWSTR pszValue,
    const LPWSTR pszOutBuf,
    const DWORD cbOutBuf,
    const LPDWORD pcbData,
    const DWORD Flags,
    const LPCWSTR pszDirectory
) noexcept
{
    return m_functions.RegLoadMUIStringW(hKey, pszValue, pszOutBuf, cbOutBuf, pcbData, Flags, pszDirectory);
}

LSTATUS NativeRegistry::GetValueW(
    const HKEY hkey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    const PVOID pvData,
    const LPDWORD pcbData
) noexcept
{
    return m_functions.RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LSTATUS NativeRegistry::NotifyChangeKeyValue(
    const HKEY hKey,
    const BOOL bWatchSubtree,
    const DWORD dwNotifyFilter,
    const HANDLE hEvent,
    const BOOL fAsynchronous
) noexcept
{
    return m_functions.RegNotifyChangeKeyValue(hKey, bWatchSubtree, dwNotifyFilter, hEvent, fAsynchronous);
}

LSTATUS NativeRegistry::FlushKey(const HKEY hKey) noexcept
{
    return m_functions.RegFlushKey(hKey);
}
} // namespace VReg
