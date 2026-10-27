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
        DWORD dwDisposition = kFillSize;
        RECORD(
            "create new",
            api.CreateKeyExW(hRoot, L"Key", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition),
            {dwDisposition}
        );
        RECORD("close", api.CloseKey(hKey));

        dwDisposition = kFillSize;
        RECORD(
            "create existing",
            api.CreateKeyExW(hRoot, L"key", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition),
            {dwDisposition}
        );
        RECORD("close", api.CloseKey(hKey));

        RECORD("open", api.OpenKeyExW(hRoot, L"KEY", 0, KEY_READ, &hKey));
        RECORD("close", api.CloseKey(hKey));

        hKey = FillKey();
        RECORD("open missing", api.OpenKeyExW(hRoot, L"Missing", 0, KEY_READ, &hKey), {KeyState(hKey)});
    }
};

struct QueryStringBufferTooSmall
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        constexpr wchar_t kText[] = L"value";
        RECORD("set", api.SetValueExW(hRoot, L"Text", 0, REG_SZ, reinterpret_cast<const BYTE*>(kText), sizeof(kText)));

        std::array<BYTE, 6> data;
        data.fill(kFill);
        DWORD dwType = kFillSize;
        DWORD cbData = static_cast<DWORD>(data.size());
        RECORD(
            "query too small",
            api.QueryValueExW(hRoot, L"Text", nullptr, &dwType, data.data(), &cbData),
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
        DWORD dwDisposition = kFillSize;
        wchar_t szClass[] = L"MyClass";
        RECORD(
            "create with class",
            api.CreateKeyExW(
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
        RECORD("close", api.CloseKey(hKey));

        dwDisposition = kFillSize;
        wchar_t szOther[] = L"OtherClass";
        RECORD(
            "create existing other class",
            api.CreateKeyExW(hRoot, L"WithClass", 0, szOther, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition),
            {dwDisposition}
        );
        RECORD("close", api.CloseKey(hKey));

        RECORD("open", api.OpenKeyExW(hRoot, L"WithClass", 0, KEY_READ, &hKey));
        for (const DWORD cchClass : {0u, 3u, 7u, 8u, 64u})
        {
            std::array<wchar_t, 64> cls;
            std::memset(cls.data(), kFill, sizeof(cls));
            DWORD cch = cchClass;
            RECORD(
                "class",
                api.QueryInfoKeyW(
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
        RECORD("close", api.CloseKey(hKey));

        dwDisposition = kFillSize;
        RECORD(
            "create volatile",
            api.CreateKeyExW(
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
        RECORD("close", api.CloseKey(hKey));

        RECORD(
            "create no disposition",
            api.CreateKeyExW(hRoot, L"NoDisposition", 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr)
        );
        RECORD("close", api.CloseKey(hKey));

        RECORD(
            "create no result",
            api.CreateKeyExW(hRoot, L"NoResult", 0, nullptr, 0, KEY_READ, nullptr, nullptr, nullptr)
        );
        dwDisposition = kFillSize;
        RECORD(
            "create no result with disposition",
            api.CreateKeyExW(hRoot, L"NoResult2", 0, nullptr, 0, KEY_READ, nullptr, nullptr, &dwDisposition),
            {dwDisposition}
        );

        dwDisposition = kFillSize;
        hKey = FillKey();
        RECORD(
            "create nested",
            api.CreateKeyExW(hRoot, L"A\\B\\C", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition),
            {dwDisposition, KeyState(hKey)}
        );
        RECORD("close", api.CloseKey(hKey));
        dwDisposition = kFillSize;
        RECORD(
            "create nested partly existing",
            api.CreateKeyExW(hRoot, L"A\\B\\D", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition),
            {dwDisposition}
        );
        RECORD("close", api.CloseKey(hKey));

        for (const wchar_t* const pszSub : {L"", L"\\Leading", L"Trailing\\", L"Double\\\\Slash", L"\\"})
        {
            dwDisposition = kFillSize;
            hKey = FillKey();
            const auto status
                = api.CreateKeyExW(hRoot, pszSub, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition);
            RECORD(
                "create odd path " + Narrow(pszSub),
                status,
                {dwDisposition, KeyState(hKey), HandedBack<Api>(hKey, hRoot)}
            );
            if (status == ERROR_SUCCESS && !HandedBack<Api>(hKey, hRoot))
                RECORD("close", api.CloseKey(hKey));
        }

        hKey = FillKey();
        RECORD(
            "create invalid handle",
            api.CreateKeyExW(nullptr, L"X", 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr),
            {KeyState(hKey)}
        );
        RECORD(
            "create reserved",
            api.CreateKeyExW(hRoot, L"R", 5, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr),
            {KeyState(hKey)}
        );
        RECORD(
            "create unknown option",
            api.CreateKeyExW(hRoot, L"O", 0, nullptr, 0x100, KEY_READ, nullptr, &hKey, nullptr),
            {KeyState(hKey)}
        );
        RECORD(
            "create long name",
            api.CreateKeyExW(hRoot, std::wstring(300, L'k').c_str(), 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr),
            {KeyState(hKey)}
        );
        dwDisposition = kFillSize;
        RECORD(
            "create case differs",
            api.CreateKeyExW(hRoot, L"withclass", 0, nullptr, 0, KEY_READ, nullptr, &hKey, &dwDisposition),
            {dwDisposition}
        );
        RECORD("close", api.CloseKey(hKey));
    }
};

struct CreateOpenAnsi
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        DWORD dwDisposition = kFillSize;
        char szClass[] = "Cl\xe4ss\x80";
        RECORD(
            "create A",
            api.CreateKeyExA(hRoot, "K\xe4y\x80", 0, szClass, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition),
            {dwDisposition, KeyState(hKey)}
        );
        RECORD("close", api.CloseKey(hKey));

        RECORD("open W of A-created", api.OpenKeyExW(hRoot, L"K\u00e4y\u20ac", 0, KEY_READ, &hKey));
        std::array<wchar_t, 16> cls;
        std::memset(cls.data(), kFill, sizeof(cls));
        DWORD cch = 16;
        RECORD(
            "class as W",
            api.QueryInfoKeyW(
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
        RECORD("close", api.CloseKey(hKey));

        dwDisposition = kFillSize;
        RECORD(
            "create A existing",
            api.CreateKeyExA(hRoot, "k\xe4y\x80", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition),
            {dwDisposition}
        );
        RECORD("close", api.CloseKey(hKey));

        RECORD("open A", api.OpenKeyExA(hRoot, "K\xe4y\x80", 0, KEY_READ, &hKey));
        RECORD("close", api.CloseKey(hKey));
        hKey = FillKey();
        RECORD("open A missing", api.OpenKeyExA(hRoot, "Missing", 0, KEY_READ, &hKey), {KeyState(hKey)});
        RECORD("open A unrepresentable", api.OpenKeyExA(hRoot, "K?y?", 0, KEY_READ, &hKey));

        RECORD(
            "create W unrepresentable",
            api.CreateKeyExW(hRoot, L"U\u4e2d", 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr)
        );
        RECORD("close", api.CloseKey(hKey));
        RECORD("open A as ?", api.OpenKeyExA(hRoot, "U?", 0, KEY_READ, &hKey));
        RECORD("open W", api.OpenKeyExW(hRoot, L"U\u4e2d", 0, KEY_READ, &hKey));
        RECORD("close", api.CloseKey(hKey));
        RECORD(
            "create W best fit",
            api.CreateKeyExW(hRoot, L"b\u0101", 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr)
        );
        RECORD("close", api.CloseKey(hKey));
        RECORD("open A best fit", api.OpenKeyExA(hRoot, "ba", 0, KEY_READ, &hKey));

        for (const char* const pszSub : {static_cast<const char*>(nullptr), "", "\\Leading", "Trailing\\"})
        {
            hKey = FillKey();
            const auto status = api.OpenKeyExA(hRoot, pszSub, 0, KEY_READ, &hKey);
            RECORD(
                std::string("open A odd ") + (pszSub ? pszSub : "(null)"),
                status,
                {KeyState(hKey), HandedBack<Api>(hKey, hRoot)}
            );
            if (status == ERROR_SUCCESS && !HandedBack<Api>(hKey, hRoot))
                RECORD("close", api.CloseKey(hKey));
        }

        hKey = FillKey();
        RECORD("open A no result", api.OpenKeyExA(hRoot, "K\xe4y\x80", 0, KEY_READ, nullptr));
        RECORD(
            "create A no result",
            api.CreateKeyExA(hRoot, "K\xe4y\x80", 0, nullptr, 0, KEY_READ, nullptr, nullptr, nullptr)
        );
        RECORD("open A invalid handle", api.OpenKeyExA(nullptr, "K", 0, KEY_READ, &hKey), {KeyState(hKey)});
        RECORD(
            "create A invalid handle",
            api.CreateKeyExA(nullptr, "K", 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr),
            {KeyState(hKey)}
        );
        RECORD(
            "create A reserved",
            api.CreateKeyExA(hRoot, "R", 5, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr),
            {KeyState(hKey)}
        );
        RECORD(
            "create A long name",
            api.CreateKeyExA(hRoot, std::string(300, 'k').c_str(), 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr),
            {KeyState(hKey)}
        );
    }
};

struct OpenKeyForms
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        RECORD("create", api.CreateKeyExW(hRoot, L"A\\B", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr));
        RECORD("close", api.CloseKey(hKey));

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
            const auto status = api.OpenKeyExW(hRoot, pszSub, 0, KEY_READ, &hKey);
            RECORD(
                std::string("open ") + (pszSub ? Narrow(pszSub) : "(null)"),
                status,
                {KeyState(hKey), HandedBack<Api>(hKey, hRoot)}
            );
            if (status == ERROR_SUCCESS && !HandedBack<Api>(hKey, hRoot))
                RECORD("close", api.CloseKey(hKey));
        }

        RECORD("open no access", api.OpenKeyExW(hRoot, L"A", 0, 0, &hKey));
        RECORD("close", api.CloseKey(hKey));
        RECORD("open link option", api.OpenKeyExW(hRoot, L"A", REG_OPTION_OPEN_LINK, KEY_READ, &hKey));
        RECORD("close", api.CloseKey(hKey));
        // Native needs SeBackupPrivilege; a virtual key has no access check to bypass and always opens.
        if constexpr (!kVirtualKeys<Api>)
        {
            hKey = FillKey();
            const auto backupStatus = api.OpenKeyExW(hRoot, L"A", REG_OPTION_BACKUP_RESTORE, KEY_READ, &hKey);
            RECORD("open backup option", backupStatus, {KeyState(hKey)});
            if (backupStatus == ERROR_SUCCESS)
                RECORD("close", api.CloseKey(hKey));
        }
        hKey = FillKey();
        RECORD("open unknown option", api.OpenKeyExW(hRoot, L"A", 0x100, KEY_READ, &hKey), {KeyState(hKey)});
        RECORD("open wow64 64", api.OpenKeyExW(hRoot, L"A", 0, KEY_READ | KEY_WOW64_64KEY, &hKey));
        RECORD("close", api.CloseKey(hKey));
        RECORD("open wow64 32", api.OpenKeyExW(hRoot, L"A", 0, KEY_READ | KEY_WOW64_32KEY, &hKey));
        RECORD("close", api.CloseKey(hKey));
        RECORD("open no result", api.OpenKeyExW(hRoot, L"A", 0, KEY_READ, nullptr));
        hKey = FillKey();
        RECORD("open invalid handle", api.OpenKeyExW(nullptr, L"A", 0, KEY_READ, &hKey), {KeyState(hKey)});
        RECORD(
            "open long name",
            api.OpenKeyExW(hRoot, std::wstring(300, L'k').c_str(), 0, KEY_READ, &hKey),
            {KeyState(hKey)}
        );

        // A virtual key has one handle per node, so it carries no per-open access rights.
        if constexpr (!kVirtualKeys<Api>)
        {
            HKEY hRead{};
            RECORD("open read", api.OpenKeyExW(hRoot, L"A", 0, KEY_READ, &hRead));
            const BYTE data[4] = {1, 2, 3, 4};
            RECORD("set on read key", api.SetValueExW(hRead, L"v", 0, REG_BINARY, data, 4));
            hKey = FillKey();
            RECORD(
                "create on read key",
                api.CreateKeyExW(hRead, L"New", 0, nullptr, 0, KEY_READ, nullptr, &hKey, nullptr),
                {KeyState(hKey)}
            );
            RECORD("delete on read key", api.DeleteKeyExW(hRead, L"B", 0, 0));
            RECORD("delete value on read key", api.DeleteValueW(hRead, L"v"));
            RECORD("close", api.CloseKey(hRead));
        }
        RECORD("close null", api.CloseKey(nullptr));
    }
};

struct PreExOpenAndCreate
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        RECORD("create W", api.CreateKeyW(hRoot, L"A\\B", &hKey));
        RECORD("close", api.CloseKey(hKey));

        for (const wchar_t* const pszSub :
             {static_cast<const wchar_t*>(nullptr), L"", L"A", L"a\\b", L"A\\", L"\\A", L"Missing", L"A\\\\B"})
        {
            const auto label = std::string(pszSub ? Narrow(pszSub) : "(null)");
            hKey = FillKey();
            auto status = api.OpenKeyW(hRoot, pszSub, &hKey);
            RECORD("open W " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open W " + label, hKey, hRoot);

            hKey = FillKey();
            status = api.CreateKeyW(hRoot, pszSub, &hKey);
            RECORD("create W " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "create W " + label, hKey, hRoot);

            const auto narrow = pszSub ? Narrow(pszSub) : std::string();
            const char* const pszNarrow = pszSub ? narrow.c_str() : nullptr;
            hKey = FillKey();
            status = api.OpenKeyA(hRoot, pszNarrow, &hKey);
            RECORD("open A " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "open A " + label, hKey, hRoot);

            hKey = FillKey();
            status = api.CreateKeyA(hRoot, pszNarrow, &hKey);
            RECORD("create A " + label, status, {KeyState(hKey), hKey == hRoot});
            if (status == ERROR_SUCCESS)
                CloseUnlessParent(api, record, "create A " + label, hKey, hRoot);
        }

        RECORD("open W no result", api.OpenKeyW(hRoot, L"A", nullptr));
        RECORD("open W empty no result", api.OpenKeyW(hRoot, L"", nullptr));
        RECORD("open A no result", api.OpenKeyA(hRoot, "A", nullptr));
        RECORD("open A empty no result", api.OpenKeyA(hRoot, "", nullptr));
        RECORD("create W no result", api.CreateKeyW(hRoot, L"A", nullptr));
        RECORD("create W empty no result", api.CreateKeyW(hRoot, L"", nullptr));
        RECORD("create A no result", api.CreateKeyA(hRoot, "A", nullptr));
        RECORD("create A empty no result", api.CreateKeyA(hRoot, "", nullptr));

        hKey = FillKey();
        RECORD("open W invalid handle", api.OpenKeyW(nullptr, L"A", &hKey), {KeyState(hKey)});
        RECORD("open W empty invalid handle", api.OpenKeyW(nullptr, L"", &hKey), {KeyState(hKey)});
        RECORD("open A empty invalid handle", api.OpenKeyA(nullptr, "", &hKey), {KeyState(hKey)});
        hKey = FillKey();
        RECORD("create W invalid handle", api.CreateKeyW(nullptr, L"A", &hKey), {KeyState(hKey)});
        RECORD("create W empty invalid handle", api.CreateKeyW(nullptr, L"", &hKey), {KeyState(hKey)});
        RECORD("create A invalid handle", api.CreateKeyA(nullptr, "A", &hKey), {KeyState(hKey)});
        RECORD("create A empty invalid handle", api.CreateKeyA(nullptr, "", &hKey), {KeyState(hKey)});

        RECORD("create A non-ASCII", api.CreateKeyA(hRoot, "N\xe4\x80", &hKey));
        RECORD("close", api.CloseKey(hKey));
        RECORD("open W non-ASCII", api.OpenKeyW(hRoot, L"N\u00e4\u20ac", &hKey));
        RECORD("close", api.CloseKey(hKey));
        RECORD("open A non-ASCII", api.OpenKeyA(hRoot, "n\xe4\x80", &hKey));
        RECORD("close", api.CloseKey(hKey));
        hKey = FillKey();
        RECORD("open A missing", api.OpenKeyA(hRoot, "Missing", &hKey), {KeyState(hKey)});
    }
};

struct DeleteKeys
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        for (const wchar_t* const pszName : {L"Parent\\Child", L"Leaf1", L"Leaf2", L"Leaf3", L"Leaf4\u00e4", L"Leaf5"})
        {
            RECORD("create", api.CreateKeyExW(hRoot, pszName, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr));
            RECORD("close", api.CloseKey(hKey));
        }

        RECORD("delete missing Ex", api.DeleteKeyExW(hRoot, L"Missing", 0, 0));
        RECORD("delete missing W", api.DeleteKeyW(hRoot, L"Missing"));
        RECORD("delete missing A", api.DeleteKeyA(hRoot, "Missing"));
        RECORD("delete missing ExA", api.DeleteKeyExA(hRoot, "Missing", 0, 0));
        RECORD("delete with children Ex", api.DeleteKeyExW(hRoot, L"Parent", 0, 0));
        RECORD("delete with children W", api.DeleteKeyW(hRoot, L"Parent"));
        RECORD("delete with children A", api.DeleteKeyA(hRoot, "Parent"));
        RECORD("delete with children ExA", api.DeleteKeyExA(hRoot, "Parent", 0, 0));
        RECORD("delete reserved", api.DeleteKeyExW(hRoot, L"Leaf1", 0, 5));
        RECORD("delete reserved A", api.DeleteKeyExA(hRoot, "Leaf1", 0, 5));
        RECORD("delete wow64 flags", api.DeleteKeyExW(hRoot, L"Leaf1", KEY_WOW64_64KEY, 0));
        RECORD("delete wow64 flags again", api.DeleteKeyExW(hRoot, L"Leaf1", KEY_WOW64_64KEY, 0));
        RECORD("delete wow64 flags A", api.DeleteKeyExA(hRoot, "Leaf2", KEY_WOW64_64KEY, 0));
        RECORD("delete other flags", api.DeleteKeyExW(hRoot, L"Leaf3", KEY_READ, 0));
        RECORD("delete invalid handle", api.DeleteKeyExW(nullptr, L"Leaf4\u00e4", 0, 0));
        RECORD("delete invalid handle A", api.DeleteKeyA(nullptr, "Leaf4\xe4"));
        RECORD("delete nested", api.DeleteKeyW(hRoot, L"Parent\\Child"));
        RECORD("delete nested again", api.DeleteKeyW(hRoot, L"Parent\\Child"));
        RECORD("delete parent", api.DeleteKeyA(hRoot, "parent"));
        RECORD("delete A non-ASCII", api.DeleteKeyA(hRoot, "Leaf4\xe4"));
        RECORD("delete backslash", api.DeleteKeyW(hRoot, L"\\"));
        RECORD("delete long", api.DeleteKeyW(hRoot, std::wstring(300, L'k').c_str()));
        RECORD("delete trailing slash", api.DeleteKeyW(hRoot, L"Leaf5\\"));

        for (const auto* const pszSub : {L"Parent", L"Leaf1", L"Leaf2", L"Leaf3", L"Leaf4\u00e4", L"Leaf5"})
        {
            hKey = FillKey();
            const auto status = api.OpenKeyExW(hRoot, pszSub, 0, KEY_READ, &hKey);
            RECORD("open " + Narrow(pszSub), status);
            if (status == ERROR_SUCCESS)
                RECORD("close", api.CloseKey(hKey));
        }
    }
};

struct OperateOnDeletedKey
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        HKEY hKey{};
        const BYTE data[4] = {1, 2, 3, 4};
        RECORD("create", api.CreateKeyExW(hRoot, L"Doomed", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr));
        RECORD("set", api.SetValueExW(hKey, L"v", 0, REG_BINARY, data, 4));
        RECORD("delete", api.DeleteKeyExW(hRoot, L"Doomed", 0, 0));

        DWORD cbData = 4;
        std::array<BYTE, 4> out;
        out.fill(kFill);
        RECORD("query", api.QueryValueExW(hKey, L"v", nullptr, nullptr, out.data(), &cbData), {cbData}, {Bytes(out)});
        cbData = 4;
        RECORD("query A", api.QueryValueExA(hKey, "v", nullptr, nullptr, out.data(), &cbData), {cbData}, {Bytes(out)});
        RECORD("set after", api.SetValueExW(hKey, L"w", 0, REG_BINARY, data, 4));
        RECORD("set after A", api.SetValueExA(hKey, "w", 0, REG_BINARY, data, 4));
        RECORD("delete value after", api.DeleteValueW(hKey, L"v"));
        HKEY hChild = FillKey();
        RECORD(
            "create child after",
            api.CreateKeyExW(hKey, L"Child", 0, nullptr, 0, KEY_READ, nullptr, &hChild, nullptr),
            {KeyState(hChild)}
        );
        hChild = FillKey();
        RECORD("open child after", api.OpenKeyExW(hKey, L"Child", 0, KEY_READ, &hChild), {KeyState(hChild)});
        std::array<wchar_t, 8> name;
        DWORD cchName = 8;
        RECORD("enum key after", api.EnumKeyExW(hKey, 0, name.data(), &cchName, nullptr, nullptr, nullptr, nullptr));
        cchName = 8;
        RECORD("enum value after", api.EnumValueW(hKey, 0, name.data(), &cchName, nullptr, nullptr, nullptr, nullptr));
        RECORD(
            "query info after",
            api.QueryInfoKeyW(
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
        RECORD("flush after", api.FlushKey(hKey));
        RECORD("delete tree after", api.DeleteTreeW(hKey, nullptr));
        RECORD("close", api.CloseKey(hKey));
    }
};

struct DeleteTrees
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        const BYTE data[4] = {1, 2, 3, 4};
        for (const wchar_t* const pszSub :
             {L"T\\A\\B\\C", L"T\\A", L"T", L"T\\S", L"U\u00e4\\X", L"V\\X", L"W\\X", L"X\\X"})
            RECORD("set " + Narrow(pszSub), api.SetKeyValueW(hRoot, pszSub, L"v", REG_BINARY, data, 4));

        RECORD("tree missing", api.DeleteTreeW(hRoot, L"Missing"));
        RECORD("tree missing A", api.DeleteTreeA(hRoot, "Missing"));
        RECORD("tree nested", api.DeleteTreeW(hRoot, L"T\\A\\B"));
        RECORD("tree nested again", api.DeleteTreeW(hRoot, L"T\\A\\B"));
        RECORD("tree A", api.DeleteTreeA(hRoot, "u\xe4"));
        RECORD("tree A again", api.DeleteTreeA(hRoot, "u\xe4"));
        RECORD("tree invalid handle", api.DeleteTreeW(nullptr, L"T"));
        RECORD("tree invalid handle A", api.DeleteTreeA(nullptr, "T"));
        RECORD("tree invalid handle null sub", api.DeleteTreeW(nullptr, nullptr));

        HKEY hKey{};
        RECORD("open", api.OpenKeyExW(hRoot, L"V", 0, KEY_ALL_ACCESS, &hKey));
        RECORD("tree contents via null W", api.DeleteTreeW(hKey, nullptr));
        RECORD("tree contents via empty A", api.DeleteTreeA(hKey, ""));
        DWORD cSubKeys = kFillSize;
        DWORD cValues = kFillSize;
        RECORD(
            "info",
            api.QueryInfoKeyW(
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
        RECORD("close", api.CloseKey(hKey));

        if constexpr (!kVirtualKeys<Api>)
        {
            RECORD("open read", api.OpenKeyExW(hRoot, L"W", 0, KEY_READ, &hKey));
            RECORD("tree on read key", api.DeleteTreeW(hKey, L"X"));
            RECORD("close", api.CloseKey(hKey));
        }

        RECORD("tree top", api.DeleteTreeW(hRoot, L"T"));
        RECORD("tree trailing slash", api.DeleteTreeW(hRoot, L"X\\"));

        RECORD("tree root", api.DeleteTreeW(hRoot, nullptr));
        cSubKeys = kFillSize;
        cValues = kFillSize;
        RECORD(
            "root info",
            api.QueryInfoKeyW(
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
        RECORD("create", api.CreateKeyExW(hRoot, L"Sub", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, nullptr));
        RECORD("flush root", api.FlushKey(hRoot));
        RECORD("flush sub", api.FlushKey(hKey));
        RECORD("flush invalid handle", api.FlushKey(nullptr));

        const HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        RECORD("notify async", api.NotifyChangeKeyValue(hKey, TRUE, REG_NOTIFY_CHANGE_LAST_SET, hEvent, TRUE));
        RECORD("notify async filter name", api.NotifyChangeKeyValue(hKey, FALSE, REG_NOTIFY_CHANGE_NAME, hEvent, TRUE));
        RECORD(
            "notify async thread agnostic",
            api.NotifyChangeKeyValue(hKey, FALSE, REG_NOTIFY_THREAD_AGNOSTIC | REG_NOTIFY_CHANGE_LAST_SET, hEvent, TRUE)
        );
        RECORD("notify async no filter", api.NotifyChangeKeyValue(hKey, FALSE, 0, hEvent, TRUE));
        RECORD("notify async bad filter", api.NotifyChangeKeyValue(hKey, FALSE, 0x80, hEvent, TRUE));
        RECORD("notify async no event", api.NotifyChangeKeyValue(hKey, FALSE, REG_NOTIFY_CHANGE_NAME, nullptr, TRUE));
        RECORD("notify invalid handle", api.NotifyChangeKeyValue(nullptr, FALSE, REG_NOTIFY_CHANGE_NAME, hEvent, TRUE));

        const BYTE data[4] = {1, 2, 3, 4};
        RECORD("set after notify", api.SetValueExW(hKey, L"v", 0, REG_BINARY, data, 4));
        RECORD("notify async again", api.NotifyChangeKeyValue(hKey, FALSE, REG_NOTIFY_CHANGE_LAST_SET, hEvent, TRUE));
        CloseHandle(hEvent);
        RECORD("close", api.CloseKey(hKey));
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

TEMPLATE_LIST_TEST_CASE("Key scenarios answer as the Win32 API does", "[oracle][keys]", Scenarios)
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
