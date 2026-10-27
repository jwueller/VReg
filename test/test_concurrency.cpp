// Readers and writers on several threads against a writable hive: no crash, no deadlock, only status codes a
// registry may answer, and a final state that is consistent with itself.

#include "oracle.h"

#include <VReg/memory_hive.h>
#include <VReg/native_registry.h>
#include <VReg/overlay_hive.h>
#include <VReg/regfile.h>
#include <VReg/virtual_registry.h>

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
constexpr auto kDuration = std::chrono::seconds(2);
constexpr auto kHangLimit = std::chrono::seconds(30);
constexpr unsigned kKeys = 6;
constexpr unsigned kSubKeys = 4;
constexpr unsigned kValues = 4;
constexpr unsigned kWriters = 4;
constexpr unsigned kReaders = 4;

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

/// What every writer stores under value @p index, so a final read can tell a torn value from a written one.
DWORD ValueData(const unsigned index)
{
    return 1000 + index * 7;
}

class Random
{
public:
    explicit Random(const std::uint32_t seed) noexcept
        : m_state(seed * 2654435761u + 1)
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
    std::wstring path = KeyName(random.Next(kKeys));
    if (random.Next(2))
        path += L'\\' + SubName(random.Next(kSubKeys));
    return path;
}

void WriterLoop(Registry& registry, const HKEY hRoot, Shared& shared, const std::uint32_t seed)
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
                registry.CreateKeyExW(hRoot, path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr);
            Judge(shared, status);
            if (status == ERROR_SUCCESS)
                registry.CloseKey(hKey);
            break;
        }
        case 2:
        case 3:
        {
            HKEY hKey{};
            const auto status = registry.OpenKeyExW(hRoot, path.c_str(), 0, KEY_SET_VALUE, &hKey);
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;

            const auto index = random.Next(kValues);
            const auto name = ValueName(index);
            const DWORD dwValue = ValueData(index);
            Judge(
                shared,
                registry.SetValueExW(
                    hKey,
                    name.c_str(),
                    0,
                    REG_DWORD,
                    reinterpret_cast<const BYTE*>(&dwValue),
                    sizeof(dwValue)
                )
            );
            registry.CloseKey(hKey);
            break;
        }
        case 4:
        {
            HKEY hKey{};
            const auto status = registry.OpenKeyExW(hRoot, path.c_str(), 0, KEY_SET_VALUE, &hKey);
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;

            Judge(shared, registry.DeleteValueW(hKey, ValueName(random.Next(kValues)).c_str()));
            registry.CloseKey(hKey);
            break;
        }
        case 5:
            Judge(shared, registry.DeleteKeyExW(hRoot, path.c_str(), 0, 0));
            break;
        case 6:
            // Rare, because it empties whole subtrees.
            if (random.Next(8) == 0)
                Judge(shared, registry.DeleteTreeW(hRoot, KeyName(random.Next(kKeys)).c_str()));
            break;
        }
    }
}

void ReadKey(Registry& registry, const HKEY hKey, Shared& shared, Random& random)
{
    switch (random.Next(4))
    {
    case 0:
    {
        DWORD dwValue{};
        DWORD cbValue = sizeof(dwValue);
        DWORD dwType{};
        const auto index = random.Next(kValues);
        const auto name = ValueName(index);
        const auto status =
            registry.QueryValueExW(hKey, name.c_str(), nullptr, &dwType, reinterpret_cast<BYTE*>(&dwValue), &cbValue);
        Judge(shared, status);
        if (status == ERROR_SUCCESS && (dwType != REG_DWORD || cbValue != sizeof(dwValue)))
            Judge(shared, ERROR_INVALID_DATA);
        break;
    }
    case 1:
        for (DWORD i = 0; i < kSubKeys + 2; ++i)
        {
            wchar_t szName[64];
            DWORD cchName = ARRAYSIZE(szName);
            const auto status = registry.EnumKeyExW(hKey, i, szName, &cchName, nullptr, nullptr, nullptr, nullptr);
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;
        }
        break;
    case 2:
        for (DWORD i = 0; i < kValues + 2; ++i)
        {
            wchar_t szName[64];
            DWORD cchName = ARRAYSIZE(szName);
            DWORD dwType{};
            BYTE data[16];
            DWORD cbData = sizeof(data);
            const auto status =
                registry.EnumValueW(hKey, i, szName, &cchName, nullptr, &dwType, data, &cbData);
            Judge(shared, status);
            if (status != ERROR_SUCCESS)
                break;
        }
        break;
    case 3:
    {
        DWORD cSubKeys{};
        DWORD cValues{};
        const auto status = registry.QueryInfoKeyW(
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
        // Every name comes from a small closed set, in the upper and the lower layer alike, so a count above it
        // means an entry was counted twice.
        if (status == ERROR_SUCCESS && (cSubKeys > kSubKeys || cValues > kValues))
            Judge(shared, ERROR_INVALID_DATA);
        break;
    }
    }
}

void ReaderLoop(Registry& registry, const HKEY hRoot, Shared& shared, const std::uint32_t seed)
{
    Random random(seed);
    while (!shared.isStopped.load(std::memory_order_relaxed))
    {
        const auto path = RandomPath(random);
        HKEY hKey{};
        const auto status = registry.OpenKeyExW(hRoot, path.c_str(), 0, KEY_READ, &hKey);
        Judge(shared, status);
        if (status != ERROR_SUCCESS)
            continue;

        // The same handle is read several times while writers delete and recreate the key under it.
        for (int i = 0; i < 4; ++i)
            ReadKey(registry, hKey, shared, random);

        registry.CloseKey(hKey);
    }
}

/// Registers one-shot watches on the whole hive and counts how many fire.
void WatcherLoop(Registry& registry, const HKEY hRoot, Shared& shared)
{
    const HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    while (!shared.isStopped.load(std::memory_order_relaxed))
    {
        ResetEvent(hEvent);
        const auto status =
            registry.NotifyChangeKeyValue(hRoot, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, hEvent, TRUE);
        Judge(shared, status);
        if (WaitForSingleObject(hEvent, 100) == WAIT_OBJECT_0)
            shared.cFired.fetch_add(1, std::memory_order_relaxed);
    }

    CloseHandle(hEvent);
}

/// Exports the whole hive while the writers run.
void ExporterLoop(Registry& registry, const wchar_t* const pszMount, Shared& shared)
{
    while (!shared.isStopped.load(std::memory_order_relaxed))
    {
        std::vector<std::byte> file;
        const auto status = RegFile::ExportSnapshot(registry, HKEY_CURRENT_USER, pszMount, RegFile::Format::Unicode, file);
        // A key may be deleted between listing and opening it.
        if (status == ERROR_SUCCESS)
            shared.cSnapshots.fetch_add(1, std::memory_order_relaxed);
        else
            Judge(shared, status);
    }
}

/// The hive, read by one thread with nothing else running, must agree with itself.
void CheckConsistent(Registry& registry, const HKEY hRoot)
{
    std::set<std::wstring> keys;
    for (DWORD i = 0;; ++i)
    {
        wchar_t szName[64];
        DWORD cchName = ARRAYSIZE(szName);
        const auto status = registry.EnumKeyExW(hRoot, i, szName, &cchName, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS)
            break;
        REQUIRE(status == ERROR_SUCCESS);
        keys.insert(std::wstring(szName, cchName));
    }

    DWORD cSubKeys{};
    REQUIRE(
        registry.QueryInfoKeyW(hRoot, nullptr, nullptr, nullptr, &cSubKeys, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
        == ERROR_SUCCESS
    );
    CHECK(cSubKeys == keys.size());

    for (unsigned k = 0; k < kKeys; ++k)
    {
        HKEY hKey{};
        const auto status = registry.OpenKeyExW(hRoot, KeyName(k).c_str(), 0, KEY_READ, &hKey);
        CHECK((status == ERROR_SUCCESS) == (keys.count(KeyName(k)) == 1));
        if (status != ERROR_SUCCESS)
            continue;

        std::set<std::wstring> subKeys;
        for (DWORD i = 0;; ++i)
        {
            wchar_t szName[64];
            DWORD cchName = ARRAYSIZE(szName);
            const auto enumStatus = registry.EnumKeyExW(hKey, i, szName, &cchName, nullptr, nullptr, nullptr, nullptr);
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
            const auto enumStatus = registry.EnumValueW(
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
            const auto index = std::stoul(std::wstring(szName + 1, cchName - 1));
            CHECK(dwValue == ValueData(static_cast<unsigned>(index)));
        }

        DWORD cSubs{};
        DWORD cVals{};
        REQUIRE(
            registry.QueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &cSubs, nullptr, nullptr, &cVals, nullptr, nullptr, nullptr, nullptr)
            == ERROR_SUCCESS
        );
        CHECK(cSubs == subKeys.size());
        CHECK(cVals == values.size());

        for (const auto& name : subKeys)
        {
            HKEY hSub{};
            CHECK(registry.OpenKeyExW(hKey, name.c_str(), 0, KEY_READ, &hSub) == ERROR_SUCCESS);
            registry.CloseKey(hSub);
        }

        for (const auto& name : values)
        {
            DWORD dwValue{};
            DWORD cbValue = sizeof(dwValue);
            CHECK(
                registry.QueryValueExW(hKey, name.c_str(), nullptr, nullptr, reinterpret_cast<BYTE*>(&dwValue), &cbValue)
                == ERROR_SUCCESS
            );
        }

        registry.CloseKey(hKey);
    }
}

/// Runs the workload against the hive mounted at @p pszMount for a bounded time. @p pfnFreezeMidway, when given, is
/// called once halfway.
void RunStress(Registry& registry, const wchar_t* const pszMount, const HKEY hRoot, void (*pfnFreezeMidway)(void*), void* pFreezeContext)
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

    for (unsigned i = 0; i < kWriters; ++i)
        launch([&, i] { WriterLoop(registry, hRoot, shared, 100 + i); });
    for (unsigned i = 0; i < kReaders; ++i)
        launch([&, i] { ReaderLoop(registry, hRoot, shared, 200 + i); });
    launch([&] { WatcherLoop(registry, hRoot, shared); });
    launch([&] { ExporterLoop(registry, pszMount, shared); });

    const auto start = Clock::now();
    bool isFrozen = pfnFreezeMidway == nullptr;
    while (Clock::now() - start < kDuration)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!isFrozen && Clock::now() - start >= kDuration / 2)
        {
            pfnFreezeMidway(pFreezeContext);
            isFrozen = true;
        }
    }

    shared.isStopped.store(true);
    const auto stopped = Clock::now();
    while (shared.cDone.load() != threads.size())
    {
        if (Clock::now() - stopped > kHangLimit)
        {
            // A thread that cannot be joined would hang the process; report and leave.
            std::fputs("concurrency test: worker threads did not finish (deadlock)\n", stderr);
            std::_Exit(3);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (auto& thread : threads)
        thread.join();

    INFO("operations: " << shared.cOperations.load() << ", watches fired: " << shared.cFired.load());
    INFO("first unexpected status: " << shared.firstBad.load());
    CHECK(shared.cBad.load() == 0);
    CHECK(shared.cOperations.load() > 1000);
    CHECK(shared.cSnapshots.load() > 0);
    if (pfnFreezeMidway == nullptr)
        CHECK(shared.cFired.load() > 0);

    CheckConsistent(registry, hRoot);
}
} // namespace

TEST_CASE("Readers and writers on a writable MemoryHive")
{
    MemoryHive hive;
    VirtualRegistry registry;
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"VRegStress", hive) == ERROR_SUCCESS);
    RunStress(registry, L"VRegStress", Hive::HandleOf(hive.Root()), nullptr, nullptr);
}

TEST_CASE("Readers and writers on a MemoryHive frozen halfway")
{
    MemoryHive hive;
    VirtualRegistry registry;
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"VRegStress", hive) == ERROR_SUCCESS);
    RunStress(registry, L"VRegStress", Hive::HandleOf(hive.Root()), [](void* const pHive) { static_cast<MemoryHive*>(pHive)->Freeze(); }, &hive);
}

TEST_CASE("Readers and writers on a writable OverlayHive over an app hive")
{
    OverlayMount mount;
    auto& native = mount.Native();

    // The lower layer holds names from the same closed set, so it shadows and is shadowed.
    for (unsigned k = 0; k < kKeys; k += 2)
    {
        for (unsigned s = 0; s < kSubKeys; s += 2)
        {
            const auto path = KeyName(k) + L'\\' + SubName(s);
            HKEY hKey{};
            REQUIRE(
                native.CreateKeyExW(mount.LowerRoot(), path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr)
                == ERROR_SUCCESS
            );
            for (unsigned v = 0; v < kValues; v += 2)
            {
                const DWORD dwValue = ValueData(v);
                const auto name = ValueName(v);
                REQUIRE(
                    native.SetValueExW(hKey, name.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(dwValue))
                    == ERROR_SUCCESS
                );
            }
            native.CloseKey(hKey);
        }
    }

    RunStress(mount.Registry(), L"VRegOracle", mount.Root(), nullptr, nullptr);
}
