#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <Shlwapi.h>

TEST_CASE("The version packs the header's numbers as MAKEDLLVERULL does, and CMake reports the same")
{
    CHECK(VREG_VERSION_MAJOR == VREG_TEST_VERSION_MAJOR);
    CHECK(VREG_VERSION_MINOR == VREG_TEST_VERSION_MINOR);
    CHECK(VREG_VERSION_PATCH == VREG_TEST_VERSION_PATCH);
    CHECK(
        VREG_VERSION
        == MAKEDLLVERULL(VREG_VERSION_MAJOR, VREG_VERSION_MINOR, VREG_VERSION_PATCH, VREG_VERSION_TWEAK)
    );
}
