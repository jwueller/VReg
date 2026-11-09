#pragma once

// A custom hive whose slots forward through the flat functions to a key of
// another registry: a provider that answers whatever that key answers, so a
// scenario run against it must answer as one run against the key directly.

#include "oracle.h"
#include "owned_values.h"

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>

namespace VRegTest
{
/// The path of @p pKey below its hive's root, components joined with
/// backslashes; empty for the root.
inline std::wstring HiveKeyPath(VRegHiveKey* pKey)
{
    std::wstring path;
    for (; VRegHiveKey_GetParent(pKey); pKey = VRegHiveKey_GetParent(pKey))
        path = path.empty() ? std::wstring(VRegHiveKey_GetName(pKey))
                            : std::wstring(VRegHiveKey_GetName(pKey)) + L"\\" + path;
    return path;
}

class ForwardingProvider
{
public:
    /// Forwards to @p hTargetRoot of @p pTarget, which MUST outlive the hive.
    /// With @p isDeleteTreeDerived the table has no `DeleteTree`, so VReg
    /// derives it from the other slots.
    ForwardingProvider(VReg* const pTarget, const HKEY hTargetRoot, const bool isDeleteTreeDerived = false) :
        m_pTarget(pTarget),
        m_hTargetRoot(hTargetRoot)
    {
        const VRegHiveVtbl* const pVtbl = isDeleteTreeDerived ? &VTBL_WITHOUT_DELETE_TREE : &VTBL;
        REQUIRE(VRegHive_CreateCustom(pVtbl, this, m_hive.Receive()) == ERROR_SUCCESS);
    }

    ForwardingProvider(const ForwardingProvider&) = delete;
    ForwardingProvider& operator=(const ForwardingProvider&) = delete;

    [[nodiscard]] VRegHive* Hive() const noexcept
    {
        return m_hive;
    }

    /// Makes `DeleteKey` answer @ref ERROR_ACCESS_DENIED for the key at
    /// @p path below the root; empty lifts it.
    void RefuseDeleteOf(std::wstring path)
    {
        m_undeletable = std::move(path);
    }

    /// The parent record the last `OpenKey` or `CreateKey` received.
    [[nodiscard]] VRegHiveKey* LastParent() const noexcept
    {
        return m_pLastParent;
    }

private:
    static ForwardingProvider& Of(VRegHive* const pHive) noexcept
    {
        return *static_cast<ForwardingProvider*>(VRegHive_GetContext(pHive));
    }

    static std::wstring Join(const std::wstring& path, const LPCWSTR pszSubKey)
    {
        if (!pszSubKey || !*pszSubKey)
            return path;
        return path.empty() ? std::wstring(pszSubKey) : path + L"\\" + pszSubKey;
    }

    LSTATUS Open(const std::wstring& path, const REGSAM samDesired, HKEY& hKey) const noexcept
    {
        return VReg_OpenKeyExW(
            m_pTarget,
            m_hTargetRoot,
            path.empty() ? nullptr : path.c_str(),
            0,
            samDesired,
            &hKey
        );
    }

    void Close(const HKEY hKey) const noexcept
    {
        VReg_CloseKey(
            m_pTarget,
            hKey
        );
    }

    static LSTATUS WINAPI OpenKey(
        VRegHive* const pHive,
        VRegHiveKey* const pParent,
        const LPCWSTR pszSubKey,
        void** const ppToken
    )
    {
        ForwardingProvider& self = Of(pHive);
        self.m_pLastParent = pParent;
        *ppToken = nullptr;
        HKEY hKey{};
        const LSTATUS status = self.Open(Join(HiveKeyPath(pParent), pszSubKey), KEY_QUERY_VALUE, hKey);
        if (status == ERROR_SUCCESS)
            self.Close(hKey);
        return status;
    }

    static LSTATUS WINAPI CreateKey(
        VRegHive* const pHive,
        VRegHiveKey* const pParent,
        const LPCWSTR pszSubKey,
        const LPCWSTR pszClassName,
        const DWORD dwOptions,
        void** const ppToken,
        BOOL* const pIsCreated
    )
    {
        ForwardingProvider& self = Of(pHive);
        self.m_pLastParent = pParent;
        const std::wstring path = Join(HiveKeyPath(pParent), pszSubKey);
        *ppToken = nullptr;
        HKEY hKey{};
        DWORD dwDisposition = 0;
        const LSTATUS status = VReg_CreateKeyExW(
            self.m_pTarget,
            self.m_hTargetRoot,
            path.c_str(),
            0,
            const_cast<LPWSTR>(pszClassName),
            dwOptions,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            &dwDisposition
        );
        if (status != ERROR_SUCCESS)
            return status;

        *pIsCreated = dwDisposition == REG_CREATED_NEW_KEY;
        self.Close(hKey);
        return ERROR_SUCCESS;
    }

    static LSTATUS WINAPI DeleteKey(VRegHive* const pHive, VRegHiveKey* const pKey)
    {
        const ForwardingProvider& self = Of(pHive);
        if (!self.m_undeletable.empty() && VRegHiveKey_EqualsPath(pKey, self.m_undeletable.c_str()))
            return ERROR_ACCESS_DENIED;
        return VReg_DeleteKeyExW(
            self.m_pTarget,
            self.m_hTargetRoot,
            HiveKeyPath(pKey).c_str(),
            0,
            0
        );
    }

    static LSTATUS WINAPI DeleteTree(VRegHive* const pHive, VRegHiveKey* const pKey)
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_ALL_ACCESS, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        status = VReg_DeleteTreeW(
            self.m_pTarget,
            hKey,
            nullptr
        );
        self.Close(hKey);
        return status;
    }

    static LSTATUS WINAPI QueryValue(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const LPCWSTR pszName,
        VRegHiveResult* const pResult
    )
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_QUERY_VALUE, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        DWORD dwType = REG_NONE;
        std::vector<std::byte> data;
        status = VRegTest::QueryValue(self.m_pTarget, hKey, pszName, &dwType, data);
        self.Close(hKey);
        if (status != ERROR_SUCCESS)
            return status;

        return VRegHiveResult_SetValue(
            pResult,
            dwType,
            data.empty() ? nullptr : reinterpret_cast<const BYTE*>(data.data()),
            static_cast<DWORD>(data.size())
        );
    }

    static LSTATUS WINAPI SetValue(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const LPCWSTR pszName,
        const DWORD dwType,
        const BYTE* const pData,
        const DWORD cbData
    )
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_SET_VALUE, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        status = VReg_SetValueExW(
            self.m_pTarget,
            hKey,
            pszName,
            0,
            dwType,
            pData,
            cbData
        );
        self.Close(hKey);
        return status;
    }

    static LSTATUS WINAPI DeleteValue(VRegHive* const pHive, VRegHiveKey* const pKey, const LPCWSTR pszName)
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_SET_VALUE, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        status = VReg_DeleteValueW(
            self.m_pTarget,
            hKey,
            pszName
        );
        self.Close(hKey);
        return status;
    }

    static LSTATUS WINAPI EnumKey(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const DWORD dwIndex,
        VRegHiveResult* const pResult
    )
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_READ, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        DWORD cchMaxClass = 0;
        status = VReg_QueryInfoKeyW(
            self.m_pTarget,
            hKey,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &cchMaxClass,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        );
        std::array<wchar_t, 256> name{};
        std::vector<wchar_t> className(cchMaxClass + 1);
        DWORD cchName = static_cast<DWORD>(name.size());
        DWORD cchClass = static_cast<DWORD>(className.size());
        FILETIME lastWrite{};
        if (status == ERROR_SUCCESS)
            status = VReg_EnumKeyExW(
                self.m_pTarget,
                hKey,
                dwIndex,
                name.data(),
                &cchName,
                nullptr,
                className.data(),
                &cchClass,
                &lastWrite
            );
        self.Close(hKey);
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetName(pResult, name.data());
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetClass(pResult, className.data());
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetLastWrite(pResult, &lastWrite);
        return status;
    }

    static LSTATUS WINAPI EnumValue(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const DWORD dwIndex,
        VRegHiveResult* const pResult
    )
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_QUERY_VALUE, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        std::wstring name;
        DWORD dwType = REG_NONE;
        std::vector<std::byte> data;
        status = VRegTest::EnumValue(self.m_pTarget, hKey, dwIndex, name, &dwType, &data);
        self.Close(hKey);
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetName(pResult, name.c_str());
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetValue(
                pResult,
                dwType,
                data.empty() ? nullptr : reinterpret_cast<const BYTE*>(data.data()),
                static_cast<DWORD>(data.size())
            );
        return status;
    }

    static LSTATUS WINAPI QueryInfo(VRegHive* const pHive, VRegHiveKey* const pKey, VRegHiveResult* const pResult)
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_QUERY_VALUE, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        std::vector<wchar_t> className(256);
        DWORD cchClass = 0;
        DWORD cSubKeys = 0;
        DWORD cchMaxSubKey = 0;
        DWORD cchMaxClass = 0;
        DWORD cValues = 0;
        DWORD cchMaxValueName = 0;
        DWORD cbMaxValue = 0;
        FILETIME lastWrite{};
        for (;;)
        {
            cchClass = static_cast<DWORD>(className.size());
            status = VReg_QueryInfoKeyW(
                self.m_pTarget,
                hKey,
                className.data(),
                &cchClass,
                nullptr,
                &cSubKeys,
                &cchMaxSubKey,
                &cchMaxClass,
                &cValues,
                &cchMaxValueName,
                &cbMaxValue,
                nullptr,
                &lastWrite
            );
            if (status != ERROR_MORE_DATA)
                break;
            className.resize(className.size() * 2);
        }
        self.Close(hKey);
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetClass(pResult, className.data());
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetLastWrite(pResult, &lastWrite);
        if (status == ERROR_SUCCESS)
            status = VRegHiveResult_SetInfo(
                pResult,
                cSubKeys,
                cchMaxSubKey,
                cchMaxClass,
                cValues,
                cchMaxValueName,
                cbMaxValue
            );
        return status;
    }

    static LSTATUS WINAPI CheckKey(VRegHive* const pHive, VRegHiveKey* const pKey)
    {
        const ForwardingProvider& self = Of(pHive);
        HKEY hKey{};
        LSTATUS status = self.Open(HiveKeyPath(pKey), KEY_QUERY_VALUE, hKey);
        if (status != ERROR_SUCCESS)
            return status;

        status = VReg_FlushKey(
            self.m_pTarget,
            hKey
        );
        self.Close(hKey);
        return status;
    }

    static constexpr VRegHiveVtbl VTBL{
        .OpenKey = &OpenKey,
        .CreateKey = &CreateKey,
        .DeleteKey = &DeleteKey,
        .DeleteTree = &DeleteTree,
        .QueryValue = &QueryValue,
        .SetValue = &SetValue,
        .DeleteValue = &DeleteValue,
        .EnumKey = &EnumKey,
        .EnumValue = &EnumValue,
        .QueryInfo = &QueryInfo,
        .CheckKey = &CheckKey,
        .Destroy = nullptr,
    };

    static constexpr VRegHiveVtbl WithoutDeleteTree(VRegHiveVtbl vtbl) noexcept
    {
        vtbl.DeleteTree = nullptr;
        return vtbl;
    }

    // Defined after the class: a constexpr member cannot call a member
    // function of the still incomplete class.
    static const VRegHiveVtbl VTBL_WITHOUT_DELETE_TREE;

    VReg* m_pTarget;
    HKEY m_hTargetRoot;
    std::wstring m_undeletable;
    VRegHiveKey* m_pLastParent{};
    OwnedHive m_hive;
};

inline const VRegHiveVtbl ForwardingProvider::VTBL_WITHOUT_DELETE_TREE = WithoutDeleteTree(VTBL);

/// A @ref ForwardingProvider over an in-memory hive, mounted below
/// `HKEY_CURRENT_USER` in a virtual registry of its own: the custom
/// counterpart of a @ref VirtualHive.
class CustomMount
{
public:
    explicit CustomMount(const bool isDeleteTreeDerived = false) :
        m_target(MakeHive()),
        m_targetRegistry(MakeVirtualRegistry()),
        m_provider(m_targetRegistry, RootHandle(m_target), isDeleteTreeDerived),
        m_registry(MakeVirtualRegistry())
    {
        REQUIRE(VReg_Mount(m_targetRegistry, HKEY_CURRENT_USER, L"VRegTarget", m_target, 0) == ERROR_SUCCESS);
        REQUIRE(VReg_Mount(m_registry, HKEY_CURRENT_USER, L"VRegOracle", m_provider.Hive(), 0) == ERROR_SUCCESS);
    }

    CustomMount(const CustomMount&) = delete;
    CustomMount& operator=(const CustomMount&) = delete;

    [[nodiscard]] VReg* Registry() noexcept
    {
        return m_registry;
    }

    [[nodiscard]] Api AsApi() noexcept
    {
        return {.pRegistry = m_registry, .isVirtualKeys = true};
    }

    [[nodiscard]] VRegHive* Hive() const noexcept
    {
        return m_provider.Hive();
    }

    [[nodiscard]] ForwardingProvider& Provider() noexcept
    {
        return m_provider;
    }

    /// The virtual handle of the custom hive's root.
    [[nodiscard]] HKEY Root() const noexcept
    {
        return RootHandle(m_provider.Hive());
    }

private:
    OwnedHive m_target;
    OwnedRegistry m_targetRegistry;
    ForwardingProvider m_provider;
    OwnedRegistry m_registry;
};

/// Runs @p Scenario against a custom hive that forwards to an in-memory one.
template <class Scenario>
Recording RecordCustom()
{
    CustomMount mount;
    Recorder recorder;
    Scenario{}(mount.AsApi(), mount.Root(), recorder);
    return recorder.Take();
}
} // namespace VRegTest
