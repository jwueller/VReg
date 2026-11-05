#pragma once

#include <Windows.h>

namespace VReg
{
/// @ref RRF_SUBKEY_WOW6464KEY and @ref RRF_SUBKEY_WOW6432KEY under names of
/// their own: the SDK defines them only when targeting Windows 10, and VReg
/// honors them whatever its consumer targets. The values are fixed by
/// @ref RegGetValueW.
inline constexpr DWORD RRF_WOW6464_SUBKEY = 0x00010000;
inline constexpr DWORD RRF_WOW6432_SUBKEY = 0x00020000;

#ifdef RRF_SUBKEY_WOW6464KEY
static_assert(RRF_WOW6464_SUBKEY == RRF_SUBKEY_WOW6464KEY);
#endif
#ifdef RRF_SUBKEY_WOW6432KEY
static_assert(RRF_WOW6432_SUBKEY == RRF_SUBKEY_WOW6432KEY);
#endif
} // namespace VReg
