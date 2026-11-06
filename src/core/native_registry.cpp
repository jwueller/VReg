#include <VReg/native_registry.h>

#include <new>

namespace VReg
{
NativeRegistry::Functions NativeRegistry::ResolveFunctions() noexcept
{
    Functions functions;
    VRegResolveNativeFunctions(&functions);
    return functions;
}

NativeRegistry::NativeRegistry() :
    m_pRegistry(nullptr)
{
    if (VRegCreateNativeRegistry(nullptr, nullptr, &m_pRegistry) != ERROR_SUCCESS)
        throw std::bad_alloc();
}

NativeRegistry::NativeRegistry(const Functions& functions) :
    m_pRegistry(nullptr)
{
    if (VRegCreateNativeRegistry(&functions, nullptr, &m_pRegistry) != ERROR_SUCCESS)
        throw std::bad_alloc();
}

NativeRegistry::~NativeRegistry()
{
    VRegDestroyRegistry(m_pRegistry);
}

NativeRegistry::Functions& NativeRegistry::GetFunctions() noexcept
{
    return *VRegGetNativeFunctions(m_pRegistry);
}

VRegRegistry* NativeRegistry::CObject() noexcept
{
    return m_pRegistry;
}

LSTATUS NativeRegistry::RegOpenKeyExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
) noexcept
{
    return VRegOpenKeyExW(
        m_pRegistry,
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

LSTATUS NativeRegistry::RegCreateKeyExW(
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
    return VRegCreateKeyExW(
        m_pRegistry,
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
    _In_ const HKEY hKey
) noexcept
{
    return VRegCloseKey(
        m_pRegistry,
        hKey
    );
}

LSTATUS NativeRegistry::RegQueryValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
) const noexcept
{
    return VRegQueryValueExW(
        m_pRegistry,
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

LSTATUS NativeRegistry::RegSetValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
) noexcept
{
    return VRegSetValueExW(
        m_pRegistry,
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS NativeRegistry::RegDeleteValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
) noexcept
{
    return VRegDeleteValueW(
        m_pRegistry,
        hKey,
        lpValueName
    );
}

LSTATUS NativeRegistry::RegDeleteKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
) noexcept
{
    return VRegDeleteKeyExW(
        m_pRegistry,
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

LSTATUS NativeRegistry::RegDeleteTreeW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
) noexcept
{
    return VRegDeleteTreeW(
        m_pRegistry,
        hKey,
        lpSubKey
    );
}

LSTATUS NativeRegistry::RegEnumKeyExW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPWSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
) const noexcept
{
    return VRegEnumKeyExW(
        m_pRegistry,
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
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPWSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
) const noexcept
{
    return VRegEnumValueW(
        m_pRegistry,
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
) const noexcept
{
    return VRegQueryInfoKeyW(
        m_pRegistry,
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
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
) const noexcept
{
    return VRegLoadMUIStringW(
        m_pRegistry,
        hKey,
        pszValue,
        pszOutBuf,
        cbOutBuf,
        pcbData,
        Flags,
        pszDirectory
    );
}

// The slot's _Post_z_ on pvData holds on every return; the analyzer does not
// carry it through the flat function.
#pragma warning(suppress: 6054)
LSTATUS NativeRegistry::RegGetValueW(
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
) const noexcept
{
    return VRegGetValueW(
        m_pRegistry,
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
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
) noexcept
{
    return VRegNotifyChangeKeyValue(
        m_pRegistry,
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

LSTATUS NativeRegistry::RegFlushKey(
    _In_ const HKEY hKey
) noexcept
{
    return VRegFlushKey(
        m_pRegistry,
        hKey
    );
}
} // namespace VReg
