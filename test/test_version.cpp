#include <VReg/core.h>

#include <catch2/catch_test_macros.hpp>

#include <Shlwapi.h>

#include <bit>
#include <format>
#include <regex>
#include <string>

TEST_CASE("The file version holds the project's numbers")
{
    const auto version = VReg::GetFileVersion();

    CHECK(version.wMajor == VREG_TEST_VERSION_MAJOR);
    CHECK(version.wMinor == VREG_TEST_VERSION_MINOR);
    CHECK(version.wPatch == VREG_TEST_VERSION_PATCH);
    // The tests always build from a checkout, so the commits are counted.
    CHECK(version.wRevision > 0);
}

TEST_CASE("The version is semver with the commit as build metadata")
{
    // The tests always build from a checkout, so the commit is known.
    const std::string expected = std::format(
        "{}\\.{}\\.{}(-preview(\\.[0-9]+)?)?\\+[0-9a-f]{{40}}(-dirty)?",
        VREG_TEST_VERSION_MAJOR,
        VREG_TEST_VERSION_MINOR,
        VREG_TEST_VERSION_PATCH
    );
    CHECK(std::regex_match(VReg::GetVersion(), std::regex(expected)));
}

TEST_CASE("A file version lies in memory as its packed four-part number")
{
    constexpr VReg::FileVersion version{.wRevision = 4, .wPatch = 3, .wMinor = 2, .wMajor = 1};

    STATIC_CHECK(std::bit_cast<ULONGLONG>(version) == MAKEDLLVERULL(1, 2, 3, 4));
    constexpr VReg::FileVersion major{.wRevision = 0, .wPatch = 0, .wMinor = 0, .wMajor = 1};
    constexpr VReg::FileVersion minors{.wRevision = 9, .wPatch = 9, .wMinor = 9, .wMajor = 0};
    constexpr VReg::FileVersion patch{.wRevision = 0, .wPatch = 1, .wMinor = 0, .wMajor = 0};
    constexpr VReg::FileVersion minor{.wRevision = 0, .wPatch = 0, .wMinor = 1, .wMajor = 0};
    STATIC_CHECK(major > minors);
    STATIC_CHECK(patch < minor);
    STATIC_CHECK(version == version);
}
