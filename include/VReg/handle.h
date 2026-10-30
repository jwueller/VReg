#pragma once

#include <Windows.h>

#include <bit>
#include <cstdint>

namespace VReg
{
/// The highest virtual handle value. Real key handles are kernel handle values
/// (multiples of 4, at most tagged in bit 1 by advapi32), and predefined roots
/// lie above this (sign-extended on x64), so an odd value at or below it is
/// never a real key handle.
///
/// The range stays 31-bit on x64 too, so code that truncates an HKEY to a
/// @ref DWORD keeps a working handle.
inline constexpr std::uintptr_t VREG_VIRTUAL_HKEY_BASE = 0x7FFFFFFF;

/// One more than the highest index @ref VirtualHKeyFromIndex accepts.
inline constexpr std::uintptr_t VREG_VIRTUAL_HKEY_COUNT = (VREG_VIRTUAL_HKEY_BASE >> 1) + 1;

/// The handle for @p index, counting down from @ref VREG_VIRTUAL_HKEY_BASE in odd
/// steps; null when @p index is out of range.
constexpr HKEY VirtualHKeyFromIndex(const std::uintptr_t index) noexcept
{
    if (index >= VREG_VIRTUAL_HKEY_COUNT)
        return nullptr;

    return std::bit_cast<HKEY>(VREG_VIRTUAL_HKEY_BASE - (index << 1));
}

constexpr bool IsVirtualHKey(const HKEY hKey) noexcept
{
    const auto value = std::bit_cast<std::uintptr_t>(hKey);
    return value != 0 && value <= VREG_VIRTUAL_HKEY_BASE && (value & 1) != 0;
}

/// The inverse of @ref VirtualHKeyFromIndex; only meaningful when
/// @ref IsVirtualHKey holds.
constexpr std::uintptr_t IndexFromVirtualHKey(const HKEY hKey) noexcept
{
    return (VREG_VIRTUAL_HKEY_BASE - std::bit_cast<std::uintptr_t>(hKey)) >> 1;
}
} // namespace VReg
