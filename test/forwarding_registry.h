#pragma once

#include <vreg.h>

#include <Windows.h>

#include <new>

namespace VRegTest
{
/// A custom @ref VReg that forwards every call to another registry. The wide
/// surface and `RegGetValueA` are filled, and every other form is derived by
/// the flat functions. Each slot calls `Derived::Forward(pThis, pszName,
/// hKey, call)`, which picks the registry and the key to use and runs
/// `call(pInner, hMapped)`, or answers a status instead; a derived class
/// states nothing else. The slots are spelled out here rather than generated
/// by a macro, so their SAL annotations stay in plain sight. The hook modules
/// build @ref InterceptRegistry without Catch2, so nothing here includes it.
template <class Derived>
class ForwardingRegistry
{
public:
    ForwardingRegistry()
    {
        if (VReg_CreateCustom(&VTBL, this, &m_pRegistry) != ERROR_SUCCESS)
            throw std::bad_alloc();
    }

    ~ForwardingRegistry()
    {
        VReg_Destroy(m_pRegistry);
    }

    ForwardingRegistry(const ForwardingRegistry&) = delete;
    ForwardingRegistry& operator=(const ForwardingRegistry&) = delete;

    operator VReg*() const noexcept
    {
        return m_pRegistry;
    }

    [[nodiscard]] VReg* Get() const noexcept
    {
        return m_pRegistry;
    }

protected:
    static Derived& Self(VReg* const pThis) noexcept
    {
        return *static_cast<Derived*>(
            static_cast<ForwardingRegistry*>(VReg_GetContext(pThis))
        );
    }

private:
    static LSTATUS WINAPI RegOpenKeyExW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_ const DWORD ulOptions,
        _In_ const REGSAM samDesired,
        _Out_ const PHKEY phkResult
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegOpenKeyExW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegOpenKeyExW(
                    pInner,
                    hMapped,
                    lpSubKey,
                    ulOptions,
                    samDesired,
                    phkResult
                );
            }
        );
    }

    static LSTATUS WINAPI RegCreateKeyExW(
        _In_ VReg* const pThis,
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
        return Derived::Forward(
            pThis,
            "RegCreateKeyExW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegCreateKeyExW(
                    pInner,
                    hMapped,
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
        );
    }

    static LSTATUS WINAPI RegCloseKey(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegCloseKey",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegCloseKey(
                    pInner,
                    hMapped
                );
            }
        );
    }

    static LSTATUS WINAPI RegQueryValueExW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpValueName,
        _Reserved_ const LPDWORD lpReserved,
        _Out_opt_ const LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegQueryValueExW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegQueryValueExW(
                    pInner,
                    hMapped,
                    lpValueName,
                    lpReserved,
                    lpType,
                    lpData,
                    lpcbData
                );
            }
        );
    }

    static LSTATUS WINAPI RegSetValueExW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpValueName,
        _Reserved_ const DWORD Reserved,
        _In_ const DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
        _In_ const DWORD cbData
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegSetValueExW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegSetValueExW(
                    pInner,
                    hMapped,
                    lpValueName,
                    Reserved,
                    dwType,
                    lpData,
                    cbData
                );
            }
        );
    }

    static LSTATUS WINAPI RegDeleteValueW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpValueName
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegDeleteValueW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegDeleteValueW(
                    pInner,
                    hMapped,
                    lpValueName
                );
            }
        );
    }

    static LSTATUS WINAPI RegDeleteKeyExW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpSubKey,
        _In_ const REGSAM samDesired,
        _Reserved_ const DWORD Reserved
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegDeleteKeyExW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegDeleteKeyExW(
                    pInner,
                    hMapped,
                    lpSubKey,
                    samDesired,
                    Reserved
                );
            }
        );
    }

    static LSTATUS WINAPI RegDeleteTreeW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegDeleteTreeW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegDeleteTreeW(
                    pInner,
                    hMapped,
                    lpSubKey
                );
            }
        );
    }

    static LSTATUS WINAPI RegEnumKeyExW(
        _In_ VReg* const pThis,
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
        return Derived::Forward(
            pThis,
            "RegEnumKeyExW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegEnumKeyExW(
                    pInner,
                    hMapped,
                    dwIndex,
                    lpName,
                    lpcchName,
                    lpReserved,
                    lpClass,
                    lpcchClass,
                    lpftLastWriteTime
                );
            }
        );
    }

    static LSTATUS WINAPI RegEnumValueW(
        _In_ VReg* const pThis,
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
        return Derived::Forward(
            pThis,
            "RegEnumValueW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegEnumValueW(
                    pInner,
                    hMapped,
                    dwIndex,
                    lpValueName,
                    lpcchValueName,
                    lpReserved,
                    lpType,
                    lpData,
                    lpcbData
                );
            }
        );
    }

    static LSTATUS WINAPI RegQueryInfoKeyW(
        _In_ VReg* const pThis,
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
        return Derived::Forward(
            pThis,
            "RegQueryInfoKeyW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegQueryInfoKeyW(
                    pInner,
                    hMapped,
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
        );
    }

    static LSTATUS WINAPI RegLoadMUIStringW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
        _In_ const DWORD cbOutBuf,
        _Out_opt_ const LPDWORD pcbData,
        _In_ const DWORD Flags,
        _In_opt_ const LPCWSTR pszDirectory
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegLoadMUIStringW",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegLoadMUIStringW(
                    pInner,
                    hMapped,
                    pszValue,
                    pszOutBuf,
                    cbOutBuf,
                    pcbData,
                    Flags,
                    pszDirectory
                );
            }
        );
    }

    static LSTATUS WINAPI RegGetValueW(
        _In_ VReg* const pThis,
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
        return Derived::Forward(
            pThis,
            "RegGetValueW",
            hkey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegGetValueW(
                    pInner,
                    hMapped,
                    lpSubKey,
                    lpValue,
                    dwFlags,
                    pdwType,
                    pvData,
                    pcbData
                );
            }
        );
    }

    static LSTATUS WINAPI RegNotifyChangeKeyValue(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const BOOL bWatchSubtree,
        _In_ const DWORD dwNotifyFilter,
        _In_opt_ const HANDLE hEvent,
        _In_ const BOOL fAsynchronous
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegNotifyChangeKeyValue",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegNotifyChangeKeyValue(
                    pInner,
                    hMapped,
                    bWatchSubtree,
                    dwNotifyFilter,
                    hEvent,
                    fAsynchronous
                );
            }
        );
    }

    static LSTATUS WINAPI RegFlushKey(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey
    ) noexcept
    {
        return Derived::Forward(
            pThis,
            "RegFlushKey",
            hKey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegFlushKey(
                    pInner,
                    hMapped
                );
            }
        );
    }

    static LSTATUS WINAPI RegGetValueA(
        _In_ VReg* const pThis,
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
        return Derived::Forward(
            pThis,
            "RegGetValueA",
            hkey,
            [&](VReg* const pInner, const HKEY hMapped)
            {
                return VReg_RegGetValueA(
                    pInner,
                    hMapped,
                    lpSubKey,
                    lpValue,
                    dwFlags,
                    pdwType,
                    pvData,
                    pcbData
                );
            }
        );
    }

    inline static const VRegVtbl VTBL = {
        .RegOpenKeyExW = &RegOpenKeyExW,
        .RegCreateKeyExW = &RegCreateKeyExW,
        .RegCloseKey = &RegCloseKey,
        .RegQueryValueExW = &RegQueryValueExW,
        .RegGetValueA = &RegGetValueA,
        .RegGetValueW = &RegGetValueW,
        .RegSetValueExW = &RegSetValueExW,
        .RegDeleteValueW = &RegDeleteValueW,
        .RegLoadMUIStringW = &RegLoadMUIStringW,
        .RegDeleteKeyExW = &RegDeleteKeyExW,
        .RegDeleteTreeW = &RegDeleteTreeW,
        .RegFlushKey = &RegFlushKey,
        .RegEnumKeyExW = &RegEnumKeyExW,
        .RegEnumValueW = &RegEnumValueW,
        .RegQueryInfoKeyW = &RegQueryInfoKeyW,
        .RegNotifyChangeKeyValue = &RegNotifyChangeKeyValue,
    };

    VReg* m_pRegistry = nullptr;
};
} // namespace VRegTest
