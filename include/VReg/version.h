#pragma once

#include <Windows.h>

#include <bit>
#include <compare>

namespace VReg
{
/// @name The version these headers declare
/// Each fits a 16-bit field of a module's version resource
/// (@ref VS_FIXEDFILEINFO).
/// @{
inline constexpr WORD VREG_VERSION_MAJOR = 0;
inline constexpr WORD VREG_VERSION_MINOR = 2;
inline constexpr WORD VREG_VERSION_PATCH = 0;
/// @}

/**
 * @brief A four-part version number, laid out in memory as the packed
 *  64-bit form Windows uses for one.
 *
 *  @ref MAKEDLLVERULL packs major, minor, build and revision into a
 *  @ref ULONGLONG from the high word down, the value a version resource's
 *  `dwFileVersionMS:dwFileVersionLS` reads as. The fields are declared low
 *  word first, so `std::bit_cast<ULONGLONG>` of a version is that number,
 *  and versions compare as it does. The patch takes the build's place.
 */
struct Version
{
    WORD wRevision;
    WORD wPatch;
    WORD wMinor;
    WORD wMajor;

    [[nodiscard]] friend constexpr bool operator==(
        const Version& left,
        const Version& right
    ) noexcept = default;

    [[nodiscard]] friend constexpr std::strong_ordering operator<=>(
        const Version& left,
        const Version& right
    ) noexcept
    {
        return std::bit_cast<ULONGLONG>(left) <=> std::bit_cast<ULONGLONG>(right);
    }
};

static_assert(sizeof(Version) == sizeof(ULONGLONG));

/// The version of the copy of VReg linked into the calling module, which can
/// differ from the headers a caller compiled against, and from the copies
/// other modules in the same process link. Its revision is always 0.
[[nodiscard]] Version GetVersion() noexcept;
} // namespace VReg
