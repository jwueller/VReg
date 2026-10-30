#pragma once

#include <Windows.h>

#include <bit>

namespace VReg
{
/// The highest virtual handle value. Real key handles are kernel handle values
/// (multiples of 4, at most tagged in bit 1 by advapi32), and predefined roots
/// lie above this (sign-extended on x64), so an odd value at or below it is
/// never a real key handle.
///
/// The range stays 31-bit on x64 too, so code that truncates an HKEY to a
/// @ref DWORD keeps a working handle.
inline constexpr UINT_PTR VREG_VIRTUAL_HKEY_BASE = 0x7FFFFFFF;

/// One more than the highest index @ref VirtualHKeyFromIndex accepts.
inline constexpr UINT_PTR VREG_VIRTUAL_HKEY_COUNT =
    (VREG_VIRTUAL_HKEY_BASE >> 1)
    + 1;

/// The handle for @p index, counting down from @ref VREG_VIRTUAL_HKEY_BASE in
/// odd steps; null when @p index is out of range.
constexpr HKEY VirtualHKeyFromIndex(const UINT_PTR index) noexcept
{
    if (index >= VREG_VIRTUAL_HKEY_COUNT)
        return nullptr;

    return std::bit_cast<HKEY>(VREG_VIRTUAL_HKEY_BASE - (index << 1));
}

constexpr bool IsVirtualHKey(const HKEY hKey) noexcept
{
    const auto value = std::bit_cast<UINT_PTR>(hKey);
    return value != 0 && value <= VREG_VIRTUAL_HKEY_BASE && (value & 1) != 0;
}

/// The inverse of @ref VirtualHKeyFromIndex; only meaningful when
/// @ref IsVirtualHKey holds.
constexpr UINT_PTR IndexFromVirtualHKey(const HKEY hKey) noexcept
{
    return (VREG_VIRTUAL_HKEY_BASE - std::bit_cast<UINT_PTR>(hKey)) >> 1;
}
} // namespace VReg
