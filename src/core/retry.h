#pragma once

#include <Windows.h>

// Names and classes are read into a temporary buffer of 256 characters, which
// holds the longest key name, and grow by doubling on ERROR_MORE_DATA, up to
// the limits below. Avoids an additional sizing call for the common case.
#define VREG__INITIAL_NAME_CHARS ((DWORD)256)
#define VREG__MAX_CLASS_CHARS ((DWORD)32768)
#define VREG__MAX_VALUE_NAME_CHARS ((DWORD)16384)

#ifdef __cplusplus
extern "C" {
#endif

/// The size to retry with: the one the call asked for, or more than before if
/// it asked for no more than it was given.
DWORD VReg__Grown(DWORD cbOld, DWORD cbRequired);

#ifdef __cplusplus
} // extern "C"
#endif
