// The .reg parser: headers, encodings, every construct, and malformed input.

#include "regfile_test.h"

#include <catch2/generators/catch_generators.hpp>

#include <cstdio>

using namespace VReg;
using namespace VReg::RegFile;
using namespace VReg::Test;

namespace
{
const std::wstring HEADER = L"Windows Registry Editor Version 5.00\r\n";
const std::wstring KEY = L"[HKEY_CURRENT_USER\\T]\r\n";

/// Parses @p body under the v5 header and a key at line 2, so body line 1 is file line 3.
Script ParseBody(const std::wstring& body)
{
    return MustParse(HEADER + KEY + body);
}

Operation OnlyValue(const Script& script)
{
    REQUIRE(script.operations.size() == 2);
    return script.operations[1];
}
} // namespace

TEST_CASE("Both headers and every encoding parse")
{
    const std::string v5 = "Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\T]\r\n\"a\"=\"b\"\r\n";
    const std::string v4 = "REGEDIT4\r\n\r\n[HKEY_CURRENT_USER\\T]\r\n\"a\"=\"b\"\r\n";
    const auto utf8Bom = [](const std::string& text) { return RawFile("\xEF\xBB\xBF" + text); };

    std::vector<std::byte> file;
    SECTION("UTF-16LE with BOM")
    {
        file = Utf16File(L"Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\T]\r\n\"a\"=\"b\"\r\n");
    }
    SECTION("UTF-16LE with BOM and REGEDIT4")
    {
        file = Utf16File(L"REGEDIT4\r\n\r\n[HKEY_CURRENT_USER\\T]\r\n\"a\"=\"b\"\r\n");
    }
    SECTION("UTF-8 with BOM")
    {
        file = utf8Bom(v5);
    }
    SECTION("UTF-8 without BOM")
    {
        file = RawFile(v5);
    }
    SECTION("REGEDIT4 in the ANSI code page")
    {
        file = RawFile(v4);
    }
    SECTION("LF line endings")
    {
        file = RawFile("Windows Registry Editor Version 5.00\n\n[HKEY_CURRENT_USER\\T]\n\"a\"=\"b\"\n");
    }

    Script script;
    ParseError error;
    REQUIRE(Parse(file, script, &error) == ERROR_SUCCESS);
    REQUIRE(script.operations.size() == 2);
    CHECK(script.operations[1].name == L"a");
    CHECK(script.operations[1].data == StringBytes(L"b"));
}

TEST_CASE("Non-ASCII text decodes per encoding")
{
    Script script;
    SECTION("UTF-8")
    {
        REQUIRE(
            Parse(
                RawFile("Windows Registry Editor Version 5.00\n[HKEY_CURRENT_USER\\T]\n\"a\"=\"caf\xC3\xA9 \xE4\xB8\xAD\"\n"),
                script,
                nullptr
            ) == ERROR_SUCCESS
        );
        CHECK(script.operations[1].data == StringBytes(L"caf\u00e9 \u4e2d"));
    }
    SECTION("REGEDIT4 through the ANSI code page")
    {
        char chAcute{};
        BOOL fUsedDefault = FALSE;
        if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, L"\u00e9", 1, &chAcute, 1, nullptr, &fUsedDefault) != 1 ||
            fUsedDefault)
            SKIP("the ANSI code page has no e acute");
        std::string text = "REGEDIT4\n[HKEY_CURRENT_USER\\T]\n\"a\"=\"x\"\n";
        text.insert(text.find("x\""), 1, chAcute);
        REQUIRE(Parse(RawFile(text), script, nullptr) == ERROR_SUCCESS);
        CHECK(script.operations[1].data == StringBytes(L"\u00e9x"));
    }
}

TEST_CASE("Unusable files are rejected whole")
{
    Script script;
    script.operations.emplace_back();
    ParseError error;

    SECTION("an empty file")
    {
        CHECK(Parse({}, script, &error) == ERROR_INVALID_DATA);
        CHECK(error.line == 1);
        CHECK(error.column == 1);
    }
    SECTION("a wrong header")
    {
        CHECK(
            Parse(
                RawFile("Windows Registry Editor Version 4.00\r\n"),
                script,
                &error
            ) == ERROR_INVALID_DATA
        );
        CHECK(error.line == 1);
    }
    SECTION("a header after a comment")
    {
        CHECK(
            Parse(
                RawFile("; hi\r\nREGEDIT4\r\n"),
                script,
                &error
            ) == ERROR_INVALID_DATA
        );
    }
    SECTION("big-endian UTF-16")
    {
        CHECK(
            Parse(RawFile("\xFE\xFF\0W"), script, &error) == ERROR_INVALID_DATA
        );
    }
    SECTION("UTF-16 with a stray byte")
    {
        auto file = Utf16File(L"REGEDIT4\r\n");
        file.push_back(std::byte{0});
        CHECK(Parse(file, script, &error) == ERROR_INVALID_DATA);
    }
    SECTION("invalid UTF-8")
    {
        CHECK(
            Parse(
                RawFile("Windows Registry Editor Version 5.00\n[HKEY_CURRENT_USER\\T]\n\"a\"=\"\xFF\"\n"),
                script,
                &error
            ) ==
            ERROR_INVALID_DATA
        );
    }
    CHECK(error.pszMessage != nullptr);
    CHECK(script.operations.size() == 1);
}

TEST_CASE("A header alone is an empty script")
{
    Script script;
    script.operations.emplace_back();
    REQUIRE(
        Parse(
            Utf16File(L"REGEDIT4\r\n\r\n; nothing\r\n"),
            script,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(script.operations.empty());
}

TEST_CASE("Keys")
{
    SECTION("create and delete, with the line they start on")
    {
        const auto script = MustParse(
            HEADER + L"\r\n[HKEY_CURRENT_USER\\Software\\Mixed Case]\r\n\r\n[-HKEY_LOCAL_MACHINE\\Gone\\Deep]\r\n"
        );
        REQUIRE(script.operations.size() == 2);
        CHECK(script.operations[0].kind == OperationKind::CreateKey);
        CHECK(script.operations[0].hRoot == HKEY_CURRENT_USER);
        CHECK(script.operations[0].path == L"Software\\Mixed Case");
        CHECK(script.operations[0].line == 3);
        CHECK(script.operations[1].kind == OperationKind::DeleteKey);
        CHECK(script.operations[1].hRoot == HKEY_LOCAL_MACHINE);
        CHECK(script.operations[1].path == L"Gone\\Deep");
        CHECK(script.operations[1].line == 5);
    }
    SECTION("roots by full name and short name, in any case")
    {
        const auto root = GENERATE(
            std::pair<const wchar_t*, HKEY>{L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
            std::pair<const wchar_t*, HKEY>{L"HKCR", HKEY_CLASSES_ROOT},
            std::pair<const wchar_t*, HKEY>{L"hkey_current_user", HKEY_CURRENT_USER},
            std::pair<const wchar_t*, HKEY>{L"HKCU", HKEY_CURRENT_USER},
            std::pair<const wchar_t*, HKEY>{L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
            std::pair<const wchar_t*, HKEY>{L"hklm", HKEY_LOCAL_MACHINE},
            std::pair<const wchar_t*, HKEY>{L"HKEY_USERS", HKEY_USERS},
            std::pair<const wchar_t*, HKEY>{L"HKU", HKEY_USERS},
            std::pair<const wchar_t*, HKEY>{L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
            std::pair<const wchar_t*, HKEY>{L"HKCC", HKEY_CURRENT_CONFIG}
        );
        const auto script = MustParse(HEADER + L"[" + root.first + L"\\A]\r\n");
        REQUIRE(script.operations.size() == 1);
        CHECK(script.operations[0].hRoot == root.second);
        CHECK(script.operations[0].path == L"A");
    }
    SECTION("a root alone has an empty path")
    {
        const auto script = MustParse(HEADER + L"[HKLM]\r\n");
        CHECK(script.operations[0].path.empty());
    }
    SECTION("a ] inside the name stays in it")
    {
        const auto script = MustParse(HEADER + L"[HKLM\\a]b]  \r\n");
        CHECK(script.operations[0].path == L"a]b");
    }
}

TEST_CASE("Strings")
{
    SECTION("plain, empty and escaped")
    {
        const auto script = ParseBody(L"\"a\"=\"text\"\r\n\"b\"=\"\"\r\n\"c\"=\"x\\\\y\\\"z\\\"\"\r\n");
        REQUIRE(script.operations.size() == 4);
        CHECK(script.operations[1].dwType == REG_SZ);
        CHECK(script.operations[1].data == StringBytes(L"text"));
        CHECK(script.operations[2].data == StringBytes(L""));
        CHECK(script.operations[3].data == StringBytes(L"x\\y\"z\""));
    }
    SECTION("the default value and escaped names")
    {
        const auto script = ParseBody(L"@=\"d\"\r\n\"a\\\"b\\\\\"=\"v\"\r\n\"@\"=\"at\"\r\n");
        REQUIRE(script.operations.size() == 4);
        CHECK(script.operations[1].name.empty());
        CHECK(script.operations[2].name == L"a\"b\\");
        CHECK(script.operations[3].name == L"@");
    }
    SECTION("whitespace around the parts")
    {
        const auto script = ParseBody(L"  \"a\" = \"b\"  \r\n\t@=\"c\"\r\n");
        REQUIRE(script.operations.size() == 3);
        CHECK(script.operations[1].data == StringBytes(L"b"));
    }
}

TEST_CASE("Numbers and binary data")
{
    SECTION("dword")
    {
        const auto script = ParseBody(L"\"a\"=dword:0000002a\r\n\"b\"=DWORD:FFFFFFFF\r\n\"c\"=dword:7\r\n");
        REQUIRE(script.operations.size() == 4);
        CHECK(script.operations[1].dwType == REG_DWORD);
        CHECK(script.operations[1].data == DwordBytes(0x2a));
        CHECK(script.operations[2].data == DwordBytes(0xFFFFFFFF));
        CHECK(script.operations[3].data == DwordBytes(7));
    }
    SECTION("hex is binary")
    {
        const auto value = OnlyValue(ParseBody(L"\"a\"=hex:00,ff,A0\r\n"));
        CHECK(value.dwType == REG_BINARY);
        CHECK(value.data == std::vector{std::byte{0}, std::byte{0xFF}, std::byte{0xA0}});
    }
    SECTION("hex with no bytes")
    {
        const auto value = OnlyValue(ParseBody(L"\"a\"=hex:\r\n"));
        CHECK(value.dwType == REG_BINARY);
        CHECK(value.data.empty());
    }
    SECTION("hex(n) for any type")
    {
        const auto script = ParseBody(
            L"\"a\"=hex(0):\r\n\"b\"=hex(2):61,00,00,00\r\n\"c\"=hex(b):01,00,00,00,00,00,00,00\r\n"
            L"\"d\"=hex(12345):01\r\n\"e\"=hex(7):\r\n@=hex(1):1\r\n"
        );
        REQUIRE(script.operations.size() == 7);
        CHECK(script.operations[1].dwType == REG_NONE);
        CHECK(script.operations[2].dwType == REG_EXPAND_SZ);
        CHECK(script.operations[3].dwType == REG_QWORD);
        CHECK(script.operations[3].data.size() == 8);
        CHECK(script.operations[4].dwType == 0x12345);
        CHECK(script.operations[5].dwType == REG_MULTI_SZ);
        CHECK(script.operations[6].data == std::vector{std::byte{1}});
    }
    SECTION("blanks around the commas")
    {
        const auto value = OnlyValue(ParseBody(L"\"a\"=hex: 01 , 02,03 \r\n"));
        CHECK(value.data.size() == 3);
    }
}

TEST_CASE("Deleting values")
{
    const auto script = ParseBody(L"\"a\"=-\r\n@=-\r\n");
    REQUIRE(script.operations.size() == 3);
    CHECK(script.operations[1].kind == OperationKind::DeleteValue);
    CHECK(script.operations[1].name == L"a");
    CHECK(script.operations[2].kind == OperationKind::DeleteValue);
    CHECK(script.operations[2].name.empty());
}

TEST_CASE("Value operations carry their key")
{
    const auto script = MustParse(HEADER + L"[HKCU\\A]\r\n\"x\"=dword:1\r\n[HKLM\\B]\r\n\"y\"=-\r\n");
    REQUIRE(script.operations.size() == 4);
    CHECK(script.operations[1].hRoot == HKEY_CURRENT_USER);
    CHECK(script.operations[1].path == L"A");
    CHECK(script.operations[3].hRoot == HKEY_LOCAL_MACHINE);
    CHECK(script.operations[3].path == L"B");
}

TEST_CASE("Continuations, comments and blank lines")
{
    SECTION("hex continues over lines with leading blanks")
    {
        const auto value = OnlyValue(ParseBody(L"\"a\"=hex:01,02,\\\r\n  03,04,\\\r\n  05\r\n"));
        CHECK(value.data.size() == 5);
        CHECK(value.data[4] == std::byte{5});
    }
    SECTION("a continuation without a next line is a syntax error")
    {
        Script script;
        ParseError error;
        const auto file = Utf16File(HEADER + KEY + L"\"a\"=hex:01,\\");
        CHECK(Parse(file, script, &error) == ERROR_INVALID_DATA);
    }
    SECTION("comments and blank lines are skipped anywhere between lines")
    {
        const auto script = MustParse(
            HEADER + L"; first\r\n\r\n  ; indented\r\n[HKCU\\A]\r\n\r\n; between\r\n\"a\"=dword:1\r\n\r\n"
        );
        CHECK(script.operations.size() == 2);
        CHECK(script.operations[1].line == 8);
    }
    SECTION("the last line needs no line ending")
    {
        const auto script = MustParse(HEADER + L"[HKCU\\A]\r\n\"a\"=dword:1");
        CHECK(script.operations.size() == 2);
    }
}

TEST_CASE("Malformed input names its line and column")
{
    struct Case
    {
        const char* pszWhy;
        std::wstring text;
        std::size_t line;
        std::size_t column;
    };
    const Case cases[] = {
        {"unterminated key", HEADER + L"[HKEY_CURRENT_USER\\x\r\n", 2, 21},
        {"text after a key", HEADER + L"[HKEY_CURRENT_USER]junk\r\n", 2, 20},
        {"unknown root", HEADER + L"[BOGUS\\x]\r\n", 2, 2},
        {"unknown root when deleting", HEADER + L"[-BOGUS\\x]\r\n", 2, 3},
        {"deleting a root", HEADER + L"[-HKEY_CURRENT_USER]\r\n", 2, 3},
        {"a NUL in a key path", HEADER + L"[-HKEY_CURRENT_USER\\" + std::wstring(1, L'\0') + L"x]\r\n", 2, 21},
        {"a NUL in a value name", HEADER + KEY + L"\"a" + std::wstring(1, L'\0') + L"b\"=dword:1\r\n", 3, 3},
        {"an empty key", HEADER + L"[]\r\n", 2, 2},
        {"a value before any key", HEADER + L"\"a\"=\"b\"\r\n", 2, 1},
        {"a value after a deleted key", HEADER + L"[-HKCU\\x]\r\n\"a\"=dword:1\r\n", 3, 1},
        {"text that is no line", HEADER + KEY + L"bogus\r\n", 3, 1},
        {"a missing equals sign", HEADER + KEY + L"\"a\" \"b\"\r\n", 3, 5},
        {"an unterminated string", HEADER + KEY + L"\"a\"=\"unterminated\r\n", 3, 18},
        {"an unterminated name", HEADER + KEY + L"\"abc\r\n", 3, 5},
        {"an invalid escape", HEADER + KEY + L"\"a\"=\"bad\\q\"\r\n", 3, 9},
        {"text after a string", HEADER + KEY + L"\"a\"=\"b\" junk\r\n", 3, 9},
        {"text after a delete", HEADER + KEY + L"\"a\"=-x\r\n", 3, 6},
        {"an unknown value form", HEADER + KEY + L"\"a\"=bogus\r\n", 3, 5},
        {"a missing value", HEADER + KEY + L"\"a\"=\r\n", 3, 5},
        {"dword without digits", HEADER + KEY + L"\"a\"=dword:\r\n", 3, 11},
        {"dword with nine digits", HEADER + KEY + L"\"a\"=dword:123456789\r\n", 3, 19},
        {"dword with trailing text", HEADER + KEY + L"\"a\"=dword:12zz\r\n", 3, 13},
        {"hex with a bad byte", HEADER + KEY + L"\"a\"=hex:01,zz\r\n", 3, 12},
        {"hex with a trailing comma", HEADER + KEY + L"\"a\"=hex:01,\r\n", 3, 12},
        {"hex with a leading comma", HEADER + KEY + L"\"a\"=hex:,01\r\n", 3, 9},
        {"hex with three digits", HEADER + KEY + L"\"a\"=hex:010\r\n", 3, 11},
        {"hex without a comma", HEADER + KEY + L"\"a\"=hex:01 02\r\n", 3, 12},
        {"hex(n) without digits", HEADER + KEY + L"\"a\"=hex(zz):\r\n", 3, 9},
        {"hex(n) without a closing parenthesis", HEADER + KEY + L"\"a\"=hex(1:\r\n", 3, 10},
        {"hex(n) with a huge type", HEADER + KEY + L"\"a\"=hex(123456789):\r\n", 3, 17},
        {"hex without a colon", HEADER + KEY + L"\"a\"=hex\r\n", 3, 8},
        {"an error on a continuation line", HEADER + KEY + L"\"a\"=hex:01,\\\r\n  zz\r\n", 4, 3},
        {"an error after comments and blanks", HEADER + KEY + L"; c\r\n\r\n\"a\"=hex:zz\r\n", 5, 9},
    };

    for (const auto& c : cases)
    {
        INFO(c.pszWhy);
        Script script;
        script.operations.emplace_back();
        ParseError error;
        const auto file = Utf16File(c.text);
        CHECK(Parse(file, script, &error) == ERROR_INVALID_DATA);
        CHECK(error.line == c.line);
        CHECK(error.column == c.column);
        CHECK(error.pszMessage != nullptr);
        CHECK(script.operations.size() == 1);
    }
}

TEST_CASE("ParseFile reads a file and reports a missing one")
{
    const auto directory = std::filesystem::temp_directory_path() / L"VRegParseFileTest";
    std::filesystem::create_directories(directory);
    const auto path = directory / L"a.reg";
    {
        const auto file = Utf16File(HEADER + KEY + L"\"a\"=dword:1\r\n");
        std::FILE* pFile = nullptr;
        REQUIRE(_wfopen_s(&pFile, path.c_str(), L"wb") == 0);
        std::fwrite(file.data(), 1, file.size(), pFile);
        std::fclose(pFile);
    }

    Script script;
    CHECK(ParseFile(path, script, nullptr) == ERROR_SUCCESS);
    CHECK(script.operations.size() == 2);
    CHECK(
        ParseFile(
            directory / L"missing.reg",
            script,
            nullptr
        ) == ERROR_FILE_NOT_FOUND
    );

    std::filesystem::remove_all(directory);
}
