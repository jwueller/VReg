#pragma once

#include <VReg/api.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The version of the copy of VReg linked into the calling module, as text
/// for display and logs. Its format is unspecified and may change; compare
/// versions with @ref VRegGetFileVersion.
VREGDEF _Ret_z_ LPCSTR WINAPI VRegGetVersion(void);

/// The version of the copy of VReg linked into the calling module, which can
/// differ from the copies other modules in the same process link, packed as
/// @ref MAKEDLLVERULL packs a four-part version: major, minor, patch and
/// revision from the high word down, the value a version resource's
/// `dwFileVersionMS:dwFileVersionLS` reads as, so versions compare as
/// numbers. The revision counts the commits in the history built from, and
/// is 0 for a build outside a git checkout. At build time, `find_package(VReg)`
/// reports the same major, minor and patch.
VREGDEF ULONGLONG WINAPI VRegGetFileVersion(void);

#ifdef __cplusplus
} // extern "C"
#endif
