#pragma once

#include "objects.h"

#include <vreg.h>

#include <Windows.h>

namespace VReg::Test
{
/// The native registry with `HKEY_CURRENT_USER` standing for the root of an
/// app hive, so a `.reg` file written for the current user applies to a
/// private hive. The wide surface is filled; the other forms derive from it.
class AppHiveRegistry
{
public:
    explicit AppHiveRegistry(const HKEY hRoot) :
        m_native(MakeNativeRegistry()),
        m_hRoot(hRoot),
        m_c{{&VTBL, nullptr}, this}
    {
    }

    AppHiveRegistry(const AppHiveRegistry&) = delete;
    AppHiveRegistry& operator=(const AppHiveRegistry&) = delete;

    operator VRegRegistry*() noexcept
    {
        return &m_c.object;
    }

    [[nodiscard]] VRegRegistry* Get() noexcept
    {
        return &m_c.object;
    }

private:
    struct CObject
    {
        VRegRegistry object;
        AppHiveRegistry* pSelf;
    };

    static AppHiveRegistry* Self(VRegRegistry* const pThis) noexcept
    {
        return reinterpret_cast<CObject*>(pThis)->pSelf;
    }

    HKEY Map(const HKEY hKey) const noexcept
    {
        return hKey == HKEY_CURRENT_USER ? m_hRoot : hKey;
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
        return VRegOpenKeyExW(
            self.m_native,
            self.Map(hKey),
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
        return VRegCreateKeyExW(
            self.m_native,
            self.Map(hKey),
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
            self.m_native,
            self.Map(hKey)
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
        return VRegQueryValueExW(
            self.m_native,
            self.Map(hKey),
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
        return VRegSetValueExW(
            self.m_native,
            self.Map(hKey),
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
        return VRegDeleteValueW(
            self.m_native,
            self.Map(hKey),
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
        return VRegDeleteKeyExW(
            self.m_native,
            self.Map(hKey),
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
        return VRegDeleteTreeW(
            self.m_native,
            self.Map(hKey),
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
        return VRegEnumKeyExW(
            self.m_native,
            self.Map(hKey),
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
        return VRegEnumValueW(
            self.m_native,
            self.Map(hKey),
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
        return VRegQueryInfoKeyW(
            self.m_native,
            self.Map(hKey),
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
        return VRegLoadMUIStringW(
            self.m_native,
            self.Map(hKey),
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
        return VRegGetValueW(
            self.m_native,
            self.Map(hkey),
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
        return VRegNotifyChangeKeyValue(
            self.m_native,
            self.Map(hKey),
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
        return VRegFlushKey(
            self.m_native,
            self.Map(hKey)
        );
    }

    static const VRegRegistryVtbl VTBL;

    OwnedRegistry m_native;
    HKEY m_hRoot;
    CObject m_c;
};

inline const VRegRegistryVtbl AppHiveRegistry::VTBL = {
    .RegOpenKeyExW = &AppHiveRegistry::RegOpenKeyExW,
    .RegCreateKeyExW = &AppHiveRegistry::RegCreateKeyExW,
    .RegCloseKey = &AppHiveRegistry::RegCloseKey,
    .RegQueryValueExW = &AppHiveRegistry::RegQueryValueExW,
    .RegSetValueExW = &AppHiveRegistry::RegSetValueExW,
    .RegDeleteValueW = &AppHiveRegistry::RegDeleteValueW,
    .RegDeleteKeyExW = &AppHiveRegistry::RegDeleteKeyExW,
    .RegDeleteTreeW = &AppHiveRegistry::RegDeleteTreeW,
    .RegEnumKeyExW = &AppHiveRegistry::RegEnumKeyExW,
    .RegEnumValueW = &AppHiveRegistry::RegEnumValueW,
    .RegQueryInfoKeyW = &AppHiveRegistry::RegQueryInfoKeyW,
    .RegLoadMUIStringW = &AppHiveRegistry::RegLoadMUIStringW,
    .RegGetValueW = &AppHiveRegistry::RegGetValueW,
    .RegNotifyChangeKeyValue = &AppHiveRegistry::RegNotifyChangeKeyValue,
    .RegFlushKey = &AppHiveRegistry::RegFlushKey,
};
} // namespace VReg::Test
