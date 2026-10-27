#include <VReg/VReg.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The linked library reports the version its headers declare")
{
    const auto version = VReg::GetVersion();

    CHECK(version.major == VReg::kVersionMajor);
    CHECK(version.minor == VReg::kVersionMinor);
    CHECK(version.patch == VReg::kVersionPatch);
}
