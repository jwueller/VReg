#pragma once

// Shared pieces of the .reg tests.

#include <VReg/native_registry.h>
#include <VReg/regfile.h>

#include "oracle.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace VReg::Test
{
inline std::vector<std::byte> Utf16File(const std::wstring_view text)
{
    std::vector<std::byte> file(2 + text.size() * sizeof(wchar_t));
    file[0] = std::byte{0xFF};
    file[1] = std::byte{0xFE};
    std::memcpy(file.data() + 2, text.data(), text.size() * sizeof(wchar_t));
    return file;
}

/// The raw bytes of @p text, for UTF-8 and ANSI fixtures written as narrow literals.
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

/// A script from UTF-16 text; fails the test on a parse error.
inline RegFile::Script MustParse(const std::wstring_view text)
{
    RegFile::Script script;
    RegFile::ParseError error;
    const auto file = Utf16File(text);
    REQUIRE(RegFile::Parse(file, script, &error) == ERROR_SUCCESS);
    return script;
}

/// The real registry with a predefined root redirected to a private app hive, so scripts that name
/// `HKEY_CURRENT_USER` act on the hive. Everything else is forwarded unchanged.
class AppHiveRegistry final : public Registry
{
public:
    explicit AppHiveRegistry(const HKEY hRoot) noexcept :
        m_hRoot(hRoot)
    {
    }

    LSTATUS OpenKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD ulOptions,
        REGSAM samDesired,
        PHKEY phkResult
    ) noexcept override
    {
        return m_native.OpenKeyExW(
            Map(hKey),
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
    }

    LSTATUS CreateKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD Reserved,
        LPWSTR lpClass,
        DWORD dwOptions,
        REGSAM samDesired,
        const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        PHKEY phkResult,
        LPDWORD lpdwDisposition
    ) noexcept override
    {
        return m_native.CreateKeyExW(
            Map(hKey),
            lpSubKey,
            Reserved,
            lpClass,
            dwOptions,
            samDesired,
            lpSecurityAttributes,
            phkResult,
            lpdwDisposition
        );
    }

    LSTATUS CloseKey(HKEY hKey) noexcept override
    {
        return m_native.CloseKey(
            hKey
        );
    }

    LSTATUS QueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept override
    {
        return m_native.QueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }

    LSTATUS SetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept override
    {
        return m_native.SetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS DeleteValueW(HKEY hKey, LPCWSTR lpValueName) noexcept override
    {
        return m_native.DeleteValueW(
            hKey,
            lpValueName
        );
    }

    LSTATUS DeleteKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        REGSAM samDesired,
        DWORD Reserved
    ) noexcept override
    {
        return m_native.DeleteKeyExW(
            Map(hKey),
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    LSTATUS DeleteTreeW(HKEY hKey, LPCWSTR lpSubKey) noexcept override
    {
        return m_native.DeleteTreeW(
            Map(hKey),
            lpSubKey
        );
    }

    LSTATUS EnumKeyExW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) noexcept override
    {
        return m_native.EnumKeyExW(
            Map(hKey),
            dwIndex,
            lpName,
            lpcchName,
            lpReserved,
            lpClass,
            lpcchClass,
            lpftLastWriteTime
        );
    }

    LSTATUS EnumValueW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept override
    {
        return m_native.EnumValueW(
            Map(hKey),
            dwIndex,
            lpValueName,
            lpcchValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }

    LSTATUS QueryInfoKeyW(
        HKEY hKey,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        LPDWORD lpReserved,
        LPDWORD lpcSubKeys,
        LPDWORD lpcbMaxSubKeyLen,
        LPDWORD lpcbMaxClassLen,
        LPDWORD lpcValues,
        LPDWORD lpcbMaxValueNameLen,
        LPDWORD lpcbMaxValueLen,
        LPDWORD lpcbSecurityDescriptor,
        PFILETIME lpftLastWriteTime
    ) noexcept override
    {
        return m_native.QueryInfoKeyW(
            Map(hKey),
            lpClass,
            lpcchClass,
            lpReserved,
            lpcSubKeys,
            lpcbMaxSubKeyLen,
            lpcbMaxClassLen,
            lpcValues,
            lpcbMaxValueNameLen,
            lpcbMaxValueLen,
            lpcbSecurityDescriptor,
            lpftLastWriteTime
        );
    }

    LSTATUS LoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    ) noexcept override
    {
        return m_native.LoadMUIStringW(
            Map(hKey),
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
    }

    LSTATUS GetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) noexcept override
    {
        return m_native.GetValueW(
            Map(hkey),
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    }

    LSTATUS NotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept override
    {
        return m_native.NotifyChangeKeyValue(
            Map(hKey),
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }

    LSTATUS FlushKey(HKEY hKey) noexcept override
    {
        return m_native.FlushKey(
            Map(hKey)
        );
    }

private:
    HKEY Map(const HKEY hKey) const noexcept
    {
        return hKey == HKEY_CURRENT_USER ? m_hRoot : hKey;
    }

    NativeRegistry m_native;
    HKEY m_hRoot;
};

/// A value as a registry holds it.
struct RegValue
{
    DWORD dwType{};
    std::vector<std::byte> data;

    friend bool operator==(const RegValue&, const RegValue&) = default;
};

/// Opens @p path below HKEY_CURRENT_USER, which the registry maps to its hive.
inline HKEY MustOpen(
    Registry& registry,
    const wchar_t* const pszPath,
    const REGSAM samDesired = KEY_READ
)
{
    HKEY hKey{};
    REQUIRE(
        registry.OpenKeyExW(
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
    Registry& registry,
    const wchar_t* const pszPath,
    const wchar_t* const pszName,
    const DWORD dwType,
    const std::vector<std::byte>& data
)
{
    HKEY hKey{};
    REQUIRE(
        registry
            .CreateKeyExW(
                HKEY_CURRENT_USER,
                pszPath,
                0,
                nullptr,
                0,
                KEY_SET_VALUE,
                nullptr,
                &hKey,
                nullptr
            ) ==
        ERROR_SUCCESS
    );
    // Native looks past the end of string data for a terminator it may add, so what follows must not be
    // zero or the stored bytes depend on the heap.
    std::vector<std::byte> padded(data);
    padded.insert(padded.end(), 4, std::byte{0x11});
    REQUIRE(
        registry.SetValueExW(
            hKey,
            pszName,
            0,
            dwType,
            reinterpret_cast<const BYTE*>(padded.data()),
            static_cast<DWORD>(data.size())
        ) == ERROR_SUCCESS
    );
    registry.CloseKey(
        hKey
    );
}

/// Reads a value back; fails the test when it is missing.
inline RegValue MustGet(
    Registry& registry,
    const wchar_t* const pszPath,
    const wchar_t* const pszName
)
{
    const HKEY hKey = MustOpen(registry, pszPath);
    RegValue value;
    REQUIRE(
        registry.QueryValue(
            hKey,
            pszName,
            &value.dwType,
            value.data
        ) == ERROR_SUCCESS
    );
    registry.CloseKey(
        hKey
    );
    return value;
}
} // namespace VReg::Test

namespace VReg::Test
{
/// Checks a stored value's type and bytes.
inline void CheckValue(
    Registry& registry,
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
} // namespace VReg::Test
