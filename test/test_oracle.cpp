// The scenario catalogue for keys. Every scenario runs against the real
// Win32 API and against each `Registry` implementation, each on a fresh app
// hive, and the recordings must match. Value, enumeration and lookup
// scenarios live in the sibling test_oracle_*.cpp files; scenario.h says how
// to write one.

#include "scenario.h"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <tuple>

using namespace VReg;
using namespace VReg::Test;

namespace
{
struct CreateOpenAndClose
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        DWORD dwDisposition = FILL_SIZE;
        RECORD(
            "create new",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create existing",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );

        RECORD(
            "open",
            api.RegOpenKeyExW(
                hRoot,
                L"KEY",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );

        hKey = FillKey();
        RECORD(
            "open missing",
            api.RegOpenKeyExW(
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
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        constexpr wchar_t VALUE_TEXT[] = L"value";
        RECORD(
            "set",
            api.RegSetValueExW(
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
            api.RegQueryValueExW(
                hRoot,
                L"Text",
                nullptr,
                &dwType,
                data.data(),
                &cbData
            ),
            {dwType, cbData},
            {Bytes(data)}
        );
    }
};

struct CreateDispositionAndClass
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        DWORD dwDisposition = FILL_SIZE;
        wchar_t szClass[] = L"MyClass";
        RECORD(
            "create with class",
            api.RegCreateKeyExW(
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
            {dwDisposition, KeyState(hKey)}
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        wchar_t szOther[] = L"OtherClass";
        RECORD(
            "create existing other class",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );

        RECORD(
            "open",
            api.RegOpenKeyExW(
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
                api.RegQueryInfoKeyW(
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
            api.RegCloseKey(
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create volatile",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );

        RECORD(
            "create no disposition",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );

        RECORD(
            "create no result",
            api.RegCreateKeyExW(
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
            api.RegCreateKeyExW(
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
            api.RegCreateKeyExW(
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
            {dwDisposition, KeyState(hKey)}
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        dwDisposition = FILL_SIZE;
        RECORD(
            "create nested partly existing",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );

        for (const wchar_t* const pszSub : {L"", L"\\Leading", L"Trailing\\", L"Double\\\\Slash", L"\\"})
        {
            dwDisposition = FILL_SIZE;
            hKey = FillKey();
            const auto status = api.RegCreateKeyExW(
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
                {dwDisposition, KeyState(hKey), HandedBack<Api>(hKey, hRoot)}
            );
            if (status == ERROR_SUCCESS && !HandedBack<Api>(hKey, hRoot))
                RECORD(
                    "close",
                    api.RegCloseKey(
                        hKey
                    )
                );
        }

        hKey = FillKey();
        RECORD(
            "create invalid handle",
            api.RegCreateKeyExW(
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
            api.RegCreateKeyExW(
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
            api.RegCreateKeyExW(
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
            api.RegCreateKeyExW(
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
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );
    }
};

struct CreateOpenAnsi
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        DWORD dwDisposition = FILL_SIZE;
        char szClass[] = "Cl\xe4ss\x80";
        RECORD(
            "create A",
            api.RegCreateKeyExA(
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
            {dwDisposition, KeyState(hKey)}
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );

        RECORD(
            "open W of A-created",
            api.RegOpenKeyExW(
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
            api.RegQueryInfoKeyW(
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
            api.RegCloseKey(
                hKey
            )
        );

        dwDisposition = FILL_SIZE;
        RECORD(
            "create A existing",
            api.RegCreateKeyExA(
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
            api.RegCloseKey(
                hKey
            )
        );

        RECORD(
            "open A",
            api.RegOpenKeyExA(
                hRoot,
                "K\xe4y\x80",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        hKey = FillKey();
        RECORD(
            "open A missing",
            api.RegOpenKeyExA(
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
            api.RegOpenKeyExA(
                hRoot,
                "K?y?",
                0,
                KEY_READ,
                &hKey
            )
        );

        RECORD(
            "create W unrepresentable",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "open A as ?",
            api.RegOpenKeyExA(
                hRoot,
                "U?",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "open W",
            api.RegOpenKeyExW(
                hRoot,
                L"U\u4e2d",
                0,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "create W best fit",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "open A best fit",
            api.RegOpenKeyExA(
                hRoot,
                "ba",
                0,
                KEY_READ,
                &hKey
            )
        );

        for (const char* const pszSub : {static_cast<const char*>(nullptr), "", "\\Leading", "Trailing\\"})
        {
            hKey = FillKey();
            const auto status = api.RegOpenKeyExA(
                hRoot,
                pszSub,
                0,
                KEY_READ,
                &hKey
            );
            RECORD(
                std::string("open A odd ") + (pszSub ? pszSub : "(null)"),
                status,
                {KeyState(hKey), HandedBack<Api>(hKey, hRoot)}
            );
            if (status == ERROR_SUCCESS && !HandedBack<Api>(hKey, hRoot))
                RECORD(
                    "close",
                    api.RegCloseKey(
                        hKey
                    )
                );
        }

        hKey = FillKey();
        RECORD(
            "open A no result",
            api.RegOpenKeyExA(
                hRoot,
                "K\xe4y\x80",
                0,
                KEY_READ,
                nullptr
            )
        );
        RECORD(
            "create A no result",
            api.RegCreateKeyExA(
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
            api.RegOpenKeyExA(
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
            api.RegCreateKeyExA(
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
            api.RegCreateKeyExA(
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
            api.RegCreateKeyExA(
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
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        RECORD(
            "create",
            api.RegCreateKeyExW(
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
            api.RegCloseKey(
                hKey
            )
        );

        for (const wchar_t* const pszSub :
             {static_cast<const wchar_t*>(nullptr),
              L"",
              L"A",
              L"a\\b",
              L"A\\",
              L"\\A",
              L"A\\\\B",
              L"A\\Missing",
              L"Missing\\B"})
        {
            hKey = FillKey();
            const auto status = api.RegOpenKeyExW(
                hRoot,
                pszSub,
                0,
                KEY_READ,
                &hKey
            );
            RECORD(
                std::string("open ") + (pszSub ? Narrow(pszSub) : "(null)"),
                status,
                {KeyState(hKey), HandedBack<Api>(hKey, hRoot)}
            );
            if (status == ERROR_SUCCESS && !HandedBack<Api>(hKey, hRoot))
                RECORD(
                    "close",
                    api.RegCloseKey(
                        hKey
                    )
                );
        }

        RECORD(
            "open no access",
            api.RegOpenKeyExW(
                hRoot,
                L"A",
                0,
                0,
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "open link option",
            api.RegOpenKeyExW(
                hRoot,
                L"A",
                REG_OPTION_OPEN_LINK,
                KEY_READ,
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        // Native needs SeBackupPrivilege; a virtual key has no access check to bypass and always opens.
        if constexpr (!VIRTUAL_KEYS<Api>)
        {
            hKey = FillKey();
            const auto backupStatus = api.RegOpenKeyExW(
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
                    api.RegCloseKey(
                        hKey
                    )
                );
        }
        hKey = FillKey();
        RECORD(
            "open unknown option",
            api.RegOpenKeyExW(
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
            api.RegOpenKeyExW(
                hRoot,
                L"A",
                0,
                KEY_READ | KEY_WOW64_64KEY,
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "open wow64 32",
            api.RegOpenKeyExW(
                hRoot,
                L"A",
                0,
                KEY_READ | KEY_WOW64_32KEY,
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "open no result",
            api.RegOpenKeyExW(
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
            api.RegOpenKeyExW(
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
            api.RegOpenKeyExW(
                hRoot,
                std::wstring(300, L'k').c_str(),
                0,
                KEY_READ,
                &hKey
            ),
            {KeyState(hKey)}
        );

        // A virtual key has one handle per node, so it carries no per-open access rights.
        if constexpr (!VIRTUAL_KEYS<Api>)
        {
            HKEY hRead{};
            RECORD(
                "open read",
                api.RegOpenKeyExW(
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
                api.RegSetValueExW(
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
                api.RegCreateKeyExW(
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
                api.RegDeleteKeyExW(
                    hRead,
                    L"B",
                    0,
                    0
                )
            );
            RECORD(
                "delete value on read key",
                api.RegDeleteValueW(
                    hRead,
                    L"v"
                )
            );
            RECORD(
                "close",
                api.RegCloseKey(
                    hRead
                )
            );
        }
        RECORD(
            "close null",
            api.RegCloseKey(
                nullptr
            )
        );
    }
};

struct PreExOpenAndCreate
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        RECORD(
            "create W",
            api.RegCreateKeyW(
                hRoot,
                L"A\\B",
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );

        for (const wchar_t* const pszSub :
             {static_cast<const wchar_t*>(nullptr), L"", L"A", L"a\\b", L"A\\", L"\\A", L"Missing", L"A\\\\B"})
        {
            const auto label = std::string(pszSub ? Narrow(pszSub) : "(null)");
            hKey = FillKey();
            auto status = api.RegOpenKeyW(
                hRoot,
                pszSub,
                &hKey
            );
            RECORD("open W " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open W " + label, hKey, hRoot);

            hKey = FillKey();
            status = api.RegCreateKeyW(
                hRoot,
                pszSub,
                &hKey
            );
            RECORD(
                "create W " + label,
                status,
                {KeyState(hKey), hKey == hRoot}
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
            status = api.RegOpenKeyA(
                hRoot,
                pszNarrow,
                &hKey
            );
            RECORD("open A " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open A " + label, hKey, hRoot);

            hKey = FillKey();
            status = api.RegCreateKeyA(
                hRoot,
                pszNarrow,
                &hKey
            );
            RECORD(
                "create A " + label,
                status,
                {KeyState(hKey), hKey == hRoot}
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
            api.RegOpenKeyW(
                hRoot,
                L"A",
                nullptr
            )
        );
        RECORD(
            "open W empty no result",
            api.RegOpenKeyW(
                hRoot,
                L"",
                nullptr
            )
        );
        RECORD(
            "open A no result",
            api.RegOpenKeyA(
                hRoot,
                "A",
                nullptr
            )
        );
        RECORD(
            "open A empty no result",
            api.RegOpenKeyA(
                hRoot,
                "",
                nullptr
            )
        );
        RECORD(
            "create W no result",
            api.RegCreateKeyW(
                hRoot,
                L"A",
                nullptr
            )
        );
        RECORD(
            "create W empty no result",
            api.RegCreateKeyW(
                hRoot,
                L"",
                nullptr
            )
        );
        RECORD(
            "create A no result",
            api.RegCreateKeyA(
                hRoot,
                "A",
                nullptr
            )
        );
        RECORD(
            "create A empty no result",
            api.RegCreateKeyA(
                hRoot,
                "",
                nullptr
            )
        );

        hKey = FillKey();
        RECORD(
            "open W invalid handle",
            api.RegOpenKeyW(
                nullptr,
                L"A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open W empty invalid handle",
            api.RegOpenKeyW(
                nullptr,
                L"",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "open A empty invalid handle",
            api.RegOpenKeyA(
                nullptr,
                "",
                &hKey
            ),
            {KeyState(hKey)}
        );
        hKey = FillKey();
        RECORD(
            "create W invalid handle",
            api.RegCreateKeyW(
                nullptr,
                L"A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create W empty invalid handle",
            api.RegCreateKeyW(
                nullptr,
                L"",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A invalid handle",
            api.RegCreateKeyA(
                nullptr,
                "A",
                &hKey
            ),
            {KeyState(hKey)}
        );
        RECORD(
            "create A empty invalid handle",
            api.RegCreateKeyA(
                nullptr,
                "",
                &hKey
            ),
            {KeyState(hKey)}
        );

        RECORD(
            "create A non-ASCII",
            api.RegCreateKeyA(
                hRoot,
                "N\xe4\x80",
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "open W non-ASCII",
            api.RegOpenKeyW(
                hRoot,
                L"N\u00e4\u20ac",
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        RECORD(
            "open A non-ASCII",
            api.RegOpenKeyA(
                hRoot,
                "n\xe4\x80",
                &hKey
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
        hKey = FillKey();
        RECORD(
            "open A missing",
            api.RegOpenKeyA(
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
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        for (const wchar_t* const pszName : {L"Parent\\Child", L"Leaf1", L"Leaf2", L"Leaf3", L"Leaf4\u00e4", L"Leaf5"})
        {
            RECORD(
                "create",
                api.RegCreateKeyExW(
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
                api.RegCloseKey(
                    hKey
                )
            );
        }

        RECORD(
            "delete missing Ex",
            api.RegDeleteKeyExW(
                hRoot,
                L"Missing",
                0,
                0
            )
        );
        RECORD(
            "delete missing W",
            api.RegDeleteKeyW(
                hRoot,
                L"Missing"
            )
        );
        RECORD(
            "delete missing A",
            api.RegDeleteKeyA(
                hRoot,
                "Missing"
            )
        );
        RECORD(
            "delete missing ExA",
            api.RegDeleteKeyExA(
                hRoot,
                "Missing",
                0,
                0
            )
        );
        RECORD(
            "delete with children Ex",
            api.RegDeleteKeyExW(
                hRoot,
                L"Parent",
                0,
                0
            )
        );
        RECORD(
            "delete with children W",
            api.RegDeleteKeyW(
                hRoot,
                L"Parent"
            )
        );
        RECORD(
            "delete with children A",
            api.RegDeleteKeyA(
                hRoot,
                "Parent"
            )
        );
        RECORD(
            "delete with children ExA",
            api.RegDeleteKeyExA(
                hRoot,
                "Parent",
                0,
                0
            )
        );
        RECORD(
            "delete reserved",
            api.RegDeleteKeyExW(
                hRoot,
                L"Leaf1",
                0,
                5
            )
        );
        RECORD(
            "delete reserved A",
            api.RegDeleteKeyExA(
                hRoot,
                "Leaf1",
                0,
                5
            )
        );
        RECORD(
            "delete wow64 flags",
            api.RegDeleteKeyExW(
                hRoot,
                L"Leaf1",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete wow64 flags again",
            api.RegDeleteKeyExW(
                hRoot,
                L"Leaf1",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete wow64 flags A",
            api.RegDeleteKeyExA(
                hRoot,
                "Leaf2",
                KEY_WOW64_64KEY,
                0
            )
        );
        RECORD(
            "delete other flags",
            api.RegDeleteKeyExW(
                hRoot,
                L"Leaf3",
                KEY_READ,
                0
            )
        );
        RECORD(
            "delete invalid handle",
            api.RegDeleteKeyExW(
                nullptr,
                L"Leaf4\u00e4",
                0,
                0
            )
        );
        RECORD(
            "delete invalid handle A",
            api.RegDeleteKeyA(
                nullptr,
                "Leaf4\xe4"
            )
        );
        RECORD(
            "delete nested",
            api.RegDeleteKeyW(
                hRoot,
                L"Parent\\Child"
            )
        );
        RECORD(
            "delete nested again",
            api.RegDeleteKeyW(
                hRoot,
                L"Parent\\Child"
            )
        );
        RECORD(
            "delete parent",
            api.RegDeleteKeyA(
                hRoot,
                "parent"
            )
        );
        RECORD(
            "delete A non-ASCII",
            api.RegDeleteKeyA(
                hRoot,
                "Leaf4\xe4"
            )
        );
        RECORD(
            "delete backslash",
            api.RegDeleteKeyW(
                hRoot,
                L"\\"
            )
        );
        RECORD(
            "delete long",
            api.RegDeleteKeyW(
                hRoot,
                std::wstring(300, L'k').c_str()
            )
        );
        RECORD(
            "delete trailing slash",
            api.RegDeleteKeyW(
                hRoot,
                L"Leaf5\\"
            )
        );

        for (const auto* const pszSub : {L"Parent", L"Leaf1", L"Leaf2", L"Leaf3", L"Leaf4\u00e4", L"Leaf5"})
        {
            hKey = FillKey();
            const auto status = api.RegOpenKeyExW(
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
                    api.RegCloseKey(
                        hKey
                    )
                );
        }
    }
};

struct OperateOnDeletedKey
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        const BYTE data[4] = {1, 2, 3, 4};
        RECORD(
            "create",
            api.RegCreateKeyExW(
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
            api.RegSetValueExW(
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
            api.RegDeleteKeyExW(
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
            api.RegQueryValueExW(
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
            api.RegQueryValueExA(
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
            api.RegSetValueExW(
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
            api.RegSetValueExA(
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
            api.RegDeleteValueW(
                hKey,
                L"v"
            )
        );
        HKEY hChild = FillKey();
        RECORD(
            "create child after",
            api.RegCreateKeyExW(
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
            api.RegOpenKeyExW(
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
            api.RegEnumKeyExW(
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
            api.RegEnumValueW(
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
            api.RegQueryInfoKeyW(
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
            api.RegFlushKey(
                hKey
            )
        );
        RECORD(
            "delete tree after",
            api.RegDeleteTreeW(
                hKey,
                nullptr
            )
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
    }
};

struct DeleteTrees
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        const BYTE data[4] = {1, 2, 3, 4};
        for (const wchar_t* const pszSub :
             {L"T\\A\\B\\C", L"T\\A", L"T", L"T\\S", L"U\u00e4\\X", L"V\\X", L"W\\X", L"X\\X"})
            RECORD(
                "set " + Narrow(pszSub),
                api.RegSetKeyValueW(
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
            api.RegDeleteTreeW(
                hRoot,
                L"Missing"
            )
        );
        RECORD(
            "tree missing A",
            api.RegDeleteTreeA(
                hRoot,
                "Missing"
            )
        );
        RECORD(
            "tree nested",
            api.RegDeleteTreeW(
                hRoot,
                L"T\\A\\B"
            )
        );
        RECORD(
            "tree nested again",
            api.RegDeleteTreeW(
                hRoot,
                L"T\\A\\B"
            )
        );
        RECORD(
            "tree A",
            api.RegDeleteTreeA(
                hRoot,
                "u\xe4"
            )
        );
        RECORD(
            "tree A again",
            api.RegDeleteTreeA(
                hRoot,
                "u\xe4"
            )
        );
        RECORD(
            "tree invalid handle",
            api.RegDeleteTreeW(
                nullptr,
                L"T"
            )
        );
        RECORD(
            "tree invalid handle A",
            api.RegDeleteTreeA(
                nullptr,
                "T"
            )
        );
        RECORD(
            "tree invalid handle null sub",
            api.RegDeleteTreeW(
                nullptr,
                nullptr
            )
        );

        HKEY hKey{};
        RECORD(
            "open",
            api.RegOpenKeyExW(
                hRoot,
                L"V",
                0,
                KEY_ALL_ACCESS,
                &hKey
            )
        );
        RECORD(
            "tree contents via null W",
            api.RegDeleteTreeW(
                hKey,
                nullptr
            )
        );
        RECORD(
            "tree contents via empty A",
            api.RegDeleteTreeA(
                hKey,
                ""
            )
        );
        DWORD cSubKeys = FILL_SIZE;
        DWORD cValues = FILL_SIZE;
        RECORD(
            "info",
            api.RegQueryInfoKeyW(
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
            {cSubKeys, cValues}
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );

        if constexpr (!VIRTUAL_KEYS<Api>)
        {
            RECORD(
                "open read",
                api.RegOpenKeyExW(
                    hRoot,
                    L"W",
                    0,
                    KEY_READ,
                    &hKey
                )
            );
            RECORD(
                "tree on read key",
                api.RegDeleteTreeW(
                    hKey,
                    L"X"
                )
            );
            RECORD(
                "close",
                api.RegCloseKey(
                    hKey
                )
            );
        }

        RECORD(
            "tree top",
            api.RegDeleteTreeW(
                hRoot,
                L"T"
            )
        );
        RECORD(
            "tree trailing slash",
            api.RegDeleteTreeW(
                hRoot,
                L"X\\"
            )
        );

        RECORD(
            "tree root",
            api.RegDeleteTreeW(
                hRoot,
                nullptr
            )
        );
        cSubKeys = FILL_SIZE;
        cValues = FILL_SIZE;
        RECORD(
            "root info",
            api.RegQueryInfoKeyW(
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
            {cSubKeys, cValues}
        );
    }
};

struct FlushAndNotify
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        RECORD(
            "create",
            api.RegCreateKeyExW(
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
            api.RegFlushKey(
                hRoot
            )
        );
        RECORD(
            "flush sub",
            api.RegFlushKey(
                hKey
            )
        );
        RECORD(
            "flush invalid handle",
            api.RegFlushKey(
                nullptr
            )
        );

        const HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        RECORD(
            "notify async",
            api.RegNotifyChangeKeyValue(
                hKey,
                TRUE,
                REG_NOTIFY_CHANGE_LAST_SET,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async filter name",
            api.RegNotifyChangeKeyValue(
                hKey,
                FALSE,
                REG_NOTIFY_CHANGE_NAME,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async thread agnostic",
            api.RegNotifyChangeKeyValue(
                hKey,
                FALSE,
                REG_NOTIFY_THREAD_AGNOSTIC | REG_NOTIFY_CHANGE_LAST_SET,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async no filter",
            api.RegNotifyChangeKeyValue(
                hKey,
                FALSE,
                0,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async bad filter",
            api.RegNotifyChangeKeyValue(
                hKey,
                FALSE,
                0x80,
                hEvent,
                TRUE
            )
        );
        RECORD(
            "notify async no event",
            api.RegNotifyChangeKeyValue(
                hKey,
                FALSE,
                REG_NOTIFY_CHANGE_NAME,
                nullptr,
                TRUE
            )
        );
        RECORD(
            "notify invalid handle",
            api.RegNotifyChangeKeyValue(
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
            api.RegSetValueExW(
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
            api.RegNotifyChangeKeyValue(
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
            api.RegCloseKey(
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
    FlushAndNotify>;
} // namespace

TEMPLATE_LIST_TEST_CASE(
    "Key scenarios answer as the Win32 API does",
    "[oracle][keys]",
    Scenarios
)
{
    Win32Api win32;
    NativeRegistry native;
    CheckSameRecording(Record<TestType>(win32), Record<TestType>(native));
}

TEMPLATE_LIST_TEST_CASE(
    "Key scenarios answer as the Win32 API does on virtual keys",
    "[oracle][virtual][keys]",
    Scenarios
)
{
    Reduced<Win32Api> win32;
    CheckSameRecording(Record<TestType>(win32), RecordVirtual<TestType>());
}

TEMPLATE_LIST_TEST_CASE(
    "Key scenarios answer as the Win32 API does on an overlay",
    "[oracle][overlay][keys]",
    Scenarios
)
{
    Reduced<Win32Api> win32;
    CheckSameRecording(Record<TestType>(win32), RecordOverlay<TestType>());
}
