/// @file vreg.h
/// @brief A virtual Windows registry for C, as an overlay over the real one, or
///  held entirely in memory.
/// @author Johannes Wüller
///
/// @details Include it from any C or C++ file. In exactly one C file per
///  module, define @ref VREG_IMPLEMENTATION before including it to compile the
///  library there; it links `advapi32` by itself. @ref VREG_STATIC makes every
///  function static, and @ref VREG_MALLOC, @ref VREG_REALLOC and @ref VREG_FREE
///  replace the allocator (see "Allocation" below).
///
///  Defining @ref VREG_WITH_DETOURS wherever it is included adds the hooks
///  (@ref VRegHooks_Install and its companions). That needs Microsoft Detours:
///  `<detours/detours.h>` on the include path, and its library linked.
///
///  The file holds, in order: the public declarations, the hooks'
///  declarations, the private declarations, the implementation, and the
///  hooks' implementation. Names starting `VRegInternal` or `VREG_INTERNAL_`
///  are private.
///
/// @copyright MIT License.
/// @verbatim
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

#ifndef VREG_H_INCLUDED
#define VREG_H_INCLUDED

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================
// Version
// ==========================================================================

// Raised with every change to this file; the tweak is always 0.
#define VREG_VERSION_MAJOR 0
#define VREG_VERSION_MINOR 13
#define VREG_VERSION_PATCH 0
#define VREG_VERSION_TWEAK 0

// Packed as MAKEDLLVERULL packs them, spelled out so no Shlwapi.h is needed.
#define VREG_VERSION (                                                         \
    ((ULONGLONG)VREG_VERSION_MAJOR << 48)                                      \
    | ((ULONGLONG)VREG_VERSION_MINOR << 32)                                    \
    | ((ULONGLONG)VREG_VERSION_PATCH << 16)                                    \
    | (ULONGLONG)VREG_VERSION_TWEAK                                            \
)

// ==========================================================================
// Linkage
// ==========================================================================

// What every VReg declaration carries: the linkage of the functions.
// Defining @ref VREG_STATIC wherever the header is included (the
// implementation too) makes every function `static`, so a module can
// hold a private copy beside another module's; two copies in one module
// behave as two copies and share nothing but the handle namespace.

#ifdef VREG_STATIC
#define VREG_DECL static
#else
#define VREG_DECL extern
#endif

// ==========================================================================
// Handles
// ==========================================================================

/// Whether @p hKey is a virtual handle: nonzero, odd and at most
/// `0x7FFFFFFF`, a range no real key handle and no predefined root lies in.
VREG_DECL BOOL WINAPI VReg_IsVirtualHKey(
    _In_opt_ HKEY hKey
);

// ==========================================================================
// The registry function table
// ==========================================================================

/**
 * @brief A registry: the Win32 `Reg*` family behind one opaque handle.
 *
 *  @ref VReg_CreateNative and @ref VReg_CreateVirtual make the two registries
 *  VReg implements; @ref VReg_CreateCustom makes one over a @ref VRegVtbl and
 *  an object of the caller's, which a wrapper uses to sit in front of another
 *  registry. Callers go through the flat functions (`VReg_OpenKeyExW` and so
 *  on), never through a table directly, and free every registry with
 *  @ref VReg_Destroy.
 *
 * @note Once hooked, every slot may run under a foreign loader lock: an
 *  implementation MUST NOT load a module, create a thread or wait on a lock a
 *  loader-lock holder could own.
 */
typedef struct VReg VReg;

typedef struct VRegVtbl VRegVtbl;

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
typedef LSTATUS WINAPI VRegOpenKeyExWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _Out_ PHKEY phkResult
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
typedef LSTATUS WINAPI VRegCreateKeyExWFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegCloseKeyFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
typedef LSTATUS WINAPI VRegQueryValueExWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
typedef LSTATUS WINAPI VRegSetValueExWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Reserved_ DWORD Reserved,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* lpData,
    _In_ DWORD cbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
typedef LSTATUS WINAPI VRegDeleteValueWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
typedef LSTATUS WINAPI VRegDeleteKeyExWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
typedef LSTATUS WINAPI VRegDeleteTreeWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
typedef LSTATUS WINAPI VRegEnumKeyExWFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegEnumValueWFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegQueryInfoKeyWFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegLoadMUIStringWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
    _In_ DWORD cbOutBuf,
    _Out_opt_ LPDWORD pcbData,
    _In_ DWORD Flags,
    _In_opt_ LPCWSTR pszDirectory
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
typedef LSTATUS WINAPI VRegGetValueWFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegNotifyChangeKeyValueFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ BOOL bWatchSubtree,
    _In_ DWORD dwNotifyFilter,
    _In_opt_ HANDLE hEvent,
    _In_ BOOL fAsynchronous
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
typedef LSTATUS WINAPI VRegFlushKeyFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
typedef LSTATUS WINAPI VRegOpenKeyExAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _Out_ PHKEY phkResult
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
typedef LSTATUS WINAPI VRegCreateKeyExAFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegQueryValueExAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
typedef LSTATUS WINAPI VRegSetValueExAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName,
    _Reserved_ DWORD Reserved,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* lpData,
    _In_ DWORD cbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
typedef LSTATUS WINAPI VRegDeleteValueAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
typedef LSTATUS WINAPI VRegDeleteKeyExAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
typedef LSTATUS WINAPI VRegDeleteTreeAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
typedef LSTATUS WINAPI VRegEnumKeyExAFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegEnumValueAFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegQueryInfoKeyAFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegLoadMUIStringAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) LPSTR pszOutBuf,
    _In_ DWORD cbOutBuf,
    _Out_opt_ LPDWORD pcbData,
    _In_ DWORD Flags,
    _In_opt_ LPCSTR pszDirectory
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
typedef LSTATUS WINAPI VRegGetValueAFunction(
    _In_ VReg* pRegistry,
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
typedef LSTATUS WINAPI VRegOpenKeyAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
typedef LSTATUS WINAPI VRegOpenKeyWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
typedef LSTATUS WINAPI VRegCreateKeyAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
typedef LSTATUS WINAPI VRegCreateKeyWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_ PHKEY phkResult
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
typedef LSTATUS WINAPI VRegQueryValueAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPSTR lpData,
    _Inout_opt_ PLONG lpcbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
typedef LSTATUS WINAPI VRegQueryValueWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPWSTR lpData,
    _Inout_opt_ PLONG lpcbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
typedef LSTATUS WINAPI VRegSetValueAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCSTR lpData,
    _In_ DWORD cbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
typedef LSTATUS WINAPI VRegSetValueWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCWSTR lpData,
    _In_ DWORD cbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
typedef LSTATUS WINAPI VRegDeleteKeyAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
typedef LSTATUS WINAPI VRegDeleteKeyWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
typedef LSTATUS WINAPI VRegEnumKeyAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_opt_(cchName) LPSTR lpName,
    _In_ DWORD cchName
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
typedef LSTATUS WINAPI VRegEnumKeyWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_writes_opt_(cchName) LPWSTR lpName,
    _In_ DWORD cchName
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
typedef LSTATUS WINAPI VRegSetKeyValueAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValueName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCVOID lpData,
    _In_ DWORD cbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
typedef LSTATUS WINAPI VRegSetKeyValueWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValueName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCVOID lpData,
    _In_ DWORD cbData
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
typedef LSTATUS WINAPI VRegDeleteKeyValueAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_opt_ LPCSTR lpValueName
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
typedef LSTATUS WINAPI VRegDeleteKeyValueWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValueName
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
typedef LSTATUS WINAPI VRegQueryMultipleValuesAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_(num_vals) PVALENTA val_list,
    _In_ DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPSTR lpValueBuf,
    _Inout_opt_ LPDWORD ldwTotsize
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
typedef LSTATUS WINAPI VRegQueryMultipleValuesWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _Out_writes_(num_vals) PVALENTW val_list,
    _In_ DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) LPWSTR lpValueBuf,
    _Inout_opt_ LPDWORD ldwTotsize
);

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
struct VRegVtbl
{
    /// Frees what the context owns, before @ref VReg_Destroy frees the
    /// registry. Null for a context its creator frees some other way.
    void (WINAPI* Destroy)(
        _In_ VReg* pThis
    );

    VRegOpenKeyExWFunction* RegOpenKeyExW;
    VRegCreateKeyExWFunction* RegCreateKeyExW;
    VRegCloseKeyFunction* RegCloseKey;
    VRegQueryValueExWFunction* RegQueryValueExW;
    VRegSetValueExWFunction* RegSetValueExW;
    VRegDeleteValueWFunction* RegDeleteValueW;
    VRegDeleteKeyExWFunction* RegDeleteKeyExW;
    VRegDeleteTreeWFunction* RegDeleteTreeW;
    VRegEnumKeyExWFunction* RegEnumKeyExW;
    VRegEnumValueWFunction* RegEnumValueW;
    VRegQueryInfoKeyWFunction* RegQueryInfoKeyW;
    VRegLoadMUIStringWFunction* RegLoadMUIStringW;
    VRegGetValueWFunction* RegGetValueW;
    VRegNotifyChangeKeyValueFunction* RegNotifyChangeKeyValue;
    VRegFlushKeyFunction* RegFlushKey;
    VRegOpenKeyExAFunction* RegOpenKeyExA;
    VRegCreateKeyExAFunction* RegCreateKeyExA;
    VRegQueryValueExAFunction* RegQueryValueExA;
    VRegSetValueExAFunction* RegSetValueExA;
    VRegDeleteValueAFunction* RegDeleteValueA;
    VRegDeleteKeyExAFunction* RegDeleteKeyExA;
    VRegDeleteTreeAFunction* RegDeleteTreeA;
    VRegEnumKeyExAFunction* RegEnumKeyExA;
    VRegEnumValueAFunction* RegEnumValueA;
    VRegQueryInfoKeyAFunction* RegQueryInfoKeyA;
    VRegLoadMUIStringAFunction* RegLoadMUIStringA;
    VRegGetValueAFunction* RegGetValueA;
    VRegOpenKeyAFunction* RegOpenKeyA;
    VRegOpenKeyWFunction* RegOpenKeyW;
    VRegCreateKeyAFunction* RegCreateKeyA;
    VRegCreateKeyWFunction* RegCreateKeyW;
    VRegQueryValueAFunction* RegQueryValueA;
    VRegQueryValueWFunction* RegQueryValueW;
    VRegSetValueAFunction* RegSetValueA;
    VRegSetValueWFunction* RegSetValueW;
    VRegDeleteKeyAFunction* RegDeleteKeyA;
    VRegDeleteKeyWFunction* RegDeleteKeyW;
    VRegEnumKeyAFunction* RegEnumKeyA;
    VRegEnumKeyWFunction* RegEnumKeyW;
    VRegSetKeyValueAFunction* RegSetKeyValueA;
    VRegSetKeyValueWFunction* RegSetKeyValueW;
    VRegDeleteKeyValueAFunction* RegDeleteKeyValueA;
    VRegDeleteKeyValueWFunction* RegDeleteKeyValueW;
    VRegQueryMultipleValuesAFunction* RegQueryMultipleValuesA;
    VRegQueryMultipleValuesWFunction* RegQueryMultipleValuesW;
};

VREG_DECL VRegOpenKeyExWFunction VReg_OpenKeyExW;
VREG_DECL VRegCreateKeyExWFunction VReg_CreateKeyExW;
VREG_DECL VRegCloseKeyFunction VReg_CloseKey;
VREG_DECL VRegQueryValueExWFunction VReg_QueryValueExW;
VREG_DECL VRegSetValueExWFunction VReg_SetValueExW;
VREG_DECL VRegDeleteValueWFunction VReg_DeleteValueW;
VREG_DECL VRegDeleteKeyExWFunction VReg_DeleteKeyExW;
VREG_DECL VRegDeleteTreeWFunction VReg_DeleteTreeW;
VREG_DECL VRegEnumKeyExWFunction VReg_EnumKeyExW;
VREG_DECL VRegEnumValueWFunction VReg_EnumValueW;
VREG_DECL VRegQueryInfoKeyWFunction VReg_QueryInfoKeyW;
VREG_DECL VRegLoadMUIStringWFunction VReg_LoadMUIStringW;
VREG_DECL VRegGetValueWFunction VReg_GetValueW;
VREG_DECL VRegNotifyChangeKeyValueFunction VReg_NotifyChangeKeyValue;
VREG_DECL VRegFlushKeyFunction VReg_FlushKey;
VREG_DECL VRegOpenKeyExAFunction VReg_OpenKeyExA;
VREG_DECL VRegCreateKeyExAFunction VReg_CreateKeyExA;
VREG_DECL VRegQueryValueExAFunction VReg_QueryValueExA;
VREG_DECL VRegSetValueExAFunction VReg_SetValueExA;
VREG_DECL VRegDeleteValueAFunction VReg_DeleteValueA;
VREG_DECL VRegDeleteKeyExAFunction VReg_DeleteKeyExA;
VREG_DECL VRegDeleteTreeAFunction VReg_DeleteTreeA;
VREG_DECL VRegEnumKeyExAFunction VReg_EnumKeyExA;
VREG_DECL VRegEnumValueAFunction VReg_EnumValueA;
VREG_DECL VRegQueryInfoKeyAFunction VReg_QueryInfoKeyA;
VREG_DECL VRegLoadMUIStringAFunction VReg_LoadMUIStringA;
VREG_DECL VRegGetValueAFunction VReg_GetValueA;
VREG_DECL VRegOpenKeyAFunction VReg_OpenKeyA;
VREG_DECL VRegOpenKeyWFunction VReg_OpenKeyW;
VREG_DECL VRegCreateKeyAFunction VReg_CreateKeyA;
VREG_DECL VRegCreateKeyWFunction VReg_CreateKeyW;
VREG_DECL VRegQueryValueAFunction VReg_QueryValueA;
VREG_DECL VRegQueryValueWFunction VReg_QueryValueW;
VREG_DECL VRegSetValueAFunction VReg_SetValueA;
VREG_DECL VRegSetValueWFunction VReg_SetValueW;
VREG_DECL VRegDeleteKeyAFunction VReg_DeleteKeyA;
VREG_DECL VRegDeleteKeyWFunction VReg_DeleteKeyW;
VREG_DECL VRegEnumKeyAFunction VReg_EnumKeyA;
VREG_DECL VRegEnumKeyWFunction VReg_EnumKeyW;
VREG_DECL VRegSetKeyValueAFunction VReg_SetKeyValueA;
VREG_DECL VRegSetKeyValueWFunction VReg_SetKeyValueW;
VREG_DECL VRegDeleteKeyValueAFunction VReg_DeleteKeyValueA;
VREG_DECL VRegDeleteKeyValueWFunction VReg_DeleteKeyValueW;
VREG_DECL VRegQueryMultipleValuesAFunction VReg_QueryMultipleValuesA;
VREG_DECL VRegQueryMultipleValuesWFunction VReg_QueryMultipleValuesW;

/// The neutral names select the wide or the ANSI form by @ref UNICODE, as
/// `winreg.h` does for the functions they mirror.
#ifdef UNICODE
#define VReg_CreateKey VReg_CreateKeyW
#define VReg_CreateKeyEx VReg_CreateKeyExW
#define VReg_DeleteKey VReg_DeleteKeyW
#define VReg_DeleteKeyEx VReg_DeleteKeyExW
#define VReg_DeleteKeyValue VReg_DeleteKeyValueW
#define VReg_DeleteTree VReg_DeleteTreeW
#define VReg_DeleteValue VReg_DeleteValueW
#define VReg_EnumKey VReg_EnumKeyW
#define VReg_EnumKeyEx VReg_EnumKeyExW
#define VReg_EnumValue VReg_EnumValueW
#define VReg_GetValue VReg_GetValueW
#define VReg_LoadMUIString VReg_LoadMUIStringW
#define VReg_OpenKey VReg_OpenKeyW
#define VReg_OpenKeyEx VReg_OpenKeyExW
#define VReg_QueryInfoKey VReg_QueryInfoKeyW
#define VReg_QueryMultipleValues VReg_QueryMultipleValuesW
#define VReg_QueryValue VReg_QueryValueW
#define VReg_QueryValueEx VReg_QueryValueExW
#define VReg_SetKeyValue VReg_SetKeyValueW
#define VReg_SetValue VReg_SetValueW
#define VReg_SetValueEx VReg_SetValueExW
#else
#define VReg_CreateKey VReg_CreateKeyA
#define VReg_CreateKeyEx VReg_CreateKeyExA
#define VReg_DeleteKey VReg_DeleteKeyA
#define VReg_DeleteKeyEx VReg_DeleteKeyExA
#define VReg_DeleteKeyValue VReg_DeleteKeyValueA
#define VReg_DeleteTree VReg_DeleteTreeA
#define VReg_DeleteValue VReg_DeleteValueA
#define VReg_EnumKey VReg_EnumKeyA
#define VReg_EnumKeyEx VReg_EnumKeyExA
#define VReg_EnumValue VReg_EnumValueA
#define VReg_GetValue VReg_GetValueA
#define VReg_LoadMUIString VReg_LoadMUIStringA
#define VReg_OpenKey VReg_OpenKeyA
#define VReg_OpenKeyEx VReg_OpenKeyExA
#define VReg_QueryInfoKey VReg_QueryInfoKeyA
#define VReg_QueryMultipleValues VReg_QueryMultipleValuesA
#define VReg_QueryValue VReg_QueryValueA
#define VReg_QueryValueEx VReg_QueryValueExA
#define VReg_SetKeyValue VReg_SetKeyValueA
#define VReg_SetValue VReg_SetValueA
#define VReg_SetValueEx VReg_SetValueExA
#endif

/// Makes a registry whose calls go to the slots of @p pVtbl, which MUST
/// outlive it. @p pvContext is the object of the implementation, which its
/// slots reach through @ref VReg_GetContext. Freed with @ref VReg_Destroy.
VREG_DECL LSTATUS WINAPI VReg_CreateCustom(
    _In_ const VRegVtbl* pVtbl,
    _In_opt_ void* pvContext,
    _Outptr_ VReg** ppRegistry
);

/// The context a registry @ref VReg_CreateCustom made was created with; null
/// for any other registry.
VREG_DECL _Ret_maybenull_ void* WINAPI VReg_GetContext(
    _In_ VReg* pRegistry
);

/// Frees a registry: a custom one calls its `Destroy` slot first, when it has
/// one. Null is ignored.
VREG_DECL void WINAPI VReg_Destroy(
    _In_opt_ VReg* pRegistry
);

// ==========================================================================
// Hives
// ==========================================================================

/**
 * @brief Storage for one subtree of a virtual namespace: a tree with one
 *  anonymous root, mounted at a path by a virtual registry, held in memory
 *  on its own, layered over a key of another registry, or answered by the
 *  slots of a @ref VRegHiveVtbl (@ref VRegHive_CreateCustom).
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
 *  Each key has exactly one virtual handle, issued when VReg first creates
 *  or opens it. Readers take the hive's lock shared and writers exclusive,
 *  until @ref VRegHive_Freeze; after that nothing writes and the readers of a
 *  hive VReg holds itself take no lock at all. The lower (a registry or a
 *  provider's slots) is never called with the lock held, so a lower call
 *  that takes a long time cannot hold up the hive.
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

/// A hive held entirely in memory; starts empty.
VREG_DECL LSTATUS WINAPI VRegHive_Create(
    _Outptr_ VRegHive** ppHive
);

/// A hive over the key @p pszLowerPath below @p hLowerRoot of @p pLower,
/// which MUST outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
/// @ref KEY_WOW64_32KEY or 0) is added to every lower open.
VREG_DECL LSTATUS WINAPI VRegHive_CreateLayered(
    _In_ VReg* pLower,
    _In_ HKEY hLowerRoot,
    _In_opt_ LPCWSTR pszLowerPath,
    _In_ REGSAM samLowerView,
    _Outptr_ VRegHive** ppHive
);

/// Frees the hive and unbinds every handle it issued; null is ignored.
VREG_DECL void WINAPI VRegHive_Destroy(
    _In_opt_ VRegHive* pHive
);

/// Makes the hive immutable from now on, so its readers take no lock and its
/// pending watches are dropped without firing. A hive
/// @ref VRegHive_CreateCustom made only refuses writes from then on: it keeps
/// locking to add the records of keys it opens, and keeps its watches, which
/// its provider still fires (@ref VRegHive_NotifyChange). Safe while it is
/// mounted and in use; there is no way back.
VREG_DECL void WINAPI VRegHive_Freeze(
    _In_ VRegHive* pHive
);

// ==========================================================================
// Custom hives
// ==========================================================================

/**
 * @brief A key of a hive: the record VReg keeps for it, as a slot of a
 *  @ref VRegHiveVtbl receives it.
 *
 *  Opaque, and valid for the life of the hive: VReg adds a record the first
 *  time it opens or creates a key, keeps it until the hive is destroyed, and
 *  answers every later call on that path with it. A provider over an
 *  unbounded key space grows by one record per distinct key opened.
 */
typedef struct VRegInternalNode VRegHiveKey;

/// Where a slot hands VReg what it answers, through the
/// `VRegHiveResult_Set*` functions. Opaque; valid only during the slot call.
typedef struct VRegHiveResult VRegHiveResult;

/// Whether the key at @p pszSubKey below @p pParent exists:
/// @ref ERROR_SUCCESS or @ref ERROR_FILE_NOT_FOUND. @p pszSubKey is one name;
/// @p ppToken receives what the provider wants to keep for the key, which
/// @ref VRegHiveKey_GetToken answers from then on (null is fine).
typedef LSTATUS WINAPI VRegHiveOpenKeyFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pParent,
    _In_ LPCWSTR pszSubKey,
    _Outptr_result_maybenull_ void** ppToken
);

/// Creates the key @p pszSubKey (one name) below @p pParent, or opens it
/// when it exists; @p pIsCreated says which. @p dwOptions are those of
/// @ref RegCreateKeyExW.
typedef LSTATUS WINAPI VRegHiveCreateKeyFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pParent,
    _In_ LPCWSTR pszSubKey,
    _In_ LPCWSTR pszClassName,
    _In_ DWORD dwOptions,
    _Outptr_result_maybenull_ void** ppToken,
    _Out_ BOOL* pIsCreated
);

/// Deletes @p pKey, which MUST answer @ref ERROR_ACCESS_DENIED while it has
/// subkeys, as native does.
typedef LSTATUS WINAPI VRegHiveDeleteKeyFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey
);

/// Deletes every subkey and value of @p pKey, leaving the key itself.
typedef LSTATUS WINAPI VRegHiveDeleteTreeFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey
);

/// The type and data of value @p pszName of @p pKey, through
/// @ref VRegHiveResult_SetValue. Two reads of a value MAY differ.
typedef LSTATUS WINAPI VRegHiveQueryValueFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey,
    _In_ LPCWSTR pszName,
    _In_ VRegHiveResult* pResult
);

typedef LSTATUS WINAPI VRegHiveSetValueFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey,
    _In_ LPCWSTR pszName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    _In_ DWORD cbData
);

typedef LSTATUS WINAPI VRegHiveDeleteValueFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey,
    _In_ LPCWSTR pszName
);

/// Subkey @p dwIndex of @p pKey, through @ref VRegHiveResult_SetName and, if
/// the provider has them, @ref VRegHiveResult_SetClass and
/// @ref VRegHiveResult_SetLastWrite; @ref ERROR_NO_MORE_ITEMS past the last.
/// The order MUST stay stable while nothing changes; native enumerates
/// subkeys by name.
typedef LSTATUS WINAPI VRegHiveEnumKeyFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey,
    _In_ DWORD dwIndex,
    _In_ VRegHiveResult* pResult
);

/// Value @p dwIndex of @p pKey, through @ref VRegHiveResult_SetName and
/// @ref VRegHiveResult_SetValue; @ref ERROR_NO_MORE_ITEMS past the last. The
/// order MUST stay stable while nothing changes; native enumerates values in
/// creation order.
typedef LSTATUS WINAPI VRegHiveEnumValueFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey,
    _In_ DWORD dwIndex,
    _In_ VRegHiveResult* pResult
);

/// What `RegQueryInfoKeyW` reports for @p pKey, through
/// @ref VRegHiveResult_SetInfo and, if the provider has them,
/// @ref VRegHiveResult_SetClass and @ref VRegHiveResult_SetLastWrite.
typedef LSTATUS WINAPI VRegHiveQueryInfoFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey,
    _In_ VRegHiveResult* pResult
);

/// What `RegFlushKey` on @p pKey answers.
typedef LSTATUS WINAPI VRegHiveCheckKeyFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey
);

/// Frees what the context owns, before VReg frees the hive.
typedef void WINAPI VRegHiveDestroyFunction(
    _In_ VRegHive* pHive
);

/**
 * @brief The slots of a hive @ref VRegHive_CreateCustom makes: what the hive
 *  holds, answered on demand, while VReg keeps every Win32 rule (handles,
 *  caller buffers, ANSI and wide, views, mounts).
 *
 *  `OpenKey`, `QueryValue`, `EnumKey` and `EnumValue` are required. A null
 *  `CreateKey` opens an existing key and answers a missing one
 *  @ref ERROR_ACCESS_DENIED, as a frozen hive does; a null `DeleteKey`,
 *  `SetValue` or `DeleteValue` answers @ref ERROR_ACCESS_DENIED. A null
 *  `DeleteTree` is derived from `EnumKey`, `DeleteKey`, `EnumValue` and
 *  `DeleteValue` as native `RegDeleteTree` works: subkeys in enumeration
 *  order, each subtree before its key, then the values, stopping at the
 *  first failure; a call after a failure continues from what is left. A
 *  null `QueryInfo` is derived by enumerating keys and values once; a null
 *  `CheckKey` succeeds; a null `Destroy` does nothing.
 *
 *  A slot receives names as the caller passed them and MUST compare them as
 *  the registry does (@ref VReg_CompareNames). Slots MAY run on any thread,
 *  concurrently, and under a foreign loader lock: they MUST NOT load a
 *  module, create a thread or wait on a lock a loader-lock holder could own.
 *  VReg holds no lock of its own across a slot call; the provider locks its
 *  own state. A slot never sees the access a key was opened with.
 */
typedef struct VRegHiveVtbl
{
    VRegHiveOpenKeyFunction* OpenKey;
    VRegHiveCreateKeyFunction* CreateKey;
    VRegHiveDeleteKeyFunction* DeleteKey;
    VRegHiveDeleteTreeFunction* DeleteTree;
    VRegHiveQueryValueFunction* QueryValue;
    VRegHiveSetValueFunction* SetValue;
    VRegHiveDeleteValueFunction* DeleteValue;
    VRegHiveEnumKeyFunction* EnumKey;
    VRegHiveEnumValueFunction* EnumValue;
    VRegHiveQueryInfoFunction* QueryInfo;
    VRegHiveCheckKeyFunction* CheckKey;
    VRegHiveDestroyFunction* Destroy;
} VRegHiveVtbl;

/// Makes a hive whose keys and values the slots of @p pVtbl answer, which
/// MUST outlive it; writes go through to the slots. @p pvContext is the
/// provider's object, which its slots reach through
/// @ref VRegHive_GetContext. Answers @ref ERROR_INVALID_PARAMETER when a
/// required slot is null. Freed with @ref VRegHive_Destroy.
VREG_DECL LSTATUS WINAPI VRegHive_CreateCustom(
    _In_ const VRegHiveVtbl* pVtbl,
    _In_opt_ void* pvContext,
    _Outptr_ VRegHive** ppHive
);

/// The context a hive @ref VRegHive_CreateCustom made was created with; null
/// for any other hive.
VREG_DECL _Ret_maybenull_ void* WINAPI VRegHive_GetContext(
    _In_ VRegHive* pHive
);

/// What the provider's `OpenKey` or `CreateKey` kept for @p pKey; null for
/// the root and for a key it kept nothing for.
VREG_DECL _Ret_maybenull_ void* WINAPI VRegHiveKey_GetToken(
    _In_ VRegHiveKey* pKey
);

/// The name of @p pKey as it was first opened or created; empty for the
/// root.
VREG_DECL _Ret_notnull_ LPCWSTR WINAPI VRegHiveKey_GetName(
    _In_ VRegHiveKey* pKey
);

/// The key @p pKey is below; null for the root.
VREG_DECL _Ret_maybenull_ VRegHiveKey* WINAPI VRegHiveKey_GetParent(
    _In_ VRegHiveKey* pKey
);

/// Whether @p pKey is the key at @p pszPath below the hive's root: the
/// components, separated by single backslashes, compared one by one as
/// @ref VReg_CompareNames compares names, so a provider joins and stores
/// nothing. The root is the empty path.
VREG_DECL BOOL WINAPI VRegHiveKey_EqualsPath(
    _In_ VRegHiveKey* pKey,
    _In_ LPCWSTR pszPath
);

/// Hands VReg a value: its type, and @p cbData bytes of data, which are
/// copied. Answers @ref ERROR_OUTOFMEMORY when the copy fails, which the slot
/// returns; every sink answers @ref ERROR_SUCCESS for what VReg did not ask
/// for, and does nothing.
VREG_DECL LSTATUS WINAPI VRegHiveResult_SetValue(
    _In_ VRegHiveResult* pResult,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    _In_ DWORD cbData
);

/// Hands VReg the name of an enumerated key or value, copied.
VREG_DECL LSTATUS WINAPI VRegHiveResult_SetName(
    _In_ VRegHiveResult* pResult,
    _In_ LPCWSTR pszName
);

/// Hands VReg the class of a key, copied.
VREG_DECL LSTATUS WINAPI VRegHiveResult_SetClass(
    _In_ VRegHiveResult* pResult,
    _In_ LPCWSTR pszClassName
);

/// Hands VReg the last write time of a key.
VREG_DECL LSTATUS WINAPI VRegHiveResult_SetLastWrite(
    _In_ VRegHiveResult* pResult,
    _In_ const FILETIME* pLastWrite
);

/// Hands VReg what `RegQueryInfoKeyW` counts for a key: lengths in
/// characters without terminators, @p cbMaxValue in bytes.
VREG_DECL LSTATUS WINAPI VRegHiveResult_SetInfo(
    _In_ VRegHiveResult* pResult,
    _In_ DWORD cSubKeys,
    _In_ DWORD cchMaxSubKey,
    _In_ DWORD cchMaxClass,
    _In_ DWORD cValues,
    _In_ DWORD cchMaxValueName,
    _In_ DWORD cbMaxValue
);

/// Compares two key or value names as the registry does: ordinal,
/// case-insensitive by uppercase. Negative, zero or positive.
VREG_DECL int WINAPI VReg_CompareNames(
    _In_ LPCWSTR pszLeft,
    _In_ LPCWSTR pszRight
);

/**
 * @brief Tells VReg that the provider's data changed on its own, so the
 *  watches @p dwFilter selects fire as for a write through the hive.
 *
 *  @ref REG_NOTIFY_CHANGE_NAME is for a subkey of @p pKey added or deleted,
 *  @ref REG_NOTIFY_CHANGE_LAST_SET for a value of @p pKey set or deleted. The
 *  watches on @p pKey that the filter selects fire, and so do the subtree
 *  watches on the keys above it; each watch fires once and is gone. A key the
 *  provider deletes on its own is not marked deleted: the provider notifies
 *  its parent, and the key's record answers whatever the provider answers
 *  from then on.
 *
 *  Loads nothing and waits on nothing but the hive's own lock, so the
 *  provider MAY call it from any thread after its change; it MUST NOT call it
 *  from inside a slot, where VReg may hold that lock.
 *
 * @return @ref ERROR_SUCCESS; @ref ERROR_INVALID_PARAMETER when @p dwFilter
 *  holds neither bit; @ref ERROR_KEY_DELETED for a key deleted through VReg.
 */
VREG_DECL LSTATUS WINAPI VRegHive_NotifyChange(
    _In_ VRegHive* pHive,
    _In_ VRegHiveKey* pKey,
    _In_ DWORD dwFilter
);

// ==========================================================================
// The native registry
// ==========================================================================

// The functions below include Vista's (RegDeleteTreeW, RegGetValueW), which
// the SDK declares only from that target on.
#if _WIN32_WINNT < 0x0600
#error "VReg needs _WIN32_WINNT 0x0600 (Windows Vista) or later."
#endif

/// The entry points the native registry calls and the hooks attach to: the
/// wide `Ex` forms and the functions with one form only. The ANSI, pre-`Ex`
/// and convenience forms are derived from these, as everywhere.
typedef struct
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

/// The real registry, reached through a table filled from the export tables
/// of `KernelBase.dll` and, for the pre-`Ex` forms only it exports,
/// `advapi32.dll`, never through @ref GetProcAddress: an apphelp shim can make
/// that answer a stub instead of the entry point. Resolution never loads a
/// module (VReg links advapi32, so both are loaded before it runs), which
/// makes it safe under the loader lock. A function neither module exports
/// stays null. Freed with @ref VReg_Destroy.
VREG_DECL LSTATUS WINAPI VReg_CreateNative(
    _Outptr_ VReg** ppRegistry
);

/// The live table of a registry @ref VReg_CreateNative made: once the
/// hooks are committed it holds the trampolines, and the registry is the one
/// below them. An entry replaced before anything uses the registry is the
/// one it calls.
VREG_DECL _Ret_notnull_ VRegNativeFunctions* WINAPI VReg_GetNativeFunctions(
    _In_ VReg* pRegistry
);

// ==========================================================================
// The virtual registry
// ==========================================================================

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
 *  entirely in memory. The `ntdll` export for native path resolution is found
 *  here, so resolving a path never loads a module. Freed with
 *  @ref VReg_Destroy.
 */
VREG_DECL LSTATUS WINAPI VReg_CreateVirtual(
    _In_opt_ VReg* pBase,
    _Outptr_ VReg** ppRegistry
);

/// Mounts @p pHive at @p pszPath below the predefined key @p hRoot (a null or
/// empty path mounts the root itself) in a registry
/// @ref VReg_CreateVirtual made. @p pHive MUST outlive the registry.
/// Answers @ref ERROR_INVALID_HANDLE for a root that is not predefined or a
/// registry that is not virtual, @ref ERROR_BAD_PATHNAME for a malformed path
/// and @ref ERROR_ALREADY_EXISTS for a path already mounted. @p samView limits
/// the calls the mount answers to those of one registry view:
/// @ref KEY_WOW64_64KEY or @ref KEY_WOW64_32KEY, or 0 for both; anything else
/// answers @ref ERROR_INVALID_PARAMETER.
VREG_DECL LSTATUS WINAPI VReg_Mount(
    _In_ VReg* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_ LPCWSTR pszPath,
    _In_ VRegHive* pHive,
    _In_ REGSAM samView
);

// ==========================================================================
// .reg scripts
// ==========================================================================

/// The encodings and headers a `.reg` file is written in.
typedef enum
{
    /// `Windows Registry Editor Version 5.00` as UTF-16LE with a BOM: what
    /// regedit writes.
    VREG_FORMAT_UNICODE,

    /// The same header as UTF-8 without a BOM.
    VREG_FORMAT_UTF8,

    /// `REGEDIT4` in @ref CP_ACP.
    VREG_FORMAT_REGEDIT4,
} VRegFormat;

typedef enum
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
typedef struct
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
typedef struct
{
    /// 1-based, in UTF-16 code units of the decoded text.
    SIZE_T line;

    SIZE_T column;

    /// A static string.
    LPCSTR pszMessage;
} VRegParseError;

/**
 * @brief Parses a whole file into a new script.
 *
 *  The encoding is taken from a BOM (UTF-16LE, UTF-8); without one, a
 *  `REGEDIT4` header means @ref CP_ACP and anything else UTF-8. The script is
 *  only created when the whole file parses, and is freed with
 *  @ref VRegScript_Destroy.
 *
 * @return @ref ERROR_SUCCESS, @ref ERROR_INVALID_DATA on a syntax or encoding
 *  error (described by @p pError, which MAY be null and is zeroed for any
 *  other outcome), or @ref ERROR_OUTOFMEMORY.
 */
VREG_DECL LSTATUS WINAPI VRegScript_Parse(
    _In_reads_bytes_(cbFile) const void* pFile,
    _In_ SIZE_T cbFile,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Reads the file @p pszPath, then @ref VRegScript_Parse. Failures to read
/// answer the Win32 error.
VREG_DECL LSTATUS WINAPI VRegScript_ParseFile(
    _In_ LPCWSTR pszPath,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Frees a script; null is ignored.
VREG_DECL void WINAPI VRegScript_Destroy(
    _In_opt_ VRegScript* pScript
);

/// The script's operations, in order, and how many; valid until the script is
/// destroyed.
VREG_DECL _Ret_notnull_ const VRegOperation* WINAPI VRegScript_GetOperations(
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
VREG_DECL LSTATUS WINAPI VReg_ApplyOperations(
    _In_reads_(cOperations) const VRegOperation* pOperations,
    _In_ SIZE_T cOperations,
    _In_ VReg* pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* pFailedIndex
);

/**
 * @brief A script that recreates the key @p pszPath below the predefined root
 *  @p hRoot and everything below it: each key, then its values, then its
 *  subkeys, keys and values sorted ordinal case-insensitive.
 *
 *  Written and applied, the script reproduces every value's bytes. Freed with
 *  @ref VRegScript_Destroy.
 *
 * @return @ref ERROR_INVALID_HANDLE when @p hRoot is not a root a `.reg` file
 *  can name, @ref ERROR_CANTREAD for a key nested deeper than the registry
 *  allows (a cycle through some other registry), otherwise the status of the
 *  registry calls.
 */
VREG_DECL LSTATUS WINAPI VRegScript_Snapshot(
    _In_ VReg* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_ LPCWSTR pszPath,
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
 *  @ref VRegScript_Snapshot. A tombstone with no lower counterpart is kept,
 *  and does nothing when applied. Freed with @ref VRegScript_Destroy.
 *
 * @return As @ref VRegScript_Snapshot, except that the hive is read instead of
 *  a registry.
 */
VREG_DECL LSTATUS WINAPI VRegScript_Delta(
    _In_ VRegHive* pHive,
    _In_ HKEY hRoot,
    _In_opt_ LPCWSTR pszPath,
    _Outptr_ VRegScript** ppScript
);

/**
 * @brief Lays @p cOperations operations out as a regedit-style file in
 *  @p format, into bytes freed with @ref VReg_Free.
 *
 *  Each key operation is a `[key]` or `[-key]` line and each value operation
 *  a `"name"=...` line under it; a value whose key differs from the last key
 *  line gets a `[key]` line of its own first, so operations built by hand
 *  without key operations lay out as a file that sets those values.
 *
 * @return @ref ERROR_NO_UNICODE_TRANSLATION when @p format cannot represent a
 *  name or string, @ref ERROR_INVALID_NAME for a name that holds a line break
 *  (no format can write one), @ref ERROR_INVALID_HANDLE for a root a `.reg`
 *  file cannot name.
 */
VREG_DECL LSTATUS WINAPI VRegScript_Write(
    _In_reads_(cOperations) const VRegOperation* pOperations,
    _In_ SIZE_T cOperations,
    _In_ VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** ppBytes,
    _Out_ SIZE_T* pcbBytes
);

/// Frees memory VReg handed to the caller to free, such as the bytes of
/// @ref VRegScript_Write; null is ignored.
VREG_DECL void WINAPI VReg_Free(
    _In_opt_ void* p
);

/// @ref VRegScript_Write into the file @p pszPath. An existing file is replaced
/// only once the layout has succeeded.
VREG_DECL LSTATUS WINAPI VRegScript_WriteFile(
    _In_reads_(cOperations) const VRegOperation* pOperations,
    _In_ SIZE_T cOperations,
    _In_ VRegFormat format,
    _In_ LPCWSTR pszPath
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

// Routes the process's `Reg*` calls into a @ref VReg (normally
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
// slot. The hooks are never detached: @ref VRegHooks_Neutralize points every
// slot at the trampoline, so the layer passes everything straight through
// while other hookers' chains stay intact in any order.
//
// Hook bodies may run under a foreign loader lock; nothing they reach loads
// a module, creates a thread or waits on a lock a loader-lock holder could
// own. The one load is the call's own: `RegLoadMUIStringW` resolving an
// indirect string loads its module, as it does natively.
//
// @note A hook body can run on any thread at any time, so no module whose
// code a hook can reach is ever safe to unload: the one holding this copy,
// and any module the registry handed to @ref VRegHooks_Install or a hive
// provider lives in. The consumer MUST pin each of them
// (@ref GetModuleHandleExW with @ref GET_MODULE_HANDLE_EX_FLAG_PIN). VReg
// pins nothing, since it cannot know those modules and a pin is
// irreversible; nothing runs at unload either, since neutralizing the
// slots then would protect only callers who arrive after it.

/// Calls @ref DetourAttach for every hook in a Detours transaction of its
/// own, enrolling only the calling thread. Every hook calls through
/// @p pRegistry, which MAY wrap the registry that answers (to trace each
/// call, say). @p pNative is the registry below the hooks, the one
/// @p pRegistry reaches for whatever it does not answer: it MUST be one
/// @ref VReg_CreateNative made, and the commit turns its function table into
/// the trampolines. Both MUST outlive the process, as MUST every module a
/// hook can reach (see the note above). Answers the first Detours error, or
/// @ref ERROR_ALREADY_INITIALIZED when this copy installed before.
VREG_DECL LONG WINAPI VRegHooks_Install(
    _In_ VReg* pRegistry,
    _In_ VReg* pNative
);

/// Makes every hook of this copy pass calls straight through, for good.
VREG_DECL void WINAPI VRegHooks_Neutralize(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VREG_DETOURS_H_INCLUDED
#endif // VREG_WITH_DETOURS

/// The private declarations come with the implementation. @ref VREG_INTERNAL
/// shows them alone, for code built beside the library that reaches into it:
/// its own tests, and the hooks compiled apart from the rest
/// (@ref VREG_INTERNAL_HOOKS_IMPLEMENTATION, as the CMake build's
/// `VReg::Detours` does).
#ifdef VREG_IMPLEMENTATION
#ifndef VREG_INTERNAL
#define VREG_INTERNAL
#endif
#ifdef VREG_WITH_DETOURS
#ifndef VREG_INTERNAL_HOOKS_IMPLEMENTATION
#define VREG_INTERNAL_HOOKS_IMPLEMENTATION
#endif
#endif
#endif

#ifdef VREG_INTERNAL_HOOKS_IMPLEMENTATION
#ifndef VREG_INTERNAL
#define VREG_INTERNAL
#endif
#endif

#ifdef VREG_INTERNAL
#ifndef VREG_INTERNAL_INCLUDED
#define VREG_INTERNAL_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================
// Allocation
// ==========================================================================

// How VReg allocates. A consumer overrides `VREG_MALLOC`,
// `VREG_REALLOC` and `VREG_FREE` together, before the implementation is
// compiled. The defaults use the process heap: no CRT dependency, and
// safe under the loader lock.

#ifndef VREG_MALLOC
#define VREG_MALLOC(cb) \
    HeapAlloc(GetProcessHeap(), 0, (cb))
#endif

#ifndef VREG_REALLOC
#define VREG_REALLOC(p, cb) \
    HeapReAlloc(GetProcessHeap(), 0, (p), (cb))
#endif

#ifndef VREG_FREE
#define VREG_FREE(p) \
    HeapFree(GetProcessHeap(), 0, (p))
#endif

/// Breaks into an attached debugger in a `_DEBUG` build; nothing otherwise.
#ifndef VREG_ASSERT
#ifdef _DEBUG
#define VREG_ASSERT(x) ((x) ? (void)0 : VRegInternal_AssertFailed())
#else
#define VREG_ASSERT(x) ((void)0)
#endif
#endif

VREG_DECL void VRegInternal_AssertFailed(void);

/// Null for @p cb of zero or when memory ran out.
VREG_DECL _Ret_maybenull_ void* VRegInternal_Alloc(
    SIZE_T cb
);

/// Like @ref VRegInternal_Alloc, zero-filled.
VREG_DECL _Ret_maybenull_ void* VRegInternal_AllocZeroed(
    SIZE_T cb
);

/// Resizes @p p (null allocates) to @p cb bytes; null when memory ran out,
/// leaving @p p as it was.
VREG_DECL _Ret_maybenull_ void* VRegInternal_ReAlloc(
    _In_opt_ void* p,
    SIZE_T cb
);

/// Frees @p p; null is ignored.
VREG_DECL void VRegInternal_Free(
    _In_opt_ void* p
);

// ==========================================================================
// Buffers
// ==========================================================================

// Owned, growable storage: a byte buffer, a wide string and an array of
// fixed-size elements. A zero-filled instance is empty and needs no
// initialization; every call that grows answers FALSE when memory ran out
// and leaves the instance as it was.

/// A view of wide text that is not necessarily terminated.
typedef struct
{
    _Field_size_opt_(cch) const WCHAR* pch;
    SIZE_T cch;
} VRegInternalWStringView;

VREG_DECL VRegInternalWStringView VRegInternal_ViewOf(
    _In_opt_ LPCWSTR psz
);

VREG_DECL DWORD VRegInternal_MinDword(DWORD a, DWORD b);
VREG_DECL DWORD VRegInternal_MaxDword(DWORD a, DWORD b);

typedef struct
{
    SIZE_T cb;
    _Field_size_opt_(cb) BYTE* pb;
} VRegInternalBufferView;

typedef struct
{
    SIZE_T cbCapacity;
    _Field_range_(0, cbCapacity) SIZE_T cb;
    _Field_size_opt_(cbCapacity) BYTE* pb;
} VRegInternalBuffer;

VREG_DECL BOOL VRegInternal_BufferReserve(
    _Inout_ VRegInternalBuffer* pBuffer,
    SIZE_T cbCapacity
);

/// Sets the size to @p cb, growing the capacity if needed; new bytes are
/// indeterminate.
VREG_DECL BOOL VRegInternal_BufferResize(
    _Inout_ VRegInternalBuffer* pBuffer,
    SIZE_T cb
);

VREG_DECL BOOL VRegInternal_BufferAssign(
    _Inout_ VRegInternalBuffer* pBuffer,
    _In_reads_bytes_opt_(cb) const void* p,
    SIZE_T cb
);

VREG_DECL BOOL VRegInternal_BufferAppend(
    _Inout_ VRegInternalBuffer* pBuffer,
    _In_reads_bytes_(cb) const void* p,
    SIZE_T cb
);

VREG_DECL void VRegInternal_BufferFree(
    _Inout_ VRegInternalBuffer* pBuffer
);

/// A wide string, terminated whenever @ref psz is not null; empty and
/// unallocated while it is.
typedef struct
{
    SIZE_T cchCapacity;
    _Field_range_(0, cchCapacity) SIZE_T cch;
    _Field_size_opt_(cchCapacity) LPWSTR psz;
} VRegInternalWString;

/// The text, `L""` for an unallocated string.
VREG_DECL _Ret_notnull_ LPCWSTR VRegInternal_WStringText(
    _In_ const VRegInternalWString* pString
);

VREG_DECL VRegInternalWStringView VRegInternal_WStringViewOf(
    _In_ const VRegInternalWString* pString
);

/// Room for @p cch characters and the terminator.
VREG_DECL BOOL VRegInternal_WStringReserve(
    _Inout_ VRegInternalWString* pString,
    SIZE_T cch
);

/// Sets the length to @p cch, growing if needed, and terminates; new
/// characters are indeterminate.
VREG_DECL BOOL VRegInternal_WStringResize(
    _Inout_ VRegInternalWString* pString,
    SIZE_T cch
);

VREG_DECL BOOL VRegInternal_WStringAssign(
    _Inout_ VRegInternalWString* pString,
    _In_reads_opt_(cch) LPCWCH pch,
    SIZE_T cch
);

VREG_DECL BOOL VRegInternal_WStringAppend(
    _Inout_ VRegInternalWString* pString,
    _In_reads_(cch) LPCWCH pch,
    SIZE_T cch
);

VREG_DECL BOOL VRegInternal_WStringAppendChar(
    _Inout_ VRegInternalWString* pString,
    wchar_t ch
);

/// Shortens the string to @p cch characters, which is at most its length.
VREG_DECL void VRegInternal_WStringTruncate(
    _Inout_ VRegInternalWString* pString,
    SIZE_T cch
);

VREG_DECL void VRegInternal_WStringFree(
    _Inout_ VRegInternalWString* pString
);

/// An array of @ref count elements of one size, given to every call.
typedef struct
{
    void* p;
    SIZE_T count;
    SIZE_T capacity;
} VRegInternalArray;

/// The element at @p index of an array of @p T, as an lvalue.
#define VREG_INTERNAL_AT(pArray, T, index) (((T*)(pArray)->p)[index])

VREG_DECL BOOL VRegInternal_ArrayReserve(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    SIZE_T capacity
);

/// Inserts a copy of @p pElement before @p index, which is at most the count.
VREG_DECL BOOL VRegInternal_ArrayInsert(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    SIZE_T index,
    _In_reads_bytes_(cbElement) const void* pElement
);

VREG_DECL BOOL VRegInternal_ArrayAppend(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* pElement
);

/// Removes the element at @p index, moving the later ones down.
VREG_DECL void VRegInternal_ArrayRemoveAt(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    SIZE_T index
);

VREG_DECL void VRegInternal_ArrayFree(
    _Inout_ VRegInternalArray* pArray
);

/// Appends a copy of @p text to @p pArray, an array of @ref VRegInternalWString.
VREG_DECL BOOL VRegInternal_WStringArrayAppend(
    _Inout_ VRegInternalArray* pArray,
    VRegInternalWStringView text
);

/// Adds a copy of @p text to @p pArray, an array of @ref VRegInternalWString sorted
/// by @ref VRegInternal_CompareNames, unless an equal name is there already.
VREG_DECL BOOL VRegInternal_WStringArrayInsertSorted(
    _Inout_ VRegInternalArray* pArray,
    VRegInternalWStringView text
);

/// Whether the sorted @p pArray holds a name equal to @p text by
/// @ref VRegInternal_CompareNames.
VREG_DECL BOOL VRegInternal_WStringArrayContainsSorted(
    _In_ const VRegInternalArray* pArray,
    VRegInternalWStringView text
);

/// Frees the strings of @p pArray, an array of @ref VRegInternalWString, and the
/// array itself.
VREG_DECL void VRegInternal_WStringArrayFree(
    _Inout_ VRegInternalArray* pArray
);

// ==========================================================================
// ANSI conversion
// ==========================================================================

/// Widens @p cch bytes through @ref CP_ACP into @p pText, without a
/// terminator beyond the one the string type keeps. Answers
/// @ref ERROR_OUTOFMEMORY when memory ran out or the text is longer than the
/// conversion can take.
VREG_DECL LSTATUS VRegInternal_WideFromAnsi(
    _In_reads_(cch) LPCCH pch,
    SIZE_T cch,
    _Inout_ VRegInternalWString* pText
);

/// Narrows @p cch UTF-16 units through @ref CP_ACP into @p pText, a buffer of
/// bytes without a terminator; what the code page cannot represent becomes
/// its default character. Answers @ref ERROR_OUTOFMEMORY as
/// @ref VRegInternal_WideFromAnsi does.
VREG_DECL LSTATUS VRegInternal_AnsiFromWide(
    _In_reads_(cch) LPCWCH pch,
    SIZE_T cch,
    _Inout_ VRegInternalBuffer* pText
);

/// A string argument that may be null: the wide copy of an ANSI argument.
typedef struct
{
    BOOL isNull;
    VRegInternalWString text;
} VRegInternalWideArg;

/// Fills @p pArg from @p psz; a zero-filled @p pArg stands for a null one.
VREG_DECL LSTATUS VRegInternal_WideArgInit(
    _In_opt_ LPCSTR psz,
    _Out_ VRegInternalWideArg* pArg
);

/// The wide text, null when the argument was null.
VREG_DECL _Ret_maybenull_ LPCWSTR VRegInternal_WideArgText(
    _In_ const VRegInternalWideArg* pArg
);

/// Like @ref VRegInternal_WideArgText, as a buffer the callee may write to.
VREG_DECL _Ret_maybenull_ LPWSTR VRegInternal_WideArgMutable(
    _In_ VRegInternalWideArg* pArg
);

VREG_DECL void VRegInternal_WideArgFree(
    _Inout_ VRegInternalWideArg* pArg
);

// ==========================================================================
// Hive nodes
// ==========================================================================

/// A key in a hive. Opaque outside the hive that owns it; never freed before
/// the hive, so a pointer to one stays valid for the hive's life.
typedef struct VRegInternalNode VRegInternalNode;

typedef struct
{
    VRegInternalWString name;
    DWORD dwType;
    VRegInternalBuffer data;

    /// Hides the lower value of this name; only in a hive with a lower
    /// registry.
    BOOL isTombstone;
} VRegInternalValue;

/// A pending change notification: fires once, then is gone.
typedef struct
{
    /// Our own duplicate of the caller's event; closed when the watch fires or
    /// the hive goes.
    HANDLE hEvent;

    BOOL isSubtree;
    DWORD dwFilter;
} VRegInternalChangeWatch;

/// The flags below @ref isDeleted describe a node's relation to the lower
/// registry and stay clear in a hive without one.
struct VRegInternalNode
{
    VRegHive* pHive;
    VRegInternalNode* pParent;
    HKEY hKey;
    VRegInternalWString name;
    VRegInternalWString className;
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

    /// What the lower's @ref VRegHiveVtbl::OpenKey kept for the key.
    void* pToken;

    /// `VRegInternalNode*`, sorted by @ref VRegInternal_CompareNames, the order the native
    /// registry enumerates subkeys in.
    VRegInternalArray children;

    /// @ref VRegInternalValue, in creation order, as the native registry enumerates
    /// values.
    VRegInternalArray values;

    /// @ref VRegInternalChangeWatch, guarded by the hive's write lock.
    VRegInternalArray watches;
};

/// Ordinal comparison of uppercased names, the registry's own collation:
/// negative, zero or positive.
VREG_DECL int VRegInternal_CompareNames(
    VRegInternalWStringView left,
    VRegInternalWStringView right
);

// ==========================================================================
// Export tables
// ==========================================================================

/// The entry point @p hModule exports as @p pszName, read from its export
/// table; null when @p hModule is null, the name is not exported, or the
/// export forwards to another module.
VREG_DECL _Ret_maybenull_ void* VRegInternal_FindExport(
    _In_opt_ HMODULE hModule,
    _In_ LPCSTR pszName
);

/// A registry function, from `KernelBase.dll` or else `advapi32.dll`, which
/// alone exports the pre-`Ex` forms; null when neither exports it. Never
/// loads a module.
VREG_DECL _Ret_maybenull_ void* VRegInternal_FindRegistryExport(
    _In_ LPCSTR pszName
);

// ==========================================================================
// Owned reads
// ==========================================================================

// Reads over a @ref VReg that size and fill owned storage
// themselves, retrying with a larger buffer while the value grows between
// the sizing call and the read. The storage stays the caller's to free.

// Names and classes are read into a temporary buffer of 256 characters, which
// holds the longest key name, and grow by doubling on ERROR_MORE_DATA, up to
// the limits below. Avoids an additional sizing call for the common case.
#define VREG_INTERNAL_INITIAL_NAME_CHARS ((DWORD)256)
#define VREG_INTERNAL_MAX_CLASS_CHARS ((DWORD)32768)
#define VREG_INTERNAL_MAX_VALUE_NAME_CHARS ((DWORD)16384)

/// The size to retry with: the one the call asked for, or more than before if
/// it asked for no more than it was given.
VREG_DECL DWORD VRegInternal_Grown(DWORD cbOld, DWORD cbRequired);

VREG_DECL LSTATUS VRegInternal_QueryValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Out_opt_ LPDWORD lpType,
    _Inout_ VRegInternalBuffer* pData
);

VREG_DECL LSTATUS VRegInternal_GetValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValue,
    _In_ DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
    _Inout_ VRegInternalBuffer* pData
);

VREG_DECL LSTATUS VRegInternal_EnumKeyOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Inout_ VRegInternalWString* pName
);

/// With a null @p pData only the name and type are read.
VREG_DECL LSTATUS VRegInternal_EnumValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Inout_ VRegInternalWString* pName,
    _Out_opt_ LPDWORD lpType,
    _Inout_opt_ VRegInternalBuffer* pData
);

/// @ref VReg_QueryInfoKeyW with the class read into @p pClass. When the read
/// fails and its last call reported a class length other than the buffer's,
/// that length is stored at @p lpcchClassReported.
VREG_DECL LSTATUS VRegInternal_QueryInfoKeyOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _Inout_ VRegInternalWString* pClass,
    _Inout_opt_ LPDWORD lpcchClassReported,
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
#define VREG_INTERNAL_RRF_WOW6464_SUBKEY ((DWORD)0x00010000)
#define VREG_INTERNAL_RRF_WOW6432_SUBKEY ((DWORD)0x00020000)

/// Both view flags: what selects a view, and together an invalid request.
#define VREG_INTERNAL_RRF_WOW_SUBKEYS (VREG_INTERNAL_RRF_WOW6464_SUBKEY | VREG_INTERNAL_RRF_WOW6432_SUBKEY)

#ifdef __cplusplus
#ifdef RRF_SUBKEY_WOW6464KEY
static_assert(VREG_INTERNAL_RRF_WOW6464_SUBKEY == RRF_SUBKEY_WOW6464KEY);
#endif
#ifdef RRF_SUBKEY_WOW6432KEY
static_assert(VREG_INTERNAL_RRF_WOW6432_SUBKEY == RRF_SUBKEY_WOW6432KEY);
#endif
#endif

// ==========================================================================
// Value types
// ==========================================================================

/// Whether data of @p dwType is UTF-16 text: @ref REG_SZ, @ref REG_EXPAND_SZ
/// or @ref REG_MULTI_SZ.
VREG_DECL BOOL VRegInternal_IsStringType(DWORD dwType);

// ==========================================================================
// The handle space
// ==========================================================================

/// The highest virtual handle value. Real key handles are kernel handle values
/// (multiples of 4, at most tagged in bit 1 by advapi32), and predefined roots
/// lie above this (sign-extended on x64), so an odd value at or below it is
/// never a real key handle.
///
/// The range stays 31-bit on x64 too, so code that truncates an HKEY to a
/// @ref DWORD keeps a working handle.
#define VREG_INTERNAL_VIRTUAL_HKEY_BASE ((UINT_PTR)0x7FFFFFFF)

/// Whether @p hKey is a predefined root, @ref HKEY_CLASSES_ROOT through
/// @ref HKEY_CURRENT_USER_LOCAL_SETTINGS: `winreg.h` builds them from a
/// negative @ref LONG, sign-extended on x64, where neither a kernel handle nor
/// a virtual one ever lies.
VREG_DECL BOOL VRegInternal_IsPredefinedHKey(
    _In_opt_ HKEY hKey
);

/// One more than the highest index @ref VRegInternal_VirtualHKeyFromIndex accepts.
#define VREG_INTERNAL_VIRTUAL_HKEY_COUNT ((VREG_INTERNAL_VIRTUAL_HKEY_BASE >> 1) + 1)

#define VREG_INTERNAL_HANDLE_BLOCK_SIZE ((UINT_PTR)4096)
#define VREG_INTERNAL_HANDLE_BLOCK_COUNT (VREG_INTERNAL_VIRTUAL_HKEY_COUNT / VREG_INTERNAL_HANDLE_BLOCK_SIZE)
#define VREG_INTERNAL_HANDLE_DIRECTORY_SIZE ((UINT_PTR)512)
#define VREG_INTERNAL_HANDLE_DIRECTORY_COUNT (VREG_INTERNAL_HANDLE_BLOCK_COUNT / VREG_INTERNAL_HANDLE_DIRECTORY_SIZE)

/// @ref VRegInternalHandleSpaceShared::magic once a copy has claimed the section:
/// "VRg1".
#define VREG_INTERNAL_HANDLE_SPACE_MAGIC 0x56526731L

/// The handle section's contents. Separately built copies of VReg read this
/// layout, so it never changes; a different layout needs a different magic
/// value and section name.
typedef struct
{
    /// @ref VREG_INTERNAL_HANDLE_SPACE_MAGIC once a copy has claimed the section; zero
    /// before.
    LONG magic;
    LONG nextBlock;
} VRegInternalHandleSpaceShared;

typedef struct VRegInternalHandleDirectory VRegInternalHandleDirectory;

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
 *  Initialized by @ref VRegInternal_HandleSpaceInit; the members are private to
 *  the implementation's "The handle space" section.
 *
 * @note The section is mapped once and never unmapped or closed, so an
 *  instance must outlive every handle it issued: in practice, it is never
 *  destroyed (@ref VRegInternal_ProcessHandleSpace).
 */
typedef struct
{
    VRegInternalHandleSpaceShared* pShared;

    SRWLOCK lock;
    UINT_PTR nextIndex;
    UINT_PTR endIndex;

    // Two levels of block directories, filled in as blocks are reserved and
    // never freed: a lookup is two acquire loads and an index.
    VRegInternalHandleDirectory* volatile directories[VREG_INTERNAL_HANDLE_DIRECTORY_COUNT];
} VRegInternalHandleSpace;

/// Opens or creates the section @p pszName. A failure leaves the instance
/// unusable: @ref VRegInternal_HandleSpaceAllocate then answers null.
VREG_DECL void VRegInternal_HandleSpaceInit(
    _Out_ VRegInternalHandleSpace* pSpace,
    _In_ LPCWSTR pszName
);

/// This copy's instance over this process's section, created on first use
/// and never destroyed.
VREG_DECL _Ret_notnull_ VRegInternalHandleSpace* VRegInternal_ProcessHandleSpace(void);

VREG_DECL BOOL VRegInternal_HandleSpaceIsUsable(
    _In_ const VRegInternalHandleSpace* pSpace
);

/// A fresh handle owned by @p pSpace and bound to @p pTarget, or null when
/// the section is unusable, the namespace is exhausted or memory ran out.
VREG_DECL _Ret_maybenull_ HKEY VRegInternal_HandleSpaceAllocate(
    _Inout_ VRegInternalHandleSpace* pSpace,
    _In_opt_ void* pTarget
);

/// Whether @p hKey is a virtual handle from one of @p pSpace's blocks.
/// Lock-free.
VREG_DECL BOOL VRegInternal_HandleSpaceOwns(
    _In_ const VRegInternalHandleSpace* pSpace,
    _In_opt_ HKEY hKey
);

/// Unbinds @p hKey from its target. The value is never handed out again;
/// later lookups answer null.
VREG_DECL void VRegInternal_HandleSpaceRelease(
    _Inout_ VRegInternalHandleSpace* pSpace,
    _In_opt_ HKEY hKey
);

/// The target @p hKey was allocated for; null when @p pSpace does not own
/// it. Lock-free.
VREG_DECL _Ret_maybenull_ void* VRegInternal_HandleSpaceFind(
    _In_ const VRegInternalHandleSpace* pSpace,
    _In_opt_ HKEY hKey
);

/// The handle for @p index, counting down from @ref VREG_INTERNAL_VIRTUAL_HKEY_BASE in
/// odd steps; null when @p index is out of range.
VREG_DECL _Ret_maybenull_ HKEY VRegInternal_VirtualHKeyFromIndex(
    UINT_PTR index
);

/// The inverse of @ref VRegInternal_VirtualHKeyFromIndex; only meaningful when
/// @ref VReg_IsVirtualHKey holds.
VREG_DECL UINT_PTR VRegInternal_IndexFromVirtualHKey(
    _In_opt_ HKEY hKey
);

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
// A string or buffer a function fills is the caller's to free.

/// Whether @ref VRegHive_Freeze made the hive immutable.
VREG_DECL BOOL VRegInternal_HiveIsFrozen(
    _In_ VRegHive* pHive
);

/// A key another registry answers. The caller owns @ref hKey and closes it
/// through @ref pRegistry.
typedef struct
{
    VReg* pRegistry;
    HKEY hKey;
} VRegInternalRedirect;

/// Where an open landed: a node of the hive, or (for a key a frozen layered
/// hive leaves untouched) a redirect to the key its lower registry opened.
typedef struct
{
    VRegInternalNode* pNode;
    VRegInternalRedirect redirect;
} VRegInternalOpenResult;

/// Closes the key of @p pRedirect through its registry.
VREG_DECL void VRegInternal_CloseRedirect(
    _In_ const VRegInternalRedirect* pRedirect
);

/// What `RegQueryInfoKeyW` reports, as owned values. Lengths are in
/// characters without terminators, except @ref cbMaxValue.
typedef struct
{
    VRegInternalWString className;
    DWORD cSubKeys;
    DWORD cchMaxSubKey;
    DWORD cchMaxClass;
    DWORD cValues;
    DWORD cchMaxValueName;
    DWORD cbMaxValue;
    FILETIME lastWrite;
} VRegInternalKeyInfo;

/// What a slot answers, into storage the hive points it at; a pointer the
/// hive leaves null asks for nothing there.
struct VRegHiveResult
{
    /// A value's type, and its data or, without @ref pData, its size.
    LPDWORD pdwType;
    LPDWORD pcbData;
    VRegInternalBuffer* pData;

    /// An enumerated entry's name; for a key also its class and last write.
    VRegInternalWString* pName;
    VRegInternalWString* pClassName;
    FILETIME* pLastWrite;

    /// @ref VRegHiveVtbl::QueryInfo.
    VRegInternalKeyInfo* pInfo;
};

/// The open of a hive's lower over a registry, which the public slot leaves
/// out: it opens with @p samDesired, and with @p isRedirect hands the key out
/// in @p pRedirect instead of only answering whether it exists.
typedef LSTATUS VRegInternal_HiveOpenLowerFunction(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pParent,
    _In_ LPCWSTR pszSubKey,
    REGSAM samDesired,
    BOOL isRedirect,
    _Out_ VRegInternalRedirect* pRedirect
);

typedef enum
{
    VREG_INTERNAL_UPPER_KEY,
    VREG_INTERNAL_UPPER_VALUE,
} VRegInternalUpperKind;

/// One entry of a hive's own layer, as @ref VRegInternal_HiveSnapshotUpper lists
/// it; the views point into the hive and are valid during the visit only.
typedef struct
{
    VRegInternalUpperKind kind;

    /// The key, relative to the hive's root and separated by backslashes; empty
    /// for the root itself.
    VRegInternalWStringView path;

    /// Values only; empty for the default value.
    VRegInternalWStringView name;

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
} VRegInternalUpperRecord;

/// Called once per record under the hive's read lock, so it MUST NOT call
/// back into the hive; a status other than @ref ERROR_SUCCESS stops the
/// snapshot and is answered.
typedef LSTATUS (*VRegInternalUpperVisitor)(
    _In_opt_ void* pContext,
    _In_ const VRegInternalUpperRecord* pRecord
);

VREG_DECL _Ret_notnull_ VRegInternalNode* VRegInternal_HiveRoot(
    _In_ VRegHive* pHive
);

/// The virtual handle of @p pNode.
VREG_DECL HKEY VRegInternal_NodeHandle(
    _In_ const VRegInternalNode* pNode
);

/// The node a virtual handle of this copy of VReg belongs to; null for any
/// other handle.
VREG_DECL _Ret_maybenull_ VRegInternalNode* VRegInternal_NodeOf(
    _In_opt_ HKEY hKey
);

VREG_DECL _Ret_notnull_ VRegHive* VRegInternal_NodeHive(
    _In_ const VRegInternalNode* pNode
);

/// Opens the key @p cPath components below @p pParent. On a writable
/// layered hive the nodes that stand for lower keys are added on the way,
/// which changes nothing the hive answers.
VREG_DECL LSTATUS VRegInternal_HiveOpenKey(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pParent,
    _In_reads_(cPath) const VRegInternalWStringView* pPath,
    SIZE_T cPath,
    REGSAM samDesired,
    _Out_ VRegInternalOpenResult* pResult
);

/// On a frozen hive an existing key opens as with @ref VRegInternal_HiveOpenKey, and
/// a missing one answers @ref ERROR_ACCESS_DENIED.
VREG_DECL LSTATUS VRegInternal_HiveCreateKey(
    _In_ VRegHive* pHive,
    _Inout_ VRegInternalNode* pParent,
    _In_reads_(cPath) const VRegInternalWStringView* pPath,
    SIZE_T cPath,
    _In_ LPCWSTR pszClassName,
    DWORD dwOptions,
    REGSAM samDesired,
    _Out_ VRegInternalOpenResult* pResult,
    _Out_ LPDWORD pdwDisposition
);

/// Deletes the last component of the path; a key with subkeys answers
/// @ref ERROR_ACCESS_DENIED, as native does.
VREG_DECL LSTATUS VRegInternal_HiveDeleteKey(
    _In_ VRegHive* pHive,
    _Inout_ VRegInternalNode* pParent,
    _In_reads_(cPath) const VRegInternalWStringView* pPath,
    SIZE_T cPath
);

/// Deletes every subkey and value of @p pNode, leaving the node itself.
VREG_DECL LSTATUS VRegInternal_HiveDeleteTree(
    _In_ VRegHive* pHive,
    _Inout_ VRegInternalNode* pNode
);

/// Reads value @p pszName of @p pNode. @p pcbData receives the size of the
/// data in bytes; @p pData receives the data itself only when it is not
/// null, so a null @p pData copies nothing.
VREG_DECL LSTATUS VRegInternal_HiveQueryValue(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pNode,
    _In_ LPCWSTR pszName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VRegInternalBuffer* pData
);

VREG_DECL LSTATUS VRegInternal_HiveSetValue(
    _In_ VRegHive* pHive,
    _Inout_ VRegInternalNode* pNode,
    _In_ LPCWSTR pszName,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData
);

VREG_DECL LSTATUS VRegInternal_HiveDeleteValue(
    _In_ VRegHive *pHive,
    _Inout_ VRegInternalNode* pNode,
    _In_ LPCWSTR pszName
);

VREG_DECL LSTATUS VRegInternal_HiveEnumKey(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pNode,
    DWORD dwIndex,
    _Inout_ VRegInternalWString* pName,
    _Inout_ VRegInternalWString* pClassName,
    _Out_ FILETIME* pLastWrite
);

/// Reads the value of @p pNode at @p dwIndex. @p pcbData receives the size
/// of the data in bytes; @p pData receives the data itself only when it
/// is not null, so a null @p pData copies nothing.
VREG_DECL LSTATUS VRegInternal_HiveEnumValue(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pNode,
    DWORD dwIndex,
    _Inout_ VRegInternalWString* pName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VRegInternalBuffer* pData
);

/// Fills @p pInfo, whose @ref VRegInternalKeyInfo::className is the caller's
/// string.
VREG_DECL LSTATUS VRegInternal_HiveQueryInfo(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pNode,
    _Inout_ VRegInternalKeyInfo* pInfo
);

/// What `RegFlushKey` on @p pNode needs from the hive: success, or
/// @ref ERROR_KEY_DELETED.
VREG_DECL LSTATUS VRegInternal_HiveCheckKey(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pNode
);

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
VREG_DECL LSTATUS VRegInternal_HiveWatch(
    _In_ VRegHive* pHive,
    _In_ VRegInternalNode* pNode,
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
VREG_DECL LSTATUS VRegInternal_HiveSnapshotUpper(
    _In_ VRegHive* pHive,
    _In_ VRegInternalUpperVisitor pfnVisit,
    _In_opt_ void* pContext
);

// ==========================================================================
// Registries
// ==========================================================================

/// Applies `X` to the name of every wide `Ex` form and every function with
/// one form only: the wide surface of @ref VRegVtbl, and the fields
/// of @ref VRegNativeFunctions.
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

/// Applies `X` to the name of every other form in @ref VRegVtbl: the
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

/// The kinds of @ref VReg: one @ref VReg_CreateCustom made over a table, and
/// the two VReg implements itself.
#define VREG_INTERNAL_KIND_CUSTOM 0
#define VREG_INTERNAL_KIND_NATIVE 1
#define VREG_INTERNAL_KIND_VIRTUAL 2

/// @ref NtQueryKey, the one `ntdll` function a virtual registry uses to find
/// the native path of a real key.
typedef LONG (NTAPI* VRegInternal_NtQueryKeyFunction)(
    HANDLE hKey,
    ULONG keyInformationClass,
    PVOID pKeyInformation,
    ULONG cbKeyInformation,
    PULONG pcbResult
);

/// A registry: its kind and what the kind needs.
struct VReg
{
    /// @ref VREG_INTERNAL_KIND_CUSTOM, @ref VREG_INTERNAL_KIND_NATIVE or
    /// @ref VREG_INTERNAL_KIND_VIRTUAL.
    DWORD dwKind;

    /// The member of @ref dwKind.
    union
    {
        /// @ref VREG_INTERNAL_KIND_CUSTOM: the table it was created with and
        /// the object its slots reach through @ref VReg_GetContext.
        struct
        {
            const VRegVtbl* lpVtbl;
            void* pvContext;
        } custom;

        /// @ref VREG_INTERNAL_KIND_NATIVE: the live table, see
        /// @ref VReg_GetNativeFunctions.
        VRegNativeFunctions native;

        /// @ref VREG_INTERNAL_KIND_VIRTUAL.
        struct
        {
            /// Null for a registry held entirely in memory.
            VReg* pBase;

            VRegInternal_NtQueryKeyFunction pfnNtQueryKey;
            struct VRegInternalMountTable* volatile pMounts;
            SRWLOCK mountLock;
        } virtualRegistry;
    } implementation;
};

// ==========================================================================
// The virtual registry
// ==========================================================================

// What the flat functions call for a registry of VREG_INTERNAL_KIND_VIRTUAL.
VREG_DECL VRegOpenKeyExWFunction VRegInternal_VirtualRegOpenKeyExW;
VREG_DECL VRegCreateKeyExWFunction VRegInternal_VirtualRegCreateKeyExW;
VREG_DECL VRegCloseKeyFunction VRegInternal_VirtualRegCloseKey;
VREG_DECL VRegQueryValueExWFunction VRegInternal_VirtualRegQueryValueExW;
VREG_DECL VRegSetValueExWFunction VRegInternal_VirtualRegSetValueExW;
VREG_DECL VRegDeleteValueWFunction VRegInternal_VirtualRegDeleteValueW;
VREG_DECL VRegDeleteKeyExWFunction VRegInternal_VirtualRegDeleteKeyExW;
VREG_DECL VRegDeleteTreeWFunction VRegInternal_VirtualRegDeleteTreeW;
VREG_DECL VRegEnumKeyExWFunction VRegInternal_VirtualRegEnumKeyExW;
VREG_DECL VRegEnumValueWFunction VRegInternal_VirtualRegEnumValueW;
VREG_DECL VRegQueryInfoKeyWFunction VRegInternal_VirtualRegQueryInfoKeyW;
VREG_DECL VRegLoadMUIStringWFunction VRegInternal_VirtualRegLoadMUIStringW;
VREG_DECL VRegGetValueWFunction VRegInternal_VirtualRegGetValueW;
VREG_DECL VRegNotifyChangeKeyValueFunction VRegInternal_VirtualRegNotifyChangeKeyValue;
VREG_DECL VRegFlushKeyFunction VRegInternal_VirtualRegFlushKey;

VREG_DECL void WINAPI VRegInternal_VirtualDestroy(
    _In_ VReg* pRegistry
);

// ==========================================================================
// Paths
// ==========================================================================

/// The longest key name component the registry accepts, in characters.
#define VREG_INTERNAL_MAX_KEY_NAME_CHARS ((SIZE_T)255)

/// Splits @p path at backslashes as the native registry does, appending the
/// components (views into @p path) to @p pComponents, an array of
/// @ref VRegInternalWStringView: empty components are skipped, a trailing backslash
/// is ignored, a leading one answers @ref ERROR_BAD_PATHNAME and a component
/// over @ref VREG_INTERNAL_MAX_KEY_NAME_CHARS @ref ERROR_INVALID_PARAMETER.
VREG_DECL LSTATUS VRegInternal_SplitPath(
    VRegInternalWStringView path,
    _Inout_ VRegInternalArray* pComponents
);

// ==========================================================================
// Status
// ==========================================================================

/// A status a failing path answers, which the analyzer cannot prove nonzero
/// when it was stored by another function.
_Post_satisfies_(return != ERROR_SUCCESS)
VREG_DECL LSTATUS VRegInternal_FailureStatus(LSTATUS status);

// ==========================================================================
// Scripts
// ==========================================================================

/// The storage behind one @ref VRegOperation of a script.
typedef struct
{
    VRegInternalWString path;
    VRegInternalWString name;
    VRegInternalBuffer data;
} VRegInternalScriptEntry;

struct VRegScript
{
    /// `VRegOperation`, pointing into @ref entries.
    VRegInternalArray operations;

    /// `VRegInternalScriptEntry`, one per operation.
    VRegInternalArray entries;
};

/// A predefined root a `.reg` file can name: exports write its long name,
/// imports accept either.
typedef struct
{
    LPCWSTR pszLong;
    LPCWSTR pszShort;
    HKEY hKey;
} VRegInternalRootKey;

/// The long name of a root a `.reg` file can name; null for any other handle.
VREG_DECL _Ret_maybenull_ LPCWSTR VRegInternal_RootName(
    _In_opt_ HKEY hRoot
);

/// The root named @p name, long or short form, ignoring case.
VREG_DECL BOOL VRegInternal_FindRoot(
    VRegInternalWStringView name,
    _Out_ PHKEY phRoot
);

VREG_DECL LSTATUS VRegInternal_ScriptCreate(
    _Outptr_ VRegScript** ppScript
);

/// Appends an operation, copying its path, name and data into the script.
VREG_DECL LSTATUS VRegInternal_ScriptAppend(
    _Inout_ VRegScript* pScript,
    VRegOperationKind kind,
    _In_ HKEY hRoot,
    VRegInternalWStringView path,
    VRegInternalWStringView name,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData,
    SIZE_T line
);

/// Decodes a whole file to text without BOM. Detects the encoding as
/// @ref VRegScript_Parse documents. Answers @ref ERROR_INVALID_DATA with
/// @p pError (zeroed by the caller) set on bytes the encoding cannot hold.
VREG_DECL LSTATUS VRegInternal_DecodeFile(
    _In_reads_bytes_(cbFile) const void* pFile,
    SIZE_T cbFile,
    _Inout_ VRegInternalWString* pText,
    _Inout_opt_ VRegParseError* pError
);

/// The bytes of @p pText in @p format, with a BOM for Unicode. Answers
/// @ref ERROR_NO_UNICODE_TRANSLATION when the format's code page cannot hold
/// it.
VREG_DECL LSTATUS VRegInternal_EncodeFile(
    _In_ const VRegInternalWString* pText,
    VRegFormat format,
    _Inout_ VRegInternalBuffer* pFile
);

/// Reads and writes whole files; Win32 errors answer as is.
VREG_DECL LSTATUS VRegInternal_ReadWholeFile(
    _In_ LPCWSTR pszPath,
    _Inout_ VRegInternalBuffer* pFile
);

VREG_DECL LSTATUS VRegInternal_WriteWholeFile(
    _In_ LPCWSTR pszPath,
    _In_reads_bytes_(cbFile) const void* pFile,
    SIZE_T cbFile
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VREG_INTERNAL_INCLUDED
#endif // VREG_INTERNAL

#ifdef VREG_IMPLEMENTATION
#ifndef VREG_IMPLEMENTATION_INCLUDED
#define VREG_IMPLEMENTATION_INCLUDED

#pragma comment(lib, "advapi32")

// ==========================================================================
// Allocation
// ==========================================================================

VREG_DECL void VRegInternal_AssertFailed(void)
{
    if (IsDebuggerPresent())
        __debugbreak();
}

VREG_DECL _Ret_maybenull_ void* VRegInternal_Alloc(
    const SIZE_T cb
)
{
    if (cb == 0)
        return NULL;

    return VREG_MALLOC(cb);
}

VREG_DECL _Ret_maybenull_ void* VRegInternal_AllocZeroed(
    const SIZE_T cb
)
{
    void* const p = VRegInternal_Alloc(cb);
    if (p)
        ZeroMemory(p, cb);

    return p;
}

VREG_DECL _Ret_maybenull_ void* VRegInternal_ReAlloc(
    _In_opt_ void* const p,
    const SIZE_T cb
)
{
    if (!p)
        return VRegInternal_Alloc(cb);
    if (cb == 0)
    {
        VREG_FREE(p);
        return NULL;
    }

    return VREG_REALLOC(p, cb);
}

VREG_DECL void VRegInternal_Free(
    _In_opt_ void* const p
)
{
    if (p)
        VREG_FREE(p);
}

// ==========================================================================
// ANSI conversion
// ==========================================================================

static SIZE_T VRegInternal_AnsiLength(_In_ const LPCSTR psz)
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
static LSTATUS VRegInternal_WideFromMultiByte(
    const UINT codePage,
    const DWORD dwFlags,
    _In_reads_(cch) const LPCCH pch,
    const SIZE_T cch,
    _Inout_ VRegInternalWString* const pText
)
{
    int cchWide;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (!VRegInternal_WStringResize(pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cchWide = MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, NULL, 0);
    if (cchWide <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VRegInternal_WStringResize(pText, (SIZE_T)cchWide))
        return ERROR_OUTOFMEMORY;

    MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, pText->psz, cchWide);
    return ERROR_SUCCESS;
}

/// Narrows @p cch UTF-16 units into @p pText in @p codePage with the
/// @ref WideCharToMultiByte flags @p dwFlags, reporting a default character
/// through @p pfUsedDefault. Fails as @ref VRegInternal_WideFromMultiByte does.
static LSTATUS VRegInternal_MultiByteFromWide(
    const UINT codePage,
    const DWORD dwFlags,
    _Out_opt_ BOOL* const pfUsedDefault,
    _In_reads_(cch) const LPCWCH pch,
    const SIZE_T cch,
    _Inout_ VRegInternalBuffer* const pText
)
{
    int cbText;
    if (pfUsedDefault)
        *pfUsedDefault = FALSE;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (!VRegInternal_BufferResize(pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cbText = WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, NULL, 0, NULL, pfUsedDefault);
    if (cbText <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VRegInternal_BufferResize(pText, (SIZE_T)cbText))
        return ERROR_OUTOFMEMORY;

    WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, (char*)pText->pb, cbText, NULL, pfUsedDefault);
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegInternal_WideFromAnsi(
    _In_reads_(cch) const LPCCH pch,
    const SIZE_T cch,
    _Inout_ VRegInternalWString* const pText
)
{
    return VRegInternal_WideFromMultiByte(CP_ACP, 0, pch, cch, pText);
}

VREG_DECL LSTATUS VRegInternal_AnsiFromWide(
    _In_reads_(cch) const LPCWCH pch,
    const SIZE_T cch,
    _Inout_ VRegInternalBuffer* const pText
)
{
    return VRegInternal_MultiByteFromWide(CP_ACP, 0, NULL, pch, cch, pText);
}

VREG_DECL LSTATUS VRegInternal_WideArgInit(
    _In_opt_ const LPCSTR psz,
    _Out_ VRegInternalWideArg* const pArg
)
{
    ZeroMemory(pArg, sizeof(*pArg));
    pArg->isNull = psz == NULL;
    if (!psz)
        return ERROR_SUCCESS;

    return VRegInternal_WideFromAnsi(psz, VRegInternal_AnsiLength(psz), &pArg->text);
}

VREG_DECL _Ret_maybenull_ LPCWSTR VRegInternal_WideArgText(
    _In_ const VRegInternalWideArg* const pArg
)
{
    return pArg->isNull ? NULL : VRegInternal_WStringText(&pArg->text);
}

VREG_DECL _Ret_maybenull_ LPWSTR VRegInternal_WideArgMutable(
    _In_ VRegInternalWideArg* const pArg
)
{
    if (pArg->isNull)
        return NULL;

    // An unallocated empty string has no storage to hand out.
    if (!pArg->text.psz && !VRegInternal_WStringReserve(&pArg->text, 0))
        return NULL;

    return pArg->text.psz;
}

VREG_DECL void VRegInternal_WideArgFree(_Inout_ VRegInternalWideArg* const pArg)
{
    VRegInternal_WStringFree(&pArg->text);
}

// ==========================================================================
// Buffers
// ==========================================================================

/// The capacity to grow to for @p needed elements: at least double the old
/// one, so repeated appends stay linear.
static SIZE_T VRegInternal_GrownCapacity(
    const SIZE_T capacity,
    const SIZE_T needed
)
{
    SIZE_T grown = capacity < 8 ? 8 : capacity * 2;
    return grown < needed ? needed : grown;
}

VREG_DECL DWORD VRegInternal_MinDword(const DWORD a, const DWORD b)
{
    return a < b ? a : b;
}

VREG_DECL DWORD VRegInternal_MaxDword(const DWORD a, const DWORD b)
{
    return a > b ? a : b;
}

static SIZE_T VRegInternal_MinSize(const SIZE_T a, const SIZE_T b)
{
    return a < b ? a : b;
}

VREG_DECL VRegInternalWStringView VRegInternal_ViewOf(
    _In_opt_ const LPCWSTR psz
)
{
    VRegInternalWStringView view;
    view.pch = psz;
    view.cch = 0;
    if (psz)
        while (psz[view.cch] != L'\0')
            ++view.cch;

    return view;
}

VREG_DECL BOOL VRegInternal_BufferReserve(
    _Inout_ VRegInternalBuffer* const pBuffer,
    const SIZE_T cbCapacity
)
{
    BYTE* pb;
    if (cbCapacity <= pBuffer->cbCapacity)
        return TRUE;

    pb = (BYTE*)VRegInternal_ReAlloc(pBuffer->pb, cbCapacity);
    if (!pb)
        return FALSE;

    pBuffer->pb = pb;
    pBuffer->cbCapacity = cbCapacity;
    return TRUE;
}

VREG_DECL BOOL VRegInternal_BufferResize(
    _Inout_ VRegInternalBuffer* const pBuffer,
    const SIZE_T cb
)
{
    if (cb > pBuffer->cbCapacity)
    {
        const SIZE_T cbGrown = VRegInternal_GrownCapacity(pBuffer->cbCapacity, cb);
        if (!VRegInternal_BufferReserve(pBuffer, cbGrown))
            return FALSE;
    }

    pBuffer->cb = cb;
    return TRUE;
}

VREG_DECL BOOL VRegInternal_BufferAssign(
    _Inout_ VRegInternalBuffer* const pBuffer,
    _In_reads_bytes_opt_(cb) const void* const p,
    const SIZE_T cb
)
{
    if (!VRegInternal_BufferResize(pBuffer, cb))
        return FALSE;

    if (cb != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(p != NULL);
        CopyMemory(pBuffer->pb, p, cb);
    }

    return TRUE;
}

VREG_DECL BOOL VRegInternal_BufferAppend(
    _Inout_ VRegInternalBuffer* const pBuffer,
    _In_reads_bytes_(cb) const void* const p,
    const SIZE_T cb
)
{
    const SIZE_T cbOld = pBuffer->cb;
    if (!VRegInternal_BufferResize(pBuffer, cbOld + cb))
        return FALSE;

    if (cb != 0)
        CopyMemory(pBuffer->pb + cbOld, p, cb);

    return TRUE;
}

VREG_DECL void VRegInternal_BufferFree(
    _Inout_ VRegInternalBuffer* const pBuffer
)
{
    VRegInternal_Free(pBuffer->pb);
    pBuffer->pb = NULL;
    pBuffer->cb = 0;
    pBuffer->cbCapacity = 0;
}

VREG_DECL _Ret_notnull_ LPCWSTR VRegInternal_WStringText(
    _In_ const VRegInternalWString* const pString
)
{
    return pString->psz ? pString->psz : L"";
}

VREG_DECL VRegInternalWStringView VRegInternal_WStringViewOf(
    _In_ const VRegInternalWString* const pString
)
{
    VRegInternalWStringView view;
    view.pch = VRegInternal_WStringText(pString);
    view.cch = pString->cch;
    return view;
}

VREG_DECL BOOL VRegInternal_WStringReserve(
    _Inout_ VRegInternalWString* const pString,
    const SIZE_T cch
)
{
    LPWSTR psz;
    if (cch < pString->cchCapacity)
        return TRUE;

    psz = (wchar_t*)VRegInternal_ReAlloc(pString->psz, (cch + 1) * sizeof(wchar_t));
    if (!psz)
        return FALSE;

    if (!pString->psz)
        psz[0] = L'\0';

    pString->psz = psz;
    pString->cchCapacity = cch + 1;
    return TRUE;
}

VREG_DECL BOOL VRegInternal_WStringResize(
    _Inout_ VRegInternalWString* const pString,
    const SIZE_T cch
)
{
    if (cch >= pString->cchCapacity)
    {
        const SIZE_T cchGrown = VRegInternal_GrownCapacity(pString->cchCapacity, cch + 1) - 1;
        if (!VRegInternal_WStringReserve(pString, cchGrown))
            return FALSE;
    }

    if (pString->psz)
        pString->psz[cch] = L'\0';

    pString->cch = cch;
    return TRUE;
}

VREG_DECL BOOL VRegInternal_WStringAssign(
    _Inout_ VRegInternalWString* const pString,
    _In_reads_opt_(cch) const LPCWCH pch,
    const SIZE_T cch
)
{
    if (!VRegInternal_WStringResize(pString, cch))
        return FALSE;

    if (cch != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(pch != NULL);
        CopyMemory(pString->psz, pch, cch * sizeof(wchar_t));
    }

    return TRUE;
}

VREG_DECL BOOL VRegInternal_WStringAppend(
    _Inout_ VRegInternalWString* const pString,
    _In_reads_(cch) const LPCWCH pch,
    const SIZE_T cch
)
{
    const SIZE_T cchOld = pString->cch;
    if (!VRegInternal_WStringResize(pString, cchOld + cch))
        return FALSE;

    if (cch != 0)
        CopyMemory(pString->psz + cchOld, pch, cch * sizeof(wchar_t));

    return TRUE;
}

VREG_DECL BOOL VRegInternal_WStringAppendChar(
    _Inout_ VRegInternalWString* const pString,
    const wchar_t ch
)
{
    return VRegInternal_WStringAppend(pString, &ch, 1);
}

/// Appends a key path component, after a backslash unless @p pPath is empty.
static BOOL VRegInternal_AppendPathComponent(
    _Inout_ VRegInternalWString* const pPath,
    _In_reads_(cch) const LPCWCH pch,
    const SIZE_T cch
)
{
    if (pPath->cch != 0 && !VRegInternal_WStringAppendChar(pPath, L'\\'))
        return FALSE;

    return VRegInternal_WStringAppend(pPath, pch, cch);
}

VREG_DECL void VRegInternal_WStringTruncate(
    _Inout_ VRegInternalWString* const pString,
    const SIZE_T cch
)
{
    VREG_ASSERT(cch <= pString->cch);
    pString->cch = cch;
    if (pString->psz)
        pString->psz[cch] = L'\0';
}

VREG_DECL void VRegInternal_WStringFree(
    _Inout_ VRegInternalWString* const pString
)
{
    VRegInternal_Free(pString->psz);
    pString->psz = NULL;
    pString->cch = 0;
    pString->cchCapacity = 0;
}

VREG_DECL BOOL VRegInternal_ArrayReserve(
    _Inout_ VRegInternalArray* const pArray,
    const SIZE_T cbElement,
    const SIZE_T capacity
)
{
    void* p;
    if (capacity <= pArray->capacity)
        return TRUE;

    p = VRegInternal_ReAlloc(pArray->p, capacity * cbElement);
    if (!p)
        return FALSE;

    pArray->p = p;
    pArray->capacity = capacity;
    return TRUE;
}

VREG_DECL BOOL VRegInternal_ArrayInsert(
    _Inout_ VRegInternalArray* const pArray,
    const SIZE_T cbElement,
    const SIZE_T index,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    BYTE* pbAt;
    VREG_ASSERT(index <= pArray->count);
    if (pArray->count == pArray->capacity)
    {
        const SIZE_T grown = VRegInternal_GrownCapacity(pArray->capacity, pArray->count + 1);
        if (!VRegInternal_ArrayReserve(pArray, cbElement, grown))
            return FALSE;
    }

    pbAt = (BYTE*)pArray->p + index * cbElement;
    MoveMemory(pbAt + cbElement, pbAt, (pArray->count - index) * cbElement);
    CopyMemory(pbAt, pElement, cbElement);
    ++pArray->count;
    return TRUE;
}

VREG_DECL BOOL VRegInternal_ArrayAppend(
    _Inout_ VRegInternalArray* const pArray,
    const SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    return VRegInternal_ArrayInsert(pArray, cbElement, pArray->count, pElement);
}

VREG_DECL void VRegInternal_ArrayRemoveAt(
    _Inout_ VRegInternalArray* const pArray,
    const SIZE_T cbElement,
    const SIZE_T index
)
{
    BYTE* const pbAt = (BYTE*)pArray->p + index * cbElement;
    VREG_ASSERT(index < pArray->count);
    MoveMemory(pbAt, pbAt + cbElement, (pArray->count - index - 1) * cbElement);
    --pArray->count;
}

VREG_DECL void VRegInternal_ArrayFree(
    _Inout_ VRegInternalArray* const pArray
)
{
    VRegInternal_Free(pArray->p);
    pArray->p = NULL;
    pArray->count = 0;
    pArray->capacity = 0;
}

VREG_DECL BOOL VRegInternal_WStringArrayAppend(
    _Inout_ VRegInternalArray* const pArray,
    const VRegInternalWStringView text
)
{
    VRegInternalWString copy = {0, 0, NULL};
    if (!VRegInternal_WStringAssign(&copy, text.pch, text.cch))
        return FALSE;
    if (!VRegInternal_ArrayAppend(pArray, sizeof(copy), &copy))
    {
        VRegInternal_WStringFree(&copy);
        return FALSE;
    }

    return TRUE;
}

VREG_DECL void VRegInternal_WStringArrayFree(
    _Inout_ VRegInternalArray* const pArray
)
{
    SIZE_T i;
    for (i = 0; i < pArray->count; ++i)
        VRegInternal_WStringFree(&VREG_INTERNAL_AT(pArray, VRegInternalWString, i));

    VRegInternal_ArrayFree(pArray);
}

VREG_DECL BOOL VRegInternal_WStringArrayInsertSorted(
    _Inout_ VRegInternalArray* const pList,
    const VRegInternalWStringView name
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pList->count;
    VRegInternalWString copy = {0, 0, NULL};
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const int order = VRegInternal_CompareNames(VRegInternal_WStringViewOf(&VREG_INTERNAL_AT(pList, VRegInternalWString, mid)), name);
        if (order == 0)
            return TRUE;
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (!VRegInternal_WStringAssign(&copy, name.pch, name.cch))
        return FALSE;
    if (!VRegInternal_ArrayInsert(pList, sizeof(copy), lo, &copy))
    {
        VRegInternal_WStringFree(&copy);
        return FALSE;
    }

    return TRUE;
}

VREG_DECL BOOL VRegInternal_WStringArrayContainsSorted(
    _In_ const VRegInternalArray* const pList,
    const VRegInternalWStringView name
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pList->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const int order = VRegInternal_CompareNames(VRegInternal_WStringViewOf(&VREG_INTERNAL_AT(pList, VRegInternalWString, mid)), name);
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
static int VRegInternal_CompareExportNames(
    _In_ LPCSTR pszLeft,
    _In_ LPCSTR pszRight
)
{
    while (*pszLeft != '\0' && *pszLeft == *pszRight)
    {
        ++pszLeft;
        ++pszRight;
    }

    return (int)(unsigned char)*pszLeft - (int)(unsigned char)*pszRight;
}

VREG_DECL _Ret_maybenull_ void* VRegInternal_FindExport(
    _In_opt_ const HMODULE hModule,
    _In_ const LPCSTR pszName
)
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
        const int order = VRegInternal_CompareExportNames(pszName, (LPCSTR)(pBase + pNames[middle]));
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

VREG_DECL _Ret_maybenull_ void* VRegInternal_FindRegistryExport(
    _In_ const LPCSTR pszName
)
{
    void* const pFunction = VRegInternal_FindExport(GetModuleHandleW(L"KernelBase.dll"), pszName);
    g_pfnImportAdvapi32 = (void*)&RegOpenKeyW;
    if (pFunction)
        return pFunction;

    return VRegInternal_FindExport(GetModuleHandleW(L"advapi32.dll"), pszName);
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

/// The table a native or virtual registry derives every form from.
static const VRegVtbl VRegInternal_NoSlots = {0};

/// The slots a derived form calls when they are filled: a custom registry's
/// own, and none for the kinds VReg implements.
static _Ret_notnull_ const VRegVtbl* VRegInternal_SlotsOf(
    _In_ const VReg* const pRegistry
)
{
    return pRegistry->dwKind == VREG_INTERNAL_KIND_CUSTOM ? pRegistry->implementation.custom.lpVtbl : &VRegInternal_NoSlots;
}

typedef struct
{
    DWORD cbValue;
    DWORD dwType;
    DWORD ibValue;
} VRegInternalPlacedValue;

static DWORD VRegInternal_AlignedToDword(const DWORD cb)
{
    return (cb + 3) & ~(DWORD)3;
}

/// Copies @p cb bytes; a null @p pSource is legal only for none.
static void VRegInternal_CopyBytes(
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
static BOOL VRegInternal_CopyAnsiName(
    _In_ const VRegInternalBuffer* const pText,
    _Out_writes_opt_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    if (!pszBuffer)
        return FALSE;

    if (pText->cb < cchBuffer)
    {
        VRegInternal_CopyBytes(pszBuffer, pText->pb, pText->cb);
        pszBuffer[pText->cb] = '\0';
        return TRUE;
    }

    if (pText->cb == cchBuffer && cchBuffer > 0)
    {
        VRegInternal_CopyBytes(pszBuffer, pText->pb, pText->cb - 1);
        pszBuffer[pText->cb - 1] = '\0';
    }
    return FALSE;
}

/// Like @ref VRegInternal_CopyAnsiName for a class buffer, which native fills with as
/// much of the text as fits (and a terminator) whenever it has room for one.
static BOOL VRegInternal_CopyAnsiClass(
    _In_ const VRegInternalBuffer* const pText,
    _Out_writes_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    SIZE_T cchCopied;
    if (cchBuffer == 0)
        return FALSE;

    cchCopied = VRegInternal_MinSize(pText->cb, cchBuffer - 1);
    VRegInternal_CopyBytes(pszBuffer, pText->pb, cchCopied);
    pszBuffer[cchCopied] = '\0';
    return pText->cb < cchBuffer;
}

VREG_DECL LSTATUS WINAPI VReg_OpenKeyExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegOpenKeyExA)
        return VRegInternal_SlotsOf(pRegistry)->RegOpenKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    HKEY hPrevious;
    LSTATUS status = VRegInternal_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    status = VReg_OpenKeyExW(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&subKey),
        ulOptions,
        samDesired,
        phkResult
    );
    // The native ANSI form leaves the result alone for an invalid handle; the
    // wide form nulls it.
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VRegInternal_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_CreateKeyExA(
    _In_ VReg* const pRegistry,
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
    if (VRegInternal_SlotsOf(pRegistry)->RegCreateKeyExA)
        return VRegInternal_SlotsOf(pRegistry)->RegCreateKeyExA(
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

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    VRegInternalWideArg className = {FALSE, {0, 0, NULL}};
    HKEY hPrevious;
    LSTATUS status = VRegInternal_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternal_WideArgInit(lpClass, &className);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VReg_CreateKeyExW(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&subKey),
        Reserved,
        VRegInternal_WideArgMutable(&className),
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition
    );
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VRegInternal_WideArgFree(&className);
    VRegInternal_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_QueryValueExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegQueryValueExA)
        return VRegInternal_SlotsOf(pRegistry)->RegQueryValueExA(
            pRegistry,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    VRegInternalWideArg valueName = {FALSE, {0, 0, NULL}};
    VRegInternalBuffer value = {0, 0, NULL};
    VRegInternalBuffer text = {0, 0, NULL};
    DWORD dwType;
    DWORD cbSize;
    DWORD cbText;
    LSTATUS sized;
    LSTATUS status;

    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VRegInternal_WideArgInit(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!lpData && !lpcbData)
    {
        status = VReg_QueryValueExW(
            pRegistry,
            hKey,
            VRegInternal_WideArgText(&valueName),
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
    sized = VReg_QueryValueExW(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&valueName),
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

    if (!VRegInternal_IsStringType(dwType))
    {
        status = VReg_QueryValueExW(
            pRegistry,
            hKey,
            VRegInternal_WideArgText(&valueName),
            NULL,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    status = VRegInternal_QueryValueOwned(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&valueName),
        &dwType,
        &value
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternal_AnsiFromWide((LPCWCH)value.pb, value.cb / sizeof(wchar_t), &text);
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
                VRegInternal_CopyBytes(lpData, value.pb, value.cb);
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
    VRegInternal_BufferFree(&text);
    VRegInternal_BufferFree(&value);
    VRegInternal_WideArgFree(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_SetValueExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegSetValueExA)
        return VRegInternal_SlotsOf(pRegistry)->RegSetValueExA(
            pRegistry,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    VRegInternalWideArg valueName = {FALSE, {0, 0, NULL}};
    VRegInternalWString text = {0, 0, NULL};
    LSTATUS status = VRegInternal_WideArgInit(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VRegInternal_IsStringType(dwType) || !lpData)
    {
        status = VReg_SetValueExW(
            pRegistry,
            hKey,
            VRegInternal_WideArgText(&valueName),
            Reserved,
            dwType,
            lpData,
            cbData
        );
        goto done;
    }

    status = VRegInternal_WideFromAnsi((LPCCH)lpData, cbData, &text);
    if (status != ERROR_SUCCESS)
        goto done;

    // Native reads the byte after unterminated data and keeps it as the
    // terminator when it is zero, which it nearly always is. VReg always
    // terminates, so no byte past cbData is ever read; reading it, as native
    // does, can fault at the end of a page.
    if (text.cch > 0 && text.psz[text.cch - 1] != L'\0')
    {
        if (!VRegInternal_WStringAppendChar(&text, L'\0'))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    status = VReg_SetValueExW(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&valueName),
        Reserved,
        dwType,
        (const BYTE*)VRegInternal_WStringText(&text),
        (DWORD)(text.cch * sizeof(wchar_t))
    );

done:
    VRegInternal_WStringFree(&text);
    VRegInternal_WideArgFree(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_DeleteValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegDeleteValueA)
        return VRegInternal_SlotsOf(pRegistry)->RegDeleteValueA(
            pRegistry,
            hKey,
            lpValueName
        );

    VRegInternalWideArg valueName = {FALSE, {0, 0, NULL}};
    LSTATUS status = VRegInternal_WideArgInit(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_DeleteValueW(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&valueName)
    );

done:
    VRegInternal_WideArgFree(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_DeleteKeyExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyExA)
        return VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    LSTATUS status = VRegInternal_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VReg_DeleteKeyExW(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&subKey),
        samDesired,
        Reserved
    );

done:
    VRegInternal_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_DeleteTreeA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegDeleteTreeA)
        return VRegInternal_SlotsOf(pRegistry)->RegDeleteTreeA(
            pRegistry,
            hKey,
            lpSubKey
        );

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    LSTATUS status = VRegInternal_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_DeleteTreeW(
        pRegistry,
        hKey,
        VRegInternal_WideArgText(&subKey)
    );

done:
    VRegInternal_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_EnumKeyExA(
    _In_ VReg* const pRegistry,
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
    if (VRegInternal_SlotsOf(pRegistry)->RegEnumKeyExA)
        return VRegInternal_SlotsOf(pRegistry)->RegEnumKeyExA(
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

    VRegInternalWString name = {0, 0, NULL};
    VRegInternalWString className = {0, 0, NULL};
    VRegInternalBuffer nameA = {0, 0, NULL};
    VRegInternalBuffer classA = {0, 0, NULL};
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
    cchName = VREG_INTERNAL_INITIAL_NAME_CHARS;
    cchClass = wantClass ? VREG_INTERNAL_INITIAL_NAME_CHARS : 0;
    ZeroMemory(&ftLastWrite, sizeof(ftLastWrite));
    for (;;)
    {
        if (!VRegInternal_WStringResize(&name, cchName)
            || !VRegInternal_WStringResize(&className, cchClass))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cchNameOut = cchName;
        cchClassOut = cchClass;
        status = VReg_EnumKeyExW(
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
            || (cchName >= VREG_INTERNAL_MAX_CLASS_CHARS && cchClass >= VREG_INTERNAL_MAX_CLASS_CHARS)
        )
            break;

        cchName = VRegInternal_MinDword(cchName * 2, VREG_INTERNAL_MAX_CLASS_CHARS);
        if (wantClass)
            cchClass = VRegInternal_MinDword(cchClass * 2, VREG_INTERNAL_MAX_CLASS_CHARS);
    }
    if (status != ERROR_SUCCESS)
        goto done;

    VRegInternal_WStringTruncate(&name, cchNameOut);
    VRegInternal_WStringTruncate(&className, cchClassOut);
    if (lpftLastWriteTime)
        *lpftLastWriteTime = ftLastWrite;

    status = VRegInternal_AnsiFromWide(VRegInternal_WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VRegInternal_CopyAnsiName(&nameA, lpName, *lpcchName))
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
        status = VRegInternal_AnsiFromWide(VRegInternal_WStringText(&className), className.cch, &classA);
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VRegInternal_CopyAnsiClass(&classA, lpClass, *lpcchClass))
        {
            status = ERROR_MORE_DATA;
            goto done;
        }
        *lpcchClass = (DWORD)classA.cb;
    }

    status = ERROR_SUCCESS;

done:
    VRegInternal_BufferFree(&classA);
    VRegInternal_BufferFree(&nameA);
    VRegInternal_WStringFree(&className);
    VRegInternal_WStringFree(&name);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_EnumValueA(
    _In_ VReg* const pRegistry,
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
    if (VRegInternal_SlotsOf(pRegistry)->RegEnumValueA)
        return VRegInternal_SlotsOf(pRegistry)->RegEnumValueA(
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

    VRegInternalWString name = {0, 0, NULL};
    VRegInternalBuffer value = {0, 0, NULL};
    VRegInternalBuffer nameA = {0, 0, NULL};
    VRegInternalBuffer text = {0, 0, NULL};
    DWORD dwType = 0;
    DWORD cbValue;
    DWORD cbBuffer;
    BOOL isString;
    BOOL nameFits;
    LSTATUS status;

    if (!lpValueName || !lpcchValueName || (lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VRegInternal_EnumValueOwned(
        pRegistry,
        hKey,
        dwIndex,
        &name,
        &dwType,
        lpcbData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternal_AnsiFromWide(VRegInternal_WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    isString = VRegInternal_IsStringType(dwType);
    cbValue = (DWORD)value.cb;
    if (lpcbData && isString)
    {
        status = VRegInternal_AnsiFromWide((LPCWCH)value.pb, value.cb / sizeof(wchar_t), &text);
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

    nameFits = VRegInternal_CopyAnsiName(&nameA, lpValueName, *lpcchValueName);
    if (lpData)
    {
        if (isString)
        {
            if (value.cb <= cbBuffer)
                VRegInternal_CopyBytes(lpData, value.pb, value.cb);
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
            VRegInternal_CopyBytes(lpData, value.pb, cbValue);
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
    VRegInternal_BufferFree(&text);
    VRegInternal_BufferFree(&nameA);
    VRegInternal_BufferFree(&value);
    VRegInternal_WStringFree(&name);
    return status;
}

// Native answers success without writing lpClass when only the class length is
// asked for, which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_QueryInfoKeyA(
    _In_ VReg* const pRegistry,
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
    if (VRegInternal_SlotsOf(pRegistry)->RegQueryInfoKeyA)
        return VRegInternal_SlotsOf(pRegistry)->RegQueryInfoKeyA(
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

    VRegInternalWString className = {0, 0, NULL};
    VRegInternalBuffer classA = {0, 0, NULL};
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
        return VReg_QueryInfoKeyW(
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

    status = VRegInternal_QueryInfoKeyOwned(
        pRegistry,
        hKey,
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

    status = VRegInternal_AnsiFromWide(VRegInternal_WStringText(&className), className.cch, &classA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (wantClassLength)
    {
        *lpcchClass = (DWORD)classA.cb;
        status = ERROR_SUCCESS;
        goto done;
    }
    if (!VRegInternal_CopyAnsiClass(&classA, lpClass, *lpcchClass))
    {
        *lpcchClass -= 1;
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchClass = (DWORD)classA.cb;
    status = ERROR_SUCCESS;

done:
    VRegInternal_BufferFree(&classA);
    VRegInternal_WStringFree(&className);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_LoadMUIStringA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegLoadMUIStringA)
        return VRegInternal_SlotsOf(pRegistry)->RegLoadMUIStringA(
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
VREG_DECL LSTATUS WINAPI VReg_GetValueA(
    _In_ VReg* const pRegistry,
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
    if (VRegInternal_SlotsOf(pRegistry)->RegGetValueA)
        return VRegInternal_SlotsOf(pRegistry)->RegGetValueA(
            pRegistry,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    VRegInternalWideArg value = {FALSE, {0, 0, NULL}};
    VRegInternalBuffer data = {0, 0, NULL};
    VRegInternalBuffer stored = {0, 0, NULL};
    VRegInternalBuffer text = {0, 0, NULL};
    VRegInternalBuffer rawText = {0, 0, NULL};
    VRegInternalBuffer storedText = {0, 0, NULL};
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

    status = VRegInternal_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternal_WideArgInit(lpValue, &value);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only string values are converted; anything else, and every failure to
    // find the value, is what the wide form answers.
    dwFlagsStored =
        (dwFlags & VREG_INTERNAL_RRF_WOW_SUBKEYS)
        | RRF_RT_ANY
        | RRF_NOEXPAND;
    dwStoredType = 0;
    statusStoredType = VReg_GetValueW(
        pRegistry,
        hkey,
        VRegInternal_WideArgText(&subKey),
        VRegInternal_WideArgText(&value),
        dwFlagsStored,
        &dwStoredType,
        NULL,
        NULL
    );
    if (statusStoredType != ERROR_SUCCESS || !VRegInternal_IsStringType(dwStoredType))
        goto forward;

    // A value whose type the flags reject is still read and reported, with the
    // failure status.
    dwFlagsTyped = dwFlags & ~RRF_ZEROONFAILURE;
    dwFlagsAnyType =
        (dwFlags & (VREG_INTERNAL_RRF_WOW_SUBKEYS | RRF_NOEXPAND))
        | RRF_RT_ANY;
    dwType = 0;
    cbQuery = 0;
    statusSized = VReg_GetValueW(
        pRegistry,
        hkey,
        VRegInternal_WideArgText(&subKey),
        VRegInternal_WideArgText(&value),
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
        VReg_GetValueW(
            pRegistry,
            hkey,
            VRegInternal_WideArgText(&subKey),
            VRegInternal_WideArgText(&value),
            dwFlagsAnyType,
            &dwType,
            NULL,
            &cbQuery
        );
    }

    status = VRegInternal_GetValueOwned(
        pRegistry,
        hkey,
        VRegInternal_WideArgText(&subKey),
        VRegInternal_WideArgText(&value),
        dwFlagsRead,
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
        status = VRegInternal_GetValueOwned(
            pRegistry,
            hkey,
            VRegInternal_WideArgText(&subKey),
            VRegInternal_WideArgText(&value),
            dwFlagsStored,
            &dwReadType,
            &stored
        );
        if (status != ERROR_SUCCESS)
            goto done;

        VReg_GetValueW(
            pRegistry,
            hkey,
            VRegInternal_WideArgText(&subKey),
            VRegInternal_WideArgText(&value),
            dwFlagsStored,
            &dwReadType,
            NULL,
            &cbStoredQuery
        );
        cbStoredRaw = cbStoredQuery - VRegInternal_MinDword(cbStoredQuery, 2);
    }

    status = VRegInternal_AnsiFromWide((LPCWCH)data.pb, data.cb / sizeof(wchar_t), &text);
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
        status = VRegInternal_AnsiFromWide(
            (LPCWCH)stored.pb,
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
        cbReported = VRegInternal_MaxDword(cbRawText, cbExpanded) + (cbExpanded > cbRawText ? 1 : 0);
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
            status = VRegInternal_AnsiFromWide(
                (LPCWCH)stored.pb,
                stored.cb / sizeof(wchar_t),
                &storedText
            );
            if (status != ERROR_SUCCESS)
                goto done;

            if (storedText.cb > 0 && storedText.cb <= cbBuffer)
            {
                if (cbStoredRaw <= cbBuffer)
                    VRegInternal_CopyBytes(pvData, stored.pb, VRegInternal_MinSize(cbStoredRaw, stored.cb));
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
            VRegInternal_CopyBytes(pbData, stored.pb, VRegInternal_MinSize(cbStoredRaw, stored.cb));
    }
    else
    {
        cbStored = cbQuery - VRegInternal_MinDword(cbQuery, dwType == REG_MULTI_SZ ? 4 : 2);
        if (cbStored <= cbBuffer)
            VRegInternal_CopyBytes(pbData, data.pb, VRegInternal_MinSize(cbStored, data.cb));
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
    status = VReg_GetValueW(
        pRegistry,
        hkey,
        VRegInternal_WideArgText(&subKey),
        VRegInternal_WideArgText(&value),
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );

done:
    VRegInternal_BufferFree(&storedText);
    VRegInternal_BufferFree(&rawText);
    VRegInternal_BufferFree(&text);
    VRegInternal_BufferFree(&stored);
    VRegInternal_BufferFree(&data);
    VRegInternal_WideArgFree(&value);
    VRegInternal_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_OpenKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegOpenKeyA)
        return VRegInternal_SlotsOf(pRegistry)->RegOpenKeyA(
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
    return VReg_OpenKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

VREG_DECL LSTATUS WINAPI VReg_OpenKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegOpenKeyW)
        return VRegInternal_SlotsOf(pRegistry)->RegOpenKeyW(
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
    return VReg_OpenKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

VREG_DECL LSTATUS WINAPI VReg_CreateKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegCreateKeyA)
        return VRegInternal_SlotsOf(pRegistry)->RegCreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VRegInternal_IsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VReg_CreateKeyExA(
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
    );
}

VREG_DECL LSTATUS WINAPI VReg_CreateKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegCreateKeyW)
        return VRegInternal_SlotsOf(pRegistry)->RegCreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VRegInternal_IsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VReg_CreateKeyExW(
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
    );
}

VREG_DECL LSTATUS WINAPI VReg_QueryValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegQueryValueA)
        return VRegInternal_SlotsOf(pRegistry)->RegQueryValueA(
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
        status = VReg_OpenKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_QueryValueExA(
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
        VReg_CloseKey(
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
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_QueryValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegQueryValueW)
        return VRegInternal_SlotsOf(pRegistry)->RegQueryValueW(
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
        status = VReg_OpenKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_QueryValueExW(
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
        VReg_CloseKey(
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
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_SetValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegSetValueA)
        return VRegInternal_SlotsOf(pRegistry)->RegSetValueA(
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
        status = VReg_CreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_SetValueExA(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)(VRegInternal_AnsiLength(lpData) + 1)
    );
    if (hSubKey != hKey)
    {
        VReg_CloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_SetValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegSetValueW)
        return VRegInternal_SlotsOf(pRegistry)->RegSetValueW(
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
        status = VReg_CreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_SetValueExW(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)((VRegInternal_ViewOf(lpData).cch + 1) * sizeof(wchar_t))
    );
    if (hSubKey != hKey)
    {
        VReg_CloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_DeleteKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyA)
        return VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyA(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VReg_DeleteKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );
}

VREG_DECL LSTATUS WINAPI VReg_DeleteKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyW)
        return VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyW(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VReg_DeleteKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );
}

VREG_DECL LSTATUS WINAPI VReg_EnumKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegEnumKeyA)
        return VRegInternal_SlotsOf(pRegistry)->RegEnumKeyA(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VReg_EnumKeyExA(
        pRegistry,
        hKey,
        dwIndex,
        lpName,
        &cchNameInOut,
        NULL,
        NULL,
        NULL,
        NULL
    );
}

VREG_DECL LSTATUS WINAPI VReg_EnumKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegEnumKeyW)
        return VRegInternal_SlotsOf(pRegistry)->RegEnumKeyW(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VReg_EnumKeyExW(
        pRegistry,
        hKey,
        dwIndex,
        lpName,
        &cchNameInOut,
        NULL,
        NULL,
        NULL,
        NULL
    );
}

// The subkey of SetKeyValue and DeleteKeyValue is opened with KEY_SET_VALUE,
// the one right the call needs; the maximum allowed access, as the pre-Ex
// forms use, was passed over. Only a caller whose access is limited to
// something else could tell.
VREG_DECL LSTATUS WINAPI VReg_SetKeyValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegSetKeyValueA)
        return VRegInternal_SlotsOf(pRegistry)->RegSetKeyValueA(
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
        status = VReg_CreateKeyExA(
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

    status = VReg_SetValueExA(
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
        VReg_CloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_SetKeyValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegSetKeyValueW)
        return VRegInternal_SlotsOf(pRegistry)->RegSetKeyValueW(
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
        status = VReg_CreateKeyExW(
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

    status = VReg_SetValueExW(
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
        VReg_CloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_DeleteKeyValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyValueA)
        return VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VReg_OpenKeyExA(
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

    status = VReg_DeleteValueA(
        pRegistry,
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
    {
        VReg_CloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_DeleteKeyValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyValueW)
        return VRegInternal_SlotsOf(pRegistry)->RegDeleteKeyValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VReg_OpenKeyExW(
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

    status = VReg_DeleteValueW(
        pRegistry,
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
    {
        VReg_CloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

/// Places the narrowed values like the wide form places the stored ones, but
/// leaves the gaps alone and rounds the total up. When they do not all fit,
/// native answers the wide form's total for the stored values.
// The SDK types lpValueBuf as a string, but it holds binary values.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_QueryMultipleValuesA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegQueryMultipleValuesA)
        return VRegInternal_SlotsOf(pRegistry)->RegQueryMultipleValuesA(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    VRegInternalPlacedValue* pPlaced = NULL;
    VRegInternalWideArg name = {FALSE, {0, 0, NULL}};
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
        pPlaced = (VRegInternalPlacedValue*)VRegInternal_AllocZeroed((SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VReg_QueryValueExA(
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

        pPlaced[i].ibValue = i == 0 ? 0 : VRegInternal_AlignedToDword(cbEnd);
        cbEnd = pPlaced[i].ibValue + pPlaced[i].cbValue;
    }

    if (cbEnd > cbBuffer)
    {
        cbWideEnd = 0;
        for (i = 0; i < num_vals; ++i)
        {
            cbWide = 0;
            status = VRegInternal_WideArgInit(val_list[i].ve_valuename, &name);
            if (status == ERROR_SUCCESS)
            {
                status = VReg_QueryValueExW(
                    pRegistry,
                    hKey,
                    VRegInternal_WideArgText(&name),
                    NULL,
                    NULL,
                    NULL,
                    &cbWide
                );
            }
            VRegInternal_WideArgFree(&name);
            if (status != ERROR_SUCCESS)
                goto done;

            cbWideEnd = (i == 0 ? 0 : VRegInternal_AlignedToDword(cbWideEnd)) + cbWide;
        }
        *ldwTotsize = cbWideEnd;
        status = ERROR_MORE_DATA;
        goto done;
    }

    for (i = 0; i < num_vals; ++i)
    {
        cbRead = pPlaced[i].cbValue;
        status = VReg_QueryValueExA(
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
    *ldwTotsize = VRegInternal_AlignedToDword(cbEnd);
    status = ERROR_SUCCESS;

done:
    VRegInternal_Free(pPlaced);
    return status;
}

/// Native places each value on a @ref DWORD boundary, reads the ones that fit
/// the buffer in order, and fills the entries only when all of them did.
VREG_DECL LSTATUS WINAPI VReg_QueryMultipleValuesW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (VRegInternal_SlotsOf(pRegistry)->RegQueryMultipleValuesW)
        return VRegInternal_SlotsOf(pRegistry)->RegQueryMultipleValuesW(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    VRegInternalPlacedValue* pPlaced = NULL;
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
        pPlaced = (VRegInternalPlacedValue*)VRegInternal_AllocZeroed((SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VReg_QueryValueExW(
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
        ibValue = i == 0 ? 0 : VRegInternal_AlignedToDword(cbEnd);
        if (ibValue > cbEnd && cbEnd < cbBuffer)
            ZeroMemory(pbBuffer + cbEnd, VRegInternal_MinDword(ibValue, cbBuffer) - cbEnd);

        pPlaced[i].ibValue = ibValue;
        if (ibValue + cbValue <= cbBuffer)
        {
            cbRead = cbValue;
            status = VReg_QueryValueExW(
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
    VRegInternal_Free(pPlaced);
    return status;
}

// ==========================================================================
// Handles
// ==========================================================================

VREG_DECL BOOL WINAPI VReg_IsVirtualHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value != 0
        && value <= VREG_INTERNAL_VIRTUAL_HKEY_BASE
        && (value & 1) != 0;
}

VREG_DECL BOOL VRegInternal_IsPredefinedHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value >= (UINT_PTR)HKEY_CLASSES_ROOT
        && value <= (UINT_PTR)HKEY_CURRENT_USER_LOCAL_SETTINGS;
}

// ==========================================================================
// The handle space
// ==========================================================================

typedef struct
{
    void* volatile targets[VREG_INTERNAL_HANDLE_BLOCK_SIZE];
} VRegInternalHandleBlock;

struct VRegInternalHandleDirectory
{
    VRegInternalHandleBlock* volatile blocks[VREG_INTERNAL_HANDLE_DIRECTORY_SIZE];
};

static VRegInternalHandleSpace g_processSpace;
static INIT_ONCE g_processSpaceOnce = INIT_ONCE_STATIC_INIT;

VREG_DECL void VRegInternal_HandleSpaceInit(
    _Out_ VRegInternalHandleSpace* const pSpace,
    _In_ const LPCWSTR pszName
)
{
    HANDLE hSection;
    VRegInternalHandleSpaceShared* pShared;
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
        sizeof(VRegInternalHandleSpaceShared),
        pszName
    );
    if (!hSection)
        return;

    pShared = (VRegInternalHandleSpaceShared*)MapViewOfFile(
        hSection,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(VRegInternalHandleSpaceShared)
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
    previous = InterlockedCompareExchange(&pShared->magic, VREG_INTERNAL_HANDLE_SPACE_MAGIC, 0);
    if (previous != 0 && previous != VREG_INTERNAL_HANDLE_SPACE_MAGIC)
        return;

    pSpace->pShared = pShared;
}

/// Writes @p value in decimal at @p pszOut, which has room for @p cchOut
/// characters including the terminator, and answers the characters written.
static SIZE_T VRegInternal_WriteDecimal(
    _Out_writes_z_(cchOut) const LPWSTR pszOut,
    const SIZE_T cchOut,
    DWORD value
)
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

static BOOL CALLBACK VRegInternal_InitProcessHandleSpace(
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
    VRegInternal_WriteDecimal(wszName + cchPrefix, sizeof(wszName) / sizeof(wszName[0]) - cchPrefix, GetCurrentProcessId());
    VRegInternal_HandleSpaceInit(&g_processSpace, wszName);
    return TRUE;
}

VREG_DECL _Ret_notnull_ VRegInternalHandleSpace* VRegInternal_ProcessHandleSpace(void)
{
    // Handles issued from it outlive every teardown, so the instance is never
    // destroyed.
    InitOnceExecuteOnce(&g_processSpaceOnce, VRegInternal_InitProcessHandleSpace, NULL, NULL);
    return &g_processSpace;
}

VREG_DECL BOOL VRegInternal_HandleSpaceIsUsable(
    _In_ const VRegInternalHandleSpace* const pSpace
)
{
    return pSpace->pShared != NULL;
}

// A slot holds null while unbound, which WritePointerRelease's annotations
// reject; the exchange accepts it, and its full barrier covers the release.
static void VRegInternal_StoreTarget(
    _Inout_ void* volatile* const pSlot,
    _In_opt_ void* const pTarget
)
{
    InterlockedExchangePointer(pSlot, pTarget);
}

static VRegInternalHandleBlock* VRegInternal_FindBlock(
    _In_ const VRegInternalHandleSpace* const pSpace,
    const UINT_PTR block
)
{
    const VRegInternalHandleDirectory* const pDirectory = (const VRegInternalHandleDirectory*)ReadPointerAcquire(
        (PVOID volatile*)&pSpace->directories[block / VREG_INTERNAL_HANDLE_DIRECTORY_SIZE]
    );
    if (!pDirectory)
        return NULL;

    return (VRegInternalHandleBlock*)ReadPointerAcquire(
        (PVOID volatile*)&pDirectory->blocks[block % VREG_INTERNAL_HANDLE_DIRECTORY_SIZE]
    );
}

static BOOL VRegInternal_AddBlock(
    _Inout_ VRegInternalHandleSpace* const pSpace,
    const UINT_PTR block
)
{
    VRegInternalHandleDirectory* volatile* const pDirectorySlot = &pSpace->directories[block / VREG_INTERNAL_HANDLE_DIRECTORY_SIZE];
    VRegInternalHandleDirectory* pDirectory = *pDirectorySlot;
    VRegInternalHandleBlock* pBlock;

    if (!pDirectory)
    {
        pDirectory = (VRegInternalHandleDirectory*)VRegInternal_AllocZeroed(sizeof(VRegInternalHandleDirectory));
        if (!pDirectory)
            return FALSE;

        WritePointerRelease((PVOID volatile*)pDirectorySlot, pDirectory);
    }

    pBlock = (VRegInternalHandleBlock*)VRegInternal_AllocZeroed(sizeof(VRegInternalHandleBlock));
    if (!pBlock)
        return FALSE;

    WritePointerRelease((PVOID volatile*)&pDirectory->blocks[block % VREG_INTERNAL_HANDLE_DIRECTORY_SIZE], pBlock);
    return TRUE;
}

VREG_DECL _Ret_maybenull_ HKEY VRegInternal_HandleSpaceAllocate(
    _Inout_ VRegInternalHandleSpace* const pSpace,
    _In_opt_ void* const pTarget
)
{
    HKEY hKey = NULL;
    UINT_PTR index;
    VRegInternalHandleBlock* pBlock;

    if (!pSpace->pShared)
        return NULL;

    AcquireSRWLockExclusive(&pSpace->lock);

    if (pSpace->nextIndex == pSpace->endIndex)
    {
        // InterlockedIncrement returns the new value; the block reserved is the
        // one before it.
        const UINT_PTR block = (UINT_PTR)InterlockedIncrement(&pSpace->pShared->nextBlock) - 1;
        if (block >= VREG_INTERNAL_HANDLE_BLOCK_COUNT || !VRegInternal_AddBlock(pSpace, block))
        {
            ReleaseSRWLockExclusive(&pSpace->lock);
            return NULL;
        }

        pSpace->nextIndex = block * VREG_INTERNAL_HANDLE_BLOCK_SIZE;
        pSpace->endIndex = pSpace->nextIndex + VREG_INTERNAL_HANDLE_BLOCK_SIZE;
    }

    index = pSpace->nextIndex++;
    pBlock = VRegInternal_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    VRegInternal_StoreTarget(&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE], pTarget);
    hKey = VRegInternal_VirtualHKeyFromIndex(index);

    ReleaseSRWLockExclusive(&pSpace->lock);
    return hKey;
}

VREG_DECL BOOL VRegInternal_HandleSpaceOwns(
    _In_ const VRegInternalHandleSpace* const pSpace,
    _In_opt_ const HKEY hKey
)
{
    return
        VReg_IsVirtualHKey(hKey)
        && VRegInternal_FindBlock(pSpace, VRegInternal_IndexFromVirtualHKey(hKey) / VREG_INTERNAL_HANDLE_BLOCK_SIZE) != NULL;
}

VREG_DECL void VRegInternal_HandleSpaceRelease(
    _Inout_ VRegInternalHandleSpace* const pSpace,
    _In_opt_ const HKEY hKey
)
{
    UINT_PTR index;
    VRegInternalHandleBlock* pBlock;

    if (!VReg_IsVirtualHKey(hKey))
        return;

    index = VRegInternal_IndexFromVirtualHKey(hKey);
    pBlock = VRegInternal_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    if (pBlock)
        VRegInternal_StoreTarget(&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE], NULL);
}

VREG_DECL _Ret_maybenull_ void* VRegInternal_HandleSpaceFind(
    _In_ const VRegInternalHandleSpace* const pSpace,
    _In_opt_ const HKEY hKey
)
{
    UINT_PTR index;
    const VRegInternalHandleBlock* pBlock;

    if (!VReg_IsVirtualHKey(hKey))
        return NULL;

    index = VRegInternal_IndexFromVirtualHKey(hKey);
    pBlock = VRegInternal_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    if (!pBlock)
        return NULL;

    return ReadPointerAcquire((PVOID volatile*)&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE]);
}

VREG_DECL _Ret_maybenull_ HKEY VRegInternal_VirtualHKeyFromIndex(
    const UINT_PTR index
)
{
    if (index >= VREG_INTERNAL_VIRTUAL_HKEY_COUNT)
        return NULL;

    return (HKEY)(VREG_INTERNAL_VIRTUAL_HKEY_BASE - (index << 1));
}

VREG_DECL UINT_PTR VRegInternal_IndexFromVirtualHKey(_In_opt_ const HKEY hKey)
{
    return (VREG_INTERNAL_VIRTUAL_HKEY_BASE - (UINT_PTR)hKey) >> 1;
}

// ==========================================================================
// Hives
// ==========================================================================

/// How a hive writes: into its own layer over the lower (copy-up), or
/// through to the lower's slots.
#define VREG_INTERNAL_HIVE_COPY_UP 0
#define VREG_INTERNAL_HIVE_WRITE_THROUGH 1

struct VRegHive
{
    /// Null for a hive held entirely in memory.
    const VRegHiveVtbl* pLowerVtbl;
    void* pvLowerContext;

    /// The lower's open with an access mask and redirects; set only for a
    /// lower over a registry, which the hive then opens through it.
    VRegInternal_HiveOpenLowerFunction* pfnOpenLower;

    /// @ref VREG_INTERNAL_HIVE_COPY_UP or @ref VREG_INTERNAL_HIVE_WRITE_THROUGH.
    DWORD dwWriteMode;

    SRWLOCK lock;
    volatile LONG isFrozen;

    /// Fixed for the life of the hive.
    VRegInternalNode* pRoot;

    /// `VRegInternalNode*`: nodes are created here and freed only with the hive; a
    /// deleted node stays allocated (marked deleted) so its handle keeps
    /// answering @ref ERROR_KEY_DELETED. Destroying the hive unbinds every
    /// handle it issued.
    VRegInternalArray nodes;
};

#define VREG_INTERNAL_NODE_AT(pArray, index) VREG_INTERNAL_AT(pArray, VRegInternalNode*, index)
#define VREG_INTERNAL_VALUE_AT(pArray, index) VREG_INTERNAL_AT(pArray, VRegInternalValue, index)
#define VREG_INTERNAL_WATCH_AT(pArray, index) VREG_INTERNAL_AT(pArray, VRegInternalChangeWatch, index)
#define VREG_INTERNAL_NAME_AT(pArray, index) VREG_INTERNAL_AT(pArray, VRegInternalWString, index)

static const VRegInternalWStringView VREG_INTERNAL_NO_NAME = {NULL, 0};

VREG_DECL int VRegInternal_CompareNames(
    const VRegInternalWStringView left,
    const VRegInternalWStringView right
)
{
    if (left.cch == 0 || right.cch == 0)
        return (left.cch == 0) == (right.cch == 0) ? 0 : (left.cch == 0 ? -1 : 1);

    return CompareStringOrdinal(left.pch, (int)left.cch, right.pch, (int)right.cch, TRUE) - CSTR_EQUAL;
}

/// Whether @p status says the thing asked about exists: found, or found but
/// not readable this way.
static BOOL VRegInternal_StatusExists(const LSTATUS status)
{
    return status == ERROR_SUCCESS || status == ERROR_MORE_DATA || status == ERROR_ACCESS_DENIED;
}

// --- Locking ---

VREG_DECL BOOL VRegInternal_HiveIsFrozen(_In_ VRegHive* const pHive)
{
    return ReadAcquire(&pHive->isFrozen) != 0;
}

/// Takes the lock shared, or nothing once frozen; answers whether it did.
/// Whether readers skip the lock: a frozen hive whose tree is all its own.
/// A custom hive keeps locking once frozen, since it still adds records.
static BOOL VRegInternal_HiveIsLockFree(_In_ VRegHive* const pHive)
{
    return VRegInternal_HiveIsFrozen(pHive) && pHive->dwWriteMode == VREG_INTERNAL_HIVE_COPY_UP;
}

static BOOL VRegInternal_HiveLockRead(_In_ VRegHive* const pHive)
{
    if (VRegInternal_HiveIsLockFree(pHive))
        return FALSE;

    AcquireSRWLockShared(&pHive->lock);
    return TRUE;
}

static void VRegInternal_HiveUnlockRead(
    _In_ VRegHive* const pHive,
    const BOOL isLocked
)
{
    if (isLocked)
        ReleaseSRWLockShared(&pHive->lock);
}

/// Takes the lock exclusive, or nothing once frozen; answers whether it did.
/// A check of @ref VRegInternal_HiveIsFrozen made before can be stale, since a
/// writer may wait here while @ref VRegHive_Freeze runs, so only this decides a
/// write.
static BOOL VRegInternal_HiveLockWrite(_In_ VRegHive* const pHive)
{
    AcquireSRWLockExclusive(&pHive->lock);
    if (!VRegInternal_HiveIsFrozen(pHive))
        return TRUE;

    ReleaseSRWLockExclusive(&pHive->lock);
    return FALSE;
}

static void VRegInternal_HiveUnlockWrite(_In_ VRegHive* const pHive)
{
    ReleaseSRWLockExclusive(&pHive->lock);
}

/// Takes the lock exclusively to add or update records, which a custom hive
/// does frozen or not; a frozen built-in hive takes none and answers FALSE.
/// Released with @ref VRegInternal_HiveUnlockWrite.
static BOOL VRegInternal_HiveLockRecords(_In_ VRegHive* const pHive)
{
    if (VRegInternal_HiveIsLockFree(pHive))
        return FALSE;

    AcquireSRWLockExclusive(&pHive->lock);
    if (!VRegInternal_HiveIsLockFree(pHive))
        return TRUE;

    ReleaseSRWLockExclusive(&pHive->lock);
    return FALSE;
}

// --- Nodes, values and watches ---

static void VRegInternal_ValueFree(_Inout_ VRegInternalValue* const pValue)
{
    VRegInternal_WStringFree(&pValue->name);
    VRegInternal_BufferFree(&pValue->data);
}

static void VRegInternal_NodeClearValues(_Inout_ VRegInternalNode* const pNode)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
        VRegInternal_ValueFree(&VREG_INTERNAL_VALUE_AT(&pNode->values, i));

    pNode->values.count = 0;
}

static void VRegInternal_NodeFree(_Inout_ VRegInternalNode* const pNode)
{
    VRegInternal_NodeClearValues(pNode);
    VRegInternal_ArrayFree(&pNode->values);
    VRegInternal_ArrayFree(&pNode->children);
    VRegInternal_ArrayFree(&pNode->watches);
    VRegInternal_WStringFree(&pNode->name);
    VRegInternal_WStringFree(&pNode->className);
    VRegInternal_Free(pNode);
}

/// Stamps @p pNode as written now.
static void VRegInternal_HiveTouch(_Inout_ VRegInternalNode* const pNode)
{
    GetSystemTimeAsFileTime(&pNode->lastWrite);
}

/// Makes @p pNode and its transparent ancestors upper keys, as a write to a
/// key below them needs. The root stays as it is: it is the mount point.
static void VRegInternal_HiveMaterialize(_Inout_ VRegInternalNode* const pNode)
{
    VRegInternalNode* pCurrent;
    for (pCurrent = pNode; pCurrent->pParent && pCurrent->isTransparent; pCurrent = pCurrent->pParent)
        pCurrent->isTransparent = FALSE;
}

static void VRegInternal_HiveDropWatches(_Inout_ VRegInternalNode* const pNode)
{
    SIZE_T i;
    for (i = 0; i < pNode->watches.count; ++i)
    {
        // The array's count bounds its initialized elements, which the
        // analyzer cannot see past the reallocation.
#pragma warning(suppress: 6001)
        CloseHandle(VREG_INTERNAL_WATCH_AT(&pNode->watches, i).hEvent);
    }

    pNode->watches.count = 0;
}

/// Signals and drops the watches of @p pNode that @p dwFilter selects;
/// @p isInherited limits them to those watching their subtree.
static void VRegInternal_HiveFire(
    _Inout_ VRegInternalNode* const pNode,
    const DWORD dwFilter,
    const BOOL isInherited
)
{
    SIZE_T i = 0;
    while (i < pNode->watches.count)
    {
        const VRegInternalChangeWatch* const pWatch = &VREG_INTERNAL_WATCH_AT(&pNode->watches, i);
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
        VRegInternal_ArrayRemoveAt(&pNode->watches, sizeof(*pWatch), i);
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
static void VRegInternal_HiveNotify(
    _Inout_ VRegInternalNode* const pNode,
    const DWORD dwFilter
)
{
    VRegInternalNode* pAncestor;
    VRegInternal_HiveFire(pNode, dwFilter, FALSE);
    for (pAncestor = pNode->pParent; pAncestor; pAncestor = pAncestor->pParent)
        VRegInternal_HiveFire(pAncestor, dwFilter, TRUE);
}

/// The child of @p pParent named @p name, or null; @p pIndex, if given,
/// receives where it is or where it would be inserted.
static _Ret_maybenull_ VRegInternalNode* VRegInternal_HiveFindChild(
    _In_ const VRegInternalNode* const pParent,
    const VRegInternalWStringView name,
    _Out_opt_ SIZE_T* const pIndex
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pParent->children.count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        if (VRegInternal_CompareNames(VRegInternal_WStringViewOf(&VREG_INTERNAL_NODE_AT(&pParent->children, mid)->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (pIndex)
        *pIndex = lo;
    if (lo < pParent->children.count)
    {
        VRegInternalNode* const pChild = VREG_INTERNAL_NODE_AT(&pParent->children, lo);
        if (VRegInternal_CompareNames(VRegInternal_WStringViewOf(&pChild->name), name) == 0)
            return pChild;
    }

    return NULL;
}

static _Ret_maybenull_ VRegInternalValue* VRegInternal_HiveFindValue(
    _In_ VRegInternalNode* const pNode,
    const VRegInternalWStringView name
)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
    {
        VRegInternalValue* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        if (VRegInternal_CompareNames(VRegInternal_WStringViewOf(&pValue->name), name) == 0)
            return pValue;
    }

    return NULL;
}

/// A new child of @p pParent, with its handle. The write lock must be held.
static LSTATUS VRegInternal_HiveAddChild(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pParent,
    const VRegInternalWStringView name,
    const VRegInternalWStringView className,
    const BOOL isVolatile,
    _Outptr_ VRegInternalNode** const ppChild
)
{
    SIZE_T index = 0;
    VRegInternalNode* const pNode = (VRegInternalNode*)VRegInternal_AllocZeroed(sizeof(VRegInternalNode));
    if (!pNode)
        return ERROR_OUTOFMEMORY;

    pNode->pHive = pHive;
    pNode->pParent = pParent;
    pNode->isVolatile = isVolatile;
    VRegInternal_HiveTouch(pNode);
    VRegInternal_HiveFindChild(pParent, name, &index);

    // Every allocation happens before the handle is bound, so a failure
    // leaves no handle pointing at a freed node.
    if (
        !VRegInternal_WStringAssign(&pNode->name, name.pch, name.cch)
        || !VRegInternal_WStringAssign(&pNode->className, className.pch, className.cch)
        || !VRegInternal_ArrayReserve(&pHive->nodes, sizeof(pNode), pHive->nodes.count + 1)
        || !VRegInternal_ArrayReserve(&pParent->children, sizeof(pNode), pParent->children.count + 1)
    )
    {
        VRegInternal_NodeFree(pNode);
        return ERROR_OUTOFMEMORY;
    }

    pNode->hKey = VRegInternal_HandleSpaceAllocate(VRegInternal_ProcessHandleSpace(), pNode);
    if (!pNode->hKey)
    {
        VRegInternal_NodeFree(pNode);
        return ERROR_OUTOFMEMORY;
    }

    VRegInternal_ArrayInsert(&pParent->children, sizeof(pNode), index, &pNode);
    VRegInternal_ArrayAppend(&pHive->nodes, sizeof(pNode), &pNode);
    VRegInternal_HiveTouch(pParent);
    *ppChild = pNode;
    return ERROR_SUCCESS;
}

/// Children of a deleted key answer as deleted too.
static void VRegInternal_HiveMarkDeleted(_Inout_ VRegInternalNode* const pNode)
{
    SIZE_T i;
    pNode->isDeleted = TRUE;
    VRegInternal_HiveFire(pNode, ~(DWORD)0, FALSE);
    for (i = 0; i < pNode->children.count; ++i)
        VRegInternal_HiveMarkDeleted(VREG_INTERNAL_NODE_AT(&pNode->children, i));
}

/// Marks @p pNode and everything below it deleted, unlinks it from its
/// parent and fires the watches on it and below it, and the name watches
/// on its parent. The write lock must be held.
static void VRegInternal_HiveRemove(_Inout_ VRegInternalNode* const pNode)
{
    if (pNode->pParent)
    {
        SIZE_T index = 0;
        VRegInternal_HiveFindChild(pNode->pParent, VRegInternal_WStringViewOf(&pNode->name), &index);
        VRegInternal_ArrayRemoveAt(&pNode->pParent->children, sizeof(pNode), index);
        VRegInternal_HiveTouch(pNode->pParent);
    }

    VRegInternal_HiveMarkDeleted(pNode);
    if (pNode->pParent)
        VRegInternal_HiveNotify(pNode->pParent, REG_NOTIFY_CHANGE_NAME);
}

// --- The lower registry ---

/// Appends @p cNames path components to @p pPath, separated by backslashes.
static BOOL VRegInternal_HiveJoinPath(
    _Inout_ VRegInternalWString* const pPath,
    _In_reads_(cNames) const VRegInternalWStringView* const pNames,
    const SIZE_T cNames
)
{
    SIZE_T i;
    for (i = 0; i < cNames; ++i)
    {
        if (!VRegInternal_AppendPathComponent(pPath, pNames[i].pch, pNames[i].cch))
            return FALSE;
    }

    return TRUE;
}

static BOOL VRegInternal_HiveAppendNodePath(
    _Inout_ VRegInternalWString* const pPath,
    _In_ const VRegInternalNode* const pNode
)
{
    if (!pNode->pParent)
        return TRUE;
    if (!VRegInternal_HiveAppendNodePath(pPath, pNode->pParent))
        return FALSE;

    return VRegInternal_AppendPathComponent(pPath, VRegInternal_WStringText(&pNode->name), pNode->name.cch);
}

/// Whether @p pNode's lower key shows through: the hive has a lower and no
/// key at or above @p pNode is opaque. Read under the lock.
static BOOL VRegInternal_HiveIsVisible(
    _In_ VRegHive* const pHive,
    _In_ const VRegInternalNode* const pNode
)
{
    const VRegInternalNode* pCurrent;
    if (!pHive->pLowerVtbl)
        return FALSE;

    for (pCurrent = pNode; pCurrent; pCurrent = pCurrent->pParent)
    {
        if (pCurrent->isOpaque)
            return FALSE;
    }

    return TRUE;
}

/// A result that asks for nothing yet.
static void VRegInternal_HiveResultInit(_Out_ VRegHiveResult* const pResult)
{
    ZeroMemory(pResult, sizeof(*pResult));
}

/// Opens the lower key at @p cPath components below @p pParent: through the
/// registry lower's own open, with @p samDesired and (when @p isRedirect) the
/// key handed out in @p pRedirect, or through the provider's `OpenKey`, which
/// answers @p ppToken.
static LSTATUS VRegInternal_HiveOpenLower(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired,
    const BOOL isRedirect,
    _Out_ VRegInternalRedirect* const pRedirect,
    _Outptr_result_maybenull_ void** const ppToken
)
{
    VRegInternalWString subKey = {0, 0, NULL};
    LSTATUS status;

    *ppToken = NULL;
    pRedirect->pRegistry = NULL;
    pRedirect->hKey = NULL;
    if (!VRegInternal_HiveJoinPath(&subKey, pPath, cPath))
    {
        VRegInternal_WStringFree(&subKey);
        return ERROR_OUTOFMEMORY;
    }

    if (pHive->pfnOpenLower)
        status = pHive->pfnOpenLower(pHive, pParent, VRegInternal_WStringText(&subKey), samDesired, isRedirect, pRedirect);
    else
        status = pHive->pLowerVtbl->OpenKey(pHive, pParent, VRegInternal_WStringText(&subKey), ppToken);

    VRegInternal_WStringFree(&subKey);
    return status;
}

/// Whether the lower has a key at @p cPath components below @p pNode, opened
/// with @p samDesired, as the status of that open.
static LSTATUS VRegInternal_HiveProbeLower(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired
)
{
    VRegInternalRedirect redirect;
    void* pToken;
    return VRegInternal_HiveOpenLower(pHive, pNode, pPath, cPath, samDesired, FALSE, &redirect, &pToken);
}

/// What `RegQueryInfoKeyW` counts for the lower key of @p pNode, derived by
/// enumerating its keys and values once; its own class stays empty and its
/// last write zero.
static LSTATUS VRegInternal_HiveDeriveInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _Inout_ VRegInternalKeyInfo* const pInfo
)
{
    VRegInternalWString name = {0, 0, NULL};
    VRegInternalWString className = {0, 0, NULL};
    VRegHiveResult result;
    DWORD dwType;
    DWORD cbData;
    DWORD i;
    LSTATUS status = ERROR_SUCCESS;

    for (i = 0;; ++i)
    {
        VRegInternal_WStringTruncate(&className, 0);
        VRegInternal_HiveResultInit(&result);
        result.pName = &name;
        result.pClassName = &className;
        status = pHive->pLowerVtbl->EnumKey(pHive, pNode, i, &result);
        if (status != ERROR_SUCCESS)
            break;

        ++pInfo->cSubKeys;
        pInfo->cchMaxSubKey = VRegInternal_MaxDword(pInfo->cchMaxSubKey, (DWORD)name.cch);
        pInfo->cchMaxClass = VRegInternal_MaxDword(pInfo->cchMaxClass, (DWORD)className.cch);
    }
    if (status != ERROR_NO_MORE_ITEMS)
        goto done;

    for (i = 0;; ++i)
    {
        cbData = 0;
        VRegInternal_HiveResultInit(&result);
        result.pName = &name;
        result.pdwType = &dwType;
        result.pcbData = &cbData;
        status = pHive->pLowerVtbl->EnumValue(pHive, pNode, i, &result);
        if (status != ERROR_SUCCESS)
            break;

        ++pInfo->cValues;
        pInfo->cchMaxValueName = VRegInternal_MaxDword(pInfo->cchMaxValueName, (DWORD)name.cch);
        pInfo->cbMaxValue = VRegInternal_MaxDword(pInfo->cbMaxValue, cbData);
    }
    if (status == ERROR_NO_MORE_ITEMS)
        status = ERROR_SUCCESS;

done:
    VRegInternal_WStringFree(&name);
    VRegInternal_WStringFree(&className);
    return status;
}

/// What `RegQueryInfoKeyW` reports for the lower key of @p pNode: the
/// lower's `QueryInfo`, or derived when it has none.
static LSTATUS VRegInternal_HiveQueryLowerInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _Inout_ VRegInternalKeyInfo* const pInfo
)
{
    VRegHiveResult result;
    if (!pHive->pLowerVtbl->QueryInfo)
        return VRegInternal_HiveDeriveInfo(pHive, pNode, pInfo);

    VRegInternal_HiveResultInit(&result);
    result.pInfo = pInfo;
    return pHive->pLowerVtbl->QueryInfo(pHive, pNode, &result);
}

/// The class of the lower key of @p pNode; empty when there is none.
static LSTATUS VRegInternal_HiveQueryLowerClass(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _Inout_ VRegInternalWString* const pClassName
)
{
    VRegInternalKeyInfo info;
    LSTATUS status;

    ZeroMemory(&info, sizeof(info));
    status = VRegInternal_HiveQueryLowerInfo(pHive, pNode, &info);
    if (status == ERROR_SUCCESS && !VRegInternal_WStringAssign(pClassName, VRegInternal_WStringText(&info.className), info.className.cch))
        status = ERROR_OUTOFMEMORY;
    else if (status == ERROR_FILE_NOT_FOUND)
        status = VRegInternal_WStringResize(pClassName, 0) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;

    VRegInternal_WStringFree(&info.className);
    return status;
}

// --- Creation and destruction ---

/// A hive over @p pLowerVtbl (null for one held entirely in memory), which
/// owns @p pvLowerContext from now on, failure included.
static LSTATUS VRegInternal_HiveCreate(
    _In_opt_ const VRegHiveVtbl* const pLowerVtbl,
    _In_opt_ void* const pvLowerContext,
    const DWORD dwWriteMode,
    _Outptr_ VRegHive** const ppHive
)
{
    VRegInternalNode* pRoot;
    VRegHive* const pHive = (VRegHive*)VRegInternal_AllocZeroed(sizeof(VRegHive));
    *ppHive = NULL;
    if (!pHive)
        return ERROR_OUTOFMEMORY;

    pHive->dwWriteMode = dwWriteMode;
    InitializeSRWLock(&pHive->lock);

    pRoot = (VRegInternalNode*)VRegInternal_AllocZeroed(sizeof(VRegInternalNode));
    if (!pRoot || !VRegInternal_ArrayAppend(&pHive->nodes, sizeof(pRoot), &pRoot))
    {
        VRegInternal_Free(pRoot);
        VRegHive_Destroy(pHive);
        return ERROR_OUTOFMEMORY;
    }

    pRoot->pHive = pHive;
    pRoot->hKey = VRegInternal_HandleSpaceAllocate(VRegInternal_ProcessHandleSpace(), pRoot);
    if (!pRoot->hKey)
    {
        VRegHive_Destroy(pHive);
        return ERROR_OUTOFMEMORY;
    }

    VRegInternal_HiveTouch(pRoot);
    if (pLowerVtbl)
    {
        pRoot->isTransparent = TRUE;
        pRoot->isLowerBacked = TRUE;
    }

    // Set last, so a failure above leaves the context to the caller.
    pHive->pLowerVtbl = pLowerVtbl;
    pHive->pvLowerContext = pvLowerContext;
    pHive->pRoot = pRoot;
    *ppHive = pHive;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegHive_Create(_Outptr_ VRegHive** const ppHive)
{
    return VRegInternal_HiveCreate(NULL, NULL, VREG_INTERNAL_HIVE_COPY_UP, ppHive);
}

VREG_DECL LSTATUS WINAPI VRegHive_CreateCustom(
    _In_ const VRegHiveVtbl* const pVtbl,
    _In_opt_ void* const pvContext,
    _Outptr_ VRegHive** const ppHive
)
{
    *ppHive = NULL;
    if (!pVtbl->OpenKey || !pVtbl->QueryValue || !pVtbl->EnumKey || !pVtbl->EnumValue)
        return ERROR_INVALID_PARAMETER;

    return VRegInternal_HiveCreate(pVtbl, pvContext, VREG_INTERNAL_HIVE_WRITE_THROUGH, ppHive);
}

VREG_DECL _Ret_maybenull_ void* WINAPI VRegHive_GetContext(
    _In_ VRegHive* const pHive
)
{
    return pHive->dwWriteMode == VREG_INTERNAL_HIVE_WRITE_THROUGH ? pHive->pvLowerContext : NULL;
}

VREG_DECL _Ret_maybenull_ void* WINAPI VRegHiveKey_GetToken(
    _In_ VRegHiveKey* const pKey
)
{
    return pKey->pToken;
}

VREG_DECL _Ret_notnull_ LPCWSTR WINAPI VRegHiveKey_GetName(
    _In_ VRegHiveKey* const pKey
)
{
    return VRegInternal_WStringText(&pKey->name);
}

VREG_DECL _Ret_maybenull_ VRegHiveKey* WINAPI VRegHiveKey_GetParent(
    _In_ VRegHiveKey* const pKey
)
{
    return pKey->pParent;
}

VREG_DECL BOOL WINAPI VRegHiveKey_EqualsPath(
    _In_ VRegHiveKey* const pKey,
    _In_ const LPCWSTR pszPath
)
{
    const VRegInternalNode* pNode = pKey;
    LPCWCH pchEnd = pszPath + VRegInternal_ViewOf(pszPath).cch;
    LPCWCH pchStart = pchEnd;

    // From the leaf upward against the components from the end backward.
    while (pNode->pParent)
    {
        VRegInternalWStringView component;
        pchStart = pchEnd;
        while (pchStart > pszPath && pchStart[-1] != L'\\')
            --pchStart;
        component.pch = pchStart;
        component.cch = (SIZE_T)(pchEnd - pchStart);
        if (component.cch == 0)
            return FALSE;
        if (VRegInternal_CompareNames(VRegInternal_WStringViewOf(&pNode->name), component) != 0)
            return FALSE;
        pNode = pNode->pParent;
        pchEnd = pchStart > pszPath ? pchStart - 1 : pszPath;
    }

    // The root is reached exactly when the last component matched began
    // the path (or the path is empty for the root itself); a separator
    // before it would name a key above the root.
    return pchStart == pszPath;
}

VREG_DECL LSTATUS WINAPI VRegHiveResult_SetValue(
    _In_ VRegHiveResult* const pResult,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    _In_ const DWORD cbData
)
{
    if (pResult->pdwType)
        *pResult->pdwType = dwType;
    if (pResult->pcbData)
        *pResult->pcbData = cbData;
    if (pResult->pData && !VRegInternal_BufferAssign(pResult->pData, pData, cbData))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegHiveResult_SetName(
    _In_ VRegHiveResult* const pResult,
    _In_ const LPCWSTR pszName
)
{
    const VRegInternalWStringView name = VRegInternal_ViewOf(pszName);
    if (pResult->pName && !VRegInternal_WStringAssign(pResult->pName, name.pch, name.cch))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegHiveResult_SetClass(
    _In_ VRegHiveResult* const pResult,
    _In_ const LPCWSTR pszClassName
)
{
    const VRegInternalWStringView className = VRegInternal_ViewOf(pszClassName);
    VRegInternalWString* const pClassName = pResult->pInfo ? &pResult->pInfo->className : pResult->pClassName;
    if (pClassName && !VRegInternal_WStringAssign(pClassName, className.pch, className.cch))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegHiveResult_SetLastWrite(
    _In_ VRegHiveResult* const pResult,
    _In_ const FILETIME* const pLastWrite
)
{
    if (pResult->pInfo)
        pResult->pInfo->lastWrite = *pLastWrite;
    else if (pResult->pLastWrite)
        *pResult->pLastWrite = *pLastWrite;

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegHiveResult_SetInfo(
    _In_ VRegHiveResult* const pResult,
    _In_ const DWORD cSubKeys,
    _In_ const DWORD cchMaxSubKey,
    _In_ const DWORD cchMaxClass,
    _In_ const DWORD cValues,
    _In_ const DWORD cchMaxValueName,
    _In_ const DWORD cbMaxValue
)
{
    VRegInternalKeyInfo* const pInfo = pResult->pInfo;
    if (pInfo)
    {
        pInfo->cSubKeys = cSubKeys;
        pInfo->cchMaxSubKey = cchMaxSubKey;
        pInfo->cchMaxClass = cchMaxClass;
        pInfo->cValues = cValues;
        pInfo->cchMaxValueName = cchMaxValueName;
        pInfo->cbMaxValue = cbMaxValue;
    }

    return ERROR_SUCCESS;
}

VREG_DECL int WINAPI VReg_CompareNames(
    _In_ const LPCWSTR pszLeft,
    _In_ const LPCWSTR pszRight
)
{
    return VRegInternal_CompareNames(VRegInternal_ViewOf(pszLeft), VRegInternal_ViewOf(pszRight));
}

VREG_DECL void WINAPI VRegHive_Destroy(_In_opt_ VRegHive* const pHive)
{
    SIZE_T i;
    if (!pHive)
        return;

    for (i = 0; i < pHive->nodes.count; ++i)
    {
        VRegInternalNode* const pNode = VREG_INTERNAL_NODE_AT(&pHive->nodes, i);
        VRegInternal_HiveDropWatches(pNode);
        VRegInternal_HandleSpaceRelease(VRegInternal_ProcessHandleSpace(), pNode->hKey);
        VRegInternal_NodeFree(pNode);
    }

    VRegInternal_ArrayFree(&pHive->nodes);
    if (pHive->pLowerVtbl && pHive->pLowerVtbl->Destroy)
        pHive->pLowerVtbl->Destroy(pHive);
    VRegInternal_Free(pHive);
}

VREG_DECL void WINAPI VRegHive_Freeze(_In_ VRegHive* const pHive)
{
    SIZE_T i;

    // Taking the lock once more waits out every writer still inside; readers
    // that see the flag afterwards skip the lock and see all their writes.
    AcquireSRWLockExclusive(&pHive->lock);
    WriteRelease(&pHive->isFrozen, 1);
    for (i = 0; pHive->dwWriteMode == VREG_INTERNAL_HIVE_COPY_UP && i < pHive->nodes.count; ++i)
        VRegInternal_HiveDropWatches(VREG_INTERNAL_NODE_AT(&pHive->nodes, i));

    ReleaseSRWLockExclusive(&pHive->lock);
}

VREG_DECL _Ret_notnull_ VRegInternalNode* VRegInternal_HiveRoot(
    _In_ VRegHive* const pHive
)
{
    return pHive->pRoot;
}

VREG_DECL HKEY VRegInternal_NodeHandle(_In_ const VRegInternalNode* const pNode)
{
    return pNode->hKey;
}

VREG_DECL _Ret_maybenull_ VRegInternalNode* VRegInternal_NodeOf(
    _In_opt_ const HKEY hKey
)
{
    return (VRegInternalNode*)VRegInternal_HandleSpaceFind(VRegInternal_ProcessHandleSpace(), hKey);
}

VREG_DECL _Ret_notnull_ VRegHive* VRegInternal_NodeHive(
    _In_ const VRegInternalNode* const pNode
)
{
    return pNode->pHive;
}

// --- Keys ---

/// Follows @p pPath through the upper layer from @p pParent as far as it
/// goes. @p pcMatched receives how many components were found and
/// @p ppCurrent the node they led to; when the path goes on from there,
/// @p pIsVisible says whether that node's lower key shows through.
static LSTATUS VRegInternal_HiveWalkUpper(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _Outptr_ VRegInternalNode** const ppCurrent,
    _Out_ SIZE_T* const pcMatched,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    VRegInternalNode* pCurrent = pParent;
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
            VRegInternalNode* const pChild = VRegInternal_HiveFindChild(pCurrent, pPath[i], NULL);
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
            *pIsVisible = VRegInternal_HiveIsVisible(pHive, pCurrent);
    }

    VRegInternal_HiveUnlockRead(pHive, isLocked);
    *ppCurrent = pCurrent;
    *pcMatched = i;
    return status;
}

/// The record of the key @p name below @p pParent, added (lower-backed,
/// keeping @p pToken) unless another thread added it meanwhile.
static LSTATUS VRegInternal_HiveAddRecord(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pParent,
    const VRegInternalWStringView name,
    _In_opt_ void* const pToken,
    _Outptr_ VRegInternalNode** const ppRecord
)
{
    VRegInternalNode* pRecord = NULL;
    LSTATUS status = ERROR_SUCCESS;

    *ppRecord = NULL;
    if (!VRegInternal_HiveLockRecords(pHive))
        return ERROR_ACCESS_DENIED;

    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        pRecord = VRegInternal_HiveFindChild(pParent, name, NULL);
        if (!pRecord)
        {
            status = VRegInternal_HiveAddChild(pHive, pParent, name, VREG_INTERNAL_NO_NAME, FALSE, &pRecord);
            if (status == ERROR_SUCCESS)
            {
                pRecord->isTransparent = TRUE;
                pRecord->isLowerBacked = TRUE;
                pRecord->pToken = pToken;
            }
        }
    }

    VRegInternal_HiveUnlockWrite(pHive);
    if (status == ERROR_SUCCESS)
        *ppRecord = pRecord;
    return status;
}

/// Opens a custom hive's key at @p cPath components below @p pCurrent one
/// key at a time, adding a record with its token for each, so a provider
/// finds every key it answered through the token it kept.
static LSTATUS VRegInternal_HiveOpenRecords(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* pCurrent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _Out_ VRegInternalOpenResult* const pResult
)
{
    SIZE_T i;
    for (i = 0; i < cPath; ++i)
    {
        VRegInternalRedirect redirect;
        void* pToken;
        LSTATUS status = VRegInternal_HiveOpenLower(pHive, pCurrent, pPath + i, 1, 0, FALSE, &redirect, &pToken);
        if (status == ERROR_SUCCESS)
            status = VRegInternal_HiveAddRecord(pHive, pCurrent, pPath[i], pToken, &pCurrent);
        if (status != ERROR_SUCCESS)
            return status;
    }

    pResult->pNode = pCurrent;
    return ERROR_SUCCESS;
}

/// Fires @p dwFilter on @p pNode after a write went through to the provider.
static void VRegInternal_HiveRecordChanged(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode,
    const DWORD dwFilter
)
{
    if (!VRegInternal_HiveLockRecords(pHive))
        return;

    if (!pNode->isDeleted)
    {
        VRegInternal_HiveTouch(pNode);
        VRegInternal_HiveNotify(pNode, dwFilter);
    }
    VRegInternal_HiveUnlockWrite(pHive);
}

VREG_DECL LSTATUS VRegInternal_HiveOpenKey(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired,
    _Out_ VRegInternalOpenResult* const pResult
)
{
    VRegInternalNode* pCurrent = pParent;
    SIZE_T cMatched = 0;
    BOOL isVisible = FALSE;
    BOOL isRedirect;
    REGSAM samLower;
    void* pToken;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->redirect.pRegistry = NULL;
    pResult->redirect.hKey = NULL;

    status = VRegInternal_HiveWalkUpper(pHive, pParent, pPath, cPath, &pCurrent, &cMatched, &isVisible);
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

    if (!pHive->pfnOpenLower)
    {
        status = VRegInternal_HiveOpenRecords(pHive, pCurrent, pPath + cMatched, cPath - cMatched, pResult);
        goto done;
    }

    // Nothing of the upper layer is at or below the key, so it is the lower
    // key's to answer. A frozen hive hands the lower key out; a writable one
    // only asks whether it exists.
    isRedirect = VRegInternal_HiveIsFrozen(pHive);
    samLower = isRedirect ? samDesired : KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES);
    status = VRegInternal_HiveOpenLower(
        pHive,
        pCurrent,
        pPath + cMatched,
        cPath - cMatched,
        samLower,
        isRedirect,
        &pResult->redirect,
        &pToken
    );
    if (status != ERROR_SUCCESS || isRedirect)
        goto done;

    // Writable: every open inside the mount answers with a node, or a write
    // through a lower handle handed out now would miss a later copy-up.
    if (!VRegInternal_HiveLockWrite(pHive))
    {
        // Frozen since the lower open: the frozen answer is a redirect.
        status = VRegInternal_HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
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
            VRegInternalNode* pChild = VRegInternal_HiveFindChild(pCurrent, pPath[i], NULL);
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

            status = VRegInternal_HiveAddChild(pHive, pCurrent, pPath[i], VREG_INTERNAL_NO_NAME, FALSE, &pChild);
            if (status != ERROR_SUCCESS)
                break;

            pChild->isTransparent = TRUE;
            pChild->isLowerBacked = TRUE;
            pCurrent = pChild;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VRegInternal_HiveUnlockWrite(pHive);

done:
    return status;
}

/// How far @p pPath exists in the upper layer below @p pParent, for
/// @ref VRegInternal_HiveCreateKey: @p pcUpper components, ending at @p ppLast;
/// @p pIsBehindTombstone when a tombstone stopped the walk; otherwise whether
/// the lower key of @p ppLast shows through, when the path goes on.
static LSTATUS VRegInternal_HiveMeasureUpper(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _Out_ SIZE_T* const pcUpper,
    _Outptr_ VRegInternalNode** const ppLast,
    _Out_ BOOL* const pIsBehindTombstone,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    VRegInternalNode* pCurrent = pParent;
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
            VRegInternalNode* const pChild = VRegInternal_HiveFindChild(pCurrent, pPath[cUpper], NULL);
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
            *pIsVisible = VRegInternal_HiveIsVisible(pHive, pCurrent);
    }

    VRegInternal_HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    *ppLast = pCurrent;
    return status;
}

/// @ref VRegInternal_HiveCreateKey on a custom hive: an existing key opens, and each
/// missing key is created through the provider, one at a time, the class
/// going to the last.
static LSTATUS VRegInternal_HiveCreateKeyThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _In_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    _Out_ VRegInternalOpenResult* const pResult,
    _Out_ LPDWORD const pdwDisposition
)
{
    VRegInternalNode* pCurrent = pParent;
    SIZE_T cMatched = 0;
    BOOL isVisible = FALSE;
    VRegInternalWString name = {0, 0, NULL};
    SIZE_T i;
    LSTATUS status;

    *pdwDisposition = REG_OPENED_EXISTING_KEY;
    status = VRegInternal_HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
    if (status != ERROR_FILE_NOT_FOUND)
        return status;
    if (VRegInternal_HiveIsFrozen(pHive) || !pHive->pLowerVtbl->CreateKey)
        return ERROR_ACCESS_DENIED;

    // The open recorded every key that exists; the rest is created.
    status = VRegInternal_HiveWalkUpper(pHive, pParent, pPath, cPath, &pCurrent, &cMatched, &isVisible);
    for (i = cMatched; status == ERROR_SUCCESS && i < cPath; ++i)
    {
        const BOOL isLast = i + 1 == cPath;
        void* pToken = NULL;
        BOOL isCreated = FALSE;
        VRegInternalNode* pRecord = NULL;
        if (!VRegInternal_WStringAssign(&name, pPath[i].pch, pPath[i].cch))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }

        status = pHive->pLowerVtbl->CreateKey(
            pHive,
            pCurrent,
            VRegInternal_WStringText(&name),
            isLast ? pszClassName : L"",
            dwOptions,
            &pToken,
            &isCreated
        );
        if (status == ERROR_SUCCESS)
            status = VRegInternal_HiveAddRecord(pHive, pCurrent, pPath[i], pToken, &pRecord);
        if (status != ERROR_SUCCESS)
            break;

        if (isCreated)
            VRegInternal_HiveRecordChanged(pHive, pCurrent, REG_NOTIFY_CHANGE_NAME);
        if (isLast && isCreated)
            *pdwDisposition = REG_CREATED_NEW_KEY;
        pCurrent = pRecord;
    }

    VRegInternal_WStringFree(&name);
    if (status == ERROR_SUCCESS)
        pResult->pNode = pCurrent;
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveCreateKey(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _In_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    _Out_ VRegInternalOpenResult* const pResult,
    _Out_ LPDWORD const pdwDisposition
)
{
    const VRegInternalWStringView className = VRegInternal_ViewOf(pszClassName);
    const BOOL isVolatile = (dwOptions & REG_OPTION_VOLATILE) != 0;
    SIZE_T cUpper = 0;
    SIZE_T cLower;
    VRegInternalNode* pLast = pParent;
    BOOL isBehindTombstone = FALSE;
    BOOL isVisible = FALSE;
    VRegInternalNode* pCurrent;
    SIZE_T i;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->redirect.pRegistry = NULL;
    pResult->redirect.hKey = NULL;
    *pdwDisposition = REG_OPENED_EXISTING_KEY;

    if (pHive->dwWriteMode == VREG_INTERNAL_HIVE_WRITE_THROUGH)
    {
        return VRegInternal_HiveCreateKeyThrough(
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
    }

    if (VRegInternal_HiveIsFrozen(pHive))
    {
        status = VRegInternal_HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
        return status == ERROR_FILE_NOT_FOUND ? ERROR_ACCESS_DENIED : status;
    }

    // How far the path exists: in the upper layer first, then below it.
    status = VRegInternal_HiveMeasureUpper(pHive, pParent, pPath, cPath, &cUpper, &pLast, &isBehindTombstone, &isVisible);
    if (status != ERROR_SUCCESS)
        goto done;

    cLower = cUpper;
    if (isVisible)
    {
        for (; cLower < cPath; ++cLower)
        {
            status = VRegInternal_HiveProbeLower(pHive, pLast, pPath + cUpper, cLower + 1 - cUpper, KEY_QUERY_VALUE);
            if (status == ERROR_FILE_NOT_FOUND)
            {
                status = ERROR_SUCCESS;
                break;
            }
            if (!VRegInternal_StatusExists(status))
                goto done;

            status = ERROR_SUCCESS;
        }
    }

    if (!VRegInternal_HiveLockWrite(pHive))
    {
        status = VRegInternal_HiveCreateKey(
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
            VRegInternalNode* pChild = VRegInternal_HiveFindChild(pCurrent, pPath[i], NULL);
            VRegInternalNode* pNew;
            if (pChild && !pChild->isTombstone)
            {
                pCurrent = pChild;
                continue;
            }

            if (pChild)
            {
                // Created again over a deleted lower key: nothing below shows.
                const VRegInternalWStringView newClass = isLast ? className : VREG_INTERNAL_NO_NAME;
                if (!VRegInternal_WStringAssign(&pChild->className, newClass.pch, newClass.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                    break;
                }

                pChild->isTombstone = FALSE;
                pChild->isOpaque = TRUE;
                VRegInternal_HiveTouch(pChild);
                VRegInternal_HiveMaterialize(pCurrent);
                VRegInternal_HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
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

            status = VRegInternal_HiveAddChild(
                pHive,
                pCurrent,
                pPath[i],
                isLast && !isInLower ? className : VREG_INTERNAL_NO_NAME,
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
                VRegInternal_HiveMaterialize(pCurrent);
                VRegInternal_HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
                if (isLast)
                    *pdwDisposition = REG_CREATED_NEW_KEY;
            }

            pCurrent = pNew;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VRegInternal_HiveUnlockWrite(pHive);

done:
    return status;
}

/// What @ref VRegInternal_HiveDeleteKey needs to know about @p pNode before it calls
/// the lower: the names of its tombstone children (a child that is not one
/// fails the call), whether its lower key and its parent's show through,
/// and its name.
static LSTATUS VRegInternal_HiveInspectForDelete(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _Inout_ VRegInternalArray* const pTombstones,
    _Out_ BOOL* const pIsVisible,
    _Out_ BOOL* const pIsParentVisible,
    _Inout_ VRegInternalWString* const pKeyName
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
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
        const VRegInternalNode* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);
        if (!pChild->isTombstone)
            status = ERROR_ACCESS_DENIED;
        else if (!VRegInternal_WStringArrayInsertSorted(pTombstones, VRegInternal_WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS)
    {
        *pIsVisible = VRegInternal_HiveIsVisible(pHive, pNode);
        *pIsParentVisible = VRegInternal_HiveIsVisible(pHive, pNode->pParent);
    }
    if (status == ERROR_SUCCESS)
    {
        const VRegInternalWStringView keyName = VRegInternal_WStringViewOf(&pNode->name);
        if (!VRegInternal_WStringAssign(pKeyName, keyName.pch, keyName.cch))
            status = ERROR_OUTOFMEMORY;
    }

    VRegInternal_HiveUnlockRead(pHive, isLocked);
    return status;
}

/// @ref VRegInternal_HiveDeleteKey on a custom hive: the provider deletes, and the
/// record is marked deleted.
static LSTATUS VRegInternal_HiveDeleteKeyThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath
)
{
    VRegInternalOpenResult opened;
    LSTATUS status;

    if (VRegInternal_HiveIsFrozen(pHive) || !pHive->pLowerVtbl->DeleteKey)
        return ERROR_ACCESS_DENIED;

    status = VRegInternal_HiveOpenKey(pHive, pParent, pPath, cPath, KEY_READ, &opened);
    if (status != ERROR_SUCCESS)
        return status;
    if (!opened.pNode->pParent)
        return ERROR_ACCESS_DENIED;

    status = pHive->pLowerVtbl->DeleteKey(pHive, opened.pNode);
    if (status != ERROR_SUCCESS || !VRegInternal_HiveLockRecords(pHive))
        return status;

    if (!opened.pNode->isDeleted)
        VRegInternal_HiveRemove(opened.pNode);
    VRegInternal_HiveUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegInternal_HiveDeleteKey(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath
)
{
    VRegInternalOpenResult opened;
    VRegInternalNode* pNode;
    VRegInternalArray tombstones = {NULL, 0, 0};
    BOOL isVisible = FALSE;
    BOOL isParentVisible = FALSE;
    VRegInternalWString keyName = {0, 0, NULL};
    VRegInternalWString lowerName = {0, 0, NULL};
    BOOL hasLower = FALSE;
    SIZE_T i;
    LSTATUS status;

    if (pHive->dwWriteMode == VREG_INTERNAL_HIVE_WRITE_THROUGH)
        return VRegInternal_HiveDeleteKeyThrough(pHive, pParent, pPath, cPath);
    if (VRegInternal_HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    status = VRegInternal_HiveOpenKey(pHive, pParent, pPath, cPath, KEY_READ, &opened);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen since the check above, so the open answered with a redirect.
    if (!opened.pNode)
    {
        VRegInternal_CloseRedirect(&opened.redirect);
        return ERROR_ACCESS_DENIED;
    }

    pNode = opened.pNode;
    status = VRegInternal_HiveInspectForDelete(pHive, pNode, &tombstones, &isVisible, &isParentVisible, &keyName);
    if (status != ERROR_SUCCESS)
        goto done;

    // Whether a lower key sits at the node's path decides if a tombstone must
    // replace the node, even when the node is opaque and hides what is below
    // it. A lower subkey that no tombstone hides still makes the key non-empty.
    if (isParentVisible)
    {
        status = VRegInternal_HiveProbeLower(pHive, pNode, NULL, 0, KEY_ENUMERATE_SUB_KEYS);
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
            goto done;

        hasLower = status == ERROR_SUCCESS;
        status = ERROR_SUCCESS;
        for (i = 0; hasLower && isVisible; ++i)
        {
            VRegHiveResult entry;
            LSTATUS enumStatus;
            VRegInternal_HiveResultInit(&entry);
            entry.pName = &lowerName;
            enumStatus = pHive->pLowerVtbl->EnumKey(pHive, pNode, (DWORD)i, &entry);
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            if (enumStatus != ERROR_SUCCESS)
            {
                status = enumStatus;
                goto done;
            }
            if (!VRegInternal_WStringArrayContainsSorted(&tombstones, VRegInternal_WStringViewOf(&lowerName)))
            {
                status = ERROR_ACCESS_DENIED;
                goto done;
            }
        }
    }

    if (!VRegInternal_HiveLockWrite(pHive))
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
        VRegInternalNode* const pNodeParent = pNode->pParent;
        for (i = 0; i < pNode->children.count; ++i)
        {
            if (!VREG_INTERNAL_NODE_AT(&pNode->children, i)->isTombstone)
            {
                status = ERROR_ACCESS_DENIED;
                break;
            }
        }

        if (status == ERROR_SUCCESS)
        {
            VRegInternal_HiveRemove(pNode);
            if (hasLower)
            {
                VRegInternalNode* pTombstone;
                status = VRegInternal_HiveAddChild(
                    pHive,
                    pNodeParent,
                    VRegInternal_WStringViewOf(&keyName),
                    VREG_INTERNAL_NO_NAME,
                    FALSE,
                    &pTombstone
                );
                if (status == ERROR_SUCCESS)
                    pTombstone->isTombstone = TRUE;
            }

            VRegInternal_HiveMaterialize(pNodeParent);
        }
    }

    VRegInternal_HiveUnlockWrite(pHive);

done:
    VRegInternal_WStringArrayFree(&tombstones);
    VRegInternal_WStringFree(&keyName);
    VRegInternal_WStringFree(&lowerName);
    return status;
}

/// Whether @p pNode was deleted through VReg, read under the lock.
static BOOL VRegInternal_HiveIsDeleted(
    _In_ VRegHive* const pHive,
    _In_ const VRegInternalNode* const pNode
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
    const BOOL isDeleted = pNode->isDeleted;
    VRegInternal_HiveUnlockRead(pHive, isLocked);
    return isDeleted;
}

/// The subkeys of @p pNode deleted through the provider's `DeleteKey`, as
/// native's `RegDeleteTree` deletes them: in enumeration order, each subtree
/// before its key, stopping at the first failure. The names are read first,
/// so a provider whose deletes leave its enumeration as it was cannot loop
/// this.
static LSTATUS VRegInternal_HiveDeleteSubkeysThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode
)
{
    VRegInternalArray names = {NULL, 0, 0};
    VRegInternalWString name = {0, 0, NULL};
    SIZE_T i;
    LSTATUS status;

    for (i = 0;; ++i)
    {
        VRegHiveResult entry;
        VRegInternal_HiveResultInit(&entry);
        entry.pName = &name;
        status = pHive->pLowerVtbl->EnumKey(pHive, pNode, (DWORD)i, &entry);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VRegInternal_WStringArrayAppend(&names, VRegInternal_WStringViewOf(&name)))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    for (i = 0; i < names.count; ++i)
    {
        const VRegInternalWStringView child = VRegInternal_WStringViewOf(&VREG_INTERNAL_AT(&names, VRegInternalWString, i));
        VRegInternalOpenResult opened;
        status = VRegInternal_HiveOpenKey(pHive, pNode, &child, 1, KEY_READ, &opened);
        if (status != ERROR_SUCCESS)
            goto done;
        status = VRegInternal_HiveDeleteSubkeysThrough(pHive, opened.pNode);
        if (status != ERROR_SUCCESS)
            goto done;
        status = pHive->pLowerVtbl->DeleteKey(pHive, opened.pNode);
        if (status != ERROR_SUCCESS)
            goto done;
        if (VRegInternal_HiveLockRecords(pHive))
        {
            if (!opened.pNode->isDeleted)
                VRegInternal_HiveRemove(opened.pNode);
            VRegInternal_HiveUnlockWrite(pHive);
        }
    }

done:
    VRegInternal_WStringFree(&name);
    VRegInternal_WStringArrayFree(&names);
    return status;
}

/// The values of @p pNode deleted through the provider's `DeleteValue`, in
/// enumeration order, stopping at the first failure; names first, as in
/// @ref VRegInternal_HiveDeleteSubkeysThrough.
static LSTATUS VRegInternal_HiveDeleteValuesThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode
)
{
    VRegInternalArray names = {NULL, 0, 0};
    VRegInternalWString name = {0, 0, NULL};
    SIZE_T i;
    LSTATUS status;

    for (i = 0;; ++i)
    {
        VRegHiveResult entry;
        VRegInternal_HiveResultInit(&entry);
        entry.pName = &name;
        status = pHive->pLowerVtbl->EnumValue(pHive, pNode, (DWORD)i, &entry);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VRegInternal_WStringArrayAppend(&names, VRegInternal_WStringViewOf(&name)))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    for (i = 0; i < names.count; ++i)
    {
        status = pHive->pLowerVtbl->DeleteValue(
            pHive,
            pNode,
            VRegInternal_WStringText(&VREG_INTERNAL_AT(&names, VRegInternalWString, i))
        );
        if (status != ERROR_SUCCESS)
            goto done;
    }

done:
    VRegInternal_WStringFree(&name);
    VRegInternal_WStringArrayFree(&names);
    return status;
}

/// @ref VRegInternal_HiveDeleteTree on a custom hive: the provider empties the key,
/// through its `DeleteTree` or, without one, key by key and value by value
/// in native's order, and the records below it are marked deleted.
static LSTATUS VRegInternal_HiveDeleteTreeThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode
)
{
    const VRegHiveVtbl* const pVtbl = pHive->pLowerVtbl;
    LSTATUS status;
    if (VRegInternal_HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;
    if (!pVtbl->DeleteTree && (!pVtbl->DeleteKey || !pVtbl->DeleteValue))
        return ERROR_ACCESS_DENIED;
    if (VRegInternal_HiveIsDeleted(pHive, pNode))
        return ERROR_KEY_DELETED;

    if (pVtbl->DeleteTree)
    {
        status = pVtbl->DeleteTree(pHive, pNode);
    }
    else
    {
        status = VRegInternal_HiveDeleteSubkeysThrough(pHive, pNode);
        if (status == ERROR_SUCCESS)
            status = VRegInternal_HiveDeleteValuesThrough(pHive, pNode);
    }
    if (status != ERROR_SUCCESS || !VRegInternal_HiveLockRecords(pHive))
        return status;

    if (!pNode->isDeleted)
    {
        while (pNode->children.count != 0)
            VRegInternal_HiveRemove(VREG_INTERNAL_NODE_AT(&pNode->children, pNode->children.count - 1));

        VRegInternal_HiveTouch(pNode);
        VRegInternal_HiveNotify(pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    }
    VRegInternal_HiveUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegInternal_HiveDeleteTree(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode
)
{
    if (pHive->dwWriteMode == VREG_INTERNAL_HIVE_WRITE_THROUGH)
        return VRegInternal_HiveDeleteTreeThrough(pHive, pNode);
    if (!VRegInternal_HiveLockWrite(pHive))
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
    {
        VRegInternal_HiveUnlockWrite(pHive);
        return ERROR_KEY_DELETED;
    }

    while (pNode->children.count != 0)
        VRegInternal_HiveRemove(VREG_INTERNAL_NODE_AT(&pNode->children, pNode->children.count - 1));

    VRegInternal_NodeClearValues(pNode);
    pNode->isOpaque = pHive->pLowerVtbl != NULL;
    pNode->isTransparent = FALSE;
    pNode->isLowerBacked = FALSE;
    VRegInternal_HiveMaterialize(pNode);
    VRegInternal_HiveTouch(pNode);
    VRegInternal_HiveNotify(pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    VRegInternal_HiveUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

// --- Values ---

/// Reads value @p name of @p pNode from the upper layer. @p pIsFound says
/// whether the layer decided the answer; otherwise @p pIsVisible and
/// @p pViewPath describe the lower key to ask.
static LSTATUS VRegInternal_HiveQueryUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    const VRegInternalWStringView name,
    _Out_ BOOL* const pIsFound,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VRegInternalBuffer* const pData,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VRegInternalValue* pValue;

    *pIsFound = TRUE;
    *pIsVisible = FALSE;
    *pdwType = REG_NONE;
    *pcbData = 0;
    pValue = pNode->isDeleted ? NULL : VRegInternal_HiveFindValue(pNode, name);
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
            if (pData && !VRegInternal_BufferAssign(pData, pValue->data.pb, pValue->data.cb))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        *pIsFound = FALSE;
        *pIsVisible = VRegInternal_HiveIsVisible(pHive, pNode);
    }

    VRegInternal_HiveUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveQueryValue(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _In_ const LPCWSTR pszName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VRegInternalBuffer* const pData
)
{
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VRegHiveResult lower;
    LSTATUS status = VRegInternal_HiveQueryUpperValue(
        pHive,
        pNode,
        VRegInternal_ViewOf(pszName),
        &isFound,
        pdwType,
        pcbData,
        pData,
        &isVisible
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;
    if (!isVisible)
    {
        status = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    VRegInternal_HiveResultInit(&lower);
    lower.pdwType = pdwType;
    lower.pcbData = pcbData;
    lower.pData = pData;
    status = pHive->pLowerVtbl->QueryValue(pHive, pNode, pszName, &lower);

done:
    return status;
}

static LSTATUS VRegInternal_HiveStoreValue(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode,
    const VRegInternalWStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData
)
{
    VRegInternalBuffer bytes = {0, 0, NULL};
    VRegInternalValue* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (!VRegInternal_BufferAssign(&bytes, pData, cbData))
        return ERROR_OUTOFMEMORY;

    if (!VRegInternal_HiveLockWrite(pHive))
    {
        VRegInternal_BufferFree(&bytes);
        return ERROR_ACCESS_DENIED;
    }

    pValue = pNode->isDeleted ? NULL : VRegInternal_HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        VRegInternal_BufferFree(&pValue->data);
        pValue->dwType = dwType;
        pValue->data = bytes;
        pValue->isTombstone = FALSE;
        bytes.pb = NULL;
    }
    else
    {
        VRegInternalValue value;
        value.name.psz = NULL;
        value.name.cch = 0;
        value.name.cchCapacity = 0;
        value.dwType = dwType;
        value.data = bytes;
        value.isTombstone = FALSE;
        if (!VRegInternal_WStringAssign(&value.name, name.pch, name.cch))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (!VRegInternal_ArrayAppend(&pNode->values, sizeof(value), &value))
        {
            VRegInternal_WStringFree(&value.name);
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            bytes.pb = NULL;
        }
    }

    if (status == ERROR_SUCCESS)
    {
        VRegInternal_HiveMaterialize(pNode);
        VRegInternal_HiveTouch(pNode);
        VRegInternal_HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
    }

    VRegInternal_HiveUnlockWrite(pHive);
    if (bytes.pb)
        VRegInternal_BufferFree(&bytes);
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveSetValue(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode,
    _In_ const LPCWSTR pszName,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData
)
{
    LSTATUS status;
    if (pHive->dwWriteMode != VREG_INTERNAL_HIVE_WRITE_THROUGH)
        return VRegInternal_HiveStoreValue(pHive, pNode, VRegInternal_ViewOf(pszName), dwType, pData, cbData);

    if (VRegInternal_HiveIsFrozen(pHive) || !pHive->pLowerVtbl->SetValue)
        return ERROR_ACCESS_DENIED;
    if (VRegInternal_HiveIsDeleted(pHive, pNode))
        return ERROR_KEY_DELETED;

    status = pHive->pLowerVtbl->SetValue(pHive, pNode, pszName, dwType, pData, cbData);
    if (status == ERROR_SUCCESS)
        VRegInternal_HiveRecordChanged(pHive, pNode, REG_NOTIFY_CHANGE_LAST_SET);
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveDeleteValue(
    _In_ VRegHive* const pHive,
    _Inout_ VRegInternalNode* const pNode,
    _In_ const LPCWSTR pszName
)
{
    const VRegInternalWStringView name = VRegInternal_ViewOf(pszName);
    BOOL isLocked;
    BOOL isInUpper = FALSE;
    BOOL isInLower = FALSE;
    BOOL isVisible = FALSE;
    VRegInternalValue* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (VRegInternal_HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;
    if (pHive->dwWriteMode == VREG_INTERNAL_HIVE_WRITE_THROUGH)
    {
        if (!pHive->pLowerVtbl->DeleteValue)
            return ERROR_ACCESS_DENIED;
        if (VRegInternal_HiveIsDeleted(pHive, pNode))
            return ERROR_KEY_DELETED;

        status = pHive->pLowerVtbl->DeleteValue(pHive, pNode, pszName);
        if (status == ERROR_SUCCESS)
            VRegInternal_HiveRecordChanged(pHive, pNode, REG_NOTIFY_CHANGE_LAST_SET);
        return status;
    }

    isLocked = VRegInternal_HiveLockRead(pHive);
    pValue = pNode->isDeleted ? NULL : VRegInternal_HiveFindValue(pNode, name);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (pValue && pValue->isTombstone)
        status = ERROR_FILE_NOT_FOUND;
    else
        isInUpper = pValue != NULL;
    if (status == ERROR_SUCCESS)
        isVisible = VRegInternal_HiveIsVisible(pHive, pNode);
    VRegInternal_HiveUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS)
        goto done;

    // A missing lower key holds no value; any other failure to open it fails
    // the call, before the value's own answer is read.
    if (isVisible)
    {
        status = VRegInternal_HiveProbeLower(pHive, pNode, NULL, 0, KEY_QUERY_VALUE);
        if (status == ERROR_SUCCESS)
        {
            VRegHiveResult lower;
            LSTATUS queryStatus;
            VRegInternal_HiveResultInit(&lower);
            queryStatus = pHive->pLowerVtbl->QueryValue(pHive, pNode, pszName, &lower);
            if (queryStatus != ERROR_FILE_NOT_FOUND && !VRegInternal_StatusExists(queryStatus))
            {
                status = queryStatus;
                goto done;
            }

            isInLower = VRegInternal_StatusExists(queryStatus);
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

    if (!VRegInternal_HiveLockWrite(pHive))
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
        pValue = VRegInternal_HiveFindValue(pNode, name);
        if (isInLower)
        {
            if (pValue)
            {
                VRegInternal_BufferFree(&pValue->data);
                pValue->dwType = REG_NONE;
                pValue->isTombstone = TRUE;
            }
            else
            {
                VRegInternalValue tombstone;
                ZeroMemory(&tombstone, sizeof(tombstone));
                tombstone.dwType = REG_NONE;
                tombstone.isTombstone = TRUE;
                if (!VRegInternal_WStringAssign(&tombstone.name, name.pch, name.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                }
                else if (!VRegInternal_ArrayAppend(&pNode->values, sizeof(tombstone), &tombstone))
                {
                    VRegInternal_WStringFree(&tombstone.name);
                    status = ERROR_OUTOFMEMORY;
                }
            }
        }
        else if (pValue)
        {
            const SIZE_T index = (SIZE_T)(pValue - &VREG_INTERNAL_VALUE_AT(&pNode->values, 0));
            VRegInternal_ValueFree(pValue);
            VRegInternal_ArrayRemoveAt(&pNode->values, sizeof(*pValue), index);
        }

        if (status == ERROR_SUCCESS)
        {
            VRegInternal_HiveMaterialize(pNode);
            VRegInternal_HiveTouch(pNode);
            VRegInternal_HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
        }
    }

    VRegInternal_HiveUnlockWrite(pHive);

done:
    return status;
}

// --- Enumeration ---

/// The upper layer's part of @ref VRegInternal_HiveEnumKey. @p pIsFound says the
/// answer is complete. Otherwise, when @p ppClassNode is set, the entry is
/// an upper key made from a lower one whose class is still to be read; and
/// when not, the lower key of @p pNode (if @p pIsVisible) continues the
/// enumeration past @p pcUpper upper entries, with the names in @p pShadowed
/// hidden.
static LSTATUS VRegInternal_HiveEnumUpperKey(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Inout_ VRegInternalWString* const pClassName,
    _Out_ FILETIME* const pLastWrite,
    _Out_ BOOL* const pIsFound,
    _Outptr_result_maybenull_ VRegInternalNode** const ppClassNode,
    _Out_ BOOL* const pIsVisible,
    _Out_ DWORD* const pcUpper,
    _Inout_ VRegInternalArray* const pShadowed
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    DWORD cUpper = 0;
    SIZE_T i;

    *pIsFound = FALSE;
    *ppClassNode = NULL;
    *pIsVisible = FALSE;
    ZeroMemory(pLastWrite, sizeof(*pLastWrite));
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && !*pIsFound && !*ppClassNode && i < pNode->children.count; ++i)
    {
        VRegInternalNode* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);

        // A transparent child stands for a lower key; it is listed with the
        // lower ones.
        if (pChild->isTransparent)
            continue;

        if (!pChild->isTombstone && cUpper++ == dwIndex)
        {
            if (
                !VRegInternal_WStringAssign(pName, VRegInternal_WStringText(&pChild->name), pChild->name.cch)
                || !VRegInternal_WStringAssign(
                    pClassName,
                    VRegInternal_WStringText(&pChild->className),
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

            if (VRegInternal_HiveIsVisible(pHive, pChild))
                *ppClassNode = pChild;
            else
                *pIsFound = TRUE;
            break;
        }

        if (!VRegInternal_WStringArrayInsertSorted(pShadowed, VRegInternal_WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound && !*ppClassNode)
        *pIsVisible = VRegInternal_HiveIsVisible(pHive, pNode);

    VRegInternal_HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveEnumKey(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Inout_ VRegInternalWString* const pClassName,
    _Out_ FILETIME* const pLastWrite
)
{
    BOOL isFound = FALSE;
    VRegInternalNode* pClassNode = NULL;
    BOOL isVisible = FALSE;
    VRegInternalWString nameBuffer = {0, 0, NULL};
    VRegInternalWString classBuffer = {0, 0, NULL};
    VRegInternalArray shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD dwSkip;
    DWORD i;
    LSTATUS status = VRegInternal_HiveEnumUpperKey(
        pHive,
        pNode,
        dwIndex,
        pName,
        pClassName,
        pLastWrite,
        &isFound,
        &pClassNode,
        &isVisible,
        &cUpper,
        &shadowed
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;

    // An upper key that was made from a lower one has no class of its own.
    if (pClassNode)
    {
        status = VRegInternal_HiveQueryLowerClass(pHive, pClassNode, pClassName);
        goto done;
    }

    if (!isVisible)
    {
        status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    dwSkip = dwIndex - cUpper;
    for (i = 0;; ++i)
    {
        VRegHiveResult entry;
        FILETIME ft;
        ZeroMemory(&ft, sizeof(ft));
        VRegInternal_HiveResultInit(&entry);
        entry.pName = &nameBuffer;
        entry.pClassName = &classBuffer;
        entry.pLastWrite = &ft;
        status = pHive->pLowerVtbl->EnumKey(pHive, pNode, i, &entry);
        if (status != ERROR_SUCCESS)
        {
            // A lower key that is missing has nothing to enumerate.
            if (status == ERROR_FILE_NOT_FOUND)
                status = ERROR_NO_MORE_ITEMS;
            goto done;
        }

        if (VRegInternal_WStringArrayContainsSorted(&shadowed, VRegInternal_WStringViewOf(&nameBuffer)) || dwSkip-- != 0)
            continue;

        if (
            !VRegInternal_WStringAssign(pName, VRegInternal_WStringText(&nameBuffer), nameBuffer.cch)
            || !VRegInternal_WStringAssign(pClassName, VRegInternal_WStringText(&classBuffer), classBuffer.cch)
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
    VRegInternal_WStringArrayFree(&shadowed);
    VRegInternal_WStringFree(&nameBuffer);
    VRegInternal_WStringFree(&classBuffer);
    return status;
}

/// The upper layer's part of @ref VRegInternal_HiveEnumValue, like
/// @ref VRegInternal_HiveEnumUpperKey without the class step.
static LSTATUS VRegInternal_HiveEnumUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VRegInternalBuffer* const pData,
    _Out_ BOOL* const pIsFound,
    _Out_ BOOL* const pIsVisible,
    _Out_ DWORD* const pcUpper,
    _Inout_ VRegInternalArray* const pShadowed
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
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
        const VRegInternalValue* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        if (!pValue->isTombstone && cUpper++ == dwIndex)
        {
            *pIsFound = TRUE;
            if (
                !VRegInternal_WStringAssign(pName, VRegInternal_WStringText(&pValue->name), pValue->name.cch)
                || (pData && !VRegInternal_BufferAssign(pData, pValue->data.pb, pValue->data.cb))
            )
            {
                status = ERROR_OUTOFMEMORY;
                break;
            }

            *pdwType = pValue->dwType;
            *pcbData = (DWORD)pValue->data.cb;
            break;
        }

        if (!VRegInternal_WStringArrayInsertSorted(pShadowed, VRegInternal_WStringViewOf(&pValue->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound)
        *pIsVisible = VRegInternal_HiveIsVisible(pHive, pNode);

    VRegInternal_HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveEnumValue(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VRegInternalBuffer* const pData
)
{
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VRegInternalArray shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD dwSkip;
    DWORD i;
    LSTATUS status = VRegInternal_HiveEnumUpperValue(
        pHive,
        pNode,
        dwIndex,
        pName,
        pdwType,
        pcbData,
        pData,
        &isFound,
        &isVisible,
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

    dwSkip = dwIndex - cUpper;
    for (i = 0;; ++i)
    {
        VRegHiveResult entry;
        VRegInternal_HiveResultInit(&entry);
        entry.pName = pName;
        entry.pdwType = pdwType;
        status = pHive->pLowerVtbl->EnumValue(pHive, pNode, i, &entry);
        if (status != ERROR_SUCCESS)
        {
            // A lower key that is missing has nothing to enumerate.
            if (status == ERROR_FILE_NOT_FOUND)
                status = ERROR_NO_MORE_ITEMS;
            goto done;
        }

        if (VRegInternal_WStringArrayContainsSorted(&shadowed, VRegInternal_WStringViewOf(pName)) || dwSkip-- != 0)
            continue;

        // The scan fetched no data; enumerate the matching entry again with
        // it (or its size), so name, type and data all come from the one
        // call, should the entry have changed since the scan.
        entry.pcbData = pcbData;
        entry.pData = pData;
        status = pHive->pLowerVtbl->EnumValue(pHive, pNode, i, &entry);
        goto done;
    }

done:
    VRegInternal_WStringArrayFree(&shadowed);
    return status;
}

/// The upper layer's part of @ref VRegInternal_HiveQueryInfo: the counts and
/// maxima of the node's own entries into @p pInfo, the names that shadow
/// lower entries, and the node's relation to its lower key.
static LSTATUS VRegInternal_HiveQueryUpperInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _Inout_ VRegInternalKeyInfo* const pInfo,
    _Inout_ VRegInternalArray* const pShadowedKeys,
    _Inout_ VRegInternalArray* const pShadowedValues,
    _Out_ BOOL* const pIsTransparent,
    _Out_ BOOL* const pIsLowerBacked,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;

    *pIsTransparent = FALSE;
    *pIsLowerBacked = FALSE;
    *pIsVisible = FALSE;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && i < pNode->children.count; ++i)
    {
        const VRegInternalNode* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);
        if (pChild->isTransparent)
            continue;

        if (!VRegInternal_WStringArrayInsertSorted(pShadowedKeys, VRegInternal_WStringViewOf(&pChild->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pChild->isTombstone)
            continue;

        ++pInfo->cSubKeys;
        pInfo->cchMaxSubKey = VRegInternal_MaxDword(pInfo->cchMaxSubKey, (DWORD)pChild->name.cch);
        pInfo->cchMaxClass = VRegInternal_MaxDword(pInfo->cchMaxClass, (DWORD)pChild->className.cch);
    }

    for (i = 0; status == ERROR_SUCCESS && i < pNode->values.count; ++i)
    {
        const VRegInternalValue* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        if (!VRegInternal_WStringArrayInsertSorted(pShadowedValues, VRegInternal_WStringViewOf(&pValue->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pValue->isTombstone)
            continue;

        ++pInfo->cValues;
        pInfo->cchMaxValueName = VRegInternal_MaxDword(pInfo->cchMaxValueName, (DWORD)pValue->name.cch);
        pInfo->cbMaxValue = VRegInternal_MaxDword(pInfo->cbMaxValue, (DWORD)pValue->data.cb);
    }

    if (status == ERROR_SUCCESS)
    {
        *pIsTransparent = pNode->isTransparent;
        *pIsLowerBacked = pNode->isLowerBacked;
        pInfo->lastWrite = pNode->lastWrite;
        const VRegInternalWStringView className = VRegInternal_WStringViewOf(&pNode->className);
        if (!VRegInternal_WStringAssign(&pInfo->className, className.pch, className.cch))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        *pIsVisible = VRegInternal_HiveIsVisible(pHive, pNode);

    VRegInternal_HiveUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveQueryInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _Inout_ VRegInternalKeyInfo* const pInfo
)
{
    VRegInternalArray shadowedKeys = {NULL, 0, 0};
    VRegInternalArray shadowedValues = {NULL, 0, 0};
    BOOL isTransparent = FALSE;
    BOOL isLowerBacked = FALSE;
    BOOL isVisible = FALSE;
    VRegInternalKeyInfo lowerInfo;
    VRegHiveResult lower;
    SIZE_T i;
    LSTATUS status;

    pInfo->cSubKeys = 0;
    pInfo->cchMaxSubKey = 0;
    pInfo->cchMaxClass = 0;
    pInfo->cValues = 0;
    pInfo->cchMaxValueName = 0;
    pInfo->cbMaxValue = 0;
    ZeroMemory(&lowerInfo, sizeof(lowerInfo));

    status = VRegInternal_HiveQueryUpperInfo(
        pHive,
        pNode,
        pInfo,
        &shadowedKeys,
        &shadowedValues,
        &isTransparent,
        &isLowerBacked,
        &isVisible
    );
    if (status != ERROR_SUCCESS || !isVisible)
        goto done;

    status = VRegInternal_HiveQueryLowerInfo(pHive, pNode, &lowerInfo);
    if (status == ERROR_FILE_NOT_FOUND)
    {
        status = ERROR_SUCCESS;
        goto done;
    }
    if (status != ERROR_SUCCESS)
        goto done;

    // An upper entry that shadows a lower one is counted once.
    for (i = 0; i < shadowedKeys.count; ++i)
    {
        const VRegInternalWStringView childName = VRegInternal_WStringViewOf(&VREG_INTERNAL_NAME_AT(&shadowedKeys, i));
        const LSTATUS childStatus = VRegInternal_HiveProbeLower(pHive, pNode, &childName, 1, KEY_QUERY_VALUE);
        if (childStatus != ERROR_FILE_NOT_FOUND && !VRegInternal_StatusExists(childStatus))
        {
            status = childStatus;
            goto done;
        }
        if (VRegInternal_StatusExists(childStatus))
            --lowerInfo.cSubKeys;
    }

    for (i = 0; i < shadowedValues.count; ++i)
    {
        LSTATUS valueStatus;
        VRegInternal_HiveResultInit(&lower);
        valueStatus = pHive->pLowerVtbl->QueryValue(
            pHive,
            pNode,
            VRegInternal_WStringText(&VREG_INTERNAL_NAME_AT(&shadowedValues, i)),
            &lower
        );
        if (valueStatus != ERROR_FILE_NOT_FOUND && !VRegInternal_StatusExists(valueStatus))
        {
            status = valueStatus;
            goto done;
        }
        if (VRegInternal_StatusExists(valueStatus))
            --lowerInfo.cValues;
    }

    pInfo->cSubKeys += lowerInfo.cSubKeys;
    pInfo->cValues += lowerInfo.cValues;
    pInfo->cchMaxSubKey = VRegInternal_MaxDword(pInfo->cchMaxSubKey, lowerInfo.cchMaxSubKey);
    pInfo->cchMaxClass = VRegInternal_MaxDword(pInfo->cchMaxClass, lowerInfo.cchMaxClass);
    pInfo->cchMaxValueName = VRegInternal_MaxDword(pInfo->cchMaxValueName, lowerInfo.cchMaxValueName);
    pInfo->cbMaxValue = VRegInternal_MaxDword(pInfo->cbMaxValue, lowerInfo.cbMaxValue);
    if (
        isLowerBacked
        && !VRegInternal_WStringAssign(&pInfo->className, VRegInternal_WStringText(&lowerInfo.className), lowerInfo.className.cch)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    if (isTransparent)
        pInfo->lastWrite = lowerInfo.lastWrite;

done:
    VRegInternal_WStringArrayFree(&shadowedKeys);
    VRegInternal_WStringArrayFree(&shadowedValues);
    VRegInternal_WStringFree(&lowerInfo.className);
    return status;
}

// --- The rest ---

/// Visits @p pNode and everything below it, leaving out a transparent node
/// without values (the way to a lower key, which only the root can be while
/// it has upper keys below it). @p pPath is the node's path, extended and
/// restored for each child.
static LSTATUS VRegInternal_HiveListUpper(
    _In_ VRegHive* const pHive,
    _In_ const VRegInternalNode* const pNode,
    _Inout_ VRegInternalWString* const pPath,
    _In_ const VRegInternalUpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    const BOOL isBare = pNode->isTransparent && pNode->values.count == 0;
    const SIZE_T cchPath = pPath->cch;
    VRegInternalUpperRecord record;
    LSTATUS status;
    SIZE_T i;

    ZeroMemory(&record, sizeof(record));
    record.path = VRegInternal_WStringViewOf(pPath);
    if (!isBare)
    {
        record.kind = VREG_INTERNAL_UPPER_KEY;
        record.isOpaque = pNode->isOpaque;
        status = pfnVisit(pContext, &record);
        if (status != ERROR_SUCCESS)
            return status;
    }

    for (i = 0; i < pNode->values.count; ++i)
    {
        const VRegInternalValue* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        ZeroMemory(&record, sizeof(record));
        record.kind = VREG_INTERNAL_UPPER_VALUE;
        record.path = VRegInternal_WStringViewOf(pPath);
        record.name = VRegInternal_WStringViewOf(&pValue->name);
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
        const VRegInternalNode* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);
        if (
            (pPath->cch != 0 && !VRegInternal_WStringAppendChar(pPath, L'\\'))
            || !VRegInternal_WStringAppend(pPath, VRegInternal_WStringText(&pChild->name), pChild->name.cch)
        )
            return ERROR_OUTOFMEMORY;

        if (pChild->isTombstone)
        {
            ZeroMemory(&record, sizeof(record));
            record.kind = VREG_INTERNAL_UPPER_KEY;
            record.path = VRegInternal_WStringViewOf(pPath);
            record.isTombstone = TRUE;
            status = pfnVisit(pContext, &record);
        }
        else
        {
            status = VRegInternal_HiveListUpper(pHive, pChild, pPath, pfnVisit, pContext);
        }

        if (status != ERROR_SUCCESS)
            return status;

        VRegInternal_WStringTruncate(pPath, cchPath);
    }

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegInternal_HiveSnapshotUpper(
    _In_ VRegHive* const pHive,
    _In_ const VRegInternalUpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    VRegInternalWString path = {0, 0, NULL};
    const BOOL isLocked = VRegInternal_HiveLockRead(pHive);
    const LSTATUS status = VRegInternal_HiveListUpper(pHive, pHive->pRoot, &path, pfnVisit, pContext);
    VRegInternal_HiveUnlockRead(pHive, isLocked);
    VRegInternal_WStringFree(&path);
    return status;
}

VREG_DECL LSTATUS VRegInternal_HiveCheckKey(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode
)
{
    if (VRegInternal_HiveIsDeleted(pHive, pNode))
        return ERROR_KEY_DELETED;
    if (pHive->pLowerVtbl && pHive->pLowerVtbl->CheckKey)
        return pHive->pLowerVtbl->CheckKey(pHive, pNode);

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegInternal_HiveWatch(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
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

    // A frozen built-in hive never changes again; a custom one still does.
    isLocked = VRegInternal_HiveLockRecords(pHive);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (isLocked)
    {
        VRegInternalChangeWatch watch;
        watch.hEvent = hOwn;
        watch.isSubtree = bWatchSubtree != FALSE;
        watch.dwFilter = dwFilter;
        if (VRegInternal_ArrayAppend(&pNode->watches, sizeof(watch), &watch))
            hOwn = NULL;
        else
            status = ERROR_OUTOFMEMORY;
    }

    if (isLocked)
        VRegInternal_HiveUnlockWrite(pHive);
    if (hOwn)
        CloseHandle(hOwn);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegHive_NotifyChange(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveKey* const pKey,
    _In_ const DWORD dwFilter
)
{
    const DWORD dwFired = dwFilter & (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    LSTATUS status = ERROR_SUCCESS;

    if (!dwFired)
        return ERROR_INVALID_PARAMETER;

    // A frozen built-in hive has no watches left to fire.
    if (!VRegInternal_HiveLockRecords(pHive))
        return pKey->isDeleted ? ERROR_KEY_DELETED : ERROR_SUCCESS;

    if (pKey->isDeleted)
        status = ERROR_KEY_DELETED;
    else
        VRegInternal_HiveNotify(pKey, dwFired);
    VRegInternal_HiveUnlockWrite(pHive);
    return status;
}

// ==========================================================================
// Hive lowers over a registry
// ==========================================================================

// The lower of a layered hive: a key of another registry, reached through
// the slots of @ref VRegHiveVtbl like any lower. Every call opens the lower
// key by its path again (the lower key's path, then the record's path in
// the hive) and closes it after, as the hive did before it had a table, so
// a record holds no handle: one lower handle per record would save the
// opens but stay pinned for the hive's life.

typedef struct
{
    VReg* pRegistry;
    HKEY hRoot;
    VRegInternalWString path;
    REGSAM samView;
} VRegInternalRegistryLower;

VREG_DECL void VRegInternal_CloseRedirect(
    _In_ const VRegInternalRedirect* const pRedirect
)
{
    // A redirect always names the registry that answers its key.
    _Analysis_assume_(pRedirect->pRegistry);
    VReg_CloseKey(
        pRedirect->pRegistry,
        pRedirect->hKey
    );
}

/// Opens the lower key at @p pszSubKey below @p pNode with @p samDesired and
/// the lower's view.
static LSTATUS VRegInternal_RegistryLowerOpen(
    _In_ VRegHive* const pHive,
    _In_ const VRegInternalNode* const pNode,
    _In_ const LPCWSTR pszSubKey,
    const REGSAM samDesired,
    _Out_ PHKEY const phKey
)
{
    const VRegInternalRegistryLower* const pLower = (const VRegInternalRegistryLower*)pHive->pvLowerContext;
    const VRegInternalWStringView subKey = VRegInternal_ViewOf(pszSubKey);
    VRegInternalWString path = {0, 0, NULL};
    LSTATUS status;

    *phKey = NULL;
    if (
        !VRegInternal_WStringAssign(&path, VRegInternal_WStringText(&pLower->path), pLower->path.cch)
        || !VRegInternal_HiveAppendNodePath(&path, pNode)
        || (subKey.cch != 0 && !VRegInternal_AppendPathComponent(&path, subKey.pch, subKey.cch))
    )
    {
        VRegInternal_WStringFree(&path);
        return ERROR_OUTOFMEMORY;
    }

    status = VReg_OpenKeyExW(
        pLower->pRegistry,
        pLower->hRoot,
        path.cch == 0 ? NULL : path.psz,
        0,
        samDesired | pLower->samView,
        phKey
    );
    VRegInternal_WStringFree(&path);
    return status;
}

static void VRegInternal_RegistryLowerClose(
    _In_ VRegHive* const pHive,
    _In_ const HKEY hKey
)
{
    const VRegInternalRegistryLower* const pLower = (const VRegInternalRegistryLower*)pHive->pvLowerContext;
    VReg_CloseKey(
        pLower->pRegistry,
        hKey
    );
}

/// The hive's own open of the lower, with the access and redirect the
/// public slot leaves out (@ref VRegInternal_HiveOpenLowerFunction).
static LSTATUS VRegInternal_RegistryLowerOpenAs(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pParent,
    _In_ const LPCWSTR pszSubKey,
    const REGSAM samDesired,
    const BOOL isRedirect,
    _Out_ VRegInternalRedirect* const pRedirect
)
{
    HKEY hKey;
    const LSTATUS status = VRegInternal_RegistryLowerOpen(pHive, pParent, pszSubKey, samDesired, &hKey);
    pRedirect->pRegistry = NULL;
    pRedirect->hKey = NULL;
    if (status != ERROR_SUCCESS)
        return status;

    if (isRedirect)
    {
        pRedirect->pRegistry = ((const VRegInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
        pRedirect->hKey = hKey;
    }
    else
    {
        VRegInternal_RegistryLowerClose(pHive, hKey);
    }

    return ERROR_SUCCESS;
}

static LSTATUS WINAPI VRegInternal_RegistryLowerOpenKey(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveKey* const pParent,
    _In_ const LPCWSTR pszSubKey,
    _Outptr_result_maybenull_ void** const ppToken
)
{
    VRegInternalRedirect redirect;
    *ppToken = NULL;
    return VRegInternal_RegistryLowerOpenAs(pHive, pParent, pszSubKey, KEY_QUERY_VALUE, FALSE, &redirect);
}

static LSTATUS WINAPI VRegInternal_RegistryLowerQueryValue(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _In_ const LPCWSTR pszName,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    HKEY hKey;
    LSTATUS status = VRegInternal_RegistryLowerOpen(pHive, pNode, L"", KEY_QUERY_VALUE, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    if (pResult->pData)
    {
        status = VRegInternal_QueryValueOwned(pRegistry, hKey, pszName, pResult->pdwType, pResult->pData);
        if (status == ERROR_SUCCESS && pResult->pcbData)
            *pResult->pcbData = (DWORD)pResult->pData->cb;
    }
    else
    {
        if (pResult->pcbData)
            *pResult->pcbData = 0;
        status = VReg_QueryValueExW(
            pRegistry,
            hKey,
            pszName,
            NULL,
            pResult->pdwType,
            NULL,
            pResult->pcbData
        );
    }

    VRegInternal_RegistryLowerClose(pHive, hKey);
    return status;
}

static LSTATUS WINAPI VRegInternal_RegistryLowerEnumKey(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _In_ const DWORD dwIndex,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    VRegInternalWString className = {0, 0, NULL};
    DWORD cchMaxClass = 0;
    DWORD cchName = VREG_INTERNAL_INITIAL_NAME_CHARS;
    DWORD cchClass;
    FILETIME lastWrite;
    HKEY hKey;
    LSTATUS status = VRegInternal_RegistryLowerOpen(pHive, pNode, L"", KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg_QueryInfoKeyW(
        pRegistry,
        hKey,
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

    cchClass = cchMaxClass + 1;
    if (!VRegInternal_WStringResize(pResult->pName, cchName) || !VRegInternal_WStringResize(&className, cchClass))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    ZeroMemory(&lastWrite, sizeof(lastWrite));
    cchName = (DWORD)pResult->pName->cchCapacity;
    cchClass = (DWORD)className.cchCapacity;
    status = VReg_EnumKeyExW(
        pRegistry,
        hKey,
        dwIndex,
        pResult->pName->psz,
        &cchName,
        NULL,
        className.psz,
        &cchClass,
        &lastWrite
    );
    if (status != ERROR_SUCCESS)
        goto done;

    VRegInternal_WStringTruncate(pResult->pName, cchName);
    if (pResult->pClassName && !VRegInternal_WStringAssign(pResult->pClassName, className.psz, cchClass))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    if (pResult->pLastWrite)
        *pResult->pLastWrite = lastWrite;

done:
    VRegInternal_WStringFree(&className);
    VRegInternal_RegistryLowerClose(pHive, hKey);
    return status;
}

static LSTATUS WINAPI VRegInternal_RegistryLowerEnumValue(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _In_ const DWORD dwIndex,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    VRegInternalWString sizeName = {0, 0, NULL};
    DWORD cchName;
    HKEY hKey;
    LSTATUS status = VRegInternal_RegistryLowerOpen(pHive, pNode, L"", KEY_QUERY_VALUE, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    status = VRegInternal_EnumValueOwned(pRegistry, hKey, dwIndex, pResult->pName, pResult->pdwType, pResult->pData);
    if (status != ERROR_SUCCESS)
        goto done;

    if (pResult->pData)
    {
        if (pResult->pcbData)
            *pResult->pcbData = (DWORD)pResult->pData->cb;
        goto done;
    }
    if (!pResult->pcbData)
        goto done;

    // The size alone, read with the name the entry was just found under.
    if (!VRegInternal_WStringResize(&sizeName, pResult->pName->cch))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    cchName = (DWORD)(pResult->pName->cch + 1);
    *pResult->pcbData = 0;
    status = VReg_EnumValueW(
        pRegistry,
        hKey,
        dwIndex,
        sizeName.psz,
        &cchName,
        NULL,
        NULL,
        NULL,
        pResult->pcbData
    );

done:
    VRegInternal_WStringFree(&sizeName);
    VRegInternal_RegistryLowerClose(pHive, hKey);
    return status;
}

static LSTATUS WINAPI VRegInternal_RegistryLowerQueryInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalNode* const pNode,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    VRegInternalKeyInfo* const pInfo = pResult->pInfo;
    HKEY hKey;
    LSTATUS status = VRegInternal_RegistryLowerOpen(pHive, pNode, L"", KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    _Analysis_assume_(pInfo);
    status = VRegInternal_QueryInfoKeyOwned(
        pRegistry,
        hKey,
        &pInfo->className,
        NULL,
        NULL,
        &pInfo->cSubKeys,
        &pInfo->cchMaxSubKey,
        &pInfo->cchMaxClass,
        &pInfo->cValues,
        &pInfo->cchMaxValueName,
        &pInfo->cbMaxValue,
        NULL,
        &pInfo->lastWrite
    );
    VRegInternal_RegistryLowerClose(pHive, hKey);
    return status;
}

static void WINAPI VRegInternal_RegistryLowerDestroy(_In_ VRegHive* const pHive)
{
    VRegInternalRegistryLower* const pLower = (VRegInternalRegistryLower*)pHive->pvLowerContext;
    if (!pLower)
        return;

    VRegInternal_WStringFree(&pLower->path);
    VRegInternal_Free(pLower);
}

// Copy-up never writes through to the lower, and nothing asks the lower to
// check a key, so those slots stay null.
static const VRegHiveVtbl VRegInternal_RegistryLowerVtbl = {
    &VRegInternal_RegistryLowerOpenKey,
    NULL,
    NULL,
    NULL,
    &VRegInternal_RegistryLowerQueryValue,
    NULL,
    NULL,
    &VRegInternal_RegistryLowerEnumKey,
    &VRegInternal_RegistryLowerEnumValue,
    &VRegInternal_RegistryLowerQueryInfo,
    NULL,
    &VRegInternal_RegistryLowerDestroy,
};

VREG_DECL LSTATUS WINAPI VRegHive_CreateLayered(
    _In_ VReg* const pLower,
    _In_ const HKEY hLowerRoot,
    _In_opt_ const LPCWSTR pszLowerPath,
    _In_ const REGSAM samLowerView,
    _Outptr_ VRegHive** const ppHive
)
{
    const VRegInternalWStringView lowerPath = VRegInternal_ViewOf(pszLowerPath);
    VRegInternalRegistryLower* const pContext = (VRegInternalRegistryLower*)VRegInternal_AllocZeroed(sizeof(VRegInternalRegistryLower));
    LSTATUS status;
    *ppHive = NULL;
    if (!pContext)
        return ERROR_OUTOFMEMORY;

    pContext->pRegistry = pLower;
    pContext->hRoot = hLowerRoot;
    pContext->samView = samLowerView;
    if (!VRegInternal_WStringAssign(&pContext->path, lowerPath.pch, lowerPath.cch))
    {
        VRegInternal_Free(pContext);
        return ERROR_OUTOFMEMORY;
    }

    status = VRegInternal_HiveCreate(&VRegInternal_RegistryLowerVtbl, pContext, VREG_INTERNAL_HIVE_COPY_UP, ppHive);
    if (status != ERROR_SUCCESS)
    {
        VRegInternal_WStringFree(&pContext->path);
        VRegInternal_Free(pContext);
        return status;
    }

    (*ppHive)->pfnOpenLower = &VRegInternal_RegistryLowerOpenAs;
    return ERROR_SUCCESS;
}

// ==========================================================================
// The native registry
// ==========================================================================

VREG_DECL LSTATUS WINAPI VReg_CreateNative(
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VRegInternal_AllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->dwKind = VREG_INTERNAL_KIND_NATIVE;

    // A resolved export is a plain code address, which only a data pointer
    // cast can carry into the typed slot.
#define VREG_RESOLVE(name) *(void**)&pRegistry->implementation.native.name = VRegInternal_FindRegistryExport(#name);
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_RESOLVE)
#undef VREG_RESOLVE

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL _Ret_notnull_ VRegNativeFunctions* WINAPI VReg_GetNativeFunctions(
    _In_ VReg* const pRegistry
)
{
    return &pRegistry->implementation.native;
}

// ==========================================================================
// Owned reads
// ==========================================================================

VREG_DECL DWORD VRegInternal_Grown(const DWORD cbOld, const DWORD cbRequired)
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}

VREG_DECL LSTATUS VRegInternal_QueryValueOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Out_opt_ const LPDWORD lpType,
    _Inout_ VRegInternalBuffer* const pData
)
{
    DWORD dwType = 0;
    DWORD cbData = 0;
    const LSTATUS sized = VReg_QueryValueExW(
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
        if (!VRegInternal_BufferResize(pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VReg_QueryValueExW(
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
            cbData = VRegInternal_Grown(cbData, cbRead);
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

VREG_DECL LSTATUS VRegInternal_GetValueOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValue,
    _In_ const DWORD dwFlags,
    _Out_opt_ const LPDWORD pdwType,
    _Inout_ VRegInternalBuffer* const pData
)
{
    DWORD dwType = 0;
    DWORD cbData = 0;
    const LSTATUS sized = VReg_GetValueW(
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
        if (!VRegInternal_BufferResize(pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VReg_GetValueW(
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
            cbData = VRegInternal_Grown(cbData, cbRead);
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

VREG_DECL LSTATUS VRegInternal_EnumKeyOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName
)
{
    DWORD cchName = VREG_INTERNAL_INITIAL_NAME_CHARS;
    for (;;)
    {
        DWORD cchRead = cchName;
        LSTATUS status;
        if (!VRegInternal_WStringResize(pName, cchName))
            return ERROR_OUTOFMEMORY;

        status = VReg_EnumKeyExW(
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
        if (status == ERROR_MORE_DATA && cchName < VREG_INTERNAL_MAX_CLASS_CHARS)
        {
            cchName = VRegInternal_MinDword(VRegInternal_Grown(cchName, cchRead), VREG_INTERNAL_MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VRegInternal_WStringTruncate(pName, cchRead);
        return status;
    }
}

VREG_DECL LSTATUS VRegInternal_EnumValueOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Out_opt_ const LPDWORD lpType,
    _Inout_opt_ VRegInternalBuffer* const pData
)
{
    DWORD cchName = VREG_INTERNAL_INITIAL_NAME_CHARS;
    DWORD cbData = 0;
    BOOL isSized = pData == NULL;
    for (;;)
    {
        DWORD cchRead = cchName;
        DWORD cbRead = cbData;
        DWORD dwType = 0;
        LSTATUS status;
        if (!VRegInternal_WStringResize(pName, cchName))
            return ERROR_OUTOFMEMORY;

        status = VReg_EnumValueW(
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
            cchName = VRegInternal_MinDword(VRegInternal_Grown(cchName, cchRead + 1), VREG_INTERNAL_MAX_VALUE_NAME_CHARS);
            if (pData)
            {
                cbData = VRegInternal_Grown(cbData, cbRead);
                if (isSized && !VRegInternal_BufferResize(pData, cbData))
                    return ERROR_OUTOFMEMORY;
            }
            continue;
        }
        if (status == ERROR_SUCCESS && !isSized)
        {
            cbData = cbRead;
            if (!VRegInternal_BufferResize(pData, cbData))
                return ERROR_OUTOFMEMORY;

            isSized = TRUE;
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            VRegInternal_WStringTruncate(pName, cchRead);
            if (pData)
                pData->cb = cbRead;
            if (lpType)
                *lpType = dwType;
        }
        return status;
    }
}

VREG_DECL LSTATUS VRegInternal_QueryInfoKeyOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Inout_ VRegInternalWString* const pClass,
    _Inout_opt_ const LPDWORD lpcchClassReported,
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
    DWORD cchClass = VREG_INTERNAL_INITIAL_NAME_CHARS;
    for (;;)
    {
        DWORD cchRead = cchClass;
        LSTATUS status;
        if (!VRegInternal_WStringResize(pClass, cchClass))
            return ERROR_OUTOFMEMORY;

        status = VReg_QueryInfoKeyW(
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
        if ((status == ERROR_MORE_DATA || status == ERROR_INSUFFICIENT_BUFFER) && cchClass < VREG_INTERNAL_MAX_CLASS_CHARS)
        {
            cchClass = VRegInternal_MinDword(cchClass * 2, VREG_INTERNAL_MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VRegInternal_WStringTruncate(pClass, cchRead);
        else if (lpcchClassReported && cchRead != cchClass)
            *lpcchClassReported = cchRead;
        return status;
    }
}

// ==========================================================================
// Paths
// ==========================================================================

VREG_DECL LSTATUS VRegInternal_SplitPath(
    const VRegInternalWStringView path,
    _Inout_ VRegInternalArray* const pComponents
)
{
    SIZE_T start = 0;
    if (path.cch != 0 && path.pch[0] == L'\\')
        return ERROR_BAD_PATHNAME;

    while (start < path.cch)
    {
        SIZE_T end = start;
        VRegInternalWStringView component;
        while (end < path.cch && path.pch[end] != L'\\')
            ++end;

        component.pch = path.pch + start;
        component.cch = end - start;
        if (component.cch != 0)
        {
            if (component.cch > VREG_INTERNAL_MAX_KEY_NAME_CHARS)
                return ERROR_INVALID_PARAMETER;
            if (!VRegInternal_ArrayAppend(pComponents, sizeof(component), &component))
                return ERROR_OUTOFMEMORY;
        }

        start = end + 1;
    }

    return ERROR_SUCCESS;
}

// ==========================================================================
// The flat functions
// ==========================================================================

VREG_DECL LSTATUS WINAPI VReg_OpenKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegOpenKeyExW(
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegOpenKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegOpenKeyExW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegOpenKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_CreateKeyExW(
    _In_ VReg* const pRegistry,
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
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegCreateKeyExW(
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

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegCreateKeyExW(
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

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegCreateKeyExW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegCreateKeyExW(
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
}

VREG_DECL LSTATUS WINAPI VReg_CloseKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegCloseKey(
            hKey
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegCloseKey(
            pRegistry,
            hKey
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegCloseKey)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegCloseKey(
            pRegistry,
            hKey
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_QueryValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegQueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegQueryValueExW(
            pRegistry,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegQueryValueExW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegQueryValueExW(
            pRegistry,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_SetValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegSetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegSetValueExW(
            pRegistry,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegSetValueExW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegSetValueExW(
            pRegistry,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_DeleteValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegDeleteValueW(
            hKey,
            lpValueName
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegDeleteValueW(
            pRegistry,
            hKey,
            lpValueName
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegDeleteValueW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegDeleteValueW(
            pRegistry,
            hKey,
            lpValueName
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_DeleteKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegDeleteKeyExW(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegDeleteKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegDeleteKeyExW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegDeleteKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_DeleteTreeW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegDeleteTreeW(
            hKey,
            lpSubKey
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegDeleteTreeW(
            pRegistry,
            hKey,
            lpSubKey
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegDeleteTreeW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegDeleteTreeW(
            pRegistry,
            hKey,
            lpSubKey
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_EnumKeyExW(
    _In_ VReg* const pRegistry,
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
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegEnumKeyExW(
            hKey,
            dwIndex,
            lpName,
            lpcchName,
            lpReserved,
            lpClass,
            lpcchClass,
            lpftLastWriteTime
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegEnumKeyExW(
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

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegEnumKeyExW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegEnumKeyExW(
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
}

VREG_DECL LSTATUS WINAPI VReg_EnumValueW(
    _In_ VReg* const pRegistry,
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
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegEnumValueW(
            hKey,
            dwIndex,
            lpValueName,
            lpcchValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegEnumValueW(
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

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegEnumValueW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegEnumValueW(
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
}

VREG_DECL LSTATUS WINAPI VReg_QueryInfoKeyW(
    _In_ VReg* const pRegistry,
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
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegQueryInfoKeyW(
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

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegQueryInfoKeyW(
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

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegQueryInfoKeyW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegQueryInfoKeyW(
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
}

VREG_DECL LSTATUS WINAPI VReg_LoadMUIStringW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegLoadMUIStringW(
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegLoadMUIStringW(
            pRegistry,
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegLoadMUIStringW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegLoadMUIStringW(
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
}

// The slot's _Post_z_ on pvData holds on every return; the analyzer does not
// carry it through the function pointer.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_GetValueW(
    _In_ VReg* const pRegistry,
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
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegGetValueW(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegGetValueW(
            pRegistry,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegGetValueW)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegGetValueW(
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
}

VREG_DECL LSTATUS WINAPI VReg_NotifyChangeKeyValue(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegNotifyChangeKeyValue(
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegNotifyChangeKeyValue(
            pRegistry,
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegNotifyChangeKeyValue)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegNotifyChangeKeyValue(
            pRegistry,
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }
}

VREG_DECL LSTATUS WINAPI VReg_FlushKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        return pRegistry->implementation.native.RegFlushKey(
            hKey
        );

    case VREG_INTERNAL_KIND_VIRTUAL:
        return VRegInternal_VirtualRegFlushKey(
            pRegistry,
            hKey
        );

    default:
        if (!pRegistry->implementation.custom.lpVtbl->RegFlushKey)
            return ERROR_CALL_NOT_IMPLEMENTED;

        return pRegistry->implementation.custom.lpVtbl->RegFlushKey(
            pRegistry,
            hKey
        );
    }
}

VREG_DECL void WINAPI VReg_Destroy(
    _In_opt_ VReg* const pRegistry
)
{
    if (!pRegistry)
        return;

    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        VRegInternal_Free(pRegistry);
        break;

    case VREG_INTERNAL_KIND_VIRTUAL:
        VRegInternal_VirtualDestroy(pRegistry);
        break;

    default:
        if (pRegistry->implementation.custom.lpVtbl->Destroy)
            pRegistry->implementation.custom.lpVtbl->Destroy(pRegistry);
        VRegInternal_Free(pRegistry);
        break;
    }
}

VREG_DECL LSTATUS WINAPI VReg_CreateCustom(
    _In_ const VRegVtbl* const pVtbl,
    _In_opt_ void* const pvContext,
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VRegInternal_AllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->dwKind = VREG_INTERNAL_KIND_CUSTOM;
    pRegistry->implementation.custom.lpVtbl = pVtbl;
    pRegistry->implementation.custom.pvContext = pvContext;

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL _Ret_maybenull_ void* WINAPI VReg_GetContext(
    _In_ VReg* const pRegistry
)
{
    return pRegistry->dwKind == VREG_INTERNAL_KIND_CUSTOM ? pRegistry->implementation.custom.pvContext : NULL;
}

// ==========================================================================
// Status
// ==========================================================================

_Post_satisfies_(return != ERROR_SUCCESS)
VREG_DECL LSTATUS VRegInternal_FailureStatus(const LSTATUS status)
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    return status;
}

// ==========================================================================
// Value types
// ==========================================================================

VREG_DECL BOOL VRegInternal_IsStringType(const DWORD dwType)
{
    return dwType == REG_SZ
        || dwType == REG_EXPAND_SZ
        || dwType == REG_MULTI_SZ;
}

// ==========================================================================
// The virtual registry
// ==========================================================================

#include <sddl.h>

typedef struct
{
    HKEY hRoot;

    /// `VRegInternalWString` components.
    VRegInternalArray path;

    VRegHive* pHive;

    /// @ref KEY_WOW64_64KEY, @ref KEY_WOW64_32KEY or 0 for both views.
    REGSAM samView;

    /// The native key path as `VRegInternalWString` components without
    /// `WOW6432Node`; empty when the mount has none.
    VRegInternalArray nativePath;
} VRegInternalMountPoint;

/// A mount table is replaced, never changed: a new one copies the mount points
/// of the old (sharing their strings, which only the newest table frees) and
/// adds one. Old tables stay reachable through @ref pPrevious until the
/// registry is destroyed, since a call in flight may still read them.
typedef struct VRegInternalMountTable VRegInternalMountTable;

struct VRegInternalMountTable
{
    /// `VRegInternalMountPoint`.
    VRegInternalArray mounts;

    VRegInternalMountTable* pPrevious;
};

/// Where a key handle plus a subkey path lands.
typedef struct
{
    /// The node, when the key is inside a mount.
    VRegInternalNode* pNode;

    /// The hive of @ref pNode.
    VRegHive* pHive;

    /// The components below @ref pNode still to walk.
    const VRegInternalWStringView* pRest;
    SIZE_T cRest;

    /// Whether the call goes to the base registry, because the key is
    /// outside every mount.
    BOOL isBase;

    LSTATUS status;

    /// `VRegInternalWStringView` into the caller's subkey.
    VRegInternalArray components;

    /// `VRegInternalWStringView` into @ref nativeText and @ref components: the
    /// native path of a real key plus the subkey.
    VRegInternalArray full;
    VRegInternalWString nativeText;
} VRegInternalTarget;

#define VREG_INTERNAL_MOUNT_AT(pArray, index) VREG_INTERNAL_AT(pArray, VRegInternalMountPoint, index)
#define VREG_INTERNAL_STRING_AT(pArray, index) VREG_INTERNAL_AT(pArray, VRegInternalWString, index)
#define VREG_INTERNAL_VIEW_AT(pArray, index) VREG_INTERNAL_AT(pArray, VRegInternalWStringView, index)

/// The view of the process: what a call without a view flag asks for.
#define VREG_INTERNAL_PROCESS_VIEW (sizeof(void*) == 8 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY)

#define VREG_INTERNAL_KEY_NAME_INFORMATION_CLASS ((ULONG)3)
#define VREG_INTERNAL_NT_STATUS_BUFFER_OVERFLOW ((LONG)0x80000005)
#define VREG_INTERNAL_NT_STATUS_BUFFER_TOO_SMALL ((LONG)0xC0000023)

#define VREG_INTERNAL_KNOWN_NOTIFY_FILTERS \
    (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY \
        | REG_NOTIFY_THREAD_AGNOSTIC)

/// @p psz, or an empty string for null: the hive layer takes no null names.
static LPCWSTR VRegInternal_OrEmpty(_In_opt_ const LPCWSTR psz)
{
    return psz ? psz : L"";
}

/// Copies a value into a caller buffer of @p cbBuffer bytes that is known to
/// hold it. Native adds a terminator after a string that lacks one, when it
/// fits, without counting it in the size.
static void VRegInternal_CopyValue(
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
    if (!VRegInternal_IsStringType(dwType))
        return;

    cbWhole = cbValue / sizeof(wchar_t) * sizeof(wchar_t);
    if (cbWhole == 0)
        return;

    isTerminated = pValue[cbWhole - 2] == 0 && pValue[cbWhole - 1] == 0;
    if (!isTerminated && cbWhole + sizeof(wchar_t) <= cbBuffer)
        ZeroMemory(pbBuffer + cbWhole, sizeof(wchar_t));
}

static BOOL VRegInternal_IsWow6432Node(const VRegInternalWStringView name)
{
    return VRegInternal_CompareNames(name, VRegInternal_ViewOf(L"WOW6432Node")) == 0;
}

/// The view a call with @p samDesired is of: the one it names, else the
/// process's.
static REGSAM VRegInternal_CallView(const REGSAM samDesired)
{
    if (samDesired & KEY_WOW64_64KEY)
        return KEY_WOW64_64KEY;
    if (samDesired & KEY_WOW64_32KEY)
        return KEY_WOW64_32KEY;

    return VREG_INTERNAL_PROCESS_VIEW;
}

/// Whether a mount of view @p samMount answers a call of view @p samCall.
static BOOL VRegInternal_Admits(const REGSAM samMount, const REGSAM samCall)
{
    return samMount == 0 || samMount == samCall;
}

/// The string form of the current user's SID; empty when it cannot be read.
static LSTATUS VRegInternal_CurrentUserSid(_Inout_ VRegInternalWString* pSid)
{
    HANDLE hToken = NULL;
    DWORD cbUser = 0;
    VRegInternalBuffer user = {0, 0, NULL};
    LSTATUS status = ERROR_SUCCESS;

    if (!VRegInternal_WStringResize(pSid, 0))
        return ERROR_OUTOFMEMORY;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return ERROR_SUCCESS;

    GetTokenInformation(hToken, TokenUser, NULL, 0, &cbUser);
    if (cbUser != 0)
    {
        if (!VRegInternal_BufferResize(&user, cbUser))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (GetTokenInformation(hToken, TokenUser, user.pb, cbUser, &cbUser))
        {
            LPWSTR pszSid = NULL;
            if (ConvertSidToStringSidW(((const TOKEN_USER*)user.pb)->User.Sid, &pszSid))
            {
                if (!VRegInternal_WStringAssign(pSid, pszSid, VRegInternal_ViewOf(pszSid).cch))
                    status = ERROR_OUTOFMEMORY;
                LocalFree(pszSid);
            }
        }
    }

    VRegInternal_BufferFree(&user);
    CloseHandle(hToken);
    return status;
}

/// Appends the native path of a predefined root to @p pPath as components;
/// nothing for a root with none.
static LSTATUS VRegInternal_NativeRootPath(
    const HKEY hRoot,
    _Inout_ VRegInternalArray* const pPath
)
{
    LPCWSTR pszSecond = NULL;
    VRegInternalWString sid = {0, 0, NULL};
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
        status = VRegInternal_CurrentUserSid(&sid);
        if (status == ERROR_SUCCESS && sid.cch != 0)
            pszSecond = L"USER";
    }

    if (status == ERROR_SUCCESS && pszSecond)
    {
        if (
            !VRegInternal_WStringArrayAppend(pPath, VRegInternal_ViewOf(L"REGISTRY"))
            || !VRegInternal_WStringArrayAppend(pPath, VRegInternal_ViewOf(pszSecond))
            || (sid.cch != 0 && !VRegInternal_WStringArrayAppend(pPath, VRegInternal_WStringViewOf(&sid)))
        )
            status = ERROR_OUTOFMEMORY;
    }

    VRegInternal_WStringFree(&sid);
    return status;
}

/// Whether @p pFull, with `WOW6432Node` components skipped, starts with
/// @p pMountPath; @p piRest then indexes the first component past the match
/// and @p pIsWow says a `WOW6432Node` was skipped.
static BOOL VRegInternal_MatchesNative(
    _In_ const VRegInternalArray* const pMountPath,
    _In_ const VRegInternalArray* const pFull,
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
        const VRegInternalWStringView component = VREG_INTERNAL_VIEW_AT(pFull, i);
        if (VRegInternal_IsWow6432Node(component))
        {
            *pIsWow = TRUE;
            continue;
        }
        if (VRegInternal_CompareNames(component, VRegInternal_WStringViewOf(&VREG_INTERNAL_STRING_AT(pMountPath, iMount))) != 0)
            return FALSE;

        ++iMount;
    }

    *piRest = i;
    return iMount == pMountPath->count;
}

// --- The mount table ---

/// The current table, or null while nothing is mounted. Lock-free.
static _Ret_maybenull_ const VRegInternalMountTable* VRegInternal_MountsOf(
    _In_ const VReg* const pThis
)
{
    return (const VRegInternalMountTable*)ReadPointerAcquire((PVOID volatile*)&pThis->implementation.virtualRegistry.pMounts);
}

/// A mount of @p pHive here; null when it is not mounted.
static _Ret_maybenull_ const VRegInternalMountPoint* VRegInternal_MountOf(
    _In_ const VReg* const pThis,
    _In_ const VRegHive* const pHive
)
{
    const VRegInternalMountTable* const pTable = VRegInternal_MountsOf(pThis);
    SIZE_T i;
    if (!pTable)
        return NULL;

    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VRegInternalMountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
        if (pMount->pHive == pHive)
            return pMount;
    }

    return NULL;
}

static void VRegInternal_MountPointFree(
    _Inout_ VRegInternalMountPoint* const pMount
)
{
    VRegInternal_WStringArrayFree(&pMount->path);
    VRegInternal_WStringArrayFree(&pMount->nativePath);
}

/// Whether the mount @p pMount is of @p hRoot at the path @p pPath, by the
/// registry's own name comparison.
static BOOL VRegInternal_MountIsAt(
    _In_ const VRegInternalMountPoint* const pMount,
    const HKEY hRoot,
    _In_ const VRegInternalArray* const pPath
)
{
    SIZE_T i;
    if (pMount->hRoot != hRoot || pMount->path.count != pPath->count)
        return FALSE;

    for (i = 0; i < pPath->count; ++i)
    {
        const VRegInternalWStringView left = VRegInternal_WStringViewOf(&VREG_INTERNAL_STRING_AT(&pMount->path, i));
        const VRegInternalWStringView right = VRegInternal_WStringViewOf(&VREG_INTERNAL_STRING_AT(pPath, i));
        if (VRegInternal_CompareNames(left, right) != 0)
            return FALSE;
    }

    return TRUE;
}

/// Publishes a table holding the current mounts plus @p pMount, whose strings
/// it then owns. Under the mount lock.
static LSTATUS VRegInternal_PublishMount(
    _In_ VReg* const pThis,
    _In_ const VRegInternalMountPoint* const pMount
)
{
    VRegInternalMountTable* const pOld = pThis->implementation.virtualRegistry.pMounts;
    VRegInternalMountTable* const pNew = (VRegInternalMountTable*)VRegInternal_AllocZeroed(sizeof(VRegInternalMountTable));
    SIZE_T i;
    if (!pNew)
        return ERROR_OUTOFMEMORY;

    for (i = 0; pOld && i < pOld->mounts.count; ++i)
    {
        const VRegInternalMountPoint* const pExisting = &VREG_INTERNAL_MOUNT_AT(&pOld->mounts, i);
        if (VRegInternal_MountIsAt(pExisting, pMount->hRoot, &pMount->path))
        {
            VRegInternal_ArrayFree(&pNew->mounts);
            VRegInternal_Free(pNew);
            return ERROR_ALREADY_EXISTS;
        }
        if (!VRegInternal_ArrayAppend(&pNew->mounts, sizeof(*pExisting), pExisting))
        {
            VRegInternal_ArrayFree(&pNew->mounts);
            VRegInternal_Free(pNew);
            return ERROR_OUTOFMEMORY;
        }
    }

    if (!VRegInternal_ArrayAppend(&pNew->mounts, sizeof(*pMount), pMount))
    {
        VRegInternal_ArrayFree(&pNew->mounts);
        VRegInternal_Free(pNew);
        return ERROR_OUTOFMEMORY;
    }

    pNew->pPrevious = pOld;
    WritePointerRelease((PVOID volatile*)&pThis->implementation.virtualRegistry.pMounts, pNew);
    return ERROR_SUCCESS;
}

// --- Resolution ---

static void VRegInternal_TargetFree(_Inout_ VRegInternalTarget* const pTarget)
{
    VRegInternal_ArrayFree(&pTarget->components);
    VRegInternal_ArrayFree(&pTarget->full);
    VRegInternal_WStringFree(&pTarget->nativeText);
}

/// Looks for a mount a real key plus the subkey components lands in, by
/// native path; fills @p pTarget and sets @p pIsResolved when it does.
static LSTATUS VRegInternal_ResolveNative(
    _In_ const VReg* const pThis,
    const HKEY hKey,
    const REGSAM samDesired,
    _Inout_ VRegInternalTarget* const pTarget,
    _Out_ BOOL* const pIsResolved
)
{
    const VRegInternalMountTable* const pTable = VRegInternal_MountsOf(pThis);
    BOOL isGated = FALSE;
    VRegInternalBuffer buffer = {0, 0, NULL};
    ULONG cbName = 0;
    const VRegInternalMountPoint* pBest = NULL;
    SIZE_T iBestRest = 0;
    REGSAM samCall;
    SIZE_T i;
    SIZE_T start;
    LSTATUS status = ERROR_SUCCESS;

    *pIsResolved = FALSE;
    for (i = 0; pTable && !isGated && i < pTable->mounts.count; ++i)
    {
        const VRegInternalMountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
        SIZE_T j;
        if (pMount->path.count == 0)
            continue;

        for (j = 0; !isGated && j < pTarget->components.count; ++j)
        {
            const VRegInternalWStringView last = VRegInternal_WStringViewOf(&VREG_INTERNAL_STRING_AT(&pMount->path, pMount->path.count - 1));
            isGated = VRegInternal_CompareNames(VREG_INTERNAL_VIEW_AT(&pTarget->components, j), last) == 0;
        }
    }
    if (!isGated || !pThis->implementation.virtualRegistry.pfnNtQueryKey || VReg_IsVirtualHKey(hKey))
        return ERROR_SUCCESS;

    // The name is a KEY_NAME_INFORMATION: a byte count, then the characters.
    if (!VRegInternal_BufferResize(&buffer, 1024))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        ULONG cbResult = 0;
        const LONG ntStatus = pThis->implementation.virtualRegistry.pfnNtQueryKey(
            hKey,
            VREG_INTERNAL_KEY_NAME_INFORMATION_CLASS,
            buffer.pb,
            (ULONG)buffer.cb,
            &cbResult
        );
        if (ntStatus >= 0)
            break;
        if (
            (ntStatus != VREG_INTERNAL_NT_STATUS_BUFFER_OVERFLOW && ntStatus != VREG_INTERNAL_NT_STATUS_BUFFER_TOO_SMALL)
            || cbResult <= buffer.cb
        )
            goto done;
        if (!VRegInternal_BufferResize(&buffer, cbResult))
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
        !VRegInternal_WStringAssign(
            &pTarget->nativeText,
            (LPCWCH)(buffer.pb + sizeof(ULONG)),
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
            VRegInternalWStringView component;
            component.pch = pTarget->nativeText.psz + start;
            component.cch = end - start;
            if (!VRegInternal_ArrayAppend(&pTarget->full, sizeof(component), &component))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
        }

        start = end + 1;
    }

    for (i = 0; i < pTarget->components.count; ++i)
    {
        const VRegInternalWStringView* const pComponent = &VREG_INTERNAL_VIEW_AT(&pTarget->components, i);
        if (!VRegInternal_ArrayAppend(&pTarget->full, sizeof(*pComponent), pComponent))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    samCall = VRegInternal_CallView(samDesired);
    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VRegInternalMountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
        BOOL isWow = FALSE;
        SIZE_T iRest = 0;
        if (
            pMount->nativePath.count == 0
            || !VRegInternal_MatchesNative(&pMount->nativePath, &pTarget->full, &isWow, &iRest)
            || !VRegInternal_Admits(pMount->samView, isWow ? KEY_WOW64_32KEY : samCall)
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
        pTarget->pNode = VRegInternal_HiveRoot(pBest->pHive);
        pTarget->pHive = pBest->pHive;
        pTarget->pRest = (const VRegInternalWStringView*)pTarget->full.p + iBestRest;
        pTarget->cRest = pTarget->full.count - iBestRest;
        *pIsResolved = TRUE;
    }

done:
    VRegInternal_BufferFree(&buffer);
    return status;
}

/// Resolves @p hKey and @p subKey (empty for the key itself). @p samDesired
/// supplies the registry view of the call. Answers a status only for a
/// failure of its own (memory); what the call should answer is in the target.
static LSTATUS VRegInternal_Resolve(
    _In_ const VReg* const pThis,
    const HKEY hKey,
    const VRegInternalWStringView subKey,
    const REGSAM samDesired,
    _Out_ VRegInternalTarget* const pTarget
)
{
    LSTATUS splitStatus;
    BOOL isSplit;
    BOOL isResolved = FALSE;
    VRegInternalNode* pNode;

    ZeroMemory(pTarget, sizeof(*pTarget));
    splitStatus = VRegInternal_SplitPath(subKey, &pTarget->components);
    if (splitStatus == ERROR_OUTOFMEMORY)
        return splitStatus;

    isSplit = splitStatus == ERROR_SUCCESS;
    pNode = VRegInternal_NodeOf(hKey);
    if (pNode)
    {
        const VRegInternalMountPoint* const pMount = VRegInternal_MountOf(pThis, VRegInternal_NodeHive(pNode));
        if (pMount)
        {
            if (isSplit)
            {
                pTarget->pNode = pNode;
                pTarget->pHive = pMount->pHive;
                pTarget->pRest = (const VRegInternalWStringView*)pTarget->components.p;
                pTarget->cRest = pTarget->components.count;
            }
            else
            {
                pTarget->status = splitStatus;
            }
            return ERROR_SUCCESS;
        }
    }
    else if (VRegInternal_HandleSpaceOwns(VRegInternal_ProcessHandleSpace(), hKey))
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (VRegInternal_IsPredefinedHKey(hKey))
    {
        const REGSAM samCall = VRegInternal_CallView(samDesired);
        const VRegInternalMountTable* const pTable = VRegInternal_MountsOf(pThis);
        const VRegInternalMountPoint* pBest = NULL;
        SIZE_T i;
        for (i = 0; pTable && isSplit && i < pTable->mounts.count; ++i)
        {
            const VRegInternalMountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
            SIZE_T j;
            BOOL isPrefix = TRUE;
            if (
                pMount->hRoot != hKey
                || pMount->path.count > pTarget->components.count
                || !VRegInternal_Admits(pMount->samView, samCall)
            )
                continue;

            for (j = 0; isPrefix && j < pMount->path.count; ++j)
            {
                const VRegInternalWStringView mountName = VRegInternal_WStringViewOf(&VREG_INTERNAL_STRING_AT(&pMount->path, j));
                isPrefix = VRegInternal_CompareNames(mountName, VREG_INTERNAL_VIEW_AT(&pTarget->components, j)) == 0;
            }
            if (!isPrefix)
                continue;

            if (!pBest || pMount->path.count > pBest->path.count)
                pBest = pMount;
        }

        if (pBest)
        {
            pTarget->pNode = VRegInternal_HiveRoot(pBest->pHive);
            pTarget->pHive = pBest->pHive;
            pTarget->pRest = (const VRegInternalWStringView*)pTarget->components.p + pBest->path.count;
            pTarget->cRest = pTarget->components.count - pBest->path.count;
            return ERROR_SUCCESS;
        }

        if (pThis->implementation.virtualRegistry.pBase)
            pTarget->isBase = TRUE;
        else
            pTarget->status = isSplit ? ERROR_FILE_NOT_FOUND : splitStatus;

        return ERROR_SUCCESS;
    }

    if (!pThis->implementation.virtualRegistry.pBase)
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (isSplit)
    {
        const LSTATUS status = VRegInternal_ResolveNative(pThis, hKey, samDesired, pTarget, &isResolved);
        if (status != ERROR_SUCCESS)
            return status;
    }

    if (!isResolved)
        pTarget->isBase = TRUE;

    return ERROR_SUCCESS;
}

// --- The slots ---

/// A call forwarded to a redirect: the flat function, on @p hKey of
/// @p pRegistry, with the arguments in @p pContext.
typedef LSTATUS (*VRegInternalRedirectedCall)(_In_ VReg* pRegistry, HKEY hKey, _In_opt_ void* pContext);

/// Answers a call whose open landed on a redirect: @p pfnCall on the
/// redirect's registry and key, or @ref ERROR_ACCESS_DENIED for a write when
/// it is null (only a frozen hive redirects, and it refuses every write).
/// Closes the key either way.
static LSTATUS VRegInternal_CallRedirect(
    _In_ const VRegInternalRedirect* const pRedirect,
    _In_opt_ const VRegInternalRedirectedCall pfnCall,
    _In_opt_ void* const pContext
)
{
    // A redirect always names the registry that answers its key.
    _Analysis_assume_(pRedirect->pRegistry);
    const LSTATUS status = pfnCall ? pfnCall(pRedirect->pRegistry, pRedirect->hKey, pContext) : ERROR_ACCESS_DENIED;
    VRegInternal_CloseRedirect(pRedirect);
    return status;
}

/// The arguments of a @ref RegGetValueW forwarded to a redirect, which
/// answers the value of the redirect's key itself.
typedef struct
{
    LPCWSTR lpValue;
    DWORD dwFlags;
    LPDWORD pdwType;
    PVOID pvData;
    LPDWORD pcbData;
} VRegInternalGetValueCall;

static LSTATUS VRegInternal_RedirectGetValue(
    _In_ VReg* const pRegistry,
    const HKEY hKey,
    _In_opt_ void* const pContext
)
{
    const VRegInternalGetValueCall* const pCall = (const VRegInternalGetValueCall*)pContext;
    _Analysis_assume_(pCall);
    return VReg_GetValueW(
        pRegistry,
        hKey,
        NULL,
        pCall->lpValue,
        pCall->dwFlags,
        pCall->pdwType,
        pCall->pvData,
        pCall->pcbData
    );
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegOpenKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    VRegInternalTarget target;
    VRegInternalOpenResult result;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_OpenKeyExW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }
    if (!samDesired)
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VRegInternal_HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, samDesired, &result);
    if (status == ERROR_SUCCESS)
        *phkResult = result.redirect.hKey ? result.redirect.hKey : VRegInternal_NodeHandle(result.pNode);

done:
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegCreateKeyExW(
    _In_ VReg* const pRegistry,
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
    VRegInternalTarget target;
    VRegInternalOpenResult result;
    DWORD dwDisposition = REG_OPENED_EXISTING_KEY;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_CreateKeyExW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    *phkResult = NULL;
    if (!target.pNode)
    {
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }
    if (dwOptions & ~(DWORD)0x1F)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    status = VRegInternal_HiveCreateKey(
        target.pHive,
        target.pNode,
        target.pRest,
        target.cRest,
        VRegInternal_OrEmpty(lpClass),
        dwOptions,
        samDesired,
        &result,
        &dwDisposition
    );
    if (status != ERROR_SUCCESS)
    {
        status = VRegInternal_FailureStatus(status);
        goto done;
    }

    *phkResult = result.redirect.hKey ? result.redirect.hKey : VRegInternal_NodeHandle(result.pNode);
    if (lpdwDisposition)
        *lpdwDisposition = dwDisposition;

done:
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegCloseKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (VRegInternal_NodeOf(hKey) || VRegInternal_IsPredefinedHKey(hKey))
        return ERROR_SUCCESS;
    if (VRegInternal_HandleSpaceOwns(VRegInternal_ProcessHandleSpace(), hKey) || !pRegistry->implementation.virtualRegistry.pBase)
        return ERROR_INVALID_HANDLE;

    return VReg_CloseKey(
        pRegistry->implementation.virtualRegistry.pBase,
        hKey
    );
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegQueryValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    VRegInternalTarget target;
    VRegInternalBuffer value = {0, 0, NULL};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    DWORD cbBuffer;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_QueryValueExW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VRegInternal_HiveQueryValue(
        target.pHive,
        target.pNode,
        VRegInternal_OrEmpty(lpValueName),
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

    VRegInternal_CopyValue(lpData, cbBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
        VRegInternal_BufferFree(&value);
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegSetValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    VRegInternalTarget target;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_SetValueExW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }
    if (!lpData && cbData)
    {
        status = ERROR_NOACCESS;
        goto done;
    }

    status = VRegInternal_HiveSetValue(
        target.pHive,
        target.pNode,
        VRegInternal_OrEmpty(lpValueName),
        dwType,
        lpData,
        lpData ? cbData : 0
    );

done:
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegDeleteValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    VRegInternalTarget target;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_DeleteValueW(
            pRegistry->implementation.virtualRegistry.pBase,
            hKey,
            lpValueName
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    status = VRegInternal_HiveDeleteValue(target.pHive, target.pNode, VRegInternal_OrEmpty(lpValueName));

done:
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegDeleteKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    VRegInternalTarget target;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_DeleteKeyExW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    status = VRegInternal_HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegDeleteTreeW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    VRegInternalTarget target;
    VRegInternalOpenResult result;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(lpSubKey), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_DeleteTreeW(
            pRegistry->implementation.virtualRegistry.pBase,
            hKey,
            lpSubKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    status = VRegInternal_HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_ALL_ACCESS, &result);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only a frozen hive redirects, and it refuses every write.
    if (result.redirect.hKey)
    {
        status = VRegInternal_CallRedirect(&result.redirect, NULL, NULL);
        goto done;
    }

    status = VRegInternal_HiveDeleteTree(target.pHive, result.pNode);
    if (status != ERROR_SUCCESS || target.cRest == 0)
        goto done;

    status = VRegInternal_HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegEnumKeyExW(
    _In_ VReg* const pRegistry,
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
    VRegInternalTarget target;
    VRegInternalWString name = {0, 0, NULL};
    VRegInternalWString className = {0, 0, NULL};
    FILETIME lastWrite;
    DWORD cchClassBuffer;
    BOOL isNameFit;
    BOOL isClassFit;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    ZeroMemory(&lastWrite, sizeof(lastWrite));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_EnumKeyExW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    cchClassBuffer = lpClass ? *lpcchClass : 0;
    if (lpcchClass)
        *lpcchClass = 0;

    status = VRegInternal_HiveEnumKey(target.pHive, target.pNode, dwIndex, &name, &className, &lastWrite);
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

    CopyMemory(lpName, VRegInternal_WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
    *lpcchName = (DWORD)name.cch;
    if (lpClass)
    {
        CopyMemory(lpClass, VRegInternal_WStringText(&className), (className.cch + 1) * sizeof(wchar_t));
        *lpcchClass = (DWORD)className.cch;
    }

done:
    if (target.pHive)
    {
        VRegInternal_WStringFree(&name);
        VRegInternal_WStringFree(&className);
    }
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegEnumValueW(
    _In_ VReg* const pRegistry,
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
    VRegInternalTarget target;
    VRegInternalWString name = {0, 0, NULL};
    VRegInternalBuffer value = {0, 0, NULL};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    BOOL isDataFit = TRUE;
    DWORD cbDataBuffer = 0;
    BOOL isNameFit;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_EnumValueW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VRegInternal_HiveEnumValue(
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
        CopyMemory(lpValueName, VRegInternal_WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
        *lpcchValueName = (DWORD)name.cch;
    }

    if (!isNameFit || !isDataFit)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    if (lpData)
        VRegInternal_CopyValue(lpData, cbDataBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
    {
        VRegInternal_WStringFree(&name);
        VRegInternal_BufferFree(&value);
    }
    VRegInternal_TargetFree(&target);
    return status;
}

// Native answers success without writing lpClass when *lpcchClass is zero,
// which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegQueryInfoKeyW(
    _In_ VReg* const pRegistry,
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
    VRegInternalTarget target;
    VRegInternalKeyInfo info;
    DWORD cchClass;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    ZeroMemory(&info, sizeof(info));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_QueryInfoKeyW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    status = VRegInternal_HiveQueryInfo(target.pHive, target.pNode, &info);
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

        CopyMemory(lpClass, VRegInternal_WStringText(&info.className), (cchClass + 1) * sizeof(wchar_t));
    }

    if (lpClass && lpcchClass && (*lpcchClass != 0 || lpcbMaxClassLen))
        *lpcchClass = cchClass;
    else if (!lpClass && lpcchClass)
        *lpcchClass = 0;

done:
    if (target.pHive)
        VRegInternal_WStringFree(&info.className);
    VRegInternal_TargetFree(&target);
    return status;
}

/// Resolves the indirect string `@<module>,-<id>` in @p pData as
/// `RegLoadMUIStringW` does natively: the module path with environment
/// strings expanded, joined to @p pszDirectory when one is given and taken
/// from the current directory otherwise, never from the search path; then
/// string `id` of the module's string table in the user's UI language. Data
/// that is no such string answers @ref ERROR_INVALID_DATA.
static LSTATUS VRegInternal_LoadIndirectString(
    _In_ const VRegInternalBuffer* const pData,
    const DWORD dwType,
    _In_opt_ const LPCWSTR pszDirectory,
    _Inout_ VRegInternalWString* const pText
)
{
    const LPCWCH pch = (LPCWCH)pData->pb;
    const SIZE_T cchData = pData->cb / sizeof(wchar_t);
    SIZE_T cch = 0;
    SIZE_T iComma = 1;
    SIZE_T i;
    DWORD dwId = 0;
    DWORD cchNeeded;
    DWORD cchWritten;
    VRegInternalWString module = {0, 0, NULL};
    VRegInternalWString expanded = {0, 0, NULL};
    VRegInternalWString path = {0, 0, NULL};
    HMODULE hModule = NULL;
    HRSRC hResource;
    HGLOBAL hBlock;
    const WORD* pBlock;
    SIZE_T cWords;
    SIZE_T iWord = 0;
    LSTATUS status = ERROR_SUCCESS;

    if (dwType != REG_SZ && dwType != REG_EXPAND_SZ)
        return ERROR_INVALID_DATA;

    // The text ends at its first terminator or with the data. The module runs
    // to the first comma, which a minus and at least one digit follow;
    // leading zeros and anything after the digits are accepted, as natively.
    while (cch < cchData && pch[cch] != L'\0')
        ++cch;
    while (iComma < cch && pch[iComma] != L',')
        ++iComma;
    if (
        cch == 0
        || pch[0] != L'@'
        || iComma + 2 >= cch
        || pch[iComma + 1] != L'-'
        || pch[iComma + 2] < L'0'
        || pch[iComma + 2] > L'9'
    )
        return ERROR_INVALID_DATA;

    // An id past the 16-bit range names no string; the digits stop growing it.
    for (i = iComma + 2; i < cch && pch[i] >= L'0' && pch[i] <= L'9'; ++i)
    {
        if (dwId <= 0xFFFF)
            dwId = dwId * 10 + (DWORD)(pch[i] - L'0');
    }

    if (!VRegInternal_WStringAssign(&module, pch + 1, iComma - 1))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    cchNeeded = ExpandEnvironmentStringsW(VRegInternal_WStringText(&module), NULL, 0);
    if (cchNeeded == 0)
    {
        status = (LSTATUS)GetLastError();
        goto done;
    }
    if (!VRegInternal_WStringResize(&expanded, cchNeeded - 1))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    cchWritten = ExpandEnvironmentStringsW(VRegInternal_WStringText(&module), expanded.psz, cchNeeded);
    if (cchWritten == 0 || cchWritten > cchNeeded)
    {
        status = cchWritten == 0 ? (LSTATUS)GetLastError() : ERROR_INSUFFICIENT_BUFFER;
        goto done;
    }
    VRegInternal_WStringTruncate(&expanded, cchWritten - 1);

    if (pszDirectory)
    {
        const VRegInternalWStringView directory = VRegInternal_ViewOf(pszDirectory);
        if (
            !VRegInternal_WStringAssign(&path, directory.pch, directory.cch)
            || !VRegInternal_WStringAppendChar(&path, L'\\')
            || !VRegInternal_WStringAppend(&path, VRegInternal_WStringText(&expanded), expanded.cch)
        )
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }
    else
    {
        // An empty module path has no full path; natively it is not found.
        cchNeeded = GetFullPathNameW(VRegInternal_WStringText(&expanded), 0, NULL, NULL);
        if (cchNeeded == 0)
        {
            status = ERROR_FILE_NOT_FOUND;
            goto done;
        }
        if (!VRegInternal_WStringResize(&path, cchNeeded - 1))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
        cchWritten = GetFullPathNameW(VRegInternal_WStringText(&expanded), cchNeeded, path.psz, NULL);
        if (cchWritten == 0 || cchWritten >= cchNeeded)
        {
            status = cchWritten == 0 ? (LSTATUS)GetLastError() : ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }
        VRegInternal_WStringTruncate(&path, cchWritten);
    }

    // The one module load in a hook body: natively RegLoadMUIStringW loads
    // the module on the caller's thread itself, so the load is the call's own
    // documented effect, not one VReg adds.
    hModule = LoadLibraryExW(
        VRegInternal_WStringText(&path),
        NULL,
        LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE
    );
    if (!hModule)
    {
        status = (LSTATUS)GetLastError();
        goto done;
    }
    if (dwId > 0xFFFF)
    {
        status = ERROR_RESOURCE_NAME_NOT_FOUND;
        goto done;
    }

    // A string table block holds 16 strings, each a length and that many
    // characters. The neutral language lets the resource loader pick the
    // user's UI language, as LoadStringW would without linking user32.
    // RT_STRING is spelled out, since its macro follows UNICODE.
    hResource = FindResourceExW(
        hModule,
        MAKEINTRESOURCEW(6),
        MAKEINTRESOURCEW(dwId / 16 + 1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)
    );
    if (!hResource)
    {
        status = (LSTATUS)GetLastError();
        goto done;
    }
    hBlock = LoadResource(hModule, hResource);
    pBlock = hBlock ? (const WORD*)LockResource(hBlock) : NULL;
    if (!pBlock)
    {
        status = ERROR_RESOURCE_NAME_NOT_FOUND;
        goto done;
    }

    cWords = SizeofResource(hModule, hResource) / sizeof(WORD);
    for (i = 0; i < dwId % 16 && iWord < cWords; ++i)
        iWord += 1 + (SIZE_T)pBlock[iWord];
    if (iWord >= cWords || pBlock[iWord] == 0 || pBlock[iWord] > cWords - iWord - 1)
    {
        status = ERROR_RESOURCE_NAME_NOT_FOUND;
        goto done;
    }
    if (!VRegInternal_WStringAssign(pText, (LPCWCH)(pBlock + iWord + 1), pBlock[iWord]))
        status = ERROR_OUTOFMEMORY;

done:
    if (hModule)
        FreeLibrary(hModule);
    VRegInternal_WStringFree(&path);
    VRegInternal_WStringFree(&expanded);
    VRegInternal_WStringFree(&module);
    return status;
}

// Native leaves pszOutBuf untouched on failure, which the terminator its
// LPWSTR type implies does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegLoadMUIStringW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    VRegInternalTarget target;
    VRegInternalBuffer data = {0, 0, NULL};
    VRegInternalWString text = {0, 0, NULL};
    DWORD dwType;
    DWORD cbData;
    DWORD cbText;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_LoadMUIStringW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
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

    // The merged read answers an owned copy, so no lock of the hive is held
    // while the string resolves.
    status = VRegInternal_HiveQueryValue(target.pHive, target.pNode, VRegInternal_OrEmpty(pszValue), &dwType, &cbData, &data);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternal_LoadIndirectString(&data, dwType, pszDirectory, &text);
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
        CopyMemory(pszOutBuf, VRegInternal_WStringText(&text), cch * sizeof(wchar_t));
        pszOutBuf[cch] = L'\0';
        goto done;
    }

    *pcbData = cbText;
    if (!pszOutBuf || cbOutBuf < cbText)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    CopyMemory(pszOutBuf, VRegInternal_WStringText(&text), cbText);

done:
    VRegInternal_BufferFree(&data);
    VRegInternal_WStringFree(&text);
    VRegInternal_TargetFree(&target);
    return status;
}

/// Answers the failure @p status after clearing the caller's buffer when the
/// call asked for that on failure.
_Post_satisfies_(return != ERROR_SUCCESS)
static LSTATUS VRegInternal_FailGetValue(
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
static BOOL VRegInternal_IsNulAt(
    _In_ const VRegInternalBuffer* const pBuffer,
    const SIZE_T iwch
)
{
    wchar_t ch;
    CopyMemory(&ch, pBuffer->pb + iwch * sizeof(wchar_t), sizeof(ch));
    return ch == L'\0';
}

// The SDK's _Post_z_ holds on every return, truncated reads and failures
// included; the analyzer cannot see the terminators written at run time.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegGetValueW(
    _In_ VReg* const pRegistry,
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

    VRegInternalTarget target;
    VRegInternalOpenResult result;
    VRegInternalNode* pNode;
    DWORD cbBuffer;
    DWORD dwType = 0;
    DWORD cbQueried = 0;
    BOOL isString;

    // Native copies the stored bytes first, then the string with the
    // terminators it lacks, then the expansion, each over the previous one and
    // only if it fits.
    VRegInternalBuffer stages[3] = {{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}};
    DWORD cbReported;
    DWORD dwTypeFlag;
    BOOL isAllowed;
    const VRegInternalBuffer* pFinal;
    BOOL isFit;
    BOOL isSizeMismatch;
    SIZE_T i;
    LSTATUS status = VRegInternal_Resolve(
        pRegistry,
        hkey,
        VRegInternal_ViewOf(lpSubKey),
        (dwFlags & VREG_INTERNAL_RRF_WOW6464_SUBKEY ? KEY_WOW64_64KEY : 0)
        | (dwFlags & VREG_INTERNAL_RRF_WOW6432_SUBKEY ? KEY_WOW64_32KEY : 0),
        &target
    );
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_GetValueW(
            pRegistry->implementation.virtualRegistry.pBase,
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
        || (dwFlags & VREG_INTERNAL_RRF_WOW_SUBKEYS) == VREG_INTERNAL_RRF_WOW_SUBKEYS
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    cbBuffer = pcbData ? *pcbData : 0;
    if (!target.pNode)
    {
        status = VRegInternal_FailGetValue(pvData, cbBuffer, dwFlags, target.status);
        goto done;
    }

    pNode = target.pNode;
    if (target.cRest != 0)
    {
        status = VRegInternal_HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_QUERY_VALUE, &result);
        if (status != ERROR_SUCCESS)
        {
            status = VRegInternal_FailGetValue(pvData, cbBuffer, dwFlags, status);
            goto done;
        }

        if (result.redirect.hKey)
        {
            VRegInternalGetValueCall call;
            call.lpValue = lpValue;
            call.dwFlags = dwFlags;
            call.pdwType = pdwType;
            call.pvData = pvData;
            call.pcbData = pcbData;
            status = VRegInternal_CallRedirect(&result.redirect, &VRegInternal_RedirectGetValue, &call);
            goto done;
        }

        pNode = result.pNode;
    }

    if (pdwType)
        *pdwType = 0;
    if (pcbData && !pvData)
        *pcbData = 0;

    status = VRegInternal_HiveQueryValue(target.pHive, pNode, VRegInternal_OrEmpty(lpValue), &dwType, &cbQueried, &stages[0]);
    if (status != ERROR_SUCCESS)
    {
        status = VRegInternal_FailGetValue(pvData, cbBuffer, dwFlags, status);
        goto done;
    }

    isString = VRegInternal_IsStringType(dwType);
    cbReported = (DWORD)stages[0].cb;
    if (isString)
    {
        const SIZE_T cwch = stages[0].cb / sizeof(wchar_t);
        SIZE_T cwchAdded = 0;
        static const wchar_t ZEROS[2] = {L'\0', L'\0'};
        if (cwch == 0 || !VRegInternal_IsNulAt(&stages[0], cwch - 1))
            cwchAdded = dwType == REG_MULTI_SZ ? 2 : 1;
        else if (dwType == REG_MULTI_SZ && (cwch == 1 || !VRegInternal_IsNulAt(&stages[0], cwch - 2)))
            cwchAdded = 1;

        if (
            !VRegInternal_BufferAssign(&stages[1], stages[0].pb, cwch * sizeof(wchar_t))
            || !VRegInternal_BufferAppend(&stages[1], ZEROS, cwchAdded * sizeof(wchar_t))
        )
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cbReported = (DWORD)stages[1].cb;
        if (dwType == REG_EXPAND_SZ && !(dwFlags & RRF_NOEXPAND))
        {
            const LPCWSTR pszRaw = (LPCWSTR)stages[1].pb;
            DWORD cwchExpanded;

            // The text was terminated above, so the stage holds at least that.
            _Analysis_assume_(pszRaw != NULL);
            cwchExpanded = ExpandEnvironmentStringsW(pszRaw, NULL, 0);
            if (!VRegInternal_BufferResize(&stages[2], (SIZE_T)cwchExpanded * sizeof(wchar_t)))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
            if (cwchExpanded != 0)
                ExpandEnvironmentStringsW(pszRaw, (wchar_t*)stages[2].pb, cwchExpanded);

            cbReported = VRegInternal_MaxDword(cbReported, (DWORD)stages[2].cb);
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
            *pcbData = isFit ? cbReported : VRegInternal_MaxDword(cbReported, cbReserved);
    }

    // Binary data passes as a DWORD or QWORD when it is no longer than one;
    // shorter data passes too, as a short REG_DWORD does. Only the exact
    // pairs check the size: with any further type admitted, native checks
    // nothing.
    isSizeMismatch =
        dwType == REG_BINARY
        && (
            ((dwFlags & RRF_RT_ANY) == RRF_RT_DWORD && stages[0].cb > sizeof(DWORD))
            || ((dwFlags & RRF_RT_ANY) == RRF_RT_QWORD && stages[0].cb > sizeof(ULONGLONG))
        );

    if (!isAllowed)
        status = ERROR_UNSUPPORTED_TYPE;
    else if (isSizeMismatch)
        status = ERROR_DATATYPE_MISMATCH;
    else if (pvData && !isFit)
        status = ERROR_MORE_DATA;
    if (status != ERROR_SUCCESS)
        status = VRegInternal_FailGetValue(pvData, cbBuffer, dwFlags, status);

done:
    for (i = 0; i < 3; ++i)
        VRegInternal_BufferFree(&stages[i]);
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegNotifyChangeKeyValue(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    VRegInternalTarget target;
    HANDLE hOwn;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_NotifyChangeKeyValue(
            pRegistry->implementation.virtualRegistry.pBase,
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
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }
    if (
        (dwNotifyFilter & ~(DWORD)VREG_INTERNAL_KNOWN_NOTIFY_FILTERS)
        || (fAsynchronous && !hEvent)
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    if (fAsynchronous)
    {
        status = VRegInternal_HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hEvent);
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

    status = VRegInternal_HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hOwn);
    if (status == ERROR_SUCCESS)
        WaitForSingleObject(hOwn, INFINITE);

    CloseHandle(hOwn);

done:
    VRegInternal_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegInternal_VirtualRegFlushKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    VRegInternalTarget target;
    LSTATUS status = VRegInternal_Resolve(pRegistry, hKey, VRegInternal_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_FlushKey(
            pRegistry->implementation.virtualRegistry.pBase,
            hKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VRegInternal_FailureStatus(target.status);
        goto done;
    }

    status = VRegInternal_HiveCheckKey(target.pHive, target.pNode);

done:
    VRegInternal_TargetFree(&target);
    return status;
}

// --- Creation and destruction ---

VREG_DECL void WINAPI VRegInternal_VirtualDestroy(_In_ VReg* const pRegistry)
{
    VRegInternalMountTable* pTable = pRegistry->implementation.virtualRegistry.pMounts;
    BOOL isNewest = TRUE;
    while (pTable)
    {
        VRegInternalMountTable* const pPrevious = pTable->pPrevious;
        if (isNewest)
        {
            SIZE_T i;
            for (i = 0; i < pTable->mounts.count; ++i)
                VRegInternal_MountPointFree(&VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i));
        }

        VRegInternal_ArrayFree(&pTable->mounts);
        VRegInternal_Free(pTable);
        pTable = pPrevious;
        isNewest = FALSE;
    }

    VRegInternal_Free(pRegistry);
}

VREG_DECL LSTATUS WINAPI VReg_CreateVirtual(
    _In_opt_ VReg* const pBase,
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VRegInternal_AllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->dwKind = VREG_INTERNAL_KIND_VIRTUAL;
    pRegistry->implementation.virtualRegistry.pBase = pBase;
    InitializeSRWLock(&pRegistry->implementation.virtualRegistry.mountLock);
    *(void**)&pRegistry->implementation.virtualRegistry.pfnNtQueryKey =
        VRegInternal_FindExport(GetModuleHandleW(L"ntdll.dll"), "NtQueryKey");

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VReg_Mount(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_ const LPCWSTR pszPath,
    _In_ VRegHive* const pHive,
    _In_ const REGSAM samView
)
{
    VRegInternalArray components = {NULL, 0, 0};
    VRegInternalMountPoint mount;
    SIZE_T i;
    LSTATUS status;

    if (pRegistry->dwKind != VREG_INTERNAL_KIND_VIRTUAL || !VRegInternal_IsPredefinedHKey(hRoot))
        return ERROR_INVALID_HANDLE;
    if (samView != 0 && samView != KEY_WOW64_64KEY && samView != KEY_WOW64_32KEY)
        return ERROR_INVALID_PARAMETER;

    ZeroMemory(&mount, sizeof(mount));
    mount.hRoot = hRoot;
    mount.pHive = pHive;
    mount.samView = samView;

    status = VRegInternal_SplitPath(VRegInternal_ViewOf(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VRegInternal_ArrayFree(&components);
        return status == ERROR_OUTOFMEMORY ? status : ERROR_BAD_PATHNAME;
    }

    for (i = 0; status == ERROR_SUCCESS && i < components.count; ++i)
    {
        if (!VRegInternal_WStringArrayAppend(&mount.path, VREG_INTERNAL_VIEW_AT(&components, i)))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VRegInternal_NativeRootPath(hRoot, &mount.nativePath);
    for (i = 0; status == ERROR_SUCCESS && mount.nativePath.count != 0 && i < components.count; ++i)
    {
        const VRegInternalWStringView component = VREG_INTERNAL_VIEW_AT(&components, i);
        if (!VRegInternal_IsWow6432Node(component) && !VRegInternal_WStringArrayAppend(&mount.nativePath, component))
            status = ERROR_OUTOFMEMORY;
    }
    VRegInternal_ArrayFree(&components);

    if (status == ERROR_SUCCESS)
    {
        AcquireSRWLockExclusive(&pRegistry->implementation.virtualRegistry.mountLock);
        status = VRegInternal_PublishMount(pRegistry, &mount);
        ReleaseSRWLockExclusive(&pRegistry->implementation.virtualRegistry.mountLock);
    }

    if (status != ERROR_SUCCESS)
        VRegInternal_MountPointFree(&mount);
    return status;
}

// ==========================================================================
// Applying scripts
// ==========================================================================

static LSTATUS VRegInternal_NotFoundIsFine(const LSTATUS status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND ? ERROR_SUCCESS : status;
}

static void VRegInternal_ApplyCloseKey(
    _In_ VReg* const pRegistry,
    _In_opt_ const HKEY hKey
)
{
    if (hKey)
        VReg_CloseKey(
            pRegistry,
            hKey
        );
}

static LSTATUS VRegInternal_PerformSetValue(
    _In_ const VRegOperation* const pOperation,
    _In_ VReg* const pRegistry
)
{
    const BYTE* const pData = pOperation->pData;
    const DWORD cbData = pOperation->cbData;
    const BOOL isString = VRegInternal_IsStringType(pOperation->dwType);
    const BOOL isTerminated = cbData >= sizeof(wchar_t) && pData[cbData - 1] == 0 && pData[cbData - 2] == 0;
    HKEY hKey = NULL;
    LSTATUS status = VReg_OpenKeyExW(
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
        VRegInternalBuffer padded = {0, 0, NULL};
        if (
            !VRegInternal_BufferAssign(&padded, pData, cbData)
            || !VRegInternal_BufferAppend(&padded, PADDING, sizeof(PADDING))
        )
        {
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            status = VReg_SetValueExW(
                pRegistry,
                hKey,
                pOperation->pszName,
                0,
                pOperation->dwType,
                padded.pb,
                cbData
            );
        }

        VRegInternal_BufferFree(&padded);
    }
    else
    {
        status = VReg_SetValueExW(
            pRegistry,
            hKey,
            pOperation->pszName,
            0,
            pOperation->dwType,
            pData,
            cbData
        );
    }

    VRegInternal_ApplyCloseKey(pRegistry, hKey);
    return status;
}

static LSTATUS VRegInternal_Perform(
    _In_ const VRegOperation* const pOperation,
    _In_ VReg* const pRegistry
)
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
        status = VReg_CreateKeyExW(
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
        VRegInternal_ApplyCloseKey(pRegistry, hKey);
        return status;

    case VREG_OPERATION_DELETE_KEY:
        status = VRegInternal_NotFoundIsFine(
            VReg_DeleteTreeW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath
            )
        );
        if (status != ERROR_SUCCESS)
            return status;

        return VRegInternal_NotFoundIsFine(
            VReg_DeleteKeyExW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath,
                0,
                0
            )
        );

    case VREG_OPERATION_SET_VALUE:
        return VRegInternal_PerformSetValue(pOperation, pRegistry);

    case VREG_OPERATION_DELETE_VALUE:
        status = VReg_OpenKeyExW(
            pRegistry,
            pOperation->hRoot,
            pOperation->pszPath,
            0,
            KEY_SET_VALUE,
            &hKey
        );
        if (status != ERROR_SUCCESS)
            return VRegInternal_NotFoundIsFine(status);

        status = VRegInternal_NotFoundIsFine(
            VReg_DeleteValueW(
                pRegistry,
                hKey,
                pOperation->pszName
            )
        );
        VRegInternal_ApplyCloseKey(pRegistry, hKey);
        return status;

    default:
        return ERROR_INVALID_PARAMETER;
    }
}

_Success_(return == ERROR_SUCCESS)
VREG_DECL LSTATUS WINAPI VReg_ApplyOperations(
    _In_reads_(cOperations) const VRegOperation* const pOperations,
    _In_ const SIZE_T cOperations,
    _In_ VReg* const pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* const pFailedIndex
)
{
    SIZE_T i;
    for (i = 0; i < cOperations; ++i)
    {
        const LSTATUS status = VRegInternal_Perform(&pOperations[i], pRegistry);
        if (status != ERROR_SUCCESS)
        {
            if (pFailedIndex)
                *pFailedIndex = i;
            return status;
        }
    }

    return ERROR_SUCCESS;
}

// ==========================================================================
// Snapshot and delta
// ==========================================================================

/// A value read from a key, held until the key's values are all known and can
/// be appended in name order.
typedef struct
{
    VRegInternalWString name;
    DWORD dwType;
    VRegInternalBuffer data;
    BOOL isTombstone;
} VRegInternalValueEntry;

/// Inserts @p pEntry into @p pValues, an array of @ref VRegInternalValueEntry sorted
/// by name; the array owns the entry from then on.
static BOOL VRegInternal_InsertValueSorted(
    _Inout_ VRegInternalArray* const pValues,
    _In_ const VRegInternalValueEntry* const pEntry
)
{
    const VRegInternalWStringView name = VRegInternal_WStringViewOf(&pEntry->name);
    SIZE_T lo = 0;
    SIZE_T hi = pValues->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const VRegInternalValueEntry* const pMid = &VREG_INTERNAL_AT(pValues, VRegInternalValueEntry, mid);
        if (VRegInternal_CompareNames(VRegInternal_WStringViewOf(&pMid->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return VRegInternal_ArrayInsert(pValues, sizeof(*pEntry), lo, pEntry);
}

static void VRegInternal_FreeValues(_Inout_ VRegInternalArray* const pValues)
{
    SIZE_T i;
    for (i = 0; i < pValues->count; ++i)
    {
        VRegInternalValueEntry* const pEntry = &VREG_INTERNAL_AT(pValues, VRegInternalValueEntry, i);
        VRegInternal_WStringFree(&pEntry->name);
        VRegInternal_BufferFree(&pEntry->data);
    }

    pValues->count = 0;
}

/// Appends the sorted values of a key as operations and empties the array.
static LSTATUS VRegInternal_FlushValues(
    _Inout_ VRegScript* const pScript,
    _In_ const HKEY hRoot,
    const VRegInternalWStringView path,
    _Inout_ VRegInternalArray* const pValues
)
{
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;
    for (i = 0; status == ERROR_SUCCESS && i < pValues->count; ++i)
    {
        const VRegInternalValueEntry* const pEntry = &VREG_INTERNAL_AT(pValues, VRegInternalValueEntry, i);
        status = VRegInternal_ScriptAppend(
            pScript,
            pEntry->isTombstone ? VREG_OPERATION_DELETE_VALUE : VREG_OPERATION_SET_VALUE,
            hRoot,
            path,
            VRegInternal_WStringViewOf(&pEntry->name),
            pEntry->dwType,
            pEntry->data.pb,
            (DWORD)pEntry->data.cb,
            0
        );
    }

    VRegInternal_FreeValues(pValues);
    return status;
}

/// Appends @p path to @p pPrefix with a backslash between, into @p pFull.
static BOOL VRegInternal_JoinKeyPath(
    _Inout_ VRegInternalWString* const pFull,
    const VRegInternalWStringView prefix,
    const VRegInternalWStringView path
)
{
    if (!VRegInternal_WStringAssign(pFull, prefix.pch, prefix.cch))
        return FALSE;
    if (path.cch == 0)
        return TRUE;

    return VRegInternal_AppendPathComponent(pFull, path.pch, path.cch);
}

// --- Snapshot ---

/// Appends the key @p hKey at @p pPath below the root, then its values, then
/// its subkeys in order.
static LSTATUS VRegInternal_WalkKey(
    _Inout_ VRegScript* const pScript,
    _In_ VReg* const pRegistry,
    _In_ const HKEY hRoot,
    _In_ const HKEY hKey,
    _In_ const VRegInternalWString* const pPath,
    const unsigned depth
)
{
    VRegInternalArray values = {NULL, 0, 0};
    VRegInternalArray subkeys = {NULL, 0, 0};
    VRegInternalWString name = {0, 0, NULL};
    VRegInternalWString childPath = {0, 0, NULL};
    DWORD dwIndex;
    SIZE_T i;
    LSTATUS status;

    // The registry nests at most 512 levels; anything deeper is a cycle
    // through some other registry.
    if (depth > 512)
        return ERROR_CANTREAD;

    status = VRegInternal_ScriptAppend(
        pScript,
        VREG_OPERATION_CREATE_KEY,
        hRoot,
        VRegInternal_WStringViewOf(pPath),
        VRegInternal_ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        VRegInternalValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        status = VRegInternal_EnumValueOwned(pRegistry, hKey, dwIndex, &entry.name, &entry.dwType, &entry.data);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            VRegInternal_WStringFree(&entry.name);
            VRegInternal_BufferFree(&entry.data);
            status = ERROR_SUCCESS;
            break;
        }
        if (status == ERROR_SUCCESS && !VRegInternal_InsertValueSorted(&values, &entry))
            status = ERROR_OUTOFMEMORY;
        if (status != ERROR_SUCCESS)
        {
            VRegInternal_WStringFree(&entry.name);
            VRegInternal_BufferFree(&entry.data);
            goto done;
        }
    }

    status = VRegInternal_FlushValues(pScript, hRoot, VRegInternal_WStringViewOf(pPath), &values);
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        status = VRegInternal_EnumKeyOwned(pRegistry, hKey, dwIndex, &name);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VRegInternal_WStringArrayInsertSorted(&subkeys, VRegInternal_WStringViewOf(&name)))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    for (i = 0; i < subkeys.count; ++i)
    {
        const VRegInternalWString* const pSubkey = &VREG_INTERNAL_AT(&subkeys, VRegInternalWString, i);
        HKEY hSubkey = NULL;

        // A symbolic link is exported as the key it is (its SymbolicLinkValue),
        // never followed: following one can leave the subtree or loop.
        status = VReg_OpenKeyExW(
            pRegistry,
            hKey,
            VRegInternal_WStringText(pSubkey),
            REG_OPTION_OPEN_LINK,
            KEY_READ,
            &hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VRegInternal_JoinKeyPath(&childPath, VRegInternal_WStringViewOf(pPath), VRegInternal_WStringViewOf(pSubkey)))
            status = ERROR_OUTOFMEMORY;
        else
            status = VRegInternal_WalkKey(pScript, pRegistry, hRoot, hSubkey, &childPath, depth + 1);

        VReg_CloseKey(
            pRegistry,
            hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;
    }

done:
    VRegInternal_FreeValues(&values);
    VRegInternal_ArrayFree(&values);
    VRegInternal_WStringArrayFree(&subkeys);
    VRegInternal_WStringFree(&name);
    VRegInternal_WStringFree(&childPath);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegScript_Snapshot(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_ const LPCWSTR pszPath,
    _Outptr_ VRegScript** const ppScript
)
{
    VRegScript* pScript = NULL;
    VRegInternalWString path = {0, 0, NULL};
    HKEY hKey = NULL;
    LSTATUS status;

    *ppScript = NULL;
    if (!VRegInternal_RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    status = VRegInternal_ScriptCreate(&pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg_OpenKeyExW(
        pRegistry,
        hRoot,
        pszPath ? pszPath : L"",
        0,
        KEY_READ,
        &hKey
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VRegInternal_WStringAssign(&path, pszPath, VRegInternal_ViewOf(pszPath).cch))
        status = ERROR_OUTOFMEMORY;
    else
        status = VRegInternal_WalkKey(pScript, pRegistry, hRoot, hKey, &path, 0);

    VReg_CloseKey(
        pRegistry,
        hKey
    );

done:
    VRegInternal_WStringFree(&path);
    if (status != ERROR_SUCCESS)
    {
        VRegScript_Destroy(pScript);
        return status;
    }

    *ppScript = pScript;
    return ERROR_SUCCESS;
}

// --- Delta ---

/// The state of the walk over a hive's upper layer: the key whose values are
/// being collected, and the values so far.
typedef struct
{
    VRegScript* pScript;
    HKEY hRoot;
    VRegInternalWStringView prefix;

    /// The full path of the key the pending values belong to.
    VRegInternalWString keyPath;
    VRegInternalArray values;
} VRegInternalDeltaWalk;

static LSTATUS VRegInternal_VisitUpper(
    _In_ void* const pContext,
    _In_ const VRegInternalUpperRecord* const pRecord
)
{
    VRegInternalDeltaWalk* const pWalk = (VRegInternalDeltaWalk*)pContext;
    LSTATUS status;

    if (pRecord->kind == VREG_INTERNAL_UPPER_VALUE)
    {
        VRegInternalValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        entry.dwType = pRecord->dwType;
        entry.isTombstone = pRecord->isTombstone;
        if (
            !VRegInternal_WStringAssign(&entry.name, pRecord->name.pch, pRecord->name.cch)
            || !VRegInternal_BufferAssign(&entry.data, pRecord->pData, pRecord->cbData)
            || !VRegInternal_InsertValueSorted(&pWalk->values, &entry)
        )
        {
            VRegInternal_WStringFree(&entry.name);
            VRegInternal_BufferFree(&entry.data);
            return ERROR_OUTOFMEMORY;
        }

        return ERROR_SUCCESS;
    }

    status = VRegInternal_FlushValues(pWalk->pScript, pWalk->hRoot, VRegInternal_WStringViewOf(&pWalk->keyPath), &pWalk->values);
    if (status != ERROR_SUCCESS)
        return status;
    if (!VRegInternal_JoinKeyPath(&pWalk->keyPath, pWalk->prefix, pRecord->path))
        return ERROR_OUTOFMEMORY;

    if (pRecord->isTombstone || pRecord->isOpaque)
    {
        status = VRegInternal_ScriptAppend(
            pWalk->pScript,
            VREG_OPERATION_DELETE_KEY,
            pWalk->hRoot,
            VRegInternal_WStringViewOf(&pWalk->keyPath),
            VRegInternal_ViewOf(L""),
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

    return VRegInternal_ScriptAppend(
        pWalk->pScript,
        VREG_OPERATION_CREATE_KEY,
        pWalk->hRoot,
        VRegInternal_WStringViewOf(&pWalk->keyPath),
        VRegInternal_ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
}

VREG_DECL LSTATUS WINAPI VRegScript_Delta(
    _In_ VRegHive* const pHive,
    _In_ const HKEY hRoot,
    _In_opt_ const LPCWSTR pszPath,
    _Outptr_ VRegScript** const ppScript
)
{
    VRegInternalDeltaWalk walk;
    LSTATUS status;

    *ppScript = NULL;
    if (!VRegInternal_RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    ZeroMemory(&walk, sizeof(walk));
    walk.hRoot = hRoot;
    walk.prefix = VRegInternal_ViewOf(pszPath);
    status = VRegInternal_ScriptCreate(&walk.pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VRegInternal_HiveSnapshotUpper(pHive, &VRegInternal_VisitUpper, &walk);
    if (status == ERROR_SUCCESS)
        status = VRegInternal_FlushValues(walk.pScript, hRoot, VRegInternal_WStringViewOf(&walk.keyPath), &walk.values);

    VRegInternal_FreeValues(&walk.values);
    VRegInternal_ArrayFree(&walk.values);
    VRegInternal_WStringFree(&walk.keyPath);
    if (status != ERROR_SUCCESS)
    {
        VRegScript_Destroy(walk.pScript);
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
typedef struct
{
    SIZE_T start;
    SIZE_T line;
    SIZE_T column;
} VRegInternalLineSegment;

typedef enum
{
    VREG_INTERNAL_KEY_NONE,
    VREG_INTERNAL_KEY_OPEN,
    VREG_INTERNAL_KEY_DELETED,
} VRegInternalKeyState;

typedef struct
{
    VRegScript* pScript;

    /// The logical line being parsed: a line of the file with its
    /// continuations joined, and the position in it.
    VRegInternalWString text;
    SIZE_T pos;

    /// `VRegInternalLineSegment`: offsets into @ref text map back to the physical
    /// lines the text came from.
    VRegInternalArray segments;

    /// The physical line the logical line starts on, 1-based.
    SIZE_T line;

    VRegInternalKeyState state;
    HKEY hRoot;
    VRegInternalWString path;

    /// Set by @ref VRegInternal_ParseFail: where in @ref text, and why.
    LPCSTR pszError;
    SIZE_T errorOffset;
} VRegInternalParser;

/// Records a syntax error at the parser's position and answers its status.
static LSTATUS VRegInternal_ParseFail(
    _Inout_ VRegInternalParser* const pParser,
    _In_ const LPCSTR pszMessage
)
{
    pParser->pszError = pszMessage;
    pParser->errorOffset = pParser->pos;
    return ERROR_INVALID_DATA;
}

static BOOL VRegInternal_IsBlank(const wchar_t ch)
{
    return ch == L' ' || ch == L'\t';
}

static int VRegInternal_HexDigit(const wchar_t ch)
{
    if (ch >= L'0' && ch <= L'9')
        return ch - L'0';
    if (ch >= L'a' && ch <= L'f')
        return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F')
        return ch - L'A' + 10;
    return -1;
}

static wchar_t VRegInternal_LowerAscii(const wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch - L'A' + L'a') : ch;
}

static BOOL VRegInternal_ViewEquals(
    const VRegInternalWStringView left,
    const VRegInternalWStringView right
)
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

static BOOL VRegInternal_EqualsIgnoreAsciiCase(
    const VRegInternalWStringView left,
    const VRegInternalWStringView right
)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (VRegInternal_LowerAscii(left.pch[i]) != VRegInternal_LowerAscii(right.pch[i]))
            return FALSE;
    }

    return TRUE;
}

static VRegInternalWStringView VRegInternal_TrimEnd(
    VRegInternalWStringView text
)
{
    while (text.cch != 0 && VRegInternal_IsBlank(text.pch[text.cch - 1]))
        --text.cch;

    return text;
}

static VRegInternalWStringView VRegInternal_Substring(
    const VRegInternalWStringView text,
    const SIZE_T start,
    const SIZE_T cch
)
{
    VRegInternalWStringView result;
    result.pch = text.pch + start;
    result.cch = cch;
    return result;
}

/// The index of @p ch in @p text from @p start on, or the text's length.
static SIZE_T VRegInternal_FindChar(
    const VRegInternalWStringView text,
    const SIZE_T start,
    const wchar_t ch
)
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
static void VRegInternal_ParserPosition(
    _In_ const VRegInternalParser* const pParser,
    const SIZE_T offset,
    _Out_ SIZE_T* const pLine,
    _Out_ SIZE_T* const pColumn
)
{
    const VRegInternalLineSegment* pSegment = &VREG_INTERNAL_AT(&pParser->segments, VRegInternalLineSegment, 0);
    SIZE_T i;
    for (i = 0; i < pParser->segments.count; ++i)
    {
        const VRegInternalLineSegment* const pCandidate = &VREG_INTERNAL_AT(&pParser->segments, VRegInternalLineSegment, i);
        if (pCandidate->start > offset)
            break;

        pSegment = pCandidate;
    }

    *pLine = pSegment->line;
    *pColumn = pSegment->column + (offset - pSegment->start);
}

static BOOL VRegInternal_AtEnd(_In_ const VRegInternalParser* const pParser)
{
    return pParser->pos >= pParser->text.cch;
}

static wchar_t VRegInternal_Current(
    _In_ const VRegInternalParser* const pParser
)
{
    return pParser->text.psz[pParser->pos];
}

static void VRegInternal_SkipBlanks(_Inout_ VRegInternalParser* const pParser)
{
    while (!VRegInternal_AtEnd(pParser) && VRegInternal_IsBlank(VRegInternal_Current(pParser)))
        ++pParser->pos;
}

static LSTATUS VRegInternal_ExpectEnd(_Inout_ VRegInternalParser* const pParser)
{
    VRegInternal_SkipBlanks(pParser);
    if (!VRegInternal_AtEnd(pParser))
        return VRegInternal_ParseFail(pParser, "unexpected text at the end of the line");

    return ERROR_SUCCESS;
}

/// Consumes @p pszToken, ignoring case, if the text continues with it.
static BOOL VRegInternal_Consume(
    _Inout_ VRegInternalParser* const pParser,
    _In_ const LPCWSTR pszToken
)
{
    const VRegInternalWStringView token = VRegInternal_ViewOf(pszToken);
    const VRegInternalWStringView text = VRegInternal_WStringViewOf(&pParser->text);
    if (text.cch - pParser->pos < token.cch)
        return FALSE;
    if (!VRegInternal_EqualsIgnoreAsciiCase(VRegInternal_Substring(text, pParser->pos, token.cch), token))
        return FALSE;

    pParser->pos += token.cch;
    return TRUE;
}

/// Reads the logical line starting at physical line @p index of @p pLines
/// (empty for blank and comment lines) and answers the index of its last
/// physical line in @p pLast.
static LSTATUS VRegInternal_ReadLogicalLine(
    _Inout_ VRegInternalParser* const pParser,
    _In_ const VRegInternalArray* const pLines,
    const SIZE_T index,
    _Out_ SIZE_T* const pLast
)
{
    VRegInternalWStringView physical = VREG_INTERNAL_AT(pLines, VRegInternalWStringView, index);
    VRegInternalLineSegment segment;
    SIZE_T current = index;

    pParser->segments.count = 0;
    if (!VRegInternal_WStringResize(&pParser->text, 0))
        return ERROR_OUTOFMEMORY;

    segment.start = 0;
    segment.line = index + 1;
    segment.column = 1;
    while (physical.cch != 0 && VRegInternal_IsBlank(physical.pch[0]))
    {
        ++physical.pch;
        --physical.cch;
        ++segment.column;
    }
    if (!VRegInternal_ArrayAppend(&pParser->segments, sizeof(segment), &segment))
        return ERROR_OUTOFMEMORY;

    *pLast = index;
    if (physical.cch == 0 || physical.pch[0] == L';')
        return ERROR_SUCCESS;

    if (!VRegInternal_WStringAssign(&pParser->text, physical.pch, physical.cch))
        return ERROR_OUTOFMEMORY;
    if (physical.pch[0] == L'[')
        return ERROR_SUCCESS;

    // A value that continues ends its line in a backslash; a closing quote
    // ends every complete one.
    while (current + 1 < pLines->count)
    {
        VRegInternalWStringView joined = VRegInternal_TrimEnd(VRegInternal_WStringViewOf(&pParser->text));
        VRegInternalWStringView next;
        if (joined.cch == 0 || joined.pch[joined.cch - 1] != L'\\')
            break;

        VRegInternal_WStringTruncate(&pParser->text, joined.cch - 1);

        ++current;
        next = VREG_INTERNAL_AT(pLines, VRegInternalWStringView, current);
        segment.start = pParser->text.cch;
        segment.line = current + 1;
        segment.column = 1;
        while (next.cch != 0 && VRegInternal_IsBlank(next.pch[0]))
        {
            ++next.pch;
            --next.cch;
            ++segment.column;
        }
        if (
            !VRegInternal_ArrayAppend(&pParser->segments, sizeof(segment), &segment)
            || !VRegInternal_WStringAppend(&pParser->text, next.pch, next.cch)
        )
            return ERROR_OUTOFMEMORY;
    }

    *pLast = current;
    return ERROR_SUCCESS;
}

static LSTATUS VRegInternal_ParseKey(_Inout_ VRegInternalParser* const pParser)
{
    const VRegInternalWStringView text = VRegInternal_WStringViewOf(&pParser->text);
    SIZE_T close = text.cch;
    BOOL isDelete;
    VRegInternalWStringView full;
    SIZE_T slash;
    VRegInternalWStringView path;
    HKEY hRoot = NULL;
    LSTATUS status;

    while (close > 0 && text.pch[close - 1] != L']')
        --close;
    if (close == 0)
    {
        pParser->pos = text.cch;
        return VRegInternal_ParseFail(pParser, "missing ] after the key name");
    }

    // `close` is one past the bracket: the position that ends the key.
    pParser->pos = close;
    status = VRegInternal_ExpectEnd(pParser);
    if (status != ERROR_SUCCESS)
        return status;

    --close;
    pParser->pos = 1;
    isDelete = text.pch[pParser->pos] == L'-';
    if (isDelete)
        ++pParser->pos;

    full = VRegInternal_Substring(text, pParser->pos, close - pParser->pos);
    slash = VRegInternal_FindChar(full, 0, L'\\');
    if (!VRegInternal_FindRoot(VRegInternal_Substring(full, 0, slash), &hRoot))
        return VRegInternal_ParseFail(pParser, "unknown registry root");

    path = slash < full.cch ? VRegInternal_Substring(full, slash + 1, full.cch - slash - 1) : VRegInternal_Substring(full, 0, 0);
    if (isDelete && path.cch == 0)
        return VRegInternal_ParseFail(pParser, "a root key cannot be deleted");

    status = VRegInternal_ScriptAppend(
        pParser->pScript,
        isDelete ? VREG_OPERATION_DELETE_KEY : VREG_OPERATION_CREATE_KEY,
        hRoot,
        path,
        VRegInternal_ViewOf(L""),
        0,
        NULL,
        0,
        pParser->line
    );
    if (status != ERROR_SUCCESS)
        return status;

    pParser->state = isDelete ? VREG_INTERNAL_KEY_DELETED : VREG_INTERNAL_KEY_OPEN;
    pParser->hRoot = hRoot;
    return VRegInternal_WStringAssign(&pParser->path, path.pch, path.cch) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

/// Reads a quoted string at the position (on its opening quote) into @p pText.
static LSTATUS VRegInternal_ParseQuoted(
    _Inout_ VRegInternalParser* const pParser,
    _Inout_ VRegInternalWString* const pText
)
{
    ++pParser->pos;
    if (!VRegInternal_WStringResize(pText, 0))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        wchar_t ch;
        if (VRegInternal_AtEnd(pParser))
            return VRegInternal_ParseFail(pParser, "unterminated string");

        ch = VRegInternal_Current(pParser);
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
            if (VRegInternal_AtEnd(pParser) || (VRegInternal_Current(pParser) != L'\\' && VRegInternal_Current(pParser) != L'"'))
            {
                --pParser->pos;
                return VRegInternal_ParseFail(pParser, "invalid escape, only \\\\ and \\\" are allowed");
            }

            ch = VRegInternal_Current(pParser);
        }

        if (!VRegInternal_WStringAppendChar(pText, ch))
            return ERROR_OUTOFMEMORY;

        ++pParser->pos;
    }
}

static LSTATUS VRegInternal_ParseHexNumber(
    _Inout_ VRegInternalParser* const pParser,
    const SIZE_T maxDigits,
    _Out_ DWORD* const pdwValue
)
{
    DWORD dwValue = 0;
    SIZE_T digits = 0;
    *pdwValue = 0;
    while (!VRegInternal_AtEnd(pParser) && VRegInternal_HexDigit(VRegInternal_Current(pParser)) >= 0)
    {
        if (++digits > maxDigits)
            return VRegInternal_ParseFail(pParser, "too many hex digits");

        dwValue = (dwValue << 4) | (DWORD)VRegInternal_HexDigit(VRegInternal_Current(pParser));
        ++pParser->pos;
    }
    if (digits == 0)
        return VRegInternal_ParseFail(pParser, "expected a hex number");

    *pdwValue = dwValue;
    return ERROR_SUCCESS;
}

static LSTATUS VRegInternal_ParseHexBytes(
    _Inout_ VRegInternalParser* const pParser,
    _Inout_ VRegInternalBuffer* const pData
)
{
    VRegInternal_SkipBlanks(pParser);
    if (VRegInternal_AtEnd(pParser))
        return ERROR_SUCCESS;

    for (;;)
    {
        DWORD dwByte;
        BYTE byte;
        LSTATUS status;
        if (VRegInternal_AtEnd(pParser) || VRegInternal_HexDigit(VRegInternal_Current(pParser)) < 0)
            return VRegInternal_ParseFail(pParser, "expected a hex byte");

        status = VRegInternal_ParseHexNumber(pParser, 2, &dwByte);
        if (status != ERROR_SUCCESS)
            return status;

        byte = (BYTE)dwByte;
        if (!VRegInternal_BufferAppend(pData, &byte, 1))
            return ERROR_OUTOFMEMORY;

        VRegInternal_SkipBlanks(pParser);
        if (VRegInternal_AtEnd(pParser))
            return ERROR_SUCCESS;
        if (VRegInternal_Current(pParser) != L',')
            return VRegInternal_ParseFail(pParser, "expected , between hex bytes");

        ++pParser->pos;
        VRegInternal_SkipBlanks(pParser);
    }
}

static LSTATUS VRegInternal_ParseValue(
    _Inout_ VRegInternalParser* const pParser
)
{
    VRegInternalWString name = {0, 0, NULL};
    VRegInternalWString text = {0, 0, NULL};
    VRegInternalBuffer data = {0, 0, NULL};
    VRegOperationKind kind = VREG_OPERATION_SET_VALUE;
    DWORD dwType = 0;
    LSTATUS status = ERROR_SUCCESS;

    if (VRegInternal_Current(pParser) == L'@')
        ++pParser->pos;
    else if (VRegInternal_Current(pParser) == L'"')
        status = VRegInternal_ParseQuoted(pParser, &name);
    else
        status = VRegInternal_ParseFail(pParser, "expected a value name or a [key]");
    if (status != ERROR_SUCCESS)
        goto done;

    if (pParser->state == VREG_INTERNAL_KEY_NONE)
    {
        pParser->pos = 0;
        status = VRegInternal_ParseFail(pParser, "a value outside any key");
        goto done;
    }

    // A value under a [-key] line is an error rather than silently dropped,
    // since it can only be a mistake.
    if (pParser->state == VREG_INTERNAL_KEY_DELETED)
    {
        pParser->pos = 0;
        status = VRegInternal_ParseFail(pParser, "a value after a deleted key");
        goto done;
    }

    VRegInternal_SkipBlanks(pParser);
    if (VRegInternal_AtEnd(pParser) || VRegInternal_Current(pParser) != L'=')
    {
        status = VRegInternal_ParseFail(pParser, "expected =");
        goto done;
    }

    ++pParser->pos;
    VRegInternal_SkipBlanks(pParser);

    if (!VRegInternal_AtEnd(pParser) && VRegInternal_Current(pParser) == L'-')
    {
        ++pParser->pos;
        status = VRegInternal_ExpectEnd(pParser);
        kind = VREG_OPERATION_DELETE_VALUE;
    }
    else if (!VRegInternal_AtEnd(pParser) && VRegInternal_Current(pParser) == L'"')
    {
        status = VRegInternal_ParseQuoted(pParser, &text);
        if (status == ERROR_SUCCESS)
            status = VRegInternal_ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_SZ;
            if (!VRegInternal_BufferAssign(&data, VRegInternal_WStringText(&text), (text.cch + 1) * sizeof(wchar_t)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VRegInternal_Consume(pParser, L"dword:"))
    {
        DWORD dwValue = 0;
        status = VRegInternal_ParseHexNumber(pParser, 8, &dwValue);
        if (status == ERROR_SUCCESS)
            status = VRegInternal_ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_DWORD;
            if (!VRegInternal_BufferAssign(&data, &dwValue, sizeof(dwValue)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VRegInternal_Consume(pParser, L"hex"))
    {
        dwType = REG_BINARY;
        if (VRegInternal_Consume(pParser, L"("))
        {
            status = VRegInternal_ParseHexNumber(pParser, 8, &dwType);
            if (status == ERROR_SUCCESS && !VRegInternal_Consume(pParser, L")"))
                status = VRegInternal_ParseFail(pParser, "expected ) after the value type");
        }
        if (status == ERROR_SUCCESS && !VRegInternal_Consume(pParser, L":"))
            status = VRegInternal_ParseFail(pParser, "expected : after hex");
        if (status == ERROR_SUCCESS)
            status = VRegInternal_ParseHexBytes(pParser, &data);
    }
    else
    {
        status = VRegInternal_ParseFail(pParser, "expected a string, dword:, hex: or -");
    }
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternal_ScriptAppend(
        pParser->pScript,
        kind,
        pParser->hRoot,
        VRegInternal_WStringViewOf(&pParser->path),
        VRegInternal_WStringViewOf(&name),
        dwType,
        data.pb,
        (DWORD)data.cb,
        pParser->line
    );

done:
    VRegInternal_WStringFree(&name);
    VRegInternal_WStringFree(&text);
    VRegInternal_BufferFree(&data);
    return status;
}

static LSTATUS VRegInternal_ParseLogicalLine(
    _Inout_ VRegInternalParser* const pParser
)
{
    // Key paths and value names reach the registry as C strings, where a NUL
    // would silently cut them short ("[-HKCU\<NUL>x]" would delete all of
    // HKCU); no valid .reg text holds one anywhere.
    pParser->pos = VRegInternal_FindChar(VRegInternal_WStringViewOf(&pParser->text), 0, L'\0');
    if (pParser->pos < pParser->text.cch)
        return VRegInternal_ParseFail(pParser, "a NUL character");

    pParser->pos = 0;
    return pParser->text.psz[0] == L'[' ? VRegInternal_ParseKey(pParser) : VRegInternal_ParseValue(pParser);
}

/// Parses all of @p text into the parser's script. A syntax error answers
/// @ref ERROR_INVALID_DATA with @p pError filled in.
static LSTATUS VRegInternal_ParseText(
    _Inout_ VRegInternalParser* const pParser,
    const VRegInternalWStringView text,
    _Inout_opt_ VRegParseError* const pError
)
{
    VRegInternalArray lines = {NULL, 0, 0};
    VRegInternalWStringView header;
    SIZE_T start;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    for (start = 0; start <= text.cch;)
    {
        const SIZE_T end = VRegInternal_FindChar(text, start, L'\n');
        VRegInternalWStringView line = VRegInternal_Substring(text, start, end - start);
        if (line.cch != 0 && line.pch[line.cch - 1] == L'\r')
            --line.cch;
        if (!VRegInternal_ArrayAppend(&lines, sizeof(line), &line))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        start = end + 1;
    }

    // The loop above appended the first line, however short the text.
    _Analysis_assume_(lines.p != NULL);
    header = VRegInternal_TrimEnd(VREG_INTERNAL_AT(&lines, VRegInternalWStringView, 0));
    if (
        !VRegInternal_ViewEquals(header, VRegInternal_ViewOf(L"Windows Registry Editor Version 5.00"))
        && !VRegInternal_ViewEquals(header, VRegInternal_ViewOf(L"REGEDIT4"))
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
        status = VRegInternal_ReadLogicalLine(pParser, &lines, i, &last);
        if (status == ERROR_SUCCESS && pParser->text.cch != 0)
            status = VRegInternal_ParseLogicalLine(pParser);
        if (status == ERROR_INVALID_DATA && pError)
            VRegInternal_ParserPosition(pParser, pParser->errorOffset, &pError->line, &pError->column);
        if (status == ERROR_INVALID_DATA && pError)
            pError->pszMessage = pParser->pszError;
        if (status != ERROR_SUCCESS)
            goto done;

        i = last;
    }

done:
    VRegInternal_ArrayFree(&lines);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegScript_Parse(
    _In_reads_bytes_(cbFile) const void* const pFile,
    _In_ const SIZE_T cbFile,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VRegInternalWString text = {0, 0, NULL};
    VRegInternalParser parser;
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));

    status = VRegInternal_DecodeFile(pFile, cbFile, &text, pError);
    if (status != ERROR_SUCCESS)
    {
        VRegInternal_WStringFree(&text);
        return status;
    }

    ZeroMemory(&parser, sizeof(parser));
    status = VRegInternal_ScriptCreate(&parser.pScript);
    if (status == ERROR_SUCCESS)
        status = VRegInternal_ParseText(&parser, VRegInternal_WStringViewOf(&text), pError);

    VRegInternal_WStringFree(&parser.text);
    VRegInternal_WStringFree(&parser.path);
    VRegInternal_ArrayFree(&parser.segments);
    VRegInternal_WStringFree(&text);
    if (status != ERROR_SUCCESS)
    {
        VRegScript_Destroy(parser.pScript);
        return status;
    }

    *ppScript = parser.pScript;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegScript_ParseFile(
    _In_ const LPCWSTR pszPath,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VRegInternalBuffer file = {0, 0, NULL};
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));
    status = VRegInternal_ReadWholeFile(pszPath, &file);
    if (status == ERROR_SUCCESS)
        status = VRegScript_Parse(file.pb, file.cb, ppScript, pError);

    VRegInternal_BufferFree(&file);
    return status;
}

// ==========================================================================
// Scripts
// ==========================================================================

/// Every root a `.reg` file can name, the one list import and export share.
static const VRegInternalRootKey VREG_INTERNAL_ROOT_KEYS[] = {
    {L"HKEY_CLASSES_ROOT", L"HKCR", HKEY_CLASSES_ROOT},
    {L"HKEY_CURRENT_USER", L"HKCU", HKEY_CURRENT_USER},
    {L"HKEY_LOCAL_MACHINE", L"HKLM", HKEY_LOCAL_MACHINE},
    {L"HKEY_USERS", L"HKU", HKEY_USERS},
    {L"HKEY_CURRENT_CONFIG", L"HKCC", HKEY_CURRENT_CONFIG},
};

VREG_DECL _Ret_maybenull_ LPCWSTR VRegInternal_RootName(
    _In_opt_ const HKEY hRoot
)
{
    SIZE_T i;
    for (i = 0; i < sizeof(VREG_INTERNAL_ROOT_KEYS) / sizeof(VREG_INTERNAL_ROOT_KEYS[0]); ++i)
    {
        if (VREG_INTERNAL_ROOT_KEYS[i].hKey == hRoot)
            return VREG_INTERNAL_ROOT_KEYS[i].pszLong;
    }

    return NULL;
}

VREG_DECL BOOL VRegInternal_FindRoot(
    const VRegInternalWStringView name,
    _Out_ PHKEY const phRoot
)
{
    SIZE_T i;
    *phRoot = NULL;
    for (i = 0; i < sizeof(VREG_INTERNAL_ROOT_KEYS) / sizeof(VREG_INTERNAL_ROOT_KEYS[0]); ++i)
    {
        const VRegInternalRootKey* const pRoot = &VREG_INTERNAL_ROOT_KEYS[i];
        if (
            VRegInternal_EqualsIgnoreAsciiCase(name, VRegInternal_ViewOf(pRoot->pszLong))
            || VRegInternal_EqualsIgnoreAsciiCase(name, VRegInternal_ViewOf(pRoot->pszShort))
        )
        {
            *phRoot = pRoot->hKey;
            return TRUE;
        }
    }

    return FALSE;
}

VREG_DECL LSTATUS VRegInternal_ScriptCreate(
    _Outptr_ VRegScript** const ppScript
)
{
    VRegScript* const pScript = (VRegScript*)VRegInternal_AllocZeroed(sizeof(VRegScript));
    *ppScript = NULL;
    if (!pScript)
        return ERROR_OUTOFMEMORY;

    *ppScript = pScript;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegInternal_ScriptAppend(
    _Inout_ VRegScript* const pScript,
    const VRegOperationKind kind,
    _In_ const HKEY hRoot,
    const VRegInternalWStringView path,
    const VRegInternalWStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    const SIZE_T line
)
{
    VRegInternalScriptEntry entry;
    VRegOperation operation;
    ZeroMemory(&entry, sizeof(entry));
    if (
        !VRegInternal_WStringAssign(&entry.path, path.pch, path.cch)
        || !VRegInternal_WStringAssign(&entry.name, name.pch, name.cch)
        || !VRegInternal_BufferAssign(&entry.data, pData, cbData)
        || !VRegInternal_ArrayReserve(&pScript->operations, sizeof(operation), pScript->operations.count + 1)
        || !VRegInternal_ArrayAppend(&pScript->entries, sizeof(entry), &entry)
    )
    {
        VRegInternal_WStringFree(&entry.path);
        VRegInternal_WStringFree(&entry.name);
        VRegInternal_BufferFree(&entry.data);
        return ERROR_OUTOFMEMORY;
    }

    // The entry's storage is on the heap, so the pointers stay valid when the
    // array of entries moves.
    operation.kind = kind;
    operation.hRoot = hRoot;
    operation.pszPath = VRegInternal_WStringText(&entry.path);
    operation.pszName = VRegInternal_WStringText(&entry.name);
    operation.dwType = dwType;
    operation.pData = entry.data.pb;
    operation.cbData = cbData;
    operation.line = line;
    VRegInternal_ArrayAppend(&pScript->operations, sizeof(operation), &operation);
    return ERROR_SUCCESS;
}

VREG_DECL void WINAPI VRegScript_Destroy(_In_opt_ VRegScript* const pScript)
{
    SIZE_T i;
    if (!pScript)
        return;

    for (i = 0; i < pScript->entries.count; ++i)
    {
        VRegInternalScriptEntry* const pEntry = &VREG_INTERNAL_AT(&pScript->entries, VRegInternalScriptEntry, i);
        VRegInternal_WStringFree(&pEntry->path);
        VRegInternal_WStringFree(&pEntry->name);
        VRegInternal_BufferFree(&pEntry->data);
    }

    VRegInternal_ArrayFree(&pScript->entries);
    VRegInternal_ArrayFree(&pScript->operations);
    VRegInternal_Free(pScript);
}

VREG_DECL _Ret_notnull_ const VRegOperation* WINAPI VRegScript_GetOperations(
    _In_ const VRegScript* const pScript,
    _Out_ SIZE_T* const pcOperations
)
{
    static const VRegOperation NONE = {VREG_OPERATION_CREATE_KEY, NULL, L"", L"", 0, NULL, 0, 0};
    *pcOperations = pScript->operations.count;
    return pScript->operations.count == 0 ? &NONE : (const VRegOperation*)pScript->operations.p;
}

VREG_DECL void WINAPI VReg_Free(_In_opt_ void* const p)
{
    VRegInternal_Free(p);
}

// ==========================================================================
// Script text
// ==========================================================================

static LSTATUS VRegInternal_DecodeFail(
    _Inout_opt_ VRegParseError* const pError,
    _In_ const LPCSTR pszMessage
)
{
    if (pError)
    {
        pError->line = 1;
        pError->column = 1;
        pError->pszMessage = pszMessage;
    }

    return ERROR_INVALID_DATA;
}

static BOOL VRegInternal_StartsWithAscii(
    _In_reads_bytes_(cb) const BYTE* const pb,
    const SIZE_T cb,
    _In_ const LPCSTR pszPrefix
)
{
    SIZE_T i;
    for (i = 0; pszPrefix[i] != '\0'; ++i)
    {
        if (i >= cb || pb[i] != (BYTE)pszPrefix[i])
            return FALSE;
    }

    return TRUE;
}

static LSTATUS VRegInternal_Widen(
    const UINT codePage,
    _In_reads_bytes_(cb) const BYTE* const pb,
    const SIZE_T cb,
    _Inout_ VRegInternalWString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const DWORD dwFlags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const LSTATUS status = VRegInternal_WideFromMultiByte(codePage, dwFlags, (LPCCH)pb, cb, pText);
    if (status == ERROR_NO_UNICODE_TRANSLATION)
        return VRegInternal_DecodeFail(pError, "the text is not valid in its encoding");

    return status;
}

VREG_DECL LSTATUS VRegInternal_DecodeFile(
    _In_reads_bytes_(cbFile) const void* const pFile,
    const SIZE_T cbFile,
    _Inout_ VRegInternalWString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const BYTE* const pb = (const BYTE*)pFile;
    if (cbFile > (SIZE_T)MAXINT32)
        return VRegInternal_DecodeFail(pError, "the file is too large");

    if (cbFile >= 2 && pb[0] == 0xFF && pb[1] == 0xFE)
    {
        const SIZE_T cbBody = cbFile - 2;
        if (cbBody % sizeof(wchar_t) != 0)
            return VRegInternal_DecodeFail(pError, "a UTF-16 file has an odd byte count");
        if (!VRegInternal_WStringAssign(pText, (LPCWCH)(pb + 2), cbBody / sizeof(wchar_t)))
            return ERROR_OUTOFMEMORY;

        return ERROR_SUCCESS;
    }
    if (cbFile >= 2 && pb[0] == 0xFE && pb[1] == 0xFF)
        return VRegInternal_DecodeFail(pError, "big-endian UTF-16 is not supported");
    if (cbFile >= 3 && pb[0] == 0xEF && pb[1] == 0xBB && pb[2] == 0xBF)
        return VRegInternal_Widen(CP_UTF8, pb + 3, cbFile - 3, pText, pError);

    return VRegInternal_Widen(
        VRegInternal_StartsWithAscii(pb, cbFile, "REGEDIT4") ? CP_ACP : CP_UTF8,
        pb,
        cbFile,
        pText,
        pError
    );
}

VREG_DECL LSTATUS VRegInternal_EncodeFile(
    _In_ const VRegInternalWString* const pText,
    const VRegFormat format,
    _Inout_ VRegInternalBuffer* const pFile
)
{
    UINT codePage;
    BOOL isUtf8;
    BOOL fUsedDefault = FALSE;
    LSTATUS status;

    if (!VRegInternal_BufferResize(pFile, 0))
        return ERROR_OUTOFMEMORY;

    if (format == VREG_FORMAT_UNICODE)
    {
        static const BYTE BOM[2] = {0xFF, 0xFE};
        if (
            !VRegInternal_BufferAppend(pFile, BOM, sizeof(BOM))
            || !VRegInternal_BufferAppend(pFile, VRegInternal_WStringText(pText), pText->cch * sizeof(wchar_t))
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
    status = VRegInternal_MultiByteFromWide(
        codePage,
        isUtf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        isUtf8 ? NULL : &fUsedDefault,
        VRegInternal_WStringText(pText),
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

VREG_DECL LSTATUS VRegInternal_ReadWholeFile(
    _In_ const LPCWSTR pszPath,
    _Inout_ VRegInternalBuffer* const pFile
)
{
    LARGE_INTEGER size;
    LSTATUS status = ERROR_SUCCESS;
    const HANDLE hFile = CreateFileW(
        pszPath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
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
    else if (!VRegInternal_BufferResize(pFile, (SIZE_T)size.QuadPart))
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

VREG_DECL LSTATUS VRegInternal_WriteWholeFile(
    _In_ const LPCWSTR pszPath,
    _In_reads_bytes_(cbFile) const void* const pFile,
    const SIZE_T cbFile
)
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
#define VREG_INTERNAL_HEX_WRAP_COLUMN ((SIZE_T)76)

static const wchar_t VREG_INTERNAL_NEWLINE[] = L"\r\n";

/// Appends the terminated literal @p psz.
static BOOL VRegInternal_OutAppend(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const LPCWSTR psz
)
{
    const VRegInternalWStringView text = VRegInternal_ViewOf(psz);
    return VRegInternal_WStringAppend(pOut, text.pch, text.cch);
}

static BOOL VRegInternal_HasLineBreak(const VRegInternalWStringView text)
{
    SIZE_T i;
    for (i = 0; i < text.cch; ++i)
    {
        if (text.pch[i] == L'\r' || text.pch[i] == L'\n')
            return TRUE;
    }

    return FALSE;
}

static BOOL VRegInternal_AppendQuoted(
    _Inout_ VRegInternalWString* const pOut,
    const VRegInternalWStringView text
)
{
    SIZE_T i;
    if (!VRegInternal_WStringAppendChar(pOut, L'"'))
        return FALSE;

    for (i = 0; i < text.cch; ++i)
    {
        const wchar_t ch = text.pch[i];
        if ((ch == L'\\' || ch == L'"') && !VRegInternal_WStringAppendChar(pOut, L'\\'))
            return FALSE;
        if (!VRegInternal_WStringAppendChar(pOut, ch))
            return FALSE;
    }

    return VRegInternal_WStringAppendChar(pOut, L'"');
}

/// The text a REG_SZ takes in the friendly form; FALSE when the form would not
/// reproduce the bytes.
static BOOL VRegInternal_FriendlyString(
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    _Out_ VRegInternalWStringView* pText
)
{
    LPCWCH pch;
    SIZE_T cch;
    SIZE_T i;

    pText->pch = NULL;
    pText->cch = 0;
    if (cbData < sizeof(wchar_t) || cbData % sizeof(wchar_t) != 0)
        return FALSE;

    // Two bytes or more of data have storage.
    _Analysis_assume_(pData != NULL);
    pch = (LPCWCH)pData;
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
static BOOL VRegInternal_AppendHexNumber(
    _Inout_ VRegInternalWString* const pOut,
    DWORD value,
    const SIZE_T cchMin
)
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
        if (!VRegInternal_WStringAppendChar(pOut, L'0'))
            return FALSE;
    }
    for (i = 0; i < cchDigits; ++i)
    {
        if (!VRegInternal_WStringAppendChar(pOut, wszDigits[cchDigits - 1 - i]))
            return FALSE;
    }

    return TRUE;
}

static BOOL VRegInternal_AppendHex(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const VRegOperation* const pOperation,
    SIZE_T column
)
{
    const SIZE_T cchBefore = pOut->cch;
    SIZE_T i;

    if (pOperation->dwType == REG_BINARY)
    {
        if (!VRegInternal_OutAppend(pOut, L"hex:"))
            return FALSE;
    }
    else
    {
        if (
            !VRegInternal_OutAppend(pOut, L"hex(")
            || !VRegInternal_AppendHexNumber(pOut, pOperation->dwType, 1)
            || !VRegInternal_OutAppend(pOut, L"):")
        )
            return FALSE;
    }

    column += pOut->cch - cchBefore;
    for (i = 0; i < pOperation->cbData; ++i)
    {
        if (!VRegInternal_AppendHexNumber(pOut, pOperation->pData[i], 2))
            return FALSE;

        column += 2;
        if (i + 1 == pOperation->cbData)
            break;
        if (!VRegInternal_WStringAppendChar(pOut, L','))
            return FALSE;
        if (++column > VREG_INTERNAL_HEX_WRAP_COLUMN)
        {
            if (!VRegInternal_OutAppend(pOut, L"\\\r\n  "))
                return FALSE;

            column = 2;
        }
    }

    return TRUE;
}

static BOOL VRegInternal_AppendValue(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const VRegOperation* const pOperation
)
{
    const SIZE_T lineStart = pOut->cch;
    const VRegInternalWStringView name = VRegInternal_ViewOf(pOperation->pszName);
    VRegInternalWStringView text;
    BOOL isAppended;

    if (name.cch == 0)
        isAppended = VRegInternal_WStringAppendChar(pOut, L'@');
    else
        isAppended = VRegInternal_AppendQuoted(pOut, name);
    if (!isAppended || !VRegInternal_WStringAppendChar(pOut, L'='))
        return FALSE;

    if (pOperation->kind == VREG_OPERATION_DELETE_VALUE)
    {
        isAppended = VRegInternal_WStringAppendChar(pOut, L'-');
    }
    else if (pOperation->dwType == REG_SZ && VRegInternal_FriendlyString(pOperation->pData, pOperation->cbData, &text))
    {
        isAppended = VRegInternal_AppendQuoted(pOut, text);
    }
    else if (pOperation->dwType == REG_DWORD && pOperation->cbData == sizeof(DWORD))
    {
        DWORD dwValue;
        CopyMemory(&dwValue, pOperation->pData, sizeof(dwValue));
        isAppended =
            VRegInternal_OutAppend(pOut, L"dword:")
            && VRegInternal_AppendHexNumber(pOut, dwValue, 8);
    }
    else
    {
        isAppended = VRegInternal_AppendHex(pOut, pOperation, pOut->cch - lineStart);
    }

    return isAppended && VRegInternal_OutAppend(pOut, VREG_INTERNAL_NEWLINE);
}

/// Appends `[key]` or `[-key]` for @p pOperation's key, preceded by a blank
/// line unless it opens the file.
static LSTATUS VRegInternal_AppendKeyLine(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const VRegOperation* const pOperation,
    const BOOL isDelete,
    const BOOL isFirst
)
{
    const LPCWSTR pszRoot = VRegInternal_RootName(pOperation->hRoot);
    const VRegInternalWStringView path = VRegInternal_ViewOf(pOperation->pszPath);
    if (!pszRoot)
        return ERROR_INVALID_HANDLE;

    if (
        (!isFirst && !VRegInternal_OutAppend(pOut, VREG_INTERNAL_NEWLINE))
        || !VRegInternal_OutAppend(pOut, isDelete ? L"[-" : L"[")
        || !VRegInternal_OutAppend(pOut, pszRoot)
        || (path.cch != 0 && !VRegInternal_WStringAppendChar(pOut, L'\\'))
        || !VRegInternal_WStringAppend(pOut, path.pch, path.cch)
        || !VRegInternal_OutAppend(pOut, L"]")
        || !VRegInternal_OutAppend(pOut, VREG_INTERNAL_NEWLINE)
    )
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

/// Whether @p pLeft and @p pRight name the same key.
static BOOL VRegInternal_SameKey(
    _In_ const VRegOperation* const pLeft,
    _In_ const VRegOperation* const pRight
)
{
    return pLeft->hRoot == pRight->hRoot
        && VRegInternal_CompareNames(VRegInternal_ViewOf(pLeft->pszPath), VRegInternal_ViewOf(pRight->pszPath)) == 0;
}

VREG_DECL LSTATUS WINAPI VRegScript_Write(
    _In_reads_(cOperations) const VRegOperation* const pOperations,
    _In_ const SIZE_T cOperations,
    _In_ const VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** const ppBytes,
    _Out_ SIZE_T* const pcbBytes
)
{
    VRegInternalWString out = {0, 0, NULL};
    VRegInternalBuffer file = {0, 0, NULL};
    const VRegOperation* pLastKey = NULL;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    *ppBytes = NULL;
    *pcbBytes = 0;
    if (
        !VRegInternal_OutAppend(
            &out,
            format == VREG_FORMAT_REGEDIT4 ? L"REGEDIT4" : L"Windows Registry Editor Version 5.00"
        )
        || !VRegInternal_OutAppend(&out, VREG_INTERNAL_NEWLINE)
        || !VRegInternal_OutAppend(&out, VREG_INTERNAL_NEWLINE)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    for (i = 0; i < cOperations; ++i)
    {
        const VRegOperation* const pOperation = &pOperations[i];
        const BOOL isKey = pOperation->kind == VREG_OPERATION_CREATE_KEY || pOperation->kind ==
            VREG_OPERATION_DELETE_KEY;
        if (VRegInternal_HasLineBreak(VRegInternal_ViewOf(pOperation->pszPath)) || VRegInternal_HasLineBreak(
            VRegInternal_ViewOf(pOperation->pszName)
        ))
        {
            status = ERROR_INVALID_NAME;
            goto done;
        }

        if (isKey || !pLastKey || !VRegInternal_SameKey(pLastKey, pOperation))
        {
            status = VRegInternal_AppendKeyLine(
                &out,
                pOperation,
                pOperation->kind == VREG_OPERATION_DELETE_KEY,
                i == 0
            );
            if (status != ERROR_SUCCESS)
                goto done;

            pLastKey = pOperation;
        }

        if (!isKey && !VRegInternal_AppendValue(&out, pOperation))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    if (cOperations != 0 && !VRegInternal_OutAppend(&out, VREG_INTERNAL_NEWLINE))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    status = VRegInternal_EncodeFile(&out, format, &file);
    if (status != ERROR_SUCCESS)
        goto done;

    // The header alone makes the file non-empty, so the encoding allocated.
    _Analysis_assume_(file.pb != NULL);
    *ppBytes = file.pb;
    *pcbBytes = file.cb;
    file.pb = NULL;

done:
    VRegInternal_WStringFree(&out);
    VRegInternal_BufferFree(&file);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegScript_WriteFile(
    _In_reads_(cOperations) const VRegOperation* const pOperations,
    _In_ const SIZE_T cOperations,
    _In_ const VRegFormat format,
    _In_ const LPCWSTR pszPath
)
{
    void* pBytes = NULL;
    SIZE_T cbBytes = 0;
    LSTATUS status = VRegScript_Write(pOperations, cOperations, format, &pBytes, &cbBytes);
    if (status != ERROR_SUCCESS)
        return status;

    status = VRegInternal_WriteWholeFile(pszPath, pBytes, cbBytes);
    VReg_Free(pBytes);
    return status;
}

#endif // VREG_IMPLEMENTATION_INCLUDED
#endif // VREG_IMPLEMENTATION

#ifdef VREG_WITH_DETOURS
#ifdef VREG_INTERNAL_HOOKS_IMPLEMENTATION
#ifndef VREG_INTERNAL_HOOKS_IMPLEMENTATION_INCLUDED
#define VREG_INTERNAL_HOOKS_IMPLEMENTATION_INCLUDED

// ==========================================================================
// Hooks
// ==========================================================================

#include <detours/detours.h>

// Detours takes hook bodies and trampolines as data pointers, and the thunks
// are bytes; the conversions below are what its API is made of.
#pragma warning(disable: 4054 4055 4152)

/// The registry every hook calls through.
static VReg* volatile g_pRegistry;

static VReg* VRegInternal_HookRegistry(void)
{
    return (VReg*)ReadPointerAcquire((PVOID volatile*)&g_pRegistry);
}

// --- The hooks the registry answers: the same call, made on the registry ---

static LSTATUS APIENTRY VRegInternal_HookRegOpenKeyExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VReg_OpenKeyExW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegCreateKeyExW(
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
    return VReg_CreateKeyExW(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegCloseKey(
    _In_ const HKEY hKey
)
{
    return VReg_CloseKey(
        VRegInternal_HookRegistry(),
        hKey
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegQueryValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VReg_QueryValueExW(
        VRegInternal_HookRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegSetValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VReg_SetValueExW(
        VRegInternal_HookRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VReg_DeleteValueW(
        VRegInternal_HookRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VReg_DeleteKeyExW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteTreeW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    return VReg_DeleteTreeW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegEnumKeyExW(
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
    return VReg_EnumKeyExW(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegEnumValueW(
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
    return VReg_EnumValueW(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegQueryInfoKeyW(
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
    return VReg_QueryInfoKeyW(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegLoadMUIStringW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    return VReg_LoadMUIStringW(
        VRegInternal_HookRegistry(),
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
static LSTATUS APIENTRY VRegInternal_HookRegGetValueW(
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
    return VReg_GetValueW(
        VRegInternal_HookRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegNotifyChangeKeyValue(
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    return VReg_NotifyChangeKeyValue(
        VRegInternal_HookRegistry(),
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegFlushKey(
    _In_ const HKEY hKey
)
{
    return VReg_FlushKey(
        VRegInternal_HookRegistry(),
        hKey
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegOpenKeyExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VReg_OpenKeyExA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegCreateKeyExA(
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
    return VReg_CreateKeyExA(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegQueryValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VReg_QueryValueExA(
        VRegInternal_HookRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegSetValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VReg_SetValueExA(
        VRegInternal_HookRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VReg_DeleteValueA(
        VRegInternal_HookRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VReg_DeleteKeyExA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteTreeA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    return VReg_DeleteTreeA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegEnumKeyExA(
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
    return VReg_EnumKeyExA(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegEnumValueA(
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
    return VReg_EnumValueA(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegQueryInfoKeyA(
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
    return VReg_QueryInfoKeyA(
        VRegInternal_HookRegistry(),
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

static LSTATUS APIENTRY VRegInternal_HookRegLoadMUIStringA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
)
{
    return VReg_LoadMUIStringA(
        VRegInternal_HookRegistry(),
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
static LSTATUS APIENTRY VRegInternal_HookRegGetValueA(
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
    return VReg_GetValueA(
        VRegInternal_HookRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegOpenKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_OpenKeyA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegOpenKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_OpenKeyW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegCreateKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_CreateKeyA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegCreateKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_CreateKeyW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegQueryValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VReg_QueryValueA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegQueryValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VReg_QueryValueW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegSetValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    return VReg_SetValueA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegSetValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    return VReg_SetValueW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    return VReg_DeleteKeyA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    return VReg_DeleteKeyW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegEnumKeyA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    return VReg_EnumKeyA(
        VRegInternal_HookRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegEnumKeyW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    return VReg_EnumKeyW(
        VRegInternal_HookRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegSetKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VReg_SetKeyValueA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegSetKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VReg_SetKeyValueW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VReg_DeleteKeyValueA(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegDeleteKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VReg_DeleteKeyValueW(
        VRegInternal_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegQueryMultipleValuesA(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VReg_QueryMultipleValuesA(
        VRegInternal_HookRegistry(),
        hKey,
        val_list,
        num_vals,
        lpValueBuf,
        ldwTotsize
    );
}

static LSTATUS APIENTRY VRegInternal_HookRegQueryMultipleValuesW(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VReg_QueryMultipleValuesW(
        VRegInternal_HookRegistry(),
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
#define VREG_INTERNAL_REFUSED_HOOKS(X) \
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

typedef enum
{
#define VREG_INDEX(name) VREG_INTERNAL_REFUSED_##name,
    VREG_INTERNAL_REFUSED_HOOKS(VREG_INDEX)
#undef VREG_INDEX

    VREG_INTERNAL_REFUSED_COUNT
} VRegInternalRefusedIndex;

static void* g_refusedTargets[VREG_INTERNAL_REFUSED_COUNT];

// The forms the registry answers besides its wide surface, by index into
// their resolved entry points. The registry below the hooks never calls
// them (an ANSI or pre-Ex call reaches it as the wide Ex form), so their
// trampolines live here and serve VRegHooks_Neutralize alone.
typedef enum
{
#define VREG_INDEX(name) VREG_INTERNAL_FORM_##name,
    VREG_INTERNAL_REGISTRY_FORMS(VREG_INDEX)
#undef VREG_INDEX

    VREG_INTERNAL_FORM_COUNT
} VRegInternalFormIndex;

static void* g_formTargets[VREG_INTERNAL_FORM_COUNT];

typedef LSTATUS (APIENTRY* VRegInternal_RegSaveKeyAFunction)(
    HKEY,
    LPCSTR,
    CONST LPSECURITY_ATTRIBUTES
);

typedef LSTATUS (APIENTRY* VRegInternal_RegSaveKeyWFunction)(
    HKEY,
    LPCWSTR,
    CONST LPSECURITY_ATTRIBUTES
);

typedef LSTATUS (APIENTRY* VRegInternal_RegSaveKeyExAFunction)(
    HKEY,
    LPCSTR,
    CONST LPSECURITY_ATTRIBUTES,
    DWORD
);

typedef LSTATUS (APIENTRY* VRegInternal_RegSaveKeyExWFunction)(
    HKEY,
    LPCWSTR,
    CONST LPSECURITY_ATTRIBUTES,
    DWORD
);

typedef LSTATUS (APIENTRY* VRegInternal_RegRestoreKeyAFunction)(
    HKEY,
    LPCSTR,
    DWORD
);

typedef LSTATUS (APIENTRY* VRegInternal_RegRestoreKeyWFunction)(
    HKEY,
    LPCWSTR,
    DWORD
);

typedef LSTATUS (APIENTRY* VRegInternal_RegLoadKeyAFunction)(
    HKEY,
    LPCSTR,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegLoadKeyWFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegUnLoadKeyAFunction)(
    HKEY,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegUnLoadKeyWFunction)(
    HKEY,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegReplaceKeyAFunction)(
    HKEY,
    LPCSTR,
    LPCSTR,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegReplaceKeyWFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegGetKeySecurityFunction)(
    HKEY,
    SECURITY_INFORMATION,
    PSECURITY_DESCRIPTOR,
    LPDWORD
);

typedef LSTATUS (APIENTRY* VRegInternal_RegSetKeySecurityFunction)(
    HKEY,
    SECURITY_INFORMATION,
    PSECURITY_DESCRIPTOR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegRenameKeyFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VRegInternal_RegCopyTreeAFunction)(
    HKEY,
    LPCSTR,
    HKEY
);

typedef LSTATUS (APIENTRY* VRegInternal_RegCopyTreeWFunction)(
    HKEY,
    LPCWSTR,
    HKEY
);

typedef LSTATUS (APIENTRY* VRegInternal_RegCreateKeyTransactedAFunction)(
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

typedef LSTATUS (APIENTRY* VRegInternal_RegCreateKeyTransactedWFunction)(
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

typedef LSTATUS (APIENTRY* VRegInternal_RegOpenKeyTransactedAFunction)(
    HKEY,
    LPCSTR,
    DWORD,
    REGSAM,
    PHKEY,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VRegInternal_RegOpenKeyTransactedWFunction)(
    HKEY,
    LPCWSTR,
    DWORD,
    REGSAM,
    PHKEY,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VRegInternal_RegDeleteKeyTransactedAFunction)(
    HKEY,
    LPCSTR,
    REGSAM,
    DWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VRegInternal_RegDeleteKeyTransactedWFunction)(
    HKEY,
    LPCWSTR,
    REGSAM,
    DWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VRegInternal_RegDisableReflectionKeyFunction)(
    HKEY
);

typedef LSTATUS (APIENTRY* VRegInternal_RegEnableReflectionKeyFunction)(
    HKEY
);

typedef LSTATUS (APIENTRY* VRegInternal_RegQueryReflectionKeyFunction)(
    HKEY,
    BOOL*
);

typedef LSTATUS (APIENTRY* VRegInternal_RegOverridePredefKeyFunction)(
    HKEY,
    HKEY
);

static BOOL VRegInternal_IsOwned(_In_opt_ const HKEY hKey)
{
    return VRegInternal_HandleSpaceOwns(VRegInternal_ProcessHandleSpace(), hKey);
}

/// Whether a @ref RegOpenKeyTransactedW call names no transaction, which it
/// answers with @ref ERROR_INVALID_TRANSACTION. The hook answers that
/// itself: on ARM64 the function reaches that answer through a branch back
/// into its first 12 bytes, which the hook's jump overwrites.
///
/// Answered on every architecture rather than on ARM64 alone, since the native
/// answer is the same everywhere and one path is simpler.
static BOOL VRegInternal_IsUntransacted(
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
static LSTATUS VRegInternal_RefuseMountPathW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszSubKey,
    const REGSAM samDesired
)
{
    VReg* const pRegistry = VRegInternal_HookRegistry();
    const VRegInternalWStringView subKey = VRegInternal_ViewOf(pszSubKey);
    VRegInternalWString prefix = {0, 0, NULL};
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T end;
    for (end = 0; end <= subKey.cch; ++end)
    {
        HKEY hProbe = NULL;
        BOOL isVirtual;
        LSTATUS opened;
        if (end != subKey.cch && subKey.pch[end] != L'\\')
            continue;

        if (!VRegInternal_WStringAssign(&prefix, subKey.pch, end))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }

        opened = VReg_OpenKeyExW(
            pRegistry,
            hKey,
            VRegInternal_WStringText(&prefix),
            0,
            KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES),
            &hProbe
        );
        if (opened != ERROR_SUCCESS)
            continue;

        isVirtual = VRegInternal_IsOwned(hProbe);
        // An empty path may answer hKey itself rather than a new handle.
        if (hProbe != hKey)
        {
            VReg_CloseKey(
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

    VRegInternal_WStringFree(&prefix);
    return status;
}

static LSTATUS VRegInternal_RefuseMountPathA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszSubKey,
    const REGSAM samDesired
)
{
    VRegInternalWideArg subKey;
    LSTATUS status = VRegInternal_WideArgInit(pszSubKey, &subKey);
    if (status == ERROR_SUCCESS)
        status = VRegInternal_RefuseMountPathW(hKey, VRegInternal_WideArgText(&subKey), samDesired);

    VRegInternal_WideArgFree(&subKey);
    return status;
}

static LSTATUS APIENTRY VRegInternal_RefuseRegSaveKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
    const VRegInternal_RegSaveKeyAFunction pfnNext =
        (VRegInternal_RegSaveKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyA];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegSaveKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
    const VRegInternal_RegSaveKeyWFunction pfnNext =
        (VRegInternal_RegSaveKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyW];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegSaveKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ const DWORD Flags
)
{
    const VRegInternal_RegSaveKeyExAFunction pfnNext =
        (VRegInternal_RegSaveKeyExAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyExA];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes,
        Flags
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegSaveKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ const DWORD Flags
)
{
    const VRegInternal_RegSaveKeyExWFunction pfnNext =
        (VRegInternal_RegSaveKeyExWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyExW];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes,
        Flags
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegRestoreKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_ const DWORD dwFlags
)
{
    const VRegInternal_RegRestoreKeyAFunction pfnNext =
        (VRegInternal_RegRestoreKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegRestoreKeyA];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        dwFlags
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegRestoreKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_ const DWORD dwFlags
)
{
    const VRegInternal_RegRestoreKeyWFunction pfnNext =
        (VRegInternal_RegRestoreKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegRestoreKeyW];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        dwFlags
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegLoadKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const LPCSTR lpFile
)
{
    const VRegInternal_RegLoadKeyAFunction pfnNext =
        (VRegInternal_RegLoadKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegLoadKeyA];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpFile
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegLoadKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const LPCWSTR lpFile
)
{
    const VRegInternal_RegLoadKeyWFunction pfnNext =
        (VRegInternal_RegLoadKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegLoadKeyW];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpFile
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegUnLoadKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    const VRegInternal_RegUnLoadKeyAFunction pfnNext =
        (VRegInternal_RegUnLoadKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegUnLoadKeyA];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegUnLoadKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    const VRegInternal_RegUnLoadKeyWFunction pfnNext =
        (VRegInternal_RegUnLoadKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegUnLoadKeyW];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegReplaceKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const LPCSTR lpNewFile,
    _In_ const LPCSTR lpOldFile
)
{
    const VRegInternal_RegReplaceKeyAFunction pfnNext =
        (VRegInternal_RegReplaceKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegReplaceKeyA];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpNewFile,
        lpOldFile
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegReplaceKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const LPCWSTR lpNewFile,
    _In_ const LPCWSTR lpOldFile
)
{
    const VRegInternal_RegReplaceKeyWFunction pfnNext =
        (VRegInternal_RegReplaceKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegReplaceKeyW];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpNewFile,
        lpOldFile
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegGetKeySecurity(
    _In_ const HKEY hKey,
    _In_ const SECURITY_INFORMATION SecurityInformation,
    _Out_writes_bytes_opt_(*lpcbSecurityDescriptor) const PSECURITY_DESCRIPTOR pSecurityDescriptor,
    _Inout_ const LPDWORD lpcbSecurityDescriptor
)
{
    const VRegInternal_RegGetKeySecurityFunction pfnNext =
        (VRegInternal_RegGetKeySecurityFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegGetKeySecurity];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        SecurityInformation,
        pSecurityDescriptor,
        lpcbSecurityDescriptor
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegSetKeySecurity(
    _In_ const HKEY hKey,
    _In_ const SECURITY_INFORMATION SecurityInformation,
    _In_ const PSECURITY_DESCRIPTOR pSecurityDescriptor
)
{
    const VRegInternal_RegSetKeySecurityFunction pfnNext =
        (VRegInternal_RegSetKeySecurityFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSetKeySecurity];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        SecurityInformation,
        pSecurityDescriptor
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegRenameKey(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKeyName,
    _In_ const LPCWSTR lpNewKeyName
)
{
    const VRegInternal_RegRenameKeyFunction pfnNext =
        (VRegInternal_RegRenameKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegRenameKey];
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKeyName,
        lpNewKeyName
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegCopyTreeA(
    _In_ const HKEY hKeySrc,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const HKEY hKeyDest
)
{
    const VRegInternal_RegCopyTreeAFunction pfnNext =
        (VRegInternal_RegCopyTreeAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCopyTreeA];
    if (VRegInternal_IsOwned(hKeySrc) || VRegInternal_IsOwned(hKeyDest))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKeySrc,
        lpSubKey,
        hKeyDest
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegCopyTreeW(
    _In_ const HKEY hKeySrc,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const HKEY hKeyDest
)
{
    const VRegInternal_RegCopyTreeWFunction pfnNext =
        (VRegInternal_RegCopyTreeWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCopyTreeW];
    if (VRegInternal_IsOwned(hKeySrc) || VRegInternal_IsOwned(hKeyDest))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKeySrc,
        lpSubKey,
        hKeyDest
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegCreateKeyTransactedA(
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
    const VRegInternal_RegCreateKeyTransactedAFunction pfnNext =
        (VRegInternal_RegCreateKeyTransactedAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCreateKeyTransactedA];
    LSTATUS status;
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VRegInternal_RefuseMountPathA(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VRegInternal_RefuseRegCreateKeyTransactedW(
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
    const VRegInternal_RegCreateKeyTransactedWFunction pfnNext =
        (VRegInternal_RegCreateKeyTransactedWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCreateKeyTransactedW];
    LSTATUS status;
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VRegInternal_RefuseMountPathW(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VRegInternal_RefuseRegOpenKeyTransactedA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VRegInternal_RegOpenKeyTransactedAFunction pfnNext =
        (VRegInternal_RegOpenKeyTransactedAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegOpenKeyTransactedA];
    LSTATUS status;
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VRegInternal_RefuseMountPathA(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VRegInternal_RefuseRegOpenKeyTransactedW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VRegInternal_RegOpenKeyTransactedWFunction pfnNext =
        (VRegInternal_RegOpenKeyTransactedWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegOpenKeyTransactedW];
    LSTATUS status;
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VRegInternal_RefuseMountPathW(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;
    if (VRegInternal_IsUntransacted(hTransaction, pExtendedParemeter))
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

static LSTATUS APIENTRY VRegInternal_RefuseRegDeleteKeyTransactedA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParameter
)
{
    const VRegInternal_RegDeleteKeyTransactedAFunction pfnNext =
        (VRegInternal_RegDeleteKeyTransactedAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegDeleteKeyTransactedA];
    LSTATUS status;
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VRegInternal_RefuseMountPathA(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VRegInternal_RefuseRegDeleteKeyTransactedW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParameter
)
{
    const VRegInternal_RegDeleteKeyTransactedWFunction pfnNext =
        (VRegInternal_RegDeleteKeyTransactedWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegDeleteKeyTransactedW];
    LSTATUS status;
    if (VRegInternal_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VRegInternal_RefuseMountPathW(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VRegInternal_RefuseRegDisableReflectionKey(
    _In_ const HKEY hBase
)
{
    const VRegInternal_RegDisableReflectionKeyFunction pfnNext =
        (VRegInternal_RegDisableReflectionKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegDisableReflectionKey];
    if (VRegInternal_IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegEnableReflectionKey(
    _In_ const HKEY hBase
)
{
    const VRegInternal_RegEnableReflectionKeyFunction pfnNext =
        (VRegInternal_RegEnableReflectionKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegEnableReflectionKey];
    if (VRegInternal_IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegQueryReflectionKey(
    _In_ const HKEY hBase,
    _Out_ BOOL* const bIsReflectionDisabled
)
{
    const VRegInternal_RegQueryReflectionKeyFunction pfnNext =
        (VRegInternal_RegQueryReflectionKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegQueryReflectionKey];
    if (VRegInternal_IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase,
        bIsReflectionDisabled
    );
}

static LSTATUS APIENTRY VRegInternal_RefuseRegOverridePredefKey(
    _In_ const HKEY hKey,
    _In_opt_ const HKEY hNewHKey
)
{
    const VRegInternal_RegOverridePredefKeyFunction pfnNext =
        (VRegInternal_RegOverridePredefKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegOverridePredefKey];
    if (VRegInternal_IsOwned(hKey) || VRegInternal_IsOwned(hNewHKey))
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
#define VREG_INTERNAL_THUNK_CODE_SIZE ((SIZE_T)16)
#define VREG_INTERNAL_THUNK_PAGE_SIZE ((SIZE_T)4096)
#define VREG_INTERNAL_THUNK_CAPACITY (VREG_INTERNAL_THUNK_PAGE_SIZE / VREG_INTERNAL_THUNK_CODE_SIZE)

static BYTE* g_pbThunks;

static void** VRegInternal_ThunkSlot(const SIZE_T index)
{
    return (void**)(g_pbThunks + VREG_INTERNAL_THUNK_PAGE_SIZE) + index;
}

/// Writes thunk @p index, initially jumping to @p pTarget, and answers its
/// address.
static void* VRegInternal_WriteThunk(
    const SIZE_T index,
    _In_ void* const pTarget
)
{
    BYTE* const pbCode = g_pbThunks + index * VREG_INTERNAL_THUNK_CODE_SIZE;
    void** const ppSlot = VRegInternal_ThunkSlot(index);
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
static void** g_ppTargets[VREG_INTERNAL_THUNK_CAPACITY];
static volatile LONG g_cHooks;

typedef struct
{
    void** ppTarget;
    void* pBody;
} VRegInternalHook;

VREG_DECL LONG WINAPI VRegHooks_Install(
    _In_ VReg* const pRegistry,
    _In_ VReg* const pNative
)
{
    static volatile LONG s_isAttached;
    VRegNativeFunctions* functions;
    VRegInternalHook hooks[VREG_INTERNAL_THUNK_CAPACITY];
    void* thunks[VREG_INTERNAL_THUNK_CAPACITY];
    SIZE_T cHooks = 0;
    SIZE_T i;
    DWORD dwOld = 0;
    LONG error = DetourTransactionBegin();
    if (error != NO_ERROR)
        return error;

    error = DetourUpdateThread(GetCurrentThread());
    if (error != NO_ERROR)
        goto done;

    if (InterlockedExchange(&s_isAttached, 1))
    {
        error = ERROR_ALREADY_INITIALIZED;
        goto done;
    }

    WritePointerRelease((PVOID volatile*)&g_pRegistry, pRegistry);
    functions = VReg_GetNativeFunctions(pNative);

#define VREG_EMULATED(name) \
    hooks[cHooks].ppTarget = (void**)&functions->name; \
    hooks[cHooks].pBody = (void*)&VRegInternal_Hook##name; \
    ++cHooks;
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_EMULATED)
#undef VREG_EMULATED

    // A form this system does not export has nothing to hook.
#define VREG_FORM(name) \
    g_formTargets[VREG_INTERNAL_FORM_##name] = VRegInternal_FindRegistryExport(#name); \
    if (g_formTargets[VREG_INTERNAL_FORM_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_formTargets[VREG_INTERNAL_FORM_##name]; \
        hooks[cHooks].pBody = (void*)&VRegInternal_Hook##name; \
        ++cHooks; \
    }
    VREG_INTERNAL_REGISTRY_FORMS(VREG_FORM)
#undef VREG_FORM

    // A refused function this system does not export has nothing to hook.
#define VREG_REFUSED(name) \
    g_refusedTargets[VREG_INTERNAL_REFUSED_##name] = VRegInternal_FindRegistryExport(#name); \
    if (g_refusedTargets[VREG_INTERNAL_REFUSED_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_refusedTargets[VREG_INTERNAL_REFUSED_##name]; \
        hooks[cHooks].pBody = (void*)&VRegInternal_Refuse##name; \
        ++cHooks; \
    }
    VREG_INTERNAL_REFUSED_HOOKS(VREG_REFUSED)
#undef VREG_REFUSED

    g_pbThunks =
        (BYTE*)VirtualAlloc(NULL, 2 * VREG_INTERNAL_THUNK_PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_pbThunks)
    {
        InterlockedExchange(&s_isAttached, 0);
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto done;
    }

    for (i = 0; i < cHooks; ++i)
    {
        g_ppTargets[i] = hooks[i].ppTarget;
        thunks[i] = VRegInternal_WriteThunk(i, hooks[i].pBody);
    }

    // The code page becomes executable and read-only.
    if (!VirtualProtect(g_pbThunks, VREG_INTERNAL_THUNK_PAGE_SIZE, PAGE_EXECUTE_READ, &dwOld))
    {
        InterlockedExchange(&s_isAttached, 0);
        error = (LONG)GetLastError();
        goto done;
    }
    FlushInstructionCache(GetCurrentProcess(), g_pbThunks, VREG_INTERNAL_THUNK_PAGE_SIZE);

    WriteRelease(&g_cHooks, (LONG)cHooks);
    for (i = 0; i < cHooks; ++i)
    {
        // The transaction is aborted on failure, so nothing is hooked and a
        // later attempt may start over (with fresh thunks).
        error = DetourAttach(hooks[i].ppTarget, thunks[i]);
        if (error != NO_ERROR)
        {
            WriteRelease(&g_cHooks, 0);
            InterlockedExchange(&s_isAttached, 0);
            goto done;
        }
    }

done:
    if (error != NO_ERROR)
    {
        DetourTransactionAbort();
        return error;
    }

    return DetourTransactionCommit();
}

VREG_DECL void WINAPI VRegHooks_Neutralize(void)
{
    // Whatever the target field holds is what lies below this layer: the
    // trampoline once committed, the function itself before.
    const LONG cHooks = ReadAcquire(&g_cHooks);
    LONG i;
    for (i = 0; i < cHooks; ++i)
        InterlockedExchangePointer(VRegInternal_ThunkSlot((SIZE_T)i), *g_ppTargets[i]);
}

#undef VREG_INTERNAL_REFUSED_HOOKS

#endif // VREG_INTERNAL_HOOKS_IMPLEMENTATION_INCLUDED
#endif // VREG_INTERNAL_HOOKS_IMPLEMENTATION
#endif // VREG_WITH_DETOURS
