#include <VReg/version.h>

#include "version_info.h"

namespace VReg
{
Version GetVersion() noexcept
{
    return {
        .wRevision = VREG_VERSION_TWEAK,
        .wPatch = VREG_VERSION_PATCH,
        .wMinor = VREG_VERSION_MINOR,
        .wMajor = VREG_VERSION_MAJOR,
    };
}

LPCSTR GetVersionString() noexcept
{
    return VREG_VERSION_STRING;
}
} // namespace VReg
