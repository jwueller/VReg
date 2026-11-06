#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The entry point @p hModule exports as @p pszName, read from its export
/// table; null when @p hModule is null, the name is not exported, or the
/// export forwards to another module.
_Ret_maybenull_ void* VReg__FindExport(_In_opt_ HMODULE hModule, _In_z_ const char* pszName);

/// A registry function, from `KernelBase.dll` or else `advapi32.dll`, which
/// alone exports the pre-`Ex` forms; null when neither exports it. Never
/// loads a module.
_Ret_maybenull_ void* VReg__FindRegistryExport(_In_z_ const char* pszName);

#ifdef __cplusplus
} // extern "C"
#endif
