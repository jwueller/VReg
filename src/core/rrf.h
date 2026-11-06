#pragma once

#include <Windows.h>

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
