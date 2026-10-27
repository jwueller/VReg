// The owned-value forms of Registry, over NativeRegistry on a private app
// hive.

#include <VReg/native_registry.h>

#include "oracle.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace VReg;
using namespace VReg::Test;

namespace
{
std::wstring WideFromBytes(const std::vector<std::byte>& bytes)
{
    std::wstring text(bytes.size() / sizeof(wchar_t), L'\0');
    std::memcpy(text.data(), bytes.data(), text.size() * sizeof(wchar_t));
    return text;
}

LSTATUS SetString(
    Registry& registry,
    const HKEY hKey,
    const wchar_t* const pszName,
    const DWORD dwType,
    const std::wstring& text
)
{
    return registry.SetValueExW(
        hKey,
        pszName,
        0,
        dwType,
        reinterpret_cast<const BYTE*>(text.c_str()),
        static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t))
    );
}

LSTATUS SetDword(
    Registry& registry,
    const HKEY hKey,
    const wchar_t* const pszName,
    const DWORD dwValue
)
{
    return registry.SetValueExW(
        hKey,
        pszName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&dwValue),
        sizeof(dwValue)
    );
}

// The first call that reads data answers that the value grew, to exercise the
// retry of the owned-value forms.
int g_nGrowthsLeft = 0;

LSTATUS WINAPI GrowingQueryValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
)
{
    if (lpData != nullptr && lpcbData != nullptr && g_nGrowthsLeft > 0)
    {
        --g_nGrowthsLeft;
        const auto status = ::RegQueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            nullptr,
            lpcbData
        );
        if (status == ERROR_SUCCESS)
        {
            *lpcbData += 16;
            return ERROR_MORE_DATA;
        }
        return status;
    }
    return ::RegQueryValueExW(
        hKey,
        lpValueName,
        lpReserved,
        lpType,
        lpData,
        lpcbData
    );
}

LSTATUS WINAPI GrowingGetValueW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    const PVOID pvData,
    const LPDWORD pcbData
)
{
    if (pvData != nullptr && pcbData != nullptr && g_nGrowthsLeft > 0)
    {
        --g_nGrowthsLeft;
        const auto status = ::RegGetValueW(
            hKey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            nullptr,
            pcbData
        );
        if (status == ERROR_SUCCESS)
        {
            *pcbData += 16;
            return ERROR_MORE_DATA;
        }
        return status;
    }
    return ::RegGetValueW(
        hKey,
        lpSubKey,
        lpValue,
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );
}

NativeRegistry GrowingRegistry()
{
    auto functions = NativeRegistry::ResolveFunctions();
    functions.RegQueryValueExW = &GrowingQueryValueExW;
    functions.RegGetValueW = &GrowingGetValueW;
    return NativeRegistry(functions);
}
} // namespace

TEST_CASE("QueryValue returns the type and the bytes")
{
    AppHive hive;
    NativeRegistry registry;

    REQUIRE(
        SetDword(registry, hive.Root(), L"Dword", 0x12345678) == ERROR_SUCCESS
    );
    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Text",
            REG_SZ,
            L"hello"
        ) == ERROR_SUCCESS
    );
    REQUIRE(
        registry.SetValueExW(
            hive.Root(),
            L"Empty",
            0,
            REG_BINARY,
            nullptr,
            0
        ) == ERROR_SUCCESS
    );

    DWORD dwType = 0;
    std::vector<std::byte> data;
    CHECK(
        registry.QueryValue(
            hive.Root(),
            L"Dword",
            &dwType,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(dwType == REG_DWORD);
    REQUIRE(data.size() == 4);
    CHECK(data[0] == std::byte{0x78});
    CHECK(data[3] == std::byte{0x12});

    CHECK(
        registry.QueryValue(
            hive.Root(),
            L"Text",
            nullptr,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(WideFromBytes(data) == std::wstring(L"hello\0", 6));

    dwType = 0;
    CHECK(
        registry.QueryValue(
            hive.Root(),
            L"Empty",
            &dwType,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(dwType == REG_BINARY);
    CHECK(data.empty());

    CHECK(
        registry.QueryValue(
            hive.Root(),
            L"Missing",
            &dwType,
            data
        ) == ERROR_FILE_NOT_FOUND
    );
}

TEST_CASE("QueryValue reads a value that grew after it was sized")
{
    AppHive hive;
    auto registry = GrowingRegistry();

    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Text",
            REG_SZ,
            L"hello"
        ) == ERROR_SUCCESS
    );

    g_nGrowthsLeft = 2;
    std::vector<std::byte> data;
    CHECK(
        registry.QueryValue(
            hive.Root(),
            L"Text",
            nullptr,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(g_nGrowthsLeft == 0);
    CHECK(WideFromBytes(data) == std::wstring(L"hello\0", 6));
}

TEST_CASE("GetValue applies the flags")
{
    AppHive hive;
    NativeRegistry registry;

    REQUIRE(
        registry.SetKeyValueW(
            hive.Root(),
            L"Sub",
            L"Dword",
            REG_DWORD,
            std::array<DWORD,
            1>{7}.data(),
            4
        )
        == ERROR_SUCCESS
    );
    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Expand",
            REG_EXPAND_SZ,
            L"%OS%"
        ) == ERROR_SUCCESS
    );

    DWORD dwType = 0;
    std::vector<std::byte> data;
    CHECK(
        registry.GetValue(
            hive.Root(),
            L"Sub",
            L"Dword",
            RRF_RT_REG_DWORD,
            &dwType,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(dwType == REG_DWORD);
    CHECK(data.size() == 4);

    CHECK(
        registry.GetValue(
            hive.Root(),
            L"Sub",
            L"Dword",
            RRF_RT_REG_SZ,
            &dwType,
            data
        ) == ERROR_UNSUPPORTED_TYPE
    );
    CHECK(
        registry.GetValue(
            hive.Root(),
            L"Sub",
            L"Missing",
            RRF_RT_ANY,
            &dwType,
            data
        ) == ERROR_FILE_NOT_FOUND
    );

    CHECK(
        registry.GetValue(
            hive.Root(),
            nullptr,
            L"Expand",
            RRF_RT_ANY,
            &dwType,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(dwType == REG_SZ);
    CHECK(WideFromBytes(data) == std::wstring(L"Windows_NT\0", 11));

    CHECK(
        registry.GetValue(
            hive.Root(),
            nullptr,
            L"Expand",
            RRF_RT_ANY | RRF_NOEXPAND,
            &dwType,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(dwType == REG_EXPAND_SZ);
    CHECK(WideFromBytes(data) == std::wstring(L"%OS%\0", 5));
}

TEST_CASE("GetValue reads a value that grew after it was sized")
{
    AppHive hive;
    auto registry = GrowingRegistry();

    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Text",
            REG_SZ,
            L"hello"
        ) == ERROR_SUCCESS
    );

    g_nGrowthsLeft = 2;
    std::vector<std::byte> data;
    CHECK(
        registry.GetValue(
            hive.Root(),
            nullptr,
            L"Text",
            RRF_RT_ANY,
            nullptr,
            data
        ) == ERROR_SUCCESS
    );
    CHECK(g_nGrowthsLeft == 0);
    CHECK(WideFromBytes(data) == std::wstring(L"hello\0", 6));
}

TEST_CASE("GetString returns the text without its terminator")
{
    AppHive hive;
    NativeRegistry registry;

    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Text",
            REG_SZ,
            L"hello"
        ) == ERROR_SUCCESS
    );
    REQUIRE(
        SetString(registry, hive.Root(), L"Empty", REG_SZ, L"") == ERROR_SUCCESS
    );
    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Expand",
            REG_EXPAND_SZ,
            L"%OS%"
        ) == ERROR_SUCCESS
    );
    REQUIRE(SetDword(registry, hive.Root(), L"Dword", 1) == ERROR_SUCCESS);
    const std::wstring embedded(L"a\0b", 3);
    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Embedded",
            REG_SZ,
            embedded
        ) == ERROR_SUCCESS
    );

    std::wstring text = L"unchanged";
    CHECK(
        registry.GetString(
            hive.Root(),
            nullptr,
            L"Text",
            RRF_RT_ANY,
            text
        ) == ERROR_SUCCESS
    );
    CHECK(text == L"hello");

    CHECK(
        registry.GetString(
            hive.Root(),
            nullptr,
            L"Empty",
            0,
            text
        ) == ERROR_SUCCESS
    );
    CHECK(text.empty());

    CHECK(
        registry.GetString(
            hive.Root(),
            nullptr,
            L"Embedded",
            0,
            text
        ) == ERROR_SUCCESS
    );
    CHECK(text == embedded);

    CHECK(
        registry.GetString(
            hive.Root(),
            nullptr,
            L"Expand",
            0,
            text
        ) == ERROR_SUCCESS
    );
    CHECK(text == L"Windows_NT");

    CHECK(
        registry.GetString(
            hive.Root(),
            nullptr,
            L"Expand",
            RRF_NOEXPAND,
            text
        ) == ERROR_SUCCESS
    );
    CHECK(text == L"%OS%");

    text = L"unchanged";
    CHECK(
        registry.GetString(
            hive.Root(),
            nullptr,
            L"Dword",
            0,
            text
        ) == ERROR_UNSUPPORTED_TYPE
    );
    CHECK(text == L"unchanged");
    CHECK(
        registry.GetString(
            hive.Root(),
            nullptr,
            L"Missing",
            0,
            text
        ) == ERROR_FILE_NOT_FOUND
    );
}

TEST_CASE("EnumKey walks the subkeys to the end")
{
    AppHive hive;
    NativeRegistry registry;

    const std::wstring longName(255, L'k');
    for (const auto& name : {std::wstring(L"Alpha"), std::wstring(L"B\u00e4ta\u4e2d"), longName})
    {
        HKEY hKey{};
        REQUIRE(
            registry.CreateKeyExW(
                hive.Root(),
                name.c_str(),
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            )
            == ERROR_SUCCESS
        );
        registry.CloseKey(
            hKey
        );
    }

    std::vector<std::wstring> names;
    std::wstring name;
    DWORD dwIndex = 0;
    while (registry.EnumKey(hive.Root(), dwIndex++, name) == ERROR_SUCCESS)
        names.push_back(name);

    REQUIRE(names.size() == 3);
    CHECK(registry.EnumKey(hive.Root(), 3, name) == ERROR_NO_MORE_ITEMS);
    CHECK((names[0] == L"Alpha" || names[1] == L"Alpha" || names[2] == L"Alpha"));
    CHECK((names[0] == L"B\u00e4ta\u4e2d" || names[1] == L"B\u00e4ta\u4e2d" || names[2] == L"B\u00e4ta\u4e2d"));
    CHECK((names[0] == longName || names[1] == longName || names[2] == longName));
}

TEST_CASE("EnumValue returns the name, type and data")
{
    AppHive hive;
    NativeRegistry registry;

    const std::wstring longName(1000, L'v');
    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            longName.c_str(),
            REG_SZ,
            std::wstring(500, L'x')
        ) == ERROR_SUCCESS
    );

    std::wstring name;
    DWORD dwType = 0;
    std::vector<std::byte> data;
    CHECK(
        registry.EnumValue(
            hive.Root(),
            0,
            name,
            &dwType,
            &data
        ) == ERROR_SUCCESS
    );
    CHECK(name == longName);
    CHECK(dwType == REG_SZ);
    CHECK(
        WideFromBytes(data) == std::wstring(500, L'x') + std::wstring(1, L'\0')
    );

    name.clear();
    dwType = 0;
    CHECK(
        registry.EnumValue(
            hive.Root(),
            0,
            name,
            &dwType,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(name == longName);
    CHECK(dwType == REG_SZ);

    CHECK(
        registry.EnumValue(
            hive.Root(),
            1,
            name,
            nullptr,
            &data
        ) == ERROR_NO_MORE_ITEMS
    );
}

TEST_CASE("EnumValue returns the default value under an empty name")
{
    AppHive hive;
    NativeRegistry registry;

    REQUIRE(SetDword(registry, hive.Root(), nullptr, 5) == ERROR_SUCCESS);

    std::wstring name = L"unchanged";
    std::vector<std::byte> data;
    DWORD dwType = 0;
    CHECK(
        registry.EnumValue(
            hive.Root(),
            0,
            name,
            &dwType,
            &data
        ) == ERROR_SUCCESS
    );
    CHECK(name.empty());
    CHECK(dwType == REG_DWORD);
    CHECK(data.size() == 4);
}

namespace
{
const std::wstring g_muiText(700, L'm');

LSTATUS WINAPI StubLoadMUIStringW(
    const HKEY hKey,
    const LPCWSTR pszValue,
    const LPWSTR pszOutBuf,
    const DWORD cbOutBuf,
    const LPDWORD pcbData,
    const DWORD Flags,
    const LPCWSTR pszDirectory
)
{
    (void)hKey;
    (void)pszValue;
    (void)Flags;
    (void)pszDirectory;
    const auto cbNeeded = static_cast<DWORD>((g_muiText.size() + 1) * sizeof(wchar_t));
    *pcbData = cbNeeded;
    if (cbOutBuf < cbNeeded)
        return ERROR_MORE_DATA;

    std::memcpy(pszOutBuf, g_muiText.c_str(), cbNeeded);
    return ERROR_SUCCESS;
}
} // namespace

TEST_CASE("LoadMUIString grows its buffer to the size it is told")
{
    auto functions = NativeRegistry::ResolveFunctions();
    functions.RegLoadMUIStringW = &StubLoadMUIStringW;
    NativeRegistry registry(functions);

    std::wstring text;
    CHECK(
        registry.LoadMUIString(
            nullptr,
            L"Value",
            0,
            nullptr,
            text
        ) == ERROR_SUCCESS
    );
    CHECK(text == g_muiText);
}

TEST_CASE("LoadMUIString resolves a resource reference")
{
    AppHive hive;
    NativeRegistry registry;

    REQUIRE(
        SetString(
            registry,
            hive.Root(),
            L"Indirect",
            REG_SZ,
            L"@%SystemRoot%\\System32\\shell32.dll,-21770"
        )
        == ERROR_SUCCESS
    );

    std::wstring text;
    CHECK(
        registry.LoadMUIString(
            hive.Root(),
            L"Indirect",
            0,
            nullptr,
            text
        ) == ERROR_SUCCESS
    );
    CHECK_FALSE(text.empty());
    CHECK(text.find(L'\0') == std::wstring::npos);

    CHECK(
        registry.LoadMUIString(
            hive.Root(),
            L"Missing",
            0,
            nullptr,
            text
        ) == ERROR_FILE_NOT_FOUND
    );
}
