#pragma once

#include "registry.h"

#include <Windows.h>

#include <cstddef>
#include <string>
#include <vector>

/**
 * @file
 * @brief Reads over any `Registry` that size and fill a `std::wstring` or
 *  `std::vector<std::byte>` themselves, retrying with a larger buffer while
 *  the value grows between the sizing call and the read.
 *
 *  They have no Win32 counterpart, so they stay out of `Registry` and carry
 *  no `Reg` prefix (which would also collide with the Win32 macros
 *  `RegQueryValue`, `RegEnumKey`, ...).
 */
namespace VReg
{
LSTATUS QueryValue(
    const Registry& registry,
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpType,
    std::vector<std::byte>& data
) noexcept;

LSTATUS GetValue(
    const Registry& registry,
    HKEY hKey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    std::vector<std::byte>& data
) noexcept;

/// A string value without its terminator. @p dwFlags takes `RRF_*` flags
/// except the type restriction, which this adds: `REG_SZ`, and with
/// `RRF_NOEXPAND` also `REG_EXPAND_SZ` (otherwise `RegGetValueW` expands it
/// into a `REG_SZ`).
LSTATUS GetString(
    const Registry& registry,
    HKEY hKey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    std::wstring& text
) noexcept;

LSTATUS EnumKey(
    const Registry& registry,
    HKEY hKey,
    DWORD dwIndex,
    std::wstring& name
) noexcept;

LSTATUS EnumValue(
    const Registry& registry,
    HKEY hKey,
    DWORD dwIndex,
    std::wstring& name,
    LPDWORD lpType,
    std::vector<std::byte>* pData
) noexcept;

LSTATUS LoadMUIString(
    const Registry& registry,
    HKEY hKey,
    LPCWSTR pszValue,
    DWORD Flags,
    LPCWSTR pszDirectory,
    std::wstring& text
) noexcept;
} // namespace VReg
