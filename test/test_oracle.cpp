// The scenario catalogue for keys. Every scenario runs against the real
// Win32 API and against each `Registry` implementation, each on a fresh app
// hive, and the recordings must match. Value, enumeration and lookup
// scenarios live in the sibling test_oracle_*.cpp files; scenario.h says how
// to write one.

#include "forwarding_hive.h"
#include "scenario.h"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <tuple>

using namespace VRegTest;

namespace
{
struct CreateOpenAndClose
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        DWORD dwDisposition = FILL_SIZE;
        RECORD(
            "create new",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"Key",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {dwDisposition}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create existing",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"key",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {dwDisposition}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"KEY",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        hKey = FillKey();
        RECORD(
            "open missing",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"Missing",
                0,
                KEY_READ,
                &hKey
            ),
            {KeyState(hKey)}
        );
    }
};

struct QueryStringBufferTooSmall
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        constexpr wchar_t VALUE_TEXT[] = L"value";
        RECORD(
            "set",
            VReg_SetValueExW(
                api,
                hRoot,
                L"Text",
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(VALUE_TEXT),
                sizeof(VALUE_TEXT)
            )
        );

        std::array<BYTE, 6> data;
        data.fill(FILL);
        DWORD dwType = FILL_SIZE;
        DWORD cbData = static_cast<DWORD>(data.size());
        RECORD(
            "query too small",
            VReg_QueryValueExW(
                api,
                hRoot,
                L"Text",
                nullptr,
                &dwType,
                data.data(),
                &cbData
            ),
            {
                dwType,
                cbData,
            },
            {Bytes(data)}
        );
    }
};

struct CreateDispositionAndClass
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        DWORD dwDisposition = FILL_SIZE;
        wchar_t szClass[] = L"MyClass";
        RECORD(
            "create with class",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"WithClass",
                0,
                szClass,
                REG_OPTION_NON_VOLATILE,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {
                dwDisposition,
                KeyState(hKey),
            }
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        wchar_t szOther[] = L"OtherClass";
        RECORD(
            "create existing other class",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"WithClass",
                0,
                szOther,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {dwDisposition}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"WithClass",
                0,
                KEY_READ,
                &hKey
            )
        );
        for (const DWORD cchClass : {0u, 3u, 7u, 8u, 64u})
        {
            std::array<wchar_t, 64> cls;
            std::memset(cls.data(), FILL, sizeof(cls));
            DWORD cch = cchClass;
            RECORD(
                "class",
                VReg_QueryInfoKeyW(
                    api,
                    hKey,
                    cls.data(),
                    &cch,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                ),
                {cch},
                {Bytes(cls)}
            );
        }
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create volatile",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"Volatile",
                0,
                nullptr,
                REG_OPTION_VOLATILE,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {dwDisposition}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "create no disposition",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"NoDisposition",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "create no result",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"NoResult",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                nullptr,
                nullptr
            )
        );
        dwDisposition = FILL_SIZE;
        RECORD(
            "create no result with disposition",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"NoResult2",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                nullptr,
                &dwDisposition
            ),
            {dwDisposition}
        );

        dwDisposition = FILL_SIZE;
        hKey = FillKey();
        RECORD(
            "create nested",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"A\\B\\C",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {
                dwDisposition,
                KeyState(hKey)
            }
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        dwDisposition = FILL_SIZE;
        RECORD(
            "create nested partly existing",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"A\\B\\D",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {dwDisposition}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        for (
            const wchar_t* const pszSub :
            {L"", L"\\Leading", L"Trailing\\", L"Double\\\\Slash", L"\\"}
        )
        {
            dwDisposition = FILL_SIZE;
            hKey = FillKey();
            const auto status = VReg_CreateKeyExW(
                api,
                hRoot,
                pszSub,
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            );
            RECORD(
                "create odd path " + Narrow(pszSub),
                status,
                {
                    dwDisposition,
                    KeyState(hKey),
                    HandedBack(api, hKey, hRoot)
                }
            );
            if (status == ERROR_SUCCESS && !HandedBack(api, hKey, hRoot))
                RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hKey
                )
            );
        }

        hKey = FillKey();
        RECORD(
            "create invalid handle",
            VReg_CreateKeyExW(
                api,
                nullptr,
                L"X",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create reserved",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"R",
                5,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create unknown option",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"O",
                0,
                nullptr,
                0x100,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create long name",
            VReg_CreateKeyExW(
                api,
                hRoot,
                std::wstring(300, L'k').c_str(),
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ),
            {KeyState(hKey)}
        );
        dwDisposition = FILL_SIZE;
        RECORD(
            "create case differs",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"withclass",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {dwDisposition}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
    }
};

struct CreateOpenAnsi
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        DWORD dwDisposition = FILL_SIZE;
        char szClass[] = "Cl\xe4ss\x80";
        RECORD(
            "create A",
            VReg_CreateKeyExA(
                api,
                hRoot,
                "K\xe4y\x80",
                0,
                szClass,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {
                dwDisposition,
                KeyState(hKey),
            }
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open W of A-created",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"K\u00e4y\u20ac",
                0,
                KEY_READ,
                &hKey
            )
        );
        std::array<wchar_t, 16> cls;
        std::memset(cls.data(), FILL, sizeof(cls));
        DWORD cch = 16;
        RECORD(
            "class as W",
            VReg_QueryInfoKeyW(
                api,
                hKey,
                cls.data(),
                &cch,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            ),
            {cch},
            {Bytes(cls)}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create A existing",
            VReg_CreateKeyExA(
                api,
                hRoot,
                "k\xe4y\x80",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                &dwDisposition
            ),
            {dwDisposition}
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open A",
            VReg_OpenKeyExA(
                api,
                hRoot,
                "K\xe4y\x80",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        hKey = FillKey();
        RECORD(
            "open A missing",
            VReg_OpenKeyExA(
                api,
                hRoot,
                "Missing",
                0,
                KEY_READ,
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open A unrepresentable",
            VReg_OpenKeyExA(
                api,
                hRoot,
                "K?y?",
                0,
                KEY_READ,
                &hKey
            )
        );

        RECORD(
            "create W unrepresentable",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"U\u4e2d",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open A as ?",
            VReg_OpenKeyExA(
                api,
                hRoot,
                "U?",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "open W",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"U\u4e2d",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "create W best fit",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"b\u0101",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open A best fit",
            VReg_OpenKeyExA(
                api,
                hRoot,
                "ba",
                0,
                KEY_READ,
                &hKey
            )
        );

        for (
            const char* const pszSub : {
                static_cast<const char*>(nullptr),
                "",
                "\\Leading",
                "Trailing\\",
            }
        )
        {
            hKey = FillKey();
            const auto status = VReg_OpenKeyExA(
                api,
                hRoot,
                pszSub,
                0,
                KEY_READ,
                &hKey
            );
            RECORD(
                std::string("open A odd ") + (pszSub ? pszSub : "(null)"),
                status,
                {
                    KeyState(hKey),
                    HandedBack(api, hKey, hRoot),
                }
            );
            if (status == ERROR_SUCCESS && !HandedBack(api, hKey, hRoot))
                RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hKey
                )
            );
        }

        hKey = FillKey();
        RECORD(
            "open A no result",
            VReg_OpenKeyExA(
                api,
                hRoot,
                "K\xe4y\x80",
                0,
                KEY_READ,
                nullptr
            )
        );
        RECORD(
            "create A no result",
            VReg_CreateKeyExA(
                api,
                hRoot,
                "K\xe4y\x80",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                nullptr,
                nullptr
            )
        );
        RECORD(
            "open A invalid handle",
            VReg_OpenKeyExA(
                api,
                nullptr,
                "K",
                0,
                KEY_READ,
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A invalid handle",
            VReg_CreateKeyExA(
                api,
                nullptr,
                "K",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A reserved",
            VReg_CreateKeyExA(
                api,
                hRoot,
                "R",
                5,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A long name",
            VReg_CreateKeyExA(
                api,
                hRoot,
                std::string(300, 'k').c_str(),
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hKey,
                nullptr
            ),
            {KeyState(hKey)}
        );
    }
};

struct OpenKeyForms
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        RECORD(
            "create",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"A\\B",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                nullptr
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        for (
            const wchar_t* const pszSub : {
                static_cast<const wchar_t*>(nullptr),
                L"",
                L"A",
                L"a\\b",
                L"A\\",
                L"\\A",
                L"A\\\\B",
                L"A\\Missing",
                L"Missing\\B"
            }
        )
        {
            hKey = FillKey();
            const auto status = VReg_OpenKeyExW(
                api,
                hRoot,
                pszSub,
                0,
                KEY_READ,
                &hKey
            );
            RECORD(
                std::string("open ") + (pszSub ? Narrow(pszSub) : "(null)"),
                status,
                {
                    KeyState(hKey),
                    HandedBack(api, hKey, hRoot),
                }
            );
            if (status == ERROR_SUCCESS && !HandedBack(api, hKey, hRoot))
                RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hKey
                )
            );
        }

        RECORD(
            "open no access",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"A",
                0,
                0,
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open link option",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"A",
                REG_OPTION_OPEN_LINK,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        // Native needs SeBackupPrivilege; a virtual key has no access check to
        // bypass and always opens.
        if (!api.isVirtualKeys)
        {
            hKey = FillKey();
            const auto backupStatus = VReg_OpenKeyExW(
                api,
                hRoot,
                L"A",
                REG_OPTION_BACKUP_RESTORE,
                KEY_READ,
                &hKey
            );
            RECORD("open backup option", backupStatus, {KeyState(hKey)});
            if (backupStatus == ERROR_SUCCESS)
                RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hKey
                )
            );
        }
        hKey = FillKey();
        RECORD(
            "open unknown option",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"A",
                0x100,
                KEY_READ,
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open wow64 64",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"A",
                0,
                KEY_READ | KEY_WOW64_64KEY,
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open wow64 32",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"A",
                0,
                KEY_READ | KEY_WOW64_32KEY,
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open no result",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"A",
                0,
                KEY_READ,
                nullptr
            )
        );
        hKey = FillKey();
        RECORD(
            "open invalid handle",
            VReg_OpenKeyExW(
                api,
                nullptr,
                L"A",
                0,
                KEY_READ,
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open long name",
            VReg_OpenKeyExW(
                api,
                hRoot,
                std::wstring(300, L'k').c_str(),
                0,
                KEY_READ,
                &hKey
            ),
            {KeyState(hKey)}
        );

        // A virtual key has one handle per node, so it carries no per-open
        // access rights.
        if (!api.isVirtualKeys)
        {
            HKEY hRead{};
            RECORD(
                "open read",
                VReg_OpenKeyExW(
                    api,
                    hRoot,
                    L"A",
                    0,
                    KEY_READ,
                    &hRead
                )
            );
            const BYTE data[4] = {1, 2, 3, 4};
            RECORD(
                "set on read key",
                VReg_SetValueExW(
                    api,
                    hRead,
                    L"v",
                    0,
                    REG_BINARY,
                    data,
                    4
                )
            );
            hKey = FillKey();
            RECORD(
                "create on read key",
                VReg_CreateKeyExW(
                    api,
                    hRead,
                    L"New",
                    0,
                    nullptr,
                    0,
                    KEY_READ,
                    nullptr,
                    &hKey,
                    nullptr
                ),
                {KeyState(hKey)}
            );
            RECORD(
                "delete on read key",
                VReg_DeleteKeyExW(
                    api,
                    hRead,
                    L"B",
                    0,
                    0
                )
            );
            RECORD(
                "delete value on read key",
                VReg_DeleteValueW(
                    api,
                    hRead,
                    L"v"
                )
            );
            RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hRead
                )
            );
        }
        RECORD(
            "close null",
            VReg_CloseKey(
                api,
                nullptr
            )
        );
    }
};

struct PreExOpenAndCreate
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        RECORD(
            "create W",
            VReg_CreateKeyW(
                api,
                hRoot,
                L"A\\B",
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        for (
            const wchar_t* const pszSub : {
                static_cast<const wchar_t*>(nullptr),
                L"",
                L"A",
                L"a\\b",
                L"A\\",
                L"\\A",
                L"Missing",
                L"A\\\\B"
            }
        )
        {
            const auto label = std::string(pszSub ? Narrow(pszSub) : "(null)");
            hKey = FillKey();
            auto status = VReg_OpenKeyW(
                api,
                hRoot,
                pszSub,
                &hKey
            );
            RECORD("open W " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open W " + label, hKey, hRoot);

            hKey = FillKey();
            status = VReg_CreateKeyW(
                api,
                hRoot,
                pszSub,
                &hKey
            );
            RECORD(
                "create W " + label,
                status,
                {
                    KeyState(hKey),
                    hKey == hRoot,
                }
            );
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(
                    api,
                    record,
                    "create W " + label,
                    hKey,
                    hRoot
                );

            const auto narrow = pszSub ? Narrow(pszSub) : std::string();
            const char* const pszNarrow = pszSub ? narrow.c_str() : nullptr;
            hKey = FillKey();
            status = VReg_OpenKeyA(
                api,
                hRoot,
                pszNarrow,
                &hKey
            );
            RECORD("open A " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open A " + label, hKey, hRoot);

            hKey = FillKey();
            status = VReg_CreateKeyA(
                api,
                hRoot,
                pszNarrow,
                &hKey
            );
            RECORD(
                "create A " + label,
                status,
                {
                    KeyState(hKey),
                    hKey == hRoot,
                }
            );
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(
                    api,
                    record,
                    "create A " + label,
                    hKey,
                    hRoot
                );
        }

        RECORD(
            "open W no result",
            VReg_OpenKeyW(
                api,
                hRoot,
                L"A",
                nullptr
            )
        );
        RECORD(
            "open W empty no result",
            VReg_OpenKeyW(
                api,
                hRoot,
                L"",
                nullptr
            )
        );
        RECORD(
            "open A no result",
            VReg_OpenKeyA(
                api,
                hRoot,
                "A",
                nullptr
            )
        );
        RECORD(
            "open A empty no result",
            VReg_OpenKeyA(
                api,
                hRoot,
                "",
                nullptr
            )
        );
        RECORD(
            "create W no result",
            VReg_CreateKeyW(
                api,
                hRoot,
                L"A",
                nullptr
            )
        );
        RECORD(
            "create W empty no result",
            VReg_CreateKeyW(
                api,
                hRoot,
                L"",
                nullptr
            )
        );
        RECORD(
            "create A no result",
            VReg_CreateKeyA(
                api,
                hRoot,
                "A",
                nullptr
            )
        );
        RECORD(
            "create A empty no result",
            VReg_CreateKeyA(
                api,
                hRoot,
                "",
                nullptr
            )
        );

        hKey = FillKey();
        RECORD(
            "open W invalid handle",
            VReg_OpenKeyW(
                api,
                nullptr,
                L"A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open W empty invalid handle",
            VReg_OpenKeyW(
                api,
                nullptr,
                L"",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open A empty invalid handle",
            VReg_OpenKeyA(
                api,
                nullptr,
                "",
                &hKey
            ),
            {KeyState(hKey)}
        );
        hKey = FillKey();
        RECORD(
            "create W invalid handle",
            VReg_CreateKeyW(
                api,
                nullptr,
                L"A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create W empty invalid handle",
            VReg_CreateKeyW(
                api,
                nullptr,
                L"",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A invalid handle",
            VReg_CreateKeyA(
                api,
                nullptr,
                "A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A empty invalid handle",
            VReg_CreateKeyA(
                api,
                nullptr,
                "",
                &hKey
            ),
            {KeyState(hKey)}
        );

        RECORD(
            "create A non-ASCII",
            VReg_CreateKeyA(
                api,
                hRoot,
                "N\xe4\x80",
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open W non-ASCII",
            VReg_OpenKeyW(
                api,
                hRoot,
                L"N\u00e4\u20ac",
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open A non-ASCII",
            VReg_OpenKeyA(
                api,
                hRoot,
                "n\xe4\x80",
                &hKey
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
        hKey = FillKey();
        RECORD(
            "open A missing",
            VReg_OpenKeyA(
                api,
                hRoot,
                "Missing",
                &hKey
            ),
            {KeyState(hKey)}
        );
    }
};

struct DeleteKeys
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        for (
            const wchar_t* const pszName : {
                L"Parent\\Child",
                L"Leaf1",
                L"Leaf2",
                L"Leaf3",
                L"Leaf4\u00e4",
                L"Leaf5"
            }
        )
        {
            RECORD(
                "create",
                VReg_CreateKeyExW(
                    api,
                    hRoot,
                    pszName,
                    0,
                    nullptr,
                    0,
                    KEY_ALL_ACCESS,
                    nullptr,
                    &hKey,
                    nullptr
                )
            );
            RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hKey
                )
            );
        }

        RECORD(
            "delete missing Ex",
            VReg_DeleteKeyExW(
                api,
                hRoot,
                L"Missing",
                0,
                0
            )
        );
        RECORD(
            "delete missing W",
            VReg_DeleteKeyW(
                api,
                hRoot,
                L"Missing"
            )
        );
        RECORD(
            "delete missing A",
            VReg_DeleteKeyA(
                api,
                hRoot,
                "Missing"
            )
        );
        RECORD(
            "delete missing ExA",
            VReg_DeleteKeyExA(
                api,
                hRoot,
                "Missing",
                0,
                0
            )
        );
        RECORD(
            "delete with children Ex",
            VReg_DeleteKeyExW(
                api,
                hRoot,
                L"Parent",
                0,
                0
            )
        );
        RECORD(
            "delete with children W",
            VReg_DeleteKeyW(
                api,
                hRoot,
                L"Parent"
            )
        );
        RECORD(
            "delete with children A",
            VReg_DeleteKeyA(
                api,
                hRoot,
                "Parent"
            )
        );
        RECORD(
            "delete with children ExA",
            VReg_DeleteKeyExA(
                api,
                hRoot,
                "Parent",
                0,
                0
            )
        );
        RECORD(
            "delete reserved",
            VReg_DeleteKeyExW(
                api,
                hRoot,
                L"Leaf1",
                0,
                5
            )
        );
        RECORD(
            "delete reserved A",
            VReg_DeleteKeyExA(
                api,
                hRoot,
                "Leaf1",
                0,
                5
            )
        );
        RECORD(
            "delete wow64 flags",
            VReg_DeleteKeyExW(
                api,
                hRoot,
                L"Leaf1",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete wow64 flags again",
            VReg_DeleteKeyExW(
                api,
                hRoot,
                L"Leaf1",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete wow64 flags A",
            VReg_DeleteKeyExA(
                api,
                hRoot,
                "Leaf2",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete other flags",
            VReg_DeleteKeyExW(
                api,
                hRoot,
                L"Leaf3",
                KEY_READ,
                0
            )
        );
        RECORD(
            "delete invalid handle",
            VReg_DeleteKeyExW(
                api,
                nullptr,
                L"Leaf4\u00e4",
                0,
                0
            )
        );
        RECORD(
            "delete invalid handle A",
            VReg_DeleteKeyA(
                api,
                nullptr,
                "Leaf4\xe4"
            )
        );
        RECORD(
            "delete nested",
            VReg_DeleteKeyW(
                api,
                hRoot,
                L"Parent\\Child"
            )
        );
        RECORD(
            "delete nested again",
            VReg_DeleteKeyW(
                api,
                hRoot,
                L"Parent\\Child"
            )
        );
        RECORD(
            "delete parent",
            VReg_DeleteKeyA(
                api,
                hRoot,
                "parent"
            )
        );
        RECORD(
            "delete A non-ASCII",
            VReg_DeleteKeyA(
                api,
                hRoot,
                "Leaf4\xe4"
            )
        );
        RECORD(
            "delete backslash",
            VReg_DeleteKeyW(
                api,
                hRoot,
                L"\\"
            )
        );
        RECORD(
            "delete long",
            VReg_DeleteKeyW(
                api,
                hRoot,
                std::wstring(300, L'k').c_str()
            )
        );
        RECORD(
            "delete trailing slash",
            VReg_DeleteKeyW(
                api,
                hRoot,
                L"Leaf5\\"
            )
        );

        for (
            const auto* const pszSub : {
                L"Parent",
                L"Leaf1",
                L"Leaf2",
                L"Leaf3",
                L"Leaf4\u00e4",
                L"Leaf5"
            }
        )
        {
            hKey = FillKey();
            const auto status = VReg_OpenKeyExW(
                api,
                hRoot,
                pszSub,
                0,
                KEY_READ,
                &hKey
            );
            RECORD("open " + Narrow(pszSub), status);
            if (status == ERROR_SUCCESS)
                RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hKey
                )
            );
        }
    }
};

struct OperateOnDeletedKey
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        const BYTE data[4] = {1, 2, 3, 4};
        RECORD(
            "create",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"Doomed",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                nullptr
            )
        );
        RECORD(
            "set",
            VReg_SetValueExW(
                api,
                hKey,
                L"v",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "delete",
            VReg_DeleteKeyExW(
                api,
                hRoot,
                L"Doomed",
                0,
                0
            )
        );

        DWORD cbData = 4;
        std::array<BYTE, 4> out;
        out.fill(FILL);
        RECORD(
            "query",
            VReg_QueryValueExW(
                api,
                hKey,
                L"v",
                nullptr,
                nullptr,
                out.data(),
                &cbData
            ),
            {cbData},
            {Bytes(out)}
        );
        cbData = 4;
        RECORD(
            "query A",
            VReg_QueryValueExA(
                api,
                hKey,
                "v",
                nullptr,
                nullptr,
                out.data(),
                &cbData
            ),
            {cbData},
            {Bytes(out)}
        );
        RECORD(
            "set after",
            VReg_SetValueExW(
                api,
                hKey,
                L"w",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "set after A",
            VReg_SetValueExA(
                api,
                hKey,
                "w",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "delete value after",
            VReg_DeleteValueW(
                api,
                hKey,
                L"v"
            )
        );
        HKEY hChild = FillKey();
        RECORD(
            "create child after",
            VReg_CreateKeyExW(
                api,
                hKey,
                L"Child",
                0,
                nullptr,
                0,
                KEY_READ,
                nullptr,
                &hChild,
                nullptr
            ),
            {KeyState(hChild)}
        );
        hChild = FillKey();
        RECORD(
            "open child after",
            VReg_OpenKeyExW(
                api,
                hKey,
                L"Child",
                0,
                KEY_READ,
                &hChild
            ),
            {KeyState(hChild)}
        );
        std::array<wchar_t, 8> name;
        DWORD cchName = 8;
        RECORD(
            "enum key after",
            VReg_EnumKeyExW(
                api,
                hKey,
                0,
                name.data(),
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            )
        );
        cchName = 8;
        RECORD(
            "enum value after",
            VReg_EnumValueW(
                api,
                hKey,
                0,
                name.data(),
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            )
        );
        RECORD(
            "query info after",
            VReg_QueryInfoKeyW(
                api,
                hKey,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            )
        );
        RECORD(
            "flush after",
            VReg_FlushKey(
                api,
                hKey
            )
        );
        RECORD(
            "delete tree after",
            VReg_DeleteTreeW(
                api,
                hKey,
                nullptr
            )
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
    }
};

struct DeleteTrees
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const BYTE data[4] = {1, 2, 3, 4};
        for (
            const wchar_t* const pszSub : {
                L"T\\A\\B\\C",
                L"T\\A",
                L"T",
                L"T\\S",
                L"U\u00e4\\X",
                L"V\\X",
                L"W\\X",
                L"X\\X"
            }
        )
            RECORD(
            "set " + Narrow(pszSub),
            VReg_SetKeyValueW(
                api,
                hRoot,
                pszSub,
                L"v",
                REG_BINARY,
                data,
                4
            )
        );

        RECORD(
            "tree missing",
            VReg_DeleteTreeW(
                api,
                hRoot,
                L"Missing"
            )
        );
        RECORD(
            "tree missing A",
            VReg_DeleteTreeA(
                api,
                hRoot,
                "Missing"
            )
        );
        RECORD(
            "tree nested",
            VReg_DeleteTreeW(
                api,
                hRoot,
                L"T\\A\\B"
            )
        );
        RECORD(
            "tree nested again",
            VReg_DeleteTreeW(
                api,
                hRoot,
                L"T\\A\\B"
            )
        );
        RECORD(
            "tree A",
            VReg_DeleteTreeA(
                api,
                hRoot,
                "u\xe4"
            )
        );
        RECORD(
            "tree A again",
            VReg_DeleteTreeA(
                api,
                hRoot,
                "u\xe4"
            )
        );
        RECORD(
            "tree invalid handle",
            VReg_DeleteTreeW(
                api,
                nullptr,
                L"T"
            )
        );
        RECORD(
            "tree invalid handle A",
            VReg_DeleteTreeA(
                api,
                nullptr,
                "T"
            )
        );
        RECORD(
            "tree invalid handle null sub",
            VReg_DeleteTreeW(
                api,
                nullptr,
                nullptr
            )
        );

        HKEY hKey{};
        RECORD(
            "open",
            VReg_OpenKeyExW(
                api,
                hRoot,
                L"V",
                0,
                KEY_ALL_ACCESS,
                &hKey
            )
        );
        RECORD(
            "tree contents via null W",
            VReg_DeleteTreeW(
                api,
                hKey,
                nullptr
            )
        );
        RECORD(
            "tree contents via empty A",
            VReg_DeleteTreeA(
                api,
                hKey,
                ""
            )
        );
        DWORD cSubKeys = FILL_SIZE;
        DWORD cValues = FILL_SIZE;
        RECORD(
            "info",
            VReg_QueryInfoKeyW(
                api,
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
            ),
            {
                cSubKeys,
                cValues,
            }
        );
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );

        if (!api.isVirtualKeys)
        {
            RECORD(
                "open read",
                VReg_OpenKeyExW(
                    api,
                    hRoot,
                    L"W",
                    0,
                    KEY_READ,
                    &hKey
                )
            );
            RECORD(
                "tree on read key",
                VReg_DeleteTreeW(
                    api,
                    hKey,
                    L"X"
                )
            );
            RECORD(
                "close",
                VReg_CloseKey(
                    api,
                    hKey
                )
            );
        }

        RECORD(
            "tree top",
            VReg_DeleteTreeW(
                api,
                hRoot,
                L"T"
            )
        );
        RECORD(
            "tree trailing slash",
            VReg_DeleteTreeW(
                api,
                hRoot,
                L"X\\"
            )
        );

        RECORD(
            "tree root",
            VReg_DeleteTreeW(
                api,
                hRoot,
                nullptr
            )
        );
        cSubKeys = FILL_SIZE;
        cValues = FILL_SIZE;
        RECORD(
            "root info",
            VReg_QueryInfoKeyW(
                api,
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
            ),
            {
                cSubKeys,
                cValues,
            }
        );
    }
};

struct FlushAndNotify
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        HKEY hKey{};
        RECORD(
            "create",
            VReg_CreateKeyExW(
                api,
                hRoot,
                L"Sub",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hKey,
                nullptr
            )
        );
        RECORD(
            "flush root",
            VReg_FlushKey(
                api,
                hRoot
            )
        );
        RECORD(
            "flush sub",
            VReg_FlushKey(
                api,
                hKey
            )
        );
        RECORD(
            "flush invalid handle",
            VReg_FlushKey(
                api,
                nullptr
            )
        );

        const HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        RECORD(
            "notify async",
            VReg_NotifyChangeKeyValue(
                api,
                hKey,
                TRUE,
                REG_NOTIFY_CHANGE_LAST_SET,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async filter name",
            VReg_NotifyChangeKeyValue(
                api,
                hKey,
                FALSE,
                REG_NOTIFY_CHANGE_NAME,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async thread agnostic",
            VReg_NotifyChangeKeyValue(
                api,
                hKey,
                FALSE,
                REG_NOTIFY_THREAD_AGNOSTIC | REG_NOTIFY_CHANGE_LAST_SET,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async no filter",
            VReg_NotifyChangeKeyValue(
                api,
                hKey,
                FALSE,
                0,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async bad filter",
            VReg_NotifyChangeKeyValue(
                api,
                hKey,
                FALSE,
                0x80,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async no event",
            VReg_NotifyChangeKeyValue(
                api,
                hKey,
                FALSE,
                REG_NOTIFY_CHANGE_NAME,
                nullptr,
                TRUE
            )
        );
        RECORD(
            "notify invalid handle",
            VReg_NotifyChangeKeyValue(
                api,
                nullptr,
                FALSE,
                REG_NOTIFY_CHANGE_NAME,
                hEvent,
                TRUE
            )
        );

        const BYTE data[4] = {1, 2, 3, 4};
        RECORD(
            "set after notify",
            VReg_SetValueExW(
                api,
                hKey,
                L"v",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "notify async again",
            VReg_NotifyChangeKeyValue(
                api,
                hKey,
                FALSE,
                REG_NOTIFY_CHANGE_LAST_SET,
                hEvent,
                TRUE
            )
        );
        CloseHandle(hEvent);
        RECORD(
            "close",
            VReg_CloseKey(
                api,
                hKey
            )
        );
    }
};

using Scenarios = std::tuple<
    CreateOpenAndClose,
    QueryStringBufferTooSmall,
    CreateDispositionAndClass,
    CreateOpenAnsi,
    OpenKeyForms,
    PreExOpenAndCreate,
    DeleteKeys,
    OperateOnDeletedKey,
    DeleteTrees,
    FlushAndNotify
>;
} // namespace

TEMPLATE_LIST_TEST_CASE(
    "Key scenarios answer as the Win32 API does",
    "[oracle][keys]",
    Scenarios
)
{
    Win32Registry win32Registry;
    const Api win32 = NativeReference(win32Registry);
    OwnedRegistry native = MakeNativeRegistry();
    CheckSameRecording(Record<TestType>(win32), Record<TestType>(Api{native, false}));
}

TEMPLATE_LIST_TEST_CASE(
    "Key scenarios answer as the Win32 API does on virtual keys",
    "[oracle][virtual][keys]",
    Scenarios
)
{
    Win32Registry win32Registry;
    const Api win32 = ReducedReference(win32Registry);
    CheckSameRecording(Record<TestType>(win32), RecordVirtual<TestType>());
}

TEMPLATE_LIST_TEST_CASE(
    "Key scenarios answer as the Win32 API does on an overlay",
    "[oracle][overlay][keys]",
    Scenarios
)
{
    Win32Registry win32Registry;
    const Api win32 = ReducedReference(win32Registry);
    CheckSameRecording(Record<TestType>(win32), RecordOverlay<TestType>());
}

TEMPLATE_LIST_TEST_CASE(
    "Key scenarios answer on a custom hive as on the hive it forwards to",
    "[oracle][custom][keys]",
    Scenarios
)
{
    CheckSameRecording(RecordVirtual<TestType>(), RecordCustom<TestType>());
}
