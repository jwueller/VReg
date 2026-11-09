#pragma once

// Owners and factories for the C objects the tests drive: a registry, a hive or
// a script is created through the C functions, freed when its owner goes, and
// passed to the flat functions as the plain pointer the owner converts to.

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace VRegTest
{
/// Owns a @ref VReg.
class OwnedRegistry
{
public:
    OwnedRegistry() noexcept = default;

    explicit OwnedRegistry(VReg* const pRegistry) noexcept :
        m_pRegistry(pRegistry)
    {
    }

    ~OwnedRegistry()
    {
        VRegDestroy(m_pRegistry);
    }

    OwnedRegistry(OwnedRegistry&& other) noexcept :
        m_pRegistry(std::exchange(other.m_pRegistry, nullptr))
    {
    }

    OwnedRegistry& operator=(OwnedRegistry&& other) noexcept
    {
        if (this != &other)
        {
            VRegDestroy(m_pRegistry);
            m_pRegistry = std::exchange(other.m_pRegistry, nullptr);
        }
        return *this;
    }

    OwnedRegistry(const OwnedRegistry&) = delete;
    OwnedRegistry& operator=(const OwnedRegistry&) = delete;

    operator VReg*() const noexcept
    {
        return m_pRegistry;
    }

    [[nodiscard]] VReg* Get() const noexcept
    {
        return m_pRegistry;
    }

    /// Where a creation function stores the registry this then owns.
    [[nodiscard]] VReg** Receive() noexcept
    {
        VRegDestroy(m_pRegistry);
        m_pRegistry = nullptr;
        return &m_pRegistry;
    }

private:
    VReg* m_pRegistry = nullptr;
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

/// A registry over the slots of @p pVtbl, with @p pvContext as their object.
inline OwnedRegistry MakeCustomRegistry(
    const VRegVtbl* const pVtbl,
    void* const pvContext
)
{
    OwnedRegistry registry;
    REQUIRE(VRegCreateCustom(pVtbl, pvContext, registry.Receive()) == ERROR_SUCCESS);
    return registry;
}

inline OwnedRegistry MakeNativeRegistry()
{
    OwnedRegistry registry;
    REQUIRE(VRegCreateNative(registry.Receive()) == ERROR_SUCCESS);
    return registry;
}

/// A virtual registry over @p pBase; @p pfnNtQueryKey, when given, replaces
/// the `ntdll` export it resolved.
inline OwnedRegistry MakeVirtualRegistry(
    VReg* const pBase = nullptr,
    const VReg_NtQueryKeyFunction pfnNtQueryKey = nullptr
)
{
    OwnedRegistry registry;
    REQUIRE(VRegCreateVirtual(pBase, registry.Receive()) == ERROR_SUCCESS);
    if (pfnNtQueryKey)
        registry.Get()->implementation.virtualRegistry.pfnNtQueryKey = pfnNtQueryKey;
    return registry;
}

/// A hive held entirely in memory.
inline OwnedHive MakeHive()
{
    OwnedHive hive;
    REQUIRE(VRegCreateHive(hive.Receive()) == ERROR_SUCCESS);
    return hive;
}

inline OwnedHive MakeLayeredHive(
    VReg* const pLower,
    const HKEY hLowerRoot,
    const wchar_t* const pszLowerPath,
    const REGSAM samLowerView = 0
)
{
    OwnedHive hive;
    const auto status = VRegCreateLayeredHive(pLower, hLowerRoot, pszLowerPath, samLowerView, hive.Receive());
    REQUIRE(status == ERROR_SUCCESS);
    return hive;
}

/// The virtual handle of a hive's root.
inline HKEY RootHandle(VRegHive* const pHive) noexcept
{
    return VReg_NodeHandle(VReg_HiveRoot(pHive));
}

/// A registry a scenario runs against, with what the scenario has to know
/// about it.
struct Api
{
    VReg* pRegistry;

    /// Whether the registry's keys are virtual: one handle per node, no
    /// per-open access rights. Scenarios skip the calls whose answer virtual
    /// keys cannot match, for the reference and the virtual registry alike.
    bool isVirtualKeys;

    operator VReg*() const noexcept
    {
        return pRegistry;
    }
};
} // namespace VRegTest
