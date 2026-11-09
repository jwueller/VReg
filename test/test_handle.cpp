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
    CHECK(ValueOf(VReg_InternalVirtualHKeyFromIndex(0)) == 0x7FFFFFFF);
    CHECK(ValueOf(VReg_InternalVirtualHKeyFromIndex(1)) == 0x7FFFFFFD);
    CHECK(ValueOf(VReg_InternalVirtualHKeyFromIndex(VREG_INTERNAL_VIRTUAL_HKEY_COUNT - 1)) == 1);
    CHECK(VReg_InternalVirtualHKeyFromIndex(VREG_INTERNAL_VIRTUAL_HKEY_COUNT) == nullptr);

    const std::uintptr_t index = GENERATE(
        std::uintptr_t{0},
        std::uintptr_t{12345},
        VREG_INTERNAL_VIRTUAL_HKEY_COUNT - 1
    );
    CAPTURE(index);
    const auto hKey = VReg_InternalVirtualHKeyFromIndex(index);
    CHECK(VReg_IsVirtualHKey(hKey));
    CHECK(VReg_InternalIndexFromVirtualHKey(hKey) == index);
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
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_CLASSES_ROOT));
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_CURRENT_USER));
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_LOCAL_MACHINE));
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_USERS));
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_PERFORMANCE_DATA));
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_CURRENT_CONFIG));
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_DYN_DATA));
    CHECK(VReg_InternalIsPredefinedHKey(HKEY_CURRENT_USER_LOCAL_SETTINGS));

    CHECK_FALSE(VReg_InternalIsPredefinedHKey(nullptr));
    CHECK_FALSE(VReg_InternalIsPredefinedHKey(std::bit_cast<HKEY>(std::uintptr_t{0x158})));
    CHECK_FALSE(VReg_InternalIsPredefinedHKey(VReg_InternalVirtualHKeyFromIndex(0)));
    CHECK_FALSE(
        VReg_InternalIsPredefinedHKey(
            std::bit_cast<HKEY>(std::bit_cast<std::uintptr_t>(HKEY_CURRENT_USER_LOCAL_SETTINGS) + 1)
        )
    );
}

TEST_CASE("Copies sharing a section hand out disjoint handles and own only theirs")
{
    const auto wsName = UniqueSectionName(L"disjoint");
    VRegInternalHandleSpace first;
    VRegInternalHandleSpace second;
    VRegInternalHandleSpace_Init(&first, wsName.c_str());
    VRegInternalHandleSpace_Init(&second, wsName.c_str());
    REQUIRE(VRegInternalHandleSpace_IsUsable(&first));
    REQUIRE(VRegInternalHandleSpace_IsUsable(&second));

    std::set<HKEY> seen;
    // Past one block, so each copy reserves a second one.
    for (std::uintptr_t i = 0; i < VREG_INTERNAL_HANDLE_BLOCK_SIZE + 10; ++i)
    {
        const auto hFirst = VRegInternalHandleSpace_Allocate(&first, &first);
        const auto hSecond = VRegInternalHandleSpace_Allocate(&second, &second);
        REQUIRE(VReg_IsVirtualHKey(hFirst));
        REQUIRE(VReg_IsVirtualHKey(hSecond));
        CHECK(seen.insert(hFirst).second);
        CHECK(seen.insert(hSecond).second);

        CHECK(VRegInternalHandleSpace_Owns(&first, hFirst));
        CHECK_FALSE(VRegInternalHandleSpace_Owns(&first, hSecond));
        CHECK(VRegInternalHandleSpace_Owns(&second, hSecond));
        CHECK_FALSE(VRegInternalHandleSpace_Owns(&second, hFirst));

        CHECK(VRegInternalHandleSpace_Find(&first, hFirst) == &first);
        CHECK(VRegInternalHandleSpace_Find(&first, hSecond) == nullptr);
        CHECK(VRegInternalHandleSpace_Find(&second, hSecond) == &second);
    }

    CHECK_FALSE(VRegInternalHandleSpace_Owns(&first, HKEY_LOCAL_MACHINE));
    CHECK_FALSE(VRegInternalHandleSpace_Owns(&first, nullptr));
    CHECK(VRegInternalHandleSpace_Find(&first, HKEY_LOCAL_MACHINE) == nullptr);
}

TEST_CASE("A released handle is never handed out again and finds nothing")
{
    const auto wsName = UniqueSectionName(L"released");
    VRegInternalHandleSpace space;
    VRegInternalHandleSpace_Init(&space, wsName.c_str());
    REQUIRE(VRegInternalHandleSpace_IsUsable(&space));

    int target = 0;
    const auto hKey = VRegInternalHandleSpace_Allocate(&space, &target);
    REQUIRE(VRegInternalHandleSpace_Find(&space, hKey) == &target);

    VRegInternalHandleSpace_Release(&space, hKey);
    CHECK(VRegInternalHandleSpace_Find(&space, hKey) == nullptr);
    CHECK(VRegInternalHandleSpace_Owns(&space, hKey));
    CHECK(VRegInternalHandleSpace_Allocate(&space, &target) != hKey);

    // A handle the space does not know is ignored.
    VRegInternalHandleSpace_Release(&space, HKEY_LOCAL_MACHINE);
    VRegInternalHandleSpace_Release(&space, nullptr);
}

TEST_CASE("A section claimed with a different layout makes the space unusable")
{
    const auto wsName = UniqueSectionName(L"foreign");
    MapRaw(wsName)->magic = 0x12345678;

    VRegInternalHandleSpace space;
    VRegInternalHandleSpace_Init(&space, wsName.c_str());
    CHECK_FALSE(VRegInternalHandleSpace_IsUsable(&space));
    CHECK(VRegInternalHandleSpace_Allocate(&space, &space) == nullptr);
}

TEST_CASE("Allocation fails once the namespace is exhausted")
{
    const auto wsName = UniqueSectionName(L"exhausted");
    auto* const pShared = MapRaw(wsName);
    pShared->magic = VREG_INTERNAL_HANDLE_SPACE_MAGIC;
    pShared->nextBlock = static_cast<LONG>(VREG_INTERNAL_HANDLE_BLOCK_COUNT - 1);

    VRegInternalHandleSpace space;
    VRegInternalHandleSpace_Init(&space, wsName.c_str());
    REQUIRE(VRegInternalHandleSpace_IsUsable(&space));
    for (std::uintptr_t i = 0; i < VREG_INTERNAL_HANDLE_BLOCK_SIZE; ++i)
        REQUIRE(VReg_IsVirtualHKey(VRegInternalHandleSpace_Allocate(&space, &space)));

    CHECK(VRegInternalHandleSpace_Allocate(&space, &space) == nullptr);
}

TEST_CASE("The process space is usable and issues virtual handles")
{
    auto* const pSpace = VReg_InternalProcessHandleSpace();
    REQUIRE(VRegInternalHandleSpace_IsUsable(pSpace));
    const auto hKey = VRegInternalHandleSpace_Allocate(pSpace, pSpace);
    CHECK(VReg_IsVirtualHKey(hKey));
    CHECK(VRegInternalHandleSpace_Owns(pSpace, hKey));
    CHECK(pSpace == VReg_InternalProcessHandleSpace());
}

TEST_CASE("The process space is the section every copy of VReg shares")
{
    const auto wsName = L"Local\\VReg-handles-" + std::to_wstring(GetCurrentProcessId());
    auto* const pShared = MapRaw(wsName);
    auto* const pSpace = VReg_InternalProcessHandleSpace();
    REQUIRE(VRegInternalHandleSpace_IsUsable(pSpace));
    CHECK(pShared->magic == VREG_INTERNAL_HANDLE_SPACE_MAGIC);

    // The first allocation reserves this copy's first block from the section.
    REQUIRE(VReg_IsVirtualHKey(VRegInternalHandleSpace_Allocate(pSpace, pSpace)));
    CHECK(pShared->nextBlock >= 1);
}
