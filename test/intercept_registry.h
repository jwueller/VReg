#pragma once

#include <vreg.h>

#include <Windows.h>

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace VReg::Test
{
/// A registry that forwards to another one, can fail chosen calls with chosen
/// statuses and reports every call it receives before forwarding it. It fills
/// the wide surface and `RegGetValueA`; every other form is derived by the
/// flat functions and reaches it as the wide form.
class InterceptRegistry
{
public:
    explicit InterceptRegistry(VRegRegistry* const pInner) noexcept :
        m_pInner(pInner),
        m_c{{&VTBL, nullptr}, this}
    {
    }

    InterceptRegistry(const InterceptRegistry&) = delete;
    InterceptRegistry& operator=(const InterceptRegistry&) = delete;

    /// Calls named here (`RegOpenKeyExW`, ...) answer the status instead of
    /// being forwarded.
    std::map<std::string, LSTATUS> failures;
    std::function<void(const std::string&)> onCall;

    operator VRegRegistry*() noexcept
    {
        return &m_c.object;
    }

    [[nodiscard]] VRegRegistry* Get() noexcept
    {
        return &m_c.object;
    }

private:
    /// The C object with the way back to its owner.
    struct CObject
    {
        VRegRegistry object;
        InterceptRegistry* pSelf;
    };

    static InterceptRegistry* Self(VRegRegistry* const pThis) noexcept
    {
        return reinterpret_cast<CObject*>(pThis)->pSelf;
    }

    std::optional<LSTATUS> Enter(const char* const pszName) const
    {
        if (onCall)
            onCall(pszName);

        const auto it = failures.find(pszName);
        if (it == failures.end())
            return std::nullopt;

        return it->second;
    }

    static LSTATUS WINAPI RegOpenKeyExW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_ const DWORD ulOptions,
        _In_ const REGSAM samDesired,
        _Out_ const PHKEY phkResult
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegOpenKeyExW"))
            return *status;
        return VRegOpenKeyExW(
            self.m_pInner,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
    }

    static LSTATUS WINAPI RegCreateKeyExW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpSubKey,
        _Reserved_ const DWORD Reserved,
        _In_opt_ const LPWSTR lpClass,
        _In_ const DWORD dwOptions,
        _In_ const REGSAM samDesired,
        _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ const PHKEY phkResult,
        _Out_opt_ const LPDWORD lpdwDisposition
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegCreateKeyExW"))
            return *status;
        return VRegCreateKeyExW(
            self.m_pInner,
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

    static LSTATUS WINAPI RegCloseKey(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey
    ) noexcept
    {
        auto& self = *Self(pThis);
        return VRegCloseKey(
            self.m_pInner,
            hKey
        );
    }

    static LSTATUS WINAPI RegQueryValueExW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpValueName,
        _Reserved_ const LPDWORD lpReserved,
        _Out_opt_ const LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegQueryValueExW"))
            return *status;
        return VRegQueryValueExW(
            self.m_pInner,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }

    static LSTATUS WINAPI RegSetValueExW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpValueName,
        _Reserved_ const DWORD Reserved,
        _In_ const DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
        _In_ const DWORD cbData
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegSetValueExW"))
            return *status;
        return VRegSetValueExW(
            self.m_pInner,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    static LSTATUS WINAPI RegDeleteValueW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpValueName
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegDeleteValueW"))
            return *status;
        return VRegDeleteValueW(
            self.m_pInner,
            hKey,
            lpValueName
        );
    }

    static LSTATUS WINAPI RegDeleteKeyExW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpSubKey,
        _In_ const REGSAM samDesired,
        _Reserved_ const DWORD Reserved
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegDeleteKeyExW"))
            return *status;
        return VRegDeleteKeyExW(
            self.m_pInner,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    static LSTATUS WINAPI RegDeleteTreeW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegDeleteTreeW"))
            return *status;
        return VRegDeleteTreeW(
            self.m_pInner,
            hKey,
            lpSubKey
        );
    }

    static LSTATUS WINAPI RegEnumKeyExW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_ const DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPWSTR lpName,
        _Inout_ const LPDWORD lpcchName,
        _Reserved_ const LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
        _Inout_opt_ const LPDWORD lpcchClass,
        _Out_opt_ const PFILETIME lpftLastWriteTime
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegEnumKeyExW"))
            return *status;
        return VRegEnumKeyExW(
            self.m_pInner,
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

    static LSTATUS WINAPI RegEnumValueW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_ const DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPWSTR lpValueName,
        _Inout_ const LPDWORD lpcchValueName,
        _Reserved_ const LPDWORD lpReserved,
        _Out_opt_ const LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
        _Inout_opt_ const LPDWORD lpcbData
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegEnumValueW"))
            return *status;
        return VRegEnumValueW(
            self.m_pInner,
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

    static LSTATUS WINAPI RegQueryInfoKeyW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
        _Inout_opt_ const LPDWORD lpcchClass,
        _Reserved_ const LPDWORD lpReserved,
        _Out_opt_ const LPDWORD lpcSubKeys,
        _Out_opt_ const LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ const LPDWORD lpcbMaxClassLen,
        _Out_opt_ const LPDWORD lpcValues,
        _Out_opt_ const LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ const LPDWORD lpcbMaxValueLen,
        _Out_opt_ const LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ const PFILETIME lpftLastWriteTime
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegQueryInfoKeyW"))
            return *status;
        return VRegQueryInfoKeyW(
            self.m_pInner,
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

    static LSTATUS WINAPI RegLoadMUIStringW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
        _In_ const DWORD cbOutBuf,
        _Out_opt_ const LPDWORD pcbData,
        _In_ const DWORD Flags,
        _In_opt_ const LPCWSTR pszDirectory
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegLoadMUIStringW"))
            return *status;
        return VRegLoadMUIStringW(
            self.m_pInner,
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
    }

    static LSTATUS WINAPI RegGetValueW(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hkey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_opt_ const LPCWSTR lpValue,
        _In_ const DWORD dwFlags,
        _Out_opt_ const LPDWORD pdwType,
        _When_(
            (dwFlags & 0x7F) == RRF_RT_REG_SZ
            || (dwFlags & 0x7F) == RRF_RT_REG_EXPAND_SZ
            || (dwFlags & 0x7F) == (RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ)
            || *pdwType == REG_SZ
            || *pdwType == REG_EXPAND_SZ,
            _Post_z_
        )
        _When_(
            (dwFlags & 0x7F) == RRF_RT_REG_MULTI_SZ
            || *pdwType == REG_MULTI_SZ,
            _Post_ _NullNull_terminated_
        )
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) const PVOID pvData,
        _Inout_opt_ const LPDWORD pcbData
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegGetValueW"))
            return *status;
        return VRegGetValueW(
            self.m_pInner,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    }

    static LSTATUS WINAPI RegNotifyChangeKeyValue(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey,
        _In_ const BOOL bWatchSubtree,
        _In_ const DWORD dwNotifyFilter,
        _In_opt_ const HANDLE hEvent,
        _In_ const BOOL fAsynchronous
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegNotifyChangeKeyValue"))
            return *status;
        return VRegNotifyChangeKeyValue(
            self.m_pInner,
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }

    static LSTATUS WINAPI RegFlushKey(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hKey
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegFlushKey"))
            return *status;
        return VRegFlushKey(
            self.m_pInner,
            hKey
        );
    }

    static LSTATUS WINAPI RegGetValueA(
        _In_ VRegRegistry* const pThis,
        _In_ const HKEY hkey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_opt_ const LPCSTR lpValue,
        _In_ const DWORD dwFlags,
        _Out_opt_ const LPDWORD pdwType,
        _When_(
            (dwFlags & 0x7F) == RRF_RT_REG_SZ
            || (dwFlags & 0x7F) == RRF_RT_REG_EXPAND_SZ
            || (dwFlags & 0x7F) == (RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ)
            || *pdwType == REG_SZ
            || *pdwType == REG_EXPAND_SZ,
            _Post_z_
        )
        _When_(
            (dwFlags & 0x7F) == RRF_RT_REG_MULTI_SZ
            || *pdwType == REG_MULTI_SZ,
            _Post_ _NullNull_terminated_
        )
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) const PVOID pvData,
        _Inout_opt_ const LPDWORD pcbData
    ) noexcept
    {
        auto& self = *Self(pThis);
        if (const auto status = self.Enter("RegGetValueA"))
            return *status;
        return VRegGetValueA(
            self.m_pInner,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    }

    static const VRegRegistryVtbl VTBL;

    VRegRegistry* m_pInner;
    CObject m_c;
};

inline const VRegRegistryVtbl InterceptRegistry::VTBL = {
    .RegOpenKeyExW = &InterceptRegistry::RegOpenKeyExW,
    .RegCreateKeyExW = &InterceptRegistry::RegCreateKeyExW,
    .RegCloseKey = &InterceptRegistry::RegCloseKey,
    .RegQueryValueExW = &InterceptRegistry::RegQueryValueExW,
    .RegSetValueExW = &InterceptRegistry::RegSetValueExW,
    .RegDeleteValueW = &InterceptRegistry::RegDeleteValueW,
    .RegDeleteKeyExW = &InterceptRegistry::RegDeleteKeyExW,
    .RegDeleteTreeW = &InterceptRegistry::RegDeleteTreeW,
    .RegEnumKeyExW = &InterceptRegistry::RegEnumKeyExW,
    .RegEnumValueW = &InterceptRegistry::RegEnumValueW,
    .RegQueryInfoKeyW = &InterceptRegistry::RegQueryInfoKeyW,
    .RegLoadMUIStringW = &InterceptRegistry::RegLoadMUIStringW,
    .RegGetValueW = &InterceptRegistry::RegGetValueW,
    .RegNotifyChangeKeyValue = &InterceptRegistry::RegNotifyChangeKeyValue,
    .RegFlushKey = &InterceptRegistry::RegFlushKey,
    .RegGetValueA = &InterceptRegistry::RegGetValueA,
};
} // namespace VReg::Test
