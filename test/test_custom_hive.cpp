// Custom hives: providers that supply a hive as a table of slots.

#include <vreg.h>

#include "forwarding_hive.h"
#include "objects.h"
#include "owned_values.h"
#include "thread.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace VRegTest;

namespace
{
/// A key of @ref ComputedProvider: its subkeys, and the names of its values.
struct ComputedKey
{
    const wchar_t* pszName;
    std::vector<const ComputedKey*> children;
    std::vector<const wchar_t*> values;
};

const ComputedKey DEEP{L"Deep", {}, {}};
const ComputedKey ALPHA{L"Alpha", {&DEEP}, {L"Name"}};
const ComputedKey BETA{L"Beta", {}, {}};
const ComputedKey ROOT{L"", {&ALPHA, &BETA}, {L"Counter", L"Text"}};

/// A read-only provider over a fixed tree whose values are computed on each
/// read: the root's `Counter` counts the reads of it. Keys are found through
/// the tokens their opens answered.
class ComputedProvider
{
public:
    ComputedProvider()
    {
        REQUIRE(VRegCreateCustomHive(&VTBL, this, m_hive.Receive()) == ERROR_SUCCESS);
    }

    ComputedProvider(const ComputedProvider&) = delete;
    ComputedProvider& operator=(const ComputedProvider&) = delete;

    [[nodiscard]] VRegHive* Hive() const noexcept
    {
        return m_hive;
    }

    /// How often a slot met a key other than the root without a token.
    [[nodiscard]] int MissingTokens() const noexcept
    {
        return m_cMissingTokens;
    }

private:
    static ComputedProvider& Of(VRegHive* const pHive) noexcept
    {
        return *static_cast<ComputedProvider*>(VRegGetHiveDelegate(pHive));
    }

    const ComputedKey* KeyOf(VRegHiveKey* const pKey) noexcept
    {
        if (!VRegGetHiveKeyParent(pKey))
            return &ROOT;
        const auto* const pComputed = static_cast<const ComputedKey*>(VRegGetHiveKeyToken(pKey));
        if (!pComputed)
            ++m_cMissingTokens;
        return pComputed;
    }

    static LSTATUS WINAPI OpenKey(
        VRegHive* const pHive,
        VRegHiveKey* const pParent,
        const LPCWSTR pszSubKey,
        void** const ppToken
    )
    {
        const ComputedKey* pCurrent = Of(pHive).KeyOf(pParent);
        *ppToken = nullptr;
        if (!pCurrent)
            return ERROR_FILE_NOT_FOUND;

        std::wstring_view rest = pszSubKey;
        while (!rest.empty())
        {
            const auto cch = rest.find(L'\\');
            const std::wstring name(rest.substr(0, cch));
            rest = cch == std::wstring_view::npos ? std::wstring_view{} : rest.substr(cch + 1);
            const ComputedKey* pNext = nullptr;
            for (const ComputedKey* const pChild : pCurrent->children)
            {
                if (VRegCompareNames(pChild->pszName, name.c_str()) == 0)
                    pNext = pChild;
            }
            if (!pNext)
                return ERROR_FILE_NOT_FOUND;
            pCurrent = pNext;
        }

        *ppToken = const_cast<ComputedKey*>(pCurrent);
        return ERROR_SUCCESS;
    }

    static LSTATUS SetComputedValue(ComputedProvider& self, const wchar_t* const pszName, VRegHiveResult* const pResult)
    {
        if (VRegCompareNames(pszName, L"Counter") == 0)
        {
            const DWORD dwCount = ++self.m_cCounterReads;
            return VRegHiveResultSetValue(pResult, REG_DWORD, reinterpret_cast<const BYTE*>(&dwCount), sizeof(dwCount));
        }

        const wchar_t* const pszText = VRegCompareNames(pszName, L"Text") == 0 ? L"computed" : L"alpha";
        return VRegHiveResultSetValue(
            pResult,
            REG_SZ,
            reinterpret_cast<const BYTE*>(pszText),
            static_cast<DWORD>((std::wcslen(pszText) + 1) * sizeof(wchar_t))
        );
    }

    static LSTATUS WINAPI QueryValue(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const LPCWSTR pszName,
        VRegHiveResult* const pResult
    )
    {
        ComputedProvider& self = Of(pHive);
        const ComputedKey* const pComputed = self.KeyOf(pKey);
        if (!pComputed)
            return ERROR_FILE_NOT_FOUND;

        for (const wchar_t* const pszValue : pComputed->values)
        {
            if (VRegCompareNames(pszValue, pszName) == 0)
                return SetComputedValue(self, pszValue, pResult);
        }
        return ERROR_FILE_NOT_FOUND;
    }

    static LSTATUS WINAPI EnumKey(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const DWORD dwIndex,
        VRegHiveResult* const pResult
    )
    {
        const ComputedKey* const pComputed = Of(pHive).KeyOf(pKey);
        if (!pComputed)
            return ERROR_FILE_NOT_FOUND;
        if (dwIndex >= pComputed->children.size())
            return ERROR_NO_MORE_ITEMS;
        return VRegHiveResultSetName(pResult, pComputed->children[dwIndex]->pszName);
    }

    static LSTATUS WINAPI EnumValue(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const DWORD dwIndex,
        VRegHiveResult* const pResult
    )
    {
        ComputedProvider& self = Of(pHive);
        const ComputedKey* const pComputed = self.KeyOf(pKey);
        if (!pComputed)
            return ERROR_FILE_NOT_FOUND;
        if (dwIndex >= pComputed->values.size())
            return ERROR_NO_MORE_ITEMS;

        const LSTATUS status = VRegHiveResultSetName(pResult, pComputed->values[dwIndex]);
        return status == ERROR_SUCCESS ? SetComputedValue(self, pComputed->values[dwIndex], pResult) : status;
    }

    static constexpr VRegHiveVtbl VTBL{
        .OpenKey = &OpenKey,
        .QueryValue = &QueryValue,
        .EnumKey = &EnumKey,
        .EnumValue = &EnumValue,
    };

    DWORD m_cCounterReads = 0;
    int m_cMissingTokens = 0;
    OwnedHive m_hive;
};

/// A provider with one value, `Grows`, whose data is longer on each of its
/// first reads and then stays as it is.
class GrowingProvider
{
public:
    GrowingProvider()
    {
        REQUIRE(VRegCreateCustomHive(&VTBL, this, m_hive.Receive()) == ERROR_SUCCESS);
    }

    GrowingProvider(const GrowingProvider&) = delete;
    GrowingProvider& operator=(const GrowingProvider&) = delete;

    [[nodiscard]] VRegHive* Hive() const noexcept
    {
        return m_hive;
    }

    /// The data once it has stopped growing.
    static std::vector<std::byte> Final()
    {
        return std::vector<std::byte>(MAX_SIZE, std::byte{0x5A});
    }

private:
    static constexpr DWORD MAX_SIZE = 48;

    static LSTATUS WINAPI OpenKey(VRegHive*, VRegHiveKey*, const LPCWSTR pszSubKey, void** const ppToken)
    {
        *ppToken = nullptr;
        return *pszSubKey ? ERROR_FILE_NOT_FOUND : ERROR_SUCCESS;
    }

    static LSTATUS WINAPI QueryValue(
        VRegHive* const pHive,
        VRegHiveKey*,
        const LPCWSTR pszName,
        VRegHiveResult* const pResult
    )
    {
        auto& self = *static_cast<GrowingProvider*>(VRegGetHiveDelegate(pHive));
        if (VRegCompareNames(pszName, L"Grows") != 0)
            return ERROR_FILE_NOT_FOUND;

        self.m_cbData = self.m_cbData + 16 > MAX_SIZE ? MAX_SIZE : self.m_cbData + 16;
        const std::vector<std::byte> data(self.m_cbData, std::byte{0x5A});
        return VRegHiveResultSetValue(pResult, REG_BINARY, reinterpret_cast<const BYTE*>(data.data()), self.m_cbData);
    }

    static LSTATUS WINAPI EnumNothing(VRegHive*, VRegHiveKey*, DWORD, VRegHiveResult*)
    {
        return ERROR_NO_MORE_ITEMS;
    }

    static constexpr VRegHiveVtbl VTBL{
        .OpenKey = &OpenKey,
        .QueryValue = &QueryValue,
        .EnumKey = &EnumNothing,
        .EnumValue = &EnumNothing,
    };

    DWORD m_cbData = 0;
    OwnedHive m_hive;
};

/// @p pHive mounted at `HKEY_CURRENT_USER\Custom` of a fresh virtual registry.
OwnedRegistry MountAlone(VRegHive* const pHive)
{
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(VRegMount(registry, HKEY_CURRENT_USER, L"Custom", pHive, 0) == ERROR_SUCCESS);
    return registry;
}

DWORD QueryDword(VReg* const registry, const HKEY hKey, const wchar_t* const pszName)
{
    DWORD dwValue = 0;
    DWORD cbValue = sizeof(dwValue);
    REQUIRE(
        VRegQueryValueExW(
            registry,
            hKey,
            pszName,
            nullptr,
            nullptr,
            reinterpret_cast<BYTE*>(&dwValue),
            &cbValue
        ) == ERROR_SUCCESS
    );
    return dwValue;
}
} // namespace

TEST_CASE("A read-only computed provider computes each read and refuses every write")
{
    ComputedProvider provider;
    OwnedRegistry registry = MountAlone(provider.Hive());
    const HKEY hRoot = RootHandle(provider.Hive());

    SECTION("Values are computed on each read")
    {
        const DWORD dwFirst = QueryDword(registry, hRoot, L"Counter");
        CHECK(QueryDword(registry, hRoot, L"Counter") == dwFirst + 1);
    }

    SECTION("Keys are found by name, case-insensitively, through their tokens")
    {
        HKEY hDeep{};
        CHECK(
            VRegOpenKeyExW(
                registry,
                hRoot,
                L"alpha\\DEEP",
                0,
                KEY_READ,
                &hDeep
            ) == ERROR_SUCCESS
        );
        HKEY hMissing{};
        CHECK(
            VRegOpenKeyExW(
                registry,
                hRoot,
                L"Alpha\\Missing",
                0,
                KEY_READ,
                &hMissing
            ) == ERROR_FILE_NOT_FOUND
        );
        std::wstring name;
        CHECK(EnumKey(registry, hRoot, 0, name) == ERROR_SUCCESS);
        CHECK(name == L"Alpha");
        CHECK(provider.MissingTokens() == 0);
    }

    SECTION("RegQueryInfoKey is derived from the enumerations")
    {
        DWORD cSubKeys = 0;
        DWORD cchMaxSubKey = 0;
        DWORD cchMaxClass = 0;
        DWORD cValues = 0;
        DWORD cchMaxValueName = 0;
        DWORD cbMaxValue = 0;
        REQUIRE(
            VRegQueryInfoKeyW(
                registry,
                hRoot,
                nullptr,
                nullptr,
                nullptr,
                &cSubKeys,
                &cchMaxSubKey,
                &cchMaxClass,
                &cValues,
                &cchMaxValueName,
                &cbMaxValue,
                nullptr,
                nullptr
            ) == ERROR_SUCCESS
        );
        CHECK(cSubKeys == 2);
        CHECK(cchMaxSubKey == 5);
        CHECK(cchMaxClass == 0);
        CHECK(cValues == 2);
        CHECK(cchMaxValueName == 7);
        CHECK(cbMaxValue == sizeof(L"computed"));
    }

    SECTION("Every write form answers access denied")
    {
        const DWORD dwOne = 1;
        const auto* const pOne = reinterpret_cast<const BYTE*>(&dwOne);
        HKEY hNew{};
        CHECK(
            VRegCreateKeyExW(
                registry,
                hRoot,
                L"New",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hNew,
                nullptr
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegCreateKeyExA(
                registry,
                hRoot,
                "New",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hNew,
                nullptr
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegCreateKeyW(
                registry,
                hRoot,
                L"New",
                &hNew
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegSetValueExW(
                registry,
                hRoot,
                L"Text",
                0,
                REG_DWORD,
                pOne,
                sizeof(dwOne)
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegSetValueExA(
                registry,
                hRoot,
                "Text",
                0,
                REG_DWORD,
                pOne,
                sizeof(dwOne)
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegSetValueW(
                registry,
                hRoot,
                L"Alpha",
                REG_SZ,
                L"x",
                0
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegSetKeyValueW(
                registry,
                hRoot,
                L"Alpha",
                L"Name",
                REG_DWORD,
                pOne,
                sizeof(dwOne)
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteValueW(
                registry,
                hRoot,
                L"Text"
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteValueA(
                registry,
                hRoot,
                "Text"
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteKeyValueW(
                registry,
                hRoot,
                L"Alpha",
                L"Name"
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteKeyW(
                registry,
                hRoot,
                L"Beta"
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteKeyExW(
                registry,
                hRoot,
                L"Beta",
                0,
                0
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteKeyA(
                registry,
                hRoot,
                "Beta"
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteTreeW(
                registry,
                hRoot,
                L"Alpha"
            ) == ERROR_ACCESS_DENIED
        );
        CHECK(
            VRegDeleteTreeW(
                registry,
                hRoot,
                nullptr
            ) == ERROR_ACCESS_DENIED
        );

        // An existing key opens, as on a frozen hive.
        HKEY hAlpha{};
        DWORD dwDisposition = 0;
        CHECK(
            VRegCreateKeyExW(
                registry,
                hRoot,
                L"Alpha",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hAlpha,
                &dwDisposition
            ) == ERROR_SUCCESS
        );
        CHECK(dwDisposition == REG_OPENED_EXISTING_KEY);
    }
}

TEST_CASE("A value that grows between the sizing call and the read is read whole")
{
    GrowingProvider provider;
    OwnedRegistry registry = MountAlone(provider.Hive());

    DWORD dwType = REG_NONE;
    std::vector<std::byte> data;
    REQUIRE(
        GetValue(registry, RootHandle(provider.Hive()), nullptr, L"Grows", RRF_RT_ANY, &dwType, data)
        == ERROR_SUCCESS
    );
    CHECK(dwType == REG_BINARY);
    CHECK(data == GrowingProvider::Final());
}

namespace
{
LSTATUS WINAPI OpenNothing(VRegHive*, VRegHiveKey*, LPCWSTR, void**)
{
    return ERROR_FILE_NOT_FOUND;
}

LSTATUS WINAPI QueryNothing(VRegHive*, VRegHiveKey*, LPCWSTR, VRegHiveResult*)
{
    return ERROR_FILE_NOT_FOUND;
}

LSTATUS WINAPI EnumNothing(VRegHive*, VRegHiveKey*, DWORD, VRegHiveResult*)
{
    return ERROR_NO_MORE_ITEMS;
}
} // namespace

TEST_CASE("A custom hive needs OpenKey, QueryValue, EnumKey and EnumValue")
{
    const VRegHiveVtbl complete{
        .OpenKey = &OpenNothing,
        .QueryValue = &QueryNothing,
        .EnumKey = &EnumNothing,
        .EnumValue = &EnumNothing,
    };

    OwnedHive hive;
    CHECK(VRegCreateCustomHive(&complete, nullptr, hive.Receive()) == ERROR_SUCCESS);
    CHECK(hive.Get() != nullptr);
    CHECK(VRegGetHiveDelegate(hive) == nullptr);

    VRegHiveVtbl missing = complete;
    SECTION("OpenKey")
    {
        missing.OpenKey = nullptr;
    }

    SECTION("QueryValue")
    {
        missing.QueryValue = nullptr;
    }

    SECTION("EnumKey")
    {
        missing.EnumKey = nullptr;
    }

    SECTION("EnumValue")
    {
        missing.EnumValue = nullptr;
    }

    VRegHive* pHive = reinterpret_cast<VRegHive*>(1);
    CHECK(VRegCreateCustomHive(&missing, nullptr, &pHive) == ERROR_INVALID_PARAMETER);
    CHECK(pHive == nullptr);
}

TEST_CASE("A custom hive keeps one handle per key and marks deleted keys")
{
    CustomMount mount;
    const HKEY hRoot = mount.Root();
    HKEY hFirst{};
    REQUIRE(
        VRegCreateKeyExW(
            mount.Registry(),
            hRoot,
            L"Key\\Sub",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hFirst,
            nullptr
        ) == ERROR_SUCCESS
    );

    HKEY hSecond{};
    REQUIRE(
        VRegOpenKeyExW(
            mount.Registry(),
            hRoot,
            L"KEY\\sub",
            0,
            KEY_READ,
            &hSecond
        ) == ERROR_SUCCESS
    );
    CHECK(hSecond == hFirst);

    REQUIRE(
        VRegDeleteKeyW(
            mount.Registry(),
            hRoot,
            L"Key\\Sub"
        ) == ERROR_SUCCESS
    );
    CHECK(
        VRegQueryValueExW(
            mount.Registry(),
            hFirst,
            L"v",
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ) == ERROR_KEY_DELETED
    );

    HKEY hRecreated{};
    REQUIRE(
        VRegCreateKeyExW(
            mount.Registry(),
            hRoot,
            L"Key\\Sub",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hRecreated,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(hRecreated != hFirst);
}

TEST_CASE("A frozen custom hive refuses writes and still opens its keys")
{
    CustomMount mount;
    const HKEY hRoot = mount.Root();
    const DWORD dwOne = 1;
    REQUIRE(
        VRegSetKeyValueW(
            mount.Registry(),
            hRoot,
            L"Key",
            L"v",
            REG_DWORD,
            &dwOne,
            sizeof(dwOne)
        ) == ERROR_SUCCESS
    );

    VRegFreezeHive(mount.Hive());
    HKEY hKey{};
    CHECK(
        VRegOpenKeyExW(
            mount.Registry(),
            hRoot,
            L"Key",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_SUCCESS
    );
    CHECK(QueryDword(mount.Registry(), hKey, L"v") == 1);
    CHECK(
        VRegSetValueExW(
            mount.Registry(),
            hKey,
            L"v",
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&dwOne),
            sizeof(dwOne)
        ) == ERROR_ACCESS_DENIED
    );
    CHECK(
        VRegDeleteKeyW(
            mount.Registry(),
            hRoot,
            L"Key"
        ) == ERROR_ACCESS_DENIED
    );
}

TEST_CASE("A custom hive's delta is empty: it holds nothing itself")
{
    CustomMount mount;
    const DWORD dwOne = 1;
    REQUIRE(
        VRegSetKeyValueW(
            mount.Registry(),
            mount.Root(),
            L"Key",
            L"v",
            REG_DWORD,
            &dwOne,
            sizeof(dwOne)
        ) == ERROR_SUCCESS
    );

    OwnedScript script;
    REQUIRE(VRegDeltaScript(mount.Hive(), HKEY_CURRENT_USER, L"VRegOracle", script.Receive()) == ERROR_SUCCESS);
    SIZE_T cOperations = 99;
    VRegGetScriptOperations(script, &cOperations);
    CHECK(cOperations == 0);
}

TEST_CASE("VRegCompareNames is the registry's collation")
{
    CHECK(VRegCompareNames(L"abc", L"ABC") == 0);
    CHECK(VRegCompareNames(L"a", L"b") < 0);
    CHECK(VRegCompareNames(L"b", L"a") > 0);
    CHECK(VRegCompareNames(L"", L"a") < 0);
    CHECK(VRegCompareNames(L"ä", L"Ä") == 0);
}

namespace
{
constexpr DWORD NAME = REG_NOTIFY_CHANGE_NAME;
constexpr DWORD LAST_SET = REG_NOTIFY_CHANGE_LAST_SET;

/// A manual-reset event a watch signals.
class Event
{
public:
    Event() noexcept :
        m_hEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    ~Event()
    {
        if (m_hEvent)
            CloseHandle(m_hEvent);
    }

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    [[nodiscard]] HANDLE Get() const noexcept
    {
        return m_hEvent;
    }

    /// A notify signals before it returns, so nothing needs to wait.
    [[nodiscard]] bool IsSet() const noexcept
    {
        return WaitForSingleObject(m_hEvent, 0) == WAIT_OBJECT_0;
    }

    void Reset() const noexcept
    {
        ResetEvent(m_hEvent);
    }

private:
    HANDLE m_hEvent;
};

/// A key of @ref ChangingProvider: subkeys and DWORD values the provider
/// changes on its own.
struct ChangingKey
{
    std::wstring name;
    std::vector<std::unique_ptr<ChangingKey>> children;
    std::vector<std::pair<std::wstring, DWORD>> values;
};

/// A read-only provider whose data changes outside VReg, which it then
/// reports through @ref VRegNotifyHiveChange.
class ChangingProvider
{
public:
    ChangingProvider()
    {
        REQUIRE(VRegCreateCustomHive(&VTBL, this, m_hive.Receive()) == ERROR_SUCCESS);
    }

    ChangingProvider(const ChangingProvider&) = delete;
    ChangingProvider& operator=(const ChangingProvider&) = delete;

    [[nodiscard]] VRegHive* Hive() const noexcept
    {
        return m_hive;
    }

    /// Adds a subkey @p pszName below @p parent, as the provider's own change.
    ChangingKey& AddKey(ChangingKey& parent, const wchar_t* const pszName)
    {
        parent.children.push_back(std::make_unique<ChangingKey>(ChangingKey{pszName, {}, {}}));
        return *parent.children.back();
    }

    ChangingKey& Root() noexcept
    {
        return m_root;
    }

private:
    static ChangingProvider& Of(VRegHive* const pHive) noexcept
    {
        return *static_cast<ChangingProvider*>(VRegGetHiveDelegate(pHive));
    }

    ChangingKey* KeyOf(VRegHiveKey* const pKey) noexcept
    {
        return VRegGetHiveKeyParent(pKey) ? static_cast<ChangingKey*>(VRegGetHiveKeyToken(pKey)) : &m_root;
    }

    static LSTATUS WINAPI OpenKey(
        VRegHive* const pHive,
        VRegHiveKey* const pParent,
        const LPCWSTR pszSubKey,
        void** const ppToken
    )
    {
        *ppToken = nullptr;
        for (const auto& pChild : Of(pHive).KeyOf(pParent)->children)
        {
            if (VRegCompareNames(pChild->name.c_str(), pszSubKey) == 0)
            {
                *ppToken = pChild.get();
                return ERROR_SUCCESS;
            }
        }
        return ERROR_FILE_NOT_FOUND;
    }

    static LSTATUS WINAPI QueryValue(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const LPCWSTR pszName,
        VRegHiveResult* const pResult
    )
    {
        for (const auto& [name, dwValue] : Of(pHive).KeyOf(pKey)->values)
        {
            if (VRegCompareNames(name.c_str(), pszName) == 0)
                return VRegHiveResultSetValue(pResult, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(dwValue));
        }
        return ERROR_FILE_NOT_FOUND;
    }

    static LSTATUS WINAPI EnumKey(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const DWORD dwIndex,
        VRegHiveResult* const pResult
    )
    {
        const ChangingKey* const pChanging = Of(pHive).KeyOf(pKey);
        if (dwIndex >= pChanging->children.size())
            return ERROR_NO_MORE_ITEMS;
        return VRegHiveResultSetName(pResult, pChanging->children[dwIndex]->name.c_str());
    }

    static LSTATUS WINAPI EnumValue(
        VRegHive* const pHive,
        VRegHiveKey* const pKey,
        const DWORD dwIndex,
        VRegHiveResult* const pResult
    )
    {
        const ChangingKey* const pChanging = Of(pHive).KeyOf(pKey);
        if (dwIndex >= pChanging->values.size())
            return ERROR_NO_MORE_ITEMS;

        const auto& [name, dwValue] = pChanging->values[dwIndex];
        const LSTATUS status = VRegHiveResultSetName(pResult, name.c_str());
        return status == ERROR_SUCCESS
            ? VRegHiveResultSetValue(pResult, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(dwValue))
            : status;
    }

    static constexpr VRegHiveVtbl VTBL{
        .OpenKey = &OpenKey,
        .QueryValue = &QueryValue,
        .EnumKey = &EnumKey,
        .EnumValue = &EnumValue,
    };

    ChangingKey m_root;
    OwnedHive m_hive;
};

/// Registers an asynchronous watch on @p hKey.
void Watch(VReg* const registry, const HKEY hKey, const BOOL bWatchSubtree, const DWORD dwFilter, const Event& event)
{
    REQUIRE(
        VRegNotifyChangeKeyValue(
            registry,
            hKey,
            bWatchSubtree,
            dwFilter,
            event.Get(),
            TRUE
        ) == ERROR_SUCCESS
    );
}

HKEY MustOpenKey(VReg* const registry, const HKEY hParent, const wchar_t* const pszSubKey)
{
    HKEY hKey{};
    REQUIRE(
        VRegOpenKeyExW(
            registry,
            hParent,
            pszSubKey,
            0,
            KEY_READ,
            &hKey
        ) == ERROR_SUCCESS
    );
    return hKey;
}
} // namespace

TEST_CASE("A provider's own value change signals last-set watches on the key once")
{
    ChangingProvider provider;
    ChangingKey& a = provider.AddKey(provider.Root(), L"A");
    OwnedRegistry registry = MountAlone(provider.Hive());
    const HKEY hA = MustOpenKey(registry, RootHandle(provider.Hive()), L"A");

    Event lastSet;
    Event name;
    Watch(registry, hA, FALSE, LAST_SET, lastSet);
    Watch(registry, hA, FALSE, NAME, name);

    a.values.emplace_back(L"v", 1);
    CHECK(VRegNotifyHiveChange(provider.Hive(), VReg_NodeOf(hA), LAST_SET) == ERROR_SUCCESS);
    CHECK(lastSet.IsSet());
    CHECK_FALSE(name.IsSet());
    CHECK(QueryDword(registry, hA, L"v") == 1);

    // A watch fires once.
    lastSet.Reset();
    CHECK(VRegNotifyHiveChange(provider.Hive(), VReg_NodeOf(hA), LAST_SET) == ERROR_SUCCESS);
    CHECK_FALSE(lastSet.IsSet());
}

TEST_CASE("A provider's own new key signals name watches on its parent and subtree watches above")
{
    ChangingProvider provider;
    ChangingKey& a = provider.AddKey(provider.Root(), L"A");
    ChangingKey& b = provider.AddKey(a, L"B");
    OwnedRegistry registry = MountAlone(provider.Hive());
    const HKEY hRoot = RootHandle(provider.Hive());
    const HKEY hB = MustOpenKey(registry, hRoot, L"A\\B");

    Event parent;
    Event subtree;
    Event rootOnly;
    Watch(registry, hB, FALSE, NAME, parent);
    Watch(registry, hRoot, TRUE, NAME, subtree);
    Watch(registry, hRoot, FALSE, NAME, rootOnly);

    provider.AddKey(b, L"C");
    CHECK(VRegNotifyHiveChange(provider.Hive(), VReg_NodeOf(hB), NAME) == ERROR_SUCCESS);
    CHECK(parent.IsSet());
    CHECK(subtree.IsSet());
    CHECK_FALSE(rootOnly.IsSet());
    MustOpenKey(registry, hB, L"C");
}

TEST_CASE("A frozen custom hive keeps its watches and still signals")
{
    ChangingProvider provider;
    ChangingKey& a = provider.AddKey(provider.Root(), L"A");
    OwnedRegistry registry = MountAlone(provider.Hive());
    const HKEY hA = MustOpenKey(registry, RootHandle(provider.Hive()), L"A");

    Event before;
    Watch(registry, hA, FALSE, LAST_SET, before);
    VRegFreezeHive(provider.Hive());
    Event after;
    Watch(registry, hA, FALSE, LAST_SET, after);

    a.values.emplace_back(L"v", 1);
    CHECK(VRegNotifyHiveChange(provider.Hive(), VReg_NodeOf(hA), LAST_SET) == ERROR_SUCCESS);
    CHECK(before.IsSet());
    CHECK(after.IsSet());
}

TEST_CASE("VRegNotifyHiveChange refuses a filter it never fires and a deleted key")
{
    CustomMount mount;
    HKEY hKey{};
    REQUIRE(
        VRegCreateKeyExW(
            mount.Registry(),
            mount.Root(),
            L"Key",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        ) == ERROR_SUCCESS
    );
    VRegHiveKey* const pKey = VReg_NodeOf(hKey);

    CHECK(VRegNotifyHiveChange(mount.Hive(), pKey, 0) == ERROR_INVALID_PARAMETER);
    CHECK(VRegNotifyHiveChange(mount.Hive(), pKey, REG_NOTIFY_CHANGE_ATTRIBUTES) == ERROR_INVALID_PARAMETER);

    REQUIRE(
        VRegDeleteKeyW(
            mount.Registry(),
            mount.Root(),
            L"Key"
        ) == ERROR_SUCCESS
    );
    CHECK(VRegNotifyHiveChange(mount.Hive(), pKey, NAME) == ERROR_KEY_DELETED);
}

TEST_CASE("A synchronous watch on a custom hive returns when the provider notifies")
{
    ChangingProvider provider;
    provider.AddKey(provider.Root(), L"A");
    OwnedRegistry registry = MountAlone(provider.Hive());
    const HKEY hA = MustOpenKey(registry, RootHandle(provider.Hive()), L"A");

    LSTATUS status = ERROR_IO_PENDING;
    Thread waiter(
        [&]
        {
            status = VRegNotifyChangeKeyValue(
                registry,
                hA,
                FALSE,
                LAST_SET,
                nullptr,
                FALSE
            );
        }
    );
    CHECK_FALSE(waiter.Wait(200));

    // The registration may not be in place yet; notify until the waiter returns.
    for (int i = 0; i < 100 && !waiter.Wait(50); ++i)
        REQUIRE(VRegNotifyHiveChange(provider.Hive(), VReg_NodeOf(hA), LAST_SET) == ERROR_SUCCESS);

    REQUIRE(waiter.Wait(5000));
    CHECK(status == ERROR_SUCCESS);
}
