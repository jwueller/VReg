/// @file vreg.h
/// @brief A virtual Windows registry for C, as an overlay over the real one, or
///  held entirely in memory.
///
/// @details Include it from any C or C++ file. In exactly one C file per
///  module, define @ref VREG_IMPLEMENTATION before including it to compile the
///  library there; it links `advapi32` by itself. @ref VREG_STATIC makes every
///  function static, and @ref VREG_MALLOC, @ref VREG_REALLOC and @ref VREG_FREE
///  replace the allocator (see "Allocation" below).
///
///  Defining @ref VREG_WITH_DETOURS wherever it is included adds the hooks
///  (@ref VRegInstallHooks and its companions). That needs Microsoft Detours:
///  `<detours/detours.h>` on the include path, and its library linked.
///
///  The file holds, in order: the public declarations, the hooks'
///  declarations, the private declarations, the implementation, and the
///  hooks' implementation. Names prefixed `VReg_` or `VREG_INTERNAL_` are
///  private.
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

#ifndef VREG_H_INCLUDED
#define VREG_H_INCLUDED

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================
// Version
// ==========================================================================

// The version of this header; VREG_VERSION_TWEAK goes up with every change.
#define VREG_VERSION_MAJOR 0
#define VREG_VERSION_MINOR 4
#define VREG_VERSION_PATCH 1
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

/// The highest virtual handle value. Real key handles are kernel handle values
/// (multiples of 4, at most tagged in bit 1 by advapi32), and predefined roots
/// lie above this (sign-extended on x64), so an odd value at or below it is
/// never a real key handle.
///
/// The range stays 31-bit on x64 too, so code that truncates an HKEY to a
/// @ref DWORD keeps a working handle.
#define VREG_INTERNAL_VIRTUAL_HKEY_BASE ((UINT_PTR)0x7FFFFFFF)

/// Whether @p hKey is a virtual handle: nonzero, odd and at most
/// @ref VREG_INTERNAL_VIRTUAL_HKEY_BASE.
VREG_DECL BOOL WINAPI VRegIsVirtualHKey(
    _In_opt_ HKEY hKey
);

/// Whether @p hKey is a predefined root, @ref HKEY_CLASSES_ROOT through
/// @ref HKEY_CURRENT_USER_LOCAL_SETTINGS: `winreg.h` builds them from a
/// negative @ref LONG, sign-extended on x64, where neither a kernel handle nor
/// a virtual one ever lies.
VREG_DECL BOOL WINAPI VRegIsPredefinedHKey(
    _In_opt_ HKEY hKey
);

// ==========================================================================
// The registry function table
// ==========================================================================

/**
 * @brief A registry: the Win32 `Reg*` family behind one opaque handle.
 *
 *  @ref VRegCreateNative and @ref VRegCreateVirtual make the two registries
 *  VReg implements; @ref VRegCreateCustom makes one over a @ref VRegVtbl and
 *  an object of the caller's, which a wrapper uses to sit in front of another
 *  registry. Callers go through the flat functions (`VRegOpenKeyExW` and so
 *  on), never through a table directly, and free every registry with
 *  @ref VRegDestroy.
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
    /// Frees what the delegate owns, before @ref VRegDestroy frees the
    /// registry. Null for a delegate its creator frees some other way.
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

VREG_DECL VRegOpenKeyExWFunction VRegOpenKeyExW;
VREG_DECL VRegCreateKeyExWFunction VRegCreateKeyExW;
VREG_DECL VRegCloseKeyFunction VRegCloseKey;
VREG_DECL VRegQueryValueExWFunction VRegQueryValueExW;
VREG_DECL VRegSetValueExWFunction VRegSetValueExW;
VREG_DECL VRegDeleteValueWFunction VRegDeleteValueW;
VREG_DECL VRegDeleteKeyExWFunction VRegDeleteKeyExW;
VREG_DECL VRegDeleteTreeWFunction VRegDeleteTreeW;
VREG_DECL VRegEnumKeyExWFunction VRegEnumKeyExW;
VREG_DECL VRegEnumValueWFunction VRegEnumValueW;
VREG_DECL VRegQueryInfoKeyWFunction VRegQueryInfoKeyW;
VREG_DECL VRegLoadMUIStringWFunction VRegLoadMUIStringW;
VREG_DECL VRegGetValueWFunction VRegGetValueW;
VREG_DECL VRegNotifyChangeKeyValueFunction VRegNotifyChangeKeyValue;
VREG_DECL VRegFlushKeyFunction VRegFlushKey;
VREG_DECL VRegOpenKeyExAFunction VRegOpenKeyExA;
VREG_DECL VRegCreateKeyExAFunction VRegCreateKeyExA;
VREG_DECL VRegQueryValueExAFunction VRegQueryValueExA;
VREG_DECL VRegSetValueExAFunction VRegSetValueExA;
VREG_DECL VRegDeleteValueAFunction VRegDeleteValueA;
VREG_DECL VRegDeleteKeyExAFunction VRegDeleteKeyExA;
VREG_DECL VRegDeleteTreeAFunction VRegDeleteTreeA;
VREG_DECL VRegEnumKeyExAFunction VRegEnumKeyExA;
VREG_DECL VRegEnumValueAFunction VRegEnumValueA;
VREG_DECL VRegQueryInfoKeyAFunction VRegQueryInfoKeyA;
VREG_DECL VRegLoadMUIStringAFunction VRegLoadMUIStringA;
VREG_DECL VRegGetValueAFunction VRegGetValueA;
VREG_DECL VRegOpenKeyAFunction VRegOpenKeyA;
VREG_DECL VRegOpenKeyWFunction VRegOpenKeyW;
VREG_DECL VRegCreateKeyAFunction VRegCreateKeyA;
VREG_DECL VRegCreateKeyWFunction VRegCreateKeyW;
VREG_DECL VRegQueryValueAFunction VRegQueryValueA;
VREG_DECL VRegQueryValueWFunction VRegQueryValueW;
VREG_DECL VRegSetValueAFunction VRegSetValueA;
VREG_DECL VRegSetValueWFunction VRegSetValueW;
VREG_DECL VRegDeleteKeyAFunction VRegDeleteKeyA;
VREG_DECL VRegDeleteKeyWFunction VRegDeleteKeyW;
VREG_DECL VRegEnumKeyAFunction VRegEnumKeyA;
VREG_DECL VRegEnumKeyWFunction VRegEnumKeyW;
VREG_DECL VRegSetKeyValueAFunction VRegSetKeyValueA;
VREG_DECL VRegSetKeyValueWFunction VRegSetKeyValueW;
VREG_DECL VRegDeleteKeyValueAFunction VRegDeleteKeyValueA;
VREG_DECL VRegDeleteKeyValueWFunction VRegDeleteKeyValueW;
VREG_DECL VRegQueryMultipleValuesAFunction VRegQueryMultipleValuesA;
VREG_DECL VRegQueryMultipleValuesWFunction VRegQueryMultipleValuesW;

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

/// Makes a registry whose calls go to the slots of @p pVtbl, which MUST
/// outlive it. @p pDelegate is the object of the implementation, which its
/// slots reach through @ref VRegGetDelegate. Freed with @ref VRegDestroy.
VREG_DECL LSTATUS WINAPI VRegCreateCustom(
    _In_ const VRegVtbl* pVtbl,
    _In_opt_ void* pDelegate,
    _Outptr_ VReg** ppRegistry
);

/// The delegate a registry @ref VRegCreateCustom made was created with; null
/// for any other registry.
VREG_DECL _Ret_maybenull_ void* WINAPI VRegGetDelegate(
    _In_ VReg* pRegistry
);

/// Frees a registry: a custom one calls its `Destroy` slot first, when it has
/// one. Null is ignored.
VREG_DECL void WINAPI VRegDestroy(
    _In_opt_ VReg* pRegistry
);

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
typedef struct VReg_Node VReg_Node;

/// A hive held entirely in memory; starts empty.
VREG_DECL LSTATUS WINAPI VRegCreateHive(
    _Outptr_ VRegHive** ppHive
);

/// A hive over the key @p pszLowerPath below @p hLowerRoot of @p pLower,
/// which MUST outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
/// @ref KEY_WOW64_32KEY or 0) is added to every lower open.
VREG_DECL LSTATUS WINAPI VRegCreateLayeredHive(
    _In_ VReg* pLower,
    _In_ HKEY hLowerRoot,
    _In_opt_z_ LPCWSTR pszLowerPath,
    _In_ REGSAM samLowerView,
    _Outptr_ VRegHive** ppHive
);

/// Frees the hive and unbinds every handle it issued; null is ignored.
VREG_DECL void WINAPI VRegDestroyHive(
    _In_opt_ VRegHive* pHive
);

/// Makes the hive immutable from now on, so its readers take no lock.
/// Safe while it is mounted and in use; there is no way back.
VREG_DECL void WINAPI VRegFreezeHive(
    _In_ VRegHive* pHive
);

VREG_DECL BOOL WINAPI VRegIsHiveFrozen(
    _In_ VRegHive* pHive
);

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
VREG_DECL LSTATUS WINAPI VRegSetHiveMuiTextValue(
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
/// one form only: the wide surface of @ref VRegVtbl, and the fields
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

/// Fills @p pFunctions from the export tables of `KernelBase.dll` and, for
/// the pre-`Ex` forms only it exports, `advapi32.dll`, never through
/// @ref GetProcAddress: an apphelp shim can make that answer a stub instead of
/// the entry point. Resolution never loads a module (VReg links advapi32, so
/// both are loaded before it runs), which makes it safe under the loader
/// lock. A function neither module exports stays null.
VREG_DECL void WINAPI VRegResolveNativeFunctions(
    _Out_ VRegNativeFunctions* pFunctions
);

/// The real registry, reached through @p pFunctions, or through
/// @ref VRegResolveNativeFunctions when that is null. The table is copied; the
/// live copy, which the hooks attach to, is @ref VRegGetNativeFunctions.
/// Freed with @ref VRegDestroy.
VREG_DECL LSTATUS WINAPI VRegCreateNative(
    _In_opt_ const VRegNativeFunctions* pFunctions,
    _Outptr_ VReg** ppRegistry
);

/// The live table of a registry @ref VRegCreateNative made: once the
/// hooks are committed it holds the trampolines, and the registry is the one
/// below them.
VREG_DECL _Ret_notnull_ VRegNativeFunctions* WINAPI VRegGetNativeFunctions(
    _In_ VReg* pRegistry
);

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
 *  loads a module. Freed with @ref VRegDestroy.
 */
VREG_DECL LSTATUS WINAPI VRegCreateVirtual(
    _In_opt_ VReg* pBase,
    _In_opt_ VRegNtQueryKeyFunction pfnNtQueryKey,
    _Outptr_ VReg** ppRegistry
);

/// Mounts @p pHive at @p pszPath below the predefined key @p hRoot (a null or
/// empty path mounts the root itself) in a registry
/// @ref VRegCreateVirtual made. @p pHive MUST outlive the registry.
/// Answers @ref ERROR_INVALID_HANDLE for a root that is not predefined or a
/// registry that is not virtual, @ref ERROR_BAD_PATHNAME for a malformed path
/// and @ref ERROR_ALREADY_EXISTS for a path already mounted. @p samView limits
/// the calls the mount answers to those of one registry view:
/// @ref KEY_WOW64_64KEY or @ref KEY_WOW64_32KEY, or 0 for both; anything else
/// answers @ref ERROR_INVALID_PARAMETER.
VREG_DECL LSTATUS WINAPI VRegMount(
    _In_ VReg* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
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
VREG_DECL LSTATUS WINAPI VRegParseScript(
    _In_reads_bytes_(cbFile) const void* pFile,
    _In_ SIZE_T cbFile,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Reads the file @p pszPath, then @ref VRegParseScript. Failures to read
/// answer the Win32 error.
VREG_DECL LSTATUS WINAPI VRegParseScriptFile(
    _In_z_ LPCWSTR pszPath,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Frees a script; null is ignored.
VREG_DECL void WINAPI VRegDestroyScript(
    _In_opt_ VRegScript* pScript
);

/// The script's operations, in order, and how many; valid until the script is
/// destroyed.
VREG_DECL _Ret_notnull_ const VRegOperation* WINAPI VRegGetScriptOperations(
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
VREG_DECL LSTATUS WINAPI VRegApplyOperations(
    _In_reads_(cOperations) const VRegOperation* pOperations,
    _In_ SIZE_T cOperations,
    _In_ VReg* pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* pFailedIndex
);

/// @ref VRegApplyOperations over a script's operations.
_Success_(return == ERROR_SUCCESS)
VREG_DECL LSTATUS WINAPI VRegApplyScript(
    _In_ const VRegScript* pScript,
    _In_ VReg* pRegistry,
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
VREG_DECL LSTATUS WINAPI VRegSnapshotScript(
    _In_ VReg* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
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
VREG_DECL LSTATUS WINAPI VRegDeltaScript(
    _In_ VRegHive* pHive,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
    _Outptr_ VRegScript** ppScript
);

/**
 * @brief Lays @p pScript out as a regedit-style file in @p format, into bytes
 *  freed with @ref VRegFree.
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
VREG_DECL LSTATUS WINAPI VRegWriteScript(
    _In_ const VRegScript* pScript,
    _In_ VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** ppBytes,
    _Out_ SIZE_T* pcbBytes
);

/// Frees memory VReg handed to the caller to free, such as the bytes of
/// @ref VRegWriteScript; null is ignored.
VREG_DECL void WINAPI VRegFree(
    _In_opt_ void* p
);

/// @ref VRegWriteScript into the file @p pszPath. An existing file is replaced
/// only once the layout has succeeded.
VREG_DECL LSTATUS WINAPI VRegWriteScriptFile(
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
/// MUST be one @ref VRegCreateNative made, and the commit turns its
/// function table into the trampolines. Both MUST outlive the process.
/// Answers the first Detours error, or @ref ERROR_ALREADY_INITIALIZED when
/// this copy attached before.
VREG_DECL LONG WINAPI VRegAttachHooks(_In_ VReg* pRegistry, _In_ VReg* pNative);

/// @ref VRegAttachHooks in a transaction of its own, enrolling only the
/// calling thread.
VREG_DECL LONG WINAPI VRegInstallHooks(_In_ VReg* pRegistry, _In_ VReg* pNative);

/// Makes every hook of this copy pass calls straight through, for good.
VREG_DECL void WINAPI VRegNeutralizeHooks(void);

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
#define VREG_ASSERT(x) ((x) ? (void)0 : VReg_AssertFailed())
#else
#define VREG_ASSERT(x) ((void)0)
#endif
#endif

VREG_DECL void VReg_AssertFailed(void);

/// Null for @p cb of zero or when memory ran out.
VREG_DECL _Ret_maybenull_ void* VReg_Alloc(
    SIZE_T cb
);

/// Like @ref VReg_Alloc, zero-filled.
VREG_DECL _Ret_maybenull_ void* VReg_AllocZeroed(
    SIZE_T cb
);

/// Resizes @p p (null allocates) to @p cb bytes; null when memory ran out,
/// leaving @p p as it was.
VREG_DECL _Ret_maybenull_ void* VReg_ReAlloc(
    _In_opt_ void* p,
    SIZE_T cb
);

/// Frees @p p; null is ignored.
VREG_DECL void VReg_Free(
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
} VReg_WStringView;

VREG_DECL VReg_WStringView VReg_ViewOf(
    _In_opt_z_ LPCWSTR psz
);

VREG_DECL DWORD VReg_MinDword(DWORD a, DWORD b);
VREG_DECL DWORD VReg_MaxDword(DWORD a, DWORD b);

typedef struct
{
    SIZE_T cb;
    _Field_size_opt_(cb) BYTE* pb;
} VReg_BufferView;

typedef struct
{
    SIZE_T cbCapacity;
    _Field_range_(0, cbCapacity) SIZE_T cb;
    _Field_size_opt_(cbCapacity) BYTE* pb;
} VReg_Buffer;

VREG_DECL BOOL VReg_BufferReserve(
    _Inout_ VReg_Buffer* pBuffer,
    SIZE_T cbCapacity
);

/// Sets the size to @p cb, growing the capacity if needed; new bytes are
/// indeterminate.
VREG_DECL BOOL VReg_BufferResize(
    _Inout_ VReg_Buffer* pBuffer,
    SIZE_T cb
);

VREG_DECL BOOL VReg_BufferAssign(
    _Inout_ VReg_Buffer* pBuffer,
    _In_reads_bytes_opt_(cb) const void* p,
    SIZE_T cb
);

VREG_DECL BOOL VReg_BufferAppend(
    _Inout_ VReg_Buffer* pBuffer,
    _In_reads_bytes_(cb) const void* p,
    SIZE_T cb
);

VREG_DECL void VReg_BufferFree(
    _Inout_ VReg_Buffer* pBuffer
);

/// A wide string, terminated whenever @ref psz is not null; empty and
/// unallocated while it is.
typedef struct
{
    SIZE_T cchCapacity;
    _Field_range_(0, cchCapacity) SIZE_T cch;
    _Field_size_opt_(cchCapacity) wchar_t* psz;
} VReg_WString;

/// The text, `L""` for an unallocated string.
VREG_DECL _Ret_z_ LPCWSTR VReg_WStringText(
    _In_ const VReg_WString* pString
);

VREG_DECL VReg_WStringView VReg_WStringViewOf(
    _In_ const VReg_WString* pString
);

/// Room for @p cch characters and the terminator.
VREG_DECL BOOL VReg_WStringReserve(
    _Inout_ VReg_WString* pString,
    SIZE_T cch
);

/// Sets the length to @p cch, growing if needed, and terminates; new
/// characters are indeterminate.
VREG_DECL BOOL VReg_WStringResize(
    _Inout_ VReg_WString* pString,
    SIZE_T cch
);

VREG_DECL BOOL VReg_WStringAssign(
    _Inout_ VReg_WString* pString,
    _In_reads_opt_(cch) const wchar_t* pch,
    SIZE_T cch
);

VREG_DECL BOOL VReg_WStringAppend(
    _Inout_ VReg_WString* pString,
    _In_reads_(cch) const wchar_t* pch,
    SIZE_T cch
);

VREG_DECL BOOL VReg_WStringAppendChar(
    _Inout_ VReg_WString* pString,
    wchar_t ch
);

/// Shortens the string to @p cch characters, which is at most its length.
VREG_DECL void VReg_WStringTruncate(
    _Inout_ VReg_WString* pString,
    SIZE_T cch
);

VREG_DECL void VReg_WStringFree(
    _Inout_ VReg_WString* pString
);

/// An array of @ref count elements of one size, given to every call.
typedef struct
{
    void* p;
    SIZE_T count;
    SIZE_T capacity;
} VReg_Array;

/// The element at @p index of an array of @p T, as an lvalue.
#define VREG_INTERNAL_AT(pArray, T, index) (((T*)(pArray)->p)[index])

VREG_DECL BOOL VReg_ArrayReserve(
    _Inout_ VReg_Array* pArray,
    SIZE_T cbElement,
    SIZE_T capacity
);

/// Inserts a copy of @p pElement before @p index, which is at most the count.
VREG_DECL BOOL VReg_ArrayInsert(
    _Inout_ VReg_Array* pArray,
    SIZE_T cbElement,
    SIZE_T index,
    _In_reads_bytes_(cbElement) const void* pElement
);

VREG_DECL BOOL VReg_ArrayAppend(
    _Inout_ VReg_Array* pArray,
    SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* pElement
);

/// Removes the element at @p index, moving the later ones down.
VREG_DECL void VReg_ArrayRemoveAt(
    _Inout_ VReg_Array* pArray,
    SIZE_T cbElement,
    SIZE_T index
);

VREG_DECL void VReg_ArrayFree(
    _Inout_ VReg_Array* pArray
);

/// Appends a copy of @p text to @p pArray, an array of @ref VReg_WString.
VREG_DECL BOOL VReg_WStringArrayAppend(
    _Inout_ VReg_Array* pArray,
    VReg_WStringView text
);

/// Adds a copy of @p text to @p pArray, an array of @ref VReg_WString sorted
/// by @ref VReg_CompareNames, unless an equal name is there already.
VREG_DECL BOOL VReg_WStringArrayInsertSorted(
    _Inout_ VReg_Array* pArray,
    VReg_WStringView text
);

/// Whether the sorted @p pArray holds a name equal to @p text by
/// @ref VReg_CompareNames.
VREG_DECL BOOL VReg_WStringArrayContainsSorted(
    _In_ const VReg_Array* pArray,
    VReg_WStringView text
);

/// Frees the strings of @p pArray, an array of @ref VReg_WString, and the
/// array itself.
VREG_DECL void VReg_WStringArrayFree(
    _Inout_ VReg_Array* pArray
);

// ==========================================================================
// ANSI conversion
// ==========================================================================

/// Widens @p cch bytes through @ref CP_ACP into @p pText, without a
/// terminator beyond the one the string type keeps. Answers
/// @ref ERROR_OUTOFMEMORY when memory ran out or the text is longer than the
/// conversion can take.
VREG_DECL LSTATUS VReg_WideFromAnsi(
    _In_reads_(cch) const char* pch,
    SIZE_T cch,
    _Inout_ VReg_WString* pText
);

/// Narrows @p cch UTF-16 units through @ref CP_ACP into @p pText, a buffer of
/// bytes without a terminator; what the code page cannot represent becomes
/// its default character. Answers @ref ERROR_OUTOFMEMORY as
/// @ref VReg_WideFromAnsi does.
VREG_DECL LSTATUS VReg_AnsiFromWide(
    _In_reads_(cch) const wchar_t* pch,
    SIZE_T cch,
    _Inout_ VReg_Buffer* pText
);

/// A string argument that may be null: the wide copy of an ANSI argument.
typedef struct VReg_WideArg
{
    BOOL isNull;
    VReg_WString text;
} VReg_WideArg;

/// Fills @p pArg from @p psz; a zero-filled @p pArg stands for a null one.
VREG_DECL LSTATUS VReg_WideArgInit(
    _In_opt_z_ LPCSTR psz,
    _Out_ VReg_WideArg* pArg
);

/// The wide text, null when the argument was null.
VREG_DECL _Ret_maybenull_z_ LPCWSTR VReg_WideArgText(
    _In_ const VReg_WideArg* pArg
);

/// Like @ref VReg_WideArgText, as a buffer the callee may write to.
VREG_DECL _Ret_maybenull_ LPWSTR VReg_WideArgMutable(
    _In_ VReg_WideArg* pArg
);

VREG_DECL void VReg_WideArgFree(
    _Inout_ VReg_WideArg* pArg
);

// ==========================================================================
// Hive nodes
// ==========================================================================

typedef struct VReg_Value
{
    VReg_WString name;
    DWORD dwType;
    VReg_Buffer data;

    /// Hides the lower value of this name; only in a hive with a lower
    /// registry.
    BOOL isTombstone;

    /// Stored by @ref VReg_HiveSetMuiTextValue: a terminated @ref REG_SZ.
    BOOL isMuiText;
} VReg_Value;

/// A pending change notification: fires once, then is gone.
typedef struct VReg_ChangeWatch
{
    /// Our own duplicate of the caller's event; closed when the watch fires or
    /// the hive goes.
    HANDLE hEvent;

    BOOL isSubtree;
    DWORD dwFilter;
} VReg_ChangeWatch;

/// The flags below @ref isDeleted describe a node's relation to the lower
/// registry and stay clear in a hive without one.
struct VReg_Node
{
    VRegHive* pHive;
    VReg_Node* pParent;
    HKEY hKey;
    VReg_WString name;
    VReg_WString className;
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

    /// `VReg_Node*`, sorted by @ref VReg_CompareNames, the order the native
    /// registry enumerates subkeys in.
    VReg_Array children;

    /// @ref VReg_Value, in creation order, as the native registry enumerates
    /// values.
    VReg_Array values;

    /// @ref VReg_ChangeWatch, guarded by the hive's write lock.
    VReg_Array watches;
};

/// Ordinal comparison of uppercased names, the registry's own collation:
/// negative, zero or positive.
VREG_DECL int VReg_CompareNames(
    VReg_WStringView left,
    VReg_WStringView right
);

// ==========================================================================
// Export tables
// ==========================================================================

/// The entry point @p hModule exports as @p pszName, read from its export
/// table; null when @p hModule is null, the name is not exported, or the
/// export forwards to another module.
VREG_DECL _Ret_maybenull_ void* VReg_FindExport(_In_opt_ HMODULE hModule, _In_z_ const char* pszName);

/// A registry function, from `KernelBase.dll` or else `advapi32.dll`, which
/// alone exports the pre-`Ex` forms; null when neither exports it. Never
/// loads a module.
VREG_DECL _Ret_maybenull_ void* VReg_FindRegistryExport(_In_z_ const char* pszName);

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
VREG_DECL DWORD VReg_Grown(DWORD cbOld, DWORD cbRequired);

VREG_DECL LSTATUS VReg_QueryValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_z_ LPCWSTR lpValueName,
    _Out_opt_ LPDWORD lpType,
    _Inout_ VReg_Buffer* pData
);

VREG_DECL LSTATUS VReg_GetValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_opt_z_ LPCWSTR lpSubKey,
    _In_opt_z_ LPCWSTR lpValue,
    _In_ DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
    _Inout_ VReg_Buffer* pData
);

VREG_DECL LSTATUS VReg_EnumKeyOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Inout_ VReg_WString* pName
);

/// With a null @p pData only the name and type are read.
VREG_DECL LSTATUS VReg_EnumValueOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Inout_ VReg_WString* pName,
    _Out_opt_ LPDWORD lpType,
    _Inout_opt_ VReg_Buffer* pData
);

/// @ref VRegQueryInfoKeyW with the class read into @p pClass. When the read
/// fails and its last call reported a class length other than the buffer's,
/// that length is stored at @p lpcchClassReported.
VREG_DECL LSTATUS VReg_QueryInfoKeyOwned(
    _In_ VReg* pRegistry,
    _In_ HKEY hKey,
    _Inout_ VReg_WString* pClass,
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
VREG_DECL BOOL VReg_IsStringType(DWORD dwType);

// ==========================================================================
// The handle space
// ==========================================================================

/// One more than the highest index @ref VReg_VirtualHKeyFromIndex accepts.
#define VREG_INTERNAL_VIRTUAL_HKEY_COUNT ((VREG_INTERNAL_VIRTUAL_HKEY_BASE >> 1) + 1)

#define VREG_INTERNAL_HANDLE_BLOCK_SIZE ((UINT_PTR)4096)
#define VREG_INTERNAL_HANDLE_BLOCK_COUNT (VREG_INTERNAL_VIRTUAL_HKEY_COUNT / VREG_INTERNAL_HANDLE_BLOCK_SIZE)
#define VREG_INTERNAL_HANDLE_DIRECTORY_SIZE ((UINT_PTR)512)
#define VREG_INTERNAL_HANDLE_DIRECTORY_COUNT (VREG_INTERNAL_HANDLE_BLOCK_COUNT / VREG_INTERNAL_HANDLE_DIRECTORY_SIZE)

/// @ref VReg_HandleSpaceShared::magic once a copy has claimed the section:
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
} VReg_HandleSpaceShared;

typedef struct VReg_HandleDirectory VReg_HandleDirectory;

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
 *  Initialized by @ref VReg_HandleSpaceInit; the members are private to
 *  the implementation's "The handle space" section.
 *
 * @note The section is mapped once and never unmapped or closed, so an
 *  instance must outlive every handle it issued: in practice, it is never
 *  destroyed (@ref VReg_ProcessHandleSpace).
 */
typedef struct
{
    VReg_HandleSpaceShared* pShared;

    SRWLOCK lock;
    UINT_PTR nextIndex;
    UINT_PTR endIndex;

    // Two levels of block directories, filled in as blocks are reserved and
    // never freed: a lookup is two acquire loads and an index.
    VReg_HandleDirectory* volatile directories[VREG_INTERNAL_HANDLE_DIRECTORY_COUNT];
} VReg_HandleSpace;

/// Opens or creates the section @p pszName. A failure leaves the instance
/// unusable: @ref VReg_HandleSpaceAllocate then answers null.
VREG_DECL void VReg_HandleSpaceInit(
    _Out_ VReg_HandleSpace* pSpace,
    _In_z_ LPCWSTR pszName
);

/// This copy's instance over this process's section, created on first use
/// and never destroyed.
VREG_DECL _Ret_notnull_ VReg_HandleSpace* VReg_ProcessHandleSpace(void);

VREG_DECL BOOL VReg_HandleSpaceIsUsable(_In_ const VReg_HandleSpace* pSpace);

/// A fresh handle owned by @p pSpace and bound to @p pTarget, or null when
/// the section is unusable, the namespace is exhausted or memory ran out.
VREG_DECL _Ret_maybenull_ HKEY VReg_HandleSpaceAllocate(_Inout_ VReg_HandleSpace* pSpace, _In_opt_ void* pTarget);

/// Whether @p hKey is a virtual handle from one of @p pSpace's blocks.
/// Lock-free.
VREG_DECL BOOL VReg_HandleSpaceOwns(_In_ const VReg_HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// Unbinds @p hKey from its target. The value is never handed out again;
/// later lookups answer null.
VREG_DECL void VReg_HandleSpaceRelease(_Inout_ VReg_HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// The target @p hKey was allocated for; null when @p pSpace does not own
/// it. Lock-free.
VREG_DECL _Ret_maybenull_ void* VReg_HandleSpaceFind(_In_ const VReg_HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// The handle for @p index, counting down from @ref VREG_INTERNAL_VIRTUAL_HKEY_BASE in
/// odd steps; null when @p index is out of range.
VREG_DECL _Ret_maybenull_ HKEY VReg_VirtualHKeyFromIndex(UINT_PTR index);

/// The inverse of @ref VReg_VirtualHKeyFromIndex; only meaningful when
/// @ref VRegIsVirtualHKey holds.
VREG_DECL UINT_PTR VReg_IndexFromVirtualHKey(_In_opt_ HKEY hKey);

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

/// Where an open landed: a node of the hive, or (for a key a frozen layered
/// hive leaves untouched) a handle its lower registry opened, which the
/// caller now owns.
typedef struct
{
    VReg_Node* pNode;
    HKEY hLower;
} VReg_OpenResult;

/// What `RegQueryInfoKeyW` reports, as owned values. Lengths are in
/// characters without terminators, except @ref cbMaxValue.
typedef struct
{
    VReg_WString className;
    DWORD cSubKeys;
    DWORD cchMaxSubKey;
    DWORD cchMaxClass;
    DWORD cValues;
    DWORD cchMaxValueName;
    DWORD cbMaxValue;
    FILETIME lastWrite;
} VReg_KeyInfo;

typedef enum
{
    VREG_INTERNAL_UPPER_KEY,
    VREG_INTERNAL_UPPER_VALUE,
} VReg_UpperKind;

/// One entry of a hive's own layer, as @ref VReg_HiveSnapshotUpper lists
/// it; the views point into the hive and are valid during the visit only.
typedef struct
{
    VReg_UpperKind kind;

    /// The key, relative to the hive's root and separated by backslashes; empty
    /// for the root itself.
    VReg_WStringView path;

    /// Values only; empty for the default value.
    VReg_WStringView name;

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
} VReg_UpperRecord;

/// Called once per record under the hive's read lock, so it MUST NOT call
/// back into the hive; a status other than @ref ERROR_SUCCESS stops the
/// snapshot and is answered.
typedef LSTATUS (*VReg_UpperVisitor)(
    _In_opt_ void* pContext,
    _In_ const VReg_UpperRecord* pRecord
);

VREG_DECL _Ret_notnull_ VReg_Node* VReg_HiveRoot(
    _In_ VRegHive* pHive
);

/// The registry below the hive, which answers the handles in
/// @ref VReg_OpenResult::hLower; null for a hive with nothing below it.
VREG_DECL _Ret_maybenull_ VReg* VReg_HiveLower(
    _In_ VRegHive* pHive
);

/// The virtual handle of @p pNode.
VREG_DECL HKEY VReg_NodeHandle(
    _In_ const VReg_Node* pNode
);

/// The node a virtual handle of this copy of VReg belongs to; null for any
/// other handle.
VREG_DECL _Ret_maybenull_ VReg_Node* VReg_NodeOf(
    _In_opt_ HKEY hKey
);

VREG_DECL _Ret_notnull_ VRegHive* VReg_NodeHive(
    _In_ const VReg_Node* pNode
);

/// Opens the key @p cPath components below @p pParent. On a writable
/// layered hive the nodes that stand for lower keys are added on the way,
/// which changes nothing the hive answers.
VREG_DECL LSTATUS VReg_HiveOpenKey(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pParent,
    _In_reads_(cPath) const VReg_WStringView* pPath,
    SIZE_T cPath,
    REGSAM samDesired,
    _Out_ VReg_OpenResult* pResult
);

/// On a frozen hive an existing key opens as with @ref VReg_HiveOpenKey, and
/// a missing one answers @ref ERROR_ACCESS_DENIED.
VREG_DECL LSTATUS VReg_HiveCreateKey(
    _In_ VRegHive* pHive,
    _Inout_ VReg_Node* pParent,
    _In_reads_(cPath) const VReg_WStringView* pPath,
    SIZE_T cPath,
    _In_z_ LPCWSTR pszClassName,
    DWORD dwOptions,
    REGSAM samDesired,
    _Out_ VReg_OpenResult* pResult,
    _Out_ LPDWORD pdwDisposition
);

/// Deletes the last component of the path; a key with subkeys answers
/// @ref ERROR_ACCESS_DENIED, as native does.
VREG_DECL LSTATUS VReg_HiveDeleteKey(
    _In_ VRegHive* pHive,
    _Inout_ VReg_Node* pParent,
    _In_reads_(cPath) const VReg_WStringView* pPath,
    SIZE_T cPath
);

/// Deletes every subkey and value of @p pNode, leaving the node itself.
VREG_DECL LSTATUS VReg_HiveDeleteTree(
    _In_ VRegHive* pHive,
    _Inout_ VReg_Node* pNode
);

/// Reads value @p pszName of @p pNode. @p pcbData receives the size of the
/// data in bytes; @p pData receives the data itself only when it is not
/// null, so a null @p pData copies nothing.
VREG_DECL LSTATUS VReg_HiveQueryValue(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VReg_Buffer* pData
);

VREG_DECL LSTATUS VReg_HiveSetValue(
    _In_ VRegHive* pHive,
    _Inout_ VReg_Node* pNode,
    _In_z_ LPCWSTR pszName,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData
);

VREG_DECL LSTATUS VReg_HiveDeleteValue(
    _In_ VRegHive *pHive,
    _Inout_ VReg_Node* pNode,
    _In_z_ LPCWSTR pszName
);

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
VREG_DECL LSTATUS VReg_HiveSetMuiTextValue(
    _In_ VRegHive* pHive,
    _Inout_ VReg_Node* pNode,
    _In_z_ LPCWSTR pszName,
    _In_z_ LPCWSTR pszText
);

/// The text of value @p pszName of @p pNode if @ref VReg_HiveSetMuiTextValue
/// stored it, or @ref ERROR_INVALID_DATA for any other value; a value from
/// below is never marked.
VREG_DECL LSTATUS VReg_HiveQueryMuiText(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Inout_ VReg_WString* pText
);

VREG_DECL LSTATUS VReg_HiveEnumKey(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode,
    DWORD dwIndex,
    _Inout_ VReg_WString* pName,
    _Inout_ VReg_WString* pClassName,
    _Out_ FILETIME* pLastWrite
);

/// Reads the value of @p pNode at @p dwIndex. @p pcbData receives the size
/// of the data in bytes; @p pData receives the data itself only when it
/// is not null, so a null @p pData copies nothing.
VREG_DECL LSTATUS VReg_HiveEnumValue(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode,
    DWORD dwIndex,
    _Inout_ VReg_WString* pName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VReg_Buffer* pData
);

/// Fills @p pInfo, whose @ref VReg_KeyInfo::className is the caller's
/// string.
VREG_DECL LSTATUS VReg_HiveQueryInfo(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode,
    _Inout_ VReg_KeyInfo* pInfo
);

/// What `RegFlushKey` on @p pNode needs from the hive: success, or
/// @ref ERROR_KEY_DELETED.
VREG_DECL LSTATUS VReg_HiveCheckKey(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode
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
VREG_DECL LSTATUS VReg_HiveWatch(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode,
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
VREG_DECL LSTATUS VReg_HiveSnapshotUpper(
    _In_ VRegHive* pHive,
    _In_ VReg_UpperVisitor pfnVisit,
    _In_opt_ void* pContext
);

/// A key of @ref VReg_HiveLower holding value @p pszName of @p pNode, when
/// that value shows through from below; null (and success) when the hive
/// holds it or nothing does. The caller closes it through the lower
/// registry.
VREG_DECL LSTATUS VReg_HiveOpenLowerValueKey(
    _In_ VRegHive* pHive,
    _In_ VReg_Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Out_ PHKEY phLower
);

// ==========================================================================
// Registries
// ==========================================================================

/// The kinds of @ref VReg: one @ref VRegCreateCustom made over a table, and
/// the two VReg implements itself.
#define VREG_INTERNAL_KIND_CUSTOM 0
#define VREG_INTERNAL_KIND_NATIVE 1
#define VREG_INTERNAL_KIND_VIRTUAL 2

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
        /// the object its slots reach through @ref VRegGetDelegate.
        struct
        {
            const VRegVtbl* lpVtbl;
            void* pDelegate;
        } custom;

        /// @ref VREG_INTERNAL_KIND_NATIVE: the live table, see
        /// @ref VRegGetNativeFunctions.
        VRegNativeFunctions native;

        /// @ref VREG_INTERNAL_KIND_VIRTUAL.
        struct
        {
            /// Null for a registry held entirely in memory.
            VReg* pBase;

            VRegNtQueryKeyFunction pfnNtQueryKey;
            struct VReg_MountTable* volatile pMounts;
            SRWLOCK mountLock;
        } virtualRegistry;
    } implementation;
};

// ==========================================================================
// The virtual registry
// ==========================================================================

// What the flat functions call for a registry of VREG_INTERNAL_KIND_VIRTUAL.
VREG_DECL VRegOpenKeyExWFunction VReg_VirtualRegOpenKeyExW;
VREG_DECL VRegCreateKeyExWFunction VReg_VirtualRegCreateKeyExW;
VREG_DECL VRegCloseKeyFunction VReg_VirtualRegCloseKey;
VREG_DECL VRegQueryValueExWFunction VReg_VirtualRegQueryValueExW;
VREG_DECL VRegSetValueExWFunction VReg_VirtualRegSetValueExW;
VREG_DECL VRegDeleteValueWFunction VReg_VirtualRegDeleteValueW;
VREG_DECL VRegDeleteKeyExWFunction VReg_VirtualRegDeleteKeyExW;
VREG_DECL VRegDeleteTreeWFunction VReg_VirtualRegDeleteTreeW;
VREG_DECL VRegEnumKeyExWFunction VReg_VirtualRegEnumKeyExW;
VREG_DECL VRegEnumValueWFunction VReg_VirtualRegEnumValueW;
VREG_DECL VRegQueryInfoKeyWFunction VReg_VirtualRegQueryInfoKeyW;
VREG_DECL VRegLoadMUIStringWFunction VReg_VirtualRegLoadMUIStringW;
VREG_DECL VRegGetValueWFunction VReg_VirtualRegGetValueW;
VREG_DECL VRegNotifyChangeKeyValueFunction VReg_VirtualRegNotifyChangeKeyValue;
VREG_DECL VRegFlushKeyFunction VReg_VirtualRegFlushKey;

VREG_DECL void WINAPI VReg_VirtualDestroy(
    _In_ VReg* pRegistry
);

// ==========================================================================
// Paths
// ==========================================================================

/// The longest key name component the registry accepts, in characters.
#define VREG_INTERNAL_MAX_KEY_NAME_CHARS ((SIZE_T)255)

/// Splits @p path at backslashes as the native registry does, appending the
/// components (views into @p path) to @p pComponents, an array of
/// @ref VReg_WStringView: empty components are skipped, a trailing backslash
/// is ignored, a leading one answers @ref ERROR_BAD_PATHNAME and a component
/// over @ref VREG_INTERNAL_MAX_KEY_NAME_CHARS @ref ERROR_INVALID_PARAMETER.
VREG_DECL LSTATUS VReg_SplitPath(VReg_WStringView path, _Inout_ VReg_Array* pComponents);

// ==========================================================================
// Status
// ==========================================================================

/// A status a failing path answers, which the analyzer cannot prove nonzero
/// when it was stored by another function.
_Post_satisfies_(return != ERROR_SUCCESS)
VREG_DECL LSTATUS VReg_FailureStatus(LSTATUS status);

// ==========================================================================
// Scripts
// ==========================================================================

/// The storage behind one @ref VRegOperation of a script.
typedef struct
{
    VReg_WString path;
    VReg_WString name;
    VReg_Buffer data;
} VReg_ScriptEntry;

struct VRegScript
{
    /// `VRegOperation`, pointing into @ref entries.
    VReg_Array operations;

    /// `VReg_ScriptEntry`, one per operation.
    VReg_Array entries;
};

/// A predefined root a `.reg` file can name: exports write its long name,
/// imports accept either.
typedef struct VReg_RootKey
{
    const wchar_t* pszLong;
    const wchar_t* pszShort;
    HKEY hKey;
} VReg_RootKey;

/// The long name of a root a `.reg` file can name; null for any other handle.
VREG_DECL _Ret_maybenull_z_ const wchar_t* VReg_RootName(
    _In_opt_ HKEY hRoot
);

/// The root named @p name, long or short form, ignoring case.
VREG_DECL BOOL VReg_FindRoot(
    VReg_WStringView name,
    _Out_ PHKEY phRoot
);

VREG_DECL LSTATUS VReg_ScriptCreate(
    _Outptr_ VRegScript** ppScript
);

/// Appends an operation, copying its path, name and data into the script.
VREG_DECL LSTATUS VReg_ScriptAppend(
    _Inout_ VRegScript* pScript,
    VRegOperationKind kind,
    _In_ HKEY hRoot,
    VReg_WStringView path,
    VReg_WStringView name,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData,
    SIZE_T line
);

/// Decodes a whole file to text without BOM. Detects the encoding as
/// @ref VRegParseScript documents. Answers @ref ERROR_INVALID_DATA with
/// @p pError (zeroed by the caller) set on bytes the encoding cannot hold.
VREG_DECL LSTATUS VReg_DecodeFile(
    _In_reads_bytes_(cbFile) const void* pFile,
    SIZE_T cbFile,
    _Inout_ VReg_WString* pText,
    _Inout_opt_ VRegParseError* pError
);

/// The bytes of @p pText in @p format, with a BOM for Unicode. Answers
/// @ref ERROR_NO_UNICODE_TRANSLATION when the format's code page cannot hold
/// it.
VREG_DECL LSTATUS VReg_EncodeFile(
    _In_ const VReg_WString* pText,
    VRegFormat format,
    _Inout_ VReg_Buffer* pFile
);

/// Reads and writes whole files; Win32 errors answer as is.
VREG_DECL LSTATUS VReg_ReadWholeFile(
    _In_z_ LPCWSTR pszPath,
    _Inout_ VReg_Buffer* pFile
);

VREG_DECL LSTATUS VReg_WriteWholeFile(
    _In_z_ LPCWSTR pszPath,
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

VREG_DECL void VReg_AssertFailed(void)
{
    if (IsDebuggerPresent())
        __debugbreak();
}

VREG_DECL _Ret_maybenull_ void* VReg_Alloc(
    const SIZE_T cb
)
{
    if (cb == 0)
        return NULL;

    return VREG_MALLOC(cb);
}

VREG_DECL _Ret_maybenull_ void* VReg_AllocZeroed(
    const SIZE_T cb
)
{
    void* const p = VReg_Alloc(cb);
    if (p)
        ZeroMemory(p, cb);

    return p;
}

VREG_DECL _Ret_maybenull_ void* VReg_ReAlloc(
    _In_opt_ void* const p,
    const SIZE_T cb
)
{
    if (!p)
        return VReg_Alloc(cb);
    if (cb == 0)
    {
        VREG_FREE(p);
        return NULL;
    }

    return VREG_REALLOC(p, cb);
}

VREG_DECL void VReg_Free(
    _In_opt_ void* const p
)
{
    if (p)
        VREG_FREE(p);
}

// ==========================================================================
// ANSI conversion
// ==========================================================================

static SIZE_T VReg_AnsiLength(_In_z_ const char* const psz)
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
static LSTATUS VReg_WideFromMultiByte(
    const UINT codePage,
    const DWORD dwFlags,
    _In_reads_(cch) const char* const pch,
    const SIZE_T cch,
    _Inout_ VReg_WString* const pText
)
{
    int cchWide;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (!VReg_WStringResize(pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cchWide = MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, NULL, 0);
    if (cchWide <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VReg_WStringResize(pText, (SIZE_T)cchWide))
        return ERROR_OUTOFMEMORY;

    MultiByteToWideChar(codePage, dwFlags, pch, (int)cch, pText->psz, cchWide);
    return ERROR_SUCCESS;
}

/// Narrows @p cch UTF-16 units into @p pText in @p codePage with the
/// @ref WideCharToMultiByte flags @p dwFlags, reporting a default character
/// through @p pfUsedDefault. Fails as @ref VReg_WideFromMultiByte does.
static LSTATUS VReg_MultiByteFromWide(
    const UINT codePage,
    const DWORD dwFlags,
    _Out_opt_ BOOL* const pfUsedDefault,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch,
    _Inout_ VReg_Buffer* const pText
)
{
    int cbText;
    if (pfUsedDefault)
        *pfUsedDefault = FALSE;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (!VReg_BufferResize(pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return ERROR_SUCCESS;

    cbText = WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, NULL, 0, NULL, pfUsedDefault);
    if (cbText <= 0)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VReg_BufferResize(pText, (SIZE_T)cbText))
        return ERROR_OUTOFMEMORY;

    WideCharToMultiByte(codePage, dwFlags, pch, (int)cch, (char*)pText->pb, cbText, NULL, pfUsedDefault);
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VReg_WideFromAnsi(
    _In_reads_(cch) const char* const pch,
    const SIZE_T cch,
    _Inout_ VReg_WString* const pText
)
{
    return VReg_WideFromMultiByte(CP_ACP, 0, pch, cch, pText);
}

VREG_DECL LSTATUS VReg_AnsiFromWide(
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch,
    _Inout_ VReg_Buffer* const pText
)
{
    return VReg_MultiByteFromWide(CP_ACP, 0, NULL, pch, cch, pText);
}

VREG_DECL LSTATUS VReg_WideArgInit(
    _In_opt_z_ const LPCSTR psz,
    _Out_ VReg_WideArg* const pArg
)
{
    ZeroMemory(pArg, sizeof(*pArg));
    pArg->isNull = psz == NULL;
    if (!psz)
        return ERROR_SUCCESS;

    return VReg_WideFromAnsi(psz, VReg_AnsiLength(psz), &pArg->text);
}

VREG_DECL _Ret_maybenull_z_ LPCWSTR VReg_WideArgText(_In_ const VReg_WideArg* const pArg)
{
    return pArg->isNull ? NULL : VReg_WStringText(&pArg->text);
}

VREG_DECL _Ret_maybenull_ LPWSTR VReg_WideArgMutable(_In_ VReg_WideArg* const pArg)
{
    if (pArg->isNull)
        return NULL;

    // An unallocated empty string has no storage to hand out.
    if (!pArg->text.psz && !VReg_WStringReserve(&pArg->text, 0))
        return NULL;

    return pArg->text.psz;
}

VREG_DECL void VReg_WideArgFree(_Inout_ VReg_WideArg* const pArg)
{
    VReg_WStringFree(&pArg->text);
}

// ==========================================================================
// Buffers
// ==========================================================================

/// The capacity to grow to for @p needed elements: at least double the old
/// one, so repeated appends stay linear.
static SIZE_T VReg_GrownCapacity(const SIZE_T capacity, const SIZE_T needed)
{
    SIZE_T grown = capacity < 8 ? 8 : capacity * 2;
    return grown < needed ? needed : grown;
}

VREG_DECL DWORD VReg_MinDword(const DWORD a, const DWORD b)
{
    return a < b ? a : b;
}

VREG_DECL DWORD VReg_MaxDword(const DWORD a, const DWORD b)
{
    return a > b ? a : b;
}

static SIZE_T VReg_MinSize(const SIZE_T a, const SIZE_T b)
{
    return a < b ? a : b;
}

VREG_DECL VReg_WStringView VReg_ViewOf(_In_opt_z_ const LPCWSTR psz)
{
    VReg_WStringView view;
    view.pch = psz;
    view.cch = 0;
    if (psz)
        while (psz[view.cch] != L'\0')
            ++view.cch;

    return view;
}

VREG_DECL BOOL VReg_BufferReserve(
    _Inout_ VReg_Buffer* const pBuffer,
    const SIZE_T cbCapacity
)
{
    BYTE* pb;
    if (cbCapacity <= pBuffer->cbCapacity)
        return TRUE;

    pb = (BYTE*)VReg_ReAlloc(pBuffer->pb, cbCapacity);
    if (!pb)
        return FALSE;

    pBuffer->pb = pb;
    pBuffer->cbCapacity = cbCapacity;
    return TRUE;
}

VREG_DECL BOOL VReg_BufferResize(
    _Inout_ VReg_Buffer* const pBuffer,
    const SIZE_T cb
)
{
    if (cb > pBuffer->cbCapacity)
    {
        const SIZE_T cbGrown = VReg_GrownCapacity(pBuffer->cbCapacity, cb);
        if (!VReg_BufferReserve(pBuffer, cbGrown))
            return FALSE;
    }

    pBuffer->cb = cb;
    return TRUE;
}

VREG_DECL BOOL VReg_BufferAssign(
    _Inout_ VReg_Buffer* const pBuffer,
    _In_reads_bytes_opt_(cb) const void* const p,
    const SIZE_T cb
)
{
    if (!VReg_BufferResize(pBuffer, cb))
        return FALSE;

    if (cb != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(p != NULL);
        CopyMemory(pBuffer->pb, p, cb);
    }

    return TRUE;
}

VREG_DECL BOOL VReg_BufferAppend(
    _Inout_ VReg_Buffer* const pBuffer,
    _In_reads_bytes_(cb) const void* const p,
    const SIZE_T cb
)
{
    const SIZE_T cbOld = pBuffer->cb;
    if (!VReg_BufferResize(pBuffer, cbOld + cb))
        return FALSE;

    if (cb != 0)
        CopyMemory(pBuffer->pb + cbOld, p, cb);

    return TRUE;
}

VREG_DECL void VReg_BufferFree(
    _Inout_ VReg_Buffer* const pBuffer
)
{
    VReg_Free(pBuffer->pb);
    pBuffer->pb = NULL;
    pBuffer->cb = 0;
    pBuffer->cbCapacity = 0;
}

VREG_DECL _Ret_z_ LPCWSTR VReg_WStringText(
    _In_ const VReg_WString* const pString
)
{
    return pString->psz ? pString->psz : L"";
}

VREG_DECL VReg_WStringView VReg_WStringViewOf(
    _In_ const VReg_WString* const pString
)
{
    VReg_WStringView view;
    view.pch = VReg_WStringText(pString);
    view.cch = pString->cch;
    return view;
}

VREG_DECL BOOL VReg_WStringReserve(
    _Inout_ VReg_WString* const pString,
    const SIZE_T cch
)
{
    wchar_t* psz;
    if (cch < pString->cchCapacity)
        return TRUE;

    psz = (wchar_t*)VReg_ReAlloc(pString->psz, (cch + 1) * sizeof(wchar_t));
    if (!psz)
        return FALSE;

    if (!pString->psz)
        psz[0] = L'\0';

    pString->psz = psz;
    pString->cchCapacity = cch + 1;
    return TRUE;
}

VREG_DECL BOOL VReg_WStringResize(
    _Inout_ VReg_WString* const pString,
    const SIZE_T cch
)
{
    if (cch >= pString->cchCapacity)
    {
        const SIZE_T cchGrown = VReg_GrownCapacity(pString->cchCapacity, cch + 1) - 1;
        if (!VReg_WStringReserve(pString, cchGrown))
            return FALSE;
    }

    if (pString->psz)
        pString->psz[cch] = L'\0';

    pString->cch = cch;
    return TRUE;
}

VREG_DECL BOOL VReg_WStringAssign(
    _Inout_ VReg_WString* const pString,
    _In_reads_opt_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    if (!VReg_WStringResize(pString, cch))
        return FALSE;

    if (cch != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(pch != NULL);
        CopyMemory(pString->psz, pch, cch * sizeof(wchar_t));
    }

    return TRUE;
}

VREG_DECL BOOL VReg_WStringAppend(
    _Inout_ VReg_WString* const pString,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    const SIZE_T cchOld = pString->cch;
    if (!VReg_WStringResize(pString, cchOld + cch))
        return FALSE;

    if (cch != 0)
        CopyMemory(pString->psz + cchOld, pch, cch * sizeof(wchar_t));

    return TRUE;
}

VREG_DECL BOOL VReg_WStringAppendChar(
    _Inout_ VReg_WString* const pString,
    const wchar_t ch
)
{
    return VReg_WStringAppend(pString, &ch, 1);
}

/// Appends a key path component, after a backslash unless @p pPath is empty.
static BOOL VReg_AppendPathComponent(
    _Inout_ VReg_WString* const pPath,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    if (pPath->cch != 0 && !VReg_WStringAppendChar(pPath, L'\\'))
        return FALSE;

    return VReg_WStringAppend(pPath, pch, cch);
}

VREG_DECL void VReg_WStringTruncate(
    _Inout_ VReg_WString* const pString,
    const SIZE_T cch
)
{
    VREG_ASSERT(cch <= pString->cch);
    pString->cch = cch;
    if (pString->psz)
        pString->psz[cch] = L'\0';
}

VREG_DECL void VReg_WStringFree(
    _Inout_ VReg_WString* const pString
)
{
    VReg_Free(pString->psz);
    pString->psz = NULL;
    pString->cch = 0;
    pString->cchCapacity = 0;
}

VREG_DECL BOOL VReg_ArrayReserve(
    _Inout_ VReg_Array* const pArray,
    const SIZE_T cbElement,
    const SIZE_T capacity
)
{
    void* p;
    if (capacity <= pArray->capacity)
        return TRUE;

    p = VReg_ReAlloc(pArray->p, capacity * cbElement);
    if (!p)
        return FALSE;

    pArray->p = p;
    pArray->capacity = capacity;
    return TRUE;
}

VREG_DECL BOOL VReg_ArrayInsert(
    _Inout_ VReg_Array* const pArray,
    const SIZE_T cbElement,
    const SIZE_T index,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    BYTE* pbAt;
    VREG_ASSERT(index <= pArray->count);
    if (pArray->count == pArray->capacity)
    {
        const SIZE_T grown = VReg_GrownCapacity(pArray->capacity, pArray->count + 1);
        if (!VReg_ArrayReserve(pArray, cbElement, grown))
            return FALSE;
    }

    pbAt = (BYTE*)pArray->p + index * cbElement;
    MoveMemory(pbAt + cbElement, pbAt, (pArray->count - index) * cbElement);
    CopyMemory(pbAt, pElement, cbElement);
    ++pArray->count;
    return TRUE;
}

VREG_DECL BOOL VReg_ArrayAppend(
    _Inout_ VReg_Array* const pArray,
    const SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    return VReg_ArrayInsert(pArray, cbElement, pArray->count, pElement);
}

VREG_DECL void VReg_ArrayRemoveAt(
    _Inout_ VReg_Array* const pArray,
    const SIZE_T cbElement,
    const SIZE_T index
)
{
    BYTE* const pbAt = (BYTE*)pArray->p + index * cbElement;
    VREG_ASSERT(index < pArray->count);
    MoveMemory(pbAt, pbAt + cbElement, (pArray->count - index - 1) * cbElement);
    --pArray->count;
}

VREG_DECL void VReg_ArrayFree(
    _Inout_ VReg_Array* const pArray
)
{
    VReg_Free(pArray->p);
    pArray->p = NULL;
    pArray->count = 0;
    pArray->capacity = 0;
}

VREG_DECL BOOL VReg_WStringArrayAppend(
    _Inout_ VReg_Array* const pArray,
    const VReg_WStringView text
)
{
    VReg_WString copy = {0, 0, NULL};
    if (!VReg_WStringAssign(&copy, text.pch, text.cch))
        return FALSE;
    if (!VReg_ArrayAppend(pArray, sizeof(copy), &copy))
    {
        VReg_WStringFree(&copy);
        return FALSE;
    }

    return TRUE;
}

VREG_DECL void VReg_WStringArrayFree(
    _Inout_ VReg_Array* const pArray
)
{
    SIZE_T i;
    for (i = 0; i < pArray->count; ++i)
        VReg_WStringFree(&VREG_INTERNAL_AT(pArray, VReg_WString, i));

    VReg_ArrayFree(pArray);
}

VREG_DECL BOOL VReg_WStringArrayInsertSorted(
    _Inout_ VReg_Array* const pList,
    const VReg_WStringView name
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pList->count;
    VReg_WString copy = {0, 0, NULL};
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const int order = VReg_CompareNames(VReg_WStringViewOf(&VREG_INTERNAL_AT(pList, VReg_WString, mid)), name);
        if (order == 0)
            return TRUE;
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (!VReg_WStringAssign(&copy, name.pch, name.cch))
        return FALSE;
    if (!VReg_ArrayInsert(pList, sizeof(copy), lo, &copy))
    {
        VReg_WStringFree(&copy);
        return FALSE;
    }

    return TRUE;
}

VREG_DECL BOOL VReg_WStringArrayContainsSorted(
    _In_ const VReg_Array* const pList,
    const VReg_WStringView name
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pList->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const int order = VReg_CompareNames(VReg_WStringViewOf(&VREG_INTERNAL_AT(pList, VReg_WString, mid)), name);
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
static int VReg_CompareExportNames(
    _In_z_ const char* pszLeft,
    _In_z_ const char* pszRight
)
{
    while (*pszLeft != '\0' && *pszLeft == *pszRight)
    {
        ++pszLeft;
        ++pszRight;
    }

    return (int)(unsigned char)*pszLeft - (int)(unsigned char)*pszRight;
}

VREG_DECL _Ret_maybenull_ void* VReg_FindExport(
    _In_opt_ const HMODULE hModule,
    _In_z_ const char* const pszName
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
        const int order = VReg_CompareExportNames(pszName, (const char*)(pBase + pNames[middle]));
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

VREG_DECL _Ret_maybenull_ void* VReg_FindRegistryExport(
    _In_z_ const char* const pszName
)
{
    void* const pFunction = VReg_FindExport(GetModuleHandleW(L"KernelBase.dll"), pszName);
    g_pfnImportAdvapi32 = (void*)&RegOpenKeyW;
    if (pFunction)
        return pFunction;

    return VReg_FindExport(GetModuleHandleW(L"advapi32.dll"), pszName);
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
static const VRegVtbl VReg_NoSlots = {0};

/// The slots a derived form calls when they are filled: a custom registry's
/// own, and none for the kinds VReg implements.
static _Ret_notnull_ const VRegVtbl* VReg_SlotsOf(_In_ const VReg* const pRegistry)
{
    return pRegistry->dwKind == VREG_INTERNAL_KIND_CUSTOM ? pRegistry->implementation.custom.lpVtbl : &VReg_NoSlots;
}

typedef struct
{
    DWORD cbValue;
    DWORD dwType;
    DWORD ibValue;
} VReg_PlacedValue;

static DWORD VReg_AlignedToDword(const DWORD cb)
{
    return (cb + 3) & ~(DWORD)3;
}

/// Copies @p cb bytes; a null @p pSource is legal only for none.
static void VReg_CopyBytes(
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
static BOOL VReg_CopyAnsiName(
    _In_ const VReg_Buffer* const pText,
    _Out_writes_opt_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    if (!pszBuffer)
        return FALSE;

    if (pText->cb < cchBuffer)
    {
        VReg_CopyBytes(pszBuffer, pText->pb, pText->cb);
        pszBuffer[pText->cb] = '\0';
        return TRUE;
    }

    if (pText->cb == cchBuffer && cchBuffer > 0)
    {
        VReg_CopyBytes(pszBuffer, pText->pb, pText->cb - 1);
        pszBuffer[pText->cb - 1] = '\0';
    }
    return FALSE;
}

/// Like @ref VReg_CopyAnsiName for a class buffer, which native fills with as
/// much of the text as fits (and a terminator) whenever it has room for one.
static BOOL VReg_CopyAnsiClass(
    _In_ const VReg_Buffer* const pText,
    _Out_writes_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    SIZE_T cchCopied;
    if (cchBuffer == 0)
        return FALSE;

    cchCopied = VReg_MinSize(pText->cb, cchBuffer - 1);
    VReg_CopyBytes(pszBuffer, pText->pb, cchCopied);
    pszBuffer[cchCopied] = '\0';
    return pText->cb < cchBuffer;
}

VREG_DECL LSTATUS WINAPI VRegOpenKeyExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_SlotsOf(pRegistry)->RegOpenKeyExA)
        return VReg_SlotsOf(pRegistry)->RegOpenKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    VReg_WideArg subKey = {FALSE, {0, 0, NULL}};
    HKEY hPrevious;
    LSTATUS status = VReg_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    status = VRegOpenKeyExW(
        pRegistry,
        hKey,
        VReg_WideArgText(&subKey),
        ulOptions,
        samDesired,
        phkResult
    );
    // The native ANSI form leaves the result alone for an invalid handle; the
    // wide form nulls it.
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VReg_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegCreateKeyExA(
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
    if (VReg_SlotsOf(pRegistry)->RegCreateKeyExA)
        return VReg_SlotsOf(pRegistry)->RegCreateKeyExA(
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

    VReg_WideArg subKey = {FALSE, {0, 0, NULL}};
    VReg_WideArg className = {FALSE, {0, 0, NULL}};
    HKEY hPrevious;
    LSTATUS status = VReg_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_WideArgInit(lpClass, &className);
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
        VReg_WideArgText(&subKey),
        Reserved,
        VReg_WideArgMutable(&className),
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition
    );
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VReg_WideArgFree(&className);
    VReg_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegQueryValueExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegQueryValueExA)
        return VReg_SlotsOf(pRegistry)->RegQueryValueExA(
            pRegistry,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    VReg_WideArg valueName = {FALSE, {0, 0, NULL}};
    VReg_Buffer value = {0, 0, NULL};
    VReg_Buffer text = {0, 0, NULL};
    DWORD dwType;
    DWORD cbSize;
    DWORD cbText;
    LSTATUS sized;
    LSTATUS status;

    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VReg_WideArgInit(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!lpData && !lpcbData)
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            VReg_WideArgText(&valueName),
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
        VReg_WideArgText(&valueName),
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

    if (!VReg_IsStringType(dwType))
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            VReg_WideArgText(&valueName),
            NULL,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    status = VReg_QueryValueOwned(
        pRegistry,
        hKey,
        VReg_WideArgText(&valueName),
        &dwType,
        &value
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_AnsiFromWide((const wchar_t*)value.pb, value.cb / sizeof(wchar_t), &text);
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
                VReg_CopyBytes(lpData, value.pb, value.cb);
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
    VReg_BufferFree(&text);
    VReg_BufferFree(&value);
    VReg_WideArgFree(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegSetValueExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegSetValueExA)
        return VReg_SlotsOf(pRegistry)->RegSetValueExA(
            pRegistry,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    VReg_WideArg valueName = {FALSE, {0, 0, NULL}};
    VReg_WString text = {0, 0, NULL};
    LSTATUS status = VReg_WideArgInit(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg_IsStringType(dwType) || !lpData)
    {
        status = VRegSetValueExW(
            pRegistry,
            hKey,
            VReg_WideArgText(&valueName),
            Reserved,
            dwType,
            lpData,
            cbData
        );
        goto done;
    }

    status = VReg_WideFromAnsi((const char*)lpData, cbData, &text);
    if (status != ERROR_SUCCESS)
        goto done;

    // Native reads the byte after unterminated data and keeps it as the
    // terminator when it is zero, which it nearly always is. Agent's choice:
    // always terminate, so no byte past cbData is ever read; reading it, as
    // native does, can fault at the end of a page.
    if (text.cch > 0 && text.psz[text.cch - 1] != L'\0')
    {
        if (!VReg_WStringAppendChar(&text, L'\0'))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    status = VRegSetValueExW(
        pRegistry,
        hKey,
        VReg_WideArgText(&valueName),
        Reserved,
        dwType,
        (const BYTE*)VReg_WStringText(&text),
        (DWORD)(text.cch * sizeof(wchar_t))
    );

done:
    VReg_WStringFree(&text);
    VReg_WideArgFree(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegDeleteValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (VReg_SlotsOf(pRegistry)->RegDeleteValueA)
        return VReg_SlotsOf(pRegistry)->RegDeleteValueA(
            pRegistry,
            hKey,
            lpValueName
        );

    VReg_WideArg valueName = {FALSE, {0, 0, NULL}};
    LSTATUS status = VReg_WideArgInit(lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegDeleteValueW(
        pRegistry,
        hKey,
        VReg_WideArgText(&valueName)
    );

done:
    VReg_WideArgFree(&valueName);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegDeleteKeyExA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (VReg_SlotsOf(pRegistry)->RegDeleteKeyExA)
        return VReg_SlotsOf(pRegistry)->RegDeleteKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    VReg_WideArg subKey = {FALSE, {0, 0, NULL}};
    LSTATUS status = VReg_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VRegDeleteKeyExW(
        pRegistry,
        hKey,
        VReg_WideArgText(&subKey),
        samDesired,
        Reserved
    );

done:
    VReg_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegDeleteTreeA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    if (VReg_SlotsOf(pRegistry)->RegDeleteTreeA)
        return VReg_SlotsOf(pRegistry)->RegDeleteTreeA(
            pRegistry,
            hKey,
            lpSubKey
        );

    VReg_WideArg subKey = {FALSE, {0, 0, NULL}};
    LSTATUS status = VReg_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegDeleteTreeW(
        pRegistry,
        hKey,
        VReg_WideArgText(&subKey)
    );

done:
    VReg_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegEnumKeyExA(
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
    if (VReg_SlotsOf(pRegistry)->RegEnumKeyExA)
        return VReg_SlotsOf(pRegistry)->RegEnumKeyExA(
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

    VReg_WString name = {0, 0, NULL};
    VReg_WString className = {0, 0, NULL};
    VReg_Buffer nameA = {0, 0, NULL};
    VReg_Buffer classA = {0, 0, NULL};
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
        if (!VReg_WStringResize(&name, cchName)
            || !VReg_WStringResize(&className, cchClass))
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
            || (cchName >= VREG_INTERNAL_MAX_CLASS_CHARS && cchClass >= VREG_INTERNAL_MAX_CLASS_CHARS)
        )
            break;

        cchName = VReg_MinDword(cchName * 2, VREG_INTERNAL_MAX_CLASS_CHARS);
        if (wantClass)
            cchClass = VReg_MinDword(cchClass * 2, VREG_INTERNAL_MAX_CLASS_CHARS);
    }
    if (status != ERROR_SUCCESS)
        goto done;

    VReg_WStringTruncate(&name, cchNameOut);
    VReg_WStringTruncate(&className, cchClassOut);
    if (lpftLastWriteTime)
        *lpftLastWriteTime = ftLastWrite;

    status = VReg_AnsiFromWide(VReg_WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg_CopyAnsiName(&nameA, lpName, *lpcchName))
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
        status = VReg_AnsiFromWide(VReg_WStringText(&className), className.cch, &classA);
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VReg_CopyAnsiClass(&classA, lpClass, *lpcchClass))
        {
            status = ERROR_MORE_DATA;
            goto done;
        }
        *lpcchClass = (DWORD)classA.cb;
    }

    status = ERROR_SUCCESS;

done:
    VReg_BufferFree(&classA);
    VReg_BufferFree(&nameA);
    VReg_WStringFree(&className);
    VReg_WStringFree(&name);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegEnumValueA(
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
    if (VReg_SlotsOf(pRegistry)->RegEnumValueA)
        return VReg_SlotsOf(pRegistry)->RegEnumValueA(
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

    VReg_WString name = {0, 0, NULL};
    VReg_Buffer value = {0, 0, NULL};
    VReg_Buffer nameA = {0, 0, NULL};
    VReg_Buffer text = {0, 0, NULL};
    DWORD dwType = 0;
    DWORD cbValue;
    DWORD cbBuffer;
    BOOL isString;
    BOOL nameFits;
    LSTATUS status;

    if (!lpValueName || !lpcchValueName || (lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VReg_EnumValueOwned(
        pRegistry,
        hKey,
        dwIndex,
        &name,
        &dwType,
        lpcbData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_AnsiFromWide(VReg_WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    isString = VReg_IsStringType(dwType);
    cbValue = (DWORD)value.cb;
    if (lpcbData && isString)
    {
        status = VReg_AnsiFromWide((const wchar_t*)value.pb, value.cb / sizeof(wchar_t), &text);
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

    nameFits = VReg_CopyAnsiName(&nameA, lpValueName, *lpcchValueName);
    if (lpData)
    {
        if (isString)
        {
            if (value.cb <= cbBuffer)
                VReg_CopyBytes(lpData, value.pb, value.cb);
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
            VReg_CopyBytes(lpData, value.pb, cbValue);
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
    VReg_BufferFree(&text);
    VReg_BufferFree(&nameA);
    VReg_BufferFree(&value);
    VReg_WStringFree(&name);
    return status;
}

// Native answers success without writing lpClass when only the class length is
// asked for, which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VRegQueryInfoKeyA(
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
    if (VReg_SlotsOf(pRegistry)->RegQueryInfoKeyA)
        return VReg_SlotsOf(pRegistry)->RegQueryInfoKeyA(
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

    VReg_WString className = {0, 0, NULL};
    VReg_Buffer classA = {0, 0, NULL};
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

    status = VReg_QueryInfoKeyOwned(
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

    status = VReg_AnsiFromWide(VReg_WStringText(&className), className.cch, &classA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (wantClassLength)
    {
        *lpcchClass = (DWORD)classA.cb;
        status = ERROR_SUCCESS;
        goto done;
    }
    if (!VReg_CopyAnsiClass(&classA, lpClass, *lpcchClass))
    {
        *lpcchClass -= 1;
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchClass = (DWORD)classA.cb;
    status = ERROR_SUCCESS;

done:
    VReg_BufferFree(&classA);
    VReg_WStringFree(&className);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegLoadMUIStringA(
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
    if (VReg_SlotsOf(pRegistry)->RegLoadMUIStringA)
        return VReg_SlotsOf(pRegistry)->RegLoadMUIStringA(
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
VREG_DECL LSTATUS WINAPI VRegGetValueA(
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
    if (VReg_SlotsOf(pRegistry)->RegGetValueA)
        return VReg_SlotsOf(pRegistry)->RegGetValueA(
            pRegistry,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    VReg_WideArg subKey = {FALSE, {0, 0, NULL}};
    VReg_WideArg value = {FALSE, {0, 0, NULL}};
    VReg_Buffer data = {0, 0, NULL};
    VReg_Buffer stored = {0, 0, NULL};
    VReg_Buffer text = {0, 0, NULL};
    VReg_Buffer rawText = {0, 0, NULL};
    VReg_Buffer storedText = {0, 0, NULL};
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

    status = VReg_WideArgInit(lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_WideArgInit(lpValue, &value);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only string values are converted; anything else, and every failure to
    // find the value, is what the wide form answers.
    dwFlagsStored =
        (dwFlags & VREG_INTERNAL_RRF_WOW_SUBKEYS)
        | RRF_RT_ANY
        | RRF_NOEXPAND;
    dwStoredType = 0;
    statusStoredType = VRegGetValueW(
        pRegistry,
        hkey,
        VReg_WideArgText(&subKey),
        VReg_WideArgText(&value),
        dwFlagsStored,
        &dwStoredType,
        NULL,
        NULL
    );
    if (statusStoredType != ERROR_SUCCESS || !VReg_IsStringType(dwStoredType))
        goto forward;

    // A value whose type the flags reject is still read and reported, with the
    // failure status.
    dwFlagsTyped = dwFlags & ~RRF_ZEROONFAILURE;
    dwFlagsAnyType =
        (dwFlags & (VREG_INTERNAL_RRF_WOW_SUBKEYS | RRF_NOEXPAND))
        | RRF_RT_ANY;
    dwType = 0;
    cbQuery = 0;
    statusSized = VRegGetValueW(
        pRegistry,
        hkey,
        VReg_WideArgText(&subKey),
        VReg_WideArgText(&value),
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
            VReg_WideArgText(&subKey),
            VReg_WideArgText(&value),
            dwFlagsAnyType,
            &dwType,
            NULL,
            &cbQuery
        );
    }

    status = VReg_GetValueOwned(
        pRegistry,
        hkey,
        VReg_WideArgText(&subKey),
        VReg_WideArgText(&value),
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
        status = VReg_GetValueOwned(
            pRegistry,
            hkey,
            VReg_WideArgText(&subKey),
            VReg_WideArgText(&value),
            dwFlagsStored,
            &dwReadType,
            &stored
        );
        if (status != ERROR_SUCCESS)
            goto done;

        VRegGetValueW(
            pRegistry,
            hkey,
            VReg_WideArgText(&subKey),
            VReg_WideArgText(&value),
            dwFlagsStored,
            &dwReadType,
            NULL,
            &cbStoredQuery
        );
        cbStoredRaw = cbStoredQuery - VReg_MinDword(cbStoredQuery, 2);
    }

    status = VReg_AnsiFromWide((const wchar_t*)data.pb, data.cb / sizeof(wchar_t), &text);
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
        status = VReg_AnsiFromWide(
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
        cbReported = VReg_MaxDword(cbRawText, cbExpanded) + (cbExpanded > cbRawText ? 1 : 0);
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
            status = VReg_AnsiFromWide(
                (const wchar_t*)stored.pb,
                stored.cb / sizeof(wchar_t),
                &storedText
            );
            if (status != ERROR_SUCCESS)
                goto done;

            if (storedText.cb > 0 && storedText.cb <= cbBuffer)
            {
                if (cbStoredRaw <= cbBuffer)
                    VReg_CopyBytes(pvData, stored.pb, VReg_MinSize(cbStoredRaw, stored.cb));
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
            VReg_CopyBytes(pbData, stored.pb, VReg_MinSize(cbStoredRaw, stored.cb));
    }
    else
    {
        cbStored = cbQuery - VReg_MinDword(cbQuery, dwType == REG_MULTI_SZ ? 4 : 2);
        if (cbStored <= cbBuffer)
            VReg_CopyBytes(pbData, data.pb, VReg_MinSize(cbStored, data.cb));
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
        VReg_WideArgText(&subKey),
        VReg_WideArgText(&value),
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );

done:
    VReg_BufferFree(&storedText);
    VReg_BufferFree(&rawText);
    VReg_BufferFree(&text);
    VReg_BufferFree(&stored);
    VReg_BufferFree(&data);
    VReg_WideArgFree(&value);
    VReg_WideArgFree(&subKey);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegOpenKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_SlotsOf(pRegistry)->RegOpenKeyA)
        return VReg_SlotsOf(pRegistry)->RegOpenKeyA(
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
    );
}

VREG_DECL LSTATUS WINAPI VRegOpenKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_SlotsOf(pRegistry)->RegOpenKeyW)
        return VReg_SlotsOf(pRegistry)->RegOpenKeyW(
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
    );
}

VREG_DECL LSTATUS WINAPI VRegCreateKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_SlotsOf(pRegistry)->RegCreateKeyA)
        return VReg_SlotsOf(pRegistry)->RegCreateKeyA(
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
    );
}

VREG_DECL LSTATUS WINAPI VRegCreateKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (VReg_SlotsOf(pRegistry)->RegCreateKeyW)
        return VReg_SlotsOf(pRegistry)->RegCreateKeyW(
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
    );
}

VREG_DECL LSTATUS WINAPI VRegQueryValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegQueryValueA)
        return VReg_SlotsOf(pRegistry)->RegQueryValueA(
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
    return status;
}

VREG_DECL LSTATUS WINAPI VRegQueryValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegQueryValueW)
        return VReg_SlotsOf(pRegistry)->RegQueryValueW(
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
    return status;
}

VREG_DECL LSTATUS WINAPI VRegSetValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegSetValueA)
        return VReg_SlotsOf(pRegistry)->RegSetValueA(
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
        (DWORD)(VReg_AnsiLength(lpData) + 1)
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

VREG_DECL LSTATUS WINAPI VRegSetValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegSetValueW)
        return VReg_SlotsOf(pRegistry)->RegSetValueW(
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
        (DWORD)((VReg_ViewOf(lpData).cch + 1) * sizeof(wchar_t))
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;
}

VREG_DECL LSTATUS WINAPI VRegDeleteKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    if (VReg_SlotsOf(pRegistry)->RegDeleteKeyA)
        return VReg_SlotsOf(pRegistry)->RegDeleteKeyA(
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
    );
}

VREG_DECL LSTATUS WINAPI VRegDeleteKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    if (VReg_SlotsOf(pRegistry)->RegDeleteKeyW)
        return VReg_SlotsOf(pRegistry)->RegDeleteKeyW(
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
    );
}

VREG_DECL LSTATUS WINAPI VRegEnumKeyA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    if (VReg_SlotsOf(pRegistry)->RegEnumKeyA)
        return VReg_SlotsOf(pRegistry)->RegEnumKeyA(
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
    );
}

VREG_DECL LSTATUS WINAPI VRegEnumKeyW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    if (VReg_SlotsOf(pRegistry)->RegEnumKeyW)
        return VReg_SlotsOf(pRegistry)->RegEnumKeyW(
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
    );
}

// The subkey of SetKeyValue and DeleteKeyValue is opened with KEY_SET_VALUE,
// the one right the call needs; the maximum allowed access, as the pre-Ex
// forms use, was passed over. Only a caller whose access is limited to
// something else could tell.
VREG_DECL LSTATUS WINAPI VRegSetKeyValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegSetKeyValueA)
        return VReg_SlotsOf(pRegistry)->RegSetKeyValueA(
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
    return status;
}

VREG_DECL LSTATUS WINAPI VRegSetKeyValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (VReg_SlotsOf(pRegistry)->RegSetKeyValueW)
        return VReg_SlotsOf(pRegistry)->RegSetKeyValueW(
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
    return status;
}

VREG_DECL LSTATUS WINAPI VRegDeleteKeyValueA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (VReg_SlotsOf(pRegistry)->RegDeleteKeyValueA)
        return VReg_SlotsOf(pRegistry)->RegDeleteKeyValueA(
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
    return status;
}

VREG_DECL LSTATUS WINAPI VRegDeleteKeyValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (VReg_SlotsOf(pRegistry)->RegDeleteKeyValueW)
        return VReg_SlotsOf(pRegistry)->RegDeleteKeyValueW(
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
    return status;
}

/// Places the narrowed values like the wide form places the stored ones, but
/// leaves the gaps alone and rounds the total up. When they do not all fit,
/// native answers the wide form's total for the stored values.
// The SDK types lpValueBuf as a string, but it holds binary values.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VRegQueryMultipleValuesA(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (VReg_SlotsOf(pRegistry)->RegQueryMultipleValuesA)
        return VReg_SlotsOf(pRegistry)->RegQueryMultipleValuesA(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    VReg_PlacedValue* pPlaced = NULL;
    VReg_WideArg name = {FALSE, {0, 0, NULL}};
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
        pPlaced = (VReg_PlacedValue*)VReg_AllocZeroed((SIZE_T)num_vals * sizeof(*pPlaced));
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

        pPlaced[i].ibValue = i == 0 ? 0 : VReg_AlignedToDword(cbEnd);
        cbEnd = pPlaced[i].ibValue + pPlaced[i].cbValue;
    }

    if (cbEnd > cbBuffer)
    {
        cbWideEnd = 0;
        for (i = 0; i < num_vals; ++i)
        {
            cbWide = 0;
            status = VReg_WideArgInit(val_list[i].ve_valuename, &name);
            if (status == ERROR_SUCCESS)
            {
                status = VRegQueryValueExW(
                    pRegistry,
                    hKey,
                    VReg_WideArgText(&name),
                    NULL,
                    NULL,
                    NULL,
                    &cbWide
                );
            }
            VReg_WideArgFree(&name);
            if (status != ERROR_SUCCESS)
                goto done;

            cbWideEnd = (i == 0 ? 0 : VReg_AlignedToDword(cbWideEnd)) + cbWide;
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
    *ldwTotsize = VReg_AlignedToDword(cbEnd);
    status = ERROR_SUCCESS;

done:
    VReg_Free(pPlaced);
    return status;
}

/// Native places each value on a @ref DWORD boundary, reads the ones that fit
/// the buffer in order, and fills the entries only when all of them did.
VREG_DECL LSTATUS WINAPI VRegQueryMultipleValuesW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (VReg_SlotsOf(pRegistry)->RegQueryMultipleValuesW)
        return VReg_SlotsOf(pRegistry)->RegQueryMultipleValuesW(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    VReg_PlacedValue* pPlaced = NULL;
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
        pPlaced = (VReg_PlacedValue*)VReg_AllocZeroed((SIZE_T)num_vals * sizeof(*pPlaced));
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
        ibValue = i == 0 ? 0 : VReg_AlignedToDword(cbEnd);
        if (ibValue > cbEnd && cbEnd < cbBuffer)
            ZeroMemory(pbBuffer + cbEnd, VReg_MinDword(ibValue, cbBuffer) - cbEnd);

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
    VReg_Free(pPlaced);
    return status;
}

// ==========================================================================
// Handles
// ==========================================================================

VREG_DECL BOOL WINAPI VRegIsVirtualHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value != 0
        && value <= VREG_INTERNAL_VIRTUAL_HKEY_BASE
        && (value & 1) != 0;
}

VREG_DECL BOOL WINAPI VRegIsPredefinedHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value >= (UINT_PTR)HKEY_CLASSES_ROOT
        && value <= (UINT_PTR)HKEY_CURRENT_USER_LOCAL_SETTINGS;
}

// ==========================================================================
// The handle space
// ==========================================================================

typedef struct VReg_HandleBlock
{
    void* volatile targets[VREG_INTERNAL_HANDLE_BLOCK_SIZE];
} VReg_HandleBlock;

struct VReg_HandleDirectory
{
    VReg_HandleBlock* volatile blocks[VREG_INTERNAL_HANDLE_DIRECTORY_SIZE];
};

static VReg_HandleSpace g_processSpace;
static INIT_ONCE g_processSpaceOnce = INIT_ONCE_STATIC_INIT;

VREG_DECL void VReg_HandleSpaceInit(_Out_ VReg_HandleSpace* const pSpace, _In_z_ const LPCWSTR pszName)
{
    HANDLE hSection;
    VReg_HandleSpaceShared* pShared;
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
        sizeof(VReg_HandleSpaceShared),
        pszName
    );
    if (!hSection)
        return;

    pShared = (VReg_HandleSpaceShared*)MapViewOfFile(
        hSection,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(VReg_HandleSpaceShared)
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
static SIZE_T VReg_WriteDecimal(_Out_writes_z_(cchOut) wchar_t* const pszOut, const SIZE_T cchOut, DWORD value)
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

static BOOL CALLBACK VReg_InitProcessHandleSpace(
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
    VReg_WriteDecimal(wszName + cchPrefix, sizeof(wszName) / sizeof(wszName[0]) - cchPrefix, GetCurrentProcessId());
    VReg_HandleSpaceInit(&g_processSpace, wszName);
    return TRUE;
}

VREG_DECL _Ret_notnull_ VReg_HandleSpace* VReg_ProcessHandleSpace(void)
{
    // Handles issued from it outlive every teardown, so the instance is never
    // destroyed.
    InitOnceExecuteOnce(&g_processSpaceOnce, VReg_InitProcessHandleSpace, NULL, NULL);
    return &g_processSpace;
}

VREG_DECL BOOL VReg_HandleSpaceIsUsable(_In_ const VReg_HandleSpace* const pSpace)
{
    return pSpace->pShared != NULL;
}

// A slot holds null while unbound, which WritePointerRelease's annotations
// reject; the exchange accepts it, and its full barrier covers the release.
static void VReg_StoreTarget(_Inout_ void* volatile* const pSlot, _In_opt_ void* const pTarget)
{
    InterlockedExchangePointer(pSlot, pTarget);
}

static VReg_HandleBlock* VReg_FindBlock(_In_ const VReg_HandleSpace* const pSpace, const UINT_PTR block)
{
    const VReg_HandleDirectory* const pDirectory = (const VReg_HandleDirectory*)ReadPointerAcquire(
        (PVOID volatile*)&pSpace->directories[block / VREG_INTERNAL_HANDLE_DIRECTORY_SIZE]
    );
    if (!pDirectory)
        return NULL;

    return (VReg_HandleBlock*)ReadPointerAcquire(
        (PVOID volatile*)&pDirectory->blocks[block % VREG_INTERNAL_HANDLE_DIRECTORY_SIZE]
    );
}

static BOOL VReg_AddBlock(_Inout_ VReg_HandleSpace* const pSpace, const UINT_PTR block)
{
    VReg_HandleDirectory* volatile* const pDirectorySlot = &pSpace->directories[block / VREG_INTERNAL_HANDLE_DIRECTORY_SIZE];
    VReg_HandleDirectory* pDirectory = *pDirectorySlot;
    VReg_HandleBlock* pBlock;

    if (!pDirectory)
    {
        pDirectory = (VReg_HandleDirectory*)VReg_AllocZeroed(sizeof(VReg_HandleDirectory));
        if (!pDirectory)
            return FALSE;

        WritePointerRelease((PVOID volatile*)pDirectorySlot, pDirectory);
    }

    pBlock = (VReg_HandleBlock*)VReg_AllocZeroed(sizeof(VReg_HandleBlock));
    if (!pBlock)
        return FALSE;

    WritePointerRelease((PVOID volatile*)&pDirectory->blocks[block % VREG_INTERNAL_HANDLE_DIRECTORY_SIZE], pBlock);
    return TRUE;
}

VREG_DECL _Ret_maybenull_ HKEY VReg_HandleSpaceAllocate(
    _Inout_ VReg_HandleSpace* const pSpace,
    _In_opt_ void* const pTarget
)
{
    HKEY hKey = NULL;
    UINT_PTR index;
    VReg_HandleBlock* pBlock;

    if (!pSpace->pShared)
        return NULL;

    AcquireSRWLockExclusive(&pSpace->lock);

    if (pSpace->nextIndex == pSpace->endIndex)
    {
        // InterlockedIncrement returns the new value; the block reserved is the
        // one before it.
        const UINT_PTR block = (UINT_PTR)InterlockedIncrement(&pSpace->pShared->nextBlock) - 1;
        if (block >= VREG_INTERNAL_HANDLE_BLOCK_COUNT || !VReg_AddBlock(pSpace, block))
        {
            ReleaseSRWLockExclusive(&pSpace->lock);
            return NULL;
        }

        pSpace->nextIndex = block * VREG_INTERNAL_HANDLE_BLOCK_SIZE;
        pSpace->endIndex = pSpace->nextIndex + VREG_INTERNAL_HANDLE_BLOCK_SIZE;
    }

    index = pSpace->nextIndex++;
    pBlock = VReg_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    VReg_StoreTarget(&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE], pTarget);
    hKey = VReg_VirtualHKeyFromIndex(index);

    ReleaseSRWLockExclusive(&pSpace->lock);
    return hKey;
}

VREG_DECL BOOL VReg_HandleSpaceOwns(_In_ const VReg_HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    return
        VRegIsVirtualHKey(hKey)
        && VReg_FindBlock(pSpace, VReg_IndexFromVirtualHKey(hKey) / VREG_INTERNAL_HANDLE_BLOCK_SIZE) != NULL;
}

VREG_DECL void VReg_HandleSpaceRelease(_Inout_ VReg_HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    UINT_PTR index;
    VReg_HandleBlock* pBlock;

    if (!VRegIsVirtualHKey(hKey))
        return;

    index = VReg_IndexFromVirtualHKey(hKey);
    pBlock = VReg_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    if (pBlock)
        VReg_StoreTarget(&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE], NULL);
}

VREG_DECL _Ret_maybenull_ void* VReg_HandleSpaceFind(
    _In_ const VReg_HandleSpace* const pSpace,
    _In_opt_ const HKEY hKey
)
{
    UINT_PTR index;
    const VReg_HandleBlock* pBlock;

    if (!VRegIsVirtualHKey(hKey))
        return NULL;

    index = VReg_IndexFromVirtualHKey(hKey);
    pBlock = VReg_FindBlock(pSpace, index / VREG_INTERNAL_HANDLE_BLOCK_SIZE);
    if (!pBlock)
        return NULL;

    return ReadPointerAcquire((PVOID volatile*)&pBlock->targets[index % VREG_INTERNAL_HANDLE_BLOCK_SIZE]);
}

VREG_DECL _Ret_maybenull_ HKEY VReg_VirtualHKeyFromIndex(const UINT_PTR index)
{
    if (index >= VREG_INTERNAL_VIRTUAL_HKEY_COUNT)
        return NULL;

    return (HKEY)(VREG_INTERNAL_VIRTUAL_HKEY_BASE - (index << 1));
}

VREG_DECL UINT_PTR VReg_IndexFromVirtualHKey(_In_opt_ const HKEY hKey)
{
    return (VREG_INTERNAL_VIRTUAL_HKEY_BASE - (UINT_PTR)hKey) >> 1;
}

// ==========================================================================
// Hives
// ==========================================================================

struct VRegHive
{
    /// Null for a hive held entirely in memory.
    VReg* pLower;
    HKEY hLowerRoot;
    VReg_WString lowerPath;
    REGSAM samLowerView;

    SRWLOCK lock;
    volatile LONG isFrozen;

    /// Fixed for the life of the hive.
    VReg_Node* pRoot;

    /// `VReg_Node*`: nodes are created here and freed only with the hive; a
    /// deleted node stays allocated (marked deleted) so its handle keeps
    /// answering @ref ERROR_KEY_DELETED. Destroying the hive unbinds every
    /// handle it issued.
    VReg_Array nodes;
};

#define VREG_INTERNAL_NODE_AT(pArray, index) VREG_INTERNAL_AT(pArray, VReg_Node*, index)
#define VREG_INTERNAL_VALUE_AT(pArray, index) VREG_INTERNAL_AT(pArray, VReg_Value, index)
#define VREG_INTERNAL_WATCH_AT(pArray, index) VREG_INTERNAL_AT(pArray, VReg_ChangeWatch, index)
#define VREG_INTERNAL_NAME_AT(pArray, index) VREG_INTERNAL_AT(pArray, VReg_WString, index)

static const VReg_WStringView VREG_INTERNAL_NO_NAME = {NULL, 0};

VREG_DECL int VReg_CompareNames(const VReg_WStringView left, const VReg_WStringView right)
{
    if (left.cch == 0 || right.cch == 0)
        return (left.cch == 0) == (right.cch == 0) ? 0 : (left.cch == 0 ? -1 : 1);

    return CompareStringOrdinal(left.pch, (int)left.cch, right.pch, (int)right.cch, TRUE) - CSTR_EQUAL;
}

/// Whether @p status says the thing asked about exists: found, or found but
/// not readable this way.
static BOOL VReg_StatusExists(const LSTATUS status)
{
    return status == ERROR_SUCCESS || status == ERROR_MORE_DATA || status == ERROR_ACCESS_DENIED;
}

// --- Locking ---

static BOOL VReg_HiveIsFrozen(_In_ VRegHive* const pHive)
{
    return ReadAcquire(&pHive->isFrozen) != 0;
}

/// Takes the lock shared, or nothing once frozen; answers whether it did.
static BOOL VReg_HiveLockRead(_In_ VRegHive* const pHive)
{
    if (VReg_HiveIsFrozen(pHive))
        return FALSE;

    AcquireSRWLockShared(&pHive->lock);
    return TRUE;
}

static void VReg_HiveUnlockRead(_In_ VRegHive* const pHive, const BOOL isLocked)
{
    if (isLocked)
        ReleaseSRWLockShared(&pHive->lock);
}

/// Takes the lock exclusive, or nothing once frozen; answers whether it did.
/// A check of @ref VReg_HiveIsFrozen made before can be stale, since a
/// writer may wait here while @ref VRegFreezeHive runs, so only this decides a
/// write.
static BOOL VReg_HiveLockWrite(_In_ VRegHive* const pHive)
{
    AcquireSRWLockExclusive(&pHive->lock);
    if (!VReg_HiveIsFrozen(pHive))
        return TRUE;

    ReleaseSRWLockExclusive(&pHive->lock);
    return FALSE;
}

static void VReg_HiveUnlockWrite(_In_ VRegHive* const pHive)
{
    ReleaseSRWLockExclusive(&pHive->lock);
}

// --- Nodes, values and watches ---

static void VReg_ValueFree(_Inout_ VReg_Value* const pValue)
{
    VReg_WStringFree(&pValue->name);
    VReg_BufferFree(&pValue->data);
}

static void VReg_NodeClearValues(_Inout_ VReg_Node* const pNode)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
        VReg_ValueFree(&VREG_INTERNAL_VALUE_AT(&pNode->values, i));

    pNode->values.count = 0;
}

static void VReg_NodeFree(_Inout_ VReg_Node* const pNode)
{
    VReg_NodeClearValues(pNode);
    VReg_ArrayFree(&pNode->values);
    VReg_ArrayFree(&pNode->children);
    VReg_ArrayFree(&pNode->watches);
    VReg_WStringFree(&pNode->name);
    VReg_WStringFree(&pNode->className);
    VReg_Free(pNode);
}

/// Stamps @p pNode as written now.
static void VReg_HiveTouch(_Inout_ VReg_Node* const pNode)
{
    GetSystemTimeAsFileTime(&pNode->lastWrite);
}

/// Makes @p pNode and its transparent ancestors upper keys, as a write to a
/// key below them needs. The root stays as it is: it is the mount point.
static void VReg_HiveMaterialize(_Inout_ VReg_Node* const pNode)
{
    VReg_Node* pCurrent;
    for (pCurrent = pNode; pCurrent->pParent && pCurrent->isTransparent; pCurrent = pCurrent->pParent)
        pCurrent->isTransparent = FALSE;
}

static void VReg_HiveDropWatches(_Inout_ VReg_Node* const pNode)
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
static void VReg_HiveFire(_Inout_ VReg_Node* const pNode, const DWORD dwFilter, const BOOL isInherited)
{
    SIZE_T i = 0;
    while (i < pNode->watches.count)
    {
        const VReg_ChangeWatch* const pWatch = &VREG_INTERNAL_WATCH_AT(&pNode->watches, i);
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
        VReg_ArrayRemoveAt(&pNode->watches, sizeof(*pWatch), i);
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
static void VReg_HiveNotify(_Inout_ VReg_Node* const pNode, const DWORD dwFilter)
{
    VReg_Node* pAncestor;
    VReg_HiveFire(pNode, dwFilter, FALSE);
    for (pAncestor = pNode->pParent; pAncestor; pAncestor = pAncestor->pParent)
        VReg_HiveFire(pAncestor, dwFilter, TRUE);
}

/// The child of @p pParent named @p name, or null; @p pIndex, if given,
/// receives where it is or where it would be inserted.
static _Ret_maybenull_ VReg_Node* VReg_HiveFindChild(
    _In_ const VReg_Node* const pParent,
    const VReg_WStringView name,
    _Out_opt_ SIZE_T* const pIndex
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pParent->children.count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        if (VReg_CompareNames(VReg_WStringViewOf(&VREG_INTERNAL_NODE_AT(&pParent->children, mid)->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (pIndex)
        *pIndex = lo;
    if (lo < pParent->children.count)
    {
        VReg_Node* const pChild = VREG_INTERNAL_NODE_AT(&pParent->children, lo);
        if (VReg_CompareNames(VReg_WStringViewOf(&pChild->name), name) == 0)
            return pChild;
    }

    return NULL;
}

static _Ret_maybenull_ VReg_Value* VReg_HiveFindValue(_In_ VReg_Node* const pNode, const VReg_WStringView name)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
    {
        VReg_Value* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        if (VReg_CompareNames(VReg_WStringViewOf(&pValue->name), name) == 0)
            return pValue;
    }

    return NULL;
}

/// A new child of @p pParent, with its handle. The write lock must be held.
static LSTATUS VReg_HiveAddChild(
    _In_ VRegHive* const pHive,
    _Inout_ VReg_Node* const pParent,
    const VReg_WStringView name,
    const VReg_WStringView className,
    const BOOL isVolatile,
    _Outptr_ VReg_Node** const ppChild
)
{
    SIZE_T index = 0;
    VReg_Node* const pNode = (VReg_Node*)VReg_AllocZeroed(sizeof(VReg_Node));
    if (!pNode)
        return ERROR_OUTOFMEMORY;

    pNode->pHive = pHive;
    pNode->pParent = pParent;
    pNode->isVolatile = isVolatile;
    VReg_HiveTouch(pNode);
    VReg_HiveFindChild(pParent, name, &index);

    // Every allocation happens before the handle is bound, so a failure
    // leaves no handle pointing at a freed node.
    if (
        !VReg_WStringAssign(&pNode->name, name.pch, name.cch)
        || !VReg_WStringAssign(&pNode->className, className.pch, className.cch)
        || !VReg_ArrayReserve(&pHive->nodes, sizeof(pNode), pHive->nodes.count + 1)
        || !VReg_ArrayReserve(&pParent->children, sizeof(pNode), pParent->children.count + 1)
    )
    {
        VReg_NodeFree(pNode);
        return ERROR_OUTOFMEMORY;
    }

    pNode->hKey = VReg_HandleSpaceAllocate(VReg_ProcessHandleSpace(), pNode);
    if (!pNode->hKey)
    {
        VReg_NodeFree(pNode);
        return ERROR_OUTOFMEMORY;
    }

    VReg_ArrayInsert(&pParent->children, sizeof(pNode), index, &pNode);
    VReg_ArrayAppend(&pHive->nodes, sizeof(pNode), &pNode);
    VReg_HiveTouch(pParent);
    *ppChild = pNode;
    return ERROR_SUCCESS;
}

/// Children of a deleted key answer as deleted too.
static void VReg_HiveMarkDeleted(_Inout_ VReg_Node* const pNode)
{
    SIZE_T i;
    pNode->isDeleted = TRUE;
    VReg_HiveFire(pNode, ~(DWORD)0, FALSE);
    for (i = 0; i < pNode->children.count; ++i)
        VReg_HiveMarkDeleted(VREG_INTERNAL_NODE_AT(&pNode->children, i));
}

/// Marks @p pNode and everything below it deleted, unlinks it from its
/// parent and fires the watches on it and below it, and the name watches
/// on its parent. The write lock must be held.
static void VReg_HiveRemove(_Inout_ VReg_Node* const pNode)
{
    if (pNode->pParent)
    {
        SIZE_T index = 0;
        VReg_HiveFindChild(pNode->pParent, VReg_WStringViewOf(&pNode->name), &index);
        VReg_ArrayRemoveAt(&pNode->pParent->children, sizeof(pNode), index);
        VReg_HiveTouch(pNode->pParent);
    }

    VReg_HiveMarkDeleted(pNode);
    if (pNode->pParent)
        VReg_HiveNotify(pNode->pParent, REG_NOTIFY_CHANGE_NAME);
}

// --- The lower registry ---

/// Appends @p cNames path components to @p pPath, separated by backslashes.
static BOOL VReg_HiveJoinPath(
    _Inout_ VReg_WString* const pPath,
    _In_reads_(cNames) const VReg_WStringView* const pNames,
    const SIZE_T cNames
)
{
    SIZE_T i;
    for (i = 0; i < cNames; ++i)
    {
        if (!VReg_AppendPathComponent(pPath, pNames[i].pch, pNames[i].cch))
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg_HiveAppendNodePath(
    _Inout_ VReg_WString* const pPath,
    _In_ const VReg_Node* const pNode
)
{
    if (!pNode->pParent)
        return TRUE;
    if (!VReg_HiveAppendNodePath(pPath, pNode->pParent))
        return FALSE;

    return VReg_AppendPathComponent(pPath, VReg_WStringText(&pNode->name), pNode->name.cch);
}

/// Whether @p pNode's lower key shows through, and its path (read under the
/// lock, used after it is released). Never visible without a lower registry.
static LSTATUS VReg_HiveViewOf(
    _In_ VRegHive* const pHive,
    _In_ const VReg_Node* const pNode,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pPath
)
{
    const VReg_Node* pCurrent;
    *pIsVisible = FALSE;
    if (!pHive->pLower)
        return ERROR_SUCCESS;

    for (pCurrent = pNode; pCurrent; pCurrent = pCurrent->pParent)
    {
        if (pCurrent->isOpaque)
            return ERROR_SUCCESS;
    }

    if (
        !VReg_WStringAssign(pPath, VReg_WStringText(&pHive->lowerPath), pHive->lowerPath.cch)
        || !VReg_HiveAppendNodePath(pPath, pNode)
    )
        return ERROR_OUTOFMEMORY;

    *pIsVisible = TRUE;
    return ERROR_SUCCESS;
}

static LSTATUS VReg_HiveOpenLower(
    _In_ VRegHive* const pHive,
    _In_ const VReg_WString* const pPath,
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

static void VReg_HiveCloseLower(_In_ VRegHive* const pHive, _In_opt_ const HKEY hKey)
{
    if (hKey)
        VRegCloseKey(
            pHive->pLower,
            hKey
        );
}

/// The class of the lower key at @p pPath; empty when there is none.
static LSTATUS VReg_HiveQueryLowerClass(
    _In_ VRegHive* const pHive,
    _In_ const VReg_WString* const pPath,
    _Inout_ VReg_WString* const pClassName
)
{
    HKEY hLower = NULL;
    LSTATUS status;

    if (!VReg_WStringResize(pClassName, 0))
        return ERROR_OUTOFMEMORY;

    status = VReg_HiveOpenLower(pHive, pPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
        return status == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : status;

    status = VReg_QueryInfoKeyOwned(
        pHive->pLower,
        hLower,
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
    VReg_HiveCloseLower(pHive, hLower);
    return status;
}

// --- Creation and destruction ---

static LSTATUS VReg_HiveCreate(
    _In_opt_ VReg* const pLower,
    _In_opt_ const HKEY hLowerRoot,
    const VReg_WStringView lowerPath,
    const REGSAM samLowerView,
    _Outptr_ VRegHive** const ppHive
)
{
    VReg_Node* pRoot;
    VRegHive* const pHive = (VRegHive*)VReg_AllocZeroed(sizeof(VRegHive));
    *ppHive = NULL;
    if (!pHive)
        return ERROR_OUTOFMEMORY;

    pHive->pLower = pLower;
    pHive->hLowerRoot = hLowerRoot;
    pHive->samLowerView = samLowerView;
    InitializeSRWLock(&pHive->lock);

    pRoot = (VReg_Node*)VReg_AllocZeroed(sizeof(VReg_Node));
    if (
        !VReg_WStringAssign(&pHive->lowerPath, lowerPath.pch, lowerPath.cch)
        || !pRoot
        || !VReg_ArrayAppend(&pHive->nodes, sizeof(pRoot), &pRoot)
    )
    {
        VReg_Free(pRoot);
        VRegDestroyHive(pHive);
        return ERROR_OUTOFMEMORY;
    }

    pRoot->pHive = pHive;
    pRoot->hKey = VReg_HandleSpaceAllocate(VReg_ProcessHandleSpace(), pRoot);
    if (!pRoot->hKey)
    {
        VRegDestroyHive(pHive);
        return ERROR_OUTOFMEMORY;
    }

    VReg_HiveTouch(pRoot);
    if (pLower)
    {
        pRoot->isTransparent = TRUE;
        pRoot->isLowerBacked = TRUE;
    }

    pHive->pRoot = pRoot;
    *ppHive = pHive;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegCreateHive(_Outptr_ VRegHive** const ppHive)
{
    return VReg_HiveCreate(NULL, NULL, VREG_INTERNAL_NO_NAME, 0, ppHive);
}

VREG_DECL LSTATUS WINAPI VRegCreateLayeredHive(
    _In_ VReg* const pLower,
    _In_ const HKEY hLowerRoot,
    _In_opt_z_ const LPCWSTR pszLowerPath,
    _In_ const REGSAM samLowerView,
    _Outptr_ VRegHive** const ppHive
)
{
    return VReg_HiveCreate(pLower, hLowerRoot, VReg_ViewOf(pszLowerPath), samLowerView, ppHive);
}

VREG_DECL void WINAPI VRegDestroyHive(_In_opt_ VRegHive* const pHive)
{
    SIZE_T i;
    if (!pHive)
        return;

    for (i = 0; i < pHive->nodes.count; ++i)
    {
        VReg_Node* const pNode = VREG_INTERNAL_NODE_AT(&pHive->nodes, i);
        VReg_HiveDropWatches(pNode);
        VReg_HandleSpaceRelease(VReg_ProcessHandleSpace(), pNode->hKey);
        VReg_NodeFree(pNode);
    }

    VReg_ArrayFree(&pHive->nodes);
    VReg_WStringFree(&pHive->lowerPath);
    VReg_Free(pHive);
}

VREG_DECL void WINAPI VRegFreezeHive(_In_ VRegHive* const pHive)
{
    SIZE_T i;

    // Taking the lock once more waits out every writer still inside; readers
    // that see the flag afterwards skip the lock and see all their writes.
    AcquireSRWLockExclusive(&pHive->lock);
    WriteRelease(&pHive->isFrozen, 1);
    for (i = 0; i < pHive->nodes.count; ++i)
        VReg_HiveDropWatches(VREG_INTERNAL_NODE_AT(&pHive->nodes, i));

    ReleaseSRWLockExclusive(&pHive->lock);
}

VREG_DECL BOOL WINAPI VRegIsHiveFrozen(_In_ VRegHive* const pHive)
{
    return VReg_HiveIsFrozen(pHive);
}

VREG_DECL _Ret_notnull_ VReg_Node* VReg_HiveRoot(_In_ VRegHive* const pHive)
{
    return pHive->pRoot;
}

VREG_DECL _Ret_maybenull_ VReg* VReg_HiveLower(_In_ VRegHive* const pHive)
{
    return pHive->pLower;
}

VREG_DECL HKEY VReg_NodeHandle(_In_ const VReg_Node* const pNode)
{
    return pNode->hKey;
}

VREG_DECL _Ret_maybenull_ VReg_Node* VReg_NodeOf(_In_opt_ const HKEY hKey)
{
    return (VReg_Node*)VReg_HandleSpaceFind(VReg_ProcessHandleSpace(), hKey);
}

VREG_DECL _Ret_notnull_ VRegHive* VReg_NodeHive(_In_ const VReg_Node* const pNode)
{
    return pNode->pHive;
}

// --- Keys ---

/// Follows @p pPath through the upper layer from @p pParent as far as it
/// goes. @p pcMatched receives how many components were found and
/// @p ppCurrent the node they led to; when the path goes on from there,
/// @p pIsVisible and @p pLowerPath describe that node's lower key.
static LSTATUS VReg_HiveWalkUpper(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pParent,
    _In_reads_(cPath) const VReg_WStringView* const pPath,
    const SIZE_T cPath,
    _Outptr_ VReg_Node** const ppCurrent,
    _Out_ SIZE_T* const pcMatched,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pLowerPath
)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    VReg_Node* pCurrent = pParent;
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
            VReg_Node* const pChild = VReg_HiveFindChild(pCurrent, pPath[i], NULL);
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
            status = VReg_HiveViewOf(pHive, pCurrent, pIsVisible, pLowerPath);
    }

    VReg_HiveUnlockRead(pHive, isLocked);
    *ppCurrent = pCurrent;
    *pcMatched = i;
    return status;
}

VREG_DECL LSTATUS VReg_HiveOpenKey(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pParent,
    _In_reads_(cPath) const VReg_WStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired,
    _Out_ VReg_OpenResult* const pResult
)
{
    VReg_Node* pCurrent = pParent;
    SIZE_T cMatched = 0;
    BOOL isVisible = FALSE;
    VReg_WString lowerPath = {0, 0, NULL};
    HKEY hLower = NULL;
    REGSAM samLower;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->hLower = NULL;

    status = VReg_HiveWalkUpper(pHive, pParent, pPath, cPath, &pCurrent, &cMatched, &isVisible, &lowerPath);
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
    if (!VReg_HiveJoinPath(&lowerPath, pPath + cMatched, cPath - cMatched))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    samLower = VReg_HiveIsFrozen(pHive) ? samDesired : KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES);
    status = VReg_HiveOpenLower(pHive, &lowerPath, samLower, &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    if (VReg_HiveIsFrozen(pHive))
    {
        pResult->hLower = hLower;
        hLower = NULL;
        goto done;
    }

    // Writable: every open inside the mount answers with a node, or a write
    // through a lower handle handed out now would miss a later copy-up.
    if (!VReg_HiveLockWrite(pHive))
    {
        // Frozen since the lower open: the frozen answer is a lower handle.
        VReg_HiveCloseLower(pHive, hLower);
        hLower = NULL;
        status = VReg_HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
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
            VReg_Node* pChild = VReg_HiveFindChild(pCurrent, pPath[i], NULL);
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

            status = VReg_HiveAddChild(pHive, pCurrent, pPath[i], VREG_INTERNAL_NO_NAME, FALSE, &pChild);
            if (status != ERROR_SUCCESS)
                break;

            pChild->isTransparent = TRUE;
            pChild->isLowerBacked = TRUE;
            pCurrent = pChild;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VReg_HiveUnlockWrite(pHive);

done:
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringFree(&lowerPath);
    return status;
}

/// How far @p pPath exists in the upper layer below @p pParent, for
/// @ref VReg_HiveCreateKey: @p pcUpper components; @p pIsBehindTombstone
/// when a tombstone stopped the walk; otherwise the lower view of the last
/// node reached, when the path goes on.
static LSTATUS VReg_HiveMeasureUpper(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pParent,
    _In_reads_(cPath) const VReg_WStringView* const pPath,
    const SIZE_T cPath,
    _Out_ SIZE_T* const pcUpper,
    _Out_ BOOL* const pIsBehindTombstone,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pViewPath
)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VReg_Node* pCurrent = pParent;
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
            const VReg_Node* const pChild = VReg_HiveFindChild(pCurrent, pPath[cUpper], NULL);
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
            status = VReg_HiveViewOf(pHive, pCurrent, pIsVisible, pViewPath);
    }

    VReg_HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREG_DECL LSTATUS VReg_HiveCreateKey(
    _In_ VRegHive* const pHive,
    _Inout_ VReg_Node* const pParent,
    _In_reads_(cPath) const VReg_WStringView* const pPath,
    const SIZE_T cPath,
    _In_z_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    _Out_ VReg_OpenResult* const pResult,
    _Out_ LPDWORD const pdwDisposition
)
{
    const VReg_WStringView className = VReg_ViewOf(pszClassName);
    const BOOL isVolatile = (dwOptions & REG_OPTION_VOLATILE) != 0;
    SIZE_T cUpper = 0;
    SIZE_T cLower;
    BOOL isBehindTombstone = FALSE;
    BOOL isVisible = FALSE;
    VReg_WString viewPath = {0, 0, NULL};
    VReg_WString probePath = {0, 0, NULL};
    VReg_Node* pCurrent;
    SIZE_T i;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->hLower = NULL;
    *pdwDisposition = REG_OPENED_EXISTING_KEY;

    if (VReg_HiveIsFrozen(pHive))
    {
        status = VReg_HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
        return status == ERROR_FILE_NOT_FOUND ? ERROR_ACCESS_DENIED : status;
    }

    // How far the path exists: in the upper layer first, then below it.
    status = VReg_HiveMeasureUpper(pHive, pParent, pPath, cPath, &cUpper, &isBehindTombstone, &isVisible, &viewPath);
    if (status != ERROR_SUCCESS)
        goto done;

    cLower = cUpper;
    if (isVisible)
    {
        for (; cLower < cPath; ++cLower)
        {
            HKEY hLower = NULL;
            if (
                !VReg_WStringAssign(&probePath, viewPath.psz, viewPath.cch)
                || !VReg_HiveJoinPath(&probePath, pPath + cUpper, cLower + 1 - cUpper)
            )
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }

            status = VReg_HiveOpenLower(pHive, &probePath, KEY_QUERY_VALUE, &hLower);
            VReg_HiveCloseLower(pHive, status == ERROR_SUCCESS ? hLower : NULL);
            if (status == ERROR_FILE_NOT_FOUND)
            {
                status = ERROR_SUCCESS;
                break;
            }
            if (!VReg_StatusExists(status))
                goto done;

            status = ERROR_SUCCESS;
        }
    }

    if (!VReg_HiveLockWrite(pHive))
    {
        status = VReg_HiveCreateKey(
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
            VReg_Node* pChild = VReg_HiveFindChild(pCurrent, pPath[i], NULL);
            VReg_Node* pNew;
            if (pChild && !pChild->isTombstone)
            {
                pCurrent = pChild;
                continue;
            }

            if (pChild)
            {
                // Created again over a deleted lower key: nothing below shows.
                const VReg_WStringView newClass = isLast ? className : VREG_INTERNAL_NO_NAME;
                if (!VReg_WStringAssign(&pChild->className, newClass.pch, newClass.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                    break;
                }

                pChild->isTombstone = FALSE;
                pChild->isOpaque = TRUE;
                VReg_HiveTouch(pChild);
                VReg_HiveMaterialize(pCurrent);
                VReg_HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
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

            status = VReg_HiveAddChild(
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
                VReg_HiveMaterialize(pCurrent);
                VReg_HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
                if (isLast)
                    *pdwDisposition = REG_CREATED_NEW_KEY;
            }

            pCurrent = pNew;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VReg_HiveUnlockWrite(pHive);

done:
    VReg_WStringFree(&viewPath);
    VReg_WStringFree(&probePath);
    return status;
}

/// What @ref VReg_HiveDeleteKey needs to know about @p pNode before it calls
/// the lower registry: the names of its tombstone children (a child that is
/// not one fails the call), its view and its parent's, and its name.
static LSTATUS VReg_HiveInspectForDelete(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    _Inout_ VReg_Array* const pTombstones,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pViewPath,
    _Out_ BOOL* const pIsParentVisible,
    _Inout_ VReg_WString* const pParentViewPath,
    _Inout_ VReg_WString* const pKeyName
)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
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
        const VReg_Node* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);
        if (!pChild->isTombstone)
            status = ERROR_ACCESS_DENIED;
        else if (!VReg_WStringArrayInsertSorted(pTombstones, VReg_WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS)
        status = VReg_HiveViewOf(pHive, pNode, pIsVisible, pViewPath);
    if (status == ERROR_SUCCESS)
        status = VReg_HiveViewOf(pHive, pNode->pParent, pIsParentVisible, pParentViewPath);
    if (status == ERROR_SUCCESS)
    {
        const VReg_WStringView keyName = VReg_WStringViewOf(&pNode->name);
        if (!VReg_WStringAssign(pKeyName, keyName.pch, keyName.cch))
            status = ERROR_OUTOFMEMORY;
    }

    VReg_HiveUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VReg_HiveDeleteKey(
    _In_ VRegHive* const pHive,
    _Inout_ VReg_Node* const pParent,
    _In_reads_(cPath) const VReg_WStringView* const pPath,
    const SIZE_T cPath
)
{
    VReg_OpenResult opened;
    VReg_Node* pNode;
    VReg_Array tombstones = {NULL, 0, 0};
    BOOL isVisible = FALSE;
    BOOL isParentVisible = FALSE;
    VReg_WString viewPath = {0, 0, NULL};
    VReg_WString parentViewPath = {0, 0, NULL};
    VReg_WString keyName = {0, 0, NULL};
    VReg_WString lowerName = {0, 0, NULL};
    HKEY hLower = NULL;
    BOOL hasLower = FALSE;
    SIZE_T i;
    LSTATUS status;

    if (VReg_HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    status = VReg_HiveOpenKey(pHive, pParent, pPath, cPath, KEY_READ, &opened);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen since the check above, so the open answered with a lower key.
    if (!opened.pNode)
    {
        VReg_HiveCloseLower(pHive, opened.hLower);
        return ERROR_ACCESS_DENIED;
    }

    pNode = opened.pNode;
    status = VReg_HiveInspectForDelete(
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
        const VReg_WStringView keyNameView = VReg_WStringViewOf(&keyName);
        if (!VReg_HiveJoinPath(&parentViewPath, &keyNameView, 1))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        status = VReg_HiveOpenLower(pHive, &parentViewPath, KEY_ENUMERATE_SUB_KEYS, &hLower);
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
            goto done;

        hasLower = status == ERROR_SUCCESS;
        status = ERROR_SUCCESS;
        for (i = 0; hasLower && isVisible; ++i)
        {
            const LSTATUS enumStatus = VReg_EnumKeyOwned(pHive->pLower, hLower, (DWORD)i, &lowerName);
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            if (enumStatus != ERROR_SUCCESS)
            {
                status = enumStatus;
                goto done;
            }
            if (!VReg_WStringArrayContainsSorted(&tombstones, VReg_WStringViewOf(&lowerName)))
            {
                status = ERROR_ACCESS_DENIED;
                goto done;
            }
        }
    }

    if (!VReg_HiveLockWrite(pHive))
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
        VReg_Node* const pNodeParent = pNode->pParent;
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
            VReg_HiveRemove(pNode);
            if (hasLower)
            {
                VReg_Node* pTombstone;
                status = VReg_HiveAddChild(
                    pHive,
                    pNodeParent,
                    VReg_WStringViewOf(&keyName),
                    VREG_INTERNAL_NO_NAME,
                    FALSE,
                    &pTombstone
                );
                if (status == ERROR_SUCCESS)
                    pTombstone->isTombstone = TRUE;
            }

            VReg_HiveMaterialize(pNodeParent);
        }
    }

    VReg_HiveUnlockWrite(pHive);

done:
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringArrayFree(&tombstones);
    VReg_WStringFree(&viewPath);
    VReg_WStringFree(&parentViewPath);
    VReg_WStringFree(&keyName);
    VReg_WStringFree(&lowerName);
    return status;
}

VREG_DECL LSTATUS VReg_HiveDeleteTree(_In_ VRegHive* const pHive, _Inout_ VReg_Node* const pNode)
{
    if (!VReg_HiveLockWrite(pHive))
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
    {
        VReg_HiveUnlockWrite(pHive);
        return ERROR_KEY_DELETED;
    }

    while (pNode->children.count != 0)
        VReg_HiveRemove(VREG_INTERNAL_NODE_AT(&pNode->children, pNode->children.count - 1));

    VReg_NodeClearValues(pNode);
    pNode->isOpaque = pHive->pLower != NULL;
    pNode->isTransparent = FALSE;
    pNode->isLowerBacked = FALSE;
    VReg_HiveMaterialize(pNode);
    VReg_HiveTouch(pNode);
    VReg_HiveNotify(pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    VReg_HiveUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

// --- Values ---

/// Reads value @p name of @p pNode from the upper layer. @p pIsFound says
/// whether the layer decided the answer; otherwise @p pIsVisible and
/// @p pViewPath describe the lower key to ask.
static LSTATUS VReg_HiveQueryUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    const VReg_WStringView name,
    _Out_ BOOL* const pIsFound,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg_Buffer* const pData,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pViewPath
)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VReg_Value* pValue;

    *pIsFound = TRUE;
    *pIsVisible = FALSE;
    *pdwType = REG_NONE;
    *pcbData = 0;
    pValue = pNode->isDeleted ? NULL : VReg_HiveFindValue(pNode, name);
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
            if (pData && !VReg_BufferAssign(pData, pValue->data.pb, pValue->data.cb))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        *pIsFound = FALSE;
        status = VReg_HiveViewOf(pHive, pNode, pIsVisible, pViewPath);
    }

    VReg_HiveUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VReg_HiveQueryValue(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg_Buffer* const pData
)
{
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg_WString viewPath = {0, 0, NULL};
    HKEY hLower = NULL;
    LSTATUS status = VReg_HiveQueryUpperValue(
        pHive,
        pNode,
        VReg_ViewOf(pszName),
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

    status = VReg_HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
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

    status = VReg_QueryValueOwned(pHive->pLower, hLower, pszName, pdwType, pData);
    if (status == ERROR_SUCCESS)
        *pcbData = (DWORD)pData->cb;

done:
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringFree(&viewPath);
    return status;
}

static LSTATUS VReg_HiveStoreValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg_Node* const pNode,
    const VReg_WStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    const BOOL isMuiText
)
{
    VReg_Buffer bytes = {0, 0, NULL};
    VReg_Value* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (!VReg_BufferAssign(&bytes, pData, cbData))
        return ERROR_OUTOFMEMORY;

    if (!VReg_HiveLockWrite(pHive))
    {
        VReg_BufferFree(&bytes);
        return ERROR_ACCESS_DENIED;
    }

    pValue = pNode->isDeleted ? NULL : VReg_HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        VReg_BufferFree(&pValue->data);
        pValue->dwType = dwType;
        pValue->data = bytes;
        pValue->isTombstone = FALSE;
        pValue->isMuiText = isMuiText;
        bytes.pb = NULL;
    }
    else
    {
        VReg_Value value;
        value.name.psz = NULL;
        value.name.cch = 0;
        value.name.cchCapacity = 0;
        value.dwType = dwType;
        value.data = bytes;
        value.isTombstone = FALSE;
        value.isMuiText = isMuiText;
        if (!VReg_WStringAssign(&value.name, name.pch, name.cch))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (!VReg_ArrayAppend(&pNode->values, sizeof(value), &value))
        {
            VReg_WStringFree(&value.name);
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            bytes.pb = NULL;
        }
    }

    if (status == ERROR_SUCCESS)
    {
        VReg_HiveMaterialize(pNode);
        VReg_HiveTouch(pNode);
        VReg_HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
    }

    VReg_HiveUnlockWrite(pHive);
    if (bytes.pb)
        VReg_BufferFree(&bytes);
    return status;
}

VREG_DECL LSTATUS VReg_HiveSetValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg_Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData
)
{
    return VReg_HiveStoreValue(pHive, pNode, VReg_ViewOf(pszName), dwType, pData, cbData, FALSE);
}

VREG_DECL LSTATUS VReg_HiveSetMuiTextValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg_Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
)
{
    const VReg_WStringView text = VReg_ViewOf(pszText);
    return VReg_HiveStoreValue(
        pHive,
        pNode,
        VReg_ViewOf(pszName),
        REG_SZ,
        (const BYTE*)pszText,
        (DWORD)((text.cch + 1) * sizeof(wchar_t)),
        TRUE
    );
}

VREG_DECL LSTATUS VReg_HiveQueryMuiText(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Inout_ VReg_WString* const pText
)
{
    const VReg_WStringView name = VReg_ViewOf(pszName);
    const BOOL isLocked = VReg_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    BOOL isFound = TRUE;
    const VReg_Value* pValue;
    DWORD dwType;
    DWORD cbData;

    pValue = pNode->isDeleted ? NULL : VReg_HiveFindValue(pNode, name);
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
            if (!VReg_WStringAssign(pText, (const wchar_t*)pValue->data.pb, cchText))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        isFound = FALSE;
    }

    VReg_HiveUnlockRead(pHive, isLocked);
    if (isFound)
        return status;

    status = VReg_HiveQueryValue(pHive, pNode, pszName, &dwType, &cbData, NULL);
    return status == ERROR_SUCCESS ? ERROR_INVALID_DATA : status;
}

VREG_DECL LSTATUS VReg_HiveDeleteValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg_Node* const pNode,
    _In_z_ const LPCWSTR pszName
)
{
    const VReg_WStringView name = VReg_ViewOf(pszName);
    BOOL isLocked;
    BOOL isInUpper = FALSE;
    BOOL isInLower = FALSE;
    BOOL isVisible = FALSE;
    VReg_WString viewPath = {0, 0, NULL};
    HKEY hLower = NULL;
    VReg_Value* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (VReg_HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    isLocked = VReg_HiveLockRead(pHive);
    pValue = pNode->isDeleted ? NULL : VReg_HiveFindValue(pNode, name);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (pValue && pValue->isTombstone)
        status = ERROR_FILE_NOT_FOUND;
    else
        isInUpper = pValue != NULL;
    if (status == ERROR_SUCCESS)
        status = VReg_HiveViewOf(pHive, pNode, &isVisible, &viewPath);
    VReg_HiveUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS)
        goto done;

    if (isVisible)
    {
        status = VReg_HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
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
            if (queryStatus != ERROR_FILE_NOT_FOUND && !VReg_StatusExists(queryStatus))
            {
                status = queryStatus;
                goto done;
            }

            isInLower = VReg_StatusExists(queryStatus);
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

    if (!VReg_HiveLockWrite(pHive))
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
        pValue = VReg_HiveFindValue(pNode, name);
        if (isInLower)
        {
            if (pValue)
            {
                VReg_BufferFree(&pValue->data);
                pValue->dwType = REG_NONE;
                pValue->isTombstone = TRUE;
                pValue->isMuiText = FALSE;
            }
            else
            {
                VReg_Value tombstone;
                ZeroMemory(&tombstone, sizeof(tombstone));
                tombstone.dwType = REG_NONE;
                tombstone.isTombstone = TRUE;
                if (!VReg_WStringAssign(&tombstone.name, name.pch, name.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                }
                else if (!VReg_ArrayAppend(&pNode->values, sizeof(tombstone), &tombstone))
                {
                    VReg_WStringFree(&tombstone.name);
                    status = ERROR_OUTOFMEMORY;
                }
            }
        }
        else if (pValue)
        {
            const SIZE_T index = (SIZE_T)(pValue - &VREG_INTERNAL_VALUE_AT(&pNode->values, 0));
            VReg_ValueFree(pValue);
            VReg_ArrayRemoveAt(&pNode->values, sizeof(*pValue), index);
        }

        if (status == ERROR_SUCCESS)
        {
            VReg_HiveMaterialize(pNode);
            VReg_HiveTouch(pNode);
            VReg_HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
        }
    }

    VReg_HiveUnlockWrite(pHive);

done:
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringFree(&viewPath);
    return status;
}

// --- Enumeration ---

/// The upper layer's part of @ref VReg_HiveEnumKey. @p pIsFound says the
/// answer is complete. Otherwise, when @p pIsClassVisible, the entry is an
/// upper key made from a lower one at @p pClassPath whose class is still to
/// be read; and when not, the lower key at @p pViewPath (if @p pIsVisible)
/// continues the enumeration past @p pcUpper upper entries, with the names in
/// @p pShadowed hidden.
static LSTATUS VReg_HiveEnumUpperKey(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg_WString* const pName,
    _Inout_ VReg_WString* const pClassName,
    _Out_ FILETIME* const pLastWrite,
    _Out_ BOOL* const pIsFound,
    _Out_ BOOL* const pIsClassVisible,
    _Inout_ VReg_WString* const pClassPath,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pViewPath,
    _Out_ DWORD* const pcUpper,
    _Inout_ VReg_Array* const pShadowed
)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
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
        const VReg_Node* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);

        // A transparent child stands for a lower key; it is listed with the
        // lower ones.
        if (pChild->isTransparent)
            continue;

        if (!pChild->isTombstone && cUpper++ == dwIndex)
        {
            if (
                !VReg_WStringAssign(pName, VReg_WStringText(&pChild->name), pChild->name.cch)
                || !VReg_WStringAssign(
                    pClassName,
                    VReg_WStringText(&pChild->className),
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

            status = VReg_HiveViewOf(pHive, pChild, pIsClassVisible, pClassPath);
            if (status == ERROR_SUCCESS && !*pIsClassVisible)
                *pIsFound = TRUE;
            break;
        }

        if (!VReg_WStringArrayInsertSorted(pShadowed, VReg_WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound && !*pIsClassVisible)
        status = VReg_HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg_HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREG_DECL LSTATUS VReg_HiveEnumKey(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg_WString* const pName,
    _Inout_ VReg_WString* const pClassName,
    _Out_ FILETIME* const pLastWrite
)
{
    BOOL isFound = FALSE;
    BOOL isClassVisible = FALSE;
    BOOL isVisible = FALSE;
    VReg_WString classPath = {0, 0, NULL};
    VReg_WString viewPath = {0, 0, NULL};
    VReg_WString nameBuffer = {0, 0, NULL};
    VReg_WString classBuffer = {0, 0, NULL};
    VReg_Array shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD cchMaxClass = 0;
    DWORD dwSkip;
    HKEY hLower = NULL;
    DWORD i;
    LSTATUS status = VReg_HiveEnumUpperKey(
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
        status = VReg_HiveQueryLowerClass(pHive, &classPath, pClassName);
        goto done;
    }

    if (!isVisible)
    {
        status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    status = VReg_HiveOpenLower(pHive, &viewPath, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hLower);
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
        !VReg_WStringResize(&nameBuffer, VREG_INTERNAL_INITIAL_NAME_CHARS)
        || !VReg_WStringResize(&classBuffer, cchMaxClass + 1)
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
        VReg_WStringView lowerName;
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
        if (VReg_WStringArrayContainsSorted(&shadowed, lowerName) || dwSkip-- != 0)
            continue;

        if (
            !VReg_WStringAssign(pName, lowerName.pch, lowerName.cch)
            || !VReg_WStringAssign(pClassName, classBuffer.psz, cchClass)
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
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringArrayFree(&shadowed);
    VReg_WStringFree(&classPath);
    VReg_WStringFree(&viewPath);
    VReg_WStringFree(&nameBuffer);
    VReg_WStringFree(&classBuffer);
    return status;
}

/// The upper layer's part of @ref VReg_HiveEnumValue, like
/// @ref VReg_HiveEnumUpperKey without the class step.
static LSTATUS VReg_HiveEnumUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg_WString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg_Buffer* const pData,
    _Out_ BOOL* const pIsFound,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pViewPath,
    _Out_ DWORD* const pcUpper,
    _Inout_ VReg_Array* const pShadowed
)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
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
        const VReg_Value* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        if (!pValue->isTombstone && cUpper++ == dwIndex)
        {
            *pIsFound = TRUE;
            if (
                !VReg_WStringAssign(pName, VReg_WStringText(&pValue->name), pValue->name.cch)
                || (pData && !VReg_BufferAssign(pData, pValue->data.pb, pValue->data.cb))
            )
            {
                status = ERROR_OUTOFMEMORY;
                break;
            }

            *pdwType = pValue->dwType;
            *pcbData = (DWORD)pValue->data.cb;
            break;
        }

        if (!VReg_WStringArrayInsertSorted(pShadowed, VReg_WStringViewOf(&pValue->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound)
        status = VReg_HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg_HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREG_DECL LSTATUS VReg_HiveEnumValue(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg_WString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg_Buffer* const pData
)
{
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg_WString viewPath = {0, 0, NULL};
    VReg_WString sizeName = {0, 0, NULL};
    VReg_Array shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD dwSkip;
    HKEY hLower = NULL;
    DWORD i;
    LSTATUS status = VReg_HiveEnumUpperValue(
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

    status = VReg_HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
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
        status = VReg_EnumValueOwned(pHive->pLower, hLower, i, pName, pdwType, NULL);
        if (status != ERROR_SUCCESS)
            goto done;

        if (VReg_WStringArrayContainsSorted(&shadowed, VReg_WStringViewOf(pName)) || dwSkip-- != 0)
            continue;

        if (pData)
        {
            // The scan fetched no data; enumerate the matching entry again
            // with it, so name, type and data all come from the one call,
            // should the entry have changed since the scan.
            status = VReg_EnumValueOwned(pHive->pLower, hLower, i, pName, pdwType, pData);
            if (status == ERROR_SUCCESS)
                *pcbData = (DWORD)pData->cb;
            goto done;
        }

        if (!VReg_WStringResize(&sizeName, pName->cch))
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
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringArrayFree(&shadowed);
    VReg_WStringFree(&viewPath);
    VReg_WStringFree(&sizeName);
    return status;
}

/// The upper layer's part of @ref VReg_HiveQueryInfo: the counts and
/// maxima of the node's own entries into @p pInfo, the names that shadow
/// lower entries, and the node's relation to its lower key.
static LSTATUS VReg_HiveQueryUpperInfo(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    _Inout_ VReg_KeyInfo* const pInfo,
    _Inout_ VReg_Array* const pShadowedKeys,
    _Inout_ VReg_Array* const pShadowedValues,
    _Out_ BOOL* const pIsTransparent,
    _Out_ BOOL* const pIsLowerBacked,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg_WString* const pViewPath
)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;

    *pIsTransparent = FALSE;
    *pIsLowerBacked = FALSE;
    *pIsVisible = FALSE;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && i < pNode->children.count; ++i)
    {
        const VReg_Node* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);
        if (pChild->isTransparent)
            continue;

        if (!VReg_WStringArrayInsertSorted(pShadowedKeys, VReg_WStringViewOf(&pChild->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pChild->isTombstone)
            continue;

        ++pInfo->cSubKeys;
        pInfo->cchMaxSubKey = VReg_MaxDword(pInfo->cchMaxSubKey, (DWORD)pChild->name.cch);
        pInfo->cchMaxClass = VReg_MaxDword(pInfo->cchMaxClass, (DWORD)pChild->className.cch);
    }

    for (i = 0; status == ERROR_SUCCESS && i < pNode->values.count; ++i)
    {
        const VReg_Value* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        if (!VReg_WStringArrayInsertSorted(pShadowedValues, VReg_WStringViewOf(&pValue->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pValue->isTombstone)
            continue;

        ++pInfo->cValues;
        pInfo->cchMaxValueName = VReg_MaxDword(pInfo->cchMaxValueName, (DWORD)pValue->name.cch);
        pInfo->cbMaxValue = VReg_MaxDword(pInfo->cbMaxValue, (DWORD)pValue->data.cb);
    }

    if (status == ERROR_SUCCESS)
    {
        *pIsTransparent = pNode->isTransparent;
        *pIsLowerBacked = pNode->isLowerBacked;
        pInfo->lastWrite = pNode->lastWrite;
        const VReg_WStringView className = VReg_WStringViewOf(&pNode->className);
        if (!VReg_WStringAssign(&pInfo->className, className.pch, className.cch))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VReg_HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg_HiveUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VReg_HiveQueryInfo(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    _Inout_ VReg_KeyInfo* const pInfo
)
{
    VReg_Array shadowedKeys = {NULL, 0, 0};
    VReg_Array shadowedValues = {NULL, 0, 0};
    BOOL isTransparent = FALSE;
    BOOL isLowerBacked = FALSE;
    BOOL isVisible = FALSE;
    VReg_WString viewPath = {0, 0, NULL};
    VReg_WString lowerClass = {0, 0, NULL};
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

    status = VReg_HiveQueryUpperInfo(
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

    status = VReg_HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hLower);
    if (status == ERROR_FILE_NOT_FOUND)
    {
        status = ERROR_SUCCESS;
        goto done;
    }
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_QueryInfoKeyOwned(
        pHive->pLower,
        hLower,
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
            VReg_WStringText(&VREG_INTERNAL_NAME_AT(&shadowedKeys, i)),
            0,
            KEY_QUERY_VALUE | pHive->samLowerView,
            &hChild
        );
        VReg_HiveCloseLower(pHive, childStatus == ERROR_SUCCESS ? hChild : NULL);
        if (childStatus != ERROR_FILE_NOT_FOUND && !VReg_StatusExists(childStatus))
        {
            status = childStatus;
            goto done;
        }
        if (VReg_StatusExists(childStatus))
            --cSubKeys;
    }

    for (i = 0; i < shadowedValues.count; ++i)
    {
        const LSTATUS valueStatus = VRegQueryValueExW(
            pHive->pLower,
            hLower,
            VReg_WStringText(&VREG_INTERNAL_NAME_AT(&shadowedValues, i)),
            NULL,
            NULL,
            NULL,
            NULL
        );
        if (valueStatus != ERROR_FILE_NOT_FOUND && !VReg_StatusExists(valueStatus))
        {
            status = valueStatus;
            goto done;
        }
        if (VReg_StatusExists(valueStatus))
            --cValues;
    }

    pInfo->cSubKeys += cSubKeys;
    pInfo->cValues += cValues;
    pInfo->cchMaxSubKey = VReg_MaxDword(pInfo->cchMaxSubKey, cchMaxSubKey);
    pInfo->cchMaxClass = VReg_MaxDword(pInfo->cchMaxClass, cchMaxClass);
    pInfo->cchMaxValueName = VReg_MaxDword(pInfo->cchMaxValueName, cchMaxValueName);
    pInfo->cbMaxValue = VReg_MaxDword(pInfo->cbMaxValue, cbMaxValue);
    if (isLowerBacked && !VReg_WStringAssign(&pInfo->className, lowerClass.psz, lowerClass.cch))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    if (isTransparent)
        pInfo->lastWrite = lastWrite;

done:
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringArrayFree(&shadowedKeys);
    VReg_WStringArrayFree(&shadowedValues);
    VReg_WStringFree(&viewPath);
    VReg_WStringFree(&lowerClass);
    return status;
}

// --- The rest ---

/// Visits @p pNode and everything below it, leaving out a transparent node
/// without values (the way to a lower key, which only the root can be while
/// it has upper keys below it). @p pPath is the node's path, extended and
/// restored for each child.
static LSTATUS VReg_HiveListUpper(
    _In_ VRegHive* const pHive,
    _In_ const VReg_Node* const pNode,
    _Inout_ VReg_WString* const pPath,
    _In_ const VReg_UpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    const BOOL isBare = pNode->isTransparent && pNode->values.count == 0;
    const SIZE_T cchPath = pPath->cch;
    VReg_UpperRecord record;
    LSTATUS status;
    SIZE_T i;

    ZeroMemory(&record, sizeof(record));
    record.path = VReg_WStringViewOf(pPath);
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
        const VReg_Value* const pValue = &VREG_INTERNAL_VALUE_AT(&pNode->values, i);
        ZeroMemory(&record, sizeof(record));
        record.kind = VREG_INTERNAL_UPPER_VALUE;
        record.path = VReg_WStringViewOf(pPath);
        record.name = VReg_WStringViewOf(&pValue->name);
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
        const VReg_Node* const pChild = VREG_INTERNAL_NODE_AT(&pNode->children, i);
        if (
            (pPath->cch != 0 && !VReg_WStringAppendChar(pPath, L'\\'))
            || !VReg_WStringAppend(pPath, VReg_WStringText(&pChild->name), pChild->name.cch)
        )
            return ERROR_OUTOFMEMORY;

        if (pChild->isTombstone)
        {
            ZeroMemory(&record, sizeof(record));
            record.kind = VREG_INTERNAL_UPPER_KEY;
            record.path = VReg_WStringViewOf(pPath);
            record.isTombstone = TRUE;
            status = pfnVisit(pContext, &record);
        }
        else
        {
            status = VReg_HiveListUpper(pHive, pChild, pPath, pfnVisit, pContext);
        }

        if (status != ERROR_SUCCESS)
            return status;

        VReg_WStringTruncate(pPath, cchPath);
    }

    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VReg_HiveSnapshotUpper(
    _In_ VRegHive* const pHive,
    _In_ const VReg_UpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    VReg_WString path = {0, 0, NULL};
    const BOOL isLocked = VReg_HiveLockRead(pHive);
    const LSTATUS status = VReg_HiveListUpper(pHive, pHive->pRoot, &path, pfnVisit, pContext);
    VReg_HiveUnlockRead(pHive, isLocked);
    VReg_WStringFree(&path);
    return status;
}

VREG_DECL LSTATUS VReg_HiveCheckKey(_In_ VRegHive* const pHive, _In_ VReg_Node* const pNode)
{
    const BOOL isLocked = VReg_HiveLockRead(pHive);
    const LSTATUS status = pNode->isDeleted ? ERROR_KEY_DELETED : ERROR_SUCCESS;
    VReg_HiveUnlockRead(pHive, isLocked);
    return status;
}

VREG_DECL LSTATUS VReg_HiveWatch(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
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

    isLocked = VReg_HiveLockWrite(pHive);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (isLocked)
    {
        VReg_ChangeWatch watch;
        watch.hEvent = hOwn;
        watch.isSubtree = bWatchSubtree != FALSE;
        watch.dwFilter = dwFilter;
        if (VReg_ArrayAppend(&pNode->watches, sizeof(watch), &watch))
            hOwn = NULL;
        else
            status = ERROR_OUTOFMEMORY;
    }

    if (isLocked)
        VReg_HiveUnlockWrite(pHive);
    if (hOwn)
        CloseHandle(hOwn);
    return status;
}

VREG_DECL LSTATUS VReg_HiveOpenLowerValueKey(
    _In_ VRegHive* const pHive,
    _In_ VReg_Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Out_ PHKEY const phLower
)
{
    const VReg_WStringView name = VReg_ViewOf(pszName);
    BOOL isLocked;
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg_WString viewPath = {0, 0, NULL};
    HKEY hLower = NULL;
    LSTATUS status = ERROR_SUCCESS;

    *phLower = NULL;
    isLocked = VReg_HiveLockRead(pHive);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (VReg_HiveFindValue(pNode, name))
        isFound = TRUE;
    else
        status = VReg_HiveViewOf(pHive, pNode, &isVisible, &viewPath);
    VReg_HiveUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS || isFound || !isVisible)
        goto done;

    status = VReg_HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
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
    if (!VReg_StatusExists(status))
        goto done;

    *phLower = hLower;
    hLower = NULL;
    status = ERROR_SUCCESS;

done:
    VReg_HiveCloseLower(pHive, hLower);
    VReg_WStringFree(&viewPath);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegSetHiveMuiTextValue(
    _In_ VRegHive* const pHive,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_opt_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
)
{
    VReg_Array components = {NULL, 0, 0};
    VReg_OpenResult opened;
    DWORD dwDisposition = 0;
    LSTATUS status = VReg_SplitPath(VReg_ViewOf(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VReg_ArrayFree(&components);
        return status;
    }

    status = VReg_HiveCreateKey(
        pHive,
        pHive->pRoot,
        (const VReg_WStringView*)components.p,
        components.count,
        L"",
        0,
        KEY_SET_VALUE,
        &opened,
        &dwDisposition
    );
    VReg_ArrayFree(&components);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen: the open answered with a lower key, which cannot take the value.
    if (!opened.pNode)
    {
        VReg_HiveCloseLower(pHive, opened.hLower);
        return ERROR_ACCESS_DENIED;
    }

    return VReg_HiveSetMuiTextValue(pHive, opened.pNode, pszName ? pszName : L"", pszText);
}

// ==========================================================================
// The native registry
// ==========================================================================

VREG_DECL void WINAPI VRegResolveNativeFunctions(_Out_ VRegNativeFunctions* const pFunctions)
{
    ZeroMemory(pFunctions, sizeof(*pFunctions));

    // A resolved export is a plain code address, which only a data pointer
    // cast can carry into the typed slot.
#define VREG_RESOLVE(name) *(void**)&pFunctions->name = VReg_FindRegistryExport(#name);
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_RESOLVE)
#undef VREG_RESOLVE
}

VREG_DECL LSTATUS WINAPI VRegCreateNative(
    _In_opt_ const VRegNativeFunctions* const pFunctions,
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VReg_AllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->dwKind = VREG_INTERNAL_KIND_NATIVE;
    if (pFunctions)
        pRegistry->implementation.native = *pFunctions;
    else
        VRegResolveNativeFunctions(&pRegistry->implementation.native);

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL _Ret_notnull_ VRegNativeFunctions* WINAPI VRegGetNativeFunctions(_In_ VReg* const pRegistry)
{
    return &pRegistry->implementation.native;
}

// ==========================================================================
// Owned reads
// ==========================================================================

VREG_DECL DWORD VReg_Grown(const DWORD cbOld, const DWORD cbRequired)
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}

VREG_DECL LSTATUS VReg_QueryValueOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR lpValueName,
    _Out_opt_ const LPDWORD lpType,
    _Inout_ VReg_Buffer* const pData
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
        if (!VReg_BufferResize(pData, cbData))
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
            cbData = VReg_Grown(cbData, cbRead);
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

VREG_DECL LSTATUS VReg_GetValueOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR lpSubKey,
    _In_opt_z_ const LPCWSTR lpValue,
    _In_ const DWORD dwFlags,
    _Out_opt_ const LPDWORD pdwType,
    _Inout_ VReg_Buffer* const pData
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
        if (!VReg_BufferResize(pData, cbData))
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
            cbData = VReg_Grown(cbData, cbRead);
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

VREG_DECL LSTATUS VReg_EnumKeyOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Inout_ VReg_WString* const pName
)
{
    DWORD cchName = VREG_INTERNAL_INITIAL_NAME_CHARS;
    for (;;)
    {
        DWORD cchRead = cchName;
        LSTATUS status;
        if (!VReg_WStringResize(pName, cchName))
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
        if (status == ERROR_MORE_DATA && cchName < VREG_INTERNAL_MAX_CLASS_CHARS)
        {
            cchName = VReg_MinDword(VReg_Grown(cchName, cchRead), VREG_INTERNAL_MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VReg_WStringTruncate(pName, cchRead);
        return status;
    }
}

VREG_DECL LSTATUS VReg_EnumValueOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Inout_ VReg_WString* const pName,
    _Out_opt_ const LPDWORD lpType,
    _Inout_opt_ VReg_Buffer* const pData
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
        if (!VReg_WStringResize(pName, cchName))
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
            cchName = VReg_MinDword(VReg_Grown(cchName, cchRead + 1), VREG_INTERNAL_MAX_VALUE_NAME_CHARS);
            if (pData)
            {
                cbData = VReg_Grown(cbData, cbRead);
                if (isSized && !VReg_BufferResize(pData, cbData))
                    return ERROR_OUTOFMEMORY;
            }
            continue;
        }
        if (status == ERROR_SUCCESS && !isSized)
        {
            cbData = cbRead;
            if (!VReg_BufferResize(pData, cbData))
                return ERROR_OUTOFMEMORY;

            isSized = TRUE;
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            VReg_WStringTruncate(pName, cchRead);
            if (pData)
                pData->cb = cbRead;
            if (lpType)
                *lpType = dwType;
        }
        return status;
    }
}

VREG_DECL LSTATUS VReg_QueryInfoKeyOwned(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _Inout_ VReg_WString* const pClass,
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
        if (!VReg_WStringResize(pClass, cchClass))
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
        if ((status == ERROR_MORE_DATA || status == ERROR_INSUFFICIENT_BUFFER) && cchClass < VREG_INTERNAL_MAX_CLASS_CHARS)
        {
            cchClass = VReg_MinDword(cchClass * 2, VREG_INTERNAL_MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VReg_WStringTruncate(pClass, cchRead);
        else if (lpcchClassReported && cchRead != cchClass)
            *lpcchClassReported = cchRead;
        return status;
    }
}

// ==========================================================================
// Paths
// ==========================================================================

VREG_DECL LSTATUS VReg_SplitPath(
    const VReg_WStringView path,
    _Inout_ VReg_Array* const pComponents
)
{
    SIZE_T start = 0;
    if (path.cch != 0 && path.pch[0] == L'\\')
        return ERROR_BAD_PATHNAME;

    while (start < path.cch)
    {
        SIZE_T end = start;
        VReg_WStringView component;
        while (end < path.cch && path.pch[end] != L'\\')
            ++end;

        component.pch = path.pch + start;
        component.cch = end - start;
        if (component.cch != 0)
        {
            if (component.cch > VREG_INTERNAL_MAX_KEY_NAME_CHARS)
                return ERROR_INVALID_PARAMETER;
            if (!VReg_ArrayAppend(pComponents, sizeof(component), &component))
                return ERROR_OUTOFMEMORY;
        }

        start = end + 1;
    }

    return ERROR_SUCCESS;
}

// ==========================================================================
// The flat functions
// ==========================================================================

VREG_DECL LSTATUS WINAPI VRegOpenKeyExW(
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
        return VReg_VirtualRegOpenKeyExW(
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

VREG_DECL LSTATUS WINAPI VRegCreateKeyExW(
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
        return VReg_VirtualRegCreateKeyExW(
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

VREG_DECL LSTATUS WINAPI VRegCloseKey(
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
        return VReg_VirtualRegCloseKey(
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

VREG_DECL LSTATUS WINAPI VRegQueryValueExW(
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
        return VReg_VirtualRegQueryValueExW(
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

VREG_DECL LSTATUS WINAPI VRegSetValueExW(
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
        return VReg_VirtualRegSetValueExW(
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

VREG_DECL LSTATUS WINAPI VRegDeleteValueW(
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
        return VReg_VirtualRegDeleteValueW(
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

VREG_DECL LSTATUS WINAPI VRegDeleteKeyExW(
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
        return VReg_VirtualRegDeleteKeyExW(
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

VREG_DECL LSTATUS WINAPI VRegDeleteTreeW(
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
        return VReg_VirtualRegDeleteTreeW(
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

VREG_DECL LSTATUS WINAPI VRegEnumKeyExW(
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
        return VReg_VirtualRegEnumKeyExW(
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

VREG_DECL LSTATUS WINAPI VRegEnumValueW(
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
        return VReg_VirtualRegEnumValueW(
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

VREG_DECL LSTATUS WINAPI VRegQueryInfoKeyW(
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
        return VReg_VirtualRegQueryInfoKeyW(
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

VREG_DECL LSTATUS WINAPI VRegLoadMUIStringW(
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
        return VReg_VirtualRegLoadMUIStringW(
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
VREG_DECL LSTATUS WINAPI VRegGetValueW(
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
        return VReg_VirtualRegGetValueW(
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

VREG_DECL LSTATUS WINAPI VRegNotifyChangeKeyValue(
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
        return VReg_VirtualRegNotifyChangeKeyValue(
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

VREG_DECL LSTATUS WINAPI VRegFlushKey(
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
        return VReg_VirtualRegFlushKey(
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

VREG_DECL void WINAPI VRegDestroy(
    _In_opt_ VReg* const pRegistry
)
{
    if (!pRegistry)
        return;

    switch (pRegistry->dwKind)
    {
    case VREG_INTERNAL_KIND_NATIVE:
        VReg_Free(pRegistry);
        break;

    case VREG_INTERNAL_KIND_VIRTUAL:
        VReg_VirtualDestroy(pRegistry);
        break;

    default:
        if (pRegistry->implementation.custom.lpVtbl->Destroy)
            pRegistry->implementation.custom.lpVtbl->Destroy(pRegistry);
        VReg_Free(pRegistry);
        break;
    }
}

VREG_DECL LSTATUS WINAPI VRegCreateCustom(
    _In_ const VRegVtbl* const pVtbl,
    _In_opt_ void* const pDelegate,
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VReg_AllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->dwKind = VREG_INTERNAL_KIND_CUSTOM;
    pRegistry->implementation.custom.lpVtbl = pVtbl;
    pRegistry->implementation.custom.pDelegate = pDelegate;

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL _Ret_maybenull_ void* WINAPI VRegGetDelegate(_In_ VReg* const pRegistry)
{
    return pRegistry->dwKind == VREG_INTERNAL_KIND_CUSTOM ? pRegistry->implementation.custom.pDelegate : NULL;
}

// ==========================================================================
// Status
// ==========================================================================

_Post_satisfies_(return != ERROR_SUCCESS)
VREG_DECL LSTATUS VReg_FailureStatus(const LSTATUS status)
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    return status;
}

// ==========================================================================
// Value types
// ==========================================================================

VREG_DECL BOOL VReg_IsStringType(const DWORD dwType)
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

    /// `VReg_WString` components.
    VReg_Array path;

    VRegHive* pHive;

    /// @ref KEY_WOW64_64KEY, @ref KEY_WOW64_32KEY or 0 for both views.
    REGSAM samView;

    /// The native key path as `VReg_WString` components without
    /// `WOW6432Node`; empty when the mount has none.
    VReg_Array nativePath;
} VReg_MountPoint;

/// A mount table is replaced, never changed: a new one copies the mount points
/// of the old (sharing their strings, which only the newest table frees) and
/// adds one. Old tables stay reachable through @ref pPrevious until the
/// registry is destroyed, since a call in flight may still read them.
typedef struct VReg_MountTable VReg_MountTable;

struct VReg_MountTable
{
    /// `VReg_MountPoint`.
    VReg_Array mounts;

    VReg_MountTable* pPrevious;
};

/// Where a key handle plus a subkey path lands.
typedef struct
{
    /// The node, when the key is inside a mount.
    VReg_Node* pNode;

    /// The hive of @ref pNode.
    VRegHive* pHive;

    /// The components below @ref pNode still to walk.
    const VReg_WStringView* pRest;
    SIZE_T cRest;

    /// Whether the call goes to the base registry, because the key is
    /// outside every mount.
    BOOL isBase;

    LSTATUS status;

    /// `VReg_WStringView` into the caller's subkey.
    VReg_Array components;

    /// `VReg_WStringView` into @ref nativeText and @ref components: the
    /// native path of a real key plus the subkey.
    VReg_Array full;
    VReg_WString nativeText;
} VReg_Target;

#define VREG_INTERNAL_MOUNT_AT(pArray, index) VREG_INTERNAL_AT(pArray, VReg_MountPoint, index)
#define VREG_INTERNAL_STRING_AT(pArray, index) VREG_INTERNAL_AT(pArray, VReg_WString, index)
#define VREG_INTERNAL_VIEW_AT(pArray, index) VREG_INTERNAL_AT(pArray, VReg_WStringView, index)

/// The view of the process: what a call without a view flag asks for.
#define VREG_INTERNAL_PROCESS_VIEW (sizeof(void*) == 8 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY)

#define VREG_INTERNAL_KEY_NAME_INFORMATION_CLASS ((ULONG)3)
#define VREG_INTERNAL_NT_STATUS_BUFFER_OVERFLOW ((LONG)0x80000005)
#define VREG_INTERNAL_NT_STATUS_BUFFER_TOO_SMALL ((LONG)0xC0000023)

#define VREG_INTERNAL_KNOWN_NOTIFY_FILTERS \
    (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY \
        | REG_NOTIFY_THREAD_AGNOSTIC)

/// @p psz, or an empty string for null: the hive layer takes no null names.
static LPCWSTR VReg_OrEmpty(_In_opt_z_ const LPCWSTR psz)
{
    return psz ? psz : L"";
}

/// Copies a value into a caller buffer of @p cbBuffer bytes that is known to
/// hold it. Native adds a terminator after a string that lacks one, when it
/// fits, without counting it in the size.
static void VReg_CopyValue(
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
    if (!VReg_IsStringType(dwType))
        return;

    cbWhole = cbValue / sizeof(wchar_t) * sizeof(wchar_t);
    if (cbWhole == 0)
        return;

    isTerminated = pValue[cbWhole - 2] == 0 && pValue[cbWhole - 1] == 0;
    if (!isTerminated && cbWhole + sizeof(wchar_t) <= cbBuffer)
        ZeroMemory(pbBuffer + cbWhole, sizeof(wchar_t));
}

static BOOL VReg_IsWow6432Node(const VReg_WStringView name)
{
    return VReg_CompareNames(name, VReg_ViewOf(L"WOW6432Node")) == 0;
}

/// The view a call with @p samDesired is of: the one it names, else the
/// process's.
static REGSAM VReg_CallView(const REGSAM samDesired)
{
    if (samDesired & KEY_WOW64_64KEY)
        return KEY_WOW64_64KEY;
    if (samDesired & KEY_WOW64_32KEY)
        return KEY_WOW64_32KEY;

    return VREG_INTERNAL_PROCESS_VIEW;
}

/// Whether a mount of view @p samMount answers a call of view @p samCall.
static BOOL VReg_Admits(const REGSAM samMount, const REGSAM samCall)
{
    return samMount == 0 || samMount == samCall;
}

/// The string form of the current user's SID; empty when it cannot be read.
static LSTATUS VReg_CurrentUserSid(_Inout_ VReg_WString* pSid)
{
    HANDLE hToken = NULL;
    DWORD cbUser = 0;
    VReg_Buffer user = {0, 0, NULL};
    LSTATUS status = ERROR_SUCCESS;

    if (!VReg_WStringResize(pSid, 0))
        return ERROR_OUTOFMEMORY;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return ERROR_SUCCESS;

    GetTokenInformation(hToken, TokenUser, NULL, 0, &cbUser);
    if (cbUser != 0)
    {
        if (!VReg_BufferResize(&user, cbUser))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (GetTokenInformation(hToken, TokenUser, user.pb, cbUser, &cbUser))
        {
            LPWSTR pszSid = NULL;
            if (ConvertSidToStringSidW(((const TOKEN_USER*)user.pb)->User.Sid, &pszSid))
            {
                if (!VReg_WStringAssign(pSid, pszSid, VReg_ViewOf(pszSid).cch))
                    status = ERROR_OUTOFMEMORY;
                LocalFree(pszSid);
            }
        }
    }

    VReg_BufferFree(&user);
    CloseHandle(hToken);
    return status;
}

/// Appends the native path of a predefined root to @p pPath as components;
/// nothing for a root with none.
static LSTATUS VReg_NativeRootPath(
    const HKEY hRoot,
    _Inout_ VReg_Array* const pPath
)
{
    const wchar_t* pszSecond = NULL;
    VReg_WString sid = {0, 0, NULL};
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
        status = VReg_CurrentUserSid(&sid);
        if (status == ERROR_SUCCESS && sid.cch != 0)
            pszSecond = L"USER";
    }

    if (status == ERROR_SUCCESS && pszSecond)
    {
        if (
            !VReg_WStringArrayAppend(pPath, VReg_ViewOf(L"REGISTRY"))
            || !VReg_WStringArrayAppend(pPath, VReg_ViewOf(pszSecond))
            || (sid.cch != 0 && !VReg_WStringArrayAppend(pPath, VReg_WStringViewOf(&sid)))
        )
            status = ERROR_OUTOFMEMORY;
    }

    VReg_WStringFree(&sid);
    return status;
}

/// Whether @p pFull, with `WOW6432Node` components skipped, starts with
/// @p pMountPath; @p piRest then indexes the first component past the match
/// and @p pIsWow says a `WOW6432Node` was skipped.
static BOOL VReg_MatchesNative(
    _In_ const VReg_Array* const pMountPath,
    _In_ const VReg_Array* const pFull,
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
        const VReg_WStringView component = VREG_INTERNAL_VIEW_AT(pFull, i);
        if (VReg_IsWow6432Node(component))
        {
            *pIsWow = TRUE;
            continue;
        }
        if (VReg_CompareNames(component, VReg_WStringViewOf(&VREG_INTERNAL_STRING_AT(pMountPath, iMount))) != 0)
            return FALSE;

        ++iMount;
    }

    *piRest = i;
    return iMount == pMountPath->count;
}

// --- The mount table ---

/// The current table, or null while nothing is mounted. Lock-free.
static _Ret_maybenull_ const VReg_MountTable* VReg_MountsOf(_In_ const VReg* const pThis)
{
    return (const VReg_MountTable*)ReadPointerAcquire((PVOID volatile*)&pThis->implementation.virtualRegistry.pMounts);
}

/// A mount of @p pHive here; null when it is not mounted.
static _Ret_maybenull_ const VReg_MountPoint* VReg_MountOf(
    _In_ const VReg* const pThis,
    _In_ const VRegHive* const pHive
)
{
    const VReg_MountTable* const pTable = VReg_MountsOf(pThis);
    SIZE_T i;
    if (!pTable)
        return NULL;

    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VReg_MountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
        if (pMount->pHive == pHive)
            return pMount;
    }

    return NULL;
}

static void VReg_MountPointFree(_Inout_ VReg_MountPoint* const pMount)
{
    VReg_WStringArrayFree(&pMount->path);
    VReg_WStringArrayFree(&pMount->nativePath);
}

/// Whether the mount @p pMount is of @p hRoot at the path @p pPath, by the
/// registry's own name comparison.
static BOOL VReg_MountIsAt(
    _In_ const VReg_MountPoint* const pMount,
    const HKEY hRoot,
    _In_ const VReg_Array* const pPath
)
{
    SIZE_T i;
    if (pMount->hRoot != hRoot || pMount->path.count != pPath->count)
        return FALSE;

    for (i = 0; i < pPath->count; ++i)
    {
        const VReg_WStringView left = VReg_WStringViewOf(&VREG_INTERNAL_STRING_AT(&pMount->path, i));
        const VReg_WStringView right = VReg_WStringViewOf(&VREG_INTERNAL_STRING_AT(pPath, i));
        if (VReg_CompareNames(left, right) != 0)
            return FALSE;
    }

    return TRUE;
}

/// Publishes a table holding the current mounts plus @p pMount, whose strings
/// it then owns. Under the mount lock.
static LSTATUS VReg_PublishMount(_In_ VReg* const pThis, _In_ const VReg_MountPoint* const pMount)
{
    VReg_MountTable* const pOld = pThis->implementation.virtualRegistry.pMounts;
    VReg_MountTable* const pNew = (VReg_MountTable*)VReg_AllocZeroed(sizeof(VReg_MountTable));
    SIZE_T i;
    if (!pNew)
        return ERROR_OUTOFMEMORY;

    for (i = 0; pOld && i < pOld->mounts.count; ++i)
    {
        const VReg_MountPoint* const pExisting = &VREG_INTERNAL_MOUNT_AT(&pOld->mounts, i);
        if (VReg_MountIsAt(pExisting, pMount->hRoot, &pMount->path))
        {
            VReg_ArrayFree(&pNew->mounts);
            VReg_Free(pNew);
            return ERROR_ALREADY_EXISTS;
        }
        if (!VReg_ArrayAppend(&pNew->mounts, sizeof(*pExisting), pExisting))
        {
            VReg_ArrayFree(&pNew->mounts);
            VReg_Free(pNew);
            return ERROR_OUTOFMEMORY;
        }
    }

    if (!VReg_ArrayAppend(&pNew->mounts, sizeof(*pMount), pMount))
    {
        VReg_ArrayFree(&pNew->mounts);
        VReg_Free(pNew);
        return ERROR_OUTOFMEMORY;
    }

    pNew->pPrevious = pOld;
    WritePointerRelease((PVOID volatile*)&pThis->implementation.virtualRegistry.pMounts, pNew);
    return ERROR_SUCCESS;
}

// --- Resolution ---

static void VReg_TargetFree(_Inout_ VReg_Target* const pTarget)
{
    VReg_ArrayFree(&pTarget->components);
    VReg_ArrayFree(&pTarget->full);
    VReg_WStringFree(&pTarget->nativeText);
}

/// Looks for a mount a real key plus the subkey components lands in, by
/// native path; fills @p pTarget and sets @p pIsResolved when it does.
static LSTATUS VReg_ResolveNative(
    _In_ const VReg* const pThis,
    const HKEY hKey,
    const REGSAM samDesired,
    _Inout_ VReg_Target* const pTarget,
    _Out_ BOOL* const pIsResolved
)
{
    const VReg_MountTable* const pTable = VReg_MountsOf(pThis);
    BOOL isGated = FALSE;
    VReg_Buffer buffer = {0, 0, NULL};
    ULONG cbName = 0;
    const VReg_MountPoint* pBest = NULL;
    SIZE_T iBestRest = 0;
    REGSAM samCall;
    SIZE_T i;
    SIZE_T start;
    LSTATUS status = ERROR_SUCCESS;

    *pIsResolved = FALSE;
    for (i = 0; pTable && !isGated && i < pTable->mounts.count; ++i)
    {
        const VReg_MountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
        SIZE_T j;
        if (pMount->path.count == 0)
            continue;

        for (j = 0; !isGated && j < pTarget->components.count; ++j)
        {
            const VReg_WStringView last = VReg_WStringViewOf(&VREG_INTERNAL_STRING_AT(&pMount->path, pMount->path.count - 1));
            isGated = VReg_CompareNames(VREG_INTERNAL_VIEW_AT(&pTarget->components, j), last) == 0;
        }
    }
    if (!isGated || !pThis->implementation.virtualRegistry.pfnNtQueryKey || VRegIsVirtualHKey(hKey))
        return ERROR_SUCCESS;

    // The name is a KEY_NAME_INFORMATION: a byte count, then the characters.
    if (!VReg_BufferResize(&buffer, 1024))
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
        if (!VReg_BufferResize(&buffer, cbResult))
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
        !VReg_WStringAssign(
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
            VReg_WStringView component;
            component.pch = pTarget->nativeText.psz + start;
            component.cch = end - start;
            if (!VReg_ArrayAppend(&pTarget->full, sizeof(component), &component))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
        }

        start = end + 1;
    }

    for (i = 0; i < pTarget->components.count; ++i)
    {
        const VReg_WStringView* const pComponent = &VREG_INTERNAL_VIEW_AT(&pTarget->components, i);
        if (!VReg_ArrayAppend(&pTarget->full, sizeof(*pComponent), pComponent))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    samCall = VReg_CallView(samDesired);
    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VReg_MountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
        BOOL isWow = FALSE;
        SIZE_T iRest = 0;
        if (
            pMount->nativePath.count == 0
            || !VReg_MatchesNative(&pMount->nativePath, &pTarget->full, &isWow, &iRest)
            || !VReg_Admits(pMount->samView, isWow ? KEY_WOW64_32KEY : samCall)
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
        pTarget->pNode = VReg_HiveRoot(pBest->pHive);
        pTarget->pHive = pBest->pHive;
        pTarget->pRest = (const VReg_WStringView*)pTarget->full.p + iBestRest;
        pTarget->cRest = pTarget->full.count - iBestRest;
        *pIsResolved = TRUE;
    }

done:
    VReg_BufferFree(&buffer);
    return status;
}

/// Resolves @p hKey and @p subKey (empty for the key itself). @p samDesired
/// supplies the registry view of the call. Answers a status only for a
/// failure of its own (memory); what the call should answer is in the target.
static LSTATUS VReg_Resolve(
    _In_ const VReg* const pThis,
    const HKEY hKey,
    const VReg_WStringView subKey,
    const REGSAM samDesired,
    _Out_ VReg_Target* const pTarget
)
{
    LSTATUS splitStatus;
    BOOL isSplit;
    BOOL isResolved = FALSE;
    VReg_Node* pNode;

    ZeroMemory(pTarget, sizeof(*pTarget));
    splitStatus = VReg_SplitPath(subKey, &pTarget->components);
    if (splitStatus == ERROR_OUTOFMEMORY)
        return splitStatus;

    isSplit = splitStatus == ERROR_SUCCESS;
    pNode = VReg_NodeOf(hKey);
    if (pNode)
    {
        const VReg_MountPoint* const pMount = VReg_MountOf(pThis, VReg_NodeHive(pNode));
        if (pMount)
        {
            if (isSplit)
            {
                pTarget->pNode = pNode;
                pTarget->pHive = pMount->pHive;
                pTarget->pRest = (const VReg_WStringView*)pTarget->components.p;
                pTarget->cRest = pTarget->components.count;
            }
            else
            {
                pTarget->status = splitStatus;
            }
            return ERROR_SUCCESS;
        }
    }
    else if (VReg_HandleSpaceOwns(VReg_ProcessHandleSpace(), hKey))
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (VRegIsPredefinedHKey(hKey))
    {
        const REGSAM samCall = VReg_CallView(samDesired);
        const VReg_MountTable* const pTable = VReg_MountsOf(pThis);
        const VReg_MountPoint* pBest = NULL;
        SIZE_T i;
        for (i = 0; pTable && isSplit && i < pTable->mounts.count; ++i)
        {
            const VReg_MountPoint* const pMount = &VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i);
            SIZE_T j;
            BOOL isPrefix = TRUE;
            if (
                pMount->hRoot != hKey
                || pMount->path.count > pTarget->components.count
                || !VReg_Admits(pMount->samView, samCall)
            )
                continue;

            for (j = 0; isPrefix && j < pMount->path.count; ++j)
            {
                const VReg_WStringView mountName = VReg_WStringViewOf(&VREG_INTERNAL_STRING_AT(&pMount->path, j));
                isPrefix = VReg_CompareNames(mountName, VREG_INTERNAL_VIEW_AT(&pTarget->components, j)) == 0;
            }
            if (!isPrefix)
                continue;

            if (!pBest || pMount->path.count > pBest->path.count)
                pBest = pMount;
        }

        if (pBest)
        {
            pTarget->pNode = VReg_HiveRoot(pBest->pHive);
            pTarget->pHive = pBest->pHive;
            pTarget->pRest = (const VReg_WStringView*)pTarget->components.p + pBest->path.count;
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
        const LSTATUS status = VReg_ResolveNative(pThis, hKey, samDesired, pTarget, &isResolved);
        if (status != ERROR_SUCCESS)
            return status;
    }

    if (!isResolved)
        pTarget->isBase = TRUE;

    return ERROR_SUCCESS;
}

// --- The slots ---

VREG_DECL LSTATUS WINAPI VReg_VirtualRegOpenKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    VReg_Target target;
    VReg_OpenResult result;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegOpenKeyExW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }
    if (!samDesired)
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VReg_HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, samDesired, &result);
    if (status == ERROR_SUCCESS)
        *phkResult = result.hLower ? result.hLower : VReg_NodeHandle(result.pNode);

done:
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegCreateKeyExW(
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
    VReg_Target target;
    VReg_OpenResult result;
    DWORD dwDisposition = REG_OPENED_EXISTING_KEY;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegCreateKeyExW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    *phkResult = NULL;
    if (!target.pNode)
    {
        status = VReg_FailureStatus(target.status);
        goto done;
    }
    if (dwOptions & ~(DWORD)0x1F)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    status = VReg_HiveCreateKey(
        target.pHive,
        target.pNode,
        target.pRest,
        target.cRest,
        VReg_OrEmpty(lpClass),
        dwOptions,
        samDesired,
        &result,
        &dwDisposition
    );
    if (status != ERROR_SUCCESS)
    {
        status = VReg_FailureStatus(status);
        goto done;
    }

    *phkResult = result.hLower ? result.hLower : VReg_NodeHandle(result.pNode);
    if (lpdwDisposition)
        *lpdwDisposition = dwDisposition;

done:
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegCloseKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    if (VReg_NodeOf(hKey) || VRegIsPredefinedHKey(hKey))
        return ERROR_SUCCESS;
    if (VReg_HandleSpaceOwns(VReg_ProcessHandleSpace(), hKey) || !pRegistry->implementation.virtualRegistry.pBase)
        return ERROR_INVALID_HANDLE;

    return VRegCloseKey(
        pRegistry->implementation.virtualRegistry.pBase,
        hKey
    );
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegQueryValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    VReg_Target target;
    VReg_Buffer value = {0, 0, NULL};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    DWORD cbBuffer;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegQueryValueExW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VReg_HiveQueryValue(
        target.pHive,
        target.pNode,
        VReg_OrEmpty(lpValueName),
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

    VReg_CopyValue(lpData, cbBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
        VReg_BufferFree(&value);
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegSetValueExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    VReg_Target target;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegSetValueExW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }
    if (!lpData && cbData)
    {
        status = ERROR_NOACCESS;
        goto done;
    }

    status = VReg_HiveSetValue(
        target.pHive,
        target.pNode,
        VReg_OrEmpty(lpValueName),
        dwType,
        lpData,
        lpData ? cbData : 0
    );

done:
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegDeleteValueW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    VReg_Target target;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteValueW(
            pRegistry->implementation.virtualRegistry.pBase,
            hKey,
            lpValueName
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    status = VReg_HiveDeleteValue(target.pHive, target.pNode, VReg_OrEmpty(lpValueName));

done:
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegDeleteKeyExW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    VReg_Target target;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteKeyExW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    status = VReg_HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegDeleteTreeW(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    VReg_Target target;
    VReg_OpenResult result;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(lpSubKey), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteTreeW(
            pRegistry->implementation.virtualRegistry.pBase,
            hKey,
            lpSubKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    status = VReg_HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_ALL_ACCESS, &result);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only a frozen hive hands out lower keys, and it refuses every write.
    if (result.hLower)
    {
        VReg* const pLower = VReg_HiveLower(target.pHive);
        // A lower handle exists only where there is a lower registry.
        _Analysis_assume_(pLower);
        VRegCloseKey(
            pLower,
            result.hLower
        );
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VReg_HiveDeleteTree(target.pHive, result.pNode);
    if (status != ERROR_SUCCESS || target.cRest == 0)
        goto done;

    status = VReg_HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegEnumKeyExW(
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
    VReg_Target target;
    VReg_WString name = {0, 0, NULL};
    VReg_WString className = {0, 0, NULL};
    FILETIME lastWrite;
    DWORD cchClassBuffer;
    BOOL isNameFit;
    BOOL isClassFit;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    ZeroMemory(&lastWrite, sizeof(lastWrite));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegEnumKeyExW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    cchClassBuffer = lpClass ? *lpcchClass : 0;
    if (lpcchClass)
        *lpcchClass = 0;

    status = VReg_HiveEnumKey(target.pHive, target.pNode, dwIndex, &name, &className, &lastWrite);
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

    CopyMemory(lpName, VReg_WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
    *lpcchName = (DWORD)name.cch;
    if (lpClass)
    {
        CopyMemory(lpClass, VReg_WStringText(&className), (className.cch + 1) * sizeof(wchar_t));
        *lpcchClass = (DWORD)className.cch;
    }

done:
    if (target.pHive)
    {
        VReg_WStringFree(&name);
        VReg_WStringFree(&className);
    }
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegEnumValueW(
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
    VReg_Target target;
    VReg_WString name = {0, 0, NULL};
    VReg_Buffer value = {0, 0, NULL};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    BOOL isDataFit = TRUE;
    DWORD cbDataBuffer = 0;
    BOOL isNameFit;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegEnumValueW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VReg_HiveEnumValue(
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
        CopyMemory(lpValueName, VReg_WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
        *lpcchValueName = (DWORD)name.cch;
    }

    if (!isNameFit || !isDataFit)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    if (lpData)
        VReg_CopyValue(lpData, cbDataBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
    {
        VReg_WStringFree(&name);
        VReg_BufferFree(&value);
    }
    VReg_TargetFree(&target);
    return status;
}

// Native answers success without writing lpClass when *lpcchClass is zero,
// which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_VirtualRegQueryInfoKeyW(
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
    VReg_Target target;
    VReg_KeyInfo info;
    DWORD cchClass;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    ZeroMemory(&info, sizeof(info));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegQueryInfoKeyW(
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
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    status = VReg_HiveQueryInfo(target.pHive, target.pNode, &info);
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

        CopyMemory(lpClass, VReg_WStringText(&info.className), (cchClass + 1) * sizeof(wchar_t));
    }

    if (lpClass && lpcchClass && (*lpcchClass != 0 || lpcbMaxClassLen))
        *lpcchClass = cchClass;
    else if (!lpClass && lpcchClass)
        *lpcchClass = 0;

done:
    if (target.pHive)
        VReg_WStringFree(&info.className);
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegLoadMUIStringW(
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
    VReg_Target target;
    HKEY hLower = NULL;
    VReg_WString text = {0, 0, NULL};
    DWORD cbText;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegLoadMUIStringW(
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
        status = VReg_FailureStatus(target.status);
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

    status = VReg_HiveOpenLowerValueKey(target.pHive, target.pNode, VReg_OrEmpty(pszValue), &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    // A value from below is resolved there, as the system would. One the hive
    // holds would need a resource module to resolve, unless it is marked as
    // the text itself.
    if (hLower)
    {
        VReg* const pLower = VReg_HiveLower(target.pHive);
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

    status = VReg_HiveQueryMuiText(target.pHive, target.pNode, VReg_OrEmpty(pszValue), &text);
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
        CopyMemory(pszOutBuf, VReg_WStringText(&text), cch * sizeof(wchar_t));
        pszOutBuf[cch] = L'\0';
        goto done;
    }

    *pcbData = cbText;
    if (!pszOutBuf || cbOutBuf < cbText)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    CopyMemory(pszOutBuf, VReg_WStringText(&text), cbText);

done:
    if (target.pHive)
        VReg_WStringFree(&text);
    VReg_TargetFree(&target);
    return status;
}

/// Answers the failure @p status after clearing the caller's buffer when the
/// call asked for that on failure.
_Post_satisfies_(return != ERROR_SUCCESS)
static LSTATUS VReg_FailGetValue(
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
static BOOL VReg_IsNulAt(_In_ const VReg_Buffer* const pBuffer, const SIZE_T iwch)
{
    wchar_t ch;
    CopyMemory(&ch, pBuffer->pb + iwch * sizeof(wchar_t), sizeof(ch));
    return ch == L'\0';
}

// The SDK's _Post_z_ holds on every return, truncated reads and failures
// included; the analyzer cannot see the terminators written at run time.
#pragma warning(suppress: 6054)
VREG_DECL LSTATUS WINAPI VReg_VirtualRegGetValueW(
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

    VReg_Target target;
    VReg_OpenResult result;
    VReg_Node* pNode;
    DWORD cbBuffer;
    DWORD dwType = 0;
    DWORD cbQueried = 0;
    BOOL isString;

    // Native copies the stored bytes first, then the string with the
    // terminators it lacks, then the expansion, each over the previous one and
    // only if it fits.
    VReg_Buffer stages[3] = {{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}};
    DWORD cbReported;
    DWORD dwTypeFlag;
    BOOL isAllowed;
    const VReg_Buffer* pFinal;
    BOOL isFit;
    BOOL isSizeMismatch;
    SIZE_T i;
    LSTATUS status = VReg_Resolve(
        pRegistry,
        hkey,
        VReg_ViewOf(lpSubKey),
        (dwFlags & VREG_INTERNAL_RRF_WOW6464_SUBKEY ? KEY_WOW64_64KEY : 0)
        | (dwFlags & VREG_INTERNAL_RRF_WOW6432_SUBKEY ? KEY_WOW64_32KEY : 0),
        &target
    );
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegGetValueW(
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
        status = VReg_FailGetValue(pvData, cbBuffer, dwFlags, target.status);
        goto done;
    }

    pNode = target.pNode;
    if (target.cRest != 0)
    {
        status = VReg_HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_QUERY_VALUE, &result);
        if (status != ERROR_SUCCESS)
        {
            status = VReg_FailGetValue(pvData, cbBuffer, dwFlags, status);
            goto done;
        }

        if (result.hLower)
        {
            VReg* const pLower = VReg_HiveLower(target.pHive);
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

    status = VReg_HiveQueryValue(target.pHive, pNode, VReg_OrEmpty(lpValue), &dwType, &cbQueried, &stages[0]);
    if (status != ERROR_SUCCESS)
    {
        status = VReg_FailGetValue(pvData, cbBuffer, dwFlags, status);
        goto done;
    }

    isString = VReg_IsStringType(dwType);
    cbReported = (DWORD)stages[0].cb;
    if (isString)
    {
        const SIZE_T cwch = stages[0].cb / sizeof(wchar_t);
        SIZE_T cwchAdded = 0;
        static const wchar_t ZEROS[2] = {L'\0', L'\0'};
        if (cwch == 0 || !VReg_IsNulAt(&stages[0], cwch - 1))
            cwchAdded = dwType == REG_MULTI_SZ ? 2 : 1;
        else if (dwType == REG_MULTI_SZ && (cwch == 1 || !VReg_IsNulAt(&stages[0], cwch - 2)))
            cwchAdded = 1;

        if (
            !VReg_BufferAssign(&stages[1], stages[0].pb, cwch * sizeof(wchar_t))
            || !VReg_BufferAppend(&stages[1], ZEROS, cwchAdded * sizeof(wchar_t))
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
            if (!VReg_BufferResize(&stages[2], (SIZE_T)cwchExpanded * sizeof(wchar_t)))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
            if (cwchExpanded != 0)
                ExpandEnvironmentStringsW(pszRaw, (wchar_t*)stages[2].pb, cwchExpanded);

            cbReported = VReg_MaxDword(cbReported, (DWORD)stages[2].cb);
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
            *pcbData = isFit ? cbReported : VReg_MaxDword(cbReported, cbReserved);
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
        status = VReg_FailGetValue(pvData, cbBuffer, dwFlags, status);

done:
    for (i = 0; i < 3; ++i)
        VReg_BufferFree(&stages[i]);
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegNotifyChangeKeyValue(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    VReg_Target target;
    HANDLE hOwn;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegNotifyChangeKeyValue(
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
        status = VReg_FailureStatus(target.status);
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
        status = VReg_HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hEvent);
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

    status = VReg_HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hOwn);
    if (status == ERROR_SUCCESS)
        WaitForSingleObject(hOwn, INFINITE);

    CloseHandle(hOwn);

done:
    VReg_TargetFree(&target);
    return status;
}

VREG_DECL LSTATUS WINAPI VReg_VirtualRegFlushKey(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hKey
)
{
    VReg_Target target;
    LSTATUS status = VReg_Resolve(pRegistry, hKey, VReg_ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegFlushKey(
            pRegistry->implementation.virtualRegistry.pBase,
            hKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg_FailureStatus(target.status);
        goto done;
    }

    status = VReg_HiveCheckKey(target.pHive, target.pNode);

done:
    VReg_TargetFree(&target);
    return status;
}

// --- Creation and destruction ---

VREG_DECL void WINAPI VReg_VirtualDestroy(_In_ VReg* const pRegistry)
{
    VReg_MountTable* pTable = pRegistry->implementation.virtualRegistry.pMounts;
    BOOL isNewest = TRUE;
    while (pTable)
    {
        VReg_MountTable* const pPrevious = pTable->pPrevious;
        if (isNewest)
        {
            SIZE_T i;
            for (i = 0; i < pTable->mounts.count; ++i)
                VReg_MountPointFree(&VREG_INTERNAL_MOUNT_AT(&pTable->mounts, i));
        }

        VReg_ArrayFree(&pTable->mounts);
        VReg_Free(pTable);
        pTable = pPrevious;
        isNewest = FALSE;
    }

    VReg_Free(pRegistry);
}

VREG_DECL LSTATUS WINAPI VRegCreateVirtual(
    _In_opt_ VReg* const pBase,
    _In_opt_ const VRegNtQueryKeyFunction pfnNtQueryKey,
    _Outptr_ VReg** const ppRegistry
)
{
    VReg* const pRegistry = (VReg*)VReg_AllocZeroed(sizeof(VReg));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->dwKind = VREG_INTERNAL_KIND_VIRTUAL;
    pRegistry->implementation.virtualRegistry.pBase = pBase;
    pRegistry->implementation.virtualRegistry.pfnNtQueryKey = pfnNtQueryKey;
    InitializeSRWLock(&pRegistry->implementation.virtualRegistry.mountLock);
    if (!pRegistry->implementation.virtualRegistry.pfnNtQueryKey)
        *(void**)&pRegistry->implementation.virtualRegistry.pfnNtQueryKey =
            VReg_FindExport(GetModuleHandleW(L"ntdll.dll"), "NtQueryKey");

    *ppRegistry = pRegistry;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegMount(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_ VRegHive* const pHive,
    _In_ const REGSAM samView
)
{
    VReg_Array components = {NULL, 0, 0};
    VReg_MountPoint mount;
    SIZE_T i;
    LSTATUS status;

    if (pRegistry->dwKind != VREG_INTERNAL_KIND_VIRTUAL || !VRegIsPredefinedHKey(hRoot))
        return ERROR_INVALID_HANDLE;
    if (samView != 0 && samView != KEY_WOW64_64KEY && samView != KEY_WOW64_32KEY)
        return ERROR_INVALID_PARAMETER;

    ZeroMemory(&mount, sizeof(mount));
    mount.hRoot = hRoot;
    mount.pHive = pHive;
    mount.samView = samView;

    status = VReg_SplitPath(VReg_ViewOf(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VReg_ArrayFree(&components);
        return status == ERROR_OUTOFMEMORY ? status : ERROR_BAD_PATHNAME;
    }

    for (i = 0; status == ERROR_SUCCESS && i < components.count; ++i)
    {
        if (!VReg_WStringArrayAppend(&mount.path, VREG_INTERNAL_VIEW_AT(&components, i)))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VReg_NativeRootPath(hRoot, &mount.nativePath);
    for (i = 0; status == ERROR_SUCCESS && mount.nativePath.count != 0 && i < components.count; ++i)
    {
        const VReg_WStringView component = VREG_INTERNAL_VIEW_AT(&components, i);
        if (!VReg_IsWow6432Node(component) && !VReg_WStringArrayAppend(&mount.nativePath, component))
            status = ERROR_OUTOFMEMORY;
    }
    VReg_ArrayFree(&components);

    if (status == ERROR_SUCCESS)
    {
        AcquireSRWLockExclusive(&pRegistry->implementation.virtualRegistry.mountLock);
        status = VReg_PublishMount(pRegistry, &mount);
        ReleaseSRWLockExclusive(&pRegistry->implementation.virtualRegistry.mountLock);
    }

    if (status != ERROR_SUCCESS)
        VReg_MountPointFree(&mount);
    return status;
}

// ==========================================================================
// Applying scripts
// ==========================================================================

static LSTATUS VReg_NotFoundIsFine(const LSTATUS status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND ? ERROR_SUCCESS : status;
}

static void VReg_ApplyCloseKey(_In_ VReg* const pRegistry, _In_opt_ const HKEY hKey)
{
    if (hKey)
        VRegCloseKey(
            pRegistry,
            hKey
        );
}

static LSTATUS VReg_PerformSetValue(_In_ const VRegOperation* const pOperation, _In_ VReg* const pRegistry)
{
    const BYTE* const pData = pOperation->pData;
    const DWORD cbData = pOperation->cbData;
    const BOOL isString = VReg_IsStringType(pOperation->dwType);
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
        VReg_Buffer padded = {0, 0, NULL};
        if (
            !VReg_BufferAssign(&padded, pData, cbData)
            || !VReg_BufferAppend(&padded, PADDING, sizeof(PADDING))
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

        VReg_BufferFree(&padded);
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

    VReg_ApplyCloseKey(pRegistry, hKey);
    return status;
}

static LSTATUS VReg_Perform(_In_ const VRegOperation* const pOperation, _In_ VReg* const pRegistry)
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
        VReg_ApplyCloseKey(pRegistry, hKey);
        return status;

    case VREG_OPERATION_DELETE_KEY:
        status = VReg_NotFoundIsFine(
            VRegDeleteTreeW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath
            )
        );
        if (status != ERROR_SUCCESS)
            return status;

        return VReg_NotFoundIsFine(
            VRegDeleteKeyExW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath,
                0,
                0
            )
        );

    case VREG_OPERATION_SET_VALUE:
        return VReg_PerformSetValue(pOperation, pRegistry);

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
            return VReg_NotFoundIsFine(status);

        status = VReg_NotFoundIsFine(
            VRegDeleteValueW(
                pRegistry,
                hKey,
                pOperation->pszName
            )
        );
        VReg_ApplyCloseKey(pRegistry, hKey);
        return status;

    default:
        return ERROR_INVALID_PARAMETER;
    }
}

_Success_(return == ERROR_SUCCESS)
VREG_DECL LSTATUS WINAPI VRegApplyOperations(
    _In_reads_(cOperations) const VRegOperation* const pOperations,
    _In_ const SIZE_T cOperations,
    _In_ VReg* const pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* const pFailedIndex
)
{
    SIZE_T i;
    for (i = 0; i < cOperations; ++i)
    {
        const LSTATUS status = VReg_Perform(&pOperations[i], pRegistry);
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
VREG_DECL LSTATUS WINAPI VRegApplyScript(
    _In_ const VRegScript* const pScript,
    _In_ VReg* const pRegistry,
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
typedef struct VReg_ValueEntry
{
    VReg_WString name;
    DWORD dwType;
    VReg_Buffer data;
    BOOL isTombstone;
} VReg_ValueEntry;

/// Inserts @p pEntry into @p pValues, an array of @ref VReg_ValueEntry sorted
/// by name; the array owns the entry from then on.
static BOOL VReg_InsertValueSorted(
    _Inout_ VReg_Array* const pValues,
    _In_ const VReg_ValueEntry* const pEntry
)
{
    const VReg_WStringView name = VReg_WStringViewOf(&pEntry->name);
    SIZE_T lo = 0;
    SIZE_T hi = pValues->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const VReg_ValueEntry* const pMid = &VREG_INTERNAL_AT(pValues, VReg_ValueEntry, mid);
        if (VReg_CompareNames(VReg_WStringViewOf(&pMid->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return VReg_ArrayInsert(pValues, sizeof(*pEntry), lo, pEntry);
}

static void VReg_FreeValues(_Inout_ VReg_Array* const pValues)
{
    SIZE_T i;
    for (i = 0; i < pValues->count; ++i)
    {
        VReg_ValueEntry* const pEntry = &VREG_INTERNAL_AT(pValues, VReg_ValueEntry, i);
        VReg_WStringFree(&pEntry->name);
        VReg_BufferFree(&pEntry->data);
    }

    pValues->count = 0;
}

/// Appends the sorted values of a key as operations and empties the array.
static LSTATUS VReg_FlushValues(
    _Inout_ VRegScript* const pScript,
    _In_ const HKEY hRoot,
    const VReg_WStringView path,
    _Inout_ VReg_Array* const pValues
)
{
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;
    for (i = 0; status == ERROR_SUCCESS && i < pValues->count; ++i)
    {
        const VReg_ValueEntry* const pEntry = &VREG_INTERNAL_AT(pValues, VReg_ValueEntry, i);
        status = VReg_ScriptAppend(
            pScript,
            pEntry->isTombstone ? VREG_OPERATION_DELETE_VALUE : VREG_OPERATION_SET_VALUE,
            hRoot,
            path,
            VReg_WStringViewOf(&pEntry->name),
            pEntry->dwType,
            pEntry->data.pb,
            (DWORD)pEntry->data.cb,
            0
        );
    }

    VReg_FreeValues(pValues);
    return status;
}

/// Appends @p path to @p pPrefix with a backslash between, into @p pFull.
static BOOL VReg_JoinKeyPath(
    _Inout_ VReg_WString* const pFull,
    const VReg_WStringView prefix,
    const VReg_WStringView path
)
{
    if (!VReg_WStringAssign(pFull, prefix.pch, prefix.cch))
        return FALSE;
    if (path.cch == 0)
        return TRUE;

    return VReg_AppendPathComponent(pFull, path.pch, path.cch);
}

// --- Snapshot ---

/// Appends the key @p hKey at @p pPath below the root, then its values, then
/// its subkeys in order.
static LSTATUS VReg_WalkKey(
    _Inout_ VRegScript* const pScript,
    _In_ VReg* const pRegistry,
    _In_ const HKEY hRoot,
    _In_ const HKEY hKey,
    _In_ const VReg_WString* const pPath,
    const unsigned depth
)
{
    VReg_Array values = {NULL, 0, 0};
    VReg_Array subkeys = {NULL, 0, 0};
    VReg_WString name = {0, 0, NULL};
    VReg_WString childPath = {0, 0, NULL};
    DWORD dwIndex;
    SIZE_T i;
    LSTATUS status;

    // The registry nests at most 512 levels; anything deeper is a cycle
    // through some other registry.
    if (depth > 512)
        return ERROR_CANTREAD;

    status = VReg_ScriptAppend(
        pScript,
        VREG_OPERATION_CREATE_KEY,
        hRoot,
        VReg_WStringViewOf(pPath),
        VReg_ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        VReg_ValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        status = VReg_EnumValueOwned(pRegistry, hKey, dwIndex, &entry.name, &entry.dwType, &entry.data);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            VReg_WStringFree(&entry.name);
            VReg_BufferFree(&entry.data);
            status = ERROR_SUCCESS;
            break;
        }
        if (status == ERROR_SUCCESS && !VReg_InsertValueSorted(&values, &entry))
            status = ERROR_OUTOFMEMORY;
        if (status != ERROR_SUCCESS)
        {
            VReg_WStringFree(&entry.name);
            VReg_BufferFree(&entry.data);
            goto done;
        }
    }

    status = VReg_FlushValues(pScript, hRoot, VReg_WStringViewOf(pPath), &values);
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        status = VReg_EnumKeyOwned(pRegistry, hKey, dwIndex, &name);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VReg_WStringArrayInsertSorted(&subkeys, VReg_WStringViewOf(&name)))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    for (i = 0; i < subkeys.count; ++i)
    {
        const VReg_WString* const pSubkey = &VREG_INTERNAL_AT(&subkeys, VReg_WString, i);
        HKEY hSubkey = NULL;

        // A symbolic link is exported as the key it is (its SymbolicLinkValue),
        // never followed: following one can leave the subtree or loop.
        status = VRegOpenKeyExW(
            pRegistry,
            hKey,
            VReg_WStringText(pSubkey),
            REG_OPTION_OPEN_LINK,
            KEY_READ,
            &hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VReg_JoinKeyPath(&childPath, VReg_WStringViewOf(pPath), VReg_WStringViewOf(pSubkey)))
            status = ERROR_OUTOFMEMORY;
        else
            status = VReg_WalkKey(pScript, pRegistry, hRoot, hSubkey, &childPath, depth + 1);

        VRegCloseKey(
            pRegistry,
            hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;
    }

done:
    VReg_FreeValues(&values);
    VReg_ArrayFree(&values);
    VReg_WStringArrayFree(&subkeys);
    VReg_WStringFree(&name);
    VReg_WStringFree(&childPath);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegSnapshotScript(
    _In_ VReg* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _Outptr_ VRegScript** const ppScript
)
{
    VRegScript* pScript = NULL;
    VReg_WString path = {0, 0, NULL};
    HKEY hKey = NULL;
    LSTATUS status;

    *ppScript = NULL;
    if (!VReg_RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    status = VReg_ScriptCreate(&pScript);
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

    if (!VReg_WStringAssign(&path, pszPath, VReg_ViewOf(pszPath).cch))
        status = ERROR_OUTOFMEMORY;
    else
        status = VReg_WalkKey(pScript, pRegistry, hRoot, hKey, &path, 0);

    VRegCloseKey(
        pRegistry,
        hKey
    );

done:
    VReg_WStringFree(&path);
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
typedef struct VReg_DeltaWalk
{
    VRegScript* pScript;
    HKEY hRoot;
    VReg_WStringView prefix;

    /// The full path of the key the pending values belong to.
    VReg_WString keyPath;
    VReg_Array values;
} VReg_DeltaWalk;

static LSTATUS VReg_VisitUpper(_In_ void* const pContext, _In_ const VReg_UpperRecord* const pRecord)
{
    VReg_DeltaWalk* const pWalk = (VReg_DeltaWalk*)pContext;
    LSTATUS status;

    if (pRecord->kind == VREG_INTERNAL_UPPER_VALUE)
    {
        VReg_ValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        entry.dwType = pRecord->dwType;
        entry.isTombstone = pRecord->isTombstone;
        if (
            !VReg_WStringAssign(&entry.name, pRecord->name.pch, pRecord->name.cch)
            || !VReg_BufferAssign(&entry.data, pRecord->pData, pRecord->cbData)
            || !VReg_InsertValueSorted(&pWalk->values, &entry)
        )
        {
            VReg_WStringFree(&entry.name);
            VReg_BufferFree(&entry.data);
            return ERROR_OUTOFMEMORY;
        }

        return ERROR_SUCCESS;
    }

    status = VReg_FlushValues(pWalk->pScript, pWalk->hRoot, VReg_WStringViewOf(&pWalk->keyPath), &pWalk->values);
    if (status != ERROR_SUCCESS)
        return status;
    if (!VReg_JoinKeyPath(&pWalk->keyPath, pWalk->prefix, pRecord->path))
        return ERROR_OUTOFMEMORY;

    if (pRecord->isTombstone || pRecord->isOpaque)
    {
        status = VReg_ScriptAppend(
            pWalk->pScript,
            VREG_OPERATION_DELETE_KEY,
            pWalk->hRoot,
            VReg_WStringViewOf(&pWalk->keyPath),
            VReg_ViewOf(L""),
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

    return VReg_ScriptAppend(
        pWalk->pScript,
        VREG_OPERATION_CREATE_KEY,
        pWalk->hRoot,
        VReg_WStringViewOf(&pWalk->keyPath),
        VReg_ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
}

VREG_DECL LSTATUS WINAPI VRegDeltaScript(
    _In_ VRegHive* const pHive,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _Outptr_ VRegScript** const ppScript
)
{
    VReg_DeltaWalk walk;
    LSTATUS status;

    *ppScript = NULL;
    if (!VReg_RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    ZeroMemory(&walk, sizeof(walk));
    walk.hRoot = hRoot;
    walk.prefix = VReg_ViewOf(pszPath);
    status = VReg_ScriptCreate(&walk.pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg_HiveSnapshotUpper(pHive, &VReg_VisitUpper, &walk);
    if (status == ERROR_SUCCESS)
        status = VReg_FlushValues(walk.pScript, hRoot, VReg_WStringViewOf(&walk.keyPath), &walk.values);

    VReg_FreeValues(&walk.values);
    VReg_ArrayFree(&walk.values);
    VReg_WStringFree(&walk.keyPath);
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
typedef struct VReg_LineSegment
{
    SIZE_T start;
    SIZE_T line;
    SIZE_T column;
} VReg_LineSegment;

typedef enum VReg_KeyState
{
    VREG_INTERNAL_KEY_NONE,
    VREG_INTERNAL_KEY_OPEN,
    VREG_INTERNAL_KEY_DELETED,
} VReg_KeyState;

typedef struct VReg_Parser
{
    VRegScript* pScript;

    /// The logical line being parsed: a line of the file with its
    /// continuations joined, and the position in it.
    VReg_WString text;
    SIZE_T pos;

    /// `VReg_LineSegment`: offsets into @ref text map back to the physical
    /// lines the text came from.
    VReg_Array segments;

    /// The physical line the logical line starts on, 1-based.
    SIZE_T line;

    VReg_KeyState state;
    HKEY hRoot;
    VReg_WString path;

    /// Set by @ref VReg_ParseFail: where in @ref text, and why.
    const char* pszError;
    SIZE_T errorOffset;
} VReg_Parser;

/// Records a syntax error at the parser's position and answers its status.
static LSTATUS VReg_ParseFail(_Inout_ VReg_Parser* const pParser, _In_z_ const char* const pszMessage)
{
    pParser->pszError = pszMessage;
    pParser->errorOffset = pParser->pos;
    return ERROR_INVALID_DATA;
}

static BOOL VReg_IsBlank(const wchar_t ch)
{
    return ch == L' ' || ch == L'\t';
}

static int VReg_HexDigit(const wchar_t ch)
{
    if (ch >= L'0' && ch <= L'9')
        return ch - L'0';
    if (ch >= L'a' && ch <= L'f')
        return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F')
        return ch - L'A' + 10;
    return -1;
}

static wchar_t VReg_LowerAscii(const wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch - L'A' + L'a') : ch;
}

static BOOL VReg_ViewEquals(const VReg_WStringView left, const VReg_WStringView right)
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

static BOOL VReg_EqualsIgnoreAsciiCase(const VReg_WStringView left, const VReg_WStringView right)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (VReg_LowerAscii(left.pch[i]) != VReg_LowerAscii(right.pch[i]))
            return FALSE;
    }

    return TRUE;
}

static VReg_WStringView VReg_TrimEnd(VReg_WStringView text)
{
    while (text.cch != 0 && VReg_IsBlank(text.pch[text.cch - 1]))
        --text.cch;

    return text;
}

static VReg_WStringView VReg_Substring(const VReg_WStringView text, const SIZE_T start, const SIZE_T cch)
{
    VReg_WStringView result;
    result.pch = text.pch + start;
    result.cch = cch;
    return result;
}

/// The index of @p ch in @p text from @p start on, or the text's length.
static SIZE_T VReg_FindChar(const VReg_WStringView text, const SIZE_T start, const wchar_t ch)
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
static void VReg_ParserPosition(
    _In_ const VReg_Parser* const pParser,
    const SIZE_T offset,
    _Out_ SIZE_T* const pLine,
    _Out_ SIZE_T* const pColumn
)
{
    const VReg_LineSegment* pSegment = &VREG_INTERNAL_AT(&pParser->segments, VReg_LineSegment, 0);
    SIZE_T i;
    for (i = 0; i < pParser->segments.count; ++i)
    {
        const VReg_LineSegment* const pCandidate = &VREG_INTERNAL_AT(&pParser->segments, VReg_LineSegment, i);
        if (pCandidate->start > offset)
            break;

        pSegment = pCandidate;
    }

    *pLine = pSegment->line;
    *pColumn = pSegment->column + (offset - pSegment->start);
}

static BOOL VReg_AtEnd(_In_ const VReg_Parser* const pParser)
{
    return pParser->pos >= pParser->text.cch;
}

static wchar_t VReg_Current(_In_ const VReg_Parser* const pParser)
{
    return pParser->text.psz[pParser->pos];
}

static void VReg_SkipBlanks(_Inout_ VReg_Parser* const pParser)
{
    while (!VReg_AtEnd(pParser) && VReg_IsBlank(VReg_Current(pParser)))
        ++pParser->pos;
}

static LSTATUS VReg_ExpectEnd(_Inout_ VReg_Parser* const pParser)
{
    VReg_SkipBlanks(pParser);
    if (!VReg_AtEnd(pParser))
        return VReg_ParseFail(pParser, "unexpected text at the end of the line");

    return ERROR_SUCCESS;
}

/// Consumes @p pszToken, ignoring case, if the text continues with it.
static BOOL VReg_Consume(_Inout_ VReg_Parser* const pParser, _In_z_ const wchar_t* const pszToken)
{
    const VReg_WStringView token = VReg_ViewOf(pszToken);
    const VReg_WStringView text = VReg_WStringViewOf(&pParser->text);
    if (text.cch - pParser->pos < token.cch)
        return FALSE;
    if (!VReg_EqualsIgnoreAsciiCase(VReg_Substring(text, pParser->pos, token.cch), token))
        return FALSE;

    pParser->pos += token.cch;
    return TRUE;
}

/// Reads the logical line starting at physical line @p index of @p pLines
/// (empty for blank and comment lines) and answers the index of its last
/// physical line in @p pLast.
static LSTATUS VReg_ReadLogicalLine(
    _Inout_ VReg_Parser* const pParser,
    _In_ const VReg_Array* const pLines,
    const SIZE_T index,
    _Out_ SIZE_T* const pLast
)
{
    VReg_WStringView physical = VREG_INTERNAL_AT(pLines, VReg_WStringView, index);
    VReg_LineSegment segment;
    SIZE_T current = index;

    pParser->segments.count = 0;
    if (!VReg_WStringResize(&pParser->text, 0))
        return ERROR_OUTOFMEMORY;

    segment.start = 0;
    segment.line = index + 1;
    segment.column = 1;
    while (physical.cch != 0 && VReg_IsBlank(physical.pch[0]))
    {
        ++physical.pch;
        --physical.cch;
        ++segment.column;
    }
    if (!VReg_ArrayAppend(&pParser->segments, sizeof(segment), &segment))
        return ERROR_OUTOFMEMORY;

    *pLast = index;
    if (physical.cch == 0 || physical.pch[0] == L';')
        return ERROR_SUCCESS;

    if (!VReg_WStringAssign(&pParser->text, physical.pch, physical.cch))
        return ERROR_OUTOFMEMORY;
    if (physical.pch[0] == L'[')
        return ERROR_SUCCESS;

    // A value that continues ends its line in a backslash; a closing quote
    // ends every complete one.
    while (current + 1 < pLines->count)
    {
        VReg_WStringView joined = VReg_TrimEnd(VReg_WStringViewOf(&pParser->text));
        VReg_WStringView next;
        if (joined.cch == 0 || joined.pch[joined.cch - 1] != L'\\')
            break;

        VReg_WStringTruncate(&pParser->text, joined.cch - 1);

        ++current;
        next = VREG_INTERNAL_AT(pLines, VReg_WStringView, current);
        segment.start = pParser->text.cch;
        segment.line = current + 1;
        segment.column = 1;
        while (next.cch != 0 && VReg_IsBlank(next.pch[0]))
        {
            ++next.pch;
            --next.cch;
            ++segment.column;
        }
        if (
            !VReg_ArrayAppend(&pParser->segments, sizeof(segment), &segment)
            || !VReg_WStringAppend(&pParser->text, next.pch, next.cch)
        )
            return ERROR_OUTOFMEMORY;
    }

    *pLast = current;
    return ERROR_SUCCESS;
}

static LSTATUS VReg_ParseKey(_Inout_ VReg_Parser* const pParser)
{
    const VReg_WStringView text = VReg_WStringViewOf(&pParser->text);
    SIZE_T close = text.cch;
    BOOL isDelete;
    VReg_WStringView full;
    SIZE_T slash;
    VReg_WStringView path;
    HKEY hRoot = NULL;
    LSTATUS status;

    while (close > 0 && text.pch[close - 1] != L']')
        --close;
    if (close == 0)
    {
        pParser->pos = text.cch;
        return VReg_ParseFail(pParser, "missing ] after the key name");
    }

    // `close` is one past the bracket: the position that ends the key.
    pParser->pos = close;
    status = VReg_ExpectEnd(pParser);
    if (status != ERROR_SUCCESS)
        return status;

    --close;
    pParser->pos = 1;
    isDelete = text.pch[pParser->pos] == L'-';
    if (isDelete)
        ++pParser->pos;

    full = VReg_Substring(text, pParser->pos, close - pParser->pos);
    slash = VReg_FindChar(full, 0, L'\\');
    if (!VReg_FindRoot(VReg_Substring(full, 0, slash), &hRoot))
        return VReg_ParseFail(pParser, "unknown registry root");

    path = slash < full.cch ? VReg_Substring(full, slash + 1, full.cch - slash - 1) : VReg_Substring(full, 0, 0);
    if (isDelete && path.cch == 0)
        return VReg_ParseFail(pParser, "a root key cannot be deleted");

    status = VReg_ScriptAppend(
        pParser->pScript,
        isDelete ? VREG_OPERATION_DELETE_KEY : VREG_OPERATION_CREATE_KEY,
        hRoot,
        path,
        VReg_ViewOf(L""),
        0,
        NULL,
        0,
        pParser->line
    );
    if (status != ERROR_SUCCESS)
        return status;

    pParser->state = isDelete ? VREG_INTERNAL_KEY_DELETED : VREG_INTERNAL_KEY_OPEN;
    pParser->hRoot = hRoot;
    return VReg_WStringAssign(&pParser->path, path.pch, path.cch) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

/// Reads a quoted string at the position (on its opening quote) into @p pText.
static LSTATUS VReg_ParseQuoted(_Inout_ VReg_Parser* const pParser, _Inout_ VReg_WString* const pText)
{
    ++pParser->pos;
    if (!VReg_WStringResize(pText, 0))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        wchar_t ch;
        if (VReg_AtEnd(pParser))
            return VReg_ParseFail(pParser, "unterminated string");

        ch = VReg_Current(pParser);
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
            if (VReg_AtEnd(pParser) || (VReg_Current(pParser) != L'\\' && VReg_Current(pParser) != L'"'))
            {
                --pParser->pos;
                return VReg_ParseFail(pParser, "invalid escape, only \\\\ and \\\" are allowed");
            }

            ch = VReg_Current(pParser);
        }

        if (!VReg_WStringAppendChar(pText, ch))
            return ERROR_OUTOFMEMORY;

        ++pParser->pos;
    }
}

static LSTATUS VReg_ParseHexNumber(
    _Inout_ VReg_Parser* const pParser,
    const SIZE_T maxDigits,
    _Out_ DWORD* const pdwValue
)
{
    DWORD dwValue = 0;
    SIZE_T digits = 0;
    *pdwValue = 0;
    while (!VReg_AtEnd(pParser) && VReg_HexDigit(VReg_Current(pParser)) >= 0)
    {
        if (++digits > maxDigits)
            return VReg_ParseFail(pParser, "too many hex digits");

        dwValue = (dwValue << 4) | (DWORD)VReg_HexDigit(VReg_Current(pParser));
        ++pParser->pos;
    }
    if (digits == 0)
        return VReg_ParseFail(pParser, "expected a hex number");

    *pdwValue = dwValue;
    return ERROR_SUCCESS;
}

static LSTATUS VReg_ParseHexBytes(_Inout_ VReg_Parser* const pParser, _Inout_ VReg_Buffer* const pData)
{
    VReg_SkipBlanks(pParser);
    if (VReg_AtEnd(pParser))
        return ERROR_SUCCESS;

    for (;;)
    {
        DWORD dwByte;
        BYTE byte;
        LSTATUS status;
        if (VReg_AtEnd(pParser) || VReg_HexDigit(VReg_Current(pParser)) < 0)
            return VReg_ParseFail(pParser, "expected a hex byte");

        status = VReg_ParseHexNumber(pParser, 2, &dwByte);
        if (status != ERROR_SUCCESS)
            return status;

        byte = (BYTE)dwByte;
        if (!VReg_BufferAppend(pData, &byte, 1))
            return ERROR_OUTOFMEMORY;

        VReg_SkipBlanks(pParser);
        if (VReg_AtEnd(pParser))
            return ERROR_SUCCESS;
        if (VReg_Current(pParser) != L',')
            return VReg_ParseFail(pParser, "expected , between hex bytes");

        ++pParser->pos;
        VReg_SkipBlanks(pParser);
    }
}

static LSTATUS VReg_ParseValue(_Inout_ VReg_Parser* const pParser)
{
    VReg_WString name = {0, 0, NULL};
    VReg_WString text = {0, 0, NULL};
    VReg_Buffer data = {0, 0, NULL};
    VRegOperationKind kind = VREG_OPERATION_SET_VALUE;
    DWORD dwType = 0;
    LSTATUS status = ERROR_SUCCESS;

    if (VReg_Current(pParser) == L'@')
        ++pParser->pos;
    else if (VReg_Current(pParser) == L'"')
        status = VReg_ParseQuoted(pParser, &name);
    else
        status = VReg_ParseFail(pParser, "expected a value name or a [key]");
    if (status != ERROR_SUCCESS)
        goto done;

    if (pParser->state == VREG_INTERNAL_KEY_NONE)
    {
        pParser->pos = 0;
        status = VReg_ParseFail(pParser, "a value outside any key");
        goto done;
    }

    // A value under a [-key] line is an error rather than silently dropped,
    // since it can only be a mistake.
    if (pParser->state == VREG_INTERNAL_KEY_DELETED)
    {
        pParser->pos = 0;
        status = VReg_ParseFail(pParser, "a value after a deleted key");
        goto done;
    }

    VReg_SkipBlanks(pParser);
    if (VReg_AtEnd(pParser) || VReg_Current(pParser) != L'=')
    {
        status = VReg_ParseFail(pParser, "expected =");
        goto done;
    }

    ++pParser->pos;
    VReg_SkipBlanks(pParser);

    if (!VReg_AtEnd(pParser) && VReg_Current(pParser) == L'-')
    {
        ++pParser->pos;
        status = VReg_ExpectEnd(pParser);
        kind = VREG_OPERATION_DELETE_VALUE;
    }
    else if (!VReg_AtEnd(pParser) && VReg_Current(pParser) == L'"')
    {
        status = VReg_ParseQuoted(pParser, &text);
        if (status == ERROR_SUCCESS)
            status = VReg_ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_SZ;
            if (!VReg_BufferAssign(&data, VReg_WStringText(&text), (text.cch + 1) * sizeof(wchar_t)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VReg_Consume(pParser, L"dword:"))
    {
        DWORD dwValue = 0;
        status = VReg_ParseHexNumber(pParser, 8, &dwValue);
        if (status == ERROR_SUCCESS)
            status = VReg_ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_DWORD;
            if (!VReg_BufferAssign(&data, &dwValue, sizeof(dwValue)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VReg_Consume(pParser, L"hex"))
    {
        dwType = REG_BINARY;
        if (VReg_Consume(pParser, L"("))
        {
            status = VReg_ParseHexNumber(pParser, 8, &dwType);
            if (status == ERROR_SUCCESS && !VReg_Consume(pParser, L")"))
                status = VReg_ParseFail(pParser, "expected ) after the value type");
        }
        if (status == ERROR_SUCCESS && !VReg_Consume(pParser, L":"))
            status = VReg_ParseFail(pParser, "expected : after hex");
        if (status == ERROR_SUCCESS)
            status = VReg_ParseHexBytes(pParser, &data);
    }
    else
    {
        status = VReg_ParseFail(pParser, "expected a string, dword:, hex: or -");
    }
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg_ScriptAppend(
        pParser->pScript,
        kind,
        pParser->hRoot,
        VReg_WStringViewOf(&pParser->path),
        VReg_WStringViewOf(&name),
        dwType,
        data.pb,
        (DWORD)data.cb,
        pParser->line
    );

done:
    VReg_WStringFree(&name);
    VReg_WStringFree(&text);
    VReg_BufferFree(&data);
    return status;
}

static LSTATUS VReg_ParseLogicalLine(_Inout_ VReg_Parser* const pParser)
{
    // Key paths and value names reach the registry as C strings, where a NUL
    // would silently cut them short ("[-HKCU\<NUL>x]" would delete all of
    // HKCU); no valid .reg text holds one anywhere.
    pParser->pos = VReg_FindChar(VReg_WStringViewOf(&pParser->text), 0, L'\0');
    if (pParser->pos < pParser->text.cch)
        return VReg_ParseFail(pParser, "a NUL character");

    pParser->pos = 0;
    return pParser->text.psz[0] == L'[' ? VReg_ParseKey(pParser) : VReg_ParseValue(pParser);
}

/// Parses all of @p text into the parser's script. A syntax error answers
/// @ref ERROR_INVALID_DATA with @p pError filled in.
static LSTATUS VReg_ParseText(
    _Inout_ VReg_Parser* const pParser,
    const VReg_WStringView text,
    _Inout_opt_ VRegParseError* const pError
)
{
    VReg_Array lines = {NULL, 0, 0};
    VReg_WStringView header;
    SIZE_T start;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    for (start = 0; start <= text.cch;)
    {
        const SIZE_T end = VReg_FindChar(text, start, L'\n');
        VReg_WStringView line = VReg_Substring(text, start, end - start);
        if (line.cch != 0 && line.pch[line.cch - 1] == L'\r')
            --line.cch;
        if (!VReg_ArrayAppend(&lines, sizeof(line), &line))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        start = end + 1;
    }

    // The loop above appended the first line, however short the text.
    _Analysis_assume_(lines.p != NULL);
    header = VReg_TrimEnd(VREG_INTERNAL_AT(&lines, VReg_WStringView, 0));
    if (
        !VReg_ViewEquals(header, VReg_ViewOf(L"Windows Registry Editor Version 5.00"))
        && !VReg_ViewEquals(header, VReg_ViewOf(L"REGEDIT4"))
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
        status = VReg_ReadLogicalLine(pParser, &lines, i, &last);
        if (status == ERROR_SUCCESS && pParser->text.cch != 0)
            status = VReg_ParseLogicalLine(pParser);
        if (status == ERROR_INVALID_DATA && pError)
            VReg_ParserPosition(pParser, pParser->errorOffset, &pError->line, &pError->column);
        if (status == ERROR_INVALID_DATA && pError)
            pError->pszMessage = pParser->pszError;
        if (status != ERROR_SUCCESS)
            goto done;

        i = last;
    }

done:
    VReg_ArrayFree(&lines);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegParseScript(
    _In_reads_bytes_(cbFile) const void* const pFile,
    _In_ const SIZE_T cbFile,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VReg_WString text = {0, 0, NULL};
    VReg_Parser parser;
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));

    status = VReg_DecodeFile(pFile, cbFile, &text, pError);
    if (status != ERROR_SUCCESS)
    {
        VReg_WStringFree(&text);
        return status;
    }

    ZeroMemory(&parser, sizeof(parser));
    status = VReg_ScriptCreate(&parser.pScript);
    if (status == ERROR_SUCCESS)
        status = VReg_ParseText(&parser, VReg_WStringViewOf(&text), pError);

    VReg_WStringFree(&parser.text);
    VReg_WStringFree(&parser.path);
    VReg_ArrayFree(&parser.segments);
    VReg_WStringFree(&text);
    if (status != ERROR_SUCCESS)
    {
        VRegDestroyScript(parser.pScript);
        return status;
    }

    *ppScript = parser.pScript;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS WINAPI VRegParseScriptFile(
    _In_z_ const LPCWSTR pszPath,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VReg_Buffer file = {0, 0, NULL};
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));
    status = VReg_ReadWholeFile(pszPath, &file);
    if (status == ERROR_SUCCESS)
        status = VRegParseScript(file.pb, file.cb, ppScript, pError);

    VReg_BufferFree(&file);
    return status;
}

// ==========================================================================
// Scripts
// ==========================================================================

/// Every root a `.reg` file can name, the one list import and export share.
static const VReg_RootKey VREG_INTERNAL_ROOT_KEYS[] = {
    {L"HKEY_CLASSES_ROOT", L"HKCR", HKEY_CLASSES_ROOT},
    {L"HKEY_CURRENT_USER", L"HKCU", HKEY_CURRENT_USER},
    {L"HKEY_LOCAL_MACHINE", L"HKLM", HKEY_LOCAL_MACHINE},
    {L"HKEY_USERS", L"HKU", HKEY_USERS},
    {L"HKEY_CURRENT_CONFIG", L"HKCC", HKEY_CURRENT_CONFIG},
};

VREG_DECL _Ret_maybenull_z_ const wchar_t* VReg_RootName(_In_opt_ const HKEY hRoot)
{
    SIZE_T i;
    for (i = 0; i < sizeof(VREG_INTERNAL_ROOT_KEYS) / sizeof(VREG_INTERNAL_ROOT_KEYS[0]); ++i)
    {
        if (VREG_INTERNAL_ROOT_KEYS[i].hKey == hRoot)
            return VREG_INTERNAL_ROOT_KEYS[i].pszLong;
    }

    return NULL;
}

VREG_DECL BOOL VReg_FindRoot(const VReg_WStringView name, _Out_ PHKEY const phRoot)
{
    SIZE_T i;
    *phRoot = NULL;
    for (i = 0; i < sizeof(VREG_INTERNAL_ROOT_KEYS) / sizeof(VREG_INTERNAL_ROOT_KEYS[0]); ++i)
    {
        const VReg_RootKey* const pRoot = &VREG_INTERNAL_ROOT_KEYS[i];
        if (
            VReg_EqualsIgnoreAsciiCase(name, VReg_ViewOf(pRoot->pszLong))
            || VReg_EqualsIgnoreAsciiCase(name, VReg_ViewOf(pRoot->pszShort))
        )
        {
            *phRoot = pRoot->hKey;
            return TRUE;
        }
    }

    return FALSE;
}

VREG_DECL LSTATUS VReg_ScriptCreate(_Outptr_ VRegScript** const ppScript)
{
    VRegScript* const pScript = (VRegScript*)VReg_AllocZeroed(sizeof(VRegScript));
    *ppScript = NULL;
    if (!pScript)
        return ERROR_OUTOFMEMORY;

    *ppScript = pScript;
    return ERROR_SUCCESS;
}

VREG_DECL LSTATUS VReg_ScriptAppend(
    _Inout_ VRegScript* const pScript,
    const VRegOperationKind kind,
    _In_ const HKEY hRoot,
    const VReg_WStringView path,
    const VReg_WStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    const SIZE_T line
)
{
    VReg_ScriptEntry entry;
    VRegOperation operation;
    ZeroMemory(&entry, sizeof(entry));
    if (
        !VReg_WStringAssign(&entry.path, path.pch, path.cch)
        || !VReg_WStringAssign(&entry.name, name.pch, name.cch)
        || !VReg_BufferAssign(&entry.data, pData, cbData)
        || !VReg_ArrayReserve(&pScript->operations, sizeof(operation), pScript->operations.count + 1)
        || !VReg_ArrayAppend(&pScript->entries, sizeof(entry), &entry)
    )
    {
        VReg_WStringFree(&entry.path);
        VReg_WStringFree(&entry.name);
        VReg_BufferFree(&entry.data);
        return ERROR_OUTOFMEMORY;
    }

    // The entry's storage is on the heap, so the pointers stay valid when the
    // array of entries moves.
    operation.kind = kind;
    operation.hRoot = hRoot;
    operation.pszPath = VReg_WStringText(&entry.path);
    operation.pszName = VReg_WStringText(&entry.name);
    operation.dwType = dwType;
    operation.pData = entry.data.pb;
    operation.cbData = cbData;
    operation.line = line;
    VReg_ArrayAppend(&pScript->operations, sizeof(operation), &operation);
    return ERROR_SUCCESS;
}

VREG_DECL void WINAPI VRegDestroyScript(_In_opt_ VRegScript* const pScript)
{
    SIZE_T i;
    if (!pScript)
        return;

    for (i = 0; i < pScript->entries.count; ++i)
    {
        VReg_ScriptEntry* const pEntry = &VREG_INTERNAL_AT(&pScript->entries, VReg_ScriptEntry, i);
        VReg_WStringFree(&pEntry->path);
        VReg_WStringFree(&pEntry->name);
        VReg_BufferFree(&pEntry->data);
    }

    VReg_ArrayFree(&pScript->entries);
    VReg_ArrayFree(&pScript->operations);
    VReg_Free(pScript);
}

VREG_DECL _Ret_notnull_ const VRegOperation* WINAPI VRegGetScriptOperations(
    _In_ const VRegScript* const pScript,
    _Out_ SIZE_T* const pcOperations
)
{
    static const VRegOperation NONE = {VREG_OPERATION_CREATE_KEY, NULL, L"", L"", 0, NULL, 0, 0};
    *pcOperations = pScript->operations.count;
    return pScript->operations.count == 0 ? &NONE : (const VRegOperation*)pScript->operations.p;
}

VREG_DECL void WINAPI VRegFree(_In_opt_ void* const p)
{
    VReg_Free(p);
}

// ==========================================================================
// Script text
// ==========================================================================

static LSTATUS VReg_DecodeFail(_Inout_opt_ VRegParseError* const pError, _In_z_ const char* const pszMessage)
{
    if (pError)
    {
        pError->line = 1;
        pError->column = 1;
        pError->pszMessage = pszMessage;
    }

    return ERROR_INVALID_DATA;
}

static BOOL VReg_StartsWithAscii(
    _In_reads_bytes_(cb) const BYTE* const pb,
    const SIZE_T cb,
    _In_z_ const char* const pszPrefix
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

static LSTATUS VReg_Widen(
    const UINT codePage,
    _In_reads_bytes_(cb) const BYTE* const pb,
    const SIZE_T cb,
    _Inout_ VReg_WString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const DWORD dwFlags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const LSTATUS status = VReg_WideFromMultiByte(codePage, dwFlags, (const char*)pb, cb, pText);
    if (status == ERROR_NO_UNICODE_TRANSLATION)
        return VReg_DecodeFail(pError, "the text is not valid in its encoding");

    return status;
}

VREG_DECL LSTATUS VReg_DecodeFile(
    _In_reads_bytes_(cbFile) const void* const pFile,
    const SIZE_T cbFile,
    _Inout_ VReg_WString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const BYTE* const pb = (const BYTE*)pFile;
    if (cbFile > (SIZE_T)MAXINT32)
        return VReg_DecodeFail(pError, "the file is too large");

    if (cbFile >= 2 && pb[0] == 0xFF && pb[1] == 0xFE)
    {
        const SIZE_T cbBody = cbFile - 2;
        if (cbBody % sizeof(wchar_t) != 0)
            return VReg_DecodeFail(pError, "a UTF-16 file has an odd byte count");
        if (!VReg_WStringAssign(pText, (const wchar_t*)(pb + 2), cbBody / sizeof(wchar_t)))
            return ERROR_OUTOFMEMORY;

        return ERROR_SUCCESS;
    }
    if (cbFile >= 2 && pb[0] == 0xFE && pb[1] == 0xFF)
        return VReg_DecodeFail(pError, "big-endian UTF-16 is not supported");
    if (cbFile >= 3 && pb[0] == 0xEF && pb[1] == 0xBB && pb[2] == 0xBF)
        return VReg_Widen(CP_UTF8, pb + 3, cbFile - 3, pText, pError);

    return VReg_Widen(
        VReg_StartsWithAscii(pb, cbFile, "REGEDIT4") ? CP_ACP : CP_UTF8,
        pb,
        cbFile,
        pText,
        pError
    );
}

VREG_DECL LSTATUS VReg_EncodeFile(
    _In_ const VReg_WString* const pText,
    const VRegFormat format,
    _Inout_ VReg_Buffer* const pFile
)
{
    UINT codePage;
    BOOL isUtf8;
    BOOL fUsedDefault = FALSE;
    LSTATUS status;

    if (!VReg_BufferResize(pFile, 0))
        return ERROR_OUTOFMEMORY;

    if (format == VREG_FORMAT_UNICODE)
    {
        static const BYTE BOM[2] = {0xFF, 0xFE};
        if (
            !VReg_BufferAppend(pFile, BOM, sizeof(BOM))
            || !VReg_BufferAppend(pFile, VReg_WStringText(pText), pText->cch * sizeof(wchar_t))
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
    status = VReg_MultiByteFromWide(
        codePage,
        isUtf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        isUtf8 ? NULL : &fUsedDefault,
        VReg_WStringText(pText),
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

VREG_DECL LSTATUS VReg_ReadWholeFile(
    _In_z_ const LPCWSTR pszPath,
    _Inout_ VReg_Buffer* const pFile
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
    else if (!VReg_BufferResize(pFile, (SIZE_T)size.QuadPart))
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

VREG_DECL LSTATUS VReg_WriteWholeFile(
    _In_z_ const LPCWSTR pszPath,
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
static BOOL VReg_OutAppend(
    _Inout_ VReg_WString* const pOut,
    _In_z_ const wchar_t* const psz
)
{
    const VReg_WStringView text = VReg_ViewOf(psz);
    return VReg_WStringAppend(pOut, text.pch, text.cch);
}

static BOOL VReg_HasLineBreak(const VReg_WStringView text)
{
    SIZE_T i;
    for (i = 0; i < text.cch; ++i)
    {
        if (text.pch[i] == L'\r' || text.pch[i] == L'\n')
            return TRUE;
    }

    return FALSE;
}

static BOOL VReg_AppendQuoted(
    _Inout_ VReg_WString* const pOut,
    const VReg_WStringView text
)
{
    SIZE_T i;
    if (!VReg_WStringAppendChar(pOut, L'"'))
        return FALSE;

    for (i = 0; i < text.cch; ++i)
    {
        const wchar_t ch = text.pch[i];
        if ((ch == L'\\' || ch == L'"') && !VReg_WStringAppendChar(pOut, L'\\'))
            return FALSE;
        if (!VReg_WStringAppendChar(pOut, ch))
            return FALSE;
    }

    return VReg_WStringAppendChar(pOut, L'"');
}

/// The text a REG_SZ takes in the friendly form; FALSE when the form would not
/// reproduce the bytes.
static BOOL VReg_FriendlyString(
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    _Out_ VReg_WStringView* pText
)
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
static BOOL VReg_AppendHexNumber(
    _Inout_ VReg_WString* const pOut,
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
        if (!VReg_WStringAppendChar(pOut, L'0'))
            return FALSE;
    }
    for (i = 0; i < cchDigits; ++i)
    {
        if (!VReg_WStringAppendChar(pOut, wszDigits[cchDigits - 1 - i]))
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg_AppendHex(
    _Inout_ VReg_WString* const pOut,
    _In_ const VRegOperation* const pOperation,
    SIZE_T column
)
{
    const SIZE_T cchBefore = pOut->cch;
    SIZE_T i;

    if (pOperation->dwType == REG_BINARY)
    {
        if (!VReg_OutAppend(pOut, L"hex:"))
            return FALSE;
    }
    else
    {
        if (
            !VReg_OutAppend(pOut, L"hex(")
            || !VReg_AppendHexNumber(pOut, pOperation->dwType, 1)
            || !VReg_OutAppend(pOut, L"):")
        )
            return FALSE;
    }

    column += pOut->cch - cchBefore;
    for (i = 0; i < pOperation->cbData; ++i)
    {
        if (!VReg_AppendHexNumber(pOut, pOperation->pData[i], 2))
            return FALSE;

        column += 2;
        if (i + 1 == pOperation->cbData)
            break;
        if (!VReg_WStringAppendChar(pOut, L','))
            return FALSE;
        if (++column > VREG_INTERNAL_HEX_WRAP_COLUMN)
        {
            if (!VReg_OutAppend(pOut, L"\\\r\n  "))
                return FALSE;

            column = 2;
        }
    }

    return TRUE;
}

static BOOL VReg_AppendValue(
    _Inout_ VReg_WString* const pOut,
    _In_ const VRegOperation* const pOperation
)
{
    const SIZE_T lineStart = pOut->cch;
    const VReg_WStringView name = VReg_ViewOf(pOperation->pszName);
    VReg_WStringView text;
    BOOL isAppended;

    if (name.cch == 0)
        isAppended = VReg_WStringAppendChar(pOut, L'@');
    else
        isAppended = VReg_AppendQuoted(pOut, name);
    if (!isAppended || !VReg_WStringAppendChar(pOut, L'='))
        return FALSE;

    if (pOperation->kind == VREG_OPERATION_DELETE_VALUE)
    {
        isAppended = VReg_WStringAppendChar(pOut, L'-');
    }
    else if (pOperation->dwType == REG_SZ && VReg_FriendlyString(pOperation->pData, pOperation->cbData, &text))
    {
        isAppended = VReg_AppendQuoted(pOut, text);
    }
    else if (pOperation->dwType == REG_DWORD && pOperation->cbData == sizeof(DWORD))
    {
        DWORD dwValue;
        CopyMemory(&dwValue, pOperation->pData, sizeof(dwValue));
        isAppended =
            VReg_OutAppend(pOut, L"dword:")
            && VReg_AppendHexNumber(pOut, dwValue, 8);
    }
    else
    {
        isAppended = VReg_AppendHex(pOut, pOperation, pOut->cch - lineStart);
    }

    return isAppended && VReg_OutAppend(pOut, VREG_INTERNAL_NEWLINE);
}

/// Appends `[key]` or `[-key]` for @p pOperation's key, preceded by a blank
/// line unless it opens the file.
static LSTATUS VReg_AppendKeyLine(
    _Inout_ VReg_WString* const pOut,
    _In_ const VRegOperation* const pOperation,
    const BOOL isDelete,
    const BOOL isFirst
)
{
    const wchar_t* const pszRoot = VReg_RootName(pOperation->hRoot);
    const VReg_WStringView path = VReg_ViewOf(pOperation->pszPath);
    if (!pszRoot)
        return ERROR_INVALID_HANDLE;

    if (
        (!isFirst && !VReg_OutAppend(pOut, VREG_INTERNAL_NEWLINE))
        || !VReg_OutAppend(pOut, isDelete ? L"[-" : L"[")
        || !VReg_OutAppend(pOut, pszRoot)
        || (path.cch != 0 && !VReg_WStringAppendChar(pOut, L'\\'))
        || !VReg_WStringAppend(pOut, path.pch, path.cch)
        || !VReg_OutAppend(pOut, L"]")
        || !VReg_OutAppend(pOut, VREG_INTERNAL_NEWLINE)
    )
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

/// Whether @p pLeft and @p pRight name the same key.
static BOOL VReg_SameKey(_In_ const VRegOperation* const pLeft, _In_ const VRegOperation* const pRight)
{
    return pLeft->hRoot == pRight->hRoot
        && VReg_CompareNames(VReg_ViewOf(pLeft->pszPath), VReg_ViewOf(pRight->pszPath)) == 0;
}

VREG_DECL LSTATUS WINAPI VRegWriteScript(
    _In_ const VRegScript* const pScript,
    _In_ const VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** const ppBytes,
    _Out_ SIZE_T* const pcbBytes
)
{
    VReg_WString out = {0, 0, NULL};
    VReg_Buffer file = {0, 0, NULL};
    const VRegOperation* pLastKey = NULL;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    *ppBytes = NULL;
    *pcbBytes = 0;
    if (
        !VReg_OutAppend(
            &out,
            format == VREG_FORMAT_REGEDIT4 ? L"REGEDIT4" : L"Windows Registry Editor Version 5.00"
        )
        || !VReg_OutAppend(&out, VREG_INTERNAL_NEWLINE)
        || !VReg_OutAppend(&out, VREG_INTERNAL_NEWLINE)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    for (i = 0; i < pScript->operations.count; ++i)
    {
        const VRegOperation* const pOperation = &VREG_INTERNAL_AT(&pScript->operations, VRegOperation, i);
        const BOOL isKey = pOperation->kind == VREG_OPERATION_CREATE_KEY || pOperation->kind ==
            VREG_OPERATION_DELETE_KEY;
        if (VReg_HasLineBreak(VReg_ViewOf(pOperation->pszPath)) || VReg_HasLineBreak(
            VReg_ViewOf(pOperation->pszName)
        ))
        {
            status = ERROR_INVALID_NAME;
            goto done;
        }

        if (isKey || !pLastKey || !VReg_SameKey(pLastKey, pOperation))
        {
            status = VReg_AppendKeyLine(
                &out,
                pOperation,
                pOperation->kind == VREG_OPERATION_DELETE_KEY,
                i == 0
            );
            if (status != ERROR_SUCCESS)
                goto done;

            pLastKey = pOperation;
        }

        if (!isKey && !VReg_AppendValue(&out, pOperation))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    if (pScript->operations.count != 0 && !VReg_OutAppend(&out, VREG_INTERNAL_NEWLINE))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    status = VReg_EncodeFile(&out, format, &file);
    if (status != ERROR_SUCCESS)
        goto done;

    // The header alone makes the file non-empty, so the encoding allocated.
    _Analysis_assume_(file.pb != NULL);
    *ppBytes = file.pb;
    *pcbBytes = file.cb;
    file.pb = NULL;

done:
    VReg_WStringFree(&out);
    VReg_BufferFree(&file);
    return status;
}

VREG_DECL LSTATUS WINAPI VRegWriteScriptFile(
    _In_ const VRegScript* const pScript,
    _In_ const VRegFormat format,
    _In_z_ const LPCWSTR pszPath
)
{
    void* pBytes = NULL;
    SIZE_T cbBytes = 0;
    LSTATUS status = VRegWriteScript(pScript, format, &pBytes, &cbBytes);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg_WriteWholeFile(pszPath, pBytes, cbBytes);
    VRegFree(pBytes);
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

#include <stdlib.h>

// Detours takes hook bodies and trampolines as data pointers, and the thunks
// are bytes; the conversions below are what its API is made of.
#pragma warning(disable: 4054 4055 4152)

/// The registry every hook calls through.
static VReg* volatile g_pRegistry;

static VReg* VReg_HookRegistry(void)
{
    return (VReg*)ReadPointerAcquire((PVOID volatile*)&g_pRegistry);
}

// --- The hooks the registry answers: the same call, made on the registry ---

static LSTATUS APIENTRY VReg_HookRegOpenKeyExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyExW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS APIENTRY VReg_HookRegCreateKeyExW(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegCloseKey(
    _In_ const HKEY hKey
)
{
    return VRegCloseKey(
        VReg_HookRegistry(),
        hKey
    );
}

static LSTATUS APIENTRY VReg_HookRegQueryValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VRegQueryValueExW(
        VReg_HookRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg_HookRegSetValueExW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueExW(
        VReg_HookRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VRegDeleteValueW(
        VReg_HookRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VRegDeleteKeyExW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteTreeW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    return VRegDeleteTreeW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg_HookRegEnumKeyExW(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegEnumValueW(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegQueryInfoKeyW(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegLoadMUIStringW(
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
        VReg_HookRegistry(),
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
static LSTATUS APIENTRY VReg_HookRegGetValueW(
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
        VReg_HookRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS APIENTRY VReg_HookRegNotifyChangeKeyValue(
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    return VRegNotifyChangeKeyValue(
        VReg_HookRegistry(),
        hKey,
        bWatchSubtree,
        dwNotifyFilter,
        hEvent,
        fAsynchronous
    );
}

static LSTATUS APIENTRY VReg_HookRegFlushKey(
    _In_ const HKEY hKey
)
{
    return VRegFlushKey(
        VReg_HookRegistry(),
        hKey
    );
}

static LSTATUS APIENTRY VReg_HookRegOpenKeyExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyExA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        ulOptions,
        samDesired,
        phkResult
    );
}

static LSTATUS APIENTRY VReg_HookRegCreateKeyExA(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegQueryValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    return VRegQueryValueExA(
        VReg_HookRegistry(),
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg_HookRegSetValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueExA(
        VReg_HookRegistry(),
        hKey,
        lpValueName,
        Reserved,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VRegDeleteValueA(
        VReg_HookRegistry(),
        hKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    return VRegDeleteKeyExA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        samDesired,
        Reserved
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteTreeA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    return VRegDeleteTreeA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg_HookRegEnumKeyExA(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegEnumValueA(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegQueryInfoKeyA(
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
        VReg_HookRegistry(),
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

static LSTATUS APIENTRY VReg_HookRegLoadMUIStringA(
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
        VReg_HookRegistry(),
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
static LSTATUS APIENTRY VReg_HookRegGetValueA(
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
        VReg_HookRegistry(),
        hkey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

static LSTATUS APIENTRY VReg_HookRegOpenKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg_HookRegOpenKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegOpenKeyW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg_HookRegCreateKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegCreateKeyA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg_HookRegCreateKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    return VRegCreateKeyW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        phkResult
    );
}

static LSTATUS APIENTRY VReg_HookRegQueryValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VRegQueryValueA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg_HookRegQueryValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    return VRegQueryValueW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        lpData,
        lpcbData
    );
}

static LSTATUS APIENTRY VReg_HookRegSetValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg_HookRegSetValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetValueW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    return VRegDeleteKeyA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    return VRegDeleteKeyW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg_HookRegEnumKeyA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    return VRegEnumKeyA(
        VReg_HookRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS APIENTRY VReg_HookRegEnumKeyW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    return VRegEnumKeyW(
        VReg_HookRegistry(),
        hKey,
        dwIndex,
        lpName,
        cchName
    );
}

static LSTATUS APIENTRY VReg_HookRegSetKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetKeyValueA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg_HookRegSetKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    return VRegSetKeyValueW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName,
        dwType,
        lpData,
        cbData
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    return VRegDeleteKeyValueA(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg_HookRegDeleteKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    return VRegDeleteKeyValueW(
        VReg_HookRegistry(),
        hKey,
        lpSubKey,
        lpValueName
    );
}

static LSTATUS APIENTRY VReg_HookRegQueryMultipleValuesA(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VRegQueryMultipleValuesA(
        VReg_HookRegistry(),
        hKey,
        val_list,
        num_vals,
        lpValueBuf,
        ldwTotsize
    );
}

static LSTATUS APIENTRY VReg_HookRegQueryMultipleValuesW(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    return VRegQueryMultipleValuesW(
        VReg_HookRegistry(),
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

typedef enum VReg_RefusedIndex
{
#define VREG_INDEX(name) VREG_INTERNAL_REFUSED_##name,
    VREG_INTERNAL_REFUSED_HOOKS(VREG_INDEX)
#undef VREG_INDEX

    VREG_INTERNAL_REFUSED_COUNT
} VReg_RefusedIndex;

static void* g_refusedTargets[VREG_INTERNAL_REFUSED_COUNT];

// The forms the registry answers besides its wide surface, by index into
// their resolved entry points. The registry below the hooks never calls
// them (an ANSI or pre-Ex call reaches it as the wide Ex form), so their
// trampolines live here and serve VRegNeutralizeHooks alone.
typedef enum VReg_FormIndex
{
#define VREG_INDEX(name) VREG_INTERNAL_FORM_##name,
    VREG_INTERNAL_REGISTRY_FORMS(VREG_INDEX)
#undef VREG_INDEX

    VREG_INTERNAL_FORM_COUNT
} VReg_FormIndex;

static void* g_formTargets[VREG_INTERNAL_FORM_COUNT];

typedef LSTATUS (APIENTRY* VReg_RegSaveKeyAFunction)(
    HKEY,
    LPCSTR,
    CONST LPSECURITY_ATTRIBUTES
);

typedef LSTATUS (APIENTRY* VReg_RegSaveKeyWFunction)(
    HKEY,
    LPCWSTR,
    CONST LPSECURITY_ATTRIBUTES
);

typedef LSTATUS (APIENTRY* VReg_RegSaveKeyExAFunction)(
    HKEY,
    LPCSTR,
    CONST LPSECURITY_ATTRIBUTES,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg_RegSaveKeyExWFunction)(
    HKEY,
    LPCWSTR,
    CONST LPSECURITY_ATTRIBUTES,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg_RegRestoreKeyAFunction)(
    HKEY,
    LPCSTR,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg_RegRestoreKeyWFunction)(
    HKEY,
    LPCWSTR,
    DWORD
);

typedef LSTATUS (APIENTRY* VReg_RegLoadKeyAFunction)(
    HKEY,
    LPCSTR,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VReg_RegLoadKeyWFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg_RegUnLoadKeyAFunction)(
    HKEY,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VReg_RegUnLoadKeyWFunction)(
    HKEY,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg_RegReplaceKeyAFunction)(
    HKEY,
    LPCSTR,
    LPCSTR,
    LPCSTR
);

typedef LSTATUS (APIENTRY* VReg_RegReplaceKeyWFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg_RegGetKeySecurityFunction)(
    HKEY,
    SECURITY_INFORMATION,
    PSECURITY_DESCRIPTOR,
    LPDWORD
);

typedef LSTATUS (APIENTRY* VReg_RegSetKeySecurityFunction)(
    HKEY,
    SECURITY_INFORMATION,
    PSECURITY_DESCRIPTOR
);

typedef LSTATUS (APIENTRY* VReg_RegRenameKeyFunction)(
    HKEY,
    LPCWSTR,
    LPCWSTR
);

typedef LSTATUS (APIENTRY* VReg_RegCopyTreeAFunction)(
    HKEY,
    LPCSTR,
    HKEY
);

typedef LSTATUS (APIENTRY* VReg_RegCopyTreeWFunction)(
    HKEY,
    LPCWSTR,
    HKEY
);

typedef LSTATUS (APIENTRY* VReg_RegCreateKeyTransactedAFunction)(
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

typedef LSTATUS (APIENTRY* VReg_RegCreateKeyTransactedWFunction)(
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

typedef LSTATUS (APIENTRY* VReg_RegOpenKeyTransactedAFunction)(
    HKEY,
    LPCSTR,
    DWORD,
    REGSAM,
    PHKEY,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg_RegOpenKeyTransactedWFunction)(
    HKEY,
    LPCWSTR,
    DWORD,
    REGSAM,
    PHKEY,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg_RegDeleteKeyTransactedAFunction)(
    HKEY,
    LPCSTR,
    REGSAM,
    DWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg_RegDeleteKeyTransactedWFunction)(
    HKEY,
    LPCWSTR,
    REGSAM,
    DWORD,
    HANDLE,
    PVOID
);

typedef LSTATUS (APIENTRY* VReg_RegDisableReflectionKeyFunction)(
    HKEY
);

typedef LSTATUS (APIENTRY* VReg_RegEnableReflectionKeyFunction)(
    HKEY
);

typedef LSTATUS (APIENTRY* VReg_RegQueryReflectionKeyFunction)(
    HKEY,
    BOOL*
);

typedef LSTATUS (APIENTRY* VReg_RegOverridePredefKeyFunction)(
    HKEY,
    HKEY
);

static BOOL VReg_IsOwned(_In_opt_ const HKEY hKey)
{
    return VReg_HandleSpaceOwns(VReg_ProcessHandleSpace(), hKey);
}

/// Whether a @ref RegOpenKeyTransactedW call names no transaction, which it
/// answers with @ref ERROR_INVALID_TRANSACTION. The hook answers that
/// itself: on ARM64 the function reaches that answer through a branch back
/// into its first 12 bytes, which the hook's jump overwrites.
///
/// Answered on every architecture rather than on ARM64 alone, since the native
/// answer is the same everywhere and one path is simpler.
static BOOL VReg_IsUntransacted(
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
static LSTATUS VReg_RefuseMountPathW(
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR pszSubKey,
    const REGSAM samDesired
)
{
    VReg* const pRegistry = VReg_HookRegistry();
    const VReg_WStringView subKey = VReg_ViewOf(pszSubKey);
    VReg_WString prefix = {0, 0, NULL};
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T end;
    for (end = 0; end <= subKey.cch; ++end)
    {
        HKEY hProbe = NULL;
        BOOL isVirtual;
        LSTATUS opened;
        if (end != subKey.cch && subKey.pch[end] != L'\\')
            continue;

        if (!VReg_WStringAssign(&prefix, subKey.pch, end))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }

        opened = VRegOpenKeyExW(
            pRegistry,
            hKey,
            VReg_WStringText(&prefix),
            0,
            KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES),
            &hProbe
        );
        if (opened != ERROR_SUCCESS)
            continue;

        isVirtual = VReg_IsOwned(hProbe);
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

    VReg_WStringFree(&prefix);
    return status;
}

static LSTATUS VReg_RefuseMountPathA(
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCSTR pszSubKey,
    const REGSAM samDesired
)
{
    VReg_WideArg subKey;
    LSTATUS status = VReg_WideArgInit(pszSubKey, &subKey);
    if (status == ERROR_SUCCESS)
        status = VReg_RefuseMountPathW(hKey, VReg_WideArgText(&subKey), samDesired);

    VReg_WideArgFree(&subKey);
    return status;
}

static LSTATUS APIENTRY VReg_RefuseRegSaveKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
    const VReg_RegSaveKeyAFunction pfnNext =
        (VReg_RegSaveKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyA];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes
    );
}

static LSTATUS APIENTRY VReg_RefuseRegSaveKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
    const VReg_RegSaveKeyWFunction pfnNext =
        (VReg_RegSaveKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyW];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes
    );
}

static LSTATUS APIENTRY VReg_RefuseRegSaveKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ const DWORD Flags
)
{
    const VReg_RegSaveKeyExAFunction pfnNext =
        (VReg_RegSaveKeyExAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyExA];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes,
        Flags
    );
}

static LSTATUS APIENTRY VReg_RefuseRegSaveKeyExW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_opt_ CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ const DWORD Flags
)
{
    const VReg_RegSaveKeyExWFunction pfnNext =
        (VReg_RegSaveKeyExWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSaveKeyExW];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        lpSecurityAttributes,
        Flags
    );
}

static LSTATUS APIENTRY VReg_RefuseRegRestoreKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpFile,
    _In_ const DWORD dwFlags
)
{
    const VReg_RegRestoreKeyAFunction pfnNext =
        (VReg_RegRestoreKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegRestoreKeyA];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        dwFlags
    );
}

static LSTATUS APIENTRY VReg_RefuseRegRestoreKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpFile,
    _In_ const DWORD dwFlags
)
{
    const VReg_RegRestoreKeyWFunction pfnNext =
        (VReg_RegRestoreKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegRestoreKeyW];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpFile,
        dwFlags
    );
}

static LSTATUS APIENTRY VReg_RefuseRegLoadKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const LPCSTR lpFile
)
{
    const VReg_RegLoadKeyAFunction pfnNext =
        (VReg_RegLoadKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegLoadKeyA];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpFile
    );
}

static LSTATUS APIENTRY VReg_RefuseRegLoadKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const LPCWSTR lpFile
)
{
    const VReg_RegLoadKeyWFunction pfnNext =
        (VReg_RegLoadKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegLoadKeyW];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpFile
    );
}

static LSTATUS APIENTRY VReg_RefuseRegUnLoadKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    const VReg_RegUnLoadKeyAFunction pfnNext =
        (VReg_RegUnLoadKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegUnLoadKeyA];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg_RefuseRegUnLoadKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    const VReg_RegUnLoadKeyWFunction pfnNext =
        (VReg_RegUnLoadKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegUnLoadKeyW];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey
    );
}

static LSTATUS APIENTRY VReg_RefuseRegReplaceKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const LPCSTR lpNewFile,
    _In_ const LPCSTR lpOldFile
)
{
    const VReg_RegReplaceKeyAFunction pfnNext =
        (VReg_RegReplaceKeyAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegReplaceKeyA];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpNewFile,
        lpOldFile
    );
}

static LSTATUS APIENTRY VReg_RefuseRegReplaceKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const LPCWSTR lpNewFile,
    _In_ const LPCWSTR lpOldFile
)
{
    const VReg_RegReplaceKeyWFunction pfnNext =
        (VReg_RegReplaceKeyWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegReplaceKeyW];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKey,
        lpNewFile,
        lpOldFile
    );
}

static LSTATUS APIENTRY VReg_RefuseRegGetKeySecurity(
    _In_ const HKEY hKey,
    _In_ const SECURITY_INFORMATION SecurityInformation,
    _Out_writes_bytes_opt_(*lpcbSecurityDescriptor) const PSECURITY_DESCRIPTOR pSecurityDescriptor,
    _Inout_ const LPDWORD lpcbSecurityDescriptor
)
{
    const VReg_RegGetKeySecurityFunction pfnNext =
        (VReg_RegGetKeySecurityFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegGetKeySecurity];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        SecurityInformation,
        pSecurityDescriptor,
        lpcbSecurityDescriptor
    );
}

static LSTATUS APIENTRY VReg_RefuseRegSetKeySecurity(
    _In_ const HKEY hKey,
    _In_ const SECURITY_INFORMATION SecurityInformation,
    _In_ const PSECURITY_DESCRIPTOR pSecurityDescriptor
)
{
    const VReg_RegSetKeySecurityFunction pfnNext =
        (VReg_RegSetKeySecurityFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegSetKeySecurity];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        SecurityInformation,
        pSecurityDescriptor
    );
}

static LSTATUS APIENTRY VReg_RefuseRegRenameKey(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKeyName,
    _In_ const LPCWSTR lpNewKeyName
)
{
    const VReg_RegRenameKeyFunction pfnNext =
        (VReg_RegRenameKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegRenameKey];
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKey,
        lpSubKeyName,
        lpNewKeyName
    );
}

static LSTATUS APIENTRY VReg_RefuseRegCopyTreeA(
    _In_ const HKEY hKeySrc,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const HKEY hKeyDest
)
{
    const VReg_RegCopyTreeAFunction pfnNext =
        (VReg_RegCopyTreeAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCopyTreeA];
    if (VReg_IsOwned(hKeySrc) || VReg_IsOwned(hKeyDest))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKeySrc,
        lpSubKey,
        hKeyDest
    );
}

static LSTATUS APIENTRY VReg_RefuseRegCopyTreeW(
    _In_ const HKEY hKeySrc,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const HKEY hKeyDest
)
{
    const VReg_RegCopyTreeWFunction pfnNext =
        (VReg_RegCopyTreeWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCopyTreeW];
    if (VReg_IsOwned(hKeySrc) || VReg_IsOwned(hKeyDest))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hKeySrc,
        lpSubKey,
        hKeyDest
    );
}

static LSTATUS APIENTRY VReg_RefuseRegCreateKeyTransactedA(
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
    const VReg_RegCreateKeyTransactedAFunction pfnNext =
        (VReg_RegCreateKeyTransactedAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCreateKeyTransactedA];
    LSTATUS status;
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg_RefuseMountPathA(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VReg_RefuseRegCreateKeyTransactedW(
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
    const VReg_RegCreateKeyTransactedWFunction pfnNext =
        (VReg_RegCreateKeyTransactedWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegCreateKeyTransactedW];
    LSTATUS status;
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg_RefuseMountPathW(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VReg_RefuseRegOpenKeyTransactedA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VReg_RegOpenKeyTransactedAFunction pfnNext =
        (VReg_RegOpenKeyTransactedAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegOpenKeyTransactedA];
    LSTATUS status;
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg_RefuseMountPathA(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VReg_RefuseRegOpenKeyTransactedW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParemeter
)
{
    const VReg_RegOpenKeyTransactedWFunction pfnNext =
        (VReg_RegOpenKeyTransactedWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegOpenKeyTransactedW];
    LSTATUS status;
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg_RefuseMountPathW(hKey, lpSubKey, samDesired);
    if (status != ERROR_SUCCESS)
        return status;
    if (VReg_IsUntransacted(hTransaction, pExtendedParemeter))
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

static LSTATUS APIENTRY VReg_RefuseRegDeleteKeyTransactedA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParameter
)
{
    const VReg_RegDeleteKeyTransactedAFunction pfnNext =
        (VReg_RegDeleteKeyTransactedAFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegDeleteKeyTransactedA];
    LSTATUS status;
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg_RefuseMountPathA(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VReg_RefuseRegDeleteKeyTransactedW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved,
    _In_ const HANDLE hTransaction,
    _Reserved_ const PVOID pExtendedParameter
)
{
    const VReg_RegDeleteKeyTransactedWFunction pfnNext =
        (VReg_RegDeleteKeyTransactedWFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegDeleteKeyTransactedW];
    LSTATUS status;
    if (VReg_IsOwned(hKey))
        return ERROR_NOT_SUPPORTED;

    status = VReg_RefuseMountPathW(hKey, lpSubKey, samDesired);
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

static LSTATUS APIENTRY VReg_RefuseRegDisableReflectionKey(
    _In_ const HKEY hBase
)
{
    const VReg_RegDisableReflectionKeyFunction pfnNext =
        (VReg_RegDisableReflectionKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegDisableReflectionKey];
    if (VReg_IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase
    );
}

static LSTATUS APIENTRY VReg_RefuseRegEnableReflectionKey(
    _In_ const HKEY hBase
)
{
    const VReg_RegEnableReflectionKeyFunction pfnNext =
        (VReg_RegEnableReflectionKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegEnableReflectionKey];
    if (VReg_IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase
    );
}

static LSTATUS APIENTRY VReg_RefuseRegQueryReflectionKey(
    _In_ const HKEY hBase,
    _Out_ BOOL* const bIsReflectionDisabled
)
{
    const VReg_RegQueryReflectionKeyFunction pfnNext =
        (VReg_RegQueryReflectionKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegQueryReflectionKey];
    if (VReg_IsOwned(hBase))
        return ERROR_NOT_SUPPORTED;

    return pfnNext(
        hBase,
        bIsReflectionDisabled
    );
}

static LSTATUS APIENTRY VReg_RefuseRegOverridePredefKey(
    _In_ const HKEY hKey,
    _In_opt_ const HKEY hNewHKey
)
{
    const VReg_RegOverridePredefKeyFunction pfnNext =
        (VReg_RegOverridePredefKeyFunction)g_refusedTargets[VREG_INTERNAL_REFUSED_RegOverridePredefKey];
    if (VReg_IsOwned(hKey) || VReg_IsOwned(hNewHKey))
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

static void** VReg_ThunkSlot(const SIZE_T index)
{
    return (void**)(g_pbThunks + VREG_INTERNAL_THUNK_PAGE_SIZE) + index;
}

/// Writes thunk @p index, initially jumping to @p pTarget, and answers its
/// address.
static void* VReg_WriteThunk(const SIZE_T index, _In_ void* const pTarget)
{
    BYTE* const pbCode = g_pbThunks + index * VREG_INTERNAL_THUNK_CODE_SIZE;
    void** const ppSlot = VReg_ThunkSlot(index);
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

typedef struct VReg_Hook
{
    void** ppTarget;
    void* pBody;
} VReg_Hook;

/// @ref VRegNeutralizeHooks in the calling convention @ref atexit takes, which
/// on x86 is not `WINAPI`.
static void __cdecl VReg_NeutralizeHooksAtExit(void)
{
    VRegNeutralizeHooks();
}

VREG_DECL LONG WINAPI VRegAttachHooks(_In_ VReg* const pRegistry, _In_ VReg* const pNative)
{
    static volatile LONG s_isAttached;
    VRegNativeFunctions* functions;
    VReg_Hook hooks[VREG_INTERNAL_THUNK_CAPACITY];
    void* thunks[VREG_INTERNAL_THUNK_CAPACITY];
    SIZE_T cHooks = 0;
    SIZE_T i;
    DWORD dwOld = 0;

    if (InterlockedExchange(&s_isAttached, 1))
        return ERROR_ALREADY_INITIALIZED;

    WritePointerRelease((PVOID volatile*)&g_pRegistry, pRegistry);
    functions = VRegGetNativeFunctions(pNative);

#define VREG_EMULATED(name) \
    hooks[cHooks].ppTarget = (void**)&functions->name; \
    hooks[cHooks].pBody = (void*)&VReg_Hook##name; \
    ++cHooks;
    VREG_INTERNAL_REGISTRY_VIRTUALS(VREG_EMULATED)
#undef VREG_EMULATED

    // A form this system does not export has nothing to hook.
#define VREG_FORM(name) \
    g_formTargets[VREG_INTERNAL_FORM_##name] = VReg_FindRegistryExport(#name); \
    if (g_formTargets[VREG_INTERNAL_FORM_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_formTargets[VREG_INTERNAL_FORM_##name]; \
        hooks[cHooks].pBody = (void*)&VReg_Hook##name; \
        ++cHooks; \
    }
    VREG_INTERNAL_REGISTRY_FORMS(VREG_FORM)
#undef VREG_FORM

    // A refused function this system does not export has nothing to hook.
#define VREG_REFUSED(name) \
    g_refusedTargets[VREG_INTERNAL_REFUSED_##name] = VReg_FindRegistryExport(#name); \
    if (g_refusedTargets[VREG_INTERNAL_REFUSED_##name]) \
    { \
        hooks[cHooks].ppTarget = &g_refusedTargets[VREG_INTERNAL_REFUSED_##name]; \
        hooks[cHooks].pBody = (void*)&VReg_Refuse##name; \
        ++cHooks; \
    }
    VREG_INTERNAL_REFUSED_HOOKS(VREG_REFUSED)
#undef VREG_REFUSED

    g_pbThunks =
        (BYTE*)VirtualAlloc(NULL, 2 * VREG_INTERNAL_THUNK_PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_pbThunks)
    {
        InterlockedExchange(&s_isAttached, 0);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    for (i = 0; i < cHooks; ++i)
    {
        g_ppTargets[i] = hooks[i].ppTarget;
        thunks[i] = VReg_WriteThunk(i, hooks[i].pBody);
    }

    // The code page becomes executable and read-only.
    if (!VirtualProtect(g_pbThunks, VREG_INTERNAL_THUNK_PAGE_SIZE, PAGE_EXECUTE_READ, &dwOld))
    {
        InterlockedExchange(&s_isAttached, 0);
        return (LONG)GetLastError();
    }
    FlushInstructionCache(GetCurrentProcess(), g_pbThunks, VREG_INTERNAL_THUNK_PAGE_SIZE);

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
    atexit(VReg_NeutralizeHooksAtExit);
    return NO_ERROR;
}

VREG_DECL LONG WINAPI VRegInstallHooks(_In_ VReg* const pRegistry, _In_ VReg* const pNative)
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

VREG_DECL void WINAPI VRegNeutralizeHooks(void)
{
    // Whatever the target field holds is what lies below this layer: the
    // trampoline once committed, the function itself before.
    const LONG cHooks = ReadAcquire(&g_cHooks);
    LONG i;
    for (i = 0; i < cHooks; ++i)
        InterlockedExchangePointer(VReg_ThunkSlot((SIZE_T)i), *g_ppTargets[i]);
}

#undef VREG_INTERNAL_REFUSED_HOOKS

#endif // VREG_INTERNAL_HOOKS_IMPLEMENTATION_INCLUDED
#endif // VREG_INTERNAL_HOOKS_IMPLEMENTATION
#endif // VREG_WITH_DETOURS
