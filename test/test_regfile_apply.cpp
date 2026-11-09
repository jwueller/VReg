// Applying a parsed script to a real private hive.

#include "regfile_test.h"

#include <catch2/generators/catch_generators.hpp>

using namespace VRegTest;

namespace
{
const std::wstring HEADER = L"Windows Registry Editor Version 5.00\r\n";

/// An operation built by hand: the strings and bytes the C operation points
/// at live here, so a Spec must outlive the call that applies its View.
struct Spec
{
    VRegOperationKind kind{};
    std::wstring path;
    std::wstring name;
    DWORD dwType{};
    std::vector<std::byte> data;

    [[nodiscard]] VRegOperation View() const
    {
        return {
            .kind = kind,
            .hRoot = HKEY_CURRENT_USER,
            .pszPath = path.c_str(),
            .pszName = name.c_str(),
            .dwType = dwType,
            .pData = reinterpret_cast<const BYTE*>(data.data()),
            .cbData = static_cast<DWORD>(data.size()),
            .line = 0,
        };
    }
};

bool KeyExists(VReg* const registry, const wchar_t* const pszPath) noexcept
{
    HKEY hKey{};
    const auto status = VReg_RegOpenKeyExW(
        registry,
        HKEY_CURRENT_USER,
        pszPath,
        0,
        KEY_READ,
        &hKey
    );
    if (status == ERROR_SUCCESS)
        VReg_RegCloseKey(
            registry,
            hKey
        );
    return status == ERROR_SUCCESS;
}
} // namespace

TEST_CASE("Apply creates keys and sets every value form")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());

    const auto script = MustParse(
        HEADER +
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
    SIZE_T failed = 99;
    REQUIRE(Apply(script, registry, &failed) == ERROR_SUCCESS);
    CHECK(failed == 99);

    const auto get = [&](
        const wchar_t* const pszName,
        const DWORD dwType,
        const std::vector<std::byte>& data
    )
    {
        CheckValue(registry, L"Software\\T\\Deep", pszName, dwType, data);
    };
    // @formatter:off
    get(L"",     REG_SZ,        StringBytes(L"default"));
    get(L"str",  REG_SZ,        StringBytes(L"a\\b"));
    get(L"dw",   REG_DWORD,     DwordBytes(0x2a));
    get(L"bin",  REG_BINARY,    {std::byte{1}, std::byte{2}, std::byte{3}});
    get(L"exp",  REG_EXPAND_SZ, StringBytes(L"%P"));
    get(L"qw",   REG_QWORD,     std::vector<std::byte>{std::byte{1}, {}, {}, {}, {}, {}, {}, {}});
    get(L"none", REG_NONE,      {});
    get(L"odd",  REG_SZ,        {std::byte{0x61}});
    // @formatter:on
}

TEST_CASE("Apply overwrites values and keeps unrelated ones")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    MustSet(registry, L"T", L"keep", REG_DWORD, DwordBytes(1));
    MustSet(registry, L"T", L"change", REG_DWORD, DwordBytes(1));

    REQUIRE(
        Apply(
            MustParse(HEADER + L"[HKCU\\T]\r\n\"change\"=\"now text\"\r\n"),
            registry,
            nullptr
        ) == ERROR_SUCCESS
    );
    CheckValue(registry, L"T", L"keep", REG_DWORD, DwordBytes(1));
    CheckValue(registry, L"T", L"change", REG_SZ, StringBytes(L"now text"));
}

TEST_CASE("Apply deletes values and keys")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    // @formatter:off
    MustSet(registry, L"T",                  L"a", REG_DWORD, DwordBytes(1));
    MustSet(registry, L"T",                  L"b", REG_DWORD, DwordBytes(2));
    MustSet(registry, L"T",                  L"",  REG_DWORD, DwordBytes(3));
    MustSet(registry, L"Gone\\Deep\\Deeper", L"x", REG_DWORD, DwordBytes(4));
    MustSet(registry, L"Gone",               L"y", REG_DWORD, DwordBytes(5));
    // @formatter:on

    SECTION("values, including the default one")
    {
        REQUIRE(
            Apply(
                MustParse(HEADER + L"[HKCU\\T]\r\n\"a\"=-\r\n@=-\r\n"),
                registry,
                nullptr
            ) == ERROR_SUCCESS
        );
        const HKEY hKey = MustOpen(registry, L"T");
        DWORD dwType{};
        CHECK(
            VReg_RegQueryValueExW(
                registry,
                hKey,
                L"a",
                nullptr,
                &dwType,
                nullptr,
                nullptr
            ) == ERROR_FILE_NOT_FOUND
        );
        CHECK(
            VReg_RegQueryValueExW(
                registry,
                hKey,
                L"",
                nullptr,
                &dwType,
                nullptr,
                nullptr
            ) == ERROR_FILE_NOT_FOUND
        );
        CHECK(
            VReg_RegQueryValueExW(
                registry,
                hKey,
                L"b",
                nullptr,
                &dwType,
                nullptr,
                nullptr
            ) == ERROR_SUCCESS
        );
        VReg_RegCloseKey(
            registry,
            hKey
        );
    }

    SECTION("a key with its whole subtree")
    {
        REQUIRE(
            Apply(
                MustParse(HEADER + L"[-HKCU\\Gone]\r\n"),
                registry,
                nullptr
            ) == ERROR_SUCCESS
        );
        CHECK_FALSE(KeyExists(registry, L"Gone"));
        CHECK(KeyExists(registry, L"T"));
    }

    SECTION("what does not exist")
    {
        REQUIRE(
            Apply(
                MustParse(
                    HEADER
                    + L"[-HKCU\\Missing\\Deep]\r\n[-HKCU\\Gone\\Deep\\Deeper\\Nope]\r\n"
                ),
                registry,
                nullptr
            ) == ERROR_SUCCESS
        );
        CHECK(KeyExists(registry, L"Gone\\Deep\\Deeper"));
    }

    SECTION("a value of a key or value that does not exist")
    {
        const Spec missingKey{VREG_OPERATION_DELETE_VALUE, L"Missing", L"v", 0, {}};
        const Spec missingValue{VREG_OPERATION_DELETE_VALUE, L"T", L"never", 0, {}};
        const std::vector<VRegOperation> operations{
            missingKey.View(),
            missingValue.View(),
        };
        CHECK(
            VReg_ApplyOperations(
                operations.data(),
                operations.size(),
                registry,
                0,
                nullptr
            ) == ERROR_SUCCESS
        );
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
        Apply(
            MustParse(HEADER + L"[-HKCU\\T]\r\n[HKCU\\T]\r\n\"z\"=dword:2\r\n"),
            registry,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK_FALSE(KeyExists(registry, L"T\\Old"));
    CheckValue(registry, L"T", L"z", REG_DWORD, DwordBytes(2));
    const HKEY hKey = MustOpen(registry, L"T");
    CHECK(
        VReg_RegQueryValueExW(
            registry,
            hKey,
            L"y",
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ) == ERROR_FILE_NOT_FOUND
    );
    VReg_RegCloseKey(
        registry,
        hKey
    );
}

TEST_CASE("Apply stops at the first failing operation")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());

    const auto first = MustParse(HEADER + L"[HKCU\\First]\r\n\"a\"=dword:1\r\n");
    const auto never = MustParse(HEADER + L"[HKCU\\Never]\r\n");
    const Spec orphan{VREG_OPERATION_SET_VALUE, L"NoSuchKey", L"v", REG_DWORD, DwordBytes(1)};
    std::vector<VRegOperation> operations;
    for (const VRegOperation& operation : Operations(first))
        operations.push_back(operation);
    operations.push_back(orphan.View());
    operations.push_back(Operations(never)[0]);

    SIZE_T failed = 99;
    CHECK(
        VReg_ApplyOperations(
            operations.data(),
            operations.size(),
            registry,
            0,
            &failed
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(failed == 2);
    CheckValue(registry, L"First", L"a", REG_DWORD, DwordBytes(1));
    CHECK_FALSE(KeyExists(registry, L"NoSuchKey"));
    CHECK_FALSE(KeyExists(registry, L"Never"));

    CHECK(
        VReg_ApplyOperations(
            operations.data(),
            operations.size(),
            registry,
            0,
            nullptr
        ) == ERROR_FILE_NOT_FOUND
    );
}

TEST_CASE("Apply continues past failing operations when told to")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());

    const auto first = MustParse(HEADER + L"[HKCU\\First]\r\n");
    const auto last = MustParse(HEADER + L"[HKCU\\Last]\r\n");
    const Spec orphan{VREG_OPERATION_SET_VALUE, L"NoSuchKey", L"v", REG_DWORD, DwordBytes(1)};
    const Spec deleteRoot{VREG_OPERATION_DELETE_KEY, L"", L"", 0, {}};
    const std::vector<VRegOperation> operations{
        Operations(first)[0],
        orphan.View(),
        deleteRoot.View(),
        Operations(last)[0],
    };

    SECTION("every operation runs, and the first failure is reported")
    {
        SIZE_T failed = 99;
        CHECK(
            VReg_ApplyOperations(
                operations.data(),
                operations.size(),
                registry,
                VREG_APPLY_CONTINUE_ON_ERROR,
                &failed
            ) == ERROR_FILE_NOT_FOUND
        );
        CHECK(failed == 1);
        CHECK(KeyExists(registry, L"First"));
        CHECK_FALSE(KeyExists(registry, L"NoSuchKey"));
        CHECK(KeyExists(registry, L"Last"));
    }

    SECTION("the failed index is optional")
    {
        CHECK(
            VReg_ApplyOperations(
                operations.data(),
                operations.size(),
                registry,
                VREG_APPLY_CONTINUE_ON_ERROR,
                nullptr
            ) == ERROR_FILE_NOT_FOUND
        );
        CHECK(KeyExists(registry, L"Last"));
    }

    SECTION("success when nothing fails")
    {
        const std::vector<VRegOperation> sound{
            Operations(first)[0],
            Operations(last)[0],
        };
        SIZE_T failed = 99;
        CHECK(
            VReg_ApplyOperations(
                sound.data(),
                sound.size(),
                registry,
                VREG_APPLY_CONTINUE_ON_ERROR,
                &failed
            ) == ERROR_SUCCESS
        );
        CHECK(failed == 99);
    }
}

TEST_CASE("Apply refuses unknown flags before doing anything")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    const OwnedScript script = MustParse(HEADER + L"[HKCU\\Never]\r\n");
    const auto operations = Operations(script);

    SIZE_T failed = 99;
    CHECK(
        VReg_ApplyOperations(
            operations.data(),
            operations.size(),
            registry,
            ~VREG_APPLY_CONTINUE_ON_ERROR,
            &failed
        ) == ERROR_INVALID_PARAMETER
    );
    CHECK(failed == 99);
    CHECK_FALSE(KeyExists(registry, L"Never"));
}

TEST_CASE("Apply refuses an operation that would delete a root")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    const OwnedScript setup = MustParse(HEADER + L"[HKCU\\Keep]\r\n");
    REQUIRE(Apply(setup, registry, nullptr) == ERROR_SUCCESS);

    const Spec deleteRoot{VREG_OPERATION_DELETE_KEY, L"", L"", 0, {}};
    const VRegOperation operation = deleteRoot.View();

    SIZE_T failed = 99;
    CHECK(
        VReg_ApplyOperations(
            &operation,
            1,
            registry,
            0,
            &failed
        ) == ERROR_INVALID_PARAMETER
    );
    CHECK(failed == 0);
    CHECK(KeyExists(registry, L"Keep"));
}

TEST_CASE("An empty script applies to nothing")
{
    AppHive hive;
    AppHiveRegistry registry(hive.Root());
    SIZE_T failed = 99;
    CHECK(
        VReg_ApplyOperations(
            nullptr,
            0,
            registry,
            0,
            &failed
        ) == ERROR_SUCCESS
    );
    CHECK(failed == 99);
}
