#include <VReg/handle.h>

#include "core/handle_space.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

using namespace VReg;

namespace
{
std::uintptr_t ValueOf(const HKEY hKey)
{
    return std::bit_cast<std::uintptr_t>(hKey);
}

/// A section name no other test or process uses.
std::wstring UniqueSectionName(const wchar_t* tag)
{
    return L"Local\\VReg-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + tag;
}

/// Maps a section the way a foreign copy would, to arrange its contents.
HandleSpace::Shared* MapRaw(const std::wstring& wsName)
{
    const auto hSection = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(HandleSpace::Shared),
        wsName.c_str()
    );
    REQUIRE(hSection);
    auto* const pShared = static_cast<HandleSpace::Shared*>(
        MapViewOfFile(
            hSection,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(HandleSpace::Shared)
        )
    );
    REQUIRE(pShared);
    return pShared;
}
} // namespace

TEST_CASE("Virtual handles count down from the base in odd steps")
{
    CHECK(ValueOf(VirtualHKeyFromIndex(0)) == 0x7FFFFFFF);
    CHECK(ValueOf(VirtualHKeyFromIndex(1)) == 0x7FFFFFFD);
    CHECK(ValueOf(VirtualHKeyFromIndex(kVirtualHKeyCount - 1)) == 1);
    CHECK(VirtualHKeyFromIndex(kVirtualHKeyCount) == nullptr);

    for (const std::uintptr_t index : {std::uintptr_t{0}, std::uintptr_t{12345}, kVirtualHKeyCount - 1})
    {
        const auto hKey = VirtualHKeyFromIndex(index);
        CHECK(IsVirtualHKey(hKey));
        CHECK(IndexFromVirtualHKey(hKey) == index);
    }
}

TEST_CASE("Real and predefined handles are never virtual")
{
    CHECK_FALSE(IsVirtualHKey(nullptr));
    CHECK_FALSE(IsVirtualHKey(std::bit_cast<HKEY>(std::uintptr_t{0x158})));
    CHECK_FALSE(IsVirtualHKey(std::bit_cast<HKEY>(std::uintptr_t{0x16E}))); // HKCR tag bit
    CHECK_FALSE(IsVirtualHKey(HKEY_CLASSES_ROOT));
    CHECK_FALSE(IsVirtualHKey(HKEY_CURRENT_USER)); // odd, but above the base
    CHECK_FALSE(IsVirtualHKey(HKEY_LOCAL_MACHINE));
    CHECK_FALSE(IsVirtualHKey(HKEY_USERS));
    CHECK_FALSE(IsVirtualHKey(HKEY_PERFORMANCE_DATA));
    CHECK_FALSE(IsVirtualHKey(HKEY_CURRENT_CONFIG));
}

TEST_CASE("Copies sharing a section hand out disjoint handles and own only theirs")
{
    const auto wsName = UniqueSectionName(L"disjoint");
    HandleSpace first(wsName);
    HandleSpace second(wsName);
    REQUIRE(first.IsUsable());
    REQUIRE(second.IsUsable());

    std::set<HKEY> seen;
    // Past one block, so each copy reserves a second one.
    for (std::uintptr_t i = 0; i < HandleSpace::kBlockSize + 10; ++i)
    {
        const auto hFirst = first.Allocate(&first);
        const auto hSecond = second.Allocate(&second);
        REQUIRE(IsVirtualHKey(hFirst));
        REQUIRE(IsVirtualHKey(hSecond));
        CHECK(seen.insert(hFirst).second);
        CHECK(seen.insert(hSecond).second);

        CHECK(first.Owns(hFirst));
        CHECK_FALSE(first.Owns(hSecond));
        CHECK(second.Owns(hSecond));
        CHECK_FALSE(second.Owns(hFirst));

        CHECK(first.Find(hFirst) == &first);
        CHECK(first.Find(hSecond) == nullptr);
        CHECK(second.Find(hSecond) == &second);
    }

    CHECK_FALSE(first.Owns(HKEY_LOCAL_MACHINE));
    CHECK_FALSE(first.Owns(nullptr));
    CHECK(first.Find(HKEY_LOCAL_MACHINE) == nullptr);
}

TEST_CASE("A section claimed with a different layout makes the space unusable")
{
    const auto wsName = UniqueSectionName(L"foreign");
    MapRaw(wsName)->magic = 0x12345678;

    HandleSpace space(wsName);
    CHECK_FALSE(space.IsUsable());
    CHECK(space.Allocate(&space) == nullptr);
}

TEST_CASE("Allocation fails once the namespace is exhausted")
{
    const auto wsName = UniqueSectionName(L"exhausted");
    auto* const pShared = MapRaw(wsName);
    pShared->magic = HandleSpace::kMagic;
    pShared->nextBlock = static_cast<LONG>(HandleSpace::kBlockCount - 1);

    HandleSpace space(wsName);
    REQUIRE(space.IsUsable());
    for (std::uintptr_t i = 0; i < HandleSpace::kBlockSize; ++i)
        REQUIRE(IsVirtualHKey(space.Allocate(&space)));

    CHECK(space.Allocate(&space) == nullptr);
}

TEST_CASE("The process space is usable and issues virtual handles")
{
    auto& space = HandleSpace::Process();
    REQUIRE(space.IsUsable());
    const auto hKey = space.Allocate(&space);
    CHECK(IsVirtualHKey(hKey));
    CHECK(space.Owns(hKey));
    CHECK(&space == &HandleSpace::Process());
}
