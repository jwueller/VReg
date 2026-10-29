#include <VReg/version.h>

#include "version_info.h"

namespace VReg
{
FileVersion GetFileVersion() noexcept
{
    return {
        .wRevision = VREG_VERSION_TWEAK,
        .wPatch = VREG_VERSION_PATCH,
        .wMinor = VREG_VERSION_MINOR,
        .wMajor = VREG_VERSION_MAJOR,
    };
}

LPCSTR GetVersion() noexcept
{
    return VREG_VERSION;
}
} // namespace VReg
