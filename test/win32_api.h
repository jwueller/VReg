#pragma once

#include <VReg/native_registry.h>

#include <Windows.h>

namespace VReg::Test
{
/// The real `Reg*` functions under the member names of `VReg::Registry`, so a
/// scenario template runs against the oracle and against any `Registry`
/// alike. Filled from this module's imports: exactly the functions an
/// application calls.
struct Win32Api
{
#define VREG_IMPORTED(name) decltype(&::name) name = &::name;
    VREG_INTERNAL_REGISTRY_FUNCTIONS(VREG_IMPORTED)
#undef VREG_IMPORTED
};
} // namespace VReg::Test
