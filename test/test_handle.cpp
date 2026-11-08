#include <vreg.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <bit>
#include <set>
#include <string>

namespace
{
std::uintptr_t ValueOf(const HKEY hKey) noexcept
{
    return std::bit_cast<std::uintptr_t>(hKey);
}

/// A section name no other test or process uses.
std::wstring UniqueSectionName(const wchar_t* tag)
{
    return
        L"Local\\VReg-test-"
        + std::to_wstring(GetCurrentProcessId())
        + L"-"
        + tag;
}

/// Maps a section the way a foreign copy would, to arrange its contents.
VReg_HandleSpaceShared* MapRaw(const std::wstring& wsName)
{
    const auto hSection = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(VReg_HandleSpaceShared),
        wsName.c_str()
    );
    REQUIRE(hSection);
    auto* const pShared = static_cast<VReg_HandleSpaceShared*>(
        MapViewOfFile(
            hSection,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(VReg_HandleSpaceShared)
        )
    );
    REQUIRE(pShared);
    return pShared;
}
} // namespace

TEST_CASE("Virtual handles count down from the base in odd steps")
{
    CHECK(ValueOf(VReg_VirtualHKeyFromIndex(0)) == 0x7FFFFFFF);
    CHECK(ValueOf(VReg_VirtualHKeyFromIndex(1)) == 0x7FFFFFFD);
    CHECK(ValueOf(VReg_VirtualHKeyFromIndex(VREG_INTERNAL_VIRTUAL_HKEY_COUNT - 1)) == 1);
    CHECK(VReg_VirtualHKeyFromIndex(VREG_INTERNAL_VIRTUAL_HKEY_COUNT) == nullptr);

    const std::uintptr_t index = GENERATE(
        std::uintptr_t{0},
        std::uintptr_t{12345},
        VREG_INTERNAL_VIRTUAL_HKEY_COUNT - 1
    );
    CAPTURE(index);
    const auto hKey = VReg_VirtualHKeyFromIndex(index);
    CHECK(VRegIsVirtualHKey(hKey));
    CHECK(VReg_IndexFromVirtualHKey(hKey) == index);
}

TEST_CASE("Real and predefined handles are never virtual")
{
    CHECK_FALSE(VRegIsVirtualHKey(nullptr));
    CHECK_FALSE(VRegIsVirtualHKey(std::bit_cast<HKEY>(std::uintptr_t{0x158})));
    CHECK_FALSE(
        VRegIsVirtualHKey(std::bit_cast<HKEY>(std::uintptr_t{0x16E}))
    ); // HKCR tag bit
    CHECK_FALSE(VRegIsVirtualHKey(HKEY_CLASSES_ROOT));
    CHECK_FALSE(VRegIsVirtualHKey(HKEY_CURRENT_USER)); // odd, but above the base
    CHECK_FALSE(VRegIsVirtualHKey(HKEY_LOCAL_MACHINE));
    CHECK_FALSE(VRegIsVirtualHKey(HKEY_USERS));
    CHECK_FALSE(VRegIsVirtualHKey(HKEY_PERFORMANCE_DATA));
    CHECK_FALSE(VRegIsVirtualHKey(HKEY_CURRENT_CONFIG));
}

TEST_CASE("Predefined roots are the range from HKCR to HKCU local settings")
{
    CHECK(VReg_IsPredefinedHKey(HKEY_CLASSES_ROOT));
    CHECK(VReg_IsPredefinedHKey(HKEY_CURRENT_USER));
    CHECK(VReg_IsPredefinedHKey(HKEY_LOCAL_MACHINE));
    CHECK(VReg_IsPredefinedHKey(HKEY_USERS));
    CHECK(VReg_IsPredefinedHKey(HKEY_PERFORMANCE_DATA));
    CHECK(VReg_IsPredefinedHKey(HKEY_CURRENT_CONFIG));
    CHECK(VReg_IsPredefinedHKey(HKEY_DYN_DATA));
    CHECK(VReg_IsPredefinedHKey(HKEY_CURRENT_USER_LOCAL_SETTINGS));

    CHECK_FALSE(VReg_IsPredefinedHKey(nullptr));
    CHECK_FALSE(VReg_IsPredefinedHKey(std::bit_cast<HKEY>(std::uintptr_t{0x158})));
    CHECK_FALSE(VReg_IsPredefinedHKey(VReg_VirtualHKeyFromIndex(0)));
    CHECK_FALSE(
        VReg_IsPredefinedHKey(
            std::bit_cast<HKEY>(std::bit_cast<std::uintptr_t>(HKEY_CURRENT_USER_LOCAL_SETTINGS) + 1)
        )
    );
}

TEST_CASE("Copies sharing a section hand out disjoint handles and own only theirs")
{
    const auto wsName = UniqueSectionName(L"disjoint");
    VReg_HandleSpace first;
    VReg_HandleSpace second;
    VReg_HandleSpaceInit(&first, wsName.c_str());
    VReg_HandleSpaceInit(&second, wsName.c_str());
    REQUIRE(VReg_HandleSpaceIsUsable(&first));
    REQUIRE(VReg_HandleSpaceIsUsable(&second));

    std::set<HKEY> seen;
    // Past one block, so each copy reserves a second one.
    for (std::uintptr_t i = 0; i < VREG_INTERNAL_HANDLE_BLOCK_SIZE + 10; ++i)
    {
        const auto hFirst = VReg_HandleSpaceAllocate(&first, &first);
        const auto hSecond = VReg_HandleSpaceAllocate(&second, &second);
        REQUIRE(VRegIsVirtualHKey(hFirst));
        REQUIRE(VRegIsVirtualHKey(hSecond));
        CHECK(seen.insert(hFirst).second);
        CHECK(seen.insert(hSecond).second);

        CHECK(VReg_HandleSpaceOwns(&first, hFirst));
        CHECK_FALSE(VReg_HandleSpaceOwns(&first, hSecond));
        CHECK(VReg_HandleSpaceOwns(&second, hSecond));
        CHECK_FALSE(VReg_HandleSpaceOwns(&second, hFirst));

        CHECK(VReg_HandleSpaceFind(&first, hFirst) == &first);
        CHECK(VReg_HandleSpaceFind(&first, hSecond) == nullptr);
        CHECK(VReg_HandleSpaceFind(&second, hSecond) == &second);
    }

    CHECK_FALSE(VReg_HandleSpaceOwns(&first, HKEY_LOCAL_MACHINE));
    CHECK_FALSE(VReg_HandleSpaceOwns(&first, nullptr));
    CHECK(VReg_HandleSpaceFind(&first, HKEY_LOCAL_MACHINE) == nullptr);
}

TEST_CASE("A released handle is never handed out again and finds nothing")
{
    const auto wsName = UniqueSectionName(L"released");
    VReg_HandleSpace space;
    VReg_HandleSpaceInit(&space, wsName.c_str());
    REQUIRE(VReg_HandleSpaceIsUsable(&space));

    int target = 0;
    const auto hKey = VReg_HandleSpaceAllocate(&space, &target);
    REQUIRE(VReg_HandleSpaceFind(&space, hKey) == &target);

    VReg_HandleSpaceRelease(&space, hKey);
    CHECK(VReg_HandleSpaceFind(&space, hKey) == nullptr);
    CHECK(VReg_HandleSpaceOwns(&space, hKey));
    CHECK(VReg_HandleSpaceAllocate(&space, &target) != hKey);

    // A handle the space does not know is ignored.
    VReg_HandleSpaceRelease(&space, HKEY_LOCAL_MACHINE);
    VReg_HandleSpaceRelease(&space, nullptr);
}

TEST_CASE("A section claimed with a different layout makes the space unusable")
{
    const auto wsName = UniqueSectionName(L"foreign");
    MapRaw(wsName)->magic = 0x12345678;

    VReg_HandleSpace space;
    VReg_HandleSpaceInit(&space, wsName.c_str());
    CHECK_FALSE(VReg_HandleSpaceIsUsable(&space));
    CHECK(VReg_HandleSpaceAllocate(&space, &space) == nullptr);
}

TEST_CASE("Allocation fails once the namespace is exhausted")
{
    const auto wsName = UniqueSectionName(L"exhausted");
    auto* const pShared = MapRaw(wsName);
    pShared->magic = VREG_INTERNAL_HANDLE_SPACE_MAGIC;
    pShared->nextBlock = static_cast<LONG>(VREG_INTERNAL_HANDLE_BLOCK_COUNT - 1);

    VReg_HandleSpace space;
    VReg_HandleSpaceInit(&space, wsName.c_str());
    REQUIRE(VReg_HandleSpaceIsUsable(&space));
    for (std::uintptr_t i = 0; i < VREG_INTERNAL_HANDLE_BLOCK_SIZE; ++i)
        REQUIRE(VRegIsVirtualHKey(VReg_HandleSpaceAllocate(&space, &space)));

    CHECK(VReg_HandleSpaceAllocate(&space, &space) == nullptr);
}

TEST_CASE("The process space is usable and issues virtual handles")
{
    auto* const pSpace = VReg_ProcessHandleSpace();
    REQUIRE(VReg_HandleSpaceIsUsable(pSpace));
    const auto hKey = VReg_HandleSpaceAllocate(pSpace, pSpace);
    CHECK(VRegIsVirtualHKey(hKey));
    CHECK(VReg_HandleSpaceOwns(pSpace, hKey));
    CHECK(pSpace == VReg_ProcessHandleSpace());
}

TEST_CASE("The process space is the section every copy of VReg shares")
{
    const auto wsName = L"Local\\VReg-handles-" + std::to_wstring(GetCurrentProcessId());
    auto* const pShared = MapRaw(wsName);
    auto* const pSpace = VReg_ProcessHandleSpace();
    REQUIRE(VReg_HandleSpaceIsUsable(pSpace));
    CHECK(pShared->magic == VREG_INTERNAL_HANDLE_SPACE_MAGIC);

    // The first allocation reserves this copy's first block from the section.
    REQUIRE(VRegIsVirtualHKey(VReg_HandleSpaceAllocate(pSpace, pSpace)));
    CHECK(pShared->nextBlock >= 1);
}
