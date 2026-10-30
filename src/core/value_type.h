#pragma once

#include <Windows.h>

namespace VReg
{
/// Whether data of @p dwType is UTF-16 text: @ref REG_SZ, @ref REG_EXPAND_SZ
/// or @ref REG_MULTI_SZ.
[[nodiscard]] inline bool IsStringType(const DWORD dwType) noexcept
{
    return dwType == REG_SZ
        || dwType == REG_EXPAND_SZ
        || dwType == REG_MULTI_SZ;
}
} // namespace VReg
