/// @file vreg.h
/// @brief A virtual Windows registry for C, as an overlay over the real one, or
///  held entirely in memory.
/// @author Johannes Wüller
/// @version @see VREG_VERSION_MAJOR, VREG_VERSION_MINOR, VREG_VERSION_PATCH,
///  VREG_VERSION_TWEAK
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

// Raised with every change to this file. Tweak is unused (always 0).
#define VREG_VERSION_MAJOR 0
#define VREG_VERSION_MINOR 17
#define VREG_VERSION_PATCH 1
#define VREG_VERSION_TWEAK 0

// Packed as MAKEDLLVERULL packs them, spelled out so no Shlwapi.h is needed.
#define VREG_VERSION (                                                         \
    ((ULONGLONG)VREG_VERSION_MAJOR << 48) |                                    \
    ((ULONGLONG)VREG_VERSION_MINOR << 32) |                                    \
    ((ULONGLONG)VREG_VERSION_PATCH << 16) |                                    \
    ((ULONGLONG)VREG_VERSION_TWEAK <<  0)                                      \
)

// ==========================================================================
// Linkage
// ==========================================================================

/// What every VReg declaration carries: the linkage of the functions.
/// Defining @ref VREG_STATIC wherever the header is included (the
/// implementation too) makes every function `static`, so a module can
/// hold a private copy beside another module's; two copies in one module
/// behave as two copies and share nothing but the handle namespace.
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
 *  registry. Callers go through the flat functions (`VReg_RegOpenKeyExW` and so
 *  on), never through a table directly, and free every registry with
 *  @ref VReg_Destroy.
 *
 * @note Once hooked, every slot may run under a foreign loader lock: an
 *  implementation MUST NOT load a module, create a thread or wait on a lock a
 *  loader-lock holder could own.
 */
typedef struct VReg VReg;

typedef struct VRegVtbl VRegVtbl;

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

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
typedef LSTATUS WINAPI VRegOpenKeyExAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _Out_ PHKEY phkResult
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
typedef LSTATUS WINAPI VRegOpenKeyExWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
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

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
typedef LSTATUS WINAPI VRegDeleteValueAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpValueName
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
typedef LSTATUS WINAPI VRegDeleteValueWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName
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

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
typedef LSTATUS WINAPI VRegDeleteKeyExAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
typedef LSTATUS WINAPI VRegDeleteKeyExWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _In_ REGSAM samDesired,
    _Reserved_ DWORD Reserved
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
typedef LSTATUS WINAPI VRegDeleteTreeAFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCSTR lpSubKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
typedef LSTATUS WINAPI VRegDeleteTreeWFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey
);

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
typedef LSTATUS WINAPI VRegFlushKeyFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey
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

/// @see https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
typedef LSTATUS WINAPI VRegNotifyChangeKeyValueFunction(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ BOOL bWatchSubtree,
    _In_ DWORD dwNotifyFilter,
    _In_opt_ HANDLE hEvent,
    _In_ BOOL fAsynchronous
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

    VRegOpenKeyAFunction* RegOpenKeyA;
    VRegOpenKeyWFunction* RegOpenKeyW;
    VRegOpenKeyExAFunction* RegOpenKeyExA;
    VRegOpenKeyExWFunction* RegOpenKeyExW;
    VRegCreateKeyAFunction* RegCreateKeyA;
    VRegCreateKeyWFunction* RegCreateKeyW;
    VRegCreateKeyExAFunction* RegCreateKeyExA;
    VRegCreateKeyExWFunction* RegCreateKeyExW;
    VRegCloseKeyFunction* RegCloseKey;
    VRegQueryValueAFunction* RegQueryValueA;
    VRegQueryValueWFunction* RegQueryValueW;
    VRegQueryValueExAFunction* RegQueryValueExA;
    VRegQueryValueExWFunction* RegQueryValueExW;
    VRegGetValueAFunction* RegGetValueA;
    VRegGetValueWFunction* RegGetValueW;
    VRegQueryMultipleValuesAFunction* RegQueryMultipleValuesA;
    VRegQueryMultipleValuesWFunction* RegQueryMultipleValuesW;
    VRegSetValueAFunction* RegSetValueA;
    VRegSetValueWFunction* RegSetValueW;
    VRegSetValueExAFunction* RegSetValueExA;
    VRegSetValueExWFunction* RegSetValueExW;
    VRegSetKeyValueAFunction* RegSetKeyValueA;
    VRegSetKeyValueWFunction* RegSetKeyValueW;
    VRegDeleteValueAFunction* RegDeleteValueA;
    VRegDeleteValueWFunction* RegDeleteValueW;
    VRegDeleteKeyValueAFunction* RegDeleteKeyValueA;
    VRegDeleteKeyValueWFunction* RegDeleteKeyValueW;
    VRegLoadMUIStringAFunction* RegLoadMUIStringA;
    VRegLoadMUIStringWFunction* RegLoadMUIStringW;
    VRegDeleteKeyAFunction* RegDeleteKeyA;
    VRegDeleteKeyWFunction* RegDeleteKeyW;
    VRegDeleteKeyExAFunction* RegDeleteKeyExA;
    VRegDeleteKeyExWFunction* RegDeleteKeyExW;
    VRegDeleteTreeAFunction* RegDeleteTreeA;
    VRegDeleteTreeWFunction* RegDeleteTreeW;
    VRegFlushKeyFunction* RegFlushKey;
    VRegEnumKeyAFunction* RegEnumKeyA;
    VRegEnumKeyWFunction* RegEnumKeyW;
    VRegEnumKeyExAFunction* RegEnumKeyExA;
    VRegEnumKeyExWFunction* RegEnumKeyExW;
    VRegEnumValueAFunction* RegEnumValueA;
    VRegEnumValueWFunction* RegEnumValueW;
    VRegQueryInfoKeyAFunction* RegQueryInfoKeyA;
    VRegQueryInfoKeyWFunction* RegQueryInfoKeyW;
    VRegNotifyChangeKeyValueFunction* RegNotifyChangeKeyValue;
};

VREG_DECL VRegOpenKeyAFunction VReg_RegOpenKeyA;
VREG_DECL VRegOpenKeyWFunction VReg_RegOpenKeyW;
VREG_DECL VRegOpenKeyExAFunction VReg_RegOpenKeyExA;
VREG_DECL VRegOpenKeyExWFunction VReg_RegOpenKeyExW;
VREG_DECL VRegCreateKeyAFunction VReg_RegCreateKeyA;
VREG_DECL VRegCreateKeyWFunction VReg_RegCreateKeyW;
VREG_DECL VRegCreateKeyExAFunction VReg_RegCreateKeyExA;
VREG_DECL VRegCreateKeyExWFunction VReg_RegCreateKeyExW;
VREG_DECL VRegCloseKeyFunction VReg_RegCloseKey;
VREG_DECL VRegQueryValueAFunction VReg_RegQueryValueA;
VREG_DECL VRegQueryValueWFunction VReg_RegQueryValueW;
VREG_DECL VRegQueryValueExAFunction VReg_RegQueryValueExA;
VREG_DECL VRegQueryValueExWFunction VReg_RegQueryValueExW;
VREG_DECL VRegGetValueAFunction VReg_RegGetValueA;
VREG_DECL VRegGetValueWFunction VReg_RegGetValueW;
VREG_DECL VRegQueryMultipleValuesAFunction VReg_RegQueryMultipleValuesA;
VREG_DECL VRegQueryMultipleValuesWFunction VReg_RegQueryMultipleValuesW;
VREG_DECL VRegSetValueAFunction VReg_RegSetValueA;
VREG_DECL VRegSetValueWFunction VReg_RegSetValueW;
VREG_DECL VRegSetValueExAFunction VReg_RegSetValueExA;
VREG_DECL VRegSetValueExWFunction VReg_RegSetValueExW;
VREG_DECL VRegSetKeyValueAFunction VReg_RegSetKeyValueA;
VREG_DECL VRegSetKeyValueWFunction VReg_RegSetKeyValueW;
VREG_DECL VRegDeleteValueAFunction VReg_RegDeleteValueA;
VREG_DECL VRegDeleteValueWFunction VReg_RegDeleteValueW;
VREG_DECL VRegDeleteKeyValueAFunction VReg_RegDeleteKeyValueA;
VREG_DECL VRegDeleteKeyValueWFunction VReg_RegDeleteKeyValueW;
VREG_DECL VRegLoadMUIStringAFunction VReg_RegLoadMUIStringA;
VREG_DECL VRegLoadMUIStringWFunction VReg_RegLoadMUIStringW;
VREG_DECL VRegDeleteKeyAFunction VReg_RegDeleteKeyA;
VREG_DECL VRegDeleteKeyWFunction VReg_RegDeleteKeyW;
VREG_DECL VRegDeleteKeyExAFunction VReg_RegDeleteKeyExA;
VREG_DECL VRegDeleteKeyExWFunction VReg_RegDeleteKeyExW;
VREG_DECL VRegDeleteTreeAFunction VReg_RegDeleteTreeA;
VREG_DECL VRegDeleteTreeWFunction VReg_RegDeleteTreeW;
VREG_DECL VRegFlushKeyFunction VReg_RegFlushKey;
VREG_DECL VRegEnumKeyAFunction VReg_RegEnumKeyA;
VREG_DECL VRegEnumKeyWFunction VReg_RegEnumKeyW;
VREG_DECL VRegEnumKeyExAFunction VReg_RegEnumKeyExA;
VREG_DECL VRegEnumKeyExWFunction VReg_RegEnumKeyExW;
VREG_DECL VRegEnumValueAFunction VReg_RegEnumValueA;
VREG_DECL VRegEnumValueWFunction VReg_RegEnumValueW;
VREG_DECL VRegQueryInfoKeyAFunction VReg_RegQueryInfoKeyA;
VREG_DECL VRegQueryInfoKeyWFunction VReg_RegQueryInfoKeyW;
VREG_DECL VRegNotifyChangeKeyValueFunction VReg_RegNotifyChangeKeyValue;

/// The neutral names select the wide or the ANSI form by @ref UNICODE, as
/// `winreg.h` does for the functions they mirror.
#ifdef UNICODE
#define VReg_RegOpenKey VReg_RegOpenKeyW
#define VReg_RegOpenKeyEx VReg_RegOpenKeyExW
#define VReg_RegCreateKey VReg_RegCreateKeyW
#define VReg_RegCreateKeyEx VReg_RegCreateKeyExW
#define VReg_RegQueryValue VReg_RegQueryValueW
#define VReg_RegQueryValueEx VReg_RegQueryValueExW
#define VReg_RegGetValue VReg_RegGetValueW
#define VReg_RegQueryMultipleValues VReg_RegQueryMultipleValuesW
#define VReg_RegSetValue VReg_RegSetValueW
#define VReg_RegSetValueEx VReg_RegSetValueExW
#define VReg_RegSetKeyValue VReg_RegSetKeyValueW
#define VReg_RegDeleteValue VReg_RegDeleteValueW
#define VReg_RegDeleteKeyValue VReg_RegDeleteKeyValueW
#define VReg_RegLoadMUIString VReg_RegLoadMUIStringW
#define VReg_RegDeleteKey VReg_RegDeleteKeyW
#define VReg_RegDeleteKeyEx VReg_RegDeleteKeyExW
#define VReg_RegDeleteTree VReg_RegDeleteTreeW
#define VReg_RegEnumKey VReg_RegEnumKeyW
#define VReg_RegEnumKeyEx VReg_RegEnumKeyExW
#define VReg_RegEnumValue VReg_RegEnumValueW
#define VReg_RegQueryInfoKey VReg_RegQueryInfoKeyW
#else
#define VReg_RegOpenKey VReg_RegOpenKeyA
#define VReg_RegOpenKeyEx VReg_RegOpenKeyExA
#define VReg_RegCreateKey VReg_RegCreateKeyA
#define VReg_RegCreateKeyEx VReg_RegCreateKeyExA
#define VReg_RegQueryValue VReg_RegQueryValueA
#define VReg_RegQueryValueEx VReg_RegQueryValueExA
#define VReg_RegGetValue VReg_RegGetValueA
#define VReg_RegQueryMultipleValues VReg_RegQueryMultipleValuesA
#define VReg_RegSetValue VReg_RegSetValueA
#define VReg_RegSetValueEx VReg_RegSetValueExA
#define VReg_RegSetKeyValue VReg_RegSetKeyValueA
#define VReg_RegDeleteValue VReg_RegDeleteValueA
#define VReg_RegDeleteKeyValue VReg_RegDeleteKeyValueA
#define VReg_RegLoadMUIString VReg_RegLoadMUIStringA
#define VReg_RegDeleteKey VReg_RegDeleteKeyA
#define VReg_RegDeleteKeyEx VReg_RegDeleteKeyExA
#define VReg_RegDeleteTree VReg_RegDeleteTreeA
#define VReg_RegEnumKey VReg_RegEnumKeyA
#define VReg_RegEnumKeyEx VReg_RegEnumKeyExA
#define VReg_RegEnumValue VReg_RegEnumValueA
#define VReg_RegQueryInfoKey VReg_RegQueryInfoKeyA
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
 *  on its own, as an overlay over a key of another registry, or answered by the
 *  slots of a @ref VRegHiveVtbl (@ref VRegHive_CreateCustom).
 *
 *  On its own, the hive starts empty and holds everything written to it.
 *  As an overlay over a lower key, reads fall through to that key and writes stay
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
 *  Frozen, an overlay hive answers an open of a key it has nothing at or below
 *  with the lower registry's own handle. A virtual registry sends later calls
 *  on such a handle to its base, so a frozen hive's lower registry MUST be
 *  that base or answer the same handles.
 *
 * @note A hive MUST outlive every registry it is mounted in. Destroying it
 *  turns its handles into invalid ones.
 *
 * @note The loader-lock rule of @ref VReg binds every call on a hive.
 */
typedef struct VRegHive VRegHive;

/// A hive held entirely in memory; starts empty.
VREG_DECL LSTATUS WINAPI VRegHive_CreateInMemory(
    _Outptr_ VRegHive** ppHive
);

/// A hive over the key @p pszLowerPath below @p hLowerRoot of @p pLower,
/// which MUST outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
/// @ref KEY_WOW64_32KEY or 0) is added to every lower open.
VREG_DECL LSTATUS WINAPI VRegHive_CreateOverlay(
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
typedef struct VRegHiveInternalNode VRegHiveKey;

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
 *  concurrently, and under a foreign loader lock, so the rule of @ref VReg
 *  binds them. VReg holds no lock of its own across a slot call; the
 *  provider locks its own state. A slot never sees the access a key was
 *  opened with.
 */
typedef struct
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

/// The real registry, through a table resolved from the export tables of
/// `KernelBase.dll` and `advapi32.dll` without loading a module, so it can be
/// made under the loader lock. A function neither module exports stays null.
/// Freed with @ref VReg_Destroy.
VREG_DECL LSTATUS WINAPI VReg_CreateNative(
    _Outptr_ VReg** ppRegistry
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
 *  @p pBase answers everything outside the mounts; null for a registry held
 *  entirely in memory. Creating, mounting and resolving a path load no
 *  module. Freed with @ref VReg_Destroy.
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
/// answers @ref ERROR_INVALID_PARAMETER. Safe while calls are in flight,
/// hooked ones included.
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
 *  `REGEDIT4` header means @ref CP_ACP and anything else UTF-8. `[path]`
 *  creates a key and `[-path]` deletes it with its subtree, the root by full
 *  name or as `HKLM`, `HKCU`, `HKCR`, `HKU` or `HKCC`; `"name"=` and `@=` set
 *  a value as a string with `\\` and `\"` escapes, `dword:`, `hex:` or
 *  `hex(n):` for any type, and `=-` deletes it; trailing-backslash
 *  continuations, `;` comments and blank lines are accepted. The script is
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

/// @ref VReg_ApplyOperations runs every operation, as `regedit /s` and
/// `reg import` do, instead of stopping at the first that fails.
#define VREG_APPLY_CONTINUE_ON_ERROR ((DWORD)0x1)

/**
 * @brief Performs @p cOperations operations through @p pRegistry, in order.
 *
 *  Stops at the first operation that fails, stores its index in
 *  @p pFailedIndex (MAY be null) and returns its status; earlier operations
 *  stay applied, and nothing is rolled back. With
 *  @ref VREG_APPLY_CONTINUE_ON_ERROR in @p dwFlags the later operations run
 *  too, and the first failure is still the one stored and returned. Deleting
 *  a key or value that does not exist is not a failure. An operation that
 *  would delete a whole root (a key deletion with an empty path) answers
 *  @ref ERROR_INVALID_PARAMETER, as does a flag that is not defined, before
 *  anything is applied.
 */
_Success_(return == ERROR_SUCCESS)
VREG_DECL LSTATUS WINAPI VReg_ApplyOperations(
    _In_ VReg* pRegistry,
    _In_reads_(cOperations) const VRegOperation* pOperations,
    _In_ SIZE_T cOperations,
    _In_ DWORD dwFlags,
    _Pre_maybenull_ _Pre_writable_size_(1) SIZE_T* pFailedIndex
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
VREG_DECL LSTATUS WINAPI VRegScript_CreateSnapshot(
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
 *  @ref VRegScript_CreateSnapshot. A tombstone with no lower counterpart is
 *  kept, and does nothing when applied. Freed with @ref VRegScript_Destroy.
 *
 * @return As @ref VRegScript_CreateSnapshot, except that the hive is read
 *  instead of a registry, and that @ref ERROR_NOT_SUPPORTED answers a hive
 *  whose root is deleted or hides its lower counterpart while @p pszPath is
 *  null or empty: the operation would delete a predefined root, which a
 *  script cannot hold. No script is created.
 */
VREG_DECL LSTATUS WINAPI VRegScript_CreateDelta(
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

#ifdef VREG_WITH_DETOURS

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
// Hook bodies run under the loader-lock rule of @ref VReg. The one load is
// the call's own: `RegLoadMUIStringW` resolving an indirect string loads its
// module, as it does natively.
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

#endif // VREG_WITH_DETOURS

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VREG_H_INCLUDED

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
#define VREG_ASSERT(x) ((x) ? (void)0 : VReg_InternalAssertFailed())
#else
#define VREG_ASSERT(x) ((void)0)
#endif
#endif

VREG_DECL void VReg_InternalAssertFailed(void);

/// Allocates @p cb zero-filled bytes; null when memory ran out.
VREG_DECL _Ret_maybenull_ void* VReg_InternalAllocZeroed(
    SIZE_T cb
);

/// Resizes @p p (null allocates) to @p cb bytes; null when memory ran out,
/// leaving @p p as it was.
VREG_DECL _Ret_maybenull_ void* VReg_InternalReAlloc(
    _In_opt_ void* p,
    SIZE_T cb
);

/// Frees @p p; null is ignored.
VREG_DECL void VReg_InternalFree(
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

VREG_DECL VRegInternalWStringView VReg_InternalViewFromText(
    _In_opt_ LPCWSTR psz
);

VREG_DECL DWORD VReg_InternalMinDword(DWORD a, DWORD b);
VREG_DECL DWORD VReg_InternalMaxDword(DWORD a, DWORD b);

typedef struct
{
    SIZE_T cbCapacity;
    _Field_range_(0, cbCapacity) SIZE_T cb;
    _Field_size_opt_(cbCapacity) BYTE* pb;
} VRegInternalBuffer;

/// Sets the size to @p cb, growing the capacity if needed; new bytes are
/// indeterminate.
VREG_DECL BOOL VRegInternalBuffer_Resize(
    _Inout_ VRegInternalBuffer* pBuffer,
    SIZE_T cb
);

VREG_DECL BOOL VRegInternalBuffer_Assign(
    _Inout_ VRegInternalBuffer* pBuffer,
    _In_reads_bytes_opt_(cb) const void* p,
    SIZE_T cb
);

VREG_DECL BOOL VRegInternalBuffer_Append(
    _Inout_ VRegInternalBuffer* pBuffer,
    _In_reads_bytes_(cb) const void* p,
    SIZE_T cb
);

VREG_DECL void VRegInternalBuffer_Free(
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
VREG_DECL _Ret_notnull_ LPCWSTR VRegInternalWString_GetText(
    _In_ const VRegInternalWString* pString
);

VREG_DECL VRegInternalWStringView VRegInternalWString_GetView(
    _In_ const VRegInternalWString* pString
);

/// Room for @p cch characters and the terminator.
VREG_DECL BOOL VRegInternalWString_Reserve(
    _Inout_ VRegInternalWString* pString,
    SIZE_T cch
);

/// Sets the length to @p cch, growing if needed, and terminates; new
/// characters are indeterminate.
VREG_DECL BOOL VRegInternalWString_Resize(
    _Inout_ VRegInternalWString* pString,
    SIZE_T cch
);

VREG_DECL BOOL VRegInternalWString_Assign(
    _Inout_ VRegInternalWString* pString,
    _In_reads_opt_(cch) LPCWCH pch,
    SIZE_T cch
);

VREG_DECL BOOL VRegInternalWString_Append(
    _Inout_ VRegInternalWString* pString,
    _In_reads_(cch) LPCWCH pch,
    SIZE_T cch
);

VREG_DECL BOOL VRegInternalWString_AppendChar(
    _Inout_ VRegInternalWString* pString,
    wchar_t ch
);

/// Shortens the string to @p cch characters, which is at most its length.
VREG_DECL void VRegInternalWString_Truncate(
    _Inout_ VRegInternalWString* pString,
    SIZE_T cch
);

VREG_DECL void VRegInternalWString_Free(
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

VREG_DECL BOOL VRegInternalArray_Reserve(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    SIZE_T capacity
);

/// Inserts a copy of @p pElement before @p index, which is at most the count.
VREG_DECL BOOL VRegInternalArray_Insert(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    SIZE_T index,
    _In_reads_bytes_(cbElement) const void* pElement
);

VREG_DECL BOOL VRegInternalArray_Append(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* pElement
);

/// Removes the element at @p index, moving the later ones down.
VREG_DECL void VRegInternalArray_RemoveAt(
    _Inout_ VRegInternalArray* pArray,
    SIZE_T cbElement,
    SIZE_T index
);

VREG_DECL void VRegInternalArray_Free(
    _Inout_ VRegInternalArray* pArray
);

/// The view of the string at @p index of @p pArray, an array of
/// @ref VRegInternalWString.
VREG_DECL VRegInternalWStringView VRegInternalArray_WStringViewAt(
    _In_ const VRegInternalArray* pArray,
    SIZE_T index
);

/// Appends a copy of @p text to @p pArray, an array of @ref VRegInternalWString.
VREG_DECL BOOL VRegInternalArray_AppendWString(
    _Inout_ VRegInternalArray* pArray,
    VRegInternalWStringView text
);

/// Adds a copy of @p text to @p pArray, an array of @ref VRegInternalWString sorted
/// by @ref VRegInternalWStringView_CompareNames, unless an equal name is there already.
VREG_DECL BOOL VRegInternalArray_InsertWStringSorted(
    _Inout_ VRegInternalArray* pArray,
    VRegInternalWStringView text
);

/// Whether the sorted @p pArray holds a name equal to @p text by
/// @ref VRegInternalWStringView_CompareNames.
VREG_DECL BOOL VRegInternalArray_ContainsWStringSorted(
    _In_ const VRegInternalArray* pArray,
    VRegInternalWStringView text
);

/// Frees the strings of @p pArray, an array of @ref VRegInternalWString, and the
/// array itself.
VREG_DECL void VRegInternalArray_FreeWStrings(
    _Inout_ VRegInternalArray* pArray
);

// ==========================================================================
// ANSI conversion
// ==========================================================================

/// Widens @p cch bytes through @ref CP_ACP into @p pText, without a
/// terminator beyond the one the string type keeps. Answers
/// @ref ERROR_OUTOFMEMORY when memory ran out or the text is longer than the
/// conversion can take.
VREG_DECL LSTATUS VReg_InternalWideFromAnsi(
    _In_reads_(cch) LPCCH pch,
    SIZE_T cch,
    _Inout_ VRegInternalWString* pText
);

/// Narrows @p cch UTF-16 units through @ref CP_ACP into @p pText, a buffer of
/// bytes without a terminator; what the code page cannot represent becomes
/// its default character. Answers @ref ERROR_OUTOFMEMORY as
/// @ref VReg_InternalWideFromAnsi does.
VREG_DECL LSTATUS VReg_InternalAnsiFromWide(
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
VREG_DECL LSTATUS VRegInternalWideArg_Init(
    _In_opt_ LPCSTR psz,
    _Out_ VRegInternalWideArg* pArg
);

/// The wide text, null when the argument was null.
VREG_DECL _Ret_maybenull_ LPCWSTR VRegInternalWideArg_GetText(
    _In_ const VRegInternalWideArg* pArg
);

/// Like @ref VRegInternalWideArg_GetText, as a buffer the callee may write to.
VREG_DECL _Ret_maybenull_ LPWSTR VRegInternalWideArg_GetMutableText(
    _In_ VRegInternalWideArg* pArg
);

VREG_DECL void VRegInternalWideArg_Free(
    _Inout_ VRegInternalWideArg* pArg
);

// ==========================================================================
// Hive nodes
// ==========================================================================

/// A key in a hive. Opaque outside the hive that owns it; never freed before
/// the hive, so a pointer to one stays valid for the hive's life.
typedef struct VRegHiveInternalNode VRegHiveInternalNode;

typedef struct
{
    VRegInternalWString name;
    DWORD dwType;
    VRegInternalBuffer data;

    /// Hides the lower value of this name; only in a hive with a lower
    /// registry.
    BOOL isTombstone;
} VRegHiveInternalValue;

/// A pending change notification: fires once, then is gone.
typedef struct
{
    /// Our own duplicate of the caller's event; closed when the watch fires or
    /// the hive goes.
    HANDLE hEvent;

    BOOL isSubtree;
    DWORD dwFilter;
} VRegHiveInternalChangeWatch;

/// The flags below @ref isDeleted describe a node's relation to the lower
/// registry and stay clear in a hive without one.
struct VRegHiveInternalNode
{
    VRegHive* pHive;
    VRegHiveInternalNode* pParent;
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

    /// `VRegHiveInternalNode*`, sorted by
    /// @ref VRegInternalWStringView_CompareNames, the order the native registry
    /// enumerates subkeys in.
    VRegInternalArray children;

    /// @ref VRegHiveInternalValue, in creation order, as the native registry
    /// enumerates values.
    VRegInternalArray values;

    /// @ref VRegHiveInternalChangeWatch, guarded by the hive's write lock.
    VRegInternalArray watches;
};

/// Ordinal comparison of uppercased names, the registry's own collation:
/// negative, zero or positive.
VREG_DECL int VRegInternalWStringView_CompareNames(
    VRegInternalWStringView left,
    VRegInternalWStringView right
);

// ==========================================================================
// Export tables
// ==========================================================================

/// The entry point @p hModule exports as @p pszName, read from its export
/// table; null when @p hModule is null, the name is not exported, or the
/// export forwards to another module.
VREG_DECL _Ret_maybenull_ void* VReg_InternalFindExport(
    _In_opt_ HMODULE hModule,
    _In_ LPCSTR pszName
);

/// A registry function, from `KernelBase.dll` or else `advapi32.dll`, which
/// alone exports the pre-`Ex` forms; null when neither exports it. Never
/// loads a module.
VREG_DECL _Ret_maybenull_ void* VReg_InternalFindRegistryExport(
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
VREG_DECL DWORD VReg_InternalGrow(DWORD cbOld, DWORD cbRequired);

VREG_DECL LSTATUS VReg_InternalQueryValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpValueName,
    _Out_opt_ LPDWORD lpType,
    _Inout_ VRegInternalBuffer* pData
);

VREG_DECL LSTATUS VReg_InternalGetValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValue,
    _In_ DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
    _Inout_ VRegInternalBuffer* pData
);

/// @ref VReg_RegEnumKeyExW with the name read into @p pName and, when
/// @p pClass is not null, the class into it. Both buffers grow until the read
/// fits or none can grow further, and then the status is answered as it is.
VREG_DECL LSTATUS VReg_InternalEnumKeyOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Inout_ VRegInternalWString* pName,
    _Inout_opt_ VRegInternalWString* pClass,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ PFILETIME lpftLastWriteTime
);

/// With a null @p pData only the name and type are read.
VREG_DECL LSTATUS VReg_InternalEnumValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Inout_ VRegInternalWString* pName,
    _Out_opt_ LPDWORD lpType,
    _Inout_opt_ VRegInternalBuffer* pData
);

/// @ref VReg_RegQueryInfoKeyW with the class read into @p pClass. When the read
/// fails and its last call reported a class length other than the buffer's,
/// that length is stored at @p lpcchClassReported.
VREG_DECL LSTATUS VReg_InternalQueryInfoKeyOwned(
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
VREG_DECL BOOL VReg_InternalIsStringType(DWORD dwType);

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
VREG_DECL BOOL VReg_InternalIsPredefinedHKey(
    _In_opt_ HKEY hKey
);

/// One more than the highest index @ref VReg_InternalVirtualHKeyFromIndex accepts.
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
 *  Initialized by @ref VRegInternalHandleSpace_Init; the members are private to
 *  the implementation's "The handle space" section.
 *
 * @note The section is mapped once and never unmapped or closed, so an
 *  instance must outlive every handle it issued: in practice, it is never
 *  destroyed (@ref VReg_InternalGetProcessHandleSpace).
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
/// unusable: @ref VRegInternalHandleSpace_Allocate then answers null.
VREG_DECL void VRegInternalHandleSpace_Init(
    _Out_ VRegInternalHandleSpace* pSpace,
    _In_ LPCWSTR pszName
);

/// This copy's instance over this process's section, created on first use
/// and never destroyed.
VREG_DECL _Ret_notnull_ VRegInternalHandleSpace* VReg_InternalGetProcessHandleSpace(
    void
);

VREG_DECL BOOL VRegInternalHandleSpace_IsUsable(
    _In_ const VRegInternalHandleSpace* pSpace
);

/// A fresh handle owned by @p pSpace and bound to @p pTarget, or null when
/// the section is unusable, the namespace is exhausted or memory ran out.
VREG_DECL _Ret_maybenull_ HKEY VRegInternalHandleSpace_Allocate(
    _Inout_ VRegInternalHandleSpace* pSpace,
    _In_opt_ void* pTarget
);

/// Whether @p hKey is a virtual handle from one of @p pSpace's blocks.
/// Lock-free.
VREG_DECL BOOL VRegInternalHandleSpace_Owns(
    _In_ const VRegInternalHandleSpace* pSpace,
    _In_opt_ HKEY hKey
);

/// Unbinds @p hKey from its target. The value is never handed out again;
/// later lookups answer null.
VREG_DECL void VRegInternalHandleSpace_Release(
    _Inout_ VRegInternalHandleSpace* pSpace,
    _In_opt_ HKEY hKey
);

/// The target @p hKey was allocated for; null when @p pSpace does not own
/// it. Lock-free.
VREG_DECL _Ret_maybenull_ void* VRegInternalHandleSpace_Find(
    _In_ const VRegInternalHandleSpace* pSpace,
    _In_opt_ HKEY hKey
);

/// The handle for @p index, counting down from @ref VREG_INTERNAL_VIRTUAL_HKEY_BASE in
/// odd steps; null when @p index is out of range.
VREG_DECL _Ret_maybenull_ HKEY VReg_InternalVirtualHKeyFromIndex(
    UINT_PTR index
);

/// The inverse of @ref VReg_InternalVirtualHKeyFromIndex; only meaningful when
/// @ref VReg_IsVirtualHKey holds.
VREG_DECL UINT_PTR VReg_InternalIndexFromVirtualHKey(
    _In_opt_ HKEY hKey
);

// ==========================================================================
// Hive internals
// ==========================================================================

// The node-level interface of a hive, with owned values, as
// the virtual registry's slots use it: caller buffers, ANSI and wide, and handles
// stay there, once for every hive. Every function answers the @ref LSTATUS
// the native registry answers for the same operation; a call on a deleted
// node answers @ref ERROR_KEY_DELETED, and a write to a frozen hive
// @ref ERROR_ACCESS_DENIED.
//
// A string or buffer a function fills is the caller's to free.

/// Whether @ref VRegHive_Freeze made the hive immutable.
VREG_DECL BOOL VRegHive_InternalIsFrozen(
    _In_ VRegHive* pHive
);

/// A key another registry answers. The caller owns @ref hKey and closes it
/// through @ref pRegistry.
typedef struct
{
    VReg* pRegistry;
    HKEY hKey;
} VRegInternalRedirect;

/// Where an open landed: a node of the hive, or (for a key a frozen overlay
/// hive leaves untouched) a redirect to the key its lower registry opened.
typedef struct
{
    VRegHiveInternalNode* pNode;
    VRegInternalRedirect redirect;
} VRegHiveInternalOpenResult;

/// Closes the key of @p pRedirect through its registry.
VREG_DECL void VRegInternalRedirect_Close(
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
typedef LSTATUS VRegHive_InternalOpenLowerFunction(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pParent,
    _In_ LPCWSTR pszSubKey,
    REGSAM samDesired,
    BOOL isRedirect,
    _Out_ VRegInternalRedirect* pRedirect
);

typedef enum
{
    VREG_INTERNAL_UPPER_KEY,
    VREG_INTERNAL_UPPER_VALUE,
} VRegHiveInternalUpperKind;

/// One entry of a hive's own layer, as @ref VRegHive_InternalSnapshotUpper lists
/// it; the views point into the hive and are valid during the visit only.
typedef struct
{
    VRegHiveInternalUpperKind kind;

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
} VRegHiveInternalUpperRecord;

/// Called once per record under the hive's read lock, so it MUST NOT call
/// back into the hive; a status other than @ref ERROR_SUCCESS stops the
/// snapshot and is answered.
typedef LSTATUS VRegInternalUpperVisitFunction(
    _In_opt_ void* pContext,
    _In_ const VRegHiveInternalUpperRecord* pRecord
);

VREG_DECL _Ret_notnull_ VRegHiveInternalNode* VRegHive_InternalGetRoot(
    _In_ VRegHive* pHive
);

/// The virtual handle of @p pNode.
VREG_DECL HKEY VRegHiveInternalNode_GetHandle(
    _In_ const VRegHiveInternalNode* pNode
);

/// The node a virtual handle of this copy of VReg belongs to; null for any
/// other handle.
VREG_DECL _Ret_maybenull_ VRegHiveInternalNode* VRegHive_InternalFindNode(
    _In_opt_ HKEY hKey
);

/// Opens the key @p cPath components below @p pParent. On a writable
/// overlay hive the nodes that stand for lower keys are added on the way,
/// which changes nothing the hive answers.
VREG_DECL LSTATUS VRegHive_InternalOpenKey(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pParent,
    _In_reads_(cPath) const VRegInternalWStringView* pPath,
    SIZE_T cPath,
    REGSAM samDesired,
    _Out_ VRegHiveInternalOpenResult* pResult
);

/// On a frozen hive an existing key opens as with @ref VRegHive_InternalOpenKey, and
/// a missing one answers @ref ERROR_ACCESS_DENIED.
VREG_DECL LSTATUS VRegHive_InternalCreateKey(
    _In_ VRegHive* pHive,
    _Inout_ VRegHiveInternalNode* pParent,
    _In_reads_(cPath) const VRegInternalWStringView* pPath,
    SIZE_T cPath,
    _In_ LPCWSTR pszClassName,
    DWORD dwOptions,
    REGSAM samDesired,
    _Out_ VRegHiveInternalOpenResult* pResult,
    _Out_ LPDWORD pdwDisposition
);

/// Deletes the last component of the path; a key with subkeys answers
/// @ref ERROR_ACCESS_DENIED, as native does.
VREG_DECL LSTATUS VRegHive_InternalDeleteKey(
    _In_ VRegHive* pHive,
    _Inout_ VRegHiveInternalNode* pParent,
    _In_reads_(cPath) const VRegInternalWStringView* pPath,
    SIZE_T cPath
);

/// Deletes every subkey and value of @p pNode, leaving the node itself.
VREG_DECL LSTATUS VRegHive_InternalDeleteTree(
    _In_ VRegHive* pHive,
    _Inout_ VRegHiveInternalNode* pNode
);

/// Reads value @p pszName of @p pNode. @p pcbData receives the size of the
/// data in bytes; @p pData receives the data itself only when it is not
/// null, so a null @p pData copies nothing.
VREG_DECL LSTATUS VRegHive_InternalQueryValue(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pNode,
    _In_ LPCWSTR pszName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VRegInternalBuffer* pData
);

VREG_DECL LSTATUS VRegHive_InternalSetValue(
    _In_ VRegHive* pHive,
    _Inout_ VRegHiveInternalNode* pNode,
    _In_ LPCWSTR pszName,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData
);

VREG_DECL LSTATUS VRegHive_InternalDeleteValue(
    _In_ VRegHive *pHive,
    _Inout_ VRegHiveInternalNode* pNode,
    _In_ LPCWSTR pszName
);

VREG_DECL LSTATUS VRegHive_InternalEnumKey(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pNode,
    DWORD dwIndex,
    _Inout_ VRegInternalWString* pName,
    _Inout_ VRegInternalWString* pClassName,
    _Out_ FILETIME* pLastWrite
);

/// Reads the value of @p pNode at @p dwIndex. @p pcbData receives the size
/// of the data in bytes; @p pData receives the data itself only when it
/// is not null, so a null @p pData copies nothing.
VREG_DECL LSTATUS VRegHive_InternalEnumValue(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pNode,
    DWORD dwIndex,
    _Inout_ VRegInternalWString* pName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VRegInternalBuffer* pData
);

/// Fills @p pInfo, whose @ref VRegInternalKeyInfo::className is the caller's
/// string.
VREG_DECL LSTATUS VRegHive_InternalQueryInfo(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pNode,
    _Inout_ VRegInternalKeyInfo* pInfo
);

/// What `RegFlushKey` on @p pNode needs from the hive: success, or
/// @ref ERROR_KEY_DELETED.
VREG_DECL LSTATUS VRegHive_InternalCheckKey(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pNode
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
 *  registry below an overlay hive is not.
 */
VREG_DECL LSTATUS VRegHive_InternalWatch(
    _In_ VRegHive* pHive,
    _In_ VRegHiveInternalNode* pNode,
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
VREG_DECL LSTATUS VRegHive_InternalSnapshotUpper(
    _In_ VRegHive* pHive,
    _In_ VRegInternalUpperVisitFunction* pfnVisit,
    _In_opt_ void* pContext
);

// ==========================================================================
// Registries
// ==========================================================================

/// Applies `X` to the name of every wide `Ex` form and every function with
/// one form only: the wide surface of @ref VRegVtbl, and the fields
/// of @ref VRegInternalNativeFunctions.
#define VREG_INTERNAL_REGISTRY_VIRTUALS(X)                                     \
    X(RegOpenKeyExW)                                                           \
    X(RegCreateKeyExW)                                                         \
    X(RegCloseKey)                                                             \
    X(RegQueryValueExW)                                                        \
    X(RegGetValueW)                                                            \
    X(RegSetValueExW)                                                          \
    X(RegDeleteValueW)                                                         \
    X(RegLoadMUIStringW)                                                       \
    X(RegDeleteKeyExW)                                                         \
    X(RegDeleteTreeW)                                                          \
    X(RegFlushKey)                                                             \
    X(RegEnumKeyExW)                                                           \
    X(RegEnumValueW)                                                           \
    X(RegQueryInfoKeyW)                                                        \
    X(RegNotifyChangeKeyValue)

/// Applies `X` to the name of every other form in @ref VRegVtbl: the
/// ANSI forms, the pre-`Ex` forms and the conveniences Win32 builds from the
/// wide forms. The hooks emulate these too, but no registry
/// calls them, so their trampolines stay with the hooks.
#define VREG_INTERNAL_REGISTRY_FORMS(X)                                        \
    X(RegOpenKeyA)                                                             \
    X(RegOpenKeyW)                                                             \
    X(RegOpenKeyExA)                                                           \
    X(RegCreateKeyA)                                                           \
    X(RegCreateKeyW)                                                           \
    X(RegCreateKeyExA)                                                         \
    X(RegQueryValueA)                                                          \
    X(RegQueryValueW)                                                          \
    X(RegQueryValueExA)                                                        \
    X(RegGetValueA)                                                            \
    X(RegQueryMultipleValuesA)                                                 \
    X(RegQueryMultipleValuesW)                                                 \
    X(RegSetValueA)                                                            \
    X(RegSetValueW)                                                            \
    X(RegSetValueExA)                                                          \
    X(RegSetKeyValueA)                                                         \
    X(RegSetKeyValueW)                                                         \
    X(RegDeleteValueA)                                                         \
    X(RegDeleteKeyValueA)                                                      \
    X(RegDeleteKeyValueW)                                                      \
    X(RegLoadMUIStringA)                                                       \
    X(RegDeleteKeyA)                                                           \
    X(RegDeleteKeyW)                                                           \
    X(RegDeleteKeyExA)                                                         \
    X(RegDeleteTreeA)                                                          \
    X(RegEnumKeyA)                                                             \
    X(RegEnumKeyW)                                                             \
    X(RegEnumKeyExA)                                                           \
    X(RegEnumValueA)                                                           \
    X(RegQueryInfoKeyA)

/// Both lists: every function of @ref VRegVtbl, wide `Ex` forms and others.
#define VREG_INTERNAL_REGISTRY_FUNCTIONS(X)                                    \
    VREG_INTERNAL_REGISTRY_VIRTUALS(X)                                         \
    VREG_INTERNAL_REGISTRY_FORMS(X)

/// The kinds of @ref VReg: one @ref VReg_CreateCustom made over a table, and
/// the two VReg implements itself.
typedef enum
{
    VREG_INTERNAL_KIND_CUSTOM,
    VREG_INTERNAL_KIND_NATIVE,
    VREG_INTERNAL_KIND_VIRTUAL,
} VRegInternalKind;

/// @ref NtQueryKey, the one `ntdll` function a virtual registry uses to find
/// the native path of a real key.
typedef LONG NTAPI VRegInternalNtQueryKeyFunction(
    HANDLE hKey,
    ULONG keyInformationClass,
    PVOID pKeyInformation,
    ULONG cbKeyInformation,
    PULONG pcbResult
);

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

    LSTATUS (WINAPI* RegLoadMUIStringW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
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

    LSTATUS (WINAPI* RegFlushKey)(
        _In_ HKEY hKey
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

    LSTATUS (WINAPI* RegNotifyChangeKeyValue)(
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    );
} VRegInternalNativeFunctions;

/// The live table of a registry @ref VReg_CreateNative made: once the
/// hooks are committed it holds the trampolines, and the registry is the one
/// below them. An entry replaced before anything uses the registry is the
/// one it calls.
VREG_DECL _Ret_notnull_ VRegInternalNativeFunctions* VReg_InternalGetNativeFunctions(
    _In_ VReg* pRegistry
);

/// A registry: its kind and what the kind needs.
struct VReg
{
    VRegInternalKind kind;

    /// The member of @ref kind.
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
        /// @ref VReg_InternalGetNativeFunctions.
        VRegInternalNativeFunctions native;

        /// @ref VREG_INTERNAL_KIND_VIRTUAL.
        struct
        {
            /// Null for a registry held entirely in memory.
            VReg* pBase;

            VRegInternalNtQueryKeyFunction* pfnNtQueryKey;
            struct VRegInternalMountTable* volatile pMounts;
            SRWLOCK mountLock;
        } virtual_;
    } implementation;
};

// ==========================================================================
// The virtual registry
// ==========================================================================

// The slots of a registry of VREG_INTERNAL_KIND_VIRTUAL, which the flat
// functions reach through its table as they reach a custom registry's.
VREG_DECL VRegOpenKeyExWFunction VReg_InternalVirtualRegOpenKeyExW;
VREG_DECL VRegCreateKeyExWFunction VReg_InternalVirtualRegCreateKeyExW;
VREG_DECL VRegCloseKeyFunction VReg_InternalVirtualRegCloseKey;
VREG_DECL VRegQueryValueExWFunction VReg_InternalVirtualRegQueryValueExW;
VREG_DECL VRegGetValueWFunction VReg_InternalVirtualRegGetValueW;
VREG_DECL VRegSetValueExWFunction VReg_InternalVirtualRegSetValueExW;
VREG_DECL VRegDeleteValueWFunction VReg_InternalVirtualRegDeleteValueW;
VREG_DECL VRegLoadMUIStringWFunction VReg_InternalVirtualRegLoadMUIStringW;
VREG_DECL VRegDeleteKeyExWFunction VReg_InternalVirtualRegDeleteKeyExW;
VREG_DECL VRegDeleteTreeWFunction VReg_InternalVirtualRegDeleteTreeW;
VREG_DECL VRegFlushKeyFunction VReg_InternalVirtualRegFlushKey;
VREG_DECL VRegEnumKeyExWFunction VReg_InternalVirtualRegEnumKeyExW;
VREG_DECL VRegEnumValueWFunction VReg_InternalVirtualRegEnumValueW;
VREG_DECL VRegQueryInfoKeyWFunction VReg_InternalVirtualRegQueryInfoKeyW;
VREG_DECL VRegNotifyChangeKeyValueFunction VReg_InternalVirtualRegNotifyChangeKeyValue;

/// Frees the mount tables; @ref VReg_Destroy frees the registry.
VREG_DECL void WINAPI VReg_InternalVirtualDestroy(
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
VREG_DECL LSTATUS VRegInternalWStringView_SplitPath(
    VRegInternalWStringView path,
    _Inout_ VRegInternalArray* pComponents
);

// ==========================================================================
// Status
// ==========================================================================

/// A status a failing path answers, which the analyzer cannot prove nonzero
/// when it was stored by another function.
_Post_satisfies_(return != ERROR_SUCCESS)
VREG_DECL LSTATUS VReg_InternalFailureStatus(LSTATUS status);

// ==========================================================================
// Scripts
// ==========================================================================

/// The storage behind one @ref VRegOperation of a script.
typedef struct
{
    VRegInternalWString path;
    VRegInternalWString name;
    VRegInternalBuffer data;
} VRegScriptInternalEntry;

struct VRegScript
{
    /// `VRegOperation`, pointing into @ref entries.
    VRegInternalArray operations;

    /// `VRegScriptInternalEntry`, one per operation.
    VRegInternalArray entries;
};

/// A predefined root a `.reg` file can name: exports write its long name,
/// imports accept either.
typedef struct
{
    LPCWSTR pszLong;
    LPCWSTR pszShort;
    HKEY hKey;
} VRegScriptInternalRootKey;

/// The long name of a root a `.reg` file can name; null for any other handle.
VREG_DECL _Ret_maybenull_ LPCWSTR VRegScript_InternalGetRootName(
    _In_opt_ HKEY hRoot
);

/// The root named @p name, long or short form, ignoring case.
VREG_DECL BOOL VRegInternalWStringView_FindRoot(
    VRegInternalWStringView name,
    _Out_ PHKEY phRoot
);

VREG_DECL LSTATUS VRegScript_InternalCreate(
    _Outptr_ VRegScript** ppScript
);

/// Appends an operation, copying its path, name and data into the script.
VREG_DECL LSTATUS VRegScript_InternalAppend(
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
VREG_DECL LSTATUS VRegScript_InternalDecodeFile(
    _In_reads_bytes_(cbFile) const void* pFile,
    SIZE_T cbFile,
    _Inout_ VRegInternalWString* pText,
    _Inout_opt_ VRegParseError* pError
);

/// The bytes of @p pText in @p format, with a BOM for Unicode. Answers
/// @ref ERROR_NO_UNICODE_TRANSLATION when the format's code page cannot hold
/// it.
VREG_DECL LSTATUS VRegScript_InternalEncodeFile(
    _In_ const VRegInternalWString* pText,
    VRegFormat format,
    _Inout_ VRegInternalBuffer* pFile
);

/// Reads and writes whole files; Win32 errors answer as is.
VREG_DECL LSTATUS VRegScript_InternalReadWholeFile(
    _In_ LPCWSTR pszPath,
    _Inout_ VRegInternalBuffer* pFile
);

VREG_DECL LSTATUS VRegScript_InternalWriteWholeFile(
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

VREG_DECL void VReg_InternalAssertFailed(void)
{
    if (IsDebuggerPresent())
        __debugbreak();
}

/// Null for @p cb of zero or when memory ran out.
static _Ret_maybenull_ void* VReg_InternalAlloc(
    const SIZE_T cb
)
{
    if (cb == 0)
        return NULL;

    return VREG_MALLOC(cb);
}

VREG_DECL _Ret_maybenull_ void* VReg_InternalAllocZeroed(
    const SIZE_T cb
)
{
    void* const p = VReg_InternalAlloc(cb);
    if (p)
        ZeroMemory(p, cb);

    return p;
}

VREG_DECL _Ret_maybenull_ void* VReg_InternalReAlloc(
    _In_opt_ void* const p,
    const SIZE_T cb
)
{
    if (!p)
        return VReg_InternalAlloc(cb);
    if (cb == 0)
    {
        VREG_FREE(p);
        return NULL;
    }

    return VREG_REALLOC(p, cb);
}

VREG_DECL void VReg_InternalFree(
    _In_opt_ void* const p
)
{
    if (p)
        VREG_FREE(p);
}

// ==========================================================================
// ANSI conversion
// ==========================================================================

static SIZE_T VReg_InternalAnsiLength(_In_ const LPCSTR psz)
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
static LSTATUS VReg_InternalWideFromMultiByte(
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
    if (!VRegInternalWString_Resize(pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cchWide = MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, NULL, 0);
    if (cchWide <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VRegInternalWString_Resize(pText, (SIZE_T)cchWide))
        return ERROR_OUTOFMEMORY;

    MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, pText->psz, cchWide);
    return ERROR_SUCCESS;
}

/// Narrows @p cch UTF-16 units into @p pText in @p codePage with the
/// @ref WideCharToMultiByte flags @p dwFlags, reporting a default character
/// through @p pfUsedDefault. Fails as @ref VReg_InternalWideFromMultiByte does.
static LSTATUS VReg_InternalMultiByteFromWide(
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
    if (!VRegInternalBuffer_Resize(pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cbText = WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, NULL, 0, NULL, pfUsedDefault);
    if (cbText <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VRegInternalBuffer_Resize(pText, (SIZE_T)cbText))
        return ERROR_OUTOFMEMORY;

    WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, (char*)pText->pb, cbText, NULL, pfUsedDefault);
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VReg_InternalWideFromAnsi(
    _In_reads_(cch) const LPCCH pch,
    const SIZE_T cch,
    _Inout_ VRegInternalWString* const pText
)
{
    return VReg_InternalWideFromMultiByte(CP_ACP, 0, pch, cch, pText);
}

VREG_DECL LSTATUS VReg_InternalAnsiFromWide(
    _In_reads_(cch) const LPCWCH pch,
    const SIZE_T cch,
    _Inout_ VRegInternalBuffer* const pText
)
{
    return VReg_InternalMultiByteFromWide(CP_ACP, 0, NULL, pch, cch, pText);
}

VREG_DECL LSTATUS VRegInternalWideArg_Init(
    _In_opt_ const LPCSTR psz,
    _Out_ VRegInternalWideArg* const pArg
)
{
    ZeroMemory(pArg, sizeof(*pArg));
    pArg->isNull = psz == NULL;
    if (!psz)
        return ERROR_SUCCESS;

    return VReg_InternalWideFromAnsi(psz, VReg_InternalAnsiLength(psz), &pArg->text);
}

VREG_DECL _Ret_maybenull_ LPCWSTR VRegInternalWideArg_GetText(
    _In_ const VRegInternalWideArg* const pArg
)
{
    return pArg->isNull ? NULL : VRegInternalWString_GetText(&pArg->text);
}

VREG_DECL _Ret_maybenull_ LPWSTR VRegInternalWideArg_GetMutableText(
    _In_ VRegInternalWideArg* const pArg
)
{
    if (pArg->isNull)
        return NULL;

    // An unallocated empty string has no storage to hand out.
    if (!pArg->text.psz && !VRegInternalWString_Reserve(&pArg->text, 0))
        return NULL;

    return pArg->text.psz;
}

VREG_DECL void VRegInternalWideArg_Free(_Inout_ VRegInternalWideArg* const pArg)
{
    VRegInternalWString_Free(&pArg->text);
}

// ==========================================================================
// Buffers
// ==========================================================================

/// The capacity to grow to for @p needed elements: at least double the old
/// one, so repeated appends stay linear.
static SIZE_T VReg_InternalGrowCapacity(
    const SIZE_T capacity,
    const SIZE_T needed
)
{
    SIZE_T grown = capacity < 8 ? 8 : capacity * 2;
    return grown < needed ? needed : grown;
}

VREG_DECL DWORD VReg_InternalMinDword(const DWORD a, const DWORD b)
{
    return a < b ? a : b;
}

VREG_DECL DWORD VReg_InternalMaxDword(const DWORD a, const DWORD b)
{
    return a > b ? a : b;
}

static SIZE_T VReg_InternalMinSize(const SIZE_T a, const SIZE_T b)
{
    return a < b ? a : b;
}

VREG_DECL VRegInternalWStringView VReg_InternalViewFromText(
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

VREG_DECL BOOL VRegInternalBuffer_Resize(
    _Inout_ VRegInternalBuffer* const pBuffer,
    const SIZE_T cb
)
{
    if (cb > pBuffer->cbCapacity)
    {
        const SIZE_T cbGrown = VReg_InternalGrowCapacity(pBuffer->cbCapacity, cb);
        BYTE* const pb = (BYTE*)VReg_InternalReAlloc(pBuffer->pb, cbGrown);
        if (!pb)
            return FALSE;

        pBuffer->pb = pb;
        pBuffer->cbCapacity = cbGrown;
    }

    pBuffer->cb = cb;
    return TRUE;
}

VREG_DECL BOOL VRegInternalBuffer_Assign(
    _Inout_ VRegInternalBuffer* const pBuffer,
    _In_reads_bytes_opt_(cb) const void* const p,
    const SIZE_T cb
)
{
    if (!VRegInternalBuffer_Resize(pBuffer, cb))
        return FALSE;

    if (cb != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(p != NULL);
        CopyMemory(pBuffer->pb, p, cb);
    }

    return TRUE;
}

VREG_DECL BOOL VRegInternalBuffer_Append(
    _Inout_ VRegInternalBuffer* const pBuffer,
    _In_reads_bytes_(cb) const void* const p,
    const SIZE_T cb
)
{
    const SIZE_T cbOld = pBuffer->cb;
    if (!VRegInternalBuffer_Resize(pBuffer, cbOld + cb))
        return FALSE;

    if (cb != 0)
        CopyMemory(pBuffer->pb + cbOld, p, cb);

    return TRUE;
}

VREG_DECL void VRegInternalBuffer_Free(
    _Inout_ VRegInternalBuffer* const pBuffer
)
{
    VReg_InternalFree(pBuffer->pb);
    pBuffer->pb = NULL;
    pBuffer->cb = 0;
    pBuffer->cbCapacity = 0;
}

VREG_DECL _Ret_notnull_ LPCWSTR VRegInternalWString_GetText(
    _In_ const VRegInternalWString* const pString
)
{
    return pString->psz ? pString->psz : L"";
}

VREG_DECL VRegInternalWStringView VRegInternalWString_GetView(
    _In_ const VRegInternalWString* const pString
)
{
    VRegInternalWStringView view;
    view.pch = VRegInternalWString_GetText(pString);
    view.cch = pString->cch;
    return view;
}

VREG_DECL BOOL VRegInternalWString_Reserve(
    _Inout_ VRegInternalWString* const pString,
    const SIZE_T cch
)
{
    LPWSTR psz;
    if (cch < pString->cchCapacity)
        return TRUE;

    psz = (wchar_t*)VReg_InternalReAlloc(pString->psz, (cch + 1) * sizeof(wchar_t));
    if (!psz)
        return FALSE;

    if (!pString->psz)
        psz[0] = L'\0';

    pString->psz = psz;
    pString->cchCapacity = cch + 1;
    return TRUE;
}

VREG_DECL BOOL VRegInternalWString_Resize(
    _Inout_ VRegInternalWString* const pString,
    const SIZE_T cch
)
{
    if (cch >= pString->cchCapacity)
    {
        const SIZE_T cchGrown = VReg_InternalGrowCapacity(pString->cchCapacity, cch + 1) - 1;
        if (!VRegInternalWString_Reserve(pString, cchGrown))
            return FALSE;
    }

    if (pString->psz)
        pString->psz[cch] = L'\0';

    pString->cch = cch;
    return TRUE;
}

VREG_DECL BOOL VRegInternalWString_Assign(
    _Inout_ VRegInternalWString* const pString,
    _In_reads_opt_(cch) const LPCWCH pch,
    const SIZE_T cch
)
{
    if (!VRegInternalWString_Resize(pString, cch))
        return FALSE;

    if (cch != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(pch != NULL);
        CopyMemory(pString->psz, pch, cch * sizeof(wchar_t));
    }

    return TRUE;
}

VREG_DECL BOOL VRegInternalWString_Append(
    _Inout_ VRegInternalWString* const pString,
    _In_reads_(cch) const LPCWCH pch,
    const SIZE_T cch
)
{
    const SIZE_T cchOld = pString->cch;
    if (!VRegInternalWString_Resize(pString, cchOld + cch))
        return FALSE;

    if (cch != 0)
        CopyMemory(pString->psz + cchOld, pch, cch * sizeof(wchar_t));

    return TRUE;
}

VREG_DECL BOOL VRegInternalWString_AppendChar(
    _Inout_ VRegInternalWString* const pString,
    const wchar_t ch
)
{
    return VRegInternalWString_Append(pString, &ch, 1);
}

/// Appends a key path component, after a backslash unless @p pPath is empty.
static BOOL VRegInternalWString_AppendPathComponent(
    _Inout_ VRegInternalWString* const pPath,
    _In_reads_(cch) const LPCWCH pch,
    const SIZE_T cch
)
{
    if (pPath->cch != 0 && !VRegInternalWString_AppendChar(pPath, L'\\'))
        return FALSE;

    return VRegInternalWString_Append(pPath, pch, cch);
}

VREG_DECL void VRegInternalWString_Truncate(
    _Inout_ VRegInternalWString* const pString,
    const SIZE_T cch
)
{
    VREG_ASSERT(cch <= pString->cch);
    pString->cch = cch;
    if (pString->psz)
        pString->psz[cch] = L'\0';
}

VREG_DECL void VRegInternalWString_Free(
    _Inout_ VRegInternalWString* const pString
)
{
    VReg_InternalFree(pString->psz);
    pString->psz = NULL;
    pString->cch = 0;
    pString->cchCapacity = 0;
}

VREG_DECL BOOL VRegInternalArray_Reserve(
    _Inout_ VRegInternalArray* const pArray,
    const SIZE_T cbElement,
    const SIZE_T capacity
)
{
    void* p;
    if (capacity <= pArray->capacity)
        return TRUE;

    p = VReg_InternalReAlloc(pArray->p, capacity * cbElement);
    if (!p)
        return FALSE;

    pArray->p = p;
    pArray->capacity = capacity;
    return TRUE;
}

VREG_DECL BOOL VRegInternalArray_Insert(
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
        const SIZE_T grown = VReg_InternalGrowCapacity(pArray->capacity, pArray->count + 1);
        if (!VRegInternalArray_Reserve(pArray, cbElement, grown))
            return FALSE;
    }

    pbAt = (BYTE*)pArray->p + index * cbElement;
    MoveMemory(pbAt + cbElement, pbAt, (pArray->count - index) * cbElement);
    CopyMemory(pbAt, pElement, cbElement);
    ++pArray->count;
    return TRUE;
}

VREG_DECL BOOL VRegInternalArray_Append(
    _Inout_ VRegInternalArray* const pArray,
    const SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    return VRegInternalArray_Insert(pArray, cbElement, pArray->count, pElement);
}

VREG_DECL void VRegInternalArray_RemoveAt(
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

VREG_DECL void VRegInternalArray_Free(
    _Inout_ VRegInternalArray* const pArray
)
{
    VReg_InternalFree(pArray->p);
    pArray->p = NULL;
    pArray->count = 0;
    pArray->capacity = 0;
}

VREG_DECL VRegInternalWStringView VRegInternalArray_WStringViewAt(
    _In_ const VRegInternalArray* const pArray,
    const SIZE_T index
)
{
    return VRegInternalWString_GetView(&VREG_INTERNAL_AT(pArray, VRegInternalWString, index));
}

static BOOL VRegInternalArray_InternalInsertWString(
    _Inout_ VRegInternalArray* const pArray,
    const SIZE_T index,
    const VRegInternalWStringView text
)
{
    VRegInternalWString copy = {0, 0, NULL};
    if (!VRegInternalWString_Assign(&copy, text.pch, text.cch))
        return FALSE;
    if (!VRegInternalArray_Insert(pArray, sizeof(copy), index, &copy))
    {
        VRegInternalWString_Free(&copy);
        return FALSE;
    }

    return TRUE;
}

VREG_DECL BOOL VRegInternalArray_AppendWString(
    _Inout_ VRegInternalArray* const pArray,
    const VRegInternalWStringView text
)
{
    return VRegInternalArray_InternalInsertWString(pArray, pArray->count, text);
}

VREG_DECL void VRegInternalArray_FreeWStrings(
    _Inout_ VRegInternalArray* const pArray
)
{
    SIZE_T i;
    for (i = 0; i < pArray->count; ++i)
        VRegInternalWString_Free(&VREG_INTERNAL_AT(pArray, VRegInternalWString, i));

    VRegInternalArray_Free(pArray);
}

/// Answers whether @p name is in @p pList, and stores the index it is at or
/// would be inserted at in @p pIndex.
static BOOL VRegInternalArray_InternalFindWStringSorted(
    _In_ const VRegInternalArray* const pList,
    const VRegInternalWStringView name,
    _Out_ SIZE_T* const pIndex
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pList->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const int order = VRegInternalWStringView_CompareNames(VRegInternalArray_WStringViewAt(pList, mid), name);
        if (order == 0)
        {
            *pIndex = mid;
            return TRUE;
        }
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    *pIndex = lo;
    return FALSE;
}

VREG_DECL BOOL VRegInternalArray_InsertWStringSorted(
    _Inout_ VRegInternalArray* const pList,
    const VRegInternalWStringView name
)
{
    SIZE_T index;
    if (VRegInternalArray_InternalFindWStringSorted(pList, name, &index))
        return TRUE;

    return VRegInternalArray_InternalInsertWString(pList, index, name);
}

VREG_DECL BOOL VRegInternalArray_ContainsWStringSorted(
    _In_ const VRegInternalArray* const pList,
    const VRegInternalWStringView name
)
{
    SIZE_T index;
    return VRegInternalArray_InternalFindWStringSorted(pList, name, &index);
}

// ==========================================================================
// Export tables
// ==========================================================================

/// Makes every module linking VReg import advapi32, so the loader maps it
/// before VReg runs and resolution finds it without loading anything. The
/// address is taken in a function body: an initializer would need the import
/// thunk's address, which C does not guarantee to be constant.
static void* volatile g_VReg_pfnImportAdvapi32;

/// Ordinal comparison of two export names: negative, zero or positive.
static int VReg_InternalCompareExportNames(
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

VREG_DECL _Ret_maybenull_ void* VReg_InternalFindExport(
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
        const int order = VReg_InternalCompareExportNames(pszName, (LPCSTR)(pBase + pNames[middle]));
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

VREG_DECL _Ret_maybenull_ void* VReg_InternalFindRegistryExport(
    _In_ const LPCSTR pszName
)
{
    void* const pFunction = VReg_InternalFindExport(GetModuleHandleW(L"KernelBase.dll"), pszName);
    g_VReg_pfnImportAdvapi32 = (void*)&RegOpenKeyW;
    if (pFunction)
        return pFunction;

    return VReg_InternalFindExport(GetModuleHandleW(L"advapi32.dll"), pszName);
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

/// The table a native registry derives every form from.
static const VRegVtbl g_VRegVtbl_Empty = {0};

/// The table of a virtual registry: its wide forms, from which every other
/// form is derived.
static const VRegVtbl g_VRegVtbl_Virtual = {
    .Destroy = &VReg_InternalVirtualDestroy,
#define VREG_FILL_SLOT(name) .name = &VReg_InternalVirtual##name,
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_FILL_SLOT)
#undef VREG_FILL_SLOT
};

/// The slots a flat function calls when they are filled: a custom registry's
/// own, the virtual registry's wide forms, and none for the native registry,
/// whose table the flat wide functions call directly.
static _Ret_notnull_ const VRegVtbl* VReg_InternalGetVtbl(
    _In_ const VReg* const pRegistry
)
{
    switch (pRegistry->kind)
    {
    case VREG_INTERNAL_KIND_CUSTOM:
        return pRegistry->implementation.custom.lpVtbl;

    case VREG_INTERNAL_KIND_VIRTUAL:
        return &g_VRegVtbl_Virtual;

    default:
        return &g_VRegVtbl_Empty;
    }
}

typedef struct
{
    DWORD cbValue;
    DWORD dwType;
    DWORD ibValue;
} VRegInternalPlacedValue;

static DWORD VReg_InternalAlignToDword(const DWORD cb)
{
    return (cb + 3) & ~(DWORD)3;
}

/// Copies @p cb bytes; a null @p pSource is legal only for none.
static void VReg_InternalCopyBytes(
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
static BOOL VRegInternalBuffer_CopyAnsiName(
    _In_ const VRegInternalBuffer* const pText,
    _Out_writes_opt_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    if (!pszBuffer)
        return FALSE;

    if (pText->cb < cchBuffer)
    {
        VReg_InternalCopyBytes(pszBuffer, pText->pb, pText->cb);
        pszBuffer[pText->cb] = '\0';
        return TRUE;
    }

    if (pText->cb == cchBuffer && cchBuffer > 0)
    {
        VReg_InternalCopyBytes(pszBuffer, pText->pb, pText->cb - 1);
        pszBuffer[pText->cb - 1] = '\0';
    }
    return FALSE;
}

/// Like @ref VRegInternalBuffer_CopyAnsiName for a class buffer, which native fills with as
/// much of the text as fits (and a terminator) whenever it has room for one.
static BOOL VRegInternalBuffer_CopyAnsiClass(
    _In_ const VRegInternalBuffer* const pText,
    _Out_writes_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    SIZE_T cchCopied;
    if (cchBuffer == 0)
        return FALSE;

    cchCopied = VReg_InternalMinSize(pText->cb, cchBuffer - 1);
    VReg_InternalCopyBytes(pszBuffer, pText->pb, cchCopied);
    pszBuffer[cchCopied] = '\0';
    return pText->cb < cchBuffer;
}

VREG_DECL LSTATUS WINAPI VReg_RegOpenKeyExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegOpenKeyExA)
        return VReg_InternalGetVtbl(pRegistry)->RegOpenKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    HKEY hPrevious;
    LSTATUS status = VRegInternalWideArg_Init(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    status = VReg_RegOpenKeyExW(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&subKey),
        ulOptions,
        samDesired,
        phkResult
    );
    // The native ANSI form leaves the result alone for an invalid handle; the
    // wide form nulls it.
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VRegInternalWideArg_Free(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegCreateKeyExA(
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
    if (VReg_InternalGetVtbl(pRegistry)->RegCreateKeyExA)
        return VReg_InternalGetVtbl(pRegistry)->RegCreateKeyExA(
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
    LSTATUS status = VRegInternalWideArg_Init(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternalWideArg_Init(lpClass, &className);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VReg_RegCreateKeyExW(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&subKey),
        Reserved,
        VRegInternalWideArg_GetMutableText(&className),
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition
    );
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VRegInternalWideArg_Free(&className);
    VRegInternalWideArg_Free(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegQueryValueExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegQueryValueExA)
        return VReg_InternalGetVtbl(pRegistry)->RegQueryValueExA(
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

    status = VRegInternalWideArg_Init(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!lpData && !lpcbData)
    {
        status = VReg_RegQueryValueExW(
            pRegistry,
            hKey,
            VRegInternalWideArg_GetText(&valueName),
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
    sized = VReg_RegQueryValueExW(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&valueName),
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

    if (!VReg_InternalIsStringType(dwType))
    {
        status = VReg_RegQueryValueExW(
            pRegistry,
            hKey,
            VRegInternalWideArg_GetText(&valueName),
            NULL,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    status = VReg_InternalQueryValueOwned(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&valueName),
        &dwType,
        &value
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_InternalAnsiFromWide((LPCWCH)value.pb, value.cb / sizeof(wchar_t), &text);
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
                VReg_InternalCopyBytes(lpData, value.pb, value.cb);
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
    VRegInternalBuffer_Free(&text);
    VRegInternalBuffer_Free(&value);
    VRegInternalWideArg_Free(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegSetValueExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegSetValueExA)
        return VReg_InternalGetVtbl(pRegistry)->RegSetValueExA(
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
    LSTATUS status = VRegInternalWideArg_Init(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg_InternalIsStringType(dwType) || !lpData)
    {
        status = VReg_RegSetValueExW(
            pRegistry,
            hKey,
            VRegInternalWideArg_GetText(&valueName),
            Reserved,
            dwType,
            lpData,
            cbData
        );
        goto done;
    }

    status = VReg_InternalWideFromAnsi((LPCCH)lpData, cbData, &text);
    if (status != ERROR_SUCCESS)
        goto done;

    // Native reads the byte after unterminated data and keeps it as the
    // terminator when it is zero, which it nearly always is. VReg always
    // terminates, so no byte past cbData is ever read; reading it, as native
    // does, can fault at the end of a page.
    if (text.cch > 0 && text.psz[text.cch - 1] != L'\0')
    {
        if (!VRegInternalWString_AppendChar(&text, L'\0'))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    status = VReg_RegSetValueExW(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&valueName),
        Reserved,
        dwType,
        (const BYTE*)VRegInternalWString_GetText(&text),
        (DWORD)(text.cch * sizeof(wchar_t))
    );

done:
    VRegInternalWString_Free(&text);
    VRegInternalWideArg_Free(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegDeleteValueA)
        return VReg_InternalGetVtbl(pRegistry)->RegDeleteValueA(
            pRegistry,
            hKey,
            lpValueName
        );

    VRegInternalWideArg valueName = {FALSE, {0, 0, NULL}};
    LSTATUS status = VRegInternalWideArg_Init(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_RegDeleteValueW(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&valueName)
    );

done:
    VRegInternalWideArg_Free(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteKeyExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyExA)
        return VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    LSTATUS status = VRegInternalWideArg_Init(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VReg_RegDeleteKeyExW(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&subKey),
        samDesired,
        Reserved
    );

done:
    VRegInternalWideArg_Free(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteTreeA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegDeleteTreeA)
        return VReg_InternalGetVtbl(pRegistry)->RegDeleteTreeA(
            pRegistry,
            hKey,
            lpSubKey
        );

    VRegInternalWideArg subKey = {FALSE, {0, 0, NULL}};
    LSTATUS status = VRegInternalWideArg_Init(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_RegDeleteTreeW(
        pRegistry,
        hKey,
        VRegInternalWideArg_GetText(&subKey)
    );

done:
    VRegInternalWideArg_Free(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegEnumKeyExA(
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
    if (VReg_InternalGetVtbl(pRegistry)->RegEnumKeyExA)
        return VReg_InternalGetVtbl(pRegistry)->RegEnumKeyExA(
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
    LSTATUS status;

    if (!lpcchName || (lpClass && !lpcchClass))
        return ERROR_INVALID_PARAMETER;

    ZeroMemory(&ftLastWrite, sizeof(ftLastWrite));
    status = VReg_InternalEnumKeyOwned(
        pRegistry,
        hKey,
        dwIndex,
        &name,
        (lpClass || lpcchClass) ? &className : NULL,
        lpReserved,
        &ftLastWrite
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpftLastWriteTime)
        *lpftLastWriteTime = ftLastWrite;

    status = VReg_InternalAnsiFromWide(VRegInternalWString_GetText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VRegInternalBuffer_CopyAnsiName(&nameA, lpName, *lpcchName))
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
        status = VReg_InternalAnsiFromWide(VRegInternalWString_GetText(&className), className.cch, &classA);
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VRegInternalBuffer_CopyAnsiClass(&classA, lpClass, *lpcchClass))
        {
            status = ERROR_MORE_DATA;
            goto done;
        }
        *lpcchClass = (DWORD)classA.cb;
    }

    status = ERROR_SUCCESS;

done:
    VRegInternalBuffer_Free(&classA);
    VRegInternalBuffer_Free(&nameA);
    VRegInternalWString_Free(&className);
    VRegInternalWString_Free(&name);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegEnumValueA(
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
    if (VReg_InternalGetVtbl(pRegistry)->RegEnumValueA)
        return VReg_InternalGetVtbl(pRegistry)->RegEnumValueA(
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

    status = VReg_InternalEnumValueOwned(
        pRegistry,
        hKey,
        dwIndex,
        &name,
        &dwType,
        lpcbData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_InternalAnsiFromWide(VRegInternalWString_GetText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    isString = VReg_InternalIsStringType(dwType);
    cbValue = (DWORD)value.cb;
    if (lpcbData && isString)
    {
        status = VReg_InternalAnsiFromWide((LPCWCH)value.pb, value.cb / sizeof(wchar_t), &text);
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

    nameFits = VRegInternalBuffer_CopyAnsiName(&nameA, lpValueName, *lpcchValueName);
    if (lpData)
    {
        if (isString)
        {
            if (value.cb <= cbBuffer)
                VReg_InternalCopyBytes(lpData, value.pb, value.cb);
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
            VReg_InternalCopyBytes(lpData, value.pb, cbValue);
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
    VRegInternalBuffer_Free(&text);
    VRegInternalBuffer_Free(&nameA);
    VRegInternalBuffer_Free(&value);
    VRegInternalWString_Free(&name);
    return status;
}

// Native answers success without writing lpClass when only the class length is
// asked for, which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_RegQueryInfoKeyA(
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
    if (VReg_InternalGetVtbl(pRegistry)->RegQueryInfoKeyA)
        return VReg_InternalGetVtbl(pRegistry)->RegQueryInfoKeyA(
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
        return VReg_RegQueryInfoKeyW(
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

    status = VReg_InternalQueryInfoKeyOwned(
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

    status = VReg_InternalAnsiFromWide(VRegInternalWString_GetText(&className), className.cch, &classA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (wantClassLength)
    {
        *lpcchClass = (DWORD)classA.cb;
        status = ERROR_SUCCESS;
        goto done;
    }
    if (!VRegInternalBuffer_CopyAnsiClass(&classA, lpClass, *lpcchClass))
    {
        *lpcchClass -= 1;
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchClass = (DWORD)classA.cb;
    status = ERROR_SUCCESS;

done:
    VRegInternalBuffer_Free(&classA);
    VRegInternalWString_Free(&className);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegLoadMUIStringA(
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
    if (VReg_InternalGetVtbl(pRegistry)->RegLoadMUIStringA)
        return VReg_InternalGetVtbl(pRegistry)->RegLoadMUIStringA(
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
VREG_DECL LSTATUS WINAPI VReg_RegGetValueA(
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
    if (VReg_InternalGetVtbl(pRegistry)->RegGetValueA)
        return VReg_InternalGetVtbl(pRegistry)->RegGetValueA(
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

    status = VRegInternalWideArg_Init(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternalWideArg_Init(lpValue, &value);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only string values are converted; anything else, and every failure to
    // find the value, is what the wide form answers.
    dwFlagsStored =
        (dwFlags & VREG_INTERNAL_RRF_WOW_SUBKEYS)
        | RRF_RT_ANY
        | RRF_NOEXPAND;
    dwStoredType = 0;
    statusStoredType = VReg_RegGetValueW(
        pRegistry,
        hkey,
        VRegInternalWideArg_GetText(&subKey),
        VRegInternalWideArg_GetText(&value),
        dwFlagsStored,
        &dwStoredType,
        NULL,
        NULL
    );
    if (statusStoredType != ERROR_SUCCESS || !VReg_InternalIsStringType(dwStoredType))
        goto forward;

    // A value whose type the flags reject is still read and reported, with the
    // failure status.
    dwFlagsTyped = dwFlags & ~RRF_ZEROONFAILURE;
    dwFlagsAnyType =
        (dwFlags & (VREG_INTERNAL_RRF_WOW_SUBKEYS | RRF_NOEXPAND))
        | RRF_RT_ANY;
    dwType = 0;
    cbQuery = 0;
    statusSized = VReg_RegGetValueW(
        pRegistry,
        hkey,
        VRegInternalWideArg_GetText(&subKey),
        VRegInternalWideArg_GetText(&value),
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
        VReg_RegGetValueW(
            pRegistry,
            hkey,
            VRegInternalWideArg_GetText(&subKey),
            VRegInternalWideArg_GetText(&value),
            dwFlagsAnyType,
            &dwType,
            NULL,
            &cbQuery
        );
    }

    status = VReg_InternalGetValueOwned(
        pRegistry,
        hkey,
        VRegInternalWideArg_GetText(&subKey),
        VRegInternalWideArg_GetText(&value),
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
        status = VReg_InternalGetValueOwned(
            pRegistry,
            hkey,
            VRegInternalWideArg_GetText(&subKey),
            VRegInternalWideArg_GetText(&value),
            dwFlagsStored,
            &dwReadType,
            &stored
        );
        if (status != ERROR_SUCCESS)
            goto done;

        VReg_RegGetValueW(
            pRegistry,
            hkey,
            VRegInternalWideArg_GetText(&subKey),
            VRegInternalWideArg_GetText(&value),
            dwFlagsStored,
            &dwReadType,
            NULL,
            &cbStoredQuery
        );
        cbStoredRaw = cbStoredQuery - VReg_InternalMinDword(cbStoredQuery, 2);
    }

    status = VReg_InternalAnsiFromWide((LPCWCH)data.pb, data.cb / sizeof(wchar_t), &text);
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
        status = VReg_InternalAnsiFromWide(
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
        cbReported = VReg_InternalMaxDword(cbRawText, cbExpanded) + (cbExpanded > cbRawText ? 1 : 0);
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
            status = VReg_InternalAnsiFromWide(
                (LPCWCH)stored.pb,
                stored.cb / sizeof(wchar_t),
                &storedText
            );
            if (status != ERROR_SUCCESS)
                goto done;

            if (storedText.cb > 0 && storedText.cb <= cbBuffer)
            {
                if (cbStoredRaw <= cbBuffer)
                    VReg_InternalCopyBytes(pvData, stored.pb, VReg_InternalMinSize(cbStoredRaw, stored.cb));
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
            VReg_InternalCopyBytes(pbData, stored.pb, VReg_InternalMinSize(cbStoredRaw, stored.cb));
    }
    else
    {
        cbStored = cbQuery - VReg_InternalMinDword(cbQuery, dwType == REG_MULTI_SZ ? 4 : 2);
        if (cbStored <= cbBuffer)
            VReg_InternalCopyBytes(pbData, data.pb, VReg_InternalMinSize(cbStored, data.cb));
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
    status = VReg_RegGetValueW(
        pRegistry,
        hkey,
        VRegInternalWideArg_GetText(&subKey),
        VRegInternalWideArg_GetText(&value),
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );

done:
    VRegInternalBuffer_Free(&storedText);
    VRegInternalBuffer_Free(&rawText);
    VRegInternalBuffer_Free(&text);
    VRegInternalBuffer_Free(&stored);
    VRegInternalBuffer_Free(&data);
    VRegInternalWideArg_Free(&value);
    VRegInternalWideArg_Free(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegOpenKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegOpenKeyA)
        return VReg_InternalGetVtbl(pRegistry)->RegOpenKeyA(
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
    return VReg_RegOpenKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegOpenKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegOpenKeyW)
        return VReg_InternalGetVtbl(pRegistry)->RegOpenKeyW(
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
    return VReg_RegOpenKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegCreateKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegCreateKeyA)
        return VReg_InternalGetVtbl(pRegistry)->RegCreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VReg_InternalIsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VReg_RegCreateKeyExA(
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

VREG_DECL LSTATUS WINAPI VReg_RegCreateKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegCreateKeyW)
        return VReg_InternalGetVtbl(pRegistry)->RegCreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VReg_InternalIsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VReg_RegCreateKeyExW(
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

static void VReg_InternalCloseUnless(
    _In_ VReg* const pRegistry,
    _In_opt_ const HKEY hOpened,
    _In_opt_ const HKEY hKept
)
{
    if (hOpened && hOpened != hKept)
        VReg_RegCloseKey(
            pRegistry,
            hOpened
        );
}

VREG_DECL LSTATUS WINAPI VReg_RegQueryValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegQueryValueA)
        return VReg_InternalGetVtbl(pRegistry)->RegQueryValueA(
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
        status = VReg_RegOpenKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_RegQueryValueExA(
        pRegistry,
        hSubKey,
        NULL,
        NULL,
        &dwType,
        (LPBYTE)lpData,
        (LPDWORD)lpcbData
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
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

VREG_DECL LSTATUS WINAPI VReg_RegQueryValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegQueryValueW)
        return VReg_InternalGetVtbl(pRegistry)->RegQueryValueW(
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
        status = VReg_RegOpenKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_RegQueryValueExW(
        pRegistry,
        hSubKey,
        NULL,
        NULL,
        &dwType,
        (LPBYTE)lpData,
        (LPDWORD)lpcbData
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
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

VREG_DECL LSTATUS WINAPI VReg_RegSetValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegSetValueA)
        return VReg_InternalGetVtbl(pRegistry)->RegSetValueA(
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
        status = VReg_RegCreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_RegSetValueExA(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)(VReg_InternalAnsiLength(lpData) + 1)
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegSetValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegSetValueW)
        return VReg_InternalGetVtbl(pRegistry)->RegSetValueW(
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
        status = VReg_RegCreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VReg_RegSetValueExW(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)((VReg_InternalViewFromText(lpData).cch + 1) * sizeof(wchar_t))
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyA)
        return VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyA(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VReg_RegDeleteKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyW)
        return VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyW(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VReg_RegDeleteKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegEnumKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegEnumKeyA)
        return VReg_InternalGetVtbl(pRegistry)->RegEnumKeyA(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VReg_RegEnumKeyExA(
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

VREG_DECL LSTATUS WINAPI VReg_RegEnumKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegEnumKeyW)
        return VReg_InternalGetVtbl(pRegistry)->RegEnumKeyW(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VReg_RegEnumKeyExW(
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
// the one right the call needs, not with the maximum allowed access the
// pre-Ex forms use. Only a caller whose access is limited to something else
// could tell.
VREG_DECL LSTATUS WINAPI VReg_RegSetKeyValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegSetKeyValueA)
        return VReg_InternalGetVtbl(pRegistry)->RegSetKeyValueA(
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
        status = VReg_RegCreateKeyExA(
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

    status = VReg_RegSetValueExA(
        pRegistry,
        hSubKey,
        lpValueName,
        0,
        dwType,
        (const BYTE*)lpData,
        cbData
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegSetKeyValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegSetKeyValueW)
        return VReg_InternalGetVtbl(pRegistry)->RegSetKeyValueW(
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
        status = VReg_RegCreateKeyExW(
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

    status = VReg_RegSetValueExW(
        pRegistry,
        hSubKey,
        lpValueName,
        0,
        dwType,
        (const BYTE*)lpData,
        cbData
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteKeyValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyValueA)
        return VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VReg_RegOpenKeyExA(
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

    status = VReg_RegDeleteValueA(
        pRegistry,
        hSubKey,
        lpValueName
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteKeyValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyValueW)
        return VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VReg_RegOpenKeyExW(
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

    status = VReg_RegDeleteValueW(
        pRegistry,
        hSubKey,
        lpValueName
    );
    VReg_InternalCloseUnless(pRegistry, hSubKey, hKey);
    return status;
}

/// Places the narrowed values like the wide form places the stored ones, but
/// leaves the gaps alone and rounds the total up. When they do not all fit,
/// native answers the wide form's total for the stored values.
// The SDK types lpValueBuf as a string, but it holds binary values.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_RegQueryMultipleValuesA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegQueryMultipleValuesA)
        return VReg_InternalGetVtbl(pRegistry)->RegQueryMultipleValuesA(
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
        pPlaced = (VRegInternalPlacedValue*)VReg_InternalAllocZeroed((SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VReg_RegQueryValueExA(
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

        pPlaced[i].ibValue = i == 0 ? 0 : VReg_InternalAlignToDword(cbEnd);
        cbEnd = pPlaced[i].ibValue + pPlaced[i].cbValue;
    }

    if (cbEnd > cbBuffer)
    {
        cbWideEnd = 0;
        for (i = 0; i < num_vals; ++i)
        {
            cbWide = 0;
            status = VRegInternalWideArg_Init(val_list[i].ve_valuename, &name);
            if (status == ERROR_SUCCESS)
            {
                status = VReg_RegQueryValueExW(
                    pRegistry,
                    hKey,
                    VRegInternalWideArg_GetText(&name),
                    NULL,
                    NULL,
                    NULL,
                    &cbWide
                );
            }
            VRegInternalWideArg_Free(&name);
            if (status != ERROR_SUCCESS)
                goto done;

            cbWideEnd = (i == 0 ? 0 : VReg_InternalAlignToDword(cbWideEnd)) + cbWide;
        }
        *ldwTotsize = cbWideEnd;
        status = ERROR_MORE_DATA;
        goto done;
    }

    for (i = 0; i < num_vals; ++i)
    {
        cbRead = pPlaced[i].cbValue;
        status = VReg_RegQueryValueExA(
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
    *ldwTotsize = VReg_InternalAlignToDword(cbEnd);
    status = ERROR_SUCCESS;

done:
    VReg_InternalFree(pPlaced);
    return status;
}

/// Native places each value on a @ref DWORD boundary, reads the ones that fit
/// the buffer in order, and fills the entries only when all of them did.
VREG_DECL LSTATUS WINAPI VReg_RegQueryMultipleValuesW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (VReg_InternalGetVtbl(pRegistry)->RegQueryMultipleValuesW)
        return VReg_InternalGetVtbl(pRegistry)->RegQueryMultipleValuesW(
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
        pPlaced = (VRegInternalPlacedValue*)VReg_InternalAllocZeroed((SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VReg_RegQueryValueExW(
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
        ibValue = i == 0 ? 0 : VReg_InternalAlignToDword(cbEnd);
        if (ibValue > cbEnd && cbEnd < cbBuffer)
            ZeroMemory(pbBuffer + cbEnd, VReg_InternalMinDword(ibValue, cbBuffer) - cbEnd);

        pPlaced[i].ibValue = ibValue;
        if (ibValue + cbValue <= cbBuffer)
        {
            cbRead = cbValue;
            status = VReg_RegQueryValueExW(
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
    VReg_InternalFree(pPlaced);
    return status;
}

// ==========================================================================
// Handles
// ==========================================================================

// Measured on Windows 11 26200, x86 and x64 (a probe opening keys through
// plain, WOW64, HKCR, HKCU, HKU, HKCC, `RegOpenCurrentUser`,
// `RegOpenUserClassesRoot`, `RegConnectRegistry`, transacted and app-hive
// routes, plus 2000 repeated opens; `test/test_handle_probe.cpp` keeps it as
// a test, run under Wine in CI): bit 0 was never set on a real handle. Bit 1
// is advapi32's tag for merged `HKEY_CLASSES_ROOT` handles, and the kernel
// ignores both low bits, so @ref NtQueryKey resolves tagged handles. An odd
// handle passed to the real API (a virtual handle leaked past a neutralized
// hook) answers @ref ERROR_INVALID_HANDLE from every function, `RegCloseKey`
// included, also under the strict-handle-check mitigation: kernelbase
// rejects it before the kernel, so it never aliases the real handle it would
// name with bit 0 dropped. Not covered: true remote `RegConnectRegistry`
// (connecting to the own machine name may short-circuit to local).

VREG_DECL BOOL WINAPI VReg_IsVirtualHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value != 0
        && value <= VREG_INTERNAL_VIRTUAL_HKEY_BASE
        && (value & 1) != 0;
}

VREG_DECL BOOL VReg_InternalIsPredefinedHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value >= (UINT_PTR)HKEY_CLASSES_ROOT
        && value <= (UINT_PTR)HKEY_CURRENT_USER_LOCAL_SETTINGS;
}

// ==========================================================================
// The handle space
// ==========================================================================

// The section is `Local\VReg-handles-<pid>`, pagefile-backed:
// @ref CreateFileMappingW is an atomic open-or-create that needs no loader
// lock, its pages come zero-filled, and the first copy claims `magic` with
// one compare-exchange, so nothing is published after the claim; a copy that
// finds another nonzero magic treats the namespace as unusable. A section
// rather than a named mutex, since the counter needs shared storage either
// way and the section needs no lock; blocks of
// @ref VREG_INTERNAL_HANDLE_BLOCK_SIZE rather than per-handle increments, for
// one cross-copy operation per block. A handle from another copy's block is
// foreign: every entry point treats it like a real handle and passes it to
// its base registry (in a hooked process, the trampoline: the next hooker,
// another copy or the real function), so it reaches the copy that issued it.
// A same-session process could pre-create the section; it can already open
// this process and do worse, so nothing defends against it.

typedef struct
{
    void* volatile targets[VREG_INTERNAL_HANDLE_BLOCK_SIZE];
} VRegInternalHandleBlock;

struct VRegInternalHandleDirectory
{
    VRegInternalHandleBlock* volatile blocks[VREG_INTERNAL_HANDLE_DIRECTORY_SIZE];
};

static VRegInternalHandleSpace g_VRegInternalHandleSpace_Process;
static INIT_ONCE g_VRegInternalHandleSpace_ProcessOnce = INIT_ONCE_STATIC_INIT;

VREG_DECL void VRegInternalHandleSpace_Init(
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
static SIZE_T VReg_InternalWriteDecimal(
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

static BOOL CALLBACK VReg_InternalInitProcessHandleSpace(
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
    VReg_InternalWriteDecimal(wszName + cchPrefix, sizeof(wszName) / sizeof(wszName[0]) - cchPrefix, GetCurrentProcessId());
    VRegInternalHandleSpace_Init(&g_VRegInternalHandleSpace_Process, wszName);
    return TRUE;
}

VREG_DECL _Ret_notnull_ VRegInternalHandleSpace* VReg_InternalGetProcessHandleSpace(
    void
)
{
    // Handles issued from it outlive every teardown, so the instance is never
    // destroyed.
    InitOnceExecuteOnce(&g_VRegInternalHandleSpace_ProcessOnce, VReg_InternalInitProcessHandleSpace, NULL, NULL);
    return &g_VRegInternalHandleSpace_Process;
}

VREG_DECL BOOL VRegInternalHandleSpace_IsUsable(
    _In_ const VRegInternalHandleSpace* const pSpace
)
{
    return pSpace->pShared != NULL;
}

// A slot holds null while unbound, which WritePointerRelease's annotations
// reject; the exchange accepts it, and its full barrier covers the release.
static void VReg_InternalStoreTarget(
    _Inout_ void* volatile* const pSlot,
    _In_opt_ void* const pTarget
)
{
    InterlockedExchangePointer(pSlot, pTarget);
}

static VRegInternalHandleBlock* VRegInternalHandleSpace_FindBlock(
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

static BOOL VRegInternalHandleSpace_AddBlock(
    _Inout_ VRegInternalHandleSpace* const pSpace,
    const UINT_PTR block
)
{
    VRegInternalHandleDirectory* volatile* const pDirectorySlot = &pSpace->directories[block / VREG_INTERNAL_HANDLE_DIRECTORY_SIZE];
    VRegInternalHandleDirectory* pDirectory = *pDirectorySlot;
    VRegInternalHandleBlock* pBlock;

    if (!pDirectory)
    {
        pDirectory = (VRegInternalHandleDirectory*)VReg_InternalAllocZeroed(sizeof(VRegInternalHandleDirectory));
        if (!pDirectory)
            return FALSE;

        WritePointerRelease((PVOID volatile*)pDirectorySlot, pDirectory);
    }

    pBlock = (VRegInternalHandleBlock*)VReg_InternalAllocZeroed(sizeof(VRegInternalHandleBlock));
    if (!pBlock)
        return FALSE;

    WritePointerRelease((PVOID volatile*)&pDirectory->blocks[block % VREG_INTERNAL_HANDLE_DIRECTORY_SIZE], pBlock);
    return TRUE;
}

VREG_DECL _Ret_maybenull_ HKEY VRegInternalHandleSpace_Allocate(
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
        if (block >= VREG_INTERNAL_HANDLE_BLOCK_COUNT || !VRegInternalHandleSpace_AddBlock(pSpace, block))
        {
            ReleaseSRWLockExclusive(&pSpace->lock);
            return NULL;
        }

        pSpace->nextIndex = block * VREG_INTERNAL_HANDLE_BLOCK_SIZE;
        pSpace->endIndex = pSpace->nextIndex + VREG_INTERNAL_HANDLE_BLOCK_SIZE;
    }

    index = pSpace->nextIndex++;
    pBlock = VRegInternalHandleSpace_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    VReg_InternalStoreTarget(&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE], pTarget);
    hKey = VReg_InternalVirtualHKeyFromIndex(index);

    ReleaseSRWLockExclusive(&pSpace->lock);
    return hKey;
}

VREG_DECL BOOL VRegInternalHandleSpace_Owns(
    _In_ const VRegInternalHandleSpace* const pSpace,
    _In_opt_ const HKEY hKey
)
{
    return
        VReg_IsVirtualHKey(hKey)
        && VRegInternalHandleSpace_FindBlock(pSpace, VReg_InternalIndexFromVirtualHKey(hKey) / VREG_INTERNAL_HANDLE_BLOCK_SIZE) != NULL;
}

VREG_DECL void VRegInternalHandleSpace_Release(
    _Inout_ VRegInternalHandleSpace* const pSpace,
    _In_opt_ const HKEY hKey
)
{
    UINT_PTR index;
    VRegInternalHandleBlock* pBlock;

    if (!VReg_IsVirtualHKey(hKey))
        return;

    index = VReg_InternalIndexFromVirtualHKey(hKey);
    pBlock = VRegInternalHandleSpace_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    if (pBlock)
        VReg_InternalStoreTarget(&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE], NULL);
}

VREG_DECL _Ret_maybenull_ void* VRegInternalHandleSpace_Find(
    _In_ const VRegInternalHandleSpace* const pSpace,
    _In_opt_ const HKEY hKey
)
{
    UINT_PTR index;
    const VRegInternalHandleBlock* pBlock;

    if (!VReg_IsVirtualHKey(hKey))
        return NULL;

    index = VReg_InternalIndexFromVirtualHKey(hKey);
    pBlock = VRegInternalHandleSpace_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    if (!pBlock)
        return NULL;

    return ReadPointerAcquire((PVOID volatile*)&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE]);
}

VREG_DECL _Ret_maybenull_ HKEY VReg_InternalVirtualHKeyFromIndex(
    const UINT_PTR index
)
{
    if (index >= VREG_INTERNAL_VIRTUAL_HKEY_COUNT)
        return NULL;

    return (HKEY)(VREG_INTERNAL_VIRTUAL_HKEY_BASE - (index << 1));
}

VREG_DECL UINT_PTR VReg_InternalIndexFromVirtualHKey(_In_opt_ const HKEY hKey)
{
    return (VREG_INTERNAL_VIRTUAL_HKEY_BASE - (UINT_PTR)hKey) >> 1;
}

// ==========================================================================
// Hives
// ==========================================================================

// One struct covers every kind: what varies is the lower behind the node
// tree, reached only through a @ref VRegHiveVtbl and its context (as
// `fopencookie` makes a `FILE` from functions and a cookie): nothing below
// for an in-memory hive, a registry subtree for an overlay ("Hive lowers
// over a registry"), the provider's slots for a custom hive.
//
// The nodes are the hive's records of its keys. Identity is the path,
// compared with @ref VRegInternalWStringView_CompareNames, so a virtual
// handle maps to a record as to any node: one handle per record, issued
// when the key is first opened or created. `RegCloseKey` on it is a no-op,
// a deleted node keeps its slot and answers @ref ERROR_KEY_DELETED, and
// nodes are freed only with the hive. That keeps reads lock-free and
// handles stable, at the cost of never reclaiming deleted nodes and of two
// opens answering the same value; per-open refcounted handles would be
// closer to Win32 identity but need a lock on every open and close.
//
// Records are added lazily under the write lock, never while a slot runs:
// look up, unlock, call the slot, lock again and insert, finding what
// another thread inserted meanwhile. A custom hive opens and creates one
// name per slot call, so every record holds the token of its own key; the
// overlay hive opens the whole rest of a path in one lower call. A provider
// that answers the same token for two paths gets two records; nothing
// compares tokens.
//
// Writes follow the kind: in-memory and overlay hives keep them in their
// own layer (copy-up), where tombstones and opaque keys hide lower entries;
// a custom hive sends them to the slots (write-through) and then updates
// the records: a created key gets its token, a deleted one is marked
// deleted, the watches fire. Enumeration and `RegQueryInfoKeyW` merge both
// layers, stepping over shadowed names and shifting lower indices by the
// upper count; a failed lower query never becomes a smaller successful
// view, and the class of a key the upper layer did not create comes from
// the lower key, also after the key was written to.
//
// The access mask of an open and the frozen hand-out never reach a
// provider. The overlay hive opens its lower through
// @ref VRegHive::pfnOpenLower, which takes the access and can hand the
// opened key out as a redirect ("this key is answered by registry R at key
// K"): a frozen overlay hive answers an untouched key that way, and the
// virtual registry forwards a call on a redirect through one helper that
// closes the key after. The redirect stays private: its constraint, that
// later calls on the handle go to the mounting registry's base, is a sharp
// edge to publish.
//
// Slots hand results over through copying sinks (@ref VRegHiveResult), so a
// computed value can live on the provider's stack and nothing it hands over
// needs a lifetime rule, at the cost of one copy per read. Caller buffers
// with @ref ERROR_MORE_DATA would bring back the rules the table exists to
// hide, and pointers valid until the next call would need every concurrent
// caller to reason about that call. A null `DeleteTree` is derived in the
// order native `RegDeleteTreeW` was measured to use (Windows 11 26300):
// subkeys in enumeration order, each subtree before its key, then the
// values; it stops at the first failure, as native makes no all-or-nothing
// promise. The names are read before the deletes, so a provider whose
// deletes do not shrink its enumeration cannot loop it.
//
// A custom hive's delta is empty: its records are all transparent, so the
// snapshot of the upper layer has nothing to write.

/// The kinds of @ref VRegHive. An in-memory or overlay hive keeps writes in
/// its own layer over the lower (copy-up); a custom hive writes through to
/// the provider's slots.
typedef enum
{
    VREG_INTERNAL_HIVE_IN_MEMORY,
    VREG_INTERNAL_HIVE_OVERLAY,
    VREG_INTERNAL_HIVE_CUSTOM,
} VRegHiveInternalKind;

struct VRegHive
{
    /// Null for a hive held entirely in memory.
    const VRegHiveVtbl* pLowerVtbl;
    void* pvLowerContext;

    /// The lower's open with an access mask and redirects; set only for a
    /// lower over a registry, which the hive then opens through it.
    VRegHive_InternalOpenLowerFunction* pfnOpenLower;

    VRegHiveInternalKind kind;

    SRWLOCK lock;
    volatile LONG isFrozen;

    /// Fixed for the life of the hive.
    VRegHiveInternalNode* pRoot;

    /// `VRegHiveInternalNode*`: nodes are created here and freed only with the
    /// hive; a deleted node stays allocated (marked deleted) so its handle
    /// keeps answering @ref ERROR_KEY_DELETED. Destroying the hive unbinds
    /// every handle it issued.
    VRegInternalArray nodes;
};

static const VRegInternalWStringView VREG_INTERNAL_NO_NAME = {NULL, 0};

VREG_DECL int VRegInternalWStringView_CompareNames(
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
static BOOL VReg_InternalIsExistingStatus(const LSTATUS status)
{
    return status == ERROR_SUCCESS || status == ERROR_MORE_DATA || status == ERROR_ACCESS_DENIED;
}

// --- Locking ---

// A writable hive takes its lock shared for readers and exclusive for
// writers, over in-memory node work only: it is never held across a call
// into the lower (some native opens load DLLs), and merged enumeration
// snapshots the upper names under the lock, releases it, then walks the
// lower layer. No holder can wait on the loader lock, so taking it inside a
// detour body under a foreign loader lock is safe. Every open inside a
// writable mount answers a virtual handle, creating a transparent node
// lazily: a real handle handed out earlier would miss a later copy-up.
// Frozen, a built-in hive is read without the lock, and an open of a
// lower-only key answers the lower handle itself, at no cost on keys the
// mount does not touch.

VREG_DECL BOOL VRegHive_InternalIsFrozen(_In_ VRegHive* const pHive)
{
    return ReadAcquire(&pHive->isFrozen) != 0;
}

/// Whether readers skip the lock: a frozen hive whose tree is all its own.
/// A custom hive keeps locking once frozen, since it still adds records.
static BOOL VRegHive_InternalIsLockFree(_In_ VRegHive* const pHive)
{
    return VRegHive_InternalIsFrozen(pHive) && pHive->kind != VREG_INTERNAL_HIVE_CUSTOM;
}

/// Takes the lock shared, or nothing once frozen; answers whether it did.
static BOOL VRegHive_InternalLockRead(_In_ VRegHive* const pHive)
{
    if (VRegHive_InternalIsLockFree(pHive))
        return FALSE;

    AcquireSRWLockShared(&pHive->lock);
    return TRUE;
}

static void VRegHive_InternalUnlockRead(
    _In_ VRegHive* const pHive,
    const BOOL isLocked
)
{
    if (isLocked)
        ReleaseSRWLockShared(&pHive->lock);
}

/// Takes the lock exclusive, or nothing once frozen; answers whether it did.
/// A check of @ref VRegHive_InternalIsFrozen made before can be stale, since a
/// writer may wait here while @ref VRegHive_Freeze runs, so only this decides a
/// write.
static BOOL VRegHive_InternalLockWrite(_In_ VRegHive* const pHive)
{
    AcquireSRWLockExclusive(&pHive->lock);
    if (!VRegHive_InternalIsFrozen(pHive))
        return TRUE;

    ReleaseSRWLockExclusive(&pHive->lock);
    return FALSE;
}

static void VRegHive_InternalUnlockWrite(_In_ VRegHive* const pHive)
{
    ReleaseSRWLockExclusive(&pHive->lock);
}

/// Takes the lock exclusively to add or update records, which a custom hive
/// does frozen or not; a frozen built-in hive takes none and answers FALSE.
/// Released with @ref VRegHive_InternalUnlockWrite.
static BOOL VRegHive_InternalLockRecords(_In_ VRegHive* const pHive)
{
    if (VRegHive_InternalIsLockFree(pHive))
        return FALSE;

    AcquireSRWLockExclusive(&pHive->lock);
    if (!VRegHive_InternalIsLockFree(pHive))
        return TRUE;

    ReleaseSRWLockExclusive(&pHive->lock);
    return FALSE;
}

// --- Nodes, values and watches ---

// A watch's event is signaled with @ref SetEvent while the write lock is
// held: neither it nor @ref CloseHandle can wait on the loader lock or on a
// lock a hive caller could hold, and deferring them would need an
// allocation after the write was made. A synchronous
// `RegNotifyChangeKeyValue` waits on a private event, as native does, so a
// watcher thread written for the native registry keeps working; answering
// at once would spin such a loop, and @ref ERROR_NOT_SUPPORTED would end it.
// A watch that never fires keeps its duplicate handle until the hive is
// destroyed or frozen.

static void VRegHiveInternalValue_Free(
    _Inout_ VRegHiveInternalValue* const pValue
)
{
    VRegInternalWString_Free(&pValue->name);
    VRegInternalBuffer_Free(&pValue->data);
}

static void VRegHiveInternalNode_ClearValues(
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
        VRegHiveInternalValue_Free(&VREG_INTERNAL_AT(&pNode->values, VRegHiveInternalValue, i));

    pNode->values.count = 0;
}

static void VRegHiveInternalNode_Free(_Inout_ VRegHiveInternalNode* const pNode)
{
    VRegHiveInternalNode_ClearValues(pNode);
    VRegInternalArray_Free(&pNode->values);
    VRegInternalArray_Free(&pNode->children);
    VRegInternalArray_Free(&pNode->watches);
    VRegInternalWString_Free(&pNode->name);
    VRegInternalWString_Free(&pNode->className);
    VReg_InternalFree(pNode);
}

/// Stamps @p pNode as written now.
static void VRegHive_InternalTouch(_Inout_ VRegHiveInternalNode* const pNode)
{
    GetSystemTimeAsFileTime(&pNode->lastWrite);
}

/// Makes @p pNode and its transparent ancestors upper keys, as a write to a
/// key below them needs. The root stays as it is: it is the mount point.
static void VRegHive_InternalMaterialize(
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    VRegHiveInternalNode* pCurrent;
    for (pCurrent = pNode; pCurrent->pParent && pCurrent->isTransparent; pCurrent = pCurrent->pParent)
        pCurrent->isTransparent = FALSE;
}

static void VRegHive_InternalDropWatches(
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    SIZE_T i;
    for (i = 0; i < pNode->watches.count; ++i)
    {
        // The array's count bounds its initialized elements, which the
        // analyzer cannot see past the reallocation.
#pragma warning(suppress: 6001)
        CloseHandle(VREG_INTERNAL_AT(&pNode->watches, VRegHiveInternalChangeWatch, i).hEvent);
    }

    pNode->watches.count = 0;
}

/// Signals and drops the watches of @p pNode that @p dwFilter selects;
/// @p isInherited limits them to those watching their subtree.
static void VRegHive_InternalFire(
    _Inout_ VRegHiveInternalNode* const pNode,
    const DWORD dwFilter,
    const BOOL isInherited
)
{
    SIZE_T i = 0;
    while (i < pNode->watches.count)
    {
        const VRegHiveInternalChangeWatch* const pWatch = &VREG_INTERNAL_AT(&pNode->watches, VRegHiveInternalChangeWatch, i);
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
        VRegInternalArray_RemoveAt(&pNode->watches, sizeof(*pWatch), i);
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
static void VRegHive_InternalNotify(
    _Inout_ VRegHiveInternalNode* const pNode,
    const DWORD dwFilter
)
{
    VRegHiveInternalNode* pAncestor;
    VRegHive_InternalFire(pNode, dwFilter, FALSE);
    for (pAncestor = pNode->pParent; pAncestor; pAncestor = pAncestor->pParent)
        VRegHive_InternalFire(pAncestor, dwFilter, TRUE);
}

/// The child of @p pParent named @p name, or null; @p pIndex, if given,
/// receives where it is or where it would be inserted.
static _Ret_maybenull_ VRegHiveInternalNode* VRegHive_InternalFindChild(
    _In_ const VRegHiveInternalNode* const pParent,
    const VRegInternalWStringView name,
    _Out_opt_ SIZE_T* const pIndex
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pParent->children.count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const VRegHiveInternalNode* const pMid = VREG_INTERNAL_AT(&pParent->children, VRegHiveInternalNode*, mid);
        if (VRegInternalWStringView_CompareNames(VRegInternalWString_GetView(&pMid->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (pIndex)
        *pIndex = lo;
    if (lo < pParent->children.count)
    {
        VRegHiveInternalNode* const pChild = VREG_INTERNAL_AT(&pParent->children, VRegHiveInternalNode*, lo);
        if (VRegInternalWStringView_CompareNames(VRegInternalWString_GetView(&pChild->name), name) == 0)
            return pChild;
    }

    return NULL;
}

static _Ret_maybenull_ VRegHiveInternalValue* VRegHive_InternalFindValue(
    _In_ VRegHiveInternalNode* const pNode,
    const VRegInternalWStringView name
)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
    {
        VRegHiveInternalValue* const pValue = &VREG_INTERNAL_AT(&pNode->values, VRegHiveInternalValue, i);
        if (VRegInternalWStringView_CompareNames(VRegInternalWString_GetView(&pValue->name), name) == 0)
            return pValue;
    }

    return NULL;
}

/// A new child of @p pParent, with its handle. The write lock must be held.
static LSTATUS VRegHive_InternalAddChild(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pParent,
    const VRegInternalWStringView name,
    const VRegInternalWStringView className,
    const BOOL isVolatile,
    _Outptr_ VRegHiveInternalNode** const ppChild
)
{
    SIZE_T index = 0;
    VRegHiveInternalNode* const pNode = (VRegHiveInternalNode*)VReg_InternalAllocZeroed(sizeof(VRegHiveInternalNode));
    if (!pNode)
        return ERROR_OUTOFMEMORY;

    pNode->pHive = pHive;
    pNode->pParent = pParent;
    pNode->isVolatile = isVolatile;
    VRegHive_InternalTouch(pNode);
    VRegHive_InternalFindChild(pParent, name, &index);

    // Every allocation happens before the handle is bound, so a failure
    // leaves no handle pointing at a freed node.
    if (
        !VRegInternalWString_Assign(&pNode->name, name.pch, name.cch)
        || !VRegInternalWString_Assign(&pNode->className, className.pch, className.cch)
        || !VRegInternalArray_Reserve(&pHive->nodes, sizeof(pNode), pHive->nodes.count + 1)
        || !VRegInternalArray_Reserve(&pParent->children, sizeof(pNode), pParent->children.count + 1)
    )
    {
        VRegHiveInternalNode_Free(pNode);
        return ERROR_OUTOFMEMORY;
    }

    pNode->hKey = VRegInternalHandleSpace_Allocate(VReg_InternalGetProcessHandleSpace(), pNode);
    if (!pNode->hKey)
    {
        VRegHiveInternalNode_Free(pNode);
        return ERROR_OUTOFMEMORY;
    }

    VRegInternalArray_Insert(&pParent->children, sizeof(pNode), index, &pNode);
    VRegInternalArray_Append(&pHive->nodes, sizeof(pNode), &pNode);
    VRegHive_InternalTouch(pParent);
    *ppChild = pNode;
    return ERROR_SUCCESS;
}

/// Children of a deleted key answer as deleted too.
static void VRegHive_InternalMarkDeleted(
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    SIZE_T i;
    pNode->isDeleted = TRUE;
    VRegHive_InternalFire(pNode, ~(DWORD)0, FALSE);
    for (i = 0; i < pNode->children.count; ++i)
        VRegHive_InternalMarkDeleted(VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, i));
}

/// Marks @p pNode and everything below it deleted, unlinks it from its
/// parent and fires the watches on it and below it, and the name watches
/// on its parent. The write lock must be held.
static void VRegHive_InternalRemove(_Inout_ VRegHiveInternalNode* const pNode)
{
    if (pNode->pParent)
    {
        SIZE_T index = 0;
        VRegHive_InternalFindChild(pNode->pParent, VRegInternalWString_GetView(&pNode->name), &index);
        VRegInternalArray_RemoveAt(&pNode->pParent->children, sizeof(pNode), index);
        VRegHive_InternalTouch(pNode->pParent);
    }

    VRegHive_InternalMarkDeleted(pNode);
    if (pNode->pParent)
        VRegHive_InternalNotify(pNode->pParent, REG_NOTIFY_CHANGE_NAME);
}

// --- The lower registry ---

/// Appends @p cNames path components to @p pPath, separated by backslashes.
static BOOL VRegHive_InternalJoinPath(
    _Inout_ VRegInternalWString* const pPath,
    _In_reads_(cNames) const VRegInternalWStringView* const pNames,
    const SIZE_T cNames
)
{
    SIZE_T i;
    for (i = 0; i < cNames; ++i)
    {
        if (!VRegInternalWString_AppendPathComponent(pPath, pNames[i].pch, pNames[i].cch))
            return FALSE;
    }

    return TRUE;
}

static BOOL VRegHive_InternalAppendNodePath(
    _Inout_ VRegInternalWString* const pPath,
    _In_ const VRegHiveInternalNode* const pNode
)
{
    if (!pNode->pParent)
        return TRUE;
    if (!VRegHive_InternalAppendNodePath(pPath, pNode->pParent))
        return FALSE;

    return VRegInternalWString_AppendPathComponent(pPath, VRegInternalWString_GetText(&pNode->name), pNode->name.cch);
}

/// Whether @p pNode's lower key shows through: the hive has a lower and no
/// key at or above @p pNode is opaque. Read under the lock.
static BOOL VRegHive_InternalIsVisible(
    _In_ VRegHive* const pHive,
    _In_ const VRegHiveInternalNode* const pNode
)
{
    const VRegHiveInternalNode* pCurrent;
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
static void VRegHiveResult_InternalInit(_Out_ VRegHiveResult* const pResult)
{
    ZeroMemory(pResult, sizeof(*pResult));
}

/// Opens the lower key at @p cPath components below @p pParent: through the
/// registry lower's own open, with @p samDesired and (when @p isRedirect) the
/// key handed out in @p pRedirect, or through the provider's `OpenKey`, which
/// answers @p ppToken.
static LSTATUS VRegHive_InternalOpenLower(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pParent,
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
    if (!VRegHive_InternalJoinPath(&subKey, pPath, cPath))
    {
        VRegInternalWString_Free(&subKey);
        return ERROR_OUTOFMEMORY;
    }

    if (pHive->pfnOpenLower)
        status = pHive->pfnOpenLower(pHive, pParent, VRegInternalWString_GetText(&subKey), samDesired, isRedirect, pRedirect);
    else
        status = pHive->pLowerVtbl->OpenKey(pHive, pParent, VRegInternalWString_GetText(&subKey), ppToken);

    VRegInternalWString_Free(&subKey);
    return status;
}

/// Whether the lower has a key at @p cPath components below @p pNode, opened
/// with @p samDesired, as the status of that open.
static LSTATUS VRegHive_InternalProbeLower(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired
)
{
    VRegInternalRedirect redirect;
    void* pToken;
    return VRegHive_InternalOpenLower(pHive, pNode, pPath, cPath, samDesired, FALSE, &redirect, &pToken);
}

/// What `RegQueryInfoKeyW` counts for the lower key of @p pNode, derived by
/// enumerating its keys and values once; its own class stays empty and its
/// last write zero.
static LSTATUS VRegHive_InternalDeriveInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
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
        VRegInternalWString_Truncate(&className, 0);
        VRegHiveResult_InternalInit(&result);
        result.pName = &name;
        result.pClassName = &className;
        status = pHive->pLowerVtbl->EnumKey(pHive, pNode, i, &result);
        if (status != ERROR_SUCCESS)
            break;

        ++pInfo->cSubKeys;
        pInfo->cchMaxSubKey = VReg_InternalMaxDword(pInfo->cchMaxSubKey, (DWORD)name.cch);
        pInfo->cchMaxClass = VReg_InternalMaxDword(pInfo->cchMaxClass, (DWORD)className.cch);
    }
    if (status != ERROR_NO_MORE_ITEMS)
        goto done;

    for (i = 0;; ++i)
    {
        cbData = 0;
        VRegHiveResult_InternalInit(&result);
        result.pName = &name;
        result.pdwType = &dwType;
        result.pcbData = &cbData;
        status = pHive->pLowerVtbl->EnumValue(pHive, pNode, i, &result);
        if (status != ERROR_SUCCESS)
            break;

        ++pInfo->cValues;
        pInfo->cchMaxValueName = VReg_InternalMaxDword(pInfo->cchMaxValueName, (DWORD)name.cch);
        pInfo->cbMaxValue = VReg_InternalMaxDword(pInfo->cbMaxValue, cbData);
    }
    if (status == ERROR_NO_MORE_ITEMS)
        status = ERROR_SUCCESS;

done:
    VRegInternalWString_Free(&name);
    VRegInternalWString_Free(&className);
    return status;
}

/// What `RegQueryInfoKeyW` reports for the lower key of @p pNode: the
/// lower's `QueryInfo`, or derived when it has none.
static LSTATUS VRegHive_InternalQueryLowerInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _Inout_ VRegInternalKeyInfo* const pInfo
)
{
    VRegHiveResult result;
    if (!pHive->pLowerVtbl->QueryInfo)
        return VRegHive_InternalDeriveInfo(pHive, pNode, pInfo);

    VRegHiveResult_InternalInit(&result);
    result.pInfo = pInfo;
    return pHive->pLowerVtbl->QueryInfo(pHive, pNode, &result);
}

/// The class of the lower key of @p pNode; empty when there is none.
static LSTATUS VRegHive_InternalQueryLowerClass(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _Inout_ VRegInternalWString* const pClassName
)
{
    VRegInternalKeyInfo info;
    LSTATUS status;

    ZeroMemory(&info, sizeof(info));
    status = VRegHive_InternalQueryLowerInfo(pHive, pNode, &info);
    if (status == ERROR_SUCCESS && !VRegInternalWString_Assign(pClassName, VRegInternalWString_GetText(&info.className), info.className.cch))
        status = ERROR_OUTOFMEMORY;
    else if (status == ERROR_FILE_NOT_FOUND)
        status = VRegInternalWString_Resize(pClassName, 0) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;

    VRegInternalWString_Free(&info.className);
    return status;
}

// --- Creation and destruction ---

/// A hive over @p pLowerVtbl (null for one held entirely in memory), which
/// owns @p pvLowerContext from now on, failure included.
static LSTATUS VRegHive_InternalCreate(
    _In_opt_ const VRegHiveVtbl* const pLowerVtbl,
    _In_opt_ void* const pvLowerContext,
    const VRegHiveInternalKind kind,
    _Outptr_ VRegHive** const ppHive
)
{
    VRegHiveInternalNode* pRoot;
    VRegHive* const pHive = (VRegHive*)VReg_InternalAllocZeroed(sizeof(VRegHive));
    *ppHive = NULL;
    if (!pHive)
        return ERROR_OUTOFMEMORY;

    pHive->kind = kind;
    InitializeSRWLock(&pHive->lock);

    pRoot = (VRegHiveInternalNode*)VReg_InternalAllocZeroed(sizeof(VRegHiveInternalNode));
    if (!pRoot || !VRegInternalArray_Append(&pHive->nodes, sizeof(pRoot), &pRoot))
    {
        VReg_InternalFree(pRoot);
        VRegHive_Destroy(pHive);
        return ERROR_OUTOFMEMORY;
    }

    pRoot->pHive = pHive;
    pRoot->hKey = VRegInternalHandleSpace_Allocate(VReg_InternalGetProcessHandleSpace(), pRoot);
    if (!pRoot->hKey)
    {
        VRegHive_Destroy(pHive);
        return ERROR_OUTOFMEMORY;
    }

    VRegHive_InternalTouch(pRoot);
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

VREG_DECL LSTATUS WINAPI VRegHive_CreateInMemory(_Outptr_ VRegHive** const ppHive)
{
    return VRegHive_InternalCreate(NULL, NULL, VREG_INTERNAL_HIVE_IN_MEMORY, ppHive);
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

    return VRegHive_InternalCreate(pVtbl, pvContext, VREG_INTERNAL_HIVE_CUSTOM, ppHive);
}

VREG_DECL _Ret_maybenull_ void* WINAPI VRegHive_GetContext(
    _In_ VRegHive* const pHive
)
{
    return pHive->kind == VREG_INTERNAL_HIVE_CUSTOM ? pHive->pvLowerContext : NULL;
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
    return VRegInternalWString_GetText(&pKey->name);
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
    const VRegHiveInternalNode* pNode = pKey;
    LPCWCH pchEnd = pszPath + VReg_InternalViewFromText(pszPath).cch;
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
        if (VRegInternalWStringView_CompareNames(VRegInternalWString_GetView(&pNode->name), component) != 0)
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
    if (pResult->pData && !VRegInternalBuffer_Assign(pResult->pData, pData, cbData))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegHiveResult_SetName(
    _In_ VRegHiveResult* const pResult,
    _In_ const LPCWSTR pszName
)
{
    const VRegInternalWStringView name = VReg_InternalViewFromText(pszName);
    if (pResult->pName && !VRegInternalWString_Assign(pResult->pName, name.pch, name.cch))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegHiveResult_SetClass(
    _In_ VRegHiveResult* const pResult,
    _In_ const LPCWSTR pszClassName
)
{
    const VRegInternalWStringView className = VReg_InternalViewFromText(pszClassName);
    VRegInternalWString* const pClassName = pResult->pInfo ? &pResult->pInfo->className : pResult->pClassName;
    if (pClassName && !VRegInternalWString_Assign(pClassName, className.pch, className.cch))
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
    return VRegInternalWStringView_CompareNames(VReg_InternalViewFromText(pszLeft), VReg_InternalViewFromText(pszRight));
}

VREG_DECL void WINAPI VRegHive_Destroy(_In_opt_ VRegHive* const pHive)
{
    SIZE_T i;
    if (!pHive)
        return;

    for (i = 0; i < pHive->nodes.count; ++i)
    {
        VRegHiveInternalNode* const pNode = VREG_INTERNAL_AT(&pHive->nodes, VRegHiveInternalNode*, i);
        VRegHive_InternalDropWatches(pNode);
        VRegInternalHandleSpace_Release(VReg_InternalGetProcessHandleSpace(), pNode->hKey);
        VRegHiveInternalNode_Free(pNode);
    }

    VRegInternalArray_Free(&pHive->nodes);
    if (pHive->pLowerVtbl && pHive->pLowerVtbl->Destroy)
        pHive->pLowerVtbl->Destroy(pHive);
    VReg_InternalFree(pHive);
}

VREG_DECL void WINAPI VRegHive_Freeze(_In_ VRegHive* const pHive)
{
    SIZE_T i;

    // Taking the lock once more waits out every writer still inside; readers
    // that see the flag afterwards skip the lock and see all their writes.
    AcquireSRWLockExclusive(&pHive->lock);
    WriteRelease(&pHive->isFrozen, 1);
    for (i = 0; pHive->kind != VREG_INTERNAL_HIVE_CUSTOM && i < pHive->nodes.count; ++i)
        VRegHive_InternalDropWatches(VREG_INTERNAL_AT(&pHive->nodes, VRegHiveInternalNode*, i));

    ReleaseSRWLockExclusive(&pHive->lock);
}

VREG_DECL _Ret_notnull_ VRegHiveInternalNode* VRegHive_InternalGetRoot(
    _In_ VRegHive* const pHive
)
{
    return pHive->pRoot;
}

VREG_DECL HKEY VRegHiveInternalNode_GetHandle(
    _In_ const VRegHiveInternalNode* const pNode
)
{
    return pNode->hKey;
}

VREG_DECL _Ret_maybenull_ VRegHiveInternalNode* VRegHive_InternalFindNode(
    _In_opt_ const HKEY hKey
)
{
    return (VRegHiveInternalNode*)VRegInternalHandleSpace_Find(VReg_InternalGetProcessHandleSpace(), hKey);
}

// --- Keys ---

/// Follows @p pPath through the upper layer from @p pParent as far as it
/// goes. @p pcMatched receives how many components were found and
/// @p ppCurrent the node they led to; when the path goes on from there,
/// @p pIsVisible says whether that node's lower key shows through.
static LSTATUS VRegHive_InternalWalkUpper(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _Outptr_ VRegHiveInternalNode** const ppCurrent,
    _Out_ SIZE_T* const pcMatched,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    VRegHiveInternalNode* pCurrent = pParent;
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
            VRegHiveInternalNode* const pChild = VRegHive_InternalFindChild(pCurrent, pPath[i], NULL);
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
            *pIsVisible = VRegHive_InternalIsVisible(pHive, pCurrent);
    }

    VRegHive_InternalUnlockRead(pHive, isLocked);
    *ppCurrent = pCurrent;
    *pcMatched = i;
    return status;
}

/// The record of the key @p name below @p pParent, added (lower-backed,
/// keeping @p pToken) unless another thread added it meanwhile.
static LSTATUS VRegHive_InternalAddRecord(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pParent,
    const VRegInternalWStringView name,
    _In_opt_ void* const pToken,
    _Outptr_ VRegHiveInternalNode** const ppRecord
)
{
    VRegHiveInternalNode* pRecord = NULL;
    LSTATUS status = ERROR_SUCCESS;

    *ppRecord = NULL;
    if (!VRegHive_InternalLockRecords(pHive))
        return ERROR_ACCESS_DENIED;

    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        pRecord = VRegHive_InternalFindChild(pParent, name, NULL);
        if (!pRecord)
        {
            status = VRegHive_InternalAddChild(pHive, pParent, name, VREG_INTERNAL_NO_NAME, FALSE, &pRecord);
            if (status == ERROR_SUCCESS)
            {
                pRecord->isTransparent = TRUE;
                pRecord->isLowerBacked = TRUE;
                pRecord->pToken = pToken;
            }
        }
    }

    VRegHive_InternalUnlockWrite(pHive);
    if (status == ERROR_SUCCESS)
        *ppRecord = pRecord;
    return status;
}

/// Opens a custom hive's key at @p cPath components below @p pCurrent one
/// key at a time, adding a record with its token for each, so a provider
/// finds every key it answered through the token it kept.
static LSTATUS VRegHive_InternalOpenRecords(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* pCurrent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _Out_ VRegHiveInternalOpenResult* const pResult
)
{
    SIZE_T i;
    for (i = 0; i < cPath; ++i)
    {
        VRegInternalRedirect redirect;
        void* pToken;
        LSTATUS status = VRegHive_InternalOpenLower(pHive, pCurrent, pPath + i, 1, 0, FALSE, &redirect, &pToken);
        if (status == ERROR_SUCCESS)
            status = VRegHive_InternalAddRecord(pHive, pCurrent, pPath[i], pToken, &pCurrent);
        if (status != ERROR_SUCCESS)
            return status;
    }

    pResult->pNode = pCurrent;
    return ERROR_SUCCESS;
}

/// Fires @p dwFilter on @p pNode after a write went through to the provider.
static void VRegHive_InternalRecordChanged(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode,
    const DWORD dwFilter
)
{
    if (!VRegHive_InternalLockRecords(pHive))
        return;

    if (!pNode->isDeleted)
    {
        VRegHive_InternalTouch(pNode);
        VRegHive_InternalNotify(pNode, dwFilter);
    }
    VRegHive_InternalUnlockWrite(pHive);
}

VREG_DECL LSTATUS VRegHive_InternalOpenKey(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired,
    _Out_ VRegHiveInternalOpenResult* const pResult
)
{
    VRegHiveInternalNode* pCurrent = pParent;
    SIZE_T cMatched = 0;
    BOOL isVisible = FALSE;
    BOOL isRedirect;
    REGSAM samLower;
    void* pToken;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->redirect.pRegistry = NULL;
    pResult->redirect.hKey = NULL;

    status = VRegHive_InternalWalkUpper(pHive, pParent, pPath, cPath, &pCurrent, &cMatched, &isVisible);
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
        status = VRegHive_InternalOpenRecords(pHive, pCurrent, pPath + cMatched, cPath - cMatched, pResult);
        goto done;
    }

    // Nothing of the upper layer is at or below the key, so it is the lower
    // key's to answer. A frozen hive hands the lower key out; a writable one
    // only asks whether it exists.
    isRedirect = VRegHive_InternalIsFrozen(pHive);
    samLower = isRedirect ? samDesired : KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES);
    status = VRegHive_InternalOpenLower(
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
    if (!VRegHive_InternalLockWrite(pHive))
    {
        // Frozen since the lower open: the frozen answer is a redirect.
        status = VRegHive_InternalOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
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
            VRegHiveInternalNode* pChild = VRegHive_InternalFindChild(pCurrent, pPath[i], NULL);
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

            status = VRegHive_InternalAddChild(pHive, pCurrent, pPath[i], VREG_INTERNAL_NO_NAME, FALSE, &pChild);
            if (status != ERROR_SUCCESS)
                break;

            pChild->isTransparent = TRUE;
            pChild->isLowerBacked = TRUE;
            pCurrent = pChild;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VRegHive_InternalUnlockWrite(pHive);

done:
    return status;
}

/// How far @p pPath exists in the upper layer below @p pParent, for
/// @ref VRegHive_InternalCreateKey: @p pcUpper components, ending at @p ppLast;
/// @p pIsBehindTombstone when a tombstone stopped the walk; otherwise whether
/// the lower key of @p ppLast shows through, when the path goes on.
static LSTATUS VRegHive_InternalMeasureUpper(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _Out_ SIZE_T* const pcUpper,
    _Outptr_ VRegHiveInternalNode** const ppLast,
    _Out_ BOOL* const pIsBehindTombstone,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    VRegHiveInternalNode* pCurrent = pParent;
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
            VRegHiveInternalNode* const pChild = VRegHive_InternalFindChild(pCurrent, pPath[cUpper], NULL);
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
            *pIsVisible = VRegHive_InternalIsVisible(pHive, pCurrent);
    }

    VRegHive_InternalUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    *ppLast = pCurrent;
    return status;
}

/// @ref VRegHive_InternalCreateKey on a custom hive: an existing key opens, and each
/// missing key is created through the provider, one at a time, the class
/// going to the last.
static LSTATUS VRegHive_InternalCreateKeyThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _In_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    _Out_ VRegHiveInternalOpenResult* const pResult,
    _Out_ LPDWORD const pdwDisposition
)
{
    VRegHiveInternalNode* pCurrent = pParent;
    SIZE_T cMatched = 0;
    BOOL isVisible = FALSE;
    VRegInternalWString name = {0, 0, NULL};
    SIZE_T i;
    LSTATUS status;

    *pdwDisposition = REG_OPENED_EXISTING_KEY;
    status = VRegHive_InternalOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
    if (status != ERROR_FILE_NOT_FOUND)
        return status;
    if (VRegHive_InternalIsFrozen(pHive) || !pHive->pLowerVtbl->CreateKey)
        return ERROR_ACCESS_DENIED;

    // The open recorded every key that exists; the rest is created.
    status = VRegHive_InternalWalkUpper(pHive, pParent, pPath, cPath, &pCurrent, &cMatched, &isVisible);
    for (i = cMatched; status == ERROR_SUCCESS && i < cPath; ++i)
    {
        const BOOL isLast = i + 1 == cPath;
        void* pToken = NULL;
        BOOL isCreated = FALSE;
        VRegHiveInternalNode* pRecord = NULL;
        if (!VRegInternalWString_Assign(&name, pPath[i].pch, pPath[i].cch))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }

        status = pHive->pLowerVtbl->CreateKey(
            pHive,
            pCurrent,
            VRegInternalWString_GetText(&name),
            isLast ? pszClassName : L"",
            dwOptions,
            &pToken,
            &isCreated
        );
        if (status == ERROR_SUCCESS)
            status = VRegHive_InternalAddRecord(pHive, pCurrent, pPath[i], pToken, &pRecord);
        if (status != ERROR_SUCCESS)
            break;

        if (isCreated)
            VRegHive_InternalRecordChanged(pHive, pCurrent, REG_NOTIFY_CHANGE_NAME);
        if (isLast && isCreated)
            *pdwDisposition = REG_CREATED_NEW_KEY;
        pCurrent = pRecord;
    }

    VRegInternalWString_Free(&name);
    if (status == ERROR_SUCCESS)
        pResult->pNode = pCurrent;
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalCreateKey(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath,
    _In_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    _Out_ VRegHiveInternalOpenResult* const pResult,
    _Out_ LPDWORD const pdwDisposition
)
{
    const VRegInternalWStringView className = VReg_InternalViewFromText(pszClassName);
    const BOOL isVolatile = (dwOptions & REG_OPTION_VOLATILE) != 0;
    SIZE_T cUpper = 0;
    SIZE_T cLower;
    VRegHiveInternalNode* pLast = pParent;
    BOOL isBehindTombstone = FALSE;
    BOOL isVisible = FALSE;
    VRegHiveInternalNode* pCurrent;
    SIZE_T i;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->redirect.pRegistry = NULL;
    pResult->redirect.hKey = NULL;
    *pdwDisposition = REG_OPENED_EXISTING_KEY;

    if (pHive->kind == VREG_INTERNAL_HIVE_CUSTOM)
    {
        return VRegHive_InternalCreateKeyThrough(
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

    if (VRegHive_InternalIsFrozen(pHive))
    {
        status = VRegHive_InternalOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
        return status == ERROR_FILE_NOT_FOUND ? ERROR_ACCESS_DENIED : status;
    }

    // How far the path exists: in the upper layer first, then below it.
    status = VRegHive_InternalMeasureUpper(pHive, pParent, pPath, cPath, &cUpper, &pLast, &isBehindTombstone, &isVisible);
    if (status != ERROR_SUCCESS)
        goto done;

    cLower = cUpper;
    if (isVisible)
    {
        for (; cLower < cPath; ++cLower)
        {
            status = VRegHive_InternalProbeLower(pHive, pLast, pPath + cUpper, cLower + 1 - cUpper, KEY_QUERY_VALUE);
            if (status == ERROR_FILE_NOT_FOUND)
            {
                status = ERROR_SUCCESS;
                break;
            }
            if (!VReg_InternalIsExistingStatus(status))
                goto done;

            status = ERROR_SUCCESS;
        }
    }

    if (!VRegHive_InternalLockWrite(pHive))
    {
        status = VRegHive_InternalCreateKey(
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
            VRegHiveInternalNode* pChild = VRegHive_InternalFindChild(pCurrent, pPath[i], NULL);
            VRegHiveInternalNode* pNew;
            if (pChild && !pChild->isTombstone)
            {
                pCurrent = pChild;
                continue;
            }

            if (pChild)
            {
                // Created again over a deleted lower key: nothing below shows.
                const VRegInternalWStringView newClass = isLast ? className : VREG_INTERNAL_NO_NAME;
                if (!VRegInternalWString_Assign(&pChild->className, newClass.pch, newClass.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                    break;
                }

                pChild->isTombstone = FALSE;
                pChild->isOpaque = TRUE;
                VRegHive_InternalTouch(pChild);
                VRegHive_InternalMaterialize(pCurrent);
                VRegHive_InternalNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
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

            status = VRegHive_InternalAddChild(
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
                VRegHive_InternalMaterialize(pCurrent);
                VRegHive_InternalNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
                if (isLast)
                    *pdwDisposition = REG_CREATED_NEW_KEY;
            }

            pCurrent = pNew;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VRegHive_InternalUnlockWrite(pHive);

done:
    return status;
}

/// What @ref VRegHive_InternalDeleteKey needs to know about @p pNode before it calls
/// the lower: the names of its tombstone children (a child that is not one
/// fails the call), whether its lower key and its parent's show through,
/// and its name.
static LSTATUS VRegHive_InternalInspectForDelete(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _Inout_ VRegInternalArray* const pTombstones,
    _Out_ BOOL* const pIsVisible,
    _Out_ BOOL* const pIsParentVisible,
    _Inout_ VRegInternalWString* const pKeyName
)
{
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
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
        const VRegHiveInternalNode* const pChild = VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, i);
        if (!pChild->isTombstone)
            status = ERROR_ACCESS_DENIED;
        else if (!VRegInternalArray_InsertWStringSorted(pTombstones, VRegInternalWString_GetView(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS)
    {
        *pIsVisible = VRegHive_InternalIsVisible(pHive, pNode);
        *pIsParentVisible = VRegHive_InternalIsVisible(pHive, pNode->pParent);
    }
    if (status == ERROR_SUCCESS)
    {
        const VRegInternalWStringView keyName = VRegInternalWString_GetView(&pNode->name);
        if (!VRegInternalWString_Assign(pKeyName, keyName.pch, keyName.cch))
            status = ERROR_OUTOFMEMORY;
    }

    VRegHive_InternalUnlockRead(pHive, isLocked);
    return status;
}

/// @ref VRegHive_InternalDeleteKey on a custom hive: the provider deletes, and the
/// record is marked deleted.
static LSTATUS VRegHive_InternalDeleteKeyThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath
)
{
    VRegHiveInternalOpenResult opened;
    LSTATUS status;

    if (VRegHive_InternalIsFrozen(pHive) || !pHive->pLowerVtbl->DeleteKey)
        return ERROR_ACCESS_DENIED;

    status = VRegHive_InternalOpenKey(pHive, pParent, pPath, cPath, KEY_READ, &opened);
    if (status != ERROR_SUCCESS)
        return status;
    if (!opened.pNode->pParent)
        return ERROR_ACCESS_DENIED;

    status = pHive->pLowerVtbl->DeleteKey(pHive, opened.pNode);
    if (status != ERROR_SUCCESS || !VRegHive_InternalLockRecords(pHive))
        return status;

    if (!opened.pNode->isDeleted)
        VRegHive_InternalRemove(opened.pNode);
    VRegHive_InternalUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegHive_InternalDeleteKey(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pParent,
    _In_reads_(cPath) const VRegInternalWStringView* const pPath,
    const SIZE_T cPath
)
{
    VRegHiveInternalOpenResult opened;
    VRegHiveInternalNode* pNode;
    VRegInternalArray tombstones = {NULL, 0, 0};
    BOOL isVisible = FALSE;
    BOOL isParentVisible = FALSE;
    VRegInternalWString keyName = {0, 0, NULL};
    VRegInternalWString lowerName = {0, 0, NULL};
    BOOL hasLower = FALSE;
    SIZE_T i;
    LSTATUS status;

    if (pHive->kind == VREG_INTERNAL_HIVE_CUSTOM)
        return VRegHive_InternalDeleteKeyThrough(pHive, pParent, pPath, cPath);
    if (VRegHive_InternalIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    status = VRegHive_InternalOpenKey(pHive, pParent, pPath, cPath, KEY_READ, &opened);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen since the check above, so the open answered with a redirect.
    if (!opened.pNode)
    {
        VRegInternalRedirect_Close(&opened.redirect);
        return ERROR_ACCESS_DENIED;
    }

    pNode = opened.pNode;
    status = VRegHive_InternalInspectForDelete(pHive, pNode, &tombstones, &isVisible, &isParentVisible, &keyName);
    if (status != ERROR_SUCCESS)
        goto done;

    // Whether a lower key sits at the node's path decides if a tombstone must
    // replace the node, even when the node is opaque and hides what is below
    // it. A lower subkey that no tombstone hides still makes the key non-empty.
    if (isParentVisible)
    {
        status = VRegHive_InternalProbeLower(pHive, pNode, NULL, 0, KEY_ENUMERATE_SUB_KEYS);
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
            goto done;

        hasLower = status == ERROR_SUCCESS;
        status = ERROR_SUCCESS;
        for (i = 0; hasLower && isVisible; ++i)
        {
            VRegHiveResult entry;
            LSTATUS enumStatus;
            VRegHiveResult_InternalInit(&entry);
            entry.pName = &lowerName;
            enumStatus = pHive->pLowerVtbl->EnumKey(pHive, pNode, (DWORD)i, &entry);
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            if (enumStatus != ERROR_SUCCESS)
            {
                status = enumStatus;
                goto done;
            }
            if (!VRegInternalArray_ContainsWStringSorted(&tombstones, VRegInternalWString_GetView(&lowerName)))
            {
                status = ERROR_ACCESS_DENIED;
                goto done;
            }
        }
    }

    if (!VRegHive_InternalLockWrite(pHive))
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
        VRegHiveInternalNode* const pNodeParent = pNode->pParent;
        for (i = 0; i < pNode->children.count; ++i)
        {
            if (!VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, i)->isTombstone)
            {
                status = ERROR_ACCESS_DENIED;
                break;
            }
        }

        if (status == ERROR_SUCCESS)
        {
            VRegHive_InternalRemove(pNode);
            if (hasLower)
            {
                VRegHiveInternalNode* pTombstone;
                status = VRegHive_InternalAddChild(
                    pHive,
                    pNodeParent,
                    VRegInternalWString_GetView(&keyName),
                    VREG_INTERNAL_NO_NAME,
                    FALSE,
                    &pTombstone
                );
                if (status == ERROR_SUCCESS)
                    pTombstone->isTombstone = TRUE;
            }

            VRegHive_InternalMaterialize(pNodeParent);
        }
    }

    VRegHive_InternalUnlockWrite(pHive);

done:
    VRegInternalArray_FreeWStrings(&tombstones);
    VRegInternalWString_Free(&keyName);
    VRegInternalWString_Free(&lowerName);
    return status;
}

/// Whether @p pNode was deleted through VReg, read under the lock.
static BOOL VRegHive_InternalIsDeleted(
    _In_ VRegHive* const pHive,
    _In_ const VRegHiveInternalNode* const pNode
)
{
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
    const BOOL isDeleted = pNode->isDeleted;
    VRegHive_InternalUnlockRead(pHive, isLocked);
    return isDeleted;
}

/// The subkeys of @p pNode deleted through the provider's `DeleteKey`, as
/// native's `RegDeleteTree` deletes them: in enumeration order, each subtree
/// before its key, stopping at the first failure. The names are read first,
/// so a provider whose deletes leave its enumeration as it was cannot loop
/// this.
static LSTATUS VRegHive_InternalDeleteSubkeysThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    VRegInternalArray names = {NULL, 0, 0};
    VRegInternalWString name = {0, 0, NULL};
    SIZE_T i;
    LSTATUS status;

    for (i = 0;; ++i)
    {
        VRegHiveResult entry;
        VRegHiveResult_InternalInit(&entry);
        entry.pName = &name;
        status = pHive->pLowerVtbl->EnumKey(pHive, pNode, (DWORD)i, &entry);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VRegInternalArray_AppendWString(&names, VRegInternalWString_GetView(&name)))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    for (i = 0; i < names.count; ++i)
    {
        const VRegInternalWStringView child = VRegInternalArray_WStringViewAt(&names, i);
        VRegHiveInternalOpenResult opened;
        status = VRegHive_InternalOpenKey(pHive, pNode, &child, 1, KEY_READ, &opened);
        if (status != ERROR_SUCCESS)
            goto done;
        status = VRegHive_InternalDeleteSubkeysThrough(pHive, opened.pNode);
        if (status != ERROR_SUCCESS)
            goto done;
        status = pHive->pLowerVtbl->DeleteKey(pHive, opened.pNode);
        if (status != ERROR_SUCCESS)
            goto done;
        if (VRegHive_InternalLockRecords(pHive))
        {
            if (!opened.pNode->isDeleted)
                VRegHive_InternalRemove(opened.pNode);
            VRegHive_InternalUnlockWrite(pHive);
        }
    }

done:
    VRegInternalWString_Free(&name);
    VRegInternalArray_FreeWStrings(&names);
    return status;
}

/// The values of @p pNode deleted through the provider's `DeleteValue`, in
/// enumeration order, stopping at the first failure; names first, as in
/// @ref VRegHive_InternalDeleteSubkeysThrough.
static LSTATUS VRegHive_InternalDeleteValuesThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    VRegInternalArray names = {NULL, 0, 0};
    VRegInternalWString name = {0, 0, NULL};
    SIZE_T i;
    LSTATUS status;

    for (i = 0;; ++i)
    {
        VRegHiveResult entry;
        VRegHiveResult_InternalInit(&entry);
        entry.pName = &name;
        status = pHive->pLowerVtbl->EnumValue(pHive, pNode, (DWORD)i, &entry);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VRegInternalArray_AppendWString(&names, VRegInternalWString_GetView(&name)))
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
            VRegInternalWString_GetText(&VREG_INTERNAL_AT(&names, VRegInternalWString, i))
        );
        if (status != ERROR_SUCCESS)
            goto done;
    }

done:
    VRegInternalWString_Free(&name);
    VRegInternalArray_FreeWStrings(&names);
    return status;
}

/// @ref VRegHive_InternalDeleteTree on a custom hive: the provider empties the key,
/// through its `DeleteTree` or, without one, key by key and value by value
/// in native's order, and the records below it are marked deleted.
static LSTATUS VRegHive_InternalDeleteTreeThrough(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    const VRegHiveVtbl* const pVtbl = pHive->pLowerVtbl;
    LSTATUS status;
    if (VRegHive_InternalIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;
    if (!pVtbl->DeleteTree && (!pVtbl->DeleteKey || !pVtbl->DeleteValue))
        return ERROR_ACCESS_DENIED;
    if (VRegHive_InternalIsDeleted(pHive, pNode))
        return ERROR_KEY_DELETED;

    if (pVtbl->DeleteTree)
    {
        status = pVtbl->DeleteTree(pHive, pNode);
    }
    else
    {
        status = VRegHive_InternalDeleteSubkeysThrough(pHive, pNode);
        if (status == ERROR_SUCCESS)
            status = VRegHive_InternalDeleteValuesThrough(pHive, pNode);
    }
    if (status != ERROR_SUCCESS || !VRegHive_InternalLockRecords(pHive))
        return status;

    if (!pNode->isDeleted)
    {
        while (pNode->children.count != 0)
            VRegHive_InternalRemove(VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, pNode->children.count - 1));

        VRegHive_InternalTouch(pNode);
        VRegHive_InternalNotify(pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    }
    VRegHive_InternalUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegHive_InternalDeleteTree(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode
)
{
    if (pHive->kind == VREG_INTERNAL_HIVE_CUSTOM)
        return VRegHive_InternalDeleteTreeThrough(pHive, pNode);
    if (!VRegHive_InternalLockWrite(pHive))
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
    {
        VRegHive_InternalUnlockWrite(pHive);
        return ERROR_KEY_DELETED;
    }

    while (pNode->children.count != 0)
        VRegHive_InternalRemove(VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, pNode->children.count - 1));

    VRegHiveInternalNode_ClearValues(pNode);
    pNode->isOpaque = pHive->pLowerVtbl != NULL;
    pNode->isTransparent = FALSE;
    pNode->isLowerBacked = FALSE;
    VRegHive_InternalMaterialize(pNode);
    VRegHive_InternalTouch(pNode);
    VRegHive_InternalNotify(pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    VRegHive_InternalUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

// --- Values ---

/// Reads value @p name of @p pNode from the upper layer. @p pIsFound says
/// whether the layer decided the answer; otherwise @p pIsVisible and
/// @p pViewPath describe the lower key to ask.
static LSTATUS VRegHive_InternalQueryUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    const VRegInternalWStringView name,
    _Out_ BOOL* const pIsFound,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VRegInternalBuffer* const pData,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VRegHiveInternalValue* pValue;

    *pIsFound = TRUE;
    *pIsVisible = FALSE;
    *pdwType = REG_NONE;
    *pcbData = 0;
    pValue = pNode->isDeleted ? NULL : VRegHive_InternalFindValue(pNode, name);
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
            if (pData && !VRegInternalBuffer_Assign(pData, pValue->data.pb, pValue->data.cb))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        *pIsFound = FALSE;
        *pIsVisible = VRegHive_InternalIsVisible(pHive, pNode);
    }

    VRegHive_InternalUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalQueryValue(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _In_ const LPCWSTR pszName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VRegInternalBuffer* const pData
)
{
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VRegHiveResult lower;
    LSTATUS status = VRegHive_InternalQueryUpperValue(
        pHive,
        pNode,
        VReg_InternalViewFromText(pszName),
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

    VRegHiveResult_InternalInit(&lower);
    lower.pdwType = pdwType;
    lower.pcbData = pcbData;
    lower.pData = pData;
    status = pHive->pLowerVtbl->QueryValue(pHive, pNode, pszName, &lower);

done:
    return status;
}

static LSTATUS VRegHive_InternalStoreValue(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode,
    const VRegInternalWStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData
)
{
    VRegInternalBuffer bytes = {0, 0, NULL};
    VRegHiveInternalValue* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (!VRegInternalBuffer_Assign(&bytes, pData, cbData))
        return ERROR_OUTOFMEMORY;

    if (!VRegHive_InternalLockWrite(pHive))
    {
        VRegInternalBuffer_Free(&bytes);
        return ERROR_ACCESS_DENIED;
    }

    pValue = pNode->isDeleted ? NULL : VRegHive_InternalFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        VRegInternalBuffer_Free(&pValue->data);
        pValue->dwType = dwType;
        pValue->data = bytes;
        pValue->isTombstone = FALSE;
        bytes.pb = NULL;
    }
    else
    {
        VRegHiveInternalValue value;
        value.name.psz = NULL;
        value.name.cch = 0;
        value.name.cchCapacity = 0;
        value.dwType = dwType;
        value.data = bytes;
        value.isTombstone = FALSE;
        if (!VRegInternalWString_Assign(&value.name, name.pch, name.cch))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (!VRegInternalArray_Append(&pNode->values, sizeof(value), &value))
        {
            VRegInternalWString_Free(&value.name);
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            bytes.pb = NULL;
        }
    }

    if (status == ERROR_SUCCESS)
    {
        VRegHive_InternalMaterialize(pNode);
        VRegHive_InternalTouch(pNode);
        VRegHive_InternalNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
    }

    VRegHive_InternalUnlockWrite(pHive);
    if (bytes.pb)
        VRegInternalBuffer_Free(&bytes);
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalSetValue(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode,
    _In_ const LPCWSTR pszName,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData
)
{
    LSTATUS status;
    if (pHive->kind != VREG_INTERNAL_HIVE_CUSTOM)
        return VRegHive_InternalStoreValue(pHive, pNode, VReg_InternalViewFromText(pszName), dwType, pData, cbData);

    if (VRegHive_InternalIsFrozen(pHive) || !pHive->pLowerVtbl->SetValue)
        return ERROR_ACCESS_DENIED;
    if (VRegHive_InternalIsDeleted(pHive, pNode))
        return ERROR_KEY_DELETED;

    status = pHive->pLowerVtbl->SetValue(pHive, pNode, pszName, dwType, pData, cbData);
    if (status == ERROR_SUCCESS)
        VRegHive_InternalRecordChanged(pHive, pNode, REG_NOTIFY_CHANGE_LAST_SET);
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalDeleteValue(
    _In_ VRegHive* const pHive,
    _Inout_ VRegHiveInternalNode* const pNode,
    _In_ const LPCWSTR pszName
)
{
    const VRegInternalWStringView name = VReg_InternalViewFromText(pszName);
    BOOL isLocked;
    BOOL isInUpper = FALSE;
    BOOL isInLower = FALSE;
    BOOL isVisible = FALSE;
    VRegHiveInternalValue* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (VRegHive_InternalIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;
    if (pHive->kind == VREG_INTERNAL_HIVE_CUSTOM)
    {
        if (!pHive->pLowerVtbl->DeleteValue)
            return ERROR_ACCESS_DENIED;
        if (VRegHive_InternalIsDeleted(pHive, pNode))
            return ERROR_KEY_DELETED;

        status = pHive->pLowerVtbl->DeleteValue(pHive, pNode, pszName);
        if (status == ERROR_SUCCESS)
            VRegHive_InternalRecordChanged(pHive, pNode, REG_NOTIFY_CHANGE_LAST_SET);
        return status;
    }

    isLocked = VRegHive_InternalLockRead(pHive);
    pValue = pNode->isDeleted ? NULL : VRegHive_InternalFindValue(pNode, name);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (pValue && pValue->isTombstone)
        status = ERROR_FILE_NOT_FOUND;
    else
        isInUpper = pValue != NULL;
    if (status == ERROR_SUCCESS)
        isVisible = VRegHive_InternalIsVisible(pHive, pNode);
    VRegHive_InternalUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS)
        goto done;

    // A missing lower key holds no value; any other failure to open it fails
    // the call, before the value's own answer is read.
    if (isVisible)
    {
        status = VRegHive_InternalProbeLower(pHive, pNode, NULL, 0, KEY_QUERY_VALUE);
        if (status == ERROR_SUCCESS)
        {
            VRegHiveResult lower;
            LSTATUS queryStatus;
            VRegHiveResult_InternalInit(&lower);
            queryStatus = pHive->pLowerVtbl->QueryValue(pHive, pNode, pszName, &lower);
            if (queryStatus != ERROR_FILE_NOT_FOUND && !VReg_InternalIsExistingStatus(queryStatus))
            {
                status = queryStatus;
                goto done;
            }

            isInLower = VReg_InternalIsExistingStatus(queryStatus);
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

    if (!VRegHive_InternalLockWrite(pHive))
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
        pValue = VRegHive_InternalFindValue(pNode, name);
        if (isInLower)
        {
            if (pValue)
            {
                VRegInternalBuffer_Free(&pValue->data);
                pValue->dwType = REG_NONE;
                pValue->isTombstone = TRUE;
            }
            else
            {
                VRegHiveInternalValue tombstone;
                ZeroMemory(&tombstone, sizeof(tombstone));
                tombstone.dwType = REG_NONE;
                tombstone.isTombstone = TRUE;
                if (!VRegInternalWString_Assign(&tombstone.name, name.pch, name.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                }
                else if (!VRegInternalArray_Append(&pNode->values, sizeof(tombstone), &tombstone))
                {
                    VRegInternalWString_Free(&tombstone.name);
                    status = ERROR_OUTOFMEMORY;
                }
            }
        }
        else if (pValue)
        {
            const SIZE_T index = (SIZE_T)(pValue - &VREG_INTERNAL_AT(&pNode->values, VRegHiveInternalValue, 0));
            VRegHiveInternalValue_Free(pValue);
            VRegInternalArray_RemoveAt(&pNode->values, sizeof(*pValue), index);
        }

        if (status == ERROR_SUCCESS)
        {
            VRegHive_InternalMaterialize(pNode);
            VRegHive_InternalTouch(pNode);
            VRegHive_InternalNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
        }
    }

    VRegHive_InternalUnlockWrite(pHive);

done:
    return status;
}

// --- Enumeration ---

/// The upper layer's part of @ref VRegHive_InternalEnumKey. @p pIsFound says the
/// answer is complete. Otherwise, when @p ppClassNode is set, the entry is
/// an upper key made from a lower one whose class is still to be read; and
/// when not, the lower key of @p pNode (if @p pIsVisible) continues the
/// enumeration past @p pcUpper upper entries, with the names in @p pShadowed
/// hidden.
static LSTATUS VRegHive_InternalEnumUpperKey(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Inout_ VRegInternalWString* const pClassName,
    _Out_ FILETIME* const pLastWrite,
    _Out_ BOOL* const pIsFound,
    _Outptr_result_maybenull_ VRegHiveInternalNode** const ppClassNode,
    _Out_ BOOL* const pIsVisible,
    _Out_ DWORD* const pcUpper,
    _Inout_ VRegInternalArray* const pShadowed
)
{
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
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
        VRegHiveInternalNode* const pChild = VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, i);

        // A transparent child stands for a lower key; it is listed with the
        // lower ones.
        if (pChild->isTransparent)
            continue;

        if (!pChild->isTombstone && cUpper++ == dwIndex)
        {
            if (
                !VRegInternalWString_Assign(pName, VRegInternalWString_GetText(&pChild->name), pChild->name.cch)
                || !VRegInternalWString_Assign(
                    pClassName,
                    VRegInternalWString_GetText(&pChild->className),
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

            if (VRegHive_InternalIsVisible(pHive, pChild))
                *ppClassNode = pChild;
            else
                *pIsFound = TRUE;
            break;
        }

        if (!VRegInternalArray_InsertWStringSorted(pShadowed, VRegInternalWString_GetView(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound && !*ppClassNode)
        *pIsVisible = VRegHive_InternalIsVisible(pHive, pNode);

    VRegHive_InternalUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalEnumKey(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Inout_ VRegInternalWString* const pClassName,
    _Out_ FILETIME* const pLastWrite
)
{
    BOOL isFound = FALSE;
    VRegHiveInternalNode* pClassNode = NULL;
    BOOL isVisible = FALSE;
    VRegInternalWString nameBuffer = {0, 0, NULL};
    VRegInternalWString classBuffer = {0, 0, NULL};
    VRegInternalArray shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD dwSkip;
    DWORD i;
    LSTATUS status = VRegHive_InternalEnumUpperKey(
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
        status = VRegHive_InternalQueryLowerClass(pHive, pClassNode, pClassName);
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
        VRegHiveResult_InternalInit(&entry);
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

        if (VRegInternalArray_ContainsWStringSorted(&shadowed, VRegInternalWString_GetView(&nameBuffer)) || dwSkip-- != 0)
            continue;

        if (
            !VRegInternalWString_Assign(pName, VRegInternalWString_GetText(&nameBuffer), nameBuffer.cch)
            || !VRegInternalWString_Assign(pClassName, VRegInternalWString_GetText(&classBuffer), classBuffer.cch)
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
    VRegInternalArray_FreeWStrings(&shadowed);
    VRegInternalWString_Free(&nameBuffer);
    VRegInternalWString_Free(&classBuffer);
    return status;
}

/// The upper layer's part of @ref VRegHive_InternalEnumValue, like
/// @ref VRegHive_InternalEnumUpperKey without the class step.
static LSTATUS VRegHive_InternalEnumUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
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
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
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
        const VRegHiveInternalValue* const pValue = &VREG_INTERNAL_AT(&pNode->values, VRegHiveInternalValue, i);
        if (!pValue->isTombstone && cUpper++ == dwIndex)
        {
            *pIsFound = TRUE;
            if (
                !VRegInternalWString_Assign(pName, VRegInternalWString_GetText(&pValue->name), pValue->name.cch)
                || (pData && !VRegInternalBuffer_Assign(pData, pValue->data.pb, pValue->data.cb))
            )
            {
                status = ERROR_OUTOFMEMORY;
                break;
            }

            *pdwType = pValue->dwType;
            *pcbData = (DWORD)pValue->data.cb;
            break;
        }

        if (!VRegInternalArray_InsertWStringSorted(pShadowed, VRegInternalWString_GetView(&pValue->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound)
        *pIsVisible = VRegHive_InternalIsVisible(pHive, pNode);

    VRegHive_InternalUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalEnumValue(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
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
    LSTATUS status = VRegHive_InternalEnumUpperValue(
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
        VRegHiveResult_InternalInit(&entry);
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

        if (VRegInternalArray_ContainsWStringSorted(&shadowed, VRegInternalWString_GetView(pName)) || dwSkip-- != 0)
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
    VRegInternalArray_FreeWStrings(&shadowed);
    return status;
}

/// The upper layer's part of @ref VRegHive_InternalQueryInfo: the counts and
/// maxima of the node's own entries into @p pInfo, the names that shadow
/// lower entries, and the node's relation to its lower key.
static LSTATUS VRegHive_InternalQueryUpperInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _Inout_ VRegInternalKeyInfo* const pInfo,
    _Inout_ VRegInternalArray* const pShadowedKeys,
    _Inout_ VRegInternalArray* const pShadowedValues,
    _Out_ BOOL* const pIsTransparent,
    _Out_ BOOL* const pIsLowerBacked,
    _Out_ BOOL* const pIsVisible
)
{
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;

    *pIsTransparent = FALSE;
    *pIsLowerBacked = FALSE;
    *pIsVisible = FALSE;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && i < pNode->children.count; ++i)
    {
        const VRegHiveInternalNode* const pChild = VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, i);
        if (pChild->isTransparent)
            continue;

        if (!VRegInternalArray_InsertWStringSorted(pShadowedKeys, VRegInternalWString_GetView(&pChild->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pChild->isTombstone)
            continue;

        ++pInfo->cSubKeys;
        pInfo->cchMaxSubKey = VReg_InternalMaxDword(pInfo->cchMaxSubKey, (DWORD)pChild->name.cch);
        pInfo->cchMaxClass = VReg_InternalMaxDword(pInfo->cchMaxClass, (DWORD)pChild->className.cch);
    }

    for (i = 0; status == ERROR_SUCCESS && i < pNode->values.count; ++i)
    {
        const VRegHiveInternalValue* const pValue = &VREG_INTERNAL_AT(&pNode->values, VRegHiveInternalValue, i);
        if (!VRegInternalArray_InsertWStringSorted(pShadowedValues, VRegInternalWString_GetView(&pValue->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pValue->isTombstone)
            continue;

        ++pInfo->cValues;
        pInfo->cchMaxValueName = VReg_InternalMaxDword(pInfo->cchMaxValueName, (DWORD)pValue->name.cch);
        pInfo->cbMaxValue = VReg_InternalMaxDword(pInfo->cbMaxValue, (DWORD)pValue->data.cb);
    }

    if (status == ERROR_SUCCESS)
    {
        *pIsTransparent = pNode->isTransparent;
        *pIsLowerBacked = pNode->isLowerBacked;
        pInfo->lastWrite = pNode->lastWrite;
        const VRegInternalWStringView className = VRegInternalWString_GetView(&pNode->className);
        if (!VRegInternalWString_Assign(&pInfo->className, className.pch, className.cch))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        *pIsVisible = VRegHive_InternalIsVisible(pHive, pNode);

    VRegHive_InternalUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalQueryInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
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

    status = VRegHive_InternalQueryUpperInfo(
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

    status = VRegHive_InternalQueryLowerInfo(pHive, pNode, &lowerInfo);
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
        const VRegInternalWStringView childName = VRegInternalArray_WStringViewAt(&shadowedKeys, i);
        const LSTATUS childStatus = VRegHive_InternalProbeLower(pHive, pNode, &childName, 1, KEY_QUERY_VALUE);
        if (childStatus != ERROR_FILE_NOT_FOUND && !VReg_InternalIsExistingStatus(childStatus))
        {
            status = childStatus;
            goto done;
        }
        if (VReg_InternalIsExistingStatus(childStatus))
            --lowerInfo.cSubKeys;
    }

    for (i = 0; i < shadowedValues.count; ++i)
    {
        LSTATUS valueStatus;
        VRegHiveResult_InternalInit(&lower);
        valueStatus = pHive->pLowerVtbl->QueryValue(
            pHive,
            pNode,
            VRegInternalWString_GetText(&VREG_INTERNAL_AT(&shadowedValues, VRegInternalWString, i)),
            &lower
        );
        if (valueStatus != ERROR_FILE_NOT_FOUND && !VReg_InternalIsExistingStatus(valueStatus))
        {
            status = valueStatus;
            goto done;
        }
        if (VReg_InternalIsExistingStatus(valueStatus))
            --lowerInfo.cValues;
    }

    pInfo->cSubKeys += lowerInfo.cSubKeys;
    pInfo->cValues += lowerInfo.cValues;
    pInfo->cchMaxSubKey = VReg_InternalMaxDword(pInfo->cchMaxSubKey, lowerInfo.cchMaxSubKey);
    pInfo->cchMaxClass = VReg_InternalMaxDword(pInfo->cchMaxClass, lowerInfo.cchMaxClass);
    pInfo->cchMaxValueName = VReg_InternalMaxDword(pInfo->cchMaxValueName, lowerInfo.cchMaxValueName);
    pInfo->cbMaxValue = VReg_InternalMaxDword(pInfo->cbMaxValue, lowerInfo.cbMaxValue);
    if (
        isLowerBacked
        && !VRegInternalWString_Assign(&pInfo->className, VRegInternalWString_GetText(&lowerInfo.className), lowerInfo.className.cch)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    if (isTransparent)
        pInfo->lastWrite = lowerInfo.lastWrite;

done:
    VRegInternalArray_FreeWStrings(&shadowedKeys);
    VRegInternalArray_FreeWStrings(&shadowedValues);
    VRegInternalWString_Free(&lowerInfo.className);
    return status;
}

// --- The rest ---

/// Visits @p pNode and everything below it, leaving out a transparent node
/// without values (the way to a lower key, which only the root can be while
/// it has upper keys below it). @p pPath is the node's path, extended and
/// restored for each child.
static LSTATUS VRegHive_InternalListUpper(
    _In_ VRegHive* const pHive,
    _In_ const VRegHiveInternalNode* const pNode,
    _Inout_ VRegInternalWString* const pPath,
    _In_ VRegInternalUpperVisitFunction* const pfnVisit,
    _In_opt_ void* const pContext
)
{
    const BOOL isBare = pNode->isTransparent && pNode->values.count == 0;
    const SIZE_T cchPath = pPath->cch;
    VRegHiveInternalUpperRecord record;
    LSTATUS status;
    SIZE_T i;

    ZeroMemory(&record, sizeof(record));
    record.path = VRegInternalWString_GetView(pPath);
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
        const VRegHiveInternalValue* const pValue = &VREG_INTERNAL_AT(&pNode->values, VRegHiveInternalValue, i);
        ZeroMemory(&record, sizeof(record));
        record.kind = VREG_INTERNAL_UPPER_VALUE;
        record.path = VRegInternalWString_GetView(pPath);
        record.name = VRegInternalWString_GetView(&pValue->name);
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
        const VRegHiveInternalNode* const pChild = VREG_INTERNAL_AT(&pNode->children, VRegHiveInternalNode*, i);
        if (
            (pPath->cch != 0 && !VRegInternalWString_AppendChar(pPath, L'\\'))
            || !VRegInternalWString_Append(pPath, VRegInternalWString_GetText(&pChild->name), pChild->name.cch)
        )
            return ERROR_OUTOFMEMORY;

        if (pChild->isTombstone)
        {
            ZeroMemory(&record, sizeof(record));
            record.kind = VREG_INTERNAL_UPPER_KEY;
            record.path = VRegInternalWString_GetView(pPath);
            record.isTombstone = TRUE;
            status = pfnVisit(pContext, &record);
        }
        else
        {
            status = VRegHive_InternalListUpper(pHive, pChild, pPath, pfnVisit, pContext);
        }

        if (status != ERROR_SUCCESS)
            return status;

        VRegInternalWString_Truncate(pPath, cchPath);
    }

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegHive_InternalSnapshotUpper(
    _In_ VRegHive* const pHive,
    _In_ VRegInternalUpperVisitFunction* const pfnVisit,
    _In_opt_ void* const pContext
)
{
    VRegInternalWString path = {0, 0, NULL};
    const BOOL isLocked = VRegHive_InternalLockRead(pHive);
    const LSTATUS status = VRegHive_InternalListUpper(pHive, pHive->pRoot, &path, pfnVisit, pContext);
    VRegHive_InternalUnlockRead(pHive, isLocked);
    VRegInternalWString_Free(&path);
    return status;
}

VREG_DECL LSTATUS VRegHive_InternalCheckKey(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode
)
{
    if (VRegHive_InternalIsDeleted(pHive, pNode))
        return ERROR_KEY_DELETED;
    if (pHive->pLowerVtbl && pHive->pLowerVtbl->CheckKey)
        return pHive->pLowerVtbl->CheckKey(pHive, pNode);

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegHive_InternalWatch(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
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
    isLocked = VRegHive_InternalLockRecords(pHive);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (isLocked)
    {
        VRegHiveInternalChangeWatch watch;
        watch.hEvent = hOwn;
        watch.isSubtree = bWatchSubtree != FALSE;
        watch.dwFilter = dwFilter;
        if (VRegInternalArray_Append(&pNode->watches, sizeof(watch), &watch))
            hOwn = NULL;
        else
            status = ERROR_OUTOFMEMORY;
    }

    if (isLocked)
        VRegHive_InternalUnlockWrite(pHive);
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
    if (!VRegHive_InternalLockRecords(pHive))
        return pKey->isDeleted ? ERROR_KEY_DELETED : ERROR_SUCCESS;

    if (pKey->isDeleted)
        status = ERROR_KEY_DELETED;
    else
        VRegHive_InternalNotify(pKey, dwFired);
    VRegHive_InternalUnlockWrite(pHive);
    return status;
}

// ==========================================================================
// Hive lowers over a registry
// ==========================================================================

// The lower of an overlay hive: a key of another registry, reached through
// the slots of @ref VRegHiveVtbl like any lower. Every call opens the lower
// key by its path again (the lower key's path, then the record's path in
// the hive) and closes it after, as the hive did before it had a table, so
// a record holds no handle: one lower handle per record would save the
// opens but stay pinned for the hive's life. The lower registry may itself
// be virtual.

typedef struct
{
    VReg* pRegistry;
    HKEY hRoot;
    VRegInternalWString path;
    REGSAM samView;
} VRegHiveInternalRegistryLower;

VREG_DECL void VRegInternalRedirect_Close(
    _In_ const VRegInternalRedirect* const pRedirect
)
{
    // A redirect always names the registry that answers its key.
    _Analysis_assume_(pRedirect->pRegistry);
    VReg_RegCloseKey(
        pRedirect->pRegistry,
        pRedirect->hKey
    );
}

/// Opens the lower key at @p pszSubKey below @p pNode with @p samDesired and
/// the lower's view.
static LSTATUS VRegHiveInternalRegistryLower_Open(
    _In_ VRegHive* const pHive,
    _In_ const VRegHiveInternalNode* const pNode,
    _In_ const LPCWSTR pszSubKey,
    const REGSAM samDesired,
    _Out_ PHKEY const phKey
)
{
    const VRegHiveInternalRegistryLower* const pLower = (const VRegHiveInternalRegistryLower*)pHive->pvLowerContext;
    const VRegInternalWStringView subKey = VReg_InternalViewFromText(pszSubKey);
    VRegInternalWString path = {0, 0, NULL};
    LSTATUS status;

    *phKey = NULL;
    if (
        !VRegInternalWString_Assign(&path, VRegInternalWString_GetText(&pLower->path), pLower->path.cch)
        || !VRegHive_InternalAppendNodePath(&path, pNode)
        || (subKey.cch != 0 && !VRegInternalWString_AppendPathComponent(&path, subKey.pch, subKey.cch))
    )
    {
        VRegInternalWString_Free(&path);
        return ERROR_OUTOFMEMORY;
    }

    status = VReg_RegOpenKeyExW(
        pLower->pRegistry,
        pLower->hRoot,
        path.cch == 0 ? NULL : path.psz,
        0,
        samDesired | pLower->samView,
        phKey
    );
    VRegInternalWString_Free(&path);
    return status;
}

static void VRegHiveInternalRegistryLower_Close(
    _In_ VRegHive* const pHive,
    _In_ const HKEY hKey
)
{
    const VRegHiveInternalRegistryLower* const pLower = (const VRegHiveInternalRegistryLower*)pHive->pvLowerContext;
    VReg_RegCloseKey(
        pLower->pRegistry,
        hKey
    );
}

/// The hive's own open of the lower, with the access and redirect the
/// public slot leaves out (@ref VRegHive_InternalOpenLowerFunction).
static LSTATUS VRegHiveInternalRegistryLower_OpenAs(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pParent,
    _In_ const LPCWSTR pszSubKey,
    const REGSAM samDesired,
    const BOOL isRedirect,
    _Out_ VRegInternalRedirect* const pRedirect
)
{
    HKEY hKey;
    const LSTATUS status = VRegHiveInternalRegistryLower_Open(pHive, pParent, pszSubKey, samDesired, &hKey);
    pRedirect->pRegistry = NULL;
    pRedirect->hKey = NULL;
    if (status != ERROR_SUCCESS)
        return status;

    if (isRedirect)
    {
        pRedirect->pRegistry = ((const VRegHiveInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
        pRedirect->hKey = hKey;
    }
    else
    {
        VRegHiveInternalRegistryLower_Close(pHive, hKey);
    }

    return ERROR_SUCCESS;
}

static LSTATUS WINAPI VRegHiveInternalRegistryLower_OpenKey(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveKey* const pParent,
    _In_ const LPCWSTR pszSubKey,
    _Outptr_result_maybenull_ void** const ppToken
)
{
    VRegInternalRedirect redirect;
    *ppToken = NULL;
    return VRegHiveInternalRegistryLower_OpenAs(pHive, pParent, pszSubKey, KEY_QUERY_VALUE, FALSE, &redirect);
}

static LSTATUS WINAPI VRegHiveInternalRegistryLower_QueryValue(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _In_ const LPCWSTR pszName,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegHiveInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    HKEY hKey;
    LSTATUS status = VRegHiveInternalRegistryLower_Open(pHive, pNode, L"", KEY_QUERY_VALUE, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    if (pResult->pData)
    {
        status = VReg_InternalQueryValueOwned(pRegistry, hKey, pszName, pResult->pdwType, pResult->pData);
        if (status == ERROR_SUCCESS && pResult->pcbData)
            *pResult->pcbData = (DWORD)pResult->pData->cb;
    }
    else
    {
        if (pResult->pcbData)
            *pResult->pcbData = 0;
        status = VReg_RegQueryValueExW(
            pRegistry,
            hKey,
            pszName,
            NULL,
            pResult->pdwType,
            NULL,
            pResult->pcbData
        );
    }

    VRegHiveInternalRegistryLower_Close(pHive, hKey);
    return status;
}

static LSTATUS WINAPI VRegHiveInternalRegistryLower_EnumKey(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _In_ const DWORD dwIndex,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegHiveInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    VRegInternalWString className = {0, 0, NULL};
    DWORD cchMaxClass = 0;
    DWORD cchName = VREG_INTERNAL_INITIAL_NAME_CHARS;
    DWORD cchClass;
    FILETIME lastWrite;
    HKEY hKey;
    LSTATUS status = VRegHiveInternalRegistryLower_Open(pHive, pNode, L"", KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg_RegQueryInfoKeyW(
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
    if (!VRegInternalWString_Resize(pResult->pName, cchName) || !VRegInternalWString_Resize(&className, cchClass))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    ZeroMemory(&lastWrite, sizeof(lastWrite));
    cchName = (DWORD)pResult->pName->cchCapacity;
    cchClass = (DWORD)className.cchCapacity;
    status = VReg_RegEnumKeyExW(
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

    VRegInternalWString_Truncate(pResult->pName, cchName);
    if (pResult->pClassName && !VRegInternalWString_Assign(pResult->pClassName, className.psz, cchClass))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    if (pResult->pLastWrite)
        *pResult->pLastWrite = lastWrite;

done:
    VRegInternalWString_Free(&className);
    VRegHiveInternalRegistryLower_Close(pHive, hKey);
    return status;
}

static LSTATUS WINAPI VRegHiveInternalRegistryLower_EnumValue(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _In_ const DWORD dwIndex,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegHiveInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    VRegInternalWString sizeName = {0, 0, NULL};
    DWORD cchName;
    HKEY hKey;
    LSTATUS status = VRegHiveInternalRegistryLower_Open(pHive, pNode, L"", KEY_QUERY_VALUE, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg_InternalEnumValueOwned(pRegistry, hKey, dwIndex, pResult->pName, pResult->pdwType, pResult->pData);
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
    if (!VRegInternalWString_Resize(&sizeName, pResult->pName->cch))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    cchName = (DWORD)(pResult->pName->cch + 1);
    *pResult->pcbData = 0;
    status = VReg_RegEnumValueW(
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
    VRegInternalWString_Free(&sizeName);
    VRegHiveInternalRegistryLower_Close(pHive, hKey);
    return status;
}

static LSTATUS WINAPI VRegHiveInternalRegistryLower_QueryInfo(
    _In_ VRegHive* const pHive,
    _In_ VRegHiveInternalNode* const pNode,
    _Inout_ VRegHiveResult* const pResult
)
{
    VReg* const pRegistry = ((const VRegHiveInternalRegistryLower*)pHive->pvLowerContext)->pRegistry;
    VRegInternalKeyInfo* const pInfo = pResult->pInfo;
    HKEY hKey;
    LSTATUS status = VRegHiveInternalRegistryLower_Open(pHive, pNode, L"", KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hKey);
    if (status != ERROR_SUCCESS)
        return status;

    _Analysis_assume_(pInfo);
    status = VReg_InternalQueryInfoKeyOwned(
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
    VRegHiveInternalRegistryLower_Close(pHive, hKey);
    return status;
}

static void WINAPI VRegHiveInternalRegistryLower_Destroy(
    _In_ VRegHive* const pHive
)
{
    VRegHiveInternalRegistryLower* const pLower = (VRegHiveInternalRegistryLower*)pHive->pvLowerContext;
    if (!pLower)
        return;

    VRegInternalWString_Free(&pLower->path);
    VReg_InternalFree(pLower);
}

// Copy-up never writes through to the lower, and nothing asks the lower to
// check a key, so those slots stay null.
static const VRegHiveVtbl g_VRegHiveVtbl_RegistryLower = {
    &VRegHiveInternalRegistryLower_OpenKey,
    NULL,
    NULL,
    NULL,
    &VRegHiveInternalRegistryLower_QueryValue,
    NULL,
    NULL,
    &VRegHiveInternalRegistryLower_EnumKey,
    &VRegHiveInternalRegistryLower_EnumValue,
    &VRegHiveInternalRegistryLower_QueryInfo,
    NULL,
    &VRegHiveInternalRegistryLower_Destroy,
};

VREG_DECL LSTATUS WINAPI VRegHive_CreateOverlay(
    _In_ VReg* const pLower,
    _In_ const HKEY hLowerRoot,
    _In_opt_ const LPCWSTR pszLowerPath,
    _In_ const REGSAM samLowerView,
    _Outptr_ VRegHive** const ppHive
)
{
    const VRegInternalWStringView lowerPath = VReg_InternalViewFromText(pszLowerPath);
    VRegHiveInternalRegistryLower* const pContext = (VRegHiveInternalRegistryLower*)VReg_InternalAllocZeroed(sizeof(VRegHiveInternalRegistryLower));
    LSTATUS status;
    *ppHive = NULL;
    if (!pContext)
        return ERROR_OUTOFMEMORY;

    pContext->pRegistry = pLower;
    pContext->hRoot = hLowerRoot;
    pContext->samView = samLowerView;
    if (!VRegInternalWString_Assign(&pContext->path, lowerPath.pch, lowerPath.cch))
    {
        VReg_InternalFree(pContext);
        return ERROR_OUTOFMEMORY;
    }

    status = VRegHive_InternalCreate(&g_VRegHiveVtbl_RegistryLower, pContext, VREG_INTERNAL_HIVE_OVERLAY, ppHive);
    if (status != ERROR_SUCCESS)
    {
        VRegInternalWString_Free(&pContext->path);
        VReg_InternalFree(pContext);
        return status;
    }

    (*ppHive)->pfnOpenLower = &VRegHiveInternalRegistryLower_OpenAs;
    return ERROR_SUCCESS;
}

// ==========================================================================
// The native registry
// ==========================================================================

// The table resolves from the export table of `KernelBase.dll`, which holds
// the bodies: advapi32's `Ex` exports only forward to them, and callers that
// import the API set (`api-ms-win-core-registry-l1-1-0`, as system DLLs and
// `onecore.lib` builds do) reach KernelBase without passing through
// advapi32, so only a hook on the body sees every caller. That KernelBase
// hosts the API set is not documented; `test/test_native_registry.cpp`
// checks the forwarding against the running system. `advapi32.dll` alone
// exports the pre-`Ex` forms, so resolution falls back to it. Never
// @ref GetProcAddress: an apphelp shim can make it answer a stub instead of
// the entry point. Once the hooks are committed the table holds the
// trampolines, so the registry is the one below them; the trampolines of
// the ANSI and pre-`Ex` hooks, which no registry calls, stay with the hooks.

VREG_DECL LSTATUS WINAPI VReg_CreateNative(
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VReg_InternalAllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->kind = VREG_INTERNAL_KIND_NATIVE;

    // A resolved export is a plain code address, which only a data pointer
    // cast can carry into the typed slot.
#define VREG_RESOLVE(name) *(void**)&pRegistry->implementation.native.name = VReg_InternalFindRegistryExport(#name);
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_RESOLVE)
#undef VREG_RESOLVE

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL _Ret_notnull_ VRegInternalNativeFunctions* VReg_InternalGetNativeFunctions(
    _In_ VReg* const pRegistry
)
{
    return &pRegistry->implementation.native;
}

// ==========================================================================
// Owned reads
// ==========================================================================

VREG_DECL DWORD VReg_InternalGrow(const DWORD cbOld, const DWORD cbRequired)
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}

VREG_DECL LSTATUS VReg_InternalQueryValueOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Out_opt_ const LPDWORD lpType,
    _Inout_ VRegInternalBuffer* const pData
)
{
    DWORD dwType = 0;
    DWORD cbData = 0;
    const LSTATUS sized = VReg_RegQueryValueExW(
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
        if (!VRegInternalBuffer_Resize(pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VReg_RegQueryValueExW(
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
            cbData = VReg_InternalGrow(cbData, cbRead);
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

VREG_DECL LSTATUS VReg_InternalGetValueOwned(
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
    const LSTATUS sized = VReg_RegGetValueW(
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
        if (!VRegInternalBuffer_Resize(pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VReg_RegGetValueW(
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
            cbData = VReg_InternalGrow(cbData, cbRead);
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

VREG_DECL LSTATUS VReg_InternalEnumKeyOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Inout_ VRegInternalWString* const pName,
    _Inout_opt_ VRegInternalWString* const pClass,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const PFILETIME lpftLastWriteTime
)
{
    DWORD cchName = VREG_INTERNAL_INITIAL_NAME_CHARS;
    DWORD cchClass = pClass ? VREG_INTERNAL_INITIAL_NAME_CHARS : 0;
    for (;;)
    {
        DWORD cchNameRead = cchName;
        DWORD cchClassRead = cchClass;
        LSTATUS status;
        if (!VRegInternalWString_Resize(pName, cchName) || (pClass && !VRegInternalWString_Resize(pClass, cchClass)))
            return ERROR_OUTOFMEMORY;

        status = VReg_RegEnumKeyExW(
            pRegistry,
            hKey,
            dwIndex,
            pName->psz,
            &cchNameRead,
            lpReserved,
            pClass ? pClass->psz : NULL,
            pClass ? &cchClassRead : NULL,
            lpftLastWriteTime
        );
        if (status == ERROR_MORE_DATA && (cchName < VREG_INTERNAL_MAX_CLASS_CHARS || (pClass && cchClass < VREG_INTERNAL_MAX_CLASS_CHARS)))
        {
            cchName = VReg_InternalMinDword(VReg_InternalGrow(cchName, cchNameRead), VREG_INTERNAL_MAX_CLASS_CHARS);
            if (pClass)
                cchClass = VReg_InternalMinDword(VReg_InternalGrow(cchClass, cchClassRead), VREG_INTERNAL_MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            VRegInternalWString_Truncate(pName, cchNameRead);
            if (pClass)
                VRegInternalWString_Truncate(pClass, cchClassRead);
        }
        return status;
    }
}

VREG_DECL LSTATUS VReg_InternalEnumValueOwned(
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
        if (!VRegInternalWString_Resize(pName, cchName))
            return ERROR_OUTOFMEMORY;

        status = VReg_RegEnumValueW(
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
            cchName = VReg_InternalMinDword(VReg_InternalGrow(cchName, cchRead + 1), VREG_INTERNAL_MAX_VALUE_NAME_CHARS);
            if (pData)
            {
                cbData = VReg_InternalGrow(cbData, cbRead);
                if (isSized && !VRegInternalBuffer_Resize(pData, cbData))
                    return ERROR_OUTOFMEMORY;
            }
            continue;
        }
        if (status == ERROR_SUCCESS && !isSized)
        {
            cbData = cbRead;
            if (!VRegInternalBuffer_Resize(pData, cbData))
                return ERROR_OUTOFMEMORY;

            isSized = TRUE;
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            VRegInternalWString_Truncate(pName, cchRead);
            if (pData)
                pData->cb = cbRead;
            if (lpType)
                *lpType = dwType;
        }
        return status;
    }
}

VREG_DECL LSTATUS VReg_InternalQueryInfoKeyOwned(
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
        if (!VRegInternalWString_Resize(pClass, cchClass))
            return ERROR_OUTOFMEMORY;

        status = VReg_RegQueryInfoKeyW(
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
            cchClass = VReg_InternalMinDword(VReg_InternalGrow(cchClass, cchRead), VREG_INTERNAL_MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VRegInternalWString_Truncate(pClass, cchRead);
        else if (lpcchClassReported && cchRead != cchClass)
            *lpcchClassReported = cchRead;
        return status;
    }
}

// ==========================================================================
// Paths
// ==========================================================================

VREG_DECL LSTATUS VRegInternalWStringView_SplitPath(
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
            if (!VRegInternalArray_Append(pComponents, sizeof(component), &component))
                return ERROR_OUTOFMEMORY;
        }

        start = end + 1;
    }

    return ERROR_SUCCESS;
}

// ==========================================================================
// The flat functions
// ==========================================================================

// A flat wide function calls the native registry's table directly, since
// that table has no slot for the object, and otherwise the slot of the
// registry's table: a custom registry's own, or @ref g_VRegVtbl_Virtual. Every
// other flat function is in "Derived forms". The slots take the registry,
// not the context (`fopencookie`'s callbacks take the cookie), so that they
// share the flat functions' typedefs; taking the context would need a
// second typedef per form. @ref VReg_CreateCustom names its kind as
// @ref VReg_CreateNative and @ref VReg_CreateVirtual do: a bare `VReg_Create`
// would read like a truncated `VReg_RegCreateKey`. The table holds the
// `Reg*` family and nothing else; the reads that size and fill an owned
// buffer themselves ("Owned reads") carry no `Reg` prefix, which would
// collide with `winreg.h` macros such as `RegQueryValue`.

VREG_DECL LSTATUS WINAPI VReg_RegOpenKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegOpenKeyExW(
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegOpenKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegOpenKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegCreateKeyExW(
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
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
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

    if (!VReg_InternalGetVtbl(pRegistry)->RegCreateKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegCreateKeyExW(
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

VREG_DECL LSTATUS WINAPI VReg_RegCloseKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegCloseKey(
            hKey
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegCloseKey)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegCloseKey(
        pRegistry,
        hKey
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegQueryValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegQueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegQueryValueExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegQueryValueExW(
        pRegistry,
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegSetValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegSetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegSetValueExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegSetValueExW(
        pRegistry,
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegDeleteValueW(
            hKey,
            lpValueName
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegDeleteValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegDeleteValueW(
        pRegistry,
        hKey,
        lpValueName
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegDeleteKeyExW(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegDeleteKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegDeleteTreeW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegDeleteTreeW(
            hKey,
            lpSubKey
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegDeleteTreeW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegDeleteTreeW(
        pRegistry,
        hKey,
        lpSubKey
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegEnumKeyExW(
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
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
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

    if (!VReg_InternalGetVtbl(pRegistry)->RegEnumKeyExW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegEnumKeyExW(
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

VREG_DECL LSTATUS WINAPI VReg_RegEnumValueW(
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
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
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

    if (!VReg_InternalGetVtbl(pRegistry)->RegEnumValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegEnumValueW(
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

VREG_DECL LSTATUS WINAPI VReg_RegQueryInfoKeyW(
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
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
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

    if (!VReg_InternalGetVtbl(pRegistry)->RegQueryInfoKeyW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegQueryInfoKeyW(
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

VREG_DECL LSTATUS WINAPI VReg_RegLoadMUIStringW(
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
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegLoadMUIStringW(
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegLoadMUIStringW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegLoadMUIStringW(
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
VREG_DECL LSTATUS WINAPI VReg_RegGetValueW(
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
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegGetValueW(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegGetValueW)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegGetValueW(
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

VREG_DECL LSTATUS WINAPI VReg_RegNotifyChangeKeyValue(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegNotifyChangeKeyValue(
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegNotifyChangeKeyValue)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegNotifyChangeKeyValue(
        pRegistry,
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

VREG_DECL LSTATUS WINAPI VReg_RegFlushKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (pRegistry->kind == VREG_INTERNAL_KIND_NATIVE)
        return pRegistry->implementation.native.RegFlushKey(
            hKey
        );

    if (!VReg_InternalGetVtbl(pRegistry)->RegFlushKey)
        return ERROR_CALL_NOT_IMPLEMENTED;

    return VReg_InternalGetVtbl(pRegistry)->RegFlushKey(
        pRegistry,
        hKey
    );
}

VREG_DECL void WINAPI VReg_Destroy(
    _In_opt_ VReg* const pRegistry
)
{
    if (!pRegistry)
        return;

    if (VReg_InternalGetVtbl(pRegistry)->Destroy)
        VReg_InternalGetVtbl(pRegistry)->Destroy(pRegistry);
    VReg_InternalFree(pRegistry);
}

VREG_DECL LSTATUS WINAPI VReg_CreateCustom(
    _In_ const VRegVtbl* const pVtbl,
    _In_opt_ void* const pvContext,
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VReg_InternalAllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->kind = VREG_INTERNAL_KIND_CUSTOM;
    pRegistry->implementation.custom.lpVtbl = pVtbl;
    pRegistry->implementation.custom.pvContext = pvContext;

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL _Ret_maybenull_ void* WINAPI VReg_GetContext(
    _In_ VReg* const pRegistry
)
{
    return pRegistry->kind == VREG_INTERNAL_KIND_CUSTOM
        ? pRegistry->implementation.custom.pvContext
        : NULL;
}

// ==========================================================================
// Status
// ==========================================================================

_Post_satisfies_(return != ERROR_SUCCESS)
VREG_DECL LSTATUS VReg_InternalFailureStatus(const LSTATUS status)
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    return status;
}

// ==========================================================================
// Value types
// ==========================================================================

VREG_DECL BOOL VReg_InternalIsStringType(const DWORD dwType)
{
    return dwType == REG_SZ
        || dwType == REG_EXPAND_SZ
        || dwType == REG_MULTI_SZ;
}

// ==========================================================================
// The virtual registry
// ==========================================================================

#include <sddl.h>

// The registry owns the caller-buffer rules (sizes in characters or bytes,
// terminators, @ref ERROR_MORE_DATA) and the ANSI and wide forms once, for
// every hive, which answers owned values through the node-level interface of
// "Hive internals". `<sddl.h>` is for the SID string of the process token,
// which names the native path of @ref HKEY_CURRENT_USER.

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
    VRegHiveInternalNode* pNode;

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

#define VREG_INTERNAL_PROCESS_VIEW \
    (sizeof(void*) == 8 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY)

#define VREG_INTERNAL_KEY_NAME_INFORMATION_CLASS    ((ULONG)3)
#define VREG_INTERNAL_NT_STATUS_BUFFER_OVERFLOW     ((LONG)0x80000005)
#define VREG_INTERNAL_NT_STATUS_BUFFER_TOO_SMALL    ((LONG)0xC0000023)

#define VREG_INTERNAL_KNOWN_NOTIFY_FILTERS (                                   \
    REG_NOTIFY_CHANGE_NAME |                                                   \
    REG_NOTIFY_CHANGE_ATTRIBUTES |                                             \
    REG_NOTIFY_CHANGE_LAST_SET |                                               \
    REG_NOTIFY_CHANGE_SECURITY |                                               \
    REG_NOTIFY_THREAD_AGNOSTIC                                                 \
)

/// @p psz, or an empty string for null: the hive layer takes no null names.
static LPCWSTR VReg_InternalOrEmpty(_In_opt_ const LPCWSTR psz)
{
    return psz ? psz : L"";
}

/// Copies a value into a caller buffer of @p cbBuffer bytes that is known to
/// hold it. Native adds a terminator after a string that lacks one, when it
/// fits, without counting it in the size.
static void VReg_InternalCopyValue(
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
    if (!VReg_InternalIsStringType(dwType))
        return;

    cbWhole = cbValue / sizeof(wchar_t) * sizeof(wchar_t);
    if (cbWhole == 0)
        return;

    isTerminated = pValue[cbWhole - 2] == 0 && pValue[cbWhole - 1] == 0;
    if (!isTerminated && cbWhole + sizeof(wchar_t) <= cbBuffer)
        ZeroMemory(pbBuffer + cbWhole, sizeof(wchar_t));
}

static BOOL VRegInternalWStringView_IsWow6432Node(const VRegInternalWStringView name)
{
    return VRegInternalWStringView_CompareNames(name, VReg_InternalViewFromText(L"WOW6432Node")) == 0;
}

/// The view a call with @p samDesired is of: the one it names, else the
/// process's.
static REGSAM VReg_InternalCallView(const REGSAM samDesired)
{
    if (samDesired & KEY_WOW64_64KEY)
        return KEY_WOW64_64KEY;
    if (samDesired & KEY_WOW64_32KEY)
        return KEY_WOW64_32KEY;

    return VREG_INTERNAL_PROCESS_VIEW;
}

/// Whether a mount of view @p samMount answers a call of view @p samCall.
static BOOL VReg_InternalAdmits(const REGSAM samMount, const REGSAM samCall)
{
    return samMount == 0 || samMount == samCall;
}

/// The string form of the current user's SID; empty when it cannot be read.
static LSTATUS VRegInternalWString_AssignCurrentUserSid(
    _Inout_ VRegInternalWString* pSid
)
{
    HANDLE hToken = NULL;
    DWORD cbUser = 0;
    VRegInternalBuffer user = {0, 0, NULL};
    LSTATUS status = ERROR_SUCCESS;

    if (!VRegInternalWString_Resize(pSid, 0))
        return ERROR_OUTOFMEMORY;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return ERROR_SUCCESS;

    GetTokenInformation(hToken, TokenUser, NULL, 0, &cbUser);
    if (cbUser != 0)
    {
        if (!VRegInternalBuffer_Resize(&user, cbUser))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (GetTokenInformation(hToken, TokenUser, user.pb, cbUser, &cbUser))
        {
            LPWSTR pszSid = NULL;
            if (ConvertSidToStringSidW(((const TOKEN_USER*)user.pb)->User.Sid, &pszSid))
            {
                if (!VRegInternalWString_Assign(pSid, pszSid, VReg_InternalViewFromText(pszSid).cch))
                    status = ERROR_OUTOFMEMORY;
                LocalFree(pszSid);
            }
        }
    }

    VRegInternalBuffer_Free(&user);
    CloseHandle(hToken);
    return status;
}

/// Appends the native path of a predefined root to @p pPath as components;
/// nothing for a root with none.
static LSTATUS VReg_InternalAppendNativeRootPath(
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
        status = VRegInternalWString_AssignCurrentUserSid(&sid);
        if (status == ERROR_SUCCESS && sid.cch != 0)
            pszSecond = L"USER";
    }

    if (status == ERROR_SUCCESS && pszSecond)
    {
        if (
            !VRegInternalArray_AppendWString(pPath, VReg_InternalViewFromText(L"REGISTRY"))
            || !VRegInternalArray_AppendWString(pPath, VReg_InternalViewFromText(pszSecond))
            || (sid.cch != 0 && !VRegInternalArray_AppendWString(pPath, VRegInternalWString_GetView(&sid)))
        )
            status = ERROR_OUTOFMEMORY;
    }

    VRegInternalWString_Free(&sid);
    return status;
}

/// Whether @p pFull, with `WOW6432Node` components skipped, starts with
/// @p pMountPath; @p piRest then indexes the first component past the match
/// and @p pIsWow says a `WOW6432Node` was skipped.
static BOOL VRegInternalArray_MatchesNative(
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
        const VRegInternalWStringView component = VREG_INTERNAL_AT(pFull, VRegInternalWStringView, i);
        if (VRegInternalWStringView_IsWow6432Node(component))
        {
            *pIsWow = TRUE;
            continue;
        }
        if (VRegInternalWStringView_CompareNames(component, VRegInternalArray_WStringViewAt(pMountPath, iMount)) != 0)
            return FALSE;

        ++iMount;
    }

    *piRest = i;
    return iMount == pMountPath->count;
}

// --- The mount table ---

/// The current table, or null while nothing is mounted. Lock-free.
static _Ret_maybenull_ const VRegInternalMountTable* VReg_InternalGetMounts(
    _In_ const VReg* const pThis
)
{
    return (const VRegInternalMountTable*)ReadPointerAcquire((PVOID volatile*)&pThis->implementation.virtual_.pMounts);
}

/// Whether @p pHive is mounted here.
static BOOL VReg_InternalHasMount(
    _In_ const VReg* const pThis,
    _In_ const VRegHive* const pHive
)
{
    const VRegInternalMountTable* const pTable = VReg_InternalGetMounts(pThis);
    SIZE_T i;
    if (!pTable)
        return FALSE;

    for (i = 0; i < pTable->mounts.count; ++i)
    {
        if (VREG_INTERNAL_AT(&pTable->mounts, VRegInternalMountPoint, i).pHive == pHive)
            return TRUE;
    }

    return FALSE;
}

static void VRegInternalMountPoint_Free(
    _Inout_ VRegInternalMountPoint* const pMount
)
{
    VRegInternalArray_FreeWStrings(&pMount->path);
    VRegInternalArray_FreeWStrings(&pMount->nativePath);
}

/// Whether the mount @p pMount is of @p hRoot at the path @p pPath, by the
/// registry's own name comparison.
static BOOL VRegInternalMountPoint_IsAt(
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
        const VRegInternalWStringView left = VRegInternalArray_WStringViewAt(&pMount->path, i);
        const VRegInternalWStringView right = VRegInternalArray_WStringViewAt(pPath, i);
        if (VRegInternalWStringView_CompareNames(left, right) != 0)
            return FALSE;
    }

    return TRUE;
}

/// Publishes a table holding the current mounts plus @p pMount, whose strings
/// it then owns. Under the mount lock.
static LSTATUS VReg_InternalPublishMount(
    _In_ VReg* const pThis,
    _In_ const VRegInternalMountPoint* const pMount
)
{
    VRegInternalMountTable* const pOld = pThis->implementation.virtual_.pMounts;
    VRegInternalMountTable* const pNew = (VRegInternalMountTable*)VReg_InternalAllocZeroed(sizeof(VRegInternalMountTable));
    SIZE_T i;
    if (!pNew)
        return ERROR_OUTOFMEMORY;

    for (i = 0; pOld && i < pOld->mounts.count; ++i)
    {
        const VRegInternalMountPoint* const pExisting = &VREG_INTERNAL_AT(&pOld->mounts, VRegInternalMountPoint, i);
        if (VRegInternalMountPoint_IsAt(pExisting, pMount->hRoot, &pMount->path))
        {
            VRegInternalArray_Free(&pNew->mounts);
            VReg_InternalFree(pNew);
            return ERROR_ALREADY_EXISTS;
        }
        if (!VRegInternalArray_Append(&pNew->mounts, sizeof(*pExisting), pExisting))
        {
            VRegInternalArray_Free(&pNew->mounts);
            VReg_InternalFree(pNew);
            return ERROR_OUTOFMEMORY;
        }
    }

    if (!VRegInternalArray_Append(&pNew->mounts, sizeof(*pMount), pMount))
    {
        VRegInternalArray_Free(&pNew->mounts);
        VReg_InternalFree(pNew);
        return ERROR_OUTOFMEMORY;
    }

    pNew->pPrevious = pOld;
    WritePointerRelease((PVOID volatile*)&pThis->implementation.virtual_.pMounts, pNew);
    return ERROR_SUCCESS;
}

// --- Resolution ---

// A call on a real parent handle is resolved by native path as
// @ref VReg_CreateVirtual documents, behind a gate: only a call whose subkey
// path has a component equal to the last component of some mount point is
// looked at, so real keys the mounts do not touch cost no syscall, and a
// mount at a root itself is not reachable this way. The path comes from
// @ref NtQueryKey (`KeyNameInformation`) through the registry's own pointer,
// which the tests replace; a failing query sends the call to the base, and
// a virtual handle of another copy is never queried. Native root names are
// `\REGISTRY\MACHINE`, `\REGISTRY\USER` and `\REGISTRY\USER\<SID>`,
// computed when the mount is added. `WOW6432Node` components are skipped
// while the path is matched against the mount point and are ordinary keys
// below it, since a hive's own tree is not a registry view; the longest
// matching mount wins. A call is of the view its `samDesired` names
// (`RRF_SUBKEY_WOW64*KEY` for `RegGetValueW`), else of the process bitness,
// and a native path that contained `WOW6432Node` is always the 32-bit view.
// Known gap: a real handle opened before the hooks committed that points at
// or inside a mount keeps reaching the lower registry for calls made on it
// directly; closing it would cost an @ref NtQueryKey on every call on a
// real handle.

static void VRegInternalTarget_Free(_Inout_ VRegInternalTarget* const pTarget)
{
    VRegInternalArray_Free(&pTarget->components);
    VRegInternalArray_Free(&pTarget->full);
    VRegInternalWString_Free(&pTarget->nativeText);
}

/// Looks for a mount a real key plus the subkey components lands in, by
/// native path; fills @p pTarget and sets @p pIsResolved when it does.
static LSTATUS VReg_InternalResolveNative(
    _In_ const VReg* const pThis,
    const HKEY hKey,
    const REGSAM samDesired,
    _Inout_ VRegInternalTarget* const pTarget,
    _Out_ BOOL* const pIsResolved
)
{
    const VRegInternalMountTable* const pTable = VReg_InternalGetMounts(pThis);
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
        const VRegInternalMountPoint* const pMount = &VREG_INTERNAL_AT(&pTable->mounts, VRegInternalMountPoint, i);
        SIZE_T j;
        if (pMount->path.count == 0)
            continue;

        for (j = 0; !isGated && j < pTarget->components.count; ++j)
        {
            const VRegInternalWStringView last = VRegInternalArray_WStringViewAt(&pMount->path, pMount->path.count - 1);
            isGated =
                VRegInternalWStringView_CompareNames(VREG_INTERNAL_AT(&pTarget->components, VRegInternalWStringView, j), last)
                == 0;
        }
    }
    if (!isGated || !pThis->implementation.virtual_.pfnNtQueryKey || VReg_IsVirtualHKey(hKey))
        return ERROR_SUCCESS;

    // The name is a KEY_NAME_INFORMATION: a byte count, then the characters.
    if (!VRegInternalBuffer_Resize(&buffer, 1024))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        ULONG cbResult = 0;
        const LONG ntStatus = pThis->implementation.virtual_.pfnNtQueryKey(
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
        if (!VRegInternalBuffer_Resize(&buffer, cbResult))
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
        !VRegInternalWString_Assign(
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
            if (!VRegInternalArray_Append(&pTarget->full, sizeof(component), &component))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
        }

        start = end + 1;
    }

    for (i = 0; i < pTarget->components.count; ++i)
    {
        const VRegInternalWStringView* const pComponent =
            &VREG_INTERNAL_AT(&pTarget->components, VRegInternalWStringView, i);
        if (!VRegInternalArray_Append(&pTarget->full, sizeof(*pComponent), pComponent))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    samCall = VReg_InternalCallView(samDesired);
    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VRegInternalMountPoint* const pMount = &VREG_INTERNAL_AT(&pTable->mounts, VRegInternalMountPoint, i);
        BOOL isWow = FALSE;
        SIZE_T iRest = 0;
        if (
            pMount->nativePath.count == 0
            || !VRegInternalArray_MatchesNative(&pMount->nativePath, &pTarget->full, &isWow, &iRest)
            || !VReg_InternalAdmits(pMount->samView, isWow ? KEY_WOW64_32KEY : samCall)
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
        pTarget->pNode = VRegHive_InternalGetRoot(pBest->pHive);
        pTarget->pRest = (const VRegInternalWStringView*)pTarget->full.p + iBestRest;
        pTarget->cRest = pTarget->full.count - iBestRest;
        *pIsResolved = TRUE;
    }

done:
    VRegInternalBuffer_Free(&buffer);
    return status;
}

/// Resolves @p hKey and @p subKey (empty for the key itself). @p samDesired
/// supplies the registry view of the call. Answers a status only for a
/// failure of its own (memory); what the call should answer is in the target.
static LSTATUS VReg_InternalResolve(
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
    VRegHiveInternalNode* pNode;

    ZeroMemory(pTarget, sizeof(*pTarget));
    splitStatus = VRegInternalWStringView_SplitPath(subKey, &pTarget->components);
    if (splitStatus == ERROR_OUTOFMEMORY)
        return splitStatus;

    isSplit = splitStatus == ERROR_SUCCESS;
    pNode = VRegHive_InternalFindNode(hKey);
    if (pNode)
    {
        if (VReg_InternalHasMount(pThis, pNode->pHive))
        {
            if (isSplit)
            {
                pTarget->pNode = pNode;
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
    else if (VRegInternalHandleSpace_Owns(VReg_InternalGetProcessHandleSpace(), hKey))
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (VReg_InternalIsPredefinedHKey(hKey))
    {
        const REGSAM samCall = VReg_InternalCallView(samDesired);
        const VRegInternalMountTable* const pTable = VReg_InternalGetMounts(pThis);
        const VRegInternalMountPoint* pBest = NULL;
        SIZE_T i;
        for (i = 0; pTable && isSplit && i < pTable->mounts.count; ++i)
        {
            const VRegInternalMountPoint* const pMount = &VREG_INTERNAL_AT(&pTable->mounts, VRegInternalMountPoint, i);
            SIZE_T j;
            BOOL isPrefix = TRUE;
            if (
                pMount->hRoot != hKey
                || pMount->path.count > pTarget->components.count
                || !VReg_InternalAdmits(pMount->samView, samCall)
            )
                continue;

            for (j = 0; isPrefix && j < pMount->path.count; ++j)
            {
                const VRegInternalWStringView mountName = VRegInternalArray_WStringViewAt(&pMount->path, j);
                isPrefix =
                    VRegInternalWStringView_CompareNames(mountName, VREG_INTERNAL_AT(&pTarget->components, VRegInternalWStringView, j))
                    == 0;
            }
            if (!isPrefix)
                continue;

            if (!pBest || pMount->path.count > pBest->path.count)
                pBest = pMount;
        }

        if (pBest)
        {
            pTarget->pNode = VRegHive_InternalGetRoot(pBest->pHive);
            pTarget->pRest = (const VRegInternalWStringView*)pTarget->components.p + pBest->path.count;
            pTarget->cRest = pTarget->components.count - pBest->path.count;
            return ERROR_SUCCESS;
        }

        if (pThis->implementation.virtual_.pBase)
            pTarget->isBase = TRUE;
        else
            pTarget->status = isSplit ? ERROR_FILE_NOT_FOUND : splitStatus;

        return ERROR_SUCCESS;
    }

    if (!pThis->implementation.virtual_.pBase)
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (isSplit)
    {
        const LSTATUS status = VReg_InternalResolveNative(pThis, hKey, samDesired, pTarget, &isResolved);
        if (status != ERROR_SUCCESS)
            return status;
    }

    if (!isResolved)
        pTarget->isBase = TRUE;

    return ERROR_SUCCESS;
}

// --- The slots ---

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegOpenKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    VRegInternalTarget target;
    VRegHiveInternalOpenResult result;
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegOpenKeyExW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }
    if (!samDesired)
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VRegHive_InternalOpenKey(target.pNode->pHive, target.pNode, target.pRest, target.cRest, samDesired, &result);
    if (status == ERROR_SUCCESS)
        *phkResult = result.redirect.hKey ? result.redirect.hKey : VRegHiveInternalNode_GetHandle(result.pNode);

done:
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegCreateKeyExW(
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
    VRegHiveInternalOpenResult result;
    DWORD dwDisposition = REG_OPENED_EXISTING_KEY;
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegCreateKeyExW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    *phkResult = NULL;
    if (!target.pNode)
    {
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }
    if (dwOptions & ~(DWORD)0x1F)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    status = VRegHive_InternalCreateKey(
        target.pNode->pHive,
        target.pNode,
        target.pRest,
        target.cRest,
        VReg_InternalOrEmpty(lpClass),
        dwOptions,
        samDesired,
        &result,
        &dwDisposition
    );
    if (status != ERROR_SUCCESS)
    {
        status = VReg_InternalFailureStatus(status);
        goto done;
    }

    *phkResult = result.redirect.hKey ? result.redirect.hKey : VRegHiveInternalNode_GetHandle(result.pNode);
    if (lpdwDisposition)
        *lpdwDisposition = dwDisposition;

done:
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegCloseKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (VRegHive_InternalFindNode(hKey) || VReg_InternalIsPredefinedHKey(hKey))
        return ERROR_SUCCESS;
    if (VRegInternalHandleSpace_Owns(VReg_InternalGetProcessHandleSpace(), hKey) || !pRegistry->implementation.virtual_.pBase)
        return ERROR_INVALID_HANDLE;

    return VReg_RegCloseKey(
        pRegistry->implementation.virtual_.pBase,
        hKey
    );
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegQueryValueExW(
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
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegQueryValueExW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VRegHive_InternalQueryValue(
        target.pNode->pHive,
        target.pNode,
        VReg_InternalOrEmpty(lpValueName),
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

    VReg_InternalCopyValue(lpData, cbBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    VRegInternalBuffer_Free(&value);
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegSetValueExW(
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
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegSetValueExW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }
    if (!lpData && cbData)
    {
        status = ERROR_NOACCESS;
        goto done;
    }

    status = VRegHive_InternalSetValue(
        target.pNode->pHive,
        target.pNode,
        VReg_InternalOrEmpty(lpValueName),
        dwType,
        lpData,
        lpData ? cbData : 0
    );

done:
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegDeleteValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    VRegInternalTarget target;
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegDeleteValueW(
            pRegistry->implementation.virtual_.pBase,
            hKey,
            lpValueName
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    status = VRegHive_InternalDeleteValue(target.pNode->pHive, target.pNode, VReg_InternalOrEmpty(lpValueName));

done:
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegDeleteKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    VRegInternalTarget target;
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegDeleteKeyExW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    status = VRegHive_InternalDeleteKey(target.pNode->pHive, target.pNode, target.pRest, target.cRest);

done:
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegDeleteTreeW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    VRegInternalTarget target;
    VRegHiveInternalOpenResult result;
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(lpSubKey), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegDeleteTreeW(
            pRegistry->implementation.virtual_.pBase,
            hKey,
            lpSubKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    status = VRegHive_InternalOpenKey(target.pNode->pHive, target.pNode, target.pRest, target.cRest, KEY_ALL_ACCESS, &result);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only a frozen hive redirects, and it refuses every write.
    if (result.redirect.hKey)
    {
        VRegInternalRedirect_Close(&result.redirect);
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VRegHive_InternalDeleteTree(target.pNode->pHive, result.pNode);
    if (status != ERROR_SUCCESS || target.cRest == 0)
        goto done;

    status = VRegHive_InternalDeleteKey(target.pNode->pHive, target.pNode, target.pRest, target.cRest);

done:
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegEnumKeyExW(
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
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    ZeroMemory(&lastWrite, sizeof(lastWrite));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegEnumKeyExW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    cchClassBuffer = lpClass ? *lpcchClass : 0;
    if (lpcchClass)
        *lpcchClass = 0;

    status = VRegHive_InternalEnumKey(target.pNode->pHive, target.pNode, dwIndex, &name, &className, &lastWrite);
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

    CopyMemory(lpName, VRegInternalWString_GetText(&name), (name.cch + 1) * sizeof(wchar_t));
    *lpcchName = (DWORD)name.cch;
    if (lpClass)
    {
        CopyMemory(lpClass, VRegInternalWString_GetText(&className), (className.cch + 1) * sizeof(wchar_t));
        *lpcchClass = (DWORD)className.cch;
    }

done:
    VRegInternalWString_Free(&name);
    VRegInternalWString_Free(&className);
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegEnumValueW(
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
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegEnumValueW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VRegHive_InternalEnumValue(
        target.pNode->pHive,
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
        CopyMemory(lpValueName, VRegInternalWString_GetText(&name), (name.cch + 1) * sizeof(wchar_t));
        *lpcchValueName = (DWORD)name.cch;
    }

    if (!isNameFit || !isDataFit)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    if (lpData)
        VReg_InternalCopyValue(lpData, cbDataBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    VRegInternalWString_Free(&name);
    VRegInternalBuffer_Free(&value);
    VRegInternalTarget_Free(&target);
    return status;
}

// Native answers success without writing lpClass when *lpcchClass is zero,
// which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegQueryInfoKeyW(
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
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    ZeroMemory(&info, sizeof(info));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegQueryInfoKeyW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    status = VRegHive_InternalQueryInfo(target.pNode->pHive, target.pNode, &info);
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

        CopyMemory(lpClass, VRegInternalWString_GetText(&info.className), (cchClass + 1) * sizeof(wchar_t));
    }

    if (lpClass && lpcchClass && (*lpcchClass != 0 || lpcbMaxClassLen))
        *lpcchClass = cchClass;
    else if (!lpClass && lpcchClass)
        *lpcchClass = 0;

done:
    VRegInternalWString_Free(&info.className);
    VRegInternalTarget_Free(&target);
    return status;
}

/// Resolves the indirect string `@<module>,-<id>` in @p pData as
/// `RegLoadMUIStringW` does natively: the module path with environment
/// strings expanded, joined to @p pszDirectory when one is given and taken
/// from the current directory otherwise, never from the search path; then
/// string `id` of the module's string table in the user's UI language. Data
/// that is no such string answers @ref ERROR_INVALID_DATA.
static LSTATUS VRegInternalBuffer_LoadIndirectString(
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

    if (!VRegInternalWString_Assign(&module, pch + 1, iComma - 1))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    cchNeeded = ExpandEnvironmentStringsW(VRegInternalWString_GetText(&module), NULL, 0);
    if (cchNeeded == 0)
    {
        status = (LSTATUS)GetLastError();
        goto done;
    }
    if (!VRegInternalWString_Resize(&expanded, cchNeeded - 1))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    cchWritten = ExpandEnvironmentStringsW(VRegInternalWString_GetText(&module), expanded.psz, cchNeeded);
    if (cchWritten == 0 || cchWritten > cchNeeded)
    {
        status = cchWritten == 0 ? (LSTATUS)GetLastError() : ERROR_INSUFFICIENT_BUFFER;
        goto done;
    }
    VRegInternalWString_Truncate(&expanded, cchWritten - 1);

    if (pszDirectory)
    {
        const VRegInternalWStringView directory = VReg_InternalViewFromText(pszDirectory);
        if (
            !VRegInternalWString_Assign(&path, directory.pch, directory.cch)
            || !VRegInternalWString_AppendChar(&path, L'\\')
            || !VRegInternalWString_Append(&path, VRegInternalWString_GetText(&expanded), expanded.cch)
        )
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }
    else
    {
        // An empty module path has no full path; natively it is not found.
        cchNeeded = GetFullPathNameW(VRegInternalWString_GetText(&expanded), 0, NULL, NULL);
        if (cchNeeded == 0)
        {
            status = ERROR_FILE_NOT_FOUND;
            goto done;
        }
        if (!VRegInternalWString_Resize(&path, cchNeeded - 1))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
        cchWritten = GetFullPathNameW(VRegInternalWString_GetText(&expanded), cchNeeded, path.psz, NULL);
        if (cchWritten == 0 || cchWritten >= cchNeeded)
        {
            status = cchWritten == 0 ? (LSTATUS)GetLastError() : ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }
        VRegInternalWString_Truncate(&path, cchWritten);
    }

    // The one module load in a hook body: natively RegLoadMUIStringW loads
    // the module on the caller's thread itself, so the load is the call's own
    // documented effect, not one VReg adds.
    hModule = LoadLibraryExW(
        VRegInternalWString_GetText(&path),
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
    if (!VRegInternalWString_Assign(pText, (LPCWCH)(pBlock + iWord + 1), pBlock[iWord]))
        status = ERROR_OUTOFMEMORY;

done:
    if (hModule)
        FreeLibrary(hModule);
    VRegInternalWString_Free(&path);
    VRegInternalWString_Free(&expanded);
    VRegInternalWString_Free(&module);
    return status;
}

// Native leaves pszOutBuf untouched on failure, which the terminator its
// LPWSTR type implies does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegLoadMUIStringW(
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
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegLoadMUIStringW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
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
    status = VRegHive_InternalQueryValue(target.pNode->pHive, target.pNode, VReg_InternalOrEmpty(pszValue), &dwType, &cbData, &data);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegInternalBuffer_LoadIndirectString(&data, dwType, pszDirectory, &text);
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
        CopyMemory(pszOutBuf, VRegInternalWString_GetText(&text), cch * sizeof(wchar_t));
        pszOutBuf[cch] = L'\0';
        goto done;
    }

    *pcbData = cbText;
    if (!pszOutBuf || cbOutBuf < cbText)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    CopyMemory(pszOutBuf, VRegInternalWString_GetText(&text), cbText);

done:
    VRegInternalBuffer_Free(&data);
    VRegInternalWString_Free(&text);
    VRegInternalTarget_Free(&target);
    return status;
}

/// Answers the failure @p status after clearing the caller's buffer when the
/// call asked for that on failure.
_Post_satisfies_(return != ERROR_SUCCESS)
static LSTATUS VReg_InternalFailGetValue(
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
static BOOL VRegInternalBuffer_IsNulAt(
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
VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegGetValueW(
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
    VRegHiveInternalOpenResult result;
    VRegHiveInternalNode* pNode;
    DWORD cbBuffer;
    DWORD dwType = 0;
    DWORD cbQueried = 0;
    BOOL isString;

    // Native copies the stored bytes first, then the string with the
    // terminators it lacks, then the expansion, each over the previous one and
    // only if it fits.
    VRegInternalBuffer stages[3] = {
        {0, 0, NULL},
        {0, 0, NULL},
        {0, 0, NULL},
    };
    DWORD cbReported;
    DWORD dwTypeFlag;
    BOOL isAllowed;
    const VRegInternalBuffer* pFinal;
    BOOL isFit;
    BOOL isSizeMismatch;
    SIZE_T i;
    LSTATUS status = VReg_InternalResolve(
        pRegistry,
        hkey,
        VReg_InternalViewFromText(lpSubKey),
        (dwFlags & VREG_INTERNAL_RRF_WOW6464_SUBKEY ? KEY_WOW64_64KEY : 0)
        | (dwFlags & VREG_INTERNAL_RRF_WOW6432_SUBKEY ? KEY_WOW64_32KEY : 0),
        &target
    );
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegGetValueW(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailGetValue(pvData, cbBuffer, dwFlags, target.status);
        goto done;
    }

    pNode = target.pNode;
    if (target.cRest != 0)
    {
        status = VRegHive_InternalOpenKey(target.pNode->pHive, target.pNode, target.pRest, target.cRest, KEY_QUERY_VALUE, &result);
        if (status != ERROR_SUCCESS)
        {
            status = VReg_InternalFailGetValue(pvData, cbBuffer, dwFlags, status);
            goto done;
        }

        if (result.redirect.hKey)
        {
            // A redirect always names the registry that answers its key.
            _Analysis_assume_(result.redirect.pRegistry);
            status = VReg_RegGetValueW(
                result.redirect.pRegistry,
                result.redirect.hKey,
                NULL,
                lpValue,
                dwFlags,
                pdwType,
                pvData,
                pcbData
            );
            VRegInternalRedirect_Close(&result.redirect);
            goto done;
        }

        pNode = result.pNode;
    }

    if (pdwType)
        *pdwType = 0;
    if (pcbData && !pvData)
        *pcbData = 0;

    status = VRegHive_InternalQueryValue(target.pNode->pHive, pNode, VReg_InternalOrEmpty(lpValue), &dwType, &cbQueried, &stages[0]);
    if (status != ERROR_SUCCESS)
    {
        status = VReg_InternalFailGetValue(pvData, cbBuffer, dwFlags, status);
        goto done;
    }

    isString = VReg_InternalIsStringType(dwType);
    cbReported = (DWORD)stages[0].cb;
    if (isString)
    {
        const SIZE_T cwch = stages[0].cb / sizeof(wchar_t);
        SIZE_T cwchAdded = 0;
        static const wchar_t ZEROS[2] = {L'\0', L'\0'};
        if (cwch == 0 || !VRegInternalBuffer_IsNulAt(&stages[0], cwch - 1))
            cwchAdded = dwType == REG_MULTI_SZ ? 2 : 1;
        else if (dwType == REG_MULTI_SZ && (cwch == 1 || !VRegInternalBuffer_IsNulAt(&stages[0], cwch - 2)))
            cwchAdded = 1;

        if (
            !VRegInternalBuffer_Assign(&stages[1], stages[0].pb, cwch * sizeof(wchar_t))
            || !VRegInternalBuffer_Append(&stages[1], ZEROS, cwchAdded * sizeof(wchar_t))
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
            if (!VRegInternalBuffer_Resize(&stages[2], (SIZE_T)cwchExpanded * sizeof(wchar_t)))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
            if (cwchExpanded != 0)
                ExpandEnvironmentStringsW(pszRaw, (wchar_t*)stages[2].pb, cwchExpanded);

            cbReported = VReg_InternalMaxDword(cbReported, (DWORD)stages[2].cb);
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
            *pcbData = isFit ? cbReported : VReg_InternalMaxDword(cbReported, cbReserved);
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
        status = VReg_InternalFailGetValue(pvData, cbBuffer, dwFlags, status);

done:
    for (i = 0; i < 3; ++i)
        VRegInternalBuffer_Free(&stages[i]);
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegNotifyChangeKeyValue(
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
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegNotifyChangeKeyValue(
            pRegistry->implementation.virtual_.pBase,
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
        status = VReg_InternalFailureStatus(target.status);
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
        status = VRegHive_InternalWatch(target.pNode->pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hEvent);
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

    status = VRegHive_InternalWatch(target.pNode->pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hOwn);
    if (status == ERROR_SUCCESS)
        WaitForSingleObject(hOwn, INFINITE);

    CloseHandle(hOwn);

done:
    VRegInternalTarget_Free(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_InternalVirtualRegFlushKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    VRegInternalTarget target;
    LSTATUS status = VReg_InternalResolve(pRegistry, hKey, VReg_InternalViewFromText(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VReg_RegFlushKey(
            pRegistry->implementation.virtual_.pBase,
            hKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg_InternalFailureStatus(target.status);
        goto done;
    }

    status = VRegHive_InternalCheckKey(target.pNode->pHive, target.pNode);

done:
    VRegInternalTarget_Free(&target);
    return status;
}

// --- Creation and destruction ---

VREG_DECL void WINAPI VReg_InternalVirtualDestroy(_In_ VReg* const pRegistry)
{
    VRegInternalMountTable* pTable = pRegistry->implementation.virtual_.pMounts;
    BOOL isNewest = TRUE;
    while (pTable)
    {
        VRegInternalMountTable* const pPrevious = pTable->pPrevious;
        if (isNewest)
        {
            SIZE_T i;
            for (i = 0; i < pTable->mounts.count; ++i)
                VRegInternalMountPoint_Free(&VREG_INTERNAL_AT(&pTable->mounts, VRegInternalMountPoint, i));
        }

        VRegInternalArray_Free(&pTable->mounts);
        VReg_InternalFree(pTable);
        pTable = pPrevious;
        isNewest = FALSE;
    }
}

VREG_DECL LSTATUS WINAPI VReg_CreateVirtual(
    _In_opt_ VReg* const pBase,
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VReg_InternalAllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->kind = VREG_INTERNAL_KIND_VIRTUAL;
    pRegistry->implementation.virtual_.pBase = pBase;
    InitializeSRWLock(&pRegistry->implementation.virtual_.mountLock);
    *(void**)&pRegistry->implementation.virtual_.pfnNtQueryKey =
        VReg_InternalFindExport(GetModuleHandleW(L"ntdll.dll"), "NtQueryKey");

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

    if (pRegistry->kind != VREG_INTERNAL_KIND_VIRTUAL || !VReg_InternalIsPredefinedHKey(hRoot))
        return ERROR_INVALID_HANDLE;
    if (samView != 0 && samView != KEY_WOW64_64KEY && samView != KEY_WOW64_32KEY)
        return ERROR_INVALID_PARAMETER;

    ZeroMemory(&mount, sizeof(mount));
    mount.hRoot = hRoot;
    mount.pHive = pHive;
    mount.samView = samView;

    status = VRegInternalWStringView_SplitPath(VReg_InternalViewFromText(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VRegInternalArray_Free(&components);
        return status == ERROR_OUTOFMEMORY ? status : ERROR_BAD_PATHNAME;
    }

    for (i = 0; status == ERROR_SUCCESS && i < components.count; ++i)
    {
        if (!VRegInternalArray_AppendWString(&mount.path, VREG_INTERNAL_AT(&components, VRegInternalWStringView, i)))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VReg_InternalAppendNativeRootPath(hRoot, &mount.nativePath);
    for (i = 0; status == ERROR_SUCCESS && mount.nativePath.count != 0 && i < components.count; ++i)
    {
        const VRegInternalWStringView component = VREG_INTERNAL_AT(&components, VRegInternalWStringView, i);
        if (!VRegInternalWStringView_IsWow6432Node(component) && !VRegInternalArray_AppendWString(&mount.nativePath, component))
            status = ERROR_OUTOFMEMORY;
    }
    VRegInternalArray_Free(&components);

    if (status == ERROR_SUCCESS)
    {
        AcquireSRWLockExclusive(&pRegistry->implementation.virtual_.mountLock);
        status = VReg_InternalPublishMount(pRegistry, &mount);
        ReleaseSRWLockExclusive(&pRegistry->implementation.virtual_.mountLock);
    }

    if (status != ERROR_SUCCESS)
        VRegInternalMountPoint_Free(&mount);
    return status;
}

// ==========================================================================
// Applying scripts
// ==========================================================================

// The whole file parses before anything is applied, so a syntax error
// writes nothing; what applied before a failure stays applied. There is no
// rollback, since native registries have none without KTM, and a dry-run
// validation pass cannot catch access-denied. Stopping at the first failure
// is the default and continuing (what `regedit /s` does) the flag, since the
// stop reports exactly which operation failed; with the flag the first
// failure is the one reported, not the last, since later ones often follow
// from it. Through an overlay, deletions become tombstones and `[-K]` then
// `[K]` an opaque key; through any other registry they are real deletes.

static LSTATUS VRegScript_InternalIgnoreNotFound(const LSTATUS status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND
        ? ERROR_SUCCESS
        : status;
}

static LSTATUS VRegScript_InternalPerformSetValue(
    _In_ const VRegOperation* const pOperation,
    _In_ VReg* const pRegistry
)
{
    const BYTE* const pData = pOperation->pData;
    const DWORD cbData = pOperation->cbData;
    const BOOL isString = VReg_InternalIsStringType(pOperation->dwType);
    const BOOL isTerminated = cbData >= sizeof(wchar_t) && pData[cbData - 1] == 0 && pData[cbData - 2] == 0;
    const BYTE* pSource = pData;
    VRegInternalBuffer padded = {0, 0, NULL};
    HKEY hKey = NULL;
    LSTATUS status = VReg_RegOpenKeyExW(
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
        if (
            !VRegInternalBuffer_Assign(&padded, pData, cbData)
            || !VRegInternalBuffer_Append(&padded, PADDING, sizeof(PADDING))
        )
        {
            status = ERROR_OUTOFMEMORY;
        }
        pSource = padded.pb;
    }

    if (status == ERROR_SUCCESS)
    {
        status = VReg_RegSetValueExW(
            pRegistry,
            hKey,
            pOperation->pszName,
            0,
            pOperation->dwType,
            pSource,
            cbData
        );
    }

    VRegInternalBuffer_Free(&padded);
    VReg_InternalCloseUnless(pRegistry, hKey, NULL);
    return status;
}

static LSTATUS VRegScript_InternalPerform(
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
        status = VReg_RegCreateKeyExW(
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
        VReg_InternalCloseUnless(pRegistry, hKey, NULL);
        return status;

    case VREG_OPERATION_DELETE_KEY:
        status = VRegScript_InternalIgnoreNotFound(
            VReg_RegDeleteTreeW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath
            )
        );
        if (status != ERROR_SUCCESS)
            return status;

        return VRegScript_InternalIgnoreNotFound(
            VReg_RegDeleteKeyExW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath,
                0,
                0
            )
        );

    case VREG_OPERATION_SET_VALUE:
        return VRegScript_InternalPerformSetValue(pOperation, pRegistry);

    case VREG_OPERATION_DELETE_VALUE:
        status = VReg_RegOpenKeyExW(
            pRegistry,
            pOperation->hRoot,
            pOperation->pszPath,
            0,
            KEY_SET_VALUE,
            &hKey
        );
        if (status != ERROR_SUCCESS)
            return VRegScript_InternalIgnoreNotFound(status);

        status = VRegScript_InternalIgnoreNotFound(
            VReg_RegDeleteValueW(
                pRegistry,
                hKey,
                pOperation->pszName
            )
        );
        VReg_InternalCloseUnless(pRegistry, hKey, NULL);
        return status;

    default:
        return ERROR_INVALID_PARAMETER;
    }
}

_Success_(return == ERROR_SUCCESS)
VREG_DECL LSTATUS WINAPI VReg_ApplyOperations(
    _In_ VReg* const pRegistry,
    _In_reads_(cOperations) const VRegOperation* const pOperations,
    _In_ const SIZE_T cOperations,
    _In_ const DWORD dwFlags,
    _Pre_maybenull_ _Pre_writable_size_(1) SIZE_T* const pFailedIndex
)
{
    LSTATUS firstFailure = ERROR_SUCCESS;
    SIZE_T i;

    if (dwFlags & ~VREG_APPLY_CONTINUE_ON_ERROR)
        return ERROR_INVALID_PARAMETER;

    for (i = 0; i < cOperations; ++i)
    {
        const LSTATUS status = VRegScript_InternalPerform(&pOperations[i], pRegistry);
        if (status == ERROR_SUCCESS || firstFailure != ERROR_SUCCESS)
            continue;

        firstFailure = status;
        if (pFailedIndex)
            *pFailedIndex = i;
        if (!(dwFlags & VREG_APPLY_CONTINUE_ON_ERROR))
            break;
    }

    return firstFailure;
}

// ==========================================================================
// Snapshot and delta
// ==========================================================================

// Keys and values are written sorted ordinal case-insensitive: stable
// fixture diffs matter more than mirroring regedit's enumeration order. The
// delta walks @ref VRegHive_InternalSnapshotUpper, so opening keys does not
// change it; a key with upper values or value tombstones appears as `[K]`,
// which is harmless, since it exists below. A tombstone without a lower
// counterpart is kept, not pruned: the delta is the overlay, and the lower
// registry can gain that key later (another machine, an install), so the
// delta must still hide it.

/// A value read from a key, held until the key's values are all known and can
/// be appended in name order.
typedef struct
{
    VRegInternalWString name;
    DWORD dwType;
    VRegInternalBuffer data;
    BOOL isTombstone;
} VRegScriptInternalValueEntry;

/// Inserts @p pEntry into @p pValues, an array of
/// @ref VRegScriptInternalValueEntry sorted by name; the array owns the entry
/// from then on.
static BOOL VRegScript_InternalInsertValueSorted(
    _Inout_ VRegInternalArray* const pValues,
    _In_ const VRegScriptInternalValueEntry* const pEntry
)
{
    const VRegInternalWStringView name = VRegInternalWString_GetView(&pEntry->name);
    SIZE_T lo = 0;
    SIZE_T hi = pValues->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const VRegScriptInternalValueEntry* const pMid = &VREG_INTERNAL_AT(pValues, VRegScriptInternalValueEntry, mid);
        if (VRegInternalWStringView_CompareNames(VRegInternalWString_GetView(&pMid->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return VRegInternalArray_Insert(pValues, sizeof(*pEntry), lo, pEntry);
}

static void VRegScript_InternalFreeValues(
    _Inout_ VRegInternalArray* const pValues
)
{
    SIZE_T i;
    for (i = 0; i < pValues->count; ++i)
    {
        VRegScriptInternalValueEntry* const pEntry = &VREG_INTERNAL_AT(pValues, VRegScriptInternalValueEntry, i);
        VRegInternalWString_Free(&pEntry->name);
        VRegInternalBuffer_Free(&pEntry->data);
    }

    pValues->count = 0;
}

/// Appends the sorted values of a key as operations and empties the array.
static LSTATUS VRegScript_InternalFlushValues(
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
        const VRegScriptInternalValueEntry* const pEntry = &VREG_INTERNAL_AT(pValues, VRegScriptInternalValueEntry, i);
        status = VRegScript_InternalAppend(
            pScript,
            pEntry->isTombstone ? VREG_OPERATION_DELETE_VALUE : VREG_OPERATION_SET_VALUE,
            hRoot,
            path,
            VRegInternalWString_GetView(&pEntry->name),
            pEntry->dwType,
            pEntry->data.pb,
            (DWORD)pEntry->data.cb,
            0
        );
    }

    VRegScript_InternalFreeValues(pValues);
    return status;
}

/// Appends @p path to @p pPrefix with a backslash between, into @p pFull.
static BOOL VRegScript_InternalJoinKeyPath(
    _Inout_ VRegInternalWString* const pFull,
    const VRegInternalWStringView prefix,
    const VRegInternalWStringView path
)
{
    if (!VRegInternalWString_Assign(pFull, prefix.pch, prefix.cch))
        return FALSE;
    if (path.cch == 0)
        return TRUE;

    return VRegInternalWString_AppendPathComponent(pFull, path.pch, path.cch);
}

// --- Snapshot ---

/// Appends the key @p hKey at @p pPath below the root, then its values, then
/// its subkeys in order.
static LSTATUS VRegScript_InternalWalkKey(
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

    status = VRegScript_InternalAppend(
        pScript,
        VREG_OPERATION_CREATE_KEY,
        hRoot,
        VRegInternalWString_GetView(pPath),
        VReg_InternalViewFromText(L""),
        0,
        NULL,
        0,
        0
    );
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        VRegScriptInternalValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        status = VReg_InternalEnumValueOwned(pRegistry, hKey, dwIndex, &entry.name, &entry.dwType, &entry.data);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            VRegInternalWString_Free(&entry.name);
            VRegInternalBuffer_Free(&entry.data);
            status = ERROR_SUCCESS;
            break;
        }
        if (status == ERROR_SUCCESS && !VRegScript_InternalInsertValueSorted(&values, &entry))
            status = ERROR_OUTOFMEMORY;
        if (status != ERROR_SUCCESS)
        {
            VRegInternalWString_Free(&entry.name);
            VRegInternalBuffer_Free(&entry.data);
            goto done;
        }
    }

    status = VRegScript_InternalFlushValues(pScript, hRoot, VRegInternalWString_GetView(pPath), &values);
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        status = VReg_InternalEnumKeyOwned(pRegistry, hKey, dwIndex, &name, NULL, NULL, NULL);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VRegInternalArray_InsertWStringSorted(&subkeys, VRegInternalWString_GetView(&name)))
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
        status = VReg_RegOpenKeyExW(
            pRegistry,
            hKey,
            VRegInternalWString_GetText(pSubkey),
            REG_OPTION_OPEN_LINK,
            KEY_READ,
            &hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VRegScript_InternalJoinKeyPath(&childPath, VRegInternalWString_GetView(pPath), VRegInternalWString_GetView(pSubkey)))
            status = ERROR_OUTOFMEMORY;
        else
            status = VRegScript_InternalWalkKey(pScript, pRegistry, hRoot, hSubkey, &childPath, depth + 1);

        VReg_RegCloseKey(
            pRegistry,
            hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;
    }

done:
    VRegScript_InternalFreeValues(&values);
    VRegInternalArray_Free(&values);
    VRegInternalArray_FreeWStrings(&subkeys);
    VRegInternalWString_Free(&name);
    VRegInternalWString_Free(&childPath);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegScript_CreateSnapshot(
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
    if (!VRegScript_InternalGetRootName(hRoot))
        return ERROR_INVALID_HANDLE;

    status = VRegScript_InternalCreate(&pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg_RegOpenKeyExW(
        pRegistry,
        hRoot,
        pszPath ? pszPath : L"",
        0,
        KEY_READ,
        &hKey
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VRegInternalWString_Assign(&path, pszPath, VReg_InternalViewFromText(pszPath).cch))
        status = ERROR_OUTOFMEMORY;
    else
        status = VRegScript_InternalWalkKey(pScript, pRegistry, hRoot, hKey, &path, 0);

    VReg_RegCloseKey(
        pRegistry,
        hKey
    );

done:
    VRegInternalWString_Free(&path);
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
} VRegScriptInternalDeltaWalk;

static LSTATUS VRegScript_InternalVisitUpper(
    _In_ void* const pContext,
    _In_ const VRegHiveInternalUpperRecord* const pRecord
)
{
    VRegScriptInternalDeltaWalk* const pWalk = (VRegScriptInternalDeltaWalk*)pContext;
    LSTATUS status;

    if (pRecord->kind == VREG_INTERNAL_UPPER_VALUE)
    {
        VRegScriptInternalValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        entry.dwType = pRecord->dwType;
        entry.isTombstone = pRecord->isTombstone;
        if (
            !VRegInternalWString_Assign(&entry.name, pRecord->name.pch, pRecord->name.cch)
            || !VRegInternalBuffer_Assign(&entry.data, pRecord->pData, pRecord->cbData)
            || !VRegScript_InternalInsertValueSorted(&pWalk->values, &entry)
        )
        {
            VRegInternalWString_Free(&entry.name);
            VRegInternalBuffer_Free(&entry.data);
            return ERROR_OUTOFMEMORY;
        }

        return ERROR_SUCCESS;
    }

    status = VRegScript_InternalFlushValues(pWalk->pScript, pWalk->hRoot, VRegInternalWString_GetView(&pWalk->keyPath), &pWalk->values);
    if (status != ERROR_SUCCESS)
        return status;
    if (!VRegScript_InternalJoinKeyPath(&pWalk->keyPath, pWalk->prefix, pRecord->path))
        return ERROR_OUTOFMEMORY;

    if (pRecord->isTombstone || pRecord->isOpaque)
    {
        // Agent's choice: a root deletion is refused here, because no `.reg`
        // file can express it and Parse and Apply reject it. The alternative
        // passed over is defining it as emptying the root in Parse, Apply and
        // Write.
        if (VRegInternalWString_GetView(&pWalk->keyPath).cch == 0)
            return ERROR_NOT_SUPPORTED;

        status = VRegScript_InternalAppend(
            pWalk->pScript,
            VREG_OPERATION_DELETE_KEY,
            pWalk->hRoot,
            VRegInternalWString_GetView(&pWalk->keyPath),
            VReg_InternalViewFromText(L""),
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

    return VRegScript_InternalAppend(
        pWalk->pScript,
        VREG_OPERATION_CREATE_KEY,
        pWalk->hRoot,
        VRegInternalWString_GetView(&pWalk->keyPath),
        VReg_InternalViewFromText(L""),
        0,
        NULL,
        0,
        0
    );
}

VREG_DECL LSTATUS WINAPI VRegScript_CreateDelta(
    _In_ VRegHive* const pHive,
    _In_ const HKEY hRoot,
    _In_opt_ const LPCWSTR pszPath,
    _Outptr_ VRegScript** const ppScript
)
{
    VRegScriptInternalDeltaWalk walk;
    LSTATUS status;

    *ppScript = NULL;
    if (!VRegScript_InternalGetRootName(hRoot))
        return ERROR_INVALID_HANDLE;

    ZeroMemory(&walk, sizeof(walk));
    walk.hRoot = hRoot;
    walk.prefix = VReg_InternalViewFromText(pszPath);
    status = VRegScript_InternalCreate(&walk.pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VRegHive_InternalSnapshotUpper(pHive, &VRegScript_InternalVisitUpper, &walk);
    if (status == ERROR_SUCCESS)
        status = VRegScript_InternalFlushValues(walk.pScript, hRoot, VRegInternalWString_GetView(&walk.keyPath), &walk.values);

    VRegScript_InternalFreeValues(&walk.values);
    VRegInternalArray_Free(&walk.values);
    VRegInternalWString_Free(&walk.keyPath);
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

static LSTATUS VRegScript_InternalDecodeFail(
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

/// Where a logical line's text came from: the physical line and column its
/// characters from @ref start on were read at.
typedef struct
{
    SIZE_T start;
    SIZE_T line;
    SIZE_T column;
} VRegScriptInternalLineSegment;

typedef enum
{
    VREG_INTERNAL_KEY_NONE,
    VREG_INTERNAL_KEY_OPEN,
    VREG_INTERNAL_KEY_DELETED,
} VRegScriptInternalKeyState;

typedef struct
{
    VRegScript* pScript;

    /// The logical line being parsed: a line of the file with its
    /// continuations joined, and the position in it.
    VRegInternalWString text;
    SIZE_T pos;

    /// `VRegScriptInternalLineSegment`: offsets into @ref text map back to the
    /// physical lines the text came from.
    VRegInternalArray segments;

    /// The physical line the logical line starts on, 1-based.
    SIZE_T line;

    VRegScriptInternalKeyState state;
    HKEY hRoot;
    VRegInternalWString path;

    /// Set by @ref VRegScriptInternalParser_ParseFail: where in @ref text, and
    /// why.
    LPCSTR pszError;
    SIZE_T errorOffset;
} VRegScriptInternalParser;

/// Records a syntax error at the parser's position and answers its status.
static LSTATUS VRegScriptInternalParser_ParseFail(
    _Inout_ VRegScriptInternalParser* const pParser,
    _In_ const LPCSTR pszMessage
)
{
    pParser->pszError = pszMessage;
    pParser->errorOffset = pParser->pos;
    return ERROR_INVALID_DATA;
}

static BOOL VRegScript_InternalIsBlank(const wchar_t ch)
{
    return ch == L' ' || ch == L'\t';
}

static int VRegScript_InternalValueFromHexDigit(const wchar_t ch)
{
    if (ch >= L'0' && ch <= L'9')
        return ch - L'0';
    if (ch >= L'a' && ch <= L'f')
        return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F')
        return ch - L'A' + 10;
    return -1;
}

static wchar_t VRegScript_InternalToLowerAscii(const wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch - L'A' + L'a') : ch;
}

static BOOL VRegInternalWStringView_Equals(
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

static BOOL VRegInternalWStringView_EqualsIgnoreAsciiCase(
    const VRegInternalWStringView left,
    const VRegInternalWStringView right
)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (VRegScript_InternalToLowerAscii(left.pch[i]) != VRegScript_InternalToLowerAscii(right.pch[i]))
            return FALSE;
    }

    return TRUE;
}

static VRegInternalWStringView VRegInternalWStringView_TrimEnd(
    VRegInternalWStringView text
)
{
    while (text.cch != 0 && VRegScript_InternalIsBlank(text.pch[text.cch - 1]))
        --text.cch;

    return text;
}

static VRegInternalWStringView VRegInternalWStringView_Substring(
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
static SIZE_T VRegInternalWStringView_FindChar(
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
static void VRegScriptInternalParser_GetPosition(
    _In_ const VRegScriptInternalParser* const pParser,
    const SIZE_T offset,
    _Out_ SIZE_T* const pLine,
    _Out_ SIZE_T* const pColumn
)
{
    const VRegScriptInternalLineSegment* pSegment = &VREG_INTERNAL_AT(&pParser->segments, VRegScriptInternalLineSegment, 0);
    SIZE_T i;
    for (i = 0; i < pParser->segments.count; ++i)
    {
        const VRegScriptInternalLineSegment* const pCandidate =
            &VREG_INTERNAL_AT(&pParser->segments, VRegScriptInternalLineSegment, i);
        if (pCandidate->start > offset)
            break;

        pSegment = pCandidate;
    }

    *pLine = pSegment->line;
    *pColumn = pSegment->column + (offset - pSegment->start);
}

static BOOL VRegScriptInternalParser_IsAtEnd(
    _In_ const VRegScriptInternalParser* const pParser
)
{
    return pParser->pos >= pParser->text.cch;
}

static wchar_t VRegScriptInternalParser_GetCurrent(
    _In_ const VRegScriptInternalParser* const pParser
)
{
    return pParser->text.psz[pParser->pos];
}

static void VRegScriptInternalParser_SkipBlanks(
    _Inout_ VRegScriptInternalParser* const pParser
)
{
    while (!VRegScriptInternalParser_IsAtEnd(pParser) && VRegScript_InternalIsBlank(VRegScriptInternalParser_GetCurrent(pParser)))
        ++pParser->pos;
}

static LSTATUS VRegScriptInternalParser_ExpectEnd(
    _Inout_ VRegScriptInternalParser* const pParser
)
{
    VRegScriptInternalParser_SkipBlanks(pParser);
    if (!VRegScriptInternalParser_IsAtEnd(pParser))
        return VRegScriptInternalParser_ParseFail(pParser, "unexpected text at the end of the line");

    return ERROR_SUCCESS;
}

/// Consumes @p pszToken, ignoring case, if the text continues with it.
static BOOL VRegScriptInternalParser_Consume(
    _Inout_ VRegScriptInternalParser* const pParser,
    _In_ const LPCWSTR pszToken
)
{
    const VRegInternalWStringView token = VReg_InternalViewFromText(pszToken);
    const VRegInternalWStringView text = VRegInternalWString_GetView(&pParser->text);
    if (text.cch - pParser->pos < token.cch)
        return FALSE;
    if (!VRegInternalWStringView_EqualsIgnoreAsciiCase(VRegInternalWStringView_Substring(text, pParser->pos, token.cch), token))
        return FALSE;

    pParser->pos += token.cch;
    return TRUE;
}

/// Reads the logical line starting at physical line @p index of @p pLines
/// (empty for blank and comment lines) and answers the index of its last
/// physical line in @p pLast.
static LSTATUS VRegScriptInternalParser_ReadLogicalLine(
    _Inout_ VRegScriptInternalParser* const pParser,
    _In_ const VRegInternalArray* const pLines,
    const SIZE_T index,
    _Out_ SIZE_T* const pLast
)
{
    VRegInternalWStringView physical = VREG_INTERNAL_AT(pLines, VRegInternalWStringView, index);
    VRegScriptInternalLineSegment segment;
    SIZE_T current = index;

    pParser->segments.count = 0;
    if (!VRegInternalWString_Resize(&pParser->text, 0))
        return ERROR_OUTOFMEMORY;

    segment.start = 0;
    segment.line = index + 1;
    segment.column = 1;
    while (physical.cch != 0 && VRegScript_InternalIsBlank(physical.pch[0]))
    {
        ++physical.pch;
        --physical.cch;
        ++segment.column;
    }
    if (!VRegInternalArray_Append(&pParser->segments, sizeof(segment), &segment))
        return ERROR_OUTOFMEMORY;

    *pLast = index;
    if (physical.cch == 0 || physical.pch[0] == L';')
        return ERROR_SUCCESS;

    if (!VRegInternalWString_Assign(&pParser->text, physical.pch, physical.cch))
        return ERROR_OUTOFMEMORY;
    if (physical.pch[0] == L'[')
        return ERROR_SUCCESS;

    // A value that continues ends its line in a backslash; a closing quote
    // ends every complete one.
    while (current + 1 < pLines->count)
    {
        VRegInternalWStringView joined = VRegInternalWStringView_TrimEnd(VRegInternalWString_GetView(&pParser->text));
        VRegInternalWStringView next;
        if (joined.cch == 0 || joined.pch[joined.cch - 1] != L'\\')
            break;

        VRegInternalWString_Truncate(&pParser->text, joined.cch - 1);

        ++current;
        next = VREG_INTERNAL_AT(pLines, VRegInternalWStringView, current);
        segment.start = pParser->text.cch;
        segment.line = current + 1;
        segment.column = 1;
        while (next.cch != 0 && VRegScript_InternalIsBlank(next.pch[0]))
        {
            ++next.pch;
            --next.cch;
            ++segment.column;
        }
        if (
            !VRegInternalArray_Append(&pParser->segments, sizeof(segment), &segment)
            || !VRegInternalWString_Append(&pParser->text, next.pch, next.cch)
        )
            return ERROR_OUTOFMEMORY;
    }

    *pLast = current;
    return ERROR_SUCCESS;
}

static LSTATUS VRegScriptInternalParser_ParseKey(
    _Inout_ VRegScriptInternalParser* const pParser
)
{
    const VRegInternalWStringView text = VRegInternalWString_GetView(&pParser->text);
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
        return VRegScriptInternalParser_ParseFail(pParser, "missing ] after the key name");
    }

    // `close` is one past the bracket: the position that ends the key.
    pParser->pos = close;
    status = VRegScriptInternalParser_ExpectEnd(pParser);
    if (status != ERROR_SUCCESS)
        return status;

    --close;
    pParser->pos = 1;
    isDelete = text.pch[pParser->pos] == L'-';
    if (isDelete)
        ++pParser->pos;

    full = VRegInternalWStringView_Substring(text, pParser->pos, close - pParser->pos);
    slash = VRegInternalWStringView_FindChar(full, 0, L'\\');
    if (!VRegInternalWStringView_FindRoot(VRegInternalWStringView_Substring(full, 0, slash), &hRoot))
        return VRegScriptInternalParser_ParseFail(pParser, "unknown registry root");

    path = slash < full.cch ? VRegInternalWStringView_Substring(full, slash + 1, full.cch - slash - 1) : VRegInternalWStringView_Substring(full, 0, 0);
    if (isDelete && path.cch == 0)
        return VRegScriptInternalParser_ParseFail(pParser, "a root key cannot be deleted");

    status = VRegScript_InternalAppend(
        pParser->pScript,
        isDelete ? VREG_OPERATION_DELETE_KEY : VREG_OPERATION_CREATE_KEY,
        hRoot,
        path,
        VReg_InternalViewFromText(L""),
        0,
        NULL,
        0,
        pParser->line
    );
    if (status != ERROR_SUCCESS)
        return status;

    pParser->state = isDelete ? VREG_INTERNAL_KEY_DELETED : VREG_INTERNAL_KEY_OPEN;
    pParser->hRoot = hRoot;
    return VRegInternalWString_Assign(&pParser->path, path.pch, path.cch) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

/// Reads a quoted string at the position (on its opening quote) into @p pText.
static LSTATUS VRegScriptInternalParser_ParseQuoted(
    _Inout_ VRegScriptInternalParser* const pParser,
    _Inout_ VRegInternalWString* const pText
)
{
    ++pParser->pos;
    if (!VRegInternalWString_Resize(pText, 0))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        wchar_t ch;
        if (VRegScriptInternalParser_IsAtEnd(pParser))
            return VRegScriptInternalParser_ParseFail(pParser, "unterminated string");

        ch = VRegScriptInternalParser_GetCurrent(pParser);
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
            if (VRegScriptInternalParser_IsAtEnd(pParser) || (VRegScriptInternalParser_GetCurrent(pParser) != L'\\' && VRegScriptInternalParser_GetCurrent(pParser) != L'"'))
            {
                --pParser->pos;
                return VRegScriptInternalParser_ParseFail(pParser, "invalid escape, only \\\\ and \\\" are allowed");
            }

            ch = VRegScriptInternalParser_GetCurrent(pParser);
        }

        if (!VRegInternalWString_AppendChar(pText, ch))
            return ERROR_OUTOFMEMORY;

        ++pParser->pos;
    }
}

static LSTATUS VRegScriptInternalParser_ParseHexNumber(
    _Inout_ VRegScriptInternalParser* const pParser,
    const SIZE_T maxDigits,
    _Out_ DWORD* const pdwValue
)
{
    DWORD dwValue = 0;
    SIZE_T digits = 0;
    *pdwValue = 0;
    while (!VRegScriptInternalParser_IsAtEnd(pParser) && VRegScript_InternalValueFromHexDigit(VRegScriptInternalParser_GetCurrent(pParser)) >= 0)
    {
        if (++digits > maxDigits)
            return VRegScriptInternalParser_ParseFail(pParser, "too many hex digits");

        dwValue = (dwValue << 4) | (DWORD)VRegScript_InternalValueFromHexDigit(VRegScriptInternalParser_GetCurrent(pParser));
        ++pParser->pos;
    }
    if (digits == 0)
        return VRegScriptInternalParser_ParseFail(pParser, "expected a hex number");

    *pdwValue = dwValue;
    return ERROR_SUCCESS;
}

static LSTATUS VRegScriptInternalParser_ParseHexBytes(
    _Inout_ VRegScriptInternalParser* const pParser,
    _Inout_ VRegInternalBuffer* const pData
)
{
    VRegScriptInternalParser_SkipBlanks(pParser);
    if (VRegScriptInternalParser_IsAtEnd(pParser))
        return ERROR_SUCCESS;

    for (;;)
    {
        DWORD dwByte;
        BYTE byte;
        LSTATUS status;
        if (VRegScriptInternalParser_IsAtEnd(pParser) || VRegScript_InternalValueFromHexDigit(VRegScriptInternalParser_GetCurrent(pParser)) < 0)
            return VRegScriptInternalParser_ParseFail(pParser, "expected a hex byte");

        status = VRegScriptInternalParser_ParseHexNumber(pParser, 2, &dwByte);
        if (status != ERROR_SUCCESS)
            return status;

        byte = (BYTE)dwByte;
        if (!VRegInternalBuffer_Append(pData, &byte, 1))
            return ERROR_OUTOFMEMORY;

        VRegScriptInternalParser_SkipBlanks(pParser);
        if (VRegScriptInternalParser_IsAtEnd(pParser))
            return ERROR_SUCCESS;
        if (VRegScriptInternalParser_GetCurrent(pParser) != L',')
            return VRegScriptInternalParser_ParseFail(pParser, "expected , between hex bytes");

        ++pParser->pos;
        VRegScriptInternalParser_SkipBlanks(pParser);
    }
}

static LSTATUS VRegScriptInternalParser_ParseValue(
    _Inout_ VRegScriptInternalParser* const pParser
)
{
    VRegInternalWString name = {0, 0, NULL};
    VRegInternalWString text = {0, 0, NULL};
    VRegInternalBuffer data = {0, 0, NULL};
    VRegOperationKind kind = VREG_OPERATION_SET_VALUE;
    DWORD dwType = 0;
    LSTATUS status = ERROR_SUCCESS;

    if (VRegScriptInternalParser_GetCurrent(pParser) == L'@')
        ++pParser->pos;
    else if (VRegScriptInternalParser_GetCurrent(pParser) == L'"')
        status = VRegScriptInternalParser_ParseQuoted(pParser, &name);
    else
        status = VRegScriptInternalParser_ParseFail(pParser, "expected a value name or a [key]");
    if (status != ERROR_SUCCESS)
        goto done;

    if (pParser->state == VREG_INTERNAL_KEY_NONE)
    {
        pParser->pos = 0;
        status = VRegScriptInternalParser_ParseFail(pParser, "a value outside any key");
        goto done;
    }

    // A value under a [-key] line is an error rather than silently dropped,
    // since it can only be a mistake.
    if (pParser->state == VREG_INTERNAL_KEY_DELETED)
    {
        pParser->pos = 0;
        status = VRegScriptInternalParser_ParseFail(pParser, "a value after a deleted key");
        goto done;
    }

    VRegScriptInternalParser_SkipBlanks(pParser);
    if (VRegScriptInternalParser_IsAtEnd(pParser) || VRegScriptInternalParser_GetCurrent(pParser) != L'=')
    {
        status = VRegScriptInternalParser_ParseFail(pParser, "expected =");
        goto done;
    }

    ++pParser->pos;
    VRegScriptInternalParser_SkipBlanks(pParser);

    if (!VRegScriptInternalParser_IsAtEnd(pParser) && VRegScriptInternalParser_GetCurrent(pParser) == L'-')
    {
        ++pParser->pos;
        status = VRegScriptInternalParser_ExpectEnd(pParser);
        kind = VREG_OPERATION_DELETE_VALUE;
    }
    else if (!VRegScriptInternalParser_IsAtEnd(pParser) && VRegScriptInternalParser_GetCurrent(pParser) == L'"')
    {
        status = VRegScriptInternalParser_ParseQuoted(pParser, &text);
        if (status == ERROR_SUCCESS)
            status = VRegScriptInternalParser_ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_SZ;
            if (!VRegInternalBuffer_Assign(&data, VRegInternalWString_GetText(&text), (text.cch + 1) * sizeof(wchar_t)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VRegScriptInternalParser_Consume(pParser, L"dword:"))
    {
        DWORD dwValue = 0;
        status = VRegScriptInternalParser_ParseHexNumber(pParser, 8, &dwValue);
        if (status == ERROR_SUCCESS)
            status = VRegScriptInternalParser_ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_DWORD;
            if (!VRegInternalBuffer_Assign(&data, &dwValue, sizeof(dwValue)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VRegScriptInternalParser_Consume(pParser, L"hex"))
    {
        dwType = REG_BINARY;
        if (VRegScriptInternalParser_Consume(pParser, L"("))
        {
            status = VRegScriptInternalParser_ParseHexNumber(pParser, 8, &dwType);
            if (status == ERROR_SUCCESS && !VRegScriptInternalParser_Consume(pParser, L")"))
                status = VRegScriptInternalParser_ParseFail(pParser, "expected ) after the value type");
        }
        if (status == ERROR_SUCCESS && !VRegScriptInternalParser_Consume(pParser, L":"))
            status = VRegScriptInternalParser_ParseFail(pParser, "expected : after hex");
        if (status == ERROR_SUCCESS)
            status = VRegScriptInternalParser_ParseHexBytes(pParser, &data);
    }
    else
    {
        status = VRegScriptInternalParser_ParseFail(pParser, "expected a string, dword:, hex: or -");
    }
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegScript_InternalAppend(
        pParser->pScript,
        kind,
        pParser->hRoot,
        VRegInternalWString_GetView(&pParser->path),
        VRegInternalWString_GetView(&name),
        dwType,
        data.pb,
        (DWORD)data.cb,
        pParser->line
    );

done:
    VRegInternalWString_Free(&name);
    VRegInternalWString_Free(&text);
    VRegInternalBuffer_Free(&data);
    return status;
}

static LSTATUS VRegScriptInternalParser_ParseLogicalLine(
    _Inout_ VRegScriptInternalParser* const pParser
)
{
    // Key paths and value names reach the registry as C strings, where a NUL
    // would silently cut them short ("[-HKCU\<NUL>x]" would delete all of
    // HKCU); no valid .reg text holds one anywhere.
    pParser->pos = VRegInternalWStringView_FindChar(VRegInternalWString_GetView(&pParser->text), 0, L'\0');
    if (pParser->pos < pParser->text.cch)
        return VRegScriptInternalParser_ParseFail(pParser, "a NUL character");

    pParser->pos = 0;
    return pParser->text.psz[0] == L'[' ? VRegScriptInternalParser_ParseKey(pParser) : VRegScriptInternalParser_ParseValue(pParser);
}

/// Parses all of @p text into the parser's script. A syntax error answers
/// @ref ERROR_INVALID_DATA with @p pError filled in.
static LSTATUS VRegScriptInternalParser_ParseText(
    _Inout_ VRegScriptInternalParser* const pParser,
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
        const SIZE_T end = VRegInternalWStringView_FindChar(text, start, L'\n');
        VRegInternalWStringView line = VRegInternalWStringView_Substring(text, start, end - start);
        if (line.cch != 0 && line.pch[line.cch - 1] == L'\r')
            --line.cch;
        if (!VRegInternalArray_Append(&lines, sizeof(line), &line))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        start = end + 1;
    }

    // The loop above appended the first line, however short the text.
    _Analysis_assume_(lines.p != NULL);
    header = VRegInternalWStringView_TrimEnd(VREG_INTERNAL_AT(&lines, VRegInternalWStringView, 0));
    if (
        !VRegInternalWStringView_Equals(header, VReg_InternalViewFromText(L"Windows Registry Editor Version 5.00"))
        && !VRegInternalWStringView_Equals(header, VReg_InternalViewFromText(L"REGEDIT4"))
    )
    {
        status = VRegScript_InternalDecodeFail(pError, "missing or unknown registry editor header");
        goto done;
    }

    for (i = 1; i < lines.count; ++i)
    {
        SIZE_T last = i;
        pParser->line = i + 1;
        status = VRegScriptInternalParser_ReadLogicalLine(pParser, &lines, i, &last);
        if (status == ERROR_SUCCESS && pParser->text.cch != 0)
            status = VRegScriptInternalParser_ParseLogicalLine(pParser);
        if (status == ERROR_INVALID_DATA && pError)
        {
            VRegScriptInternalParser_GetPosition(pParser, pParser->errorOffset, &pError->line, &pError->column);
            pError->pszMessage = pParser->pszError;
        }
        if (status != ERROR_SUCCESS)
            goto done;

        i = last;
    }

done:
    VRegInternalArray_Free(&lines);
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
    VRegScriptInternalParser parser;
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));

    status = VRegScript_InternalDecodeFile(pFile, cbFile, &text, pError);
    if (status != ERROR_SUCCESS)
    {
        VRegInternalWString_Free(&text);
        return status;
    }

    ZeroMemory(&parser, sizeof(parser));
    status = VRegScript_InternalCreate(&parser.pScript);
    if (status == ERROR_SUCCESS)
        status = VRegScriptInternalParser_ParseText(&parser, VRegInternalWString_GetView(&text), pError);

    VRegInternalWString_Free(&parser.text);
    VRegInternalWString_Free(&parser.path);
    VRegInternalArray_Free(&parser.segments);
    VRegInternalWString_Free(&text);
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
    status = VRegScript_InternalReadWholeFile(pszPath, &file);
    if (status == ERROR_SUCCESS)
        status = VRegScript_Parse(file.pb, file.cb, ppScript, pError);

    VRegInternalBuffer_Free(&file);
    return status;
}

// ==========================================================================
// Scripts
// ==========================================================================

/// Every root a `.reg` file can name, the one list import and export share.
static const VRegScriptInternalRootKey VREG_INTERNAL_ROOT_KEYS[] = {
    {L"HKEY_CLASSES_ROOT", L"HKCR", HKEY_CLASSES_ROOT},
    {L"HKEY_CURRENT_USER", L"HKCU", HKEY_CURRENT_USER},
    {L"HKEY_LOCAL_MACHINE", L"HKLM", HKEY_LOCAL_MACHINE},
    {L"HKEY_USERS", L"HKU", HKEY_USERS},
    {L"HKEY_CURRENT_CONFIG", L"HKCC", HKEY_CURRENT_CONFIG},
};

VREG_DECL _Ret_maybenull_ LPCWSTR VRegScript_InternalGetRootName(
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

VREG_DECL BOOL VRegInternalWStringView_FindRoot(
    const VRegInternalWStringView name,
    _Out_ PHKEY const phRoot
)
{
    SIZE_T i;
    *phRoot = NULL;
    for (i = 0; i < sizeof(VREG_INTERNAL_ROOT_KEYS) / sizeof(VREG_INTERNAL_ROOT_KEYS[0]); ++i)
    {
        const VRegScriptInternalRootKey* const pRoot = &VREG_INTERNAL_ROOT_KEYS[i];
        if (
            VRegInternalWStringView_EqualsIgnoreAsciiCase(name, VReg_InternalViewFromText(pRoot->pszLong))
            || VRegInternalWStringView_EqualsIgnoreAsciiCase(name, VReg_InternalViewFromText(pRoot->pszShort))
        )
        {
            *phRoot = pRoot->hKey;
            return TRUE;
        }
    }

    return FALSE;
}

VREG_DECL LSTATUS VRegScript_InternalCreate(
    _Outptr_ VRegScript** const ppScript
)
{
    VRegScript* const pScript = (VRegScript*)VReg_InternalAllocZeroed(sizeof(VRegScript));
    *ppScript = NULL;
    if (!pScript)
        return ERROR_OUTOFMEMORY;

    *ppScript = pScript;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VRegScript_InternalAppend(
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
    VRegScriptInternalEntry entry;
    VRegOperation operation;
    ZeroMemory(&entry, sizeof(entry));
    if (
        !VRegInternalWString_Assign(&entry.path, path.pch, path.cch)
        || !VRegInternalWString_Assign(&entry.name, name.pch, name.cch)
        || !VRegInternalBuffer_Assign(&entry.data, pData, cbData)
        || !VRegInternalArray_Reserve(&pScript->operations, sizeof(operation), pScript->operations.count + 1)
        || !VRegInternalArray_Append(&pScript->entries, sizeof(entry), &entry)
    )
    {
        VRegInternalWString_Free(&entry.path);
        VRegInternalWString_Free(&entry.name);
        VRegInternalBuffer_Free(&entry.data);
        return ERROR_OUTOFMEMORY;
    }

    // The entry's storage is on the heap, so the pointers stay valid when the
    // array of entries moves.
    operation.kind = kind;
    operation.hRoot = hRoot;
    operation.pszPath = VRegInternalWString_GetText(&entry.path);
    operation.pszName = VRegInternalWString_GetText(&entry.name);
    operation.dwType = dwType;
    operation.pData = entry.data.pb;
    operation.cbData = cbData;
    operation.line = line;
    VRegInternalArray_Append(&pScript->operations, sizeof(operation), &operation);
    return ERROR_SUCCESS;
}

VREG_DECL void WINAPI VRegScript_Destroy(_In_opt_ VRegScript* const pScript)
{
    SIZE_T i;
    if (!pScript)
        return;

    for (i = 0; i < pScript->entries.count; ++i)
    {
        VRegScriptInternalEntry* const pEntry = &VREG_INTERNAL_AT(&pScript->entries, VRegScriptInternalEntry, i);
        VRegInternalWString_Free(&pEntry->path);
        VRegInternalWString_Free(&pEntry->name);
        VRegInternalBuffer_Free(&pEntry->data);
    }

    VRegInternalArray_Free(&pScript->entries);
    VRegInternalArray_Free(&pScript->operations);
    VReg_InternalFree(pScript);
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
    VReg_InternalFree(p);
}

// ==========================================================================
// Script text
// ==========================================================================

static BOOL VReg_InternalStartsWithAscii(
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

static LSTATUS VReg_InternalWiden(
    const UINT codePage,
    _In_reads_bytes_(cb) const BYTE* const pb,
    const SIZE_T cb,
    _Inout_ VRegInternalWString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const DWORD dwFlags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const LSTATUS status = VReg_InternalWideFromMultiByte(codePage, dwFlags, (LPCCH)pb, cb, pText);
    if (status == ERROR_NO_UNICODE_TRANSLATION)
        return VRegScript_InternalDecodeFail(pError, "the text is not valid in its encoding");

    return status;
}

VREG_DECL LSTATUS VRegScript_InternalDecodeFile(
    _In_reads_bytes_(cbFile) const void* const pFile,
    const SIZE_T cbFile,
    _Inout_ VRegInternalWString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const BYTE* const pb = (const BYTE*)pFile;
    if (cbFile > (SIZE_T)MAXINT32)
        return VRegScript_InternalDecodeFail(pError, "the file is too large");

    if (cbFile >= 2 && pb[0] == 0xFF && pb[1] == 0xFE)
    {
        const SIZE_T cbBody = cbFile - 2;
        if (cbBody % sizeof(wchar_t) != 0)
            return VRegScript_InternalDecodeFail(pError, "a UTF-16 file has an odd byte count");
        if (!VRegInternalWString_Assign(pText, (LPCWCH)(pb + 2), cbBody / sizeof(wchar_t)))
            return ERROR_OUTOFMEMORY;

        return ERROR_SUCCESS;
    }
    if (cbFile >= 2 && pb[0] == 0xFE && pb[1] == 0xFF)
        return VRegScript_InternalDecodeFail(pError, "big-endian UTF-16 is not supported");
    if (cbFile >= 3 && pb[0] == 0xEF && pb[1] == 0xBB && pb[2] == 0xBF)
        return VReg_InternalWiden(CP_UTF8, pb + 3, cbFile - 3, pText, pError);

    return VReg_InternalWiden(
        VReg_InternalStartsWithAscii(pb, cbFile, "REGEDIT4") ? CP_ACP : CP_UTF8,
        pb,
        cbFile,
        pText,
        pError
    );
}

VREG_DECL LSTATUS VRegScript_InternalEncodeFile(
    _In_ const VRegInternalWString* const pText,
    const VRegFormat format,
    _Inout_ VRegInternalBuffer* const pFile
)
{
    UINT codePage;
    BOOL isUtf8;
    BOOL fUsedDefault = FALSE;
    LSTATUS status;

    if (!VRegInternalBuffer_Resize(pFile, 0))
        return ERROR_OUTOFMEMORY;

    if (format == VREG_FORMAT_UNICODE)
    {
        static const BYTE BOM[2] = {0xFF, 0xFE};
        if (
            !VRegInternalBuffer_Append(pFile, BOM, sizeof(BOM))
            || !VRegInternalBuffer_Append(pFile, VRegInternalWString_GetText(pText), pText->cch * sizeof(wchar_t))
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
    status = VReg_InternalMultiByteFromWide(
        codePage,
        isUtf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        isUtf8 ? NULL : &fUsedDefault,
        VRegInternalWString_GetText(pText),
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

VREG_DECL LSTATUS VRegScript_InternalReadWholeFile(
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
    else if (!VRegInternalBuffer_Resize(pFile, (SIZE_T)size.QuadPart))
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

VREG_DECL LSTATUS VRegScript_InternalWriteWholeFile(
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

// Byte-exact round trip: a value takes its friendly form only when that
// reproduces its bytes on re-import, otherwise `hex(n):` (a `REG_SZ` without
// terminator, with an embedded NUL or newline, or an odd byte count). The
// layout is `reg export`'s byte for byte apart from the order: CRLF, hex
// data broken with a `\` continuation and a two-space indent, a blank line
// between keys. Windows imports the UTF-8 form intact, non-BMP characters
// included: `regedit /s` with or without a BOM, `reg import` only without
// one (with a BOM it answers "not a registry file"), which is why
// @ref VREG_FORMAT_UTF8 writes none.

// Windows `reg export` breaks after the first comma past this column and
// indents continuation lines by two spaces. Wine regedit breaks at the column
// itself, which differs when a line ends exactly there.
#define VREG_INTERNAL_HEX_WRAP_COLUMN ((SIZE_T)76)

static const wchar_t VREG_INTERNAL_NEWLINE[] = L"\r\n";

/// Appends the terminated literal @p psz.
static BOOL VRegInternalWString_OutAppend(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const LPCWSTR psz
)
{
    const VRegInternalWStringView text = VReg_InternalViewFromText(psz);
    return VRegInternalWString_Append(pOut, text.pch, text.cch);
}

static BOOL VRegInternalWStringView_HasLineBreak(const VRegInternalWStringView text)
{
    SIZE_T i;
    for (i = 0; i < text.cch; ++i)
    {
        if (text.pch[i] == L'\r' || text.pch[i] == L'\n')
            return TRUE;
    }

    return FALSE;
}

static BOOL VRegInternalWString_AppendQuoted(
    _Inout_ VRegInternalWString* const pOut,
    const VRegInternalWStringView text
)
{
    SIZE_T i;
    if (!VRegInternalWString_AppendChar(pOut, L'"'))
        return FALSE;

    for (i = 0; i < text.cch; ++i)
    {
        const wchar_t ch = text.pch[i];
        if ((ch == L'\\' || ch == L'"') && !VRegInternalWString_AppendChar(pOut, L'\\'))
            return FALSE;
        if (!VRegInternalWString_AppendChar(pOut, ch))
            return FALSE;
    }

    return VRegInternalWString_AppendChar(pOut, L'"');
}

/// The text a REG_SZ takes in the friendly form; FALSE when the form would not
/// reproduce the bytes.
static BOOL VRegScript_InternalFormatFriendlyString(
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
static BOOL VRegScript_InternalAppendHexNumber(
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
        if (!VRegInternalWString_AppendChar(pOut, L'0'))
            return FALSE;
    }
    for (i = 0; i < cchDigits; ++i)
    {
        if (!VRegInternalWString_AppendChar(pOut, wszDigits[cchDigits - 1 - i]))
            return FALSE;
    }

    return TRUE;
}

static BOOL VRegScript_InternalAppendHex(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const VRegOperation* const pOperation,
    SIZE_T column
)
{
    const SIZE_T cchBefore = pOut->cch;
    SIZE_T i;

    if (pOperation->dwType == REG_BINARY)
    {
        if (!VRegInternalWString_OutAppend(pOut, L"hex:"))
            return FALSE;
    }
    else
    {
        if (
            !VRegInternalWString_OutAppend(pOut, L"hex(")
            || !VRegScript_InternalAppendHexNumber(pOut, pOperation->dwType, 1)
            || !VRegInternalWString_OutAppend(pOut, L"):")
        )
            return FALSE;
    }

    column += pOut->cch - cchBefore;
    for (i = 0; i < pOperation->cbData; ++i)
    {
        if (!VRegScript_InternalAppendHexNumber(pOut, pOperation->pData[i], 2))
            return FALSE;

        column += 2;
        if (i + 1 == pOperation->cbData)
            break;
        if (!VRegInternalWString_AppendChar(pOut, L','))
            return FALSE;
        if (++column > VREG_INTERNAL_HEX_WRAP_COLUMN)
        {
            if (!VRegInternalWString_OutAppend(pOut, L"\\\r\n  "))
                return FALSE;

            column = 2;
        }
    }

    return TRUE;
}

static BOOL VRegScript_InternalAppendValue(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const VRegOperation* const pOperation
)
{
    const SIZE_T lineStart = pOut->cch;
    const VRegInternalWStringView name = VReg_InternalViewFromText(pOperation->pszName);
    VRegInternalWStringView text;
    BOOL isAppended;

    if (name.cch == 0)
        isAppended = VRegInternalWString_AppendChar(pOut, L'@');
    else
        isAppended = VRegInternalWString_AppendQuoted(pOut, name);
    if (!isAppended || !VRegInternalWString_AppendChar(pOut, L'='))
        return FALSE;

    if (pOperation->kind == VREG_OPERATION_DELETE_VALUE)
    {
        isAppended = VRegInternalWString_AppendChar(pOut, L'-');
    }
    else if (pOperation->dwType == REG_SZ && VRegScript_InternalFormatFriendlyString(pOperation->pData, pOperation->cbData, &text))
    {
        isAppended = VRegInternalWString_AppendQuoted(pOut, text);
    }
    else if (pOperation->dwType == REG_DWORD && pOperation->cbData == sizeof(DWORD))
    {
        DWORD dwValue;
        CopyMemory(&dwValue, pOperation->pData, sizeof(dwValue));
        isAppended =
            VRegInternalWString_OutAppend(pOut, L"dword:")
            && VRegScript_InternalAppendHexNumber(pOut, dwValue, 8);
    }
    else
    {
        isAppended = VRegScript_InternalAppendHex(pOut, pOperation, pOut->cch - lineStart);
    }

    return isAppended && VRegInternalWString_OutAppend(pOut, VREG_INTERNAL_NEWLINE);
}

/// Appends `[key]` or `[-key]` for @p pOperation's key, preceded by a blank
/// line.
static LSTATUS VRegScript_InternalAppendKeyLine(
    _Inout_ VRegInternalWString* const pOut,
    _In_ const VRegOperation* const pOperation,
    const BOOL isDelete
)
{
    const LPCWSTR pszRoot = VRegScript_InternalGetRootName(pOperation->hRoot);
    const VRegInternalWStringView path = VReg_InternalViewFromText(pOperation->pszPath);
    if (!pszRoot)
        return ERROR_INVALID_HANDLE;

    if (
        !VRegInternalWString_OutAppend(pOut, VREG_INTERNAL_NEWLINE)
        || !VRegInternalWString_OutAppend(pOut, isDelete ? L"[-" : L"[")
        || !VRegInternalWString_OutAppend(pOut, pszRoot)
        || (path.cch != 0 && !VRegInternalWString_AppendChar(pOut, L'\\'))
        || !VRegInternalWString_Append(pOut, path.pch, path.cch)
        || !VRegInternalWString_OutAppend(pOut, L"]")
        || !VRegInternalWString_OutAppend(pOut, VREG_INTERNAL_NEWLINE)
    )
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

/// Whether @p pLeft and @p pRight name the same key.
static BOOL VReg_InternalSameKey(
    _In_ const VRegOperation* const pLeft,
    _In_ const VRegOperation* const pRight
)
{
    return pLeft->hRoot == pRight->hRoot
        && VRegInternalWStringView_CompareNames(VReg_InternalViewFromText(pLeft->pszPath), VReg_InternalViewFromText(pRight->pszPath)) == 0;
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
        !VRegInternalWString_OutAppend(
            &out,
            format == VREG_FORMAT_REGEDIT4 ? L"REGEDIT4" : L"Windows Registry Editor Version 5.00"
        )
        || !VRegInternalWString_OutAppend(&out, VREG_INTERNAL_NEWLINE)
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
        if (
            VRegInternalWStringView_HasLineBreak(VReg_InternalViewFromText(pOperation->pszPath))
            || VRegInternalWStringView_HasLineBreak(VReg_InternalViewFromText(pOperation->pszName))
        )
        {
            status = ERROR_INVALID_NAME;
            goto done;
        }

        if (isKey || !pLastKey || !VReg_InternalSameKey(pLastKey, pOperation))
        {
            status = VRegScript_InternalAppendKeyLine(
                &out,
                pOperation,
                pOperation->kind == VREG_OPERATION_DELETE_KEY
            );
            if (status != ERROR_SUCCESS)
                goto done;

            pLastKey = pOperation;
        }

        if (!isKey && !VRegScript_InternalAppendValue(&out, pOperation))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    if (!VRegInternalWString_OutAppend(&out, VREG_INTERNAL_NEWLINE))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    status = VRegScript_InternalEncodeFile(&out, format, &file);
    if (status != ERROR_SUCCESS)
        goto done;

    // The header alone makes the file non-empty, so the encoding allocated.
    _Analysis_assume_(file.pb != NULL);
    *ppBytes = file.pb;
    *pcbBytes = file.cb;
    file.pb = NULL;

done:
    VRegInternalWString_Free(&out);
    VRegInternalBuffer_Free(&file);
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

    status = VRegScript_InternalWriteWholeFile(pszPath, pBytes, cbBytes);
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

// Every `Reg*` export that accepts a key handle is hooked; the README's
// "Supported functions" lists each group. The functions refused on virtual
// handles stay out of @ref VRegVtbl: as slots they would either be required,
// so every wrapper forwards calls nothing answers, or carry a default that
// breaks passthrough behind a wrapper that leaves them null; one joins the
// table once a hive can answer it. `RegLoadMUIStringA` answers
// @ref ERROR_CALL_NOT_IMPLEMENTED natively on every input and is hooked only
// so a virtual handle gets that same answer.
//
// Each copy's base registry is whatever its trampoline reaches: another
// hooker, another copy, or the real function. A copy answers what it owns
// (its mounts, its handles) and passes everything else down; on a path
// mounted by two copies the one on top wins, undetected. Nothing runs at
// unload because nothing short of the consumer's pin would do: a thread
// already inside a detour body when the module unmaps returns into unmapped
// code, switching the slot cannot reach it, and draining calls in flight is
// impossible, since detach runs under the loader lock, which a call inside a
// native open may be waiting for, and the count's decrement would run in the
// image itself.

/// The registry every hook calls through.
static VReg* volatile g_VRegHooks_pRegistry;

static VReg* VRegHooks_InternalGetRegistry(void)
{
    return (VReg*)ReadPointerAcquire((PVOID volatile*)&g_VRegHooks_pRegistry);
}

// --- The hooks the registry answers: the same call, made on the registry ---

static LSTATUS WINAPI VRegHooks_InternalRegOpenKeyExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VReg_RegOpenKeyExW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegCreateKeyExW(
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
    return VReg_RegCreateKeyExW(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegCloseKey(
    _In_ const HKEY hKey
)
{
    return VReg_RegCloseKey(
        VRegHooks_InternalGetRegistry(),
        hKey
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegQueryValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VReg_RegQueryValueExW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegSetValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VReg_RegSetValueExW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VReg_RegDeleteValueW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VReg_RegDeleteKeyExW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteTreeW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    return VReg_RegDeleteTreeW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegEnumKeyExW(
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
    return VReg_RegEnumKeyExW(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegEnumValueW(
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
    return VReg_RegEnumValueW(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegQueryInfoKeyW(
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
    return VReg_RegQueryInfoKeyW(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegLoadMUIStringW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    return VReg_RegLoadMUIStringW(
        VRegHooks_InternalGetRegistry(),
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
static LSTATUS WINAPI VRegHooks_InternalRegGetValueW(
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
    return VReg_RegGetValueW(
        VRegHooks_InternalGetRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegNotifyChangeKeyValue(
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    return VReg_RegNotifyChangeKeyValue(
        VRegHooks_InternalGetRegistry(),
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegFlushKey(
    _In_ const HKEY hKey
)
{
    return VReg_RegFlushKey(
        VRegHooks_InternalGetRegistry(),
        hKey
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegOpenKeyExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VReg_RegOpenKeyExA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegCreateKeyExA(
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
    return VReg_RegCreateKeyExA(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegQueryValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VReg_RegQueryValueExA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegSetValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VReg_RegSetValueExA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VReg_RegDeleteValueA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VReg_RegDeleteKeyExA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteTreeA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    return VReg_RegDeleteTreeA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegEnumKeyExA(
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
    return VReg_RegEnumKeyExA(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegEnumValueA(
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
    return VReg_RegEnumValueA(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegQueryInfoKeyA(
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
    return VReg_RegQueryInfoKeyA(
        VRegHooks_InternalGetRegistry(),
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

static LSTATUS WINAPI VRegHooks_InternalRegLoadMUIStringA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
)
{
    return VReg_RegLoadMUIStringA(
        VRegHooks_InternalGetRegistry(),
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
static LSTATUS WINAPI VRegHooks_InternalRegGetValueA(
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
    return VReg_RegGetValueA(
        VRegHooks_InternalGetRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegOpenKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_RegOpenKeyA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegOpenKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_RegOpenKeyW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegCreateKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_RegCreateKeyA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegCreateKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VReg_RegCreateKeyW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegQueryValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VReg_RegQueryValueA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegQueryValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VReg_RegQueryValueW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegSetValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    return VReg_RegSetValueA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegSetValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    return VReg_RegSetValueW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    return VReg_RegDeleteKeyA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    return VReg_RegDeleteKeyW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegEnumKeyA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    return VReg_RegEnumKeyA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegEnumKeyW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    return VReg_RegEnumKeyW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegSetKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VReg_RegSetKeyValueA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegSetKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VReg_RegSetKeyValueW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VReg_RegDeleteKeyValueA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegDeleteKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VReg_RegDeleteKeyValueW(
        VRegHooks_InternalGetRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegQueryMultipleValuesA(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VReg_RegQueryMultipleValuesA(
        VRegHooks_InternalGetRegistry(),
        hKey,
        val_list,
        num_vals,
        lpValueBuf,
        ldwTotsize
    );
}

static LSTATUS WINAPI VRegHooks_InternalRegQueryMultipleValuesW(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VReg_RegQueryMultipleValuesW(
        VRegHooks_InternalGetRegistry(),
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
#define VREG_LIST_INDEX(name) VREG_INTERNAL_REFUSED_##name,
    VREG_INTERNAL_REFUSED_HOOKS(VREG_LIST_INDEX)
#undef VREG_LIST_INDEX

    VREG_INTERNAL_REFUSED_COUNT
} VRegHooksInternalRefusedIndex;

static void* g_VRegHooks_RefusedTargets[VREG_INTERNAL_REFUSED_COUNT];

// The forms the registry answers besides its wide surface, by index into
// their resolved entry points. The registry below the hooks never calls
// them (an ANSI or pre-Ex call reaches it as the wide Ex form), so their
// trampolines live here and serve VRegHooks_Neutralize alone.
typedef enum
{
#define VREG_LIST_INDEX(name) VREG_INTERNAL_FORM_##name,
    VREG_INTERNAL_REGISTRY_FORMS(VREG_LIST_INDEX)
#undef VREG_LIST_INDEX

    VREG_INTERNAL_FORM_COUNT
} VRegHooksInternalFormIndex;

static void* g_VRegHooks_FormTargets[VREG_INTERNAL_FORM_COUNT];

static BOOL VReg_InternalIsOwned(_In_opt_ const HKEY hKey)
{
    return VRegInternalHandleSpace_Owns(VReg_InternalGetProcessHandleSpace(), hKey);
}

/// Whether a @ref RegOpenKeyTransactedW call names no transaction, which it
/// answers with @ref ERROR_INVALID_TRANSACTION. The hook answers that
/// itself: on ARM64 the function reaches that answer through a branch back
/// into its first 12 bytes, which the hook's jump overwrites.
///
/// Answered on every architecture rather than on ARM64 alone, since the native
/// answer is the same everywhere and one path is simpler.
static BOOL VReg_InternalIsUntransacted(
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
static LSTATUS VRegHooks_InternalRefuseMountPathW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszSubKey,
    const REGSAM samDesired
)
{
    VReg* const pRegistry = VRegHooks_InternalGetRegistry();
    const VRegInternalWStringView subKey = VReg_InternalViewFromText(pszSubKey);
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

        if (!VRegInternalWString_Assign(&prefix, subKey.pch, end))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }

        opened = VReg_RegOpenKeyExW(
            pRegistry,
            hKey,
            VRegInternalWString_GetText(&prefix),
            0,
            KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES),
            &hProbe
        );
        if (opened != ERROR_SUCCESS)
            continue;

        isVirtual = VReg_InternalIsOwned(hProbe);
        // An empty path may answer hKey itself rather than a new handle.
        if (hProbe != hKey)
        {
            VReg_RegCloseKey(
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

    VRegInternalWString_Free(&prefix);
    return status;
}

static LSTATUS VRegHooks_InternalRefuseMountPathA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszSubKey,
    const REGSAM samDesired
)
{
    VRegInternalWideArg subKey;
    LSTATUS status = VRegInternalWideArg_Init(pszSubKey, &subKey);
    if (status == ERROR_SUCCESS)
        status = VRegHooks_InternalRefuseMountPathW(hKey, VRegInternalWideArg_GetText(&subKey), samDesired);

    VRegInternalWideArg_Free(&subKey);
    return status;
}

/// @ref ERROR_NOT_SUPPORTED when @p hKey is a virtual handle, else
/// @ref ERROR_SUCCESS.
static LSTATUS VRegHooks_InternalRefuseOwned(
    _In_opt_ const HKEY hKey
)
{
    return VReg_InternalIsOwned(hKey)
        ? ERROR_NOT_SUPPORTED
        : ERROR_SUCCESS;
}

/// @ref ERROR_NOT_SUPPORTED when either handle is a virtual handle, else
/// @ref ERROR_SUCCESS.
static LSTATUS VRegHooks_InternalRefuseOwnedPair(
    _In_opt_ const HKEY hFirst,
    _In_opt_ const HKEY hSecond
)
{
    return VReg_InternalIsOwned(hFirst) || VReg_InternalIsOwned(hSecond)
        ? ERROR_NOT_SUPPORTED
        : ERROR_SUCCESS;
}

/// The refusal of a transacted ANSI call: a virtual handle first, then a
/// mount path.
static LSTATUS VRegHooks_InternalRefuseTransactedA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszSubKey,
    const REGSAM samDesired
)
{
    LSTATUS status = VRegHooks_InternalRefuseOwned(hKey);
    if (status != ERROR_SUCCESS)
        return status;

    return VRegHooks_InternalRefuseMountPathA(hKey, pszSubKey, samDesired);
}

/// The refusal of a transacted wide call: a virtual handle first, then a
/// mount path.
static LSTATUS VRegHooks_InternalRefuseTransactedW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszSubKey,
    const REGSAM samDesired
)
{
    LSTATUS status = VRegHooks_InternalRefuseOwned(hKey);
    if (status != ERROR_SUCCESS)
        return status;

    return VRegHooks_InternalRefuseMountPathW(hKey, pszSubKey, samDesired);
}

/// The refusal of @ref RegOpenKeyTransactedW, which also answers a call that
/// names no transaction.
static LSTATUS VRegHooks_InternalRefuseOpenTransactedW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszSubKey,
    const REGSAM samDesired,
    _In_opt_ const HANDLE hTransaction,
    _In_opt_ const PVOID pExtendedParemeter
)
{
    const LSTATUS status = VRegHooks_InternalRefuseTransactedW(hKey, pszSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;

    if (VReg_InternalIsUntransacted(hTransaction, pExtendedParemeter))
        return ERROR_INVALID_TRANSACTION;

    return ERROR_SUCCESS;
}

/// Defines the typedef and the hook of a function refused on virtual handles.
/// `refusal` yields an @ref LSTATUS: @ref ERROR_SUCCESS passes the call on to
/// the entry point resolved for it.
#define VREG_INTERNAL_REFUSED_HOOK(name, params, refusal, ...)                 \
    typedef LSTATUS WINAPI VRegInternal##name##Function params;                \
                                                                               \
    static LSTATUS WINAPI VRegHooks_InternalRefuse##name params                \
    {                                                                          \
        VRegInternal##name##Function* const pfnNext =                          \
            (VRegInternal##name##Function*)g_VRegHooks_RefusedTargets[VREG_INTERNAL_REFUSED_##name]; \
        const LSTATUS status = (refusal);                                      \
        if (status != ERROR_SUCCESS)                                           \
            return status;                                                     \
                                                                               \
        return pfnNext(__VA_ARGS__);                                           \
    }

VREG_INTERNAL_REFUSED_HOOK(
    RegSaveKeyA,
    (
        _In_ const HKEY hKey,
        _In_ const LPCSTR lpFile,
        _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpFile,
    lpSecurityAttributes
)

VREG_INTERNAL_REFUSED_HOOK(
    RegSaveKeyW,
    (
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpFile,
        _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpFile,
    lpSecurityAttributes
)

VREG_INTERNAL_REFUSED_HOOK(
    RegSaveKeyExA,
    (
        _In_ const HKEY hKey,
        _In_ const LPCSTR lpFile,
        _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _In_ const DWORD Flags
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpFile,
    lpSecurityAttributes,
    Flags
)

VREG_INTERNAL_REFUSED_HOOK(
    RegSaveKeyExW,
    (
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpFile,
        _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _In_ const DWORD Flags
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpFile,
    lpSecurityAttributes,
    Flags
)

VREG_INTERNAL_REFUSED_HOOK(
    RegRestoreKeyA,
    (
        _In_ const HKEY hKey,
        _In_ const LPCSTR lpFile,
        _In_ const DWORD dwFlags
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpFile,
    dwFlags
)

VREG_INTERNAL_REFUSED_HOOK(
    RegRestoreKeyW,
    (
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpFile,
        _In_ const DWORD dwFlags
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpFile,
    dwFlags
)

VREG_INTERNAL_REFUSED_HOOK(
    RegLoadKeyA,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_ const LPCSTR lpFile
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpSubKey,
    lpFile
)

VREG_INTERNAL_REFUSED_HOOK(
    RegLoadKeyW,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_ const LPCWSTR lpFile
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpSubKey,
    lpFile
)

VREG_INTERNAL_REFUSED_HOOK(
    RegUnLoadKeyA,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpSubKey
)

VREG_INTERNAL_REFUSED_HOOK(
    RegUnLoadKeyW,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpSubKey
)

VREG_INTERNAL_REFUSED_HOOK(
    RegReplaceKeyA,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_ const LPCSTR lpNewFile,
        _In_ const LPCSTR lpOldFile
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpSubKey,
    lpNewFile,
    lpOldFile
)

VREG_INTERNAL_REFUSED_HOOK(
    RegReplaceKeyW,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_ const LPCWSTR lpNewFile,
        _In_ const LPCWSTR lpOldFile
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpSubKey,
    lpNewFile,
    lpOldFile
)

VREG_INTERNAL_REFUSED_HOOK(
    RegGetKeySecurity,
    (
        _In_ const HKEY hKey,
        _In_ const SECURITY_INFORMATION SecurityInformation,
        _Out_writes_bytes_opt_(*lpcbSecurityDescriptor) const PSECURITY_DESCRIPTOR pSecurityDescriptor,
        _Inout_ const LPDWORD lpcbSecurityDescriptor
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    SecurityInformation,
    pSecurityDescriptor,
    lpcbSecurityDescriptor
)

VREG_INTERNAL_REFUSED_HOOK(
    RegSetKeySecurity,
    (
        _In_ const HKEY hKey,
        _In_ const SECURITY_INFORMATION SecurityInformation,
        _In_ const PSECURITY_DESCRIPTOR pSecurityDescriptor
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    SecurityInformation,
    pSecurityDescriptor
)

VREG_INTERNAL_REFUSED_HOOK(
    RegRenameKey,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKeyName,
        _In_ const LPCWSTR lpNewKeyName
    ),
    VRegHooks_InternalRefuseOwned(hKey),
    hKey,
    lpSubKeyName,
    lpNewKeyName
)

VREG_INTERNAL_REFUSED_HOOK(
    RegCopyTreeA,
    (
        _In_ const HKEY hKeySrc,
        _In_opt_ const LPCSTR lpSubKey,
        _In_ const HKEY hKeyDest
    ),
    VRegHooks_InternalRefuseOwnedPair(hKeySrc, hKeyDest),
    hKeySrc,
    lpSubKey,
    hKeyDest
)

VREG_INTERNAL_REFUSED_HOOK(
    RegCopyTreeW,
    (
        _In_ const HKEY hKeySrc,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_ const HKEY hKeyDest
    ),
    VRegHooks_InternalRefuseOwnedPair(hKeySrc, hKeyDest),
    hKeySrc,
    lpSubKey,
    hKeyDest
)

VREG_INTERNAL_REFUSED_HOOK(
    RegCreateKeyTransactedA,
    (
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
    ),
    VRegHooks_InternalRefuseTransactedA(hKey, lpSubKey, samDesired),
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
)

VREG_INTERNAL_REFUSED_HOOK(
    RegCreateKeyTransactedW,
    (
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
    ),
    VRegHooks_InternalRefuseTransactedW(hKey, lpSubKey, samDesired),
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
)

VREG_INTERNAL_REFUSED_HOOK(
    RegOpenKeyTransactedA,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCSTR lpSubKey,
        _In_ const DWORD ulOptions,
        _In_ const REGSAM samDesired,
        _Out_ const PHKEY phkResult,
        _In_ const HANDLE hTransaction,
        _Reserved_ const PVOID pExtendedParemeter
    ),
    VRegHooks_InternalRefuseTransactedA(hKey, lpSubKey, samDesired),
    hKey,
    lpSubKey,
    ulOptions,
    samDesired,
    phkResult,
    hTransaction,
    pExtendedParemeter
)

VREG_INTERNAL_REFUSED_HOOK(
    RegOpenKeyTransactedW,
    (
        _In_ const HKEY hKey,
        _In_opt_ const LPCWSTR lpSubKey,
        _In_ const DWORD ulOptions,
        _In_ const REGSAM samDesired,
        _Out_ const PHKEY phkResult,
        _In_ const HANDLE hTransaction,
        _Reserved_ const PVOID pExtendedParemeter
    ),
    VRegHooks_InternalRefuseOpenTransactedW(hKey, lpSubKey, samDesired, hTransaction, pExtendedParemeter),
    hKey,
    lpSubKey,
    ulOptions,
    samDesired,
    phkResult,
    hTransaction,
    pExtendedParemeter
)

VREG_INTERNAL_REFUSED_HOOK(
    RegDeleteKeyTransactedA,
    (
        _In_ const HKEY hKey,
        _In_ const LPCSTR lpSubKey,
        _In_ const REGSAM samDesired,
        _Reserved_ const DWORD Reserved,
        _In_ const HANDLE hTransaction,
        _Reserved_ const PVOID pExtendedParameter
    ),
    VRegHooks_InternalRefuseTransactedA(hKey, lpSubKey, samDesired),
    hKey,
    lpSubKey,
    samDesired,
    Reserved,
    hTransaction,
    pExtendedParameter
)

VREG_INTERNAL_REFUSED_HOOK(
    RegDeleteKeyTransactedW,
    (
        _In_ const HKEY hKey,
        _In_ const LPCWSTR lpSubKey,
        _In_ const REGSAM samDesired,
        _Reserved_ const DWORD Reserved,
        _In_ const HANDLE hTransaction,
        _Reserved_ const PVOID pExtendedParameter
    ),
    VRegHooks_InternalRefuseTransactedW(hKey, lpSubKey, samDesired),
    hKey,
    lpSubKey,
    samDesired,
    Reserved,
    hTransaction,
    pExtendedParameter
)

VREG_INTERNAL_REFUSED_HOOK(
    RegDisableReflectionKey,
    (
        _In_ const HKEY hBase
    ),
    VRegHooks_InternalRefuseOwned(hBase),
    hBase
)

VREG_INTERNAL_REFUSED_HOOK(
    RegEnableReflectionKey,
    (
        _In_ const HKEY hBase
    ),
    VRegHooks_InternalRefuseOwned(hBase),
    hBase
)

VREG_INTERNAL_REFUSED_HOOK(
    RegQueryReflectionKey,
    (
        _In_ const HKEY hBase,
        _Out_ BOOL* const bIsReflectionDisabled
    ),
    VRegHooks_InternalRefuseOwned(hBase),
    hBase,
    bIsReflectionDisabled
)

VREG_INTERNAL_REFUSED_HOOK(
    RegOverridePredefKey,
    (
        _In_ const HKEY hKey,
        _In_opt_ const HKEY hNewHKey
    ),
    VRegHooks_InternalRefuseOwnedPair(hKey, hNewHKey),
    hKey,
    hNewHKey
)

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

static BYTE* g_VRegHooks_pbThunks;

static void** VReg_InternalGetThunkSlot(const SIZE_T index)
{
    return (void**)(g_VRegHooks_pbThunks + VREG_INTERNAL_THUNK_PAGE_SIZE) + index;
}

/// Writes thunk @p index, initially jumping to @p pTarget, and answers its
/// address.
static void* VReg_InternalWriteThunk(
    const SIZE_T index,
    _In_ void* const pTarget
)
{
    BYTE* const pbCode = g_VRegHooks_pbThunks + index * VREG_INTERNAL_THUNK_CODE_SIZE;
    void** const ppSlot = VReg_InternalGetThunkSlot(index);
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
static void** g_VRegHooks_ppTargets[VREG_INTERNAL_THUNK_CAPACITY];
static volatile LONG g_VRegHooks_cHooks;

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
    VRegInternalNativeFunctions* functions;
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

    WritePointerRelease((PVOID volatile*)&g_VRegHooks_pRegistry, pRegistry);
    functions = VReg_InternalGetNativeFunctions(pNative);

#define VREG_HOOK_EMULATED(name) \
    hooks[cHooks].ppTarget = (void**)&functions->name; \
    hooks[cHooks].pBody = (void*)&VRegHooks_Internal##name; \
    ++cHooks;
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_HOOK_EMULATED)
#undef VREG_HOOK_EMULATED

    // A form this system does not export has nothing to hook.
#define VREG_HOOK_FORM(name) \
    g_VRegHooks_FormTargets[VREG_INTERNAL_FORM_##name] = VReg_InternalFindRegistryExport(#name); \
    if (g_VRegHooks_FormTargets[VREG_INTERNAL_FORM_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_VRegHooks_FormTargets[VREG_INTERNAL_FORM_##name]; \
        hooks[cHooks].pBody = (void*)&VRegHooks_Internal##name; \
        ++cHooks; \
    }
    VREG_INTERNAL_REGISTRY_FORMS(VREG_HOOK_FORM)
#undef VREG_HOOK_FORM

    // A refused function this system does not export has nothing to hook.
#define VREG_HOOK_REFUSED(name) \
    g_VRegHooks_RefusedTargets[VREG_INTERNAL_REFUSED_##name] = VReg_InternalFindRegistryExport(#name); \
    if (g_VRegHooks_RefusedTargets[VREG_INTERNAL_REFUSED_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_VRegHooks_RefusedTargets[VREG_INTERNAL_REFUSED_##name]; \
        hooks[cHooks].pBody = (void*)&VRegHooks_InternalRefuse##name; \
        ++cHooks; \
    }
    VREG_INTERNAL_REFUSED_HOOKS(VREG_HOOK_REFUSED)
#undef VREG_HOOK_REFUSED

    g_VRegHooks_pbThunks =
        (BYTE*)VirtualAlloc(NULL, 2 * VREG_INTERNAL_THUNK_PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_VRegHooks_pbThunks)
    {
        InterlockedExchange(&s_isAttached, 0);
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto done;
    }

    for (i = 0; i < cHooks; ++i)
    {
        g_VRegHooks_ppTargets[i] = hooks[i].ppTarget;
        thunks[i] = VReg_InternalWriteThunk(i, hooks[i].pBody);
    }

    // The code page becomes executable and read-only.
    if (!VirtualProtect(g_VRegHooks_pbThunks, VREG_INTERNAL_THUNK_PAGE_SIZE, PAGE_EXECUTE_READ, &dwOld))
    {
        InterlockedExchange(&s_isAttached, 0);
        error = (LONG)GetLastError();
        goto done;
    }
    FlushInstructionCache(GetCurrentProcess(), g_VRegHooks_pbThunks, VREG_INTERNAL_THUNK_PAGE_SIZE);

    WriteRelease(&g_VRegHooks_cHooks, (LONG)cHooks);
    for (i = 0; i < cHooks; ++i)
    {
        // The transaction is aborted on failure, so nothing is hooked and a
        // later attempt may start over (with fresh thunks).
        error = DetourAttach(hooks[i].ppTarget, thunks[i]);
        if (error != NO_ERROR)
        {
            WriteRelease(&g_VRegHooks_cHooks, 0);
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
    const LONG cHooks = ReadAcquire(&g_VRegHooks_cHooks);
    LONG i;
    for (i = 0; i < cHooks; ++i)
        InterlockedExchangePointer(VReg_InternalGetThunkSlot((SIZE_T)i), *g_VRegHooks_ppTargets[i]);
}

#undef VREG_INTERNAL_REFUSED_HOOKS

#endif // VREG_INTERNAL_HOOKS_IMPLEMENTATION_INCLUDED
#endif // VREG_INTERNAL_HOOKS_IMPLEMENTATION
#endif // VREG_WITH_DETOURS
