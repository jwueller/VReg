#include <VReg/native_registry.h>

#include "core/export_table.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <string>
#include <vector>

using namespace VReg;

TEST_CASE("Export lookup agrees with the loader")
{
    const auto hKernelBase = GetModuleHandleW(L"KernelBase.dll");
    REQUIRE(hKernelBase);

    for (const auto* const pszName : {"RegOpenKeyExW", "RegCloseKey", "RegQueryInfoKeyA", "CreateFileW"})
    {
        INFO(pszName);
        CHECK(
            FindExport(
                hKernelBase,
                pszName
            ) == reinterpret_cast<void*>(GetProcAddress(hKernelBase, pszName))
        );
    }

    CHECK(FindExport(hKernelBase, "NoSuchExport") == nullptr);
    CHECK(FindExport(nullptr, "RegOpenKeyExW") == nullptr);
}

namespace
{
std::vector<std::string> ExportNames(const HMODULE hModule)
{
    const auto* const pBase = reinterpret_cast<const BYTE*>(hModule);
    const auto* const pDos = reinterpret_cast<const IMAGE_DOS_HEADER*>(pBase);
    const auto* const pNt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        pBase + pDos->e_lfanew
    );
    const auto& directory =
        pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    const auto* const pExports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
        pBase + directory.VirtualAddress
    );
    const auto* const pNames = reinterpret_cast<const DWORD*>(
        pBase + pExports->AddressOfNames
    );

    std::vector<std::string> names;
    for (DWORD i = 0; i < pExports->NumberOfNames; ++i)
        names.emplace_back(reinterpret_cast<const char*>(pBase + pNames[i]));
    return names;
}
} // namespace

TEST_CASE("Forwarded exports are not resolved")
{
    // Which exports advapi32 forwards differs between Windows versions and
    // Wine, so the loader names them: GetProcAddress follows a forwarder into
    // the module it names.
    const auto hAdvapi32 = GetModuleHandleW(L"advapi32.dll");
    REQUIRE(hAdvapi32);

    std::size_t cForwarded = 0;
    for (const auto& name : ExportNames(hAdvapi32))
    {
        const auto pfn = GetProcAddress(hAdvapi32, name.c_str());
        HMODULE hOwner{};
        const bool isOwned = pfn
            && GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(pfn),
                &hOwner
            );
        if (!isOwned || hOwner == hAdvapi32)
            continue;

        INFO(name);
        CHECK(FindExport(hAdvapi32, name.c_str()) == nullptr);
        ++cForwarded;
    }
    CHECK(cForwarded > 0);
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
