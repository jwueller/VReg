#pragma once

#include <Windows.h>

namespace VReg
{
// Names and classes are read into a temporary buffer of 256 characters, which
// holds the longest key name, and grow by doubling on ERROR_MORE_DATA, up to
// the limits below. Avoids an additional sizing call for the common case.
inline constexpr DWORD INITIAL_NAME_CHARS = 256;
inline constexpr DWORD MAX_CLASS_CHARS = 32768;
inline constexpr DWORD MAX_VALUE_NAME_CHARS = 16384;

/// The size to retry with: the one the call asked for, or more than before if
/// it asked for no more than it was given.
inline DWORD Grown(const DWORD cbOld, const DWORD cbRequired) noexcept
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}
} // namespace VReg
