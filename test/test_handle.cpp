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
VRegInternalHandleSpaceShared* MapRaw(const std::wstring& wsName)
{
    const auto hSection = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(VRegInternalHandleSpaceShared),
        wsName.c_str()
    );
    REQUIRE(hSection);
    auto* const pShared = static_cast<VRegInternalHandleSpaceShared*>(
        MapViewOfFile(
            hSection,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(VRegInternalHandleSpaceShared)
        )
    );
    REQUIRE(pShared);
    return pShared;
}
} // namespace

TEST_CASE("Virtual handles count down from the base in odd steps")
{
    CHECK(ValueOf(VRegInternal_VirtualHKeyFromIndex(0)) == 0x7FFFFFFF);
    CHECK(ValueOf(VRegInternal_VirtualHKeyFromIndex(1)) == 0x7FFFFFFD);
    CHECK(ValueOf(VRegInternal_VirtualHKeyFromIndex(VREG_INTERNAL_VIRTUAL_HKEY_COUNT - 1)) == 1);
    CHECK(VRegInternal_VirtualHKeyFromIndex(VREG_INTERNAL_VIRTUAL_HKEY_COUNT) == nullptr);

    const std::uintptr_t index = GENERATE(
        std::uintptr_t{0},
        std::uintptr_t{12345},
        VREG_INTERNAL_VIRTUAL_HKEY_COUNT - 1
    );
    CAPTURE(index);
    const auto hKey = VRegInternal_VirtualHKeyFromIndex(index);
    CHECK(VReg_IsVirtualHKey(hKey));
    CHECK(VRegInternal_IndexFromVirtualHKey(hKey) == index);
}

TEST_CASE("Real and predefined handles are never virtual")
{
    CHECK_FALSE(VReg_IsVirtualHKey(nullptr));
    CHECK_FALSE(VReg_IsVirtualHKey(std::bit_cast<HKEY>(std::uintptr_t{0x158})));
    CHECK_FALSE(
        VReg_IsVirtualHKey(std::bit_cast<HKEY>(std::uintptr_t{0x16E}))
    ); // HKCR tag bit
    CHECK_FALSE(VReg_IsVirtualHKey(HKEY_CLASSES_ROOT));
    CHECK_FALSE(VReg_IsVirtualHKey(HKEY_CURRENT_USER)); // odd, but above the base
    CHECK_FALSE(VReg_IsVirtualHKey(HKEY_LOCAL_MACHINE));
    CHECK_FALSE(VReg_IsVirtualHKey(HKEY_USERS));
    CHECK_FALSE(VReg_IsVirtualHKey(HKEY_PERFORMANCE_DATA));
    CHECK_FALSE(VReg_IsVirtualHKey(HKEY_CURRENT_CONFIG));
}

TEST_CASE("Predefined roots are the range from HKCR to HKCU local settings")
{
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_CLASSES_ROOT));
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_CURRENT_USER));
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_LOCAL_MACHINE));
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_USERS));
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_PERFORMANCE_DATA));
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_CURRENT_CONFIG));
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_DYN_DATA));
    CHECK(VRegInternal_IsPredefinedHKey(HKEY_CURRENT_USER_LOCAL_SETTINGS));

    CHECK_FALSE(VRegInternal_IsPredefinedHKey(nullptr));
    CHECK_FALSE(VRegInternal_IsPredefinedHKey(std::bit_cast<HKEY>(std::uintptr_t{0x158})));
    CHECK_FALSE(VRegInternal_IsPredefinedHKey(VRegInternal_VirtualHKeyFromIndex(0)));
    CHECK_FALSE(
        VRegInternal_IsPredefinedHKey(
            std::bit_cast<HKEY>(std::bit_cast<std::uintptr_t>(HKEY_CURRENT_USER_LOCAL_SETTINGS) + 1)
        )
    );
}

TEST_CASE("Copies sharing a section hand out disjoint handles and own only theirs")
{
    const auto wsName = UniqueSectionName(L"disjoint");
    VRegInternalHandleSpace first;
    VRegInternalHandleSpace second;
    VRegInternal_HandleSpaceInit(&first, wsName.c_str());
    VRegInternal_HandleSpaceInit(&second, wsName.c_str());
    REQUIRE(VRegInternal_HandleSpaceIsUsable(&first));
    REQUIRE(VRegInternal_HandleSpaceIsUsable(&second));

    std::set<HKEY> seen;
    // Past one block, so each copy reserves a second one.
    for (std::uintptr_t i = 0; i < VREG_INTERNAL_HANDLE_BLOCK_SIZE + 10; ++i)
    {
        const auto hFirst = VRegInternal_HandleSpaceAllocate(&first, &first);
        const auto hSecond = VRegInternal_HandleSpaceAllocate(&second, &second);
        REQUIRE(VReg_IsVirtualHKey(hFirst));
        REQUIRE(VReg_IsVirtualHKey(hSecond));
        CHECK(seen.insert(hFirst).second);
        CHECK(seen.insert(hSecond).second);

        CHECK(VRegInternal_HandleSpaceOwns(&first, hFirst));
        CHECK_FALSE(VRegInternal_HandleSpaceOwns(&first, hSecond));
        CHECK(VRegInternal_HandleSpaceOwns(&second, hSecond));
        CHECK_FALSE(VRegInternal_HandleSpaceOwns(&second, hFirst));

        CHECK(VRegInternal_HandleSpaceFind(&first, hFirst) == &first);
        CHECK(VRegInternal_HandleSpaceFind(&first, hSecond) == nullptr);
        CHECK(VRegInternal_HandleSpaceFind(&second, hSecond) == &second);
    }

    CHECK_FALSE(VRegInternal_HandleSpaceOwns(&first, HKEY_LOCAL_MACHINE));
    CHECK_FALSE(VRegInternal_HandleSpaceOwns(&first, nullptr));
    CHECK(VRegInternal_HandleSpaceFind(&first, HKEY_LOCAL_MACHINE) == nullptr);
}

TEST_CASE("A released handle is never handed out again and finds nothing")
{
    const auto wsName = UniqueSectionName(L"released");
    VRegInternalHandleSpace space;
    VRegInternal_HandleSpaceInit(&space, wsName.c_str());
    REQUIRE(VRegInternal_HandleSpaceIsUsable(&space));

    int target = 0;
    const auto hKey = VRegInternal_HandleSpaceAllocate(&space, &target);
    REQUIRE(VRegInternal_HandleSpaceFind(&space, hKey) == &target);

    VRegInternal_HandleSpaceRelease(&space, hKey);
    CHECK(VRegInternal_HandleSpaceFind(&space, hKey) == nullptr);
    CHECK(VRegInternal_HandleSpaceOwns(&space, hKey));
    CHECK(VRegInternal_HandleSpaceAllocate(&space, &target) != hKey);

    // A handle the space does not know is ignored.
    VRegInternal_HandleSpaceRelease(&space, HKEY_LOCAL_MACHINE);
    VRegInternal_HandleSpaceRelease(&space, nullptr);
}

TEST_CASE("A section claimed with a different layout makes the space unusable")
{
    const auto wsName = UniqueSectionName(L"foreign");
    MapRaw(wsName)->magic = 0x12345678;

    VRegInternalHandleSpace space;
    VRegInternal_HandleSpaceInit(&space, wsName.c_str());
    CHECK_FALSE(VRegInternal_HandleSpaceIsUsable(&space));
    CHECK(VRegInternal_HandleSpaceAllocate(&space, &space) == nullptr);
}

TEST_CASE("Allocation fails once the namespace is exhausted")
{
    const auto wsName = UniqueSectionName(L"exhausted");
    auto* const pShared = MapRaw(wsName);
    pShared->magic = VREG_INTERNAL_HANDLE_SPACE_MAGIC;
    pShared->nextBlock = static_cast<LONG>(VREG_INTERNAL_HANDLE_BLOCK_COUNT - 1);

    VRegInternalHandleSpace space;
    VRegInternal_HandleSpaceInit(&space, wsName.c_str());
    REQUIRE(VRegInternal_HandleSpaceIsUsable(&space));
    for (std::uintptr_t i = 0; i < VREG_INTERNAL_HANDLE_BLOCK_SIZE; ++i)
        REQUIRE(VReg_IsVirtualHKey(VRegInternal_HandleSpaceAllocate(&space, &space)));

    CHECK(VRegInternal_HandleSpaceAllocate(&space, &space) == nullptr);
}

TEST_CASE("The process space is usable and issues virtual handles")
{
    auto* const pSpace = VRegInternal_ProcessHandleSpace();
    REQUIRE(VRegInternal_HandleSpaceIsUsable(pSpace));
    const auto hKey = VRegInternal_HandleSpaceAllocate(pSpace, pSpace);
    CHECK(VReg_IsVirtualHKey(hKey));
    CHECK(VRegInternal_HandleSpaceOwns(pSpace, hKey));
    CHECK(pSpace == VRegInternal_ProcessHandleSpace());
}

TEST_CASE("The process space is the section every copy of VReg shares")
{
    const auto wsName = L"Local\\VReg-handles-" + std::to_wstring(GetCurrentProcessId());
    auto* const pShared = MapRaw(wsName);
    auto* const pSpace = VRegInternal_ProcessHandleSpace();
    REQUIRE(VRegInternal_HandleSpaceIsUsable(pSpace));
    CHECK(pShared->magic == VREG_INTERNAL_HANDLE_SPACE_MAGIC);

    // The first allocation reserves this copy's first block from the section.
    REQUIRE(VReg_IsVirtualHKey(VRegInternal_HandleSpaceAllocate(pSpace, pSpace)));
    CHECK(pShared->nextBlock >= 1);
}
