#include <VReg/detours.h>

#include <VReg/native_registry.h>

#include "core/ansi.h"
#include "core/buffer.h"
#include "core/export_table.h"
#include "core/handle_space.h"

#include <detours/detours.h>

#include <stdlib.h>

// Detours takes hook bodies and trampolines as data pointers, and the thunks
// are bytes; the conversions below are what its API is made of.
#pragma warning(disable: 4054 4055 4152)

/// The registry every hook calls through.
static VRegRegistry* volatile g_pRegistry;

static VRegRegistry* VReg__HookRegistry(void)
{
    return (VRegRegistry*)ReadPointerAcquire((PVOID volatile*)&g_pRegistry);
}

// --- The hooks the registry answers: the same call, made on the registry ---

static LSTATUS APIENTRY VReg__HookRegOpenKeyExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyExW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS APIENTRY VReg__HookRegCreateKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPWSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition
)
{
    return VRegCreateKeyExW(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegCloseKey(
    _In_ const HKEY hKey
)
{
    return VRegCloseKey(
        VReg__HookRegistry(),
        hKey
    );
}

static LSTATUS APIENTRY VReg__HookRegQueryValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VRegQueryValueExW(
        VReg__HookRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg__HookRegSetValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueExW(
        VReg__HookRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VRegDeleteValueW(
        VReg__HookRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VRegDeleteKeyExW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteTreeW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    return VRegDeleteTreeW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg__HookRegEnumKeyExW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPWSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
)
{
    return VRegEnumKeyExW(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegEnumValueW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPWSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
)
{
    return VRegEnumValueW(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegQueryInfoKeyW(
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
)
{
    return VRegQueryInfoKeyW(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegLoadMUIStringW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    return VRegLoadMUIStringW(
        VReg__HookRegistry(),
        hKey,
        pszValue,
        pszOutBuf,
        cbOutBuf,
        pcbData,
        Flags,
        pszDirectory
    );
}

// The SDK's _Post_z_ on pvData holds on every return; the analyzer does not
// carry it through the flat function.
#pragma warning(suppress: 6054)
static LSTATUS APIENTRY VReg__HookRegGetValueW(
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
)
{
    return VRegGetValueW(
        VReg__HookRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS APIENTRY VReg__HookRegNotifyChangeKeyValue(
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    return VRegNotifyChangeKeyValue(
        VReg__HookRegistry(),
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

static LSTATUS APIENTRY VReg__HookRegFlushKey(
    _In_ const HKEY hKey
)
{
    return VRegFlushKey(
        VReg__HookRegistry(),
        hKey
    );
}

static LSTATUS APIENTRY VReg__HookRegOpenKeyExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyExA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS APIENTRY VReg__HookRegCreateKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition
)
{
    return VRegCreateKeyExA(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegQueryValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VRegQueryValueExA(
        VReg__HookRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg__HookRegSetValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueExA(
        VReg__HookRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VRegDeleteValueA(
        VReg__HookRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VRegDeleteKeyExA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteTreeA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    return VRegDeleteTreeA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg__HookRegEnumKeyExA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
)
{
    return VRegEnumKeyExA(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegEnumValueA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
)
{
    return VRegEnumValueA(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegQueryInfoKeyA(
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
)
{
    return VRegQueryInfoKeyA(
        VReg__HookRegistry(),
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

static LSTATUS APIENTRY VReg__HookRegLoadMUIStringA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
)
{
    return VRegLoadMUIStringA(
        VReg__HookRegistry(),
        hKey,
        pszValue,
        pszOutBuf,
        cbOutBuf,
        pcbData,
        Flags,
        pszDirectory
    );
}

// The SDK's _Post_z_ on pvData holds on every return; the analyzer does not
// carry it through the flat function.
#pragma warning(suppress: 6054)
static LSTATUS APIENTRY VReg__HookRegGetValueA(
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
)
{
    return VRegGetValueA(
        VReg__HookRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS APIENTRY VReg__HookRegOpenKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg__HookRegOpenKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg__HookRegCreateKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegCreateKeyA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg__HookRegCreateKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegCreateKeyW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg__HookRegQueryValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VRegQueryValueA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg__HookRegQueryValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VRegQueryValueW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg__HookRegSetValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg__HookRegSetValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    return VRegDeleteKeyA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    return VRegDeleteKeyW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg__HookRegEnumKeyA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    return VRegEnumKeyA(
        VReg__HookRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS APIENTRY VReg__HookRegEnumKeyW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    return VRegEnumKeyW(
        VReg__HookRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS APIENTRY VReg__HookRegSetKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetKeyValueA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg__HookRegSetKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetKeyValueW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VRegDeleteKeyValueA(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg__HookRegDeleteKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VRegDeleteKeyValueW(
        VReg__HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg__HookRegQueryMultipleValuesA(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VRegQueryMultipleValuesA(
        VReg__HookRegistry(),
        hKey,
        val_list,
        num_vals,
        lpValueBuf,
        ldwTotsize
    );
}

static LSTATUS APIENTRY VReg__HookRegQueryMultipleValuesW(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VRegQueryMultipleValuesW(
        VReg__HookRegistry(),
        hKey,
        val_list,
        num_vals,
        lpValueBuf,
        ldwTotsize
    );
}

// --- The hooks nothing virtual can answer ---

// The functions refused on virtual handles, by index into their resolved
// entry points (the trampolines once the hooks are committed).
#define VREG__REFUSED_HOOKS(X) \
    X(RegSaveKeyA) \
    X(RegSaveKeyW) \
    X(RegSaveKeyExA) \
    X(RegSaveKeyExW) \
    X(RegRestoreKeyA) \
    X(RegRestoreKeyW) \
    X(RegLoadKeyA) \
    X(RegLoadKeyW) \
    X(RegUnLoadKeyA) \
    X(RegUnLoadKeyW) \
    X(RegReplaceKeyA) \
    X(RegReplaceKeyW) \
    X(RegGetKeySecurity) \
    X(RegSetKeySecurity) \
    X(RegRenameKey) \
    X(RegCopyTreeA) \
    X(RegCopyTreeW) \
    X(RegCreateKeyTransactedA) \
    X(RegCreateKeyTransactedW) \
    X(RegOpenKeyTransactedA) \
    X(RegOpenKeyTransactedW) \
    X(RegDeleteKeyTransactedA) \
    X(RegDeleteKeyTransactedW) \
    X(RegDisableReflectionKey) \
    X(RegEnableReflectionKey) \
    X(RegQueryReflectionKey) \
    X(RegOverridePredefKey)

typedef enum VReg__RefusedIndex
{
#define VREG_INDEX(name) VREG__REFUSED_##name,
    VREG__REFUSED_HOOKS(VREG_INDEX)
#undef VREG_INDEX

    VREG__REFUSED_COUNT
} VReg__RefusedIndex;

static void* g_refusedTargets[VREG__REFUSED_COUNT];

// The forms the registry answers besides its wide surface, by index into
// their resolved entry points. The registry below the hooks never calls
// them (an ANSI or pre-Ex call reaches it as the wide Ex form), so their
// trampolines live here and serve VRegNeutralizeHooks alone.
typedef enum VReg__FormIndex
{
#define VREG_INDEX(name) VREG__FORM_##name,
    VREG_INTERNAL_REGISTRY_FORMS(VREG_INDEX)
#undef VREG_INDEX

    VREG__FORM_COUNT
} VReg__FormIndex;

static void* g_formTargets[VREG__FORM_COUNT];

typedef LSTATUS (APIENTRY* VReg__RegSaveKeyAFunction)(
    HKEY,
    LPCSTR,
    CONST LPSECURITY_ATTRIBUTES
);

typedef LSTATUS (APIENTRY* VReg__RegSaveKeyWFunction)(
    HKEY,
    LPCWSTR,
    CONST LPSECURITY_ATTRIBUTES
);

typedef LSTATUS (APIENTRY* VReg__RegSaveKeyExAFunction)(
    HKEY,
    LPCSTR,
    CONST LPSECURITY_ATTRIBUTES,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg__RegSaveKeyExWFunction)(
    HKEY,
    LPCWSTR,
    CONST LPSECURITY_ATTRIBUTES,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg__RegRestoreKeyAFunction)(
    HKEY,
    LPCSTR,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg__RegRestoreKeyWFunction)(
    HKEY,
    LPCWSTR,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg__RegLoadKeyAFunction)(
    HKEY,
    LPCSTR,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VReg__RegLoadKeyWFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg__RegUnLoadKeyAFunction)(
    HKEY,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VReg__RegUnLoadKeyWFunction)(
    HKEY,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg__RegReplaceKeyAFunction)(
    HKEY,
    LPCSTR,
    LPCSTR,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VReg__RegReplaceKeyWFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg__RegGetKeySecurityFunction)(
    HKEY,
    SECURITY_INFORMATION,
    PSECURITY_DESCRIPTOR,
    LPDWORD
);

typedef LSTATUS (APIENTRY* VReg__RegSetKeySecurityFunction)(
    HKEY,
    SECURITY_INFORMATION,
    PSECURITY_DESCRIPTOR
);

typedef LSTATUS (APIENTRY* VReg__RegRenameKeyFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg__RegCopyTreeAFunction)(
    HKEY,
    LPCSTR,
    HKEY
);

typedef LSTATUS (APIENTRY* VReg__RegCopyTreeWFunction)(
    HKEY,
    LPCWSTR,
    HKEY
);

typedef LSTATUS (APIENTRY* VReg__RegCreateKeyTransactedAFunction)(
    HKEY,
    LPCSTR,
    DWORD,
    LPSTR,
    DWORD,
    REGSAM,
    CONST LPSECURITY_ATTRIBUTES,
    PHKEY,
    LPDWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg__RegCreateKeyTransactedWFunction)(
    HKEY,
    LPCWSTR,
    DWORD,
    LPWSTR,
    DWORD,
    REGSAM,
    CONST LPSECURITY_ATTRIBUTES,
    PHKEY,
    LPDWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg__RegOpenKeyTransactedAFunction)(
    HKEY,
    LPCSTR,
    DWORD,
    REGSAM,
    PHKEY,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg__RegOpenKeyTransactedWFunction)(
    HKEY,
    LPCWSTR,
    DWORD,
    REGSAM,
    PHKEY,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg__RegDeleteKeyTransactedAFunction)(
    HKEY,
    LPCSTR,
    REGSAM,
    DWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg__RegDeleteKeyTransactedWFunction)(
    HKEY,
    LPCWSTR,
    REGSAM,
    DWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg__RegDisableReflectionKeyFunction)(
    HKEY
);

typedef LSTATUS (APIENTRY* VReg__RegEnableReflectionKeyFunction)(
    HKEY
);

typedef LSTATUS (APIENTRY* VReg__RegQueryReflectionKeyFunction)(
    HKEY,
    BOOL *
);

typedef LSTATUS (APIENTRY* VReg__RegOverridePredefKeyFunction)(
    HKEY,
    HKEY
);


static BOOL VReg__IsOwned(_In_opt_ const HKEY hKey)
{
    return VReg__HandleSpaceOwns(VReg__ProcessHandleSpace(), hKey);
}

/// Whether a @ref RegOpenKeyTransactedW call names no transaction, which it
/// answers with @ref ERROR_INVALID_TRANSACTION. The hook answers that
/// itself: on ARM64 the function reaches that answer through a branch back
/// into its first 12 bytes, which the hook's jump overwrites.
///
/// Answered on every architecture rather than on ARM64 alone, since the native
/// answer is the same everywhere and one path is simpler.
static BOOL VReg__IsUntransacted(
    _In_opt_ const HANDLE hTransaction,
    _In_opt_ const PVOID pExtendedParemeter
)
{
    return !pExtendedParemeter && (!hTransaction || hTransaction == INVALID_HANDLE_VALUE);
}

/**
 * @brief @ref ERROR_NOT_SUPPORTED when @p hKey plus @p pszSubKey lies in a
 *  mount of the hooked registry, as a transacted call on a virtual handle
 *  answers; @ref ERROR_SUCCESS to pass the call on.
 *
 *  The registry is asked only through its `Reg*` slots, so a wrapper in front
 *  of it needs nothing more: each prefix of the path, the key itself
 *  included, is opened, and the path is in a mount when one of them opens as
 *  a virtual handle of this copy. Every prefix is tried because the real keys
 *  above a mount point need not exist.
 *
 *  It probes by opening rather than through a query beside the `Reg*` slots,
 *  which a wrapper would have to forward and which would add a slot outside
 *  the Win32 family. Opening is read-only, and a deprecated API can afford
 *  one open per path component.
 */
static LSTATUS VReg__RefuseMountPathW(
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR pszSubKey,
    const REGSAM samDesired
)
{
    VRegRegistry* const pRegistry = VReg__HookRegistry();
    void* const pAllocContext = pRegistry->pAllocContext;
    const VReg__WStringView subKey = VReg__ViewOf(pszSubKey);
    VReg__WString prefix = {NULL, 0, 0};
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T end;
    for (end = 0; end <= subKey.cch; ++end)
    {
        HKEY hProbe = NULL;
        BOOL isVirtual;
        LSTATUS opened;
        if (end != subKey.cch && subKey.pch[end] != L'\\')
            continue;

        if (!VReg__WStringAssign(pAllocContext, &prefix, subKey.pch, end))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }

        opened = VRegOpenKeyExW(
            pRegistry,
            hKey,
            VReg__WStringText(&prefix),
            0,
            KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES),
            &hProbe
        );
        if (opened != ERROR_SUCCESS)
            continue;

        isVirtual = VReg__IsOwned(hProbe);
        // An empty path may answer hKey itself rather than a new handle.
        if (hProbe != hKey)
        {
            VRegCloseKey(
                pRegistry,
                hProbe
            );
        }
        if (isVirtual)
        {
            status = ERROR_NOT_SUPPORTED;
            break;
        }
    }

    VReg__WStringFree(pAllocContext, &prefix);
    return status;
}

static LSTATUS VReg__RefuseMountPathA(
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCSTR pszSubKey,
    const REGSAM samDesired
)
{
    void* const pAllocContext = VReg__HookRegistry()->pAllocContext;
    VReg__WideArg subKey;
    LSTATUS status = VReg__WideArgInit(pAllocContext, pszSubKey, &subKey);
    if (status == ERROR_SUCCESS)
        status = VReg__RefuseMountPathW(hKey, VReg__WideArgText(&subKey), samDesired);

    VReg__WideArgFree(pAllocContext, &subKey);
    return status;
}




























static LSTATUS APIENTRY VReg__RefuseRegSaveKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
    const VReg__RegSaveKeyAFunction pfnNext =
        (VReg__RegSaveKeyAFunction)g_refusedTargets[VREG__REFUSED_RegSaveKeyA];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes
    );
}

static LSTATUS APIENTRY VReg__RefuseRegSaveKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
    const VReg__RegSaveKeyWFunction pfnNext =
        (VReg__RegSaveKeyWFunction)g_refusedTargets[VREG__REFUSED_RegSaveKeyW];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes
    );
}

static LSTATUS APIENTRY VReg__RefuseRegSaveKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ const DWORD Flags
)
{
    const VReg__RegSaveKeyExAFunction pfnNext =
        (VReg__RegSaveKeyExAFunction)g_refusedTargets[VREG__REFUSED_RegSaveKeyExA];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes,
        Flags
    );
}

static LSTATUS APIENTRY VReg__RefuseRegSaveKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ const DWORD Flags
)
{
    const VReg__RegSaveKeyExWFunction pfnNext =
        (VReg__RegSaveKeyExWFunction)g_refusedTargets[VREG__REFUSED_RegSaveKeyExW];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes,
        Flags
    );
}

static LSTATUS APIENTRY VReg__RefuseRegRestoreKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_ const DWORD dwFlags
)
{
    const VReg__RegRestoreKeyAFunction pfnNext =
        (VReg__RegRestoreKeyAFunction)g_refusedTargets[VREG__REFUSED_RegRestoreKeyA];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        dwFlags
    );
}

static LSTATUS APIENTRY VReg__RefuseRegRestoreKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_ const DWORD dwFlags
)
{
    const VReg__RegRestoreKeyWFunction pfnNext =
        (VReg__RegRestoreKeyWFunction)g_refusedTargets[VREG__REFUSED_RegRestoreKeyW];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        dwFlags
    );
}

static LSTATUS APIENTRY VReg__RefuseRegLoadKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const LPCSTR lpFile
)
{
    const VReg__RegLoadKeyAFunction pfnNext =
        (VReg__RegLoadKeyAFunction)g_refusedTargets[VREG__REFUSED_RegLoadKeyA];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpFile
    );
}

static LSTATUS APIENTRY VReg__RefuseRegLoadKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const LPCWSTR lpFile
)
{
    const VReg__RegLoadKeyWFunction pfnNext =
        (VReg__RegLoadKeyWFunction)g_refusedTargets[VREG__REFUSED_RegLoadKeyW];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpFile
    );
}

static LSTATUS APIENTRY VReg__RefuseRegUnLoadKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    const VReg__RegUnLoadKeyAFunction pfnNext =
        (VReg__RegUnLoadKeyAFunction)g_refusedTargets[VREG__REFUSED_RegUnLoadKeyA];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg__RefuseRegUnLoadKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    const VReg__RegUnLoadKeyWFunction pfnNext =
        (VReg__RegUnLoadKeyWFunction)g_refusedTargets[VREG__REFUSED_RegUnLoadKeyW];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg__RefuseRegReplaceKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const LPCSTR lpNewFile,
    _In_ const LPCSTR lpOldFile
)
{
    const VReg__RegReplaceKeyAFunction pfnNext =
        (VReg__RegReplaceKeyAFunction)g_refusedTargets[VREG__REFUSED_RegReplaceKeyA];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpNewFile,
        lpOldFile
    );
}

static LSTATUS APIENTRY VReg__RefuseRegReplaceKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const LPCWSTR lpNewFile,
    _In_ const LPCWSTR lpOldFile
)
{
    const VReg__RegReplaceKeyWFunction pfnNext =
        (VReg__RegReplaceKeyWFunction)g_refusedTargets[VREG__REFUSED_RegReplaceKeyW];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpNewFile,
        lpOldFile
    );
}

static LSTATUS APIENTRY VReg__RefuseRegGetKeySecurity(
    _In_ const HKEY hKey,
    _In_ const SECURITY_INFORMATION SecurityInformation,
    _Out_writes_bytes_opt_(*lpcbSecurityDescriptor) const PSECURITY_DESCRIPTOR pSecurityDescriptor,
    _Inout_ const LPDWORD lpcbSecurityDescriptor
)
{
    const VReg__RegGetKeySecurityFunction pfnNext =
        (VReg__RegGetKeySecurityFunction)g_refusedTargets[VREG__REFUSED_RegGetKeySecurity];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        SecurityInformation,
        pSecurityDescriptor,
        lpcbSecurityDescriptor
    );
}

static LSTATUS APIENTRY VReg__RefuseRegSetKeySecurity(
    _In_ const HKEY hKey,
    _In_ const SECURITY_INFORMATION SecurityInformation,
    _In_ const PSECURITY_DESCRIPTOR pSecurityDescriptor
)
{
    const VReg__RegSetKeySecurityFunction pfnNext =
        (VReg__RegSetKeySecurityFunction)g_refusedTargets[VREG__REFUSED_RegSetKeySecurity];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        SecurityInformation,
        pSecurityDescriptor
    );
}

static LSTATUS APIENTRY VReg__RefuseRegRenameKey(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKeyName,
    _In_ const LPCWSTR lpNewKeyName
)
{
    const VReg__RegRenameKeyFunction pfnNext =
        (VReg__RegRenameKeyFunction)g_refusedTargets[VREG__REFUSED_RegRenameKey];
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKeyName,
        lpNewKeyName
    );
}

static LSTATUS APIENTRY VReg__RefuseRegCopyTreeA(
    _In_ const HKEY hKeySrc,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const HKEY hKeyDest
)
{
    const VReg__RegCopyTreeAFunction pfnNext =
        (VReg__RegCopyTreeAFunction)g_refusedTargets[VREG__REFUSED_RegCopyTreeA];
    if (VReg__IsOwned(hKeySrc) || VReg__IsOwned(hKeyDest))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKeySrc,
        lpSubKey,
        hKeyDest
    );
}

static LSTATUS APIENTRY VReg__RefuseRegCopyTreeW(
    _In_ const HKEY hKeySrc,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const HKEY hKeyDest
)
{
    const VReg__RegCopyTreeWFunction pfnNext =
        (VReg__RegCopyTreeWFunction)g_refusedTargets[VREG__REFUSED_RegCopyTreeW];
    if (VReg__IsOwned(hKeySrc) || VReg__IsOwned(hKeyDest))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKeySrc,
        lpSubKey,
        hKeyDest
    );
}

static LSTATUS APIENTRY VReg__RefuseRegCreateKeyTransactedA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VReg__RegCreateKeyTransactedAFunction pfnNext =
        (VReg__RegCreateKeyTransactedAFunction)g_refusedTargets[VREG__REFUSED_RegCreateKeyTransactedA];
    LSTATUS status;
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg__RefuseMountPathA(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;

    return pfnNext(
        hKey,
        lpSubKey,
        Reserved,
        lpClass,
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition,
        hTransaction,
        pExtendedParemeter
    );
}

static LSTATUS APIENTRY VReg__RefuseRegCreateKeyTransactedW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPWSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VReg__RegCreateKeyTransactedWFunction pfnNext =
        (VReg__RegCreateKeyTransactedWFunction)g_refusedTargets[VREG__REFUSED_RegCreateKeyTransactedW];
    LSTATUS status;
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg__RefuseMountPathW(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;

    return pfnNext(
        hKey,
        lpSubKey,
        Reserved,
        lpClass,
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition,
        hTransaction,
        pExtendedParemeter
    );
}

static LSTATUS APIENTRY VReg__RefuseRegOpenKeyTransactedA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VReg__RegOpenKeyTransactedAFunction pfnNext =
        (VReg__RegOpenKeyTransactedAFunction)g_refusedTargets[VREG__REFUSED_RegOpenKeyTransactedA];
    LSTATUS status;
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg__RefuseMountPathA(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;

    return pfnNext(
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult,
        hTransaction,
        pExtendedParemeter
    );
}

static LSTATUS APIENTRY VReg__RefuseRegOpenKeyTransactedW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VReg__RegOpenKeyTransactedWFunction pfnNext =
        (VReg__RegOpenKeyTransactedWFunction)g_refusedTargets[VREG__REFUSED_RegOpenKeyTransactedW];
    LSTATUS status;
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg__RefuseMountPathW(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;
    if (VReg__IsUntransacted(hTransaction, pExtendedParemeter))
        return ERROR_INVALID_TRANSACTION;

    return pfnNext(
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult,
        hTransaction,
        pExtendedParemeter
    );
}

static LSTATUS APIENTRY VReg__RefuseRegDeleteKeyTransactedA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParameter
)
{
    const VReg__RegDeleteKeyTransactedAFunction pfnNext =
        (VReg__RegDeleteKeyTransactedAFunction)g_refusedTargets[VREG__REFUSED_RegDeleteKeyTransactedA];
    LSTATUS status;
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg__RefuseMountPathA(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;

    return pfnNext(
        hKey,
        lpSubKey,
        samDesired,
        Reserved,
        hTransaction,
        pExtendedParameter
    );
}

static LSTATUS APIENTRY VReg__RefuseRegDeleteKeyTransactedW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParameter
)
{
    const VReg__RegDeleteKeyTransactedWFunction pfnNext =
        (VReg__RegDeleteKeyTransactedWFunction)g_refusedTargets[VREG__REFUSED_RegDeleteKeyTransactedW];
    LSTATUS status;
    if (VReg__IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg__RefuseMountPathW(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;

    return pfnNext(
        hKey,
        lpSubKey,
        samDesired,
        Reserved,
        hTransaction,
        pExtendedParameter
    );
}

static LSTATUS APIENTRY VReg__RefuseRegDisableReflectionKey(
    _In_ const HKEY hBase
)
{
    const VReg__RegDisableReflectionKeyFunction pfnNext =
        (VReg__RegDisableReflectionKeyFunction)g_refusedTargets[VREG__REFUSED_RegDisableReflectionKey];
    if (VReg__IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase
    );
}

static LSTATUS APIENTRY VReg__RefuseRegEnableReflectionKey(
    _In_ const HKEY hBase
)
{
    const VReg__RegEnableReflectionKeyFunction pfnNext =
        (VReg__RegEnableReflectionKeyFunction)g_refusedTargets[VREG__REFUSED_RegEnableReflectionKey];
    if (VReg__IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase
    );
}

static LSTATUS APIENTRY VReg__RefuseRegQueryReflectionKey(
    _In_ const HKEY hBase,
    _Out_ BOOL * const bIsReflectionDisabled
)
{
    const VReg__RegQueryReflectionKeyFunction pfnNext =
        (VReg__RegQueryReflectionKeyFunction)g_refusedTargets[VREG__REFUSED_RegQueryReflectionKey];
    if (VReg__IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase,
        bIsReflectionDisabled
    );
}

static LSTATUS APIENTRY VReg__RefuseRegOverridePredefKey(
    _In_ const HKEY hKey,
    _In_opt_ const HKEY hNewHKey
)
{
    const VReg__RegOverridePredefKeyFunction pfnNext =
        (VReg__RegOverridePredefKeyFunction)g_refusedTargets[VREG__REFUSED_RegOverridePredefKey];
    if (VReg__IsOwned(hKey) || VReg__IsOwned(hNewHKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        hNewHKey
    );
}

// --- The thunks ---

/**
 * @brief The thunks every hook detours to: `jmp [slot]`, in memory outside
 *  any module image so they outlive the module that made them.
 *
 *  Code and slots sit on two pages of one allocation, so an x64 or ARM64
 *  thunk can reach its slot PC-relative. The memory is leaked on purpose:
 *  another hooker's trampoline may jump into a thunk after this module is
 *  gone.
 */
#define VREG__THUNK_CODE_SIZE ((SIZE_T)16)
#define VREG__THUNK_PAGE_SIZE ((SIZE_T)4096)
#define VREG__THUNK_CAPACITY (VREG__THUNK_PAGE_SIZE / VREG__THUNK_CODE_SIZE)

static BYTE* g_pbThunks;

static void** VReg__ThunkSlot(const SIZE_T index)
{
    return (void**)(g_pbThunks + VREG__THUNK_PAGE_SIZE) + index;
}

/// Writes thunk @p index, initially jumping to @p pTarget, and answers its
/// address.
static void* VReg__WriteThunk(const SIZE_T index, _In_ void* const pTarget)
{
    BYTE* const pbCode = g_pbThunks + index * VREG__THUNK_CODE_SIZE;
    void** const ppSlot = VReg__ThunkSlot(index);
    *ppSlot = pTarget;

#ifdef _M_ARM64EC
#error "ARM64EC is not supported: its x64 and native callers need hooks of their own."
#endif

#ifdef _M_ARM64
    {
        // ldr x16, slot (PC-relative, in words); br x16.
        const INT_PTR offset = (INT_PTR)ppSlot - (INT_PTR)pbCode;
        const UINT32 ldr = 0x58000010u | ((UINT32)(offset / 4) << 5);
        const UINT32 br = 0xD61F0200u;
        CopyMemory(pbCode, &ldr, sizeof(ldr));
        CopyMemory(pbCode + 4, &br, sizeof(br));
    }
#else
    // jmp qword/dword ptr [slot]: RIP-relative on x64, absolute on x86.
    pbCode[0] = 0xFF;
    pbCode[1] = 0x25;
#ifdef _M_X64
    {
        const INT32 displacement = (INT32)((INT_PTR)ppSlot - (INT_PTR)(pbCode + 6));
        CopyMemory(pbCode + 2, &displacement, sizeof(displacement));
    }
#else
    {
        const UINT32 address = (UINT32)(UINT_PTR)ppSlot;
        CopyMemory(pbCode + 2, &address, sizeof(address));
    }
#endif
#endif
    return pbCode;
}

/// Where Detours keeps each hooked entry point and, once committed, its
/// trampoline.
static void** g_ppTargets[VREG__THUNK_CAPACITY];
static volatile LONG g_cHooks;

typedef struct VReg__Hook
{
    void** ppTarget;
    void* pBody;
} VReg__Hook;

VREGDEF LONG WINAPI VRegAttachHooks(_In_ VRegRegistry* const pRegistry, _In_ VRegRegistry* const pNative)
{
    static volatile LONG s_isAttached;
    VRegNativeFunctions* functions;
    VReg__Hook hooks[VREG__THUNK_CAPACITY];
    void* thunks[VREG__THUNK_CAPACITY];
    SIZE_T cHooks = 0;
    SIZE_T i;
    DWORD dwOld = 0;

    if (InterlockedExchange(&s_isAttached, 1))
        return ERROR_ALREADY_INITIALIZED;

    WritePointerRelease((PVOID volatile*)&g_pRegistry, pRegistry);
    functions = VRegGetNativeFunctions(pNative);

#define VREG_EMULATED(name) \
    hooks[cHooks].ppTarget = (void**)&functions->name; \
    hooks[cHooks].pBody = (void*)&VReg__Hook##name; \
    ++cHooks;
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_EMULATED)
#undef VREG_EMULATED

    // A form this system does not export has nothing to hook.
#define VREG_FORM(name) \
    g_formTargets[VREG__FORM_##name] = VReg__FindRegistryExport(#name); \
    if (g_formTargets[VREG__FORM_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_formTargets[VREG__FORM_##name]; \
        hooks[cHooks].pBody = (void*)&VReg__Hook##name; \
        ++cHooks; \
    }
    VREG_INTERNAL_REGISTRY_FORMS(VREG_FORM)
#undef VREG_FORM

    // A refused function this system does not export has nothing to hook.
#define VREG_REFUSED(name) \
    g_refusedTargets[VREG__REFUSED_##name] = VReg__FindRegistryExport(#name); \
    if (g_refusedTargets[VREG__REFUSED_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_refusedTargets[VREG__REFUSED_##name]; \
        hooks[cHooks].pBody = (void*)&VReg__Refuse##name; \
        ++cHooks; \
    }
    VREG__REFUSED_HOOKS(VREG_REFUSED)
#undef VREG_REFUSED

    g_pbThunks =
        (BYTE*)VirtualAlloc(NULL, 2 * VREG__THUNK_PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_pbThunks)
    {
        InterlockedExchange(&s_isAttached, 0);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    for (i = 0; i < cHooks; ++i)
    {
        g_ppTargets[i] = hooks[i].ppTarget;
        thunks[i] = VReg__WriteThunk(i, hooks[i].pBody);
    }

    // The code page becomes executable and read-only.
    if (!VirtualProtect(g_pbThunks, VREG__THUNK_PAGE_SIZE, PAGE_EXECUTE_READ, &dwOld))
    {
        InterlockedExchange(&s_isAttached, 0);
        return (LONG)GetLastError();
    }
    FlushInstructionCache(GetCurrentProcess(), g_pbThunks, VREG__THUNK_PAGE_SIZE);

    WriteRelease(&g_cHooks, (LONG)cHooks);
    for (i = 0; i < cHooks; ++i)
    {
        // The caller aborts the transaction on failure, so nothing is hooked
        // and a later attempt may start over (with fresh thunks).
        const LONG error = DetourAttach(hooks[i].ppTarget, thunks[i]);
        if (error != NO_ERROR)
        {
            WriteRelease(&g_cHooks, 0);
            InterlockedExchange(&s_isAttached, 0);
            return error;
        }
    }

    // Agent's choice: the hooks neutralize themselves when the module that
    // holds their bodies unloads (or the process exits) through the CRT's exit
    // table, which runs for a DLL at its detach. A TLS callback would avoid
    // the CRT but is MSVC-specific and far less plain.
    atexit(VRegNeutralizeHooks);
    return NO_ERROR;
}

VREGDEF LONG WINAPI VRegInstallHooks(_In_ VRegRegistry* const pRegistry, _In_ VRegRegistry* const pNative)
{
    LONG error = DetourTransactionBegin();
    if (error != NO_ERROR)
        return error;

    error = DetourUpdateThread(GetCurrentThread());
    if (error == NO_ERROR)
        error = VRegAttachHooks(pRegistry, pNative);

    if (error != NO_ERROR)
    {
        DetourTransactionAbort();
        return error;
    }

    return DetourTransactionCommit();
}

VREGDEF void WINAPI VRegNeutralizeHooks(void)
{
    // Whatever the target field holds is what lies below this layer: the
    // trampoline once committed, the function itself before.
    const LONG cHooks = ReadAcquire(&g_cHooks);
    LONG i;
    for (i = 0; i < cHooks; ++i)
        InterlockedExchangePointer(VReg__ThunkSlot((SIZE_T)i), *g_ppTargets[i]);
}

#undef VREG__REFUSED_HOOKS
