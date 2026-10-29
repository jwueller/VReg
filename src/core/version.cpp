#include <VReg/version.h>

namespace VReg
{
Version GetVersion() noexcept
{
    return {
        .major = VREG_VERSION_MAJOR,
        .minor = VREG_VERSION_MINOR,
        .patch = VREG_VERSION_PATCH,
    };
}
} // namespace VReg
