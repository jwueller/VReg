// The handle encoding assumes no real key handle ever has bit 0 set. This
// opens keys through every route that might tag a handle and checks that
// assumption against the running system (and against Wine in CI).

#include <VReg/handle.h>

#include <catch2/catch_test_macros.hpp>

#include <Windows.h>
#include <ktmw32.h>

#include <filesystem>
#include <string>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ktmw32.lib")

namespace
{
/// Checks the handle a successful route produced and closes it. A route that
/// fails on this system (no transactions, no remote registry) proves nothing
/// either way and is skipped.
void CheckRoute(const char* route, const LSTATUS error, const HKEY hKey)
{
    INFO(route);
    if (error != ERROR_SUCCESS)
    {
        WARN(route << " unavailable: error " << error);
        return;
    }

    CHECK((std::bit_cast<std::uintptr_t>(hKey) & 1) == 0);
    CHECK_FALSE(VReg::IsVirtualHKey(hKey));
    RegCloseKey(hKey);
}

HKEY Open(const HKEY hkParent, const wchar_t* lpszName, const REGSAM sam, LSTATUS& error)
{
    HKEY hKey{};
    error = RegOpenKeyExW(hkParent, lpszName, 0, sam, &hKey);
    return hKey;
}
} // namespace

TEST_CASE("No route to a real key handle sets bit 0")
{
    LSTATUS e{};
    HKEY h{};

    h = Open(HKEY_LOCAL_MACHINE, L"SOFTWARE", KEY_READ, e);
    CheckRoute("HKLM\\SOFTWARE", e, h);
    h = Open(HKEY_LOCAL_MACHINE, L"SOFTWARE", KEY_READ | KEY_WOW64_32KEY, e);
    CheckRoute("HKLM\\SOFTWARE (WOW64_32)", e, h);
    h = Open(HKEY_LOCAL_MACHINE, L"SOFTWARE", KEY_READ | KEY_WOW64_64KEY, e);
    CheckRoute("HKLM\\SOFTWARE (WOW64_64)", e, h);
    h = Open(HKEY_CLASSES_ROOT, L"CLSID", KEY_READ, e);
    CheckRoute("HKCR\\CLSID", e, h);
    h = Open(HKEY_CLASSES_ROOT, L"*", KEY_READ, e);
    CheckRoute("HKCR\\*", e, h);
    h = Open(HKEY_CURRENT_USER, L"Software", KEY_READ, e);
    CheckRoute("HKCU\\Software", e, h);
    h = Open(HKEY_USERS, L".DEFAULT", KEY_READ, e);
    CheckRoute("HKU\\.DEFAULT", e, h);
    h = Open(HKEY_CURRENT_CONFIG, L"Software", KEY_READ, e);
    CheckRoute("HKCC\\Software", e, h);

    e = RegOpenCurrentUser(KEY_READ, &h);
    CheckRoute("RegOpenCurrentUser", e, h);

    HANDLE hToken{};
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_DUPLICATE, &hToken))
    {
        e = RegOpenUserClassesRoot(hToken, 0, KEY_READ, &h);
        CheckRoute("RegOpenUserClassesRoot", e, h);
        CloseHandle(hToken);
    }

    e = RegConnectRegistryW(nullptr, HKEY_LOCAL_MACHINE, &h);
    CheckRoute("RegConnectRegistry (local)", e, h);

    if (const auto hTransaction = CreateTransaction(nullptr, nullptr, 0, 0, 0, 0, nullptr);
        hTransaction != INVALID_HANDLE_VALUE)
    {
        e = RegOpenKeyTransactedW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &h, hTransaction, nullptr);
        CheckRoute("RegOpenKeyTransacted", e, h);
        RollbackTransaction(hTransaction);
        CloseHandle(hTransaction);
    }

    const auto hivePath = std::filesystem::temp_directory_path()
        / (L"VReg-probe-" + std::to_wstring(GetCurrentProcessId()) + L".hiv");
    HKEY hHive{};
    e = RegLoadAppKeyW(hivePath.c_str(), &hHive, KEY_ALL_ACCESS, 0, 0);
    if (e == ERROR_SUCCESS)
    {
        e = RegCreateKeyExW(hHive, L"Child", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &h, nullptr);
        CheckRoute("app hive child", e, h);
        CheckRoute("RegLoadAppKey", ERROR_SUCCESS, hHive);
    }
    else
    {
        CheckRoute("RegLoadAppKey", e, hHive);
    }

    // The hive unloads once its last handle closes; its log files go with it.
    std::error_code ignored;
    for (const auto* suffix : {L"", L".LOG1", L".LOG2"})
        std::filesystem::remove(hivePath.native() + suffix, ignored);

    for (int i = 0; i < 2000; ++i)
    {
        h = Open(HKEY_LOCAL_MACHINE, L"SOFTWARE", KEY_READ, e);
        REQUIRE(e == ERROR_SUCCESS);
        REQUIRE((std::bit_cast<std::uintptr_t>(h) & 1) == 0);
        RegCloseKey(h);
    }
}
