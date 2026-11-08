#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <Shlwapi.h>

TEST_CASE("The version packs the header's numbers as MAKEDLLVERULL does, and CMake reports the same")
{
    CHECK(VREG_VERSION_MAJOR == VREG_TEST_VERSION_MAJOR);
    CHECK(VREG_VERSION_MINOR == VREG_TEST_VERSION_MINOR);
    CHECK(VREG_VERSION_PATCH == VREG_TEST_VERSION_PATCH);

    const ULONGLONG version = VREG_VERSION;
    CHECK(static_cast<WORD>(version >> 48) == VREG_VERSION_MAJOR);
    CHECK(static_cast<WORD>(version >> 32) == VREG_VERSION_MINOR);
    CHECK(static_cast<WORD>(version >> 16) == VREG_VERSION_PATCH);
    CHECK(static_cast<WORD>(version) == VREG_VERSION_TWEAK);
}
