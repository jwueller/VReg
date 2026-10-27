#include <VReg/version.h>

namespace VReg
{
Version GetVersion() noexcept
{
    return {
        .major = kVersionMajor,
        .minor = kVersionMinor,
        .patch = kVersionPatch,
    };
}
} // namespace VReg
