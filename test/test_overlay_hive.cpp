#include "oracle.h"

#include <VReg/handle.h>
#include <VReg/native_registry.h>
#include <VReg/overlay_hive.h>
#include <VReg/virtual_registry.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chrono>
#include <format>
#include <functional>
#include <future>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace VReg;
using namespace VReg::Test;

namespace
{
/// A registry that forwards to another one, can fail chosen calls with chosen
/// statuses and reports every call it receives before forwarding it.
class InterceptRegistry final : public Registry
{
public:
    explicit InterceptRegistry(Registry& inner) noexcept
        : m_inner(inner)
    {
    }

    /// Calls named here (`OpenKeyExW`, ...) answer the status instead of being forwarded.
    std::map<std::string, LSTATUS> failures;
    std::function<void(const std::string&)> onCall;

    LSTATUS OpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) noexcept override
    {
        if (const auto status = Enter("OpenKeyExW"))
            return *status;
        return m_inner.OpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
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
        if (const auto status = Enter("CreateKeyExW"))
            return *status;
        return m_inner.CreateKeyExW(
            hKey,
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
        return m_inner.CloseKey(hKey);
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
        if (const auto status = Enter("QueryValueExW"))
            return *status;
        return m_inner.QueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
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
        if (const auto status = Enter("SetValueExW"))
            return *status;
        return m_inner.SetValueExW(hKey, lpValueName, Reserved, dwType, lpData, cbData);
    }

    LSTATUS DeleteValueW(HKEY hKey, LPCWSTR lpValueName) noexcept override
    {
        if (const auto status = Enter("DeleteValueW"))
            return *status;
        return m_inner.DeleteValueW(hKey, lpValueName);
    }

    LSTATUS DeleteKeyExW(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved) noexcept override
    {
        if (const auto status = Enter("DeleteKeyExW"))
            return *status;
        return m_inner.DeleteKeyExW(hKey, lpSubKey, samDesired, Reserved);
    }

    LSTATUS DeleteTreeW(HKEY hKey, LPCWSTR lpSubKey) noexcept override
    {
        if (const auto status = Enter("DeleteTreeW"))
            return *status;
        return m_inner.DeleteTreeW(hKey, lpSubKey);
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
        if (const auto status = Enter("EnumKeyExW"))
            return *status;
        return m_inner.EnumKeyExW(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime);
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
        if (const auto status = Enter("EnumValueW"))
            return *status;
        return m_inner.EnumValueW(hKey, dwIndex, lpValueName, lpcchValueName, lpReserved, lpType, lpData, lpcbData);
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
        if (const auto status = Enter("QueryInfoKeyW"))
            return *status;
        return m_inner.QueryInfoKeyW(
            hKey,
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
        if (const auto status = Enter("LoadMUIStringW"))
            return *status;
        return m_inner.LoadMUIStringW(hKey, pszValue, pszOutBuf, cbOutBuf, pcbData, Flags, pszDirectory);
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
        if (const auto status = Enter("GetValueW"))
            return *status;
        return m_inner.GetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
    }

    LSTATUS NotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept override
    {
        if (const auto status = Enter("NotifyChangeKeyValue"))
            return *status;
        return m_inner.NotifyChangeKeyValue(hKey, bWatchSubtree, dwNotifyFilter, hEvent, fAsynchronous);
    }

    LSTATUS FlushKey(HKEY hKey) noexcept override
    {
        if (const auto status = Enter("FlushKey"))
            return *status;
        return m_inner.FlushKey(hKey);
    }

private:
    std::optional<LSTATUS> Enter(const char* const pszName)
    {
        if (onCall)
            onCall(pszName);

        const auto it = failures.find(pszName);
        if (it == failures.end())
            return std::nullopt;

        return it->second;
    }

    Registry& m_inner;
};

/// A writable overlay over the key `Lower` of an app hive, mounted below
/// `HKEY_CURRENT_USER`, with every lower call going through an
/// @ref InterceptRegistry that is also the base.
class Fixture
{
public:
    explicit Fixture(const std::wstring& mountPath = L"VRegOverlayTest")
        : m_lowerRegistry(m_native)
        , m_overlay(m_lowerRegistry, m_appHive.Root(), L"Lower")
        , m_registry(&m_lowerRegistry)
    {
        REQUIRE(
            m_native.CreateKeyExW(m_appHive.Root(), L"Lower", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &m_hLower, nullptr)
            == ERROR_SUCCESS
        );
        REQUIRE(m_registry.Mount(HKEY_CURRENT_USER, mountPath, m_overlay) == ERROR_SUCCESS);
    }

    ~Fixture()
    {
        m_native.CloseKey(m_hLower);
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    /// Direct access to the lower key, bypassing the overlay.
    [[nodiscard]] NativeRegistry& Native() noexcept
    {
        return m_native;
    }

    [[nodiscard]] InterceptRegistry& Lower() noexcept
    {
        return m_lowerRegistry;
    }

    [[nodiscard]] HKEY LowerRoot() const noexcept
    {
        return m_hLower;
    }

    [[nodiscard]] OverlayHive& Overlay() noexcept
    {
        return m_overlay;
    }

    [[nodiscard]] VirtualRegistry& Registry() noexcept
    {
        return m_registry;
    }

    [[nodiscard]] HKEY Root() noexcept
    {
        return Hive::HandleOf(m_overlay.Root());
    }

private:
    AppHive m_appHive;
    NativeRegistry m_native;
    InterceptRegistry m_lowerRegistry;
    HKEY m_hLower{};
    OverlayHive m_overlay;
    VirtualRegistry m_registry;
};

HKEY Create(Registry& registry, const HKEY hParent, const wchar_t* const pszPath, const wchar_t* const pszClass = nullptr)
{
    std::wstring className = pszClass ? pszClass : L"";
    HKEY hKey{};
    REQUIRE(
        registry.CreateKeyExW(
            hParent,
            pszPath,
            0,
            pszClass ? className.data() : nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        )
        == ERROR_SUCCESS
    );
    return hKey;
}

LSTATUS Open(Registry& registry, const HKEY hParent, const wchar_t* const pszPath, HKEY& hKey)
{
    hKey = nullptr;
    return registry.OpenKeyExW(hParent, pszPath, 0, KEY_ALL_ACCESS, &hKey);
}

HKEY MustOpen(Registry& registry, const HKEY hParent, const wchar_t* const pszPath)
{
    HKEY hKey{};
    REQUIRE(Open(registry, hParent, pszPath, hKey) == ERROR_SUCCESS);
    return hKey;
}

LSTATUS SetDword(Registry& registry, const HKEY hKey, const wchar_t* const pszName, const DWORD dwValue)
{
    return registry.SetValueExW(hKey, pszName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(dwValue));
}

LSTATUS SetString(Registry& registry, const HKEY hKey, const wchar_t* const pszName, const std::wstring& text)
{
    return registry.SetValueExW(
        hKey,
        pszName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(text.c_str()),
        static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t))
    );
}

LSTATUS SetSized(Registry& registry, const HKEY hKey, const wchar_t* const pszName, const DWORD cbData)
{
    const std::vector<BYTE> data(cbData, 0x5A);
    return registry.SetValueExW(hKey, pszName, 0, REG_BINARY, data.data(), cbData);
}

LSTATUS QueryDword(Registry& registry, const HKEY hKey, const wchar_t* const pszName, DWORD& dwValue)
{
    dwValue = 0;
    DWORD cbValue = sizeof(dwValue);
    return registry.QueryValueExW(hKey, pszName, nullptr, nullptr, reinterpret_cast<BYTE*>(&dwValue), &cbValue);
}

/// The value as a number, or 0xFFFFFFFF when the query fails.
DWORD DwordOr(Registry& registry, const HKEY hKey, const wchar_t* const pszName)
{
    DWORD dwValue{};
    return QueryDword(registry, hKey, pszName, dwValue) == ERROR_SUCCESS ? dwValue : 0xFFFFFFFF;
}

/// Everything an enumeration returns, and the status that ended it.
struct Listing
{
    std::vector<std::wstring> names;
    std::vector<std::wstring> classes;
    LSTATUS end = ERROR_SUCCESS;
};

Listing ListKeys(Registry& registry, const HKEY hKey)
{
    Listing listing;
    for (DWORD i = 0;; ++i)
    {
        wchar_t name[300]{};
        wchar_t className[300]{};
        DWORD cchName = 300;
        DWORD cchClass = 300;
        const auto status = registry.EnumKeyExW(hKey, i, name, &cchName, nullptr, className, &cchClass, nullptr);
        if (status != ERROR_SUCCESS)
        {
            listing.end = status;
            return listing;
        }

        listing.names.emplace_back(name, cchName);
        listing.classes.emplace_back(className, cchClass);
        REQUIRE(i < 1000);
    }
}

Listing ListValues(Registry& registry, const HKEY hKey)
{
    Listing listing;
    for (DWORD i = 0;; ++i)
    {
        wchar_t name[300]{};
        DWORD cchName = 300;
        const auto status = registry.EnumValueW(hKey, i, name, &cchName, nullptr, nullptr, nullptr, nullptr);
        if (status != ERROR_SUCCESS)
        {
            listing.end = status;
            return listing;
        }

        listing.names.emplace_back(name, cchName);
        REQUIRE(i < 1000);
    }
}

using Names = std::vector<std::wstring>;

struct Info
{
    LSTATUS status{};
    DWORD cSubKeys{};
    DWORD cchMaxSubKey{};
    DWORD cchMaxClass{};
    DWORD cValues{};
    DWORD cchMaxValueName{};
    DWORD cbMaxValue{};
    std::wstring className;
};

Info QueryInfo(Registry& registry, const HKEY hKey)
{
    Info info;
    wchar_t className[300]{};
    DWORD cchClass = 300;
    info.status = registry.QueryInfoKeyW(
        hKey,
        className,
        &cchClass,
        nullptr,
        &info.cSubKeys,
        &info.cchMaxSubKey,
        &info.cchMaxClass,
        &info.cValues,
        &info.cchMaxValueName,
        &info.cbMaxValue,
        nullptr,
        nullptr
    );
    if (info.status == ERROR_SUCCESS)
        info.className.assign(className, cchClass);

    return info;
}

/// Every key and value below @p hKey as text lines, in enumeration order.
void Snapshot(Registry& registry, const HKEY hKey, const std::wstring& path, std::vector<std::wstring>& lines)
{
    for (DWORD i = 0;; ++i)
    {
        std::wstring name;
        DWORD dwType{};
        std::vector<std::byte> data;
        if (registry.EnumValue(hKey, i, name, &dwType, &data) != ERROR_SUCCESS)
            break;

        auto line = std::format(L"{}|{}|{}|", path, name, dwType);
        for (const auto byte : data)
            line += std::format(L"{:02x}", static_cast<unsigned>(byte));
        lines.push_back(std::move(line));
    }

    for (DWORD i = 0;; ++i)
    {
        std::wstring name;
        if (registry.EnumKey(hKey, i, name) != ERROR_SUCCESS)
            break;

        lines.push_back(std::format(L"{}\\{}", path, name));
        HKEY hChild{};
        REQUIRE(registry.OpenKeyExW(hKey, name.c_str(), 0, KEY_READ, &hChild) == ERROR_SUCCESS);
        Snapshot(registry, hChild, std::format(L"{}\\{}", path, name), lines);
        registry.CloseKey(hChild);
    }
}

std::vector<std::wstring> Snapshot(Registry& registry, const HKEY hKey)
{
    std::vector<std::wstring> lines;
    Snapshot(registry, hKey, L"", lines);
    return lines;
}

constexpr wchar_t kMuiString[] = L"@%SystemRoot%\\system32\\shell32.dll,-21787";
} // namespace

TEST_CASE("Upper values and keys shadow lower ones")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    REQUIRE(SetDword(native, fx.LowerRoot(), L"v1", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(native, fx.LowerRoot(), L"v2", 2) == ERROR_SUCCESS);
    const HKEY hLowerK = Create(native, fx.LowerRoot(), L"K");
    REQUIRE(SetDword(native, hLowerK, L"a", 1) == ERROR_SUCCESS);
    const auto before = Snapshot(native, fx.LowerRoot());

    CHECK(DwordOr(reg, fx.Root(), L"v1") == 1);
    REQUIRE(SetDword(reg, fx.Root(), L"v1", 10) == ERROR_SUCCESS);
    CHECK(DwordOr(reg, fx.Root(), L"v1") == 10);
    CHECK(DwordOr(reg, fx.Root(), L"v2") == 2);

    DWORD dwDisposition = 0;
    HKEY hK{};
    REQUIRE(
        reg.CreateKeyExW(fx.Root(), L"K", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hK, &dwDisposition) == ERROR_SUCCESS
    );
    CHECK(dwDisposition == REG_OPENED_EXISTING_KEY);
    CHECK(IsVirtualHKey(hK));
    REQUIRE(SetDword(reg, hK, L"b", 5) == ERROR_SUCCESS);
    CHECK(DwordOr(reg, hK, L"a") == 1);
    CHECK(DwordOr(reg, hK, L"b") == 5);
    CHECK(ListKeys(reg, fx.Root()).names == Names{L"K"});
    CHECK(ListValues(reg, hK).names.size() == 2);
    CHECK(MustOpen(reg, fx.Root(), L"k") == hK);

    CHECK(Snapshot(native, fx.LowerRoot()) == before);
    native.CloseKey(hLowerK);
}

TEST_CASE("Value tombstones hide lower values")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    REQUIRE(SetDword(native, fx.LowerRoot(), L"v1", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(native, fx.LowerRoot(), L"v2", 2) == ERROR_SUCCESS);
    const auto before = Snapshot(native, fx.LowerRoot());

    REQUIRE(reg.DeleteValueW(fx.Root(), L"v1") == ERROR_SUCCESS);
    DWORD dwValue{};
    CHECK(QueryDword(reg, fx.Root(), L"V1", dwValue) == ERROR_FILE_NOT_FOUND);
    CHECK(ListValues(reg, fx.Root()).names == Names{L"v2"});
    CHECK(QueryInfo(reg, fx.Root()).cValues == 1);
    CHECK(reg.DeleteValueW(fx.Root(), L"v1") == ERROR_FILE_NOT_FOUND);
    CHECK(reg.DeleteValueW(fx.Root(), L"missing") == ERROR_FILE_NOT_FOUND);
    CHECK(Snapshot(native, fx.LowerRoot()) == before);

    REQUIRE(SetDword(reg, fx.Root(), L"v1", 7) == ERROR_SUCCESS);
    CHECK(DwordOr(reg, fx.Root(), L"v1") == 7);
    CHECK(ListValues(reg, fx.Root()).names == Names{L"v1", L"v2"});

    SECTION("An upper-only value is simply removed")
    {
        REQUIRE(SetDword(reg, fx.Root(), L"u", 1) == ERROR_SUCCESS);
        REQUIRE(reg.DeleteValueW(fx.Root(), L"u") == ERROR_SUCCESS);
        CHECK(QueryInfo(reg, fx.Root()).cValues == 2);
    }
}

TEST_CASE("Key tombstones hide lower keys")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    native.CloseKey(Create(native, fx.LowerRoot(), L"Empty"));
    native.CloseKey(Create(native, fx.LowerRoot(), L"Full\\Child"));
    const auto before = Snapshot(native, fx.LowerRoot());

    REQUIRE(reg.DeleteKeyExW(fx.Root(), L"empty", 0, 0) == ERROR_SUCCESS);
    HKEY hKey{};
    CHECK(Open(reg, fx.Root(), L"Empty", hKey) == ERROR_FILE_NOT_FOUND);
    CHECK(ListKeys(reg, fx.Root()).names == Names{L"Full"});
    CHECK(QueryInfo(reg, fx.Root()).cSubKeys == 1);
    CHECK(reg.DeleteKeyExW(fx.Root(), L"Empty", 0, 0) == ERROR_FILE_NOT_FOUND);
    CHECK(Snapshot(native, fx.LowerRoot()) == before);

    CHECK(reg.DeleteKeyExW(fx.Root(), L"Full", 0, 0) == ERROR_ACCESS_DENIED);
    REQUIRE(reg.DeleteKeyExW(fx.Root(), L"Full\\Child", 0, 0) == ERROR_SUCCESS);
    CHECK(Open(reg, fx.Root(), L"Full\\Child", hKey) == ERROR_FILE_NOT_FOUND);
    REQUIRE(reg.DeleteKeyExW(fx.Root(), L"Full", 0, 0) == ERROR_SUCCESS);
    CHECK(ListKeys(reg, fx.Root()).names.empty());
    CHECK(Snapshot(native, fx.LowerRoot()) == before);

    SECTION("Creating the key again makes a new, empty one")
    {
        DWORD dwDisposition = 0;
        REQUIRE(reg.CreateKeyExW(fx.Root(), L"Full", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
                == ERROR_SUCCESS);
        CHECK(dwDisposition == REG_CREATED_NEW_KEY);
        CHECK(ListKeys(reg, hKey).names.empty());
        CHECK(Open(reg, fx.Root(), L"Full\\Child", hKey) == ERROR_FILE_NOT_FOUND);
    }
}

TEST_CASE("Opaque keys hide everything below them")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    const HKEY hLowerK = Create(native, fx.LowerRoot(), L"K");
    REQUIRE(SetDword(native, hLowerK, L"a", 1) == ERROR_SUCCESS);
    native.CloseKey(Create(native, hLowerK, L"S\\Deep"));
    const HKEY hLowerL = Create(native, fx.LowerRoot(), L"L");
    REQUIRE(SetDword(native, hLowerL, L"a", 1) == ERROR_SUCCESS);
    native.CloseKey(Create(native, hLowerL, L"S"));
    const auto before = Snapshot(native, fx.LowerRoot());

    SECTION("RegDeleteTree on a subkey, then creating it again")
    {
        REQUIRE(reg.DeleteTreeW(fx.Root(), L"K") == ERROR_SUCCESS);
        HKEY hKey{};
        CHECK(Open(reg, fx.Root(), L"K", hKey) == ERROR_FILE_NOT_FOUND);
        CHECK(ListKeys(reg, fx.Root()).names == Names{L"L"});
        CHECK(Snapshot(native, fx.LowerRoot()) == before);

        DWORD dwDisposition = 0;
        REQUIRE(reg.CreateKeyExW(fx.Root(), L"K", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
                == ERROR_SUCCESS);
        CHECK(dwDisposition == REG_CREATED_NEW_KEY);
        CHECK(ListKeys(reg, hKey).names.empty());
        CHECK(ListValues(reg, hKey).names.empty());
        const auto info = QueryInfo(reg, hKey);
        CHECK(info.cSubKeys == 0);
        CHECK(info.cValues == 0);
        CHECK(Open(reg, fx.Root(), L"K\\S", hKey) == ERROR_FILE_NOT_FOUND);

        const HKEY hNew = MustOpen(reg, fx.Root(), L"K");
        REQUIRE(SetDword(reg, hNew, L"only", 9) == ERROR_SUCCESS);
        CHECK(ListValues(reg, hNew).names == Names{L"only"});
        CHECK(Snapshot(native, fx.LowerRoot()) == before);
    }

    SECTION("RegDeleteTree on the key itself keeps it, emptied")
    {
        const HKEY hL = MustOpen(reg, fx.Root(), L"L");
        REQUIRE(reg.DeleteTreeW(hL, nullptr) == ERROR_SUCCESS);
        CHECK(ListKeys(reg, hL).names.empty());
        CHECK(ListValues(reg, hL).names.empty());
        CHECK(ListKeys(reg, hL).end == ERROR_NO_MORE_ITEMS);
        HKEY hKey{};
        CHECK(Open(reg, fx.Root(), L"L\\S", hKey) == ERROR_FILE_NOT_FOUND);
        CHECK(Open(reg, fx.Root(), L"L", hKey) == ERROR_SUCCESS);
        CHECK(ListKeys(reg, fx.Root()).names == Names{L"L", L"K"});
        CHECK(Snapshot(native, fx.LowerRoot()) == before);
    }

    SECTION("RegDeleteTree on the mount root hides the whole lower key")
    {
        REQUIRE(reg.DeleteTreeW(fx.Root(), nullptr) == ERROR_SUCCESS);
        CHECK(ListKeys(reg, fx.Root()).names.empty());
        CHECK(ListValues(reg, fx.Root()).names.empty());
        HKEY hKey{};
        CHECK(Open(reg, fx.Root(), L"K", hKey) == ERROR_FILE_NOT_FOUND);
        CHECK(Snapshot(native, fx.LowerRoot()) == before);
    }

    native.CloseKey(hLowerK);
    native.CloseKey(hLowerL);
}

TEST_CASE("Enumeration merges both layers")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    for (const auto* const pszName : {L"a", L"c", L"e"})
        native.CloseKey(Create(native, fx.LowerRoot(), pszName));
    for (const auto* const pszName : {L"l1", L"l2", L"l3"})
        REQUIRE(SetDword(native, fx.LowerRoot(), pszName, 1) == ERROR_SUCCESS);

    reg.CloseKey(Create(reg, fx.Root(), L"b"));
    reg.CloseKey(Create(reg, fx.Root(), L"d"));
    REQUIRE(SetDword(reg, MustOpen(reg, fx.Root(), L"c"), L"touch", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(reg, fx.Root(), L"u1", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(reg, fx.Root(), L"l2", 2) == ERROR_SUCCESS);

    SECTION("Names appear once, upper entries first, and the sequence ends with ERROR_NO_MORE_ITEMS")
    {
        const auto keys = ListKeys(reg, fx.Root());
        CHECK(keys.end == ERROR_NO_MORE_ITEMS);
        REQUIRE(keys.names.size() == 5);
        CHECK(Names(keys.names.begin(), keys.names.begin() + 3) == Names{L"b", L"c", L"d"});
        CHECK(Names(keys.names.begin() + 3, keys.names.end()) == Names{L"a", L"e"});

        const auto values = ListValues(reg, fx.Root());
        CHECK(values.end == ERROR_NO_MORE_ITEMS);
        CHECK(values.names == Names{L"u1", L"l2", L"l1", L"l3"});
    }

    SECTION("Index access is stable and bounded")
    {
        wchar_t name[16]{};
        DWORD cchName = 16;
        CHECK(reg.EnumKeyExW(fx.Root(), 3, name, &cchName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
        CHECK(std::wstring(name, cchName) == L"a");
        cchName = 16;
        CHECK(reg.EnumKeyExW(fx.Root(), 5, name, &cchName, nullptr, nullptr, nullptr, nullptr)
              == ERROR_NO_MORE_ITEMS);
        cchName = 16;
        CHECK(reg.EnumKeyExW(fx.Root(), 100, name, &cchName, nullptr, nullptr, nullptr, nullptr)
              == ERROR_NO_MORE_ITEMS);
    }

    SECTION("Tombstoned lower keys are stepped over")
    {
        REQUIRE(reg.DeleteKeyExW(fx.Root(), L"a", 0, 0) == ERROR_SUCCESS);
        const auto keys = ListKeys(reg, fx.Root());
        CHECK(keys.end == ERROR_NO_MORE_ITEMS);
        CHECK(keys.names == Names{L"b", L"c", L"d", L"e"});
    }
}

TEST_CASE("QueryInfoKey covers both layers")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    native.CloseKey(Create(native, fx.LowerRoot(), L"aa", L"ccc"));
    native.CloseKey(Create(native, fx.LowerRoot(), L"bbbb"));
    REQUIRE(SetSized(native, fx.LowerRoot(), L"v1", 4) == ERROR_SUCCESS);
    REQUIRE(SetSized(native, fx.LowerRoot(), L"value2", 10) == ERROR_SUCCESS);

    auto info = QueryInfo(reg, fx.Root());
    REQUIRE(info.status == ERROR_SUCCESS);
    CHECK(info.cSubKeys == 2);
    CHECK(info.cchMaxSubKey == 4);
    CHECK(info.cchMaxClass == 3);
    CHECK(info.cValues == 2);
    CHECK(info.cchMaxValueName == 6);
    CHECK(info.cbMaxValue == 10);

    reg.CloseKey(Create(reg, fx.Root(), L"ccccccc", L"upperclass"));
    REQUIRE(SetDword(reg, MustOpen(reg, fx.Root(), L"bbbb"), L"x", 1) == ERROR_SUCCESS);
    REQUIRE(SetSized(reg, fx.Root(), L"x", 20) == ERROR_SUCCESS);
    REQUIRE(SetSized(reg, fx.Root(), L"v1", 2) == ERROR_SUCCESS);

    info = QueryInfo(reg, fx.Root());
    REQUIRE(info.status == ERROR_SUCCESS);
    CHECK(info.cSubKeys == 3);
    CHECK(info.cchMaxSubKey == 7);
    CHECK(info.cchMaxClass == 10);
    CHECK(info.cValues == 3);
    CHECK(info.cchMaxValueName == 6);
    CHECK(info.cbMaxValue == 20);

    SECTION("A shadowed name is counted once, a tombstone not at all")
    {
        REQUIRE(reg.DeleteValueW(fx.Root(), L"value2") == ERROR_SUCCESS);
        REQUIRE(reg.DeleteKeyExW(fx.Root(), L"aa", 0, 0) == ERROR_SUCCESS);
        info = QueryInfo(reg, fx.Root());
        REQUIRE(info.status == ERROR_SUCCESS);
        CHECK(info.cSubKeys == 2);
        CHECK(info.cValues == 2);
        CHECK(info.cbMaxValue == 20);
    }
}

TEST_CASE("A failed lower query is never a smaller successful view")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    native.CloseKey(Create(native, fx.LowerRoot(), L"sub"));
    REQUIRE(SetDword(native, fx.LowerRoot(), L"v", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(native, fx.LowerRoot(), L"lw", 3) == ERROR_SUCCESS);
    reg.CloseKey(Create(reg, fx.Root(), L"upperkey"));
    REQUIRE(SetDword(reg, fx.Root(), L"v", 2) == ERROR_SUCCESS);
    REQUIRE(SetDword(reg, fx.Root(), L"uv", 2) == ERROR_SUCCESS);

    const auto [call, status] = GENERATE(
        std::pair<const char*, LSTATUS>{"OpenKeyExW", ERROR_ACCESS_DENIED},
        std::pair<const char*, LSTATUS>{"OpenKeyExW", ERROR_NOT_ENOUGH_MEMORY},
        std::pair<const char*, LSTATUS>{"QueryInfoKeyW", ERROR_ACCESS_DENIED},
        std::pair<const char*, LSTATUS>{"QueryInfoKeyW", ERROR_INVALID_FUNCTION},
        std::pair<const char*, LSTATUS>{"QueryValueExW", ERROR_INVALID_FUNCTION},
        std::pair<const char*, LSTATUS>{"EnumKeyExW", ERROR_INVALID_FUNCTION},
        std::pair<const char*, LSTATUS>{"EnumValueW", ERROR_INVALID_FUNCTION}
    );
    INFO(call << " " << status);
    fx.Lower().failures[call] = status;
    const std::string name = call;

    const auto info = QueryInfo(reg, fx.Root());
    CHECK((name == "OpenKeyExW" || name == "QueryInfoKeyW" || name == "QueryValueExW") == (info.status != ERROR_SUCCESS));
    if (info.status != ERROR_SUCCESS)
    {
        CHECK(info.status == status);
        // A failing child open counts as failing too.
    }

    // The upper entries come first and never need the lower layer.
    wchar_t buffer[32]{};
    DWORD cch = 32;
    CHECK(reg.EnumKeyExW(fx.Root(), 0, buffer, &cch, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
    cch = 32;
    CHECK(reg.EnumValueW(fx.Root(), 0, buffer, &cch, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);

    cch = 32;
    const auto keyStatus = reg.EnumKeyExW(fx.Root(), 1, buffer, &cch, nullptr, nullptr, nullptr, nullptr);
    cch = 32;
    const auto valueStatus = reg.EnumValueW(fx.Root(), 2, buffer, &cch, nullptr, nullptr, nullptr, nullptr);
    if (name == "OpenKeyExW" || name == "QueryInfoKeyW" || name == "EnumKeyExW")
        CHECK(keyStatus == status);
    else
        CHECK(keyStatus == ERROR_SUCCESS);
    if (name == "OpenKeyExW" || name == "EnumValueW")
        CHECK(valueStatus == status);
}

TEST_CASE("Classes come from the lower key for keys the upper layer does not define")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    native.CloseKey(Create(native, fx.LowerRoot(), L"K", L"LowerClass"));
    native.CloseKey(Create(native, fx.LowerRoot(), L"Untouched", L"Other"));
    reg.CloseKey(Create(reg, fx.Root(), L"U", L"UpperClass"));

    const auto classOf = [&](const std::wstring& key)
    {
        const auto listing = ListKeys(reg, fx.Root());
        for (std::size_t i = 0; i < listing.names.size(); ++i)
        {
            if (listing.names[i] == key)
                return listing.classes[i];
        }
        return std::wstring(L"<missing>");
    };

    CHECK(classOf(L"K") == L"LowerClass");
    CHECK(classOf(L"U") == L"UpperClass");
    CHECK(QueryInfo(reg, MustOpen(reg, fx.Root(), L"K")).className == L"LowerClass");
    CHECK(QueryInfo(reg, MustOpen(reg, fx.Root(), L"U")).className == L"UpperClass");

    SECTION("Creating an existing key keeps its class")
    {
        reg.CloseKey(Create(reg, fx.Root(), L"K", L"Ignored"));
        CHECK(classOf(L"K") == L"LowerClass");
    }

    SECTION("Copy-up keeps the class")
    {
        REQUIRE(SetDword(reg, MustOpen(reg, fx.Root(), L"K"), L"written", 1) == ERROR_SUCCESS);
        CHECK(classOf(L"K") == L"LowerClass");
        CHECK(QueryInfo(reg, MustOpen(reg, fx.Root(), L"K")).className == L"LowerClass");
    }

    SECTION("A key created again takes the new class")
    {
        REQUIRE(reg.DeleteKeyExW(fx.Root(), L"K", 0, 0) == ERROR_SUCCESS);
        reg.CloseKey(Create(reg, fx.Root(), L"K", L"Fresh"));
        CHECK(classOf(L"K") == L"Fresh");
        CHECK(QueryInfo(reg, MustOpen(reg, fx.Root(), L"K")).className == L"Fresh");
    }
}

TEST_CASE("An upper-only key ends its enumerations with ERROR_NO_MORE_ITEMS")
{
    Fixture fx;
    auto& reg = fx.Registry();
    const HKEY hUpper = Create(reg, fx.Root(), L"UpperOnly");

    wchar_t buffer[32]{};
    DWORD cch = 32;
    CHECK(reg.EnumKeyExW(hUpper, 0, buffer, &cch, nullptr, nullptr, nullptr, nullptr) == ERROR_NO_MORE_ITEMS);
    cch = 32;
    CHECK(reg.EnumValueW(hUpper, 0, buffer, &cch, nullptr, nullptr, nullptr, nullptr) == ERROR_NO_MORE_ITEMS);

    reg.CloseKey(Create(reg, hUpper, L"child"));
    REQUIRE(SetDword(reg, hUpper, L"v", 1) == ERROR_SUCCESS);
    CHECK(ListKeys(reg, hUpper).names == Names{L"child"});
    CHECK(ListKeys(reg, hUpper).end == ERROR_NO_MORE_ITEMS);
    CHECK(ListValues(reg, hUpper).names == Names{L"v"});
    CHECK(ListValues(reg, hUpper).end == ERROR_NO_MORE_ITEMS);
    CHECK(QueryInfo(reg, hUpper).cSubKeys == 1);
}

TEST_CASE("A key created as a whole path is not a direct child of the root")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    native.CloseKey(Create(native, fx.LowerRoot(), L"Other"));

    reg.CloseKey(Create(reg, fx.Root(), L"A\\B\\C"));
    CHECK(ListKeys(reg, fx.Root()).names == Names{L"A", L"Other"});
    CHECK(QueryInfo(reg, fx.Root()).cSubKeys == 2);
    CHECK(ListKeys(reg, MustOpen(reg, fx.Root(), L"A")).names == Names{L"B"});
    CHECK(ListKeys(reg, MustOpen(reg, fx.Root(), L"A\\B")).names == Names{L"C"});
    CHECK(ListKeys(reg, MustOpen(reg, fx.Root(), L"A\\B\\C")).names.empty());

    SECTION("Also below a lower key")
    {
        reg.CloseKey(Create(reg, fx.Root(), L"Other\\X\\Y"));
        CHECK(ListKeys(reg, fx.Root()).names == Names{L"A", L"Other"});
        CHECK(ListKeys(reg, MustOpen(reg, fx.Root(), L"Other")).names == Names{L"X"});
    }
}

TEST_CASE("A handle to an ancestor of the mount point opens down into the mount")
{
    const auto name = std::format(L"VRegOverlayAncestor-{}", GetCurrentProcessId());
    Fixture fx(L"Software\\" + name);
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    const HKEY hLowerKey = Create(native, fx.LowerRoot(), L"L");
    REQUIRE(SetDword(native, hLowerKey, L"v", 11) == ERROR_SUCCESS);
    native.CloseKey(hLowerKey);

    HKEY hSoftware{};
    REQUIRE(fx.Lower().OpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &hSoftware) == ERROR_SUCCESS);
    REQUIRE_FALSE(IsVirtualHKey(hSoftware));

    HKEY hKey{};
    REQUIRE(Open(reg, hSoftware, (name + L"\\L").c_str(), hKey) == ERROR_SUCCESS);
    CHECK(IsVirtualHKey(hKey));
    CHECK(DwordOr(reg, hKey, L"v") == 11);
    CHECK(Open(reg, hSoftware, name.c_str(), hKey) == ERROR_SUCCESS);
    CHECK(hKey == fx.Root());
    CHECK(Open(reg, hSoftware, (name + L"\\Missing").c_str(), hKey) == ERROR_FILE_NOT_FOUND);

    DWORD dwDisposition = 0;
    REQUIRE(reg.CreateKeyExW(hSoftware, (name + L"\\L\\New").c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
            == ERROR_SUCCESS);
    CHECK(IsVirtualHKey(hKey));
    CHECK(dwDisposition == REG_CREATED_NEW_KEY);

    HKEY hCurrentUser{};
    REQUIRE(RegOpenCurrentUser(KEY_READ, &hCurrentUser) == ERROR_SUCCESS);
    CHECK(Open(reg, hCurrentUser, (L"Software\\" + name + L"\\L").c_str(), hKey) == ERROR_SUCCESS);
    CHECK(IsVirtualHKey(hKey));
    RegCloseKey(hCurrentUser);

    fx.Lower().CloseKey(hSoftware);
}

TEST_CASE("RegLoadMUIStringW forwards a lower value to the lower key")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    REQUIRE(SetString(native, fx.LowerRoot(), L"Indirect", kMuiString) == ERROR_SUCCESS);

    std::wstring expected(256, L'\0');
    DWORD cbExpected = 0;
    const auto expectedStatus = native.LoadMUIStringW(
        fx.LowerRoot(),
        L"Indirect",
        expected.data(),
        static_cast<DWORD>(expected.size() * sizeof(wchar_t)),
        &cbExpected,
        0,
        nullptr
    );
    REQUIRE(expectedStatus == ERROR_SUCCESS);

    std::wstring actual(256, L'\0');
    DWORD cbActual = 0;
    CHECK(reg.LoadMUIStringW(
              fx.Root(),
              L"Indirect",
              actual.data(),
              static_cast<DWORD>(actual.size() * sizeof(wchar_t)),
              &cbActual,
              0,
              nullptr
          )
          == expectedStatus);
    CHECK(cbActual == cbExpected);
    CHECK(actual == expected);

    SECTION("Through a handle below the root")
    {
        const HKEY hSub = Create(native, fx.LowerRoot(), L"Sub");
        REQUIRE(SetString(native, hSub, L"Indirect", kMuiString) == ERROR_SUCCESS);
        const HKEY hVirtualSub = MustOpen(reg, fx.Root(), L"Sub");
        DWORD cb = 0;
        std::wstring text(256, L'\0');
        CHECK(reg.LoadMUIStringW(hVirtualSub, L"Indirect", text.data(), 512, &cb, 0, nullptr) == ERROR_SUCCESS);
        CHECK(text == expected);
        native.CloseKey(hSub);
    }

    SECTION("A value the upper layer holds is a literal")
    {
        REQUIRE(SetString(reg, fx.Root(), L"Indirect", kMuiString) == ERROR_SUCCESS);
        DWORD cb = 0;
        std::wstring text(256, L'\0');
        CHECK(reg.LoadMUIStringW(fx.Root(), L"Indirect", text.data(), 512, &cb, 0, nullptr) == ERROR_INVALID_DATA);

        REQUIRE(SetString(reg, fx.Root(), L"Plain", L"plain text") == ERROR_SUCCESS);
        CHECK(reg.LoadMUIStringW(fx.Root(), L"Plain", text.data(), 512, &cb, 0, nullptr) == ERROR_INVALID_DATA);
    }

    SECTION("A missing value")
    {
        DWORD cb = 0;
        std::wstring text(256, L'\0');
        CHECK(reg.LoadMUIStringW(fx.Root(), L"Missing", text.data(), 512, &cb, 0, nullptr) == ERROR_FILE_NOT_FOUND);
    }
}

TEST_CASE("A frozen overlay refuses writes and hands out lower keys")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    REQUIRE(SetDword(native, fx.LowerRoot(), L"lv", 1) == ERROR_SUCCESS);
    const HKEY hM = Create(native, fx.LowerRoot(), L"M");
    REQUIRE(SetDword(native, hM, L"m", 3) == ERROR_SUCCESS);
    native.CloseKey(hM);
    const HKEY hLowerK = Create(native, fx.LowerRoot(), L"K\\Sub");
    native.CloseKey(hLowerK);

    REQUIRE(SetDword(reg, fx.Root(), L"uv", 2) == ERROR_SUCCESS);
    reg.CloseKey(Create(reg, fx.Root(), L"U"));
    REQUIRE(SetDword(reg, MustOpen(reg, fx.Root(), L"K"), L"b", 4) == ERROR_SUCCESS);
    const auto before = Snapshot(native, fx.LowerRoot());

    fx.Overlay().Freeze();
    REQUIRE(fx.Overlay().IsFrozen());

    SECTION("Writes answer ERROR_ACCESS_DENIED")
    {
        HKEY hKey{};
        DWORD dwDisposition = 0;
        CHECK(SetDword(reg, fx.Root(), L"new", 1) == ERROR_ACCESS_DENIED);
        CHECK(reg.DeleteValueW(fx.Root(), L"uv") == ERROR_ACCESS_DENIED);
        CHECK(reg.DeleteValueW(fx.Root(), L"lv") == ERROR_ACCESS_DENIED);
        CHECK(reg.DeleteKeyExW(fx.Root(), L"U", 0, 0) == ERROR_ACCESS_DENIED);
        CHECK(reg.DeleteKeyExW(fx.Root(), L"M", 0, 0) == ERROR_ACCESS_DENIED);
        CHECK(reg.DeleteTreeW(fx.Root(), L"K") == ERROR_ACCESS_DENIED);
        CHECK(reg.DeleteTreeW(fx.Root(), L"M") == ERROR_ACCESS_DENIED);
        CHECK(reg.DeleteTreeW(fx.Root(), nullptr) == ERROR_ACCESS_DENIED);
        CHECK(reg.CreateKeyExW(fx.Root(), L"Nope", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
              == ERROR_ACCESS_DENIED);
        CHECK(reg.CreateKeyExW(fx.Root(), L"M\\Nope", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
              == ERROR_ACCESS_DENIED);
        CHECK(Snapshot(native, fx.LowerRoot()) == before);
    }

    SECTION("Opening a lower-only key returns a real handle that works with the base")
    {
        HKEY hKey{};
        REQUIRE(Open(reg, fx.Root(), L"M", hKey) == ERROR_SUCCESS);
        CHECK_FALSE(IsVirtualHKey(hKey));
        CHECK(DwordOr(reg, hKey, L"m") == 3);
        CHECK(reg.CloseKey(hKey) == ERROR_SUCCESS);

        REQUIRE(Open(reg, fx.Root(), L"K\\Sub", hKey) == ERROR_SUCCESS);
        CHECK_FALSE(IsVirtualHKey(hKey));
        CHECK(reg.CloseKey(hKey) == ERROR_SUCCESS);

        REQUIRE(Open(reg, fx.Root(), L"U", hKey) == ERROR_SUCCESS);
        CHECK(IsVirtualHKey(hKey));
        REQUIRE(Open(reg, fx.Root(), L"K", hKey) == ERROR_SUCCESS);
        CHECK(IsVirtualHKey(hKey));
        CHECK(DwordOr(reg, hKey, L"b") == 4);
        CHECK(Open(reg, fx.Root(), L"Missing", hKey) == ERROR_FILE_NOT_FOUND);
    }

    SECTION("Reads fall through")
    {
        CHECK(DwordOr(reg, fx.Root(), L"lv") == 1);
        CHECK(DwordOr(reg, fx.Root(), L"uv") == 2);
        CHECK(ListKeys(reg, fx.Root()).names.size() == 3);
        CHECK(QueryInfo(reg, fx.Root()).cSubKeys == 3);
    }

    SECTION("RegCreateKeyEx of an existing key opens it")
    {
        HKEY hKey{};
        DWORD dwDisposition = 0;
        REQUIRE(reg.CreateKeyExW(fx.Root(), L"M", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
                == ERROR_SUCCESS);
        CHECK(dwDisposition == REG_OPENED_EXISTING_KEY);
        CHECK_FALSE(IsVirtualHKey(hKey));
        CHECK(DwordOr(reg, hKey, L"m") == 3);
        reg.CloseKey(hKey);

        REQUIRE(reg.CreateKeyExW(fx.Root(), L"U", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
                == ERROR_SUCCESS);
        CHECK(dwDisposition == REG_OPENED_EXISTING_KEY);
        CHECK(IsVirtualHKey(hKey));
    }
}

TEST_CASE("A writable overlay hands out virtual handles and copies up")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    const HKEY hM = Create(native, fx.LowerRoot(), L"M\\Deep");
    REQUIRE(SetDword(native, hM, L"d", 6) == ERROR_SUCCESS);
    native.CloseKey(hM);
    REQUIRE(SetDword(native, fx.LowerRoot(), L"lv", 1) == ERROR_SUCCESS);
    const auto before = Snapshot(native, fx.LowerRoot());

    HKEY hKey{};
    REQUIRE(Open(reg, fx.Root(), L"M", hKey) == ERROR_SUCCESS);
    CHECK(IsVirtualHKey(hKey));
    const HKEY hM2 = hKey;
    REQUIRE(Open(reg, fx.Root(), L"M\\Deep", hKey) == ERROR_SUCCESS);
    CHECK(IsVirtualHKey(hKey));
    CHECK(DwordOr(reg, hKey, L"d") == 6);
    CHECK(Open(reg, fx.Root(), L"M\\Missing", hKey) == ERROR_FILE_NOT_FOUND);
    CHECK(Open(reg, fx.Root(), L"Missing\\Deep", hKey) == ERROR_FILE_NOT_FOUND);

    REQUIRE(Open(reg, hM2, L"Deep", hKey) == ERROR_SUCCESS);
    REQUIRE(SetDword(reg, hKey, L"d", 60) == ERROR_SUCCESS);
    REQUIRE(SetDword(reg, hKey, L"extra", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(reg, hM2, L"m", 5) == ERROR_SUCCESS);
    REQUIRE(reg.DeleteValueW(fx.Root(), L"lv") == ERROR_SUCCESS);

    CHECK(DwordOr(reg, hKey, L"d") == 60);
    CHECK(DwordOr(reg, hM2, L"m") == 5);
    CHECK(ListKeys(reg, hM2).names == Names{L"Deep"});
    CHECK(QueryInfo(reg, hKey).cValues == 2);
    CHECK(Snapshot(native, fx.LowerRoot()) == before);
}

namespace
{
/// Whether the hive's lock is free, found by writing to an unrelated key
/// from another thread and waiting for it with a timeout.
bool IsLockFree(OverlayHive& overlay, Node* const pProbe)
{
    std::promise<void> done;
    auto finished = done.get_future();
    std::thread writer(
        [&]
        {
            const DWORD dwValue = 1;
            overlay.SetValue(pProbe, L"probe", REG_DWORD, std::as_bytes(std::span(&dwValue, 1)));
            done.set_value();
        }
    );
    const bool isFree = finished.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    writer.join();
    return isFree;
}
} // namespace

TEST_CASE("The hive lock is never held across a call into the lower registry")
{
    Fixture fx;
    auto& native = fx.Native();
    auto& reg = fx.Registry();
    const HKEY hLowerL = Create(native, fx.LowerRoot(), L"L");
    REQUIRE(SetDword(native, hLowerL, L"v", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(native, hLowerL, L"w", 1) == ERROR_SUCCESS);
    native.CloseKey(Create(native, hLowerL, L"s1"));
    native.CloseKey(Create(native, hLowerL, L"Empty"));
    native.CloseKey(hLowerL);

    const HKEY hProbe = Create(reg, fx.Root(), L"Probe");
    REQUIRE(SetDword(reg, hProbe, L"seed", 0) == ERROR_SUCCESS);
    auto* const pProbe = Hive::NodeOf(hProbe);
    REQUIRE(pProbe != nullptr);

    int cCalls = 0;
    int cBusy = 0;
    fx.Lower().onCall = [&](const std::string&)
    {
        ++cCalls;
        if (!IsLockFree(fx.Overlay(), pProbe))
            ++cBusy;
    };

    const HKEY hL = MustOpen(reg, fx.Root(), L"L");
    const auto check = [&](const char* const pszLabel, const std::function<void()>& operation)
    {
        INFO(pszLabel);
        cCalls = 0;
        cBusy = 0;
        operation();
        CHECK(cCalls > 0);
        CHECK(cBusy == 0);
    };

    check("open",
          [&]
          {
              HKEY hKey{};
              CHECK(Open(reg, fx.Root(), L"L\\s1", hKey) == ERROR_SUCCESS);
          });
    check("create",
          [&]
          {
              HKEY hKey{};
              CHECK(reg.CreateKeyExW(fx.Root(), L"L\\New\\Deeper", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr)
                    == ERROR_SUCCESS);
          });
    check("query value",
          [&]
          {
              DWORD dwValue{};
              CHECK(QueryDword(reg, hL, L"v", dwValue) == ERROR_SUCCESS);
          });
    check("enumerate keys",
          [&]
          {
              CHECK(ListKeys(reg, hL).end == ERROR_NO_MORE_ITEMS);
          });
    check("enumerate values",
          [&]
          {
              CHECK(ListValues(reg, hL).end == ERROR_NO_MORE_ITEMS);
          });
    check("query info",
          [&]
          {
              CHECK(QueryInfo(reg, hL).status == ERROR_SUCCESS);
          });
    check("delete key",
          [&]
          {
              CHECK(reg.DeleteKeyExW(fx.Root(), L"L\\Empty", 0, 0) == ERROR_SUCCESS);
          });
    check("delete value",
          [&]
          {
              CHECK(reg.DeleteValueW(hL, L"w") == ERROR_SUCCESS);
          });
    check("load MUI string",
          [&]
          {
              DWORD cb = 0;
              wchar_t text[8]{};
              reg.LoadMUIStringW(hL, L"v", text, sizeof(text), &cb, 0, nullptr);
          });
    check("delete tree",
          [&]
          {
              CHECK(reg.DeleteTreeW(fx.Root(), L"L\\s1") == ERROR_SUCCESS);
          });

    fx.Lower().onCall = nullptr;
}
