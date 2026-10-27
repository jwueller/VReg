#include <VReg/native_registry.h>

#include "core/export_table.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>

using namespace VReg;

TEST_CASE("Export lookup agrees with the loader")
{
    const auto hKernelBase = GetModuleHandleW(L"KernelBase.dll");
    REQUIRE(hKernelBase);

    for (const auto* const pszName : {"RegOpenKeyExW", "RegCloseKey", "RegQueryInfoKeyA", "CreateFileW"})
    {
        INFO(pszName);
        CHECK(FindExport(hKernelBase, pszName) == reinterpret_cast<const void*>(GetProcAddress(hKernelBase, pszName)));
    }

    CHECK(FindExport(hKernelBase, "NoSuchExport") == nullptr);
    CHECK(FindExport(nullptr, "RegOpenKeyExW") == nullptr);
}

TEST_CASE("Forwarded exports are not resolved")
{
    // advapi32 forwards this one to KernelBase.
    const auto hAdvapi32 = GetModuleHandleW(L"advapi32.dll");
    REQUIRE(hAdvapi32);
    CHECK(GetProcAddress(hAdvapi32, "AddMandatoryAce") != nullptr);
    CHECK(FindExport(hAdvapi32, "AddMandatoryAce") == nullptr);
}

TEST_CASE("Every native registry function resolves")
{
    const auto functions = NativeRegistry::ResolveFunctions();
    const auto pointers = std::bit_cast<std::array<void*, sizeof(functions) / sizeof(void*)>>(functions);
    for (std::size_t i = 0; i < pointers.size(); ++i)
    {
        INFO("field " << i);
        CHECK(pointers[i] != nullptr);
    }
}
