// Export, parse and apply into a second hive must reproduce every value byte for byte.

#include "regfile_test.h"

#include <catch2/generators/catch_generators.hpp>

using namespace VReg;
using namespace VReg::RegFile;
using namespace VReg::Test;

namespace
{
struct Entry
{
    const wchar_t* pszKey;
    std::wstring name;
    RegValue value;
};

std::vector<std::byte> Counting(const int count)
{
    std::vector<std::byte> bytes;
    for (int i = 0; i < count; ++i)
        bytes.push_back(static_cast<std::byte>(i * 7));
    return bytes;
}

std::vector<Entry> AsciiEntries()
{
    const std::wstring withNul(L"a\0b", 3);
    return {
        {L"RT", L"", {REG_SZ, StringBytes(L"default")}},
        {L"RT", L"plain", {REG_SZ, StringBytes(L"text")}},
        {L"RT", L"escapes", {REG_SZ, StringBytes(L"C:\\dir\\\"quoted\"\\")}},
        {L"RT", L"empty", {REG_SZ, StringBytes(L"")}},
        {L"RT", L"unterminated", {REG_SZ, BytesOf(L"ab")}},
        {L"RT", L"embedded nul", {REG_SZ, StringBytes(withNul)}},
        {L"RT", L"newline", {REG_SZ, StringBytes(L"a\r\nb\nc")}},
        {L"RT", L"tab and semicolon", {REG_SZ, StringBytes(L"a\tb;c")}},
        {L"RT", L"odd", {REG_SZ, {std::byte{0x61}, std::byte{0}, std::byte{0x62}}}},
        {L"RT", L"zero bytes", {REG_SZ, {}}},
        {L"RT", L"expand", {REG_EXPAND_SZ, StringBytes(L"%SystemRoot%\\x")}},
        {L"RT", L"multi", {REG_MULTI_SZ, BytesOf(
            std::wstring(L"one\0two\0\0", 9)
        )}},
        {L"RT", L"dword", {REG_DWORD, DwordBytes(0xDEADBEEF)}},
        {L"RT", L"dword zero", {REG_DWORD, DwordBytes(0)}},
        {L"RT", L"dword short", {REG_DWORD, Counting(3)}},
        {L"RT", L"dword long", {REG_DWORD, Counting(5)}},
        {L"RT", L"dword big endian", {REG_DWORD_BIG_ENDIAN, Counting(4)}},
        {L"RT", L"qword", {REG_QWORD, Counting(8)}},
        {L"RT", L"binary empty", {REG_BINARY, {}}},
        {L"RT", L"binary 1", {REG_BINARY, Counting(1)}},
        {L"RT", L"binary 200", {REG_BINARY, Counting(200)}},
        {L"RT", L"none", {REG_NONE, {}}},
        {L"RT", L"none data", {REG_NONE, Counting(5)}},
        {L"RT", L"link", {REG_LINK, BytesOf(L"link")}},
        {L"RT", L"resource list", {REG_RESOURCE_LIST, Counting(30)}},
        {L"RT", L"unknown type", {0x12345, Counting(4)}},
        {L"RT", L"@", {REG_DWORD, DwordBytes(1)}},
        {L"RT", L"quote\"and\\slash", {REG_DWORD, DwordBytes(2)}},
        {L"RT", L" padded ", {REG_DWORD, DwordBytes(3)}},
        {L"RT", L"hex:01", {REG_DWORD, DwordBytes(4)}},
        {L"RT", L"; not a comment", {REG_DWORD, DwordBytes(5)}},
        {L"RT\\Sub", L"inner", {REG_SZ, StringBytes(L"x")}},
        {L"RT\\Sub\\Deeper", L"", {REG_BINARY, Counting(60)}},
        {L"RT\\a]b", L"closing bracket", {REG_DWORD, DwordBytes(6)}},
        {L"RT\\Empty", L"", {REG_NONE, {}}},
    };
}

void Populate(Registry& registry, const std::vector<Entry>& entries)
{
    for (const auto& entry : entries)
        MustSet(
            registry,
            entry.pszKey,
            entry.name.c_str(),
            entry.value.dwType,
            entry.value.data
        );
}

void CheckSame(Registry& registry, const std::vector<Entry>& entries)
{
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        INFO("entry " << i);
        CheckValue(
            registry,
            entry.pszKey,
            entry.name.c_str(),
            entry.value.dwType,
            entry.value.data
        );
    }
}

/// Exports "RT" from one hive, parses the result and applies it to a fresh hive.
void RoundTrip(const std::vector<Entry>& entries, const Format format)
{
    AppHive source;
    AppHiveRegistry sourceRegistry(source.Root());
    Populate(sourceRegistry, entries);

    std::vector<std::byte> file;
    REQUIRE(
        ExportSnapshot(
            sourceRegistry,
            HKEY_CURRENT_USER,
            L"RT",
            format,
            file
        ) == ERROR_SUCCESS
    );

    Script script;
    ParseError error;
    const auto status = Parse(file, script, &error);
    INFO("line " << error.line << " column " << error.column);
    REQUIRE(status == ERROR_SUCCESS);

    AppHive target;
    AppHiveRegistry targetRegistry(target.Root());
    REQUIRE(Apply(script, targetRegistry, nullptr) == ERROR_SUCCESS);
    CheckSame(targetRegistry, entries);

    std::vector<std::byte> again;
    REQUIRE(
        ExportSnapshot(
            targetRegistry,
            HKEY_CURRENT_USER,
            L"RT",
            format,
            again
        ) == ERROR_SUCCESS
    );
    CHECK(again == file);
}
} // namespace

TEST_CASE("Every value type survives a round trip in every format")
{
    const auto format = GENERATE(
        Format::Unicode,
        Format::Utf8,
        Format::Regedit4
    );
    INFO("format " << static_cast<int>(format));
    RoundTrip(AsciiEntries(), format);
}

TEST_CASE("Non-ASCII names and strings survive Unicode and UTF-8")
{
    const auto format = GENERATE(Format::Unicode, Format::Utf8);
    const std::vector<Entry> entries = {
        {L"RT\\\u4e2d\u6587", L"caf\u00e9", {REG_SZ, StringBytes(L"\u00e9\u4e2d \U0001F600")}},
        {L"RT\\\u4e2d\u6587", L"", {REG_EXPAND_SZ, StringBytes(L"%\u00e9%")}},
        {L"RT", L"\U0001F600", {REG_DWORD, DwordBytes(7)}},
    };
    RoundTrip(entries, format);
}

TEST_CASE("A value of the ANSI code page survives REGEDIT4")
{
    char chAcute{};
    BOOL fUsedDefault = FALSE;
    if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, L"\u00e9", 1, &chAcute, 1, nullptr, &fUsedDefault) != 1 ||
        fUsedDefault)
        SKIP("the ANSI code page has no e acute");
    const std::vector<Entry> entries = {
        {L"RT\\caf\u00e9", L"n\u00e9", {REG_SZ, StringBytes(L"\u00e9")}},
    };
    RoundTrip(entries, Format::Regedit4);
}

TEST_CASE("Applying an export over the same data changes nothing")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    const auto entries = AsciiEntries();
    Populate(registry, entries);

    std::vector<std::byte> file;
    REQUIRE(
        ExportSnapshot(
            registry,
            HKEY_CURRENT_USER,
            L"RT",
            Format::Unicode,
            file
        ) == ERROR_SUCCESS
    );
    Script script;
    REQUIRE(Parse(file, script, nullptr) == ERROR_SUCCESS);
    REQUIRE(Apply(script, registry, nullptr) == ERROR_SUCCESS);

    std::vector<std::byte> again;
    REQUIRE(
        ExportSnapshot(
            registry,
            HKEY_CURRENT_USER,
            L"RT",
            Format::Unicode,
            again
        ) == ERROR_SUCCESS
    );
    CHECK(again == file);
}

TEST_CASE("The exported file ends where regedit's does")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    Populate(registry, AsciiEntries());
    std::vector<std::byte> file;
    REQUIRE(
        ExportSnapshot(
            registry,
            HKEY_CURRENT_USER,
            L"RT",
            Format::Utf8,
            file
        ) == ERROR_SUCCESS
    );
    const auto text = AsString(file);
    CHECK(text.starts_with("Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\RT]\r\n"));
    CHECK(text.ends_with("\r\n\r\n"));
    CHECK(text.find("\n\n") == std::string::npos);
}
