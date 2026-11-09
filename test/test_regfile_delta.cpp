// Delta export: an overlay's upper layer as a file that, applied over the same
// lower registry, reproduces the merged view.

#include "regfile_test.h"

using namespace VRegTest;

namespace
{
const std::wstring HEADER = L"Windows Registry Editor Version 5.00\r\n\r\n";
constexpr const wchar_t* MOUNT = L"VRegDelta";

/// Two overlays over the same lower key of an app hive: one to change, one to
/// apply the exported delta to.
class DeltaFixture
{
public:
    DeltaFixture() :
        m_native(MakeNativeRegistry())
    {
        REQUIRE(
            VReg_CreateKeyExW(
                m_native,
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
        m_modified = MakeLayeredHive(m_native, m_appHive.Root(), L"Lower");
        m_fresh = MakeLayeredHive(m_native, m_appHive.Root(), L"Lower");
        m_modifiedRegistry = MakeVirtualRegistry(m_native);
        m_freshRegistry = MakeVirtualRegistry(m_native);
        REQUIRE(
            VReg_Mount(
                m_modifiedRegistry,
                HKEY_CURRENT_USER,
                MOUNT,
                m_modified,
                0
            ) == ERROR_SUCCESS
        );
        REQUIRE(
            VReg_Mount(
                m_freshRegistry,
                HKEY_CURRENT_USER,
                MOUNT,
                m_fresh,
                0
            ) == ERROR_SUCCESS
        );
    }

    ~DeltaFixture()
    {
        VReg_CloseKey(
            m_native,
            m_hLower
        );
    }

    DeltaFixture(const DeltaFixture&) = delete;
    DeltaFixture& operator=(const DeltaFixture&) = delete;

    /// Sets a value of the lower registry, creating the key.
    void LowerSet(
        const wchar_t* const pszPath,
        const wchar_t* const pszName,
        const DWORD dwType,
        const std::vector<std::byte>& data
    )
    {
        HKEY hKey{};
        REQUIRE(
            VReg_CreateKeyExW(
                m_native,
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
            VReg_SetValueExW(
                m_native,
                hKey,
                pszName,
                0,
                dwType,
                reinterpret_cast<const BYTE*>(data.data()),
                static_cast<DWORD>(data.size())
            )
            == ERROR_SUCCESS
        );
        VReg_CloseKey(
            m_native,
            hKey
        );
    }

    void LowerKey(const wchar_t* const pszPath)
    {
        HKEY hKey{};
        REQUIRE(
            VReg_CreateKeyExW(
                m_native,
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
        VReg_CloseKey(
            m_native,
            hKey
        );
    }

    /// Changes to the modified overlay take paths relative to its root.
    void Set(
        const std::wstring& path,
        const wchar_t* const pszName,
        const DWORD dwType,
        const std::vector<std::byte>& data
    )
    {
        MustSet(m_modifiedRegistry, Full(path).c_str(), pszName, dwType, data);
    }

    void CreateKey(const std::wstring& path)
    {
        HKEY hKey{};
        REQUIRE(
            VReg_CreateKeyExW(
                m_modifiedRegistry,
                HKEY_CURRENT_USER,
                Full(path).c_str(),
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ) == ERROR_SUCCESS
        );
        VReg_CloseKey(
            m_modifiedRegistry,
            hKey
        );
    }

    void Open(const std::wstring& path)
    {
        VReg_CloseKey(
            m_modifiedRegistry,
            MustOpen(m_modifiedRegistry, Full(path).c_str())
        );
    }

    void DeleteValue(const std::wstring& path, const wchar_t* const pszName)
    {
        const HKEY hKey = MustOpen(
            m_modifiedRegistry,
            Full(path).c_str(),
            KEY_SET_VALUE
        );
        CHECK(
            VReg_DeleteValueW(
                m_modifiedRegistry,
                hKey,
                pszName
            ) == ERROR_SUCCESS
        );
        VReg_CloseKey(
            m_modifiedRegistry,
            hKey
        );
    }

    void DeleteKey(const std::wstring& path)
    {
        CHECK(
            VReg_DeleteKeyExW(
                m_modifiedRegistry,
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
        const HKEY hKey = MustOpen(
            m_modifiedRegistry,
            Full(path).c_str(),
            KEY_ALL_ACCESS
        );
        CHECK(
            VReg_DeleteTreeW(
                m_modifiedRegistry,
                hKey,
                nullptr
            ) == ERROR_SUCCESS
        );
        VReg_CloseKey(
            m_modifiedRegistry,
            hKey
        );
    }

    void DeleteTree(const std::wstring& path)
    {
        CHECK(
            VReg_DeleteTreeW(
                m_modifiedRegistry,
                HKEY_CURRENT_USER,
                Full(path).c_str()
            ) == ERROR_SUCCESS
        );
    }

    [[nodiscard]] std::vector<std::byte> Delta(
        const VRegFormat format = VREG_FORMAT_UNICODE
    )
    {
        return ExportDelta(
            m_modified,
            HKEY_CURRENT_USER,
            MOUNT,
            format
        );
    }

    /// Applies the exported delta to the fresh overlay and requires both merged
    /// views to match.
    void CheckRoundTrip(const VRegFormat format = VREG_FORMAT_UNICODE)
    {
        const auto file = Delta(format);
        OwnedScript script;
        VRegParseError error{};
        REQUIRE(Parse(file, script, &error) == ERROR_SUCCESS);
        SIZE_T iFailed{};
        REQUIRE(Apply(script, m_freshRegistry, &iFailed) == ERROR_SUCCESS);

        const auto expected = ExportSnapshot(
            m_modifiedRegistry,
            HKEY_CURRENT_USER,
            MOUNT,
            VREG_FORMAT_UNICODE
        );
        const auto actual = ExportSnapshot(
            m_freshRegistry,
            HKEY_CURRENT_USER,
            MOUNT,
            VREG_FORMAT_UNICODE
        );
        CHECK(AsString(actual) == AsString(expected));

        // Applying the delta leaves the same upper layer behind.
        const auto again = ExportDelta(
            m_fresh,
            HKEY_CURRENT_USER,
            MOUNT,
            format
        );
        CHECK(AsString(again) == AsString(file));
    }

    [[nodiscard]] VRegHive* Modified() noexcept
    {
        return m_modified;
    }

private:
    static std::wstring Full(const std::wstring& path)
    {
        return
            path.empty()
                ? std::wstring(MOUNT)
                : std::wstring(MOUNT) + L'\\' + path;
    }

    AppHive m_appHive;
    OwnedRegistry m_native;
    HKEY m_hLower{};
    OwnedHive m_modified;
    OwnedHive m_fresh;
    OwnedRegistry m_modifiedRegistry;
    OwnedRegistry m_freshRegistry;
};

std::vector<std::byte> Text(const std::wstring& body)
{
    return Utf16File(HEADER + body);
}
} // namespace

TEST_CASE("An overlay with no changes has an empty delta")
{
    DeltaFixture f;
    f.LowerSet(L"A", L"v", REG_DWORD, DwordBytes(1));
    f.Open(L"A");
    CHECK(f.Delta() == Utf16File(HEADER));
}

TEST_CASE("Delta names its keys after the mount point")
{
    DeltaFixture f;
    f.CreateKey(L"New");

    std::vector<std::byte> file = ExportDelta(
        f.Modified(),
        HKEY_LOCAL_MACHINE,
        L"Software\\Mount",
        VREG_FORMAT_UNICODE
    );
    CHECK(
        file
        == Text(
            L"[HKEY_LOCAL_MACHINE\\Software\\Mount\\New]\r\n\r\n"
        )
    );

    file = ExportDelta(
        f.Modified(),
        HKEY_USERS,
        L"",
        VREG_FORMAT_UNICODE
    );
    CHECK(file == Text(L"[HKEY_USERS\\New]\r\n\r\n"));

    CHECK(
        TryExportDelta(
            f.Modified(),
            reinterpret_cast<HKEY>(0x1234),
            L"",
            VREG_FORMAT_UNICODE,
            file
        ) == ERROR_INVALID_HANDLE
    );
}

TEST_CASE("Delta writes value and key tombstones")
{
    DeltaFixture f;
    // @formatter:off
    f.LowerSet(L"",     L"Keep", REG_DWORD, DwordBytes(1));
    f.LowerSet(L"",     L"Drop", REG_DWORD, DwordBytes(2));
    f.LowerSet(L"Gone", L"x",    REG_DWORD, DwordBytes(3));
    f.LowerSet(L"Stay", L"y",    REG_DWORD, DwordBytes(4));
    // @formatter:on
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
    // @formatter:off
    f.LowerSet(L"",           L"",   REG_SZ,       StringBytes(L"root default"));
    f.LowerSet(L"A",          L"v1", REG_SZ,       StringBytes(L"one"));
    f.LowerSet(L"A",          L"v2", REG_DWORD,    DwordBytes(2));
    f.LowerSet(L"A\\B",       L"w",  REG_BINARY,   {std::byte{1}, std::byte{2}});
    f.LowerSet(L"A\\B\\C",    L"z",  REG_MULTI_SZ, BytesOf(std::wstring(L"p\0q\0\0", 5)));
    f.LowerSet(L"D",          L"d",  REG_DWORD,    DwordBytes(4));
    f.LowerSet(L"D\\E",       L"e",  REG_DWORD,    DwordBytes(5));
    f.LowerSet(L"D\\E\\Deep", L"f",  REG_QWORD,    std::vector<std::byte>(8, std::byte{9}));
    // @formatter:on
    f.LowerKey(L"Untouched\\Child");

    // @formatter:off
    f.Set(L"A",              L"v1", REG_EXPAND_SZ, StringBytes(L"%TEMP%"));
    f.Set(L"A",              L"v3", REG_DWORD,     DwordBytes(3));
    f.Set(L"A\\New\\Nested", L"n",  REG_SZ,        StringBytes(L"nested"));
    f.Set(L"A\\B",           L"w2", REG_DWORD,     DwordBytes(6));
    // @formatter:on
    f.DeleteValue(L"A", L"v2");
    f.DeleteKey(L"A\\B\\C");
    f.DeleteTree(L"D");
    f.Set(L"D", L"after", REG_DWORD, DwordBytes(7));
    f.CreateKey(L"D\\F");
    f.Set(L"X\\Y\\Z", L"", REG_SZ, StringBytes(L"only upper"));
    f.Open(L"Untouched\\Child");

    f.CheckRoundTrip();
    f.CheckRoundTrip(VREG_FORMAT_UTF8);
}
