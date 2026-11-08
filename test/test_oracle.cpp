// The scenario catalogue for keys. Every scenario runs against the real
// Win32 API and against each `Registry` implementation, each on a fresh app
// hive, and the recordings must match. Value, enumeration and lookup
// scenarios live in the sibling test_oracle_*.cpp files; scenario.h says how
// to write one.

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
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create existing",
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open",
            VRegOpenKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        hKey = FillKey();
        RECORD(
            "open missing",
            VRegOpenKeyExW(
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
            VRegSetValueExW(
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
            VRegQueryValueExW(
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
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        wchar_t szOther[] = L"OtherClass";
        RECORD(
            "create existing other class",
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open",
            VRegOpenKeyExW(
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
                VRegQueryInfoKeyW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create volatile",
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "create no disposition",
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "create no result",
            VRegCreateKeyExW(
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
            VRegCreateKeyExW(
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
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        dwDisposition = FILL_SIZE;
        RECORD(
            "create nested partly existing",
            VRegCreateKeyExW(
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
            VRegCloseKey(
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
            const auto status = VRegCreateKeyExW(
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
                VRegCloseKey(
                    api,
                    hKey
                )
            );
        }

        hKey = FillKey();
        RECORD(
            "create invalid handle",
            VRegCreateKeyExW(
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
            VRegCreateKeyExW(
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
            VRegCreateKeyExW(
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
            VRegCreateKeyExW(
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
            VRegCreateKeyExW(
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
            VRegCloseKey(
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
            VRegCreateKeyExA(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open W of A-created",
            VRegOpenKeyExW(
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
            VRegQueryInfoKeyW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create A existing",
            VRegCreateKeyExA(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        RECORD(
            "open A",
            VRegOpenKeyExA(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        hKey = FillKey();
        RECORD(
            "open A missing",
            VRegOpenKeyExA(
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
            VRegOpenKeyExA(
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
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open A as ?",
            VRegOpenKeyExA(
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
            VRegOpenKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "create W best fit",
            VRegCreateKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open A best fit",
            VRegOpenKeyExA(
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
            const auto status = VRegOpenKeyExA(
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
                VRegCloseKey(
                    api,
                    hKey
                )
            );
        }

        hKey = FillKey();
        RECORD(
            "open A no result",
            VRegOpenKeyExA(
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
            VRegCreateKeyExA(
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
            VRegOpenKeyExA(
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
            VRegCreateKeyExA(
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
            VRegCreateKeyExA(
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
            VRegCreateKeyExA(
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
            VRegCreateKeyExW(
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
            VRegCloseKey(
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
            const auto status = VRegOpenKeyExW(
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
                VRegCloseKey(
                    api,
                    hKey
                )
            );
        }

        RECORD(
            "open no access",
            VRegOpenKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open link option",
            VRegOpenKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        // Native needs SeBackupPrivilege; a virtual key has no access check to
        // bypass and always opens.
        if (!api.isVirtualKeys)
        {
            hKey = FillKey();
            const auto backupStatus = VRegOpenKeyExW(
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
                VRegCloseKey(
                    api,
                    hKey
                )
            );
        }
        hKey = FillKey();
        RECORD(
            "open unknown option",
            VRegOpenKeyExW(
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
            VRegOpenKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open wow64 32",
            VRegOpenKeyExW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open no result",
            VRegOpenKeyExW(
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
            VRegOpenKeyExW(
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
            VRegOpenKeyExW(
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
                VRegOpenKeyExW(
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
                VRegSetValueExW(
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
                VRegCreateKeyExW(
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
                VRegDeleteKeyExW(
                    api,
                    hRead,
                    L"B",
                    0,
                    0
                )
            );
            RECORD(
                "delete value on read key",
                VRegDeleteValueW(
                    api,
                    hRead,
                    L"v"
                )
            );
            RECORD(
                "close",
                VRegCloseKey(
                    api,
                    hRead
                )
            );
        }
        RECORD(
            "close null",
            VRegCloseKey(
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
            VRegCreateKeyW(
                api,
                hRoot,
                L"A\\B",
                &hKey
            )
        );
        RECORD(
            "close",
            VRegCloseKey(
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
            auto status = VRegOpenKeyW(
                api,
                hRoot,
                pszSub,
                &hKey
            );
            RECORD("open W " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open W " + label, hKey, hRoot);

            hKey = FillKey();
            status = VRegCreateKeyW(
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
            status = VRegOpenKeyA(
                api,
                hRoot,
                pszNarrow,
                &hKey
            );
            RECORD("open A " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open A " + label, hKey, hRoot);

            hKey = FillKey();
            status = VRegCreateKeyA(
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
            VRegOpenKeyW(
                api,
                hRoot,
                L"A",
                nullptr
            )
        );
        RECORD(
            "open W empty no result",
            VRegOpenKeyW(
                api,
                hRoot,
                L"",
                nullptr
            )
        );
        RECORD(
            "open A no result",
            VRegOpenKeyA(
                api,
                hRoot,
                "A",
                nullptr
            )
        );
        RECORD(
            "open A empty no result",
            VRegOpenKeyA(
                api,
                hRoot,
                "",
                nullptr
            )
        );
        RECORD(
            "create W no result",
            VRegCreateKeyW(
                api,
                hRoot,
                L"A",
                nullptr
            )
        );
        RECORD(
            "create W empty no result",
            VRegCreateKeyW(
                api,
                hRoot,
                L"",
                nullptr
            )
        );
        RECORD(
            "create A no result",
            VRegCreateKeyA(
                api,
                hRoot,
                "A",
                nullptr
            )
        );
        RECORD(
            "create A empty no result",
            VRegCreateKeyA(
                api,
                hRoot,
                "",
                nullptr
            )
        );

        hKey = FillKey();
        RECORD(
            "open W invalid handle",
            VRegOpenKeyW(
                api,
                nullptr,
                L"A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open W empty invalid handle",
            VRegOpenKeyW(
                api,
                nullptr,
                L"",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open A empty invalid handle",
            VRegOpenKeyA(
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
            VRegCreateKeyW(
                api,
                nullptr,
                L"A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create W empty invalid handle",
            VRegCreateKeyW(
                api,
                nullptr,
                L"",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A invalid handle",
            VRegCreateKeyA(
                api,
                nullptr,
                "A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A empty invalid handle",
            VRegCreateKeyA(
                api,
                nullptr,
                "",
                &hKey
            ),
            {KeyState(hKey)}
        );

        RECORD(
            "create A non-ASCII",
            VRegCreateKeyA(
                api,
                hRoot,
                "N\xe4\x80",
                &hKey
            )
        );
        RECORD(
            "close",
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open W non-ASCII",
            VRegOpenKeyW(
                api,
                hRoot,
                L"N\u00e4\u20ac",
                &hKey
            )
        );
        RECORD(
            "close",
            VRegCloseKey(
                api,
                hKey
            )
        );
        RECORD(
            "open A non-ASCII",
            VRegOpenKeyA(
                api,
                hRoot,
                "n\xe4\x80",
                &hKey
            )
        );
        RECORD(
            "close",
            VRegCloseKey(
                api,
                hKey
            )
        );
        hKey = FillKey();
        RECORD(
            "open A missing",
            VRegOpenKeyA(
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
                VRegCreateKeyExW(
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
                VRegCloseKey(
                    api,
                    hKey
                )
            );
        }

        RECORD(
            "delete missing Ex",
            VRegDeleteKeyExW(
                api,
                hRoot,
                L"Missing",
                0,
                0
            )
        );
        RECORD(
            "delete missing W",
            VRegDeleteKeyW(
                api,
                hRoot,
                L"Missing"
            )
        );
        RECORD(
            "delete missing A",
            VRegDeleteKeyA(
                api,
                hRoot,
                "Missing"
            )
        );
        RECORD(
            "delete missing ExA",
            VRegDeleteKeyExA(
                api,
                hRoot,
                "Missing",
                0,
                0
            )
        );
        RECORD(
            "delete with children Ex",
            VRegDeleteKeyExW(
                api,
                hRoot,
                L"Parent",
                0,
                0
            )
        );
        RECORD(
            "delete with children W",
            VRegDeleteKeyW(
                api,
                hRoot,
                L"Parent"
            )
        );
        RECORD(
            "delete with children A",
            VRegDeleteKeyA(
                api,
                hRoot,
                "Parent"
            )
        );
        RECORD(
            "delete with children ExA",
            VRegDeleteKeyExA(
                api,
                hRoot,
                "Parent",
                0,
                0
            )
        );
        RECORD(
            "delete reserved",
            VRegDeleteKeyExW(
                api,
                hRoot,
                L"Leaf1",
                0,
                5
            )
        );
        RECORD(
            "delete reserved A",
            VRegDeleteKeyExA(
                api,
                hRoot,
                "Leaf1",
                0,
                5
            )
        );
        RECORD(
            "delete wow64 flags",
            VRegDeleteKeyExW(
                api,
                hRoot,
                L"Leaf1",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete wow64 flags again",
            VRegDeleteKeyExW(
                api,
                hRoot,
                L"Leaf1",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete wow64 flags A",
            VRegDeleteKeyExA(
                api,
                hRoot,
                "Leaf2",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete other flags",
            VRegDeleteKeyExW(
                api,
                hRoot,
                L"Leaf3",
                KEY_READ,
                0
            )
        );
        RECORD(
            "delete invalid handle",
            VRegDeleteKeyExW(
                api,
                nullptr,
                L"Leaf4\u00e4",
                0,
                0
            )
        );
        RECORD(
            "delete invalid handle A",
            VRegDeleteKeyA(
                api,
                nullptr,
                "Leaf4\xe4"
            )
        );
        RECORD(
            "delete nested",
            VRegDeleteKeyW(
                api,
                hRoot,
                L"Parent\\Child"
            )
        );
        RECORD(
            "delete nested again",
            VRegDeleteKeyW(
                api,
                hRoot,
                L"Parent\\Child"
            )
        );
        RECORD(
            "delete parent",
            VRegDeleteKeyA(
                api,
                hRoot,
                "parent"
            )
        );
        RECORD(
            "delete A non-ASCII",
            VRegDeleteKeyA(
                api,
                hRoot,
                "Leaf4\xe4"
            )
        );
        RECORD(
            "delete backslash",
            VRegDeleteKeyW(
                api,
                hRoot,
                L"\\"
            )
        );
        RECORD(
            "delete long",
            VRegDeleteKeyW(
                api,
                hRoot,
                std::wstring(300, L'k').c_str()
            )
        );
        RECORD(
            "delete trailing slash",
            VRegDeleteKeyW(
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
            const auto status = VRegOpenKeyExW(
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
                VRegCloseKey(
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
            VRegCreateKeyExW(
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
            VRegSetValueExW(
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
            VRegDeleteKeyExW(
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
            VRegQueryValueExW(
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
            VRegQueryValueExA(
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
            VRegSetValueExW(
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
            VRegSetValueExA(
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
            VRegDeleteValueW(
                api,
                hKey,
                L"v"
            )
        );
        HKEY hChild = FillKey();
        RECORD(
            "create child after",
            VRegCreateKeyExW(
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
            VRegOpenKeyExW(
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
            VRegEnumKeyExW(
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
            VRegEnumValueW(
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
            VRegQueryInfoKeyW(
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
            VRegFlushKey(
                api,
                hKey
            )
        );
        RECORD(
            "delete tree after",
            VRegDeleteTreeW(
                api,
                hKey,
                nullptr
            )
        );
        RECORD(
            "close",
            VRegCloseKey(
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
            VRegSetKeyValueW(
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
            VRegDeleteTreeW(
                api,
                hRoot,
                L"Missing"
            )
        );
        RECORD(
            "tree missing A",
            VRegDeleteTreeA(
                api,
                hRoot,
                "Missing"
            )
        );
        RECORD(
            "tree nested",
            VRegDeleteTreeW(
                api,
                hRoot,
                L"T\\A\\B"
            )
        );
        RECORD(
            "tree nested again",
            VRegDeleteTreeW(
                api,
                hRoot,
                L"T\\A\\B"
            )
        );
        RECORD(
            "tree A",
            VRegDeleteTreeA(
                api,
                hRoot,
                "u\xe4"
            )
        );
        RECORD(
            "tree A again",
            VRegDeleteTreeA(
                api,
                hRoot,
                "u\xe4"
            )
        );
        RECORD(
            "tree invalid handle",
            VRegDeleteTreeW(
                api,
                nullptr,
                L"T"
            )
        );
        RECORD(
            "tree invalid handle A",
            VRegDeleteTreeA(
                api,
                nullptr,
                "T"
            )
        );
        RECORD(
            "tree invalid handle null sub",
            VRegDeleteTreeW(
                api,
                nullptr,
                nullptr
            )
        );

        HKEY hKey{};
        RECORD(
            "open",
            VRegOpenKeyExW(
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
            VRegDeleteTreeW(
                api,
                hKey,
                nullptr
            )
        );
        RECORD(
            "tree contents via empty A",
            VRegDeleteTreeA(
                api,
                hKey,
                ""
            )
        );
        DWORD cSubKeys = FILL_SIZE;
        DWORD cValues = FILL_SIZE;
        RECORD(
            "info",
            VRegQueryInfoKeyW(
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
            VRegCloseKey(
                api,
                hKey
            )
        );

        if (!api.isVirtualKeys)
        {
            RECORD(
                "open read",
                VRegOpenKeyExW(
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
                VRegDeleteTreeW(
                    api,
                    hKey,
                    L"X"
                )
            );
            RECORD(
                "close",
                VRegCloseKey(
                    api,
                    hKey
                )
            );
        }

        RECORD(
            "tree top",
            VRegDeleteTreeW(
                api,
                hRoot,
                L"T"
            )
        );
        RECORD(
            "tree trailing slash",
            VRegDeleteTreeW(
                api,
                hRoot,
                L"X\\"
            )
        );

        RECORD(
            "tree root",
            VRegDeleteTreeW(
                api,
                hRoot,
                nullptr
            )
        );
        cSubKeys = FILL_SIZE;
        cValues = FILL_SIZE;
        RECORD(
            "root info",
            VRegQueryInfoKeyW(
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
            VRegCreateKeyExW(
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
            VRegFlushKey(
                api,
                hRoot
            )
        );
        RECORD(
            "flush sub",
            VRegFlushKey(
                api,
                hKey
            )
        );
        RECORD(
            "flush invalid handle",
            VRegFlushKey(
                api,
                nullptr
            )
        );

        const HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        RECORD(
            "notify async",
            VRegNotifyChangeKeyValue(
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
            VRegNotifyChangeKeyValue(
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
            VRegNotifyChangeKeyValue(
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
            VRegNotifyChangeKeyValue(
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
            VRegNotifyChangeKeyValue(
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
            VRegNotifyChangeKeyValue(
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
            VRegNotifyChangeKeyValue(
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
            VRegSetValueExW(
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
            VRegNotifyChangeKeyValue(
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
            VRegCloseKey(
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
