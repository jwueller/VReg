#pragma once

#include "../intercept_registry.h"

#include <vreg.h>

#include <Windows.h>

#include <string_view>

namespace VReg::Test
{
/// The name of the value that tells the hive of one module from another's.
inline constexpr wchar_t MODULE_VALUE[] = L"Module";

/// Mounts a hive whose @ref MODULE_VALUE is @p pszModule at
/// `HKEY_CURRENT_USER\pszPath` in a registry over the real one, and installs
/// the hooks over it; through an `InterceptRegistry` in front of it when
/// @p ppIntercept asks for one. The objects are leaked: the hooks outlive the
/// caller.
inline LONG InstallMount(
    const wchar_t* const pszPath,
    const wchar_t* const pszModule,
    InterceptRegistry** const ppIntercept = nullptr
)
{
    VRegHive* pHive = nullptr;
    VRegRegistry* pNative = nullptr;
    VRegRegistry* pRegistry = nullptr;
    LONG error = VRegCreateHive(nullptr, &pHive);
    if (error == ERROR_SUCCESS)
        error = VRegCreateNativeRegistry(nullptr, nullptr, &pNative);
    if (error == ERROR_SUCCESS)
        error = VRegCreateVirtualRegistry(pNative, nullptr, nullptr, &pRegistry);
    if (error == ERROR_SUCCESS)
        error = VRegMount(pRegistry, HKEY_CURRENT_USER, pszPath, pHive, 0);
    if (error != ERROR_SUCCESS)
        return error;

    const std::wstring_view module = pszModule;
    error = VRegSetKeyValueW(
        pRegistry,
        HKEY_CURRENT_USER,
        pszPath,
        MODULE_VALUE,
        REG_SZ,
        pszModule,
        static_cast<DWORD>((module.size() + 1) * sizeof(wchar_t))
    );
    if (error != ERROR_SUCCESS)
        return error;

    if (!ppIntercept)
        return VRegInstallHooks(pRegistry, pNative);

    *ppIntercept = new InterceptRegistry(pRegistry);
    return VRegInstallHooks(**ppIntercept, pNative);
}
} // namespace VReg::Test
