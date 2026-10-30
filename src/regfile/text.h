#pragma once

#include <VReg/regfile.h>

#include <Windows.h>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace VReg::RegFile
{
/// Decodes a whole file to text without BOM. Detects the encoding as @ref Parse
/// documents. Answers @ref ERROR_INVALID_DATA with @p pError set on bytes the
/// encoding cannot hold.
LSTATUS DecodeFile(
    std::span<const std::byte> file,
    std::wstring& text,
    ParseError* pError
);

/// The bytes of @p text in @p format, with a BOM for Unicode.
/// Answers @ref ERROR_NO_UNICODE_TRANSLATION when the format's code page cannot
/// hold it.
LSTATUS EncodeFile(
    const std::wstring& text,
    Format format,
    std::vector<std::byte>& file
);

/// Reads and writes whole files; Win32 errors answer as is.
LSTATUS ReadWholeFile(
    const std::filesystem::path& path,
    std::vector<std::byte>& file
);

LSTATUS WriteWholeFile(
    const std::filesystem::path& path,
    std::span<const std::byte> file
);
} // namespace VReg::RegFile
