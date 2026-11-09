#pragma once

#include "objects.h"

#include <vreg.h>

#include <Windows.h>

namespace VRegTest
{
/// The real `Reg*` functions behind a @ref VReg with every slot
/// filled, so a scenario runs against the oracle and against any registry
/// alike. Filled from this module's imports: exactly the functions an
/// application calls.
class Win32Registry
{
public:
    Win32Registry() :
        m_registry(MakeCustomRegistry(&VTBL, nullptr))
    {
    }

    Win32Registry(const Win32Registry&) = delete;
    Win32Registry& operator=(const Win32Registry&) = delete;

    operator VReg*() const noexcept
    {
        return m_registry;
    }

    [[nodiscard]] VReg* Get() const noexcept
    {
        return m_registry;
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
        (void)pThis;
        return ::RegOpenKeyExW(
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
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
        (void)pThis;
        return ::RegCreateKeyExW(
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
        _In_ VReg* const pThis,
        _In_ const HKEY hKey
    ) noexcept
    {
        (void)pThis;
        return ::RegCloseKey(
            hKey
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
        (void)pThis;
        return ::RegQueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
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
        (void)pThis;
        return ::RegSetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    static LSTATUS WINAPI RegDeleteValueW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpValueName
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteValueW(
            hKey,
            lpValueName
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
        (void)pThis;
        return ::RegDeleteKeyExW(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    static LSTATUS WINAPI RegDeleteTreeW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteTreeW(
            hKey,
            lpSubKey
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
        (void)pThis;
        return ::RegEnumKeyExW(
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
        (void)pThis;
        return ::RegEnumValueW(
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
        (void)pThis;
        return ::RegQueryInfoKeyW(
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
        (void)pThis;
        return ::RegLoadMUIStringW(
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
        (void)pThis;
        return ::RegGetValueW(
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
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const BOOL bWatchSubtree,
        _In_ const DWORD dwNotifyFilter,
        _In_opt_ const HANDLE hEvent,
        _In_ const BOOL fAsynchronous
    ) noexcept
    {
        (void)pThis;
        return ::RegNotifyChangeKeyValue(
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }

    static LSTATUS WINAPI RegFlushKey(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey
    ) noexcept
    {
        (void)pThis;
        return ::RegFlushKey(
            hKey
        );
    }

    static LSTATUS WINAPI RegOpenKeyExA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_ const DWORD ulOptions,
        _In_ const REGSAM samDesired,
        _Out_ const PHKEY phkResult
    ) noexcept
    {
        (void)pThis;
        return ::RegOpenKeyExA(
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
    }

    static LSTATUS WINAPI RegCreateKeyExA(
        _In_ VReg* const pThis,
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
        (void)pThis;
        return ::RegCreateKeyExA(
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

    static LSTATUS WINAPI RegQueryValueExA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpValueName,
        _Reserved_ const LPDWORD lpReserved,
        _Out_opt_ const LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
    ) noexcept
    {
        (void)pThis;
        return ::RegQueryValueExA(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }

    static LSTATUS WINAPI RegSetValueExA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpValueName,
        _Reserved_ const DWORD Reserved,
        _In_ const DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
        _In_ const DWORD cbData
    ) noexcept
    {
        (void)pThis;
        return ::RegSetValueExA(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    static LSTATUS WINAPI RegDeleteValueA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpValueName
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteValueA(
            hKey,
            lpValueName
        );
    }

    static LSTATUS WINAPI RegDeleteKeyExA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const LPCSTR lpSubKey,
        _In_ const REGSAM samDesired,
        _Reserved_ const DWORD Reserved
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteKeyExA(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    static LSTATUS WINAPI RegDeleteTreeA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteTreeA(
            hKey,
            lpSubKey
        );
    }

    static LSTATUS WINAPI RegEnumKeyExA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPSTR lpName,
        _Inout_ const LPDWORD lpcchName,
        _Reserved_ const LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPSTR lpClass,
        _Inout_opt_ const LPDWORD lpcchClass,
        _Out_opt_ const PFILETIME lpftLastWriteTime
    ) noexcept
    {
        (void)pThis;
        return ::RegEnumKeyExA(
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

    static LSTATUS WINAPI RegEnumValueA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPSTR lpValueName,
        _Inout_ const LPDWORD lpcchValueName,
        _Reserved_ const LPDWORD lpReserved,
        _Out_opt_ const LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
        _Inout_opt_ const LPDWORD lpcbData
    ) noexcept
    {
        (void)pThis;
        return ::RegEnumValueA(
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

    static LSTATUS WINAPI RegQueryInfoKeyA(
        _In_ VReg* const pThis,
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
    ) noexcept
    {
        (void)pThis;
        return ::RegQueryInfoKeyA(
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

    static LSTATUS WINAPI RegLoadMUIStringA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
        _In_ const DWORD cbOutBuf,
        _Out_opt_ const LPDWORD pcbData,
        _In_ const DWORD Flags,
        _In_opt_ const LPCSTR pszDirectory
    ) noexcept
    {
        (void)pThis;
        return ::RegLoadMUIStringA(
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
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
        (void)pThis;
        return ::RegGetValueA(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    }

    static LSTATUS WINAPI RegOpenKeyA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _Out_ const PHKEY phkResult
    ) noexcept
    {
        (void)pThis;
        return ::RegOpenKeyA(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    static LSTATUS WINAPI RegOpenKeyW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _Out_ const PHKEY phkResult
    ) noexcept
    {
        (void)pThis;
        return ::RegOpenKeyW(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    static LSTATUS WINAPI RegCreateKeyA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _Out_ const PHKEY phkResult
    ) noexcept
    {
        (void)pThis;
        return ::RegCreateKeyA(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    static LSTATUS WINAPI RegCreateKeyW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _Out_ const PHKEY phkResult
    ) noexcept
    {
        (void)pThis;
        return ::RegCreateKeyW(
            hKey,
            lpSubKey,
            phkResult
        );
    }

    static LSTATUS WINAPI RegQueryValueA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
        _Inout_opt_ const PLONG lpcbData
    ) noexcept
    {
        (void)pThis;
        return ::RegQueryValueA(
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );
    }

    static LSTATUS WINAPI RegQueryValueW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
        _Inout_opt_ const PLONG lpcbData
    ) noexcept
    {
        (void)pThis;
        return ::RegQueryValueW(
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );
    }

    static LSTATUS WINAPI RegSetValueA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_ const DWORD dwType,
        _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
        _In_ const DWORD cbData
    ) noexcept
    {
        (void)pThis;
        return ::RegSetValueA(
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );
    }

    static LSTATUS WINAPI RegSetValueW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_ const DWORD dwType,
        _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
        _In_ const DWORD cbData
    ) noexcept
    {
        (void)pThis;
        return ::RegSetValueW(
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );
    }

    static LSTATUS WINAPI RegDeleteKeyA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const LPCSTR lpSubKey
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteKeyA(
            hKey,
            lpSubKey
        );
    }

    static LSTATUS WINAPI RegDeleteKeyW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpSubKey
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteKeyW(
            hKey,
            lpSubKey
        );
    }

    static LSTATUS WINAPI RegEnumKeyA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const DWORD dwIndex,
        _Out_writes_opt_(cchName) const LPSTR lpName,
        _In_ const DWORD cchName
    ) noexcept
    {
        (void)pThis;
        return ::RegEnumKeyA(
            hKey,
            dwIndex,
            lpName,
            cchName
        );
    }

    static LSTATUS WINAPI RegEnumKeyW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_ const DWORD dwIndex,
        _Out_writes_opt_(cchName) const LPWSTR lpName,
        _In_ const DWORD cchName
    ) noexcept
    {
        (void)pThis;
        return ::RegEnumKeyW(
            hKey,
            dwIndex,
            lpName,
            cchName
        );
    }

    static LSTATUS WINAPI RegSetKeyValueA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_opt_ const LPCSTR lpValueName,
        _In_ const DWORD dwType,
        _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
        _In_ const DWORD cbData
    ) noexcept
    {
        (void)pThis;
        return ::RegSetKeyValueA(
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );
    }

    static LSTATUS WINAPI RegSetKeyValueW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_opt_ const LPCWSTR lpValueName,
        _In_ const DWORD dwType,
        _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
        _In_ const DWORD cbData
    ) noexcept
    {
        (void)pThis;
        return ::RegSetKeyValueW(
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );
    }

    static LSTATUS WINAPI RegDeleteKeyValueA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_opt_ const LPCSTR lpValueName
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteKeyValueA(
            hKey,
            lpSubKey,
            lpValueName
        );
    }

    static LSTATUS WINAPI RegDeleteKeyValueW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_opt_ const LPCWSTR lpValueName
    ) noexcept
    {
        (void)pThis;
        return ::RegDeleteKeyValueW(
            hKey,
            lpSubKey,
            lpValueName
        );
    }

    static LSTATUS WINAPI RegQueryMultipleValuesA(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _Out_writes_(num_vals) const PVALENTA val_list,
        _In_ const DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
        _Inout_opt_ const LPDWORD ldwTotsize
    ) noexcept
    {
        (void)pThis;
        return ::RegQueryMultipleValuesA(
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );
    }

    static LSTATUS WINAPI RegQueryMultipleValuesW(
        _In_ VReg* const pThis,
        _In_ const HKEY hKey,
        _Out_writes_(num_vals) const PVALENTW val_list,
        _In_ const DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
        _Inout_opt_ const LPDWORD ldwTotsize
    ) noexcept
    {
        (void)pThis;
        return ::RegQueryMultipleValuesW(
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );
    }

    static const VRegVtbl VTBL;

    OwnedRegistry m_registry;
};

inline const VRegVtbl Win32Registry::VTBL = {
    .RegOpenKeyA = &Win32Registry::RegOpenKeyA,
    .RegOpenKeyW = &Win32Registry::RegOpenKeyW,
    .RegOpenKeyExA = &Win32Registry::RegOpenKeyExA,
    .RegOpenKeyExW = &Win32Registry::RegOpenKeyExW,
    .RegCreateKeyA = &Win32Registry::RegCreateKeyA,
    .RegCreateKeyW = &Win32Registry::RegCreateKeyW,
    .RegCreateKeyExA = &Win32Registry::RegCreateKeyExA,
    .RegCreateKeyExW = &Win32Registry::RegCreateKeyExW,
    .RegCloseKey = &Win32Registry::RegCloseKey,
    .RegQueryValueA = &Win32Registry::RegQueryValueA,
    .RegQueryValueW = &Win32Registry::RegQueryValueW,
    .RegQueryValueExA = &Win32Registry::RegQueryValueExA,
    .RegQueryValueExW = &Win32Registry::RegQueryValueExW,
    .RegGetValueA = &Win32Registry::RegGetValueA,
    .RegGetValueW = &Win32Registry::RegGetValueW,
    .RegQueryMultipleValuesA = &Win32Registry::RegQueryMultipleValuesA,
    .RegQueryMultipleValuesW = &Win32Registry::RegQueryMultipleValuesW,
    .RegSetValueA = &Win32Registry::RegSetValueA,
    .RegSetValueW = &Win32Registry::RegSetValueW,
    .RegSetValueExA = &Win32Registry::RegSetValueExA,
    .RegSetValueExW = &Win32Registry::RegSetValueExW,
    .RegSetKeyValueA = &Win32Registry::RegSetKeyValueA,
    .RegSetKeyValueW = &Win32Registry::RegSetKeyValueW,
    .RegDeleteValueA = &Win32Registry::RegDeleteValueA,
    .RegDeleteValueW = &Win32Registry::RegDeleteValueW,
    .RegDeleteKeyValueA = &Win32Registry::RegDeleteKeyValueA,
    .RegDeleteKeyValueW = &Win32Registry::RegDeleteKeyValueW,
    .RegLoadMUIStringA = &Win32Registry::RegLoadMUIStringA,
    .RegLoadMUIStringW = &Win32Registry::RegLoadMUIStringW,
    .RegDeleteKeyA = &Win32Registry::RegDeleteKeyA,
    .RegDeleteKeyW = &Win32Registry::RegDeleteKeyW,
    .RegDeleteKeyExA = &Win32Registry::RegDeleteKeyExA,
    .RegDeleteKeyExW = &Win32Registry::RegDeleteKeyExW,
    .RegDeleteTreeA = &Win32Registry::RegDeleteTreeA,
    .RegDeleteTreeW = &Win32Registry::RegDeleteTreeW,
    .RegFlushKey = &Win32Registry::RegFlushKey,
    .RegEnumKeyA = &Win32Registry::RegEnumKeyA,
    .RegEnumKeyW = &Win32Registry::RegEnumKeyW,
    .RegEnumKeyExA = &Win32Registry::RegEnumKeyExA,
    .RegEnumKeyExW = &Win32Registry::RegEnumKeyExW,
    .RegEnumValueA = &Win32Registry::RegEnumValueA,
    .RegEnumValueW = &Win32Registry::RegEnumValueW,
    .RegQueryInfoKeyA = &Win32Registry::RegQueryInfoKeyA,
    .RegQueryInfoKeyW = &Win32Registry::RegQueryInfoKeyW,
    .RegNotifyChangeKeyValue = &Win32Registry::RegNotifyChangeKeyValue,
};
} // namespace VRegTest
