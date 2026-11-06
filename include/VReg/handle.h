#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

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
BOOL WINAPI VRegIsVirtualHKey(_In_opt_ HKEY hKey);

/// Whether @p hKey is a predefined root, @ref HKEY_CLASSES_ROOT through
/// @ref HKEY_CURRENT_USER_LOCAL_SETTINGS: `winreg.h` builds them from a
/// negative @ref LONG, sign-extended on x64, where neither a kernel handle nor
/// a virtual one ever lies.
BOOL WINAPI VRegIsPredefinedHKey(_In_opt_ HKEY hKey);

#ifdef __cplusplus
} // extern "C"
#endif
