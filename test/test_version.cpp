#include <VReg/core.h>

#include <catch2/catch_test_macros.hpp>

#include <Shlwapi.h>

#include <bit>

TEST_CASE("The linked library reports the version its headers declare")
{
    const auto version = VReg::GetVersion();

    CHECK(version.wMajor == VReg::VREG_VERSION_MAJOR);
    CHECK(version.wMinor == VReg::VREG_VERSION_MINOR);
    CHECK(version.wPatch == VReg::VREG_VERSION_PATCH);
    CHECK(version.wRevision == 0);
}

TEST_CASE("A version lies in memory as its packed four-part number")
{
    constexpr VReg::Version version{.wRevision = 4, .wPatch = 3, .wMinor = 2, .wMajor = 1};

    STATIC_CHECK(std::bit_cast<ULONGLONG>(version) == MAKEDLLVERULL(1, 2, 3, 4));
    STATIC_CHECK(VReg::Version{.wMajor = 1} > VReg::Version{.wPatch = 9, .wMinor = 9});
    STATIC_CHECK(VReg::Version{.wPatch = 1} < VReg::Version{.wMinor = 1});
    STATIC_CHECK(version == version);
}
