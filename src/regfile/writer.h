#pragma once

#include <VReg/regfile.h>

#include <Windows.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace VReg::RegFile
{
/// One line group of an output file. Every export reduces to a list of these,
/// so an export of another source only has to produce records.
struct Record
{
    enum class Kind
    {
        /// `[key]`
        Key,
        /// `[-key]`
        DeleteKey,
        /// `"name"=data`, under the most recent Key record.
        Value,
        /// `"name"=-`, under the most recent Key record.
        DeleteValue,
    } kind{};

    /// The full key path with the root's long name.
    std::wstring key;

    std::wstring name;
    DWORD dwType{};
    std::vector<std::byte> data;
};

/// Lays out @p records as a regedit-style file in @p format.
/// Answers @ref ERROR_NO_UNICODE_TRANSLATION or @ref ERROR_INVALID_NAME as
/// @ref ExportSnapshot documents.
LSTATUS WriteRecords(
    std::span<const Record> records,
    Format format,
    std::vector<std::byte>& file
);
} // namespace VReg::RegFile
