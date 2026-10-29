#include <VReg/VReg.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The linked library reports the version its headers declare")
{
    const auto version = VReg::GetVersion();

    CHECK(version.major == VReg::VREG_VERSION_MAJOR);
    CHECK(version.minor == VReg::VREG_VERSION_MINOR);
    CHECK(version.patch == VReg::VREG_VERSION_PATCH);
}
