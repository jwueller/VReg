#pragma once

#include <Windows.h>

namespace VReg
{
/// The entry point @p hModule exports as @p pszName, read from its export
/// table; null when @p hModule is null, the name is not exported, or the
/// export forwards to another module.
[[nodiscard]] void* FindExport(HMODULE hModule, const char* pszName) noexcept;

/// A registry function, from `KernelBase.dll` or else `advapi32.dll`, which
/// alone exports the pre-`Ex` forms; null when neither exports it. Never
/// loads a module.
[[nodiscard]] void* FindRegistryExport(const char* pszName) noexcept;
} // namespace VReg
