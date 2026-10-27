#include "oracle.h"
#include "scenario.h"

#include <VReg/handle.h>
#include <VReg/memory_hive.h>
#include <VReg/native_registry.h>
#include <VReg/virtual_registry.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

using namespace VReg;
using namespace VReg::Test;

namespace
{
HKEY CreateKey(Registry& registry, const HKEY hParent, const wchar_t* const pszPath)
{
    HKEY hKey{};
    REQUIRE(
        registry.CreateKeyExW(hParent, pszPath, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr) == ERROR_SUCCESS
    );
    return hKey;
}

HKEY OpenKey(Registry& registry, const HKEY hParent, const wchar_t* const pszPath)
{
    HKEY hKey{};
    REQUIRE(registry.OpenKeyExW(hParent, pszPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS);
    return hKey;
}

LSTATUS SetDword(Registry& registry, const HKEY hKey, const wchar_t* const pszName, const DWORD dwValue)
{
    return registry.SetValueExW(hKey, pszName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(dwValue));
}

DWORD QueryDword(Registry& registry, const HKEY hKey, const wchar_t* const pszName, LSTATUS& status)
{
    DWORD dwValue = 0;
    DWORD cbValue = sizeof(dwValue);
    status = registry.QueryValueExW(hKey, pszName, nullptr, nullptr, reinterpret_cast<BYTE*>(&dwValue), &cbValue);
    return dwValue;
}
} // namespace

TEST_CASE("A node has one handle however it is reached")
{
    MemoryHive hive;
    VirtualRegistry registry;
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"Test", hive) == ERROR_SUCCESS);

    const HKEY hRoot = Hive::HandleOf(hive.Root());
    CHECK(IsVirtualHKey(hRoot));

    const HKEY hCreated = CreateKey(registry, hRoot, L"A\\B");
    const HKEY hOpened = OpenKey(registry, hRoot, L"a\\b");
    const HKEY hViaParent = OpenKey(registry, OpenKey(registry, hRoot, L"A"), L"B");
    const HKEY hViaPath = OpenKey(registry, HKEY_CURRENT_USER, L"Test\\A\\B");

    CHECK(IsVirtualHKey(hCreated));
    CHECK(hOpened == hCreated);
    CHECK(hViaParent == hCreated);
    CHECK(hViaPath == hCreated);
    CHECK(OpenKey(registry, HKEY_CURRENT_USER, L"Test") == hRoot);
    CHECK(registry.CloseKey(hCreated) == ERROR_SUCCESS);
    CHECK(OpenKey(registry, hRoot, L"A\\B") == hCreated);
}

TEST_CASE("A deleted node and its children answer ERROR_KEY_DELETED")
{
    MemoryHive hive;
    VirtualRegistry registry;
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"Test", hive) == ERROR_SUCCESS);
    const HKEY hRoot = Hive::HandleOf(hive.Root());

    const HKEY hParent = CreateKey(registry, hRoot, L"Parent");
    const HKEY hChild = CreateKey(registry, hRoot, L"Parent\\Child");
    REQUIRE(SetDword(registry, hChild, L"v", 1) == ERROR_SUCCESS);

    REQUIRE(registry.DeleteTreeW(hRoot, L"Parent") == ERROR_SUCCESS);

    LSTATUS status = ERROR_SUCCESS;
    QueryDword(registry, hParent, L"v", status);
    CHECK(status == ERROR_KEY_DELETED);
    QueryDword(registry, hChild, L"v", status);
    CHECK(status == ERROR_KEY_DELETED);
    CHECK(SetDword(registry, hChild, L"v", 2) == ERROR_KEY_DELETED);
    CHECK(registry.FlushKey(hChild) == ERROR_KEY_DELETED);

    HKEY hKey = nullptr;
    CHECK(registry.OpenKeyExW(hParent, L"Child", 0, KEY_READ, &hKey) == ERROR_KEY_DELETED);
    CHECK(registry.OpenKeyExW(hRoot, L"Parent", 0, KEY_READ, &hKey) == ERROR_FILE_NOT_FOUND);
    CHECK(registry.CloseKey(hChild) == ERROR_SUCCESS);
}

TEST_CASE("A frozen hive refuses writes and still answers reads")
{
    MemoryHive hive;
    VirtualRegistry registry;
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"Test", hive) == ERROR_SUCCESS);
    const HKEY hRoot = Hive::HandleOf(hive.Root());

    const HKEY hSub = CreateKey(registry, hRoot, L"Sub");
    REQUIRE(SetDword(registry, hSub, L"v", 7) == ERROR_SUCCESS);
    REQUIRE_FALSE(hive.IsFrozen());

    hive.Freeze();
    CHECK(hive.IsFrozen());

    LSTATUS status = ERROR_SUCCESS;
    CHECK(QueryDword(registry, hSub, L"v", status) == 7);
    CHECK(status == ERROR_SUCCESS);
    CHECK(OpenKey(registry, hRoot, L"SUB") == hSub);

    std::array<wchar_t, 8> name{};
    DWORD cchName = static_cast<DWORD>(name.size());
    CHECK(registry.EnumKeyExW(hRoot, 0, name.data(), &cchName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
    CHECK(std::wstring(name.data(), cchName) == L"Sub");

    HKEY hKey = nullptr;
    DWORD dwDisposition = 0;
    CHECK(registry.CreateKeyExW(hRoot, L"New", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition)
          == ERROR_ACCESS_DENIED);
    CHECK(SetDword(registry, hSub, L"v", 8) == ERROR_ACCESS_DENIED);
    CHECK(registry.DeleteValueW(hSub, L"v") == ERROR_ACCESS_DENIED);
    CHECK(registry.DeleteKeyExW(hRoot, L"Sub", 0, 0) == ERROR_ACCESS_DENIED);
    CHECK(registry.DeleteTreeW(hRoot, L"Sub") == ERROR_ACCESS_DENIED);
    CHECK(QueryDword(registry, hSub, L"v", status) == 7);
}

TEST_CASE("Mount rejects what cannot be mounted")
{
    MemoryHive hive;
    MemoryHive other;
    VirtualRegistry registry;

    CHECK(registry.Mount(nullptr, L"A", hive) == ERROR_INVALID_HANDLE);
    CHECK(registry.Mount(Hive::HandleOf(hive.Root()), L"A", hive) == ERROR_INVALID_HANDLE);
    CHECK(registry.Mount(HKEY_CURRENT_USER, L"\\Leading", hive) == ERROR_BAD_PATHNAME);
    CHECK(registry.Mount(HKEY_CURRENT_USER, std::wstring(300, L'k'), hive) == ERROR_BAD_PATHNAME);

    CHECK(registry.Mount(HKEY_CURRENT_USER, L"Software\\Test", hive) == ERROR_SUCCESS);
    CHECK(registry.Mount(HKEY_CURRENT_USER, L"software\\TEST\\", other) == ERROR_ALREADY_EXISTS);
    CHECK(registry.Mount(HKEY_LOCAL_MACHINE, L"Software\\Test", other) == ERROR_SUCCESS);
}

TEST_CASE("Pure in-memory configuration")
{
    MemoryHive machine;
    MemoryHive user;
    VirtualRegistry registry;
    REQUIRE(registry.Mount(HKEY_LOCAL_MACHINE, L"", machine) == ERROR_SUCCESS);
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"", user) == ERROR_SUCCESS);

    const HKEY hMachine = CreateKey(registry, HKEY_LOCAL_MACHINE, L"Software\\Vendor");
    const HKEY hUser = CreateKey(registry, HKEY_CURRENT_USER, L"Software\\Vendor");
    CHECK(hMachine != hUser);
    REQUIRE(SetDword(registry, hMachine, L"v", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(registry, hUser, L"v", 2) == ERROR_SUCCESS);

    LSTATUS status = ERROR_SUCCESS;
    const HKEY hMachineAgain = OpenKey(registry, HKEY_LOCAL_MACHINE, L"software\\vendor");
    CHECK(QueryDword(registry, hMachineAgain, L"v", status) == 1);
    CHECK(QueryDword(registry, OpenKey(registry, HKEY_CURRENT_USER, L"Software\\Vendor"), L"v", status) == 2);

    HKEY hKey = nullptr;
    CHECK(registry.OpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Missing", 0, KEY_READ, &hKey) == ERROR_FILE_NOT_FOUND);
    CHECK(registry.OpenKeyExW(HKEY_USERS, L".DEFAULT", 0, KEY_READ, &hKey) == ERROR_FILE_NOT_FOUND);
    CHECK(registry.OpenKeyExW(HKEY_USERS, nullptr, 0, KEY_READ, &hKey) == ERROR_FILE_NOT_FOUND);
    CHECK(registry.CreateKeyExW(HKEY_USERS, L"S-1", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr)
          == ERROR_FILE_NOT_FOUND);
    CHECK(registry.OpenKeyExW(nullptr, L"Software", 0, KEY_READ, &hKey) == ERROR_INVALID_HANDLE);
    CHECK(registry.CloseKey(HKEY_LOCAL_MACHINE) == ERROR_SUCCESS);
}

TEST_CASE("The longest mount prefix wins")
{
    MemoryHive outer;
    MemoryHive inner;
    VirtualRegistry registry;
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"Software", outer) == ERROR_SUCCESS);
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"Software\\Vendor\\App", inner) == ERROR_SUCCESS);

    const HKEY hInnerRoot = Hive::HandleOf(inner.Root());
    const HKEY hOuterRoot = Hive::HandleOf(outer.Root());

    CHECK(OpenKey(registry, HKEY_CURRENT_USER, L"Software\\Vendor\\App") == hInnerRoot);
    CHECK(OpenKey(registry, HKEY_CURRENT_USER, L"software") == hOuterRoot);

    const HKEY hDeep = CreateKey(registry, HKEY_CURRENT_USER, L"Software\\Vendor\\App\\Deep");
    CHECK(OpenKey(registry, hInnerRoot, L"Deep") == hDeep);

    HKEY hMissing = nullptr;
    CHECK(registry.OpenKeyExW(hOuterRoot, L"Deep", 0, KEY_READ, &hMissing) == ERROR_FILE_NOT_FOUND);

    const HKEY hSibling = CreateKey(registry, HKEY_CURRENT_USER, L"Software\\Vendor\\Other");
    CHECK(OpenKey(registry, hOuterRoot, L"Vendor\\Other") == hSibling);

    HKEY hKey = nullptr;
    CHECK(registry.OpenKeyExW(hInnerRoot, L"Other", 0, KEY_READ, &hKey) == ERROR_FILE_NOT_FOUND);
}

TEST_CASE("Calls outside the mounts reach the base registry")
{
    NativeRegistry native;
    VirtualRegistry registry(&native);
    MemoryHive hive;
    REQUIRE(registry.Mount(HKEY_CURRENT_USER, L"VRegTestMount", hive) == ERROR_SUCCESS);

    AppHive appHive;
    const HKEY hApp = appHive.Root();

    SECTION("A real handle gets the answers native gives")
    {
        HKEY hViaRegistry = nullptr;
        HKEY hViaNative = nullptr;
        DWORD dwDispositionRegistry = kFillSize;
        DWORD dwDispositionNative = kFillSize;
        REQUIRE(registry.CreateKeyExW(
                    hApp,
                    L"A\\B",
                    0,
                    nullptr,
                    0,
                    KEY_ALL_ACCESS,
                    nullptr,
                    &hViaRegistry,
                    &dwDispositionRegistry
                )
                == ERROR_SUCCESS);
        REQUIRE(native.CreateKeyExW(
                    hApp,
                    L"A\\B",
                    0,
                    nullptr,
                    0,
                    KEY_ALL_ACCESS,
                    nullptr,
                    &hViaNative,
                    &dwDispositionNative
                )
                == ERROR_SUCCESS);
        CHECK(dwDispositionRegistry == REG_CREATED_NEW_KEY);
        CHECK(dwDispositionNative == REG_OPENED_EXISTING_KEY);
        CHECK_FALSE(IsVirtualHKey(hViaRegistry));
        CHECK(hViaRegistry != hViaNative);

        REQUIRE(SetDword(registry, hViaRegistry, L"v", 42) == ERROR_SUCCESS);
        LSTATUS status = ERROR_SUCCESS;
        CHECK(QueryDword(native, hViaNative, L"v", status) == 42);
        CHECK(status == ERROR_SUCCESS);

        std::array<BYTE, 2> data{};
        DWORD cbViaRegistry = static_cast<DWORD>(data.size());
        DWORD cbViaNative = static_cast<DWORD>(data.size());
        const auto statusRegistry
            = registry.QueryValueExW(hViaRegistry, L"v", nullptr, nullptr, data.data(), &cbViaRegistry);
        const auto statusNative
            = native.QueryValueExW(hViaNative, L"v", nullptr, nullptr, data.data(), &cbViaNative);
        CHECK(statusRegistry == ERROR_MORE_DATA);
        CHECK(statusRegistry == statusNative);
        CHECK(cbViaRegistry == cbViaNative);

        HKEY hMissing = nullptr;
        CHECK(registry.OpenKeyExW(hApp, L"Missing", 0, KEY_READ, &hMissing)
              == native.OpenKeyExW(hApp, L"Missing", 0, KEY_READ, &hMissing));

        CHECK(registry.CloseKey(hViaRegistry) == ERROR_SUCCESS);
        CHECK(native.CloseKey(hViaNative) == ERROR_SUCCESS);
    }

    SECTION("Paths outside the mounts reach the real registry, those inside do not")
    {
        HKEY hReal = nullptr;
        CHECK(registry.OpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &hReal) == ERROR_SUCCESS);
        CHECK_FALSE(IsVirtualHKey(hReal));
        CHECK(registry.CloseKey(hReal) == ERROR_SUCCESS);

        HKEY hVirtual = nullptr;
        CHECK(
            registry.CreateKeyExW(
                HKEY_CURRENT_USER,
                L"VRegTestMount\\Key",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hVirtual,
                nullptr
            )
            == ERROR_SUCCESS
        );
        CHECK(IsVirtualHKey(hVirtual));
        CHECK(native.OpenKeyExW(HKEY_CURRENT_USER, L"VRegTestMount", 0, KEY_READ, &hReal) == ERROR_FILE_NOT_FOUND);
    }
}

TEST_CASE("A handle released with its hive is invalid")
{
    VirtualRegistry registry;
    HKEY hRoot = nullptr;
    HKEY hChild = nullptr;
    {
        MemoryHive hive;
        VirtualRegistry mounted;
        REQUIRE(mounted.Mount(HKEY_CURRENT_USER, L"Test", hive) == ERROR_SUCCESS);
        hRoot = Hive::HandleOf(hive.Root());
        hChild = CreateKey(mounted, hRoot, L"Child");
        CHECK(Hive::NodeOf(hChild) != nullptr);
    }

    CHECK(Hive::NodeOf(hRoot) == nullptr);
    CHECK(Hive::NodeOf(hChild) == nullptr);

    LSTATUS status = ERROR_SUCCESS;
    QueryDword(registry, hRoot, L"v", status);
    CHECK(status == ERROR_INVALID_HANDLE);
    QueryDword(registry, hChild, L"v", status);
    CHECK(status == ERROR_INVALID_HANDLE);
    CHECK(registry.CloseKey(hChild) == ERROR_INVALID_HANDLE);
}
