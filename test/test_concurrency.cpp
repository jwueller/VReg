// Readers and writers on several threads against a writable hive: no crash, no
// deadlock, only status codes a registry may answer, and a final state that is
// consistent with itself.

#include "oracle.h"

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace VReg;
using namespace VReg::Test;
using Clock = std::chrono::steady_clock;

namespace
{
constexpr auto DURATION = std::chrono::seconds(2);
constexpr auto HANG_LIMIT = std::chrono::seconds(30);
constexpr unsigned KEYS = 6;
constexpr unsigned SUB_KEYS = 4;
constexpr unsigned VALUES = 4;
constexpr unsigned WRITERS = 4;
constexpr unsigned READERS = 4;

std::wstring KeyName(const unsigned i)
{
    return L"K" + std::to_wstring(i);
}

std::wstring SubName(const unsigned i)
{
    return L"S" + std::to_wstring(i);
}

std::wstring ValueName(const unsigned i)
{
    return L"V" + std::to_wstring(i);
}

/// What every writer stores under value @p index, so a final read can tell a
/// torn value from a written one.
DWORD ValueData(const unsigned index) noexcept
{
    return 1000 + index * 7;
}

class Random
{
public:
    explicit Random(const std::uint32_t seed) noexcept :
        m_state(seed * 2654435761u + 1)
    {
    }

    unsigned Next(const unsigned bound) noexcept
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state % bound;
    }

private:
    std::uint32_t m_state;
};

struct Shared
{
    std::atomic<bool> isStopped = false;
    std::atomic<unsigned> cDone = 0;
    std::atomic<unsigned long long> cOperations = 0;
    std::atomic<unsigned> cBad = 0;
    std::atomic<LSTATUS> firstBad = ERROR_SUCCESS;
    std::atomic<unsigned> cFired = 0;
    std::atomic<unsigned> cSnapshots = 0;
};

/// Counts a status a registry call may not answer in this workload.
void Judge(Shared& shared, const LSTATUS status) noexcept
{
    shared.cOperations.fetch_add(1, std::memory_order_relaxed);
    switch (status)
    {
    case ERROR_SUCCESS:
    case ERROR_FILE_NOT_FOUND:
    case ERROR_KEY_DELETED:
    case ERROR_ACCESS_DENIED:
    case ERROR_NO_MORE_ITEMS:
        return;
    default:
        shared.cBad.fetch_add(1, std::memory_order_relaxed);
        LSTATUS none = ERROR_SUCCESS;
        shared.firstBad.compare_exchange_strong(none, status);
    }
}

std::wstring RandomPath(Random& random)
{
    std::wstring path = KeyName(random.Next(KEYS));
    if (random.Next(2))
        path += L'\\' + SubName(random.Next(SUB_KEYS));
    return path;
}

void WriterLoop(
    VRegRegistry* const registry,
    const HKEY hRoot,
    Shared& shared,
    const std::uint32_t seed
)
{
    Random random(seed);
    while (!shared.isStopped.load(std::memory_order_relaxed))
    {
        const auto path = RandomPath(random);
        switch (random.Next(7))
        {
        case 0:
        case 1:
        {
            HKEY hKey{};
            const auto status =
                VRegCreateKeyExW(
                    registry,
                    hRoot,
                    path.c_str(),
                    0,
                    nullptr,
                    0,
                    KEY_ALL_ACCESS,
                    nullptr,
                    &hKey,
                    nullptr
                );
            Judge(shared, status);
            if (status == ERROR_SUCCESS)
                VRegCloseKey(
                    registry,
                    hKey
                );
            break;
        }
        case 2:
        case 3:
        {
            HKEY hKey{};
            const auto status = VRegOpenKeyExW(
                registry,
                hRoot,
                path.c_str(),
                0,
                KEY_SET_VALUE,
                &hKey
            );
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;

            const auto index = random.Next(VALUES);
            const auto name = ValueName(index);
            const DWORD dwValue = ValueData(index);
            Judge(
                shared,
                VRegSetValueExW(
                    registry,
                    hKey,
                    name.c_str(),
                    0,
                    REG_DWORD,
                    reinterpret_cast<const BYTE*>(&dwValue),
                    sizeof(dwValue)
                )
            );
            VRegCloseKey(
                registry,
                hKey
            );
            break;
        }
        case 4:
        {
            HKEY hKey{};
            const auto status = VRegOpenKeyExW(
                registry,
                hRoot,
                path.c_str(),
                0,
                KEY_SET_VALUE,
                &hKey
            );
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;

            Judge(
                shared,
                VRegDeleteValueW(
                    registry,
                    hKey,
                    ValueName(random.Next(VALUES)).c_str()
                )
            );
            VRegCloseKey(
                registry,
                hKey
            );
            break;
        }
        case 5:
            Judge(
                shared,
                VRegDeleteKeyExW(
                    registry,
                    hRoot,
                    path.c_str(),
                    0,
                    0
                )
            );
            break;
        case 6:
            // Rare, because it empties whole subtrees.
            if (random.Next(8) == 0)
                Judge(
                    shared,
                    VRegDeleteTreeW(
                        registry,
                        hRoot,
                        KeyName(random.Next(KEYS)).c_str()
                    )
                );
            break;
        }
    }
}

void ReadKey(
    VRegRegistry* const registry,
    const HKEY hKey,
    Shared& shared,
    Random& random
)
{
    switch (random.Next(4))
    {
    case 0:
    {
        DWORD dwValue{};
        DWORD cbValue = sizeof(dwValue);
        DWORD dwType{};
        const auto index = random.Next(VALUES);
        const auto name = ValueName(index);
        const auto status =
            VRegQueryValueExW(
                registry,
                hKey,
                name.c_str(),
                nullptr,
                &dwType,
                reinterpret_cast<BYTE*>(&dwValue),
                &cbValue
            );
        Judge(shared, status);
        if (
            status == ERROR_SUCCESS
            && (dwType != REG_DWORD || cbValue != sizeof(dwValue))
        )
            Judge(shared, ERROR_INVALID_DATA);
        break;
    }
    case 1:
        for (DWORD i = 0; i < SUB_KEYS + 2; ++i)
        {
            wchar_t szName[64];
            DWORD cchName = ARRAYSIZE(szName);
            const auto status = VRegEnumKeyExW(
                registry,
                hKey,
                i,
                szName,
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;
        }
        break;
    case 2:
        for (DWORD i = 0; i < VALUES + 2; ++i)
        {
            wchar_t szName[64];
            DWORD cchName = ARRAYSIZE(szName);
            DWORD dwType{};
            BYTE data[16];
            DWORD cbData = sizeof(data);
            const auto status =
                VRegEnumValueW(
                    registry,
                    hKey,
                    i,
                    szName,
                    &cchName,
                    nullptr,
                    &dwType,
                    data,
                    &cbData
                );
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;
        }
        break;
    case 3:
    {
        DWORD cSubKeys{};
        DWORD cValues{};
        const auto status = VRegQueryInfoKeyW(
            registry,
            hKey,
            nullptr,
            nullptr,
            nullptr,
            &cSubKeys,
            nullptr,
            nullptr,
            &cValues,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        );
        Judge(shared, status);
        // Every name comes from a small closed set, in the upper and the lower
        // layer alike, so a count above it means an entry was counted twice.
        if (
            status == ERROR_SUCCESS && (cSubKeys > SUB_KEYS || cValues > VALUES)
        )
            Judge(shared, ERROR_INVALID_DATA);
        break;
    }
    }
}

void ReaderLoop(
    VRegRegistry* const registry,
    const HKEY hRoot,
    Shared& shared,
    const std::uint32_t seed
)
{
    Random random(seed);
    while (!shared.isStopped.load(std::memory_order_relaxed))
    {
        const auto path = RandomPath(random);
        HKEY hKey{};
        const auto status = VRegOpenKeyExW(
            registry,
            hRoot,
            path.c_str(),
            0,
            KEY_READ,
            &hKey
        );
        Judge(shared, status);
        if (status != ERROR_SUCCESS)
            continue;

        // The same handle is read several times while writers delete and
        // recreate the key under it.
        for (int i = 0; i < 4; ++i)
            ReadKey(registry, hKey, shared, random);

        VRegCloseKey(
            registry,
            hKey
        );
    }
}

/// Registers one-shot watches on the whole hive and counts how many fire.
void WatcherLoop(VRegRegistry* const registry, const HKEY hRoot, Shared& shared) noexcept
{
    const HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    while (!shared.isStopped.load(std::memory_order_relaxed))
    {
        ResetEvent(hEvent);
        const auto status =
            VRegNotifyChangeKeyValue(
                registry,
                hRoot,
                TRUE,
                REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET,
                hEvent,
                TRUE
            );
        Judge(shared, status);
        if (WaitForSingleObject(hEvent, 100) == WAIT_OBJECT_0)
            shared.cFired.fetch_add(1, std::memory_order_relaxed);
    }

    CloseHandle(hEvent);
}

/// Exports the whole hive while the writers run.
void ExporterLoop(
    VRegRegistry* const registry,
    const wchar_t* const pszMount,
    Shared& shared
)
{
    while (!shared.isStopped.load(std::memory_order_relaxed))
    {
        OwnedScript script;
        const auto status = VRegSnapshotScript(registry, HKEY_CURRENT_USER, pszMount, nullptr, script.Receive());
        // A key may be deleted between listing and opening it.
        if (status == ERROR_SUCCESS)
            shared.cSnapshots.fetch_add(1, std::memory_order_relaxed);
        else
            Judge(shared, status);
    }
}

/// The hive, read by one thread with nothing else running, must agree with
/// itself.
void CheckConsistent(VRegRegistry* const registry, const HKEY hRoot)
{
    std::set<std::wstring> keys;
    for (DWORD i = 0;; ++i)
    {
        wchar_t szName[64];
        DWORD cchName = ARRAYSIZE(szName);
        const auto status = VRegEnumKeyExW(
            registry,
            hRoot,
            i,
            szName,
            &cchName,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        );
        if (status == ERROR_NO_MORE_ITEMS)
            break;
        REQUIRE(status == ERROR_SUCCESS);
        keys.insert(std::wstring(szName, cchName));
    }

    DWORD cSubKeys{};
    REQUIRE(
        VRegQueryInfoKeyW(
            registry,
            hRoot,
            nullptr,
            nullptr,
            nullptr,
            &cSubKeys,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        )
        == ERROR_SUCCESS
    );
    CHECK(cSubKeys == keys.size());

    for (unsigned k = 0; k < KEYS; ++k)
    {
        HKEY hKey{};
        const auto status = VRegOpenKeyExW(
            registry,
            hRoot,
            KeyName(k).c_str(),
            0,
            KEY_READ,
            &hKey
        );
        CHECK((status == ERROR_SUCCESS) == (keys.count(KeyName(k)) == 1));
        if (status != ERROR_SUCCESS)
            continue;

        std::set<std::wstring> subKeys;
        for (DWORD i = 0;; ++i)
        {
            wchar_t szName[64];
            DWORD cchName = ARRAYSIZE(szName);
            const auto enumStatus = VRegEnumKeyExW(
                registry,
                hKey,
                i,
                szName,
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            REQUIRE(enumStatus == ERROR_SUCCESS);
            CHECK(subKeys.insert(std::wstring(szName, cchName)).second);
        }

        std::set<std::wstring> values;
        for (DWORD i = 0;; ++i)
        {
            wchar_t szName[64];
            DWORD cchName = ARRAYSIZE(szName);
            DWORD dwType{};
            DWORD dwValue{};
            DWORD cbValue = sizeof(dwValue);
            const auto enumStatus = VRegEnumValueW(
                registry,
                hKey,
                i,
                szName,
                &cchName,
                nullptr,
                &dwType,
                reinterpret_cast<BYTE*>(&dwValue),
                &cbValue
            );
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            REQUIRE(enumStatus == ERROR_SUCCESS);
            CHECK(values.insert(std::wstring(szName, cchName)).second);
            CHECK(dwType == REG_DWORD);
            const auto index = std::stoul(
                std::wstring(szName + 1, cchName - 1)
            );
            CHECK(dwValue == ValueData(static_cast<unsigned>(index)));
        }

        DWORD cSubs{};
        DWORD cVals{};
        REQUIRE(
            VRegQueryInfoKeyW(
                registry,
                hKey,
                nullptr,
                nullptr,
                nullptr,
                &cSubs,
                nullptr,
                nullptr,
                &cVals,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            )
            == ERROR_SUCCESS
        );
        CHECK(cSubs == subKeys.size());
        CHECK(cVals == values.size());

        for (const auto& name : subKeys)
        {
            HKEY hSub{};
            CHECK(
                VRegOpenKeyExW(
                    registry,
                    hKey,
                    name.c_str(),
                    0,
                    KEY_READ,
                    &hSub
                ) == ERROR_SUCCESS
            );
            VRegCloseKey(
                registry,
                hSub
            );
        }

        for (const auto& name : values)
        {
            DWORD dwValue{};
            DWORD cbValue = sizeof(dwValue);
            CHECK(
                VRegQueryValueExW(
                    registry,
                    hKey,
                    name.c_str(),
                    nullptr,
                    nullptr,
                    reinterpret_cast<BYTE*>(&dwValue),
                    &cbValue
                )
                == ERROR_SUCCESS
            );
        }

        VRegCloseKey(
            registry,
            hKey
        );
    }
}

/// Runs the workload against the hive mounted at @p pszMount for a bounded
/// time. @p pfnFreezeMidway, when given, is called once halfway.
void RunStress(
    VRegRegistry* const registry,
    const wchar_t* const pszMount,
    const HKEY hRoot,
    void (*pfnFreezeMidway)(void*),
    void* pFreezeContext
)
{
    Shared shared;
    std::vector<std::thread> threads;
    auto launch = [&](auto&& body)
    {
        threads.emplace_back(
            [&shared, body]
            {
                body();
                shared.cDone.fetch_add(1);
            }
        );
    };

    for (unsigned i = 0; i < WRITERS; ++i)
        launch([&, i] { WriterLoop(registry, hRoot, shared, 100 + i); });
    for (unsigned i = 0; i < READERS; ++i)
        launch([&, i] { ReaderLoop(registry, hRoot, shared, 200 + i); });
    launch([&] { WatcherLoop(registry, hRoot, shared); });
    launch([&] { ExporterLoop(registry, pszMount, shared); });

    const auto start = Clock::now();
    bool isFrozen = pfnFreezeMidway == nullptr;
    while (Clock::now() - start < DURATION)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!isFrozen && Clock::now() - start >= DURATION / 2)
        {
            pfnFreezeMidway(pFreezeContext);
            isFrozen = true;
        }
    }

    shared.isStopped.store(true);
    const auto stopped = Clock::now();
    while (shared.cDone.load() != threads.size())
    {
        if (Clock::now() - stopped > HANG_LIMIT)
        {
            // A thread that cannot be joined would hang the process; report and
            // leave.
            std::fputs(
                "concurrency test: worker threads did not finish (deadlock)\n",
                stderr
            );
            std::_Exit(3);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (auto& thread : threads)
        thread.join();

    INFO(
        "operations: "
        << shared.cOperations.load()
        << ", watches fired: "
        << shared.cFired.load()
    );
    INFO("first unexpected status: " << shared.firstBad.load());
    CHECK(shared.cBad.load() == 0);
    CHECK(shared.cOperations.load() > 1000);
    CHECK(shared.cSnapshots.load() > 0);
    if (pfnFreezeMidway == nullptr)
        CHECK(shared.cFired.load() > 0);

    CheckConsistent(registry, hRoot);
}
} // namespace

TEST_CASE("Readers and writers on a writable Hive")
{
    OwnedHive hive = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(
        VRegMount(registry, HKEY_CURRENT_USER, L"VRegStress", hive, 0) == ERROR_SUCCESS
    );
    RunStress(
        registry,
        L"VRegStress",
        RootHandle(hive),
        nullptr,
        nullptr
    );
}

TEST_CASE("Readers and writers on a Hive frozen halfway")
{
    OwnedHive hive = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(
        VRegMount(registry, HKEY_CURRENT_USER, L"VRegStress", hive, 0) == ERROR_SUCCESS
    );
    RunStress(
        registry,
        L"VRegStress",
        RootHandle(hive),
        [](void* const pHive) { VRegFreezeHive(static_cast<VRegHive*>(pHive)); },
        hive.Get()
    );
}

TEST_CASE("Readers and writers on a writable Hive over an app hive")
{
    OverlayMount mount;
    VRegRegistry* const native = mount.Native();

    // The lower layer holds names from the same closed set, so it shadows and
    // is shadowed.
    for (unsigned k = 0; k < KEYS; k += 2)
    {
        for (unsigned s = 0; s < SUB_KEYS; s += 2)
        {
            const auto path = KeyName(k) + L'\\' + SubName(s);
            HKEY hKey{};
            REQUIRE(
                VRegCreateKeyExW(
                    native,
                    mount.LowerRoot(),
                    path.c_str(),
                    0,
                    nullptr,
                    0,
                    KEY_ALL_ACCESS,
                    nullptr,
                    &hKey,
                    nullptr
                )
                == ERROR_SUCCESS
            );
            for (unsigned v = 0; v < VALUES; v += 2)
            {
                const DWORD dwValue = ValueData(v);
                const auto name = ValueName(v);
                REQUIRE(
                    VRegSetValueExW(
                        native,
                        hKey,
                        name.c_str(),
                        0,
                        REG_DWORD,
                        reinterpret_cast<const BYTE*>(&dwValue),
                        sizeof(dwValue)
                    )
                    == ERROR_SUCCESS
                );
            }
            VRegCloseKey(
                native,
                hKey
            );
        }
    }

    RunStress(mount.Registry(), L"VRegOracle", mount.Root(), nullptr, nullptr);
}
