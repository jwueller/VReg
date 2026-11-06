#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Whether data of @p dwType is UTF-16 text: @ref REG_SZ, @ref REG_EXPAND_SZ
/// or @ref REG_MULTI_SZ.
BOOL VReg__IsStringType(DWORD dwType);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace VReg
{
[[nodiscard]] inline bool IsStringType(const DWORD dwType) noexcept
{
    return VReg__IsStringType(dwType) != FALSE;
}
} // namespace VReg

#endif // __cplusplus
