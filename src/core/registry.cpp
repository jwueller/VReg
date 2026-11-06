#include <VReg/registry.h>

namespace VReg
{
namespace
{
Registry* OwnerOf(VRegRegistry* const pThis) noexcept
{
    return reinterpret_cast<Registry::CObjectData*>(pThis)->pOwner;
}

LSTATUS WINAPI ForwardRegOpenKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
) noexcept
{
    return OwnerOf(pThis)->RegOpenKeyExW(
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

LSTATUS WINAPI ForwardRegCreateKeyExW(
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
    return OwnerOf(pThis)->RegCreateKeyExW(
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

LSTATUS WINAPI ForwardRegCloseKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
) noexcept
{
    return OwnerOf(pThis)->RegCloseKey(
        hKey
    );
}

LSTATUS WINAPI ForwardRegQueryValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
) noexcept
{
    return OwnerOf(pThis)->RegQueryValueExW(
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

LSTATUS WINAPI ForwardRegSetValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
) noexcept
{
    return OwnerOf(pThis)->RegSetValueExW(
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS WINAPI ForwardRegDeleteValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
) noexcept
{
    return OwnerOf(pThis)->RegDeleteValueW(
        hKey,
        lpValueName
    );
}

LSTATUS WINAPI ForwardRegDeleteKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
) noexcept
{
    return OwnerOf(pThis)->RegDeleteKeyExW(
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

LSTATUS WINAPI ForwardRegDeleteTreeW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
) noexcept
{
    return OwnerOf(pThis)->RegDeleteTreeW(
        hKey,
        lpSubKey
    );
}

LSTATUS WINAPI ForwardRegEnumKeyExW(
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
    return OwnerOf(pThis)->RegEnumKeyExW(
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

LSTATUS WINAPI ForwardRegEnumValueW(
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
    return OwnerOf(pThis)->RegEnumValueW(
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

LSTATUS WINAPI ForwardRegQueryInfoKeyW(
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
    return OwnerOf(pThis)->RegQueryInfoKeyW(
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

LSTATUS WINAPI ForwardRegLoadMUIStringW(
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
    return OwnerOf(pThis)->RegLoadMUIStringW(
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
// carry it through the virtual call.
#pragma warning(suppress: 6054)
LSTATUS WINAPI ForwardRegGetValueW(
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
    return OwnerOf(pThis)->RegGetValueW(
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

LSTATUS WINAPI ForwardRegNotifyChangeKeyValue(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
) noexcept
{
    return OwnerOf(pThis)->RegNotifyChangeKeyValue(
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

LSTATUS WINAPI ForwardRegFlushKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
) noexcept
{
    return OwnerOf(pThis)->RegFlushKey(
        hKey
    );
}
} // namespace

const VRegRegistryVtbl Registry::s_cVtbl = {
    .RegOpenKeyExW = &ForwardRegOpenKeyExW,
    .RegCreateKeyExW = &ForwardRegCreateKeyExW,
    .RegCloseKey = &ForwardRegCloseKey,
    .RegQueryValueExW = &ForwardRegQueryValueExW,
    .RegSetValueExW = &ForwardRegSetValueExW,
    .RegDeleteValueW = &ForwardRegDeleteValueW,
    .RegDeleteKeyExW = &ForwardRegDeleteKeyExW,
    .RegDeleteTreeW = &ForwardRegDeleteTreeW,
    .RegEnumKeyExW = &ForwardRegEnumKeyExW,
    .RegEnumValueW = &ForwardRegEnumValueW,
    .RegQueryInfoKeyW = &ForwardRegQueryInfoKeyW,
    .RegLoadMUIStringW = &ForwardRegLoadMUIStringW,
    .RegGetValueW = &ForwardRegGetValueW,
    .RegNotifyChangeKeyValue = &ForwardRegNotifyChangeKeyValue,
    .RegFlushKey = &ForwardRegFlushKey,
};

Registry::Registry() noexcept :
    m_cObject{.object = {&s_cVtbl, nullptr}, .pOwner = this}
{
}

VRegRegistry* Registry::CObject() noexcept
{
    return &m_cObject.object;
}

LSTATUS Registry::RegOpenKeyExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
) noexcept
{
    return VRegOpenKeyExA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

LSTATUS Registry::RegCreateKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition
) noexcept
{
    return VRegCreateKeyExA(
        const_cast<Registry*>(this)->CObject(),
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

LSTATUS Registry::RegQueryValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
) const noexcept
{
    return VRegQueryValueExA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

LSTATUS Registry::RegSetValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
) noexcept
{
    return VRegSetValueExA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS Registry::RegDeleteValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
) noexcept
{
    return VRegDeleteValueA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpValueName
    );
}

LSTATUS Registry::RegDeleteKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
) noexcept
{
    return VRegDeleteKeyExA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

LSTATUS Registry::RegDeleteTreeA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
) noexcept
{
    return VRegDeleteTreeA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey
    );
}

LSTATUS Registry::RegEnumKeyExA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
) const noexcept
{
    return VRegEnumKeyExA(
        const_cast<Registry*>(this)->CObject(),
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

LSTATUS Registry::RegEnumValueA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
) const noexcept
{
    return VRegEnumValueA(
        const_cast<Registry*>(this)->CObject(),
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

LSTATUS Registry::RegQueryInfoKeyA(
    _In_ const HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPSTR lpClass,
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
    return VRegQueryInfoKeyA(
        const_cast<Registry*>(this)->CObject(),
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

LSTATUS Registry::RegLoadMUIStringA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
) const noexcept
{
    return VRegLoadMUIStringA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        pszValue,
        pszOutBuf,
        cbOutBuf,
        pcbData,
        Flags,
        pszDirectory
    );
}

// The SDK's _Post_z_ holds on every return; the analyzer does not carry it
// through the flat function.
#pragma warning(suppress: 6054)
LSTATUS Registry::RegGetValueA(
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
) const noexcept
{
    return VRegGetValueA(
        const_cast<Registry*>(this)->CObject(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

LSTATUS Registry::RegOpenKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    return VRegOpenKeyA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        phkResult
    );
}

LSTATUS Registry::RegOpenKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    return VRegOpenKeyW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        phkResult
    );
}

LSTATUS Registry::RegCreateKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    return VRegCreateKeyA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        phkResult
    );
}

LSTATUS Registry::RegCreateKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    return VRegCreateKeyW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        phkResult
    );
}

LSTATUS Registry::RegQueryValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
) noexcept
{
    return VRegQueryValueA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

LSTATUS Registry::RegQueryValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
) noexcept
{
    return VRegQueryValueW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

LSTATUS Registry::RegSetValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
) noexcept
{
    return VRegSetValueA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS Registry::RegSetValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
) noexcept
{
    return VRegSetValueW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS Registry::RegDeleteKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
) noexcept
{
    return VRegDeleteKeyA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey
    );
}

LSTATUS Registry::RegDeleteKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
) noexcept
{
    return VRegDeleteKeyW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey
    );
}

LSTATUS Registry::RegEnumKeyA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
) const noexcept
{
    return VRegEnumKeyA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

LSTATUS Registry::RegEnumKeyW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
) const noexcept
{
    return VRegEnumKeyW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

LSTATUS Registry::RegSetKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
) noexcept
{
    return VRegSetKeyValueA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS Registry::RegSetKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
) noexcept
{
    return VRegSetKeyValueW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

LSTATUS Registry::RegDeleteKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
) noexcept
{
    return VRegDeleteKeyValueA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

LSTATUS Registry::RegDeleteKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
) noexcept
{
    return VRegDeleteKeyValueW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

LSTATUS Registry::RegQueryMultipleValuesA(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
) const noexcept
{
    return VRegQueryMultipleValuesA(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        val_list,
        num_vals,
        lpValueBuf,
        ldwTotsize
    );
}

LSTATUS Registry::RegQueryMultipleValuesW(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
) const noexcept
{
    return VRegQueryMultipleValuesW(
        const_cast<Registry*>(this)->CObject(),
        hKey,
        val_list,
        num_vals,
        lpValueBuf,
        ldwTotsize
    );
}
} // namespace VReg
