#pragma once

namespace VReg
{
inline constexpr unsigned kVersionMajor = 0;
inline constexpr unsigned kVersionMinor = 2;
inline constexpr unsigned kVersionPatch = 0;

struct Version
{
    unsigned major;
    unsigned minor;
    unsigned patch;
};

/// The version of the copy of VReg linked into the calling module, which can
/// differ from the headers a caller compiled against, and from the copies
/// other modules in the same process link.
[[nodiscard]] Version GetVersion() noexcept;
} // namespace VReg
