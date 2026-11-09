#pragma once

#include "../intercept_registry.h"

#include <vreg.h>

#include <Windows.h>

#include <string_view>

namespace VRegTest
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
    VReg* pNative = nullptr;
    VReg* pRegistry = nullptr;
    LONG error = VRegHive_CreateInMemory(&pHive);
    if (error == ERROR_SUCCESS)
        error = VReg_CreateNative(&pNative);
    if (error == ERROR_SUCCESS)
        error = VReg_CreateVirtual(pNative, &pRegistry);
    if (error == ERROR_SUCCESS)
        error = VReg_Mount(pRegistry, HKEY_CURRENT_USER, pszPath, pHive, 0);
    if (error != ERROR_SUCCESS)
        return error;

    const std::wstring_view module = pszModule;
    error = VReg_RegSetKeyValueW(
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

    // Every module a hook can reach is pinned, as the README requires; here
    // the hooks, the registry and the hive all live in this one.
    HMODULE hModule{};
    const BOOL isPinned = ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&InstallMount),
        &hModule
    );
    if (!isPinned)
        return static_cast<LONG>(::GetLastError());

    if (!ppIntercept)
        return VRegHooks_Install(pRegistry, pNative);

    *ppIntercept = new InterceptRegistry(pRegistry);
    return VRegHooks_Install(**ppIntercept, pNative);
}
} // namespace VRegTest
