#pragma once

#include "registry.h"

#include <Windows.h>

#include <cstddef>
#include <string>
#include <vector>

/**
 * @file
 * @brief Reads over any @ref VReg::Registry that size and fill a
 *  @ref std::wstring or `std::vector<std::byte>` themselves, retrying with a
 *  larger buffer while the value grows between the sizing call and the read.
 *
 *  They have no Win32 counterpart, so they stay out of @ref VReg::Registry
 *  and carry no `Reg` prefix (which would also collide with the Win32 macros
 *  @ref RegQueryValue, @ref RegEnumKey, ...).
 */
namespace VReg
{
LSTATUS QueryValue(
    const Registry& registry,
    HKEY hKey,
    _In_opt_z_ LPCWSTR lpValueName,
    _Out_opt_ LPDWORD lpType,
    std::vector<std::byte>& data
) noexcept;

LSTATUS GetValue(
    const Registry& registry,
    HKEY hKey,
    _In_opt_z_ LPCWSTR lpSubKey,
    _In_opt_z_ LPCWSTR lpValue,
    DWORD dwFlags,
    _Out_opt_ LPDWORD pdwType,
    std::vector<std::byte>& data
) noexcept;

/// A string value without its terminator. @p dwFlags takes `RRF_*` flags
/// except the type restriction, which this adds: @ref REG_SZ, and with
/// @ref RRF_NOEXPAND also @ref REG_EXPAND_SZ (otherwise
/// @ref Registry::RegGetValueW expands it into a @ref REG_SZ).
LSTATUS GetString(
    const Registry& registry,
    HKEY hKey,
    _In_opt_z_ LPCWSTR lpSubKey,
    _In_opt_z_ LPCWSTR lpValue,
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
    _Out_opt_ LPDWORD lpType,
    _Out_opt_ std::vector<std::byte>* pData
) noexcept;

LSTATUS LoadMUIString(
    const Registry& registry,
    HKEY hKey,
    _In_opt_z_ LPCWSTR pszValue,
    DWORD Flags,
    _In_opt_z_ LPCWSTR pszDirectory,
    std::wstring& text
) noexcept;
} // namespace VReg
