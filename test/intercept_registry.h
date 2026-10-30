#pragma once

#include <VReg/registry.h>

#include <Windows.h>

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace VReg::Test
{
/// A registry that forwards to another one, can fail chosen calls with chosen
/// statuses and reports every call it receives before forwarding it.
class InterceptRegistry final : public Registry
{
public:
    explicit InterceptRegistry(Registry& inner) noexcept :
        m_inner(inner)
    {
    }

    /// Calls named here (`RegOpenKeyExW`, ...) answer the status instead of
    /// being forwarded.
    std::map<std::string, LSTATUS> failures;
    std::function<void(const std::string&)> onCall;

    LSTATUS RegOpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept override
    {
        if (const auto status = Enter("RegOpenKeyExW"))
            return *status;
        return m_inner.RegOpenKeyExW(
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
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        PHKEY phkResult,
        LPDWORD lpdwDisposition
    ) noexcept override
    {
        if (const auto status = Enter("RegCreateKeyExW"))
            return *status;
        return m_inner.RegCreateKeyExW(
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
    ) noexcept override
    {
        return m_inner.RegCloseKey(
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
    ) const noexcept override
    {
        if (const auto status = Enter("RegQueryValueExW"))
            return *status;
        return m_inner.RegQueryValueExW(
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
    ) noexcept override
    {
        if (const auto status = Enter("RegSetValueExW"))
            return *status;
        return m_inner.RegSetValueExW(
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
    ) noexcept override
    {
        if (const auto status = Enter("RegDeleteValueW"))
            return *status;
        return m_inner.RegDeleteValueW(
            hKey,
            lpValueName
        );
    }

    LSTATUS RegDeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept override
    {
        if (const auto status = Enter("RegDeleteKeyExW"))
            return *status;
        return m_inner.RegDeleteKeyExW(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    LSTATUS RegDeleteTreeW(
        HKEY hKey,
        LPCWSTR lpSubKey
    ) noexcept override
    {
        if (const auto status = Enter("RegDeleteTreeW"))
            return *status;
        return m_inner.RegDeleteTreeW(
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
    ) const noexcept override
    {
        if (const auto status = Enter("RegEnumKeyExW"))
            return *status;
        return m_inner.RegEnumKeyExW(
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
    ) const noexcept override
    {
        if (const auto status = Enter("RegEnumValueW"))
            return *status;
        return m_inner.RegEnumValueW(
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
    ) const noexcept override
    {
        if (const auto status = Enter("RegQueryInfoKeyW"))
            return *status;
        return m_inner.RegQueryInfoKeyW(
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
    ) const noexcept override
    {
        if (const auto status = Enter("RegLoadMUIStringW"))
            return *status;
        return m_inner.RegLoadMUIStringW(
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
    ) const noexcept override
    {
        if (const auto status = Enter("RegGetValueW"))
            return *status;
        return m_inner.RegGetValueW(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
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
    ) const noexcept override
    {
        if (const auto status = Enter("RegGetValueA"))
            return *status;
        return m_inner.RegGetValueA(
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
    ) noexcept override
    {
        if (const auto status = Enter("RegNotifyChangeKeyValue"))
            return *status;
        return m_inner.RegNotifyChangeKeyValue(
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }

    LSTATUS RegFlushKey(
        HKEY hKey
    ) noexcept override
    {
        if (const auto status = Enter("RegFlushKey"))
            return *status;
        return m_inner.RegFlushKey(
            hKey
        );
    }

private:
    std::optional<LSTATUS> Enter(const char* const pszName) const
    {
        if (onCall)
            onCall(pszName);

        const auto it = failures.find(pszName);
        if (it == failures.end())
            return std::nullopt;

        return it->second;
    }

    Registry& m_inner;
};
} // namespace VReg::Test
