#include <VReg/core.h>

#include <catch2/catch_test_macros.hpp>

#include <Shlwapi.h>

#include <bit>
#include <format>
#include <regex>
#include <string>

TEST_CASE("The linked library reports the project's version")
{
    const auto version = VReg::GetVersion();

    CHECK(version.wMajor == VREG_TEST_VERSION_MAJOR);
    CHECK(version.wMinor == VREG_TEST_VERSION_MINOR);
    CHECK(version.wPatch == VREG_TEST_VERSION_PATCH);
    // The tests always build from a checkout, so the commits are counted.
    CHECK(version.wRevision > 0);
}

TEST_CASE("The version string is semver with the commit as build metadata")
{
    // The tests always build from a checkout, so the commit is known.
    const std::string expected = std::format(
        "{}\\.{}\\.{}(-preview(\\.[0-9]+)?)?\\+[0-9a-f]{{40}}(-dirty)?",
        VREG_TEST_VERSION_MAJOR,
        VREG_TEST_VERSION_MINOR,
        VREG_TEST_VERSION_PATCH
    );
    CHECK(std::regex_match(VReg::GetVersionString(), std::regex(expected)));
}

TEST_CASE("A version lies in memory as its packed four-part number")
{
    constexpr VReg::Version version{.wRevision = 4, .wPatch = 3, .wMinor = 2, .wMajor = 1};

    STATIC_CHECK(std::bit_cast<ULONGLONG>(version) == MAKEDLLVERULL(1, 2, 3, 4));
    STATIC_CHECK(VReg::Version{.wMajor = 1} > VReg::Version{.wPatch = 9, .wMinor = 9});
    STATIC_CHECK(VReg::Version{.wPatch = 1} < VReg::Version{.wMinor = 1});
    STATIC_CHECK(version == version);
}
