#pragma once

#include <Windows.h>

namespace VReg
{
/// The entry point @p hModule exports as @p pszName, read from its export
/// table; null when @p hModule is null, the name is not exported, or the
/// export forwards to another module.
[[nodiscard]] const void* FindExport(HMODULE hModule, const char* pszName) noexcept;
} // namespace VReg
