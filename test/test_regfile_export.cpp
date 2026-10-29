// Snapshot export: layout, ordering, escaping and format limits, as exact text.

#include "regfile_test.h"

#include <algorithm>

using namespace VReg;
using namespace VReg::RegFile;
using namespace VReg::Test;

namespace
{
const std::wstring kHeader = L"Windows Registry Editor Version 5.00\r\n\r\n";

std::vector<std::byte> Sequence(const int first, const int count)
{
    std::vector<std::byte> bytes;
    for (int i = 0; i < count; ++i)
        bytes.push_back(static_cast<std::byte>(first + i));
    return bytes;
}

std::vector<std::byte> Export(
    Registry& registry,
    const std::wstring_view path,
    const Format format
)
{
    std::vector<std::byte> file;
    REQUIRE(
        ExportSnapshot(
            registry,
            HKEY_CURRENT_USER,
            path,
            format,
            file
        ) == ERROR_SUCCESS
    );
    return file;
}
} // namespace

TEST_CASE("Export lays a snapshot out the way regedit does")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(
        registry,
        L"Software\\VRegTest",
        L"",
        REG_SZ,
        StringBytes(L"default")
    );
    MustSet(
        registry,
        L"Software\\VRegTest",
        L"Dword",
        REG_DWORD,
        DwordBytes(42)
    );
    MustSet(
        registry,
        L"Software\\VRegTest",
        L"Str",
        REG_SZ,
        StringBytes(L"a\\b \"q\"")
    );
    MustSet(
        registry,
        L"Software\\VRegTest\\Sub",
        L"Bin",
        REG_BINARY,
        {std::byte{1}, std::byte{2}, std::byte{0xFF}}
    );
    MustSet(registry, L"Software\\VRegTest\\Empty", L"Empty", REG_BINARY, {});
    {
        HKEY hKey{};
        REQUIRE(
            registry.RegCreateKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\VRegTest\\NoValues",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ) == ERROR_SUCCESS
        );
        registry.RegCloseKey(
            hKey
        );
    }

    const std::wstring expected = kHeader +
        L"[HKEY_CURRENT_USER\\Software\\VRegTest]\r\n"
        L"@=\"default\"\r\n"
        L"\"Dword\"=dword:0000002a\r\n"
        L"\"Str\"=\"a\\\\b \\\"q\\\"\"\r\n"
        L"\r\n"
        L"[HKEY_CURRENT_USER\\Software\\VRegTest\\Empty]\r\n"
        L"\"Empty\"=hex:\r\n"
        L"\r\n"
        L"[HKEY_CURRENT_USER\\Software\\VRegTest\\NoValues]\r\n"
        L"\r\n"
        L"[HKEY_CURRENT_USER\\Software\\VRegTest\\Sub]\r\n"
        L"\"Bin\"=hex:01,02,ff\r\n"
        L"\r\n";
    CHECK(
        Export(
            registry,
            L"Software\\VRegTest",
            Format::Unicode
        ) == Utf16File(expected)
    );
}

TEST_CASE("Export wraps hex data the way regedit does")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"W", L"Wrap", REG_BINARY, Sequence(0, 60));
    MustSet(registry, L"W", L"", REG_BINARY, Sequence(0, 30));
    MustSet(registry, L"W", L"Edge", REG_BINARY, Sequence(0, 23));
    MustSet(registry, L"W", L"Edge76", REG_BINARY, Sequence(0, 23));
    MustSet(registry, L"W", L"Last", REG_BINARY, Sequence(0, 22));

    const std::wstring expected = kHeader +
        L"[HKEY_CURRENT_USER\\W]\r\n"
        L"@=hex:00,01,02,03,04,05,06,07,08,09,0a,0b,0c,0d,0e,0f,10,11,12,13,14,15,16,17,\\\r\n"
        L"  18,19,1a,1b,1c,1d\r\n"
        L"\"Edge\"=hex:00,01,02,03,04,05,06,07,08,09,0a,0b,0c,0d,0e,0f,10,11,12,13,14,15,\\\r\n"
        L"  16\r\n"
        L"\"Edge76\"=hex:00,01,02,03,04,05,06,07,08,09,0a,0b,0c,0d,0e,0f,10,11,12,13,14,15,\\\r\n"
        L"  16\r\n"
        L"\"Last\"=hex:00,01,02,03,04,05,06,07,08,09,0a,0b,0c,0d,0e,0f,10,11,12,13,14,15\r\n"
        L"\"Wrap\"=hex:00,01,02,03,04,05,06,07,08,09,0a,0b,0c,0d,0e,0f,10,11,12,13,14,15,\\\r\n"
        L"  16,17,18,19,1a,1b,1c,1d,1e,1f,20,21,22,23,24,25,26,27,28,29,2a,2b,2c,2d,2e,\\\r\n"
        L"  2f,30,31,32,33,34,35,36,37,38,39,3a,3b\r\n"
        L"\r\n";
    CHECK(Export(registry, L"W", Format::Unicode) == Utf16File(expected));
}

TEST_CASE("Export writes typed values as hex(n) and friendly forms only when exact")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"T", L"1 sz", REG_SZ, StringBytes(L"text"));
    MustSet(registry, L"T", L"2 empty sz", REG_SZ, StringBytes(L""));
    MustSet(registry, L"T", L"3 unterminated", REG_SZ, BytesOf(L"ab"));
    MustSet(
        registry,
        L"T",
        L"4 embedded nul",
        REG_SZ,
        StringBytes(std::wstring(L"a\0b", 3))
    );
    MustSet(registry, L"T", L"5 newline", REG_SZ, StringBytes(L"a\r\nb"));
    MustSet(
        registry,
        L"T",
        L"6 odd",
        REG_SZ,
        {std::byte{0x61}, std::byte{0}, std::byte{0x62}}
    );
    MustSet(registry, L"T", L"7 zero bytes", REG_SZ, {});
    MustSet(registry, L"T", L"8 expand", REG_EXPAND_SZ, StringBytes(L"%P%"));
    MustSet(
        registry,
        L"T",
        L"9 multi",
        REG_MULTI_SZ,
        BytesOf(std::wstring(L"a\0b\0\0", 5))
    );
    MustSet(registry, L"T", L"a dword3", REG_DWORD, Sequence(1, 3));
    MustSet(registry, L"T", L"b qword", REG_QWORD, Sequence(1, 8));
    MustSet(registry, L"T", L"c none", REG_NONE, {});
    MustSet(registry, L"T", L"d type", 0x12345, {std::byte{7}});

    const std::wstring expected = kHeader +
        L"[HKEY_CURRENT_USER\\T]\r\n"
        L"\"1 sz\"=\"text\"\r\n"
        L"\"2 empty sz\"=\"\"\r\n"
        L"\"3 unterminated\"=hex(1):61,00,62,00\r\n"
        L"\"4 embedded nul\"=hex(1):61,00,00,00,62,00,00,00\r\n"
        L"\"5 newline\"=hex(1):61,00,0d,00,0a,00,62,00,00,00\r\n"
        L"\"6 odd\"=hex(1):61,00,62\r\n"
        L"\"7 zero bytes\"=hex(1):\r\n"
        L"\"8 expand\"=hex(2):25,00,50,00,25,00,00,00\r\n"
        L"\"9 multi\"=hex(7):61,00,00,00,62,00,00,00,00,00\r\n"
        L"\"a dword3\"=hex(4):01,02,03\r\n"
        L"\"b qword\"=hex(b):01,02,03,04,05,06,07,08\r\n"
        L"\"c none\"=hex(0):\r\n"
        L"\"d type\"=hex(12345):07\r\n"
        L"\r\n";
    CHECK(Export(registry, L"T", Format::Unicode) == Utf16File(expected));
}

TEST_CASE("Export orders keys and values ordinal case-insensitive")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    for (const auto* const pszName : {L"b", L"A", L"_x", L"c", L"B2"})
    {
        MustSet(
            registry,
            (std::wstring(L"S\\") + pszName).c_str(),
            L"z",
            REG_DWORD,
            DwordBytes(1)
        );
        MustSet(
            registry,
            (std::wstring(L"S\\") + pszName).c_str(),
            L"y",
            REG_DWORD,
            DwordBytes(2)
        );
        MustSet(
            registry,
            (std::wstring(L"S\\") + pszName).c_str(),
            L"",
            REG_DWORD,
            DwordBytes(3)
        );
        MustSet(
            registry,
            (std::wstring(L"S\\") + pszName).c_str(),
            L"Ab",
            REG_DWORD,
            DwordBytes(4)
        );
    }

    const auto file = Export(registry, L"S", Format::Utf8);
    const auto text = AsString(file);
    std::vector<std::size_t> positions;
    for (const auto* const pszKey : {"\\S]", "\\S\\A]", "\\S\\b]", "\\S\\B2]", "\\S\\c]", "\\S\\_x]"})
    {
        const auto at = text.find(pszKey);
        REQUIRE(at != std::string::npos);
        positions.push_back(at);
    }
    CHECK(std::is_sorted(positions.begin(), positions.end()));

    const auto blockA = text.find("[HKEY_CURRENT_USER\\S\\A]");
    positions.clear();
    for (const auto* const pszValue : {"@=", "\"Ab\"=", "\"y\"=", "\"z\"="})
    {
        const auto at = text.find(pszValue, blockA);
        REQUIRE(at != std::string::npos);
        positions.push_back(at);
    }
    CHECK(std::is_sorted(positions.begin(), positions.end()));
    CHECK(Export(registry, L"S", Format::Utf8) == file);
}

TEST_CASE("Export escapes names the way strings are escaped")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"N\\a]b", L"q\"uo\\te", REG_DWORD, DwordBytes(1));
    MustSet(registry, L"N\\a]b", L"@", REG_DWORD, DwordBytes(2));

    const std::wstring expected = kHeader +
        L"[HKEY_CURRENT_USER\\N]\r\n"
        L"\r\n"
        L"[HKEY_CURRENT_USER\\N\\a]b]\r\n"
        L"\"@\"=dword:00000002\r\n"
        L"\"q\\\"uo\\\\te\"=dword:00000001\r\n"
        L"\r\n";
    CHECK(Export(registry, L"N", Format::Unicode) == Utf16File(expected));
}

TEST_CASE("Export in the other formats")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"F", L"s", REG_SZ, StringBytes(L"v"));
    MustSet(registry, L"F", L"h", REG_BINARY, {std::byte{1}});

    SECTION("UTF-8 has no BOM")
    {
        CHECK(
            AsString(Export(registry, L"F", Format::Utf8)) ==
            "Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\F]\r\n\"h\"=hex:01\r\n\"s\"=\"v\"\r\n\r\n"
        );
    }
    SECTION("REGEDIT4 has its own header")
    {
        CHECK(
            AsString(Export(registry, L"F", Format::Regedit4)) ==
            "REGEDIT4\r\n\r\n[HKEY_CURRENT_USER\\F]\r\n\"h\"=hex:01\r\n\"s\"=\"v\"\r\n\r\n"
        );
    }
}

TEST_CASE("Non-ASCII text exports in Unicode and UTF-8")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"U\\中", L"café", REG_SZ, StringBytes(L"é"));

    CHECK(
        Export(registry, L"U", Format::Utf8) ==
        RawFile(
            "Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\U]\r\n\r\n"
            "[HKEY_CURRENT_USER\\U\\\xE4\xB8\xAD]\r\n\"caf\xC3\xA9\"=\"\xC3\xA9\"\r\n\r\n"
        )
    );
    CHECK(
        Export(registry, L"U", Format::Unicode) ==
        Utf16File(
            kHeader + L"[HKEY_CURRENT_USER\\U]\r\n\r\n[HKEY_CURRENT_USER\\U\\中]\r\n\"café\"=\"é\"\r\n\r\n"
        )
    );
}

TEST_CASE("REGEDIT4 refuses what the ANSI code page cannot hold")
{
    if (GetACP() == CP_UTF8)
        SKIP("every character fits an ANSI code page of UTF-8");
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    // Outside every legacy code page.
    const std::wstring unrepresentable = L"\U0001F600";
    std::vector<std::byte> file{std::byte{1}};

    SECTION("in string data")
    {
        MustSet(registry, L"R", L"v", REG_SZ, StringBytes(unrepresentable));
    }
    SECTION("in a value name")
    {
        MustSet(
            registry,
            L"R",
            unrepresentable.c_str(),
            REG_DWORD,
            DwordBytes(1)
        );
    }
    SECTION("in a key name")
    {
        MustSet(
            registry,
            (L"R\\" + unrepresentable).c_str(),
            L"v",
            REG_DWORD,
            DwordBytes(1)
        );
    }

    CHECK(
        ExportSnapshot(
            registry,
            HKEY_CURRENT_USER,
            L"R",
            Format::Regedit4,
            file
        ) == ERROR_NO_UNICODE_TRANSLATION
    );
    CHECK(file == std::vector{std::byte{1}});
    CHECK(
        ExportSnapshot(
            registry,
            HKEY_CURRENT_USER,
            L"R",
            Format::Unicode,
            file
        ) == ERROR_SUCCESS
    );
}

TEST_CASE("Export errors")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    std::vector<std::byte> file{std::byte{1}};

    SECTION("a missing key")
    {
        CHECK(
            ExportSnapshot(
                registry,
                HKEY_CURRENT_USER,
                L"Missing",
                Format::Unicode,
                file
            ) == ERROR_FILE_NOT_FOUND
        );
    }
    SECTION("a root that is not predefined")
    {
        CHECK(
            ExportSnapshot(
                registry,
                hive.Root(),
                L"",
                Format::Unicode,
                file
            ) == ERROR_INVALID_HANDLE
        );
    }
    SECTION("a name with a line break")
    {
        MustSet(registry, L"L", L"a\nb", REG_DWORD, DwordBytes(1));
        CHECK(
            ExportSnapshot(
                registry,
                HKEY_CURRENT_USER,
                L"L",
                Format::Unicode,
                file
            ) == ERROR_INVALID_NAME
        );
    }
    CHECK(file == std::vector{std::byte{1}});
}

TEST_CASE("The root itself can be exported")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"", L"v", REG_DWORD, DwordBytes(1));
    CHECK(
        Export(registry, L"", Format::Unicode) ==
        Utf16File(kHeader + L"[HKEY_CURRENT_USER]\r\n\"v\"=dword:00000001\r\n\r\n")
    );
}

TEST_CASE("ExportSnapshotToFile writes the same bytes and keeps a file on failure")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"E", L"v", REG_DWORD, DwordBytes(1));

    const auto directory = std::filesystem::temp_directory_path() / L"VRegExportFileTest";
    std::filesystem::create_directories(directory);
    const auto path = directory / L"out.reg";

    REQUIRE(
        ExportSnapshotToFile(
            registry,
            HKEY_CURRENT_USER,
            L"E",
            Format::Unicode,
            path
        ) == ERROR_SUCCESS
    );
    Script script;
    REQUIRE(ParseFile(path, script, nullptr) == ERROR_SUCCESS);
    CHECK(script.operations.size() == 2);
    const auto size = std::filesystem::file_size(path);

    CHECK(
        ExportSnapshotToFile(
            registry,
            HKEY_CURRENT_USER,
            L"Missing",
            Format::Unicode,
            path
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(std::filesystem::file_size(path) == size);

    std::filesystem::remove_all(directory);
}
