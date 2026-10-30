#pragma once

#include <VReg/native_registry.h>

#include <Windows.h>

namespace VReg::Test
{
/// The real `Reg*` functions under the member names of `VReg::Registry`, so a
/// scenario template runs against the oracle and against any `Registry`
/// alike. The table is @ref NativeRegistry::Functions, filled from this
/// module's imports: exactly the functions an application calls.
struct Win32Api : NativeRegistry::Functions
{
#define VREG_IMPORTED(name) &::name,
    Win32Api() noexcept :
        Functions{VREG_REGISTRY_FUNCTIONS(VREG_IMPORTED)}
    {
    }
#undef VREG_IMPORTED
};
} // namespace VReg::Test
