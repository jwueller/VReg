#pragma once

#include "registry.h"

#include <Windows.h>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VReg
{
class OverlayHive;
}

namespace VReg::RegFile
{
/// The encodings and headers a `.reg` file is written in.
enum class Format
{
    /// `Windows Registry Editor Version 5.00` as UTF-16LE with a BOM: what
    /// regedit writes.
    Unicode,
    /// The same header as UTF-8 without a BOM.
    Utf8,
    /// `REGEDIT4` in @ref CP_ACP.
    Regedit4,
};

enum class OperationKind
{
    CreateKey,
    DeleteKey,
    SetValue,
    DeleteValue,
};

/// One line of a `.reg` file that does something. Value operations act on the
/// key most recently created by the file; each carries that key so it stands
/// alone.
struct Operation
{
    OperationKind kind{};
    /// A predefined root such as @ref HKEY_LOCAL_MACHINE.
    HKEY hRoot{};
    /// Below @ref hRoot, without a leading backslash; empty for the root
    /// itself.
    std::wstring path;
    /// Empty for the default value (`@`).
    std::wstring name;
    DWORD dwType{};
    std::vector<std::byte> data;
    /// 1-based line of the file the operation starts on.
    std::size_t line{};
};

/// The operations of a file, in file order.
struct Script
{
    std::vector<Operation> operations;
};

/// Where and why a file failed to parse.
struct ParseError
{
    /// 1-based, in UTF-16 code units of the decoded text.
    std::size_t line{};
    std::size_t column{};
    /// A static string.
    const char* pszMessage{};
};

/**
 * @brief Parses a whole file.
 *
 *  The encoding is taken from a BOM (UTF-16LE, UTF-8); without one, a
 *  `REGEDIT4` header means @ref CP_ACP and anything else UTF-8. @p script is
 *  only written when the whole file parses.
 *
 * @return @ref ERROR_SUCCESS, @ref ERROR_INVALID_DATA on a syntax or encoding
 *  error (described by @p pError, which MAY be null), or
 *  @ref ERROR_OUTOFMEMORY.
 */
LSTATUS Parse(
    std::span<const std::byte> file,
    Script& script,
    ParseError* pError
) noexcept;

/// Reads @p path, then @ref Parse. Failures to read answer the Win32 error.
LSTATUS ParseFile(
    const std::filesystem::path& path,
    Script& script,
    ParseError* pError
) noexcept;

/**
 * @brief Performs @p script through @p registry, in order.
 *
 *  Stops at the first operation that fails, stores its index in @p pFailed (MAY
 *  be null) and returns its status; earlier operations stay applied. Deleting a
 *  key or value that does not exist is not a failure.
 */
LSTATUS Apply(
    const Script& script,
    Registry& registry,
    std::size_t* pFailed
) noexcept;

/**
 * @brief Writes the key @p path below the predefined root @p hRoot and
 *  everything below it.
 *
 *  Keys and values are sorted ordinal case-insensitive. The output reproduces
 *  every value's bytes when parsed and applied. @p file is only written on
 *  success.
 *
 * @return @ref ERROR_NO_UNICODE_TRANSLATION when @p format cannot represent a
 *  name or string, @ref ERROR_INVALID_NAME for a name that holds a line break
 *  (no format can write one), @ref ERROR_INVALID_HANDLE when @p hRoot is not a
 *  predefined root, otherwise the status of the registry calls.
 */
LSTATUS ExportSnapshot(
    Registry& registry,
    HKEY hRoot,
    std::wstring_view path,
    Format format,
    std::vector<std::byte>& file
) noexcept;

/// @ref ExportSnapshot into a file. An existing file is replaced only once the
/// export has succeeded.
LSTATUS ExportSnapshotToFile(
    Registry& registry,
    HKEY hRoot,
    std::wstring_view path,
    Format format,
    const std::filesystem::path& fileName
) noexcept;

/**
 * @brief Writes what the upper layer of @p hive holds, as a file that
 *  reproduces the hive's merged view when it is applied over the same lower
 *  registry.
 *
 *  @p hRoot and @p path name where the hive is mounted: key names in the file
 *  are that prefix plus the key's path in the hive. Upper keys are written as
 *  `[key]` with their upper values, a deleted lower value as `"name"=-`, a
 *  deleted lower key as `[-key]`, and a key that hides its lower counterpart as
 *  `[-key]` followed by `[key]` and its contents. A key that only shows a lower
 *  key through is left out. Order and layout are those of @ref ExportSnapshot.
 *  A tombstone with no lower counterpart is kept, and does nothing when
 *  applied.
 *
 * @return As @ref ExportSnapshot, except that the hive is read instead of a
 *  registry.
 */
LSTATUS ExportDelta(
    OverlayHive& hive,
    HKEY hRoot,
    std::wstring_view path,
    Format format,
    std::vector<std::byte>& file
) noexcept;

/// @ref ExportDelta into a file. An existing file is replaced only once the
/// export has succeeded.
LSTATUS ExportDeltaToFile(
    OverlayHive& hive,
    HKEY hRoot,
    std::wstring_view path,
    Format format,
    const std::filesystem::path& fileName
) noexcept;
} // namespace VReg::RegFile
