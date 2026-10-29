#pragma once

#include <Windows.h>

#include <bit>
#include <compare>

namespace VReg
{
/**
 * @brief A four-part version number, laid out in memory as the packed
 *  64-bit form Windows uses for one.
 *
 *  @ref MAKEDLLVERULL packs major, minor, build and revision into a
 *  @ref ULONGLONG from the high word down, the value a version resource's
 *  `dwFileVersionMS:dwFileVersionLS` reads as. The fields are declared low
 *  word first, so `std::bit_cast<ULONGLONG>` of a version is that number,
 *  and versions compare as it does. The patch takes the build's place, and
 *  the revision counts the commits in the history built from, so the number
 *  orders every build. Each part fits a 16-bit field of a version resource
 *  (@ref VS_FIXEDFILEINFO).
 */
struct FileVersion
{
    WORD wRevision;
    WORD wPatch;
    WORD wMinor;
    WORD wMajor;

    [[nodiscard]] friend constexpr bool operator==(
        const FileVersion& left,
        const FileVersion& right
    ) noexcept = default;

    [[nodiscard]] friend constexpr std::strong_ordering operator<=>(
        const FileVersion& left,
        const FileVersion& right
    ) noexcept
    {
        return std::bit_cast<ULONGLONG>(left) <=> std::bit_cast<ULONGLONG>(right);
    }
};

// The field order holds for a little-endian target, which every Windows one
// is; a big-endian one would have to reverse it.
static_assert(
    std::bit_cast<ULONGLONG>(
        FileVersion{
            .wRevision = 4,
            .wPatch = 3,
            .wMinor = 2,
            .wMajor = 1,
        }
    )
    == 0x0001'0002'0003'0004
);

/// The version of the copy of VReg linked into the calling module, which can
/// differ from the copies other modules in the same process link: a semver
/// string with the full git commit it was built from as build metadata. The
/// released commit, built unchanged, is the release
/// (`0.3.0+0123456789abcdef0123456789abcdef01234567`); any other build is a
/// preview of it that counts the commits since the last release and marks
/// local changes (`0.3.0-preview.5+0123...4567-dirty`), so previews sort
/// below their release. A build outside a git checkout, as from a release
/// archive, is the version alone unless the `VREG_SOURCE_COMMIT` CMake
/// variable gives the commit.
[[nodiscard]] LPCSTR GetVersion() noexcept;

/// The numbers of @ref GetVersion, for comparing builds and for version
/// resources; its revision is 0 for a build outside a git checkout. At build
/// time, `find_package(VReg)` reports the same major, minor and patch.
[[nodiscard]] FileVersion GetFileVersion() noexcept;
} // namespace VReg
