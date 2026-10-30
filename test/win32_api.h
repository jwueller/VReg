#pragma once

#include <Windows.h>

namespace VReg::Test
{
// A pointer to the real function: a call through it converts its arguments
// against the real signature, as a direct call would.
#define VREG_FORWARD(name) static inline const auto name = &::name;

/// The real `Reg*` functions under the member names of `VReg::Registry`, so a
/// scenario template runs against the oracle and against any `Registry`
/// alike.
struct Win32Api
{
    VREG_FORWARD(RegOpenKeyExW)
    VREG_FORWARD(RegCreateKeyExW)
    VREG_FORWARD(RegCloseKey)
    VREG_FORWARD(RegQueryValueExW)
    VREG_FORWARD(RegSetValueExW)
    VREG_FORWARD(RegDeleteValueW)
    VREG_FORWARD(RegDeleteKeyExW)
    VREG_FORWARD(RegDeleteTreeW)
    VREG_FORWARD(RegEnumKeyExW)
    VREG_FORWARD(RegEnumValueW)
    VREG_FORWARD(RegQueryInfoKeyW)
    VREG_FORWARD(RegLoadMUIStringW)
    VREG_FORWARD(RegGetValueW)
    VREG_FORWARD(RegNotifyChangeKeyValue)
    VREG_FORWARD(RegFlushKey)
    VREG_FORWARD(RegOpenKeyExA)
    VREG_FORWARD(RegCreateKeyExA)
    VREG_FORWARD(RegQueryValueExA)
    VREG_FORWARD(RegSetValueExA)
    VREG_FORWARD(RegDeleteValueA)
    VREG_FORWARD(RegDeleteKeyExA)
    VREG_FORWARD(RegDeleteTreeA)
    VREG_FORWARD(RegEnumKeyExA)
    VREG_FORWARD(RegEnumValueA)
    VREG_FORWARD(RegQueryInfoKeyA)
    VREG_FORWARD(RegLoadMUIStringA)
    VREG_FORWARD(RegGetValueA)
    VREG_FORWARD(RegOpenKeyA)
    VREG_FORWARD(RegOpenKeyW)
    VREG_FORWARD(RegCreateKeyA)
    VREG_FORWARD(RegCreateKeyW)
    VREG_FORWARD(RegQueryValueA)
    VREG_FORWARD(RegQueryValueW)
    VREG_FORWARD(RegSetValueA)
    VREG_FORWARD(RegSetValueW)
    VREG_FORWARD(RegDeleteKeyA)
    VREG_FORWARD(RegDeleteKeyW)
    VREG_FORWARD(RegEnumKeyA)
    VREG_FORWARD(RegEnumKeyW)
    VREG_FORWARD(RegSetKeyValueA)
    VREG_FORWARD(RegSetKeyValueW)
    VREG_FORWARD(RegDeleteKeyValueA)
    VREG_FORWARD(RegDeleteKeyValueW)
    VREG_FORWARD(RegQueryMultipleValuesA)
    VREG_FORWARD(RegQueryMultipleValuesW)
};

#undef VREG_FORWARD
} // namespace VReg::Test
