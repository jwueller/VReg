#pragma once

#include <Windows.h>

#include <new>
#include <stdexcept>

namespace VReg
{
// Agent's choice: names and classes are read into a buffer of 256 characters, which holds the longest key name,
// and grow by doubling on ERROR_MORE_DATA up to the limits below. Sizing each call with a query first was passed
// over as a second round trip for what is almost always one.
inline constexpr DWORD INITIAL_NAME_CHARS = 256;
inline constexpr DWORD MAX_CLASS_CHARS = 32768;
inline constexpr DWORD MAX_VALUE_NAME_CHARS = 16384;

template <class Function>
LSTATUS Guard(Function&& function) noexcept
{
    try
    {
        return function();
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
    catch (const std::length_error&)
    {
        return ERROR_OUTOFMEMORY;
    }
}

/// The size to retry with: the one the call asked for, or more than before if
/// it asked for no more than it was given.
inline DWORD Grown(const DWORD cbOld, const DWORD cbRequired) noexcept
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}
} // namespace VReg
