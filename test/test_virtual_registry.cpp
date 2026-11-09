#include "oracle.h"
#include "owned_values.h"
#include "scenario.h"

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <format>
#include <map>

using namespace VRegTest;

namespace
{
HKEY CreateKey(
    VReg* const registry,
    const HKEY hParent,
    const wchar_t* const pszPath
)
{
    HKEY hKey{};
    REQUIRE(
        VReg_RegCreateKeyExW(
            registry,
            hParent,
            pszPath,
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        ) == ERROR_SUCCESS
    );
    return hKey;
}

HKEY OpenKey(
    VReg* const registry,
    const HKEY hParent,
    const wchar_t* const pszPath
)
{
    HKEY hKey{};
    REQUIRE(
        VReg_RegOpenKeyExW(
            registry,
            hParent,
            pszPath,
            0,
            KEY_READ,
            &hKey
        ) == ERROR_SUCCESS
    );
    return hKey;
}

LSTATUS SetDword(
    VReg* const registry,
    const HKEY hKey,
    const wchar_t* const pszName,
    const DWORD dwValue
) noexcept
{
    return VReg_RegSetValueExW(
        registry,
        hKey,
        pszName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&dwValue),
        sizeof(dwValue)
    );
}

DWORD QueryDword(
    VReg* const registry,
    const HKEY hKey,
    const wchar_t* const pszName,
    LSTATUS& status
) noexcept
{
    DWORD dwValue = 0;
    DWORD cbValue = sizeof(dwValue);
    status = VReg_RegQueryValueExW(
        registry,
        hKey,
        pszName,
        nullptr,
        nullptr,
        reinterpret_cast<BYTE*>(&dwValue),
        &cbValue
    );
    return dwValue;
}
} // namespace

TEST_CASE("A node has one handle however it is reached")
{
    OwnedHive hive = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(VReg_Mount(registry, HKEY_CURRENT_USER, L"Test", hive, 0) == ERROR_SUCCESS);

    const HKEY hRoot = RootHandle(hive);
    CHECK(VReg_IsVirtualHKey(hRoot));

    const HKEY hCreated = CreateKey(registry, hRoot, L"A\\B");
    const HKEY hOpened = OpenKey(registry, hRoot, L"a\\b");
    const HKEY hViaParent = OpenKey(registry, OpenKey(registry, hRoot, L"A"), L"B");
    const HKEY hViaPath = OpenKey(registry, HKEY_CURRENT_USER, L"Test\\A\\B");

    CHECK(VReg_IsVirtualHKey(hCreated));
    CHECK(hOpened == hCreated);
    CHECK(hViaParent == hCreated);
    CHECK(hViaPath == hCreated);
    CHECK(OpenKey(registry, HKEY_CURRENT_USER, L"Test") == hRoot);
    CHECK(
        VReg_RegCloseKey(
            registry,
            hCreated
        ) == ERROR_SUCCESS
    );
    CHECK(OpenKey(registry, hRoot, L"A\\B") == hCreated);
}

TEST_CASE("A deleted node and its children answer ERROR_KEY_DELETED")
{
    OwnedHive hive = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(VReg_Mount(registry, HKEY_CURRENT_USER, L"Test", hive, 0) == ERROR_SUCCESS);
    const HKEY hRoot = RootHandle(hive);

    const HKEY hParent = CreateKey(registry, hRoot, L"Parent");
    const HKEY hChild = CreateKey(registry, hRoot, L"Parent\\Child");
    REQUIRE(SetDword(registry, hChild, L"v", 1) == ERROR_SUCCESS);

    REQUIRE(
        VReg_RegDeleteTreeW(
            registry,
            hRoot,
            L"Parent"
        ) == ERROR_SUCCESS
    );

    LSTATUS status = ERROR_SUCCESS;
    QueryDword(registry, hParent, L"v", status);
    CHECK(status == ERROR_KEY_DELETED);
    QueryDword(registry, hChild, L"v", status);
    CHECK(status == ERROR_KEY_DELETED);
    CHECK(SetDword(registry, hChild, L"v", 2) == ERROR_KEY_DELETED);
    CHECK(
        VReg_RegFlushKey(
            registry,
            hChild
        ) == ERROR_KEY_DELETED
    );

    HKEY hKey = nullptr;
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            hParent,
            L"Child",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_KEY_DELETED
    );
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            hRoot,
            L"Parent",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(
        VReg_RegCloseKey(
            registry,
            hChild
        ) == ERROR_SUCCESS
    );
}

TEST_CASE("A frozen hive refuses writes and still answers reads")
{
    OwnedHive hive = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(VReg_Mount(registry, HKEY_CURRENT_USER, L"Test", hive, 0) == ERROR_SUCCESS);
    const HKEY hRoot = RootHandle(hive);

    const HKEY hSub = CreateKey(registry, hRoot, L"Sub");
    REQUIRE(SetDword(registry, hSub, L"v", 7) == ERROR_SUCCESS);
    REQUIRE_FALSE(VRegHive_InternalIsFrozen(hive));

    VRegHive_Freeze(hive);
    CHECK(VRegHive_InternalIsFrozen(hive));

    LSTATUS status = ERROR_SUCCESS;
    CHECK(QueryDword(registry, hSub, L"v", status) == 7);
    CHECK(status == ERROR_SUCCESS);
    CHECK(OpenKey(registry, hRoot, L"SUB") == hSub);

    std::array<wchar_t, 8> name{};
    DWORD cchName = static_cast<DWORD>(name.size());
    CHECK(
        VReg_RegEnumKeyExW(
            registry,
            hRoot,
            0,
            name.data(),
            &cchName,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(std::wstring(name.data(), cchName) == L"Sub");

    HKEY hKey = nullptr;
    DWORD dwDisposition = 0;
    CHECK(
        VReg_RegCreateKeyExW(
            registry,
            hRoot,
            L"New",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            &dwDisposition
        ) == ERROR_ACCESS_DENIED
    );
    CHECK(SetDword(registry, hSub, L"v", 8) == ERROR_ACCESS_DENIED);
    CHECK(
        VReg_RegDeleteValueW(
            registry,
            hSub,
            L"v"
        ) == ERROR_ACCESS_DENIED
    );
    CHECK(
        VReg_RegDeleteKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            0
        ) == ERROR_ACCESS_DENIED
    );
    CHECK(
        VReg_RegDeleteTreeW(
            registry,
            hRoot,
            L"Sub"
        ) == ERROR_ACCESS_DENIED
    );
    CHECK(QueryDword(registry, hSub, L"v", status) == 7);
}

TEST_CASE("A hive created through the C functions freezes and is destroyed")
{
    VRegHive* pHive = nullptr;
    REQUIRE(VRegHive_CreateInMemory(&pHive) == ERROR_SUCCESS);

    CHECK_FALSE(VRegHive_InternalIsFrozen(pHive));
    VRegHive_Freeze(pHive);
    CHECK(VRegHive_InternalIsFrozen(pHive));

    VRegHive_Destroy(pHive);
    VRegHive_Destroy(nullptr);
}

TEST_CASE("Mount rejects what cannot be mounted")
{
    OwnedHive hive = MakeHive();
    OwnedHive other = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();

    CHECK(VReg_Mount(registry, nullptr, L"A", hive, 0) == ERROR_INVALID_HANDLE);
    CHECK(
        VReg_Mount(
            registry,
            RootHandle(hive),
            L"A",
            hive,
            0
        ) == ERROR_INVALID_HANDLE
    );
    CHECK(
        VReg_Mount(
            registry,
            HKEY_CURRENT_USER,
            L"\\Leading",
            hive,
            0
        ) == ERROR_BAD_PATHNAME
    );
    CHECK(
        VReg_Mount(
            registry,
            HKEY_CURRENT_USER,
            std::wstring(300, L'k').c_str(),
            hive,
            0
        ) == ERROR_BAD_PATHNAME
    );

    CHECK(
        VReg_Mount(
            registry,
            HKEY_CURRENT_USER,
            L"Software\\Test",
            hive,
            0
        ) == ERROR_SUCCESS
    );
    CHECK(
        VReg_Mount(
            registry,
            HKEY_CURRENT_USER,
            L"software\\TEST\\",
            other,
            0
        ) == ERROR_ALREADY_EXISTS
    );
    CHECK(
        VReg_Mount(
            registry,
            HKEY_LOCAL_MACHINE,
            L"Software\\Test",
            other,
            0
        ) == ERROR_SUCCESS
    );
}

TEST_CASE("Pure in-memory configuration")
{
    OwnedHive machine = MakeHive();
    OwnedHive user = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(VReg_Mount(registry, HKEY_LOCAL_MACHINE, L"", machine, 0) == ERROR_SUCCESS);
    REQUIRE(VReg_Mount(registry, HKEY_CURRENT_USER, L"", user, 0) == ERROR_SUCCESS);

    const HKEY hMachine = CreateKey(
        registry,
        HKEY_LOCAL_MACHINE,
        L"Software\\Vendor"
    );
    const HKEY hUser = CreateKey(
        registry,
        HKEY_CURRENT_USER,
        L"Software\\Vendor"
    );
    CHECK(hMachine != hUser);
    REQUIRE(SetDword(registry, hMachine, L"v", 1) == ERROR_SUCCESS);
    REQUIRE(SetDword(registry, hUser, L"v", 2) == ERROR_SUCCESS);

    LSTATUS status = ERROR_SUCCESS;
    const HKEY hMachineAgain = OpenKey(
        registry,
        HKEY_LOCAL_MACHINE,
        L"software\\vendor"
    );
    CHECK(QueryDword(registry, hMachineAgain, L"v", status) == 1);
    CHECK(
        QueryDword(
            registry,
            OpenKey(registry, HKEY_CURRENT_USER, L"Software\\Vendor"),
            L"v",
            status
        ) == 2
    );

    HKEY hKey = nullptr;
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            HKEY_LOCAL_MACHINE,
            L"Software\\Missing",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            HKEY_USERS,
            L".DEFAULT",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            HKEY_USERS,
            nullptr,
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(
        VReg_RegCreateKeyExW(
            registry,
            HKEY_USERS,
            L"S-1",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            nullptr,
            L"Software",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_INVALID_HANDLE
    );
    CHECK(
        VReg_RegCloseKey(
            registry,
            HKEY_LOCAL_MACHINE
        ) == ERROR_SUCCESS
    );
}

TEST_CASE("The longest mount prefix wins")
{
    OwnedHive outer = MakeHive();
    OwnedHive inner = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(
        VReg_Mount(registry, HKEY_CURRENT_USER, L"Software", outer, 0) == ERROR_SUCCESS
    );
    REQUIRE(
        VReg_Mount(
            registry,
            HKEY_CURRENT_USER,
            L"Software\\Vendor\\App",
            inner,
            0
        ) == ERROR_SUCCESS
    );

    const HKEY hInnerRoot = RootHandle(inner);
    const HKEY hOuterRoot = RootHandle(outer);

    CHECK(
        OpenKey(
            registry,
            HKEY_CURRENT_USER,
            L"Software\\Vendor\\App"
        ) == hInnerRoot
    );
    CHECK(OpenKey(registry, HKEY_CURRENT_USER, L"software") == hOuterRoot);

    const HKEY hDeep = CreateKey(
        registry,
        HKEY_CURRENT_USER,
        L"Software\\Vendor\\App\\Deep"
    );
    CHECK(OpenKey(registry, hInnerRoot, L"Deep") == hDeep);

    HKEY hMissing = nullptr;
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            hOuterRoot,
            L"Deep",
            0,
            KEY_READ,
            &hMissing
        ) == ERROR_FILE_NOT_FOUND
    );

    const HKEY hSibling = CreateKey(
        registry,
        HKEY_CURRENT_USER,
        L"Software\\Vendor\\Other"
    );
    CHECK(OpenKey(registry, hOuterRoot, L"Vendor\\Other") == hSibling);

    HKEY hKey = nullptr;
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            hInnerRoot,
            L"Other",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );
}

TEST_CASE("Calls outside the mounts reach the base registry")
{
    OwnedRegistry native = MakeNativeRegistry();
    OwnedRegistry registry = MakeVirtualRegistry(native);
    OwnedHive hive = MakeHive();
    REQUIRE(
        VReg_Mount(
            registry,
            HKEY_CURRENT_USER,
            L"VRegTestMount",
            hive,
            0
        ) == ERROR_SUCCESS
    );

    AppHive appHive;
    const HKEY hApp = appHive.Root();

    SECTION("A real handle gets the answers native gives")
    {
        HKEY hViaRegistry = nullptr;
        HKEY hViaNative = nullptr;
        DWORD dwDispositionRegistry = FILL_SIZE;
        DWORD dwDispositionNative = FILL_SIZE;
        REQUIRE(
            VReg_RegCreateKeyExW(
                registry,
                hApp,
                L"A\\B",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hViaRegistry,
                &dwDispositionRegistry
            ) == ERROR_SUCCESS
        );
        REQUIRE(
            VReg_RegCreateKeyExW(
                native,
                hApp,
                L"A\\B",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hViaNative,
                &dwDispositionNative
            ) == ERROR_SUCCESS
        );
        CHECK(dwDispositionRegistry == REG_CREATED_NEW_KEY);
        CHECK(dwDispositionNative == REG_OPENED_EXISTING_KEY);
        CHECK_FALSE(VReg_IsVirtualHKey(hViaRegistry));
        CHECK(hViaRegistry != hViaNative);

        REQUIRE(SetDword(registry, hViaRegistry, L"v", 42) == ERROR_SUCCESS);
        LSTATUS status = ERROR_SUCCESS;
        CHECK(QueryDword(native, hViaNative, L"v", status) == 42);
        CHECK(status == ERROR_SUCCESS);

        std::array<BYTE, 2> data{};
        DWORD cbViaRegistry = static_cast<DWORD>(data.size());
        DWORD cbViaNative = static_cast<DWORD>(data.size());
        const auto statusRegistry = VReg_RegQueryValueExW(
            registry,
            hViaRegistry,
            L"v",
            nullptr,
            nullptr,
            data.data(),
            &cbViaRegistry
        );
        const auto statusNative = VReg_RegQueryValueExW(
            native,
            hViaNative,
            L"v",
            nullptr,
            nullptr,
            data.data(),
            &cbViaNative
        );
        CHECK(statusRegistry == ERROR_MORE_DATA);
        CHECK(statusRegistry == statusNative);
        CHECK(cbViaRegistry == cbViaNative);

        HKEY hMissing = nullptr;
        CHECK(
            VReg_RegOpenKeyExW(
                registry,
                hApp,
                L"Missing",
                0,
                KEY_READ,
                &hMissing
            ) == VReg_RegOpenKeyExW(
                native,
                hApp,
                L"Missing",
                0,
                KEY_READ,
                &hMissing
            )
        );

        CHECK(
            VReg_RegCloseKey(
                registry,
                hViaRegistry
            ) == ERROR_SUCCESS
        );
        CHECK(
            VReg_RegCloseKey(
                native,
                hViaNative
            ) == ERROR_SUCCESS
        );
    }

    SECTION("Paths outside the mounts reach the real registry, those inside do not")
    {
        HKEY hReal = nullptr;
        CHECK(
            VReg_RegOpenKeyExW(
                registry,
                HKEY_CURRENT_USER,
                L"Software",
                0,
                KEY_READ,
                &hReal
            ) == ERROR_SUCCESS
        );
        CHECK_FALSE(VReg_IsVirtualHKey(hReal));
        CHECK(
            VReg_RegCloseKey(
                registry,
                hReal
            ) == ERROR_SUCCESS
        );

        HKEY hVirtual = nullptr;
        CHECK(
            VReg_RegCreateKeyExW(
                registry,
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
        CHECK(VReg_IsVirtualHKey(hVirtual));
        CHECK(
            VReg_RegOpenKeyExW(
                native,
                HKEY_CURRENT_USER,
                L"VRegTestMount",
                0,
                KEY_READ,
                &hReal
            ) == ERROR_FILE_NOT_FOUND
        );
    }
}

TEST_CASE("A handle released with its hive is invalid")
{
    OwnedRegistry registry = MakeVirtualRegistry();
    HKEY hRoot = nullptr;
    HKEY hChild = nullptr;
    {
        OwnedHive hive = MakeHive();
        OwnedRegistry mounted = MakeVirtualRegistry();
        REQUIRE(
            VReg_Mount(mounted, HKEY_CURRENT_USER, L"Test", hive, 0) == ERROR_SUCCESS
        );
        hRoot = RootHandle(hive);
        hChild = CreateKey(mounted, hRoot, L"Child");
        CHECK(VRegInternalNode_Of(hChild) != nullptr);
    }

    CHECK(VRegInternalNode_Of(hRoot) == nullptr);
    CHECK(VRegInternalNode_Of(hChild) == nullptr);

    LSTATUS status = ERROR_SUCCESS;
    QueryDword(registry, hRoot, L"v", status);
    CHECK(status == ERROR_INVALID_HANDLE);
    QueryDword(registry, hChild, L"v", status);
    CHECK(status == ERROR_INVALID_HANDLE);
    CHECK(
        VReg_RegCloseKey(
            registry,
            hChild
        ) == ERROR_INVALID_HANDLE
    );
}

namespace
{
constexpr bool IS_64BIT_PROCESS = sizeof(void*) == 8;

/// An `NtQueryKey` that knows a fixed set of fake handles by their native
/// paths, for a `VirtualRegistry` whose base answers `ERROR_INVALID_HANDLE`
/// to them.
struct FakeNtQueryKey
{
    static inline std::map<HKEY, std::wstring> s_paths;
    static inline int s_cCalls = 0;

    static LONG NTAPI Query(
        const HANDLE hKey,
        const ULONG keyInformationClass,
        const PVOID pKeyInformation,
        const ULONG cbKeyInformation,
        const PULONG pcbResult
    ) noexcept
    {
        ++s_cCalls;
        const auto it = s_paths.find(static_cast<HKEY>(hKey));
        if (keyInformationClass != 3 || it == s_paths.end())
            return static_cast<LONG>(0xC0000008);

        const ULONG cbName = static_cast<ULONG>(
            it->second.size() * sizeof(wchar_t)
        );
        *pcbResult = sizeof(ULONG) + cbName;
        if (cbKeyInformation < *pcbResult)
            return static_cast<LONG>(0x80000005);

        std::memcpy(pKeyInformation, &cbName, sizeof(cbName));
        std::memcpy(
            static_cast<BYTE*>(pKeyInformation) + sizeof(cbName),
            it->second.data(),
            cbName
        );
        return 0;
    }

    static void Reset() noexcept
    {
        s_paths.clear();
        s_cCalls = 0;
    }
};

HKEY FakeKey(const std::uintptr_t id, const std::wstring& nativePath)
{
    // Far above any handle value the process has, and even, so not virtual.
    const auto hKey = reinterpret_cast<HKEY>(0x70000000 + id * 4);
    FakeNtQueryKey::s_paths[hKey] = nativePath;
    return hKey;
}

LSTATUS OpenVia(
    VReg* const registry,
    const HKEY hParent,
    const wchar_t* const pszPath,
    const REGSAM sam,
    HKEY& hKey
) noexcept
{
    hKey = nullptr;
    return VReg_RegOpenKeyExW(
        registry,
        hParent,
        pszPath,
        0,
        sam,
        &hKey
    );
}
} // namespace

TEST_CASE("A real parent handle is resolved by its native path")
{
    FakeNtQueryKey::Reset();
    OwnedRegistry native = MakeNativeRegistry();
    OwnedRegistry registry = MakeVirtualRegistry(native, &FakeNtQueryKey::Query);
    OwnedHive hive = MakeHive();
    REQUIRE(
        VReg_Mount(
            registry,
            HKEY_LOCAL_MACHINE,
            L"Software\\VRegNative\\App",
            hive,
            0
        ) == ERROR_SUCCESS
    );
    const HKEY hSub = CreateKey(
        registry,
        HKEY_LOCAL_MACHINE,
        L"Software\\VRegNative\\App\\Sub"
    );
    const HKEY hRoot = RootHandle(hive);

    HKEY hKey = nullptr;

    SECTION("Opens land in the mount")
    {
        const HKEY hParent = FakeKey(
            1,
            L"\\REGISTRY\\MACHINE\\SOFTWARE\\VRegNative"
        );
        CHECK(OpenVia(registry, hParent, L"app", KEY_READ, hKey) == ERROR_SUCCESS);
        CHECK(hKey == hRoot);
        CHECK(
            OpenVia(
                registry,
                hParent,
                L"App\\SUB",
                KEY_READ,
                hKey
            ) == ERROR_SUCCESS
        );
        CHECK(hKey == hSub);
        CHECK(
            OpenVia(
                registry,
                hParent,
                L"App\\Missing",
                KEY_READ,
                hKey
            ) == ERROR_FILE_NOT_FOUND
        );

        const HKEY hMachine = FakeKey(2, L"\\REGISTRY\\MACHINE");
        CHECK(
            OpenVia(
                registry,
                hMachine,
                L"Software\\VRegNative\\App\\Sub",
                KEY_READ,
                hKey
            ) == ERROR_SUCCESS
        );
        CHECK(hKey == hSub);

        HKEY hCreated = nullptr;
        DWORD dwDisposition = 0;
        CHECK(
            VReg_RegCreateKeyExW(
                registry,
                hParent,
                L"App\\New",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hCreated,
                &dwDisposition
            )
            == ERROR_SUCCESS
        );
        CHECK(VReg_IsVirtualHKey(hCreated));
        CHECK(dwDisposition == REG_CREATED_NEW_KEY);
        CHECK(
            VReg_RegDeleteKeyExW(
                registry,
                hParent,
                L"App\\New",
                0,
                0
            ) == ERROR_SUCCESS
        );
    }

    SECTION("The gate skips the query unless a component names a mount point")
    {
        const HKEY hParent = FakeKey(
            1,
            L"\\REGISTRY\\MACHINE\\SOFTWARE\\VRegNative"
        );
        FakeNtQueryKey::s_cCalls = 0;
        CHECK(
            OpenVia(
                registry,
                hParent,
                L"Other\\Deeper",
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );
        CHECK(
            OpenVia(
                registry,
                hParent,
                nullptr,
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );
        CHECK(FakeNtQueryKey::s_cCalls == 0);

        CHECK(
            OpenVia(
                registry,
                hParent,
                L"Other\\APP",
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );
        CHECK(FakeNtQueryKey::s_cCalls == 1);
    }

    SECTION("A failing query, or a path outside every mount, reaches the base")
    {
        CHECK(
            OpenVia(
                registry,
                reinterpret_cast<HKEY>(0x70009900),
                L"App",
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );
        const HKEY hOther = FakeKey(
            3,
            L"\\REGISTRY\\MACHINE\\SOFTWARE\\Elsewhere"
        );
        CHECK(
            OpenVia(
                registry,
                hOther,
                L"App",
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );
        const HKEY hUser = FakeKey(
            4,
            L"\\REGISTRY\\USER\\S-1-5-18\\SOFTWARE\\VRegNative"
        );
        CHECK(
            OpenVia(
                registry,
                hUser,
                L"App",
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );
    }

    SECTION("A native path longer than the first buffer is read in full")
    {
        const std::wstring prefix =
            L"\\REGISTRY\\MACHINE\\SOFTWARE\\VRegNative\\App\\";
        const HKEY hLong = FakeKey(5, prefix + std::wstring(200, L'x'));
        CHECK(
            OpenVia(
                registry,
                hLong,
                L"App",
                KEY_READ,
                hKey
            ) == ERROR_FILE_NOT_FOUND
        );
        CHECK(
            OpenVia(
                registry,
                hLong,
                L"Sub",
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );

        FakeNtQueryKey::s_paths[hLong] = prefix + std::wstring(1000, L'x');
        CHECK(
            OpenVia(
                registry,
                hLong,
                L"App",
                KEY_READ,
                hKey
            ) == ERROR_FILE_NOT_FOUND
        );
    }

    SECTION("Roots without a native path are not reachable")
    {
        OwnedHive classes = MakeHive();
        REQUIRE(
            VReg_Mount(
                registry,
                HKEY_CLASSES_ROOT,
                L"VRegNativeClass",
                classes,
                0
            ) == ERROR_SUCCESS
        );
        const HKEY hParent = FakeKey(
            7,
            L"\\REGISTRY\\MACHINE\\SOFTWARE\\Classes"
        );
        CHECK(
            OpenVia(
                registry,
                hParent,
                L"VRegNativeClass",
                KEY_READ,
                hKey
            ) == ERROR_INVALID_HANDLE
        );
        CHECK(
            OpenVia(
                registry,
                HKEY_CLASSES_ROOT,
                L"VRegNativeClass",
                KEY_READ,
                hKey
            ) == ERROR_SUCCESS
        );
    }
}

TEST_CASE("WOW6432Node components are stripped whole and ordinal case-insensitively")
{
    FakeNtQueryKey::Reset();
    OwnedRegistry native = MakeNativeRegistry();
    OwnedRegistry registry = MakeVirtualRegistry(native, &FakeNtQueryKey::Query);
    OwnedHive hive = MakeHive();
    REQUIRE(
        VReg_Mount(
            registry,
            HKEY_LOCAL_MACHINE,
            L"Software\\VRegNative\\App",
            hive,
            0
        ) == ERROR_SUCCESS
    );
    CreateKey(
        registry,
        HKEY_LOCAL_MACHINE,
        L"Software\\VRegNative\\App\\Wow6432Node"
    );

    HKEY hKey = nullptr;
    const HKEY hNode = FakeKey(
        1,
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\WOW6432NODE\\VRegNative"
    );
    CHECK(OpenVia(registry, hNode, L"App", KEY_READ, hKey) == ERROR_SUCCESS);
    CHECK(hKey == RootHandle(hive));

    const HKEY hSoftware = FakeKey(2, L"\\REGISTRY\\MACHINE\\SOFTWARE");
    CHECK(
        OpenVia(
            registry,
            hSoftware,
            L"wow6432node\\VRegNative\\App",
            KEY_READ,
            hKey
        ) == ERROR_SUCCESS
    );

    const HKEY hSimilar = FakeKey(
        3,
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\XWOW6432NODE\\VRegNative"
    );
    CHECK(
        OpenVia(
            registry,
            hSimilar,
            L"App",
            KEY_READ,
            hKey
        ) == ERROR_INVALID_HANDLE
    );

    // Below the mount point the name is an ordinary key.
    CHECK(
        OpenVia(
            registry,
            hSoftware,
            L"VRegNative\\App\\Wow6432Node",
            KEY_READ,
            hKey
        ) == ERROR_SUCCESS
    );
    CHECK(
        OpenVia(
            registry,
            hSoftware,
            L"VRegNative\\App\\Missing\\Wow6432Node",
            KEY_READ,
            hKey
        ) == ERROR_FILE_NOT_FOUND
    );
}

TEST_CASE("A mount answers only the calls of its view")
{
    FakeNtQueryKey::Reset();
    OwnedRegistry native = MakeNativeRegistry();
    OwnedRegistry registry = MakeVirtualRegistry(native, &FakeNtQueryKey::Query);
    OwnedHive hive64 = MakeHive();
    OwnedHive hive32 = MakeHive();
    OwnedHive hiveBoth = MakeHive();
    REQUIRE(
        VReg_Mount(
            registry,
            HKEY_LOCAL_MACHINE,
            L"Software\\VRegViewTest64",
            hive64,
            KEY_WOW64_64KEY
        ) == ERROR_SUCCESS
    );
    REQUIRE(
        VReg_Mount(
            registry,
            HKEY_LOCAL_MACHINE,
            L"Software\\VRegViewTest32",
            hive32,
            KEY_WOW64_32KEY
        ) == ERROR_SUCCESS
    );
    REQUIRE(
        VReg_Mount(
            registry,
            HKEY_LOCAL_MACHINE,
            L"Software\\VRegViewTestBoth",
            hiveBoth,
            0
        ) == ERROR_SUCCESS
    );

    const REGSAM sam64 = KEY_READ | KEY_WOW64_64KEY;
    const REGSAM sam32 = KEY_READ | KEY_WOW64_32KEY;
    const HKEY hSoftware = FakeKey(1, L"\\REGISTRY\\MACHINE\\SOFTWARE");
    const HKEY hSoftware32 = FakeKey(
        2,
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Wow6432Node"
    );

    HKEY hKey = nullptr;
    const auto isVirtual = [&](
        const HKEY hParent,
        const wchar_t* const pszPath,
        const REGSAM sam
    )
    {
        const auto status = OpenVia(registry, hParent, pszPath, sam, hKey);
        return status == ERROR_SUCCESS && VReg_IsVirtualHKey(hKey);
    };

    SECTION("Through a real parent")
    {
        CHECK(isVirtual(hSoftware, L"VRegViewTest64", sam64));
        CHECK_FALSE(isVirtual(hSoftware, L"VRegViewTest64", sam32));
        CHECK(isVirtual(hSoftware, L"VRegViewTest32", sam32));
        CHECK_FALSE(isVirtual(hSoftware, L"VRegViewTest32", sam64));
        CHECK(isVirtual(hSoftware, L"VRegViewTestBoth", sam32));
        CHECK(isVirtual(hSoftware, L"VRegViewTestBoth", sam64));

        CHECK(isVirtual(hSoftware, L"VRegViewTest64", KEY_READ) == IS_64BIT_PROCESS);
        CHECK(isVirtual(hSoftware, L"VRegViewTest32", KEY_READ) == !IS_64BIT_PROCESS);
    }

    SECTION("A path that contained WOW6432Node is the 32-bit view")
    {
        CHECK(isVirtual(hSoftware32, L"VRegViewTest32", sam64));
        CHECK(isVirtual(hSoftware32, L"VRegViewTest32", KEY_READ));
        CHECK_FALSE(isVirtual(hSoftware32, L"VRegViewTest64", sam64));
        CHECK_FALSE(isVirtual(hSoftware32, L"VRegViewTest64", sam32));
        CHECK(isVirtual(hSoftware32, L"VRegViewTestBoth", sam64));
    }

    SECTION("Through a predefined root")
    {
        CHECK(isVirtual(HKEY_LOCAL_MACHINE, L"Software\\VRegViewTest64", sam64));
        CHECK(
            OpenVia(
                registry,
                HKEY_LOCAL_MACHINE,
                L"Software\\VRegViewTest64",
                sam32,
                hKey
            ) == ERROR_FILE_NOT_FOUND
        );
        CHECK(isVirtual(HKEY_LOCAL_MACHINE, L"Software\\VRegViewTest32", sam32));
        CHECK(
            OpenVia(
                registry,
                HKEY_LOCAL_MACHINE,
                L"Software\\VRegViewTest32",
                sam64,
                hKey
            ) == ERROR_FILE_NOT_FOUND
        );
        CHECK(isVirtual(HKEY_LOCAL_MACHINE, L"Software\\VRegViewTestBoth", sam32));
        CHECK(isVirtual(HKEY_LOCAL_MACHINE, L"Software\\VRegViewTestBoth", sam64));
    }
}

TEST_CASE("A real key above a mount reaches it through the real NtQueryKey")
{
    OwnedRegistry native = MakeNativeRegistry();
    OwnedRegistry registry = MakeVirtualRegistry(native);
    OwnedHive hive = MakeHive();
    const auto name = std::format(
        L"VRegNativePathTest-{}",
        GetCurrentProcessId()
    );
    REQUIRE(VReg_Mount(registry, HKEY_CURRENT_USER, name.c_str(), hive, 0) == ERROR_SUCCESS);
    const HKEY hSub = CreateKey(
        registry,
        HKEY_CURRENT_USER,
        (name + L"\\x").c_str()
    );
    REQUIRE(VReg_IsVirtualHKey(hSub));

    HKEY hCurrentUser = nullptr;
    REQUIRE(RegOpenCurrentUser(KEY_READ, &hCurrentUser) == ERROR_SUCCESS);
    REQUIRE_FALSE(VReg_IsVirtualHKey(hCurrentUser));

    HKEY hKey = nullptr;
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            hCurrentUser,
            (name + L"\\x").c_str(),
            0,
            KEY_READ,
            &hKey
        ) == ERROR_SUCCESS
    );
    CHECK(hKey == hSub);
    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            hCurrentUser,
            name.c_str(),
            0,
            KEY_READ,
            &hKey
        ) == ERROR_SUCCESS
    );
    CHECK(hKey == RootHandle(hive));

    CHECK(
        VReg_RegOpenKeyExW(
            registry,
            hCurrentUser,
            L"Software",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_SUCCESS
    );
    CHECK_FALSE(VReg_IsVirtualHKey(hKey));
    CHECK(
        VReg_RegCloseKey(
            registry,
            hKey
        ) == ERROR_SUCCESS
    );

    RegCloseKey(hCurrentUser);
}
