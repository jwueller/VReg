/// @file vreg.h
/// @brief A virtual Windows registry for C, as an overlay over the real one, or
///  held entirely in memory.
///
/// @details Include it from any C or C++ file. In exactly one C file per
///  module, define `VREG_IMPLEMENTATION` before including it to compile the
///  library there; it links `advapi32` by itself. `VREG_STATIC` makes every
///  function static, and `VREG_MALLOC`, `VREG_REALLOC` and `VREG_FREE` replace
///  the allocator (see "Allocation" below).
///
///  Defining `VREG_WITH_DETOURS` wherever it is included adds the hooks
///  (`VRegInstallHooks` and its companions). That needs Microsoft Detours:
///  `<detours/detours.h>` on the include path, and its library linked.
///
///  The file holds, in order: the public declarations, the hooks'
///  declarations, the private declarations, the implementation, and the
///  hooks' implementation. Names with a double underscore (`VReg__`,
///  `VREG__`) are private.
///
/// @copyright
/// @verbatim
/// MIT License
///
/// Copyright (c) 2026 Johannes Wüller
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.
/// @endverbatim

// The guards are explicit instead of `#pragma once` because the header is
// entered again in the file that defines VREG_IMPLEMENTATION after an
// earlier include without it; every later part has a guard of its own for
// the same reason.

#ifndef VREG_H_INCLUDED
#define VREG_H_INCLUDED

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================
// Linkage
// ==========================================================================

// What every VReg declaration carries: the linkage of the functions.
// Defining `VREG_STATIC` wherever the header is included (the
// implementation too) makes every function `static`, so a module can
// hold a private copy beside another module's; two copies in one module
// behave as two copies and share nothing but the handle namespace.

#ifdef VREG_STATIC
#define VREGDEF static
#else
#define VREGDEF extern
#endif

// ==========================================================================
// Handles
// ==========================================================================

/// The highest virtual handle value. Real key handles are kernel handle values
/// (multiples of 4, at most tagged in bit 1 by advapi32), and predefined roots
/// lie above this (sign-extended on x64), so an odd value at or below it is
/// never a real key handle.
///
/// The range stays 31-bit on x64 too, so code that truncates an HKEY to a
/// @ref DWORD keeps a working handle.
#define VREG_VIRTUAL_HKEY_BASE ((UINT_PTR)0x7FFFFFFF)

/// Whether @p hKey is a virtual handle: nonzero, odd and at most
/// @ref VREG_VIRTUAL_HKEY_BASE.
VREGDEF BOOL WINAPI VRegIsVirtualHKey(_In_opt_ HKEY hKey);

/// Whether @p hKey is a predefined root, @ref HKEY_CLASSES_ROOT through
/// @ref HKEY_CURRENT_USER_LOCAL_SETTINGS: `winreg.h` builds them from a
/// negative @ref LONG, sign-extended on x64, where neither a kernel handle nor
/// a virtual one ever lies.
VREGDEF BOOL WINAPI VRegIsPredefinedHKey(_In_opt_ HKEY hKey);

// ==========================================================================
// The registry function table
// ==========================================================================

typedef struct VRegRegistry VRegRegistry;
typedef struct VRegRegistryVtbl VRegRegistryVtbl;

/**
 * @brief A registry: the Win32 `Reg*` family behind a function table, in the
 *  shape of a COM object without `IUnknown`.
 *
 *  An implementation puts this struct first in its own and fills a
 *  @ref VRegRegistryVtbl; a wrapper over another registry does the same and
 *  keeps a pointer to the one it wraps. Every slot is `WINAPI` and takes the
 *  object first. Callers go through the flat functions (`VRegOpenKeyExW` and
 *  so on), never through the table directly.
 *
 * @note Once hooked, every slot may run under a foreign loader lock: an
 *  implementation MUST NOT load a module, create a thread or wait on a lock a
 *  loader-lock holder could own.
 */
struct VRegRegistry
{
    const VRegRegistryVtbl* lpVtbl;

    /// What the registry allocates with, passed to `VREG_MALLOC` and its
    /// companions; the flat functions allocate their temporaries with it too.
    void* pAllocContext;
};

/**
 * @brief One slot per form of the `Reg*` family, with the object first.
 *
 *  The wide `Ex` forms are the surface every implementation fills; the flat
 *  function of one that is null answers @ref ERROR_CALL_NOT_IMPLEMENTED. Every
 *  other slot (the ANSI forms, the pre-`Ex` forms and the conveniences Win32
 *  builds from other calls) MAY be null, in which case its flat function
 *  derives the call from the wide `Ex` forms: ANSI forms widen and narrow
 *  through @ref CP_ACP as the native ones do. A wrapper that fills such a slot
 *  sees the form its caller used, which is what a tracing wrapper needs.
 */
struct VRegRegistryVtbl
{
    /// Frees the registry and everything it owns. Null for one whose creator
    /// frees it some other way.
    void (WINAPI* Destroy)(_In_ VRegRegistry* pThis);

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
    LSTATUS (WINAPI* RegOpenKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
    LSTATUS (WINAPI* RegCreateKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPWSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regclosekey
    LSTATUS (WINAPI* RegCloseKey)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
    LSTATUS (WINAPI* RegQueryValueExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
    LSTATUS (WINAPI* RegSetValueExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
    LSTATUS (WINAPI* RegDeleteValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
    LSTATUS (WINAPI* RegDeleteKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
    LSTATUS (WINAPI* RegDeleteTreeW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
    LSTATUS (WINAPI* RegEnumKeyExW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluew
    LSTATUS (WINAPI* RegEnumValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeyw
    LSTATUS (WINAPI* RegQueryInfoKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringw
    LSTATUS (WINAPI* RegLoadMUIStringW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
    LSTATUS (WINAPI* RegGetValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hkey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
    LSTATUS (WINAPI* RegNotifyChangeKeyValue)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
    LSTATUS (WINAPI* RegFlushKey)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
    LSTATUS (WINAPI* RegOpenKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
    LSTATUS (WINAPI* RegCreateKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
    LSTATUS (WINAPI* RegQueryValueExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
    LSTATUS (WINAPI* RegSetValueExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
    LSTATUS (WINAPI* RegDeleteValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
    LSTATUS (WINAPI* RegDeleteKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
    LSTATUS (WINAPI* RegDeleteTreeA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
    LSTATUS (WINAPI* RegEnumKeyExA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluea
    LSTATUS (WINAPI* RegEnumValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeya
    LSTATUS (WINAPI* RegQueryInfoKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringa
    LSTATUS (WINAPI* RegLoadMUIStringA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCSTR pszDirectory
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
    LSTATUS (WINAPI* RegGetValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hkey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeya
    LSTATUS (WINAPI* RegOpenKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
    LSTATUS (WINAPI* RegOpenKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
    LSTATUS (WINAPI* RegCreateKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
    LSTATUS (WINAPI* RegCreateKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_ PHKEY phkResult
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
    LSTATUS (WINAPI* RegQueryValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPSTR lpData,
        _Inout_opt_ PLONG lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
    LSTATUS (WINAPI* RegQueryValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPWSTR lpData,
        _Inout_opt_ PLONG lpcbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
    LSTATUS (WINAPI* RegSetValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCSTR lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
    LSTATUS (WINAPI* RegSetValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCWSTR lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
    LSTATUS (WINAPI* RegDeleteKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
    LSTATUS (WINAPI* RegDeleteKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
    LSTATUS (WINAPI* RegEnumKeyA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_opt_(cchName) LPSTR lpName,
        _In_ DWORD cchName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
    LSTATUS (WINAPI* RegEnumKeyW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_opt_(cchName) LPWSTR lpName,
        _In_ DWORD cchName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
    LSTATUS (WINAPI* RegSetKeyValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValueName,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCVOID lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
    LSTATUS (WINAPI* RegSetKeyValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValueName,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) LPCVOID lpData,
        _In_ DWORD cbData
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
    LSTATUS (WINAPI* RegDeleteKeyValueA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCSTR lpSubKey,
        _In_opt_ LPCSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
    LSTATUS (WINAPI* RegDeleteKeyValueW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValueName
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
    LSTATUS (WINAPI* RegQueryMultipleValuesA)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_(num_vals) PVALENTA val_list,
        _In_ DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPSTR lpValueBuf,
        _Inout_opt_ LPDWORD ldwTotsize
    );

    /// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
    LSTATUS (WINAPI* RegQueryMultipleValuesW)(
        _In_ VRegRegistry* pThis,
        _In_ HKEY hKey,
        _Out_writes_(num_vals) PVALENTW val_list,
        _In_ DWORD num_vals,
        _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPWSTR lpValueBuf,
        _Inout_opt_ LPDWORD ldwTotsize
    );
};

/// Calls the `RegOpenKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
VREGDEF LSTATUS WINAPI VRegOpenKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
VREGDEF LSTATUS WINAPI VRegCreateKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _Reserved_ DWORD Reserved,
    _In_opt_ LPWSTR lpClass,
    _In_ DWORD dwOptions,
    _In_ REGSAM samDesired,
    _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ PHKEY phkResult,
    _Out_opt_ LPDWORD lpdwDisposition
);

/// Calls the `RegCloseKey` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regclosekey
VREGDEF LSTATUS WINAPI VRegCloseKey(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey
);

/// Calls the `RegQueryValueExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
VREGDEF LSTATUS WINAPI VRegQueryValueExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
);

/// Calls the `RegSetValueExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
VREGDEF LSTATUS WINAPI VRegSetValueExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Reserved_ DWORD Reserved,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteValueW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
VREGDEF LSTATUS WINAPI VRegDeleteValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName
);

/// Calls the `RegDeleteKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
VREGDEF LSTATUS WINAPI VRegDeleteKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// Calls the `RegDeleteTreeW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
VREGDEF LSTATUS WINAPI VRegDeleteTreeW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey
);

/// Calls the `RegEnumKeyExW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
VREGDEF LSTATUS WINAPI VRegEnumKeyExW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
    _Inout_ LPDWORD lpcchName,
    _Reserved_ LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegEnumValueW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluew
VREGDEF LSTATUS WINAPI VRegEnumValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
    _Inout_ LPDWORD lpcchValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _Inout_opt_ LPDWORD lpcbData
);

/// Calls the `RegQueryInfoKeyW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeyw
VREGDEF LSTATUS WINAPI VRegQueryInfoKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpcSubKeys,
    _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
    _Out_opt_ LPDWORD lpcbMaxClassLen,
    _Out_opt_ LPDWORD lpcValues,
    _Out_opt_ LPDWORD lpcbMaxValueNameLen,
    _Out_opt_ LPDWORD lpcbMaxValueLen,
    _Out_opt_ LPDWORD lpcbSecurityDescriptor,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegLoadMUIStringW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringw
VREGDEF LSTATUS WINAPI VRegLoadMUIStringW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
    _In_ DWORD cbOutBuf,
    _Out_opt_ LPDWORD pcbData,
    _In_ DWORD Flags,
    _In_opt_ LPCWSTR pszDirectory
);

/// Calls the `RegGetValueW` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
VREGDEF LSTATUS WINAPI VRegGetValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hkey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValue,
    _In_ DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
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
    _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
    _Inout_opt_ LPDWORD pcbData
);

/// Calls the `RegNotifyChangeKeyValue` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
VREGDEF LSTATUS WINAPI VRegNotifyChangeKeyValue(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ BOOL bWatchSubtree,
    _In_ DWORD dwNotifyFilter,
    _In_opt_ HANDLE hEvent,
    _In_ BOOL fAsynchronous
);

/// Calls the `RegFlushKey` slot of @p pRegistry.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
VREGDEF LSTATUS WINAPI VRegFlushKey(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey
);

/// Calls the `RegOpenKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
VREGDEF LSTATUS WINAPI VRegOpenKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
VREGDEF LSTATUS WINAPI VRegCreateKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey,
    _Reserved_ DWORD Reserved,
    _In_opt_ LPSTR lpClass,
    _In_ DWORD dwOptions,
    _In_ REGSAM samDesired,
    _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ PHKEY phkResult,
    _Out_opt_ LPDWORD lpdwDisposition
);

/// Calls the `RegQueryValueExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
VREGDEF LSTATUS WINAPI VRegQueryValueExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
);

/// Calls the `RegSetValueExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
VREGDEF LSTATUS WINAPI VRegSetValueExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName,
    _Reserved_ DWORD Reserved,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
VREGDEF LSTATUS WINAPI VRegDeleteValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName
);

/// Calls the `RegDeleteKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
VREGDEF LSTATUS WINAPI VRegDeleteKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// Calls the `RegDeleteTreeA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
VREGDEF LSTATUS WINAPI VRegDeleteTreeA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey
);

/// Calls the `RegEnumKeyExA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
VREGDEF LSTATUS WINAPI VRegEnumKeyExA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPSTR lpName,
    _Inout_ LPDWORD lpcchName,
    _Reserved_ LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegEnumValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluea
VREGDEF LSTATUS WINAPI VRegEnumValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPSTR lpValueName,
    _Inout_ LPDWORD lpcchValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _Inout_opt_ LPDWORD lpcbData
);

/// Calls the `RegQueryInfoKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeya
VREGDEF LSTATUS WINAPI VRegQueryInfoKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPSTR lpClass,
    _Inout_opt_ LPDWORD lpcchClass,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpcSubKeys,
    _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
    _Out_opt_ LPDWORD lpcbMaxClassLen,
    _Out_opt_ LPDWORD lpcValues,
    _Out_opt_ LPDWORD lpcbMaxValueNameLen,
    _Out_opt_ LPDWORD lpcbMaxValueLen,
    _Out_opt_ LPDWORD lpcbSecurityDescriptor,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// Calls the `RegLoadMUIStringA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringa
VREGDEF LSTATUS WINAPI VRegLoadMUIStringA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) LPSTR pszOutBuf,
    _In_ DWORD cbOutBuf,
    _Out_opt_ LPDWORD pcbData,
    _In_ DWORD Flags,
    _In_opt_ LPCSTR pszDirectory
);

/// Calls the `RegGetValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
VREGDEF LSTATUS WINAPI VRegGetValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hkey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValue,
    _In_ DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
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
    _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
    _Inout_opt_ LPDWORD pcbData
);

/// Calls the `RegOpenKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeya
VREGDEF LSTATUS WINAPI VRegOpenKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegOpenKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
VREGDEF LSTATUS WINAPI VRegOpenKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
VREGDEF LSTATUS WINAPI VRegCreateKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegCreateKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
VREGDEF LSTATUS WINAPI VRegCreateKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// Calls the `RegQueryValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
VREGDEF LSTATUS WINAPI VRegQueryValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPSTR lpData,
    _Inout_opt_ PLONG lpcbData
);

/// Calls the `RegQueryValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
VREGDEF LSTATUS WINAPI VRegQueryValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPWSTR lpData,
    _Inout_opt_ PLONG lpcbData
);

/// Calls the `RegSetValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
VREGDEF LSTATUS WINAPI VRegSetValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCSTR lpData,
    _In_ DWORD cbData
);

/// Calls the `RegSetValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
VREGDEF LSTATUS WINAPI VRegSetValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCWSTR lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
VREGDEF LSTATUS WINAPI VRegDeleteKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey
);

/// Calls the `RegDeleteKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
VREGDEF LSTATUS WINAPI VRegDeleteKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey
);

/// Calls the `RegEnumKeyA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
VREGDEF LSTATUS WINAPI VRegEnumKeyA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_opt_(cchName) LPSTR lpName,
    _In_ DWORD cchName
);

/// Calls the `RegEnumKeyW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
VREGDEF LSTATUS WINAPI VRegEnumKeyW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_opt_(cchName) LPWSTR lpName,
    _In_ DWORD cchName
);

/// Calls the `RegSetKeyValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
VREGDEF LSTATUS WINAPI VRegSetKeyValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValueName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCVOID lpData,
    _In_ DWORD cbData
);

/// Calls the `RegSetKeyValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
VREGDEF LSTATUS WINAPI VRegSetKeyValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValueName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCVOID lpData,
    _In_ DWORD cbData
);

/// Calls the `RegDeleteKeyValueA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
VREGDEF LSTATUS WINAPI VRegDeleteKeyValueA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValueName
);

/// Calls the `RegDeleteKeyValueW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
VREGDEF LSTATUS WINAPI VRegDeleteKeyValueW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValueName
);

/// Calls the `RegQueryMultipleValuesA` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
VREGDEF LSTATUS WINAPI VRegQueryMultipleValuesA(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_(num_vals) PVALENTA val_list,
    _In_ DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPSTR lpValueBuf,
    _Inout_opt_ LPDWORD ldwTotsize
);

/// Calls the `RegQueryMultipleValuesW` slot of @p pRegistry, or derives the call from the
/// wide `Ex` forms when the slot is null.
/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
VREGDEF LSTATUS WINAPI VRegQueryMultipleValuesW(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_(num_vals) PVALENTW val_list,
    _In_ DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPWSTR lpValueBuf,
    _Inout_opt_ LPDWORD ldwTotsize
);

/// The neutral names select the wide or the ANSI form by @ref UNICODE, as
/// `winreg.h` does for the functions they mirror.
#ifdef UNICODE
#define VRegCreateKey VRegCreateKeyW
#define VRegCreateKeyEx VRegCreateKeyExW
#define VRegDeleteKey VRegDeleteKeyW
#define VRegDeleteKeyEx VRegDeleteKeyExW
#define VRegDeleteKeyValue VRegDeleteKeyValueW
#define VRegDeleteTree VRegDeleteTreeW
#define VRegDeleteValue VRegDeleteValueW
#define VRegEnumKey VRegEnumKeyW
#define VRegEnumKeyEx VRegEnumKeyExW
#define VRegEnumValue VRegEnumValueW
#define VRegGetValue VRegGetValueW
#define VRegLoadMUIString VRegLoadMUIStringW
#define VRegOpenKey VRegOpenKeyW
#define VRegOpenKeyEx VRegOpenKeyExW
#define VRegQueryInfoKey VRegQueryInfoKeyW
#define VRegQueryMultipleValues VRegQueryMultipleValuesW
#define VRegQueryValue VRegQueryValueW
#define VRegQueryValueEx VRegQueryValueExW
#define VRegSetKeyValue VRegSetKeyValueW
#define VRegSetValue VRegSetValueW
#define VRegSetValueEx VRegSetValueExW
#else
#define VRegCreateKey VRegCreateKeyA
#define VRegCreateKeyEx VRegCreateKeyExA
#define VRegDeleteKey VRegDeleteKeyA
#define VRegDeleteKeyEx VRegDeleteKeyExA
#define VRegDeleteKeyValue VRegDeleteKeyValueA
#define VRegDeleteTree VRegDeleteTreeA
#define VRegDeleteValue VRegDeleteValueA
#define VRegEnumKey VRegEnumKeyA
#define VRegEnumKeyEx VRegEnumKeyExA
#define VRegEnumValue VRegEnumValueA
#define VRegGetValue VRegGetValueA
#define VRegLoadMUIString VRegLoadMUIStringA
#define VRegOpenKey VRegOpenKeyA
#define VRegOpenKeyEx VRegOpenKeyExA
#define VRegQueryInfoKey VRegQueryInfoKeyA
#define VRegQueryMultipleValues VRegQueryMultipleValuesA
#define VRegQueryValue VRegQueryValueA
#define VRegQueryValueEx VRegQueryValueExA
#define VRegSetKeyValue VRegSetKeyValueA
#define VRegSetValue VRegSetValueA
#define VRegSetValueEx VRegSetValueExA
#endif

/// Frees a registry through its `Destroy` slot; null, and a registry whose
/// slot is null, are ignored.
VREGDEF void WINAPI VRegDestroyRegistry(_In_opt_ VRegRegistry* pRegistry);

// ==========================================================================
// Hives
// ==========================================================================

/**
 * @brief Storage for one subtree of a virtual namespace: a tree with one
 *  anonymous root, mounted at a path by a virtual registry, held in memory
 *  on its own or layered over a key of another registry.
 *
 *  On its own, the hive starts empty and holds everything written to it.
 *  Layered over a lower key, reads fall through to that key and writes stay
 *  in memory: the hive's own layer adds and overrides keys and values.
 *  Deleting a lower value or key leaves a tombstone that hides it; a key
 *  deleted and created again, or emptied with `RegDeleteTreeW`, becomes
 *  opaque and hides everything below it in the lower layer. Enumeration
 *  lists the hive's own entries first, then the lower ones no entry shadows;
 *  counts cover both.
 *
 *  Each key has exactly one virtual handle, issued when the key is created.
 *  Readers take the hive's lock shared and writers exclusive, until
 *  @ref VRegFreezeHive; after that nothing writes and readers take no lock
 *  at all. The lower registry is never called with the lock held, so a lower
 *  call that loads a module or takes a long time cannot hold up the hive.
 *
 *  Frozen, a layered hive answers an open of a key it has nothing at or below
 *  with the lower registry's own handle. A virtual registry sends later calls
 *  on such a handle to its base, so a frozen hive's lower registry MUST be
 *  that base or answer the same handles.
 *
 * @note A hive MUST outlive every registry it is mounted in. Destroying it
 *  turns its handles into invalid ones.
 *
 * @note Once hooked, every call on a hive may run under a foreign loader
 *  lock: nothing here loads a module, creates a thread or waits on a lock a
 *  loader-lock holder could own.
 */
typedef struct VRegHive VRegHive;

/// A key in a hive. Opaque outside the hive that owns it; never freed before
/// the hive, so a pointer to one stays valid for the hive's life.
typedef struct VReg__Node VReg__Node;

/// A hive held entirely in memory; starts empty. @p pAllocContext is passed
/// to every allocation the hive makes.
VREGDEF LSTATUS WINAPI VRegCreateHive(_In_opt_ void* pAllocContext, _Outptr_ VRegHive** ppHive);

/// A hive over the key @p pszLowerPath below @p hLowerRoot of @p pLower,
/// which MUST outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
/// @ref KEY_WOW64_32KEY or 0) is added to every lower open.
VREGDEF LSTATUS WINAPI VRegCreateLayeredHive(
    _In_ VRegRegistry* pLower,
    _In_ HKEY hLowerRoot,
    _In_opt_z_ LPCWSTR pszLowerPath,
    _In_ REGSAM samLowerView,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegHive** ppHive
);

/// Frees the hive and unbinds every handle it issued; null is ignored.
VREGDEF void WINAPI VRegDestroyHive(_In_opt_ VRegHive* pHive);

/// Makes the hive immutable from now on, so its readers take no lock.
/// Safe while it is mounted and in use; there is no way back.
VREGDEF void WINAPI VRegFreezeHive(_In_ VRegHive* pHive);

VREGDEF BOOL WINAPI VRegIsHiveFrozen(_In_ VRegHive* pHive);

/**
 * @brief Creates the key @p pszPath below the hive's root if needed and sets
 *  its value @p pszName to @p pszText as a @ref REG_SZ marked as MUI text.
 *
 *  `RegLoadMUIStringW` answers an unmarked hive value @ref ERROR_INVALID_DATA,
 *  since resolving an `@` resource reference would load a module. A marked
 *  one stands for the text such a reference resolves to and answers with
 *  @p pszText itself. Any other write of the value drops the mark, and a
 *  `.reg` export carries it as a plain string.
 */
VREGDEF LSTATUS WINAPI VRegSetHiveMuiTextValue(
    _In_ VRegHive* pHive,
    _In_opt_z_ LPCWSTR pszPath,
    _In_opt_z_ LPCWSTR pszName,
    _In_z_ LPCWSTR pszText
);

// ==========================================================================
// The native registry
// ==========================================================================

// The functions below include Vista's (RegDeleteTreeW, RegGetValueW), which
// the SDK declares only from that target on.
#if _WIN32_WINNT < 0x0600
#error "VReg needs _WIN32_WINNT 0x0600 (Windows Vista) or later."
#endif

/// Applies `X` to the name of every wide `Ex` form and every function with
/// one form only: the wide surface of @ref VRegRegistryVtbl, and the fields
/// of @ref VRegNativeFunctions. It serves VReg's own sources and
/// tests; it is not part of the API.
#define VREG_INTERNAL_REGISTRY_VIRTUALS(X)                                     \
    X(RegOpenKeyExW)                                                           \
    X(RegCreateKeyExW)                                                         \
    X(RegCloseKey)                                                             \
    X(RegQueryValueExW)                                                        \
    X(RegSetValueExW)                                                          \
    X(RegDeleteValueW)                                                         \
    X(RegDeleteKeyExW)                                                         \
    X(RegDeleteTreeW)                                                          \
    X(RegEnumKeyExW)                                                           \
    X(RegEnumValueW)                                                           \
    X(RegQueryInfoKeyW)                                                        \
    X(RegLoadMUIStringW)                                                       \
    X(RegGetValueW)                                                            \
    X(RegNotifyChangeKeyValue)                                                 \
    X(RegFlushKey)

/// Applies `X` to the name of every other form in @ref VRegRegistryVtbl: the
/// ANSI forms, the pre-`Ex` forms and the conveniences Win32 builds from the
/// wide forms. The hooks emulate these too, but no registry
/// calls them, so their trampolines stay with the hooks.
#define VREG_INTERNAL_REGISTRY_FORMS(X)                                        \
    X(RegOpenKeyExA)                                                           \
    X(RegOpenKeyW)                                                             \
    X(RegOpenKeyA)                                                             \
    X(RegCreateKeyExA)                                                         \
    X(RegCreateKeyW)                                                           \
    X(RegCreateKeyA)                                                           \
    X(RegQueryValueExA)                                                        \
    X(RegQueryValueW)                                                          \
    X(RegQueryValueA)                                                          \
    X(RegSetValueExA)                                                          \
    X(RegSetValueW)                                                            \
    X(RegSetValueA)                                                            \
    X(RegDeleteValueA)                                                         \
    X(RegDeleteKeyExA)                                                         \
    X(RegDeleteKeyW)                                                           \
    X(RegDeleteKeyA)                                                           \
    X(RegDeleteTreeA)                                                          \
    X(RegEnumKeyExA)                                                           \
    X(RegEnumKeyW)                                                             \
    X(RegEnumKeyA)                                                             \
    X(RegEnumValueA)                                                           \
    X(RegQueryInfoKeyA)                                                        \
    X(RegLoadMUIStringA)                                                       \
    X(RegGetValueA)                                                            \
    X(RegSetKeyValueW)                                                         \
    X(RegSetKeyValueA)                                                         \
    X(RegDeleteKeyValueW)                                                      \
    X(RegDeleteKeyValueA)                                                      \
    X(RegQueryMultipleValuesW)                                                 \
    X(RegQueryMultipleValuesA)

/// Both lists: every function the @ref VReg::Registry surface mirrors.
#define VREG_INTERNAL_REGISTRY_FUNCTIONS(X)                                    \
    VREG_INTERNAL_REGISTRY_VIRTUALS(X)                                         \
    VREG_INTERNAL_REGISTRY_FORMS(X)

/// The entry points the native registry calls and the hooks attach to: the
/// wide `Ex` forms and the functions with one form only. The ANSI, pre-`Ex`
/// and convenience forms are derived from these, as everywhere.
typedef struct VRegNativeFunctions
{
    LSTATUS (WINAPI* RegOpenKeyExW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    );

    LSTATUS (WINAPI* RegCreateKeyExW)(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPWSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    );

    LSTATUS (WINAPI* RegCloseKey)(
        _In_ HKEY hKey
    );

    LSTATUS (WINAPI* RegQueryValueExW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    );

    LSTATUS (WINAPI* RegSetValueExW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    );

    LSTATUS (WINAPI* RegDeleteValueW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName
    );

    LSTATUS (WINAPI* RegDeleteKeyExW)(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    );

    LSTATUS (WINAPI* RegDeleteTreeW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey
    );

    LSTATUS (WINAPI* RegEnumKeyExW)(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    LSTATUS (WINAPI* RegEnumValueW)(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    );

    LSTATUS (WINAPI* RegQueryInfoKeyW)(
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    LSTATUS (WINAPI* RegLoadMUIStringW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
    );

    LSTATUS (WINAPI* RegGetValueW)(
        _In_ HKEY hkey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    );

    LSTATUS (WINAPI* RegNotifyChangeKeyValue)(
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    );

    LSTATUS (WINAPI* RegFlushKey)(
        _In_ HKEY hKey
    );
} VRegNativeFunctions;

/// Fills @p pFunctions from the export tables of `KernelBase.dll` and, for
/// the pre-`Ex` forms only it exports, `advapi32.dll`, never through
/// @ref GetProcAddress: an apphelp shim can make that answer a stub instead of
/// the entry point. Resolution never loads a module (VReg links advapi32, so
/// both are loaded before it runs), which makes it safe under the loader
/// lock. A function neither module exports stays null.
VREGDEF void WINAPI VRegResolveNativeFunctions(_Out_ VRegNativeFunctions* pFunctions);

/// The real registry, reached through @p pFunctions, or through
/// @ref VRegResolveNativeFunctions when that is null. The table is copied; the
/// live copy, which the hooks attach to, is @ref VRegGetNativeFunctions.
/// Freed with @ref VRegDestroyRegistry.
VREGDEF LSTATUS WINAPI VRegCreateNativeRegistry(
    _In_opt_ const VRegNativeFunctions* pFunctions,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegRegistry** ppRegistry
);

/// The live table of a registry @ref VRegCreateNativeRegistry made: once the
/// hooks are committed it holds the trampolines, and the registry is the one
/// below them.
VREGDEF _Ret_notnull_ VRegNativeFunctions* WINAPI VRegGetNativeFunctions(_In_ VRegRegistry* pRegistry);

// ==========================================================================
// The virtual registry
// ==========================================================================

/// @ref NtQueryKey, the one `ntdll` function a virtual registry uses to find
/// the native path of a real key.
typedef LONG (NTAPI* VRegNtQueryKeyFunction)(
    HANDLE hKey,
    ULONG keyInformationClass,
    PVOID pKeyInformation,
    ULONG cbKeyInformation,
    PULONG pcbResult
);

/**
 * @brief A namespace of hives mounted at registry paths over an optional base
 *  registry.
 *
 *  A call lands in a hive when it passes a virtual handle of a mounted hive,
 *  or a predefined root plus a path at or below a mount point. Everything
 *  else goes to the base registry; with no base it answers
 *  @ref ERROR_FILE_NOT_FOUND (or @ref ERROR_INVALID_HANDLE for a handle that
 *  was never valid).
 *
 *  With a base, a call on a real key handle whose subkey path names a mount
 *  point's last component is resolved by the key's native path
 *  (@ref NtQueryKey), so a handle opened above a mount reaches it. Only
 *  mounts below @ref HKEY_LOCAL_MACHINE, @ref HKEY_USERS and
 *  @ref HKEY_CURRENT_USER have a native path.
 *
 *  The virtual registry owns the caller-buffer rules (sizes in characters or
 *  bytes, terminators, @ref ERROR_MORE_DATA) for every hive.
 *
 *  Mounting publishes a new mount table with one atomic store, and old tables
 *  are kept until the registry is destroyed, so calls in flight never take a
 *  lock to read it.
 *
 *  @p pBase answers everything outside the mounts; null for a registry held
 *  entirely in memory. @p pfnNtQueryKey replaces the `ntdll` export for native
 *  path resolution (a test seam); resolution happens here, so no call ever
 *  loads a module. Freed with @ref VRegDestroyRegistry.
 */
VREGDEF LSTATUS WINAPI VRegCreateVirtualRegistry(
    _In_opt_ VRegRegistry* pBase,
    _In_opt_ VRegNtQueryKeyFunction pfnNtQueryKey,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegRegistry** ppRegistry
);

/// Mounts @p pHive at @p pszPath below the predefined key @p hRoot (a null or
/// empty path mounts the root itself) in a registry
/// @ref VRegCreateVirtualRegistry made. @p pHive MUST outlive the registry.
/// Answers @ref ERROR_INVALID_HANDLE for a root that is not predefined or a
/// registry that is not virtual, @ref ERROR_BAD_PATHNAME for a malformed path
/// and @ref ERROR_ALREADY_EXISTS for a path already mounted. @p samView limits
/// the calls the mount answers to those of one registry view:
/// @ref KEY_WOW64_64KEY or @ref KEY_WOW64_32KEY, or 0 for both; anything else
/// answers @ref ERROR_INVALID_PARAMETER.
VREGDEF LSTATUS WINAPI VRegMount(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
    _In_ VRegHive* pHive,
    _In_ REGSAM samView
);

// ==========================================================================
// Version
// ==========================================================================

/// The version of this header: the version under development, which a
/// release tag `vX.Y.Z` makes final.
#define VREG_VERSION_MAJOR 0
#define VREG_VERSION_MINOR 4
#define VREG_VERSION_PATCH 1

/// The version of the copy of VReg linked into the calling module, as text
/// for display and logs. Its format is unspecified and may change; compare
/// versions with @ref VRegGetFileVersion.
VREGDEF _Ret_z_ LPCSTR WINAPI VRegGetVersion(void);

/// The version of the copy of VReg linked into the calling module, which can
/// differ from the copies other modules in the same process link, packed as
/// @ref MAKEDLLVERULL packs a four-part version: major, minor, patch and
/// revision from the high word down, the value a version resource's
/// `dwFileVersionMS:dwFileVersionLS` reads as, so versions compare as
/// numbers. Major, minor and patch are @ref VREG_VERSION_MAJOR,
/// @ref VREG_VERSION_MINOR and @ref VREG_VERSION_PATCH. The revision counts
/// the commits in the history a CMake build of a git checkout was built
/// from, and is 0 otherwise. At build time, `find_package(VReg)` reports the
/// same major, minor and patch.
VREGDEF ULONGLONG WINAPI VRegGetFileVersion(void);

// ==========================================================================
// .reg scripts
// ==========================================================================

/// The encodings and headers a `.reg` file is written in.
typedef enum VRegFormat
{
    /// `Windows Registry Editor Version 5.00` as UTF-16LE with a BOM: what
    /// regedit writes.
    VREG_FORMAT_UNICODE,

    /// The same header as UTF-8 without a BOM.
    VREG_FORMAT_UTF8,

    /// `REGEDIT4` in @ref CP_ACP.
    VREG_FORMAT_REGEDIT4,
} VRegFormat;

typedef enum VRegOperationKind
{
    VREG_OPERATION_CREATE_KEY,
    VREG_OPERATION_DELETE_KEY,
    VREG_OPERATION_SET_VALUE,
    VREG_OPERATION_DELETE_VALUE,
} VRegOperationKind;

/// One line of a `.reg` file that does something. Value operations act on the
/// key most recently created by the file; each carries that key so it stands
/// alone. The pointers belong to whoever built the operation: to the script
/// for one a script holds.
typedef struct VRegOperation
{
    VRegOperationKind kind;

    /// A predefined root such as @ref HKEY_LOCAL_MACHINE.
    HKEY hRoot;

    /// Below @ref hRoot, without a leading backslash; empty for the root
    /// itself.
    LPCWSTR pszPath;

    /// Empty for the default value (`@`). Values only.
    LPCWSTR pszName;

    /// Values only.
    DWORD dwType;

    /// Values only.
    const BYTE* pData;
    DWORD cbData;

    /// 1-based line of the file the operation starts on; zero for one that
    /// came from no file.
    SIZE_T line;
} VRegOperation;

/// The operations of a file, in file order, with the storage they point into.
typedef struct VRegScript VRegScript;

/// Where and why a file failed to parse.
typedef struct VRegParseError
{
    /// 1-based, in UTF-16 code units of the decoded text.
    SIZE_T line;

    SIZE_T column;

    /// A static string.
    const char* pszMessage;
} VRegParseError;

/**
 * @brief Parses a whole file into a new script.
 *
 *  The encoding is taken from a BOM (UTF-16LE, UTF-8); without one, a
 *  `REGEDIT4` header means @ref CP_ACP and anything else UTF-8. The script is
 *  only created when the whole file parses, and is freed with
 *  @ref VRegDestroyScript.
 *
 * @return @ref ERROR_SUCCESS, @ref ERROR_INVALID_DATA on a syntax or encoding
 *  error (described by @p pError, which MAY be null and is zeroed for any
 *  other outcome), or @ref ERROR_OUTOFMEMORY.
 */
VREGDEF LSTATUS WINAPI VRegParseScript(
    _In_reads_bytes_(cbFile) const void* pFile,
    _In_ SIZE_T cbFile,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Reads the file @p pszPath, then @ref VRegParseScript. Failures to read
/// answer the Win32 error.
VREGDEF LSTATUS WINAPI VRegParseScriptFile(
    _In_z_ LPCWSTR pszPath,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Frees a script; null is ignored.
VREGDEF void WINAPI VRegDestroyScript(_In_opt_ VRegScript* pScript);

/// The script's operations, in order, and how many; valid until the script is
/// destroyed.
VREGDEF _Ret_notnull_ const VRegOperation* WINAPI VRegGetScriptOperations(
    _In_ const VRegScript* pScript,
    _Out_ SIZE_T* pcOperations
);

/**
 * @brief Performs @p cOperations operations through @p pRegistry, in order.
 *
 *  Stops at the first operation that fails, stores its index in
 *  @p pFailedIndex (MAY be null) and returns its status; earlier operations
 *  stay applied. Deleting a key or value that does not exist is not a
 *  failure. An operation that would delete a whole root (a key deletion with
 *  an empty path) answers @ref ERROR_INVALID_PARAMETER.
 */
_Success_(return == ERROR_SUCCESS)
VREGDEF LSTATUS WINAPI VRegApplyOperations(
    _In_reads_(cOperations) const VRegOperation* pOperations,
    _In_ SIZE_T cOperations,
    _In_ VRegRegistry* pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* pFailedIndex
);

/// @ref VRegApplyOperations over a script's operations.
_Success_(return == ERROR_SUCCESS)
VREGDEF LSTATUS WINAPI VRegApplyScript(
    _In_ const VRegScript* pScript,
    _In_ VRegRegistry* pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* pFailedIndex
);

/**
 * @brief A script that recreates the key @p pszPath below the predefined root
 *  @p hRoot and everything below it: each key, then its values, then its
 *  subkeys, keys and values sorted ordinal case-insensitive.
 *
 *  Written and applied, the script reproduces every value's bytes. Freed with
 *  @ref VRegDestroyScript.
 *
 * @return @ref ERROR_INVALID_HANDLE when @p hRoot is not a root a `.reg` file
 *  can name, @ref ERROR_CANTREAD for a key nested deeper than the registry
 *  allows (a cycle through some other registry), otherwise the status of the
 *  registry calls.
 */
VREGDEF LSTATUS WINAPI VRegSnapshotScript(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript
);

/**
 * @brief A script of what @p pHive holds itself, which reproduces the hive's
 *  merged view when applied over the same lower registry (or, for a hive
 *  held entirely in memory, its contents).
 *
 *  @p hRoot and @p pszPath name where the hive is mounted: the operations'
 *  paths are that prefix plus the key's path in the hive. The hive's own
 *  keys are created with their own values, a deleted lower value is a value
 *  deletion, a deleted lower key a key deletion, and a key that hides its
 *  lower counterpart a deletion followed by its creation and contents. A key
 *  that only shows a lower key through is left out. Order is that of
 *  @ref VRegSnapshotScript. A tombstone with no lower counterpart is kept,
 *  and does nothing when applied. Freed with @ref VRegDestroyScript.
 *
 * @return As @ref VRegSnapshotScript, except that the hive is read instead of
 *  a registry.
 */
VREGDEF LSTATUS WINAPI VRegDeltaScript(
    _In_ VRegHive* pHive,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript
);

/**
 * @brief Lays @p pScript out as a regedit-style file in @p format, into bytes
 *  allocated with the script's context, freed with @ref VRegFreeScriptBytes.
 *
 *  Each key operation is a `[key]` or `[-key]` line and each value operation
 *  a `"name"=...` line under it; a value whose key differs from the last key
 *  line gets a `[key]` line of its own first, so a script built by hand lays
 *  out as one parsed from a file would.
 *
 * @return @ref ERROR_NO_UNICODE_TRANSLATION when @p format cannot represent a
 *  name or string, @ref ERROR_INVALID_NAME for a name that holds a line break
 *  (no format can write one), @ref ERROR_INVALID_HANDLE for a root a `.reg`
 *  file cannot name.
 */
VREGDEF LSTATUS WINAPI VRegWriteScript(
    _In_ const VRegScript* pScript,
    _In_ VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** ppBytes,
    _Out_ SIZE_T* pcbBytes
);

VREGDEF void WINAPI VRegFreeScriptBytes(_In_ const VRegScript* pScript, _In_opt_ void* pBytes);

/// @ref VRegWriteScript into the file @p pszPath. An existing file is replaced
/// only once the layout has succeeded.
VREGDEF LSTATUS WINAPI VRegWriteScriptFile(
    _In_ const VRegScript* pScript,
    _In_ VRegFormat format,
    _In_z_ LPCWSTR pszPath
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VREG_H_INCLUDED

#ifdef VREG_WITH_DETOURS
#ifndef VREG_DETOURS_H_INCLUDED
#define VREG_DETOURS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================
// Hooks
// ==========================================================================

// Routes the process's `Reg*` calls into a @ref VRegRegistry (normally
// a virtual registry) through Microsoft Detours.
//
// Every `Reg*` export that takes a key handle is hooked, since a virtual
// handle reaching an unhooked one would fail as invalid. Most are answered
// by the registry; the rest (save, restore, load, security, rename, copy,
// transacted forms, reflection, predefined-key overrides) answer
// @ref ERROR_NOT_SUPPORTED for a virtual handle of this copy (the
// transacted forms also for a path into a mount) and pass everything else
// on.
//
// Each hook is a small thunk outside the module image that jumps through a
// slot. The hooks are never detached: @ref VRegNeutralizeHooks points every
// slot at the trampoline, so the layer passes everything straight through
// while other hookers' chains stay intact in any order. It runs by itself
// when the module is unloaded.
//
// Hook bodies may run under a foreign loader lock; nothing they reach loads
// a module, creates a thread or waits on a lock a loader-lock holder could
// own.
//
// @note A thread still inside a hook body when its module unmaps crashes:
// the module MUST NOT unload while registry calls are in flight. A module
// that cannot rule that out MAY pin itself (@ref GetModuleHandleExW with
// @ref GET_MODULE_HANDLE_EX_FLAG_PIN), which keeps it loaded until the
// process exits.

/// Calls @ref DetourAttach for every hook, inside a transaction the caller
/// begins and commits (for example one that also carries its own hooks).
/// Every hook calls through @p pRegistry, which MAY wrap the registry that
/// answers (to trace each call, say). @p pNative is the registry below the
/// hooks, the one @p pRegistry reaches for whatever it does not answer: it
/// MUST be one @ref VRegCreateNativeRegistry made, and the commit turns its
/// function table into the trampolines. Both MUST outlive the process.
/// Answers the first Detours error, or @ref ERROR_ALREADY_INITIALIZED when
/// this copy attached before.
VREGDEF LONG WINAPI VRegAttachHooks(_In_ VRegRegistry* pRegistry, _In_ VRegRegistry* pNative);

/// @ref VRegAttachHooks in a transaction of its own, enrolling only the
/// calling thread.
VREGDEF LONG WINAPI VRegInstallHooks(_In_ VRegRegistry* pRegistry, _In_ VRegRegistry* pNative);

/// Makes every hook of this copy pass calls straight through, for good.
VREGDEF void WINAPI VRegNeutralizeHooks(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VREG_DETOURS_H_INCLUDED
#endif // VREG_WITH_DETOURS

// The private declarations come with the implementation. VREG__INTERNAL
// shows them alone, for code built beside the library that reaches into it:
// its own tests, and the hooks compiled apart from the rest
// (VREG__HOOKS_IMPLEMENTATION, as the CMake build's VReg::Detours does).
#ifdef VREG_IMPLEMENTATION
#ifndef VREG__INTERNAL
#define VREG__INTERNAL
#endif
#ifdef VREG_WITH_DETOURS
#ifndef VREG__HOOKS_IMPLEMENTATION
#define VREG__HOOKS_IMPLEMENTATION
#endif
#endif
#endif

#ifdef VREG__HOOKS_IMPLEMENTATION
#ifndef VREG__INTERNAL
#define VREG__INTERNAL
#endif
#endif

#ifdef VREG__INTERNAL
#ifndef VREG__INTERNAL_INCLUDED
#define VREG__INTERNAL_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================
// Allocation
// ==========================================================================

// How VReg allocates. A consumer overrides `VREG_MALLOC`,
// `VREG_REALLOC` and `VREG_FREE` together, before the implementation is
// compiled; each takes the allocation context the registry or hive that
// allocates was created with. The defaults use the process heap: no CRT
// dependency, and safe under the loader lock.

#ifndef VREG_MALLOC
#define VREG_MALLOC(pAllocContext, cb) HeapAlloc(GetProcessHeap(), 0, (cb))
#endif

#ifndef VREG_REALLOC
#define VREG_REALLOC(pAllocContext, p, cb) HeapReAlloc(GetProcessHeap(), 0, (p), (cb))
#endif

#ifndef VREG_FREE
#define VREG_FREE(pAllocContext, p) HeapFree(GetProcessHeap(), 0, (p))
#endif

/// Breaks into an attached debugger in a `_DEBUG` build; nothing otherwise.
#ifndef VREG_ASSERT
#ifdef _DEBUG
#define VREG_ASSERT(x) ((x) ? (void)0 : VReg__AssertFailed())
#else
#define VREG_ASSERT(x) ((void)0)
#endif
#endif

VREGDEF void VReg__AssertFailed(void);

/// Null for @p cb of zero or when memory ran out.
VREGDEF _Ret_maybenull_ void* VReg__Alloc(_In_opt_ void* pAllocContext, SIZE_T cb);

/// Like @ref VReg__Alloc, zero-filled.
VREGDEF _Ret_maybenull_ void* VReg__AllocZeroed(_In_opt_ void* pAllocContext, SIZE_T cb);

/// Resizes @p p (null allocates) to @p cb bytes; null when memory ran out,
/// leaving @p p as it was.
VREGDEF _Ret_maybenull_ void* VReg__Realloc(_In_opt_ void* pAllocContext, _In_opt_ void* p, SIZE_T cb);

/// Frees @p p; null is ignored.
VREGDEF void VReg__Free(_In_opt_ void* pAllocContext, _In_opt_ void* p);

// ==========================================================================
// Buffers
// ==========================================================================

// Owned, growable storage with an allocation context passed to every
// call that may allocate: a byte buffer, a wide string and an array of
// fixed-size elements. A zero-filled instance is empty and needs no
// initialization; every call that grows answers FALSE when memory ran out
// and leaves the instance as it was.

/// A view of wide text that is not necessarily terminated.
typedef struct VReg__WStringView
{
    const wchar_t* pch;
    SIZE_T cch;
} VReg__WStringView;

VREGDEF VReg__WStringView VReg__ViewOf(_In_opt_z_ LPCWSTR psz);

VREGDEF DWORD VReg__MinDword(DWORD a, DWORD b);
VREGDEF DWORD VReg__MaxDword(DWORD a, DWORD b);

typedef struct VReg__Buffer
{
    BYTE* pb;
    SIZE_T cb;
    SIZE_T cbCapacity;
} VReg__Buffer;

VREGDEF BOOL VReg__BufferReserve(_In_opt_ void* pAllocContext, _Inout_ VReg__Buffer* pBuffer, SIZE_T cbCapacity);

/// Sets the size to @p cb, growing the capacity if needed; new bytes are
/// indeterminate.
VREGDEF BOOL VReg__BufferResize(_In_opt_ void* pAllocContext, _Inout_ VReg__Buffer* pBuffer, SIZE_T cb);

VREGDEF BOOL VReg__BufferAssign(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Buffer* pBuffer,
    _In_reads_bytes_opt_(cb) const void* p,
    SIZE_T cb
);

VREGDEF BOOL VReg__BufferAppend(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Buffer* pBuffer,
    _In_reads_bytes_(cb) const void* p,
    SIZE_T cb
);

VREGDEF void VReg__BufferFree(_In_opt_ void* pAllocContext, _Inout_ VReg__Buffer* pBuffer);

/// A wide string, terminated whenever @ref psz is not null; empty and
/// unallocated while it is.
typedef struct VReg__WString
{
    wchar_t* psz;
    SIZE_T cch;
    SIZE_T cchCapacity;
} VReg__WString;

/// The text, `L""` for an unallocated string.
VREGDEF _Ret_z_ LPCWSTR VReg__WStringText(_In_ const VReg__WString* pString);

VREGDEF VReg__WStringView VReg__WStringViewOf(_In_ const VReg__WString* pString);

/// Room for @p cch characters and the terminator.
VREGDEF BOOL VReg__WStringReserve(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString, SIZE_T cch);

/// Sets the length to @p cch, growing if needed, and terminates; new
/// characters are indeterminate.
VREGDEF BOOL VReg__WStringResize(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString, SIZE_T cch);

VREGDEF BOOL VReg__WStringAssign(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pString,
    _In_reads_opt_(cch) const wchar_t* pch,
    SIZE_T cch
);

VREGDEF BOOL VReg__WStringAppend(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pString,
    _In_reads_(cch) const wchar_t* pch,
    SIZE_T cch
);

VREGDEF BOOL VReg__WStringAppendChar(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString, wchar_t ch);

/// Shortens the string to @p cch characters, which is at most its length.
VREGDEF void VReg__WStringTruncate(_Inout_ VReg__WString* pString, SIZE_T cch);

VREGDEF void VReg__WStringFree(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString);

/// An array of @ref count elements of one size, given to every call.
typedef struct VReg__Array
{
    void* p;
    SIZE_T count;
    SIZE_T capacity;
} VReg__Array;

/// The element at @p index of an array of @p T, as an lvalue.
#define VREG__AT(pArray, T, index) (((T*)(pArray)->p)[index])

VREGDEF BOOL VReg__ArrayReserve(_In_opt_ void* pAllocContext, _Inout_ VReg__Array* pArray, SIZE_T cbElement, SIZE_T capacity);

/// Inserts a copy of @p pElement before @p index, which is at most the count.
VREGDEF BOOL VReg__ArrayInsert(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Array* pArray,
    SIZE_T cbElement,
    SIZE_T index,
    _In_reads_bytes_(cbElement) const void* pElement
);

VREGDEF BOOL VReg__ArrayAppend(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Array* pArray,
    SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* pElement
);

/// Removes the element at @p index, moving the later ones down.
VREGDEF void VReg__ArrayRemoveAt(_Inout_ VReg__Array* pArray, SIZE_T cbElement, SIZE_T index);

VREGDEF void VReg__ArrayFree(_In_opt_ void* pAllocContext, _Inout_ VReg__Array* pArray);

/// Appends a copy of @p text to @p pArray, an array of @ref VReg__WString.
VREGDEF BOOL VReg__WStringArrayAppend(_In_opt_ void* pAllocContext, _Inout_ VReg__Array* pArray, VReg__WStringView text);

/// Adds a copy of @p text to @p pArray, an array of @ref VReg__WString sorted
/// by @ref VReg__CompareNames, unless an equal name is there already.
VREGDEF BOOL VReg__WStringArrayInsertSorted(_In_opt_ void* pAllocContext, _Inout_ VReg__Array* pArray, VReg__WStringView text);

/// Whether the sorted @p pArray holds a name equal to @p text by
/// @ref VReg__CompareNames.
VREGDEF BOOL VReg__WStringArrayContainsSorted(_In_ const VReg__Array* pArray, VReg__WStringView text);

/// Frees the strings of @p pArray, an array of @ref VReg__WString, and the
/// array itself.
VREGDEF void VReg__WStringArrayFree(_In_opt_ void* pAllocContext, _Inout_ VReg__Array* pArray);

// ==========================================================================
// ANSI conversion
// ==========================================================================

/// Widens @p cch bytes through @ref CP_ACP into @p pText, without a
/// terminator beyond the one the string type keeps. Answers
/// @ref ERROR_OUTOFMEMORY when memory ran out or the text is longer than the
/// conversion can take.
VREGDEF LSTATUS VReg__WideFromAnsi(
    _In_opt_ void* pAllocContext,
    _In_reads_(cch) const char* pch,
    SIZE_T cch,
    _Inout_ VReg__WString* pText
);

/// Narrows @p cch UTF-16 units through @ref CP_ACP into @p pText, a buffer of
/// bytes without a terminator; what the code page cannot represent becomes
/// its default character. Answers @ref ERROR_OUTOFMEMORY as
/// @ref VReg__WideFromAnsi does.
VREGDEF LSTATUS VReg__AnsiFromWide(
    _In_opt_ void* pAllocContext,
    _In_reads_(cch) const wchar_t* pch,
    SIZE_T cch,
    _Inout_ VReg__Buffer* pText
);

/// A string argument that may be null: the wide copy of an ANSI argument.
typedef struct VReg__WideArg
{
    BOOL isNull;
    VReg__WString text;
} VReg__WideArg;

/// Fills @p pArg from @p psz; a zero-filled @p pArg stands for a null one.
VREGDEF LSTATUS VReg__WideArgInit(_In_opt_ void* pAllocContext, _In_opt_z_ LPCSTR psz, _Out_ VReg__WideArg* pArg);

/// The wide text, null when the argument was null.
VREGDEF _Ret_maybenull_z_ LPCWSTR VReg__WideArgText(_In_ const VReg__WideArg* pArg);

/// Like @ref VReg__WideArgText, as a buffer the callee may write to.
VREGDEF _Ret_maybenull_ LPWSTR VReg__WideArgMutable(_In_ VReg__WideArg* pArg);

VREGDEF void VReg__WideArgFree(_In_opt_ void* pAllocContext, _Inout_ VReg__WideArg* pArg);

// ==========================================================================
// Hive nodes
// ==========================================================================

typedef struct VReg__Value
{
    VReg__WString name;
    DWORD dwType;
    VReg__Buffer data;

    /// Hides the lower value of this name; only in a hive with a lower
    /// registry.
    BOOL isTombstone;

    /// Stored by @ref VReg__HiveSetMuiTextValue: a terminated @ref REG_SZ.
    BOOL isMuiText;
} VReg__Value;

/// A pending change notification: fires once, then is gone.
typedef struct VReg__ChangeWatch
{
    /// Our own duplicate of the caller's event; closed when the watch fires or
    /// the hive goes.
    HANDLE hEvent;

    BOOL isSubtree;
    DWORD dwFilter;
} VReg__ChangeWatch;

/// The flags below @ref isDeleted describe a node's relation to the lower
/// registry and stay clear in a hive without one.
struct VReg__Node
{
    VRegHive* pHive;
    VReg__Node* pParent;
    HKEY hKey;
    VReg__WString name;
    VReg__WString className;
    FILETIME lastWrite;
    BOOL isVolatile;
    BOOL isDeleted;

    /// The key exists because the lower key at its path does; the node is
    /// just the way to it. Writing to it makes it an upper key.
    BOOL isTransparent;

    /// Hides the lower key of this name; never handed out.
    BOOL isTombstone;

    /// Hides every lower subkey and value (a key deleted and created again,
    /// or emptied by `RegDeleteTreeW`).
    BOOL isOpaque;

    /// The lower key at this path exists and its class shows through (the
    /// node was made from it, not created over it).
    BOOL isLowerBacked;

    /// `VReg__Node*`, sorted by @ref VReg__CompareNames, the order the native
    /// registry enumerates subkeys in.
    VReg__Array children;

    /// @ref VReg__Value, in creation order, as the native registry enumerates
    /// values.
    VReg__Array values;

    /// @ref VReg__ChangeWatch, guarded by the hive's write lock.
    VReg__Array watches;
};

/// Ordinal comparison of uppercased names, the registry's own collation:
/// negative, zero or positive.
VREGDEF int VReg__CompareNames(VReg__WStringView left, VReg__WStringView right);

// ==========================================================================
// Export tables
// ==========================================================================

/// The entry point @p hModule exports as @p pszName, read from its export
/// table; null when @p hModule is null, the name is not exported, or the
/// export forwards to another module.
VREGDEF _Ret_maybenull_ void* VReg__FindExport(_In_opt_ HMODULE hModule, _In_z_ const char* pszName);

/// A registry function, from `KernelBase.dll` or else `advapi32.dll`, which
/// alone exports the pre-`Ex` forms; null when neither exports it. Never
/// loads a module.
VREGDEF _Ret_maybenull_ void* VReg__FindRegistryExport(_In_z_ const char* pszName);

// ==========================================================================
// Owned reads
// ==========================================================================

// Reads over a @ref VRegRegistry that size and fill owned storage
// themselves, retrying with a larger buffer while the value grows between
// the sizing call and the read. The storage is allocated with the
// @p pAllocContext given and stays the caller's to free.

// Names and classes are read into a temporary buffer of 256 characters, which
// holds the longest key name, and grow by doubling on ERROR_MORE_DATA, up to
// the limits below. Avoids an additional sizing call for the common case.
#define VREG__INITIAL_NAME_CHARS ((DWORD)256)
#define VREG__MAX_CLASS_CHARS ((DWORD)32768)
#define VREG__MAX_VALUE_NAME_CHARS ((DWORD)16384)

/// The size to retry with: the one the call asked for, or more than before if
/// it asked for no more than it was given.
VREGDEF DWORD VReg__Grown(DWORD cbOld, DWORD cbRequired);

VREGDEF LSTATUS VReg__QueryValueOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_z_ LPCWSTR lpValueName,
    _In_opt_ void* pAllocContext,
    _Out_opt_ LPDWORD lpType,
    _Inout_ VReg__Buffer* pData
);

VREGDEF LSTATUS VReg__GetValueOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_z_ LPCWSTR lpSubKey,
    _In_opt_z_ LPCWSTR lpValue,
    _In_ DWORD dwFlags,
    _In_opt_ void* pAllocContext,
    _Out_opt_ LPDWORD pdwType,
    _Inout_ VReg__Buffer* pData
);

VREGDEF LSTATUS VReg__EnumKeyOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pName
);

/// With a null @p pData only the name and type are read.
VREGDEF LSTATUS VReg__EnumValueOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pName,
    _Out_opt_ LPDWORD lpType,
    _Inout_opt_ VReg__Buffer* pData
);

/// @ref VRegQueryInfoKeyW with the class read into @p pClass. When the read
/// fails and its last call reported a class length other than the buffer's,
/// that length is stored at @p lpcchClassReported.
VREGDEF LSTATUS VReg__QueryInfoKeyOwned(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pClass,
    _Out_opt_ LPDWORD lpcchClassReported,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpcSubKeys,
    _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
    _Out_opt_ LPDWORD lpcbMaxClassLen,
    _Out_opt_ LPDWORD lpcValues,
    _Out_opt_ LPDWORD lpcbMaxValueNameLen,
    _Out_opt_ LPDWORD lpcbMaxValueLen,
    _Out_opt_ LPDWORD lpcbSecurityDescriptor,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

// ==========================================================================
// RRF_* filters
// ==========================================================================

/// @ref RRF_SUBKEY_WOW6464KEY and @ref RRF_SUBKEY_WOW6432KEY under names of
/// their own: the SDK defines them only when targeting Windows 10, and VReg
/// honors them whatever its consumer targets. The values are fixed by
/// @ref RegGetValueW.
#define VREG__RRF_WOW6464_SUBKEY ((DWORD)0x00010000)
#define VREG__RRF_WOW6432_SUBKEY ((DWORD)0x00020000)

/// Both view flags: what selects a view, and together an invalid request.
#define VREG__RRF_WOW_SUBKEYS (VREG__RRF_WOW6464_SUBKEY | VREG__RRF_WOW6432_SUBKEY)

#ifdef __cplusplus
#ifdef RRF_SUBKEY_WOW6464KEY
static_assert(VREG__RRF_WOW6464_SUBKEY == RRF_SUBKEY_WOW6464KEY);
#endif
#ifdef RRF_SUBKEY_WOW6432KEY
static_assert(VREG__RRF_WOW6432_SUBKEY == RRF_SUBKEY_WOW6432KEY);
#endif
#endif

// ==========================================================================
// Value types
// ==========================================================================

/// Whether data of @p dwType is UTF-16 text: @ref REG_SZ, @ref REG_EXPAND_SZ
/// or @ref REG_MULTI_SZ.
VREGDEF BOOL VReg__IsStringType(DWORD dwType);

// ==========================================================================
// The handle space
// ==========================================================================

/// One more than the highest index @ref VReg__VirtualHKeyFromIndex accepts.
#define VREG__VIRTUAL_HKEY_COUNT ((VREG_VIRTUAL_HKEY_BASE >> 1) + 1)

#define VREG__HANDLE_BLOCK_SIZE ((UINT_PTR)4096)
#define VREG__HANDLE_BLOCK_COUNT (VREG__VIRTUAL_HKEY_COUNT / VREG__HANDLE_BLOCK_SIZE)
#define VREG__HANDLE_DIRECTORY_SIZE ((UINT_PTR)512)

/// @ref VReg__HandleSpaceShared::magic once a copy has claimed the section:
/// "VRg1".
#define VREG__HANDLE_SPACE_MAGIC 0x56526731L

/// The handle section's contents. Separately built copies of VReg read this
/// layout, so it never changes; a different layout needs a different magic
/// value and section name.
typedef struct VReg__HandleSpaceShared
{
    /// @ref VREG__HANDLE_SPACE_MAGIC once a copy has claimed the section; zero
    /// before.
    LONG magic;
    LONG nextBlock;
} VReg__HandleSpaceShared;

typedef struct VReg__HandleDirectory VReg__HandleDirectory;

/**
 * @brief Hands out virtual handles from the process-wide namespace that every
 *  copy of VReg in the process shares.
 *
 *  Each statically linked copy has its own instance, and all instances in a
 *  process meet in one named section that holds the next free block of
 *  handle values. An instance reserves whole blocks from it and hands out
 *  values from its own blocks without further shared traffic; it owns a
 *  handle exactly when the handle falls in one of its blocks.
 *
 *  Initialized by @ref VReg__HandleSpaceInit; the members are private to
 *  the implementation's "The handle space" section.
 *
 * @note The section is mapped once and never unmapped or closed, so an
 *  instance must outlive every handle it issued: in practice, it is never
 *  destroyed (@ref VReg__ProcessHandleSpace).
 */
typedef struct VReg__HandleSpace
{
    VReg__HandleSpaceShared* pShared;

    SRWLOCK lock;
    UINT_PTR nextIndex;
    UINT_PTR endIndex;

    // Two levels of block directories, filled in as blocks are reserved and
    // never freed: a lookup is two acquire loads and an index.
    VReg__HandleDirectory* volatile directories[VREG__HANDLE_BLOCK_COUNT / VREG__HANDLE_DIRECTORY_SIZE];
} VReg__HandleSpace;

/// Opens or creates the section @p pszName. A failure leaves the instance
/// unusable: @ref VReg__HandleSpaceAllocate then answers null.
VREGDEF void VReg__HandleSpaceInit(_Out_ VReg__HandleSpace* pSpace, _In_z_ LPCWSTR pszName);

/// This copy's instance over this process's section, created on first use
/// and never destroyed.
VREGDEF _Ret_notnull_ VReg__HandleSpace* VReg__ProcessHandleSpace(void);

VREGDEF BOOL VReg__HandleSpaceIsUsable(_In_ const VReg__HandleSpace* pSpace);

/// A fresh handle owned by @p pSpace and bound to @p pTarget, or null when
/// the section is unusable, the namespace is exhausted or memory ran out.
VREGDEF _Ret_maybenull_ HKEY VReg__HandleSpaceAllocate(_Inout_ VReg__HandleSpace* pSpace, _In_opt_ void* pTarget);

/// Whether @p hKey is a virtual handle from one of @p pSpace's blocks.
/// Lock-free.
VREGDEF BOOL VReg__HandleSpaceOwns(_In_ const VReg__HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// Unbinds @p hKey from its target. The value is never handed out again;
/// later lookups answer null.
VREGDEF void VReg__HandleSpaceRelease(_Inout_ VReg__HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// The target @p hKey was allocated for; null when @p pSpace does not own
/// it. Lock-free.
VREGDEF _Ret_maybenull_ void* VReg__HandleSpaceFind(_In_ const VReg__HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// The handle for @p index, counting down from @ref VREG_VIRTUAL_HKEY_BASE in
/// odd steps; null when @p index is out of range.
VREGDEF _Ret_maybenull_ HKEY VReg__VirtualHKeyFromIndex(UINT_PTR index);

/// The inverse of @ref VReg__VirtualHKeyFromIndex; only meaningful when
/// @ref VRegIsVirtualHKey holds.
VREGDEF UINT_PTR VReg__IndexFromVirtualHKey(_In_opt_ HKEY hKey);

// ==========================================================================
// Hive internals
// ==========================================================================

// The node-level interface of a hive, with owned values, as
// `virtual_registry` uses it: caller buffers, ANSI and wide, and handles
// stay there, once for every hive. Every function answers the @ref LSTATUS
// the native registry answers for the same operation; a call on a deleted
// node answers @ref ERROR_KEY_DELETED, and a write to a frozen hive
// @ref ERROR_ACCESS_DENIED.
//
// A string or buffer a function fills is the caller's, allocated with the
// hive's allocation context (@ref VReg__HiveAllocContext) and freed by the
// caller with it.

/// Where an open landed: a node of the hive, or (for a key a frozen layered
/// hive leaves untouched) a handle its lower registry opened, which the
/// caller now owns.
typedef struct VReg__OpenResult
{
    VReg__Node* pNode;
    HKEY hLower;
} VReg__OpenResult;

/// What `RegQueryInfoKeyW` reports, as owned values. Lengths are in
/// characters without terminators, except @ref cbMaxValue.
typedef struct VReg__KeyInfo
{
    VReg__WString className;
    DWORD cSubKeys;
    DWORD cchMaxSubKey;
    DWORD cchMaxClass;
    DWORD cValues;
    DWORD cchMaxValueName;
    DWORD cbMaxValue;
    FILETIME lastWrite;
} VReg__KeyInfo;

typedef enum VReg__UpperKind
{
    VREG__UPPER_KEY,
    VREG__UPPER_VALUE,
} VReg__UpperKind;

/// One entry of a hive's own layer, as @ref VReg__HiveSnapshotUpper lists
/// it; the views point into the hive and are valid during the visit only.
typedef struct VReg__UpperRecord
{
    VReg__UpperKind kind;

    /// The key, relative to the hive's root and separated by backslashes; empty
    /// for the root itself.
    VReg__WStringView path;

    /// Values only; empty for the default value.
    VReg__WStringView name;

    /// Values only.
    DWORD dwType;

    /// Values only.
    const BYTE* pData;
    DWORD cbData;

    /// A key or value of the lower layer that is deleted here.
    BOOL isTombstone;

    /// Keys only: the key hides everything the lower key of this path holds (it
    /// was deleted and created again, or emptied by `RegDeleteTreeW`).
    BOOL isOpaque;
} VReg__UpperRecord;

/// Called once per record under the hive's read lock, so it MUST NOT call
/// back into the hive; a status other than @ref ERROR_SUCCESS stops the
/// snapshot and is answered.
typedef LSTATUS (*VReg__UpperVisitor)(_In_opt_ void* pContext, _In_ const VReg__UpperRecord* pRecord);

VREGDEF _Ret_notnull_ VReg__Node* VReg__HiveRoot(_In_ VRegHive* pHive);

/// The registry below the hive, which answers the handles in
/// @ref VReg__OpenResult::hLower; null for a hive with nothing below it.
VREGDEF _Ret_maybenull_ VRegRegistry* VReg__HiveLower(_In_ VRegHive* pHive);

VREGDEF _Ret_maybenull_ void* VReg__HiveAllocContext(_In_ VRegHive* pHive);

/// The virtual handle of @p pNode.
VREGDEF HKEY VReg__NodeHandle(_In_ const VReg__Node* pNode);

/// The node a virtual handle of this copy of VReg belongs to; null for any
/// other handle.
VREGDEF _Ret_maybenull_ VReg__Node* VReg__NodeOf(_In_opt_ HKEY hKey);

VREGDEF _Ret_notnull_ VRegHive* VReg__NodeHive(_In_ const VReg__Node* pNode);

/// Opens the key @p cPath components below @p pParent. On a writable
/// layered hive the nodes that stand for lower keys are added on the way,
/// which changes nothing the hive answers.
VREGDEF LSTATUS VReg__HiveOpenKey(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pParent,
    _In_reads_(cPath) const VReg__WStringView* pPath,
    SIZE_T cPath,
    REGSAM samDesired,
    _Out_ VReg__OpenResult* pResult
);

/// On a frozen hive an existing key opens as with @ref VReg__HiveOpenKey, and
/// a missing one answers @ref ERROR_ACCESS_DENIED.
VREGDEF LSTATUS VReg__HiveCreateKey(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pParent,
    _In_reads_(cPath) const VReg__WStringView* pPath,
    SIZE_T cPath,
    _In_z_ LPCWSTR pszClassName,
    DWORD dwOptions,
    REGSAM samDesired,
    _Out_ VReg__OpenResult* pResult,
    _Out_ LPDWORD pdwDisposition
);

/// Deletes the last component of the path; a key with subkeys answers
/// @ref ERROR_ACCESS_DENIED, as native does.
VREGDEF LSTATUS VReg__HiveDeleteKey(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pParent,
    _In_reads_(cPath) const VReg__WStringView* pPath,
    SIZE_T cPath
);

/// Deletes every subkey and value of @p pNode, leaving the node itself.
VREGDEF LSTATUS VReg__HiveDeleteTree(_In_ VRegHive* pHive, _Inout_ VReg__Node* pNode);

/// Reads value @p pszName of @p pNode. @p pcbData receives the size of the
/// data in bytes; @p pData receives the data itself only when it is not
/// null, so a null @p pData copies nothing.
VREGDEF LSTATUS VReg__HiveQueryValue(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VReg__Buffer* pData
);

VREGDEF LSTATUS VReg__HiveSetValue(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData
);

VREGDEF LSTATUS VReg__HiveDeleteValue(_In_ VRegHive* pHive, _Inout_ VReg__Node* pNode, _In_z_ LPCWSTR pszName);

/**
 * @brief Sets value @p pszName of @p pNode to @p pszText as a @ref REG_SZ and
 *  marks it MUI text.
 *
 *  `RegLoadMUIStringW` answers an unmarked hive value @ref ERROR_INVALID_DATA,
 *  since resolving an `@` resource reference would load a module. A marked
 *  one stands for the text such a reference resolves to and answers with
 *  @p pszText itself. Any other write of the value drops the mark, and a
 *  `.reg` export carries it as a plain string.
 */
VREGDEF LSTATUS VReg__HiveSetMuiTextValue(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _In_z_ LPCWSTR pszText
);

/// The text of value @p pszName of @p pNode if @ref VReg__HiveSetMuiTextValue
/// stored it, or @ref ERROR_INVALID_DATA for any other value; a value from
/// below is never marked.
VREGDEF LSTATUS VReg__HiveQueryMuiText(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Inout_ VReg__WString* pText
);

VREGDEF LSTATUS VReg__HiveEnumKey(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    DWORD dwIndex,
    _Inout_ VReg__WString* pName,
    _Inout_ VReg__WString* pClassName,
    _Out_ FILETIME* pLastWrite
);

/// Reads the value of @p pNode at @p dwIndex. @p pcbData receives the size
/// of the data in bytes; @p pData receives the data itself only when it
/// is not null, so a null @p pData copies nothing.
VREGDEF LSTATUS VReg__HiveEnumValue(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    DWORD dwIndex,
    _Inout_ VReg__WString* pName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VReg__Buffer* pData
);

/// Fills @p pInfo, whose @ref VReg__KeyInfo::className is the caller's
/// string.
VREGDEF LSTATUS VReg__HiveQueryInfo(_In_ VRegHive* pHive, _In_ VReg__Node* pNode, _Inout_ VReg__KeyInfo* pInfo);

/// What `RegFlushKey` on @p pNode needs from the hive: success, or
/// @ref ERROR_KEY_DELETED.
VREGDEF LSTATUS VReg__HiveCheckKey(_In_ VRegHive* pHive, _In_ VReg__Node* pNode);

/**
 * @brief Registers a one-shot change notification on @p pNode.
 *
 *  @p hEvent is signaled once, by the first write that @p dwFilter selects
 *  (@ref REG_NOTIFY_CHANGE_NAME for a subkey added or deleted,
 *  @ref REG_NOTIFY_CHANGE_LAST_SET for a value set or deleted; the other
 *  bits never fire) on the key, or below it when @p bWatchSubtree.
 *  Deleting a watched key fires its watches too. The hive duplicates
 *  @p hEvent, so the caller MAY close its own handle. A frozen hive
 *  accepts the registration and never signals. Answers
 *  @ref ERROR_KEY_DELETED for a deleted key, or the error of duplicating
 *  @p hEvent.
 *
 * @note Only writes made through the hive are seen; a change in the
 *  registry below a layered hive is not.
 */
VREGDEF LSTATUS VReg__HiveWatch(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    BOOL bWatchSubtree,
    DWORD dwFilter,
    _In_ HANDLE hEvent
);

/**
 * @brief Lists the hive's own layer: what it holds itself, as opposed to
 *  what shows through from below.
 *
 *  This is the seam `.reg` export reads. A key comes before its values,
 *  which come before its subkeys, which are in name order; values are in
 *  creation order. A key that only stands for a lower key, with no values
 *  of its own, is not listed; the hive's own keys below it are. Reads only
 *  the hive, never the lower registry.
 */
VREGDEF LSTATUS VReg__HiveSnapshotUpper(_In_ VRegHive* pHive, _In_ VReg__UpperVisitor pfnVisit, _In_opt_ void* pContext);

/// A key of @ref VReg__HiveLower holding value @p pszName of @p pNode, when
/// that value shows through from below; null (and success) when the hive
/// holds it or nothing does. The caller closes it through the lower
/// registry.
VREGDEF LSTATUS VReg__HiveOpenLowerValueKey(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Out_ PHKEY phLower
);

// ==========================================================================
// Paths
// ==========================================================================

/// The longest key name component the registry accepts, in characters.
#define VREG__MAX_KEY_NAME_CHARS ((SIZE_T)255)

/// Splits @p path at backslashes as the native registry does, appending the
/// components (views into @p path) to @p pComponents, an array of
/// @ref VReg__WStringView: empty components are skipped, a trailing backslash
/// is ignored, a leading one answers @ref ERROR_BAD_PATHNAME and a component
/// over @ref VREG__MAX_KEY_NAME_CHARS @ref ERROR_INVALID_PARAMETER.
VREGDEF LSTATUS VReg__SplitPath(_In_opt_ void* pAllocContext, VReg__WStringView path, _Inout_ VReg__Array* pComponents);

// ==========================================================================
// Status
// ==========================================================================

/// A status a failing path answers, which the analyzer cannot prove nonzero
/// when it was stored by another function.
_Post_satisfies_(return != ERROR_SUCCESS)
VREGDEF LSTATUS VReg__FailureStatus(LSTATUS status);

// ==========================================================================
// Scripts
// ==========================================================================

/// The storage behind one @ref VRegOperation of a script.
typedef struct VReg__ScriptEntry
{
    VReg__WString path;
    VReg__WString name;
    VReg__Buffer data;
} VReg__ScriptEntry;

struct VRegScript
{
    void* pAllocContext;

    /// `VRegOperation`, pointing into @ref entries.
    VReg__Array operations;

    /// `VReg__ScriptEntry`, one per operation.
    VReg__Array entries;
};

/// A predefined root a `.reg` file can name: exports write its long name,
/// imports accept either.
typedef struct VReg__RootKey
{
    const wchar_t* pszLong;
    const wchar_t* pszShort;
    HKEY hKey;
} VReg__RootKey;

/// The long name of a root a `.reg` file can name; null for any other handle.
VREGDEF _Ret_maybenull_z_ const wchar_t* VReg__RootName(_In_opt_ HKEY hRoot);

/// The root named @p name, long or short form, ignoring case.
VREGDEF BOOL VReg__FindRoot(VReg__WStringView name, _Out_ PHKEY phRoot);

VREGDEF LSTATUS VReg__ScriptCreate(_In_opt_ void* pAllocContext, _Outptr_ VRegScript** ppScript);

/// Appends an operation, copying its path, name and data into the script.
VREGDEF LSTATUS VReg__ScriptAppend(
    _Inout_ VRegScript* pScript,
    VRegOperationKind kind,
    _In_ HKEY hRoot,
    VReg__WStringView path,
    VReg__WStringView name,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData,
    SIZE_T line
);

/// Decodes a whole file to text without BOM. Detects the encoding as
/// @ref VRegParseScript documents. Answers @ref ERROR_INVALID_DATA with
/// @p pError (zeroed by the caller) set on bytes the encoding cannot hold.
VREGDEF LSTATUS VReg__DecodeFile(
    _In_opt_ void* pAllocContext,
    _In_reads_bytes_(cbFile) const void* pFile,
    SIZE_T cbFile,
    _Inout_ VReg__WString* pText,
    _Inout_opt_ VRegParseError* pError
);

/// The bytes of @p pText in @p format, with a BOM for Unicode. Answers
/// @ref ERROR_NO_UNICODE_TRANSLATION when the format's code page cannot hold
/// it.
VREGDEF LSTATUS VReg__EncodeFile(
    _In_opt_ void* pAllocContext,
    _In_ const VReg__WString* pText,
    VRegFormat format,
    _Inout_ VReg__Buffer* pFile
);

/// Reads and writes whole files; Win32 errors answer as is.
VREGDEF LSTATUS VReg__ReadWholeFile(_In_opt_ void* pAllocContext, _In_z_ LPCWSTR pszPath, _Inout_ VReg__Buffer* pFile);

VREGDEF LSTATUS VReg__WriteWholeFile(_In_z_ LPCWSTR pszPath, _In_reads_bytes_(cbFile) const void* pFile, SIZE_T cbFile);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VREG__INTERNAL_INCLUDED
#endif // VREG__INTERNAL

#ifdef VREG_IMPLEMENTATION
#ifndef VREG_IMPLEMENTATION_INCLUDED
#define VREG_IMPLEMENTATION_INCLUDED

#pragma comment(lib, "advapi32")

// ==========================================================================
// Allocation
// ==========================================================================

VREGDEF void VReg__AssertFailed(void)
{
    if (IsDebuggerPresent())
        __debugbreak();
}

VREGDEF _Ret_maybenull_ void* VReg__Alloc(_In_opt_ void* const pAllocContext, const SIZE_T cb)
{
    (void)pAllocContext;
    if (cb == 0)
        return NULL;

    return VREG_MALLOC(pAllocContext, cb);
}

VREGDEF _Ret_maybenull_ void* VReg__AllocZeroed(_In_opt_ void* const pAllocContext, const SIZE_T cb)
{
    void* const p = VReg__Alloc(pAllocContext, cb);
    if (p)
        ZeroMemory(p, cb);

    return p;
}

VREGDEF _Ret_maybenull_ void* VReg__Realloc(_In_opt_ void* const pAllocContext, _In_opt_ void* const p, const SIZE_T cb)
{
    (void)pAllocContext;
    if (!p)
        return VReg__Alloc(pAllocContext, cb);
    if (cb == 0)
    {
        VREG_FREE(pAllocContext, p);
        return NULL;
    }

    return VREG_REALLOC(pAllocContext, p, cb);
}

VREGDEF void VReg__Free(_In_opt_ void* const pAllocContext, _In_opt_ void* const p)
{
    (void)pAllocContext;
    if (p)
        VREG_FREE(pAllocContext, p);
}

// ==========================================================================
// ANSI conversion
// ==========================================================================

static SIZE_T VReg__AnsiLength(_In_z_ const char* const psz)
{
    SIZE_T cch = 0;
    while (psz[cch] != '\0')
        ++cch;

    return cch;
}

/// Widens @p cch bytes in @p codePage into @p pText with the
/// @ref MultiByteToWideChar flags @p dwFlags. Answers
/// @ref ERROR_NO_UNICODE_TRANSLATION when the conversion fails, leaving
/// @p pText empty.
static LSTATUS VReg__WideFromMultiByte(
    _In_opt_ void* const pAllocContext,
    const UINT codePage,
    const DWORD dwFlags,
    _In_reads_(cch) const char* const pch,
    const SIZE_T cch,
    _Inout_ VReg__WString* const pText
)
{
    int cchWide;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (!VReg__WStringResize(pAllocContext, pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cchWide = MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, NULL, 0);
    if (cchWide <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VReg__WStringResize(pAllocContext, pText, (SIZE_T)cchWide))
        return ERROR_OUTOFMEMORY;

    MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, pText->psz, cchWide);
    return ERROR_SUCCESS;
}

/// Narrows @p cch UTF-16 units into @p pText in @p codePage with the
/// @ref WideCharToMultiByte flags @p dwFlags, reporting a default character
/// through @p pfUsedDefault. Fails as @ref VReg__WideFromMultiByte does.
static LSTATUS VReg__MultiByteFromWide(
    _In_opt_ void* const pAllocContext,
    const UINT codePage,
    const DWORD dwFlags,
    _Out_opt_ BOOL* const pfUsedDefault,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch,
    _Inout_ VReg__Buffer* const pText
)
{
    int cbText;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (!VReg__BufferResize(pAllocContext, pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cbText = WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, NULL, 0, NULL, pfUsedDefault);
    if (cbText <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VReg__BufferResize(pAllocContext, pText, (SIZE_T)cbText))
        return ERROR_OUTOFMEMORY;

    WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, (char*)pText->pb, cbText, NULL, pfUsedDefault);
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS VReg__WideFromAnsi(
    _In_opt_ void* const pAllocContext,
    _In_reads_(cch) const char* const pch,
    const SIZE_T cch,
    _Inout_ VReg__WString* const pText
)
{
    return VReg__WideFromMultiByte(pAllocContext, CP_ACP, 0, pch, cch, pText);
}

VREGDEF LSTATUS VReg__AnsiFromWide(
    _In_opt_ void* const pAllocContext,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch,
    _Inout_ VReg__Buffer* const pText
)
{
    return VReg__MultiByteFromWide(pAllocContext, CP_ACP, 0, NULL, pch, cch, pText);
}

VREGDEF LSTATUS VReg__WideArgInit(
    _In_opt_ void* const pAllocContext,
    _In_opt_z_ const LPCSTR psz,
    _Out_ VReg__WideArg* const pArg
)
{
    ZeroMemory(pArg, sizeof(*pArg));
    pArg->isNull = psz == NULL;
    if (!psz)
        return ERROR_SUCCESS;

    return VReg__WideFromAnsi(pAllocContext, psz, VReg__AnsiLength(psz), &pArg->text);
}

VREGDEF _Ret_maybenull_z_ LPCWSTR VReg__WideArgText(_In_ const VReg__WideArg* const pArg)
{
    return pArg->isNull ? NULL : VReg__WStringText(&pArg->text);
}

VREGDEF _Ret_maybenull_ LPWSTR VReg__WideArgMutable(_In_ VReg__WideArg* const pArg)
{
    if (pArg->isNull)
        return NULL;

    // An unallocated empty string has no storage to hand out.
    if (!pArg->text.psz && !VReg__WStringReserve(NULL, &pArg->text, 0))
        return NULL;

    return pArg->text.psz;
}

VREGDEF void VReg__WideArgFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__WideArg* const pArg)
{
    VReg__WStringFree(pAllocContext, &pArg->text);
}

// ==========================================================================
// Buffers
// ==========================================================================

/// The capacity to grow to for @p needed elements: at least double the old
/// one, so repeated appends stay linear.
static SIZE_T VReg__GrownCapacity(const SIZE_T capacity, const SIZE_T needed)
{
    SIZE_T grown = capacity < 8 ? 8 : capacity * 2;
    return grown < needed ? needed : grown;
}

VREGDEF DWORD VReg__MinDword(const DWORD a, const DWORD b)
{
    return a < b ? a : b;
}

VREGDEF DWORD VReg__MaxDword(const DWORD a, const DWORD b)
{
    return a > b ? a : b;
}

static SIZE_T VReg__MinSize(const SIZE_T a, const SIZE_T b)
{
    return a < b ? a : b;
}

VREGDEF VReg__WStringView VReg__ViewOf(_In_opt_z_ const LPCWSTR psz)
{
    VReg__WStringView view;
    view.pch = psz;
    view.cch = 0;
    if (psz)
        while (psz[view.cch] != L'\0')
            ++view.cch;

    return view;
}

VREGDEF BOOL VReg__BufferReserve(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Buffer* const pBuffer,
    const SIZE_T cbCapacity
)
{
    BYTE* pb;
    if (cbCapacity <= pBuffer->cbCapacity)
        return TRUE;

    pb = (BYTE*)VReg__Realloc(pAllocContext, pBuffer->pb, cbCapacity);
    if (!pb)
        return FALSE;

    pBuffer->pb = pb;
    pBuffer->cbCapacity = cbCapacity;
    return TRUE;
}

VREGDEF BOOL VReg__BufferResize(_In_opt_ void* const pAllocContext, _Inout_ VReg__Buffer* const pBuffer, const SIZE_T cb)
{
    if (cb > pBuffer->cbCapacity)
    {
        const SIZE_T cbGrown = VReg__GrownCapacity(pBuffer->cbCapacity, cb);
        if (!VReg__BufferReserve(pAllocContext, pBuffer, cbGrown))
            return FALSE;
    }

    pBuffer->cb = cb;
    return TRUE;
}

VREGDEF BOOL VReg__BufferAssign(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Buffer* const pBuffer,
    _In_reads_bytes_opt_(cb) const void* const p,
    const SIZE_T cb
)
{
    if (!VReg__BufferResize(pAllocContext, pBuffer, cb))
        return FALSE;

    if (cb != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(p != NULL);
        CopyMemory(pBuffer->pb, p, cb);
    }

    return TRUE;
}

VREGDEF BOOL VReg__BufferAppend(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Buffer* const pBuffer,
    _In_reads_bytes_(cb) const void* const p,
    const SIZE_T cb
)
{
    const SIZE_T cbOld = pBuffer->cb;
    if (!VReg__BufferResize(pAllocContext, pBuffer, cbOld + cb))
        return FALSE;

    if (cb != 0)
        CopyMemory(pBuffer->pb + cbOld, p, cb);

    return TRUE;
}

VREGDEF void VReg__BufferFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Buffer* const pBuffer)
{
    VReg__Free(pAllocContext, pBuffer->pb);
    pBuffer->pb = NULL;
    pBuffer->cb = 0;
    pBuffer->cbCapacity = 0;
}

VREGDEF _Ret_z_ LPCWSTR VReg__WStringText(_In_ const VReg__WString* const pString)
{
    return pString->psz ? pString->psz : L"";
}

VREGDEF VReg__WStringView VReg__WStringViewOf(_In_ const VReg__WString* const pString)
{
    VReg__WStringView view;
    view.pch = VReg__WStringText(pString);
    view.cch = pString->cch;
    return view;
}

VREGDEF BOOL VReg__WStringReserve(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString, const SIZE_T cch)
{
    wchar_t* psz;
    if (cch < pString->cchCapacity)
        return TRUE;

    psz = (wchar_t*)VReg__Realloc(pAllocContext, pString->psz, (cch + 1) * sizeof(wchar_t));
    if (!psz)
        return FALSE;

    if (!pString->psz)
        psz[0] = L'\0';

    pString->psz = psz;
    pString->cchCapacity = cch + 1;
    return TRUE;
}

VREGDEF BOOL VReg__WStringResize(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString, const SIZE_T cch)
{
    if (cch >= pString->cchCapacity)
    {
        const SIZE_T cchGrown = VReg__GrownCapacity(pString->cchCapacity, cch + 1) - 1;
        if (!VReg__WStringReserve(pAllocContext, pString, cchGrown))
            return FALSE;
    }

    if (pString->psz)
        pString->psz[cch] = L'\0';

    pString->cch = cch;
    return TRUE;
}

VREGDEF BOOL VReg__WStringAssign(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pString,
    _In_reads_opt_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    if (!VReg__WStringResize(pAllocContext, pString, cch))
        return FALSE;

    if (cch != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(pch != NULL);
        CopyMemory(pString->psz, pch, cch * sizeof(wchar_t));
    }

    return TRUE;
}

VREGDEF BOOL VReg__WStringAppend(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pString,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    const SIZE_T cchOld = pString->cch;
    if (!VReg__WStringResize(pAllocContext, pString, cchOld + cch))
        return FALSE;

    if (cch != 0)
        CopyMemory(pString->psz + cchOld, pch, cch * sizeof(wchar_t));

    return TRUE;
}

VREGDEF BOOL VReg__WStringAppendChar(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString, const wchar_t ch)
{
    return VReg__WStringAppend(pAllocContext, pString, &ch, 1);
}

/// Appends a key path component, after a backslash unless @p pPath is empty.
static BOOL VReg__AppendPathComponent(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pPath,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    if (pPath->cch != 0 && !VReg__WStringAppendChar(pAllocContext, pPath, L'\\'))
        return FALSE;

    return VReg__WStringAppend(pAllocContext, pPath, pch, cch);
}

VREGDEF void VReg__WStringTruncate(_Inout_ VReg__WString* const pString, const SIZE_T cch)
{
    VREG_ASSERT(cch <= pString->cch);
    pString->cch = cch;
    if (pString->psz)
        pString->psz[cch] = L'\0';
}

VREGDEF void VReg__WStringFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString)
{
    VReg__Free(pAllocContext, pString->psz);
    pString->psz = NULL;
    pString->cch = 0;
    pString->cchCapacity = 0;
}

VREGDEF BOOL VReg__ArrayReserve(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pArray,
    const SIZE_T cbElement,
    const SIZE_T capacity
)
{
    void* p;
    if (capacity <= pArray->capacity)
        return TRUE;

    p = VReg__Realloc(pAllocContext, pArray->p, capacity * cbElement);
    if (!p)
        return FALSE;

    pArray->p = p;
    pArray->capacity = capacity;
    return TRUE;
}

VREGDEF BOOL VReg__ArrayInsert(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pArray,
    const SIZE_T cbElement,
    const SIZE_T index,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    BYTE* pbAt;
    VREG_ASSERT(index <= pArray->count);
    if (pArray->count == pArray->capacity)
    {
        const SIZE_T grown = VReg__GrownCapacity(pArray->capacity, pArray->count + 1);
        if (!VReg__ArrayReserve(pAllocContext, pArray, cbElement, grown))
            return FALSE;
    }

    pbAt = (BYTE*)pArray->p + index * cbElement;
    MoveMemory(pbAt + cbElement, pbAt, (pArray->count - index) * cbElement);
    CopyMemory(pbAt, pElement, cbElement);
    ++pArray->count;
    return TRUE;
}

VREGDEF BOOL VReg__ArrayAppend(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pArray,
    const SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    return VReg__ArrayInsert(pAllocContext, pArray, cbElement, pArray->count, pElement);
}

VREGDEF void VReg__ArrayRemoveAt(_Inout_ VReg__Array* const pArray, const SIZE_T cbElement, const SIZE_T index)
{
    BYTE* const pbAt = (BYTE*)pArray->p + index * cbElement;
    VREG_ASSERT(index < pArray->count);
    MoveMemory(pbAt, pbAt + cbElement, (pArray->count - index - 1) * cbElement);
    --pArray->count;
}

VREGDEF void VReg__ArrayFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Array* const pArray)
{
    VReg__Free(pAllocContext, pArray->p);
    pArray->p = NULL;
    pArray->count = 0;
    pArray->capacity = 0;
}

VREGDEF BOOL VReg__WStringArrayAppend(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pArray,
    const VReg__WStringView text
)
{
    VReg__WString copy = {NULL, 0, 0};
    if (!VReg__WStringAssign(pAllocContext, &copy, text.pch, text.cch))
        return FALSE;
    if (!VReg__ArrayAppend(pAllocContext, pArray, sizeof(copy), &copy))
    {
        VReg__WStringFree(pAllocContext, &copy);
        return FALSE;
    }

    return TRUE;
}

VREGDEF void VReg__WStringArrayFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Array* const pArray)
{
    SIZE_T i;
    for (i = 0; i < pArray->count; ++i)
        VReg__WStringFree(pAllocContext, &VREG__AT(pArray, VReg__WString, i));

    VReg__ArrayFree(pAllocContext, pArray);
}

VREGDEF BOOL VReg__WStringArrayInsertSorted(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pList,
    const VReg__WStringView name
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pList->count;
    VReg__WString copy = {NULL, 0, 0};
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const int order = VReg__CompareNames(VReg__WStringViewOf(&VREG__AT(pList, VReg__WString, mid)), name);
        if (order == 0)
            return TRUE;
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (!VReg__WStringAssign(pAllocContext, &copy, name.pch, name.cch))
        return FALSE;
    if (!VReg__ArrayInsert(pAllocContext, pList, sizeof(copy), lo, &copy))
    {
        VReg__WStringFree(pAllocContext, &copy);
        return FALSE;
    }

    return TRUE;
}

VREGDEF BOOL VReg__WStringArrayContainsSorted(_In_ const VReg__Array* const pList, const VReg__WStringView name)
{
    SIZE_T lo = 0;
    SIZE_T hi = pList->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const int order = VReg__CompareNames(VReg__WStringViewOf(&VREG__AT(pList, VReg__WString, mid)), name);
        if (order == 0)
            return TRUE;
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return FALSE;
}

// ==========================================================================
// Export tables
// ==========================================================================

/// Makes every module linking VReg import advapi32, so the loader maps it
/// before VReg runs and resolution finds it without loading anything. The
/// address is taken in a function body: an initializer would need the import
/// thunk's address, which C does not guarantee to be constant.
static void* volatile g_pfnImportAdvapi32;

/// Ordinal comparison of two export names: negative, zero or positive.
static int VReg__CompareExportNames(_In_z_ const char* pszLeft, _In_z_ const char* pszRight)
{
    while (*pszLeft != '\0' && *pszLeft == *pszRight)
    {
        ++pszLeft;
        ++pszRight;
    }

    return (int)(unsigned char)*pszLeft - (int)(unsigned char)*pszRight;
}

VREGDEF _Ret_maybenull_ void* VReg__FindExport(_In_opt_ const HMODULE hModule, _In_z_ const char* const pszName)
{
    const BYTE* pBase;
    const IMAGE_DOS_HEADER* pDos;
    const IMAGE_NT_HEADERS* pNt;
    const IMAGE_DATA_DIRECTORY* pDirectory;
    const IMAGE_EXPORT_DIRECTORY* pExports;
    const DWORD* pNames;
    const WORD* pOrdinals;
    const DWORD* pFunctions;
    DWORD low;
    DWORD high;

    if (!hModule)
        return NULL;

    pBase = (const BYTE*)hModule;
    pDos = (const IMAGE_DOS_HEADER*)pBase;
    pNt = (const IMAGE_NT_HEADERS*)(pBase + pDos->e_lfanew);
    pDirectory = &pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (pDirectory->Size == 0)
        return NULL;

    pExports = (const IMAGE_EXPORT_DIRECTORY*)(pBase + pDirectory->VirtualAddress);
    pNames = (const DWORD*)(pBase + pExports->AddressOfNames);
    pOrdinals = (const WORD*)(pBase + pExports->AddressOfNameOrdinals);
    pFunctions = (const DWORD*)(pBase + pExports->AddressOfFunctions);

    // The name table is sorted by ordinal string comparison; the loader
    // binary-searches it the same way.
    low = 0;
    high = pExports->NumberOfNames;
    while (low < high)
    {
        const DWORD middle = low + (high - low) / 2;
        const int order = VReg__CompareExportNames(pszName, (const char*)(pBase + pNames[middle]));
        if (order == 0)
        {
            const DWORD rva = pFunctions[pOrdinals[middle]];

            // An address inside the export directory is a forwarder string.
            if (rva >= pDirectory->VirtualAddress && rva < pDirectory->VirtualAddress + pDirectory->Size)
                return NULL;

            return (BYTE*)hModule + rva;
        }

        if (order < 0)
            high = middle;
        else
            low = middle + 1;
    }

    return NULL;
}

VREGDEF _Ret_maybenull_ void* VReg__FindRegistryExport(_In_z_ const char* const pszName)
{
    void* const pFunction = VReg__FindExport(GetModuleHandleW(L"KernelBase.dll"), pszName);
    g_pfnImportAdvapi32 = (void*)&RegOpenKeyW;
    if (pFunction)
        return pFunction;

    return VReg__FindExport(GetModuleHandleW(L"advapi32.dll"), pszName);
}

// ==========================================================================
// Derived forms
// ==========================================================================

// The forms Win32 builds from the wide `Ex` ones: ANSI, pre-`Ex` and
// the conveniences. Each calls its slot when the registry fills it and
// derives the call from the wide forms otherwise; ANSI forms widen and
// narrow through @ref CP_ACP in both directions, as the native ANSI
// functions do. Every one of them answers what the native function answers
// for the same input: status, out-sizes and bytes.

typedef struct VReg__PlacedValue
{
    DWORD cbValue;
    DWORD dwType;
    DWORD ibValue;
} VReg__PlacedValue;

static DWORD VReg__AlignedToDword(const DWORD cb)
{
    return (cb + 3) & ~(DWORD)3;
}

/// Copies @p cb bytes; a null @p pSource is legal only for none.
static void VReg__CopyBytes(
    _Out_writes_bytes_(cb) void* const pDestination,
    _In_reads_bytes_opt_(cb) const void* const pSource,
    const SIZE_T cb
)
{
    if (cb == 0)
        return;

    _Analysis_assume_(pSource != NULL);
    CopyMemory(pDestination, pSource, cb);
}

/// Copies @p pText and a terminator into a name buffer of @p cchBuffer bytes.
/// False if it does not fit: native then writes nothing unless the buffer is
/// exactly the text's length, in which case it writes the text with its last
/// character replaced by the terminator.
static BOOL VReg__CopyAnsiName(
    _In_ const VReg__Buffer* const pText,
    _Out_writes_opt_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    if (!pszBuffer)
        return FALSE;

    if (pText->cb < cchBuffer)
    {
        VReg__CopyBytes(pszBuffer, pText->pb, pText->cb);
        pszBuffer[pText->cb] = '\0';
        return TRUE;
    }

    if (pText->cb == cchBuffer && cchBuffer > 0)
    {
        VReg__CopyBytes(pszBuffer, pText->pb, pText->cb - 1);
        pszBuffer[pText->cb - 1] = '\0';
    }
    return FALSE;
}

/// Like @ref VReg__CopyAnsiName for a class buffer, which native fills with as
/// much of the text as fits (and a terminator) whenever it has room for one.
static BOOL VReg__CopyAnsiClass(
    _In_ const VReg__Buffer* const pText,
    _Out_writes_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    SIZE_T cchCopied;
    if (cchBuffer == 0)
        return FALSE;

    cchCopied = VReg__MinSize(pText->cb, cchBuffer - 1);
    VReg__CopyBytes(pszBuffer, pText->pb, cchCopied);
    pszBuffer[cchCopied] = '\0';
    return pText->cb < cchBuffer;
}

VREGDEF LSTATUS WINAPI VRegOpenKeyExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegOpenKeyExA)
        return pRegistry->lpVtbl->RegOpenKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    HKEY hPrevious;
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    status = VRegOpenKeyExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey),
        ulOptions,
        samDesired,
        phkResult
    );
    // The native ANSI form leaves the result alone for an invalid handle; the
    // wide form nulls it.
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

VREGDEF LSTATUS WINAPI VRegCreateKeyExA(
    _In_ VRegRegistry* const pRegistry,
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
    if (pRegistry->lpVtbl->RegCreateKeyExA)
        return pRegistry->lpVtbl->RegCreateKeyExA(
            pRegistry,
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

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    VReg__WideArg className = {FALSE, {NULL, 0, 0}};
    HKEY hPrevious;
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__WideArgInit(pAllocContext, lpClass, &className);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VRegCreateKeyExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey),
        Reserved,
        VReg__WideArgMutable(&className),
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition
    );
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VReg__WideArgFree(pAllocContext, &className);
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

VREGDEF LSTATUS WINAPI VRegQueryValueExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (pRegistry->lpVtbl->RegQueryValueExA)
        return pRegistry->lpVtbl->RegQueryValueExA(
            pRegistry,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg valueName = {FALSE, {NULL, 0, 0}};
    VReg__Buffer value = {NULL, 0, 0};
    VReg__Buffer text = {NULL, 0, 0};
    DWORD dwType;
    DWORD cbSize;
    DWORD cbText;
    LSTATUS sized;
    LSTATUS status;

    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VReg__WideArgInit(pAllocContext, lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!lpData && !lpcbData)
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            VReg__WideArgText(&valueName),
            NULL,
            lpType,
            NULL,
            NULL
        );
        goto done;
    }

    // Deliberately reads the _Out_ parameter: a failure below writes it back.
#pragma warning(suppress: 6001)
    dwType = lpType ? *lpType : 0;
    cbSize = (!lpData && lpcbData) ? *lpcbData : 0;
    sized = VRegQueryValueExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName),
        NULL,
        &dwType,
        NULL,
        &cbSize
    );
    if (sized != ERROR_SUCCESS)
    {
        if (lpType)
            *lpType = dwType;
        if (!lpData && lpcbData)
            *lpcbData = cbSize;
        status = sized;
        goto done;
    }

    if (!VReg__IsStringType(dwType))
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            VReg__WideArgText(&valueName),
            NULL,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    status = VReg__QueryValueOwned(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName),
        pAllocContext,
        &dwType,
        &value
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__AnsiFromWide(pAllocContext, (const wchar_t*)value.pb, value.cb / sizeof(wchar_t), &text);
    if (status != ERROR_SUCCESS)
        goto done;

    cbText = (DWORD)text.cb;
    if (lpData)
    {
        if (cbText > *lpcbData)
        {
            status = ERROR_MORE_DATA;
        }
        else
        {
            // Native reads the wide data into the caller's buffer when it fits
            // there and narrows it in place, so the bytes past the text are the
            // tail of the wide data; it also terminates text that has no
            // terminator if there is room.
            if (value.cb <= *lpcbData)
                VReg__CopyBytes(lpData, value.pb, value.cb);
            if (cbText > 0)
            {
                CopyMemory(lpData, text.pb, cbText);
                if (cbText < *lpcbData && lpData[cbText - 1] != 0)
                    lpData[cbText] = 0;
            }
        }
    }

    if (lpType)
        *lpType = dwType;
    if (lpcbData)
        *lpcbData = cbText;

done:
    VReg__BufferFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &value);
    VReg__WideArgFree(pAllocContext, &valueName);
    return status;}

VREGDEF LSTATUS WINAPI VRegSetValueExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetValueExA)
        return pRegistry->lpVtbl->RegSetValueExA(
            pRegistry,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg valueName = {FALSE, {NULL, 0, 0}};
    VReg__WString text = {NULL, 0, 0};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg__IsStringType(dwType) || !lpData)
    {
        status = VRegSetValueExW(
            pRegistry,
            hKey,
            VReg__WideArgText(&valueName),
            Reserved,
            dwType,
            lpData,
            cbData
        );
        goto done;
    }

    status = VReg__WideFromAnsi(pAllocContext, (const char*)lpData, cbData, &text);
    if (status != ERROR_SUCCESS)
        goto done;

    // Native reads the byte after unterminated data and keeps it as the
    // terminator when it is zero, which it nearly always is. Agent's choice:
    // always terminate, so no byte past cbData is ever read; reading it, as
    // native does, can fault at the end of a page.
    if (text.cch > 0 && text.psz[text.cch - 1] != L'\0')
    {
        if (!VReg__WStringAppendChar(pAllocContext, &text, L'\0'))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    status = VRegSetValueExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName),
        Reserved,
        dwType,
        (const BYTE*)VReg__WStringText(&text),
        (DWORD)(text.cch * sizeof(wchar_t))
    );

done:
    VReg__WStringFree(pAllocContext, &text);
    VReg__WideArgFree(pAllocContext, &valueName);
    return status;}

VREGDEF LSTATUS WINAPI VRegDeleteValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (pRegistry->lpVtbl->RegDeleteValueA)
        return pRegistry->lpVtbl->RegDeleteValueA(
            pRegistry,
            hKey,
            lpValueName
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg valueName = {FALSE, {NULL, 0, 0}};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegDeleteValueW(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName)
    );

done:
    VReg__WideArgFree(pAllocContext, &valueName);
    return status;}

VREGDEF LSTATUS WINAPI VRegDeleteKeyExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyExA)
        return pRegistry->lpVtbl->RegDeleteKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VRegDeleteKeyExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey),
        samDesired,
        Reserved
    );

done:
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

VREGDEF LSTATUS WINAPI VRegDeleteTreeA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    if (pRegistry->lpVtbl->RegDeleteTreeA)
        return pRegistry->lpVtbl->RegDeleteTreeA(
            pRegistry,
            hKey,
            lpSubKey
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegDeleteTreeW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey)
    );

done:
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

VREGDEF LSTATUS WINAPI VRegEnumKeyExA(
    _In_ VRegRegistry* const pRegistry,
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
    if (pRegistry->lpVtbl->RegEnumKeyExA)
        return pRegistry->lpVtbl->RegEnumKeyExA(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            lpcchName,
            lpReserved,
            lpClass,
            lpcchClass,
            lpftLastWriteTime
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WString name = {NULL, 0, 0};
    VReg__WString className = {NULL, 0, 0};
    VReg__Buffer nameA = {NULL, 0, 0};
    VReg__Buffer classA = {NULL, 0, 0};
    FILETIME ftLastWrite;
    BOOL wantClass;
    DWORD cchName;
    DWORD cchClass;
    DWORD cchNameOut;
    DWORD cchClassOut;
    LSTATUS status;

    if (!lpcchName || (lpClass && !lpcchClass))
        return ERROR_INVALID_PARAMETER;

    wantClass = lpClass || lpcchClass;
    cchName = VREG__INITIAL_NAME_CHARS;
    cchClass = wantClass ? VREG__INITIAL_NAME_CHARS : 0;
    ZeroMemory(&ftLastWrite, sizeof(ftLastWrite));
    for (;;)
    {
        if (!VReg__WStringResize(pAllocContext, &name, cchName)
            || !VReg__WStringResize(pAllocContext, &className, cchClass))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cchNameOut = cchName;
        cchClassOut = cchClass;
        status = VRegEnumKeyExW(
            pRegistry,
            hKey,
            dwIndex,
            name.psz,
            &cchNameOut,
            lpReserved,
            wantClass ? className.psz : NULL,
            wantClass ? &cchClassOut : NULL,
            &ftLastWrite
        );
        if (
            status != ERROR_MORE_DATA
            || (cchName >= VREG__MAX_CLASS_CHARS && cchClass >= VREG__MAX_CLASS_CHARS)
        )
            break;

        cchName = VReg__MinDword(cchName * 2, VREG__MAX_CLASS_CHARS);
        if (wantClass)
            cchClass = VReg__MinDword(cchClass * 2, VREG__MAX_CLASS_CHARS);
    }
    if (status != ERROR_SUCCESS)
        goto done;

    VReg__WStringTruncate(&name, cchNameOut);
    VReg__WStringTruncate(&className, cchClassOut);
    if (lpftLastWriteTime)
        *lpftLastWriteTime = ftLastWrite;

    status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg__CopyAnsiName(&nameA, lpName, *lpcchName))
    {
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchName = (DWORD)nameA.cb;

    if (lpcchClass && !lpClass)
    {
        *lpcchClass = 0;
    }
    else if (lpcchClass)
    {
        status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&className), className.cch, &classA);
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VReg__CopyAnsiClass(&classA, lpClass, *lpcchClass))
        {
            status = ERROR_MORE_DATA;
            goto done;
        }
        *lpcchClass = (DWORD)classA.cb;
    }

    status = ERROR_SUCCESS;

done:
    VReg__BufferFree(pAllocContext, &classA);
    VReg__BufferFree(pAllocContext, &nameA);
    VReg__WStringFree(pAllocContext, &className);
    VReg__WStringFree(pAllocContext, &name);
    return status;}

VREGDEF LSTATUS WINAPI VRegEnumValueA(
    _In_ VRegRegistry* const pRegistry,
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
    if (pRegistry->lpVtbl->RegEnumValueA)
        return pRegistry->lpVtbl->RegEnumValueA(
            pRegistry,
            hKey,
            dwIndex,
            lpValueName,
            lpcchValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WString name = {NULL, 0, 0};
    VReg__Buffer value = {NULL, 0, 0};
    VReg__Buffer nameA = {NULL, 0, 0};
    VReg__Buffer text = {NULL, 0, 0};
    DWORD dwType = 0;
    DWORD cbValue;
    DWORD cbBuffer;
    BOOL isString;
    BOOL nameFits;
    LSTATUS status;

    if (!lpValueName || !lpcchValueName || (lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VReg__EnumValueOwned(
        pRegistry,
        hKey,
        dwIndex,
        pAllocContext,
        &name,
        &dwType,
        lpcbData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    isString = VReg__IsStringType(dwType);
    cbValue = (DWORD)value.cb;
    if (lpcbData && isString)
    {
        status = VReg__AnsiFromWide(pAllocContext, (const wchar_t*)value.pb, value.cb / sizeof(wchar_t), &text);
        if (status != ERROR_SUCCESS)
            goto done;

        cbValue = (DWORD)text.cb;
    }

    // Native reports the type and size even when the call goes on to fail.
    if (lpType)
        *lpType = dwType;
    cbBuffer = lpcbData ? *lpcbData : 0;
    if (lpcbData)
        *lpcbData = cbValue;
    if (lpData && cbValue > cbBuffer)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    if (nameA.cb > *lpcchValueName)
    {
        // Native terminates the text in the caller's buffer here, and writes
        // nothing else.
        if (lpData && isString && cbValue > 0 && cbValue < cbBuffer)
            lpData[cbValue] = 0;
        status = ERROR_MORE_DATA;
        goto done;
    }

    nameFits = VReg__CopyAnsiName(&nameA, lpValueName, *lpcchValueName);
    if (lpData)
    {
        if (isString)
        {
            if (value.cb <= cbBuffer)
                VReg__CopyBytes(lpData, value.pb, value.cb);
            if (cbValue > 0)
            {
                CopyMemory(lpData, text.pb, cbValue);
                // The capacity is the caller's *lpcbData, which was overwritten
                // above; cbBuffer holds it.
#pragma warning(suppress: 6386)
                if (cbValue < cbBuffer && lpData[cbValue - 1] != 0)
                    lpData[cbValue] = 0;
            }
        }
        else
        {
            VReg__CopyBytes(lpData, value.pb, cbValue);
        }
    }

    if (!nameFits)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchValueName = (DWORD)nameA.cb;
    status = ERROR_SUCCESS;

done:
    VReg__BufferFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &nameA);
    VReg__BufferFree(pAllocContext, &value);
    VReg__WStringFree(pAllocContext, &name);
    return status;}

// Native answers success without writing lpClass when only the class length is
// asked for, which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
VREGDEF LSTATUS WINAPI VRegQueryInfoKeyA(
    _In_ VRegRegistry* const pRegistry,
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
    if (pRegistry->lpVtbl->RegQueryInfoKeyA)
        return pRegistry->lpVtbl->RegQueryInfoKeyA(
            pRegistry,
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

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WString className = {NULL, 0, 0};
    VReg__Buffer classA = {NULL, 0, 0};
    BOOL wantClass;
    BOOL wantClassLength;
    LSTATUS status;

    if (lpClass && !lpcchClass)
        return ERROR_INVALID_PARAMETER;

    // Native answers success for a zero-length class buffer, without a class,
    // but reports the class length if the caller asked for the longest class.
    wantClass = lpClass && *lpcchClass != 0;
    wantClassLength = lpClass && !wantClass && lpcbMaxClassLen;
    if (!wantClass && !wantClassLength)
    {
        return VRegQueryInfoKeyW(
            pRegistry,
            hKey,
            NULL,
            lpClass ? NULL : lpcchClass,
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

    status = VReg__QueryInfoKeyOwned(
        pRegistry,
        hKey,
        pAllocContext,
        &className,
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
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&className), className.cch, &classA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (wantClassLength)
    {
        *lpcchClass = (DWORD)classA.cb;
        status = ERROR_SUCCESS;
        goto done;
    }
    if (!VReg__CopyAnsiClass(&classA, lpClass, *lpcchClass))
    {
        *lpcchClass -= 1;
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchClass = (DWORD)classA.cb;
    status = ERROR_SUCCESS;

done:
    VReg__BufferFree(pAllocContext, &classA);
    VReg__WStringFree(pAllocContext, &className);
    return status;}

VREGDEF LSTATUS WINAPI VRegLoadMUIStringA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
)
{
    if (pRegistry->lpVtbl->RegLoadMUIStringA)
        return pRegistry->lpVtbl->RegLoadMUIStringA(
            pRegistry,
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );

    (void)hKey;
    (void)pszValue;
    (void)pszOutBuf;
    (void)cbOutBuf;
    (void)pcbData;
    (void)Flags;
    (void)pszDirectory;
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// The SDK's _Post_z_ holds on every return, truncated reads and failures
// included; the analyzer cannot see the terminators written at run time.
#pragma warning(suppress: 6054)
VREGDEF LSTATUS WINAPI VRegGetValueA(
    _In_ VRegRegistry* const pRegistry,
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
    if (pRegistry->lpVtbl->RegGetValueA)
        return pRegistry->lpVtbl->RegGetValueA(
            pRegistry,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    VReg__WideArg value = {FALSE, {NULL, 0, 0}};
    VReg__Buffer data = {NULL, 0, 0};
    VReg__Buffer stored = {NULL, 0, 0};
    VReg__Buffer text = {NULL, 0, 0};
    VReg__Buffer rawText = {NULL, 0, 0};
    VReg__Buffer storedText = {NULL, 0, 0};
    DWORD dwFlagsStored;
    DWORD dwFlagsTyped;
    DWORD dwFlagsAnyType;
    DWORD dwFlagsRead;
    DWORD dwStoredType;
    DWORD dwType;
    DWORD dwReadType;
    DWORD cbQuery;
    DWORD cbStoredQuery;
    DWORD cbStoredRaw;
    DWORD cbStored;
    DWORD cbText;
    DWORD cbRawText;
    DWORD cbExpanded;
    DWORD cbReported;
    DWORD cbBuffer;
    DWORD cbNeeded;
    SIZE_T ibNul;
    BYTE* pbData;
    BOOL isMismatch;
    BOOL isExpanded;
    LSTATUS statusStoredType;
    LSTATUS statusSized;
    LSTATUS statusMismatch;
    LSTATUS status;

    if (pvData && !pcbData)
        return ERROR_INVALID_PARAMETER;

    status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__WideArgInit(pAllocContext, lpValue, &value);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only string values are converted; anything else, and every failure to
    // find the value, is what the wide form answers.
    dwFlagsStored =
        (dwFlags & VREG__RRF_WOW_SUBKEYS)
        | RRF_RT_ANY
        | RRF_NOEXPAND;
    dwStoredType = 0;
    statusStoredType = VRegGetValueW(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlagsStored,
        &dwStoredType,
        NULL,
        NULL
    );
    if (statusStoredType != ERROR_SUCCESS || !VReg__IsStringType(dwStoredType))
        goto forward;

    // A value whose type the flags reject is still read and reported, with the
    // failure status.
    dwFlagsTyped = dwFlags & ~RRF_ZEROONFAILURE;
    dwFlagsAnyType =
        (dwFlags & (VREG__RRF_WOW_SUBKEYS | RRF_NOEXPAND))
        | RRF_RT_ANY;
    dwType = 0;
    cbQuery = 0;
    statusSized = VRegGetValueW(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlagsTyped,
        &dwType,
        NULL,
        &cbQuery
    );
    isMismatch = statusSized == ERROR_UNSUPPORTED_TYPE;
    if (statusSized != ERROR_SUCCESS && !isMismatch)
        goto forward;

    dwFlagsRead = isMismatch ? dwFlagsAnyType : dwFlagsTyped;
    if (isMismatch)
    {
        VRegGetValueW(
            pRegistry,
            hkey,
            VReg__WideArgText(&subKey),
            VReg__WideArgText(&value),
            dwFlagsAnyType,
            &dwType,
            NULL,
            &cbQuery
        );
    }

    status = VReg__GetValueOwned(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlagsRead,
        pAllocContext,
        &dwType,
        &data
    );
    if (status != ERROR_SUCCESS)
        goto done;

    isExpanded =
        dwStoredType == REG_EXPAND_SZ
        && !(dwFlags & RRF_NOEXPAND);
    cbStoredRaw = 0;
    if (isExpanded)
    {
        dwReadType = 0;
        cbStoredQuery = 0;
        status = VReg__GetValueOwned(
            pRegistry,
            hkey,
            VReg__WideArgText(&subKey),
            VReg__WideArgText(&value),
            dwFlagsStored,
            pAllocContext,
            &dwReadType,
            &stored
        );
        if (status != ERROR_SUCCESS)
            goto done;

        VRegGetValueW(
            pRegistry,
            hkey,
            VReg__WideArgText(&subKey),
            VReg__WideArgText(&value),
            dwFlagsStored,
            &dwReadType,
            NULL,
            &cbStoredQuery
        );
        cbStoredRaw = cbStoredQuery - VReg__MinDword(cbStoredQuery, 2);
    }

    status = VReg__AnsiFromWide(pAllocContext, (const wchar_t*)data.pb, data.cb / sizeof(wchar_t), &text);
    if (status != ERROR_SUCCESS)
        goto done;

    cbText = (DWORD)text.cb;
    // What a call that cannot return the text reports as its size: room for the
    // terminator the wide form reserves; for an expanded value, the larger of
    // the unexpanded text and the expansion, and a character more when the
    // expansion is the larger.
    cbReported = cbText + (
        cbQuery > data.cb
            ? (DWORD)((cbQuery - data.cb) / sizeof(wchar_t))
            : 0
    );
    if (isExpanded)
    {
        status = VReg__AnsiFromWide(
            pAllocContext,
            (const wchar_t*)stored.pb,
            cbStoredRaw / sizeof(wchar_t),
            &rawText
        );
        if (status != ERROR_SUCCESS)
            goto done;

        cbRawText = (DWORD)rawText.cb;
        ibNul = 0;
        while (ibNul < text.cb && text.pb[ibNul] != 0)
            ++ibNul;
        cbExpanded = ibNul == text.cb ? cbText : (DWORD)(ibNul + 1);
        cbReported = VReg__MaxDword(cbRawText, cbExpanded) + (cbExpanded > cbRawText ? 1 : 0);
    }
    cbBuffer = pcbData ? *pcbData : 0;
    cbNeeded = isExpanded ? cbText + 1 : cbText;
    statusMismatch = isMismatch ? statusSized : ERROR_SUCCESS;

    if (pdwType)
        *pdwType = dwType;

    if (!pvData)
    {
        if (pcbData)
            *pcbData = cbReported;
        status = statusMismatch;
        goto done;
    }

    if (cbBuffer < cbNeeded)
    {
        *pcbData = cbReported;
        if (isExpanded)
        {
            // Native copies the unexpanded text, then clears its first
            // character.
            status = VReg__AnsiFromWide(
                pAllocContext,
                (const wchar_t*)stored.pb,
                stored.cb / sizeof(wchar_t),
                &storedText
            );
            if (status != ERROR_SUCCESS)
                goto done;

            if (storedText.cb > 0 && storedText.cb <= cbBuffer)
            {
                if (cbStoredRaw <= cbBuffer)
                    VReg__CopyBytes(pvData, stored.pb, VReg__MinSize(cbStoredRaw, stored.cb));
                CopyMemory(pvData, storedText.pb, storedText.cb);
                *(char*)pvData = '\0';
                *pcbData = cbNeeded;
            }
        }
        if ((dwFlags & RRF_ZEROONFAILURE) && cbBuffer > 0)
            ZeroMemory(pvData, cbBuffer);
        status = isMismatch ? statusSized : ERROR_MORE_DATA;
        goto done;
    }

    // Native reads the stored wide data into the caller's buffer when it fits
    // there and narrows it in place, so the bytes past the text are the tail of
    // the stored data. The wide form reports two bytes (four for a
    // multi-string) more than are stored.
    pbData = (BYTE*)pvData;
    if (isExpanded)
    {
        if (cbStoredRaw <= cbBuffer)
            VReg__CopyBytes(pbData, stored.pb, VReg__MinSize(cbStoredRaw, stored.cb));
    }
    else
    {
        cbStored = cbQuery - VReg__MinDword(cbQuery, dwType == REG_MULTI_SZ ? 4 : 2);
        if (cbStored <= cbBuffer)
            VReg__CopyBytes(pbData, data.pb, VReg__MinSize(cbStored, data.cb));
    }
    if (cbText > 0)
    {
        CopyMemory(pbData, text.pb, cbText);
        if (cbText < cbBuffer && pbData[cbText - 1] != 0)
            pbData[cbText] = 0;
    }
    *pcbData = cbText;

    if (isMismatch && (dwFlags & RRF_ZEROONFAILURE) && cbBuffer > 0)
        ZeroMemory(pvData, cbBuffer);
    status = statusMismatch;
    goto done;

forward:
    status = VRegGetValueW(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );

done:
    VReg__BufferFree(pAllocContext, &storedText);
    VReg__BufferFree(pAllocContext, &rawText);
    VReg__BufferFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &stored);
    VReg__BufferFree(pAllocContext, &data);
    VReg__WideArgFree(pAllocContext, &value);
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

VREGDEF LSTATUS WINAPI VRegOpenKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegOpenKeyA)
        return pRegistry->lpVtbl->RegOpenKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegOpenKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );}

VREGDEF LSTATUS WINAPI VRegOpenKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegOpenKeyW)
        return pRegistry->lpVtbl->RegOpenKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegOpenKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );}

VREGDEF LSTATUS WINAPI VRegCreateKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegCreateKeyA)
        return pRegistry->lpVtbl->RegCreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VRegIsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegCreateKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        MAXIMUM_ALLOWED,
        NULL,
        phkResult,
        NULL
    );}

VREGDEF LSTATUS WINAPI VRegCreateKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegCreateKeyW)
        return pRegistry->lpVtbl->RegCreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VRegIsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegCreateKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        MAXIMUM_ALLOWED,
        NULL,
        phkResult,
        NULL
    );}

VREGDEF LSTATUS WINAPI VRegQueryValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (pRegistry->lpVtbl->RegQueryValueA)
        return pRegistry->lpVtbl->RegQueryValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );

    HKEY hSubKey = hKey;
    DWORD dwType = 0;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegQueryValueExA(
        pRegistry,
        hSubKey,
        NULL,
        NULL,
        &dwType,
        (LPBYTE)lpData,
        (LPDWORD)lpcbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    if (status == ERROR_SUCCESS && dwType != REG_SZ)
        status = ERROR_INVALID_DATA;

    if (status == ERROR_FILE_NOT_FOUND)
    {
        if (lpData)
            *lpData = '\0';
        if (lpcbData)
            *lpcbData = sizeof(char);
        status = ERROR_SUCCESS;
    }
    return status;}

VREGDEF LSTATUS WINAPI VRegQueryValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (pRegistry->lpVtbl->RegQueryValueW)
        return pRegistry->lpVtbl->RegQueryValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );

    HKEY hSubKey = hKey;
    DWORD dwType = 0;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegQueryValueExW(
        pRegistry,
        hSubKey,
        NULL,
        NULL,
        &dwType,
        (LPBYTE)lpData,
        (LPDWORD)lpcbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    if (status == ERROR_SUCCESS && dwType != REG_SZ)
        status = ERROR_INVALID_DATA;

    if (status == ERROR_FILE_NOT_FOUND)
    {
        if (lpData)
            *lpData = L'\0';
        if (lpcbData)
            *lpcbData = sizeof(wchar_t);
        status = ERROR_SUCCESS;
    }
    return status;}

VREGDEF LSTATUS WINAPI VRegSetValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetValueA)
        return pRegistry->lpVtbl->RegSetValueA(
            pRegistry,
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;

    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExA(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)(VReg__AnsiLength(lpData) + 1)
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

VREGDEF LSTATUS WINAPI VRegSetValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetValueW)
        return pRegistry->lpVtbl->RegSetValueW(
            pRegistry,
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;

    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExW(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)((VReg__ViewOf(lpData).cch + 1) * sizeof(wchar_t))
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

VREGDEF LSTATUS WINAPI VRegDeleteKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyA)
        return pRegistry->lpVtbl->RegDeleteKeyA(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VRegDeleteKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );}

VREGDEF LSTATUS WINAPI VRegDeleteKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyW)
        return pRegistry->lpVtbl->RegDeleteKeyW(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VRegDeleteKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );}

VREGDEF LSTATUS WINAPI VRegEnumKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    if (pRegistry->lpVtbl->RegEnumKeyA)
        return pRegistry->lpVtbl->RegEnumKeyA(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VRegEnumKeyExA(
        pRegistry,
        hKey,
        dwIndex,
        lpName,
        &cchNameInOut,
        NULL,
        NULL,
        NULL,
        NULL
    );}

VREGDEF LSTATUS WINAPI VRegEnumKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    if (pRegistry->lpVtbl->RegEnumKeyW)
        return pRegistry->lpVtbl->RegEnumKeyW(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VRegEnumKeyExW(
        pRegistry,
        hKey,
        dwIndex,
        lpName,
        &cchNameInOut,
        NULL,
        NULL,
        NULL,
        NULL
    );}

// The subkey of SetKeyValue and DeleteKeyValue is opened with KEY_SET_VALUE,
// the one right the call needs; the maximum allowed access, as the pre-Ex
// forms use, was passed over. Only a caller whose access is limited to
// something else could tell.
VREGDEF LSTATUS WINAPI VRegSetKeyValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetKeyValueA)
        return pRegistry->lpVtbl->RegSetKeyValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            NULL,
            0,
            KEY_SET_VALUE,
            NULL,
            &hSubKey,
            NULL
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExA(
        pRegistry,
        hSubKey,
        lpValueName,
        0,
        dwType,
        (const BYTE*)lpData,
        cbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

VREGDEF LSTATUS WINAPI VRegSetKeyValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetKeyValueW)
        return pRegistry->lpVtbl->RegSetKeyValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            NULL,
            0,
            KEY_SET_VALUE,
            NULL,
            &hSubKey,
            NULL
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExW(
        pRegistry,
        hSubKey,
        lpValueName,
        0,
        dwType,
        (const BYTE*)lpData,
        cbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

VREGDEF LSTATUS WINAPI VRegDeleteKeyValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyValueA)
        return pRegistry->lpVtbl->RegDeleteKeyValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegDeleteValueA(
        pRegistry,
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

VREGDEF LSTATUS WINAPI VRegDeleteKeyValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyValueW)
        return pRegistry->lpVtbl->RegDeleteKeyValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegDeleteValueW(
        pRegistry,
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

/// Places the narrowed values like the wide form places the stored ones, but
/// leaves the gaps alone and rounds the total up. When they do not all fit,
/// native answers the wide form's total for the stored values.
// The SDK types lpValueBuf as a string, but it holds binary values.
#pragma warning(suppress: 6054)
VREGDEF LSTATUS WINAPI VRegQueryMultipleValuesA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (pRegistry->lpVtbl->RegQueryMultipleValuesA)
        return pRegistry->lpVtbl->RegQueryMultipleValuesA(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__PlacedValue* pPlaced = NULL;
    VReg__WideArg name = {FALSE, {NULL, 0, 0}};
    BYTE* pbBuffer;
    DWORD cbBuffer;
    DWORD cbEnd = 0;
    DWORD cbWideEnd;
    DWORD cbWide;
    DWORD cbRead;
    DWORD i;
    LSTATUS status;

    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    cbBuffer = *ldwTotsize;
    pbBuffer = (BYTE*)lpValueBuf;
    if (num_vals > 0)
    {
        pPlaced = (VReg__PlacedValue*)VReg__AllocZeroed(pAllocContext, (SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VRegQueryValueExA(
            pRegistry,
            hKey,
            val_list[i].ve_valuename,
            NULL,
            NULL,
            NULL,
            &pPlaced[i].cbValue
        );
        if (status != ERROR_SUCCESS)
            goto done;

        pPlaced[i].ibValue = i == 0 ? 0 : VReg__AlignedToDword(cbEnd);
        cbEnd = pPlaced[i].ibValue + pPlaced[i].cbValue;
    }

    if (cbEnd > cbBuffer)
    {
        cbWideEnd = 0;
        for (i = 0; i < num_vals; ++i)
        {
            cbWide = 0;
            status = VReg__WideArgInit(pAllocContext, val_list[i].ve_valuename, &name);
            if (status == ERROR_SUCCESS)
            {
                status = VRegQueryValueExW(
                    pRegistry,
                    hKey,
                    VReg__WideArgText(&name),
                    NULL,
                    NULL,
                    NULL,
                    &cbWide
                );
            }
            VReg__WideArgFree(pAllocContext, &name);
            if (status != ERROR_SUCCESS)
                goto done;

            cbWideEnd = (i == 0 ? 0 : VReg__AlignedToDword(cbWideEnd)) + cbWide;
        }
        *ldwTotsize = cbWideEnd;
        status = ERROR_MORE_DATA;
        goto done;
    }

    for (i = 0; i < num_vals; ++i)
    {
        cbRead = pPlaced[i].cbValue;
        status = VRegQueryValueExA(
            pRegistry,
            hKey,
            val_list[i].ve_valuename,
            NULL,
            &pPlaced[i].dwType,
            pbBuffer + pPlaced[i].ibValue,
            &cbRead
        );
        if (status != ERROR_SUCCESS)
            goto done;

        pPlaced[i].cbValue = cbRead;
    }

    for (i = 0; i < num_vals; ++i)
    {
        val_list[i].ve_valuelen = pPlaced[i].cbValue;
        val_list[i].ve_type = pPlaced[i].dwType;
        val_list[i].ve_valueptr = (DWORD_PTR)(pbBuffer + pPlaced[i].ibValue);
    }
    *ldwTotsize = VReg__AlignedToDword(cbEnd);
    status = ERROR_SUCCESS;

done:
    VReg__Free(pAllocContext, pPlaced);
    return status;}

/// Native places each value on a @ref DWORD boundary, reads the ones that fit
/// the buffer in order, and fills the entries only when all of them did.
VREGDEF LSTATUS WINAPI VRegQueryMultipleValuesW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (pRegistry->lpVtbl->RegQueryMultipleValuesW)
        return pRegistry->lpVtbl->RegQueryMultipleValuesW(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__PlacedValue* pPlaced = NULL;
    BYTE* pbBuffer;
    DWORD cbBuffer;
    DWORD cbEnd = 0;
    DWORD cbValue;
    DWORD cbRead;
    DWORD ibValue;
    DWORD i;
    BOOL isComplete = TRUE;
    LSTATUS status;

    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;

    cbBuffer = *ldwTotsize;
    *ldwTotsize = 0;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    pbBuffer = (BYTE*)lpValueBuf;
    if (num_vals > 0)
    {
        pPlaced = (VReg__PlacedValue*)VReg__AllocZeroed(pAllocContext, (SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            val_list[i].ve_valuename,
            NULL,
            NULL,
            NULL,
            &pPlaced[i].cbValue
        );
        if (status != ERROR_SUCCESS)
        {
            if (status == ERROR_INVALID_HANDLE)
                *ldwTotsize = cbBuffer;
            goto done;
        }
    }

    for (i = 0; i < num_vals; ++i)
    {
        cbValue = pPlaced[i].cbValue;
        ibValue = i == 0 ? 0 : VReg__AlignedToDword(cbEnd);
        if (ibValue > cbEnd && cbEnd < cbBuffer)
            ZeroMemory(pbBuffer + cbEnd, VReg__MinDword(ibValue, cbBuffer) - cbEnd);

        pPlaced[i].ibValue = ibValue;
        if (ibValue + cbValue <= cbBuffer)
        {
            cbRead = cbValue;
            status = VRegQueryValueExW(
                pRegistry,
                hKey,
                val_list[i].ve_valuename,
                NULL,
                &pPlaced[i].dwType,
                pbBuffer + ibValue,
                &cbRead
            );
            if (status != ERROR_SUCCESS)
                goto done;

            pPlaced[i].cbValue = cbRead;
        }
        else
        {
            isComplete = FALSE;
        }
        cbEnd = ibValue + cbValue;
    }

    *ldwTotsize = cbEnd;
    if (!isComplete)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    for (i = 0; i < num_vals; ++i)
    {
        val_list[i].ve_valuelen = pPlaced[i].cbValue;
        val_list[i].ve_type = pPlaced[i].dwType;
        val_list[i].ve_valueptr = (DWORD_PTR)(pbBuffer + pPlaced[i].ibValue);
    }
    status = ERROR_SUCCESS;

done:
    VReg__Free(pAllocContext, pPlaced);
    return status;
}

// ==========================================================================
// Handles
// ==========================================================================

VREGDEF BOOL WINAPI VRegIsVirtualHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value != 0 && value <= VREG_VIRTUAL_HKEY_BASE && (value & 1) != 0;
}

VREGDEF BOOL WINAPI VRegIsPredefinedHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value >= (UINT_PTR)HKEY_CLASSES_ROOT && value <= (UINT_PTR)HKEY_CURRENT_USER_LOCAL_SETTINGS;
}

// ==========================================================================
// The handle space
// ==========================================================================

typedef struct VReg__HandleBlock
{
    void* volatile targets[VREG__HANDLE_BLOCK_SIZE];
} VReg__HandleBlock;

struct VReg__HandleDirectory
{
    VReg__HandleBlock* volatile blocks[VREG__HANDLE_DIRECTORY_SIZE];
};

static VReg__HandleSpace g_processSpace;
static INIT_ONCE g_processSpaceOnce = INIT_ONCE_STATIC_INIT;

// Agent's choice: the block tables come from the process heap directly, not
// from the allocation macros. They live for the process and belong to no hive
// or registry, so no caller's allocation context could own them.
static void* VReg__AllocateZeroed(const SIZE_T cb)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}

VREGDEF void VReg__HandleSpaceInit(_Out_ VReg__HandleSpace* const pSpace, _In_z_ const LPCWSTR pszName)
{
    HANDLE hSection;
    VReg__HandleSpaceShared* pShared;
    LONG previous;

    ZeroMemory(pSpace, sizeof(*pSpace));
    InitializeSRWLock(&pSpace->lock);

    // Pagefile-backed: the pages start zero-filled, which is the unclaimed
    // state. Open-or-create is atomic, so racing copies get the same section.
    hSection = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(VReg__HandleSpaceShared),
        pszName
    );
    if (!hSection)
        return;

    pShared = (VReg__HandleSpaceShared*)MapViewOfFile(
        hSection,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(VReg__HandleSpaceShared)
    );
    if (!pShared)
    {
        CloseHandle(hSection);
        return;
    }

    // The handle stays open on purpose, and so does the view: other copies'
    // handles were reserved through it, and a section that vanished and came
    // back zeroed would hand them out again.

    // Whoever moves magic from zero has claimed it; the layout version is part
    // of the magic value, so there is nothing else to publish.
    previous = InterlockedCompareExchange(&pShared->magic, VREG__HANDLE_SPACE_MAGIC, 0);
    if (previous != 0 && previous != VREG__HANDLE_SPACE_MAGIC)
        return;

    pSpace->pShared = pShared;
}

/// Writes @p value in decimal at @p pszOut, which has room for @p cchOut
/// characters including the terminator, and answers the characters written.
static SIZE_T VReg__WriteDecimal(_Out_writes_z_(cchOut) wchar_t* const pszOut, const SIZE_T cchOut, DWORD value)
{
    wchar_t wszDigits[10];
    SIZE_T cchDigits = 0;
    SIZE_T i;

    do
    {
        wszDigits[cchDigits++] = (wchar_t)(L'0' + value % 10);
        value /= 10;
    }
    while (value != 0 && cchDigits < 10);

    if (cchDigits >= cchOut)
        cchDigits = cchOut - 1;

    for (i = 0; i < cchDigits; ++i)
        pszOut[i] = wszDigits[cchDigits - 1 - i];

    pszOut[cchDigits] = L'\0';
    return cchDigits;
}

static BOOL CALLBACK VReg__InitProcessHandleSpace(
    _Inout_ PINIT_ONCE const pInitOnce,
    _Inout_opt_ const PVOID pParameter,
    _Outptr_opt_result_maybenull_ PVOID* const ppContext
)
{
    // Every copy of VReg in the process, however built, finds the others by
    // this name, so it never changes: a copy using another name would hand
    // out handles that overlap theirs.
    static const wchar_t wszPrefix[] = L"Local\\VReg-handles-";
    wchar_t wszName[64];
    SIZE_T cchPrefix = sizeof(wszPrefix) / sizeof(wszPrefix[0]) - 1;

    (void)pInitOnce;
    (void)pParameter;
    if (ppContext)
        *ppContext = NULL;

    CopyMemory(wszName, wszPrefix, cchPrefix * sizeof(wchar_t));
    VReg__WriteDecimal(wszName + cchPrefix, sizeof(wszName) / sizeof(wszName[0]) - cchPrefix, GetCurrentProcessId());
    VReg__HandleSpaceInit(&g_processSpace, wszName);
    return TRUE;
}

VREGDEF _Ret_notnull_ VReg__HandleSpace* VReg__ProcessHandleSpace(void)
{
    // Handles issued from it outlive every teardown, so the instance is never
    // destroyed.
    InitOnceExecuteOnce(&g_processSpaceOnce, VReg__InitProcessHandleSpace, NULL, NULL);
    return &g_processSpace;
}

VREGDEF BOOL VReg__HandleSpaceIsUsable(_In_ const VReg__HandleSpace* const pSpace)
{
    return pSpace->pShared != NULL;
}

// A slot holds null while unbound, which WritePointerRelease's annotations
// reject; the exchange accepts it, and its full barrier covers the release.
static void VReg__StoreTarget(_Inout_ void* volatile* const pSlot, _In_opt_ void* const pTarget)
{
    InterlockedExchangePointer(pSlot, pTarget);
}

static VReg__HandleBlock* VReg__FindBlock(_In_ const VReg__HandleSpace* const pSpace, const UINT_PTR block)
{
    const VReg__HandleDirectory* const pDirectory = (const VReg__HandleDirectory*)ReadPointerAcquire(
        (PVOID volatile*)&pSpace->directories[block / VREG__HANDLE_DIRECTORY_SIZE]
    );
    if (!pDirectory)
        return NULL;

    return (VReg__HandleBlock*)ReadPointerAcquire(
        (PVOID volatile*)&pDirectory->blocks[block % VREG__HANDLE_DIRECTORY_SIZE]
    );
}

static BOOL VReg__AddBlock(_Inout_ VReg__HandleSpace* const pSpace, const UINT_PTR block)
{
    VReg__HandleDirectory* volatile* const pDirectorySlot = &pSpace->directories[block / VREG__HANDLE_DIRECTORY_SIZE];
    VReg__HandleDirectory* pDirectory = *pDirectorySlot;
    VReg__HandleBlock* pBlock;

    if (!pDirectory)
    {
        pDirectory = (VReg__HandleDirectory*)VReg__AllocateZeroed(sizeof(VReg__HandleDirectory));
        if (!pDirectory)
            return FALSE;

        WritePointerRelease((PVOID volatile*)pDirectorySlot, pDirectory);
    }

    pBlock = (VReg__HandleBlock*)VReg__AllocateZeroed(sizeof(VReg__HandleBlock));
    if (!pBlock)
        return FALSE;

    WritePointerRelease((PVOID volatile*)&pDirectory->blocks[block % VREG__HANDLE_DIRECTORY_SIZE], pBlock);
    return TRUE;
}

VREGDEF _Ret_maybenull_ HKEY VReg__HandleSpaceAllocate(_Inout_ VReg__HandleSpace* const pSpace, _In_opt_ void* const pTarget)
{
    HKEY hKey = NULL;
    UINT_PTR index;
    VReg__HandleBlock* pBlock;

    if (!pSpace->pShared)
        return NULL;

    AcquireSRWLockExclusive(&pSpace->lock);

    if (pSpace->nextIndex == pSpace->endIndex)
    {
        // InterlockedIncrement returns the new value; the block reserved is the
        // one before it.
        const UINT_PTR block = (UINT_PTR)InterlockedIncrement(&pSpace->pShared->nextBlock) - 1;
        if (block >= VREG__HANDLE_BLOCK_COUNT || !VReg__AddBlock(pSpace, block))
        {
            ReleaseSRWLockExclusive(&pSpace->lock);
            return NULL;
        }

        pSpace->nextIndex = block * VREG__HANDLE_BLOCK_SIZE;
        pSpace->endIndex = pSpace->nextIndex + VREG__HANDLE_BLOCK_SIZE;
    }

    index = pSpace->nextIndex++;
    pBlock = VReg__FindBlock(pSpace, index / VREG__HANDLE_BLOCK_SIZE);
    VReg__StoreTarget(&pBlock->targets[index % VREG__HANDLE_BLOCK_SIZE], pTarget);
    hKey = VReg__VirtualHKeyFromIndex(index);

    ReleaseSRWLockExclusive(&pSpace->lock);
    return hKey;
}

VREGDEF BOOL VReg__HandleSpaceOwns(_In_ const VReg__HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    return
        VRegIsVirtualHKey(hKey)
        && VReg__FindBlock(pSpace, VReg__IndexFromVirtualHKey(hKey) / VREG__HANDLE_BLOCK_SIZE) != NULL;
}

VREGDEF void VReg__HandleSpaceRelease(_Inout_ VReg__HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    UINT_PTR index;
    VReg__HandleBlock* pBlock;

    if (!VRegIsVirtualHKey(hKey))
        return;

    index = VReg__IndexFromVirtualHKey(hKey);
    pBlock = VReg__FindBlock(pSpace, index / VREG__HANDLE_BLOCK_SIZE);
    if (pBlock)
        VReg__StoreTarget(&pBlock->targets[index % VREG__HANDLE_BLOCK_SIZE], NULL);
}

VREGDEF _Ret_maybenull_ void* VReg__HandleSpaceFind(_In_ const VReg__HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    UINT_PTR index;
    const VReg__HandleBlock* pBlock;

    if (!VRegIsVirtualHKey(hKey))
        return NULL;

    index = VReg__IndexFromVirtualHKey(hKey);
    pBlock = VReg__FindBlock(pSpace, index / VREG__HANDLE_BLOCK_SIZE);
    if (!pBlock)
        return NULL;

    return ReadPointerAcquire((PVOID volatile*)&pBlock->targets[index % VREG__HANDLE_BLOCK_SIZE]);
}

VREGDEF _Ret_maybenull_ HKEY VReg__VirtualHKeyFromIndex(const UINT_PTR index)
{
    if (index >= VREG__VIRTUAL_HKEY_COUNT)
        return NULL;

    return (HKEY)(VREG_VIRTUAL_HKEY_BASE - (index << 1));
}

VREGDEF UINT_PTR VReg__IndexFromVirtualHKey(_In_opt_ const HKEY hKey)
{
    return (VREG_VIRTUAL_HKEY_BASE - (UINT_PTR)hKey) >> 1;
}

// ==========================================================================
// Hives
// ==========================================================================

struct VRegHive
{
    void* pAllocContext;

    /// Null for a hive held entirely in memory.
    VRegRegistry* pLower;
    HKEY hLowerRoot;
    VReg__WString lowerPath;
    REGSAM samLowerView;

    SRWLOCK lock;
    volatile LONG isFrozen;

    /// Fixed for the life of the hive.
    VReg__Node* pRoot;

    /// `VReg__Node*`: nodes are created here and freed only with the hive; a
    /// deleted node stays allocated (marked deleted) so its handle keeps
    /// answering @ref ERROR_KEY_DELETED. Destroying the hive unbinds every
    /// handle it issued.
    VReg__Array nodes;
};

#define VREG__NODE_AT(pArray, index) VREG__AT(pArray, VReg__Node*, index)
#define VREG__VALUE_AT(pArray, index) VREG__AT(pArray, VReg__Value, index)
#define VREG__WATCH_AT(pArray, index) VREG__AT(pArray, VReg__ChangeWatch, index)
#define VREG__NAME_AT(pArray, index) VREG__AT(pArray, VReg__WString, index)

static const VReg__WStringView VREG__NO_NAME = {NULL, 0};

VREGDEF int VReg__CompareNames(const VReg__WStringView left, const VReg__WStringView right)
{
    if (left.cch == 0 || right.cch == 0)
        return (left.cch == 0) == (right.cch == 0) ? 0 : (left.cch == 0 ? -1 : 1);

    return CompareStringOrdinal(left.pch, (int)left.cch, right.pch, (int)right.cch, TRUE) - CSTR_EQUAL;
}

/// Whether @p status says the thing asked about exists: found, or found but
/// not readable this way.
static BOOL VReg__StatusExists(const LSTATUS status)
{
    return status == ERROR_SUCCESS || status == ERROR_MORE_DATA || status == ERROR_ACCESS_DENIED;
}

// --- Locking ---

static BOOL VReg__HiveIsFrozen(_In_ VRegHive* const pHive)
{
    return ReadAcquire(&pHive->isFrozen) != 0;
}

/// Takes the lock shared, or nothing once frozen; answers whether it did.
static BOOL VReg__HiveLockRead(_In_ VRegHive* const pHive)
{
    if (VReg__HiveIsFrozen(pHive))
        return FALSE;

    AcquireSRWLockShared(&pHive->lock);
    return TRUE;
}

static void VReg__HiveUnlockRead(_In_ VRegHive* const pHive, const BOOL isLocked)
{
    if (isLocked)
        ReleaseSRWLockShared(&pHive->lock);
}

/// Takes the lock exclusive, or nothing once frozen; answers whether it did.
/// A check of @ref VReg__HiveIsFrozen made before can be stale, since a
/// writer may wait here while @ref VRegFreezeHive runs, so only this decides a
/// write.
static BOOL VReg__HiveLockWrite(_In_ VRegHive* const pHive)
{
    AcquireSRWLockExclusive(&pHive->lock);
    if (!VReg__HiveIsFrozen(pHive))
        return TRUE;

    ReleaseSRWLockExclusive(&pHive->lock);
    return FALSE;
}

static void VReg__HiveUnlockWrite(_In_ VRegHive* const pHive)
{
    ReleaseSRWLockExclusive(&pHive->lock);
}

// --- Nodes, values and watches ---

static void VReg__ValueFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Value* const pValue)
{
    VReg__WStringFree(pAllocContext, &pValue->name);
    VReg__BufferFree(pAllocContext, &pValue->data);
}

static void VReg__NodeClearValues(_In_opt_ void* const pAllocContext, _Inout_ VReg__Node* const pNode)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
        VReg__ValueFree(pAllocContext, &VREG__VALUE_AT(&pNode->values, i));

    pNode->values.count = 0;
}

static void VReg__NodeFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Node* const pNode)
{
    VReg__NodeClearValues(pAllocContext, pNode);
    VReg__ArrayFree(pAllocContext, &pNode->values);
    VReg__ArrayFree(pAllocContext, &pNode->children);
    VReg__ArrayFree(pAllocContext, &pNode->watches);
    VReg__WStringFree(pAllocContext, &pNode->name);
    VReg__WStringFree(pAllocContext, &pNode->className);
    VReg__Free(pAllocContext, pNode);
}

/// Stamps @p pNode as written now.
static void VReg__HiveTouch(_Inout_ VReg__Node* const pNode)
{
    GetSystemTimeAsFileTime(&pNode->lastWrite);
}

/// Makes @p pNode and its transparent ancestors upper keys, as a write to a
/// key below them needs. The root stays as it is: it is the mount point.
static void VReg__HiveMaterialize(_Inout_ VReg__Node* const pNode)
{
    VReg__Node* pCurrent;
    for (pCurrent = pNode; pCurrent->pParent && pCurrent->isTransparent; pCurrent = pCurrent->pParent)
        pCurrent->isTransparent = FALSE;
}

static void VReg__HiveDropWatches(_Inout_ VReg__Node* const pNode)
{
    SIZE_T i;
    for (i = 0; i < pNode->watches.count; ++i)
    {
        // The array's count bounds its initialized elements, which the
        // analyzer cannot see past the reallocation.
#pragma warning(suppress: 6001)
        CloseHandle(VREG__WATCH_AT(&pNode->watches, i).hEvent);
    }

    pNode->watches.count = 0;
}

/// Signals and drops the watches of @p pNode that @p dwFilter selects;
/// @p isInherited limits them to those watching their subtree.
static void VReg__HiveFire(_Inout_ VReg__Node* const pNode, const DWORD dwFilter, const BOOL isInherited)
{
    SIZE_T i = 0;
    while (i < pNode->watches.count)
    {
        const VReg__ChangeWatch* const pWatch = &VREG__WATCH_AT(&pNode->watches, i);
        HANDLE hEvent;
        if (!(pWatch->dwFilter & dwFilter) || (isInherited && !pWatch->isSubtree))
        {
            ++i;
            continue;
        }

        // The array's count bounds its initialized elements, which the
        // analyzer cannot see past the reallocation.
#pragma warning(suppress: 6001)
        hEvent = pWatch->hEvent;
        VReg__ArrayRemoveAt(&pNode->watches, sizeof(*pWatch), i);
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}

/// Fires and drops the watches a change of kind @p dwFilter (a
/// `REG_NOTIFY_CHANGE_*` bit) on @p pNode reaches: its own, and those of its
/// ancestors that watch their subtree. Neither @ref SetEvent nor
/// @ref CloseHandle can wait on the loader lock or on anything a caller of
/// the hive could hold, so they run under the write lock, which the caller
/// must hold; deferring them would need an allocation after the change is
/// made.
static void VReg__HiveNotify(_Inout_ VReg__Node* const pNode, const DWORD dwFilter)
{
    VReg__Node* pAncestor;
    VReg__HiveFire(pNode, dwFilter, FALSE);
    for (pAncestor = pNode->pParent; pAncestor; pAncestor = pAncestor->pParent)
        VReg__HiveFire(pAncestor, dwFilter, TRUE);
}

/// The child of @p pParent named @p name, or null; @p pIndex, if given,
/// receives where it is or where it would be inserted.
static _Ret_maybenull_ VReg__Node* VReg__HiveFindChild(
    _In_ const VReg__Node* const pParent,
    const VReg__WStringView name,
    _Out_opt_ SIZE_T* const pIndex
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pParent->children.count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        if (VReg__CompareNames(VReg__WStringViewOf(&VREG__NODE_AT(&pParent->children, mid)->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (pIndex)
        *pIndex = lo;
    if (lo < pParent->children.count)
    {
        VReg__Node* const pChild = VREG__NODE_AT(&pParent->children, lo);
        if (VReg__CompareNames(VReg__WStringViewOf(&pChild->name), name) == 0)
            return pChild;
    }

    return NULL;
}

static _Ret_maybenull_ VReg__Value* VReg__HiveFindValue(_In_ VReg__Node* const pNode, const VReg__WStringView name)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
    {
        VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        if (VReg__CompareNames(VReg__WStringViewOf(&pValue->name), name) == 0)
            return pValue;
    }

    return NULL;
}

/// A new child of @p pParent, with its handle. The write lock must be held.
static LSTATUS VReg__HiveAddChild(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pParent,
    const VReg__WStringView name,
    const VReg__WStringView className,
    const BOOL isVolatile,
    _Outptr_ VReg__Node** const ppChild
)
{
    void* const pAllocContext = pHive->pAllocContext;
    SIZE_T index = 0;
    VReg__Node* const pNode = (VReg__Node*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__Node));
    if (!pNode)
        return ERROR_OUTOFMEMORY;

    pNode->pHive = pHive;
    pNode->pParent = pParent;
    pNode->isVolatile = isVolatile;
    VReg__HiveTouch(pNode);
    VReg__HiveFindChild(pParent, name, &index);

    // Every allocation happens before the handle is bound, so a failure
    // leaves no handle pointing at a freed node.
    if (
        !VReg__WStringAssign(pAllocContext, &pNode->name, name.pch, name.cch)
        || !VReg__WStringAssign(pAllocContext, &pNode->className, className.pch, className.cch)
        || !VReg__ArrayReserve(pAllocContext, &pHive->nodes, sizeof(pNode), pHive->nodes.count + 1)
        || !VReg__ArrayReserve(pAllocContext, &pParent->children, sizeof(pNode), pParent->children.count + 1)
    )
    {
        VReg__NodeFree(pAllocContext, pNode);
        return ERROR_OUTOFMEMORY;
    }

    pNode->hKey = VReg__HandleSpaceAllocate(VReg__ProcessHandleSpace(), pNode);
    if (!pNode->hKey)
    {
        VReg__NodeFree(pAllocContext, pNode);
        return ERROR_OUTOFMEMORY;
    }

    VReg__ArrayInsert(pAllocContext, &pParent->children, sizeof(pNode), index, &pNode);
    VReg__ArrayAppend(pAllocContext, &pHive->nodes, sizeof(pNode), &pNode);
    VReg__HiveTouch(pParent);
    *ppChild = pNode;
    return ERROR_SUCCESS;
}

/// Children of a deleted key answer as deleted too.
static void VReg__HiveMarkDeleted(_Inout_ VReg__Node* const pNode)
{
    SIZE_T i;
    pNode->isDeleted = TRUE;
    VReg__HiveFire(pNode, ~(DWORD)0, FALSE);
    for (i = 0; i < pNode->children.count; ++i)
        VReg__HiveMarkDeleted(VREG__NODE_AT(&pNode->children, i));
}

/// Marks @p pNode and everything below it deleted, unlinks it from its
/// parent and fires the watches on it and below it, and the name watches
/// on its parent. The write lock must be held.
static void VReg__HiveRemove(_Inout_ VReg__Node* const pNode)
{
    if (pNode->pParent)
    {
        SIZE_T index = 0;
        VReg__HiveFindChild(pNode->pParent, VReg__WStringViewOf(&pNode->name), &index);
        VReg__ArrayRemoveAt(&pNode->pParent->children, sizeof(pNode), index);
        VReg__HiveTouch(pNode->pParent);
    }

    VReg__HiveMarkDeleted(pNode);
    if (pNode->pParent)
        VReg__HiveNotify(pNode->pParent, REG_NOTIFY_CHANGE_NAME);
}

// --- The lower registry ---

/// Appends @p cNames path components to @p pPath, separated by backslashes.
static BOOL VReg__HiveJoinPath(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pPath,
    _In_reads_(cNames) const VReg__WStringView* const pNames,
    const SIZE_T cNames
)
{
    SIZE_T i;
    for (i = 0; i < cNames; ++i)
    {
        if (!VReg__AppendPathComponent(pAllocContext, pPath, pNames[i].pch, pNames[i].cch))
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg__HiveAppendNodePath(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pPath,
    _In_ const VReg__Node* const pNode
)
{
    if (!pNode->pParent)
        return TRUE;
    if (!VReg__HiveAppendNodePath(pAllocContext, pPath, pNode->pParent))
        return FALSE;

    return VReg__AppendPathComponent(pAllocContext, pPath, VReg__WStringText(&pNode->name), pNode->name.cch);
}

/// Whether @p pNode's lower key shows through, and its path (read under the
/// lock, used after it is released). Never visible without a lower registry.
static LSTATUS VReg__HiveViewOf(
    _In_ VRegHive* const pHive,
    _In_ const VReg__Node* const pNode,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pPath
)
{
    const VReg__Node* pCurrent;
    *pIsVisible = FALSE;
    if (!pHive->pLower)
        return ERROR_SUCCESS;

    for (pCurrent = pNode; pCurrent; pCurrent = pCurrent->pParent)
    {
        if (pCurrent->isOpaque)
            return ERROR_SUCCESS;
    }

    if (
        !VReg__WStringAssign(pHive->pAllocContext, pPath, VReg__WStringText(&pHive->lowerPath), pHive->lowerPath.cch)
        || !VReg__HiveAppendNodePath(pHive->pAllocContext, pPath, pNode)
    )
        return ERROR_OUTOFMEMORY;

    *pIsVisible = TRUE;
    return ERROR_SUCCESS;
}

static LSTATUS VReg__HiveOpenLower(
    _In_ VRegHive* const pHive,
    _In_ const VReg__WString* const pPath,
    const REGSAM samDesired,
    _Out_ PHKEY const phKey
)
{
    return VRegOpenKeyExW(
        pHive->pLower,
        pHive->hLowerRoot,
        pPath->cch == 0 ? NULL : pPath->psz,
        0,
        samDesired | pHive->samLowerView,
        phKey
    );
}

static void VReg__HiveCloseLower(_In_ VRegHive* const pHive, _In_opt_ const HKEY hKey)
{
    if (hKey)
        VRegCloseKey(
            pHive->pLower,
            hKey
        );
}

/// The class of the lower key at @p pPath; empty when there is none.
static LSTATUS VReg__HiveQueryLowerClass(
    _In_ VRegHive* const pHive,
    _In_ const VReg__WString* const pPath,
    _Inout_ VReg__WString* const pClassName
)
{
    HKEY hLower = NULL;
    LSTATUS status;

    if (!VReg__WStringResize(pHive->pAllocContext, pClassName, 0))
        return ERROR_OUTOFMEMORY;

    status = VReg__HiveOpenLower(pHive, pPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
        return status == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : status;

    status = VReg__QueryInfoKeyOwned(
        pHive->pLower,
        hLower,
        pHive->pAllocContext,
        pClassName,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );
    VReg__HiveCloseLower(pHive, hLower);
    return status;
}

// --- Creation and destruction ---

static LSTATUS VReg__HiveCreate(
    _In_opt_ VRegRegistry* const pLower,
    _In_opt_ const HKEY hLowerRoot,
    const VReg__WStringView lowerPath,
    const REGSAM samLowerView,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegHive** const ppHive
)
{
    VReg__Node* pRoot;
    VRegHive* const pHive = (VRegHive*)VReg__AllocZeroed(pAllocContext, sizeof(VRegHive));
    *ppHive = NULL;
    if (!pHive)
        return ERROR_OUTOFMEMORY;

    pHive->pAllocContext = pAllocContext;
    pHive->pLower = pLower;
    pHive->hLowerRoot = hLowerRoot;
    pHive->samLowerView = samLowerView;
    InitializeSRWLock(&pHive->lock);

    pRoot = (VReg__Node*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__Node));
    if (
        !VReg__WStringAssign(pAllocContext, &pHive->lowerPath, lowerPath.pch, lowerPath.cch)
        || !pRoot
        || !VReg__ArrayAppend(pAllocContext, &pHive->nodes, sizeof(pRoot), &pRoot)
    )
    {
        VReg__Free(pAllocContext, pRoot);
        VRegDestroyHive(pHive);
        return ERROR_OUTOFMEMORY;
    }

    pRoot->pHive = pHive;
    pRoot->hKey = VReg__HandleSpaceAllocate(VReg__ProcessHandleSpace(), pRoot);
    if (!pRoot->hKey)
    {
        VRegDestroyHive(pHive);
        return ERROR_OUTOFMEMORY;
    }

    VReg__HiveTouch(pRoot);
    if (pLower)
    {
        pRoot->isTransparent = TRUE;
        pRoot->isLowerBacked = TRUE;
    }

    pHive->pRoot = pRoot;
    *ppHive = pHive;
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS WINAPI VRegCreateHive(_In_opt_ void* const pAllocContext, _Outptr_ VRegHive** const ppHive)
{
    return VReg__HiveCreate(NULL, NULL, VREG__NO_NAME, 0, pAllocContext, ppHive);
}

VREGDEF LSTATUS WINAPI VRegCreateLayeredHive(
    _In_ VRegRegistry* const pLower,
    _In_ const HKEY hLowerRoot,
    _In_opt_z_ const LPCWSTR pszLowerPath,
    _In_ const REGSAM samLowerView,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegHive** const ppHive
)
{
    return VReg__HiveCreate(pLower, hLowerRoot, VReg__ViewOf(pszLowerPath), samLowerView, pAllocContext, ppHive);
}

VREGDEF void WINAPI VRegDestroyHive(_In_opt_ VRegHive* const pHive)
{
    SIZE_T i;
    if (!pHive)
        return;

    for (i = 0; i < pHive->nodes.count; ++i)
    {
        VReg__Node* const pNode = VREG__NODE_AT(&pHive->nodes, i);
        VReg__HiveDropWatches(pNode);
        VReg__HandleSpaceRelease(VReg__ProcessHandleSpace(), pNode->hKey);
        VReg__NodeFree(pHive->pAllocContext, pNode);
    }

    VReg__ArrayFree(pHive->pAllocContext, &pHive->nodes);
    VReg__WStringFree(pHive->pAllocContext, &pHive->lowerPath);
    VReg__Free(pHive->pAllocContext, pHive);
}

VREGDEF void WINAPI VRegFreezeHive(_In_ VRegHive* const pHive)
{
    SIZE_T i;

    // Taking the lock once more waits out every writer still inside; readers
    // that see the flag afterwards skip the lock and see all their writes.
    AcquireSRWLockExclusive(&pHive->lock);
    WriteRelease(&pHive->isFrozen, 1);
    for (i = 0; i < pHive->nodes.count; ++i)
        VReg__HiveDropWatches(VREG__NODE_AT(&pHive->nodes, i));

    ReleaseSRWLockExclusive(&pHive->lock);
}

VREGDEF BOOL WINAPI VRegIsHiveFrozen(_In_ VRegHive* const pHive)
{
    return VReg__HiveIsFrozen(pHive);
}

VREGDEF _Ret_notnull_ VReg__Node* VReg__HiveRoot(_In_ VRegHive* const pHive)
{
    return pHive->pRoot;
}

VREGDEF _Ret_maybenull_ VRegRegistry* VReg__HiveLower(_In_ VRegHive* const pHive)
{
    return pHive->pLower;
}

VREGDEF _Ret_maybenull_ void* VReg__HiveAllocContext(_In_ VRegHive* const pHive)
{
    return pHive->pAllocContext;
}

VREGDEF HKEY VReg__NodeHandle(_In_ const VReg__Node* const pNode)
{
    return pNode->hKey;
}

VREGDEF _Ret_maybenull_ VReg__Node* VReg__NodeOf(_In_opt_ const HKEY hKey)
{
    return (VReg__Node*)VReg__HandleSpaceFind(VReg__ProcessHandleSpace(), hKey);
}

VREGDEF _Ret_notnull_ VRegHive* VReg__NodeHive(_In_ const VReg__Node* const pNode)
{
    return pNode->pHive;
}

// --- Keys ---

/// Follows @p pPath through the upper layer from @p pParent as far as it
/// goes. @p pcMatched receives how many components were found and
/// @p ppCurrent the node they led to; when the path goes on from there,
/// @p pIsVisible and @p pLowerPath describe that node's lower key.
static LSTATUS VReg__HiveWalkUpper(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    _Outptr_ VReg__Node** const ppCurrent,
    _Out_ SIZE_T* const pcMatched,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pLowerPath
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    VReg__Node* pCurrent = pParent;
    SIZE_T i = 0;

    *pIsVisible = FALSE;
    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        for (; i < cPath; ++i)
        {
            VReg__Node* const pChild = VReg__HiveFindChild(pCurrent, pPath[i], NULL);
            if (!pChild)
                break;
            if (pChild->isTombstone)
            {
                status = ERROR_FILE_NOT_FOUND;
                break;
            }

            pCurrent = pChild;
        }

        if (status == ERROR_SUCCESS && i < cPath)
            status = VReg__HiveViewOf(pHive, pCurrent, pIsVisible, pLowerPath);
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    *ppCurrent = pCurrent;
    *pcMatched = i;
    return status;
}

VREGDEF LSTATUS VReg__HiveOpenKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired,
    _Out_ VReg__OpenResult* const pResult
)
{
    VReg__Node* pCurrent = pParent;
    SIZE_T cMatched = 0;
    BOOL isVisible = FALSE;
    VReg__WString lowerPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    REGSAM samLower;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->hLower = NULL;

    status = VReg__HiveWalkUpper(pHive, pParent, pPath, cPath, &pCurrent, &cMatched, &isVisible, &lowerPath);
    if (status != ERROR_SUCCESS)
        goto done;
    if (cMatched == cPath)
    {
        pResult->pNode = pCurrent;
        goto done;
    }
    if (!isVisible)
    {
        status = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    // Nothing of the upper layer is at or below the key, so it is the lower
    // key's to answer.
    if (!VReg__HiveJoinPath(pHive->pAllocContext, &lowerPath, pPath + cMatched, cPath - cMatched))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    samLower = VReg__HiveIsFrozen(pHive) ? samDesired : KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES);
    status = VReg__HiveOpenLower(pHive, &lowerPath, samLower, &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    if (VReg__HiveIsFrozen(pHive))
    {
        pResult->hLower = hLower;
        hLower = NULL;
        goto done;
    }

    // Writable: every open inside the mount answers with a node, or a write
    // through a lower handle handed out now would miss a later copy-up.
    if (!VReg__HiveLockWrite(pHive))
    {
        // Frozen since the lower open: the frozen answer is a lower handle.
        VReg__HiveCloseLower(pHive, hLower);
        hLower = NULL;
        status = VReg__HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
        goto done;
    }

    if (pCurrent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        SIZE_T i;
        for (i = cMatched; i < cPath; ++i)
        {
            VReg__Node* pChild = VReg__HiveFindChild(pCurrent, pPath[i], NULL);
            if (pChild)
            {
                if (pChild->isTombstone)
                {
                    status = ERROR_FILE_NOT_FOUND;
                    break;
                }

                pCurrent = pChild;
                continue;
            }

            status = VReg__HiveAddChild(pHive, pCurrent, pPath[i], VREG__NO_NAME, FALSE, &pChild);
            if (status != ERROR_SUCCESS)
                break;

            pChild->isTransparent = TRUE;
            pChild->isLowerBacked = TRUE;
            pCurrent = pChild;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pHive->pAllocContext, &lowerPath);
    return status;
}

/// How far @p pPath exists in the upper layer below @p pParent, for
/// @ref VReg__HiveCreateKey: @p pcUpper components; @p pIsBehindTombstone
/// when a tombstone stopped the walk; otherwise the lower view of the last
/// node reached, when the path goes on.
static LSTATUS VReg__HiveMeasureUpper(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    _Out_ SIZE_T* const pcUpper,
    _Out_ BOOL* const pIsBehindTombstone,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VReg__Node* pCurrent = pParent;
    SIZE_T cUpper = 0;

    *pIsBehindTombstone = FALSE;
    *pIsVisible = FALSE;
    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        for (; cUpper < cPath; ++cUpper)
        {
            const VReg__Node* const pChild = VReg__HiveFindChild(pCurrent, pPath[cUpper], NULL);
            if (!pChild)
                break;
            if (pChild->isTombstone)
            {
                *pIsBehindTombstone = TRUE;
                break;
            }

            pCurrent = pChild;
        }

        if (cUpper < cPath && !*pIsBehindTombstone)
            status = VReg__HiveViewOf(pHive, pCurrent, pIsVisible, pViewPath);
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREGDEF LSTATUS VReg__HiveCreateKey(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    _In_z_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    _Out_ VReg__OpenResult* const pResult,
    _Out_ LPDWORD const pdwDisposition
)
{
    const VReg__WStringView className = VReg__ViewOf(pszClassName);
    const BOOL isVolatile = (dwOptions & REG_OPTION_VOLATILE) != 0;
    SIZE_T cUpper = 0;
    SIZE_T cLower;
    BOOL isBehindTombstone = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString probePath = {NULL, 0, 0};
    VReg__Node* pCurrent;
    SIZE_T i;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->hLower = NULL;
    *pdwDisposition = REG_OPENED_EXISTING_KEY;

    if (VReg__HiveIsFrozen(pHive))
    {
        status = VReg__HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
        return status == ERROR_FILE_NOT_FOUND ? ERROR_ACCESS_DENIED : status;
    }

    // How far the path exists: in the upper layer first, then below it.
    status = VReg__HiveMeasureUpper(pHive, pParent, pPath, cPath, &cUpper, &isBehindTombstone, &isVisible, &viewPath);
    if (status != ERROR_SUCCESS)
        goto done;

    cLower = cUpper;
    if (isVisible)
    {
        for (; cLower < cPath; ++cLower)
        {
            HKEY hLower = NULL;
            if (
                !VReg__WStringAssign(pHive->pAllocContext, &probePath, viewPath.psz, viewPath.cch)
                || !VReg__HiveJoinPath(pHive->pAllocContext, &probePath, pPath + cUpper, cLower + 1 - cUpper)
            )
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }

            status = VReg__HiveOpenLower(pHive, &probePath, KEY_QUERY_VALUE, &hLower);
            VReg__HiveCloseLower(pHive, status == ERROR_SUCCESS ? hLower : NULL);
            if (status == ERROR_FILE_NOT_FOUND)
            {
                status = ERROR_SUCCESS;
                break;
            }
            if (!VReg__StatusExists(status))
                goto done;

            status = ERROR_SUCCESS;
        }
    }

    if (!VReg__HiveLockWrite(pHive))
    {
        status = VReg__HiveCreateKey(
            pHive,
            pParent,
            pPath,
            cPath,
            pszClassName,
            dwOptions,
            samDesired,
            pResult,
            pdwDisposition
        );
        goto done;
    }

    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        pCurrent = pParent;
        for (i = 0; i < cPath; ++i)
        {
            const BOOL isLast = i + 1 == cPath;
            const BOOL isInLower = i >= cUpper && i < cLower;
            VReg__Node* pChild = VReg__HiveFindChild(pCurrent, pPath[i], NULL);
            VReg__Node* pNew;
            if (pChild && !pChild->isTombstone)
            {
                pCurrent = pChild;
                continue;
            }

            if (pChild)
            {
                // Created again over a deleted lower key: nothing below shows.
                const VReg__WStringView newClass = isLast ? className : VREG__NO_NAME;
                if (!VReg__WStringAssign(pHive->pAllocContext, &pChild->className, newClass.pch, newClass.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                    break;
                }

                pChild->isTombstone = FALSE;
                pChild->isOpaque = TRUE;
                VReg__HiveTouch(pChild);
                VReg__HiveMaterialize(pCurrent);
                VReg__HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
                pCurrent = pChild;
                if (isLast)
                    *pdwDisposition = REG_CREATED_NEW_KEY;
                continue;
            }

            if (!isInLower && pCurrent->isVolatile && !isVolatile)
            {
                status = ERROR_CHILD_MUST_BE_VOLATILE;
                break;
            }

            status = VReg__HiveAddChild(
                pHive,
                pCurrent,
                pPath[i],
                isLast && !isInLower ? className : VREG__NO_NAME,
                !isInLower && isVolatile,
                &pNew
            );
            if (status != ERROR_SUCCESS)
                break;

            if (isInLower)
            {
                pNew->isTransparent = TRUE;
                pNew->isLowerBacked = TRUE;
            }
            else
            {
                VReg__HiveMaterialize(pCurrent);
                VReg__HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
                if (isLast)
                    *pdwDisposition = REG_CREATED_NEW_KEY;
            }

            pCurrent = pNew;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__WStringFree(pHive->pAllocContext, &viewPath);
    VReg__WStringFree(pHive->pAllocContext, &probePath);
    return status;
}

/// What @ref VReg__HiveDeleteKey needs to know about @p pNode before it calls
/// the lower registry: the names of its tombstone children (a child that is
/// not one fails the call), its view and its parent's, and its name.
static LSTATUS VReg__HiveInspectForDelete(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _Inout_ VReg__Array* const pTombstones,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath,
    _Out_ BOOL* const pIsParentVisible,
    _Inout_ VReg__WString* const pParentViewPath,
    _Inout_ VReg__WString* const pKeyName
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;

    *pIsVisible = FALSE;
    *pIsParentVisible = FALSE;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (!pNode->pParent)
        status = ERROR_ACCESS_DENIED;

    for (i = 0; status == ERROR_SUCCESS && i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);
        if (!pChild->isTombstone)
            status = ERROR_ACCESS_DENIED;
        else if (!VReg__WStringArrayInsertSorted(pHive->pAllocContext, pTombstones, VReg__WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);
    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode->pParent, pIsParentVisible, pParentViewPath);
    if (status == ERROR_SUCCESS)
    {
        const VReg__WStringView keyName = VReg__WStringViewOf(&pNode->name);
        if (!VReg__WStringAssign(pHive->pAllocContext, pKeyName, keyName.pch, keyName.cch))
            status = ERROR_OUTOFMEMORY;
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveDeleteKey(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath
)
{
    void* const pAllocContext = pHive->pAllocContext;
    VReg__OpenResult opened;
    VReg__Node* pNode;
    VReg__Array tombstones = {NULL, 0, 0};
    BOOL isVisible = FALSE;
    BOOL isParentVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString parentViewPath = {NULL, 0, 0};
    VReg__WString keyName = {NULL, 0, 0};
    VReg__WString lowerName = {NULL, 0, 0};
    HKEY hLower = NULL;
    BOOL hasLower = FALSE;
    SIZE_T i;
    LSTATUS status;

    if (VReg__HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    status = VReg__HiveOpenKey(pHive, pParent, pPath, cPath, KEY_READ, &opened);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen since the check above, so the open answered with a lower key.
    if (!opened.pNode)
    {
        VReg__HiveCloseLower(pHive, opened.hLower);
        return ERROR_ACCESS_DENIED;
    }

    pNode = opened.pNode;
    status = VReg__HiveInspectForDelete(
        pHive,
        pNode,
        &tombstones,
        &isVisible,
        &viewPath,
        &isParentVisible,
        &parentViewPath,
        &keyName
    );
    if (status != ERROR_SUCCESS)
        goto done;

    // Whether a lower key sits at the node's path decides if a tombstone must
    // replace the node, even when the node is opaque and hides what is below
    // it. A lower subkey that no tombstone hides still makes the key non-empty.
    if (isParentVisible)
    {
        const VReg__WStringView keyNameView = VReg__WStringViewOf(&keyName);
        if (!VReg__HiveJoinPath(pAllocContext, &parentViewPath, &keyNameView, 1))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        status = VReg__HiveOpenLower(pHive, &parentViewPath, KEY_ENUMERATE_SUB_KEYS, &hLower);
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
            goto done;

        hasLower = status == ERROR_SUCCESS;
        status = ERROR_SUCCESS;
        for (i = 0; hasLower && isVisible; ++i)
        {
            const LSTATUS enumStatus = VReg__EnumKeyOwned(pHive->pLower, hLower, (DWORD)i, pAllocContext, &lowerName);
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            if (enumStatus != ERROR_SUCCESS)
            {
                status = enumStatus;
                goto done;
            }
            if (!VReg__WStringArrayContainsSorted(&tombstones, VReg__WStringViewOf(&lowerName)))
            {
                status = ERROR_ACCESS_DENIED;
                goto done;
            }
        }
    }

    if (!VReg__HiveLockWrite(pHive))
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        VReg__Node* const pNodeParent = pNode->pParent;
        for (i = 0; i < pNode->children.count; ++i)
        {
            if (!VREG__NODE_AT(&pNode->children, i)->isTombstone)
            {
                status = ERROR_ACCESS_DENIED;
                break;
            }
        }

        if (status == ERROR_SUCCESS)
        {
            VReg__HiveRemove(pNode);
            if (hasLower)
            {
                VReg__Node* pTombstone;
                status = VReg__HiveAddChild(
                    pHive,
                    pNodeParent,
                    VReg__WStringViewOf(&keyName),
                    VREG__NO_NAME,
                    FALSE,
                    &pTombstone
                );
                if (status == ERROR_SUCCESS)
                    pTombstone->isTombstone = TRUE;
            }

            VReg__HiveMaterialize(pNodeParent);
        }
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &tombstones);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &parentViewPath);
    VReg__WStringFree(pAllocContext, &keyName);
    VReg__WStringFree(pAllocContext, &lowerName);
    return status;
}

VREGDEF LSTATUS VReg__HiveDeleteTree(_In_ VRegHive* const pHive, _Inout_ VReg__Node* const pNode)
{
    if (!VReg__HiveLockWrite(pHive))
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
    {
        VReg__HiveUnlockWrite(pHive);
        return ERROR_KEY_DELETED;
    }

    while (pNode->children.count != 0)
        VReg__HiveRemove(VREG__NODE_AT(&pNode->children, pNode->children.count - 1));

    VReg__NodeClearValues(pHive->pAllocContext, pNode);
    pNode->isOpaque = pHive->pLower != NULL;
    pNode->isTransparent = FALSE;
    pNode->isLowerBacked = FALSE;
    VReg__HiveMaterialize(pNode);
    VReg__HiveTouch(pNode);
    VReg__HiveNotify(pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    VReg__HiveUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

// --- Values ---

/// Reads value @p name of @p pNode from the upper layer. @p pIsFound says
/// whether the layer decided the answer; otherwise @p pIsVisible and
/// @p pViewPath describe the lower key to ask.
static LSTATUS VReg__HiveQueryUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const VReg__WStringView name,
    _Out_ BOOL* const pIsFound,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VReg__Value* pValue;

    *pIsFound = TRUE;
    *pIsVisible = FALSE;
    *pdwType = REG_NONE;
    *pcbData = 0;
    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        if (pValue->isTombstone)
        {
            status = ERROR_FILE_NOT_FOUND;
        }
        else
        {
            *pdwType = pValue->dwType;
            *pcbData = (DWORD)pValue->data.cb;
            if (pData && !VReg__BufferAssign(pHive->pAllocContext, pData, pValue->data.pb, pValue->data.cb))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        *pIsFound = FALSE;
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveQueryValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData
)
{
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    LSTATUS status = VReg__HiveQueryUpperValue(
        pHive,
        pNode,
        VReg__ViewOf(pszName),
        &isFound,
        pdwType,
        pcbData,
        pData,
        &isVisible,
        &viewPath
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;
    if (!isVisible)
    {
        status = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!pData)
    {
        *pcbData = 0;
        status = VRegQueryValueExW(
            pHive->pLower,
            hLower,
            pszName,
            NULL,
            pdwType,
            NULL,
            pcbData
        );
        goto done;
    }

    status = VReg__QueryValueOwned(pHive->pLower, hLower, pszName, pHive->pAllocContext, pdwType, pData);
    if (status == ERROR_SUCCESS)
        *pcbData = (DWORD)pData->cb;

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pHive->pAllocContext, &viewPath);
    return status;
}

static LSTATUS VReg__HiveStoreValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pNode,
    const VReg__WStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    const BOOL isMuiText
)
{
    void* const pAllocContext = pHive->pAllocContext;
    VReg__Buffer bytes = {NULL, 0, 0};
    VReg__Value* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (!VReg__BufferAssign(pAllocContext, &bytes, pData, cbData))
        return ERROR_OUTOFMEMORY;

    if (!VReg__HiveLockWrite(pHive))
    {
        VReg__BufferFree(pAllocContext, &bytes);
        return ERROR_ACCESS_DENIED;
    }

    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        VReg__BufferFree(pAllocContext, &pValue->data);
        pValue->dwType = dwType;
        pValue->data = bytes;
        pValue->isTombstone = FALSE;
        pValue->isMuiText = isMuiText;
        bytes.pb = NULL;
    }
    else
    {
        VReg__Value value;
        value.name.psz = NULL;
        value.name.cch = 0;
        value.name.cchCapacity = 0;
        value.dwType = dwType;
        value.data = bytes;
        value.isTombstone = FALSE;
        value.isMuiText = isMuiText;
        if (!VReg__WStringAssign(pAllocContext, &value.name, name.pch, name.cch))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (!VReg__ArrayAppend(pAllocContext, &pNode->values, sizeof(value), &value))
        {
            VReg__WStringFree(pAllocContext, &value.name);
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            bytes.pb = NULL;
        }
    }

    if (status == ERROR_SUCCESS)
    {
        VReg__HiveMaterialize(pNode);
        VReg__HiveTouch(pNode);
        VReg__HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
    }

    VReg__HiveUnlockWrite(pHive);
    if (bytes.pb)
        VReg__BufferFree(pAllocContext, &bytes);
    return status;
}

VREGDEF LSTATUS VReg__HiveSetValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData
)
{
    return VReg__HiveStoreValue(pHive, pNode, VReg__ViewOf(pszName), dwType, pData, cbData, FALSE);
}

VREGDEF LSTATUS VReg__HiveSetMuiTextValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
)
{
    const VReg__WStringView text = VReg__ViewOf(pszText);
    return VReg__HiveStoreValue(
        pHive,
        pNode,
        VReg__ViewOf(pszName),
        REG_SZ,
        (const BYTE*)pszText,
        (DWORD)((text.cch + 1) * sizeof(wchar_t)),
        TRUE
    );
}

VREGDEF LSTATUS VReg__HiveQueryMuiText(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Inout_ VReg__WString* const pText
)
{
    const VReg__WStringView name = VReg__ViewOf(pszName);
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    BOOL isFound = TRUE;
    const VReg__Value* pValue;
    DWORD dwType;
    DWORD cbData;

    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        if (pValue->isTombstone)
        {
            status = ERROR_FILE_NOT_FOUND;
        }
        else if (!pValue->isMuiText)
        {
            status = ERROR_INVALID_DATA;
        }
        else
        {
            const SIZE_T cchText = pValue->data.cb < sizeof(wchar_t) ? 0 : pValue->data.cb / sizeof(wchar_t) - 1;
            if (!VReg__WStringAssign(pHive->pAllocContext, pText, (const wchar_t*)pValue->data.pb, cchText))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        isFound = FALSE;
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    if (isFound)
        return status;

    status = VReg__HiveQueryValue(pHive, pNode, pszName, &dwType, &cbData, NULL);
    return status == ERROR_SUCCESS ? ERROR_INVALID_DATA : status;
}

VREGDEF LSTATUS VReg__HiveDeleteValue(_In_ VRegHive* const pHive, _Inout_ VReg__Node* const pNode, _In_z_ const LPCWSTR pszName)
{
    void* const pAllocContext = pHive->pAllocContext;
    const VReg__WStringView name = VReg__ViewOf(pszName);
    BOOL isLocked;
    BOOL isInUpper = FALSE;
    BOOL isInLower = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    VReg__Value* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (VReg__HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    isLocked = VReg__HiveLockRead(pHive);
    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (pValue && pValue->isTombstone)
        status = ERROR_FILE_NOT_FOUND;
    else
        isInUpper = pValue != NULL;
    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode, &isVisible, &viewPath);
    VReg__HiveUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS)
        goto done;

    if (isVisible)
    {
        status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
        if (status == ERROR_SUCCESS)
        {
            const LSTATUS queryStatus = VRegQueryValueExW(
                pHive->pLower,
                hLower,
                pszName,
                NULL,
                NULL,
                NULL,
                NULL
            );
            if (queryStatus != ERROR_FILE_NOT_FOUND && !VReg__StatusExists(queryStatus))
            {
                status = queryStatus;
                goto done;
            }

            isInLower = VReg__StatusExists(queryStatus);
        }
        else if (status != ERROR_FILE_NOT_FOUND)
        {
            goto done;
        }

        status = ERROR_SUCCESS;
    }

    if (!isInUpper && !isInLower)
    {
        status = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    if (!VReg__HiveLockWrite(pHive))
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        pValue = VReg__HiveFindValue(pNode, name);
        if (isInLower)
        {
            if (pValue)
            {
                VReg__BufferFree(pAllocContext, &pValue->data);
                pValue->dwType = REG_NONE;
                pValue->isTombstone = TRUE;
                pValue->isMuiText = FALSE;
            }
            else
            {
                VReg__Value tombstone;
                ZeroMemory(&tombstone, sizeof(tombstone));
                tombstone.dwType = REG_NONE;
                tombstone.isTombstone = TRUE;
                if (!VReg__WStringAssign(pAllocContext, &tombstone.name, name.pch, name.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                }
                else if (!VReg__ArrayAppend(pAllocContext, &pNode->values, sizeof(tombstone), &tombstone))
                {
                    VReg__WStringFree(pAllocContext, &tombstone.name);
                    status = ERROR_OUTOFMEMORY;
                }
            }
        }
        else if (pValue)
        {
            const SIZE_T index = (SIZE_T)(pValue - &VREG__VALUE_AT(&pNode->values, 0));
            VReg__ValueFree(pAllocContext, pValue);
            VReg__ArrayRemoveAt(&pNode->values, sizeof(*pValue), index);
        }

        if (status == ERROR_SUCCESS)
        {
            VReg__HiveMaterialize(pNode);
            VReg__HiveTouch(pNode);
            VReg__HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
        }
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pAllocContext, &viewPath);
    return status;
}

// --- Enumeration ---

/// The upper layer's part of @ref VReg__HiveEnumKey. @p pIsFound says the
/// answer is complete. Otherwise, when @p pIsClassVisible, the entry is an
/// upper key made from a lower one at @p pClassPath whose class is still to
/// be read; and when not, the lower key at @p pViewPath (if @p pIsVisible)
/// continues the enumeration past @p pcUpper upper entries, with the names in
/// @p pShadowed hidden.
static LSTATUS VReg__HiveEnumUpperKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Inout_ VReg__WString* const pClassName,
    _Out_ FILETIME* const pLastWrite,
    _Out_ BOOL* const pIsFound,
    _Out_ BOOL* const pIsClassVisible,
    _Inout_ VReg__WString* const pClassPath,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath,
    _Out_ DWORD* const pcUpper,
    _Inout_ VReg__Array* const pShadowed
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    DWORD cUpper = 0;
    SIZE_T i;

    *pIsFound = FALSE;
    *pIsClassVisible = FALSE;
    *pIsVisible = FALSE;
    ZeroMemory(pLastWrite, sizeof(*pLastWrite));
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && !*pIsFound && !*pIsClassVisible && i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);

        // A transparent child stands for a lower key; it is listed with the
        // lower ones.
        if (pChild->isTransparent)
            continue;

        if (!pChild->isTombstone && cUpper++ == dwIndex)
        {
            if (
                !VReg__WStringAssign(pAllocContext, pName, VReg__WStringText(&pChild->name), pChild->name.cch)
                || !VReg__WStringAssign(
                    pAllocContext,
                    pClassName,
                    VReg__WStringText(&pChild->className),
                    pChild->className.cch
                )
            )
            {
                status = ERROR_OUTOFMEMORY;
                break;
            }

            *pLastWrite = pChild->lastWrite;
            if (!pChild->isLowerBacked)
            {
                *pIsFound = TRUE;
                break;
            }

            status = VReg__HiveViewOf(pHive, pChild, pIsClassVisible, pClassPath);
            if (status == ERROR_SUCCESS && !*pIsClassVisible)
                *pIsFound = TRUE;
            break;
        }

        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowed, VReg__WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound && !*pIsClassVisible)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg__HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREGDEF LSTATUS VReg__HiveEnumKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Inout_ VReg__WString* const pClassName,
    _Out_ FILETIME* const pLastWrite
)
{
    void* const pAllocContext = pHive->pAllocContext;
    BOOL isFound = FALSE;
    BOOL isClassVisible = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString classPath = {NULL, 0, 0};
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString nameBuffer = {NULL, 0, 0};
    VReg__WString classBuffer = {NULL, 0, 0};
    VReg__Array shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD cchMaxClass = 0;
    DWORD dwSkip;
    HKEY hLower = NULL;
    DWORD i;
    LSTATUS status = VReg__HiveEnumUpperKey(
        pHive,
        pNode,
        dwIndex,
        pName,
        pClassName,
        pLastWrite,
        &isFound,
        &isClassVisible,
        &classPath,
        &isVisible,
        &viewPath,
        &cUpper,
        &shadowed
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;

    // An upper key that was made from a lower one has no class of its own.
    if (isClassVisible)
    {
        status = VReg__HiveQueryLowerClass(pHive, &classPath, pClassName);
        goto done;
    }

    if (!isVisible)
    {
        status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    status = VRegQueryInfoKeyW(
        pHive->pLower,
        hLower,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &cchMaxClass,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (
        !VReg__WStringResize(pAllocContext, &nameBuffer, VREG__INITIAL_NAME_CHARS)
        || !VReg__WStringResize(pAllocContext, &classBuffer, cchMaxClass + 1)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    dwSkip = dwIndex - cUpper;
    for (i = 0;; ++i)
    {
        DWORD cchName = (DWORD)nameBuffer.cchCapacity;
        DWORD cchClass = (DWORD)classBuffer.cchCapacity;
        FILETIME ft;
        VReg__WStringView lowerName;
        ZeroMemory(&ft, sizeof(ft));
        status = VRegEnumKeyExW(
            pHive->pLower,
            hLower,
            i,
            nameBuffer.psz,
            &cchName,
            NULL,
            classBuffer.psz,
            &cchClass,
            &ft
        );
        if (status != ERROR_SUCCESS)
            goto done;

        lowerName.pch = nameBuffer.psz;
        lowerName.cch = cchName;
        if (VReg__WStringArrayContainsSorted(&shadowed, lowerName) || dwSkip-- != 0)
            continue;

        if (
            !VReg__WStringAssign(pAllocContext, pName, lowerName.pch, lowerName.cch)
            || !VReg__WStringAssign(pAllocContext, pClassName, classBuffer.psz, cchClass)
        )
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        *pLastWrite = ft;
        status = ERROR_SUCCESS;
        goto done;
    }

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &shadowed);
    VReg__WStringFree(pAllocContext, &classPath);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &nameBuffer);
    VReg__WStringFree(pAllocContext, &classBuffer);
    return status;
}

/// The upper layer's part of @ref VReg__HiveEnumValue, like
/// @ref VReg__HiveEnumUpperKey without the class step.
static LSTATUS VReg__HiveEnumUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData,
    _Out_ BOOL* const pIsFound,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath,
    _Out_ DWORD* const pcUpper,
    _Inout_ VReg__Array* const pShadowed
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    DWORD cUpper = 0;
    SIZE_T i;

    *pIsFound = FALSE;
    *pIsVisible = FALSE;
    *pdwType = REG_NONE;
    *pcbData = 0;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && !*pIsFound && i < pNode->values.count; ++i)
    {
        const VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        if (!pValue->isTombstone && cUpper++ == dwIndex)
        {
            *pIsFound = TRUE;
            if (
                !VReg__WStringAssign(pAllocContext, pName, VReg__WStringText(&pValue->name), pValue->name.cch)
                || (pData && !VReg__BufferAssign(pAllocContext, pData, pValue->data.pb, pValue->data.cb))
            )
            {
                status = ERROR_OUTOFMEMORY;
                break;
            }

            *pdwType = pValue->dwType;
            *pcbData = (DWORD)pValue->data.cb;
            break;
        }

        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowed, VReg__WStringViewOf(&pValue->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg__HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREGDEF LSTATUS VReg__HiveEnumValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData
)
{
    void* const pAllocContext = pHive->pAllocContext;
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString sizeName = {NULL, 0, 0};
    VReg__Array shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD dwSkip;
    HKEY hLower = NULL;
    DWORD i;
    LSTATUS status = VReg__HiveEnumUpperValue(
        pHive,
        pNode,
        dwIndex,
        pName,
        pdwType,
        pcbData,
        pData,
        &isFound,
        &isVisible,
        &viewPath,
        &cUpper,
        &shadowed
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;
    if (!isVisible)
    {
        status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    dwSkip = dwIndex - cUpper;
    for (i = 0;; ++i)
    {
        DWORD cchName;
        status = VReg__EnumValueOwned(pHive->pLower, hLower, i, pAllocContext, pName, pdwType, NULL);
        if (status != ERROR_SUCCESS)
            goto done;

        if (VReg__WStringArrayContainsSorted(&shadowed, VReg__WStringViewOf(pName)) || dwSkip-- != 0)
            continue;

        if (pData)
        {
            // The scan fetched no data; enumerate the matching entry again
            // with it, so name, type and data all come from the one call,
            // should the entry have changed since the scan.
            status = VReg__EnumValueOwned(pHive->pLower, hLower, i, pAllocContext, pName, pdwType, pData);
            if (status == ERROR_SUCCESS)
                *pcbData = (DWORD)pData->cb;
            goto done;
        }

        if (!VReg__WStringResize(pAllocContext, &sizeName, pName->cch))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cchName = (DWORD)(pName->cch + 1);
        *pcbData = 0;
        status = VRegEnumValueW(
            pHive->pLower,
            hLower,
            i,
            sizeName.psz,
            &cchName,
            NULL,
            NULL,
            NULL,
            pcbData
        );
        goto done;
    }

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &shadowed);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &sizeName);
    return status;
}

/// The upper layer's part of @ref VReg__HiveQueryInfo: the counts and
/// maxima of the node's own entries into @p pInfo, the names that shadow
/// lower entries, and the node's relation to its lower key.
static LSTATUS VReg__HiveQueryUpperInfo(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _Inout_ VReg__KeyInfo* const pInfo,
    _Inout_ VReg__Array* const pShadowedKeys,
    _Inout_ VReg__Array* const pShadowedValues,
    _Out_ BOOL* const pIsTransparent,
    _Out_ BOOL* const pIsLowerBacked,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;

    *pIsTransparent = FALSE;
    *pIsLowerBacked = FALSE;
    *pIsVisible = FALSE;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);
        if (pChild->isTransparent)
            continue;

        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowedKeys, VReg__WStringViewOf(&pChild->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pChild->isTombstone)
            continue;

        ++pInfo->cSubKeys;
        pInfo->cchMaxSubKey = VReg__MaxDword(pInfo->cchMaxSubKey, (DWORD)pChild->name.cch);
        pInfo->cchMaxClass = VReg__MaxDword(pInfo->cchMaxClass, (DWORD)pChild->className.cch);
    }

    for (i = 0; status == ERROR_SUCCESS && i < pNode->values.count; ++i)
    {
        const VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowedValues, VReg__WStringViewOf(&pValue->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pValue->isTombstone)
            continue;

        ++pInfo->cValues;
        pInfo->cchMaxValueName = VReg__MaxDword(pInfo->cchMaxValueName, (DWORD)pValue->name.cch);
        pInfo->cbMaxValue = VReg__MaxDword(pInfo->cbMaxValue, (DWORD)pValue->data.cb);
    }

    if (status == ERROR_SUCCESS)
    {
        *pIsTransparent = pNode->isTransparent;
        *pIsLowerBacked = pNode->isLowerBacked;
        pInfo->lastWrite = pNode->lastWrite;
        const VReg__WStringView className = VReg__WStringViewOf(&pNode->className);
        if (!VReg__WStringAssign(pAllocContext, &pInfo->className, className.pch, className.cch))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveQueryInfo(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _Inout_ VReg__KeyInfo* const pInfo
)
{
    void* const pAllocContext = pHive->pAllocContext;
    VReg__Array shadowedKeys = {NULL, 0, 0};
    VReg__Array shadowedValues = {NULL, 0, 0};
    BOOL isTransparent = FALSE;
    BOOL isLowerBacked = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString lowerClass = {NULL, 0, 0};
    HKEY hLower = NULL;
    DWORD cSubKeys = 0;
    DWORD cchMaxSubKey = 0;
    DWORD cchMaxClass = 0;
    DWORD cValues = 0;
    DWORD cchMaxValueName = 0;
    DWORD cbMaxValue = 0;
    FILETIME lastWrite;
    SIZE_T i;
    LSTATUS status;

    pInfo->cSubKeys = 0;
    pInfo->cchMaxSubKey = 0;
    pInfo->cchMaxClass = 0;
    pInfo->cValues = 0;
    pInfo->cchMaxValueName = 0;
    pInfo->cbMaxValue = 0;
    ZeroMemory(&lastWrite, sizeof(lastWrite));

    status = VReg__HiveQueryUpperInfo(
        pHive,
        pNode,
        pInfo,
        &shadowedKeys,
        &shadowedValues,
        &isTransparent,
        &isLowerBacked,
        &isVisible,
        &viewPath
    );
    if (status != ERROR_SUCCESS || !isVisible)
        goto done;

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hLower);
    if (status == ERROR_FILE_NOT_FOUND)
    {
        status = ERROR_SUCCESS;
        goto done;
    }
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__QueryInfoKeyOwned(
        pHive->pLower,
        hLower,
        pAllocContext,
        &lowerClass,
        NULL,
        NULL,
        &cSubKeys,
        &cchMaxSubKey,
        &cchMaxClass,
        &cValues,
        &cchMaxValueName,
        &cbMaxValue,
        NULL,
        &lastWrite
    );
    if (status != ERROR_SUCCESS)
        goto done;

    // An upper entry that shadows a lower one is counted once.
    for (i = 0; i < shadowedKeys.count; ++i)
    {
        HKEY hChild = NULL;
        const LSTATUS childStatus = VRegOpenKeyExW(
            pHive->pLower,
            hLower,
            VReg__WStringText(&VREG__NAME_AT(&shadowedKeys, i)),
            0,
            KEY_QUERY_VALUE | pHive->samLowerView,
            &hChild
        );
        VReg__HiveCloseLower(pHive, childStatus == ERROR_SUCCESS ? hChild : NULL);
        if (childStatus != ERROR_FILE_NOT_FOUND && !VReg__StatusExists(childStatus))
        {
            status = childStatus;
            goto done;
        }
        if (VReg__StatusExists(childStatus))
            --cSubKeys;
    }

    for (i = 0; i < shadowedValues.count; ++i)
    {
        const LSTATUS valueStatus = VRegQueryValueExW(
            pHive->pLower,
            hLower,
            VReg__WStringText(&VREG__NAME_AT(&shadowedValues, i)),
            NULL,
            NULL,
            NULL,
            NULL
        );
        if (valueStatus != ERROR_FILE_NOT_FOUND && !VReg__StatusExists(valueStatus))
        {
            status = valueStatus;
            goto done;
        }
        if (VReg__StatusExists(valueStatus))
            --cValues;
    }

    pInfo->cSubKeys += cSubKeys;
    pInfo->cValues += cValues;
    pInfo->cchMaxSubKey = VReg__MaxDword(pInfo->cchMaxSubKey, cchMaxSubKey);
    pInfo->cchMaxClass = VReg__MaxDword(pInfo->cchMaxClass, cchMaxClass);
    pInfo->cchMaxValueName = VReg__MaxDword(pInfo->cchMaxValueName, cchMaxValueName);
    pInfo->cbMaxValue = VReg__MaxDword(pInfo->cbMaxValue, cbMaxValue);
    if (isLowerBacked && !VReg__WStringAssign(pAllocContext, &pInfo->className, lowerClass.psz, lowerClass.cch))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    if (isTransparent)
        pInfo->lastWrite = lastWrite;

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &shadowedKeys);
    VReg__WStringArrayFree(pAllocContext, &shadowedValues);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &lowerClass);
    return status;
}

// --- The rest ---

/// Visits @p pNode and everything below it, leaving out a transparent node
/// without values (the way to a lower key, which only the root can be while
/// it has upper keys below it). @p pPath is the node's path, extended and
/// restored for each child.
static LSTATUS VReg__HiveListUpper(
    _In_ VRegHive* const pHive,
    _In_ const VReg__Node* const pNode,
    _Inout_ VReg__WString* const pPath,
    _In_ const VReg__UpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isBare = pNode->isTransparent && pNode->values.count == 0;
    const SIZE_T cchPath = pPath->cch;
    VReg__UpperRecord record;
    LSTATUS status;
    SIZE_T i;

    ZeroMemory(&record, sizeof(record));
    record.path = VReg__WStringViewOf(pPath);
    if (!isBare)
    {
        record.kind = VREG__UPPER_KEY;
        record.isOpaque = pNode->isOpaque;
        status = pfnVisit(pContext, &record);
        if (status != ERROR_SUCCESS)
            return status;
    }

    for (i = 0; i < pNode->values.count; ++i)
    {
        const VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        ZeroMemory(&record, sizeof(record));
        record.kind = VREG__UPPER_VALUE;
        record.path = VReg__WStringViewOf(pPath);
        record.name = VReg__WStringViewOf(&pValue->name);
        record.dwType = pValue->dwType;
        record.pData = pValue->data.pb;
        record.cbData = (DWORD)pValue->data.cb;
        record.isTombstone = pValue->isTombstone;
        status = pfnVisit(pContext, &record);
        if (status != ERROR_SUCCESS)
            return status;
    }

    for (i = 0; i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);
        if (
            (pPath->cch != 0 && !VReg__WStringAppendChar(pAllocContext, pPath, L'\\'))
            || !VReg__WStringAppend(pAllocContext, pPath, VReg__WStringText(&pChild->name), pChild->name.cch)
        )
            return ERROR_OUTOFMEMORY;

        if (pChild->isTombstone)
        {
            ZeroMemory(&record, sizeof(record));
            record.kind = VREG__UPPER_KEY;
            record.path = VReg__WStringViewOf(pPath);
            record.isTombstone = TRUE;
            status = pfnVisit(pContext, &record);
        }
        else
        {
            status = VReg__HiveListUpper(pHive, pChild, pPath, pfnVisit, pContext);
        }

        if (status != ERROR_SUCCESS)
            return status;

        VReg__WStringTruncate(pPath, cchPath);
    }

    return ERROR_SUCCESS;
}

VREGDEF LSTATUS VReg__HiveSnapshotUpper(
    _In_ VRegHive* const pHive,
    _In_ const VReg__UpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    VReg__WString path = {NULL, 0, 0};
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    const LSTATUS status = VReg__HiveListUpper(pHive, pHive->pRoot, &path, pfnVisit, pContext);
    VReg__HiveUnlockRead(pHive, isLocked);
    VReg__WStringFree(pHive->pAllocContext, &path);
    return status;
}

VREGDEF LSTATUS VReg__HiveCheckKey(_In_ VRegHive* const pHive, _In_ VReg__Node* const pNode)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    const LSTATUS status = pNode->isDeleted ? ERROR_KEY_DELETED : ERROR_SUCCESS;
    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveWatch(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const BOOL bWatchSubtree,
    const DWORD dwFilter,
    _In_ const HANDLE hEvent
)
{
    HANDLE hOwn = NULL;
    BOOL isLocked;
    LSTATUS status = ERROR_SUCCESS;

    if (!(dwFilter & (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET)))
        return ERROR_SUCCESS;

    if (!DuplicateHandle(GetCurrentProcess(), hEvent, GetCurrentProcess(), &hOwn, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return GetLastError();

    isLocked = VReg__HiveLockWrite(pHive);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (isLocked)
    {
        VReg__ChangeWatch watch;
        watch.hEvent = hOwn;
        watch.isSubtree = bWatchSubtree != FALSE;
        watch.dwFilter = dwFilter;
        if (VReg__ArrayAppend(pHive->pAllocContext, &pNode->watches, sizeof(watch), &watch))
            hOwn = NULL;
        else
            status = ERROR_OUTOFMEMORY;
    }

    if (isLocked)
        VReg__HiveUnlockWrite(pHive);
    if (hOwn)
        CloseHandle(hOwn);
    return status;
}

VREGDEF LSTATUS VReg__HiveOpenLowerValueKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Out_ PHKEY const phLower
)
{
    const VReg__WStringView name = VReg__ViewOf(pszName);
    BOOL isLocked;
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    LSTATUS status = ERROR_SUCCESS;

    *phLower = NULL;
    isLocked = VReg__HiveLockRead(pHive);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (VReg__HiveFindValue(pNode, name))
        isFound = TRUE;
    else
        status = VReg__HiveViewOf(pHive, pNode, &isVisible, &viewPath);
    VReg__HiveUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS || isFound || !isVisible)
        goto done;

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_SUCCESS;
        goto done;
    }

    status = VRegQueryValueExW(
        pHive->pLower,
        hLower,
        pszName,
        NULL,
        NULL,
        NULL,
        NULL
    );
    if (status == ERROR_FILE_NOT_FOUND)
    {
        status = ERROR_SUCCESS;
        goto done;
    }
    if (!VReg__StatusExists(status))
        goto done;

    *phLower = hLower;
    hLower = NULL;
    status = ERROR_SUCCESS;

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pHive->pAllocContext, &viewPath);
    return status;
}

VREGDEF LSTATUS WINAPI VRegSetHiveMuiTextValue(
    _In_ VRegHive* const pHive,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_opt_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
)
{
    VReg__Array components = {NULL, 0, 0};
    VReg__OpenResult opened;
    DWORD dwDisposition = 0;
    LSTATUS status = VReg__SplitPath(pHive->pAllocContext, VReg__ViewOf(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VReg__ArrayFree(pHive->pAllocContext, &components);
        return status;
    }

    status = VReg__HiveCreateKey(
        pHive,
        pHive->pRoot,
        (const VReg__WStringView*)components.p,
        components.count,
        L"",
        0,
        KEY_SET_VALUE,
        &opened,
        &dwDisposition
    );
    VReg__ArrayFree(pHive->pAllocContext, &components);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen: the open answered with a lower key, which cannot take the value.
    if (!opened.pNode)
    {
        VReg__HiveCloseLower(pHive, opened.hLower);
        return ERROR_ACCESS_DENIED;
    }

    return VReg__HiveSetMuiTextValue(pHive, opened.pNode, pszName ? pszName : L"", pszText);
}

// ==========================================================================
// The native registry
// ==========================================================================

typedef struct VReg__NativeRegistry
{
    VRegRegistry object;
    void* pAllocContext;
    VRegNativeFunctions functions;
} VReg__NativeRegistry;

static VReg__NativeRegistry* VReg__NativeOf(_In_ VRegRegistry* const pThis)
{
    return (VReg__NativeRegistry*)pThis;
}

static void WINAPI VReg__NativeDestroy(_In_ VRegRegistry* const pThis)
{
    VReg__NativeRegistry* const pRegistry = VReg__NativeOf(pThis);
    VReg__Free(pRegistry->pAllocContext, pRegistry);
}

static LSTATUS WINAPI VReg__NativeRegOpenKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VReg__NativeOf(pThis)->functions.RegOpenKeyExW(
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS WINAPI VReg__NativeRegCreateKeyExW(
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
)
{
    return VReg__NativeOf(pThis)->functions.RegCreateKeyExW(
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

static LSTATUS WINAPI VReg__NativeRegCloseKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    return VReg__NativeOf(pThis)->functions.RegCloseKey(
        hKey
    );
}

static LSTATUS WINAPI VReg__NativeRegQueryValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VReg__NativeOf(pThis)->functions.RegQueryValueExW(
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS WINAPI VReg__NativeRegSetValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VReg__NativeOf(pThis)->functions.RegSetValueExW(
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VReg__NativeRegDeleteValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VReg__NativeOf(pThis)->functions.RegDeleteValueW(
        hKey,
        lpValueName
    );
}

static LSTATUS WINAPI VReg__NativeRegDeleteKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VReg__NativeOf(pThis)->functions.RegDeleteKeyExW(
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS WINAPI VReg__NativeRegDeleteTreeW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    return VReg__NativeOf(pThis)->functions.RegDeleteTreeW(
        hKey,
        lpSubKey
    );
}

static LSTATUS WINAPI VReg__NativeRegEnumKeyExW(
    _In_ VRegRegistry* const pThis,
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
    return VReg__NativeOf(pThis)->functions.RegEnumKeyExW(
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

static LSTATUS WINAPI VReg__NativeRegEnumValueW(
    _In_ VRegRegistry* const pThis,
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
    return VReg__NativeOf(pThis)->functions.RegEnumValueW(
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

static LSTATUS WINAPI VReg__NativeRegQueryInfoKeyW(
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
)
{
    return VReg__NativeOf(pThis)->functions.RegQueryInfoKeyW(
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

static LSTATUS WINAPI VReg__NativeRegLoadMUIStringW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    return VReg__NativeOf(pThis)->functions.RegLoadMUIStringW(
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
// carry it through the function pointer.
#pragma warning(suppress: 6054)
static LSTATUS WINAPI VReg__NativeRegGetValueW(
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
)
{
    return VReg__NativeOf(pThis)->functions.RegGetValueW(
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS WINAPI VReg__NativeRegNotifyChangeKeyValue(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    return VReg__NativeOf(pThis)->functions.RegNotifyChangeKeyValue(
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

static LSTATUS WINAPI VReg__NativeRegFlushKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    return VReg__NativeOf(pThis)->functions.RegFlushKey(
        hKey
    );
}

static const VRegRegistryVtbl VReg__NativeRegistryVtbl = {
    .Destroy = &VReg__NativeDestroy,
    .RegOpenKeyExW = &VReg__NativeRegOpenKeyExW,
    .RegCreateKeyExW = &VReg__NativeRegCreateKeyExW,
    .RegCloseKey = &VReg__NativeRegCloseKey,
    .RegQueryValueExW = &VReg__NativeRegQueryValueExW,
    .RegSetValueExW = &VReg__NativeRegSetValueExW,
    .RegDeleteValueW = &VReg__NativeRegDeleteValueW,
    .RegDeleteKeyExW = &VReg__NativeRegDeleteKeyExW,
    .RegDeleteTreeW = &VReg__NativeRegDeleteTreeW,
    .RegEnumKeyExW = &VReg__NativeRegEnumKeyExW,
    .RegEnumValueW = &VReg__NativeRegEnumValueW,
    .RegQueryInfoKeyW = &VReg__NativeRegQueryInfoKeyW,
    .RegLoadMUIStringW = &VReg__NativeRegLoadMUIStringW,
    .RegGetValueW = &VReg__NativeRegGetValueW,
    .RegNotifyChangeKeyValue = &VReg__NativeRegNotifyChangeKeyValue,
    .RegFlushKey = &VReg__NativeRegFlushKey,
};

VREGDEF void WINAPI VRegResolveNativeFunctions(_Out_ VRegNativeFunctions* const pFunctions)
{
    ZeroMemory(pFunctions, sizeof(*pFunctions));

    // A resolved export is a plain code address, which only a data pointer
    // cast can carry into the typed slot.
#define VREG_RESOLVE(name) *(void**)&pFunctions->name = VReg__FindRegistryExport(#name);
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_RESOLVE)
#undef VREG_RESOLVE
}

VREGDEF LSTATUS WINAPI VRegCreateNativeRegistry(
    _In_opt_ const VRegNativeFunctions* const pFunctions,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegRegistry** const ppRegistry
)
{
    VReg__NativeRegistry* const pRegistry =
        (VReg__NativeRegistry*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__NativeRegistry));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->object.lpVtbl = &VReg__NativeRegistryVtbl;
    pRegistry->object.pAllocContext = pAllocContext;
    pRegistry->pAllocContext = pAllocContext;
    if (pFunctions)
        pRegistry->functions = *pFunctions;
    else
        VRegResolveNativeFunctions(&pRegistry->functions);

    *ppRegistry = &pRegistry->object;
    return ERROR_SUCCESS;
}

VREGDEF _Ret_notnull_ VRegNativeFunctions* WINAPI VRegGetNativeFunctions(_In_ VRegRegistry* const pRegistry)
{
    return &VReg__NativeOf(pRegistry)->functions;
}

// ==========================================================================
// Owned reads
// ==========================================================================

VREGDEF DWORD VReg__Grown(const DWORD cbOld, const DWORD cbRequired)
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}

VREGDEF LSTATUS VReg__QueryValueOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR lpValueName,
    _In_opt_ void* const pAllocContext,
    _Out_opt_ const LPDWORD lpType,
    _Inout_ VReg__Buffer* const pData
)
{
    DWORD dwType = 0;
    DWORD cbData = 0;
    const LSTATUS sized = VRegQueryValueExW(
        pRegistry,
        hKey,
        lpValueName,
        NULL,
        &dwType,
        NULL,
        &cbData
    );
    if (sized != ERROR_SUCCESS)
        return sized;

    for (;;)
    {
        DWORD cbRead = cbData;
        LSTATUS status;
        if (!VReg__BufferResize(pAllocContext, pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            lpValueName,
            NULL,
            &dwType,
            pData->pb,
            &cbRead
        );
        if (status == ERROR_MORE_DATA)
        {
            cbData = VReg__Grown(cbData, cbRead);
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            pData->cb = cbRead;
            if (lpType)
                *lpType = dwType;
        }
        return status;
    }
}

VREGDEF LSTATUS VReg__GetValueOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR lpSubKey,
    _In_opt_z_ const LPCWSTR lpValue,
    _In_ const DWORD dwFlags,
    _In_opt_ void* const pAllocContext,
    _Out_opt_ const LPDWORD pdwType,
    _Inout_ VReg__Buffer* const pData
)
{
    DWORD dwType = 0;
    DWORD cbData = 0;
    const LSTATUS sized = VRegGetValueW(
        pRegistry,
        hKey,
        lpSubKey,
        lpValue,
        dwFlags,
        &dwType,
        NULL,
        &cbData
    );
    if (sized != ERROR_SUCCESS)
        return sized;

    for (;;)
    {
        DWORD cbRead = cbData;
        LSTATUS status;
        if (!VReg__BufferResize(pAllocContext, pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VRegGetValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValue,
            dwFlags,
            &dwType,
            pData->pb,
            &cbRead
        );
        if (status == ERROR_MORE_DATA)
        {
            cbData = VReg__Grown(cbData, cbRead);
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            pData->cb = cbRead;
            if (pdwType)
                *pdwType = dwType;
        }
        return status;
    }
}

VREGDEF LSTATUS VReg__EnumKeyOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pName
)
{
    DWORD cchName = VREG__INITIAL_NAME_CHARS;
    for (;;)
    {
        DWORD cchRead = cchName;
        LSTATUS status;
        if (!VReg__WStringResize(pAllocContext, pName, cchName))
            return ERROR_OUTOFMEMORY;

        status = VRegEnumKeyExW(
            pRegistry,
            hKey,
            dwIndex,
            pName->psz,
            &cchRead,
            NULL,
            NULL,
            NULL,
            NULL
        );
        if (status == ERROR_MORE_DATA && cchName < VREG__MAX_CLASS_CHARS)
        {
            cchName = VReg__MinDword(VReg__Grown(cchName, cchRead), VREG__MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VReg__WStringTruncate(pName, cchRead);
        return status;
    }
}

VREGDEF LSTATUS VReg__EnumValueOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pName,
    _Out_opt_ const LPDWORD lpType,
    _Inout_opt_ VReg__Buffer* const pData
)
{
    DWORD cchName = VREG__INITIAL_NAME_CHARS;
    DWORD cbData = 0;
    BOOL isSized = pData == NULL;
    for (;;)
    {
        DWORD cchRead = cchName;
        DWORD cbRead = cbData;
        DWORD dwType = 0;
        LSTATUS status;
        if (!VReg__WStringResize(pAllocContext, pName, cchName))
            return ERROR_OUTOFMEMORY;

        status = VRegEnumValueW(
            pRegistry,
            hKey,
            dwIndex,
            pName->psz,
            &cchRead,
            NULL,
            &dwType,
            (pData && isSized) ? pData->pb : NULL,
            pData ? &cbRead : NULL
        );
        if (status == ERROR_MORE_DATA)
        {
            cchName = VReg__MinDword(VReg__Grown(cchName, cchRead + 1), VREG__MAX_VALUE_NAME_CHARS);
            if (pData)
            {
                cbData = VReg__Grown(cbData, cbRead);
                if (isSized && !VReg__BufferResize(pAllocContext, pData, cbData))
                    return ERROR_OUTOFMEMORY;
            }
            continue;
        }
        if (status == ERROR_SUCCESS && !isSized)
        {
            cbData = cbRead;
            if (!VReg__BufferResize(pAllocContext, pData, cbData))
                return ERROR_OUTOFMEMORY;

            isSized = TRUE;
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            VReg__WStringTruncate(pName, cchRead);
            if (pData)
                pData->cb = cbRead;
            if (lpType)
                *lpType = dwType;
        }
        return status;
    }
}

VREGDEF LSTATUS VReg__QueryInfoKeyOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pClass,
    _Out_opt_ const LPDWORD lpcchClassReported,
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
    DWORD cchClass = VREG__INITIAL_NAME_CHARS;
    for (;;)
    {
        DWORD cchRead = cchClass;
        LSTATUS status;
        if (!VReg__WStringResize(pAllocContext, pClass, cchClass))
            return ERROR_OUTOFMEMORY;

        status = VRegQueryInfoKeyW(
            pRegistry,
            hKey,
            pClass->psz,
            &cchRead,
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
        if ((status == ERROR_MORE_DATA || status == ERROR_INSUFFICIENT_BUFFER) && cchClass < VREG__MAX_CLASS_CHARS)
        {
            cchClass = VReg__MinDword(cchClass * 2, VREG__MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VReg__WStringTruncate(pClass, cchRead);
        else if (lpcchClassReported && cchRead != cchClass)
            *lpcchClassReported = cchRead;
        return status;
    }
}

// ==========================================================================
// Paths
// ==========================================================================

VREGDEF LSTATUS VReg__SplitPath(
    _In_opt_ void* const pAllocContext,
    const VReg__WStringView path,
    _Inout_ VReg__Array* const pComponents
)
{
    SIZE_T start = 0;
    if (path.cch != 0 && path.pch[0] == L'\\')
        return ERROR_BAD_PATHNAME;

    while (start < path.cch)
    {
        SIZE_T end = start;
        VReg__WStringView component;
        while (end < path.cch && path.pch[end] != L'\\')
            ++end;

        component.pch = path.pch + start;
        component.cch = end - start;
        if (component.cch != 0)
        {
            if (component.cch > VREG__MAX_KEY_NAME_CHARS)
                return ERROR_INVALID_PARAMETER;
            if (!VReg__ArrayAppend(pAllocContext, pComponents, sizeof(component), &component))
                return ERROR_OUTOFMEMORY;
        }

        start = end + 1;
    }

    return ERROR_SUCCESS;
}

// ==========================================================================
// The flat functions
// ==========================================================================

VREGDEF LSTATUS WINAPI VRegOpenKeyExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (!pRegistry->lpVtbl->RegOpenKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegOpenKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

VREGDEF LSTATUS WINAPI VRegCreateKeyExW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegCreateKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegCreateKeyExW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegCloseKey(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (!pRegistry->lpVtbl->RegCloseKey)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegCloseKey(
        pRegistry,
        hKey
    );
}

VREGDEF LSTATUS WINAPI VRegQueryValueExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (!pRegistry->lpVtbl->RegQueryValueExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegQueryValueExW(
        pRegistry,
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

VREGDEF LSTATUS WINAPI VRegSetValueExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (!pRegistry->lpVtbl->RegSetValueExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegSetValueExW(
        pRegistry,
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

VREGDEF LSTATUS WINAPI VRegDeleteValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (!pRegistry->lpVtbl->RegDeleteValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegDeleteValueW(
        pRegistry,
        hKey,
        lpValueName
    );
}

VREGDEF LSTATUS WINAPI VRegDeleteKeyExW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (!pRegistry->lpVtbl->RegDeleteKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegDeleteKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

VREGDEF LSTATUS WINAPI VRegDeleteTreeW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    if (!pRegistry->lpVtbl->RegDeleteTreeW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegDeleteTreeW(
        pRegistry,
        hKey,
        lpSubKey
    );
}

VREGDEF LSTATUS WINAPI VRegEnumKeyExW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegEnumKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegEnumKeyExW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegEnumValueW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegEnumValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegEnumValueW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegQueryInfoKeyW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegQueryInfoKeyW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegQueryInfoKeyW(
        pRegistry,
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

VREGDEF LSTATUS WINAPI VRegLoadMUIStringW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    if (!pRegistry->lpVtbl->RegLoadMUIStringW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegLoadMUIStringW(
        pRegistry,
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
// carry it through the function pointer.
#pragma warning(suppress: 6054)
VREGDEF LSTATUS WINAPI VRegGetValueW(
    _In_ VRegRegistry* const pRegistry,
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
    if (!pRegistry->lpVtbl->RegGetValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegGetValueW(
        pRegistry,
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

VREGDEF LSTATUS WINAPI VRegNotifyChangeKeyValue(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    if (!pRegistry->lpVtbl->RegNotifyChangeKeyValue)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegNotifyChangeKeyValue(
        pRegistry,
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

VREGDEF LSTATUS WINAPI VRegFlushKey(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (!pRegistry->lpVtbl->RegFlushKey)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return pRegistry->lpVtbl->RegFlushKey(
        pRegistry,
        hKey
    );
}

VREGDEF void WINAPI VRegDestroyRegistry(_In_opt_ VRegRegistry* const pRegistry)
{
    if (pRegistry && pRegistry->lpVtbl->Destroy)
        pRegistry->lpVtbl->Destroy(pRegistry);
}

// ==========================================================================
// Status
// ==========================================================================

_Post_satisfies_(return != ERROR_SUCCESS)
VREGDEF LSTATUS VReg__FailureStatus(const LSTATUS status)
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    return status;
}

// ==========================================================================
// Value types
// ==========================================================================

VREGDEF BOOL VReg__IsStringType(const DWORD dwType)
{
    return dwType == REG_SZ || dwType == REG_EXPAND_SZ || dwType == REG_MULTI_SZ;
}

// ==========================================================================
// Version
// ==========================================================================

// The CMake build defines both before it includes the header, from what git
// says about the checkout (cmake/VRegVersion.cmake): the commit count as
// the revision, and a preview's distance and commit in the text.
#ifndef VREG__VERSION_REVISION
#define VREG__VERSION_REVISION 0
#endif

#define VREG__STRINGIZE_EXPANDED(x) #x
#define VREG__STRINGIZE(x) VREG__STRINGIZE_EXPANDED(x)

#ifndef VREG__VERSION_TEXT
#define VREG__VERSION_TEXT                                                     \
    VREG__STRINGIZE(VREG_VERSION_MAJOR) "." VREG__STRINGIZE(VREG_VERSION_MINOR) \
        "." VREG__STRINGIZE(VREG_VERSION_PATCH)
#endif

// Major, minor, patch and revision from the high word down, as MAKEDLLVERULL
// packs them.
#define VREG__FILE_VERSION                                                     \
    (((ULONGLONG)VREG_VERSION_MAJOR << 48) | ((ULONGLONG)VREG_VERSION_MINOR << 32) \
        | ((ULONGLONG)VREG_VERSION_PATCH << 16) | (ULONGLONG)VREG__VERSION_REVISION)

VREGDEF ULONGLONG WINAPI VRegGetFileVersion(void)
{
    return VREG__FILE_VERSION;
}

VREGDEF _Ret_z_ LPCSTR WINAPI VRegGetVersion(void)
{
    return VREG__VERSION_TEXT;
}

#undef VREG__STRINGIZE
#undef VREG__STRINGIZE_EXPANDED

// ==========================================================================
// The virtual registry
// ==========================================================================

#include <sddl.h>

typedef struct VReg__MountPoint
{
    HKEY hRoot;

    /// `VReg__WString` components.
    VReg__Array path;

    VRegHive* pHive;

    /// @ref KEY_WOW64_64KEY, @ref KEY_WOW64_32KEY or 0 for both views.
    REGSAM samView;

    /// The native key path as `VReg__WString` components without
    /// `WOW6432Node`; empty when the mount has none.
    VReg__Array nativePath;
} VReg__MountPoint;

/// A mount table is replaced, never changed: a new one copies the mount points
/// of the old (sharing their strings, which only the newest table frees) and
/// adds one. Old tables stay reachable through @ref pPrevious until the
/// registry is destroyed, since a call in flight may still read them.
typedef struct VReg__MountTable VReg__MountTable;

struct VReg__MountTable
{
    /// `VReg__MountPoint`.
    VReg__Array mounts;

    VReg__MountTable* pPrevious;
};

typedef struct VReg__VirtualRegistry
{
    VRegRegistry object;
    void* pAllocContext;

    /// Null for a registry held entirely in memory.
    VRegRegistry* pBase;

    VRegNtQueryKeyFunction pfnNtQueryKey;
    VReg__MountTable* volatile pMounts;
    SRWLOCK mountLock;
} VReg__VirtualRegistry;

/// Where a key handle plus a subkey path lands.
typedef struct VReg__Target
{
    /// The node, when the key is inside a mount.
    VReg__Node* pNode;

    /// The hive of @ref pNode.
    VRegHive* pHive;

    /// The components below @ref pNode still to walk.
    const VReg__WStringView* pRest;
    SIZE_T cRest;

    /// Whether the call goes to the base registry, because the key is
    /// outside every mount.
    BOOL isBase;

    LSTATUS status;

    /// `VReg__WStringView` into the caller's subkey.
    VReg__Array components;

    /// `VReg__WStringView` into @ref nativeText and @ref components: the
    /// native path of a real key plus the subkey.
    VReg__Array full;
    VReg__WString nativeText;
} VReg__Target;

#define VREG__MOUNT_AT(pArray, index) VREG__AT(pArray, VReg__MountPoint, index)
#define VREG__STRING_AT(pArray, index) VREG__AT(pArray, VReg__WString, index)
#define VREG__VIEW_AT(pArray, index) VREG__AT(pArray, VReg__WStringView, index)

/// The view of the process: what a call without a view flag asks for.
#define VREG__PROCESS_VIEW (sizeof(void*) == 8 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY)

#define VREG__KEY_NAME_INFORMATION_CLASS ((ULONG)3)
#define VREG__NT_STATUS_BUFFER_OVERFLOW ((LONG)0x80000005)
#define VREG__NT_STATUS_BUFFER_TOO_SMALL ((LONG)0xC0000023)

#define VREG__KNOWN_NOTIFY_FILTERS \
    (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY \
        | REG_NOTIFY_THREAD_AGNOSTIC)

static VReg__VirtualRegistry* VReg__VirtualOf(_In_ VRegRegistry* const pThis)
{
    return (VReg__VirtualRegistry*)pThis;
}

/// @p psz, or an empty string for null: the hive layer takes no null names.
static LPCWSTR VReg__OrEmpty(_In_opt_z_ const LPCWSTR psz)
{
    return psz ? psz : L"";
}

/// Copies a value into a caller buffer of @p cbBuffer bytes that is known to
/// hold it. Native adds a terminator after a string that lacks one, when it
/// fits, without counting it in the size.
static void VReg__CopyValue(
    _Out_writes_bytes_(cbBuffer) const LPBYTE pbBuffer,
    const DWORD cbBuffer,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbValue) const BYTE* const pValue,
    const DWORD cbValue
)
{
    DWORD cbWhole;
    BOOL isTerminated;
    if (cbValue == 0)
        return;

    // A null value is legal only when it is empty.
    _Analysis_assume_(pValue != NULL);
    CopyMemory(pbBuffer, pValue, cbValue);
    if (!VReg__IsStringType(dwType))
        return;

    cbWhole = cbValue / sizeof(wchar_t) * sizeof(wchar_t);
    if (cbWhole == 0)
        return;

    isTerminated = pValue[cbWhole - 2] == 0 && pValue[cbWhole - 1] == 0;
    if (!isTerminated && cbWhole + sizeof(wchar_t) <= cbBuffer)
        ZeroMemory(pbBuffer + cbWhole, sizeof(wchar_t));
}

static BOOL VReg__IsWow6432Node(const VReg__WStringView name)
{
    return VReg__CompareNames(name, VReg__ViewOf(L"WOW6432Node")) == 0;
}

/// The view a call with @p samDesired is of: the one it names, else the
/// process's.
static REGSAM VReg__CallView(const REGSAM samDesired)
{
    if (samDesired & KEY_WOW64_64KEY)
        return KEY_WOW64_64KEY;
    if (samDesired & KEY_WOW64_32KEY)
        return KEY_WOW64_32KEY;

    return VREG__PROCESS_VIEW;
}

/// Whether a mount of view @p samMount answers a call of view @p samCall.
static BOOL VReg__Admits(const REGSAM samMount, const REGSAM samCall)
{
    return samMount == 0 || samMount == samCall;
}

/// The string form of the current user's SID; empty when it cannot be read.
static LSTATUS VReg__CurrentUserSid(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* pSid)
{
    HANDLE hToken = NULL;
    DWORD cbUser = 0;
    VReg__Buffer user = {NULL, 0, 0};
    LSTATUS status = ERROR_SUCCESS;

    if (!VReg__WStringResize(pAllocContext, pSid, 0))
        return ERROR_OUTOFMEMORY;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return ERROR_SUCCESS;

    GetTokenInformation(hToken, TokenUser, NULL, 0, &cbUser);
    if (cbUser != 0)
    {
        if (!VReg__BufferResize(pAllocContext, &user, cbUser))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (GetTokenInformation(hToken, TokenUser, user.pb, cbUser, &cbUser))
        {
            LPWSTR pszSid = NULL;
            if (ConvertSidToStringSidW(((const TOKEN_USER*)user.pb)->User.Sid, &pszSid))
            {
                if (!VReg__WStringAssign(pAllocContext, pSid, pszSid, VReg__ViewOf(pszSid).cch))
                    status = ERROR_OUTOFMEMORY;
                LocalFree(pszSid);
            }
        }
    }

    VReg__BufferFree(pAllocContext, &user);
    CloseHandle(hToken);
    return status;
}

/// Appends the native path of a predefined root to @p pPath as components;
/// nothing for a root with none.
static LSTATUS VReg__NativeRootPath(
    _In_opt_ void* const pAllocContext,
    const HKEY hRoot,
    _Inout_ VReg__Array* const pPath
)
{
    const wchar_t* pszSecond = NULL;
    VReg__WString sid = {NULL, 0, 0};
    LSTATUS status = ERROR_SUCCESS;

    if (hRoot == HKEY_LOCAL_MACHINE)
    {
        pszSecond = L"MACHINE";
    }
    else if (hRoot == HKEY_USERS)
    {
        pszSecond = L"USER";
    }
    else if (hRoot == HKEY_CURRENT_USER)
    {
        status = VReg__CurrentUserSid(pAllocContext, &sid);
        if (status == ERROR_SUCCESS && sid.cch != 0)
            pszSecond = L"USER";
    }

    if (status == ERROR_SUCCESS && pszSecond)
    {
        if (
            !VReg__WStringArrayAppend(pAllocContext, pPath, VReg__ViewOf(L"REGISTRY"))
            || !VReg__WStringArrayAppend(pAllocContext, pPath, VReg__ViewOf(pszSecond))
            || (sid.cch != 0 && !VReg__WStringArrayAppend(pAllocContext, pPath, VReg__WStringViewOf(&sid)))
        )
            status = ERROR_OUTOFMEMORY;
    }

    VReg__WStringFree(pAllocContext, &sid);
    return status;
}

/// Whether @p pFull, with `WOW6432Node` components skipped, starts with
/// @p pMountPath; @p piRest then indexes the first component past the match
/// and @p pIsWow says a `WOW6432Node` was skipped.
static BOOL VReg__MatchesNative(
    _In_ const VReg__Array* const pMountPath,
    _In_ const VReg__Array* const pFull,
    _Out_ BOOL* const pIsWow,
    _Out_ SIZE_T* const piRest
)
{
    SIZE_T iMount = 0;
    SIZE_T i = 0;
    *pIsWow = FALSE;
    *piRest = 0;
    for (; i < pFull->count && iMount < pMountPath->count; ++i)
    {
        const VReg__WStringView component = VREG__VIEW_AT(pFull, i);
        if (VReg__IsWow6432Node(component))
        {
            *pIsWow = TRUE;
            continue;
        }
        if (VReg__CompareNames(component, VReg__WStringViewOf(&VREG__STRING_AT(pMountPath, iMount))) != 0)
            return FALSE;

        ++iMount;
    }

    *piRest = i;
    return iMount == pMountPath->count;
}

// --- The mount table ---

/// The current table, or null while nothing is mounted. Lock-free.
static _Ret_maybenull_ const VReg__MountTable* VReg__MountsOf(_In_ const VReg__VirtualRegistry* const pThis)
{
    return (const VReg__MountTable*)ReadPointerAcquire((PVOID volatile*)&pThis->pMounts);
}

/// A mount of @p pHive here; null when it is not mounted.
static _Ret_maybenull_ const VReg__MountPoint* VReg__MountOf(
    _In_ const VReg__VirtualRegistry* const pThis,
    _In_ const VRegHive* const pHive
)
{
    const VReg__MountTable* const pTable = VReg__MountsOf(pThis);
    SIZE_T i;
    if (!pTable)
        return NULL;

    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
        if (pMount->pHive == pHive)
            return pMount;
    }

    return NULL;
}

static void VReg__MountPointFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__MountPoint* const pMount)
{
    VReg__WStringArrayFree(pAllocContext, &pMount->path);
    VReg__WStringArrayFree(pAllocContext, &pMount->nativePath);
}

/// Whether the mount @p pMount is of @p hRoot at the path @p pPath, by the
/// registry's own name comparison.
static BOOL VReg__MountIsAt(
    _In_ const VReg__MountPoint* const pMount,
    const HKEY hRoot,
    _In_ const VReg__Array* const pPath
)
{
    SIZE_T i;
    if (pMount->hRoot != hRoot || pMount->path.count != pPath->count)
        return FALSE;

    for (i = 0; i < pPath->count; ++i)
    {
        const VReg__WStringView left = VReg__WStringViewOf(&VREG__STRING_AT(&pMount->path, i));
        const VReg__WStringView right = VReg__WStringViewOf(&VREG__STRING_AT(pPath, i));
        if (VReg__CompareNames(left, right) != 0)
            return FALSE;
    }

    return TRUE;
}

/// Publishes a table holding the current mounts plus @p pMount, whose strings
/// it then owns. Under the mount lock.
static LSTATUS VReg__PublishMount(_In_ VReg__VirtualRegistry* const pThis, _In_ const VReg__MountPoint* const pMount)
{
    void* const pAllocContext = pThis->pAllocContext;
    VReg__MountTable* const pOld = pThis->pMounts;
    VReg__MountTable* const pNew = (VReg__MountTable*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__MountTable));
    SIZE_T i;
    if (!pNew)
        return ERROR_OUTOFMEMORY;

    for (i = 0; pOld && i < pOld->mounts.count; ++i)
    {
        const VReg__MountPoint* const pExisting = &VREG__MOUNT_AT(&pOld->mounts, i);
        if (VReg__MountIsAt(pExisting, pMount->hRoot, &pMount->path))
        {
            VReg__ArrayFree(pAllocContext, &pNew->mounts);
            VReg__Free(pAllocContext, pNew);
            return ERROR_ALREADY_EXISTS;
        }
        if (!VReg__ArrayAppend(pAllocContext, &pNew->mounts, sizeof(*pExisting), pExisting))
        {
            VReg__ArrayFree(pAllocContext, &pNew->mounts);
            VReg__Free(pAllocContext, pNew);
            return ERROR_OUTOFMEMORY;
        }
    }

    if (!VReg__ArrayAppend(pAllocContext, &pNew->mounts, sizeof(*pMount), pMount))
    {
        VReg__ArrayFree(pAllocContext, &pNew->mounts);
        VReg__Free(pAllocContext, pNew);
        return ERROR_OUTOFMEMORY;
    }

    pNew->pPrevious = pOld;
    WritePointerRelease((PVOID volatile*)&pThis->pMounts, pNew);
    return ERROR_SUCCESS;
}

// --- Resolution ---

static void VReg__TargetFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Target* const pTarget)
{
    VReg__ArrayFree(pAllocContext, &pTarget->components);
    VReg__ArrayFree(pAllocContext, &pTarget->full);
    VReg__WStringFree(pAllocContext, &pTarget->nativeText);
}

/// Looks for a mount a real key plus the subkey components lands in, by
/// native path; fills @p pTarget and sets @p pIsResolved when it does.
static LSTATUS VReg__ResolveNative(
    _In_ const VReg__VirtualRegistry* const pThis,
    const HKEY hKey,
    const REGSAM samDesired,
    _Inout_ VReg__Target* const pTarget,
    _Out_ BOOL* const pIsResolved
)
{
    void* const pAllocContext = pThis->pAllocContext;
    const VReg__MountTable* const pTable = VReg__MountsOf(pThis);
    BOOL isGated = FALSE;
    VReg__Buffer buffer = {NULL, 0, 0};
    ULONG cbName = 0;
    const VReg__MountPoint* pBest = NULL;
    SIZE_T iBestRest = 0;
    REGSAM samCall;
    SIZE_T i;
    SIZE_T start;
    LSTATUS status = ERROR_SUCCESS;

    *pIsResolved = FALSE;
    for (i = 0; pTable && !isGated && i < pTable->mounts.count; ++i)
    {
        const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
        SIZE_T j;
        if (pMount->path.count == 0)
            continue;

        for (j = 0; !isGated && j < pTarget->components.count; ++j)
        {
            const VReg__WStringView last = VReg__WStringViewOf(&VREG__STRING_AT(&pMount->path, pMount->path.count - 1));
            isGated = VReg__CompareNames(VREG__VIEW_AT(&pTarget->components, j), last) == 0;
        }
    }
    if (!isGated || !pThis->pfnNtQueryKey || VRegIsVirtualHKey(hKey))
        return ERROR_SUCCESS;

    // The name is a KEY_NAME_INFORMATION: a byte count, then the characters.
    if (!VReg__BufferResize(pAllocContext, &buffer, 1024))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        ULONG cbResult = 0;
        const LONG ntStatus = pThis->pfnNtQueryKey(
            hKey,
            VREG__KEY_NAME_INFORMATION_CLASS,
            buffer.pb,
            (ULONG)buffer.cb,
            &cbResult
        );
        if (ntStatus >= 0)
            break;
        if (
            (ntStatus != VREG__NT_STATUS_BUFFER_OVERFLOW && ntStatus != VREG__NT_STATUS_BUFFER_TOO_SMALL)
            || cbResult <= buffer.cb
        )
            goto done;
        if (!VReg__BufferResize(pAllocContext, &buffer, cbResult))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    // A resize to a nonzero size that succeeded allocated the storage.
    _Analysis_assume_(buffer.pb != NULL);
    CopyMemory(&cbName, buffer.pb, sizeof(cbName));
    if (cbName > buffer.cb - sizeof(ULONG))
        goto done;
    if (
        !VReg__WStringAssign(
            pAllocContext,
            &pTarget->nativeText,
            (const wchar_t*)(buffer.pb + sizeof(ULONG)),
            cbName / sizeof(wchar_t)
        )
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    for (start = 0; start < pTarget->nativeText.cch;)
    {
        SIZE_T end = start;
        while (end < pTarget->nativeText.cch && pTarget->nativeText.psz[end] != L'\\')
            ++end;
        if (end > start)
        {
            VReg__WStringView component;
            component.pch = pTarget->nativeText.psz + start;
            component.cch = end - start;
            if (!VReg__ArrayAppend(pAllocContext, &pTarget->full, sizeof(component), &component))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
        }

        start = end + 1;
    }

    for (i = 0; i < pTarget->components.count; ++i)
    {
        const VReg__WStringView* const pComponent = &VREG__VIEW_AT(&pTarget->components, i);
        if (!VReg__ArrayAppend(pAllocContext, &pTarget->full, sizeof(*pComponent), pComponent))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    samCall = VReg__CallView(samDesired);
    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
        BOOL isWow = FALSE;
        SIZE_T iRest = 0;
        if (
            pMount->nativePath.count == 0
            || !VReg__MatchesNative(&pMount->nativePath, &pTarget->full, &isWow, &iRest)
            || !VReg__Admits(pMount->samView, isWow ? KEY_WOW64_32KEY : samCall)
        )
            continue;

        if (!pBest || pMount->nativePath.count > pBest->nativePath.count)
        {
            pBest = pMount;
            iBestRest = iRest;
        }
    }

    if (pBest)
    {
        pTarget->pNode = VReg__HiveRoot(pBest->pHive);
        pTarget->pHive = pBest->pHive;
        pTarget->pRest = (const VReg__WStringView*)pTarget->full.p + iBestRest;
        pTarget->cRest = pTarget->full.count - iBestRest;
        *pIsResolved = TRUE;
    }

done:
    VReg__BufferFree(pAllocContext, &buffer);
    return status;
}

/// Resolves @p hKey and @p subKey (empty for the key itself). @p samDesired
/// supplies the registry view of the call. Answers a status only for a
/// failure of its own (memory); what the call should answer is in the target.
static LSTATUS VReg__Resolve(
    _In_ const VReg__VirtualRegistry* const pThis,
    const HKEY hKey,
    const VReg__WStringView subKey,
    const REGSAM samDesired,
    _Out_ VReg__Target* const pTarget
)
{
    void* const pAllocContext = pThis->pAllocContext;
    LSTATUS splitStatus;
    BOOL isSplit;
    BOOL isResolved = FALSE;
    VReg__Node* pNode;

    ZeroMemory(pTarget, sizeof(*pTarget));
    splitStatus = VReg__SplitPath(pAllocContext, subKey, &pTarget->components);
    if (splitStatus == ERROR_OUTOFMEMORY)
        return splitStatus;

    isSplit = splitStatus == ERROR_SUCCESS;
    pNode = VReg__NodeOf(hKey);
    if (pNode)
    {
        const VReg__MountPoint* const pMount = VReg__MountOf(pThis, VReg__NodeHive(pNode));
        if (pMount)
        {
            if (isSplit)
            {
                pTarget->pNode = pNode;
                pTarget->pHive = pMount->pHive;
                pTarget->pRest = (const VReg__WStringView*)pTarget->components.p;
                pTarget->cRest = pTarget->components.count;
            }
            else
            {
                pTarget->status = splitStatus;
            }
            return ERROR_SUCCESS;
        }
    }
    else if (VReg__HandleSpaceOwns(VReg__ProcessHandleSpace(), hKey))
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (VRegIsPredefinedHKey(hKey))
    {
        const REGSAM samCall = VReg__CallView(samDesired);
        const VReg__MountTable* const pTable = VReg__MountsOf(pThis);
        const VReg__MountPoint* pBest = NULL;
        SIZE_T i;
        for (i = 0; pTable && isSplit && i < pTable->mounts.count; ++i)
        {
            const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
            SIZE_T j;
            BOOL isPrefix = TRUE;
            if (
                pMount->hRoot != hKey
                || pMount->path.count > pTarget->components.count
                || !VReg__Admits(pMount->samView, samCall)
            )
                continue;

            for (j = 0; isPrefix && j < pMount->path.count; ++j)
            {
                const VReg__WStringView mountName = VReg__WStringViewOf(&VREG__STRING_AT(&pMount->path, j));
                isPrefix = VReg__CompareNames(mountName, VREG__VIEW_AT(&pTarget->components, j)) == 0;
            }
            if (!isPrefix)
                continue;

            if (!pBest || pMount->path.count > pBest->path.count)
                pBest = pMount;
        }

        if (pBest)
        {
            pTarget->pNode = VReg__HiveRoot(pBest->pHive);
            pTarget->pHive = pBest->pHive;
            pTarget->pRest = (const VReg__WStringView*)pTarget->components.p + pBest->path.count;
            pTarget->cRest = pTarget->components.count - pBest->path.count;
            return ERROR_SUCCESS;
        }

        if (pThis->pBase)
            pTarget->isBase = TRUE;
        else
            pTarget->status = isSplit ? ERROR_FILE_NOT_FOUND : splitStatus;

        return ERROR_SUCCESS;
    }

    if (!pThis->pBase)
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (isSplit)
    {
        const LSTATUS status = VReg__ResolveNative(pThis, hKey, samDesired, pTarget, &isResolved);
        if (status != ERROR_SUCCESS)
            return status;
    }

    if (!isResolved)
        pTarget->isBase = TRUE;

    return ERROR_SUCCESS;
}

// --- The slots ---

static LSTATUS WINAPI VReg__VirtualRegOpenKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__OpenResult result;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegOpenKeyExW(
            pRegistry->pBase,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
        goto done;
    }

    if (!phkResult)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    *phkResult = NULL;
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (!samDesired)
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VReg__HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, samDesired, &result);
    if (status == ERROR_SUCCESS)
        *phkResult = result.hLower ? result.hLower : VReg__NodeHandle(result.pNode);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegCreateKeyExW(
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
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__OpenResult result;
    DWORD dwDisposition = REG_OPENED_EXISTING_KEY;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegCreateKeyExW(
            pRegistry->pBase,
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
        goto done;
    }

    if (!phkResult)
    {
        status = ERROR_BADKEY;
        goto done;
    }

    // Native leaves the result alone for these two and nulls it for every other
    // failure.
    if (Reserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (target.status == ERROR_INVALID_HANDLE)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    *phkResult = NULL;
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (dwOptions & ~(DWORD)0x1F)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    status = VReg__HiveCreateKey(
        target.pHive,
        target.pNode,
        target.pRest,
        target.cRest,
        VReg__OrEmpty(lpClass),
        dwOptions,
        samDesired,
        &result,
        &dwDisposition
    );
    if (status != ERROR_SUCCESS)
    {
        status = VReg__FailureStatus(status);
        goto done;
    }

    *phkResult = result.hLower ? result.hLower : VReg__NodeHandle(result.pNode);
    if (lpdwDisposition)
        *lpdwDisposition = dwDisposition;

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegCloseKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    if (VReg__NodeOf(hKey) || VRegIsPredefinedHKey(hKey))
        return ERROR_SUCCESS;
    if (VReg__HandleSpaceOwns(VReg__ProcessHandleSpace(), hKey) || !pRegistry->pBase)
        return ERROR_INVALID_HANDLE;

    return VRegCloseKey(
        pRegistry->pBase,
        hKey
    );
}

static LSTATUS WINAPI VReg__VirtualRegQueryValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__Buffer value = {NULL, 0, 0};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    DWORD cbBuffer;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegQueryValueExW(
            pRegistry->pBase,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    if ((lpData && !lpcbData) || lpReserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VReg__HiveQueryValue(
        target.pHive,
        target.pNode,
        VReg__OrEmpty(lpValueName),
        &dwType,
        &cbValue,
        lpData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpType)
        *lpType = dwType;
    if (!lpData)
    {
        if (lpcbData)
            *lpcbData = cbValue;
        goto done;
    }

    cbBuffer = *lpcbData;
    *lpcbData = cbValue;
    if (cbValue > cbBuffer)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    VReg__CopyValue(lpData, cbBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
        VReg__BufferFree(VReg__HiveAllocContext(target.pHive), &value);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegSetValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegSetValueExW(
            pRegistry->pBase,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
        goto done;
    }

    if (Reserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (!lpData && cbData)
    {
        status = ERROR_NOACCESS;
        goto done;
    }

    status = VReg__HiveSetValue(
        target.pHive,
        target.pNode,
        VReg__OrEmpty(lpValueName),
        dwType,
        lpData,
        lpData ? cbData : 0
    );

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegDeleteValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteValueW(
            pRegistry->pBase,
            hKey,
            lpValueName
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveDeleteValue(target.pHive, target.pNode, VReg__OrEmpty(lpValueName));

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegDeleteKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteKeyExW(
            pRegistry->pBase,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
        goto done;
    }

    if (Reserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegDeleteTreeW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__OpenResult result;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteTreeW(
            pRegistry->pBase,
            hKey,
            lpSubKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_ALL_ACCESS, &result);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only a frozen hive hands out lower keys, and it refuses every write.
    if (result.hLower)
    {
        VRegRegistry* const pLower = VReg__HiveLower(target.pHive);
        // A lower handle exists only where there is a lower registry.
        _Analysis_assume_(pLower);
        VRegCloseKey(
            pLower,
            result.hLower
        );
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VReg__HiveDeleteTree(target.pHive, result.pNode);
    if (status != ERROR_SUCCESS || target.cRest == 0)
        goto done;

    status = VReg__HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegEnumKeyExW(
    _In_ VRegRegistry* const pThis,
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
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__WString name = {NULL, 0, 0};
    VReg__WString className = {NULL, 0, 0};
    FILETIME lastWrite;
    DWORD cchClassBuffer;
    BOOL isNameFit;
    BOOL isClassFit;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    ZeroMemory(&lastWrite, sizeof(lastWrite));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegEnumKeyExW(
            pRegistry->pBase,
            hKey,
            dwIndex,
            lpName,
            lpcchName,
            lpReserved,
            lpClass,
            lpcchClass,
            lpftLastWriteTime
        );
        goto done;
    }

    if (lpReserved || !lpcchName || (lpClass && !lpcchClass))
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    cchClassBuffer = lpClass ? *lpcchClass : 0;
    if (lpcchClass)
        *lpcchClass = 0;

    status = VReg__HiveEnumKey(target.pHive, target.pNode, dwIndex, &name, &className, &lastWrite);
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpftLastWriteTime)
        *lpftLastWriteTime = lastWrite;

    isNameFit = lpName && name.cch < *lpcchName;
    isClassFit = !lpClass || className.cch < cchClassBuffer;
    if (!isNameFit || !isClassFit)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    CopyMemory(lpName, VReg__WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
    *lpcchName = (DWORD)name.cch;
    if (lpClass)
    {
        CopyMemory(lpClass, VReg__WStringText(&className), (className.cch + 1) * sizeof(wchar_t));
        *lpcchClass = (DWORD)className.cch;
    }

done:
    if (target.pHive)
    {
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &name);
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &className);
    }
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegEnumValueW(
    _In_ VRegRegistry* const pThis,
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
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__WString name = {NULL, 0, 0};
    VReg__Buffer value = {NULL, 0, 0};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    BOOL isDataFit = TRUE;
    DWORD cbDataBuffer = 0;
    BOOL isNameFit;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegEnumValueW(
            pRegistry->pBase,
            hKey,
            dwIndex,
            lpValueName,
            lpcchValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    if (lpReserved || !lpValueName || !lpcchValueName || (lpData && !lpcbData))
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VReg__HiveEnumValue(
        target.pHive,
        target.pNode,
        dwIndex,
        &name,
        &dwType,
        &cbValue,
        lpData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpType)
        *lpType = dwType;

    if (lpcbData)
    {
        cbDataBuffer = *lpcbData;
        isDataFit = !lpData || cbValue <= cbDataBuffer;
        *lpcbData = cbValue;
    }

    isNameFit = name.cch < *lpcchValueName;
    if (isNameFit)
    {
        CopyMemory(lpValueName, VReg__WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
        *lpcchValueName = (DWORD)name.cch;
    }

    if (!isNameFit || !isDataFit)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    if (lpData)
        VReg__CopyValue(lpData, cbDataBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
    {
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &name);
        VReg__BufferFree(VReg__HiveAllocContext(target.pHive), &value);
    }
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

// Native answers success without writing lpClass when *lpcchClass is zero,
// which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
static LSTATUS WINAPI VReg__VirtualRegQueryInfoKeyW(
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
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__KeyInfo info;
    DWORD cchClass;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    ZeroMemory(&info, sizeof(info));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegQueryInfoKeyW(
            pRegistry->pBase,
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
        goto done;
    }

    if (lpReserved || (lpClass && !lpcchClass))
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveQueryInfo(target.pHive, target.pNode, &info);
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpcSubKeys)
        *lpcSubKeys = info.cSubKeys;
    if (lpcbMaxSubKeyLen)
        *lpcbMaxSubKeyLen = info.cchMaxSubKey;
    if (lpcbMaxClassLen)
        *lpcbMaxClassLen = info.cchMaxClass;
    if (lpcValues)
        *lpcValues = info.cValues;
    if (lpcbMaxValueNameLen)
        *lpcbMaxValueNameLen = info.cchMaxValueName;
    if (lpcbMaxValueLen)
        *lpcbMaxValueLen = info.cbMaxValue;
    if (lpcbSecurityDescriptor)
        *lpcbSecurityDescriptor = 0;
    if (lpftLastWriteTime)
        *lpftLastWriteTime = info.lastWrite;

    cchClass = (DWORD)info.className.cch;
    if (lpClass && *lpcchClass != 0)
    {
        if (*lpcchClass < cchClass)
        {
            *lpcchClass = cchClass;
            status = ERROR_MORE_DATA;
            goto done;
        }
        if (*lpcchClass == cchClass)
        {
            *lpcchClass = cchClass - 1;
            status = ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }

        CopyMemory(lpClass, VReg__WStringText(&info.className), (cchClass + 1) * sizeof(wchar_t));
    }

    if (lpClass && lpcchClass && (*lpcchClass != 0 || lpcbMaxClassLen))
        *lpcchClass = cchClass;
    else if (!lpClass && lpcchClass)
        *lpcchClass = 0;

done:
    if (target.pHive)
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &info.className);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegLoadMUIStringW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    HKEY hLower = NULL;
    VReg__WString text = {NULL, 0, 0};
    DWORD cbText;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegLoadMUIStringW(
            pRegistry->pBase,
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
        goto done;
    }

    if (!hKey)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (
        (Flags & ~(DWORD)REG_MUI_STRING_TRUNCATE)
        || ((Flags & REG_MUI_STRING_TRUNCATE) != 0) != (pcbData == NULL)
        || cbOutBuf % sizeof(wchar_t) != 0
        || (!pszOutBuf && cbOutBuf != 0)
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    status = VReg__HiveOpenLowerValueKey(target.pHive, target.pNode, VReg__OrEmpty(pszValue), &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    // A value from below is resolved there, as the system would. One the hive
    // holds would need a resource module to resolve, unless it is marked as
    // the text itself.
    if (hLower)
    {
        VRegRegistry* const pLower = VReg__HiveLower(target.pHive);
        // A lower handle exists only where there is a lower registry.
        _Analysis_assume_(pLower);
        status = VRegLoadMUIStringW(
            pLower,
            hLower,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
        VRegCloseKey(
            pLower,
            hLower
        );
        goto done;
    }

    status = VReg__HiveQueryMuiText(target.pHive, target.pNode, VReg__OrEmpty(pszValue), &text);
    if (status != ERROR_SUCCESS)
        goto done;

    // Truncation fills whatever buffer there is, terminated; otherwise the
    // size is reported and the text copied only when all of it fits.
    cbText = (DWORD)((text.cch + 1) * sizeof(wchar_t));
    if (!pcbData)
    {
        SIZE_T cch;
        if (!pszOutBuf)
        {
            status = ERROR_MORE_DATA;
            goto done;
        }
        if (cbOutBuf == 0)
            goto done;

        cch = cbOutBuf / sizeof(wchar_t) - 1;
        if (cch > text.cch)
            cch = text.cch;
        CopyMemory(pszOutBuf, VReg__WStringText(&text), cch * sizeof(wchar_t));
        pszOutBuf[cch] = L'\0';
        goto done;
    }

    *pcbData = cbText;
    if (!pszOutBuf || cbOutBuf < cbText)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    CopyMemory(pszOutBuf, VReg__WStringText(&text), cbText);

done:
    if (target.pHive)
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &text);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

/// Answers the failure @p status after clearing the caller's buffer when the
/// call asked for that on failure.
_Post_satisfies_(return != ERROR_SUCCESS)
static LSTATUS VReg__FailGetValue(
    _Out_writes_bytes_opt_(cbBuffer) const PVOID pvData,
    const DWORD cbBuffer,
    const DWORD dwFlags,
    const LSTATUS status
)
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    if (pvData && (dwFlags & RRF_ZEROONFAILURE))
        ZeroMemory(pvData, cbBuffer);
    return status;
}

/// Whether the character at @p iwch of @p pBuffer is a terminator.
static BOOL VReg__IsNulAt(_In_ const VReg__Buffer* const pBuffer, const SIZE_T iwch)
{
    wchar_t ch;
    CopyMemory(&ch, pBuffer->pb + iwch * sizeof(wchar_t), sizeof(ch));
    return ch == L'\0';
}

// The SDK's _Post_z_ holds on every return, truncated reads and failures
// included; the analyzer cannot see the terminators written at run time.
#pragma warning(suppress: 6054)
static LSTATUS WINAPI VReg__VirtualRegGetValueW(
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
)
{
    static const DWORD TYPE_FLAGS[] = {
        RRF_RT_REG_NONE,
        RRF_RT_REG_SZ,
        RRF_RT_REG_EXPAND_SZ,
        RRF_RT_REG_BINARY,
        RRF_RT_REG_DWORD,
        0,
        0,
        RRF_RT_REG_MULTI_SZ,
        0,
        0,
        0,
        RRF_RT_REG_QWORD,
    };

    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    void* pHiveContext = NULL;
    VReg__Target target;
    VReg__OpenResult result;
    VReg__Node* pNode;
    DWORD cbBuffer;
    DWORD dwType = 0;
    DWORD cbQueried = 0;
    BOOL isString;

    // Native copies the stored bytes first, then the string with the
    // terminators it lacks, then the expansion, each over the previous one and
    // only if it fits.
    VReg__Buffer stages[3] = {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}};
    DWORD cbReported;
    DWORD dwTypeFlag;
    BOOL isAllowed;
    const VReg__Buffer* pFinal;
    BOOL isFit;
    BOOL isSizeMismatch;
    SIZE_T i;
    LSTATUS status = VReg__Resolve(
        pRegistry,
        hkey,
        VReg__ViewOf(lpSubKey),
        (dwFlags & VREG__RRF_WOW6464_SUBKEY ? KEY_WOW64_64KEY : 0)
        | (dwFlags & VREG__RRF_WOW6432_SUBKEY ? KEY_WOW64_32KEY : 0),
        &target
    );
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegGetValueW(
            pRegistry->pBase,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
        goto done;
    }

    if (
        !hkey
        || (pvData && !pcbData)
        || !(dwFlags & RRF_RT_ANY)
        || (dwFlags & VREG__RRF_WOW_SUBKEYS) == VREG__RRF_WOW_SUBKEYS
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    cbBuffer = pcbData ? *pcbData : 0;
    if (!target.pNode)
    {
        status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, target.status);
        goto done;
    }

    pHiveContext = VReg__HiveAllocContext(target.pHive);
    pNode = target.pNode;
    if (target.cRest != 0)
    {
        status = VReg__HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_QUERY_VALUE, &result);
        if (status != ERROR_SUCCESS)
        {
            status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, status);
            goto done;
        }

        if (result.hLower)
        {
            VRegRegistry* const pLower = VReg__HiveLower(target.pHive);
            // A lower handle exists only where there is a lower registry.
            _Analysis_assume_(pLower);
            status = VRegGetValueW(
                pLower,
                result.hLower,
                NULL,
                lpValue,
                dwFlags,
                pdwType,
                pvData,
                pcbData
            );
            VRegCloseKey(
                pLower,
                result.hLower
            );
            goto done;
        }

        pNode = result.pNode;
    }

    if (pdwType)
        *pdwType = 0;
    if (pcbData && !pvData)
        *pcbData = 0;

    status = VReg__HiveQueryValue(target.pHive, pNode, VReg__OrEmpty(lpValue), &dwType, &cbQueried, &stages[0]);
    if (status != ERROR_SUCCESS)
    {
        status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, status);
        goto done;
    }

    isString = VReg__IsStringType(dwType);
    cbReported = (DWORD)stages[0].cb;
    if (isString)
    {
        const SIZE_T cwch = stages[0].cb / sizeof(wchar_t);
        SIZE_T cwchAdded = 0;
        static const wchar_t ZEROS[2] = {L'\0', L'\0'};
        if (cwch == 0 || !VReg__IsNulAt(&stages[0], cwch - 1))
            cwchAdded = dwType == REG_MULTI_SZ ? 2 : 1;
        else if (dwType == REG_MULTI_SZ && (cwch == 1 || !VReg__IsNulAt(&stages[0], cwch - 2)))
            cwchAdded = 1;

        if (
            !VReg__BufferAssign(pHiveContext, &stages[1], stages[0].pb, cwch * sizeof(wchar_t))
            || !VReg__BufferAppend(pHiveContext, &stages[1], ZEROS, cwchAdded * sizeof(wchar_t))
        )
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cbReported = (DWORD)stages[1].cb;
        if (dwType == REG_EXPAND_SZ && !(dwFlags & RRF_NOEXPAND))
        {
            const wchar_t* const pszRaw = (const wchar_t*)stages[1].pb;
            DWORD cwchExpanded;

            // The text was terminated above, so the stage holds at least that.
            _Analysis_assume_(pszRaw != NULL);
            cwchExpanded = ExpandEnvironmentStringsW(pszRaw, NULL, 0);
            if (!VReg__BufferResize(pHiveContext, &stages[2], (SIZE_T)cwchExpanded * sizeof(wchar_t)))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
            if (cwchExpanded != 0)
                ExpandEnvironmentStringsW(pszRaw, (wchar_t*)stages[2].pb, cwchExpanded);

            cbReported = VReg__MaxDword(cbReported, (DWORD)stages[2].cb);
            dwType = REG_SZ;
        }
    }

    dwTypeFlag = dwType < sizeof(TYPE_FLAGS) / sizeof(TYPE_FLAGS[0]) ? TYPE_FLAGS[dwType] : 0;
    isAllowed = dwTypeFlag ? (dwFlags & dwTypeFlag) != 0 : (dwFlags & RRF_RT_ANY) == RRF_RT_ANY;

    pFinal = stages[2].cb != 0 ? &stages[2] : (stages[1].cb != 0 ? &stages[1] : &stages[0]);
    isFit = pvData && pFinal->cb <= cbBuffer;
    if (pvData)
    {
        for (i = 0; i < 3; ++i)
        {
            if (stages[i].cb != 0 && stages[i].cb <= cbBuffer)
                CopyMemory(pvData, stages[i].pb, stages[i].cb);
        }

        // An expansion that does not fit leaves the unexpanded text with its
        // first character cleared.
        if (stages[2].cb != 0 && !isFit && stages[1].cb <= cbBuffer)
            ZeroMemory(pvData, sizeof(wchar_t));
    }

    if (pdwType)
        *pdwType = dwType;
    if (pcbData)
    {
        // A string that cannot be returned reports room for the terminators it
        // may lack.
        const DWORD cbTerminators = dwType == REG_MULTI_SZ ? 4 : (isString ? 2 : 0);
        const DWORD cbReserved = (DWORD)stages[0].cb + cbTerminators;
        if (stages[2].cb != 0)
            *pcbData = cbReported;
        else
            *pcbData = isFit ? cbReported : VReg__MaxDword(cbReported, cbReserved);
    }

    // Binary data passes as a DWORD only when it is empty or has that size.
    // Agent's choice: native was measured with sizes 0 and 5 only; rejecting
    // every other size is a guess for 1 to 3 and 6 or more.
    isSizeMismatch =
        dwType == REG_BINARY
        && (dwFlags & RRF_RT_ANY) == RRF_RT_DWORD
        && stages[0].cb != 0
        && stages[0].cb != sizeof(DWORD);

    if (!isAllowed)
        status = ERROR_UNSUPPORTED_TYPE;
    else if (isSizeMismatch)
        status = ERROR_DATATYPE_MISMATCH;
    else if (pvData && !isFit)
        status = ERROR_MORE_DATA;
    if (status != ERROR_SUCCESS)
        status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, status);

done:
    for (i = 0; i < 3; ++i)
        VReg__BufferFree(pHiveContext, &stages[i]);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegNotifyChangeKeyValue(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    HANDLE hOwn;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegNotifyChangeKeyValue(
            pRegistry->pBase,
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (
        (dwNotifyFilter & ~(DWORD)VREG__KNOWN_NOTIFY_FILTERS)
        || (fAsynchronous && !hEvent)
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    if (fAsynchronous)
    {
        status = VReg__HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hEvent);
        goto done;
    }

    // A synchronous call waits for the change like the native one, on a private
    // event, so a watcher thread written for the native registry behaves the
    // same (and, as there, never returns on a key nothing changes).
    hOwn = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hOwn)
    {
        status = GetLastError();
        goto done;
    }

    status = VReg__HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hOwn);
    if (status == ERROR_SUCCESS)
        WaitForSingleObject(hOwn, INFINITE);

    CloseHandle(hOwn);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegFlushKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegFlushKey(
            pRegistry->pBase,
            hKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveCheckKey(target.pHive, target.pNode);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

// --- Creation and destruction ---

static void WINAPI VReg__VirtualDestroy(_In_ VRegRegistry* const pThis)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__MountTable* pTable = pRegistry->pMounts;
    BOOL isNewest = TRUE;
    while (pTable)
    {
        VReg__MountTable* const pPrevious = pTable->pPrevious;
        if (isNewest)
        {
            SIZE_T i;
            for (i = 0; i < pTable->mounts.count; ++i)
                VReg__MountPointFree(pAllocContext, &VREG__MOUNT_AT(&pTable->mounts, i));
        }

        VReg__ArrayFree(pAllocContext, &pTable->mounts);
        VReg__Free(pAllocContext, pTable);
        pTable = pPrevious;
        isNewest = FALSE;
    }

    VReg__Free(pAllocContext, pRegistry);
}

static const VRegRegistryVtbl VReg__VirtualRegistryVtbl = {
    .Destroy = &VReg__VirtualDestroy,
    .RegOpenKeyExW = &VReg__VirtualRegOpenKeyExW,
    .RegCreateKeyExW = &VReg__VirtualRegCreateKeyExW,
    .RegCloseKey = &VReg__VirtualRegCloseKey,
    .RegQueryValueExW = &VReg__VirtualRegQueryValueExW,
    .RegSetValueExW = &VReg__VirtualRegSetValueExW,
    .RegDeleteValueW = &VReg__VirtualRegDeleteValueW,
    .RegDeleteKeyExW = &VReg__VirtualRegDeleteKeyExW,
    .RegDeleteTreeW = &VReg__VirtualRegDeleteTreeW,
    .RegEnumKeyExW = &VReg__VirtualRegEnumKeyExW,
    .RegEnumValueW = &VReg__VirtualRegEnumValueW,
    .RegQueryInfoKeyW = &VReg__VirtualRegQueryInfoKeyW,
    .RegLoadMUIStringW = &VReg__VirtualRegLoadMUIStringW,
    .RegGetValueW = &VReg__VirtualRegGetValueW,
    .RegNotifyChangeKeyValue = &VReg__VirtualRegNotifyChangeKeyValue,
    .RegFlushKey = &VReg__VirtualRegFlushKey,
};

VREGDEF LSTATUS WINAPI VRegCreateVirtualRegistry(
    _In_opt_ VRegRegistry* const pBase,
    _In_opt_ const VRegNtQueryKeyFunction pfnNtQueryKey,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegRegistry** const ppRegistry
)
{
    VReg__VirtualRegistry* const pRegistry =
        (VReg__VirtualRegistry*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__VirtualRegistry));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->object.lpVtbl = &VReg__VirtualRegistryVtbl;
    pRegistry->object.pAllocContext = pAllocContext;
    pRegistry->pAllocContext = pAllocContext;
    pRegistry->pBase = pBase;
    pRegistry->pfnNtQueryKey = pfnNtQueryKey;
    InitializeSRWLock(&pRegistry->mountLock);
    if (!pRegistry->pfnNtQueryKey)
        *(void**)&pRegistry->pfnNtQueryKey = VReg__FindExport(GetModuleHandleW(L"ntdll.dll"), "NtQueryKey");

    *ppRegistry = &pRegistry->object;
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS WINAPI VRegMount(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_ VRegHive* const pHive,
    _In_ const REGSAM samView
)
{
    VReg__VirtualRegistry* pThis;
    void* pAllocContext;
    VReg__Array components = {NULL, 0, 0};
    VReg__MountPoint mount;
    SIZE_T i;
    LSTATUS status;

    if (pRegistry->lpVtbl != &VReg__VirtualRegistryVtbl || !VRegIsPredefinedHKey(hRoot))
        return ERROR_INVALID_HANDLE;
    if (samView != 0 && samView != KEY_WOW64_64KEY && samView != KEY_WOW64_32KEY)
        return ERROR_INVALID_PARAMETER;

    pThis = VReg__VirtualOf(pRegistry);
    pAllocContext = pThis->pAllocContext;
    ZeroMemory(&mount, sizeof(mount));
    mount.hRoot = hRoot;
    mount.pHive = pHive;
    mount.samView = samView;

    status = VReg__SplitPath(pAllocContext, VReg__ViewOf(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VReg__ArrayFree(pAllocContext, &components);
        return status == ERROR_OUTOFMEMORY ? status : ERROR_BAD_PATHNAME;
    }

    for (i = 0; status == ERROR_SUCCESS && i < components.count; ++i)
    {
        if (!VReg__WStringArrayAppend(pAllocContext, &mount.path, VREG__VIEW_AT(&components, i)))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VReg__NativeRootPath(pAllocContext, hRoot, &mount.nativePath);
    for (i = 0; status == ERROR_SUCCESS && mount.nativePath.count != 0 && i < components.count; ++i)
    {
        const VReg__WStringView component = VREG__VIEW_AT(&components, i);
        if (!VReg__IsWow6432Node(component) && !VReg__WStringArrayAppend(pAllocContext, &mount.nativePath, component))
            status = ERROR_OUTOFMEMORY;
    }
    VReg__ArrayFree(pAllocContext, &components);

    if (status == ERROR_SUCCESS)
    {
        AcquireSRWLockExclusive(&pThis->mountLock);
        status = VReg__PublishMount(pThis, &mount);
        ReleaseSRWLockExclusive(&pThis->mountLock);
    }

    if (status != ERROR_SUCCESS)
        VReg__MountPointFree(pAllocContext, &mount);
    return status;
}

// ==========================================================================
// Applying scripts
// ==========================================================================

static LSTATUS VReg__NotFoundIsFine(const LSTATUS status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND ? ERROR_SUCCESS : status;
}

static void VReg__ApplyCloseKey(_In_ VRegRegistry* const pRegistry, _In_opt_ const HKEY hKey)
{
    if (hKey)
        VRegCloseKey(
            pRegistry,
            hKey
        );
}

static LSTATUS VReg__PerformSetValue(_In_ const VRegOperation* const pOperation, _In_ VRegRegistry* const pRegistry)
{
    const BYTE* const pData = pOperation->pData;
    const DWORD cbData = pOperation->cbData;
    const BOOL isString = VReg__IsStringType(pOperation->dwType);
    const BOOL isTerminated = cbData >= sizeof(wchar_t) && pData[cbData - 1] == 0 && pData[cbData - 2] == 0;
    HKEY hKey = NULL;
    LSTATUS status = VRegOpenKeyExW(
        pRegistry,
        pOperation->hRoot,
        pOperation->pszPath,
        0,
        KEY_SET_VALUE,
        &hKey
    );
    if (status != ERROR_SUCCESS)
        return status;

    // Native reads past unterminated string data for a terminator and may
    // store one more wchar if it finds a zero there, so such data goes in
    // with nonzero bytes after it to store exactly what the file says.
    if (isString && !isTerminated)
    {
        static const BYTE PADDING[4] = {0x11, 0x11, 0x11, 0x11};
        VReg__Buffer padded = {NULL, 0, 0};
        if (
            !VReg__BufferAssign(pRegistry->pAllocContext, &padded, pData, cbData)
            || !VReg__BufferAppend(pRegistry->pAllocContext, &padded, PADDING, sizeof(PADDING))
        )
        {
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            status = VRegSetValueExW(
                pRegistry,
                hKey,
                pOperation->pszName,
                0,
                pOperation->dwType,
                padded.pb,
                cbData
            );
        }

        VReg__BufferFree(pRegistry->pAllocContext, &padded);
    }
    else
    {
        status = VRegSetValueExW(
            pRegistry,
            hKey,
            pOperation->pszName,
            0,
            pOperation->dwType,
            pData,
            cbData
        );
    }

    VReg__ApplyCloseKey(pRegistry, hKey);
    return status;
}

static LSTATUS VReg__Perform(_In_ const VRegOperation* const pOperation, _In_ VRegRegistry* const pRegistry)
{
    HKEY hKey = NULL;
    LSTATUS status;

    // A script can be built by hand, so the parser's guarantee is checked
    // again: an empty path would delete a whole root.
    if (pOperation->kind == VREG_OPERATION_DELETE_KEY && pOperation->pszPath[0] == L'\0')
        return ERROR_INVALID_PARAMETER;

    switch (pOperation->kind)
    {
    case VREG_OPERATION_CREATE_KEY:
        status = VRegCreateKeyExW(
            pRegistry,
            pOperation->hRoot,
            pOperation->pszPath,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_CREATE_SUB_KEY,
            NULL,
            &hKey,
            NULL
        );
        VReg__ApplyCloseKey(pRegistry, hKey);
        return status;

    case VREG_OPERATION_DELETE_KEY:
        status = VReg__NotFoundIsFine(
            VRegDeleteTreeW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath
            )
        );
        if (status != ERROR_SUCCESS)
            return status;

        return VReg__NotFoundIsFine(
            VRegDeleteKeyExW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath,
                0,
                0
            )
        );

    case VREG_OPERATION_SET_VALUE:
        return VReg__PerformSetValue(pOperation, pRegistry);

    case VREG_OPERATION_DELETE_VALUE:
        status = VRegOpenKeyExW(
            pRegistry,
            pOperation->hRoot,
            pOperation->pszPath,
            0,
            KEY_SET_VALUE,
            &hKey
        );
        if (status != ERROR_SUCCESS)
            return VReg__NotFoundIsFine(status);

        status = VReg__NotFoundIsFine(
            VRegDeleteValueW(
                pRegistry,
                hKey,
                pOperation->pszName
            )
        );
        VReg__ApplyCloseKey(pRegistry, hKey);
        return status;

    default:
        return ERROR_INVALID_PARAMETER;
    }
}

_Success_(return == ERROR_SUCCESS)
VREGDEF LSTATUS WINAPI VRegApplyOperations(
    _In_reads_(cOperations) const VRegOperation* const pOperations,
    _In_ const SIZE_T cOperations,
    _In_ VRegRegistry* const pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* const pFailedIndex
)
{
    SIZE_T i;
    for (i = 0; i < cOperations; ++i)
    {
        const LSTATUS status = VReg__Perform(&pOperations[i], pRegistry);
        if (status != ERROR_SUCCESS)
        {
            if (pFailedIndex)
                *pFailedIndex = i;
            return status;
        }
    }

    return ERROR_SUCCESS;
}

_Success_(return == ERROR_SUCCESS)
VREGDEF LSTATUS WINAPI VRegApplyScript(
    _In_ const VRegScript* const pScript,
    _In_ VRegRegistry* const pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* const pFailedIndex
)
{
    const VRegOperation* const pOperations = (const VRegOperation*)pScript->operations.p;
    return VRegApplyOperations(pOperations, pScript->operations.count, pRegistry, pFailedIndex);
}

// ==========================================================================
// Snapshot and delta
// ==========================================================================

/// A value read from a key, held until the key's values are all known and can
/// be appended in name order.
typedef struct VReg__ValueEntry
{
    VReg__WString name;
    DWORD dwType;
    VReg__Buffer data;
    BOOL isTombstone;
} VReg__ValueEntry;

/// Inserts @p pEntry into @p pValues, an array of @ref VReg__ValueEntry sorted
/// by name; the array owns the entry from then on.
static BOOL VReg__InsertValueSorted(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pValues,
    _In_ const VReg__ValueEntry* const pEntry
)
{
    const VReg__WStringView name = VReg__WStringViewOf(&pEntry->name);
    SIZE_T lo = 0;
    SIZE_T hi = pValues->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const VReg__ValueEntry* const pMid = &VREG__AT(pValues, VReg__ValueEntry, mid);
        if (VReg__CompareNames(VReg__WStringViewOf(&pMid->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return VReg__ArrayInsert(pAllocContext, pValues, sizeof(*pEntry), lo, pEntry);
}

static void VReg__FreeValues(_In_opt_ void* const pAllocContext, _Inout_ VReg__Array* const pValues)
{
    SIZE_T i;
    for (i = 0; i < pValues->count; ++i)
    {
        VReg__ValueEntry* const pEntry = &VREG__AT(pValues, VReg__ValueEntry, i);
        VReg__WStringFree(pAllocContext, &pEntry->name);
        VReg__BufferFree(pAllocContext, &pEntry->data);
    }

    pValues->count = 0;
}

/// Appends the sorted values of a key as operations and empties the array.
static LSTATUS VReg__FlushValues(
    _Inout_ VRegScript* const pScript,
    _In_ const HKEY hRoot,
    const VReg__WStringView path,
    _Inout_ VReg__Array* const pValues
)
{
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;
    for (i = 0; status == ERROR_SUCCESS && i < pValues->count; ++i)
    {
        const VReg__ValueEntry* const pEntry = &VREG__AT(pValues, VReg__ValueEntry, i);
        status = VReg__ScriptAppend(
            pScript,
            pEntry->isTombstone ? VREG_OPERATION_DELETE_VALUE : VREG_OPERATION_SET_VALUE,
            hRoot,
            path,
            VReg__WStringViewOf(&pEntry->name),
            pEntry->dwType,
            pEntry->data.pb,
            (DWORD)pEntry->data.cb,
            0
        );
    }

    VReg__FreeValues(pScript->pAllocContext, pValues);
    return status;
}

/// Appends @p path to @p pPrefix with a backslash between, into @p pFull.
static BOOL VReg__JoinKeyPath(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pFull,
    const VReg__WStringView prefix,
    const VReg__WStringView path
)
{
    if (!VReg__WStringAssign(pAllocContext, pFull, prefix.pch, prefix.cch))
        return FALSE;
    if (path.cch == 0)
        return TRUE;

    return VReg__AppendPathComponent(pAllocContext, pFull, path.pch, path.cch);
}

// --- Snapshot ---

/// Appends the key @p hKey at @p pPath below the root, then its values, then
/// its subkeys in order.
static LSTATUS VReg__WalkKey(
    _Inout_ VRegScript* const pScript,
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hRoot,
    _In_ const HKEY hKey,
    _In_ const VReg__WString* const pPath,
    const unsigned depth
)
{
    void* const pAllocContext = pScript->pAllocContext;
    VReg__Array values = {NULL, 0, 0};
    VReg__Array subkeys = {NULL, 0, 0};
    VReg__WString name = {NULL, 0, 0};
    VReg__WString childPath = {NULL, 0, 0};
    DWORD dwIndex;
    SIZE_T i;
    LSTATUS status;

    // The registry nests at most 512 levels; anything deeper is a cycle
    // through some other registry.
    if (depth > 512)
        return ERROR_CANTREAD;

    status = VReg__ScriptAppend(
        pScript,
        VREG_OPERATION_CREATE_KEY,
        hRoot,
        VReg__WStringViewOf(pPath),
        VReg__ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        VReg__ValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        status = VReg__EnumValueOwned(pRegistry, hKey, dwIndex, pAllocContext, &entry.name, &entry.dwType, &entry.data);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            VReg__WStringFree(pAllocContext, &entry.name);
            VReg__BufferFree(pAllocContext, &entry.data);
            status = ERROR_SUCCESS;
            break;
        }
        if (status == ERROR_SUCCESS && !VReg__InsertValueSorted(pAllocContext, &values, &entry))
            status = ERROR_OUTOFMEMORY;
        if (status != ERROR_SUCCESS)
        {
            VReg__WStringFree(pAllocContext, &entry.name);
            VReg__BufferFree(pAllocContext, &entry.data);
            goto done;
        }
    }

    status = VReg__FlushValues(pScript, hRoot, VReg__WStringViewOf(pPath), &values);
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        status = VReg__EnumKeyOwned(pRegistry, hKey, dwIndex, pAllocContext, &name);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VReg__WStringArrayInsertSorted(pAllocContext, &subkeys, VReg__WStringViewOf(&name)))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    for (i = 0; i < subkeys.count; ++i)
    {
        const VReg__WString* const pSubkey = &VREG__AT(&subkeys, VReg__WString, i);
        HKEY hSubkey = NULL;

        // A symbolic link is exported as the key it is (its SymbolicLinkValue),
        // never followed: following one can leave the subtree or loop.
        status = VRegOpenKeyExW(
            pRegistry,
            hKey,
            VReg__WStringText(pSubkey),
            REG_OPTION_OPEN_LINK,
            KEY_READ,
            &hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VReg__JoinKeyPath(pAllocContext, &childPath, VReg__WStringViewOf(pPath), VReg__WStringViewOf(pSubkey)))
            status = ERROR_OUTOFMEMORY;
        else
            status = VReg__WalkKey(pScript, pRegistry, hRoot, hSubkey, &childPath, depth + 1);

        VRegCloseKey(
            pRegistry,
            hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;
    }

done:
    VReg__FreeValues(pAllocContext, &values);
    VReg__ArrayFree(pAllocContext, &values);
    VReg__WStringArrayFree(pAllocContext, &subkeys);
    VReg__WStringFree(pAllocContext, &name);
    VReg__WStringFree(pAllocContext, &childPath);
    return status;
}

VREGDEF LSTATUS WINAPI VRegSnapshotScript(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript
)
{
    VRegScript* pScript = NULL;
    VReg__WString path = {NULL, 0, 0};
    HKEY hKey = NULL;
    LSTATUS status;

    *ppScript = NULL;
    if (!VReg__RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    status = VReg__ScriptCreate(pAllocContext, &pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VRegOpenKeyExW(
        pRegistry,
        hRoot,
        pszPath ? pszPath : L"",
        0,
        KEY_READ,
        &hKey
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg__WStringAssign(pAllocContext, &path, pszPath, VReg__ViewOf(pszPath).cch))
        status = ERROR_OUTOFMEMORY;
    else
        status = VReg__WalkKey(pScript, pRegistry, hRoot, hKey, &path, 0);

    VRegCloseKey(
        pRegistry,
        hKey
    );

done:
    VReg__WStringFree(pAllocContext, &path);
    if (status != ERROR_SUCCESS)
    {
        VRegDestroyScript(pScript);
        return status;
    }

    *ppScript = pScript;
    return ERROR_SUCCESS;
}

// --- Delta ---

/// The state of the walk over a hive's upper layer: the key whose values are
/// being collected, and the values so far.
typedef struct VReg__DeltaWalk
{
    VRegScript* pScript;
    HKEY hRoot;
    VReg__WStringView prefix;

    /// The full path of the key the pending values belong to.
    VReg__WString keyPath;
    VReg__Array values;
} VReg__DeltaWalk;

static LSTATUS VReg__VisitUpper(_In_ void* const pContext, _In_ const VReg__UpperRecord* const pRecord)
{
    VReg__DeltaWalk* const pWalk = (VReg__DeltaWalk*)pContext;
    void* const pAllocContext = pWalk->pScript->pAllocContext;
    LSTATUS status;

    if (pRecord->kind == VREG__UPPER_VALUE)
    {
        VReg__ValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        entry.dwType = pRecord->dwType;
        entry.isTombstone = pRecord->isTombstone;
        if (
            !VReg__WStringAssign(pAllocContext, &entry.name, pRecord->name.pch, pRecord->name.cch)
            || !VReg__BufferAssign(pAllocContext, &entry.data, pRecord->pData, pRecord->cbData)
            || !VReg__InsertValueSorted(pAllocContext, &pWalk->values, &entry)
        )
        {
            VReg__WStringFree(pAllocContext, &entry.name);
            VReg__BufferFree(pAllocContext, &entry.data);
            return ERROR_OUTOFMEMORY;
        }

        return ERROR_SUCCESS;
    }

    status = VReg__FlushValues(pWalk->pScript, pWalk->hRoot, VReg__WStringViewOf(&pWalk->keyPath), &pWalk->values);
    if (status != ERROR_SUCCESS)
        return status;
    if (!VReg__JoinKeyPath(pAllocContext, &pWalk->keyPath, pWalk->prefix, pRecord->path))
        return ERROR_OUTOFMEMORY;

    if (pRecord->isTombstone || pRecord->isOpaque)
    {
        status = VReg__ScriptAppend(
            pWalk->pScript,
            VREG_OPERATION_DELETE_KEY,
            pWalk->hRoot,
            VReg__WStringViewOf(&pWalk->keyPath),
            VReg__ViewOf(L""),
            0,
            NULL,
            0,
            0
        );
        if (status != ERROR_SUCCESS)
            return status;
    }
    if (pRecord->isTombstone)
        return ERROR_SUCCESS;

    return VReg__ScriptAppend(
        pWalk->pScript,
        VREG_OPERATION_CREATE_KEY,
        pWalk->hRoot,
        VReg__WStringViewOf(&pWalk->keyPath),
        VReg__ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
}

VREGDEF LSTATUS WINAPI VRegDeltaScript(
    _In_ VRegHive* const pHive,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript
)
{
    VReg__DeltaWalk walk;
    LSTATUS status;

    *ppScript = NULL;
    if (!VReg__RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    ZeroMemory(&walk, sizeof(walk));
    walk.hRoot = hRoot;
    walk.prefix = VReg__ViewOf(pszPath);
    status = VReg__ScriptCreate(pAllocContext, &walk.pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg__HiveSnapshotUpper(pHive, &VReg__VisitUpper, &walk);
    if (status == ERROR_SUCCESS)
        status = VReg__FlushValues(walk.pScript, hRoot, VReg__WStringViewOf(&walk.keyPath), &walk.values);

    VReg__FreeValues(pAllocContext, &walk.values);
    VReg__ArrayFree(pAllocContext, &walk.values);
    VReg__WStringFree(pAllocContext, &walk.keyPath);
    if (status != ERROR_SUCCESS)
    {
        VRegDestroyScript(walk.pScript);
        return status;
    }

    *ppScript = walk.pScript;
    return ERROR_SUCCESS;
}

// ==========================================================================
// Parsing scripts
// ==========================================================================

/// Where a logical line's text came from: the physical line and column its
/// characters from @ref start on were read at.
typedef struct VReg__LineSegment
{
    SIZE_T start;
    SIZE_T line;
    SIZE_T column;
} VReg__LineSegment;

typedef enum VReg__KeyState
{
    VREG__KEY_NONE,
    VREG__KEY_OPEN,
    VREG__KEY_DELETED,
} VReg__KeyState;

typedef struct VReg__Parser
{
    void* pAllocContext;
    VRegScript* pScript;

    /// The logical line being parsed: a line of the file with its
    /// continuations joined, and the position in it.
    VReg__WString text;
    SIZE_T pos;

    /// `VReg__LineSegment`: offsets into @ref text map back to the physical
    /// lines the text came from.
    VReg__Array segments;

    /// The physical line the logical line starts on, 1-based.
    SIZE_T line;

    VReg__KeyState state;
    HKEY hRoot;
    VReg__WString path;

    /// Set by @ref VReg__ParseFail: where in @ref text, and why.
    const char* pszError;
    SIZE_T errorOffset;
} VReg__Parser;

/// Records a syntax error at the parser's position and answers its status.
static LSTATUS VReg__ParseFail(_Inout_ VReg__Parser* const pParser, _In_z_ const char* const pszMessage)
{
    pParser->pszError = pszMessage;
    pParser->errorOffset = pParser->pos;
    return ERROR_INVALID_DATA;
}

static BOOL VReg__IsBlank(const wchar_t ch)
{
    return ch == L' ' || ch == L'\t';
}

static int VReg__HexDigit(const wchar_t ch)
{
    if (ch >= L'0' && ch <= L'9')
        return ch - L'0';
    if (ch >= L'a' && ch <= L'f')
        return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F')
        return ch - L'A' + 10;
    return -1;
}

static wchar_t VReg__LowerAscii(const wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch - L'A' + L'a') : ch;
}

static BOOL VReg__ViewEquals(const VReg__WStringView left, const VReg__WStringView right)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (left.pch[i] != right.pch[i])
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg__EqualsIgnoreAsciiCase(const VReg__WStringView left, const VReg__WStringView right)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (VReg__LowerAscii(left.pch[i]) != VReg__LowerAscii(right.pch[i]))
            return FALSE;
    }

    return TRUE;
}

static VReg__WStringView VReg__TrimEnd(VReg__WStringView text)
{
    while (text.cch != 0 && VReg__IsBlank(text.pch[text.cch - 1]))
        --text.cch;

    return text;
}

static VReg__WStringView VReg__Substring(const VReg__WStringView text, const SIZE_T start, const SIZE_T cch)
{
    VReg__WStringView result;
    result.pch = text.pch + start;
    result.cch = cch;
    return result;
}

/// The index of @p ch in @p text from @p start on, or the text's length.
static SIZE_T VReg__FindChar(const VReg__WStringView text, const SIZE_T start, const wchar_t ch)
{
    SIZE_T i;
    for (i = start; i < text.cch; ++i)
    {
        if (text.pch[i] == ch)
            return i;
    }

    return text.cch;
}

/// The physical line and column of @p offset into the logical line.
static void VReg__ParserPosition(
    _In_ const VReg__Parser* const pParser,
    const SIZE_T offset,
    _Out_ SIZE_T* const pLine,
    _Out_ SIZE_T* const pColumn
)
{
    const VReg__LineSegment* pSegment = &VREG__AT(&pParser->segments, VReg__LineSegment, 0);
    SIZE_T i;
    for (i = 0; i < pParser->segments.count; ++i)
    {
        const VReg__LineSegment* const pCandidate = &VREG__AT(&pParser->segments, VReg__LineSegment, i);
        if (pCandidate->start > offset)
            break;

        pSegment = pCandidate;
    }

    *pLine = pSegment->line;
    *pColumn = pSegment->column + (offset - pSegment->start);
}

static BOOL VReg__AtEnd(_In_ const VReg__Parser* const pParser)
{
    return pParser->pos >= pParser->text.cch;
}

static wchar_t VReg__Current(_In_ const VReg__Parser* const pParser)
{
    return pParser->text.psz[pParser->pos];
}

static void VReg__SkipBlanks(_Inout_ VReg__Parser* const pParser)
{
    while (!VReg__AtEnd(pParser) && VReg__IsBlank(VReg__Current(pParser)))
        ++pParser->pos;
}

static LSTATUS VReg__ExpectEnd(_Inout_ VReg__Parser* const pParser)
{
    VReg__SkipBlanks(pParser);
    if (!VReg__AtEnd(pParser))
        return VReg__ParseFail(pParser, "unexpected text at the end of the line");

    return ERROR_SUCCESS;
}

/// Consumes @p pszToken, ignoring case, if the text continues with it.
static BOOL VReg__Consume(_Inout_ VReg__Parser* const pParser, _In_z_ const wchar_t* const pszToken)
{
    const VReg__WStringView token = VReg__ViewOf(pszToken);
    const VReg__WStringView text = VReg__WStringViewOf(&pParser->text);
    if (text.cch - pParser->pos < token.cch)
        return FALSE;
    if (!VReg__EqualsIgnoreAsciiCase(VReg__Substring(text, pParser->pos, token.cch), token))
        return FALSE;

    pParser->pos += token.cch;
    return TRUE;
}

/// Reads the logical line starting at physical line @p index of @p pLines
/// (empty for blank and comment lines) and answers the index of its last
/// physical line in @p pLast.
static LSTATUS VReg__ReadLogicalLine(
    _Inout_ VReg__Parser* const pParser,
    _In_ const VReg__Array* const pLines,
    const SIZE_T index,
    _Out_ SIZE_T* const pLast
)
{
    void* const pAllocContext = pParser->pAllocContext;
    VReg__WStringView physical = VREG__AT(pLines, VReg__WStringView, index);
    VReg__LineSegment segment;
    SIZE_T current = index;

    pParser->segments.count = 0;
    if (!VReg__WStringResize(pAllocContext, &pParser->text, 0))
        return ERROR_OUTOFMEMORY;

    segment.start = 0;
    segment.line = index + 1;
    segment.column = 1;
    while (physical.cch != 0 && VReg__IsBlank(physical.pch[0]))
    {
        ++physical.pch;
        --physical.cch;
        ++segment.column;
    }
    if (!VReg__ArrayAppend(pAllocContext, &pParser->segments, sizeof(segment), &segment))
        return ERROR_OUTOFMEMORY;

    *pLast = index;
    if (physical.cch == 0 || physical.pch[0] == L';')
        return ERROR_SUCCESS;

    if (!VReg__WStringAssign(pAllocContext, &pParser->text, physical.pch, physical.cch))
        return ERROR_OUTOFMEMORY;
    if (physical.pch[0] == L'[')
        return ERROR_SUCCESS;

    // A value that continues ends its line in a backslash; a closing quote
    // ends every complete one.
    while (current + 1 < pLines->count)
    {
        VReg__WStringView joined = VReg__TrimEnd(VReg__WStringViewOf(&pParser->text));
        VReg__WStringView next;
        if (joined.cch == 0 || joined.pch[joined.cch - 1] != L'\\')
            break;

        VReg__WStringTruncate(&pParser->text, joined.cch - 1);

        ++current;
        next = VREG__AT(pLines, VReg__WStringView, current);
        segment.start = pParser->text.cch;
        segment.line = current + 1;
        segment.column = 1;
        while (next.cch != 0 && VReg__IsBlank(next.pch[0]))
        {
            ++next.pch;
            --next.cch;
            ++segment.column;
        }
        if (
            !VReg__ArrayAppend(pAllocContext, &pParser->segments, sizeof(segment), &segment)
            || !VReg__WStringAppend(pAllocContext, &pParser->text, next.pch, next.cch)
        )
            return ERROR_OUTOFMEMORY;
    }

    *pLast = current;
    return ERROR_SUCCESS;
}

static LSTATUS VReg__ParseKey(_Inout_ VReg__Parser* const pParser)
{
    void* const pAllocContext = pParser->pAllocContext;
    const VReg__WStringView text = VReg__WStringViewOf(&pParser->text);
    SIZE_T close = text.cch;
    BOOL isDelete;
    VReg__WStringView full;
    SIZE_T slash;
    VReg__WStringView path;
    HKEY hRoot = NULL;
    LSTATUS status;

    while (close > 0 && text.pch[close - 1] != L']')
        --close;
    if (close == 0)
    {
        pParser->pos = text.cch;
        return VReg__ParseFail(pParser, "missing ] after the key name");
    }

    // `close` is one past the bracket: the position that ends the key.
    pParser->pos = close;
    status = VReg__ExpectEnd(pParser);
    if (status != ERROR_SUCCESS)
        return status;

    --close;
    pParser->pos = 1;
    isDelete = text.pch[pParser->pos] == L'-';
    if (isDelete)
        ++pParser->pos;

    full = VReg__Substring(text, pParser->pos, close - pParser->pos);
    slash = VReg__FindChar(full, 0, L'\\');
    if (!VReg__FindRoot(VReg__Substring(full, 0, slash), &hRoot))
        return VReg__ParseFail(pParser, "unknown registry root");

    path = slash < full.cch ? VReg__Substring(full, slash + 1, full.cch - slash - 1) : VReg__Substring(full, 0, 0);
    if (isDelete && path.cch == 0)
        return VReg__ParseFail(pParser, "a root key cannot be deleted");

    status = VReg__ScriptAppend(
        pParser->pScript,
        isDelete ? VREG_OPERATION_DELETE_KEY : VREG_OPERATION_CREATE_KEY,
        hRoot,
        path,
        VReg__ViewOf(L""),
        0,
        NULL,
        0,
        pParser->line
    );
    if (status != ERROR_SUCCESS)
        return status;

    pParser->state = isDelete ? VREG__KEY_DELETED : VREG__KEY_OPEN;
    pParser->hRoot = hRoot;
    return VReg__WStringAssign(pAllocContext, &pParser->path, path.pch, path.cch) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

/// Reads a quoted string at the position (on its opening quote) into @p pText.
static LSTATUS VReg__ParseQuoted(_Inout_ VReg__Parser* const pParser, _Inout_ VReg__WString* const pText)
{
    void* const pAllocContext = pParser->pAllocContext;
    ++pParser->pos;
    if (!VReg__WStringResize(pAllocContext, pText, 0))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        wchar_t ch;
        if (VReg__AtEnd(pParser))
            return VReg__ParseFail(pParser, "unterminated string");

        ch = VReg__Current(pParser);
        if (ch == L'"')
        {
            ++pParser->pos;
            return ERROR_SUCCESS;
        }

        // Any escape but `\\` and `\"` is a syntax error, so a typo cannot
        // silently change data; regedit is more lenient here.
        if (ch == L'\\')
        {
            ++pParser->pos;
            if (VReg__AtEnd(pParser) || (VReg__Current(pParser) != L'\\' && VReg__Current(pParser) != L'"'))
            {
                --pParser->pos;
                return VReg__ParseFail(pParser, "invalid escape, only \\\\ and \\\" are allowed");
            }

            ch = VReg__Current(pParser);
        }

        if (!VReg__WStringAppendChar(pAllocContext, pText, ch))
            return ERROR_OUTOFMEMORY;

        ++pParser->pos;
    }
}

static LSTATUS VReg__ParseHexNumber(_Inout_ VReg__Parser* const pParser, const SIZE_T maxDigits, _Out_ DWORD* const pdwValue)
{
    DWORD dwValue = 0;
    SIZE_T digits = 0;
    *pdwValue = 0;
    while (!VReg__AtEnd(pParser) && VReg__HexDigit(VReg__Current(pParser)) >= 0)
    {
        if (++digits > maxDigits)
            return VReg__ParseFail(pParser, "too many hex digits");

        dwValue = (dwValue << 4) | (DWORD)VReg__HexDigit(VReg__Current(pParser));
        ++pParser->pos;
    }
    if (digits == 0)
        return VReg__ParseFail(pParser, "expected a hex number");

    *pdwValue = dwValue;
    return ERROR_SUCCESS;
}

static LSTATUS VReg__ParseHexBytes(_Inout_ VReg__Parser* const pParser, _Inout_ VReg__Buffer* const pData)
{
    VReg__SkipBlanks(pParser);
    if (VReg__AtEnd(pParser))
        return ERROR_SUCCESS;

    for (;;)
    {
        DWORD dwByte;
        BYTE byte;
        LSTATUS status;
        if (VReg__AtEnd(pParser) || VReg__HexDigit(VReg__Current(pParser)) < 0)
            return VReg__ParseFail(pParser, "expected a hex byte");

        status = VReg__ParseHexNumber(pParser, 2, &dwByte);
        if (status != ERROR_SUCCESS)
            return status;

        byte = (BYTE)dwByte;
        if (!VReg__BufferAppend(pParser->pAllocContext, pData, &byte, 1))
            return ERROR_OUTOFMEMORY;

        VReg__SkipBlanks(pParser);
        if (VReg__AtEnd(pParser))
            return ERROR_SUCCESS;
        if (VReg__Current(pParser) != L',')
            return VReg__ParseFail(pParser, "expected , between hex bytes");

        ++pParser->pos;
        VReg__SkipBlanks(pParser);
    }
}

static LSTATUS VReg__ParseValue(_Inout_ VReg__Parser* const pParser)
{
    void* const pAllocContext = pParser->pAllocContext;
    VReg__WString name = {NULL, 0, 0};
    VReg__WString text = {NULL, 0, 0};
    VReg__Buffer data = {NULL, 0, 0};
    VRegOperationKind kind = VREG_OPERATION_SET_VALUE;
    DWORD dwType = 0;
    LSTATUS status = ERROR_SUCCESS;

    if (VReg__Current(pParser) == L'@')
        ++pParser->pos;
    else if (VReg__Current(pParser) == L'"')
        status = VReg__ParseQuoted(pParser, &name);
    else
        status = VReg__ParseFail(pParser, "expected a value name or a [key]");
    if (status != ERROR_SUCCESS)
        goto done;

    if (pParser->state == VREG__KEY_NONE)
    {
        pParser->pos = 0;
        status = VReg__ParseFail(pParser, "a value outside any key");
        goto done;
    }

    // A value under a [-key] line is an error rather than silently dropped,
    // since it can only be a mistake.
    if (pParser->state == VREG__KEY_DELETED)
    {
        pParser->pos = 0;
        status = VReg__ParseFail(pParser, "a value after a deleted key");
        goto done;
    }

    VReg__SkipBlanks(pParser);
    if (VReg__AtEnd(pParser) || VReg__Current(pParser) != L'=')
    {
        status = VReg__ParseFail(pParser, "expected =");
        goto done;
    }

    ++pParser->pos;
    VReg__SkipBlanks(pParser);

    if (!VReg__AtEnd(pParser) && VReg__Current(pParser) == L'-')
    {
        ++pParser->pos;
        status = VReg__ExpectEnd(pParser);
        kind = VREG_OPERATION_DELETE_VALUE;
    }
    else if (!VReg__AtEnd(pParser) && VReg__Current(pParser) == L'"')
    {
        status = VReg__ParseQuoted(pParser, &text);
        if (status == ERROR_SUCCESS)
            status = VReg__ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_SZ;
            if (!VReg__BufferAssign(pAllocContext, &data, VReg__WStringText(&text), (text.cch + 1) * sizeof(wchar_t)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VReg__Consume(pParser, L"dword:"))
    {
        DWORD dwValue = 0;
        status = VReg__ParseHexNumber(pParser, 8, &dwValue);
        if (status == ERROR_SUCCESS)
            status = VReg__ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_DWORD;
            if (!VReg__BufferAssign(pAllocContext, &data, &dwValue, sizeof(dwValue)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VReg__Consume(pParser, L"hex"))
    {
        dwType = REG_BINARY;
        if (VReg__Consume(pParser, L"("))
        {
            status = VReg__ParseHexNumber(pParser, 8, &dwType);
            if (status == ERROR_SUCCESS && !VReg__Consume(pParser, L")"))
                status = VReg__ParseFail(pParser, "expected ) after the value type");
        }
        if (status == ERROR_SUCCESS && !VReg__Consume(pParser, L":"))
            status = VReg__ParseFail(pParser, "expected : after hex");
        if (status == ERROR_SUCCESS)
            status = VReg__ParseHexBytes(pParser, &data);
    }
    else
    {
        status = VReg__ParseFail(pParser, "expected a string, dword:, hex: or -");
    }
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__ScriptAppend(
        pParser->pScript,
        kind,
        pParser->hRoot,
        VReg__WStringViewOf(&pParser->path),
        VReg__WStringViewOf(&name),
        dwType,
        data.pb,
        (DWORD)data.cb,
        pParser->line
    );

done:
    VReg__WStringFree(pAllocContext, &name);
    VReg__WStringFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &data);
    return status;
}

static LSTATUS VReg__ParseLogicalLine(_Inout_ VReg__Parser* const pParser)
{
    // Key paths and value names reach the registry as C strings, where a NUL
    // would silently cut them short ("[-HKCU\<NUL>x]" would delete all of
    // HKCU); no valid .reg text holds one anywhere.
    pParser->pos = VReg__FindChar(VReg__WStringViewOf(&pParser->text), 0, L'\0');
    if (pParser->pos < pParser->text.cch)
        return VReg__ParseFail(pParser, "a NUL character");

    pParser->pos = 0;
    return pParser->text.psz[0] == L'[' ? VReg__ParseKey(pParser) : VReg__ParseValue(pParser);
}

/// Parses all of @p text into the parser's script. A syntax error answers
/// @ref ERROR_INVALID_DATA with @p pError filled in.
static LSTATUS VReg__ParseText(
    _Inout_ VReg__Parser* const pParser,
    const VReg__WStringView text,
    _Inout_opt_ VRegParseError* const pError
)
{
    void* const pAllocContext = pParser->pAllocContext;
    VReg__Array lines = {NULL, 0, 0};
    VReg__WStringView header;
    SIZE_T start;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    for (start = 0; start <= text.cch;)
    {
        const SIZE_T end = VReg__FindChar(text, start, L'\n');
        VReg__WStringView line = VReg__Substring(text, start, end - start);
        if (line.cch != 0 && line.pch[line.cch - 1] == L'\r')
            --line.cch;
        if (!VReg__ArrayAppend(pAllocContext, &lines, sizeof(line), &line))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        start = end + 1;
    }

    // The loop above appended the first line, however short the text.
    _Analysis_assume_(lines.p != NULL);
    header = VReg__TrimEnd(VREG__AT(&lines, VReg__WStringView, 0));
    if (
        !VReg__ViewEquals(header, VReg__ViewOf(L"Windows Registry Editor Version 5.00"))
        && !VReg__ViewEquals(header, VReg__ViewOf(L"REGEDIT4"))
    )
    {
        if (pError)
        {
            pError->line = 1;
            pError->column = 1;
            pError->pszMessage = "missing or unknown registry editor header";
        }

        status = ERROR_INVALID_DATA;
        goto done;
    }

    for (i = 1; i < lines.count; ++i)
    {
        SIZE_T last = i;
        pParser->line = i + 1;
        status = VReg__ReadLogicalLine(pParser, &lines, i, &last);
        if (status == ERROR_SUCCESS && pParser->text.cch != 0)
            status = VReg__ParseLogicalLine(pParser);
        if (status == ERROR_INVALID_DATA && pError)
            VReg__ParserPosition(pParser, pParser->errorOffset, &pError->line, &pError->column);
        if (status == ERROR_INVALID_DATA && pError)
            pError->pszMessage = pParser->pszError;
        if (status != ERROR_SUCCESS)
            goto done;

        i = last;
    }

done:
    VReg__ArrayFree(pAllocContext, &lines);
    return status;
}

VREGDEF LSTATUS WINAPI VRegParseScript(
    _In_reads_bytes_(cbFile) const void* const pFile,
    _In_ const SIZE_T cbFile,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VReg__WString text = {NULL, 0, 0};
    VReg__Parser parser;
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));

    status = VReg__DecodeFile(pAllocContext, pFile, cbFile, &text, pError);
    if (status != ERROR_SUCCESS)
    {
        VReg__WStringFree(pAllocContext, &text);
        return status;
    }

    ZeroMemory(&parser, sizeof(parser));
    parser.pAllocContext = pAllocContext;
    status = VReg__ScriptCreate(pAllocContext, &parser.pScript);
    if (status == ERROR_SUCCESS)
        status = VReg__ParseText(&parser, VReg__WStringViewOf(&text), pError);

    VReg__WStringFree(pAllocContext, &parser.text);
    VReg__WStringFree(pAllocContext, &parser.path);
    VReg__ArrayFree(pAllocContext, &parser.segments);
    VReg__WStringFree(pAllocContext, &text);
    if (status != ERROR_SUCCESS)
    {
        VRegDestroyScript(parser.pScript);
        return status;
    }

    *ppScript = parser.pScript;
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS WINAPI VRegParseScriptFile(
    _In_z_ const LPCWSTR pszPath,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VReg__Buffer file = {NULL, 0, 0};
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));
    status = VReg__ReadWholeFile(pAllocContext, pszPath, &file);
    if (status == ERROR_SUCCESS)
        status = VRegParseScript(file.pb, file.cb, pAllocContext, ppScript, pError);

    VReg__BufferFree(pAllocContext, &file);
    return status;
}

// ==========================================================================
// Scripts
// ==========================================================================

/// Every root a `.reg` file can name, the one list import and export share.
static const VReg__RootKey VREG__ROOT_KEYS[] = {
    {L"HKEY_CLASSES_ROOT", L"HKCR", HKEY_CLASSES_ROOT},
    {L"HKEY_CURRENT_USER", L"HKCU", HKEY_CURRENT_USER},
    {L"HKEY_LOCAL_MACHINE", L"HKLM", HKEY_LOCAL_MACHINE},
    {L"HKEY_USERS", L"HKU", HKEY_USERS},
    {L"HKEY_CURRENT_CONFIG", L"HKCC", HKEY_CURRENT_CONFIG},
};

VREGDEF _Ret_maybenull_z_ const wchar_t* VReg__RootName(_In_opt_ const HKEY hRoot)
{
    SIZE_T i;
    for (i = 0; i < sizeof(VREG__ROOT_KEYS) / sizeof(VREG__ROOT_KEYS[0]); ++i)
    {
        if (VREG__ROOT_KEYS[i].hKey == hRoot)
            return VREG__ROOT_KEYS[i].pszLong;
    }

    return NULL;
}

VREGDEF BOOL VReg__FindRoot(const VReg__WStringView name, _Out_ PHKEY const phRoot)
{
    SIZE_T i;
    *phRoot = NULL;
    for (i = 0; i < sizeof(VREG__ROOT_KEYS) / sizeof(VREG__ROOT_KEYS[0]); ++i)
    {
        const VReg__RootKey* const pRoot = &VREG__ROOT_KEYS[i];
        if (
            VReg__EqualsIgnoreAsciiCase(name, VReg__ViewOf(pRoot->pszLong))
            || VReg__EqualsIgnoreAsciiCase(name, VReg__ViewOf(pRoot->pszShort))
        )
        {
            *phRoot = pRoot->hKey;
            return TRUE;
        }
    }

    return FALSE;
}

VREGDEF LSTATUS VReg__ScriptCreate(_In_opt_ void* const pAllocContext, _Outptr_ VRegScript** const ppScript)
{
    VRegScript* const pScript = (VRegScript*)VReg__AllocZeroed(pAllocContext, sizeof(VRegScript));
    *ppScript = NULL;
    if (!pScript)
        return ERROR_OUTOFMEMORY;

    pScript->pAllocContext = pAllocContext;
    *ppScript = pScript;
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS VReg__ScriptAppend(
    _Inout_ VRegScript* const pScript,
    const VRegOperationKind kind,
    _In_ const HKEY hRoot,
    const VReg__WStringView path,
    const VReg__WStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    const SIZE_T line
)
{
    void* const pAllocContext = pScript->pAllocContext;
    VReg__ScriptEntry entry;
    VRegOperation operation;
    ZeroMemory(&entry, sizeof(entry));
    if (
        !VReg__WStringAssign(pAllocContext, &entry.path, path.pch, path.cch)
        || !VReg__WStringAssign(pAllocContext, &entry.name, name.pch, name.cch)
        || !VReg__BufferAssign(pAllocContext, &entry.data, pData, cbData)
        || !VReg__ArrayReserve(pAllocContext, &pScript->operations, sizeof(operation), pScript->operations.count + 1)
        || !VReg__ArrayAppend(pAllocContext, &pScript->entries, sizeof(entry), &entry)
    )
    {
        VReg__WStringFree(pAllocContext, &entry.path);
        VReg__WStringFree(pAllocContext, &entry.name);
        VReg__BufferFree(pAllocContext, &entry.data);
        return ERROR_OUTOFMEMORY;
    }

    // The entry's storage is on the heap, so the pointers stay valid when the
    // array of entries moves.
    operation.kind = kind;
    operation.hRoot = hRoot;
    operation.pszPath = VReg__WStringText(&entry.path);
    operation.pszName = VReg__WStringText(&entry.name);
    operation.dwType = dwType;
    operation.pData = entry.data.pb;
    operation.cbData = cbData;
    operation.line = line;
    VReg__ArrayAppend(pAllocContext, &pScript->operations, sizeof(operation), &operation);
    return ERROR_SUCCESS;
}

VREGDEF void WINAPI VRegDestroyScript(_In_opt_ VRegScript* const pScript)
{
    SIZE_T i;
    if (!pScript)
        return;

    for (i = 0; i < pScript->entries.count; ++i)
    {
        VReg__ScriptEntry* const pEntry = &VREG__AT(&pScript->entries, VReg__ScriptEntry, i);
        VReg__WStringFree(pScript->pAllocContext, &pEntry->path);
        VReg__WStringFree(pScript->pAllocContext, &pEntry->name);
        VReg__BufferFree(pScript->pAllocContext, &pEntry->data);
    }

    VReg__ArrayFree(pScript->pAllocContext, &pScript->entries);
    VReg__ArrayFree(pScript->pAllocContext, &pScript->operations);
    VReg__Free(pScript->pAllocContext, pScript);
}

VREGDEF _Ret_notnull_ const VRegOperation* WINAPI VRegGetScriptOperations(
    _In_ const VRegScript* const pScript,
    _Out_ SIZE_T* const pcOperations
)
{
    static const VRegOperation NONE = {VREG_OPERATION_CREATE_KEY, NULL, L"", L"", 0, NULL, 0, 0};
    *pcOperations = pScript->operations.count;
    return pScript->operations.count == 0 ? &NONE : (const VRegOperation*)pScript->operations.p;
}

VREGDEF void WINAPI VRegFreeScriptBytes(_In_ const VRegScript* const pScript, _In_opt_ void* const pBytes)
{
    VReg__Free(pScript->pAllocContext, pBytes);
}

// ==========================================================================
// Script text
// ==========================================================================

static LSTATUS VReg__DecodeFail(_Inout_opt_ VRegParseError* const pError, _In_z_ const char* const pszMessage)
{
    if (pError)
    {
        pError->line = 1;
        pError->column = 1;
        pError->pszMessage = pszMessage;
    }

    return ERROR_INVALID_DATA;
}

static BOOL VReg__StartsWithAscii(_In_reads_bytes_(cb) const BYTE* const pb, const SIZE_T cb, _In_z_ const char* const pszPrefix)
{
    SIZE_T i;
    for (i = 0; pszPrefix[i] != '\0'; ++i)
    {
        if (i >= cb || pb[i] != (BYTE)pszPrefix[i])
            return FALSE;
    }

    return TRUE;
}

static LSTATUS VReg__Widen(
    _In_opt_ void* const pAllocContext,
    const UINT codePage,
    _In_reads_bytes_(cb) const BYTE* const pb,
    const SIZE_T cb,
    _Inout_ VReg__WString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const DWORD dwFlags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const LSTATUS status = VReg__WideFromMultiByte(pAllocContext, codePage, dwFlags, (const char*)pb, cb, pText);
    if (status == ERROR_NO_UNICODE_TRANSLATION)
        return VReg__DecodeFail(pError, "the text is not valid in its encoding");

    return status;
}

VREGDEF LSTATUS VReg__DecodeFile(
    _In_opt_ void* const pAllocContext,
    _In_reads_bytes_(cbFile) const void* const pFile,
    const SIZE_T cbFile,
    _Inout_ VReg__WString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const BYTE* const pb = (const BYTE*)pFile;
    if (cbFile > (SIZE_T)MAXINT32)
        return VReg__DecodeFail(pError, "the file is too large");

    if (cbFile >= 2 && pb[0] == 0xFF && pb[1] == 0xFE)
    {
        const SIZE_T cbBody = cbFile - 2;
        if (cbBody % sizeof(wchar_t) != 0)
            return VReg__DecodeFail(pError, "a UTF-16 file has an odd byte count");
        if (!VReg__WStringAssign(pAllocContext, pText, (const wchar_t*)(pb + 2), cbBody / sizeof(wchar_t)))
            return ERROR_OUTOFMEMORY;

        return ERROR_SUCCESS;
    }
    if (cbFile >= 2 && pb[0] == 0xFE && pb[1] == 0xFF)
        return VReg__DecodeFail(pError, "big-endian UTF-16 is not supported");
    if (cbFile >= 3 && pb[0] == 0xEF && pb[1] == 0xBB && pb[2] == 0xBF)
        return VReg__Widen(pAllocContext, CP_UTF8, pb + 3, cbFile - 3, pText, pError);

    return VReg__Widen(
        pAllocContext,
        VReg__StartsWithAscii(pb, cbFile, "REGEDIT4") ? CP_ACP : CP_UTF8,
        pb,
        cbFile,
        pText,
        pError
    );
}

VREGDEF LSTATUS VReg__EncodeFile(
    _In_opt_ void* const pAllocContext,
    _In_ const VReg__WString* const pText,
    const VRegFormat format,
    _Inout_ VReg__Buffer* const pFile
)
{
    UINT codePage;
    BOOL isUtf8;
    BOOL fUsedDefault = FALSE;
    LSTATUS status;

    if (!VReg__BufferResize(pAllocContext, pFile, 0))
        return ERROR_OUTOFMEMORY;

    if (format == VREG_FORMAT_UNICODE)
    {
        static const BYTE BOM[2] = {0xFF, 0xFE};
        if (
            !VReg__BufferAppend(pAllocContext, pFile, BOM, sizeof(BOM))
            || !VReg__BufferAppend(pAllocContext, pFile, VReg__WStringText(pText), pText->cch * sizeof(wchar_t))
        )
            return ERROR_OUTOFMEMORY;

        return ERROR_SUCCESS;
    }
    if (pText->cch > (SIZE_T)MAXINT32)
        return ERROR_NOT_ENOUGH_MEMORY;

    // An ACP of UTF-8 takes neither the best-fit flag nor a default-char
    // report.
    codePage = format == VREG_FORMAT_UTF8 ? CP_UTF8 : CP_ACP;
    isUtf8 = codePage == CP_UTF8 || GetACP() == CP_UTF8;
    status = VReg__MultiByteFromWide(
        pAllocContext,
        codePage,
        isUtf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        isUtf8 ? NULL : &fUsedDefault,
        VReg__WStringText(pText),
        pText->cch,
        pFile
    );
    if (status != ERROR_SUCCESS)
        return status;
    if (fUsedDefault)
    {
        pFile->cb = 0;
        return ERROR_NO_UNICODE_TRANSLATION;
    }

    return ERROR_SUCCESS;
}

VREGDEF LSTATUS VReg__ReadWholeFile(_In_opt_ void* const pAllocContext, _In_z_ const LPCWSTR pszPath, _Inout_ VReg__Buffer* const pFile)
{
    LARGE_INTEGER size;
    LSTATUS status = ERROR_SUCCESS;
    const HANDLE hFile = CreateFileW(pszPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return (LSTATUS)GetLastError();

    size.QuadPart = 0;
    if (!GetFileSizeEx(hFile, &size))
    {
        status = (LSTATUS)GetLastError();
    }
    else if (size.QuadPart > MAXINT32)
    {
        status = ERROR_FILE_TOO_LARGE;
    }
    else if (!VReg__BufferResize(pAllocContext, pFile, (SIZE_T)size.QuadPart))
    {
        status = ERROR_OUTOFMEMORY;
    }
    else
    {
        DWORD cbRead = 0;
        if (!ReadFile(hFile, pFile->pb, (DWORD)pFile->cb, &cbRead, NULL))
            status = (LSTATUS)GetLastError();
        else
            pFile->cb = cbRead;
    }

    CloseHandle(hFile);
    return status;
}

VREGDEF LSTATUS VReg__WriteWholeFile(_In_z_ const LPCWSTR pszPath, _In_reads_bytes_(cbFile) const void* const pFile, const SIZE_T cbFile)
{
    LSTATUS status = ERROR_SUCCESS;
    DWORD cbWritten = 0;
    const HANDLE hFile = CreateFileW(pszPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return (LSTATUS)GetLastError();

    if (!WriteFile(hFile, pFile, (DWORD)cbFile, &cbWritten, NULL))
        status = (LSTATUS)GetLastError();
    if (!CloseHandle(hFile) && status == ERROR_SUCCESS)
        status = (LSTATUS)GetLastError();
    return status;
}

// ==========================================================================
// Writing scripts
// ==========================================================================

// Windows `reg export` breaks after the first comma past this column and
// indents continuation lines by two spaces. Wine regedit breaks at the column
// itself, which differs when a line ends exactly there.
#define VREG__HEX_WRAP_COLUMN ((SIZE_T)76)

static const wchar_t VREG__NEWLINE[] = L"\r\n";

/// Appends the terminated literal @p psz.
static BOOL VReg__OutAppend(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, _In_z_ const wchar_t* const psz)
{
    const VReg__WStringView text = VReg__ViewOf(psz);
    return VReg__WStringAppend(pAllocContext, pOut, text.pch, text.cch);
}

static BOOL VReg__HasLineBreak(const VReg__WStringView text)
{
    SIZE_T i;
    for (i = 0; i < text.cch; ++i)
    {
        if (text.pch[i] == L'\r' || text.pch[i] == L'\n')
            return TRUE;
    }

    return FALSE;
}

static BOOL VReg__AppendQuoted(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, const VReg__WStringView text)
{
    SIZE_T i;
    if (!VReg__WStringAppendChar(pAllocContext, pOut, L'"'))
        return FALSE;

    for (i = 0; i < text.cch; ++i)
    {
        const wchar_t ch = text.pch[i];
        if ((ch == L'\\' || ch == L'"') && !VReg__WStringAppendChar(pAllocContext, pOut, L'\\'))
            return FALSE;
        if (!VReg__WStringAppendChar(pAllocContext, pOut, ch))
            return FALSE;
    }

    return VReg__WStringAppendChar(pAllocContext, pOut, L'"');
}

/// The text a REG_SZ takes in the friendly form; FALSE when the form would not
/// reproduce the bytes.
static BOOL VReg__FriendlyString(_In_reads_bytes_opt_(cbData) const BYTE* const pData, const DWORD cbData, _Out_ VReg__WStringView* pText)
{
    const wchar_t* pch;
    SIZE_T cch;
    SIZE_T i;

    pText->pch = NULL;
    pText->cch = 0;
    if (cbData < sizeof(wchar_t) || cbData % sizeof(wchar_t) != 0)
        return FALSE;

    // Two bytes or more of data have storage.
    _Analysis_assume_(pData != NULL);
    pch = (const wchar_t*)pData;
    cch = cbData / sizeof(wchar_t);
    if (pch[cch - 1] != L'\0')
        return FALSE;

    --cch;
    for (i = 0; i < cch; ++i)
    {
        if (pch[i] == L'\0' || pch[i] == L'\r' || pch[i] == L'\n')
            return FALSE;
    }

    pText->pch = pch;
    pText->cch = cch;
    return TRUE;
}

/// Appends @p value in lowercase hex with at least @p cchMin digits.
static BOOL VReg__AppendHexNumber(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, DWORD value, const SIZE_T cchMin)
{
    static const wchar_t DIGITS[] = L"0123456789abcdef";
    wchar_t wszDigits[8];
    SIZE_T cchDigits = 0;
    SIZE_T i;

    do
    {
        wszDigits[cchDigits++] = DIGITS[value & 0xF];
        value >>= 4;
    }
    while (value != 0 && cchDigits < 8);

    for (i = cchDigits; i < cchMin; ++i)
    {
        if (!VReg__WStringAppendChar(pAllocContext, pOut, L'0'))
            return FALSE;
    }
    for (i = 0; i < cchDigits; ++i)
    {
        if (!VReg__WStringAppendChar(pAllocContext, pOut, wszDigits[cchDigits - 1 - i]))
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg__AppendHex(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pOut,
    _In_ const VRegOperation* const pOperation,
    SIZE_T column
)
{
    const SIZE_T cchBefore = pOut->cch;
    SIZE_T i;

    if (pOperation->dwType == REG_BINARY)
    {
        if (!VReg__OutAppend(pAllocContext, pOut, L"hex:"))
            return FALSE;
    }
    else
    {
        if (
            !VReg__OutAppend(pAllocContext, pOut, L"hex(")
            || !VReg__AppendHexNumber(pAllocContext, pOut, pOperation->dwType, 1)
            || !VReg__OutAppend(pAllocContext, pOut, L"):")
        )
            return FALSE;
    }

    column += pOut->cch - cchBefore;
    for (i = 0; i < pOperation->cbData; ++i)
    {
        if (!VReg__AppendHexNumber(pAllocContext, pOut, pOperation->pData[i], 2))
            return FALSE;

        column += 2;
        if (i + 1 == pOperation->cbData)
            break;
        if (!VReg__WStringAppendChar(pAllocContext, pOut, L','))
            return FALSE;
        if (++column > VREG__HEX_WRAP_COLUMN)
        {
            if (!VReg__OutAppend(pAllocContext, pOut, L"\\\r\n  "))
                return FALSE;

            column = 2;
        }
    }

    return TRUE;
}

static BOOL VReg__AppendValue(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, _In_ const VRegOperation* const pOperation)
{
    const SIZE_T lineStart = pOut->cch;
    const VReg__WStringView name = VReg__ViewOf(pOperation->pszName);
    VReg__WStringView text;
    BOOL isAppended;

    if (name.cch == 0)
        isAppended = VReg__WStringAppendChar(pAllocContext, pOut, L'@');
    else
        isAppended = VReg__AppendQuoted(pAllocContext, pOut, name);
    if (!isAppended || !VReg__WStringAppendChar(pAllocContext, pOut, L'='))
        return FALSE;

    if (pOperation->kind == VREG_OPERATION_DELETE_VALUE)
    {
        isAppended = VReg__WStringAppendChar(pAllocContext, pOut, L'-');
    }
    else if (pOperation->dwType == REG_SZ && VReg__FriendlyString(pOperation->pData, pOperation->cbData, &text))
    {
        isAppended = VReg__AppendQuoted(pAllocContext, pOut, text);
    }
    else if (pOperation->dwType == REG_DWORD && pOperation->cbData == sizeof(DWORD))
    {
        DWORD dwValue;
        CopyMemory(&dwValue, pOperation->pData, sizeof(dwValue));
        isAppended =
            VReg__OutAppend(pAllocContext, pOut, L"dword:")
            && VReg__AppendHexNumber(pAllocContext, pOut, dwValue, 8);
    }
    else
    {
        isAppended = VReg__AppendHex(pAllocContext, pOut, pOperation, pOut->cch - lineStart);
    }

    return isAppended && VReg__OutAppend(pAllocContext, pOut, VREG__NEWLINE);
}

/// Appends `[key]` or `[-key]` for @p pOperation's key, preceded by a blank
/// line unless it opens the file.
static LSTATUS VReg__AppendKeyLine(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pOut,
    _In_ const VRegOperation* const pOperation,
    const BOOL isDelete,
    const BOOL isFirst
)
{
    const wchar_t* const pszRoot = VReg__RootName(pOperation->hRoot);
    const VReg__WStringView path = VReg__ViewOf(pOperation->pszPath);
    if (!pszRoot)
        return ERROR_INVALID_HANDLE;

    if (
        (!isFirst && !VReg__OutAppend(pAllocContext, pOut, VREG__NEWLINE))
        || !VReg__OutAppend(pAllocContext, pOut, isDelete ? L"[-" : L"[")
        || !VReg__OutAppend(pAllocContext, pOut, pszRoot)
        || (path.cch != 0 && !VReg__WStringAppendChar(pAllocContext, pOut, L'\\'))
        || !VReg__WStringAppend(pAllocContext, pOut, path.pch, path.cch)
        || !VReg__OutAppend(pAllocContext, pOut, L"]")
        || !VReg__OutAppend(pAllocContext, pOut, VREG__NEWLINE)
    )
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

/// Whether @p pLeft and @p pRight name the same key.
static BOOL VReg__SameKey(_In_ const VRegOperation* const pLeft, _In_ const VRegOperation* const pRight)
{
    return pLeft->hRoot == pRight->hRoot
        && VReg__CompareNames(VReg__ViewOf(pLeft->pszPath), VReg__ViewOf(pRight->pszPath)) == 0;
}

VREGDEF LSTATUS WINAPI VRegWriteScript(
    _In_ const VRegScript* const pScript,
    _In_ const VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** const ppBytes,
    _Out_ SIZE_T* const pcbBytes
)
{
    void* const pAllocContext = pScript->pAllocContext;
    VReg__WString out = {NULL, 0, 0};
    VReg__Buffer file = {NULL, 0, 0};
    const VRegOperation* pLastKey = NULL;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    *ppBytes = NULL;
    *pcbBytes = 0;
    if (
        !VReg__OutAppend(pAllocContext, &out, format == VREG_FORMAT_REGEDIT4 ? L"REGEDIT4" : L"Windows Registry Editor Version 5.00")
        || !VReg__OutAppend(pAllocContext, &out, VREG__NEWLINE)
        || !VReg__OutAppend(pAllocContext, &out, VREG__NEWLINE)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    for (i = 0; i < pScript->operations.count; ++i)
    {
        const VRegOperation* const pOperation = &VREG__AT(&pScript->operations, VRegOperation, i);
        const BOOL isKey = pOperation->kind == VREG_OPERATION_CREATE_KEY || pOperation->kind == VREG_OPERATION_DELETE_KEY;
        if (VReg__HasLineBreak(VReg__ViewOf(pOperation->pszPath)) || VReg__HasLineBreak(VReg__ViewOf(pOperation->pszName)))
        {
            status = ERROR_INVALID_NAME;
            goto done;
        }

        if (isKey || !pLastKey || !VReg__SameKey(pLastKey, pOperation))
        {
            status = VReg__AppendKeyLine(pAllocContext, &out, pOperation, pOperation->kind == VREG_OPERATION_DELETE_KEY, i == 0);
            if (status != ERROR_SUCCESS)
                goto done;

            pLastKey = pOperation;
        }

        if (!isKey && !VReg__AppendValue(pAllocContext, &out, pOperation))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    if (pScript->operations.count != 0 && !VReg__OutAppend(pAllocContext, &out, VREG__NEWLINE))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    status = VReg__EncodeFile(pAllocContext, &out, format, &file);
    if (status != ERROR_SUCCESS)
        goto done;

    // The header alone makes the file non-empty, so the encoding allocated.
    _Analysis_assume_(file.pb != NULL);
    *ppBytes = file.pb;
    *pcbBytes = file.cb;
    file.pb = NULL;

done:
    VReg__WStringFree(pAllocContext, &out);
    VReg__BufferFree(pAllocContext, &file);
    return status;
}

VREGDEF LSTATUS WINAPI VRegWriteScriptFile(_In_ const VRegScript* const pScript, _In_ const VRegFormat format, _In_z_ const LPCWSTR pszPath)
{
    void* pBytes = NULL;
    SIZE_T cbBytes = 0;
    LSTATUS status = VRegWriteScript(pScript, format, &pBytes, &cbBytes);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg__WriteWholeFile(pszPath, pBytes, cbBytes);
    VRegFreeScriptBytes(pScript, pBytes);
    return status;
}

#endif // VREG_IMPLEMENTATION_INCLUDED
#endif // VREG_IMPLEMENTATION

#ifdef VREG_WITH_DETOURS
#ifdef VREG__HOOKS_IMPLEMENTATION
#ifndef VREG__HOOKS_IMPLEMENTATION_INCLUDED
#define VREG__HOOKS_IMPLEMENTATION_INCLUDED

// ==========================================================================
// Hooks
// ==========================================================================

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

#endif // VREG__HOOKS_IMPLEMENTATION_INCLUDED
#endif // VREG__HOOKS_IMPLEMENTATION
#endif // VREG_WITH_DETOURS
