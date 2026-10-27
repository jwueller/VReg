// Applying a parsed script to a real private hive.

#include "regfile_test.h"


using namespace VReg;
using namespace VReg::RegFile;
using namespace VReg::Test;

namespace
{
const std::wstring kHeader = L"Windows Registry Editor Version 5.00\r\n";

bool KeyExists(Registry& registry, const wchar_t* const pszPath)
{
    HKEY hKey{};
    const auto status = registry.OpenKeyExW(HKEY_CURRENT_USER, pszPath, 0, KEY_READ, &hKey);
    if (status == ERROR_SUCCESS)
        registry.CloseKey(hKey);
    return status == ERROR_SUCCESS;
}
} // namespace

TEST_CASE("Apply creates keys and sets every value form")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());

    const auto script = MustParse(
        kHeader +
        L"[HKEY_CURRENT_USER\\Software\\T\\Deep]\r\n"
        L"@=\"default\"\r\n"
        L"\"str\"=\"a\\\\b\"\r\n"
        L"\"dw\"=dword:0000002a\r\n"
        L"\"bin\"=hex:01,02,03\r\n"
        L"\"exp\"=hex(2):25,00,50,00,00,00\r\n"
        L"\"qw\"=hex(b):01,00,00,00,00,00,00,00\r\n"
        L"\"none\"=hex(0):\r\n"
        L"\"odd\"=hex(1):61\r\n"
    );
    std::size_t failed = 99;
    REQUIRE(Apply(script, registry, &failed) == ERROR_SUCCESS);
    CHECK(failed == 99);

    const auto get = [&](const wchar_t* const pszName, const DWORD dwType, const std::vector<std::byte>& data)
    {
        CheckValue(registry, L"Software\\T\\Deep", pszName, dwType, data);
    };
    get(L"", REG_SZ, StringBytes(L"default"));
    get(L"str", REG_SZ, StringBytes(L"a\\b"));
    get(L"dw", REG_DWORD, DwordBytes(0x2a));
    get(L"bin", REG_BINARY, {std::byte{1}, std::byte{2}, std::byte{3}});
    get(L"exp", REG_EXPAND_SZ, StringBytes(L"%P"));
    get(L"qw", REG_QWORD, std::vector<std::byte>{std::byte{1}, {}, {}, {}, {}, {}, {}, {}});
    get(L"none", REG_NONE, {});
    get(L"odd", REG_SZ, {std::byte{0x61}});
}

TEST_CASE("Apply overwrites values and keeps unrelated ones")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"T", L"keep", REG_DWORD, DwordBytes(1));
    MustSet(registry, L"T", L"change", REG_DWORD, DwordBytes(1));

    REQUIRE(Apply(MustParse(kHeader + L"[HKCU\\T]\r\n\"change\"=\"now text\"\r\n"), registry, nullptr) == ERROR_SUCCESS);
    CheckValue(registry, L"T", L"keep", REG_DWORD, DwordBytes(1));
    CheckValue(registry, L"T", L"change", REG_SZ, StringBytes(L"now text"));
}

TEST_CASE("Apply deletes values and keys")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"T", L"a", REG_DWORD, DwordBytes(1));
    MustSet(registry, L"T", L"b", REG_DWORD, DwordBytes(2));
    MustSet(registry, L"T", L"", REG_DWORD, DwordBytes(3));
    MustSet(registry, L"Gone\\Deep\\Deeper", L"x", REG_DWORD, DwordBytes(4));
    MustSet(registry, L"Gone", L"y", REG_DWORD, DwordBytes(5));

    SECTION("values, including the default one")
    {
        REQUIRE(Apply(MustParse(kHeader + L"[HKCU\\T]\r\n\"a\"=-\r\n@=-\r\n"), registry, nullptr) == ERROR_SUCCESS);
        const HKEY hKey = MustOpen(registry, L"T");
        DWORD dwType{};
        CHECK(registry.QueryValueExW(hKey, L"a", nullptr, &dwType, nullptr, nullptr) == ERROR_FILE_NOT_FOUND);
        CHECK(registry.QueryValueExW(hKey, L"", nullptr, &dwType, nullptr, nullptr) == ERROR_FILE_NOT_FOUND);
        CHECK(registry.QueryValueExW(hKey, L"b", nullptr, &dwType, nullptr, nullptr) == ERROR_SUCCESS);
        registry.CloseKey(hKey);
    }
    SECTION("a key with its whole subtree")
    {
        REQUIRE(Apply(MustParse(kHeader + L"[-HKCU\\Gone]\r\n"), registry, nullptr) == ERROR_SUCCESS);
        CHECK_FALSE(KeyExists(registry, L"Gone"));
        CHECK(KeyExists(registry, L"T"));
    }
    SECTION("what does not exist")
    {
        REQUIRE(
            Apply(
                MustParse(kHeader + L"[-HKCU\\Missing\\Deep]\r\n[-HKCU\\Gone\\Deep\\Deeper\\Nope]\r\n"),
                registry,
                nullptr
            ) == ERROR_SUCCESS
        );
        CHECK(KeyExists(registry, L"Gone\\Deep\\Deeper"));
    }
    SECTION("a value of a key or value that does not exist")
    {
        Script script;
        Operation operation;
        operation.kind = OperationKind::DeleteValue;
        operation.hRoot = HKEY_CURRENT_USER;
        operation.path = L"Missing";
        operation.name = L"v";
        script.operations.push_back(operation);
        operation.path = L"T";
        operation.name = L"never";
        script.operations.push_back(operation);
        CHECK(Apply(script, registry, nullptr) == ERROR_SUCCESS);
        CHECK_FALSE(KeyExists(registry, L"Missing"));
    }
}

TEST_CASE("Delete then create replaces a key")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"T\\Old", L"x", REG_DWORD, DwordBytes(1));
    MustSet(registry, L"T", L"y", REG_DWORD, DwordBytes(1));

    REQUIRE(
        Apply(MustParse(kHeader + L"[-HKCU\\T]\r\n[HKCU\\T]\r\n\"z\"=dword:2\r\n"), registry, nullptr) == ERROR_SUCCESS
    );
    CHECK_FALSE(KeyExists(registry, L"T\\Old"));
    CheckValue(registry, L"T", L"z", REG_DWORD, DwordBytes(2));
    const HKEY hKey = MustOpen(registry, L"T");
    CHECK(registry.QueryValueExW(hKey, L"y", nullptr, nullptr, nullptr, nullptr) == ERROR_FILE_NOT_FOUND);
    registry.CloseKey(hKey);
}

TEST_CASE("Apply stops at the first failing operation")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());

    Script script = MustParse(kHeader + L"[HKCU\\First]\r\n\"a\"=dword:1\r\n");
    Operation orphan;
    orphan.kind = OperationKind::SetValue;
    orphan.hRoot = HKEY_CURRENT_USER;
    orphan.path = L"NoSuchKey";
    orphan.name = L"v";
    orphan.dwType = REG_DWORD;
    orphan.data = DwordBytes(1);
    script.operations.push_back(orphan);
    script.operations.push_back(MustParse(kHeader + L"[HKCU\\Never]\r\n").operations[0]);

    std::size_t failed = 99;
    CHECK(Apply(script, registry, &failed) == ERROR_FILE_NOT_FOUND);
    CHECK(failed == 2);
    CheckValue(registry, L"First", L"a", REG_DWORD, DwordBytes(1));
    CHECK_FALSE(KeyExists(registry, L"NoSuchKey"));
    CHECK_FALSE(KeyExists(registry, L"Never"));

    CHECK(Apply(script, registry, nullptr) == ERROR_FILE_NOT_FOUND);
}

TEST_CASE("Apply refuses operations that would cut a path short or delete a root")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    const Script setup = MustParse(kHeader + L"[HKCU\\Keep]\r\n");
    REQUIRE(Apply(setup, registry, nullptr) == ERROR_SUCCESS);

    Operation deleteRoot;
    deleteRoot.kind = OperationKind::DeleteKey;
    deleteRoot.hRoot = HKEY_CURRENT_USER;

    Operation nulInPath = deleteRoot;
    nulInPath.path = std::wstring(1, L'\0') + L"x";

    Operation nulInName;
    nulInName.kind = OperationKind::SetValue;
    nulInName.hRoot = HKEY_CURRENT_USER;
    nulInName.path = L"Keep";
    nulInName.name = L"a" + std::wstring(1, L'\0') + L"b";
    nulInName.dwType = REG_DWORD;
    nulInName.data = DwordBytes(1);

    for (const auto& operation : {deleteRoot, nulInPath, nulInName})
    {
        std::size_t failed = 99;
        CHECK(Apply(Script{.operations = {operation}}, registry, &failed) == ERROR_INVALID_PARAMETER);
        CHECK(failed == 0);
    }

    CHECK(KeyExists(registry, L"Keep"));
}

TEST_CASE("An empty script applies to nothing")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    std::size_t failed = 99;
    CHECK(Apply(Script{}, registry, &failed) == ERROR_SUCCESS);
    CHECK(failed == 99);
}

