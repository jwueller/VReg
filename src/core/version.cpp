#include <VReg/version.h>

namespace VReg
{
Version GetVersion() noexcept
{
    return {
        .wRevision = 0,
        .wPatch = VREG_VERSION_PATCH,
        .wMinor = VREG_VERSION_MINOR,
        .wMajor = VREG_VERSION_MAJOR,
    };
}
} // namespace VReg
