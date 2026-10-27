#pragma once

#include <VReg/detours.h>
#include <VReg/memory_hive.h>
#include <VReg/native_registry.h>
#include <VReg/virtual_registry.h>

#include <Windows.h>

#include <cstddef>
#include <span>
#include <string_view>

namespace VReg::Test
{
/// The name of the value that tells the hive of one module from another's.
inline constexpr wchar_t kModuleValue[] = L"Module";

/// Mounts a hive whose @ref kModuleValue is @p pszModule at
/// `HKEY_CURRENT_USER\pszPath` in a registry over the real one, and installs
/// the hooks over it. The objects are leaked: the hooks outlive the caller.
inline LONG InstallMount(
    const wchar_t* const pszPath,
    const wchar_t* const pszModule
)
{
    auto* const pHive = new MemoryHive;
    auto* const pNative = new NativeRegistry;
    auto* const pRegistry = new VirtualRegistry(pNative);

    const std::wstring_view module = pszModule;
    const auto bytes = std::as_bytes(std::span(pszModule, module.size() + 1));
    const auto stored = pHive->SetValue(
        pHive->Root(),
        kModuleValue,
        REG_SZ,
        bytes
    );
    if (stored != ERROR_SUCCESS)
        return stored;

    const auto error = pRegistry->Mount(HKEY_CURRENT_USER, pszPath, *pHive);
    if (error != ERROR_SUCCESS)
        return error;

    return Detours::Install(*pRegistry, *pNative);
}
} // namespace VReg::Test
