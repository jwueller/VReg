// Delta export: an overlay's upper layer as a file that, applied over the same lower registry, reproduces the merged
// view.

#include "regfile_test.h"

using namespace VReg;
using namespace VReg::RegFile;
using namespace VReg::Test;

namespace
{
const std::wstring kHeader = L"Windows Registry Editor Version 5.00\r\n\r\n";
constexpr const wchar_t* kMount = L"VRegDelta";

/// Two overlays over the same lower key of an app hive: one to change, one to apply the exported delta to.
class DeltaFixture
{
public:
    DeltaFixture()
        : m_modified(m_native, m_appHive.Root(), L"Lower")
        , m_fresh(m_native, m_appHive.Root(), L"Lower")
        , m_modifiedRegistry(&m_native)
        , m_freshRegistry(&m_native)
    {
        REQUIRE(
            m_native.CreateKeyExW(
                m_appHive.Root(),
                L"Lower",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &m_hLower,
                nullptr
            )
            == ERROR_SUCCESS
        );
        REQUIRE(m_modifiedRegistry.Mount(HKEY_CURRENT_USER, kMount, m_modified) == ERROR_SUCCESS);
        REQUIRE(m_freshRegistry.Mount(HKEY_CURRENT_USER, kMount, m_fresh) == ERROR_SUCCESS);
    }

    ~DeltaFixture()
    {
        m_native.CloseKey(
            m_hLower
        );
    }

    DeltaFixture(const DeltaFixture&) = delete;
    DeltaFixture& operator=(const DeltaFixture&) = delete;

    /// Sets a value of the lower registry, creating the key.
    void LowerSet(const wchar_t* const pszPath, const wchar_t* const pszName, const DWORD dwType, const std::vector<std::byte>& data)
    {
        HKEY hKey{};
        REQUIRE(
            m_native.CreateKeyExW(
                m_hLower,
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
        REQUIRE(
            m_native.SetValueExW(
                hKey,
                pszName,
                0,
                dwType,
                reinterpret_cast<const BYTE*>(data.data()),
                static_cast<DWORD>(data.size())
            )
            == ERROR_SUCCESS
        );
        m_native.CloseKey(
            hKey
        );
    }

    void LowerKey(const wchar_t* const pszPath)
    {
        HKEY hKey{};
        REQUIRE(
            m_native.CreateKeyExW(
                m_hLower,
                pszPath,
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ) == ERROR_SUCCESS
        );
        m_native.CloseKey(
            hKey
        );
    }

    /// Changes to the modified overlay take paths relative to its root.
    void Set(const std::wstring& path, const wchar_t* const pszName, const DWORD dwType, const std::vector<std::byte>& data)
    {
        MustSet(m_modifiedRegistry, Full(path).c_str(), pszName, dwType, data);
    }

    void CreateKey(const std::wstring& path)
    {
        HKEY hKey{};
        REQUIRE(
            m_modifiedRegistry
                .CreateKeyExW(
                    HKEY_CURRENT_USER,
                    Full(path).c_str(),
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
        m_modifiedRegistry.CloseKey(
            hKey
        );
    }

    void Open(const std::wstring& path)
    {
        m_modifiedRegistry.CloseKey(
            MustOpen(m_modifiedRegistry, Full(path).c_str())
        );
    }

    void DeleteValue(const std::wstring& path, const wchar_t* const pszName)
    {
        const HKEY hKey = MustOpen(m_modifiedRegistry, Full(path).c_str(), KEY_SET_VALUE);
        CHECK(
            m_modifiedRegistry.DeleteValueW(
                hKey,
                pszName
            ) == ERROR_SUCCESS
        );
        m_modifiedRegistry.CloseKey(
            hKey
        );
    }

    void DeleteKey(const std::wstring& path)
    {
        CHECK(
            m_modifiedRegistry.DeleteKeyExW(
                HKEY_CURRENT_USER,
                Full(path).c_str(),
                0,
                0
            ) == ERROR_SUCCESS
        );
    }

    /// `RegDeleteTree` on an open key: empties it but keeps the key.
    void EmptyKey(const std::wstring& path)
    {
        const HKEY hKey = MustOpen(m_modifiedRegistry, Full(path).c_str(), KEY_ALL_ACCESS);
        CHECK(
            m_modifiedRegistry.DeleteTreeW(
                hKey,
                nullptr
            ) == ERROR_SUCCESS
        );
        m_modifiedRegistry.CloseKey(
            hKey
        );
    }

    void DeleteTree(const std::wstring& path)
    {
        CHECK(
            m_modifiedRegistry.DeleteTreeW(
                HKEY_CURRENT_USER,
                Full(path).c_str()
            ) == ERROR_SUCCESS
        );
    }

    [[nodiscard]] std::vector<std::byte> Delta(const Format format = Format::Unicode)
    {
        std::vector<std::byte> file;
        REQUIRE(ExportDelta(m_modified, HKEY_CURRENT_USER, kMount, format, file) == ERROR_SUCCESS);
        return file;
    }

    /// Applies the exported delta to the fresh overlay and requires both merged views to match.
    void CheckRoundTrip(const Format format = Format::Unicode)
    {
        const auto file = Delta(format);
        Script script;
        ParseError error;
        REQUIRE(Parse(file, script, &error) == ERROR_SUCCESS);
        std::size_t iFailed{};
        REQUIRE(Apply(script, m_freshRegistry, &iFailed) == ERROR_SUCCESS);

        std::vector<std::byte> expected;
        std::vector<std::byte> actual;
        REQUIRE(ExportSnapshot(m_modifiedRegistry, HKEY_CURRENT_USER, kMount, Format::Unicode, expected) == ERROR_SUCCESS);
        REQUIRE(ExportSnapshot(m_freshRegistry, HKEY_CURRENT_USER, kMount, Format::Unicode, actual) == ERROR_SUCCESS);
        CHECK(AsString(actual) == AsString(expected));

        // Applying the delta leaves the same upper layer behind.
        std::vector<std::byte> again;
        REQUIRE(ExportDelta(m_fresh, HKEY_CURRENT_USER, kMount, format, again) == ERROR_SUCCESS);
        CHECK(AsString(again) == AsString(file));
    }

    [[nodiscard]] OverlayHive& Modified() noexcept
    {
        return m_modified;
    }

private:
    static std::wstring Full(const std::wstring& path)
    {
        return path.empty() ? std::wstring(kMount) : std::wstring(kMount) + L'\\' + path;
    }

    AppHive m_appHive;
    NativeRegistry m_native;
    HKEY m_hLower{};
    OverlayHive m_modified;
    OverlayHive m_fresh;
    VirtualRegistry m_modifiedRegistry;
    VirtualRegistry m_freshRegistry;
};

std::vector<std::byte> Text(const std::wstring& body)
{
    return Utf16File(kHeader + body);
}
} // namespace

TEST_CASE("An overlay with no changes has an empty delta")
{
    DeltaFixture f;
    f.LowerSet(L"A", L"v", REG_DWORD, DwordBytes(1));
    f.Open(L"A");
    CHECK(f.Delta() == Utf16File(kHeader));
}

TEST_CASE("Delta names its keys after the mount point")
{
    DeltaFixture f;
    f.CreateKey(L"New");

    std::vector<std::byte> file;
    REQUIRE(ExportDelta(f.Modified(), HKEY_LOCAL_MACHINE, L"Software\\Mount", Format::Unicode, file) == ERROR_SUCCESS);
    CHECK(
        file
        == Text(
            L"[HKEY_LOCAL_MACHINE\\Software\\Mount\\New]\r\n\r\n"
        )
    );

    REQUIRE(ExportDelta(f.Modified(), HKEY_USERS, L"", Format::Unicode, file) == ERROR_SUCCESS);
    CHECK(file == Text(L"[HKEY_USERS\\New]\r\n\r\n"));

    CHECK(ExportDelta(f.Modified(), reinterpret_cast<HKEY>(0x1234), L"", Format::Unicode, file) == ERROR_INVALID_HANDLE);
}

TEST_CASE("Delta writes value and key tombstones")
{
    DeltaFixture f;
    f.LowerSet(L"", L"Keep", REG_DWORD, DwordBytes(1));
    f.LowerSet(L"", L"Drop", REG_DWORD, DwordBytes(2));
    f.LowerSet(L"Gone", L"x", REG_DWORD, DwordBytes(3));
    f.LowerSet(L"Stay", L"y", REG_DWORD, DwordBytes(4));
    f.DeleteValue(L"", L"Drop");
    f.DeleteKey(L"Gone");

    CHECK(
        f.Delta()
        == Text(
            L"[HKEY_CURRENT_USER\\VRegDelta]\r\n"
            L"\"Drop\"=-\r\n"
            L"\r\n"
            L"[-HKEY_CURRENT_USER\\VRegDelta\\Gone]\r\n"
            L"\r\n"
        )
    );
    f.CheckRoundTrip();
}

TEST_CASE("Delta writes an opaque key as a delete followed by a create")
{
    DeltaFixture f;
    f.LowerSet(L"Old", L"a", REG_DWORD, DwordBytes(1));
    f.LowerSet(L"Old\\Child", L"b", REG_DWORD, DwordBytes(2));
    f.DeleteTree(L"Old");
    f.Set(L"Old", L"n", REG_DWORD, DwordBytes(3));

    CHECK(
        f.Delta()
        == Text(
            L"[-HKEY_CURRENT_USER\\VRegDelta\\Old]\r\n"
            L"\r\n"
            L"[HKEY_CURRENT_USER\\VRegDelta\\Old]\r\n"
            L"\"n\"=dword:00000003\r\n"
            L"\r\n"
        )
    );
    f.CheckRoundTrip();
}

TEST_CASE("A key deleted and created again hides the lower key")
{
    DeltaFixture f;
    f.LowerSet(L"Re", L"lower", REG_SZ, StringBytes(L"x"));
    f.LowerSet(L"Re\\Sub", L"deep", REG_DWORD, DwordBytes(1));
    f.DeleteTree(L"Re");
    f.CreateKey(L"Re");
    f.Set(L"Re", L"upper", REG_SZ, StringBytes(L"y"));
    f.CreateKey(L"Re\\Fresh");

    CHECK(
        f.Delta()
        == Text(
            L"[-HKEY_CURRENT_USER\\VRegDelta\\Re]\r\n"
            L"\r\n"
            L"[HKEY_CURRENT_USER\\VRegDelta\\Re]\r\n"
            L"\"upper\"=\"y\"\r\n"
            L"\r\n"
            L"[HKEY_CURRENT_USER\\VRegDelta\\Re\\Fresh]\r\n"
            L"\r\n"
        )
    );
    f.CheckRoundTrip();
}

TEST_CASE("A key emptied in place hides the lower key")
{
    DeltaFixture f;
    f.LowerSet(L"E", L"lower", REG_DWORD, DwordBytes(1));
    f.LowerSet(L"E\\Sub", L"deep", REG_DWORD, DwordBytes(2));
    f.EmptyKey(L"E");

    CHECK(
        f.Delta()
        == Text(
            L"[-HKEY_CURRENT_USER\\VRegDelta\\E]\r\n"
            L"\r\n"
            L"[HKEY_CURRENT_USER\\VRegDelta\\E]\r\n"
            L"\r\n"
        )
    );
    f.CheckRoundTrip();
}

TEST_CASE("A lower key with an upper tombstone below it is written as a key")
{
    DeltaFixture f;
    f.LowerSet(L"T", L"v", REG_DWORD, DwordBytes(1));
    f.LowerSet(L"T", L"w", REG_DWORD, DwordBytes(2));
    f.LowerKey(L"Other");
    f.Open(L"Other");
    f.DeleteValue(L"T", L"v");

    CHECK(
        f.Delta()
        == Text(
            L"[HKEY_CURRENT_USER\\VRegDelta\\T]\r\n"
            L"\"v\"=-\r\n"
            L"\r\n"
        )
    );
    f.CheckRoundTrip();
}

TEST_CASE("Delta lists values and keys in sorted order")
{
    DeltaFixture f;
    f.Set(L"b", L"z", REG_DWORD, DwordBytes(1));
    f.Set(L"b", L"B", REG_DWORD, DwordBytes(2));
    f.Set(L"b", L"a", REG_DWORD, DwordBytes(3));
    f.CreateKey(L"C");
    f.CreateKey(L"a");

    CHECK(
        f.Delta()
        == Text(
            L"[HKEY_CURRENT_USER\\VRegDelta\\a]\r\n"
            L"\r\n"
            L"[HKEY_CURRENT_USER\\VRegDelta\\b]\r\n"
            L"\"a\"=dword:00000003\r\n"
            L"\"B\"=dword:00000002\r\n"
            L"\"z\"=dword:00000001\r\n"
            L"\r\n"
            L"[HKEY_CURRENT_USER\\VRegDelta\\C]\r\n"
            L"\r\n"
        )
    );
}

TEST_CASE("A delta reproduces a mix of changes over a lower registry")
{
    DeltaFixture f;
    f.LowerSet(L"", L"", REG_SZ, StringBytes(L"root default"));
    f.LowerSet(L"A", L"v1", REG_SZ, StringBytes(L"one"));
    f.LowerSet(L"A", L"v2", REG_DWORD, DwordBytes(2));
    f.LowerSet(L"A\\B", L"w", REG_BINARY, {std::byte{1}, std::byte{2}});
    f.LowerSet(L"A\\B\\C", L"z", REG_MULTI_SZ, BytesOf(std::wstring(L"p\0q\0\0", 5)));
    f.LowerSet(L"D", L"d", REG_DWORD, DwordBytes(4));
    f.LowerSet(L"D\\E", L"e", REG_DWORD, DwordBytes(5));
    f.LowerSet(L"D\\E\\Deep", L"f", REG_QWORD, std::vector<std::byte>(8, std::byte{9}));
    f.LowerKey(L"Untouched\\Child");

    f.Set(L"A", L"v1", REG_EXPAND_SZ, StringBytes(L"%TEMP%"));
    f.Set(L"A", L"v3", REG_DWORD, DwordBytes(3));
    f.Set(L"A\\New\\Nested", L"n", REG_SZ, StringBytes(L"nested"));
    f.Set(L"A\\B", L"w2", REG_DWORD, DwordBytes(6));
    f.DeleteValue(L"A", L"v2");
    f.DeleteKey(L"A\\B\\C");
    f.DeleteTree(L"D");
    f.Set(L"D", L"after", REG_DWORD, DwordBytes(7));
    f.CreateKey(L"D\\F");
    f.Set(L"X\\Y\\Z", L"", REG_SZ, StringBytes(L"only upper"));
    f.Open(L"Untouched\\Child");

    f.CheckRoundTrip();
    f.CheckRoundTrip(Format::Utf8);
}
