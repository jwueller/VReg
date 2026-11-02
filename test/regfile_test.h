#pragma once

// Shared pieces of the .reg tests.

#include <VReg/native_registry.h>
#include <VReg/owned_values.h>
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

/// A script from UTF-16 text; fails the test on a parse error.
inline RegFile::Script MustParse(const std::wstring_view text)
{
    RegFile::Script script;
    RegFile::ParseError error;
    const auto file = Utf16File(text);
    REQUIRE(RegFile::Parse(file, script, &error) == ERROR_SUCCESS);
    return script;
}

/// The real registry with a predefined root redirected to a private app hive,
/// so scripts that name `HKEY_CURRENT_USER` act on the hive. Everything else is
/// forwarded unchanged.
class AppHiveRegistry final : public Registry
{
public:
    explicit AppHiveRegistry(const HKEY hRoot) noexcept :
        m_hRoot(hRoot)
    {
    }

    LSTATUS RegOpenKeyExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    ) noexcept override
    {
        return m_native.RegOpenKeyExW(
            Map(hKey),
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
    }

    LSTATUS RegCreateKeyExW(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPWSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    ) noexcept override
    {
        return m_native.RegCreateKeyExW(
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

    LSTATUS RegCloseKey(
        _In_ HKEY hKey
    ) noexcept override
    {
        return m_native.RegCloseKey(
            hKey
        );
    }

    LSTATUS RegQueryValueExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    ) const noexcept override
    {
        return m_native.RegQueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
    }

    LSTATUS RegSetValueExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    ) noexcept override
    {
        return m_native.RegSetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
    }

    LSTATUS RegDeleteValueW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName
    ) noexcept override
    {
        return m_native.RegDeleteValueW(
            hKey,
            lpValueName
        );
    }

    LSTATUS RegDeleteKeyExW(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    ) noexcept override
    {
        return m_native.RegDeleteKeyExW(
            Map(hKey),
            lpSubKey,
            samDesired,
            Reserved
        );
    }

    LSTATUS RegDeleteTreeW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey
    ) noexcept override
    {
        return m_native.RegDeleteTreeW(
            Map(hKey),
            lpSubKey
        );
    }

    LSTATUS RegEnumKeyExW(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept override
    {
        return m_native.RegEnumKeyExW(
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

    LSTATUS RegEnumValueW(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    ) const noexcept override
    {
        return m_native.RegEnumValueW(
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

    LSTATUS RegQueryInfoKeyW(
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept override
    {
        return m_native.RegQueryInfoKeyW(
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

    LSTATUS RegLoadMUIStringW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
    ) const noexcept override
    {
        return m_native.RegLoadMUIStringW(
            Map(hKey),
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
    }

    LSTATUS RegGetValueW(
        _In_ HKEY hkey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
        _When_(
            (dwFlags & 0x7F) == RRF_RT_REG_SZ
            || (dwFlags & 0x7F) == RRF_RT_REG_EXPAND_SZ
            || (dwFlags & 0x7F) == (RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ)
            || *pdwType == REG_SZ
            || *pdwType == REG_EXPAND_SZ,
            _Post_z_
        )
        _When_(
            (dwFlags & 0x7F) == RRF_RT_REG_MULTI_SZ
            || *pdwType == REG_MULTI_SZ,
            _Post_ _NullNull_terminated_
        )
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    ) const noexcept override
    {
        return m_native.RegGetValueW(
            Map(hkey),
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    }

    LSTATUS RegNotifyChangeKeyValue(
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    ) noexcept override
    {
        return m_native.RegNotifyChangeKeyValue(
            Map(hKey),
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
    }

    LSTATUS RegFlushKey(
        _In_ HKEY hKey
    ) noexcept override
    {
        return m_native.RegFlushKey(
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
        registry.RegOpenKeyExW(
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
        registry.RegCreateKeyExW(
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
        registry.RegSetValueExW(
            hKey,
            pszName,
            0,
            dwType,
            reinterpret_cast<const BYTE*>(padded.data()),
            static_cast<DWORD>(data.size())
        ) == ERROR_SUCCESS
    );
    registry.RegCloseKey(
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
        QueryValue(
            registry,
            hKey,
            pszName,
            &value.dwType,
            value.data
        ) == ERROR_SUCCESS
    );
    registry.RegCloseKey(
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
