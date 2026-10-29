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

    /// Calls named here (`OpenKeyExW`, ...) answer the status instead of being forwarded.
    std::map<std::string, LSTATUS> failures;
    std::function<void(const std::string&)> onCall;

    LSTATUS OpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept override
    {
        if (const auto status = Enter("OpenKeyExW"))
            return *status;
        return m_inner.OpenKeyExW(
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
    ) noexcept override
    {
        if (const auto status = Enter("CreateKeyExW"))
            return *status;
        return m_inner.CreateKeyExW(
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

    LSTATUS CloseKey(HKEY hKey) noexcept override
    {
        return m_inner.CloseKey(
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
    ) noexcept override
    {
        if (const auto status = Enter("QueryValueExW"))
            return *status;
        return m_inner.QueryValueExW(
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
    ) noexcept override
    {
        if (const auto status = Enter("SetValueExW"))
            return *status;
        return m_inner.SetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS DeleteValueW(HKEY hKey, LPCWSTR lpValueName) noexcept override
    {
        if (const auto status = Enter("DeleteValueW"))
            return *status;
        return m_inner.DeleteValueW(
            hKey,
            lpValueName
        );
    }

    LSTATUS DeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept override
    {
        if (const auto status = Enter("DeleteKeyExW"))
            return *status;
        return m_inner.DeleteKeyExW(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    LSTATUS DeleteTreeW(HKEY hKey, LPCWSTR lpSubKey) noexcept override
    {
        if (const auto status = Enter("DeleteTreeW"))
            return *status;
        return m_inner.DeleteTreeW(
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
    ) noexcept override
    {
        if (const auto status = Enter("EnumKeyExW"))
            return *status;
        return m_inner.EnumKeyExW(
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
    ) noexcept override
    {
        if (const auto status = Enter("EnumValueW"))
            return *status;
        return m_inner.EnumValueW(
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
    ) noexcept override
    {
        if (const auto status = Enter("QueryInfoKeyW"))
            return *status;
        return m_inner.QueryInfoKeyW(
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
    ) noexcept override
    {
        if (const auto status = Enter("LoadMUIStringW"))
            return *status;
        return m_inner.LoadMUIStringW(
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
    ) noexcept override
    {
        if (const auto status = Enter("GetValueW"))
            return *status;
        return m_inner.GetValueW(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
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
    ) noexcept override
    {
        if (const auto status = Enter("GetValueA"))
            return *status;
        return m_inner.GetValueA(
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
    ) noexcept override
    {
        if (const auto status = Enter("NotifyChangeKeyValue"))
            return *status;
        return m_inner.NotifyChangeKeyValue(
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }

    LSTATUS FlushKey(HKEY hKey) noexcept override
    {
        if (const auto status = Enter("FlushKey"))
            return *status;
        return m_inner.FlushKey(
            hKey
        );
    }

private:
    std::optional<LSTATUS> Enter(const char* const pszName)
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
