#pragma once

// Owners and factories for the C objects the tests drive: a registry, a hive or
// a script is created through the C functions, freed when its owner goes, and
// passed to the flat functions as the plain pointer the owner converts to.

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace VReg::Test
{
/// Owns a @ref VRegRegistry.
class OwnedRegistry
{
public:
    OwnedRegistry() noexcept = default;

    explicit OwnedRegistry(VRegRegistry* const pRegistry) noexcept :
        m_pRegistry(pRegistry)
    {
    }

    ~OwnedRegistry()
    {
        VRegDestroyRegistry(m_pRegistry);
    }

    OwnedRegistry(OwnedRegistry&& other) noexcept :
        m_pRegistry(std::exchange(other.m_pRegistry, nullptr))
    {
    }

    OwnedRegistry& operator=(OwnedRegistry&& other) noexcept
    {
        if (this != &other)
        {
            VRegDestroyRegistry(m_pRegistry);
            m_pRegistry = std::exchange(other.m_pRegistry, nullptr);
        }
        return *this;
    }

    OwnedRegistry(const OwnedRegistry&) = delete;
    OwnedRegistry& operator=(const OwnedRegistry&) = delete;

    operator VRegRegistry*() const noexcept
    {
        return m_pRegistry;
    }

    [[nodiscard]] VRegRegistry* Get() const noexcept
    {
        return m_pRegistry;
    }

    /// Where a creation function stores the registry this then owns.
    [[nodiscard]] VRegRegistry** Receive() noexcept
    {
        VRegDestroyRegistry(m_pRegistry);
        m_pRegistry = nullptr;
        return &m_pRegistry;
    }

private:
    VRegRegistry* m_pRegistry = nullptr;
};

/// Owns a @ref VRegHive.
class OwnedHive
{
public:
    OwnedHive() noexcept = default;

    ~OwnedHive()
    {
        VRegDestroyHive(m_pHive);
    }

    OwnedHive(OwnedHive&& other) noexcept :
        m_pHive(std::exchange(other.m_pHive, nullptr))
    {
    }

    OwnedHive& operator=(OwnedHive&& other) noexcept
    {
        if (this != &other)
        {
            VRegDestroyHive(m_pHive);
            m_pHive = std::exchange(other.m_pHive, nullptr);
        }
        return *this;
    }

    OwnedHive(const OwnedHive&) = delete;
    OwnedHive& operator=(const OwnedHive&) = delete;

    operator VRegHive*() const noexcept
    {
        return m_pHive;
    }

    [[nodiscard]] VRegHive* Get() const noexcept
    {
        return m_pHive;
    }

    [[nodiscard]] VRegHive** Receive() noexcept
    {
        VRegDestroyHive(m_pHive);
        m_pHive = nullptr;
        return &m_pHive;
    }

private:
    VRegHive* m_pHive = nullptr;
};

/// Owns a @ref VRegScript.
class OwnedScript
{
public:
    OwnedScript() noexcept = default;

    ~OwnedScript()
    {
        VRegDestroyScript(m_pScript);
    }

    OwnedScript(OwnedScript&& other) noexcept :
        m_pScript(std::exchange(other.m_pScript, nullptr))
    {
    }

    OwnedScript& operator=(OwnedScript&& other) noexcept
    {
        if (this != &other)
        {
            VRegDestroyScript(m_pScript);
            m_pScript = std::exchange(other.m_pScript, nullptr);
        }
        return *this;
    }

    OwnedScript(const OwnedScript&) = delete;
    OwnedScript& operator=(const OwnedScript&) = delete;

    operator VRegScript*() const noexcept
    {
        return m_pScript;
    }

    [[nodiscard]] VRegScript* Get() const noexcept
    {
        return m_pScript;
    }

    [[nodiscard]] VRegScript** Receive() noexcept
    {
        VRegDestroyScript(m_pScript);
        m_pScript = nullptr;
        return &m_pScript;
    }

private:
    VRegScript* m_pScript = nullptr;
};

/// The native registry over @p pFunctions, or the resolved table.
inline OwnedRegistry MakeNativeRegistry(const VRegNativeFunctions* const pFunctions = nullptr)
{
    OwnedRegistry registry;
    REQUIRE(VRegCreateNativeRegistry(pFunctions, nullptr, registry.Receive()) == ERROR_SUCCESS);
    return registry;
}

inline OwnedRegistry MakeVirtualRegistry(
    VRegRegistry* const pBase = nullptr,
    const VRegNtQueryKeyFunction pfnNtQueryKey = nullptr
)
{
    OwnedRegistry registry;
    REQUIRE(VRegCreateVirtualRegistry(pBase, pfnNtQueryKey, nullptr, registry.Receive()) == ERROR_SUCCESS);
    return registry;
}

/// A hive held entirely in memory.
inline OwnedHive MakeHive()
{
    OwnedHive hive;
    REQUIRE(VRegCreateHive(nullptr, hive.Receive()) == ERROR_SUCCESS);
    return hive;
}

inline OwnedHive MakeLayeredHive(
    VRegRegistry* const pLower,
    const HKEY hLowerRoot,
    const wchar_t* const pszLowerPath,
    const REGSAM samLowerView = 0
)
{
    OwnedHive hive;
    const auto status = VRegCreateLayeredHive(pLower, hLowerRoot, pszLowerPath, samLowerView, nullptr, hive.Receive());
    REQUIRE(status == ERROR_SUCCESS);
    return hive;
}

/// The virtual handle of a hive's root.
inline HKEY RootHandle(VRegHive* const pHive) noexcept
{
    return VReg__NodeHandle(VReg__HiveRoot(pHive));
}

/// A registry a scenario runs against, with what the scenario has to know
/// about it.
struct Api
{
    VRegRegistry* pRegistry;

    /// Whether the registry's keys are virtual: one handle per node, no
    /// per-open access rights. Scenarios skip the calls whose answer virtual
    /// keys cannot match, for the reference and the virtual registry alike.
    bool isVirtualKeys;

    operator VRegRegistry*() const noexcept
    {
        return pRegistry;
    }
};
} // namespace VReg::Test
