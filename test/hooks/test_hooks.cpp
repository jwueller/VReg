// Every case installs hooks (or loads modules that do), which never detach, so
// each must run in a process of its own: ctest does that, a bare run of this
// executable does not.

#include "../oracle.h"
#include "hook_mount.h"

#include <VReg/handle.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

using namespace VReg;
using namespace VReg::Test;

namespace
{
std::wstring MountName(const wchar_t* const pszTag)
{
    return std::format(L"VRegHook-{}-{}", GetCurrentProcessId(), pszTag);
}

/// Whether a real key named @p name exists below the real `HKEY_CURRENT_USER`.
/// Asked through ntdll, which nothing hooks: through the hooked `Reg*`
/// functions, native-path resolution rightly lands on the mount.
bool RealKeyExists(const std::wstring& name)
{
    struct UnicodeString
    {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    };
    struct ObjectAttributes
    {
        ULONG Length;
        HANDLE RootDirectory;
        UnicodeString* ObjectName;
        ULONG Attributes;
        PVOID SecurityDescriptor;
        PVOID SecurityQualityOfService;
    };
    using PfnNtOpenKey = LONG(NTAPI*)(PHANDLE, ACCESS_MASK, ObjectAttributes*);
    using PfnNtClose = LONG(NTAPI*)(HANDLE);
    const auto hNtdll = GetModuleHandleW(L"ntdll.dll");
    const auto pfnNtOpenKey = reinterpret_cast<PfnNtOpenKey>(
        GetProcAddress(hNtdll, "NtOpenKey")
    );
    const auto pfnNtClose = reinterpret_cast<PfnNtClose>(
        GetProcAddress(hNtdll, "NtClose")
    );
    REQUIRE(pfnNtOpenKey);
    REQUIRE(pfnNtClose);

    HKEY hUser{};
    REQUIRE(
        ::RegOpenCurrentUser(
            KEY_READ,
            &hUser
        ) == ERROR_SUCCESS
    );
    std::wstring buffer = name;
    const auto cbName = static_cast<USHORT>(buffer.size() * sizeof(wchar_t));
    UnicodeString objectName{cbName, cbName, buffer.data()};
    constexpr ULONG kObjCaseInsensitive = 0x40;
    ObjectAttributes attributes{sizeof(ObjectAttributes), hUser, &objectName, kObjCaseInsensitive, nullptr, nullptr};
    HANDLE hKey{};
    const auto status = pfnNtOpenKey(&hKey, KEY_READ, &attributes);
    if (status >= 0)
        pfnNtClose(hKey);
    ::RegCloseKey(
        hUser
    );
    return status >= 0;
}

/// The module value of the key @p hKey, or nothing with @p status set.
std::optional<std::wstring> ReadModule(const HKEY hKey, LSTATUS& status)
{
    std::array<wchar_t, 16> buffer{};
    DWORD cbBuffer = sizeof(buffer);
    status = ::RegGetValueW(
        hKey,
        nullptr,
        kModuleValue,
        RRF_RT_REG_SZ,
        nullptr,
        buffer.data(),
        &cbBuffer
    );
    if (status != ERROR_SUCCESS)
        return std::nullopt;
    return std::wstring(buffer.data());
}

/// The module value of the mount at `HKEY_CURRENT_USER\name`, or nothing when
/// the open or the read fails.
std::optional<std::wstring> ReadMountModule(const std::wstring& name)
{
    HKEY hKey{};
    const auto opened = ::RegOpenKeyExW(
        HKEY_CURRENT_USER,
        name.c_str(),
        0,
        KEY_READ,
        &hKey
    );
    if (opened != ERROR_SUCCESS)
        return std::nullopt;
    LSTATUS status{};
    auto module = ReadModule(hKey, status);
    ::RegCloseKey(
        hKey
    );
    return module;
}

HKEY OpenMount(const std::wstring& name)
{
    HKEY hKey{};
    REQUIRE(
        ::RegOpenKeyExW(
            HKEY_CURRENT_USER,
            name.c_str(),
            0,
            KEY_ALL_ACCESS,
            &hKey
        ) == ERROR_SUCCESS
    );
    return hKey;
}

std::filesystem::path ExeDirectory()
{
    std::array<wchar_t, MAX_PATH> path{};
    REQUIRE(
        ::GetModuleFileNameW(
            nullptr,
            path.data(),
            static_cast<DWORD>(path.size())
        ) < path.size()
    );
    return std::filesystem::path(path.data()).parent_path();
}

/// A hook module next to this executable, with the name of its mount.
struct Module
{
    HMODULE hModule{};
    std::wstring name;
};

/// Loads VReg_HookModule<tag>.dll and has it install hooks over a mount of its
/// own.
Module LoadModule(const wchar_t* const pszTag)
{
    Module module;
    module.name = MountName(pszTag);

    const auto path = ExeDirectory() / std::format(L"VReg_HookModule{}.dll", pszTag);
    module.hModule = ::LoadLibraryW(path.c_str());
    REQUIRE(module.hModule != nullptr);

    using InstallFunction = LONG(__cdecl*)(const wchar_t*);
    const auto pfnInstall = reinterpret_cast<InstallFunction>(
        ::GetProcAddress(module.hModule, "HookModuleInstall")
    );
    REQUIRE(pfnInstall != nullptr);
    REQUIRE(pfnInstall(module.name.c_str()) == NO_ERROR);
    return module;
}

void CheckRealKeysWork()
{
    HKEY hKey{};
    REQUIRE(
        ::RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
            0,
            KEY_READ,
            &hKey
        )
        == ERROR_SUCCESS
    );
    CHECK(!IsVirtualHKey(hKey));
    DWORD cValues = 0;
    CHECK(
        ::RegQueryInfoKeyW(
            hKey,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &cValues,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        )
        == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hKey
        ) == ERROR_SUCCESS
    );
}

/// What two installed modules must satisfy while both are loaded.
void CheckBothMounts(
    const Module& lower,
    const Module& upper,
    const wchar_t* const pszLower,
    const wchar_t* const pszUpper
)
{
    CHECK(ReadMountModule(lower.name) == pszLower);
    CHECK(ReadMountModule(upper.name) == pszUpper);

    const HKEY hLower = OpenMount(lower.name);
    const HKEY hUpper = OpenMount(upper.name);
    CHECK(IsVirtualHKey(hLower));
    CHECK(IsVirtualHKey(hUpper));
    CHECK(hLower != hUpper);

    // A handle of the lower module is answered by it through the upper one's
    // hooks, which do not own it.
    LSTATUS status{};
    CHECK(ReadModule(hLower, status) == pszLower);
    CHECK(ReadModule(hUpper, status) == pszUpper);

    HKEY hSub{};
    CHECK(
        ::RegCreateKeyExW(
            hLower,
            L"Sub",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hSub,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hSub
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegOpenKeyExW(
            hUpper,
            L"Sub",
            0,
            KEY_READ,
            &hSub
        ) == ERROR_FILE_NOT_FOUND
    );

    CHECK(
        ::RegCloseKey(
            hLower
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hUpper
        ) == ERROR_SUCCESS
    );
    CheckRealKeysWork();
}
} // namespace

TEST_CASE("Hooked Reg functions answer on virtual keys")
{
    const auto name = MountName(L"Exe");
    REQUIRE(InstallMount(name.c_str(), L"Exe") == NO_ERROR);

    const HKEY hRoot = OpenMount(name);
    CHECK(IsVirtualHKey(hRoot));
    LSTATUS status{};
    CHECK(ReadModule(hRoot, status) == L"Exe");

    HKEY hByA{};
    const auto nameA = std::format("VRegHook-{}-Exe", GetCurrentProcessId());
    CHECK(
        ::RegOpenKeyExA(
            HKEY_CURRENT_USER,
            nameA.c_str(),
            0,
            KEY_READ,
            &hByA
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hByA
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegOpenKeyA(
            HKEY_CURRENT_USER,
            nameA.c_str(),
            &hByA
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hByA
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegOpenKeyW(
            HKEY_CURRENT_USER,
            name.c_str(),
            &hByA
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hByA
        ) == ERROR_SUCCESS
    );

    HKEY hDeep{};
    DWORD dwDisposition = 0;
    REQUIRE(
        ::RegCreateKeyExW(
            hRoot,
            L"Sub\\Deep",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hDeep,
            &dwDisposition
        )
        == ERROR_SUCCESS
    );
    CHECK(IsVirtualHKey(hDeep));
    CHECK(dwDisposition == REG_CREATED_NEW_KEY);

    HKEY hOther{};
    CHECK(
        ::RegCreateKeyExA(
            hRoot,
            "SubA",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hOther,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hOther
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCreateKeyW(
            hRoot,
            L"SubB",
            &hOther
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hOther
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCreateKeyA(
            hRoot,
            "SubC",
            &hOther
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hOther
        ) == ERROR_SUCCESS
    );

    const DWORD dwValue = 0x1234;
    CHECK(
        ::RegSetValueExW(
            hDeep,
            L"Number",
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&dwValue),
            sizeof(dwValue)
        )
        == ERROR_SUCCESS
    );
    DWORD dwType = 0;
    DWORD dwRead = 0;
    DWORD cbRead = sizeof(dwRead);
    CHECK(
        ::RegQueryValueExW(
            hDeep,
            L"Number",
            nullptr,
            &dwType,
            reinterpret_cast<BYTE*>(&dwRead),
            &cbRead
        )
        == ERROR_SUCCESS
    );
    CHECK(dwType == REG_DWORD);
    CHECK(dwRead == dwValue);

    CHECK(
        ::RegSetValueExA(
            hDeep,
            "Text",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>("hello"),
            6
        ) == ERROR_SUCCESS
    );
    std::array<char, 16> narrow{};
    cbRead = sizeof(narrow);
    CHECK(
        ::RegQueryValueExA(
            hDeep,
            "Text",
            nullptr,
            &dwType,
            reinterpret_cast<BYTE*>(narrow.data()),
            &cbRead
        )
        == ERROR_SUCCESS
    );
    CHECK(std::string(narrow.data()) == "hello");
    CHECK(cbRead == 6);
    std::array<wchar_t, 16> wide{};
    cbRead = sizeof(wide);
    CHECK(
        ::RegQueryValueExW(
            hDeep,
            L"Text",
            nullptr,
            &dwType,
            reinterpret_cast<BYTE*>(wide.data()),
            &cbRead
        )
        == ERROR_SUCCESS
    );
    CHECK(std::wstring(wide.data()) == L"hello");
    CHECK(cbRead == 12);

    CHECK(
        ::RegSetValueW(
            hRoot,
            L"SubB",
            REG_SZ,
            L"unnamed",
            0
        ) == ERROR_SUCCESS
    );
    LONG cbUnnamed = sizeof(wide);
    wide.fill(0);
    CHECK(
        ::RegQueryValueW(
            hRoot,
            L"SubB",
            wide.data(),
            &cbUnnamed
        ) == ERROR_SUCCESS
    );
    CHECK(std::wstring(wide.data()) == L"unnamed");
    CHECK(
        ::RegSetValueA(
            hRoot,
            "SubC",
            REG_SZ,
            "narrow",
            0
        ) == ERROR_SUCCESS
    );
    LONG cbNarrow = sizeof(narrow);
    narrow.fill(0);
    CHECK(
        ::RegQueryValueA(
            hRoot,
            "SubC",
            narrow.data(),
            &cbNarrow
        ) == ERROR_SUCCESS
    );
    CHECK(std::string(narrow.data()) == "narrow");

    wide.fill(0);
    cbRead = sizeof(wide);
    CHECK(
        ::RegGetValueW(
            hDeep,
            nullptr,
            L"Text",
            RRF_RT_REG_SZ,
            &dwType,
            wide.data(),
            &cbRead
        ) == ERROR_SUCCESS
    );
    CHECK(std::wstring(wide.data()) == L"hello");
    cbRead = sizeof(dwRead);
    CHECK(
        ::RegGetValueW(
            hDeep,
            nullptr,
            L"Text",
            RRF_RT_REG_DWORD,
            nullptr,
            &dwRead,
            &cbRead
        ) == ERROR_UNSUPPORTED_TYPE
    );

    DWORD cSubKeys = 0;
    DWORD cValues = 0;
    CHECK(
        ::RegQueryInfoKeyW(
            hRoot,
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
        )
        == ERROR_SUCCESS
    );
    CHECK(cSubKeys == 4);
    CHECK(cValues == 1);

    std::vector<std::wstring> names;
    for (DWORD i = 0;; ++i)
    {
        std::array<wchar_t, 32> buffer{};
        DWORD cchBuffer = static_cast<DWORD>(buffer.size());
        const auto statusEnum =
            ::RegEnumKeyExW(
                hRoot,
                i,
                buffer.data(),
                &cchBuffer,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
        if (statusEnum == ERROR_NO_MORE_ITEMS)
            break;
        REQUIRE(statusEnum == ERROR_SUCCESS);
        names.emplace_back(buffer.data(), cchBuffer);
    }
    CHECK(names.size() == 4);
    CHECK(std::ranges::count(names, L"Sub") == 1);

    std::array<wchar_t, 32> valueName{};
    DWORD cchValueName = static_cast<DWORD>(valueName.size());
    CHECK(
        ::RegEnumValueW(
            hRoot,
            0,
            valueName.data(),
            &cchValueName,
            nullptr,
            &dwType,
            nullptr,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(std::wstring(valueName.data()) == kModuleValue);
    cchValueName = static_cast<DWORD>(valueName.size());
    CHECK(
        ::RegEnumValueW(
            hRoot,
            1,
            valueName.data(),
            &cchValueName,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        )
        == ERROR_NO_MORE_ITEMS
    );

    CHECK(
        ::RegDeleteValueW(
            hDeep,
            L"Number"
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegDeleteValueA(
            hDeep,
            "Number"
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(
        ::RegCloseKey(
            hDeep
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegDeleteKeyW(
            hRoot,
            L"SubC"
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegDeleteKeyExW(
            hRoot,
            L"SubB",
            0,
            0
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegDeleteTreeW(
            hRoot,
            L"Sub"
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegDeleteTreeA(
            hRoot,
            "SubA"
        ) == ERROR_SUCCESS
    );

    HKEY hGone{};
    CHECK(
        ::RegOpenKeyExW(
            hRoot,
            L"Sub",
            0,
            KEY_READ,
            &hGone
        ) == ERROR_FILE_NOT_FOUND
    );

    CHECK(
        ::RegFlushKey(
            hRoot
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hRoot
        ) == ERROR_SUCCESS
    );
    CHECK(!RealKeyExists(name));
}

TEST_CASE("Hooked Reg functions pass real keys through")
{
    const auto name = MountName(L"Exe");
    REQUIRE(InstallMount(name.c_str(), L"Exe") == NO_ERROR);
    AppHive appHive;
    const HKEY hRoot = appHive.Root();

    HKEY hKey{};
    DWORD dwDisposition = 0;
    REQUIRE(
        ::RegCreateKeyExW(
            hRoot,
            L"Real\\Child",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            &dwDisposition
        )
        == ERROR_SUCCESS
    );
    CHECK(!IsVirtualHKey(hKey));
    CHECK(dwDisposition == REG_CREATED_NEW_KEY);

    const DWORD dwValue = 7;
    CHECK(
        ::RegSetValueExW(
            hKey,
            L"Number",
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&dwValue),
            sizeof(dwValue)
        )
        == ERROR_SUCCESS
    );
    CHECK(
        ::RegSetValueExA(
            hKey,
            "Text",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>("real"),
            5
        ) == ERROR_SUCCESS
    );

    DWORD dwRead = 0;
    DWORD cbRead = sizeof(dwRead);
    CHECK(
        ::RegGetValueW(
            hKey,
            nullptr,
            L"Number",
            RRF_RT_REG_DWORD,
            nullptr,
            &dwRead,
            &cbRead
        ) == ERROR_SUCCESS
    );
    CHECK(dwRead == dwValue);
    CHECK(
        ::RegQueryValueExW(
            hKey,
            L"Missing",
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ) == ERROR_FILE_NOT_FOUND
    );

    DWORD cValues = 0;
    CHECK(
        ::RegQueryInfoKeyW(
            hKey,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &cValues,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        )
        == ERROR_SUCCESS
    );
    CHECK(cValues == 2);

    std::array<wchar_t, 32> buffer{};
    DWORD cchBuffer = static_cast<DWORD>(buffer.size());
    CHECK(
        ::RegEnumKeyExW(
            hRoot,
            0,
            buffer.data(),
            &cchBuffer,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(std::wstring(buffer.data()) == L"Real");
    cchBuffer = static_cast<DWORD>(buffer.size());
    CHECK(
        ::RegEnumValueW(
            hKey,
            0,
            buffer.data(),
            &cchBuffer,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ) == ERROR_SUCCESS
    );

    CHECK(
        ::RegCloseKey(
            hKey
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegDeleteTreeW(
            hRoot,
            L"Real"
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegOpenKeyExW(
            hRoot,
            L"Real",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );
    CHECK(
        ::RegCloseKey(
            hKey
        ) == ERROR_INVALID_HANDLE
    );

    CheckRealKeysWork();
    CHECK(!RealKeyExists(name));
}

TEST_CASE("Refused functions answer not supported on virtual keys and pass real ones through")
{
    const auto name = MountName(L"Exe");
    REQUIRE(InstallMount(name.c_str(), L"Exe") == NO_ERROR);
    const HKEY hVirtual = OpenMount(name);

    CHECK(
        ::RegSaveKeyW(
            hVirtual,
            L"unused.hiv",
            nullptr
        ) == ERROR_NOT_SUPPORTED
    );
    CHECK(
        ::RegSaveKeyExA(
            hVirtual,
            "unused.hiv",
            nullptr,
            REG_LATEST_FORMAT
        ) == ERROR_NOT_SUPPORTED
    );
    CHECK(
        ::RegRenameKey(
            hVirtual,
            L"Old",
            L"New"
        ) == ERROR_NOT_SUPPORTED
    );
    CHECK(
        ::RegCopyTreeW(
            hVirtual,
            nullptr,
            HKEY_CURRENT_USER
        ) == ERROR_NOT_SUPPORTED
    );
    CHECK(
        ::RegCopyTreeW(
            HKEY_CURRENT_USER,
            nullptr,
            hVirtual
        ) == ERROR_NOT_SUPPORTED
    );
    DWORD cbSecurity = 0;
    CHECK(
        ::RegGetKeySecurity(
            hVirtual,
            OWNER_SECURITY_INFORMATION,
            nullptr,
            &cbSecurity
        ) == ERROR_NOT_SUPPORTED
    );

    AppHive appHive;
    const HKEY hRoot = appHive.Root();
    HKEY hKey{};
    REQUIRE(
        ::RegCreateKeyExW(
            hRoot,
            L"Old",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hKey
        ) == ERROR_SUCCESS
    );

    CHECK(
        ::RegRenameKey(
            hRoot,
            L"Old",
            L"New"
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegOpenKeyExW(
            hRoot,
            L"New",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegCloseKey(
            hKey
        ) == ERROR_SUCCESS
    );
    CHECK(
        ::RegOpenKeyExW(
            hRoot,
            L"Old",
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );

    CHECK(
        ::RegGetKeySecurity(
            hRoot,
            OWNER_SECURITY_INFORMATION,
            nullptr,
            &cbSecurity
        ) == ERROR_INSUFFICIENT_BUFFER
    );
    CHECK(cbSecurity != 0);
    CHECK(
        ::RegSaveKeyW(
            hRoot,
            L"unused.hiv",
            nullptr
        ) != ERROR_NOT_SUPPORTED
    );

    CHECK(
        ::RegCloseKey(
            hVirtual
        ) == ERROR_SUCCESS
    );
    CHECK(!RealKeyExists(name));
}

TEST_CASE("RegLoadMUIStringA answers not implemented on virtual keys")
{
    const auto name = MountName(L"Exe");
    REQUIRE(InstallMount(name.c_str(), L"Exe") == NO_ERROR);
    const HKEY hVirtual = OpenMount(name);

    std::array<char, 16> buffer{};
    DWORD cbData = 0;
    CHECK(
        ::RegLoadMUIStringA(
            hVirtual,
            "Anything",
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &cbData,
            0,
            nullptr
        )
        == ERROR_CALL_NOT_IMPLEMENTED
    );

    CHECK(
        ::RegCloseKey(
            hVirtual
        ) == ERROR_SUCCESS
    );
    CHECK(!RealKeyExists(name));
}

TEST_CASE("A second Attach answers already initialized")
{
    const auto name = MountName(L"Exe");
    REQUIRE(InstallMount(name.c_str(), L"Exe") == NO_ERROR);

    NativeRegistry native;
    VirtualRegistry registry(&native);
    CHECK(Detours::Attach(registry, native) == ERROR_ALREADY_INITIALIZED);
    CHECK(Detours::Install(registry, native) == ERROR_ALREADY_INITIALIZED);
    CHECK(ReadMountModule(name) == L"Exe");
    CHECK(!RealKeyExists(name));
}

TEST_CASE("The hooks call through the registry handed to Install")
{
    const auto name = MountName(L"Exe");
    InterceptRegistry* pIntercept = nullptr;
    REQUIRE(InstallMount(name.c_str(), L"Exe", &pIntercept) == NO_ERROR);

    std::vector<std::string> calls;
    pIntercept->onCall = [&](const std::string& call) { calls.push_back(call); };
    CHECK(ReadMountModule(name) == L"Exe");

    pIntercept->failures["RegGetValueW"] = ERROR_ACCESS_DENIED;
    const HKEY hRoot = OpenMount(name);
    LSTATUS status{};
    CHECK(!ReadModule(hRoot, status));
    CHECK(status == ERROR_ACCESS_DENIED);

    pIntercept->failures.clear();
    std::array<char, 16> module{};
    DWORD cbModule = sizeof(module);
    CHECK(
        ::RegGetValueA(
            hRoot,
            nullptr,
            "Module",
            RRF_RT_REG_SZ,
            nullptr,
            module.data(),
            &cbModule
        ) == ERROR_SUCCESS
    );
    CHECK(std::string(module.data()) == "Exe");

    pIntercept->onCall = nullptr;
    CHECK(std::ranges::count(calls, "RegOpenKeyExW") == 2);
    CHECK(std::ranges::count(calls, "RegGetValueW") == 2);
    CHECK(std::ranges::count(calls, "RegGetValueA") == 1);
}

TEST_CASE("Neutralize removes the mount and leaves real keys working")
{
    const auto name = MountName(L"Exe");
    REQUIRE(InstallMount(name.c_str(), L"Exe") == NO_ERROR);
    REQUIRE(ReadMountModule(name) == L"Exe");

    Detours::Neutralize();

    HKEY hKey{};
    CHECK(
        ::RegOpenKeyExW(
            HKEY_CURRENT_USER,
            name.c_str(),
            0,
            KEY_READ,
            &hKey
        ) == ERROR_FILE_NOT_FOUND
    );
    CheckRealKeysWork();

    AppHive appHive;
    REQUIRE(
        ::RegCreateKeyExW(
            appHive.Root(),
            L"Real",
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
    CHECK(
        ::RegCloseKey(
            hKey
        ) == ERROR_SUCCESS
    );
    CHECK(!RealKeyExists(name));
}

TEST_CASE("Two modules, the lower one unloaded first")
{
    const auto lower = LoadModule(L"A");
    const auto upper = LoadModule(L"B");
    CheckBothMounts(lower, upper, L"A", L"B");

    REQUIRE(::FreeLibrary(lower.hModule));
    CHECK(ReadMountModule(upper.name) == L"B");
    CHECK(ReadMountModule(lower.name) == std::nullopt);
    CheckRealKeysWork();

    REQUIRE(::FreeLibrary(upper.hModule));
    CHECK(ReadMountModule(upper.name) == std::nullopt);
    CheckRealKeysWork();
    CHECK(!RealKeyExists(lower.name));
    CHECK(!RealKeyExists(upper.name));
}

TEST_CASE("Two modules, the upper one unloaded first")
{
    const auto lower = LoadModule(L"B");
    const auto upper = LoadModule(L"A");
    CheckBothMounts(lower, upper, L"B", L"A");

    REQUIRE(::FreeLibrary(upper.hModule));
    CHECK(ReadMountModule(lower.name) == L"B");
    CHECK(ReadMountModule(upper.name) == std::nullopt);
    CheckRealKeysWork();

    REQUIRE(::FreeLibrary(lower.hModule));
    CHECK(ReadMountModule(lower.name) == std::nullopt);
    CheckRealKeysWork();
    CHECK(!RealKeyExists(lower.name));
    CHECK(!RealKeyExists(upper.name));
}
