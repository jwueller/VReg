#include <VReg/core.h>

#include <catch2/catch_test_macros.hpp>

#include <Shlwapi.h>

#include <format>
#include <regex>
#include <string>

TEST_CASE("The file version holds the project's numbers, packed as MAKEDLLVERULL does")
{
    const ULONGLONG version = VRegGetFileVersion();

    CHECK(static_cast<WORD>(version >> 48) == VREG_TEST_VERSION_MAJOR);
    CHECK(static_cast<WORD>(version >> 32) == VREG_TEST_VERSION_MINOR);
    CHECK(static_cast<WORD>(version >> 16) == VREG_TEST_VERSION_PATCH);
    // The tests always build from a checkout, so the commits are counted.
    CHECK(static_cast<WORD>(version) > 0);

    const WORD wRevision = static_cast<WORD>(version);
    CHECK(
        version
        == MAKEDLLVERULL(VREG_TEST_VERSION_MAJOR, VREG_TEST_VERSION_MINOR, VREG_TEST_VERSION_PATCH, wRevision)
    );
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
    CHECK(std::regex_match(VRegGetVersion(), std::regex(expected)));
}
