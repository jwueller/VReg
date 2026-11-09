#pragma once

// Shared pieces of the .reg tests.

#include <vreg.h>

#include "app_hive_registry.h"
#include "oracle.h"
#include "owned_values.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VRegTest
{
inline std::vector<std::byte> Utf16File(const std::wstring_view text)
{
    std::vector<std::byte> file(2 + text.size() * sizeof(wchar_t));
    file[0] = std::byte{0xFF};
    file[1] = std::byte{0xFE};
    std::memcpy(file.data() + 2, text.data(), text.size() * sizeof(wchar_t));
    return file;
}

/// The raw bytes of @p text, for UTF-8 and ANSI fixtures written as narrow
/// literals.
inline std::vector<std::byte> RawFile(const std::string_view text)
{
    std::vector<std::byte> file(text.size());
    std::memcpy(file.data(), text.data(), text.size());
    return file;
}

inline std::string AsString(const std::vector<std::byte>& file)
{
    return std::string(reinterpret_cast<const char*>(file.data()), file.size());
}

inline std::vector<std::byte> BytesOf(const std::wstring& text)
{
    std::vector<std::byte> bytes(text.size() * sizeof(wchar_t));
    std::memcpy(bytes.data(), text.data(), bytes.size());
    return bytes;
}

/// Terminated UTF-16 bytes, as a REG_SZ holds them.
inline std::vector<std::byte> StringBytes(const std::wstring& text)
{
    return BytesOf(text + L'\0');
}

inline std::vector<std::byte> DwordBytes(const DWORD dwValue)
{
    std::vector<std::byte> bytes(sizeof(dwValue));
    std::memcpy(bytes.data(), &dwValue, sizeof(dwValue));
    return bytes;
}

/// Parses @p file into @p script, which owns the result (null after a failure).
inline LSTATUS Parse(
    const std::span<const std::byte> file,
    OwnedScript& script,
    VRegParseError* const pError
)
{
    return VRegScript_Parse(
        file.data(),
        file.size(),
        script.Receive(),
        pError
    );
}

/// A script from UTF-16 text; fails the test on a parse error.
inline OwnedScript MustParse(const std::wstring_view text)
{
    OwnedScript script;
    VRegParseError error{};
    const auto file = Utf16File(text);
    REQUIRE(Parse(file, script, &error) == ERROR_SUCCESS);
    return script;
}

/// The operations a script holds; valid while the script lives.
inline std::span<const VRegOperation> Operations(
    const VRegScript* const pScript
)
{
    SIZE_T cOperations = 0;
    const VRegOperation* const pOperations = VRegScript_GetOperations(pScript, &cOperations);
    return {pOperations, cOperations};
}

/// Applies a script's operations through @p registry.
inline LSTATUS Apply(
    const VRegScript* const pScript,
    VReg* const registry,
    SIZE_T* const pFailedIndex
)
{
    const auto operations = Operations(pScript);
    return VReg_ApplyOperations(operations.data(), operations.size(), registry, 0, pFailedIndex);
}

inline std::vector<std::byte> DataOf(const VRegOperation& operation)
{
    const auto* const pData = reinterpret_cast<const std::byte*>(operation.pData);
    return pData == nullptr ? std::vector<std::byte>{} : std::vector<std::byte>(pData, pData + operation.cbData);
}

/// Lays @p pScript out in @p format; @p file is left alone on failure.
inline LSTATUS TryWriteBytes(
    const VRegScript* const pScript,
    const VRegFormat format,
    std::vector<std::byte>& file
)
{
    void* pBytes = nullptr;
    SIZE_T cbBytes = 0;
    const auto operations = Operations(pScript);
    const LSTATUS status = VRegScript_Write(
        operations.data(),
        operations.size(),
        format,
        &pBytes,
        &cbBytes
    );
    if (status != ERROR_SUCCESS)
        return status;

    const auto* const pFirst = static_cast<const std::byte*>(pBytes);
    file.assign(pFirst, pFirst + cbBytes);
    VReg_Free(pBytes);
    return ERROR_SUCCESS;
}

inline std::vector<std::byte> WriteBytes(
    const VRegScript* const pScript,
    const VRegFormat format
)
{
    std::vector<std::byte> file;
    REQUIRE(TryWriteBytes(pScript, format, file) == ERROR_SUCCESS);
    return file;
}

/// A snapshot of @p pszPath below @p hRoot as file bytes; @p file is left
/// alone on failure.
inline LSTATUS TryExportSnapshot(
    VReg* const registry,
    const HKEY hRoot,
    const LPCWSTR pszPath,
    const VRegFormat format,
    std::vector<std::byte>& file
)
{
    OwnedScript script;
    const LSTATUS status = VRegScript_CreateSnapshot(
        registry,
        hRoot,
        pszPath,
        script.Receive()
    );
    if (status != ERROR_SUCCESS)
        return status;
    return TryWriteBytes(script, format, file);
}

inline std::vector<std::byte> ExportSnapshot(
    VReg* const registry,
    const HKEY hRoot,
    const LPCWSTR pszPath,
    const VRegFormat format
)
{
    std::vector<std::byte> file;
    REQUIRE(TryExportSnapshot(registry, hRoot, pszPath, format, file) == ERROR_SUCCESS);
    return file;
}

/// The delta of @p hive mounted at @p hRoot and @p pszPath as file bytes;
/// @p file is left alone on failure.
inline LSTATUS TryExportDelta(
    VRegHive* const hive,
    const HKEY hRoot,
    const LPCWSTR pszPath,
    const VRegFormat format,
    std::vector<std::byte>& file
)
{
    OwnedScript script;
    const LSTATUS status = VRegScript_CreateDelta(
        hive,
        hRoot,
        pszPath,
        script.Receive()
    );
    if (status != ERROR_SUCCESS)
        return status;
    return TryWriteBytes(script, format, file);
}

inline std::vector<std::byte> ExportDelta(
    VRegHive* const hive,
    const HKEY hRoot,
    const LPCWSTR pszPath,
    const VRegFormat format
)
{
    std::vector<std::byte> file;
    REQUIRE(TryExportDelta(hive, hRoot, pszPath, format, file) == ERROR_SUCCESS);
    return file;
}

/// A value as a registry holds it.
struct RegValue
{
    DWORD dwType{};
    std::vector<std::byte> data;

    friend bool operator==(const RegValue&, const RegValue&) = default;
};

/// Opens @p path below HKEY_CURRENT_USER, which the registry maps to its hive.
inline HKEY MustOpen(
    VReg* const registry,
    const wchar_t* const pszPath,
    const REGSAM samDesired = KEY_READ
)
{
    HKEY hKey{};
    REQUIRE(
        VReg_RegOpenKeyExW(
            registry,
            HKEY_CURRENT_USER,
            pszPath,
            0,
            samDesired,
            &hKey
        ) == ERROR_SUCCESS
    );
    return hKey;
}

/// Sets a value on the existing or new key @p pszPath.
inline void MustSet(
    VReg* const registry,
    const wchar_t* const pszPath,
    const wchar_t* const pszName,
    const DWORD dwType,
    const std::vector<std::byte>& data
)
{
    HKEY hKey{};
    REQUIRE(
        VReg_RegCreateKeyExW(
            registry,
            HKEY_CURRENT_USER,
            pszPath,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &hKey,
            nullptr
        ) == ERROR_SUCCESS
    );
    // Native looks past the end of string data for a terminator it may add, so
    // what follows must not be zero or the stored bytes depend on the heap.
    std::vector<std::byte> padded(data);
    padded.insert(padded.end(), 4, std::byte{0x11});
    REQUIRE(
        VReg_RegSetValueExW(
            registry,
            hKey,
            pszName,
            0,
            dwType,
            reinterpret_cast<const BYTE*>(padded.data()),
            static_cast<DWORD>(data.size())
        ) == ERROR_SUCCESS
    );
    VReg_RegCloseKey(
        registry,
        hKey
    );
}

/// Reads a value back; fails the test when it is missing.
inline RegValue MustGet(
    VReg* const registry,
    const wchar_t* const pszPath,
    const wchar_t* const pszName
)
{
    const HKEY hKey = MustOpen(registry, pszPath);
    RegValue value;
    REQUIRE(
        QueryValue(
            registry,
            hKey,
            pszName,
            &value.dwType,
            value.data
        ) == ERROR_SUCCESS
    );
    VReg_RegCloseKey(
        registry,
        hKey
    );
    return value;
}
} // namespace VRegTest

namespace VRegTest
{
/// Checks a stored value's type and bytes.
inline void CheckValue(
    VReg* const registry,
    const wchar_t* const pszPath,
    const wchar_t* const pszName,
    const DWORD dwType,
    const std::vector<std::byte>& data
)
{
    const RegValue value = MustGet(registry, pszPath, pszName);
    CHECK(value.dwType == dwType);
    CHECK(value.data == data);
}
} // namespace VRegTest
